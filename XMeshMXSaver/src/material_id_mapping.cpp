// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "material_id_mapping.hpp"

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include <frantic/particles/streams/apply_function_particle_istream.hpp>
#include <frantic/particles/streams/set_channel_particle_istream.hpp>

class material_id_mapper {
    const std::map<int, int>& m_materialIDMapping;
    int m_endMaterialID;
    MtlID m_defaultMaterialID;

    material_id_mapper& operator=( const material_id_mapper& ); // not implemented

  public:
    material_id_mapper( const std::map<int, int>& materialIDMapping, MtlID defaultMaterialID );
    MtlID apply( MtlID materialID );
    MtlID apply_int( int materialID );
};

material_id_mapper::material_id_mapper( const std::map<int, int>& materialIDMapping, MtlID defaultMaterialID )
    : m_materialIDMapping( materialIDMapping )
    , m_defaultMaterialID( defaultMaterialID ) {
    MtlID maxMtlId = 0;
    for( std::map<int, int>::const_iterator i = m_materialIDMapping.begin(); i != m_materialIDMapping.end(); ++i ) {
        if( i->first > maxMtlId ) {
            maxMtlId = static_cast<MtlID>( i->first );
        }
    }
    m_endMaterialID = 1 + maxMtlId;
}

MtlID material_id_mapper::apply( MtlID materialID ) {
    if( materialID >= m_endMaterialID ) {
        materialID = static_cast<MtlID>( materialID % m_endMaterialID );
    }
    std::map<int, int>::const_iterator i = m_materialIDMapping.find( materialID );
    if( i == m_materialIDMapping.end() ) {
        materialID = m_defaultMaterialID;
    } else {
        if( i->second < 0 ) {
            materialID = m_defaultMaterialID;
        } else {
            materialID = static_cast<MtlID>( i->second );
        }
    }
    return materialID;
}

MtlID material_id_mapper::apply_int( int materialID ) {
    if( materialID >= m_endMaterialID ) {
        materialID = materialID % m_endMaterialID;
    }
    return apply( static_cast<MtlID>( materialID ) );
}

template <class MeshType, class DataType>
struct mesh_channel_type_traits {};

template <class DataType>
struct mesh_channel_type_traits<frantic::geometry::trimesh3, DataType> {
    typedef frantic::geometry::trimesh3_face_channel_accessor<DataType> face_channel_accessor_t;
    static DataType get_value( face_channel_accessor_t& acc, std::size_t i ) { return acc[i]; }
    static void set_value( face_channel_accessor_t& acc, std::size_t i, DataType val ) { acc[i] = val; }
};

template <class DataType>
struct mesh_channel_type_traits<frantic::geometry::polymesh3, DataType> {
    typedef frantic::geometry::polymesh3_face_accessor<DataType> face_channel_accessor_t;
    static DataType get_value( face_channel_accessor_t& acc, std::size_t i ) { return acc.get_face( i ); }
    static void set_value( face_channel_accessor_t& acc, std::size_t i, DataType val ) { acc.get_face( i ) = val; }
};

template <class T>
struct mesh_type_traits {};

template <>
struct mesh_type_traits<frantic::geometry::trimesh3> {
    typedef frantic::geometry::trimesh3 mesh_t;
    // typedef frantic::geometry::trimesh3_face_channel_accessor face_channel_accessor_t;
    // static std::size_t get_face_count( mesh_t & mesh ) {
    //		return mesh.face_count();
    //	}
    template <class T>
    static typename mesh_channel_type_traits<mesh_t, T>::face_channel_accessor_t
    get_face_channel_accessor( mesh_t& mesh, const frantic::tstring& channelName ) {
        return mesh.get_face_channel_accessor<T>( channelName );
    }
    template <class T>
    static void add_face_channel( mesh_t& mesh, const frantic::tstring& channelName ) {
        mesh.add_face_channel<T>( channelName );
    }
};

