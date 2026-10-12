// Только stage с AICF_LoadoutClothingProbe.c. Проверяет реальные адреса
// объединённых строк и replay/undo, не имитирует пользовательский UI.
class AICF_LoadoutContentsChecks : AICF_LoadoutClothingChecks
{
	ref AICF_LoadoutNavigation m_Tree = new AICF_LoadoutNavigation();
	int m_iPages;
	int m_iNestedItems;

	void CheckContents(bool requireNested, ResourceName requiredOwner = "")
	{
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		array<int> rootLocations = {}, rootSlots = {};
		m_Tree.Rows("", rootLocations, rootSlots);
		Check("CONTENTS_ROOT_UNCHANGED", !rootSlots.IsEmpty() && !m_Tree.HasContents(""));
		AICF_LoadoutLocation removable;
		int removeSlot = -1;
		string nestedPath;
		foreach (int row, int rootIndex : rootLocations)
		{
			int child = m_Tree.Child(m_Tree.m_Locations[rootIndex], rootSlots[row]);
			if (child < 0) continue;
			string path = m_Tree.m_Locations[child].m_sPath;
			if (m_Tree.m_Locations[child].m_Owner.FindComponent(BaseWeaponComponent))
			{
				Check("CONTENTS_WEAPON_MOUNTS", !m_Tree.HasContents(path));
				continue;
			}
			if (!m_Tree.HasContents(path)) continue;
			m_iPages++;
			array<int> locations = {}, slots = {}, againLocations = {}, againSlots = {};
			m_Tree.ContentRows(path, locations, slots);
			m_Tree.ContentRows(path, againLocations, againSlots);
			bool exact = locations.Count() == slots.Count() && slots.Count() == againSlots.Count();
			bool adding;
			int itemRows, addRows, expectedItems, expectedPockets;
			set<IEntity> seen = new set<IEntity>();
			set<IEntity> expected = new set<IEntity>();
			set<BaseInventoryStorageComponent> pockets = new set<BaseInventoryStorageComponent>();
			foreach (AICF_LoadoutLocation node : m_Tree.m_Locations)
			{
				if (!node.m_sPath.StartsWith(path) || !SCR_UniversalInventoryStorageComponent.Cast(node.m_Storage) ||
					AICF_LoadoutSlotView.HasFixedSlots(node.m_Storage) || pockets.Contains(node.m_Storage)) continue;
				pockets.Insert(node.m_Storage);
				expectedPockets++;
				array<InventoryItemComponent> ownedItems = {};
				node.m_Storage.GetOwnedItems(ownedItems, false);
				foreach (InventoryItemComponent ownedItem : ownedItems)
				{
					if (!ownedItem || !ownedItem.GetParentSlot() || ownedItem.GetParentSlot().GetStorage() != node.m_Storage || expected.Contains(ownedItem.GetOwner())) continue;
					expected.Insert(ownedItem.GetOwner());
					expectedItems++;
				}
			}
			foreach (int r, int index : locations)
			{
				AICF_LoadoutLocation location = m_Tree.m_Locations[index];
				IEntity owner = AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), location.m_sPath);
				exact = exact && owner == location.m_Owner && AICF_LoadoutInventory.FindStorage(owner, location.m_sStorage) == location.m_Storage &&
					location.m_sPath.StartsWith(path) && !AICF_LoadoutSlotView.HasFixedSlots(location.m_Storage) &&
					againLocations[r] == index && againSlots[r] == slots[r] && m_Tree.PocketNumber(path, index) > 0;
				if (slots[r] < 0)
				{
					adding = true;
					addRows++;
					continue;
				}
				IEntity item = location.m_Storage.Get(slots[r]);
				exact = exact && !adding && item && !seen.Contains(item) && expected.Contains(item);
				seen.Insert(item);
				itemRows++;
				if (location.m_sPath != path)
				{
					m_iNestedItems++;
					if (!removable && (requiredOwner.IsEmpty() || SCR_ResourceNameUtils.GetPrefabName(m_Tree.m_Locations[child].m_Owner) == requiredOwner))
					{
						removable = location;
						removeSlot = slots[r];
						nestedPath = path;
					}
				}
			}
			Check("CONTENTS_EXACT_UNIQUE_ORDERED", exact && itemRows == expectedItems && addRows == expectedPockets,
				string.Format("path=%1 items=%2 pockets=%3", path, itemRows, addRows));
		}
		Check("CONTENTS_PAGES", m_iPages > 0);
		if (requireNested) Check("CONTENTS_NESTED_ITEMS", removable != null);
		if (!removable) return;
		int ignored;
		string before = AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored);
		string itemPath = removable.m_sPath, storageId = removable.m_sStorage;
		ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(removable.m_Storage.Get(removeSlot));
		array<int> legacyLocations = {}, legacySlots = {};
		m_Tree.Rows(itemPath, legacyLocations, legacySlots);
		bool legacyAddress;
		foreach (int legacyRow, int legacyIndex : legacyLocations)
			if (m_Tree.m_Locations[legacyIndex].m_sStorage == storageId && legacySlots[legacyRow] == removeSlot) legacyAddress = true;
		Check("CONTENTS_LEGACY_ADDRESS", legacyAddress, string.Format("path=%1 storage=%2 slot=%3", itemPath, storageId, removeSlot));
		string removedPrefix = itemPath + storageId + "#" + removeSlot.ToString() + "/";
		array<string> beforeLines = {}, kept = {};
		before.Split("\n", beforeLines, true);
		foreach (string line : beforeLines)
			if (!line.StartsWith(removedPrefix)) kept.Insert(line);
		m_Recipe.Add(itemPath, storageId, removeSlot, "", 1);
		m_Tree.Clear();
		if (!Build()) return;
		BaseInventoryStorageComponent storage = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), itemPath), storageId);
		Check("CONTENTS_REMOVE_EXACT", storage && !storage.Get(removeSlot));
		Check("CONTENTS_OTHER_ITEMS_UNCHANGED", AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored) == SCR_StringHelper.Join("\n", kept));
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		Check("CONTENTS_PARENT_REMAINS", m_Tree.HasContents(nestedPath));
		Roundtrip("CONTENTS_REMOVED");
		string savePath = AICF_PersonalLoadoutStore.DIRECTORY + "/contents_" + m_sSide + "_" + m_iPages.ToString();
		Check("CONTENTS_SAVE", AICF_PersonalLoadoutStore.Save(savePath, m_Recipe, 1));
		int revision;
		AICF_LoadoutRecipe saved = AICF_PersonalLoadoutStore.Load(savePath, revision);
		Check("CONTENTS_RELOAD", saved && revision == 1 && saved.Encode() == m_Recipe.Encode());
		m_Recipe.Undo();
		m_Tree.Clear();
		if (!Build()) return;
		storage = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), itemPath), storageId);
		Check("CONTENTS_UNDO_EXACT", storage && SCR_ResourceNameUtils.GetPrefabName(storage.Get(removeSlot)) == prefab &&
			before == AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored));
	}

	void TV102()
	{
		array<string> prefabs = {};
		m_Catalog.List(SCR_EArsenalItemType.VEST_AND_WAIST, prefabs);
		ResourceName chosen;
		int selectedSlot = -1;
		foreach (ResourceName prefab : prefabs)
		{
			string name = m_Catalog.Name(prefab);
			name.ToLower();
			if (!name.Contains("tv-102")) continue;
			IEntity candidate = m_Draft.GetPreview().ResolvePreviewEntityForPrefab(prefab);
			if (!candidate) continue;
			BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(candidate.FindComponent(BaseLoadoutClothComponent));
			int slot = -1;
			if (AICF_LoadoutClothing.Covers(cloth, LoadoutVestArea)) slot = Root().GetSlotFromArea(LoadoutVestArea).GetID();
			else if (AICF_LoadoutClothing.Covers(cloth, LoadoutArmoredVestSlotArea)) slot = Root().GetSlotFromArea(LoadoutArmoredVestSlotArea).GetID();
			if (slot < 0) continue;
			chosen = prefab;
			selectedSlot = slot;
			if (name.Contains("[7]") && name.Contains("emr")) break;
		}
		Check("CONTENTS_TV102_CANDIDATE", !chosen.IsEmpty(), chosen);
		if (chosen.IsEmpty()) return;
		m_Recipe.Add("", AICF_LoadoutInventory.StorageId(m_Draft.GetCharacter(), Root()), selectedSlot, chosen, 1);
		m_Tree.Clear();
		if (Build())
		{
			Check("CONTENTS_TV102_EQUIPPED", SCR_ResourceNameUtils.GetPrefabName(AICF_LoadoutClothing.CoveringItem(Root(), selectedSlot)) == chosen);
			// Этот wearable TV-102 не равен отдельному Pouch Preset [7].
			// Наличие вложенных предметов проверено у штатного WCS комплекта.
			CheckContents(false);
		}
	}

	override void Run(string side, SCR_AIGroup group = null)
	{
		m_sSide = side;
		m_Faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey(side)));
		m_Catalog = new AICF_LoadoutCatalog(m_Faction);
		m_Recipe = new AICF_LoadoutRecipe();
		AICF_GroupSpawner source = new AICF_GroupSpawner();
		string role;
		m_Recipe.m_sCharacter = source.ResolveRecruitPrefab(m_Faction, 0, role);
		m_Recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		m_Recipe.m_sFaction = side;
		m_Recipe.m_sName = "Contents probe";
		if (Build())
		{
			CheckContents(m_Recipe.m_sProfile.Contains("RHS"));
			if (side == "USSR" && m_Recipe.m_sProfile.Contains("RHS")) TV102();
		}
		m_Tree.Clear();
		m_Draft.Clear();
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFContentsProbeDone;
	override protected void Update()
	{
		super.Update();
		string enabled;
		if (m_bAICFContentsProbeDone || !m_bRosterReady || !System.GetCLIParam("aicfContentsProbe", enabled) || enabled != "1") return;
		m_bAICFContentsProbeDone = true;
		AICF_LoadoutContentsChecks checks = new AICF_LoadoutContentsChecks();
		checks.Run("US");
		checks.Run("USSR");
		Print(string.Format("[AICF][CONTENTS_PROBE_DONE] failures=%1 pages=%2 nested_items=%3", checks.m_iFailures, checks.m_iPages, checks.m_iNestedItems));
		GetGame().RequestClose();
	}
}
