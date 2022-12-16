// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

//#include <frantic/geometry/trimesh3_particle_builder.hpp>
#include "trimesh3_particle_builder.hpp"

namespace frantic {
namespace geometry {

namespace {

struct output_mesh_accessors {
    std::vector<frantic::geometry::trimesh3_vertex_channel_general_accessor> vertexAccessors;
    std::vector<frantic::geometry::trimesh3_face_channel_general_accessor> faceAccessors;
    std::vector<frantic::tstring> vertexChannelNames;
    std::vector<frantic::tstring> faceChannelNames;
};

class vertex_channel_assignment {
  public:
    virtual ~vertex_channel_assignment() {}
    virtual const frantic::tstring& name() = 0;
    virtual std::size_t arity() = 0;
    virtual frantic::channels::data_type_t data_type() = 0;
    virtual bool requires_custom_faces() = 0;
    virtual std::size_t get_custom_face_data_count() = 0;
    virtual void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t firstData,
                      std::size_t firstFace, const frantic::graphics::transform4f& xform,
                      const frantic::graphics::transform4f& xformTimeDerivative,
                      const frantic::geometry::trimesh3& inMesh, const char* inParticle ) = 0;
};

class face_channel_assignment {
  public:
    virtual ~face_channel_assignment() {}
    virtual const frantic::tstring& name() = 0;
    virtual std::size_t arity() = 0;
    virtual frantic::channels::data_type_t data_type() = 0;
    virtual void set( frantic::geometry::trimesh3_face_channel_general_accessor& outAcc, std::size_t firstFace,
                      const frantic::graphics::transform4f& xform,
                      const frantic::graphics::transform4f& xformTimeDerivative,
                      const frantic::geometry::trimesh3& inMesh, const char* inParticle ) = 0;
};

class channel_assignment_collection {
    std::vector<boost::shared_ptr<vertex_channel_assignment>> m_heldVertexChannelAssignments;
    std::vector<vertex_channel_assignment*> m_outputVertexChannelAssignments;
    std::vector<boost::shared_ptr<face_channel_assignment>> m_heldFaceChannelAssignments;
    std::vector<face_channel_assignment*> m_outputFaceChannelAssignments;

    std::size_t get_vertex_assignment_count();
    std::size_t get_face_assignment_count();
    vertex_channel_assignment* get_vertex_channel_assignment( std::size_t i );
    face_channel_assignment* get_face_channel_assignment( std::size_t i );

    friend void combine_trimesh3_particle( frantic::geometry::trimesh3& outMesh, output_mesh_accessors& outAccessors,
                                           const frantic::graphics::transform4f& xform,
                                           const frantic::graphics::transform4f& xformTimeDerivative,
                                           channel_assignment_collection& channelAssignment,
                                           const frantic::geometry::trimesh3& inMesh, const char* inParticle );

  public:
    bool has_vertex_channel( const frantic::tstring& name );
    bool has_face_channel( const frantic::tstring& name );
    void add_vertex_assignment( const frantic::tstring& outName, const frantic::tstring& inName,
                                const frantic::channels::channel_map& channelMap,
                                const trimesh3_particle_combine::trimesh3_transform_t transformType );
    void add_vertex_assignment( const frantic::tstring& name, const frantic::geometry::trimesh3& mesh,
                                const trimesh3_particle_combine::trimesh3_transform_t transformType );
    void add_face_assignment( const frantic::tstring& outName, const frantic::tstring& inName,
                              const frantic::channels::channel_map& channelMap );
    void add_face_assignment( const frantic::tstring& name, const frantic::geometry::trimesh3& mesh );
    void update_for_output( const output_mesh_accessors& outAccessors );
    bool ensure_mesh_has_assigned_channels(
        frantic::geometry::trimesh3& mesh,
        std::map<frantic::tstring, std::pair<frantic::channels::data_type_t, std::size_t>>& );
};

bool update_accessors( frantic::geometry::trimesh3& mesh, output_mesh_accessors& outAccessors );

void combine_trimesh3_particle( frantic::geometry::trimesh3& outMesh, output_mesh_accessors& outAccessors,
                                const frantic::graphics::transform4f& xform,
                                const frantic::graphics::transform4f& xformTimeDerivative,
                                channel_assignment_collection& channelAssignment,
                                const frantic::geometry::trimesh3& inMesh, const char* inParticle );

// alternative conversion function to support int32 -> uint16 required for
// PFlow's MtlIndex -> XMesh's MaterialID
typedef boost::function<void( char*, const char*, std::size_t )> face_channel_type_convertor_function_t;

void convert_int32_to_uint16( char* out, const char* in, std::size_t count,
                              const frantic::tstring& channelNameForErrorMessage ) {
    do {
        const boost::int32_t inVal = *reinterpret_cast<const boost::int32_t*>( in );
        if( inVal < 0 || inVal > std::numeric_limits<boost::uint16_t>::max() ) {
            throw std::runtime_error(
                "convert_int32_to_uint16() - Error converting channel \"" +
                frantic::strings::to_string( channelNameForErrorMessage ) +
                "\": value is out of range for uint16: " + boost::lexical_cast<std::string>( inVal ) );
        }
        *reinterpret_cast<boost::uint16_t*>( out ) = boost::uint16_t( inVal );
        out += 2;
        in += 4;
    } while( --count );
}

face_channel_type_convertor_function_t
get_face_channel_type_convertor_function( frantic::channels::data_type_t sourceType,
                                          frantic::channels::data_type_t destType,
                                          const frantic::tstring& channelNameForErrorMessage ) {
    if( sourceType == frantic::channels::data_type_int32 && destType == frantic::channels::data_type_uint16 ) {
        return boost::bind<void>( convert_int32_to_uint16, _1, _2, _3, channelNameForErrorMessage );
    } else {
        return get_channel_type_convertor_function( sourceType, destType, channelNameForErrorMessage );
    }
}

class vertex_channel_assignment_from_mesh : public vertex_channel_assignment {
  protected:
    frantic::geometry::const_trimesh3_vertex_channel_general_accessor m_inAcc;
    frantic::tstring m_name;

  public:
    vertex_channel_assignment_from_mesh(
        const frantic::tstring& name, const frantic::geometry::const_trimesh3_vertex_channel_general_accessor& inAcc )
        : m_name( name )
        , m_inAcc( inAcc ) {}
    const frantic::tstring& name() { return m_name; }
    std::size_t arity() { return m_inAcc.arity(); }
    frantic::channels::data_type_t data_type() { return m_inAcc.data_type(); }
    bool requires_custom_faces() { return m_inAcc.has_custom_faces(); }
    std::size_t get_custom_face_data_count() { return m_inAcc.size(); }
};

class face_channel_assignment_from_mesh : public face_channel_assignment {
  protected:
    frantic::geometry::const_trimesh3_face_channel_general_accessor m_inAcc;
    frantic::tstring m_name;

