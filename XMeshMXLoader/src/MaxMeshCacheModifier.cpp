// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "MaxMeshCacheModifier.h"
#include "distributed_file_system_hack.hpp"
#include "resource.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <frantic/files/filename_sequence.hpp>
#include <frantic/geometry/xmesh_reader.hpp>
#include <frantic/geometry/xmesh_writer.hpp>
#include <frantic/graphics/raw_byte_buffer.hpp>

#include <frantic/max3d/convert.hpp>
#include <frantic/max3d/logging/max_progress_logger.hpp>
#include <frantic/max3d/maxscript/maxscript.hpp>

#ifdef ENABLE_MAX_MESH_CACHE_MODIFIER

#define MaxMeshCacheModifier_CLASS_NAME "XMeshAnimCache"
#define MaxMeshCacheModifier_SCRIPT_NAME "XMeshAnimCache"
#define MaxMeshCacheModifier_INTERFACE_NAME "XMeshAnimCacheInterface"

#define WM_UPDATESEQ WM_USER + 0x0034
#define WM_UPDATESTATUS WM_USER + 0x0035

extern HINSTANCE ghInstance;

class MaxMeshCacheModifierDesc : public ClassDesc2 {
    ParamBlockDesc2* m_pblockDesc;

  public:
    MaxMeshCacheModifierDesc();
    virtual ~MaxMeshCacheModifierDesc();

    virtual int IsPublic() { return TRUE; }
    virtual void* Create( BOOL /*loading=FALSE*/ ) { return new MaxMeshCacheModifier; }
    virtual const MCHAR* ClassName() { return _M( MaxMeshCacheModifier_CLASS_NAME ); }
    virtual SClass_ID SuperClassID() { return OSM_CLASS_ID; }
    virtual Class_ID ClassID() { return MaxMeshCacheModifier_CLASS_ID; }
    virtual const MCHAR* Category() { return _M( "Thinkbox" ); }
    virtual const MCHAR* InternalName() { return _M( MaxMeshCacheModifier_SCRIPT_NAME ); }
    virtual HINSTANCE HInstance() { return ghInstance; }
    virtual Class_ID SubClassID() { return Class_ID( 0, 0 ); }
};

ClassDesc2* GetMaxMeshCacheModifierDesc() {
    static MaxMeshCacheModifierDesc theImpl;
    return &theImpl;
}

enum {
    kSequenceName,
    kUsePlaybackGraph,
    kUsePlaybackRange,
    kPlaybackGraph,
    kPlaybackOffset,
    kPlaybackRangeMin,
    kPlaybackRangeMax,
    kIsRecording // Obsolete
};

class MaxMeshCacheModifierDialogProc : public ParamMap2UserDlgProc {

  private:
    void update_sequence_name( HWND hWnd, IParamBlock2* pblock ) {
        const MCHAR* strSeq = pblock->GetStr( kSequenceName );
        if( !strSeq || *strSeq == '\0' ) {
            SetWindowText( hWnd, "Pick a sequence" );
        } else {
            boost::filesystem::path seqPath( strSeq );
            std::string filename = seqPath.filename().string();
            SetWindowText( hWnd, filename.c_str() );
        }
    }

