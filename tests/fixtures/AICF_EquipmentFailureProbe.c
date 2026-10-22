// Только isolated Arland WCS + RHS stage; production CLI не расширяется.
class AICF_EquipmentFailureProbe
{
	static int s_iChecks;
	static int s_iFailures;
	static int s_iBuilds;
	static bool s_bFailBuild;
	static int s_iPreviewRefreshes;
	static int s_iDeferredPreviewRefreshes;
	static int s_iDeferredCampaignRefreshes;
	static ref array<IEntity> s_aCleared = {};

	static void Check(string name, bool passed)
	{
		s_iChecks++;
		if (!passed) s_iFailures++;
		Print(string.Format("[AICF][EQUIPMENT_PROBE][CHECK] name=%1 passed=%2", name, passed));
	}

	static void RetryPolicy()
	{
		AICF_EquipmentPlanFailures failures = new AICF_EquipmentPlanFailures();
		string key = "US|source|binding";
		Check("initial", failures.CanAttempt(key, 100));
		failures.Record(key, 100);
		Check("same_key_blocked", !failures.CanAttempt(key, 100));
		Check("before_deadline", !failures.CanAttempt(key, 120099));
		Check("other_faction", failures.CanAttempt("USSR|source|binding", 101));
		Check("other_source", failures.CanAttempt("US|other|binding", 101));
		Check("other_binding", failures.CanAttempt("US|source|other", 101));
		Check("clock_backwards", !failures.CanAttempt(key, 99));
		Check("deadline", failures.CanAttempt(key, 120100) && failures.Count() == 0);
		for (int index; index < AICF_EquipmentPlanFailures.MAX_ENTRIES; index++) failures.Record(index.ToString(), 200000);
		Check("capacity", failures.Count() == AICF_EquipmentPlanFailures.MAX_ENTRIES);
		failures.Record("overflow", 200001);
		Check("bounded", failures.Count() == AICF_EquipmentPlanFailures.MAX_ENTRIES && !failures.CanAttempt("overflow", 200001));
		Check("no_early_eviction", !failures.CanAttempt("0", 200001));
		Check("expired_capacity", failures.CanAttempt("overflow", 320000) && failures.Count() == 0);
		failures.Record(key, 320000);
		failures.Forget(key);
		Check("forget", failures.CanAttempt(key, 320001));
		failures.Record(key, 320000);
		failures.Clear();
		Check("clear", failures.Count() == 0 && failures.CanAttempt(key, 320001));
	}

	static int Retained()
	{
		int alive;
		foreach (IEntity item : s_aCleared) if (item) alive++;
		return alive;
	}
}

modded class AICF_WCSInfantryEquipment
{
	static ref array<ResourceName> s_aAICFLaunchers = {};
	static void AICF_RecordLauncher(ResourceName prefab, string exactPath = "")
	{
		if (prefab.Contains("Launcher_") && !s_aAICFLaunchers.Contains(prefab)) s_aAICFLaunchers.Insert(prefab);
	}

	static void AICF_TestSecondary(IEntity model, FactionKey faction, ResourceName source)
	{
		InventoryStorageSlot slot = SecondaryLauncherSlot(model);
		if (!slot) { AICF_EquipmentFailureProbe.Check("secondary_slot", false); return; }
		InventoryStorageManagerComponent manager = AICF_EquipmentInventory.Manager(model);
		IEntity original = slot.GetAttachedEntity();
		AICF_EquipmentFailureProbe.Check("existing_CG_idempotent", ArmSecondaryLauncher(model, faction, source) && original == slot.GetAttachedEntity());
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(original.FindComponent(BaseWeaponComponent));
		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		for (int barrel; barrel < muzzle.GetBarrelsCount(); barrel++) muzzle.ClearChamber(barrel);
		AICF_EquipmentFailureProbe.Check("empty_CG_rejected", !ArmSecondaryLauncher(model, faction, source) && original == slot.GetAttachedEntity());
		muzzle = null;
		weapon = null;
		AICF_LoadoutInventory.DeleteLocal(original);
		IEntity rifle = AICF_LoadoutInventory.InsertLocal(AICF_WCSInfantryKit.Weapon(faction, source), slot.GetStorage(), slot.GetID(), manager);
		AICF_EquipmentFailureProbe.Check("secondary_rifle_preserved", rifle && !ArmSecondaryLauncher(model, faction, source) && slot.GetAttachedEntity() == rifle);
		if (rifle) AICF_LoadoutInventory.DeleteLocal(rifle);
		SearchResourcesFilter filter = new SearchResourcesFilter();
		filter.rootPath = "$ArmaReforger:Prefabs/Weapons/Launchers";
		filter.fileExtensions = {"et"};
		ResourceDatabase.SearchResources(filter, AICF_RecordLauncher);
		s_aAICFLaunchers.Sort();
		int alternatives;
		foreach (ResourceName prefab : s_aAICFLaunchers)
		{
			if (prefab.Contains("M2CG") || prefab.Contains("M72A3") || prefab.Contains("_base")) continue;
			IEntity item = AICF_LoadoutInventory.InsertLocal(prefab, slot.GetStorage(), slot.GetID(), manager);
			if (!item) continue;
			BaseWeaponComponent candidate = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			bool disposable = candidate && candidate.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER && candidate.GetCurrentMuzzle() && candidate.GetCurrentMuzzle().IsDisposable();
			candidate = null;
			if (disposable)
			{
				alternatives++;
				AICF_EquipmentFailureProbe.Check("alternate_disposable_replaced", ArmSecondaryLauncher(model, faction, source) && HasSecondaryLauncher(model, faction, source));
			}
			else AICF_EquipmentFailureProbe.Check("reloadable_preserved", !ArmSecondaryLauncher(model, faction, source) && slot.GetAttachedEntity() == item);
			if (slot.GetAttachedEntity()) AICF_LoadoutInventory.DeleteLocal(slot.GetAttachedEntity());
		}
		AICF_EquipmentFailureProbe.Check("alternate_disposable_tested", alternatives > 0);
		AICF_EquipmentFailureProbe.Check("empty_slot_CG", ArmSecondaryLauncher(model, faction, source) && HasSecondaryLauncher(model, faction, source));
	}
}

