// Controlled native capacity race. Same competing RequestSpawn(4), different
// selected target dispatch only. No native limit/importance/dormant count writes.
class AICF_NativeSpawnRaceProbe
{
	static AICF_GroupSlot s_Slot;
	static SCR_AIGroup s_Target;
	static SCR_AIGroup s_Competitor;
	static EntityID s_TargetId;
	static string s_sTargetRpl;
	static int s_iOriginalGeneration;
	static int s_iStartedAt;
	static int s_iLimit;
	static int s_iRequests;
	static int s_iExpands;
	static int s_iAdded;
	static int s_iCompleted;
	static int s_iOtherRequests;
	static int s_iBatchCreated;
	static int s_iPurges;
	static int s_iRequestAt;
	static bool s_bSelected;
	static bool s_bDone;
	static bool s_bInvalid;
	static bool s_bCapacityHold;
	static bool Enabled()
	{
		string flag;
		return Replication.IsServer() && System.GetCLIParam("aicfNativeSpawnRaceProbe", flag) && flag == "1";
	}
	static bool Batch()
	{
		string flag;
		return System.GetCLIParam("aicfNativeSpawnRaceBatch", flag) && (flag == "1" || flag == "2");
	}
	static bool ProductionBatch()
	{
		string flag;
		return System.GetCLIParam("aicfNativeSpawnRaceBatch", flag) && flag == "2";
	}
	static void Census(string boundary, SCR_AIGroup created = null)
	{
		AIWorld world = GetGame().GetAIWorld();
		array<AIAgent> agents = {};
		world.GetAIAgents(agents);
		int groups, countedGroups, actors, countedActors;
		foreach (AIAgent agent : agents)
		{
			if (!agent) continue;
			if (AIGroup.Cast(agent))
			{
				groups++;
				if (agent.CountTowardsAIWorldLimit()) countedGroups++;
			}
			else
			{
				actors++;
				if (agent.CountTowardsAIWorldLimit()) countedActors++;
			}
		}
		int createdCounts = -1;
		if (created) createdCounts = created.CountTowardsAIWorldLimit();
		string details = string.Format("boundary=%1 active=%2 intended=%3 hard=%4 agents=%5 groups=%6 counted_groups=%7 actors=%8 counted_actors=%9",
			boundary, world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount(), world.GetLimitOfActiveAIs(),
			agents.Count(), groups, countedGroups, actors, countedActors);
		Print("[AICF][NATIVE_SPAWN_RACE_CENSUS] " + details + string.Format(" created_counts=%1", createdCounts));
	}
	static bool ExactTarget()
	{
		if (!s_Slot || !s_Target || s_Slot.GetGroup() != s_Target || s_Target.GetID() != s_TargetId) return false;
		return s_Slot.GetSpawnGeneration() == s_iOriginalGeneration + 1 &&
			AICF_VehicleBehaviorBinding.ComponentRpl(s_Target) == s_sTargetRpl &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(s_Target);
	}
	static bool Fits(int remaining)
	{
		AIWorld world = GetGame().GetAIWorld();
		if (!world || world.GetLimitOfActiveAIs() != s_iLimit || remaining <= 0) return false;
		int active = world.GetCurrentNumOfActiveAIs();
		int intended = world.GetActiveGroupIntendedCount();
		return active >= 0 && intended >= 0 && world.CanAIBeActivated() &&
			s_iLimit - Math.Max(active, intended) >= remaining;
	}
	static void Ledger(string boundary)
	{
		AIWorld world = GetGame().GetAIWorld();
		int actual = -1;
		if (s_Target) actual = s_Target.GetAgentsCount();
		int other = -1;
		if (s_Competitor) other = s_Competitor.GetAgentsCount();
		string details = string.Format("boundary=%1 mode_batch=%2 target=%3 actual=%4 active=%5 intended=%6 limit=%7 target_requests=%8 expands=%9",
			boundary, Batch(), s_TargetId, actual, world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount(),
			world.GetLimitOfActiveAIs(), s_iRequests, s_iExpands);
		details += string.Format(" other_requests=%1 other_actual=%2 added=%3 completed=%4 generation=%5 original_generation=%6 started_at_ms=%7 request_at_ms=%8 queue_depth_observable=0",
			s_iOtherRequests, other, s_iAdded, s_iCompleted, s_Slot.GetSpawnGeneration(), s_iOriginalGeneration,
			s_Slot.GetSpawnStartedAtMs(), s_iRequestAt);
		Print("[AICF][NATIVE_SPAWN_RACE] " + details);
	}
	static void OnAdded(AIAgent agent)
	{
		if (!ExactTarget() || !agent || agent.GetParentGroup() != s_Target) { s_bInvalid = true; return; }
		s_iAdded++;
		Ledger("TARGET_NATIVE_AGENT_ADDED");
	}
	static void OnCompleted(SCR_AIGroup group)
	{
		if (!ExactTarget() || group != s_Target || group.GetAgentsCount() != 5) { s_bInvalid = true; return; }
		s_iCompleted++;
		Ledger("TARGET_NATIVE_ALL_COMPLETED");
	}
}

