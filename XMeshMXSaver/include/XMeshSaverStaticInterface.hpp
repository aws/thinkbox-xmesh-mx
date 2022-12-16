// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include <frantic/files/filename_sequence.hpp>
#include <frantic/geometry/xmesh_metadata.hpp>
#include <frantic/geometry/xmesh_sequence_saver.hpp>
#include <frantic/win32/log_window.hpp>

#include <frantic/max3d/fnpublish/StaticInterface.hpp>
#include <frantic/max3d/geometry/mesh_request.hpp>
#include <frantic/max3d/windows.hpp>

// for calling the proper message box handler when there's an error.
class max_message_box_handler {
  public:
    int operator()( const frantic::tstring msg, const frantic::tstring title, unsigned int type ) {
        return frantic::max3d::MsgBox( msg, title, type );
    }
};

class MeshSaverStaticInterface : public frantic::max3d::fnpublish::StaticInterface<MeshSaverStaticInterface> {

    // return true if m_sequenceName is set (non-empty)
    bool has_sequence() const;
    frantic::files::filename_sequence m_sequenceName; // required to manage file sequences
    frantic::geometry::xmesh_sequence_saver m_xss;    // required for saving xmesh sequences

    frantic::geometry::xmesh_metadata m_metadata;

    float m_timeStepScale;
    float m_timeStepInitialOffset; // In frames

    std::map<INode*, std::map<int, int>> m_materialIDMapping;
    int m_defaultMaterialID;

    int m_compressionLevel;
    std::size_t m_threadCount;

    frantic::win32::log_window m_logWindow;
    bool m_logPopupError;

    // channels that should be acquired from the 3ds Max scene's meshes
    std::vector<frantic::tstring> m_sourceChannelNames;

    // flip Y and Z axes, putting it into Y-up as needed by most other 3d packages
    bool m_objFlipYZ;

    void report_error( const frantic::tstring& msg );

    void apply_material_id_mapping( INode* node, frantic::geometry::trimesh3& mesh );
    void apply_material_id_mapping( INode* node, frantic::geometry::polymesh3_ptr mesh );

    void build_channel_propagation_policy( frantic::channels::channel_propagation_policy& cpp, bool addVelocity );

    void get_specialized_node_trimesh3( INode* meshNode, TimeValue startTime, TimeValue endTime,
                                        frantic::geometry::trimesh3& outMesh,
                                        frantic::max3d::max_interval_t& outValidityInterval, float timeStepScale,
                                        bool ignoreEmptyMeshes, bool ignoreTopologyWarnings, bool useObjectSpace,
                                        const frantic::channels::channel_propagation_policy& cpp );

    void save_meshes_to_sequence( std::vector<INode*> nodes, bool ignoreEmptyMeshes, bool ignoreTopologyWarnings,
                                  bool useObjectSpace, bool findVelocity, TimeValue t );

    int get_zlib_compression_level();

    frantic::graphics::transform4f get_to_obj_transform();

    void save_polymesh_sequence( std::vector<INode*> nodes, bool ignoreEmptyMeshes, bool ignoreTopologyWarnings,
                                 bool useObjectSpace, bool findVelocity, TimeValue t );

  public:
    /**
     * Register the interface and publish functions
     */
    MeshSaverStaticInterface();
    void SetTimeStepScale( float value );

    float GetTimeStepScale();

    void SetTimeStepInitialOffset( float value );

    float GetTimeStepInitialOffset();

    /**
     * For grabbing the plugin script locations relative to the dll's
     *
     * @return the path to the plugin scripts
     */
    const MCHAR* GetMeshSaverHome();

    const MCHAR* GetSettingsDirectory();

    /**
     * Returns a formatted version string
     *
     * @return a string of the form "Version: x.x.x rev.xxxxx"
     **/
    const MCHAR* get_version();

    /**
     *  Return the version number as a string.
     *
     * @return a version number string of the form "major.minor.patch.svnRev"
     */
    const MCHAR* get_version_number_string();

    /**
     * @return attributions for third-party software used in the XMesh Saver,
     */
    MSTR GetAttributions();

    /**
     * Sets the name of the mesh sequence
     *
     * @param filename the name of the cache
     **/
    void SetSequenceName( const MCHAR* filename );

    /**
     * Saves the mesh at time t to the xmesh sequence being maintained by the plugin static interface
     *
     * @param inode						the node the save
     * @param ignoreEmptyMeshes			ignore any empty meshes and proceed without throwing an error
     * @param ignoreTopologyWarnings	ignore any topology warnings and save out meshes without velocities when topology
     * prevents this
     * @param useObjectSpace			save the node mesh in object space, rather than world space
     * @param t							the time to save at
     */
    void SaveMeshToSequence( INode* inode, bool ignoreEmptyMeshes, bool ignoreTopologyWarnings, bool useObjectSpace,
                             bool findVelocity, frantic::max3d::fnpublish::TimeWrapper t );

    /**
     * Save a collection of nodes as a single frame at time t
     *
     * @param nodes						the nodes to save
     * @param ignoreEmptyMeshes			ignore any empty meshes and proceed without throwing an error
     * @param ignoreTopologyWarnings	ignore any topology warnings and save out meshes without velocities when topology
     * prevents this
     * @param t							the time to save at
     */
    void SaveMeshesToSequence( const Tab<INode*>& nodes, bool ignoreEmptyMeshes, bool ignoreTopologyWarnings,
                               bool findVelocity, frantic::max3d::fnpublish::TimeWrapper t );

