// Authoritative Stage 3 orchestration. A vehicle temporarily owns the group's
// waypoint queue only from boarding through dismount; the stable infantry slot,
// strategic target, reinforcement ledger and victory rules remain unchanged.
class AICF_VehicleCoordinator
{
	protected static const float DISEMBARK_TRIGGER_RADIUS_METERS = 40.0;

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

	protected bool m_bUSTransportCompleted;
	protected bool m_bUSSRTransportCompleted;
	protected bool m_bUSArmedCompleted;
	protected bool m_bUSSRArmedCompleted;
	protected bool m_bResultLogged;

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

		ProcessFaction(usState, usFaction, m_aUSRuntime, m_aUSCapReported, m_aUSNextVehicleAtMs);
		ProcessFaction(ussrState, ussrFaction, m_aUSSRRuntime, m_aUSSRCapReported, m_aUSSRNextVehicleAtMs);
		TryEmitResult();
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
		}

		return false;
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
			if (AICF_Stage3Diagnostics.HasErrors())
				reason = "STAGE3_ERRORS";
			AICF_Stage3Diagnostics.Info("RESULT", string.Format("status=FAIL reason=%1", reason));
		}
	}

	protected void ProcessFaction(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		array<ref AICF_VehicleRuntime> runtimes,
		array<bool> capReported,
		array<int> nextVehicleAtMs)
	{
		if (!factionState || !faction)
			return;

		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			AICF_VehicleRuntime runtime = runtimes[slotId];
			if (runtime && !IsRuntimeCurrent(runtime, slot))
			{
				BeginDetachedCleanup(runtime, slot, "GROUP_GENERATION_CHANGED");
				ProcessTerminal(runtime, slot, runtimes, slotId);
				continue;
			}

			if (!runtime)
			{
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

			ProcessRuntime(runtime, factionState, faction, slot, runtimes, nextVehicleAtMs, slotId);
		}
	}

	protected void ProcessRuntime(
		AICF_VehicleRuntime runtime,
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		array<ref AICF_VehicleRuntime> runtimes,
		array<int> nextVehicleAtMs,
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
				ProcessFallback(runtime, faction, slot, nextVehicleAtMs);
				break;
			case AICF_EVehicleState.ABANDONED:
			case AICF_EVehicleState.DESTROYED:
				ProcessTerminal(runtime, slot, runtimes, slotId);
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
			BeginFallback(runtime, faction, slot, "PREFAB_UNAVAILABLE");
			return;
		}

		runtime.SetState(AICF_EVehicleState.SPAWNING);
		Vehicle vehicle;
		SCR_AIVehicleUsageComponent usage;
		SCR_CampaignMilitaryBaseComponent spawnBase;
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
			runtime,
			vehicle,
			usage,
			spawnBase))
		{
			runtime.SetState(AICF_EVehicleState.REQUESTED);
			runtime.SetNextAttemptAtMs(System.GetTickCount() + m_Config.GetRetryIntervalMs());
			if (runtime.MarkSpawnBlockedReported())
				AICF_Stage3Diagnostics.Warning("VEHICLE_SPAWN_SITE_REJECTED", runtime.DescribeContext("NO_SAFE_SPAWN_AVAILABLE"));
			return;
		}

		if (!runtime.BindVehicle(vehicle, usage, prefab, spawnBase))
		{
			RplComponent.DeleteRplEntity(vehicle, false);
			AICF_Stage3Diagnostics.Error("VEHICLE_BIND_FAILED", runtime.DescribeContext("RUNTIME_BIND_REJECTED"));
			BeginFallback(runtime, faction, slot, "RUNTIME_BIND_REJECTED");
			return;
		}
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_SPAWNED",
			string.Format("%1 prefab=%2 base=%3", runtime.DescribeContext("SPAWN_SUCCESS"), prefab, AICF_Stage1Diagnostics.BaseKey(spawnBase)));

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

		bool hasPilot;
		bool hasTurret;
		int accessibleSeats = m_Watchdog.CountAccessibleSeats(runtime, hasPilot, hasTurret);
		int aliveAgents = AICF_GroupRuntime.CountAliveAgents(group);
		if (!hasPilot || accessibleSeats < aliveAgents || (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT && !hasTurret))
		{
			AICF_Stage3Diagnostics.Warning(
				"BOARDING_TIMEOUT",
				string.Format("%1 alive=%2 accessible_seats=%3 has_pilot=%4 has_turret=%5", runtime.DescribeContext("INSUFFICIENT_COMPARTMENTS"), aliveAgents, accessibleSeats, hasPilot, hasTurret));
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

		groupUtility.AddUsableVehicle(runtime.GetVehicleUsage());
		m_OrderPlanner.SuspendOrderForVehicle(slot);
		SCR_BoardingEntityWaypoint waypoint = m_WaypointFactory.CreateBoardingWaypoint(
			vehicle,
			runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT);
		if (!waypoint)
		{
			BeginFallback(runtime, faction, slot, "BOARDING_WAYPOINT_FAILED");
			return;
		}

		group.AddWaypointAt(waypoint, 0);
		runtime.SetActiveWaypoint(waypoint);
		runtime.SetState(AICF_EVehicleState.BOARDING);
		AICF_Stage3Diagnostics.Info("VEHICLE_ASSIGNED", string.Format("%1 prefab=%2", runtime.DescribeContext(reason), runtime.GetVehiclePrefab()));
		AICF_Stage3Diagnostics.Info("BOARDING_STARTED", string.Format("%1 alive=%2 seats=%3", runtime.DescribeContext(reason), aliveAgents, accessibleSeats));
		int passengerCount = Math.Max(0, aliveAgents - 1);
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
			passengerCount = Math.Max(0, passengerCount - 1);
		AICF_Stage3Diagnostics.Info(
			"PASSENGERS_ASSIGNED",
			string.Format(
				"%1 requested=%2 policy=ALL_OR_FALLBACK",
				runtime.DescribeContext("STOCK_GET_IN_WAYPOINT"),
				passengerCount));
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

		IEntity driver = m_Watchdog.ResolveAliveDriver(runtime);
		if (driver && driver != runtime.GetLastDriver())
		{
			runtime.SetLastDriver(driver);
			AICF_Stage3Diagnostics.Info("DRIVER_ASSIGNED", string.Format("%1 driver=%2", runtime.DescribeContext("PILOT_COMPARTMENT_OCCUPIED"), driver.GetID()));
		}
		IEntity gunner;
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
		{
			gunner = m_Watchdog.ResolveAliveGunner(runtime);
			if (gunner && gunner != runtime.GetLastGunner())
			{
				runtime.SetLastGunner(gunner);
				AICF_Stage3Diagnostics.Info("GUNNER_ASSIGNED", string.Format("%1 gunner=%2", runtime.DescribeContext("TURRET_COMPARTMENT_OCCUPIED"), gunner.GetID()));
			}
		}

		if (driver && (runtime.GetKind() != AICF_EVehicleKind.ARMED_LIGHT || gunner) &&
			m_Watchdog.AreAllAliveMembersInVehicle(slot.GetGroup(), runtime.GetVehicle()))
		{
			int mounted = m_Watchdog.CountAliveGroupMembersInVehicle(slot.GetGroup(), runtime.GetVehicle());
			DeleteRuntimeWaypoint(runtime);
			AICF_Stage3Diagnostics.Info(
				"BOARDING_COMPLETE",
				string.Format("%1 mounted=%2 driver=1 gunner=%3", runtime.DescribeContext("ALL_ALIVE_MEMBERS_MOUNTED"), mounted, gunner != null));
			StartMovement(runtime, faction, slot, "BOARDING_COMPLETE");
			return;
		}

		if (System.GetTickCount(runtime.GetStateStartedAtMs()) >= m_Config.GetBoardingTimeoutMs())
		{
			AICF_Stage3Diagnostics.Warning("BOARDING_TIMEOUT", string.Format("%1 timeout_ms=%2", runtime.DescribeContext("BOARDING_DEADLINE_EXCEEDED"), m_Config.GetBoardingTimeoutMs()));
			BeginFallback(runtime, faction, slot, "BOARDING_TIMEOUT");
		}
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
		vector dismountPosition = CalculateDismountPosition(runtime.GetVehicle().GetOrigin(), target.GetOwner().GetOrigin());
		AIWaypoint moveWaypoint = m_WaypointFactory.CreateMoveWaypoint(dismountPosition);
		if (!moveWaypoint)
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_ROUTE_WAYPOINT_FAILED");
			return;
		}

		slot.GetGroup().AddWaypointAt(moveWaypoint, 0);
		runtime.SetActiveWaypoint(moveWaypoint);
		runtime.SetState(AICF_EVehicleState.MOVING);
		runtime.ObserveProgress(
			Math.Sqrt(vector.DistanceSqXZ(runtime.GetVehicle().GetOrigin(), dismountPosition)),
			m_Config.GetProgressMeters());
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_ROUTE_ASSIGNED",
			string.Format("%1 target=%2 dismount_distance_m=%3", runtime.DescribeContext(reason), AICF_Stage1Diagnostics.BaseKey(target), m_Config.GetDismountDistanceMeters()));
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
		if (!m_Watchdog.CanMove(runtime))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_IMMOBILIZED");
			return;
		}
		if (m_Watchdog.IsOverturned(runtime))
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_OVERTURNED");
			return;
		}
		if (!runtime.GetTargetBase() || runtime.GetTargetBase().GetFaction() == faction)
		{
			BeginFallback(runtime, faction, slot, "STRATEGIC_TARGET_CHANGED");
			return;
		}

		IEntity driver = m_Watchdog.ResolveAliveDriver(runtime);
		if (!driver)
		{
			BeginDriverRecovery(runtime, faction, slot);
			return;
		}
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT && !m_Watchdog.ResolveAliveGunner(runtime))
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

		float distanceMeters = Math.Sqrt(vector.DistanceSqXZ(runtime.GetVehicle().GetOrigin(), moveWaypoint.GetOrigin()));
		if (distanceMeters <= DISEMBARK_TRIGGER_RADIUS_METERS)
		{
			BeginDismount(runtime, faction, slot, "DISEMBARK_POSITION_REACHED");
			return;
		}

		if (runtime.ObserveProgress(distanceMeters, m_Config.GetProgressMeters()))
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_PROGRESS",
				string.Format("%1 distance_m=%2", runtime.DescribeContext("DISTANCE_REDUCED"), distanceMeters));
			if (runtime.HasPendingRouteRecovery())
			{
				runtime.ConfirmRouteRecovery();
				AICF_Stage3Diagnostics.Info("VEHICLE_RECOVERY_SUCCEEDED", runtime.DescribeContext("ROUTE_PROGRESS_RESTORED"));
			}
		}
		if (!runtime.IsStuck(m_Config.GetStuckTimeoutMs()))
			return;

		AICF_Stage3Diagnostics.Warning(
			"VEHICLE_STUCK_DETECTED",
			string.Format("%1 distance_m=%2 timeout_ms=%3 attempt=%4", runtime.DescribeContext("NO_ROUTE_PROGRESS"), distanceMeters, m_Config.GetStuckTimeoutMs(), runtime.GetRecoveryCount() + 1));
		AICF_Stage3Diagnostics.Info("VEHICLE_RECOVERY_STARTED", runtime.DescribeContext("REBUILD_ROUTE"));
		if (runtime.GetRecoveryCount() >= m_Config.GetMaxRecoveries())
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_STUCK_PERSISTENT");
			return;
		}

		vector destination = moveWaypoint.GetOrigin();
		DeleteRuntimeWaypoint(runtime);
		AIWaypoint rebuilt = m_WaypointFactory.CreateMoveWaypoint(destination);
		if (!rebuilt)
		{
			BeginFallback(runtime, faction, slot, "VEHICLE_ROUTE_RECOVERY_FAILED");
			return;
		}

		slot.GetGroup().AddWaypointAt(rebuilt, 0);
		runtime.SetActiveWaypoint(rebuilt);
		runtime.RecordRecovery(distanceMeters);
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_STUCK_RECOVERY",
			string.Format("%1 action=REBUILD_ROUTE attempt=%2", runtime.DescribeContext("REBUILD_ROUTE"), runtime.GetRecoveryCount()));
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
		SCR_BoardingEntityWaypoint waypoint = m_WaypointFactory.CreateDriverRecoveryWaypoint(runtime.GetVehicle());
		if (!waypoint)
		{
			BeginFallback(runtime, faction, slot, "DRIVER_RECOVERY_WAYPOINT_FAILED");
			return;
		}

		slot.GetGroup().AddWaypointAt(waypoint, 0);
		runtime.SetActiveWaypoint(waypoint);
		runtime.SetRecoveringDriver(true);
		runtime.RecordCrewRecovery();
		runtime.SetState(AICF_EVehicleState.RECOVERING);
		AICF_Stage3Diagnostics.Warning("DRIVER_LOST", runtime.DescribeContext("PILOT_COMPARTMENT_EMPTY_OR_DRIVER_DEAD"));
		AICF_Stage3Diagnostics.Info("VEHICLE_RECOVERY_STARTED", runtime.DescribeContext("REASSIGN_DRIVER"));
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
		SCR_BoardingEntityWaypoint waypoint = m_WaypointFactory.CreateGunnerRecoveryWaypoint(runtime.GetVehicle());
		if (!waypoint)
		{
			BeginFallback(runtime, faction, slot, "GUNNER_RECOVERY_WAYPOINT_FAILED");
			return;
		}

		slot.GetGroup().AddWaypointAt(waypoint, 0);
		runtime.SetActiveWaypoint(waypoint);
		runtime.SetRecoveringDriver(false);
		runtime.RecordCrewRecovery();
		runtime.SetState(AICF_EVehicleState.RECOVERING);
		AICF_Stage3Diagnostics.Warning("GUNNER_LOST", runtime.DescribeContext("TURRET_COMPARTMENT_EMPTY_OR_GUNNER_DEAD"));
		AICF_Stage3Diagnostics.Info("VEHICLE_RECOVERY_STARTED", runtime.DescribeContext("REASSIGN_GUNNER"));
	}

	protected void ProcessDriverRecovery(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (m_Watchdog.IsDestroyed(runtime) || !m_Watchdog.CanMove(runtime))
		{
			BeginFallback(runtime, faction, slot, "DRIVER_RECOVERY_VEHICLE_UNUSABLE");
			return;
		}

		IEntity driver = m_Watchdog.ResolveAliveDriver(runtime);
		if (!driver && !runtime.IsRecoveringDriver())
		{
			BeginDriverRecovery(runtime, faction, slot);
			return;
		}

		IEntity gunner;
		if (runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT)
			gunner = m_Watchdog.ResolveAliveGunner(runtime);
		if (driver && runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT && !gunner && runtime.IsRecoveringDriver())
		{
			BeginGunnerRecovery(runtime, faction, slot);
			return;
		}
		if (driver && (runtime.GetKind() != AICF_EVehicleKind.ARMED_LIGHT || gunner))
		{
			bool recoveredDriver = runtime.IsRecoveringDriver();
			string recoveryReason = "GUNNER_REASSIGNED";
			if (recoveredDriver)
				recoveryReason = "DRIVER_REASSIGNED";
			runtime.SetLastDriver(driver);
			if (gunner && !recoveredDriver)
			{
				runtime.SetLastGunner(gunner);
				AICF_Stage3Diagnostics.Info("GUNNER_REASSIGNED", string.Format("%1 gunner=%2", runtime.DescribeContext("RECOVERY_SUCCESS"), gunner.GetID()));
			}
			DeleteRuntimeWaypoint(runtime);
			if (recoveredDriver)
				AICF_Stage3Diagnostics.Info("DRIVER_REASSIGNED", string.Format("%1 driver=%2", runtime.DescribeContext("RECOVERY_SUCCESS"), driver.GetID()));
			AICF_Stage3Diagnostics.Info("VEHICLE_RECOVERY_SUCCEEDED", runtime.DescribeContext(recoveryReason));
			StartMovement(runtime, faction, slot, recoveryReason);
			return;
		}

		if (System.GetTickCount(runtime.GetStateStartedAtMs()) >= m_Config.GetBoardingTimeoutMs())
			BeginFallback(runtime, faction, slot, "DRIVER_RECOVERY_TIMEOUT");
	}

	protected void BeginDismount(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		string reason)
	{
		DeleteRuntimeWaypoint(runtime);
		SCR_BoardingWaypoint waypoint = m_WaypointFactory.CreateDismountWaypoint(runtime.GetVehicle());
		if (!waypoint)
		{
			BeginFallback(runtime, faction, slot, "DISEMBARK_WAYPOINT_FAILED");
			return;
		}

		slot.GetGroup().AddWaypointAt(waypoint, 0);
		runtime.SetActiveWaypoint(waypoint);
		runtime.SetState(AICF_EVehicleState.DISEMBARKING);
		AICF_Stage3Diagnostics.Info("DISEMBARK_STARTED", runtime.DescribeContext(reason));
	}

	protected void ProcessDismount(
		AICF_VehicleRuntime runtime,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (m_Watchdog.AreAllAliveMembersOutOfVehicle(slot.GetGroup(), runtime.GetVehicle()))
		{
			CompleteDismount(runtime, faction, slot);
			return;
		}

		if (System.GetTickCount(runtime.GetStateStartedAtMs()) >= m_Config.GetBoardingTimeoutMs())
		{
			AICF_Stage3Diagnostics.Warning("BOARDING_TIMEOUT", string.Format("%1 phase=DISEMBARK timeout_ms=%2", runtime.DescribeContext("DISEMBARK_TIMEOUT"), m_Config.GetBoardingTimeoutMs()));
			BeginFallback(runtime, faction, slot, "DISEMBARK_TIMEOUT");
		}
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
		AICF_Stage3Diagnostics.Info("DISEMBARK_COMPLETE", runtime.DescribeContext("ALL_ALIVE_MEMBERS_ON_FOOT"));
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

		if (!IsRouteLongEnough(leader.GetOrigin(), newTarget.GetOwner().GetOrigin()))
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
		array<int> nextVehicleAtMs)
	{
		bool groupCurrent = IsRuntimeCurrent(runtime, slot);
		bool allOut = !runtime.GetVehicle() || !runtime.GetVehicle().IsOccupied();
		if (groupCurrent && runtime.GetVehicle())
			allOut = m_Watchdog.AreAllAliveMembersOutOfVehicle(slot.GetGroup(), runtime.GetVehicle());

		if (!allOut && System.GetTickCount(runtime.GetStateStartedAtMs()) < m_Config.GetBoardingTimeoutMs())
			return;

		DeleteRuntimeWaypoint(runtime);
		DetachVehicleFromGroup(runtime);
		if (groupCurrent)
		{
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
			nextVehicleAtMs[slot.GetSlotId()] = System.GetTickCount() + m_Config.GetCleanupDelayMs() + m_Config.GetRetryIntervalMs();
	}

	protected void BeginDetachedCleanup(
		AICF_VehicleRuntime runtime,
		AICF_GroupSlot slot,
		string reason)
	{
		if (!runtime)
			return;

		DeleteRuntimeWaypoint(runtime);
		DetachVehicleFromGroup(runtime);
		runtime.SetTerminalReason(reason);
		runtime.SetState(AICF_EVehicleState.ABANDONED);
		runtime.ScheduleCleanup(System.GetTickCount() + m_Config.GetCleanupDelayMs());
		if (slot)
			slot.ClearVehicleRuntime(runtime);
		AICF_Stage3Diagnostics.Warning("VEHICLE_ABANDONED", runtime.DescribeContext(reason));
	}

	protected void ProcessTerminal(
		AICF_VehicleRuntime runtime,
		AICF_GroupSlot slot,
		array<ref AICF_VehicleRuntime> runtimes,
		int slotId)
	{
		if (!runtime || System.GetTickCount() < runtime.GetCleanupAtMs())
			return;
		if (runtime.GetVehicle() && m_Watchdog.HasAliveOccupant(runtime.GetVehicle()))
			return;

		DeleteRuntimeWaypoint(runtime);
		DetachVehicleFromGroup(runtime);
		if (runtime.GetVehicle())
			RplComponent.DeleteRplEntity(runtime.GetVehicle(), false);
		if (slot)
			slot.ClearVehicleRuntime(runtime);
		runtimes[slotId] = null;
		AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP", runtime.DescribeContext("ENTITY_DELETED_OR_ALREADY_GONE"));
	}

	protected bool CanStartVehicleTrip(AICF_GroupSlot slot)
	{
		if (!slot || !slot.IsCombatReady() || slot.GetRole() != AICF_EGroupRole.ATTACK ||
			!slot.GetGroup() || !slot.GetTargetBase() || !slot.GetTargetBase().GetOwner() || !slot.GetWaypoint())
		{
			return false;
		}

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		return leader && IsRouteLongEnough(leader.GetOrigin(), slot.GetTargetBase().GetOwner().GetOrigin());
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

	protected bool IsRouteLongEnough(vector from, vector to)
	{
		return vector.DistanceSqXZ(from, to) >= m_Config.GetMinimumRouteMeters() * m_Config.GetMinimumRouteMeters();
	}

	protected vector CalculateDismountPosition(vector vehiclePosition, vector targetPosition)
	{
		vector awayFromTarget = vehiclePosition - targetPosition;
		awayFromTarget[1] = 0;
		float distance = Math.Sqrt(vector.DistanceSqXZ(vehiclePosition, targetPosition));
		if (distance < 1.0)
			awayFromTarget = "1 0 0";
		else
			awayFromTarget = awayFromTarget / distance;

		vector position = targetPosition + awayFromTarget * m_Config.GetDismountDistanceMeters();
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		return position;
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
		bool us = runtime.GetFactionKey() == "US";
		if (runtime.GetKind() == AICF_EVehicleKind.TRANSPORT)
		{
			if (us)
				m_bUSTransportCompleted = true;
			else
				m_bUSSRTransportCompleted = true;
		}
		else
		{
			if (us)
				m_bUSArmedCompleted = true;
			else
				m_bUSSRArmedCompleted = true;
		}
	}

	protected void TryEmitResult()
	{
		if (m_bResultLogged || AICF_Stage3Diagnostics.HasErrors())
			return;

		bool transportComplete = m_Config.GetTransportVehiclesPerFaction() == 0 || (m_bUSTransportCompleted && m_bUSSRTransportCompleted);
		bool armedComplete = m_Config.GetArmedLightVehiclesPerFaction() == 0 || (m_bUSArmedCompleted && m_bUSSRArmedCompleted);
		if (!transportComplete || !armedComplete)
			return;

		m_bResultLogged = true;
		AICF_Stage3Diagnostics.Info(
			"RESULT",
			string.Format("status=PASS transport_complete=%1 armed_light_complete=%2 scope=AUTOMATED_TRIP_INVARIANTS", transportComplete, armedComplete));
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
