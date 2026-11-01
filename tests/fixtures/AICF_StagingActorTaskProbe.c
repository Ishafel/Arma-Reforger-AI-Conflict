// Controlled acquisition task proof. Separate from natural war and Multi.
class AICF_StagingTaskProbeState
{
	static SCR_AIGroup s_Group;
	static ChimeraCharacter s_Actor;
	static bool s_OldDisabled;
	static bool s_Setup;
	static int s_Removed;
	static int s_Checks;
	static int s_Failures;
	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfStagingTaskProbe", value) && value == "1";
	}
	static void Check(string rule, bool passed, string details = "")
	{
		s_Checks++;
		if (!passed) s_Failures++;
		Print(string.Format("[AICF][STAGING_TASK_CASE] rule=%1 passed=%2 details=[%3]", rule, passed, details));
	}
	static void Restore()
	{
		if (s_Actor && s_Actor.GetCharacterController()) s_Actor.GetCharacterController().SetDisableMovementControls(s_OldDisabled);
	}
}

modded class AICF_VehicleStagingActionToken
{
	SCR_AIMoveIndividuallyBehavior AICF_STSwapAction(SCR_AIMoveIndividuallyBehavior action)
	{
		SCR_AIMoveIndividuallyBehavior original = m_Action; m_Action = action; return original;
	}
	void AICF_STRequestDelta(int delta) { m_iRequestGeneration += delta; }
	void AICF_STGoalDelta(vector delta) { m_vTargetPosition += delta; }
	AICF_StrategicAssignmentSnapshot AICF_STSwapAssignment(AICF_StrategicAssignmentSnapshot assignment)
	{
		AICF_StrategicAssignmentSnapshot original = m_TaskAssignment; m_TaskAssignment = assignment; return original;
	}
}

modded class AICF_VehicleAcquisitionFlow
{
	override protected void MaintainStagingMemberActions(AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot currentAssignment, AICF_VehicleRequestState requestState,
		AICF_VehicleSpawnPlan plan, int nowMs)
	{
		if (AICF_StagingTaskProbeState.Enabled() && trip.GetAssignment().GetGroup() == AICF_StagingTaskProbeState.s_Group)
		{
			if (!AICF_StagingTaskProbeState.s_Setup && plan.RequiresManualRally())
			{
				ChimeraCharacter actor;
				float farthest = -1;
				for (int member; member < 5; member++)
				{
					ChimeraCharacter candidate = ChimeraCharacter.Cast(trip.VehiclePresetMember(member));
					if (!candidate) continue;
					float distance = vector.DistanceXZ(candidate.GetOrigin(), plan.GetStagingPosition());
					if (distance > farthest) { farthest = distance; actor = candidate; }
				}
				if (!actor) return;
				vector direction = actor.GetOrigin() - plan.GetStagingPosition(); direction[1] = 0; direction.Normalize();
				vector query = plan.GetStagingPosition() + direction * 26.5;
				vector destination;
				if (AICF_InfantrySpawnPlacement.IsUsable(trip.GetAssignment().GetGroup(), query, destination) &&
					vector.Distance(actor.GetOrigin(), destination) <= 8 && vector.DistanceSqXZ(query, destination) <= 0.25)
				{
					float shift = vector.Distance(actor.GetOrigin(), destination);
					AICF_StagingTaskProbeState.s_Actor = actor;
					AICF_StagingTaskProbeState.s_OldDisabled = actor.GetCharacterController().GetDisableMovementControls();
					actor.GetCharacterController().SetDisableMovementControls(true);
					vector transform[4]; actor.GetWorldTransform(transform); transform[3] = destination; actor.Teleport(transform);
					AICF_StagingTaskProbeState.s_Setup = true;
					AICF_StagingTaskProbeState.Check("SHORT_SETUP_ONLY", shift <= 8 && !trip.GetLease(), string.Format("shift_m=%1", shift));
				}
			}
			if (AICF_StagingTaskProbeState.s_Setup)
			{
				SCR_AIGroup group = trip.GetAssignment().GetGroup();
				AIWaypoint waypoint = plan.GetApproachWaypoint();
				array<AIWaypoint> queue = {}; group.GetWaypoints(queue);
				if (waypoint && queue.Contains(waypoint))
				{
					group.RemoveWaypoint(waypoint);
					queue.Clear(); group.GetWaypoints(queue);
					if (!queue.Contains(waypoint)) AICF_StagingTaskProbeState.s_Removed++;
				}
			}
		}
		super.MaintainStagingMemberActions(trip, currentAssignment, requestState, plan, nowMs);
	}
}

