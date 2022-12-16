// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <Shlobj.h>

#include <boost/assign.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/math/common_factor.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/thread.hpp>

#include <zlib.h>

#include <frantic/max3d/geometry/auto_mesh.hpp>
#include <frantic/max3d/geometry/mesh.hpp>
#include <frantic/max3d/geometry/mesh_request.hpp>
#include <frantic/max3d/geometry/null_view.hpp>
#include <frantic/max3d/geometry/polymesh.hpp>
#include <frantic/max3d/max_utility.hpp>
#include <frantic/max3d/rendering/renderplugin_utils.hpp>

#include <frantic/diagnostics/profiling_manager.hpp>
#include <frantic/geometry/polymesh3_file_io.hpp>
#include <frantic/geometry/xmesh_writer.hpp>

#include "XMeshSaverVersion.h"

#include "XMeshSaverStaticInterface.hpp"

#include "attributions.hpp"

#include "boost_range_tab.hpp"

#include "get_particle_group_node_trimesh3.hpp"
#include "material_id_mapping.hpp"

#define XMeshSaver_INTERFACE Interface_ID( 0x399d6468, 0x71ef3a55 )

using namespace frantic;
using namespace frantic::geometry;
using namespace frantic::files;
using namespace frantic::max3d;
using namespace frantic::max3d::geometry;
using namespace frantic::max3d::rendering;

namespace fs = boost::filesystem;

extern HINSTANCE ghInstance;

void init_notify_proc( void* param, NotifyInfo* /*info*/ ) {
    try {
        frantic::win32::log_window* pLogWindow = reinterpret_cast<frantic::win32::log_window*>( param );
        if( pLogWindow ) {
            pLogWindow->init( ghInstance, GetCOREInterface()->GetMAXHWnd() );
        }
    } catch( std::exception& e ) {
        if( GetCOREInterface() ) {
            LogSys* log = GetCOREInterface()->Log();
            if( log ) {
                log->LogEntry( SYSLOG_ERROR, DISPLAY_DIALOG, _T( "XMesh Saver" ),
                               _T( "Unable to initialize XMesh Saver logging.\n\n%s" ), e.what() );
            }
        }
    }
}

inline frantic::geometry::xmesh_metadata::length_unit_t get_length_unit_from_3dsmax( int maxLengthUnit ) {
    switch( maxLengthUnit ) {
    case UNITS_INCHES:
        return frantic::geometry::xmesh_metadata::length_unit_inches;
    case UNITS_FEET:
        return frantic::geometry::xmesh_metadata::length_unit_feet;
    case UNITS_MILES:
        return frantic::geometry::xmesh_metadata::length_unit_miles;
    case UNITS_MILLIMETERS:
        return frantic::geometry::xmesh_metadata::length_unit_millimeters;
    case UNITS_CENTIMETERS:
        return frantic::geometry::xmesh_metadata::length_unit_centimeters;
    case UNITS_METERS:
        return frantic::geometry::xmesh_metadata::length_unit_meters;
    case UNITS_KILOMETERS:
        return frantic::geometry::xmesh_metadata::length_unit_kilometers;
    default:
        throw std::runtime_error( "get_length_unit_from_3dsmax Error: unknown 3ds Max unit: " +
                                  boost::lexical_cast<std::string>( maxLengthUnit ) );
    }
}

inline void set_metadata_from_scene( xmesh_metadata& metadata ) {
    // set framesPerSecond
    {
        boost::int64_t numerator = TIME_TICKSPERSEC;
        boost::int64_t denominator = GetTicksPerFrame();

        boost::int64_t fractionGCD = boost::math::gcd( numerator, denominator );

        numerator /= fractionGCD;
        denominator /= fractionGCD;

        metadata.set_frames_per_second( numerator, denominator );
    }

    // set length unit
    {
        int unitType;
        float unitScale;
#if MAX_VERSION_MAJOR >= 24
        GetSystemUnitInfo( &unitType, &unitScale );
#else
        GetMasterUnitInfo( &unitType, &unitScale );
#endif
        metadata.set_length_unit( unitScale, get_length_unit_from_3dsmax( unitType ) );
    }
}

inline void set_metadata_from_mesh( xmesh_metadata& metadata, const frantic::geometry::trimesh3& mesh ) {
    // set boundbox
    metadata.set_boundbox( mesh.compute_bound_box() );
}

inline void set_metadata_from_mesh( xmesh_metadata& metadata, const frantic::geometry::polymesh3::ptr_type mesh ) {
    frantic::graphics::boundbox3f bbox;

    if( mesh->has_vertex_channel( _T("verts") ) ) {
        frantic::geometry::polymesh3_const_vertex_accessor<frantic::graphics::vector3f> verts =
            mesh->get_const_vertex_accessor<frantic::graphics::vector3f>( _T("verts") );
        for( std::size_t i = 0, ie = verts.vertex_count(); i != ie; ++i ) {
            bbox += verts.get_vertex( i );
        }
    }

    metadata.set_boundbox( bbox );
}

inline void set_metadata_from_mesh( xmesh_metadata& metadata, const std::vector<frantic::graphics::vector3f>& verts ) {
    frantic::graphics::boundbox3f bbox;

    for( std::size_t i = 0; i < verts.size(); ++i ) {
        bbox += verts[i];
    }

    metadata.set_boundbox( bbox );
}

inline void clear_metadata_from_mesh( xmesh_metadata& metadata ) { metadata.clear_boundbox(); }

struct to_log {
    frantic::win32::log_window& m_wnd;
    std::string m_prefix;

    to_log& operator=( const to_log& );

  public:
    to_log( frantic::win32::log_window& wnd, const char* prefix )
        : m_wnd( wnd )
        , m_prefix( prefix ) {}
    void operator()( const char* msg ) { m_wnd.log( m_prefix + msg ); }
};

struct to_log_with_optional_popup {
    frantic::win32::log_window& m_wnd;
    frantic::tstring m_prefix;
    bool& m_logPopup;

    to_log_with_optional_popup& operator=( const to_log_with_optional_popup& );

  public:
    to_log_with_optional_popup( frantic::win32::log_window& wnd, const TCHAR* prefix, bool& logPopup )
        : m_wnd( wnd )
        , m_prefix( prefix )
        , m_logPopup( logPopup ) {}
    void operator()( const TCHAR* msg ) {
        if( m_logPopup && !m_wnd.is_visible() ) {
            m_wnd.show();
        }
        m_wnd.log( m_prefix + msg );
    }
};

void to_debug_log( const TCHAR* szMsg ) {
    if( frantic::logging::is_logging_debug() ) {
        GetMeshSaverStaticInterface().LogMessageInternal( frantic::tstring( _T("DBG: ") ) + szMsg );
    }
}

void to_stats_log( const TCHAR* szMsg ) {
    if( frantic::logging::is_logging_stats() ) {
        GetMeshSaverStaticInterface().LogMessageInternal( frantic::tstring( _T("STS: ") ) + szMsg );
    }
}

void to_progress_log( const TCHAR* szMsg ) {
    if( frantic::logging::is_logging_progress() ) {
        GetMeshSaverStaticInterface().LogMessageInternal( frantic::tstring( _T("PRG: ") ) + szMsg );
    }
}

void remove_default_value_channels( frantic::geometry::trimesh3& mesh ) {
    const frantic::tstring faceEdgeVisibility( _T("FaceEdgeVisibility") );
    if( mesh.has_face_channel( faceEdgeVisibility ) ) {
        bool hasDefaultValue = true;
        frantic::geometry::trimesh3_face_channel_accessor<boost::int8_t> visAcc(
            mesh.get_face_channel_accessor<boost::int8_t>( faceEdgeVisibility ) );
        for( std::size_t i = 0, ie = visAcc.size(); i < ie; ++i ) {
            if( visAcc[i] != EDGE_ALL ) {
                hasDefaultValue = false;
                break;
            }
        }
        if( hasDefaultValue ) {
            mesh.erase_face_channel( faceEdgeVisibility );
        }
    }
}

