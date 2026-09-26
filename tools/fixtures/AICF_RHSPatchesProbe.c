// Только изолированная копия RHS: 20 штатных ролей, inventory и повторная выдача.
// -aicfPatchProbe 1; завершает сервер после проверки, не входит в gameplay addon.
modded class AICF_MatchController
{
	protected int m_iAICFPatchProbeTicks;
	protected ref array<SCR_ChimeraCharacter> m_aAICFPatchModels = {};
	protected ref array<string> m_aAICFPatchInventory = {};

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
			for (int i; i < 10; i++)
			{
				string role;
				ResourceName prefab = m_GroupSpawner.ResolveRecruitPrefab(faction, i, role);
				EntitySpawnParams spawn = new EntitySpawnParams();
				spawn.TransformMode = ETransformMode.WORLD;
				spawn.Transform[3] = anchor.GetOrigin() + Vector(i * 3, 0, 10);
				spawn.Transform[3][1] = GetGame().GetWorld().GetSurfaceY(spawn.Transform[3][0], spawn.Transform[3][2]) + 0.1;
				SCR_ChimeraCharacter model = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), null, spawn));
				if (!model) continue;
				AIControlComponent control = AIControlComponent.Cast(model.FindComponent(AIControlComponent));
				if (control) control.DeactivateAI();
				DamageManagerComponent damage = DamageManagerComponent.Cast(model.FindComponent(DamageManagerComponent));
				if (damage) damage.EnableDamageHandling(false);
				m_aAICFPatchModels.Insert(model);
				m_aAICFPatchInventory.Insert(AICF_PatchInventory(model, false));
				Print(string.Format("[AICF][PATCH_PROBE_MODEL] entity=%1 faction=%2 role=%3 prefab=%4", model.GetID(), faction.GetFactionKey(), role, prefab));
			}
		}
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
			bool ok = attached > 0 && missing == 0 && preserved && idempotent;
			if (ok) passed++;
			Print(string.Format("[AICF][PATCH_PROBE_CHECK] entity=%1 attached=%2 missing=%3 preserved=%4 idempotent=%5 passed=%6", model.GetID(), attached, missing, preserved, idempotent, ok));
		}
		Print(string.Format("[AICF][PATCH_PROBE_RESULT] expected=20 actual=%1 passed=%2", m_aAICFPatchModels.Count(), passed));
	}
}
