-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
plugin Geometry LegacyXMeshLoader
name:     "LegacyXMesh"
category: "Thinkbox"
classid:  #(0x3fcc7ef4, 0x58e55fc)
extends:  LegacyXMeshLoaderBase
replaceui:true
invisible:true
version:  2
remap:    #( #("cacheName", "proxyName"), #("renderSequenceName", "proxySequenceName") )
(
	local meshFilesRollout
	local loadModeNames=#("Frame Velocity Offset", "Subframe Velocity Offset", "Frame Interpolation", "Subframe Interpolation")
	local sequenceNames=#("Render Sequence", "Proxy Sequence")
	
	fn Invalidate =
	(
		delegate.Invalidate()
	)	

	parameters mainParam rollout:mainRollout
	(
		showIcon  type:#boolean default:true animatable:false ui:chk_showIcon
		iconSize  type:#float   default:10   animatable:false ui:spn_iconSize
		meshScale type:#float   default:1    animatable:false ui:spn_meshScale
		
		keepMeshInMemory type:#boolean default:true animatable:false ui:chk_keepMeshInMemory 
        
        velocitiesToMapChannel type:#boolean default:false ui:ckVelocitiesToMapChannel animatable:false
		on velocitiesToMapChannel set val do invalidate()
		outputVelocityMapChannel type:#integer default:2 ui:spn_velocityMapChannel  animatable:false
		on velocitiesToMapChannel set val do if velocitiesToMapChannel then invalidate()
	)
    
	parameters meshFilesParam rollout:meshFilesRollout
	(
	
--FILE LIST--

		renderSequenceName type:#string default:"" ui:txt_renderSequenceName animatable:false
		proxySequenceName type:#string default:"" ui:txt_proxySequenceName animatable:false

		autogenProxyPath type:#boolean default:false ui:chk_autogenProxyPath animatable:false

		enablePlaybackGraph type:#boolean default:false ui:chk_enablePlaybackGraph animatable:false
		on enablePlaybackGraph set val do invalidate()
		playbackGraphTime type:#float default:0 ui:spn_playbackGraphTime animatable:true --ANIMATABLE!
		on playbackGraphTime set val do invalidate()
		
		
--TIMING--		

		-- flag for whether to load just a single frame
		loadSingleFrame type:#boolean default:false ui:chk_loadSingleFrame animatable:false
		frameOffset type:#integer default:0 ui:spn_frameOffset animatable:false
		
		-- defines an interval over which to load/display mesh frame information
		limitToRange type:#boolean default:false ui:chk_limitToRange animatable:false
		startFrame type:#integer default:0 ui:spn_startFrame animatable:false
		endFrame type:#integer default:100 ui:spn_endFrame animatable:false

		-- specify how to handle out-of-range frames when limitToRange is set
		beforeRangeBehavior type:#integer default:1 ui:ddl_beforeRangeBehavior animatable:false
		afterRangeBehavior type:#integer default:1 ui:ddl_afterRangeBehavior animatable:false

--LOADING--
		loadMode type:#string default:"Frame Velocity Offset"
		loadModeIndex type:#integer default:1 ui:dd_loadMode animatable:false
		
		on loadMode set val do ( 
			-- check for valid string
			if classof(val) != String then throw "Invalid String"
					
			-- convert val to the correct index
			i = 1;
			while val != loadModeNames[i] and i <= loadModeNames.count do i = i + 1
						
			if i > loadModeNames.count then (
				msg = "Unsupported load mode: '" + val + "'."
				throw msg
			)
			else loadModeIndex = i
		)

		on loadSingleFrame set val do invalidate()
		on limitToRange set val do invalidate()
		--on startFrame set val do invalidate()
		--on endFrame set val do invalidate()
		on frameOffset set val do invalidate()
		on beforeRangeBehavior set val do invalidate()
		on afterRangeBehavior set val do invalidate()
	)
	
	parameters loadingParam rollout:loadingRollout
	(
		disableLoading      type:#boolean default:false animatable:false ui:chk_disableLoading
		useViewportSettings type:#boolean default:false animatable:false ui:chk_useViewportSettings
		
		renderSequenceID   type:#string default:"Render Sequence" animatable:false
		viewportSequenceID type:#string default:"Proxy Sequence"  animatable:false
		
		renderSequenceIndex   type:#integer ui:dd_renderQuality   default:1 animatable:false
		viewportSequenceIndex type:#integer ui:dd_viewportQuality default:2 animatable:false
	
		on renderSequenceID set val do ( 
			-- check for valid string
			if classof(val) != String then throw "Invalid String"
					
			-- convert val to the correct index
			i = 1;
			while val != sequenceNames[i] and i <= sequenceNames.count do i = i + 1
						
			if i > sequenceNames.count then (
				msg = "Unsupported display mode: '" + val + "'."
				throw msg
			)
			else renderSequenceIndex = i
		)

		on viewportSequenceID set val do ( 
			-- check for valid string
			if classof(val) != String then throw "Invalid String"
					
			-- convert val to the correct index
			i = 1;
			while val != sequenceNames[i] and i <= sequenceNames.count do i = i + 1
						
			if i > sequenceNames.count then (
				msg = "Unsupported display mode: '" + val + "'."
				throw msg
			)
			else viewportSequenceIndex = i
		)

	)
	
	rollout mainRollout "XMesh Loader"
	(
    	label lblMeshLoader "� 2011 Thinkbox Software" align:#center
		label lblVersion "" align:#center

		group "Options" (
			checkbox chk_showIcon "Icon" across:2
			spinner spn_iconSize "Size:" range:[0,10000000,10] fieldwidth:45 type:#worldunits align:#right offset:[4,1]
			spinner spn_meshScale "Mesh Scale:" range:[-100000000,100000000,1] align:#right fieldwidth:45 offset:[4,0] type:#float

			checkbox chk_keepMeshInMemory "Keep Mesh in Memory" default:true
            
            checkbox ckVelocitiesToMapChannel "Velocity To Map Channel" animatable:false
			spinner spn_velocityMapChannel "Map Channel:" range:[0,99,2] fieldwidth:45 offset:[4,-3] type:#integer
			
		)

		on spn_meshScale changed val do invalidate()

		on chk_keepMeshInMemory changed val do invalidate()

		on mainRollout open do (
			lblVersion.text = XMeshLoaderUtils.Version
		)
	)
	
	rollout meshFilesRollout "Mesh Files"
	(
		label lbl_seqName "Render Sequence:" align:#left offset:[-9,0]
		edittext txt_renderSequenceName align:#left width:142 across:2 offset:[-13,-3]
		button btn_setSequence "..." width:14 height:17 offset:[36,-3]
		
		checkbox chk_autogenProxyPath "Automatic Proxy Path" offset:[-9,0]
		
		label lbl_proxySequenceName "Proxy Sequence:" align:#left offset:[-9,0]
		edittext txt_proxySequenceName align:#left width:142 across:2 offset:[-13,-3]
		button btn_setProxy "..." width:14 height:17 offset:[36,-3]

		button btn_updateSourceSequences "Update Source Sequences" width:148 align:#center
		
		group "Timing"		
		(
			checkbox chk_loadSingleFrame "Load Single Frame Only" offset:[0,-3]

			checkbox chk_enablePlaybackGraph "Use Playback Graph" offset:[0,-3]
			spinner spn_playbackGraphTime "Playback Graph [a]" range:[-100000,100000,0] fieldwidth:42 offset:[4,-3] type:#integer enabled:enablePlaybackGraph
		
			spinner spn_frameOffset "Frame Offset:" range:[-100000,100000,0] fieldwidth:42 offset:[4,-3] type:#integer

			checkbox chk_limitToRange "Limit To Custom Range:" offset:[-2,-3]
			button btn_setRangeFromFrames "Range" width:40 height:18 align:#left offset:[-3,-4] across:3 tooltip:"Check availability of frames and set the Custom Range to the intersection of all good intervals." 
			spinner spn_startFrame "" range:[-10000000,1000000,0] fieldwidth:35 offset:[-8,-3] type:#integer across:2 align:#left 
			spinner spn_endFrame "-" range:[-10000000,10000000,0] fieldwidth:42 offset:[5,-3] type:#integer align:#right
			dropdownlist ddl_beforeRangeBehavior items:#("Hold First","Blank") across:2 width:72 offset:[-3,0]
			dropdownlist ddl_afterRangeBehavior items:#("Hold Last","Blank") width:72 offset:[3,0]
		)
		
		group "Loading Mode"
		(
			dropdownlist dd_loadMode items:loadModeNames animatable:false selection:1 align:#center width:145
		)
		
		fn UpdateGUI =
		(
			spn_startFrame.enabled = spn_endFrame.enabled = limitToRange
			chk_enablePlaybackGraph.enabled = spn_frameOffset.enabled = chk_limitToRange.enabled = not chk_loadSingleFrame.checked
			spn_playbackGraphTime.enabled = chk_enablePlaybackGraph.checked and not chk_loadSingleFrame.checked
			
			txt_proxySequenceName.enabled = btn_setProxy.enabled = lbl_proxySequenceName.enabled = not chk_autogenProxyPath.checked
		)
		
		fn UpdateSourceSequences =
		(
			if ( txt_renderSequenceName.text != "" ) do
				delegate.RenderSequence  = txt_renderSequenceName.text
			if ( txt_proxySequenceName.text != "" ) do
				delegate.ProxySequence  = txt_proxySequenceName.text
		)
		
		fn SetAutoProxyPath =
		(
			-- Automatic Proxy Path Generation:
			--
			-- This automagically generates the default proxy path from the path in the render
			-- sequence edit box., sets the proxy path in the delegate object, and turns on the
			-- proxy mode for loading.
					
			renderSequence = txt_renderSequenceName.text
				
			if ( filePattern == "" ) do (
				return 0
			)
			
			basePath = GetFilenamePath renderSequence
			baseType = GetFilenameType renderSequence
			baseNameWithNumber = GetFilenameFile renderSequence
			baseName = (TrimRight baseNameWithNumber "1234567890#.")
			numberString = ""
			for i = baseName.count+1 to baseNameWithNumber.count do
				numberString = numberString + baseNameWithNumber[i]
			
			dirs = GetDirectories (basePath + baseName + "_proxy")
					
			
			if (dirs.count == 0) do
			(
				messageBox ("Unable to locate proxy sequence at default path: " + basePath + baseName + "_proxy")
				viewportSequenceID = "Render Sequence"
				autogenProxyPath = false
				delegate.ProxySequence = txt_proxySequenceName.text = ""
				return 0
			)
			
			proxyDir = dirs[1]
			proxyPath = proxyDir + baseName + "_proxy" + numberString + baseType

			viewportSequenceID = "Proxy Sequence"
			delegate.ProxySequence = proxyPath
			txt_proxySequenceName.text = proxyPath
			
			spn_startFrame.value = delegate.startFrame
			spn_endFrame.value = delegate.endFrame
			chk_limitToRange.checked = true
				
		)
		
		on dd_loadMode selected val do (
			loadMode = loadModeNames[val]
		)
		
		on btn_setRangeFromFrames pressed do
		(
			spn_startFrame.value = delegate.startFrame
			spn_endFrame.value = delegate.endFrame
			
			chk_limitToRange.checked = true
			invalidate()
			UpdateGUI()
		)
		
		on chk_limitToRange changed state do UpdateGUI ()

		on btn_setSequence pressed do
		(
			local newSequenceName = getOpenFileName caption:"Select the Mesh Sequence to Add" filename:renderSequenceName types:"All Mesh Files|*.xmesh;*.obj|Thinkbox XMesh Files (*.xmesh)|*.xmesh|Wavefront OBJ Files (*.obj)|*.obj|All Files (*.*)|*.*"
			if newSequenceName != undefined do
			(
				delegate.RenderSequence  = newSequenceName
				txt_renderSequenceName.text = newSequenceName
				
				if autogenProxyPath then 
					SetAutoProxyPath()
				else
					viewportSequenceID = "Render Sequence"
				
				UpdateSourceSequences()
				spn_startFrame.value = delegate.startFrame
				spn_endFrame.value = delegate.endFrame
				chk_limitToRange.checked = true
				UpdateGUI()
			)
			
			Invalidate()
		)
		
		on btn_setProxy pressed do
		(
			local newSequenceName = getOpenFileName caption:"Select the Mesh Sequence to Add" filename:renderSequenceName types:"All Mesh Files|*.xmesh;*.obj|Thinkbox XMesh Files (*.xmesh)|*.xmesh|Wavefront OBJ Files (*.obj)|*.obj|All Files (*.*)|*.*"
			if newSequenceName != undefined do 
			(
				txt_proxySequenceName.text = newSequenceName
				delegate.ProxySequence = newSequenceName

				UpdateSourceSequences()
				spn_startFrame.value = delegate.startFrame
				spn_endFrame.value = delegate.endFrame
				chk_limitToRange.checked = true
				UpdateGUI()
			)
			
			Invalidate()
		)
		
		on chk_enablePlaybackGraph changed state do 
		(
			spn_playbackGraphTime.enabled = state
		)
		
		on chk_loadSingleFrame changed state do 
		(
			chk_limitToRange.enabled = not state
			spn_startFrame.enabled = spn_endFrame.enabled = chk_limitToRange.checked AND not state
			chk_enablePlaybackGraph.enabled = spn_frameOffset.enabled  = not state
			spn_playbackGraphTime.enabled = enablePlaybackGraph AND not state
		)
		
		on spn_playbackGraphTime changed val do invalidate()
		
		on txt_renderSequenceName changed val do
		(
			delegate.RenderSequence = val
			if ( val == "" and txt_proxySequenceName.text != "" ) do
				renderSequenceID = "Proxy Sequence"
		)
		
		on txt_proxySequenceName changed val do
		(
			delegate.ProxySequence = val
			if ( val == "" and txt_renderSequenceName.text != "" ) do
				viewportSequenceID = "Render Sequence"
		)
		
		on chk_autogenProxyPath changed val do
		(
			if (chk_autogenProxyPath.checked == true and txt_renderSequenceName.text != "") then SetAutoProxyPath()
			Invalidate()		
			UpdateGUI()
		)
		
		on meshFilesRollout open do
		(
			UpdateGUI()
		)	
		
		on btn_updateSourceSequences pressed do
		(
			UpdateSourceSequences()
		)
		
	)--end rollout

	rollout loadingRollout "Loading"
	(
		checkbox chk_disableLoading "Disable in Viewport" default:false
		
		group "Render Quality"
		(
			checkbox chk_useViewportSettings "Use Viewport Settings" default:false
			dropdownlist dd_renderQuality items:sequenceNames selection:1 align:#center width:145
		)
		
		group "Viewport Quality"
		(
			dropdownlist dd_viewportQuality items:sequenceNames selection:2 align:#center width:145
		)
		
		fn UpdateGUI = 
		(
			dd_renderQuality.enabled = not chk_useViewportSettings.checked
		)
		
		on chk_disableLoading changed val do 
		(
			invalidate()
		)
		
		on dd_renderQuality selected val do
		(
			renderSequenceID = sequenceNames[val]
			invalidate()
		)
		
		on dd_viewportQuality selected val do
		(
			viewportSequenceID = sequenceNames[val]
			invalidate()
		)
		
		on chk_useViewportSettings changed val do
		(
			UpdateGUI()
			invalidate()
		)
	)
	
	on create do       ( delegate.SetScriptedOwner this )

	on clone oldObj do ( delegate.SetScriptedOwner this )

	on Load do ( 
		
		delegate.SetScriptedOwner this

		if ( renderSequenceName != "" ) do delegate.RenderSequence = renderSequenceName
		if ( proxySequenceName != "" ) do	delegate.ProxySequence  = proxySequenceName
	)
	
	tool create 
	( 
		on mousePoint click do
		case click of
		(
			1: nodeTM.translation = gridPoint
			2: #stop
		)
		
		on mouseMove click do
		case click of
		(
			2: (iconSize = (length gridDist)*0.7)
		)
	) 	
)
