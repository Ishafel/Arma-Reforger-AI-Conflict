// Thin production facade: composition root, faction/slot scheduler and aggregate
// query boundary. Domain side effects remain owned by the dedicated flows.
class AICF_VehicleCoordinator
{
	protected ref AICF_Stage3Config m_Config;
	protected SCR_GameModeCampaign m_Campaign;
	protected AICF_ConflictAdapter m_ConflictAdapter;
	protected ref AICF_OrderPlanner m_OrderPlanner;
	protected ref AICF_ObjectiveGraph m_ObjectiveGraph;
	protected ref AICF_TargetSelector m_TargetSelector;
	protected ref AICF_TransportTripRegistry m_Trips;
	protected ref AICF_FleetRegistry m_Fleets;
	protected ref AICF_TransportTripController m_TripController;
	protected ref AICF_VehicleCleanupManager m_CleanupManager;
	protected ref AICF_VehicleDomainDiagnostics m_Diagnostics;
	protected ref AICF_VehicleAcceptanceMonitor m_Acceptance;
	protected ref array<ref AICF_VehicleAdmissionAudit> m_aAdmissionAudits = {};
	protected int m_iObservedBaseRevision;
	protected bool m_bStopped;

	void AICF_VehicleCoordinator(
		AICF_Stage3Config config,
		SCR_GameModeCampaign campaign,
		AICF_ConflictAdapter conflictAdapter,
		AICF_OrderPlanner orderPlanner,
		AICF_GroupCohesionPolicy cohesionPolicy,
		AICF_ObjectiveGraph objectiveGraph,
		AICF_TargetSelector targetSelector)
	{
		m_Config = config;
		m_Campaign = campaign;
		m_ConflictAdapter = conflictAdapter;
		m_OrderPlanner = orderPlanner;
		m_ObjectiveGraph = objectiveGraph;
		m_TargetSelector = targetSelector;
		m_Trips = new AICF_TransportTripRegistry();
		m_Fleets = new AICF_FleetRegistry();
		m_TripController = new AICF_TransportTripController(
			config,
			campaign,
			conflictAdapter,
			cohesionPolicy,
			orderPlanner,
			objectiveGraph,
			targetSelector);
		m_CleanupManager = new AICF_VehicleCleanupManager(config, campaign);
		m_TripController.SetCleanupManager(m_CleanupManager);
		m_Diagnostics = new AICF_VehicleDomainDiagnostics();
		m_Acceptance = new AICF_VehicleAcceptanceMonitor(config, 2);
		m_CleanupManager.SetAcceptanceMonitor(m_Acceptance);
	}

	void Update(
		AICF_FactionState usState,
		SCR_CampaignFaction usFaction,
		AICF_FactionState ussrState,
		SCR_CampaignFaction ussrFaction,
		int baseRevision,
		bool dispatchTrips)
	{
		if (!IsAuthorityReady())
			return;
		m_iObservedBaseRevision = Math.Max(0, baseRevision);
		EnsureFactionFleet(usState, usFaction);
		EnsureFactionFleet(ussrState, ussrFaction);
		m_Acceptance.SyncFactionConfiguration(usState);
		m_Acceptance.SyncFactionConfiguration(ussrState);
		if (dispatchTrips)
		{
			ProcessFaction(usState, usFaction);
			ProcessFaction(ussrState, ussrFaction);
		}
		int nowMs = System.GetTickCount();
		for (int fleetIndex; fleetIndex < m_Fleets.GetFleetCount(); fleetIndex++)
			m_CleanupManager.UpdateFleet(m_Fleets.GetFleet(fleetIndex), nowMs);
		m_Acceptance.Audit();
	}

