// Копировать только в отдельный test stage. Запуск: -aicfWCSLoadoutCatalogProbe 1.
// Проверяет каталог/источники, не визуальное поведение UI.
modded class AICF_MatchController
{
	protected bool m_bAICFLoadoutCatalogProbed;
	protected int m_iAICFLoadoutCatalogFailures;

	protected void AICF_CatalogCheck(string rule, bool passed)
	{
		if (!passed) m_iAICFLoadoutCatalogFailures++;
		Print(string.Format("[AICF][LOADOUT_CATALOG_CHECK] rule=%1 passed=%2", rule, passed));
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfWCSLoadoutCatalogProbe", enabled) || enabled != "1" || !m_bRosterReady || m_bAICFLoadoutCatalogProbed) return;
		m_bAICFLoadoutCatalogProbed = true;
		AICF_CatalogCheck("NATIVE_INPUTS_PRESERVED", AICF_WCSCatalogInputEvidence.s_iCheckedFactions == 2 && AICF_WCSCatalogInputEvidence.s_iMissing == 0);
		AICF_ContentProfile profile = AICF_ContentProfile.GetActive();
		// Три concrete GUID: stock override, RHS override, новый WCS descendant.
		AICF_CatalogCheck("VANILLA_OVERRIDE", profile.GetLoadoutItemSource("{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et") == "Vanilla");
		AICF_CatalogCheck("RHS_OVERRIDE", profile.GetLoadoutItemSource("{E998FA484B0C4BFF}Prefabs/Weapons/Handguns/Glock/Handgun_Glock17.et") == "RHS");
		AICF_CatalogCheck("WCS_DESCENDANT", profile.GetLoadoutItemSource("{007C773699518CCB}Prefabs/Weapons/Rifles/M4A1/WCS_Variants/Rifle_M4A1_GA_UD145_Suppressed_SF_SQUAD.et") == "WCS");
		AICF_WCSItemOrigins origins = new AICF_WCSItemOrigins();
		array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
		foreach (SCR_CampaignFaction faction : factions)
		{
			AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
			array<string> all = {};
			catalog.List(0, all);
			array<string> partition = {};
			foreach (string source : {"Vanilla", "RHS", "WCS"})
			{
				array<string> items = {};
				catalog.List(0, items, 0, source);
				int invalid;
				foreach (string prefab : items)
				{
					string indexed;
					if (partition.Contains(prefab) || !catalog.Allows(prefab) || !origins.Find(prefab, indexed) || indexed != source) invalid++;
					partition.Insert(prefab);
				}
				AICF_CatalogCheck(faction.GetFactionKey() + "_" + source, !items.IsEmpty() && invalid == 0);
				Print(string.Format("[AICF][LOADOUT_CATALOG_TAB] faction=%1 source=%2 count=%3 invalid=%4", faction.GetFactionKey(), source, items.Count(), invalid));
			}
			partition.Sort();
			bool same = all.Count() == partition.Count();
			foreach (int index, string expected : all)
				if (!partition.IsIndexValid(index) || partition[index] != expected) same = false;
			AICF_CatalogCheck("PARTITION_" + faction.GetFactionKey(), same);
			array<SCR_EntityCatalogEntry> entries = {};
			faction.GetFactionEntityCatalogOfType(EEntityCatalogType.ITEM).GetEntityList(entries);
			int omitted;
			int excluded;
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				if (catalog.Allows(entry.GetPrefab()))
				{
					if (!all.Contains(entry.GetPrefab())) omitted++;
				}
				else excluded++;
			}
			AICF_CatalogCheck("NO_OMISSION_" + faction.GetFactionKey(), omitted == 0);
			Print(string.Format("[AICF][LOADOUT_CATALOG_COVERAGE] faction=%1 entries=%2 allowed=%3 excluded=%4 omitted=%5", faction.GetFactionKey(), entries.Count(), all.Count(), excluded, omitted));
			array<string> ammunition = {};
			catalog.List(0, ammunition, SCR_EArsenalItemMode.AMMUNITION);
			AICF_CatalogCheck("AMMUNITION_" + faction.GetFactionKey(), !ammunition.IsEmpty());
			array<int> categories = {SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemType.TORSO, SCR_EArsenalItemType.BACKPACK, SCR_EArsenalItemType.WEAPON_ATTACHMENT};
			foreach (int category : categories)
			{
				array<string> items = {};
				catalog.List(category, items);
				AICF_CatalogCheck("CATEGORY_" + faction.GetFactionKey() + "_" + category, !items.IsEmpty());
			}
		}
		Print(string.Format("[AICF][LOADOUT_CATALOG_DONE] failures=%1", m_iAICFLoadoutCatalogFailures));
		GetGame().RequestClose();
	}
}


class AICF_WCSCatalogInputEvidence
{
	static int s_iMissing;
	static int s_iCheckedFactions;
}

modded class AICF_WCSItemCatalog
{
	override int Build(FactionKey faction, array<ref SCR_EntityCatalog> originals)
	{
		array<SCR_EntityCatalogEntry> inputs = {};
		foreach (SCR_EntityCatalog original : originals)
			if (original.GetCatalogType() == EEntityCatalogType.ITEM) AICF_WCSVehicleCatalog.Entries(original, inputs);
		ResourceName path;
		if (faction == "RHS_USAF") path = "{5F7EC52FC40A03E2}Configs/EntityCatalog/US/InventoryItems_EntityCatalog_US.conf";
		else if (faction == "RHS_AFRF") path = "{C53421647C3D0D2E}Configs/EntityCatalog/USSR/InventoryItems_EntityCatalog_USSR.conf";
		else return super.Build(faction, originals);
		SCR_EntityCatalog stock;
		Resource resource = Resource.Load(path);
		if (resource && resource.IsValid())
		{
			stock = SCR_EntityCatalog.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			AICF_WCSVehicleCatalog.Entries(stock, inputs);
		}
		else AICF_WCSCatalogInputEvidence.s_iMissing++;
		int result = super.Build(faction, originals);
		array<SCR_EntityCatalogEntry> output = {};
		GetEntityList(output);
		array<ResourceName> names = {};
		foreach (SCR_EntityCatalogEntry item : output) names.Insert(item.GetPrefab());
		int missing;
		int expected;
		foreach (SCR_EntityCatalogEntry input : inputs)
		{
			if (!input.IsEnabled()) continue;
			expected++;
			if (!names.Contains(input.GetPrefab())) missing++;
		}
		AICF_WCSCatalogInputEvidence.s_iMissing += missing;
		AICF_WCSCatalogInputEvidence.s_iCheckedFactions++;
		Print(string.Format("[AICF][LOADOUT_CATALOG_INPUTS] faction=%1 expected=%2 missing=%3", faction, expected, missing));
		return result;
	}
}


