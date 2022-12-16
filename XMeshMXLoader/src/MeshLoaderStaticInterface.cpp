// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/files/filename_sequence.hpp>
#include <frantic/geometry/polymesh3.hpp>
#include <frantic/geometry/polymesh3_file_io.hpp>
#include <frantic/geometry/xmesh_metadata.hpp>
#include <frantic/logging/logging_level.hpp>

#include <frantic/max3d/geometry/polymesh.hpp>
#include <frantic/max3d/max_utility.hpp>

#include "XMeshLoaderVersion.h"

#include "MeshLoaderStaticInterface.hpp"

using namespace std;
using namespace frantic;
using namespace frantic::max3d;

namespace fs = boost::filesystem;

extern HINSTANCE ghInstance;

/**
 * Register the interface and publish functions
 */
MeshLoaderStaticInterface::MeshLoaderStaticInterface()
    : m_logWindow( _T("XMesh Loader Log Window"), 0, 0, true )
    , m_logPopupError( true ) //,
                              // m_logPopupWarning( true )
{
    FFCreateDescriptor c( this, Interface_ID( 0x61b05af7, 0x2d9d262b ), _T("XMeshLoaderUtils"), 0 );

    c.add_property( &MeshLoaderStaticInterface::get_version, _T("Version") );
    c.add_property( &MeshLoaderStaticInterface::GetMeshLoaderHome, _T("MeshLoaderHome") );
    c.add_property( &MeshLoaderStaticInterface::GetPopupLogWindowOnError,
                    &MeshLoaderStaticInterface::SetPopupLogWindowOnError, _T("PopupLogWindowOnError") );
    c.add_property( &MeshLoaderStaticInterface::GetLogWindowVisible, &MeshLoaderStaticInterface::SetLogWindowVisible,
                    _T("LogWindowVisible") );
    c.add_function( &MeshLoaderStaticInterface::FocusLogWindow, _T("FocusLogWindow") );
    c.add_property( &MeshLoaderStaticInterface::GetLoggingLevel, &MeshLoaderStaticInterface::SetLoggingLevel,
                    _T("LoggingLevel") );
    c.add_function( &MeshLoaderStaticInterface::load_polymesh, _T("LoadPolymesh"), _T("Path") );

    c.add_function( &MeshLoaderStaticInterface::ReplaceSequenceNumber, _T("ReplaceSequenceNumber"), _T("file"),
                    _T("frame") );

    c.add_function( &MeshLoaderStaticInterface::LoadUserDataArray, _T("LoadUserDataArray"), _T("filename") );
}

/**
 * For grabbing the plugin script locations relative to the dll's
 *
 * @return the path to the plugin scripts
 */
frantic::tstring MeshLoaderStaticInterface::GetMeshLoaderHome() {
    // MeshLoader home is one level up from where this .dlr is (or two levels up if there is a win32/x64 subpath being
    // used)
    fs::path homePath = fs::path( win32::GetModuleFileName( ghInstance ) ).branch_path();

    // If this structure has been subdivided into win32 and x64 subdirectories, then go one directory back.
    frantic::tstring pathLeaf = frantic::strings::to_lower( frantic::files::to_tstring( homePath.leaf() ) );
    if( pathLeaf == _T("win32") || pathLeaf == _T("x64") )
        homePath = homePath.branch_path();

    // Now go back past the "3dsMax#Plugin" folder
    homePath = homePath.branch_path();

    return files::ensure_trailing_pathseparator( frantic::files::to_tstring( homePath ) );
}

/**
 * Returns a formatted version string
 *
 * @return a string of the form "Version: x.x.x.xxxxx"
 **/
frantic::tstring MeshLoaderStaticInterface::get_version() {

    frantic::tstring version( _T("Version: ") );
    version += _T( FRANTIC_VERSION );
    // The version data used to be in the form FRANTIC_VERSION = x.x.x,
    // FRANTIC_SUBVERSION_REVISION = xxxxx.  Now FRANTIC_VERSION includes
    // the subversion revision: x.x.x.xxxxx so I am getting rid of the
    // rev. text.
    // version += " rev.";
    // version += FRANTIC_SUBVERSION_REVISION;
    return version;
}

