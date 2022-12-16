// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <tbb/task_scheduler_init.h>

extern void InitializeMeshSaverLogging();

HINSTANCE ghInstance = 0;

static BOOL gbControlsInit = FALSE;
static tbb::task_scheduler_init g_tbbScheduler( tbb::task_scheduler_init::deferred );

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
    }

    return true;
}

// Class descriptor
class FranticMaxMeshSaverDesc : public ClassDesc {
  public:
    int IsPublic() { return 0; }
    void* Create( BOOL /*loading*/ ) { return NULL; }
#if MAX_VERSION_MAJOR >= 24
    const TCHAR* NonLocalizedClassName() { return _T("XMeshSaverBase"); }
#endif
    const TCHAR* ClassName() { return _T("XMeshSaverBase"); }
    SClass_ID SuperClassID() { return HELPER_CLASS_ID; }
    Class_ID ClassID() { return Class_ID( 0x4c487536, 0x6b877632 ); }
    const TCHAR* Category() { return _T(""); }
};
FranticMaxMeshSaverDesc fmphClassDesc;

extern "C" {
//------------------------------------------------------
// This is the interface to 3DSMax
//------------------------------------------------------
__declspec( dllexport ) const TCHAR* LibDescription() {
    return _T("XMesh Saver - Thinkbox Software - www.thinkboxsoftware.com");
}

__declspec( dllexport ) int LibNumberClasses() { return 1; }

__declspec( dllexport ) ClassDesc* LibClassDesc( int /*i*/ ) { return &fmphClassDesc; }

// Return version so can detect obsolete DLLs
__declspec( dllexport ) ULONG LibVersion() { return VERSION_3DSMAX; }

// Let the plug-in register itself for deferred loading
__declspec( dllexport ) ULONG CanAutoDefer() { return 0; }

__declspec( dllexport ) int LibInitialize() {
    try {
        g_tbbScheduler.initialize();
        // BEWARE!!  InitializeMeshSaverLogging() schedules a callback from 3ds Max.
        // If LibInitialize returns FALSE after this callback is registered,
        // then 3ds Max will crash.
        InitializeMeshSaverLogging();
        return TRUE;
    } catch( std::exception& e ) {
        Interface* coreInterface = GetCOREInterface();
        if( coreInterface ) {
            const std::string errmsg = std::string( "Unable to initialize XMesh Loader: " ) + e.what();
            LogSys* log = GetCOREInterface()->Log();
            if( log ) {
                log->LogEntry( SYSLOG_ERROR, DISPLAY_DIALOG, _T( "XMesh Saver" ), _T( "%s" ), errmsg.c_str() );
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

// This can be removed when we get a version of Flex which supports Visual Studio 2015
#if MAX_VERSION_MAJOR >= 19
extern "C" {
FILE __iob_func[3] = { *stdin, *stdout, *stderr };
}
#endif