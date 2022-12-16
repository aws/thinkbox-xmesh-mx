// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "get_particle_group_node_trimesh3.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/concatenated_particle_istream.hpp>
#include <frantic/particles/streams/particle_array_particle_istream.hpp>
#include <frantic/sort/sort.hpp>

#include <frantic/max3d/geometry/auto_mesh.hpp>
#include <frantic/max3d/geometry/null_view.hpp>
#include <frantic/max3d/geometry/polymesh.hpp>
#include <frantic/max3d/particles/streams/max_pflow_particle_istream.hpp> // TODO: remove dependence on particles?

#include "get_particle_group_mesh_particle_istream.hpp"
#include "material_id_mapping.hpp"
#include "trimesh3_particle_builder.hpp"

using namespace frantic::geometry;
using namespace frantic::max3d;
// using namespace frantic::max3d::rendering;

namespace frantic {
namespace max3d {
namespace geometry {

bool is_particle_group( INode* node ) {
    if( node ) {
        if( GetParticleGroupInterface( node->GetObjectRef() ) ) {
            return true;
        }
    }
    return false;
}

void cull_particle_system_nodes( std::vector<INode*>& nodes,
                                 std::map<INode*, std::vector<INode*>>& outParticleSystems ) {
    // keep track of nodes that aren't particle systems
    std::vector<INode*> outNodes;

    outParticleSystems.clear();

    BOOST_FOREACH( INode* node, nodes ) {
        if( !node ) {
            throw std::runtime_error( "cull_particle_systems Error: node is NULL" );
        }

        IParticleGroup* particleGroup = GetParticleGroupInterface( node->GetObjectRef() );
        INode* particleSystem = NULL;
        if( particleGroup ) {
            particleSystem = particleGroup->GetParticleSystem();
        }

        if( particleGroup && particleSystem ) {
            outParticleSystems[particleSystem].push_back( node );
        } else {
            outNodes.push_back( node );
        }
    }

    nodes.swap( outNodes );
}

namespace {
class particle_id_comparison {
    frantic::channels::channel_accessor<int> m_idAcc;

