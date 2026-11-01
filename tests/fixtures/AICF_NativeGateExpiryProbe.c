// Isolated controlled expiry only after actual native OpenGate/exact-seat return.
// No fabricated actions/completion receipts, clock writes or physical correction.
class AICF_NativeGateExpiryProbe
{
	static AICF_TransportTrip s_Trip;
	static int s_iInjectedAt;
	static int s_iRoute;
	static int s_iAttempts;
	static int s_iMaximum;
	static int s_iDeadline;
	static int s_iWindowStart;
	static int s_iWindowDeadline;
	static int s_iReturnCompletedAt;
	static int s_iReturnedAt;
	static string s_sOperation;
	static EntityID s_VehicleId;
	static string s_sRpl;
	static IEntity s_SetupGate;
	static EntityID s_SetupGateId;
	static IEntity s_HQGate;
	static bool s_bDoorCloseRequested;

	static bool Enabled()
	{
		string flag;
		return GetGame() && Replication.IsServer() && System.GetCLIParam("aicfNativeGateExpiryProbe", flag) && flag == "1";
	}

	static void Log(string eventName, string details)
	{
		Print(string.Format("[AICF][NATIVE_GATE_EXPIRY_PROBE] event=%1 t_ms=%2 classification=CONTROLLED_GATE_EXPIRY_ONLY natural_expiry_proof=0 operation_id=%3 %4",
			eventName, System.GetTickCount(), s_sOperation, details));
	}
}

