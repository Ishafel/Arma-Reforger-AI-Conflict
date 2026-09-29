// Только изолированный stage. Ошибки Restore включаются на один synchronous case.
modded class AICF_LoadoutInventory
{
	static int s_iAICFDeployFault;
	static int s_iAICFDeployRestoreCalls;

	override static bool Restore(IEntity entity, string value)
	{
		if (!s_iAICFDeployFault || entity.GetWorld() != GetGame().GetWorld()) return super.Restore(entity, value);
		s_iAICFDeployRestoreCalls++;
		if (s_iAICFDeployRestoreCalls == 1)
		{
			// Ошибка после изменения inventory: rollback обязан вернуть исходное.
			super.Restore(entity, value);
			return false;
		}
		if (s_iAICFDeployFault == 2) return false;
		if (s_iAICFDeployFault == 3) return true; // Ложный успех без восстановления.
		return super.Restore(entity, value);
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFWCSDeployProbeDone;
	protected int m_iAICFDeployTotal;
	protected int m_iAICFDeployPassed;

	protected void AICF_DeployCase(SCR_FactionPlayerLoadout loadout, SCR_CampaignFaction faction, ResourceName prefab, int fault)
	{
		m_iAICFDeployTotal++;
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = faction.GetMainBase().GetOwner().GetOrigin();
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
		if (character) loadout.OnLoadoutSpawned(character, 0);
		// Native SCR_SpawnProtectionComponent делает это перед WCS PrepareEntity_S.
		SCR_DamageManagerComponent damage;
		if (character) damage = SCR_DamageManagerComponent.Cast(character.FindComponent(SCR_DamageManagerComponent));
		if (damage) damage.EnableDamageHandling(false);
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		int ignored;
		string before = AICF_LoadoutInventory.Signature(character, catalog, ignored);
		AICF_LoadoutInventory.s_iAICFDeployFault = fault;
		AICF_LoadoutInventory.s_iAICFDeployRestoreCalls = 0;
		bool restored;
		bool applied = AICF_WCSPlayerEquipment.TryApply(character, loadout, restored);
		AICF_LoadoutInventory.s_iAICFDeployFault = 0;
		bool passed;
		if (fault == 0)
		{
			string role;
			ResourceName leader = m_GroupSpawner.ResolveRecruitPrefab(faction, 0, role);
			passed = applied && !restored && AICF_WCSInfantryEquipment.ValidateWCS(character, faction.GetFactionKey(), leader);
		}
		else if (fault == 1)
			passed = !applied && restored && !before.IsEmpty() && AICF_LoadoutInventory.Signature(character, catalog, ignored) == before;
		else
			passed = !applied && !restored;
		if (passed) m_iAICFDeployPassed++;
		Print(string.Format("[AICF][WCS_DEPLOY_CASE] faction=%1 prefab=%2 fault=%3 applied=%4 restored=%5 passed=%6", loadout.GetFactionKey(), prefab, fault, applied, restored, passed));
		if (character) RplComponent.DeleteRplEntity(character, false);
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfWCSPlayerDeployProbe", enabled) || enabled != "1" || !m_bRosterReady || m_bAICFWCSDeployProbeDone) return;
		m_bAICFWCSDeployProbeDone = true;
		foreach (SCR_BasePlayerLoadout baseLoadout : GetGame().GetLoadoutManager().GetPlayerLoadouts())
		{
			SCR_FactionPlayerLoadout loadout = SCR_FactionPlayerLoadout.Cast(baseLoadout);
			if (!AICF_WCSPlayerEquipment.IsDefault(loadout)) continue;
			SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(loadout.GetFactionKey()));
			if (!faction || !faction.GetMainBase()) continue;
			array<ResourceName> prefabs = {loadout.GetDefaultLoadoutResource()};
			Resource resource = Resource.Load(loadout.GetDefaultLoadoutResource());
			IEntityComponentSource source = SCR_BaseContainerTools.FindComponentSource(resource, SCR_EditableEntityComponent);
			SCR_EditableEntityVariantData variantData;
			if (source) source.Get("m_VariantData", variantData);
			array<SCR_EditableEntityVariant> variants = {};
			if (variantData) variantData.GetVariants(variants);
			foreach (SCR_EditableEntityVariant variant : variants)
				if (!prefabs.Contains(variant.m_sVariantPrefab)) prefabs.Insert(variant.m_sVariantPrefab);
			foreach (ResourceName prefab : prefabs)
				for (int fault; fault < 4; fault++) AICF_DeployCase(loadout, faction, prefab, fault);
		}
		Print(string.Format("[AICF][WCS_DEPLOY_RESULT] total=%1 passed=%2", m_iAICFDeployTotal, m_iAICFDeployPassed));
		GetGame().RequestClose();
	}
}
