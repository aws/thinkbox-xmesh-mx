// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/channels/channel_map.hpp>
#include <frantic/geometry/trimesh3.hpp>

namespace frantic {
namespace geometry {

namespace trimesh3_particle_combine {
/**
 *  The trimesh3_particle_builder combines the particle and mesh channels to populate the output mesh.
 *
 *  If a channel is available in both the input particles and the input mesh, this specifies whether the
 * trimesh3_particle_builder should use the value from the particle or the mesh.
 */
enum preferred_source_t { preferred_source_particle, preferred_source_mesh };

/**
 *  Specify how a mesh channel should be affected by the particle's transform.
 */
enum trimesh3_transform_t {
    trimesh3_transform_none,
    trimesh3_transform_point,
    trimesh3_transform_vector,
    trimesh3_transform_normal,
    trimesh3_transform_velocity,
};
} // namespace trimesh3_particle_combine

class trimesh3_particle_builder {
  public:
    virtual ~trimesh3_particle_builder(){};

    /**
     *  Enable the output mesh vertex channel to be populated from the specified particle channel.
     *  By default the output mesh channels are populated only from the input mesh.
     *  This must be set before the first call to combine().
     *
     * @param outChannelName output vertex channel name.
     * @param inChannelName input particle channel name.
     * @param preferredSource if the channel is present in both the input particle and the input mesh,
     *	specify which one should be used.
     */
    virtual void
    assign_vertex_channel_from_particle( const frantic::tstring& outChannelName, const frantic::tstring& inChannelName,
                                         trimesh3_particle_combine::preferred_source_t preferredSource ) = 0;

    /**
     *  Enable the output mesh face channel to be populated from the specified particle channel.
     *  By default the output mesh channels are populated only from the input mesh.
     *  This must be set before the first call to combine().
     *
     * @param outChannelName output vertex channel name.
     * @param inChannelName input particle channel name.
     * @param preferredSource if the channel is present in both the input particle and the input mesh,
     *	specify which one should be used.
     */
    virtual void assign_face_channel_from_particle( const frantic::tstring& outChannelName,
                                                    const frantic::tstring& inChannelName,
                                                    trimesh3_particle_combine::preferred_source_t preferredSource ) = 0;

    /**
     *  Specify how the channel should be affected by the particle's transform.
     *  This affects channels that are sourced from both the input particle and the input mesh.
     *  This must be set before the first call to combine().
     *
     * @param channelName vertex channel name.
     * @param transformType how the vertex channel should be affected by the particle transform.
     */
    virtual void set_vertex_channel_transform_type( const frantic::tstring& channelName,
                                                    trimesh3_particle_combine::trimesh3_transform_t transformType ) = 0;

    /**
     *  Specify the desired type for the specified face channel in the output
     * mesh.
     *  This must be set before the first call to combine().
     *
     * @param channelName face channel name.
     * @param dataType the desired data type for the channel.
     * @param arity the desired arity for the channel.
     */
    virtual void set_face_channel_type( const frantic::tstring& channelName, frantic::channels::data_type_t dataType,
                                        std::size_t arity ) = 0;

    /**
     *  Add the mesh, using the specified transform, to the combined mesh.
     *  The particle is used to set channel values in the mesh according to the assignments
     * defined by assign_vertex_channel_from_particle and assign_mface_channel_from_particle.
     *
     * @param xform the particle transform.
     * @param xformTimeDerivative the particle transform's time derivative.  (This contributes to the velocity of
     *channels with transform type trimesh3_transform_velocity.)
     * @param inMesh the mesh used to represent the particle.
     * @param inParticle the input particle.
     */
    virtual void combine( const frantic::graphics::transform4f& xform,
                          const frantic::graphics::transform4f& xformTimeDerivative,
                          const frantic::geometry::trimesh3& inMesh, const char* inParticle ) = 0;

    /**
     *  Complete the combined output mesh construction and return the resulting mesh.
     *  You must not make any subsequent calls to combine().
     */
    virtual boost::shared_ptr<frantic::geometry::trimesh3> finalize() = 0;
};

/**
 *  Return a new trimesh3_particle_builder.
 *
 *  Caller is responsible for deleting the returned object.
 */
trimesh3_particle_builder* get_trimesh3_particle_builder( const frantic::channels::channel_map& particleChannelMap );

} // namespace geometry
} // namespace frantic
