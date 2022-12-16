// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <boost/array.hpp>
#include <frantic/files/filename_sequence.hpp>
#include <frantic/max3d/fpwrapper/mixin_wrapper.hpp>
#include <max.h>

#define MaxMeshCacheModifier_CLASS_ID Class_ID( 0x5e8e6288, 0x49c630fc )
#define MaxMeshCacheModifier_INTERFACE_ID Interface_ID( 0x7bd64a25, 0x63420779 )
ClassDesc2* GetMaxMeshCacheModifierDesc();

enum MaxMeshCacheModifierStatus { kStatusOk, kStatusNotSet, kStatusError };

struct vertex_cache_entry {
    vertex_cache_entry()
        : frame( std::numeric_limits<double>::max() ) {}
    void clear() {
        frame = std::numeric_limits<double>::max();
        data.clear();
    }
    void clear_and_deallocate() {
        clear();
        data.trim();
    }
    void swap( vertex_cache_entry& other ) {
        std::swap( frame, other.frame );
        data.swap( other.data );
    }
    double frame;
    frantic::graphics::raw_byte_buffer data;
};

class MaxMeshCacheModifier : public OSModifier,
                             public frantic::max3d::fpwrapper::FFMixinInterface<MaxMeshCacheModifier> {
    IParamBlock2* m_pblock;

    // Store status messages to display to the user.
    MaxMeshCacheModifierStatus m_status;
    std::string m_statusMessage;

    frantic::files::filename_sequence m_filenameSequence;
    std::string m_lastVertexFilename;
    boost::array<vertex_cache_entry, 2> m_vertexData;

    friend class XMeshAnimCachePBAccessor;
    friend class MaxMeshCacheModifierDialogProc;

    int get_current_frame( TimeValue t, Interval& outValid );

    double get_current_frame( TimeValue t );
    void read_verts_from_xmesh_file( const std::string& filename, const std::size_t expectedVertexCount,
                                     frantic::graphics::raw_byte_buffer& vertexBuffer );
    void reset_filename_sequence( const std::string& filename );
    void load_vertex_data( const std::string& xmeshFilename, const double frameNumber,
                           const std::size_t expectedVertexCount, frantic::graphics::raw_byte_buffer& outVertexData );
    void fill_vertex_data( const std::size_t fileCount, const double frame[2], const std::size_t expectedVertexCount );

    void InitializeFPDescriptor();

    float get_start_frame();
    float get_end_frame();

  public:
    MaxMeshCacheModifier();
    virtual ~MaxMeshCacheModifier();

    // From Animatable
    virtual int NumSubs();
    virtual Animatable* SubAnim( int i );
#if MAX_VERSION_MAJOR >= 24
    virtual MSTR SubAnimName( int i, bool localized );
#else
    virtual MSTR SubAnimName( int i );
#endif

    virtual int NumParamBlocks();
    virtual IParamBlock2* GetParamBlock( int i );
    virtual IParamBlock2* GetParamBlockByID( short id );

    virtual void BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev = NULL );
    virtual void EndEditParams( IObjParam* ip, ULONG flags, Animatable* next = NULL );

    virtual Class_ID ClassID();
#if MAX_VERSION_MAJOR >= 24
    virtual void GetClassName( MSTR& s, bool localized );
#else
    virtual void GetClassName( MSTR& s );
#endif

    // From ReferenceMaker
    virtual int NumRefs();
    virtual RefTargetHandle GetReference( int i );
    virtual void SetReference( int i, RefTargetHandle rtarg );

#if MAX_VERSION_MAJOR >= 17
    virtual RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message, BOOL propagate );
#else
    virtual RefResult NotifyRefChanged( Interval changeInt, RefTargetHandle hTarget, PartID& partID,
                                        RefMessage message );
#endif

    // From ReferenceTarget
    virtual RefTargetHandle Clone( RemapDir& remap );

    // From BaseObject
#if MAX_VERSION_MAJOR >= 24
    virtual const MCHAR* GetObjectName( bool localized );
#elif MAX_VERSION_MAJOR >= 15
    virtual const MCHAR* GetObjectName();
#else
    virtual MCHAR* GetObjectName();
#endif
    virtual CreateMouseCallBack* GetCreateMouseCallBack();
    int NumInterfaces();
    FPInterface* GetInterface( int i );
    virtual FPInterface* GetInterface( Interface_ID id );

    // From Modifier
    virtual Interval LocalValidity( TimeValue t );
    virtual ChannelMask ChannelsUsed();
    virtual ChannelMask ChannelsChanged();
    virtual Class_ID InputType();

    virtual void ModifyObject( TimeValue t, ModContext& mc, ObjectState* os, INode* node );
};