  public:
    face_channel_assignment_from_mesh( const frantic::tstring& name,
                                       const frantic::geometry::const_trimesh3_face_channel_general_accessor& inAcc )
        : m_name( name )
        , m_inAcc( inAcc ) {}
    const frantic::tstring& name() { return m_name; }
    std::size_t arity() { return m_inAcc.arity(); }
    frantic::channels::data_type_t data_type() { return m_inAcc.data_type(); }
};

class face_channel_assignment_from_mesh_none : public face_channel_assignment_from_mesh {
  public:
    face_channel_assignment_from_mesh_none(
        const frantic::tstring& name, const frantic::geometry::const_trimesh3_face_channel_general_accessor& inAcc )
        : face_channel_assignment_from_mesh( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_face_channel_general_accessor& outAcc, std::size_t facesBegin,
              const frantic::graphics::transform4f& /*xform*/,
              const frantic::graphics::transform4f& /*xformTimeDerivative*/, const frantic::geometry::trimesh3& inMesh,
              const char* /*inParticle*/ ) {
        face_channel_type_convertor_function_t convertType =
            get_face_channel_type_convertor_function( m_inAcc.data_type(), outAcc.data_type(), m_name );
        char* buffer = reinterpret_cast<char*>( alloca( outAcc.primitive_size() ) );
        for( std::size_t i = 0, ie = inMesh.face_count(); i != ie; ++i ) {
            convertType( buffer, m_inAcc.data( i ), outAcc.arity() );
            memcpy( outAcc.data( facesBegin + i ), buffer, outAcc.primitive_size() );
        }
    }
};

class vertex_channel_assignment_from_mesh_none : public vertex_channel_assignment_from_mesh {
  public:
    vertex_channel_assignment_from_mesh_none(
        const frantic::tstring& name, const frantic::geometry::const_trimesh3_vertex_channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_mesh( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& /*xform*/,
              const frantic::graphics::transform4f& /*xformTimeDerivative*/, const frantic::geometry::trimesh3& inMesh,
              const char* /*inParticle*/ ) {
        frantic::channels::channel_type_convertor_function_t convertType =
            frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(), outAcc.data_type(), name() );
        char* buffer = reinterpret_cast<char*>( alloca( outAcc.primitive_size() ) );
        for( std::size_t i = 0, ie = m_inAcc.size(); i != ie; ++i ) {
            convertType( buffer, m_inAcc.data( i ), outAcc.arity() );
            memcpy( outAcc.data( vertexBegin + i ), buffer, outAcc.primitive_size() );
        }
        if( outAcc.has_custom_faces() ) {
            const frantic::graphics::vector3 faceOffset( static_cast<boost::int32_t>( vertexBegin ) );
            for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
                outAcc.face( facesBegin + i ) = faceOffset + m_inAcc.face( i );
            }
        }
    }
};

class vertex_channel_assignment_from_mesh_point : public vertex_channel_assignment_from_mesh {
  public:
    vertex_channel_assignment_from_mesh_point(
        const frantic::tstring& name, const frantic::geometry::const_trimesh3_vertex_channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_mesh( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& xform,
              const frantic::graphics::transform4f& /*xformTimeDerivative*/, const frantic::geometry::trimesh3& inMesh,
              const char* /*inParticle*/ ) {
        if( outAcc.arity() != 3 ) {
            throw std::runtime_error( "vertex_channel_assignment_from_mesh_point Error: arity of channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must be 3, but instead it is " +
                                      boost::lexical_cast<std::string>( outAcc.arity() ) );
        }
        frantic::channels::channel_type_convertor_function_t convertFromChannel =
            frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(),
                                                                    frantic::channels::data_type_float32, name() );
        frantic::channels::channel_type_convertor_function_t convertToChannel =
            frantic::channels::get_channel_type_convertor_function( frantic::channels::data_type_float32,
                                                                    outAcc.data_type(), name() );
        frantic::graphics::vector3f tempIn, tempOut;
        for( std::size_t i = 0, ie = m_inAcc.size(); i != ie; ++i ) {
            convertFromChannel( reinterpret_cast<char*>( &tempIn ), m_inAcc.data( i ), outAcc.arity() );
            tempOut = xform * tempIn;
            convertToChannel( outAcc.data( vertexBegin + i ), reinterpret_cast<char*>( &tempOut ), outAcc.arity() );
        }
        if( outAcc.has_custom_faces() ) {
            const frantic::graphics::vector3 faceOffset( static_cast<boost::int32_t>( vertexBegin ) );
            for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
                outAcc.face( facesBegin + i ) = faceOffset + m_inAcc.face( i );
            }
        }
    }
};

class vertex_channel_assignment_from_mesh_vector : public vertex_channel_assignment_from_mesh {
  public:
    vertex_channel_assignment_from_mesh_vector(
        const frantic::tstring& name, const frantic::geometry::const_trimesh3_vertex_channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_mesh( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& xform,
              const frantic::graphics::transform4f& /*xformTimeDerivative*/, const frantic::geometry::trimesh3& inMesh,
              const char* /*inParticle*/ ) {
        if( outAcc.arity() != 3 ) {
            throw std::runtime_error( "vertex_channel_assignment_from_mesh_vector Error: arity of channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must be 3, but instead it is " +
                                      boost::lexical_cast<std::string>( outAcc.arity() ) );
        }
        frantic::channels::channel_type_convertor_function_t convertFromChannel =
            frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(),
                                                                    frantic::channels::data_type_float32, name() );
        frantic::channels::channel_type_convertor_function_t convertToChannel =
            frantic::channels::get_channel_type_convertor_function( frantic::channels::data_type_float32,
                                                                    outAcc.data_type(), name() );
        frantic::graphics::vector3f tempIn, tempOut;
        for( std::size_t i = 0, ie = m_inAcc.size(); i != ie; ++i ) {
            convertFromChannel( reinterpret_cast<char*>( &tempIn ), m_inAcc.data( i ), outAcc.arity() );
            tempOut = xform.transform_no_translation( tempIn );
            convertToChannel( outAcc.data( vertexBegin + i ), reinterpret_cast<char*>( &tempOut ), outAcc.arity() );
        }
        if( outAcc.has_custom_faces() ) {
            const frantic::graphics::vector3 faceOffset( static_cast<boost::int32_t>( vertexBegin ) );
            for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
                outAcc.face( facesBegin + i ) = faceOffset + m_inAcc.face( i );
            }
        }
    }
};

bool is_zero_without_translation( const frantic::graphics::transform4f& xform ) {
    return xform.get_column( 0 ).is_zero() && xform.get_column( 1 ).is_zero() && xform.get_column( 2 ).is_zero();
}

class vertex_channel_assignment_from_mesh_normal : public vertex_channel_assignment_from_mesh {
  public:
    vertex_channel_assignment_from_mesh_normal(
        const frantic::tstring& name, const frantic::geometry::const_trimesh3_vertex_channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_mesh( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& xform,
              const frantic::graphics::transform4f& /*xformTimeDerivative*/, const frantic::geometry::trimesh3& inMesh,
              const char* /*inParticle*/ ) {
        if( outAcc.arity() != 3 ) {
            throw std::runtime_error( "vertex_channel_assignment_from_mesh_normal Error: arity of channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must be 3, but instead it is " +
                                      boost::lexical_cast<std::string>( outAcc.arity() ) );
        }
        frantic::channels::channel_type_convertor_function_t convertFromChannel =
            frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(),
                                                                    frantic::channels::data_type_float32, name() );
        frantic::channels::channel_type_convertor_function_t convertToChannel =
            frantic::channels::get_channel_type_convertor_function( frantic::channels::data_type_float32,
                                                                    outAcc.data_type(), name() );
        // If the non-translation part of xform is zero, then it is not invertible,
        // so use the identity transform instead.
        // TODO: what about other non-invertible cases?
        const frantic::graphics::transform4f xformInverse =
            is_zero_without_translation( xform ) ? frantic::graphics::transform4f() : xform.to_inverse();
        frantic::graphics::vector3f tempIn, tempOut;
        for( std::size_t i = 0, ie = m_inAcc.size(); i != ie; ++i ) {
            convertFromChannel( reinterpret_cast<char*>( &tempIn ), m_inAcc.data( i ), outAcc.arity() );
            tempOut = xformInverse.transpose_transform_no_translation( tempIn );
            convertToChannel( outAcc.data( vertexBegin + i ), reinterpret_cast<char*>( &tempOut ), outAcc.arity() );
        }
        if( outAcc.has_custom_faces() ) {
            const frantic::graphics::vector3 faceOffset( static_cast<boost::int32_t>( vertexBegin ) );
            for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
                outAcc.face( facesBegin + i ) = faceOffset + m_inAcc.face( i );
            }
        }
    }
};

