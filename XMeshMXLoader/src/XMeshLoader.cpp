// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "XMeshLoader.hpp"
#include "XMeshLoaderVersion.h"

#include <frantic/graphics/color3f.hpp>
#include <frantic/max3d/geometry/mesh.hpp>
#include <frantic/max3d/geometry/polymesh.hpp>
#include <frantic/max3d/geopipe/named_selection_sets.hpp>
#include <frantic/max3d/max_utility.hpp>
#include <frantic/max3d/ui/about_dialog.hpp>
#include <frantic/max3d/units.hpp>

#include <frantic/math/utils.hpp>

#include <frantic/geometry/polymesh3.hpp>
#include <frantic/geometry/polymesh3_builder.hpp>
#include <frantic/geometry/trimesh3.hpp>
#include <frantic/geometry/trimesh3_file_io.hpp>

#include <boost/assign/list_of.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/numeric/conversion/bounds.hpp>

#include <frantic/files/filename_sequence.hpp>

#include "attributions.hpp"
#include "mesh_channel_assignment.hpp"

#include <Shellapi.h>

#include <iostream>

#include <algorithm>

#if MAX_VERSION_MAJOR >= 14
#include <Graphics/IDisplayManager.h>
#endif

#include <IPathConfigMgr.h>

#if MAX_VERSION_MAJOR >= 12
#include <AssetManagement/AssetType.h>
#else
#include <IAssetAccessor.h>
#endif

#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/timer.hpp>

#include "fixed_combo_box.hpp"

#include "MeshLoaderStaticInterface.hpp"

#include "build_icon_mesh.hpp"

#include "xmesh_loader_gui_resources.hpp"

#include "resource.h"

/* namespaces */
using namespace frantic;
using namespace frantic::geometry;
using namespace frantic::files;
using namespace frantic::graphics;
using namespace frantic::max3d;
using namespace frantic::max3d::geometry;
using namespace frantic::channels;
using namespace frantic::geometry;
using namespace boost::assign;
using namespace xmesh;

extern HINSTANCE ghInstance;

extern TCHAR* GetString( UINT id );

boost::shared_ptr<Mesh> XMeshLoader::m_pIconMesh = BuildIconMesh();

XMeshLoader* XMeshLoader::editObj = 0;

// parameter list
enum {
    pb_showIcon,
    pb_iconSize,
    pb_meshScale,

    pb_keepMeshInMemory,

    pb_writeVelocityMapChannel,
    pb_velocityMapChannel,

    pb_renderSequence,
    pb_proxySequence,
    pb_autogenProxyPath,

    pb_loadSingleFrame,
    pb_frameOffset,
    pb_limitToRange,
    pb_rangeFirstFrame,
    pb_rangeLastFrame,
    pb_enablePlaybackGraph,
    pb_playbackGraphTime,
    pb_beforeRangeBehavior,
    pb_afterRangeBehavior,

    pb_loadMode,

    pb_enableViewportMesh,
    pb_enableRenderMesh,
    pb_renderSequenceID,
    pb_viewportSequenceID,
    pb_renderUsingViewportSettings,
    pb_displayMode,
    pb_displayPercent,

    pb_useFileLengthUnit,
    pb_lengthUnit,

    pb_ignoreMissingViewOnRenderNode
};

enum {
    main_param_map,
    help_param_map,
    files_param_map,
    loading_param_map,
    //
    param_map_count // keep last
};

// The class descriptor for XMeshLoader
class XMeshLoaderDesc : public ClassDesc2 {
    ParamBlockDesc2* m_pParamDesc;

  public:
    XMeshLoaderDesc();
    ~XMeshLoaderDesc();

    int IsPublic() { return TRUE; }
    void* Create( BOOL /*loading = FALSE*/ ) { return new XMeshLoader(); }
#if MAX_VERSION_MAJOR >= 24
    const TCHAR* NonLocalizedClassName() { return _T( XMeshLoader_CLASS_NAME ); }
#endif
    const TCHAR* ClassName() { return _T( XMeshLoader_CLASS_NAME ); }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return XMeshLoader_CLASS_ID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    // returns fixed parsable name (scripter-visible name)
    const TCHAR* InternalName() { return _T( XMeshLoader_CLASS_NAME ); }
    // returns owning module handle
    HINSTANCE HInstance() { return ghInstance; }
};

namespace FRAME_RANGE_CLAMP_MODE {
enum frame_range_clamp_mode_enum {
    HOLD,
    BLANK,
    //
    COUNT
};
};

#define FRAME_RANGE_CLAMP_MODE_HOLD_BEFORE _T("Hold First")
#define FRAME_RANGE_CLAMP_MODE_HOLD_AFTER _T("Hold Last")
#define FRAME_RANGE_CLAMP_MODE_BLANK _T("Blank")

#define DEFAULT_FRAME_RANGE_CLAMP_MODE ( FRAME_RANGE_CLAMP_MODE::HOLD )

namespace LOAD_MODE {
enum load_mode_enum {
    FRAME_VELOCITY_OFFSET,
    NEAREST_FRAME_VELOCITY_OFFSET,
    SUBFRAME_VELOCITY_OFFSET,
    FRAME_INTERPOLATION,
    SUBFRAME_INTERPOLATION,
    //
    COUNT
};
};

#define LOAD_MODE_FRAME_VELOCITY_OFFSET _T("Frame Velocity Offset")
#define LOAD_MODE_NEAREST_FRAME_VELOCITY_OFFSET _T("Nearest Frame Velocity Offset")
#define LOAD_MODE_SUBFRAME_VELOCITY_OFFSET _T("Subframe Velocity Offset")
#define LOAD_MODE_FRAME_INTERPOLATION _T("Frame Interpolation")
#define LOAD_MODE_SUBFRAME_INTERPOLATION _T("Subframe Interpolation")

#define DEFAULT_LOAD_MODE ( LOAD_MODE::FRAME_VELOCITY_OFFSET )

namespace SEQUENCE_ID {
enum sequence_id_enum {
    RENDER,
    PROXY,
    //
    COUNT
};
};

#define SEQUENCE_ID_RENDER _T("Render Sequence")
#define SEQUENCE_ID_PROXY _T("Proxy Sequence")

#define DEFAULT_RENDER_SEQUENCE_ID ( SEQUENCE_ID::RENDER )
#define DEFAULT_VIEWPORT_SEQUENCE_ID ( SEQUENCE_ID::PROXY )

namespace DISPLAY_MODE {
enum display_mode_enum {
    MESH,
    BOX,
    VERTICES,
    FACES,
    //
    COUNT
};
};

#define DISPLAY_MODE_MESH _T("Mesh")
#define DISPLAY_MODE_BOX _T("Box")
#define DISPLAY_MODE_VERTICES _T("Verts")
#define DISPLAY_MODE_FACES _T("Faces")

#define DEFAULT_DISPLAY_MODE ( DISPLAY_MODE::MESH )

namespace LENGTH_UNIT {
enum length_unit_enum {
    GENERIC,
    INCHES,
    FEET,
    MILES,
    MILLIMETERS,
    CENTIMETERS,
    METERS,
    KILOMETERS,
    CUSTOM,
    //
    COUNT
};
};

#define LENGTH_UNIT_GENERIC _T("Generic")
#define LENGTH_UNIT_INCHES _T("Inches")
#define LENGTH_UNIT_FEET _T("Feet")
#define LENGTH_UNIT_MILES _T("Miles")
#define LENGTH_UNIT_MILLIMETERS _T("Millimeters")
#define LENGTH_UNIT_CENTIMETERS _T("Centimeters")
#define LENGTH_UNIT_METERS _T("Meters")
#define LENGTH_UNIT_KILOMETERS _T("Kilometers")
#define LENGTH_UNIT_CUSTOM _T("Custom")

#define DEFAULT_LENGTH_UNIT ( LENGTH_UNIT::GENERIC )

class create_mouse_callback : public CreateMouseCallBack {
  private:
    XMeshLoader* m_obj;
    IPoint2 m_sp0;
    Point3 m_p0;

  public:
    int proc( ViewExp* vpt, int msg, int point, int flags, IPoint2 m, Matrix3& mat );
    void SetObj( XMeshLoader* obj );
};

int create_mouse_callback::proc( ViewExp* vpt, int msg, int point, int /*flags*/, IPoint2 m, Matrix3& mat ) {
    float r;
    Point3 p1, center;

    if( msg == MOUSE_FREEMOVE ) {
        vpt->SnapPreview( m, m, NULL, SNAP_IN_3D );
    }

    if( !m_obj ) {
        return CREATE_ABORT;
    }

    if( msg == MOUSE_POINT || msg == MOUSE_MOVE ) {
        switch( point ) {
        case 0:
            // m_obj->suspendSnap = TRUE;
            m_sp0 = m;
            m_p0 = vpt->SnapPoint( m, m, NULL, SNAP_IN_3D );
            mat.SetTrans( m_p0 );
            break;
        case 1:
            if( msg == MOUSE_MOVE ) {
                p1 = vpt->SnapPoint( m, m, NULL, SNAP_IN_3D );
                r = ( p1 - m_p0 ).Length();
                IParamBlock2* pb = m_obj->GetParamBlockByID( 0 );
                if( !pb ) {
                    return CREATE_ABORT;
                }
                pb->SetValue( pb_iconSize, 0, 2.f * r );
            } else if( msg == MOUSE_POINT ) {
                // m_obj->suspendSnap=FALSE;
                return CREATE_STOP;
            }
            break;
        }
    }

    if( msg == MOUSE_ABORT ) {
        return CREATE_ABORT;
    }

    return TRUE;
}

void create_mouse_callback::SetObj( XMeshLoader* obj ) { m_obj = obj; }

static create_mouse_callback g_createMouseCB;

class XMeshLoaderPLCB : public PostLoadCallback {
    XMeshLoader* parent;

  public:
    XMeshLoaderPLCB( XMeshLoader* p )
        : parent( p ) {}

    void proc( ILoad* iload ) {
        parent->PostLoadCallback( iload );
        delete this;
    }
};

class MainDlgProc : public ParamMap2UserDlgProc {
    XMeshLoader* m_obj;

  public:
    MainDlgProc()
        : m_obj( 0 ) {}
    virtual ~MainDlgProc() {}
    virtual INT_PTR DlgProc( TimeValue /*t*/, IParamMap2* /*map*/, HWND /*hWnd*/, UINT msg, WPARAM /*wParam*/,
                             LPARAM /*lParam*/ ) {
        switch( msg ) {
        case WM_INITDIALOG:
            if( m_obj ) {
                UpdateEnables();
            }
            return TRUE;
        }
        return FALSE;
    }
    virtual void DeleteThis() {}
    void Update( TimeValue /*t*/ ) {
        try {
            UpdateEnables();
        } catch( const std::exception& e ) {
            frantic::tstring errmsg = _T("MainDlgProc::Update: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
            FF_LOG( error ) << errmsg << std::endl;
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Error"), _T("%s"), e.what() );
            if( frantic::max3d::is_network_render_server() )
                throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
        }
    }
    void UpdateEnables() {
        if( !m_obj ) {
            return;
        }

        IParamBlock2* pb = m_obj->GetParamBlockByID( mesh_loader_param_block );
        if( !pb ) {
            return;
        }
        IParamMap2* pm = pb->GetMap( main_param_map );
        if( !pm ) {
            return;
        }
        const HWND hwnd = pm->GetHWnd();
        if( !hwnd ) {
            return;
        }

        const bool enableMapChannel = pb->GetInt( pb_writeVelocityMapChannel ) != 0;

        EnableWindow( GetDlgItem( hwnd, IDC_VELOCITY_MAP_CHANNEL_STATIC ), enableMapChannel );
        pm->Enable( pb_velocityMapChannel, enableMapChannel );
    }
    void set_obj( XMeshLoader* obj ) { m_obj = obj; }
};

MainDlgProc* GetMainDlgProc() {
    static MainDlgProc mainDlgProc;
    return &mainDlgProc;
}

class HelpDlgProc : public ParamMap2UserDlgProc {
    XMeshLoader* m_obj;

  public:
    HelpDlgProc()
        : m_obj( 0 ) {}
    virtual ~HelpDlgProc() {}
    virtual INT_PTR DlgProc( TimeValue /*t*/, IParamMap2* /*map*/, HWND hWnd, UINT msg, WPARAM wParam,
                             LPARAM /*lParam*/ ) {
        switch( msg ) {
        case WM_INITDIALOG: {
            SetWindowText( GetDlgItem( hWnd, IDC_VERSION ),
                           ( frantic::tstring( _T("Version: ") ) + _T( FRANTIC_VERSION ) ).c_str() );
        }
            return TRUE;
        case WM_COMMAND: {
            const int id = LOWORD( wParam );
            // const int notifyCode = HIWORD( wParam );
            switch( id ) {
            case IDC_ONLINE_HELP: {
                HINSTANCE result = ShellExecute( NULL, _T("open"),
                                                 _T("http://www.thinkboxsoftware.com"),
                                                 NULL, NULL, SW_SHOWNORMAL );
                if( result <= (HINSTANCE)32 ) {
                    throw std::runtime_error( "Unable to open online help." );
                }
            } break;
            case IDC_ABOUT:
                frantic::max3d::ui::show_about_dialog( ghInstance, _T("About XMesh Loader"), _T("XMesh Loader MX"),
                                                       _T( FRANTIC_VERSION ), get_attributions() );
                break;
            case IDC_LOG_WINDOW:
                //			if( notifyCode == BN_RIGHTCLICK ) {
                //				notify_if_missing_frost_mxs();
                //				frantic::max3d::mxs::expression( "if (FrostUi!=undefined) do
                //(FrostUi.on_LogWindowOptions_pressed frostNode)" ) 					.bind( "frostNode", m_obj ).at_time( t
                //).evaluate<void>(); 			} else {
                GetMeshLoaderInterface()->SetLogWindowVisible( true );
                GetMeshLoaderInterface()->FocusLogWindow();
                //			}
                break;
            }
        } break;
        }
        return FALSE;
    }
    virtual void DeleteThis() {}
    void set_obj( XMeshLoader* obj ) { m_obj = obj; }
};

HelpDlgProc* GetHelpDlgProc() {
    static HelpDlgProc helpDlgProc;
    return &helpDlgProc;
}

// TODO: is this exception handling necessary?
frantic::tstring get_path_filename( const frantic::tstring& path ) {
    if( path.empty() ) {
        return _T("");
    }
    try {
        return frantic::files::filename_from_path( path );
    } catch( std::runtime_error& ) {
    }
    return _T("");
}

// TODO: is this exception handling necessary?
frantic::tstring get_path_directory( const frantic::tstring& path ) {
    if( path.empty() ) {
        return _T("");
    }
    try {
        return frantic::files::directory_from_path( path );
    } catch( std::runtime_error& ) {
    }
    return _T("");
}

bool is_valid_file_name( const frantic::tstring& filename ) {
    if( filename.empty() ) {
        return false;
    }
    const TCHAR lastChar = filename[filename.size() - 1];
    if( lastChar == '.' || lastChar == ' ' ) {
        return false;
    }

    // forbid:
    // 0
    //< > : " / \ | ? *
    //[1,31]
    std::set<TCHAR> forbiddenCharacters =
        boost::assign::list_of<TCHAR>( '<' )( '>' )( ':' )( '\"' )( '/' )( '\\' )( '|' )( '?' )( '*' );
    for( TCHAR c = 0; c <= 31; ++c ) {
        forbiddenCharacters.insert( c );
    }

    for( frantic::tstring::size_type i = 0; i < filename.size(); ++i ) {
        if( forbiddenCharacters.find( filename[i] ) != forbiddenCharacters.end() ) {
            return false;
        }
    }

    std::set<frantic::tstring> forbiddenNames = boost::assign::list_of( _T("CON") )( _T("PRN") )( _T("AUX") )(
        _T("NUL") )( _T("COM1") )( _T("COM2") )( _T("COM3") )( _T("COM4") )( _T("COM5") )( _T("COM6") )( _T("COM7") )(
        _T("COM8") )( _T("COM9") )( _T("LPT1") )( _T("LPT2") )( _T("LPT3") )( _T("LPT4") )( _T("LPT5") )( _T("LPT6") )(
        _T("LPT7") )( _T("LPT8") )( _T("LPT9") );
    if( forbiddenNames.find( filename ) != forbiddenNames.end() ) {
        return false;
    }

    return true;
}

frantic::tstring do_open_mesh_file_dialog( const frantic::tstring& initialPath = _T("") ) {
    MSTR filename;
    MSTR initialDir;

    if( frantic::files::directory_exists( initialPath ) ) {
        initialDir = initialPath.c_str();
        filename = _T("");
    } else {
        const frantic::tstring initialPathDirectory = get_path_directory( initialPath );
        if( frantic::files::directory_exists( initialPathDirectory ) ) {
            initialDir = initialPathDirectory.c_str();
        } else {
            initialDir = _T("");
        }
        const frantic::tstring initialPathFilename = get_path_filename( initialPath );
        if( is_valid_file_name( initialPathFilename ) ) {
            filename = initialPathFilename.c_str();
        } else {
            filename = _T("");
        }
    }

    FilterList extList;
    extList.Append( _M( "All Mesh Files" ) );
    extList.Append( _M( "*.xmesh;*.obj" ) );
    extList.Append( _M( "Thinkbox XMesh Files (*.xmesh)" ) );
    extList.Append( _M( "*.xmesh" ) );
    extList.Append( _M( "Wavefront OBJ Files (*.obj)" ) );
    extList.Append( _M( "*.obj" ) );
    extList.Append( _M( "All Files (*.*)" ) );
    extList.Append( _M( "*.*" ) );
    bool success = GetCOREInterface8()->DoMaxOpenDialog(
        GetCOREInterface()->GetMAXHWnd(), _M( "Select the Mesh Sequence to Add" ), filename, initialDir, extList );
    if( success && filename.data() && !frantic::tstring( filename.data() ).empty() &&
        frantic::files::file_exists( frantic::tstring( filename.data() ) ) ) {
        return frantic::tstring( filename.data() );
    }
    return _T("");
}

class FilesDlgProc : public ParamMap2UserDlgProc {
    XMeshLoader* m_obj;
    // keep track of tooltip updates to make sure they are updated
    // otherwise they can get stuck in at least one case: when you create an
    // xmesh loader and delete it before leaving the create panel
    // in that case Update() gets called but not WM_INITDIALOG
    bool m_needTooltipUpdate;
    std::vector<int> m_beforeRangeBehaviorIndexToCode;
    std::vector<int> m_afterRangeBehaviorIndexToCode;
    std::vector<int> m_loadModeIndexToCode;
    std::vector<int> m_lengthUnitIndexToCode;

