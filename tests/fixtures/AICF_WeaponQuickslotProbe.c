// Isolated native component test; actual player deployment is a separate gate.
modded class AICF_MatchController
{
	protected bool m_bAICFQuickslotProbeDone;
	protected int m_iAICFQuickslotChecks;
	protected int m_iAICFQuickslotFailures;

	protected void AICF_QuickslotCheck(string name, bool pass)
	{
		m_iAICFQuickslotChecks++;
		if (!pass) m_iAICFQuickslotFailures++;
		Print(string.Format("[AICF][QUICKSLOT_CASE] case=%1 pass=%2", name, pass));
	}
	protected IEntity AICF_QuickslotWeapon(IEntity character, int index)
	{
		BaseWeaponManagerComponent manager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		array<WeaponSlotComponent> slots = {};
		manager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
			if (slot.GetWeaponSlotIndex() == index) return slot.GetWeaponEntity();
		return null;
	}
	override protected void Update()
	{
		super.Update();
		if (!m_bRosterReady || m_bAICFQuickslotProbeDone || !System.IsCLIParam("aicfQuickslotProbe")) return;
		m_bAICFQuickslotProbeDone = true;
		AICF_RunQuickslotProbe();
		Print(string.Format("[AICF][QUICKSLOT_DONE] checks=%1 failed=%2", m_iAICFQuickslotChecks, m_iAICFQuickslotFailures));
		GetGame().RequestClose();
	}
	protected void AICF_RunQuickslotProbe()
	{
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(m_USSRFaction);
		catalog.SetPersonalRules();
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		string role, reason, source;
		recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		recipe.m_sFaction = "USSR";
		recipe.m_sName = "Two rifles quickslot probe";
		recipe.m_sCharacter = m_GroupSpawner.ResolveRecruitPrefab(m_USSRFaction, 0, role);
		if (System.GetCLIParam("aicfQuickslotCharacter", source)) recipe.m_sCharacter = source;
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		bool built = draft.Build(recipe, catalog, reason);
		AICF_QuickslotCheck("DRAFT", built);
		if (!built) return;
		IEntity character = draft.GetCharacter();
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		array<WeaponSlotComponent> slots = {};
		weapons.GetWeaponsSlots(slots);
		BaseInventoryStorageComponent secondStorage;
		int secondSlot = -1;
		foreach (WeaponSlotComponent slot : slots)
		{
			if (slot.GetWeaponSlotIndex() != 1 || slot.GetWeaponSlotType() != "primary") continue;
			InventoryStorageSlot info = slot.GetSlotInfo();
			if (!info) continue;
			secondStorage = info.GetStorage();
			secondSlot = info.GetID();
		}
		AICF_QuickslotCheck("SECOND_STORAGE", secondStorage && secondSlot >= 0);
		if (!secondStorage) return;
		array<string> prefabs = {};
		catalog.List(0, prefabs, SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS);
		ResourceName svd;
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		foreach (string prefab : prefabs)
		{
			string lower = prefab;
			lower.ToLower();
			if (lower.Contains("svd") && inventory.CanInsertResourceInStorage(prefab, secondStorage, secondSlot)) { svd = prefab; break; }
		}
		AICF_QuickslotCheck("SVD_AVAILABLE", !svd.IsEmpty());
		if (svd.IsEmpty()) return;
		recipe.Add("", AICF_LoadoutInventory.StorageId(character, secondStorage), secondSlot, svd, 1);
		AICF_LoadoutRecipe reloaded = AICF_LoadoutRecipe.Decode(recipe.Encode());
		AICF_QuickslotCheck("PRESET_DECODE", reloaded != null);
		if (!reloaded) return;
		string path = AICF_PersonalLoadoutStore.DIRECTORY + "/quickslot_probe";
		AICF_QuickslotCheck("FILE_SAVE", AICF_PersonalLoadoutStore.Save(path, reloaded, 1));
		int revision;
		AICF_LoadoutRecipe saved = AICF_PersonalLoadoutStore.Load(path, revision);
		AICF_QuickslotCheck("FILE_LOAD", saved && revision == 1 && saved.Encode() == reloaded.Encode());
		if (!saved) return;
		reloaded = saved;
		built = draft.Build(reloaded, catalog, reason);
		AICF_QuickslotCheck("PRESET_RELOAD", built && reloaded.m_aPrefabs.Find(svd) >= 0);
		if (!built) return;
		character = draft.GetCharacter();
		IEntity primary = AICF_QuickslotWeapon(character, 0);
		IEntity secondary = AICF_QuickslotWeapon(character, 1);
		AICF_QuickslotCheck("BOTH_WEAPONS", primary && secondary && SCR_ResourceNameUtils.GetPrefabName(secondary) == svd);
		if (!primary || !secondary) return;
		SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		storage.InitAsPlayer(character, true);
		Print(string.Format("[AICF][QUICKSLOT_READBACK] primary=%1 secondary=%2 quick0=%3 quick1=%4 prefab=%5", primary.GetID(), secondary.GetID(), storage.GetItemFromQuickSlot(0), storage.GetItemFromQuickSlot(1), svd));
		AICF_QuickslotCheck("PRIMARY_QUICKSLOT", storage.GetItemFromQuickSlot(0) == primary);
		AICF_QuickslotCheck("SECONDARY_QUICKSLOT", storage.GetItemFromQuickSlot(1) == secondary);
		storage.InitAsPlayer(character, false);
		storage.RemoveItemFromQuickSlot(secondary);
		storage.StoreItemToQuickSlot(secondary, 4);
		storage.InitAsPlayer(character, true);
		AICF_QuickslotCheck("CUSTOM_BINDING", storage.GetItemFromQuickSlot(4) == secondary && storage.GetItemFromQuickSlot(1) != secondary);
		storage.InitAsPlayer(character, false);
		storage.RemoveItemFromQuickSlot(secondary);
		storage.StoreItemToQuickSlot(primary, 1);
		storage.InitAsPlayer(character, true);
		AICF_QuickslotCheck("OCCUPIED_BINDING", storage.GetItemFromQuickSlot(1) == primary && storage.GetEntityIndexInQuickslots(secondary) < 0);
		storage.InitAsPlayer(character, false);
		draft.Clear();
	}
}
