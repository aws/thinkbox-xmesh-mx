// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

class xmesh_loader_gui_resources {
    bool m_loadedIcons;
    HIMAGELIST m_himlIcons;
    HIMAGELIST m_himlLampIcons;

    xmesh_loader_gui_resources();
    ~xmesh_loader_gui_resources();

    void load_icons();

    xmesh_loader_gui_resources( const xmesh_loader_gui_resources& );
    xmesh_loader_gui_resources& operator=( const xmesh_loader_gui_resources& );

  public:
    static xmesh_loader_gui_resources& get_instance();

    void apply_custbutton_fast_forward_icon( HWND hwnd );
    void apply_custbutton_x_icon( HWND hwnd );
    void apply_custbutton_down_arrow_icon( HWND hwnd );
    void apply_custbutton_up_arrow_icon( HWND hwnd );
    void apply_custbutton_right_arrow_icon( HWND hwnd );
    void apply_custbutton_left_arrow_icon( HWND hwnd );

    void apply_custimage_clear_lamp_icon( HWND hwnd );
    void apply_custimage_warning_lamp_icon( HWND hwnd );
    void apply_custimage_error_lamp_icon( HWND hwnd );
    void apply_custimage_lamp_icon( HWND hwnd, int i );
};

xmesh_loader_gui_resources* GetXMeshLoaderGuiResources();
