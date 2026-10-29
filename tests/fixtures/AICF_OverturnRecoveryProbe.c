// Isolated VehicleScenario scaffold. A local pose fault is injected immediately
// before the real coordinator tick; no health writes or large entity movement.
class AICF_OverturnProbeState
{
	static AICF_TransportTrip s_Trip;
	static AICF_GroupSlot s_Slot;
	static Vehicle s_Vehicle;
	static bool s_Armed;
	static int s_InjectedAt;
	static int s_RouteGeneration;
	static int s_Deadline;
	static int s_Checks;
	static int s_Failures;
	static vector s_Flipped;
	static void Check(string rule, bool passed, string details = "")
	{
		s_Checks++; if (!passed) s_Failures++;
		Print(string.Format("[AICF][OVERTURN_CASE] rule=%1 passed=%2 details=[%3]", rule, passed, details));
	}
}
modded class AICF_VehicleMovementState
{
	int AICF_OPSetAttempts(int value) { int previous = m_iMobilityRecoveryAttempts; m_iMobilityRecoveryAttempts = value; return previous; }
}
modded class AICF_TransportTrip
{
	int AICF_OPSetDeadline(int value) { int previous = m_iAbsoluteDeadlineMs; m_iAbsoluteDeadlineMs = value; return previous; }
}
modded class AICF_VehicleCoordinator
{
	override void Update(AICF_FactionState usState, SCR_CampaignFaction usFaction, AICF_FactionState ussrState,
		SCR_CampaignFaction ussrFaction, int baseRevision, bool dispatchTrips)
	{
		AICF_TransportTrip trip = AICF_OverturnProbeState.s_Trip;
		if (AICF_OverturnProbeState.s_Armed && trip && GetTrip(AICF_OverturnProbeState.s_Slot) == trip)
		{
			AICF_OverturnProbeState.s_Armed = false;
			Vehicle vehicle = AICF_OverturnProbeState.s_Vehicle;
			AICF_OverturnProbeState.Check("UPRIGHT_NOT_RECOVERY_SCOPE", !AICF_OverturnRecovery.CanConsider(trip, System.GetTickCount()));
			vector before = vehicle.GetOrigin(); vector angles = vehicle.GetYawPitchRoll(); angles[2] = 95;
			vector transform[4]; Math3D.AnglesToMatrix(angles, transform); transform[3] = before + Vector(0, 1.4, 0);
			bool changed = vehicle.SetWorldTransform(transform);
			vehicle.GetPhysics().SetVelocity(vector.Zero); vehicle.GetPhysics().SetAngularVelocity(vector.Zero);
			AICF_OverturnProbeState.s_Flipped = vehicle.GetOrigin();
			AICF_OverturnProbeState.s_InjectedAt = System.GetTickCount();
			AICF_OverturnProbeState.Check("HEALTHY_FLIPPED_BEFORE_NATIVE_TICK", changed &&
				AICF_OverturnRecovery.CanConsider(trip, System.GetTickCount()) && vector.Distance(before, vehicle.GetOrigin()) < 2,
				string.Format("before=%1 flipped=%2 ypr=%3 speed=%4", before, vehicle.GetOrigin(), vehicle.GetYawPitchRoll(), vehicle.GetPhysics().GetVelocity().Length()));
			AICF_VehicleMovementState state = trip.GetMovementState();
			int previousAttempts = state.AICF_OPSetAttempts(2);
			AICF_OverturnProbeState.Check("EXHAUSTED_OLD_BUDGET_REJECTED", !AICF_OverturnRecovery.CanConsider(trip, System.GetTickCount()));
			state.AICF_OPSetAttempts(previousAttempts);
			int previousDeadline = trip.AICF_OPSetDeadline(System.GetTickCount() - 1);
			AICF_OverturnProbeState.Check("EXPIRED_TRIP_REJECTED", !AICF_OverturnRecovery.CanConsider(trip, System.GetTickCount()));
			trip.AICF_OPSetDeadline(previousDeadline);
			state.SetOverturnRecovery(new AICF_OverturnRecovery(trip, System.GetTickCount() - 3001));
			AICF_OverturnProbeState.Check("EXPIRED_OBSERVATION_REJECTED", !AICF_OverturnRecovery.CanConsider(trip, System.GetTickCount()));
			state.SetOverturnRecovery(null);
			vehicle.GetPhysics().SetVelocity("1.01 0 0");
			state.SetOverturnRecovery(new AICF_OverturnRecovery(trip, System.GetTickCount()));
			AICF_OverturnProbeState.Check("FAST_POSE_NEVER_MUTATES", !AICF_OverturnRecovery.CanMutateNow(trip));
			state.SetOverturnRecovery(null); vehicle.GetPhysics().SetVelocity(vector.Zero);
		}
		super.Update(usState, usFaction, ussrState, ussrFaction, baseRevision, dispatchTrips);
	}
}
modded class AICF_MatchController
{
	protected AICF_TransportTrip m_OPTrip;
	protected Vehicle m_OPVehicle;
	protected EntityID m_OPVehicleId;
	protected vector m_vOPRepaired;
	protected bool m_bOPRepaired;
	protected int m_iOPStarted;
	protected int m_iOPMoving;
	protected float m_fOPCarHealth;
	protected ref array<float> m_aOPMemberHealth = {};
	override protected bool AICF_VSAlternativeScenario(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		int now = System.GetTickCount(); if (!m_iOPStarted) m_iOPStarted = now;
		if (!m_OPTrip && m_iVSStep < 2) return false;
		if (now - m_iOPStarted > 300000 || AICF_OverturnProbeState.s_Failures || !trip || trip.IsTerminal())
		{
			AICF_OverturnProbeState.Check("BOUNDED_CURRENT_TRIP", false); AICF_OPFinish(); return true;
		}
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetLease() || !trip.GetLease().HasPhysicalAsset()) return true;
		AICF_VehicleMovementState state = trip.GetMovementState(); Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!m_OPTrip)
		{
			if (vector.DistanceXZ(vehicle.GetOrigin(), m_vVSStart) < 25 || !AICF_VSExact(slot) || !state.IsRouteWaypointBound()) return true;
			SCR_AIGroupUtilityComponent utility = m_VSGroup.GetGroupUtilityComponent(); SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
			if (!move || utility.GetExecutedAction() != move || move.GetActionState() != EAIActionState.RUNNING || move.m_RelatedWaypoint != state.GetRouteWaypoint()) return true;
			if (state.GetNativeGate() || state.GetCrewRecoveryToken() || state.GetTransitMoveFailure() || state.GetNativeCombatActivity()) return true;
			m_OPTrip = trip; m_OPVehicle = vehicle; m_OPVehicleId = vehicle.GetID();
			AICF_OverturnProbeState.s_Trip = trip; AICF_OverturnProbeState.s_Slot = slot; AICF_OverturnProbeState.s_Vehicle = vehicle;
			AICF_OverturnProbeState.s_RouteGeneration = state.GetRouteGeneration(); AICF_OverturnProbeState.s_Deadline = trip.GetAbsoluteDeadlineMs();
			m_fOPCarHealth = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent)).GetDefaultHitZone().GetHealthScaled();
			for (int member; member < m_iVSSize; member++) m_aOPMemberHealth.Insert(DamageManagerComponent.Cast(slot.GetRosterMember(member).FindComponent(DamageManagerComponent)).GetDefaultHitZone().GetHealthScaled());
			AICF_OverturnProbeState.Check("OLD_TWO_ATTEMPTS_AVAILABLE", state.GetMobilityRecoveryAttempts() == 0 && state.GetMaximumMobilityRecoveryAttempts() == 2);
			AICF_OverturnProbeState.s_Armed = true; return true;
		}
		if (!AICF_OverturnProbeState.s_InjectedAt) return true;
		if (!m_bOPRepaired)
		{
			if (state.GetRouteGeneration() <= AICF_OverturnProbeState.s_RouteGeneration) return true;
			m_vOPRepaired = vehicle.GetOrigin(); vector transform[4]; vehicle.GetWorldTransform(transform);
			AICF_OverturnProbeState.Check("ACTUAL_SHORT_UPRIGHT_ROUTE_REPAIR", transform[1][1] >= 0.25 &&
				vector.DistanceXZ(AICF_OverturnProbeState.s_Flipped, m_vOPRepaired) >= 4 && vector.Distance(AICF_OverturnProbeState.s_Flipped, m_vOPRepaired) <= 15,
				string.Format("flipped=%1 repaired=%2 actual3d=%3 up_y=%4", AICF_OverturnProbeState.s_Flipped, m_vOPRepaired, vector.Distance(AICF_OverturnProbeState.s_Flipped, m_vOPRepaired), transform[1][1]));
			AICF_OverturnProbeState.Check("OLD_BUDGET_AND_DEADLINE_UNCHANGED", state.GetMobilityRecoveryAttempts() == 1 && state.GetMaximumMobilityRecoveryAttempts() == 2 && trip.GetAbsoluteDeadlineMs() == AICF_OverturnProbeState.s_Deadline && !state.GetOverturnRecovery());
			m_bOPRepaired = true; return true;
		}
		if (vehicle.GetPhysics().GetVelocity().Length() >= 1) m_iOPMoving++;
		if (vector.DistanceXZ(m_vOPRepaired, vehicle.GetOrigin()) < 25 || m_iOPMoving < 3) return true;
		bool health = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent)).GetDefaultHitZone().GetHealthScaled() == m_fOPCarHealth;
		for (int actorIndex; actorIndex < m_iVSSize; actorIndex++) health = health && DamageManagerComponent.Cast(slot.GetRosterMember(actorIndex).FindComponent(DamageManagerComponent)).GetDefaultHitZone().GetHealthScaled() == m_aOPMemberHealth[actorIndex];
		AICF_OverturnProbeState.Check("ORIGINAL_FIVE_SAME_ASSET_HEALTH_UNCHANGED", health && trip == m_OPTrip && vehicle == m_OPVehicle && vehicle.GetID() == m_OPVehicleId && AICF_VSExact(slot));
		AICF_OverturnProbeState.Check("ACTUAL_NATIVE_DRIVE_AFTER_UPRIGHT", true, string.Format("displacement_m=%1 velocity_polls=%2 deadline=%3", vector.DistanceXZ(m_vOPRepaired, vehicle.GetOrigin()), m_iOPMoving, trip.GetAbsoluteDeadlineMs()));
		AICF_OPFinish(); return true;
	}
	protected void AICF_OPFinish()
	{
		AICF_OverturnProbeState.s_Armed = false;
		Print(string.Format("[AICF][OVERTURN_FINISHED] checks=%1 failures=%2 classification=CONTROLLED_OVERTURN_RECOVERY_ONLY", AICF_OverturnProbeState.s_Checks, AICF_OverturnProbeState.s_Failures));
		AICF_VSFinish(); AICF_OverturnProbeState.s_Trip = null; AICF_OverturnProbeState.s_Vehicle = null; AICF_OverturnProbeState.s_Slot = null;
	}
}