    void do_create_record( IParamBlock2* pblock ) {
        frantic::max3d::mxs::frame<2> f;
        frantic::max3d::mxs::local<String> initialPath( f );
        frantic::max3d::mxs::local<Value> result( f );

        try {
            const MCHAR* seqName = pblock->GetStr( kSequenceName );
            if( !seqName )
                seqName = "";
            initialPath = new String( seqName );
            result = frantic::max3d::mxs::expression( "ffShowMeshCacheModFloater initialPath:thePath" )
                         .bind( "thePath", initialPath )
                         .evaluate<Value*>();

            // If we did not get a struct back, it means the user cancelled the dialog.
            if( !is_struct( result.ptr() ) )
                return;

            // Grab the return values from the maxscript struct.
            Struct* pResult = static_cast<Struct*>( result.ptr() );
            int frameStart = frantic::max3d::mxs::get_struct_value( pResult, "rangeStart" )->to_int();
            int frameEnd = frantic::max3d::mxs::get_struct_value( pResult, "rangeEnd" )->to_int();
            std::string newPath = frantic::max3d::mxs::get_struct_value( pResult, "sequencePath" )->to_string();

            // If the path is empty, we can't really do anything.
            if( newPath.empty() )
                return;

            // TODO: Check if any of this sequence already exists, and warn the user of overwriting.

            // If the mod is disabled, the recording won't work, so warn the user.
            Modifier* pMod = static_cast<Modifier*>( pblock->GetOwner() );
            if( !pMod->IsEnabled() ) {
                MessageBox( NULL, "The XMeshCache modifier is disabled, so recording cannot continue.", "ERROR",
                            MB_OK );
                return;
            }

            Interface7* ip = (Interface7*)GetCOREInterface();

            // Grab the node that is being edited.
            INode* pNode = ip->FindNodeFromBaseObject( ip->GetCurEditObject() );
            if( !pNode ) {
                MessageBox( NULL, "Cannot retrieve the node that the XMeshCache Modifier is applied to.", "ERROR",
                            MB_OK );
                return;
            }

            // Put the modifier into recording mode, then evaluate the object stack at each time we need
            // a sample.
            // TODO: Disable modifiers above the XMeshCache since they are not needed when recording.
            // TODO: Allow the user to make caches at different times than exact frames.
            BOOL isRecording = TRUE;
            TimeValue curTime = ip->GetTime();
            pblock->SetValue( kSequenceName, curTime, (MCHAR*)newPath.c_str() );
            pblock->SetValue( kIsRecording, curTime, isRecording );

            try {
                frantic::max3d::logging::max_progress_logger progress( "", 0, 100, true, ip );

                // We need to run the modifier now at each time step in order to generate the records.
                for( int frame = frameStart; frame <= frameEnd; ++frame ) {
                    curTime = frame * GetTicksPerFrame();
                    ObjectState os = pNode->EvalWorldState( curTime );

                    progress.update_progress( frame - frameStart, frameEnd - frameStart );
                }
            } catch( const frantic::logging::progress_cancel_exception& ) {
            }

            // Turn off the recording mode.
            isRecording = FALSE;
            pblock->SetValue( kIsRecording, curTime, isRecording );
        } catch( const std::exception& e ) {
            MessageBox( NULL, e.what(), "ERROR", MB_OK );
        }
    }

  public:
    void DeleteThis() {}

    INT_PTR DlgProc( TimeValue /*t*/, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
        try {
            IParamBlock2* pblock = map->GetParamBlock();
            if( !pblock )
                return FALSE;

            switch( msg ) {
            case WM_INITDIALOG:
                SetWindowText( GetDlgItem( hWnd, IDC_MESHCACHEMOD_RELOAD_SEQ_BUTTON ), "Reload Source Sequence" );
                SetWindowText( GetDlgItem( hWnd, IDC_MESHCACHEMOD_RECORD_BUTTON ), "Create a New Record ..." );
                update_sequence_name( GetDlgItem( hWnd, IDC_MESHCACHEMOD_SEQ_EDIT ), pblock );
                break;
            case WM_COMMAND: {
                int ctrlID = LOWORD( wParam );

                switch( ctrlID ) {
                case IDC_MESHCACHEMOD_RELOAD_SEQ_BUTTON:
                    MaxMeshCacheModifier* maxMeshCacheModifier = (MaxMeshCacheModifier*)pblock->GetOwner();
                    if( maxMeshCacheModifier == 0 ) {
                        return false;
                    }
                    const MCHAR* strSeq = pblock->GetStr( kSequenceName );
                    maxMeshCacheModifier->reset_filename_sequence( std::string( strSeq ) );
                    return TRUE;
                }
                /*if( LOWORD(wParam) == IDC_MESHCACHEMOD_RECORD_BUTTON ){
                        do_create_record( pblock );
                        return TRUE;
                }*/
            } break;
            case WM_UPDATESEQ:
                update_sequence_name( GetDlgItem( hWnd, IDC_MESHCACHEMOD_SEQ_EDIT ), pblock );
                return TRUE;
            case WM_UPDATESTATUS:
                SetWindowText( GetDlgItem( hWnd, IDC_MESHCACHEMOD_STATUS ), (char*)lParam );
                return TRUE;
            }
        } catch( const std::exception& e ) {
            std::string errmsg = "XMeshAnimCache: DlgProc: " + std::string( e.what() ) + "\n";
            mprintf( "%s", errmsg.c_str() );
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMeshAnimCache Object Error"), _T("%s"), e.what() );
            if( GetCOREInterface()->IsNetworkRenderServer() )
                throw MAXException( const_cast<char*>( errmsg.c_str() ) );
        }
        return FALSE;
    }
};

ParamMap2UserDlgProc* GetMaxMeshCacheModifierDialogProc() {
    static MaxMeshCacheModifierDialogProc theProc;
    return &theProc;
}