modded class SCR_AIGroup
{
	bool AICF_RaceNoAuthoredAttachments()
	{
		int vehicles = -1, staticWaypoints = -1, dynamicWaypoints = -1;
		if (m_aStaticVehicles) vehicles = m_aStaticVehicles.Count();
		if (m_aStaticWaypoints) staticWaypoints = m_aStaticWaypoints.Count();
		if (m_aSpawnedWaypoints) dynamicWaypoints = m_aSpawnedWaypoints.Count();
		Print(string.Format("[AICF][NATIVE_SPAWN_RACE_AUTHORED_SHAPE] vehicle_refs=%1 static_waypoints=%2 dynamic_waypoints=%3", vehicles, staticWaypoints, dynamicWaypoints));
		return vehicles == 0 && staticWaypoints == 0 && dynamicWaypoints == 0;
	}
	override void RequestSpawn(int slotsWanted = -1, float observerRange = 0)
	{
		if (AICF_NativeSpawnRaceProbe.Enabled())
		{
			if (this == AICF_NativeSpawnRaceProbe.s_Target)
			{
				AICF_NativeSpawnRaceProbe.s_iRequests++;
				AICF_NativeSpawnRaceProbe.Ledger("TARGET_REQUEST_SPAWN");
			}
			if (this == AICF_NativeSpawnRaceProbe.s_Competitor)
			{
				AICF_NativeSpawnRaceProbe.s_iOtherRequests++;
				if (slotsWanted != 4) AICF_NativeSpawnRaceProbe.s_bInvalid = true;
			}
		}
		super.RequestSpawn(slotsWanted, observerRange);
	}
	override bool ExpandOneMember()
	{
		bool selected = AICF_NativeSpawnRaceProbe.Enabled() && this == AICF_NativeSpawnRaceProbe.s_Target;
		if (selected)
		{
			AICF_NativeSpawnRaceProbe.s_iExpands++;
			AICF_NativeSpawnRaceProbe.Ledger("TARGET_BEFORE_NATIVE_EXPAND");
		}
		bool expanded = super.ExpandOneMember();
		if (selected) AICF_NativeSpawnRaceProbe.Ledger("TARGET_AFTER_NATIVE_EXPAND");
		return expanded;
	}
}