class vertex_channel_assignment_from_mesh_velocity : public vertex_channel_assignment_from_mesh {
  public:
    vertex_channel_assignment_from_mesh_velocity(
        const frantic::tstring& name, const frantic::geometry::const_trimesh3_vertex_channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_mesh( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& xform,
              const frantic::graphics::transform4f& xformTimeDerivative, const frantic::geometry::trimesh3& inMesh,
              const char* /*inParticle*/ ) {
        if( outAcc.arity() != 3 ) {
            throw std::runtime_error( "vertex_channel_assignment_from_mesh_velocity Error: arity of channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must be 3, but instead it is " +
                                      boost::lexical_cast<std::string>( outAcc.arity() ) );
        }
        if( outAcc.has_custom_faces() ) {
            throw std::runtime_error( "vertex_channel_assignment_from_mesh_velocity Error: channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must not have custom faces" );
        }
        frantic::channels::channel_type_convertor_function_t convertFromChannel =
            frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(),
                                                                    frantic::channels::data_type_float32, name() );
        frantic::channels::channel_type_convertor_function_t convertToChannel =
            frantic::channels::get_channel_type_convertor_function( frantic::channels::data_type_float32,
                                                                    outAcc.data_type(), name() );
        frantic::graphics::vector3f tempIn, tempOut;
        for( std::size_t i = 0, ie = m_inAcc.size(); i != ie; ++i ) {
            convertFromChannel( reinterpret_cast<char*>( &tempIn ), m_inAcc.data( i ), outAcc.arity() );
            tempOut = xform.transform_no_translation( tempIn ) + xformTimeDerivative * inMesh.get_vertex( i );
            convertToChannel( outAcc.data( vertexBegin + i ), reinterpret_cast<char*>( &tempOut ), outAcc.arity() );
        }
        if( outAcc.has_custom_faces() ) {
            const frantic::graphics::vector3 faceOffset( static_cast<boost::int32_t>( vertexBegin ) );
            for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
                outAcc.face( facesBegin + i ) = faceOffset + m_inAcc.face( i );
            }
        }
    }
};

class vertex_channel_assignment_from_particle : public vertex_channel_assignment {
  protected:
    frantic::channels::channel_general_accessor m_inAcc;
    frantic::tstring m_name;

  public:
    vertex_channel_assignment_from_particle( const frantic::tstring& name,
                                             const frantic::channels::channel_general_accessor& inAcc )
        : m_name( name )
        , m_inAcc( inAcc ) {}
    const frantic::tstring& name() { return m_name; }
    std::size_t arity() { return m_inAcc.arity(); }
    frantic::channels::data_type_t data_type() { return m_inAcc.data_type(); }
    bool requires_custom_faces() { return false; }
};

class vertex_channel_assignment_from_particle_none : public vertex_channel_assignment_from_particle {
  public:
    vertex_channel_assignment_from_particle_none( const frantic::tstring& name,
                                                  const frantic::channels::channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_particle( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& /*xform*/,
              const frantic::graphics::transform4f& /*xformTimeDerivative*/, const frantic::geometry::trimesh3& inMesh,
              const char* inParticle ) {
        frantic::channels::channel_type_convertor_function_t convertType =
            frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(), outAcc.data_type(), name() );
        char* buffer = reinterpret_cast<char*>( alloca( outAcc.primitive_size() ) );
        convertType( buffer, m_inAcc.get_channel_data_pointer( inParticle ), outAcc.arity() );
        if( outAcc.has_custom_faces() ) {
            memcpy( outAcc.data( vertexBegin ), buffer, outAcc.primitive_size() );
            const frantic::graphics::vector3 face( static_cast<boost::int32_t>( vertexBegin ) );
            for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
                outAcc.face( facesBegin + i ) = face;
            }
        } else {
            for( std::size_t i = 0; i < inMesh.vertex_count(); ++i ) {
                memcpy( outAcc.data( vertexBegin + i ), buffer, outAcc.primitive_size() );
            }
        }
    }
    std::size_t get_custom_face_data_count() { return 1; }
};

static void set_vertex_from_particle_none( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc,
                                           std::size_t vertexBegin, std::size_t facesBegin,
                                           const frantic::graphics::transform4f& /*xform*/,
                                           const frantic::graphics::transform4f& /*xformTimeDerivative*/,
                                           const frantic::geometry::trimesh3& inMesh, const char* inParticle,
                                           const frantic::tstring& name,
                                           frantic::channels::channel_general_accessor& inAcc ) {
    frantic::channels::channel_type_convertor_function_t convertType =
        frantic::channels::get_channel_type_convertor_function( inAcc.data_type(), outAcc.data_type(), name );
    char* buffer = reinterpret_cast<char*>( alloca( outAcc.primitive_size() ) );
    convertType( buffer, inAcc.get_channel_data_pointer( inParticle ), outAcc.arity() );
    if( outAcc.has_custom_faces() ) {
        memcpy( outAcc.data( vertexBegin ), buffer, outAcc.primitive_size() );
        const frantic::graphics::vector3 face( static_cast<boost::int32_t>( vertexBegin ) );
        for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
            outAcc.face( facesBegin + i ) = face;
        }
    } else {
        for( std::size_t i = 0; i < inMesh.vertex_count(); ++i ) {
            memcpy( outAcc.data( vertexBegin + i ), buffer, outAcc.primitive_size() );
        }
    }
}

class vertex_channel_assignment_from_particle_point : public vertex_channel_assignment_from_particle {
  public:
    vertex_channel_assignment_from_particle_point( const frantic::tstring& name,
                                                   const frantic::channels::channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_particle( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& xform,
              const frantic::graphics::transform4f& xformTimeDerivative, const frantic::geometry::trimesh3& inMesh,
              const char* inParticle ) {
        if( outAcc.arity() != 3 ) {
            throw std::runtime_error( "vertex_channel_assignment_from_particle_point Error: arity of channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must be 3, but instead it is " +
                                      boost::lexical_cast<std::string>( outAcc.arity() ) );
        }
        if( xform.is_identity() ) {
            set_vertex_from_particle_none( outAcc, vertexBegin, facesBegin, xform, xformTimeDerivative, inMesh,
                                           inParticle, m_name, m_inAcc );
        } else {
            frantic::channels::channel_type_convertor_function_t convertFromChannel =
                frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(),
                                                                        frantic::channels::data_type_float32, name() );
            frantic::channels::channel_type_convertor_function_t convertToChannel =
                frantic::channels::get_channel_type_convertor_function( frantic::channels::data_type_float32,
                                                                        outAcc.data_type(), name() );
            frantic::graphics::vector3f temp;
            char* buffer = reinterpret_cast<char*>( alloca( outAcc.primitive_size() ) );
            convertFromChannel( reinterpret_cast<char*>( &temp ), m_inAcc.get_channel_data_pointer( inParticle ),
                                outAcc.arity() );
            temp = xform * temp;
            convertToChannel( buffer, reinterpret_cast<char*>( &temp ), outAcc.arity() );
            if( outAcc.has_custom_faces() ) {
                memcpy( outAcc.data( vertexBegin ), buffer, outAcc.primitive_size() );
                const frantic::graphics::vector3 face( static_cast<boost::int32_t>( vertexBegin ) );
                for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
                    outAcc.face( facesBegin + i ) = face;
                }
            } else {
                for( std::size_t i = 0, ie = inMesh.vertex_count(); i != ie; ++i ) {
                    memcpy( outAcc.data( vertexBegin + i ), buffer, outAcc.primitive_size() );
                }
            }
        }
    }
    std::size_t get_custom_face_data_count() { return 1; }
};

