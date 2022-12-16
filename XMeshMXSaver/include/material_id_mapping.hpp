// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include <frantic/geometry/polymesh3.hpp>
#include <frantic/geometry/trimesh3.hpp>
#include <frantic/particles/streams/particle_istream.hpp>

void apply_material_id_mapping( INode* meshNode, frantic::geometry::trimesh3& mesh,
                                const std::map<INode*, std::map<int, int>>& materialIDMapping,
                                MtlID defaultMaterialID );

void apply_material_id_mapping( INode* meshNode, frantic::geometry::polymesh3_ptr mesh,
                                const std::map<INode*, std::map<int, int>>& materialIDMapping,
                                MtlID defaultMaterialID );

boost::shared_ptr<frantic::particles::streams::particle_istream> apply_material_id_mapping_particle_istream(
    boost::shared_ptr<frantic::particles::streams::particle_istream> pin, INode* meshNode,
    const std::map<INode*, std::map<int, int>>& materialIDMapping, MtlID defaultMaterialID );