	void NotifyStrategicContextChanged(int baseRevision, string reason)
	{
		m_iObservedBaseRevision = Math.Max(m_iObservedBaseRevision, baseRevision);
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_REQUEST_CONTEXT_REVISION",
			string.Format(
				"base_revision=%1 reason=%2 owner=STRATEGIC_PLANNING",
				m_iObservedBaseRevision,
				reason));
	}

	bool IsControllingMovement(AICF_GroupSlot slot)
	{
		AICF_VehicleSlotView view = GetSlotView(slot);
		return view && view.IsControllingMovement();
	}

	bool IsRestorePending(AICF_GroupSlot slot)
	{
		AICF_VehicleSlotView view = GetSlotView(slot);
		return view && view.IsRestorePending();
	}

	AICF_VehicleSlotView GetSlotView(AICF_GroupSlot slot)
	{
		AICF_TransportTrip trip = FindTripForSlot(slot);
		if (!trip)
			return null;
		return new AICF_VehicleSlotView(trip);
	}

	// User-facing mobility is a physical projection first and a lifecycle
	// projection second. In particular, retained fail-closed cleanup may outlive
	// the Trip record while living members remain linked to the vehicle.
	string GetSlotDisplayStatusText(AICF_GroupSlot slot)
	{
		if (!slot)
			return "Пешком";
		SCR_AIGroup group = slot.GetGroup();
		int alive = AICF_GroupRuntime.CountAliveAgents(group);
		int inVehicle = AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(group);
		AICF_VehicleSlotView view = GetSlotView(slot);
		if (inVehicle > 0)
		{
			string phase;
			if (view)
				phase = view.GetPhase();
			if (phase == "BOARDING")
				return string.Format("Посадка %1/%2", inVehicle, alive);
			if (phase == "DISMOUNT")
				return string.Format("Высадка, в машине %1/%2", inVehicle, alive);
			if (phase == "TRANSIT")
				return string.Format("Движение на технике %1/%2", inVehicle, alive);
			return string.Format("В технике %1/%2", inVehicle, alive);
		}
		if (view)
			return view.GetStatusText();
		return "Пешком";
	}

	AICF_TransportTrip GetTrip(AICF_GroupSlot slot)
	{
		return FindTripForSlot(slot);
	}

	bool ReplanControlledMovement(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason,
		int minimumDwellMs,
		int stableCandidateMs,
		int baseRevision)
	{
		if (!slot || !faction || !IsControllingMovement(slot))
			return false;
		AICF_TransportTrip trip = FindTripForSlot(slot);
		if (!trip || trip.GetFactionKey() != faction.GetFactionKey())
			return false;
		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		bool reconciled = m_OrderPlanner.ReconcileStrategicOrder(
			slot,
			faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			reason,
			minimumDwellMs,
			stableCandidateMs);
		if (!reconciled)
			return false;
		if (!AdoptCurrentStrategicAssignment(slot, faction, reason, baseRevision))
			return false;
		return oldTarget && oldTarget != slot.GetTargetBase();
	}

	// Planning may already have committed a specialized QRF/loss-response
	// assignment. This boundary snapshots that planning truth and lets the Trip
	// controller perform SAFE_REUSE plus the sole vehicle-order handoff.
	bool AdoptCurrentStrategicAssignment(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason,
		int baseRevision)
	{
		if (!slot || !faction || !IsControllingMovement(slot))
			return false;
		AICF_TransportTrip trip = FindTripForSlot(slot);
		if (!trip || trip.GetFactionKey() != faction.GetFactionKey())
			return false;
		AICF_StrategicAssignmentSnapshot assignment;
		if (!m_OrderPlanner.TryCreateAssignmentSnapshot(
			slot,
			faction,
			Math.Max(0, baseRevision),
			assignment))
		{
			return false;
		}
		AICF_FactionFleet fleet = m_Fleets.GetOrCreateFleet(
			faction.GetFactionKey(),
			m_Config.GetMaxVehiclesPerFaction());
		AICF_ETransportTripPhase previousPhase = trip.GetPhase();
		int previousTransitions = trip.GetTransitionCount();
		AICF_TripOutcome observed = m_TripController.Tick(
			trip,
			assignment,
			slot,
			fleet,
			faction);
		ObserveTerminalCommit(trip, previousPhase, previousTransitions, observed);
		return trip.GetAssignment().GetAssignmentRevision() ==
			assignment.GetAssignmentRevision() &&
			trip.GetAssignment().GetBaseRevision() == assignment.GetBaseRevision();
	}

	int GetReservedCount(FactionKey factionKey)
	{
		AICF_FactionFleet fleet = m_Fleets.FindFleet(factionKey);
		if (!fleet)
			return 0;
		return fleet.GetReservedCount();
	}

	int GetCapHeldCount(FactionKey factionKey)
	{
		AICF_FactionFleet fleet = m_Fleets.FindFleet(factionKey);
		if (!fleet)
			return 0;
		return fleet.GetActiveOrReservedCount();
	}

	int GetReleasePendingCount(FactionKey factionKey)
	{
		AICF_FactionFleet fleet = m_Fleets.FindFleet(factionKey);
		if (!fleet)
			return 0;
		return fleet.GetReleasePendingCount();
	}

	int GetFailedClosedCount(FactionKey factionKey)
	{
		AICF_FactionFleet fleet = m_Fleets.FindFleet(factionKey);
		if (!fleet)
			return 0;
		return fleet.GetFailedClosedCount();
	}

	int GetRetainedPhysicalCount(FactionKey factionKey)
	{
		if (!m_CleanupManager)
			return 0;
		return m_CleanupManager.GetRetainedPhysicalCount(factionKey);
	}

	int GetSpawnedCount(FactionKey factionKey)
	{
		AICF_FactionFleet fleet = m_Fleets.FindFleet(factionKey);
		if (!fleet)
			return 0;
		return fleet.GetActiveCount();
	}

	int GetWorldPoolCount(FactionKey factionKey)
	{
		AICF_FactionFleet fleet = m_Fleets.FindFleet(factionKey);
		if (!fleet)
			return 0;
		return fleet.GetWorldPoolCount();
	}

	void Heartbeat(
		int managedGroups,
		int managedAgents,
		int managedWaypoints,
		int trackedEntities)
	{
		int active;
		int reserved;
		int releasePending;
		int failedClosed;
		int capHeld;
		int pool;
		int retainedPhysical;
		for (int fleetIndex; fleetIndex < m_Fleets.GetFleetCount(); fleetIndex++)
		{
			AICF_FactionFleet fleet = m_Fleets.GetFleet(fleetIndex);
			if (!fleet)
				continue;
			active += fleet.GetActiveCount();
			reserved += fleet.GetReservedCount();
			releasePending += fleet.GetReleasePendingCount();
			failedClosed += fleet.GetFailedClosedCount();
			capHeld += fleet.GetActiveOrReservedCount();
			pool += fleet.GetWorldPoolCount();
			if (m_CleanupManager)
				retainedPhysical += m_CleanupManager.GetRetainedPhysicalCount(fleet.GetFactionKey());
		}
		string heartbeatDetails = string.Format(
			"trips=%1 active=%2 reserved=%3 release_pending=%4 failed_closed=%5 cap_held=%6",
			m_Trips.GetCount(),
			active,
			reserved,
			releasePending,
			failedClosed,
			capHeld);
		heartbeatDetails += string.Format(
			" world_pool=%1 retained_physical=%2 managed_agents=%3",
			pool,
			retainedPhysical,
			managedAgents);
		AICF_Stage3Diagnostics.Info("HEARTBEAT", heartbeatDetails);
		m_Diagnostics.FleetHeartbeat(
			m_Fleets,
			managedGroups,
			managedAgents,
			managedWaypoints,
			trackedEntities,
			retainedPhysical);
		m_Acceptance.Audit();
	}

	void Stop(bool cleanupEntities)
	{
		StopWithFactionContexts(cleanupEntities, null, null, null, null);
	}

	void StopWithFactionContexts(
		bool cleanupEntities,
		AICF_FactionState firstState,
		SCR_CampaignFaction firstFaction,
		AICF_FactionState secondState,
		SCR_CampaignFaction secondFaction)
	{
		if (m_bStopped)
			return;
		m_bStopped = true;
		int nowMs = System.GetTickCount();
		for (int tripIndex; tripIndex < m_Trips.GetCount(); tripIndex++)
		{
			AICF_TransportTrip trip = m_Trips.Get(tripIndex);
			if (!trip)
				continue;
			AICF_FactionFleet fleet = m_Fleets.FindFleet(trip.GetFactionKey());
			AICF_GroupSlot slot;
			SCR_CampaignFaction faction;
			ResolveStopContext(
				trip,
				firstState,
				firstFaction,
				secondState,
				secondFaction,
				slot,
				faction);
			AICF_ETransportTripPhase previousPhase = trip.GetPhase();
			int previousTransitions = trip.GetTransitionCount();
			AICF_TripOutcome stopOutcome = m_TripController.AbortForStop(
				trip,
				slot,
				fleet,
				faction,
				!cleanupEntities,
				"COORDINATOR_STOP");
			ObserveTerminalCommit(trip, previousPhase, previousTransitions, stopOutcome);
			if (cleanupEntities && m_CleanupManager && fleet && trip.GetLease())
				m_CleanupManager.BeginStopLease(trip, fleet, trip.GetLease(), nowMs);
		}
		if (cleanupEntities && m_CleanupManager)
		{
			for (int fleetIndex; fleetIndex < m_Fleets.GetFleetCount(); fleetIndex++)
				m_CleanupManager.BeginStopFleet(m_Fleets.GetFleet(fleetIndex), nowMs);
		}
		m_Acceptance.Stop("COORDINATOR_STOP");
	}

	protected void ResolveStopContext(
		AICF_TransportTrip trip,
		AICF_FactionState firstState,
		SCR_CampaignFaction firstFaction,
		AICF_FactionState secondState,
		SCR_CampaignFaction secondFaction,
		out AICF_GroupSlot slot,
		out SCR_CampaignFaction faction)
	{
		slot = null;
		faction = null;
		AICF_FactionState state;
		if (firstFaction && firstFaction.GetFactionKey() == trip.GetFactionKey())
		{
			state = firstState;
			faction = firstFaction;
		}
		else if (secondFaction && secondFaction.GetFactionKey() == trip.GetFactionKey())
		{
			state = secondState;
			faction = secondFaction;
		}
		if (!state)
			return;
		AICF_GroupSlot candidate = state.GetSlot(trip.GetSlotId());
		if (!candidate || candidate.GetSpawnGeneration() != trip.GetGroupGeneration() ||
			candidate.GetGroup() != trip.GetAssignment().GetGroup())
		{
			faction = null;
			return;
		}
		slot = candidate;
	}

	protected AICF_FactionFleet EnsureFactionFleet(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction)
	{
		if (!factionState || !faction)
			return null;
		m_Acceptance.RegisterFaction(faction.GetFactionKey());
		return m_Fleets.GetOrCreateFleet(
			faction.GetFactionKey(),
			m_Config.GetMaxVehiclesPerFaction());
	}

	protected void ProcessFaction(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction)
	{
		if (!factionState || !faction)
			return;
		FactionKey factionKey = faction.GetFactionKey();
		AICF_FactionFleet fleet = EnsureFactionFleet(factionState, faction);
		if (!fleet)
			return;

		for (int slotId; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			ProcessSlot(slot, faction, fleet);
		}
	}

	protected void ProcessSlot(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_FactionFleet fleet)
	{
		if (!slot)
			return;
		AICF_TransportTrip trip = m_Trips.Find(faction.GetFactionKey(), slot.GetSlotId());
		if (trip && trip.GetAssignment() &&
			trip.GetAssignment().GetUnitType() != slot.GetUnitType())
		{
			AICF_ETransportTripPhase changedTypePhase = trip.GetPhase();
			int changedTypeTransitions = trip.GetTransitionCount();
			AICF_TripOutcome changedTypeOutcome = m_TripController.TerminateStaleTrip(
				trip,
				fleet,
				"COMMANDER_UNIT_TYPE_CHANGED");
			ObserveTerminalCommit(
				trip,
				changedTypePhase,
				changedTypeTransitions,
				changedTypeOutcome);
			if (!m_TripController.CanRetireTrip(trip))
				return;
			m_Trips.RemoveTerminal(trip);
			trip = null;
		}
		AICF_StrategicAssignmentSnapshot assignment;
		if (!ResolveAssignment(slot, faction, trip, assignment))
		{
			if (trip)
			{
				AICF_ETransportTripPhase unavailablePhase = trip.GetPhase();
				int unavailableTransitions = trip.GetTransitionCount();
				AICF_TripOutcome unavailableOutcome = m_TripController.TerminateStaleTrip(
					trip,
					fleet,
					"ASSIGNMENT_SNAPSHOT_UNAVAILABLE");
				ObserveTerminalCommit(
					trip,
					unavailablePhase,
					unavailableTransitions,
					unavailableOutcome);
			}
			return;
		}

		if (trip && !trip.IsCurrent(assignment))
		{
			AICF_ETransportTripPhase stalePhase = trip.GetPhase();
			int staleTransitions = trip.GetTransitionCount();
			AICF_TripOutcome staleOutcome = m_TripController.TerminateStaleTrip(
				trip,
				fleet,
				"GROUP_GENERATION_REPLACED");
			ObserveTerminalCommit(
				trip,
				stalePhase,
				staleTransitions,
				staleOutcome);
			if (!m_TripController.CanRetireTrip(trip))
				return;
			m_Trips.RemoveTerminal(trip);
			trip = null;
		}
		if (trip)
		{
			AICF_ETransportTripPhase activePhase = trip.GetPhase();
			string orderOwnershipFence = GetInfantryOrderAdmissionFenceReason(slot);
			if (!orderOwnershipFence.IsEmpty() &&
				(activePhase == AICF_ETransportTripPhase.WAITING_FOR_SITE ||
				activePhase == AICF_ETransportTripPhase.SITE_PLANNED ||
				activePhase == AICF_ETransportTripPhase.APPROACHING_SITE ||
				activePhase == AICF_ETransportTripPhase.STAGING_CONFIRMED ||
				activePhase == AICF_ETransportTripPhase.SPAWN_COMMIT))
			{
				// StartBoarding suspends the infantry waypoint. Keep an admitted but
				// pre-boarding trip inert while infantry reliability owns either its
				// durability candidate or its bounded route-replan hold.
				ReportAdmissionOnce(assignment, orderOwnershipFence);
				return;
			}
			AICF_ETransportTripPhase previousPhase = trip.GetPhase();
			int previousTransitions = trip.GetTransitionCount();
			AICF_TripOutcome observed = m_TripController.Tick(
				trip,
				assignment,
				slot,
				fleet,
				faction);
			ObserveTerminalCommit(trip, previousPhase, previousTransitions, observed);
			if (!ShouldReplaceTerminalTrip(trip, assignment))
				return;
			m_Trips.RemoveTerminal(trip);
		}

		string ineligibleReason;
		if (!IsAdmissionEligible(slot, assignment, fleet, ineligibleReason))
		{
			ReportAdmissionOnce(assignment, ineligibleReason);
			return;
		}
		int nowMs = System.GetTickCount();
		AICF_TransportTrip admitted = m_Trips.Create(
			assignment,
			nowMs,
			CalculateTripDeadlineMs(nowMs),
			"STRATEGIC_ASSIGNMENT_ADMITTED");
		if (!admitted)
			return;
		AICF_ETransportTripPhase admittedPhase = admitted.GetPhase();
		int admittedTransitions = admitted.GetTransitionCount();
		AICF_TripOutcome admittedOutcome = m_TripController.Tick(
			admitted,
			assignment,
			slot,
			fleet,
			faction);
		ObserveTerminalCommit(
			admitted,
			admittedPhase,
			admittedTransitions,
			admittedOutcome);
	}

	protected bool ResolveAssignment(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_TransportTrip trip,
		out AICF_StrategicAssignmentSnapshot assignment)
	{
		assignment = null;
		if (!slot || !faction || !slot.IsCombatReady() || !slot.GetTargetBase())
			return false;
		if (trip && trip.GetAssignment() &&
			trip.GetAssignment().MatchesCurrent(
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				slot.GetGroup()) &&
			trip.GetAssignment().GetAssignmentRevision() == slot.GetStrategicAssignmentRevision() &&
			trip.GetAssignment().GetBaseRevision() == m_iObservedBaseRevision)
		{
			assignment = trip.GetAssignment();
			return true;
		}
		return m_OrderPlanner.TryCreateAssignmentSnapshot(
			slot,
			faction,
			m_iObservedBaseRevision,
			assignment);
	}

	protected bool IsAdmissionEligible(
		AICF_GroupSlot slot,
		AICF_StrategicAssignmentSnapshot assignment,
		AICF_FactionFleet fleet,
		out string reason)
	{
		reason = "NONE";
		string orderOwnershipFence = GetInfantryOrderAdmissionFenceReason(slot);
		if (!orderOwnershipFence.IsEmpty())
		{
			reason = orderOwnershipFence;
			return false;
		}
		if (slot.GetUnitType() == AICF_EGroupUnitType.INFANTRY)
		{
			reason = "COMMANDER_INFANTRY_PROFILE";
			return false;
		}
		int alive = AICF_GroupRuntime.CountAliveAgents(assignment.GetGroup());
		if (alive < m_Config.GetMinimumVehicleRequestAgents())
		{
			reason = "MINIMUM_ROSTER_NOT_MET";
			return false;
		}
		if (!IsWaypointQueued(assignment.GetGroup(), assignment.GetMeaningfulInfantryWaypoint()))
		{
			reason = "MEANINGFUL_INFANTRY_ORDER_MISSING";
			return false;
		}
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(assignment.GetGroup());
		if (!leader)
		{
			reason = "ALIVE_LEADER_MISSING";
			return false;
		}
		float routeMeters = Math.Sqrt(vector.DistanceSqXZ(
			leader.GetOrigin(),
			assignment.GetTargetPosition()));
		if (routeMeters < m_Config.GetMinimumRouteMeters())
		{
			reason = "ROUTE_BELOW_VEHICLE_THRESHOLD";
			return false;
		}
		if (fleet.HasLeaseForSlot(assignment.GetSlotId()))
		{
			reason = "SLOT_LEASE_ALREADY_PRESENT";
			return false;
		}
		int admissionHeld = fleet.GetActiveOrReservedCount() +
			m_Trips.GetPreLeaseNonTerminalCount(assignment.GetFactionKey());
		if (admissionHeld >= fleet.GetMaximumActiveOrReserved())
		{
			// Keep the squad on its meaningful infantry order.  A TransportTrip is
			// created only after an admission slot exists, so cap/site pressure cannot
			// manufacture a parallel fleet of APPROACHING/WAITING operations.
			reason = "VEHICLE_CAP_UNAVAILABLE_PRE_ADMISSION";
			return false;
		}
		return true;
	}

	protected string GetInfantryOrderAdmissionFenceReason(AICF_GroupSlot slot)
	{
		if (!slot)
			return string.Empty;
		if (slot.IsTemporaryRouteReplanHold())
			return "TEMPORARY_ROUTE_REPLAN_HOLD";
		if (slot.HasPendingOrderRecovery())
			return "ORDER_RECOVERY_PENDING";
		return string.Empty;
	}

	protected bool ShouldReplaceTerminalTrip(
		AICF_TransportTrip trip,
		AICF_StrategicAssignmentSnapshot assignment)
	{
		if (!trip || !assignment || !m_TripController.CanRetireTrip(trip))
			return false;
		return trip.GetAssignment().GetAssignmentRevision() != assignment.GetAssignmentRevision() ||
			trip.GetAssignment().GetBaseRevision() != assignment.GetBaseRevision() ||
			trip.GetAssignment().GetTargetBase() != assignment.GetTargetBase();
	}

	protected void ObserveTerminalCommit(
		AICF_TransportTrip trip,
		AICF_ETransportTripPhase previousPhase,
		int previousTransitions,
		AICF_TripOutcome observed)
	{
		// A terminal flow result is only evidence after the controller has
		// committed the matching terminal Trip phase.  Keep this guard outside
		// the controller so a future transition/reconciliation regression cannot
		// silently turn a destroyed-asset fallback into another scheduler loop.
		if (trip && observed && observed.IsTerminal() && !trip.IsTerminal())
		{
			m_Acceptance.ObserveTripFailure(
				trip,
				AICF_EVehicleAcceptanceFailureDomain.IDENTITY,
				"UNCOMMITTED_TERMINAL_OUTCOME:" + observed.GetReason());
			return;
		}
		if (!trip || !trip.IsTerminal() || previousTransitions == trip.GetTransitionCount() ||
			previousPhase == trip.GetPhase())
		{
			return;
		}
		AICF_TripOutcome terminalOutcome = observed;
		if (trip.GetPhase() == AICF_ETransportTripPhase.COMPLETE)
			terminalOutcome = AICF_TripOutcome.CompleteTrip(trip.GetTerminalReason(), trip.GetCausationId());
		else if (trip.GetPhase() == AICF_ETransportTripPhase.FALLBACK)
			terminalOutcome = AICF_TripOutcome.FallbackToFoot(trip.GetTerminalReason(), trip.GetCausationId());
		else
			terminalOutcome = AICF_TripOutcome.TerminalFailClosed(trip.GetTerminalReason(), trip.GetCausationId());
		m_Acceptance.ObserveCommittedTerminal(
			trip,
			terminalOutcome,
			ResolveTerminalFailureDomain(trip.GetPreviousPhase(), terminalOutcome));
	}

	protected AICF_EVehicleAcceptanceFailureDomain ResolveTerminalFailureDomain(
		AICF_ETransportTripPhase previousPhase,
		AICF_TripOutcome terminalOutcome)
	{
		if (!terminalOutcome ||
			terminalOutcome.GetKind() != AICF_ETripOutcomeKind.FALLBACK_TO_FOOT)
		{
			return AICF_EVehicleAcceptanceFailureDomain.NONE;
		}
		string terminalReason = terminalOutcome.GetReason();
		if (terminalReason == "VEHICLE_ON_FIRE" ||
			terminalReason == "VEHICLE_ON_FIRE_DURING_BOARDING")
		{
			// Combat, mines and other stock-world damage are valid runtime
			// conditions. Keep the terminal fallback evidence without turning an
			// unattributed destroyed asset into an acceptance failure.
			return AICF_EVehicleAcceptanceFailureDomain.NONE;
		}
		switch (previousPhase)
		{
			case AICF_ETransportTripPhase.BOARDING:
				return AICF_EVehicleAcceptanceFailureDomain.BOARDING;
			case AICF_ETransportTripPhase.TRANSIT:
				return AICF_EVehicleAcceptanceFailureDomain.RECOVERY;
			case AICF_ETransportTripPhase.DISMOUNT:
				return AICF_EVehicleAcceptanceFailureDomain.DISMOUNT;
		}
		return AICF_EVehicleAcceptanceFailureDomain.NONE;
	}

	protected void ReportAdmissionOnce(
		AICF_StrategicAssignmentSnapshot assignment,
		string reason)
	{
		AICF_VehicleAdmissionAudit audit = FindAdmissionAudit(
			assignment.GetFactionKey(),
			assignment.GetSlotId());
		if (audit && audit.Matches(assignment, reason))
			return;
		if (!audit)
		{
			audit = new AICF_VehicleAdmissionAudit();
			m_aAdmissionAudits.Insert(audit);
		}
		audit.Record(assignment, reason);
		AICF_Stage35Diagnostics.Info(
			"VEHICLE_REQUEST_INELIGIBLE",
			string.Format(
				"faction=%1 slot=%2 group_generation=%3 assignment_revision=%4 reason=%5",
				assignment.GetFactionKey(),
				assignment.GetSlotKey(),
				assignment.GetGroupGeneration(),
				assignment.GetAssignmentRevision(),
				reason));
	}

	protected AICF_VehicleAdmissionAudit FindAdmissionAudit(FactionKey factionKey, int slotId)
	{
		foreach (AICF_VehicleAdmissionAudit audit : m_aAdmissionAudits)
		{
			if (audit && audit.IsSlot(factionKey, slotId))
				return audit;
		}
		return null;
	}

	protected AICF_TransportTrip FindTripForSlot(AICF_GroupSlot slot)
	{
		if (!slot)
			return null;
		for (int index; index < m_Trips.GetCount(); index++)
		{
			AICF_TransportTrip trip = m_Trips.Get(index);
			if (trip && trip.GetSlotId() == slot.GetSlotId() &&
				trip.GetGroupGeneration() == slot.GetSpawnGeneration() &&
				trip.GetAssignment().GetGroup() == slot.GetGroup())
			{
				return trip;
			}
		}
		return null;
	}

	protected bool IsWaypointQueued(SCR_AIGroup group, AIWaypoint waypoint)
	{
		if (!group || !waypoint)
			return false;
		array<AIWaypoint> queue = {};
		group.GetWaypoints(queue);
		return queue.Contains(waypoint);
	}

	protected int CalculateTripDeadlineMs(int nowMs)
	{
		int budgetMs = m_Config.GetCohesionWaitTimeoutMs();
		budgetMs += m_Config.GetWaitProbeIntervalMs() * 2;
		budgetMs += m_Config.GetRetryBackoffMaxMs() * m_Config.GetSpawnMaxAttempts();
		budgetMs += m_Config.GetBoardingTimeoutMs() * 6;
		budgetMs += m_Config.GetObjectiveProgressTimeoutMs() * 2;
		budgetMs += m_Config.GetStuckTimeoutMs() * 2;
		budgetMs = Math.Max(600000, budgetMs);
		budgetMs = Math.Min(1800000, budgetMs);
		return nowMs + budgetMs;
	}

	protected bool IsAuthorityReady()
	{
		return !m_bStopped && m_Config && Replication.IsServer() &&
			m_Campaign && m_Campaign.IsMaster() && m_ConflictAdapter && m_OrderPlanner &&
			m_ObjectiveGraph && m_TargetSelector && m_Trips && m_Fleets &&
			m_TripController && m_CleanupManager && m_Diagnostics && m_Acceptance;
	}
}

