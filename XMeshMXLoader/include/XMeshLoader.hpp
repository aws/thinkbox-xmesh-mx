// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include <frantic/files/filename_sequence.hpp>
#include <frantic/geometry/polymesh3.hpp>
#include <frantic/geometry/xmesh_metadata.hpp>

#include <frantic/max3d/fpwrapper/mixin_wrapper.hpp>
#include <frantic/max3d/maxscript/scripted_object_ref.hpp>
#include <frantic/max3d/maxscript/scripted_object_ref_impl.hpp>
#include <frantic/max3d/parameter_extraction.hpp>

#include <xmesh/cached_polymesh3_loader.hpp>

#include "const_value_simulated_object_accessor.hpp"
#include "mesh_channel_assignment.hpp"
#include "simple_object_accessor.hpp"

#define XMeshLoader_CLASS_ID Class_ID( 0x5c6a3834, 0x3bb02060 )         // class id of the plugin
#define XMeshLoader_INTERFACE_ID Interface_ID( 0x481471ba, 0x5b1719c0 ) // class id of the plugin interface
#define XMeshLoader_CLASS_NAME "XMeshLoader"
#define XMeshLoader_DISPLAY_NAME "XMeshLoader"

ClassDesc* GetXMeshLoaderClassDesc();

enum {
    mesh_loader_param_block,
    //
    mesh_loader_param_block_count // keep last
};

class XMeshLoader : public GeomObject, public frantic::max3d::fpwrapper::FFMixinInterface<XMeshLoader> {
    typedef frantic::files::filename_sequence filename_sequence;

  public:
    IParamBlock2* pblock2;
    Mesh mesh;
    MNMesh mm;
    Interval ivalid;

    frantic::graphics::boundbox3f m_meshBoundingBox;

  protected:
    simple_object_accessor<bool> m_showIcon;
    simple_object_accessor<float> m_iconSize;
    simple_object_accessor<float> m_meshScale;

    simple_object_accessor<bool> m_keepMeshInMemory;

    simple_object_accessor<int> m_outputVelocityMapChannel;
    simple_object_accessor<bool> m_velToMapChannel;

    simple_object_accessor<frantic::tstring> m_renderPath;
    simple_object_accessor<frantic::tstring> m_proxyPath;
    simple_object_accessor<bool> m_autogenProxyPath;

    simple_object_accessor<bool> m_loadSingleFrame;
    simple_object_accessor<int> m_frameOffset;
    simple_object_accessor<bool> m_limitToRange;
    simple_object_accessor<int> m_startFrame;
    simple_object_accessor<int> m_endFrame;
    simple_object_accessor<bool> m_enablePlaybackGraph;
    simple_object_accessor<float> m_playbackGraphTime;
    simple_object_accessor<int> m_beforeRangeMode;
    simple_object_accessor<int> m_afterRangeMode;

    simple_object_accessor<int> m_loadMode;

    simple_object_accessor<bool> m_enableViewportMesh;
    simple_object_accessor<bool> m_enableRenderMesh;
    simple_object_accessor<int> m_renderSequenceID;
    simple_object_accessor<int> m_viewportSequenceID;
    const_value_simulated_object_accessor<bool> m_renderUsingViewportSettings;
    simple_object_accessor<int> m_displayMode;
    simple_object_accessor<float> m_displayPercent;

    simple_object_accessor<bool> m_useFileLengthUnit;
    simple_object_accessor<int> m_lengthUnit;

    simple_object_accessor<int> m_ignoreMissingViewOnRenderNode;

    static std::vector<int> m_beforeRangeBehaviorDisplayCodes;
    static std::map<int, frantic::tstring> m_beforeRangeBehaviorNames;

    static std::vector<int> m_afterRangeBehaviorDisplayCodes;
    static std::map<int, frantic::tstring> m_afterRangeBehaviorNames;

