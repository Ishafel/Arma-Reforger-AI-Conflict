// Только transient preview entities без gameplay компонентов. Metadata строится
// один раз из полного prefab tree, включая linked children, и кэшируется.
class AICF_ConstructionVolume
{
	vector m_vMin;
	vector m_vMax;
}

class AICF_ConstructionExitPair
{
	ref AICF_ConstructionVolume m_Forward = new AICF_ConstructionVolume();
	ref AICF_ConstructionVolume m_Backward = new AICF_ConstructionVolume();
}

class AICF_ConstructionGeometryPreviewClass : SCR_BasePreviewEntityClass
{
}

class AICF_ConstructionGeometryPreview : SCR_BasePreviewEntity
{
	IEntitySource m_Source;
	EPreviewEntityShape m_eShape;
	ResourceName m_sMesh;
	bool m_bOutline;

	static AICF_ConstructionGeometryPreview Create(SCR_BasePreviewEntry entry, AICF_ConstructionGeometryPreview parent)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		entry.LoadTransform(params.Transform);
		if (entry.m_Shape != EPreviewEntityShape.ELLIPSE && entry.m_Shape != EPreviewEntityShape.RECTANGLE)
			Math3D.MatrixScale(params.Transform, entry.GetScale());
		AICF_ConstructionGeometryPreview node = AICF_ConstructionGeometryPreview.Cast(GetGame().SpawnEntity(AICF_ConstructionGeometryPreview, GetGame().GetWorld(), params));
		if (!node)
			return null;
		node.m_Source = entry.m_EntitySource;
		node.m_eShape = entry.m_Shape;
		node.m_sMesh = entry.m_Mesh;
		if (entry.m_Shape == EPreviewEntityShape.MESH && !entry.m_Mesh.IsEmpty())
		{
			Resource mesh = Resource.Load(entry.m_Mesh);
			if (mesh && mesh.GetResource())
				node.SetPreviewObject(mesh.GetResource().ToVObject(), "{58F07022C12D0CF5}Assets/Editor/PlacingPreview/Preview.emat");
		}
		if (parent)
		{
			int pivot = -1;
			if (!entry.m_iPivotID.IsEmpty() && parent.GetAnimation())
				pivot = parent.GetAnimation().GetBoneIndex(entry.m_iPivotID);
			parent.AddChild(node, pivot, EAddChildFlags.AUTO_TRANSFORM);
		}
		return node;
	}
}

class AICF_ConstructionMetadata
{
	ResourceName m_sPrefab;
	int m_iPrefabId = -1;
	AICF_EConstructionType m_eType;
	vector m_vMin;
	vector m_vMax;
	int m_iMeshes;
	int m_iSpawnSlots;
	int m_iRequiredServices;
	bool m_bValid;
	bool m_bGeometryError;
	bool m_bGeometryLoaded;
	ref array<vector> m_aServiceAnchors = {};
	ref array<ref AICF_ConstructionExitPair> m_aExitPairs = {};
	ref array<ref AICF_ConstructionVolume> m_aSolids = {};
	ref array<ref AICF_ConstructionVolume> m_aCollisionVolumes = {};
	ref array<EEditableEntityLabel> m_aLabels = {};
	protected ref array<ref Resource> m_aPreviewResources = {};
	protected ref array<ref SCR_BasePreviewEntry> m_aEntries = {};
	protected ref array<AICF_ConstructionGeometryPreview> m_aPreviewNodes = {};
	protected AICF_ConstructionGeometryPreview m_PreviewRoot;
	protected bool m_bGeometryPrepared;
	protected bool m_bGeometryCollected;
	protected int m_iGeometryCursor;
	protected int m_iGeometryCpuMs;
	protected int m_iOutlineStart;
	ResourceName m_sOutline;

	static EEditableEntityLabel ServiceLabel(AICF_EConstructionType type)
	{
		switch (type)
		{
			case AICF_EConstructionType.ARMORY: return EEditableEntityLabel.SERVICE_ARMORY;
			case AICF_EConstructionType.LIGHT_DEPOT: return EEditableEntityLabel.SERVICE_VEHICLE_DEPOT_LIGHT;
			case AICF_EConstructionType.HEAVY_DEPOT: return EEditableEntityLabel.SERVICE_VEHICLE_DEPOT_HEAVY;
		}
		return EEditableEntityLabel.SERVICE_LIVING_AREA;
	}

