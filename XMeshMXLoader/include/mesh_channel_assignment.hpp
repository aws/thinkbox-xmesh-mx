// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <MeshNormalSpec.h>
#include <frantic/math/fractions.hpp>
#include <frantic/math/uint128.hpp>

using namespace frantic::geometry;

class vertex_channel_source {
  public:
    typedef boost::shared_ptr<vertex_channel_source> ptr_type;

    virtual ~vertex_channel_source() {}

    virtual std::size_t size() = 0;
    virtual frantic::graphics::vector3f get( std::size_t ) = 0;

    virtual std::size_t face_count() = 0;
    virtual polymesh3_const_face_range get_face( std::size_t ) = 0;

    virtual bool copy_to_verts( Mesh& dest ) {
        bool changedGeometry = false;
        { // scope for reading and writing verts
            std::size_t i = 0;
            const std::size_t iEnd = size();
            // if( ! changedGeometry && ! changedTopology ) {
            for( ; i != iEnd; ++i ) {
                if( dest.getVert( (int)i ) != frantic::max3d::to_max_t( get( i ) ) )
                    break;
            }
            //}
            if( i != iEnd ) {
                changedGeometry = true;
                for( ; i != iEnd; ++i ) {
                    dest.setVert( static_cast<int>( i ), frantic::max3d::to_max_t( get( i ) ) );
                }
            }
        }
        return changedGeometry;
    }
};

template <class T>
class typed_vertex_channel_source {
  public:
    typedef boost::shared_ptr<typed_vertex_channel_source> ptr_type;

    virtual ~typed_vertex_channel_source() {}

    virtual std::size_t size() = 0;
    virtual T get( std::size_t ) = 0;

    virtual std::size_t face_count() = 0;
    virtual polymesh3_const_face_range get_face( std::size_t ) = 0;
};

template <class T>
class face_channel_source {
  public:
    typedef boost::shared_ptr<face_channel_source> ptr_type;

    virtual ~face_channel_source() {}

    virtual std::size_t size() = 0;
    virtual T get( std::size_t ) = 0;
};

class accessor_vertex_channel_source : public vertex_channel_source {
    polymesh3_const_vertex_accessor<frantic::graphics::vector3f> m_acc;

  public:
    accessor_vertex_channel_source( polymesh3_const_vertex_accessor<frantic::graphics::vector3f>& acc )
        : m_acc( acc ) {}

    std::size_t size() { return m_acc.vertex_count(); }

    inline frantic::graphics::vector3f get( std::size_t i ) { return m_acc.get_vertex( i ); }

    std::size_t face_count() { return m_acc.face_count(); }

    polymesh3_const_face_range get_face( std::size_t i ) { return m_acc.get_face( i ); }

    bool copy_to_verts( Mesh& dest ) {
        bool changedGeometry = false;
        { // scope for reading and writing verts
            std::size_t i = 0;
            const std::size_t iEnd = m_acc.vertex_count();
            // if( ! changedGeometry && ! changedTopology ) {
            for( ; i != iEnd; ++i ) {
                if( dest.getVert( (int)i ) != frantic::max3d::to_max_t( m_acc.get_vertex( i ) ) )
                    break;
            }
            //}
            if( i != iEnd ) {
                changedGeometry = true;
                if( ( sizeof( frantic::graphics::vector3f ) == sizeof( Point3 ) ) && iEnd != 0 ) {
                    // mprintf( "match!\n" );
                    memcpy( &dest.verts[0], &m_acc.get_vertex( 0 ), sizeof( Point3 ) * m_acc.vertex_count() );
                } else {
                    // mprintf( "mismatch\n" );
                    for( ; i != iEnd; ++i ) {
                        memcpy( &dest.verts[i], &m_acc.get_vertex( i ), 3 * sizeof( float ) );
                        // dest.setVert( static_cast<int>( i ), frantic::max3d::to_max_t( m_acc.get_vertex( i ) ) );
                    }
                }
            }
        }
        return changedGeometry;
    }
};

class cvt_accessor_vertex_channel_source : public vertex_channel_source {
    polymesh3_const_cvt_vertex_accessor<frantic::graphics::vector3f> m_acc;

  public:
    cvt_accessor_vertex_channel_source( polymesh3_const_cvt_vertex_accessor<frantic::graphics::vector3f>& acc )
        : m_acc( acc ) {}

    std::size_t size() { return m_acc.vertex_count(); }

    frantic::graphics::vector3f get( std::size_t i ) { return m_acc.get_vertex( i ); }

    std::size_t face_count() { return m_acc.face_count(); }

    polymesh3_const_face_range get_face( std::size_t i ) { return m_acc.get_face( i ); }
};

class adapt_arity_accessor_vertex_channel_source : public vertex_channel_source {
    polymesh3_const_vertex_accessor<void> m_acc;
    frantic::channels::channel_type_convertor_function_t m_cvt;
    std::size_t m_cvtArity;

  public:
    adapt_arity_accessor_vertex_channel_source( polymesh3_const_vertex_accessor<void>& acc,
                                                const frantic::tstring& channelNameForErrorMessage )
        : m_acc( acc ) {
        m_cvt = frantic::channels::get_channel_type_convertor_function(
            m_acc.get_type(), frantic::channels::data_type_float32, channelNameForErrorMessage );
        m_cvtArity = std::min<std::size_t>( m_acc.get_arity(), 3 );
    }

    std::size_t size() { return m_acc.vertex_count(); }

    frantic::graphics::vector3f get( std::size_t i ) {
        frantic::graphics::vector3f result;
        m_cvt( reinterpret_cast<char*>( &result[0] ), m_acc.get_vertex( i ), m_cvtArity );
        return result;
    }

    std::size_t face_count() { return m_acc.face_count(); }

    polymesh3_const_face_range get_face( std::size_t faceIndex ) { return m_acc.get_face( faceIndex ); }
};

class delegated_vertex_channel_source : public vertex_channel_source {
  protected:
    vertex_channel_source::ptr_type m_source;

  public:
    delegated_vertex_channel_source( vertex_channel_source::ptr_type source )
        : m_source( source ) {}

    std::size_t size() { return m_source->size(); }

    std::size_t face_count() { return m_source->face_count(); }

    polymesh3_const_face_range get_face( std::size_t i ) { return m_source->get_face( i ); }
};

class scaled_vertex_channel_source : public delegated_vertex_channel_source {
    float m_scale;

