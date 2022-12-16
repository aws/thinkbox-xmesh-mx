// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "xmesh_reader_overlapped.hpp"
#include "zlib_reader.hpp"

#if 0

void xmesh_reader_overlapped::load_xmesh_array_file( const std::string& filename, char* data, const std::string& correctDataType, size_t correctDataSize, size_t correctDataCount ) const {
	load_xmesh_array_file_overlapped( filename, data, correctDataType, correctDataSize, correctDataCount );
}

void load_xmesh_array_file_overlapped( const std::string& filename, char* data, const std::string& correctDataType, size_t correctDataSize, size_t correctDataCount ) {
	frantic::files::gzip_read_interface stream;

	stream.open( filename );

#pragma pack( push, 1 )
	struct {
		char header[8];
		char dataTag[12];
		boost::int32_t dataCount;
		boost::int32_t dataSize;

		std::string get_header_string() const {
			char szHeader[9];
			memcpy( szHeader, header, 8 );
			szHeader[8] = 0;
			return std::string( szHeader );
		}

		std::string get_data_tag() const {
			char szDataTag[13];
			memcpy( szDataTag, dataTag, 12 );
			szDataTag[12] = 0;
			return std::string( szDataTag );
		}
	} header;
#pragma pack( pop )

	stream.read( reinterpret_cast<char*>( & header ), 28 );

	if( header.get_header_string() != "xmeshdat" ) {
		throw std::runtime_error( "load_xmesh_array_file: File \"" + filename + "\" had invalid header." );
	}
	if( header.get_data_tag() != correctDataType ) {
		throw std::runtime_error( "load_xmesh_array_file: Input file \"" + filename + "\" has wrong data tag, \"" + header.get_data_tag() + "\".  The expected data tag is \"" + correctDataType + "\"." );
	}
	if( header.dataSize != (int)correctDataSize ) {
		throw std::runtime_error( "load_xmesh_array_file: File \"" + filename + "\" contains the wrong data size.  Expected " + boost::lexical_cast<std::string>(correctDataSize) + ", but got " + boost::lexical_cast<std::string>(header.dataSize) + "." );
	}
	if( header.dataCount != (int)correctDataCount ){
		throw std::runtime_error( "load_xmesh_array_file: File \"" + filename + "\" contains the wrong data count.  Expected " + boost::lexical_cast<std::string>(correctDataCount) + ", but got " + boost::lexical_cast<std::string>(header.dataCount) + "." );
	}

	// Read the data
	if( header.dataCount > 0 ) {
		stream.read( data, header.dataCount * header.dataSize );
	}
}

#endif