	static SCR_EServicePointType ServiceType(AICF_EConstructionType type)
	{
		switch (type)
		{
			case AICF_EConstructionType.ARMORY: return SCR_EServicePointType.ARMORY;
			case AICF_EConstructionType.LIGHT_DEPOT: return SCR_EServicePointType.LIGHT_VEHICLE_DEPOT;
			case AICF_EConstructionType.HEAVY_DEPOT: return SCR_EServicePointType.HEAVY_VEHICLE_DEPOT;
		}
		return SCR_EServicePointType.BARRACKS;
	}

	static bool HasOnlineService(IEntity entity, AICF_EConstructionType type)
	{
		if (!entity)
			return false;
		SCR_ServicePointComponent service = SCR_ServicePointComponent.Cast(entity.FindComponent(SCR_ServicePointComponent));
		if (service && service.GetType() == ServiceType(type) && service.GetServiceState() == SCR_EServicePointStatus.ONLINE)
			return true;
		IEntity child = entity.GetChildren();
		while (child)
		{
			if (HasOnlineService(child, type))
				return true;
			child = child.GetSibling();
		}
		return false;
	}

	bool IsDepot()
	{
		return m_eType == AICF_EConstructionType.LIGHT_DEPOT || m_eType == AICF_EConstructionType.HEAVY_DEPOT;
	}

	void Load(ResourceName prefab, AICF_EConstructionType type, SCR_CampaignBuildingManagerComponent manager, SCR_CampaignFaction faction)
	{
		m_sPrefab = prefab;
		m_eType = type;
		m_iPrefabId = manager.GetCompositionId(prefab);
		if (m_iPrefabId < 0)
			return;
		SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(prefab);
		if (!info || info.GetFactionKey() != faction.GetFactionKey() || !info.HasEntityLabel(faction.GetFactionLabel()) ||
			!info.HasEntityLabel(ServiceLabel(type)) || !info.HasEntityLabel(EEditableEntityLabel.TRAIT_SERVICE))
			return;
		info.GetEntityLabels(m_aLabels);
		if (!manager.GetOutlineManager())
			return;
		m_sOutline = manager.GetOutlineManager().AICF_ResolveOutline(prefab, info);
		if (m_sOutline.IsEmpty())
			return;
		m_bValid = true;
	}

	// -1 unsupported, 0 pending, 1 cached. Создание meshes распределено по ticks.
	int StepGeometry(int entriesPerTick, int sliceMs)
	{
		if (m_bGeometryLoaded)
		{
			if (m_bValid)
				return 1;
			return -1;
		}
		int before = System.GetTickCount();
		if (!m_bGeometryPrepared)
		{
			m_bGeometryPrepared = true;
			if (!PrepareEntries())
				m_bGeometryError = true;
		}
		int count;
		while (!m_bGeometryError && m_iGeometryCursor < m_aEntries.Count() && count++ < entriesPerTick && System.GetTickCount() - before < sliceMs)
		{
			SCR_BasePreviewEntry entry = m_aEntries[m_iGeometryCursor];
			AICF_ConstructionGeometryPreview parent;
			if (entry.m_iParentID >= 0)
			{
				if (entry.m_iParentID >= m_aPreviewNodes.Count())
				{
					m_bGeometryError = true;
					break;
				}
				parent = m_aPreviewNodes[entry.m_iParentID];
			}
			AICF_ConstructionGeometryPreview node = AICF_ConstructionGeometryPreview.Create(entry, parent);
			if (!node)
			{
				m_bGeometryError = true;
				break;
			}
			if (!m_PreviewRoot)
				m_PreviewRoot = node;
			node.m_bOutline = m_iGeometryCursor >= m_iOutlineStart;
			m_aPreviewNodes.Insert(node);
			m_iGeometryCursor++;
		}
		if (!m_bGeometryError && m_iGeometryCursor < m_aEntries.Count())
		{
			m_iGeometryCpuMs += System.GetTickCount() - before;
			return 0;
		}
		if (!m_bGeometryError && !m_bGeometryCollected && m_PreviewRoot)
		{
			Collect(m_PreviewRoot);
			m_bGeometryCollected = true;
			if (m_aCollisionVolumes.Count() > 256)
				m_bGeometryError = true;
		}
		if (!m_bGeometryError && !CompactCollisionVolumes(sliceMs))
		{
			m_iGeometryCpuMs += System.GetTickCount() - before;
			return 0;
		}
		ReleasePreview();
		m_iGeometryCpuMs += System.GetTickCount() - before;
		m_bGeometryLoaded = true;
		m_bValid = !m_bGeometryError && m_iMeshes > 0 && m_iRequiredServices > 0 && m_vMax[0] > m_vMin[0] && m_vMax[2] > m_vMin[2];
		if (m_aCollisionVolumes.IsEmpty())
			m_bValid = false;
		if (IsDepot() && (m_aExitPairs.IsEmpty() || m_aExitPairs.Count() > 4))
			m_bValid = false;
		AICF_Stage1Diagnostics.Info("CONSTRUCTION_METADATA", string.Format("prefab=%1 valid=%2 min=%3 max=%4 meshes=%5 anchors=%6 services=%7 spawn_slots=%8 cpu_ms=%9",
			m_sPrefab, m_bValid, m_vMin, m_vMax, m_iMeshes, m_aServiceAnchors.Count(), m_iRequiredServices, m_iSpawnSlots, m_iGeometryCpuMs));
		AICF_Stage1Diagnostics.Info("CONSTRUCTION_COLLISION_METADATA", string.Format("prefab=%1 volumes=%2", m_sPrefab, m_aCollisionVolumes.Count()));
		foreach (int index, AICF_ConstructionExitPair pair : m_aExitPairs)
		{
			AICF_Stage1Diagnostics.Info("CONSTRUCTION_EXIT_METADATA", string.Format("prefab=%1 slot=%2 forward_min=%3 forward_max=%4 backward_min=%5 backward_max=%6 forward_clear=%7 backward_clear=%8",
				m_sPrefab, index, pair.m_Forward.m_vMin, pair.m_Forward.m_vMax, pair.m_Backward.m_vMin, pair.m_Backward.m_vMax,
				ExitAvoidsComposition(pair.m_Forward), ExitAvoidsComposition(pair.m_Backward)));
		}
		if (m_bValid)
			return 1;
		return -1;
	}