class vertex_channel_assignment_from_particle_vector : public vertex_channel_assignment_from_particle {
  public:
    vertex_channel_assignment_from_particle_vector( const frantic::tstring& name,
                                                    const frantic::channels::channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_particle( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& xform,
              const frantic::graphics::transform4f& xformTimeDerivative, const frantic::geometry::trimesh3& inMesh,
              const char* inParticle ) {
        if( outAcc.arity() != 3 ) {
            throw std::runtime_error( "vertex_channel_assignment_from_particle_vector Error: arity of channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must be 3, but instead it is " +
                                      boost::lexical_cast<std::string>( outAcc.arity() ) );
        }
        if( xform.is_identity() /*or identity without translation*/ ) {
            set_vertex_from_particle_none( outAcc, vertexBegin, facesBegin, xform, xformTimeDerivative, inMesh,
                                           inParticle, name(), m_inAcc );
        } else {
            frantic::channels::channel_type_convertor_function_t convertFromChannel =
                frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(),
                                                                        frantic::channels::data_type_float32, name() );
            frantic::channels::channel_type_convertor_function_t convertToChannel =
                frantic::channels::get_channel_type_convertor_function( frantic::channels::data_type_float32,
                                                                        outAcc.data_type(), name() );
            frantic::graphics::vector3f temp;
            char* buffer = reinterpret_cast<char*>( alloca( outAcc.primitive_size() ) );
            convertFromChannel( reinterpret_cast<char*>( &temp ), m_inAcc.get_channel_data_pointer( inParticle ),
                                outAcc.arity() );
            temp = xform.transform_no_translation( temp );
            convertToChannel( buffer, reinterpret_cast<char*>( &temp ), outAcc.arity() );
            if( outAcc.has_custom_faces() ) {
                memcpy( outAcc.data( vertexBegin ), buffer, outAcc.primitive_size() );
                const frantic::graphics::vector3 face( static_cast<boost::int32_t>( vertexBegin ) );
                for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
                    outAcc.face( facesBegin + i ) = face;
                }
            } else {
                for( std::size_t i = 0, ie = inMesh.vertex_count(); i != ie; ++i ) {
                    memcpy( outAcc.data( vertexBegin + i ), buffer, outAcc.primitive_size() );
                }
            }
        }
    }
    std::size_t get_custom_face_data_count() { return 1; }
};

class vertex_channel_assignment_from_particle_normal : public vertex_channel_assignment_from_particle {
  public:
    vertex_channel_assignment_from_particle_normal( const frantic::tstring& name,
                                                    const frantic::channels::channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_particle( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& xform,
              const frantic::graphics::transform4f& xformTimeDerivative, const frantic::geometry::trimesh3& inMesh,
              const char* inParticle ) {
        if( outAcc.arity() != 3 ) {
            throw std::runtime_error( "vertex_channel_assignment_from_particle_normal Error: arity of channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must be 3, but instead it is " +
                                      boost::lexical_cast<std::string>( outAcc.arity() ) );
        }
        if( xform.is_identity() /*or identity without translation*/ ) {
            set_vertex_from_particle_none( outAcc, vertexBegin, facesBegin, xform, xformTimeDerivative, inMesh,
                                           inParticle, name(), m_inAcc );
        } else {
            frantic::channels::channel_type_convertor_function_t convertFromChannel =
                frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(),
                                                                        frantic::channels::data_type_float32, name() );
            frantic::channels::channel_type_convertor_function_t convertToChannel =
                frantic::channels::get_channel_type_convertor_function( frantic::channels::data_type_float32,
                                                                        outAcc.data_type(), name() );
            const frantic::graphics::transform4f xformInverse = xform.to_inverse();
            frantic::graphics::vector3f temp;
            char* buffer = reinterpret_cast<char*>( alloca( outAcc.primitive_size() ) );
            convertFromChannel( reinterpret_cast<char*>( &temp ), m_inAcc.get_channel_data_pointer( inParticle ),
                                outAcc.arity() );
            temp = xformInverse.transpose_transform_no_translation( temp );
            convertToChannel( buffer, reinterpret_cast<char*>( &temp ), outAcc.arity() );
            if( outAcc.has_custom_faces() ) {
                memcpy( outAcc.data( vertexBegin ), buffer, outAcc.primitive_size() );
                const frantic::graphics::vector3 face( static_cast<boost::int32_t>( vertexBegin ) );
                for( std::size_t i = 0; i < inMesh.face_count(); ++i ) {
                    outAcc.face( facesBegin + i ) = face;
                }
            } else {
                for( std::size_t i = 0, ie = inMesh.vertex_count(); i != ie; ++i ) {
                    memcpy( outAcc.data( vertexBegin + i ), buffer, outAcc.primitive_size() );
                }
            }
        }
    }
    std::size_t get_custom_face_data_count() { return 1; }
};

class vertex_channel_assignment_from_particle_velocity : public vertex_channel_assignment_from_particle {
  public:
    vertex_channel_assignment_from_particle_velocity( const frantic::tstring& name,
                                                      const frantic::channels::channel_general_accessor& inAcc )
        : vertex_channel_assignment_from_particle( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_vertex_channel_general_accessor& outAcc, std::size_t vertexBegin,
              std::size_t facesBegin, const frantic::graphics::transform4f& xform,
              const frantic::graphics::transform4f& xformTimeDerivative, const frantic::geometry::trimesh3& inMesh,
              const char* inParticle ) {
        if( outAcc.arity() != 3 ) {
            throw std::runtime_error( "vertex_channel_assignment_from_particle_velocity Error: arity of channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must be 3, but instead it is " +
                                      boost::lexical_cast<std::string>( outAcc.arity() ) );
        }
        if( outAcc.has_custom_faces() ) {
            throw std::runtime_error( "vertex_channel_assignment_from_particle_velocity Error: channel \'" +
                                      frantic::strings::to_string( name() ) + "\' must not have custom faces" );
        }
        if( xform.is_identity() && xformTimeDerivative.is_zero() ) {
            set_vertex_from_particle_none( outAcc, vertexBegin, facesBegin, xform, xformTimeDerivative, inMesh,
                                           inParticle, name(), m_inAcc );
        } else {
            frantic::channels::channel_type_convertor_function_t convertFromChannel =
                frantic::channels::get_channel_type_convertor_function( m_inAcc.data_type(),
                                                                        frantic::channels::data_type_float32, name() );
            frantic::channels::channel_type_convertor_function_t convertToChannel =
                frantic::channels::get_channel_type_convertor_function( frantic::channels::data_type_float32,
                                                                        outAcc.data_type(), name() );
            frantic::graphics::vector3f temp;
            // frantic::graphics::vector3f transformedVelocity;
            frantic::graphics::vector3f velocity;
            char* buffer = reinterpret_cast<char*>( alloca( outAcc.primitive_size() ) );
            convertFromChannel( reinterpret_cast<char*>( &velocity ), m_inAcc.get_channel_data_pointer( inParticle ),
                                outAcc.arity() );
            // transformedVelocity = xform.transform_no_translation( velocity );
            for( std::size_t i = 0, ie = inMesh.vertex_count(); i != ie; ++i ) {
                temp = velocity + xformTimeDerivative * inMesh.get_vertex( i );
                convertToChannel( buffer, reinterpret_cast<char*>( &temp ), outAcc.arity() );
                memcpy( outAcc.data( vertexBegin + i ), buffer, outAcc.primitive_size() );
            }
        }
    }
    std::size_t get_custom_face_data_count() {
        // TODO : what ?
        return 1;
    }
};

class face_channel_assignment_from_particle : public face_channel_assignment {
  protected:
    frantic::channels::channel_general_accessor m_inAcc;
    frantic::tstring m_name;

  public:
    face_channel_assignment_from_particle( const frantic::tstring& name,
                                           const frantic::channels::channel_general_accessor& inAcc )
        : m_name( name )
        , m_inAcc( inAcc ) {}
    const frantic::tstring& name() { return m_name; }
    std::size_t arity() { return m_inAcc.arity(); }
    frantic::channels::data_type_t data_type() { return m_inAcc.data_type(); }
};

