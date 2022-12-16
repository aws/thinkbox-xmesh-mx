// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#pragma warning( disable : 4511 4127 4702 4239 )

// Standard C++ headers
#include <map>
#include <string>
#include <vector>

#include <boost/function.hpp>

#include <frantic/max3d/standard_max_includes.hpp>

#ifdef base_type
#undef base_type
#endif

// link in the 3ds max libraries
#include <frantic/geometry/trimesh3_file_io.hpp>
#include <frantic/max3d/geometry/mesh.hpp>

#include "resource.h"
