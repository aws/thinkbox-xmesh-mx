// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "stdafx.h"

#include <tbb/task_scheduler_init.h>

// Get the class descs needed
#include "LegacyXMeshLoader.hpp"
#include "MaxMeshCacheModifier.h"
#include "XMeshLoader.hpp"

extern void InitializeMeshLoaderLogging();

BOOL gbControlsInit = FALSE;
HINSTANCE ghInstance = 0;

namespace {
// Put all the classdesc's in a vector, to make adding & removing plugins easier
std::vector<ClassDesc*> classDescList;
} // anonymous namespace

BOOL MeshLoaderInitialize() {
    classDescList.clear();
    classDescList.push_back( GetXMeshLoaderClassDesc() );
    classDescList.push_back( GetLegacyXMeshLoaderClassDesc() );
#ifdef ENABLE_MAX_MESH_CACHE_MODIFIER
    classDescList.push_back( GetMaxMeshCacheModifierDesc() );
#endif

    return true;
}

BOOL APIENTRY DllMain( HANDLE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/
) {
    if( !gbControlsInit ) {
        gbControlsInit = TRUE;

        // InitCustomControls() is deprecated in max 2009
#if MAX_VERSION_MAJOR < 11
        // init 3DStudio Max controls
        InitCustomControls( ghInstance );
#endif

        // init Windows controls
        InitCommonControls();
    }

    if( ul_reason_for_call == DLL_PROCESS_ATTACH ) {
        ghInstance = (HINSTANCE)hModule;
        return MeshLoaderInitialize();
    }

    return true;
}

TCHAR* GetString( UINT id ) {
    static TCHAR buf[256];

    if( ghInstance )
        return LoadString( ghInstance, id, buf, sizeof( buf ) / sizeof( TCHAR ) ) ? buf : NULL;
    return NULL;
}

static tbb::task_scheduler_init g_tbbScheduler( tbb::task_scheduler_init::deferred );

extern "C" {
//------------------------------------------------------
// This is the interface to 3DSMax
//------------------------------------------------------
__declspec( dllexport ) const TCHAR* LibDescription() {
    return _T("XMesh Loader - Thinkbox Software - www.thinkboxsoftware.com");
}

__declspec( dllexport ) int LibNumberClasses() { return (int)classDescList.size(); }

__declspec( dllexport ) ClassDesc* LibClassDesc( int i ) {
    if( i >= 0 && i < (int)classDescList.size() )
        return classDescList[i];
    else
        return 0;
}

// Return version so can detect obsolete DLLs
__declspec( dllexport ) ULONG LibVersion() { return VERSION_3DSMAX; }

// Let the plug-in register itself for deferred loading
__declspec( dllexport ) ULONG CanAutoDefer() {
#if MAX_VERSION_MAJOR >= 14
    return 1;
#else
    return 0;
#endif
}

__declspec( dllexport ) int LibInitialize() {
    try {
        g_tbbScheduler.initialize();
        // BEWARE!!  InitializeMeshLoaderLogging() schedules a callback from 3ds Max.
        // If LibInitialize returns FALSE after this callback is registered,
        // then 3ds Max will crash.
        InitializeMeshLoaderLogging();
        return TRUE;
    } catch( std::exception& e ) {
        Interface* coreInterface = GetCOREInterface();
        if( coreInterface ) {
            const std::string errmsg = std::string( "Unable to initialize XMesh Loader: " ) + e.what();
            LogSys* log = GetCOREInterface()->Log();
            if( log ) {
                log->LogEntry( SYSLOG_ERROR, DISPLAY_DIALOG, _T( "XMesh Loader" ), _T( "%s" ), errmsg.c_str() );
            }
        }
    }
    return FALSE;
}

__declspec( dllexport ) int LibShutdown() {
    // IMPORTANT! This is in LibShutdown since the destructor on g_tbbScheduler was hanging
    //  under certain conditions.
    g_tbbScheduler.terminate();
    return TRUE;
}

} // extern "C"
