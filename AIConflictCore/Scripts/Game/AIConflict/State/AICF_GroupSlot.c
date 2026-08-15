// Stable faction-local identity for one managed group across all replacements.
class AICF_GroupSlot
{
	protected int m_iSlotId;
	protected int m_iRoleIndex;
	protected AICF_EGroupRole m_Role;
	protected AICF_EGroupSlotState m_State;
	protected int m_iReinforcementReadyAtMs;
	protected int m_iSpawnStartedAtMs;
	protected int m_iSpawnGeneration;
	protected int m_iRosterSpawnRequestedAtMs;
	protected int m_iLastRosterProgressAtMs;
	protected int m_iRosterExpectedCount;
	protected int m_iLastOrderRecoveryAtMs;
	protected int m_iOrderRecoveryStartedAtMs;
	protected int m_iOrderRecoveryFirstStableAtMs;
	protected int m_iOrderRecoveryStablePolls;
	protected int m_iPendingOrderRecoveryAssignmentRevision;
	protected int m_iPendingOrderRecoveryGroupGeneration;
	protected int m_iPendingOrderRecoveryReliabilityAttemptId;
	protected int m_iLastProgressAtMs;
	protected int m_iStuckRecoveryCount;
	protected int m_iObjectiveHoldStartedAtMs;
	protected int m_iPersistentStuckFieldHoldStartedAtMs;
	protected bool m_bReplacementDeployment;
	protected bool m_bRosterSpawnRequested;
	protected bool m_bRosterCompletionCallbackObserved;
	protected bool m_bTargetUnavailableReported;
	protected bool m_bRecoveringFromStuck;
	protected bool m_bPersistentStuckReported;
	protected bool m_bPersistentStuckFieldHold;
	protected bool m_bObjectiveHoldReported;
	protected bool m_bLoadBlockReported;
	protected bool m_bPendingOrderRecoveryCountsAsStuck;
	protected bool m_bPendingOrderRecoveryCountsAsReliabilityAttempt;
	protected bool m_bUnexplainedMobIdleDeadlineReported;
	protected bool m_bMeaningfulTaskLossReported;
	protected bool m_bMeaningfulTaskDeadlineReported;
	protected float m_fBestDistanceToTarget = -1.0;
	protected string m_sPendingOrderRecoveryCause;
	protected int m_iStrategicAssignmentAtMs;
	protected int m_iStrategicAssignmentRevision;
	protected int m_iStrategicCandidateFirstSeenAtMs;
	protected int m_iUnexplainedMobIdleStartedAtMs;
	protected int m_iMeaningfulTaskLostStartedAtMs;
	protected int m_iMeaningfulTaskLossEpisode;
	protected int m_iMeaningfulTaskLossReportedAssignmentRevision = -1;
	protected int m_iLastCommanderMotionAtMs;
	protected int m_iPendingStuckRecoveryEvidenceStartedAtMs;
	protected int m_iPendingStuckRecoveryAttempt;
	protected int m_iStuckEpisodeSequence;
	protected int m_iStuckEpisodeId;
	protected int m_iPersistentStuckEpisodeId;
	protected int m_iPersistentStuckGroupGeneration;
	protected int m_iPersistentStuckAssignmentRevision;
	protected int m_iStuckRecoveryAttemptedTotal;
	protected int m_iStuckRecoveryRouteConfirmedTotal;
	protected int m_iStuckRecoveryMovementOnlyTotal;
	protected int m_iStuckRecoveryRegressedTotal;
	protected int m_iStuckRecoveryFailedTotal;
	protected int m_iStuckRecoverySupersededTotal;
	protected int m_iStuckRecoveryIssueFailedTotal;
	protected int m_iOwnedWaypointTerminalAtMs;
	protected int m_iOwnedWaypointTerminalGeneration;
	protected int m_iOrderReliabilityRepairFailureCount;
	protected int m_iMobEgressLastOutwardProgressAtMs;
	protected int m_iMobEgressLastHiddenRecoveryAttemptAtMs;
	protected bool m_bHasCommanderMotionSample;
	protected bool m_bPendingStuckRecoveryEvidence;
	protected bool m_bMobEgressSoftNudgeApplied;
	protected bool m_bMobEgressProgressExtensionReported;
	protected bool m_bMobEgressDeadlineDeferredReported;
	protected bool m_bMobEgressHiddenMutationConsumed;
	protected bool m_bOrderReliabilityRepairBudgetExhaustionReported;
	protected vector m_vLastCommanderMotionPosition;
	protected vector m_vPendingStuckRecoveryStartPosition;
	protected vector m_vStuckEpisodeAnchor;
	protected vector m_vPersistentStuckAnchor;
	protected float m_fPendingStuckRecoveryStartDistance = -1.0;
	protected float m_fStuckEpisodeAnchorDistance = -1.0;
	protected float m_fMobEgressBestDistanceFromMob = -1.0;
	protected string m_sOperationalPosture;
	protected string m_sStrategicCandidatePosture;
	protected string m_sMobIdleSuppressionReason;
	protected string m_sOwnedWaypointTerminalOutcome;
	protected string m_sMobEgressLastHiddenRecoveryRejection;
	protected string m_sMeaningfulTaskLossReportedWaypointId;
	protected ref array<AIAgent> m_aRosterObservedAgents = {};
	protected ref array<int> m_aRosterObservedAtMs = {};
	protected ref array<AIAgent> m_aRosterCallbackAgents = {};

	protected SCR_AIGroup m_Group;
	protected SCR_CampaignMilitaryBaseComponent m_TargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_ProgressTargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_ObjectiveHoldTargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_PendingOrderRecoveryTargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_StrategicCandidateTargetBase;
	protected AIWaypoint m_Waypoint;
	protected AIWaypoint m_OwnedWaypointTerminalWaypoint;
	protected AIWaypoint m_PendingOrderRecoveryWaypoint;
	protected AIWaypoint m_PendingStuckRecoveryWaypoint;
	protected SCR_AIGroup m_PendingOrderRecoveryGroup;
	protected SCR_AIGroup m_PendingStuckRecoveryGroup;
	protected SCR_CampaignMilitaryBaseComponent m_PendingStuckRecoveryTargetBase;
	protected SCR_AIGroup m_PersistentStuckGroup;
	protected SCR_CampaignMilitaryBaseComponent m_PersistentStuckTargetBase;

	void AICF_GroupSlot(int slotId, AICF_EGroupRole role, int roleIndex = 0)
	{
		m_iSlotId = slotId;
		m_iRoleIndex = roleIndex;
		m_Role = role;
		m_State = AICF_EGroupSlotState.EMPTY;
	}

	int GetSlotId()
	{
		return m_iSlotId;
	}

	AICF_EGroupRole GetRole()
	{
		return m_Role;
	}

	int GetRoleIndex()
	{
		return m_iRoleIndex;
	}

	string GetSlotKey()
	{
		switch (m_Role)
		{
			case AICF_EGroupRole.ATTACK:
				return string.Format("A%1", m_iRoleIndex);
			case AICF_EGroupRole.DEFEND:
				return string.Format("D%1", m_iRoleIndex);
			case AICF_EGroupRole.RESERVE:
				return string.Format("R%1", m_iRoleIndex);
		}

		return string.Format("S%1", m_iSlotId);
	}