  public:
    scaled_vertex_channel_source( vertex_channel_source::ptr_type source, float scale )
        : delegated_vertex_channel_source( source )
        , m_scale( scale ) {}

    frantic::graphics::vector3f get( std::size_t i ) { return m_scale * m_source->get( i ); }
};

class transform_point_vertex_channel_source : public delegated_vertex_channel_source {
    frantic::graphics::transform4f m_transform;

  public:
    transform_point_vertex_channel_source( vertex_channel_source::ptr_type source,
                                           const frantic::graphics::transform4f& transform )
        : delegated_vertex_channel_source( source )
        , m_transform( transform ) {}

    frantic::graphics::vector3f get( std::size_t i ) { return m_transform * m_source->get( i ); }
};

class transform_vector_vertex_channel_source : public delegated_vertex_channel_source {
    frantic::graphics::transform4f m_transform;

  public:
    transform_vector_vertex_channel_source( vertex_channel_source::ptr_type source,
                                            const frantic::graphics::transform4f& transform )
        : delegated_vertex_channel_source( source )
        , m_transform( transform ) {}

    frantic::graphics::vector3f get( std::size_t i ) {
        return m_transform.transform_no_translation( m_source->get( i ) );
    }
};

class transform_normal_vertex_channel_source : public delegated_vertex_channel_source {
    frantic::graphics::transform4f m_invTransform;

  public:
    transform_normal_vertex_channel_source( vertex_channel_source::ptr_type source,
                                            const frantic::graphics::transform4f& transform )
        : delegated_vertex_channel_source( source ) {
        m_invTransform = transform.to_inverse();
    }

    frantic::graphics::vector3f get( std::size_t i ) {
        return m_invTransform.transpose_transform_no_translation( m_source->get( i ) );
    }
};

class velocity_offset_vertex_channel_source : public vertex_channel_source {
    vertex_channel_source::ptr_type m_position;
    vertex_channel_source::ptr_type m_velocity;
    float m_timeOffset;

  public:
    velocity_offset_vertex_channel_source( vertex_channel_source::ptr_type position,
                                           vertex_channel_source::ptr_type velocity, float timeOffset )
        : m_position( position )
        , m_velocity( velocity )
        , m_timeOffset( timeOffset ) {
        if( position->size() != velocity->size() ) {
            throw std::runtime_error( "velocity_offset_vertex_channel_source Mismatch between Position vertex count (" +
                                      boost::lexical_cast<std::string>( position->size() ) +
                                      ") and Velocity vertex count (" +
                                      boost::lexical_cast<std::string>( velocity->size() ) + ")" );
        }
    }

    std::size_t size() { return m_position->size(); }

    frantic::graphics::vector3f get( std::size_t i ) {
        return m_position->get( i ) + m_timeOffset * m_velocity->get( i );
    }

    std::size_t face_count() { return m_position->face_count(); }

    polymesh3_const_face_range get_face( std::size_t i ) { return m_position->get_face( i ); }
};

class fractional_index_filter {
    boost::int64_t m_lastOutput;
    boost::int64_t m_lastInput;
    boost::int64_t m_numerator;
    boost::int64_t m_denominator;
    boost::int64_t m_accumulator;
    boost::int64_t m_totalCount;

  public:
    fractional_index_filter( boost::int64_t totalCount, double fraction ) {
        m_lastOutput = std::numeric_limits<boost::int64_t>::max();
        m_lastInput = std::numeric_limits<boost::int64_t>::max();
        m_totalCount = totalCount;

        fraction = frantic::math::clamp( fraction, 0.0, 1.0 );

        std::pair<boost::int64_t, boost::int64_t> rational = frantic::math::get_rational_representation( fraction );
        m_numerator = rational.first;
        m_denominator = rational.second;
        m_accumulator = 0;

        std::cout << boost::lexical_cast<std::string>( m_numerator ) << std::endl
                  << boost::lexical_cast<std::string>( m_denominator ) << std::endl;

        using frantic::math::uint128;
        boost::int64_t limit = std::numeric_limits<boost::int64_t>::max();
        m_totalCount = std::min(
            limit, ( uint128( m_totalCount ) * uint128( m_numerator ) / m_denominator ).to_integral<boost::int64_t>() );
    }
    boost::int64_t get_input_for_output( boost::int64_t output ) {
        if( output == m_lastOutput ) {
            return m_lastInput;
        } else if( output > m_lastOutput ) {
            boost::int64_t input = m_lastInput;
            bool done = false;

            while( !done ) {
                ++input;
                m_accumulator += m_numerator;
                if( m_accumulator >= m_denominator ) {
                    m_accumulator -= m_denominator;
                    done = true;
                }
            }

            m_lastInput = input;
            m_lastOutput = output;

            return m_lastInput;
        } else {
            m_accumulator = 0;
            boost::int64_t outputAcc = -1;
            boost::int64_t input = -1;

            while( outputAcc != output ) {
                ++input;
                m_accumulator += m_numerator;
                if( m_accumulator >= m_denominator ) {
                    m_accumulator -= m_denominator;
                    ++outputAcc;
                }
            }

            m_lastInput = input;
            m_lastOutput = output;

            return m_lastInput;
        }
    }
    std::size_t size() { return static_cast<std::size_t>( m_totalCount ); }
};

class fractional_vertex_channel_source : public vertex_channel_source {
    vertex_channel_source::ptr_type m_source;
    std::size_t m_totalCount;
    fractional_index_filter m_filter;

  public:
    fractional_vertex_channel_source( vertex_channel_source::ptr_type source, double fraction )
        : m_source( source )
        , m_filter( source->size(), fraction ) {
        m_totalCount = m_filter.size();
    }

    std::size_t size() { return m_totalCount; }

    std::size_t face_count() { return 0; }

    polymesh3_const_face_range get_face( std::size_t /*i*/ ) {
        return polymesh3_const_face_range( (frantic::geometry::polymesh3_const_face_iterator)0,
                                           (frantic::geometry::polymesh3_const_face_iterator)0 );
    }

    frantic::graphics::vector3f get( std::size_t i ) {
        return m_source->get( static_cast<std::size_t>( m_filter.get_input_for_output( i ) ) );
    }
};

class fractional_face_vertex_channel_source : public vertex_channel_source {
    vertex_channel_source::ptr_type m_source;
    std::size_t m_totalCount;
    fractional_index_filter m_filter;