template <>
struct mesh_type_traits<frantic::geometry::polymesh3> {
    typedef frantic::geometry::polymesh3 mesh_t;
    // static std::size_t get_face_count( mesh_t & mesh ) {
    //		return mesh.face_count();
    //	}
    template <class T>
    static typename mesh_channel_type_traits<mesh_t, T>::face_channel_accessor_t
    get_face_channel_accessor( mesh_t& mesh, const frantic::tstring& channelName ) {
        return mesh.get_face_accessor<T>( channelName );
    }
    template <class T>
    static void add_face_channel( mesh_t& mesh, const frantic::tstring& channelName ) {
        mesh.add_empty_face_channel( channelName, frantic::channels::channel_data_type_traits<T>::data_type(),
                                     frantic::channels::channel_data_type_traits<T>::arity() );
    }
};

template <typename T>
void apply_material_id_mapping( INode* meshNode, T& mesh, const std::map<INode*, std::map<int, int>>& materialIDMapping,
                                MtlID defaultMaterialID ) {
    const std::map<int, int>* mapping = 0;
    if( mesh.has_face_channel( _T("MaterialID") ) ) {
        std::map<INode*, std::map<int, int>>::const_iterator i = materialIDMapping.find( meshNode );
        if( i != materialIDMapping.end() ) {
            mapping = &i->second;
        }
    } else {
        mesh_type_traits<T>::add_face_channel<MtlID>( mesh, _T("MaterialID") );
    }

    typename mesh_channel_type_traits<typename T, typename MtlID>::face_channel_accessor_t acc =
        mesh_type_traits<T>::get_face_channel_accessor<typename MtlID>( mesh, _T("MaterialID") );

    if( mapping ) {
        material_id_mapper materialIDMapper( *mapping, defaultMaterialID );

        for( std::size_t faceIndex = 0, faceIndexEnd = mesh.face_count(); faceIndex != faceIndexEnd; ++faceIndex ) {
            MtlID mtlId = mesh_channel_type_traits<T, MtlID>::get_value( acc, faceIndex );
            mtlId = materialIDMapper.apply( mtlId );
            mesh_channel_type_traits<T, MtlID>::set_value( acc, faceIndex, mtlId );
        }
    } else {
        for( std::size_t faceIndex = 0, faceIndexEnd = mesh.face_count(); faceIndex != faceIndexEnd; ++faceIndex ) {
            mesh_channel_type_traits<T, MtlID>::set_value( acc, faceIndex, defaultMaterialID );
        }
    }
}

void apply_material_id_mapping( INode* meshNode, frantic::geometry::trimesh3& mesh,
                                const std::map<INode*, std::map<int, int>>& materialIDMapping,
                                MtlID defaultMaterialID ) {
    apply_material_id_mapping<frantic::geometry::trimesh3>( meshNode, mesh, materialIDMapping, defaultMaterialID );
}

void apply_material_id_mapping( INode* meshNode, frantic::geometry::polymesh3_ptr mesh,
                                const std::map<INode*, std::map<int, int>>& materialIDMapping,
                                MtlID defaultMaterialID ) {
    if( !mesh ) {
        throw std::runtime_error( "apply_material_id_mapping Error: mesh is NULL" );
    }
    apply_material_id_mapping<frantic::geometry::polymesh3>( meshNode, *mesh, materialIDMapping, defaultMaterialID );
}

boost::shared_ptr<frantic::particles::streams::particle_istream> apply_material_id_mapping_particle_istream(
    boost::shared_ptr<frantic::particles::streams::particle_istream> pin, INode* meshNode,
    const std::map<INode*, std::map<int, int>>& materialIDMapping, MtlID defaultMaterialID ) {
    std::map<INode*, std::map<int, int>>::const_iterator i = materialIDMapping.find( meshNode );
    if( i == materialIDMapping.end() ) {
        return boost::make_shared<frantic::particles::streams::set_channel_particle_istream<int>>( pin, _T("MtlIndex"),
                                                                                                   defaultMaterialID );
    } else {
        boost::shared_ptr<material_id_mapper> materialIDMapper =
            boost::make_shared<material_id_mapper>( i->second, defaultMaterialID );
        boost::array<frantic::tstring, 1> inChannels = { _T("MtlIndex") };
        return boost::make_shared<frantic::particles::streams::apply_function_particle_istream<int( int )>>(
            pin, boost::bind( &material_id_mapper::apply_int, materialIDMapper, _1 ), _T("MtlIndex"), inChannels );
    }
}