	string GetOperationalPosture()
	{
		return m_sOperationalPosture;
	}

	int GetStrategicAssignmentAgeMs()
	{
		if (m_iStrategicAssignmentAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iStrategicAssignmentAtMs);
	}

	int GetStrategicAssignmentRevision()
	{
		return m_iStrategicAssignmentRevision;
	}

	int GetStrategicCandidateAgeMs()
	{
		if (m_iStrategicCandidateFirstSeenAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iStrategicCandidateFirstSeenAtMs);
	}

	void RecordStrategicAssignment(
		SCR_CampaignMilitaryBaseComponent targetBase,
		string posture)
	{
		m_sOperationalPosture = posture;
		m_iStrategicAssignmentAtMs = System.GetTickCount();
		m_iStrategicAssignmentRevision++;
		ResetOrderReliabilityRepairFailureBudget();
		ResetMeaningfulTaskObservation();
		ClearStrategicCandidate();
	}

	bool IsStrategicCandidateReady(
		SCR_CampaignMilitaryBaseComponent targetBase,
		string posture,
		int minimumDwellMs,
		int stableCandidateMs)
	{
		if (!targetBase)
			return false;
		if (targetBase == m_TargetBase && posture == m_sOperationalPosture)
		{
			ClearStrategicCandidate();
			return false;
		}

		if (m_StrategicCandidateTargetBase != targetBase || m_sStrategicCandidatePosture != posture)
		{
			m_StrategicCandidateTargetBase = targetBase;
			m_sStrategicCandidatePosture = posture;
			m_iStrategicCandidateFirstSeenAtMs = System.GetTickCount();
			return false;
		}

		if (GetStrategicAssignmentAgeMs() < minimumDwellMs)
			return false;

		return m_iStrategicCandidateFirstSeenAtMs > 0 &&
			System.GetTickCount(m_iStrategicCandidateFirstSeenAtMs) >= stableCandidateMs;
	}

	void ClearStrategicCandidate()
	{
		m_StrategicCandidateTargetBase = null;
		m_sStrategicCandidatePosture = string.Empty;
		m_iStrategicCandidateFirstSeenAtMs = 0;
	}

	int ObserveUnexplainedMobIdle(bool isIdle, int observationSlackMs = 0)
	{
		if (!isIdle)
		{
			m_iUnexplainedMobIdleStartedAtMs = 0;
			m_bUnexplainedMobIdleDeadlineReported = false;
			return 0;
		}

		if (m_iUnexplainedMobIdleStartedAtMs <= 0)
		{
			int nowMs = System.GetTickCount();
			m_iUnexplainedMobIdleStartedAtMs = Math.Max(1, nowMs - Math.Max(0, observationSlackMs));
			return System.GetTickCount(m_iUnexplainedMobIdleStartedAtMs);
		}

		return System.GetTickCount(m_iUnexplainedMobIdleStartedAtMs);
	}

	int GetUnexplainedMobIdleAgeMs()
	{
		if (m_iUnexplainedMobIdleStartedAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iUnexplainedMobIdleStartedAtMs);
	}

	int ObserveMeaningfulTaskLoss(bool taskLost, int observationSlackMs = 0)
	{
		if (!taskLost)
		{
			m_iMeaningfulTaskLostStartedAtMs = 0;
			m_bMeaningfulTaskLossReported = false;
			m_bMeaningfulTaskDeadlineReported = false;
			return 0;
		}

		if (m_iMeaningfulTaskLostStartedAtMs <= 0)
		{
			int nowMs = System.GetTickCount();
			m_iMeaningfulTaskLostStartedAtMs = Math.Max(1, nowMs - Math.Max(0, observationSlackMs));
		}

		return System.GetTickCount(m_iMeaningfulTaskLostStartedAtMs);
	}

	int GetMeaningfulTaskLostAgeMs()
	{
		if (m_iMeaningfulTaskLostStartedAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iMeaningfulTaskLostStartedAtMs);
	}

	bool MarkMeaningfulTaskLossReported(int assignmentRevision, string waypointId)
	{
		if (m_bMeaningfulTaskLossReported)
			return false;

		m_bMeaningfulTaskLossReported = true;
		m_iMeaningfulTaskLossEpisode++;
		m_iMeaningfulTaskLossReportedAssignmentRevision = assignmentRevision;
		m_sMeaningfulTaskLossReportedWaypointId = waypointId;
		return true;
	}

	bool HasReportedMeaningfulTaskLoss()
	{
		return m_bMeaningfulTaskLossReported;
	}

	int GetMeaningfulTaskLossEpisode()
	{
		return m_iMeaningfulTaskLossEpisode;
	}

	int GetReportedMeaningfulTaskLossAssignmentRevision()
	{
		return m_iMeaningfulTaskLossReportedAssignmentRevision;
	}

	string GetReportedMeaningfulTaskLossWaypointId()
	{
		return m_sMeaningfulTaskLossReportedWaypointId;
	}

	bool MarkMeaningfulTaskDeadlineReported()
	{
		if (m_bMeaningfulTaskDeadlineReported)
			return false;

		m_bMeaningfulTaskDeadlineReported = true;
		return true;
	}

	bool ObserveMobIdleSuppression(string reason)
	{
		if (reason.IsEmpty() || reason == "NONE")
		{
			bool suppressionCleared = !m_sMobIdleSuppressionReason.IsEmpty();
			m_sMobIdleSuppressionReason = string.Empty;
			return suppressionCleared;
		}

		if (m_sMobIdleSuppressionReason == reason)
			return false;

		m_sMobIdleSuppressionReason = reason;
		return true;
	}

	string GetMobIdleSuppressionReason()
	{
		return m_sMobIdleSuppressionReason;
	}

	int ObserveCommanderMotion(vector position, float minimumMovementMeters)
	{
		if (!m_bHasCommanderMotionSample)
		{
			m_bHasCommanderMotionSample = true;
			m_vLastCommanderMotionPosition = position;
			m_iLastCommanderMotionAtMs = System.GetTickCount();
			return 0;
		}

		if (vector.DistanceSqXZ(position, m_vLastCommanderMotionPosition) >=
			minimumMovementMeters * minimumMovementMeters)
		{
			m_vLastCommanderMotionPosition = position;
			m_iLastCommanderMotionAtMs = System.GetTickCount();
			return 0;
		}

		return System.GetTickCount(m_iLastCommanderMotionAtMs);
	}

