-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
(
	local du = DeadlineUtil  --this is the interface exposed by the Lightning Plug-in which provides communication between Deadline and 3ds Max
	if du == undefined do  --if the script is not being run on Deadline (for testing purposes),
	(
		struct DeadlineUtilStruct   --define a stand-in struct with the same methods as the Lightning plug-in
		(
			fn SetTitle title = ( format "Title: %\n" title ),
			fn SetProgress percent = (true),
			fn FailRender msg = ( throw msg ),
			fn GetJobInfoEntry key = ( undefined ),
			fn GetAuxFilename index = ( undefined ),
			fn LogMessage msg = ( format "%\n" msg ),
			CurrentFrame = ((sliderTime as string) as integer)
		)
		du = DeadlineUtilStruct() --create an instance of the stand-in struct
	)--end if

	fn validateObjectNameAsFileName theName =
	(
		local newName = ""
		for i = 1 to theName.count do
		(
			local theCode = bit.charAsInt theName[i]
			if i > 1 AND (theCode < 48 OR (theCode > 57 AND theCode < 65) OR (theCode > 90 AND theCode < 97) OR theCode > 122) then
				newName +="_"
			else
				newName +=theName[i]
		)
		newName
	)
	
	fn getFrameList startFrame endFrame FrameStep =
	(
		local currentTimeValue = (startFrame as time) as integer
		local currentTimeStep = (TicksPerFrame/frameStep) as integer
		local endFrameAsTicks = (endFrame as time) as integer
		local theFramesToSave = #()
		while currentTimeValue <= endFrameAsTicks do
		(
			append theFramesToSave (currentTimeValue as float/TicksPerFrame)
			currentTimeValue+= currentTimeStep
		)
		if findItem theFramesToSave endFrame == 0 do append theFramesToSave endFrame
		theFramesToSave
	)	

	fn getVisibility o =
	(
		local theVC = getVisController o
		if theVC == undefined then 
			if o.visibility == true then 1.0 else 0.0 
		else 
			theVC.value		
	)
	fn getInheritedVisibility o mode:2 =
	(
		local currentNode = o
		local currentVis = getVisibility currentNode
		local done= false
		while not done do
		(
			local theParent = currentNode.parent
			if theParent == undefined then
			(
				done = true
			)
			else
			(
				if currentNode.inheritVisibility then
				(
					currentVis *= getVisibility theParent
					currentNode = currentNode.parent
				)
				else
				(
					done = true
				)
			)
		)
			
		case mode of
		(
			default: currentVis > 0.0
			3: currentVis >= 1.0
		)
	)
	

	du.SetTitle "Thinkbox XMesh Saving Job" --set the job title
	du.LogMessage "Starting Thinkbox XMesh Saving Job..." --output a message to the log
	local st = timestamp() --get the current system time

	local filePath = du.GetJobInfoEntry "xmesh_FilePath"
	local saveMode = (du.GetJobInfoEntry "xmesh_SaveMode") as Integer
	local objectSourceMode = (du.GetJobInfoEntry "xmesh_ObjectSourceMode") as Integer
	local saveVelocity = (du.GetJobInfoEntry "xmesh_SaveVelocity") as BooleanClass
	local ignoreTopology = (du.GetJobInfoEntry "xmesh_IgnoreTopology") as BooleanClass
	local ignoreEmpty = (du.GetJobInfoEntry "xmesh_IgnoreEmpty") as BooleanClass
	local savePolymesh = (du.GetJobInfoEntry "xmesh_SavePolymesh") as BooleanClass
	local disableDataOptimization = (du.GetJobInfoEntry "xmesh_DisableDataOptimization") as BooleanClass
	local enableMaterialIDMapping = (du.GetJobInfoEntry "xmesh_EnableMaterialIDMapping") as BooleanClass
	local setSceneRender = (du.GetJobInfoEntry "xmesh_SetSceneRender") as BooleanClass
	local metadataFile = du.GetJobInfoEntry "xmesh_MetadataFile"
	local sourceChannelsString = du.GetJobInfoEntry "xmesh_SourceChannels"
	local coordinateSystemString = du.GetJobInfoEntry "xmesh_CoordinateSystem"
	local objectVisibilityMode = (du.GetJobInfoEntry "xmesh_ObjectVisibilityMode") as Integer

	local rangeFirstFrame = (du.GetJobInfoEntry "xmesh_StartFrame") as Integer
	local rangeEndFrame = (du.GetJobInfoEntry "xmesh_EndFrame") as Integer
	local samplingStep = (du.GetJobInfoEntry "xmesh_SamplingStep") as Float
	local rangeSegments = (du.GetJobInfoEntry "xmesh_RangeSegments") as Integer

	du.LogMessage "Passed Properties Reading"

	if metadataFile != undefined do
	(
		local metadataPath = du.JobsDataFolder + "\\" + metadataFile
		if (doesFileExist metadataPath) then
		(
			XMeshSaverUtils.LoadMetadata metadataPath
		)
		else
		(
			du.FailRender("Error: Could not load xmesh metadata from path: \"" + metadataPath + "\"")
		)
	)

	if sourceChannelsString != undefined do
	(
		local sourceChannels = filterString sourceChannelsString ","
		XMeshSaverUtils.sourceChannels = sourceChannels
	)
	
	local objFlipYZ = false
	if coordinateSystemString != undefined do
	(
		local coordinateSystem = coordinateSystemString as Name
		if coordinateSystem == #zuprh then
		(
			objFlipYZ = false
		)
		else if coordinateSystem == #yuprh then
		(
			objFlipYZ = true
		)
		else
		(
			du.FailRender("Error: Unknown Coordinate System: " + coordinateSystemString)
		)
	)
	XMeshSaverUtils.objFlipYZ = objFlipYZ

	local theRangeSegmentLength = (ceil (1.0*(rangeEndFrame-rangeFirstFrame+1))/rangeSegments) as integer
	local startFrame = rangeFirstFrame
	local theTimeRanges = #()
	for i = 1 to rangeSegments do
	(
		append theTimeRanges #(startFrame, startFrame+theRangeSegmentLength)
		startFrame = startFrame+theRangeSegmentLength
	)
	theTimeRanges[theTimeRanges.count][2] = rangeEndFrame

	du.LogMessage (theTimeRanges as string)

	if savePolymesh then
	(
		SaveMeshToSequence = XMeshSaverUtils.SavePolymeshToSequence
		SaveMeshesToSequence = XMeshSaverUtils.SavePolymeshesToSequence
	)
	else
	(
		SaveMeshToSequence = XMeshSaverUtils.SaveMeshToSequence
		SaveMeshesToSequence = XMeshSaverUtils.SaveMeshesToSequence
	)

	XMeshSaverUtils.AcquireLicense()
	try
	(
		if setSceneRender do
			XMeshSaverUtils.SetSceneRenderBegin()
		if saveMode == 1 or objectSourceMode == 6 then --saving all objects as one mesh
		(
			local meshNodeCount = (du.GetJobInfoEntry "xmesh_MeshNodeCount") as integer --get the number of nodes passed to the script to process
			if objectSourceMode == 6 then
				du.LogMessage ("Saving " + (meshNodeCount as string) + " PFlow Events...")
			else
				du.LogMessage ("Saving " + (meshNodeCount as string) + " Objects...")
			local meshNodes = #() --init an array to collect objects into
			for idx = 1 to meshNodeCount do
			(
				local ss = stringstream ""
				format "xmesh_MeshNodeHandle%" idx to:ss
				local theHandle = (du.GetJobInfoEntry (ss as string)) as integer
				local theNode = maxops.getNodeByHandle theHandle
				meshNodes[idx] = theNode
				if not (isValidNode theNode) then
					du.FailRender("Object with Node Handle [" + theHandle as string + "] not found. Aborting....")
			)
			du.LogMessage ("Saving " + meshNodes as string)
			if enableMaterialIDMapping do
				local theMaterial = XMeshSaver_MaterialUtils.getMaterialFromNodes meshNodes  --build Material info
			--du.LogMessage ("Material "+ theMaterial as string)

			makeDir (getFileNamePath filePath) all:true
			XMeshSaverUtils.SetSequenceName filePath --set the save path
			du.LogMessage ("Path: " + filePath )

			--du.LogMessage ("Saving "+ meshNodes.count as string +" Objects from Frame " + theTimeRanges[du.CurrentFrame][1] as string +" to Frame " + theTimeRanges[du.CurrentFrame][2] as string + " In World Space...")

			local theFrameList = getFrameList theTimeRanges[du.CurrentFrame][1] theTimeRanges[du.CurrentFrame][2] samplingStep
			for t = 1 to theFrameList.count do
			(
				du.setProgress (100.0 * t/theFrameList.count)
				du.LogMessage ("XMesh Saving Frame : " + theFrameList[t] as string )
				if disableDataOptimization == true do
				(
					XMeshSaverUtils.SetSequenceName "c:\\"
					XMeshSaverUtils.SetSequenceName filePath						
				)				
				at time theFrameList[t] 
				(
					local filteredMeshesToSave = case  ObjectVisibilityMode of
					(
						default: meshNodes				
						2: for o in meshNodes where getInheritedVisibility o mode:2 collect o
						3: for o in meshNodes where getInheritedVisibility o mode:3 collect o
					)
					SaveMeshesToSequence filteredMeshesToSave ignoreEmpty ignoreTopology saveVelocity
				)
			)
			progressEnd()
			XMeshSaverUtils.ClearAllMaterialIDMapping()
		)
		else
		(
			local emptyMesh = Editable_Mesh()
			local meshNodeHandle = du.GetJobInfoEntry "xmesh_MeshNodeHandle"
			local meshNode = maxops.getNodeByHandle (meshNodeHandle as integer)
			if isValidNode meshNode then
			(
				makeDir (getFileNamePath filePath) all:true
				XMeshSaverUtils.SetSequenceName filePath
				local theModeString = if saveMode == 3 then " In Object Space..." else " In World Space..."
				du.LogMessage ("Saving Individual Object ["+ meshNode.name +"] from Frame " + theTimeRanges[du.CurrentFrame][1] as string +" to Frame " + theTimeRanges[du.CurrentFrame][2] as string  + theModeString)

				if enableMaterialIDMapping do
					local theMaterial = XMeshSaver_MaterialUtils.getMaterialFromNodes #(meshNode)  --build Material info

				local theFrameList = getFrameList theTimeRanges[du.CurrentFrame][1] theTimeRanges[du.CurrentFrame][2] samplingStep
				for t = 1 to theFrameList.count do
				(
					du.setProgress (100.0 * t/theFrameList.count)
					du.LogMessage ("XMesh Saving Frame : " + theFrameList[t] as string )
					if disableDataOptimization == true do
					(
						XMeshSaverUtils.SetSequenceName "c:\\"
						XMeshSaverUtils.SetSequenceName filePath						
					)
					at time theFrameList[t] 
					(
						local filteredMeshToSave = case ObjectVisibilityMode of
						(
							default: meshNode
							2: if ( getInheritedVisibility meshNode mode:2 ) then meshNode else emptyMesh
							3: if ( getInheritedVisibility meshNode mode:3 ) then meshNode else emptyMesh							
						)						
						SaveMeshToSequence filteredMeshToSave ignoreEmpty ignoreTopology (saveMode==3) saveVelocity
					)
				)
			)
			else
				du.FailRender ("Object with Node Handle [" + meshNodeHandle as string + "] not found. Aborting....")
		)--end if
		if setSceneRender do
			XMeshSaverUtils.SetSceneRenderEnd()
	)
	catch
	(
		XMeshSaverUtils.ReleaseLicense()
		throw()
	)
	XMeshSaverUtils.ReleaseLicense()

	du.LogMessage ("Finished XMesh Saving Job in "+ ((timestamp() - st)/1000.0) as string + " sec.") --output the job duration
	true  --return true if the task has finished successfully
)--end script
