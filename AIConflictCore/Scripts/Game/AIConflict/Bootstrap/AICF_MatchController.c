// Long-lived authoritative infantry match loop for the Stage 1 vertical slice.
class AICF_MatchController
{
	protected static const int UPDATE_INTERVAL_MS = 1000;
	protected static const int REINFORCEMENT_RETRY_MS = 5000;
	protected static const int HEARTBEAT_INTERVAL_MS = 60000;
	protected static const int PLAYER_FACTION_RETRY_MS = 1000;
	protected static const int MAX_PLAYER_FACTION_ATTEMPTS = 120;
	protected static const int BASE_REPLAN_DELAY_MS = 1000;
	protected static const int GROUP_SPAWN_TIMEOUT_MS = 30000;
	protected static const int PENDING_GROUP_AGENT_BUDGET = 8;
	protected static const float RELAY_CAPTURE_RADIUS_METERS = 30.0;
	protected static const float STUCK_WATCHDOG_IGNORE_RADIUS_METERS = 100.0;
	// The first valid observation only starts the durability window. Two further
	// polls (three observations total) and two full reliability intervals prevent
	// a waypoint that survives one five-second boundary from being confirmed.
	protected static const int ORDER_RECOVERY_STABLE_POLLS = 2;
	protected static const int ORDER_RECOVERY_INITIAL_STABLE_OBSERVATION = 1;
	protected static const int ORDER_RECOVERY_STABLE_INTERVALS = 2;
	protected static const int ORDER_RECOVERY_MIN_STABLE_MS = 10000;

	protected bool m_bStarted;
	protected bool m_bStopped;
	protected bool m_bSubscribed;
	protected bool m_bRosterReady;
	protected bool m_bReplanScheduled;
	protected bool m_bGraphRebuildNeeded;
	protected bool m_bResultLogged;
	protected bool m_bCampaignWasRunning;
	protected bool m_bTestOrderDropInjected;

	protected bool m_bObservedCapture;
	protected bool m_bObservedRetarget;
	protected bool m_bObservedRetargetWithinDeadline;
	protected bool m_bObservedReinforcement;
	protected bool m_bObservedTicketDebit;
	protected bool m_bObservedPlayerJoin;
	protected int m_iLastCaptureAtMs;
	protected int m_iOrderRecoveryAttempts;
	protected int m_iOrderRecoveries;
	protected int m_iStuckDetections;
	protected int m_iStuckRecoveries;
	protected int m_iStuckRecycles;
	protected int m_iDuplicateSpawnsPrevented;
	protected int m_iLifecycleAudits;
	protected FactionKey m_sObservedPlayerFaction;

	protected ref AICF_Stage1Config m_Config;
	protected ref AICF_Stage2Config m_Stage2Config;
	protected ref AICF_Stage3Config m_Stage3Config;
	protected ref AICF_ConflictAdapter m_ConflictAdapter;
	protected ref AICF_ObjectiveGraph m_ObjectiveGraph;
	protected ref AICF_TargetSelector m_TargetSelector;
	protected ref AICF_GroupSpawner m_GroupSpawner;
	protected ref AICF_GroupCohesionPolicy m_GroupCohesionPolicy;
	protected ref AICF_ManagedAILODPolicy m_ManagedAILODPolicy;
	protected ref AICF_ReinforcementSystem m_ReinforcementSystem;
	protected ref AICF_OrderPlanner m_OrderPlanner;
	protected ref AICF_VictorySystem m_VictorySystem;
	protected ref AICF_GroupMapMarkerSystem m_GroupMapMarkers;
	protected ref AICF_VehicleCoordinator m_VehicleCoordinator;
	protected ref AICF_FactionState m_USState;
	protected ref AICF_FactionState m_USSRState;

	protected SCR_GameModeCampaign m_Campaign;
	protected SCR_CampaignFaction m_USFaction;
	protected SCR_CampaignFaction m_USSRFaction;
	protected SCR_MilitaryBaseSystem m_BaseSystem;
	protected SCR_CampaignMilitaryBaseComponent m_LastChangedBase;

	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aTrackedBases = {};
	protected ref array<FactionKey> m_aTrackedBaseOwners = {};

	void Start(SCR_GameModeCampaign campaign)
	{
		if (m_bStarted)
		{
			AICF_Stage1Diagnostics.Warning("CONTROLLER_DUPLICATE_SKIPPED", "Stage 1 controller is already running");
			return;
		}

		m_bStarted = true;
		if (!AICF_Stage1Diagnostics.IsConfigured())
			AICF_Stage1Diagnostics.Configure(string.Format("stage1-%1", System.GetTickCount()));

		if (!GetGame().InPlayMode() || !Replication.IsServer() || !campaign || !campaign.IsMaster())
		{
			Fail("SERVER_AUTHORITY_REQUIRED", "Stage 1 requires a running authoritative Conflict server/master");
			return;
		}

		m_Campaign = campaign;
		m_Config = new AICF_Stage1Config();
		m_Stage2Config = new AICF_Stage2Config();
		m_Stage3Config = new AICF_Stage3Config();
		AICF_Stage2Diagnostics.Configure();
		AICF_Stage3Diagnostics.Configure();
		m_ConflictAdapter = new AICF_ConflictAdapter();
		m_ObjectiveGraph = new AICF_ObjectiveGraph();
		m_TargetSelector = new AICF_TargetSelector();
		m_GroupSpawner = new AICF_GroupSpawner();
		m_GroupCohesionPolicy = new AICF_GroupCohesionPolicy();
		m_ManagedAILODPolicy = new AICF_ManagedAILODPolicy();
		m_ReinforcementSystem = new AICF_ReinforcementSystem();
		m_OrderPlanner = new AICF_OrderPlanner();
		m_VictorySystem = new AICF_VictorySystem();
		if (m_Stage3Config.GetVehiclesEnabled())
		{
			m_VehicleCoordinator = new AICF_VehicleCoordinator(
				m_Stage3Config,
				m_Campaign,
				m_ConflictAdapter,
				m_OrderPlanner,
				m_GroupCohesionPolicy,
				m_ObjectiveGraph,
				m_TargetSelector);
		}
		m_GroupMapMarkers = new AICF_GroupMapMarkerSystem();

		array<SCR_CampaignMilitaryBaseComponent> objectiveBases = {};
		array<SCR_CampaignMilitaryBaseComponent> graphBases = {};
		if (!m_ConflictAdapter.CollectBases(m_Campaign, objectiveBases, graphBases) ||
			!m_ObjectiveGraph.Build(graphBases, objectiveBases))
		{
			Fail("GRAPH_BUILD_FAILED", "Could not build the initial live Conflict graph");
			return;
		}

		if (!m_ConflictAdapter.ResolveFaction(m_Campaign, SCR_ECampaignFaction.BLUFOR, "US", m_USFaction) ||
			!m_ConflictAdapter.ResolveFaction(m_Campaign, SCR_ECampaignFaction.OPFOR, "USSR", m_USSRFaction))
		{
			Fail("FACTION_RESOLUTION_FAILED", "Stock US and USSR factions are required");
			return;
		}

		m_USState = new AICF_FactionState(m_USFaction.GetFactionKey(), m_Config);
		m_USSRState = new AICF_FactionState(m_USSRFaction.GetFactionKey(), m_Config);
		CacheBaseOwners(graphBases);
		Subscribe();

		string expectedPlayerFaction = m_Config.GetExpectedPlayerFaction();
		if (expectedPlayerFaction.IsEmpty())
			expectedPlayerFaction = "ANY";

		AICF_Stage1Diagnostics.Info(
			"CONFIG",
			string.Format(
				"commander_interval_ms=%1 replacement_delay_ms=%2 initial_tickets=%3 groups_per_faction=%4 replacement_ticket_cost=%5 max_managed_agents=%6 expected_player_faction=%7 map_markers=ALWAYS_GLOBAL war_tempo_percent=%8",
				m_Config.GetCommanderIntervalMs(),
				m_Config.GetReinforcementDelayMs(),
				m_Config.GetInitialTickets(),
				AICF_Stage1Config.GROUP_SLOTS_PER_FACTION,
				m_Config.GetReplacementTicketCost(),
				m_Config.GetMaxManagedAgents(),
				expectedPlayerFaction,
				m_Config.GetWarTempoPercent()));
		AICF_Stage2Diagnostics.Info(
			"RELIABILITY_CONFIG",
			string.Format(
				"interval_ms=%1 order_retry_ms=%2 stuck_watchdog=%3 stuck_timeout_ms=%4 stuck_progress_m=%5 max_stuck_recoveries=%6 objective_hold_timeout_ms=%7 max_concurrent_spawns=%8 require_player=%9",
				m_Stage2Config.GetReliabilityIntervalMs(),
				m_Stage2Config.GetOrderRecoveryRetryMs(),
				m_Stage2Config.GetStuckWatchdogEnabled(),
				m_Stage2Config.GetStuckTimeoutMs(),
				m_Stage2Config.GetStuckProgressMeters(),
				m_Stage2Config.GetMaxStuckRecoveries(),
				m_Stage2Config.GetObjectiveHoldTimeoutMs(),
				m_Stage2Config.GetMaxConcurrentReplacementSpawns(),
				m_Config.GetRequirePlayerForResult()));
		string stage3ConfigLine = string.Format(
			"enabled=%1 transports_per_faction=%2 armed_light_per_faction=%3 max_vehicles_per_faction=%4 boarding_timeout_ms=%5 stuck_timeout_ms=%6 progress_m=%7 motion_m=%8 objective_progress_timeout_ms=%9",
			m_Stage3Config.GetVehiclesEnabled(),
			m_Stage3Config.GetTransportVehiclesPerFaction(),
			m_Stage3Config.GetArmedLightVehiclesPerFaction(),
			m_Stage3Config.GetMaxVehiclesPerFaction(),
			m_Stage3Config.GetBoardingTimeoutMs(),
			m_Stage3Config.GetStuckTimeoutMs(),
			m_Stage3Config.GetProgressMeters(),
			m_Stage3Config.GetMotionMeters(),
			m_Stage3Config.GetObjectiveProgressTimeoutMs());
		stage3ConfigLine += string.Format(
			" max_recoveries=%1 dismount_distance_m=%2 retry_ms=%3 cleanup_delay_ms=%4 minimum_route_m=%5 maximum_reuse_distance_m=%6 maximum_spawn_distance_m=%7 cohesion_distance_m=%8",
			m_Stage3Config.GetMaxRecoveries(),
			m_Stage3Config.GetDismountDistanceMeters(),
			m_Stage3Config.GetRetryIntervalMs(),
			m_Stage3Config.GetCleanupDelayMs(),
			m_Stage3Config.GetMinimumRouteMeters(),
			m_Stage3Config.GetMaximumReuseDistanceMeters(),
			m_Stage3Config.GetMaximumSpawnDistanceMeters(),
			m_Stage3Config.GetCohesionDistanceMeters());
		AICF_Stage3Diagnostics.Info("CONFIG", stage3ConfigLine);
		if (m_Stage2Config.HasTestDropOrder())
		{
			AICF_Stage2Diagnostics.Warning(
				"TEST_HOOK_CONFIGURED",
				string.Format(
					"action=DROP_ORDER faction=%1 slot=%2 at_ms=%3",
					m_Stage2Config.GetTestDropOrderFaction(),
					m_Stage2Config.GetTestDropOrderSlot(),
					m_Stage2Config.GetTestDropOrderAtMs()));
		}
		AICF_Stage1Diagnostics.Info("MATCH_START", "map=Arland factions=US,USSR");
		SyncTickets();

		if (!SpawnInitialRoster(m_USState, m_USFaction) || !SpawnInitialRoster(m_USSRState, m_USSRFaction))
		{
			Fail("INITIAL_ROSTER_FAILED", "All eight managed groups must be created");
			return;
		}

		GetGame().GetCallqueue().CallLater(Update, UPDATE_INTERVAL_MS, true);
		GetGame().GetCallqueue().CallLater(CommanderTick, m_Config.GetCommanderIntervalMs(), true);
		GetGame().GetCallqueue().CallLater(ReliabilityTick, m_Stage2Config.GetReliabilityIntervalMs(), true);
		GetGame().GetCallqueue().CallLater(Heartbeat, HEARTBEAT_INTERVAL_MS, true);
	}