INode* MeshLoaderStaticInterface::load_polymesh( const frantic::tstring& path ) {
    INode* pNewNode = NULL;
    PolyObject* pNewPoly = NULL;
    try {
        pNewPoly = CreateEditablePolyObject();
        MNMesh& out = pNewPoly->GetMesh();
        frantic::geometry::polymesh3_ptr in = frantic::geometry::load_polymesh_file( path );

        frantic::max3d::geometry::polymesh_copy( out, in );

        Interface* ip = GetCOREInterface();
        pNewNode = ip->CreateObjectNode( pNewPoly );

        // TODO: Automatically set up an XMeshCache modifier to load animated data.

        return pNewNode;
    } catch( const std::exception& e ) {
        MessageBox( NULL, frantic::strings::to_tstring( e.what() ).c_str(), _T("MeshLoaderUtils.LoadPolymesh() ERROR"),
                    MB_OK );
        if( pNewPoly )
            pNewPoly->DeleteThis();
        if( pNewNode )
            pNewNode->DeleteThis();
        return NULL;
    } catch( ... ) {
        if( pNewPoly )
            pNewPoly->DeleteThis();
        if( pNewNode )
            pNewNode->DeleteThis();
        throw;
    }
}

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
                log->LogEntry( SYSLOG_ERROR, DISPLAY_DIALOG, _T( "XMesh Loader" ),
                               _T( "Unable to initialize XMesh Loader logging.\n\n%s" ), e.what() );
            }
        }
    }
}

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
        GetMeshLoaderInterface()->LogMessageInternal( frantic::tstring( _T("DBG: ") ) + szMsg );
    }
}

void to_stats_log( const TCHAR* szMsg ) {
    if( frantic::logging::is_logging_stats() ) {
        GetMeshLoaderInterface()->LogMessageInternal( frantic::tstring( _T("STS: ") ) + szMsg );
    }
}

void to_progress_log( const TCHAR* szMsg ) {
    if( frantic::logging::is_logging_progress() ) {
        GetMeshLoaderInterface()->LogMessageInternal( frantic::tstring( _T("PRG: ") ) + szMsg );
    }
}

void MeshLoaderStaticInterface::InitializeLogging() {
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
        if( GetCOREInterface() && !frantic::max3d::is_network_render_server() ) {
            m_logWindow.init( ghInstance, NULL );
        }
    }
}

void MeshLoaderStaticInterface::SetLoggingLevel( int loggingLevel ) {
    frantic::logging::set_logging_level( loggingLevel );
}

int MeshLoaderStaticInterface::GetLoggingLevel() { return frantic::logging::get_logging_level(); }

bool MeshLoaderStaticInterface::GetPopupLogWindowOnError() { return m_logPopupError; }

void MeshLoaderStaticInterface::SetPopupLogWindowOnError( bool popupError ) { m_logPopupError = popupError; }

bool MeshLoaderStaticInterface::GetLogWindowVisible() { return m_logWindow.is_visible(); }

void MeshLoaderStaticInterface::SetLogWindowVisible( bool visible ) { m_logWindow.show( visible ); }

void MeshLoaderStaticInterface::FocusLogWindow() {
    if( m_logWindow.is_visible() ) {
        SetFocus( m_logWindow.handle() );
    }
}

void MeshLoaderStaticInterface::LogMessageInternal( const frantic::tstring& msg ) { m_logWindow.log( msg ); }

frantic::tstring MeshLoaderStaticInterface::ReplaceSequenceNumber( const frantic::tstring& file, float frame ) {
    frantic::files::filename_pattern fp( file );
    return fp[frame];
}

FPValue MeshLoaderStaticInterface::LoadUserDataArray( const frantic::tstring& filename ) {
    typedef std::pair<frantic::tstring, frantic::tstring> string_pair;
    typedef std::vector<string_pair> result_t;

    if( !frantic::files::file_exists( filename ) ) {
        throw std::runtime_error( "XMeshLoaderUtils.LoadUserDataArray Error: input file \'" +
                                  frantic::strings::to_string( filename ) + "\' does not exist." );
    }

    frantic::geometry::xmesh_metadata metadata;
    read_xmesh_metadata( frantic::strings::to_wstring( filename ), metadata );

    std::vector<frantic::tstring> userDataKeys;
    metadata.get_user_data_keys( userDataKeys );

    result_t result;
    result.reserve( userDataKeys.size() );

    for( std::vector<frantic::tstring>::const_iterator i = userDataKeys.begin(); i != userDataKeys.end(); ++i ) {
        result.push_back( string_pair( *i, metadata.get_user_data( *i ) ) );
    }

    FPValue out;
    frantic::max3d::fpwrapper::MaxTypeTraits<result_t>::set_fpvalue( result, out );
    return out;
}

MeshLoaderStaticInterface* GetMeshLoaderInterface() {
    static MeshLoaderStaticInterface theMeshLoaderStaticInterface;
    return &theMeshLoaderStaticInterface;
}

void InitializeMeshLoaderLogging() { GetMeshLoaderInterface()->InitializeLogging(); }