class XMeshAnimCachePBAccessor : public PBAccessor {
    void Set( PB2Value& /*v*/, ReferenceMaker* owner, ParamID id, int /*tabIndex*/, TimeValue /*t*/ ) {
        try {
            if( !owner ) {
                throw std::runtime_error( "Error: owner is NULL" );
            }
            IParamBlock2* pblock = owner->GetParamBlockByID( 0 );
            if( !pblock ) {
                throw std::runtime_error( "Error: pblock is NULL" );
            }

            switch( id ) {
            case kSequenceName: {
                const MCHAR* strSeq = pblock->GetStr( kSequenceName );
                if( strSeq && strSeq[0] != '\0' ) {
                    MaxMeshCacheModifier* maxMeshCacheModifier = static_cast<MaxMeshCacheModifier*>( owner );
                    maxMeshCacheModifier->reset_filename_sequence( std::string( strSeq ) );
                }
            }
            }
        } catch( std::exception& e ) {
            std::string errmsg = "XMeshAnimCache: PBAccessor: " + std::string( e.what() ) + "\n";
            mprintf( "%s", errmsg.c_str() );
            LogSys* log = GetCOREInterface()->Log();
            log->LogEntry( SYSLOG_ERROR, NO_DIALOG, _T("XMeshAnimCache Object Error"), _T("%s"), e.what() );
            if( GetCOREInterface()->IsNetworkRenderServer() )
                throw MAXException( const_cast<char*>( errmsg.c_str() ) );
        }
    }
};

static XMeshAnimCachePBAccessor xmeshAnimCachePBAccessor;

MaxMeshCacheModifierDesc::MaxMeshCacheModifierDesc() {
    m_pblockDesc = new ParamBlockDesc2( 0,                                                 // BlockID
                                        _M( "Parameters" ),                                // Internal name
                                        0,                                                 // String Resource name
                                        GetMaxMeshCacheModifierDesc(),                     // ClassDesc
                                        P_AUTO_CONSTRUCT | P_AUTO_UI | P_CALLSETS_ON_LOAD, // Flags

                                        0, // Auto construct block num

                                        IDD_MESHCACHEMOD,                    // UI dialog
                                        IDS_PARAMETERS,                      // Dialog title rsc
                                        0,                                   // Show flags
                                        0,                                   // Rollup flags
                                        GetMaxMeshCacheModifierDialogProc(), // Dialog proc

                                        end );

    m_pblockDesc->AddParam( kSequenceName, _M( "SequencePath" ), TYPE_FILENAME, P_RESET_DEFAULT, 0, end );
    m_pblockDesc->ParamOption( kSequenceName, p_caption, IDS_MESHCACHEMOD_SEQ_CAPTION, end );
    m_pblockDesc->ParamOption( kSequenceName, p_file_types, IDS_MESHCACHEMOD_SEQ_FILETYPES, end );
    m_pblockDesc->ParamOption( kSequenceName, p_ui, TYPE_FILEOPENBUTTON, IDC_MESHCACHEMOD_SEQ_BUTTON, end );
    m_pblockDesc->ParamOption( kSequenceName, p_accessor, &xmeshAnimCachePBAccessor, end );

    m_pblockDesc->AddParam( kUsePlaybackGraph, _M( "UsePlaybackGraph" ), TYPE_BOOL, P_RESET_DEFAULT, 0, end );
    m_pblockDesc->ParamOption( kUsePlaybackGraph, p_default, FALSE, end );
    m_pblockDesc->ParamOption( kUsePlaybackGraph, p_ui, TYPE_SINGLECHEKBOX, IDC_MESHCACHEMOD_GRAPH_CHECKBOX, end );

    m_pblockDesc->AddParam( kUsePlaybackRange, _M( "UsePlaybackRange" ), TYPE_BOOL, P_RESET_DEFAULT, 0, end );
    m_pblockDesc->ParamOption( kUsePlaybackRange, p_default, FALSE, end );
    m_pblockDesc->ParamOption( kUsePlaybackRange, p_ui, TYPE_SINGLECHEKBOX, IDC_MESHCACHEMOD_RANGE_CHECKBOX, end );

    m_pblockDesc->AddParam( kPlaybackGraph, _M( "PlaybackGraph" ), TYPE_FLOAT, P_RESET_DEFAULT | P_ANIMATABLE, 0, end );
    m_pblockDesc->ParamOption( kPlaybackGraph, p_default, 0.f, end );
    m_pblockDesc->ParamOption( kPlaybackGraph, p_range, -FLT_MAX, FLT_MAX, end );
    m_pblockDesc->ParamOption( kPlaybackGraph, p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_MESHCACHEMOD_GRAPH_EDIT,
                               IDC_MESHCACHEMOD_GRAPH_SPINNER, SPIN_AUTOSCALE, end );

    m_pblockDesc->AddParam( kPlaybackOffset, _M( "PlaybackOffset" ), TYPE_FLOAT, P_RESET_DEFAULT, 0, end );
    m_pblockDesc->ParamOption( kPlaybackOffset, p_default, 0.f, end );
    m_pblockDesc->ParamOption( kPlaybackOffset, p_range, -FLT_MAX, FLT_MAX, end );
    m_pblockDesc->ParamOption( kPlaybackOffset, p_ui, TYPE_SPINNER, EDITTYPE_FLOAT, IDC_MESHCACHEMOD_OFFSET_EDIT,
                               IDC_MESHCACHEMOD_OFFSET_SPINNER, 1.f, end );

    m_pblockDesc->AddParam( kPlaybackRangeMin, _M( "PlaybackRangeMin" ), TYPE_INT, P_RESET_DEFAULT, 0, end );
    m_pblockDesc->ParamOption( kPlaybackRangeMin, p_default, 0, end );
    m_pblockDesc->ParamOption( kPlaybackRangeMin, p_range, INT_MIN, INT_MAX, end );
    m_pblockDesc->ParamOption( kPlaybackRangeMin, p_ui, TYPE_SPINNER, EDITTYPE_INT, IDC_MESHCACHEMOD_RANGEMIN_EDIT,
                               IDC_MESHCACHEMOD_RANGEMIN_SPINNER, SPIN_AUTOSCALE, end );

    m_pblockDesc->AddParam( kPlaybackRangeMax, _M( "PlaybackRangeMax" ), TYPE_INT, P_RESET_DEFAULT, 0, end );
    m_pblockDesc->ParamOption( kPlaybackRangeMax, p_default, 100, end );
    m_pblockDesc->ParamOption( kPlaybackRangeMax, p_range, INT_MIN, INT_MAX, end );
    m_pblockDesc->ParamOption( kPlaybackRangeMax, p_ui, TYPE_SPINNER, EDITTYPE_INT, IDC_MESHCACHEMOD_RANGEMAX_EDIT,
                               IDC_MESHCACHEMOD_RANGEMAX_SPINNER, SPIN_AUTOSCALE, end );

    /*m_pblockDesc->AddParam( kIsRecording, _M(""), TYPE_BOOL, P_RESET_DEFAULT | P_TRANSIENT, 0, end );
    m_pblockDesc->ParamOption( kUsePlaybackRange, p_default, FALSE, end );*/
}