	// Tracks progress specifically toward leaving the MOB envelope. Generic motion
	// is insufficient here because circling inside the spawn area must not make a
	// truly stalled group look healthy.
	int ObserveMobEgressOutwardProgress(
		bool requiresEgress,
		float distanceFromMobMeters,
		float minimumProgressMeters)
	{
		if (!requiresEgress || distanceFromMobMeters < 0)
		{
			ResetMobEgressRecovery();
			return 0;
		}

		int nowMs = System.GetTickCount();
		if (m_iMobEgressLastOutwardProgressAtMs <= 0 ||
			m_fMobEgressBestDistanceFromMob < 0)
		{
			m_iMobEgressLastOutwardProgressAtMs = nowMs;
			m_fMobEgressBestDistanceFromMob = distanceFromMobMeters;
			return 0;
		}

		float thresholdMeters = Math.Max(1.0, minimumProgressMeters);
		if (distanceFromMobMeters >= m_fMobEgressBestDistanceFromMob + thresholdMeters)
		{
			m_fMobEgressBestDistanceFromMob = distanceFromMobMeters;
			m_iMobEgressLastOutwardProgressAtMs = nowMs;
			return 0;
		}

		return System.GetTickCount(m_iMobEgressLastOutwardProgressAtMs);
	}

	float GetMobEgressBestDistanceFromMob()
	{
		return m_fMobEgressBestDistanceFromMob;
	}

	bool MarkMobEgressSoftNudgeApplied()
	{
		if (m_bMobEgressSoftNudgeApplied)
			return false;

		m_bMobEgressSoftNudgeApplied = true;
		return true;
	}

	bool MarkMobEgressProgressExtensionReported()
	{
		if (m_bMobEgressProgressExtensionReported)
			return false;

		m_bMobEgressProgressExtensionReported = true;
		return true;
	}

	bool MarkMobEgressDeadlineDeferredReported()
	{
		if (m_bMobEgressDeadlineDeferredReported)
			return false;

		m_bMobEgressDeadlineDeferredReported = true;
		return true;
	}

	bool CanAttemptMobEgressHiddenRecovery(int retryIntervalMs)
	{
		if (m_iMobEgressLastHiddenRecoveryAttemptAtMs <= 0)
			return true;

		return System.GetTickCount(m_iMobEgressLastHiddenRecoveryAttemptAtMs) >=
			Math.Max(1000, retryIntervalMs);
	}

	bool IsMobEgressHiddenMutationConsumed()
	{
		return m_bMobEgressHiddenMutationConsumed;
	}

	void MarkMobEgressHiddenMutationConsumed()
	{
		m_bMobEgressHiddenMutationConsumed = true;
	}

	void RecordMobEgressHiddenRecoveryAttempt(string rejectionReason)
	{
		m_iMobEgressLastHiddenRecoveryAttemptAtMs = System.GetTickCount();
		m_sMobEgressLastHiddenRecoveryRejection = rejectionReason;
	}

	string GetMobEgressLastHiddenRecoveryRejection()
	{
		return m_sMobEgressLastHiddenRecoveryRejection;
	}

	void ResetMobEgressRecovery()
	{
		m_iMobEgressLastOutwardProgressAtMs = 0;
		m_iMobEgressLastHiddenRecoveryAttemptAtMs = 0;
		m_fMobEgressBestDistanceFromMob = -1.0;
		m_bMobEgressSoftNudgeApplied = false;
		m_bMobEgressProgressExtensionReported = false;
		m_bMobEgressDeadlineDeferredReported = false;
		m_bMobEgressHiddenMutationConsumed = false;
		m_sMobEgressLastHiddenRecoveryRejection = string.Empty;
	}

	bool MarkUnexplainedMobIdleDeadlineReported()
	{
		if (m_bUnexplainedMobIdleDeadlineReported)
			return false;

		m_bUnexplainedMobIdleDeadlineReported = true;
		return true;
	}

	AICF_EGroupSlotState GetState()
	{
		return m_State;
	}

	SCR_AIGroup GetGroup()
	{
		return m_Group;
	}

	SCR_CampaignMilitaryBaseComponent GetTargetBase()
	{
		return m_TargetBase;
	}

	AIWaypoint GetWaypoint()
	{
		return m_Waypoint;
	}

	int GetReinforcementReadyAtMs()
	{
		return m_iReinforcementReadyAtMs;
	}

	int GetSpawnStartedAtMs()
	{
		return m_iSpawnStartedAtMs;
	}

	int GetSpawnGeneration()
	{
		return m_iSpawnGeneration;
	}

	bool MarkRosterSpawnRequested(int expectedCount)
	{
		if (m_State != AICF_EGroupSlotState.SPAWNING || !m_Group || m_bRosterSpawnRequested ||
			expectedCount <= 0)
			return false;

		m_bRosterSpawnRequested = true;
		m_iRosterExpectedCount = expectedCount;
		m_iRosterSpawnRequestedAtMs = System.GetTickCount();
		m_iLastRosterProgressAtMs = m_iRosterSpawnRequestedAtMs;
		return true;
	}

	bool IsRosterSpawnRequested()
	{
		return m_bRosterSpawnRequested;
	}

	int GetRosterExpectedCount()
	{
		return m_iRosterExpectedCount;
	}

	int GetRosterSpawnRequestAgeMs()
	{
		if (m_iRosterSpawnRequestedAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iRosterSpawnRequestedAtMs);
	}

	int GetRosterProgressAgeMs()
	{
		if (m_iLastRosterProgressAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iLastRosterProgressAtMs);
	}

	void ObserveRosterAgent(AIAgent agent, bool fromCallback = false)
	{
		if (!agent || m_State != AICF_EGroupSlotState.SPAWNING || !m_Group ||
			agent.GetParentGroup() != m_Group)
		{
			return;
		}

		if (!m_aRosterObservedAgents.Contains(agent))
		{
			m_aRosterObservedAgents.Insert(agent);
			m_aRosterObservedAtMs.Insert(System.GetTickCount());
			m_iLastRosterProgressAtMs = System.GetTickCount();
		}

		if (fromCallback && !m_aRosterCallbackAgents.Contains(agent))
			m_aRosterCallbackAgents.Insert(agent);
	}

	int GetRosterAgentObservedAgeMs(AIAgent agent)
	{
		int index = m_aRosterObservedAgents.Find(agent);
		if (index < 0 || index >= m_aRosterObservedAtMs.Count())
			return -1;

		return System.GetTickCount(m_aRosterObservedAtMs[index]);
	}

	int GetRosterCallbackAgentCount()
	{
		return m_aRosterCallbackAgents.Count();
	}

	bool WasRosterAgentObservedFromCallback(AIAgent agent)
	{
		return agent && m_aRosterCallbackAgents.Contains(agent);
	}

	void MarkRosterCompletionCallbackObserved()
	{
		if (m_State != AICF_EGroupSlotState.SPAWNING)
			return;

		m_bRosterCompletionCallbackObserved = true;
		m_iLastRosterProgressAtMs = System.GetTickCount();
	}

	bool WasRosterCompletionCallbackObserved()
	{
		return m_bRosterCompletionCallbackObserved;
	}

	int GetStuckRecoveryCount()
	{
		return m_iStuckRecoveryCount;
	}

	bool IsRecoveringFromStuck()
	{
		return m_bRecoveringFromStuck;
	}

	bool IsPersistentStuckFieldHold()
	{
		return m_bPersistentStuckFieldHold;
	}