modded class RHS_EquipmentStorageComponent
{
	protected int m_iAICFRequestDepth;
	override protected void RequestAttachedItemVisualsRefresh(int slotID = -1)
	{
		m_iAICFRequestDepth++;
		super.RequestAttachedItemVisualsRefresh(slotID);
		m_iAICFRequestDepth--;
		if (!System.IsCLIParam("aicfEquipmentFailureProbe") || !GetOwner()) return;
		if (GetOwner().GetWorld() != GetGame().GetWorld())
		{
			AICF_EquipmentFailureProbe.s_iPreviewRefreshes++;
		}
	}
	override protected void RefreshAttachedItemVisuals(int slotID = -1)
	{
		if (System.IsCLIParam("aicfEquipmentFailureProbe") && !m_iAICFRequestDepth && GetOwner())
		{
			if (GetOwner().GetWorld() != GetGame().GetWorld()) AICF_EquipmentFailureProbe.s_iDeferredPreviewRefreshes++;
			else AICF_EquipmentFailureProbe.s_iDeferredCampaignRefreshes++;
		}
		super.RefreshAttachedItemVisuals(slotID);
	}
}

modded class AICF_EquipmentDraft
{
	override void Clear()
	{
		array<IEntity> items = {};
		if (System.IsCLIParam("aicfEquipmentFailureProbe") && m_Character)
		{
			AICF_EquipmentInventory.Manager(m_Character).GetItems(items);
			AICF_EquipmentInventory.UniqueItems(items, AICF_EquipmentFailureProbe.s_aCleared);
		}
		super.Clear();
		if (!items.IsEmpty())
		{
			int alive;
			foreach (IEntity item : items) if (item) alive++;
			AICF_EquipmentFailureProbe.Check("immediate_native_cleanup", alive == 0);
		}
	}
}

modded class AICF_EquipmentPlan
{
	override bool Build(ResourceName source, SCR_CampaignFaction faction, AICF_LoadoutBinding binding)
	{
		if (System.IsCLIParam("aicfEquipmentFailureProbe")) AICF_EquipmentFailureProbe.s_iBuilds++;
		bool result = super.Build(source, faction, binding);
		if (AICF_EquipmentFailureProbe.s_bFailBuild)
		{
			AICF_EquipmentFailureProbe.Check("injected_after_native_build", result);
			m_sFailureReason = "FIXTURE_FAILURE_AFTER_BUILD";
			return false;
		}
		return result;
	}
}