class face_channel_assignment_from_particle_none : public face_channel_assignment_from_particle {
  public:
    face_channel_assignment_from_particle_none( const frantic::tstring& name,
                                                const frantic::channels::channel_general_accessor& inAcc )
        : face_channel_assignment_from_particle( name, inAcc ) {}
    void set( frantic::geometry::trimesh3_face_channel_general_accessor& outAcc, std::size_t facesBegin,
              const frantic::graphics::transform4f& /*xform*/,
              const frantic::graphics::transform4f& /*xformTimeDerivative*/, const frantic::geometry::trimesh3& inMesh,
              const char* inParticle ) {
        face_channel_type_convertor_function_t convertType =
            get_face_channel_type_convertor_function( m_inAcc.data_type(), outAcc.data_type(), m_name );
        char* buffer = reinterpret_cast<char*>( alloca( outAcc.primitive_size() ) );
        convertType( buffer, m_inAcc.get_channel_data_pointer( inParticle ), outAcc.arity() );
        for( std::size_t i = 0, ie = inMesh.face_count(); i != ie; ++i ) {
            memcpy( outAcc.data( facesBegin + i ), buffer, outAcc.primitive_size() );
        }
    }
};

std::size_t channel_assignment_collection::get_vertex_assignment_count() {
    return m_outputVertexChannelAssignments.size();
}
std::size_t channel_assignment_collection::get_face_assignment_count() { return m_outputFaceChannelAssignments.size(); }
vertex_channel_assignment* channel_assignment_collection::get_vertex_channel_assignment( std::size_t i ) {
    if( i < m_outputVertexChannelAssignments.size() ) {
        return m_outputVertexChannelAssignments[i];
    } else {
        return 0;
    }
}
face_channel_assignment* channel_assignment_collection::get_face_channel_assignment( std::size_t i ) {
    if( i < m_outputFaceChannelAssignments.size() ) {
        return m_outputFaceChannelAssignments[i];
    } else {
        return 0;
    }
}

bool channel_assignment_collection::has_vertex_channel( const frantic::tstring& name ) {
    for( std::size_t i = 0; i < m_heldVertexChannelAssignments.size(); ++i ) {
        vertex_channel_assignment* assignment = m_heldVertexChannelAssignments[i].get();
        if( assignment ) {
            if( assignment->name() == name ) {
                return true;
            }
        }
    }
    return false;
}

bool channel_assignment_collection::has_face_channel( const frantic::tstring& name ) {
    for( std::size_t i = 0; i < m_heldFaceChannelAssignments.size(); ++i ) {
        face_channel_assignment* assignment = m_heldFaceChannelAssignments[i].get();
        if( assignment ) {
            if( assignment->name() == name ) {
                return true;
            }
        }
    }
    return false;
}

void channel_assignment_collection::add_vertex_assignment(
    const frantic::tstring& outName, const frantic::tstring& inName, const frantic::channels::channel_map& channelMap,
    const trimesh3_particle_combine::trimesh3_transform_t transformType ) {
    if( has_vertex_channel( outName ) ) {
        throw std::runtime_error( "add_vertex_assignment Error: the channel assignment already has a channel named \'" +
                                  frantic::strings::to_string( outName ) + "\'" );
    }
    frantic::channels::channel_general_accessor acc = channelMap.get_general_accessor( inName );
    boost::shared_ptr<vertex_channel_assignment> assignment;
    switch( transformType ) {
    case trimesh3_particle_combine::trimesh3_transform_none:
        assignment = boost::make_shared<vertex_channel_assignment_from_particle_none>( outName, acc );
        break;
    case trimesh3_particle_combine::trimesh3_transform_point:
        assignment = boost::make_shared<vertex_channel_assignment_from_particle_point>( outName, acc );
        break;
    case trimesh3_particle_combine::trimesh3_transform_vector:
        assignment = boost::make_shared<vertex_channel_assignment_from_particle_vector>( outName, acc );
        break;
    case trimesh3_particle_combine::trimesh3_transform_normal:
        assignment = boost::make_shared<vertex_channel_assignment_from_particle_normal>( outName, acc );
        break;
    case trimesh3_particle_combine::trimesh3_transform_velocity:
        assignment = boost::make_shared<vertex_channel_assignment_from_particle_velocity>( outName, acc );
        break;
    default:
        throw std::runtime_error( "add_vertex_assignment Error: unknown transform type: " +
                                  boost::lexical_cast<std::string>( transformType ) );
    }
    if( assignment ) {
        m_heldVertexChannelAssignments.push_back( assignment );
    }
}
void channel_assignment_collection::add_vertex_assignment(
    const frantic::tstring& name, const frantic::geometry::trimesh3& mesh,
    const trimesh3_particle_combine::trimesh3_transform_t transformType ) {
    if( has_vertex_channel( name ) ) {
        throw std::runtime_error( "add_vertex_assignment Error: the channel assignment already has a channel named \'" +
                                  frantic::strings::to_string( name ) + "\'" );
    }
    frantic::geometry::const_trimesh3_vertex_channel_general_accessor acc =
        mesh.get_vertex_channel_general_accessor( name );
    boost::shared_ptr<vertex_channel_assignment> assignment;
    switch( transformType ) {
    case trimesh3_particle_combine::trimesh3_transform_none:
        assignment = boost::make_shared<vertex_channel_assignment_from_mesh_none>( name, acc );
        break;
    case trimesh3_particle_combine::trimesh3_transform_point:
        assignment = boost::make_shared<vertex_channel_assignment_from_mesh_point>( name, acc );
        break;
    case trimesh3_particle_combine::trimesh3_transform_vector:
        assignment = boost::make_shared<vertex_channel_assignment_from_mesh_vector>( name, acc );
        break;
    case trimesh3_particle_combine::trimesh3_transform_normal:
        assignment = boost::make_shared<vertex_channel_assignment_from_mesh_normal>( name, acc );
        break;
    case trimesh3_particle_combine::trimesh3_transform_velocity:
        assignment = boost::make_shared<vertex_channel_assignment_from_mesh_velocity>( name, acc );
        break;
    default:
        throw std::runtime_error( "add_vertex_assignment Error: unknown transform type: " +
                                  boost::lexical_cast<std::string>( transformType ) );
    }
    if( assignment ) {
        m_heldVertexChannelAssignments.push_back( assignment );
    }
}
void channel_assignment_collection::add_face_assignment( const frantic::tstring& outName,
                                                         const frantic::tstring& inName,
                                                         const frantic::channels::channel_map& channelMap ) {
    if( has_face_channel( outName ) ) {
        throw std::runtime_error( "add_face_assignment Error: the channel assignment already has a channel named \'" +
                                  frantic::strings::to_string( outName ) + "\'" );
    }
    frantic::channels::channel_general_accessor acc = channelMap.get_general_accessor( inName );
    boost::shared_ptr<face_channel_assignment> assignment;
    assignment = boost::make_shared<face_channel_assignment_from_particle_none>( outName, acc );
    if( assignment ) {
        m_heldFaceChannelAssignments.push_back( assignment );
    }
}
void channel_assignment_collection::add_face_assignment( const frantic::tstring& name,
                                                         const frantic::geometry::trimesh3& mesh ) {
    if( has_face_channel( name ) ) {
        throw std::runtime_error( "add_face_assignment Error: the channel assignment already has a channel named \'" +
                                  frantic::strings::to_string( name ) + "\'" );
    }
    frantic::geometry::const_trimesh3_face_channel_general_accessor acc =
        mesh.get_face_channel_general_accessor( name );
    boost::shared_ptr<face_channel_assignment> assignment;
    assignment = boost::make_shared<face_channel_assignment_from_mesh_none>( name, acc );
    if( assignment ) {
        m_heldFaceChannelAssignments.push_back( assignment );
    }
}