  public:
    FilesDlgProc()
        : m_obj( 0 )
        , m_needTooltipUpdate( true ) {}
    virtual ~FilesDlgProc() {}
    virtual INT_PTR DlgProc( TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM /*lParam*/ ) {
        switch( msg ) {
        case WM_INITDIALOG:
            if( m_obj ) {
                fixed_combo_box beforeRangeBehaviorControl(
                    hWnd, IDC_PARTICLE_FILES_BEFORE_RANGE_BEHAVIOR, XMeshLoader::get_before_range_behavior_codes,
                    XMeshLoader::get_before_range_behavior_name, m_beforeRangeBehaviorIndexToCode );
                beforeRangeBehaviorControl.reset_strings();
                beforeRangeBehaviorControl.set_cur_sel_code( m_obj->get_before_range_behavior() );

                fixed_combo_box afterRangeBehaviorControl(
                    hWnd, IDC_PARTICLE_FILES_AFTER_RANGE_BEHAVIOR, XMeshLoader::get_after_range_behavior_codes,
                    XMeshLoader::get_after_range_behavior_name, m_afterRangeBehaviorIndexToCode );
                afterRangeBehaviorControl.reset_strings();
                afterRangeBehaviorControl.set_cur_sel_code( m_obj->get_after_range_behavior() );

                fixed_combo_box loadModeControl( hWnd, IDC_MOTION_BLUR_MODE, XMeshLoader::get_load_mode_codes,
                                                 XMeshLoader::get_load_mode_name, m_loadModeIndexToCode );
                loadModeControl.reset_strings();
                loadModeControl.set_cur_sel_code( m_obj->get_load_mode() );

                fixed_combo_box lengthUnitControl( hWnd, IDC_LENGTH_UNIT, XMeshLoader::get_length_unit_codes,
                                                   XMeshLoader::get_length_unit_name, m_lengthUnitIndexToCode );
                lengthUnitControl.reset_strings();
                lengthUnitControl.set_cur_sel_code( m_obj->get_length_unit() );

                UpdateTooltips();

                UpdateEnables();

                // UpdateStatusMessages();

                update_loading_frame_info();
            }
            return TRUE;
        case WM_COMMAND: {
            const int id = LOWORD( wParam );
            const int notifyCode = HIWORD( wParam );
            switch( id ) {
            case IDC_ENABLE_AUTO_PROXY_SEQUENCE:
                if( m_obj ) {
                    if( Button_GetCheck( GetDlgItem( hWnd, id ) ) == BST_CHECKED ) {
                        m_obj->check_auto_proxy_path();
                    }
                }
                break;
            case IDC_PARTICLE_FILES_BEFORE_RANGE_BEHAVIOR:
                if( CBN_SELCHANGE == notifyCode ) {
                    fixed_combo_box beforeRangeBehaviorControl(
                        hWnd, IDC_PARTICLE_FILES_BEFORE_RANGE_BEHAVIOR, XMeshLoader::get_before_range_behavior_codes,
                        XMeshLoader::get_before_range_behavior_name, m_beforeRangeBehaviorIndexToCode );
                    int sel = beforeRangeBehaviorControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_before_range_behavior( sel );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
                break;
            case IDC_PARTICLE_FILES_AFTER_RANGE_BEHAVIOR:
                if( CBN_SELCHANGE == notifyCode ) {
                    fixed_combo_box afterRangeBehaviorControl(
                        hWnd, IDC_PARTICLE_FILES_AFTER_RANGE_BEHAVIOR, XMeshLoader::get_after_range_behavior_codes,
                        XMeshLoader::get_after_range_behavior_name, m_afterRangeBehaviorIndexToCode );
                    int sel = afterRangeBehaviorControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_after_range_behavior( sel );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
                break;
            case IDC_MOTION_BLUR_MODE:
                if( CBN_SELCHANGE == notifyCode ) {
                    fixed_combo_box loadModeControl( hWnd, IDC_MOTION_BLUR_MODE, XMeshLoader::get_load_mode_codes,
                                                     XMeshLoader::get_load_mode_name, m_loadModeIndexToCode );
                    int sel = loadModeControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_load_mode( sel );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
                break;
            case IDC_RENDER_SEQUENCE_BROWSE:
                if( m_obj ) {
                    const frantic::tstring filename = do_open_mesh_file_dialog( m_obj->get_render_path() );
                    if( !filename.empty() ) {
                        if( map ) {
                            IParamBlock2* pblock = map->GetParamBlock();
                            if( pblock ) {
#if MAX_VERSION_MAJOR >= 12
                                pblock->SetValue( pb_renderSequence, t, filename.c_str() );
#else
                                MSTR temp( filename.c_str() );
                                pblock->SetValue( pb_renderSequence, t, temp );
#endif
                            }
                        }

                        m_obj->set_to_valid_frame_range( false, true );
                        if( m_obj->is_autogen_proxy_path_enabled() ) {
                            m_obj->check_auto_proxy_path();
                        } else {
                            m_obj->set_control_viewport_sequence_id( SEQUENCE_ID::RENDER );
                        }

                        m_obj->invalidate();
                    }
                }
                break;
            case IDC_PROXY_SEQUENCE_BROWSE:
                if( m_obj ) {
                    const frantic::tstring filename = do_open_mesh_file_dialog( m_obj->get_proxy_path() );
                    if( !filename.empty() ) {
                        if( map ) {
                            IParamBlock2* pblock = map->GetParamBlock();
                            if( pblock ) {
#if MAX_VERSION_MAJOR >= 12
                                pblock->SetValue( pb_proxySequence, t, filename.c_str() );
#else
                                MSTR temp( filename.c_str() );
                                pblock->SetValue( pb_proxySequence, t, temp );
#endif
                            }
                        }
                        m_obj->invalidate();
                    }
                }
                break;
            case IDC_RELOAD_SOURCE_SEQUENCES:
                if( m_obj ) {
                    m_obj->invalidate_sequences();
                    m_obj->invalidate();
                }
                break;
            case IDC_SET_USING_EXISTING_FRAMES:
                if( m_obj ) {
                    m_obj->set_to_valid_frame_range( true, false );
                    m_obj->invalidate();
                }
                break;
            case IDC_LENGTH_UNIT:
                if( m_obj && CBN_SELCHANGE == notifyCode ) {
                    fixed_combo_box lengthUnitControl( hWnd, IDC_LENGTH_UNIT, XMeshLoader::get_length_unit_codes,
                                                       XMeshLoader::get_length_unit_name, m_lengthUnitIndexToCode );
                    int sel = lengthUnitControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_length_unit( sel );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
            }
        }
        }
        return FALSE;
    }
    void Update( TimeValue /*t*/ ) {
        try {
            if( m_needTooltipUpdate ) {
                UpdateTooltips();
                update_loading_frame_info();
            }
            UpdateEnables();
            // UpdateStatusMessages();
        } catch( const std::exception& e ) {
            frantic::tstring errmsg =
                _T("FilesDlgProc::Update: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
            FF_LOG( error ) << errmsg << std::endl;
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Error"), _T("%s"), e.what() );
            if( frantic::max3d::is_network_render_server() )
                throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
        }
    }

    virtual void DeleteThis() {}

    void UpdateTooltips() {
        if( !m_obj ) {
            return;
        }
        IParamBlock2* pb = m_obj->GetParamBlockByID( mesh_loader_param_block );
        if( !pb ) {
            return;
        }
        IParamMap2* pm = pb->GetMap( files_param_map );
        if( !pm ) {
            return;
        }
        const HWND hwnd = pm->GetHWnd();
        if( !hwnd ) {
            return;
        }

        const frantic::tstring renderPath = m_obj->get_render_path();
        frantic::max3d::SetCustEditTooltip( GetDlgItem( hwnd, IDC_RENDER_SEQUENCE ), !renderPath.empty(), renderPath );
        const frantic::tstring proxyPath = m_obj->get_proxy_path();
        frantic::max3d::SetCustEditTooltip( GetDlgItem( hwnd, IDC_PROXY_SEQUENCE ), !proxyPath.empty(), proxyPath );
        m_needTooltipUpdate = false;
    }

    void UpdateEnables() {
        if( !m_obj ) {
            return;
        }

        IParamBlock2* pb = m_obj->GetParamBlockByID( mesh_loader_param_block );
        if( !pb ) {
            return;
        }
        IParamMap2* pm = pb->GetMap( files_param_map );
        if( !pm ) {
            return;
        }
        const HWND hwnd = pm->GetHWnd();
        if( !hwnd ) {
            return;
        }

        const bool autogenProxyPath = pb->GetInt( pb_autogenProxyPath ) != 0;
        const bool loadSingleFrame = pb->GetInt( pb_loadSingleFrame ) != 0; // m_obj->get_load_single_frame();
        const bool enablePlaybackGraph =
            pb->GetInt( pb_enablePlaybackGraph ) != 0;                // m_obj->get_enable_playback_graph();
        const bool limitToRange = pb->GetInt( pb_limitToRange ) != 0; // m_obj->get_limit_to_range();
        const bool enableLengthUnit = pb->GetInt( pb_useFileLengthUnit ) == 0;
        const bool enableCustomLengthUnit = enableLengthUnit && ( m_obj->get_length_unit() == ( LENGTH_UNIT::CUSTOM ) );
        // const bool inCreateMode = GetCOREInterface()->GetCommandPanelTaskMode() == TASK_MODE_CREATE;

        // proxy path controls
        EnableWindow( GetDlgItem( hwnd, IDC_PROXY_SEQUENCE_STATIC ), !autogenProxyPath );
        pm->Enable( pb_proxySequence, !autogenProxyPath );
        EnableCustButton( GetDlgItem( hwnd, IDC_PROXY_SEQUENCE_BROWSE ), !autogenProxyPath );

        // use playback graph
        pm->Enable( pb_enablePlaybackGraph, !loadSingleFrame );
        // playback graph
        pm->Enable( pb_playbackGraphTime, enablePlaybackGraph && !loadSingleFrame );

        // frame offset
        EnableWindow( GetDlgItem( hwnd, IDC_FRAME_OFFSET_STATIC ), !loadSingleFrame );
        pm->Enable( pb_frameOffset, !loadSingleFrame );
        // limit to range
        pm->Enable( pb_limitToRange, !loadSingleFrame );
        // range
        EnableCustButton( GetDlgItem( hwnd, IDC_SET_USING_EXISTING_FRAMES ), limitToRange && !loadSingleFrame );
        pm->Enable( pb_rangeFirstFrame, limitToRange && !loadSingleFrame );
        EnableWindow( GetDlgItem( hwnd, IDC_RANGE_SEPARATOR_STATIC ), limitToRange && !loadSingleFrame );
        pm->Enable( pb_rangeLastFrame, limitToRange && !loadSingleFrame );
        // hold first
        EnableWindow( GetDlgItem( hwnd, IDC_PARTICLE_FILES_BEFORE_RANGE_BEHAVIOR ), limitToRange && !loadSingleFrame );
        // hold last
        EnableWindow( GetDlgItem( hwnd, IDC_PARTICLE_FILES_AFTER_RANGE_BEHAVIOR ), limitToRange && !loadSingleFrame );

        // length unit controls
        EnableWindow( GetDlgItem( hwnd, IDC_LENGTH_UNIT ), enableLengthUnit );

        // custom length unit controls
        EnableWindow( GetDlgItem( hwnd, IDC_CUSTOM_SCALE_STATIC ), enableCustomLengthUnit );
        pm->Enable( pb_meshScale, enableCustomLengthUnit );
    }

    void update_loading_frame_info() {
        if( !m_obj ) {
            return;
        }

        if( m_obj->editObj == m_obj ) {
            IParamBlock2* pb = m_obj->GetParamBlockByID( mesh_loader_param_block );
            if( !pb ) {
                return;
            }
            IParamMap2* map = pb->GetMap( files_param_map );
            if( !map ) {
                return;
            }
            const HWND hwnd = map->GetHWnd();
            if( !hwnd ) {
                return;
            }

            HWND hwndText = GetDlgItem( hwnd, IDC_LOADING_FRAME );
            const frantic::tstring s = m_obj->get_loading_frame_info( GetCOREInterface()->GetTime() );
            frantic::max3d::SetCustStatusText( hwndText, s );
        }
    }

    void InvalidateUI( HWND hwnd, int element ) {
        if( !hwnd ) {
            return;
        }
        switch( element ) {
        case pb_renderSequence: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const frantic::tstring renderPath = m_obj->get_render_path();
                frantic::max3d::SetCustEditText( GetDlgItem( hwnd, IDC_RENDER_SEQUENCE ), renderPath );
                frantic::max3d::SetCustEditTooltip( GetDlgItem( hwnd, IDC_RENDER_SEQUENCE ), !renderPath.empty(),
                                                    renderPath );
            }
        } break;
        case pb_proxySequence: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const frantic::tstring proxyPath = m_obj->get_proxy_path();
                frantic::max3d::SetCustEditText( GetDlgItem( hwnd, IDC_PROXY_SEQUENCE ), proxyPath );
                frantic::max3d::SetCustEditTooltip( GetDlgItem( hwnd, IDC_PROXY_SEQUENCE ), !proxyPath.empty(),
                                                    proxyPath );
            }
        } break;
        case pb_beforeRangeBehavior: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int beforeRangeBehavior = m_obj->get_before_range_behavior();
                fixed_combo_box beforeRangeBehaviorControl(
                    hwnd, IDC_PARTICLE_FILES_BEFORE_RANGE_BEHAVIOR, XMeshLoader::get_before_range_behavior_codes,
                    XMeshLoader::get_before_range_behavior_name, m_beforeRangeBehaviorIndexToCode );
                beforeRangeBehaviorControl.set_cur_sel_code( beforeRangeBehavior );
            }
        } break;
        case pb_afterRangeBehavior: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int afterRangeBehavior = m_obj->get_after_range_behavior();
                fixed_combo_box afterRangeBehaviorControl(
                    hwnd, IDC_PARTICLE_FILES_AFTER_RANGE_BEHAVIOR, XMeshLoader::get_after_range_behavior_codes,
                    XMeshLoader::get_after_range_behavior_name, m_afterRangeBehaviorIndexToCode );
                afterRangeBehaviorControl.set_cur_sel_code( afterRangeBehavior );
            }
        } break;
        case pb_loadMode: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int loadMode = m_obj->get_load_mode();
                fixed_combo_box loadModeControl( hwnd, IDC_MOTION_BLUR_MODE, XMeshLoader::get_load_mode_codes,
                                                 XMeshLoader::get_load_mode_name, m_loadModeIndexToCode );
                loadModeControl.set_cur_sel_code( loadMode );
            }
        } break;
        case pb_lengthUnit: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int lengthUnit = m_obj->get_length_unit();
                fixed_combo_box lengthUnitControl( hwnd, IDC_LENGTH_UNIT, XMeshLoader::get_length_unit_codes,
                                                   XMeshLoader::get_length_unit_name, m_lengthUnitIndexToCode );
                lengthUnitControl.set_cur_sel_code( lengthUnit );
                UpdateEnables();
            }
        } break;
        }
    }
    void set_obj( XMeshLoader* obj ) {
        m_obj = obj;
        m_needTooltipUpdate = true;
    }
};

FilesDlgProc* GetFilesDlgProc() {
    static FilesDlgProc filesDlgProc;
    return &filesDlgProc;
}

class LoadingDlgProc : public ParamMap2UserDlgProc {
    XMeshLoader* m_obj;
    std::vector<int> m_renderSequenceIDIndexToCode;
    std::vector<int> m_viewportSequenceIDIndexToCode;
    std::vector<int> m_displayModeIndexToCode;

  public:
    LoadingDlgProc()
        : m_obj( 0 ) {}
    virtual ~LoadingDlgProc() {}
    virtual INT_PTR DlgProc( TimeValue /*t*/, IParamMap2* /*map*/, HWND hWnd, UINT msg, WPARAM wParam,
                             LPARAM /*lParam*/ ) {
        switch( msg ) {
        case WM_INITDIALOG: {
            if( m_obj ) {
                fixed_combo_box renderSequenceIDControl(
                    hWnd, IDC_RENDER_QUALITY, XMeshLoader::get_render_sequence_id_codes,
                    XMeshLoader::get_render_sequence_id_name, m_renderSequenceIDIndexToCode );
                renderSequenceIDControl.reset_strings();
                renderSequenceIDControl.set_cur_sel_code( m_obj->get_control_render_sequence_id() );

                fixed_combo_box viewportSequenceIDControl(
                    hWnd, IDC_VIEWPORT_QUALITY, XMeshLoader::get_viewport_sequence_id_codes,
                    XMeshLoader::get_viewport_sequence_id_name, m_viewportSequenceIDIndexToCode );
                viewportSequenceIDControl.reset_strings();
                viewportSequenceIDControl.set_cur_sel_code( m_obj->get_control_viewport_sequence_id() );

                fixed_combo_box displayModeControl( hWnd, IDC_DISPLAY_MODE, XMeshLoader::get_display_mode_codes,
                                                    XMeshLoader::get_display_mode_name, m_displayModeIndexToCode );
                displayModeControl.reset_strings();
                displayModeControl.set_cur_sel_code( m_obj->get_display_mode() );

                UpdateEnables();
            }
        }
            return TRUE;
        case WM_COMMAND: {
            const int id = LOWORD( wParam );
            const int notifyCode = HIWORD( wParam );
            switch( id ) {
            case IDC_RENDER_QUALITY:
                if( CBN_SELCHANGE == notifyCode ) {
                    fixed_combo_box renderSequenceIDControl(
                        hWnd, IDC_RENDER_QUALITY, XMeshLoader::get_render_sequence_id_codes,
                        XMeshLoader::get_render_sequence_id_name, m_renderSequenceIDIndexToCode );
                    int sel = renderSequenceIDControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_control_render_sequence_id( sel );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
                break;
            case IDC_VIEWPORT_QUALITY:
                if( CBN_SELCHANGE == notifyCode ) {
                    fixed_combo_box viewportSequenceIDControl(
                        hWnd, IDC_VIEWPORT_QUALITY, XMeshLoader::get_viewport_sequence_id_codes,
                        XMeshLoader::get_viewport_sequence_id_name, m_viewportSequenceIDIndexToCode );
                    int sel = viewportSequenceIDControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_control_viewport_sequence_id( sel );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
                break;
            case IDC_DISPLAY_MODE:
                if( CBN_SELCHANGE == notifyCode ) {
                    fixed_combo_box displayModeControl( hWnd, IDC_DISPLAY_MODE, XMeshLoader::get_display_mode_codes,
                                                        XMeshLoader::get_display_mode_name, m_displayModeIndexToCode );
                    int sel = displayModeControl.get_cur_sel_code();
                    if( !theHold.Holding() ) {
                        theHold.Begin();
                    }
                    m_obj->set_display_mode( sel );
                    theHold.Accept( IDS_PARAMETER_CHANGE );
                }
                break;
            }
        }
        }
        return FALSE;
    }
    virtual void DeleteThis() {}
    void InvalidateUI( HWND hwnd, int element ) {
        if( !hwnd ) {
            return;
        }
        switch( element ) {
        case pb_renderSequenceID: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int renderSequenceID = m_obj->get_control_render_sequence_id();
                fixed_combo_box renderSequenceIDControl(
                    hwnd, IDC_RENDER_QUALITY, XMeshLoader::get_render_sequence_id_codes,
                    XMeshLoader::get_render_sequence_id_name, m_renderSequenceIDIndexToCode );
                renderSequenceIDControl.set_cur_sel_code( renderSequenceID );
            }
        } break;
        case pb_viewportSequenceID: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int viewportSequenceID = m_obj->get_control_viewport_sequence_id();
                fixed_combo_box viewportSequenceIDControl(
                    hwnd, IDC_VIEWPORT_QUALITY, XMeshLoader::get_viewport_sequence_id_codes,
                    XMeshLoader::get_viewport_sequence_id_name, m_viewportSequenceIDIndexToCode );
                viewportSequenceIDControl.set_cur_sel_code( viewportSequenceID );
            }
        } break;
        case pb_displayMode: {
            if( m_obj && m_obj->editObj == m_obj ) {
                const int displayMode = m_obj->get_display_mode();
                fixed_combo_box displayModeControl( hwnd, IDC_DISPLAY_MODE, XMeshLoader::get_display_mode_codes,
                                                    XMeshLoader::get_display_mode_name, m_displayModeIndexToCode );
                displayModeControl.set_cur_sel_code( displayMode );
            }
        } break;
        }
    }
    void Update( TimeValue /*t*/ ) {
        try {
            UpdateEnables();
        } catch( const std::exception& e ) {
            frantic::tstring errmsg =
                _T("LoadingDlgProc::Update: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
            FF_LOG( error ) << errmsg << std::endl;
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Error"), _T("%s"), e.what() );
            if( frantic::max3d::is_network_render_server() )
                throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
        }
    }
    void UpdateEnables() {
        if( !m_obj ) {
            return;
        }

        IParamBlock2* pb = m_obj->GetParamBlockByID( mesh_loader_param_block );
        if( !pb ) {
            return;
        }
        IParamMap2* pm = pb->GetMap( loading_param_map );
        if( !pm ) {
            return;
        }
        const HWND hwnd = pm->GetHWnd();
        if( !hwnd ) {
            return;
        }

        const bool useViewportSettings = false; // pb->GetInt( pb_renderUsingViewportSettings ) != 0;
        const int displayMode = pb->GetInt( pb_displayMode );
        const bool enablePercent = ( displayMode == DISPLAY_MODE::FACES || displayMode == DISPLAY_MODE::VERTICES );

        EnableWindow( GetDlgItem( hwnd, IDC_RENDER_QUALITY ), !useViewportSettings );

        pm->Enable( pb_displayPercent, enablePercent );
        EnableWindow( GetDlgItem( hwnd, IDC_DISPLAY_PERCENT_STATIC ), enablePercent );
    }
    void set_obj( XMeshLoader* obj ) { m_obj = obj; }
};

LoadingDlgProc* GetLoadingDlgProc() {
    static LoadingDlgProc loadingDlgProc;
    return &loadingDlgProc;
}