  public:
    fractional_face_vertex_channel_source( vertex_channel_source::ptr_type source, double fraction )
        : m_source( source )
        , m_filter( source->face_count(), fraction ) {
        m_totalCount = m_filter.size();
    }

    std::size_t size() { return m_source->size(); }

    std::size_t face_count() { return m_totalCount; }

    polymesh3_const_face_range get_face( std::size_t i ) {
        return m_source->get_face( static_cast<std::size_t>( m_filter.get_input_for_output( i ) ) );
    }

    frantic::graphics::vector3f get( std::size_t i ) { return m_source->get( i ); }
};

template <class T>
class accessor_typed_vertex_channel_source : public typed_vertex_channel_source<T> {
    polymesh3_const_vertex_accessor<T> m_acc;

  public:
    accessor_typed_vertex_channel_source( polymesh3_const_vertex_accessor<T>& acc )
        : m_acc( acc ) {}

    std::size_t size() { return m_acc.vertex_count(); }

    inline T get( std::size_t i ) { return m_acc.get_vertex( i ); }

    std::size_t face_count() { return m_acc.face_count(); }

    polymesh3_const_face_range get_face( std::size_t i ) { return m_acc.get_face( i ); }
};

template <class T>
class cvt_accessor_typed_vertex_channel_source : public typed_vertex_channel_source<T> {
    polymesh3_const_cvt_vertex_accessor<T> m_acc;

  public:
    cvt_accessor_typed_vertex_channel_source( polymesh3_const_cvt_vertex_accessor<T>& acc )
        : m_acc( acc ) {}

    std::size_t size() { return m_acc.vertex_count(); }

    inline T get( std::size_t i ) { return m_acc.get_vertex( i ); }

    std::size_t face_count() { return m_acc.face_count(); }

    polymesh3_const_face_range get_face( std::size_t i ) { return m_acc.get_face( i ); }
};

template <class T>
class fractional_typed_vertex_channel_source : public typed_vertex_channel_source<T> {
    typed_vertex_channel_source<T>::ptr_type m_source;
    std::size_t m_totalCount;
    fractional_index_filter m_filter;

  public:
    fractional_typed_vertex_channel_source( typed_vertex_channel_source<T>::ptr_type source, double fraction )
        : m_source( source )
        , m_filter( source->size(), fraction ) {
        m_totalCount = m_filter.size();
    }

    std::size_t size() { return m_totalCount; }

    T get( std::size_t i ) { return m_source->get( static_cast<std::size_t>( m_filter.get_input_for_output( i ) ) ); }

    std::size_t face_count() { return m_acc.face_count(); }

    polymesh3_const_face_range get_face( std::size_t i ) { return m_acc.get_face( i ); }
};

template <class T>
class fractional_face_typed_vertex_channel_source : public typed_vertex_channel_source<T> {
    typed_vertex_channel_source<T>::ptr_type m_source;
    std::size_t m_totalCount;
    fractional_index_filter m_filter;

  public:
    fractional_face_typed_vertex_channel_source( typed_vertex_channel_source<T>::ptr_type source, double fraction )
        : m_source( source )
        , m_filter( source->face_count(), fraction ) {
        m_totalCount = m_filter.size();
    }

    std::size_t size() { return m_source->size(); }

    T get( std::size_t i ) { return m_source->get( i ); }

    std::size_t face_count() { return m_totalCount; }

    polymesh3_const_face_range get_face( std::size_t i ) {
        return m_source->get_face( static_cast<std::size_t>( m_filter.get_input_for_output( i ) ) );
    }
};

template <class T>
class accessor_face_channel_source : public face_channel_source<T> {
    polymesh3_const_face_accessor<T> m_acc;

  public:
    accessor_face_channel_source( polymesh3_const_face_accessor<T>& acc )
        : m_acc( acc ) {}

    std::size_t size() { return m_acc.face_count(); }

    T get( std::size_t i ) { return m_acc.get_face( i ); }
};

template <class T>
class cvt_accessor_face_channel_source : public face_channel_source<T> {
    polymesh3_const_cvt_face_accessor<T> m_acc;

  public:
    cvt_accessor_face_channel_source( polymesh3_const_cvt_face_accessor<T>& acc )
        : m_acc( acc ) {}

    std::size_t size() { return m_acc.face_count(); }

    T get( std::size_t i ) { return m_acc.get_face( i ); }
};

template <class T>
class delegated_face_channel_source : public face_channel_source<T> {
  protected:
    face_channel_source<T>::ptr_type m_source;

  public:
    delegated_face_channel_source( face_channel_source<T>::ptr_type source )
        : m_source( source ) {}

    std::size_t size() { return m_source->size(); }
};

class transform_point_face_channel_source : public delegated_face_channel_source<frantic::graphics::vector3f> {
    frantic::graphics::transform4f m_transform;

  public:
    transform_point_face_channel_source( face_channel_source<frantic::graphics::vector3f>::ptr_type source,
                                         const frantic::graphics::transform4f& transform )
        : delegated_face_channel_source( source )
        , m_transform( transform ) {}

    frantic::graphics::vector3f get( std::size_t i ) { return m_transform * m_source->get( i ); }
};

class transform_vector_face_channel_source : public delegated_face_channel_source<frantic::graphics::vector3f> {
    frantic::graphics::transform4f m_transform;

  public:
    transform_vector_face_channel_source( face_channel_source<frantic::graphics::vector3f>::ptr_type source,
                                          const frantic::graphics::transform4f& transform )
        : delegated_face_channel_source( source )
        , m_transform( transform ) {}

    frantic::graphics::vector3f get( std::size_t i ) {
        return m_transform.transform_no_translation( m_source->get( i ) );
    }
};

class transform_normal_face_channel_source : public delegated_face_channel_source<frantic::graphics::vector3f> {
    frantic::graphics::transform4f m_invTransform;

  public:
    transform_normal_face_channel_source( face_channel_source<frantic::graphics::vector3f>::ptr_type source,
                                          const frantic::graphics::transform4f& transform )
        : delegated_face_channel_source( source ) {
        m_invTransform = transform.to_inverse();
    }

    frantic::graphics::vector3f get( std::size_t i ) {
        m_invTransform.transpose_transform_no_translation( m_source->get( i ) );
    }
};

template <class T>
class fractional_face_channel_source : public face_channel_source<T> {
    face_channel_source<T>::ptr_type m_source;
    std::size_t m_totalCount;
    fractional_index_filter m_filter;