	int EnsureStuckEpisode(vector anchor, float distanceMeters)
	{
		if (m_iStuckEpisodeId > 0)
			return m_iStuckEpisodeId;

		m_iStuckEpisodeSequence++;
		m_iStuckEpisodeId = m_iStuckEpisodeSequence;
		m_vStuckEpisodeAnchor = anchor;
		m_fStuckEpisodeAnchorDistance = distanceMeters;
		return m_iStuckEpisodeId;
	}

	int GetStuckEpisodeId()
	{
		return m_iStuckEpisodeId;
	}

	vector GetStuckEpisodeAnchor()
	{
		return m_vStuckEpisodeAnchor;
	}

	float GetStuckEpisodeAnchorDistance()
	{
		return m_fStuckEpisodeAnchorDistance;
	}

	float GetStuckEpisodeAnchorDelta(vector currentPosition)
	{
		if (m_iStuckEpisodeId <= 0)
			return 0;

		return Math.Sqrt(vector.DistanceSqXZ(currentPosition, m_vStuckEpisodeAnchor));
	}

	int GetPersistentStuckEpisodeId()
	{
		return m_iPersistentStuckEpisodeId;
	}

	vector GetPersistentStuckAnchor()
	{
		return m_vPersistentStuckAnchor;
	}

	float GetPersistentStuckAnchorDelta(vector currentPosition)
	{
		if (!m_bPersistentStuckFieldHold)
			return 0;

		return Math.Sqrt(vector.DistanceSqXZ(currentPosition, m_vPersistentStuckAnchor));
	}

	bool IsPersistentStuckContextCurrent()
	{
		return m_bPersistentStuckFieldHold &&
			m_Group == m_PersistentStuckGroup &&
			m_TargetBase == m_PersistentStuckTargetBase &&
			m_iSpawnGeneration == m_iPersistentStuckGroupGeneration &&
			m_iStrategicAssignmentRevision == m_iPersistentStuckAssignmentRevision;
	}

	int GetStuckRecoveryAttemptedTotal() { return m_iStuckRecoveryAttemptedTotal; }
	int GetStuckRecoveryRouteConfirmedTotal() { return m_iStuckRecoveryRouteConfirmedTotal; }
	int GetStuckRecoveryMovementOnlyTotal() { return m_iStuckRecoveryMovementOnlyTotal; }
	int GetStuckRecoveryRegressedTotal() { return m_iStuckRecoveryRegressedTotal; }
	int GetStuckRecoveryFailedTotal() { return m_iStuckRecoveryFailedTotal; }
	int GetStuckRecoverySupersededTotal() { return m_iStuckRecoverySupersededTotal; }
	int GetStuckRecoveryIssueFailedTotal() { return m_iStuckRecoveryIssueFailedTotal; }

	bool IsReplacementDeployment()
	{
		return m_bReplacementDeployment;
	}

	// Returns true only for the first report in the current graph generation.
	bool MarkTargetUnavailableReported()
	{
		if (m_bTargetUnavailableReported)
			return false;

		m_bTargetUnavailableReported = true;
		return true;
	}

	void ResetTargetUnavailableReport()
	{
		m_bTargetUnavailableReported = false;
	}

	bool MarkLoadBlockReported()
	{
		if (m_bLoadBlockReported)
			return false;

		m_bLoadBlockReported = true;
		return true;
	}

	bool HasLoadBlockReport()
	{
		return m_bLoadBlockReported;
	}

	void ResetLoadBlockReported()
	{
		m_bLoadBlockReported = false;
	}

	bool MarkPersistentStuckReported()
	{
		if (m_bPersistentStuckReported)
			return false;

		m_bPersistentStuckReported = true;
		return true;
	}

	void BeginPersistentStuckFieldHold(vector fieldPosition)
	{
		ClearPendingOrderRecovery();
		SupersedePendingStuckRecoveryEvidence("PERSISTENT_FIELD_HOLD");
		m_bPersistentStuckFieldHold = true;
		m_iPersistentStuckFieldHoldStartedAtMs = System.GetTickCount();
		m_iPersistentStuckEpisodeId = m_iStuckEpisodeId;
		m_PersistentStuckGroup = m_Group;
		m_PersistentStuckTargetBase = m_TargetBase;
		m_iPersistentStuckGroupGeneration = m_iSpawnGeneration;
		m_iPersistentStuckAssignmentRevision = m_iStrategicAssignmentRevision;
		m_vPersistentStuckAnchor = fieldPosition;
		m_bRecoveringFromStuck = false;
		m_iLastProgressAtMs = System.GetTickCount();
	}

	void ClearPersistentStuckFieldHold()
	{
		m_bPersistentStuckFieldHold = false;
		m_iPersistentStuckFieldHoldStartedAtMs = 0;
		m_iPersistentStuckEpisodeId = 0;
		m_PersistentStuckGroup = null;
		m_PersistentStuckTargetBase = null;
		m_iPersistentStuckGroupGeneration = 0;
		m_iPersistentStuckAssignmentRevision = 0;
		m_vPersistentStuckAnchor = vector.Zero;
	}

	bool IsPersistentStuckFieldHoldRetryDue(int holdMs)
	{
		return m_bPersistentStuckFieldHold && m_iPersistentStuckFieldHoldStartedAtMs > 0 &&
			System.GetTickCount(m_iPersistentStuckFieldHoldStartedAtMs) >= holdMs;
	}

	void ResumeFromPersistentStuckFieldHold(string reason)
	{
		ResetProgressTracking(reason);
	}

	bool BeginInitialSpawn()
	{
		if (m_State != AICF_EGroupSlotState.EMPTY)
			return false;

		ClearRuntimeReferences();
		m_bReplacementDeployment = false;
		m_iSpawnGeneration++;
		ResetOrderReliabilityRepairFailureBudget();
		m_iSpawnStartedAtMs = System.GetTickCount();
		m_State = AICF_EGroupSlotState.SPAWNING;
		return true;
	}

	bool BeginReplacementSpawn(int nowMilliseconds)
	{
		if (!IsReinforcementDue(nowMilliseconds))
			return false;

		ClearRuntimeReferences();
		m_bReplacementDeployment = true;
		m_iSpawnGeneration++;
		ResetOrderReliabilityRepairFailureBudget();
		m_iSpawnStartedAtMs = System.GetTickCount();
		m_State = AICF_EGroupSlotState.SPAWNING;
		return true;
	}

	bool BindSpawnedGroup(SCR_AIGroup group)
	{
		if (m_State != AICF_EGroupSlotState.SPAWNING || !group || m_Group)
			return false;

		m_Group = group;
		m_Group.GetOnWaypointCompleted().Insert(OnOwnedWaypointCompleted);
		m_Group.GetOnWaypointRemoved().Insert(OnOwnedWaypointRemoved);
		return true;
	}

	bool MarkReady()
	{
		if (m_State != AICF_EGroupSlotState.SPAWNING || !m_Group)
			return false;

		m_State = AICF_EGroupSlotState.READY;
		return true;
	}

