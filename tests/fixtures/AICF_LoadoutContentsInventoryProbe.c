// Terminal probe исходного комплекта; только isolated stage с ClothingProbe.
class AICF_LoadoutContentsInventoryChecks : AICF_LoadoutClothingChecks
{
	ref AICF_LoadoutNavigation m_Tree = new AICF_LoadoutNavigation();
	ref array<IEntity> m_NativeItems = {};
	ref array<string> m_NativePaths = {};
	ref array<BaseInventoryStorageComponent> m_NativeStorages = {};
	ref array<int> m_NativeSlots = {};
	ref set<IEntity> m_Seen = new set<IEntity>();

	override void Roundtrip(string rule)
	{
		string snapshot, reason;
		int ignored;
		string expected = AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored);
		Check(rule + "_CAPTURE", AICF_LoadoutInventory.Capture(m_Draft.GetCharacter(), snapshot));
		AICF_LoadoutRecipe empty = AICF_LoadoutRecipe.Decode(m_Recipe.Encode());
		Check(rule + "_RECIPE", empty != null);
		if (!empty) return;
		while (!empty.m_aPaths.IsEmpty()) empty.Undo();
		AICF_LoadoutDraft restored = new AICF_LoadoutDraft();
		bool passed = restored.Build(empty, m_Catalog, reason) && AICF_LoadoutInventory.Restore(restored.GetCharacter(), snapshot);
		Check(rule + "_RESTORE", passed, reason);
		Check(rule + "_SIGNATURE", passed && expected == AICF_LoadoutInventory.Signature(restored.GetCharacter(), m_Catalog, ignored));
		if (passed)
		{
			AICF_LoadoutDraft original = m_Draft;
			m_Draft = restored;
			Compare();
			m_Draft = original;
		}
		m_Tree.Clear();
		m_NativeItems.Clear(); m_NativeStorages.Clear(); m_Seen.Clear();
		restored.Clear();
		AICF_LoadoutService service = new AICF_LoadoutService(null);
		Check(rule + "_VALIDATE", service.AICF_ClothingValidate(m_Recipe, m_Faction, reason) != null, reason);
	}

	// Oracle не читает m_Locations и не использует классификацию ContentRows.
	// GetAll + FindItemSlot независимо обходят native inventory, включая types,
	// которые production мог ошибочно отфильтровать.
	void Native(IEntity entity, string path = "", int depth = 0)
	{
		if (!entity || depth > 6 || m_Seen.Contains(entity)) return;
		m_Seen.Insert(entity);
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!AICF_LoadoutInventory.Editable(storage)) continue;
			array<IEntity> entities = {};
			storage.GetAll(entities, false);
			foreach (IEntity item : entities)
			{
				InventoryStorageSlot slot = storage.FindItemSlot(item);
				if (!slot || slot.GetStorage() != storage) continue;
				string child = path + AICF_LoadoutInventory.StorageId(entity, storage) + "#" + slot.GetID().ToString() + "/";
				Print(string.Format("[AICF][MAG_NATIVE] side=%1 entity=%2 prefab=%3 parent=%4 storage=%5 slot=%6 path=%7", m_sSide, item.GetID(), SCR_ResourceNameUtils.GetPrefabName(item), entity.GetID(), AICF_LoadoutInventory.StorageId(entity, storage), slot.GetID(), path));
				if (!AICF_LoadoutSlotView.HasFixedSlots(storage))
				{
					m_NativeItems.Insert(item);
					m_NativePaths.Insert(path);
					m_NativeStorages.Insert(storage);
					m_NativeSlots.Insert(slot.GetID());
				}
				// Оружейная страница имеет отдельный контракт, не cargo.
				if (!item.FindComponent(BaseWeaponComponent)) Native(item, child, depth + 1);
			}
		}
	}

	void Compare()
	{
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		m_NativeItems.Clear(); m_NativePaths.Clear(); m_NativeStorages.Clear(); m_NativeSlots.Clear(); m_Seen.Clear();
		Native(m_Draft.GetCharacter());
		set<string> pages = new set<string>();
		foreach (AICF_LoadoutLocation page : m_Tree.m_Locations)
		{
			string path = page.m_sPath;
			if (path.IsEmpty() || pages.Contains(path) || page.m_Owner.FindComponent(BaseWeaponComponent)) continue;
			pages.Insert(path);
			array<int> locations = {}, slots = {}, againLocations = {}, againSlots = {};
			m_Tree.ContentRows(path, locations, slots);
			m_Tree.ContentRows(path, againLocations, againSlots);
			set<IEntity> actual = new set<IEntity>();
			bool exact = slots.Count() == againSlots.Count();
			foreach (int r, int index : locations)
			{
				exact = exact && index == againLocations[r] && slots[r] == againSlots[r];
				if (slots[r] < 0) continue;
				AICF_LoadoutLocation location = m_Tree.m_Locations[index];
				IEntity item = location.m_Storage.Get(slots[r]);
				int nativeIndex = m_NativeItems.Find(item);
				exact = exact && item && !actual.Contains(item) && nativeIndex >= 0;
				if (nativeIndex >= 0)
					exact = exact && m_NativePaths[nativeIndex].StartsWith(path) && m_NativeStorages[nativeIndex] == location.m_Storage && m_NativeSlots[nativeIndex] == slots[r];
				actual.Insert(item);
			}
			int expected;
			foreach (int i, IEntity candidate : m_NativeItems)
			{
				if (!m_NativePaths[i].StartsWith(path)) continue;
				expected++;
				exact = exact && actual.Contains(candidate);
			}
			Check("MAG_ROWS_EXACT", exact && expected == actual.Count(), string.Format("path=%1 expected=%2 actual=%3", path, expected, actual.Count()));
		}
	}

	void Edit()
	{
		int selected = -1, count;
		foreach (int i, IEntity item : m_NativeItems)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			if (!prefab.Contains("545x39_AK12_30rnd")) continue;
			count++;
			if (selected < 0 && m_NativeSlots[i] == 1) selected = i;
		}
		bool wcs = m_Recipe.m_sProfile.Contains("WCS");
		if (m_sSide == "USSR" && wcs) Check("MAG_ORIGINAL_SIX", count == 6, string.Format("count=%1 source=%2", count, m_Recipe.m_sCharacter));
		if (selected < 0)
		{
			foreach (int j, IEntity cargo : m_NativeItems)
				if (!m_NativePaths[j].IsEmpty() && cargo.FindComponent(BaseMagazineComponent)) { selected = j; break; }
		}
		Check("MAG_EDIT_CANDIDATE", selected >= 0);
		if (selected < 0) return;
		string path = m_NativePaths[selected];
		string id = AICF_LoadoutInventory.StorageId(m_NativeStorages[selected].GetOwner(), m_NativeStorages[selected]);
		int slot = m_NativeSlots[selected], ignored;
		string before = AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored);
		string prefix = path + id + "#" + slot.ToString() + "/";
		array<string> lines = {}, kept = {};
		before.Split("\n", lines, true);
		foreach (string line : lines) if (!line.StartsWith(prefix)) kept.Insert(line);
		m_Recipe.Add(path, id, slot, "", 1);
		m_Tree.Clear();
		if (!Build()) return;
		BaseInventoryStorageComponent target = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), path), id);
		Check("MAG_REMOVE_EXACT", target && !target.Get(slot) && AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored) == SCR_StringHelper.Join("\n", kept), prefix);
		Compare();
		Roundtrip("MAG_REMOVED");
		m_Recipe.Undo();
		m_Tree.Clear();
		if (!Build()) return;
		Check("MAG_UNDO_EXACT", before == AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored));
		Compare();
		Roundtrip("MAG_ORIGINAL");
	}

	override void Run(string side, SCR_AIGroup group = null)
	{
		m_sSide = side;
		m_Faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey(side)));
		m_Catalog = new AICF_LoadoutCatalog(m_Faction);
		m_Recipe = new AICF_LoadoutRecipe();
		AICF_GroupSpawner roster = new AICF_GroupSpawner();
		string role;
		m_Recipe.m_sCharacter = roster.ResolveRecruitPrefab(m_Faction, 0, role);
		m_Recipe.m_sCharacter = AICF_ContentProfile.GetActive().GetPersonalLoadoutSource(m_Faction, m_Recipe.m_sCharacter);
		m_Recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		m_Recipe.m_sFaction = side;
		m_Recipe.m_sName = "Native contents probe";
		if (Build()) { Compare(); Edit(); }
		m_Tree.Clear();
		m_NativeItems.Clear(); m_NativeStorages.Clear(); m_Seen.Clear();
		m_Draft.Clear();
		// Existing stock multi-area и RHS/WCS helmet cases проверяют, что
		// reconciliation snapshot не ломает замену самой одежды.
		super.Run(side, group);
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFContentsInventoryProbeDone;
	protected int m_iAICFContentsInventoryAt;
	protected bool m_bAICFContentsClientSeen;
	override protected void Update()
	{
		super.Update();
		string enabled;
		if (m_bAICFContentsInventoryProbeDone)
		{
			int players = GetGame().GetPlayerManager().GetPlayerCount();
			if (players > 0) m_bAICFContentsClientSeen = true;
			if ((m_bAICFContentsClientSeen && players == 0) || System.GetTickCount(m_iAICFContentsInventoryAt) > 180000)
				GetGame().RequestClose();
			return;
		}
		if (m_bAICFContentsInventoryProbeDone || !m_bRosterReady || !System.GetCLIParam("aicfContentsInventoryProbe", enabled) || enabled != "1") return;
		m_bAICFContentsInventoryProbeDone = true;
		m_iAICFContentsInventoryAt = System.GetTickCount();
		AICF_LoadoutContentsInventoryChecks checks = new AICF_LoadoutContentsInventoryChecks();
		checks.Run("USSR");
		checks.Run("US");
		Print(string.Format("[AICF][MAG_PROBE_DONE] failures=%1", checks.m_iFailures));
		string hold;
		if (!System.GetCLIParam("aicfContentsInventoryHoldServer", hold) || hold != "1") GetGame().RequestClose();
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() && System.GetCLIParam("aicfContentsInventoryProbe", enabled) && enabled == "1")
			GetGame().GetCallqueue().CallLater(AICF_RunContentsInventoryProbe, 8000, false);
	}
	void AICF_RunContentsInventoryProbe()
	{
		AICF_LoadoutContentsInventoryChecks checks = new AICF_LoadoutContentsInventoryChecks();
		checks.Run("USSR");
		checks.Run("US");
		Print(string.Format("[AICF][MAG_PROBE_DONE] failures=%1", checks.m_iFailures));
		GetGame().RequestClose();
	}
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_RunContentsInventoryProbe);
		super.OnGameEnd();
	}
}
