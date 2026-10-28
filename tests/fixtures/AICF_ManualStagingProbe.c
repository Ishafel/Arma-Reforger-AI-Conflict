// Requires VehicleBehaviorProbe + VehicleBehaviorSettingsProbe scaffold, but no
// aicfVehicleBehaviorProbe flag. Own roster; controlled movement block, not war.
class AICF_ManualStagingProbeState
{
	static SCR_AIGroup s_Group;
	static ChimeraCharacter s_Actor;
	static AICF_TransportTrip s_Trip;
	static vector s_Before;
	static vector s_Destination;
	static float s_Health;
	static bool s_OldMovementDisabled;
	static int s_Checks;
	static int s_Failures;
	static int s_SetupAt;
	static int s_SubmittedAt;
	static int s_ApproachDeadline;
	static int s_TripDeadline;
	static ref AICF_VehicleSpawnPlan s_ExhaustionPlan;
	static int s_WaypointRemovals;
	static bool s_MissingReissueReported;
	static bool s_TwoAttemptsReported;
	static bool s_LiveWaypointReported;
	static bool s_ReissueRemovalReported;
	static AIWaypoint s_TemporaryWaypoint;
	static bool ExhaustionEnabled()
	{
		string flag;
		return Enabled() && System.GetCLIParam("aicfManualStagingExhaustionProbe", flag) && flag == "1";
	}
	static bool Enabled()
	{
		string flag;
		return System.GetCLIParam("aicfManualStagingProbe", flag) && flag == "1";
	}
	static void Check(string name, bool passed, string details = "")
	{
		s_Checks++;
		if (!passed) s_Failures++;
		Print(string.Format("[AICF][MANUAL_STAGING_CASE] case=%1 passed=%2 details=[%3]", name, passed, details));
	}
	static void RestoreMovement()
	{
		if (s_TemporaryWaypoint && s_Group)
		{
			array<AIWaypoint> queue = {}; s_Group.GetWaypoints(queue);
			if (queue.Contains(s_TemporaryWaypoint)) s_Group.RemoveWaypoint(s_TemporaryWaypoint);
			queue.Clear(); s_Group.GetWaypoints(queue);
			if (!queue.Contains(s_TemporaryWaypoint) && s_Group.GetCurrentWaypoint() != s_TemporaryWaypoint)
			{
				RplComponent.DeleteRplEntity(s_TemporaryWaypoint, false);
				s_TemporaryWaypoint = null;
			}
		}
		if (s_Actor && s_Actor.GetCharacterController())
			s_Actor.GetCharacterController().SetDisableMovementControls(s_OldMovementDisabled);
	}
}

modded class AICF_VehicleStagingActionToken
{
	SCR_AIMoveIndividuallyBehavior AICF_MSSwapAction(SCR_AIMoveIndividuallyBehavior replacement)
	{
		SCR_AIMoveIndividuallyBehavior original = m_Action;
		m_Action = replacement;
		return original;
	}
}

