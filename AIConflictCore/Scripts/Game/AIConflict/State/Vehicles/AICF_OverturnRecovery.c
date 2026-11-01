// A short observation lease for one healthy, fully crewed AI vehicle roll.
// It never extends trip clocks and never authorizes health or roster changes.
class AICF_OverturnRecovery
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_VehicleLease m_Lease;
	protected AIWaypoint m_Waypoint;
	protected int m_iRouteGeneration;
	protected int m_iObservedAtMs;
	static const int OBSERVATION_WINDOW_MS = 3000;

	void AICF_OverturnRecovery(AICF_TransportTrip trip, int nowMs)
	{
		m_Trip = trip;
		m_Assignment = trip.GetAssignment();
		m_Lease = trip.GetLease();
		m_Waypoint = trip.GetMovementState().GetRouteWaypoint();
		m_iRouteGeneration = trip.GetMovementState().GetRouteGeneration();
		m_iObservedAtMs = nowMs;
	}
	static bool WithinWindow(int nowMs, int observedAtMs)
	{
		return observedAtMs > 0 && nowMs >= observedAtMs && nowMs - observedAtMs <= OBSERVATION_WINDOW_MS;
	}
	static bool IsSlowEnough(float speed, float angularSpeed)
	{
		return speed >= 0 && speed <= 1.0 && angularSpeed >= 0 && angularSpeed <= 0.5;
	}
	static bool IsLocalCorrectionDistance(float meters)
	{
		return meters >= 0 && meters <= 15.0;
	}
	bool Matches(AICF_TransportTrip trip, int nowMs)
	{
		if (!trip || trip != m_Trip || trip.GetAssignment() != m_Assignment || trip.GetLease() != m_Lease) return false;
		AICF_VehicleMovementState state = trip.GetMovementState();
		if (!state || state.GetRouteGeneration() != m_iRouteGeneration || state.GetRouteWaypoint() != m_Waypoint) return false;
		return WithinWindow(nowMs, m_iObservedAtMs);
	}
	int AgeMs(int nowMs) { return nowMs - m_iObservedAtMs; }

	static bool CanConsider(AICF_TransportTrip trip, int nowMs)
	{
		string reason;
		if (!ReadScope(trip, reason)) return false;
		AICF_VehicleMovementState state = trip.GetMovementState();
		if (state.GetMobilityRecoveryAttempts() >= state.GetMaximumMobilityRecoveryAttempts()) return false;
		AICF_OverturnRecovery observed = state.GetOverturnRecovery();
		return !observed || observed.Matches(trip, nowMs);
	}
	static bool CanMutateNow(AICF_TransportTrip trip)
	{
		string reason;
		if (!ReadScope(trip, reason)) return false;
		AICF_OverturnRecovery observed = trip.GetMovementState().GetOverturnRecovery();
		if (!observed || !observed.Matches(trip, System.GetTickCount())) return false;
		Physics physics = trip.GetLease().GetVehicle().GetPhysics();
		return physics && IsSlowEnough(physics.GetVelocity().Length(), physics.GetAngularVelocity().Length());
	}
	static bool ReadScope(AICF_TransportTrip trip, out string reason)
	{
		reason = "TRIP_SCOPE";
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() ||
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetVehiclePreset() ||
			!trip.IsVehiclePresetCurrent() || trip.IsDeadlineReached(System.GetTickCount())) return false;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		if (!group) return false;
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		Vehicle vehicle;
		SCR_AIGroup verifiedGroup;
		SCR_AIGroupVehicle registered;
		AIAgent pilot;
		AIGroupMovementComponent movement;
		if (!AICF_VehicleNativeSubgroup.Read(trip, utility, verifiedGroup, vehicle, registered, pilot, movement, reason)) return false;
		if (verifiedGroup != group || !vehicle || !registered || !pilot || !movement) return false;
		reason = "CURRENT_MOVE_SCOPE";
		AICF_VehicleMovementState state = trip.GetMovementState();
		if (!state || state.GetTransitMoveFailure() || state.GetNativeCombatActivity() ||
			state.GetRouteGeneration() <= 0 || !state.IsRouteWaypointBound() || !state.GetRouteWaypoint()) return false;
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
		if (!move || utility.GetExecutedAction() != move || move.GetActionState() != EAIActionState.RUNNING ||
			move.GetRemoveAction() || move.GetActionIsSuspended() || move.m_Utility != utility ||
			move.m_RelatedWaypoint != state.GetRouteWaypoint() || group.GetCurrentWaypoint() != state.GetRouteWaypoint()) return false;
		if (!AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(trip, utility, registered.GetSubgroupHandleId())) return false;
		reason = "HEALTHY_OVERTURN_ONLY";
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (!usage || usage.GetDamageState() != EDamageState.UNDAMAGED || !damage || !damage.GetDefaultHitZone()) return false;
		if (damage.GetDefaultHitZone().GetHealthScaled() != 1.0 || damage.GetMovementDamage() != 0 ||
			!SCR_AIVehicleUsability.VehicleCanMove(vehicle) || SCR_AIVehicleUsability.VehicleIsOnFire(vehicle)) return false;
		vector transform[4]; vehicle.GetWorldTransform(transform);
		if (transform[1][1] >= 0.25 || !vehicle.GetPhysics()) return false;
		reason = "ALL_ORIGINAL_CONSCIOUS_SETTLED";
		array<AIAgent> agents = {}; group.GetAgents(agents);
		int captured;
		for (int index; index < AICF_Stage1Config.MAX_GROUP_SIZE; index++)
		{
			IEntity actor = trip.VehiclePresetMember(index);
			if (!actor) continue;
			captured++;
			if (!AICF_GroupRuntime.IsAliveCharacter(actor) || !SCR_AIDamageHandling.IsConscious(actor) ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor) || !AICF_VehicleNativeSubgroup.Settled(actor, vehicle)) return false;
		}
		if (captured <= 0 || captured != agents.Count()) return false;
		reason = "READY";
		return true;
	}
}
