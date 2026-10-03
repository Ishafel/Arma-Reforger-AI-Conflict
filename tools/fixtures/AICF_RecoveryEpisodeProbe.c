// Отдельный run проверяет коррекцию двух исходных бойцов без изменения
// production deployment policy и без переноса доноров между группами fixture.
modded class AICF_GroupSlot
{
	override int GetDeploymentSize()
	{
		string enabled;
		if (GetSlotId() == 0 && System.GetCLIParam("aicfRecruitMultiMemberProbe", enabled) && enabled == "1")
			return 2;
		return super.GetDeploymentSize();
	}
}

class AICF_RecruitmentMoveFailureProbeNode : SCR_AIProcessFailedMovementResult
{
	bool Handle(SCR_AIGroup group, int result, int handler, bool related)
	{
		OnInit(group);
		return AICF_HandleFailedMovement(result, handler, related, group.GetCurrentWaypoint().GetOrigin());
	}
}

class AICF_SmartActionProbeNode : SCR_AIGetSmartActionsState
{
	void Initialize(AIAgent owner)
	{
		OnInit(owner);
	}
}

// Только stage, вместе с AICF_InfantryRecruitmentRuntimeProbe.c.
// Инъекции: удаление waypoint из queue и ускорение таймеров. Живые группы,
// сервис, восстановление приказов и последующая покупка — production.
modded class AICF_InfantryRecruitmentService
{
	protected int m_iEpisodeProbePhase;
	protected bool m_bEpisodeProbeFailed;
	protected ref AICF_InfantryRecruitmentOrder m_EpisodeProbeOrder;
	protected bool m_bHiddenProbeAttempted;
	protected vector m_vHiddenProbeOrigin;
	protected bool m_bMoveFailureProbeDone;

	bool EpisodeProbeDone()
	{
		return m_iEpisodeProbePhase == 3 && !m_bEpisodeProbeFailed;
	}

	override protected string Tick(AICF_InfantryRecruitmentOrder order, bool graphReady)
	{
		string enabled;
		if (!System.GetCLIParam("aicfRecoveryEpisodeProbe", enabled) || enabled != "1")
			return super.Tick(order, graphReady);
		if (m_iEpisodeProbePhase == 0 && order.IsCurrent(order.m_Slot) && !order.IsPhysicallyPresent())
		{
			string hiddenContinue;
			if (System.GetCLIParam("aicfRecruitHiddenContinue", hiddenContinue) && hiddenContinue == "1" &&
				(AICF_ContentProfile.GetActive().GetStableFactionKey(order.m_Faction.GetFactionKey()) != "USSR" ||
				vector.DistanceXZ(AICF_GroupRuntime.ResolveAliveLeader(order.m_Group).GetOrigin(), order.m_vPosition) < 55))
				return super.Tick(order, graphReady);
			m_EpisodeProbeOrder = order;
			EntityID groupId = order.m_GroupId;
			int started = order.m_iStartedAtMs;
			int token = order.m_iToken;
			order.m_Group.RemoveWaypoint(order.m_Waypoint);
			string result = super.Tick(order, graphReady);
			bool repaired = result.IsEmpty() && order.m_iApproachRepairs == 1 &&
				order.m_Group.GetCurrentWaypoint() == order.m_Waypoint &&
				order.m_GroupId == groupId && order.m_iStartedAtMs == started && order.m_iToken == token &&
				order.m_fPaid == 0 && !order.m_Donor;
			Print(string.Format("[AICF][EPISODE_PROBE] case=WAYPOINT_LOSS passed=%1", repaired));
			m_bEpisodeProbeFailed = !repaired;
			m_iEpisodeProbePhase = 1;
			return result;
		}
		if (m_iEpisodeProbePhase == 1 && order == m_EpisodeProbeOrder)
		{
			if (!m_bMoveFailureProbeDone)
			{
				SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(order.m_Group.GetGroupUtilityComponent().GetCurrentAction());
				if (!activity || activity.m_RelatedWaypoint != order.m_Waypoint || order.m_Group.GetCurrentWaypoint() != order.m_Waypoint)
					return super.Tick(order, graphReady);
				m_bMoveFailureProbeDone = true;
				int generation = order.m_iGeneration;
				order.m_iGeneration++;
				bool staleRejected = !order.m_Slot.ReportRecruitmentFailedMovement(order.m_Group, order.m_Waypoint);
				order.m_iGeneration = generation;
				bool emptyRejected = !order.m_Slot.ReportRecruitmentFailedMovement(order.m_Group, null);
				AICF_RecruitmentMoveFailureProbeNode node = new AICF_RecruitmentMoveFailureProbeNode();
				bool foreignRejected = !node.Handle(order.m_Group, EMoveError.UNKNOWN, 999999, true);
				bool unrelatedRejected = !node.Handle(order.m_Group, EMoveError.UNKNOWN, -1, false);
				bool handled = node.Handle(order.m_Group, EMoveError.UNKNOWN, -1, true);
				bool repeatHandled = order.m_Slot.ReportRecruitmentFailedMovement(order.m_Group, order.m_Waypoint) ==
					(order.m_Waypoint && order.m_Group.GetCurrentWaypoint() == order.m_Waypoint);
				bool passed = staleRejected && emptyRejected && foreignRejected && unrelatedRejected && handled && repeatHandled &&
					order.m_bMovementFailed && order.m_iApproachRepairs == 1 && order.m_fPaid == 0;
				Print(string.Format("[AICF][EPISODE_PROBE] case=RECRUITMENT_MOVE_FAILURE_FENCES passed=%1", passed));
				m_bEpisodeProbeFailed = m_bEpisodeProbeFailed || !passed;
			}
			string hidden;
			if (System.GetCLIParam("aicfRecruitHiddenProbe", hidden) && hidden == "1")
			{
				if (!m_bHiddenProbeAttempted)
				{
					m_bHiddenProbeAttempted = true;
					m_vHiddenProbeOrigin = AICF_GroupRuntime.ResolveAliveLeader(order.m_Group).GetOrigin();
					AICF_RecruitmentDeniedProbe denied = new AICF_RecruitmentDeniedProbe();
					bool fenced = !AICF_InfantryRecruitmentRecovery.TryRecover(order, denied, m_RecoveryConfig) &&
						denied.m_iCalls > 0 && !order.m_Slot.HasUsedIsolatedNavmeshRecovery();
					int generation = order.m_iGeneration;
					order.m_iGeneration++;
					bool stale = !AICF_InfantryRecruitmentRecovery.TryRecover(order, m_RecoveryWatchdog, m_RecoveryConfig);
					order.m_iGeneration = generation;
					order.m_iApproachProgressAtMs = System.GetTickCount() - 46000;
					order.m_fApproachBestDistance = 0;
					string recoveryResult = super.Tick(order, graphReady);
					bool submitted = recoveryResult.IsEmpty() && order.m_bHiddenRecoveryPending &&
						order.m_Slot.HasUsedIsolatedNavmeshRecovery() && !order.m_Donor && order.m_fPaid == 0;
					Print(string.Format("[AICF][EPISODE_PROBE] case=HIDDEN_FENCES_AND_SUBMISSION passed=%1 fence=%2 stale=%3 submitted=%4", fenced && stale && submitted, fenced, stale, submitted));
					m_bEpisodeProbeFailed = m_bEpisodeProbeFailed || !fenced || !stale || !submitted;
					return recoveryResult;
				}
				if (order.m_bHiddenRecoveryPending)
				{
					string observed = super.Tick(order, graphReady);
					if (!order.m_bHiddenRecoveryPending)
					{
						float shift = vector.DistanceXZ(m_vHiddenProbeOrigin, AICF_GroupRuntime.ResolveAliveLeader(order.m_Group).GetOrigin());
						bool bounded = shift > 0 && shift <= 10 && !order.m_Donor && order.m_fPaid == 0 &&
							!AICF_InfantryRecruitmentRecovery.TryRecover(order, m_RecoveryWatchdog, m_RecoveryConfig);
						Print(string.Format("[AICF][EPISODE_PROBE] case=HIDDEN_OBSERVED_AND_BUDGET passed=%1 observed_shift_m=%2 paid=%3", bounded, shift, order.m_fPaid));
						m_bEpisodeProbeFailed = m_bEpisodeProbeFailed || !bounded;
						// Отдельный focused run: после переноса оставляем тот же визит
						// идти до естественного прибытия и полного набора.
						string continueApproach;
						if (System.GetCLIParam("aicfRecruitHiddenContinue", continueApproach) && continueApproach == "1")
							m_iEpisodeProbePhase = 3;
					}
					return observed;
				}
			}
			order.m_iApproachProgressAtMs = System.GetTickCount() - 46000;
			order.m_fApproachBestDistance = 0;
			order.m_iApproachRepairs = 2;
			string result = super.Tick(order, graphReady);
			bool exhausted = result == "APPROACH_RECOVERY_EXHAUSTED" && !order.m_Donor && order.m_fPaid == 0;
			Print(string.Format("[AICF][EPISODE_PROBE] case=APPROACH_EXHAUSTION passed=%1", exhausted));
			m_bEpisodeProbeFailed = m_bEpisodeProbeFailed || !exhausted;
			m_iEpisodeProbePhase = 2;
			return result;
		}
		return super.Tick(order, graphReady);
	}

	override protected void ConsiderFaction(AICF_FactionState state, SCR_CampaignFaction faction)
	{
		if (m_iEpisodeProbePhase == 2 && m_EpisodeProbeOrder && m_EpisodeProbeOrder.m_Faction == faction)
		{
			AICF_InfantryRecruitmentOrder order = m_EpisodeProbeOrder;
			bool blocked = IsApproachBlocked(order.m_Slot, order.m_Service);
			bool contextRelease;
			foreach (AICF_RecruitmentApproachFailure failure : m_aApproachFailures)
			{
				if (failure.m_Slot == order.m_Slot && failure.m_ServiceId == order.m_ServiceId)
					contextRelease = !failure.IsContextCurrent(m_Graph.GetRevision() + 1);
			}
			Print(string.Format("[AICF][EPISODE_PROBE] case=BARRACKS_RETRY_CONTEXT passed=%1 blocked=%2 changed_graph_releases=%3",
				blocked && contextRelease, blocked, contextRelease));
			m_bEpisodeProbeFailed = m_bEpisodeProbeFailed || !blocked || !contextRelease;
			// Инъекция изменившегося контекста; затем штатный подход/покупка.
			m_aApproachFailures.Clear();
			m_iEpisodeProbePhase = 3;
		}
		super.ConsiderFaction(state, faction);
	}
}

