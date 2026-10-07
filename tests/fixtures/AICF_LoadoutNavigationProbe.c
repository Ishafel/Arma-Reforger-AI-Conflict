// Только изолированный test stage вместе с AICF_LoadoutClothingProbe.c.
class AICF_LoadoutNavigationChecks : AICF_LoadoutClothingChecks
{
	ref AICF_LoadoutNavigation m_Tree = new AICF_LoadoutNavigation();

	void CheckTree()
	{
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		array<int> locations = {}, slots = {};
		m_Tree.Rows("", locations, slots);
		int clothing, weapons, children;
		bool addresses = locations.Count() == slots.Count();
		foreach (int row, int index : locations)
		{
			AICF_LoadoutLocation location = m_Tree.m_Locations[index];
			if (EquipedWeaponStorageComponent.Cast(location.m_Storage))
				weapons++;
			else if (EquipedLoadoutStorageComponent.Cast(location.m_Storage))
				clothing++;
			if (m_Tree.Child(location, slots[row]) >= 0)
				children++;
		}
		foreach (AICF_LoadoutLocation node : m_Tree.m_Locations)
		{
			IEntity resolved = AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), node.m_sPath);
			addresses = addresses && resolved == node.m_Owner && AICF_LoadoutInventory.FindStorage(resolved, node.m_sStorage) == node.m_Storage;
		}
		Check("NAV_ROOT_CLOTHING_AND_WEAPONS", clothing >= 8 && weapons > 0, string.Format("clothing=%1 weapons=%2", clothing, weapons));
		Check("NAV_ROOT_CHILDREN", children > 0, "children=" + children.ToString());
		Check("NAV_ALL_OPERATION_ADDRESSES", addresses, "locations=" + m_Tree.m_Locations.Count().ToString());
	}

	void Rig()
	{
		array<string> prefabs = {};
		m_Catalog.List(SCR_EArsenalItemType.VEST_AND_WAIST, prefabs);
		ResourceName rig;
		foreach (ResourceName prefab : prefabs)
		{
			string name = m_Catalog.Name(prefab);
			name.ToLower();
			if (name.Contains("6sh117"))
			{
				Print("[AICF][NAVIGATION_CANDIDATE] name=" + name + " prefab=" + prefab);
				if (name.Contains("sl kit"))
					rig = prefab;
			}
		}
		Check("NAV_6SH117_SL_CANDIDATE", !rig.IsEmpty());
		if (rig.IsEmpty())
			return;
		EquipedLoadoutStorageComponent root = Root();
		int rigSlot = root.GetSlotFromArea(LoadoutVestArea).GetID();
		m_Recipe.Add("", AICF_LoadoutInventory.StorageId(m_Draft.GetCharacter(), root), rigSlot, rig, 1);
		if (!Build())
			return;
		Check("NAV_EXACT_RIG", SCR_ResourceNameUtils.GetPrefabName(AICF_LoadoutClothing.CoveringItem(Root(), rigSlot)) == rig);
		CheckTree();
		int child = -1;
		foreach (AICF_LoadoutLocation location : m_Tree.m_Locations)
		{
			if (location.m_sPath.IsEmpty() && EquipedLoadoutStorageComponent.Cast(location.m_Storage))
				child = m_Tree.Child(location, rigSlot);
		}
		Check("NAV_6SH117_OPEN", child >= 0);
		if (child < 0)
			return;
		string rigPath = m_Tree.m_Locations[child].m_sPath;
		Check("NAV_6SH117_PARENT", AICF_LoadoutNavigation.ParentPath(rigPath).IsEmpty());
		array<int> locations = {}, slots = {};
		m_Tree.Rows(rigPath, locations, slots);
		Check("NAV_6SH117_ROWS", slots.Count() >= 17, "rows=" + slots.Count().ToString());
		Check("NAV_6SH117_CAPACITY", m_Tree.CapacityText(rigPath) != "Вместимость: —", m_Tree.CapacityText(rigPath));
		int pockets;
		AICF_LoadoutLocation removable;
		int removeSlot;
		ResourceName removePrefab;
		foreach (int row, int index : locations)
		{
			AICF_LoadoutLocation node = m_Tree.m_Locations[index];
			if (slots[row] < 0)
				continue;
			IEntity item = node.m_Storage.Get(slots[row]);
			if (!item)
				continue;
			string name = m_Catalog.EntityName(item);
			Check("NAV_POUCH_READABLE_NAME", !name.IsEmpty() && !name.StartsWith("#") && !name.Contains("_"), name);
			Print(string.Format("[AICF][NAVIGATION_ITEM] slot=%1 name=%2 storage=%3 source=%4", slots[row], name, node.m_Storage.Type(), node.m_Storage.GetSlot(slots[row]).GetSourceName()));
			int inside = m_Tree.Child(node, slots[row]);
			if (inside < 0)
				continue;
			string pouchPath = m_Tree.m_Locations[inside].m_sPath;
			Check("NAV_POUCH_PARENT", AICF_LoadoutNavigation.ParentPath(pouchPath) == rigPath);
			array<int> pocketLocations = {}, pocketSlots = {};
			m_Tree.Rows(pouchPath, pocketLocations, pocketSlots);
			Check("NAV_POUCH_ROWS", !pocketSlots.IsEmpty(), "name=" + name);
			foreach (int r, int pocketIndex : pocketLocations)
			{
				if (pocketSlots[r] >= 0)
					continue;
				pockets++;
				Check("NAV_POUCH_CATEGORIES", !AICF_LoadoutSlotView.HasFixedSlots(m_Tree.m_Locations[pocketIndex].m_Storage));
			}
			if (!removable)
			{
				removable = node;
				removeSlot = slots[row];
				removePrefab = SCR_ResourceNameUtils.GetPrefabName(item);
			}
		}
		Check("NAV_POUCH_ADD_TARGETS", pockets > 0, "pockets=" + pockets.ToString());
		Check("NAV_EMPTY_ITEM_NO_PAGE", m_Tree.Child(m_Tree.m_Locations[locations[0]], slots[0]) < 0);
		if (!removable)
			return;
		string path = removable.m_sPath, storageId = removable.m_sStorage;
		m_Recipe.Add(path, storageId, removeSlot, "", 1);
		m_Tree.Clear();
		if (Build())
		{
			BaseInventoryStorageComponent target = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), path), storageId);
			Check("NAV_REMOVE_POUCH", target && !target.Get(removeSlot));
		}
		m_Recipe.Undo();
		if (Build())
		{
			BaseInventoryStorageComponent target = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), path), storageId);
			Check("NAV_UNDO_POUCH", target && SCR_ResourceNameUtils.GetPrefabName(target.Get(removeSlot)) == removePrefab);
			CheckTree();
		}
	}

	override void Run(string side, SCR_AIGroup group = null)
	{
		m_sSide = side;
		m_Group = group;
		m_Faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey(side)));
		m_Catalog = new AICF_LoadoutCatalog(m_Faction);
		m_Recipe = new AICF_LoadoutRecipe();
		AICF_GroupSpawner source = new AICF_GroupSpawner();
		string role;
		m_Recipe.m_sCharacter = source.ResolveRecruitPrefab(m_Faction, 0, role);
		m_Recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		m_Recipe.m_sFaction = side;
		m_Recipe.m_sName = "Navigation probe";
		if (Build())
		{
			CheckTree();
			if (side == "USSR" && m_Recipe.m_sProfile.Contains("RHS"))
				Rig();
		}
		m_Tree.Clear();
		m_Draft.Clear();
	}
}

