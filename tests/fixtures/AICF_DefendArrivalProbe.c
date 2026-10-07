// Isolated stage, -aicfDefendArrivalProbe 1 -aicfAICommanderMode USSR.
// Инъекция late move result при действующем US system hold; затем native BT работает 60 секунд.
class AICF_DefendArrivalProbeNode : SCR_AIProcessFailedMovementResult
{
	bool Inject(SCR_AIGroup group, int result, int handler = AIGroupMovementComponent.DEFAULT_HANDLER_ID, bool related = true)
	{
		OnInit(group);
		return AICF_HandleDefendArrival(result, handler, related, group.GetLeaderEntity().GetOrigin());
	}

	bool InjectApproachFailure(SCR_AIGroup group)
	{
		OnInit(group);
		return AICF_HandleFailedMovement(EMoveError.STUCK, AIGroupMovementComponent.DEFAULT_HANDLER_ID, true, group.GetLeaderEntity().GetOrigin());
	}
}

modded class AICF_MatchController
{
	protected int m_iAICFDefendProbeAt;
	protected int m_iAICFDefendProbeFailures;
	protected int m_iAICFDefendProbeAssignment;
	protected AIWaypoint m_AICFDefendProbeWaypoint;
	protected SCR_AIGroupUtilityComponent m_AICFDefendProbeUtility;
	protected ref SCR_AIDefendActivity m_AICFDefendReplacement;
	protected bool m_bAICFDefendProbeDone;

	protected void AICF_DefendCheck(string name, bool passed)
	{
		if (!passed) m_iAICFDefendProbeFailures++;
		Print(string.Format("[AICF][DEFEND_HANDOFF_PROBE] case=%1 passed=%2", name, passed));
	}

	protected void AICF_DefendReplaceAction(int result, IEntity vehicle, bool related, vector location)
	{
		m_AICFDefendProbeUtility.SetCurrentAction(m_AICFDefendReplacement);
		m_AICFDefendProbeUtility.SetExecutedAction(m_AICFDefendReplacement);
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfDefendArrivalProbe", enabled) || enabled != "1" ||
			!m_bRosterReady || m_bStopped || m_bAICFDefendProbeDone)
			return;
		AICF_GroupSlot slot = m_USState.GetSlot(9);
		SCR_AIGroup group = slot.GetGroup();
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		AIWaypoint waypoint = slot.GetWaypoint();
		if (!m_iAICFDefendProbeAt)
		{
			if (!SCR_DefendWaypoint.Cast(waypoint) || !AICF_DefendArrivalHandoff.IsFormationAtWaypoint(group, waypoint))
				return;
			m_iAICFDefendProbeAt = System.GetTickCount();
			m_AICFDefendProbeWaypoint = waypoint;
			m_iAICFDefendProbeAssignment = slot.GetStrategicAssignmentRevision();
			m_AICFDefendProbeUtility = utility;
			SCR_AIMoveActivity move = new SCR_AIMoveActivity(utility, waypoint, waypoint.GetOrigin(), null);
			utility.AddAction(move);
			utility.SetCurrentAction(move);
			utility.SetExecutedAction(move);
			AICF_DefendArrivalProbeNode node = new AICF_DefendArrivalProbeNode();
			AICF_DefendCheck("WAITING_UNCHANGED", !node.Inject(group, EMoveError.WAITING_ON_NAVLINK));
			AICF_DefendCheck("UNKNOWN_UNCHANGED", !node.Inject(group, EMoveError.UNKNOWN));
			AICF_DefendCheck("UNRELATED_UNCHANGED", !node.Inject(group, EMoveError.STUCK, AIGroupMovementComponent.DEFAULT_HANDLER_ID, false));
			AICF_DefendCheck("VEHICLE_UNCHANGED", !node.Inject(group, EMoveError.STUCK, -2));
			move.m_RelatedWaypoint = m_USState.GetSlot(8).GetWaypoint();
			AICF_DefendCheck("FOREIGN_WAYPOINT", !node.Inject(group, EMoveError.STUCK));
			move.m_RelatedWaypoint = waypoint;
			vector origin = waypoint.GetOrigin();
			waypoint.SetOrigin(origin + Vector(500, 0, 500));
			AICF_DefendCheck("DISTANT_FORMATION", !node.Inject(group, EMoveError.STUCK));
			waypoint.SetOrigin(origin);
			m_AICFDefendReplacement = new SCR_AIDefendActivity(utility, waypoint, vector.Zero);
			utility.AddAction(m_AICFDefendReplacement);
			utility.GetOnMoveFailed().Insert(AICF_DefendReplaceAction);
			bool handled = node.Inject(group, EMoveError.STOPPED);
			utility.GetOnMoveFailed().Remove(AICF_DefendReplaceAction);
			AICF_DefendCheck("SYNCHRONOUS_REPLACEMENT", handled && utility.GetExecutedAction() == m_AICFDefendReplacement &&
				move.GetActionState() != EAIActionState.FAILED && m_AICFDefendReplacement.GetActionState() != EAIActionState.FAILED);
			utility.SetCurrentAction(move);
			utility.SetExecutedAction(move);
			AICF_DefendCheck("ARRIVED_MOVE_HANDOFF", node.Inject(group, EMoveError.STUCK));
			AICF_DefendCheck("WAYPOINT_RETAINED", slot.GetWaypoint() == waypoint && group.GetCurrentWaypoint() == waypoint &&
				slot.GetStrategicAssignmentRevision() == m_iAICFDefendProbeAssignment && !slot.HasFailedMovement());
			AICF_DefendCheck("OLD_MOVE_FAILED", move.GetActionState() == EAIActionState.FAILED);
			utility.CancelActivitiesRelatedToWaypoint(waypoint, SCR_AIDefendActivity, true);
			SCR_AIMoveActivity secondMove = new SCR_AIMoveActivity(utility, waypoint, waypoint.GetOrigin(), null);
			utility.AddAction(secondMove);
			utility.SetCurrentAction(secondMove);
			utility.SetExecutedAction(secondMove);
			AICF_DefendCheck("NEW_ACTIVITY_SUBMITTED", node.Inject(group, EMoveError.STOPPED));
			AICF_TestDefendApproachFailure(node);
			return;
		}
		if (System.GetTickCount(m_iAICFDefendProbeAt) < 60000)
			return;
		m_bAICFDefendProbeDone = true;
		SCR_AIDefendActivity active = SCR_AIDefendActivity.Cast(utility.GetExecutedAction());
		AICF_DefendCheck("NATIVE_HOLD_60S", active && active.m_RelatedWaypoint == m_AICFDefendProbeWaypoint &&
			active.GetActionState() == EAIActionState.RUNNING && group.GetCurrentWaypoint() == m_AICFDefendProbeWaypoint);
		AICF_DefendCheck("NO_ASSIGNMENT_CHURN", slot.GetStrategicAssignmentRevision() == m_iAICFDefendProbeAssignment);
		Print(string.Format("[AICF][DEFEND_HANDOFF_PROBE_DONE] failures=%1", m_iAICFDefendProbeFailures));
		GetGame().RequestClose();
	}

	protected void AICF_TestDefendApproachFailure(AICF_DefendArrivalProbeNode node)
	{
		AICF_GroupSlot slot = m_USState.GetSlot(8);
		SCR_AIGroup group = slot.GetGroup();
		vector destination = AICF_GroupRuntime.ResolveAliveLeader(group).GetOrigin() + Vector(500, 0, 500);
		bool issued = m_OrderPlanner.AssignPlayerPointOrder(slot, m_USFaction, destination);
		AICF_DefendCheck("APPROACH_ORDER_CREATED", issued);
		if (!issued) return;
		AIWaypoint waypoint = slot.GetWaypoint();
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		SCR_AIMoveActivity move = new SCR_AIMoveActivity(utility, waypoint, waypoint.GetOrigin(), null);
		utility.AddAction(move);
		utility.SetCurrentAction(move);
		utility.SetExecutedAction(move);
		AICF_DefendCheck("APPROACH_FAILURE_OWNED", node.InjectApproachFailure(group) && slot.HasFailedMovement());
		AICF_DefendCheck("APPROACH_NOT_COMPLETED", group.GetCurrentWaypoint() == waypoint && slot.GetWaypoint() == waypoint);
	}
}