void transform( std::vector<frantic::graphics::vector3f>& points, const frantic::graphics::transform4f& xform ) {
    if( !xform.is_identity() ) {
        BOOST_FOREACH( frantic::graphics::vector3f& point, points ) {
            point = xform * point;
        }
    }
}

namespace {

void zero_channels( frantic::geometry::trimesh3& mesh ) {
    { // scope for vertex channels
        std::vector<frantic::tstring> vertexChannelNames;
        mesh.get_vertex_channel_names( vertexChannelNames );
        BOOST_FOREACH( const frantic::tstring& channelName, vertexChannelNames ) {
            frantic::geometry::trimesh3_vertex_channel_general_accessor acc =
                mesh.get_vertex_channel_general_accessor( channelName );
            memset( acc.data( 0 ), 0, acc.size() * acc.primitive_size() );
        }
    }

    { // scope for face channels
        std::vector<frantic::tstring> faceChannelNames;
        mesh.get_face_channel_names( faceChannelNames );
        BOOST_FOREACH( const frantic::tstring& channelName, faceChannelNames ) {
            frantic::geometry::trimesh3_face_channel_general_accessor acc =
                mesh.get_face_channel_general_accessor( channelName );
            memset( acc.data( 0 ), 0, acc.size() * acc.primitive_size() );
        }
    }
}

void zero_channels( frantic::geometry::polymesh3_ptr mesh ) {
    for( frantic::geometry::polymesh3::iterator ch( mesh->begin() ), chEnd( mesh->end() ); ch != chEnd; ++ch ) {
        if( ch->first == _T("verts") ) {
            continue;
        }
        const std::size_t elementSize = ch->second.element_size();
        if( ch->second.is_vertex_channel() ) {
            frantic::geometry::polymesh3_vertex_accessor<void> acc( mesh->get_vertex_accessor( ch->first ) );
            for( std::size_t i( 0 ), ie( acc.vertex_count() ); i != ie; ++i ) {
                memset( acc.get_vertex( i ), 0, elementSize );
            }
        } else {
            frantic::geometry::polymesh3_face_accessor<void> acc( mesh->get_face_accessor( ch->first ) );
            for( std::size_t i( 0 ), ie( acc.face_count() ); i != ie; ++i ) {
                memset( acc.get_face( i ), 0, elementSize );
            }
        }
    }
}

class xmesh_compatibility_checker {
    std::map<frantic::tstring, std::vector<int>> m_objectInvalidMayaFaces;

  public:
    void check( frantic::geometry::const_polymesh3_ptr mesh, const frantic::tstring& name ) {
        frantic::geometry::polymesh3_const_vertex_accessor<frantic::graphics::vector3f> geomAcc =
            mesh->get_const_vertex_accessor<frantic::graphics::vector3f>( _T("verts") );
        for( std::size_t i = 0, ie = geomAcc.face_count(); i < ie; ++i ) {
            frantic::geometry::polymesh3_const_face_range face = geomAcc.get_face( i );
            const std::size_t degree = face.second - face.first;
            if( degree > 3 && !frantic::geometry::is_valid_maya_face( face ) ) {
                m_objectInvalidMayaFaces[name].push_back( static_cast<int>( i ) );
            }
        }
    }
    const std::map<frantic::tstring, std::vector<int>>& get_invalid_object_faces() const {
        return m_objectInvalidMayaFaces;
    }
    std::map<frantic::tstring, std::vector<int>>& get_invalid_object_faces() { return m_objectInvalidMayaFaces; }
    bool has_warning() const { return !m_objectInvalidMayaFaces.empty(); }
};

template <class CharType>
inline std::basic_ostream<CharType>& operator<<( std::basic_ostream<CharType>& o,
                                                 const xmesh_compatibility_checker& xcc ) {
    const std::map<frantic::tstring, std::vector<int>>& invalidObjectFaces = xcc.get_invalid_object_faces();

    const std::size_t invalidObjectCount = invalidObjectFaces.size();
    const std::size_t printObjectCount = invalidObjectCount <= 3 ? invalidObjectCount : 2;
    const std::size_t remainingObjectCount = invalidObjectCount - printObjectCount;

    std::size_t i = 0;
    for( std::map<frantic::tstring, std::vector<int>>::const_iterator iter = invalidObjectFaces.begin();
         i < printObjectCount && iter != invalidObjectFaces.end(); ++iter, ++i ) {
        if( i > 0 ) {
            o << std::endl;
        }
        const std::size_t faceIndexCount = iter->second.size();
        const std::size_t printFaceIndexCount = faceIndexCount <= 4 ? faceIndexCount : 3;
        const std::size_t remainingFaceIndexCount = faceIndexCount - printFaceIndexCount;

        o << "Maya compatibility warning: Duplicate edges found in node \"" << iter->first << "\", ";
        o << ( faceIndexCount > 1 ? "faces " : "face " );

        for( std::size_t faceIndex = 0; faceIndex < printFaceIndexCount; ++faceIndex ) {
            o << boost::lexical_cast<std::basic_string<CharType>>(
                iter->second[faceIndex] + 1 ); // + 1 because I assume that's what Max users expect
            if( faceIndex + 1 < faceIndexCount ) {
                o << ", ";
            }
        }
        if( remainingFaceIndexCount > 0 ) {
            o << "plus " << boost::lexical_cast<std::basic_string<CharType>>( remainingFaceIndexCount )
              << " other faces";
        }
    }
    if( remainingObjectCount > 0 ) {
        o << std::endl
          << "Maya compatibility warning: Duplicate edges found in "
          << boost::lexical_cast<std::basic_string<CharType>>( remainingObjectCount ) << " more nodes";
    }
    return o;
}

frantic::tstring get_node_name( INode* node ) {
    if( node ) {
        const MCHAR* name = node->GetName();
        if( name ) {
            return name;
        } else {
            return _T("<null>");
        }
    } else {
        return _T("<null>");
    }
}

void convert_channels_from_max3d_to_xmesh( polymesh3_ptr mesh ) {
    if( !mesh ) {
        throw std::runtime_error( "convert_channels_from_max3d_to_xmesh Error: mesh is NULL" );
    }

    // 3ds Max stores Sharpness values in the range [0..1], while XMesh stores
    // values in the range [0..10] (as in OpenSubdiv), so we need to scale the
    // channel values by 10 before writing them to disk.
    std::vector<frantic::tstring> sharpnessChannels =
        boost::assign::list_of( _T("EdgeSharpness") )( _T("VertexSharpness") );

    BOOST_FOREACH( const frantic::tstring& channel, sharpnessChannels ) {
        if( mesh->has_channel( channel ) ) {
            scale_channel( mesh, channel, 10 );
        }
    }
}

} // anonymous namespace

bool MeshSaverStaticInterface::has_sequence() const {
    // TODO : check some other way?
    // filename_pattern.get_pattern() == "" doesn't work because it defaults to ####
    const filename_pattern& fp = m_sequenceName.get_filename_pattern();
    const bool noSequence = fp.get_directory( false ).empty() && fp.get_prefix().empty() && fp.get_extension().empty();
    return !noSequence;
}

void MeshSaverStaticInterface::report_error( const frantic::tstring& msg ) {
    mprintf( _T("%s\n"), msg.c_str() );
    FF_LOG( error ) << msg << std::endl;
}

void MeshSaverStaticInterface::apply_material_id_mapping( INode* meshNode, frantic::geometry::trimesh3& mesh ) {
    if( m_materialIDMapping.size() ) {
        ::apply_material_id_mapping( meshNode, mesh, m_materialIDMapping,
                                     static_cast<MtlID>( m_defaultMaterialID - 1 ) );
    }
}

