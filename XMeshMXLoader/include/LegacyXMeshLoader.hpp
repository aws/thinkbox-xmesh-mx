// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include <frantic/files/filename_sequence.hpp>
#include <frantic/max3d/fpwrapper/mixin_wrapper.hpp>
#include <frantic/max3d/maxscript/scripted_object_ref.hpp>
#include <frantic/max3d/maxscript/scripted_object_ref_impl.hpp>
#include <frantic/max3d/parameter_extraction.hpp>

#define LegacyXMeshLoader_CLASS_ID Class_ID( 0x9472661, 0x22c326be ) // class id of the plugin
#define LegacyXMeshLoader_SCRIPTED_CLASS_ID                                                                            \
    Class_ID( 0x3fcc7ef4, 0x58e55fc ) // class id of the scripted plugin gui extension
#define LegacyXMeshLoader_INTERFACE_ID Interface_ID( 0x4744ed5, 0x38fb1cdc ) // class id of the plugin interface
#define LegacyXMeshLoader_CLASS_NAME "LegacyXMeshLoaderBase"

ClassDesc* GetLegacyXMeshLoaderClassDesc();

class LegacyXMeshLoader;
inline LegacyXMeshLoader* GetLegacyXMeshLoader( Object* obj ) {
    if( obj->ClassID() == LegacyXMeshLoader_SCRIPTED_CLASS_ID && obj->NumRefs() > 0 ) {
        ReferenceTarget* pDelegate = obj->GetReference( 0 );
        if( pDelegate->ClassID() == LegacyXMeshLoader_CLASS_ID )
            return reinterpret_cast<LegacyXMeshLoader*>( pDelegate );
    }

    return NULL;
}

class LegacyXMeshLoader : public SimpleObject2, public frantic::max3d::fpwrapper::FFMixinInterface<LegacyXMeshLoader> {
    typedef frantic::files::filename_sequence filename_sequence;

    // scripted plugin access
    frantic::max3d::maxscript::scripted_object_ref m_parent;

    frantic::max3d::maxscript::scripted_object_accessor<int> m_startFrame;
    frantic::max3d::maxscript::scripted_object_accessor<int> m_frameOffset;
    frantic::max3d::maxscript::scripted_object_accessor<int> m_endFrame;
    frantic::max3d::maxscript::scripted_object_accessor<int> m_beforeRangeMode;
    frantic::max3d::maxscript::scripted_object_accessor<int> m_afterRangeMode;
    frantic::max3d::maxscript::scripted_object_accessor<bool> m_showIcon;
    frantic::max3d::maxscript::scripted_object_accessor<bool> m_limitToRange;
    frantic::max3d::maxscript::scripted_object_accessor<bool> m_disableLoading;
    frantic::max3d::maxscript::scripted_object_accessor<bool> m_loadSingleFrame;
    frantic::max3d::maxscript::scripted_object_accessor<bool> m_autogenProxyPath;
    frantic::max3d::maxscript::scripted_object_accessor<bool> m_keepMeshInMemory;
    frantic::max3d::maxscript::scripted_object_accessor<bool> m_enablePlaybackGraph;
    frantic::max3d::maxscript::scripted_object_accessor<bool> m_useViewportSettings;
    frantic::max3d::maxscript::scripted_object_accessor<float> m_iconSize;
    frantic::max3d::maxscript::scripted_object_accessor<float> m_meshScale;
    frantic::max3d::maxscript::scripted_object_accessor<float> m_playbackGraphTime;
    frantic::max3d::maxscript::scripted_object_accessor<frantic::tstring> m_loadMode;
    frantic::max3d::maxscript::scripted_object_accessor<frantic::tstring> m_proxyPath;
    frantic::max3d::maxscript::scripted_object_accessor<frantic::tstring> m_renderPath;
    frantic::max3d::maxscript::scripted_object_accessor<frantic::tstring> m_renderSequenceID;
    frantic::max3d::maxscript::scripted_object_accessor<frantic::tstring> m_viewportSequenceID;
    frantic::max3d::maxscript::scripted_object_accessor<bool> m_velToMapChannel;
    frantic::max3d::maxscript::scripted_object_accessor<int> m_outputVelocityMapChannel;

    // for offsetting with velocity
    frantic::geometry::trimesh3 m_cachedTrimesh3; // a current copy of the trimesh being viewed

    // for interpolating
    Interval m_cachedInterval;
    std::pair<frantic::geometry::trimesh3, frantic::geometry::trimesh3> m_cachedTrimesh3Interval;

    int m_cachedSequenceID;                     // which sequence does the cache represent?
    bool m_cachedMeshInRenderingMode;           // was the cached mesh constructed in render mode?
    TimeValue m_cachedFrameTicks, m_cachedTime; // cached frame in ticks, cached time (not the same)
    frantic::tstring m_cachedLoadingMode;       // which loading mode was the cache constructed in?

    bool m_inRenderingMode; // are we rendering?
    TimeValue m_renderTime;

    std::vector<filename_sequence> m_fileSequences; // file sequences

    enum { RENDER_SEQ, PROXY_SEQ, SEQ_COUNT }; // sequence name symbolic constants

