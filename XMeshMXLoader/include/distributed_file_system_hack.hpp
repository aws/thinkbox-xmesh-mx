// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

#pragma comment( lib, "netapi32.lib" ) // needed for NetDfsGetClientInfo in get_dfs_client_cached_storage_filename()

/**
 *  Attempt to resolve the UNC equivalent of the the supplied path.
 *
 * @param path attempt to resolve the UNC equivalent of this path.
 * @param[out] outUniversalName if the function returns true, then this
 *		is the UNC equivalent of path.  Otherwise this is undefined.
 * @return true if the UNC equivalent could be resolved, and false otherwise.
 */
bool try_get_universal_name( const std::string& path, std::string& outUniversalName );

/**
 *  Return the cached path of a file's storage location in a Distributed File
 * System.  If no cache entry exists, then an empty string is returned
 * instead.
 *
 * @param filename  the full path of a file for which to search for a DFS
 *		storage cache entry.
 * @return  the cached path to a storage location for the filename, or an
 *		empty string if no cache entry exists.
 */
std::string get_dfs_client_cached_storage_filename( const std::string& filename );
