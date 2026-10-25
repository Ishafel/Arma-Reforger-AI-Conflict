// Каталог faction/profile — whitelist, а topology читается с native модели
// в private preview world. Кампания не получает пробную машину или экипаж.
class AICF_VehiclePresetCatalog
{
	protected static int s_iWorldSequence;
	protected ref SharedItemRef m_World;
	protected IEntity m_Model;
	protected ref array<BaseCompartmentSlot> m_aSeats = {};
	protected SCR_CampaignFaction m_Faction;
	protected ref array<ResourceName> m_aPrefabs = {};
	protected ref array<string> m_aNames = {};
	protected ref array<int> m_aCosts = {};

	void AICF_VehiclePresetCatalog(SCR_CampaignFaction faction)
	{
		m_Faction = faction;
		if (!faction) return;
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE);
		if (!catalog) return;
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (!entry || !entry.IsEnabled() || entry.GetPrefab().IsEmpty() || m_aPrefabs.Contains(entry.GetPrefab())) continue;
			SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
			Resource resource = Resource.Load(entry.GetPrefab());
			if (!data || data.GetSupplyCost() <= 0 || !resource || !resource.IsValid()) continue;
			m_aPrefabs.Insert(entry.GetPrefab());
			string name = FilePath.StripPath(entry.GetPrefab());
			SCR_UIInfo info = entry.GetEntityUiInfo();
			if (info && !info.GetName().IsEmpty()) name = info.GetName();
			m_aNames.Insert(name);
			m_aCosts.Insert(data.GetSupplyCost());
		}
	}

	int Cost(ResourceName prefab)
	{
		int index = m_aPrefabs.Find(prefab);
		if (index < 0) return -1;
		return m_aCosts[index];
	}

	string List()
	{
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("prefabs", m_aPrefabs);
		json.WriteValue("names", m_aNames);
		json.WriteValue("costs", m_aCosts);
		return json.SaveToString();
	}

	bool Inspect(ResourceName prefab, out string reason)
	{
		Clear();
		reason = "PREFAB_UNAVAILABLE";
		if (Cost(prefab) <= 0) return false;
		m_World = BaseWorld.CreateWorld("ChimeraWorld", "AICF_VehiclePreset_" + (++s_iWorldSequence).ToString());
		if (!m_World || !m_World.IsValid()) { reason = "PREVIEW_WORLD_UNAVAILABLE"; return false; }
		ChimeraWorld world = m_World.GetRef();
		if (!world || world == GetGame().GetWorld()) { reason = "PREVIEW_WORLD_UNAVAILABLE"; Clear(); return false; }
		world.LoadSystems("{3A1DEDB2A5818B53}Configs/Systems/MapPreviewSystemsConfig.conf");
		// Backend topology не использует глобальный UI ItemPreviewManager cache.
		m_Model = GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), world);
		if (!Vehicle.Cast(m_Model) || m_Model.GetWorld() == GetGame().GetWorld()) { Clear(); return false; }
		if (!m_Model.FindComponent(VehicleWheeledSimulation)) { reason = "UNSUPPORTED_VEHICLE"; Clear(); return false; }
		AICF_VehiclePreset.Collect(m_Model, m_aSeats);
		int pilots;
		foreach (BaseCompartmentSlot seat : m_aSeats)
			if (seat.GetType() == ECompartmentType.PILOT) pilots++;
		if (pilots != 1 || m_aSeats.Count() > AICF_VehiclePreset.MAX_SEATS) { reason = "UNSUPPORTED_VEHICLE"; Clear(); return false; }
		reason = "READY";
		return true;
	}

	array<BaseCompartmentSlot> Seats() { return m_aSeats; }

	AICF_VehiclePreset Draft(ResourceName prefab)
	{
		AICF_VehiclePreset preset = new AICF_VehiclePreset();
		preset.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		preset.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(m_Faction.GetFactionKey());
		preset.m_sName = FilePath.StripPath(prefab);
		preset.m_sPrefab = prefab;
		foreach (BaseCompartmentSlot seat : m_aSeats)
		{
			preset.m_aSeats.Insert(AICF_VehiclePreset.SeatKey(seat));
			preset.m_aMembers.Insert(-1);
		}
		return preset;
	}

	string SeatNames()
	{
		array<string> names = {};
		foreach (BaseCompartmentSlot seat : m_aSeats)
		{
			string name = seat.GetCompartmentName(true);
			UIInfo info = seat.GetUIInfo();
			if (info && !info.GetName().IsEmpty()) name = info.GetName();
			names.Insert(name);
		}
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("names", names);
		return json.SaveToString();
	}

	void Clear()
	{
		m_aSeats.Clear();
		if (m_Model) SCR_EntityHelper.DeleteEntityAndChildren(m_Model);
		m_Model = null;
		m_World = null;
	}

	void ~AICF_VehiclePresetCatalog() { Clear(); }
}
