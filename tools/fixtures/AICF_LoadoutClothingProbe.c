// Временная terminal fixture, только в изолированном stage обоих peers.
modded class AICF_LoadoutService
{
	AICF_LoadoutBinding AICF_ClothingValidate(AICF_LoadoutRecipe recipe, SCR_CampaignFaction faction, out string reason)
	{
		return Validate(recipe, faction, reason);
	}
}

class AICF_LoadoutClothingChecks
{
	int m_iFailures;
	string m_sSide;
	ref AICF_LoadoutCatalog m_Catalog;
	ref AICF_LoadoutRecipe m_Recipe;
	ref AICF_LoadoutDraft m_Draft = new AICF_LoadoutDraft();
	SCR_CampaignFaction m_Faction;
	SCR_AIGroup m_Group;

	void Check(string rule, bool passed, string detail = "")
	{
		if (!passed)
			m_iFailures++;
		Print(string.Format("[AICF][CLOTHING_PROBE] side=%1 server=%2 rule=%3 passed=%4 %5", m_sSide, Replication.IsServer(), rule, passed, detail));
	}

	EquipedLoadoutStorageComponent Root()
	{
		return EquipedLoadoutStorageComponent.Cast(m_Draft.GetCharacter().FindComponent(CharacterInventoryStorageComponent));
	}

	bool Build()
	{
		string reason;
		bool built = m_Draft.Build(m_Recipe, m_Catalog, reason);
		Check("BUILD", built, reason);
		return built;
	}

