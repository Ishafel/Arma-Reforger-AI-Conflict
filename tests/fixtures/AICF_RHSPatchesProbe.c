// Только изолированная копия RHS: 33 роли + по 8 одинаковых бойцов трёх фракций.
// Проверяет inventory, повторную выдачу и разнообразие внутри одного prefab.
// -aicfPatchProbe 1; завершает сервер после проверки, не входит в gameplay addon.
modded class AICF_MatchController
{
	protected int m_iAICFPatchProbeTicks;
	protected ref array<SCR_ChimeraCharacter> m_aAICFPatchModels = {};
	protected ref array<string> m_aAICFPatchInventory = {};
	protected ref array<bool> m_aAICFPatchRepeated = {};

	override protected void Update()
	{
		string enabled;
		if (!System.GetCLIParam("aicfPatchProbe", enabled) || enabled != "1" || !m_bRosterReady)
		{
			super.Update();
			return;
		}
		m_iAICFPatchProbeTicks++;
		if (m_iAICFPatchProbeTicks == 1) AICF_SpawnPatchModels();
		// FIA сначала полностью меняет снаряжение. Сверяем последующие
		// операции с уже готовым комплектом ЧВК, а не со штатным FIA inventory.
		if (m_iAICFPatchProbeTicks == 4)
		{
			foreach (int index, SCR_ChimeraCharacter model : m_aAICFPatchModels)
			{
				if (model.GetFactionKey() == "FIA") m_aAICFPatchInventory[index] = AICF_PatchInventory(model, false);
			}
		}
		if (m_iAICFPatchProbeTicks == 8) AICF_CheckPatchModels();
		if (m_iAICFPatchProbeTicks == 10) GetGame().RequestClose();
	}

	protected void AICF_SpawnPatchModels()
	{
		array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
		foreach (SCR_CampaignFaction faction : factions)
		{
			IEntity anchor = AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(0).GetGroup());
			if (faction == m_USSRFaction) anchor = AICF_GroupRuntime.ResolveAliveLeader(m_USSRState.GetSlot(0).GetGroup());
			if (!anchor) continue;
			for (int i; i < 18; i++)
			{
				string role;
				int roleIndex = i;
				if (i >= 10) roleIndex = 0;
				ResourceName prefab = m_GroupSpawner.ResolveRecruitPrefab(faction, roleIndex, role);
				AICF_SpawnPatchModel(prefab, anchor.GetOrigin() + Vector(i * 3, 0, 10), i >= 10);
			}
		}
		SCR_Faction fia = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey("FIA"));
		array<SCR_EntityCatalogEntry> entries = {};
		fia.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER).GetEntityList(entries);
		array<string> roles = {"Rifleman", "SL", "Medic", "MG", "AMG", "LAT", "AT", "AAT", "Sharpshooter", "RTO", "Sapper", "Ammo", "Scout"};
		for (int repeat; repeat < 8; repeat++) roles.Insert("Rifleman");
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(0).GetGroup());
		foreach (int roleIndex, string suffix : roles)
		{
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				if (!entry.GetPrefab().EndsWith("/Character_FIA_" + suffix + ".et")) continue;
				AICF_SpawnPatchModel(entry.GetPrefab(), leader.GetOrigin() + Vector(roleIndex * 3, 0, 20), roleIndex >= 13);
				break;
			}
		}
	}

	protected void AICF_SpawnPatchModel(ResourceName prefab, vector position, bool repeated)
	{
		EntitySpawnParams spawn = new EntitySpawnParams();
		spawn.TransformMode = ETransformMode.WORLD;
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.1;
		spawn.Transform[3] = position;
		SCR_ChimeraCharacter model = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), null, spawn));
		if (!model) return;
		AIControlComponent control = AIControlComponent.Cast(model.FindComponent(AIControlComponent));
		if (control) control.DeactivateAI();
		DamageManagerComponent damage = DamageManagerComponent.Cast(model.FindComponent(DamageManagerComponent));
		if (damage) damage.EnableDamageHandling(false);
		m_aAICFPatchModels.Insert(model);
		m_aAICFPatchInventory.Insert(AICF_PatchInventory(model, false));
		m_aAICFPatchRepeated.Insert(repeated);
		Print(string.Format("[AICF][PATCH_PROBE_MODEL] entity=%1 faction=%2 repeated=%3 prefab=%4", model.GetID(), model.GetFactionKey(), repeated, prefab));
	}

	protected string AICF_PatchInventory(SCR_ChimeraCharacter model, bool patchesOnly)
	{
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
		array<IEntity> items = {};
		manager.GetItems(items);
		array<string> signature = {};
		foreach (IEntity item : items)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			if (prefab.Contains("/Patches/") != patchesOnly) continue;
			InventoryItemComponent component = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			string parent;
			if (component && component.GetParentSlot())
				parent = component.GetParentSlot().GetSourceName();
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			int ammo = -1;
			if (magazine) ammo = magazine.GetAmmoCount();
			signature.Insert(item.GetID().ToString() + ":" + prefab + ":" + parent + ":" + ammo.ToString());
		}
		signature.Sort();
		return SCR_StringHelper.Join(";", signature);
	}

	protected void AICF_CheckPatchModels()
	{
		int passed;
		foreach (int index, SCR_ChimeraCharacter model : m_aAICFPatchModels)
		{
			InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
			array<BaseInventoryStorageComponent> storages = {};
			manager.GetStorages(storages);
			int attached;
			int missing;
			foreach (BaseInventoryStorageComponent storage : storages)
			{
				for (int i; i < storage.GetSlotsCount(); i++)
				{
					InventoryStorageSlot slot = storage.GetSlot(i);
					if (!AICF_RHSDefaultPatches.IsPatchSlot(slot)) continue;
					IEntity patch = slot.GetAttachedEntity();
					if (patch) attached++;
					else if (!slot.IsLocked() && manager.CanInsertResourceInStorage(AICF_RHSDefaultPatches.Flag(model.GetFactionKey()), storage, i)) missing++;
					Print(string.Format("[AICF][PATCH_PROBE_SLOT] entity=%1 owner=%2 slot=%3 patch=%4", model.GetID(), SCR_ResourceNameUtils.GetPrefabName(storage.GetOwner()), slot.GetSourceName(), SCR_ResourceNameUtils.GetPrefabName(patch)));
				}
			}
			bool preserved = AICF_PatchInventory(model, false) == m_aAICFPatchInventory[index];
			string before = AICF_PatchInventory(model, true);
			AICF_RHSDefaultPatches.Apply(model);
			bool idempotent = before == AICF_PatchInventory(model, true);
			bool armament = true;
			if (model.GetFactionKey() == "FIA")
			{
				ResourceName source = SCR_ResourceNameUtils.GetPrefabName(model);
				armament = !model.AICF_IsPMCEquipmentPending() && AICF_RHSPMCArmament.Validate(model, source, AICF_RHSPMCEquipment.Variant(source));
			}
			bool ok = attached > 0 && missing == 0 && preserved && idempotent && armament;
			if (ok) passed++;
			Print(string.Format("[AICF][PATCH_PROBE_CHECK] entity=%1 attached=%2 missing=%3 preserved=%4 idempotent=%5 passed=%6", model.GetID(), attached, missing, preserved, idempotent, ok));
		}
		AICF_CheckPatchVariety("RHS_USAF");
		AICF_CheckPatchVariety("RHS_AFRF");
		AICF_CheckPatchVariety("FIA");
		Print(string.Format("[AICF][PATCH_PROBE_RESULT] expected=57 actual=%1 passed=%2", m_aAICFPatchModels.Count(), passed));
	}

	protected void AICF_CheckPatchVariety(FactionKey faction)
	{
		array<string> appearances = {};
		array<ResourceName> sources = {};
		int repeated;
		foreach (int index, SCR_ChimeraCharacter model : m_aAICFPatchModels)
		{
			if (m_aAICFPatchRepeated[index] && model.GetFactionKey() == faction)
			{
				repeated++;
				ResourceName source = SCR_ResourceNameUtils.GetPrefabName(model);
				if (!sources.Contains(source)) sources.Insert(source);
				array<IEntity> items = {};
				InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
				manager.GetItems(items);
				array<string> patches = {};
				foreach (IEntity item : items)
				{
					ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
					if (prefab.Contains("/Patches/")) patches.Insert(prefab);
				}
				patches.Sort();
				string appearance = SCR_StringHelper.Join(";", patches);
				if (!appearances.Contains(appearance)) appearances.Insert(appearance);
				Print(string.Format("[AICF][PATCH_VARIETY_MODEL] entity=%1 patches=%2", model.GetID(), appearance));
			}
		}
		bool variety = repeated == 8 && sources.Count() == 1 && appearances.Count() == 8;
		Print(string.Format("[AICF][PATCH_VARIETY_CHECK] faction=%1 repeated=%2 source_prefabs=%3 unique_sets=%4 passed=%5", faction, repeated, sources.Count(), appearances.Count(), variety));
	}
}