void MeshSaverStaticInterface::apply_material_id_mapping( INode* meshNode, polymesh3_ptr mesh ) {
    if( !mesh ) {
        throw std::runtime_error( "apply_material_id_mapping() Internal error: mesh is NULL" );
    }
    if( m_materialIDMapping.size() ) {
        ::apply_material_id_mapping( meshNode, mesh, m_materialIDMapping,
                                     static_cast<MtlID>( m_defaultMaterialID - 1 ) );
    }
}

void MeshSaverStaticInterface::build_channel_propagation_policy( frantic::channels::channel_propagation_policy& cpp,
                                                                 bool addVelocity ) {
    cpp = frantic::channels::channel_propagation_policy( true );
    BOOST_FOREACH( const frantic::tstring& channelName, m_sourceChannelNames ) {
        cpp.add_channel( channelName );
    }
    if( addVelocity ) {
        cpp.add_channel( _T("Velocity") );
    }
}

void MeshSaverStaticInterface::get_specialized_node_trimesh3(
    INode* meshNode, TimeValue startTime, TimeValue endTime, frantic::geometry::trimesh3& outMesh,
    max_interval_t& outValidityInterval, float timeStepScale, bool ignoreEmptyMeshes, bool ignoreTopologyWarnings,
    bool useObjectSpace, const frantic::channels::channel_propagation_policy& cpp ) {
    if( frantic::max3d::geometry::is_particle_group( meshNode ) ) {
        throw std::runtime_error( "get_specialized_node_trimesh3 Internal Error: unexpected particle group node" );
    } else {
        frantic::max3d::geometry::get_node_trimesh3( meshNode, startTime, endTime, outMesh, outValidityInterval,
                                                     timeStepScale, ignoreEmptyMeshes, ignoreTopologyWarnings,
                                                     useObjectSpace, cpp );
    }
    if( cpp.is_channel_included( _T("MaterialID") ) ) {
        apply_material_id_mapping( meshNode, outMesh );
    }
}

void MeshSaverStaticInterface::save_meshes_to_sequence( std::vector<INode*> nodes, bool ignoreEmptyMeshes,
                                                        bool ignoreTopologyWarnings, bool useObjectSpace,
                                                        bool findVelocity, TimeValue t ) {
    if( !has_sequence() )
        throw std::runtime_error( "No sequence name set.  Please call XMeshSaverUtils.SetSequenceName to set the "
                                  "destination file sequence for the mesh." );

    frantic::diagnostics::profiling_manager prof;
    prof.new_profiling_section( _T("Get Mesh") );
    prof.new_profiling_section( _T("Save Mesh") );

    set_metadata_from_scene( m_metadata );

    const double frame = double( t ) / double( GetTicksPerFrame() );
    const TimeValue initialEndTime =
        findVelocity ? t + static_cast<TimeValue>( m_timeStepInitialOffset * GetTicksPerFrame() + 0.5f ) : t;

    trimesh3 outMesh;

    // only work through the vector if it has elements (otherwise just save an empty mesh)
    if( nodes.size() > 0 ) {
        trimesh3 tempMesh;

        max_interval_t maxValidity;

        frantic::channels::channel_propagation_policy cpp;
        build_channel_propagation_policy( cpp, findVelocity );

        std::map<INode*, std::vector<INode*>> particleSystems;
        cull_particle_system_nodes( nodes, particleSystems );

        // need to save all nodes of a particle system at the same time,
        // so that we can sort the particles from all events
        BOOST_FOREACH( std::vector<INode*>& particleSystemNodes, particleSystems | boost::adaptors::map_values ) {
            tempMesh.clear();

            prof.enter_section( 0 );
            frantic::max3d::geometry::get_particle_system_trimesh3(
                particleSystemNodes, t, initialEndTime, tempMesh, maxValidity, ignoreEmptyMeshes, cpp,
                m_materialIDMapping, static_cast<MtlID>( m_defaultMaterialID - 1 ) );
            prof.exit_section( 0 );

            if( outMesh.vertex_count() == 0 ) {
                outMesh.swap( tempMesh );
            } else {
                outMesh.combine( tempMesh );
            }
        }

        for( std::size_t i = 0; i < nodes.size(); ++i ) {
            tempMesh.clear();
            if( nodes[i] == NULL )
                throw std::runtime_error( "One of your mesh objects appears to have a NULL pointer.  Ie, it doesn't "
                                          "exist or Max can't seem to find it.  Perhaps you deleted it?" );

            prof.enter_section( 0 );
            get_specialized_node_trimesh3( nodes[i], t, initialEndTime, tempMesh, maxValidity, m_timeStepScale,
                                           ignoreEmptyMeshes, ignoreTopologyWarnings, useObjectSpace, cpp );
            prof.exit_section( 0 );

            if( outMesh.vertex_count() == 0 ) {
                outMesh.swap( tempMesh );
            } else {
                outMesh.combine( tempMesh ); // combine with the output mesh
            }
        }
    }

    remove_default_value_channels( outMesh );

    set_metadata_from_mesh( m_metadata, outMesh );

    // save the output mesh
    prof.enter_section( 1 );
    const frantic::tstring outFile = m_sequenceName[frame];
    const frantic::tstring ext = frantic::strings::to_lower( frantic::files::extension_from_path( outFile ) );
    if( ext == _T(".obj") ) {
        outMesh.transform( get_to_obj_transform() );
        write_obj_mesh_file( outFile, outMesh );
    } else if( ext == _T(".xmesh") ) {
        m_xss.set_compression_level( get_zlib_compression_level() );
        m_xss.set_thread_count( m_threadCount );
        m_xss.write_xmesh( outMesh, m_metadata, outFile );
    } else {
        throw std::runtime_error( "Unrecognized extension: " + frantic::strings::to_string( ext ) + "\n" );
    }
    prof.exit_section( 1 );

    FF_LOG( debug ) << prof << std::endl;
}

int MeshSaverStaticInterface::get_zlib_compression_level() {
    switch( m_compressionLevel ) {
    case 0:
        return Z_NO_COMPRESSION;
    default:
        return Z_BEST_SPEED;
    }
}

frantic::graphics::transform4f MeshSaverStaticInterface::get_to_obj_transform() {
    using namespace frantic::graphics;

    if( m_objFlipYZ ) {
        return transform4f( vector3f( 1, 0, 0 ), vector3f( 0, 0, -1 ), vector3f( 0, 1, 0 ) );
    } else {
        return transform4f::identity();
    }
}

void MeshSaverStaticInterface::SetTimeStepScale( float value ) {
    if( value > 0.f && value < 1.f )
        m_timeStepScale = value;
    else
        throw std::domain_error(
            "XMeshSaverUtils.SetTimeStepScale() The specified value is not in the input range (0,1)" );
}

float MeshSaverStaticInterface::GetTimeStepScale() { return m_timeStepScale; }

void MeshSaverStaticInterface::SetTimeStepInitialOffset( float value ) {
    if( value > 0.f && value <= 1.f )
        m_timeStepInitialOffset = value;
    else
        throw std::domain_error(
            "XMeshSaverUtils.SetTimeStepInitialOffset() The specified value is not in the input range (0,1]" );
}

float MeshSaverStaticInterface::GetTimeStepInitialOffset() { return m_timeStepInitialOffset; }

/**
 * For grabbing the plugin script locations relative to the dll's
 *
 * @return the path to the plugin scripts
 */