modded class AICF_TransportNativeGate
{
	protected bool m_bAICFGEPForcedExpiry;

	override bool IsExpired(int now)
	{
		// The flow calls IsExpired both before and inside its recovery predicate.
		// The fault remains true only on this one captured gate; no second gate/trip.
		if (m_bAICFGEPForcedExpiry) return true;
		bool natural = super.IsExpired(now);
		if (!natural) AICF_GEPPrepareActualDoor();
		if (natural || !AICF_NativeGateExpiryProbe.Enabled() || AICF_NativeGateExpiryProbe.s_iInjectedAt > 0 ||
			!m_Trip || AICF_NativeGateExpiryProbe.s_Trip != m_Trip || !Matches(m_Trip) ||
			!m_bObservedExit || !m_bReturnCompleted || m_iReturnCompletedAtMs <= 0 ||
			m_iReturnedAtMs < m_iReturnCompletedAtMs || m_iReturnedAtMs <= 0 || m_iReturnedAtMs > now ||
			!AICF_VehicleShortRecovery.IsSlowVehicle(m_Vehicle)) return natural;
		if (AICF_NativeGateExpiryProbe.s_bDoorCloseRequested && (!AICF_NativeGateExpiryProbe.s_SetupGate || m_SmartOwner != AICF_NativeGateExpiryProbe.s_SetupGate ||
			m_SmartOwner.GetID() != AICF_NativeGateExpiryProbe.s_SetupGateId)) return natural;
		AICF_VehicleMovementState state = m_Trip.GetMovementState();
		if (state.GetMobilityRecoveryAttempts() >= state.GetMaximumMobilityRecoveryAttempts()) return natural;
		m_bAICFGEPForcedExpiry = true;
		AICF_NativeGateExpiryProbe.s_iInjectedAt = now;
		AICF_NativeGateExpiryProbe.s_iRoute = state.GetRouteGeneration();
		AICF_NativeGateExpiryProbe.s_iAttempts = state.GetMobilityRecoveryAttempts();
		AICF_NativeGateExpiryProbe.s_iMaximum = state.GetMaximumMobilityRecoveryAttempts();
		AICF_NativeGateExpiryProbe.s_iDeadline = m_Trip.GetAbsoluteDeadlineMs();
		AICF_NativeGateExpiryProbe.s_iWindowStart = m_iStarted;
		AICF_NativeGateExpiryProbe.s_iWindowDeadline = m_iDeadline;
		AICF_NativeGateExpiryProbe.s_iReturnCompletedAt = m_iReturnCompletedAtMs;
		AICF_NativeGateExpiryProbe.s_iReturnedAt = m_iReturnedAtMs;
		AICF_GEPReceipt();
		return true;
	}

	protected void AICF_GEPPrepareActualDoor()
	{
		string setup;
		if (!AICF_NativeGateExpiryProbe.Enabled() || !System.GetCLIParam("aicfNativeGateExpiryGateHQ", setup) || setup != "1" ||
			AICF_NativeGateExpiryProbe.s_bDoorCloseRequested || !m_Trip || AICF_NativeGateExpiryProbe.s_Trip != m_Trip || !Matches(m_Trip)) return;
		BaseDoorComponent door = BaseDoorComponent.Cast(m_SmartOwner.FindComponent(BaseDoorComponent));
		if (!door) return;
		AICF_NativeGateExpiryProbe.s_SetupGate = m_SmartOwner;
		AICF_NativeGateExpiryProbe.s_SetupGateId = m_SmartOwner.GetID();
		AICF_NativeGateExpiryProbe.s_bDoorCloseRequested = true;
		Print(string.Format("[AICF][NATIVE_GATE_EXPIRY_SETUP] event=SELECTED gate=%1 gate_position=%2 actual_tag=OpenGate selection=ACTUAL_PRODUCTION_CAPTURED_GATE operation_id=%3",
			AICF_GroupTrace.Entity(m_SmartOwner), m_SmartOwner.GetOrigin(), m_sOperation));
		float previous = door.GetNormalizedDoorState();
		door.SetControlValue(0, RplId.Invalid());
		Print(string.Format("[AICF][NATIVE_GATE_EXPIRY_SETUP] event=CLOSE_REQUESTED gate=%1 before_state=%2 control=%3 state=%4 actor_moved=0 vehicle_moved=0 native_actions_issued=0",
			AICF_GroupTrace.Entity(m_SmartOwner), previous, door.GetControlValue(), door.GetNormalizedDoorState()));
	}

	protected void AICF_GEPReceipt()
	{
		string details = string.Format("vehicle=%1 vehicle_rpl=%2 member=%3 seat=%4 smart_owner=%5 route_generation=%6 attempts=%7 maximum=%8 trip_deadline_ms=%9",
			AICF_GroupTrace.Entity(m_Vehicle), AICF_VehicleBehaviorBinding.ComponentRpl(m_Vehicle), AICF_GroupTrace.Entity(m_Member), m_sSeatKey,
			AICF_GroupTrace.Entity(m_SmartOwner), m_iRouteGeneration, AICF_NativeGateExpiryProbe.s_iAttempts,
			AICF_NativeGateExpiryProbe.s_iMaximum, m_Trip.GetAbsoluteDeadlineMs());
		details += string.Format(" window_start_ms=%1 window_deadline_ms=%2 observed_exit=%3 return_completed_receipt=%4 return_completed_at_ms=%5 returned_at_ms=%6 injected_at_ms=%7 gate_node_rpl=%8 native_history_actual=1 clocks_written=0 actions_issued=0 physical_correction=0",
			m_iStarted, m_iDeadline, m_bObservedExit, m_bReturnCompleted, m_iReturnCompletedAtMs, m_iReturnedAtMs, AICF_NativeGateExpiryProbe.s_iInjectedAt, m_sVehicleRpl);
		AICF_NativeGateExpiryProbe.Log("EXPIRED", details);
		Log("TRANSPORT_NATIVE_GATE_CONTROLLED_EXPIRY");
	}
}