	bool AssignObjective(SCR_CampaignMilitaryBaseComponent targetBase, AIWaypoint waypoint)
	{
		if (m_State != AICF_EGroupSlotState.READY || !m_Group || !targetBase || !waypoint)
			return false;

		// Any newly assigned waypoint supersedes a candidate that was still being
		// verified. RecoverOrder starts a fresh verification after this assignment.
		ClearPendingOrderRecovery();
		SupersedePendingStuckRecoveryEvidence("OBJECTIVE_REASSIGNED");
		ClearPersistentStuckFieldHold();

		if (m_ProgressTargetBase != targetBase)
			ResetProgressTracking("TARGET_CHANGED");
		else
			ClearObjectiveHold();

		ClearOwnedWaypointTerminalOutcome();
		m_TargetBase = targetBase;
		m_Waypoint = waypoint;
		return true;
	}

	string GetOwnedWaypointTerminalOutcome(AIWaypoint waypoint)
	{
		if (!waypoint || waypoint != m_OwnedWaypointTerminalWaypoint ||
			m_iOwnedWaypointTerminalGeneration != m_iSpawnGeneration)
		{
			return string.Empty;
		}

		return m_sOwnedWaypointTerminalOutcome;
	}

	int GetOwnedWaypointTerminalAgeMs(AIWaypoint waypoint)
	{
		if (GetOwnedWaypointTerminalOutcome(waypoint).IsEmpty() ||
			m_iOwnedWaypointTerminalAtMs <= 0)
		{
			return 0;
		}

		return System.GetTickCount(m_iOwnedWaypointTerminalAtMs);
	}

	protected void OnOwnedWaypointCompleted(AIWaypoint waypoint)
	{
		if (!waypoint || waypoint != m_Waypoint)
			return;

		RecordOwnedWaypointTerminalOutcome(waypoint, "GROUP_CALLBACK_COMPLETED", string.Empty);
	}

	protected void OnOwnedWaypointRemoved(AIWaypoint waypoint)
	{
		if (!waypoint || waypoint != m_Waypoint)
			return;

		string priorOutcome = GetOwnedWaypointTerminalOutcome(waypoint);
		if (priorOutcome != "GROUP_CALLBACK_COMPLETED")
			RecordOwnedWaypointTerminalOutcome(waypoint, "GROUP_CALLBACK_REMOVED", priorOutcome);
		else
			LogOwnedWaypointTerminalOutcome(waypoint, "GROUP_CALLBACK_REMOVED", priorOutcome);
	}

	protected void RecordOwnedWaypointTerminalOutcome(
		AIWaypoint waypoint,
		string outcome,
		string priorOutcome)
	{
		m_OwnedWaypointTerminalWaypoint = waypoint;
		m_sOwnedWaypointTerminalOutcome = outcome;
		m_iOwnedWaypointTerminalAtMs = System.GetTickCount();
		m_iOwnedWaypointTerminalGeneration = m_iSpawnGeneration;
		LogOwnedWaypointTerminalOutcome(waypoint, outcome, priorOutcome);
	}