const MCHAR* MeshSaverStaticInterface::GetMeshSaverHome() {
    static frantic::tstring s_result;

    // MeshSaver home is one level up from where this .dlr is (or two levels up if there is a win32/x64 subpath being
    // used)
    fs::path homePath = fs::path( win32::GetModuleFileName( ghInstance ) ).branch_path();

    // If this structure has been subdivided into win32 and x64 subdirectories, then go one directory back.
    frantic::tstring pathLeaf = frantic::strings::to_lower( frantic::files::to_tstring( homePath.leaf() ) );
    if( pathLeaf == _T("win32") || pathLeaf == _T("x64") )
        homePath = homePath.branch_path();

    // Now go back past the "3dsMax#Plugin" folder
    homePath = homePath.branch_path();

    s_result = files::ensure_trailing_pathseparator( frantic::files::to_tstring( homePath ) );
    return s_result.c_str();
}

const MCHAR* MeshSaverStaticInterface::GetSettingsDirectory() {
    static frantic::tstring s_result;

    std::vector<TCHAR> buffer( MAX_PATH + 2, 0 );
    HRESULT result = SHGetFolderPath( GetCOREInterface()->GetMAXHWnd(), CSIDL_LOCAL_APPDATA, NULL, 0, &buffer[0] );
    frantic::tstring appDataPath = _T("");
    if( result == S_OK ) {
        appDataPath = frantic::tstring( &buffer[0] );
    } else if( result == S_FALSE ) {
        // ANSI
        // ok, but folder does not exist
        appDataPath = frantic::tstring( &buffer[0] );
    } else if( result == E_FAIL ) {
        // UNICODE
        // ok, but folder does not exit
        appDataPath = frantic::tstring( &buffer[0] );
    } else {
        throw std::runtime_error( "XMeshSaverUtils.GetSettingsDirectory: Error getting directory for user settings." );
    }

    s_result = frantic::files::to_tstring( boost::filesystem::path( appDataPath ) / _T("Thinkbox") / _T("XMeshSaver") );
    return s_result.c_str();
}

/**
 * Returns a formatted version string
 *
 * @return a string of the form "Version: x.x.x"
 **/
const MCHAR* MeshSaverStaticInterface::get_version() {
    static frantic::tstring s_result;

    frantic::tstring version( _T("Version:") );
    version += _T( FRANTIC_VERSION );

    s_result = version;
    return s_result.c_str();
}

/**
 *  Return the version number as a string.
 *
 * @return a version number string of the form "major.minor.patch.svnRev"
 */
const MCHAR* MeshSaverStaticInterface::get_version_number_string() {
    static frantic::tstring s_result( _T( FRANTIC_VERSION ) );
    return s_result.c_str();
}

MSTR MeshSaverStaticInterface::GetAttributions() { return get_attributions().c_str(); }

/**
 * Sets the name of the mesh sequence
 *
 * @param filename the name of the cache
 **/
void MeshSaverStaticInterface::SetSequenceName( const MCHAR* filename ) {
    if( !filename )
        throw std::runtime_error( "XMeshSaverUtils.SetSequenceName(): filename is NULL" );
    if( m_sequenceName.get_filename_pattern().matches_pattern( filename ) )
        return;
    m_sequenceName.get_filename_pattern().set( filename );
    m_sequenceName.sync_frame_set();
    m_xss.clear();
    if( !m_sequenceName.directory_exists() )
        throw std::runtime_error( "XMeshSaverUtils.SetSequenceName(): You must provide a valid path!" );
}

/**
 * Saves the mesh at time t to the xmesh sequence being maintained by the plugin static interface
 *
 * @param inode						the node the save
 * @param ignoreEmptyMeshes			ignore any empty meshes and proceed without throwing an error
 * @param ignoreTopologyWarnings	ignore any topology warnings and save out meshes without velocities when topology
 * prevents this
 * @param useObjectSpace			save the node mesh in object space, rather than world space
 * @param t							the time to save at
 */