modded class AICF_VehicleManualStagingRecovery
{
	protected void AICF_MSNegative(string name, bool recovered, vector before, float health)
	{
		AICF_ManualStagingProbeState.Check(name, !recovered && !m_bConsumed &&
			!m_Plan.WasStagingMemberRelocated(m_Actor) && vector.Distance(before, m_Actor.GetOrigin()) < 0.01 &&
			m_Actor.GetDamageManager().GetHealthScaled() == health && m_Actor.GetCharacterController().GetLifeState() == ECharacterLifeState.ALIVE,
			"actual_original_native_actor=1 transform_health_life_unchanged=1 consumed=0");
	}
	override bool TryRecover()
	{
		if (AICF_ManualStagingProbeState.ExhaustionEnabled()) return super.TryRecover();
		if (!AICF_ManualStagingProbeState.Enabled() || m_Group != AICF_ManualStagingProbeState.s_Group ||
			!IsCurrent(System.GetTickCount())) return super.TryRecover();
		vector before = m_Actor.GetOrigin();
		float health = m_Actor.GetDamageManager().GetHealthScaled();
		m_iRequestGeneration++;
		AICF_MSNegative("STALE_REQUEST_NO_MUTATION", super.TryRecover(), before, health);
		m_iRequestGeneration--;
		m_iPresetRevision++;
		AICF_MSNegative("STALE_PRESET_NO_MUTATION", super.TryRecover(), before, health);
		m_iPresetRevision--;
		string originalRpl = m_sGroupRpl; m_sGroupRpl = "STALE";
		AICF_MSNegative("STALE_RPL_NO_MUTATION", super.TryRecover(), before, health);
		m_sGroupRpl = originalRpl;
		AICF_StrategicAssignmentSnapshot current = m_CurrentAssignment; m_CurrentAssignment = null;
		AICF_MSNegative("NO_CURRENT_ASSIGNMENT_NO_MUTATION", super.TryRecover(), before, health);
		m_CurrentAssignment = current;
		SCR_AIMoveIndividuallyBehavior original = m_Token.AICF_MSSwapAction(null);
		AICF_MSNegative("LOST_ACTION_NO_MUTATION", super.TryRecover(), before, health);
		m_Token.AICF_MSSwapAction(original);
		AICF_ManualStagingProbeState.Check("EXHAUSTED_TWO_OWNED_ATTEMPTS", m_Token.GetRetryCount() == 1 &&
			m_Plan.WasStagingMemberRetried(m_Actor) && m_Plan.IsStagingMemberExhausted(m_Actor) &&
			m_Token.IsActionOwnedByUtility() && !m_Plan.WasStagingMemberRelocated(m_Actor), "ordinary_owner_attempts=2 injected_clock=0");
		AICF_ManualStagingProbeState.s_Before = before;
		AICF_ManualStagingProbeState.s_Health = health;
		AICF_ManualStagingProbeState.s_ApproachDeadline = m_Plan.GetApproachDeadlineMs();
		AICF_ManualStagingProbeState.s_TripDeadline = m_Trip.GetAbsoluteDeadlineMs();
		bool submitted = super.TryRecover();
		AICF_ManualStagingProbeState.Check("SHORT_SUBMITTED_ONCE", submitted && m_bConsumed && m_Plan.WasStagingMemberRelocated(m_Actor));
		if (submitted)
		{
			AICF_ManualStagingProbeState.s_SubmittedAt = System.GetTickCount();
			AICF_ManualStagingProbeState.Check("DUPLICATE_NO_MUTATION", !super.TryRecover() && m_bConsumed &&
				m_Plan.GetApproachDeadlineMs() == AICF_ManualStagingProbeState.s_ApproachDeadline &&
				m_Trip.GetAbsoluteDeadlineMs() == AICF_ManualStagingProbeState.s_TripDeadline);
		}
		return submitted;
	}
}