// Инъекция отказа существующего player/LOS fence; положительный путь использует
// настоящий watchdog. Самих игроков эта headless fixture не создаёт.
class AICF_RecruitmentDeniedProbe : AICF_VehicleWatchdog
{
	int m_iCalls;
	override bool CanApplyHiddenRecovery(vector source, vector destination, float playerProtectionRadiusMeters, out float nearestPlayerMeters, out string rejectionReason)
	{
		m_iCalls++;
		nearestPlayerMeters = 1;
		rejectionReason = "PLAYER_CONTROLLED_ENTITY_NEARBY";
		return false;
	}
}

modded class AICF_RouteRecoveryEpisode
{
	protected bool m_bEpisodeProbeExpired;

	void EpisodeProbeExpire(bool expired)
	{
		m_bEpisodeProbeExpired = expired;
	}

	override int GetAgeMs()
	{
		if (m_bEpisodeProbeExpired)
			return DEADLINE_MS + 1;
		return super.GetAgeMs();
	}
}

modded class AICF_MatchController
{
	protected bool m_bRouteEpisodeProbeDone;

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfRecoveryEpisodeProbe", enabled) || enabled != "1" ||
			!m_bRosterReady || m_bStopped || m_bRouteEpisodeProbeDone)
			return;
		AICF_GroupSlot slot = m_USState.GetSlot(1);
		if (!slot || !slot.IsCombatReady() || !slot.GetWaypoint())
			return;
		AICF_RouteRecoveryEpisode episode = slot.GetRouteRecoveryEpisode();
		EntityID groupId = slot.GetGroup().GetID();
		episode.Begin(slot);
		episode.Observe(slot, m_ObjectiveGraph.GetRevision());
		episode.EpisodeProbeExpire(true);
		for (int attempt; attempt < 16; attempt++)
		{
			slot.ResetFalseCompletionRecovery();
			slot.RecordStrategicAssignment(slot.GetTargetBase(), slot.GetOperationalPosture());
			slot.BeginTemporaryRouteReplanHold(AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup()).GetOrigin());
		}
		AIWaypoint temporaryWaypoint = slot.GetWaypoint();
		int temporaryAssignment = slot.GetStrategicAssignmentRevision();
		bool postureBlocked = !m_OrderPlanner.ReconcileAICommanderOrder(slot, m_USFaction, m_ObjectiveGraph,
			m_TargetSelector, "PROBE_BASE_OWNER_CHANGED", 0, 0) &&
			!m_OrderPlanner.AssignAICommanderLossResponseOrder(slot, m_USFaction, m_ObjectiveGraph,
			m_TargetSelector, slot.GetTargetBase(), 0, 0) && slot.IsTemporaryRouteReplanHold() &&
			slot.GetStrategicAssignmentRevision() == temporaryAssignment && slot.GetWaypoint() == temporaryWaypoint;
		Print(string.Format("[AICF][EPISODE_PROBE] case=POSTURE_AND_QRF_HOLD_OWNER passed=%1", postureBlocked));
		bool exhausted = ProcessRouteRecoveryEpisode(slot, m_USFaction) && slot.IsPersistentStuckFieldHold() &&
			slot.GetGroup().GetID() == groupId;
		AIWaypoint heldWaypoint = slot.GetWaypoint();
		bool ownersBlocked = !m_OrderPlanner.CanRebuildAfterHiddenMobEgress(slot, m_USFaction) &&
			!m_OrderPlanner.RebuildCurrentOrder(slot, m_USFaction, "PROBE_MOB_EGRESS") &&
			!TryRecoverOrder(m_USState, slot, m_USFaction, "MEANINGFUL_TASK_LOST") &&
			ResolveAllowedIdleReason(slot, m_USFaction.GetMainBase(), false, false) == "PERSISTENT_STUCK_FIELD_HOLD" &&
			!slot.HasPendingOrderRecovery() && slot.GetWaypoint() == heldWaypoint;
		Print(string.Format("[AICF][EPISODE_PROBE] case=EXHAUSTED_OWNER_FENCES passed=%1", ownersBlocked));
		AICF_SmartActionProbeNode smartNode = new AICF_SmartActionProbeNode();
		smartNode.Initialize(slot.GetGroup());
		bool staleAction = smartNode.EOnTaskSimulate(slot.GetGroup(), 0.016) == ENodeResult.FAIL &&
			slot.GetWaypoint() == heldWaypoint && slot.GetGroup().GetCurrentWaypoint() == heldWaypoint;
		Print(string.Format("[AICF][EPISODE_PROBE] case=SMART_ACTION_NULL_CLEANUP passed=%1", staleAction));
		bool changed = episode.Observe(slot, m_ObjectiveGraph.GetRevision() + 1) == "CONTEXT_CHANGED";
		episode.EpisodeProbeExpire(false);
		Print(string.Format("[AICF][EPISODE_PROBE] case=REPLAN_EPISODE_BOUND passed=%1 hold=%2 changed_graph_releases=%3",
			exhausted && changed, exhausted, changed));
		ResumePersistentStuckFieldHold(slot, m_USFaction, "PROBE_CONTEXT_CHANGED");
		AssignFactionStrategicOrder(slot, m_USFaction, "PROBE_CONTEXT_CHANGED");
		m_bRouteEpisodeProbeDone = true;
	}
}
