// Только изолированный test stage вместе с AICF_LoadoutClothingProbe.c.
class AICF_LoadoutMagazineChecks : AICF_LoadoutClothingChecks
{
	ref AICF_LoadoutNavigation m_Tree = new AICF_LoadoutNavigation();

	BaseWeaponComponent Weapon(int slot)
	{
		EquipedWeaponStorageComponent storage = EquipedWeaponStorageComponent.Cast(m_Draft.GetCharacter().FindComponent(EquipedWeaponStorageComponent));
		if (!storage || !storage.Get(slot))
			return null;
		return BaseWeaponComponent.Cast(storage.Get(slot).FindComponent(BaseWeaponComponent));
	}

	int Count(ResourceName prefab)
	{
		int count;
		foreach (AICF_LoadoutLocation node : m_Tree.m_Locations)
		{
			if (!SCR_UniversalInventoryStorageComponent.Cast(node.m_Storage) || AICF_LoadoutSlotView.HasFixedSlots(node.m_Storage))
				continue;
			for (int slot; slot < node.m_Storage.GetSlotsCount(); slot++)
			{
				if (SCR_ResourceNameUtils.GetPrefabName(node.m_Storage.Get(slot)) == prefab)
					count++;
			}
		}
		return count;
	}

	void Spare(int weaponSlot)
	{
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		BaseWeaponComponent weapon = Weapon(weaponSlot);
		Check("MAG_WEAPON", weapon && weapon.GetCurrentMuzzle());
		if (!weapon || !weapon.GetCurrentMuzzle())
			return;
		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		ResourceName loaded;
		if (muzzle.GetMagazine())
			loaded = SCR_ResourceNameUtils.GetPrefabName(muzzle.GetMagazine().GetOwner());
		int loadedCount = muzzle.GetAmmoCount();
		ResourceName prefab;
		AICF_LoadoutLocation destination;
		string reason;
		int ignored;
		string before = AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored);
		int started = System.GetTickCount();
		bool found = AICF_LoadoutMagazines.Resolve(m_Draft, m_Catalog, m_Tree, muzzle, prefab, destination, reason);
		Check("MAG_RESOLVE", found, string.Format("prefab=%1 elapsedMs=%2 reason=%3", prefab, System.GetTickCount() - started, reason));
		Check("MAG_READ_ONLY", before == AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored));
		if (!found)
			return;
		Check("MAG_WELL_AND_CATALOG", AICF_LoadoutMagazines.Compatible(prefab, muzzle, m_Catalog, m_Draft.GetPreview()));
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
		if (AICF_LoadoutMagazines.Compatible(loaded, muzzle, m_Catalog, m_Draft.GetPreview()) && AICF_LoadoutMagazines.Destination(loaded, m_Tree, manager))
			Check("MAG_LOADED_PRIORITY", prefab == loaded);
		else if (AICF_LoadoutMagazines.Compatible(muzzle.GetDefaultMagazineOrProjectileName(), muzzle, m_Catalog, m_Draft.GetPreview()) && AICF_LoadoutMagazines.Destination(muzzle.GetDefaultMagazineOrProjectileName(), m_Tree, manager))
			Check("MAG_DEFAULT_PRIORITY", prefab == muzzle.GetDefaultMagazineOrProjectileName());
		Check("MAG_POCKET_DESTINATION", !destination.m_sPath.IsEmpty() && SCR_UniversalInventoryStorageComponent.Cast(destination.m_Storage) && !AICF_LoadoutSlotView.HasFixedSlots(destination.m_Storage));
		ResourceName repeated;
		AICF_LoadoutLocation same;
		Check("MAG_DETERMINISTIC", AICF_LoadoutMagazines.Resolve(m_Draft, m_Catalog, m_Tree, muzzle, repeated, same, reason) && repeated == prefab && same == destination);
		int countBefore = Count(prefab);
		string path = destination.m_sPath, storageId = destination.m_sStorage;
		m_Recipe.Add(path, storageId, -1, prefab, 1);
		m_Tree.Clear();
		if (!Build())
			return;
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		Check("MAG_EXACTLY_ONE_SPARE", Count(prefab) == countBefore + 1);
		muzzle = Weapon(weaponSlot).GetCurrentMuzzle();
		ResourceName stillLoaded;
		if (muzzle.GetMagazine())
			stillLoaded = SCR_ResourceNameUtils.GetPrefabName(muzzle.GetMagazine().GetOwner());
		Check("MAG_LOADED_UNCHANGED", stillLoaded == loaded && muzzle.GetAmmoCount() == loadedCount);
		Roundtrip("MAG_SAVED");
		m_Recipe.Undo();
		m_Tree.Clear();
		if (!Build())
			return;
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		Check("MAG_UNDO", Count(prefab) == countBefore && before == AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored));
	}

	void Pistol()
	{
		array<string> weapons = {};
		m_Catalog.List(0, weapons, SCR_EArsenalItemMode.WEAPON);
		AICF_LoadoutItemAreas areas = new AICF_LoadoutItemAreas();
		ResourceName pistol;
		foreach (ResourceName candidate : weapons)
		{
			IEntity item = m_Draft.GetPreview().ResolvePreviewEntityForPrefab(candidate);
			BaseWeaponComponent gun;
			if (item)
				gun = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (gun && gun.GetWeaponType() == EWeaponType.WT_HANDGUN && areas.WeaponType(candidate, m_Draft.GetPreview()) == "secondary" &&
				gun.GetCurrentMuzzle() && !gun.GetCurrentMuzzle().IsDisposable() && gun.GetCurrentMuzzle().GetMagazineWell())
			{
				pistol = candidate;
				break;
			}
		}
		Check("MAG_PISTOL_CANDIDATE", !pistol.IsEmpty(), pistol);
		if (pistol.IsEmpty())
			return;
		EquipedWeaponStorageComponent storage = EquipedWeaponStorageComponent.Cast(m_Draft.GetCharacter().FindComponent(EquipedWeaponStorageComponent));
		m_Recipe.Add("", AICF_LoadoutInventory.StorageId(m_Draft.GetCharacter(), storage), 2, pistol, 1);
		m_Tree.Clear();
		if (Build())
			Spare(2);
	}

	void RejectAndFill()
	{
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		BaseMuzzleComponent muzzle = Weapon(0).GetCurrentMuzzle();
		ResourceName prefab;
		AICF_LoadoutLocation destination;
		string reason;
		Check("MAG_NO_FOCUS", !AICF_LoadoutMagazines.Resolve(m_Draft, m_Catalog, m_Tree, null, prefab, destination, reason) && !reason.IsEmpty());
		Check("MAG_NO_CHARACTER_PREFAB", !AICF_LoadoutMagazines.Compatible(m_Recipe.m_sCharacter, muzzle, m_Catalog, m_Draft.GetPreview()));
		if (!AICF_LoadoutMagazines.Resolve(m_Draft, m_Catalog, m_Tree, muzzle, prefab, destination, reason))
			return;
		AICF_LoadoutDraft other = new AICF_LoadoutDraft();
		if (other.Build(m_Recipe, m_Catalog, reason))
			Check("MAG_OTHER_WORLD_REJECTED", !AICF_LoadoutMagazines.Compatible(prefab, muzzle, m_Catalog, other.GetPreview()));
		other.Clear();
		BaseWeaponComponent pistol = Weapon(2);
		Check("MAG_WRONG_WELL_REJECTED", pistol && !AICF_LoadoutMagazines.Compatible(prefab, pistol.GetCurrentMuzzle(), m_Catalog, m_Draft.GetPreview()));
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
		AICF_LoadoutNavigation empty = new AICF_LoadoutNavigation();
		Check("MAG_NO_POCKETS", !AICF_LoadoutMagazines.Destination(prefab, empty, manager));
		// Один настоящий карман: native объём должен остановить добавление.
		empty.m_Locations.Insert(destination);
		int added;
		while (added < 64 && AICF_LoadoutInventory.InsertLocal(prefab, destination.m_Storage, -1, manager))
			added++;
		Check("MAG_FULL_POCKET", added < 64 && !AICF_LoadoutMagazines.Destination(prefab, empty, manager), "added=" + added.ToString());
		empty.Clear();
	}

	void Unloaded()
	{
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		BaseMuzzleComponent muzzle = Weapon(0).GetCurrentMuzzle();
		string path, storageId;
		int magazineSlot = -1;
		foreach (AICF_LoadoutLocation node : m_Tree.m_Locations)
		{
			if (node.m_Owner != muzzle.GetOwner())
				continue;
			for (int slot; slot < node.m_Storage.GetSlotsCount(); slot++)
			{
				if (node.m_Storage.GetSlot(slot).GetParentContainer() != muzzle)
					continue;
				path = node.m_sPath;
				storageId = node.m_sStorage;
				magazineSlot = slot;
			}
		}
		Check("MAG_UNLOAD_ADDRESS", magazineSlot >= 0);
		if (magazineSlot < 0)
			return;
		m_Recipe.Add(path, storageId, magazineSlot, "", 1);
		m_Tree.Clear();
		if (Build())
		{
			Check("MAG_UNLOADED", !Weapon(0).GetCurrentMuzzle().GetMagazine());
			Spare(0);
		}
		m_Recipe.Undo();
		m_Tree.Clear();
		Build();
	}

	void RHSPouch()
	{
		if (m_sSide != "USSR" || !m_Recipe.m_sProfile.Contains("RHS"))
			return;
		array<string> rigs = {};
		m_Catalog.List(SCR_EArsenalItemType.VEST_AND_WAIST, rigs);
		ResourceName rig;
		foreach (ResourceName candidate : rigs)
		{
			string name = m_Catalog.Name(candidate);
			name.ToLower();
			if (name.Contains("6sh117") && name.Contains("sl kit"))
			{
				rig = candidate;
				break;
			}
		}
		Check("MAG_RHS_RIG_FOUND", !rig.IsEmpty());
		if (rig.IsEmpty())
			return;
		string rootId = AICF_LoadoutInventory.StorageId(m_Draft.GetCharacter(), Root());
		int rigSlot = Root().GetSlotFromArea(LoadoutVestArea).GetID();
		m_Recipe.Add("", rootId, rigSlot, rig, 1);
		m_Tree.Clear();
		if (!Build())
			return;
		m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
		string rigPath = rootId + "#" + rigSlot.ToString() + "/";
		AICF_LoadoutNavigation pockets = new AICF_LoadoutNavigation();
		foreach (AICF_LoadoutLocation node : m_Tree.m_Locations)
		{
			if (node.m_sPath.StartsWith(rigPath))
				pockets.m_Locations.Insert(node);
		}
		ResourceName prefab;
		AICF_LoadoutLocation destination;
		string reason;
		bool found = AICF_LoadoutMagazines.Resolve(m_Draft, m_Catalog, pockets, Weapon(0).GetCurrentMuzzle(), prefab, destination, reason);
		Check("MAG_RHS_NESTED_POCKET", found && destination.m_sPath != rigPath && destination.m_sPath.StartsWith(rigPath), reason);
		if (found)
		{
			int before = Count(prefab);
			m_Recipe.Add(destination.m_sPath, destination.m_sStorage, -1, prefab, 1);
			pockets.Clear();
			m_Tree.Clear();
			if (Build())
			{
				m_Tree.Build(m_Draft.GetCharacter(), m_Catalog);
				Check("MAG_RHS_POUCH_ADDED", Count(prefab) == before + 1);
				Roundtrip("MAG_RHS_POUCH_SAVED");
			}
		}
		pockets.Clear();
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
		m_Recipe.m_sName = "Magazine probe";
		if (Build())
		{
			Spare(0);
			Pistol();
			Unloaded();
			RHSPouch();
			RejectAndFill();
		}
		m_Tree.Clear();
		m_Draft.Clear();
	}
}