    static std::vector<int> m_loadModeDisplayCodes;
    static std::map<int, frantic::tstring> m_loadModeNames;

    static std::vector<int> m_renderSequenceIDDisplayCodes;
    static std::map<int, frantic::tstring> m_renderSequenceIDNames;

    static std::vector<int> m_viewportSequenceIDDisplayCodes;
    static std::map<int, frantic::tstring> m_viewportSequenceIDNames;

    static std::vector<int> m_displayModeDisplayCodes;
    static std::map<int, frantic::tstring> m_displayModeNames;

    static std::vector<int> m_lengthUnitDisplayCodes;
    static std::map<int, frantic::tstring> m_lengthUnitNames;

    class StaticInitializer {
      public:
        StaticInitializer();
    };
    static StaticInitializer m_staticInitializer;

    bool m_cachedPolymesh3IsTriangles;
    bool m_doneBuildNormals;

    // for offsetting with velocity
    frantic::geometry::const_polymesh3_ptr m_cachedPolymesh3;

    // for interpolating
    Interval m_cachedInterval;
    std::pair<frantic::geometry::const_polymesh3_ptr, frantic::geometry::const_polymesh3_ptr> m_cachedPolymesh3Interval;

    int m_cachedSequenceID;           // which sequence does the cache represent?
    bool m_cachedMeshInRenderingMode; // was the cached mesh constructed in render mode?
    TimeValue m_cachedTime;           // cached time
    int m_cachedLoadingMode;          // which loading mode was the cache constructed in?
    int m_cachedDisplayMode;          // which display mode was the cache constructed in?

    bool m_inRenderingMode; // are we rendering?
    TimeValue m_renderTime;

    std::vector<filename_sequence> m_fileSequences;    // file sequences
    std::vector<frantic::tstring> m_fileSequencePaths; // paths used to create the file sequences

    static boost::shared_ptr<Mesh> m_pIconMesh; // gizmo/bbox icon

    xmesh::cached_polymesh3_loader m_polymesh3Loader;

    xmesh_metadata m_metadata;

  public:
    static XMeshLoader* editObj;

    XMeshLoader();
    virtual ~XMeshLoader();

    // Animatable methods
    void DeleteThis();
    Class_ID ClassID();
#if MAX_VERSION_MAJOR >= 24
    void GetClassName( MSTR& s, bool localized );
#else
    void GetClassName( MSTR& s );
#endif
    int NumSubs();
    Animatable* SubAnim( int i );
#if MAX_VERSION_MAJOR >= 24
    MSTR SubAnimName( int i, bool localized );
#else
    MSTR SubAnimName( int i );
#endif
    int NumParamBlocks();
    IParamBlock2* GetParamBlock( int i );
    IParamBlock2* GetParamBlockByID( BlockID i );
    void BeginEditParams( IObjParam* ip, ULONG flags, Animatable* prev = NULL );
    void EndEditParams( IObjParam* ip, ULONG flags, Animatable* next = NULL );
    ReferenceTarget* Clone( RemapDir& remap );
    void FreeCaches();
    int RenderBegin( TimeValue t, ULONG flags );
    int RenderEnd( TimeValue t );
#if MAX_VERSION_MAJOR >= 12
    void EnumAuxFiles( AssetEnumCallback& nameEnum, DWORD flags );
#else
    void EnumAuxFiles( NameEnumCallback& nameEnum, DWORD flags );
#endif
    BaseInterface* GetInterface( Interface_ID id );

    // From ReferenceMaker
    int NumRefs();
    RefTargetHandle GetReference( int i );
    void SetReference( int i, RefTargetHandle rtarg );
    IOResult Load( ILoad* iload );
#if MAX_VERSION_MAJOR >= 17
    RefResult NotifyRefChanged( const Interval& changeInt, RefTargetHandle hTarget, PartID& partID, RefMessage message,
                                BOOL propagate );
#else
    RefResult NotifyRefChanged( Interval changeInt, RefTargetHandle hTarget, PartID& partID, RefMessage message );
#endif