  public:
    fractional_face_channel_source( face_channel_source<T>::ptr_type source, double fraction )
        : m_source( source )
        , m_filter( source->size(), fraction ) {
        m_totalCount = m_filter.size();
    }

    std::size_t size() { return m_totalCount; }

    T get( std::size_t i ) { return m_source->get( static_cast<std::size_t>( m_filter.get_input_for_output( i ) ) ); }
};

struct mesh_channel_assignment {
    vertex_channel_source::ptr_type verts;
    vertex_channel_source::ptr_type vertexNormal;
    std::vector<vertex_channel_source::ptr_type> maps;
    typed_vertex_channel_source<float>::ptr_type vertexSelection;
    typed_vertex_channel_source<float>::ptr_type edgeCrease;
    typed_vertex_channel_source<float>::ptr_type vertexCrease;

    face_channel_source<int>::ptr_type smoothingGroup;
    face_channel_source<MtlID>::ptr_type materialID;
    face_channel_source<boost::int32_t>::ptr_type faceSelection;
    face_channel_source<boost::int8_t>::ptr_type faceEdgeVisibility;

    mesh_channel_assignment() { maps.resize( MAX_MESHMAPS ); }
};

struct mesh_channel_assignment_uid {
    typedef boost::uint64_t uid_t;
    typedef struct {
        boost::uint64_t data;
        boost::uint64_t faces;
        void clear() {
            data = 0;
            faces = 0;
        }
    } map_uid_t;

    uid_t verts;
    uid_t faces;

    uid_t smoothingGroup;
    uid_t materialID;

    std::vector<map_uid_t> maps;

    mesh_channel_assignment_uid() {
        maps.resize( MAX_MESHMAPS );
        clear();
    }

    void clear() {
        verts = 0;
        faces = 0;
        smoothingGroup = 0;
        materialID = 0;
        for( std::vector<map_uid_t>::iterator i = maps.begin(); i != maps.end(); ++i ) {
            i->clear();
        }
    }
};

inline bool is_triangle_mesh( vertex_channel_source& verts ) {
    for( std::size_t i = 0, ie = verts.face_count(); i != ie; ++i ) {
        polymesh3_const_face_range face = verts.get_face( i );
        if( ( face.second - face.first ) != 3 ) {
            return false;
        }
    }
    return true;
}

inline vertex_channel_source::ptr_type create_map_vertex_channel_source( frantic::geometry::const_polymesh3_ptr mesh,
                                                                         const frantic::tstring& channelName ) {
    if( !mesh ) {
        throw std::runtime_error( "create_map_vertex_channel_source Error: mesh is NULL" );
    }

    polymesh3_const_vertex_accessor<void> acc = mesh->get_const_vertex_accessor( channelName );
    switch( acc.get_arity() ) {
    case 1:
        return vertex_channel_source::ptr_type();
    case 2:
        return vertex_channel_source::ptr_type( new adapt_arity_accessor_vertex_channel_source(
            mesh->get_const_vertex_accessor( channelName ), channelName ) );
    case 3:
        if( acc.get_type() == frantic::channels::data_type_float32 ) {
            return vertex_channel_source::ptr_type( new accessor_vertex_channel_source(
                mesh->get_const_vertex_accessor<frantic::graphics::vector3f>( channelName ) ) );
        } else {
            return vertex_channel_source::ptr_type( new cvt_accessor_vertex_channel_source(
                mesh->get_const_cvt_vertex_accessor<frantic::graphics::vector3f>( channelName ) ) );
        }
    case 4:
        return vertex_channel_source::ptr_type( new adapt_arity_accessor_vertex_channel_source(
            mesh->get_const_vertex_accessor( channelName ), channelName ) );
    default:
        throw std::runtime_error( "create_map_vertex_channel_source Error: unexpected arity " +
                                  boost::lexical_cast<std::string>( acc.get_arity() ) + " in vertex channel \"" +
                                  frantic::strings::to_string( channelName ) + "\"" );
    }
}

template <class T, T val>
class constant_face_data {
  public:
    T get( std::size_t ) const { return val; }
};

template <class T>
class native_face_data {
    frantic::geometry::polymesh3_const_face_accessor<void>& m_acc;
    native_face_data& operator=( const native_face_data& ); // not implemented
  public:
    native_face_data( frantic::geometry::polymesh3_const_face_accessor<void>& acc )
        : m_acc( acc ) {}
    T get( std::size_t i ) const { return *reinterpret_cast<const T*>( m_acc.get_face( i ) ); }
};

template <class T>
class cvt_face_data {
    frantic::geometry::polymesh3_const_face_accessor<void>& m_acc;
    std::size_t m_arity;
    frantic::channels::channel_type_convertor_function_t m_convertType;
    cvt_face_data<T>& operator=( const cvt_face_data<T>& ); // not implemented
  public:
    cvt_face_data( frantic::geometry::polymesh3_const_face_accessor<void>& acc,
                   const std::string& channelNameForErrorMessage )
        : m_acc( acc ) {
        if( m_acc.get_arity() != frantic::channels::channel_data_type_traits<T>::arity() ) {
            throw std::runtime_error(
                "Arity mismatch for channel \'" + channelNameForErrorMessage + "\'.  Input has arity " +
                boost::lexical_cast<std::string>( m_acc.get_arity() ) + ", while output has arity " +
                boost::lexical_cast<std::string>( frantic::channels::channel_data_type_traits<T>::arity() ) + "." );
        }
        m_arity = m_acc.get_arity();
        m_convertType = frantic::channels::get_channel_type_convertor_function(
            m_acc.get_type(), frantic::channels::channel_data_type_traits<T>::data_type(), channelNameForErrorMessage );
    }
    T get_face( std::size_t i ) const {
        T out = 0;
        m_convertType( reinterpret_cast<char*>( out ), m_acc.get_face( i ), m_arity );
        return out;
    }
};

