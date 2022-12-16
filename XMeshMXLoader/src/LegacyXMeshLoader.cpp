// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/graphics/color3f.hpp>
#include <frantic/max3d/geometry/mesh.hpp>
#include <frantic/max3d/geopipe/named_selection_sets.hpp>
#include <frantic/max3d/max_utility.hpp>

#include <frantic/math/utils.hpp>

#include <frantic/geometry/trimesh3.hpp>
#include <frantic/geometry/trimesh3_file_io.hpp>

#include <boost/filesystem/exception.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/numeric/conversion/bounds.hpp>

#include <frantic/files/filename_sequence.hpp>

#include <iostream>

#include <algorithm>

#if MAX_VERSION_MAJOR >= 14
#include <Graphics/IDisplayManager.h>
#endif

#include "LegacyXMeshLoader.hpp"
#include "build_icon_mesh.hpp"

/* namespaces */
using namespace std;
using namespace boost;
using namespace frantic;
using namespace frantic::geometry;
using namespace frantic::files;
using namespace frantic::graphics;
using namespace frantic::max3d;
using namespace frantic::max3d::geometry;
using namespace frantic::channels;
using namespace frantic::geometry;

extern HINSTANCE ghInstance;

boost::shared_ptr<Mesh> LegacyXMeshLoader::m_pIconMesh = BuildLegacyIconMesh();

/**
 *  Used for the beforeRangeMode and afterRangeMode.
 */
namespace clamp_mode {
enum clamp_mode_enum {
    /**
     *  Fill outside of the defined range by repeating the nearest defined frame.
     */
    hold = 1,
    /**
     *  Fill outside of the defined range by using an empty levelset.
     */
    blank
};
}

// The class descriptor for LegacyXMeshLoader
class LegacyXMeshLoaderDesc : public ClassDesc2 {
  public:
    int IsPublic() { return FALSE; }
    void* Create( BOOL /*loading = FALSE*/ ) { return new LegacyXMeshLoader(); }
#if MAX_VERSION_MAJOR >= 24
    const TCHAR* NonLocalizedClassName() { return _T( LegacyXMeshLoader_CLASS_NAME ); }
#endif
    const TCHAR* ClassName() { return _T( LegacyXMeshLoader_CLASS_NAME ); }
    SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() { return LegacyXMeshLoader_CLASS_ID; }
    const TCHAR* Category() { return _T("Thinkbox"); }

    // returns fixed parsable name (scripter-visible name)
    const TCHAR* InternalName() { return _T( LegacyXMeshLoader_CLASS_NAME ); }
    // returns owning module handle
    HINSTANCE HInstance() { return ghInstance; }
};

ClassDesc* GetLegacyXMeshLoaderClassDesc() {
    static LegacyXMeshLoaderDesc theDesc;
    return &theDesc;
}

static void notify_render_preeval( void* param, NotifyInfo* info ) {
    LegacyXMeshLoader* pMeshLoader = (LegacyXMeshLoader*)param;
    TimeValue* pTime = (TimeValue*)info->callParam;
    if( pMeshLoader && pTime ) {
        pMeshLoader->SetRenderTime( *pTime );
        pMeshLoader->SetEmptyValidityAndNotifyDependents();
    }
}

static void notify_post_renderframe( void* param, NotifyInfo* info ) {
    LegacyXMeshLoader* pMeshLoader = (LegacyXMeshLoader*)param;
    RenderGlobalContext* pContext = (RenderGlobalContext*)info->callParam;
    if( pMeshLoader && pContext ) {
        pMeshLoader->ClearRenderTime();
    }
}

// --- Required Implementations ------------------------------

// Constructor
LegacyXMeshLoader::LegacyXMeshLoader()
    : m_loadMode( m_parent, _T("loadMode") )
    , m_showIcon( m_parent, _T("showIcon") )
    , m_iconSize( m_parent, _T("iconSize") )
    , m_keepMeshInMemory( m_parent, _T("keepMeshInMemory") )
    , m_enablePlaybackGraph( m_parent, _T("enablePlaybackGraph") )
    , m_playbackGraphTime( m_parent, _T("playbackGraphTime") )
    , m_meshScale( m_parent, _T("meshScale") )
    , m_loadSingleFrame( m_parent, _T("loadSingleFrame") )
    , m_frameOffset( m_parent, _T("frameOffset") )
    , m_limitToRange( m_parent, _T("limitToRange") )
    , m_startFrame( m_parent, _T("startFrame") )
    , m_endFrame( m_parent, _T("endFrame") )
    , m_beforeRangeMode( m_parent, _T("beforeRangeBehavior") )
    , m_afterRangeMode( m_parent, _T("afterRangeBehavior") )
    ,

    m_renderPath( m_parent, _T("renderSequenceName") )
    , m_proxyPath( m_parent, _T("proxySequenceName") )
    , m_renderSequenceID( m_parent, _T("renderSequenceID") )
    , m_viewportSequenceID( m_parent, _T("viewportSequenceID") )
    , m_autogenProxyPath( m_parent, _T("autogenProxyPath") )
    ,

    m_useViewportSettings( m_parent, _T("useViewportSettings") )
    , m_disableLoading( m_parent, _T("disableLoading") )
    ,

    m_velToMapChannel( m_parent, _T("velocitiesToMapChannel") )
    , m_outputVelocityMapChannel( m_parent, _T("outputVelocityMapChannel") )
    ,

    m_inRenderingMode( false )
    , m_renderTime( TIME_NegInfinity )
    , m_cachedMeshInRenderingMode( false ) {
    m_fileSequences.resize( SEQ_COUNT );

    invalidate_cache();

    InitializeFPDescriptor();

    RegisterNotification( notify_render_preeval, (void*)this, NOTIFY_RENDER_PREEVAL );
    RegisterNotification( notify_post_renderframe, (void*)this, NOTIFY_POST_RENDERFRAME );
}