class mesh_loader_pb_accessor : public PBAccessor {
  public:
    // void  Get( PB2Value & v, ReferenceMaker *owner, ParamID id, int tabIndex, TimeValue t, Interval & valid );
    void Set( PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t ) {
        XMeshLoader* obj = (XMeshLoader*)owner;
        if( !obj ) {
            return;
        }
        IParamBlock2* pb = obj->GetParamBlockByID( mesh_loader_param_block );
        if( !pb ) {
            return;
        }

        try {
            obj->on_param_set( v, id, tabIndex, t );
        } catch( std::exception& e ) {
            frantic::tstring errmsg = _T("XMeshLoader: Set: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
            FF_LOG( error ) << errmsg << std::endl;
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
            if( frantic::max3d::is_network_render_server() )
                throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
        }
    }

    void TabChanged( tab_changes /*changeCode*/, Tab<PB2Value>* /*tab*/, ReferenceMaker* owner, ParamID /*id*/,
                     int /*tabIndex*/, int /*count*/ ) {
        XMeshLoader* obj = (XMeshLoader*)owner;
        if( !obj ) {
            return;
        }
        IParamBlock2* pb = obj->GetParamBlockByID( mesh_loader_param_block );
        if( !pb ) {
            return;
        }

        try {
            // obj->on_tab_changed( changeCode, tab, id, tabIndex, count );
        } catch( std::exception& e ) {
            frantic::tstring errmsg =
                _T("XMeshLoader: TabChanged: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
            FF_LOG( error ) << errmsg << std::endl;
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
            if( frantic::max3d::is_network_render_server() )
                throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
        }
    }
};

mesh_loader_pb_accessor* GetMeshLoaderPBAccessor() {
    static mesh_loader_pb_accessor meshLoaderPBAccessor;
    return &meshLoaderPBAccessor;
}

class XMeshLoaderAssetAccessor : public IAssetAccessor {
  protected:
    XMeshLoader* m_XMeshLoader;
    ParamID m_parameterId;

  public:
    XMeshLoaderAssetAccessor( XMeshLoader* XMeshLoader, ParamID parameterId )
        : m_XMeshLoader( XMeshLoader )
        , m_parameterId( parameterId ) {}
#if MAX_VERSION_MAJOR >= 12
    MaxSDK::AssetManagement::AssetUser GetAsset() const {
        return m_XMeshLoader->pblock2->GetAssetUser( m_parameterId );
    }
#else
    MaxSDK::Util::Path GetPath() const {
        TCHAR* path = 0;
        Interval ivalid;
        m_XMeshLoader->pblock2->GetValue( m_parameterId, 0, path, ivalid );
        TSTR pathString;
        if( path ) {
            pathString = path;
        }
        return MaxSDK::Util::Path( pathString.data() );
    }
#endif

#if MAX_VERSION_MAJOR >= 12
    bool SetAsset( const MaxSDK::AssetManagement::AssetUser& aNewAssetUser ) {
        m_XMeshLoader->pblock2->SetValue( m_parameterId, 0, aNewAssetUser );
        return true;
    }
#else
    void SetPath( const MaxSDK::Util::Path& aNewPath ) {
        m_XMeshLoader->pblock2->SetValue( m_parameterId, 0, const_cast<TCHAR*>( aNewPath.GetCStr() ) );
    }
#endif

#if MAX_VERSION_MAJOR >= 12
    MaxSDK::AssetManagement::AssetType GetAssetType() const { return MaxSDK::AssetManagement::kAnimationAsset; }
#else
    int GetAssetType() const { return IAssetAccessor::kAnimationAsset; }
#endif
};

#if MAX_VERSION_MAJOR >= 12
void XMeshLoader::EnumAuxFiles( AssetEnumCallback& nameEnum, DWORD flags ) {
#else
void XMeshLoader::EnumAuxFiles( NameEnumCallback& nameEnum, DWORD flags ) {
#endif
    // TODO: how should this work with autogenProxyPath?
    // what I'm doing now is almost certainly wrong
    if( ( flags & FILE_ENUM_CHECK_AWORK1 ) && TestAFlag( A_WORK1 ) )
        return;

    const bool onRenderNode = frantic::max3d::is_network_render_server() != 0;
    const bool ignoreMissingViewport = m_ignoreMissingViewOnRenderNode.at_time( 0 ) != 0;

    const bool ignoreMissingRenderFile =
        onRenderNode && ignoreMissingViewport && get_render_sequence_id() != SEQUENCE_ID::RENDER;
    const bool ignoreMissingProxyFile =
        onRenderNode && ignoreMissingViewport && get_render_sequence_id() != SEQUENCE_ID::PROXY;

    if( flags & FILE_ENUM_ACCESSOR_INTERFACE ) {
        XMeshLoaderAssetAccessor renderAccessor( this, pb_renderSequence );
#if MAX_VERSION_MAJOR >= 12
        if( renderAccessor.GetAsset().GetId() != MaxSDK::AssetManagement::kInvalidId ) {
            const frantic::tstring path = renderAccessor.GetAsset().GetFullFilePath().data();
#else
        if( !renderAccessor.GetPath().IsEmpty() ) {
            const frantic::tstring path = renderAccessor.GetPath().GetString().data();
#endif
            if( !ignoreMissingRenderFile || frantic::files::file_exists( path ) ) {
                IEnumAuxAssetsCallback* callback = static_cast<IEnumAuxAssetsCallback*>( &nameEnum );
                callback->DeclareAsset( renderAccessor );
            }
        }

        if( !m_autogenProxyPath.at_time( 0 ) ) {
            XMeshLoaderAssetAccessor proxyAccessor( this, pb_proxySequence );
#if MAX_VERSION_MAJOR >= 12
            if( proxyAccessor.GetAsset().GetId() != MaxSDK::AssetManagement::kInvalidId ) {
                const frantic::tstring path = proxyAccessor.GetAsset().GetFullFilePath().data();
#else
            if( !proxyAccessor.GetPath().IsEmpty() ) {
                const frantic::tstring path = proxyAccessor.GetPath().GetString().data();
#endif
                if( !ignoreMissingProxyFile || frantic::files::file_exists( path ) ) {
                    IEnumAuxAssetsCallback* callback = static_cast<IEnumAuxAssetsCallback*>( &nameEnum );
                    callback->DeclareAsset( proxyAccessor );
                }
            }
        }
    } else {
#if MAX_VERSION_MAJOR >= 12
        if( pblock2->GetAssetUser( pb_renderSequence ).GetId() != MaxSDK::AssetManagement::kInvalidId ) {
            const frantic::tstring path = pblock2->GetAssetUser( pb_renderSequence ).GetFullFilePath().data();
            if( !ignoreMissingRenderFile || frantic::files::file_exists( path ) ) {
                IPathConfigMgr::GetPathConfigMgr()->RecordInputAsset( pblock2->GetAssetUser( pb_renderSequence ),
                                                                      nameEnum, flags );
            }
        }
        if( !m_autogenProxyPath.at_time( 0 ) ) {
            if( pblock2->GetAssetUser( pb_proxySequence ).GetId() != MaxSDK::AssetManagement::kInvalidId ) {
                const frantic::tstring path = pblock2->GetAssetUser( pb_proxySequence ).GetFullFilePath().data();
                if( !ignoreMissingProxyFile || frantic::files::file_exists( path ) ) {
                    IPathConfigMgr::GetPathConfigMgr()->RecordInputAsset( pblock2->GetAssetUser( pb_proxySequence ),
                                                                          nameEnum, flags );
                }
            }
        }
#else
        TCHAR* renderString = NULL;
        Interval ivalid;
        pblock2->GetValue( pb_renderSequence, 0, renderString, ivalid );
        if( renderString ) {
            MaxSDK::Util::Path renderPath( renderString );
            if( !ignoreMissingRenderFile || frantic::files::file_exists( renderPath.GetString().data() ) ) {
                IPathConfigMgr::GetPathConfigMgr()->RecordInputAsset( renderPath, nameEnum, flags );
            }
        }

        if( !m_autogenProxyPath.at_time( 0 ) ) {
            TCHAR* proxyString = NULL;
            pblock2->GetValue( pb_proxySequence, 0, proxyString, ivalid );
            if( proxyString ) {
                MaxSDK::Util::Path proxyPath( proxyString );
                if( !ignoreMissingProxyFile || frantic::files::file_exists( proxyPath.GetString().data() ) ) {
                    IPathConfigMgr::GetPathConfigMgr()->RecordInputAsset( proxyPath, nameEnum, flags );
                }
            }
        }
#endif

        GeomObject::EnumAuxFiles( nameEnum, flags );
    }
}

#if MAX_VERSION_MAJOR >= 15
#define P_END p_end
#else
#define P_END end
#endif

XMeshLoaderDesc::XMeshLoaderDesc() {
    m_pParamDesc = new ParamBlockDesc2(
        mesh_loader_param_block, _T("Parameters"), NULL, this, P_AUTO_CONSTRUCT | P_AUTO_UI | P_MULTIMAP | P_VERSION,
        // version
        1,
        // param block ref number
        mesh_loader_param_block,
        // rollouts
        param_map_count, main_param_map, IDD_ROLLOUT_MAIN, IDS_ROLLOUT_MAIN, 0, 0,
        GetMainDlgProc(), // AutoUI stuff for panel 0
        help_param_map, IDD_ROLLOUT_HELP, IDS_ROLLOUT_HELP, 0, APPENDROLL_CLOSED, GetHelpDlgProc(), files_param_map,
        IDD_ROLLOUT_FILES, IDS_ROLLOUT_FILES, 0, 0, GetFilesDlgProc(), loading_param_map, IDD_ROLLOUT_LOADING,
        IDS_ROLLOUT_LOADING, 0, 0, GetLoadingDlgProc(), P_END );

    m_pParamDesc->AddParam( pb_showIcon, _T( "showIcon" ), TYPE_BOOL, P_RESET_DEFAULT, IDS_SHOWICON, P_END );
    m_pParamDesc->ParamOption( pb_showIcon, p_default, TRUE, P_END );
    m_pParamDesc->ParamOption( pb_showIcon, p_ui, 0, TYPE_SINGLECHEKBOX, IDC_ICON_SHOW, P_END );
    m_pParamDesc->ParamOption( pb_showIcon, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_iconSize, _T( "iconSize" ), TYPE_WORLD, P_RESET_DEFAULT, IDS_ICONSIZE, P_END );
    m_pParamDesc->ParamOption( pb_iconSize, p_default, 10.f, P_END );
    m_pParamDesc->ParamOption( pb_iconSize, p_range, 0.f, FLT_MAX, P_END );
    m_pParamDesc->ParamOption( pb_iconSize, p_ui, 0, TYPE_SPINNER, EDITTYPE_UNIVERSE, IDC_ICON_SIZE, IDC_ICON_SIZE_SPIN,
                               SPIN_AUTOSCALE, P_END );
    m_pParamDesc->ParamOption( pb_iconSize, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_meshScale, _T( "meshScale" ), TYPE_FLOAT, P_RESET_DEFAULT, IDS_MESHSCALE, P_END );
    m_pParamDesc->ParamOption( pb_meshScale, p_default, 1.f, P_END );
    m_pParamDesc->ParamOption( pb_meshScale, p_range, 0.f, FLT_MAX, P_END );
    m_pParamDesc->ParamOption( pb_meshScale, p_ui, 2, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_CUSTOM_SCALE,
                               IDC_CUSTOM_SCALE_SPIN, SPIN_AUTOSCALE, P_END );
    m_pParamDesc->ParamOption( pb_meshScale, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_keepMeshInMemory, _T( "keepMeshInMemory" ), TYPE_BOOL, P_RESET_DEFAULT,
                            IDS_KEEPMESHINMEMORY, P_END );
    m_pParamDesc->ParamOption( pb_keepMeshInMemory, p_default, TRUE, P_END );
    m_pParamDesc->ParamOption( pb_keepMeshInMemory, p_ui, 0, TYPE_SINGLECHEKBOX, IDC_KEEP_MESH_IN_MEMORY, P_END );
    m_pParamDesc->ParamOption( pb_keepMeshInMemory, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_writeVelocityMapChannel, _T( "writeVelocityMapChannel" ), TYPE_BOOL, P_RESET_DEFAULT,
                            IDS_WRITEVELOCITYMAPCHANNEL, P_END );
    m_pParamDesc->ParamOption( pb_writeVelocityMapChannel, p_default, FALSE, P_END );
    m_pParamDesc->ParamOption( pb_writeVelocityMapChannel, p_ui, 0, TYPE_SINGLECHEKBOX, IDC_WRITE_VELOCITY_MAP_CHANNEL,
                               P_END );
    m_pParamDesc->ParamOption( pb_writeVelocityMapChannel, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_velocityMapChannel, _T( "velocityMapChannel" ), TYPE_INT, P_RESET_DEFAULT,
                            IDS_VELOCITYMAPCHANNEL, P_END );
    m_pParamDesc->ParamOption( pb_velocityMapChannel, p_default, 2, P_END );
    m_pParamDesc->ParamOption( pb_velocityMapChannel, p_range, 0, 99, P_END );
    m_pParamDesc->ParamOption( pb_velocityMapChannel, p_ui, 0, TYPE_SPINNER, EDITTYPE_INT, IDC_VELOCITY_MAP_CHANNEL,
                               IDC_VELOCITY_MAP_CHANNEL_SPIN, 1.f, P_END );
    m_pParamDesc->ParamOption( pb_velocityMapChannel, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_renderSequence, _T( "renderSequence" ), TYPE_FILENAME, P_RESET_DEFAULT,
                            IDS_RENDERSEQUENCE, P_END );
#if MAX_VERSION_MAJOR >= 12
    m_pParamDesc->ParamOption( pb_renderSequence, p_assetTypeID, MaxSDK::AssetManagement::kAnimationAsset, P_END );
#endif
    m_pParamDesc->ParamOption( pb_renderSequence, p_ui, 2, TYPE_EDITBOX, IDC_RENDER_SEQUENCE, P_END );
    m_pParamDesc->ParamOption( pb_renderSequence, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_proxySequence, _T( "proxySequence" ), TYPE_FILENAME, P_RESET_DEFAULT, IDS_PROXYSEQUENCE,
                            P_END );
#if MAX_VERSION_MAJOR >= 12
    m_pParamDesc->ParamOption( pb_proxySequence, p_assetTypeID, MaxSDK::AssetManagement::kAnimationAsset, P_END );
#endif
    m_pParamDesc->ParamOption( pb_proxySequence, p_ui, 2, TYPE_EDITBOX, IDC_PROXY_SEQUENCE, P_END );
    m_pParamDesc->ParamOption( pb_proxySequence, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_autogenProxyPath, _T( "autogenProxyPath" ), TYPE_BOOL, P_RESET_DEFAULT,
                            IDS_AUTOGENPROXYPATH, P_END );
    m_pParamDesc->ParamOption( pb_autogenProxyPath, p_default, FALSE, P_END );
    m_pParamDesc->ParamOption( pb_autogenProxyPath, p_ui, 2, TYPE_SINGLECHEKBOX, IDC_ENABLE_AUTO_PROXY_SEQUENCE,
                               P_END );
    m_pParamDesc->ParamOption( pb_autogenProxyPath, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_loadSingleFrame, _T( "loadSingleFrame" ), TYPE_BOOL, P_RESET_DEFAULT,
                            IDS_LOADSINGLEFRAME, P_END );
    m_pParamDesc->ParamOption( pb_loadSingleFrame, p_default, FALSE, P_END );
    m_pParamDesc->ParamOption( pb_loadSingleFrame, p_ui, 2, TYPE_SINGLECHEKBOX, IDC_LOAD_SINGLE_FRAME, P_END );
    m_pParamDesc->ParamOption( pb_loadSingleFrame, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_frameOffset, _T( "frameOffset" ), TYPE_INT, P_RESET_DEFAULT, IDS_FRAMEOFFSET, P_END );
    m_pParamDesc->ParamOption( pb_frameOffset, p_default, 0, P_END );
    m_pParamDesc->ParamOption( pb_frameOffset, p_range, -100000, 100000, P_END );
    m_pParamDesc->ParamOption( pb_frameOffset, p_ui, 2, TYPE_SPINNER, EDITTYPE_INT, IDC_FRAME_OFFSET,
                               IDC_FRAME_OFFSET_SPIN, 1.f, P_END );
    m_pParamDesc->ParamOption( pb_frameOffset, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_limitToRange, _T( "limitToRange" ), TYPE_BOOL, P_RESET_DEFAULT, IDS_LIMITTORANGE,
                            P_END );
    m_pParamDesc->ParamOption( pb_limitToRange, p_default, FALSE, P_END );
    m_pParamDesc->ParamOption( pb_limitToRange, p_ui, 2, TYPE_SINGLECHEKBOX, IDC_LIMIT_TO_RANGE, P_END );
    m_pParamDesc->ParamOption( pb_limitToRange, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_rangeFirstFrame, _T( "rangeFirstFrame" ), TYPE_INT, P_RESET_DEFAULT, IDS_RANGEFIRSTFRAME,
                            P_END );
    m_pParamDesc->ParamOption( pb_rangeFirstFrame, p_default, 0, P_END );
    m_pParamDesc->ParamOption( pb_rangeFirstFrame, p_range, -100000, 100000, P_END );
    m_pParamDesc->ParamOption( pb_rangeFirstFrame, p_ui, 2, TYPE_SPINNER, EDITTYPE_INT, IDC_FIRST_FRAME,
                               IDC_FIRST_FRAME_SPIN, 1.f, P_END );
    m_pParamDesc->ParamOption( pb_rangeFirstFrame, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_rangeLastFrame, _T( "rangeLastFrame" ), TYPE_INT, P_RESET_DEFAULT, IDS_RANGELASTFRAME,
                            P_END );
    m_pParamDesc->ParamOption( pb_rangeLastFrame, p_default, 100, P_END );
    m_pParamDesc->ParamOption( pb_rangeLastFrame, p_range, -100000, 100000, P_END );
    m_pParamDesc->ParamOption( pb_rangeLastFrame, p_ui, 2, TYPE_SPINNER, EDITTYPE_INT, IDC_LAST_FRAME,
                               IDC_LAST_FRAME_SPIN, 1.f, P_END );
    m_pParamDesc->ParamOption( pb_rangeLastFrame, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_enablePlaybackGraph, _T( "enablePlaybackGraph" ), TYPE_BOOL, P_RESET_DEFAULT,
                            IDS_ENABLEPLAYBACKGRAPH, P_END );
    m_pParamDesc->ParamOption( pb_enablePlaybackGraph, p_default, FALSE, P_END );
    m_pParamDesc->ParamOption( pb_enablePlaybackGraph, p_ui, 2, TYPE_SINGLECHEKBOX, IDC_ENABLE_PLAYBACK_GRAPH, P_END );
    m_pParamDesc->ParamOption( pb_enablePlaybackGraph, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_playbackGraphTime, _T( "playbackGraphTime" ), TYPE_FLOAT, P_RESET_DEFAULT | P_ANIMATABLE,
                            IDS_PLAYBACKGRAPHTIME, P_END );
    m_pParamDesc->ParamOption( pb_playbackGraphTime, p_default, 0.f, P_END );
    m_pParamDesc->ParamOption( pb_playbackGraphTime, p_range, -std::numeric_limits<float>::max(),
                               std::numeric_limits<float>::max(), P_END );
    m_pParamDesc->ParamOption( pb_playbackGraphTime, p_ui, 2, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_PLAYBACK_GRAPH,
                               IDC_PLAYBACK_GRAPH_SPIN, 0.1f, P_END );
    m_pParamDesc->ParamOption( pb_playbackGraphTime, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_beforeRangeBehavior, _T( "beforeRangeBehavior" ), TYPE_INT, P_RESET_DEFAULT,
                            IDS_BEFORERANGEBEHAVIOR, P_END );
    m_pParamDesc->ParamOption( pb_beforeRangeBehavior, p_default, DEFAULT_FRAME_RANGE_CLAMP_MODE, P_END );
    m_pParamDesc->ParamOption( pb_beforeRangeBehavior, p_range, 0, 1, P_END );
    m_pParamDesc->ParamOption( pb_beforeRangeBehavior, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_afterRangeBehavior, _T( "afterRangeBehavior" ), TYPE_INT, P_RESET_DEFAULT,
                            IDS_AFTERRANGEBEHAVIOR, P_END );
    m_pParamDesc->ParamOption( pb_afterRangeBehavior, p_default, DEFAULT_FRAME_RANGE_CLAMP_MODE, P_END );
    m_pParamDesc->ParamOption( pb_afterRangeBehavior, p_range, 0, 1, P_END );
    m_pParamDesc->ParamOption( pb_afterRangeBehavior, p_accessor, GetMeshLoaderPBAccessor(), P_END );
    // pb_fileLengthUnit,
    // pb_fileCustomScale,

    m_pParamDesc->AddParam( pb_loadMode, _T( "loadMode" ), TYPE_INT, P_RESET_DEFAULT, IDS_LOADMODE, P_END );
    m_pParamDesc->ParamOption( pb_loadMode, p_default, DEFAULT_LOAD_MODE, P_END );
    m_pParamDesc->ParamOption( pb_loadMode, p_accessor, GetMeshLoaderPBAccessor(), P_END );
    // m_pParamDesc->ParamOption( pb_loadMode, p_range, 0, 1 );

    m_pParamDesc->AddParam( pb_enableViewportMesh, _T( "enableViewportMesh" ), TYPE_BOOL, P_RESET_DEFAULT,
                            IDS_ENABLEVIEWPORTMESH, P_END );
    m_pParamDesc->ParamOption( pb_enableViewportMesh, p_default, TRUE, P_END );
    m_pParamDesc->ParamOption( pb_enableViewportMesh, p_ui, 3, TYPE_SINGLECHEKBOX, IDC_ENABLE_IN_VIEWPORT, P_END );
    m_pParamDesc->ParamOption( pb_enableViewportMesh, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_enableRenderMesh, _T( "enableRenderMesh" ), TYPE_BOOL, P_RESET_DEFAULT,
                            IDS_ENABLERENDERMESH, P_END );
    m_pParamDesc->ParamOption( pb_enableRenderMesh, p_default, TRUE, P_END );
    m_pParamDesc->ParamOption( pb_enableRenderMesh, p_ui, 3, TYPE_SINGLECHEKBOX, IDC_ENABLE_IN_RENDER, P_END );
    m_pParamDesc->ParamOption( pb_enableRenderMesh, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_renderSequenceID, _T( "renderSequenceID" ), TYPE_INT, P_RESET_DEFAULT,
                            IDS_RENDERSEQUENCEID, P_END );
    m_pParamDesc->ParamOption( pb_renderSequenceID, p_default, DEFAULT_RENDER_SEQUENCE_ID, P_END );
    m_pParamDesc->ParamOption( pb_renderSequenceID, p_range, 0, 1, P_END );
    m_pParamDesc->ParamOption( pb_renderSequenceID, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_viewportSequenceID, _T( "viewportSequenceID" ), TYPE_INT, P_RESET_DEFAULT,
                            IDS_VIEWPORTSEQUENCEID, P_END );
    m_pParamDesc->ParamOption( pb_viewportSequenceID, p_default, DEFAULT_VIEWPORT_SEQUENCE_ID, P_END );
    m_pParamDesc->ParamOption( pb_viewportSequenceID, p_range, 0, 1, P_END );
    m_pParamDesc->ParamOption( pb_viewportSequenceID, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_renderUsingViewportSettings, _T( "" ), TYPE_BOOL, P_RESET_DEFAULT,
                            IDS_RENDERUSINGVIEWPORTSETTINGS, P_END );
    m_pParamDesc->ParamOption( pb_renderUsingViewportSettings, p_default, FALSE, P_END );
    m_pParamDesc->ParamOption( pb_renderUsingViewportSettings, p_range, 0, 1, P_END );
    // m_pParamDesc->ParamOption( pb_renderUsingViewportSettings, p_ui, 3, TYPE_SINGLECHEKBOX,
    // IDC_RENDER_USING_VIEWPORT_SETTINGS, P_END );
    m_pParamDesc->ParamOption( pb_renderUsingViewportSettings, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_displayMode, _T( "displayMode" ), TYPE_INT, P_RESET_DEFAULT, IDS_DISPLAYMODE, P_END );
    m_pParamDesc->ParamOption( pb_displayMode, p_default, DEFAULT_DISPLAY_MODE, P_END );
    m_pParamDesc->ParamOption( pb_displayMode, p_range, 0, ( DISPLAY_MODE::COUNT - 1 ), P_END );
    m_pParamDesc->ParamOption( pb_displayMode, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_displayPercent, _T( "displayPercent" ), TYPE_FLOAT, P_RESET_DEFAULT, IDS_DISPLAYPERCENT,
                            P_END );
    m_pParamDesc->ParamOption( pb_displayPercent, p_default, 5.f, P_END );
    m_pParamDesc->ParamOption( pb_displayPercent, p_range, 0.f, 100.f, P_END );
    m_pParamDesc->ParamOption( pb_displayPercent, p_ui, 3, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_DISPLAY_PERCENT,
                               IDC_DISPLAY_PERCENT_SPIN, SPIN_AUTOSCALE, P_END );
    m_pParamDesc->ParamOption( pb_displayPercent, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_useFileLengthUnit, _T( "useFileLengthUnit" ), TYPE_BOOL, P_RESET_DEFAULT,
                            IDS_USEFILELENGTHUNIT, P_END );
    m_pParamDesc->ParamOption( pb_useFileLengthUnit, p_default, FALSE, P_END );
    m_pParamDesc->ParamOption( pb_useFileLengthUnit, p_accessor, GetMeshLoaderPBAccessor(), P_END );
    m_pParamDesc->ParamOption( pb_useFileLengthUnit, p_ui, 2, TYPE_SINGLECHEKBOX, IDC_USE_FILE_UNITS, P_END );

    m_pParamDesc->AddParam( pb_lengthUnit, _T( "lengthUnit" ), TYPE_INT, P_RESET_DEFAULT, IDS_LENGTHUNIT, P_END );
    m_pParamDesc->ParamOption( pb_lengthUnit, p_default, (int)DEFAULT_LENGTH_UNIT, P_END );
    m_pParamDesc->ParamOption( pb_lengthUnit, p_range, 0, ( LENGTH_UNIT::COUNT - 1 ), P_END );
    m_pParamDesc->ParamOption( pb_lengthUnit, p_accessor, GetMeshLoaderPBAccessor(), P_END );

    m_pParamDesc->AddParam( pb_ignoreMissingViewOnRenderNode, _T( "ignoreMissingViewportSequenceOnRenderNode" ),
                            TYPE_INT, P_RESET_DEFAULT, IDS_IGNOREMISSINGVIEWONRENDERNODE, P_END );
    m_pParamDesc->ParamOption( pb_ignoreMissingViewOnRenderNode, p_default, 0, P_END );
    m_pParamDesc->ParamOption( pb_ignoreMissingViewOnRenderNode, p_accessor, GetMeshLoaderPBAccessor(), P_END );
    m_pParamDesc->ParamOption( pb_ignoreMissingViewOnRenderNode, p_ui, 3, TYPE_SINGLECHEKBOX,
                               IDC_IGNOREMISSINGVIEWPORTSEQUENCEONRENDERNODE, P_END );
}

XMeshLoaderDesc::~XMeshLoaderDesc() {
    delete m_pParamDesc;
    m_pParamDesc = 0;
}

ClassDesc* GetXMeshLoaderClassDesc() {
    static XMeshLoaderDesc theDesc;
    return &theDesc;
}

static void notify_render_preeval( void* param, NotifyInfo* info ) {
    XMeshLoader* pMeshLoader = (XMeshLoader*)param;
    TimeValue* pTime = (TimeValue*)info->callParam;
    if( pMeshLoader && pTime ) {
        pMeshLoader->SetRenderTime( *pTime );
        pMeshLoader->SetEmptyValidityAndNotifyDependents();
    }
}

static void notify_post_renderframe( void* param, NotifyInfo* info ) {
    XMeshLoader* pMeshLoader = (XMeshLoader*)param;
    RenderGlobalContext* pContext = (RenderGlobalContext*)info->callParam;
    if( pMeshLoader && pContext ) {
        pMeshLoader->ClearRenderTime();
    }
}

std::vector<int> XMeshLoader::m_beforeRangeBehaviorDisplayCodes =
    list_of( FRAME_RANGE_CLAMP_MODE::HOLD )( FRAME_RANGE_CLAMP_MODE::BLANK );
std::vector<int> XMeshLoader::m_afterRangeBehaviorDisplayCodes =
    list_of( FRAME_RANGE_CLAMP_MODE::HOLD )( FRAME_RANGE_CLAMP_MODE::BLANK );
std::vector<int> XMeshLoader::m_loadModeDisplayCodes =
    list_of( LOAD_MODE::FRAME_VELOCITY_OFFSET )( LOAD_MODE::NEAREST_FRAME_VELOCITY_OFFSET )(
        LOAD_MODE::SUBFRAME_VELOCITY_OFFSET )( LOAD_MODE::FRAME_INTERPOLATION )( LOAD_MODE::SUBFRAME_INTERPOLATION );
std::vector<int> XMeshLoader::m_renderSequenceIDDisplayCodes = list_of( SEQUENCE_ID::RENDER )( SEQUENCE_ID::PROXY );
std::vector<int> XMeshLoader::m_viewportSequenceIDDisplayCodes = list_of( SEQUENCE_ID::RENDER )( SEQUENCE_ID::PROXY );
std::vector<int> XMeshLoader::m_displayModeDisplayCodes =
    list_of( DISPLAY_MODE::MESH )( DISPLAY_MODE::BOX )( DISPLAY_MODE::VERTICES )( DISPLAY_MODE::FACES );
std::vector<int> XMeshLoader::m_lengthUnitDisplayCodes = list_of( LENGTH_UNIT::GENERIC )( LENGTH_UNIT::INCHES )(
    LENGTH_UNIT::FEET )( LENGTH_UNIT::MILES )( LENGTH_UNIT::MILLIMETERS )( LENGTH_UNIT::CENTIMETERS )(
    LENGTH_UNIT::METERS )( LENGTH_UNIT::KILOMETERS )( LENGTH_UNIT::CUSTOM );

std::map<int, frantic::tstring> XMeshLoader::m_beforeRangeBehaviorNames;
std::map<int, frantic::tstring> XMeshLoader::m_afterRangeBehaviorNames;
std::map<int, frantic::tstring> XMeshLoader::m_loadModeNames;
std::map<int, frantic::tstring> XMeshLoader::m_renderSequenceIDNames;
std::map<int, frantic::tstring> XMeshLoader::m_viewportSequenceIDNames;
std::map<int, frantic::tstring> XMeshLoader::m_displayModeNames;
std::map<int, frantic::tstring> XMeshLoader::m_lengthUnitNames;

const std::vector<int>& XMeshLoader::get_before_range_behavior_codes() { return m_beforeRangeBehaviorDisplayCodes; }

const std::vector<int>& XMeshLoader::get_after_range_behavior_codes() { return m_afterRangeBehaviorDisplayCodes; }

const std::vector<int>& XMeshLoader::get_load_mode_codes() { return m_loadModeDisplayCodes; }

const std::vector<int>& XMeshLoader::get_render_sequence_id_codes() { return m_renderSequenceIDDisplayCodes; }

const std::vector<int>& XMeshLoader::get_viewport_sequence_id_codes() { return m_viewportSequenceIDDisplayCodes; }

const std::vector<int>& XMeshLoader::get_display_mode_codes() { return m_displayModeDisplayCodes; }

const std::vector<int>& XMeshLoader::get_length_unit_codes() { return m_lengthUnitDisplayCodes; }

frantic::tstring get_code_name( const std::map<int, frantic::tstring>& codeToNameMap, int code ) {
    std::map<int, frantic::tstring>::const_iterator i = codeToNameMap.find( code );
    if( i == codeToNameMap.end() ) {
        return _T("<unknown:") + boost::lexical_cast<frantic::tstring>( code ) + _T(">");
    } else {
        return i->second;
    }
}

frantic::tstring XMeshLoader::get_before_range_behavior_name( int code ) {
    return get_code_name( m_beforeRangeBehaviorNames, code );
}

frantic::tstring XMeshLoader::get_after_range_behavior_name( int code ) {
    return get_code_name( m_afterRangeBehaviorNames, code );
}

frantic::tstring XMeshLoader::get_load_mode_name( int code ) { return get_code_name( m_loadModeNames, code ); }

frantic::tstring XMeshLoader::get_render_sequence_id_name( int code ) {
    return get_code_name( m_renderSequenceIDNames, code );
}

frantic::tstring XMeshLoader::get_viewport_sequence_id_name( int code ) {
    return get_code_name( m_viewportSequenceIDNames, code );
}

frantic::tstring XMeshLoader::get_display_mode_name( int code ) { return get_code_name( m_displayModeNames, code ); }

frantic::tstring XMeshLoader::get_length_unit_name( int code ) { return get_code_name( m_lengthUnitNames, code ); }

int XMeshLoader::get_before_range_behavior() { return m_beforeRangeMode.at_time( 0 ); }
int XMeshLoader::get_after_range_behavior() { return m_afterRangeMode.at_time( 0 ); }
int XMeshLoader::get_load_mode() { return m_loadMode.at_time( 0 ); }
int XMeshLoader::get_control_render_sequence_id() { return m_renderSequenceID.at_time( 0 ); }
int XMeshLoader::get_control_viewport_sequence_id() { return m_viewportSequenceID.at_time( 0 ); }
int XMeshLoader::get_display_mode() { return m_displayMode.at_time( 0 ); }
int XMeshLoader::get_length_unit() { return m_lengthUnit.at_time( 0 ); }

void XMeshLoader::set_before_range_behavior( int val ) { m_beforeRangeMode.at_time( 0 ) = val; }
void XMeshLoader::set_after_range_behavior( int val ) { m_afterRangeMode.at_time( 0 ) = val; }
void XMeshLoader::set_load_mode( int val ) { m_loadMode.at_time( 0 ) = val; }
void XMeshLoader::set_control_render_sequence_id( int val ) { m_renderSequenceID.at_time( 0 ) = val; }
void XMeshLoader::set_control_viewport_sequence_id( int val ) { m_viewportSequenceID.at_time( 0 ) = val; }
void XMeshLoader::set_display_mode( int val ) { m_displayMode.at_time( 0 ) = val; }
void XMeshLoader::set_length_unit( int val ) { m_lengthUnit.at_time( 0 ) = val; }

XMeshLoader::StaticInitializer::StaticInitializer() {
    m_beforeRangeBehaviorNames.clear();
    m_beforeRangeBehaviorNames[FRAME_RANGE_CLAMP_MODE::HOLD] = FRAME_RANGE_CLAMP_MODE_HOLD_BEFORE;
    m_beforeRangeBehaviorNames[FRAME_RANGE_CLAMP_MODE::BLANK] = FRAME_RANGE_CLAMP_MODE_BLANK;

    m_afterRangeBehaviorNames.clear();
    m_afterRangeBehaviorNames[FRAME_RANGE_CLAMP_MODE::HOLD] = FRAME_RANGE_CLAMP_MODE_HOLD_AFTER;
    m_afterRangeBehaviorNames[FRAME_RANGE_CLAMP_MODE::BLANK] = FRAME_RANGE_CLAMP_MODE_BLANK;

    m_loadModeNames.clear();
    m_loadModeNames[LOAD_MODE::FRAME_VELOCITY_OFFSET] = LOAD_MODE_FRAME_VELOCITY_OFFSET;
    m_loadModeNames[LOAD_MODE::NEAREST_FRAME_VELOCITY_OFFSET] = LOAD_MODE_NEAREST_FRAME_VELOCITY_OFFSET;
    m_loadModeNames[LOAD_MODE::SUBFRAME_VELOCITY_OFFSET] = LOAD_MODE_SUBFRAME_VELOCITY_OFFSET;
    m_loadModeNames[LOAD_MODE::FRAME_INTERPOLATION] = LOAD_MODE_FRAME_INTERPOLATION;
    m_loadModeNames[LOAD_MODE::SUBFRAME_INTERPOLATION] = LOAD_MODE_SUBFRAME_INTERPOLATION;

    m_renderSequenceIDNames.clear();
    m_renderSequenceIDNames[SEQUENCE_ID::RENDER] = SEQUENCE_ID_RENDER;
    m_renderSequenceIDNames[SEQUENCE_ID::PROXY] = SEQUENCE_ID_PROXY;

    m_viewportSequenceIDNames.clear();
    m_viewportSequenceIDNames[SEQUENCE_ID::RENDER] = SEQUENCE_ID_RENDER;
    m_viewportSequenceIDNames[SEQUENCE_ID::PROXY] = SEQUENCE_ID_PROXY;

    m_displayModeNames.clear();
    m_displayModeNames[DISPLAY_MODE::MESH] = DISPLAY_MODE_MESH;
    m_displayModeNames[DISPLAY_MODE::BOX] = DISPLAY_MODE_BOX;
    m_displayModeNames[DISPLAY_MODE::VERTICES] = DISPLAY_MODE_VERTICES;
    m_displayModeNames[DISPLAY_MODE::FACES] = DISPLAY_MODE_FACES;

    m_lengthUnitNames.clear();
    m_lengthUnitNames[LENGTH_UNIT::GENERIC] = LENGTH_UNIT_GENERIC;
    m_lengthUnitNames[LENGTH_UNIT::INCHES] = LENGTH_UNIT_INCHES;
    m_lengthUnitNames[LENGTH_UNIT::FEET] = LENGTH_UNIT_FEET;
    m_lengthUnitNames[LENGTH_UNIT::MILES] = LENGTH_UNIT_MILES;
    m_lengthUnitNames[LENGTH_UNIT::MILLIMETERS] = LENGTH_UNIT_MILLIMETERS;
    m_lengthUnitNames[LENGTH_UNIT::CENTIMETERS] = LENGTH_UNIT_CENTIMETERS;
    m_lengthUnitNames[LENGTH_UNIT::METERS] = LENGTH_UNIT_METERS;
    m_lengthUnitNames[LENGTH_UNIT::KILOMETERS] = LENGTH_UNIT_KILOMETERS;
    m_lengthUnitNames[LENGTH_UNIT::CUSTOM] = LENGTH_UNIT_CUSTOM;
}

XMeshLoader::StaticInitializer XMeshLoader::m_staticInitializer;

// --- Required Implementations ------------------------------

// Constructor
#pragma warning( push, 3 )
#pragma warning( disable : 4355 ) // 'this' : used in base member initializer list
XMeshLoader::XMeshLoader()
    : pblock2( 0 )
    , m_cachedPolymesh3( 0 )
    , m_showIcon( this, pb_showIcon )
    , m_iconSize( this, pb_iconSize )
    , m_meshScale( this, pb_meshScale )
    ,

    m_keepMeshInMemory( this, pb_keepMeshInMemory )
    ,

    m_outputVelocityMapChannel( this, pb_velocityMapChannel )
    , m_velToMapChannel( this, pb_writeVelocityMapChannel )
    ,

    m_renderPath( this, pb_renderSequence )
    , m_proxyPath( this, pb_proxySequence )
    , m_autogenProxyPath( this, pb_autogenProxyPath )
    ,

    m_loadSingleFrame( this, pb_loadSingleFrame )
    , m_frameOffset( this, pb_frameOffset )
    , m_limitToRange( this, pb_limitToRange )
    , m_startFrame( this, pb_rangeFirstFrame )
    , m_endFrame( this, pb_rangeLastFrame )
    , m_enablePlaybackGraph( this, pb_enablePlaybackGraph )
    , m_playbackGraphTime( this, pb_playbackGraphTime )
    , m_beforeRangeMode( this, pb_beforeRangeBehavior )
    , m_afterRangeMode( this, pb_afterRangeBehavior )
    ,

    m_loadMode( this, pb_loadMode )
    ,

    m_enableViewportMesh( this, pb_enableViewportMesh )
    , m_enableRenderMesh( this, pb_enableRenderMesh )
    , m_renderSequenceID( this, pb_renderSequenceID )
    , m_viewportSequenceID( this, pb_viewportSequenceID )
    , m_renderUsingViewportSettings( false )
    , m_displayMode( this, pb_displayMode )
    , m_displayPercent( this, pb_displayPercent )
    ,

    m_useFileLengthUnit( this, pb_useFileLengthUnit )
    , m_lengthUnit( this, pb_lengthUnit )
    ,

    m_ignoreMissingViewOnRenderNode( this, pb_ignoreMissingViewOnRenderNode )
    ,

    m_inRenderingMode( false )
    , m_renderTime( TIME_NegInfinity )
    , m_cachedMeshInRenderingMode( false )
    , m_cachedLoadingMode( std::numeric_limits<int>::max() )
    ,

    m_cachedPolymesh3IsTriangles( true )
    , m_doneBuildNormals( false ) {
    ivalid.SetEmpty();
    GetXMeshLoaderClassDesc()->MakeAutoParamBlocks( this );
    InitializeFPDescriptor();

    m_fileSequences.resize( SEQUENCE_ID::COUNT );
    m_fileSequencePaths.resize( SEQUENCE_ID::COUNT );

    if( frantic::max3d::is_network_render_server() ) {
        m_polymesh3Loader.set_thread_count( 1 );
    } else {
        const int maxThreads = 4;
        m_polymesh3Loader.set_thread_count( std::max<std::size_t>(
            1, static_cast<std::size_t>( std::min<int>( maxThreads, boost::thread::hardware_concurrency() ) ) ) );
    }

    invalidate_cache();

    RegisterNotification( notify_render_preeval, (void*)this, NOTIFY_RENDER_PREEVAL );
    RegisterNotification( notify_post_renderframe, (void*)this, NOTIFY_POST_RENDERFRAME );
}
#pragma warning( pop )

XMeshLoader::~XMeshLoader() {
    UnRegisterNotification( notify_post_renderframe, (void*)this, NOTIFY_POST_RENDERFRAME );
    UnRegisterNotification( notify_render_preeval, (void*)this, NOTIFY_RENDER_PREEVAL );

    DeleteAllRefsFromMe();
};

// Animatable methods
void XMeshLoader::DeleteThis() { delete this; }
Class_ID XMeshLoader::ClassID() { return XMeshLoader_CLASS_ID; }
#if MAX_VERSION_MAJOR >= 24
void XMeshLoader::GetClassName( MSTR& s, bool localized ) {
#else
void XMeshLoader::GetClassName( MSTR& s ) {
#endif
    s = GetXMeshLoaderClassDesc()->ClassName();
}
int XMeshLoader::NumSubs() { return 1; }
Animatable* XMeshLoader::SubAnim( int i ) { return ( i == 0 ) ? pblock2 : 0; }
#if MAX_VERSION_MAJOR >= 24
MSTR XMeshLoader::SubAnimName( int i, bool localized ) {
#else
MSTR XMeshLoader::SubAnimName( int i ) {
#endif
    return ( i == 0 ) ? pblock2->GetLocalName() : _T("");
}
int XMeshLoader::NumParamBlocks() { return 1; }
IParamBlock2* XMeshLoader::GetParamBlock( int i ) {
    switch( i ) {
    case 0:
        return pblock2;
    default:
        return 0;
    }
}
IParamBlock2* XMeshLoader::GetParamBlockByID( BlockID id ) {
    if( id == pblock2->ID() ) {
        return pblock2;
    } else {
        return 0;
    }
}
void XMeshLoader::BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev ) {
    editObj = this;
    GetMainDlgProc()->set_obj( this );
    GetHelpDlgProc()->set_obj( this );
    GetFilesDlgProc()->set_obj( this );
    GetLoadingDlgProc()->set_obj( this );
    GetXMeshLoaderClassDesc()->BeginEditParams( ip, this, flags, prev );
}
void XMeshLoader::EndEditParams( IObjParam* ip, ULONG flags, Animatable* next ) {
    GetXMeshLoaderClassDesc()->EndEditParams( ip, this, flags, next );
    GetLoadingDlgProc()->set_obj( 0 );
    GetFilesDlgProc()->set_obj( 0 );
    GetHelpDlgProc()->set_obj( 0 );
    GetMainDlgProc()->set_obj( 0 );
    editObj = 0;
}
ReferenceTarget* XMeshLoader::Clone( RemapDir& remap ) {
    XMeshLoader* oldObj = this;
    XMeshLoader* newObj = new XMeshLoader();
    for( int i = 0, iEnd = newObj->NumRefs(); i < iEnd; ++i ) {
        newObj->ReplaceReference( i, remap.CloneRef( oldObj->GetReference( i ) ) );
    }
    oldObj->BaseClone( oldObj, newObj, remap );
    return newObj;
}
void XMeshLoader::FreeCaches() {
    ivalid.SetEmpty();
    clear_cache();
}

int XMeshLoader::RenderBegin( TimeValue /*t*/, ULONG flags ) {
    try {
        // Only switch to rendering mode if it's not in the material editor
        if( ( flags & RENDERBEGIN_IN_MEDIT ) == 0 ) {
            m_inRenderingMode = true;
            ivalid = NEVER;
            invalidate();
        }
    } catch( const std::exception& e ) {
        const frantic::tstring errmsg = _T("RenderBegin: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
    }
    return 1;
}

int XMeshLoader::RenderEnd( TimeValue /*t*/ ) {
    m_inRenderingMode = false;
    invalidate();
    return 1;
}

BaseInterface* XMeshLoader::GetInterface( Interface_ID id ) {
    if( id == XMeshLoader_INTERFACE_ID ) {
        return static_cast<frantic::max3d::fpwrapper::FFMixinInterface<XMeshLoader>*>( this );
    } else {
        return GeomObject::GetInterface( id );
    }
}

// From ReferenceMaker
int XMeshLoader::NumRefs() { return 1; }

RefTargetHandle XMeshLoader::GetReference( int i ) { // return (RefTargetHandle)pblock;}
    if( i == 0 ) {
        return pblock2;
    } else {
        return NULL;
    }
}

void XMeshLoader::SetReference( int i, RefTargetHandle rtarg ) {
    if( i == 0 ) {
        pblock2 = static_cast<IParamBlock2*>( rtarg );
    }
}

IOResult XMeshLoader::Load( ILoad* iload ) {
    iload->RegisterPostLoadCallback( new XMeshLoaderPLCB( this ) );
    return IO_OK;
}

#if MAX_VERSION_MAJOR >= 17
RefResult XMeshLoader::NotifyRefChanged( const Interval& /*changeInt*/, RefTargetHandle /*hTarget*/, PartID& partID,
                                         RefMessage message, BOOL /*propagate*/ ) {
#else
RefResult XMeshLoader::NotifyRefChanged( Interval /*changeInt*/, RefTargetHandle /*hTarget*/, PartID& partID,
                                         RefMessage message ) {
#endif
    switch( message ) {
    case REFMSG_CHANGE:
        ivalid.SetEmpty();
        // int tabIndex = -1;
        // ParamID changingParam = pblock2->LastNotifyParamID( tabIndex );
        if( editObj == this ) {
            reinterpret_cast<ClassDesc2*>( GetXMeshLoaderClassDesc() )->InvalidateUI();
        }
        return ( REF_SUCCEED );
    case REFMSG_GET_PARAM_DIM: {
        GetParamDim* gpd = (GetParamDim*)partID;
        if( pblock2 && gpd->index < pblock2->NumParams() ) {
            gpd->dim = pblock2->GetParamDef( static_cast<ParamID>( gpd->index ) ).dim;
        } else {
            gpd->dim = defaultDim; // GetParameterDim( gpd->index );
        }
    }
#if MAX_VERSION_MAJOR >= 12
        return REF_HALT;
#else
        return REF_STOP;
#endif
    // TODO : I think we shouldn't need this, but parameters don't appear without it.
    case REFMSG_GET_PARAM_NAME: {
        GetParamName* gpd = (GetParamName*)partID;
        TCHAR* s = 0;
        if( pblock2 && gpd->index < pblock2->NumParams() ) {
            const StringResID stringID = pblock2->GetParamDef( static_cast<ParamID>( gpd->index ) ).local_name;
            if( stringID ) {
                s = GetString( static_cast<UINT>( stringID ) );
            }
        }
        if( s ) {
            gpd->name = TSTR( s );
        } else {
            gpd->name = TSTR( _T( "" ) );
        }
    }
#if MAX_VERSION_MAJOR >= 12
        return REF_HALT;
#else
        return REF_STOP;
#endif
    }
    return ( REF_SUCCEED );
    /*
    switch( message ) {
            case REFMSG_CHANGE:
                    if( hTarget == pblock2 ) {
                            int tabIndex = -1;
                            ParamID changingParam = pblock2->LastNotifyParamID( tabIndex );
                            if( editObj == this ) {
                                    reinterpret_cast<ClassDesc2*>( GetXMeshLoaderClassDesc() )->InvalidateUI();
                            }
                    }
                    break;
    }
    return REF_SUCCEED;
    */
}

// int XMeshLoader::NumInterfaces() { return 1; }
/*
FPInterface* XMeshLoader::GetInterface( int i ) {
        if( i == 0 ) {
                return GetInterface( XMeshLoader_INTERFACE_ID );
        } else {
                return SimpleObject2::GetInterface( i );
        }
}
*/

CreateMouseCallBack* XMeshLoader::GetCreateMouseCallBack() {
    g_createMouseCB.SetObj( this );
    return &g_createMouseCB;
}

// Display, GetWorldBoundBox, GetLocalBoundBox are virtual methods of SimpleObject2 that must be implemented.
// HitTest is optional.
int XMeshLoader::Display( TimeValue time, INode* inode, ViewExp* pView, int flags ) {
    try {
        if( !inode || !pView )
            return 0;

        if( inode->IsNodeHidden() ) {
            return TRUE;
        }

        UpdateMesh( time );

        GraphicsWindow* gw = pView->getGW();
        const DWORD rndLimits = gw->getRndLimits();

        // gw->setRndLimits( GW_Z_BUFFER|GW_WIREFRAME );
        Matrix3 tm = inode->GetNodeTM( time );
        tm.PreTranslate( inode->GetObjOffsetPos() );
        PreRotateMatrix( tm, inode->GetObjOffsetRot() );
        ApplyScaling( tm, inode->GetObjOffsetScale() );
        gw->setTransform( tm );

        const int displayMode = m_displayMode.at_time( time );

        if( displayMode == DISPLAY_MODE::BOX ) {
            const color3f fillColor = color3f::from_RGBA( inode->GetWireColor() );
            const color3f lineColor = inode->Selected() ? color3f( &from_max_t( GetSelColor() )[0] ) : fillColor;
            gw->setColor( LINE_COLOR, lineColor.r, lineColor.g, lineColor.b );
            frantic::max3d::DrawBox( gw, frantic::max3d::to_max_t( m_meshBoundingBox ) );
        } else {
            // Let the SimpleObject2 display code do its thing
#if MAX_VERSION_MAJOR >= 14
            if( !MaxSDK::Graphics::IsRetainedModeEnabled() ||
                MaxSDK::Graphics::IsRetainedModeEnabled() && displayMode == DISPLAY_MODE::VERTICES ) {
#else
            {
#endif
                if( displayMode == DISPLAY_MODE::VERTICES ) {
                    gw->setRndLimits( GW_Z_BUFFER | GW_VERT_TICKS );
                } else {
                    build_normals();
                }
                Rect damageRect;
                if( flags & USE_DAMAGE_RECT )
                    damageRect = pView->GetDammageRect();
                if( has_triangle_mesh() ) {
                    mesh.render( gw, inode->Mtls(), ( flags & USE_DAMAGE_RECT ) ? &damageRect : NULL, COMP_ALL,
                                 inode->NumMtls() );
                } else {
                    mm.render( gw, inode->Mtls(), ( flags & USE_DAMAGE_RECT ) ? &damageRect : NULL, COMP_ALL,
                               inode->NumMtls() );
                }
            }
        }

        gw->setRndLimits( GW_Z_BUFFER | GW_WIREFRAME );

        // Render the icon if necessary
        if( m_showIcon.at_time( time ) && !( gw->getRndMode() & GW_BOX_MODE ) && !inode->GetBoxMode() ) {
            Matrix3 iconMatrix = inode->GetNodeTM( time );
            float f = m_iconSize.at_time( time );
            iconMatrix.Scale( Point3( f, f, f ) );

            gw->setTransform( iconMatrix );

            // This wireframe drawing function is easier to use, because we don't have to mess with the material and
            // stuff.
            gw->setRndLimits( GW_WIREFRAME | ( GW_Z_BUFFER & rndLimits ) );
            max3d::draw_mesh_wireframe(
                gw, m_pIconMesh.get(), inode->Selected() ? color3f( 1 ) : color3f::from_RGBA( inode->GetWireColor() ) );
        }

        gw->setRndLimits( rndLimits );

        return TRUE;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T("Display: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );

        return 0;
    }
}

bool is_hardware_hit_testing( ViewExp* vpt ) {
#if MAX_VERSION_MAJOR >= 15
    return MaxSDK::Graphics::IsHardwareHitTesting( vpt );
#else
    UNREFERENCED_PARAMETER( vpt );
    return false;
#endif
}

int XMeshLoader::HitTest( TimeValue time, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt ) {
    try {
        GraphicsWindow* gw = vpt->getGW();

        if( ( ( flags & HIT_SELONLY ) && !inode->Selected() ) || gw == NULL || inode == NULL )
            return 0;

        const DWORD rndLimits = gw->getRndLimits();

        const int displayMode = m_displayMode.at_time( time );

        HitRegion hitRegion;
        MakeHitRegion( hitRegion, type, crossing, 4, p );
        gw->setHitRegion( &hitRegion );
        gw->clearHitCode();

        if( gw->getRndMode() & GW_BOX_MODE ) {
            gw->setRndLimits( rndLimits & ~( DWORD( GW_ILLUM | GW_FLAT | GW_TEXTURE | GW_CONSTANT ) ) | GW_PICK );

            Matrix3 tm = inode->GetNodeTM( time );
            gw->setTransform( tm );

            Box3 box;
            GetLocalBoundBox( time, inode, vpt, box );
            const int hit = frantic::max3d::hit_test_box( gw, box, hitRegion, flags & HIT_ABORTONHIT );
            gw->setRndLimits( rndLimits );
            return hit;
        }

        if( displayMode == DISPLAY_MODE::BOX ) {
            gw->setRndLimits( rndLimits & ~( DWORD( GW_ILLUM | GW_FLAT | GW_TEXTURE | GW_CONSTANT ) ) | GW_PICK );

            Matrix3 tm = inode->GetNodeTM( time );
            gw->setTransform( tm );

            Box3 box = frantic::max3d::to_max_t( m_meshBoundingBox );
            const int hit = frantic::max3d::hit_test_box( gw, box, hitRegion, flags & HIT_ABORTONHIT );
            if( hit ) {
                gw->setRndLimits( rndLimits );
                return hit;
            }
        } else if( displayMode == DISPLAY_MODE::VERTICES || !is_hardware_hit_testing( vpt ) ) {
            if( displayMode == DISPLAY_MODE::VERTICES ) {
                gw->setRndLimits( GW_VERT_TICKS | GW_Z_BUFFER );
            }

            gw->setTransform( inode->GetObjectTM( time ) );

            if( has_triangle_mesh() ) {
                if( mesh.select( gw, inode->Mtls(), &hitRegion, flags & HIT_ABORTONHIT, inode->NumMtls() ) ) {
                    gw->setRndLimits( rndLimits );
                    return true;
                }
            } else {
                if( mm.select( gw, inode->Mtls(), &hitRegion, flags & HIT_ABORTONHIT, inode->NumMtls() ) ) {
                    gw->setRndLimits( rndLimits );
                    return true;
                }
            }
        }

        // Hit test against the icon if necessary
        if( m_showIcon.at_time( time ) ) {
            gw->setRndLimits( rndLimits & ~( DWORD( GW_ILLUM | GW_FLAT | GW_TEXTURE | GW_CONSTANT ) ) | GW_WIREFRAME |
                              GW_PICK );
            gw->setHitRegion( &hitRegion );
            gw->clearHitCode();

            Matrix3 iconMatrix = inode->GetNodeTM( time );
            float f = m_iconSize.at_time( time );
            iconMatrix.Scale( Point3( f, f, f ) );

            gw->setTransform( iconMatrix );
            if( m_pIconMesh->select( gw, NULL, &hitRegion ) ) {
                gw->setRndLimits( rndLimits );
                return true;
            }
        }

        gw->setRndLimits( rndLimits );
        return false;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T("HitTest: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );

        return false;
    }
}

#if MAX_VERSION_MAJOR >= 17

unsigned long XMeshLoader::GetObjectDisplayRequirement() const {
    return MaxSDK::Graphics::ObjectDisplayRequireLegacyDisplayMode;
}

#elif MAX_VERSION_MAJOR >= 14

bool XMeshLoader::RequiresSupportForLegacyDisplayMode() const { return true; }

#endif

#if MAX_VERSION_MAJOR >= 17

bool XMeshLoader::PrepareDisplay( const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext ) {
    try {
        BuildMesh( prepareDisplayContext.GetDisplayTime() );

        mRenderItemHandles.ClearAllRenderItems();

        mm.InvalidateHardwareMesh( 1 );

        if( m_displayMode.at_time( 0 ) == DISPLAY_MODE::MESH || m_displayMode.at_time( 0 ) == DISPLAY_MODE::FACES ) {
            if( has_triangle_mesh() ) {
                if( mesh.getNumVerts() > 0 ) {
                    MaxSDK::Graphics::GenerateMeshRenderItemsContext generateRenderItemsContext;
                    generateRenderItemsContext.GenerateDefaultContext( prepareDisplayContext );

                    MaxSDK::Graphics::IMeshDisplay2* pMeshDisplay = static_cast<MaxSDK::Graphics::IMeshDisplay2*>(
                        mesh.GetInterface( IMesh_DISPLAY2_INTERFACE_ID ) );
                    if( !pMeshDisplay ) {
                        return false;
                    }
                    pMeshDisplay->PrepareDisplay( generateRenderItemsContext );

                    return true;
                }
            } else {
                return PreparePolyObjectDisplay( mm, prepareDisplayContext );
            }
        }

        return true;

    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T("PrepareDisplay: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }

    return false;
}

bool XMeshLoader::UpdatePerNodeItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                                      MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                                      MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer ) {
    try {
        mm.InvalidateHardwareMesh( 1 );

        if( m_displayMode.at_time( 0 ) == DISPLAY_MODE::MESH || m_displayMode.at_time( 0 ) == DISPLAY_MODE::FACES ) {
            if( has_triangle_mesh() ) {
                if( mesh.getNumVerts() > 0 ) {
                    MaxSDK::Graphics::IMeshDisplay2* pMeshDisplay = static_cast<MaxSDK::Graphics::IMeshDisplay2*>(
                        mesh.GetInterface( IMesh_DISPLAY2_INTERFACE_ID ) );
                    if( !pMeshDisplay ) {
                        return false;
                    }

                    MaxSDK::Graphics::GenerateMeshRenderItemsContext generateRenderItemsContext;
                    generateRenderItemsContext.GenerateDefaultContext( updateDisplayContext );
                    generateRenderItemsContext.RemoveInvisibleMeshElementDescriptions( nodeContext.GetRenderNode() );

                    pMeshDisplay->GetRenderItems( generateRenderItemsContext, nodeContext, targetRenderItemContainer );

                    return true;
                }
            } else {
                return UpdatePolyObjectPerNodeItemsDisplay( mm, updateDisplayContext, nodeContext,
                                                            targetRenderItemContainer );
            }
        }
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T("UpdatePerNodeItems: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }

    return false;
}

#elif MAX_VERSION_MAJOR >= 15

bool XMeshLoader::UpdateDisplay( const MaxSDK::Graphics::MaxContext& /*maxContext*/,
                                 const MaxSDK::Graphics::UpdateDisplayContext& displayContext ) {
    try {
        BuildMesh( displayContext.GetDisplayTime() );

        if( m_displayMode.at_time( 0 ) == DISPLAY_MODE::MESH || m_displayMode.at_time( 0 ) == DISPLAY_MODE::FACES ) {
            if( has_triangle_mesh() ) {
                if( mesh.getNumVerts() > 0 ) {
                    MaxSDK::Graphics::GenerateMeshRenderItemsContext generateRenderItemsContext;
                    generateRenderItemsContext.GenerateDefaultContext( displayContext );
                    mesh.GenerateRenderItems( mRenderItemHandles, generateRenderItemsContext );
                    return true;
                }
            } else {
                if( mm.numv > 0 ) {
                    MaxSDK::Graphics::GenerateMeshRenderItemsContext generateRenderItemsContext;
                    generateRenderItemsContext.GenerateDefaultContext( displayContext );
                    mm.GenerateRenderItems( mRenderItemHandles, generateRenderItemsContext );
                    return true;
                }
            }
        }

        // If we did not set a mesh above, then clear the existing mesh
        mRenderItemHandles.ClearAllRenderItems();
        return true;

    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T("UpdateDisplay: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
    return false;
}

#elif MAX_VERSION_MAJOR == 14

bool XMeshLoader::UpdateDisplay( unsigned long renderItemCategories,
                                 const MaxSDK::Graphics::MaterialRequiredStreams& materialRequiredStreams,
                                 TimeValue t ) {
    try {
        BuildMesh( t );

        if( m_displayMode.at_time( 0 ) == DISPLAY_MODE::MESH || m_displayMode.at_time( 0 ) == DISPLAY_MODE::FACES ) {
            if( has_triangle_mesh() ) {
                MaxSDK::Graphics::GenerateRenderItems( mRenderItemHandles, &mesh, renderItemCategories,
                                                       materialRequiredStreams );
            } else {
                MaxSDK::Graphics::GenerateRenderItems( mRenderItemHandles, &mm, renderItemCategories,
                                                       materialRequiredStreams );
            }
        } else {
            mRenderItemHandles.removeAll();
        }

        return true;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T("UpdateDisplay: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
    return false;
}

#endif

// from Object
ObjectState XMeshLoader::Eval( TimeValue t ) {
    UpdateMesh( t );
    return ObjectState( this );
}

Interval XMeshLoader::ObjectValidity( TimeValue /*t*/ ) {
    // UpdateMesh( t );
    return ivalid;
}

void XMeshLoader::InitNodeName( MSTR& s ) { s = _T( XMeshLoader_DISPLAY_NAME ); }

int XMeshLoader::CanConvertToType( Class_ID obtype ) {
    if( obtype == defObjectClassID )
        return TRUE;
    if( obtype == mapObjectClassID )
        return TRUE;
    if( obtype == polyObjectClassID )
        return TRUE;
    if( obtype == triObjectClassID )
        return TRUE;
#ifndef NO_PATCHES
    if( obtype == patchObjectClassID )
        return TRUE;
#endif
    if( Object::CanConvertToType( obtype ) )
        return TRUE;
    if( CanConvertTriObject( obtype ) )
        return TRUE;
    return FALSE;
}

Object* XMeshLoader::ConvertToType( TimeValue t, Class_ID obtype ) {
    // TODO : do we ever need to build normals?
    // I believe we don't for Mesh, because the sdk docs state that operator= does not copy rVerts
    if( obtype == triObjectClassID ) {
        UpdateMesh( t );
        TriObject* triob;
        triob = CreateNewTriObject();
        if( has_triangle_mesh() ) {
            triob->GetMesh() = mesh;
            triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        } else {
            mm.OutToTri( triob->GetMesh() );
            triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        }
        return triob;
    }
    if( obtype == defObjectClassID || obtype == mapObjectClassID ) {
        if( has_triangle_mesh() ) {
            TriObject* triob;
            UpdateMesh( t );
            triob = CreateNewTriObject();
            triob->GetMesh() = mesh;
            triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
            return triob;
        } else {
            PolyObject* polyob;
            polyob = new PolyObject;
            polyob->GetMesh() = mm;
            polyob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            polyob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
            return polyob;
        }
    }
    if( obtype == polyObjectClassID ) {
        PolyObject* polyob;
        UpdateMesh( t );
        if( has_triangle_mesh() ) {
            TriObject* triob;
            UpdateMesh( t );
            triob = CreateNewTriObject();
            triob->GetMesh() = mesh;
            triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
            polyob = static_cast<PolyObject*>( triob->ConvertToType( t, polyObjectClassID ) );
        } else {
            polyob = new PolyObject;
            polyob->GetMesh() = mm;
            polyob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
            polyob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        }
        return polyob;
    }
#ifndef NO_PATCHES
    if( obtype == patchObjectClassID ) {
        UpdateMesh( t );
        PatchObject* patchob = new PatchObject();
        if( has_triangle_mesh() ) {
            patchob->patch = mesh;
        } else {
            Mesh* tempMesh = new class Mesh;
            mm.OutToTri( *tempMesh );
            patchob->patch = *tempMesh;
            delete tempMesh;
        }
        patchob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
        patchob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        return patchob;
    }
#endif
    if( Object::CanConvertToType( obtype ) ) {
        UpdateMesh( t );
        return Object::ConvertToType( t, obtype );
    }
    if( CanConvertTriObject( obtype ) ) {
        UpdateMesh( t );
        TriObject* triob = CreateNewTriObject();
        triob->GetMesh() = mesh;
        triob->SetChannelValidity( TOPO_CHAN_NUM, ObjectValidity( t ) );
        triob->SetChannelValidity( GEOM_CHAN_NUM, ObjectValidity( t ) );
        Object* ob = triob->ConvertToType( t, obtype );
        if( ob != triob ) {
            triob->DeleteThis();
        }
        return ob;
    }
    return NULL;
}

BOOL XMeshLoader::PolygonCount( TimeValue t, int& numFaces, int& numVerts ) {
    try {
        UpdateMesh( t );
        if( has_triangle_mesh() ) {
            numFaces = mesh.numFaces;
            numVerts = mesh.numVerts;
        } else {
            numFaces = mm.numf;
            numVerts = mm.numv;
        }
        return TRUE;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T("PolygonCount: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
    numFaces = 0;
    numVerts = 0;
    return 0;
}

int XMeshLoader::IntersectRay( TimeValue t, Ray& ray, float& at, Point3& norm ) {
    UpdateMesh( t );
    build_normals();
    return mesh.IntersectRay( ray, at, norm );
}

void XMeshLoader::GetWorldBoundBox( TimeValue time, INode* inode, ViewExp* vpt, Box3& box ) {
    GetLocalBoundBox( time, inode, vpt, box );
    box = box * inode->GetObjectTM( time );
}

void XMeshLoader::GetLocalBoundBox( TimeValue t, INode* /*inode*/, ViewExp* /*vpt*/, Box3& box ) {
    try {
        UpdateMesh( t );

        box = frantic::max3d::to_max_t( m_meshBoundingBox );

        if( m_showIcon.at_time( t ) ) {
            // Compute the world-space scaled bounding box
            float scale = m_iconSize.at_time( t );
            Matrix3 iconMatrix( 1 );
            iconMatrix.Scale( Point3( scale, scale, scale ) );
            box += m_pIconMesh->getBoundingBox( &iconMatrix );

            // Box3 iconBox( Point3( -0.5f, -0.5f, 0.f ), Point3( 0.5f, 0.5f, 0.f ) );
            // iconBox.Scale( scale );
            // box += iconBox;
        }

    } catch( const std::exception& e ) {
        frantic::tstring errmsg = _T("GetLocalBoundBox: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

void XMeshLoader::GetDeformBBox( TimeValue t, Box3& box, Matrix3* tm, BOOL /*useSel*/ ) {
    UpdateMesh( t );
    box.Init();
    if( tm ) {
        bool done = false;

        if( has_triangle_mesh() ) {
            if( mesh.numVerts > 0 ) {
                box = mesh.getBoundingBox( tm );
                done = true;
            }
        } else {
            if( mm.numv > 0 ) {
                box = mm.getBoundingBox( tm );
                done = true;
            }
        }

        if( !done && !m_meshBoundingBox.is_empty() ) {
            for( int i = 0; i < 8; ++i ) {
                box += frantic::max3d::to_max_t( m_meshBoundingBox.get_corner( i ) ) * ( *tm );
            }
        }
    } else {
        if( !m_meshBoundingBox.is_empty() ) {
            box = frantic::max3d::to_max_t( m_meshBoundingBox );
        }
    }
}

Mesh* XMeshLoader::GetRenderMesh( TimeValue t, INode* /*inode*/, View& /*view*/, BOOL& needDelete ) {
    UpdateMesh( t );
    if( has_triangle_mesh() ) {
        needDelete = FALSE;
        return &mesh;
    } else {
        needDelete = TRUE;
        Mesh* result = new class Mesh;
        mm.OutToTri( *result );
        return result;
    }
}

//--- MeshLoader methods -------------------------------

void XMeshLoader::UpdateMesh( TimeValue t ) {
    if( !ivalid.InInterval( t ) ) {
        BuildMesh( t );
    }
}

inline bool is_translation_only( const frantic::graphics::transform4f& xformIn ) {
    frantic::graphics::transform4f xform( xformIn );
    xform.set_translation( frantic::graphics::vector3f( 0, 0, 0 ) );
    return xform.is_identity();
}

inline vertex_channel_source::ptr_type
get_transformed( vertex_channel_source::ptr_type source, const frantic::graphics::transform4f& xform,
                 frantic::geometry::xmesh_metadata::transform_type_t transformType ) {
    vertex_channel_source::ptr_type result = source;
    switch( transformType ) {
    case frantic::geometry::xmesh_metadata::transform_type_point:
        if( !xform.is_identity() ) {
            result.reset( new transform_point_vertex_channel_source( source, xform ) );
        }
        break;
    case frantic::geometry::xmesh_metadata::transform_type_vector:
        if( !is_translation_only( xform ) ) {
            result.reset( new transform_vector_vertex_channel_source( source, xform ) );
        }
        break;
    case frantic::geometry::xmesh_metadata::transform_type_normal:
        if( !is_translation_only( xform ) ) {
            result.reset( new transform_normal_vertex_channel_source( source, xform ) );
        }
        break;
    }
    return result;
}

void XMeshLoader::build_channel_assignment( mesh_channel_assignment& channels, bool useVelocity, float timeOffset,
                                            float timeDerivative, const frantic::graphics::transform4f& xform ) {
    const bool hasXForm = !xform.is_identity();

    const float scale = static_cast<float>( calculate_mesh_scale_factor() );

    const int displayMode =
        get_current_display_mode(); // m_inRenderingMode ? (DISPLAY_MODE::MESH) : (int)(m_displayMode.at_time( t ));

    const double displayFraction =
        frantic::math::clamp<double>( static_cast<double>( m_displayPercent.at_time( 0 ) ) / 100.f, 0.0, 1.0 );

    vertex_channel_source::ptr_type verts( new accessor_vertex_channel_source(
        m_cachedPolymesh3->get_const_vertex_accessor<frantic::graphics::vector3f>( _T("verts") ) ) );
    if( displayMode == DISPLAY_MODE::VERTICES && displayFraction < 1.0 ) {
        verts.reset( new fractional_vertex_channel_source( verts, displayFraction ) );
    }
    if( scale != 1.f ) {
        verts.reset( new scaled_vertex_channel_source( verts, scale ) );
    }

    vertex_channel_source::ptr_type velocity;
    if( useVelocity ) {
        if( m_cachedPolymesh3->has_vertex_channel( _T("Velocity") ) &&
            ( timeOffset != 0 || m_velToMapChannel.at_time( 0 ) ) ) {
            const polymesh3_channel& velocityCh = m_cachedPolymesh3->get_channel_info( _T("Velocity") );
            if( velocityCh.type() == frantic::channels::data_type_float32 ) {
                velocity.reset( new accessor_vertex_channel_source(
                    m_cachedPolymesh3->get_const_vertex_accessor<frantic::graphics::vector3f>( _T("Velocity") ) ) );
            } else {
                velocity.reset( new cvt_accessor_vertex_channel_source(
                    m_cachedPolymesh3->get_const_cvt_vertex_accessor<frantic::graphics::vector3f>( _T("Velocity") ) ) );
            }
            if( displayMode == DISPLAY_MODE::VERTICES && displayFraction < 1.0 ) {
                velocity.reset( new fractional_vertex_channel_source( velocity, displayFraction ) );
            }
            if( scale != 1.f || timeDerivative != 1.f ) {
                velocity.reset( new scaled_vertex_channel_source( velocity, scale * timeDerivative ) );
            }
        }
    }
    if( velocity && timeOffset != 0 ) {
        verts.reset( new velocity_offset_vertex_channel_source( verts, velocity, timeOffset ) );
    }

    if( displayMode == DISPLAY_MODE::MESH || displayMode == DISPLAY_MODE::FACES ) {
        for( polymesh3::iterator it = m_cachedPolymesh3->begin(), itEnd = m_cachedPolymesh3->end(); it != itEnd;
             ++it ) {
            if( it->second.is_vertex_channel() ) {
                bool isMap = false;
                int mapNum = 0;
                if( it->first == _T("Color") ) {
                    isMap = true;
                    mapNum = 0;
                } else if( it->first == _T("TextureCoord") ) {
                    isMap = true;
                    mapNum = 1;
                } else if( it->first == _T("Selection") ) {
                    typed_vertex_channel_source<float>::ptr_type vertexSelection;
                    if( it->second.type() == frantic::channels::channel_data_type_traits<float>::data_type() ) {
                        vertexSelection.reset( new accessor_typed_vertex_channel_source<float>(
                            m_cachedPolymesh3->get_const_vertex_accessor<float>( it->first ) ) );
                    } else {
                        vertexSelection.reset( new cvt_accessor_typed_vertex_channel_source<float>(
                            m_cachedPolymesh3->get_const_cvt_vertex_accessor<float>( it->first ) ) );
                    }
                    channels.vertexSelection = vertexSelection;
                } else if( it->first == _T("Normal") ) {
                    vertex_channel_source::ptr_type vertexNormal;
                    if( it->second.type() == frantic::channels::data_type_float32 ) {
                        vertexNormal.reset( new accessor_vertex_channel_source(
                            m_cachedPolymesh3->get_const_vertex_accessor<frantic::graphics::vector3f>( it->first ) ) );
                    } else {
                        vertexNormal.reset( new cvt_accessor_vertex_channel_source(
                            m_cachedPolymesh3->get_const_cvt_vertex_accessor<frantic::graphics::vector3f>(
                                it->first ) ) );
                    }
                    channels.vertexNormal = vertexNormal;
                } else if( it->first == _T("EdgeSharpness") ) {
                    typed_vertex_channel_source<float>::ptr_type edgeCrease;
                    if( it->second.type() == frantic::channels::channel_data_type_traits<float>::data_type() ) {
                        edgeCrease.reset( new accessor_typed_vertex_channel_source<float>(
                            m_cachedPolymesh3->get_const_vertex_accessor<float>( it->first ) ) );
                    } else {
                        edgeCrease.reset( new cvt_accessor_typed_vertex_channel_source<float>(
                            m_cachedPolymesh3->get_const_cvt_vertex_accessor<float>( it->first ) ) );
                    }
                    channels.edgeCrease = edgeCrease;
                } else if( it->first == _T("VertexSharpness") ) {
                    typed_vertex_channel_source<float>::ptr_type vertexCrease;
                    if( it->second.type() == frantic::channels::channel_data_type_traits<float>::data_type() ) {
                        vertexCrease.reset( new accessor_typed_vertex_channel_source<float>(
                            m_cachedPolymesh3->get_const_vertex_accessor<float>( it->first ) ) );
                    } else {
                        vertexCrease.reset( new cvt_accessor_typed_vertex_channel_source<float>(
                            m_cachedPolymesh3->get_const_cvt_vertex_accessor<float>( it->first ) ) );
                    }
                    channels.vertexCrease = vertexCrease;
                } else if( _tcsnccmp( it->first.c_str(), _T("Mapping"), 7 ) == 0 ) {
                    bool gotMapNum = false;
                    try {
                        mapNum = boost::lexical_cast<int>( it->first.substr( 7 ) );
                        gotMapNum = true;
                    } catch( boost::bad_lexical_cast& ) {
                        gotMapNum = false;
                    }
                    isMap = gotMapNum && mapNum >= 0 && mapNum < MAX_MESHMAPS;
                }
                if( isMap ) {
                    vertex_channel_source::ptr_type ch =
                        create_map_vertex_channel_source( m_cachedPolymesh3, it->first );
                    if( hasXForm ) {
                        ch = get_transformed( ch, xform, m_metadata.get_channel_transform_type( it->first ) );
                    }
                    if( mapNum >= 0 && mapNum < static_cast<int>( channels.maps.size() ) ) {
                        channels.maps[mapNum] = ch;
                    }
                }
            } else {
                if( it->first == _T("SmoothingGroup") ) {
                    face_channel_source<int>::ptr_type sg;
                    if( it->second.type() == frantic::channels::channel_data_type_traits<int>::data_type() ) {
                        sg.reset( new accessor_face_channel_source<int>(
                            m_cachedPolymesh3->get_const_face_accessor<int>( it->first ) ) );
                    } else {
                        sg.reset( new cvt_accessor_face_channel_source<int>(
                            m_cachedPolymesh3->get_const_cvt_face_accessor<int>( it->first ) ) );
                    }
                    channels.smoothingGroup = sg;
                } else if( it->first == _T("MaterialID") ) {
                    face_channel_source<MtlID>::ptr_type matId;
                    if( it->second.type() == frantic::channels::channel_data_type_traits<MtlID>::data_type() ) {
                        matId.reset( new accessor_face_channel_source<MtlID>(
                            m_cachedPolymesh3->get_const_face_accessor<MtlID>( it->first ) ) );
                    } else {
                        matId.reset( new cvt_accessor_face_channel_source<MtlID>(
                            m_cachedPolymesh3->get_const_cvt_face_accessor<MtlID>( it->first ) ) );
                    }
                    channels.materialID = matId;
                } else if( it->first == _T("FaceSelection") ) {
                    face_channel_source<boost::int32_t>::ptr_type faceSelection;
                    if( it->second.type() ==
                        frantic::channels::channel_data_type_traits<boost::int32_t>::data_type() ) {
                        faceSelection.reset( new accessor_face_channel_source<boost::int32_t>(
                            m_cachedPolymesh3->get_const_face_accessor<boost::int32_t>( it->first ) ) );
                    } else {
                        faceSelection.reset( new cvt_accessor_face_channel_source<boost::int32_t>(
                            m_cachedPolymesh3->get_const_cvt_face_accessor<boost::int32_t>( it->first ) ) );
                    }
                    channels.faceSelection = faceSelection;
                } else if( it->first == _T("FaceEdgeVisibility") ) {
                    face_channel_source<boost::int8_t>::ptr_type faceEdgeVisibility;
                    if( it->second.type() == frantic::channels::channel_data_type_traits<boost::int8_t>::data_type() ) {
                        faceEdgeVisibility.reset( new accessor_face_channel_source<boost::int8_t>(
                            m_cachedPolymesh3->get_const_face_accessor<boost::int8_t>( it->first ) ) );
                    } else {
                        faceEdgeVisibility.reset( new cvt_accessor_face_channel_source<boost::int8_t>(
                            m_cachedPolymesh3->get_const_cvt_face_accessor<boost::int8_t>( it->first ) ) );
                    }
                    channels.faceEdgeVisibility = faceEdgeVisibility;
                }
            }
        }

        // Velocity to map channel
        if( velocity && m_velToMapChannel.at_time( 0 ) ) {
            const int mapChannel = m_outputVelocityMapChannel.at_time( 0 );
            if( mapChannel >= 0 && mapChannel < static_cast<int>( channels.maps.size() ) ) {
                if( hasXForm ) {
                    velocity =
                        get_transformed( velocity, xform, m_metadata.get_channel_transform_type( _T("Velocity") ) );
                }
                channels.maps[m_outputVelocityMapChannel.at_time( 0 )] = velocity;
            }
        }
    }

    channels.verts = verts;

    if( displayMode == DISPLAY_MODE::FACES && displayFraction < 1.0 ) {
        if( channels.verts ) {
            channels.verts.reset( new fractional_face_vertex_channel_source( channels.verts, displayFraction ) );
        }
        for( std::size_t i = 0; i < channels.maps.size(); ++i ) {
            if( channels.maps[i] ) {
                channels.maps[i].reset(
                    new fractional_face_vertex_channel_source( channels.maps[i], displayFraction ) );
            }
        }
        if( channels.vertexSelection ) {
            channels.vertexSelection.reset(
                new fractional_face_typed_vertex_channel_source<float>( channels.vertexSelection, displayFraction ) );
        }
        if( channels.vertexNormal ) {
            channels.vertexNormal.reset(
                new fractional_face_vertex_channel_source( channels.vertexNormal, displayFraction ) );
        }
        if( channels.smoothingGroup ) {
            channels.smoothingGroup.reset(
                new fractional_face_channel_source<int>( channels.smoothingGroup, displayFraction ) );
        }
        if( channels.materialID ) {
            channels.materialID.reset(
                new fractional_face_channel_source<MtlID>( channels.materialID, displayFraction ) );
        }
        if( channels.faceSelection ) {
            channels.faceSelection.reset(
                new fractional_face_channel_source<boost::int32_t>( channels.faceSelection, displayFraction ) );
        }
        if( channels.faceEdgeVisibility ) {
            channels.faceEdgeVisibility.reset(
                new fractional_face_channel_source<boost::int8_t>( channels.faceEdgeVisibility, displayFraction ) );
        }
    }
}

namespace {

void clear_faces( Mesh& mesh ) {
    mesh.setNumFaces( 0 );
    mesh.InvalidateTopologyCache();
}

void clear_faces( MNMesh& mm ) {
    mm.setNumFaces( 0 );
    mm.InvalidateTopoCache();
    mm.FillInMesh();
    mm.PrepForPipeline();
}

} // anonymous namespace

void XMeshLoader::polymesh_copy( bool useVelocity, float timeOffset, float timeDerivative,
                                 const frantic::graphics::transform4f& xform ) {
    if( !m_cachedPolymesh3 ) {
        m_meshBoundingBox.set_to_empty();
        clear_polymesh( mm );
        clear_mesh( mesh );
        return;
    }

    // build a channel assignment
    mesh_channel_assignment channels;
    build_channel_assignment( channels, useVelocity, timeOffset, timeDerivative, xform );

    const int displayMode = get_current_display_mode();
    const float scale = static_cast<float>( calculate_mesh_scale_factor() );

    if( displayMode == DISPLAY_MODE::BOX ) {
        clear_polymesh( mm );
        clear_mesh( mesh );

        if( m_metadata.has_boundbox() ) {
            m_meshBoundingBox = m_metadata.get_boundbox();
            m_meshBoundingBox.minimum() *= scale;
            m_meshBoundingBox.maximum() *= scale;
        } else {
            m_meshBoundingBox.set_to_empty();
            if( channels.verts ) {
                for( std::size_t i = 0, ie = channels.verts->size(); i != ie; ++i ) {
                    m_meshBoundingBox += channels.verts->get( i );
                }
            }
        }
    } else {
        if( has_triangle_mesh() ) {
            ::polymesh_copy( mesh, channels );
            m_meshBoundingBox = frantic::max3d::from_max_t( mesh.getBoundingBox() );
        } else {
            ::polymesh_copy( mm, channels );
            m_meshBoundingBox = frantic::max3d::from_max_t( mm.getBoundingBox() );
        }

        if( displayMode == DISPLAY_MODE::VERTICES ) {
            clear_faces( mm );
            clear_faces( mesh );
        }
    }
}

/**
 * Creates the mesh for MAX to display for the specified time
 *
 * @param time the time to build a mesh for
 */
void XMeshLoader::BuildMesh( TimeValue t ) {

    // If we've got a valid mesh, just return.
    if( ivalid.InInterval( t ) )
        return;
    ivalid.SetInstant( t );

    Matrix3 m;
    m.IdentityMatrix();

    try {
        boost::timer timer;

        FF_LOG( stats ) << "Updating for frame "
                        << boost::basic_format<TCHAR>( _T("%.3f") ) % ( static_cast<double>( t ) / GetTicksPerFrame() )
                        << " (" << get_node_name() << ")" << std::endl;

        const bool ignoreMissingFile =
            !m_inRenderingMode && frantic::max3d::is_network_render_server() && m_ignoreMissingViewOnRenderNode.at_time( t );

        // Update the "Loading Frame:" display
        display_loading_frame_info();

        // If we're drawing the viewport and loading is disabled, build an empty mesh (ignore disableLoading at render
        // time) and return
        if( m_inRenderingMode && !m_enableRenderMesh.at_time( t ) ||
            !m_inRenderingMode && !m_enableViewportMesh.at_time( t ) ) {
            m_cachedPolymesh3.reset();
            m_cachedPolymesh3IsTriangles = true;
            m_meshBoundingBox.set_to_empty();
            m_doneBuildNormals = false;
            clear_mesh( mesh );
            clear_polymesh( mm );
            return;
        }

        const int loadMode = m_loadMode.at_time( t );
        const int displayMode = m_displayMode.at_time( t );
        bool invalidCache = false;

        int loadMask = 0;
        if( m_inRenderingMode || frantic::max3d::is_network_render_server() ) {
            loadMask = LOAD_POLYMESH3_MASK::ALL;
        } else {
            if( displayMode == DISPLAY_MODE::BOX ) {
                loadMask |= LOAD_POLYMESH3_MASK::BOX;
            } else if( displayMode == DISPLAY_MODE::MESH || displayMode == DISPLAY_MODE::FACES ) {
                loadMask |= LOAD_POLYMESH3_MASK::STATIC_MESH;
            } else if( displayMode == DISPLAY_MODE::VERTICES ) {
                loadMask |= LOAD_POLYMESH3_MASK::VERTS;
            }
        }

        // If we are only loading a single frame, load what's in the sequence name edit text box
        if( m_loadSingleFrame.at_time( t ) ) {
            if( m_cachedSequenceID != get_current_sequence_id() || m_cachedLoadingMode != loadMode ||
                m_cachedMeshInRenderingMode != m_inRenderingMode ||
                !m_inRenderingMode && m_cachedDisplayMode != displayMode ) {
                const frantic::tstring filename = get_current_sequence_path();

                if( filename.empty() || ignoreMissingFile && !frantic::files::file_exists( filename ) ) {
                    m_cachedPolymesh3.reset();
                } else {
                    m_cachedPolymesh3.reset();
                    m_metadata.clear();
                    m_cachedPolymesh3 =
                        m_polymesh3Loader.load( filename, &m_metadata, loadMask ); // load_polymesh_file( filename );
                    m_cachedPolymesh3IsTriangles = m_cachedPolymesh3->is_triangle_mesh();
                    ivalid = FOREVER;
                }

                invalidCache = true;
                m_cachedTime = t;
            }

            if( m_cachedPolymesh3 ) {
                polymesh_copy( false, 0, 0, m );
            } else {
                m_cachedPolymesh3IsTriangles = true;
                m_meshBoundingBox.set_to_empty();
                clear_mesh( mesh );
                clear_polymesh( mm );
            }
        } else {
            TimeValue tpf = GetTicksPerFrame();
            TimeValue realTime;

            const frantic::tstring filename = get_current_sequence_path();

            if( filename.empty() ) {
                m_cachedPolymesh3.reset();
                m_cachedPolymesh3IsTriangles = true;
                m_meshBoundingBox.set_to_empty();
                clear_mesh( mesh );
                clear_polymesh( mm );
                invalidCache = true;
                m_cachedTime = t;
            } else {
                // Pick the correct file sequence
                frantic::files::filename_sequence& sequence( get_current_sequence( !ignoreMissingFile ) );

                // if the file cache is empty, clear the mesh and return
                if( sequence.get_frame_set().empty() ) {
                    m_cachedPolymesh3.reset();
                    m_cachedPolymesh3IsTriangles = true;
                    m_meshBoundingBox.set_to_empty();
                    m_doneBuildNormals = false;
                    clear_mesh( mesh );
                    clear_polymesh( mm );

                    const frantic::tstring currentSequencePath = get_current_sequence_path();
                    if( currentSequencePath.empty() || ignoreMissingFile ) {
                        return;
                    } else {
                        throw std::runtime_error( "No files found in sequence: " +
                                                  frantic::strings::to_string( currentSequencePath ) );
                    }
                }

                // Depending on the loading mode, the cached information works differently

                // These load modes find the nearest mesh to the query time in the underlying sequence, and load it
                // offsetting the position appropriately according to the velocity.
                if( loadMode == LOAD_MODE::FRAME_VELOCITY_OFFSET ||
                    loadMode == LOAD_MODE::NEAREST_FRAME_VELOCITY_OFFSET ||
                    loadMode == LOAD_MODE::SUBFRAME_VELOCITY_OFFSET ) {
                    TimeValue outPivotTime = round_to_nearest_wholeframe( t );
                    if( ( loadMode == LOAD_MODE::FRAME_VELOCITY_OFFSET ||
                          loadMode == LOAD_MODE::SUBFRAME_VELOCITY_OFFSET ) &&
                        HasValidRenderTime() ) {
                        outPivotTime = round_to_nearest_wholeframe( GetRenderTime() );
                    }

                    TimeValue outTime = t;

                    TimeValue inTime, inPivotTime;
                    float inTimeDerivative, inPivotTimeDerivative;

                    get_timing_data_without_limit_to_range( outTime, tpf / 4, inTime, inTimeDerivative );
                    get_timing_data_without_limit_to_range( outPivotTime, tpf / 4, inPivotTime, inPivotTimeDerivative );

                    enum clamp_region {
                        CLAMP_REGION_INSIDE,
                        CLAMP_REGION_BEFORE,
                        CLAMP_REGION_AFTER
                    } clampRegion = CLAMP_REGION_INSIDE;

                    TimeValue requestTime;
                    const float timeDerivative = inTimeDerivative;

                    // Determine the request time, and
                    // handle clamping outside the frame range.
                    std::pair<TimeValue, TimeValue> frameRange( m_startFrame.at_time( t ) * GetTicksPerFrame(),
                                                                m_endFrame.at_time( t ) * GetTicksPerFrame() );
                    if( loadMode == LOAD_MODE::FRAME_VELOCITY_OFFSET ||
                        loadMode == LOAD_MODE::NEAREST_FRAME_VELOCITY_OFFSET ) {
                        if( m_limitToRange.at_time( t ) ) {
                            if( inPivotTime < frameRange.first ) {
                                clampRegion = CLAMP_REGION_BEFORE;
                                inPivotTime = frameRange.first;
                            } else if( inPivotTime > frameRange.second ) {
                                clampRegion = CLAMP_REGION_AFTER;
                                inPivotTime = frameRange.second;
                            }
                        }
                        requestTime = inPivotTime;
                    } else if( loadMode == LOAD_MODE::SUBFRAME_VELOCITY_OFFSET ) {
                        if( m_limitToRange.at_time( t ) ) {
                            // When the input time is outside the range limit,
                            // but the frame being rendered is still inside the limit,
                            // use velocity offset from the frame being rendered.
                            //
                            // This is intended to fix the old behaviour, which created
                            // incorrect velocities in the frames on the limit edges.
                            if( inTime < frameRange.first ) {
                                if( inPivotTime < frameRange.first ) {
                                    requestTime = frameRange.first;
                                    clampRegion = CLAMP_REGION_BEFORE;
                                } else {
                                    requestTime = inPivotTime;
                                }
                            } else if( inTime > frameRange.second ) {
                                if( inPivotTime > frameRange.second ) {
                                    requestTime = frameRange.second;
                                    clampRegion = CLAMP_REGION_AFTER;
                                } else {
                                    requestTime = inPivotTime;
                                }
                            } else {
                                requestTime = inTime;
                            }
                        } else {
                            requestTime = inTime;
                        }
                    } else {
                        throw std::runtime_error( "Internal Error: loadMode mismatch" );
                    }

                    bool useEmptyMesh = false;
                    bool useHoldMesh = false;

                    if( clampRegion != CLAMP_REGION_INSIDE ) {
                        const int beforeRangeMode = m_beforeRangeMode.at_time( t );
                        const int afterRangeMode = m_afterRangeMode.at_time( t );
                        const int rangeMode = ( clampRegion == CLAMP_REGION_BEFORE ) ? beforeRangeMode : afterRangeMode;

                        if( rangeMode == FRAME_RANGE_CLAMP_MODE::BLANK ) {
                            useEmptyMesh = true;
                        } else {
                            useHoldMesh = true;
                        }
                    }

                    if( useEmptyMesh ) {
                        const TimeValue invalidTime = ( std::numeric_limits<TimeValue>::min )();

                        invalidCache = true;
                        m_cachedTime = invalidTime;
                        m_cachedPolymesh3.reset();
                        m_cachedPolymesh3IsTriangles = true;
                        m_meshBoundingBox.set_to_empty();
                        clear_mesh( mesh );
                        clear_polymesh( mm );
                    } else {
                        double sampleFrameNumber;
                        if( !get_nearest_subframe( requestTime, sampleFrameNumber ) ) {
                            throw std::runtime_error(
                                "An appropriate frame to offset for time " +
                                boost::lexical_cast<std::string>( float( inPivotTime ) / GetTicksPerFrame() ) +
                                " could not be found in the sequence: " +
                                frantic::strings::to_string( get_current_sequence_path() ) +
                                "\nPlease ensure that the sequence exists, and set Limit to Custom Range (in the Files "
                                "rollout) to correspond to the sequence frames." );
                        }

                        const TimeValue sampleTime = (TimeValue)( sampleFrameNumber * GetTicksPerFrame() );

                        if( useHoldMesh ) {
                            inTime = sampleTime;
                        }

                        if( ( inTime != sampleTime || m_velToMapChannel.at_time( t ) ) &&
                            m_displayMode.at_time( t ) != DISPLAY_MODE::BOX ) {
                            loadMask |= LOAD_POLYMESH3_MASK::VELOCITY;
                        }

                        // Check the cache and rebuild if necessary
                        if( loadMode != m_cachedLoadingMode || sampleTime != m_cachedTime ||
                            get_current_sequence_id() != m_cachedSequenceID ||
                            !m_inRenderingMode && m_cachedDisplayMode != displayMode ) {
                            invalidCache = true;
                            m_cachedTime = sampleTime;
                            load_mesh_at_frame( sampleFrameNumber, loadMask );
                        }

                        const float timeOffset =
                            ( timeDerivative != 0 ? TicksToSec( inTime - sampleTime ) / timeDerivative : 0 );
                        polymesh_copy( true, timeOffset, timeDerivative, m );
                    }
                } else if( loadMode == LOAD_MODE::FRAME_INTERPOLATION ||
                           loadMode == LOAD_MODE::SUBFRAME_INTERPOLATION ) {

                    bool useSubframes = false;
                    if( loadMode == LOAD_MODE::SUBFRAME_INTERPOLATION )
                        useSubframes = true;

                    float timeDerivative;
                    bool isEmptyMeshTime;
                    get_timing_data( t, tpf / 4, realTime, timeDerivative, isEmptyMeshTime );

                    if( isEmptyMeshTime ) {
                        const TimeValue invalidTime = ( std::numeric_limits<TimeValue>::min )();

                        invalidCache = true;
                        m_cachedInterval.Set( invalidTime, invalidTime );
                        m_cachedPolymesh3.reset();
                        m_cachedPolymesh3IsTriangles = true;
                    } else {
                        double frameNumber = (double)realTime / GetTicksPerFrame();

                        // Find the bracketing frames for this time
                        std::pair<double, double> frameBracket;
                        float alpha;
                        bool foundFrames;
                        if( useSubframes )
                            foundFrames = sequence.get_nearest_subframe_interval( frameNumber, frameBracket, alpha );
                        else
                            foundFrames = sequence.get_nearest_wholeframe_interval( frameNumber, frameBracket, alpha );

                        if( !foundFrames )
                            throw std::runtime_error( "Appropriate frames to interpolate at time " +
                                                      boost::lexical_cast<std::string>( frameNumber ) +
                                                      " could not be found in the sequence." );

                        Interval interval;
                        interval.Set( (TimeValue)( frameBracket.first * tpf ),
                                      (TimeValue)( frameBracket.second * tpf ) );

                        if( loadMode != m_cachedLoadingMode || m_cachedInterval != interval ||
                            get_current_sequence_id() != m_cachedSequenceID ||
                            !m_inRenderingMode && m_cachedDisplayMode != displayMode ) {
                            invalidCache = true;

                            load_mesh_interpolated( frameBracket, alpha,
                                                    loadMask ); // this sets the cached interval for us
                        } else {
                            if( loadMask == LOAD_POLYMESH3_MASK::BOX ) {
                                // Arbitrarily use the first mesh.
                                // We won't use the mesh -- we'll use the metadata instead.
                                // TODO: fix this ?
                                m_cachedPolymesh3 = m_cachedPolymesh3Interval.first;
                            } else {
                                m_cachedPolymesh3 = linear_interpolate( m_cachedPolymesh3Interval.first,
                                                                        m_cachedPolymesh3Interval.second, alpha );
                            }
                            m_cachedPolymesh3IsTriangles = m_cachedPolymesh3->is_triangle_mesh();
                        }
                    }

                    if( m_cachedPolymesh3 ) {
                        polymesh_copy( false, 0, 0, m );
                    } else {
                        m_cachedPolymesh3IsTriangles = true;
                        m_meshBoundingBox.set_to_empty();
                        clear_mesh( mesh );
                        clear_polymesh( mm );
                    }

                } else {
                    throw std::runtime_error( "Unknown loading mode \"" + boost::lexical_cast<std::string>( loadMode ) +
                                              "\"" );
                }
            }
        }

        // rebuild the bounding box to match the new mesh
        m_doneBuildNormals = false;
        mesh.buildBoundingBox();

        // if we had to build a new cache, then save the new cache data
        if( invalidCache ) {
            // Save cache settings
            m_cachedSequenceID = get_current_sequence_id();
            m_cachedLoadingMode = loadMode;
            m_cachedMeshInRenderingMode = m_inRenderingMode;
            m_cachedDisplayMode = displayMode;

            // If we don't want to cache a copy of the trimesh in memory, wipe it out after loading.
            // Make sure to invalidate the cachedMesh times so that a new one will get loaded
            // on the next change in time.
            if( !m_keepMeshInMemory.at_time( t ) ) {
                clear_trimesh3_cache();
            }
        }

        FF_LOG( stats ) << "Update time [s]: " << boost::basic_format<TCHAR>( _T("%.3f") ) % timer.elapsed()
                        << std::endl;

    } catch( const std::exception& e ) {
        m_cachedPolymesh3.reset();
        m_cachedPolymesh3IsTriangles = true;
        m_meshBoundingBox.set_to_empty();
        m_doneBuildNormals = false;
        m_cachedPolymesh3Interval.first.reset();
        m_cachedPolymesh3Interval.second.reset();
        clear_mesh( mesh );
        clear_polymesh( mm );
        invalidate_cache();

        frantic::tstring errmsg = _T("XMeshLoader::BuildMesh (") + get_node_name() + _T("): ") +
                                  frantic::strings::to_tstring( e.what() ) + _T("\n");
        report_error( errmsg );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

/**
 * Calculate the frame time and time derivative, accounting for all timing options
 *
 * @param time     the original time
 * @param timeStep the time step to use when calculating the time derivative
 * @param outTime  the adjusted time
 * @param outTimeDerivative the calculated time derivative
 */
void XMeshLoader::get_timing_data_without_limit_to_range( TimeValue time, TimeValue timeStep, TimeValue& outTime,
                                                          float& outTimeDerivative ) {
    TimeValue realTime( time ), tpf = GetTicksPerFrame();
    float timeDerivative( 1.0f );

    if( m_enablePlaybackGraph.at_time( time ) )
        realTime = static_cast<TimeValue>( m_playbackGraphTime.at_time( time ) * tpf );

    // --- Add frame offset
    realTime += m_frameOffset.at_time( time ) * GetTicksPerFrame();

    // --- Calculate time derivative
    if( m_enablePlaybackGraph.at_time( time ) ) {
        float intervalStart = static_cast<float>( m_playbackGraphTime.at_time( time - ( timeStep / 2 ) ) );
        float intervalEnd = static_cast<float>( m_playbackGraphTime.at_time( time + ( timeStep / 2 ) ) );
        timeDerivative = tpf * ( intervalEnd - intervalStart ) / timeStep;
    }

    outTime = realTime;
    outTimeDerivative = timeDerivative;
}

/**
 * Calculate the frame time and time derivative, accounting for all timing options
 *
 * @param time     the original time
 * @param timeStep the time step to use when calculating the time derivative
 * @param outTime  the adjusted time
 * @param outTimeDerivative the calculated time derivative
 * @param outIsBlankTime true if an empty mesh is requested for this time
 */
void XMeshLoader::get_timing_data( TimeValue time, TimeValue timeStep, TimeValue& outTime, float& outTimeDerivative,
                                   bool& outIsBlankTime ) {
    TimeValue realTime( time ), tpf = GetTicksPerFrame();
    float timeDerivative( 1.0f );

    get_timing_data_without_limit_to_range( time, timeStep, realTime, timeDerivative );

    outIsBlankTime = false;

    if( m_limitToRange.at_time( time ) ) {
        // --- Make sure the frame value is within the supported range of the cache.
        //     Clamp to range if specified, otherwise throw
        std::pair<TimeValue, TimeValue> frameRange( m_startFrame.at_time( time ) * tpf,
                                                    m_endFrame.at_time( time ) * tpf );

        if( ( realTime < frameRange.first && m_beforeRangeMode.at_time( time ) == FRAME_RANGE_CLAMP_MODE::BLANK ) ||
            ( realTime > frameRange.second && m_afterRangeMode.at_time( time ) == FRAME_RANGE_CLAMP_MODE::BLANK ) ) {
            outIsBlankTime = true;
        } else {
            realTime = frantic::math::clamp( realTime, frameRange.first, frameRange.second );
        }
    }

    outTime = realTime;
    outTimeDerivative = timeDerivative;
}

/**
 * This loads the mesh derived by interpolating between the frames in interval by alpha
 *
 * @param interval the start and end frames of the range
 * @param alpha    the interpolation amount
 */
void XMeshLoader::load_mesh_interpolated( std::pair<double, double> interval, float alpha, int loadMask ) {

    filename_sequence& sequence = get_current_sequence();
    int tpf = GetTicksPerFrame();

    // check for file existence
    if( !sequence.get_frame_set().frame_exists( interval.first ) ) {
        throw std::runtime_error( "XMeshLoader::load_mesh_interpolated: Frame " +
                                  boost::lexical_cast<std::string>( interval.first ) +
                                  " does not exist in the selected sequence." );
    } else if( !sequence.get_frame_set().frame_exists( interval.second ) ) {
        throw std::runtime_error( "XMeshLoader::load_mesh_interpolated: Frame " +
                                  boost::lexical_cast<std::string>( interval.second ) +
                                  " does not exist in the selected sequence." );
    } else {
        // Load the meshes
        // TODO: how do we blend metadata ?
        const bool loadMetadataFromFirst = alpha < 0.5f;
        m_metadata.clear();
        m_cachedPolymesh3Interval.first.reset();
        m_cachedPolymesh3Interval.first =
            m_polymesh3Loader.load( sequence[interval.first], loadMetadataFromFirst ? &m_metadata : 0, loadMask );
        // m_cachedPolymesh3Interval.first = load_polymesh_file( sequence[interval.first] );
        m_cachedPolymesh3Interval.second.reset();
        m_cachedPolymesh3Interval.second =
            m_polymesh3Loader.load( sequence[interval.second], loadMetadataFromFirst ? 0 : &m_metadata, loadMask );
        // m_cachedPolymesh3Interval.second = load_polymesh_file( sequence[interval.second] );

        m_cachedInterval.Set( static_cast<TimeValue>( interval.first * tpf ),
                              static_cast<TimeValue>( interval.second * tpf ) );

        if( loadMask == LOAD_POLYMESH3_MASK::BOX ) {
            // Arbitrarily use the first polymesh.
            // We will not look at the polymesh anyways because we're using the box.
            // TODO : we shouldn't need a mesh at all in this case
            m_cachedPolymesh3 = m_cachedPolymesh3Interval.first;
        } else {
            if( alpha == 0 ) {
                m_cachedPolymesh3 = m_cachedPolymesh3Interval.first;
            } else if( alpha == 1.f ) {
                m_cachedPolymesh3 = m_cachedPolymesh3Interval.second;
            } else {
                m_cachedPolymesh3 =
                    linear_interpolate( m_cachedPolymesh3Interval.first, m_cachedPolymesh3Interval.second, alpha );
            }
        }
        m_cachedPolymesh3IsTriangles = m_cachedPolymesh3->is_triangle_mesh();
    }
}

/**
 * Given a time value, finds the closest frame to that time (including subframes)
 *
 * @param frameTime   the time to find a frame near
 * @param frameNumber holds the nearest frame number on return
 * @return            true if a suitable frame was found, false otherwise
 */
bool XMeshLoader::get_nearest_subframe( TimeValue frameTime, double& frameNumber ) {

    std::pair<double, double> range;
    float alpha;
    double frame( (double)frameTime / GetTicksPerFrame() );

    if( get_current_sequence().get_frame_set().frame_exists( frame ) ) {
        frameNumber = frame;
    } else if( get_current_sequence().get_frame_set().get_nearest_subframe_interval( frame, range, alpha ) ) {
        if( alpha < 0.5 )
            frameNumber = range.first;
        else
            frameNumber = range.second;
    } else {
        return false;
    }

    return true;
}

TimeValue XMeshLoader::round_to_nearest_wholeframe( TimeValue t ) const {
    const TimeValue tpf = GetTicksPerFrame();

    if( t >= 0 ) {
        return ( ( t + tpf / 2 ) / tpf ) * tpf;
    } else {
        return ( ( t - tpf / 2 + 1 ) / tpf ) * tpf;
    }
}

/**
 * Given a time value, finds the closest wholeframe to that time
 *
 * @param frameTime   the time to find a frame near
 * @param frameNumber holds the nearest frame number on return
 * @return            true if a suitable frame was found, false otherwise
 */
bool XMeshLoader::get_nearest_wholeframe( TimeValue frameTime, double& frameNumber ) {

    TimeValue tpf = GetTicksPerFrame();
    frameTime = round_to_nearest_wholeframe( frameTime );
    double frame = (double)frameTime / tpf;

    std::pair<double, double> range;
    float alpha;

    if( get_current_sequence().get_frame_set().frame_exists( frame ) ) {
        frameNumber = frame;
    } else if( get_current_sequence().get_nearest_wholeframe_interval( frame, range, alpha ) ) {
        if( alpha < 0.5 )
            frameNumber = range.first;
        else
            frameNumber = range.second;
    } else {
        return false;
    }

    return true;
}

/**
 * This is just a wrapper for NotifyDependents
 */
void XMeshLoader::NotifyEveryone() {
    // Called when we change the object, so it's dependents get informed
    // and it redraws, etc.
    NotifyDependents( FOREVER, (PartID)PART_ALL, REFMSG_CHANGE );
}

/**
 * This loads the mesh for the specified frame into the cached trimesh.
 *
 * @param frame the frame to load
 */
void XMeshLoader::load_mesh_at_frame( double frame, int loadMask ) {

    filename_sequence& sequence = get_current_sequence();

    if( !sequence.get_frame_set().empty() && sequence.get_frame_set().frame_exists( frame ) ) {
        // If the cache wasn't empty, check to make sure the file is in the cache, and load it
        m_cachedPolymesh3.reset();
        m_metadata.clear();
        m_cachedPolymesh3 = m_polymesh3Loader.load( sequence[frame], &m_metadata, loadMask );
        m_cachedPolymesh3IsTriangles = m_cachedPolymesh3->is_triangle_mesh();
    } else {
        // The file requested isn't there.
        throw std::runtime_error( "XMeshLoader::load_mesh_at_frame: File '" +
                                  frantic::strings::to_string( sequence[frame] ) + "' requested does not exist." );
    }
}

frantic::tstring XMeshLoader::get_proxy_path() {
#if MAX_VERSION_MAJOR >= 12
    MaxSDK::AssetManagement::AssetUser asset = pblock2->GetAssetUser( pb_proxySequence );
    if( asset.GetId() != MaxSDK::AssetManagement::kInvalidId ) {
        MSTR path;
        bool success = asset.GetFullFilePath( path );
        if( success ) {
            return frantic::tstring( path );
        }
    }
#endif
    return m_proxyPath.at_time( 0 );
}

frantic::tstring XMeshLoader::get_render_path() {
#if MAX_VERSION_MAJOR >= 12
    MaxSDK::AssetManagement::AssetUser asset = pblock2->GetAssetUser( pb_renderSequence );
    if( asset.GetId() != MaxSDK::AssetManagement::kInvalidId ) {
        MSTR path;
        bool success = asset.GetFullFilePath( path );
        if( success ) {
            return frantic::tstring( path );
        }
    }
#endif
    return m_renderPath.at_time( 0 );
}

/**
 * Reset the cached mesh info (to force a reload on the next call to BuildMesh)
 */
void XMeshLoader::invalidate_cache() {
    TimeValue invalidTime = ( std::numeric_limits<TimeValue>::min )();

    m_cachedTime = invalidTime;
    m_cachedInterval.Set( invalidTime, invalidTime );

    m_cachedSequenceID = ( std::numeric_limits<int>::min )();
    m_cachedLoadingMode = std::numeric_limits<int>::max();
}

void XMeshLoader::invalidate_sequences() {
    for( std::size_t i = 0; i < SEQUENCE_ID::COUNT; ++i ) {
        m_fileSequences[i] = frantic::files::filename_sequence();
        m_fileSequencePaths[i].clear();
    }
    m_polymesh3Loader.clear_cache();
}

bool XMeshLoader::is_autogen_proxy_path_enabled() { return m_autogenProxyPath.at_time( 0 ); }

void XMeshLoader::set_to_valid_frame_range( bool notify, bool setLoadSingleFrame ) {
    bool gotRange = false;
    std::pair<int, int> frameRange;

    try {
        const frantic::tstring path = get_render_sequence_path();
        if( path.empty() ) {
            return;
        }
        const frantic::files::filename_sequence& sequence = get_render_sequence();
        const frantic::files::frame_set& fset = sequence.get_frame_set();

        if( !fset.empty() ) {
            frameRange = fset.wholeframe_range();
            gotRange = true;
        }
    } catch( std::exception& ) {
    }

    if( gotRange ) {
        if( setLoadSingleFrame ) {
            m_loadSingleFrame.at_time( 0 ) = false;
        }
        m_startFrame.at_time( 0 ) = frameRange.first;
        m_endFrame.at_time( 0 ) = frameRange.second;
        m_limitToRange.at_time( 0 ) = true;
    } else {
        const frantic::tstring filename = get_render_sequence_path();
        const boost::filesystem::path filepath( filename );
        if( boost::filesystem::exists( filepath ) && boost::filesystem::is_regular_file( filepath ) ) {
            if( setLoadSingleFrame ) {
                m_loadSingleFrame.at_time( 0 ) = true;
            } else if( notify ) {
                const bool loadSingleFrame = m_loadSingleFrame.at_time( 0 );
                const frantic::tstring msg = _T("Cannot determine a safe range.  The sequence\n") + filename +
                                             _T("\ndoes not have a frame number.\n\nIt will load correctly only in ")
                                             _T("'Load Single Frame Only' mode which is currently ") +
                                             ( loadSingleFrame ? _T("ON") : _T("OFF") ) + _T(".");
                UINT icon = loadSingleFrame ? MB_ICONINFORMATION : MB_ICONWARNING;
                MaxMsgBox( GetCOREInterface()->GetMAXHWnd(), msg.c_str(), _T("XMesh Loader Range"),
                           MB_OK | icon | MB_APPLMODAL );
            }
        } else if( notify ) {
            // error: no files in sequence
            const frantic::tstring msg =
                _T("Cannot determine a safe range.  No files were found in the sequence:\n") + filename;
            MaxMsgBox( GetCOREInterface()->GetMAXHWnd(), msg.c_str(), _T("XMesh Loader Range"),
                       MB_OK | MB_ICONWARNING );
        }
    }
}

/**
 * Reset the cached mesh info and force a reload immediately
 */
void XMeshLoader::invalidate() {
    invalidate_cache();

    ivalid.SetEmpty();
    NotifyEveryone();
}

void XMeshLoader::clear_trimesh3_cache() {
    m_polymesh3Loader.clear_cache();
    m_cachedPolymesh3.reset();
    m_cachedPolymesh3IsTriangles = true;
    m_cachedPolymesh3Interval.first.reset();
    m_cachedPolymesh3Interval.second.reset();
    invalidate_cache();
}

/**
 * Clears all cached mesh data, including the max mesh.
 */
void XMeshLoader::clear_cache() {
    clear_mesh( mesh );
    clear_polymesh( mm );
    clear_trimesh3_cache();
}

void XMeshLoader::clear_cache_mxs() {
    try {
        invalidate_sequences();
        clear_cache();
        invalidate();
    } catch( std::exception& e ) {
        frantic::tstring errmsg = _T("ClearCache: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        FF_LOG( error ) << errmsg << std::endl;
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

void XMeshLoader::set_to_valid_frame_range_mxs() {
    set_to_valid_frame_range( false, true );
    invalidate();
}

int XMeshLoader::get_current_sequence_id() {
    int seqInt;

    if( !m_inRenderingMode || m_renderUsingViewportSettings.at_time( 0 ) ) {
        seqInt = m_viewportSequenceID.at_time( 0 );
    } else {
        seqInt = m_renderSequenceID.at_time( 0 );
    }

    return ( seqInt == SEQUENCE_ID::RENDER ) ? ( SEQUENCE_ID::RENDER ) : ( SEQUENCE_ID::PROXY );
}

int XMeshLoader::get_render_sequence_id() {
    int seqInt;

    if( m_renderUsingViewportSettings.at_time( 0 ) ) {
        seqInt = m_viewportSequenceID.at_time( 0 );
    } else {
        seqInt = m_renderSequenceID.at_time( 0 );
    }

    return ( seqInt == SEQUENCE_ID::RENDER ) ? ( SEQUENCE_ID::RENDER ) : ( SEQUENCE_ID::PROXY );
}

frantic::tstring XMeshLoader::get_sequence_path( int seqId ) {
    if( seqId == SEQUENCE_ID::PROXY ) {
        if( m_autogenProxyPath.at_time( 0 ) ) {
            return get_auto_proxy_path();
        } else {
            return get_proxy_path();
        }
    } else {
        return get_render_path();
    }
}

frantic::files::filename_sequence& XMeshLoader::get_sequence( int seqId, bool throwIfMissing ) {
    const frantic::tstring seqPath = get_sequence_path( seqId );
    if( m_fileSequencePaths[seqId] != seqPath ) {
        if( seqPath.empty() ) {
            m_fileSequences[seqId] = frantic::files::filename_sequence();
        } else {
            m_fileSequences[seqId] = frantic::files::filename_sequence( seqPath );
            bool success = false;
            std::string errorMessage;
            try {
                m_fileSequences[seqId].sync_frame_set();
                success = true;
            } catch( std::exception& e ) {
                errorMessage = e.what();
            }
            if( !success ) {
                // this clear is probably unnecessary
                m_fileSequences[seqId].get_frame_set().clear();
                if( throwIfMissing ) {
                    throw std::runtime_error( errorMessage + "\nPath: " + frantic::strings::to_string( seqPath ) );
                }
            }
        }
        m_fileSequencePaths[seqId] = seqPath;
    }
    return m_fileSequences[seqId];
}

frantic::tstring XMeshLoader::get_current_sequence_path() { return get_sequence_path( get_current_sequence_id() ); }

frantic::tstring XMeshLoader::get_render_sequence_path() { return get_sequence_path( get_render_sequence_id() ); }

frantic::files::filename_sequence& XMeshLoader::get_current_sequence( bool throwIfMissing ) {
    return get_sequence( get_current_sequence_id(), throwIfMissing );
}

frantic::files::filename_sequence& XMeshLoader::get_render_sequence() {
    return get_sequence( get_render_sequence_id() );
}

frantic::tstring XMeshLoader::get_auto_proxy_path() {
    const frantic::tstring renderPath = get_render_path();
    if( renderPath.empty() ) {
        return _T("");
    }
    const frantic::tstring proxyFilenameInRenderDirectory =
        frantic::files::filename_pattern::add_before_sequence_number( renderPath, _T("_proxy") );
    const frantic::tstring proxyFilename = frantic::files::filename_from_path( proxyFilenameInRenderDirectory );
    return frantic::files::to_tstring( boost::filesystem::path( get_auto_proxy_directory() ) / proxyFilename );
}

frantic::tstring XMeshLoader::get_auto_proxy_directory() {
    const frantic::tstring renderPath = get_render_path();

    if( renderPath.empty() ) {
        return _T("");
    }

    frantic::files::filename_sequence renderSequence( renderPath );
    const frantic::tstring renderPrefix = renderSequence.get_filename_pattern().get_prefix();
    const frantic::tstring proxyDirPrefix =
        renderPrefix + /*( boost::algorithm::iends_with( renderPrefix, "_" ) ? "" : "_" ) */ +_T("_proxy");
    return renderSequence.get_filename_pattern().get_directory( true ) + proxyDirPrefix;
}

void XMeshLoader::check_auto_proxy_path() {
    const frantic::tstring proxyPath = get_auto_proxy_path();

    if( proxyPath.empty() ) {
        return;
    }

    const frantic::tstring proxyDir = frantic::files::directory_from_path( proxyPath );

    if( !frantic::files::directory_exists( proxyDir ) ) {
        const frantic::tstring msg = _T("Missing automatic proxy path:\n\n") + proxyPath;
        MaxMsgBox( GetCOREInterface()->GetMAXHWnd(), msg.c_str(), _T("XMesh Loader - Automatic Proxy Path"),
                   MB_OK | MB_ICONWARNING | MB_APPLMODAL );
        return;
    }

    bool hasSequence = false;
    try {
        frantic::files::filename_sequence seq( proxyPath );
        seq.sync_frame_set();
        hasSequence = !seq.get_frame_set().empty();
    } catch( std::exception& ) {
    }

    if( !hasSequence ) {
        if( !frantic::files::file_exists( proxyPath ) ) {
            const frantic::tstring msg = _T("Missing automatic proxy path:\n\n") + proxyPath;
            MaxMsgBox( GetCOREInterface()->GetMAXHWnd(), msg.c_str(), _T("XMesh Loader - Automatic Proxy Path"),
                       MB_OK | MB_ICONWARNING | MB_APPLMODAL );
            return;
        }
    }
}

void XMeshLoader::report_warning( const frantic::tstring& msg ) {
    try {
        FF_LOG( warning ) << msg << std::endl;
    } catch( std::exception& /*e*/ ) {
    }
}

void XMeshLoader::report_error( const frantic::tstring& msg ) {
    try {
        FF_LOG( error ) << msg << std::endl;
    } catch( std::exception& /*e*/ ) {
    }
}

void XMeshLoader::SetRenderTime( TimeValue t ) { m_renderTime = t; }

void XMeshLoader::ClearRenderTime() { m_renderTime = TIME_NegInfinity; }

void XMeshLoader::SetEmptyValidityAndNotifyDependents() {
    ivalid.SetEmpty();
    NotifyEveryone();
}

void XMeshLoader::on_param_set( PB2Value& /*v*/, ParamID id, int /*tabIndex*/, TimeValue t ) {
    switch( id ) {
    case pb_showIcon:
        break;
    case pb_iconSize:
        break;
    case pb_meshScale:
        invalidate();
        break;
    case pb_keepMeshInMemory:
        if( !m_keepMeshInMemory.at_time( t ) ) {
            clear_trimesh3_cache();
        }
        break;
    case pb_writeVelocityMapChannel:
        invalidate();
        break;
    case pb_velocityMapChannel:
        if( m_outputVelocityMapChannel.at_time( t ) ) {
            invalidate();
        }
        break;
    case pb_renderSequence:
        if( editObj == this && pblock2 ) {
            IParamMap2* pm = pblock2->GetMap( files_param_map );
            if( pm ) {
                HWND hwnd = pm->GetHWnd();
                GetFilesDlgProc()->InvalidateUI( hwnd, id );
            }
        }
        if( get_current_sequence_id() == SEQUENCE_ID::RENDER ) {
            invalidate_sequences();
            clear_cache();
        }
        invalidate();
        break;
    case pb_proxySequence:
        if( editObj == this && pblock2 ) {
            IParamMap2* pm = pblock2->GetMap( files_param_map );
            if( pm ) {
                HWND hwnd = pm->GetHWnd();
                GetFilesDlgProc()->InvalidateUI( hwnd, id );
            }
        }
        if( get_current_sequence_id() == SEQUENCE_ID::PROXY ) {
            invalidate_sequences();
            clear_cache();
        }
        invalidate();
        break;
    case pb_autogenProxyPath:
        break;
    case pb_loadSingleFrame:
        invalidate();
        break;
    case pb_frameOffset:
        invalidate();
        break;
    case pb_limitToRange:
        invalidate();
        break;
    case pb_rangeFirstFrame:
        if( m_limitToRange.at_time( t ) ) {
            invalidate();
        }
        break;
    case pb_rangeLastFrame:
        if( m_limitToRange.at_time( t ) ) {
            invalidate();
        }
        break;
    case pb_enablePlaybackGraph:
        invalidate();
        break;
    case pb_playbackGraphTime:
        if( m_enablePlaybackGraph.at_time( t ) ) {
            invalidate();
        }
        break;
    case pb_beforeRangeBehavior:
        if( editObj == this && pblock2 ) {
            IParamMap2* pm = pblock2->GetMap( files_param_map );
            if( pm ) {
                HWND hwnd = pm->GetHWnd();
                GetFilesDlgProc()->InvalidateUI( hwnd, id );
            }
        }
        if( m_limitToRange.at_time( t ) ) {
            invalidate();
        }
        break;
    case pb_afterRangeBehavior:
        if( editObj == this && pblock2 ) {
            IParamMap2* pm = pblock2->GetMap( files_param_map );
            if( pm ) {
                HWND hwnd = pm->GetHWnd();
                GetFilesDlgProc()->InvalidateUI( hwnd, id );
            }
        }
        if( m_limitToRange.at_time( t ) ) {
            invalidate();
        }
        break;
    case pb_loadMode:
        if( editObj == this && pblock2 ) {
            IParamMap2* pm = pblock2->GetMap( files_param_map );
            if( pm ) {
                HWND hwnd = pm->GetHWnd();
                GetFilesDlgProc()->InvalidateUI( hwnd, id );
            }
        }
        invalidate();
        break;
    case pb_enableViewportMesh:
        invalidate();
        break;
    case pb_enableRenderMesh:
        break;
    case pb_renderSequenceID:
        if( editObj == this && pblock2 ) {
            IParamMap2* pm = pblock2->GetMap( loading_param_map );
            if( pm ) {
                HWND hwnd = pm->GetHWnd();
                GetLoadingDlgProc()->InvalidateUI( hwnd, id );
            }
        }
        break;
    case pb_viewportSequenceID:
        if( editObj == this && pblock2 ) {
            IParamMap2* pm = pblock2->GetMap( loading_param_map );
            if( pm ) {
                HWND hwnd = pm->GetHWnd();
                GetLoadingDlgProc()->InvalidateUI( hwnd, id );
            }
        }
        invalidate();
        break;
    case pb_renderUsingViewportSettings:
        break;
    case pb_displayMode:
        if( editObj == this && pblock2 ) {
            IParamMap2* pm = pblock2->GetMap( loading_param_map );
            if( pm ) {
                HWND hwnd = pm->GetHWnd();
                GetLoadingDlgProc()->InvalidateUI( hwnd, id );
            }
        }
        invalidate();
        break;
    case pb_displayPercent:
        invalidate();
        break;
    case pb_useFileLengthUnit:
        invalidate();
        break;
    case pb_lengthUnit:
        if( editObj == this && pblock2 ) {
            IParamMap2* pm = pblock2->GetMap( files_param_map );
            if( pm ) {
                HWND hwnd = pm->GetHWnd();
                GetFilesDlgProc()->InvalidateUI( hwnd, id );
            }
        }
        invalidate();
        break;
    }
}

TimeValue XMeshLoader::GetRenderTime() const { return m_renderTime; }

bool XMeshLoader::HasValidRenderTime() const { return m_renderTime != TIME_NegInfinity; }

int XMeshLoader::get_current_display_mode() {
    const int displayMode = m_inRenderingMode ? ( DISPLAY_MODE::MESH ) : (int)( m_displayMode.at_time( 0 ) );
    return displayMode;
}

bool XMeshLoader::has_triangle_mesh() { return m_cachedPolymesh3IsTriangles; }

void XMeshLoader::build_normals() {
    if( !m_doneBuildNormals ) {
        if( has_triangle_mesh() ) {
            mesh.buildNormals();
        } else {
            mm.buildNormals();
        }
        m_doneBuildNormals = true;
    }
}

// copied from Frost
double XMeshLoader::get_system_scale_checked( int type ) {
    double scale = frantic::max3d::get_scale( type );
    if( scale < 0 ) {
        throw std::runtime_error( "get_system_scale_checked: Unrecognized unit type code (" +
                                  boost::lexical_cast<std::string>( type ) + ")" );
    } else {
        return scale;
    }
}

double XMeshLoader::get_to_max_scale_factor( double scale,
                                             frantic::geometry::xmesh_metadata::length_unit_t lengthUnit ) {
    switch( lengthUnit ) {
    case frantic::geometry::xmesh_metadata::length_unit_unitless:
        return scale;
    case frantic::geometry::xmesh_metadata::length_unit_inches:
        return scale / get_system_scale_checked( UNITS_INCHES );
    case frantic::geometry::xmesh_metadata::length_unit_feet:
        return scale / get_system_scale_checked( UNITS_FEET );
    case frantic::geometry::xmesh_metadata::length_unit_miles:
        return scale / get_system_scale_checked( UNITS_MILES );
    case frantic::geometry::xmesh_metadata::length_unit_millimeters:
        return scale / get_system_scale_checked( UNITS_MILLIMETERS );
    case frantic::geometry::xmesh_metadata::length_unit_centimeters:
        return scale / get_system_scale_checked( UNITS_CENTIMETERS );
    case frantic::geometry::xmesh_metadata::length_unit_meters:
        return scale / get_system_scale_checked( UNITS_METERS );
    case frantic::geometry::xmesh_metadata::length_unit_kilometers:
        return scale / get_system_scale_checked( UNITS_KILOMETERS );
    default:
        throw std::runtime_error( "get_to_max_scale: Unknown unit: " + boost::lexical_cast<std::string>( lengthUnit ) );
    }
}

double XMeshLoader::calculate_mesh_scale_factor() {
    if( m_useFileLengthUnit.at_time( 0 ) ) {
        if( m_metadata.has_length_unit() &&
            m_metadata.get_length_unit() != frantic::geometry::xmesh_metadata::length_unit_unitless ) {
            return get_to_max_scale_factor( m_metadata.get_length_unit_scale(), m_metadata.get_length_unit() );
        } else {
            throw std::runtime_error(
                "Error: \'Use File Unit\' is enabled, but the input file does not have a length unit." );
        }
    } else {
        const int unit = m_lengthUnit.at_time( 0 );

        switch( unit ) {
        case LENGTH_UNIT::GENERIC:
            return 1.0;
        case LENGTH_UNIT::INCHES:
            return 1.0 / get_system_scale_checked( UNITS_INCHES );
        case LENGTH_UNIT::FEET:
            return 1.0 / get_system_scale_checked( UNITS_FEET );
        case LENGTH_UNIT::MILES:
            return 1.0 / get_system_scale_checked( UNITS_MILES );
        case LENGTH_UNIT::MILLIMETERS:
            return 1.0 / get_system_scale_checked( UNITS_MILLIMETERS );
        case LENGTH_UNIT::CENTIMETERS:
            return 1.0 / get_system_scale_checked( UNITS_CENTIMETERS );
        case LENGTH_UNIT::METERS:
            return 1.0 / get_system_scale_checked( UNITS_METERS );
        case LENGTH_UNIT::KILOMETERS:
            return 1.0 / get_system_scale_checked( UNITS_KILOMETERS );
        case LENGTH_UNIT::CUSTOM:
            return m_meshScale.at_time( 0 );
        default:
            throw std::runtime_error( "calculate_mesh_scale_factor: Unrecognized length unit: " + unit );
        }
    }
}

frantic::tstring XMeshLoader::get_loading_frame_info( TimeValue t ) {
    if( m_loadSingleFrame.at_time( t ) ) {
        return _T("File");
    } else {
        const TimeValue tpf = GetTicksPerFrame();
        TimeValue realTime;
        float timeDerivative;
        bool isEmptyMeshTime;
        get_timing_data( t, tpf / 4, realTime, timeDerivative, isEmptyMeshTime );
        if( isEmptyMeshTime ) {
            return _T("Blank");
        } else {
            const double frameNumber = (double)realTime / GetTicksPerFrame();
            const double roundedFrameNumber = frantic::math::round( frameNumber );
            const std::size_t bufferSize = 32;
            TCHAR buffer[bufferSize] = { 0 };
            if( roundedFrameNumber == frameNumber ) {
                _sntprintf_s( buffer, bufferSize, _TRUNCATE, _T("%d"), static_cast<int>( roundedFrameNumber ) );
            } else {
                _sntprintf_s( buffer, bufferSize, _TRUNCATE, _T("%.2f"), frameNumber );
            }
            return buffer;
        }
    }
}

void XMeshLoader::display_loading_frame_info() {
    try {
        if( editObj == this ) {
            GetFilesDlgProc()->update_loading_frame_info();
        }
    } catch( std::exception& e ) {
        frantic::tstring errmsg =
            _T("display_loading_frame_info: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        FF_LOG( error ) << errmsg << std::endl;
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

void XMeshLoader::PostLoadCallback( ILoad* /*iload*/ ) {
    IParamBlock2PostLoadInfo* postLoadInfo =
        (IParamBlock2PostLoadInfo*)pblock2->GetInterface( IPARAMBLOCK2POSTLOADINFO_ID );

    if( postLoadInfo != NULL && postLoadInfo->GetVersion() == 0 ) {
        float meshScale = pblock2->GetFloat( pb_meshScale, 0, 0 );
        if( meshScale != 1.f ) {
            pblock2->SetValue( pb_lengthUnit, 0, LENGTH_UNIT::CUSTOM, 0 );
        }
    }
}

frantic::tstring XMeshLoader::get_node_name() {
    frantic::tstring name = _T("<error>");

    ULONG handle = 0;
    this->NotifyDependents( FOREVER, (PartID)&handle, REFMSG_GET_NODE_HANDLE );

    INode* node = 0;
    if( handle ) {
        node = GetCOREInterface()->GetINodeByHandle( handle );
    }
    if( node ) {
        const TCHAR* pName = node->GetName();
        if( pName ) {
            name = pName;
        } else {
            name = _T("<null>");
        }
    } else {
        name = _T("<missing>");
    }
    return name;
}

//---------------------------------------------------------------------------
// Function publishing setup

void XMeshLoader::InitializeFPDescriptor() {
    FFCreateDescriptor c( this, XMeshLoader_INTERFACE_ID, _T("XMeshLoaderInterface"), GetXMeshLoaderClassDesc() );

    c.add_function( &XMeshLoader::clear_cache_mxs, _T("ClearCache") );
    c.add_function( &XMeshLoader::set_to_valid_frame_range_mxs, _T("SetToValidFrameRange") );
}
