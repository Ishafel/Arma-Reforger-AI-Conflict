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
	protected static const int GROUP_SPAWN_AUDIT_INTERVAL_MS = 5000;
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
	protected int m_iStuckFieldHolds;
	protected int m_iDuplicateSpawnsPrevented;
	protected int m_iLifecycleAudits;
	protected int m_iStrategicBaseRevision;
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
	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aPendingOwnerChangedBases = {};
	protected ref array<FactionKey> m_aPendingOwnerChangeOldOwners = {};
	protected ref array<FactionKey> m_aPendingOwnerChangeNewOwners = {};
	protected ref array<SCR_AIGroup> m_aSpawnAuditGroups = {};
	protected ref array<int> m_aSpawnAuditLastLoggedAtMs = {};
	protected ref array<int> m_aSpawnObserverGenerations = {};

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
		AICF_Stage35Diagnostics.Configure();
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
		m_iStrategicBaseRevision = 1;

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
		stage3ConfigLine += string.Format(
			" spawn_max_attempts=%1 retry_backoff_max_ms=%2 wait_probe_interval_ms=%3 abandoned_world_pool_per_faction=%4 minimum_vehicle_request_agents=%5 cohesion_wait_timeout_ms=%6",
			m_Stage3Config.GetSpawnMaxAttempts(),
			m_Stage3Config.GetRetryBackoffMaxMs(),
			m_Stage3Config.GetWaitProbeIntervalMs(),
			m_Stage3Config.GetAbandonedWorldPoolPerFaction(),
			m_Stage3Config.GetMinimumVehicleRequestAgents(),
			m_Stage3Config.GetCohesionWaitTimeoutMs());
		AICF_Stage3Diagnostics.Info("CONFIG", stage3ConfigLine);
		bool activeForcesRolesEnabled = m_Config.GetActiveForcesRolesEnabled();
		int configuredAttackSlots = AICF_Stage1Config.LEGACY_ATTACK_SLOTS_PER_FACTION;
		int configuredDefendSlots = AICF_Stage1Config.LEGACY_DEFEND_SLOTS_PER_FACTION;
		int configuredReserveSlots = AICF_Stage1Config.LEGACY_RESERVE_SLOTS_PER_FACTION;
		if (activeForcesRolesEnabled)
		{
			configuredAttackSlots = AICF_Stage1Config.ATTACK_SLOTS_PER_FACTION;
			configuredDefendSlots = AICF_Stage1Config.DEFEND_SLOTS_PER_FACTION;
			configuredReserveSlots = AICF_Stage1Config.RESERVE_SLOTS_PER_FACTION;
		}
		string stage35ConfigLine = string.Format(
			"group_size=%1 groups_per_faction=%2 active_roles=%3 attack=%4 defend=%5 reserve=%6 legacy_attack=%7 legacy_defend=%8 legacy_reserve=%9",
			AICF_Stage1Config.MANAGED_GROUP_SIZE,
			AICF_Stage1Config.GROUP_SLOTS_PER_FACTION,
			activeForcesRolesEnabled,
			configuredAttackSlots,
			configuredDefendSlots,
			configuredReserveSlots,
			AICF_Stage1Config.LEGACY_ATTACK_SLOTS_PER_FACTION,
			AICF_Stage1Config.LEGACY_DEFEND_SLOTS_PER_FACTION,
			AICF_Stage1Config.LEGACY_RESERVE_SLOTS_PER_FACTION);
		stage35ConfigLine += string.Format(
			" minimum_dwell_ms=%1 max_managed_agents=%2 target_active_vehicles_per_faction=%3 minimum_vehicle_request_agents=%4",
			m_Config.GetRoleMinimumDwellMs(),
			m_Config.GetMaxManagedAgents(),
			m_Stage3Config.GetMaxVehiclesPerFaction(),
			m_Stage3Config.GetMinimumVehicleRequestAgents());
		AICF_Stage35Diagnostics.Info("CONFIG", stage35ConfigLine);
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
					"faction=%1 slot=%2 slot_key=%3 role=%4",
					faction.GetFactionKey(),
					slotId,
					slot.GetSlotKey(),
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
		// The facade always advances independent asset cleanup. During the delayed
		// graph-rebuild window it suppresses Trip dispatch only, so no stale target
		// is admitted while protected-clearance and delete confirmation keep moving.
		if (m_VehicleCoordinator)
		{
			m_VehicleCoordinator.Update(
				m_USState,
				m_USFaction,
				m_USSRState,
				m_USSRFaction,
				m_iStrategicBaseRevision,
				!m_bReplanScheduled && !m_bGraphRebuildNeeded);
		}
		// Sample continuous MOB presence at the one-second authority cadence. If it
		// were sampled only on CommanderTick, a group entering just after a tick could
		// remain for almost three commander intervals before the two-interval error.
		AuditActiveFactionTasking(m_USState, m_USFaction);
		AuditActiveFactionTasking(m_USSRState, m_USSRFaction);
		if (m_GroupMapMarkers)
			m_GroupMapMarkers.Sync(m_USState, m_USSRState, m_VehicleCoordinator);
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
				int actualCount;
				if (spawningGroup)
				{
					array<AIAgent> observedAgents = {};
					spawningGroup.GetAgents(observedAgents);
					actualCount = observedAgents.Count();
					foreach (AIAgent observedAgent : observedAgents)
						slot.ObserveRosterAgent(observedAgent);
				}
				MaybeLogSpawnAudit(faction, slot, false);
				if (spawningGroup && actualCount > AICF_Stage1Config.MANAGED_GROUP_SIZE)
				{
					int factionMismatchCount;
					int nonAliveCount;
					AICF_GroupRuntime.HasExactFactionRoster(
						spawningGroup,
						faction.GetFactionKey(),
						AICF_Stage1Config.MANAGED_GROUP_SIZE,
						actualCount,
						factionMismatchCount,
						nonAliveCount);
					HandleInvalidRoster(
						factionState,
						faction,
						slot,
						actualCount,
						factionMismatchCount,
						nonAliveCount);
				}
				else if (spawningGroup && slot.IsRosterSpawnRequested() &&
					actualCount == AICF_Stage1Config.MANAGED_GROUP_SIZE &&
					spawningGroup.GetNumberOfMembersToSpawn() == AICF_Stage1Config.MANAGED_GROUP_SIZE)
				{
					int factionMismatchCount;
					int nonAliveCount;
					bool rosterValid = AICF_GroupRuntime.HasExactFactionRoster(
						spawningGroup,
						faction.GetFactionKey(),
						AICF_Stage1Config.MANAGED_GROUP_SIZE,
						actualCount,
						factionMismatchCount,
						nonAliveCount);
					if (rosterValid && slot.MarkReady())
						CompleteReadyDeployment(factionState, faction, slot);
					else if (!rosterValid)
						HandleInvalidRoster(
							factionState,
							faction,
							slot,
							actualCount,
							factionMismatchCount,
							nonAliveCount);
				}
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

		DetachSpawnObservers(slot.GetGroup());

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
			"GROUP_SPAWNED",
			string.Format(
				"faction=%1 slot=%2 slot_key=%3 generation=%4 deployment=%5 initial_agents=%6 faction_correct=1",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetSlotKey(),
				slot.GetSpawnGeneration(),
				reason,
				slot.GetGroup().GetAgentsCount()));
		AICF_Stage35Diagnostics.Info(
			"GROUP_ROSTER_READY",
			string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 generation=%4 deployment=%5 initial_agents=%6 expected_agents=%7 faction_correct=1",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				reason,
				slot.GetGroup().GetAgentsCount(),
				AICF_Stage1Config.MANAGED_GROUP_SIZE));

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

		// Observe both queue callbacks without giving them authority to mark READY.
		// The exact alive/faction poll remains the acceptance gate.
		group.GetOnAgentAdded().Insert(OnSpawningAgentAdded);
		group.GetOnAllDelayedEntitySpawned().Insert(OnRosterSpawnQueueCompleted);
		m_aSpawnAuditGroups.Insert(group);
		m_aSpawnAuditLastLoggedAtMs.Insert(0);
		m_aSpawnObserverGenerations.Insert(slot.GetSpawnGeneration());

		AICF_Stage2Diagnostics.Info(
			"SPAWN_BOUND",
			string.Format(
				"faction=%1 slot=%2 generation=%3 group=%4 kind=%5",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				GroupKey(group),
				deploymentKind));

		// In 1.8 this request transfers transient navmesh/AI-budget retries to
		// SCR_AIWorld. It intentionally happens after the managed identity and
		// generation observers above are installed.
		if (!m_GroupSpawner ||
			!slot.MarkRosterSpawnRequested(AICF_Stage1Config.MANAGED_GROUP_SIZE) ||
			!m_GroupSpawner.BeginRosterSpawn(group, AICF_Stage1Config.MANAGED_GROUP_SIZE))
		{
			AICF_Stage1Diagnostics.Error(
				"ROSTER_SPAWN_REQUEST_FAILED",
				string.Format(
					"faction=%1 slot=%2 generation=%3 group=%4 expected=%5",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					slot.GetSpawnGeneration(),
					GroupKey(group),
					AICF_Stage1Config.MANAGED_GROUP_SIZE));
			DetachSpawnObservers(group);
			return false;
		}

		AICF_Stage1Diagnostics.Info(
			"ROSTER_SPAWN_REQUESTED",
			string.Format(
				"faction=%1 slot=%2 slot_key=%3 generation=%4 group=%5 expected=%6 spawning_pending=%7 pending_source=AICF_EXPECTED_MINUS_ACTUAL owner=SCR_AIWORLD_REQUEST",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetSlotKey(),
				slot.GetSpawnGeneration(),
				GroupKey(group),
				AICF_Stage1Config.MANAGED_GROUP_SIZE,
				AICF_Stage1Config.MANAGED_GROUP_SIZE));
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

	protected void OnSpawningAgentAdded(AIAgent agent)
	{
		if (m_bStopped || !agent)
			return;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		AICF_FactionState factionState;
		SCR_CampaignFaction faction;
		AICF_GroupSlot slot;
		if (!group || !FindManagedGroup(group, factionState, faction, slot) ||
			!slot || slot.GetState() != AICF_EGroupSlotState.SPAWNING ||
			!IsCurrentSpawnObserverGeneration(group, slot))
		{
			return;
		}

		slot.ObserveRosterAgent(agent, true);
	}

	protected void OnRosterSpawnQueueCompleted(SCR_AIGroup group)
	{
		if (m_bStopped || !group)
			return;

		AICF_FactionState factionState;
		SCR_CampaignFaction faction;
		AICF_GroupSlot slot;
		if (!FindManagedGroup(group, factionState, faction, slot) ||
			!slot || slot.GetState() != AICF_EGroupSlotState.SPAWNING ||
			!IsCurrentSpawnObserverGeneration(group, slot))
		{
			return;
		}

		slot.MarkRosterCompletionCallbackObserved();
	}

	protected bool IsCurrentSpawnObserverGeneration(SCR_AIGroup group, AICF_GroupSlot slot)
	{
		int observerIndex = m_aSpawnAuditGroups.Find(group);
		return observerIndex >= 0 && observerIndex < m_aSpawnObserverGenerations.Count() &&
			slot && m_aSpawnObserverGenerations[observerIndex] == slot.GetSpawnGeneration();
	}

	protected void DetachSpawnObservers(SCR_AIGroup group)
	{
		if (!group)
			return;

		group.GetOnAgentAdded().Remove(OnSpawningAgentAdded);
		group.GetOnAllDelayedEntitySpawned().Remove(OnRosterSpawnQueueCompleted);
		int auditIndex = m_aSpawnAuditGroups.Find(group);
		if (auditIndex >= 0)
		{
			m_aSpawnAuditGroups.Remove(auditIndex);
			if (auditIndex < m_aSpawnAuditLastLoggedAtMs.Count())
				m_aSpawnAuditLastLoggedAtMs.Remove(auditIndex);
			if (auditIndex < m_aSpawnObserverGenerations.Count())
				m_aSpawnObserverGenerations.Remove(auditIndex);
		}
	}

	protected void MaybeLogSpawnAudit(
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		bool force,
		string trigger = "PERIODIC")
	{
		if (!faction || !slot || slot.GetState() != AICF_EGroupSlotState.SPAWNING)
			return;

		SCR_AIGroup group = slot.GetGroup();
		int auditIndex = m_aSpawnAuditGroups.Find(group);
		int nowMs = System.GetTickCount();
		if (!force)
		{
			if (auditIndex < 0 || auditIndex >= m_aSpawnAuditLastLoggedAtMs.Count())
				return;
			int lastLoggedAtMs = m_aSpawnAuditLastLoggedAtMs[auditIndex];
			if (lastLoggedAtMs > 0 && System.GetTickCount(lastLoggedAtMs) < GROUP_SPAWN_AUDIT_INTERVAL_MS)
				return;
		}

		if (auditIndex >= 0 && auditIndex < m_aSpawnAuditLastLoggedAtMs.Count())
			m_aSpawnAuditLastLoggedAtMs[auditIndex] = nowMs;
		AICF_Stage1Diagnostics.Info(
			"GROUP_SPAWN_AUDIT",
			string.Format("trigger=%1 %2", trigger, BuildGroupSpawnSnapshot(faction, slot)));
	}

	protected void AuditAllSpawningGroups(string trigger)
	{
		AuditFactionSpawningGroups(m_USState, m_USFaction, trigger);
		AuditFactionSpawningGroups(m_USSRState, m_USSRFaction, trigger);
	}

	protected void AuditFactionSpawningGroups(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		string trigger)
	{
		if (!factionState || !faction)
			return;

		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (slot && slot.GetState() == AICF_EGroupSlotState.SPAWNING)
				MaybeLogSpawnAudit(faction, slot, true, trigger);
		}
	}

	protected string BuildGroupSpawnSnapshot(
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		return AICF_GroupRuntime.BuildSpawnSnapshot(
			faction,
			slot,
			AICF_Stage1Config.MANAGED_GROUP_SIZE);
	}

	protected string ResolveIncompleteRosterReason(
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		return AICF_GroupRuntime.ResolveSpawnIncompleteReason(
			faction,
			slot,
			AICF_Stage1Config.MANAGED_GROUP_SIZE);
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
		// Preserve the immutable failure evidence before callbacks, entity identity,
		// member references, or the slot generation context are cleared.
		int failedGeneration = slot.GetSpawnGeneration();
		string failedGroupKey = GroupKey(group);
		string incompleteReason = ResolveIncompleteRosterReason(faction, slot);
		string timeoutSnapshot = string.Format(
			"timeout_ms=%1 %2",
			GROUP_SPAWN_TIMEOUT_MS,
			BuildGroupSpawnSnapshot(faction, slot));
		AICF_Stage1Diagnostics.Error("GROUP_SPAWN_TIMEOUT", timeoutSnapshot);
		if (!replacement)
			AuditAllSpawningGroups("INITIAL_TIMEOUT_ABORT");
		DetachSpawnObservers(group);
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

		if (!replacement)
		{
			Fail(
				"INITIAL_GROUP_SPAWN_TIMEOUT",
				string.Format(
					"faction=%1 slot=%2 generation=%3 group=%4 reason=%5",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					failedGeneration,
					failedGroupKey,
					incompleteReason));
			return;
		}

		slot.BeginReinforcementWait(System.GetTickCount() + REINFORCEMENT_RETRY_MS);
	}

	protected void HandleInvalidRoster(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot,
		int actualCount,
		int factionMismatchCount,
		int nonAliveCount)
	{
		bool replacement = slot.IsReplacementDeployment();
		SCR_AIGroup group = slot.GetGroup();
		DetachSpawnObservers(group);
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

		string deploymentKind = "INITIAL";
		if (replacement)
			deploymentKind = "REPLACEMENT";
		AICF_Stage35Diagnostics.Error(
			"GROUP_ROSTER_REJECTED",
			string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 actual=%4 expected=%5 faction_mismatches=%6 non_alive=%7 deployment=%8",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetSlotId(),
				actualCount,
				AICF_Stage1Config.MANAGED_GROUP_SIZE,
				factionMismatchCount,
				nonAliveCount,
				deploymentKind));

		if (!replacement)
		{
			Fail(
				"INITIAL_GROUP_ROSTER_INVALID",
				string.Format(
					"faction=%1 slot=%2 actual=%3 expected=%4 mismatches=%5 non_alive=%6",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					actualCount,
					AICF_Stage1Config.MANAGED_GROUP_SIZE,
					factionMismatchCount,
					nonAliveCount));
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

		if (RevalidateFactionOrders(m_USSRState, m_USSRFaction, "COMMANDER_REPLAN"))
			return;

	}

	protected void AuditActiveFactionTasking(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction)
	{
		if (!m_Config.GetActiveForcesRolesEnabled() || !factionState || !faction)
			return;

		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot)
				continue;
			if (!slot.IsCombatReady())
			{
				slot.ObserveUnexplainedMobIdle(false);
				slot.ObserveMeaningfulTaskLoss(false);
				slot.ObserveMobIdleSuppression(string.Empty);
				continue;
			}
			int alive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
			if (alive <= 0)
			{
				// Empty groups are handled by the lifecycle/reinforcement path. They
				// cannot lose or recover an executable authority task.
				slot.ObserveUnexplainedMobIdle(false);
				slot.ObserveMeaningfulTaskLoss(false);
				slot.ObserveMobIdleSuppression(string.Empty);
				continue;
			}

			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			bool atMob = false;
			float distanceToMobMeters = -1.0;
			int commanderMotionAgeMs = 0;
			if (leader && mainBase && mainBase.GetOwner())
			{
				distanceToMobMeters = Math.Sqrt(vector.DistanceSqXZ(leader.GetOrigin(), mainBase.GetOwner().GetOrigin()));
				atMob = distanceToMobMeters <= STUCK_WATCHDOG_IGNORE_RADIUS_METERS;
				commanderMotionAgeMs = slot.ObserveCommanderMotion(
					leader.GetOrigin(),
					m_Stage2Config.GetStuckProgressMeters());
			}

			AICF_VehicleSlotView vehicleView;
			if (m_VehicleCoordinator)
				vehicleView = m_VehicleCoordinator.GetSlotView(slot);
			bool vehicleLifecycle = IsBoundedVehicleLifecycle(vehicleView);
			bool safeVehicleSpawnWait = IsSafeVehicleSpawnWait(vehicleView);
			bool infantryWaypointBound = IsWaypointBoundToGroup(slot.GetGroup(), slot.GetWaypoint());
			bool vehicleWaypointBound = vehicleView && vehicleView.GetVehicleWaypoint() &&
				IsWaypointBoundToGroup(slot.GetGroup(), vehicleView.GetVehicleWaypoint());
			string allowedIdleReason = ResolveAllowedIdleReason(
				slot,
				mainBase,
				vehicleLifecycle,
				safeVehicleSpawnWait);
			bool allowedException = allowedIdleReason != "NONE";
			bool meaningfulTask = HasMeaningfulTask(slot, vehicleView);
			bool meaningfulTaskLossWasReported = slot.HasReportedMeaningfulTaskLoss();
			int reportedTaskLossEpisode = slot.GetMeaningfulTaskLossEpisode();
			int reportedTaskLossAssignmentRevision =
				slot.GetReportedMeaningfulTaskLossAssignmentRevision();
			string reportedTaskLossWaypointId =
				slot.GetReportedMeaningfulTaskLossWaypointId();
			int currentAssignmentRevision = slot.GetStrategicAssignmentRevision();
			int previousTasklessAgeMs = slot.GetMeaningfulTaskLostAgeMs();
			int tasklessAgeMs = slot.ObserveMeaningfulTaskLoss(!meaningfulTask, UPDATE_INTERVAL_MS);
			int taskLossGraceMs = Math.Max(
				UPDATE_INTERVAL_MS,
				m_Stage2Config.GetReliabilityIntervalMs());
			string vehicleState = "NONE";
			if (vehicleView)
				vehicleState = vehicleView.GetPhase();
			AIWaypoint meaningfulTaskWaypoint = slot.GetWaypoint();
			if (!meaningfulTaskWaypoint && vehicleView)
				meaningfulTaskWaypoint = vehicleView.GetVehicleWaypoint();
			string meaningfulTaskWaypointId = WaypointKey(meaningfulTaskWaypoint);

			if (!meaningfulTask && tasklessAgeMs >= taskLossGraceMs &&
				slot.MarkMeaningfulTaskLossReported(
					currentAssignmentRevision,
					meaningfulTaskWaypointId))
			{
				string taskLostLine = string.Format(
					"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 alive=%5 target=%6 role=%7 posture=%8 vehicle_state=%9",
					faction.GetFactionKey(),
					slot.GetSlotKey(),
					slot.GetSlotId(),
					slot.GetSpawnGeneration(),
					alive,
					AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					slot.GetOperationalPosture(),
					vehicleState);
				taskLostLine += string.Format(
					" infantry_waypoint=%1 vehicle_waypoint=%2 loss_trigger=AUTHORITY_TASK_AUDIT loss_reason=NO_EXECUTABLE_WAYPOINT taskless_age_ms=%3 at_mob=%4",
					infantryWaypointBound,
					vehicleWaypointBound,
					tasklessAgeMs,
					atMob);
				taskLostLine += string.Format(
					" loss_grace_ms=%1 assignment_revision=%2 task_waypoint=%3 loss_episode=%4",
					taskLossGraceMs,
					currentAssignmentRevision,
					meaningfulTaskWaypointId,
					slot.GetMeaningfulTaskLossEpisode());
				AICF_Stage35Diagnostics.Warning("MEANINGFUL_TASK_LOST", taskLostLine);
			}
			else if (meaningfulTask && previousTasklessAgeMs > 0 &&
				meaningfulTaskLossWasReported &&
				reportedTaskLossAssignmentRevision == currentAssignmentRevision)
			{
				AICF_Stage35Diagnostics.Info(
					"MEANINGFUL_TASK_RECOVERED",
					string.Format(
						"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 target=%5 vehicle_state=%6 taskless_age_ms=%7 infantry_waypoint=%8 vehicle_waypoint=%9",
						faction.GetFactionKey(),
						slot.GetSlotKey(),
						slot.GetSlotId(),
						slot.GetSpawnGeneration(),
						AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
						vehicleState,
						previousTasklessAgeMs,
						infantryWaypointBound,
						vehicleWaypointBound) + string.Format(
						" alive=%1 role=%2 posture=%3 at_mob=%4",
						alive,
						AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
						slot.GetOperationalPosture(),
						atMob) + string.Format(
						" assignment_revision=%1 task_waypoint=%2 loss_episode=%3 lost_task_waypoint=%4",
						currentAssignmentRevision,
						meaningfulTaskWaypointId,
						reportedTaskLossEpisode,
						reportedTaskLossWaypointId));
			}

			bool vehicleOwnsTaskRepair = vehicleView &&
				(vehicleView.IsControllingMovement() || vehicleView.IsRestorePending());
			bool repairSucceeded = false;
			if (!meaningfulTask && !vehicleOwnsTaskRepair)
				repairSucceeded = TryRecoverOrder(slot, faction, "MEANINGFUL_TASK_LOST");

			int taskDeadlineMs = 2 * m_Config.GetCommanderIntervalMs();
			if (!meaningfulTask && !repairSucceeded && tasklessAgeMs >= taskDeadlineMs && slot.MarkMeaningfulTaskDeadlineReported())
			{
				AICF_Stage35Diagnostics.Error(
					"MEANINGFUL_TASK_DEADLINE_MISSED",
					string.Format(
						"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 taskless_age_ms=%5 deadline_ms=%6 target=%7 vehicle_state=%8 at_mob=%9",
						faction.GetFactionKey(),
						slot.GetSlotKey(),
						slot.GetSlotId(),
						slot.GetSpawnGeneration(),
						tasklessAgeMs,
						taskDeadlineMs,
						AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
						vehicleState,
						atMob));
			}

			string suppressionRule = allowedIdleReason;
			if (!atMob)
				suppressionRule = "OUTSIDE_MOB";
			else if (!allowedException)
				suppressionRule = string.Empty;
			if (slot.ObserveMobIdleSuppression(suppressionRule))
			{
				AICF_Stage35Diagnostics.Info(
					"IDLE_DEADLINE_SUPPRESSED",
					string.Format(
						"faction=%1 slot=%2 numeric_slot=%3 scope=MOB suppression_rule=%4 at_mob=%5 distance_to_mob_m=%6 meaningful_task=%7 allowed_idle_reason=%8 vehicle_state=%9",
						faction.GetFactionKey(),
						slot.GetSlotKey(),
						slot.GetSlotId(),
						suppressionRule,
						atMob,
						distanceToMobMeters,
						meaningfulTask,
						allowedIdleReason,
						vehicleState) + string.Format(
						" taskless_age_ms=%1 suppression_active=%2",
						tasklessAgeMs,
						!suppressionRule.IsEmpty()));
			}
			// The Stage 3.5 contract limits continuous presence inside the MOB radius,
			// not merely a lack of waypoint or measurable motion. Circling/pathfinding
			// inside the radius therefore cannot reset this deadline.
			bool mobPresenceRequiresEgress = atMob && !allowedException;
			int mobPresenceMs = slot.ObserveUnexplainedMobIdle(
				mobPresenceRequiresEgress,
				UPDATE_INTERVAL_MS);
			if (mobPresenceMs < 2 * m_Config.GetCommanderIntervalMs() ||
				!slot.MarkUnexplainedMobIdleDeadlineReported())
			{
				continue;
			}

			string mobEgressLine = string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 role=%4 mob_presence_ms=%5 motion_age_ms=%6 egress_deadline_ms=%7 target=%8 waypoint=%9",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
				mobPresenceMs,
				commanderMotionAgeMs,
				2 * m_Config.GetCommanderIntervalMs(),
				AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
				slot.GetWaypoint() != null);
			mobEgressLine += string.Format(
				" vehicle_state=%1 at_mob=%2 meaningful_task=%3 allowed_idle_reason=%4",
				vehicleState,
				atMob,
				meaningfulTask,
				allowedIdleReason);
			AICF_Stage35Diagnostics.Error("MOB_EGRESS_DEADLINE_MISSED", mobEgressLine);
		}
	}

	protected bool HasMeaningfulTask(
		AICF_GroupSlot slot,
		AICF_VehicleSlotView vehicleView)
	{
		if (!slot || !slot.GetTargetBase())
			return false;
		if (IsWaypointBoundToGroup(slot.GetGroup(), slot.GetWaypoint()))
			return true;
		if (vehicleView && vehicleView.HasExecutableVehicleTask())
		{
			AIWaypoint vehicleWaypoint = vehicleView.GetVehicleWaypoint();
			if (!vehicleWaypoint)
				return vehicleView.GetPhase() == "BOARDING";
			return IsWaypointBoundToGroup(slot.GetGroup(), vehicleWaypoint);
		}
		return slot.IsPersistentStuckFieldHold();
	}

	protected bool IsWaypointBoundToGroup(SCR_AIGroup group, AIWaypoint waypoint)
	{
		if (!group || !waypoint)
			return false;

		array<AIWaypoint> waypointQueue = {};
		group.GetWaypoints(waypointQueue);
		return waypointQueue.Contains(waypoint);
	}

	protected string ResolveAllowedIdleReason(
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent mainBase,
		bool vehicleLifecycle,
		bool safeVehicleSpawnWait)
	{
		if (slot && slot.GetRole() == AICF_EGroupRole.DEFEND && slot.GetTargetBase() == mainBase)
			return "HQ_DEFENSE";
		if (safeVehicleSpawnWait)
			return "SAFE_VEHICLE_SPAWN_WAIT";
		if (vehicleLifecycle)
			return "BOUNDED_VEHICLE_LIFECYCLE";
		return "NONE";
	}

	protected bool IsBoundedVehicleLifecycle(AICF_VehicleSlotView vehicleView)
	{
		return vehicleView && vehicleView.IsControllingMovement() &&
			vehicleView.HasExecutableVehicleTask();
	}

	protected bool IsSafeVehicleSpawnWait(AICF_VehicleSlotView vehicleView)
	{
		return vehicleView && vehicleView.IsSafeSpawnWait();
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
			{
				m_VehicleCoordinator.ReplanControlledMovement(
					slot,
					faction,
					reason,
					m_Config.GetRoleMinimumDwellMs(),
					m_Config.GetCommanderIntervalMs(),
					m_iStrategicBaseRevision);
				continue;
			}
			// CommanderTick may observe the candidate between reliability polls, but
			// only ReliabilityTick is allowed to confirm or reject its stability.
			if (slot.HasPendingOrderRecovery())
				continue;
			if (slot.IsPersistentStuckFieldHold())
			{
				if (m_OrderPlanner.IsStrategicTargetValid(slot, faction, slot.GetTargetBase()))
				{
					if (!slot.IsPersistentStuckFieldHoldRetryDue(m_Stage2Config.GetStuckTimeoutMs()))
						continue;
					slot.ResumeFromPersistentStuckFieldHold();
					bool fieldResume = m_OrderPlanner.RebuildCurrentOrder(
						slot,
						faction,
						"PERSISTENT_STUCK_FIELD_RETRY");
					if (!fieldResume)
						slot.BeginPersistentStuckFieldHold();
					AICF_Stage2Diagnostics.Info(
						"GROUP_STUCK_FIELD_RESUMED",
						string.Format(
							"faction=%1 slot=%2 group=%3 target=%4 success=%5 trigger=HOLD_TIMEOUT hold_ms=%6 entity_preserved=1",
							faction.GetFactionKey(),
							slot.GetSlotId(),
							GroupKey(slot.GetGroup()),
							AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
							fieldResume,
							m_Stage2Config.GetStuckTimeoutMs()));
					continue;
				}
				slot.ResumeFromPersistentStuckFieldHold();
			}

			// The stock CaptureRelay smart-action waypoint reaches Signal Hill in the
			// dedicated Conflict runtime, but can finish without invoking its user
			// action. Preserve the stock radio eligibility rules and complete that
			// authority operation only after a living managed agent has really arrived.
			if (TryCaptureArrivedRelay(slot, faction))
				return true;

			string failureReason = m_OrderPlanner.GetOrderFailureReason(slot, faction);
			if (failureReason.IsEmpty())
			{
				m_OrderPlanner.ReconcileStrategicOrder(
					slot,
					faction,
					m_ObjectiveGraph,
					m_TargetSelector,
					reason,
					m_Config.GetRoleMinimumDwellMs(),
					m_Config.GetCommanderIntervalMs());
				continue;
			}

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
		if (m_VehicleCoordinator &&
			(m_VehicleCoordinator.IsControllingMovement(slot) ||
			m_VehicleCoordinator.IsRestorePending(slot)))
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
			if (m_VehicleCoordinator &&
				(m_VehicleCoordinator.IsControllingMovement(slot) ||
				m_VehicleCoordinator.IsRestorePending(slot)))
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
			if (slot.IsPersistentStuckFieldHold())
			{
				if (m_OrderPlanner.IsStrategicTargetValid(slot, faction, slot.GetTargetBase()))
					continue;
				slot.ResumeFromPersistentStuckFieldHold();
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

		if (alreadyCountsAsStuck && slot.HasPendingStuckRecoveryEvidence())
		{
			int evidenceAttempt = slot.GetPendingStuckRecoveryAttempt();
			int evidenceAgeMs = slot.GetPendingStuckRecoveryEvidenceAgeMs();
			string evidenceWaypointId = WaypointKey(
				slot.GetPendingStuckRecoveryWaypoint());
			slot.ClearPendingStuckRecoveryEvidence();
			AICF_Stage2Diagnostics.Info(
				"GROUP_STUCK_RECOVERY",
				string.Format(
					"faction=%1 slot=%2 action=CONFIRM_EXECUTION success=0 attempt=%3 order_issue_succeeded=1 movement_resumed=0 route_progress_resumed=0 evidence_state=ORDER_BINDING_UNSTABLE evidence_age_ms=%4 evidence_waypoint=%5",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					evidenceAttempt,
					evidenceAgeMs,
					evidenceWaypointId));
		}

		slot.ClearPendingOrderRecovery();
		if (!alreadyCountsAsStuck)
		{
			// An unstable recovery candidate is a binding failure, not another
			// physical stuck detection. It still consumes the bounded per-slot
			// recovery budget, while GROUP_STUCK_DETECTED owns the global counter.
			slot.RecordStuckRecovery(distanceMeters);
		}

		if (slot.GetStuckRecoveryCount() >= m_Stage2Config.GetMaxStuckRecoveries())
		{
			HoldPersistentStuckGroup(factionState, faction, slot, target, distanceMeters);
			return;
		}

		TryRecoverOrder(slot, faction, failureReason);
	}

	protected bool TryRecoverOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string failureReason)
	{
		int requestedAtMs = System.GetTickCount();
		if (!slot || !faction || slot.HasPendingOrderRecovery())
			return false;
		if (!slot.CanAttemptOrderRecovery(m_Stage2Config.GetOrderRecoveryRetryMs()))
			return false;

		string oldWaypointId = "NONE";
		if (slot.GetWaypoint())
			oldWaypointId = slot.GetWaypoint().GetID().ToString();
		string vehicleState = "NONE";
		AICF_VehicleSlotView vehicleView;
		if (m_VehicleCoordinator)
			vehicleView = m_VehicleCoordinator.GetSlotView(slot);
		if (vehicleView)
			vehicleState = vehicleView.GetPhase();
		AICF_Stage35Diagnostics.Info(
			"ORDER_RESTORE_REQUESTED",
			string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 trigger=RELIABILITY_AUDIT reason=%5 target=%6 old_waypoint=%7 vehicle_state=%8 taskless_age_ms=%9",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				failureReason,
				AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
				oldWaypointId,
				vehicleState,
				slot.GetMeaningfulTaskLostAgeMs()));

		slot.MarkOrderRecoveryAttempt();
		m_iOrderRecoveryAttempts++;
		bool recovered = m_OrderPlanner.RecoverOrder(
			slot,
			faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			failureReason);
		AIWaypoint newWaypoint = slot.GetWaypoint();
		string newWaypointId = "NONE";
		if (newWaypoint)
			newWaypointId = newWaypoint.GetID().ToString();
		array<AIWaypoint> waypointQueue = {};
		int queueCount;
		bool inQueue;
		bool isCurrent;
		if (slot.GetGroup())
		{
			queueCount = slot.GetGroup().GetWaypoints(waypointQueue);
			inQueue = newWaypoint && waypointQueue.Contains(newWaypoint);
			isCurrent = newWaypoint && slot.GetGroup().GetCurrentWaypoint() == newWaypoint;
		}
		bool postconditionMeaningful = recovered && newWaypoint && inQueue && isCurrent;
		string failure = "NONE";
		if (!recovered)
			failure = "PLANNER_REJECTED";
		else if (!postconditionMeaningful)
			failure = "WAYPOINT_BIND_MISMATCH";
		AICF_Stage35Diagnostics.Info(
			"ORDER_RESTORE_RESULT",
			string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 success=%5 old_waypoint=%6 new_waypoint=%7 bound_to_group=%8 is_current=%9",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				recovered && postconditionMeaningful,
				oldWaypointId,
				newWaypointId,
				inQueue,
				isCurrent) + string.Format(
				" queue_count=%1 postcondition_meaningful_task=%2 failure_reason=%3 latency_ms=%4 trigger=RELIABILITY_AUDIT reason=%5",
				queueCount,
				postconditionMeaningful,
				failure,
				System.GetTickCount(requestedAtMs),
				failureReason));
		if (recovered && !postconditionMeaningful)
			AICF_Stage35Diagnostics.Warning("WAYPOINT_BIND_MISMATCH", string.Format("faction=%1 slot=%2 waypoint=%3 queue_count=%4", faction.GetFactionKey(), slot.GetSlotKey(), newWaypointId, queueCount));
		return recovered && postconditionMeaningful;
	}

	protected void MonitorGroupProgress(
		AICF_FactionState factionState,
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction)
	{
		if (!m_Stage2Config.GetStuckWatchdogEnabled())
			return;
		if (slot && slot.IsPersistentStuckFieldHold())
			return;

		SCR_AIGroup group = slot.GetGroup();
		AIWaypoint waypoint = slot.GetWaypoint();
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (!group || !waypoint || !target)
			return;

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!leader)
			return;

		vector leaderPosition = leader.GetOrigin();
		float distanceMeters = Math.Sqrt(vector.DistanceSqXZ(
			leaderPosition,
			waypoint.GetOrigin()));
		float evidenceThresholdMeters = Math.Max(
			1.0,
			m_Stage2Config.GetStuckProgressMeters());
		if (slot.HasPendingStuckRecoveryEvidence())
		{
			int evidenceAttempt = slot.GetPendingStuckRecoveryAttempt();
			int evidenceAgeMs = slot.GetPendingStuckRecoveryEvidenceAgeMs();
			string evidenceWaypointId = WaypointKey(
				slot.GetPendingStuckRecoveryWaypoint());
			if (!slot.IsPendingStuckRecoveryEvidenceContextCurrent())
			{
				slot.ClearPendingStuckRecoveryEvidence();
				AICF_Stage2Diagnostics.Info(
					"GROUP_STUCK_RECOVERY",
					string.Format(
						"faction=%1 slot=%2 action=CONFIRM_EXECUTION success=0 attempt=%3 order_issue_succeeded=1 movement_resumed=0 route_progress_resumed=0 evidence_state=CONTEXT_INVALIDATED evidence_age_ms=%4 evidence_waypoint=%5",
						faction.GetFactionKey(),
						slot.GetSlotId(),
						evidenceAttempt,
						evidenceAgeMs,
						evidenceWaypointId));
			}
			else
			{
				bool movementResumed;
				bool routeProgressResumed;
				float displacementMeters;
				float routeReductionMeters;
				bool executionConfirmed = slot.EvaluatePendingStuckRecoveryEvidence(
					leaderPosition,
					distanceMeters,
					evidenceThresholdMeters,
					movementResumed,
					routeProgressResumed,
					displacementMeters,
					routeReductionMeters);
				if (executionConfirmed)
				{
					slot.ConfirmPendingStuckRecoveryEvidence(distanceMeters);
					m_iStuckRecoveries++;
					string confirmedRecoveryDetails = string.Format(
						"faction=%1 slot=%2 action=CONFIRM_EXECUTION success=1 attempt=%3 order_issue_succeeded=1 movement_resumed=%4 route_progress_resumed=%5 displacement_m=%6 route_reduction_m=%7 evidence_age_ms=%8 threshold_m=%9",
						faction.GetFactionKey(),
						slot.GetSlotId(),
						evidenceAttempt,
						movementResumed,
						routeProgressResumed,
						displacementMeters,
						routeReductionMeters,
						evidenceAgeMs,
						evidenceThresholdMeters);
					confirmedRecoveryDetails += string.Format(
						" evidence_waypoint=%1",
						evidenceWaypointId);
					AICF_Stage2Diagnostics.Info(
						"GROUP_STUCK_RECOVERY",
						confirmedRecoveryDetails);
				}
			}
		}
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

		if (slot.HasPendingStuckRecoveryEvidence())
		{
			bool movementResumed;
			bool routeProgressResumed;
			float displacementMeters;
			float routeReductionMeters;
			slot.EvaluatePendingStuckRecoveryEvidence(
				leaderPosition,
				distanceMeters,
				evidenceThresholdMeters,
				movementResumed,
				routeProgressResumed,
				displacementMeters,
				routeReductionMeters);
			int failedAttempt = slot.GetPendingStuckRecoveryAttempt();
			int failedEvidenceAgeMs = slot.GetPendingStuckRecoveryEvidenceAgeMs();
			string failedEvidenceWaypointId = WaypointKey(
				slot.GetPendingStuckRecoveryWaypoint());
			slot.ClearPendingStuckRecoveryEvidence();
			AICF_Stage2Diagnostics.Info(
				"GROUP_STUCK_RECOVERY",
				string.Format(
					"faction=%1 slot=%2 action=CONFIRM_EXECUTION success=0 attempt=%3 order_issue_succeeded=1 movement_resumed=0 route_progress_resumed=0 displacement_m=%4 route_reduction_m=%5 evidence_state=TIMEOUT evidence_age_ms=%6 threshold_m=%7 evidence_waypoint=%8",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					failedAttempt,
					displacementMeters,
					routeReductionMeters,
					failedEvidenceAgeMs,
					evidenceThresholdMeters,
					failedEvidenceWaypointId));
		}

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
		// produce leader progress, preserve the field group under a durable local
		// hold instead of deleting it and teleporting its replacement to a MOB.
		if (slot.GetStuckRecoveryCount() >= m_Stage2Config.GetMaxStuckRecoveries())
		{
			HoldPersistentStuckGroup(factionState, faction, slot, target, distanceMeters);
			return;
		}

		bool orderIssueSucceeded = m_OrderPlanner.RebuildCurrentOrder(
			slot,
			faction,
			"STUCK_ROUTE_REBUILD");
		if (!orderIssueSucceeded)
		{
			orderIssueSucceeded = m_OrderPlanner.RecoverOrder(
				slot,
				faction,
				m_ObjectiveGraph,
				m_TargetSelector,
				"STUCK_TARGET_INVALID",
				true);
		}

		AIWaypoint recoveryWaypoint = slot.GetWaypoint();
		float recoveryRouteDistanceMeters = -1.0;
		if (orderIssueSucceeded && recoveryWaypoint)
		{
			recoveryRouteDistanceMeters = Math.Sqrt(vector.DistanceSqXZ(
				leaderPosition,
				recoveryWaypoint.GetOrigin()));
			slot.ArmPendingStuckRecoveryEvidence(
				group,
				target,
				recoveryWaypoint,
				leaderPosition,
				recoveryRouteDistanceMeters,
				recoveryAttempt);
		}
		float recoveryBaselineDistance = distanceMeters;
		if (recoveryRouteDistanceMeters >= 0)
			recoveryBaselineDistance = recoveryRouteDistanceMeters;
		slot.RecordStuckRecovery(recoveryBaselineDistance);

		string recoveryEvidenceState = "NOT_ARMED";
		if (slot.HasPendingStuckRecoveryEvidence())
			recoveryEvidenceState = "PENDING";
		AICF_Stage2Diagnostics.Info(
			"GROUP_STUCK_RECOVERY",
			string.Format(
				"faction=%1 slot=%2 action=REBUILD_ORDER success=0 attempt=%3 order_issue_succeeded=%4 movement_resumed=0 route_progress_resumed=0 evidence_state=%5 threshold_m=%6 recovery_waypoint=%7",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				recoveryAttempt,
				orderIssueSucceeded,
				recoveryEvidenceState,
				evidenceThresholdMeters,
				WaypointKey(recoveryWaypoint)));

	}

	protected void HoldPersistentStuckGroup(
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
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!leader)
			return;
		vector fieldPosition = leader.GetOrigin();
		AICF_Stage2Diagnostics.Warning(
			"GROUP_STUCK_PERSISTENT",
			string.Format(
				"faction=%1 slot=%2 group=%3 target=%4 distance_m=%5 recoveries=%6 action=FIELD_HOLD entity_preserved=1 ticket_policy=NONE",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				groupKey,
				AICF_Stage1Diagnostics.BaseKey(target),
				distanceMeters,
				slot.GetStuckRecoveryCount()));

		m_GroupCohesionPolicy.NormalizeAfterMovementFailure(group);
		if (!m_OrderPlanner.HoldPositionForPersistentStuck(
			slot,
			faction,
			target,
			fieldPosition))
		{
			AICF_Stage2Diagnostics.Error(
				"STUCK_FIELD_HOLD_REJECTED",
				string.Format("faction=%1 slot=%2 group=%3 target=%4", faction.GetFactionKey(), slot.GetSlotId(), groupKey, AICF_Stage1Diagnostics.BaseKey(target)));
			return;
		}

		m_iStuckFieldHolds++;
		AICF_Stage2Diagnostics.Info(
			"GROUP_STUCK_FIELD_HOLD",
			string.Format(
				"faction=%1 slot=%2 group=%3 target=%4 position=[%5,%6,%7] entity_preserved=1 group_generation=%8 resume=HOLD_TIMEOUT_OR_STRATEGIC_CONTEXT_CHANGE",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				groupKey,
				AICF_Stage1Diagnostics.BaseKey(target),
				fieldPosition[0],
				fieldPosition[1],
				fieldPosition[2],
				slot.GetSpawnGeneration()));
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
		RecordPendingOwnerChange(base, oldOwner, newOwner);
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
		m_iStrategicBaseRevision++;
		if (m_VehicleCoordinator)
			m_VehicleCoordinator.NotifyStrategicContextChanged(
				m_iStrategicBaseRevision,
				"BASE_GRAPH_REBUILT");

		ReplanFactionAfterBaseChange(m_USState, m_USFaction);
		ReplanFactionAfterBaseChange(m_USSRState, m_USSRFaction);
		m_bGraphRebuildNeeded = false;
		m_LastChangedBase = null;
		m_aPendingOwnerChangedBases.Clear();
		m_aPendingOwnerChangeOldOwners.Clear();
		m_aPendingOwnerChangeNewOwners.Clear();
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
			bool vehicleControlled = m_VehicleCoordinator && m_VehicleCoordinator.IsControllingMovement(slot);
			array<SCR_CampaignMilitaryBaseComponent> relevantLostBases = {};
			if (slot.GetRole() == AICF_EGroupRole.DEFEND)
			{
				for (int changeIndex = 0; changeIndex < m_aPendingOwnerChangedBases.Count(); changeIndex++)
				{
					SCR_CampaignMilitaryBaseComponent lostBase = m_aPendingOwnerChangedBases[changeIndex];
					if (m_aPendingOwnerChangeOldOwners[changeIndex] == faction.GetFactionKey() &&
						m_aPendingOwnerChangeNewOwners[changeIndex] != faction.GetFactionKey() &&
						IsDefendLossResponseRelevant(slot, lostBase))
					{
						relevantLostBases.Insert(lostBase);
					}
				}
			}

			if (!relevantLostBases.IsEmpty())
			{
				foreach (SCR_CampaignMilitaryBaseComponent lostBase : relevantLostBases)
				{
					bool responseChanged = m_OrderPlanner.AssignLossResponseOrder(
						slot,
						faction,
						m_ObjectiveGraph,
						m_TargetSelector,
						lostBase,
						m_Config.GetRoleMinimumDwellMs(),
						m_Config.GetCommanderIntervalMs());
					reassigned = responseChanged || reassigned;
				}
				if (reassigned && vehicleControlled && slot.GetTargetBase() != oldTarget)
				{
					m_VehicleCoordinator.AdoptCurrentStrategicAssignment(
						slot,
						faction,
						"BASE_OWNER_CHANGED_QRF",
						m_iStrategicBaseRevision);
				}
			}
			else if (vehicleControlled)
			{
					reassigned = m_VehicleCoordinator.ReplanControlledMovement(
						slot,
						faction,
						"BASE_OWNER_CHANGED",
						m_Config.GetRoleMinimumDwellMs(),
						m_Config.GetCommanderIntervalMs(),
						m_iStrategicBaseRevision);
			}
			else if (slot.IsPersistentStuckFieldHold())
			{
				SCR_CampaignMilitaryBaseComponent heldTarget = slot.GetTargetBase();
				if (m_OrderPlanner.IsStrategicTargetValid(slot, faction, heldTarget))
				{
					slot.ResumeFromPersistentStuckFieldHold();
					reassigned = m_OrderPlanner.RebuildCurrentOrder(
						slot,
						faction,
						"PERSISTENT_STUCK_CONTEXT_CHANGED");
				}
				else
				{
					slot.ResumeFromPersistentStuckFieldHold();
					reassigned = m_OrderPlanner.AssignOrder(
						slot,
						faction,
						m_ObjectiveGraph,
						m_TargetSelector,
						"PERSISTENT_STUCK_TARGET_CHANGED",
						m_LastChangedBase);
				}
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
			else
			{
				reassigned = m_OrderPlanner.ReconcileStrategicOrder(
					slot,
					faction,
					m_ObjectiveGraph,
					m_TargetSelector,
					"BASE_OWNER_CHANGED",
					m_Config.GetRoleMinimumDwellMs(),
					m_Config.GetCommanderIntervalMs());
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

	protected void RecordPendingOwnerChange(
		SCR_CampaignMilitaryBaseComponent base,
		FactionKey oldOwner,
		FactionKey newOwner)
	{
		if (!base)
			return;

		int pendingIndex = m_aPendingOwnerChangedBases.Find(base);
		if (pendingIndex >= 0)
		{
			// Preserve the first owner and update only the net new owner. A rapid
			// capture-and-recapture back to the original owner therefore cancels loss.
			m_aPendingOwnerChangeNewOwners[pendingIndex] = newOwner;
			return;
		}
		m_aPendingOwnerChangedBases.Insert(base);
		m_aPendingOwnerChangeOldOwners.Insert(oldOwner);
		m_aPendingOwnerChangeNewOwners.Insert(newOwner);
	}

	protected bool IsDefendLossResponseRelevant(
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent lostBase)
	{
		if (!slot || !lostBase)
			return false;

		SCR_CampaignMilitaryBaseComponent defendedBase = slot.GetTargetBase();
		if (!defendedBase || defendedBase == lostBase)
			return true;

		int defendedNodeId = m_ObjectiveGraph.FindNodeId(defendedBase);
		int lostNodeId = m_ObjectiveGraph.FindNodeId(lostBase);
		if (defendedNodeId < 0 || lostNodeId < 0)
			return false;

		AICF_ObjectiveNode defendedNode = m_ObjectiveGraph.GetNode(defendedNodeId);
		AICF_ObjectiveNode lostNode = m_ObjectiveGraph.GetNode(lostNodeId);
		return (defendedNode && defendedNode.GetOutgoingNodeIds().Contains(lostNodeId)) ||
			(lostNode && lostNode.GetOutgoingNodeIds().Contains(defendedNodeId));
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
				"audits=%1 order_attempts=%2 order_recovered=%3 stuck_detected=%4 stuck_recovered=%5 stuck_field_holds=%6 duplicate_spawns_prevented=%7 concurrent_spawns=%8 managed_agents=%9",
				m_iLifecycleAudits,
				m_iOrderRecoveryAttempts,
				m_iOrderRecoveries,
				m_iStuckDetections,
				m_iStuckRecoveries,
				m_iStuckFieldHolds,
				m_iDuplicateSpawnsPrevented,
				CountConcurrentReplacementSpawns(),
				CountManagedAgents()));

		int usReservedVehicles;
		int ussrReservedVehicles;
		int usSpawnedVehicles;
		int ussrSpawnedVehicles;
		int usWorldPool;
		int ussrWorldPool;
		int usCapHeld;
		int ussrCapHeld;
		int usReleasePending;
		int ussrReleasePending;
		int usFailedClosed;
		int ussrFailedClosed;
		int usRetainedPhysical;
		int ussrRetainedPhysical;
		if (m_VehicleCoordinator)
		{
			usReservedVehicles = m_VehicleCoordinator.GetReservedCount("US");
			ussrReservedVehicles = m_VehicleCoordinator.GetReservedCount("USSR");
			usSpawnedVehicles = m_VehicleCoordinator.GetSpawnedCount("US");
			ussrSpawnedVehicles = m_VehicleCoordinator.GetSpawnedCount("USSR");
			usWorldPool = m_VehicleCoordinator.GetWorldPoolCount("US");
			ussrWorldPool = m_VehicleCoordinator.GetWorldPoolCount("USSR");
			usCapHeld = m_VehicleCoordinator.GetCapHeldCount("US");
			ussrCapHeld = m_VehicleCoordinator.GetCapHeldCount("USSR");
			usReleasePending = m_VehicleCoordinator.GetReleasePendingCount("US");
			ussrReleasePending = m_VehicleCoordinator.GetReleasePendingCount("USSR");
			usFailedClosed = m_VehicleCoordinator.GetFailedClosedCount("US");
			ussrFailedClosed = m_VehicleCoordinator.GetFailedClosedCount("USSR");
			usRetainedPhysical = m_VehicleCoordinator.GetRetainedPhysicalCount("US");
			ussrRetainedPhysical = m_VehicleCoordinator.GetRetainedPhysicalCount("USSR");
		}
		int managedGroups = CountManagedGroups();
		int managedAgents = CountManagedAgents();
		int managedWaypoints = CountManagedWaypoints();
		int trackedEntities = managedGroups + managedAgents + managedWaypoints +
			usSpawnedVehicles + ussrSpawnedVehicles + usWorldPool + ussrWorldPool +
			usRetainedPhysical + ussrRetainedPhysical;
		string forceHeartbeatLine = string.Format(
			"managed_groups=%1 managed_agents=%2 managed_waypoints=%3 tracked_entities=%4 us_active=%5 us_reserved=%6 us_world_pool=%7 ussr_active=%8 ussr_reserved=%9",
			managedGroups,
			managedAgents,
			managedWaypoints,
			trackedEntities,
			usSpawnedVehicles,
			usReservedVehicles,
			usWorldPool,
			ussrSpawnedVehicles,
			ussrReservedVehicles);
		forceHeartbeatLine += string.Format(
			" ussr_world_pool=%1 vehicle_cap=%2 us_cap_held=%3 us_release_pending=%4 us_failed_closed=%5 us_retained_physical=%6",
			ussrWorldPool,
			m_Stage3Config.GetMaxVehiclesPerFaction(),
			usCapHeld,
			usReleasePending,
			usFailedClosed,
			usRetainedPhysical);
		forceHeartbeatLine += string.Format(
			" ussr_cap_held=%1 ussr_release_pending=%2 ussr_failed_closed=%3 ussr_retained_physical=%4",
			ussrCapHeld,
			ussrReleasePending,
			ussrFailedClosed,
			ussrRetainedPhysical);
		if (!m_VehicleCoordinator)
			AICF_Stage35Diagnostics.Info("FORCE_HEARTBEAT", forceHeartbeatLine);
		EmitStage35FactionActivity(m_USState, m_USFaction);
		EmitStage35FactionActivity(m_USSRState, m_USSRFaction);

		if (m_VehicleCoordinator)
		{
			m_VehicleCoordinator.Heartbeat(
				managedGroups,
				managedAgents,
				managedWaypoints,
				trackedEntities);
		}
	}

	protected void EmitStage35FactionActivity(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction)
	{
		if (!factionState || !faction)
			return;

		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot)
				continue;

			SCR_AIGroup group = slot.GetGroup();
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
			int alive = AICF_GroupRuntime.CountAliveAgents(group);
			float distanceToMobMeters = -1.0;
			bool atMob;
			if (leader && mainBase && mainBase.GetOwner())
			{
				distanceToMobMeters = Math.Sqrt(vector.DistanceSqXZ(
					leader.GetOrigin(),
					mainBase.GetOwner().GetOrigin()));
				atMob = distanceToMobMeters <= STUCK_WATCHDOG_IGNORE_RADIUS_METERS;
			}

			AICF_VehicleSlotView vehicleView;
			if (m_VehicleCoordinator)
				vehicleView = m_VehicleCoordinator.GetSlotView(slot);
			string vehicleState = "NONE";
			bool vehicleLifecycle;
			bool vehicleWaypoint;
			bool infantryWaypoint = IsWaypointBoundToGroup(slot.GetGroup(), slot.GetWaypoint());
			if (vehicleView)
			{
				vehicleState = vehicleView.GetPhase();
				vehicleLifecycle = IsBoundedVehicleLifecycle(vehicleView);
				vehicleWaypoint = vehicleView.GetVehicleWaypoint() &&
					IsWaypointBoundToGroup(slot.GetGroup(), vehicleView.GetVehicleWaypoint());
			}

			string allowedIdleReason = "NONE";
			if (slot.GetState() == AICF_EGroupSlotState.SPAWNING ||
				slot.GetState() == AICF_EGroupSlotState.WAITING)
			{
				allowedIdleReason = "SAFE_SPAWN_OR_ROSTER_RECOVERY";
			}
			else
			{
				bool safeSpawnWait = IsSafeVehicleSpawnWait(vehicleView);
				allowedIdleReason = ResolveAllowedIdleReason(
					slot,
					mainBase,
					vehicleLifecycle,
					safeSpawnWait);
			}

			bool hasMeaningfulTask = HasMeaningfulTask(slot, vehicleView);
			int mobPresenceMs = slot.GetUnexplainedMobIdleAgeMs();
			bool restorePending = false;
			string vehicleFailureReason = "NONE";
			string vehicleTerminalReason = "NONE";
			string vehicleOperationId = "NONE";
			int rosterCount;
			if (group)
				rosterCount = group.GetAgentsCount();
			if (vehicleView)
			{
				restorePending = vehicleView.IsRestorePending();
				vehicleFailureReason = vehicleView.GetFailureReason();
				vehicleTerminalReason = vehicleView.GetTerminalReason();
				if (!vehicleView.GetOperationId().IsEmpty())
					vehicleOperationId = vehicleView.GetOperationId();
			}
			string slotActivityLine = string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 state=%4 role=%5 posture=%6 alive=%7 target=%8 infantry_waypoint=%9",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.StateToString(slot.GetState()),
				AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
				slot.GetOperationalPosture(),
				alive,
				AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
				infantryWaypoint);
			slotActivityLine += string.Format(
				" vehicle_waypoint=%1 vehicle_state=%2 at_mob=%3 distance_to_mob_m=%4 meaningful_task=%5 allowed_idle_reason=%6 mob_presence_ms=%7",
				vehicleWaypoint,
				vehicleState,
				atMob,
				distanceToMobMeters,
				hasMeaningfulTask,
				allowedIdleReason,
				mobPresenceMs);
			slotActivityLine += string.Format(
				" taskless_age_ms=%1 restore_pending=%2 vehicle_failure_reason=%3 vehicle_terminal_reason=%4 idle_suppression=%5",
				slot.GetMeaningfulTaskLostAgeMs(),
				restorePending,
				vehicleFailureReason,
				vehicleTerminalReason,
				slot.GetMobIdleSuppressionReason());
			slotActivityLine += string.Format(
				" group=%1 group_generation=%2 roster=%3 assignment_revision=%4 base_revision=%5 operation_id=%6",
				GroupKey(group),
				slot.GetSpawnGeneration(),
				rosterCount,
				slot.GetStrategicAssignmentRevision(),
				m_iStrategicBaseRevision,
				vehicleOperationId);
			AICF_Stage35Diagnostics.Info("SLOT_ACTIVITY", slotActivityLine);
		}
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
			!AICF_Stage35Diagnostics.HasErrors() &&
			(!m_Stage3Config.GetVehiclesEnabled() || !AICF_Stage3Diagnostics.HasErrors());

		AICF_Stage2Diagnostics.Info(
			"MATCH_RELIABILITY_SUMMARY",
			string.Format(
				"audits=%1 order_attempts=%2 order_recovered=%3 stuck_detected=%4 stuck_recovered=%5 stuck_field_holds=%6 duplicate_spawns_prevented=%7 errors=%8",
				m_iLifecycleAudits,
				m_iOrderRecoveryAttempts,
				m_iOrderRecoveries,
				m_iStuckDetections,
				m_iStuckRecoveries,
				m_iStuckFieldHolds,
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

	protected int CountManagedGroups()
	{
		return CountFactionGroups(m_USState) + CountFactionGroups(m_USSRState);
	}

	protected int CountFactionGroups(AICF_FactionState factionState)
	{
		if (!factionState)
			return 0;

		int count;
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (slot && slot.GetGroup())
				count++;
		}
		return count;
	}

	protected int CountManagedWaypoints()
	{
		return CountFactionManagedWaypoints(m_USState) +
			CountFactionManagedWaypoints(m_USSRState);
	}

	protected int CountFactionManagedWaypoints(AICF_FactionState factionState)
	{
		if (!factionState)
			return 0;

		int count;
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot)
				continue;
			if (slot.GetWaypoint())
				count++;
			AICF_VehicleSlotView vehicleView;
			if (m_VehicleCoordinator)
				vehicleView = m_VehicleCoordinator.GetSlotView(slot);
			if (vehicleView && vehicleView.GetVehicleWaypoint())
				count++;
		}
		return count;
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
		return AICF_GroupRuntime.FormatEntityId(group);
	}

	protected string WaypointKey(AIWaypoint waypoint)
	{
		if (!waypoint)
			return "NONE";

		return waypoint.GetID().ToString();
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
			m_VehicleCoordinator.StopWithFactionContexts(
				cleanupEntities,
				m_USState,
				m_USFaction,
				m_USSRState,
				m_USSRFaction);
			m_VehicleCoordinator = null;
		}

		ReleaseFactionGroups(m_USState, cleanupEntities);
		ReleaseFactionGroups(m_USSRState, cleanupEntities);
		m_aSpawnAuditGroups.Clear();
		m_aSpawnAuditLastLoggedAtMs.Clear();
		m_aSpawnObserverGenerations.Clear();
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
				DetachSpawnObservers(group);
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