void MeshSaverStaticInterface::SaveMeshToSequence( INode* inode, bool ignoreEmptyMeshes, bool ignoreTopologyWarnings,
                                                   bool useObjectSpace, bool findVelocity,
                                                   frantic::max3d::fnpublish::TimeWrapper t ) {
    try {
        if( inode == NULL )
            throw std::runtime_error( "Pointer to given mesh node is null.  Ie, it doesn't exist or Max can't seem to "
                                      "find it.  Perhaps you deleted it?" );

        std::vector<INode*> nodes( 1, inode );

        save_meshes_to_sequence( nodes, ignoreEmptyMeshes, ignoreTopologyWarnings, useObjectSpace, findVelocity, t );
    } catch( const std::exception& e ) {
        frantic::tstring errmsg =
            _T("XMeshSaverUtils.SaveMeshToSequence() - ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        if( is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

/**
 * Save a collection of nodes as a single frame at time t
 *
 * @param nodes						the nodes to save
 * @param ignoreEmptyMeshes			ignore any empty meshes and proceed without throwing an error
 * @param ignoreTopologyWarnings	ignore any topology warnings and save out meshes without velocities when topology
 * prevents this
 * @param t							the time to save at
 */
void MeshSaverStaticInterface::SaveMeshesToSequence( const Tab<INode*>& nodesTab, bool ignoreEmptyMeshes,
                                                     bool ignoreTopologyWarnings, bool findVelocity,
                                                     frantic::max3d::fnpublish::TimeWrapper t ) {
    const bool useObjectSpace = false;

    try {
        std::vector<INode*> nodes;
        boost::range::push_back( nodes, nodesTab );

        save_meshes_to_sequence( nodes, ignoreEmptyMeshes, ignoreTopologyWarnings, useObjectSpace, findVelocity, t );
    } catch( const std::exception& e ) {
        frantic::tstring errmsg =
            _T("XMeshSaverUtils.SaveMeshesToSequence() - ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        if( is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

void MeshSaverStaticInterface::save_polymesh_sequence( std::vector<INode*> nodes, bool ignoreEmptyMeshes,
                                                       bool ignoreTopologyWarnings, bool useObjectSpace,
                                                       bool findVelocity, TimeValue t ) {
    const double frame = double( t ) / double( GetTicksPerFrame() );
    const TimeValue initialEndTime =
        findVelocity ? t + static_cast<TimeValue>( m_timeStepInitialOffset * GetTicksPerFrame() + 0.5f ) : t;

    if( !has_sequence() )
        throw std::runtime_error(
            "No sequence name set.  Please call SetSequenceName to set the destination file sequence for the mesh." );

    frantic::diagnostics::profiling_manager prof;
    prof.new_profiling_section( _T("Get Mesh") );
    prof.new_profiling_section( _T("Check Mesh") );
    prof.new_profiling_section( _T("Save Mesh") );

    set_metadata_from_scene( m_metadata );

    frantic::channels::channel_propagation_policy cpp;
    build_channel_propagation_policy( cpp, findVelocity );

    std::vector<polymesh3_ptr> meshes;
    meshes.reserve( nodes.size() );

    std::map<INode*, std::vector<INode*>> particleSystems;
    cull_particle_system_nodes( nodes, particleSystems );

    xmesh_compatibility_checker xcc;

    // need to save all nodes of a particle system at the same time,
    // so that we can sort the particles from all events
    for( std::map<INode*, std::vector<INode*>>::iterator i = particleSystems.begin(), ie = particleSystems.end();
         i != ie; ++i ) {
        std::vector<INode*>& particleSystemNodes = i->second;

        prof.enter_section( 0 );
        polymesh3_ptr temp =
            get_particle_system_polymesh3( particleSystemNodes, t, initialEndTime, ignoreEmptyMeshes, cpp,
                                           m_materialIDMapping, static_cast<MtlID>( m_defaultMaterialID - 1 ) );
        prof.exit_section( 0 );

        if( frantic::logging::is_logging_warnings() ) {
            prof.enter_section( 1 );
            xcc.check( temp, get_node_name( i->first ) );
            prof.exit_section( 1 );
        }

        meshes.push_back( temp );
    }

    BOOST_FOREACH( INode* node, nodes ) {
        if( node == NULL )
            throw std::runtime_error( "One of your mesh objects appears to have a NULL pointer.  Ie, it doesn't exist "
                                      "or Max can't seem to find it.  Perhaps you deleted it?" );

        max_interval_t maxValidity;

        prof.enter_section( 0 );
        polymesh3_ptr temp = get_node_polymesh3( node, t, initialEndTime, maxValidity, m_timeStepScale,
                                                 ignoreEmptyMeshes, ignoreTopologyWarnings, useObjectSpace, cpp );
        prof.exit_section( 0 );

        if( frantic::logging::is_logging_warnings() ) {
            prof.enter_section( 1 );
            xcc.check( temp, get_node_name( node ) );
            prof.exit_section( 1 );
        }

        if( cpp.is_channel_included( _T("MaterialID") ) ) {
            apply_material_id_mapping( node, temp );
        }

        meshes.push_back( temp );
    }

    if( xcc.has_warning() ) {
        FF_LOG( warning ) << xcc << std::endl;
    }

    polymesh3_ptr polymesh = combine( meshes );

    set_metadata_from_mesh( m_metadata, polymesh );

    convert_channels_from_max3d_to_xmesh( polymesh );

    // save the output mesh
    prof.enter_section( 2 );
    const frantic::tstring outFile = m_sequenceName[frame];
    const frantic::tstring ext = frantic::strings::to_lower( frantic::files::extension_from_path( outFile ) );
    if( ext == _T(".obj") ) {
        transform( polymesh, get_to_obj_transform() );
        write_obj_polymesh_file( outFile, polymesh );
    } else if( ext == _T(".xmesh") ) {
        m_xss.set_compression_level( get_zlib_compression_level() );
        m_xss.set_thread_count( m_threadCount );
        m_xss.write_xmesh( polymesh, m_metadata, outFile );
    } else {
        throw std::runtime_error( "Unrecognized extension: " + frantic::strings::to_string( ext ) + "\n" );
    }
    prof.exit_section( 2 );

    FF_LOG( debug ) << prof << std::endl;
}

/**
 * Saves the mesh at time t to the xmesh sequence being maintained by the plugin static interface
 *
 * @param inode						the node the save
 * @param ignoreEmptyMeshes			ignore any empty meshes and proceed without throwing an error
 * @param ignoreTopologyWarnings	ignore any topology warnings and save out meshes without velocities when topology
 * prevents this
 * @param useObjectSpace			save the node mesh in object space, rather than world space
 * @param t							the time to save at
 */
void MeshSaverStaticInterface::SavePolymeshToSequence( INode* inode, bool ignoreEmptyMeshes,
                                                       bool ignoreTopologyWarnings, bool useObjectSpace,
                                                       bool findVelocity, frantic::max3d::fnpublish::TimeWrapper t ) {
    try {
        std::vector<INode*> nodes;
        nodes.push_back( inode );

        save_polymesh_sequence( nodes, ignoreEmptyMeshes, ignoreTopologyWarnings, useObjectSpace, findVelocity, t );
    } catch( const std::exception& e ) {
        frantic::tstring errmsg =
            _T("XMeshSaverUtils.SavePolyMeshToSequence() - ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        if( is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

/**
 * Save a collection of nodes as a single frame at time t
 *
 * @param nodes						the nodes to save
 * @param ignoreEmptyMeshes			ignore any empty meshes and proceed without throwing an error
 * @param ignoreTopologyWarnings	ignore any topology warnings and save out meshes without velocities when topology
 * prevents this
 * @param t							the time to save at
 */
void MeshSaverStaticInterface::SavePolymeshesToSequence( const Tab<INode*>& nodesTab, bool ignoreEmptyMeshes,
                                                         bool ignoreTopologyWarnings, bool findVelocity,
                                                         frantic::max3d::fnpublish::TimeWrapper t ) {
    try {
        std::vector<INode*> nodes;
        boost::range::push_back( nodes, nodesTab );

        const bool useObjectSpace = false;

        save_polymesh_sequence( nodes, ignoreEmptyMeshes, ignoreTopologyWarnings, useObjectSpace, findVelocity, t );
    } catch( const std::exception& e ) {
        frantic::tstring errmsg =
            _T("XMeshSaverUtils.SavePolymeshesToSequence() - ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        if( is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

/**
 * This function will save a single polygon mesh to a file. If the object supplied does not
 * produce a PolyObject at the top of its stack, then the object will be converted to a PolyObject.
 * If that conversion causes the number of vertices to change, then the user will be notified.
 * @param pNode Pointer to the scene node to extract the polymesh from
 * @param path The file path to save the mesh to
 * @param t The automatically populated current scene time
 */
void MeshSaverStaticInterface::SavePolymesh( INode* pNode, const MCHAR* inPath, bool vertsOnly, bool worldSpace,
                                             frantic::max3d::fnpublish::TimeWrapper t ) {
    if( !pNode )
        throw std::runtime_error( "Pointer to given mesh node is null.  Ie, it doesn't exist or Max can't seem to find "
                                  "it.  Perhaps you deleted it?" );

    if( !inPath )
        throw std::runtime_error( "path is NULL" );

    const frantic::tstring path( inPath );
    if( path.empty() )
        throw std::runtime_error( "path cannot be an empty string" );

    frantic::diagnostics::profiling_manager prof;
    prof.new_profiling_section( _T("Get Mesh") );
    prof.new_profiling_section( _T("Check Mesh") );
    prof.new_profiling_section( _T("Save Mesh") );

    set_metadata_from_scene( m_metadata );

    transform4f xform;
    if( worldSpace ) {
        Interval xformValidity;
        Matrix3 maxXform = pNode->GetObjTMAfterWSM( t, &xformValidity );
        xform = transform4f( maxXform );
    }

    prof.enter_section( 0 );
    AutoPolyObject pPolyObj = get_polyobject_from_inode( pNode, t );

    if( !vertsOnly ) {
        frantic::channels::channel_propagation_policy cpp;
        build_channel_propagation_policy( cpp, false );

        polymesh3_ptr polymesh = frantic::max3d::geometry::from_max_t( pPolyObj->GetMesh(), cpp );
        frantic::geometry::transform( polymesh, xform );

        prof.exit_section( 0 );

        if( frantic::logging::is_logging_warnings() ) {
            xmesh_compatibility_checker xcc;
            prof.enter_section( 1 );
            xcc.check( polymesh, get_node_name( pNode ) );
            prof.exit_section( 1 );
            if( xcc.has_warning() ) {
                FF_LOG( warning ) << xcc << std::endl;
            }
        }

        if( cpp.is_channel_included( _T("MaterialID") ) )
            apply_material_id_mapping( pNode, polymesh );

        set_metadata_from_mesh( m_metadata, polymesh );

        convert_channels_from_max3d_to_xmesh( polymesh );

        prof.enter_section( 2 );
        const frantic::tstring ext = frantic::strings::to_lower( frantic::files::extension_from_path( path ) );
        if( ext == _T(".obj") ) {
            frantic::geometry::transform( polymesh, get_to_obj_transform() );
            write_obj_polymesh_file( path, polymesh );
        } else if( ext == _T(".xmesh") ) {
            write_xmesh_polymesh_file( path, polymesh, &m_metadata ); //, get_zlib_compression_level() );
        } else {
            throw std::runtime_error( "Unrecognized extension: " + frantic::strings::to_string( ext ) + "\n" );
        }
        prof.exit_section( 2 );
    } else {
        // We only want to save the verts, so we will just iterate over the object's points, saving them
        // into a vertex channel.
        int nPoints = pPolyObj->NumPoints();

        std::vector<frantic::graphics::vector3f> verts;
        verts.reserve( nPoints );

        for( int i = 0; i < nPoints; ++i )
            verts.push_back( frantic::max3d::from_max_t( pPolyObj->GetPoint( i ) ) );

        transform( verts, xform );

        prof.exit_section( 0 );

        set_metadata_from_mesh( m_metadata, verts );

        prof.enter_section( 2 );
        const frantic::tstring ext = frantic::strings::to_lower( frantic::files::extension_from_path( path ) );
        if( ext == _T(".obj") ) {
            frantic::geometry::polymesh3_builder builder;
            for( int i = 0; i < nPoints; ++i )
                builder.add_vertex( verts[i] );
            polymesh3_ptr polymesh = builder.finalize();
            frantic::geometry::transform( polymesh, get_to_obj_transform() );
            frantic::geometry::write_obj_polymesh_file( path, polymesh );
        } else if( ext == _T(".xmesh") ) {
            frantic::geometry::xmesh_writer meshWriter( path );
            meshWriter.set_metadata( m_metadata );
            meshWriter.set_compression_level( get_zlib_compression_level() );
            meshWriter.write_vertex_channel( _T("verts"), (char*)&verts[0].x, frantic::channels::data_type_float32, 3,
                                             (std::size_t)nPoints );
            meshWriter.close();
        } else {
            throw std::runtime_error( "Unrecognized extension: " + frantic::strings::to_string( ext ) + "\n" );
        }
        prof.exit_section( 2 );
    }

    FF_LOG( debug ) << prof << std::endl;
}

void MeshSaverStaticInterface::SetSceneRenderBegin( void ) {
    Interface* ip = GetCOREInterface();
    if( ip ) {
        const TimeValue t = ip->GetTime();
        std::set<ReferenceMaker*> doneNodes;
        refmaker_call_recursive( ip->GetRootNode(), doneNodes, render_begin_function( t, 0 ) );
    }
}

void MeshSaverStaticInterface::SetSceneRenderEnd( void ) {
    Interface* ip = GetCOREInterface();
    if( ip ) {
        const TimeValue t = ip->GetTime();
        std::set<ReferenceMaker*> doneNodes;
        refmaker_call_recursive( ip->GetRootNode(), doneNodes, render_end_function( t ) );
    }
}

void MeshSaverStaticInterface::ClearAllMaterialIDMapping() { m_materialIDMapping.clear(); }

void MeshSaverStaticInterface::SetMaterialIDMapping( INode* node, const Tab<int>& from1, const Tab<int>& to1 ) {
    // if( shapeIndex1 <= 0 ) {
    // throw std::runtime_error( "ShapeIndex must be >= 1" );
    //}
    if( !node ) {
        throw std::runtime_error( "MeshNode is NULL" );
    }
    if( from1.Count() != to1.Count() ) {
        throw std::runtime_error(
            "FromMaterialIDList and ToMaterialIDList must have the same size, but instead their sizes are " +
            boost::lexical_cast<std::string>( from1.Count() ) + " and " +
            boost::lexical_cast<std::string>( to1.Count() ) + "." );
    }
    BOOST_FOREACH( int i, from1 ) {
        if( i < 1 || i > ( std::numeric_limits<MtlID>::max() + 1 ) ) {
            throw std::runtime_error( "FromMaterialIDList entries must be in range [1.." +
                                      boost::lexical_cast<std::string>( std::numeric_limits<MtlID>::max() + 1 ) +
                                      "], but got " + boost::lexical_cast<std::string>( i ) );
        }
    }
    BOOST_FOREACH( int i, to1 ) {
        if( i > ( std::numeric_limits<MtlID>::max() + 1 ) ) {
            throw std::runtime_error( "ToMaterialIDList entries must be <= " +
                                      boost::lexical_cast<std::string>( std::numeric_limits<MtlID>::max() + 1 ) +
                                      ", but got " + boost::lexical_cast<std::string>( i ) );
        }
    }
    // const std::size_t shapeIndex0 = shapeIndex1 - 1;
    std::pair<std::map<INode*, std::map<int, int>>::iterator, bool> result =
        m_materialIDMapping.insert( std::pair<INode*, std::map<int, int>>( node, std::map<int, int>() ) );
    std::map<int, int>& m = result.first->second;
    m.clear();
    for( std::size_t i = 0, ie = from1.Count(); i != ie; ++i ) {
        m[from1[i] - 1] = to1[i] - 1;
    }
    if( m.size() == 0 ) {
        m_materialIDMapping.erase( result.first );
    }
}

int MeshSaverStaticInterface::GetDefaultMaterialID() { return m_defaultMaterialID; }

void MeshSaverStaticInterface::SetDefaultMaterialID( int matID ) {
    if( matID < 1 || matID > ( std::numeric_limits<MtlID>::max() + 1 ) ) {
        throw std::runtime_error( "MatID must be in range [1.." +
                                  boost::lexical_cast<std::string>( std::numeric_limits<MtlID>::max() + 1 ) +
                                  "], but got " + boost::lexical_cast<std::string>( matID ) + " instead." );
    }
    m_defaultMaterialID = matID;
}

void MeshSaverStaticInterface::SetCompressionLevel( int compressionLevel ) {
    // if( compressionLevel == Z_DEFAULT_COMPRESSION || ( compressionLevel >= Z_NO_COMPRESSION && compressionLevel <=
    // Z_BEST_COMPRESSION ) ) {
    if( compressionLevel == -1 || ( compressionLevel >= 0 && compressionLevel <= 1 ) ) {
        m_compressionLevel = compressionLevel;
    }
}

int MeshSaverStaticInterface::GetCompressionLevel() { return m_compressionLevel; }

void MeshSaverStaticInterface::SetThreadCount( int threadCount ) {
    threadCount = frantic::math::clamp<int>( threadCount, 1, 8 );
    m_threadCount = static_cast<std::size_t>( threadCount );
}

int MeshSaverStaticInterface::GetThreadCount() { return static_cast<int>( m_threadCount ); }

void MeshSaverStaticInterface::InitializeLogging() {
    frantic::logging::set_logging_level( frantic::logging::level::warning );

    frantic::logging::debug.rdbuf( frantic::logging::new_ffstreambuf( &to_debug_log ) );
    frantic::logging::stats.rdbuf( frantic::logging::new_ffstreambuf( &to_stats_log ) );
    frantic::logging::progress.rdbuf( frantic::logging::new_ffstreambuf( &to_progress_log ) );
    frantic::logging::warning.rdbuf(
        frantic::logging::new_ffstreambuf( to_log_with_optional_popup( m_logWindow, _T("WRN: "), m_logPopupError ) ) );
    frantic::logging::error.rdbuf(
        frantic::logging::new_ffstreambuf( to_log_with_optional_popup( m_logWindow, _T("ERR: "), m_logPopupError ) ) );

    int result = RegisterNotification( &init_notify_proc, &m_logWindow, NOTIFY_SYSTEM_STARTUP );
    // BEWARE!!! DO NOT THROW AN EXCEPTION BELOW IF init_notify_proc HAS REGISTERED A CALLBACK
    // If you throw an exception, the DLL will be unloaded.
    // The callback will then try to call code from the DLL that has been unloaded.
    if( !result ) {
        if( GetCOREInterface() && !is_network_render_server() ) {
            m_logWindow.init( ghInstance, NULL );
        }
    }
}

void MeshSaverStaticInterface::SetLoggingLevel( int loggingLevel ) {
    frantic::logging::set_logging_level( loggingLevel );
}

int MeshSaverStaticInterface::GetLoggingLevel() { return frantic::logging::get_logging_level(); }

bool MeshSaverStaticInterface::GetPopupLogWindowOnError() { return m_logPopupError; }

void MeshSaverStaticInterface::SetPopupLogWindowOnError( bool popupError ) { m_logPopupError = popupError; }

bool MeshSaverStaticInterface::GetLogWindowVisible() { return m_logWindow.is_visible(); }

void MeshSaverStaticInterface::SetLogWindowVisible( bool visible ) { m_logWindow.show( visible ); }

void MeshSaverStaticInterface::FocusLogWindow() {
    if( m_logWindow.is_visible() ) {
        SetFocus( m_logWindow.handle() );
    }
}

void MeshSaverStaticInterface::LogMessageInternal( const frantic::tstring& msg ) { m_logWindow.log( msg ); }

void MeshSaverStaticInterface::LogError( const MCHAR* msg ) {
    if( !msg ) {
        throw std::runtime_error( "XMeshSaverUtils.LogError(): msg is NULL" );
    }
    FF_LOG( error ) << msg << std::endl;
}

void MeshSaverStaticInterface::LogWarning( const MCHAR* msg ) {
    if( !msg ) {
        throw std::runtime_error( "XMeshSaverUtils.LogWarning(): msg is NULL" );
    }
    FF_LOG( warning ) << msg << std::endl;
}

void MeshSaverStaticInterface::LogProgress( const MCHAR* msg ) {
    if( !msg ) {
        throw std::runtime_error( "XMeshSaverUtils.LogProgress(): msg is NULL" );
    }
    FF_LOG( progress ) << msg << std::endl;
}

void MeshSaverStaticInterface::LogStats( const MCHAR* msg ) {
    if( !msg ) {
        throw std::runtime_error( "XMeshSaverUtils.LogStats(): msg is NULL" );
    }
    FF_LOG( stats ) << msg << std::endl;
}

void MeshSaverStaticInterface::LogDebug( const MCHAR* msg ) {
    if( !msg ) {
        throw std::runtime_error( "XMeshSaverUtils.LogDebug(): msg is NULL" );
    }
    FF_LOG( debug ) << msg << std::endl;
}

const MCHAR* MeshSaverStaticInterface::ReplaceSequenceNumber( const MCHAR* file, float frame ) {
    if( !file ) {
        throw std::runtime_error( "XMeshSaverUtils.ReplaceSequenceNumber(): file is NULL" );
    }
    static frantic::tstring buffer;
    frantic::files::filename_pattern fp( file );
    buffer = fp[frame];
    return buffer.c_str();
}

void MeshSaverStaticInterface::SetUserData( const MCHAR* key, const MCHAR* value ) {
    if( !key ) {
        throw std::runtime_error( "XMeshSaverUtils.SetUserData(): key is NULL" );
    }
    if( !value ) {
        throw std::runtime_error( "XMeshSaverUtils.SetUserData(): value is NULL" );
    }
    m_metadata.set_user_data( key, value );
}

FPValue MeshSaverStaticInterface::GetUserDataArray() {
    typedef std::pair<frantic::tstring, frantic::tstring> string_pair;
    typedef std::vector<string_pair> result_t;

    std::vector<frantic::tstring> userDataKeys;
    m_metadata.get_user_data_keys( userDataKeys );

    result_t result;
    result.reserve( userDataKeys.size() );

    for( std::vector<frantic::tstring>::const_iterator i = userDataKeys.begin(); i != userDataKeys.end(); ++i ) {
        result.push_back( string_pair( *i, m_metadata.get_user_data( *i ) ) );
    }

    FPValue out;
    frantic::max3d::fpwrapper::MaxTypeTraits<result_t>::set_fpvalue( result, out );
    return out;
}

void MeshSaverStaticInterface::DeleteUserData( const MCHAR* key ) {
    if( !key ) {
        throw std::runtime_error( "XMeshSaverUtils.DeleteUserData(): key is NULL" );
    }
    m_metadata.erase_user_data( key );
}

void MeshSaverStaticInterface::ClearUserData() { m_metadata.clear_user_data(); }

/*
void MeshSaverStaticInterface::SetChannelTransformType( const std::string & key, const std::string & value ) {
        transform_type_t transformType = get_transform_type_from_string( value );
        if( transformType == transform_type_invalid ) {
                throw std::runtime_error( "SetChannelTransformType Error: cannot set channel \'" + key + "\' to unknown
transformType \'" + value + "\'." );
        }
        m_metadata.set_channel_transform_type( key, transformType );
}

void MeshSaverStaticInterface::ClearChannelTransformType() {
        m_metadata.clear_channel_transform_type();
}

FPValue MeshSaverStaticInterface::GetChannelTransformTypeArray() {
        typedef std::pair<std::string,std::string> string_pair;
        typedef std::vector<string_pair> result_t;

        result_t result;
        result.reserve( m_metadata.channelTransformType.size() );

        for( xmesh_metadata::channel_transform_type_collection_t::iterator i = m_metadata.channelTransformType.begin();
i != m_metadata.channelTransformType.end(); ++i ) { result.push_back( string_pair( i->first, get_transform_type_string(
i->second ) ) );
        }

        FPValue out;
        frantic::max3d::fpwrapper::MaxTypeTraits<result_t>::set_fpvalue( result, out );
        return out;
}
*/

void MeshSaverStaticInterface::LoadMetadata( const MCHAR* filename ) {
    if( !filename ) {
        throw std::runtime_error( "XMeshSaverUtils.LoadMetadata(): filename is NULL" );
    }
    if( !frantic::files::file_exists( filename ) ) {
        throw std::runtime_error( "LoadMetadata Error: input file \'" + frantic::strings::to_string( filename ) +
                                  "\' does not exist." );
    }
    read_xmesh_metadata( frantic::strings::to_wstring( filename ), m_metadata );
}

void MeshSaverStaticInterface::SaveMetadata( const MCHAR* filename ) {
    if( !filename ) {
        throw std::runtime_error( "XMeshSaverUtils.SaveMetadata(): filename is NULL" );
    }
    if( frantic::files::file_exists( filename ) ) {
        throw std::runtime_error( "SaveMetadata Error: output file \'" + frantic::strings::to_string( filename ) +
                                  "\' already exists." );
    }

    clear_metadata_from_mesh( m_metadata );

    write_xmesh_metadata( frantic::strings::to_wstring( filename ), m_metadata );
}

Tab<const MCHAR*> MeshSaverStaticInterface::GetSourceChannelNames() {
    static std::vector<frantic::tstring> s_cachedResult;

    s_cachedResult.clear();
    boost::range::push_back( s_cachedResult, m_sourceChannelNames );

    Tab<const MCHAR*> result;
    result.SetCount( static_cast<int>( s_cachedResult.size() ) );
    for( std::size_t i = 0; i < s_cachedResult.size(); ++i ) {
        result[static_cast<INT_PTR>( i )] = s_cachedResult[i].c_str();
    }
    return result;
}

void MeshSaverStaticInterface::SetSourceChannelNames( Tab<const MCHAR*> channelNames ) {
    m_sourceChannelNames.clear();
    for( int i = 0; i < channelNames.Count(); ++i ) {
        const MCHAR* s = channelNames[i];
        m_sourceChannelNames.push_back( s ? s : _T("") );
    }
}

void MeshSaverStaticInterface::SetObjFlipYZ( const bool objFlipYZ ) { m_objFlipYZ = objFlipYZ; }

bool MeshSaverStaticInterface::GetObjFlipYZ() { return m_objFlipYZ; }

/**
 * Register the interface and publish functions
 */
MeshSaverStaticInterface::MeshSaverStaticInterface()
    : StaticInterface<MeshSaverStaticInterface>( XMeshSaver_INTERFACE, _T("XMeshSaverUtils"), 0 )
    , m_timeStepScale( 0.5f )
    , m_timeStepInitialOffset( 0.25f )
    , m_defaultMaterialID( 1 )
    , m_compressionLevel( 1 )
    , m_logWindow( _T("XMesh Saver Log Window"), 0, 0, true )
    , m_logPopupError( true ) {
    m_threadCount = frantic::math::clamp<std::size_t>( boost::thread::hardware_concurrency(), 1, 4 );

    m_sourceChannelNames = { _T( "Velocity" ), _T( "Color" ), _T( "TextureCoord" ) };
    for( int i = 2; i < 100; ++i ) {
        m_sourceChannelNames.push_back( _T("Mapping") + boost::lexical_cast<frantic::tstring>( i ) );
    }
    m_sourceChannelNames.push_back( _T("MaterialID") );
    m_sourceChannelNames.push_back( _T("SmoothingGroup") );
    m_sourceChannelNames.push_back( _T("FaceEdgeVisibility") );

    this->function( _T("SetSequenceName"), &MeshSaverStaticInterface::SetSequenceName ).param( _T("SequenceName") );
    this->function( _T("SaveMeshToSequence"), &MeshSaverStaticInterface::SaveMeshToSequence )
        .param( _T("Node") )
        .param( _T("IgnoreEmptyMeshes") )
        .param( _T("IgnoreTopologyChanges") )
        .param( _T("UseObjectSpace") )
        .param( _T("FindVelocity") );
    this->function( _T("SaveMeshesToSequence"), &MeshSaverStaticInterface::SaveMeshesToSequence )
        .param( _T("Nodes") )
        .param( _T("IgnoreEmptyMeshes") )
        .param( _T("IgnoreTopologyChanges") )
        .param( _T("FindVelocity") );
    // c.add_function( &MeshSaverStaticInterface::SaveMeshesInIntervalToSequence, _T("SaveMeshesInIntervalToSequence"),
    // _T("Node"), _T("Interval"), _T("NumSamples"), _T("NumRetries"), _T("Options") );

    this->function( _T("SavePolymesh"), &MeshSaverStaticInterface::SavePolymesh )
        .param( _T("Node") )
        .param( _T("Path") )
        .param( _T("VertsOnly") )
        .param( _T("WorldSpace"), false );

    this->function( _T("SavePolymeshToSequence"), &MeshSaverStaticInterface::SavePolymeshToSequence )
        .param( _T("Node") )
        .param( _T("IgnoreEmptyMeshes") )
        .param( _T("IgnoreTopologyChanges") )
        .param( _T("UseObjectSpace") )
        .param( _T("FindVelocity") );
    this->function( _T("SavePolymeshesToSequence"), &MeshSaverStaticInterface::SavePolymeshesToSequence )
        .param( _T("Nodes") )
        .param( _T("IgnoreEmptyMeshes") )
        .param( _T("IgnoreTopologyChanges") )
        .param( _T("FindVelocity") );

    this->function( _T("SetSceneRenderBegin"), &MeshSaverStaticInterface::SetSceneRenderBegin );
    this->function( _T("SetSceneRenderEnd"), &MeshSaverStaticInterface::SetSceneRenderEnd );

    this->read_only_property( _T("Version"), &MeshSaverStaticInterface::get_version );
    this->read_only_property( _T("VersionNumber"), &MeshSaverStaticInterface::get_version_number_string );

    this->read_only_property( _T("Attributions"), &MeshSaverStaticInterface::GetAttributions );

    this->read_only_property( _T("XMeshSaverHome"), &MeshSaverStaticInterface::GetMeshSaverHome );
    this->read_only_property( _T("SettingsDirectory"), &MeshSaverStaticInterface::GetSettingsDirectory );

    this->read_write_property( _T("TimeStepInitialOffset"), &MeshSaverStaticInterface::GetTimeStepInitialOffset,
                               &MeshSaverStaticInterface::SetTimeStepInitialOffset );
    this->read_write_property( _T("TimeStepScale"), &MeshSaverStaticInterface::GetTimeStepScale,
                               &MeshSaverStaticInterface::SetTimeStepScale );

    this->function( _T("ClearAllMaterialIDMapping"), &MeshSaverStaticInterface::ClearAllMaterialIDMapping );
    this->function( _T("SetMaterialIDMapping"), &MeshSaverStaticInterface::SetMaterialIDMapping )
        .param( _T("MeshNode") )
        .param( _T("FromMaterialIDList") )
        .param( _T("ToMaterialIDList") );
    this->read_write_property( _T("DefaultMaterialID"), &MeshSaverStaticInterface::GetDefaultMaterialID,
                               &MeshSaverStaticInterface::SetDefaultMaterialID );

    this->read_write_property( _T("CompressionLevel"), &MeshSaverStaticInterface::GetCompressionLevel,
                               &MeshSaverStaticInterface::SetCompressionLevel );

    this->read_write_property( _T("ThreadCount"), &MeshSaverStaticInterface::GetThreadCount,
                               &MeshSaverStaticInterface::SetThreadCount );

    this->function( _T("LogError"), &MeshSaverStaticInterface::LogError ).param( _T("Msg") );
    this->function( _T("LogWarning"), &MeshSaverStaticInterface::LogWarning ).param( _T("Msg") );
    this->function( _T("LogProgress"), &MeshSaverStaticInterface::LogProgress ).param( _T("Msg") );
    this->function( _T("LogStats"), &MeshSaverStaticInterface::LogStats ).param( _T("Msg") );
    this->function( _T("LogDebug"), &MeshSaverStaticInterface::LogDebug ).param( _T("Msg") );

    this->read_write_property( _T("PopupLogWindowOnError"), &MeshSaverStaticInterface::GetPopupLogWindowOnError,
                               &MeshSaverStaticInterface::SetPopupLogWindowOnError );
    this->read_write_property( _T("LogWindowVisible"), &MeshSaverStaticInterface::GetLogWindowVisible,
                               &MeshSaverStaticInterface::SetLogWindowVisible );
    this->function( _T("FocusLogWindow"), &MeshSaverStaticInterface::FocusLogWindow );
    this->read_write_property( _T("LoggingLevel"), &MeshSaverStaticInterface::GetLoggingLevel,
                               &MeshSaverStaticInterface::SetLoggingLevel );

    this->function( _T("ReplaceSequenceNumber"), &MeshSaverStaticInterface::ReplaceSequenceNumber )
        .param( _T("file") )
        .param( _T("frame") );

    this->function( _T("SetUserData"), &MeshSaverStaticInterface::SetUserData ).param( _T("key") ).param( _T("value") );
    this->function( _T("GetUserDataArray"), &MeshSaverStaticInterface::GetUserDataArray );
    this->function( _T("DeleteUserData"), &MeshSaverStaticInterface::DeleteUserData ).param( _T("key") );
    this->function( _T("ClearUserData"), &MeshSaverStaticInterface::ClearUserData );

    // c.add_function( & MeshSaverStaticInterface::SetChannelTransformType, _T("SetChannelTransformType"),
    // _T("channel"), _T("transformType") ); c.add_function( & MeshSaverStaticInterface::GetChannelTransformTypeArray,
    // _T("GetChannelTransformTypeArray") ); c.add_function( & MeshSaverStaticInterface::ClearChannelTransformType,
    // _T("ClearChannelTransformType") );

    this->function( _T("LoadMetadata"), &MeshSaverStaticInterface::LoadMetadata ).param( _T("filename") );
    this->function( _T("SaveMetadata"), &MeshSaverStaticInterface::SaveMetadata ).param( _T("filename") );

    this->read_write_property( _T("SourceChannels"), &MeshSaverStaticInterface::GetSourceChannelNames,
                               &MeshSaverStaticInterface::SetSourceChannelNames );

    this->read_write_property( _T("ObjFlipYZ"), &MeshSaverStaticInterface::GetObjFlipYZ,
                               &MeshSaverStaticInterface::SetObjFlipYZ );
}

MeshSaverStaticInterface theMeshSaverStaticInterface;

MeshSaverStaticInterface& GetMeshSaverStaticInterface() { return theMeshSaverStaticInterface; }

void InitializeMeshSaverLogging() { GetMeshSaverStaticInterface().InitializeLogging(); }
