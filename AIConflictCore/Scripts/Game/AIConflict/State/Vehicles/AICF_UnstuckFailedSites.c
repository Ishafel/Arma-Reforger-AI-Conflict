// Отрицательная память максимум двух действительно безрезультатных origins.
// Не является разрешением mutation и не меняет clocks, budget или progress.
class AICF_UnstuckFailedSites
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_VehicleLease m_Lease;
	protected AICF_VehicleMovementState m_State;
	protected Vehicle m_Vehicle;
	protected SCR_AIGroup m_Group;
	protected SCR_CampaignMilitaryBaseComponent m_Base;
	protected EntityID m_GroupId;
	protected EntityID m_VehicleId;
	protected string m_sGroupRpl;
	protected string m_sVehicleRpl;
	protected string m_sEpoch;
	protected vector m_vGoal;
	protected AICF_EOrderTargetKind m_TargetKind;
	protected int m_iTransitions;
	protected int m_iRouteGeneration;
	protected AIWaypoint m_Waypoint;
	protected ref array<vector> m_aSites = {};
	protected bool m_bPrepared;
	protected vector m_vPreparedOrigin;
	protected int m_iPreparedAt;
	protected int m_iPreparedMotionAt;
	protected ref AICF_UnstuckMutationScope m_PreparedScope;
	protected bool m_bOwnedReplacement;
	protected AIWaypoint m_ExpectedWaypoint;
	protected EntityID m_ExpectedWaypointId;
	protected int m_iExpectedGeneration;

	protected string Epoch(AICF_TransportTrip trip)
	{
		return string.Format("%1:%2:%3:%4:%5:%6:%7:%8", trip.GetFactionKey(), trip.GetSlotId(),
			trip.GetGroupGeneration(), trip.GetTripGeneration(), trip.GetLease().GetLeaseGeneration(),
			trip.GetLease().GetVehicleGeneration(), trip.GetAssignment().GetAssignmentRevision(), trip.GetAssignment().GetStrategicIntentRevision());
	}
	protected bool Context(AICF_TransportTrip trip, Vehicle vehicle)
	{
		if (!trip || !vehicle || trip != m_Trip || !m_Lease || !m_State || !m_Group || !m_Vehicle) return false;
		if (!AICF_UnstuckMutationScope.Capture(trip, vehicle)) return false;
		if (!trip || !vehicle || !m_Lease || !m_State || !m_Group || !m_Vehicle || !trip.GetAssignment()) return false;
		if (trip.GetLease() != m_Lease || vehicle != m_Vehicle || trip.GetMovementState() != m_State) return false;
		if (m_State.GetUnstuckFailedSites() != this || trip.GetTransitionCount() != m_iTransitions) return false;
		if (trip.GetAssignment().GetGroup() != m_Group || Epoch(trip) != m_sEpoch) return false;
		if (m_Group.GetID() != m_GroupId || vehicle.GetID() != m_VehicleId) return false;
		if (AICF_VehicleBehaviorBinding.ComponentRpl(m_Group) != m_sGroupRpl || AICF_VehicleBehaviorBinding.ComponentRpl(vehicle) != m_sVehicleRpl) return false;
		return trip.GetAssignment().GetTargetPosition() == m_vGoal && trip.GetAssignment().GetTargetKind() == m_TargetKind && trip.GetAssignment().GetTargetBase() == m_Base;
	}
	protected bool RouteCurrent(AICF_TransportTrip trip, Vehicle vehicle)
	{
		return Context(trip, vehicle) && m_Waypoint && m_State.GetRouteWaypoint() == m_Waypoint &&
			m_State.GetRouteGeneration() == m_iRouteGeneration && m_State.IsRouteWaypointBound();
	}
	void Clear()
	{
		m_aSites.Clear();
		m_Trip = null;
		m_Lease = null;
		m_State = null;
		m_Vehicle = null;
		m_Group = null;
		m_Base = null;
		m_Waypoint = null;
		m_bPrepared = false;
		m_PreparedScope = null;
		m_bOwnedReplacement = false;
		m_ExpectedWaypoint = null;
	}
	bool PrepareFailure(AICF_TransportTrip trip, Vehicle vehicle, string reason, int timeoutMs)
	{
		m_bPrepared = false;
		m_PreparedScope = null;
		if (m_Trip && !RouteCurrent(trip, vehicle)) Clear();
		if (reason != "NO_PHYSICAL_MOVEMENT" || !trip || !vehicle || timeoutMs <= 0) return false;
		AICF_VehicleMovementState state = trip.GetMovementState();
		int now = System.GetTickCount();
		if (!state || now < state.GetLastPhysicalMotionAtMs() || now - state.GetLastPhysicalMotionAtMs() < timeoutMs) return false;
		if (state.GetMobilityRecoveryAttempts() < 1 || state.GetMobilityRecoveryAttempts() > 2 || state.GetMaximumMobilityRecoveryAttempts() != 2) return false;
		AICF_UnstuckMutationScope scope = AICF_UnstuckMutationScope.Capture(trip, vehicle);
		if (!scope || !state.GetRouteWaypoint() || !state.IsRouteWaypointBound()) return false;
		if (!m_Trip)
		{
			m_Trip = trip;
			m_Lease = trip.GetLease();
			m_State = state;
			m_Vehicle = vehicle;
			m_Group = trip.GetAssignment().GetGroup();
			m_GroupId = m_Group.GetID();
			m_VehicleId = vehicle.GetID();
			m_sGroupRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_Group);
			m_sVehicleRpl = AICF_VehicleBehaviorBinding.ComponentRpl(vehicle);
			m_sEpoch = Epoch(trip);
			m_iTransitions = trip.GetTransitionCount();
			m_vGoal = trip.GetAssignment().GetTargetPosition();
			m_TargetKind = trip.GetAssignment().GetTargetKind();
			m_Base = trip.GetAssignment().GetTargetBase();
			m_iRouteGeneration = state.GetRouteGeneration();
			m_Waypoint = state.GetRouteWaypoint();
		}
		if (!RouteCurrent(trip, vehicle) || !scope.IsCurrent()) return false;
		m_vPreparedOrigin = vehicle.GetOrigin();
		m_iPreparedAt = now;
		m_iPreparedMotionAt = state.GetLastPhysicalMotionAtMs();
		m_PreparedScope = scope;
		m_bPrepared = true;
		return true;
	}
	bool PreparedCurrent(AICF_TransportTrip trip, Vehicle vehicle)
	{
		return m_bPrepared && m_PreparedScope && m_PreparedScope.IsCurrent() && RouteCurrent(trip, vehicle) &&
			vehicle.GetOrigin() == m_vPreparedOrigin && m_State.GetLastPhysicalMotionAtMs() == m_iPreparedMotionAt;
	}
	static bool NearFailedSite(vector candidate, vector failed, float radius)
	{
		return vector.Distance(candidate, failed) <= radius;
	}
	bool Rejects(AICF_TransportTrip trip, Vehicle vehicle, vector candidate, float radius)
	{
		if (!RouteCurrent(trip, vehicle)) return false;
		foreach (vector site : m_aSites)
			if (NearFailedSite(candidate, site, radius)) return true;
		return false;
	}
	bool CommitSuccessfulRelocation(AICF_TransportTrip trip, Vehicle vehicle, bool transformed)
	{
		int now = System.GetTickCount();
		bool current = transformed && m_bPrepared && RouteCurrent(trip, vehicle) && now >= m_iPreparedAt && now - m_iPreparedAt <= 3000;
		if (current) current = m_State.GetLastPhysicalMotionAtMs() == m_iPreparedMotionAt && m_aSites.Count() < 2;
		if (current)
		{
			bool duplicate;
			foreach (vector site : m_aSites)
				if (NearFailedSite(m_vPreparedOrigin, site, 4)) duplicate = true;
			if (!duplicate) m_aSites.Insert(m_vPreparedOrigin);
		}
		m_bPrepared = false;
		m_PreparedScope = null;
		return current;
	}
	void BeginOwnedRouteReplacement(AICF_TransportTrip trip, Vehicle vehicle)
	{
		m_bOwnedReplacement = m_aSites.Count() > 0 && RouteCurrent(trip, vehicle);
		m_ExpectedWaypoint = null;
	}
	void StageOwnedRouteReplacement(AICF_TransportTrip trip, Vehicle vehicle, bool successful)
	{
		if (!successful || !m_bOwnedReplacement || !Context(trip, vehicle))
		{
			Clear();
			return;
		}
		AIWaypoint next = m_State.GetRouteWaypoint();
		if (!next || next == m_Waypoint || m_State.GetSupersededRouteWaypoint() != m_Waypoint || m_State.GetRouteGeneration() != m_iRouteGeneration + 1)
		{
			Clear();
			return;
		}
		m_ExpectedWaypoint = next;
		m_ExpectedWaypointId = next.GetID();
		m_iExpectedGeneration = m_State.GetRouteGeneration();
	}
	void AdvanceOwnedRoute(AICF_TransportTrip trip, Vehicle vehicle, AIWaypoint bound)
	{
		if (!m_bOwnedReplacement || !m_ExpectedWaypoint) return;
		if (!Context(trip, vehicle) || !bound || bound != m_ExpectedWaypoint || bound.GetID() != m_ExpectedWaypointId)
		{
			Clear();
			return;
		}
		if (m_State.GetRouteWaypoint() != bound || m_State.GetRouteGeneration() != m_iExpectedGeneration || !m_State.IsRouteWaypointBound())
		{
			Clear();
			return;
		}
		array<AIWaypoint> queue = {};
		m_Group.GetWaypoints(queue);
		if (!queue.Contains(bound))
		{
			Clear();
			return;
		}
		if (!Context(trip, vehicle) || !bound || bound != m_ExpectedWaypoint || !m_State.IsRouteWaypointBound())
		{
			Clear();
			return;
		}
		if (m_State.GetRouteWaypoint() != bound || m_State.GetRouteGeneration() != m_iExpectedGeneration)
		{
			Clear();
			return;
		}
		m_Waypoint = bound;
		m_iRouteGeneration = m_iExpectedGeneration;
		m_bOwnedReplacement = false;
		m_ExpectedWaypoint = null;
	}
	int Count() { return m_aSites.Count(); }
	bool HasPreparedFailure() { return m_bPrepared; }
	void CancelPreparedFailure()
	{
		m_bPrepared = false;
		m_PreparedScope = null;
	}
}