modded class AICF_GroupSpawner
{
	override SCR_AIGroup SpawnGroup(SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent spawnBase,
		int slotId, int desiredSize, bool managedDeployment = true)
	{
		bool observed = AICF_NativeSpawnRaceProbe.Enabled() && AICF_NativeSpawnRaceProbe.s_Slot &&
			AICF_NativeSpawnRaceProbe.s_Slot.GetSpawnGeneration() == AICF_NativeSpawnRaceProbe.s_iOriginalGeneration + 1 &&
			faction && faction.GetFactionKey() == "USSR" && (slotId == 0 || slotId == 9);
		if (observed) AICF_NativeSpawnRaceProbe.Census("BEFORE_EMPTY_GROUP_CREATE");
		SCR_AIGroup created = super.SpawnGroup(faction, spawnBase, slotId, desiredSize, managedDeployment);
		if (observed) AICF_NativeSpawnRaceProbe.Census("AFTER_EMPTY_GROUP_CREATE", created);
		return created;
	}

	override bool BeginRosterSpawn(SCR_AIGroup group, int expectedSize)
	{
		if (!AICF_NativeSpawnRaceProbe.Enabled() || !AICF_NativeSpawnRaceProbe.s_Slot ||
			group != AICF_NativeSpawnRaceProbe.s_Slot.GetGroup() || expectedSize != 5)
			return super.BeginRosterSpawn(group, expectedSize);
		AICF_GroupSlot slot = AICF_NativeSpawnRaceProbe.s_Slot;
		if (slot.GetSpawnGeneration() != AICF_NativeSpawnRaceProbe.s_iOriginalGeneration + 1 || !slot.IsReplacementDeployment())
			return super.BeginRosterSpawn(group, expectedSize);
		AICF_NativeSpawnRaceProbe.s_Target = group;
		AICF_NativeSpawnRaceProbe.s_TargetId = group.GetID();
		AICF_NativeSpawnRaceProbe.s_sTargetRpl = AICF_VehicleBehaviorBinding.ComponentRpl(group);
		AICF_NativeSpawnRaceProbe.s_iRequestAt = System.GetTickCount();
		AICF_NativeSpawnRaceProbe.s_bSelected = true;
		group.GetOnAgentAdded().Insert(AICF_NativeSpawnRaceProbe.OnAdded);
		group.GetOnAllDelayedEntitySpawned().Insert(AICF_NativeSpawnRaceProbe.OnCompleted);
		AICF_NativeSpawnRaceProbe.Ledger("BOUND_OBSERVERS_INSTALLED_BEFORE_REQUEST");
		if (!AICF_NativeSpawnRaceProbe.ExactTarget() || !slot.IsRosterSpawnRequested() ||
			group.GetAgentsCount() != 0 || !AICF_NativeSpawnRaceProbe.Fits(5))
			AICF_NativeSpawnRaceProbe.s_bInvalid = true;
		if (!group.AICF_RaceNoAuthoredAttachments()) AICF_NativeSpawnRaceProbe.s_bInvalid = true;
		// Identical competing native request in both cases; no member created here.
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(group.GetFaction());
		if (faction && faction.GetMainBase())
			AICF_NativeSpawnRaceProbe.s_Competitor = SpawnGroup(faction, faction.GetMainBase(), 9, 4, false);
		if (!AICF_NativeSpawnRaceProbe.s_Competitor || AICF_NativeSpawnRaceProbe.s_Competitor.GetAgentsCount() != 0)
		{
			AICF_NativeSpawnRaceProbe.s_bInvalid = true;
			return super.BeginRosterSpawn(group, expectedSize);
		}
		AICF_NativeSpawnRaceProbe.s_Competitor.SetNumberOfMembersToSpawn(4);
		AICF_NativeSpawnRaceProbe.s_Competitor.RequestSpawn(4);
		AICF_NativeSpawnRaceProbe.Ledger("SAME_OTHER_GROWTH_REQUEST_4_ENQUEUED");
		if (!AICF_NativeSpawnRaceProbe.Batch()) return super.BeginRosterSpawn(group, expectedSize);
		// Mode2 uses the real production bound caller/helper. The same fixture also
		// compiles against the old source; no references to the new API are needed.
		if (AICF_NativeSpawnRaceProbe.ProductionBatch()) return super.BeginRosterSpawn(group, expectedSize);
		// No script RequestSpawn has run, but native auto-expansion of an empty
		// configured group was observed in the previous control. Explicitly purge
		// only this fresh exact owned target; the native queue has no read getter.
		ChimeraAIWorld world = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!world || !AICF_NativeSpawnRaceProbe.ExactTarget() || group.GetAgentsCount() != 0)
		{
			AICF_NativeSpawnRaceProbe.s_bInvalid = true;
			return super.BeginRosterSpawn(group, expectedSize);
		}
		world.PurgeSpawnRequestsForGroup(group);
		AICF_NativeSpawnRaceProbe.s_iPurges++;
		AICF_NativeSpawnRaceProbe.Ledger("EXACT_OWN_TARGET_QUEUE_PURGED_BEFORE_BATCH");
		group.SetNumberOfMembersToSpawn(expectedSize);
		for (int index; index < expectedSize; index++)
		{
			int before = group.GetAgentsCount();
			if (AICF_NativeSpawnRaceProbe.s_bInvalid || !AICF_NativeSpawnRaceProbe.ExactTarget() ||
				!AICF_NativeSpawnRaceProbe.Fits(expectedSize - before)) break;
			bool expanded = group.ExpandOneMember();
			if (!expanded || group.GetAgentsCount() != before + 1) break;
			AICF_NativeSpawnRaceProbe.s_iBatchCreated++;
		}
		int remaining = expectedSize - group.GetAgentsCount();
		if (remaining > 0) group.RequestSpawn(remaining);
		AICF_NativeSpawnRaceProbe.Ledger("BOUND_BATCH_RETURNED");
		return true;
	}
}

