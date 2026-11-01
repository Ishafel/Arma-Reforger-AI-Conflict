// Isolated Scenario scaffold only. Queues the actual stock high-priority
// GetOut activity; native selection/dispatch and production recovery decide.
// This is a controlled fault, not evidence of a naturally occurring AI failure.
class AICF_HealthyAbandonObservation
{
	static AICF_TransportTrip s_Trip;
	static SCR_AIGetOutActivity s_Action;
	static bool s_bObserved;
	static bool s_bRecovered;
	static int s_iAttemptsBefore;
	static int s_iDeadline;

	static bool Enabled()
	{
		string enabled;
		return System.GetCLIParam("aicfHealthyNativeAbandonProbe", enabled) && enabled == "1";
	}
}

modded class AICF_VehicleTransitFlow
{
	override protected AICF_TripOutcome RecoverHealthyNativeAbandon(AICF_TransportTrip trip, string causationId)
	{
		AICF_TripOutcome outcome = super.RecoverHealthyNativeAbandon(trip, causationId);
		if (!AICF_HealthyAbandonObservation.Enabled() || AICF_HealthyAbandonObservation.s_Trip != trip ||
			AICF_HealthyAbandonObservation.s_bObserved) return outcome;
		AICF_HealthyAbandonObservation.s_bObserved = true;
		SCR_AIGetOutActivity action = AICF_HealthyAbandonObservation.s_Action;
		AICF_HealthyAbandonObservation.s_bRecovered = outcome && outcome.GetKind() == AICF_ETripOutcomeKind.START_MOVEMENT &&
			action && action.GetActionState() == EAIActionState.FAILED &&
			trip.GetMovementState().GetMobilityRecoveryAttempts() == AICF_HealthyAbandonObservation.s_iAttemptsBefore + 1 &&
			trip.GetAbsoluteDeadlineMs() == AICF_HealthyAbandonObservation.s_iDeadline;
		Print(string.Format("[AICF][HEALTHY_NATIVE_ABANDON_PROBE] event=DECISION operation_id=%1 observed=1 recovered=%2 attempts=%3 deadline=%4 controlled_fault=1 native_selection=1 production_outcome_unchanged=1",
			trip.GetOperationId(), AICF_HealthyAbandonObservation.s_bRecovered,
			trip.GetMovementState().GetMobilityRecoveryAttempts(), trip.GetAbsoluteDeadlineMs()));
		return outcome;
	}
}

