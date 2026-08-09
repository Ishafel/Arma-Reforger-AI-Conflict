// Authoritative Stage 3 orchestration. A vehicle temporarily owns the group's
// waypoint queue only from boarding through dismount; the stable infantry slot,
// strategic target, reinforcement ledger and victory rules remain unchanged.
class AICF_VehicleCoordinator
{
	protected static const int MOTION_REPORT_INTERVAL_MS = 10000;
	protected static const float BOARDING_STAGING_THRESHOLD_METERS = 75.0;
	protected static const float BOARDING_APPROACH_PROGRESS_METERS = 3.0;
	protected static const int BOARDING_APPROACH_ACTIVATION_WINDOW_MS = 5000;
	protected static const int BOARDING_SETTLED_POLLS_REQUIRED = 2;
	protected static const int BOARDING_TRANSITION_GRACE_MS = 10000;
	protected static const int BOARDING_PROGRESS_FRESH_MS = 10000;
	protected static const int DISMOUNT_CLEAR_POLLS_REQUIRED = 2;
	protected static const int DISMOUNT_CLEARANCE_RECOVERY_DELAY_MS = 3000;
	protected static const int DISMOUNT_CLEARANCE_RECOVERY_MAX_ATTEMPTS = 3;
	protected static const int DISMOUNT_CLEARANCE_TERMINAL_MAX_ATTEMPTS = 6;
	protected static const int VEHICLE_DELETE_RETRY_INTERVAL_MS = 2000;
	protected static const int VEHICLE_DELETE_MAX_ATTEMPTS = 3;
	protected static const int VEHICLE_DELETE_CONFIRM_TIMEOUT_MS = 10000;
	protected static const float DISMOUNT_CLEARANCE_MARGIN_METERS = 0.5;

	protected ref AICF_Stage3Config m_Config;
	protected ref AICF_VehicleCatalog m_Catalog;
	protected ref AICF_VehicleSpawner m_Spawner;
	protected ref AICF_VehicleWaypointFactory m_WaypointFactory;
	protected ref AICF_VehicleWatchdog m_Watchdog;
	protected ref AICF_OrderPlanner m_OrderPlanner;
	protected ref AICF_GroupCohesionPolicy m_CohesionPolicy;
	protected ref AICF_ObjectiveGraph m_ObjectiveGraph;
	protected ref AICF_TargetSelector m_TargetSelector;
	protected AICF_ConflictAdapter m_ConflictAdapter;
	protected SCR_GameModeCampaign m_Campaign;

	protected ref array<ref AICF_VehicleRuntime> m_aUSRuntime = {};
	protected ref array<ref AICF_VehicleRuntime> m_aUSSRRuntime = {};
	protected ref array<bool> m_aUSCapReported = {};
	protected ref array<bool> m_aUSSRCapReported = {};
	protected ref array<int> m_aUSNextVehicleAtMs = {};
	protected ref array<int> m_aUSSRNextVehicleAtMs = {};
	protected ref array<int> m_aUSNextVehicleGeneration = {};
	protected ref array<int> m_aUSSRNextVehicleGeneration = {};

	protected ref array<bool> m_aUSTransportCompletedSlots = {};
	protected ref array<bool> m_aUSSRTransportCompletedSlots = {};
	protected ref array<bool> m_aUSArmedCompletedSlots = {};
	protected ref array<bool> m_aUSSRArmedCompletedSlots = {};
	protected bool m_bResultLogged;
	protected bool m_bResultCandidateLogged;
	protected bool m_bAcceptanceFailureLatched;
	protected int m_iAcceptanceFailureCount;
	protected string m_sFirstAcceptanceFailureReason;

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
		m_CohesionPolicy = cohesionPolicy;
		m_ObjectiveGraph = objectiveGraph;
		m_TargetSelector = targetSelector;
		m_Catalog = new AICF_VehicleCatalog();
		m_Spawner = new AICF_VehicleSpawner();
		m_WaypointFactory = new AICF_VehicleWaypointFactory();
		m_Watchdog = new AICF_VehicleWatchdog();