	void ReleasePreview()
	{
		if (m_PreviewRoot)
			SCR_EntityHelper.DeleteEntityAndChildren(m_PreviewRoot);
		m_PreviewRoot = null;
		m_aPreviewNodes.Clear();
		m_aEntries.Clear();
		m_aPreviewResources.Clear();
	}

	protected bool PrepareEntries()
	{
		Resource resource = Resource.Load(m_sPrefab);
		if (!resource || !resource.IsValid())
			return false;
		IEntitySource source = SCR_BaseContainerTools.FindEntitySource(resource);
		if (!source || !SCR_CampaignBuildingCompositionComponentClass.GetCampaignBuildingCompositionSource(source))
			return false;
		m_aPreviewResources.Insert(resource);
		array<ref SCR_BasePreviewEntry> raw = {};
		SCR_PrefabPreviewEntity.GetPreviewEntries(source, raw, flags: EPreviewEntityFlag.IGNORE_PREFAB);
		ExpandEntries(raw, 0, -1, m_aEntries, 0, false);
		if (m_aEntries.IsEmpty() || m_aEntries.Count() > 2048)
			return false;
		// Полный stock layout добавляется к завершённой composition.
		Resource outline = Resource.Load(m_sOutline);
		IEntitySource outlineSource = SCR_BaseContainerTools.FindEntitySource(outline);
		if (!outlineSource)
			return false;
		m_aPreviewResources.Insert(outline);
		array<ref SCR_BasePreviewEntry> outlineEntries = {};
		SCR_PrefabPreviewEntity.GetPreviewEntries(outlineSource, outlineEntries, flags: EPreviewEntityFlag.IGNORE_PREFAB);
		m_iOutlineStart = m_aEntries.Count();
		ExpandEntries(outlineEntries, 0, 0, m_aEntries, 0, false);
		return !m_bGeometryError;
	}