    /**
     * Saves the polymesh at time t to the xmesh sequence being maintained by the plugin static interface
     *
     * @param inode						the node the save
     * @param ignoreEmptyMeshes			ignore any empty meshes and proceed without throwing an error
     * @param ignoreTopologyWarnings	ignore any topology warnings and save out meshes without velocities when topology
     * prevents this
     * @param useObjectSpace			save the node mesh in object space, rather than world space
     * @param t							the time to save at
     */
    void SavePolymeshToSequence( INode* inode, bool ignoreEmptyMeshes, bool ignoreTopologyWarnings, bool useObjectSpace,
                                 bool findVelocity, frantic::max3d::fnpublish::TimeWrapper t );

    /**
     * Save a collection of polymeshes as a single frame at time t
     *
     * @param nodes						the nodes to save
     * @param ignoreEmptyMeshes			ignore any empty meshes and proceed without throwing an error
     * @param ignoreTopologyWarnings	ignore any topology warnings and save out meshes without velocities when topology
     * prevents this
     * @param t							the time to save at
     */
    void SavePolymeshesToSequence( const Tab<INode*>& nodes, bool ignoreEmptyMeshes, bool ignoreTopologyWarnings,
                                   bool findVelocity, frantic::max3d::fnpublish::TimeWrapper t );

    /**
     *
     * Save some number of meshes from a node in the given interval.
     *
     * @param node			the node to save the meshes from
     * @param interval		the interval to save in
     * @param numSamples	the number of samples to attempt to save out over the interval.  if
                                                    topology is taken into account, it might not be possible to find
                                                    enough consistent meshes for the required sampling.  samples are
                                                    attempted to be drawn equal distances apart.
     * @param numRetries	the number of tries to save out each sample.  this determines how hard
                                                    the sampler will work to find a consistent mesh.  the more retries,
                                                    the longer the caching will take on meshes whose topology changes
                                                    often, and the further the resulting samples will be from equal
                                                    spacing.
     * @param option		a vector of strings describing options. for example:
     *		ignoreEmptyMeshes		ignore any empty meshes and proceed without throwing an error
     *		ignoreTopologyWarnings	ignore any topology warnings and save out meshes without velocities when topology
     prevents this *		useObjectSpace			save the node mesh in object space, rather than world
     space *		saveVelocity			save out a velocity
     *
     */
    // void SaveMeshesInIntervalToSequence( INode* inode, Interval interval, int numSamples, int numRetries, const
    // std::vector<frantic::tstring>& options );

    /**
     * This function will save a single polygon mesh to a file. If the object supplied does not
     * produce a PolyObject at the top of its stack, then the object will be converted to a PolyObject.
     * If that conversion causes the number of vertices to change, then the user will be notified.
     * @param pNode Pointer to the scene node to extract the polymesh from
     * @param path The file path to save the mesh to
     * @param t The automatically populated current scene time
     */
    void SavePolymesh( INode* pNode, const MCHAR* path, bool vertsOnly, bool worldSpace,
                       frantic::max3d::fnpublish::TimeWrapper t );

    void SetSceneRenderBegin( void );

    void SetSceneRenderEnd( void );

    void ClearAllMaterialIDMapping();
    void SetMaterialIDMapping( INode* node, const Tab<int>& from1, const Tab<int>& to1 );
    int GetDefaultMaterialID();
    void SetDefaultMaterialID( int matID );

    void SetCompressionLevel( int compressionLevel );
    int GetCompressionLevel();

    void SetThreadCount( int threadCount );
    int GetThreadCount();

    void InitializeLogging();
    void SetLoggingLevel( int loggingLevel );
    int GetLoggingLevel();
    bool GetPopupLogWindowOnError();
    void SetPopupLogWindowOnError( bool popupError );
    void FocusLogWindow();
    bool GetLogWindowVisible();
    void SetLogWindowVisible( bool visible );
    void LogMessageInternal( const frantic::tstring& msg );
    void LogError( const MCHAR* msg );
    void LogWarning( const MCHAR* msg );
    void LogProgress( const MCHAR* msg );
    void LogStats( const MCHAR* msg );
    void LogDebug( const MCHAR* msg );

    const MCHAR* ReplaceSequenceNumber( const MCHAR* file, float frame );

    void SetUserData( const MCHAR* key, const MCHAR* value );
    void DeleteUserData( const MCHAR* key );
    void ClearUserData();
    FPValue GetUserDataArray();
    // FPValue GetUserData( const std::string& key );

    // Removing these for now because I was missing something:
    // in some cases we need to transform the channels at saving time
    // (object space to world space) and sometimes we don't (already
    // in object space).  I think we need some way to specify whether
    // the channel should be tranformed, because in some cases, such as
    // xmesh loaders, channels may already be transformed into world space.
    // void SetChannelTransformType( const std::string & name, const std::string & transformType );
    // void ClearChannelTransformType();
    // FPValue GetChannelTransformTypeArray();
    // void ResetChannelTransformType();
    // FPValue GetChannelTransformType( const std::string& name );
    // FPValue GetChannelClassKeys();

    void LoadMetadata( const MCHAR* filename );
    void SaveMetadata( const MCHAR* filename );

    Tab<const MCHAR*> GetSourceChannelNames();
    void SetSourceChannelNames( Tab<const MCHAR*> channelNames );

    void SetObjFlipYZ( const bool );
    bool GetObjFlipYZ();

    // int GetMissingSourceChannelBehavior();
    // void SetMissingSourceChannelBehavior( int );
};

MeshSaverStaticInterface& GetMeshSaverStaticInterface();
void InitializeMeshSaverLogging();