MaxMeshCacheModifierDesc::~MaxMeshCacheModifierDesc() {
    if( m_pblockDesc )
        delete m_pblockDesc;
    m_pblockDesc = NULL;
}

//-----------------------------------------------------
// MaxMeshCacheModifier
//-----------------------------------------------------
MaxMeshCacheModifier::MaxMeshCacheModifier() {
    m_pblock = NULL;
    m_status = kStatusNotSet;
    m_statusMessage = "";

    InitializeFPDescriptor();

    GetMaxMeshCacheModifierDesc()->MakeAutoParamBlocks( this );
}

MaxMeshCacheModifier::~MaxMeshCacheModifier() { ReplaceReference( 0, NULL ); }

int MaxMeshCacheModifier::NumSubs() { return 1; }

Animatable* MaxMeshCacheModifier::SubAnim( int i ) { return ( i == 0 ) ? m_pblock : NULL; }

MSTR MaxMeshCacheModifier::SubAnimName( int i ) { return ( i == 0 ) ? m_pblock->GetLocalName() : ""; }

int MaxMeshCacheModifier::NumParamBlocks() { return 1; }

IParamBlock2* MaxMeshCacheModifier::GetParamBlock( int i ) { return ( i == 0 ) ? m_pblock : NULL; }

IParamBlock2* MaxMeshCacheModifier::GetParamBlockByID( short id ) { return ( id == m_pblock->ID() ) ? m_pblock : NULL; }

void MaxMeshCacheModifier::BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev ) {
    GetMaxMeshCacheModifierDesc()->BeginEditParams( ip, this, flags, prev );

    IParamMap2* pMap = m_pblock->GetMap();
    if( pMap )
        SendMessage( pMap->GetHWnd(), WM_UPDATESTATUS, m_status, (LPARAM)m_statusMessage.c_str() );
}