	// Stock helper не передаёт IGNORE_PREFAB детям: повторное раскрытие исключает
	// сокращённую геометрию и gameplay PREFAB entries до создания preview nodes.
	protected void ExpandEntries(array<ref SCR_BasePreviewEntry> raw, int index, int parent,
		array<ref SCR_BasePreviewEntry> output, int depth, bool refresh = true)
	{
		if (depth > 32 || output.Count() >= 2048 || index >= raw.Count())
		{
			m_bGeometryError = true;
			return;
		}
		SCR_BasePreviewEntry entry = raw[index];
		if (entry.m_Resource)
			m_aPreviewResources.Insert(entry.m_Resource);
		if (refresh && entry.m_EntitySource)
		{
			for (int c; c < entry.m_EntitySource.GetComponentCount(); c++)
			{
				if (!entry.m_EntitySource.GetComponent(c).GetClassName().ToType().IsInherited(SCR_PreviewEntityComponent))
					continue;
				array<ref SCR_BasePreviewEntry> expanded = {};
				SCR_BasePreviewEntry root = new SCR_BasePreviewEntry(true);
				root.m_Resource = entry.m_Resource;
				root.m_vPosition = entry.m_vPosition;
				root.m_vAngles = entry.m_vAngles;
				root.m_vScale = entry.m_vScale;
				root.m_iPivotID = entry.m_iPivotID;
				SCR_PrefabPreviewEntity.GetPreviewEntries(entry.m_EntitySource, expanded, entry: root, flags: EPreviewEntityFlag.IGNORE_PREFAB);
				ExpandEntries(expanded, 0, parent, output, depth + 1, false);
				return;
			}
		}
		if (entry.m_Shape == EPreviewEntityShape.PREFAB)
		{
			m_bGeometryError = true;
			return;
		}
		int id = output.Count();
		SCR_BasePreviewEntry copy = new SCR_BasePreviewEntry();
		copy.CopyFrom(entry);
		copy.m_Resource = entry.m_Resource;
		copy.m_iParentID = parent;
		output.Insert(copy);
		for (int child = index + 1; child < raw.Count(); child++)
		{
			if (raw[child].m_iParentID == index)
				ExpandEntries(raw, child, id, output, depth + 1);
		}
	}

	protected void Collect(IEntity entity)
	{
		AICF_ConstructionGeometryPreview node = AICF_ConstructionGeometryPreview.Cast(entity);
		if (node && !node.m_sMesh.IsEmpty())
		{
			if (!node.GetVObject() || node.m_eShape != EPreviewEntityShape.MESH)
				m_bGeometryError = true;
			else
			{
				vector mins, maxs;
				node.GetWorldBounds(mins, maxs);
				Include(mins);
				Include(maxs);
				AddCollisionVolume(mins, maxs);
				if (!node.m_bOutline && maxs[1] > 0.5 && mins[1] < 4.1)
				{
					AICF_ConstructionVolume solid = new AICF_ConstructionVolume();
					solid.m_vMin = mins;
					solid.m_vMax = maxs;
					m_aSolids.Insert(solid);
				}
				m_iMeshes++;
			}
		}
		if (node && node.m_Source)
		{
			for (int i; i < node.m_Source.GetComponentCount(); i++)
			{
				IEntityComponentSource component = node.m_Source.GetComponent(i);
				string name = component.GetClassName();
				if (name.ToType().IsInherited(SCR_ServicePointComponent))
				{
					SCR_EServicePointType serviceType;
					if (component.Get("m_eType", serviceType) && serviceType == ServiceType(m_eType))
						m_iRequiredServices++;
				}
				if (name.ToType().IsInherited(SCR_EntitySpawnerSlotComponent))
				{
					vector slotMin, slotMax;
					if (!component.Get("m_vMinBounds", slotMin) || !component.Get("m_vMaxBounds", slotMax))
						m_bGeometryError = true;
					else
					{
						vector transform[4];
						node.GetWorldTransform(transform);
						vector spawnMin, spawnMax;
						AICF_ConstructionSiteSearch.TransformBounds(slotMin, slotMax, transform, 0, spawnMin, spawnMax);
						AddCollisionVolume(spawnMin, spawnMax);
						SCR_EEntitySpawnerSlotType slotType;
						component.Get("m_eSlotType", slotType);
						int vehicleTypes = SCR_EEntitySpawnerSlotType.VEHICLE_SMALL | SCR_EEntitySpawnerSlotType.VEHICLE_MEDIUM | SCR_EEntitySpawnerSlotType.VEHICLE_LARGE;
						if (IsDepot() && (slotType & vehicleTypes))
							AddExitPair(slotMin, slotMax, transform);
						for (int corner; corner < 8; corner++)
						{
							vector point = slotMin;
							for (int axis; axis < 3; axis++)
							{
								if (corner & (1 << axis))
									point[axis] = slotMax[axis];
							}
							Include(transform[3] + transform[0] * point[0] + transform[1] * point[1] + transform[2] * point[2]);
						}
						m_iSpawnSlots++;
					}
				}
				if (name.ToType().IsInherited(SCR_CampaignBuildingProviderComponent) || name.ToType().IsInherited(SCR_ServicePointComponent))
				{
					m_aServiceAnchors.Insert(node.GetOrigin());
					Include(node.GetOrigin());
				}
			}
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			Collect(child);
			child = child.GetSibling();
		}
	}

