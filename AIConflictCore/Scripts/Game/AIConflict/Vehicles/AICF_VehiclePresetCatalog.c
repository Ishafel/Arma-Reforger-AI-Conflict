enum AICF_EVehicleCatalogSource
{
	ALL,
	FACTION,
	CIVILIAN
}

enum AICF_EVehicleCatalogCategory
{
	ALL,
	CARS,
	TRUCKS,
	ARMOR,
	OTHER
}

// Своя faction + native CIV — whitelist, а topology читается с native модели
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
	protected ref array<int> m_aSources = {};
	protected ref array<int> m_aCategories = {};

	void AICF_VehiclePresetCatalog(SCR_CampaignFaction faction)
	{
		m_Faction = faction;
		if (!faction) return;
		Append(faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE), AICF_EVehicleCatalogSource.FACTION);
		SCR_Faction civilian = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey("CIV"));
		if (civilian)
			Append(civilian.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, false), AICF_EVehicleCatalogSource.CIVILIAN);
		SCR_EntityCatalogManagerComponent manager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (manager)
			Append(manager.GetEntityCatalogOfType(EEntityCatalogType.VEHICLE, false), AICF_EVehicleCatalogSource.CIVILIAN, true);
	}

	protected void Append(SCR_EntityCatalog catalog, int source, bool requireCivilianLabel = false)
	{
		if (!catalog) return;
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (!entry || !entry.IsEnabled() || entry.GetPrefab().IsEmpty() || m_aPrefabs.Contains(entry.GetPrefab())) continue;
			if (requireCivilianLabel && !entry.HasEditableEntityLabel(EEditableEntityLabel.FACTION_CIV)) continue;
			Resource resource = Resource.Load(entry.GetPrefab());
			if (!resource || !resource.IsValid()) continue;
			if (!SCR_BaseContainerTools.FindComponentSource(resource, VehicleWheeledSimulation)) continue;
			// Legacy модовые tracked prefabs могут наследовать wheel component.
			string path = entry.GetPrefab();
			path.ToLower();
			if (path.Contains("/tracked/") || SCR_BaseContainerTools.FindComponentSource(resource, VehicleTrackedSimulation)) continue;
			m_aPrefabs.Insert(entry.GetPrefab());
			string name = FilePath.StripPath(entry.GetPrefab());
			SCR_UIInfo info = entry.GetEntityUiInfo();
			if (info && !info.GetName().IsEmpty()) name = info.GetName();
			m_aNames.Insert(name);
			m_aCosts.Insert(EntryCost(entry, resource));
			m_aSources.Insert(source);
			m_aCategories.Insert(Category(entry));
		}
	}

	static int Category(SCR_EntityCatalogEntry entry)
	{
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC) || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMOR))
			return AICF_EVehicleCatalogCategory.ARMOR;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK))
			return AICF_EVehicleCatalogCategory.TRUCKS;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_CAR))
			return AICF_EVehicleCatalogCategory.CARS;
		return AICF_EVehicleCatalogCategory.OTHER;
	}

	protected int EntryCost(SCR_EntityCatalogEntry entry, Resource resource)
	{
		int quote = AICF_ContentProfile.GetActive().GetVehiclePresetCostOverride(entry.GetPrefab());
		if (quote >= 0) return quote;
		SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (data) return data.GetSupplyCost();
		// Тот же native CAMPAIGN budget, который читает SpawnerData.InitData.
		// Читаем суммы без MergeBudgetCosts: он может менять source objects.
		IEntityComponentSource component = SCR_EditableEntityComponentClass.GetEditableEntitySource(resource);
		if (!component) return -1;
		BaseContainer info = component.GetObject("m_UIInfo");
		if (!info) return -1;
		array<ref SCR_EntityBudgetValue> own = {};
		array<ref SCR_EntityBudgetValue> children = {};
		info.Get("m_EntityBudgetCost", own);
		info.Get("m_EntityChildrenBudgetCost", children);
		int total = -1;
		if (!ReadCampaignCost(own, total) || !ReadCampaignCost(children, total)) return -1;
		return total;
	}

	protected bool ReadCampaignCost(array<ref SCR_EntityBudgetValue> budgets, inout int total)
	{
		foreach (SCR_EntityBudgetValue budget : budgets)
		{
			if (!budget || budget.GetBudgetType() != EEditableEntityBudget.CAMPAIGN) continue;
			int value = budget.GetBudgetValue();
			if (value < 0 || (total >= 0 && value > 2147483646 - total)) return false;
			if (total < 0) total = 0;
			total += value;
		}
		return true;
	}

	int Source(ResourceName prefab)
	{
		int index = m_aPrefabs.Find(prefab);
		if (index < 0) return -1;
		return m_aSources[index];
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
		json.WriteValue("sources", m_aSources);
		json.WriteValue("categories", m_aCategories);
		return json.SaveToString();
	}

	bool Inspect(ResourceName prefab, out string reason)
	{
		Clear();
		reason = "PREFAB_UNAVAILABLE";
		if (!m_aPrefabs.Contains(prefab)) return false;
		if (Cost(prefab) <= 0) { reason = "VEHICLE_COST_UNAVAILABLE"; return false; }
		m_World = BaseWorld.CreateWorld("ChimeraWorld", "AICF_VehiclePreset_" + (++s_iWorldSequence).ToString());
		if (!m_World || !m_World.IsValid()) { reason = "PREVIEW_WORLD_UNAVAILABLE"; return false; }
		ChimeraWorld world = m_World.GetRef();
		if (!world || world == GetGame().GetWorld()) { reason = "PREVIEW_WORLD_UNAVAILABLE"; Clear(); return false; }
		world.LoadSystems("{3A1DEDB2A5818B53}Configs/Systems/MapPreviewSystemsConfig.conf");
		// Backend topology не использует глобальный UI ItemPreviewManager cache.
		m_Model = GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), world);
		if (!Vehicle.Cast(m_Model) || m_Model.GetWorld() == GetGame().GetWorld()) { Clear(); return false; }
		if (!m_Model.FindComponent(VehicleWheeledSimulation)) { reason = "UNSUPPORTED_VEHICLE"; Clear(); return false; }
		if (m_Model.FindComponent(VehicleTrackedSimulation)) { reason = "UNSUPPORTED_VEHICLE"; Clear(); return false; }
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
