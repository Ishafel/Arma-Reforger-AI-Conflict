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

	protected bool m_bStarted;
	protected bool m_bStopped;
	protected bool m_bSubscribed;
	protected bool m_bRosterReady;
	protected bool m_bReplanScheduled;
	protected bool m_bGraphRebuildNeeded;
	protected bool m_bResultLogged;
	protected bool m_bCampaignWasRunning;

	protected bool m_bObservedCapture;
	protected bool m_bObservedRetarget;
	protected bool m_bObservedRetargetWithinDeadline;
	protected bool m_bObservedReinforcement;
	protected bool m_bObservedTicketDebit;
	protected bool m_bObservedPlayerJoin;
	protected int m_iLastCaptureAtMs;
	protected FactionKey m_sObservedPlayerFaction;

	protected ref AICF_Stage1Config m_Config;
	protected ref AICF_ConflictAdapter m_ConflictAdapter;
	protected ref AICF_ObjectiveGraph m_ObjectiveGraph;
	protected ref AICF_TargetSelector m_TargetSelector;
	protected ref AICF_GroupSpawner m_GroupSpawner;
	protected ref AICF_ReinforcementSystem m_ReinforcementSystem;
	protected ref AICF_OrderPlanner m_OrderPlanner;
	protected ref AICF_VictorySystem m_VictorySystem;
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
		m_ConflictAdapter = new AICF_ConflictAdapter();
		m_ObjectiveGraph = new AICF_ObjectiveGraph();
		m_TargetSelector = new AICF_TargetSelector();
		m_GroupSpawner = new AICF_GroupSpawner();
		m_ReinforcementSystem = new AICF_ReinforcementSystem();
		m_OrderPlanner = new AICF_OrderPlanner();
		m_VictorySystem = new AICF_VictorySystem();

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
				"commander_interval_ms=%1 replacement_delay_ms=%2 initial_tickets=%3 groups_per_faction=%4 replacement_ticket_cost=%5 max_managed_agents=%6 expected_player_faction=%7",
				m_Config.GetCommanderIntervalMs(),
				m_Config.GetReinforcementDelayMs(),
				m_Config.GetInitialTickets(),
				AICF_Stage1Config.GROUP_SLOTS_PER_FACTION,
				m_Config.GetReplacementTicketCost(),
				m_Config.GetMaxManagedAgents(),
				expectedPlayerFaction));
		AICF_Stage1Diagnostics.Info("MATCH_START", "map=Arland factions=US,USSR");
		SyncTickets();

		if (!SpawnInitialRoster(m_USState, m_USFaction) || !SpawnInitialRoster(m_USSRState, m_USSRFaction))
		{
			Fail("INITIAL_ROSTER_FAILED", "All eight managed groups must be created");
			return;
		}

		GetGame().GetCallqueue().CallLater(Update, UPDATE_INTERVAL_MS, true);
		GetGame().GetCallqueue().CallLater(CommanderTick, m_Config.GetCommanderIntervalMs(), true);
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

			AICF_Stage1Diagnostics.Info(
				"ROLE_ASSIGNED",
				string.Format(
					"faction=%1 slot=%2 role=%3",
					faction.GetFactionKey(),
					slotId,
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole())));

			SCR_AIGroup group = m_GroupSpawner.SpawnGroup(faction, spawnBase, slotId);
			if (!group || !slot.BindSpawnedGroup(group))
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

			if (slot.GetState() == AICF_EGroupSlotState.READY && !slot.GetWaypoint())
			{
				CompleteReadyDeployment(factionState, faction, slot);
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
				"faction=%1 slot=%2 group=%3 agents=%4 deployment=%5",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				GroupKey(slot.GetGroup()),
				slot.GetGroup().GetAgentsCount(),
				reason));

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
		if (!factionState.TryReserveDeployment(AICF_EDeploymentKind.REPLACEMENT))
			return;

		if (!slot.BeginReplacementSpawn(nowMs))
		{
			factionState.ReleaseDeploymentReservation(AICF_EDeploymentKind.REPLACEMENT);
			return;
		}

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

		if (!slot.BindSpawnedGroup(group))
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

	protected void HandleSpawnTimeout(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_GroupSlot slot)
	{
		bool replacement = slot.IsReplacementDeployment();
		SCR_AIGroup group = slot.GetGroup();
		if (group)
		{
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
			ReplanAfterBaseChange();

		RevalidateFactionOrders(m_USState, m_USFaction, "COMMANDER_REPLAN");
		RevalidateFactionOrders(m_USSRState, m_USSRFaction, "COMMANDER_REPLAN");
	}

	protected void RevalidateFactionOrders(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		string reason)
	{
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot || !slot.IsCombatReady() || m_OrderPlanner.IsOrderValid(slot, faction))
				continue;

			SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
			SCR_CampaignMilitaryBaseComponent excludedTarget;
			if (oldTarget && oldTarget.GetFaction() == faction)
				excludedTarget = oldTarget;
			m_OrderPlanner.AssignOrder(slot, faction, m_ObjectiveGraph, m_TargetSelector, reason, excludedTarget);
		}
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
			if (!slot || !slot.IsCombatReady() || m_OrderPlanner.IsOrderValid(slot, faction))
				continue;

			SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
			if (m_OrderPlanner.AssignOrder(
				slot,
				faction,
				m_ObjectiveGraph,
				m_TargetSelector,
				"BASE_OWNER_CHANGED",
				m_LastChangedBase))
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
		bool playerFactionValid = m_bObservedPlayerJoin;
		if (!expectedPlayerFaction.IsEmpty())
			playerFactionValid = m_sObservedPlayerFaction == expectedPlayerFaction;

		bool success = m_bRosterReady &&
			m_bObservedCapture &&
			m_bObservedRetarget &&
			m_bObservedRetargetWithinDeadline &&
			m_bObservedReinforcement &&
			m_bObservedTicketDebit &&
			playerFactionValid &&
			m_ReinforcementSystem.HasRejectedUnsafeSite() &&
			!AICF_Stage1Diagnostics.HasErrors();

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
				group.GetOnEmpty().Remove(OnGroupEmpty);

			if (!cleanupEntities)
				continue;

			m_OrderPlanner.ClearOrder(slot);
			if (group)
				RplComponent.DeleteRplEntity(group, false);
			slot.Reset();
		}
	}
}