LegacyXMeshLoader::~LegacyXMeshLoader() {
    UnRegisterNotification( notify_post_renderframe, (void*)this, NOTIFY_POST_RENDERFRAME );
    UnRegisterNotification( notify_render_preeval, (void*)this, NOTIFY_RENDER_PREEVAL );

    DeleteAllRefsFromMe();
};

RefTargetHandle LegacyXMeshLoader::Clone( RemapDir& remap ) {
    LegacyXMeshLoader* newob = new LegacyXMeshLoader();
    BaseClone( this, newob, remap );
    return ( newob );
}

int LegacyXMeshLoader::NumInterfaces() { return 1; }
FPInterface* LegacyXMeshLoader::GetInterface( int i ) {
    if( i == 0 )
        return GetInterface( LegacyXMeshLoader_INTERFACE_ID );
    return NULL;
}

FPInterface* LegacyXMeshLoader::GetInterface( Interface_ID id ) {
    if( id == LegacyXMeshLoader_INTERFACE_ID )
        return static_cast<frantic::max3d::fpwrapper::FFMixinInterface<LegacyXMeshLoader>*>( this );
    return NULL;
}

// Display, GetWorldBoundBox, GetLocalBoundBox are virtual methods of SimpleObject2 that must be implemented.
// HitTest is optional.
void LegacyXMeshLoader::GetWorldBoundBox( TimeValue time, INode* inode, ViewExp* vpt, Box3& box ) {
    GetLocalBoundBox( time, inode, vpt, box );
    box = box * inode->GetNodeTM( time );
}