		for (int i = 0; i < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION; i++)
		{
			m_aUSRuntime.Insert(null);
			m_aUSSRRuntime.Insert(null);
			m_aUSCapReported.Insert(false);
			m_aUSSRCapReported.Insert(false);
			m_aUSNextVehicleAtMs.Insert(0);
			m_aUSSRNextVehicleAtMs.Insert(0);
			m_aUSNextVehicleGeneration.Insert(-1);
			m_aUSSRNextVehicleGeneration.Insert(-1);
			m_aUSTransportCompletedSlots.Insert(false);
			m_aUSSRTransportCompletedSlots.Insert(false);
			m_aUSArmedCompletedSlots.Insert(false);
			m_aUSSRArmedCompletedSlots.Insert(false);
		}
	}

	void Update(
		AICF_FactionState usState,
		SCR_CampaignFaction usFaction,
		AICF_FactionState ussrState,
		SCR_CampaignFaction ussrFaction)
	{
		if (!m_Config || !m_Config.GetVehiclesEnabled() || !Replication.IsServer())
			return;

		ProcessFaction(usState, usFaction, m_aUSRuntime, m_aUSCapReported, m_aUSNextVehicleAtMs, m_aUSNextVehicleGeneration);
		ProcessFaction(ussrState, ussrFaction, m_aUSSRRuntime, m_aUSSRCapReported, m_aUSSRNextVehicleAtMs, m_aUSSRNextVehicleGeneration);
		TryEmitResultCandidate();
	}

	bool IsControllingMovement(AICF_GroupSlot slot)
	{
		if (!slot)
			return false;

		AICF_VehicleRuntime runtime = slot.GetVehicleRuntime();
		if (!runtime)
			return false;

		switch (runtime.GetState())
		{
			case AICF_EVehicleState.BOARDING:
			case AICF_EVehicleState.MOUNTED:
			case AICF_EVehicleState.MOVING:
			case AICF_EVehicleState.DISEMBARKING:
			case AICF_EVehicleState.RECOVERING:
			case AICF_EVehicleState.INFANTRY_FALLBACK:
				return true;
			case AICF_EVehicleState.ABANDONED:
			case AICF_EVehicleState.DESTROYED:
				return runtime.IsInfantryFallbackRestorePending();
		}

		return false;
	}

	// A base capture can invalidate the strategic target while vehicle control
	// owns the group's waypoint queue. Let the normal planner select/log the new
	// target, then immediately remove its temporary infantry waypoint. The next
	// coordinator update reroutes the existing vehicle to slot.GetTargetBase().
	bool ReplanControlledMovement(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason,
		SCR_CampaignMilitaryBaseComponent excludedTarget = null)
	{
		if (!slot || !faction || !IsControllingMovement(slot))
			return false;

		AICF_VehicleRuntime runtime = slot.GetVehicleRuntime();
		if (!runtime || m_OrderPlanner.IsStrategicTargetValid(slot, faction, runtime.GetTargetBase()))
			return false;

		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		if (!m_OrderPlanner.AssignOrder(
			slot,
			faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			reason,
			excludedTarget))
		{
			return false;
		}

		m_OrderPlanner.SuspendOrderForVehicle(slot);
		return oldTarget && slot.GetTargetBase() != oldTarget;
	}

	string GetMarkerState(AICF_GroupSlot slot)
	{
		if (!slot || !slot.GetVehicleRuntime())
			return string.Empty;

		AICF_EVehicleState state = slot.GetVehicleRuntime().GetState();
		if (state == AICF_EVehicleState.NONE)
			return string.Empty;
		if (state == AICF_EVehicleState.INFANTRY_FALLBACK || state == AICF_EVehicleState.ABANDONED || state == AICF_EVehicleState.DESTROYED)
			return "FALLBACK";

		return AICF_Stage3Diagnostics.StateToString(state);
	}

	void Heartbeat(int managedAgents)
	{
		if (!m_Config || !m_Config.GetVehiclesEnabled())
			return;

		AICF_Stage3Diagnostics.Info(
			"HEARTBEAT",
			string.Format(
				"us_active=%1 ussr_active=%2 mounted_groups=%3 abandoned=%4 recovering=%5 managed_agents=%6",
				CountSpawned(m_aUSRuntime),
				CountSpawned(m_aUSSRRuntime),
				CountState(m_aUSRuntime, AICF_EVehicleState.MOVING) + CountState(m_aUSSRRuntime, AICF_EVehicleState.MOVING),
				CountTerminal(m_aUSRuntime) + CountTerminal(m_aUSSRRuntime),
				CountState(m_aUSRuntime, AICF_EVehicleState.RECOVERING) + CountState(m_aUSSRRuntime, AICF_EVehicleState.RECOVERING),
				managedAgents));
	}

	void Stop(bool cleanupEntities)
	{
		StopRuntimes(m_aUSRuntime, cleanupEntities);
		StopRuntimes(m_aUSSRRuntime, cleanupEntities);
		if (!m_bResultLogged)
		{
			m_bResultLogged = true;
			string reason = "STOPPED_BEFORE_ACCEPTANCE";
			if (m_bAcceptanceFailureLatched)
				reason = string.Format("ACCEPTANCE_FAILURE_%1", m_sFirstAcceptanceFailureReason);
			else if (AICF_Stage3Diagnostics.HasErrors())
				reason = "STAGE3_ERRORS";
			else if (m_bResultCandidateLogged)
				reason = "READY_NOT_FINALIZED";
			AICF_Stage3Diagnostics.Info(
				"RESULT",
				string.Format(
					"status=FAIL reason=%1 candidate_ready=%2 acceptance_failure=%3 acceptance_failure_count=%4",
					reason,
					m_bResultCandidateLogged,
					m_bAcceptanceFailureLatched,
					m_iAcceptanceFailureCount));
		}
	}

	protected void ProcessFaction(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		array<ref AICF_VehicleRuntime> runtimes,
		array<bool> capReported,
		array<int> nextVehicleAtMs,
		array<int> nextVehicleGeneration)
	{
		if (!factionState || !faction)
			return;

		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			AICF_VehicleRuntime runtime = runtimes[slotId];
			if (runtime && (runtime.GetState() == AICF_EVehicleState.ABANDONED ||
				runtime.GetState() == AICF_EVehicleState.DESTROYED))
			{
				ProcessTerminal(runtime, faction, slot, runtimes, nextVehicleAtMs, nextVehicleGeneration, slotId);
				continue;
			}

			if (runtime && !IsRuntimeCurrent(runtime, slot))
			{
				if (runtime.GetState() != AICF_EVehicleState.ABANDONED && runtime.GetState() != AICF_EVehicleState.DESTROYED)
					BeginDetachedCleanup(runtime, slot, GetDetachReason(runtime, slot));
				ProcessTerminal(runtime, faction, slot, runtimes, nextVehicleAtMs, nextVehicleGeneration, slotId);
				continue;
			}

			if (!runtime)
			{
				if (nextVehicleGeneration.IsIndexValid(slotId) && nextVehicleAtMs.IsIndexValid(slotId) &&
					nextVehicleGeneration[slotId] != slot.GetSpawnGeneration())
				{
					nextVehicleGeneration[slotId] = -1;
					nextVehicleAtMs[slotId] = 0;
				}

				AICF_EVehicleKind desiredKind;
				if (!TryGetDesiredKind(slot, desiredKind) || !CanStartVehicleTrip(slot) || System.GetTickCount() < nextVehicleAtMs[slotId])
					continue;

				if (CountReserved(runtimes) >= m_Config.GetMaxVehiclesPerFaction())
				{
					if (!capReported[slotId])
					{
						capReported[slotId] = true;
						AICF_Stage3Diagnostics.Info(
							"VEHICLE_CAP_BLOCKED",
							string.Format("faction=%1 slot=%2 active_or_reserved=%3 limit=%4", faction.GetFactionKey(), slotId, CountReserved(runtimes), m_Config.GetMaxVehiclesPerFaction()));
					}
					continue;
				}

				capReported[slotId] = false;
				runtime = new AICF_VehicleRuntime(faction.GetFactionKey(), slotId, slot.GetSpawnGeneration(), desiredKind);
				runtime.SetGroup(slot.GetGroup());
				runtime.SetTargetBase(slot.GetTargetBase());
				runtimes[slotId] = runtime;
				slot.SetVehicleRuntime(runtime);
				AICF_Stage3Diagnostics.Info("VEHICLE_REQUESTED", runtime.DescribeContext("LONG_RANGE_ATTACK_ORDER"));
			}

			ProcessRuntime(runtime, factionState, faction, slot, runtimes, nextVehicleAtMs, nextVehicleGeneration, slotId);
		}
	}

	protected void ProcessRuntime(
		AICF_VehicleRuntime runtime,
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		array<ref AICF_VehicleRuntime> runtimes,
		array<int> nextVehicleAtMs,
		array<int> nextVehicleGeneration,
		int slotId)
	{
		if (!runtime)
			return;

		switch (runtime.GetState())
		{
			case AICF_EVehicleState.REQUESTED:
				ProcessRequested(runtime, faction, slot);
				break;
			case AICF_EVehicleState.BOARDING:
				ProcessBoarding(runtime, faction, slot);
				break;
			case AICF_EVehicleState.MOVING:
				ProcessMoving(runtime, faction, slot);
				break;
			case AICF_EVehicleState.RECOVERING:
				ProcessDriverRecovery(runtime, faction, slot);
				break;
			case AICF_EVehicleState.DISEMBARKING:
				ProcessDismount(runtime, faction, slot);
				break;
			case AICF_EVehicleState.DISMOUNTED:
				ProcessDismounted(runtime, faction, slot);
				break;
			case AICF_EVehicleState.INFANTRY_FALLBACK:
				ProcessFallback(runtime, faction, slot, nextVehicleAtMs, nextVehicleGeneration);
				break;
			case AICF_EVehicleState.ABANDONED:
			case AICF_EVehicleState.DESTROYED:
				ProcessTerminal(runtime, faction, slot, runtimes, nextVehicleAtMs, nextVehicleGeneration, slotId);
				break;
		}
	}

	protected void ProcessRequested(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (System.GetTickCount() < runtime.GetNextAttemptAtMs())
			return;

		if (runtime.GetVehicle())
		{
			StartBoarding(runtime, faction, slot, "REUSE_ASSIGNED_VEHICLE");
			return;
		}

		ResourceName prefab = m_Catalog.SelectPrefab(faction, runtime.GetKind());
		if (prefab.IsEmpty())
		{
			slot.RecordVehicleTerminalFailure("PREFAB_UNAVAILABLE");
			BeginFallback(runtime, faction, slot, "PREFAB_UNAVAILABLE");
			return;
		}

		runtime.SetState(AICF_EVehicleState.SPAWNING);
		Vehicle vehicle;
		SCR_AIVehicleUsageComponent usage;
		SCR_CampaignMilitaryBaseComponent spawnBase;
		SCR_CampaignMilitaryBaseComponent failureBase;
		string failureReason;
		bool retryable;
		vector groupPosition = slot.GetGroup().GetOrigin();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (leader)
			groupPosition = leader.GetOrigin();

		if (!m_Spawner.TrySpawn(
			m_Campaign,
			faction,
			slot,
			m_ConflictAdapter,
			prefab,
			groupPosition,
			m_Config.GetMaximumSpawnDistanceMeters(),
			m_Config.GetMaximumReuseDistanceMeters(),
			runtime,
			vehicle,
			usage,
			spawnBase,
			failureReason,
			retryable,
			failureBase))
		{
			if (retryable)
			{
				runtime.SetState(AICF_EVehicleState.REQUESTED);
				int retryDelayMs = m_Config.GetRetryIntervalMs();
				if (failureReason == "SPAWN_FACTION_INITIALIZING")
					retryDelayMs = Math.Min(retryDelayMs, 1000);
				runtime.SetNextAttemptAtMs(System.GetTickCount() + retryDelayMs);
				string reportKey = string.Format("SITE:%1:%2", AICF_Stage1Diagnostics.BaseKey(failureBase), failureReason);
				if (runtime.MarkSpawnIssueReported(reportKey))
				{
					string details = string.Format(
						"%1 base=%2 retryable=1 retry_ms=%3",
						runtime.DescribeContext(failureReason),
						AICF_Stage1Diagnostics.BaseKey(failureBase),
						retryDelayMs);
					if (failureReason == "SPAWN_FACTION_INITIALIZING")
						AICF_Stage3Diagnostics.Info("VEHICLE_SPAWN_DEFERRED", details);
					else
						AICF_Stage3Diagnostics.Warning("VEHICLE_SPAWN_SITE_REJECTED", details);
				}
				return;
			}

			slot.RecordVehicleTerminalFailure(failureReason);
			string failureEvent = "VEHICLE_SPAWN_FAILED";
			if (failureReason.Contains("FACTION"))
				failureEvent = "VEHICLE_FACTION_MISMATCH";
			else if (failureReason == "AI_USAGE_INVALID")
				failureEvent = "VEHICLE_AI_USAGE_INVALID";
			AICF_Stage3Diagnostics.Error(
				failureEvent,
				string.Format("%1 prefab=%2 base=%3 retryable=0", runtime.DescribeContext(failureReason), prefab, AICF_Stage1Diagnostics.BaseKey(failureBase)));
			BeginFallback(runtime, faction, slot, failureReason);
			return;
		}

		if (!runtime.BindVehicle(vehicle, usage, prefab, spawnBase))
		{
			RplComponent.DeleteRplEntity(vehicle, false);
			slot.RecordVehicleTerminalFailure("RUNTIME_BIND_REJECTED");
			AICF_Stage3Diagnostics.Error("VEHICLE_BIND_FAILED", runtime.DescribeContext("RUNTIME_BIND_REJECTED"));
			BeginFallback(runtime, faction, slot, "RUNTIME_BIND_REJECTED");
			return;
		}
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_SPAWNED",
			string.Format("%1 prefab=%2 base=%3 assigned_faction=%4", runtime.DescribeContext("SPAWN_SUCCESS"), prefab, AICF_Stage1Diagnostics.BaseKey(spawnBase), faction.GetFactionKey()));

		StartBoarding(runtime, faction, slot, "NEW_VEHICLE");
	}

	protected void StartBoarding(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string reason)
	{
		SCR_AIGroup group = slot.GetGroup();
		Vehicle vehicle = runtime.GetVehicle();
		if (!group || !vehicle)
		{
			BeginFallback(runtime, faction, slot, "BOARDING_INPUT_INVALID");
			return;
		}

		int distanceAlive;
		float leaderDistanceMeters;
		float nearestDistanceMeters;
		float farthestDistanceMeters;
		string distanceSamples;
		m_Watchdog.MeasureAliveGroupDistances(
			group,
			vehicle,
			distanceAlive,
			leaderDistanceMeters,
			nearestDistanceMeters,
			farthestDistanceMeters,
			distanceSamples);
		float maximumBoardingDistanceMeters = m_Config.GetMaximumReuseDistanceMeters();
		if (distanceAlive > 0 && farthestDistanceMeters > maximumBoardingDistanceMeters)
		{
			AICF_Stage3Diagnostics.Warning(
				"BOARDING_REJECTED",
				string.Format(
					"%1 alive=%2 leader_m=%3 nearest_m=%4 farthest_m=%5 maximum_m=%6 member_samples=[%7]",
					runtime.DescribeContext("VEHICLE_TOO_FAR"),
					distanceAlive,
					leaderDistanceMeters,
					nearestDistanceMeters,
					farthestDistanceMeters,
					maximumBoardingDistanceMeters,
					distanceSamples));
			BeginFallback(runtime, faction, slot, "VEHICLE_TOO_FAR");
			return;
		}
		float stagingThresholdMeters = Math.Min(BOARDING_STAGING_THRESHOLD_METERS, maximumBoardingDistanceMeters);
		bool needsApproach = distanceAlive > 0 && farthestDistanceMeters > stagingThresholdMeters;

		int interruptedActions = m_Watchdog.ResetGroupVehicleActions(group);
		IEntity driver = m_Watchdog.ResolveAliveDriver(runtime);
		if (!m_Watchdog.IsAliveGroupMember(group, driver) ||
			!m_Watchdog.IsMemberSettledInVehicle(driver, vehicle))
			driver = null;
		IEntity gunner;
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
		{
			gunner = m_Watchdog.ResolveAliveGunner(runtime);
			if (!m_Watchdog.IsAliveGroupMember(group, gunner) ||
				!m_Watchdog.IsMemberSettledInVehicle(gunner, vehicle))
				gunner = null;
		}

		bool hasFreePilot;
		bool hasFreeTurret;
		int accessibleSeats = m_Watchdog.CountAccessibleSeats(runtime, hasFreePilot, hasFreeTurret);
		int aliveAgents = AICF_GroupRuntime.CountAliveAgents(group);
		int mounted = m_Watchdog.CountAliveGroupMembersInVehicle(group, vehicle);
		int availableCapacity = accessibleSeats + mounted;
		if (aliveAgents <= 0 || (!driver && !hasFreePilot) || availableCapacity < aliveAgents ||
			(runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT && ((!gunner && !hasFreeTurret) || aliveAgents < 2)))
		{
			AICF_Stage3Diagnostics.Warning(
				"BOARDING_REJECTED",
				string.Format(
					"%1 alive=%2 mounted=%3 empty_accessible=%4 available_capacity=%5 driver_ready=%6 gunner_ready=%7 free_pilot=%8 free_turret=%9",
					runtime.DescribeContext("INSUFFICIENT_COMPARTMENTS"),
					aliveAgents,
					mounted,
					accessibleSeats,
					availableCapacity,
					driver != null,
					gunner != null,
					hasFreePilot,
					hasFreeTurret));
			BeginFallback(runtime, faction, slot, "INSUFFICIENT_COMPARTMENTS");
			return;
		}

		SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
		if (!groupUtility)
		{
			AICF_Stage3Diagnostics.Error("VEHICLE_GROUP_UTILITY_MISSING", runtime.DescribeContext("GROUP_UTILITY_MISSING"));
			BeginFallback(runtime, faction, slot, "GROUP_UTILITY_MISSING");
			return;
		}

		// Mandatory crew seats are assigned by exact, reserved actions below.
		// Keeping the vehicle registered in group utility at this point lets its
		// generic vehicle behaviours race the PILOT/GUNNER phases and occupy cargo
		// first. It is attached only after all mandatory crew roles are physical.
		DetachVehicleFromGroup(runtime);
		m_OrderPlanner.SuspendOrderForVehicle(slot);
		runtime.SetState(AICF_EVehicleState.BOARDING);
		runtime.SetBoardingPhase(AICF_EVehicleBoardingPhase.NONE);
		runtime.SetLastDriver(driver);
		runtime.SetLastGunner(gunner);
		bool driverPhasePlanned = driver == null;
		bool gunnerPhasePlanned = runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT &&
			(gunner == null || driverPhasePlanned);
		int plannedPhases = CalculatePlannedBoardingPhaseCount(
			runtime.GetKind(),
			needsApproach,
			driverPhasePlanned,
			gunnerPhasePlanned);
		runtime.BeginBoardingDeadline(
			plannedPhases,
			driverPhasePlanned,
			gunnerPhasePlanned);
		int phaseTimeoutMs = m_Config.GetBoardingTimeoutMs();
		if (driver)
			AICF_Stage3Diagnostics.Info("DRIVER_ASSIGNED", string.Format("%1 driver=%2", runtime.DescribeContext("PILOT_COMPARTMENT_ALREADY_OCCUPIED"), driver.GetID()));
		if (gunner)
			AICF_Stage3Diagnostics.Info("GUNNER_ASSIGNED", string.Format("%1 gunner=%2", runtime.DescribeContext("TURRET_COMPARTMENT_ALREADY_OCCUPIED"), gunner.GetID()));
		AICF_Stage3Diagnostics.Info("VEHICLE_ASSIGNED", string.Format("%1 prefab=%2", runtime.DescribeContext(reason), runtime.GetVehiclePrefab()));
		string boardingStartedDetails = string.Format(
			"%1 alive=%2 mounted=%3 empty_accessible=%4 available_capacity=%5 policy=APPROACH_THEN_ROLE_ORDERED phase_timeout_ms=%6 planned_phases=%7 total_timeout_ms=%8 group_to_vehicle_distance_m=%9",
			runtime.DescribeContext(reason),
			aliveAgents,
			mounted,
			accessibleSeats,
			availableCapacity,
			phaseTimeoutMs,
			plannedPhases,
			phaseTimeoutMs * plannedPhases,
			leaderDistanceMeters);
		boardingStartedDetails += string.Format(
			" nearest_m=%1 farthest_m=%2 staging_threshold_m=%3 approach_planned=%4 interrupted_actions=%5 member_samples=[%6]",
			nearestDistanceMeters,
			farthestDistanceMeters,
			stagingThresholdMeters,
			needsApproach,
			interruptedActions,
			distanceSamples);
		AICF_Stage3Diagnostics.Info("BOARDING_STARTED", boardingStartedDetails);

		if (needsApproach)
		{
			if (!StartBoardingPhase(runtime, slot, AICF_EVehicleBoardingPhase.APPROACH))
			{
				LatchAcceptanceFailure(runtime, "BOARDING_APPROACH_WAYPOINT_FAILED");
				BeginFallback(runtime, faction, slot, "BOARDING_APPROACH_WAYPOINT_FAILED");
			}
			return;
		}

		// Normalize a pre-existing wrong-seat occupant synchronously. Waiting for
		// the next coordinator poll would let the exact DRIVER action race cargo
		// or turret occupancy that is already visible in this snapshot.
		if (!driver && mounted > 0)
		{
			runtime.SetBoardingPhase(AICF_EVehicleBoardingPhase.DRIVER);
			runtime.RestartPhaseDeadline();
			ProcessBoardingRoleReset(runtime, faction, slot, mounted);
			return;
		}

		string phaseFailureReason;
		if (!ContinueRoleOrderedBoarding(runtime, slot, phaseFailureReason))
			BeginFallback(runtime, faction, slot, phaseFailureReason);
	}

	protected void ProcessBoarding(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (m_Watchdog.IsDestroyed(runtime))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_DESTROYED_DURING_BOARDING");
			return;
		}
		if (m_Watchdog.IsOnFire(runtime))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_ON_FIRE_DURING_BOARDING");
			return;
		}
		if (m_Watchdog.IsOverturned(runtime))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_OVERTURNED_DURING_BOARDING");
			return;
		}

		SCR_AIGroup group = slot.GetGroup();
		Vehicle vehicle = runtime.GetVehicle();
		if (!group || !vehicle)
		{
			BeginFallback(runtime, faction, slot, "BOARDING_INPUT_INVALID");
			return;
		}

		int aliveCount;
		int linkedCount;
		int compartmentCount;
		int gettingInCount;
		int gettingOutCount;
		int characterVehicleCount;
		int settledCount;
		float nearestDistanceMeters;
		float farthestDistanceMeters;
		string memberSamples;
		if (!m_Watchdog.InspectBoardingProgress(
			group,
			vehicle,
			aliveCount,
			linkedCount,
			compartmentCount,
			gettingInCount,
			gettingOutCount,
			characterVehicleCount,
			settledCount,
			nearestDistanceMeters,
			farthestDistanceMeters,
			memberSamples))
		{
			BeginFallback(runtime, faction, slot, "BOARDING_PROGRESS_INPUT_INVALID");
			return;
		}

		AICF_EVehicleBoardingPhase phase = runtime.GetBoardingPhase();
		bool progressAdvanced = runtime.ObserveBoardingProgress(
			linkedCount,
			compartmentCount,
			gettingInCount,
			characterVehicleCount,
			settledCount);
		if (phase == AICF_EVehicleBoardingPhase.APPROACH &&
			runtime.ObserveBoardingApproachProgress(farthestDistanceMeters, BOARDING_APPROACH_PROGRESS_METERS))
		{
			progressAdvanced = true;
		}
		string waypointState = DescribeBoardingWaypointState(group, runtime);
		if (progressAdvanced)
		{
			string progressDetails = string.Format(
				"%1 phase=%2 alive=%3 linked=%4 compartment=%5 getting_in=%6 getting_out=%7",
				runtime.DescribeContext("PHYSICAL_PROGRESS_ADVANCED"),
				typename.EnumToString(AICF_EVehicleBoardingPhase, phase),
				aliveCount,
				linkedCount,
				compartmentCount,
				gettingInCount,
				gettingOutCount);
			progressDetails += string.Format(
				" character_vehicle=%1 settled=%2 max_linked=%3 max_compartment=%4 max_getting_in=%5 max_character_vehicle=%6 max_settled=%7",
				characterVehicleCount,
				settledCount,
				runtime.GetBoardingMaxLinkedCount(),
				runtime.GetBoardingMaxCompartmentCount(),
				runtime.GetBoardingMaxGettingInCount(),
				runtime.GetBoardingMaxCharacterVehicleCount(),
				runtime.GetBoardingMaxSettledCount());
			progressDetails += string.Format(
				" nearest_m=%1 farthest_m=%2 best_farthest_m=%3 waypoint=[%4] members=[%5]",
				nearestDistanceMeters,
				farthestDistanceMeters,
				runtime.GetBestBoardingFarthestDistanceMeters(),
				waypointState,
				memberSamples);
			AICF_Stage3Diagnostics.Info(
				"BOARDING_PROGRESS",
				progressDetails);
		}

		if (phase == AICF_EVehicleBoardingPhase.APPROACH)
		{
			float stagingThresholdMeters = Math.Min(
				BOARDING_STAGING_THRESHOLD_METERS,
				m_Config.GetMaximumReuseDistanceMeters());
			AIWaypoint approachWaypoint = runtime.GetActiveWaypoint();
			array<AIWaypoint> approachQueue = {};
			group.GetWaypoints(approachQueue);
			bool approachInQueue = approachWaypoint && approachQueue.Contains(approachWaypoint);
			bool approachCurrent = approachWaypoint && group.GetCurrentWaypoint() == approachWaypoint;
			int approachActivationAgeMs = System.GetTickCount(runtime.GetStateStartedAtMs());
			bool approachActivationExpired = approachActivationAgeMs >= BOARDING_APPROACH_ACTIVATION_WINDOW_MS;
			if (farthestDistanceMeters > stagingThresholdMeters &&
				(!approachWaypoint || !approachInQueue || (approachActivationExpired && !approachCurrent)))
			{
				string lostCause = "MOVE_WAYPOINT_NOT_CURRENT";
				if (!approachWaypoint)
					lostCause = "MOVE_WAYPOINT_MISSING";
				else if (!approachInQueue)
					lostCause = "MOVE_WAYPOINT_NOT_IN_QUEUE";
				AICF_Stage3Diagnostics.Warning(
					"BOARDING_APPROACH_LOST",
					string.Format(
						"%1 alive=%2 nearest_m=%3 farthest_m=%4 threshold_m=%5 activation_age_ms=%6 activation_window_ms=%7",
						runtime.DescribeContext(lostCause),
						aliveCount,
						nearestDistanceMeters,
						farthestDistanceMeters,
						stagingThresholdMeters,
						approachActivationAgeMs,
						BOARDING_APPROACH_ACTIVATION_WINDOW_MS) +
					string.Format(" waypoint=[%1] members=[%2]", waypointState, memberSamples));
				LatchAcceptanceFailure(runtime, "BOARDING_APPROACH_WAYPOINT_LOST");
				BeginFallback(runtime, faction, slot, "BOARDING_APPROACH_WAYPOINT_LOST");
				return;
			}
			if (farthestDistanceMeters <= stagingThresholdMeters)
			{
				DeleteRuntimeWaypoint(runtime);
				AICF_Stage3Diagnostics.Info(
					"BOARDING_APPROACH_COMPLETE",
					string.Format(
						"%1 alive=%2 nearest_m=%3 farthest_m=%4 threshold_m=%5 waypoint=[%6] members=[%7]",
						runtime.DescribeContext("ALL_ALIVE_MEMBERS_STAGED"),
						aliveCount,
						nearestDistanceMeters,
						farthestDistanceMeters,
						stagingThresholdMeters,
						waypointState,
						memberSamples));
				IEntity currentDriver = m_Watchdog.ResolveAliveDriver(runtime);
				if (!m_Watchdog.IsAliveGroupMember(group, currentDriver) ||
					!m_Watchdog.IsMemberSettledInVehicle(currentDriver, vehicle))
				{
					currentDriver = null;
				}
				int currentMounted = m_Watchdog.CountAliveGroupMembersInVehicle(group, vehicle);
				if (!currentDriver && currentMounted > 0)
				{
					runtime.SetBoardingPhase(AICF_EVehicleBoardingPhase.DRIVER);
					runtime.RestartPhaseDeadline();
					ProcessBoardingRoleReset(runtime, faction, slot, currentMounted);
					return;
				}
				string phaseFailureReason;
				if (!ContinueRoleOrderedBoarding(runtime, slot, phaseFailureReason))
					BeginFallback(runtime, faction, slot, phaseFailureReason);
				return;
			}
		}

		IEntity driver = m_Watchdog.ResolveAliveDriver(runtime);
		if (!m_Watchdog.IsAliveGroupMember(group, driver))
			driver = null;
		bool driverSettled = driver && m_Watchdog.IsMemberSettledInVehicle(driver, runtime.GetVehicle());
		if (driverSettled && driver != runtime.GetLastDriver())
		{
			runtime.SetLastDriver(driver);
			AICF_Stage3Diagnostics.Info("DRIVER_ASSIGNED", string.Format("%1 driver=%2", runtime.DescribeContext("PILOT_COMPARTMENT_OCCUPIED"), driver.GetID()));
		}

		IEntity gunner;
		bool gunnerSettled;
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
		{
			gunner = m_Watchdog.ResolveAliveGunner(runtime);
			if (!m_Watchdog.IsAliveGroupMember(group, gunner))
				gunner = null;
			gunnerSettled = gunner && m_Watchdog.IsMemberSettledInVehicle(gunner, runtime.GetVehicle());
			if (gunnerSettled && gunner != runtime.GetLastGunner())
			{
				runtime.SetLastGunner(gunner);
				AICF_Stage3Diagnostics.Info("GUNNER_ASSIGNED", string.Format("%1 gunner=%2", runtime.DescribeContext("TURRET_COMPARTMENT_OCCUPIED"), gunner.GetID()));
			}
		}

		int mounted = m_Watchdog.CountAliveGroupMembersInVehicle(group, vehicle);
		if (phase == AICF_EVehicleBoardingPhase.DRIVER && !driver &&
			(mounted > 0 || (runtime.IsBoardingRoleResetAttempted() && !runtime.IsBoardingRoleRetryIssued())))
		{
			ProcessBoardingRoleReset(runtime, faction, slot, mounted);
			return;
		}

		if (phase == AICF_EVehicleBoardingPhase.DRIVER && driverSettled)
		{
			// The exact action may still be finishing its success edge. Do not Fail it:
			// SCR_AIGetInVehicle failure ejects an entity that already reached its seat.
			runtime.ClearCrewRecoveryTracking();
			if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
			{
				if (gunnerSettled)
				{
					if (!StartBoardingPhase(runtime, slot, AICF_EVehicleBoardingPhase.PASSENGERS))
						BeginFallback(runtime, faction, slot, "PASSENGER_BOARDING_WAYPOINT_FAILED");
					return;
				}
				if (!runtime.IsBoardingGunnerPhasePlanned())
				{
					BeginFallback(runtime, faction, slot, "GUNNER_LOST_BEFORE_ROLE_PHASE");
					return;
				}
				if (!StartBoardingPhase(runtime, slot, AICF_EVehicleBoardingPhase.GUNNER))
				{
					BeginFallback(runtime, faction, slot, "GUNNER_BOARDING_ACTION_FAILED");
					return;
				}
				return;
			}

			if (!StartBoardingPhase(runtime, slot, AICF_EVehicleBoardingPhase.PASSENGERS))
			{
				BeginFallback(runtime, faction, slot, "PASSENGER_BOARDING_WAYPOINT_FAILED");
				return;
			}
			return;
		}

		if (phase == AICF_EVehicleBoardingPhase.GUNNER)
		{
			if (!driverSettled)
			{
				BeginFallback(runtime, faction, slot, "DRIVER_LOST_DURING_BOARDING");
				return;
			}

			if (gunnerSettled)
			{
				runtime.ClearCrewRecoveryTracking();
				if (!StartBoardingPhase(runtime, slot, AICF_EVehicleBoardingPhase.PASSENGERS))
				{
					BeginFallback(runtime, faction, slot, "PASSENGER_BOARDING_WAYPOINT_FAILED");
					return;
				}
				return;
			}
		}

		if (phase == AICF_EVehicleBoardingPhase.PASSENGERS)
		{
			if (!driverSettled || (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT && !gunnerSettled))
			{
				BeginFallback(runtime, faction, slot, "CREW_ROLE_LOST_DURING_BOARDING");
				return;
			}

			bool allSettled = aliveCount > 0 && settledCount == aliveCount &&
				m_Watchdog.AreAllAliveMembersSettledInVehicle(group, vehicle);
			int settledPolls = runtime.RecordBoardingSettledPoll(allSettled);
			if (settledPolls >= BOARDING_SETTLED_POLLS_REQUIRED)
			{
				CompleteBoarding(runtime, faction, slot, gunner);
				return;
			}
		}
		else
		{
			runtime.ResetBoardingSettledPolls();
		}

		int phaseAgeMs = System.GetTickCount(runtime.GetStateStartedAtMs());
		int phaseTimeoutMs = m_Config.GetBoardingTimeoutMs();
		int totalAgeMs = runtime.GetBoardingAgeMs();
		int totalTimeoutMs = phaseTimeoutMs * runtime.GetPlannedBoardingPhaseCount();
		bool phaseExpired = phaseAgeMs >= phaseTimeoutMs;
		bool totalExpired = totalAgeMs >= totalTimeoutMs;
		bool softDeadlineExpired = phaseExpired || totalExpired;
		if (softDeadlineExpired)
		{
			bool graceWasGranted = runtime.IsBoardingGraceGranted();
			bool graceEligible = gettingInCount > 0 || runtime.HasRecentBoardingProgress(BOARDING_PROGRESS_FRESH_MS);
			bool graceGranted = runtime.EvaluateBoardingTransitionGrace(graceEligible);
			if (graceGranted && !graceWasGranted)
			{
				string graceDetails = string.Format(
					"%1 phase=%2 grace_ms=%3 phase_age_ms=%4 phase_timeout_ms=%5 total_age_ms=%6 total_timeout_ms=%7 getting_in=%8",
					runtime.DescribeContext("VERIFIED_PROGRESS_AT_SOFT_DEADLINE"),
					typename.EnumToString(AICF_EVehicleBoardingPhase, phase),
					BOARDING_TRANSITION_GRACE_MS,
					phaseAgeMs,
					phaseTimeoutMs,
					totalAgeMs,
					totalTimeoutMs,
					gettingInCount);
				graceDetails += string.Format(
					" max_linked=%1 max_compartment=%2 max_getting_in=%3 max_character_vehicle=%4 max_settled=%5 best_farthest_m=%6 waypoint=[%7] members=[%8]",
					runtime.GetBoardingMaxLinkedCount(),
					runtime.GetBoardingMaxCompartmentCount(),
					runtime.GetBoardingMaxGettingInCount(),
					runtime.GetBoardingMaxCharacterVehicleCount(),
					runtime.GetBoardingMaxSettledCount(),
					runtime.GetBestBoardingFarthestDistanceMeters(),
					waypointState,
					memberSamples);
				AICF_Stage3Diagnostics.Info(
					"BOARDING_TRANSITION_GRACE",
					graceDetails);
			}
			bool phaseHardExpired = phaseAgeMs >= phaseTimeoutMs + BOARDING_TRANSITION_GRACE_MS;
			bool totalHardExpired = totalAgeMs >= totalTimeoutMs + BOARDING_TRANSITION_GRACE_MS;
			if (graceGranted && !phaseHardExpired && !totalHardExpired)
				return;
		}
		if (softDeadlineExpired)
		{
			int alive = AICF_GroupRuntime.CountAliveAgents(group);
			string cause = "PASSENGERS_NOT_MOUNTED";
			if (phase == AICF_EVehicleBoardingPhase.APPROACH)
				cause = "APPROACH_NOT_COMPLETE";
			else if (!driverSettled)
				cause = "DRIVER_NOT_ASSIGNED";
			else if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT && !gunnerSettled)
				cause = "GUNNER_NOT_ASSIGNED";
			string deadlineScope = "PHASE";
			string deadlineReason = "BOARDING_PHASE_DEADLINE_EXCEEDED";
			if (totalExpired)
			{
				deadlineScope = "TOTAL";
				deadlineReason = "BOARDING_TOTAL_DEADLINE_EXCEEDED";
			}
			if (runtime.IsBoardingGraceGranted())
			{
				deadlineScope = "PHASE_GRACE";
				deadlineReason = "BOARDING_PHASE_GRACE_DEADLINE_EXCEEDED";
				if (totalAgeMs >= totalTimeoutMs + BOARDING_TRANSITION_GRACE_MS)
				{
					deadlineScope = "TOTAL_GRACE";
					deadlineReason = "BOARDING_TOTAL_GRACE_DEADLINE_EXCEEDED";
				}
			}
			string timeoutDetails = string.Format(
				"%1 phase=%2 cause=%3 alive=%4 mounted=%5 driver=%6 gunner=%7",
				runtime.DescribeContext(deadlineReason),
				typename.EnumToString(AICF_EVehicleBoardingPhase, runtime.GetBoardingPhase()),
				cause,
				alive,
				mounted,
				driverSettled,
				gunnerSettled);
			timeoutDetails += string.Format(
				" phase_age_ms=%1 timeout_ms=%2 total_age_ms=%3 total_timeout_ms=%4 planned_phases=%5 deadline_scope=%6 settled_polls=%7",
				phaseAgeMs,
				phaseTimeoutMs,
				totalAgeMs,
				totalTimeoutMs,
				runtime.GetPlannedBoardingPhaseCount(),
				deadlineScope,
				runtime.GetBoardingSettledPollCount());
			timeoutDetails += string.Format(
				" max_linked=%1 max_compartment=%2 max_getting_in=%3 max_character_vehicle=%4 max_settled=%5 best_farthest_m=%6 waypoint=[%7] members=[%8]",
				runtime.GetBoardingMaxLinkedCount(),
				runtime.GetBoardingMaxCompartmentCount(),
				runtime.GetBoardingMaxGettingInCount(),
				runtime.GetBoardingMaxCharacterVehicleCount(),
				runtime.GetBoardingMaxSettledCount(),
				runtime.GetBestBoardingFarthestDistanceMeters(),
				waypointState,
				memberSamples);
			AICF_Stage3Diagnostics.Warning("BOARDING_TIMEOUT", timeoutDetails);
			LatchAcceptanceFailure(runtime, string.Format("BOARDING_TIMEOUT_%1", cause));
			BeginFallback(runtime, faction, slot, string.Format("BOARDING_TIMEOUT_%1", cause));
		}
	}

	protected void CompleteBoarding(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		IEntity gunner)
	{
		SCR_AIGroup group = slot.GetGroup();
		Vehicle vehicle = runtime.GetVehicle();
		if (runtime.GetBoardingSettledPollCount() < BOARDING_SETTLED_POLLS_REQUIRED ||
			!m_Watchdog.AreAllAliveMembersSettledInVehicle(group, vehicle))
		{
			return;
		}

		int mounted = m_Watchdog.CountAliveGroupMembersInVehicle(group, vehicle);
		string waypointState = DescribeBoardingWaypointState(group, runtime);
		if (!AttachVehicleToGroup(runtime, slot))
		{
			BeginFallback(runtime, faction, slot, "GROUP_UTILITY_MISSING_AFTER_CREW");
			return;
		}
		DeleteRuntimeWaypoint(runtime);
		runtime.ClearCrewRecoveryTracking();
		runtime.SetBoardingPhase(AICF_EVehicleBoardingPhase.NONE);
		AICF_Stage3Diagnostics.Info(
			"BOARDING_COMPLETE",
			string.Format(
				"%1 mounted=%2 driver=1 gunner=%3 settled_polls=%4 max_linked=%5 max_compartment=%6 max_getting_in=%7 max_character_vehicle=%8 max_settled=%9",
				runtime.DescribeContext("ALL_ALIVE_MEMBERS_MOUNTED"),
				mounted,
				gunner != null,
				runtime.GetBoardingSettledPollCount(),
				runtime.GetBoardingMaxLinkedCount(),
				runtime.GetBoardingMaxCompartmentCount(),
				runtime.GetBoardingMaxGettingInCount(),
				runtime.GetBoardingMaxCharacterVehicleCount(),
				runtime.GetBoardingMaxSettledCount()) +
			string.Format(" waypoint=[%1]", waypointState));
		StartMovement(runtime, faction, slot, "BOARDING_COMPLETE");
	}

	protected void ProcessBoardingRoleReset(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		int mounted)
	{
		SCR_AIGroup group = slot.GetGroup();
		if (!runtime.IsBoardingRoleResetAttempted())
		{
			runtime.BeginBoardingRoleReset();
			CancelCrewRecovery(runtime);
			DeleteRuntimeWaypoint(runtime);
			int interrupted = m_Watchdog.ResetGroupVehicleActions(group);
			int forced = ForceAliveGroupMembersOut(group, runtime.GetVehicle());
			int remaining = m_Watchdog.CountAliveGroupMembersInVehicle(group, runtime.GetVehicle());
			AICF_Stage3Diagnostics.Warning(
				"BOARDING_ROLE_RESET",
				string.Format(
					"%1 phase=DRIVER mounted_before=%2 forced=%3 remaining=%4 interrupted_actions=%5 reset_timeout_ms=%6 occupants=[%7]",
					runtime.DescribeContext("MOUNTED_WITHOUT_DRIVER"),
					mounted,
					forced,
					remaining,
					interrupted,
					GetBoardingRoleResetTimeoutMs(),
					m_Watchdog.DescribeGroupVehicleOccupants(group, runtime)));
			if (remaining <= 0)
				IssueBoardingRoleRetry(runtime, faction, slot);
			return;
		}

		if (runtime.IsBoardingRoleRetryIssued())
		{
			RejectBoardingRoleViolation(runtime, faction, slot, "ROLE_RETRY_OCCUPIED_NON_DRIVER");
			return;
		}

		int remainingMounted = m_Watchdog.CountAliveGroupMembersInVehicle(group, runtime.GetVehicle());
		if (remainingMounted <= 0)
		{
			IssueBoardingRoleRetry(runtime, faction, slot);
			return;
		}

		if (runtime.GetBoardingRoleResetAgeMs() >= GetBoardingRoleResetTimeoutMs())
			RejectBoardingRoleViolation(runtime, faction, slot, "ROLE_RESET_DEADLINE_EXCEEDED");
	}

	protected void IssueBoardingRoleRetry(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		m_Watchdog.ResetGroupVehicleActions(slot.GetGroup());
		if (!StartBoardingPhase(runtime, slot, AICF_EVehicleBoardingPhase.DRIVER))
		{
			RejectBoardingRoleViolation(runtime, faction, slot, "ROLE_RETRY_ACTION_FAILED");
			return;
		}

		runtime.MarkBoardingRoleRetryIssued();
		AICF_Stage3Diagnostics.Info(
			"BOARDING_ROLE_RETRY",
			string.Format("%1 phase=DRIVER allowance=PILOT_ONLY", runtime.DescribeContext("ROLE_RESET_COMPLETE")));
	}

	protected void RejectBoardingRoleViolation(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string cause)
	{
		LatchAcceptanceFailure(runtime, "BOARDING_ROLE_VIOLATION");
		AICF_Stage3Diagnostics.Warning(
			"BOARDING_ROLE_VIOLATION",
			string.Format(
				"%1 cause=%2 reset_age_ms=%3 retry_issued=%4 occupants=[%5]",
				runtime.DescribeContext("ROLE_ORDERING_FAILED"),
				cause,
				runtime.GetBoardingRoleResetAgeMs(),
				runtime.IsBoardingRoleRetryIssued(),
				m_Watchdog.DescribeGroupVehicleOccupants(slot.GetGroup(), runtime)));
		BeginFallback(runtime, faction, slot, "BOARDING_ROLE_VIOLATION");
	}

	protected int GetBoardingRoleResetTimeoutMs()
	{
		int timeoutMs = m_Config.GetBoardingTimeoutMs() / 2;
		if (timeoutMs > 10000)
			timeoutMs = 10000;
		if (timeoutMs < 1000)
			timeoutMs = 1000;
		return timeoutMs;
	}

	protected bool ContinueRoleOrderedBoarding(
		AICF_VehicleRuntime runtime,
		AICF_GroupSlot slot,
		out string failureReason)
	{
		failureReason = string.Empty;
		SCR_AIGroup group = slot.GetGroup();
		Vehicle vehicle = runtime.GetVehicle();
		if (!group || !vehicle)
		{
			failureReason = "BOARDING_INPUT_INVALID";
			return false;
		}

		IEntity driver = m_Watchdog.ResolveAliveDriver(runtime);
		bool driverSettled = m_Watchdog.IsAliveGroupMember(group, driver) &&
			m_Watchdog.IsMemberSettledInVehicle(driver, vehicle);
		if (!driverSettled)
		{
			if (!runtime.IsBoardingDriverPhasePlanned())
			{
				failureReason = "DRIVER_LOST_BEFORE_ROLE_PHASE";
				return false;
			}
			if (!StartBoardingPhase(runtime, slot, AICF_EVehicleBoardingPhase.DRIVER))
			{
				failureReason = "DRIVER_BOARDING_ACTION_FAILED";
				return false;
			}
			return true;
		}

		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
		{
			IEntity gunner = m_Watchdog.ResolveAliveGunner(runtime);
			bool gunnerSettled = m_Watchdog.IsAliveGroupMember(group, gunner) &&
				m_Watchdog.IsMemberSettledInVehicle(gunner, vehicle);
			if (!gunnerSettled)
			{
				if (!runtime.IsBoardingGunnerPhasePlanned())
				{
					failureReason = "GUNNER_LOST_BEFORE_ROLE_PHASE";
					return false;
				}
				if (!StartBoardingPhase(runtime, slot, AICF_EVehicleBoardingPhase.GUNNER))
				{
					failureReason = "GUNNER_BOARDING_ACTION_FAILED";
					return false;
				}
				return true;
			}
		}

		if (!StartBoardingPhase(runtime, slot, AICF_EVehicleBoardingPhase.PASSENGERS))
		{
			failureReason = "PASSENGER_BOARDING_WAYPOINT_FAILED";
			return false;
		}
		return true;
	}

	protected bool StartBoardingPhase(
		AICF_VehicleRuntime runtime,
		AICF_GroupSlot slot,
		AICF_EVehicleBoardingPhase phase)
	{
		if (!runtime || !slot || !slot.GetGroup() || !runtime.GetVehicle())
			return false;

		AICF_EVehicleBoardingPhase previousPhase = runtime.GetBoardingPhase();
		DeleteRuntimeWaypoint(runtime);
		CancelCrewRecovery(runtime);
		SCR_BoardingEntityWaypoint waypoint;
		string allowance;
		if (phase == AICF_EVehicleBoardingPhase.APPROACH)
		{
			// No vehicle utility and no GetIn action are allowed before every
			// living member is inside the stock 100 m hard-search radius.
			DetachVehicleFromGroup(runtime);
			AIWaypoint approachWaypoint = m_WaypointFactory.CreateBoardingApproachWaypoint(runtime.GetVehicle());
			if (!approachWaypoint)
				return false;
			slot.GetGroup().AddWaypointAt(approachWaypoint, 0);
			runtime.SetActiveWaypoint(approachWaypoint);
			allowance = "MOVE_ONLY_NO_VEHICLE_UTILITY";
		}
		else if (phase == AICF_EVehicleBoardingPhase.DRIVER)
		{
			IEntity currentGunner = m_Watchdog.ResolveAliveGunner(runtime);
			if (!m_Watchdog.IsAliveGroupMember(slot.GetGroup(), currentGunner))
				currentGunner = null;
			AIAgent driverAgent = SelectCrewRecoveryAgent(slot.GetGroup(), runtime.GetLastDriver(), currentGunner);
			SCR_AIGetInVehicle driverAction = CreateCrewRecoveryAction(runtime, driverAgent, EAICompartmentType.Pilot);
			if (!driverAgent || !driverAction)
				return false;
			runtime.TrackCrewRecovery(driverAgent, driverAction);
			allowance = "PILOT_EXACT_ACTION";
		}
		else if (phase == AICF_EVehicleBoardingPhase.GUNNER)
		{
			IEntity currentDriver = m_Watchdog.ResolveAliveDriver(runtime);
			if (!m_Watchdog.IsAliveGroupMember(slot.GetGroup(), currentDriver))
				currentDriver = null;
			AIAgent gunnerAgent = SelectCrewRecoveryAgent(slot.GetGroup(), runtime.GetLastGunner(), currentDriver);
			SCR_AIGetInVehicle gunnerAction = CreateCrewRecoveryAction(runtime, gunnerAgent, EAICompartmentType.Turret);
			if (!gunnerAgent || !gunnerAction)
				return false;
			runtime.TrackCrewRecovery(gunnerAgent, gunnerAction);
			allowance = "TURRET_EXACT_ACTION";
		}
		else if (phase == AICF_EVehicleBoardingPhase.PASSENGERS)
		{
			if (!AttachVehicleToGroup(runtime, slot))
				return false;
			int passengerCount = Math.Max(
				0,
				AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) -
				m_Watchdog.CountAliveGroupMembersInVehicle(slot.GetGroup(), runtime.GetVehicle()));
			if (passengerCount == 0)
			{
				runtime.SetBoardingPhase(phase);
				runtime.ResetBoardingSettledPolls();
				if (previousPhase != phase)
					runtime.RestartPhaseDeadline();
				AICF_Stage3Diagnostics.Info(
					"BOARDING_PHASE_STARTED",
					string.Format(
						"%1 phase=%2 allowance=NO_CARGO_REQUIRED phase_timeout_ms=%3",
						runtime.DescribeContext("ROLE_SEAT_RESERVATION"),
						typename.EnumToString(AICF_EVehicleBoardingPhase, phase),
						m_Config.GetBoardingTimeoutMs()));
				AICF_Stage3Diagnostics.Info(
					"PASSENGERS_ASSIGNED",
					string.Format("%1 requested=0 policy=NO_CARGO_REQUIRED", runtime.DescribeContext("ROLE_ORDERED_GET_IN")));
				return true;
			}

			waypoint = m_WaypointFactory.CreatePassengerBoardingWaypoint(runtime.GetVehicle());
			allowance = "CARGO_ONLY";
			if (waypoint)
			{
				AICF_Stage3Diagnostics.Info(
					"PASSENGERS_ASSIGNED",
					string.Format("%1 requested=%2 policy=CARGO_ONLY_AFTER_CREW", runtime.DescribeContext("ROLE_ORDERED_GET_IN"), passengerCount));
			}
		}
		else
		{
			return false;
		}

		if (phase == AICF_EVehicleBoardingPhase.PASSENGERS)
		{
			if (!waypoint)
				return false;
			slot.GetGroup().AddWaypointAt(waypoint, 0);
			runtime.SetActiveWaypoint(waypoint);
		}
		runtime.SetBoardingPhase(phase);
		runtime.ResetBoardingSettledPolls();
		if (previousPhase != phase)
			runtime.RestartPhaseDeadline();
		AICF_Stage3Diagnostics.Info(
			"BOARDING_PHASE_STARTED",
			string.Format(
				"%1 phase=%2 allowance=%3 phase_timeout_ms=%4",
				runtime.DescribeContext("ROLE_SEAT_RESERVATION"),
				typename.EnumToString(AICF_EVehicleBoardingPhase, phase),
				allowance,
				m_Config.GetBoardingTimeoutMs()));
		return true;
	}

	protected bool AttachVehicleToGroup(AICF_VehicleRuntime runtime, AICF_GroupSlot slot)
	{
		if (!runtime || !runtime.GetVehicleUsage() || !slot || !slot.GetGroup())
			return false;

		SCR_AIGroupUtilityComponent groupUtility = slot.GetGroup().GetGroupUtilityComponent();
		if (!groupUtility)
			return false;

		groupUtility.AddUsableVehicle(runtime.GetVehicleUsage());
		return true;
	}

	protected void StartMovement(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string reason)
	{
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (!target || !target.GetOwner() || (slot.GetRole() == AICF_EGroupRole.ATTACK && target.GetFaction() == faction))
		{
			BeginFallback(runtime, faction, slot, "TARGET_INVALID_BEFORE_MOVE");
			return;
		}

		runtime.SetTargetBase(target);
		// Route to a road-reachable point near the real objective and stop early
		// by measured target distance. This avoids relying on an arbitrary radial
		// surface point, whose reachability was not observable in Transport T2.
		vector targetPosition;
		if (!m_OrderPlanner.TryResolveTargetPosition(target, slot.GetRole(), targetPosition))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_TARGET_POSITION_UNAVAILABLE");
			return;
		}
		vector routeEndpoint;
		string routeMode;
		AIWaypoint moveWaypoint = m_WaypointFactory.CreateMoveWaypoint(
			runtime.GetVehicle().GetOrigin(),
			targetPosition,
			m_Config.GetDismountDistanceMeters(),
			routeEndpoint,
			routeMode);
		if (!moveWaypoint)
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_ROUTE_WAYPOINT_FAILED");
			return;
		}

		slot.GetGroup().AddWaypointAt(moveWaypoint, 0);
		runtime.SetActiveWaypoint(moveWaypoint);
		runtime.SetState(AICF_EVehicleState.MOVING);
		runtime.ObserveProgress(
			Math.Sqrt(vector.DistanceSqXZ(runtime.GetVehicle().GetOrigin(), routeEndpoint)),
			m_Config.GetProgressMeters());
		runtime.ObserveMotion(runtime.GetVehicle().GetOrigin(), m_Config.GetMotionMeters());
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_ROUTE_ASSIGNED",
			string.Format(
				"%1 target=%2 route_mode=%3 endpoint_offset_m=%4 dismount_distance_m=%5",
				runtime.DescribeContext(reason),
				AICF_Stage1Diagnostics.BaseKey(target),
				routeMode,
				Math.Sqrt(vector.DistanceSqXZ(routeEndpoint, targetPosition)),
				m_Config.GetDismountDistanceMeters()));
	}

	protected void ProcessMoving(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (m_Watchdog.IsDestroyed(runtime))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_DESTROYED");
			return;
		}
		if (m_Watchdog.IsOnFire(runtime))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_ON_FIRE");
			return;
		}
		if (m_Watchdog.IsOverturned(runtime))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_OVERTURNED");
			return;
		}

		// Commander retargeting must reuse the current vehicle. Abandoning it and
		// requesting another one would violate both the cap and the fallback
		// contract, while following the old waypoint would stall the campaign.
		SCR_CampaignMilitaryBaseComponent assignedTarget = slot.GetTargetBase();
		if (assignedTarget && assignedTarget != runtime.GetTargetBase())
		{
			if (!assignedTarget.GetOwner() ||
				(slot.GetRole() == AICF_EGroupRole.ATTACK && assignedTarget.GetFaction() == faction))
			{
				BeginFallback(runtime, faction, slot, "STRATEGIC_TARGET_CHANGED_INVALID");
				return;
			}

			vector reassignedTargetPosition;
			if (!m_OrderPlanner.TryResolveTargetPosition(assignedTarget, slot.GetRole(), reassignedTargetPosition))
			{
				BeginFallback(runtime, faction, slot, "STRATEGIC_TARGET_CHANGED_POSITION_UNAVAILABLE");
				return;
			}

			runtime.SetTargetBase(assignedTarget);
			if (vector.DistanceSqXZ(runtime.GetVehicle().GetOrigin(), reassignedTargetPosition) <=
				m_Config.GetDismountDistanceMeters() * m_Config.GetDismountDistanceMeters())
			{
				BeginDismount(runtime, faction, slot, "STRATEGIC_TARGET_CHANGED_WITHIN_DISMOUNT_RANGE");
				return;
			}

			DeleteRuntimeWaypoint(runtime);
			StartMovement(runtime, faction, slot, "STRATEGIC_TARGET_CHANGED");
			return;
		}
		if (!runtime.GetTargetBase() || runtime.GetTargetBase().GetFaction() == faction)
		{
			BeginFallback(runtime, faction, slot, "STRATEGIC_TARGET_CHANGED");
			return;
		}
		vector targetPosition;
		if (!m_OrderPlanner.TryResolveTargetPosition(runtime.GetTargetBase(), slot.GetRole(), targetPosition))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_TARGET_POSITION_UNAVAILABLE");
			return;
		}

		IEntity driver = m_Watchdog.ResolveAliveDriver(runtime);
		if (!m_Watchdog.IsAliveGroupMember(slot.GetGroup(), driver))
			driver = null;
		if (!driver)
		{
			BeginDriverRecovery(runtime, faction, slot);
			return;
		}
		IEntity movingGunner;
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
			movingGunner = m_Watchdog.ResolveAliveGunner(runtime);
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT &&
			!m_Watchdog.IsAliveGroupMember(slot.GetGroup(), movingGunner))
		{
			BeginGunnerRecovery(runtime, faction, slot);
			return;
		}

		if (!m_Watchdog.IsGroupCohesiveAroundVehicle(slot.GetGroup(), runtime.GetVehicle(), m_Config.GetCohesionDistanceMeters()))
		{
			BeginFallback(runtime, faction, slot, "GROUP_COHESION_EXCEEDED");
			return;
		}

		AIWaypoint moveWaypoint = runtime.GetActiveWaypoint();
		if (!moveWaypoint)
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_ROUTE_REFERENCE_LOST");
			return;
		}

		float targetDistanceMeters = Math.Sqrt(vector.DistanceSqXZ(
			runtime.GetVehicle().GetOrigin(),
			targetPosition));
		if (targetDistanceMeters <= m_Config.GetDismountDistanceMeters())
		{
			BeginDismount(runtime, faction, slot, "DISEMBARK_POSITION_REACHED");
			return;
		}

		float routeDistanceMeters = Math.Sqrt(vector.DistanceSqXZ(
			runtime.GetVehicle().GetOrigin(),
			moveWaypoint.GetOrigin()));
		bool routeProgress = runtime.ObserveProgress(routeDistanceMeters, m_Config.GetProgressMeters());
		bool physicalMotion = runtime.ObserveMotion(runtime.GetVehicle().GetOrigin(), m_Config.GetMotionMeters());
		if (routeProgress)
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_PROGRESS",
				string.Format(
					"%1 route_distance_m=%2 target_distance_m=%3",
					runtime.DescribeContext("DISTANCE_REDUCED"),
					routeDistanceMeters,
					targetDistanceMeters));
			if (runtime.HasPendingRouteRecovery())
			{
				runtime.ConfirmRouteRecovery();
				AICF_Stage3Diagnostics.Info("VEHICLE_RECOVERY_SUCCEEDED", runtime.DescribeContext("ROUTE_PROGRESS_RESTORED"));
			}
		}
		else if (physicalMotion)
		{
			if (runtime.MarkMotionReportDue(MOTION_REPORT_INTERVAL_MS))
			{
				AICF_Stage3Diagnostics.Info(
					"VEHICLE_MOTION",
					string.Format(
						"%1 route_distance_m=%2 target_distance_m=%3",
						runtime.DescribeContext("PHYSICAL_MOVEMENT_WITHOUT_ROUTE_REDUCTION"),
						routeDistanceMeters,
						targetDistanceMeters));
			}
			if (runtime.HasPendingRouteRecovery() && !runtime.RecoveryRequiresRouteProgress())
			{
				runtime.ConfirmRouteRecovery();
				AICF_Stage3Diagnostics.Info("VEHICLE_RECOVERY_SUCCEEDED", runtime.DescribeContext("PHYSICAL_MOVEMENT_RESTORED"));
			}
		}

		bool stationary = runtime.IsStationary(m_Config.GetStuckTimeoutMs());
		bool routeStalled = runtime.IsRouteStalled(m_Config.GetObjectiveProgressTimeoutMs());
		if (!stationary && !routeStalled)
			return;

		// VehicleCanMove() is a movement-damage threshold, not a measurement of
		// physical motion. It only explains a confirmed no-progress timeout and
		// must never bypass the configured stuck/recovery contract.
		bool movementUsable = m_Watchdog.CanMove(runtime);
		string stuckReason = "NO_OBJECTIVE_PROGRESS";
		if (stationary)
			stuckReason = "NO_PHYSICAL_MOVEMENT";
		if (stationary && !movementUsable)
			stuckReason = "MOVEMENT_DAMAGE_WITHOUT_PROGRESS";
		string stuckDetails = string.Format(
			"%1 route_distance_m=%2 target_distance_m=%3 route_progress_age_ms=%4 motion_age_ms=%5 movement_damage=%6 movement_usable=%7",
			runtime.DescribeContext(stuckReason),
			routeDistanceMeters,
			targetDistanceMeters,
			runtime.GetRouteProgressAgeMs(),
			runtime.GetMotionAgeMs(),
			m_Watchdog.GetMovementDamage(runtime),
			movementUsable);
		stuckDetails += string.Format(
			" stationary_timeout_ms=%1 objective_timeout_ms=%2 attempt=%3",
			m_Config.GetStuckTimeoutMs(),
			m_Config.GetObjectiveProgressTimeoutMs(),
			runtime.GetRecoveryCount() + 1);
		AICF_Stage3Diagnostics.Warning("VEHICLE_STUCK_DETECTED", stuckDetails);
		if (stationary && !movementUsable)
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_RECOVERY_MOBILITY_UNAVAILABLE");
			return;
		}
		if (runtime.GetRecoveryCount() >= m_Config.GetMaxRecoveries())
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_STUCK_PERSISTENT");
			return;
		}
		AICF_Stage3Diagnostics.Info("VEHICLE_RECOVERY_STARTED", runtime.DescribeContext(stuckReason));

		vector destination = targetPosition;
		vector routeEndpoint;
		string routeMode;
		DeleteRuntimeWaypoint(runtime);
		AIWaypoint rebuilt = m_WaypointFactory.CreateMoveWaypoint(
			runtime.GetVehicle().GetOrigin(),
			destination,
			m_Config.GetDismountDistanceMeters(),
			routeEndpoint,
			routeMode);
		if (!rebuilt)
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_ROUTE_RECOVERY_FAILED");
			return;
		}

		slot.GetGroup().AddWaypointAt(rebuilt, 0);
		runtime.SetActiveWaypoint(rebuilt);
		runtime.RecordRecovery(
			Math.Sqrt(vector.DistanceSqXZ(runtime.GetVehicle().GetOrigin(), routeEndpoint)),
			runtime.GetVehicle().GetOrigin(),
			routeStalled && !stationary);
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_STUCK_RECOVERY",
			string.Format(
				"%1 action=REBUILD_ROUTE attempt=%2 route_mode=%3 endpoint_offset_m=%4",
				runtime.DescribeContext("REBUILD_ROUTE"),
				runtime.GetRecoveryCount(),
				routeMode,
				Math.Sqrt(vector.DistanceSqXZ(routeEndpoint, destination))));
	}

	protected void BeginDriverRecovery(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (runtime.GetRecoveryCount() >= m_Config.GetMaxRecoveries())
		{
			BeginFallback(runtime, faction, slot, "DRIVER_RECOVERY_EXHAUSTED");
			return;
		}

		DeleteRuntimeWaypoint(runtime);
		CancelCrewRecovery(runtime);
		IEntity gunner;
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
		{
			gunner = m_Watchdog.ResolveAliveGunner(runtime);
			if (!m_Watchdog.IsAliveGroupMember(slot.GetGroup(), gunner))
				gunner = null;
		}
		AIAgent recoveryAgent = SelectCrewRecoveryAgent(slot.GetGroup(), runtime.GetLastDriver(), gunner);
		SCR_AIGetInVehicle recoveryAction = CreateCrewRecoveryAction(
			runtime,
			recoveryAgent,
			EAICompartmentType.Pilot);
		if (!recoveryAgent || !recoveryAction)
		{
			BeginFallback(runtime, faction, slot, "DRIVER_RECOVERY_AGENT_UNAVAILABLE");
			return;
		}

		runtime.TrackCrewRecovery(recoveryAgent, recoveryAction);
		runtime.SetCrewRecoveryPhase(AICF_EVehicleCrewRecoveryPhase.DRIVER);
		runtime.RecordCrewRecovery();
		runtime.SetState(AICF_EVehicleState.RECOVERING);
		runtime.RestartPhaseDeadline();
		AICF_Stage3Diagnostics.Warning("DRIVER_LOST", runtime.DescribeContext("PILOT_COMPARTMENT_EMPTY_OR_DRIVER_DEAD"));
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_RECOVERY_STARTED",
			string.Format("%1 agent=%2 role=PILOT mode=DIRECT_ROLE_ACTION", runtime.DescribeContext("REASSIGN_DRIVER"), recoveryAgent.GetControlledEntity().GetID()));
	}

	protected AIAgent SelectCrewRecoveryAgent(
		SCR_AIGroup group,
		IEntity preferredEntity,
		IEntity excludedEntity = null)
	{
		if (!group)
			return null;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		AIAgent firstAvailable;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity) || entity == excludedEntity)
				continue;
			if (entity == preferredEntity)
				return agent;
			if (!firstAvailable)
				firstAvailable = agent;
		}

		return firstAvailable;
	}

	protected SCR_AIGetInVehicle CreateCrewRecoveryAction(
		AICF_VehicleRuntime runtime,
		AIAgent recoveryAgent,
		EAICompartmentType role)
	{
		if (!runtime || !runtime.GetVehicle() || !runtime.GetVehicleUsage() || !recoveryAgent)
			return null;

		IEntity recoveryEntity = recoveryAgent.GetControlledEntity();
		if (!AICF_GroupRuntime.IsAliveCharacter(recoveryEntity))
			return null;

		BaseCompartmentSlot roleSlot;
		if (role == EAICompartmentType.Pilot)
			roleSlot = runtime.GetVehicleUsage().GetPilotCompartmentSlot();
		else if (role == EAICompartmentType.Turret)
			roleSlot = runtime.GetVehicleUsage().GetTurretCompartmentSlot();
		if (!roleSlot || !roleSlot.IsCompartmentAccessible() || roleSlot.IsReserved())
			return null;

		IEntity occupant = roleSlot.GetOccupant();
		if (occupant == recoveryEntity)
			return null;
		if (occupant && SCR_AIDamageHandling.IsConscious(occupant))
			return null;

		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			recoveryAgent.FindComponent(SCR_AIUtilityComponent));
		if (!utility)
			return null;

		roleSlot.SetReserved(recoveryEntity);
		SCR_AIGetInVehicle action = new SCR_AIGetInVehicle(
			utility,
			null,
			runtime.GetVehicle(),
			roleSlot,
			role,
			SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE,
			SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
		utility.AddAction(action);
		return action;
	}

	protected void CancelCrewRecovery(AICF_VehicleRuntime runtime)
	{
		if (!runtime)
			return;

		SCR_AIGetInVehicle action = runtime.GetCrewRecoveryAction();
		if (action)
		{
			EAIActionState state = action.GetActionState();
			if (state != EAIActionState.COMPLETED && state != EAIActionState.FAILED)
				action.Fail();
		}
		runtime.ClearCrewRecoveryTracking();
	}

	protected void BeginGunnerRecovery(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (runtime.GetRecoveryCount() >= m_Config.GetMaxRecoveries())
		{
			BeginFallback(runtime, faction, slot, "GUNNER_RECOVERY_EXHAUSTED");
			return;
		}

		DeleteRuntimeWaypoint(runtime);
		CancelCrewRecovery(runtime);
		IEntity driver = m_Watchdog.ResolveAliveDriver(runtime);
		if (!m_Watchdog.IsAliveGroupMember(slot.GetGroup(), driver))
			driver = null;
		AIAgent recoveryAgent = SelectCrewRecoveryAgent(slot.GetGroup(), runtime.GetLastGunner(), driver);
		SCR_AIGetInVehicle recoveryAction = CreateCrewRecoveryAction(
			runtime,
			recoveryAgent,
			EAICompartmentType.Turret);
		if (!recoveryAgent || !recoveryAction)
		{
			BeginFallback(runtime, faction, slot, "GUNNER_RECOVERY_AGENT_UNAVAILABLE");
			return;
		}

		runtime.TrackCrewRecovery(recoveryAgent, recoveryAction);
		runtime.SetCrewRecoveryPhase(AICF_EVehicleCrewRecoveryPhase.GUNNER);
		runtime.RecordCrewRecovery();
		runtime.SetState(AICF_EVehicleState.RECOVERING);
		runtime.RestartPhaseDeadline();
		AICF_Stage3Diagnostics.Warning("GUNNER_LOST", runtime.DescribeContext("TURRET_COMPARTMENT_EMPTY_OR_GUNNER_DEAD"));
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_RECOVERY_STARTED",
			string.Format("%1 agent=%2 role=TURRET mode=DIRECT_ROLE_ACTION", runtime.DescribeContext("REASSIGN_GUNNER"), recoveryAgent.GetControlledEntity().GetID()));
	}

	protected void ProcessDriverRecovery(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (m_Watchdog.IsDestroyed(runtime) || m_Watchdog.IsOnFire(runtime) || m_Watchdog.IsOverturned(runtime))
		{
			BeginFallback(runtime, faction, slot, "DRIVER_RECOVERY_VEHICLE_UNUSABLE");
			return;
		}

		AICF_EVehicleCrewRecoveryPhase recoveryPhase = runtime.GetCrewRecoveryPhase();
		IEntity driver = m_Watchdog.ResolveAliveDriver(runtime);
		if (!m_Watchdog.IsAliveGroupMember(slot.GetGroup(), driver))
			driver = null;
		bool driverSettled = driver && m_Watchdog.IsMemberSettledInVehicle(driver, runtime.GetVehicle());
		if (!driverSettled && recoveryPhase == AICF_EVehicleCrewRecoveryPhase.GUNNER)
		{
			// Driver loss while the turret is being recovered invalidates the whole
			// crew set. Cancel the turret token and restore the pilot first.
			BeginDriverRecovery(runtime, faction, slot);
			return;
		}

		IEntity gunner;
		bool gunnerSettled;
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
		{
			gunner = m_Watchdog.ResolveAliveGunner(runtime);
			if (!m_Watchdog.IsAliveGroupMember(slot.GetGroup(), gunner))
				gunner = null;
			gunnerSettled = gunner && m_Watchdog.IsMemberSettledInVehicle(gunner, runtime.GetVehicle());
		}
		if (driverSettled && recoveryPhase == AICF_EVehicleCrewRecoveryPhase.DRIVER)
		{
			runtime.ClearCrewRecoveryTracking();
			runtime.SetLastDriver(driver);
			AICF_Stage3Diagnostics.Info(
				"DRIVER_REASSIGNED",
				string.Format("%1 driver=%2", runtime.DescribeContext("RECOVERY_ROLE_CONFIRMED"), driver.GetID()));
			if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT && !gunnerSettled)
			{
				BeginGunnerRecovery(runtime, faction, slot);
				return;
			}
		}

		if (gunnerSettled && recoveryPhase == AICF_EVehicleCrewRecoveryPhase.GUNNER)
		{
			runtime.ClearCrewRecoveryTracking();
			runtime.SetLastGunner(gunner);
			AICF_Stage3Diagnostics.Info(
				"GUNNER_REASSIGNED",
				string.Format("%1 gunner=%2", runtime.DescribeContext("RECOVERY_ROLE_CONFIRMED"), gunner.GetID()));
		}

		if (driverSettled && (runtime.GetKind() != AICF_EVehicleKind.ARMED_LIGHT || gunnerSettled))
		{
			// Recovery succeeds only when the complete mandatory crew set is
			// physically occupied by alive members of this group.
			runtime.SetLastDriver(driver);
			if (gunner)
				runtime.SetLastGunner(gunner);
			DeleteRuntimeWaypoint(runtime);
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_RECOVERY_SUCCEEDED",
				runtime.DescribeContext("ALL_REQUIRED_CREW_RESTORED"));
			StartMovement(runtime, faction, slot, "ALL_REQUIRED_CREW_RESTORED");
			return;
		}

		if (System.GetTickCount(runtime.GetStateStartedAtMs()) >= m_Config.GetBoardingTimeoutMs())
		{
			string timeoutReason = "GUNNER_RECOVERY_TIMEOUT";
			if (recoveryPhase == AICF_EVehicleCrewRecoveryPhase.DRIVER)
				timeoutReason = "DRIVER_RECOVERY_TIMEOUT";
			BeginFallback(runtime, faction, slot, timeoutReason);
		}
	}

	protected void BeginDismount(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string reason)
	{
		runtime.ResetDismountReissue();
		int interrupted = m_Watchdog.ResetGroupVehicleActions(slot.GetGroup());
		if (!IssueDismountWaypoint(runtime, slot))
		{
			BeginFallback(runtime, faction, slot, "DISEMBARK_WAYPOINT_FAILED");
			return;
		}

		runtime.SetState(AICF_EVehicleState.DISEMBARKING);
		AICF_Stage3Diagnostics.Info(
			"DISEMBARK_STARTED",
			string.Format(
				"%1 interrupted_actions=%2 occupants=[%3]",
				runtime.DescribeContext(reason),
				interrupted,
				m_Watchdog.DescribeGroupVehicleOccupants(slot.GetGroup(), runtime)));
	}

	protected void ProcessDismount(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (!runtime || !slot || !slot.GetGroup() || !runtime.GetVehicle())
		{
			if (runtime)
			{
				LatchAcceptanceFailure(runtime, "DISEMBARK_INPUT_INVALID");
				AICF_Stage3Diagnostics.Error(
					"DISEMBARK_INPUT_INVALID",
					runtime.DescribeContext("GROUP_OR_VEHICLE_MISSING"));
				BeginFallback(runtime, faction, slot, "DISEMBARK_INPUT_INVALID");
			}
			return;
		}

		int logicalOccupants;
		int transitions;
		int insideBounds;
		string clearanceSamples;
		bool safelyClear = m_Watchdog.InspectProtectedMemberDismountClearance(
			slot.GetGroup(),
			runtime.GetVehicle(),
			logicalOccupants,
			transitions,
			insideBounds,
			clearanceSamples);
		bool physicalOnlyBlocked = !safelyClear && logicalOccupants == 0 && transitions == 0 && insideBounds > 0;
		int clearPolls = runtime.ObserveDismountClearance(safelyClear, physicalOnlyBlocked);
		if (safelyClear && clearPolls >= DISMOUNT_CLEAR_POLLS_REQUIRED)
		{
			CompleteDismount(runtime, faction, slot);
			return;
		}

		// A compartment link can disappear before the get-out animation has moved
		// the character clear of the vehicle. T6 proved that treating this edge as
		// completion can strand a soldier in the cargo bed indefinitely. Recover a
		// logically-out but physically trapped member once, then require two fresh
		// safe-clear observations before completing the trip.
		if (physicalOnlyBlocked &&
			runtime.GetDismountClearanceBlockedAgeMs() >= DISMOUNT_CLEARANCE_RECOVERY_DELAY_MS &&
			runtime.CanAttemptDismountClearanceRecovery(DISMOUNT_CLEARANCE_RECOVERY_MAX_ATTEMPTS))
		{
			int relocated = RelocateTrappedDismountedMembers(slot.GetGroup(), runtime.GetVehicle());
			runtime.RecordDismountClearanceRecoveryAttempt(relocated > 0);
			AICF_Stage3Diagnostics.Warning(
				"DISEMBARK_CLEARANCE_RECOVERY",
				string.Format(
					"%1 relocated=%2 attempt=%3 maximum_attempts=%4 inside_bounds=%5 samples=[%6]",
					runtime.DescribeContext("LOGICALLY_OUT_BUT_INSIDE_VEHICLE_BOUNDS"),
					relocated,
					runtime.GetDismountClearanceRecoveryAttempts(),
					DISMOUNT_CLEARANCE_RECOVERY_MAX_ATTEMPTS,
					insideBounds,
					clearanceSamples));
		}

		int dismountAgeMs = System.GetTickCount(runtime.GetStateStartedAtMs());
		int dismountTimeoutMs = m_Config.GetBoardingTimeoutMs();
		if (dismountAgeMs >= dismountTimeoutMs)
		{
			AICF_Stage3Diagnostics.Warning(
				"DISEMBARK_TIMEOUT",
				string.Format(
					"%1 age_ms=%2 timeout_ms=%3 reissue_attempted=%4 logical=%5 transitions=%6 inside_bounds=%7 clearance_recovery_attempts=%8 samples=[%9]",
					runtime.DescribeContext("PROTECTED_OCCUPANTS_REMAIN"),
					dismountAgeMs,
					dismountTimeoutMs,
					runtime.IsDismountReissueAttempted(),
					logicalOccupants,
					transitions,
					insideBounds,
					runtime.GetDismountClearanceRecoveryAttempts(),
					clearanceSamples));
			LatchAcceptanceFailure(runtime, "DISEMBARK_TIMEOUT");
			BeginFallback(runtime, faction, slot, "DISEMBARK_TIMEOUT");
			return;
		}

		if (!runtime.IsDismountReissueAttempted() && dismountAgeMs >= dismountTimeoutMs / 2)
		{
			runtime.MarkDismountReissueAttempted();
			string occupantSamplesBefore = m_Watchdog.DescribeGroupVehicleOccupants(slot.GetGroup(), runtime);
			int interrupted = m_Watchdog.ResetGroupVehicleActions(slot.GetGroup());
			if (!IssueDismountWaypoint(runtime, slot))
			{
				BeginFallback(runtime, faction, slot, "DISEMBARK_REISSUE_WAYPOINT_FAILED");
				return;
			}

			AICF_Stage3Diagnostics.Warning(
				"DISEMBARK_REISSUED",
				string.Format(
					"%1 age_ms=%2 timeout_ms=%3 interrupted_actions=%4 occupants=[%5]",
					runtime.DescribeContext("HALF_DEADLINE_REISSUE"),
					dismountAgeMs,
					dismountTimeoutMs,
					interrupted,
					occupantSamplesBefore));
		}
	}

	protected int RelocateTrappedDismountedMembers(SCR_AIGroup group, Vehicle vehicle)
	{
		if (!group || !vehicle || !Replication.IsServer())
			return 0;

		vector boundsMin;
		vector boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);
		float clearanceRadius = Math.Max(
			Math.Max(Math.AbsFloat(boundsMin[0]), Math.AbsFloat(boundsMax[0])),
			Math.Max(Math.AbsFloat(boundsMin[2]), Math.AbsFloat(boundsMax[2]))) + 2.0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int relocated;
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group)
				continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!AICF_GroupRuntime.IsAliveCharacter(character))
				continue;

			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (CompartmentAccessComponent.GetVehicleIn(character) == vehicle || character.IsInVehicle() ||
				(access && (access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut())))
			{
				continue;
			}

			vector localOrigin = vehicle.CoordToLocal(character.GetOrigin());
			if (!IsInsideExpandedDismountBounds(localOrigin, boundsMin, boundsMax))
				continue;

			vector direction = Vector(localOrigin[0], 0, localOrigin[2]);
			if (direction.LengthSq() < 0.01)
				direction = "1 0 0";
			direction.Normalize();
			vector safePosition;
			bool found;
			for (int attempt = 0; attempt < 3; attempt++)
			{
				vector searchCenter = vehicle.CoordToParent(direction * (clearanceRadius + attempt * 3.0));
				if (!SCR_WorldTools.FindEmptyTerrainPosition(safePosition, searchCenter, 2.0, 0.5, 2.0))
					continue;
				if (IsInsideExpandedDismountBounds(vehicle.CoordToLocal(safePosition), boundsMin, boundsMax))
					continue;
				found = true;
				break;
			}
			if (!found)
				continue;

			vector transform[4];
			character.GetWorldTransform(transform);
			transform[3] = safePosition;
			character.Teleport(transform);
			relocated++;
		}

		return relocated;
	}

	protected bool IsInsideExpandedDismountBounds(vector localOrigin, vector boundsMin, vector boundsMax)
	{
		return localOrigin[0] >= boundsMin[0] - DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[0] <= boundsMax[0] + DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[1] >= boundsMin[1] - DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[1] <= boundsMax[1] + DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[2] >= boundsMin[2] - DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[2] <= boundsMax[2] + DISMOUNT_CLEARANCE_MARGIN_METERS;
	}

	protected bool IssueDismountWaypoint(AICF_VehicleRuntime runtime, AICF_GroupSlot slot)
	{
		DeleteRuntimeWaypoint(runtime);
		SCR_BoardingWaypoint waypoint = m_WaypointFactory.CreateDismountWaypoint(runtime.GetVehicle());
		if (!waypoint)
			return false;

		slot.GetGroup().AddWaypointAt(waypoint, 0);
		runtime.SetActiveWaypoint(waypoint);
		return true;
	}

	protected void CompleteDismount(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		DeleteRuntimeWaypoint(runtime);
		DetachVehicleFromGroup(runtime);
		runtime.SetState(AICF_EVehicleState.DISMOUNTED);
		runtime.MarkTripCompleted();
		AICF_Stage3Diagnostics.Info("DISEMBARK_COMPLETE", runtime.DescribeContext("ALL_PROTECTED_MEMBERS_SAFELY_CLEAR"));
		RestoreInfantryOrder(runtime, faction, slot, "VEHICLE_DISEMBARK_COMPLETE");
		m_CohesionPolicy.Apply(slot.GetGroup());
		MarkCompleted(runtime);
	}

	protected void ProcessDismounted(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (m_Watchdog.IsDestroyed(runtime))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_DESTROYED_AFTER_DISEMBARK");
			return;
		}

		SCR_CampaignMilitaryBaseComponent newTarget = slot.GetTargetBase();
		if (!newTarget || newTarget == runtime.GetTargetBase())
			return;

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (!leader || vector.DistanceSqXZ(leader.GetOrigin(), runtime.GetVehicle().GetOrigin()) >
			m_Config.GetMaximumReuseDistanceMeters() * m_Config.GetMaximumReuseDistanceMeters())
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_REUSE_DISTANCE_EXCEEDED");
			return;
		}

		vector targetPosition;
		if (!m_OrderPlanner.TryResolveTargetPosition(newTarget, slot.GetRole(), targetPosition) ||
			!IsRouteLongEnough(leader.GetOrigin(), targetPosition))
			return;

		runtime.BeginReuse(slot.GetSpawnGeneration(), newTarget);
		StartBoarding(runtime, faction, slot, "SAFE_REUSE");
	}

	protected void BeginFallback(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string reason)
	{
		if (!runtime || runtime.GetState() == AICF_EVehicleState.INFANTRY_FALLBACK ||
			runtime.GetState() == AICF_EVehicleState.ABANDONED || runtime.GetState() == AICF_EVehicleState.DESTROYED)
		{
			return;
		}

		CancelCrewRecovery(runtime);
		DeleteRuntimeWaypoint(runtime);
		runtime.SetTerminalReason(reason);
		if ((reason.Contains("RECOVERY") || reason.Contains("STUCK_PERSISTENT")) && runtime.MarkRecoveryFailureReported())
			AICF_Stage3Diagnostics.Warning("VEHICLE_RECOVERY_FAILED", runtime.DescribeContext(reason));
		runtime.SetState(AICF_EVehicleState.INFANTRY_FALLBACK);
		AICF_Stage3Diagnostics.Warning("INFANTRY_FALLBACK", runtime.DescribeContext(reason));

		if (slot && IsRuntimeCurrent(runtime, slot) && runtime.GetVehicle() && runtime.GetVehicle().IsOccupied())
		{
			SCR_BoardingWaypoint waypoint = m_WaypointFactory.CreateDismountWaypoint(runtime.GetVehicle());
			if (waypoint)
			{
				slot.GetGroup().AddWaypointAt(waypoint, 0);
				runtime.SetActiveWaypoint(waypoint);
			}
		}
	}

	protected void ProcessFallback(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		array<int> nextVehicleAtMs,
		array<int> nextVehicleGeneration)
	{
		CancelCrewRecovery(runtime);
		bool groupCurrent = IsRuntimeCurrent(runtime, slot);
		bool allOut = !runtime.GetVehicle() || !runtime.GetVehicle().IsOccupied();
		if (groupCurrent && runtime.GetVehicle())
			allOut = RecoverProtectedDismountClearance(
				runtime,
				slot.GetGroup(),
				runtime.GetVehicle(),
				"FALLBACK_CLEARANCE",
				false,
				DISMOUNT_CLEARANCE_TERMINAL_MAX_ATTEMPTS);

		if (!allOut && System.GetTickCount(runtime.GetStateStartedAtMs()) < m_Config.GetBoardingTimeoutMs())
			return;
		if (!allOut && groupCurrent)
		{
			int forced = ForceAliveGroupMembersOut(slot.GetGroup(), runtime.GetVehicle());
			allOut = RecoverProtectedDismountClearance(
				runtime,
				slot.GetGroup(),
				runtime.GetVehicle(),
				"FALLBACK_FORCE_CLEARANCE",
				true,
				DISMOUNT_CLEARANCE_TERMINAL_MAX_ATTEMPTS);
			if (forced > 0 && runtime.MarkFallbackForceExitReported())
			{
				AICF_Stage3Diagnostics.Warning(
					"FALLBACK_FORCE_DISEMBARK",
					string.Format("%1 forced=%2 all_out=%3", runtime.DescribeContext("ANIMATED_DISEMBARK_DEADLINE_EXCEEDED"), forced, allOut));
			}
			if (!allOut)
			{
				int fallbackAgeMs = System.GetTickCount(runtime.GetStateStartedAtMs());
				if (fallbackAgeMs < m_Config.GetBoardingTimeoutMs() * 2)
					return;
				LatchAcceptanceFailure(runtime, "FALLBACK_DISEMBARK_FAILED");
				if (runtime.MarkFallbackExitFailureReported())
				{
					AICF_Stage3Diagnostics.Error(
						"FALLBACK_DISEMBARK_FAILED",
						string.Format("%1 forced=%2 protected_occupants_remain=1", runtime.DescribeContext("FORCED_EXIT_FAILED"), forced));
				}
				DeleteRuntimeWaypoint(runtime);
				DetachVehicleFromGroup(runtime);
				string primaryTerminalReason = runtime.GetTerminalReason();
				if (primaryTerminalReason.IsEmpty())
					runtime.SetTerminalReason("FALLBACK_DISEMBARK_FAILED");
				else if (!primaryTerminalReason.Contains("FALLBACK_DISEMBARK_FAILED"))
					runtime.SetTerminalReason(primaryTerminalReason + "+FALLBACK_DISEMBARK_FAILED");
				SCR_CampaignMilitaryBaseComponent failedTarget = slot.GetTargetBase();
				if (!failedTarget)
					failedTarget = runtime.GetTargetBase();
				slot.SuppressVehicleTripForAssignment(runtime.GetGroupGeneration(), failedTarget);
				slot.RecordVehicleTerminalFailure("FALLBACK_DISEMBARK_FAILED");
				runtime.MarkInfantryFallbackRestorePending();
				runtime.SetState(AICF_EVehicleState.ABANDONED);
				runtime.ScheduleCleanup(System.GetTickCount() + m_Config.GetCleanupDelayMs());
				AICF_Stage3Diagnostics.Warning("VEHICLE_ABANDONED", runtime.DescribeContext("FALLBACK_DISEMBARK_FAILED"));
				return;
			}
		}

		DeleteRuntimeWaypoint(runtime);
		DetachVehicleFromGroup(runtime);
		if (groupCurrent)
		{
			SCR_CampaignMilitaryBaseComponent fallbackTarget = slot.GetTargetBase();
			if (!fallbackTarget)
				fallbackTarget = runtime.GetTargetBase();
			slot.SuppressVehicleTripForAssignment(runtime.GetGroupGeneration(), fallbackTarget);
			RestoreInfantryOrder(runtime, faction, slot, "VEHICLE_FALLBACK");
			m_CohesionPolicy.Apply(slot.GetGroup());
		}

		bool destroyed = runtime.GetTerminalReason().Contains("DESTROYED") || m_Watchdog.IsDestroyed(runtime);
		if (destroyed)
		{
			runtime.SetState(AICF_EVehicleState.DESTROYED);
			AICF_Stage3Diagnostics.Warning("VEHICLE_DESTROYED", runtime.DescribeContext(runtime.GetTerminalReason()));
		}
		else
		{
			runtime.SetState(AICF_EVehicleState.ABANDONED);
			AICF_Stage3Diagnostics.Warning("VEHICLE_ABANDONED", runtime.DescribeContext(runtime.GetTerminalReason()));
		}

		runtime.ScheduleCleanup(System.GetTickCount() + m_Config.GetCleanupDelayMs());
		if (slot && nextVehicleAtMs.IsIndexValid(slot.GetSlotId()))
		{
			nextVehicleAtMs[slot.GetSlotId()] = System.GetTickCount() + m_Config.GetCleanupDelayMs() + m_Config.GetRetryIntervalMs();
			if (nextVehicleGeneration.IsIndexValid(slot.GetSlotId()))
				nextVehicleGeneration[slot.GetSlotId()] = runtime.GetGroupGeneration();
		}
	}

	protected int ForceAliveGroupMembersOut(SCR_AIGroup group, Vehicle vehicle)
	{
		if (!group || !vehicle)
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int forced;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!AICF_GroupRuntime.IsAliveCharacter(character) ||
				CompartmentAccessComponent.GetVehicleIn(character) != vehicle)
			{
				continue;
			}

			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access)
				continue;
			access.InterruptVehicleActionQueue(true, true, true);
			vector teleportTransform[4];
			bool hasTeleportLocation = access.FindSuitableTeleportLocation(teleportTransform);
			if (hasTeleportLocation && access.GetOutVehicle_NoDoor(teleportTransform, false, false, true))
				forced++;
			else if (access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.INVALID, false, true))
				forced++;
		}

		return forced;
	}

	protected bool RecoverProtectedDismountClearance(
		AICF_VehicleRuntime runtime,
		SCR_AIGroup group,
		Vehicle vehicle,
		string reason,
		bool allowRelocation,
		int maximumAttempts)
	{
		if (!runtime || !group || !vehicle)
			return false;

		int logicalOccupants;
		int transitions;
		int insideBounds;
		string samples;
		bool safelyClear = m_Watchdog.InspectProtectedMemberDismountClearance(
			group,
			vehicle,
			logicalOccupants,
			transitions,
			insideBounds,
			samples);
		bool physicalOnlyBlocked = !safelyClear && logicalOccupants == 0 && transitions == 0 && insideBounds > 0;
		runtime.ObserveDismountClearance(safelyClear, physicalOnlyBlocked);
		if (safelyClear || !allowRelocation || !physicalOnlyBlocked ||
			runtime.GetDismountClearanceBlockedAgeMs() < DISMOUNT_CLEARANCE_RECOVERY_DELAY_MS ||
			!runtime.CanAttemptDismountClearanceRecovery(maximumAttempts))
		{
			return safelyClear;
		}

		int relocated = RelocateTrappedDismountedMembers(group, vehicle);
		runtime.RecordDismountClearanceRecoveryAttempt(relocated > 0);
		AICF_Stage3Diagnostics.Warning(
			"DISEMBARK_CLEARANCE_RECOVERY",
			string.Format(
				"%1 relocated=%2 attempt=%3 maximum_attempts=%4 inside_bounds=%5 samples=[%6]",
				runtime.DescribeContext(reason),
				relocated,
				runtime.GetDismountClearanceRecoveryAttempts(),
				maximumAttempts,
				insideBounds,
				samples));

		return m_Watchdog.AreAllProtectedMembersSafelyClear(group, vehicle);
	}

	protected void BeginDetachedCleanup(
		AICF_VehicleRuntime runtime,
		AICF_GroupSlot slot,
		string reason)
	{
		if (!runtime || runtime.GetState() == AICF_EVehicleState.ABANDONED ||
			runtime.GetState() == AICF_EVehicleState.DESTROYED)
		{
			return;
		}

		DeleteRuntimeWaypoint(runtime);
		DetachVehicleFromGroup(runtime);
		if (runtime.GetTerminalReason().IsEmpty())
			runtime.SetTerminalReason(reason);
		runtime.SetState(AICF_EVehicleState.ABANDONED);
		runtime.ScheduleCleanup(System.GetTickCount() + m_Config.GetCleanupDelayMs());
		if (slot)
			slot.ClearVehicleRuntime(runtime);
		AICF_Stage3Diagnostics.Warning(
			"VEHICLE_ABANDONED",
			string.Format("%1 detach_reason=%2", runtime.DescribeContext(runtime.GetTerminalReason()), reason));
	}

	protected void ProcessTerminal(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		array<ref AICF_VehicleRuntime> runtimes,
		array<int> nextVehicleAtMs,
		array<int> nextVehicleGeneration,
		int slotId)
	{
		if (!runtime)
			return;
		if (runtime.HasVehicleDeleteConfirmationPending())
		{
			ProcessVehicleDeleteConfirmation(runtime, slot, runtimes, nextVehicleAtMs, nextVehicleGeneration, slotId);
			return;
		}
		CancelCrewRecovery(runtime);
		bool groupCurrent = IsRuntimeCurrent(runtime, slot);
		if (runtime.IsInfantryFallbackRestorePending() && !runtime.GetGroup())
		{
			// There is no managed group left to receive an infantry order. Drop
			// only the restore obligation; the global protected-occupant gate below
			// still prevents deleting a vehicle occupied by any living character.
			runtime.ClearInfantryFallbackRestorePending();
			AICF_Stage3Diagnostics.Warning(
				"INFANTRY_FALLBACK_RESTORE_SKIPPED",
				runtime.DescribeContext("GROUP_NO_LONGER_EXISTS"));
		}
		if (runtime.IsInfantryFallbackRestorePending())
		{
			bool groupOut = !runtime.GetVehicle() || RecoverProtectedDismountClearance(
				runtime,
				runtime.GetGroup(),
				runtime.GetVehicle(),
				"TERMINAL_PENDING_CLEARANCE",
				true,
				DISMOUNT_CLEARANCE_TERMINAL_MAX_ATTEMPTS);
			if (!groupOut && runtime.GetVehicle())
			{
				ForceAliveGroupMembersOut(runtime.GetGroup(), runtime.GetVehicle());
				groupOut = RecoverProtectedDismountClearance(
					runtime,
					runtime.GetGroup(),
					runtime.GetVehicle(),
					"TERMINAL_FORCE_CLEARANCE",
					true,
					DISMOUNT_CLEARANCE_TERMINAL_MAX_ATTEMPTS);
			}
			if (groupOut && groupCurrent)
			{
				RestoreInfantryOrder(runtime, faction, slot, "FALLBACK_DISEMBARK_FAILED");
				m_CohesionPolicy.Apply(slot.GetGroup());
				runtime.ClearInfantryFallbackRestorePending();
			}
			else if (!groupOut && groupCurrent && runtime.GetVehicle())
			{
				int logicalOccupants;
				int transitions;
				int insideBounds;
				string samples;
				m_Watchdog.InspectProtectedMemberDismountClearance(
					runtime.GetGroup(),
					runtime.GetVehicle(),
					logicalOccupants,
					transitions,
					insideBounds,
					samples);
				// A member with no compartment link or transition is safe to receive
				// an infantry order even while the vehicle entity is retained as a
				// physical-clearance guard. Movement itself can clear the footprint;
				// deletion below remains blocked until a later verified clear poll.
				if (logicalOccupants == 0 && transitions == 0 && insideBounds > 0)
				{
					RestoreInfantryOrder(runtime, faction, slot, "PHYSICAL_CLEARANCE_PENDING");
					m_CohesionPolicy.Apply(slot.GetGroup());
					runtime.ClearInfantryFallbackRestorePending();
					AICF_Stage3Diagnostics.Warning(
						"DISEMBARK_CLEARANCE_PENDING",
						string.Format("%1 inside_bounds=%2 samples=[%3]", runtime.DescribeContext("INFANTRY_ORDER_RESTORED_VEHICLE_RETAINED"), insideBounds, samples));
				}
			}
			if (!groupOut)
				return;
		}

		if (runtime.GetVehicle() && runtime.GetGroup() &&
			!RecoverProtectedDismountClearance(
				runtime,
				runtime.GetGroup(),
				runtime.GetVehicle(),
				"TERMINAL_DELETE_CLEARANCE",
				true,
				DISMOUNT_CLEARANCE_TERMINAL_MAX_ATTEMPTS))
		{
			return;
		}

		AICF_EVehicleKind replacementKind;
		bool expediteForReplacement = !groupCurrent && slot &&
			slot.GetSpawnGeneration() > runtime.GetGroupGeneration() &&
			TryGetDesiredKind(slot, replacementKind) && CanStartVehicleTrip(slot);
		bool cleanupDue = System.GetTickCount() >= runtime.GetCleanupAtMs();
		if (!cleanupDue && !expediteForReplacement)
			return;
		if (runtime.GetVehicle() && m_Watchdog.HasProtectedOccupant(runtime.GetVehicle()))
		{
			ForceAliveGroupMembersOut(runtime.GetGroup(), runtime.GetVehicle());
			if (m_Watchdog.HasProtectedOccupant(runtime.GetVehicle()))
				return;
		}

		DeleteRuntimeWaypoint(runtime);
		DetachVehicleFromGroup(runtime);
		if (runtime.GetVehicle())
		{
			RequestVehicleDelete(runtime, expediteForReplacement && !cleanupDue);
			return;
		}

		FinalizeVehicleCleanup(runtime, slot, runtimes, nextVehicleAtMs, nextVehicleGeneration, slotId, expediteForReplacement && !cleanupDue);
	}

	protected void RequestVehicleDelete(AICF_VehicleRuntime runtime, bool replacementCapacityRequired)
	{
		Vehicle vehicle = runtime.GetVehicle();
		if (!vehicle)
			return;

		EntityID entityId = vehicle.GetID();
		string entityIdString = entityId.ToString();
		string rplIdString = "NONE";
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (rpl)
			rplIdString = rpl.Id().ToString();
		vector origin = vehicle.GetOrigin();
		string reason = "CLEANUP_DUE";
		if (replacementCapacityRequired)
			reason = "REPLACEMENT_CAPACITY_REQUIRED";
		runtime.BeginVehicleDeleteConfirmation(entityId, entityIdString, rplIdString, origin);
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_DELETE_REQUESTED",
			string.Format(
				"%1 entity_id=%2 rpl_id=%3 origin=%4 attempt=1",
				runtime.DescribeContext(reason),
				entityIdString,
				rplIdString,
				origin));
		RplComponent.DeleteRplEntity(vehicle, false);
		runtime.ClearVehicleReferenceAfterDeleteRequest();
	}

	protected void ProcessVehicleDeleteConfirmation(
		AICF_VehicleRuntime runtime,
		AICF_GroupSlot slot,
		array<ref AICF_VehicleRuntime> runtimes,
		array<int> nextVehicleAtMs,
		array<int> nextVehicleGeneration,
		int slotId)
	{
		IEntity remaining = GetGame().GetWorld().FindEntityByID(runtime.GetVehicleDeleteEntityId());
		if (remaining)
		{
			if (runtime.CanRetryVehicleDelete(VEHICLE_DELETE_RETRY_INTERVAL_MS, VEHICLE_DELETE_MAX_ATTEMPTS))
			{
				runtime.RecordVehicleDeleteRetry();
				AICF_Stage3Diagnostics.Warning(
					"VEHICLE_DELETE_RETRIED",
					string.Format(
						"%1 entity_id=%2 rpl_id=%3 attempt=%4",
						runtime.DescribeContext("AUTHORITY_ENTITY_STILL_RESOLVES"),
						runtime.GetVehicleDeleteEntityIdString(),
						runtime.GetVehicleDeleteRplId(),
						runtime.GetVehicleDeleteAttempts()));
				RplComponent.DeleteRplEntity(remaining, false);
			}

			if (runtime.GetVehicleDeleteAgeMs() >= VEHICLE_DELETE_CONFIRM_TIMEOUT_MS &&
				runtime.MarkVehicleDeleteFailureReported())
			{
				LatchAcceptanceFailure(runtime, "VEHICLE_DELETE_NOT_CONFIRMED");
				AICF_Stage3Diagnostics.Error(
					"VEHICLE_DELETE_NOT_CONFIRMED",
					string.Format(
						"%1 entity_id=%2 rpl_id=%3 attempts=%4 age_ms=%5",
						runtime.DescribeContext("AUTHORITY_ENTITY_STILL_RESOLVES"),
						runtime.GetVehicleDeleteEntityIdString(),
						runtime.GetVehicleDeleteRplId(),
						runtime.GetVehicleDeleteAttempts(),
						runtime.GetVehicleDeleteAgeMs()));
			}
			return;
		}

		bool groupCurrent = IsRuntimeCurrent(runtime, slot);
		AICF_EVehicleKind replacementKind;
		bool expediteForReplacement = !groupCurrent && slot &&
			slot.GetSpawnGeneration() > runtime.GetGroupGeneration() &&
			TryGetDesiredKind(slot, replacementKind) && CanStartVehicleTrip(slot);
		bool cleanupDue = System.GetTickCount() >= runtime.GetCleanupAtMs();
		FinalizeVehicleCleanup(
			runtime,
			slot,
			runtimes,
			nextVehicleAtMs,
			nextVehicleGeneration,
			slotId,
			expediteForReplacement && !cleanupDue);
	}

	protected void FinalizeVehicleCleanup(
		AICF_VehicleRuntime runtime,
		AICF_GroupSlot slot,
		array<ref AICF_VehicleRuntime> runtimes,
		array<int> nextVehicleAtMs,
		array<int> nextVehicleGeneration,
		int slotId,
		bool replacementCapacityRequired)
	{
		string cleanupReason = "AUTHORITY_DELETE_CONFIRMED";
		if (replacementCapacityRequired)
			cleanupReason = "REPLACEMENT_CAPACITY_REQUIRED";
		string cleanupDetails = string.Format(
			"%1 entity_id=%2 rpl_id=%3 origin=%4 delete_attempts=%5",
			runtime.DescribeContext(cleanupReason),
			runtime.GetVehicleDeleteEntityIdString(),
			runtime.GetVehicleDeleteRplId(),
			runtime.GetVehicleDeleteOrigin(),
			runtime.GetVehicleDeleteAttempts());
		if (slot)
			slot.ClearVehicleRuntime(runtime);
		if (runtimes.IsIndexValid(slotId) && runtimes[slotId] == runtime)
			runtimes[slotId] = null;
		if (replacementCapacityRequired && nextVehicleAtMs.IsIndexValid(slotId))
		{
			nextVehicleAtMs[slotId] = 0;
			if (nextVehicleGeneration.IsIndexValid(slotId))
				nextVehicleGeneration[slotId] = -1;
		}
		AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP_CONFIRMED", cleanupDetails);
		AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP", cleanupDetails);
		runtime.ClearVehicleDeleteConfirmation();
	}

	protected int CalculatePlannedBoardingPhaseCount(
		AICF_EVehicleKind kind,
		bool approachPlanned,
		bool driverPlanned,
		bool gunnerPlanned)
	{
		// PASSENGERS is always planned, including the two-poll physical
		// confirmation path when every living member is already mounted.
		int plannedPhases = 1;
		if (approachPlanned)
			plannedPhases++;
		if (driverPlanned)
			plannedPhases++;
		if (kind == AICF_EVehicleKind.ARMED_LIGHT && gunnerPlanned)
			plannedPhases++;
		return plannedPhases;
	}

	protected string DescribeBoardingWaypointState(
		SCR_AIGroup group,
		AICF_VehicleRuntime runtime)
	{
		if (!group || !runtime)
			return "INVALID_INPUT";

		AIWaypoint tracked = runtime.GetActiveWaypoint();
		AIWaypoint current = group.GetCurrentWaypoint();
		array<AIWaypoint> waypointQueue = {};
		int queueCount = group.GetWaypoints(waypointQueue);
		string trackedId = "NONE";
		string currentId = "NONE";
		if (tracked)
			trackedId = tracked.GetID().ToString();
		if (current)
			currentId = current.GetID().ToString();
		return string.Format(
			"tracked=%1 current=%2 is_current=%3 in_queue=%4 queue_count=%5",
			trackedId,
			currentId,
			tracked && tracked == current,
			tracked && waypointQueue.Contains(tracked),
			queueCount);
	}

	protected bool CanStartVehicleTrip(AICF_GroupSlot slot)
	{
		if (!slot || slot.HasVehicleTerminalFailure() || slot.IsVehicleTripSuppressedForCurrentAssignment() ||
			!slot.IsCombatReady() || slot.GetRole() != AICF_EGroupRole.ATTACK ||
			!slot.GetGroup() || !slot.GetTargetBase() || !slot.GetTargetBase().GetOwner() || !slot.GetWaypoint())
		{
			return false;
		}

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		vector targetPosition;
		return leader && m_OrderPlanner.TryResolveTargetPosition(slot.GetTargetBase(), slot.GetRole(), targetPosition) &&
			IsRouteLongEnough(leader.GetOrigin(), targetPosition);
	}

	protected bool TryGetDesiredKind(AICF_GroupSlot slot, out AICF_EVehicleKind kind)
	{
		if (!slot || slot.GetRole() != AICF_EGroupRole.ATTACK)
			return false;

		int slotId = slot.GetSlotId();
		if (slotId < m_Config.GetTransportVehiclesPerFaction())
		{
			kind = AICF_EVehicleKind.TRANSPORT;
			return true;
		}

		if (slotId - m_Config.GetTransportVehiclesPerFaction() < m_Config.GetArmedLightVehiclesPerFaction())
		{
			kind = AICF_EVehicleKind.ARMED_LIGHT;
			return true;
		}

		return false;
	}

	protected bool IsRuntimeCurrent(AICF_VehicleRuntime runtime, AICF_GroupSlot slot)
	{
		return runtime && slot && slot.IsCombatReady() && slot.GetGroup() == runtime.GetGroup() &&
			slot.GetSpawnGeneration() == runtime.GetGroupGeneration() && slot.GetVehicleRuntime() == runtime;
	}

	protected string GetDetachReason(AICF_VehicleRuntime runtime, AICF_GroupSlot slot)
	{
		if (!slot)
			return "SLOT_MISSING";
		if (slot.GetSpawnGeneration() != runtime.GetGroupGeneration())
			return "GROUP_GENERATION_CHANGED";
		if (!slot.IsCombatReady())
			return "GROUP_NOT_COMBAT_READY";
		if (slot.GetGroup() != runtime.GetGroup())
			return "GROUP_REFERENCE_CHANGED";
		if (slot.GetVehicleRuntime() != runtime)
			return "VEHICLE_RUNTIME_DETACHED";

		return "RUNTIME_NOT_CURRENT";
	}

	protected bool IsRouteLongEnough(vector from, vector to)
	{
		return vector.DistanceSqXZ(from, to) >= m_Config.GetMinimumRouteMeters() * m_Config.GetMinimumRouteMeters();
	}

	protected void RestoreInfantryOrder(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string reason)
	{
		if (!slot || !slot.IsCombatReady() || slot.GetWaypoint())
			return;

		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		bool restored = m_OrderPlanner.RebuildCurrentOrder(slot, faction, reason);
		if (!restored)
			restored = m_OrderPlanner.AssignOrder(slot, faction, m_ObjectiveGraph, m_TargetSelector, reason, oldTarget);

		if (!restored)
		{
			AICF_Stage3Diagnostics.Warning("INFANTRY_FALLBACK", string.Format("%1 order_restored=0", runtime.DescribeContext("ORDER_RESTORE_DEFERRED")));
			return;
		}

		AICF_Stage3Diagnostics.Info("INFANTRY_FALLBACK", string.Format("%1 order_restored=1 target=%2", runtime.DescribeContext(reason), AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase())));
	}

	protected void DeleteRuntimeWaypoint(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetActiveWaypoint())
			return;

		AIWaypoint waypoint = runtime.GetActiveWaypoint();
		m_WaypointFactory.DeleteOwnedWaypoint(runtime.GetGroup(), waypoint);
		runtime.ClearActiveWaypoint(waypoint);
	}

	protected void DetachVehicleFromGroup(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetGroup() || !runtime.GetVehicleUsage())
			return;

		CancelCrewRecovery(runtime);
		SCR_AIGroupUtilityComponent groupUtility = runtime.GetGroup().GetGroupUtilityComponent();
		if (groupUtility && groupUtility.IsUsableVehicle(runtime.GetVehicleUsage()))
			groupUtility.RemoveUsableVehicle(runtime.GetVehicleUsage());
	}

	protected int CountReserved(array<ref AICF_VehicleRuntime> runtimes)
	{
		int count;
		foreach (AICF_VehicleRuntime runtime : runtimes)
		{
			if (runtime)
				count++;
		}
		return count;
	}

	protected int CountSpawned(array<ref AICF_VehicleRuntime> runtimes)
	{
		int count;
		foreach (AICF_VehicleRuntime runtime : runtimes)
		{
			if (runtime && runtime.GetVehicle())
				count++;
		}
		return count;
	}

	protected int CountState(array<ref AICF_VehicleRuntime> runtimes, AICF_EVehicleState state)
	{
		int count;
		foreach (AICF_VehicleRuntime runtime : runtimes)
		{
			if (runtime && runtime.GetState() == state)
				count++;
		}
		return count;
	}

	protected int CountTerminal(array<ref AICF_VehicleRuntime> runtimes)
	{
		return CountState(runtimes, AICF_EVehicleState.ABANDONED) + CountState(runtimes, AICF_EVehicleState.DESTROYED);
	}

	protected void MarkCompleted(AICF_VehicleRuntime runtime)
	{
		if (!runtime)
			return;

		bool us = runtime.GetFactionKey() == "US";
		int slotId = runtime.GetSlotId();
		if (runtime.GetKind() == AICF_EVehicleKind.TRANSPORT)
		{
			if (us && m_aUSTransportCompletedSlots.IsIndexValid(slotId))
				m_aUSTransportCompletedSlots[slotId] = true;
			else if (!us && m_aUSSRTransportCompletedSlots.IsIndexValid(slotId))
				m_aUSSRTransportCompletedSlots[slotId] = true;
		}
		else
		{
			if (us && m_aUSArmedCompletedSlots.IsIndexValid(slotId))
				m_aUSArmedCompletedSlots[slotId] = true;
			else if (!us && m_aUSSRArmedCompletedSlots.IsIndexValid(slotId))
				m_aUSSRArmedCompletedSlots[slotId] = true;
		}
	}

	protected void LatchAcceptanceFailure(AICF_VehicleRuntime runtime, string reason)
	{
		bool firstFailure = !m_bAcceptanceFailureLatched;
		m_bAcceptanceFailureLatched = true;
		m_iAcceptanceFailureCount++;
		if (m_sFirstAcceptanceFailureReason.IsEmpty())
			m_sFirstAcceptanceFailureReason = reason;

		string context = string.Format("reason=%1", reason);
		if (runtime)
			context = runtime.DescribeContext(reason);
		AICF_Stage3Diagnostics.Warning(
			"ACCEPTANCE_FAILURE_LATCHED",
			string.Format(
				"%1 count=%2 first_reason=%3 first_failure=%4",
				context,
				m_iAcceptanceFailureCount,
				m_sFirstAcceptanceFailureReason,
				firstFailure));

		if (firstFailure && m_bResultCandidateLogged)
		{
			AICF_Stage3Diagnostics.Warning(
				"RESULT_CANDIDATE",
				string.Format(
					"status=INVALIDATED reason=%1 acceptance_failure_count=%2 final=0",
					reason,
					m_iAcceptanceFailureCount));
		}
	}

	protected void TryEmitResultCandidate()
	{
		if (m_bResultCandidateLogged || m_bAcceptanceFailureLatched || AICF_Stage3Diagnostics.HasErrors())
			return;

		int transportCount = m_Config.GetTransportVehiclesPerFaction();
		int armedCount = m_Config.GetArmedLightVehiclesPerFaction();
		bool transportComplete = AreConfiguredTripsComplete(m_aUSTransportCompletedSlots, 0, transportCount) &&
			AreConfiguredTripsComplete(m_aUSSRTransportCompletedSlots, 0, transportCount);
		bool armedComplete = AreConfiguredTripsComplete(m_aUSArmedCompletedSlots, transportCount, armedCount) &&
			AreConfiguredTripsComplete(m_aUSSRArmedCompletedSlots, transportCount, armedCount);
		if (!transportComplete || !armedComplete)
			return;

		m_bResultCandidateLogged = true;
		AICF_Stage3Diagnostics.Info(
			"RESULT_CANDIDATE",
			string.Format(
				"status=READY transport_complete=%1 armed_light_complete=%2 transport_slots_per_faction=%3 armed_slots_per_faction=%4 scope=AUTOMATED_TRIP_INVARIANTS final=0 requires_log_review=1",
				transportComplete,
				armedComplete,
				transportCount,
				armedCount));
	}

	protected bool AreConfiguredTripsComplete(array<bool> completedSlots, int firstSlot, int count)
	{
		for (int slotId = firstSlot; slotId < firstSlot + count; slotId++)
		{
			if (!completedSlots.IsIndexValid(slotId) || !completedSlots[slotId])
				return false;
		}

		return true;
	}

	protected void StopRuntimes(array<ref AICF_VehicleRuntime> runtimes, bool cleanupEntities)
	{
		foreach (AICF_VehicleRuntime runtime : runtimes)
		{
			if (!runtime)
				continue;

			DeleteRuntimeWaypoint(runtime);
			DetachVehicleFromGroup(runtime);
			if (cleanupEntities && runtime.GetVehicle() && !runtime.GetVehicle().IsOccupied())
				RplComponent.DeleteRplEntity(runtime.GetVehicle(), false);
		}
	}
}