modded class AICF_MatchController
{
	protected bool m_bSTFinished;
	protected int m_iSTStarted;
	protected int m_iSTProved;
	protected int m_iSTApproachDeadline;
	protected int m_iSTTripDeadline;
	protected int m_iSTAssignment;
	protected int m_iSTIntent;
	protected vector m_vSTReleasedOrigin;
	protected float m_fSTHealth;
	override protected void Update()
	{
		super.Update();
		if (!AICF_StagingTaskProbeState.Enabled() || !Replication.IsServer() || !m_bRosterReady || m_bSTFinished) return;
		int now = System.GetTickCount();
		if (!m_iSTStarted)
		{
			m_iSTStarted = now; m_sVBScenario = "transport";
			if (!AICF_VBInitialize()) { AICF_StagingTaskProbeState.Check("SETUP", false); AICF_STFinish(); }
			return;
		}
		if (now - m_iSTStarted > 180000 || AICF_StagingTaskProbeState.s_Failures || m_iVBFailures)
		{
			AICF_StagingTaskProbeState.Check("BOUNDED_FINISH", false); AICF_STFinish(); return;
		}
		AICF_GroupSlot slot = m_VBState.GetSlot(0);
		if (!slot.IsCombatReady())
		{
			if (m_VBGroup.GetAgentsCount() != 5) return;
			if (!AICF_VBPrepare(slot)) { AICF_StagingTaskProbeState.Check("PRESET", false); AICF_STFinish(); return; }
			AICF_StagingTaskProbeState.s_Group = m_VBGroup;
		}
		AICF_VBThreat(false); AICF_VBObserve();
		if (m_sVBFaction == "US") m_VBCoordinator.Update(m_VBState, m_VBFaction, null, null, m_iStrategicBaseRevision, true);
		else m_VBCoordinator.Update(null, null, m_VBState, m_VBFaction, m_iStrategicBaseRevision, true);
		AICF_TransportTrip trip = m_VBCoordinator.GetTrip(slot);
		if (!trip) return;
		if (trip.IsTerminal()) { AICF_StagingTaskProbeState.Check("NO_PREMATURE_FALLBACK", false, trip.GetTerminalReason()); AICF_STFinish(); return; }
		AICF_VehicleSpawnPlan plan = trip.GetRequestState().GetSpawnPlan();
		if (!plan || !AICF_StagingTaskProbeState.s_Actor) return;
		if (!m_iSTProved)
		{
			if (!AICF_StagingTaskProbeState.s_Removed || !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now)) return;
			array<AIWaypoint> queue = {}; m_VBGroup.GetWaypoints(queue);
			if (m_VBGroup.GetCurrentWaypoint() == plan.GetApproachWaypoint() || queue.Contains(plan.GetApproachWaypoint())) return;
			m_iSTProved = now; m_iSTApproachDeadline = plan.GetApproachDeadlineMs(); m_iSTTripDeadline = trip.GetAbsoluteDeadlineMs();
			m_iSTAssignment = slot.GetStrategicAssignmentRevision(); m_iSTIntent = slot.GetStrategicIntentRevision();
			m_vSTReleasedOrigin = AICF_StagingTaskProbeState.s_Actor.GetOrigin(); m_fSTHealth = AICF_StagingTaskProbeState.s_Actor.GetDamageManager().GetHealthScaled();
			AICF_VehicleSlotView view = m_VBCoordinator.GetSlotView(slot);
			AICF_StagingTaskProbeState.Check("ACTUAL_RUNNING_ACTOR_WITHOUT_GROUP_WAYPOINT", view.HasStagingActorTask() && HasMeaningfulTask(slot, view),
				string.Format("removed=%1 waypoint_current=0 waypoint_queued=0 actor_tokens=%2", AICF_StagingTaskProbeState.s_Removed, plan.GetStagingActionTokenCount()));
			AICF_STNegatives(trip, plan, now);
			AICF_StagingTaskProbeState.Restore();
			return;
		}
		if (now - m_iSTProved >= 30000) { AICF_StagingTaskProbeState.Check("COMPLETION_WITHIN_ORIGINAL_TASK_DEADLINE", false); AICF_STFinish(); return; }
		if (trip.GetPhase() == AICF_ETransportTripPhase.APPROACHING_SITE) return;
		bool exact = slot.GetGroup() == m_VBGroup && slot.GetSpawnGeneration() == trip.GetGroupGeneration();
		for (int member; member < 5; member++) exact = exact && slot.GetRosterMember(member) == m_aVBMembers[member] && AICF_GroupRuntime.IsAliveCharacter(m_aVBMembers[member]);
		AICF_StagingTaskProbeState.Check("ACTUAL_STAGING_COMPLETION", plan.GetStagedCount() == 5 && plan.GetAliveCount() == 5 && exact &&
			vector.Distance(AICF_StagingTaskProbeState.s_Actor.GetOrigin(), m_vSTReleasedOrigin) >= 2,
			string.Format("elapsed_ms=%1 moved_m=%2 phase=%3", now-m_iSTProved, vector.Distance(AICF_StagingTaskProbeState.s_Actor.GetOrigin(), m_vSTReleasedOrigin), trip.GetPhase()));
		AICF_StagingTaskProbeState.Check("DEADLINES_REVISIONS_HEALTH_UNCHANGED", plan.GetApproachDeadlineMs() == m_iSTApproachDeadline && trip.GetAbsoluteDeadlineMs() == m_iSTTripDeadline &&
			slot.GetStrategicAssignmentRevision() == m_iSTAssignment && slot.GetStrategicIntentRevision() == m_iSTIntent &&
			AICF_StagingTaskProbeState.s_Actor.GetDamageManager().GetHealthScaled() == m_fSTHealth);
		AICF_STFinish();
	}
	protected void AICF_STNegatives(AICF_TransportTrip trip, AICF_VehicleSpawnPlan plan, int now)
	{
		AICF_StagingTaskProbeState.Check("EXPIRED_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, plan.GetApproachDeadlineMs()));
		AICF_StagingTaskProbeState.Check("STALE_GRAPH_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision+1, now));
		array<ref SCR_AIMoveIndividuallyBehavior> actions = {};
		array<ref AICF_StrategicAssignmentSnapshot> assignments = {};
		for (int index; index < plan.GetStagingActionTokenCount(); index++) { AICF_VehicleStagingActionToken token = plan.GetStagingActionToken(index); token.AICF_STRequestDelta(1); }
		AICF_StagingTaskProbeState.Check("STALE_REQUEST_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now));
		for (int restore; restore < plan.GetStagingActionTokenCount(); restore++) plan.GetStagingActionToken(restore).AICF_STRequestDelta(-1);
		for (int goal; goal < plan.GetStagingActionTokenCount(); goal++) plan.GetStagingActionToken(goal).AICF_STGoalDelta("1 0 0");
		AICF_StagingTaskProbeState.Check("FOREIGN_GOAL_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now));
		for (int goalRestore; goalRestore < plan.GetStagingActionTokenCount(); goalRestore++) plan.GetStagingActionToken(goalRestore).AICF_STGoalDelta("-1 0 0");
		for (int capture; capture < plan.GetStagingActionTokenCount(); capture++) assignments.Insert(plan.GetStagingActionToken(capture).AICF_STSwapAssignment(null));
		AICF_StagingTaskProbeState.Check("STALE_ASSIGNMENT_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now));
		for (int assignmentRestore; assignmentRestore < plan.GetStagingActionTokenCount(); assignmentRestore++) plan.GetStagingActionToken(assignmentRestore).AICF_STSwapAssignment(assignments[assignmentRestore]);
		for (int remove; remove < plan.GetStagingActionTokenCount(); remove++) actions.Insert(plan.GetStagingActionToken(remove).AICF_STSwapAction(null));
		AICF_StagingTaskProbeState.Check("NO_ACTIVITY_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now));
		for (int actionRestore; actionRestore < plan.GetStagingActionTokenCount(); actionRestore++) plan.GetStagingActionToken(actionRestore).AICF_STSwapAction(actions[actionRestore]);
		for (int foreign; foreign < plan.GetStagingActionTokenCount(); foreign++)
		{
			AICF_VehicleStagingActionToken source = plan.GetStagingActionToken(foreign);
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(source.GetAgent().FindComponent(SCR_AIUtilityComponent));
			SCR_AIMoveIndividuallyBehavior unowned = new SCR_AIMoveIndividuallyBehavior(utility, null, source.GetTargetPosition());
			source.AICF_STSwapAction(unowned);
		}
		AICF_StagingTaskProbeState.Check("UNOWNED_NATIVE_ACTIVITY_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now));
		for (int foreignRestore; foreignRestore < plan.GetStagingActionTokenCount(); foreignRestore++) plan.GetStagingActionToken(foreignRestore).AICF_STSwapAction(actions[foreignRestore]);
		foreach (SCR_AIMoveIndividuallyBehavior suspend : actions) if (suspend) suspend.SetActionIsSuspended(true);
		AICF_StagingTaskProbeState.Check("SUSPENDED_NATIVE_ACTIVITY_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now));
		foreach (SCR_AIMoveIndividuallyBehavior resume : actions) if (resume) resume.SetActionIsSuspended(false);
		foreach (SCR_AIMoveIndividuallyBehavior cancel : actions) if (cancel) cancel.SetRemoveAction(true);
		AICF_StagingTaskProbeState.Check("CANCELLED_NATIVE_ACTIVITY_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now));
		foreach (SCR_AIMoveIndividuallyBehavior cancelRestore : actions) if (cancelRestore) cancelRestore.SetRemoveAction(false);
		AICF_StagingTaskProbeState.Check("RESTORED_ORIGINAL_RUNNING_PROOF", AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now));
		foreach (SCR_AIMoveIndividuallyBehavior action : actions) if (action) action.Fail();
		AICF_StagingTaskProbeState.Check("FAILED_ACTIVITY_FALSE", !AICF_VehicleStagingTaskProof.HasCurrentTask(trip, m_iStrategicBaseRevision, now));
	}
	protected void AICF_STFinish()
	{
		if (m_bSTFinished) return;
		m_bSTFinished = true; AICF_StagingTaskProbeState.Restore();
		Print(string.Format("[AICF][STAGING_TASK_FINISHED] checks=%1 failures=%2 classification=CONTROLLED_ACTOR_TASK_ONLY", AICF_StagingTaskProbeState.s_Checks, AICF_StagingTaskProbeState.s_Failures));
		AICF_VBFinish();
	}
}