modded class AICF_VehicleAcquisitionFlow
{
	override protected void MaintainStagingMemberActions(AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment, AICF_VehicleRequestState requestState,
		AICF_VehicleSpawnPlan plan, int nowMs)
	{
		if (AICF_ManualStagingProbeState.Enabled() && trip.GetAssignment().GetGroup() == AICF_ManualStagingProbeState.s_Group &&
			!AICF_ManualStagingProbeState.s_SetupAt && plan.RequiresManualRally())
		{
			ChimeraCharacter actor = ChimeraCharacter.Cast(trip.VehiclePresetMember(1));
			if (actor && actor.GetCharacterController())
			{
				vector direction = actor.GetOrigin() - plan.GetStagingPosition(); direction[1] = 0; direction.Normalize();
				float setupDistance = 28;
				if (AICF_ManualStagingProbeState.ExhaustionEnabled()) setupDistance = 40;
				vector query = plan.GetStagingPosition() + direction * setupDistance;
				vector destination;
				if (AICF_InfantrySpawnPlacement.IsUsable(trip.GetAssignment().GetGroup(), query, destination) &&
					vector.Distance(actor.GetOrigin(), destination) <= 25 && vector.DistanceSqXZ(query, destination) <= 0.25)
				{
					AICF_ManualStagingProbeState.s_Actor = actor;
					AICF_ManualStagingProbeState.s_Trip = trip;
					AICF_ManualStagingProbeState.s_ExhaustionPlan = plan;
					AICF_ManualStagingProbeState.s_ApproachDeadline = plan.GetApproachDeadlineMs();
					AICF_ManualStagingProbeState.s_TripDeadline = trip.GetAbsoluteDeadlineMs();
					AICF_ManualStagingProbeState.s_Health = actor.GetDamageManager().GetHealthScaled();
					AICF_ManualStagingProbeState.s_OldMovementDisabled = actor.GetCharacterController().GetDisableMovementControls();
					actor.GetCharacterController().SetDisableMovementControls(true);
					vector transform[4]; actor.GetWorldTransform(transform); transform[3] = destination;
					actor.Teleport(transform);
					AICF_ManualStagingProbeState.s_SetupAt = nowMs;
					AICF_ManualStagingProbeState.Check("CONTROLLED_NATIVE_MOVEMENT_BLOCK", !trip.GetLease() && !trip.IsVehiclePresetDebited(),
						string.Format("setup_destination=%1 setup_shift_m=%2 movement_controls_disabled=1 fixture_setup_only=1", destination, vector.Distance(actor.GetOrigin(), destination)));
				}
			}
		}
		bool exhaustionProbe = AICF_ManualStagingProbeState.ExhaustionEnabled() && trip == AICF_ManualStagingProbeState.s_Trip;
		AICF_VehicleStagingExhaustion proof = plan.GetManualStagingExhaustion();
		if (exhaustionProbe && !AICF_ManualStagingProbeState.s_MissingReissueReported)
		{
			// Capture actual counter0 before the engine can spend its sole reissue.
			AICF_ManualStagingProbeState.s_MissingReissueReported = true;
			AIWaypoint first = plan.GetApproachWaypoint();
			array<AIWaypoint> firstQueue = {}; trip.GetAssignment().GetGroup().GetWaypoints(firstQueue);
			bool nativeZero = plan.GetApproachWaypointReissueCount() == 0;
			AICF_ManualStagingProbeState.Check("EXHAUSTION_MISSING_REISSUE_NOT_FALLBACK", nativeZero && proof &&
				!proof.ShouldEnd(currentAssignment, m_Spawner, nowMs, m_Config.GetSpawnStagingRadiusMeters()),
				string.Format("actual_native_reissue_count=%1 before_member_attempt_exhaustion=1 isolated_counter_logic_covered_offline=1", plan.GetApproachWaypointReissueCount()));
			bool firstQueued = first && firstQueue.Contains(first);
			if (nativeZero && firstQueued) trip.GetAssignment().GetGroup().RemoveWaypoint(first);
			firstQueue.Clear(); trip.GetAssignment().GetGroup().GetWaypoints(firstQueue);
			AICF_ManualStagingProbeState.Check("EXHAUSTION_WAYPOINT_REMOVED_FIRST", nativeZero && firstQueued && !firstQueue.Contains(first),
				"controlled_exact_original_plan_waypoint_removal=1 actual_native_counter_unchanged=1");
			if (nativeZero && firstQueued && !firstQueue.Contains(first)) AICF_ManualStagingProbeState.s_WaypointRemovals++;
		}
		super.MaintainStagingMemberActions(trip, currentAssignment, requestState, plan, nowMs);
		if (!exhaustionProbe) return;
		IEntity blocked = AICF_ManualStagingProbeState.s_Actor;
		if (!blocked || !plan.IsStagingMemberExhausted(blocked) || plan.FindStagingActionToken(blocked)) return;
		if (!AICF_ManualStagingProbeState.s_TwoAttemptsReported)
		{
			AICF_ManualStagingProbeState.s_TwoAttemptsReported = true;
			AICF_ManualStagingProbeState.Check("EXHAUSTION_TWO_ATTEMPTS_SPENT", plan.WasStagingMemberRetried(blocked) &&
				plan.IsStagingMemberExhausted(blocked) && !plan.WasStagingMemberRelocated(blocked), "short_step_to_rally_impossible_from_40m=1");
		}
		if (plan.GetApproachWaypointReissueCount() != 1 || AICF_ManualStagingProbeState.s_ReissueRemovalReported) return;
		AIWaypoint waypoint = plan.GetApproachWaypoint();
		array<AIWaypoint> queue = {}; trip.GetAssignment().GetGroup().GetWaypoints(queue);
		bool replacementQueued = waypoint && queue.Contains(waypoint);
		if (replacementQueued)
		{
			AICF_ManualStagingProbeState.s_LiveWaypointReported = true;
			AICF_ManualStagingProbeState.Check("EXHAUSTION_LIVE_WAYPOINT_NOT_FALLBACK", proof &&
				!proof.ShouldEnd(currentAssignment, m_Spawner, nowMs, m_Config.GetSpawnStagingRadiusMeters()), "actual_reissue1_live_waypoint=1");
			trip.GetAssignment().GetGroup().RemoveWaypoint(waypoint);
			queue.Clear(); trip.GetAssignment().GetGroup().GetWaypoints(queue);
			AICF_ManualStagingProbeState.s_ReissueRemovalReported = true;
			AICF_ManualStagingProbeState.Check("EXHAUSTION_WAYPOINT_REMOVED_REISSUE", !queue.Contains(waypoint) &&
				plan.GetApproachWaypointReissueCount() == 1, "ordinary_fixed_reissue_budget=1 controlled_exact_replacement_remove=1");
			if (!queue.Contains(waypoint)) AICF_ManualStagingProbeState.s_WaypointRemovals++;
		}
		else
		{
			// Native may remove the replacement before this poll. A clearly foreign
			// fixture waypoint proves the any-queued-waypoint veto without plan rebinding.
			AICF_VehicleWaypointFactory factory = new AICF_VehicleWaypointFactory();
			AIWaypoint temporary = factory.CreateSpawnStagingWaypoint(plan.GetStagingPosition(), m_Config.GetSpawnStagingRadiusMeters());
			AICF_ManualStagingProbeState.s_TemporaryWaypoint = temporary;
			if (temporary) trip.GetAssignment().GetGroup().AddWaypoint(temporary);
			queue.Clear(); trip.GetAssignment().GetGroup().GetWaypoints(queue);
			bool nativeQueued = temporary && queue.Contains(temporary);
			AICF_ManualStagingProbeState.s_LiveWaypointReported = true;
			AICF_ManualStagingProbeState.Check("EXHAUSTION_LIVE_WAYPOINT_NOT_FALLBACK", nativeQueued && proof &&
				!proof.ShouldEnd(currentAssignment, m_Spawner, nowMs, m_Config.GetSpawnStagingRadiusMeters()),
				"fixture_temporary_native_waypoint=1 plan_rebound=0 original_replacement_already_native_unqueued=1");
			if (temporary) trip.GetAssignment().GetGroup().RemoveWaypoint(temporary);
			queue.Clear(); trip.GetAssignment().GetGroup().GetWaypoints(queue);
			bool detached = temporary && !queue.Contains(temporary) && trip.GetAssignment().GetGroup().GetCurrentWaypoint() != temporary;
			if (detached) { RplComponent.DeleteRplEntity(temporary, false); AICF_ManualStagingProbeState.s_TemporaryWaypoint = null; }
			AICF_ManualStagingProbeState.s_ReissueRemovalReported = true;
			AICF_ManualStagingProbeState.Check("EXHAUSTION_WAYPOINT_REMOVED_REISSUE", nativeQueued && detached &&
				plan.GetApproachWaypointReissueCount() == 1 && (!waypoint || !queue.Contains(waypoint)),
				"replacement_native_unqueued=1 temporary_removed_before_delete=1 actual_reissue_counter_unchanged=1");
		}
	}
}

