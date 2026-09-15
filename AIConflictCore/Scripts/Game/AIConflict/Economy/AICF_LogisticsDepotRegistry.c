// Read-only stock catalog bridge. Использует те же filters/slots, что player
// production; не вызывает InitiateSpawn и не изменяет player prices/permissions.
modded class SCR_CatalogEntitySpawnerComponent
{
	SCR_EntitySpawnerSlotComponent AICF_LogisticsFreeSlot(SCR_EntityCatalogEntry entry)
	{
		array<SCR_EntitySpawnerSlotComponent> candidates = {};
		AICF_LogisticsCandidateSlots(entry, candidates);
		if (candidates.IsEmpty()) return null;
		return candidates[0];
	}
	bool AICF_LogisticsSupports(SCR_EntityCatalogEntry entry)
	{
		array<SCR_EntitySpawnerSlotComponent> compatible = {};
		AICF_LogisticsCandidateSlots(entry, compatible, false);
		return !compatible.IsEmpty();
	}

	void AICF_LogisticsCandidateSlots(SCR_EntityCatalogEntry entry, out array<SCR_EntitySpawnerSlotComponent> candidates, bool requireClear = true)
	{
		candidates.Clear();
		array<SCR_EntitySpawnerSlotComponent> slots = {};
		slots.Copy(m_aChildSlots);
		foreach (SCR_EntitySpawnerSlotComponent nearby : m_aNearSlots)
		{
			if (!slots.Contains(nearby)) slots.Insert(nearby);
		}
		int checked;
		foreach (SCR_EntitySpawnerSlotComponent slot : slots)
		{
			if (checked++ >= 16) break;
			if (AICF_AllowsLogisticsEntry(entry, slot) && (!requireClear || slot.AICF_LogisticsClear(entry.GetPrefab()))) candidates.Insert(slot);
		}
	}
	bool AICF_AllowsLogisticsEntry(SCR_EntityCatalogEntry entry, SCR_EntitySpawnerSlotComponent slot)
	{
		SCR_EntityCatalogSpawnerData data;
		if (entry) data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		return entry && m_aAssetList.Contains(entry) && data && slot && slot.GetOwner() &&
			(m_aChildSlots.Contains(slot) || m_aNearSlots.Contains(slot)) && data.CanSpawnInSlot(slot.GetSlotType());
	}
}

modded class SCR_EntitySpawnerSlotComponent
{
	// Stock IsOccupied/TraceCallback удаляет wrecks и dereference null physics.
	// Read-only admission по конкретной машине, а не большому stock slot box.
	bool AICF_LogisticsClear(ResourceName prefab)
	{
		if (!GetOwner()) return false;
		AICF_LogisticsVehicleFootprint footprint = AICF_LogisticsVehicleFootprint.Get(prefab);
		if (!footprint || !footprint.m_bValid) return false;
		vector transform[4];
		GetOwner().GetWorldTransform(transform);
		TraceOBB body;
		return footprint.IsClear(GetGame().GetWorld(), transform, body);
	}
}

class AICF_LogisticsDepotWatch
{
	SCR_CampaignBuildingCompositionComponent m_Composition;
	AICF_LogisticsDepotRegistry m_Registry;
	void Changed(bool spawned) { if (m_Registry) m_Registry.MarkDirty(); }
	void Stop()
	{
		if (m_Composition) m_Composition.GetOnCompositionSpawned().Remove(Changed);
		m_Registry = null;
	}
}

class AICF_LogisticsDepotRegistry
{
	protected SCR_MilitaryBaseSystem m_BaseSystem;
	protected SCR_CampaignBuildingManagerComponent m_Manager;
	protected ref array<ref AICF_LogisticsDepotWatch> m_aWatches = {};
	protected static ref map<ResourceName, float> s_mCapacities = new map<ResourceName, float>();
	protected int m_iCursor;
	protected int m_iNextScanMs;
	protected int m_iNextSlot = AICF_LogisticsConfig.SERVICE_SLOT_FIRST;
	protected bool m_bStopped;
	protected ref array<SCR_CatalogEntitySpawnerComponent> m_aScan = {};
	ref array<ref AICF_LogisticsWorker> m_aWorkers = {};

	void Start(SCR_GameModeCampaign campaign)
	{
		m_bStopped = false;
		m_BaseSystem = SCR_MilitaryBaseSystem.GetInstance();
		m_Manager = SCR_CampaignBuildingManagerComponent.Cast(campaign.FindComponent(SCR_CampaignBuildingManagerComponent));
		if (m_BaseSystem) m_BaseSystem.GetOnBaseFactionChanged().Insert(OwnerChanged);
		if (m_Manager)
		{
			m_Manager.GetOnEntitySpawnedByProvider().Insert(Placed);
			m_Manager.GetOnCompositionUnregistered().Insert(Removed);
		}
	}