    // Virtual methods From Object
    BOOL UseSelectionBrackets() { return TRUE; }

    // Virtual methods From BaseObject
    CreateMouseCallBack* GetCreateMouseCallBack();
#if MAX_VERSION_MAJOR >= 24
    const TCHAR* GetObjectName( bool localized ) { return _T( XMeshLoader_CLASS_NAME ); }
#elif MAX_VERSION_MAJOR >= 15
    const TCHAR* GetObjectName() { return _T( XMeshLoader_CLASS_NAME ); }
#else
    TCHAR* GetObjectName() { return _T( XMeshLoader_CLASS_NAME ); }
#endif
    BOOL HasViewDependentBoundingBox() { return TRUE; }

    int Display( TimeValue t, INode* inode, ViewExp* pView, int flags );
    int HitTest( TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt );

#if MAX_VERSION_MAJOR >= 17
    // IObjectDisplay2 entries
    unsigned long GetObjectDisplayRequirement() const;
#elif MAX_VERSION_MAJOR >= 14
    bool RequiresSupportForLegacyDisplayMode() const;
#endif

#if MAX_VERSION_MAJOR >= 17
    bool PrepareDisplay( const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext );
    bool UpdatePerNodeItems( const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
                             MaxSDK::Graphics::UpdateNodeContext& nodeContext,
                             MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer );
#elif MAX_VERSION_MAJOR >= 15
    bool UpdateDisplay( const MaxSDK::Graphics::MaxContext& maxContext,
                        const MaxSDK::Graphics::UpdateDisplayContext& displayContext );
#elif MAX_VERSION_MAJOR == 14
    bool UpdateDisplay( unsigned long renderItemCategories,
                        const MaxSDK::Graphics::MaterialRequiredStreams& materialRequiredStreams, TimeValue t );
#endif

    // from Object
    ObjectState Eval( TimeValue t );
    Interval ObjectValidity( TimeValue t );
    void InitNodeName( MSTR& s );
    int CanConvertToType( Class_ID obtype );
    Object* ConvertToType( TimeValue t, Class_ID obtype );
    BOOL PolygonCount( TimeValue t, int& numFaces, int& numVerts );

    // From GeomObject
    int IntersectRay( TimeValue t, Ray& ray, float& at, Point3& norm );
    void GetWorldBoundBox( TimeValue t, INode* inode, ViewExp* vpt, Box3& box );
    void GetLocalBoundBox( TimeValue t, INode* inode, ViewExp* vpt, Box3& box );
    void GetDeformBBox( TimeValue t, Box3& box, Matrix3* tm, BOOL useSel );
    Mesh* GetRenderMesh( TimeValue t, INode* inode, View& view, BOOL& needDelete );

    // To support the Frantic Function Publishing interface
    void InitializeFPDescriptor();

    void SetRenderTime( TimeValue t );
    void ClearRenderTime();
    TimeValue GetRenderTime() const;
    bool HasValidRenderTime() const;

    void SetEmptyValidityAndNotifyDependents();

    void on_param_set( PB2Value& v, ParamID id, int /*tabIndex*/, TimeValue t );

    static const std::vector<int>& get_before_range_behavior_codes();
    static const std::vector<int>& get_after_range_behavior_codes();
    static const std::vector<int>& get_load_mode_codes();
    static const std::vector<int>& get_render_sequence_id_codes();
    static const std::vector<int>& get_viewport_sequence_id_codes();
    static const std::vector<int>& get_display_mode_codes();
    static const std::vector<int>& get_length_unit_codes();

    static frantic::tstring get_before_range_behavior_name( int code );
    static frantic::tstring get_after_range_behavior_name( int code );
    static frantic::tstring get_load_mode_name( int code );
    static frantic::tstring get_render_sequence_id_name( int code );
    static frantic::tstring get_viewport_sequence_id_name( int code );
    static frantic::tstring get_display_mode_name( int code );
    static frantic::tstring get_length_unit_name( int code );