  public:
    particle_id_comparison( frantic::channels::channel_accessor<int>& idAcc )
        : m_idAcc( idAcc ) {}
    bool operator()( const char* first, const char* second ) const { return m_idAcc( first ) < m_idAcc( second ); }
};

void set_default_particle( boost::shared_ptr<frantic::particles::streams::particle_istream>& pin ) {
    const frantic::tstring scale( _T("Scale") );
    const frantic::tstring orientation( _T("Orientation") );

    const frantic::channels::channel_map& channelMap = pin->get_channel_map();

    const std::size_t structureSize = channelMap.structure_size();

    if( structureSize == 0 ) {
        return;
    }

    boost::scoped_array<char> defaultParticle( new char[structureSize] );
    memset( defaultParticle.get(), 0, structureSize );

    if( channelMap.has_channel( scale ) ) {
        frantic::channels::channel_accessor<frantic::graphics::vector3f> scaleAcc(
            channelMap.get_accessor<frantic::graphics::vector3f>( scale ) );
        scaleAcc( defaultParticle.get() ) = frantic::graphics::vector3f( 1.f );
    }

    if( channelMap.has_channel( orientation ) ) {
        frantic::channels::channel_accessor<frantic::graphics::vector4f> orientationAcc(
            channelMap.get_accessor<frantic::graphics::vector4f>( orientation ) );
        orientationAcc( defaultParticle.get() ) = frantic::graphics::vector4f( 0, 0, 0, 1.f );
    }

    pin->set_default_particle( defaultParticle.get() );
}
} // namespace

void apply_material_id_mapping_trimesh3( INode* meshNode, frantic::geometry::trimesh3& mesh,
                                         const std::map<INode*, std::map<int, int>>& materialIDMapping,
                                         MtlID defaultMaterialID ) {
    apply_material_id_mapping( meshNode, mesh, materialIDMapping, defaultMaterialID );
}

frantic::channels::channel_propagation_policy exclude_channel( const frantic::channels::channel_propagation_policy& cpp,
                                                               const frantic::tstring& channelName ) {
    frantic::channels::channel_propagation_policy result( cpp );
    if( result.is_include_list() ) {
        result.remove_channel( channelName );
    } else {
        result.add_channel( channelName );
    }
    return result;
}

frantic::channels::channel_propagation_policy include_channel( const frantic::channels::channel_propagation_policy& cpp,
                                                               const frantic::tstring& channelName ) {
    frantic::channels::channel_propagation_policy result( cpp );
    if( result.is_include_list() ) {
        result.add_channel( channelName );
    } else {
        result.remove_channel( channelName );
    }
    return result;
}

void build_particle_system_trimesh3( std::vector<INode*>& nodes, TimeValue startTime, TimeValue endTime,
                                     const frantic::channels::channel_propagation_policy& cpp,
                                     const std::map<INode*, std::map<int, int>>& materialIDMapping,
                                     MtlID defaultMaterialID, frantic::geometry::trimesh3& outMesh ) {
    if( nodes.empty() ) {
        throw std::runtime_error( "build_particle_system_trimesh3 Error: nodes list is empty" );
    }

    outMesh.clear_and_deallocate();

    frantic::channels::channel_map defaultChannelMap;
    defaultChannelMap.define_channel<frantic::graphics::vector3f>( _T("Position") );
    defaultChannelMap.end_channel_definition();

    typedef std::vector<boost::shared_ptr<frantic::geometry::trimesh3>> mesh_collection_t;
    boost::shared_ptr<mesh_collection_t> meshCollection = boost::make_shared<mesh_collection_t>();

    const frantic::channels::channel_propagation_policy cppWithoutMaterialID = exclude_channel( cpp, _T("MaterialID") );

    INode* particleSystem = NULL;

    std::vector<boost::shared_ptr<frantic::particles::streams::particle_istream>> pins;

    BOOST_FOREACH( INode* node, nodes ) {
        if( !node ) {
            throw std::runtime_error( "build_particle_system_trimesh3 Error: INode is NULL" );
        }

        IParticleGroup* particleGroup = GetParticleGroupInterface( node->GetObjectRef() );
        if( !particleGroup ) {
            throw std::runtime_error( "build_particle_system_trimesh3 Error: node does not have IParticleGroup" );
        }

        INode* nodeParticleSystem = particleGroup->GetParticleSystem();
        if( !nodeParticleSystem ) {
            throw std::runtime_error( "build_particle_system_trimesh3 Error: node does not have a particle system" );
        }

        if( particleSystem != nodeParticleSystem ) {
            if( particleSystem ) {
                throw std::runtime_error( "build_particle_system_trimesh3 Error: mismatch in node particle systems" );
            } else {
                particleSystem = nodeParticleSystem;
            }
        }

        boost::function<void( frantic::geometry::trimesh3& )> meshProcessor;

        boost::shared_ptr<frantic::particles::streams::particle_istream> pin =
            boost::make_shared<frantic::max3d::particles::streams::max_pflow_particle_istream>(
                node, startTime, defaultChannelMap, false );
        pin->set_channel_map( pin->get_native_channel_map() );
        if( !materialIDMapping.empty() && cpp.is_channel_included( _T("MaterialID") ) ) {
            if( pin->get_channel_map().has_channel( _T("MtlIndex" ) ) ) {
                pin = apply_material_id_mapping_particle_istream( pin, node, materialIDMapping, defaultMaterialID );
            } else {
                meshProcessor = boost::bind( apply_material_id_mapping_trimesh3, node, _1,
                                             boost::ref( materialIDMapping ), defaultMaterialID );
            }
        }
        const frantic::channels::channel_propagation_policy& nodeCPP =
            pin->get_channel_map().has_channel( _T("MtlIndex") ) ? cppWithoutMaterialID : cpp;
        pin = boost::make_shared<get_particle_group_mesh_particle_istream>(
            pin, particleGroup, _T("MeshCollectionIndex"), meshCollection, nodeCPP, meshProcessor );

        { // scope for temporary channel maps
            const frantic::channels::channel_map& inChannelMap = pin->get_native_channel_map();
            frantic::channels::channel_map channelMap;

            channelMap.union_channel_map( inChannelMap );

            if( !inChannelMap.has_channel( _T("Velocity") ) ) {
                channelMap.define_channel<frantic::graphics::vector3f>( _T("Velocity") );
            }
            channelMap.end_channel_definition();

            pin->set_channel_map( channelMap );
        }

        pins.push_back( pin );
    }

    boost::shared_ptr<frantic::particles::streams::particle_istream> pin =
        boost::make_shared<frantic::particles::streams::concatenated_particle_istream>( pins );

    set_default_particle( pin );

    frantic::particles::particle_array pa( pin->get_channel_map() );
    if( pin->get_channel_map().has_channel( _T("ID") ) ) {
        pa.insert_particles( pin );
        particle_id_comparison pred( pa.get_channel_map().get_accessor<int>( _T("ID") ) );
        frantic::sort::sort( pa.begin(), pa.end(), pred );
        pin = boost::make_shared<frantic::particles::streams::particle_array_particle_istream>( pa );
    }

    typedef get_particle_group_mesh_particle_istream::mesh_collection_index_t mesh_collection_index_t;

    const frantic::channels::channel_map& channelMap = pin->get_channel_map();

    frantic::channels::channel_accessor<mesh_collection_index_t> meshIndexAcc =
        channelMap.get_accessor<mesh_collection_index_t>( _T("MeshCollectionIndex") );
    frantic::channels::channel_accessor<frantic::graphics::vector3f> velocityAcc;
    if( pin->get_channel_map().has_channel( _T("Velocity") ) ) {
        velocityAcc = channelMap.get_accessor<frantic::graphics::vector3f>( _T("Velocity") );
    }
    frantic::channels::channel_accessor<frantic::graphics::vector3f> positionAcc;
    if( pin->get_channel_map().has_channel( _T("Position") ) ) {
        positionAcc = channelMap.get_accessor<frantic::graphics::vector3f>( _T("Position") );
    }
    frantic::channels::channel_accessor<frantic::graphics::vector3f> scaleAcc;
    if( pin->get_channel_map().has_channel( _T("Scale") ) ) {
        scaleAcc = channelMap.get_accessor<frantic::graphics::vector3f>( _T("Scale") );
    }
    frantic::channels::channel_accessor<frantic::graphics::vector4f> orientationAcc;
    if( pin->get_channel_map().has_channel( _T("Orientation") ) ) {
        orientationAcc = channelMap.get_accessor<frantic::graphics::vector4f>( _T("Orientation") );
    }
    frantic::channels::channel_accessor<frantic::graphics::vector4f> spinAcc;
    if( pin->get_channel_map().has_channel( _T("Spin") ) ) {
        spinAcc = channelMap.get_accessor<frantic::graphics::vector4f>( _T("Spin") );
    }

    boost::scoped_ptr<trimesh3_particle_builder> particleMeshBuilder( get_trimesh3_particle_builder( channelMap ) );

    // no transform for velocity!  it comes from the particle (world space)
    // TODO: this suggests that we need to add some more cases for velocity handling..
    // For example, we need a different transform for velocity from particles vs mesh
    particleMeshBuilder->set_vertex_channel_transform_type(
        _T("Velocity"), frantic::geometry::trimesh3_particle_combine::trimesh3_transform_velocity );
    particleMeshBuilder->set_vertex_channel_transform_type(
        _T("Normal"), frantic::geometry::trimesh3_particle_combine::trimesh3_transform_normal );
    if( channelMap.has_channel( _T("MtlIndex") ) && cpp.is_channel_included( _T("MaterialID") ) ) {
        particleMeshBuilder->set_face_channel_type( _T("MaterialID"), frantic::channels::data_type_uint16, 1 );
        // we actually prefer using the particle's MaterialID channel
        // but, if we have multiple particle groups, some of which have
        // a MtlIndex and some of which don't, then we can't tell if
        // the MtlIndex is real
        // to work around this, earlier, we removed the MaterialID
        // channel from the meshes from events that have a MtlIndex
        // channel so we can set this to preferred_source_mesh
        particleMeshBuilder->assign_face_channel_from_particle(
            _T("MaterialID"), _T("MtlIndex"), frantic::geometry::trimesh3_particle_combine::preferred_source_mesh );
    }
    for( std::size_t i = 0; i < channelMap.channel_count(); ++i ) {
        const frantic::tstring& channelName = channelMap[i].name();
        if( cpp.is_channel_included( channelName ) ) {
            if( channelName == _T("Velocity") ) {
                particleMeshBuilder->assign_vertex_channel_from_particle(
                    channelName, channelName, frantic::geometry::trimesh3_particle_combine::preferred_source_particle );
            } else if( channelName == _T("Color") ) {
                particleMeshBuilder->assign_vertex_channel_from_particle(
                    channelName, channelName, frantic::geometry::trimesh3_particle_combine::preferred_source_particle );
            } else if( channelName == _T("TextureCoord") ) {
                particleMeshBuilder->assign_vertex_channel_from_particle(
                    channelName, channelName, frantic::geometry::trimesh3_particle_combine::preferred_source_mesh );
            } else if( channelName.substr( 0, 7 ) == _T("Mapping") ) {
                particleMeshBuilder->assign_vertex_channel_from_particle(
                    channelName, channelName, frantic::geometry::trimesh3_particle_combine::preferred_source_mesh );
            }
        }
    }

    std::vector<char> bufferV( pin->get_channel_map().structure_size() );
    char* buffer = bufferV.size() > 0 ? &bufferV[0] : 0;

    while( pin->get_particle( buffer ) ) {
        mesh_collection_index_t meshIndex = meshIndexAcc( buffer );
        if( meshIndex >= 0 ) {
            frantic::graphics::vector3f position( 0 );
            if( positionAcc.is_valid() ) {
                position = positionAcc( buffer );
            }
            frantic::graphics::transform4f orientation = frantic::graphics::transform4f::identity();
            if( orientationAcc.is_valid() ) {
                orientation = orientationAcc( buffer ).quaternion_to_matrix();
            }
            frantic::graphics::transform4f scale = frantic::graphics::transform4f::identity();
            if( scaleAcc.is_valid() ) {
                const frantic::graphics::vector3f scaleVec = scaleAcc( buffer );
                scale = frantic::graphics::transform4f::from_scale( scaleVec.x, scaleVec.y, scaleVec.z );
            }
            const frantic::graphics::transform4f xform =
                frantic::graphics::transform4f::from_translation( position ) * orientation * scale;
            frantic::graphics::transform4f xformTimeDerivative = frantic::graphics::transform4f::zero();
            if( spinAcc.is_valid() ) {
                const frantic::graphics::vector4f spin = spinAcc( buffer );
                const float angleRads = spin.w;
                const frantic::graphics::vector3f axis( spin.x, spin.y, spin.z );
                const float axisMag = axis.get_magnitude();
                if( axisMag != 0 ) {
                    const frantic::graphics::vector3f axisNormalized( axis.x / axisMag, axis.y / axisMag,
                                                                      axis.z / axisMag );
                    frantic::graphics::transform4f xformNoTranslation( xform );
                    xformNoTranslation.set_translation( frantic::graphics::vector3f( 0 ) );
                    xformTimeDerivative =
                        ( float( TIME_TICKSPERSEC ) / float( endTime - startTime ) *
                          ( frantic::graphics::transform4f::from_angle_axis(
                                float( endTime - startTime ) / float( TIME_TICKSPERSEC ) * angleRads, axis ) -
                            frantic::graphics::transform4f::identity() ) ) *
                        xformNoTranslation;
                }
            }
            particleMeshBuilder->combine( xform, xformTimeDerivative, *( meshCollection->at( meshIndex ).get() ),
                                          buffer );
        }
    }

    boost::shared_ptr<frantic::geometry::trimesh3> pMesh = particleMeshBuilder->finalize();
    pMesh->swap( outMesh );
}

void build_particle_group_trimesh3( INode* node, TimeValue startTime, TimeValue endTime,
                                    const frantic::channels::channel_propagation_policy& cpp,
                                    frantic::geometry::trimesh3& outMesh ) {
    std::vector<INode*> nodes;
    nodes.push_back( node );
    std::map<INode*, std::map<int, int>> materialIDMapping;
    MtlID defaultMaterialID = 0;
    build_particle_system_trimesh3( nodes, startTime, endTime, cpp, materialIDMapping, defaultMaterialID, outMesh );
}

void get_particle_system_trimesh3( std::vector<INode*>& nodes, TimeValue startTime, TimeValue endTime,
                                   frantic::geometry::trimesh3& outMesh, max_interval_t& outValidityInterval,
                                   bool ignoreEmptyMeshes, const frantic::channels::channel_propagation_policy& cpp,
                                   const std::map<INode*, std::map<int, int>>& materialIDMapping,
                                   MtlID defaultMaterialID ) {
    outMesh.clear();

    if( nodes.empty() ) {
        throw std::runtime_error( "get_particle_system_trimesh3 Error: nodes list is empty" );
    }

    build_particle_system_trimesh3( nodes, startTime, endTime, cpp, materialIDMapping, defaultMaterialID, outMesh );

    if( outMesh.vertex_count() == 0 ) {
        if( ignoreEmptyMeshes ) {
            mprintf( _T("ignoring empty mesh\n") );
        } else {
            // TODO: this check should be per-node, and not for the whole collection!
            std::stringstream ss;
            ss << "get_particle_system_trimesh3 - The sampled mesh for nodes: ";
            BOOST_FOREACH( INode* node, nodes ) {
                ss << "\"" << frantic::strings::to_string( node->GetName() ) << "\" ";
            }
            ss << "does not have any vertices";
            throw std::runtime_error( ss.str() );
        }
    }

    outValidityInterval.first = startTime;
    outValidityInterval.second = startTime;
}

frantic::geometry::polymesh3_ptr
get_particle_system_polymesh3( std::vector<INode*>& nodes, TimeValue startTime, TimeValue endTime,
                               bool ignoreEmptyMeshes, const frantic::channels::channel_propagation_policy& cpp,
                               const std::map<INode*, std::map<int, int>>& materialIDMapping,
                               MtlID defaultMaterialID ) {
    if( nodes.empty() ) {
        throw std::runtime_error( "get_particle_system_polymesh3 Error: nodes list is empty" );
    }

    const frantic::channels::channel_propagation_policy cppWithEdgeVisibility =
        include_channel( cpp, _T("FaceEdgeVisibility") );

    frantic::geometry::trimesh3 trimesh;
    build_particle_system_trimesh3( nodes, startTime, endTime, cppWithEdgeVisibility, materialIDMapping,
                                    defaultMaterialID, trimesh );

    Mesh mesh;
    mesh_copy( mesh, trimesh );
    trimesh.clear_and_deallocate();

    MNMesh polymesh;
    polymesh.SetFromTri( mesh );
    mesh.FreeAll();

    make_polymesh( polymesh );

    frantic::geometry::polymesh3_ptr result = from_max_t( polymesh, cpp );

    if( result->vertex_count() == 0 ) {
        if( ignoreEmptyMeshes ) {
            mprintf( _T("ignoring empty mesh\n") );
        } else {
            // TODO: this check should be per-node, and not for the whole collection!
            std::stringstream ss;
            ss << "get_particle_system_polymesh3 - The sampled mesh for nodes: ";
            BOOST_FOREACH( INode* node, nodes ) {
                ss << "\"" << frantic::strings::to_string( node->GetName() ) << "\" ";
            }
            ss << "does not have any vertices";
            throw std::runtime_error( ss.str() );
        }
    }

    return result;
}

} // namespace geometry
} // namespace max3d
} // namespace frantic
