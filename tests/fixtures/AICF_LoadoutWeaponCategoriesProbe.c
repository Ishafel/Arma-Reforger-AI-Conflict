// Только отдельный stage. Реальный faction catalog и native preview weapons.
modded class AICF_MatchController
{
	protected bool m_bAICFWeaponCategoriesDone;
	protected int m_iAICFWeaponCategoryFailures;
	protected int m_iAICFWeaponCategoryChecks;

	protected void AICF_WeaponCategoryCheck(string rule, bool passed)
	{
		m_iAICFWeaponCategoryChecks++;
		if (!passed) m_iAICFWeaponCategoryFailures++;
		Print(string.Format("[AICF][WEAPON_CATEGORY_CHECK] rule=%1 passed=%2", rule, passed));
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (m_bAICFWeaponCategoriesDone || !m_bRosterReady || !System.GetCLIParam("aicfWeaponCategoriesProbe", enabled) || enabled != "1") return;
		m_bAICFWeaponCategoriesDone = true;
		AICF_ProbeWeaponCategories(m_USFaction);
		AICF_ProbeWeaponCategories(m_USSRFaction);
		Print(string.Format("[AICF][WEAPON_CATEGORIES_DONE] checks=%1 failures=%2", m_iAICFWeaponCategoryChecks, m_iAICFWeaponCategoryFailures));
		GetGame().RequestClose();
	}

	protected void AICF_ProbeWeaponCategories(SCR_CampaignFaction faction)
	{
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		AICF_GroupSpawner source = new AICF_GroupSpawner();
		string role, reason;
		recipe.m_sCharacter = source.ResolveRecruitPrefab(faction, 0, role);
		recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		if (!draft.Build(recipe, catalog, reason))
		{
			m_iAICFWeaponCategoryFailures++;
			Print("[AICF][WEAPON_CATEGORIES_BUILD_FAILED] " + reason);
			return;
		}
		array<string> prefabs = {};
		catalog.List(0, prefabs, SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS);
		AICF_LoadoutItemAreas areas = new AICF_LoadoutItemAreas();
		areas.FilterWeapons(prefabs, "primary", draft.GetPreview());
		AICF_LoadoutWeaponCategories categories = new AICF_LoadoutWeaponCategories();
		AICF_LoadoutNavigation tree = new AICF_LoadoutNavigation();
		tree.Build(draft.GetCharacter(), catalog);
		array<int> locations = {}, slots = {};
		tree.Rows("", locations, slots);
		int primarySlots;
		foreach (int row, int location : locations)
		{
			WeaponSlotComponent slot = AICF_LoadoutSlotView.WeaponSlot(tree.m_Locations[location].m_Storage, slots[row]);
			if (slot && slot.GetWeaponSlotType() == "primary") primarySlots++;
		}
		AICF_WeaponCategoryCheck("TWO_PRIMARY_SLOTS_" + faction.GetFactionKey(), primarySlots == 2);
		array<string> all = {};
		all.Copy(prefabs);
		categories.Filter(all, AICF_ELoadoutWeaponCategory.ALL, draft.GetPreview());
		AICF_WeaponCategoryCheck("ALL_UNCHANGED_" + faction.GetFactionKey(), !prefabs.IsEmpty() && SCR_StringHelper.Join("\n", all) == SCR_StringHelper.Join("\n", prefabs));
		set<string> seen = new set<string>();
		bool partition = true;
		for (int category = 1; category <= AICF_ELoadoutWeaponCategory.OTHER; category++)
		{
			array<string> filtered = {};
			filtered.Copy(prefabs);
			categories.Filter(filtered, category, draft.GetPreview());
			int last = -1;
			foreach (ResourceName value : filtered)
			{
				int index = prefabs.Find(value);
				partition = partition && index > last && !seen.Contains(value);
				last = index;
				seen.Insert(value);
			}
			Print(string.Format("[AICF][WEAPON_CATEGORY_COUNT] faction=%1 category=%2 count=%3", faction.GetFactionKey(), category, filtered.Count()));
		}
		AICF_WeaponCategoryCheck("EXACT_PARTITION_ORDER_" + faction.GetFactionKey(), partition && seen.Count() == prefabs.Count());
		array<string> known = {
			"{B31929F65F0D0279}Prefabs/Weapons/Rifles/M14/Rifle_M21.et",
			"{3EB02CDAD5F23C82}Prefabs/Weapons/Rifles/SVD/Rifle_SVD.et",
			"{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et",
			"{D182DCDD72BF7E34}Prefabs/Weapons/MachineGuns/M60/MG_M60.et",
			"{7A82FE978603F137}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7.et",
			"{FC695E70D2773ACB}Prefabs/Weapons/Rifles/G28/Rifle_G28.et",
			"{C1311F93EAC26D4E}Prefabs/Weapons/Rifles/DVL10/Rifle_DVL10.et",
			"{D0F213711B925CF5}Prefabs/Weapons/Rifles/M27IAR/Rifle_M38SDMR.et",
			"{66AAEDA477051298}Prefabs/Weapons/Rifles/M27IAR/Rifle_M27IAR.et",
			"{256BBFF8BDCDF97B}Prefabs/Weapons/Rifles/ASVAL/Rifle_VSS.et",
			"{072527D0E670FCF2}Prefabs/Weapons/Rifles/MPX/Rifle_MPX.et",
			"{5A987A8A13763769}Prefabs/Weapons/Rifles/M16/Rifle_M16A2_M203.et"};
		array<int> expected = {AICF_ELoadoutWeaponCategory.MARKSMAN, AICF_ELoadoutWeaponCategory.MARKSMAN,
			AICF_ELoadoutWeaponCategory.ASSAULT, AICF_ELoadoutWeaponCategory.MACHINE_GUN,
			AICF_ELoadoutWeaponCategory.ROCKET_LAUNCHER, AICF_ELoadoutWeaponCategory.MARKSMAN,
			AICF_ELoadoutWeaponCategory.SNIPER, AICF_ELoadoutWeaponCategory.MARKSMAN,
			AICF_ELoadoutWeaponCategory.MACHINE_GUN, AICF_ELoadoutWeaponCategory.MARKSMAN,
			AICF_ELoadoutWeaponCategory.SUBMACHINE_GUN, AICF_ELoadoutWeaponCategory.ASSAULT};
		int examples;
		foreach (int k, ResourceName example : known)
		{
			if (!prefabs.Contains(example)) continue;
			examples++;
			AICF_WeaponCategoryCheck("KNOWN_" + example, categories.Category(example, draft.GetPreview()) == expected[k]);
		}
		AICF_WeaponCategoryCheck("KNOWN_COVERAGE_" + faction.GetFactionKey(), examples >= 2);
		array<string> sources = {};
		AICF_ContentProfile.GetActive().GetLoadoutSources(sources);
		foreach (string itemSource : sources)
		{
			array<string> sourceItems = {}, expectedItems = {};
			catalog.List(0, sourceItems, SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS, itemSource);
			areas.FilterWeapons(sourceItems, "primary", draft.GetPreview());
			categories.Filter(sourceItems, AICF_ELoadoutWeaponCategory.MARKSMAN, draft.GetPreview());
			foreach (ResourceName item : prefabs)
				if (catalog.Source(item) == itemSource && categories.Category(item, draft.GetPreview()) == AICF_ELoadoutWeaponCategory.MARKSMAN)
					expectedItems.Insert(item);
			// Legacy native compatibility удаляет через Remove и может переставлять
			// элементы. Здесь сравниваем membership; порядок нового Filter — выше.
			sourceItems.Sort();
			expectedItems.Sort();
			AICF_WeaponCategoryCheck("SOURCE_INTERSECTION_" + faction.GetFactionKey() + "_" + itemSource,
				SCR_StringHelper.Join("\n", sourceItems) == SCR_StringHelper.Join("\n", expectedItems));
		}
		foreach (ResourceName prefab : prefabs)
		{
			IEntity entity = draft.GetPreview().ResolvePreviewEntityForPrefab(prefab);
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(entity.FindComponent(BaseWeaponComponent));
			Print(string.Format("[AICF][WEAPON_CATEGORY_ITEM] faction=%1 type=%2 native=%3 prefab=%4 name=%5 category=%6", faction.GetFactionKey(), catalog.ItemType(prefab), weapon.GetWeaponType(), prefab, catalog.Name(prefab), categories.Category(prefab, draft.GetPreview())));
		}
		tree.Clear();
		categories.Clear();
		draft.Clear();
	}
}
