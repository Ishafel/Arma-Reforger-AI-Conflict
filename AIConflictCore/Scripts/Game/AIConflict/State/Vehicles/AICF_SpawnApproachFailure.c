// Identity-bound receipt for one native UNKNOWN during a phase-owned approach.
// Acquisition records it; only its ordinary Tick returns the terminal outcome.
class AICF_SpawnApproachFailure
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_GroupSlot m_Slot;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_VehicleRequestState m_Request;
	protected AICF_VehicleSpawnPlan m_Plan;
	protected AICF_VehicleSpawner m_Spawner;
	protected SCR_AIGroup m_Group;
	protected SCR_AIGroupUtilityComponent m_Utility;
	protected AIActionBase m_Action;
	protected SCR_AIActivityBase m_Activity;
	protected AIWaypoint m_Waypoint;
	protected EntityID m_GroupId;
	protected string m_sGroupRplId;
	protected int m_iRequestGeneration;
	protected int m_iTripGeneration;
	protected bool m_bDelivered;

	void AICF_SpawnApproachFailure(AICF_TransportTrip trip, AICF_GroupSlot slot,
		AICF_VehicleSpawnPlan plan, AICF_VehicleSpawner spawner,
		SCR_AIGroupUtilityComponent utility, AIActionBase action, AIWaypoint waypoint)
	{
		m_Trip = trip;
		m_Slot = slot;
		m_Assignment = trip.GetAssignment();
		m_Request = trip.GetRequestState();
		m_Plan = plan;
		m_Spawner = spawner;
		m_Group = m_Assignment.GetGroup();
		m_GroupId = m_Group.GetID();
		RplComponent rpl = RplComponent.Cast(m_Group.FindComponent(RplComponent));
		if (rpl) m_sGroupRplId = rpl.Id().ToString();
		m_iRequestGeneration = m_Request.GetRequestGeneration();
		m_iTripGeneration = trip.GetTripGeneration();
		m_Utility = utility;
		m_Action = action;
		m_Activity = SCR_AIActivityBase.Cast(utility.GetCurrentAction());
		m_Waypoint = waypoint;
	}

	AIActionBase GetAction() { return m_Action; }
	AIWaypoint GetWaypoint() { return m_Waypoint; }
	bool WasDelivered() { return m_bDelivered; }
	bool MatchesTrip(AICF_TransportTrip trip) { return trip == m_Trip && IsPlanCurrent(); }

	bool IsPlanCurrent()
	{
		if (!Replication.IsServer() || !m_Trip || !m_Slot || !m_Assignment || !m_Request)
			return false;
		if (!m_Plan || !m_Spawner || !m_Group || !m_Waypoint || !m_Trip.IsValid())
			return false;
		if (m_Trip.IsTerminal() || m_Trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE ||
			m_Trip.GetLease() || m_Trip.GetAssignment() != m_Assignment)
			return false;
		if (m_Trip.GetRequestState() != m_Request || m_Request.GetSpawnPlan() != m_Plan ||
			m_Trip.GetTripGeneration() != m_iTripGeneration || m_Request.GetRequestGeneration() != m_iRequestGeneration)
			return false;
		if (m_Plan.GetApproachWaypoint() != m_Waypoint || m_Plan.GetApproachFailure() != this ||
			!m_Plan.IsValid() || m_Group.GetGroupUtilityComponent() != m_Utility)
			return false;
		if (!m_Slot.IsCombatReady() || m_Slot.GetGroup() != m_Group || m_Group.GetID() != m_GroupId)
			return false;
		if (m_Slot.GetSpawnGeneration() != m_Assignment.GetGroupGeneration() ||
			m_Slot.GetSlotId() != m_Assignment.GetSlotId())
			return false;
		if (m_Slot.GetStrategicAssignmentRevision() != m_Assignment.GetAssignmentRevision() ||
			m_Slot.GetStrategicIntentRevision() != m_Assignment.GetStrategicIntentRevision() ||
			(m_Trip.GetVehiclePresetSlot() && !m_Trip.IsVehiclePresetCurrent()))
			return false;
		int nowMs = System.GetTickCount();
		if (m_Trip.IsDeadlineReached(nowMs) || m_Request.IsDeadlineReached(nowMs) ||
			nowMs >= m_Plan.GetApproachDeadlineMs() ||
			!m_Spawner.IsSiteReservationCurrent(m_Plan.GetReservation(), m_Trip, m_Request, nowMs))
			return false;
		RplComponent rpl = RplComponent.Cast(m_Group.FindComponent(RplComponent));
		if (!rpl || !rpl.IsMaster() || rpl.IsProxy() || rpl.Id().ToString() != m_sGroupRplId)
			return false;
		return m_Group.GetFaction() && m_Group.GetFaction().GetFactionKey() == m_Assignment.GetFactionKey();
	}

	bool IsSourceCurrent()
	{
		if (!IsPlanCurrent() || !m_Utility || !m_Action || !m_Activity || m_Action != m_Activity)
			return false;
		if (m_Group.GetGroupUtilityComponent() != m_Utility ||
			m_Utility.GetExecutedAction() != m_Action || m_Utility.GetCurrentAction() != m_Activity)
			return false;
		if (m_Activity.m_RelatedWaypoint != m_Waypoint || m_Group.GetCurrentWaypoint() != m_Waypoint ||
			m_Action.GetActionState() == EAIActionState.COMPLETED || m_Action.GetActionState() == EAIActionState.FAILED)
			return false;
		array<AIWaypoint> queue = {};
		m_Group.GetWaypoints(queue);
		return queue.Contains(m_Waypoint) && IsFootRosterCurrent();
	}

	protected bool IsFootRosterCurrent()
	{
		array<AIAgent> agents = {};
		m_Group.GetAgents(agents);
		int alive;
		foreach (AIAgent agent : agents)
		{
			IEntity member;
			if (agent) member = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(member)) continue;
			if (agent.GetParentGroup() != m_Group ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) return false;
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(member.FindComponent(CompartmentAccessComponent));
			if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim() ||
				CompartmentAccessComponent.GetVehicleIn(member)) return false;
			alive++;
		}
		return alive > 0;
	}

	bool ConfirmDelivery()
	{
		if (m_bDelivered || !IsSourceCurrent()) return false;
		m_bDelivered = true;
		return true;
	}
}