modded class AICF_MatchController
{
	protected bool m_bNavigationProbeDone;
	override protected void Update()
	{
		super.Update();
		string enabled;
		if (m_bNavigationProbeDone || !m_bRosterReady || !System.GetCLIParam("aicfNavigationProbe", enabled) || enabled != "1")
			return;
		m_bNavigationProbeDone = true;
		AICF_LoadoutNavigationChecks checks = new AICF_LoadoutNavigationChecks();
		checks.Run("US");
		checks.Run("USSR");
		Print(string.Format("[AICF][NAVIGATION_PROBE] server=1 finished=1 failures=%1", checks.m_iFailures));
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() && System.GetCLIParam("aicfNavigationProbe", enabled) && enabled == "1")
			GetGame().GetCallqueue().CallLater(AICF_RunNavigationProbe, 8000, false);
	}
	void AICF_RunNavigationProbe()
	{
		AICF_LoadoutNavigationChecks checks = new AICF_LoadoutNavigationChecks();
		checks.Run("US");
		checks.Run("USSR");
		Print(string.Format("[AICF][NAVIGATION_PROBE] server=0 finished=1 failures=%1", checks.m_iFailures));
		GetGame().RequestClose();
	}
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_RunNavigationProbe);
		super.OnGameEnd();
	}
}
