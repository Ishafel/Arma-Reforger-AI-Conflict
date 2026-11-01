// Контролируемый отказ на настоящем current vehicle handler. Не natural STUCK
// и не proof обычной войны; native движение после recovery проверяется отдельно.
class AICF_TransitFailureRecoveryNode : SCR_AIProcessFailedMovementResult
{
	bool Inject(SCR_AIGroup group, int result, int handler, bool related, vector location)
	{
		m_Group = group;
		m_GroupUtilityComponent = group.GetGroupUtilityComponent();
		return AICF_HandleTransitFailedMovement(result, handler, related, location);
	}
}

modded class AICF_MatchController
{
	protected ref AICF_TransitFailureRecoveryNode m_TFRNode;
	protected ref AICF_VehicleWatchdog m_TFRWatchdog = new AICF_VehicleWatchdog();
	protected ref AICF_TransitMoveFailure m_TFRPreviousReceipt;
	protected vector m_vTFRPreviousFailureLocation;
	protected AICF_TransportTrip m_TFRTrip;
	protected Vehicle m_TFRVehicle;
	protected EntityID m_TFRVehicleId;
	protected int m_iTFRStarted;
	protected int m_iTFRInjectedAt;
	protected int m_iTFRCallbacks;
	protected int m_iTFRStep;
	protected int m_iTFRRoute;
	protected int m_iTFRDeadline;
	protected int m_iTFRMovingPolls;
	protected vector m_vTFRAfterRepair;
	protected bool m_bTFRRepairObserved;

	protected void AICF_TFRCallback(int result, IEntity vehicle, bool related, vector location)
	{
		m_iTFRCallbacks++;
	}

	protected void AICF_TFRLog(string eventName, string details, int decisionAtMs)
	{
		Print(string.Format("[AICF][TRANSIT_FAILURE_RECOVERY_PROBE] event=%1 t_ms=%2 classification=CONTROLLED_FAILURE_RECOVERY_ONLY natural_failure_proof=0 %3",
			eventName, decisionAtMs, details));
	}

	override protected bool AICF_VSAlternativeScenario(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		int now = System.GetTickCount();
		if (!m_iTFRStarted) m_iTFRStarted = now;
		// Обычный scaffold сам завершает initial boarding/30s hold и выдаёт
		// первый настоящий distant order; probe не блокирует эту подготовку.
		if (!m_TFRTrip && m_iVSStep < 2) return false;
		if (now - m_iTFRStarted > 300000)
		{
			AICF_VSCheck("TFR_TIMEOUT", false); AICF_VSFinish(); return true;
		}
		if (!trip || trip.IsTerminal())
		{
			AICF_VSCheck("TFR_UNEXPECTED_TERMINAL", false); AICF_VSFinish(); return true;
		}
		if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetLease() ||
			!trip.GetLease().HasPhysicalAsset() || !trip.GetMovementState().IsRouteWaypointBound()) return true;
		AICF_VehicleMovementState state = trip.GetMovementState();
		// Не вмешиваемся в настоящее открытие ворот/возврат crew. Такой отказ
		// принадлежит другому owner, и production Capture обязан его отклонить.
		if (state.GetNativeGate() || state.GetCrewRecoveryToken() || state.IsCrewRecoveryRoutePending() ||
			state.GetNativeCrewEntry(EAICompartmentType.Pilot) || state.GetNativeCrewEntry(EAICompartmentType.Turret)) return true;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		SCR_AIGroupUtilityComponent utility = m_VSGroup.GetGroupUtilityComponent();
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
		if (!move || utility.GetExecutedAction() != move || move.GetActionState() != EAIActionState.RUNNING ||
			move.m_RelatedWaypoint != state.GetRouteWaypoint() ||
			!m_TFRWatchdog.AreAllAliveMembersSettledInVehicle(m_VSGroup, vehicle)) return true;
		// До fault доказываем выход с исходной площадки настоящим native движением.
		if (!m_TFRTrip && vector.DistanceXZ(vehicle.GetOrigin(), m_vVSStart) < 25) return true;
		if (!m_TFRTrip)
		{
			m_TFRTrip = trip;
			m_TFRVehicle = vehicle;
			m_TFRVehicleId = vehicle.GetID();
			m_iTFRDeadline = trip.GetAbsoluteDeadlineMs();
			m_TFRNode = new AICF_TransitFailureRecoveryNode();
			AICF_VSCheck("TFR_INITIAL_BUDGET_TWO", state.GetMaximumMobilityRecoveryAttempts() == 2 && state.GetMobilityRecoveryAttempts() == 0);
			AICF_TFRLog("START", string.Format("operation_id=%1 vehicle=%2 route_generation=%3 deadline=%4",
				trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), state.GetRouteGeneration(), m_iTFRDeadline), now);
		}
		if (m_TFRTrip != trip || m_TFRVehicle != vehicle || vehicle.GetID() != m_TFRVehicleId || !AICF_VSOwnRecoveryRoster(slot))
		{
			AICF_VSCheck("TFR_IDENTITY_CURRENT", false); AICF_VSFinish(); return true;
		}
		if (m_iTFRInjectedAt > 0)
		{
			if (!m_bTFRRepairObserved)
			{
				if (state.GetRouteGeneration() <= m_iTFRRoute) return true;
				AICF_VSCheck("TFR_NEXT_TICK_BOUNDED_RECOVERY", now > m_iTFRInjectedAt && now - m_iTFRInjectedAt < 10000 &&
					state.GetMobilityRecoveryAttempts() == m_iTFRStep && state.GetMaximumMobilityRecoveryAttempts() == 2 &&
					trip.GetAbsoluteDeadlineMs() == m_iTFRDeadline && !state.GetTransitMoveFailure());
				m_bTFRRepairObserved = true;
				m_vTFRAfterRepair = vehicle.GetOrigin();
				AICF_TFRLog("REPAIR", string.Format("step=%1 operation_id=%2 vehicle=%3 route_generation=%4 attempts=%5 maximum=2 delay_ms=%6 deadline=%7 position=%8",
					m_iTFRStep, trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), state.GetRouteGeneration(), state.GetMobilityRecoveryAttempts(), now - m_iTFRInjectedAt, trip.GetAbsoluteDeadlineMs(), vehicle.GetOrigin()), now);
				return true;
			}
			Physics physics = vehicle.GetPhysics();
			if (physics && physics.GetVelocity().Length() >= 0.5) m_iTFRMovingPolls++;
			float displacement = vector.DistanceXZ(m_vTFRAfterRepair, vehicle.GetOrigin());
			if (displacement < 25 || m_iTFRMovingPolls < 3) return true;
			AICF_VSCheck("TFR_REAL_MOVEMENT_AFTER_REPAIR", displacement >= 25 && m_iTFRMovingPolls >= 3);
			AICF_TFRLog("MOVED", string.Format("step=%1 operation_id=%2 vehicle=%3 displacement_m=%4 moving_polls=%5 attempts=%6 deadline=%7",
				m_iTFRStep, trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), displacement, m_iTFRMovingPolls, state.GetMobilityRecoveryAttempts(), trip.GetAbsoluteDeadlineMs()), now);
			m_iTFRInjectedAt = 0;
			m_iTFRMovingPolls = 0;
			m_bTFRRepairObserved = false;
		}
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager) return true;
		array<BaseCompartmentSlot> seats = {};
		manager.GetCompartments(seats);
		ChimeraCharacter driver;
		foreach (BaseCompartmentSlot seat : seats)
			if (PilotCompartmentSlot.Cast(seat)) driver = ChimeraCharacter.Cast(seat.GetOccupant());
		if (!driver || !driver.GetAIControlComponent()) return true;
		AIAgent agent = driver.GetAIControlComponent().GetControlAIAgent();
		AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(m_VSGroup.FindComponent(AIGroupMovementComponent));
		if (!agent || !movement) return true;
		int handler = movement.GetAgentMoveHandlerId(agent);
		if (handler == AIGroupMovementComponent.DEFAULT_HANDLER_ID) return true;
		// Первый selected Move ещё может предшествовать подключению Pilot
		// к native vehicle subgroup. Fault вводится только в реальный car handler.
		SCR_AIGroupVehicle registered = utility.m_VehicleMgr.FindVehicle(vehicle);
		AICarMovementComponent car = AICarMovementComponent.Cast(agent.GetMovementComponent());
		if (!registered || registered.GetSubgroupHandleId() != handler ||
			utility.m_VehicleMgr.FindVehicleBySubgroupId(handler) != registered ||
			!car || !car.GetAIAgent() || car.GetAIAgent().GetControlledEntity() != vehicle) return true;
		vector location = state.GetRouteEndpoint();
		if (m_iTFRStep == 1 && m_TFRPreviousReceipt)
			AICF_VSCheck("TFR_NEW_ACTION_SAME_LOCATION_NOT_DUPLICATE", !m_TFRPreviousReceipt.CanDiscardRetiredNativeFailure(
				m_VSGroup, utility, EMoveError.STUCK, handler, true, m_vTFRPreviousFailureLocation));
		if (m_iTFRStep == 2)
		{
			AICF_VSCheck("TFR_EXHAUSTED_PRESERVES_STOCK", !m_TFRNode.Inject(m_VSGroup, EMoveError.STUCK, handler, true, location) && !state.GetTransitMoveFailure());
			AICF_TFRLog("DONE", string.Format("operation_id=%1 vehicle=%2 attempts=2 maximum=2 callbacks=%3 original_roster=1 deadline=%4",
				trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), m_iTFRCallbacks, trip.GetAbsoluteDeadlineMs()), now);
			AICF_VSFinish(); return true;
		}
		AICF_VSCheck("TFR_UNRELATED_REJECTED", !m_TFRNode.Inject(m_VSGroup, EMoveError.STUCK, handler, false, location));
		AICF_VSCheck("TFR_DEFAULT_HANDLER_REJECTED", !m_TFRNode.Inject(m_VSGroup, EMoveError.STUCK, AIGroupMovementComponent.DEFAULT_HANDLER_ID, true, location));
		AICF_VSCheck("TFR_MINUS_ONE_HANDLER_REJECTED", !m_TFRNode.Inject(m_VSGroup, EMoveError.STUCK, -1, true, location));
		AICF_VSCheck("TFR_MINUS_TWO_HANDLER_REJECTED", !m_TFRNode.Inject(m_VSGroup, EMoveError.STUCK, -2, true, location));
		AICF_VSCheck("TFR_FOREIGN_HANDLER_REJECTED", !m_TFRNode.Inject(m_VSGroup, EMoveError.STUCK, 999999, true, location));
		AICF_VSCheck("TFR_STOPPED_PRESERVES_STOCK", !m_TFRNode.Inject(m_VSGroup, EMoveError.STOPPED, handler, true, location));
		int result = EMoveError.STUCK;
		if (m_iTFRStep == 1) result = EMoveError.UNKNOWN;
		int before = m_iTFRCallbacks;
		utility.GetOnMoveFailed().Insert(AICF_TFRCallback);
		bool accepted = m_TFRNode.Inject(m_VSGroup, result, handler, true, location);
		bool repeated = m_TFRNode.Inject(m_VSGroup, result, handler, true, location);
		utility.GetOnMoveFailed().Remove(AICF_TFRCallback);
		m_TFRPreviousReceipt = state.GetTransitMoveFailure();
		m_vTFRPreviousFailureLocation = location;
		m_iTFRStep++;
		m_iTFRRoute = state.GetRouteGeneration();
		m_iTFRInjectedAt = now;
		AICF_VSCheck("TFR_EXACT_NATIVE_FAILURE_CAPTURED", accepted && repeated && m_iTFRCallbacks == before + 1 &&
			state.GetTransitMoveFailure() && state.GetTransitMoveFailure().WasDelivered() && state.GetMobilityRecoveryAttempts() == m_iTFRStep - 1);
		AICF_TFRLog("INJECTED", string.Format("step=%1 operation_id=%2 vehicle=%3 native_result=%4 handler=%5 accepted=%6 repeated=%7 callbacks=%8 route_generation=%9",
			m_iTFRStep, trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), typename.EnumToString(EMoveError, result), handler, accepted, repeated, m_iTFRCallbacks, state.GetRouteGeneration()), now);
		if (!accepted || !repeated) AICF_VSFinish();
		return true;
	}
}