template <class GeomAcc, class SmoothingGroupAcc, class MaterialIDAcc, class EdgeVisibilityAcc>
bool copy_face_channels( Mesh& dest, bool changedTopology, GeomAcc& vertAcc, SmoothingGroupAcc& smAcc,
                         MaterialIDAcc& matAcc, EdgeVisibilityAcc& edgeVisAcc ) {
    if( dest.getNumFaces() != (int)vertAcc.face_count() ) {
        dest.setNumFaces( (int)vertAcc.face_count() );
        changedTopology = true;
    }

    std::size_t i = 0;
    const std::size_t ie = vertAcc.face_count();

    if( !changedTopology ) {
        const DWORD flagsMask = ( (DWORD)FACE_MATID_MASK << FACE_MATID_SHIFT ) | (DWORD)EDGE_ALL;
        for( ; i != ie; ++i ) {
            frantic::geometry::polymesh3_const_face_range f = vertAcc.get_face( i );
            if( dest.faces[(int)i].v[0] != static_cast<DWORD>( f.first[0] ) ||
                dest.faces[(int)i].v[1] != static_cast<DWORD>( f.first[1] ) ||
                dest.faces[(int)i].v[2] != static_cast<DWORD>( f.first[2] ) ) {
                break;
            }
            if( dest.faces[(int)i].getSmGroup() != static_cast<DWORD>( smAcc.get( i ) ) ) {
                break;
            }
            const DWORD srcMatFlags = ( static_cast<DWORD>( matAcc.get( i ) ) << FACE_MATID_SHIFT );
            const DWORD srcVisFlags = ( edgeVisAcc.get( i ) & (DWORD)EDGE_ALL );
            const DWORD srcFlags = srcMatFlags | srcVisFlags;
            if( ( dest.faces[(int)i].flags & flagsMask ) != srcFlags ) {
                break;
            }
        }
    }

    if( i != ie ) {
        changedTopology = true;
        for( ; i != ie; ++i ) {
            frantic::geometry::polymesh3_const_face_range f = vertAcc.get_face( i );
            dest.faces[(int)i].setVerts( f.first[0], f.first[1], f.first[2] );
            dest.faces[(int)i].setSmGroup( smAcc.get( i ) );
            dest.faces[(int)i].flags =
                ( static_cast<DWORD>( matAcc.get( i ) ) << FACE_MATID_SHIFT ) | ( edgeVisAcc.get( i ) & EDGE_ALL );
        }
    }

    return changedTopology;
}

inline std::size_t get_num_maps( const std::vector<vertex_channel_source::ptr_type>& maps ) {
    std::size_t numMaps = 0;
    for( std::size_t i = 0; i < maps.size(); ++i ) {
        if( maps[i] ) {
            numMaps = 1 + i;
        }
    }
    return numMaps;
}

// return true if data is changed
// TODO: Currently scans through face data, to see if they're unchanged.  Does this make sense?
// We may be able to get the same information from elsewhere
inline bool copy_face_channels( Mesh& dest, mesh_channel_assignment& channels, bool changedTopology ) {
    vertex_channel_source& vertAcc = *channels.verts;

    const bool hasSmoothingGroup = ( channels.smoothingGroup != 0 );
    const bool hasMaterialID = ( channels.materialID != 0 );
    const bool hasEdgeVisibility = ( channels.faceEdgeVisibility != 0 );

    face_channel_source<int>::ptr_type smAcc = channels.smoothingGroup;
    face_channel_source<MtlID>::ptr_type matAcc = channels.materialID;
    face_channel_source<boost::int8_t>::ptr_type visAcc = channels.faceEdgeVisibility;

    constant_face_data<int, 1> defaultSmoothingGroup;
    constant_face_data<MtlID, 0> defaultMaterialID;
    constant_face_data<boost::int8_t, EDGE_ALL> defaultEdgeVisibility;

    if( hasSmoothingGroup && hasMaterialID && hasEdgeVisibility ) {
        return copy_face_channels( dest, changedTopology, vertAcc, *smAcc, *matAcc, *visAcc );
    } else if( hasSmoothingGroup && hasMaterialID ) {
        return copy_face_channels( dest, changedTopology, vertAcc, *smAcc, *matAcc, defaultEdgeVisibility );
    } else if( hasSmoothingGroup && hasEdgeVisibility ) {
        return copy_face_channels( dest, changedTopology, vertAcc, *smAcc, defaultMaterialID, *visAcc );
    } else if( hasSmoothingGroup ) {
        return copy_face_channels( dest, changedTopology, vertAcc, *smAcc, defaultMaterialID, defaultEdgeVisibility );
    } else if( hasMaterialID && hasEdgeVisibility ) {
        return copy_face_channels( dest, changedTopology, vertAcc, defaultSmoothingGroup, *matAcc, *visAcc );
    } else if( hasMaterialID ) {
        return copy_face_channels( dest, changedTopology, vertAcc, defaultSmoothingGroup, *matAcc,
                                   defaultEdgeVisibility );
    } else if( hasEdgeVisibility ) {
        return copy_face_channels( dest, changedTopology, vertAcc, defaultSmoothingGroup, defaultMaterialID, *visAcc );
    } else {
        return copy_face_channels( dest, changedTopology, vertAcc, defaultSmoothingGroup, defaultMaterialID,
                                   defaultEdgeVisibility );
    }
}

