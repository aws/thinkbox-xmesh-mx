// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <boost/make_shared.hpp>

#include <frantic/channels/channel_map_adaptor.hpp>
#include <frantic/particles/streams/particle_istream.hpp>

namespace frantic {
namespace max3d {
namespace geometry {

class get_particle_group_mesh_particle_istream : public frantic::particles::streams::delegated_particle_istream {
  public:
    typedef int mesh_collection_index_t;
    typedef std::vector<boost::shared_ptr<frantic::geometry::trimesh3>> mesh_collection_t;
    typedef boost::shared_ptr<mesh_collection_t> mesh_collection_ptr_t;

  private:
    IParticleGroup* m_particles;
    frantic::channels::channel_accessor<mesh_collection_index_t> m_meshCollectionIndexAccessor;

    frantic::tstring m_meshCollectionIndexChannelName;

    frantic::channels::channel_map m_nativeMap; // May have to add to the native map;

    // When an argument references a channel not specified in the user-supplied channel
    // map, it will request a modified channel map from the delegate with the extra channels.
    frantic::channels::channel_map m_outMap;
    frantic::channels::channel_map_adaptor m_adaptor;
    boost::scoped_array<char> m_tempBuffer;

    std::size_t m_meshCollectionValueCount;
    // first m_meshCollectionValueCount entries are indexed by valueIndex
    // subsequent entries are indexed using m_pmeshIndexCache
    mesh_collection_ptr_t m_meshCollection;
    // add this to valueIndex (valueIndex < m_meshCollectionValueCount)
    // when indexing into m_meshCollection
    std::size_t m_meshCollectionValueIndexOffset;

    // indexed by Mesh*, if valueIndex is out of range
    // these are from the shape instance operator
    typedef std::map<const Mesh*, mesh_collection_index_t> pmesh_to_index_map_t;
    pmesh_to_index_map_t m_pmeshIndexCache;

    frantic::channels::channel_propagation_policy m_meshCPP;

    // an optional function that is run on every new mesh after copying it
    // from 3ds Max
    boost::function<void( frantic::geometry::trimesh3& )> m_meshProcessor;

    struct {
        IParticleChannelMeshR* mesh;
    } m_channels;

    void get_particle_mesh( const Mesh* in, frantic::geometry::trimesh3* out ) {
        if( in ) {
            mesh_copy( *out, *const_cast<Mesh*>( in ), m_meshCPP );
            if( m_meshProcessor ) {
                m_meshProcessor( *out );
            }
        } else {
            out->clear_and_deallocate();
        }
    }

    mesh_collection_index_t get_particle_mesh_index( const Mesh* inMesh ) {
        pmesh_to_index_map_t::iterator i = m_pmeshIndexCache.find( inMesh );
        if( i == m_pmeshIndexCache.end() ) {
            const int meshIndex = static_cast<int>( m_meshCollection->size() );
            boost::shared_ptr<frantic::geometry::trimesh3> outMesh( new frantic::geometry::trimesh3 );
            get_particle_mesh( inMesh, outMesh.get() );
            m_meshCollection->push_back( outMesh );
            m_pmeshIndexCache[inMesh] = meshIndex;
            return meshIndex;
        } else {
            return i->second;
        }
    }

    void build_mesh_collection( IParticleChannelMeshR* meshChannel ) {
        m_meshCollectionValueCount = 0;
        m_meshCollectionValueIndexOffset = m_meshCollection->size();
        if( meshChannel ) {
            const int valueCount = std::max( 0, meshChannel->GetValueCount() );
            m_meshCollection->resize( m_meshCollectionValueIndexOffset + valueCount );
            m_meshCollectionValueCount = valueCount;
            for( int valueIndex = 0; valueIndex < valueCount; ++valueIndex ) {
                const Mesh* m = meshChannel->GetValueByIndex( valueIndex );
                boost::shared_ptr<frantic::geometry::trimesh3> trimesh =
                    boost::make_shared<frantic::geometry::trimesh3>();
                m_meshCollection->at( valueIndex + m_meshCollectionValueIndexOffset ) = trimesh;
                get_particle_mesh( m, m_meshCollection->at( valueIndex + m_meshCollectionValueIndexOffset ).get() );
            }
        }
    }

    void populate_mesh_collection_index( char* p ) {
        if( !m_meshCollectionIndexAccessor.is_valid() ) {
            return;
        }

        const int index = static_cast<int>( particle_index() );

        int meshCollectionIndex = -1;

        if( m_channels.mesh ) {
            const int valueIndex = m_channels.mesh->GetValueIndex( index );
            if( valueIndex >= 0 && valueIndex < static_cast<int>( m_meshCollectionValueCount ) ) {
                meshCollectionIndex = valueIndex + static_cast<int>( m_meshCollectionValueIndexOffset );
            } else {
                meshCollectionIndex = get_particle_mesh_index( m_channels.mesh->GetValue( index ) );
            }
        }

        m_meshCollectionIndexAccessor( p ) = meshCollectionIndex;
    }

