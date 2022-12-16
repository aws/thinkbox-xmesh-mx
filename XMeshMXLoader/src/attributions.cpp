// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "attributions.hpp"

#include <MeshLoaderStaticInterface.hpp>

#include <frantic/strings/utf8.hpp>

#include <boost/filesystem/path.hpp>

#include <codecvt>
#include <fstream>
#include <memory>
#include <sstream>

std::wstring get_attributions() {
    const boost::filesystem::path homePath =
        frantic::strings::to_wstring( GetMeshLoaderInterface()->GetMeshLoaderHome() );
    const boost::filesystem::path attributionPath = homePath / L"Legal/third_party_licenses.txt";
    std::wifstream wideIn( attributionPath.wstring() );
    if( wideIn.fail() ) {
        return L"Could not load attributions.";
    }
    wideIn.imbue( std::locale( std::locale::empty(), new std::codecvt_utf8<wchar_t> ) );
    std::wstringstream wideStream;
    wideStream << wideIn.rdbuf();
    return wideStream.str();
}