inline void polymesh_copy( Mesh& dest, mesh_channel_assignment_uid& outUid, mesh_channel_assignment& channels,
                           mesh_channel_assignment_uid& inUid ) {
    // mprintf( "copy to Mesh\n" );
    if( !channels.verts ) {
        throw std::runtime_error( "polymesh_copy() verts is NULL" );
    }
    /*
    if( ! is_triangle_mesh( *channels.verts ) ) {
            throw std::runtime_error( "polymesh_copy() the input is not a triangle mesh" );
    }
    */

    vertex_channel_source& verts = *channels.verts;

    // Clear left-over mesh data
    // TODO: Do we need to clear anything else?
    dest.ClearVertexWeights();
    dest.ClearVSelectionWeights();

    bool changedGeometry = false;
    bool changedTopology = false;
    bool changedChannel = false;

    if( dest.getNumVerts() != (int)verts.size() ) {
        dest.setNumVerts( (int)verts.size() );
        changedGeometry = true;
        changedTopology = true;
    }
    if( dest.getNumFaces() != (int)verts.face_count() ) {
        dest.setNumFaces( (int)verts.face_count() );
        changedTopology = true;
    }

    if( !inUid.verts || inUid.verts != outUid.verts ) {
        changedGeometry |= verts.copy_to_verts( dest );
        outUid.verts = inUid.verts;
    }
    /*
    { // scope for reading and writing verts
            std::size_t i = 0;
            const std::size_t iEnd = verts.size();
            if( ! changedGeometry && ! changedTopology ) {
                    for( ; i != iEnd; ++i ) {
                            if( dest.getVert( (int)i ) != frantic::max3d::to_max_t( verts.get( i ) ) )
                                    break;
                    }
            }
            if( i != iEnd ) {
                    changedGeometry = true;
                    for( ; i != iEnd; ++i ) {
                            dest.setVert( static_cast<int>( i ), frantic::max3d::to_max_t( verts.get( i ) ) );
                    }
            }
    }
    */

    if( !inUid.faces || inUid.faces != outUid.faces || !inUid.smoothingGroup ||
        inUid.smoothingGroup != outUid.smoothingGroup || !inUid.materialID || inUid.materialID != outUid.materialID ) {
        changedTopology |= copy_face_channels( dest, channels, changedTopology );

        outUid.faces = inUid.faces;
        outUid.smoothingGroup = inUid.smoothingGroup;
        outUid.materialID = inUid.materialID;
    }

    for( std::size_t i = 0, ie = std::min<std::size_t>( channels.maps.size(), MAX_MESHMAPS ); i < ie; ++i ) {
        if( !channels.maps[i] ) {
            dest.setMapSupport( (int)i, FALSE );
            outUid.maps[i].clear();
        }
    }
    dest.setNumMaps( (int)get_num_maps( channels.maps ) );

    for( std::size_t mapIndex = 0; mapIndex < channels.maps.size(); ++mapIndex ) {
        if( channels.maps[mapIndex] ) {
            vertex_channel_source& ch = *channels.maps[mapIndex];
            const int mapNum = static_cast<int>( mapIndex );
            std::size_t numFaces = ch.face_count() ? ch.face_count() : verts.face_count();
            std::size_t numVerts = ch.size();

            if( !dest.mapSupport( mapNum ) ) {
                dest.setMapSupport( mapNum );
            }

            MeshMap& map = dest.Map( mapNum );
            if( map.vnum != (int)numVerts )
                map.setNumVerts( (int)numVerts );
            if( map.fnum != (int)numFaces )
                map.setNumFaces( (int)numFaces );

            UVVert* pMapVerts = map.tv;
            if( !inUid.maps[mapIndex].faces || inUid.maps[mapIndex].faces != outUid.maps[mapIndex].faces ) {
                for( std::size_t i = 0; i < numVerts; ++i ) {
                    pMapVerts[i] = frantic::max3d::to_max_t( ch.get( i ) );
                }
                outUid.maps[mapIndex].faces = inUid.maps[mapIndex].faces;
                changedChannel = true;
            }

            if( !inUid.maps[mapIndex].data || inUid.maps[mapIndex].data != outUid.maps[mapIndex].data ) {
                TVFace* pTVFaces = dest.mapFaces( mapNum );
                if( ch.face_count() ) {
                    for( std::size_t i = 0; i < numFaces; ++i ) {
                        const DWORD* face = (const DWORD*)ch.get_face( i ).first;
                        pTVFaces[i].setTVerts( face[0], face[1], face[2] );
                    }
                } else {
                    for( std::size_t i = 0; i < numFaces; ++i ) {
                        const DWORD* face = (const DWORD*)verts.get_face( i ).first;
                        pTVFaces[i].setTVerts( face[0], face[1], face[2] );
                    }
                }
                outUid.maps[mapIndex].data = inUid.maps[mapIndex].data;
                changedChannel = true;
            }
        }
    }

    if( changedTopology ) {
        dest.InvalidateTopologyCache();
    } else if( changedGeometry ) {
        dest.InvalidateGeomCache();
    } else if( changedChannel ) {
        dest.SetFlag( MESH_PARTIALCACHEINVALID );
        dest.InvalidateGeomCache();
    }
}

