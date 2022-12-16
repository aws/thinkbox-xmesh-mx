// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/geometry/xmesh_reader.hpp>

#if 0
class xmesh_reader_overlapped : public frantic::geometry::xmesh_reader {
protected:
	virtual void load_xmesh_array_file( const frantic::tstring& filename, char* data, const std::string& correctDataType, size_t correctDataSize, size_t correctDataCount ) const;
public:
	xmesh_reader_overlapped( const frantic::tstring& path )
		:	xmesh_reader( path )
	{
	}
};

void load_xmesh_array_file_overlapped( const frantic::tstring& filename, char* data, const std::string& correctDataType, size_t correctDataSize, size_t correctDataCount );
#endif