modded class AICF_MatchController
{
	protected int m_iHNAStarted;
	protected int m_iHNAInjected;
	protected int m_iHNARoute;
	protected int m_iHNAMovingPolls;
	protected bool m_bHNAMovementStarted;
	protected vector m_vHNAMovementStart;
	protected Vehicle m_HNAVehicle;
	protected EntityID m_HNAVehicleId;
	protected ref AICF_VehicleWatchdog m_HNAWatchdog = new AICF_VehicleWatchdog();
	protected bool m_bHNAProfileRestored;

	override protected bool SpawnInitialRoster(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		if (AICF_HealthyAbandonObservation.Enabled() && AICF_VSCanonicalOwner() && Replication.IsServer() &&
			factionState && factionState == m_USSRState && faction && faction == m_USSRFaction)
		{
			AICF_GroupSlot slot = factionState.GetSlot(0);
			if (!slot || slot.GetState() != AICF_EGroupSlotState.EMPTY || slot.GetGroup()) return false;
			slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
			slot.SetDesiredSize(5);
		}
		return super.SpawnInitialRoster(factionState, faction);
	}

	override protected void ProcessFaction(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		super.ProcessFaction(factionState, faction);
		if (!AICF_HealthyAbandonObservation.Enabled() || !AICF_VSCanonicalOwner() || !Replication.IsServer() ||
			m_bStopped || m_bHNAProfileRestored || factionState != m_USSRState || faction != m_USSRFaction) return;
		AICF_GroupSlot slot = factionState.GetSlot(0);
		if (!slot || !slot.IsCombatReady() || slot.GetSpawnGeneration() != 1 || slot.IsReplacementDeployment() ||
			slot.GetUnitType() != AICF_EGroupUnitType.MOTORIZED_LIGHT || slot.GetDesiredSize() != 5 ||
			!slot.GetGroup() || slot.GetGroup().GetAgentsCount() != 5 || !slot.GetRosterMember(4)) return;
		slot.SetUnitType(AICF_EGroupUnitType.INFANTRY);
		m_bHNAProfileRestored = true;
		Print("[AICF][HEALTHY_NATIVE_ABANDON_PROBE] event=CANONICAL_READY actual=5 initial_generation=1 unit_type=INFANTRY before_coordinator_tick=1");
	}

	override protected bool AICF_VSAlternativeScenario(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		if (!AICF_HealthyAbandonObservation.Enabled()) return super.AICF_VSAlternativeScenario(slot, trip);
		int now = System.GetTickCount();
		if (!m_iHNAStarted) m_iHNAStarted = now;
		if (now - m_iHNAStarted > 300000 || (m_iHNAInjected && now - m_iHNAInjected > 90000))
		{
			AICF_VSCheck("HNA_TIMEOUT", false); AICF_VSFinish(); return true;
		}
		if (!m_iHNAInjected && m_iVSStep < 2) return false;
		if (!trip || trip.IsTerminal())
		{
			AICF_VSCheck("HNA_UNEXPECTED_TERMINAL", false); AICF_VSFinish(); return true;
		}
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetLease() ||
			!trip.GetLease().HasPhysicalAsset() || !trip.GetMovementState().IsRouteWaypointBound()) return true;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		AICF_VehicleMovementState state = trip.GetMovementState();
		if (!m_iHNAInjected)
		{
			SCR_AIGroupUtilityComponent utility = m_VSGroup.GetGroupUtilityComponent();
			SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
			if (!move || utility.GetExecutedAction() != move || move.GetActionState() != EAIActionState.RUNNING ||
				move.m_RelatedWaypoint != state.GetRouteWaypoint() || state.GetNativeGate() ||
				state.GetCrewRecoveryToken() || state.GetTransitMoveFailure() ||
				!m_HNAWatchdog.AreAllAliveMembersSettledInVehicle(m_VSGroup, vehicle) ||
				vector.DistanceXZ(vehicle.GetOrigin(), m_vVSStart) < 25) return true;
			m_HNAVehicle = vehicle; m_HNAVehicleId = vehicle.GetID();
			m_iHNARoute = state.GetRouteGeneration();
			AICF_HealthyAbandonObservation.s_Trip = trip;
			AICF_HealthyAbandonObservation.s_iAttemptsBefore = state.GetMobilityRecoveryAttempts();
			AICF_HealthyAbandonObservation.s_iDeadline = trip.GetAbsoluteDeadlineMs();
			ref SCR_AIGetOutActivity abandonment = new SCR_AIGetOutActivity(utility, null, vehicle,
				priority: SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY,
				priorityLevel: SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
			utility.AddAction(abandonment);
			AICF_HealthyAbandonObservation.s_Action = abandonment;
			m_iHNAInjected = now;
			Print(string.Format("[AICF][HEALTHY_NATIVE_ABANDON_PROBE] event=INJECTED operation_id=%1 vehicle=%2 route_generation=%3 attempts=%4 deadline=%5 health_mutation=0 pose_mutation=0 controlled_fault=1",
				trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), m_iHNARoute, state.GetMobilityRecoveryAttempts(), trip.GetAbsoluteDeadlineMs()));
			return true;
		}
		if (trip != AICF_HealthyAbandonObservation.s_Trip || vehicle != m_HNAVehicle || vehicle.GetID() != m_HNAVehicleId || !AICF_VSOwnRecoveryRoster(slot))
		{
			AICF_VSCheck("HNA_SAME_VEHICLE_ORIGINAL_ROSTER", false); AICF_VSFinish(); return true;
		}
		if (!AICF_HealthyAbandonObservation.s_bObserved) return true;
		if (!AICF_HealthyAbandonObservation.s_bRecovered)
		{
			AICF_VSCheck("HNA_PRODUCTION_RECOVERY", false); AICF_VSFinish(); return true;
		}
		if (!m_bHNAMovementStarted)
		{
			AICF_VSCheck("HNA_PRODUCTION_RECOVERY", state.GetRouteGeneration() > m_iHNARoute);
			m_bHNAMovementStarted = true;
			m_vHNAMovementStart = vehicle.GetOrigin();
			return true;
		}
		Physics physics = vehicle.GetPhysics();
		if (physics && physics.GetVelocity().Length() >= 0.5) m_iHNAMovingPolls++;
		float displacement = vector.DistanceXZ(m_vHNAMovementStart, vehicle.GetOrigin());
		if (displacement < 25 || m_iHNAMovingPolls < 3) return true;
		AICF_VSCheck("HNA_SAME_VEHICLE_PHYSICAL_MOVEMENT", m_HNAWatchdog.AreAllAliveMembersSettledInVehicle(m_VSGroup, vehicle) &&
			trip.GetAbsoluteDeadlineMs() == AICF_HealthyAbandonObservation.s_iDeadline);
		Print(string.Format("[AICF][HEALTHY_NATIVE_ABANDON_PROBE] event=DONE operation_id=%1 vehicle=%2 displacement_m=%3 moving_polls=%4 attempts=%5 deadline=%6 original_roster=1 controlled_fault=1 natural_failure_proof=0",
			trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), displacement, m_iHNAMovingPolls,
			state.GetMobilityRecoveryAttempts(), trip.GetAbsoluteDeadlineMs()));
		AICF_VSFinish(); return true;
	}
}