	protected void AddExitPair(vector mins, vector maxs, vector transform[4])
	{
		// Spawn envelope входит в footprint. Коридор отдельно: достаточно одного
		// свободного направления; дорога под выездом не является препятствием.
		AICF_ConstructionExitPair pair = new AICF_ConstructionExitPair();
		vector forwardMin = mins;
		vector forwardMax = maxs;
		forwardMin[2] = maxs[2];
		forwardMax[2] = maxs[2] + 20;
		vector backwardMin = mins;
		vector backwardMax = maxs;
		backwardMin[2] = mins[2] - 20;
		backwardMax[2] = mins[2];
		AICF_ConstructionSiteSearch.TransformBounds(forwardMin, forwardMax, transform, 0, pair.m_Forward.m_vMin, pair.m_Forward.m_vMax);
		AICF_ConstructionSiteSearch.TransformBounds(backwardMin, backwardMax, transform, 0, pair.m_Backward.m_vMin, pair.m_Backward.m_vMax);
		m_aExitPairs.Insert(pair);
	}

	protected void AddCollisionVolume(vector mins, vector maxs)
	{
		// Та же нижняя граница 0.3 м, что у stock placing и прежней проверки.
		// Низкий outline не становится сплошной стеной высотой со всё здание.
		if (maxs[1] <= 0.3)
			return;
		AICF_ConstructionVolume volume = new AICF_ConstructionVolume();
		volume.m_vMin = mins;
		volume.m_vMin[1] = Math.Max(0.3, mins[1]);
		volume.m_vMax = maxs;
		m_aCollisionVolumes.Insert(volume);
	}

	protected float CollisionSize(vector mins, vector maxs)
	{
		vector size = maxs - mins;
		return Math.Max(0.01, size[0]) * Math.Max(0.01, size[1]) * Math.Max(0.01, size[2]);
	}

	protected bool CompactCollisionVolumes(int sliceMs)
	{
		// Объединение только расширяет bounds: ни одна часть prefab не теряется.
		// Лимит даёт конечный live commit/completion без сотен physics queries.
		int started = System.GetTickCount();
		while (m_aCollisionVolumes.Count() > 24 && System.GetTickCount() - started < sliceMs)
		{
			float bestCost = float.MAX;
			int bestA;
			int bestB = 1;
			vector bestMin, bestMax;
			for (int a; a < m_aCollisionVolumes.Count(); a++)
			{
				AICF_ConstructionVolume first = m_aCollisionVolumes[a];
				for (int b = a + 1; b < m_aCollisionVolumes.Count(); b++)
				{
					AICF_ConstructionVolume second = m_aCollisionVolumes[b];
					vector mins, maxs;
					for (int axis; axis < 3; axis++)
					{
						mins[axis] = Math.Min(first.m_vMin[axis], second.m_vMin[axis]);
						maxs[axis] = Math.Max(first.m_vMax[axis], second.m_vMax[axis]);
					}
					float cost = CollisionSize(mins, maxs) - CollisionSize(first.m_vMin, first.m_vMax) - CollisionSize(second.m_vMin, second.m_vMax);
					if (cost < bestCost)
					{
						bestCost = cost;
						bestA = a;
						bestB = b;
						bestMin = mins;
						bestMax = maxs;
					}
				}
			}
			m_aCollisionVolumes[bestA].m_vMin = bestMin;
			m_aCollisionVolumes[bestA].m_vMax = bestMax;
			m_aCollisionVolumes.Remove(bestB);
		}
		return m_aCollisionVolumes.Count() <= 24;
	}

	bool ExitAvoidsComposition(AICF_ConstructionVolume exitVolume)
	{
		foreach (AICF_ConstructionVolume solid : m_aSolids)
		{
			if (solid.m_vMin[0] < exitVolume.m_vMax[0] - 0.1 && solid.m_vMax[0] > exitVolume.m_vMin[0] + 0.1 &&
				solid.m_vMin[2] < exitVolume.m_vMax[2] - 0.1 && solid.m_vMax[2] > exitVolume.m_vMin[2] + 0.1)
				return false;
		}
		return true;
	}

	protected void Include(vector point)
	{
		for (int axis; axis < 3; axis++)
		{
			m_vMin[axis] = Math.Min(m_vMin[axis], point[axis]);
			m_vMax[axis] = Math.Max(m_vMax[axis], point[axis]);
		}
	}

	bool AllowsProvider(SCR_CampaignBuildingProviderComponent provider)
	{
		if (!m_bValid || !provider)
			return false;
		array<EEditableEntityLabel> traits = provider.GetAvailableTraits();
		foreach (EEditableEntityLabel trait : traits)
		{
			if (m_aLabels.Contains(trait))
				return true;
		}
		return false;
	}
}
