# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

project( XMeshMXSaver )

find_package( thinkboxcmlibrary REQUIRED )
include( PrecompiledHeader )
include( ThinkboxCMLibrary )

set( SUPPORTED_MAX_VERSIONS 2022 2023 )

option( MAX_VERSION "The version of the 3ds Max SDK to build the library against." 2022 )

if( NOT MAX_VERSION IN_LIST SUPPORTED_MAX_VERSIONS )
  message( FATAL_ERROR "ERROR: Cannot build for unsupported 3ds Max version ${MAX_VERSION}" )
endif()

add_library( xmeshmxsaver SHARED )

set_target_properties( xmeshmxsaver PROPERTIES OUTPUT_NAME "XMeshSaver" )
set_target_properties( xmeshmxsaver PROPERTIES PREFIX "" )
set_target_properties( xmeshmxsaver PROPERTIES SUFFIX ".dlo" )

target_include_directories( xmeshmxsaver PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include> )

target_include_directories( xmeshmxsaver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )

file( GLOB H_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "include/*.hpp" "include/*.h" )	
file( GLOB CXX_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp" )

target_sources( xmeshmxsaver PRIVATE
    "MeshSaver.rc"
    "XMeshSaverVersion.rc"
    "about.rc"
    "MeshSaver.def"
    "resource.h"
    "stdafx.h"
    "stdafx.cpp"
    ${H_FILES}
    ${CXX_FILES}
)

add_precompiled_header( xmeshmxsaver stdafx.h SOURCE_CXX stdafx.cpp )

# The Conan version of Boost was built with this, and it changes the library names.
# As a result, we need to set this to tell Boost to look for the right libraries to
# link against.
target_compile_definitions( xmeshmxsaver PUBLIC BOOST_AUTO_LINK_SYSTEM )

find_package( thinkboxlibrary REQUIRED )
find_package( thinkboxmxlibrary REQUIRED )
find_package( maxsdk REQUIRED )
find_package( libb2 REQUIRED )
find_package( Boost REQUIRED )
find_package( OpenEXR REQUIRED )
find_package( ZLIB REQUIRED )
find_package( TBB REQUIRED )
find_package( tinyxml2 REQUIRED )
find_package( xxHash REQUIRED )

target_include_directories( xmeshmxsaver PUBLIC ${thinkboxlibrary_INCLUDE_DIRS} )
target_include_directories( xmeshmxsaver PUBLIC ${maxsdk_INCLUDE_DIRS} )
target_include_directories( xmeshmxsaver PUBLIC ${thinkboxmxlibrary_INCLUDE_DIRS} )
target_include_directories( xmeshmxsaver PUBLIC ${libb2_INCLUDE_DIRS} )
target_include_directories( xmeshmxsaver PUBLIC ${Boost_INCLUDE_DIRS} )
target_include_directories( xmeshmxsaver PUBLIC ${OpenEXR_INCLUDE_DIRS} )
target_include_directories( xmeshmxsaver PUBLIC ${ZLIB_INCLUDE_DIRS} )
target_include_directories( xmeshmxsaver PUBLIC ${TBB_INCLUDE_DIRS} )
target_include_directories( xmeshmxsaver PUBLIC ${tinyxml2_INCLUDE_DIRS} )
target_include_directories( xmeshmxsaver PUBLIC ${xxHash_INCLUDE_DIRS} )

target_link_libraries( xmeshmxsaver PUBLIC thinkboxlibrary::thinkboxlibrary )
target_link_libraries( xmeshmxsaver PUBLIC maxsdk::maxsdk )
target_link_libraries( xmeshmxsaver PUBLIC thinkboxmxlibrary::thinkboxmxlibrary )
target_link_libraries( xmeshmxsaver PUBLIC libb2::libb2 )
target_link_libraries( xmeshmxsaver PUBLIC Boost::Boost )
target_link_libraries( xmeshmxsaver PUBLIC OpenEXR::OpenEXR )
target_link_libraries( xmeshmxsaver PUBLIC ZLIB::ZLIB )
target_link_libraries( xmeshmxsaver PUBLIC TBB::tbb )
target_link_libraries( xmeshmxsaver PUBLIC tinyxml2::tinyxml2 )
target_link_libraries( xmeshmxsaver PUBLIC xxHash::xxHash )

frantic_common_platform_setup( xmeshmxsaver )
frantic_default_source_groups( xmeshmxsaver HEADERDIR include SOURCEDIR src )

# Disable optimization for the RelWithDebInfo configuration.
# This allows breakpoints to be hit reliably when debugging in Visual Studio.
target_compile_options( xmeshmxsaver PRIVATE "$<$<CONFIG:Release>:/O2>$<$<CONFIG:RelWithDebInfo>:/Od>" )

install( TARGETS xmeshmxsaver
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib
          ARCHIVE DESTINATION lib
)