	void Roundtrip(string rule)
	{
		string snapshot, reason;
		int ignored;
		string expected = AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored);
		Check(rule + "_CAPTURE", AICF_LoadoutInventory.Capture(m_Draft.GetCharacter(), snapshot));
		AICF_LoadoutRecipe empty = AICF_LoadoutRecipe.Decode(m_Recipe.Encode());
		while (!empty.m_aPaths.IsEmpty())
			empty.Undo();
		AICF_LoadoutDraft restored = new AICF_LoadoutDraft();
		bool passed = restored.Build(empty, m_Catalog, reason) && AICF_LoadoutInventory.Restore(restored.GetCharacter(), snapshot);
		Check(rule + "_RESTORE", passed, reason);
		Check(rule + "_SIGNATURE", passed && expected == AICF_LoadoutInventory.Signature(restored.GetCharacter(), m_Catalog, ignored));
		restored.Clear();
		AICF_LoadoutService service = new AICF_LoadoutService(null);
		AICF_LoadoutBinding binding = service.AICF_ClothingValidate(m_Recipe, m_Faction, reason);
		Check(rule + "_VALIDATE", binding != null, reason);
		if (binding && m_Group)
			Check(rule + "_AI_APPLY", AICF_LoadoutApplicator.Apply(AICF_GroupRuntime.ResolveAliveLeader(m_Group), m_Group, m_Faction, binding, reason), reason);
	}

	void MultiArea()
	{
		EquipedLoadoutStorageComponent root = Root();
		LoadoutSlotInfo jacket = root.GetSlotFromArea(LoadoutJacketArea);
		LoadoutSlotInfo pants = root.GetSlotFromArea(LoadoutPantsArea);
		ResourceName oldPants = SCR_ResourceNameUtils.GetPrefabName(root.Get(pants.GetID()));
		string rootId = AICF_LoadoutInventory.StorageId(m_Draft.GetCharacter(), root);
		array<string> all = {};
		m_Catalog.List(SCR_EArsenalItemType.TORSO | SCR_EArsenalItemType.LEGS, all);
		ResourceName suit;
		foreach (ResourceName prefab : all)
		{
			IEntity item = m_Draft.GetPreview().ResolvePreviewEntityForPrefab(prefab);
			if (!item)
				continue;
			BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent));
			if (AICF_LoadoutClothing.Covers(cloth, LoadoutJacketArea) && AICF_LoadoutClothing.Covers(cloth, LoadoutPantsArea))
			{
				suit = prefab;
				break;
			}
		}
		Check("MULTI_CANDIDATE", !suit.IsEmpty(), "prefab=" + suit);
		if (suit.IsEmpty())
			return;
		AICF_LoadoutItemAreas areas = new AICF_LoadoutItemAreas();
		Check("MULTI_CATALOG_BOTH", areas.MatchesArea(suit, LoadoutJacketArea, m_Draft.GetPreview()) && areas.MatchesArea(suit, LoadoutPantsArea, m_Draft.GetPreview()));
		int jacketId = jacket.GetID(), pantsId = pants.GetID();
		m_Recipe.Add("", rootId, pantsId, suit, 1);
		if (!Build())
			return;
		root = Root();
		IEntity worn = AICF_LoadoutClothing.CoveringItem(root, jacketId);
		Check("MULTI_SECONDARY_SLOT", worn && worn == AICF_LoadoutClothing.CoveringItem(root, pantsId) && SCR_ResourceNameUtils.GetPrefabName(worn) == suit);
		Roundtrip("MULTI");
		m_Recipe.Add("", rootId, pantsId, "", 1);
		if (Build())
			Check("MULTI_REMOVE_SECONDARY", !AICF_LoadoutClothing.CoveringItem(Root(), pantsId) && !AICF_LoadoutClothing.CoveringItem(Root(), jacketId));
		m_Recipe.Undo();
		if (Build())
			Check("MULTI_UNDO", SCR_ResourceNameUtils.GetPrefabName(AICF_LoadoutClothing.CoveringItem(Root(), pantsId)) == suit);
		m_Recipe.Add("", rootId, pantsId, oldPants, 1);
		if (Build())
		{
			Check("MULTI_REPLACE_WITH_PANTS", SCR_ResourceNameUtils.GetPrefabName(Root().Get(pantsId)) == oldPants && !Root().Get(jacketId));
			Roundtrip("PANTS");
		}
		m_Recipe.Undo();
		m_Recipe.Undo();
		m_Recipe.Add("", rootId, jacketId, suit, 1);
		if (Build())
			Check("MULTI_PRIMARY_SLOT", SCR_ResourceNameUtils.GetPrefabName(AICF_LoadoutClothing.CoveringItem(Root(), pantsId)) == suit);
	}

	void Helmet()
	{
		array<string> all = {}, masks = {}, helmets = {};
		m_Catalog.List(0, all);
		foreach (string prefab : all)
		{
			string name = prefab;
			name.ToLower();
			if (name.Contains("balaclava"))
				masks.Insert(prefab);
		}
		m_Catalog.List(SCR_EArsenalItemType.HEADWEAR, helmets);
		Check("BALACLAVA_CATALOG", !masks.IsEmpty(), "count=" + masks.Count().ToString());
		EquipedLoadoutStorageComponent root = Root();
		LoadoutSlotInfo head = root.GetSlotFromArea(LoadoutHeadCoverArea);
		int headId = head.GetID();
		string rootId = AICF_LoadoutInventory.StorageId(m_Draft.GetCharacter(), root);
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(m_Draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
		ResourceName helmet, mask;
		string storageId;
		int selectedSlot = -1;
		foreach (ResourceName hat : helmets)
		{
			IEntity item = m_Draft.GetPreview().ResolvePreviewEntityForPrefab(hat);
			if (!item)
				continue;
			set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
			SCR_PlayerArsenalLoadout.FindStorageComponents(item, storages);
			foreach (BaseInventoryStorageComponent storage : storages)
			{
				Print(string.Format("[AICF][CLOTHING_PROBE] helmet=%1 storage=%2 editable=%3 fixed=%4 slots=%5", hat, storage.Type(), AICF_LoadoutInventory.Editable(storage), AICF_LoadoutSlotView.HasFixedSlots(storage), storage.GetSlotsCount()));
				if (!AICF_LoadoutInventory.Editable(storage) || !AICF_LoadoutSlotView.HasFixedSlots(storage))
					continue;
				for (int slot; slot < storage.GetSlotsCount(); slot++)
				{
					array<string> matches = {};
					matches.Copy(masks);
					AICF_LoadoutItemAreas.FilterAttachments(matches, storage, slot, m_Draft.GetPreview(), manager);
					if (matches.IsEmpty())
						continue;
					helmet = hat;
					mask = matches[0];
					storageId = AICF_LoadoutInventory.StorageId(item, storage);
					selectedSlot = slot;
					break;
				}
				if (selectedSlot >= 0)
					break;
			}
			if (selectedSlot >= 0)
				break;
		}
		Check("BALACLAVA_SLOT", selectedSlot >= 0, string.Format("helmet=%1 mask=%2 slot=%3 storage=%4", helmet, mask, selectedSlot, storageId));
		if (selectedSlot < 0)
			return;
		m_Recipe.Add("", rootId, headId, helmet, 1);
		if (!Build())
			return;
		string path = rootId + "#" + headId.ToString() + "/";
		BaseInventoryStorageComponent focused = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), path), storageId);
		manager = InventoryStorageManagerComponent.Cast(m_Draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
		int ignored;
		string before = AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored);
		int started = System.GetTickCount();
		int category, mode;
		AICF_LoadoutSlotView.AttachmentCatalog(focused, selectedSlot, category, mode);
		m_Catalog.List(category, all, mode);
		int available = all.Count();
		AICF_LoadoutItemAreas.FilterAttachments(all, focused, selectedSlot, m_Draft.GetPreview(), manager);
		string label;
		AICF_LoadoutSlotView.Describe(focused, selectedSlot, label);
		Print(string.Format("[AICF][CLOTHING_PROBE] full_catalog=%1 matches=%2 duration_ms=%3 label=%4 mask_type=%5 source=%6", available, all.Count(), System.GetTickCount(started), label, m_Catalog.ItemType(mask), focused.GetSlot(selectedSlot).GetSourceName()));
		Check("BALACLAVA_FULL_CATALOG", all.Contains(mask));
		Check("HELMET_FILTER_READ_ONLY", before == AICF_LoadoutInventory.Signature(m_Draft.GetCharacter(), m_Catalog, ignored));
		m_Recipe.Add(path, storageId, selectedSlot, mask, 1);
		if (!Build())
			return;
		BaseInventoryStorageComponent target = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), path), storageId);
		Check("BALACLAVA_ATTACHED", target && SCR_ResourceNameUtils.GetPrefabName(target.Get(selectedSlot)) == mask);
		Roundtrip("BALACLAVA");
		m_Recipe.Add(path, storageId, selectedSlot, "", 1);
		if (Build())
		{
			target = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), path), storageId);
			Check("BALACLAVA_REMOVE", target && !target.Get(selectedSlot));
		}
		m_Recipe.Undo();
		if (Build())
		{
			target = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(m_Draft.GetCharacter(), path), storageId);
			Check("BALACLAVA_UNDO", target && SCR_ResourceNameUtils.GetPrefabName(target.Get(selectedSlot)) == mask);
		}
	}

	void Run(string side, SCR_AIGroup group = null)
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
		m_Recipe.m_sName = "Clothing probe";
		if (Build())
		{
			if (m_Recipe.m_sProfile.Contains("RHS"))
				Helmet();
			else if (side == "USSR")
				MultiArea();
		}
		m_Draft.Clear();
	}
}