void MaxMeshCacheModifier::EndEditParams( IObjParam* ip, ULONG flags, Animatable* next ) {
    GetMaxMeshCacheModifierDesc()->EndEditParams( ip, this, flags, next );
}

Class_ID MaxMeshCacheModifier::ClassID() { return GetMaxMeshCacheModifierDesc()->ClassID(); }

void MaxMeshCacheModifier::GetClassName( MSTR& s ) { s = GetMaxMeshCacheModifierDesc()->ClassName(); }

int MaxMeshCacheModifier::NumRefs() { return 1; }

RefTargetHandle MaxMeshCacheModifier::GetReference( int i ) { return ( i == 0 ) ? m_pblock : NULL; }

void MaxMeshCacheModifier::SetReference( int i, RefTargetHandle rtarg ) {
    if( i == 0 )
        m_pblock = (IParamBlock2*)rtarg;
}

RefResult MaxMeshCacheModifier::NotifyRefChanged( Interval /*changeInt*/, RefTargetHandle hTarget, PartID& /*partID*/,
                                                  RefMessage message ) {
    if( hTarget == m_pblock && message == REFMSG_CHANGE ) {
        int paramID = m_pblock->LastNotifyParamID();
        if( paramID < 0 )
            return REF_STOP;

        IParamMap2* pMap = m_pblock->GetMap();

        switch( paramID ) {
        case kSequenceName:
            if( pMap )
                SendMessage( pMap->GetHWnd(), WM_UPDATESEQ, 0, 0 );
            // TODO: Kill cache.
            break;
        }
    }
    return REF_SUCCEED;
}

RefTargetHandle MaxMeshCacheModifier::Clone( RemapDir& remap ) {
    MaxMeshCacheModifier* newob = new MaxMeshCacheModifier;
    newob->ReplaceReference( 0, remap.CloneRef( m_pblock ) );
    BaseClone( this, newob, remap );
    return ( newob );
}

CreateMouseCallBack* MaxMeshCacheModifier::GetCreateMouseCallBack() { return NULL; }

int MaxMeshCacheModifier::NumInterfaces() { return 1; }

FPInterface* MaxMeshCacheModifier::GetInterface( int i ) {
    if( i == 0 ) {
        return GetInterface( MaxMeshCacheModifier_INTERFACE_ID );
    }
    return NULL;
}

FPInterface* MaxMeshCacheModifier::GetInterface( Interface_ID id ) {
    if( id == MaxMeshCacheModifier_INTERFACE_ID ) {
        return static_cast<frantic::max3d::fpwrapper::FFMixinInterface<MaxMeshCacheModifier>*>( this );
    }
    return NULL;
}