	void Stop()
	{
		m_bStopped = true;
		if (m_BaseSystem) m_BaseSystem.GetOnBaseFactionChanged().Remove(OwnerChanged);
		if (m_Manager)
		{
			m_Manager.GetOnEntitySpawnedByProvider().Remove(Placed);
			m_Manager.GetOnCompositionUnregistered().Remove(Removed);
		}
		foreach (AICF_LogisticsDepotWatch watch : m_aWatches) watch.Stop();
		m_aWatches.Clear();
		m_aScan.Clear();
		foreach (AICF_LogisticsWorker worker : m_aWorkers)
		{
			if (worker.m_ExitHistory) worker.m_ExitHistory.m_aFailures.Clear();
		}
	}

	void MarkDirty() { m_iNextScanMs = 0; }
	protected void OwnerChanged(SCR_MilitaryBaseComponent base, Faction faction) { MarkDirty(); }
	protected void Placed(int prefabID, SCR_EditableEntityComponent entity, int playerId, SCR_CampaignBuildingProviderComponent provider)
	{
		if (entity && entity.GetOwner()) Watch(SCR_CampaignBuildingCompositionComponent.Cast(entity.GetOwner().FindComponent(SCR_CampaignBuildingCompositionComponent)));
		MarkDirty();
	}
	protected void Removed(SCR_CampaignBuildingCompositionComponent composition)
	{
		for (int i = m_aWatches.Count() - 1; i >= 0; i--)
		{
			if (m_aWatches[i].m_Composition == composition)
			{
				m_aWatches[i].Stop();
				m_aWatches.Remove(i);
			}
		}
		MarkDirty();
	}
	protected void Watch(SCR_CampaignBuildingCompositionComponent composition)
	{
		if (!composition) return;
		foreach (AICF_LogisticsDepotWatch old : m_aWatches)
		{
			if (old.m_Composition == composition) return;
		}
		AICF_LogisticsDepotWatch watch = new AICF_LogisticsDepotWatch();
		watch.m_Composition = composition;
		watch.m_Registry = this;
		composition.GetOnCompositionSpawned().Insert(watch.Changed);
		m_aWatches.Insert(watch);
	}

	static IEntity BuildingRoot(IEntity entity)
	{
		IEntity result = entity;
		for (int depth; entity && depth < 32; depth++)
		{
			if (entity.FindComponent(SCR_CampaignBuildingCompositionComponent)) return entity;
			IEntity parent = entity.GetParent();
			if (!parent || parent.FindComponent(SCR_CampaignBuildingProviderComponent) || parent.FindComponent(SCR_CampaignMilitaryBaseComponent)) break;
			result = parent;
			entity = parent;
		}
		return result;
	}

	static bool Live(AICF_LogisticsWorker w)
	{
		if (!w || !w.m_Depot || w.m_Depot.GetID() != w.m_DepotId || !w.m_Home || !w.m_Home.GetOwner() ||
			w.m_Home.GetOwner().GetID() != w.m_HomeId) return false;
		if (!w.m_Home.IsInitialized() || w.m_Home.GetFaction() != w.m_Faction ||
			w.m_Home.IsBeingCaptured() || w.m_Home.GetCaptureState() != SCR_EBaseCaptureState.NONE || w.m_Home.AreEnemiesPresent()) return false;
		if (!w.m_Production || !w.m_Production.GetOwner() || w.m_Production.GetOwner().GetID() != w.m_ProductionId ||
			w.m_Production.GetServiceState() != SCR_EServicePointStatus.ONLINE || w.m_Production.GetFaction() != w.m_Faction ||
			BuildingRoot(w.m_Production.GetOwner()) != w.m_Depot) return false;
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(w.m_Depot.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (composition)
		{
			if (!composition.IsCompositionSpawned() || !w.m_Provider || !w.m_Provider.GetOwner()) return false;
			if (w.m_Provider.GetOwner().GetID() != w.m_ProviderId || composition.GetProviderEntity() != w.m_Provider.GetOwner() ||
				w.m_Provider.GetCampaignMilitaryBaseComponent() != w.m_Home || SCR_Faction.GetEntityFaction(w.m_Provider.GetOwner()) != w.m_Faction) return false;
		}
		array<SCR_MilitaryBaseComponent> bases = {};
		w.m_Production.GetBases(bases);
		return bases.Contains(w.m_Home);
	}

	static float PrefabCapacity(ResourceName prefab)
	{
		float cached;
		if (s_mCapacities.Find(prefab, cached)) return cached;
		cached = ReadPrefabCapacity(prefab, 0);
		s_mCapacities.Set(prefab, cached);
		return cached;
	}

	protected static float ReadPrefabCapacity(ResourceName prefab, int depth)
	{
		if (depth > 2) return 0;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid()) return 0;
		IEntitySource entity = SCR_BaseContainerTools.FindEntitySource(resource);
		if (!entity) return 0;
		float total;
		for (int i; i < entity.GetComponentCount(); i++)
		{
			IEntityComponentSource component = entity.GetComponent(i);
			if (component.GetClassName() == "SlotManagerComponent")
			{
				BaseContainerList slots = component.GetObjectArray("Slots");
				if (!slots || slots.Count() > 128) continue;
				for (int si; si < slots.Count(); si++)
				{
					BaseContainer slot = slots.Get(si);
					bool enabled = true;
					slot.Get("Enabled", enabled);
					ResourceName attachment;
					slot.Get("Prefab", attachment);
					if (enabled && !attachment.IsEmpty()) total += ReadPrefabCapacity(attachment, depth + 1);
				}
			}
			if (!component.GetClassName().ToType().IsInherited(SCR_ResourceComponent)) continue;
			BaseContainerList containers = component.GetObjectArray("m_aContainers");
			if (!containers) continue;
			for (int c; c < containers.Count(); c++)
			{
				SCR_ResourceContainer container = SCR_ResourceContainer.Cast(BaseContainerTools.CreateInstanceFromContainer(containers.Get(c)));
				if (container && container.GetResourceType() == EResourceType.SUPPLIES) total += container.GetMaxResourceValue();
			}
		}
		return total;
	}