void channel_assignment_collection::update_for_output( const output_mesh_accessors& outAccessors ) {
    // todo : check arity & data type ?
    std::vector<vertex_channel_assignment*> newVertexChannelAssignments;
    std::vector<face_channel_assignment*> newFaceChannelAssignments;
    { // scope for vertices
        std::map<frantic::tstring, std::size_t> vertexChannelNameToIndex;
        for( std::size_t i = 0; i < m_heldVertexChannelAssignments.size(); ++i ) {
            vertex_channel_assignment* assignment = m_heldVertexChannelAssignments[i].get();
            if( assignment ) {
                vertexChannelNameToIndex[assignment->name()] = i;
            }
        }
        newVertexChannelAssignments.reserve( outAccessors.vertexChannelNames.size() );
        BOOST_FOREACH( const frantic::tstring& channelName, outAccessors.vertexChannelNames ) {
            std::map<frantic::tstring, std::size_t>::iterator i = vertexChannelNameToIndex.find( channelName );
            if( i == vertexChannelNameToIndex.end() ) {
                newVertexChannelAssignments.push_back( 0 );
            } else {
                newVertexChannelAssignments.push_back( m_heldVertexChannelAssignments[i->second].get() );
            }
        }
    }
    { // scope for faces
        std::map<frantic::tstring, std::size_t> faceChannelNameToIndex;
        for( std::size_t i = 0; i < m_heldFaceChannelAssignments.size(); ++i ) {
            face_channel_assignment* assignment = m_heldFaceChannelAssignments[i].get();
            if( assignment ) {
                faceChannelNameToIndex[assignment->name()] = i;
            }
        }
        newFaceChannelAssignments.reserve( outAccessors.faceChannelNames.size() );
        BOOST_FOREACH( const frantic::tstring& channelName, outAccessors.faceChannelNames ) {
            std::map<frantic::tstring, std::size_t>::iterator i = faceChannelNameToIndex.find( channelName );
            if( i == faceChannelNameToIndex.end() ) {
                newFaceChannelAssignments.push_back( 0 );
            } else {
                newFaceChannelAssignments.push_back( m_heldFaceChannelAssignments[i->second].get() );
            }
        }
    }
    newVertexChannelAssignments.swap( m_outputVertexChannelAssignments );
    newFaceChannelAssignments.swap( m_outputFaceChannelAssignments );
}

bool channel_assignment_collection::ensure_mesh_has_assigned_channels(
    frantic::geometry::trimesh3& mesh,
    std::map<frantic::tstring, std::pair<frantic::channels::data_type_t, std::size_t>>& faceChannelDesiredType ) {
    bool changed = false;
    for( std::size_t i = 0, ie = m_heldVertexChannelAssignments.size(); i != ie; ++i ) {
        vertex_channel_assignment* assignment = m_heldVertexChannelAssignments[i].get();
        if( assignment ) {
            const frantic::tstring& channelName = assignment->name();
            if( !mesh.has_vertex_channel( channelName ) ) {
                mesh.add_vertex_channel_raw( channelName, assignment->arity(), assignment->data_type() );
                changed = true;
            }
            frantic::geometry::trimesh3_vertex_channel_general_accessor acc =
                mesh.get_vertex_channel_general_accessor( channelName );
            if( assignment->requires_custom_faces() && !acc.has_custom_faces() ) {
                mesh.set_vertex_channel_custom_faces( channelName, true );
                frantic::geometry::trimesh3_vertex_channel_general_accessor acc =
                    mesh.get_vertex_channel_general_accessor( channelName );
                acc.set_vertex_count( mesh.vertex_count() );
                for( std::size_t i = 0; i < mesh.vertex_count(); ++i ) {
                    memset( acc.data( i ), 0, acc.primitive_size() );
                }
                acc = mesh.get_vertex_channel_general_accessor( channelName );
                changed = true;
            }
            if( acc.arity() != assignment->arity() ) {
                throw std::runtime_error( "Mismatch in arity of vertex channel \'" +
                                          frantic::strings::to_string( channelName ) + "\'.  Mesh has arity " +
                                          boost::lexical_cast<std::string>( acc.arity() ) + " while input has arity " +
                                          boost::lexical_cast<std::string>( assignment->arity() ) + "." );
            }
            frantic::channels::get_channel_type_convertor_function( assignment->data_type(), acc.data_type(),
                                                                    channelName );
        }
    }
    for( std::size_t i = 0, ie = m_heldFaceChannelAssignments.size(); i != ie; ++i ) {
        face_channel_assignment* assignment = m_heldFaceChannelAssignments[i].get();
        if( assignment ) {
            const frantic::tstring& channelName = assignment->name();
            if( !mesh.has_face_channel( channelName ) ) {
                std::map<frantic::tstring, std::pair<frantic::channels::data_type_t, std::size_t>>::iterator j =
                    faceChannelDesiredType.find( channelName );
                if( j == faceChannelDesiredType.end() ) {
                    mesh.add_face_channel_raw( channelName, assignment->arity(), assignment->data_type() );
                } else {
                    std::pair<frantic::channels::data_type_t, std::size_t> typeAndArity = j->second;
                    mesh.add_face_channel_raw( channelName, typeAndArity.second, typeAndArity.first );
                }
                changed = true;
            }
            frantic::geometry::trimesh3_face_channel_general_accessor acc =
                mesh.get_face_channel_general_accessor( channelName );
            if( acc.arity() != assignment->arity() ) {
                throw std::runtime_error( "Mismatch in arity of face channel \'" +
                                          frantic::strings::to_string( channelName ) + "\'.  Mesh has arity " +
                                          boost::lexical_cast<std::string>( acc.arity() ) + " while input has arity " +
                                          boost::lexical_cast<std::string>( assignment->arity() ) + "." );
            }
            get_face_channel_type_convertor_function( assignment->data_type(), acc.data_type(), channelName );
        }
    }
    return changed;
}

bool update_accessors( frantic::geometry::trimesh3& mesh, output_mesh_accessors& outAccessors ) {
    bool changed = false;
    { // vertex channels
        std::vector<frantic::tstring> channelNames;
        mesh.get_vertex_channel_names( channelNames );
        std::map<frantic::tstring, std::size_t> channelNameToOutAccessorIndex;
        for( std::size_t i = 0; i < outAccessors.vertexChannelNames.size(); ++i ) {
            channelNameToOutAccessorIndex[outAccessors.vertexChannelNames[i]] = i;
        }
        BOOST_FOREACH( const frantic::tstring& channelName, channelNames ) {
            std::map<frantic::tstring, std::size_t>::iterator i = channelNameToOutAccessorIndex.find( channelName );
            if( i == channelNameToOutAccessorIndex.end() ) {
                frantic::geometry::trimesh3_vertex_channel_general_accessor acc =
                    mesh.get_vertex_channel_general_accessor( channelName );
                outAccessors.vertexAccessors.push_back( acc );
                outAccessors.vertexChannelNames.push_back( channelName );
                changed = true;
            } else {
                frantic::geometry::trimesh3_vertex_channel_general_accessor acc =
                    mesh.get_vertex_channel_general_accessor( channelName );
                const frantic::geometry::trimesh3_vertex_channel_general_accessor& currentAcc =
                    outAccessors.vertexAccessors[i->second];
                if( acc.arity() != currentAcc.arity() || acc.data_type() != currentAcc.data_type() ||
                    acc.has_custom_faces() != currentAcc.has_custom_faces() ) {
                    outAccessors.vertexAccessors[i->second] = acc;
                    changed = true;
                }
            }
        }
    }
    { // face channels
        std::vector<frantic::tstring> channelNames;
        mesh.get_face_channel_names( channelNames );
        std::map<frantic::tstring, std::size_t> channelNameToOutAccessorIndex;
        for( std::size_t i = 0; i < outAccessors.faceChannelNames.size(); ++i ) {
            channelNameToOutAccessorIndex[outAccessors.faceChannelNames[i]] = i;
        }
        BOOST_FOREACH( const frantic::tstring& channelName, channelNames ) {
            std::map<frantic::tstring, std::size_t>::iterator i = channelNameToOutAccessorIndex.find( channelName );
            if( i == channelNameToOutAccessorIndex.end() ) {
                frantic::geometry::trimesh3_face_channel_general_accessor acc =
                    mesh.get_face_channel_general_accessor( channelName );
                outAccessors.faceAccessors.push_back( acc );
                outAccessors.faceChannelNames.push_back( channelName );
                changed = true;
            } else {
                frantic::geometry::trimesh3_face_channel_general_accessor acc =
                    mesh.get_face_channel_general_accessor( channelName );
                const frantic::geometry::trimesh3_face_channel_general_accessor currentAcc =
                    outAccessors.faceAccessors[i->second];
                if( acc.arity() != currentAcc.arity() || acc.data_type() != currentAcc.data_type() ) {
                    outAccessors.faceAccessors[i->second] = acc;
                    changed = true;
                }
            }
        }
    }
    return changed;
}