    static boost::shared_ptr<Mesh> m_pIconMesh; // gizmo/bbox icon

  public:
    LegacyXMeshLoader();
    virtual ~LegacyXMeshLoader();

    // Virtual methods From Object
    Interval ObjectValidity( TimeValue time ) { return Interval( time, time ); }
    ObjectState Eval( TimeValue time ) {
        BuildMesh( time );
        return ObjectState( this );
    }
    BOOL UseSelectionBrackets() { return FALSE; }

    // Virtual methods From BaseObject
    CreateMouseCallBack* GetCreateMouseCallBack() { return NULL; }
#if MAX_VERSION_MAJOR >= 24
    const TCHAR* GetObjectName( bool localized ) { return _T( LegacyXMeshLoader_CLASS_NAME ); }
#elif MAX_VERSION_MAJOR >= 15
    const TCHAR* GetObjectName() { return _T( LegacyXMeshLoader_CLASS_NAME ); }
#else
    TCHAR* GetObjectName() { return _T( LegacyXMeshLoader_CLASS_NAME ); }
#endif
    BOOL HasViewDependentBoundingBox() { return TRUE; }

    void GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* vpt, Box3& box );
    void GetLocalBoundBox( TimeValue t, INode* inode, ViewExp* vpt, Box3& box );
    int Display( TimeValue t, INode* inode, ViewExp* pView, int flags );
    int HitTest( TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt );

#if MAX_VERSION_MAJOR >= 17
    // IObjectDisplay2 entries
    unsigned long GetObjectDisplayRequirement() const;
#elif MAX_VERSION_MAJOR >= 14
    bool RequiresSupportForLegacyDisplayMode() const;
#endif

#if MAX_VERSION_MAJOR >= 15 && MAX_VERSION_MAJOR < 17
    bool UpdateDisplay( const MaxSDK::Graphics::MaxContext& maxContext,
                        const MaxSDK::Graphics::UpdateDisplayContext& displayContext );
#elif MAX_VERSION_MAJOR == 14
    bool UpdateDisplay( unsigned long renderItemCategories,
                        const MaxSDK::Graphics::MaterialRequiredStreams& materialRequiredStreams, TimeValue t );
#endif

    // Virtual methods From ReferenceTarget
    RefTargetHandle Clone( RemapDir& remap );

    // Virtual methods From Animatable
    void DeleteThis() { delete this; }
    Class_ID ClassID() { return LegacyXMeshLoader_CLASS_ID; }
    int RenderBegin( TimeValue t, ULONG flags );
    int RenderEnd( TimeValue t );

    int NumInterfaces();
    FPInterface* GetInterface( int i );
    FPInterface* GetInterface( Interface_ID id );

    // To support the Frantic Function Publishing interface
    void InitializeFPDescriptor();

    // Required for new scripted plugin format
    void set_scripted_owner( ReferenceTarget* scriptedOwner );

    std::vector<INode*> ret_array();

    void SetRenderTime( TimeValue t );
    void ClearRenderTime();
    TimeValue GetRenderTime() const;
    bool HasValidRenderTime() const;

    void SetEmptyValidityAndNotifyDependents();

  private:
    //-----------------------------------------
    // LegacyXMeshLoader specific functions
    //-----------------------------------------

    static void AcquireLicense();
    static void ReleaseLicense();

    void invalidate();
    void clear_cache();

    void scale_cached_velocity_channel( float timeDerivative );

    void get_timing_data( TimeValue time, TimeValue timeStep, TimeValue& outTime, float& outTimeDerivative,
                          bool& outIsEmptyMeshTime );

    float get_start_frame();
    float get_end_frame();

    TimeValue round_to_nearest_wholeframe( TimeValue t ) const;

    // these allow you to grab the nearest subframe/wholeframe from a sequence
    // they return false when an appropriate frame can't be found
    bool get_nearest_subframe( TimeValue time, double& frameNumber );
    bool get_nearest_wholeframe( TimeValue time, double& frameNumber );

    // cached mesh info utility functions
    bool is_cache_valid( TimeValue time );
    void invalidate_cache();

    // check for a frame in the cache at time t
    bool frame_exists( TimeValue t );

    // loads an interpolated mesh into the locally cached trimesh3 based upon the given interval
    // and fractional alpha distance from the start
    void load_mesh_interpolated( std::pair<double, double> interval, float alpha );

    // loads a mesh into the locally cached trimesh3
    void load_mesh_at_frame( double frame );

    // this just wraps the NotifyDependents call
    void NotifyEveryone();

    // required virtual function
    void BuildMesh( TimeValue frameTime );

    // sequence getters
    frantic::tstring get_render_sequence();
    frantic::tstring get_proxy_sequence();

    // sequence setters
    void init_sequence( const frantic::tstring& filename, frantic::files::filename_sequence& outSequence );
    void set_render_sequence( const frantic::tstring& filename );
    void set_proxy_sequence( const frantic::tstring& filename );

    // current sequence wrappers
    int current_sequence_id();
    filename_sequence& current_sequence();
};
