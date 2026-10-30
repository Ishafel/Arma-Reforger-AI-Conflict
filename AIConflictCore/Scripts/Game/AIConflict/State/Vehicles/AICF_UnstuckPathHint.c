// Только подсказка выбора короткой коррекции. Не является route-progress proof.
class AICF_UnstuckPathHint
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_MatchController m_Controller;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_VehicleLease m_Lease;
	protected ref AICF_UnstuckMutationScope m_MutationScope;
	protected SCR_AIGroup m_Group;
	protected SCR_AIGroupUtilityComponent m_Utility;
	protected SCR_AIGroupVehicle m_Record;
	protected AIAgent m_Pilot;
	protected AICarMovementComponent m_Car;
	protected AIAgent m_CarAgent;
	protected SCR_AIMoveActivity m_Move;
	protected AIWaypoint m_Waypoint;
	protected EntityID m_WaypointId;
	protected RplComponent m_WaypointRpl;
	protected string m_sWaypointRplId;
	protected string m_sScope;
	protected vector m_vOrigin;
	protected vector m_vEndpoint;
	protected vector m_vGoal;
	protected int m_iHandler;
	protected int m_iRouteGeneration;
	protected int m_iCapturedAtMs;
	protected float m_fOriginArc;
	protected ref array<vector> m_aPoints = {};
	protected string m_sKind;
	static const int MAXIMUM_LOCAL_POINTS = 128;
	static const int MAXIMUM_AGE_MS = 3000;

	static bool LocalFragment(array<vector> points, vector origin)
	{
		if (!points || points.Count() < 2 || points.Count() > MAXIMUM_LOCAL_POINTS ||
			!AICF_VehicleNativeRouteProgress.FinitePoint(origin)) return false;
		foreach (vector point : points)
			if (!AICF_VehicleNativeRouteProgress.FinitePoint(point)) return false;
		return vector.Distance(points[0], origin) <= 2.0;
	}

	static bool ForwardCandidate(array<vector> points, float originArc, vector candidate)
	{
		AICF_VehicleNativeRouteProjection projected = AICF_VehicleNativeRouteProgress.ProjectUnique(points, candidate);
		return projected && projected.m_sReason.IsEmpty() && projected.m_fArc >= originArc + 4.0 &&
			projected.m_fGap <= 2.0 && projected.m_fHeight <= 2.0;
	}

	static AICF_UnstuckPathHint Capture(AICF_TransportTrip trip, vector origin)
	{
		AICF_UnstuckPathHint hint = new AICF_UnstuckPathHint();
		hint.m_Trip = trip;
		hint.m_vOrigin = origin;
		hint.m_iCapturedAtMs = System.GetTickCount();
		if (!hint.ReadInitial()) return null;
		hint.m_Car.GetCurrentPath(hint.m_aPoints);
		bool fullRoute = AICF_VehicleNativeRouteProgress.ValidatePath(hint.m_aPoints, hint.m_vEndpoint);
		if (!fullRoute && !LocalFragment(hint.m_aPoints, origin)) return null;
		AICF_VehicleNativeRouteProjection projected = AICF_VehicleNativeRouteProgress.ProjectUnique(hint.m_aPoints, origin);
		if (!projected || !projected.m_sReason.IsEmpty() || projected.m_fGap > 2.0 || projected.m_fHeight > 2.0) return null;
		hint.m_fOriginArc = projected.m_fArc;
		hint.m_sKind = "LOCAL_FRAGMENT";
		if (fullRoute) hint.m_sKind = "FULL_NATIVE_PATH";
		if (!hint.IsCurrent()) return null;
		return hint;
	}

	protected bool ReadInitial()
	{
		if (!Replication.IsServer() || !m_Trip || !m_Trip.IsValid() || m_Trip.IsTerminal() ||
			m_Trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || m_Trip.IsDeadlineReached(System.GetTickCount())) return false;
		m_Controller = AICF_MatchController.GetActiveController();
		m_Assignment = m_Trip.GetAssignment();
		m_Lease = m_Trip.GetLease();
		if (!m_Controller || !m_Assignment || !m_Lease) return false;
		m_Group = m_Assignment.GetGroup();
		if (!m_Group || m_Controller.FindManagedInfantrySlot(m_Group) != m_Trip.GetVehiclePresetSlot()) return false;
		m_Utility = m_Group.GetGroupUtilityComponent();
		SCR_AIGroup verifiedGroup;
		Vehicle vehicle;
		AIGroupMovementComponent movement;
		string reason;
		if (!AICF_VehicleNativeSubgroup.Read(m_Trip, m_Utility, verifiedGroup, vehicle, m_Record, m_Pilot, movement, reason)) return false;
		if (verifiedGroup != m_Group || !vehicle || !m_Record || !m_Pilot || !movement || vehicle.GetOrigin() != m_vOrigin) return false;
		m_MutationScope = AICF_UnstuckMutationScope.Capture(m_Trip, vehicle);
		if (!m_MutationScope) return false;
		m_iHandler = m_Record.GetSubgroupHandleId();
		if (!AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(m_Trip, m_Utility, m_iHandler)) return false;
		AICF_VehicleMovementState state = m_Trip.GetMovementState();
		if (!state || !state.IsRouteWaypointBound() || state.GetRouteGeneration() <= 0) return false;
		m_Waypoint = state.GetRouteWaypoint();
		m_iRouteGeneration = state.GetRouteGeneration();
		m_vEndpoint = state.GetRouteEndpoint();
		m_vGoal = m_Assignment.GetTargetPosition();
		m_Move = SCR_AIMoveActivity.Cast(m_Utility.GetCurrentAction());
		if (!MoveCurrent()) return false;
		m_WaypointId = m_Waypoint.GetID();
		m_WaypointRpl = RplComponent.Cast(m_Waypoint.FindComponent(RplComponent));
		if (m_WaypointRpl)
		{
			if (!m_WaypointRpl.IsMaster() || m_WaypointRpl.IsProxy()) return false;
			m_sWaypointRplId = m_WaypointRpl.Id().ToString();
		}
		m_sScope = Scope();
		m_Car = AICarMovementComponent.Cast(m_Pilot.GetMovementComponent());
		if (!m_Car) return false;
		m_CarAgent = m_Car.GetAIAgent();
		return HealthyActiveStationary(vehicle);
	}
	protected string Scope()
	{
		return string.Format("%1:%2:%3:%4:%5:%6:%7:%8", m_Trip.GetFactionKey(), m_Trip.GetSlotId(),
			m_Trip.GetGroupGeneration(), m_Trip.GetTripGeneration(), m_Lease.GetLeaseGeneration(), m_Lease.GetVehicleGeneration(),
			m_Assignment.GetAssignmentRevision(), m_Assignment.GetStrategicIntentRevision());
	}

	protected bool MoveCurrent()
	{
		if (!m_Group || !m_Utility) return false;
		if (!m_Move || !m_Waypoint || m_Move.m_Utility != m_Utility || m_Move.m_RelatedWaypoint != m_Waypoint ||
			m_Utility.GetCurrentAction() != m_Move || m_Utility.GetExecutedAction() != m_Move) return false;
		if (m_Move.GetRemoveAction() || m_Move.GetActionIsSuspended() || m_Move.GetActionState() != EAIActionState.RUNNING) return false;
		array<AIWaypoint> queue = {};
		m_Group.GetWaypoints(queue);
		return m_Group.GetCurrentWaypoint() == m_Waypoint && queue.Contains(m_Waypoint);
	}

	protected bool HealthyActiveStationary(Vehicle vehicle)
	{
		if (!vehicle || !m_Pilot || !m_CarAgent || m_CarAgent.GetControlledEntity() != vehicle ||
			!m_CarAgent.IsAIActivated() || !m_CarAgent.CountTowardsAIWorldLimit() || m_CarAgent.GetLOD() != 0 ||
			!m_Pilot.IsAIActivated()) return false;
		Physics physics = vehicle.GetPhysics();
		if (!physics || physics.GetVelocity().Length() > 1.0) return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (!usage || usage.GetDamageState() != EDamageState.UNDAMAGED || !damage || !damage.GetDefaultHitZone()) return false;
		return damage.GetDefaultHitZone().GetHealthScaled() == 1.0 && damage.GetMovementDamage() == 0 &&
			SCR_AIVehicleUsability.VehicleCanMove(vehicle) && !SCR_AIVehicleUsability.VehicleIsOnFire(vehicle);
	}

	bool IsCurrent()
	{
		if (!m_Trip || !m_Assignment || !m_Lease || !m_Group || !m_Utility) return false;
		if (!m_Record || !m_Pilot || !m_Car || !m_CarAgent || !m_MutationScope) return false;
		int now = System.GetTickCount();
		if (m_iCapturedAtMs <= 0 || now < m_iCapturedAtMs || now - m_iCapturedAtMs > MAXIMUM_AGE_MS) return false;
		if (!m_Trip || m_Trip.IsTerminal() || m_Trip.IsDeadlineReached(now) || m_Trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) return false;
		if (m_Trip.GetAssignment() != m_Assignment || m_Trip.GetLease() != m_Lease || !m_Controller ||
			AICF_MatchController.GetActiveController() != m_Controller || m_Controller.FindManagedInfantrySlot(m_Group) != m_Trip.GetVehiclePresetSlot()) return false;
		if (Scope() != m_sScope || !m_Waypoint || m_Waypoint.GetID() != m_WaypointId ||
			RplComponent.Cast(m_Waypoint.FindComponent(RplComponent)) != m_WaypointRpl) return false;
		if (m_WaypointRpl && (!m_WaypointRpl.IsMaster() || m_WaypointRpl.IsProxy() || m_WaypointRpl.Id().ToString() != m_sWaypointRplId)) return false;
		AICF_VehicleMovementState state = m_Trip.GetMovementState();
		if (!state || state.GetRouteGeneration() != m_iRouteGeneration || state.GetRouteWaypoint() != m_Waypoint ||
			state.GetRouteEndpoint() != m_vEndpoint || m_Assignment.GetTargetPosition() != m_vGoal || !state.IsRouteWaypointBound()) return false;
		SCR_AIGroup group;
		Vehicle vehicle;
		SCR_AIGroupVehicle record;
		AIAgent pilot;
		AIGroupMovementComponent movement;
		string reason;
		if (!AICF_VehicleNativeSubgroup.Read(m_Trip, m_Utility, group, vehicle, record, pilot, movement, reason)) return false;
		if (!m_Record || !m_Pilot || !m_Car || !m_CarAgent || !m_Lease) return false;
		if (group != m_Group || record != m_Record || pilot != m_Pilot || vehicle != m_Lease.GetVehicle() ||
			vehicle.GetOrigin() != m_vOrigin || !MoveCurrent()) return false;
		if (!AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(m_Trip, m_Utility, m_iHandler)) return false;
		if (m_Pilot.GetMovementComponent() != m_Car || m_Car.GetAIAgent() != m_CarAgent || !HealthyActiveStationary(vehicle)) return false;
		array<vector> repeated = {};
		m_Car.GetCurrentPath(repeated);
		if (!m_Record || !m_Pilot || !m_Car || !m_CarAgent || !m_MutationScope) return false;
		if (!AICF_VehicleNativeRouteProgress.SamePath(m_aPoints, repeated)) return false;
		if (!m_MutationScope.IsCurrent() || !MoveCurrent()) return false;
		if (m_Pilot.GetMovementComponent() != m_Car || m_Car.GetAIAgent() != m_CarAgent || !HealthyActiveStationary(vehicle)) return false;
		return AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(m_Trip, m_Utility, m_iHandler);
	}

	bool Prefers(vector candidate) { return ForwardCandidate(m_aPoints, m_fOriginArc, candidate); }
	string Details()
	{
		return string.Format(" hint_only=1 path_progress=0 hint_kind=%1 hint_points=%2 hint_origin_arc=%3 hint_handler=%4 hint_route_generation=%5",
			m_sKind, m_aPoints.Count(), m_fOriginArc, m_iHandler, m_iRouteGeneration);
	}
}