// Admission diagnostics only. This is neither vehicle state nor a cooldown;
// it suppresses duplicate audit lines until planning identity/reason changes.
class AICF_VehicleAdmissionAudit
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected int m_iGroupGeneration;
	protected int m_iAssignmentRevision;
	protected int m_iBaseRevision;
	protected string m_sReason;

	bool IsSlot(FactionKey factionKey, int slotId)
	{
		return m_sFactionKey == factionKey && m_iSlotId == slotId;
	}

	bool Matches(AICF_StrategicAssignmentSnapshot assignment, string reason)
	{
		return assignment && IsSlot(assignment.GetFactionKey(), assignment.GetSlotId()) &&
			m_iGroupGeneration == assignment.GetGroupGeneration() &&
			m_iAssignmentRevision == assignment.GetAssignmentRevision() &&
			m_iBaseRevision == assignment.GetBaseRevision() && m_sReason == reason;
	}

	void Record(AICF_StrategicAssignmentSnapshot assignment, string reason)
	{
		m_sFactionKey = assignment.GetFactionKey();
		m_iSlotId = assignment.GetSlotId();
		m_iGroupGeneration = assignment.GetGroupGeneration();
		m_iAssignmentRevision = assignment.GetAssignmentRevision();
		m_iBaseRevision = assignment.GetBaseRevision();
		m_sReason = reason;
	}
}