inline void polymesh_copy( Mesh& dest, mesh_channel_assignment& channels ) {
    // mprintf( "copy to Mesh\n" );
    if( !channels.verts ) {
        throw std::runtime_error( "polymesh_copy() verts is NULL" );
    }
    /*
    if( ! is_triangle_mesh( *channels.verts ) ) {
            throw std::runtime_error( "polymesh_copy() the input is not a triangle mesh" );
    }
    */

    vertex_channel_source& verts = *channels.verts;

    // Clear left-over mesh data
    // TODO: Do we need to clear anything else?
    dest.ClearVertexWeights();
    dest.ClearVSelectionWeights();

    bool changedGeometry = false;
    bool changedTopology = false;
    bool changedChannel = false;

    if( dest.getNumVerts() != (int)verts.size() ) {
        dest.setNumVerts( (int)verts.size() );
        changedGeometry = true;
        changedTopology = true;
    }
    if( dest.getNumFaces() != (int)verts.face_count() ) {
        dest.setNumFaces( (int)verts.face_count() );
        changedTopology = true;
    }

    changedGeometry |= verts.copy_to_verts( dest );

    /*
    { // scope for reading and writing verts
            std::size_t i = 0;
            const std::size_t iEnd = verts.size();
            if( ! changedGeometry && ! changedTopology ) {
                    for( ; i != iEnd; ++i ) {
                            if( dest.getVert( (int)i ) != frantic::max3d::to_max_t( verts.get( i ) ) )
                                    break;
                    }
            }
            if( i != iEnd ) {
                    changedGeometry = true;
                    for( ; i != iEnd; ++i ) {
                            dest.setVert( static_cast<int>( i ), frantic::max3d::to_max_t( verts.get( i ) ) );
                    }
            }
    }
    */

    changedTopology |= copy_face_channels( dest, channels, changedTopology );

    if( channels.vertexSelection ) {
        if( !dest.vDataSupport( VDATA_SELECT ) ) {
            dest.SupportVSelectionWeights();
        }
        for( std::size_t i = 0, ie = channels.vertexSelection->size(); i != ie; ++i ) {
            dest.getVSelectionWeights()[static_cast<int>( i )] = channels.vertexSelection->get( i );
        }
        dest.selLevel = MESH_VERTEX;

        changedChannel = true;
    }

    if( channels.faceSelection ) {
        int numBits = dest.FaceSel().GetSize();
        if( numBits != dest.getNumFaces() ) {
            dest.FaceSel().SetSize( dest.getNumFaces(), TRUE );
        }
        for( std::size_t i = 0, ie = channels.faceSelection->size(); i != ie; ++i ) {
            dest.FaceSel().Set( static_cast<int>( i ), channels.faceSelection->get( i ) != 0 );
        }
        dest.selLevel = MESH_FACE;

        changedChannel = true;
    }

    if( channels.vertexNormal ) {
        vertex_channel_source& ch = *channels.vertexNormal;
        vertex_channel_source& faceCh = ch.face_count() > 0 ? ch : verts;

        // TODO: what do we need to invalidate when the normals change?
        changedTopology = true;
        changedGeometry = true;

        dest.SpecifyNormals();
        MeshNormalSpec* normalSpec = dest.GetSpecifiedNormals();
        if( !normalSpec ) {
            throw std::runtime_error( "polymesh_copy() Unable to specify normals" );
        }
        normalSpec->SetParent( &dest );

        if( !normalSpec->SetNumNormals( static_cast<int>( ch.size() ) ) ) {
            throw std::runtime_error( "polymesh_copy() Unable to allocate normals" );
        }
        if( !normalSpec->SetNumFaces( static_cast<int>( faceCh.face_count() ) ) ) {
            throw std::runtime_error( "polymesh_copy() Unable to allocate normal faces" );
        }

        Point3* normalArray = normalSpec->GetNormalArray();
        for( std::size_t i = 0, iEnd = ch.size(); i != iEnd; ++i ) {
            normalArray[i] = frantic::max3d::to_max_t( ch.get( i ) );
        }
        normalSpec->SetAllExplicit();

        for( std::size_t faceIndex = 0, faceIndexEnd = faceCh.face_count(); faceIndex != faceIndexEnd; ++faceIndex ) {
            frantic::geometry::polymesh3_const_face_range f = faceCh.get_face( faceIndex );

            MeshNormalFace& normalFace = normalSpec->Face( static_cast<int>( faceIndex ) );
            for( int corner = 0; corner < 3; ++corner ) {
                normalFace.SetNormalID( corner, f.first[corner] );
            }
            normalFace.SpecifyAll();
        }
        normalSpec->CheckNormals();
    } else {
        dest.ClearSpecifiedNormals();
    }

    for( std::size_t i = 0, ie = std::min<std::size_t>( channels.maps.size(), MAX_MESHMAPS ); i < ie; ++i ) {
        if( !channels.maps[i] ) {
            dest.setMapSupport( (int)i, FALSE );
        }
    }
    dest.setNumMaps( (int)get_num_maps( channels.maps ) );

    for( std::size_t mapIndex = 0; mapIndex < channels.maps.size(); ++mapIndex ) {
        if( channels.maps[mapIndex] ) {
            vertex_channel_source& ch = *channels.maps[mapIndex];
            const int mapNum = static_cast<int>( mapIndex );
            std::size_t numFaces = ch.face_count() ? ch.face_count() : verts.face_count();
            std::size_t numVerts = ch.size();

            if( !dest.mapSupport( mapNum ) ) {
                dest.setMapSupport( mapNum );
            }

            MeshMap& map = dest.Map( mapNum );
            if( map.vnum != (int)numVerts )
                map.setNumVerts( (int)numVerts );
            if( map.fnum != (int)numFaces )
                map.setNumFaces( (int)numFaces );

            UVVert* pMapVerts = map.tv;
            for( std::size_t i = 0; i < numVerts; ++i ) {
                pMapVerts[i] = frantic::max3d::to_max_t( ch.get( i ) );
            }

            TVFace* pTVFaces = dest.mapFaces( mapNum );
            if( ch.face_count() ) {
                for( std::size_t i = 0; i < numFaces; ++i ) {
                    const DWORD* face = (const DWORD*)ch.get_face( i ).first;
                    pTVFaces[i].setTVerts( face[0], face[1], face[2] );
                }
            } else {
                for( std::size_t i = 0; i < numFaces; ++i ) {
                    const DWORD* face = (const DWORD*)verts.get_face( i ).first;
                    pTVFaces[i].setTVerts( face[0], face[1], face[2] );
                }
            }
            changedChannel = true;
        }
    }

    if( changedTopology ) {
        // note: InvalidateTopologyCache() calls InvalidateGeomCache() internally
        dest.InvalidateTopologyCache();
    } else if( changedGeometry ) {
        dest.InvalidateGeomCache();
    } else if( changedChannel ) {
        dest.SetFlag( MESH_PARTIALCACHEINVALID );
        dest.InvalidateGeomCache();
    }
}

