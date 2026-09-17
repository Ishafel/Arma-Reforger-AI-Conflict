// Временно копировать в Core/Loadouts. Обычный addon fixture не загружает.
// Только canonical terminal launcher; собственный процесс завершается RequestClose.
modded class AICF_LoadoutService
{
	AICF_LoadoutBinding AICF_ProbeValidate(AICF_LoadoutRecipe recipe, SCR_CampaignFaction faction, out string reason)
	{
		return Validate(recipe, faction, reason);
	}
}

modded class AICF_MatchController
{
	protected int m_iLoadoutProbePhase;
	protected int m_iLoadoutProbeAt;
	protected SCR_AIGroup m_LoadoutProbeGroup;
	protected ref AICF_LoadoutBinding m_LoadoutProbeBinding;
	protected int m_iLoadoutProbeFailures;
	protected ref AICF_GroupSlot m_LoadoutProbeSlot;
	protected EntityID m_LoadoutProbeOriginalGroup;

	protected void LoadoutCheck(string rule, bool passed, string detail = "")
	{
		if (!passed)
			m_iLoadoutProbeFailures++;
		Print(string.Format("[AICF][LOADOUT_PROBE] rule=%1 passed=%2 %3", rule, passed, detail));
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (System.GetCLIParam("aicfLoadoutRecruitProbe", enabled) && enabled == "1" && m_bRosterReady)
		{
			if (m_iLoadoutProbePhase == 0)
			{
				m_iLoadoutProbePhase = 1;
				AICF_LoadoutBinding us = RunLoadoutDraftProbe(m_USFaction, 1);
				AICF_LoadoutBinding ussr = RunLoadoutDraftProbe(m_USSRFaction, 1);
				if (us)
					m_USState.GetSlot(0).SetLoadout(1, us, m_USState.GetSlot(0).GetLoadoutRevision());
				if (ussr)
					m_USSRState.GetSlot(0).SetLoadout(1, ussr, m_USSRState.GetSlot(0).GetLoadoutRevision());
				LoadoutCheck("RECRUIT_BINDINGS", us && ussr);
			}
			return;
		}
		if (!System.GetCLIParam("aicfLoadoutProbe", enabled) || enabled != "1" || !m_bRosterReady)
			return;
		if (m_iLoadoutProbeAt == 0)
			m_iLoadoutProbeAt = System.GetTickCount();
		if (m_iLoadoutProbePhase == 0)
		{
			m_iLoadoutProbePhase = 1;
			RunLoadoutDraftProbe(m_USFaction);
			RunLoadoutDraftProbe(m_USSRFaction);
			m_LoadoutProbeGroup = m_GroupSpawner.SpawnGroup(m_USFaction, m_USFaction.GetMainBase(), 0, 3, false);
			if (m_LoadoutProbeGroup)
			{
				m_LoadoutProbeSlot = new AICF_GroupSlot(99, AICF_EGroupRole.ATTACK);
				m_LoadoutProbeSlot.SetDesiredSize(3);
				if (m_LoadoutProbeBinding)
					m_LoadoutProbeSlot.SetLoadout(0, m_LoadoutProbeBinding, 0);
				m_LoadoutProbeSlot.BeginInitialSpawn();
				m_LoadoutProbeSlot.BindSpawnedGroup(m_LoadoutProbeGroup);
				m_GroupSpawner.BeginRosterSpawn(m_LoadoutProbeGroup, 3);
			}
			LoadoutCheck("DONOR_REQUEST", m_LoadoutProbeGroup != null);
		}
		else if (m_iLoadoutProbePhase == 1 && m_LoadoutProbeGroup && m_LoadoutProbeGroup.GetAgentsCount() == 3)
		{
			m_iLoadoutProbePhase = 2;
			for (int i; i < 3; i++)
				LoadoutCheck("INDEX_IDENTITY_" + i.ToString(), m_LoadoutProbeGroup.AICF_GetSpawnMember(i) != null);
			string reason;
			LoadoutCheck("AI_APPLY", m_LoadoutProbeBinding && AICF_LoadoutApplicator.Apply(
				m_LoadoutProbeGroup.AICF_GetSpawnMember(0), m_LoadoutProbeGroup, m_USFaction, m_LoadoutProbeBinding, reason), "reason=" + reason);
			LoadoutCheck("INITIAL_DEPLOYMENT_APPLY", m_LoadoutProbeBinding && m_LoadoutProbeSlot.ApplyDeploymentLoadouts(m_USFaction));
			AICF_GroupSlot slot = m_USState.GetSlot(0);
			int revision = slot.GetLoadoutRevision();
			if (m_LoadoutProbeBinding)
			{
				LoadoutCheck("BIND_STABLE_MEMBER", slot.SetLoadout(0, m_LoadoutProbeBinding, revision));
				LoadoutCheck("REJECT_STALE_REVISION", !slot.SetLoadout(0, m_LoadoutProbeBinding, revision));
				LoadoutCheck("FREE_DEPLOYMENT", slot.GetDeploymentLoadoutCost() == 0 && m_LoadoutProbeBinding.m_iCost == 0);
				m_LoadoutProbeSlot.SetLoadout(0, m_LoadoutProbeBinding, m_LoadoutProbeSlot.GetLoadoutRevision());
				LoadoutCheck("REJECT_STALE_DEPLOYMENT", !m_LoadoutProbeSlot.ApplyDeploymentLoadouts(m_USFaction));
				int generation = m_LoadoutProbeSlot.GetSpawnGeneration();
				m_LoadoutProbeSlot.MarkReady();
				m_LoadoutProbeSlot.MarkDestroyed();
				m_LoadoutProbeSlot.BeginReinforcementWait(0);
				LoadoutCheck("REPLACEMENT_BEGIN", m_LoadoutProbeSlot.BeginReplacementSpawn(System.GetTickCount()));
				m_LoadoutProbeOriginalGroup = m_LoadoutProbeGroup.GetID();
				m_LoadoutProbeGroup.DespawnMembers();
				RplComponent.DeleteRplEntity(m_LoadoutProbeGroup, false);
				m_LoadoutProbeGroup = m_GroupSpawner.SpawnGroup(m_USFaction, m_USFaction.GetMainBase(), 99, 3, false);
				m_LoadoutProbeSlot.BindSpawnedGroup(m_LoadoutProbeGroup);
				m_GroupSpawner.BeginRosterSpawn(m_LoadoutProbeGroup, 3);
				LoadoutCheck("REPLACEMENT_BINDING_RETAINED", m_LoadoutProbeSlot.GetSlotId() == 99 &&
					m_LoadoutProbeSlot.GetSpawnGeneration() > generation && m_LoadoutProbeSlot.GetLoadout(0) != null);
			}
		}
		else if (m_iLoadoutProbePhase == 2 && m_LoadoutProbeGroup && m_LoadoutProbeGroup.GetAgentsCount() == 3)
		{
			LoadoutCheck("REPLACEMENT_NEW_GROUP", m_LoadoutProbeGroup.GetID() != m_LoadoutProbeOriginalGroup);
			LoadoutCheck("REPLACEMENT_APPLY", m_LoadoutProbeSlot.ApplyDeploymentLoadouts(m_USFaction));
			m_iLoadoutProbePhase = 3;
		}
		if (m_iLoadoutProbePhase == 3 || (m_iLoadoutProbePhase < 3 && System.GetTickCount(m_iLoadoutProbeAt) > 60000))
		{
			if (m_LoadoutProbeGroup)
			{
				m_LoadoutProbeSlot.Reset();
				m_LoadoutProbeGroup.DespawnMembers();
				RplComponent.DeleteRplEntity(m_LoadoutProbeGroup, false);
				m_LoadoutProbeGroup = null;
			}
			LoadoutCheck("COMPLETED", m_iLoadoutProbePhase == 3);
			Print(string.Format("[AICF][LOADOUT_PROBE] finished=1 failures=%1", m_iLoadoutProbeFailures));
			m_iLoadoutProbePhase = 4;
		}
		string hold;
		if (m_iLoadoutProbePhase == 4 && (!System.GetCLIParam("aicfLoadoutProbeHoldSeconds", hold) || System.GetTickCount(m_iLoadoutProbeAt) > Math.Min(300, hold.ToInt()) * 1000))
			GetGame().RequestClose();
	}

	protected AICF_LoadoutBinding RunLoadoutDraftProbe(SCR_CampaignFaction faction, int member = 0)
	{
		string role;
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		recipe.m_sCharacter = m_GroupSpawner.ResolveRecruitPrefab(faction, member, role);
		recipe.m_sFaction = m_ContentProfile.GetStableFactionKey(faction.GetFactionKey());
		recipe.m_sProfile = m_ContentProfile.GetProfileKey();
		recipe.m_sName = "Runtime probe " + recipe.m_sFaction;
		LoadoutCheck("RECIPE_ROUNDTRIP", AICF_LoadoutRecipe.Decode(recipe.Encode()) != null);
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		string reason;
		bool built = draft.Build(recipe, catalog, reason);
		LoadoutCheck("ISOLATED_DRAFT_" + recipe.m_sFaction, built, "reason=" + reason);
		if (!built)
			return null;
		LoadoutCheck("WORLD_ISOLATED", draft.GetCharacter().GetWorld() != GetGame().GetWorld());
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
		LoadoutCheck("CHANGE_CLOTHING_" + recipe.m_sFaction, ProbeChange(draft.GetCharacter(), "", manager, catalog, recipe, SCR_EArsenalItemType.HEADWEAR));
		LoadoutCheck("CHANGE_WEAPON_" + recipe.m_sFaction, ProbeChange(draft.GetCharacter(), "", manager, catalog, recipe, SCR_EArsenalItemType.RIFLE));
		LoadoutCheck("ADD_ATTACHMENT_" + recipe.m_sFaction, ProbeInsert(draft.GetCharacter(), "", manager, catalog, recipe, true));
		LoadoutCheck("ADD_QUANTITY_" + recipe.m_sFaction, ProbeInsert(draft.GetCharacter(), "", manager, catalog, recipe, false));
		draft.Clear();
		AICF_LoadoutService service = new AICF_LoadoutService(this);
		AICF_LoadoutBinding binding = service.AICF_ProbeValidate(recipe, faction, reason);
		LoadoutCheck("VALIDATE_READBACK_" + recipe.m_sFaction, binding != null, "reason=" + reason);
		if (binding)
		{
			LoadoutCheck("FREE_TEMPLATE_" + recipe.m_sFaction, binding.m_iCost == 0);
			AICF_LoadoutStore store = new AICF_LoadoutStore();
			LoadoutCheck("LIBRARY_SAVE", store.Save(recipe, reason), "reason=" + reason);
			AICF_LoadoutStore reopened = new AICF_LoadoutStore();
			LoadoutCheck("LIBRARY_REOPEN", reopened.List(recipe).Contains(recipe.m_sName));
			if (recipe.m_sFaction == "US")
				m_LoadoutProbeBinding = binding;
		}
		recipe.Add("", "untrusted", 0, "{0000000000000000}invalid.et", 1);
		LoadoutCheck("REJECT_UNKNOWN_RESOURCE", !service.AICF_ProbeValidate(recipe, faction, reason));
		recipe.Undo();
		recipe.Add("", "untrusted", -1, "invalid", 17);
		LoadoutCheck("REJECT_QUANTITY_LIMIT", !AICF_LoadoutRecipe.Decode(recipe.Encode()));
		recipe.Undo();
		recipe.m_aCounts.Insert(1);
		LoadoutCheck("REJECT_ARRAY_MISMATCH", !AICF_LoadoutRecipe.Decode(recipe.Encode()));
		return binding;
	}

	protected bool ProbeChange(IEntity entity, string path, InventoryStorageManagerComponent manager, AICF_LoadoutCatalog catalog, AICF_LoadoutRecipe recipe, int kind)
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!AICF_LoadoutInventory.Editable(storage))
				continue;
			string id = AICF_LoadoutInventory.StorageId(entity, storage);
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				int slot = item.GetParentSlot().GetID();
				ResourceName original = SCR_ResourceNameUtils.GetPrefabName(item.GetOwner());
				SCR_EntityCatalogEntry entry = catalog.Find(original);
				SCR_ArsenalItem data;
				if (entry)
					data = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
				BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.GetOwner().FindComponent(BaseWeaponComponent));
				if ((weapon && kind == SCR_EArsenalItemType.RIFLE && weapon.GetWeaponType() == EWeaponType.WT_RIFLE) ||
					(data && kind == SCR_EArsenalItemType.HEADWEAR && (data.GetItemType() & kind) != 0))
				{
					array<string> candidates = {};
					catalog.List(kind, candidates);
					AICF_LoadoutInventory.DeleteLocal(item.GetOwner());
					foreach (ResourceName candidate : candidates)
					{
						if (candidate == original)
							continue;
						IEntity inserted = AICF_LoadoutInventory.InsertLocal(candidate, storage, slot, manager);
						if (!inserted)
							continue;
						string reason;
						if (kind == SCR_EArsenalItemType.HEADWEAR || AICF_LoadoutInventory.HasUsableWeapons(manager.GetOwner(), reason))
						{
							recipe.Add(path, id, slot, candidate, 1);
							Print(string.Format("[AICF][LOADOUT_PROBE] changed_kind=%1 before=%2 after=%3", kind, original, candidate));
							return true;
						}
						AICF_LoadoutInventory.DeleteLocal(inserted);
					}
					AICF_LoadoutInventory.InsertLocal(original, storage, slot, manager);
					return false;
				}
				if (ProbeChange(item.GetOwner(), path + id + "#" + slot.ToString() + "/", manager, catalog, recipe, kind))
					return true;
			}
		}
		return false;
	}

	protected bool ProbeInsert(IEntity entity, string path, InventoryStorageManagerComponent manager, AICF_LoadoutCatalog catalog, AICF_LoadoutRecipe recipe, bool attachment)
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!AICF_LoadoutInventory.Editable(storage))
				continue;
			string id = AICF_LoadoutInventory.StorageId(entity, storage);
			if (attachment == (WeaponAttachmentsStorageComponent.Cast(storage) != null) && !path.IsEmpty())
			{
				int kind = SCR_EArsenalItemType.HEAL;
				int quantity = 2;
				if (attachment)
				{
					kind = SCR_EArsenalItemType.WEAPON_ATTACHMENT;
					quantity = 1;
				}
				array<string> candidates = {};
				catalog.List(kind, candidates);
				foreach (ResourceName candidate : candidates)
				{
					IEntity first = AICF_LoadoutInventory.InsertLocal(candidate, storage, -1, manager);
					if (!first)
						continue;
					if (quantity == 2 && !AICF_LoadoutInventory.InsertLocal(candidate, storage, -1, manager))
					{
						AICF_LoadoutInventory.DeleteLocal(first);
						continue;
					}
					recipe.Add(path, id, -1, candidate, quantity);
					Print(string.Format("[AICF][LOADOUT_PROBE] inserted=%1 quantity=%2 path=%3", candidate, quantity, path));
					return true;
				}
			}
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				if (ProbeInsert(item.GetOwner(), path + id + "#" + item.GetParentSlot().GetID().ToString() + "/", manager, catalog, recipe, attachment))
					return true;
			}
		}
		return false;
	}
}