	void Update(AICF_LogisticsConfig config, int now)
	{
		if (m_bStopped || !Replication.IsServer()) return;
		if (m_iCursor >= m_aScan.Count())
		{
			if (now < m_iNextScanMs) return;
			m_aScan.Copy(SCR_CatalogEntitySpawnerComponent.INSTANCES);
			m_iCursor = 0;
			m_iNextScanMs = now + config.m_iPlannerIntervalMs;
		}
		for (int budget; budget < AICF_LogisticsConfig.REGISTRY_BUDGET && m_iCursor < m_aScan.Count(); budget++)
			Inspect(m_aScan[m_iCursor++], config);
	}

	protected void Inspect(SCR_CatalogEntitySpawnerComponent production, AICF_LogisticsConfig config)
	{
		if (!production || !production.GetOwner() || (production.GetType() != SCR_EServicePointType.LIGHT_VEHICLE_DEPOT &&
			production.GetType() != SCR_EServicePointType.HEAVY_VEHICLE_DEPOT)) return;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(production.GetFaction());
		if (!faction || AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()).IsEmpty()) return;
		array<SCR_MilitaryBaseComponent> bases = {};
		production.GetBases(bases);
		SCR_CampaignMilitaryBaseComponent home;
		foreach (SCR_MilitaryBaseComponent base : bases)
		{
			SCR_CampaignMilitaryBaseComponent candidate = SCR_CampaignMilitaryBaseComponent.Cast(base);
			if (candidate && candidate.GetFaction() == faction && candidate.IsInitialized())
			{
				if (home && home != candidate) return; // ambiguous overlapping base membership
				home = candidate;
			}
		}
		IEntity root = BuildingRoot(production.GetOwner());
		if (!home || !root) return;
		Watch(SCR_CampaignBuildingCompositionComponent.Cast(root.FindComponent(SCR_CampaignBuildingCompositionComponent)));
		// Один пустой slot для обнаруженного depot. Остальные появляются по заявкам.
		if (!Find(root, faction, 0)) CreateWorker(root, faction, home, production, 0);
		foreach (AICF_LogisticsWorker w : m_aWorkers)
		{
			if (w.m_Depot != root || w.m_DepotId != root.GetID() || w.m_Faction != faction) continue;
			// Несколько service components одного root делят ordinals. До acquisition
			// разрешён другой совместимый компонент, без нового slot/generation.
			if (w.m_Production != production && !w.m_Lease && !w.m_Group && !w.m_Vehicle && (!Live(w) || !SelectEntry(w)))
			{
				w.m_Production = production;
				w.m_ProductionId = production.GetOwner().GetID();
				w.m_Entry = null;
			}
			bool eligible = Live(w) && SelectEntry(w);
			if (eligible != w.m_bEligible || w.m_sLastReason.IsEmpty())
			{
				if (eligible) w.Log("LOGISTICS_DEPOT_READY", string.Format("ordinal=%1 prefab=%2 capacity_metadata=%3 tier=%4", w.m_iOrdinal, w.m_Entry.GetPrefab(), w.m_fPrefabCapacity, production.GetType()));
				else w.Log("LOGISTICS_DEPOT_REJECTED", "reason=OFFLINE_OR_NO_SUPPORTED_CARGO_PRODUCTION");
				w.m_sLastReason = "DEPOT_RECONCILED";
			}
			w.m_bEligible = eligible;
		}
	}

	protected AICF_LogisticsWorker CreateWorker(IEntity root, SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent home, SCR_CatalogEntitySpawnerComponent production, int ordinal)
	{
		AICF_LogisticsWorker w = new AICF_LogisticsWorker();
		w.m_iSlot = m_iNextSlot++;
		w.m_iOrdinal = ordinal;
		w.m_Faction = faction;
		w.m_Depot = root;
		w.m_DepotId = root.GetID();
		w.m_ExitHistory = new AICF_LogisticsExitHistory();
		foreach (AICF_LogisticsWorker sibling : m_aWorkers)
		{
			if (sibling.m_Depot == root && sibling.m_DepotId == root.GetID() && sibling.m_Faction == faction)
			{
				w.m_ExitHistory = sibling.m_ExitHistory;
				break;
			}
		}
		w.m_Home = home;
		w.m_HomeId = home.GetOwner().GetID();
		w.m_Production = production;
		w.m_ProductionId = production.GetOwner().GetID();
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(root.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (composition && composition.GetProviderEntity())
		{
			w.m_Provider = SCR_CampaignBuildingProviderComponent.Cast(composition.GetProviderEntity().FindComponent(SCR_CampaignBuildingProviderComponent));
			w.m_ProviderId = composition.GetProviderEntity().GetID();
		}
		m_aWorkers.Insert(w);
		return w;
	}

	// Создаёт только identity; lease, AI и physical spawn остаются у vehicle domain.
	AICF_LogisticsWorker AddManualWorker(AICF_LogisticsWorker depot)
	{
		if (m_bStopped || !Replication.IsServer() || !depot || !m_aWorkers.Contains(depot) ||
			depot.m_bStopped || !Live(depot)) return null;
		int ordinal;
		foreach (AICF_LogisticsWorker sibling : m_aWorkers)
		{
			if (sibling.m_Depot == depot.m_Depot && sibling.m_DepotId == depot.m_DepotId && sibling.m_Faction == depot.m_Faction)
				ordinal = Math.Max(ordinal, sibling.m_iOrdinal + 1);
		}
		AICF_LogisticsWorker w = CreateWorker(depot.m_Depot, depot.m_Faction, depot.m_Home, depot.m_Production, ordinal);
		w.m_bEligible = Live(w) && SelectEntry(w);
		if (!w.m_bEligible)
		{
			m_aWorkers.RemoveItem(w);
			return null;
		}
		w.Log("LOGISTICS_DEPOT_READY", string.Format("ordinal=%1 prefab=%2 capacity_metadata=%3 tier=%4 reason=MANUAL_WORKER_ADDED", ordinal, w.m_Entry.GetPrefab(), w.m_fPrefabCapacity, w.m_Production.GetType()));
		w.m_sLastReason = "DEPOT_RECONCILED";
		return w;
	}

	protected AICF_LogisticsWorker Find(IEntity root, Faction faction, int ordinal)
	{
		foreach (AICF_LogisticsWorker w : m_aWorkers)
		{
			if (w.m_Depot == root && w.m_DepotId == root.GetID() && w.m_Faction == faction && w.m_iOrdinal == ordinal) return w;
		}
		return null;
	}

	static bool SelectEntry(AICF_LogisticsWorker w)
	{
		if (w.m_Lease) return w.m_Entry != null;
		array<string> suffixes = {};
		AICF_ContentProfile profile = AICF_ContentProfile.GetActive();
		profile.BuildLogisticsSuffixPreference(profile.GetStableFactionKey(w.m_Faction.GetFactionKey()), suffixes);
		foreach (string suffix : suffixes)
		{
			for (int i; i < 256; i++)
			{
				SCR_EntityCatalogEntry entry = w.m_Production.GetEntryAtIndex(i);
				if (!entry) break;
				ResourceName prefab = entry.GetPrefab();
				if (!prefab.EndsWith(suffix)) continue;
				if (!w.m_Production.AICF_LogisticsSupports(entry)) continue;
				float capacity = PrefabCapacity(prefab);
				if (capacity <= 0) continue;
				w.m_Entry = entry;
				w.m_fPrefabCapacity = capacity;
				return true;
			}
		}
		w.m_Entry = null;
		return false;
	}
}