inline void polymesh_copy( MNMesh& dest, mesh_channel_assignment& channels ) {
    dest.Clear();

    if( !channels.verts ) {
        throw std::runtime_error( "polymesh_copy() verts is NULL" );
    }
    vertex_channel_source& verts = *channels.verts;

    dest.setNumVerts( (int)verts.size() );
    dest.setNumFaces( (int)verts.face_count() );

    for( std::size_t i = 0, iEnd = verts.size(); i < iEnd; ++i ) {
        dest.V( (int)i )->p = frantic::max3d::to_max_t( verts.get( i ) );
    }
    for( std::size_t i = 0, iEnd = verts.face_count(); i < iEnd; ++i ) {
        frantic::geometry::polymesh3_const_face_range f = verts.get_face( i );
        dest.F( (int)i )->MakePoly( (int)( f.second - f.first ), const_cast<int*>( f.first ) );
        dest.F( (int)i )->smGroup = 1;
        dest.F( (int)i )->material = 0;
    }

    if( channels.vertexNormal ) {
        vertex_channel_source& ch = *channels.vertexNormal;
        vertex_channel_source& faceCh = ch.face_count() > 0 ? ch : verts;

        dest.SpecifyNormals();
        MNNormalSpec* normalSpec = dest.GetSpecifiedNormals();
        if( !normalSpec ) {
            throw std::runtime_error( "polymesh_copy() Unable to specify normals" );
        }
        normalSpec->SetParent( &dest );

        if( !normalSpec->SetNumNormals( static_cast<int>( ch.size() ) ) ) {
            throw std::runtime_error( "polymesh_copy() Unable to allocate normals" );
        }
        if( !normalSpec->SetNumFaces( static_cast<int>( faceCh.face_count() ) ) ) {
            throw std::runtime_error( "polymesh_copy() Unable to allocate normal faces" );
        }

        Point3* normalArray = normalSpec->GetNormalArray();
        for( std::size_t i = 0, iEnd = ch.size(); i != iEnd; ++i ) {
            normalArray[i] = frantic::max3d::to_max_t( ch.get( i ) );
        }
        normalSpec->SetAllExplicit();

        for( std::size_t faceIndex = 0, faceIndexEnd = faceCh.face_count(); faceIndex != faceIndexEnd; ++faceIndex ) {
            frantic::geometry::polymesh3_const_face_range f = faceCh.get_face( faceIndex );
            int degree = (int)( f.second - f.first );

            MNNormalFace& normalFace = normalSpec->Face( static_cast<int>( faceIndex ) );
            normalFace.SetDegree( degree );
            for( int corner = 0; corner != degree; ++corner ) {
                normalFace.SetNormalID( corner, f.first[corner] );
            }
            normalFace.SpecifyAll();
        }
        normalSpec->CheckNormals();
    } else {
        dest.ClearSpecifiedNormals();
    }

    if( channels.smoothingGroup ) {
        face_channel_source<int>& ch = *channels.smoothingGroup;
        for( std::size_t i = 0, iEnd = verts.face_count(); i != iEnd; ++i ) {
            dest.F( (int)i )->smGroup = ch.get( i );
        }
    }

    if( channels.materialID ) {
        face_channel_source<MtlID>& ch = *channels.materialID;
        for( std::size_t i = 0, iEnd = verts.face_count(); i != iEnd; ++i ) {
            dest.F( (int)i )->material = ch.get( i );
        }
    }

    if( channels.vertexSelection ) {
        dest.SupportVSelectionWeights();
        float* vSelectionWeights = dest.getVSelectionWeights();
        for( std::size_t i = 0, iEnd = channels.vertexSelection->size(); i != iEnd; ++i ) {
            vSelectionWeights[i] = channels.vertexSelection->get( i );
        }
        dest.selLevel = MNM_SL_VERTEX;
    }

    if( channels.faceSelection ) {
        BitArray faceSel( static_cast<int>( channels.faceSelection->size() ) );
        faceSel.ClearAll(); // probably not necessary
        for( std::size_t i = 0, iEnd = channels.faceSelection->size(); i != iEnd; ++i ) {
            if( channels.faceSelection->get( i ) ) {
                faceSel.Set( static_cast<int>( i ) );
            }
        }
        dest.FaceSelect( faceSel );
        dest.selLevel = MNM_SL_FACE;
    }

    dest.SetMapNum( (int)get_num_maps( channels.maps ) );

    for( std::size_t mapIndex = 0; mapIndex < channels.maps.size(); ++mapIndex ) {
        if( channels.maps[mapIndex] ) {
            const int mapNum = static_cast<int>( mapIndex );
            vertex_channel_source& ch = *channels.maps[mapIndex];
            const std::size_t numFaces = ch.face_count() > 0 ? ch.face_count() : verts.face_count();
            const std::size_t numVerts = ch.size();

            dest.InitMap( mapNum );

            MNMap* pMap = dest.M( mapNum );
            pMap->setNumVerts( (int)numVerts );
            pMap->setNumFaces( (int)numFaces );

            for( std::size_t i = 0; i < numVerts; ++i ) {
                pMap->v[(int)i] = frantic::max3d::to_max_t( ch.get( i ) );
            }
            if( ch.face_count() ) {
                for( std::size_t i = 0; i < numFaces; ++i ) {
                    frantic::geometry::polymesh3_const_face_range r = ch.get_face( i );
                    pMap->F( (int)i )->MakePoly( (int)( r.second - r.first ), const_cast<int*>( r.first ) );
                }
            } else {
                for( std::size_t i = 0; i < numFaces; ++i ) {
                    frantic::geometry::polymesh3_const_face_range r = verts.get_face( i );
                    pMap->F( (int)i )->MakePoly( (int)( r.second - r.first ), const_cast<int*>( r.first ) );
                }
            }
        }
    }

    dest.InvalidateGeomCache();
    dest.InvalidateTopoCache();
    dest.FillInMesh();

    if( channels.edgeCrease ) {
        dest.setEDataSupport( EDATA_CREASE, TRUE );
        if( !dest.eDataSupport( EDATA_CREASE ) ) {
            throw std::runtime_error( "polymesh_copy() Could not enable support for edge crease data channel" );
        }

        float* edgeData = dest.edgeFloat( EDATA_CREASE );
        if( edgeData == NULL ) {
            throw std::runtime_error(
                "polymesh_copy() Could not obtain access to edge crease data in destination mesh" );
        }

        typed_vertex_channel_source<float>& ch = *channels.edgeCrease;
        const std::size_t numFaces = ch.face_count() > 0 ? ch.face_count() : verts.face_count();

        for( std::size_t i = 0; i < numFaces; ++i ) {
            frantic::geometry::polymesh3_const_face_range f = ch.face_count() ? ch.get_face( i ) : verts.get_face( i );
            MNFace* face = dest.F( (int)i );

            int edgeCounter = 0;
            for( frantic::geometry::polymesh3_const_face_iterator it = f.first, ie = f.second; it != ie; ++it ) {
                int edgeIndex = face->edg[edgeCounter];
                float magnitude = ch.get( *it );
                if( magnitude > 0 ) {
                    // Scale by 0.1, from XMesh and OpenSubdiv's [0..10] range
                    // to 3ds Max's [0..1].
                    // TODO: should maybe scale before it reaches this
                    // function?
                    edgeData[edgeIndex] = 0.1f * magnitude;
                } else {
                    edgeData[edgeIndex] = 0;
                }
                ++edgeCounter;
            }
        }
    }

    if( channels.vertexCrease && frantic::max3d::geometry::is_vdata_crease_supported() ) {
        const int vdataCrease = frantic::max3d::geometry::get_vdata_crease_channel();
        dest.setVDataSupport( vdataCrease, TRUE );
        if( !dest.vDataSupport( vdataCrease ) ) {
            throw std::runtime_error( "polymesh_copy() Could not enable support for vertex crease data channel" );
        }

        float* vertexData = dest.vertexFloat( vdataCrease );
        if( vertexData == NULL ) {
            throw std::runtime_error(
                "polymesh_copy() Could not obtain access to vertex crease data in destination mesh" );
        }

        typed_vertex_channel_source<float>& ch = *channels.vertexCrease;
        std::size_t numVerts = ch.size();

        if( numVerts != dest.numv ) {
            throw std::runtime_error(
                "polymesh_copy() Number of vertices in vertex crease channel and destination mesh must be equal" );
        }

        for( int i = 0; i < numVerts; ++i ) {
            // Scale by 0.1, from XMesh and OpenSubdiv's [0..10] range to
            // 3ds Max's [0..1].
            // TODO: should maybe scale before it reaches this function?
            vertexData[i] = 0.1f * ch.get( i );
        }
    }

    dest.PrepForPipeline();
}
