-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
MacroScript XMeshSaver category:"Thinkbox" buttonText:"XMesh Saver" toolTip:"Save Geometry To XMesh" silentErrors:false icon:#("XMesh",2)
(

  persistent global XMeshSaverTools_LastProcessedObjects
  if XMeshSaverTools_LastProcessedObjects == undefined do XMeshSaverTools_LastProcessedObjects = #()

  global XMesh_saveParams_rollout
  try(destroyDialog XMesh_saveParams_rollout)catch()

  if classof XMeshSaverUtils != Interface then
  (
		if not IsNetServer() do
		(
			local msg = "XMesh Saver is not available.\n\nThe XMesh Saver is not installed correctly.\n\nTo use XMesh Saver, please:\n1. Install XMesh Saver, and\n2. Add XMesh Saver to this copy of 3ds Max by running Start -> All Programs -> Thinkbox -> XMesh Saver -> Add XMesh Saver to 3ds Max"
			MessageBox msg title:"XMesh Saver"
		)
  )
  else
  (
	local theIniFile = XMeshSaverUtils.SettingsDirectory + "\\XMeshSaver.ini"

	local saveOptions_rollout, saveChannels_rollout, deadlineParams_rollout, advancedSettings_rollout --local definitions of both rollouts, so they can "see" each-other

	local defaultVersionNumber = "v0001"

	local meshFileTypes = #(
		#(".xmesh",#zuprh, ".XMesh","Save Sequence in Thinkbox XMesh Format"),
		#(".obj",#yuprh,".OBJ, Y-up", "Save Sequence in Wavefront OBJ Format using Right-Handed Y-up coordinate space for use with Autodesk MAYA, Autodesk SOFTIMAGE,\nThe Foundry's NUKE and MARI etc."),
		#(".obj",#zuprh, ".OBJ, Z-up","Save Sequence in Wavefront OBJ Format using\n3ds Max Z-up coordinate space.")
	)
	local meshFileTypesArray = for i in meshFileTypes collect i[1]
	local defaultMeshFileType = meshFileTypes[1][1]

	local sourceList = #() --list of sources
	local toSaveList = #()
	local meshesToSave = #() --will store the NODES here
	local TPGroupsList = #()

	local sortDirection = 1
	local lastColumnClicked = 0

	local tabuGeometryClasses = #(TargetObject, KrakatoaPRTLoader, PRT_Volume, PRT_FumeFX, PRT_Maker, PRT_Source, PRT_Hair  )

	local allPossibleSourceChannels = #("Velocity","Color","MaterialID","SmoothingGroup","TextureCoord","Selection","FaceSelection","FaceEdgeVisibility","EdgeSharpness")
	if (maxVersion())[1] >= 18000 do append allPossibleSourceChannels ("VertexSharpness")
	for i = 2 to 99 do append allPossibleSourceChannels ("Mapping" + (i as String))

	local defaultActiveSourceChannels = deepCopy allPossibleSourceChannels
	join allPossibleSourceChannels #("Normal")

	local XMeshSaver_SettingsStructDef
	struct XMeshSaver_SettingsStructDef
	(
		saveMode = 1,
		objectSourceMode = 1,
		ignoreTopology = false,
		ignoreEmpty = true,
		folderPerObject = true,
		frameStart = 0,
		frameEnd = 100,
		frameStep = 1.0,
		proxyFrameStep = 1.0,
		useProxyFrameStep = false,
		baseCachePath = "",
		projectName = "$scene\$user",
		versionNumber = defaultVersionNumber,
		fileBaseName = "$auto",
		filePath = "",
		fileType = defaultMeshFileType,
		coordinateSystem = #zuprh,
		createMaterialLibrary = false,
		createXMeshLoaders = false,
		missingMaterialsHandling = 4,
		sourceObjectsHandling = 4,
		rangeSegments = 1,
		saveProxy = false,
		autoOptimizeProxy = 2,
		VertexPercent = 10.0,
		MinVertices = 1000,
		XMeshLoaderViewport = 1,
		XMeshLoaderViewPercent = 5.0,
		activeSourceChannels = defaultActiveSourceChannels,
		proxyActiveSourceChannels = defaultActiveSourceChannels,
		ObjectSpaceAnimationMode = #bake,
		SavePolymesh = false,
		DisableDataOptimization = false,
		SaveMetaData = true,
		ObjectVisibilityMode = 1
	)
	global XMeshSaver_Settings = XMeshSaver_SettingsStructDef()

	local btn_pickPath_width = 18
	local btn_explorePath_width = 18
	local btn_addToMeshLists_width = 34
	local btn_removeFromMeshLists_width = 34
	local btn_updateLists_width = 60
	local btn_pickBasePath_width = 18
	local btn_ExploreBaseFolder_width = 18
	local btn_projectName_width = 26
	local btn_versionNumber_width = 26
	local btn_fileBaseName_width = 26
	local btn_exploreOutputFolder_width = 18

	-- filterString search box state
	local hasFilterString = false
	local userChangingFilterString = false

	fn getStructSettings =
	(
		for p in getPropNames XMeshSaver_Settings collect #(p,getProperty XMeshSaver_Settings p)
	)

	fn setStructSettings theSettings =
	(
		for p in theSettings where findItem #(#ObjectSourceMode) p[1] == 0 do
			try(setProperty XMeshSaver_Settings p[1]  p[2])catch()
		advancedSettings_rollout.loadSettings fromDisk:false
		saveOptions_rollout.loadSettings fromDisk:false
		saveChannels_rollout.populateLists()
		saveOptions_rollout.updateSamplingStepLists()
		saveOptions_rollout.setSamplingTooltip()
	)

	fn setBaseCachePath val save:true =
	(
		XMesh_saveParams_rollout.edt_basePath.text = XMeshSaver_Settings.baseCachePath = val
		XMesh_saveParams_rollout.updatePathFromSettings()
		setIniSetting theIniFile "SavePath" "BaseCachePath" val
		saveOptions_rollout.canSaveCheck()
	)

	fn setProjectName val save:true =
	(
		XMesh_saveParams_rollout.edt_ProjectName.text = XMeshSaver_Settings.projectName = val
		XMesh_saveParams_rollout.updatePathFromSettings()
		setIniSetting theIniFile "SavePath" "ProjectName" val
	)

	fn setVersionNumber val save:true =
	(
		XMesh_saveParams_rollout.edt_versionNumber.text = XMeshSaver_Settings.versionNumber = val
		XMesh_saveParams_rollout.updatePathFromSettings()
		if val.count == 0 then
			setIniSetting theIniFile "SavePath" "EnableVersionNumber" "false"
		else
			setIniSetting theIniFile "SavePath" "EnableVersionNumber" "true"
	)

	fn setFileBaseName val save:true =
	(
		XMesh_saveParams_rollout.edt_FileBaseName.text = XMeshSaver_Settings.FileBaseName = val
		XMesh_saveParams_rollout.updatePathFromSettings()
		setIniSetting theIniFile "SavePath" "FileBaseName" val
		saveOptions_rollout.canSaveCheck()
	)

	fn findMeshFileType fileType coordinateSystem =
	(
		local foundIndex = -1
		for i = 1 to meshFileTypes.count do
		(
			if meshFileTypes[i][1] == (toLower fileType) do
			(
				if foundIndex == -1 do
				(
					foundIndex = i
				)
				if (coordinateSystem != "") and (meshFileTypes[i][2] == (coordinateSystem as Name)) do
				(
					foundIndex = i
				)
			)
		)
		foundIndex
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

	local upTriangleBitmap, upTriangleMask
	local downTriangleBitmap, downTriangleMask
	local leftTriangleBitmap, leftTriangleMask
	local rightTriangleBitmap, rightTriangleMask
	local columnsIconBitmap, columnsIconMask
	local twoArrowsIconBitmap, twoArrowsIconMask

	fn toIntegerColor c =
	(
		color (255*c.r) (255*c.g) (255*c.b)
	)
	fn createBitmaps =
	(
		local upTriangleData = #(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,
			0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,
			0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,
			0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,
			0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)

		local downTriangleData = #(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,
			0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,
			0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,
			0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,
			0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)

		local rightTriangleData = #(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,
			0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,
			0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,
			0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,
			0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,
			0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,
			0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,
			0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)

		local leftTriangleData = #(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,
			0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,
			0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,
			0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,
			0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,
			0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,
			0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,
			0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)

		local twoArrowsIconData = #(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,
			0,0,1,1,0,0,0,0,0,1,1,0,0,0,0,0,
			0,0,1,0,1,0,0,0,0,1,0,1,0,0,0,0,
			0,0,1,0,0,1,0,0,0,1,0,0,1,0,0,0,
			0,0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,
			0,0,1,0,0,0,0,1,0,1,0,0,0,0,1,0,
			0,0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,
			0,0,1,0,0,1,0,0,0,1,0,0,1,0,0,0,
			0,0,1,0,1,0,0,0,0,1,0,1,0,0,0,0,
			0,0,1,1,0,0,0,0,0,1,1,0,0,0,0,0,
			0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)

		local columnsIconData = #(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,1,1,1,1,1,1,0,0,1,0,0,0,0,0,
			0,0,1,0,0,0,0,1,0,0,1,1,0,0,0,0,
			0,0,1,0,0,0,0,1,0,0,1,1,1,0,0,0,
			0,0,1,0,0,0,0,1,0,0,1,1,1,1,0,0,
			0,0,1,0,0,0,0,1,0,0,1,1,1,1,1,0,
			0,0,1,0,0,0,0,1,0,0,1,1,1,1,1,1,
			0,0,1,0,0,0,0,1,0,0,1,1,1,1,1,0,
			0,0,1,0,0,0,0,1,0,0,1,1,1,1,0,0,
			0,0,1,0,0,0,0,1,0,0,1,1,1,0,0,0,
			0,0,1,0,0,0,0,1,0,0,1,1,0,0,0,0,
			0,0,1,1,1,1,1,1,0,0,1,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)

		fn copyDataToImage &dest &destMask source foreColor xOffset =
		(
			local colorPixel = foreColor
			local clearPixel = toIntegerColor ((colorman.getcolor #background) as color)
			local opaque = (color 0 0 0)
			local transparent = (color 255 255 255)
			local i = 1
			for y = 0 to 15 do
			(
				local rowPixels = getPixels dest [xOffset,y] 16
				local rowMask = getPixels destMask [xOffset,y] 16
				for x = 0 to 15 do
				(
					rowPixels[x+1] = if source[i] != 0 then colorPixel else clearPixel
					rowMask[x+1] = if source[i] != 0 then opaque else transparent
					i += 1
				)
				setPixels dest [xOffset,y] rowPixels
				setPixels destMask [xOffset,y] rowMask
			)
		)
		upTriangleBitmap = bitmap 32 16
		upTriangleMask = bitmap 32 16
		copyDataToImage &upTriangleBitmap &upTriangleMask upTriangleData (toIntegerColor ((colorman.getcolor #text) as color)) 0
		copyDataToImage &upTriangleBitmap &upTriangleMask upTriangleData (toIntegerColor ((colorman.getcolor #shadow) as color)) 16

		downTriangleBitmap = bitmap 32 16
		downTriangleMask = bitmap 32 16
		copyDataToImage &downTriangleBitmap &downTriangleMask downTriangleData (toIntegerColor ((colorman.getcolor #text) as color)) 0
		copyDataToImage &downTriangleBitmap &downTriangleMask downTriangleData (toIntegerColor ((colorman.getcolor #shadow) as color)) 16

		leftTriangleBitmap = bitmap 32 16
		leftTriangleMask = bitmap 32 16
		copyDataToImage &leftTriangleBitmap &leftTriangleMask leftTriangleData (toIntegerColor ((colorman.getcolor #text) as color)) 0
		copyDataToImage &leftTriangleBitmap &leftTriangleMask leftTriangleData (toIntegerColor ((colorman.getcolor #shadow) as color)) 16

		rightTriangleBitmap = bitmap 32 16
		rightTriangleMask = bitmap 32 16
		copyDataToImage &rightTriangleBitmap &rightTriangleMask rightTriangleData (toIntegerColor ((colorman.getcolor #text) as color)) 0
		copyDataToImage &rightTriangleBitmap &rightTriangleMask rightTriangleData (toIntegerColor ((colorman.getcolor #shadow) as color)) 16


		twoArrowsIconBitmap = bitmap 32 16
		twoArrowsIconMask = bitmap 32 16
		copyDataToImage &twoArrowsIconBitmap &twoArrowsIconMask twoArrowsIconData (toIntegerColor ((colorman.getcolor #text) as color)) 0
		copyDataToImage &twoArrowsIconBitmap &twoArrowsIconMask twoArrowsIconData (toIntegerColor ((colorman.getcolor #shadow) as color)) 16

		columnsIconBitmap = bitmap 32 16
		columnsIconMask = bitmap 32 16
		copyDataToImage &columnsIconBitmap &columnsIconMask columnsIconData (toIntegerColor ((colorman.getcolor #text) as color)) 0
		copyDataToImage &columnsIconBitmap &columnsIconMask columnsIconData (toIntegerColor ((colorman.getcolor #shadow) as color)) 16

	)
	createBitmaps()

	local dialogSizeToSave = undefined
	local dialogPositionToSave = undefined

	fn updateTitlebar =
	(
		XMesh_saveParams_rollout.title =  "XMesh Saver - v" + XMeshSaverUtils.VersionNumber -- + (if XMeshSaverUtils.HasLicense then " - License Acquired" else " - Not Holding A License")
	)

	fn buildCommaSeparatedStringFromArray a =
	(
		local s = ""
		for i = 1 to a.count do
		(
			if i > 1 do s += ","
			s += a[i]
		)
		s
	)

	fn isXMeshLoaderInstalled =
	(
		-- check for renderSequence, which does not exist in alpha releases
		(classof XMeshLoader == GeometryClass) and (findItem (getPropNames XMeshLoader) #renderSequence) > 0
	)

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

	fn getProxyPath path =
	(
		local basePath = GetFilenamePath path
		local baseType = GetFilenameType path
		local baseNameWithNumber = GetFilenameFile path
		local baseName = (TrimRight baseNameWithNumber "1234567890#,")
		local numberString = ""
		for i = baseName.count+1 to baseNameWithNumber.count do
			numberString = numberString + baseNameWithNumber[i]

		local proxyDir = basePath + baseName + "_proxy" + "/"

		proxyPath = proxyDir + baseName + "_proxy" + numberString + baseType
		proxyPath
	)

	fn buildCacheFileNamePreSubst =
	(
		local finalPath = XMeshSaver_Settings.baseCachePath
		local theProjectName = XMeshSaver_Settings.projectName
		if theProjectName != "" do
		(
			if (not matchPattern finalPath pattern:"*\\") and (not matchPattern theProjectName pattern:"\\*") do
				finalPath += "\\"
			finalPath += theProjectName
		)
		local theVersionString = XMeshSaver_Settings.versionNumber
		if theVersionString != "" do
		(
			if (not matchPattern finalPath pattern:"*\\") and (not matchPattern theVersionString pattern:"\\*") do
				finalPath += "\\"
			finalPath += theVersionString
		)
		local theFileBaseName = XMeshSaver_Settings.fileBaseName
		if finalPath.count > 0 and (not matchPattern finalPath pattern:"*\\") and (not matchPattern theFileBaseName pattern:"\\*") do
			finalPath += "\\"
		finalPath += theFileBaseName
		if finalPath.count > 0 do
			finalPath += XMeshSaver_Settings.fileType
		finalPath
	)

	fn hasPathSubst str =
	(
		(matchPattern str pattern:"$scene" ignoreCase:false) \
			or (matchPattern str pattern:"$user" ignoreCase:false) \
			or (matchPattern str pattern:"$auto" ignoreCase:false) \
			or (matchPattern str pattern:"$date" ignoreCase:false)
	)

	fn performPathSubst finalPath numobjects: =
	(
		local theMaxSceneName = getFileNameFile maxFileName
		if theMaxSceneName == "" do theMaxSceneName = "Untitled"

		local theAutoObjectsName = "XMesh_"
		case XMeshSaver_Settings.saveMode of
		(
			1:
			(
				theAutoObjectsName = "XMesh_MultiWS"
				if numobjects != unsupplied do theAutoObjectsName += numobjects as string
				theAutoObjectsName += "_"
			)
			2:
			(
				theAutoObjectsName = "XMesh_SingleWS"
			)
			3:
			(
				theAutoObjectsName = "XMesh_SingleOS"
			)
		)

		finalPath = substituteString finalPath "$scene" theMaxSceneName
		finalPath = substituteString finalPath "$user" sysinfo.username
		finalPath = substituteString finalPath "$auto" theAutoObjectsName
		finalPath = substituteString finalPath "$date" (validateObjectNameAsFileName localtime)

		finalPath
	)

	fn buildCacheFileName numobjects: =
	(
		local finalPath = buildCacheFileNamePreSubst()

		performPathSubst finalPath numobjects:numObjects
	)

	fn getZeroes theNumber theBase =
	(
		local theCount = theBase - (theNumber as string).count
		if theCount > 0 then substring "0000000" 1 theCount else ""
	)

	fn getNextVersion next: =
	(
		local finalPath = XMeshSaver_Settings.baseCachePath
		if not matchPattern finalPath pattern:"*\\" do finalPath += "\\"
		local theMaxSceneName = getFileNameFile maxFileName
		if theMaxSceneName == "" do theMaxSceneName = "Untitled"
		local projectName = substituteString XMeshSaver_Settings.projectName "$scene" theMaxSceneName
		local projectName = substituteString projectName "$user" sysinfo.username
		finalPath += projectName
		local theDirs = getDirectories (finalPath +"\\*")
		local theBase = 4
		local theVersions = for i in theDirs collect
		(
			local theLeaf = pathConfig.stripPathToLeaf (substring i 1 (i.count-1))
			local theNumber = try(execute (substring theLeaf 2 -1))catch(undefined)
			if matchPattern theLeaf pattern:"v*" and classof theNumber == Integer then
			(
				theBase = theLeaf.count-1
				theNumber
			)
			else
				dontcollect
		)
		sort theVersions
		local theNewVersion = if theVersions.count > 0 then
		(
			theVersions[theVersions.count]
		)
		else
			0

		theNewVersion = case next of
		(
			default: (theNewVersion + 1)
			#last: theNewVersion
			#minor: (( (floor (theNewVersion/10.0)+1)*10) as integer)
			#major: (( (floor (theNewVersion/100.0)+1)*100) as integer)
		)

		"v" + getZeroes theNewVersion theBase + theNewVersion as string
	)

	fn isVersionString s =
	(
		if s != undefined and s.count > 1 then
		(
			i = (substring s 2 -1) as Integer
			(matchPattern s pattern:"v*") and (not isSpace s[2]) and (i != undefined)
		)
		else
			false
	)

	fn splitFileSequenceNumber s =
	(
		local doneSequenceNumber = false
		local gotSubframe = false
		local sequenceNumber = ""
		for i = s.count to 1 by -1 while (not doneSequenceNumber) do
		(
			local n = (s[i] as Integer)
			if (s[i] == "0" or (n != undefined and n > 0 and n <= 9)) or s[i] == "#" then
			(
				doneSequenceNumber = false
			)
			else if (s[i] == ",") then
			(
				if gotSubframe then
					doneSequenceNumber = true
				else
					gotSubframe = true
			)
			else
			(
				doneSequenceNumber = true
			)
			if not doneSequenceNumber do
				sequenceNumber = s[i] + sequenceNumber
		)
		local base = substring s 1 (s.count-sequenceNumber.count)
		if base.count > 0 then
			#(base,sequenceNumber)
		else
			#(sequenceNumber,"")
	)

	fn hasSequenceNumber s =
	(
		(splitFileSequenceNumber s)[2].count > 0
	)

	fn getPathComponents thePath numObjects:"" =
	(
		local theBase = ""
		local theProject = ""
		local theVersionNumber = ""
		local theFileName = ""
		local theFileType = ""

		fn minimum a b = ( if a < b then a else b )
		fn maximum a b = ( if a > b then a else b )

		fn reverseArray a = ( for i = a.count to 1 by -1 collect a[i] )
		fn splitPath p = ( filterString p "\\/" )

		theFileType = getFilenameType thePath
		local i = findItem meshFileTypesArray (toLower theFileType)
		if i > 0 then
			theFileType = meshFileTypesArray[i]
		else
			theFileType = defaultMeshFileType

		local theOldFileName = XMeshSaver_Settings.fileBaseName
		local theOldFileNameSubst = performPathSubst theOldFileName numObjects:numObjects
		theFileName = getFilenameFile thePath
		theFileName = (splitFileSequenceNumber theFileName)[1]
		i = findString theFileName "$object"
		if i != undefined do
		(
			theFileName = substring theFileName 1 (i-1)
			if theFileName.count > 0 and theFileName[theFileName.count] == "_" do
				theFileName = substring theFileName 1 (theFileName.count-1)
		)
		--if theFileName.count == 0 do
			--theFileName = "$auto"
		if theFileName == theOldFileNameSubst do
			theFileName = theOldFileName

		thePath = getFilenamePath thePath

		local isNetworkPath = matchpattern thePath pattern:"\\\\*"
		local fString = splitPath thePath
		fString = reverseArray fString
		fString = for i in fString where i.count > 0 collect i

		-- remove Sub-Folder Per Object if it is present
		if fString.count > 0 and findString fString[1] "$object" != undefined do
			deleteItem fString 1

		-- make sure nothing else has $object, because we don't handle it normally
		-- TODO: handle $object normally
		fString = for i in fString collect (substituteString i "$object" "")
		fString = for i in fString collect (if i.count > 0 then i else dontcollect)

		-- is the last folder a version folder?
		if isVersionString fString[1] then
		(
			theVersionNumber = fString[1]
			deleteItem fString 1
		)
		else
		(
			theVersionNumber = ""
		)

		-- put unc computer name and drive letter in theBase
		if isNetworkPath then
		(
			theBase = "\\\\"
			if fString.count > 0 do
			(
				theBase += fString[fString.count]
				deleteItem fString fString.count
			)
		)
		if fString.count > 0 and (findString fString[fString.count] ":" != undefined) do
		(
			theBase += fString[fString.count]
			deleteItem fString fString.count
		)

		-- assume that everything with a $subst belongs in the project
		local lastSubstIndex = 0
		for i = 1 to fString.count do
		(
			if hasPathSubst fString[i] do
			(
				lastSubstIndex = i
			)
		)

		-- try to match the old project string
		local oldProj = XMeshSaver_Settings.projectName
		local oldProjSplit = reverseArray(splitPath oldProj)
		local oldProjSubst = performPathSubst oldProj numObjects:numObjects
		local oldProjSubstSplit = reverseArray(splitPath oldProjSubst)

		local matchesOldProj = true
		local lastMatchIndex = 0
		if oldProjSplit.count == oldProjSubstSplit.count then
		(
			for i = 1 to (minimum oldProjSubstSplit.count fString.count) while matchesOldProj do
			(
				if fString[i] == oldProjSubstSplit[i] then
				(
					fString[i] = oldProjSplit[i]
					lastMatchIndex = i
				)
				else
					matchesOldProj = false
			)
		)

		-- build the project string from subfolders with $subst, or subfolders that match with the old project
		for i = 1 to (maximum lastSubstIndex lastMatchIndex) do
		(
			if theProject.count == 0 then
				theProject = fString[1]
			else
				theProject = fString[1] + "\\" + theProject
			deleteItem fString 1
		)

		-- if the project is empty, use the last subfolder
		if theProject.count == 0 and fString.count > 0 do
		(
			theProject = fString[1]
			deleteItem fString 1
		)

		fString = reverseArray fString

		-- put everything left over into the base
		if theBase.count > 0 and (not matchPattern theBase pattern:"*\\") do
			theBase += "\\"
		for s in fString do
		(
			theBase += s + "\\"
		)

		#(theBase,theProject,theVersionNumber,theFileName,theFileType)
	)

	fn SaveMatIDMapFile theMaterial theMatIDMapFile =
	(
		deleteFile theMatIDMapFile
		if theMaterial != undefined do
		(
			if classof theMaterial == MultiMaterial then
			(
				for m = 1 to theMaterial.numsubs do
				(
					setIniSetting theMatIDMapFile "MaterialNames" ((theMaterial.materialIDList[m] - 1) as String) (try(theMaterial.materialList[m].name)catch("undefined"))
				)
			)
			else
			(
				setIniSetting theMatIDMapFile "MaterialNames" "0" (try(theMaterial.name)catch("undefined"))
			)
		)
	)

	fn getFrameList theStep=
	(
		local currentTimeValue = (XMeshSaver_Settings.frameStart as time) as integer
		local currentTimeStep = (TicksPerFrame/theStep) as integer
		local endFrameAsTicks = (XMeshSaver_Settings.frameEnd as time) as integer
		local theFramesToSave = #()
		while currentTimeValue <= endFrameAsTicks do
		(
			append theFramesToSave (currentTimeValue as float/TicksPerFrame)
			currentTimeValue+= currentTimeStep
		)
		if findItem theFramesToSave XMeshSaver_Settings.frameEnd == 0 do append theFramesToSave XMeshSaver_Settings.frameEnd
		--with PrintAllElements on XMeshSaverUtils.LogProgress (theFramesToSave as string)
		theFramesToSave
	)

	fn getValidSamplingSteps =
	(
		join #(0.1,0.2,0.5) (for i = 1 to TicksPerFrame where (TicksPerFrame/i) as float == 1.0*TicksPerFrame/i  collect i)
	)

	fn collectAllGeometry selOnly:false =
	(
		local theObjects = if selOnly then selection as array else objects as array
		for o in theObjects where ( findItem GeometryClass.classes (classof o) > 0 and findItem tabuGeometryClasses (classof o.baseobject) == 0 ) or classof o.baseobject == XRefObject collect o
	)

	fn collectTPGroups theParent theBaseName theTP =
	(
		for g = 1 to theParent.NumPGroups() do
		(
			local theGroup = theParent.getPGroup g
			if theGroup != undefined do
			(
				local theName = theBaseName + ">" + (theParent.PGroupName g)
				append TPGroupsList #(theGroup, theName, theTP)
				collectTPGroups theGroup theName theTP
			)
		)
	)

	struct RunToDoNothing
	(
		fn run = ()
	)

	struct RunToResetTPRenderableState
	(
		theTP = undefined,
		theTPGroupsList = #(),
		theEnabledStates = #(),
		fn run =
		(
			if theTP != undefined and theEnabledStates.count > 0 do
			(
				for i = 1 to theTPGroupsList.count do theTPGroupsList[i][1].renderable = theEnabledStates[i]
			)
		)
	)

	fn EnableTPGroups theNode theGroups =
	(
		local theRestoreObject = RunToDoNothing()
		if XMeshSaver_Settings.ObjectSourceMode == 9 do
		(
			TPGroupsList = #()
			collectTPGroups theNode.GroupManager theNode.name theNode

			theRestoreObject = RunToResetTPRenderableState()
			theRestoreObject.theTP = theNode
			theRestoreObject.theTPGroupsList = deepcopy TPGroupsList
			theRestoreObject.theEnabledStates = for i in TPGroupsList collect i[1].renderable

			--local TPGroupsToRender = for i in theGroups collect i[1]
			for i in TPGroupsList do i[1].renderable = findItem theGroups i[1] > 0
		)
		theRestoreObject
	)

	fn GetNodeMeshes theNode =
	(
		local theNodeMeshes = #()

		if isValidObj theNode do
		(
			if classof theNode == PF_Source then
			(
				local dependentNodes = refs.dependentNodes theNode
				for aNode in dependentNodes where (isValidNode aNode and classof aNode == ParticleGroup) do
						appendIfUnique theNodeMeshes aNode
			)
			else if ((findItem GeometryClass.classes (classof theNode) > 0 or (try(classof theNode.baseobject == XRefObject)catch(false))) and (classof theNode) != TargetObject) then
			(
				theNodeMeshes = #(theNode)
			)
		)

		theNodeMeshes
	)

	fn GetMeshToSaveName theMeshesToSave i =
	(
		if XMeshSaver_Settings.ObjectSourceMode != 9 then --if not TP groups
		(
			theMeshesToSave[i].name
		)
		else
		(
			theMeshesToSave[i][2]
		)
	)

	fn GetMeshToSaveNode theMeshesToSave i =
	(
		if XMeshSaver_Settings.ObjectSourceMode != 9 then --if not TP groups
		(
			theMeshesToSave[i]
		)
		else
		(
			theMeshesToSave[i][3]
		)
	)

	fn GetMeshesToSave =
	(
		local theMeshesToSave = #()

		if findItem #(9,14) XMeshSaver_Settings.ObjectSourceMode == 0 then --if not TP groups and not Previous Session
		(
			for o in toSaveList where (XMeshSaver_Settings.ObjectSourceMode == 4 and not isDeleted o) or (findItem #(4,5) XMeshSaver_Settings.ObjectSourceMode == 0 and isValidObj o) or XMeshSaver_Settings.ObjectSourceMode == 5 do
			(
				case XMeshSaver_Settings.ObjectSourceMode of
				(
					4:
					(
						for aSelection in o do
						(
							local theItemMeshes = GetNodeMeshes aSelection
							for j in theItemMeshes do
								appendIfUnique theMeshesToSave j
						)
					)
					5:
					(
						for i in refs.dependents (ILayerManager.getLayerObject o.name) do
							for j in GetNodeMeshes i do
								appendIfUnique theMeshesToSave j
					)
					12:
					(
						for i in o.children do appendIfUnique theMeshesToSave i
					)
					13:
					(
						local theChildren = XMesh_saveParams_rollout.CollectChildrenRecursive o
						for i in theChildren do appendIfUnique theMeshesToSave i
					)
					default:
					(
						local theNodeMeshes = GetNodeMeshes o
						for i in theNodeMeshes do
							appendIfUnique theMeshesToSave i
					)

				)
			)
			theMeshesToSave = for o in theMeshesToSave where findItem GeometryClass.classes (classof o) > 0 and findItem tabuGeometryClasses (classof o.baseobject) == 0 or classof o.baseobject == XRefObject collect o
		)
		else if XMeshSaver_Settings.ObjectSourceMode == 14 then --previous session
		(
			for o in toSaveList do
				for i in o[3] where isValidNode i do appendIfUnique theMeshesToSave i
		)
		else --TP
		(
			for o in toSaveList do appendIfUnique theMeshesToSave o
		)

		theMeshesToSave
	)

	fn CanMakeProxyModifyTheScene = (
		XMeshSaver_Settings.ObjectSourceMode != 9 and XMeshSaver_Settings.autoOptimizeProxy > 1
	)

	fn UpdateProxy theMeshes =
	(
		if CanMakeProxyModifyTheScene() do
		(
			for o in theMeshes where findItem #(ParticleGroup) (classof o.baseobject) == 0  do
			(
				case XMeshSaver_Settings.autoOptimizeProxy of
				(
					default:
					(
					)
					3:
					(
						for theMod in o.modifiers where classof theMod == MultiRes do
						(
							max modify mode
							ModPanel.setCurrentObject theMod
							theMod.reqGenerate = true
							ModPanel.setCurrentObject theMod
							theMod.VertexPercent =  XMeshSaver_Settings.VertexPercent
							classof o
							if theMod.VertexCount <  XMeshSaver_Settings.MinVertices do theMod.VertexCount = XMeshSaver_Settings.MinVertices
							--format "VP:% VC:%\n" theMod.VertexPercent theMod.VertexCount
						)
					)
					4:
					(
						for theMod in o.modifiers where classof theMod == ProOptimizer do
						(
							max modify mode
							ModPanel.setCurrentObject theMod
							theMod.calculate = false
							classof o
							theMod.calculate = true
							ModPanel.setCurrentObject theMod
							theMod.VertexPercent =  XMeshSaver_Settings.VertexPercent
							classof o
							if theMod.VertexCount <  XMeshSaver_Settings.MinVertices do theMod.VertexCount = XMeshSaver_Settings.MinVertices
							--format "VP:% VC:% OK:%\n" theMod.VertexPercent theMod.VertexCount theMod.calculate
						)
					)
				)--end case
			)--end o loop
		)--end if can make proxy
	)

	-- call before saving proxy file
	fn MakeProxy theMeshes =
	(
		if CanMakeProxyModifyTheScene() do
		(
			for o in theMeshes where findItem #(ParticleGroup) (classof o.baseobject) == 0  do
			(
				case XMeshSaver_Settings.autoOptimizeProxy of
				(
					default:
					(
						local theMod = Optimize()
						if validModifier o theMod do
						(
							addModifier o theMod
							theMod.enabledInViews = true
							theMod.enabledInRenders  = false
							theMod.name = "XMeshProxy_DeleteMe"
						)
					)
					3:
					(
						local theMod = MultiRes()
						if validModifier o theMod do
						(
							addModifier o theMod
							theMod.enabledInViews = true
							theMod.enabledInRenders  = false
							theMod.name = "XMeshProxy_DeleteMe"
							max modify mode
							ModPanel.setCurrentObject theMod
							theMod.reqGenerate = true
							ModPanel.setCurrentObject theMod
							theMod.VertexPercent =  XMeshSaver_Settings.VertexPercent
							classof o
							if theMod.VertexCount <  XMeshSaver_Settings.MinVertices do theMod.VertexCount = XMeshSaver_Settings.MinVertices
							--format "VP:% VC:%\n" theMod.VertexPercent theMod.VertexCount
						)
					)
					4:
					(
						local theMod = ProOptimizer()
						if validModifier o theMod do
						(
							addModifier o theMod
							theMod.enabledInViews = true
							theMod.enabledInRenders  = false
							theMod.name = "XMeshProxy_DeleteMe"
							max modify mode
							ModPanel.setCurrentObject theMod
							theMod.calculate = true
							ModPanel.setCurrentObject theMod
							theMod.VertexPercent =  XMeshSaver_Settings.VertexPercent
							classof o
							if theMod.VertexCount <  XMeshSaver_Settings.MinVertices do theMod.VertexCount = XMeshSaver_Settings.MinVertices
							--format "VP:% VC:% OK:%\n" theMod.VertexPercent theMod.VertexCount theMod.calculate
						)
					)
				)--end case
			)--end o loop
		)--end if can make proxy
	)

	-- call after saving proxy file
	fn RemoveProxy theMeshes =
	(
		--return false
		if XMeshSaver_Settings.ObjectSourceMode != 9 and XMeshSaver_Settings.autoOptimizeProxy > 1 do
		(
			for o in theMeshes do
			(
				try
				(
					for m = o.modifiers.count to 1 by -1 where o.modifiers[m].name == "XMeshProxy_DeleteMe" do deleteModifier o m
				)
				catch()
			)
		)
	)

	fn prepareMetaData theMeshesToSave =
	(
		XMeshSaverUtils.ClearUserData()
		if XMeshSaver_Settings.SaveMetaData do
		(
			--XMeshSaverUtils.SetUserData "Username" sysinfo.username
			--XMeshSaverUtils.SetUserData "Computername" sysinfo.computername
			XMeshSaverUtils.SetUserData "Localtime" (localtime)
			XMeshSaverUtils.SetUserData "SaveMode" (#("All Objects In World Space","Individual Objects In World Space","Individual Objects In Object Space")[XMeshSaver_Settings.saveMode])
			XMeshSaverUtils.SetUserData "SourceMode" (XMesh_saveParams_rollout.ddl_objectSourceMode.items[XMeshSaver_Settings.objectSourceMode])
			XMeshSaverUtils.SetUserData "MeshCount" (theMeshesToSave.count as string)
			for i = 1 to theMeshesToSave.count do
				XMeshSaverUtils.SetUserData ("Mesh_"+i as string) (GetMeshToSaveName theMeshesToSave i)
		)
	)

	rcmenu SourceList_Menu
	(
		fn isSessionHistory = XMeshSaver_Settings.ObjectSourceMode == 14
		menuItem mnu_selectAll "Select ALL"
		separator sep_10
		menuItem mnu_moveToSaveList "MOVE to Save List" enabled:(XMesh_saveParams_rollout.dnc_sourceList.SelectedIndices.Count > 0)
		separator sep_50 filter:isSessionHistory
		menuItem mnu_restorePresetFromHistory "RESTORE Settings From Session History Record..." filter:isSessionHistory enabled:(XMesh_saveParams_rollout.dnc_sourceList.SelectedIndices.Count == 1)
		separator sep_100 filter:isSessionHistory
		menuItem mnu_removeSelectedFromHistory "DELETE Selected Record from Session History..." filter:isSessionHistory

		on mnu_restorePresetFromHistory picked do
		(
			local theSel = XMesh_saveParams_rollout.getListViewSelection XMesh_saveParams_rollout.dnc_sourceList
			if theSel.count == 1 do
			(
				local theDate = XMesh_saveParams_rollout.dnc_sourceList.Items.Item[theSel[1]-1].SubItems.Item[1].Text
				local theSettings = undefined
				for i in ::XMeshSaverTools_LastProcessedObjects where i[1] == theDate do theSettings = i[4]
				if theSettings != undefined do setStructSettings theSettings
			)
		)

		on mnu_removeSelectedFromHistory picked do
		(
			local q = querybox "Are you sure you want to REMOVE\nthe highlighted Sessions from the History List?\n\nThis step cannot be undone!\n" title:"XMesh Saver Session History"
			if q then
			(
				local theSel = XMesh_saveParams_rollout.getListViewSelection XMesh_saveParams_rollout.dnc_sourceList
				for i = 1 to theSel.count do
				(
					deleteItem ::XMeshSaverTools_LastProcessedObjects (XMesh_saveParams_rollout.dnc_sourceList.Items.count - theSel[i] + 1)
				)
				XMesh_saveParams_rollout.updateAllLists()
			)
		)

		on mnu_selectAll picked do
		(
			for i = 0 to (XMesh_saveParams_rollout.dnc_sourceList.Items.Count-1) do
				XMesh_saveParams_rollout.dnc_sourceList.SelectedIndices.Add i
		)
		on mnu_moveToSaveList picked do
		(
			XMesh_saveParams_rollout.addToMeshList()
		)
	)

	rcmenu SaveList_Menu
	(
		fn isSessionHistory = XMeshSaver_Settings.ObjectSourceMode == 14
		menuItem mnu_selectAll "Select ALL"
		separator sep_10
		menuItem mnu_removeFromSaveList "REMOVE from Save List" enabled:(XMesh_saveParams_rollout.dnc_meshesToSave.SelectedIndices.Count > 0)
		separator sep_100 filter:isSessionHistory
		menuItem mnu_restorePresetFromHistory "RESTORE Settings From Session History Record..." filter:isSessionHistory enabled:(XMesh_saveParams_rollout.dnc_meshesToSave.SelectedIndices.Count == 1)

		on mnu_restorePresetFromHistory picked do
		(
			local theSel = XMesh_saveParams_rollout.getListViewSelection XMesh_saveParams_rollout.dnc_meshesToSave
			if theSel.count == 1 do
			(
				local theDate = XMesh_saveParams_rollout.dnc_meshesToSave.Items.Item[theSel[1]-1].SubItems.Item[1].Text
				local theSettings = undefined
				for i in ::XMeshSaverTools_LastProcessedObjects where i[1] == theDate do theSettings = i[4]
				if theSettings != undefined do setStructSettings theSettings
			)
		)

		on mnu_selectAll picked do
		(
			for i = 0 to (XMesh_saveParams_rollout.dnc_meshesToSave.Items.Count-1) do
				XMesh_saveParams_rollout.dnc_meshesToSave.SelectedIndices.Add i
		)
		on mnu_removeFromSaveList picked do
		(
			XMesh_saveParams_rollout.removeFromMeshLists()
		)
	)

	rcmenu ProjectName_Menu
	(
		menuItem mnu_settoDefault "Set to Default [$scene\$user]"
		separator sep_10
		menuItem mnu_addScene "Add \$scene"
		menuItem mnu_addUser "Add \$user"
		menuItem mnu_addDate "Add \$date"
		separator sep_20
		menuItem mnu_clearField "Clear Project Field"

		on mnu_settoDefault picked do
		(
			setProjectName "$scene\$user"
		)
		on mnu_addScene picked do
		(
			setProjectName (XMeshSaver_Settings.projectName += "\\$scene")
		)
		on mnu_addUser picked do
		(
			setProjectName (XMeshSaver_Settings.projectName += "\\$user")
		)
		on mnu_addDate picked do
		(
			setProjectName (XMeshSaver_Settings.projectName += "\\$date")
		)
		on mnu_clearField picked do
		(
			setProjectName ""
		)
	)

	rcmenu VersionNumber_Menu
	(
		menuItem mnu_setToLatest "Set to Last Version"
		separator sep_10
		menuItem mnu_setToNextVersion "Set to Next Version (Increase By 1)"
		menuItem mnu_setToNextMinor "Set to Next Minor Version (Increase By 10)"
		menuItem mnu_setToNextMajor "Set to Next Major Version (Increase By 100)"
		separator sep_20
		menuItem mnu_clearField "Clear Version Field"

		on mnu_setToLatest picked do
		(
			setVersionNumber (getNextVersion next:#last)
		)
		on mnu_setToNextVersion picked do
		(
			setVersionNumber (getNextVersion next:#version)
		)
		on mnu_setToNextMinor picked do
		(
			setVersionNumber (getNextVersion next:#minor)
		)
		on mnu_setToNextMajor picked do
		(
			setVersionNumber (getNextVersion next:#major)
		)
		on mnu_clearField picked do
		(
			setVersionNumber ""
		)
	)

	rcmenu FileBaseName_Menu
	(
		menuItem mnu_settoDefault "Set to Default [$auto]"
		separator sep_10
		menuItem mnu_addScene "Add _$scene"
		menuItem mnu_addUser "Add _$user"
		separator sep_20
		menuItem mnu_clearField "Clear File Name Field"

		on mnu_settoDefault picked do
		(
			setFileBaseName "$auto"
		)
		on mnu_addScene picked do
		(
			setFileBaseName (XMeshSaver_Settings.FileBaseName += "_$scene")
		)
		on mnu_addUser picked do
		(
			setFileBaseName (XMeshSaver_Settings.FileBaseName += "_$user")
		)
		on mnu_clearField picked do
		(
			setFileBaseName ""
		)
	)

	rcmenu mnu_columnsResize
	(

 		fn desktopsize1440 = sysinfo.desktopsize.x >=1440
		fn desktopsize1920 = sysinfo.desktopsize.x >=1920
		menuItem mnu_openHelp "Open Online Help..."
		separator sep_10

		menuItem mnu_Columns1 "1 Column  (480 px)" checked:((getDialogSize XMesh_saveParams_rollout).x == 480)
		menuItem mnu_Columns2 "2 Columns (960 px)" checked:((getDialogSize XMesh_saveParams_rollout).x == 961)
		menuItem mnu_Columns3 "3 Columns (1440 px)" checked:((getDialogSize XMesh_saveParams_rollout).x == 1441) filter:desktopsize1440
		menuItem mnu_Columns4 "4 Columns (1920 px)" checked:((getDialogSize XMesh_saveParams_rollout).x == 1920) filter:desktopsize1920
		
		separator sep_20
		
		menuItem mnu_aboutDialog "About XMesh MX..."

		on mnu_Columns1 picked do XMesh_saveParams_rollout.resized [XMesh_saveParams_rollout.width =480,(getDialogSize XMesh_saveParams_rollout).y]
		on mnu_Columns2 picked do XMesh_saveParams_rollout.resized [XMesh_saveParams_rollout.width =961,(getDialogSize XMesh_saveParams_rollout).y]
		on mnu_Columns3 picked do XMesh_saveParams_rollout.resized [XMesh_saveParams_rollout.width =1441,(getDialogSize XMesh_saveParams_rollout).y]
		on mnu_Columns4 picked do XMesh_saveParams_rollout.resized [XMesh_saveParams_rollout.width =1920,(getDialogSize XMesh_saveParams_rollout).y]
		
		on mnu_aboutDialog picked do macros.run "XMesh" "AboutXMesh"
	)

	rollout XMesh_saveParams_rollout "XMesh Saver"
	(
		dropdownlist ddl_objectSourceMode width:160 offset:[-10,-3] across:6 height:20 align:#left \
			items:#("Selected Geometry","Scene Geometry","Visible Scene Geometry","Named Selection Sets","Layers","Particle Flow Systems","Particle Flow Events","Thinking Particles Systems","Thinking Particles Groups","Frost Objects Only","Scene XMesh Loaders","3ds Max Groups","3ds Max Nested Groups","Previous Sessions History")
		button btn_updateLists "RELOAD" width:btn_updateLists_width align:#center offset:[85,-2] \
			tooltip:"Reset both lists to defaults - clear the bottom (objects to save) list and display all available objects according to the current mode in the top (sources) list."
		button btn_addToMeshLists "ADD" width:btn_addToMeshLists_width height:21 align:#center offset:[80,-2] \
			images:#(downTriangleBitmap,downTriangleMask,2,1,1,2,2) \
			tooltip:"Move the highlighted objects from the top (sources) list to the bottom (objects to save) list.\nAlternatively, double-click in the top list."
		button btn_removeFromMeshLists "REMOVE" width:btn_removeFromMeshLists_width height:21 align:#center offset:[55,-2] \
			images:#(upTriangleBitmap,upTriangleMask,2,1,1,2,2) \
			tooltip:"Move the highlighted objects from the bottom (objects to save) list to the top (sources) list.\nAlternatively, double-click in the bottom list."
		dotNetControl edt_filterString "System.Windows.Forms.TextBox" offset:[50,-2] width:130 height:20 align:#right

		button btn_columnsIcon "[][]" width:21 height:21 align:#right offset:[15,-2] \
			images:#(columnsIconBitmap,columnsIconMask,2,1,1,2,2) \
			tooltip:"Show Menu to Open Online Help or\nSwitch the Layout between 1, 2, 3 and 4 Columns"



		dotNetControl dnc_sourceList "System.Windows.Forms.ListView" width:480 height:200 pos:[0,30] --align:#center offset:[0,-4]
		--label lbl_objectsToSave "Objects to Save:" align:#left
		dotNetControl dnc_meshesToSave "System.Windows.Forms.ListView" width:480 height:200 pos:[0,230] --align:#center offset:[0,-4]

		button btn_pickPath "..." align:#left height:18 width:btn_pickPath_width
		edittext edt_path align:#center
		button btn_explorePath "E" align:#right height:18 width:btn_explorePath_width tooltip:"Explore Output Folder..."

		groupBox grp_pathComponents "Path Components"
		label lbl_basePath "Base:"
		button btn_pickBasePath "..." across:3 align:#left height:18 width:btn_pickBasePath_width offset:[-5,0] tooltip:"Pick Base Output Folder..."
		edittext edt_basePath fieldwidth:435 offset:[-1,0] align:#center
		button btn_ExploreBaseFolder "E" align:#right height:18 width:btn_ExploreBaseFolder_width offset:[0,0] tooltip:"Explore Base Output Folder..."

		button btn_ProjectName "Proj" width:btn_ProjectName_width height:18 across:7 offset:[-5,0] tooltip:"Project Subfolder - Click to set standard symbolic paths..."
		edittext edt_ProjectName fieldwidth:98 offset:[1,0] align:#left
		button btn_versionNumber "Ver" width:btn_versionNumber_width  height:18 tooltip:"Version Subfolder - Click to set latest or increment new versions..."
		edittext edt_versionNumber fieldwidth:100 offset:[-1,0] align:#center
		button btn_FileBaseName "File" width:btn_fileBaseName_width height:18 tooltip:"File Name - Click to set standard symbolic names..."
		edittext edt_FileBaseName fieldwidth:100 offset:[-1,0] align:#left
		dropdownlist ddl_fileType width:75 align:#right
		button btn_ExploreOutputFolder "E" align:#right height:18 width:btn_exploreOutputFolder_width offset:[0,0] tooltip:"Explore Output Folder..." --images:#(twoArrowsIconBitmap,twoArrowsIconMask,2,1,1,2,2)

		subRollout sub_rollout width:480 height:200 align:#left offset:[0,-3] across:4
		subRollout sub_rollout2 width:480 height:200 align:#center offset:[0,-3]
		subRollout sub_rollout3 width:480 height:200 align:#right offset:[0,-3]
		subRollout sub_rollout4 width:480 height:200 align:#right offset:[0,-3]

		fn CompileComparer =
		(
			local source = ""
			source+="using System;\n"
			source+="using System.Windows.Forms;\n"
			source+="using System.Collections;\n"
			source+="public class ListViewItemComparer : IComparer\n"
			source+="{\n"
			source+="  private int col;\n"
			source+="  private int sort;\n"
			source+="  public ListViewItemComparer()\n"
			source+="  {\n"
			source+="      col = 0;\n"
			source+="      sort = 1;\n"
			source+="  }\n"
			source+="  public ListViewItemComparer(int column, int sorting)\n"
			source+="  {\n"
			source+="    sort = sorting;\n"
			source+="    col = column;\n"
			source+="  }\n"
			source+="  public int Compare(object x, object y)\n"
			source+="  {\n"
			source+="      if ((((ListViewItem)x).SubItems[col].Tag.GetType() == typeof(float))){\n"
			source+="      float compResult = (((float)((ListViewItem)x).SubItems[col].Tag) - ((float)((ListViewItem)y).SubItems[col].Tag));\n"
			source+="      if (compResult>0) return sort * 1; if (compResult < 0) return sort * -1; return 0;"
			source+="   } else \n"
			source+="   return ((String.Compare(((ListViewItem)x).SubItems[col].Text, ((ListViewItem)y).SubItems[col].Text))*sort);\n"
			source+="  }\n"
			source+="}\n"
			csharpProvider = dotnetobject "Microsoft.CSharp.CSharpCodeProvider"
			compilerParams = dotnetobject "System.CodeDom.Compiler.CompilerParameters"
			compilerParams.ReferencedAssemblies.AddRange #("System.dll", "System.Windows.Forms.dll")
			compilerParams.GenerateInMemory = true
			compilerResults = csharpProvider.CompileAssemblyFromSource compilerParams #(source)
			if compilerResults.Errors.Count == 0 then
			(
				compilerResults.CompiledAssembly
			)
			else
			(
				for i = 0 to compilerResults.Errors.Count-1 do
					format "%\n" compilerResults.Errors.Item[i].ErrorText
				false
			)
		)
		local theAssembly = CompileComparer()

		on dnc_sourceList ColumnClick args do
		(
			--if args.Column  > 0 do
			(
				if lastColumnClicked == args.Column then
					sortDirection *= -1
				else
					lastColumnClicked = args.Column
				dnc_sourceList.ListViewItemSorter = dotNetObject "ListViewItemComparer" args.Column sortDirection
			)
		)

		on dnc_meshesToSave ColumnClick args do
		(
			if lastColumnClicked == args.Column then
				sortDirection *= -1
			else
				lastColumnClicked = args.Column
			dnc_meshesToSave.ListViewItemSorter = dotNetObject "ListViewItemComparer" args.Column sortDirection
		)

		fn updateMoveButtonsEnabled =
		(
			btn_addToMeshLists.enabled = dnc_sourceList.SelectedItems.count > 0
			btn_removeFromMeshLists.enabled = dnc_meshesToSave.SelectedItems.count > 0
		)

		on dnc_sourceList SelectedIndexChanged do
		(
			updateMoveButtonsEnabled()
		)
		on dnc_meshesToSave SelectedIndexChanged do
		(
			updateMoveButtonsEnabled()
		)

		on btn_columnsIcon pressed do
		(
			popupmenu mnu_columnsResize mouse:mouse.screenpos
		)

		fn initList mode:#source =
		(
			local lv = if mode == #source then dnc_sourceList else dnc_meshesToSave
			fn getNameHeading mode objectSourceMode =
			(
				if mode == #source then
				(
					case objectSourceMode of
					(
						default:"Available Object"
						4:"Available Named Selection Set"
						5:"Available Layers"
						6:"Available Particle Flow System"
						7:"Available Particle Flow Event"
						8:"Available Thinking Particles System"
						9:"Available Thinking Particle Group"
						12: "Available 3ds Max Group"
						13: "Available 3ds Max Nested Group"
						14: "Previous Sessions"
					)
				)
				else
				(
					case objectSourceMode of
					(
						default:"Object to Save"
						4:"Named Selection Set to Save"
						5:"Layers to Save"
						6:"Particle Flow System to Save"
						7:"Particle Flow Event to Save"
						8:"Thinking Particles System to Save"
						9:"Thinking Particle Group to Save"
						12: "3ds Max Group to Save"
						13: "3ds Max Group to Nested Save"
						14: "Previous Sessions to Save"
					)
				)
			)
			local nameHeading = getNameHeading mode XMeshSaver_Settings.ObjectSourceMode
			local infolayout_def = case XMeshSaver_Settings.ObjectSourceMode of
			(
				default: #(#("#",40),#(nameHeading,198),#("Class",100),#("Faces",66),#("Vertices",66))
				4: #(#("#",40),#(nameHeading, 200), #("# Objects",70),#("# Geometry",70), #("Geometry To Save", 1000))
				5: #(#("#",40),#(nameHeading, 200), #("# Objects",70),#("# Geometry",70), #("Geometry To Save", 1000))

				6: #(#("#",40),#(nameHeading, 200))
				7: #(#("#",40),#(nameHeading, 400))
				8: #(#("#",40),#(nameHeading,200))
				9: #(#("#",40),#(nameHeading,298),#("Group Prts",66),#("Tree Prts",66))
				12: #(#("#",40),#(nameHeading, 200), #("# Objects",70),#("# Geometry",70), #("Geometry To Save", 1000))
				13: #(#("#",40),#(nameHeading, 200), #("# Objects",70),#("# Geometry",70), #("Geometry To Save", 1000))
				14: #(#("#",40), #(nameHeading, 130), #("Scene File", 100), #("Cnt",40),#("Geometry To Save", 1000))
			)
			lv.Clear()
			if mode == #source then
				lv.backColor = (dotNetClass "System.Drawing.Color").fromARGB 225 221 221
			else
				lv.backColor = (dotNetClass "System.Drawing.Color").fromARGB 221 225 221

			--lv.Sorting = lv.Sorting.None
			lv.View = (dotNetClass "System.Windows.Forms.View").Details
			lv.gridLines = true
			lv.fullRowSelect = true
			lv.checkboxes = false
			lv.hideSelection = false
			lv.ShowItemToolTips	= true
			lv.AllowDrop = true
			for i in infolayout_def do
				lv.Columns.add i[1]  i[2]
		)

		fn getFilterString =
		(
			if hasFilterString then
				edt_filterString.text
			else
				""
		)

		local theChildrenArray = #()
		fn RecurseChildren theRoot =
		(
			append theChildrenArray theRoot
			for o in theRoot.children do
			(
				RecurseChildren o
			)
		)

		fn collectChildrenRecursive theRoot =
		(
			theChildrenArray = #()
			RecurseChildren theRoot
			theChildrenArray
		)

		fn updateList lv theList showSublist:false =
		(
			local theRange = #()
			for i = 1 to theList.count do
			(
				local filterPass = true
				local theName = if XMeshSaver_Settings.ObjectSourceMode == 14 then
					theList[i][1]
				else
					try(theList[i].name)catch("???")

				if lv == dnc_sourceList do
					filterPass = matchPattern theName pattern:("*"+getFilterString()+"*")

				if filterPass do
				(
					case XMeshSaver_Settings.ObjectSourceMode of
					(
						default:
						(
							local li = dotNetObject "System.Windows.Forms.ListViewItem" (i as string)
							(li.SubItems.Item 0).tag = i as float
							li.Tag = i
							local subLi = li.SubItems.add theName
							subLi.tag = theName
							local subLi = li.SubItems.add (try((classof theList[i]) as string)catch("???"))
							subLi.tag = (try((classof theList[i]) as string)catch("???"))
							local theMeshCounts = try(GetTriMeshFaceCount theList[i])catch(#("???","???"))
							if isValidNode theList[i] and (classof theList[i].baseobject == PF_Source or classof theList[i].baseobject == ParticleGroup) do theMeshCounts = #("--","--")
							local theMeshCountFloats = for f in theMeshCounts collect ( if (f as float == undefined) then 0.0 else f as float )
							local subLi = li.SubItems.add (theMeshCounts[1] as string)
							subLi.tag = theMeshCountFloats[1]
							local subLi = li.SubItems.add (theMeshCounts[2] as string)
							subLi.tag = theMeshCountFloats[2]
							append theRange li
						)
						4: (
							local li = dotNetObject "System.Windows.Forms.ListViewItem" (i as string)
							li.Tag = i
							(li.SubItems.Item 0).tag = i as float

							local subLi = li.SubItems.add theName
							subLi.tag = theName

							subLi = li.SubItems.add (try(theList[i].count as string)catch("???"))
							subLi.tag = (try(theList[i].count)catch("???"))

							local geometryObjects = #()
							if not isDeleted theList[i] do
								geometryObjects = for o in theList[i] where ( isValidObj o and findItem GeometryClass.classes (classof o) > 0 and ( classof o ) != TargetObject) collect o
							subLi = li.SubItems.add (geometryObjects.count as string)
							subLi.tag = geometryObjects.count
							local theTooltip = "" as stringStream
							--if not isDeleted theList[i] do
							for j in geometryObjects do format "'%' " (try(j.name)catch("???")) to:theTooltip
							li.ToolTipText = theTooltip as string
							subLi = li.SubItems.add theTooltip
							subLi.tag = (try(theList[i].name as string)catch("???"))
							append theRange li
						)
						12: (
							local li = dotNetObject "System.Windows.Forms.ListViewItem" (i as string)
							li.Tag = i
							(li.SubItems.Item 0).tag = i as float

							local subLi = li.SubItems.add theName
							subLi.tag = theName

							subLi = li.SubItems.add (theList[i].children.count as string)
							subLi.tag = theList[i].children.count

							local geometryObjects = #()
							if not isDeleted theList[i] do
								geometryObjects = for o in theList[i].children where ( isValidObj o and findItem GeometryClass.classes (classof o) > 0 and ( classof o ) != TargetObject) collect o
							subLi = li.SubItems.add (geometryObjects.count as string)
							subLi.tag = geometryObjects.count
							local theTooltip = "" as stringStream
							for j in geometryObjects do format "'%' " (try(j.name)catch("???")) to:theTooltip
							li.ToolTipText = theTooltip as string
							subLi = li.SubItems.add theTooltip
							subLi.tag = (try(theList[i].name as string)catch("???"))
							append theRange li
						)
						13: (
							local li = dotNetObject "System.Windows.Forms.ListViewItem" (i as string)
							li.Tag = i
							(li.SubItems.Item 0).tag = i as float

							local subLi = li.SubItems.add theName
							subLi.tag = theName

							local theChildren = collectChildrenRecursive theList[i]

							subLi = li.SubItems.add (theChildren.count as string)
							subLi.tag = theChildren.count

							local geometryObjects = #()
							if not isDeleted theList[i] do
								geometryObjects = for o in theChildren where ( isValidObj o and findItem GeometryClass.classes (classof o) > 0 and ( classof o ) != TargetObject) collect o
							subLi = li.SubItems.add (geometryObjects.count as string)
							subLi.tag = geometryObjects.count
							local theTooltip = "" as stringStream
							for j in geometryObjects do format "'%' " (try(j.name)catch("???")) to:theTooltip
							li.ToolTipText = theTooltip as string
							subLi = li.SubItems.add theTooltip
							subLi.tag = (try(theList[i].name as string)catch("???"))
							append theRange li
						)
						14: (
							local li = dotNetObject "System.Windows.Forms.ListViewItem" (i as string)
							li.Tag = i
							(li.SubItems.Item 0).tag = i as float

							local subLi = li.SubItems.add theName
							subLi.tag = theName

							local subLi = li.SubItems.add theList[i][2]
							subLi.tag = theList[i][2]

							local theChildren = for obj in theList[i][3] where isValidNode obj collect obj

							subLi = li.SubItems.add (theChildren.count as string)
							subLi.tag = theChildren.count

							local theTooltip = "" as stringStream
							for j in theChildren do format "'%' " (try(j.name)catch("???")) to:theTooltip
							li.ToolTipText = theTooltip as string
							subLi = li.SubItems.add theTooltip
							subLi.tag = (try(theList[i].name as string)catch("???"))
							append theRange li
						)
						5: (
							local li = dotNetObject "System.Windows.Forms.ListViewItem" (i as string)
							li.Tag = i
							(li.SubItems.Item 0).tag = i as float

							local subLi = li.SubItems.add theName
							subLi.tag = theName

							local allLayerObjects = refs.dependentNodes (ILayerManager.getLayerObject theName)

							subLi = li.SubItems.add (try(allLayerObjects.count as string)catch("???"))
							subLi.tag = (try(theList[i].count)catch("???"))

							geometryObjects = for o in allLayerObjects where ( isValidObj o and ((findItem GeometryClass.classes (classof o) > 0 and findItem tabuGeometryClasses (classof o ) == 0) or classof o.baseobject == XRefObject  ))  collect o
							subLi = li.SubItems.add (geometryObjects.count as string)
							subLi.tag = geometryObjects.count
							local theTooltip = "" as stringStream
							for j in geometryObjects do format "'%' " (try(j.name)catch("???")) to:theTooltip
							li.ToolTipText = theTooltip as string
							subLi = li.SubItems.add theTooltip
							subLi.tag = (try(theList[i].name as string)catch("???"))
							append theRange li
						)
						9:
						(
							local li = dotNetObject "System.Windows.Forms.ListViewItem" (i as string)
							(li.SubItems.Item 0).tag = i as float
							li.Tag = i
							local subLi = li.SubItems.add theList[i][2]
							subLi.tag = theList[i][2]
							--local subLi = li.SubItems.add "TP Group"
							--subLi.tag = "TP Group"
							local theLocalCount = theList[i][1].GetParticleCount false
							local subLi = li.SubItems.add (theLocalCount as string)
							subLi.tag = theLocalCount
							local theTreeCount = theList[i][1].GetParticleCount true
							local subLi = li.SubItems.add (theTreeCount as string)
							subLi.tag = theTreeCount
							append theRange li
						)
					)
				)
			)
			lv.Items.AddRange theRange
		)

		fn AddRolloutToSubrollout theSubRollout theRollout rolledUp:true =
		(
			local theSubRollouts = #(XMesh_saveParams_rollout.sub_rollout, XMesh_saveParams_rollout.sub_rollout2, XMesh_saveParams_rollout.sub_rollout3, XMesh_saveParams_rollout.sub_rollout4)
			--make sure the rollout is not in another sub-rollout before adding it
			for s in theSubRollouts do
			(
				local theIndex = findItem s.rollouts theRollout
				if theIndex > 0 and s != theSubRollout do removeSubRollout s theRollout
			)--end s loop
			addSubRollout theSubRollout theRollout rolledup:rolledUp
		)

		fn resizeDialog size =
		(
			--return false
			if size.x < 480 do size.x = XMesh_saveParams_rollout.width = 480
			if size.y < 500 do size.y = XMesh_saveParams_rollout.height = 500

			local oldStateSaveChannelsRollout = not saveChannels_rollout.open
			local oldStateAdvancedRollout = not advancedSettings_rollout.open
			local oldStateDeadlineRollout = not deadlineParams_rollout.open
			local columnsHeights = #(10,10,10,10)

			columnsHeights[1] += if saveOptions_rollout.open then saveOptions_rollout.height+22 else 22

			if size.x <= 960 then
			(
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout saveChannels_rollout rolledup:oldStateSaveChannelsRollout
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout advancedSettings_rollout rolledup:oldStateAdvancedRollout
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout deadlineParams_rollout 	rolledup:oldStateDeadlineRollout

				columnsHeights[1] += if saveChannels_rollout.open then saveChannels_rollout.height+22 else 22
				columnsHeights[1] += if advancedSettings_rollout.open then advancedSettings_rollout.height+22 else 22
				columnsHeights[1] += if deadlineParams_rollout.open then deadlineParams_rollout.height+22 else 22
			)
			else if size.x <= 1440 then
			(
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout saveChannels_rollout rolledup:oldStateSaveChannelsRollout
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout2 advancedSettings_rollout rolledup:oldStateAdvancedRollout
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout2 deadlineParams_rollout rolledup:oldStateDeadlineRollout

				columnsHeights[1] += if saveChannels_rollout.open then saveChannels_rollout.height+22 else 22
				columnsHeights[2] += if advancedSettings_rollout.open then advancedSettings_rollout.height+22 else 22
				columnsHeights[2] += if deadlineParams_rollout.open then deadlineParams_rollout.height+22 else 22
			)
			else if size.x <= 1919  then
			(
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout saveChannels_rollout rolledup:oldStateSaveChannelsRollout
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout2 advancedSettings_rollout rolledup:oldStateAdvancedRollout
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout3 deadlineParams_rollout rolledup:oldStateDeadlineRollout

				columnsHeights[1] += if saveChannels_rollout.open then saveChannels_rollout.height+22 else 22
				columnsHeights[2] += if advancedSettings_rollout.open then advancedSettings_rollout.height+22 else 22
				columnsHeights[3] += if deadlineParams_rollout.open then deadlineParams_rollout.height+22 else 22
			)
			else
			(
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout2 saveChannels_rollout rolledup:oldStateSaveChannelsRollout
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout3 advancedSettings_rollout rolledup:oldStateAdvancedRollout
				AddRolloutToSubrollout XMesh_saveParams_rollout.sub_rollout4 deadlineParams_rollout rolledup:oldStateDeadlineRollout

				columnsHeights[2] += if saveChannels_rollout.open then saveChannels_rollout.height+22 else 22
				columnsHeights[3] += if advancedSettings_rollout.open then advancedSettings_rollout.height+22 else 22
				columnsHeights[4] += if deadlineParams_rollout.open then deadlineParams_rollout.height+22 else 22
			)

			btn_updateLists.pos = [ddl_objectSourceMode.pos.x+ddl_objectSourceMode.width,2]
			btn_addToMeshLists.pos = btn_updateLists.pos + [btn_updateLists_width+12,0]
			btn_removeFromMeshLists.pos = btn_addToMeshLists.pos +[btn_addToMeshLists_width,0]
			edt_filterString.width = size.x-(btn_removeFromMeshLists.pos.x+btn_removeFromMeshLists_width+12+3+21)
			edt_filterString.pos  = [size.x-3-21-edt_filterString.width,2]
			btn_columnsIcon.pos = [size.x-23,2]

			local theHeadNeedsHeight = 25
			local thePathNeedsHeight = 90
			local theSourceNeedsHeight = 155
			local theOptionsWantHeight = amax columnsHeights

			local theSourceHeight = amax theSourceNeedsHeight (size.y - theHeadNeedsHeight - theOptionsWantHeight - thePathNeedsHeight)

			local theOptionsHeight = size.y - theHeadNeedsHeight - theSourceHeight - thePathNeedsHeight
			sub_rollout4.height = sub_rollout3.height  = sub_rollout2.height  = sub_rollout.height = theOptionsHeight
			sub_rollout.pos = [0,size.y-theOptionsHeight]
			sub_rollout2.pos = [480,size.y-theOptionsHeight]
			sub_rollout3.pos = [960,size.y-theOptionsHeight]
			sub_rollout4.pos = [1440,size.y-theOptionsHeight]

			dnc_sourceList.pos = [3,26]
			case XMeshSaver_Settings.ObjectSourceMode of
			(
				1:
				(
					dnc_meshesToSave.height = theSourceHeight
					dnc_meshesToSave.pos = [3,26]
					dnc_sourceList.height = 0
					btn_removeFromMeshLists.visible = btn_addToMeshLists.visible = false
					edt_filterString.pos = [0,-100] --this is a hack to "hide" the filter by moving it outside of the visible UI as it does not replaint correctly
				)
				default:
				(
					dnc_sourceList.height = dnc_meshesToSave.height = (theSourceHeight-2)/2
					dnc_meshesToSave.pos = [3,theHeadNeedsHeight+theSourceHeight-dnc_meshesToSave.height]
					btn_removeFromMeshLists.visible = btn_addToMeshLists.visible = true
				)
			)

			dnc_meshesToSave.width = dnc_sourceList.width = size.x-5

			local rowY = theHeadNeedsHeight + theSourceHeight + 4
			btn_pickPath.pos = [3,rowY]
			edt_path.pos = btn_pickPath.pos + [btn_pickPath_width,0]
			edt_path.width = size.x - 5 - btn_pickPath_width - btn_explorePath_width
			btn_explorePath.pos = [size.x - 2 - btn_explorePath_width,rowY]

			rowY += 22
			grp_pathComponents.pos = [1,rowY]
			grp_pathComponents.height = theHeadNeedsHeight + theSourceHeight + thePathNeedsHeight - rowY - 2
			grp_pathComponents.width = size.x-2

			rowY += 18
			lbl_basePath.pos = [10,rowY+2]
			btn_pickBasePath.pos = [38,rowY]
			edt_basePath.pos = btn_pickBasePath.pos + [btn_pickBasePath_width,0]
			btn_ExploreBaseFolder.pos = [size.x-7-btn_ExploreBaseFolder_width,rowY]
			edt_basePath.width = btn_ExploreBaseFolder.pos.x - btn_pickBasePath.pos.x - btn_pickBasePath_width

			rowY += 20
			local rowFixedWidth = btn_projectName_width + btn_versionNumber_width + btn_fileBaseName_width + ddl_fileType.width + btn_exploreOutputFolder_width
			local rowFreeWidth = size.x - 26 - rowFixedWidth
			btn_ProjectName.pos = [7,rowY]
			edt_projectName.pos = btn_projectName.pos + [btn_projectName_width,0]
			edt_ProjectName.width = edt_FileBaseName.width = 2 * rowFreeWidth / 5

			btn_versionNumber.pos = edt_ProjectName.pos + [edt_projectName.width+6,0]
			edt_versionNumber.width = rowFreeWidth - edt_projectName.width - edt_fileBaseName.width
			edt_versionNumber.pos = btn_versionNumber.pos + [btn_versionNumber_width,0]

			btn_FileBaseName.pos = edt_versionNumber.pos + [edt_versionNumber.width+6,0]
			edt_FileBaseName.pos = btn_fileBaseName.pos + [btn_fileBaseName_width,0]

			ddl_fileType.pos = edt_fileBaseName.pos + [edt_fileBaseName.width,-2]

			btn_exploreOutputFolder.pos = [ddl_fileType.pos.x + ddl_fileType.width,rowY]

			saveOptions_rollout.resizeDialog size
		)


		fn updateAllLists reload:true=
		(
			btn_updateLists.tooltip = case XMeshSaver_Settings.ObjectSourceMode of
			(
				1:"Reload list from selected scene objects"
				default:"Reset both lists to defaults - clear the bottom (objects to save) list and display all available objects according to the current mode in the top (sources) list."
			)

			if reload do
			case XMeshSaver_Settings.ObjectSourceMode of
			(
				1:
				(
					sourceList = #()
					toSaveList = collectAllGeometry selOnly:true
					dnc_sourceList.visible = false
				)
				2:
				(
					toSaveList = #()
					sourceList = collectAllGeometry()
					dnc_sourceList.visible = true
				)
				3:
				(
					toSaveList = #()
					sourceList = for o in collectAllGeometry() where not o.isHiddenInVpt collect o
					dnc_sourceList.visible = true
				)
				4: --NSS
				(
					toSaveList = #()
					sourceList = for o in SelectionSets collect o
					dnc_sourceList.visible = true
				)
				5: --Layers
				(
					toSaveList = #()
					sourceList = for i = 0 to layerManager.count-1 collect layerManager.getLayer i
					dnc_sourceList.visible = true
				)
				6:
				(
					toSaveList = #()
					sourceList = for o in objects where classof o.baseobject == PF_Source collect o
					dnc_sourceList.visible = true
				)
				7:
				(
					toSaveList = #()
					sourceList = for o in objects where classof o.baseobject == ParticleGroup collect o
					dnc_sourceList.visible = true
				)
				8:
				(
					toSaveList = #()
					sourceList = for o in objects where classof o.baseobject == Thinking collect o
					dnc_sourceList.visible = true
				)
				9:
				(
					toSaveList = #()
					TPGroupsList = #()
					for o in objects where classof o.baseobject == Thinking do
					(
						local theBaseName = o.name
						local theGM = o.GroupManager
						collectTPGroups theGM theBaseName o
					)
					sourceList = deepCopy TPGroupsList
					dnc_sourceList.visible = true
				)
				10:
				(
					toSaveList = #()
					sourceList = for o in objects where classof o.baseobject == Frost collect o
					dnc_sourceList.visible = true
				)
				11:
				(
					toSaveList = #()
					sourceList = for o in objects where classof o.baseobject == XMeshLoader collect o
					dnc_sourceList.visible = true
				)
				12: --Groups
				(
					toSaveList = #()
					sourceList = for o in objects where isGroupHead o collect o
					dnc_sourceList.visible = true
				)
				13: --Groups
				(
					toSaveList = #()
					sourceList = for o in objects where isGroupHead o collect o
					dnc_sourceList.visible = true
				)
				14: --Last Session
				(
					sourceList = for i = ::XMeshSaverTools_LastProcessedObjects.count to 1 by -1 collect ::XMeshSaverTools_LastProcessedObjects[i]
					toSaveList =  #()
					dnc_sourceList.visible = true
				)
				default:
				(
					sourceList = #()
					toSaveList = #()
					dnc_sourceList.visible = true
				)
			)

			resizeDialog (getDialogSize	XMesh_saveParams_rollout)
			initList mode:#source
			initList mode:#meshes
			updateList dnc_sourceList sourceList
			updateList dnc_meshesToSave toSaveList
		)

		fn getListViewSelection lv =	--returns an array of the selected ListView items
		(
			try
				sort (for i = 1 to lv.items.count where lv.items.item[i-1].Selected and lv.items.item[i-1].Tag > 0 collect lv.items.item[i-1].Tag)
			catch
				#()
		)

		fn addToMeshList =
		(
			local theSel = getListViewSelection dnc_sourceList
			for i = theSel.count to 1 by -1 do
			(
				append toSaveList sourceList[theSel[i]]
				deleteItem sourceList theSel[i]
			)
			initList mode:#source
			initList mode:#meshes
			updateList dnc_sourceList sourceList
			updateList dnc_meshesToSave toSaveList
			updateMoveButtonsEnabled()
			saveOptions_rollout.canSaveCheck()
		)

		fn removeFromMeshLists =
		(
			local theSel = getListViewSelection dnc_meshesToSave
			for i = theSel.count to 1 by -1 do
			(
				append sourceList toSaveList[theSel[i]]
				deleteItem toSaveList theSel[i]
			)
			initList mode:#source
			initList mode:#meshes
			updateList dnc_sourceList sourceList
			updateList dnc_meshesToSave toSaveList --showSublist:true
			updateMoveButtonsEnabled()
			saveOptions_rollout.canSaveCheck()
		)

		fn showPromptFilterString =
		(
			edt_filterString.font = dotNetObject "System.Drawing.Font" edt_filterString.font (dotNetClass "System.Drawing.FontStyle").Italic
			edt_filterString.text = "Search by name"
			hasFilterString = false
		)
		fn showEmptyFilterString =
		(
			edt_filterString.font = dotNetObject "System.Drawing.Font" edt_filterString.font (dotNetClass "System.Drawing.FontStyle").Regular
			edt_filterString.text = ""
			hasFilterString = false
		)
		fn toDotNetColor c =
		(
			local dnColor = dotNetClass "System.Drawing.Color"
			dnColor.fromARGB (255*c[1]) (255*c[2]) (255*c[3])
		)

		on edt_filterString GotFocus e do
		(
			if not hasFilterString do
				showEmptyFilterString()
			userChangingFilterString = true
		)

		on edt_filterString LostFocus e do
		(
			userChangingFilterString = false
			if not hasFilterString do
				showPromptFilterString()
		)

		on edt_filterString TextChanged e do
		(
			if userChangingFilterString do
				hasFilterString = (edt_filterString.Text.count != 0)
			initList mode:#source
			updateList dnc_sourceList sourceList
		)

		on dnc_sourceList DoubleClick EventArgs do
		(
			addToMeshList()
		)

		on dnc_sourceList MouseUp args do
		(
			if args.Button == (DotNetClass "System.Windows.Forms.MouseButtons").Right do
				popupmenu SourceList_Menu pos:mouse.screenpos
		)

		fn OnItemDrag mode arg =
		(
			local dropTarget = (if mode==#source then dnc_meshesToSave else dnc_sourceList)
			dropTarget.DoDragDrop arg.item (dotNetClass "System.Windows.Forms.DragDropEffects").Move
		)

		fn OnDragOver mode sender arg =
		(
			local dragSource = (if mode==#source then dnc_meshesToSave else dnc_sourceList)
			if arg.data.GetDataPresent(dotNetClass "System.Windows.Forms.ListViewItem") do
			(
				local item = arg.data.GetData(dotNetClass "System.Windows.Forms.ListViewItem")
				if item.listView.equals dragSource then
					arg.effect = (dotNetClass "System.Windows.Forms.DragDropEffects").Move
				else
					arg.effect = (dotNetClass "System.Windows.Forms.DragDropEffects").None
			)
		)

		fn OnDragDrop mode sender arg =
		(
			local dragSource = (if mode==#source then dnc_meshesToSave else dnc_sourceList)
			if arg.data.GetDataPresent(dotNetClass "System.Windows.Forms.ListViewItem") do
			(
				local item = arg.data.GetData(dotNetClass "System.Windows.Forms.ListViewItem")
				if item.listView.equals dragSource do
				(
					if mode == #source then
						removeFromMeshLists()
					else
						addToMeshList()
				)
			)
		)

		on dnc_sourceList ItemDrag arg do ( OnItemDrag #source arg )
		on dnc_sourceList DragOver sender arg do ( OnDragOver #source sender arg )
		on dnc_sourceList DragDrop sender arg do ( OnDragDrop #source sender arg )

		on dnc_meshesToSave ItemDrag arg do ( OnItemDrag #meshes arg )
		on dnc_meshesToSave DragOver sender arg do ( OnDragOver #meshes sender arg )
		on dnc_meshesToSave DragDrop sender arg do ( OnDragDrop #meshes sender arg )

		on btn_addToMeshLists pressed do
		(
			addToMeshList()
		)

		on btn_removeFromMeshLists pressed do
		(
			removeFromMeshLists()
		)

		on dnc_meshesToSave DoubleClick EventArgs do
		(
			removeFromMeshLists()
		)

		on dnc_meshesToSave MouseUp args do
		(
			if args.Button == (DotNetClass "System.Windows.Forms.MouseButtons").Right do
				popupmenu SaveList_Menu pos:mouse.screenpos
		)

		fn updatePathFromSettings =
		(
			local filePath = buildCacheFileNamePreSubst()
			local theFilePath = getFilenamePath filePath
			local theFileName = getFilenameFile filePath
			local theFileType = getFilenameType filePath

			if XMeshSaver_Settings.saveMode != 1 then
			(
				local theMeshName = "$object"
				filePath = if XMeshSaver_Settings.folderPerObject then
					(theFilePath + theMeshName + "\\" + theFileName + "_" + theMeshName  + "_" + theFileType )
				else
					(theFilePath + theFileName + "_" + theMeshName + "_" + theFileType )
			)
			edt_path.text = filePath
		)

		fn maybeSetBasePathFromUser thePath =
		(
			result = false
			if thePath != "" and not (doesFileExist thePath) do
			(
				local doCreate = queryBox (thePath + "\nPath does not exist.\nCreate it now?") title:"XMesh Saver" beep:false
				if doCreate do
					makeDir thePath all:true
			)
			if doesFileExist thePath then
			(
				XMeshSaver_Settings.baseCachePath = thePath
				setIniSetting theIniFile "SavePath" "BaseCachePath" XMeshSaver_Settings.baseCachePath
				result = true
			)
			else
			(
				thePath = XMeshSaver_Settings.baseCachePath
				setIniSetting theIniFile "SavePath" "BaseCachePath" XMeshSaver_Settings.baseCachePath
			)
			edt_basePath.text = thePath
			updatePathFromSettings()
			saveOptions_rollout.canSaveCheck()
			result
		)

		fn exploreOutputFolder =
		(
			local thePath = (getFileNamePath (buildCacheFileName()))
			if doesFileExist thePath then
				shellLaunch "explorer.exe" thePath
			else
				messageBox ("The output folder does not exist:\n" + (thePath as String) + "\n\nIt will be created automatically when you save.") title:"XMesh Saver"
		)

		fn onUserEnteredPath newPath =
		(
			local pathComponents = getPathComponents newPath numobjects:""
			--XMesh_saveParams_rollout.edt_basePath.text = XMeshSaver_Settings.baseCachePath = pathComponents[1]
			local doUpdate = maybeSetBasePathFromUser pathComponents[1]
			if doUpdate do
			(
				XMesh_saveParams_rollout.edt_ProjectName.text = XMeshSaver_Settings.projectName = pathComponents[2]
				XMesh_saveParams_rollout.edt_versionNumber.text = XMeshSaver_Settings.versionNumber = pathComponents[3]
				XMesh_saveParams_rollout.edt_FileBaseName.text = XMeshSaver_Settings.fileBaseName = pathComponents[4]
				if pathComponents[5] != "" do
				(
					local i = findMeshFileType pathComponents[5] (if pathComponents[5] == XMeshSaver_Settings.fileType then XMeshSaver_Settings.CoordinateSystem else "")
					if i == 0 do
						i = 1
					if i > 0 do
					(
						XMesh_saveParams_rollout.ddl_fileType.selection = i
						XMeshSaver_Settings.fileType = meshFileTypes[i][1]
						XMeshSaver_Settings.CoordinateSystem = meshFileTypes[i][2]
					)
				)
				setIniSetting theIniFile "SavePath" "ProjectName" XMeshSaver_Settings.projectName
				setIniSetting theIniFile "SavePath" "EnableVersionNumber" ((XMeshSaver_Settings.versionNumber.count != 0) as String)
				setIniSetting theIniFile "SavePath" "FileBaseName" XMeshSaver_Settings.fileBaseName
				setIniSetting theIniFile "SavePath" "FileType" XMeshSaver_Settings.fileType
				setIniSetting theIniFile "Settings" "CoordinateSystem" (XMeshSaver_Settings.CoordinateSystem as String)
			)
			updatePathFromSettings()
		)

		on btn_pickPath pressed do
		(
			local numObjects = ""--(GetMeshesToSave()).count
			local oldPath = buildCacheFileName numobjects:numObjects
			if not doesFileExist (getFileNamePath oldPath) then
				oldPath = "$exports\\" + (getFileNameFile oldPath) + (getFileNameType oldPath)
				--oldPath = getFileNameFile oldPath + getFileNameType oldPath

			newPath = getSaveFileName caption:"Select Save Location for Meshes" filename:oldPath types:"Thinkbox XMesh Files (*.xmesh)|*.xmesh|Wavefront OBJ Files (*.obj)|*.obj|" historyCategory:"XMesh"
			if newPath != undefined do
			(
				onUserEnteredPath newPath
			)
		)

		on edt_path entered val do
		(
			onUserEnteredPath val
		)

		on btn_explorePath pressed do
		(
			exploreOutputFolder()
		)

		on btn_pickBasePath pressed do
		(
			local newPath = getSavePath initialDir:XMeshSaver_Settings.baseCachePath
			if newPath != undefined do
			(
				setBaseCachePath newPath
				edt_basePath.text = XMeshSaver_Settings.baseCachePath = newPath
				XMesh_saveParams_rollout.updatePathFromSettings()
				setIniSetting theIniFile "SavePath" "BaseCachePath" newPath
			)
			saveOptions_rollout.canSaveCheck()
		)

		on edt_basePath changed txt do
		(
			if doesFileExist txt then
			(
				XMeshSaver_Settings.baseCachePath = txt
			)
			saveOptions_rollout.canSaveCheck()
		)
		on edt_basePath entered txt do
		(
			maybeSetBasePathFromUser txt
		)
		on btn_ExploreBaseFolder pressed do
		(
			local thePath = XMeshSaver_Settings.baseCachePath
			if doesFileExist thePath then
				shellLaunch "explorer.exe" thePath
			else
				messageBox ("The base folder does not exist:\n" + (thePath as String) + "\n\nIt will be created automatically when you save.") title:"XMesh Saver"
		)

		on btn_ProjectName pressed do
		(
			popupmenu ProjectName_Menu pos:mouse.screenpos
		)
		on edt_ProjectName entered txt do
		(
			XMeshSaver_Settings.ProjectName = txt
			updatePathFromSettings()
			setIniSetting theIniFile "SavePath" "ProjectName" txt
		)
		on btn_versionNumber pressed do
		(
			popupmenu VersionNumber_Menu pos:mouse.screenpos
		)
		on edt_versionNumber entered txt do
		(
			if txt.count != 0 and not isVersionString txt do
				txt = getNextVersion next:#version
			edt_versionNumber.text = txt
			XMeshSaver_Settings.versionNumber = txt
			updatePathFromSettings()
			setIniSetting theIniFile "SavePath" "EnableVersionNumber" ((txt.count != 0) as String)
		)
		on btn_FileBaseName pressed do
		(
			popupmenu FileBaseName_Menu pos:mouse.screenpos
		)
		on edt_FileBaseName changed val do
		(
			XMeshSaver_Settings.FileBaseName = val
			saveOptions_rollout.canSaveCheck()
		)
		on edt_FileBaseName entered txt do
		(
			XMeshSaver_Settings.FileBaseName = txt
			updatePathFromSettings()
			setIniSetting theIniFile "SavePath" "FileBaseName" txt
			saveOptions_rollout.canSaveCheck()
		)

		on ddl_fileType selected itm do
		(
			XMeshSaver_Settings.FileType = meshFileTypes[itm][1]
			XMeshSaver_Settings.CoordinateSystem = meshFileTypes[itm][2]
			ddl_fileType.tooltip = meshFileTypes[itm][4]
			updatePathFromSettings()
			setIniSetting theIniFile "SavePath" "FileType" meshFileTypes[itm][1]
			setIniSetting theIniFile "Settings" "CoordinateSystem" meshFileTypes[itm][2]
		)


		on btn_ExploreOutputFolder pressed do
		(
			exploreOutputFolder()
		)

		on ddl_objectSourceMode selected itm do
		(
			setIniSetting theIniFile "Settings" "ObjectSourceMode" ddl_objectSourceMode.selected
			XMeshSaver_Settings.ObjectSourceMode = ddl_objectSourceMode.selection
			updateAllLists()
			updateMoveButtonsEnabled()
			saveOptions_rollout.canSaveCheck()
		)
		on btn_updateLists pressed do
		(
			updateAllLists()
			updateMoveButtonsEnabled()
			saveOptions_rollout.canSaveCheck()
		)

		on XMesh_saveParams_rollout resized size do
		(
			resizeDialog size
			setIniSetting theIniFile "Dialog" "Size" (size as String)
		)

		on XMesh_saveParams_rollout moved position do
		(
			if XMesh_saveParams_rollout.placement != #minimized do
			(
				setIniSetting theIniFile "Dialog" "Position" (position as String)
			)
		)



		on XMesh_saveParams_rollout open do
		(
			local theVal = getIniSetting theIniFile "Settings" "ObjectSourceMode"
			if theVal != "" do
			(
				local theIndex = findItem ddl_objectSourceMode.items theVal
				if theIndex == 0 do theIndex = 1
				ddl_objectSourceMode.selection = XMeshSaver_Settings.ObjectSourceMode = theIndex
			)
			hasFilterString = userChangingFilterString = false
			edt_filterString.foreColor = toDotNetColor (colorMan.getColor #windowText)
			edt_filterString.backColor = toDotNetColor (colorMan.getColor #window)
			edt_filterString.height = 2

			ddl_fileType.items = for i in meshFileTypes collect i[3]
			--ddl_fileType.tooltip = meshFileTypes[ddl_fileType.selection][4]

			showPromptFilterString()
			resizeDialog (getDialogSize XMesh_saveParams_rollout)
			updateAllLists()
			updateMoveButtonsEnabled()
		)
	)

	rollout saveOptions_rollout "Save Options" category:10
	(
		local theValidSteps = #(1)

		dropdownlist ddl_saveMode width:462 align:#center items:#("Save ALL Objects To ONE MESH In WORLD SPACE Coordinates","Save EACH Object To INDIVIDUAL MESH In WORLD SPACE Coordinates","Save EACH Object To INDIVIDUAL MESH in OBJECT SPACE Coordinates")  offset:[1,-3]

		checkbutton chk_folderPerObject ">Make Sub-Folder Per Object" align:#left width:153 offset:[-10,-2] across:3
		checkbutton chk_createMaterialLibrary ">Save Material Library" align:#center width:153 offset:[0,-2]
		checkbutton chk_createXMeshLoader ">Create XMesh Loaders" align:#right width:153 offset:[10,-2]

		group "Frame Range To Save" (
			spinner spn_frameStart range:[-100000,100000,XMeshSaver_Settings.frameStart] fieldwidth:50 offset:[-6,-2] type:#integer align:#left across:5
			spinner spn_frameEnd "To " range:[-100000,100000,XMeshSaver_Settings.frameEnd] fieldwidth:50 offset:[-30,-2] type:#integer align:#center
			dropdownlist ddl_frameStep items:#("1 Sample") width:130 align:#center offset:[-10,-5] height:20
			button btn_getSceneRange "Set to Range" align:#right offset:[12,-5] width:80 tooltip:"Set the From and To Frames to the Current Scene Range"
			button btn_getCurrentFrame "Set to Frame" align:#right offset:[6,-5] width:80 tooltip:"Set the From and To Frames to the Current Frame Value to Save a Single Frame"
		)

		group "Proxy Sequence"
		(
			checkbutton chk_saveProxy ">Save Proxy Sequence" across:3 width:150 offset:[-5,-3]
			dropdownlist ddl_applyOptimize items:#("DON'T Optimize Proxies","Add OPTIMIZE Modifier","Add MULTIRES Modifier","Add PROOPTIMIZER Modifier") offset:[-2,-3] width:162
			spinner spn_vertexPercent "Vertex %:" range:[0.0,100.0,10.0]  fieldwidth:50 offset:[0,-1]

			checkbutton chk_useProxySampling ">Use Proxy Sampling" width:150 offset:[-5,-3] across:3
			dropdownlist ddl_proxyFrameStep items:#("1 Sample") width:162 offset:[-2,-3] height:20
			spinner spn_MinVertices "Min. Vertices:" range:[0,10000000,1000] fieldwidth:50 type:#integer offset:[-1,-1]

		)
		button btn_saveMeshes "Save Meshes Locally..." width:460 height:45 offset:[0,0]

		fn resizeDialog size =
		(
			saveOptions_rollout.width = size.x
		)

		fn canSaveCheck =
		(
			local theCount = toSaveList.count
			if XMeshSaver_Settings.ObjectSourceMode == 4 do
			(
				local theArray = #()
				for o in toSaveList where not isDeleted o do
					for j in o where ( findItem GeometryClass.classes (classof j) > 0 and findItem tabuGeometryClasses (classof j ) == 0 ) or classof j.baseobject == XRefObject  do appendIfUnique theArray j
				theCount = theArray.count
			)
			if XMeshSaver_Settings.ObjectSourceMode == 5 do
			(
				local theArray = #()
				for o in toSaveList do
					for j in (refs.dependentNodes (ILayerManager.getLayerObject o.name) ) where ( findItem GeometryClass.classes (classof j) > 0 and findItem tabuGeometryClasses (classof j ) == 0 ) or classof j.baseobject == XRefObject do appendIfUnique theArray j
				theCount = theArray.count
			)
			if XMeshSaver_Settings.ObjectSourceMode >= 12 do
			(
				local theArray = GetMeshesToSave()
				theCount = theArray.count
			)

			if not doesFileExist XMesh_saveParams_rollout.edt_basePath.text then
			(
				btn_saveMeshes.caption = "PLEASE SPECIFY A VALID BASE PATH TO SAVE TO..."
				deadlineParams_rollout.btn_submit.caption = "PLEASE SPECIFY A VALID BASE PATH TO SAVE TO VIA DEADLINE..."
				btn_saveMeshes.enabled = deadlineParams_rollout.btn_submit.enabled = false
			)
			else if XMeshSaver_Settings.fileBaseName.count == 0 then
			(
				btn_saveMeshes.caption = "PLEASE SPECIFY A VALID FILE NAME TO SAVE TO..."
				deadlineParams_rollout.btn_submit.caption = "PLEASE SPECIFY A VALID FILE NAME TO SAVE TO VIA DEADLINE..."
				btn_saveMeshes.enabled = deadlineParams_rollout.btn_submit.enabled = false
			)
			else if toSaveList.count == 0 then
			(
				btn_saveMeshes.caption = "PLEASE SPECIFY AT LEAST ONE OBJECT TO SAVE..."
				deadlineParams_rollout.btn_submit.caption = "PLEASE SPECIFY AT LEAST ONE OBJECT TO SUBMIT TO DEADLINE..."
				btn_saveMeshes.enabled = deadlineParams_rollout.btn_submit.enabled = false
			)
			else
			(
				local theMode = case XMeshSaver_Settings.SaveMode of
				(
					1: "AS ONE MESH IN WORLD SPACE"
					2: ((if theCount > 1 then "INDIVIDUALLY " else "") + "IN WORLD SPACE")
					3: ((if theCount > 1 then "INDIVIDUALLY " else "") + "IN OBJECT SPACE")
				)

				local theObjWord = case XMeshSaver_Settings.ObjectSourceMode of
				(
					default: "OBJECT"
					6: "PF SYSTEM"
					7: "PF EVENT"
					8: "TP SYSTEM"
					9: "TP GROUP"
					10: "FROST OBJECT"
					11: "XMESH OBJECT"
					12: "GROUP OBJECT"
					13: "NESTED GROUP OBJECT"
					14: "PREVIOUS SESSION OBJECT"
				)
				local theObjString = (if theCount == 1 then " "+theObjWord+" " else " "+theObjWord+"S ")
				local theLayerString = ""
				if XMeshSaver_Settings.ObjectSourceMode == 5 do
					theLayerString	 = " FROM " + toSaveList.count as string + " LAYER" + (if toSaveList.count == 1 then " " else "S ")

				local theFolderString = ""
				if XMeshSaver_Settings.SaveMode == 3 and XMeshSaver_Settings.folderPerObject do theFolderString = " TO " + (if theCount == 1 then "A SUB-FOLDER" else "SUB-FOLDERS")
				btn_saveMeshes.caption = "SAVE "+ theCount as string + theObjString +theLayerString	+ theMode + theFolderString + "..."
				btn_saveMeshes.enabled = deadlineParams_rollout.btn_submit.enabled = true
				deadlineParams_rollout.btn_submit.caption = "PROCESS "+ theCount as string +theObjString + theMode + " ON DEADLINE..."
			)

			chk_folderPerObject.enabled = XMeshSaver_Settings.saveMode > 1

			if XMeshSaver_Settings.ObjectSourceMode == 8 and XMeshSaver_Settings.SaveMode == 1 and theCount > 1 do
			(
				btn_saveMeshes.enabled = deadlineParams_rollout.btn_submit.enabled = false
				btn_saveMeshes.caption = deadlineParams_rollout.btn_submit.caption = "CANNOT SAVE MULTIPLE TP SYSTEMS AS ONE OBJECT, PLEASE SWITCH MODE!"
			)
			if XMeshSaver_Settings.ObjectSourceMode == 9 and XMeshSaver_Settings.SaveMode == 1 and theCount > 1 do
			(
				local theTPs = #()
				for o in toSaveList do appendIfUnique theTPs o[3]
				if theTPs.count > 1 do
				(
					btn_saveMeshes.enabled = deadlineParams_rollout.btn_submit.enabled = false
					btn_saveMeshes.caption = deadlineParams_rollout.btn_submit.caption = "CANNOT SAVE GROUPS FROM MULTIPLE TP SYSTEMS AS ONE OBJECT, PLEASE SWITCH MODE!"
				)
			)
			if XMeshSaver_Settings.ObjectSourceMode == 14 and theCount == 0 do
			(
				btn_saveMeshes.enabled = deadlineParams_rollout.btn_submit.enabled = false
				btn_saveMeshes.caption = deadlineParams_rollout.btn_submit.caption = "CANNOT SAVE PREVIEWS SESSION IF ALL OBJECTS ARE MISSING!"
			)
		)


		fn loadSettings fromDisk:true =
		(
			local theVal

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "SaveMode")
				if theVal == OK do theVal = 1
				XMeshSaver_Settings.saveMode = theVal
			)
			ddl_saveMode.selection = XMeshSaver_Settings.saveMode

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "FolderPerObject")
				if theVal == OK do theVal = true
				XMeshSaver_Settings.folderPerObject = theVal
			)
			chk_folderPerObject.checked = XMeshSaver_Settings.folderPerObject

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "CreateMaterialLibrary")
				if theVal == OK do theVal = true
				XMeshSaver_Settings.createMaterialLibrary = theVal
			)
			chk_createMaterialLibrary.checked = XMeshSaver_Settings.createMaterialLibrary

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "CreateXMeshLoader")
				if theVal == OK do theVal = true
				XMeshSaver_Settings.createXMeshLoaders = theVal
			)
			chk_createXMeshLoader.checked = XMeshSaver_Settings.createXMeshLoaders

			if fromDisk do
			(
				theVal = getIniSetting theIniFile "SavePath" "BaseCachePath"
				if theVal == "" do theVal = getDir #Export
				XMeshSaver_Settings.baseCachePath = theVal

				local valExists
				valExists = hasIniSetting theIniFile "SavePath" "ProjectName"
				if valExists do XMeshSaver_Settings.projectName = (getIniSetting theIniFile "SavePath" "ProjectName")

				theVal = execute (getIniSetting theIniFile "SavePath" "EnableVersionNumber")
				if theVal == OK do theVal = true
				if theVal then
					XMeshSaver_Settings.versionNumber = getNextVersion()
				else
					XMeshSaver_Settings.versionNumber = ""

				theVal = getIniSetting theIniFile "SavePath" "FileBaseName"
				if theVal != "" do XMeshSaver_Settings.fileBaseName  = theVal
			)

			XMesh_saveParams_rollout.edt_basePath.text = XMeshSaver_Settings.baseCachePath
			XMesh_saveParams_rollout.edt_projectName.text = XMeshSaver_Settings.projectName
			XMesh_saveParams_rollout.edt_versionNumber.text = XMeshSaver_Settings.versionNumber
			XMesh_saveParams_rollout.edt_FileBaseName.text = XMeshSaver_Settings.fileBaseName

			local theFileType =	XMeshSaver_Settings.FileType
			if fromDisk do theFileType = getIniSetting theIniFile "SavePath" "FileType"
			if theFileType != "" do
			(
				local theCoordinateSystem = XMeshSaver_Settings.CoordinateSystem
				if fromDisk do theCoordinateSystem = getIniSetting theIniFile "Settings" "CoordinateSystem"
				local theIndex = findMeshFileType theFileType theCoordinateSystem
				if theIndex > 0 do
				(
					XMesh_saveParams_rollout.ddl_fileType.selection = theIndex
					XMesh_saveParams_rollout.ddl_fileType.tooltip = meshFileTypes[theIndex][4]
					XMeshSaver_Settings.fileType = meshFileTypes[theIndex][1]
					XMeshSaver_Settings.CoordinateSystem = meshFileTypes[theIndex][2]
				)
			)

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "FrameStep")
				if theVal == OK do theVal = 1.0
				XMeshSaver_Settings.frameStep  = theVal

				theVal = execute (getIniSetting theIniFile "Settings" "ProxyFrameStep")
				if theVal == OK do theVal = 1.0
				XMeshSaver_Settings.ProxyFrameStep  = theVal
			)

			XMesh_saveParams_rollout.updatePathFromSettings()

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "SaveProxy")
				if theVal == OK do theVal = false
				XMeshSaver_Settings.saveProxy = theVal
			)
			chk_saveProxy.checked = XMeshSaver_Settings.saveProxy

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "UseProxyFrameStep")
				if theVal == OK do theVal = false
				XMeshSaver_Settings.useProxyFrameStep = theVal
			)
			chk_useProxySampling.checked = XMeshSaver_Settings.useProxyFrameStep

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "AutoOptimizeProxy")
				if theVal == OK or classof theVal == BooleanClass do theVal = 2
				XMeshSaver_Settings.autoOptimizeProxy = theVal
			)
			ddl_applyOptimize.selection = XMeshSaver_Settings.autoOptimizeProxy
			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Advanced" "VertexPercent")
				if theVal == OK do theVal = 5.0
				XMeshSaver_Settings.VertexPercent = theVal
			)
			spn_vertexPercent.value = XMeshSaver_Settings.VertexPercent
			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Advanced" "MinVertices")
				if theVal == OK do theVal = 2000
				XMeshSaver_Settings.MinVertices	= theVal
			)
			spn_MinVertices.value = XMeshSaver_Settings.MinVertices
		)

		fn createMayaMELScript theObjects theMelScriptFile theXMeshFile =
		(
			--local theMat = XMeshSaver_MaterialUtils.getMaterialFromNodes theObjects
			local theMelScript = createFile theMelScriptFile
			if theMelScript == undefined do return false
			theXMeshFile = substituteString theXMeshFile "\\" "/"
			format "python(\"import createXMeshLoader; reload(createXMeshLoader); createXMeshLoader.createXMeshLoaderFromPath(\\\"%\\\");\");\n"	theXmeshFile to:theMelScript
			--format "string $sel[] = `ls -sl`;\n"	to:theMelScript
			--format "string $xmloader = `substitute \"Transform\" $sel[0] \"\"`;\n" to:theMelScript
			close theMelScript
			--edit theMelScriptFile
		)--end fn 
		

		fn createXMeshLoaderScript thePath theOriginalObjects theProxyPath:undefined theFrameRange:undefined theMatLibPath: =
		(
			local theName = "XML_" + getFileNameFile thePath
			local theXMeshLoader = (getNodeByName theName)
			local theScriptFile = createFile (getFileNamePath thePath + "CREATE_"+ theName  + ".MS")

			local theRenderFile = thePath
			if theFrameRange != undefined do
			(
				theRenderFile = XMeshSaverUtils.ReplaceSequenceNumber theRenderFile theFrameRange[1]
			)

			format "(\nlocal theXMeshLoader = XMeshLoader()\n" to:theScriptFile
			format "local thePath = getFilenamePath (getThisScriptFilename())\n" to:theScriptFile
			format "local goOn = true\n" to:theScriptFile
			format "if not doesFileExist (thePath+\"\\\\\"+\"%\" ) do (thePath = @\"%\")\n" (filenameFromPath theRenderFile) (getFileNamePath thePath) to:theScriptFile
			format "if not doesFileExist (thePath+\"\\\\\"+\"%\" ) do ((messagebox \"Please ensure you are executing the script from a MAPPED PATH or local drive to automatically resolve the path.\n\n If you are executing from a Network location, make sure the hard-coded path in the script exists.\" title:\"XMesh Source Sequence Path Not Found\"); goOn = false) \n" (filenameFromPath theRenderFile) to:theScriptFile
			format "if goOn == true do (\n" to:theScriptFile
			format "local theXMeshLayer = LayerManager.getLayerFromName  \"XMesh Loaders\" \n" to:theScriptFile
			format "if theXMeshLayer == undefined do theXMeshLayer = LayerManager.newLayerFromName \"XMesh Loaders\" \n" to:theScriptFile
			format "theXMeshLayer.addnode theXMeshLoader \n" to:theScriptFile

			format "theXMeshLoader.viewportSequenceID = 0 \n" to:theScriptFile
			format "theXMeshLoader.name = uniquename \"%_\" \n" theName to:theScriptFile
			if XMeshSaver_Settings.saveMode == 2 then
			(
				format "theXMeshLoader.wirecolor = % \n" theOriginalObjects[1].wirecolor to:theScriptFile
			)
			else if XMeshSaver_Settings.saveMode == 3 do
			(
				format "theXMeshLoader.wirecolor = % \n" theOriginalObjects[1].wirecolor to:theScriptFile
				format "at time 0 theXMeshLoader.transform = % \n" (at time 0 theOriginalObjects[1].transform) to:theScriptFile

				case XMeshSaver_Settings.ObjectSpaceAnimationMode of
				(
					#bake:
					(
						if theFrameRange != undefined and theFrameRange[2]-theFrameRange[1] > 0 then
						(
							format "local theTM=#(\n" to:theScriptFile
							for t =  theFrameRange[1] to theFrameRange[2]-1 do
								format "\t#(%,%),\n" t (at time t theOriginalObjects[1].transform) to:theScriptFile
							format "\t#(%,%))\n" theFrameRange[2] (at time theFrameRange[2] theOriginalObjects[1].transform) to:theScriptFile
							format "for t in theTM do with animate on at time t[1] theXmeshLoader.transform = t[2]\n" to:theScriptFile
						)
					)
					#linktoparent:
					(
						if isValidNode theOriginalObjects[1].parent do
							format "try(at time 0 theXMeshLoader.parent = (getNodeByName \"%\").parent)catch()\n" theOriginalObjects[1].name to:theScriptFile
					)
					#linktosource:
					(
							format "at time 0 theXMeshLoader.parent = getNodeByName \"%\" \n" theOriginalObjects[1].name to:theScriptFile
					)
				)

			)

			if XMeshSaver_Settings.xmeshLoaderViewport < 5 then
			(
				format "theXMeshLoader.enableViewportMesh = true \n" to:theScriptFile
				format "theXMeshLoader.displayMode = % \n" (XMeshSaver_Settings.xmeshLoaderViewport-1) to:theScriptFile
				format "theXMeshLoader.displayPercent = % \n" XMeshSaver_Settings.XMeshLoaderViewPercent to:theScriptFile
			)
			else
			(
				format "theXMeshLoader.enableViewportMesh = false \n" to:theScriptFile
			)
			format "theXMeshLoader.limitToRange = % \n" (theFrameRange != undefined) to:theScriptFile
			format "select theXMeshLoader \n" to:theScriptFile
			if theFrameRange != undefined do
			(
				format "theXMeshLoader.rangeFirstFrame = % \n" theFrameRange[1] to:theScriptFile
				format "theXMeshLoader.rangeLastFrame = % \n" theFrameRange[2] to:theScriptFile
			)
			if theProxyPath == undefined or theProxyPath == "" then
			(
				format "theXMeshLoader.viewportSequenceID = 0 \n" to:theScriptFile
				format "theXMeshLoader.proxySequence = \"\" \n" to:theScriptFile
			)
			else
			(
				format "theXMeshLoader.viewportSequenceID = 1 \n" to:theScriptFile
				local theProxyFile = theProxyPath
				if theFrameRange != undefined do
				(
					theProxyFile = XMeshSaverUtils.ReplaceSequenceNumber theProxyFile theFrameRange[1]
				)
				local theProxyDir = filterString (getFileNamePath theProxyFile) "\\"
				--format "%\n" theProxyDir
				theProxyDir = theProxyDir[theProxyDir.count]+"\\\\"
				format "theXMeshLoader.proxySequence = thePath + \"%%\" \n" theProxyDir (filenameFromPath theProxyFile) to:theScriptFile
			)

			format "theXMeshLoader.renderSequence = thePath + \"%\" \n" (filenameFromPath theRenderFile) to:theScriptFile
			if XMeshSaver_Settings.frameStep > 1.0 do
			(
				format "theXMeshLoader.loadMode = 2 \n" to:theScriptFile
			)
			if theMatLibPath != unsupplied and theMatLibPath != undefined do
			(
				format "local theMatLibPath = thePath + \"%\" \n" (fileNameFromPath theMatLibPath) to:theScriptFile
				format "if doesFileExist theMatLibPath do (\n" to:theScriptFile
				format "local theMatLib = loadTempMaterialLibrary theMatLibPath \n" to:theScriptFile
				format "if theMatLib != undefined do theXMeshLoader.material = theMatLib[1] \n" to:theScriptFile
				format ")\n" to:theScriptFile
			)
			format ")\n)\n" to:theScriptFile
			close theScriptFile
			
			createMayaMELScript theOriginalObjects (getFileNamePath thePath + "CREATE_"+ theName  + ".MEL") theRenderFile
		)


		fn createXMeshLoader thePath theOriginalObjects theProxyPath:undefined theFrameRange:undefined theMaterial: theMatLibPath: =
		(
			if isXMeshLoaderInstalled() do
			(
				local theName = "XML_" + getFileNameFile thePath
				/*local theXMeshLoaders = getNodeByName theName all:true
				theXMeshLoaders = for o in theXMeshLoaders where classof o.baseObject == XMeshLoader collect o
				local theXMeshLoader = if theXMeshLoaders.count > 0 then theXMeshLoaders[1] else undefined
				if not isValidNode theXMeshLoader then
				(*/
					theXMeshLoader = XMeshLoader()
					local theXMeshLayer = LayerManager.getLayerFromName  "XMesh Loaders"
					if theXMeshLayer == undefined do theXMeshLayer = LayerManager.newLayerFromName "XMesh Loaders"
					theXMeshLayer.addnode theXMeshLoader

					theXMeshLoader.viewportSequenceID = 0
					theXMeshLoader.name = uniquename theName
					if XMeshSaver_Settings.saveMode == 2 then
					(
						theXMeshLoader.wirecolor = theOriginalObjects[1].wirecolor
					)
					else if XMeshSaver_Settings.saveMode == 3 do
					(
						at time 0 theXMeshLoader.transform = theOriginalObjects[1].transform
						theXMeshLoader.wirecolor = theOriginalObjects[1].wirecolor
					)
				--)--end create new XMesh Loader
				if XMeshSaver_Settings.saveMode == 3 do
				(
					case XMeshSaver_Settings.ObjectSpaceAnimationMode of
					(
						#bake:
						(
							if theFrameRange != undefined then
							(
								deleteKeys theXMeshLoader.transform.controller --delete existing keys
								theXMeshLoader.parent = undefined --unlink
								for t =  theFrameRange[1] to  theFrameRange[2] do
								(
									at time t with animate on
										theXMeshLoader.transform = theOriginalObjects[1].transform
								)
							)
						)
						#linktoparent:
						(
							deleteKeys theXMeshLoader.transform.controller
							at time 0 theXMeshLoader.parent = theOriginalObjects[1].parent
						)
						#linktosource:
						(
							deleteKeys theXMeshLoader.transform.controller
							at time 0 theXMeshLoader.parent = theOriginalObjects[1]
						)
					)
				)
				if XMeshSaver_Settings.xmeshLoaderViewport < 5 then
				(
					theXMeshLoader.enableViewportMesh = true
					theXMeshLoader.displayMode = XMeshSaver_Settings.xmeshLoaderViewport-1
					theXMeshLoader.displayPercent = XMeshSaver_Settings.XMeshLoaderViewPercent
				)
				else
				(
					theXMeshLoader.enableViewportMesh = false
				)
				theXMeshLoader.limitToRange = (theFrameRange != undefined)
				select theXMeshLoader
				if theFrameRange != undefined do
				(
					theXMeshLoader.rangeFirstFrame = theFrameRange[1]
					theXMeshLoader.rangeLastFrame = theFrameRange[2]
				)
				if theProxyPath == undefined then
				(
					theXMeshLoader.viewportSequenceID = 0
					theXMeshLoader.proxySequence = ""
				)
				else
				(
					theXMeshLoader.viewportSequenceID = 1
					local theProxyFile = theProxyPath
					if theFrameRange != undefined do
					(
						theProxyFile = XMeshSaverUtils.ReplaceSequenceNumber theProxyFile theFrameRange[1]
					)
					theXMeshLoader.proxySequence = theProxyFile
				)
				case XMeshSaver_Settings.sourceObjectsHandling of
				(
					1: ( ) --do nothing
					2: (
							freeze theOriginalObjects
							for o in toSaveList do try(freeze o)catch()
					)
					3: (
							theOriginalObjects.boxMode = true
					)
					5: (
							theOriginalObjects.renderable = false
					)
					6: (
							theOriginalObjects.renderable = false
							theOriginalObjects.boxMode = true
					)
					default:
					(
						hide theOriginalObjects
						for o in toSaveList do try(hide o)catch()
					)
				)
				local theRenderFile = thePath
				if theFrameRange != undefined do
				(
					theRenderFile = XMeshSaverUtils.ReplaceSequenceNumber theRenderFile theFrameRange[1]
				)
				theXMeshLoader.renderSequence = theRenderFile
				if theMaterial != unsupplied do theXMeshLoader.material = theMaterial

				if XMeshSaver_Settings.frameStep > 1.0 do theXMeshLoader.loadMode = 2

				if theMatLibPath != unsupplied and theMatLibPath != undefined and doesFileExist theMatLibPath do
				(
					local theMatLib = loadTempMaterialLibrary theMatLibPath
					if theMatLib != undefined then
						theXMeshLoader.material = theMatLib[1]
					else
						format "XMesh Saver - Error reading from material library: %\n" (theMatLibPath as String)
				)
			)
		)


		on spn_vertexPercent changed val do
		(
			setIniSetting theIniFile "Advanced" "VertexPercent" (val as string)
			XMeshSaver_Settings.VertexPercent = val
		)

		on spn_MinVertices changed val do
		(
			setIniSetting theIniFile "Advanced" "MinVertices" (val as string)
			XMeshSaver_Settings.MinVertices = val
		)

		on ddl_saveMode selected itm do
		(
			XMeshSaver_Settings.saveMode = itm
			XMesh_saveParams_rollout.updatePathFromSettings()
			setIniSetting theIniFile "Settings" "SaveMode" (XMeshSaver_Settings.saveMode as string)
			canSaveCheck()
		)

		on chk_folderPerObject changed val do
		(
			XMeshSaver_Settings.folderPerObject = val
			XMesh_saveParams_rollout.updatePathFromSettings()
			setIniSetting theIniFile "Settings" "FolderPerObject" (XMeshSaver_Settings.folderPerObject as string)
			canSaveCheck()
		)

		on chk_createMaterialLibrary changed  val do
		(
			XMeshSaver_Settings.createMaterialLibrary =  val
			setIniSetting theIniFile "Settings" "CreateMaterialLibrary" (XMeshSaver_Settings.createMaterialLibrary as string)
			canSaveCheck()
		)

		on chk_createXMeshLoader changed  val do
		(
			XMeshSaver_Settings.createXMeshLoaders =  val
			setIniSetting theIniFile "Settings" "CreateXMeshLoader" (XMeshSaver_Settings.createXMeshLoaders as string)
			canSaveCheck()
		)

		on chk_saveProxy changed val do
		(
			XMeshSaver_Settings.saveProxy = val
			setIniSetting theIniFile "Settings" "SaveProxy" (val as String)
		)

		on chk_useProxySampling changed val do
		(
			XMeshSaver_Settings.useProxyFrameStep = val
			setIniSetting theIniFile "Settings" "UseProxyFrameStep" (val as string)
		)

		fn updateOptimizeTooltip =
		(
			ddl_applyOptimize.tooltip = case XMeshSaver_Settings.autoOptimizeProxy of
			(
				default: "Objects with separate Viewport/Render settings\nlike Particle Flow or Frost will save\na lower resolution Proxy mesh.\n\nAll other geometry objects will save\nthe same meshes for Render and Proxy."
				2: "Objects without Viewport/Render settings\nwill be optimized using a temporary\nOPTIMIZE modifier, reducing the\npolygon counts by Face Angle Threshold."
				3: "Objects without Viewport/Render settings\nwill be optimized using a temporary\nMULTIRES modifier, reducing the\nvertex counts by Percentage and Min.Vertices.\n(See Advanced Settings Rollout)."
				4: "Objects without Viewport/Render settings\nwill be optimized using a temporary\nPROOPTIMIZER modifier, reducing the\nvertex counts by Percentage and Min.Vertices.\n(See Advanced Settings Rollout)."
			)
			spn_vertexPercent.enabled = spn_MinVertices.enabled = ddl_applyOptimize.selection > 2
		)

		on ddl_applyOptimize selected itm do
		(
			XMeshSaver_Settings.autoOptimizeProxy = itm
			setIniSetting theIniFile "Settings" "AutoOptimizeProxy" (itm as string)
			updateOptimizeTooltip()
		)

		fn setSamplingTooltip =
		(
			local theList = getFrameList XMeshSaver_Settings.frameStep
			local theListString = theList as string
			ddl_frameStep.tooltip = substring theListString 3 (theListString.count-3) + " " + (if theList.count > 20 then theList[theList.count] as string else "")

			local theList = getFrameList XMeshSaver_Settings.ProxyFrameStep
			local theListString = theList as string
			ddl_ProxyFrameStep.tooltip = substring theListString 3 (theListString.count-3) + " " + (if theList.count > 20 then theList[theList.count] as string else "")
		)
		on spn_frameStart changed val do
		(
			XMeshSaver_Settings.frameStart = val
			if val > XMeshSaver_Settings.frameEnd do XMeshSaver_Settings.frameEnd = spn_frameEnd.value = val
			setSamplingTooltip()
		)
		on spn_frameEnd changed val do
		(
			XMeshSaver_Settings.frameEnd = val
			if val < XMeshSaver_Settings.frameStart do XMeshSaver_Settings.frameStart = spn_frameStart.value = val
			setSamplingTooltip()
		)

		fn updateSamplingStepLists =
		(
			theValidSteps = getValidSamplingSteps()
			ddl_ProxyFrameStep.items = ddl_frameStep.items = for i in theValidSteps collect
			(
				if i >= 1 then
					(i as string) + (if i == 1 then " Sample/Frame" else " Samples/Frame")
				else
					("Every "+((1/i) as integer) as string + " Frames")
			)
			ddl_frameStep.selection = findItem theValidSteps XMeshSaver_Settings.frameStep
			ddl_ProxyFrameStep.selection = findItem theValidSteps XMeshSaver_Settings.ProxyFrameStep
		)

		on ddl_frameStep selected itm do
		(
			XMeshSaver_Settings.frameStep = theValidSteps[itm]
			setIniSetting theIniFile "Settings" "FrameStep" (XMeshSaver_Settings.frameStep as string)
			updateSamplingStepLists()
			setSamplingTooltip()
		)

		on ddl_ProxyFrameStep selected itm do
		(
			XMeshSaver_Settings.ProxyFrameStep = theValidSteps[itm]
			setIniSetting theIniFile "Settings" "ProxyFrameStep" (XMeshSaver_Settings.proxyFrameStep as string)
			updateSamplingStepLists()
			setSamplingTooltip()
		)

		on btn_getSceneRange pressed do
		(
			XMeshSaver_Settings.frameStart = spn_frameStart.value = animationrange.start.frame as integer
			XMeshSaver_Settings.frameEnd = spn_frameEnd.value = animationrange.end.frame as integer
		)

		on btn_getCurrentFrame pressed do
		(
			XMeshSaver_Settings.frameStart = spn_frameStart.value = XMeshSaver_Settings.frameEnd = spn_frameEnd.value =sliderTime.frame as integer
		)

		on btn_saveMeshes pressed do
		(
			local st = timestamp()
			local theMeshesToSave = GetMeshesToSave()
			if theMeshesToSave.count == 0 do return false
			XMeshSaverUtils.LogDebug ("SAVING THESE MESHES:")
			for i = 1 to theMeshesToSave.count do XMeshSaverUtils.LogDebug ("  "+ (GetMeshToSaveName theMeshesToSave i))

			if ::XMeshSaverTools_LastProcessedObjects == undefined do ::XMeshSaverTools_LastProcessedObjects = #()

			append ::XMeshSaverTools_LastProcessedObjects #(localtime, maxfilename, deepcopy theMeshesToSave, getStructSettings())

			XMeshSaverUtils.ClearAllMaterialIDMapping()
			XMeshSaverUtils.sourceChannels = XMeshSaver_Settings.activeSourceChannels
			local saveVelocity = (findItem XMeshSaver_Settings.activeSourceChannels "Velocity") != 0
			XMeshSaverUtils.objFlipYZ = (XMeshSaver_Settings.CoordinateSystem == #yuprh)

			if XMeshSaver_Settings.SavePolymesh then
			(
				SaveMeshToSequence = XMeshSaverUtils.SavePolymeshToSequence
				SaveMeshesToSequence = XMeshSaverUtils.SavePolymeshesToSequence
			)
			else
			(
				SaveMeshToSequence = XMeshSaverUtils.SaveMeshToSequence
				SaveMeshesToSequence = XMeshSaverUtils.SaveMeshesToSequence
			)

			-- reset sequence
			XMeshSaverUtils.SetSequenceName "c:\\"

			XMeshSaverUtils.AcquireLicense()
			try
			(
				if XMeshSaverUtils.HasLicense do
				(
					disableSceneRedraw()
					try
					(
						--Handling TP Groups saving:
						if XMeshSaver_Settings.ObjectSourceMode == 9 then
						(
							if XMeshSaver_Settings.saveMode == 1 then --if saving all groups as one XMesh
							(
								local thePathToSaveTo = buildCacheFileName()
								makeDir (getFileNamePath thePathToSaveTo) all:true
								XMeshSaverUtils.SetSequenceName thePathToSaveTo

								local theTPToSave = theMeshesToSave[1][3]
								if XMeshSaver_Settings.createMaterialLibrary do
								(
									local theMaterial = XMeshSaver_MaterialUtils.getMaterialFromNodes #(theTPToSave)
									--local theMaterial = theTPToSave.material
									if theMaterial != undefined do
									(
										theMaterial.name = "XMesh_MultiMaterial"
										local theMatLibPath = (getFileNamePath thePathToSaveTo + getFileNameFile thePathToSaveTo + ".mat")
										deleteFile theMatLibPath
										if XMeshSaver_Settings.createMaterialLibrary and theMaterial.numsubs > 1 do
										(
											local theMatLib = materialLibrary()
											append theMatLib theMaterial
											saveTempMaterialLibrary theMatLib theMatLibPath

											local theMatIDmapFile = getFileNamePath thePathToSaveTo + getFileNameFile thePathToSaveTo + ".matIDmap"
											SaveMatIDMapFile theMaterial theMatIDmapFile
										)
									)
								)

								TPGroupsList = #() --init. the array to collect TP groups
								collectTPGroups theTPToSave.GroupManager theTPToSave.name theTPToSave --collect them
								local theEnabledStates = for i in TPGroupsList collect i[1].renderable
								local TPGroupsToRender = for i in theMeshesToSave collect i[1]
								for i in TPGroupsList do i[1].renderable = findItem TPGroupsToRender i[1] > 0

								prepareMetaData theMeshesToSave

								progressStart ("Saving " + theTPToSave.name+ " Groups")

								local result = true
								local theFramesToSave = getFrameList XMeshSaver_Settings.frameStep
								for i = 1 to theFramesToSave.count do
								(
									result = progressUpdate  (100.0*i/theFramesToSave.count)
									if not result do exit
									if XMeshSaver_Settings.DisableDataOptimization == true do
									(
										XMeshSaverUtils.SetSequenceName "c:\\"
										XMeshSaverUtils.SetSequenceName thePathToSaveTo
									)

									at time theFramesToSave[i] (SaveMeshesToSequence #(theTPToSave) XMeshSaver_Settings.ignoreEmpty XMeshSaver_Settings.ignoreTopology saveVelocity)
								)

								progressEnd()
								if result do
								(
									local theFrameRange = #(XMeshSaver_Settings.frameStart, XMeshSaver_Settings.frameEnd)
									if XMeshSaver_Settings.createXMeshLoaders do
									(
										createXMeshLoader thePathToSaveTo #(theTPToSave) theFrameRange:theFrameRange theMatLibPath:theMatLibPath
									)
									createXMeshLoaderScript thePathToSaveTo #(theTPToSave) theFrameRange:theFrameRange theMatLibPath:theMatLibPath
								)

								for i = 1 to TPGroupsList.count do TPGroupsList[i][1].renderable = theEnabledStates[i]
							) -- modes 2 and 3
							else
							(
								for aGroup in theMeshesToSave do
								(
									progressStart ("Saving " + aGroup[2] + "...")

									local filePath = buildCacheFileName()
									local theFilePath = getFilenamePath filePath
									local theFileName = getFilenameFile filePath
									local theFileType = getFilenameType filePath
									local theMeshName = validateObjectNameAsFileName aGroup[2]
									local theMaterial = undefined

									local theNewPath = if XMeshSaver_Settings.folderPerObject then
										(theFilePath + theMeshName + "\\" + theFileName + "_" + theMeshName  + "_" + theFileType )
									else
										(theFilePath + theFileName + "_" + theMeshName + "_" + theFileType )

									makeDir (getFileNamePath theNewPath) all:true
									XMeshSaverUtils.SetSequenceName theNewPath

									XMeshSaverUtils.ClearAllMaterialIDMapping()

									local theTP = (refs.dependentNodes aGroup[1])[1]
									if XMeshSaver_Settings.createMaterialLibrary and theTP.material != undefined do
									(
										theMaterial = XMeshSaver_MaterialUtils.getMaterialFromNodes #(theTP)
										local theMatLib = materialLibrary()
										append theMatLib theMaterial
										saveTempMaterialLibrary theMatLib (getFileNamePath theNewPath + getFileNameFile theNewPath+ ".mat")

										local theMatIDmapFile = getFileNamePath theNewPath + getFileNameFile theNewPath + ".matIDmap"
										SaveMatIDMapFile theMaterial theMatIDmapFile
									)

									TPGroupsList = #() --init. the array to collect TP groups
									collectTPGroups theTP.GroupManager theTP.name theTP --collect them
									local theEnabledStates = for i in TPGroupsList collect i[1].renderable
									for i in TPGroupsList do i[1].renderable = i[1] == aGroup[1]

									prepareMetaData #(aGroup)
									local result = true
									local theFramesToSave = getFrameList XMeshSaver_Settings.frameStep
									for i = 1 to theFramesToSave.count do
									(
										result = progressUpdate  (100.0*i/theFramesToSave.count)
										if not result do exit
										if XMeshSaver_Settings.DisableDataOptimization == true do
										(
											XMeshSaverUtils.SetSequenceName "c:\\"
											XMeshSaverUtils.SetSequenceName theNewPath
										)
										at time theFramesToSave[i]
										(
											SaveMeshToSequence theTP XMeshSaver_Settings.ignoreEmpty XMeshSaver_Settings.ignoreTopology (XMeshSaver_Settings.saveMode==3) saveVelocity
										)
									)
									progressEnd()
									if result do
									(
										local theFrameRange = #(XMeshSaver_Settings.frameStart, XMeshSaver_Settings.frameEnd)
										if XMeshSaver_Settings.createXMeshLoaders and result do
										(
											createXMeshLoader theNewPath #(theTP) theFrameRange:theFrameRange theMaterial:theMaterial
										)
										createXMeshLoaderScript theNewPath #(theTP) theFrameRange:theFrameRange
									)
									for i = 1 to TPGroupsList.count do TPGroupsList[i][1].renderable = theEnabledStates[i]
								)--end aGroup loop
							)
						)
						else
						(
							local saveProxy = XMeshSaver_Settings.saveProxy
							local setSceneRenderList = (if saveProxy then #(false,true) else #(true))
							local theProxyPath = undefined
							if XMeshSaver_Settings.saveMode == 1 then -- world space mode
							(
								local thePathToSaveTo = buildCacheFileName numobjects:theMeshesToSave.count
								--print thePathToSaveTo
								makeDir (getFileNamePath thePathToSaveTo) all:true
								XMeshSaverUtils.SetSequenceName thePathToSaveTo

								if XMeshSaver_Settings.createMaterialLibrary do
								(
									--Handle Objects with Missing Materials:
									local noMatList = for o in theMeshesToSave where o.material == undefined collect o
									case XMeshSaver_Settings.missingMaterialsHandling of
									(
										2: standInMaterial = Standard name:"NoMaterial Stand-in"
										3: standInMaterial = Standard name:"NoMaterial Red Stand-in" diffusecolor:red selfIllumAmount:100
									)
									for o in noMatList do
									(
										case XMeshSaver_Settings.missingMaterialsHandling of
										(
											1: () --do nothing
											default: (o.material = standInMaterial) --2 or 3
											4: o.material = Standard diffusecolor:o.wirecolor
										)
									)
									local theMaterial = XMeshSaver_MaterialUtils.getMaterialFromNodes theMeshesToSave
									theMaterial.name = "XMesh_MultiMaterial"
									local theMatLibPath = (getFileNamePath thePathToSaveTo + getFileNameFile thePathToSaveTo + ".mat")
									deleteFile theMatLibPath
									if theMaterial.numsubs > 1 do
									(
										local theMatLib = materialLibrary()
										append theMatLib theMaterial
										saveTempMaterialLibrary theMatLib theMatLibPath

										local theMatIDmapFile = getFileNamePath thePathToSaveTo + getFileNameFile thePathToSaveTo + ".matIDmap"
										SaveMatIDMapFile theMaterial theMatIDmapFile
									)
									for o in noMatList do o.material = undefined
								)

								local result = true
								for setSceneRender in setSceneRenderList while result do
								(
									local thePath = thePathToSaveTo

									if setSceneRender then
									(
										saveVelocity = (findItem XMeshSaver_Settings.activeSourceChannels "Velocity") != 0
										XMeshSaverUtils.sourceChannels = XMeshSaver_Settings.activeSourceChannels
									)
									else
									(
										--filter active channels that are allowed in proxy mode
										local tempArray = (for aChannel in XMeshSaver_Settings.ActiveSourceChannels where findItem XMeshSaver_Settings.proxyActiveSourceChannels aChannel > 0 collect aChannel)
										saveVelocity = (findItem tempArray "Velocity") != 0
										XMeshSaverUtils.sourceChannels = tempArray

										thePath = theProxyPath = getProxyPath thePathToSaveTo
										MakeProxy theMeshesToSave
									)


									try
									(
										makeDir (getFileNamePath thePath) all:true
										XMeshSaverUtils.SetSequenceName thePath
										if setSceneRender do
											XMeshSaverUtils.SetSceneRenderBegin()
										try
										(
											progressStart (if setSceneRender then "Saving Meshes..." else "Saving Proxy Meshes...")
											prepareMetaData theMeshesToSave

											local theFramesToSave = getFrameList (if XMeshSaver_Settings.useProxyFrameStep and not setSceneRender then XMeshSaver_Settings.ProxyFrameStep else XMeshSaver_Settings.frameStep)

											for i = 1 to theFramesToSave.count do
											(
												result = progressUpdate  (100.0*i/theFramesToSave.count)
												if not result do exit
												if XMeshSaver_Settings.DisableDataOptimization == true do
												(
													XMeshSaverUtils.SetSequenceName "c:\\"
													XMeshSaverUtils.SetSequenceName thePath
												)
												at time theFramesToSave[i]
												(
													local filteredMeshesToSave = case  XMeshSaver_Settings.ObjectVisibilityMode of
													(
														default: theMeshesToSave
														2: for o in theMeshesToSave where getInheritedVisibility o mode:2 collect o
														3: for o in theMeshesToSave where getInheritedVisibility o mode:3 collect o
													)

													if not setSceneRender do
													(
														updateProxy filteredMeshesToSave
													)
													(SaveMeshesToSequence filteredMeshesToSave XMeshSaver_Settings.ignoreEmpty XMeshSaver_Settings.ignoreTopology saveVelocity)
												)
											)
											progressEnd()
										)
										catch
										(
											if setSceneRender do
												XMeshSaverUtils.SetSceneRenderEnd()
											throw()
										)
										if setSceneRender do
											XMeshSaverUtils.SetSceneRenderEnd()
									)
									catch
									(
										if not setSceneRender do
											RemoveProxy theMeshesToSave
										throw()
									)
									if not setSceneRender do
										RemoveProxy theMeshesToSave
								)--end setSceneRender loop

								XMeshSaverUtils.ClearAllMaterialIDMapping()
								if result do
								(
									local theFrameRange = #(XMeshSaver_Settings.frameStart, XMeshSaver_Settings.frameEnd)
									if XMeshSaver_Settings.createXMeshLoaders and result do
									(
										createXMeshLoader thePathToSaveTo theMeshesToSave theProxyPath:theProxyPath theFrameRange:theFrameRange theMatLibPath:theMatLibPath
									)
									createXMeshLoaderScript thePathToSaveTo theMeshesToSave theProxyPath:theProxyPath theFrameRange:theFrameRange theMatLibPath:theMatLibPath
								)
							)
							else
							(
								local emptyMesh = Editable_Mesh()
								local result = true
								for setSceneRender in setSceneRenderList while result do
								(

									if setSceneRender then
									(
										saveVelocity = (findItem XMeshSaver_Settings.activeSourceChannels "Velocity") != 0
										XMeshSaverUtils.sourceChannels = XMeshSaver_Settings.activeSourceChannels
										XMeshSaverUtils.SetSceneRenderBegin()
									)
									else
									(
										--filter active channels that are allowed in proxy mode
										local tempArray = (for aChannel in XMeshSaver_Settings.ActiveSourceChannels where findItem XMeshSaver_Settings.proxyActiveSourceChannels aChannel > 0 collect aChannel)
										saveVelocity = (findItem tempArray "Velocity") != 0
										XMeshSaverUtils.sourceChannels = tempArray
									)


									try
									(
										for aMesh in theMeshesToSave while result do
										(
											progressStart (if setSceneRender then ("Saving " + aMesh.name + "...") else ("Saving " + aMesh.name + " Proxy..."))

											local filePath = buildCacheFileName()
											local theFilePath = getFilenamePath filePath
											local theFileName = getFilenameFile filePath
											local theFileType = getFilenameType filePath
											local theMeshName = validateObjectNameAsFileName aMesh.name
											local theMaterial = undefined

											local theNewPath = if XMeshSaver_Settings.folderPerObject then
												(theFilePath + theMeshName + "\\" + theFileName + "_" + theMeshName  + "_" + theFileType )
											else
												(theFilePath + theFileName + "_" + theMeshName + "_" + theFileType )

											if saveProxy do
											(
												theProxyPath = getProxyPath theNewPath
												if not setSceneRender do (
													theNewPath = theProxyPath
													MakeProxy aMesh
												)
											)
											try
											(
												makeDir (getFileNamePath theNewPath) all:true
												XMeshSaverUtils.SetSequenceName theNewPath

												XMeshSaverUtils.ClearAllMaterialIDMapping()

												if XMeshSaver_Settings.createMaterialLibrary and aMesh.material != undefined and setSceneRender do
												(
													if classof aMesh == Thinking then
													(
														theMaterial = XMeshSaver_MaterialUtils.getMaterialFromNodes #(aMesh)
														theMaterial.name = "XMesh_MultiMaterial"
													)
													else
													(
														theMaterial = aMesh.material
													)

													if theMaterial != undefined do
													(
														local theMatLib = materialLibrary()
														append theMatLib theMaterial
														local theMatLibPath = (getFileNamePath theNewPath + getFileNameFile theNewPath+ ".mat")
														deleteFile theMatLibPath
														saveTempMaterialLibrary theMatLib theMatLibPath
														local theMatIDmapFile = getFileNamePath theNewPath + getFileNameFile theNewPath + ".matIDmap"
														SaveMatIDMapFile theMaterial theMatIDmapFile
													)
												)

												prepareMetaData #(aMesh)

												local theFramesToSave = getFrameList (if XMeshSaver_Settings.useProxyFrameStep and not setSceneRender then XMeshSaver_Settings.ProxyFrameStep else XMeshSaver_Settings.frameStep)
												for i = 1 to theFramesToSave.count do
												(
													result = progressUpdate  (100.0*i/theFramesToSave.count)
													if not result do exit
													if XMeshSaver_Settings.DisableDataOptimization == true do
													(
														XMeshSaverUtils.SetSequenceName "c:\\"
														XMeshSaverUtils.SetSequenceName theNewPath
													)

													at time theFramesToSave[i]
													(
														local filteredMeshToSave = case  XMeshSaver_Settings.ObjectVisibilityMode of
														(
															default: aMesh
															2: if ( getInheritedVisibility aMesh mode:2 ) then aMesh else emptyMesh
															3: if ( getInheritedVisibility aMesh mode:3 ) then aMesh else emptyMesh
														)
														if not setSceneRender do
														(
															UpdateProxy filteredMeshToSave
														)
														SaveMeshToSequence filteredMeshToSave XMeshSaver_Settings.ignoreEmpty XMeshSaver_Settings.ignoreTopology (XMeshSaver_Settings.saveMode==3) saveVelocity
													)
												)
												progressEnd()
											)
											catch
											(
												if not setSceneRender do
													RemoveProxy aMesh
												throw()
											)
											if not setSceneRender do
												RemoveProxy aMesh

											if result and (setSceneRender == setSceneRenderList[setSceneRenderList.count]) do
											(
												local theFrameRange = #(XMeshSaver_Settings.frameStart, XMeshSaver_Settings.frameEnd)
												if XMeshSaver_Settings.createXMeshLoaders do
												(
													createXMeshLoader theNewPath #(aMesh) theProxyPath:theProxyPath theFrameRange:theFrameRange theMaterial:theMaterial
												)
												createXMeshLoaderScript theNewPath #(aMesh) theProxyPath:theProxyPath theFrameRange:theFrameRange
											)
										)
									)
									catch
									(
										if setSceneRender do
											XMeshSaverUtils.SetSceneRenderEnd()
										throw()
									)
									if setSceneRender do
										XMeshSaverUtils.SetSceneRenderEnd()
								)
								delete emptyMesh
							)
						)
					)
					catch
					(
						enableSceneRedraw()
						throw()
					)
					enableSceneRedraw()
				)
			)
			catch
			(
				XMeshSaverUtils.ReleaseLicense()
				throw()
			)
			XMeshSaverUtils.ReleaseLicense()
			XMesh_saveParams_rollout.updateAllLists reload:false
			XMeshSaverUtils.LogStats ("XMesh Cache Time: "+((timestamp()-st)/1000.0) as string+ " seconds." )
		)

		on saveOptions_rollout open do
		(
			--resizeDialog (getDialogSize XMesh_saveParams_rollout)
			--updateAllLists()
			chk_createXMeshLoader.enabled = isXMeshLoaderInstalled()
			if not isXMeshLoaderInstalled() do
			(
				chk_createXMeshLoader.enabled = false
				chk_createXMeshLoader.tooltip = "Disabled because XMesh Loader is not installed."
			)
			XMeshSaver_Settings.frameStart = spn_frameStart.value = animationrange.start.frame as integer
			XMeshSaver_Settings.frameEnd = spn_frameEnd.value = animationrange.end.frame as integer
			loadSettings()
			updateOptimizeTooltip()
			canSaveCheck()


			updateSamplingStepLists()
			setSamplingTooltip()
		)

		on saveOptions_rollout rolledUp val do
		(
			XMesh_saveParams_rollout.resizeDialog (getDialogSize XMesh_saveParams_rollout)
		)
	)

	rollout saveChannels_rollout "Save Channels" category:20
	(
		local activeChannels = #()
		local inactiveChannels = #()
		local proxyChannels = #()

		multilistbox lbx_inactiveChannels " Do Not Save These Channels:" items:#() height:13 across:2 width:218 align:#left offset:[-10,-2]
		multilistbox lbx_activeChannels " Save These Channels (If They Exist):" items:#() height:13 width:218 align:#right offset:[10,-2]

		button btn_ChannelAdd ">" height:86 width:22 pos:[223,22] tooltip:"Move the Selected Channels to the\n'Save These Channels...' list."
		button btn_ChannelRemove "<" height:86 pos:[223,110] width:22 tooltip:"Move the Selected Channels to the\n'Do Not Save These Channels' list."

		button btn_inactiveAll "All" width:50 height:20 align:#left offset:[-10,0] across:7 tooltip:"Select All Inactive Channels."
		button btn_inactiveInvert "Invert" width:50 height:20 align:#left offset:[-23,0] tooltip:"Invert Inactive Channels Selection."
		button btn_defaultChannels "Reset To Defaults" width:115 height:20 offset:[-34,0] align:#left

		button btn_ProxyChannelOff "Proxy OFF" width:60 height:20 offset:[43,0] align:#center tooltip:"Turns the selected Channels OFF in Proxy Mode."
		button btn_ProxyChannelOn "Proxy ON" width:55 height:20 offset:[37,0] align:#center tooltip:"Turns the selected Channels ON in Proxy Mode."

		button btn_activeAll "All" width:50 height:20 align:#right offset:[22,0] tooltip:"Select All Active Channels."
		button btn_activeInvert "Invert" width:50 height:20 align:#right offset:[9,0] tooltip:"Invert Active Channels Selection."

		fn populateLists =
		(
			activeChannels = for i in XMeshSaver_Settings.activeSourceChannels collect i
			proxyChannels = for i in XMeshSaver_Settings.proxyActiveSourceChannels collect i
			inactiveChannels = for i in allPossibleSourceChannels where findItem activeChannels i == 0 collect i

			lbx_inactiveChannels.items = inactiveChannels
			lbx_activeChannels.items = for i in activeChannels collect (if findItem proxyChannels i > 0 then i else i + " [-Proxy]")
		)

		fn loadSettings =
		(
			local keys = getIniSetting theIniFile "SaveChannels"
			local theVal = getIniSetting theIniFile "SaveChannels" "SourceChannels"
			if theVal == "" and (findItem keys "SourceChannels" == 0) then
				XMeshSaver_Settings.activeSourceChannels = deepCopy defaultActiveSourceChannels
			else
				XMeshSaver_Settings.activeSourceChannels = filterString theVal ","

			local theVal = getIniSetting theIniFile "SaveChannels" "ProxySourceChannels"
			if theVal == "" and (findItem keys "ProxySourceChannels" == 0) then
				XMeshSaver_Settings.proxyActiveSourceChannels = deepCopy defaultActiveSourceChannels
			else
				XMeshSaver_Settings.proxyActiveSourceChannels = filterString theVal ","
		)

		fn storeActiveChannels =
		(
			XMeshSaver_Settings.activeSourceChannels = activeChannels
			setIniSetting theIniFile "SaveChannels" "SourceChannels" (buildCommaSeparatedStringFromArray XMeshSaver_Settings.activeSourceChannels)

			XMeshSaver_Settings.proxyActiveSourceChannels = proxyChannels
			setIniSetting theIniFile "SaveChannels" "ProxySourceChannels" (buildCommaSeparatedStringFromArray XMeshSaver_Settings.proxyActiveSourceChannels)
		)

		fn addChannel =
		(
			local theArray = lbx_inactiveChannels.selection as array
			local toSelect = for i in lbx_activeChannels.selection collect activeChannels[i]
			local newSelection = #{}

			for j = theArray.count to 1 by -1 do
			(
				theIndex = theArray[j]
				append activeChannels inactiveChannels[theIndex]
				append proxyChannels inactiveChannels[theIndex]
				append toSelect inactiveChannels[theIndex][1]
				deleteItem inactiveChannels theIndex
			)
			storeActiveChannels()
			populateLists()
			for j in toSelect do
				for i = 1 to activeChannels.count where activeChannels[i] == j do append newSelection i
			lbx_activeChannels.selection = newSelection
			lbx_inactiveChannels.selection = #{}
		)

		fn removeChannel =
		(
			local theArray = lbx_activeChannels.selection as array
			local toSelect = for i in lbx_inactiveChannels.selection collect inactiveChannels[i]
			local newSelection = #{}
			for j = theArray.count to 1 by -1 do
			(
				local theIndex = theArray[j]
				--if not activeChannels[theIndex][1] == "Position" do
				(
					append inactiveChannels activeChannels[theIndex]
					append toSelect activeChannels[theIndex]
					deleteItem activeChannels theIndex
				)
			)
			storeActiveChannels()
			populateLists()
			for j in toSelect do
				for i = 1 to inactiveChannels.count where inactiveChannels[i] == j do append newSelection i
			lbx_inactiveChannels.selection = newSelection
			lbx_activeChannels.selection = #{}
		)

		on btn_inactiveAll pressed do lbx_inactiveChannels.selection = #{1..lbx_inactiveChannels.items.count}
		on btn_inactiveInvert pressed do lbx_inactiveChannels.selection = #{1..lbx_inactiveChannels.items.count} - lbx_inactiveChannels.selection

		on btn_activeAll pressed do lbx_activeChannels.selection = #{1..lbx_activeChannels.items.count}
		on btn_activeInvert pressed do lbx_activeChannels.selection = #{1..lbx_activeChannels.items.count} - lbx_activeChannels.selection

		on btn_ChannelAdd pressed do addChannel()
		on btn_ChannelRemove pressed do removeChannel()
		on lbx_inactiveChannels doubleClicked itm do addChannel()
		on lbx_activeChannels doubleClicked itm do removeChannel()

		on btn_ProxyChannelOn pressed do
		(
			local theArray = lbx_activeChannels.selection as array
			for i in theArray do
			(
				appendIfUnique proxyChannels activeChannels[i]
			)
			storeActiveChannels()
			populateLists()
		)
		on btn_ProxyChannelOff pressed do
		(
			local theArray = lbx_activeChannels.selection as array
			for i in theArray do
			(
				local theIndex = findItem proxyChannels activeChannels[i]
				if theIndex > 0 do deleteItem proxyChannels theIndex
			)
			storeActiveChannels()
			populateLists()
		)

		on btn_defaultChannels pressed do
		(
			XMeshSaver_Settings.activeSourceChannels = defaultActiveSourceChannels
			XMeshSaver_Settings.ProxyActiveSourceChannels = defaultActiveSourceChannels
			setIniSetting theIniFile "SaveChannels" "SourceChannels" (buildCommaSeparatedStringFromArray XMeshSaver_Settings.activeSourceChannels)
			populateLists()
		)

		on saveChannels_rollout open do
		(
			btn_ChannelAdd.images = #(rightTriangleBitmap,rightTriangleMask,2,1,1,2,2)
			btn_ChannelRemove.images = #(leftTriangleBitmap,leftTriangleMask,2,1,1,2,2)
			loadSettings()
			populateLists()
		)

		on saveChannels_rollout rolledUp val do
		(
			XMesh_saveParams_rollout.resizeDialog (getDialogSize XMesh_saveParams_rollout)
		)
	)

	rollout advancedSettings_rollout "Advanced Settings" category:100
	(
		checkbutton chk_ignoreEmpty ">Empty Mesh" offset:[-10,-3] width:153 align:#left across:3 tooltip:"When checked, empty meshes will be accepted for saving to disk.\n\nWhen unchecked, an error will be thrown and the saving will be interrupted if an empty mesh is detected."
		checkbutton chk_ignoreTopology ">Changing Topology" offset:[0,-3] width:153 align:#center tooltip:"When checked, changing topology between frames will be accepted.\n\nWhen unchecked, an error will be thrown and the saving will be interrupted if a changing topology is detected."
		checkbutton chk_savePolymesh ">Save Polymesh" offset:[9,-3] width:152 align:#right tooltip:"When checked, the mesh will be saved as a polygon mesh.\n\nWhen unchecked, the mesh will be saved as a triangle mesh.\n\nThis setting is NOT STICKY between 3ds Max sessions and will default to OFF after a restart!"
		checkbutton chk_saveMetaData ">Save MetaData" offset:[-10,-3] width:153 align:#left across:3 tooltip:"When checked, the current time, source type, saving mode, and list of objects will be saved to disk.\n\nWhen unchecked, this information will not be saved, reducing the file size.\n\nThis setting is NOT STICKY between 3ds Max sessions and will default to ON after a restart!"
		checkbutton chk_compressData ">Compress Data" offset:[0,-3] width:153 align:#center tooltip:"When checked, the .XMDAT files will use ZIP compression.\n\nWhen unchecked, the .XMDAT files will be saved uncompressed (faster saving at significant disk space cost).\n\nThis setting is NOT STICKY between 3ds Max sessions and will default to ON after a restart!"
		checkbutton chk_OptimizeData ">Optimize Data" offset:[9,-3] width:152 align:#right tooltip:"When checked, constant data will be saved just once and reused by following frames.\n\nWhen unchecked, new .XMDAT files will be saved even if the channel is static.\n\nThis setting is NOT STICKY between 3ds Max sessions and will default to ON after a restart!"

		dropdownlist ddl_missingMaterialsHandling "When an object has no material," width:320 align:#left offset:[-10,2] across:2 \
			items:#("DO NOTHING - Use No Material or an Empty Sub-Material","Use DEFAULT GRAY Standard Stand-In Material","Use RED NEON Standard Stand-In Material","Use OBJECT COLOR Standard Stand-In Material") selection:XMeshSaver_Settings.missingMaterialsHandling
			tooltip:"When an XMesh Loader is created, it attempts to represent the Materials of the Source Objects using a new Multi/Sub Material.\n\nBut when an object did not have a Material, it needs a stand-in representation within the Multi/Sub Material."

		dropdownlist ddl_ObjectVisibilityMode " When Visibility Less Than 1," offset:[10,1] width:140 align:#right \
			items:#("SAVE Anyway","SAVE If Positive","DO NOT Save") selection:XMeshSaver_Settings.ObjectVisibilityMode tooltip:"When an oject has Visibility below 1.0, XMesh Saver can either\nIgnore the Visibility and save anyway,\nSkip the object if the Visibility has any value below 1.0, or\nSkip the object if the Visibility has any value equal or less than 0.0."

		dropdownlist ddl_sourceObjectsHandling "When an XMesh Loader is created from saved Source Objects," offset:[-10,1] width:320 across:2 align:#left \
			items:#("DO NOTHING - Leave the Source Objects Untouched","FREEZE the Source Objects","Switch Source Objects to BOUNDING BOX display","HIDE the Source Objects","Turn OFF the Source Objects' Renderable property","Turn OFF Renderable property, set to BOUNDING BOX display") selection:XMeshSaver_Settings.sourceObjectsHandling \
			tooltip:"When an XMesh Loader is created, it would overlap with the Source Objects it represents.\n\nThis option lets you decide what to do with the Source Objects once the caching has finished."

		dropdownlist ddl_ObjectSpaceAnimationMode " When In Object Space," offset:[10,1] width:140 align:#right \
			items:#("BAKE Transforms","Link To SOURCE","Link To Source's PARENT","DO NOTHING") selection:XMeshSaver_Settings.sourceObjectsHandling tooltip:"When an XMesh Loader is created from a sequence saved in Object Space, the XMesh Loader's animation could be baked to keyframes, or it could be linked to the original source object or its parent to reproduce the node-level transformations."

		group "XMesh Loader Viewport Settings:"
		(
			dropdownlist ddl_xmeshLoaderViewport items:#("Display FULL Mesh", "Display BOUNDING BOX", "Display A Percentage Of VERTICES","Display A Percentage Of FACES","DISABLE Mesh Display") align:#left width:220 across:2 offset:[0,-1] \
				tooltip:"Controls the Viewport display mode of XMesh Loaders created or updated after the saving process has finished."
			spinner spn_XMeshLoaderViewPercent "Viewport Percentage:" range:[0.0,100.0,5.0] fieldwidth:50 align:#right
		)

		checkbutton chk_openLogOnError ">Open Log On Errors" width:115 across:4 align:#left offset:[-10,2] \
			tooltip:"When checked, the XMesh Saver Log Window will pop up automatically when there are Error messages.\n\nWhen unchecked, the XMesh Saver Log Window will not open even if there are Error messages."

		dropdownlist ddl_logLevel items:#("Log NONE","Log ERRORS","Log WARNINGS","Log PROGRESS","Log STATS","Log DEBUG") width:115 align:#center offset:[-3,2] \
			tooltip:"Sets the XMesh Saver Log Window Filter Level.\n\nIt can also be overridden from the Log Window's Menu."

		button btn_openLog "Open Log Window..." width:115  offset:[2,2] align:#center tooltip:"Opens the XMesh Saver Log Window..."
		button btn_configureLicense "Configure License..." width:115 align:#right offset:[10,2] tooltip:"Opens the Configure License Dialog..."

		on ddl_logLevel selected itm do XMeshSaverUtils.LoggingLevel = (itm-1)
		on chk_openLogOnError changed state do XMeshSaverUtils.PopupLogWindowOnError = state

		fn loadSettings fromDisk:true=
		(
			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "IgnoreEmpty")
				if theVal == OK do theVal = true
				XMeshSaver_Settings.ignoreEmpty = theVal
			)
			chk_ignoreEmpty.checked = XMeshSaver_Settings.ignoreEmpty

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Settings" "IgnoreTopology")
				if theVal == OK do theVal = true
				XMeshSaver_Settings.ignoreTopology = theVal
			)
			chk_ignoreTopology.checked = XMeshSaver_Settings.ignoreTopology

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Advanced" "MissingMaterialsHandling")
				if theVal == OK do theVal = 4
				XMeshSaver_Settings.missingMaterialsHandling = theVal
			)
			ddl_missingMaterialsHandling.selection = XMeshSaver_Settings.missingMaterialsHandling

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Advanced" "SourceObjectsHandling")
				if theVal == OK do theVal = 4
				XMeshSaver_Settings.sourceObjectsHandling = theVal
			)
			ddl_sourceObjectsHandling.selection = XMeshSaver_Settings.sourceObjectsHandling

			if fromDisk do
			(
				theVal = (getIniSetting theIniFile "Advanced" "ObjectVisibilityMode")
				if theVal == "" then theVal = 1 else theVal = execute theVal
				XMeshSaver_Settings.ObjectVisibilityMode = theVal
			)
			ddl_ObjectVisibilityMode.selection = XMeshSaver_Settings.ObjectVisibilityMode

			if fromDisk do
			(
				theVal = (getIniSetting theIniFile "Advanced" "ObjectSpaceAnimationMode")
				if theVal == "" then theVal = #bake else theVal = theVal as name
				XMeshSaver_Settings.ObjectSpaceAnimationMode = theVal
			)
			local theIndex = findItem #(#bake, #linkToSource, #linktoparent, #donothing) XMeshSaver_Settings.ObjectSpaceAnimationMode
			if theIndex == 0 do
			(
				XMeshSaver_Settings.ObjectSpaceAnimationMode = #bake
				theIndex = 1
			)
			ddl_ObjectSpaceAnimationMode.selection = theIndex

			chk_savePolymesh.checked = XMeshSaver_Settings.SavePolymesh

			chk_saveMetaData.checked = XMeshSaver_Settings.SaveMetaData
			chk_compressData.checked = XMeshSaverUtils.CompressionLevel != 0
			chk_OptimizeData.checked = XMeshSaver_Settings.DisableDataOptimization == false

			ddl_logLevel.selection = XMeshSaverUtils.LoggingLevel + 1
			chk_openLogOnError.state = XMeshSaverUtils.PopupLogWindowOnError

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Advanced" "XMeshLoaderViewport")
				if theVal == OK do theVal = 1
				XMeshSaver_Settings.xmeshLoaderViewport = theVal
			)
			ddl_xmeshLoaderViewport.selection = XMeshSaver_Settings.xmeshLoaderViewport

			if fromDisk do
			(
				theVal = execute (getIniSetting theIniFile "Advanced" "XMeshLoaderViewPercent")
				if theVal == OK do theVal = 5.0
				XMeshSaver_Settings.XMeshLoaderViewPercent = theVal
			)
			spn_XMeshLoaderViewPercent.value = XMeshSaver_Settings.XMeshLoaderViewPercent
			spn_XMeshLoaderViewPercent.enabled = findItem #(3,4) XMeshSaver_Settings.xmeshLoaderViewport  > 0
		)

		on ddl_ObjectVisibilityMode selected itm do
		(
			XMeshSaver_Settings.ObjectVisibilityMode = itm
			setIniSetting theIniFile "Advanced" "ObjectVisibilityMode" (XMeshSaver_Settings.ObjectVisibilityMode as string)
		)

		on ddl_ObjectSpaceAnimationMode selected itm do
		(
			XMeshSaver_Settings.ObjectSpaceAnimationMode  = #(#bake, #linktosource, #linktoparent, #donothing)[itm]
			setIniSetting theIniFile "Advanced" "ObjectSpaceAnimationMode" (XMeshSaver_Settings.ObjectSpaceAnimationMode as string)
		)

		on ddl_xmeshLoaderViewport selected itm do
		(
			setIniSetting theIniFile "Advanced" "XMeshLoaderViewport" (itm as string)
			XMeshSaver_Settings.xmeshLoaderViewport  = itm
			spn_XMeshLoaderViewPercent.enabled = findItem #(3,4) XMeshSaver_Settings.xmeshLoaderViewport  > 0
		)
		on spn_XMeshLoaderViewPercent changed val do
		(
			setIniSetting theIniFile "Advanced" "XMeshLoaderViewPercent" (val as string)
			XMeshSaver_Settings.XMeshLoaderViewPercent = val
		)

		on chk_savePolymesh changed state do
		(
			XMeshSaver_Settings.SavePolymesh = state
		)

		on chk_compressData changed state do
		(
			XMeshSaverUtils.CompressionLevel = (if state then 1 else 0)
		)

		on chk_OptimizeData changed state do
		(
			XMeshSaver_Settings.DisableDataOptimization = not state
		)

		on chk_saveMetaData changed state do
		(
			XMeshSaver_Settings.SaveMetaData = state
		)

		on btn_openLog pressed do
		(
			XMeshSaverUtils.LogWindowVisible = true
			XMeshSaverUtils.FocusLogWindow()
		)

		on btn_configureLicense pressed do
		(
			XMeshSaverUtils.ConfigureLicense()
		)

		on chk_ignoreTopology changed val do
		(
			XMeshSaver_Settings.ignoreTopology = val
			setIniSetting theIniFile "Settings" "IgnoreTopology" (val as string)
		)
		on chk_ignoreEmpty changed val do
		(
			XMeshSaver_Settings.ignoreEmpty = val
			setIniSetting theIniFile "Settings" "IgnoreEmpty" (val as string)
		)


		on ddl_missingMaterialsHandling selected itm do
		(
			XMeshSaver_Settings.missingMaterialsHandling = itm
			setIniSetting theIniFile "Advanced" "MissingMaterialsHandling" (itm as string)
		)

		on ddl_sourceObjectsHandling selected itm do
		(
			XMeshSaver_Settings.sourceObjectsHandling = itm
			setIniSetting theIniFile "Advanced" "SourceObjectsHandling" (itm as string)
		)

		on advancedSettings_rollout open do
		(
			loadSettings()
		)

		on advanceSettings_rollout close do
		(
			XMeshSaverUtils.ReleaseLicense()
		)

		on advancedSettings_rollout rolledUp val do
		(
			loadSettings()
			XMesh_saveParams_rollout.resizeDialog (getDialogSize XMesh_saveParams_rollout)
		)
	)

	rollout deadlineParams_rollout "Submit Job to Deadline" category:200
	(
		global SMTDSettings, SMTDFunctions, SMTDPaths

		edittext edt_jobname "Job Base Name:" fieldwidth:360 align:#left offset:[-2,0]
		edittext edt_comment "Comment:" fieldwidth:386 align:#left offset:[0,0]

		edittext edt_user "Name:" fieldwidth:130 align:#left offset:[18,0] across:2
		edittext edt_dept "Department:" fieldwidth:179 align:#right offset:[-1,0]

		group ""
		(
			label lbl_pools "Pool:" align:#left across:4 offset:[25,-3]
			dropdownList ddl_poollist "" width:170 align:#center offset:[-27,-6]
			label lbl_groups "Group:" align:#center offset:[-26,-3]
			dropdownList ddl_grouplist "" width:170 align:#right offset:[1,-6]

			label lbl_priority "Priority:" align:#left offset:[11,2] across:3
			progressbar sld_priority width:342 height:18 range:[0,100,50] type:#integer align:#center offset:[4,0]
			spinner spn_priority "" type:#integer fieldwidth:35 align:#right  offset:[-1,1]
		)

		spinner spn_numberOfTasks "Number of Parallel Tasks: " range:[1,1000,1] type:#integer  align:#left  fieldwidth:35 offset:[0,4] across:2 tooltip:"Splits the animation range in the given number of segments and creates one task per segment in a single job.\n\nThis allows the parallel processing on multiple Workers up to the 'Number Of Machines Working Concurrently' value."
		spinner spn_machineLimit "Number of Machines Working Concurrently: " range:[0,1000,1] type:#integer align:#right  fieldwidth:35 offset:[1,4]  tooltip:"When processing sub-ranges of the animation in parallel according to the 'Number Of Parallel Tasks' value, defines the maximum number of Workers to dequeue tasks at the same time."

		button btn_submit "SUBMIT MESH SAVING JOB TO DEADLINE" width:460 height:45 align:#center offset:[0,8]

		label lbl_getDeadline01 "DEADLINE WAS NOT DETECTED ON YOUR SYSTEM!" visible:false pos:[15,15]

		fn GetDeadlineNetworkRoot =
		(
			local theNetworkRoot = undefined
			try
			(
				local result = -2

				local submitOutputFile = sysInfo.tempdir + "submitOutput.txt"
				local submitExitCodeFile = sysInfo.tempdir + "submitExitCode.txt"

				deleteFile submitOutputFile
				deleteFile submitExitCodeFile

				local deadlinePath = systemTools.getEnvVariable "DEADLINE_PATH"
				local deadlineCommandFile = "deadlinecommandbg.exe"
				if deadlinePath != undefined do
				(
					deadlineCommandFile = deadlinePath + "\\" + deadlineCommandFile
				)

				local commandArguments = "-outputfiles \"" + submitOutputFile + "\" \"" + submitExitCodeFile + "\" -getrepositoryroot"
				local didLaunch = ShellLaunch deadlineCommandFile commandArguments

				if didLaunch do
				(
					local startTimeStamp = timestamp()
					local ready = false
					while not ready do
					(
						sleep 0.15
						if doesFileExist submitExitCodeFile do
						(
							local theFile = openFile submitExitCodeFile
							try(result = readValue theFile)catch(result = -2)
							try(close theFile)catch()
							ready = true
						)
						if timestamp() - startTimeStamp > 10000 then
						(
							result = -3
							ready = true
						)
					)

					if( result == 0 ) then
					(
						local resultFile = OpenFile submitOutputFile
						local resultMsg = ""
						if (resultFile != undefined) do
						(
							try(resultMsg = readLine resultFile)catch()
							try(close resultFile)catch()
						)

						theNetworkRoot = resultMsg
					)
					else
					(
						if result == -3 then
							XMeshSaverUtils.LogError "XMesh Saver - Timed out getting Repository Root from Deadline Command. (error code: 1003)"
						else
							XMeshSaverUtils.LogError "XMesh Saver - Failed to get Repository Root from Deadline Command. (error code: 1004)"
					)
				)
			)
			catch
			(
				XMeshSaverUtils.LogError "XMesh Saver - Error calling Deadline Command to get Repository Root. (error code: 1005)"
			)
			theNetworkRoot
		)

		fn getFileInfoDotNet theFileName =
		(
			local fileLookup = dotnetobject "System.IO.DirectoryInfo" (getFileNamePath theFileName)
			local allMatchingFiles = #()
			try (
				allMatchingFiles = fileLookup.getFiles (filenamefrompath theFileName)
			) catch()
			if allMatchingFiles.count == 1 then
			(
				local dotNetFile = allMatchingFiles[1]
				local fname = dotNetFile.FullName
				local date_ = dotNetFile.lastWriteTime.ToString()
				local size_ = dotNetFile.length
				#(fname, date_, size_)
			)
			else
				#()
		)

		fn LoadDeadline =
		(
			local startTimeStamp = timestamp()

			local success = false

			if SMTDPaths == undefined then
			(
				setWaitCursor()

				local theNetworkRoot = GetDeadlineNetworkRoot()
				if theNetworkRoot != undefined do
				(
					try
					(
						local theFileName = "SubmitMaxToDeadline_Functions.ms"
						local theSource = theNetworkRoot + "\\submission\\3dsmax\\Main\\" + theFileName
						if not doesFileExist theSource do theSource = theNetworkRoot + "\\submission\\3dsmax\\" + theFileName
						local theTargetDir = (GetDir #userscripts)
						local theTarget = (theTargetDir + "\\" + theFileName)
						if doesFileExist theSource then
						(
							local theInfo1 = getFileInfoDotNet theSource
							local theInfo2 = #("","","")
							if doesFileExist theTarget do
								theInfo2 = getFileInfoDotNet theTarget

							if theInfo1[2] != theInfo2[2] or theInfo1[3] != theInfo2[3] then
							(
								deleteFile theTarget
								copyFile theSource theTarget
							)
							local fileInTimeStamp = timeStamp()
							fileIn theTarget quiet:true

							success = true
						)
						else
						(
							XMeshSaverUtils.LogError("XMesh Saver - Failed To Find the file [" + theFileName + "] in Deadline Repository.")
						)
					)
					catch
					(
						XMeshSaverUtils.LogError("XMesh Saver - Failed To Load the file [" + theFileName + "] from Deadline Repository.")
					)
				)

				setArrowCursor()
			)
			else
			(
				success = true
			)
			success
		)

		fn job_priority_update val =
		(
			if val <= 100 do
			(
				theRed = (255.0 - 255.0*val/100.0)*2.0
				if theRed > 255 do theRed = 255
				theGreen = 512.0*val/100.0
				if theGreen > 255 do theGreen = 255
				sld_priority.color = [theRed, theGreen, 0]
				sld_priority.value = spn_priority.value = val
			)
			val
		)

		fn jobNameFromMaxName meshName =
		(
			local theName = getFileNameFile maxFileName
			if theName == "" do theName = "Untitled 3ds Max Mesh Saver Job"
			theName += " - [" + meshName + "]"
			SMTDSettings.jobName = edt_jobname.text = theName
		)

		fn saveMaxFileCopyForDeadline =
		(
			local TempMaxFile = SMTDPaths.TempDir + maxFileName
			if maxFileName == "" do TempMaxFile += "Untitled.max"
			if (doesFileExist TempMaxFile) do deleteFile TempMaxFile
			SMTDFunctions.SaveMaxFileCopy TempMaxFile
			TempMaxFile
		)

		on edt_jobname entered txt do SMTDSettings.jobName = txt
		on edt_comment entered txt do SMTDSettings.comment = txt

		on spn_priority changed value do
		(
			job_priority_update value
			setIniSetting theIniFile "Deadline" "Priority" (value as string)
		)

		on sld_priority clicked value do
		(
			job_priority_update value
			setIniSetting theIniFile "Deadline" "Priority" (value as string)
		)

		on ddl_poollist selected itm do
		(
			setIniSetting theIniFile "Deadline" "PoolName" (ddl_poollist.selected)
		)

		on ddl_grouplist selected itm do
		(
			setIniSetting theIniFile "Deadline" "Group" (ddl_grouplist.selected)
		)

		on spn_machineLimit changed val do
		(
			setIniSetting theIniFile "Deadline" "MachineLimit" (val as string)
			if val > spn_numberOfTasks.value do
			(
				spn_numberOfTasks.value = val
				setIniSetting theIniFile "Deadline" "NumberOfTasks" (spn_numberOfTasks.value as string)
			)
		)

		on spn_numberOfTasks changed val do
		(
			XMeshSaver_Settings.rangeSegments = val
			setIniSetting theIniFile "Deadline" "NumberOfTasks" (val as string)
			if val < spn_machineLimit.value do
			(
				spn_machineLimit.value = val
				setIniSetting theIniFile "Deadline" "MachineLimit" (spn_machineLimit.value as string)
			)
		)

		on btn_submit pressed do
		(
			totalSubmissionSuccessful = true

			-- copy values from controls to SMTDSettings
			local oldJobName = SMTDSettings.jobName
			local oldComment = SMTDSettings.comment
			local oldUsername = SMTDSettings.UserName
			local oldDepartment = SMTDSettings.Department
			local oldPoolName = SMTDSettings.PoolName
			local oldGroup = SMTDSettings.Group
			local oldPriority = SMTDSettings.Priority
			local oldMachineLimit = SMTDSettings.MachineLimit
			local oldChunkSize = SMTDSettings.ChunkSize
			local oldSubmitSceneMode = #reposave
			try(oldSubmitSceneMode = SMTDSettings.SubmitSceneMode )catch()

			SMTDSettings.jobName = edt_jobname.text
			SMTDSettings.comment = edt_comment.text
			SMTDSettings.UserName = edt_user.text
			SMTDSettings.Department = edt_dept.text
			SMTDSettings.PoolName = ddl_poollist.selected
			SMTDSettings.Group = ddl_grouplist.selected
			SMTDSettings.Priority = spn_priority.value
			SMTDSettings.MachineLimit = spn_machineLimit.value
			SMTDSettings.ChunkSize = 1
			try(SMTDSettings.SubmitSceneMode = #reposave )catch() --if running DL 5.1 or higher, make sure the MAX file is sent with the job

			for p in getPropNames SMTDSettings where matchPattern (p as string) pattern:"ExportAdvancedRenderInfo*" do
				setProperty SMTDSettings p false

			SMTDSettings.SubmitAsMXSJob = true
			SMTDSettings.ForceWorkstationMode = false

			SMTDSettings.SequentialJob = true
			SMTDSettings.LimitEnabled = SMTDSettings.MachineLimit > 0

			local theMeshesToSave = GetMeshesToSave()
			XMeshSaverUtils.ClearAllMaterialIDMapping()

			local noMatList = #()
			local theMatLibPath = ""
			if XMeshSaver_Settings.createMaterialLibrary and XMeshSaver_Settings.saveMode == 1 do
			(
				local filePath = buildCacheFileName numobjects:theMeshesToSave.count
				makeDir (getFileNamePath filePath) all:true
				local theMaterial = undefined
				if XMeshSaver_Settings.ObjectSourceMode == 9 then
				(
					local theTPToSave = theMeshesToSave[1][3]
					--theMaterial = theTPToSave.material
					theMaterial = XMeshSaver_MaterialUtils.getMaterialFromNodes #(theTPToSave)
				)
				else
				(
					--Handle Objects with Missing Materials:
					noMatList = for o in theMeshesToSave where o.material == undefined collect o
					case XMeshSaver_Settings.missingMaterialsHandling of
					(
						2: standInMaterial = Standard name:"NoMaterial Stand-in"
						3: standInMaterial = Standard name:"NoMaterial Red Stand-in" diffusecolor:red selfIllumAmount:100
					)
					for o in noMatList do
					(
						case XMeshSaver_Settings.missingMaterialsHandling of
						(
							1: () --do nothing
							default: (o.material = standInMaterial) --2 or 3
							4: o.material = Standard diffusecolor:o.wirecolor
						)
					)
					theMaterial = XMeshSaver_MaterialUtils.getMaterialFromNodes theMeshesToSave
				)

				if theMaterial != undefined do
				(
					theMaterial.name = "XMesh_MultiMaterial"
					theMatLibPath = (getFileNamePath filePath + getFileNameFile filePath + ".mat")
					deleteFile theMatLibPath
					if theMaterial.numsubs > 1 do
					(
						local theMatLib = materialLibrary()
						append theMatLib theMaterial
						saveTempMaterialLibrary theMatLib theMatLibPath
						local theMatIDmapFile = getFileNamePath theMatLibPath + getFileNameFile theMatLibPath + ".matIDmap"
						SaveMatIDMapFile theMaterial theMatIDmapFile
					)
				)
			)

			local saveSingleMaxFile = true
			if XMeshSaver_Settings.saveProxy and CanMakeProxyModifyTheScene() do
				saveSingleMaxFile = false
			if XMeshSaver_Settings.ObjectSourceMode == 9 do -- TP Groups
				saveSingleMaxFile = false

			local tempMaxFile = ""
			if saveSingleMaxFile do
				tempMaxFile = saveMaxFileCopyForDeadline()

			local tempMetadataFilename = "xmesh_metadata.xml"
			local tempMetadataPath = SMTDPaths.TempDir + tempMetadataFilename

			for o in noMatList do o.material = undefined

			local setSceneRenderList = if (XMeshSaver_Settings.ObjectSourceMode == 9) then #(false) else (if XMeshSaver_Settings.saveProxy then #(false,true) else #(true))

			case XMeshSaver_Settings.saveMode of
			(
				1:
				(
					local theNodesToSave = if (XMeshSaver_Settings.ObjectSourceMode == 9) then (#(GetMeshToSaveNode theMeshesToSave 1)) else theMeshesToSave
					for setSceneRender in setSceneRenderList do
					(
						XMeshSaverUtils.LogProgress "---------------------------------------------------------------------------------------------------------------------------------"
						if setSceneRender then
							XMeshSaverUtils.LogProgress "Starting RENDER MESH SAVING Deadline Job Submission..."
						else
							XMeshSaverUtils.LogProgress "Starting PROXY MESH SAVING Deadline Job Submission..."

						if setSceneRender then
						(

							local saveVelocity = (findItem XMeshSaver_Settings.activeSourceChannels "Velocity") != 0
							local sourceChannelsString = buildCommaSeparatedStringFromArray XMeshSaver_Settings.activeSourceChannels
								if sourceChannelsString == "" do sourceChannelsString = ","
						)
						else
						(
							--filter active channels that are allowed in proxy mode
							local tempArray = (for aChannel in XMeshSaver_Settings.ActiveSourceChannels where findItem XMeshSaver_Settings.proxyActiveSourceChannels aChannel > 0 collect aChannel)
							local saveVelocity = (findItem tempArray "Velocity") != 0
							local sourceChannelsString = buildCommaSeparatedStringFromArray tempArray
								if sourceChannelsString == "" do sourceChannelsString = ","
						)

						if not setSceneRender do
							MakeProxy theMeshesToSave
						try (
							local resetGroups = RunToDoNothing()
							if XMeshSaver_Settings.ObjectSourceMode == 9 do
								resetGroups = EnableTPGroups theNodesToSave[1] ( for i in theMeshesToSave collect i[1] )
							try (
								if not saveSingleMaxFile do
									tempMaxFile = saveMaxFileCopyForDeadline()
							) catch (
								resetGroups.run()
								throw()
							)
							resetGroups.run()
						) catch (
							if not setSceneRender do
								RemoveProxy theMeshesToSave
							throw()
						)
						if not setSceneRender do
							RemoveProxy theMeshesToSave

						prepareMetaData theMeshesToSave
						if (doesFileExist tempMetadataPath) do deleteFile tempMetadataPath
						XMeshSaverUtils.SaveMetadata tempMetadataPath


						local filePath = buildCacheFileName numobjects:theMeshesToSave.count

						if setSceneRender do
						(
							local theProxyPath = ""
							if XMeshSaver_Settings.saveProxy do getProxyPath filePath
							makeDir (getFileNamePath filePath) all:true
							saveOptions_rollout.createXMeshLoaderScript filePath theMeshesToSave theProxyPath:theProxyPath theFrameRange:#(XMeshSaver_Settings.frameStart,XMeshSaver_Settings.frameEnd) theMatLibPath:theMatLibPath
						)

						if not setSceneRender and setSceneRenderList.count > 1 then
							filePath = getProxyPath filePath
						SMTDFunctions.CreateJobInfoFile SMTDPaths.JobInfofile renderOutputOverride:(filePath)
						makeDir (getFileNamePath filePath) all:true
						local theJobInfofile = openFile SMTDPaths.JobInfofile mode:"at"
						format "xmesh_FilePath=%\n" filePath to:theJobInfofile
						format "xmesh_SaveMode=%\n" XMeshSaver_Settings.saveMode to:theJobInfofile
						format "xmesh_ObjectSourceMode=%\n" XMeshSaver_Settings.objectSourceMode to:theJobInfofile
						format "xmesh_SaveVelocity=%\n" saveVelocity to:theJobInfofile
						format "xmesh_IgnoreTopology=%\n" XMeshSaver_Settings.ignoreTopology to:theJobInfofile
						format "xmesh_IgnoreEmpty=%\n" XMeshSaver_Settings.ignoreEmpty to:theJobInfofile
						format "xmesh_SavePolymesh=%\n" XMeshSaver_Settings.SavePolymesh to:theJobInfofile
						format "xmesh_DisableDataOptimization=%\n" XMeshSaver_Settings.DisableDataOptimization to:theJobInfofile
						format "xmesh_SaveMetaData=%\n" XMeshSaver_Settings.SaveMetaData to:theJobInfofile
						format "xmesh_EnableMaterialIDMapping=%\n" XMeshSaver_Settings.createMaterialLibrary to:theJobInfofile
						format "xmesh_SetSceneRender=%\n" setSceneRender to:theJobInfofile
						format "xmesh_MetadataFile=%\n" tempMetadataFilename to:theJobInfofile
						format "xmesh_SourceChannels=%\n" sourceChannelsString to:theJobInfoFile
						format "xmesh_CoordinateSystem=%\n" (XMeshSaver_Settings.CoordinateSystem as String) to:theJobInfoFile

						format "xmesh_StartFrame=%\n" XMeshSaver_Settings.frameStart to:theJobInfofile
						format "xmesh_EndFrame=%\n" XMeshSaver_Settings.frameEnd to:theJobInfofile
						format "xmesh_SamplingStep=%\n" XMeshSaver_Settings.frameStep to:theJobInfofile
						format "xmesh_ProxySamplingStep=%\n" XMeshSaver_Settings.ProxyFrameStep to:theJobInfofile
						format "xmesh_useProxyFrameStep=%\n" XMeshSaver_Settings.useProxyFrameStep to:theJobInfofile
						format "xmesh_ObjectVisibilityMode=%\n" XMeshSaver_Settings.ObjectVisibilityMode to:theJobInfofile

						format "xmesh_RangeSegments=%\n" XMeshSaver_Settings.rangeSegments to:theJobInfofile

						SMTDSettings.jobName = edt_jobName.text + " [multiple meshes]" + (if (setSceneRender or setSceneRenderList.count < 2) then "" else "[proxy]")
						oldRendTimeType = rendTimeType
						rendTimeType = 4
						SMTDFunctions.CreateSubmitInfoFile SMTDPaths.SubmitInfofile customFrameSequence:("1-"+XMeshSaver_Settings.rangeSegments as string) customOutputFile:(filePath)
						rendTimeType = oldRendTimeType

						format "xmesh_MeshNodeCount=%\n" (theNodesToSave.count) to:theJobInfofile
						for idx = 1 to theNodesToSave.count do
						(
							format "xmesh_MeshNodeHandle%=%\n" idx theNodesToSave[idx].inode.handle to:theJobInfoFile
							format "xmesh_MeshNodeName%=%\n" idx theNodesToSave[idx].name to:theJobInfoFile
						)
						close theJobInfofile

						local initialArgs = " \"" + SMTDPaths.SubmitInfofile + "\" \"" + SMTDPaths.JobInfofile  + "\" \"" + TempMaxFile  + "\"" + " "
						initialArgs += "\"" + XMeshSaverUtils.XMeshSaverHome + "Scripts\\XMeshSaverDeadlineScriptJob.ms\"" + " "
						initialArgs += "\"" + tempMetadataPath + "\""

						local retCode = SMTDFunctions.waitForCommandToComplete initialArgs 3600

						SMTD_LastMessage = SMTDFunctions.getRenderMessage()
						if retCode != #success then
						(
							XMeshSaverUtils.LogProgress "---------------------------------------------------------------------------------------------------------------------------------"
							local theReportSS = SMTD_LastMessage as stringStream
							while not eof theReportSS do
								XMeshSaverUtils.LogError  (readLine theReportSS)
							XMeshSaverUtils.LogError ("ERROR Submitting XMesh Saver Job to Deadline for "+theNodesToSave.count as string+" "+(if theNodesToSave.count == 1 then " Object" else " Objects"))
							messageBox SMTD_LastMessage title:"ERROR Submitting XMesh Saver Job to Deadline!"
						)
						else
						(
							XMeshSaverUtils.LogProgress "---------------------------------------------------------------------------------------------------------------------------------"
							local theReportSS = SMTD_LastMessage as stringStream
							while not eof theReportSS do
								XMeshSaverUtils.LogProgress  (readLine theReportSS)
							XMeshSaverUtils.LogProgress ("Job Submission to Deadline for "+theNodesToSave.count as string+" "+(if theNodesToSave.count == 1 then " Object" else " Objects")+" SUCCESSFUL!")
							messageBox SMTD_LastMessage title:"XMesh Saver Job Submission to Deadline SUCCESSFUL!"
						)
					)
					XMeshSaverUtils.LogProgress "==========================================================================================================================================================================="

				)
				default:
				(
					local saveMaxFilePerObject = (not saveSingleMaxFile) and (XMeshSaver_settings.ObjectSourceMode == 9)
					for setSceneRender in setSceneRenderList do
					(
						XMeshSaverUtils.LogProgress "---------------------------------------------------------------------------------------------------------------------------------"
						if setSceneRender then
							XMeshSaverUtils.LogProgress "Starting Deadline RENDER MESH SAVING Job Submission..."
						else
							XMeshSaverUtils.LogProgress "Starting Deadline PROXY MESH SAVING Job Submission..."

						if setSceneRender then
						(
							local saveVelocity = (findItem XMeshSaver_Settings.activeSourceChannels "Velocity") != 0
							local sourceChannelsString = buildCommaSeparatedStringFromArray XMeshSaver_Settings.activeSourceChannels
								if sourceChannelsString == "" do sourceChannelsString = ","
						)
						else
						(
							--filter active channels that are allowed in proxy mode
							local tempArray = (for aChannel in XMeshSaver_Settings.ActiveSourceChannels where findItem XMeshSaver_Settings.proxyActiveSourceChannels aChannel > 0 collect aChannel)
							local saveVelocity = (findItem tempArray "Velocity") != 0
							local sourceChannelsString = buildCommaSeparatedStringFromArray tempArray
								if sourceChannelsString == "" do sourceChannelsString = ","
						)

						if not saveMaxFilePerObject do
						(
							if not setSceneRender do
								MakeProxy theMeshesToSave
							try (
								if not saveSingleMaxFile do
									tempMaxFile = saveMaxFileCopyForDeadline()
							) catch (
								if not setSceneRender do
									RemoveProxy theMeshesToSave
								throw()
							)
							if not setSceneRender do
								RemoveProxy theMeshesToSave
						)
						for meshIndex = 1 to theMeshesToSave.count do
						(
							local aMesh = theMeshesToSave[meshIndex]
							local theMeshName = (GetMeshToSaveName theMeshesToSave meshIndex)
							local theMeshNode = (GetMeshToSaveNode theMeshesToSave meshIndex)
							if saveMaxFilePerObject do
							(
								local resetGroups = RunToDoNothing()
								if XMeshSaver_Settings.ObjectSourceMode == 9 do
									resetGroups = EnableTPGroups theMeshNode #(aMesh[1])
								try (
									tempMaxFile = saveMaxFileCopyForDeadline()
								) catch (
									resetGroups.run()
									throw()
								)
								resetGroups.run()
							)

							prepareMetaData #(aMesh)
							if (doesFileExist tempMetadataPath) do deleteFile tempMetadataPath
							XMeshSaverUtils.SaveMetadata tempMetadataPath

							local filePath = buildCacheFileName() --XMesh_saveParams_rollout.edt_basePath.text
							local theFilePath = getFilenamePath filePath
							local theFileName = getFilenameFile filePath
							local theFileType = getFilenameType filePath
							local theMeshName = validateObjectNameAsFileName theMeshName

							local theNewPath = if XMeshSaver_Settings.folderPerObject then
								(theFilePath + theMeshName + "\\" + theFileName + "_" + theMeshName + "_" + theFileType )
							else
								(theFilePath + theFileName + "_" + theMeshName + "_" + theFileType )

							if not setSceneRender and setSceneRenderList.count > 1 do
								theNewPath = getProxyPath theNewPath

							SMTDFunctions.CreateJobInfoFile SMTDPaths.JobInfofile renderOutputOverride:(theNewPath)
							local theJobInfofile = openFile SMTDPaths.JobInfofile mode:"at"

							makeDir (getFileNamePath theNewPath) all:true

							if XMeshSaver_Settings.createMaterialLibrary do
							(
								if setSceneRender == setSceneRenderList[setSceneRenderList.count] do
								(
									if theMeshNode.material != undefined do
									(
										local theMaterial = XMeshSaver_MaterialUtils.getMaterialFromNodes #(theMeshNode)
										local theMatLib = materialLibrary()
										append theMatLib theMaterial
										theMatLibPath = (getFileNamePath theNewPath + getFileNameFile theNewPath+ ".mat")
										deleteFile theMatLibPath
										saveTempMaterialLibrary theMatLib theMatLibPath
										local theMatIDmapFile = getFileNamePath theNewPath + getFileNameFile theNewPath + ".matIDmap"
										SaveMatIDMapFile theMaterial theMatIDmapFile
									)
								)
							)

							if setSceneRender do
							(
								local theProxyPath = ""
								if XMeshSaver_Settings.saveProxy do getProxyPath theNewPath
								saveOptions_rollout.createXMeshLoaderScript theNewPath #(theMeshNode) theProxyPath:theProxyPath theFrameRange:#(XMeshSaver_Settings.frameStart,XMeshSaver_Settings.frameEnd) theMatLibPath:theMatLibPath
							)

							format "xmesh_FilePath=%\n" theNewPath to:theJobInfofile
							format "xmesh_SaveMode=%\n" XMeshSaver_Settings.saveMode to:theJobInfofile
							format "xmesh_ObjectSourceMode=%\n" XMeshSaver_Settings.objectSourceMode to:theJobInfofile
							format "xmesh_SaveVelocity=%\n" saveVelocity to:theJobInfofile
							format "xmesh_IgnoreTopology=%\n" XMeshSaver_Settings.ignoreTopology to:theJobInfofile
							format "xmesh_IgnoreEmpty=%\n" XMeshSaver_Settings.ignoreEmpty to:theJobInfofile
							format "xmesh_SavePolymesh=%\n" XMeshSaver_Settings.SavePolymesh to:theJobInfofile
							format "xmesh_SaveMetaData=%\n" XMeshSaver_Settings.SaveMetaData to:theJobInfofile
							format "xmesh_DisableDataOptimization=%\n" XMeshSaver_Settings.DisableDataOptimization to:theJobInfofile
							format "xmesh_EnableMaterialIDMapping=%\n" XMeshSaver_Settings.createMaterialLibrary to:theJobInfofile
							format "xmesh_SetSceneRender=%\n" setSceneRender to:theJobInfofile
							format "xmesh_MetadataFile=%\n" tempMetadataFilename to:theJobInfofile
							format "xmesh_SourceChannels=%\n" sourceChannelsString to:theJobInfoFile
							format "xmesh_CoordinateSystem=%\n" (XMeshSaver_Settings.CoordinateSystem as String) to:theJobInfoFile

							format "xmesh_StartFrame=%\n" XMeshSaver_Settings.frameStart to:theJobInfofile
							format "xmesh_EndFrame=%\n" XMeshSaver_Settings.frameEnd to:theJobInfofile
							format "xmesh_SamplingStep=%\n" XMeshSaver_Settings.frameStep to:theJobInfofile
							format "xmesh_ProxySamplingStep=%\n" XMeshSaver_Settings.ProxyFrameStep to:theJobInfofile
							format "xmesh_useProxyFrameStep=%\n" XMeshSaver_Settings.useProxyFrameStep to:theJobInfofile
							format "xmesh_ObjectVisibilityMode=%\n" XMeshSaver_Settings.ObjectVisibilityMode to:theJobInfofile

							format "xmesh_RangeSegments=%\n" XMeshSaver_Settings.rangeSegments to:theJobInfofile

							SMTDSettings.jobName = edt_jobName.text + " [" + theMeshName + "]" + (if (setSceneRender or setSceneRenderList.count < 2) then "" else "[proxy]")
							-- change the rendTimeType to use SMTD's frame number handling
							oldRendTimeType = rendTimeType
							rendTimeType = 4
							SMTDFunctions.CreateSubmitInfoFile SMTDPaths.SubmitInfofile customFrameSequence:("1-"+XMeshSaver_Settings.rangeSegments as string) customOutputFile:(theNewPath)
							rendTimeType = oldRendTimeType
							format "xmesh_MeshNodeHandle=%\n" theMeshNode.inode.handle to:theJobInfofile
							close theJobInfofile

							local initialArgs = " \"" + SMTDPaths.SubmitInfofile + "\" \"" + SMTDPaths.JobInfofile  + "\" \"" + TempMaxFile  + "\" "
							initialArgs += "\"" + XMeshSaverUtils.XMeshSaverHome + "Scripts\\XMeshSaverDeadlineScriptJob.ms\"" + " "
							initialArgs += "\"" + tempMetadataPath + "\""

							local retCode = SMTDFunctions.waitForCommandToComplete initialArgs 3600

							SMTD_LastMessage = SMTDFunctions.getRenderMessage()
							if retCode != #success then
							(
								XMeshSaverUtils.LogProgress "---------------------------------------------------------------------------------------------------------------------------------"
								XMeshSaverUtils.LogError ("ERROR Submitting Job to Deadline for Object ["+theMeshNode.name+"]!")
								local theReportSS = SMTD_LastMessage as stringStream
								while not eof theReportSS do
									XMeshSaverUtils.LogError (readLine theReportSS)
								messageBox SMTD_LastMessage title:"ERROR Submitting XMesh Saver Job to Deadline!"
								totalSubmissionSuccessful = false
								exit
							)
							else
							(
								XMeshSaverUtils.LogProgress "---------------------------------------------------------------------------------------------------------------------------------"
								local theReportSS = SMTD_LastMessage as stringStream
								while not eof theReportSS do
									XMeshSaverUtils.LogProgress  (readLine theReportSS)
								XMeshSaverUtils.LogWindowVisible = true
								XMeshSaverUtils.FocusLogWindow()
								XMeshSaverUtils.LogProgress ("Job Submission to Deadline for Object ["+theMeshNode.name+"] SUCCESSFUL!")
							)
						)--end i loop
					)--end render/viewport state loop
					XMeshSaverUtils.LogProgress "==========================================================================================================================================================================="
					if totalSubmissionSuccessful do
						messageBox "The Submission To Deadline Was SUCCESSFUL!\nPlease see the XMesh Saver Log Window For Details." title:"XMesh Saver"
				)--end default
			)--end case
			XMeshSaverUtils.ClearAllMaterialIDMapping()

			SMTDSettings.jobName = oldJobName
			SMTDSettings.comment = oldComment
			SMTDSettings.UserName = oldUsername
			SMTDSettings.Department = oldDepartment
			SMTDSettings.PoolName = oldPoolName
			SMTDSettings.Group = oldGroup
			SMTDSettings.Priority = oldPriority
			SMTDSettings.MachineLimit = oldMachineLimit
			SMTDSettings.ChunkSize = oldChunkSize
			try(SMTDSettings.SubmitSceneMode = oldSubmitSceneMode )catch()

		)--end submit

		-- from SubmitMaxToDeadline.ms
		fn displayPoolList =
		(
			-- SMTDFunctions.CollectPools() returns a "" as the first pool
			-- If we leave it in, then we need to make sure that it's not
			-- selected later on.
			-- Unlike the SMTD rollout, we will filter it out now.
			--ddl_poollist.items = SMTDSettings.Pools

			poolList = #()
			for i in SMTDSettings.Pools do
				if i.count > 0 do
					append poolList i
			if poolList.count == 0 do
				append poolList "none"

			ddl_poollist.items = poolList

			local theIndex = findItem ddl_poollist.items SMTDSettings.PoolName
			if theIndex > 0 then
				ddl_poollist.selection = theIndex
			else
				ddl_poollist.selection = 1
		)

		-- from SubmitMaxToDeadline.ms
		fn displayGroupList =
		(
			--ddl_grouplist.items = SMTDSettings.Groups

			groupList = #()
			for i in SMTDSettings.Groups do
				if i.count > 0 do
					append groupList i
			if groupList.count == 0 do
				append groupList "none"

			ddl_grouplist.items = groupList

			local theIndex = findItem ddl_grouplist.items SMTDSettings.Group
			if theIndex > 0 then
				ddl_grouplist.selection = theIndex
			else
				ddl_grouplist.selection = 1
		)

		fn loadSettings =
		(
			local hasDeadline = LoadDeadline()

			edt_jobname.visible = edt_comment.visible = edt_user.visible = edt_dept.visible = hasDeadline
			lbl_pools.visible = ddl_poollist.visible = lbl_groups.visible = ddl_grouplist.visible = hasDeadline
			lbl_priority.visible = sld_priority.visible = spn_priority.visible = hasDeadline
			spn_machineLimit.visible = spn_numberOfTasks.visible = btn_submit.visible = hasDeadline

			lbl_getDeadline01.visible = lbl_getDeadline02.visible = lbl_getDeadline03.visible = lbl_getDeadline04.visible = url_getDeadline01.visible = not hasDeadline

			if hasDeadline do
			(
				SMTDFunctions.loadSettings()

				if ((maxVersion())[1] > 8000) do
				(
					if is64bitApplication() then
						SMTDSettings.MaxVersionToForce = "64bit"
					else
						SMTDSettings.MaxVersionToForce = "32bit"
				)

				theVal = execute (getIniSetting theIniFile "Deadline" "PoolExclusive")
				if theVal != OK do SMTDSettings.PoolExclusive = theVal
				theVal = execute (getIniSetting theIniFile "Deadline" "Priority")
				if theVal != OK do SMTDSettings.Priority= theVal
				SMTDSettings.PoolName= (getIniSetting theIniFile "Deadline" "PoolName")
				SMTDSettings.Group= (getIniSetting theIniFile "Deadline" "Group")
				theVal = execute (getIniSetting theIniFile "Deadline" "MachineLimit")
				if theVal != OK do SMTDSettings.MachineLimit  = theVal
				theVal = execute (getIniSetting theIniFile "Deadline" "NumberOfTasks")
				if theVal != OK do spn_numberOfTasks.value  = XMeshSaver_Settings.rangeSegments = theVal

				if SMTDSettings.Pools.count == 0 do
					SMTDFunctions.collectPools()
				displayPoolList()

				if SMTDSettings.Groups.count == 0 do
					SMTDFunctions.collectGroups()
				displayGroupList()

				job_priority_update SMTDSettings.Priority

				if filePath == "" then
					btn_submit.enabled = false
				else
					btn_submit.enabled = true

				local theName = getFileNameFile maxFileName
				if theName == "" do theName = "Untitled 3ds Max Mesh Saving Job"
				edt_jobname.text = theName

				edt_user.text = SMTDSettings.UserName
				edt_dept.text = SMTDSettings.Department
				spn_machineLimit.value = SMTDSettings.machineLimit
				--spn_chunkSize.value = SMTDSettings.chunkSize

				saveOptions_rollout.canSaveCheck()
			)
		)

		on deadlineParams_rollout rolledUp val do
		(
			if val do
			(
				loadSettings()
			)
			XMesh_saveParams_rollout.resizeDialog (getDialogSize XMesh_saveParams_rollout)
		)

		on deadlineParams_rollout open do
		(
			saveOptions_rollout.canSaveCheck()
			if deadlineParams_rollout.open do loadSettings()
		)
	)--end deadline rollout

	makeDir (XMeshSaverUtils.SettingsDirectory) all:true

	-- put up new rollout floater and add rollout to it.
	local thePosition = execute (getIniSetting theIniFile "Dialog" "Position")
	if thePosition == OK do thePosition = [100,100]
	local theSize = execute (getIniSetting theIniFile "Dialog" "Size" )
	if theSize == OK do theSize = [480,700]
	createDialog XMesh_saveParams_rollout 480 theSize.y thePosition.x thePosition.y style:#(#style_titlebar, #style_border, #style_sysmenu, #style_minimizebox, #style_maximizebox, #style_resizing) --menu:XMesh_MainMenu
	updateTitlebar()
	AddSubRollout XMesh_saveParams_rollout.sub_rollout saveOptions_rollout rolledUp:false
	AddSubRollout XMesh_saveParams_rollout.sub_rollout saveChannels_rollout rolledUp:true
	AddSubRollout XMesh_saveParams_rollout.sub_rollout advancedSettings_rollout rolledUp:true
	AddSubRollout XMesh_saveParams_rollout.sub_rollout deadlineParams_rollout rolledUp:true
	XMesh_saveParams_rollout.resizeDialog (getDialogSize XMesh_saveParams_rollout)
  )
)


MacroScript CreateXMeshLoader category:"Thinkbox" buttonText:"XMesh Loader" toolTip:"Create XMesh Loader" silentErrors:false icon:#("XMesh",1)
(
	try(XMeshLoader isselected:true )catch()
)

macroScript AboutXMesh category:"XMesh"
(
	global XMeshSaver_About_Dialog
	try(destroyDialog XMeshSaver_About_Dialog)catch()
	rollout XMeshSaver_About_Dialog "About XMesh MX"
	(
		label lbl_about10 "XMESH MX"
		label lbl_about20 "by Thinkbox Software, Inc." offset:[0,-3]

		edittext edt_version "Version:" text:"" readonly:true fieldwidth:150 align:#left across:2
		edittext edt_dlrdate "Plug-in DLL Date:" text:"" readonly:true fieldwidth:150 align:#right
		edittext edt_installationfolder "Installation Folder:" text:"" readonly:true width:515 align:#right offset:[0,-3]

		label lbl_attributions "3rd Party Software Attributions:" align:#left offset:[0,2] across:2
		button btn_copyattributions "Copy To Clipboard" align:#right height:20 offset:[0,-3]
		dotNetControl dnc_attributions "System.Windows.Forms.TextBox" width:520  height:400 offset:[0,-4] align:#center

		on btn_copyattributions pressed do
		(
			dnc_attributions.SelectAll()
			dnc_attributions.Copy()
		)

		on XMeshSaver_About_Dialog open do
		(
			edt_version.text = XMeshSaverUtils.VersionNumber
			local maxVer = (((maxVersion())[1]/1000)-2) as string
			if maxVer == "16" do maxVer = "15"
			if maxVer == "14" do maxVer = "13"
			edt_installationfolder.text = XMeshSaverUtils.XMeshSaverHome
			edt_dlrdate.text = try(getFileModDate (XMeshSaverUtils.XMeshSaverHome + "3dsMax20"+maxVer+"\\x64\\XMeshSaver.dlo"))catch("???")
			try
			(
				dnc_attributions.multiline = true
				dnc_attributions.AcceptsReturn = false
				dnc_attributions.AcceptsTab = false
				dnc_attributions.WordWrap = false
				dnc_attributions.ScrollBars= dnc_attributions.ScrollBars.Both

				local theColor = (colorman.getColor #window) * [255,255,270]
				dnc_attributions.BackColor = dnc_attributions.BackColor.fromARGB theColor.x theColor.y theColor.z

				local theColor = (colorman.getColor #windowtext) * 255
				dnc_attributions.ForeColor = dnc_attributions.ForeColor.fromARGB theColor.x theColor.y theColor.z

				local txt = XMeshSaverUtils.Attributions
				txt = substituteString txt  "\n" "\r\n"
				dnc_attributions.text = txt
			)
			catch
			(
				dnc_attributions.text = "No attributions text found in the XMesh Saver Plug-in DLL."
			)
		)
	)
	createDialog XMeshSaver_About_Dialog 540 510
)
