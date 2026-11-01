// Native сравнение полного каталога с активными own/CIV источниками.
// Копировать только в isolated Core/Vehicles/AICF_ZCatalogProbe.c.
modded class AICF_MatchController
{
	protected bool m_bVehicleCatalogChecked;
	protected int m_iVehicleCatalogChecks;
	protected int m_iVehicleCatalogFailures;

	protected void AICF_CatalogCheck(string rule, bool passed)
	{
		m_iVehicleCatalogChecks++;
		if (!passed) m_iVehicleCatalogFailures++;
		Print(string.Format("[AICF][VEHICLE_CATALOG_CHECK] rule=%1 passed=%2", rule, passed));
	}

	protected string AICF_CatalogBudgets(ResourceName prefab)
	{
		IEntityComponentSource source = SCR_EditableEntityComponentClass.GetEditableEntitySource(Resource.Load(prefab));
		if (!source) return "NONE";
		BaseContainer info = source.GetObject("m_UIInfo");
		if (!info) return "NONE";
		array<ref SCR_EntityBudgetValue> own = {};
		array<ref SCR_EntityBudgetValue> children = {};
		info.Get("m_EntityBudgetCost", own);
		info.Get("m_EntityChildrenBudgetCost", children);
		string result;
		foreach (SCR_EntityBudgetValue budget : own)
			result += string.Format("O%1:%2;", budget.GetBudgetType(), budget.GetBudgetValue());
		foreach (SCR_EntityBudgetValue child : children)
			result += string.Format("C%1:%2;", child.GetBudgetType(), child.GetBudgetValue());
		return result;
	}

	protected void AICF_CatalogFaction(SCR_CampaignFaction faction, SCR_CampaignFaction enemy)
	{
		string prefix = m_ContentProfile.GetStableFactionKey(faction.GetFactionKey()) + "_";
		array<SCR_EntityCatalogEntry> own = {};
		faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE).GetEntityList(own);
		array<SCR_EntityCatalogEntry> civilian = {};
		SCR_Faction civ = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey("CIV"));
		if (civ && civ.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, false))
			civ.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, false).GetEntityList(civilian);
		array<SCR_EntityCatalogEntry> factionless = {};
		SCR_EntityCatalogManagerComponent manager = SCR_EntityCatalogManagerComponent.GetInstance();
		SCR_EntityCatalog neutral;
		if (manager) neutral = manager.GetEntityCatalogOfType(EEntityCatalogType.VEHICLE, false);
		if (neutral) neutral.GetEntityList(factionless);
		foreach (SCR_EntityCatalogEntry neutralEntry : factionless)
			if (neutralEntry.HasEditableEntityLabel(EEditableEntityLabel.FACTION_CIV)) civilian.Insert(neutralEntry);
		array<ResourceName> expected = {};
		array<string> before = {};
		array<SCR_EntityCatalogEntry> entries = {};
		entries.InsertAll(own);
		entries.InsertAll(civilian);
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			Resource resource = Resource.Load(entry.GetPrefab());
			if (!entry.IsEnabled() || !resource || !resource.IsValid() || expected.Contains(entry.GetPrefab())) continue;
			if (!SCR_BaseContainerTools.FindComponentSource(resource, VehicleWheeledSimulation)) continue;
			string path = entry.GetPrefab();
			path.ToLower();
			if (path.Contains("/tracked/") || SCR_BaseContainerTools.FindComponentSource(resource, VehicleTrackedSimulation)) continue;
			expected.Insert(entry.GetPrefab());
			before.Insert(AICF_CatalogBudgets(entry.GetPrefab()));
		}
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		AICF_VehiclePresetCatalog second = new AICF_VehiclePresetCatalog(faction);
		JsonLoadContext json = new JsonLoadContext();
		array<ResourceName> actual = {};
		array<string> names = {};
		array<int> sources = {}, categories = {}, costs = {};
		json.LoadFromString(catalog.List());
		json.ReadValue("prefabs", actual);
		json.ReadValue("names", names);
		json.ReadValue("sources", sources);
		json.ReadValue("categories", categories);
		json.ReadValue("costs", costs);
		AICF_CatalogCheck(prefix + "PARALLEL_COLUMNS", actual.Count() == names.Count() && actual.Count() == sources.Count() && actual.Count() == categories.Count() && actual.Count() == costs.Count());
		bool complete = actual.Count() == expected.Count();
		bool unchanged = true;
		bool validPrices = true;
		foreach (int index, ResourceName prefab : expected)
		{
			if (!actual.Contains(prefab)) complete = false;
			if (AICF_CatalogBudgets(prefab) != before[index]) unchanged = false;
			if (catalog.Cost(prefab) != second.Cost(prefab)) validPrices = false;
		}
		AICF_CatalogCheck(prefix + "ALL_NATIVE_ENTRIES", complete);
		AICF_CatalogCheck(prefix + "BUDGETS_UNCHANGED", unchanged);
		AICF_CatalogCheck(prefix + "STABLE_QUOTES", validPrices);
		bool ownVisible = true, civVisible = true, categoryMatch = true;
		int ownCount, civCount, cars, trucks, armor;
		foreach (int actualIndex, ResourceName actualPrefab : actual)
		{
			if (sources[actualIndex] == AICF_EVehicleCatalogSource.FACTION) ownCount++;
			else if (sources[actualIndex] == AICF_EVehicleCatalogSource.CIVILIAN) civCount++;
			else categoryMatch = false;
			if (categories[actualIndex] == AICF_EVehicleCatalogCategory.CARS) cars++;
			if (categories[actualIndex] == AICF_EVehicleCatalogCategory.TRUCKS) trucks++;
			if (categories[actualIndex] == AICF_EVehicleCatalogCategory.ARMOR) armor++;
		}
		foreach (SCR_EntityCatalogEntry ownEntry : own)
			if (actual.Contains(ownEntry.GetPrefab()) && catalog.Source(ownEntry.GetPrefab()) != AICF_EVehicleCatalogSource.FACTION) ownVisible = false;
		foreach (SCR_EntityCatalogEntry civEntry : civilian)
			if (actual.Contains(civEntry.GetPrefab()) && !AICF_CatalogHasEntry(own, civEntry.GetPrefab()) && catalog.Source(civEntry.GetPrefab()) != AICF_EVehicleCatalogSource.CIVILIAN) civVisible = false;
		foreach (SCR_EntityCatalogEntry typed : entries)
		{
			int typedIndex = actual.Find(typed.GetPrefab());
			if (typedIndex < 0) continue;
			if (typed.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_HELICOPTER) || typed.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_AIRPLANE) || typed.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TURRET)) categoryMatch = false;
			if (typed.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK) && categories[typedIndex] != AICF_EVehicleCatalogCategory.TRUCKS) categoryMatch = false;
		}
		AICF_CatalogCheck(prefix + "OWN_SOURCE", ownVisible && ownCount > 0);
		AICF_CatalogCheck(prefix + "CIVILIAN_SOURCE", civVisible && civCount > 0);
		AICF_CatalogCheck(prefix + "NATIVE_CATEGORIES", categoryMatch && cars > 0 && trucks > 0);
		array<SCR_EntityCatalogEntry> opponents = {};
		enemy.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE).GetEntityList(opponents);
		bool enemyBlocked = true;
		int rejected;
		foreach (SCR_EntityCatalogEntry opponent : opponents)
		{
			if (expected.Contains(opponent.GetPrefab())) continue;
			string rejection;
			if (catalog.Source(opponent.GetPrefab()) >= 0 || catalog.Inspect(opponent.GetPrefab(), rejection) || rejection != "PREFAB_UNAVAILABLE") enemyBlocked = false;
			rejected++;
		}
		AICF_CatalogCheck(prefix + "ENEMY_REJECTED", enemyBlocked && rejected > 0);
		ResourceName sample;
		foreach (ResourceName candidate : actual)
			if (candidate.Contains("UAZ469_uncovered_CIV_blue.et")) sample = candidate;
		string reason;
		bool inspected = !sample.IsEmpty() && catalog.Inspect(sample, reason);
		Print(string.Format("[AICF][VEHICLE_CATALOG_SAMPLE] faction=%1 sample=%2 cost=%3 inspected=%4 reason=%5", prefix, sample, catalog.Cost(sample), inspected, reason));
		AICF_CatalogCheck(prefix + "CIVILIAN_INSPECTED", inspected && catalog.Cost(sample) > 0);
		if (inspected)
		{
			AICF_VehiclePreset draft = catalog.Draft(sample);
			AICF_CatalogCheck(prefix + "CIVILIAN_RECIPIENT", draft.m_sFaction == m_ContentProfile.GetStableFactionKey(faction.GetFactionKey()) && draft.m_sProfile == m_ContentProfile.GetProfileKey() && draft.MatchesSeats(catalog.Seats()));
		}
		Print(string.Format("[AICF][VEHICLE_CATALOG_TOTAL] faction=%1 own=%2 civilian=%3 total=%4 cars=%5 trucks=%6 armor=%7", prefix, ownCount, civCount, actual.Count(), cars, trucks, armor));
		catalog.Clear();
	}

	protected bool AICF_CatalogHasEntry(array<SCR_EntityCatalogEntry> entries, ResourceName prefab)
	{
		foreach (SCR_EntityCatalogEntry entry : entries)
			if (entry.GetPrefab() == prefab) return true;
		return false;
	}

	override protected void Update()
	{
		super.Update();
		string flag;
		if (!m_bRosterReady || m_bVehicleCatalogChecked || !System.GetCLIParam("aicfVehicleCatalogProbe", flag)) return;
		m_bVehicleCatalogChecked = true;
		int supplies = m_EconomySystem.GetTotalSupplies(m_USFaction) + m_EconomySystem.GetTotalSupplies(m_USSRFaction);
		AICF_CatalogFaction(m_USFaction, m_USSRFaction);
		AICF_CatalogFaction(m_USSRFaction, m_USFaction);
		AICF_CatalogCheck("NO_PURCHASE_OR_SPAWN", supplies == m_EconomySystem.GetTotalSupplies(m_USFaction) + m_EconomySystem.GetTotalSupplies(m_USSRFaction) && m_VehicleCoordinator.GetSpawnedCount(m_USFaction.GetFactionKey()) == 0 && m_VehicleCoordinator.GetSpawnedCount(m_USSRFaction.GetFactionKey()) == 0);
		Print(string.Format("[AICF][VEHICLE_CATALOG_DONE] checks=%1 failures=%2", m_iVehicleCatalogChecks, m_iVehicleCatalogFailures));
		GetGame().RequestClose();
	}
}
