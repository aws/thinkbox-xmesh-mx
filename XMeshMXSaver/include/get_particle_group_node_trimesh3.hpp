// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/geometry/trimesh3.hpp>
#include <frantic/max3d/geometry/mesh_request.hpp>

namespace frantic {
namespace max3d {
namespace geometry {

/**
 * Return true if the node is a ParticleGroup.
 *
 * If this function returns false, then get_particle_group_node_trimesh3()
 * will throw an exception.
 *
 * @param  node		the node to check.
 * @return true if the node is a ParticleGroup, and false otherwise.
 */
bool is_particle_group( INode* node );

/**
 *  Move IParticleGroup nodes from nodes into outParticleSystems, which is
 * a map from particle system INodes (which were *not* originally part of
 * nodes) to the IParticleGroup inodes within that particle system.
 *
 * @param[in,out] nodes a list of nodes to filter.
 * @param[out]    outParticleSystems a map of particle system nodes to the
 *                IParticleGroup nodes within that particle system.
 */
void cull_particle_system_nodes( std::vector<INode*>& nodes,
                                 std::map<INode*, std::vector<INode*>>& outParticleSystems );

/**
 * Build a mesh from a collection of ParticleGroup nodes.  The nodes
 * must all belong to the same particle system.
 *
 * @param  meshNodes	the nodes to create a mesh from
 * @param  startTime	the startTime of the interval at which to compute the velocity
 * @param  endTime		the endTime of the interval at which to compute the velocity
 * @param  outMesh		the output trimesh with velocity channel
 * @param  outValidity	the validity of the returned trimesh
 * @param  ignoreEmptyMeshes	ignore when the mesh is empty, otherwise throw an exception
 */
void get_particle_system_trimesh3( std::vector<INode*>& nodes, TimeValue startTime, TimeValue endTime,
                                   frantic::geometry::trimesh3& outMesh, max_interval_t& outValidityInterval,
                                   bool ignoreEmptyMeshes, const frantic::channels::channel_propagation_policy& cpp,
                                   const std::map<INode*, std::map<int, int>>& materialIDMapping,
                                   MtlID defaultMaterialID );

/**
 * Build a mesh from a collection of ParticleGroup nodes.  The nodes
 * must all belong to the same particle system.
 *
 * @param  meshNodes	the nodes to create a mesh from
 * @param  startTime	the startTime of the interval at which to compute the velocity
 * @param  endTime		the endTime of the interval at which to compute the velocity
 * @param  ignoreEmptyMeshes	ignore when the mesh is empty, otherwise throw an exception
 * @param  cpp			channel propagation policy to apply to the output mesh
 * @param  materialIDMapping
 * @param  defaultMaterialID
 * @return the output mesh
 */
frantic::geometry::polymesh3_ptr
get_particle_system_polymesh3( std::vector<INode*>& nodes, TimeValue startTime, TimeValue endTime,
                               bool ignoreEmptyMeshes, const frantic::channels::channel_propagation_policy& cpp,
                               const std::map<INode*, std::map<int, int>>& materialIDMapping, MtlID defaultMaterialID );

} // namespace geometry
} // namespace max3d
} // namespace frantic