#if MAX_VERSION_MAJOR >= 15
const MCHAR* MaxMeshCacheModifier::GetObjectName() {
#else
MCHAR* MaxMeshCacheModifier::GetObjectName() {
#endif
    return _M( "XMesh Cache" );
}

Interval MaxMeshCacheModifier::LocalValidity( TimeValue t ) {
    /*
    Interval iv;
    if( !m_pblock->GetInt( kIsRecording ) ){
            get_current_frame( t, iv );
    }else{
            //When recording we want to get called every time the stack is evaluated.
            iv.SetInstant(t);
    }

    return iv;
    */

    return Interval( t, t );
}

ChannelMask MaxMeshCacheModifier::ChannelsUsed() { return GEOM_CHANNEL; }

ChannelMask MaxMeshCacheModifier::ChannelsChanged() { return GEOM_CHANNEL; }

Class_ID MaxMeshCacheModifier::InputType() { return defObjectClassID; }

int MaxMeshCacheModifier::get_current_frame( TimeValue t, Interval& outValid ) {
    BOOL usePlayback = m_pblock->GetInt( kUsePlaybackGraph );
    BOOL useRange = m_pblock->GetInt( kUsePlaybackRange );

    float floatFrame;
    if( usePlayback ) {
        outValid = FOREVER;
        m_pblock->GetValue( kPlaybackGraph, t, floatFrame, outValid );
    } else {
        floatFrame = float( t ) / float( GetTicksPerFrame() );
        outValid.SetInstant( t );
    }

    floatFrame += m_pblock->GetFloat( kPlaybackOffset );
    floatFrame += 0.5f;

    int intFrame = static_cast<int>( floor( floatFrame ) );

    // Clamp the calculated frame into the custom limited range.
    //
    // NOTE: If we are not using the playback graph, then the validity outside this range is
    // very easy to specify exactly. If we are using the playback graph, it would involve a
    // rootfind on the curve, and I'm not really in the mood for that sort of hijinks. ;)
    //
    // NOTE: An empty interval (ie. rangeMax < rangeMin ) is treated as if there was only the minFrame
    //        to be loaded forever. time(t) == minFrame for all t.
    if( useRange ) {
        int frameMin = m_pblock->GetInt( kPlaybackRangeMin );
        int frameMax = m_pblock->GetInt( kPlaybackRangeMax );
        if( frameMin < frameMax ) {
            if( intFrame <= frameMin ) {
                intFrame = frameMin;
                if( !usePlayback )
                    outValid.Set( TIME_NegInfinity, frameMin * GetTicksPerFrame() );
            } else if( intFrame >= frameMax ) {
                intFrame = frameMax;
                if( !usePlayback )
                    outValid.Set( frameMax * GetTicksPerFrame(), TIME_PosInfinity );
            }
        } else {
            // TODO: If an empty interval is created, flag as no-op.
            intFrame = frameMin;
            outValid = FOREVER;
        }
    }

    return intFrame;
}

// copied from int get_current_frame()
double MaxMeshCacheModifier::get_current_frame( TimeValue t ) {
    BOOL usePlayback = m_pblock->GetInt( kUsePlaybackGraph );
    BOOL useRange = m_pblock->GetInt( kUsePlaybackRange );
    Interval valid;

    double frame;
    if( usePlayback ) {
        float floatFrame;
        m_pblock->GetValue( kPlaybackGraph, t, floatFrame, valid );
        frame = floatFrame;
    } else {
        frame = double( t ) / double( GetTicksPerFrame() );
    }

    frame += m_pblock->GetFloat( kPlaybackOffset );

    // Clamp the calculated frame into the custom limited range.
    //
    // NOTE: If we are not using the playback graph, then the validity outside this range is
    // very easy to specify exactly. If we are using the playback graph, it would involve a
    // rootfind on the curve, and I'm not really in the mood for that sort of hijinks. ;)
    //
    // NOTE: An empty interval (ie. rangeMax < rangeMin ) is treated as if there was only the minFrame
    //        to be loaded forever. time(t) == minFrame for all t.
    if( useRange ) {
        int frameMin = m_pblock->GetInt( kPlaybackRangeMin );
        int frameMax = m_pblock->GetInt( kPlaybackRangeMax );
        if( frameMin < frameMax ) {
            if( frame <= frameMin ) {
                frame = frameMin;
            } else if( frame >= frameMax ) {
                frame = frameMax;
            }
        } else {
            // TODO: If an empty interval is created, flag as no-op.
            frame = frameMin;
        }
    }

    return frame;
}

void MaxMeshCacheModifier::read_verts_from_xmesh_file( const std::string& filename,
                                                       const std::size_t expectedVertexCount,
                                                       frantic::graphics::raw_byte_buffer& vertexBuffer ) {
    vertexBuffer.resize( sizeof( frantic::graphics::vector3f ) * expectedVertexCount );

    frantic::geometry::xmesh_reader meshHeader( filename );
    const frantic::geometry::xmesh_vertex_channel& vertexChannel = meshHeader.get_vertex_channel( "verts" );
    if( vertexChannel.get_vertex_count() != expectedVertexCount )
        throw std::runtime_error( "Vertex count does not match" );

    meshHeader.load_vertex_channel( "verts", vertexBuffer.ptr_at( 0 ), frantic::channels::data_type_float32, 3,
                                    expectedVertexCount );

    m_lastVertexFilename = meshHeader.get_vertex_channel( "verts" ).get_vertex_file_path().string();
}

void MaxMeshCacheModifier::reset_filename_sequence( const std::string& filename ) {
    for( std::size_t i = 0; i < m_vertexData.size(); ++i ) {
        m_vertexData[i].clear_and_deallocate();
    }
    m_lastVertexFilename.clear();
    if( !filename.empty() ) {
        boost::filesystem::path seqPath( filename );
        std::string universalFilename;
        const bool success = try_get_universal_name( seqPath.string(), universalFilename );
        if( !success ) {
            universalFilename = filename;
        }
        m_filenameSequence = frantic::files::filename_sequence( universalFilename );
        m_filenameSequence.sync_frame_set();
    } else {
        m_filenameSequence = frantic::files::filename_sequence();
    }
}

void MaxMeshCacheModifier::load_vertex_data( const std::string& xmeshFilename, const double frameNumber,
                                             const std::size_t expectedVertexCount,
                                             frantic::graphics::raw_byte_buffer& outVertexData ) {
    outVertexData.resize( sizeof( frantic::graphics::vector3f ) * expectedVertexCount );

    // This block is intended to help compensate for slow fopen().
    // TODO: This should be removed if the issue is resolved.
    if( !m_lastVertexFilename.empty() ) {
        frantic::files::filename_sequence vertexGuessSequence( m_lastVertexFilename );
        const std::string vertexGuessFilename = vertexGuessSequence[frameNumber];
        // const std::string vertexGuessFilename = frantic::files::replace_sequence_number( m_lastVertexFilename,
        // frameNumber );
        try {
            frantic::geometry::load_xmesh_array_file( vertexGuessFilename, outVertexData.ptr_at( 0 ),
                                                      std::string( "float32[3]" ), 12, expectedVertexCount );
            return;
        } catch( const frantic::files::file_open_error& ) {
        }
    }

    // This block is intended to help compensate for slow fopen().
    // TODO: This should be removed if the issue is resolved.
    const std::string cacheFilename = get_dfs_client_cached_storage_filename( xmeshFilename );
    if( !cacheFilename.empty() ) {
        try {
            read_verts_from_xmesh_file( cacheFilename, expectedVertexCount, outVertexData );
            return;
        } catch( const frantic::files::file_open_error& ) {
        }
    }

    boost::filesystem::path xmeshPath( xmeshFilename );
    if( !boost::filesystem::exists( xmeshPath ) ) {
        throw std::runtime_error( "Frame " + boost::lexical_cast<std::string>( frameNumber ) + " does not exist" );
    }
    read_verts_from_xmesh_file( xmeshFilename, expectedVertexCount, outVertexData );
}

void MaxMeshCacheModifier::fill_vertex_data( const std::size_t fileCount, const double frame[2],
                                             const std::size_t expectedVertexCount ) {
    if( fileCount == 1 ) {
        if( m_vertexData[0].frame != frame[0] ) {
            if( m_vertexData[1].frame == frame[0] ) {
                m_vertexData[0].swap( m_vertexData[1] );
            }
        }
    } else if( fileCount == 2 ) {
        if( m_vertexData[0].frame == frame[1] && m_vertexData[1].frame == frame[0] ) {
            m_vertexData[0].swap( m_vertexData[1] );
        } else {
            if( frame[0] == m_vertexData[1].frame ) {
                m_vertexData[0] = m_vertexData[1];
            }
            if( frame[1] == m_vertexData[0].frame ) {
                m_vertexData[1] = m_vertexData[0];
            }
        }
    } else {
        throw std::runtime_error( "Expected 1 or 2 adjacent files, but got " +
                                  boost::lexical_cast<std::string>( fileCount ) + "." );
    }

    for( std::size_t i = 0; i < fileCount; ++i ) {
        if( m_vertexData[i].frame != frame[i] || frame[i] == std::numeric_limits<double>::max() ) {
            m_vertexData[i].clear();
            load_vertex_data( m_filenameSequence[frame[i]], frame[i], expectedVertexCount, m_vertexData[i].data );
            m_vertexData[i].frame = frame[i];
        }
        if( m_vertexData[i].frame != frame[i] ) {
            throw std::runtime_error( "Internal Error: failed to cache frame number" );
        }
        if( m_vertexData[i].data.size() != ( 12 * expectedVertexCount ) ) {
            throw std::runtime_error( "Internal Error: inconsistent vertex count" );
        }
    }
}

float MaxMeshCacheModifier::get_start_frame() {
    frantic::files::filename_sequence& sequence = m_filenameSequence;
    return !( sequence.get_frame_set().empty() ) ? ( static_cast<float>( *( sequence.get_frame_set().begin() ) ) )
                                                 : ( 0 );
}

float MaxMeshCacheModifier::get_end_frame() {
    frantic::files::filename_sequence& sequence = m_filenameSequence;
    return !( sequence.get_frame_set().empty() ) ? ( static_cast<float>( *( --sequence.get_frame_set().end() ) ) )
                                                 : ( 0 );
}

void MaxMeshCacheModifier::InitializeFPDescriptor() {
    FFCreateDescriptor c( this, MaxMeshCacheModifier_INTERFACE_ID, MaxMeshCacheModifier_INTERFACE_NAME,
                          GetMaxMeshCacheModifierDesc() );

    c.add_property( &MaxMeshCacheModifier::get_start_frame, "startFrame" );
    c.add_property( &MaxMeshCacheModifier::get_end_frame, "endFrame" );
}

void MaxMeshCacheModifier::ModifyObject( TimeValue t, ModContext& /*mc*/, ObjectState* os, INode* /*node*/ ) {
    const MCHAR* strSeq = m_pblock->GetStr( kSequenceName );
    if( !strSeq || *strSeq == '\0' ) {
        m_status = kStatusNotSet;
        m_statusMessage = "Sequence Not Set";

        IParamMap2* pMap = m_pblock->GetMap();
        if( pMap )
            SendMessage( pMap->GetHWnd(), WM_UPDATESTATUS, m_status, (LPARAM)m_statusMessage.c_str() );
        return;
    }

    try {
        // BOOL isRecording = m_pblock->GetInt( kIsRecording );
        // if( !isRecording ){

        const double frameNumber = get_current_frame( t );

        std::pair<double, double> bracketingFrameNumberPair;
        float alpha;
        const bool hasBracketingFrames =
            m_filenameSequence.get_nearest_subframe_interval( frameNumber, bracketingFrameNumberPair, alpha );
        const double bracketingFrameNumbers[2] = { bracketingFrameNumberPair.first, bracketingFrameNumberPair.second };

        if( hasBracketingFrames ) {
            const std::size_t fileCount = ( alpha == 0 ) ? 1 : 2;

            const std::size_t expectedVertexCount = (std::size_t)os->obj->NumPoints();

            fill_vertex_data( fileCount, bracketingFrameNumbers, expectedVertexCount );

            for( std::size_t i = 0; i < fileCount; ++i ) {
                if( m_vertexData[i].data.size() != expectedVertexCount * sizeof( frantic::graphics::vector3f ) ) {
                    throw std::runtime_error( "Vertex count does not match." );
                }
            }

            if( fileCount == 1 ) {
                frantic::graphics::vector3f* pVerts =
                    reinterpret_cast<frantic::graphics::vector3f*>( m_vertexData[0].data.ptr_at( 0 ) );
                for( std::size_t i = 0; i < expectedVertexCount; ++i )
                    os->obj->SetPoint( (int)i, frantic::max3d::to_max_t( pVerts[i] ) );
            } else {
                frantic::graphics::vector3f* pVerts0 =
                    reinterpret_cast<frantic::graphics::vector3f*>( m_vertexData[0].data.ptr_at( 0 ) );
                frantic::graphics::vector3f* pVerts1 =
                    reinterpret_cast<frantic::graphics::vector3f*>( m_vertexData[1].data.ptr_at( 0 ) );
                for( std::size_t i = 0; i < expectedVertexCount; ++i )
                    os->obj->SetPoint(
                        (int)i, frantic::max3d::to_max_t( ( ( 1.f - alpha ) * pVerts0[i] ) + ( alpha * pVerts1[i] ) ) );
            }
        } else {
            throw std::runtime_error( "Frame " + boost::lexical_cast<std::string>( frameNumber ) +
                                      " has no bracketing files." );
        }
        os->obj->PointsWereChanged();
        os->obj->UpdateValidity( GEOM_CHAN_NUM, Interval( t, t ) );

        m_status = kStatusOk;
        m_statusMessage = "Ok";

        IParamMap2* pMap = m_pblock->GetMap();
        if( pMap )
            SendMessage( pMap->GetHWnd(), WM_UPDATESTATUS, m_status, (LPARAM)m_statusMessage.c_str() );

        /*}else{
                int nVerts = os->obj->NumPoints();

                boost::scoped_array<float> vertData( new float[3 * nVerts] );

                float* pCurVert = vertData.get();
                for( int i = 0; i < nVerts; ++i, pCurVert += 3 ){
                        Point3 p = os->obj->GetPoint(i);
                        pCurVert[0] = p.x;
                        pCurVert[1] = p.y;
                        pCurVert[2] = p.z;
                }

                frantic::geometry::xmesh_writer meshWriter( meshSeq.get_filename_pattern()[frameNum] );
                meshWriter.write_vertex_channel( "verts", (char*)vertData.get(), frantic::channels::data_type_float32,
        3, (std::size_t)nVerts ); meshWriter.close();

                os->obj->UpdateValidity(GEOM_CHAN_NUM, Interval(t,t));
        }*/
    } catch( const std::exception& e ) {
        os->obj->UpdateValidity( GEOM_CHAN_NUM, Interval( t, t ) );

        m_status = kStatusError;
        m_statusMessage = e.what();

        IParamMap2* pMap = m_pblock->GetMap();
        if( pMap )
            SendMessage( pMap->GetHWnd(), WM_UPDATESTATUS, m_status, (LPARAM)m_statusMessage.c_str() );
    }
}

#endif // #ifdef ENABLE_MAX_MESH_CACHE_MODIFIER