modded class AICF_EquipmentService
{
	void AICF_TestFailures(SCR_CampaignFaction faction)
	{
		AICF_EquipmentService isolated = new AICF_EquipmentService(m_Campaign, m_Planner, m_Graph, m_Selector, m_RosterSource);
		isolated.AICF_TestFailureKeys(faction);
	}

	void AICF_TestFailureKeys(SCR_CampaignFaction faction)
	{
		AICF_GroupSlot slot = new AICF_GroupSlot(0, AICF_EGroupRole.ATTACK);
		AICF_EquipmentFailureProbe.s_bFailBuild = true;
		int before = AICF_EquipmentFailureProbe.s_iBuilds;
		AICF_EquipmentFailureProbe.Check("failed_plan_null", !Plan(slot, faction, 3));
		for (int attempt; attempt < 32; attempt++) Plan(slot, faction, 3);
		AICF_EquipmentFailureProbe.Check("single_failed_build", AICF_EquipmentFailureProbe.s_iBuilds == before + 1);
		Plan(slot, faction, 6);
		AICF_EquipmentFailureProbe.Check("independent_source_builds", AICF_EquipmentFailureProbe.s_iBuilds == before + 2);
		AICF_LoadoutBinding binding = new AICF_LoadoutBinding();
		binding.m_Recipe = new AICF_LoadoutRecipe();
		// Валидный inventory той же роли даёт другой key без ложного native отказа.
		string role;
		AICF_EquipmentPlan model = new AICF_EquipmentPlan();
		AICF_EquipmentFailureProbe.s_bFailBuild = false;
		bool built = model.Build(m_RosterSource.ResolveRecruitPrefab(faction, 3, role), faction, null);
		AICF_EquipmentFailureProbe.Check("binding_source_build", built);
		if (built) binding.m_sInventory = model.m_sInventory;
		if (built) AICF_WCSInfantryEquipment.AICF_TestSecondary(model.m_Draft.GetCharacter(), faction.GetFactionKey(), m_RosterSource.ResolveRecruitPrefab(faction, 3, role));
		model = null;
		AICF_EquipmentFailureProbe.Check("binding_revision", slot.SetLoadout(3, binding, 0));
		AICF_EquipmentFailureProbe.s_bFailBuild = true;
		before = AICF_EquipmentFailureProbe.s_iBuilds;
		Plan(slot, faction, 3);
		AICF_EquipmentFailureProbe.Check("changed_binding_builds", AICF_EquipmentFailureProbe.s_iBuilds == before + 1);
		AICF_EquipmentFailureProbe.s_bFailBuild = false;
		Stop();
		AICF_EquipmentFailureProbe.Check("stop_clears_failures", m_PlanFailures.Count() == 0);
	}

	void AICF_TestPlans(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		for (int index; index < 10; index++)
		{
			AICF_EquipmentPlan plan = Plan(slot, faction, index);
			AICF_EquipmentFailureProbe.Check(faction.GetFactionKey() + "_plan_" + index.ToString(), plan != null);
			if (!plan) continue;
			string role;
			ResourceName source = m_RosterSource.ResolveRecruitPrefab(faction, index, role);
			if (faction.GetFactionKey() == "RHS_USAF" && !AICF_WCSInfantryKit.SecondaryLauncher(faction.GetFactionKey(), source).IsEmpty())
				AICF_EquipmentFailureProbe.Check("M2CG_" + index.ToString(), AICF_WCSInfantryEquipment.HasSecondaryLauncher(plan.m_Draft.GetCharacter(), faction.GetFactionKey(), source));
		}
	}
}

modded class AICF_MatchController
{
	protected int m_iAICFEquipmentProbeReady;
	override protected void Update()
	{
		super.Update();
		if (!m_bRosterReady || !System.IsCLIParam("aicfEquipmentFailureProbe")) return;
		if (!m_iAICFEquipmentProbeReady)
		{
			m_iAICFEquipmentProbeReady = System.GetTickCount();
			AICF_EquipmentFailureProbe.RetryPolicy();
			m_EquipmentService.AICF_TestPlans(m_USState.GetSlot(0), m_USFaction);
			m_EquipmentService.AICF_TestPlans(m_USSRState.GetSlot(0), m_USSRFaction);
			m_EquipmentService.AICF_TestFailures(m_USFaction);
		}
		if (System.GetTickCount(m_iAICFEquipmentProbeReady) < 30000) return;
		AICF_EquipmentFailureProbe.Check("delayed_native_cleanup", AICF_EquipmentFailureProbe.s_aCleared.Count() > 0 && AICF_EquipmentFailureProbe.Retained() == 0);
		Print(string.Format("[AICF][EQUIPMENT_PROBE][VISUALS] private_requests=%1 private_deferred=%2 campaign_deferred=%3", AICF_EquipmentFailureProbe.s_iPreviewRefreshes, AICF_EquipmentFailureProbe.s_iDeferredPreviewRefreshes, AICF_EquipmentFailureProbe.s_iDeferredCampaignRefreshes));
		AICF_EquipmentFailureProbe.Check("private_refresh_no_deferred_calls", AICF_EquipmentFailureProbe.s_iPreviewRefreshes > 0 && AICF_EquipmentFailureProbe.s_iDeferredPreviewRefreshes == 0);
		AICF_EquipmentFailureProbe.Check("campaign_refresh_still_deferred", AICF_EquipmentFailureProbe.s_iDeferredCampaignRefreshes > 0);
		Print(string.Format("[AICF][EQUIPMENT_PROBE][DONE] checks=%1 failures=%2 sampled=%3 alive=%4", AICF_EquipmentFailureProbe.s_iChecks, AICF_EquipmentFailureProbe.s_iFailures, AICF_EquipmentFailureProbe.s_aCleared.Count(), AICF_EquipmentFailureProbe.Retained()));
		GetGame().RequestClose();
	}
}
