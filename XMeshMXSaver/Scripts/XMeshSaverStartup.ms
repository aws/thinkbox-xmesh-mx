-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
-- need the path of the XMeshSaver script(s)
XMeshSaverScriptPath = XMeshSaverUtils.XMeshSaverHome + "Scripts\\"

if XMeshSaverScriptPath != undefined then
(
	if not (IsNetServer()) do --if running in workstation mode,
	(
		local iconFiles = #("XMesh_24i.bmp", "XMesh_24a.bmp", "XMesh_16i.bmp", "XMesh_16a.bmp")
		local updatedIcons = 0 --count the updates
		for i in iconFiles do --go through all icon files
		(
			local theTarget = (GetDir #usericons + "\\" + i) 
			local theSource = (XMeshSaverScriptPath + i) 
			if (doesFileExist theSource and doesFileExist theTarget and getFileModDate theTarget != getFileModDate theSource) or (not doesFileExist theTarget) then
			(
				deleteFile theTarget  
				local result = copyFile theSource theTarget  
				if result do updatedIcons += 1 
			)	
		)	
		if updatedIcons > 0 do colorMan.reiniticons() 
		fileIn (XMeshSaverScriptPath + "XMeshSaver.mcr")
	)
	fileIn (XMeshSaverScriptPath + "XMeshSaver_MaterialUtils.ms")
	callbacks.addScript #postSystemStartup "fileIn (XMeshSaverUtils.XMeshSaverHome+\"/Scripts/XMeshMenuCreator.ms\")" id:#xmeshsaverstartup
)