modded class AICF_InfantryRecruitmentService
{
	override protected void ConsiderFaction(AICF_FactionState state, SCR_CampaignFaction faction)
	{
		if (AICF_NativeSpawnRaceProbe.Enabled()) return;
		super.ConsiderFaction(state, faction);
	}
}

modded class AICF_MatchController
{
	protected ref array<SCR_AIGroup> m_aRacePressure = {};
	protected ref array<int> m_aRacePressureExpected = {};
	protected ref array<EntityID> m_aRaceReadyMembers = {};
	protected ref array<string> m_aRaceReadyRpl = {};
	protected int m_iRacePhase;
	protected int m_iRaceReadyAt;
	protected int m_iRaceTicketsBefore;
	protected int m_iRaceLimitCandidate;
	protected int m_iRaceLimitStableAt;
	protected bool m_bRaceLimitCaptured;

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		if (!m_bRosterReady || AICF_NativeSpawnRaceProbe.s_iStartedAt > 0 || !AICF_NativeSpawnRaceProbe.Enabled()) return;
		AICF_NativeSpawnRaceProbe.s_iStartedAt = System.GetTickCount();
		AICF_NativeSpawnRaceProbe.s_iLimit = -1;
		AICF_NativeSpawnRaceProbe.s_Slot = m_USSRState.GetSlot(0);
		AICF_NativeSpawnRaceProbe.s_iOriginalGeneration = m_USSRState.GetSlot(0).GetSpawnGeneration();
		m_Construction.Stop();
		// Reuse only the stock ONLINE-barracks setup helper, never its stress wave.
		AICF_ReplacementStressPrepareBase(m_USSRFaction);
		for (int side; side < 2; side++)
		{
			AICF_FactionState state = m_USState;
			SCR_CampaignFaction faction = m_USFaction;
			if (side == 1) { state = m_USSRState; faction = m_USSRFaction; }
			for (int slotIndex; slotIndex < state.GetSlotCount(); slotIndex++)
			{
				AICF_GroupSlot slot = state.GetSlot(slotIndex);
				IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
				if (leader) m_OrderPlanner.AssignPlayerPointOrder(slot, faction, leader.GetOrigin(), false);
			}
		}
		// The competitor is created only inside the later target admission call.
		// No intervening native frame can pre-expand it before RequestSpawn(4).
		Print("[AICF][NATIVE_SPAWN_RACE_START] classification=CONTROLLED_NATIVE_QUEUE_COMPETITION other_requested=4 no_limit_writes=1 no_importance_writes=1 no_heal=1 no_actor_pose_writes=1 initial_request_owner=NATIVE_QUEUE batch_owner=FIXTURE_ONLY_AFTER_MANAGED_BIND");
		GetGame().GetCallqueue().CallLater(AICF_RaceTick, 250, true);
	}

	protected void AICF_RaceFinish(bool passed, string reason)
	{
		if (AICF_NativeSpawnRaceProbe.s_bDone) return;
		AICF_NativeSpawnRaceProbe.s_bDone = true;
		GetGame().GetCallqueue().Remove(AICF_RaceTick);
		if (AICF_NativeSpawnRaceProbe.s_Target)
		{
			AICF_NativeSpawnRaceProbe.s_Target.GetOnAgentAdded().Remove(AICF_NativeSpawnRaceProbe.OnAdded);
			AICF_NativeSpawnRaceProbe.s_Target.GetOnAllDelayedEntitySpawned().Remove(AICF_NativeSpawnRaceProbe.OnCompleted);
		}
		Print(string.Format("[AICF][NATIVE_SPAWN_RACE_DONE] passed=%1 reason=%2 mode_batch=%3 batch_created=%4 target_requests=%5 expands=%6 callbacks_added=%7 callbacks_completed=%8",
			passed, reason, AICF_NativeSpawnRaceProbe.Batch(), AICF_NativeSpawnRaceProbe.s_iBatchCreated,
			AICF_NativeSpawnRaceProbe.s_iRequests, AICF_NativeSpawnRaceProbe.s_iExpands,
			AICF_NativeSpawnRaceProbe.s_iAdded, AICF_NativeSpawnRaceProbe.s_iCompleted));
		GetGame().RequestClose();
	}

	protected int AICF_RacePressurePending()
	{
		int pending;
		for (int index; index < m_aRacePressure.Count(); index++)
		{
			SCR_AIGroup group = m_aRacePressure[index];
			if (!group) return -1;
			pending += Math.Max(0, m_aRacePressureExpected[index] - group.GetAgentsCount());
			int count, recovered;
			m_ManagedAILODPolicy.KeepCaptureEligible(group, count, recovered);
		}
		return pending;
	}

	protected void AICF_RaceTick()
	{
		if (AICF_NativeSpawnRaceProbe.s_bDone) return;
		AIWorld world = GetGame().GetAIWorld();
		int now = System.GetTickCount();
		if (now - AICF_NativeSpawnRaceProbe.s_iStartedAt > 600000) { AICF_RaceFinish(false, "SETUP_OR_TAIL_DEADLINE"); return; }
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aAICFReplacementStressBuildings)
			if (building && !building.IsCompositionSpawned() && building.GetCompositionLayout())
				building.GetCompositionLayout().AddBuildingValue(building.GetCompositionLayout().GetToBuildValue());
		SCR_CampaignMilitaryBaseComponent base = m_USSRFaction.GetMainBase();
		if (base.GetSupplies() < 1000) base.AddSupplies(1000 - base.GetSupplies());
		// Startup owner installs its builder-overhead budget after ROSTER_READY.
		// Capture a stable native value before pressure or any replacement clock.
		if (!m_bRaceLimitCaptured)
		{
			int candidate = world.GetLimitOfActiveAIs();
			if (candidate <= 0) return;
			if (candidate != m_iRaceLimitCandidate)
			{
				m_iRaceLimitCandidate = candidate; m_iRaceLimitStableAt = now;
				Print(string.Format("[AICF][NATIVE_SPAWN_RACE_SETUP_LIMIT] observed=%1 pressure_started=0 target_killed=0", candidate));
			}
			if (now - m_iRaceLimitStableAt < 5000) return;
			AICF_NativeSpawnRaceProbe.s_iLimit = candidate;
			m_bRaceLimitCaptured = true;
			Print(string.Format("[AICF][NATIVE_SPAWN_RACE_FIXED_LIMIT] limit=%1 observed_stable_ms=%2 writes=0", candidate, now - m_iRaceLimitStableAt));
		}
		if (world.GetLimitOfActiveAIs() != AICF_NativeSpawnRaceProbe.s_iLimit) { AICF_RaceFinish(false, "NATIVE_LIMIT_CHANGED"); return; }
		int pending = AICF_RacePressurePending();
		if (pending < 0) { AICF_RaceFinish(false, "PRESSURE_GROUP_LOST"); return; }
		AICF_GroupSlot slot = AICF_NativeSpawnRaceProbe.s_Slot;
		if (m_iRacePhase == 0)
		{
			int free = world.GetLimitOfActiveAIs() - Math.Max(world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount());
			if (pending > 0) return;
			if (free > 4)
			{
				int desired = Math.Min(10, free - 4);
				SCR_AIGroup pressure = m_GroupSpawner.SpawnGroup(m_USSRFaction, base, m_aRacePressure.Count() % 10, desired, false);
				if (!pressure) { AICF_RaceFinish(false, "PRESSURE_CREATE_FAILED"); return; }
				m_aRacePressure.Insert(pressure); m_aRacePressureExpected.Insert(desired);
				if (!m_GroupSpawner.BeginRosterSpawn(pressure, desired)) AICF_RaceFinish(false, "PRESSURE_REQUEST_FAILED");
				return;
			}
			if (free != 4 || slot.GetState() != AICF_EGroupSlotState.READY || !slot.GetGroup()) return;
			if (!m_ConflictAdapter.GetReplacementSpawnRejectionReason(base, m_USSRFaction).IsEmpty()) return;
			array<AIAgent> agents = {}; slot.GetGroup().GetAgents(agents);
			if (agents.Count() != 1) { AICF_RaceFinish(false, "INITIAL_ORIGINAL_NOT_SINGLE"); return; }
			ChimeraCharacter actor = ChimeraCharacter.Cast(agents[0].GetControlledEntity());
			if (!actor || !AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor)) { AICF_RaceFinish(false, "ORIGINAL_IDENTITY_LOST"); return; }
			m_iRaceTicketsBefore = m_USSRState.GetSpentTickets();
			slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT); slot.SetDesiredSize(5);
			m_iRacePhase = 1;
			Print(string.Format("[AICF][NATIVE_SPAWN_RACE_ORIGINAL_LOSS] generation=%1 member=%2 free_before=%3 fixture_pressure_groups=%4 controlled_death=1 resurrect=0",
				slot.GetSpawnGeneration(), actor.GetID(), free, m_aRacePressure.Count()));
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(actor.GetDamageManager());
			damage.Kill(Instigator.CreateInstigatorGM());
			return;
		}
		if (!AICF_NativeSpawnRaceProbe.s_bSelected) return;
		if (!AICF_NativeSpawnRaceProbe.ExactTarget()) { AICF_RaceFinish(false, "TARGET_IDENTITY_CHANGED"); return; }
		if (AICF_NativeSpawnRaceProbe.s_bInvalid) { AICF_RaceFinish(false, "TARGET_NATIVE_FENCE_INVALID"); return; }
		if (!world.CanAIBeActivated() && slot.GetGroup().GetAgentsCount() < 5 && AICF_NativeSpawnRaceProbe.s_Competitor.GetAgentsCount() > 0)
			AICF_NativeSpawnRaceProbe.s_bCapacityHold = true;
		if (slot.GetState() != AICF_EGroupSlotState.READY) return;
		int actual, wrong, nonAlive;
		if (!AICF_GroupRuntime.HasExactFactionRoster(slot.GetGroup(), "USSR", 5, actual, wrong, nonAlive)) { AICF_RaceFinish(false, "READY_NOT_EXACT_FIVE"); return; }
		array<AIAgent> readyAgents = {}; slot.GetGroup().GetAgents(readyAgents);
		if (m_iRaceReadyAt == 0)
		{
			m_iRaceReadyAt = now;
			foreach (AIAgent readyAgent : readyAgents)
			{
				IEntity member = readyAgent.GetControlledEntity();
				m_aRaceReadyMembers.Insert(member.GetID());
				m_aRaceReadyRpl.Insert(AICF_VehicleBehaviorBinding.ComponentRpl(member));
			}
			AICF_NativeSpawnRaceProbe.Ledger("ACTUAL_FIRST_NEXT_GENERATION_READY");
		}
		for (int memberIndex; memberIndex < readyAgents.Count(); memberIndex++)
		{
			IEntity same = readyAgents[memberIndex].GetControlledEntity();
			if (same.GetID() != m_aRaceReadyMembers[memberIndex] || AICF_VehicleBehaviorBinding.ComponentRpl(same) != m_aRaceReadyRpl[memberIndex])
				{ AICF_RaceFinish(false, "TAIL_MEMBER_CHANGED"); return; }
		}
		if (now - m_iRaceReadyAt < 35000) return;
		bool batchProof = AICF_NativeSpawnRaceProbe.Batch() && AICF_NativeSpawnRaceProbe.s_iBatchCreated == 5 &&
			AICF_NativeSpawnRaceProbe.s_iRequests == 0 && AICF_NativeSpawnRaceProbe.s_iExpands == 5 &&
			AICF_NativeSpawnRaceProbe.s_iAdded == 5 && AICF_NativeSpawnRaceProbe.s_iCompleted == 1 && AICF_NativeSpawnRaceProbe.s_iPurges == 1;
		if (AICF_NativeSpawnRaceProbe.ProductionBatch())
			batchProof = AICF_NativeSpawnRaceProbe.s_iRequests == 0 && AICF_NativeSpawnRaceProbe.s_iExpands == 5 &&
				AICF_NativeSpawnRaceProbe.s_iAdded == 5 && AICF_NativeSpawnRaceProbe.s_iCompleted == 1;
		batchProof = batchProof && Math.Max(world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount()) <= world.GetLimitOfActiveAIs();
		bool debit = m_USSRState.GetSpentTickets() - m_iRaceTicketsBefore == 1;
		AICF_NativeSpawnRaceProbe.Ledger("AFTER_35S_DUPLICATE_AND_NATIVE_BUDGET_TAIL");
		if (!AICF_NativeSpawnRaceProbe.Batch())
		{
			AICF_RaceFinish(false, "BASELINE_NOT_PARTIAL_CAPACITY_RACE"); return;
		}
		AICF_RaceFinish(batchProof && debit && AICF_NativeSpawnRaceProbe.s_iOtherRequests == 1, "FULL_ROSTER_NATIVE_CALLBACKS_SINGLE_DEBIT_TAIL");
	}

	override protected void HandleSpawnTimeout(AICF_FactionState factionState, SCR_CampaignFaction faction, AICF_GroupSlot slot)
	{
		bool selected = AICF_NativeSpawnRaceProbe.Enabled() && slot == AICF_NativeSpawnRaceProbe.s_Slot && AICF_NativeSpawnRaceProbe.s_bSelected;
		if (selected) AICF_NativeSpawnRaceProbe.Ledger("ACTUAL_30S_TIMEOUT_BEFORE_OWNER_CLEANUP");
		super.HandleSpawnTimeout(factionState, faction, slot);
		if (selected) AICF_RaceFinish(false, "ACTUAL_NATIVE_PARTIAL_CAPACITY_TIMEOUT");
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_RaceTick);
		if (AICF_NativeSpawnRaceProbe.s_Target)
		{
			AICF_NativeSpawnRaceProbe.s_Target.GetOnAgentAdded().Remove(AICF_NativeSpawnRaceProbe.OnAdded);
			AICF_NativeSpawnRaceProbe.s_Target.GetOnAllDelayedEntitySpawned().Remove(AICF_NativeSpawnRaceProbe.OnCompleted);
		}
		super.Stop(cleanupEntities);
	}
}
