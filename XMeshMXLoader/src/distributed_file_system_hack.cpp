// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <lm.h>
#include <lmdfs.h>
#include <windows.h>

#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>

#include "distributed_file_system_hack.hpp"

#ifdef ENABLE_MAX_MESH_CACHE_MODIFIER

namespace detail {

class net_api_buffer_ptr {
    LPBYTE* m_netApiBuffer;
    net_api_buffer_ptr( const net_api_buffer_ptr& );            // not implemented
    net_api_buffer_ptr& operator=( const net_api_buffer_ptr& ); // not implemented
  public:
    net_api_buffer_ptr()
        : m_netApiBuffer( 0 ) {}
    net_api_buffer_ptr( LPBYTE* netApiBuffer )
        : m_netApiBuffer( netApiBuffer ) {}
    ~net_api_buffer_ptr() {
        if( m_netApiBuffer ) {
            NetApiBufferFree( m_netApiBuffer );
        }
    }
};

} // namespace detail

// taken from frantic::files::get_universal_name
bool try_get_universal_name( const std::string& path, std::string& outUniversalName ) {
#if defined( WIN32 ) || defined( _WIN64 )

    if( path.length() < 2 || path[1] != ':' ) {
        // ehh
        outUniversalName = path;
        return true;
    }

    DWORD dwRetVal;
    WCHAR Buffer[1024];
    DWORD dwBufferLength = 1024;
    UNIVERSAL_NAME_INFO* unameinfo;
    // REMOTE_NAME_INFO *remotenameinfo;

    unameinfo = (UNIVERSAL_NAME_INFO*)&Buffer;
    dwRetVal = WNetGetUniversalName( path.c_str(), UNIVERSAL_NAME_INFO_LEVEL, (LPVOID)unameinfo, &dwBufferLength );

    if( dwRetVal == NO_ERROR ) {
        outUniversalName = std::string( unameinfo->lpUniversalName );
        return true;
    } else {
        return false;
    }
#else
    throw std::runtime_error( "files::get_universal_name():  This method is not yet implemented!" );
#endif
}

std::string get_dfs_client_cached_storage_filename( const std::string& filename ) {
    // NB: Using wchar for strings, data structures, and windows library
    // calls throughout.

    // This is probably incorrect..  Do we need to account for code pages and such?
    // Same goes for the returned string.
    std::wstring filenameW( filename.begin(), filename.end() );
    LPWSTR universalName = 0;

    DWORD universalNameBufferSize = 520;
    std::vector<boost::int8_t> universalNameVariableBuffer( universalNameBufferSize );

    // WNetGetUniversalName resolves the drive letter to a unc path
    DWORD status = WNetGetUniversalNameW( filenameW.c_str(), UNIVERSAL_NAME_INFO_LEVEL, &universalNameVariableBuffer[0],
                                          &universalNameBufferSize );
    if( status == ERROR_MORE_DATA ) {
        universalNameVariableBuffer.resize( universalNameBufferSize + 1 );
        status = WNetGetUniversalNameW( filenameW.c_str(), UNIVERSAL_NAME_INFO_LEVEL, &universalNameVariableBuffer[0],
                                        &universalNameBufferSize );
    }

    // if( status == ERROR_NOT_CONNECTED ) {
    //  not a redirected (network) device

    if( status == NO_ERROR ) {
        universalName = ( reinterpret_cast<UNIVERSAL_NAME_INFOW*>( &universalNameVariableBuffer[0] ) )->lpUniversalName;
    } else if( filenameW.length() > 0 ) {
        // probably safe, but this is a little off
        universalName = &filenameW[0];
    }

    if( universalName ) {
        PDFS_INFO_3 pinfo = 0;

        NET_API_STATUS netStatus = NetDfsGetClientInfo( universalName, 0, 0, 3, (LPBYTE*)&pinfo );
        if( netStatus == NERR_Success ) {
            detail::net_api_buffer_ptr freeNetApiBufferAfterScope( (LPBYTE*)pinfo );
            if( pinfo == 0 ) {
                throw std::runtime_error( "get_dfs_client_cached_storage_filename Error: DFS_INFO pointer is NULL" );
            }
            const DWORD volumeState = pinfo->State & DFS_VOLUME_STATES;
            const DWORD numberOfStorages = pinfo->NumberOfStorages;

            if( volumeState & ( DFS_VOLUME_STATE_OK | DFS_VOLUME_STATE_ONLINE ) ) {
                for( DWORD storageNumber = 0; storageNumber < numberOfStorages; ++storageNumber ) {
                    const DFS_STORAGE_INFO& storage = pinfo->Storage[storageNumber];
                    if( storage.State & DFS_STORAGE_STATE_ONLINE ) {
                        const std::wstring oldDir( boost::algorithm::trim_left_copy_if(
                            std::wstring( pinfo->EntryPath ), boost::bind( std::equal_to<wchar_t>(), _1, L'\\' ) ) );

                        std::wstring newDir( std::wstring( storage.ServerName ) + L"\\" + storage.ShareName );
                        // TODO: I doubt this is necessary..
                        boost::algorithm::trim_left_if( newDir, boost::bind( std::equal_to<wchar_t>(), _1, L'\\' ) );

                        std::wstring newFilenameW( universalName );
                        boost::iterator_range<std::wstring::iterator> r =
                            boost::algorithm::ifind_first( newFilenameW, oldDir );
                        if( r ) {
                            boost::algorithm::replace_range( newFilenameW, r, newDir );
                            // We may need some sanitization here.
                            // I assume that the unicode functions above can produce
                            // prefixes like \\?\ (which fopen seems to handle), and
                            // filenames longer than MAX_PATH (which regular fopen
                            // cannot handle).
                            return std::string( newFilenameW.begin(), newFilenameW.end() );
                        }
                    }
                }
            }
        }
    }

    return std::string();
}

#endif // #ifdef ENABLE_MAX_MESH_CACHE_MODIFIER