inline void combine_trimesh3_particle( frantic::geometry::trimesh3& outMesh, output_mesh_accessors& outAccessors,
                                       const frantic::graphics::transform4f& xform,
                                       const frantic::graphics::transform4f& xformTimeDerivative,
                                       channel_assignment_collection& channelAssignment,
                                       const frantic::geometry::trimesh3& inMesh, const char* inParticle ) {
    if( inMesh.is_empty() )
        return;

    for( std::size_t i = 0; i < outAccessors.vertexAccessors.size(); ++i ) {
        frantic::geometry::trimesh3_vertex_channel_general_accessor& outAccessor = outAccessors.vertexAccessors[i];

        vertex_channel_assignment* assignment = channelAssignment.get_vertex_channel_assignment( i );

        if( assignment ) {
            std::size_t vertexBegin;
            std::size_t faceBegin;
            if( outAccessor.has_custom_faces() ) {
                vertexBegin = outAccessor.size();
                faceBegin = outAccessor.face_count();
                outAccessor.add_vertices( assignment->get_custom_face_data_count() );
                outAccessor.add_faces( inMesh.face_count() );
            } else {
                vertexBegin = outMesh.vertex_count();
                faceBegin = outMesh.face_count();
                outAccessor.add_vertices( inMesh.vertex_count() );
            }
            assignment->set( outAccessor, vertexBegin, faceBegin, xform, xformTimeDerivative, inMesh, inParticle );
        } else {
            if( outAccessor.has_custom_faces() ) {
                const std::size_t dataBegin = outAccessor.size();
                char* outData = outAccessor.add_vertex();
                memset( outData, 0, outAccessor.primitive_size() );
                const frantic::graphics::vector3 face( static_cast<boost::int32_t>( dataBegin ) );
                for( std::size_t faceIndex = 0, faceIndexEnd = inMesh.face_count(); faceIndex != faceIndexEnd;
                     ++faceIndex ) {
                    outAccessor.add_face( face );
                }
            } else {
                const std::size_t dataBegin = outAccessor.size();
                outAccessor.add_vertices( inMesh.vertex_count() );
                memset( outAccessor.data( dataBegin ), 0, inMesh.vertex_count() * outAccessor.primitive_size() );
            }
        }
    }

    for( std::size_t i = 0; i < outAccessors.faceAccessors.size(); ++i ) {
        frantic::geometry::trimesh3_face_channel_general_accessor& outAccessor = outAccessors.faceAccessors[i];

        face_channel_assignment* assignment = channelAssignment.get_face_channel_assignment( i );

        const std::size_t facesBegin = outAccessor.size();
        outAccessor.add_faces( inMesh.face_count() );

        if( assignment ) {
            assignment->set( outAccessor, facesBegin, xform, xformTimeDerivative, inMesh, inParticle );
        } else {
            memset( outAccessor.data( facesBegin ), 0, inMesh.face_count() * outAccessor.primitive_size() );
        }
    }

    const std::size_t firstVertex = outMesh.vertex_count();
    const std::size_t finalVertexCount = firstVertex + inMesh.vertex_count();

    if( finalVertexCount > firstVertex * 14 / 10 ) {
        outMesh.reserve_vertices( finalVertexCount );
    }

    if( xform.is_identity() ) {
        for( std::size_t i = 0, ie = inMesh.vertex_count(); i != ie; ++i ) {
            outMesh.add_vertex( inMesh.get_vertex( i ) );
        }
    } else {
        for( std::size_t i = 0, ie = inMesh.vertex_count(); i != ie; ++i ) {
            outMesh.add_vertex( xform * inMesh.get_vertex( i ) );
        }
    }

    const std::size_t finalFaceCount = outMesh.face_count() + inMesh.face_count();
    if( finalFaceCount > outMesh.face_count() * 14 / 10 ) {
        outMesh.reserve_faces( finalFaceCount );
    }
    for( std::size_t i = 0, ie = inMesh.face_count(); i != ie; ++i ) {
        outMesh.add_face( inMesh.get_face( i ) +
                          frantic::graphics::vector3( static_cast<boost::int32_t>( firstVertex ) ) );
    }
}

} // anonymous namespace

struct channel_name_and_preferred_source {
    frantic::tstring channelName;
    trimesh3_particle_combine::preferred_source_t preferredSource;
    channel_name_and_preferred_source()
        : channelName( _T("") )
        , preferredSource( trimesh3_particle_combine::preferred_source_particle ) {}
    channel_name_and_preferred_source( const frantic::tstring& channelName,
                                       trimesh3_particle_combine::preferred_source_t preferredSource )
        : channelName( channelName )
        , preferredSource( preferredSource ) {}
};

class trimesh3_particle_builder_with_cached_accessors : public trimesh3_particle_builder {
    boost::shared_ptr<frantic::geometry::trimesh3> m_mesh;
    frantic::channels::channel_map m_particleChannelMap;
    typedef std::map<frantic::tstring, channel_name_and_preferred_source> preferred_input_map_t;
    typedef std::map<const frantic::geometry::trimesh3*, boost::shared_ptr<channel_assignment_collection>>
        mesh_to_channel_assignment_t;
    mesh_to_channel_assignment_t m_meshToChannelAssignment;
    std::map<frantic::tstring, channel_name_and_preferred_source> m_vertexChannelInput;
    std::map<frantic::tstring, channel_name_and_preferred_source> m_faceChannelInput;
    std::map<frantic::tstring, trimesh3_particle_combine::trimesh3_transform_t> m_vertexChannelTransformType;
    // face channel name -> desired data type and arity
    typedef std::map<frantic::tstring, std::pair<frantic::channels::data_type_t, std::size_t>> channel_name_to_type_t;
    std::map<frantic::tstring, std::pair<frantic::channels::data_type_t, std::size_t>> m_faceChannelDesiredType;
    trimesh3_particle_combine::trimesh3_transform_t
    get_vertex_channel_transform_type( const frantic::tstring& channelName );
    output_mesh_accessors m_outAccessors;

  public:
    trimesh3_particle_builder_with_cached_accessors();
    trimesh3_particle_builder_with_cached_accessors( const frantic::channels::channel_map& particleChannelMap );
    ~trimesh3_particle_builder_with_cached_accessors() {}
    void assign_vertex_channel_from_particle( const frantic::tstring& outChannelName,
                                              const frantic::tstring& inChannelName,
                                              trimesh3_particle_combine::preferred_source_t preferredSource );
    void assign_face_channel_from_particle( const frantic::tstring& outChannelName,
                                            const frantic::tstring& inChannelName,
                                            trimesh3_particle_combine::preferred_source_t preferredSource );
    void set_vertex_channel_transform_type( const frantic::tstring& channelName,
                                            trimesh3_particle_combine::trimesh3_transform_t transformType );
    void set_face_channel_type( const frantic::tstring& channelName, frantic::channels::data_type_t dataType,
                                std::size_t arity );
    void combine( const frantic::graphics::transform4f& xform,
                  const frantic::graphics::transform4f& xformTimeDerivative, const frantic::geometry::trimesh3& inMesh,
                  const char* inParticle );
    boost::shared_ptr<frantic::geometry::trimesh3> finalize();
};

