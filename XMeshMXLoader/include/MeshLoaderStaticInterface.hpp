// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/max3d/fpwrapper/static_wrapper.hpp>
#include <frantic/win32/log_window.hpp>

class MeshLoaderStaticInterface
    : public frantic::max3d::fpwrapper::FFStaticInterface<MeshLoaderStaticInterface, FP_CORE> {
    frantic::win32::log_window m_logWindow;
    bool m_logPopupError;
    // bool						m_logPopupWarning;

  public:
    /**
     * Register the interface and publish functions
     */
    MeshLoaderStaticInterface();

    /**
     * For grabbing the plugin script locations relative to the dll's
     *
     * @return the path to the plugin scripts
     */
    frantic::tstring GetMeshLoaderHome();

    /**
     * Returns a formatted version string
     *
     * @return a string of the form "Version: x.x.x.xxxxx"
     **/
    frantic::tstring get_version();

    INode* load_polymesh( const frantic::tstring& path );

    void InitializeLogging();
    void SetLoggingLevel( int loggingLevel );
    int GetLoggingLevel();
    bool GetPopupLogWindowOnError();
    void SetPopupLogWindowOnError( bool popupError );
    void FocusLogWindow();
    bool GetLogWindowVisible();
    void SetLogWindowVisible( bool visible );
    void LogMessageInternal( const frantic::tstring& msg );

    frantic::tstring ReplaceSequenceNumber( const frantic::tstring& file, float frame );

    FPValue LoadUserDataArray( const frantic::tstring& filename );
};

MeshLoaderStaticInterface* GetMeshLoaderInterface();
void InitializeMeshLoaderLogging();