    int get_before_range_behavior();
    int get_after_range_behavior();
    int get_load_mode();
    int get_control_render_sequence_id();
    int get_control_viewport_sequence_id();
    int get_display_mode();
    int get_length_unit();

    void set_before_range_behavior( int );
    void set_after_range_behavior( int );
    void set_load_mode( int );
    void set_control_render_sequence_id( int );
    void set_control_viewport_sequence_id( int );
    void set_display_mode( int );
    void set_length_unit( int );

    frantic::tstring get_proxy_path();
    frantic::tstring get_render_path();

    void invalidate_sequences();
    void invalidate();

    bool is_autogen_proxy_path_enabled();

    void set_to_valid_frame_range( bool notify = false, bool setLoadSingleFrame = false );

    frantic::tstring get_auto_proxy_path();
    frantic::tstring get_auto_proxy_directory();
    void check_auto_proxy_path();

    void report_warning( const frantic::tstring& msg );
    void report_error( const frantic::tstring& msg );

    frantic::tstring get_loading_frame_info( TimeValue t );

    void PostLoadCallback( ILoad* iload );

  private:
    //-----------------------------------------
    // XMeshLoader specific functions
    //-----------------------------------------

    void clear_cache();
    void clear_trimesh3_cache();
    void clear_cache_mxs();

    void set_to_valid_frame_range_mxs();

    void get_timing_data_without_limit_to_range( TimeValue time, TimeValue timeStep, TimeValue& outTime,
                                                 float& outTimeDerivative );
    void get_timing_data( TimeValue time, TimeValue timeStep, TimeValue& outTime, float& outTimeDerivative,
                          bool& outIsEmptyMeshTime );

    TimeValue round_to_nearest_wholeframe( TimeValue t ) const;

    // these allow you to grab the nearest subframe/wholeframe from a sequence
    // they return false when an appropriate frame can't be found
    bool get_nearest_subframe( TimeValue time, double& frameNumber );
    bool get_nearest_wholeframe( TimeValue time, double& frameNumber );

    // cached mesh info utility functions
    void invalidate_cache();

    // loads an interpolated mesh into the locally cached trimesh3 based upon the given interval
    // and fractional alpha distance from the start
    void load_mesh_interpolated( std::pair<double, double> interval, float alpha, int loadMask );

    // loads a mesh into the locally cached trimesh3
    void load_mesh_at_frame( double frame, int loadMask );

    // this just wraps the NotifyDependents call
    void NotifyEveryone();

    // our methods
    void UpdateMesh( TimeValue t );
    void build_channel_assignment( mesh_channel_assignment& channels, bool useVelocity, float timeOffset,
                                   float timeDerivative, const frantic::graphics::transform4f& xform );
    void polymesh_copy( bool useVelocity = false, float timeOffset = 0, float timeDerivative = 1.f,
                        const frantic::graphics::transform4f& xform = frantic::graphics::transform4f() );

    void BuildMesh( TimeValue t );

    int get_current_sequence_id();
    int get_render_sequence_id();
    frantic::tstring get_sequence_path( int seqId );
    frantic::files::filename_sequence& get_sequence( int seqId, bool throwIfMissing = true );
    frantic::tstring get_current_sequence_path();
    frantic::tstring get_render_sequence_path();
    frantic::files::filename_sequence& get_current_sequence( bool throwIfMissing = true );
    frantic::files::filename_sequence& get_render_sequence();

    int get_current_display_mode();

    bool has_triangle_mesh();

    void build_normals();

    static double get_system_scale_checked( int type );
    static double get_to_max_scale_factor( double scale, frantic::geometry::xmesh_metadata::length_unit_t lengthUnit );
    double calculate_mesh_scale_factor();

    void display_loading_frame_info();

    frantic::tstring get_node_name();
};