modded class AICF_MatchController
{
	protected bool m_bMagazineProbeDone;
	override protected void Update()
	{
		super.Update();
		string enabled;
		if (m_bMagazineProbeDone || !m_bRosterReady || !System.GetCLIParam("aicfMagazineProbe", enabled) || enabled != "1")
			return;
		m_bMagazineProbeDone = true;
		AICF_LoadoutMagazineChecks checks = new AICF_LoadoutMagazineChecks();
		checks.Run("US");
		checks.Run("USSR");
		Print(string.Format("[AICF][MAGAZINE_PROBE] server=1 finished=1 failures=%1", checks.m_iFailures));
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() && System.GetCLIParam("aicfMagazineProbe", enabled) && enabled == "1")
			GetGame().GetCallqueue().CallLater(AICF_RunMagazineProbe, 8000, false);
	}
	void AICF_RunMagazineProbe()
	{
		AICF_LoadoutMagazineChecks checks = new AICF_LoadoutMagazineChecks();
		checks.Run("US");
		checks.Run("USSR");
		Print(string.Format("[AICF][MAGAZINE_PROBE] server=0 finished=1 failures=%1", checks.m_iFailures));
		GetGame().RequestClose();
	}
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_RunMagazineProbe);
		super.OnGameEnd();
	}
}