modded class AICF_MatchController
{
	protected int m_iGEPStarted;
	protected int m_iGEPRepairAt;
	protected int m_iGEPNextPoll;
	protected int m_iGEPMovingPolls;
	protected vector m_vGEPRepairPosition;
	protected vector m_vGEPScanOrigin;
	protected IEntity m_GEPClosestGate;
	protected float m_fGEPClosestGate;
	protected bool m_bGEPClosedObserved;

	protected bool AICF_GEPCollectGate(IEntity entity)
	{
		if (!entity || entity.GetWorld() != GetGame().GetWorld()) return true;
		SCR_AISmartActionComponent smart = SCR_AISmartActionComponent.Cast(entity.FindComponent(SCR_AISmartActionComponent));
		BaseDoorComponent door = BaseDoorComponent.Cast(entity.FindComponent(BaseDoorComponent));
		if (!smart || !door) return true;
		array<string> tags = {}; smart.GetTags(tags);
		if (!tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)) return true;
		float distance = vector.DistanceXZ(m_vGEPScanOrigin, entity.GetOrigin());
		if (distance >= m_fGEPClosestGate) return true;
		m_GEPClosestGate = entity; m_fGEPClosestGate = distance;
		return true;
	}

	override protected bool AICF_VSSelectFaction()
	{
		string setup;
		if (!AICF_NativeGateExpiryProbe.Enabled() || !System.GetCLIParam("aicfNativeGateExpiryGateHQ", setup) || setup != "1") return super.AICF_VSSelectFaction();
		if (m_VSFaction || m_VSState) return false;
		array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
		float best = 300;
		foreach (SCR_CampaignFaction faction : factions)
		{
			if (!faction || !faction.GetMainBase() || !faction.GetMainBase().GetSpawnPoint()) continue;
			m_vGEPScanOrigin = faction.GetMainBase().GetSpawnPoint().GetOrigin();
			m_GEPClosestGate = null; m_fGEPClosestGate = 300;
			GetGame().GetWorld().QueryEntitiesBySphere(m_vGEPScanOrigin, 300, AICF_GEPCollectGate, null, EQueryEntitiesFlags.ALL);
			if (!m_GEPClosestGate || m_fGEPClosestGate >= best) continue;
			best = m_fGEPClosestGate;
			m_VSFaction = faction; m_sVSFaction = faction.GetFactionKey();
			AICF_NativeGateExpiryProbe.s_HQGate = m_GEPClosestGate;
		}
		if (!m_VSFaction) return false;
		Print(string.Format("[AICF][NATIVE_GATE_EXPIRY_SETUP] event=HQ_SELECTED faction=%1 hq_position=%2 gate=%3 gate_position=%4 distance_m=%5 actual_tag=OpenGate selection=NATIVE_HQ_GATE_NO_COORDINATES",
			m_sVSFaction, m_VSFaction.GetMainBase().GetSpawnPoint().GetOrigin(), AICF_GroupTrace.Entity(AICF_NativeGateExpiryProbe.s_HQGate), AICF_NativeGateExpiryProbe.s_HQGate.GetOrigin(), best));
		return true;
	}

	override protected bool AICF_VSAlternativeScenario(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		if (!AICF_NativeGateExpiryProbe.Enabled()) return super.AICF_VSAlternativeScenario(slot, trip);
		int now = System.GetTickCount();
		if (AICF_NativeGateExpiryProbe.s_bDoorCloseRequested && !m_bGEPClosedObserved && AICF_NativeGateExpiryProbe.s_SetupGate)
		{
			BaseDoorComponent door = BaseDoorComponent.Cast(AICF_NativeGateExpiryProbe.s_SetupGate.FindComponent(BaseDoorComponent));
			if (door && Math.AbsFloat(door.GetNormalizedDoorState()) <= 0.01 && Math.AbsFloat(door.GetControlValue()) <= 0.01)
			{
				m_bGEPClosedObserved = true;
				Print(string.Format("[AICF][NATIVE_GATE_EXPIRY_SETUP] event=CLOSED_READBACK gate=%1 state=%2 control=%3 actual_native_state=1",
					AICF_GroupTrace.Entity(AICF_NativeGateExpiryProbe.s_SetupGate), door.GetNormalizedDoorState(), door.GetControlValue()));
			}
		}
		if (!m_iGEPStarted) m_iGEPStarted = now;
		if (!AICF_NativeGateExpiryProbe.s_Trip && m_iVSStep < 2) return false;
		if (now - m_iGEPStarted > 360000)
		{
			AICF_VSCheck("GEP_TIMEOUT_ACTUAL_GATE_REQUIRED", false); AICF_VSFinish(); return true;
		}
		if (!trip || trip.IsTerminal() || !AICF_VSRetainedCurrent(slot, trip))
		{
			AICF_VSCheck("GEP_ORIGINAL_IDENTITY_CURRENT", false); AICF_VSFinish(); return true;
		}
		if (!AICF_NativeGateExpiryProbe.s_Trip)
		{
			AICF_NativeGateExpiryProbe.s_Trip = trip;
			AICF_NativeGateExpiryProbe.s_sOperation = trip.GetOperationId();
			AICF_NativeGateExpiryProbe.s_VehicleId = m_VSVehicleId;
			AICF_NativeGateExpiryProbe.s_sRpl = m_sVSRetainedRpl;
			AICF_NativeGateExpiryProbe.Log("START", string.Format("vehicle=%1 vehicle_rpl=%2 trip_deadline_ms=%3 original_roster=1 one_purchase=1 goal=%4 gate_node_rpl=%5",
				AICF_GroupTrace.Entity(m_VSVehicle), m_sVSRetainedRpl, trip.GetAbsoluteDeadlineMs(), m_vVSGoal, Replication.FindItemId(m_VSVehicle).ToString()));
		}
		if (trip != AICF_NativeGateExpiryProbe.s_Trip || m_VSVehicleId != AICF_NativeGateExpiryProbe.s_VehicleId ||
			m_sVSRetainedRpl != AICF_NativeGateExpiryProbe.s_sRpl || trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT)
		{
			AICF_VSCheck("GEP_SAME_TRIP_TRANSIT", false); AICF_VSFinish(); return true;
		}
		if (AICF_NativeGateExpiryProbe.s_iInjectedAt <= 0) return true;
		AICF_VehicleMovementState state = trip.GetMovementState();
		if (!m_iGEPRepairAt)
		{
			if (state.GetRouteGeneration() <= AICF_NativeGateExpiryProbe.s_iRoute || !state.IsRouteWaypointBound()) return true;
			bool bounded = state.GetMobilityRecoveryAttempts() == AICF_NativeGateExpiryProbe.s_iAttempts + 1 &&
				state.GetMaximumMobilityRecoveryAttempts() == AICF_NativeGateExpiryProbe.s_iMaximum &&
				state.GetMaximumMobilityRecoveryAttempts() <= 2 && trip.GetAbsoluteDeadlineMs() == AICF_NativeGateExpiryProbe.s_iDeadline;
			AICF_VSCheck("GEP_ONE_BOUNDED_ROUTE_REPAIR", bounded);
			if (!bounded) { AICF_VSFinish(); return true; }
			m_iGEPRepairAt = now;
			m_vGEPRepairPosition = m_VSVehicle.GetOrigin();
			string repairDetails = string.Format("vehicle=%1 vehicle_rpl=%2 route_generation=%3 attempts=%4 maximum=%5 trip_deadline_ms=%6 delay_ms=%7 position=%8 observed_at_ms=%9",
				AICF_GroupTrace.Entity(m_VSVehicle), m_sVSRetainedRpl, state.GetRouteGeneration(), state.GetMobilityRecoveryAttempts(),
				state.GetMaximumMobilityRecoveryAttempts(), trip.GetAbsoluteDeadlineMs(), now - AICF_NativeGateExpiryProbe.s_iInjectedAt, m_vGEPRepairPosition, now);
			repairDetails += " gate_node_rpl=" + Replication.FindItemId(m_VSVehicle).ToString();
			AICF_NativeGateExpiryProbe.Log("REPAIR", repairDetails);
			return true;
		}
		if (now < m_iGEPNextPoll) return true;
		m_iGEPNextPoll = now + 1000;
		Physics physics = m_VSVehicle.GetPhysics();
		if (!physics || !physics.IsDynamic()) return true;
		bool exactCrew = AICF_VSExact(slot);
		if (exactCrew && physics && physics.GetVelocity().Length() >= 0.5) m_iGEPMovingPolls++;
		float distance = vector.DistanceXZ(m_vGEPRepairPosition, m_VSVehicle.GetOrigin());
		if (distance < 25 || m_iGEPMovingPolls < 3) return true;
		bool final = exactCrew && AICF_VSRetainedCurrent(slot, trip) &&
			state.GetMobilityRecoveryAttempts() == AICF_NativeGateExpiryProbe.s_iAttempts + 1 &&
			trip.GetAbsoluteDeadlineMs() == AICF_NativeGateExpiryProbe.s_iDeadline;
		AICF_VSCheck("GEP_NATIVE_25M_THREE_POLLS_SAME_ASSET", final);
		if (final) AICF_VSRetainedBoarding(slot, trip);
		string details = string.Format("vehicle=%1 vehicle_rpl=%2 displacement_m=%3 moving_polls=%4 attempts=%5 maximum=%6 trip_deadline_ms=%7 original_roster=%8 one_purchase=%9",
			AICF_GroupTrace.Entity(m_VSVehicle), m_sVSRetainedRpl, distance, m_iGEPMovingPolls, state.GetMobilityRecoveryAttempts(),
			state.GetMaximumMobilityRecoveryAttempts(), trip.GetAbsoluteDeadlineMs(), final, m_VSVehicles.GetSpawnedCount(m_VSFaction.GetFactionKey()) == 1);
		details += string.Format(" position=%1 speed_mps=%2 exact_crew=%3 observed_at_ms=%4 gate_node_rpl=%5", m_VSVehicle.GetOrigin(), physics.GetVelocity().Length(), exactCrew, now, Replication.FindItemId(m_VSVehicle).ToString());
		AICF_NativeGateExpiryProbe.Log("MOVED", details);
		AICF_VSFinish();
		return true;
	}
}
