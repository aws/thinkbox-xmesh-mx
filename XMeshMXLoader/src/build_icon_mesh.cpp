// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <boost/shared_ptr.hpp>

void BuildMesh_XMeshLogoMesh( Mesh& outMesh );
void BuildMesh_MeshLoaderLogo( Mesh& outMesh );

boost::shared_ptr<Mesh> BuildIconMesh() {
    boost::shared_ptr<Mesh> result( new Mesh() );
    BuildMesh_XMeshLogoMesh( *result );
    return result;
}

boost::shared_ptr<Mesh> BuildLegacyIconMesh() {
    boost::shared_ptr<Mesh> result( new Mesh() );
    BuildMesh_MeshLoaderLogo( *result );
    return result;
}