modded class AICF_MatchController
{
	protected int m_iManualStagingStarted;
	protected int m_iManualStagingClearPolls;
	protected bool m_bManualStagingFinished;
	protected ref AICF_TransportTrip m_ManualStagingRestoreTrip;
	protected int m_iManualStagingRestoreDeadline;
	protected int m_iManualStagingRestoreNextPoll;
	protected int m_iManualStagingOriginalHandoffDeadline;

	override protected void Update()
	{
		super.Update();
		if (!AICF_ManualStagingProbeState.Enabled() || !Replication.IsServer() || !m_bRosterReady || m_bManualStagingFinished) return;
		int now = System.GetTickCount();
		if (!m_iManualStagingStarted)
		{
			m_iManualStagingStarted = now;
			m_sVBScenario = "transport";
			if (!AICF_VBInitialize()) { AICF_ManualStagingProbeState.Check("SETUP", false); AICF_MSFinish(); }
			return;
		}
		if (now - m_iManualStagingStarted > 180000 || AICF_ManualStagingProbeState.s_Failures || m_iVBFailures)
		{
			AICF_ManualStagingProbeState.Check("BOUNDED_FINISH", false); AICF_MSFinish(); return;
		}
		AICF_GroupSlot slot = m_VBState.GetSlot(0);
		if (!slot.IsCombatReady())
		{
			if (m_VBGroup.GetAgentsCount() != 5) return;
			if (!AICF_VBPrepare(slot)) { AICF_ManualStagingProbeState.Check("ROSTER_PRESET", false); AICF_MSFinish(); return; }
			AICF_ManualStagingProbeState.s_Group = m_VBGroup;
		}
		AICF_VBObserve();
		AICF_VBThreat(false);
		AICF_TransportTrip trip = m_VBCoordinator.GetTrip(slot);
		if (AICF_ManualStagingProbeState.ExhaustionEnabled() && ((trip && trip.IsTerminal()) || m_ManualStagingRestoreTrip))
		{
			if (!m_ManualStagingRestoreTrip) m_ManualStagingRestoreTrip = trip;
			AICF_MSExhaustionFinished(slot, m_ManualStagingRestoreTrip, now); return;
		}
		if (AICF_ManualStagingProbeState.s_SubmittedAt)
		{
			ChimeraCharacter actor = AICF_ManualStagingProbeState.s_Actor;
			AICF_VehicleSpawnPlan plan = trip.GetRequestState().GetSpawnPlan();
			int ready;
			bool exact = true;
			for (int member; member < 5; member++)
			{
				IEntity original = m_aVBMembers[member];
				exact = exact && original && original.GetID() == m_aVBMemberIds[member] && slot.GetRosterMember(member) == original &&
					trip.VehiclePresetMember(member) == original && AICF_GroupRuntime.IsAliveCharacter(original);
				if (AICF_VehicleSpawner.IsStagingGeometryReady(vector.DistanceXZ(original.GetOrigin(), plan.GetStagingPosition()),
					m_Stage3Config.GetSpawnStagingRadiusMeters(), vector.DistanceXZ(original.GetOrigin(), plan.GetSpawnPosition()), 90, true)) ready++;
			}
			float shift = vector.Distance(actor.GetOrigin(), AICF_ManualStagingProbeState.s_Before);
			if (ready == 5 && shift >= 2 && shift <= 8) m_iManualStagingClearPolls++;
			else m_iManualStagingClearPolls = 0;
			if (m_iManualStagingClearPolls < 2 && now - AICF_ManualStagingProbeState.s_SubmittedAt < 5000) return;
			AICF_ManualStagingProbeState.Check("ACTUAL_SHORT_TELEPORT", m_iManualStagingClearPolls >= 2 && shift <= 8,
				string.Format("actual_3d_shift_m=%1 clear_polls=%2", shift, m_iManualStagingClearPolls));
			AICF_ManualStagingProbeState.Check("ALL_FIVE_PHYSICAL_RALLY_BEFORE_PAYMENT", ready == 5 && !trip.GetLease() && !trip.IsVehiclePresetDebited());
			AICF_ManualStagingProbeState.Check("ORIGINAL_IDENTITIES_HEALTH_UNCHANGED", exact &&
				actor.GetCharacterController().GetLifeState() == ECharacterLifeState.ALIVE && actor.GetDamageManager().GetHealthScaled() == AICF_ManualStagingProbeState.s_Health);
			AICF_ManualStagingProbeState.Check("DEADLINES_UNCHANGED", plan.GetApproachDeadlineMs() == AICF_ManualStagingProbeState.s_ApproachDeadline &&
				trip.GetAbsoluteDeadlineMs() == AICF_ManualStagingProbeState.s_TripDeadline && now < plan.GetApproachDeadlineMs());
			AICF_ManualStagingProbeState.Check("NO_BUY_OR_DEBIT", !trip.IsVehiclePresetDebited() && !trip.GetLease() &&
				m_VBCoordinator.GetSpawnedCount(m_VBFaction.GetFactionKey()) == 0 && trip.GetRequestState().GetAttemptCount() == 0);
			AICF_MSFinish(); return;
		}
		if (trip && trip.IsTerminal()) { AICF_ManualStagingProbeState.Check("NO_PREMATURE_FALLBACK", false, trip.GetTerminalReason()); AICF_MSFinish(); return; }
		if (m_sVBFaction == "US") m_VBCoordinator.Update(m_VBState, m_VBFaction, null, null, m_iStrategicBaseRevision, true);
		else m_VBCoordinator.Update(null, null, m_VBState, m_VBFaction, m_iStrategicBaseRevision, true);
	}
	protected void AICF_MSFinish()
	{
		if (m_bManualStagingFinished) return;
		m_bManualStagingFinished = true;
		AICF_ManualStagingProbeState.RestoreMovement();
		AICF_ManualStagingProbeState.s_ExhaustionPlan = null;
		m_ManualStagingRestoreTrip = null;
		Print(string.Format("[AICF][MANUAL_STAGING_FINISHED] checks=%1 failures=%2 scope=CONTROLLED_NATIVE_MOVEMENT_BLOCK",
			AICF_ManualStagingProbeState.s_Checks, AICF_ManualStagingProbeState.s_Failures));
		AICF_VBFinish();
	}
	protected void AICF_MSExhaustionFinished(AICF_GroupSlot slot, AICF_TransportTrip trip, int nowMs)
	{
		AICF_VehicleHandoffState handoff = trip.GetHandoffState();
		if (!m_iManualStagingRestoreDeadline)
		{
			m_iManualStagingOriginalHandoffDeadline = handoff.GetAbsoluteOrderDeadlineMs();
			m_iManualStagingRestoreDeadline = Math.Min(nowMs + 30000, m_iManualStagingOriginalHandoffDeadline);
		}
		if (!handoff.IsOrderRestored())
		{
			if (m_sVBFaction == "US") m_VBCoordinator.Update(m_VBState, m_VBFaction, null, null, m_iStrategicBaseRevision, true);
			else m_VBCoordinator.Update(null, null, m_VBState, m_VBFaction, m_iStrategicBaseRevision, true);
		}
		// The isolated state is outside the root's normal faction arrays. Use the
		// actual production durability owner at its configured cadence, never clear
		// pending evidence or increment stable polls directly in the fixture.
		if (slot.HasPendingOrderRecovery() && nowMs >= m_iManualStagingRestoreNextPoll)
		{
			ProcessPendingOrderRecovery(m_VBState, slot, m_VBFaction);
			m_iManualStagingRestoreNextPoll = nowMs + m_Stage2Config.GetReliabilityIntervalMs();
		}
		AIWaypoint infantryWaypoint = slot.GetWaypoint();
		array<AIWaypoint> nativeQueue = {}; m_VBGroup.GetWaypoints(nativeQueue);
		bool exactQueued = infantryWaypoint && handoff.GetRestoredWaypoint() == infantryWaypoint &&
			m_VBGroup.GetCurrentWaypoint() == infantryWaypoint && nativeQueue.Contains(infantryWaypoint);
		bool sameIdentity = slot.GetGroup() == m_VBGroup && m_VBGroup.GetID() == m_VBGroupId && slot.GetSpawnGeneration() == trip.GetGroupGeneration();
		bool meaningful = HasMeaningfulTask(slot, m_VBCoordinator.GetSlotView(slot));
		string orderFailure = m_OrderPlanner.GetOrderFailureReason(slot, m_VBFaction);
		bool restored = handoff.IsOrderRestored() && exactQueued && sameIdentity && meaningful && orderFailure.IsEmpty() && !slot.HasPendingOrderRecovery();
		string observation = string.Format("operation_id=%1 original_group=%2 current_group=%3 infantry_wp=%4 current_wp=%5 queued=%6 handoff_restored=%7 pending=%8 meaningful=%9",
			trip.GetOperationId(), m_VBGroupId, slot.GetGroup().GetID(), AICF_GroupTrace.Entity(infantryWaypoint),
			AICF_GroupTrace.Entity(m_VBGroup.GetCurrentWaypoint()), exactQueued, handoff.IsOrderRestored(), slot.HasPendingOrderRecovery(), meaningful);
		observation += string.Format(" order_failure=%1 now_ms=%2 original_handoff_deadline_ms=%3 wait_deadline_ms=%4 restored=%5 owner=PRODUCTION_DURABILITY",
			orderFailure, nowMs, m_iManualStagingOriginalHandoffDeadline, m_iManualStagingRestoreDeadline, restored);
		Print("[AICF][MANUAL_STAGING_RESTORE_OBSERVATION] " + observation);
		if (!restored && nowMs < m_iManualStagingRestoreDeadline) return;
		AICF_ManualStagingProbeState.Check("EXHAUSTION_EARLY_TYPED_FALLBACK", trip.GetPhase() == AICF_ETransportTripPhase.FALLBACK &&
			trip.GetTerminalReason() == "SPAWN_STAGING_ACTIONS_EXHAUSTED" && trip.GetPhaseStartedAtMs() < AICF_ManualStagingProbeState.s_ApproachDeadline);
		AICF_ManualStagingProbeState.Check("EXHAUSTION_PLAN_RELEASED", !trip.GetRequestState().GetSpawnPlan() && !trip.GetLease());
		AICF_ManualStagingProbeState.Check("EXHAUSTION_ORDER_RESTORED", restored &&
			handoff.GetAbsoluteOrderDeadlineMs() == m_iManualStagingOriginalHandoffDeadline, observation);
		AICF_ManualStagingProbeState.Check("EXHAUSTION_NO_TELEPORT_DEBIT", !AICF_ManualStagingProbeState.s_SubmittedAt &&
			!trip.IsVehiclePresetDebited() && m_VBCoordinator.GetSpawnedCount(m_VBFaction.GetFactionKey()) == 0 &&
			AICF_ManualStagingProbeState.s_ExhaustionPlan && !AICF_ManualStagingProbeState.s_ExhaustionPlan.WasStagingMemberRelocated(AICF_ManualStagingProbeState.s_Actor));
		bool original = true;
		for (int position; position < 5; position++)
			original = original && m_aVBMembers[position] && slot.GetRosterMember(position) == m_aVBMembers[position] &&
				m_aVBMembers[position].GetID() == m_aVBMemberIds[position] && AICF_GroupRuntime.IsAliveCharacter(m_aVBMembers[position]);
		AICF_ManualStagingProbeState.Check("EXHAUSTION_ORIGINAL_HEALTH", original &&
			AICF_ManualStagingProbeState.s_Actor.GetDamageManager().GetHealthScaled() == AICF_ManualStagingProbeState.s_Health);
		AICF_ManualStagingProbeState.Check("EXHAUSTION_DEADLINE_UNCHANGED", trip.GetAbsoluteDeadlineMs() == AICF_ManualStagingProbeState.s_TripDeadline &&
			AICF_ManualStagingProbeState.s_ExhaustionPlan.GetApproachDeadlineMs() == AICF_ManualStagingProbeState.s_ApproachDeadline);
		AICF_MSFinish();
	}
	override protected void Stop(bool cleanupEntities)
	{
		AICF_ManualStagingProbeState.RestoreMovement();
		super.Stop(cleanupEntities);
	}
}
