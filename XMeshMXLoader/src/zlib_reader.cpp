// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#if 0
#include <frantic/files/zlib_reader.hpp>

namespace frantic{ namespace files{ namespace detail{

#ifdef WIN32
win32_reader::win32_reader(){
	m_fileHandle = INVALID_HANDLE_VALUE;
	m_buffer = NULL;
	ZeroMemory( &m_overlapped, sizeof(OVERLAPPED) );
}

win32_reader::~win32_reader(){
	if( m_buffer ){
		VirtualFree( m_buffer, 0, MEM_RELEASE );
		m_buffer = NULL;
	}
	close();
}

void win32_reader::open( const boost::filesystem::path& filePath ){
	if( m_fileHandle != INVALID_HANDLE_VALUE )
		throw std::runtime_error( "win32_reader::open() Called on an already open stream!" );
		
	m_filePath = filePath;
	m_fileOffset.QuadPart = 0;
	m_fileSize.QuadPart = 0;
	m_readOffset = 0;
		
	m_fileHandle = CreateFile( filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | /*FILE_FLAG_NO_BUFFERING |*/ FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, NULL );
	//m_fileHandle = CreateFile( filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL );
	if( m_fileHandle == INVALID_HANDLE_VALUE ){
		if( GetLastError() == ERROR_FILE_NOT_FOUND )
			throw std::runtime_error( "win32_reader::open() File not found: " + m_filePath.string() );
		else
			throw std::runtime_error( "win32_reader::open() CreateFile() failed: " + frantic::win32::GetLastErrorMessageA() );
	}

	if( !GetFileSizeEx( m_fileHandle, &m_fileSize ) )
		throw std::runtime_error( "win32_reader::open() GetFileSizeEx() failed: " + frantic::win32::GetLastErrorMessageA() );
		
	SYSTEM_INFO si;
	ZeroMemory( &si, sizeof(SYSTEM_INFO) );

	GetSystemInfo( &si );

	//m_singleBufferSize = (1 << 8) * si.dwPageSize; //si.dwPageSize is expected to be 4096 so let's allocate 256 of them to get a 1MB read size.
	m_singleBufferSize = 16 * si.dwAllocationGranularity; //Expected to be 64Kb so let's make a single buffer be 16 of these 

	//m_buffer.reset( new char[2 * m_singleBufferSize] );
	m_buffer = VirtualAlloc( NULL, 2 * m_singleBufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	if( !m_buffer )
		throw std::runtime_error( "win32_reader::open() VirtualAlloc of size: " + boost::lexical_cast<std::string>(2*m_singleBufferSize) + " failed for file \"" + filePath.string() + "\"" );

	m_eof = FALSE;
	m_curBuffer = 0;		
	m_lastSyncReadSize = 0;

	ZeroMemory( &m_overlapped, sizeof(OVERLAPPED) );

	m_overlapped.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	DWORD readSize = static_cast<DWORD>(m_singleBufferSize);
	DWORD fileLeft = static_cast<DWORD>(m_fileSize.QuadPart - m_fileOffset.QuadPart);
	//if( fileLeft < readSize )
	//	readSize = fileLeft;

	if( fileLeft == 0 ){
		m_eof = TRUE;
	}else{
		m_overlapped.Offset     = m_fileOffset.LowPart;
		m_overlapped.OffsetHigh = m_fileOffset.HighPart;

		DWORD syncReadSize;

		BOOL ioResult = ReadFile( m_fileHandle, (char*)m_buffer, readSize, &syncReadSize, &m_overlapped );
		if( !ioResult ){
			switch( GetLastError() ){
			case ERROR_IO_PENDING:
				m_lastSyncReadSize = (DWORD)-1;
				break;
			default:
				throw std::runtime_error( "win32_reader::open() ReadFile() failed: " + frantic::win32::GetLastErrorMessageA() );
			}
		}else{
			m_lastSyncReadSize = syncReadSize;
		}
	}
}

void win32_reader::close(){
	if( m_fileHandle != INVALID_HANDLE_VALUE ){
		if( m_overlapped.hEvent )
			CloseHandle( m_overlapped.hEvent );

		m_overlapped.hEvent = NULL;

		if( !CloseHandle( m_fileHandle ) && !std::uncaught_exception() )
			throw std::runtime_error( "win32_reader::close() Failed to close handle for: " + m_filePath.string() );

		m_fileHandle = INVALID_HANDLE_VALUE;
	}
}

void win32_reader::seekg( std::ios::off_type off ){
	//NOTE: We currently only support seeking before the first call to read().
	//
	//This limitation can be overcome by adding a check that discards the current async read and repositions the 
	//file pointer before restarting the read.
	if( m_fileOffset.QuadPart == 0 )
		m_readOffset = std::max( 0ul, std::min( m_singleBufferSize, static_cast<DWORD>( off ) ) );
	return;
}

char* win32_reader::read( std::size_t& outSize ){
	if( m_eof || m_fileHandle == INVALID_HANDLE_VALUE )
		return NULL;

	char* outBuffer;
		
	DWORD numBytes;
	if( m_lastSyncReadSize == (DWORD)-1 ){
		BOOL ioResult = GetOverlappedResult( m_fileHandle, &m_overlapped, &numBytes, TRUE );
		if( !ioResult )
			throw std::runtime_error( "win32_reader::read() GetOverlappedResult() failed: " + frantic::win32::GetLastErrorMessageA() );
	}else{
		numBytes = m_lastSyncReadSize;
	}

	outSize = numBytes - m_readOffset;
	outBuffer = (char*)m_buffer + m_singleBufferSize * m_curBuffer + m_readOffset;

	m_readOffset = 0;
	m_fileOffset.QuadPart += numBytes;

	DWORD readSize = static_cast<DWORD>(m_singleBufferSize);
	DWORD fileLeft = static_cast<DWORD>(m_fileSize.QuadPart - m_fileOffset.QuadPart);
		
	if( fileLeft == 0 ){
		m_eof = TRUE;
	}else{
		m_curBuffer = (++m_curBuffer % 2);

		char* nextBuffer = (char*)m_buffer + m_singleBufferSize * m_curBuffer;

		m_overlapped.Offset     = m_fileOffset.LowPart;
		m_overlapped.OffsetHigh = m_fileOffset.HighPart;

		DWORD syncReadSize;

		BOOL ioResult = ReadFile( m_fileHandle, nextBuffer, readSize, &syncReadSize, &m_overlapped );
		if( !ioResult ){
			switch( GetLastError() ){
			case ERROR_IO_PENDING:
				m_lastSyncReadSize = (DWORD)-1;
				break;
			default:
				throw std::runtime_error( "win32_reader::read() ReadFile() failed: " + frantic::win32::GetLastErrorMessageA() );
			}
		}else{
			m_lastSyncReadSize = syncReadSize;
		}
	}

	if( outSize > m_singleBufferSize )
		throw std::runtime_error( "win32_reader::read() Internal error: Returned buffer has wrong size" );

	return outBuffer;
}

#else
basic_reader::basic_reader(){
	m_bufferSize = (1<<20); //1Mb read buffer
	m_buffer.reset( new char[m_bufferSize] );
	m_file = NULL;
}

basic_reader::~basic_reader(){
	close();
}

void basic_reader::open( const boost::filesystem::path& filePath ){
	if( m_file )
		throw std::runtime_error( "basic_reader::open() The file: \"" + filePath.string() + "\" is already open" );
	
	m_file = frantic::files::fopen( filePath, "r" );
	if( !m_file )
		throw std::runtime_error( "basic_reader::open() Could not open file \"" + filePath.string() + "\"" );
}
	
void basic_reader::close(){
	if( m_file ){
		fclose( m_file );
		m_file = NULL;
	}
}

void basic_reader::seekg( std::ios::off_type off ){
	fseek( m_file, off, SEEK_SET );
}

char* basic_reader::read( std::size_t& outSize ){
	std::size_t numRead = fread( m_buffer.get(), 1, m_bufferSize, m_file );
	if( numRead != m_bufferSize ){
		if( ferror( m_file ) )
			throw std::runtime_error( "basic_reader::open() Error reading from file" );
	}
	
	outSize = numRead;

	return m_buffer.get();
}
#endif

}}}}
#endif
