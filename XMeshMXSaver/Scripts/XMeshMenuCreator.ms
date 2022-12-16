-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
(
	local CurrentVersion = "20160908"
	makeDir (GetDir #plugcfg + "\\XMesh\\") all:true
	local isMenuRequested = not matchPattern (getIniSetting (GetDir #plugcfg + "\\XMesh\\XMeshPreferences.ini" ) "MenuSystem" "CreateMenu") pattern:"false"
	local theMainMenu = menuMan.getMainMenuBar()
	local theXMeshMenu = menuMan.findMenu "XMesh"
	local oldVersion = (getIniSetting (GetDir #plugcfg + "\\XMesh\\XMeshPreferences.ini" ) "MenuSystem" "LastVersion")
	if oldVersion == "" do oldVersion = "20100506"
	local needUpdate = oldVersion != CurrentVersion
	if theXMeshMenu != undefined and (not isMenuRequested or needUpdate) do
	(
		if needUpdate then
			format "--Old XMesh Menu Version % Found and Removed.\n" oldVersion
		else
			format "--XMesh Menu Found and Removed at User Request via the XMesh Preferences\n"
		menuMan.unRegisterMenu theXMeshMenu
		menuMan.updateMenuBar()
		theXMeshMenu = undefined
	)
	if theXMeshMenu == undefined and isMenuRequested do
	(
		if needUpdate then
			format "--Creating Updated XMesh Menu Version %\n" CurrentVersion
		else
			format "--XMesh Menu Not Found - Creating...\n"

		theXMeshMenu = menuMan.createMenu "XMesh"

		theMenuDefinitionArray = #(
			#(#menuitem,"XMeshSaver","Thinkbox","XMesh SAVER"),
			#(#separator),
			#(#menuitem,"CreateXMeshLoader","Thinkbox","Create XMesh LOADER")
		)
		local theSubMenu = undefined
		for aDef in theMenuDefinitionArray do
		(
			case aDef[1] of
			(
				#menuitem:
				(
					try(
						theAction = menuMan.createActionItem aDef[2] aDef[3]
						theAction.setTitle aDef[4]
						theAction.setUseCustomTitle true
						if theSubMenu == undefined then
							theXMeshMenu.addItem theAction -1
						else
							theSubMenu.addItem theAction -1
					)catch()
				)
				#separator:
				(
					try(
						theAction = menuMan.createSeparatorItem()
						if theSubMenu == undefined then
							theXMeshMenu.addItem theAction -1
						else
							theSubMenu.addItem theAction -1
					)catch()
				)
				#submenustart:
				(
					try(
						theSubMenu = menuMan.createMenu aDef[2]
					)catch()
				)
				#submenuend:
				(
					try(
						theAction = menuMan.createSubMenuItem (theSubMenu.getTitle()) theSubMenu
						theXMeshMenu.addItem theAction -1
					)catch()
					theSubMenu = undefined
				)
			)--end case
		)--end aDef loop
		theSubMenu = menuMan.createSubMenuItem "XMesh" theXMeshMenu
		theMainMenu.addItem theSubMenu -1
		menuMan.updateMenuBar()
		setIniSetting (GetDir #plugcfg + "\\XMesh\\XMeshPreferences.ini" ) "MenuSystem" "LastVersion" CurrentVersion
	)--end if
)--end script