// Cached geometry только transient preview, до создания gameplay vehicle.
// Stock slot bounds исключают собственные props; этот меньший реальный объём
// отдельно проверяется со всеми препятствиями, включая props самого depot.
class AICF_LogisticsVehicleFootprint
{
	vector m_vMin;
	vector m_vMax;
	bool m_bValid;
	protected static ref map<ResourceName, ref AICF_LogisticsVehicleFootprint> s_mCache = new map<ResourceName, ref AICF_LogisticsVehicleFootprint>();

	static AICF_LogisticsVehicleFootprint Get(ResourceName prefab)
	{
		AICF_LogisticsVehicleFootprint result;
		if (s_mCache.Find(prefab, result)) return result;
		result = new AICF_LogisticsVehicleFootprint();
		s_mCache.Set(prefab, result);
		Resource resource = Resource.Load(prefab);
		IEntitySource source = SCR_BaseContainerTools.FindEntitySource(resource);
		if (!source) return result;
		array<ref SCR_BasePreviewEntry> entries = {};
		SCR_PrefabPreviewEntity.GetPreviewEntries(source, entries, flags: EPreviewEntityFlag.IGNORE_PREFAB);
		if (entries.IsEmpty() || entries.Count() > 128) return result;
		array<AICF_ConstructionGeometryPreview> nodes = {};
		bool failed;
		int meshes;
		foreach (SCR_BasePreviewEntry entry : entries)
		{
			if (entry.m_Shape == EPreviewEntityShape.PREFAB) { failed = true; break; }
			AICF_ConstructionGeometryPreview parent;
			if (entry.m_iParentID >= 0)
			{
				if (entry.m_iParentID >= nodes.Count()) { failed = true; break; }
				parent = nodes[entry.m_iParentID];
			}
			AICF_ConstructionGeometryPreview node = AICF_ConstructionGeometryPreview.Create(entry, parent);
			if (!node) { failed = true; break; }
			nodes.Insert(node);
			if (entry.m_Shape != EPreviewEntityShape.MESH || entry.m_Mesh.IsEmpty()) continue;
			if (!node.GetVObject()) { failed = true; break; }
			vector mins, maxs;
			node.GetWorldBounds(mins, maxs);
			for (int axis; axis < 3; axis++)
			{
				if (meshes == 0 || mins[axis] < result.m_vMin[axis]) result.m_vMin[axis] = mins[axis];
				if (meshes == 0 || maxs[axis] > result.m_vMax[axis]) result.m_vMax[axis] = maxs[axis];
			}
			meshes++;
		}
		for (int i = nodes.Count() - 1; i >= 0; i--)
		{
			if (nodes[i]) delete nodes[i];
		}
		result.m_bValid = !failed && meshes > 0 && result.m_vMax[0] > result.m_vMin[0] && result.m_vMax[2] > result.m_vMin[2];
		return result;
	}
}
