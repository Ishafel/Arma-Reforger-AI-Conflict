// Controlled UNKNOWN through the production node boundary; no native failure
// is manufactured as natural pathfinding evidence or normal arrival success.
class AICF_SpawnApproachFailureProbeNode : SCR_AIProcessFailedMovementResult
{
	bool Inject(SCR_AIGroup group, int handler, vector location)
	{
		m_Group = group;
		m_GroupUtilityComponent = group.GetGroupUtilityComponent();
		return AICF_HandleSpawnApproachFailedMovement(EMoveError.UNKNOWN, handler, true, location);
	}
}

modded class AICF_MatchController
{
	protected ref AICF_TransportTrip m_SAFTrip;
	protected ref AICF_SpawnApproachFailureProbeNode m_SAFNode;
	protected EntityID m_SAFWaypointId;
	protected int m_iSAFInjectedAt;
	protected int m_iSAFCallbacks;

	protected void AICF_SAFLog(string eventName, string details, int nowMs)
	{
		string prefix = string.Format("[AICF][SPAWN_APPROACH_FAILURE_PROBE] event=%1 t_ms=%2 classification=CONTROLLED_FAULT_INJECTION normal_arrival_proof=0 ", eventName, nowMs);
		Print(prefix + details);
	}

	protected void AICF_SAFOnMoveFailed(int result, IEntity vehicle, bool related, vector location)
	{
		m_iSAFCallbacks++;
	}

	override protected bool AICF_VSAlternativeScenario(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		string enabled;
		if (!System.GetCLIParam("aicfSpawnApproachFailureProbe", enabled) || enabled != "1")
			return super.AICF_VSAlternativeScenario(slot, trip);
		int now = System.GetTickCount();
		if (!m_SAFTrip)
		{
			if (trip.IsTerminal())
			{
				AICF_VSCheck("APPROACH_INJECTION_NOT_REACHED", false); AICF_VSFinish(); return true;
			}
			if (trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE || trip.GetLease() || !AICF_VSOwnRecoveryRoster(slot)) return true;
			AICF_VehicleSpawnPlan plan = trip.GetRequestState().GetSpawnPlan();
			if (!plan || !plan.GetApproachWaypoint() || m_VSGroup.GetCurrentWaypoint() != plan.GetApproachWaypoint()) return true;
			SCR_AIGroupUtilityComponent utility = m_VSGroup.GetGroupUtilityComponent();
			SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(utility.GetCurrentAction());
			if (!activity || activity.m_RelatedWaypoint != plan.GetApproachWaypoint() || utility.GetExecutedAction() != activity) return true;
			ChimeraCharacter leader = ChimeraCharacter.Cast(m_VSGroup.GetLeaderEntity());
			if (!leader || !leader.GetAIControlComponent()) return true;
			AIAgent agent = leader.GetAIControlComponent().GetControlAIAgent();
			AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(m_VSGroup.FindComponent(AIGroupMovementComponent));
			if (!agent || !movement) return true;
			int handler = movement.GetAgentMoveHandlerId(agent);
			m_SAFTrip = trip;
			m_SAFWaypointId = plan.GetApproachWaypoint().GetID();
			m_iSAFInjectedAt = now;
			utility.GetOnMoveFailed().Insert(AICF_SAFOnMoveFailed);
			m_SAFNode = new AICF_SpawnApproachFailureProbeNode();
			bool accepted = m_SAFNode.Inject(m_VSGroup, handler, plan.GetApproachWaypoint().GetOrigin());
			utility.GetOnMoveFailed().Remove(AICF_SAFOnMoveFailed);
			string injectedDetails = string.Format("operation_id=%1 group=%2 waypoint_id=%3 handler=%4 accepted=%5 callbacks=%6 phase=%7 plan_pending=%8 no_lease=1",
				trip.GetOperationId(), AICF_GroupTrace.Entity(m_VSGroup), m_SAFWaypointId, handler, accepted, m_iSAFCallbacks,
				typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase()), plan.GetApproachFailure() != null);
			AICF_SAFLog("INJECTED", injectedDetails, now);
			AICF_VSCheck("EXACT_APPROACH_REPORT_ACCEPTED", accepted && m_iSAFCallbacks == 1 && !trip.IsTerminal() && plan.GetApproachFailure() && plan.GetApproachFailure().WasDelivered());
			if (!accepted) AICF_VSFinish();
			return true;
		}
		if (now - m_iSAFInjectedAt > 120000)
		{
			AICF_VSCheck("APPROACH_FALLBACK_TIMEOUT", false); AICF_VSFinish(); return true;
		}
		if (trip != m_SAFTrip || !AICF_VSOwnRecoveryRoster(slot))
		{
			AICF_VSCheck("APPROACH_ORIGINAL_IDENTITY", false); AICF_VSFinish(); return true;
		}
		if (!trip.IsTerminal() || !trip.GetHandoffState().IsOrderRestored() || slot.HasPendingOrderRecovery()) return true;
		bool fallback = trip.GetPhase() == AICF_ETransportTripPhase.FALLBACK && trip.GetTerminalReason() == "SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN";
		bool restored = slot.GetWaypoint() && slot.GetWaypoint() == m_VSGroup.GetCurrentWaypoint() &&
			m_OrderPlanner.GetOrderFailureReason(slot, m_USSRFaction).IsEmpty();
		AICF_VSCheck("APPROACH_NEXT_TICK_FALLBACK", fallback && now > m_iSAFInjectedAt);
		AICF_VSCheck("APPROACH_PLAN_RELEASED", !trip.GetRequestState().GetSpawnPlan() && !trip.GetLease());
		AICF_VSCheck("APPROACH_ORDER_RESTORED", restored);
		AICF_VSCheck("APPROACH_CALLBACK_ONCE", m_iSAFCallbacks == 1);
		string doneDetails = string.Format("operation_id=%1 elapsed_ms=%2 phase=%3 reason=%4 plan_released=%5 order_restored=%6 callbacks=%7 failures=%8 original_roster=1",
			trip.GetOperationId(), now - m_iSAFInjectedAt, typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase()),
			trip.GetTerminalReason(), !trip.GetRequestState().GetSpawnPlan(), restored, m_iSAFCallbacks, m_iVSFailures);
		AICF_SAFLog("DONE", doneDetails, now);
		AICF_VSFinish();
		return true;
	}
}
