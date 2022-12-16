// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/win32/utility.hpp>

class fixed_combo_box {
    HWND m_hwndComboBox;
    const std::vector<int>& ( *m_getDisplayCodes )( void );
    frantic::tstring ( *m_getDisplayName )( int );
    std::vector<int>& m_indexToCode;

    void display_error() {
        if( m_hwndComboBox ) {
            frantic::win32::ffComboBox_Clear( m_hwndComboBox );
            m_indexToCode.clear();

            ComboBox_AddString( m_hwndComboBox, _T("<error>") );
            // SendMessage( m_hwndComboBox, CB_ADDSTRING, 0L, (LPARAM)(_T("<error>")) );
        }
    }

    void add_code( int code ) {
        if( m_getDisplayName ) {
            int ret = ComboBox_AddString( m_hwndComboBox, m_getDisplayName( code ).c_str() );
            // int ret = (int)(DWORD)SendMessage( m_hwndComboBox, CB_ADDSTRING, 0L, (LPARAM)m_getDisplayName( code
            // ).c_str() );
            if( ret == CB_ERR ) {
                throw std::runtime_error( "Internal Error: error adding string to combo box" );
            } else if( ret == CB_ERRSPACE ) {
                throw std::runtime_error( "Internal Error: insufficient space to add string to combo box" );
            } else if( ret >= 0 ) {
                m_indexToCode.push_back( code );
            } else {
                throw std::runtime_error(
                    "Internal Error: an unknown error occurred while attempting to add string to combo box" );
            }
        } else {
            throw std::runtime_error( "Internal Error: getDisplayName function pointer is NULL" );
        }
    }

    fixed_combo_box& operator=( const fixed_combo_box& );

  public:
    fixed_combo_box( HWND hwnd, int id, const std::vector<int>& ( *getDisplayCodes )(),
                     frantic::tstring ( *getDisplayName )( int ), std::vector<int>& indexToCode )
        : m_getDisplayCodes( getDisplayCodes )
        , m_getDisplayName( getDisplayName )
        , m_indexToCode( indexToCode ) {
        m_hwndComboBox = GetDlgItem( hwnd, id );
    }

    void reset_strings() {
        if( m_hwndComboBox ) {
            frantic::win32::ffComboBox_Clear( m_hwndComboBox );
            m_indexToCode.clear();

            bool error = false;

            try {
                if( m_getDisplayCodes ) {
                    const std::vector<int>& displayCodes = m_getDisplayCodes();

                    for( std::size_t i = 0; i < displayCodes.size(); ++i ) {
                        // The return value is the zero-based index of the string in the list. If an error occurs, the
                        // return value is CB_ERR. If there is insufficient space to store the new string, the return
                        // value is CB_ERRSPACE.
                        const int displayCode = displayCodes[i];
                        add_code( displayCode );
                    }
                } else {
                    throw std::runtime_error( "Internal Error: function pointer is NULL" );
                }
            } catch( const std::runtime_error& /*e*/ ) {
                error = true;
            }

            if( error ) {
                display_error();
            }
        }
    }

    void set_cur_sel_code( int code ) {
        if( m_hwndComboBox ) {
            bool err = false;
            try {
                std::vector<int>::iterator iter = std::find( m_indexToCode.begin(), m_indexToCode.end(), code );
                if( iter == m_indexToCode.end() ) {
                    reset_strings();
                    add_code( code );
                    iter = std::find( m_indexToCode.begin(), m_indexToCode.end(), code );
                    if( iter == m_indexToCode.end() ) {
                        throw std::runtime_error( "Internal Error: unable to add mystery code to combo box" );
                    } else {
                        ComboBox_SetCurSel( m_hwndComboBox, iter - m_indexToCode.begin() );
                    }
                } else {
                    ComboBox_SetCurSel( m_hwndComboBox, iter - m_indexToCode.begin() );
                }
            } catch( const std::exception& /*e*/ ) {
                err = true;
            }
            if( err ) {
                display_error();
            }
        }
    }

    int get_cur_sel_code() {
        if( m_hwndComboBox ) {
            const int sel = ComboBox_GetCurSel( m_hwndComboBox );
            if( sel >= 0 && static_cast<std::size_t>( sel ) < m_indexToCode.size() ) {
                return m_indexToCode[sel];
            } else {
                throw std::runtime_error( "Internal Error: missing list index mapping entry" );
            }
        }
        return -1;
    }
};