  public:
    get_particle_group_mesh_particle_istream( boost::shared_ptr<frantic::particles::streams::particle_istream> pin,
                                              IParticleGroup* particles,
                                              const frantic::tstring& meshCollectionIndexChannelName,
                                              mesh_collection_ptr_t meshCollection,
                                              const frantic::channels::channel_propagation_policy& meshCPP,
                                              boost::function<void( frantic::geometry::trimesh3& )> meshProcessor = 0 )
        : delegated_particle_istream( pin )
        , m_particles( particles )
        , m_meshCollectionIndexChannelName( meshCollectionIndexChannelName )
        , m_meshCollection( meshCollection )
        , m_meshCollectionValueCount( 0 )
        , m_meshCollectionValueIndexOffset( 0 )
        , m_meshCPP( meshCPP )
        , m_meshProcessor( meshProcessor ) {
        if( !particles ) {
            throw std::runtime_error( "get_particle_group_mesh_particle_istream Error: particles is NULL" );
        }

        IPFSystem* particleSystem = PFSystemInterface( m_particles->GetParticleSystem() );
        if( !particleSystem ) {
            throw std::runtime_error(
                "get_particle_group_mesh_particle_istream() - Could not get the IPFSystem from the IParticleGroup" );
        }

        IParticleObjectExt* particleSystemParticles = GetParticleObjectExtInterface( particleSystem );
        if( !particleSystemParticles ) {
            throw std::runtime_error( "get_particle_group_mesh_particle_istream() - Could not get the "
                                      "IParticleObjectExt from the IPFSystem" );
        }

        set_channel_map( m_delegate->get_channel_map() );

        m_nativeMap = m_delegate->get_native_channel_map();
        if( !m_nativeMap.has_channel( m_meshCollectionIndexChannelName ) ) {
            m_nativeMap.append_channel<mesh_collection_index_t>( m_meshCollectionIndexChannelName );
        }

        IObject* particleContainer = m_particles->GetParticleContainer();
        if( !particleContainer ) {
            // Apparently PFlow has started making bunk particle event objects that don't have a particle container.
            // This allows them to silently slip away instead of stopping the render. FF_LOG(warning) <<
            // "get_particle_group_mesh_particle_istream() - Could not GetParticleContainer() from the IParticleGroup"
            // << std::endl;
            return;
        }

        IParticleChannelAmountR* amountChannel = GetParticleChannelAmountRInterface( particleContainer );
        if( !amountChannel ) {
            throw std::runtime_error(
                "get_particle_group_mesh_particle_istream() - Could not get the pflow IParticleChannelAmountR" );
        }

        IChannelContainer* channelContainer = GetChannelContainerInterface( particleContainer );
        if( !channelContainer ) {
            throw std::runtime_error( "get_particle_group_mesh_particle_istream() - Could not get the pflow "
                                      "IParticleContainer interface from the supplied node" );
        }

        m_channels.mesh = GetParticleChannelShapeRInterface( channelContainer );

        build_mesh_collection( m_channels.mesh );
    }

    virtual ~get_particle_group_mesh_particle_istream() {}

    void set_channel_map( const frantic::channels::channel_map& pcm ) {
        frantic::channels::channel_map requested = m_outMap = pcm;

        m_meshCollectionIndexAccessor.reset();
        if( requested.has_channel( m_meshCollectionIndexChannelName ) ) {
            m_meshCollectionIndexAccessor =
                requested.get_accessor<mesh_collection_index_t>( m_meshCollectionIndexChannelName );
        }

        m_delegate->set_channel_map( requested );
        m_adaptor.set( m_outMap, requested );
        m_tempBuffer.reset( m_adaptor.is_identity() ? NULL : new char[requested.structure_size()] );
    }

    std::size_t particle_size() const { return m_outMap.structure_size(); }

    const frantic::channels::channel_map& get_channel_map() const { return m_outMap; }

    const frantic::channels::channel_map& get_native_channel_map() const { return m_nativeMap; }

    void set_default_particle( char* buffer ) {
        if( m_adaptor.is_identity() )
            m_delegate->set_default_particle( buffer );
        else {
            frantic::channels::channel_map_adaptor tempAdaptor( m_delegate->get_channel_map(), m_outMap );

            boost::scoped_array<char> pDefault( new char[tempAdaptor.dest_size()] );
            memset( pDefault.get(), 0, tempAdaptor.dest_size() );
            tempAdaptor.copy_structure( pDefault.get(), buffer );

            m_delegate->set_default_particle( pDefault.get() );
        }
    }

    bool get_particle( char* outBuffer ) {
        char* inBuffer = ( m_adaptor.is_identity() ) ? outBuffer : m_tempBuffer.get();

        if( !m_delegate->get_particle( inBuffer ) )
            return false;

        populate_mesh_collection_index( inBuffer );

        if( inBuffer != outBuffer )
            m_adaptor.copy_structure( outBuffer, inBuffer );

        return true;
    }

    bool get_particles( char* buffer, std::size_t& numParticles ) {
        for( std::size_t i = 0; i < numParticles; ++i ) {
            if( !this->get_particle( buffer ) ) {
                numParticles = i;
                return false;
            }
            buffer += particle_size();
        }
        return true;
    }
};

} // namespace geometry
} // namespace max3d
} // namespace frantic