	protected void LogOwnedWaypointTerminalOutcome(
		AIWaypoint waypoint,
		string outcome,
		string priorOutcome)
	{
		string factionKey = "NONE";
		if (m_Group && m_Group.GetFaction())
			factionKey = m_Group.GetFaction().GetFactionKey();
		array<AIWaypoint> waypointQueue = {};
		int queueCount;
		bool trackedInQueue;
		bool isCurrent;
		if (m_Group)
		{
			queueCount = m_Group.GetWaypoints(waypointQueue);
			trackedInQueue = waypointQueue.Contains(waypoint);
			isCurrent = m_Group.GetCurrentWaypoint() == waypoint;
		}
		if (priorOutcome.IsEmpty())
			priorOutcome = "NONE";
		AICF_Stage35Diagnostics.Info(
			"ORDER_WAYPOINT_TERMINAL_OBSERVED",
			string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 assignment_revision=%5 waypoint=%6 outcome=%7 prior_outcome=%8",
				factionKey,
				GetSlotKey(),
				m_iSlotId,
				m_iSpawnGeneration,
				m_iStrategicAssignmentRevision,
				waypoint.GetID(),
				outcome,
				priorOutcome) + string.Format(
				" is_current=%1 tracked_in_queue=%2 queue_count=%3",
				isCurrent,
				trackedInQueue,
				queueCount));
	}

	protected void ClearOwnedWaypointTerminalOutcome()
	{
		m_OwnedWaypointTerminalWaypoint = null;
		m_sOwnedWaypointTerminalOutcome = string.Empty;
		m_iOwnedWaypointTerminalAtMs = 0;
		m_iOwnedWaypointTerminalGeneration = 0;
	}

	bool CanAttemptOrderRecovery(int retryIntervalMs)
	{
		if (m_iLastOrderRecoveryAtMs <= 0)
			return true;

		return System.GetTickCount(m_iLastOrderRecoveryAtMs) >= retryIntervalMs;
	}

	void MarkOrderRecoveryAttempt()
	{
		m_iLastOrderRecoveryAtMs = System.GetTickCount();
	}

	int RecordOrderReliabilityRepairFailure()
	{
		m_iOrderReliabilityRepairFailureCount++;
		return m_iOrderReliabilityRepairFailureCount;
	}

	int GetOrderReliabilityRepairFailureCount()
	{
		return m_iOrderReliabilityRepairFailureCount;
	}

	bool IsOrderReliabilityRepairFailureBudgetExhausted(int maximumFailures)
	{
		return maximumFailures > 0 &&
			m_iOrderReliabilityRepairFailureCount >= maximumFailures;
	}

	bool MarkOrderReliabilityRepairBudgetExhaustionReported()
	{
		if (m_bOrderReliabilityRepairBudgetExhaustionReported)
			return false;

		m_bOrderReliabilityRepairBudgetExhaustionReported = true;
		return true;
	}

	protected void ResetOrderReliabilityRepairFailureBudget()
	{
		m_iOrderReliabilityRepairFailureCount = 0;
		m_bOrderReliabilityRepairBudgetExhaustionReported = false;
	}

	void BeginOrderRecoveryVerification(
		string failureReason,
		bool countsAsStuckRecovery = false,
		bool countsAsReliabilityAttempt = false)
	{
		ClearPendingOrderRecovery();
		if (!m_Group || !m_TargetBase || !m_Waypoint)
			return;

		m_PendingOrderRecoveryGroup = m_Group;
		m_PendingOrderRecoveryTargetBase = m_TargetBase;
		m_PendingOrderRecoveryWaypoint = m_Waypoint;
		m_sPendingOrderRecoveryCause = failureReason;
		m_bPendingOrderRecoveryCountsAsStuck = countsAsStuckRecovery;
		m_bPendingOrderRecoveryCountsAsReliabilityAttempt =
			countsAsReliabilityAttempt && !countsAsStuckRecovery;
		m_iPendingOrderRecoveryAssignmentRevision = m_iStrategicAssignmentRevision;
		m_iPendingOrderRecoveryGroupGeneration = m_iSpawnGeneration;
		m_iOrderRecoveryStartedAtMs = System.GetTickCount();
	}

	// RecoverOrder creates the durability candidate inside the planner. The
	// reliability caller marks that candidate immediately after the synchronous
	// call, keeping vehicle handoff and stuck-route verification out of repair
	// attempt accounting without coupling the planner to controller telemetry.
	bool MarkPendingOrderRecoveryAsReliabilityAttempt(int attemptId)
	{
		if (!HasPendingOrderRecovery() || m_bPendingOrderRecoveryCountsAsStuck || attemptId <= 0)
			return false;

		m_bPendingOrderRecoveryCountsAsReliabilityAttempt = true;
		m_iPendingOrderRecoveryReliabilityAttemptId = attemptId;
		return true;
	}

	bool HasPendingOrderRecovery()
	{
		return m_PendingOrderRecoveryGroup && m_PendingOrderRecoveryTargetBase &&
			m_PendingOrderRecoveryWaypoint;
	}

	bool IsPendingOrderRecoveryContextCurrent()
	{
		return HasPendingOrderRecovery() && m_Group == m_PendingOrderRecoveryGroup &&
			m_TargetBase == m_PendingOrderRecoveryTargetBase &&
			m_Waypoint == m_PendingOrderRecoveryWaypoint &&
			m_iSpawnGeneration == m_iPendingOrderRecoveryGroupGeneration &&
			m_iStrategicAssignmentRevision == m_iPendingOrderRecoveryAssignmentRevision;
	}

	SCR_AIGroup GetPendingOrderRecoveryGroup()
	{
		return m_PendingOrderRecoveryGroup;
	}

	SCR_CampaignMilitaryBaseComponent GetPendingOrderRecoveryTargetBase()
	{
		return m_PendingOrderRecoveryTargetBase;
	}

	AIWaypoint GetPendingOrderRecoveryWaypoint()
	{
		return m_PendingOrderRecoveryWaypoint;
	}

	string GetPendingOrderRecoveryCause()
	{
		return m_sPendingOrderRecoveryCause;
	}

	bool PendingOrderRecoveryCountsAsStuckRecovery()
	{
		return m_bPendingOrderRecoveryCountsAsStuck;
	}

	bool PendingOrderRecoveryCountsAsReliabilityAttempt()
	{
		return m_bPendingOrderRecoveryCountsAsReliabilityAttempt;
	}

	int GetPendingOrderRecoveryReliabilityAttemptId()
	{
		return m_iPendingOrderRecoveryReliabilityAttemptId;
	}

	int GetPendingOrderRecoveryAssignmentRevision()
	{
		return m_iPendingOrderRecoveryAssignmentRevision;
	}

	int GetPendingOrderRecoveryGroupGeneration()
	{
		return m_iPendingOrderRecoveryGroupGeneration;
	}

	int RecordPendingOrderRecoveryStablePoll()
	{
		if (m_iOrderRecoveryFirstStableAtMs <= 0)
			m_iOrderRecoveryFirstStableAtMs = System.GetTickCount();

		m_iOrderRecoveryStablePolls++;
		return m_iOrderRecoveryStablePolls;
	}

	int GetPendingOrderRecoveryStableElapsedMs()
	{
		if (m_iOrderRecoveryFirstStableAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iOrderRecoveryFirstStableAtMs);
	}

	int GetPendingOrderRecoveryStablePolls()
	{
		return m_iOrderRecoveryStablePolls;
	}

	int GetPendingOrderRecoveryElapsedMs()
	{
		if (m_iOrderRecoveryStartedAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iOrderRecoveryStartedAtMs);
	}

	void ClearPendingOrderRecovery()
	{
		m_PendingOrderRecoveryGroup = null;
		m_PendingOrderRecoveryTargetBase = null;
		m_PendingOrderRecoveryWaypoint = null;
		m_sPendingOrderRecoveryCause = string.Empty;
		m_bPendingOrderRecoveryCountsAsStuck = false;
		m_bPendingOrderRecoveryCountsAsReliabilityAttempt = false;
		m_iPendingOrderRecoveryAssignmentRevision = 0;
		m_iPendingOrderRecoveryGroupGeneration = 0;
		m_iPendingOrderRecoveryReliabilityAttemptId = 0;
		m_iOrderRecoveryStartedAtMs = 0;
		m_iOrderRecoveryFirstStableAtMs = 0;
		m_iOrderRecoveryStablePolls = 0;
	}

	bool ObserveProgress(
		SCR_CampaignMilitaryBaseComponent targetBase,
		float distanceMeters,
		float minimumProgressMeters)
	{
		if (!targetBase || distanceMeters < 0)
			return false;

		if (m_ProgressTargetBase != targetBase || m_iLastProgressAtMs <= 0)
		{
			m_ProgressTargetBase = targetBase;
			m_fBestDistanceToTarget = distanceMeters;
			m_iLastProgressAtMs = System.GetTickCount();
			m_iStuckRecoveryCount = 0;
			m_bRecoveringFromStuck = false;
			m_bPersistentStuckReported = false;
			CompleteStuckEpisodeTracking();
			return true;
		}

		if (distanceMeters > m_fBestDistanceToTarget - minimumProgressMeters)
			return false;

		m_fBestDistanceToTarget = distanceMeters;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iStuckRecoveryCount = 0;
		m_bRecoveringFromStuck = false;
		m_bPersistentStuckReported = false;
		CompleteStuckEpisodeTracking();
		return true;
	}

	bool IsStuck(int timeoutMs)
	{
		return m_iLastProgressAtMs > 0 && System.GetTickCount(m_iLastProgressAtMs) >= timeoutMs;
	}

	void ConfirmAtObjective(
		SCR_CampaignMilitaryBaseComponent targetBase,
		float distanceMeters)
	{
		if (m_bPendingStuckRecoveryEvidence)
		{
			RecordStuckRecoveryTerminalOutcome("ROUTE_PROGRESS_AT_OBJECTIVE");
			ClearPendingStuckRecoveryEvidenceInternal();
		}
		m_ProgressTargetBase = targetBase;
		m_fBestDistanceToTarget = distanceMeters;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iStuckRecoveryCount = 0;
		m_bRecoveringFromStuck = false;
		m_bPersistentStuckReported = false;
		CompleteStuckEpisodeTracking();
	}

	// Returns true only when a fresh hold window starts for this objective.
	bool BeginObjectiveHold(SCR_CampaignMilitaryBaseComponent targetBase)
	{
		if (!targetBase)
			return false;

		if (m_ObjectiveHoldTargetBase == targetBase && m_iObjectiveHoldStartedAtMs > 0)
			return false;

		m_ObjectiveHoldTargetBase = targetBase;
		m_iObjectiveHoldStartedAtMs = System.GetTickCount();
		m_bObjectiveHoldReported = false;
		return true;
	}

	int GetObjectiveHoldElapsedMs()
	{
		if (m_iObjectiveHoldStartedAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iObjectiveHoldStartedAtMs);
	}

	bool MarkObjectiveHoldReported()
	{
		if (m_bObjectiveHoldReported)
			return false;

		m_bObjectiveHoldReported = true;
		return true;
	}

	void ClearObjectiveHold()
	{
		m_ObjectiveHoldTargetBase = null;
		m_iObjectiveHoldStartedAtMs = 0;
		m_bObjectiveHoldReported = false;
	}

	void RecordStuckRecovery(float currentDistanceMeters)
	{
		m_iStuckRecoveryCount++;
		m_iLastProgressAtMs = System.GetTickCount();
		// A transiently missing leader/target yields -1. Preserve the previous
		// usable baseline so the next positive sample can still prove progress.
		if (currentDistanceMeters >= 0)
			m_fBestDistanceToTarget = currentDistanceMeters;
		m_bRecoveringFromStuck = true;
	}

	void RecordStuckRecoveryAttempt(bool evidencePending)
	{
		m_iStuckRecoveryAttemptedTotal++;
		if (!evidencePending)
			m_iStuckRecoveryIssueFailedTotal++;
	}

	void ArmPendingStuckRecoveryEvidence(
		SCR_AIGroup group,
		SCR_CampaignMilitaryBaseComponent targetBase,
		AIWaypoint waypoint,
		vector leaderPosition,
		float routeDistanceMeters,
		int attempt)
	{
		SupersedePendingStuckRecoveryEvidence("NEW_RECOVERY_ATTEMPT");
		if (!group || !targetBase || !waypoint || attempt <= 0)
			return;

		m_bPendingStuckRecoveryEvidence = true;
		m_PendingStuckRecoveryGroup = group;
		m_PendingStuckRecoveryTargetBase = targetBase;
		m_PendingStuckRecoveryWaypoint = waypoint;
		m_vPendingStuckRecoveryStartPosition = leaderPosition;
		m_fPendingStuckRecoveryStartDistance = routeDistanceMeters;
		m_iPendingStuckRecoveryAttempt = attempt;
		m_iPendingStuckRecoveryEvidenceStartedAtMs = System.GetTickCount();
	}

	bool HasPendingStuckRecoveryEvidence()
	{
		return m_bPendingStuckRecoveryEvidence;
	}

	bool IsPendingStuckRecoveryEvidenceContextCurrent()
	{
		return m_bPendingStuckRecoveryEvidence &&
			m_Group == m_PendingStuckRecoveryGroup &&
			m_TargetBase == m_PendingStuckRecoveryTargetBase &&
			m_Waypoint == m_PendingStuckRecoveryWaypoint;
	}

	bool EvaluatePendingStuckRecoveryEvidence(
		vector leaderPosition,
		float routeDistanceMeters,
		float thresholdMeters,
		out bool movementResumed,
		out bool routeProgressResumed,
		out float displacementMeters,
		out float routeReductionMeters)
	{
		movementResumed = false;
		routeProgressResumed = false;
		displacementMeters = 0;
		routeReductionMeters = 0;
		if (!IsPendingStuckRecoveryEvidenceContextCurrent() || thresholdMeters <= 0)
			return false;

		displacementMeters = Math.Sqrt(vector.DistanceSqXZ(
			leaderPosition,
			m_vPendingStuckRecoveryStartPosition));
		if (m_fPendingStuckRecoveryStartDistance >= 0 && routeDistanceMeters >= 0)
			routeReductionMeters = m_fPendingStuckRecoveryStartDistance - routeDistanceMeters;

		movementResumed = displacementMeters >= thresholdMeters;
		routeProgressResumed = routeReductionMeters >= thresholdMeters;
		// Body displacement alone may be lateral or backwards. It is useful
		// telemetry, but only objective-route reduction proves recovery.
		return routeProgressResumed;
	}

	int GetPendingStuckRecoveryEvidenceAgeMs()
	{
		if (!m_bPendingStuckRecoveryEvidence ||
			m_iPendingStuckRecoveryEvidenceStartedAtMs <= 0)
		{
			return 0;
		}

		return System.GetTickCount(m_iPendingStuckRecoveryEvidenceStartedAtMs);
	}

	int GetPendingStuckRecoveryAttempt()
	{
		return m_iPendingStuckRecoveryAttempt;
	}

	AIWaypoint GetPendingStuckRecoveryWaypoint()
	{
		return m_PendingStuckRecoveryWaypoint;
	}

	void ConfirmPendingStuckRecoveryEvidence(float routeDistanceMeters)
	{
		SCR_CampaignMilitaryBaseComponent targetBase = m_PendingStuckRecoveryTargetBase;
		RecordStuckRecoveryTerminalOutcome("ROUTE_PROGRESS");
		ClearPendingStuckRecoveryEvidenceInternal();
		m_ProgressTargetBase = targetBase;
		m_fBestDistanceToTarget = routeDistanceMeters;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iStuckRecoveryCount = 0;
		m_bRecoveringFromStuck = false;
		m_bPersistentStuckReported = false;
		CompleteStuckEpisodeTracking();
	}

	void CompletePendingStuckRecoveryEvidence(string outcome)
	{
		if (!m_bPendingStuckRecoveryEvidence)
			return;

		RecordStuckRecoveryTerminalOutcome(outcome);
		ClearPendingStuckRecoveryEvidenceInternal();
	}

	void SupersedePendingStuckRecoveryEvidence(string reason)
	{
		if (!m_bPendingStuckRecoveryEvidence)
			return;

		string factionKey = "NONE";
		if (m_PendingStuckRecoveryGroup && m_PendingStuckRecoveryGroup.GetFaction())
			factionKey = m_PendingStuckRecoveryGroup.GetFaction().GetFactionKey();
		AICF_Stage2Diagnostics.Info(
			"GROUP_STUCK_RECOVERY",
			string.Format(
				"faction=%1 slot=%2 action=CONFIRM_EXECUTION success=0 attempt=%3 order_issue_succeeded=1 evidence_state=SUPERSEDED evidence_age_ms=%4 evidence_waypoint=%5",
				factionKey,
				m_iSlotId,
				m_iPendingStuckRecoveryAttempt,
				GetPendingStuckRecoveryEvidenceAgeMs(),
				m_PendingStuckRecoveryWaypoint.GetID()) + string.Format(
				" outcome=SUPERSEDED reason=%1 episode_id=%2 group_generation=%3 assignment_revision=%4",
				reason,
				m_iStuckEpisodeId,
				m_iSpawnGeneration,
				m_iStrategicAssignmentRevision));
		RecordStuckRecoveryTerminalOutcome("SUPERSEDED");
		ClearPendingStuckRecoveryEvidenceInternal();
	}

	void ClearPendingStuckRecoveryEvidence()
	{
		SupersedePendingStuckRecoveryEvidence("UNCLASSIFIED_CLEAR");
	}

	protected void RecordStuckRecoveryTerminalOutcome(string outcome)
	{
		if (outcome == "ROUTE_PROGRESS" || outcome == "ROUTE_PROGRESS_AT_OBJECTIVE")
			m_iStuckRecoveryRouteConfirmedTotal++;
		else if (outcome == "MOVEMENT_ONLY")
			m_iStuckRecoveryMovementOnlyTotal++;
		else if (outcome == "MOVEMENT_ONLY_REGRESSED")
			m_iStuckRecoveryRegressedTotal++;
		else if (outcome == "SUPERSEDED")
			m_iStuckRecoverySupersededTotal++;
		else if (outcome == "ISSUE_FAILED")
			m_iStuckRecoveryIssueFailedTotal++;
		else
			m_iStuckRecoveryFailedTotal++;
	}

	protected void ClearPendingStuckRecoveryEvidenceInternal()
	{
		m_bPendingStuckRecoveryEvidence = false;
		m_PendingStuckRecoveryGroup = null;
		m_PendingStuckRecoveryTargetBase = null;
		m_PendingStuckRecoveryWaypoint = null;
		m_vPendingStuckRecoveryStartPosition = vector.Zero;
		m_fPendingStuckRecoveryStartDistance = -1.0;
		m_iPendingStuckRecoveryAttempt = 0;
		m_iPendingStuckRecoveryEvidenceStartedAtMs = 0;
	}

	protected void CompleteStuckEpisodeTracking()
	{
		m_iStuckEpisodeId = 0;
		m_vStuckEpisodeAnchor = vector.Zero;
		m_fStuckEpisodeAnchorDistance = -1.0;
	}

	// The caller charges a replacement ticket immediately before committing the ready deployment.
	bool CommitDeploymentReady()
	{
		if (!CanCommitDeploymentReady())
			return false;

		m_bReplacementDeployment = false;
		return true;
	}

	bool CanCommitDeploymentReady()
	{
		return m_State == AICF_EGroupSlotState.READY && m_Group && m_TargetBase && m_Waypoint;
	}

	void ClearObjective()
	{
		ClearPendingOrderRecovery();
		SupersedePendingStuckRecoveryEvidence("OBJECTIVE_CLEARED");
		m_TargetBase = null;
		m_Waypoint = null;
	}

	// Vehicle control temporarily owns the group's waypoint queue while retaining
	// the strategic target needed to restore the infantry order after dismount.
	void SuspendObjectiveWaypoint()
	{
		ClearPendingOrderRecovery();
		SupersedePendingStuckRecoveryEvidence("VEHICLE_CONTROL_ACQUIRED");
		m_Waypoint = null;
		ClearObjectiveHold();
	}

	bool MarkDestroyed()
	{
		if (m_State != AICF_EGroupSlotState.SPAWNING && m_State != AICF_EGroupSlotState.READY)
			return false;

		ClearRuntimeReferences();
		m_State = AICF_EGroupSlotState.DESTROYED;
		return true;
	}

	bool BeginReinforcementWait(int readyAtMilliseconds)
	{
		if (m_State != AICF_EGroupSlotState.DESTROYED)
			return false;

		if (readyAtMilliseconds < 0)
			readyAtMilliseconds = 0;

		m_iReinforcementReadyAtMs = readyAtMilliseconds;
		m_State = AICF_EGroupSlotState.WAITING;
		return true;
	}

	bool IsReinforcementDue(int nowMilliseconds)
	{
		return m_State == AICF_EGroupSlotState.WAITING && nowMilliseconds >= m_iReinforcementReadyAtMs;
	}

	bool PostponeReinforcementUntil(int readyAtMilliseconds)
	{
		if (m_State != AICF_EGroupSlotState.WAITING)
			return false;

		if (readyAtMilliseconds > m_iReinforcementReadyAtMs)
			m_iReinforcementReadyAtMs = readyAtMilliseconds;
		return true;
	}

	bool ReturnSpawnToWait(int readyAtMilliseconds)
	{
		if (m_State != AICF_EGroupSlotState.SPAWNING)
			return false;

		ClearRuntimeReferences();
		if (readyAtMilliseconds < 0)
			readyAtMilliseconds = 0;

		m_iReinforcementReadyAtMs = readyAtMilliseconds;
		m_State = AICF_EGroupSlotState.WAITING;
		return true;
	}

	bool ResetInitialSpawn()
	{
		if (m_State != AICF_EGroupSlotState.SPAWNING)
			return false;

		Reset();
		return true;
	}

	void Reset()
	{
		ClearRuntimeReferences();
		m_bReplacementDeployment = false;
		m_State = AICF_EGroupSlotState.EMPTY;
	}

	bool IsCombatReady()
	{
		return m_State == AICF_EGroupSlotState.READY && m_Group;
	}

	protected void ClearRuntimeReferences()
	{
		if (m_Group)
		{
			m_Group.GetOnWaypointCompleted().Remove(OnOwnedWaypointCompleted);
			m_Group.GetOnWaypointRemoved().Remove(OnOwnedWaypointRemoved);
		}
		ClearPendingOrderRecovery();
		SupersedePendingStuckRecoveryEvidence("GROUP_RUNTIME_CLEARED");
		ClearOwnedWaypointTerminalOutcome();
		m_bRosterSpawnRequested = false;
		m_bRosterCompletionCallbackObserved = false;
		m_iRosterSpawnRequestedAtMs = 0;
		m_iLastRosterProgressAtMs = 0;
		m_iRosterExpectedCount = 0;
		m_aRosterObservedAgents.Clear();
		m_aRosterObservedAtMs.Clear();
		m_aRosterCallbackAgents.Clear();
		m_Group = null;
		m_TargetBase = null;
		m_Waypoint = null;
		m_iReinforcementReadyAtMs = 0;
		m_iSpawnStartedAtMs = 0;
		m_iLastOrderRecoveryAtMs = 0;
		m_bTargetUnavailableReported = false;
		m_bLoadBlockReported = false;
		m_sOperationalPosture = string.Empty;
		m_iStrategicAssignmentAtMs = 0;
		m_iStrategicAssignmentRevision = 0;
		ResetOrderReliabilityRepairFailureBudget();
		m_iUnexplainedMobIdleStartedAtMs = 0;
		m_bUnexplainedMobIdleDeadlineReported = false;
		ResetMeaningfulTaskObservation();
		m_iMeaningfulTaskLossEpisode = 0;
		m_iMeaningfulTaskLossReportedAssignmentRevision = -1;
		m_sMeaningfulTaskLossReportedWaypointId = string.Empty;
		m_sMobIdleSuppressionReason = string.Empty;
		m_iLastCommanderMotionAtMs = 0;
		m_bHasCommanderMotionSample = false;
		m_vLastCommanderMotionPosition = vector.Zero;
		ResetMobEgressRecovery();
		ClearStrategicCandidate();
		ResetProgressTracking("GROUP_RUNTIME_CLEARED");
	}

	protected void ResetProgressTracking(string reason)
	{
		SupersedePendingStuckRecoveryEvidence(reason);
		m_ProgressTargetBase = null;
		m_fBestDistanceToTarget = -1.0;
		m_iLastProgressAtMs = 0;
		m_iStuckRecoveryCount = 0;
		m_bRecoveringFromStuck = false;
		m_bPersistentStuckReported = false;
		m_bPersistentStuckFieldHold = false;
		m_iPersistentStuckFieldHoldStartedAtMs = 0;
		ClearPersistentStuckFieldHold();
		CompleteStuckEpisodeTracking();
		ClearObjectiveHold();
	}

	protected void ResetMeaningfulTaskObservation()
	{
		m_iMeaningfulTaskLostStartedAtMs = 0;
		m_bMeaningfulTaskLossReported = false;
		m_bMeaningfulTaskDeadlineReported = false;
	}
}