trimesh3_particle_builder* get_trimesh3_particle_builder( const frantic::channels::channel_map& particleChannelMap ) {
    return new trimesh3_particle_builder_with_cached_accessors( particleChannelMap );
}

trimesh3_particle_combine::trimesh3_transform_t
trimesh3_particle_builder_with_cached_accessors::get_vertex_channel_transform_type(
    const frantic::tstring& channelName ) {
    std::map<frantic::tstring, trimesh3_particle_combine::trimesh3_transform_t>::iterator i =
        m_vertexChannelTransformType.find( channelName );
    if( i == m_vertexChannelTransformType.end() ) {
        return trimesh3_particle_combine::trimesh3_transform_none;
    } else {
        return i->second;
    }
}

trimesh3_particle_builder_with_cached_accessors::trimesh3_particle_builder_with_cached_accessors() {
    m_mesh = boost::make_shared<frantic::geometry::trimesh3>();
}

trimesh3_particle_builder_with_cached_accessors::trimesh3_particle_builder_with_cached_accessors(
    const frantic::channels::channel_map& particleChannelMap )
    : m_particleChannelMap( particleChannelMap ) {
    m_mesh = boost::make_shared<frantic::geometry::trimesh3>();
}

void trimesh3_particle_builder_with_cached_accessors::assign_vertex_channel_from_particle(
    const frantic::tstring& outChannelName, const frantic::tstring& inChannelName,
    trimesh3_particle_combine::preferred_source_t preferredSource ) {
    if( !m_particleChannelMap.has_channel( inChannelName ) ) {
        throw std::runtime_error(
            "assign_vertex_channel_from_particle Error: particle channel map does not have the specified channel: \'" +
            frantic::strings::to_string( inChannelName ) + "\'." );
    }
    m_vertexChannelInput[outChannelName] = channel_name_and_preferred_source( inChannelName, preferredSource );
}

void trimesh3_particle_builder_with_cached_accessors::assign_face_channel_from_particle(
    const frantic::tstring& outChannelName, const frantic::tstring& inChannelName,
    trimesh3_particle_combine::preferred_source_t preferredSource ) {
    if( !m_particleChannelMap.has_channel( inChannelName ) ) {
        throw std::runtime_error(
            "assign_face_channel_from_particle Error: particle channel map does not have the specified channel: \'" +
            frantic::strings::to_string( inChannelName ) + "\'." );
    }
    m_faceChannelInput[outChannelName] = channel_name_and_preferred_source( inChannelName, preferredSource );
}

void trimesh3_particle_builder_with_cached_accessors::set_vertex_channel_transform_type(
    const frantic::tstring& channelName, trimesh3_particle_combine::trimesh3_transform_t transformType ) {
    m_vertexChannelTransformType[channelName] = transformType;
}

void trimesh3_particle_builder_with_cached_accessors::set_face_channel_type( const frantic::tstring& channelName,
                                                                             frantic::channels::data_type_t dataType,
                                                                             std::size_t arity ) {
    m_faceChannelDesiredType[channelName] = std::make_pair( dataType, arity );
}

void trimesh3_particle_builder_with_cached_accessors::combine(
    const frantic::graphics::transform4f& xform, const frantic::graphics::transform4f& xformTimeDerivative,
    const frantic::geometry::trimesh3& inMesh, const char* inParticle ) {
    mesh_to_channel_assignment_t::iterator meshToChannelAssignmentIterator = m_meshToChannelAssignment.find( &inMesh );
    if( meshToChannelAssignmentIterator == m_meshToChannelAssignment.end() ) {
        // create assignments
        boost::shared_ptr<channel_assignment_collection> channelAssignmentCollection =
            boost::make_shared<channel_assignment_collection>();
        { // vertex channels
            std::vector<frantic::tstring> vertexChannelNamesV;
            inMesh.get_vertex_channel_names( vertexChannelNamesV );
            std::set<frantic::tstring> vertexChannelNames( vertexChannelNamesV.begin(), vertexChannelNamesV.end() );
            for( preferred_input_map_t::iterator i = m_vertexChannelInput.begin(), ie = m_vertexChannelInput.end();
                 i != ie; ++i ) {
                const frantic::tstring& channelName = i->first;
                std::set<frantic::tstring>::iterator j = vertexChannelNames.find( channelName );
                if( i->second.preferredSource == trimesh3_particle_combine::preferred_source_particle ||
                    j == vertexChannelNames.end() ) {
                    channelAssignmentCollection->add_vertex_assignment(
                        channelName, i->second.channelName, m_particleChannelMap,
                        get_vertex_channel_transform_type( channelName ) );
                    if( j != vertexChannelNames.end() ) {
                        vertexChannelNames.erase( j );
                    }
                }
            }
            BOOST_FOREACH( const frantic::tstring& vertexChannelName, vertexChannelNames ) {
                channelAssignmentCollection->add_vertex_assignment(
                    vertexChannelName, inMesh, get_vertex_channel_transform_type( vertexChannelName ) );
            }
        }

        { // face channels
            std::vector<frantic::tstring> faceChannelNamesV;
            inMesh.get_face_channel_names( faceChannelNamesV );
            std::set<frantic::tstring> faceChannelNames( faceChannelNamesV.begin(), faceChannelNamesV.end() );
            for( preferred_input_map_t::iterator i = m_faceChannelInput.begin(), ie = m_faceChannelInput.end(); i != ie;
                 ++i ) {
                const frantic::tstring& channelName = i->first;
                std::set<frantic::tstring>::iterator j = faceChannelNames.find( channelName );
                if( i->second.preferredSource == trimesh3_particle_combine::preferred_source_particle ||
                    j == faceChannelNames.end() ) {
                    channelAssignmentCollection->add_face_assignment( channelName, i->second.channelName,
                                                                      m_particleChannelMap );
                    if( j != faceChannelNames.end() ) {
                        faceChannelNames.erase( j );
                    }
                }
            }
            BOOST_FOREACH( const frantic::tstring& faceChannelName, faceChannelNames ) {
                channelAssignmentCollection->add_face_assignment( faceChannelName, inMesh );
            }
        }

        bool hasUpdate =
            channelAssignmentCollection->ensure_mesh_has_assigned_channels( *m_mesh.get(), m_faceChannelDesiredType );
        if( hasUpdate ) {
            hasUpdate |= update_accessors( *m_mesh.get(), m_outAccessors );
        }
        channelAssignmentCollection->update_for_output( m_outAccessors );
        if( hasUpdate ) {
            for( mesh_to_channel_assignment_t::iterator j = m_meshToChannelAssignment.begin(),
                                                        je = m_meshToChannelAssignment.end();
                 j != je; ++j ) {
                j->second->update_for_output( m_outAccessors );
            }
        }
        std::pair<mesh_to_channel_assignment_t::iterator, bool> dest = m_meshToChannelAssignment.insert(
            mesh_to_channel_assignment_t::value_type( &inMesh, channelAssignmentCollection ) );
        meshToChannelAssignmentIterator = dest.first;
    }
    // combine
    channel_assignment_collection& channelAssignment = *( meshToChannelAssignmentIterator->second.get() );
    combine_trimesh3_particle( *m_mesh.get(), m_outAccessors, xform, xformTimeDerivative, channelAssignment, inMesh,
                               inParticle );
}

boost::shared_ptr<frantic::geometry::trimesh3> trimesh3_particle_builder_with_cached_accessors::finalize() {
    return m_mesh;
}

} // namespace geometry
} // namespace frantic