modded class AICF_MatchController
{
	protected bool m_bClothingProbeDone;
	protected int m_iClothingProbeAt;
	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!m_bRosterReady || !System.GetCLIParam("aicfClothingProbe", enabled) || enabled != "1")
			return;
		if (!m_bClothingProbeDone)
		{
			m_bClothingProbeDone = true;
			m_iClothingProbeAt = System.GetTickCount();
			AICF_LoadoutClothingChecks checks = new AICF_LoadoutClothingChecks();
			checks.Run("US", m_USState.GetSlot(0).GetGroup());
			checks.Run("USSR", m_USSRState.GetSlot(0).GetGroup());
			Print(string.Format("[AICF][CLOTHING_PROBE] server=1 finished=1 failures=%1", checks.m_iFailures));
		}
		if (System.GetTickCount(m_iClothingProbeAt) > 180000)
			GetGame().RequestClose();
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() && System.GetCLIParam("aicfClothingProbe", enabled) && enabled == "1")
			GetGame().GetCallqueue().CallLater(AICF_RunClothingProbe, 8000, false);
	}
	void AICF_RunClothingProbe()
	{
		AICF_LoadoutClothingChecks checks = new AICF_LoadoutClothingChecks();
		checks.Run("US");
		checks.Run("USSR");
		Print(string.Format("[AICF][CLOTHING_PROBE] server=0 finished=1 failures=%1", checks.m_iFailures));
		GetGame().RequestClose();
	}
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_RunClothingProbe);
		super.OnGameEnd();
	}
}
