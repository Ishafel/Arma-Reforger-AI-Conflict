// Изолированный controlled replacement stress: настоящая гибель и штатный lifecycle.
// Fixture пополняет supplies баз, но не меняет лимиты AI, deadline, очередь или здоровье.
modded class AICF_MatchController
{
	protected bool m_bAICFReplacementStressStarted;
	protected bool m_bAICFReplacementStressDone;
	protected int m_iAICFReplacementStressStart;
	protected int m_iAICFReplacementStressWave;
	protected int m_iAICFReplacementStressFailures;
	protected int m_iAICFReplacementStressWarmup = 90000;
	protected bool m_bAICFReplacementStressCapacity;
	protected bool m_bAICFReplacementStressCapacityReleased;
	protected bool m_bAICFReplacementStressCapacityObserved;
	protected bool m_bAICFReplacementStressInjected;
	protected int m_iAICFReplacementStressSavedLimit = -1;
	protected int m_iAICFReplacementStressCapacityKillAt;
	protected ref array<int> m_aAICFReplacementStressGenerations = {};
	protected ref array<bool> m_aAICFReplacementStressReady = {};
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aAICFReplacementStressBuildings = {};

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFReplacementStressStarted ||
			!System.GetCLIParam("aicfReplacementStress", enabled) || enabled != "1") return;
		m_bAICFReplacementStressStarted = true;
		string capacity;
		m_bAICFReplacementStressCapacity = System.GetCLIParam("aicfReplacementStressWaitCapacity", capacity) && capacity == "1";
		string injected;
		m_bAICFReplacementStressInjected = System.GetCLIParam("aicfReplacementStressInjectCapacity", injected) && injected == "1";
		if (m_bAICFReplacementStressInjected) m_bAICFReplacementStressCapacity = true;
		m_iAICFReplacementStressStart = System.GetTickCount();
		m_Construction.Stop();
		AICF_ReplacementStressPrepareBase(m_USFaction);
		AICF_ReplacementStressPrepareBase(m_USSRFaction);
		string warmup;
		if (System.GetCLIParam("aicfReplacementStressWarmupMs", warmup))
			m_iAICFReplacementStressWarmup = Math.ClampInt(warmup.ToInt(), 0, 300000);
		string classification = "CONTROLLED_REPLACEMENT_STRESS";
		int waves = 2;
		if (m_bAICFReplacementStressCapacity) { classification = "CONTROLLED_NATIVE_CAPACITY_STRESS"; waves = 1; }
		if (m_bAICFReplacementStressInjected) classification = "CONTROLLED_NATIVE_CAPACITY_INJECTION";
		Print(string.Format("[AICF][REPLACEMENT_STRESS_START] warmup_ms=%1 selected_per_side=3 waves=%2 deadline_ms=30000 classification=%3 fixture_barracks=1 fixture_supplies=1", m_iAICFReplacementStressWarmup, waves, classification));
		GetGame().GetCallqueue().CallLater(AICF_ReplacementStressTick, 250, true);
	}

	protected bool AICF_ReplacementStressCanStart()
	{
		for (int side; side < 2; side++)
		{
			AICF_FactionState state = m_USState;
			if (side == 1) state = m_USSRState;
			for (int index; index < 3; index++)
			{
				AICF_GroupSlot slot = state.GetSlot(index);
				if (slot.GetState() != AICF_EGroupSlotState.READY || !slot.GetGroup() || AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) == 0) return false;
			}
		}
		if (!m_bAICFReplacementStressCapacity) return true;
		if (m_bAICFReplacementStressInjected) { m_iAICFReplacementStressCapacityKillAt = System.GetTickCount(); return true; }
		AIWorld world = GetGame().GetAIWorld();
		if (!world) return false;
		int active = world.GetCurrentNumOfActiveAIs();
		int intended = world.GetActiveGroupIntendedCount();
		int limit = world.GetLimitOfActiveAIs();
		int free = limit - Math.Max(active, intended);
		if (limit <= 0 || free >= 5) return false;
		Print(string.Format("[AICF][REPLACEMENT_STRESS_PRESSURE] active=%1 intended=%2 limit=%3 free=%4 desired=5 group_can_activate=%5 limit_changed=0 importance_changed=0", active, intended, limit, free, world.CanActivateGroup(m_USState.GetSlot(0).GetGroup())));
		m_iAICFReplacementStressCapacityKillAt = System.GetTickCount();
		return true;
	}

	protected void AICF_ReplacementStressInjectCapacity()
	{
		if (!m_bAICFReplacementStressInjected || m_iAICFReplacementStressSavedLimit >= 0 ||
			m_iAICFReplacementStressCapacityKillAt <= 0 || System.GetTickCount() - m_iAICFReplacementStressCapacityKillAt < 1000) return;
		AIWorld world = GetGame().GetAIWorld();
		m_iAICFReplacementStressSavedLimit = world.GetLimitOfActiveAIs();
		// Fixture-only: штатный builder budget writer иначе отменяет fault в следующем tick.
		if (m_BaseBuilders) m_BaseBuilders.Stop();
		int active = world.GetCurrentNumOfActiveAIs();
		int intended = world.GetActiveGroupIntendedCount();
		int injectedLimit = Math.Min(m_iAICFReplacementStressSavedLimit, Math.Max(active, intended) + 1);
		world.SetLimitOfActiveAIs(injectedLimit);
		int limit = world.GetLimitOfActiveAIs();
		int canActivate = -1;
		SCR_AIGroup probe = m_USState.GetSlot(0).GetGroup();
		if (probe) { canActivate = 0; if (world.CanActivateGroup(probe)) canActivate = 1; }
		Print(string.Format("[AICF][REPLACEMENT_STRESS_PRESSURE] active=%1 intended=%2 limit=%3 free=%4 desired=5 group_can_activate=%5 limit_changed=1 importance_changed=0 original_limit=%6 classification=CONTROLLED_NATIVE_CAPACITY_INJECTION", active, intended, limit, limit - Math.Max(active, intended), canActivate, m_iAICFReplacementStressSavedLimit));
	}

	protected void AICF_ReplacementStressRestoreCapacity()
	{
		if (!m_bAICFReplacementStressInjected || m_iAICFReplacementStressSavedLimit < 0 || !GetGame() || !GetGame().GetAIWorld()) return;
		AIWorld world = GetGame().GetAIWorld();
		int originalLimit = m_iAICFReplacementStressSavedLimit;
		world.SetLimitOfActiveAIs(originalLimit);
		m_iAICFReplacementStressSavedLimit = -1;
		Print(string.Format("[AICF][REPLACEMENT_STRESS_NATIVE_LIMIT_RESTORED] original=%1 actual=%2", originalLimit, world.GetLimitOfActiveAIs()));
	}

	protected void AICF_ReplacementStressReleaseCapacity()
	{
		if (!m_bAICFReplacementStressCapacity || m_bAICFReplacementStressCapacityReleased ||
			m_iAICFReplacementStressCapacityKillAt <= 0 || System.GetTickCount() - m_iAICFReplacementStressCapacityKillAt < 65000) return;
		m_bAICFReplacementStressCapacityReleased = true;
		AIWorld world = GetGame().GetAIWorld();
		if (m_bAICFReplacementStressInjected)
		{
			AICF_ReplacementStressRestoreCapacity();
			Print(string.Format("[AICF][REPLACEMENT_STRESS_CAPACITY_RELEASE] elapsed_since_kill_ms=%1 active_before=%2 intended_before=%3 limit=%4 mechanism=RESTORE_ORIGINAL_NATIVE_LIMIT limit_changed=1 importance_changed=0", System.GetTickCount() - m_iAICFReplacementStressCapacityKillAt, world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount(), world.GetLimitOfActiveAIs()));
			return;
		}
		Print(string.Format("[AICF][REPLACEMENT_STRESS_CAPACITY_RELEASE] elapsed_since_kill_ms=%1 active_before=%2 intended_before=%3 limit=%4 mechanism=KILL_OTHER_OWN_MANAGED_AI limit_changed=0 importance_changed=0", System.GetTickCount() - m_iAICFReplacementStressCapacityKillAt, world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount(), world.GetLimitOfActiveAIs()));
		for (int side; side < 2; side++)
		{
			AICF_FactionState state = m_USState;
			if (side == 1) state = m_USSRState;
			for (int index = 3; index < state.GetSlotCount(); index++)
			{
				SCR_AIGroup group = state.GetSlot(index).GetGroup();
				if (!group) continue;
				array<AIAgent> agents = {};
				group.GetAgents(agents);
				foreach (AIAgent agent : agents)
				{
					if (!agent) continue;
					ChimeraCharacter actor = ChimeraCharacter.Cast(agent.GetControlledEntity());
					if (!actor) continue;
					SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(actor.GetDamageManager());
					if (damage) damage.Kill(Instigator.CreateInstigatorGM());
				}
			}
		}
	}

	protected void AICF_ReplacementStressPrepareBase(SCR_CampaignFaction faction)
	{
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		if (!base || !base.GetMasterProvider()) return;
		vector position = base.GetMasterProvider().GetOwner().GetOrigin() + "50 0 30";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab(faction.GetFactionKey(), AICF_EConstructionType.SMALL_BARRACKS);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (!entity) return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation) affiliation.SetAffiliatedFaction(faction);
		SCR_CampaignBuildingCompositionComponent building = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (!building) { SCR_EntityHelper.DeleteEntityAndChildren(entity); return; }
		building.SetProviderEntity(base.GetMasterProvider().GetOwner());
		m_aAICFReplacementStressBuildings.Insert(building);
	}

	protected void AICF_ReplacementStressWave()
	{
		m_iAICFReplacementStressWave++;
		m_aAICFReplacementStressGenerations.Clear();
		m_aAICFReplacementStressReady.Clear();
		for (int side; side < 2; side++)
		{
			AICF_FactionState state = m_USState;
			if (side == 1) state = m_USSRState;
			for (int index; index < 3; index++)
			{
				AICF_GroupSlot slot = state.GetSlot(index);
				SCR_AIGroup group = slot.GetGroup();
				slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
				slot.SetDesiredSize(5);
				m_aAICFReplacementStressGenerations.Insert(slot.GetSpawnGeneration());
				m_aAICFReplacementStressReady.Insert(false);
				array<AIAgent> agents = {};
				if (group) group.GetAgents(agents);
				Print(string.Format("[AICF][REPLACEMENT_STRESS_KILL] wave=%1 faction=%2 numeric_slot=%3 generation=%4 group=%5 actual=%6", m_iAICFReplacementStressWave, state.GetFactionKey(), index, slot.GetSpawnGeneration(), AICF_GroupRuntime.FormatEntityId(group), agents.Count()));
				foreach (AIAgent agent : agents)
				{
					if (!agent) continue;
					ChimeraCharacter actor = ChimeraCharacter.Cast(agent.GetControlledEntity());
					if (!actor) continue;
					SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(actor.GetDamageManager());
					if (damage) damage.Kill(Instigator.CreateInstigatorGM());
				}
			}
		}
	}

	protected void AICF_ReplacementStressTick()
	{
		if (m_bAICFReplacementStressDone) return;
		int elapsed = System.GetTickCount() - m_iAICFReplacementStressStart;
		int limitMs = 600000;
		if (m_bAICFReplacementStressCapacity) limitMs = 1200000;
		if (elapsed > limitMs) { m_iAICFReplacementStressFailures++; AICF_ReplacementStressFinish(); return; }
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aAICFReplacementStressBuildings)
		{
			if (building && !building.IsCompositionSpawned() && building.GetCompositionLayout())
			{
				SCR_CampaignBuildingLayoutComponent layout = building.GetCompositionLayout();
				layout.AddBuildingValue(layout.GetToBuildValue());
			}
		}
		for (int factionIndex; factionIndex < 2; factionIndex++)
		{
			SCR_CampaignFaction funded = m_USFaction;
			if (factionIndex == 1) funded = m_USSRFaction;
			SCR_CampaignMilitaryBaseComponent mainBase = funded.GetMainBase();
			if (mainBase) mainBase.AddSupplies(mainBase.GetSuppliesMax() - mainBase.GetSupplies());
		}
		if (elapsed < m_iAICFReplacementStressWarmup) return;
		if (m_iAICFReplacementStressWave == 0)
		{
			if (!m_ConflictAdapter.GetReplacementSpawnRejectionReason(m_USFaction.GetMainBase(), m_USFaction).IsEmpty() ||
				!m_ConflictAdapter.GetReplacementSpawnRejectionReason(m_USSRFaction.GetMainBase(), m_USSRFaction).IsEmpty()) return;
			if (!AICF_ReplacementStressCanStart()) return;
			AICF_ReplacementStressWave(); return;
		}
		if (m_iAICFReplacementStressWave > 0 && !m_bAICFReplacementStressCapacityReleased) AICF_ReplacementStressInjectCapacity();
		AICF_ReplacementStressReleaseCapacity();
		int ready;
		for (int side; side < 2; side++)
		{
			AICF_FactionState state = m_USState;
			SCR_CampaignFaction faction = m_USFaction;
			if (side == 1) { state = m_USSRState; faction = m_USSRFaction; }
			for (int index; index < 3; index++)
			{
				int record = side * 3 + index;
				if (m_aAICFReplacementStressReady[record]) { ready++; continue; }
				AICF_GroupSlot slot = state.GetSlot(index);
				if (m_bAICFReplacementStressCapacity && !m_bAICFReplacementStressCapacityObserved &&
					slot.GetState() == AICF_EGroupSlotState.WAITING && slot.GetSpawnGeneration() == m_aAICFReplacementStressGenerations[record] &&
					System.GetTickCount() - m_iAICFReplacementStressCapacityKillAt >= 35000)
				{
					AIWorld waitingWorld = GetGame().GetAIWorld();
					int activeWaiting = waitingWorld.GetCurrentNumOfActiveAIs();
					int intendedWaiting = waitingWorld.GetActiveGroupIntendedCount();
					int limitWaiting = waitingWorld.GetLimitOfActiveAIs();
					int freeWaiting = limitWaiting - Math.Max(activeWaiting, intendedWaiting);
					if (freeWaiting < 5)
					{
						m_bAICFReplacementStressCapacityObserved = true;
						Print(string.Format("[AICF][REPLACEMENT_STRESS_PRESSURE_HOLD] faction=%1 numeric_slot=%2 generation=%3 active=%4 intended=%5 limit=%6 free=%7 request_age_ms=0 actual=0 global_can_activate=%8 phase=WAITING_NO_NEW_GENERATION no_new_generation=1 elapsed_since_kill_ms=%9",
							state.GetFactionKey(), index, slot.GetSpawnGeneration(), activeWaiting, intendedWaiting, limitWaiting, freeWaiting, waitingWorld.CanAIBeActivated(), System.GetTickCount() - m_iAICFReplacementStressCapacityKillAt));
					}
				}
				if (m_bAICFReplacementStressCapacity && !m_bAICFReplacementStressCapacityObserved &&
					slot.GetState() == AICF_EGroupSlotState.SPAWNING && slot.GetGroup() && slot.GetRosterSpawnRequestAgeMs() >= 5000)
				{
					AIWorld world = GetGame().GetAIWorld();
					SCR_AIGroup pending = slot.GetGroup();
					int active = world.GetCurrentNumOfActiveAIs();
					int intended = world.GetActiveGroupIntendedCount();
					int limit = world.GetLimitOfActiveAIs();
					int free = limit - Math.Max(active, intended);
					if (pending.GetAgentsCount() < 5 && free < 5 && !world.CanAIBeActivated())
					{
						m_bAICFReplacementStressCapacityObserved = true;
						Print(string.Format("[AICF][REPLACEMENT_STRESS_PRESSURE_HOLD] faction=%1 numeric_slot=%2 generation=%3 active=%4 intended=%5 limit=%6 free=%7 request_age_ms=%8 actual=%9",
							state.GetFactionKey(), index, slot.GetSpawnGeneration(), active, intended, limit, free, slot.GetRosterSpawnRequestAgeMs(), pending.GetAgentsCount()) + " global_can_activate=0 phase=SPAWNING " + pending.AICF_DescribeSpawnDispatch());
					}
				}
				if (m_bAICFReplacementStressCapacity && slot.GetSpawnGeneration() > m_aAICFReplacementStressGenerations[record] &&
					slot.GetState() == AICF_EGroupSlotState.WAITING)
				{
					m_iAICFReplacementStressFailures++;
					Print(string.Format("[AICF][REPLACEMENT_STRESS_FIRST_ATTEMPT_FAILED] faction=%1 numeric_slot=%2 generation=%3", state.GetFactionKey(), index, slot.GetSpawnGeneration()));
					AICF_ReplacementStressFinish(); return;
				}
				if (slot.GetSpawnGeneration() <= m_aAICFReplacementStressGenerations[record] || slot.GetState() != AICF_EGroupSlotState.READY) continue;
				int actual, wrongFaction, nonAlive;
				bool exact = AICF_GroupRuntime.HasExactFactionRoster(slot.GetGroup(), state.GetFactionKey(), 5, actual, wrongFaction, nonAlive);
				bool firstGeneration = slot.GetSpawnGeneration() == m_aAICFReplacementStressGenerations[record] + 1;
				if (!exact || !firstGeneration) m_iAICFReplacementStressFailures++;
				Print(string.Format("[AICF][REPLACEMENT_STRESS_READY] wave=%1 faction=%2 numeric_slot=%3 generation=%4 exact=%5 first_generation=%6 actual=%7", m_iAICFReplacementStressWave, state.GetFactionKey(), index, slot.GetSpawnGeneration(), exact, firstGeneration, actual));
				Print("[AICF][REPLACEMENT_STRESS_SNAPSHOT] " + AICF_GroupRuntime.BuildSpawnSnapshot(faction, slot, 5));
				m_aAICFReplacementStressReady[record] = true;
				ready++;
			}
		}
		if (ready < 6) return;
		if (m_bAICFReplacementStressCapacity && !m_bAICFReplacementStressCapacityReleased) return;
		if (m_bAICFReplacementStressCapacity && !m_bAICFReplacementStressCapacityObserved) m_iAICFReplacementStressFailures++;
		if (!m_bAICFReplacementStressCapacity && m_iAICFReplacementStressWave < 2) AICF_ReplacementStressWave();
		else AICF_ReplacementStressFinish();
	}

	protected void AICF_ReplacementStressFinish()
	{
		m_bAICFReplacementStressDone = true;
		GetGame().GetCallqueue().Remove(AICF_ReplacementStressTick);
		Print(string.Format("[AICF][REPLACEMENT_STRESS_DONE] waves=%1 failures=%2 elapsed_ms=%3", m_iAICFReplacementStressWave, m_iAICFReplacementStressFailures, System.GetTickCount() - m_iAICFReplacementStressStart));
		Stop(true);
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_ReplacementStressTick);
		AICF_ReplacementStressRestoreCapacity();
		super.Stop(cleanupEntities);
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aAICFReplacementStressBuildings)
		{
			if (building && building.GetOwner()) SCR_EntityHelper.DeleteEntityAndChildren(building.GetOwner());
		}
		m_aAICFReplacementStressBuildings.Clear();
	}
}