	protected bool SpawnInitialRoster(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		if (!factionState || !faction)
			return false;

		// During the first frames Conflict has initialized HQs but still reports all spawn points
		// inactive. Initial deployment is free and may use its owned, uncontested HQ directly;
		// replacements later use the stricter live spawn-point validation.
		SCR_CampaignMilitaryBaseComponent spawnBase = faction.GetMainBase();
		if (!spawnBase || !spawnBase.GetOwner() || !spawnBase.IsInitialized() ||
			spawnBase.GetFaction() != faction ||
			spawnBase.GetCaptureState() != SCR_EBaseCaptureState.NONE ||
			spawnBase.IsBeingCaptured())
		{
			AICF_Stage1Diagnostics.Error(
				"INITIAL_SPAWN_SITE_MISSING",
				string.Format("faction=%1", faction.GetFactionKey()));
			return false;
		}

		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot || !slot.BeginInitialSpawn())
				return false;

			AICF_Stage2Diagnostics.Info(
				"SPAWN_ATTEMPT_STARTED",
				string.Format(
					"faction=%1 slot=%2 generation=%3 kind=INITIAL",
					faction.GetFactionKey(),
					slotId,
					slot.GetSpawnGeneration()));

			AICF_Stage1Diagnostics.Info(
				"ROLE_ASSIGNED",
				string.Format(
					"faction=%1 slot=%2 role=%3",
					faction.GetFactionKey(),
					slotId,
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole())));

			SCR_AIGroup group = m_GroupSpawner.SpawnGroup(faction, spawnBase, slotId);
			if (!group || !BindManagedGroup(factionState, faction, slot, group, "INITIAL"))
			{
				if (group)
					RplComponent.DeleteRplEntity(group, false);
				slot.ResetInitialSpawn();
				return false;
			}

			group.GetOnEmpty().Insert(OnGroupEmpty);
		}

		return true;
	}

	protected void Update()
	{
		if (m_bStopped)
			return;

		if (!m_Campaign)
		{
			Fail("CAMPAIGN_LOST", "SCR_GameModeCampaign is no longer available");
			return;
		}

		if (!m_Campaign.IsRunning())
		{
			// On dedicated startup HasStarted() becomes true several frames before RUNNING.
			if (m_bCampaignWasRunning)
			{
				if (m_VictorySystem && m_VictorySystem.IsEnded())
				{
					if (m_VictorySystem.ConfirmMatchEnd(m_Campaign))
						FinalizeResult();
				}
				else
				{
					AICF_Stage1Diagnostics.Result(false, "reason=STOCK_MATCH_ENDED_BEFORE_AICF_VICTORY");
					m_bResultLogged = true;
					Stop(false);
				}
			}
			return;
		}
		m_bCampaignWasRunning = true;

		ProcessFaction(m_USState, m_USFaction);
		ProcessFaction(m_USSRState, m_USSRFaction);
		// Base ownership changes arrive before the delayed graph rebuild. Pause
		// vehicle orchestration only for that scheduled one-second window so a
		// newly friendly old target can be rerouted. If Build later fails, normal
		// safety/fallback/cleanup polling must resume instead of freezing vehicles.
		if (m_VehicleCoordinator && !m_bReplanScheduled)
			m_VehicleCoordinator.Update(m_USState, m_USFaction, m_USSRState, m_USSRFaction);
		if (m_GroupMapMarkers)
			m_GroupMapMarkers.Sync(m_USState, m_USSRState);
		TryLogRosterReady();
		EvaluateVictory();
	}

	protected void ProcessFaction(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		if (!factionState || !faction)
			return;

		int nowMs = System.GetTickCount();
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot)
				continue;

			if (slot.GetState() == AICF_EGroupSlotState.SPAWNING)
			{
				SCR_AIGroup spawningGroup = slot.GetGroup();
				if (spawningGroup && spawningGroup.GetAgentsCount() > 0 && slot.MarkReady())
					CompleteReadyDeployment(factionState, faction, slot);
				else if (System.GetTickCount(slot.GetSpawnStartedAtMs()) >= GROUP_SPAWN_TIMEOUT_MS)
					HandleSpawnTimeout(factionState, faction, slot);
				continue;
			}

			if (slot.GetState() == AICF_EGroupSlotState.READY && !slot.GetGroup())
			{
				HandleLostReadyGroup(factionState, faction, slot);
				continue;
			}

			if (slot.IsReinforcementDue(nowMs))
				TryStartReplacement(factionState, faction, slot, nowMs);
		}
	}

	protected void CompleteReadyDeployment(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (!slot || !slot.IsCombatReady())
			return;

		int managedAgents;
		int recoveredFromMaxLOD;
		if (!m_ManagedAILODPolicy.KeepCaptureEligible(slot.GetGroup(), managedAgents, recoveredFromMaxLOD))
		{
			AICF_Stage1Diagnostics.Error(
				"MANAGED_AI_LOD_POLICY_FAILED",
				string.Format("faction=%1 slot=%2", faction.GetFactionKey(), slot.GetSlotId()));
			return;
		}

		bool replacement = slot.IsReplacementDeployment();
		string reason = "INITIAL_DEPLOYMENT";
		AICF_EDeploymentKind deploymentKind = AICF_EDeploymentKind.INITIAL;
		if (replacement)
		{
			reason = "REPLACEMENT_DEPLOYMENT";
			deploymentKind = AICF_EDeploymentKind.REPLACEMENT;
		}

		if (!slot.GetWaypoint() && !m_OrderPlanner.AssignOrder(slot, faction, m_ObjectiveGraph, m_TargetSelector, reason))
			return;

		bool cohesionApplied = m_GroupCohesionPolicy.Apply(slot.GetGroup());
		AICF_Stage2Diagnostics.Info(
			"GROUP_COHESION_POLICY_APPLIED",
			string.Format(
				"faction=%1 slot=%2 formation=%3 displacement=0 success=%4",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				AICF_GroupCohesionPolicy.FORMATION_NAME,
				cohesionApplied));

		int ticketsBefore = factionState.GetTickets();
		if (!slot.CanCommitDeploymentReady())
		{
			AICF_Stage1Diagnostics.Error(
				"SLOT_COMMIT_PRECONDITION_FAILED",
				string.Format("faction=%1 slot=%2", faction.GetFactionKey(), slot.GetSlotId()));
			return;
		}

		if (!factionState.TryCommitDeployment(deploymentKind))
		{
			AICF_Stage1Diagnostics.Error(
				"TICKET_COMMIT_FAILED",
				string.Format("faction=%1 slot=%2 tickets=%3", faction.GetFactionKey(), slot.GetSlotId(), ticketsBefore));
			return;
		}

		if (!slot.CommitDeploymentReady())
		{
			AICF_Stage1Diagnostics.Error(
				"SLOT_COMMIT_FAILED",
				string.Format("faction=%1 slot=%2", faction.GetFactionKey(), slot.GetSlotId()));
			return;
		}

		AICF_Stage1Diagnostics.Info(
			"SLOT_READY",
			string.Format(
				"faction=%1 slot=%2 group=%3 agents=%4 deployment=%5 lod_policy=PREVENT_MAX recovered_from_max_lod=%6",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				GroupKey(slot.GetGroup()),
				slot.GetGroup().GetAgentsCount(),
				reason,
				recoveredFromMaxLOD));

		if (replacement)
		{
			m_bObservedReinforcement = true;
			m_bObservedTicketDebit = true;
			AICF_Stage1Diagnostics.Info(
				"TICKET_DEBIT",
				string.Format(
					"faction=%1 slot=%2 before=%3 after=%4 reason=REPLACEMENT_SPAWNED",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					ticketsBefore,
					factionState.GetTickets()));
			SyncTickets();
		}
	}

	protected void TryStartReplacement(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		int nowMs)
	{
		int scheduledReadyAtMs = slot.GetReinforcementReadyAtMs();
		if (CountConcurrentReplacementSpawns() >= m_Stage2Config.GetMaxConcurrentReplacementSpawns())
		{
			// This is intentional load shedding, not a reinforcement timing fault.
			// Move the due time with the queue so the eventual start is measured from
			// the slot's actual scheduled turn rather than its original contention time.
			slot.PostponeReinforcementUntil(nowMs + UPDATE_INTERVAL_MS);
			if (slot.MarkLoadBlockReported())
			{
				AICF_Stage2Diagnostics.Info(
					"LOAD_LIMIT_BLOCKED",
					string.Format(
						"faction=%1 slot=%2 reason=SPAWN_CONCURRENCY active=%3 limit=%4",
						faction.GetFactionKey(),
						slot.GetSlotId(),
						CountConcurrentReplacementSpawns(),
						m_Stage2Config.GetMaxConcurrentReplacementSpawns()));
			}
			return;
		}
		bool releasedFromLoadQueue = slot.HasLoadBlockReport();
		slot.ResetLoadBlockReported();
		if (releasedFromLoadQueue)
		{
			AICF_Stage2Diagnostics.Info(
				"LOAD_LIMIT_RELEASED",
				string.Format(
					"faction=%1 slot=%2 action=START_REPLACEMENT",
					faction.GetFactionKey(),
					slot.GetSlotId()));
		}

		if (!factionState.TryReserveDeployment(AICF_EDeploymentKind.REPLACEMENT))
			return;

		if (!slot.BeginReplacementSpawn(nowMs))
		{
			factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);
			return;
		}

		AICF_Stage2Diagnostics.Info(
			"SPAWN_ATTEMPT_STARTED",
			string.Format(
				"faction=%1 slot=%2 generation=%3 kind=REPLACEMENT",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration()));

		int replacementOvershootMs = nowMs - scheduledReadyAtMs;
		if (replacementOvershootMs < 0 || replacementOvershootMs > 2000)
		{
			AICF_Stage1Diagnostics.Error(
				"REINFORCEMENT_TIMING_VIOLATION",
				string.Format(
					"faction=%1 slot=%2 overshoot_ms=%3",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					replacementOvershootMs));
		}

		if (CountProjectedManagedAgents() > m_Config.GetMaxManagedAgents())
		{
			AICF_Stage1Diagnostics.Warning(
				"REINFORCEMENT_BLOCKED",
				string.Format("faction=%1 slot=%2 reason=AI_LIMIT", faction.GetFactionKey(), slot.GetSlotId()));
			slot.ReturnSpawnToWait(nowMs + REINFORCEMENT_RETRY_MS);
			factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);
			return;
		}

		SCR_AIGroup group;
		SCR_CampaignMilitaryBaseComponent spawnBase;
		if (!m_ReinforcementSystem.TrySpawn(
			m_Campaign,
			faction,
			slot,
			m_ConflictAdapter,
			m_GroupSpawner,
			group,
			spawnBase))
		{
			slot.ReturnSpawnToWait(nowMs + REINFORCEMENT_RETRY_MS);
			factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);
			return;
		}

		if (!BindManagedGroup(factionState, faction, slot, group, "REPLACEMENT"))
		{
			RplComponent.DeleteRplEntity(group, false);
			slot.ReturnSpawnToWait(nowMs + REINFORCEMENT_RETRY_MS);
			factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);
			return;
		}

		group.GetOnEmpty().Insert(OnGroupEmpty);
		AICF_Stage1Diagnostics.Info(
			"REINFORCEMENT_SPAWNED",
			string.Format(
				"faction=%1 slot=%2 group=%3 base=%4",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				GroupKey(group),
				AICF_Stage1Diagnostics.BaseKey(spawnBase)));
	}

	protected bool BindManagedGroup(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		SCR_AIGroup group,
		string deploymentKind)
	{
		if (!factionState || !faction || !slot || !group)
			return false;

		string rejectionReason;
		if (slot.GetGroup())
			rejectionReason = "SLOT_ALREADY_BOUND";
		else if (IsGroupBoundElsewhere(group, slot))
			rejectionReason = "GROUP_ALREADY_MANAGED";
		else if (!slot.BindSpawnedGroup(group))
			rejectionReason = "SLOT_STATE_REJECTED";

		if (!rejectionReason.IsEmpty())
		{
			m_iDuplicateSpawnsPrevented++;
			AICF_Stage2Diagnostics.Warning(
				"DUPLICATE_SPAWN_PREVENTED",
				string.Format(
					"faction=%1 slot=%2 generation=%3 group=%4 reason=%5",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					slot.GetSpawnGeneration(),
					GroupKey(group),
					rejectionReason));
			return false;
		}

		AICF_Stage2Diagnostics.Info(
			"SPAWN_BOUND",
			string.Format(
				"faction=%1 slot=%2 generation=%3 group=%4 kind=%5",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				GroupKey(group),
				deploymentKind));
		return true;
	}

	protected bool IsGroupBoundElsewhere(SCR_AIGroup group, AICF_GroupSlot expectedSlot)
	{
		return IsGroupBoundInFaction(m_USState, group, expectedSlot) ||
			IsGroupBoundInFaction(m_USSRState, group, expectedSlot);
	}

	protected bool IsGroupBoundInFaction(
		AICF_FactionState factionState,
		SCR_AIGroup group,
		AICF_GroupSlot expectedSlot)
	{
		if (!factionState || !group)
			return false;

		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (slot && slot != expectedSlot && slot.GetGroup() == group)
				return true;
		}

		return false;
	}

	protected int CountConcurrentReplacementSpawns()
	{
		return CountFactionReplacementSpawns(m_USState) + CountFactionReplacementSpawns(m_USSRState);
	}

	protected int CountFactionReplacementSpawns(AICF_FactionState factionState)
	{
		if (!factionState)
			return 0;

		int count;
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (slot && slot.GetState() == AICF_EGroupSlotState.SPAWNING && slot.IsReplacementDeployment())
				count++;
		}

		return count;
	}

	protected void HandleSpawnTimeout(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		bool replacement = slot.IsReplacementDeployment();
		SCR_AIGroup group = slot.GetGroup();
		if (group)
		{
			m_ManagedAILODPolicy.Release(group);
			group.GetOnEmpty().Remove(OnGroupEmpty);
			RplComponent.DeleteRplEntity(group, false);
		}

		if (replacement)
			factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);

		if (!slot.MarkDestroyed())
			return;

		AICF_Stage1Diagnostics.Error(
			"GROUP_SPAWN_TIMEOUT",
			string.Format("faction=%1 slot=%2 timeout_ms=%3", faction.GetFactionKey(), slot.GetSlotId(), GROUP_SPAWN_TIMEOUT_MS));

		if (!replacement)
		{
			Fail("INITIAL_GROUP_SPAWN_TIMEOUT", string.Format("faction=%1 slot=%2", faction.GetFactionKey(), slot.GetSlotId()));
			return;
		}

		slot.BeginReinforcementWait(System.GetTickCount() + REINFORCEMENT_RETRY_MS);
	}

	protected void HandleLostReadyGroup(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		if (slot.IsReplacementDeployment())
			factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);

		m_OrderPlanner.ClearOrder(slot);
		if (!slot.MarkDestroyed())
			return;

		AICF_Stage1Diagnostics.Error(
			"READY_GROUP_LOST",
			string.Format("faction=%1 slot=%2", faction.GetFactionKey(), slot.GetSlotId()));
		slot.BeginReinforcementWait(System.GetTickCount() + m_Config.GetReinforcementDelayMs());
	}

	protected void OnGroupEmpty(AIGroup rawGroup)
	{
		if (m_bStopped || !Replication.IsServer())
			return;

		SCR_AIGroup group = SCR_AIGroup.Cast(rawGroup);
		if (!group)
			return;

		m_ManagedAILODPolicy.Release(group);
		group.GetOnEmpty().Remove(OnGroupEmpty);

		AICF_FactionState factionState;
		SCR_CampaignFaction faction;
		AICF_GroupSlot slot;
		if (!FindManagedGroup(group, factionState, faction, slot))
		{
			AICF_Stage1Diagnostics.Warning("GROUP_EMPTY_UNMANAGED", string.Format("group=%1", GroupKey(group)));
			return;
		}

		string groupKey = GroupKey(group);
		if (slot.IsReplacementDeployment())
			factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);
		m_OrderPlanner.ClearOrder(slot);
		if (!slot.MarkDestroyed())
			return;

		int readyAtAbsoluteMs = System.GetTickCount() + m_Config.GetReinforcementDelayMs();
		slot.BeginReinforcementWait(readyAtAbsoluteMs);
		int emptyAtElapsedMs = AICF_Stage1Diagnostics.GetElapsedMs();

		AICF_Stage1Diagnostics.InfoAt(
			"GROUP_EMPTY",
			string.Format("faction=%1 slot=%2 group=%3", faction.GetFactionKey(), slot.GetSlotId(), groupKey),
			emptyAtElapsedMs);
		AICF_Stage1Diagnostics.InfoAt(
			"REINFORCEMENT_SCHEDULED",
			string.Format(
				"faction=%1 slot=%2 ready_at_ms=%3 delay_ms=%4",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				emptyAtElapsedMs + m_Config.GetReinforcementDelayMs(),
				m_Config.GetReinforcementDelayMs()),
			emptyAtElapsedMs);

		EvaluateVictory();
	}

	protected bool FindManagedGroup(
		SCR_AIGroup group,
		out AICF_FactionState factionState,
		out SCR_CampaignFaction faction,
		out AICF_GroupSlot slot)
	{
		factionState = m_USState;
		faction = m_USFaction;
		slot = m_USState.FindSlotByGroup(group);
		if (slot)
			return true;

		factionState = m_USSRState;
		faction = m_USSRFaction;
		slot = m_USSRState.FindSlotByGroup(group);
		return slot != null;
	}

	protected void CommanderTick()
	{
		if (m_bStopped || !m_Campaign || !m_Campaign.IsRunning())
			return;

		if (m_bGraphRebuildNeeded)
		{
			// A base event already owns a one-second delayed rebuild. Do not race that
			// callback or revalidate against a stale graph and bypass retarget telemetry.
			if (!m_bReplanScheduled)
				ReplanAfterBaseChange();

			if (m_bGraphRebuildNeeded)
				return;
		}

		// A relay capture changes radio coverage synchronously. Stop this commander
		// pass immediately so no other slot is replanned against the old graph.
		if (RevalidateFactionOrders(m_USState, m_USFaction, "COMMANDER_REPLAN"))
			return;

		RevalidateFactionOrders(m_USSRState, m_USSRFaction, "COMMANDER_REPLAN");
	}

	// Returns true when this pass changed base ownership and invalidated the graph.
	protected bool RevalidateFactionOrders(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		string reason)
	{
		if (!factionState || !faction)
			return false;

		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot || !slot.IsCombatReady())
				continue;
			if (m_VehicleCoordinator && m_VehicleCoordinator.IsControllingMovement(slot))
				continue;
			// CommanderTick may observe the candidate between reliability polls, but
			// only ReliabilityTick is allowed to confirm or reject its stability.
			if (slot.HasPendingOrderRecovery())
				continue;

			// The stock CaptureRelay smart-action waypoint reaches Signal Hill in the
			// dedicated Conflict runtime, but can finish without invoking its user
			// action. Preserve the stock radio eligibility rules and complete that
			// authority operation only after a living managed agent has really arrived.
			if (TryCaptureArrivedRelay(slot, faction))
				return true;

			string failureReason = m_OrderPlanner.GetOrderFailureReason(slot, faction);
			if (failureReason.IsEmpty())
				continue;

			if (failureReason != "TARGET_INVALID")
			{
				TryRecoverOrder(slot, faction, failureReason);
				continue;
			}

			SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
			SCR_CampaignMilitaryBaseComponent excludedTarget;
			if (oldTarget && oldTarget.GetFaction() == faction)
				excludedTarget = oldTarget;
			m_OrderPlanner.AssignOrder(slot, faction, m_ObjectiveGraph, m_TargetSelector, reason, excludedTarget);
		}

		return false;
	}

	protected void ReliabilityTick()
	{
		if (m_bStopped || !m_Campaign || !m_Campaign.IsRunning())
			return;

		TryInjectTestOrderLoss();
		AuditLifecycleInvariants();
		if (m_bGraphRebuildNeeded || m_bReplanScheduled)
			return;

		ProcessFactionReliability(m_USState, m_USFaction);
		ProcessFactionReliability(m_USSRState, m_USSRFaction);
	}

	protected void TryInjectTestOrderLoss()
	{
		if (m_bTestOrderDropInjected || !m_bRosterReady || !m_Stage2Config.HasTestDropOrder() ||
			AICF_Stage1Diagnostics.GetElapsedMs() < m_Stage2Config.GetTestDropOrderAtMs())
		{
			return;
		}

		AICF_FactionState factionState = m_USState;
		if (m_Stage2Config.GetTestDropOrderFaction() == "USSR")
			factionState = m_USSRState;
		if (!factionState)
			return;

		AICF_GroupSlot slot = factionState.GetSlot(m_Stage2Config.GetTestDropOrderSlot());
		if (!slot || !slot.IsCombatReady() || !slot.GetWaypoint())
			return;
		if (m_VehicleCoordinator && m_VehicleCoordinator.IsControllingMovement(slot))
			return;

		string oldTarget = AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase());
		m_OrderPlanner.ClearOrder(slot);
		m_bTestOrderDropInjected = true;
		AICF_Stage2Diagnostics.Warning(
			"TEST_ORDER_DROPPED",
			string.Format(
				"faction=%1 slot=%2 old_target=%3",
				m_Stage2Config.GetTestDropOrderFaction(),
				m_Stage2Config.GetTestDropOrderSlot(),
				oldTarget));
	}

	protected void ProcessFactionReliability(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction)
	{
		if (!factionState || !faction)
			return;

		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot || !slot.IsCombatReady())
				continue;
			if (m_VehicleCoordinator && m_VehicleCoordinator.IsControllingMovement(slot))
				continue;

			// Relay waypoints complete through a stock smart action. Keep the existing
			// authority fallback active in the more frequent reliability pass as well.
			if (TryCaptureArrivedRelay(slot, faction))
				continue;
			if (slot.HasPendingOrderRecovery())
			{
				ProcessPendingOrderRecovery(factionState, slot, faction);
				continue;
			}

			string failureReason = m_OrderPlanner.GetOrderFailureReason(slot, faction);
			if (!failureReason.IsEmpty())
			{
				if (TryHoldCompletedOrderAtObjective(slot, faction, failureReason))
					continue;

				TryRecoverOrder(slot, faction, failureReason);
				continue;
			}

			MonitorGroupProgress(factionState, slot, faction);
		}
	}

	protected bool TryHoldCompletedOrderAtObjective(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string failureReason)
	{
		// A completed S&D/defend waypoint is expected while the group is fighting or
		// seizing the assigned base. Reissuing it every reliability tick interrupts
		// that activity and produces recovery churn.
		if (failureReason != "WAYPOINT_NOT_CURRENT" || !slot || !faction)
			return false;

		SCR_AIGroup group = slot.GetGroup();
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		AIWaypoint waypoint = slot.GetWaypoint();
		if (!group || !target || !target.GetOwner() || !target.IsInitialized() || !waypoint)
			return false;

		if (slot.GetRole() == AICF_EGroupRole.ATTACK)
		{
			if (target.GetFaction() == faction || !target.IsValidTarget(faction))
				return false;
		}
		else if (target.GetFaction() != faction)
		{
			return false;
		}

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!leader)
			return false;

		float distanceMeters = Math.Sqrt(vector.DistanceSqXZ(
			leader.GetOrigin(),
			waypoint.GetOrigin()));
		if (distanceMeters > STUCK_WATCHDOG_IGNORE_RADIUS_METERS)
		{
			slot.ClearObjectiveHold();
			return false;
		}

		slot.BeginObjectiveHold(target);
		int holdElapsedMs = slot.GetObjectiveHoldElapsedMs();
		if (holdElapsedMs >= m_Stage2Config.GetObjectiveHoldTimeoutMs())
		{
			AICF_Stage2Diagnostics.Warning(
				"OBJECTIVE_HOLD_EXPIRED",
				string.Format(
					"faction=%1 slot=%2 target=%3 distance_m=%4 hold_ms=%5 action=REBUILD_ORDER",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.BaseKey(target),
					distanceMeters,
					holdElapsedMs));
			slot.ClearObjectiveHold();
			return false;
		}

		slot.ConfirmAtObjective(target, distanceMeters);
		if (slot.MarkObjectiveHoldReported())
		{
			AICF_Stage2Diagnostics.Info(
				"ORDER_RECOVERY_SUPPRESSED",
				string.Format(
					"faction=%1 slot=%2 role=%3 target=%4 distance_m=%5 state=AT_OBJECTIVE timeout_ms=%6",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					AICF_Stage1Diagnostics.BaseKey(target),
					distanceMeters,
					m_Stage2Config.GetObjectiveHoldTimeoutMs()));
		}

		return true;
	}

	protected void ProcessPendingOrderRecovery(
		AICF_FactionState factionState,
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction)
	{
		if (!factionState || !slot || !faction || !slot.HasPendingOrderRecovery())
			return;

		if (!slot.IsPendingOrderRecoveryContextCurrent())
		{
			slot.ClearPendingOrderRecovery();
			return;
		}

		SCR_AIGroup group = slot.GetPendingOrderRecoveryGroup();
		SCR_CampaignMilitaryBaseComponent target = slot.GetPendingOrderRecoveryTargetBase();
		AIWaypoint expectedWaypoint = slot.GetPendingOrderRecoveryWaypoint();
		string originalCause = slot.GetPendingOrderRecoveryCause();
		bool alreadyCountsAsStuck = slot.PendingOrderRecoveryCountsAsStuckRecovery();
		int lifetimeMs = slot.GetPendingOrderRecoveryElapsedMs();
		string failureReason = m_OrderPlanner.GetOrderFailureReason(slot, faction);
		if (failureReason == "TARGET_INVALID")
		{
			slot.ClearPendingOrderRecovery();
			return;
		}

		AIWaypoint currentWaypoint = group.GetCurrentWaypoint();
		array<AIWaypoint> waypointQueue = {};
		int queueCount = group.GetWaypoints(waypointQueue);
		bool trackedInQueue = waypointQueue.Contains(expectedWaypoint);
		if (failureReason.IsEmpty() && currentWaypoint == expectedWaypoint && trackedInQueue)
		{
			int stablePolls = slot.RecordPendingOrderRecoveryStablePoll();
			int stableMs = slot.GetPendingOrderRecoveryStableElapsedMs();
			int requiredStableMs = m_Stage2Config.GetReliabilityIntervalMs() * ORDER_RECOVERY_STABLE_INTERVALS;
			if (requiredStableMs < ORDER_RECOVERY_MIN_STABLE_MS)
				requiredStableMs = ORDER_RECOVERY_MIN_STABLE_MS;
			int requiredStablePolls = ORDER_RECOVERY_STABLE_POLLS + ORDER_RECOVERY_INITIAL_STABLE_OBSERVATION;
			bool minimumPollsSatisfied = !(stablePolls < ORDER_RECOVERY_STABLE_POLLS +
				ORDER_RECOVERY_INITIAL_STABLE_OBSERVATION);
			bool durabilitySatisfied = minimumPollsSatisfied && stableMs >= requiredStableMs;

			// Keep candidate telemetry useful without producing another per-poll churn:
			// report the first observation, the poll threshold, and final durability.
			if (stablePolls == 1 || stablePolls == requiredStablePolls || durabilitySatisfied)
			{
				string stabilityDetails = string.Format(
					"faction=%1 slot=%2 role=%3 target=%4 waypoint=%5 stable_polls=%6 required_polls=%7 stable_ms=%8",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					AICF_Stage1Diagnostics.BaseKey(target),
					expectedWaypoint.GetID(),
					stablePolls,
					requiredStablePolls,
					stableMs);
				stabilityDetails += string.Format(
					" required_stable_ms=%1 candidate_ms=%2 tracked_in_queue=%3 queue_count=%4 durable=%5 state=DURABILITY_SAMPLE",
					requiredStableMs,
					lifetimeMs,
					trackedInQueue,
					queueCount,
					durabilitySatisfied);
				AICF_Stage2Diagnostics.Info("ORDER_RECOVERY_STABILITY", stabilityDetails);
			}

			if (!durabilitySatisfied)
				return;

			string confirmedWaypointId = string.Format("%1", expectedWaypoint.GetID());
			slot.ClearPendingOrderRecovery();
			m_iOrderRecoveries++;
			string recoveredDetails = string.Format(
				"faction=%1 slot=%2 role=%3 cause=%4 target=%5 waypoint=%6 stable_polls=%7 stable_ms=%8",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					originalCause,
					AICF_Stage1Diagnostics.BaseKey(target),
					confirmedWaypointId,
					stablePolls,
					stableMs);
			recoveredDetails += string.Format(
				" required_stable_ms=%1 candidate_ms=%2",
					requiredStableMs,
					lifetimeMs);
			AICF_Stage2Diagnostics.Info("ORDER_RECOVERED", recoveredDetails);
			return;
		}

		if (TryHoldCompletedOrderAtObjective(slot, faction, failureReason))
		{
			slot.ClearPendingOrderRecovery();
			return;
		}

		string expectedWaypointId = string.Format("%1", expectedWaypoint.GetID());
		string currentWaypointId = "NONE";
		if (currentWaypoint)
			currentWaypointId = string.Format("%1", currentWaypoint.GetID());

		float distanceMeters = -1.0;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		vector targetPosition;
		if (leader && m_OrderPlanner.TryResolveTargetPosition(target, slot.GetRole(), targetPosition))
		{
			distanceMeters = Math.Sqrt(vector.DistanceSqXZ(
				leader.GetOrigin(),
				targetPosition));
		}
		else if (leader && expectedWaypoint)
		{
			distanceMeters = Math.Sqrt(vector.DistanceSqXZ(
				leader.GetOrigin(),
				expectedWaypoint.GetOrigin()));
		}

		int recoveryAttempt = slot.GetStuckRecoveryCount() + 1;
		if (alreadyCountsAsStuck)
			recoveryAttempt = slot.GetStuckRecoveryCount();
		int stablePollsBeforeFailure = slot.GetPendingOrderRecoveryStablePolls();
		int stableMsBeforeFailure = slot.GetPendingOrderRecoveryStableElapsedMs();
		string unstableDetails = string.Format(
			"faction=%1 slot=%2 role=%3 original_cause=%4 failure=%5 target=%6 distance_m=%7 expected_waypoint=%8",
			faction.GetFactionKey(),
			slot.GetSlotId(),
			AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
			originalCause,
			failureReason,
			AICF_Stage1Diagnostics.BaseKey(target),
			distanceMeters,
			expectedWaypointId);
		unstableDetails += string.Format(
			" current_waypoint=%1 tracked_in_queue=%2 queue_count=%3 lifetime_ms=%4 stable_polls=%5 stable_ms=%6",
			currentWaypointId,
			trackedInQueue,
			queueCount,
			lifetimeMs,
			stablePollsBeforeFailure,
			stableMsBeforeFailure);
		unstableDetails += string.Format(
			" attempt=%1 already_counted_as_stuck=%2",
			recoveryAttempt,
			alreadyCountsAsStuck);
		AICF_Stage2Diagnostics.Warning("ORDER_RECOVERY_UNSTABLE", unstableDetails);

		slot.ClearPendingOrderRecovery();
		if (!alreadyCountsAsStuck)
		{
			m_iStuckDetections++;
			slot.RecordStuckRecovery(distanceMeters);
		}

		if (slot.GetStuckRecoveryCount() >= m_Stage2Config.GetMaxStuckRecoveries())
		{
			RecyclePersistentStuckGroup(factionState, faction, slot, target, distanceMeters);
			return;
		}

		TryRecoverOrder(slot, faction, failureReason);
	}

	protected void TryRecoverOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string failureReason)
	{
		if (!slot || !faction || slot.HasPendingOrderRecovery())
			return;
		if (!slot.CanAttemptOrderRecovery(m_Stage2Config.GetOrderRecoveryRetryMs()))
			return;

		slot.MarkOrderRecoveryAttempt();
		m_iOrderRecoveryAttempts++;
		m_OrderPlanner.RecoverOrder(
			slot,
			faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			failureReason);
	}

	protected void MonitorGroupProgress(
		AICF_FactionState factionState,
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction)
	{
		if (!m_Stage2Config.GetStuckWatchdogEnabled())
			return;

		SCR_AIGroup group = slot.GetGroup();
		AIWaypoint waypoint = slot.GetWaypoint();
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (!group || !waypoint || !target)
			return;

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!leader)
			return;

		float distanceMeters = Math.Sqrt(vector.DistanceSqXZ(leader.GetOrigin(), waypoint.GetOrigin()));
		if (distanceMeters <= STUCK_WATCHDOG_IGNORE_RADIUS_METERS)
		{
			slot.ConfirmAtObjective(target, distanceMeters);
			return;
		}

		slot.ObserveProgress(
			target,
			distanceMeters,
			m_Stage2Config.GetStuckProgressMeters());
		if (!slot.IsStuck(m_Stage2Config.GetStuckTimeoutMs()))
			return;

		int recoveryAttempt = slot.GetStuckRecoveryCount() + 1;
		m_iStuckDetections++;
		AICF_Stage2Diagnostics.Warning(
			"GROUP_STUCK_DETECTED",
			string.Format(
				"faction=%1 slot=%2 role=%3 target=%4 distance_m=%5 timeout_ms=%6 attempt=%7",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
				AICF_Stage1Diagnostics.BaseKey(target),
				distanceMeters,
				m_Stage2Config.GetStuckTimeoutMs(),
				recoveryAttempt));

		// Allow the configured number of real route rebuilds. If all of them fail to
		// produce leader progress, retire this group through the normal replacement
		// lifecycle instead of rebuilding waypoints forever.
		if (slot.GetStuckRecoveryCount() >= m_Stage2Config.GetMaxStuckRecoveries())
		{
			RecyclePersistentStuckGroup(factionState, faction, slot, target, distanceMeters);
			return;
		}

		bool recovered = m_OrderPlanner.RebuildCurrentOrder(slot, faction, "STUCK_ROUTE_REBUILD");
		if (!recovered)
		{
			recovered = m_OrderPlanner.RecoverOrder(
				slot,
				faction,
				m_ObjectiveGraph,
				m_TargetSelector,
				"STUCK_TARGET_INVALID",
				true);
		}

		slot.RecordStuckRecovery(distanceMeters);
		if (recovered)
			m_iStuckRecoveries++;

		AICF_Stage2Diagnostics.Info(
			"GROUP_STUCK_RECOVERY",
			string.Format(
				"faction=%1 slot=%2 action=REBUILD_ORDER success=%3 attempt=%4",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				recovered,
				recoveryAttempt));

	}

	protected void RecyclePersistentStuckGroup(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent target,
		float distanceMeters)
	{
		if (!factionState || !faction || !slot || !slot.MarkPersistentStuckReported())
			return;

		SCR_AIGroup group = slot.GetGroup();
		if (!group)
			return;

		string groupKey = GroupKey(group);
		AICF_Stage2Diagnostics.Warning(
			"GROUP_STUCK_PERSISTENT",
			string.Format(
				"faction=%1 slot=%2 group=%3 target=%4 distance_m=%5 recoveries=%6 action=RECYCLE_GROUP ticket_policy=STANDARD_REPLACEMENT",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				groupKey,
				AICF_Stage1Diagnostics.BaseKey(target),
				distanceMeters,
				slot.GetStuckRecoveryCount()));

		m_ManagedAILODPolicy.Release(group);
		group.GetOnEmpty().Remove(OnGroupEmpty);
		m_OrderPlanner.ClearOrder(slot);
		if (!slot.MarkDestroyed())
		{
			AICF_Stage2Diagnostics.Error(
				"STUCK_RECYCLE_STATE_REJECTED",
				string.Format("faction=%1 slot=%2", faction.GetFactionKey(), slot.GetSlotId()));
			return;
		}

		int recycledAtElapsedMs = AICF_Stage1Diagnostics.GetElapsedMs();
		int readyAtAbsoluteMs = System.GetTickCount() + m_Config.GetReinforcementDelayMs();
		if (!slot.BeginReinforcementWait(readyAtAbsoluteMs))
		{
			AICF_Stage2Diagnostics.Error(
				"STUCK_RECYCLE_SCHEDULE_REJECTED",
				string.Format("faction=%1 slot=%2", faction.GetFactionKey(), slot.GetSlotId()));
			return;
		}

		m_iStuckRecycles++;
		AICF_Stage2Diagnostics.Info(
			"GROUP_RECYCLED",
			string.Format(
				"faction=%1 slot=%2 old_group=%3 cause=PERSISTENT_STUCK replacement_cost=%4",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				groupKey,
				m_Config.GetReplacementTicketCost()));
		AICF_Stage1Diagnostics.InfoAt(
			"REINFORCEMENT_SCHEDULED",
			string.Format(
				"faction=%1 slot=%2 ready_at_ms=%3 delay_ms=%4 reason=PERSISTENT_STUCK",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				recycledAtElapsedMs + m_Config.GetReinforcementDelayMs(),
				m_Config.GetReinforcementDelayMs()),
			recycledAtElapsedMs);

		RplComponent.DeleteRplEntity(group, false);
		EvaluateVictory();
	}

	protected void AuditLifecycleInvariants()
	{
		m_iLifecycleAudits++;
		array<SCR_AIGroup> seenGroups = {};
		AuditFactionLifecycle(m_USState, m_USFaction, seenGroups);
		AuditFactionLifecycle(m_USSRState, m_USSRFaction, seenGroups);

		int concurrentSpawns = CountConcurrentReplacementSpawns();
		if (concurrentSpawns > m_Stage2Config.GetMaxConcurrentReplacementSpawns())
		{
			AICF_Stage2Diagnostics.Error(
				"SPAWN_CONCURRENCY_INVARIANT_FAILED",
				string.Format(
					"active=%1 limit=%2",
					concurrentSpawns,
					m_Stage2Config.GetMaxConcurrentReplacementSpawns()));
		}
	}

	protected void AuditFactionLifecycle(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		array<SCR_AIGroup> seenGroups)
	{
		if (!factionState || !faction)
			return;

		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot)
				continue;

			SCR_AIGroup group = slot.GetGroup();
			if (!group)
				continue;

			if (seenGroups.Contains(group))
			{
				AICF_Stage2Diagnostics.Error(
					"DUPLICATE_GROUP_BINDING",
					string.Format(
						"faction=%1 slot=%2 group=%3",
						faction.GetFactionKey(),
						slotId,
						GroupKey(group)));
				continue;
			}
			seenGroups.Insert(group);

			AICF_EGroupSlotState state = slot.GetState();
			if (state != AICF_EGroupSlotState.SPAWNING && state != AICF_EGroupSlotState.READY)
			{
				AICF_Stage2Diagnostics.Error(
					"GROUP_SLOT_STATE_INVARIANT_FAILED",
					string.Format(
						"faction=%1 slot=%2 state=%3 group=%4",
						faction.GetFactionKey(),
						slotId,
						AICF_Stage1Diagnostics.StateToString(state),
						GroupKey(group)));
			}
		}
	}

	protected bool TryCaptureArrivedRelay(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		if (!Replication.IsServer() || !slot || !faction || slot.GetRole() != AICF_EGroupRole.ATTACK)
			return false;

		SCR_CampaignMilitaryBaseComponent relay = slot.GetTargetBase();
		if (!relay || !relay.GetOwner() || !relay.IsInitialized() ||
			relay.GetType() != SCR_ECampaignBaseType.RELAY ||
			relay.GetFaction() == faction || !relay.IsValidTarget(faction))
		{
			return false;
		}

		// Only a slot that was explicitly assigned the stock relay smart-action
		// waypoint may use the server-side completion fallback.
		if (!SCR_SmartActionWaypoint.Cast(slot.GetWaypoint()))
			return false;

		SCR_AIGroup group = slot.GetGroup();
		if (!group)
			return false;

		int arrivedAgents = CountAliveAgentsNearRelay(group, relay);
		if (arrivedAgents <= 0)
			return false;

		FactionKey oldOwner = "NONE";
		if (relay.GetFaction())
			oldOwner = relay.GetFaction().GetFactionKey();

		relay.CaptureRelay(faction, SCR_CampaignMilitaryBaseComponent.INVALID_PLAYER_INDEX);
		if (relay.GetFaction() != faction)
			return false;

		AICF_Stage1Diagnostics.Info(
			"RELAY_CAPTURED_BY_AI",
			string.Format(
				"faction=%1 slot=%2 base=%3 old_owner=%4 agents_in_radius=%5 radius_m=%6",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.BaseKey(relay),
				oldOwner,
				arrivedAgents,
				RELAY_CAPTURE_RADIUS_METERS));
		return true;
	}

	protected int CountAliveAgentsNearRelay(
		SCR_AIGroup group,
		SCR_CampaignMilitaryBaseComponent relay)
	{
		if (!group || !relay || !relay.GetOwner())
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		vector relayPosition = relay.GetOwner().GetOrigin();
		float radiusSquared = RELAY_CAPTURE_RADIUS_METERS * RELAY_CAPTURE_RADIUS_METERS;
		int count;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity controlledEntity = agent.GetControlledEntity();
			ChimeraCharacter character = ChimeraCharacter.Cast(controlledEntity);
			if (!character)
				continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
				continue;

			if (vector.DistanceSqXZ(controlledEntity.GetOrigin(), relayPosition) <= radiusSquared)
				count++;
		}

		return count;
	}

	protected void OnBaseFactionChanged(SCR_MilitaryBaseComponent rawBase, Faction newFaction)
	{
		if (m_bStopped || !Replication.IsServer())
			return;

		SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(rawBase);
		if (!base || !base.GetOwner() || !base.IsInitialized())
			return;

		FactionKey oldOwner = GetCachedBaseOwner(base);
		FactionKey newOwner = "NONE";
		if (newFaction)
			newOwner = newFaction.GetFactionKey();

		SetCachedBaseOwner(base, newOwner);
		if (oldOwner == newOwner)
			return;

		m_bObservedCapture = true;
		m_iLastCaptureAtMs = AICF_Stage1Diagnostics.GetElapsedMs();
		m_bGraphRebuildNeeded = true;
		m_LastChangedBase = base;
		AICF_Stage1Diagnostics.Info(
			"BASE_OWNER_CHANGED",
			string.Format(
				"base=%1 old_owner=%2 new_owner=%3",
				AICF_Stage1Diagnostics.BaseKey(base),
				oldOwner,
				newOwner));

		if (!m_bReplanScheduled)
		{
			m_bReplanScheduled = true;
			GetGame().GetCallqueue().CallLater(ReplanAfterBaseChange, BASE_REPLAN_DELAY_MS, false);
		}
	}

	protected void ReplanAfterBaseChange()
	{
		m_bReplanScheduled = false;
		if (m_bStopped || !m_Campaign || !m_Campaign.IsRunning())
			return;

		array<SCR_CampaignMilitaryBaseComponent> objectiveBases = {};
		array<SCR_CampaignMilitaryBaseComponent> graphBases = {};
		if (!m_ConflictAdapter.CollectBases(m_Campaign, objectiveBases, graphBases) ||
			!m_ObjectiveGraph.Build(graphBases, objectiveBases))
		{
			AICF_Stage1Diagnostics.Warning("GRAPH_REBUILD_FAILED", "Will retry after the next commander interval");
			return;
		}

		ReplanFactionAfterBaseChange(m_USState, m_USFaction);
		ReplanFactionAfterBaseChange(m_USSRState, m_USSRFaction);
		m_bGraphRebuildNeeded = false;
		m_LastChangedBase = null;
	}

	protected void ReplanFactionAfterBaseChange(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction)
	{
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot)
				continue;

			// A rebuilt graph is a new target-availability generation. Allow one fresh
			// diagnostic if the new snapshot still has no legal destination.
			slot.ResetTargetUnavailableReport();
			if (!slot.IsCombatReady())
				continue;

			SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
			bool reassigned = false;
			if (m_VehicleCoordinator && m_VehicleCoordinator.IsControllingMovement(slot))
			{
				reassigned = m_VehicleCoordinator.ReplanControlledMovement(
					slot,
					faction,
					"BASE_OWNER_CHANGED",
					m_LastChangedBase);
			}
			else if (!m_OrderPlanner.IsOrderValid(slot, faction))
			{
				reassigned = m_OrderPlanner.AssignOrder(
					slot,
					faction,
					m_ObjectiveGraph,
					m_TargetSelector,
					"BASE_OWNER_CHANGED",
					m_LastChangedBase);
			}

			if (reassigned)
			{
				if (oldTarget && slot.GetTargetBase() != oldTarget)
				{
					m_bObservedRetarget = true;
					int retargetLatencyMs = AICF_Stage1Diagnostics.GetElapsedMs() - m_iLastCaptureAtMs;
					if (retargetLatencyMs <= 2 * m_Config.GetCommanderIntervalMs())
						m_bObservedRetargetWithinDeadline = true;
					else
						AICF_Stage1Diagnostics.Error(
							"RETARGET_DEADLINE_MISSED",
							string.Format("faction=%1 slot=%2 latency_ms=%3", faction.GetFactionKey(), slot.GetSlotId(), retargetLatencyMs));
				}
			}
		}
	}

	protected void OnPlayerConnected(int playerId)
	{
		if (m_bStopped)
			return;

		GetGame().GetCallqueue().CallLater(TryLogPlayerJoined, PLAYER_FACTION_RETRY_MS, false, playerId, 0);
	}

	protected void TryLogPlayerJoined(int playerId, int attempt)
	{
		if (m_bStopped)
			return;

		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		Faction faction;
		if (factionManager)
			faction = factionManager.GetPlayerFaction(playerId);

		if (faction && (faction.GetFactionKey() == "US" || faction.GetFactionKey() == "USSR"))
		{
			m_bObservedPlayerJoin = true;
			m_sObservedPlayerFaction = faction.GetFactionKey();
			AICF_Stage1Diagnostics.Info(
				"PLAYER_JOINED",
				string.Format("player=%1 faction=%2", playerId, faction.GetFactionKey()));
			return;
		}

		attempt++;
		if (attempt < MAX_PLAYER_FACTION_ATTEMPTS)
			GetGame().GetCallqueue().CallLater(TryLogPlayerJoined, PLAYER_FACTION_RETRY_MS, false, playerId, attempt);
	}

	protected void TryLogRosterReady()
	{
		if (m_bRosterReady || !m_USState || !m_USSRState)
			return;

		if (m_USState.CountSlotsByState(AICF_EGroupSlotState.READY) != AICF_Stage1Config.GROUP_SLOTS_PER_FACTION ||
			m_USSRState.CountSlotsByState(AICF_EGroupSlotState.READY) != AICF_Stage1Config.GROUP_SLOTS_PER_FACTION)
			return;

		m_bRosterReady = true;
		AICF_Stage1Diagnostics.Info(
			"ROSTER_READY",
			string.Format(
				"us_groups=%1 ussr_groups=%2 us_attack=%3 us_defend=%4 us_reserve=%5 ussr_attack=%6 ussr_defend=%7 ussr_reserve=%8",
				m_USState.CountSlotsByState(AICF_EGroupSlotState.READY),
				m_USSRState.CountSlotsByState(AICF_EGroupSlotState.READY),
				m_USState.CountSlotsByRole(AICF_EGroupRole.ATTACK),
				m_USState.CountSlotsByRole(AICF_EGroupRole.DEFEND),
				m_USState.CountSlotsByRole(AICF_EGroupRole.RESERVE),
				m_USSRState.CountSlotsByRole(AICF_EGroupRole.ATTACK),
				m_USSRState.CountSlotsByRole(AICF_EGroupRole.DEFEND),
				m_USSRState.CountSlotsByRole(AICF_EGroupRole.RESERVE)));
	}

	protected void Heartbeat()
	{
		if (m_bStopped || !m_Campaign || !m_Campaign.IsRunning())
			return;

		AICF_Stage1Diagnostics.Info(
			"HEARTBEAT",
			string.Format(
				"state=RUNNING us_tickets=%1 ussr_tickets=%2 us_combat_groups=%3 ussr_combat_groups=%4 managed_agents=%5",
				m_USState.GetTickets(),
				m_USSRState.GetTickets(),
				m_USState.CountSlotsByState(AICF_EGroupSlotState.READY),
				m_USSRState.CountSlotsByState(AICF_EGroupSlotState.READY),
				CountManagedAgents()));

		AICF_Stage2Diagnostics.Info(
			"RELIABILITY_HEARTBEAT",
			string.Format(
				"audits=%1 order_attempts=%2 order_recovered=%3 stuck_detected=%4 stuck_recovered=%5 stuck_recycled=%6 duplicate_spawns_prevented=%7 concurrent_spawns=%8 managed_agents=%9",
				m_iLifecycleAudits,
				m_iOrderRecoveryAttempts,
				m_iOrderRecoveries,
				m_iStuckDetections,
				m_iStuckRecoveries,
				m_iStuckRecycles,
				m_iDuplicateSpawnsPrevented,
				CountConcurrentReplacementSpawns(),
				CountManagedAgents()));

		if (m_VehicleCoordinator)
			m_VehicleCoordinator.Heartbeat(CountManagedAgents());
	}

	protected void EvaluateVictory()
	{
		if (m_bStopped || !m_VictorySystem)
			return;

		m_VictorySystem.EvaluateAndEnd(m_Campaign, m_USState, m_USSRState);
	}

	protected void FinalizeResult()
	{
		if (m_bStopped || m_bResultLogged)
			return;

		FactionKey expectedPlayerFaction = m_Config.GetExpectedPlayerFaction();
		bool playerFactionValid = true;
		if (m_Config.GetRequirePlayerForResult())
		{
			playerFactionValid = m_bObservedPlayerJoin;
			if (!expectedPlayerFaction.IsEmpty())
				playerFactionValid = m_sObservedPlayerFaction == expectedPlayerFaction;
		}

		bool success = m_bRosterReady &&
			m_bObservedCapture &&
			m_bObservedRetarget &&
			m_bObservedRetargetWithinDeadline &&
			m_bObservedReinforcement &&
			m_bObservedTicketDebit &&
			playerFactionValid &&
			m_ReinforcementSystem.HasRejectedUnsafeSite() &&
			!AICF_Stage1Diagnostics.HasErrors() &&
			!AICF_Stage2Diagnostics.HasErrors() &&
			(!m_Stage3Config.GetVehiclesEnabled() || !AICF_Stage3Diagnostics.HasErrors());

		AICF_Stage2Diagnostics.Info(
			"MATCH_RELIABILITY_SUMMARY",
			string.Format(
				"audits=%1 order_attempts=%2 order_recovered=%3 stuck_detected=%4 stuck_recovered=%5 stuck_recycled=%6 duplicate_spawns_prevented=%7 errors=%8",
				m_iLifecycleAudits,
				m_iOrderRecoveryAttempts,
				m_iOrderRecoveries,
				m_iStuckDetections,
				m_iStuckRecoveries,
				m_iStuckRecycles,
				m_iDuplicateSpawnsPrevented,
				AICF_Stage2Diagnostics.HasErrors()));

		m_bResultLogged = true;
		AICF_Stage1Diagnostics.Result(
			success,
			string.Format(
				"winner=%1 roster=%2 capture=%3 retarget=%4 retarget_deadline=%5 reinforcement=%6 ticket_debit=%7 player_faction=%8 unsafe_spawn_rejected=%9",
				m_VictorySystem.GetWinnerKey(),
				m_bRosterReady,
				m_bObservedCapture,
				m_bObservedRetarget,
				m_bObservedRetargetWithinDeadline,
				m_bObservedReinforcement,
				m_bObservedTicketDebit,
				m_sObservedPlayerFaction,
				m_ReinforcementSystem.HasRejectedUnsafeSite()));
		Stop(false);
	}

	protected int CountManagedAgents()
	{
		return CountFactionAgents(m_USState) + CountFactionAgents(m_USSRState);
	}

	protected int CountProjectedManagedAgents()
	{
		return CountProjectedFactionAgents(m_USState) + CountProjectedFactionAgents(m_USSRState);
	}

	protected int CountProjectedFactionAgents(AICF_FactionState factionState)
	{
		if (!factionState)
			return 0;

		int count;
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot)
				continue;

			SCR_AIGroup group = slot.GetGroup();
			if (group && group.GetAgentsCount() > 0)
				count += group.GetAgentsCount();
			else if (slot.GetState() == AICF_EGroupSlotState.SPAWNING)
				count += PENDING_GROUP_AGENT_BUDGET;
		}

		return count;
	}

	protected int CountFactionAgents(AICF_FactionState factionState)
	{
		if (!factionState)
			return 0;

		int count;
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (slot && slot.GetGroup())
				count += slot.GetGroup().GetAgentsCount();
		}

		return count;
	}

	protected void SyncTickets()
	{
		if (m_Campaign && m_USState && m_USSRState)
			m_Campaign.AICF_SetTickets(m_USState.GetTickets(), m_USSRState.GetTickets());
	}

	protected void Subscribe()
	{
		m_BaseSystem = SCR_MilitaryBaseSystem.GetInstance();
		if (m_BaseSystem)
			m_BaseSystem.GetOnBaseFactionChanged().Insert(OnBaseFactionChanged);

		m_Campaign.GetOnPlayerConnected().Insert(OnPlayerConnected);
		m_bSubscribed = true;
	}

	protected void CacheBaseOwners(array<SCR_CampaignMilitaryBaseComponent> bases)
	{
		m_aTrackedBases.Clear();
		m_aTrackedBaseOwners.Clear();
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			if (!base)
				continue;

			FactionKey ownerKey = "NONE";
			Faction owner = base.GetFaction();
			if (owner)
				ownerKey = owner.GetFactionKey();

			m_aTrackedBases.Insert(base);
			m_aTrackedBaseOwners.Insert(ownerKey);
		}
	}

	protected FactionKey GetCachedBaseOwner(SCR_CampaignMilitaryBaseComponent base)
	{
		int index = m_aTrackedBases.Find(base);
		if (index < 0 || index >= m_aTrackedBaseOwners.Count())
			return "NONE";

		return m_aTrackedBaseOwners[index];
	}

	protected void SetCachedBaseOwner(SCR_CampaignMilitaryBaseComponent base, FactionKey ownerKey)
	{
		int index = m_aTrackedBases.Find(base);
		if (index < 0)
		{
			m_aTrackedBases.Insert(base);
			m_aTrackedBaseOwners.Insert(ownerKey);
			return;
		}

		m_aTrackedBaseOwners[index] = ownerKey;
	}

	protected string GroupKey(SCR_AIGroup group)
	{
		if (!group)
			return "NONE";

		return group.GetID().ToString();
	}

	protected void Fail(string eventName, string message)
	{
		if (m_bStopped)
			return;

		AICF_Stage1Diagnostics.Error(eventName, message);
		if (!m_bResultLogged)
		{
			m_bResultLogged = true;
			AICF_Stage1Diagnostics.Result(false, string.Format("reason=%1 detail=%2", eventName, message));
		}
		Stop(true);
	}

	protected void Stop(bool cleanupEntities)
	{
		if (m_bStopped)
			return;

		m_bStopped = true;
		ScriptCallQueue callqueue = GetGame().GetCallqueue();
		callqueue.Remove(Update);
		callqueue.Remove(CommanderTick);
		callqueue.Remove(ReliabilityTick);
		callqueue.Remove(Heartbeat);
		callqueue.Remove(ReplanAfterBaseChange);
		callqueue.Remove(TryLogPlayerJoined);

		if (m_bSubscribed)
		{
			if (m_BaseSystem)
				m_BaseSystem.GetOnBaseFactionChanged().Remove(OnBaseFactionChanged);
			if (m_Campaign)
				m_Campaign.GetOnPlayerConnected().Remove(OnPlayerConnected);
			m_bSubscribed = false;
		}

		if (m_GroupMapMarkers)
		{
			m_GroupMapMarkers.Stop();
			m_GroupMapMarkers = null;
		}

		if (m_VehicleCoordinator)
		{
			m_VehicleCoordinator.Stop(cleanupEntities);
			m_VehicleCoordinator = null;
		}

		ReleaseFactionGroups(m_USState, cleanupEntities);
		ReleaseFactionGroups(m_USSRState, cleanupEntities);
		m_BaseSystem = null;
	}

	protected void ReleaseFactionGroups(AICF_FactionState factionState, bool cleanupEntities)
	{
		if (!factionState)
			return;

		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot)
				continue;

			SCR_AIGroup group = slot.GetGroup();
			if (group)
			{
				m_ManagedAILODPolicy.Release(group);
				group.GetOnEmpty().Remove(OnGroupEmpty);
			}

			if (!cleanupEntities)
				continue;

			m_OrderPlanner.ClearOrder(slot);
			if (group)
				RplComponent.DeleteRplEntity(group, false);
			slot.Reset();
		}
	}
}