void LegacyXMeshLoader::GetLocalBoundBox( TimeValue time, INode* inode, ViewExp* /*vpt*/, Box3& box ) {
    try {

        // renderable only evaluates to false if it is at render time and the flag is off
        if( !inode->Renderable() )
            m_cachedTrimesh3.clear_and_deallocate();
        else
            BuildMesh( time );

        box = mesh.getBoundingBox();
        if( m_showIcon.at_time( time ) ) {
            // Compute the world-space scaled bounding box
            float scale = m_iconSize.at_time( time );
            Matrix3 iconMatrix( 1 );
            iconMatrix.Scale( Point3( scale, scale, scale ) );
            box += m_pIconMesh->getBoundingBox( &iconMatrix );
        }

    } catch( const std::exception& e ) {
        frantic::tstring errmsg =
            _T("LegacyXMeshLoader::GetLocalBoundBox: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        mprintf( _T("%s"), errmsg.c_str() );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

int LegacyXMeshLoader::Display( TimeValue time, INode* inode, ViewExp* pView, int flags ) {
    try {
        if( !inode || !pView )
            return 0;

        if( inode->IsNodeHidden() ) {
            return TRUE;
        }

        BuildMesh( time );

        // Let the SimpleObject2 display code do its thing
#if MAX_VERSION_MAJOR >= 14
        if( !MaxSDK::Graphics::IsRetainedModeEnabled() ) {
#else
        {
#endif
            SimpleObject2::Display( time, inode, pView, flags );
        }

        GraphicsWindow* gw = pView->getGW();
        DWORD rndLimits = gw->getRndLimits();

        gw->setRndLimits( GW_Z_BUFFER | GW_WIREFRAME );
        gw->setTransform( inode->GetNodeTM( time ) );

        // Render the icon if necessary
        if( m_showIcon.at_time( time ) ) {
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
        frantic::tstring errmsg =
            _T("LegacyXMeshLoader::Display: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        mprintf( _T("%s"), errmsg.c_str() );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );

        return 0;
    }
}

int LegacyXMeshLoader::HitTest( TimeValue time, INode* inode, int type, int crossing, int flags, IPoint2* p,
                                ViewExp* vpt ) {
    try {
        GraphicsWindow* gw = vpt->getGW();

        if( ( ( flags & HIT_SELONLY ) && !inode->Selected() ) || gw == NULL || inode == NULL )
            return 0;

        const DWORD rndLimits = gw->getRndLimits();

        if( gw->getRndMode() & GW_BOX_MODE ) {
            gw->setRndLimits( rndLimits & ~( DWORD( GW_ILLUM | GW_FLAT | GW_TEXTURE | GW_CONSTANT ) ) | GW_PICK );

            Matrix3 tm = inode->GetNodeTM( time );
            gw->setTransform( tm );

            HitRegion hitRegion;
            MakeHitRegion( hitRegion, type, crossing, 4, p );
            gw->setHitRegion( &hitRegion );
            gw->clearHitCode();

            Box3 box;
            GetLocalBoundBox( time, inode, vpt, box );
            const int hit = frantic::max3d::hit_test_box( gw, box, hitRegion, flags & HIT_ABORTONHIT );
            gw->setRndLimits( rndLimits );
            return hit;
        }

        // Let the SimpleObject2 hit test code do its thing
        if( SimpleObject2::HitTest( time, inode, type, crossing, flags, p, vpt ) )
            return 1;

        HitRegion hitRegion;
        MakeHitRegion( hitRegion, type, crossing, 4, p );

        gw->setRndLimits( rndLimits & ~( DWORD( GW_ILLUM | GW_FLAT | GW_TEXTURE | GW_CONSTANT ) ) | GW_WIREFRAME |
                          GW_PICK );
        gw->setHitRegion( &hitRegion );
        gw->clearHitCode();

        // Hit test against the icon if necessary
        if( m_showIcon.at_time( time ) ) {
            Matrix3 iconMatrix = inode->GetNodeTM( time );
            float f = m_iconSize.at_time( time );
            iconMatrix.Scale( Point3( f, f, f ) );

            gw->setTransform( iconMatrix );
            if( m_pIconMesh->select( gw, NULL, &hitRegion ) )
                return true;
        }

        return false;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg =
            _T("LegacyXMeshLoader::HitTest: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        mprintf( _T("%s"), errmsg.c_str() );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );

        return false;
    }
}

#if MAX_VERSION_MAJOR >= 17

unsigned long LegacyXMeshLoader::GetObjectDisplayRequirement() const {
    return MaxSDK::Graphics::ObjectDisplayRequireLegacyDisplayMode;
}

#elif MAX_VERSION_MAJOR >= 14

bool LegacyXMeshLoader::RequiresSupportForLegacyDisplayMode() const { return true; }

#endif

#if MAX_VERSION_MAJOR >= 15 && MAX_VERSION_MAJOR < 17

bool LegacyXMeshLoader::UpdateDisplay( const MaxSDK::Graphics::MaxContext& /*maxContext*/,
                                       const MaxSDK::Graphics::UpdateDisplayContext& displayContext ) {
    try {
        BuildMesh( displayContext.GetDisplayTime() );

        if( mesh.getNumVerts() > 0 ) {
            MaxSDK::Graphics::GenerateMeshRenderItemsContext generateRenderItemsContext;
            generateRenderItemsContext.GenerateDefaultContext( displayContext );
            mesh.GenerateRenderItems( mRenderItemHandles, generateRenderItemsContext );
            return true;
        }
    } catch( const std::exception& e ) {
        frantic::tstring errmsg =
            _T("LegacyXMeshLoader::UpdateDisplay: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        mprintf( _T("%s"), errmsg.c_str() );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( frantic::max3d::is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
    return false;
}

#elif MAX_VERSION_MAJOR == 14

bool LegacyXMeshLoader::UpdateDisplay( unsigned long renderItemCategories,
                                       const MaxSDK::Graphics::MaterialRequiredStreams& materialRequiredStreams,
                                       TimeValue t ) {
    try {
        BuildMesh( t );

        MaxSDK::Graphics::GenerateRenderItems( mRenderItemHandles, &mesh, renderItemCategories,
                                               materialRequiredStreams );

        return true;
    } catch( const std::exception& e ) {
        frantic::tstring errmsg =
            _T("LegacyXMeshLoader::UpdateDisplay: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        mprintf( _T("%s"), errmsg.c_str() );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
    return false;
}

#endif

int LegacyXMeshLoader::RenderBegin( TimeValue /*t*/, ULONG flags ) {
    try {
        // Only switch to rendering mode if it's not in the material editor
        if( ( flags & RENDERBEGIN_IN_MEDIT ) == 0 ) {
            m_inRenderingMode = true;
            ivalid = NEVER;
            invalidate();
        }
    } catch( const std::exception& e ) {
        mprintf( _T("LegacyXMeshLoader.RenderBegin: %s\n"), frantic::strings::to_tstring( e.what() ).c_str() );
    }
    return 1;
}

int LegacyXMeshLoader::RenderEnd( TimeValue /*t*/ ) {
    m_inRenderingMode = false;
    invalidate();
    return 1;
}

//--- MeshLoader methods -------------------------------

/**
 * Creates the mesh for MAX to display for the specified time
 *
 * @param time the time to build a mesh for
 */
void LegacyXMeshLoader::BuildMesh( TimeValue t ) {

    // If we've got a valid mesh, just return.
    if( ivalid.InInterval( t ) )
        return;
    ivalid.SetInstant( t );

    if( m_parent.get_target() == NULL )
        return;

    try {
        // If we're drawing the viewport and loading is disabled, build an empty mesh (ignore disableLoading at render
        // time) and return
        if( ( m_disableLoading.at_time( t ) && !m_inRenderingMode ) ) {
            m_cachedTrimesh3.clear_and_deallocate();
            clear_mesh( mesh );
            return;
        }

        frantic::tstring loadMode = m_loadMode.at_time( t );

        // If we are only loading a single frame, load what's in the sequence name edit text box
        if( m_loadSingleFrame.at_time( t ) ) {

            if( m_cachedSequenceID != current_sequence_id() || m_cachedLoadingMode != loadMode ||
                m_cachedMeshInRenderingMode != m_inRenderingMode ) {

                frantic::tstring filename( m_renderPath.at_time( t ).operator const frantic::strings::tstring() );

                if( current_sequence_id() == PROXY_SEQ )
                    filename = m_proxyPath.at_time( t ).operator const frantic::strings::tstring();

                load_mesh_file( filename, m_cachedTrimesh3 );

                m_cachedTrimesh3.scale( m_meshScale.at_time( t ) );

                mesh_copy( mesh, m_cachedTrimesh3 );

                m_cachedTime = t;
                m_cachedSequenceID = current_sequence_id();
                m_cachedLoadingMode = loadMode;
                m_cachedMeshInRenderingMode = m_inRenderingMode;
            }
            return;
        }

        TimeValue tpf = GetTicksPerFrame();
        TimeValue realTime;

        // Pick the correct file sequence
        frantic::files::filename_sequence& frameSequence( current_sequence() );

        // if the file cache is empty, clear the mesh and return
        if( frameSequence.get_frame_set().empty() ) {
            m_cachedTrimesh3.clear_and_deallocate();
            clear_mesh( mesh );
            return;
        }

        float timeDerivative;
        bool invalidCache = false;
        bool isEmptyMeshTime;

        // Depending on the loading mode, the cached information works differently

        // These load modes find the nearest mesh to the query time in the underlying sequence, and load it
        // offsetting the position appropriately according to the velocity.
        if( loadMode == _T("Frame Velocity Offset") || loadMode == _T("Nearest Frame Velocity Offset") ||
            loadMode == _T("Subframe Velocity Offset") ) {

            TimeValue requestTime = t;

            // This mode clamps to the nearest whole frame of the requested time, and loads the mesh for
            // that whole frame.  The loaded mesh is then offset to the appropriate requested time using any
            // existing velocity channel data.  This means that the resulting meshes for a sequence will
            // always have consistent topology over the frame interval [t-.5,t+.5).
            if( loadMode == _T("Frame Velocity Offset") || loadMode == _T("Nearest Frame Velocity Offset") ) {
                requestTime = round_to_nearest_wholeframe( t );
                if( loadMode == _T("Frame Velocity Offset") && HasValidRenderTime() ) {
                    requestTime = round_to_nearest_wholeframe( GetRenderTime() );
                }
            }

            // Get nearest mesh to the current requested time (playback graph considered)
            get_timing_data( requestTime, tpf / 4, realTime, timeDerivative, isEmptyMeshTime );

            if( isEmptyMeshTime ) {
                const TimeValue invalidTime = ( std::numeric_limits<TimeValue>::min )();

                invalidCache = true;
                m_cachedTime = invalidTime;
                m_cachedTrimesh3.clear_and_deallocate();
                mesh_copy( mesh, m_cachedTrimesh3 );
            } else {
                double frameNumber;
                if( !get_nearest_subframe( realTime, frameNumber ) )
                    throw std::runtime_error(
                        "An appropriate frame to offset for time " +
                        boost::lexical_cast<std::string>( float( realTime ) / GetTicksPerFrame() ) +
                        " could not be found in the sequence." );

                // mprintf("requested time: %f  closest sample: %f\n", float(realTime)/GetTicksPerFrame(), frameNumber);

                TimeValue sampleTime = (TimeValue)( frameNumber * GetTicksPerFrame() );

                // Check the cache and rebuild if necessary
                if( loadMode != m_cachedLoadingMode || sampleTime != m_cachedTime ||
                    current_sequence_id() != m_cachedSequenceID ) {
                    invalidCache = true;
                    m_cachedTime = sampleTime;
                    load_mesh_at_frame( frameNumber );
                    m_cachedTrimesh3.scale( m_meshScale.at_time( t ) );
                    scale_cached_velocity_channel( timeDerivative );
                }

                if( m_velToMapChannel.at_time( t ) && m_cachedTrimesh3.has_vertex_channel( _T("Velocity") ) ) {
                    frantic::tstring channelName =
                        _T("Mapping") + lexical_cast<frantic::tstring>( m_outputVelocityMapChannel.at_time( t ) );

                    if( !m_cachedTrimesh3.has_vertex_channel( channelName ) )
                        m_cachedTrimesh3.add_vertex_channel<vector3f>( channelName );

                    trimesh3_vertex_channel_accessor<vector3f> toAcc =
                        m_cachedTrimesh3.get_vertex_channel_accessor<vector3f>( channelName );
                    trimesh3_vertex_channel_accessor<vector3f> fromAcc =
                        m_cachedTrimesh3.get_vertex_channel_accessor<vector3f>( _T("Velocity") );

                    memcpy( &toAcc[0], &fromAcc[0], fromAcc.size() * sizeof( vector3f ) );
                }

                // Offset the mesh according to the fetched timing data
                if( loadMode == _T("Subframe Velocity Offset") ) {
                    // If we're doing subframes, offset from the closest cached subframe sample.
                    if( realTime == sampleTime )
                        mesh_copy( mesh, m_cachedTrimesh3 );
                    else
                        mesh_copy_time_offset( mesh, m_cachedTrimesh3,
                                               TicksToSec( realTime - sampleTime ) / timeDerivative );
                } else {
                    // Otherwise, offset from the nearest frame sample that's already cached.
                    if( t == requestTime )
                        mesh_copy( mesh, m_cachedTrimesh3 );
                    else
                        mesh_copy_time_offset( mesh, m_cachedTrimesh3, TicksToSec( t - requestTime ) );
                }
            }
        } else if( loadMode == _T("Frame Interpolation") || loadMode == _T("Subframe Interpolation") ) {

            bool useSubframes = false;
            if( loadMode == _T("Subframe Interpolation") )
                useSubframes = true;

            get_timing_data( t, tpf / 4, realTime, timeDerivative, isEmptyMeshTime );
            if( isEmptyMeshTime ) {
                const TimeValue invalidTime = ( std::numeric_limits<TimeValue>::min )();

                invalidCache = true;
                m_cachedInterval.Set( invalidTime, invalidTime );
                m_cachedTrimesh3.clear_and_deallocate();
            } else {
                double frameNumber = (double)realTime / GetTicksPerFrame();

                // Find the bracketing frames for this time
                filename_sequence& sequence = current_sequence();
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
                interval.Set( (TimeValue)( frameBracket.first * tpf ), (TimeValue)( frameBracket.second * tpf ) );

                if( loadMode != m_cachedLoadingMode || m_cachedInterval != interval ||
                    current_sequence_id() != m_cachedSequenceID ) {

                    invalidCache = true;
                    // get_timing_data( t, tpf/4, realTime, timeDerivative );

                    load_mesh_interpolated( frameBracket, alpha ); // this sets the cached interval for us
                } else {
                    m_cachedTrimesh3.linear_interpolation( m_cachedTrimesh3Interval.first,
                                                           m_cachedTrimesh3Interval.second, alpha );
                }
                m_cachedTrimesh3.scale( m_meshScale.at_time( t ) );
            }

            mesh_copy( mesh, m_cachedTrimesh3 );

        } else {
            throw std::runtime_error( "Unknown loading mode \"" + frantic::strings::to_string( loadMode ) + "\"" );
            return;
        }

        // rebuild the bounding box to match the new mesh
        mesh.buildBoundingBox();

        // if we had to build a new cache, then save the new cache data
        if( invalidCache ) {
            // Save cache settings
            m_cachedSequenceID = current_sequence_id();
            m_cachedLoadingMode = loadMode;
            m_cachedMeshInRenderingMode = m_inRenderingMode;

            // If we don't want to cache a copy of the trimesh in memory, wipe it out after loading.
            // Make sure to invalidate the cachedMesh times so that a new one will get loaded
            // on the next change in time.
            if( !m_keepMeshInMemory.at_time( t ) ) {
                if( loadMode == _T("Frame Velocity Offset") || loadMode == _T("Nearest Frame Velocity Offset") ||
                    loadMode == _T("Subframe Velocity Offset") ) {
                    m_cachedTrimesh3.clear_and_deallocate();
                }
                if( loadMode == _T("Frame Interpolation") || loadMode == _T("Subframe Interpolation") ) {
                    m_cachedTrimesh3Interval.first.clear_and_deallocate();
                    m_cachedTrimesh3Interval.second.clear_and_deallocate();
                }
                invalidate_cache();
            }
        }

        // mprintf("clearing trimesh3\n");
        // m_cachedTrimesh3.clear();

        // mprintf("clearing max mesh\n");
        // clear_mesh(mesh);

    } catch( const std::exception& e ) {

        m_cachedTrimesh3.clear_and_deallocate();
        m_cachedTrimesh3Interval.first.clear_and_deallocate();
        m_cachedTrimesh3Interval.second.clear_and_deallocate();
        clear_mesh( mesh );

        frantic::tstring errmsg =
            _T("LegacyXMeshLoader::BuildMesh: ") + frantic::strings::to_tstring( e.what() ) + _T("\n");
        mprintf( _T("%s"), errmsg.c_str() );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( is_network_render_server() )
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
 * @param outIsBlankTime true if an empty mesh is requested for this time
 */
void LegacyXMeshLoader::get_timing_data( TimeValue time, TimeValue timeStep, TimeValue& outTime,
                                         float& outTimeDerivative, bool& outIsBlankTime ) {
    TimeValue realTime( time ), tpf = GetTicksPerFrame();
    float timeDerivative( 1.0f );

    if( m_enablePlaybackGraph.at_time( time ) )
        realTime = static_cast<TimeValue>( m_playbackGraphTime.at_time( time ) * tpf );

    // --- Add frame offset
    realTime += m_frameOffset.at_time( time ) * GetTicksPerFrame();

    // --- Make sure the frame value is within the supported range of the cache.
    //     Clamp to range if specified, otherwise throw
    std::pair<TimeValue, TimeValue> frameRange( m_startFrame.at_time( time ) * tpf, m_endFrame.at_time( time ) * tpf );

    outIsBlankTime = false;

    if( m_limitToRange.at_time( time ) ) {
        if( ( realTime < frameRange.first && m_beforeRangeMode.at_time( time ) == clamp_mode::blank ) ||
            ( realTime > frameRange.second && m_afterRangeMode.at_time( time ) == clamp_mode::blank ) ) {
            outIsBlankTime = true;
        } else {
            realTime = frantic::math::clamp( realTime, frameRange.first, frameRange.second );
        }
    }

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
 * This loads the mesh derived by interpolating between the frames in interval by alpha
 *
 * @param interval the start and end frames of the range
 * @param alpha    the interpolation amout
 */
void LegacyXMeshLoader::load_mesh_interpolated( std::pair<double, double> interval, float alpha ) {

    filename_sequence& sequence = current_sequence();
    int tpf = GetTicksPerFrame();

    // check for file existence
    if( !sequence.get_frame_set().frame_exists( interval.first ) )
        mprintf( _T("LegacyXMeshLoader::load_mesh_interpolated: Frame %f does not exist in the selected cache."),
                 interval.first );
    else if( !sequence.get_frame_set().frame_exists( interval.second ) )
        mprintf( _T("LegacyXMeshLoader::load_mesh_interpolated: Frame %f does not exist in the selected cache."),
                 interval.second );
    else {
        // Load the meshes
        load_mesh_file( sequence[interval.first], m_cachedTrimesh3Interval.first );
        load_mesh_file( sequence[interval.second], m_cachedTrimesh3Interval.second );

        m_cachedInterval.Set( static_cast<TimeValue>( interval.first * tpf ),
                              static_cast<TimeValue>( interval.second * tpf ) );

        m_cachedTrimesh3.linear_interpolation( m_cachedTrimesh3Interval.first, m_cachedTrimesh3Interval.second, alpha );
    }
}

/**
 * Given a time value, finds the closest frame to that time (including subframes)
 *
 * @param frameTime   the time to find a frame near
 * @param frameNumber holds the nearest frame number on return
 * @return            true if a suitable frame was found, false otherwise
 */
bool LegacyXMeshLoader::get_nearest_subframe( TimeValue frameTime, double& frameNumber ) {

    std::pair<double, double> range;
    float alpha;
    double frame( (double)frameTime / GetTicksPerFrame() );

    if( current_sequence().get_frame_set().frame_exists( frame ) ) {
        frameNumber = frame;
    } else if( current_sequence().get_frame_set().get_nearest_subframe_interval( frame, range, alpha ) ) {

        if( alpha < 0.5 )
            frameNumber = range.first;
        else
            frameNumber = range.second;
    } else {

        return false;
    }

    return true;
}

TimeValue LegacyXMeshLoader::round_to_nearest_wholeframe( TimeValue t ) const {
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
bool LegacyXMeshLoader::get_nearest_wholeframe( TimeValue frameTime, double& frameNumber ) {

    TimeValue tpf = GetTicksPerFrame();
    frameTime = round_to_nearest_wholeframe( frameTime );
    double frame = (double)frameTime / tpf;

    std::pair<double, double> range;
    float alpha;

    if( current_sequence().get_frame_set().frame_exists( frame ) ) {
        frameNumber = frame;
    } else if( current_sequence().get_nearest_wholeframe_interval( frame, range, alpha ) ) {

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
void LegacyXMeshLoader::NotifyEveryone() {
    // Called when we change the object, so it's dependents get informed
    // and it redraws, etc.
    NotifyDependents( FOREVER, (PartID)PART_ALL, REFMSG_CHANGE );
}

/**
 * This loads the mesh for the specified frame into the cached trimesh.
 *
 * @param frame the frame to load
 */
void LegacyXMeshLoader::load_mesh_at_frame( double frame ) { // TimeValue frameTime) {

    filename_sequence& sequence = current_sequence();

    if( !sequence.get_frame_set().empty() && sequence.get_frame_set().frame_exists( frame ) )
        // If the cache wasn't empty, check to make sure the file is in the cache, and load it
        load_mesh_file( sequence[frame], m_cachedTrimesh3 );
    else
        // The file requested isn't there.
        throw std::runtime_error( "LegacyXMeshLoader::load_mesh_at_frame: File '" +
                                  frantic::strings::to_string( sequence[frame] ) + "' requested does not exist." );
}

/**
 * If the the cached mesh has a velocity channel, scale it by the
 * time derivative
 *
 * @param timeDerivative  The scaling value
 */
void LegacyXMeshLoader::scale_cached_velocity_channel( float timeDerivative ) {
    if( m_cachedTrimesh3.has_vertex_channel( _T("Velocity") ) ) {

        trimesh3_vertex_channel_general_accessor ca =
            m_cachedTrimesh3.get_vertex_channel_general_accessor( _T("Velocity") );
        char* data[1];
        for( unsigned int i = 0; i < ca.size(); ++i ) {
            data[0] = ca.data( i );
            ca.get_weighted_sum_combine_function()( &timeDerivative, data, 1, 3, data[0] );
        }
    }
}

/**
 * Set (initialize) the filename_sequence indexed by sequence_id
 *
 * @param sequence_id  the sequence to set
 * @param filename     path to a file in the sequence
 */
void LegacyXMeshLoader::init_sequence( const frantic::tstring& filename, filename_sequence& outSequence ) {
    try {

        if( filename != _T("") ) {
            outSequence = filename_sequence( filename );
            outSequence.sync_frame_set();
        } else {
            outSequence = filename_sequence();
            throw std::runtime_error( "The supplied sequence is invalid." );
        }

    } catch( const std::exception& e ) {

        frantic::tstring errmsg = _T("LegacyXMeshLoader::init_sequence: ") + frantic::strings::to_tstring( e.what() ) +
                                  _T("  (sequence name: \"") + filename + _T("\")\n");
        mprintf( _T("%s"), errmsg.c_str() );
        LogSys* log = GetCOREInterface()->Log();
        log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMesh Loader Object Error"), _T("%s"), e.what() );
        if( is_network_render_server() )
            throw MAXException( const_cast<TCHAR*>( errmsg.c_str() ) );
    }
}

/**
 * Named setter for the render sequence. Set the path for the render sequence.
 *
 * @param filename path to a frame in the new sequence
 */
void LegacyXMeshLoader::set_render_sequence( const frantic::tstring& filename ) {
    init_sequence( filename, m_fileSequences[RENDER_SEQ] );
    invalidate();
}

/**
 * Named setter for the proxy sequence. Set the path for the proxy sequence.
 *
 * @param filename path to a frame in the new sequence
 */
void LegacyXMeshLoader::set_proxy_sequence( const frantic::tstring& filename ) {
    init_sequence( filename, m_fileSequences[PROXY_SEQ] );
    invalidate();
}

/**
 * Named getter for the proxy sequence. Get the path to the first frame in the proxy sequence.
 *
 * @return  path to the first frame in the proxy sequence
 */
frantic::tstring LegacyXMeshLoader::get_proxy_sequence() {
    return m_fileSequences[PROXY_SEQ].get_filename_pattern().get_pattern();
}

/**
 * Named getter for the render sequence. Get the path to the first frame in the render sequence.
 *
 * @return  path to the first frame in the render sequence
 */
frantic::tstring LegacyXMeshLoader::get_render_sequence() {
    return m_fileSequences[RENDER_SEQ].get_filename_pattern().get_pattern();
}

/**
 * Reset the cached mesh info (to force a reload on the next call to BuildMesh)
 */
void LegacyXMeshLoader::invalidate_cache() {
    TimeValue invalidTime = ( std::numeric_limits<TimeValue>::min )();

    m_cachedTime = m_cachedFrameTicks = invalidTime;
    m_cachedInterval.Set( invalidTime, invalidTime );

    m_cachedSequenceID = ( std::numeric_limits<int>::min )();
    m_cachedLoadingMode = _T("");
}

/**
 * Reset the cached mesh info and force a reload immediately
 */
void LegacyXMeshLoader::invalidate() {
    invalidate_cache();

    ivalid.SetEmpty();
    NotifyEveryone();
}

/**
 * Clears all cached mesh data, including the max mesh.
 */
void LegacyXMeshLoader::clear_cache() {
    /*
    mprintf("LegacyXMeshLoader::clear_cache()\n");
    MessageBox(NULL, "LegacyXMeshLoader::clear_cache()", "LegacyXMeshLoader::clear_cache()", MB_OK);
    std::ofstream fout("C:\\debug.txt", std::ios_base::app);
    fout << "~LegacyXMeshLoader()::clear_cache() " << time(NULL) << std::endl;
    */

    m_cachedTrimesh3.clear_and_deallocate();
    m_cachedTrimesh3Interval.first.clear_and_deallocate();
    m_cachedTrimesh3Interval.second.clear_and_deallocate();
    clear_mesh( mesh );
}

/**
 * Check if the cached trimesh or trimesh interval is valid for a given time
 *
 * @param filename time  the time to query validity for
 * @return         true if the cache is valid @ time, false otherwise
 */
bool LegacyXMeshLoader::is_cache_valid( TimeValue time ) {
    if( ( time == m_cachedTime ) && ( current_sequence_id() == m_cachedSequenceID ) &&
        ( frantic::tstring( m_loadMode.at_time( time ).operator const frantic::strings::tstring() ) == m_cachedLoadingMode ) )
        return true;

    return false;
}

/**
 * Wrapper for retrieving the currently active frame sequence
 *
 * @return  a reference to the currently active frame sequence
 */
filename_sequence& LegacyXMeshLoader::current_sequence() { return m_fileSequences[current_sequence_id()]; }

/**
 * Wrapper for retrieving the id of the currently active frame sequence
 *
 * @return  the id of the currently active frame sequence
 */
int LegacyXMeshLoader::current_sequence_id() {
    frantic::tstring seq_str;

    if( !m_inRenderingMode || m_useViewportSettings.at_time( 0 ) )
        seq_str = m_viewportSequenceID.at_time( 0 ).operator const frantic::strings::tstring();
    else
        seq_str = m_renderSequenceID.at_time( 0 ).operator const frantic::strings::tstring();

    return ( seq_str == _T("Render Sequence") ) ? ( 0 ) : ( 1 );
}

/**
 * Retrieves the lowest frame number of the currently active sequence
 *
 * @return  the lowest frame number of the currently active sequence
 */
float LegacyXMeshLoader::get_start_frame() {
    filename_sequence& sequence = current_sequence();

    return !( sequence.get_frame_set().empty() ) ? ( static_cast<float>( *( sequence.get_frame_set().begin() ) ) )
                                                 : ( 0 );
}

/**
 * Retrieves the highest frame number of the currently active sequence
 *
 * @return  the highest frame number of the currently active sequence
 */
float LegacyXMeshLoader::get_end_frame() {
    filename_sequence& sequence = current_sequence();

    return !( sequence.get_frame_set().empty() ) ? ( static_cast<float>( *( --sequence.get_frame_set().end() ) ) )
                                                 : ( 0 );
}

void LegacyXMeshLoader::SetRenderTime( TimeValue t ) { m_renderTime = t; }

void LegacyXMeshLoader::ClearRenderTime() { m_renderTime = TIME_NegInfinity; }

void LegacyXMeshLoader::SetEmptyValidityAndNotifyDependents() {
    ivalid.SetEmpty();
    NotifyEveryone();
}

TimeValue LegacyXMeshLoader::GetRenderTime() const { return m_renderTime; }

bool LegacyXMeshLoader::HasValidRenderTime() const { return m_renderTime != TIME_NegInfinity; }

//---------------------------------------------------------------------------
// Function publishing setup

void LegacyXMeshLoader::set_scripted_owner( ReferenceTarget* scriptedOwner ) { m_parent.attach_to( scriptedOwner ); }

void LegacyXMeshLoader::InitializeFPDescriptor() {
    FFCreateDescriptor c( this, LegacyXMeshLoader_INTERFACE_ID, _T("MeshLoaderInterface"),
                          GetLegacyXMeshLoaderClassDesc() );

    c.add_function( &LegacyXMeshLoader::set_scripted_owner, _T("SetScriptedOwner") );
    c.add_function( &LegacyXMeshLoader::invalidate, _T("Invalidate") );
    c.add_function( &LegacyXMeshLoader::clear_cache, _T("ClearCache") );

    c.add_property( &LegacyXMeshLoader::get_start_frame, _T("StartFrame") );
    c.add_property( &LegacyXMeshLoader::get_end_frame, _T("EndFrame") );
    c.add_property( &LegacyXMeshLoader::get_render_sequence, &LegacyXMeshLoader::set_render_sequence,
                    _T("RenderSequence") );
    c.add_property( &LegacyXMeshLoader::get_proxy_sequence, &LegacyXMeshLoader::set_proxy_sequence,
                    _T("ProxySequence") );
}
