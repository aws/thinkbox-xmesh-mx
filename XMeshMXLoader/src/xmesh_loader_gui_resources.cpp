// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "resource.h"

#include "xmesh_loader_gui_resources.hpp"

extern HINSTANCE ghInstance;

xmesh_loader_gui_resources::xmesh_loader_gui_resources()
    : m_loadedIcons( false )
    , m_himlIcons( 0 )
    , m_himlLampIcons( 0 ) {}

xmesh_loader_gui_resources::~xmesh_loader_gui_resources() {
    if( m_himlIcons ) {
        ImageList_Destroy( m_himlIcons );
        m_himlIcons = 0;
    }
    if( m_himlLampIcons ) {
        ImageList_Destroy( m_himlLampIcons );
        m_himlLampIcons = 0;
    }
}

void xmesh_loader_gui_resources::load_icons() {
    if( !m_loadedIcons ) {
        m_himlIcons = 0;

        HBITMAP hbmKrakatoaGui = (HBITMAP)LoadImage( ghInstance, MAKEINTRESOURCE( IDB_KRAKATOA_GUI ), IMAGE_BITMAP, 512,
                                                     16, LR_DEFAULTCOLOR );

        if( hbmKrakatoaGui ) {
            m_himlIcons = ImageList_Create( 16, 16, ILC_COLOR24 | ILC_MASK, 0, 32 );

            if( m_himlIcons ) {
                int err = ImageList_AddMasked( m_himlIcons, hbmKrakatoaGui, RGB( 0xff, 0xff, 0xff ) );
                if( err == -1 ) {
                    ImageList_Destroy( m_himlIcons );
                    m_himlIcons = 0;
                }
            }

            DeleteObject( hbmKrakatoaGui );
        }

        HBITMAP hbmLamps =
            (HBITMAP)LoadImage( ghInstance, MAKEINTRESOURCE( IDB_LAMPS ), IMAGE_BITMAP, 512, 16, LR_DEFAULTCOLOR );

        if( hbmLamps ) {
            m_himlLampIcons = ImageList_Create( 16, 16, ILC_COLOR24 | ILC_MASK, 0, 32 );
            if( m_himlLampIcons ) {
                int err = ImageList_AddMasked( m_himlLampIcons, hbmLamps, RGB( 0xff, 0x00, 0xff ) );
                if( err == -1 ) {
                    ImageList_Destroy( m_himlLampIcons );
                    m_himlLampIcons = 0;
                }
            }
        }

        m_loadedIcons = true;
    }
}

xmesh_loader_gui_resources& xmesh_loader_gui_resources::get_instance() {
    static xmesh_loader_gui_resources frostGuiResources;
    return frostGuiResources;
}

void xmesh_loader_gui_resources::apply_custbutton_fast_forward_icon( HWND hwnd ) {
    load_icons();
    if( hwnd && m_himlIcons ) {
        ICustButton* button = GetICustButton( hwnd );
        if( button ) {
            button->SetImage( m_himlIcons, 0, 0, 1, 1, 16, 16 );
            ReleaseICustButton( button );
        }
    }
}
void xmesh_loader_gui_resources::apply_custbutton_x_icon( HWND hwnd ) {
    load_icons();
    if( hwnd && m_himlIcons ) {
        ICustButton* button = GetICustButton( hwnd );
        if( button ) {
            button->SetImage( m_himlIcons, 13, 13, 18, 18, 16, 16 );
            ReleaseICustButton( button );
        }
    }
}

void xmesh_loader_gui_resources::apply_custbutton_down_arrow_icon( HWND hwnd ) {
    load_icons();
    if( hwnd && m_himlIcons ) {
        ICustButton* button = GetICustButton( hwnd );
        if( button ) {
            button->SetImage( m_himlIcons, 14, 14, 16, 16, 16, 16 );
            ReleaseICustButton( button );
        }
    }
}

void xmesh_loader_gui_resources::apply_custbutton_up_arrow_icon( HWND hwnd ) {
    load_icons();
    if( hwnd && m_himlIcons ) {
        ICustButton* button = GetICustButton( hwnd );
        if( button ) {
            button->SetImage( m_himlIcons, 15, 15, 17, 17, 16, 16 );
            ReleaseICustButton( button );
        }
    }
}

void xmesh_loader_gui_resources::apply_custbutton_right_arrow_icon( HWND hwnd ) {
    load_icons();
    if( hwnd && m_himlIcons ) {
        ICustButton* button = GetICustButton( hwnd );
        if( button ) {
            button->SetImage( m_himlIcons, 5, 5, 7, 7, 16, 16 );
            ReleaseICustButton( button );
        }
    }
}

void xmesh_loader_gui_resources::apply_custbutton_left_arrow_icon( HWND hwnd ) {
    load_icons();
    if( hwnd && m_himlIcons ) {
        ICustButton* button = GetICustButton( hwnd );
        if( button ) {
            button->SetImage( m_himlIcons, 6, 6, 8, 8, 16, 16 );
            ReleaseICustButton( button );
        }
    }
}

void xmesh_loader_gui_resources::apply_custimage_clear_lamp_icon( HWND hwnd ) {
    load_icons();
    if( hwnd && m_himlLampIcons ) {
        ICustImage* img = GetICustImage( hwnd );
        if( img ) {
            img->SetImage( m_himlLampIcons, 0, 16, 16 );
            ReleaseICustImage( img );
        }
    }
}

void xmesh_loader_gui_resources::apply_custimage_warning_lamp_icon( HWND hwnd ) {
    load_icons();
    if( hwnd && m_himlLampIcons ) {
        ICustImage* img = GetICustImage( hwnd );
        if( img ) {
            img->SetImage( m_himlLampIcons, 1, 16, 16 );
            ReleaseICustImage( img );
        }
    }
}

void xmesh_loader_gui_resources::apply_custimage_error_lamp_icon( HWND hwnd ) {
    load_icons();
    if( hwnd && m_himlLampIcons ) {
        ICustImage* img = GetICustImage( hwnd );
        if( img ) {
            img->SetImage( m_himlLampIcons, 2, 16, 16 );
            ReleaseICustImage( img );
        }
    }
}

void xmesh_loader_gui_resources::apply_custimage_lamp_icon( HWND hwnd, int i ) {
    load_icons();
    if( hwnd && m_himlLampIcons ) {
        ICustImage* img = GetICustImage( hwnd );
        if( img ) {
            img->SetImage( m_himlLampIcons, i, 16, 16 );
            ReleaseICustImage( img );
        }
    }
}

xmesh_loader_gui_resources* GetXMeshLoaderGuiResources() { return &xmesh_loader_gui_resources::get_instance(); }
