-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
struct XMeshSaver_MaterialUtils (
	m_memoTPMaterialLibrary = MaterialLibrary(),
	m_memoTPMaterialIDRange = #(),

	fn getTPMaterialIDRanges theMaterial = (
		if classof theMaterial == MultiMaterial then (
			maxMaterialID = amax (theMaterial.materialIDList as Array)
			if maxMaterialID == undefined do
				maxMaterialID = 0
			#(0,maxMaterialID)
		) else if classof theMaterial == TP_Multi_Sub_Object then (
			local memoIndex = findItem m_memoTPMaterialLibrary theMaterial
			if memoIndex == 0 then (
				materialIDSize = 0
				childMaterialIDRanges = #(0)
				for i = 1 to theMaterial.materialList.count do (
					local r = getTPMaterialIDRanges theMaterial.materialList[i]
					materialIDSize += r[r.count]
					append childMaterialIDRanges materialIDSize
				)
				append m_memoTPMaterialLibrary theMaterial
				append m_memoTPMaterialIDRange childMaterialIDRanges
				childMaterialIDRanges
			) else (
				m_memoTPMaterialIDRange[memoIndex]
			)
		) else (
			#(0,1)
		)
	),

	fn getSubMaterial theMaterial theMaterialID = (
		if classof theMaterial == MultiMaterial then (
			local localMaterialID = 1 + ((mod (theMaterialID - 1) theMaterial.numsubs) as Integer)
			local i = findItem theMaterial.materialIDList localMaterialID
			if i > 0 then (
				getSubMaterial theMaterial.materialList[i] theMaterialID
			) else (
				undefined
			)
		) else if classof theMaterial == TP_Multi_Sub_Object then (
			local childMaterialIDRanges = getTPMaterialIDRanges theMaterial
			if childMaterialIDRanges.count > 1 and childMaterialIDRanges[childMaterialIDRanges.count] > 0 then (
				local localMaterialID = 1 + ((mod (theMaterialID - 1) childMaterialIDRanges[childMaterialIDRanges.count]) as Integer)
				local childIndex = undefined
				for i = 2 to childMaterialIDRanges.count while childIndex == undefined do (
					if localMaterialID <= childMaterialIDRanges[i] do (
						childIndex = i - 1
					)
				)
				if childIndex == undefined then (
					throw "getSubMaterial Internal Error: childIndex is undefined"
				) else (
					getSubMaterial theMaterial.materialList[childIndex] (theMaterialID - childMaterialIDRanges[childIndex])
				)
			) else (
				undefined
			)
		) else (
			theMaterial
		)
	),

	fn greatestCommonDivisor a b = (
		while b != 0 do (
			local temp = b
			b = ( mod a b ) as Integer
			a = temp
		)
		a
	),

	fn leastCommonMultiple a b = (
		a * b / ( greatestCommonDivisor a b )
	),

	fn getMaterialIDPeriod theMaterial thePeriod:1 = (
		if classof theMaterial == MultiMaterial then (
			thePeriod = leastCommonMultiple thePeriod theMaterial.numsubs
			for theSubMaterial in theMaterial.materialList do (
				thePeriod = getMaterialIDPeriod theSubMaterial thePeriod:thePeriod
			)
		) else if classof theMaterial == TP_Multi_Sub_Object then (
			local childMaterialIDRanges = getTPMaterialIDRanges theMaterial
			if childMaterialIDRanges.count > 1 and childMaterialIDRanges[childMaterialIDRanges.count] > 0 then (
				thePeriod = leastCommonMultiple thePeriod childMaterialIDRanges[childMaterialIDRanges.count]
			)
			for theSubMaterial in theMaterial.materialList do (
				thePeriod = getMaterialIDPeriod theSubMaterial thePeriod:thePeriod
			)
		)
		thePeriod = amin #(thePeriod, 65536)
	),

	fn findOrAddMaterial &materialList theMaterial = (
		matID = findItem materialList theMaterial
		if matID == 0 do (
			append materialList theMaterial
			matID = materialList.count
		)
		matID
	),

	fn getTPNonMultiMaterialIDCount theMaterial = (
		if theMaterial == undefined then (
			1
		) else if classof theMaterial == VRayMtlWrapper then (
			getTPNonMultiMaterialIDCount theMaterial.baseMtl
		) else (
			ss = StringStream ""
			showproperties theMaterial to:ss
			seek ss 0
			materialPropertyCount = 0
			while (skipToString ss ":") != undefined and (not eof ss) do (
				if trimLeft (readLine ss) == "material" then (
					materialPropertyCount += 1
				)
			)
			if materialPropertyCount == 0 then (
				materialPropertyCount = 1
			)
			materialPropertyCount
		)
	),

	fn buildTPMaterialIDMapping theMaterial &materialList &fromMatIDList &toMatIDList materialIDOffset:0 = (
		if theMaterial == undefined then (
			materialIDOffset += 1
			append fromMatIDList materialIDOffset
			append toMatIDList -1
		) else if classof theMaterial == MultiMaterial then (
			for matIndex = 1 to theMaterial.materialIDList.count do (
				local childMaterialID = theMaterial.materialIDList[matIndex]
				local childMaterial = getSubMaterial theMaterial childMaterialID
				if childMaterial == undefined then (
					append fromMatIDList (childMaterialID + materialIDOffset)
					append toMatIDList -1
				) else (
					append fromMatIDList (childMaterialID + materialIDOffset)
					append toMatIDList (findOrAddMaterial &materialList childMaterial)
				)
			)
			materialIDOffset += amax (theMaterial.materialIDList as Array)
		) else if classof theMaterial == TP_Multi_Sub_Object then (
			for childIndex = 1 to theMaterial.materialList.count do (
				materialIDOffset = buildTPMaterialIDMapping theMaterial.materialList[childIndex] &materialList &fromMatIDList &toMatIDList materialIDOffset:materialIDOffset
			)
		) else (
			materialIDCount = getTPNonMultiMaterialIDCount theMaterial
			for i = 1 to materialIDCount do (
				materialIDOffset += 1
				append fromMatIDList materialIDOffset
				append toMatIDList (findOrAddMaterial &materialList theMaterial)
			)
		)
		materialIDOffset
	),

	fn buildMaterialIDMapping theMaterial &materialList &fromMatIDList &toMatIDList progressBegin: progressEnd: = (
		if classof theMaterial == TP_Multi_Sub_Object then (
			buildTPMaterialIDMapping theMaterial &materialList &fromMatIDList &toMatIDList
		) else (
			local materialIDPeriod = getMaterialIDPeriod theMaterial
			for matID = 1 to materialIDPeriod do (
				progressUpdate (progressBegin + (progressEnd - progressBegin) * (matID - 1) / materialIDPeriod)
				local theChildMaterial = getSubMaterial theMaterial matID
				if theChildMaterial == undefined then (
					if matID == materialIDPeriod do (
						append fromMatIDList matID
						append toMatIDList -1
					)
				) else (
					append fromMatIDList matID
					append toMatIDList ( findOrAddMaterial &materialList theChildMaterial )
				)
			)
		)
	),

	fn getMaterialFromNodes theNodeList = (
		progressStart "Building Material Library..."

		XMeshSaverUtils.ClearAllMaterialIDMapping()

		local self = XMeshSaver_MaterialUtils()

		local materialList = #()

		local st = timestamp()

		for nodeIndex = 1 to theNodeList.count do (
			progressUpdate (100.0 * ((nodeIndex - 1.0) / theNodeList.count))
			local theNode = theNodeList[nodeIndex]
			if not isValidNode theNode do continue
			if theNode.material == undefined do continue
			local fromMatIDList = #()
			local toMatIDList = #()
			self.buildMaterialIDMapping theNode.material &materialList &fromMatIDList &toMatIDList progressBegin:(100.0 * (nodeIndex - 1) / theNodeList.count) progressEnd:(100.0 * nodeIndex / theNodeList.count)
			if fromMatIDList.count > 0 do (
				XMeshSaverUtils.SetMaterialIDMapping theNode fromMatIDList toMatIDList
			)
		)

		append materialList undefined
		XMeshSaverUtils.DefaultMaterialID = materialList.count

		if materialList.count > 1000 do (
			throw ("Too many materials: " + (materialList.count as String))
		)

		local materialIDList = (for i = 1 to materialList.count collect i)

		-- create the material
		outMaterial = MultiMaterial numsubs:materialList.count
		outMaterial.materialList = materialList
		outMaterial.materialIDList = materialIDList

		progressEnd()

		XMeshSaverUtils.LogStats ("Building Material Library Time: "+((timestamp()-st)/1000.0) as string+ " seconds.")

		outMaterial
	)
)

