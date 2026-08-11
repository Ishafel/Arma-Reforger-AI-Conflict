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
	protected int m_iLastOrderRecoveryAtMs;
	protected int m_iOrderRecoveryStartedAtMs;
	protected int m_iOrderRecoveryFirstStableAtMs;
	protected int m_iOrderRecoveryStablePolls;
	protected int m_iLastProgressAtMs;
	protected int m_iStuckRecoveryCount;
	protected int m_iObjectiveHoldStartedAtMs;
	protected int m_iPersistentStuckFieldHoldStartedAtMs;
	protected bool m_bReplacementDeployment;
	protected bool m_bTargetUnavailableReported;
	protected bool m_bRecoveringFromStuck;
	protected bool m_bPersistentStuckReported;
	protected bool m_bPersistentStuckFieldHold;
	protected bool m_bObjectiveHoldReported;
	protected bool m_bLoadBlockReported;
	protected bool m_bPendingOrderRecoveryCountsAsStuck;
	protected bool m_bUnexplainedMobIdleDeadlineReported;
	protected bool m_bMeaningfulTaskLossReported;
	protected bool m_bMeaningfulTaskDeadlineReported;
	protected float m_fBestDistanceToTarget = -1.0;
	protected string m_sVehicleTerminalFailure;
	protected string m_sPendingOrderRecoveryCause;
	protected int m_iVehicleFallbackGroupGeneration = -1;
	protected int m_iStrategicAssignmentAtMs;
	protected int m_iStrategicCandidateFirstSeenAtMs;
	protected int m_iUnexplainedMobIdleStartedAtMs;
	protected int m_iMeaningfulTaskLostStartedAtMs;
	protected int m_iLastCommanderMotionAtMs;
	protected bool m_bHasCommanderMotionSample;
	protected vector m_vLastCommanderMotionPosition;
	protected string m_sOperationalPosture;
	protected string m_sStrategicCandidatePosture;
	protected string m_sMobIdleSuppressionReason;

	protected SCR_AIGroup m_Group;
	protected SCR_CampaignMilitaryBaseComponent m_TargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_ProgressTargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_ObjectiveHoldTargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_VehicleFallbackTargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_PendingOrderRecoveryTargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_StrategicCandidateTargetBase;
	protected AIWaypoint m_Waypoint;
	protected AIWaypoint m_PendingOrderRecoveryWaypoint;
	protected SCR_AIGroup m_PendingOrderRecoveryGroup;
	protected ref AICF_VehicleRuntime m_VehicleRuntime;

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

	bool MarkMeaningfulTaskLossReported()
	{
		if (m_bMeaningfulTaskLossReported)
			return false;

		m_bMeaningfulTaskLossReported = true;
		return true;
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

	AICF_VehicleRuntime GetVehicleRuntime()
	{
		return m_VehicleRuntime;
	}

	void SetVehicleRuntime(AICF_VehicleRuntime runtime)
	{
		m_VehicleRuntime = runtime;
	}

	void ClearVehicleRuntime(AICF_VehicleRuntime expected = null)
	{
		if (expected && expected != m_VehicleRuntime)
			return;

		m_VehicleRuntime = null;
	}

	bool HasVehicleTerminalFailure()
	{
		return !m_sVehicleTerminalFailure.IsEmpty();
	}

	string GetVehicleTerminalFailure()
	{
		return m_sVehicleTerminalFailure;
	}

	void RecordVehicleTerminalFailure(string reason)
	{
		m_sVehicleTerminalFailure = reason;
	}

	// A failed/abandoned trip must hand control back to infantry for the current
	// group and objective. A replacement group or a genuinely new target may
	// request transport again.
	void SuppressVehicleTripForAssignment(
		int groupGeneration,
		SCR_CampaignMilitaryBaseComponent targetBase)
	{
		m_iVehicleFallbackGroupGeneration = groupGeneration;
		m_VehicleFallbackTargetBase = targetBase;
	}

	bool IsVehicleTripSuppressedForCurrentAssignment()
	{
		return m_iVehicleFallbackGroupGeneration == m_iSpawnGeneration &&
			m_VehicleFallbackTargetBase && m_VehicleFallbackTargetBase == m_TargetBase;
	}

	void ClearVehicleTripSuppression()
	{
		m_iVehicleFallbackGroupGeneration = -1;
		m_VehicleFallbackTargetBase = null;
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

	void BeginPersistentStuckFieldHold()
	{
		ClearPendingOrderRecovery();
		m_bPersistentStuckFieldHold = true;
		m_iPersistentStuckFieldHoldStartedAtMs = System.GetTickCount();
		m_bRecoveringFromStuck = false;
		m_iLastProgressAtMs = System.GetTickCount();
	}

	void ClearPersistentStuckFieldHold()
	{
		m_bPersistentStuckFieldHold = false;
		m_iPersistentStuckFieldHoldStartedAtMs = 0;
	}

	bool IsPersistentStuckFieldHoldRetryDue(int holdMs)
	{
		return m_bPersistentStuckFieldHold && m_iPersistentStuckFieldHoldStartedAtMs > 0 &&
			System.GetTickCount(m_iPersistentStuckFieldHoldStartedAtMs) >= holdMs;
	}

	void ResumeFromPersistentStuckFieldHold()
	{
		ResetProgressTracking();
	}

	bool BeginInitialSpawn()
	{
		if (m_State != AICF_EGroupSlotState.EMPTY)
			return false;

		ClearRuntimeReferences();
		m_bReplacementDeployment = false;
		m_iSpawnGeneration++;
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
		m_iSpawnStartedAtMs = System.GetTickCount();
		m_State = AICF_EGroupSlotState.SPAWNING;
		return true;
	}

	bool BindSpawnedGroup(SCR_AIGroup group)
	{
		if (m_State != AICF_EGroupSlotState.SPAWNING || !group || m_Group)
			return false;

		m_Group = group;
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
		ClearPersistentStuckFieldHold();

		if (m_VehicleFallbackTargetBase && m_VehicleFallbackTargetBase != targetBase)
			ClearVehicleTripSuppression();

		if (m_ProgressTargetBase != targetBase)
			ResetProgressTracking();
		else
			ClearObjectiveHold();

		m_TargetBase = targetBase;
		m_Waypoint = waypoint;
		return true;
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

	void BeginOrderRecoveryVerification(string failureReason, bool countsAsStuckRecovery = false)
	{
		ClearPendingOrderRecovery();
		if (!m_Group || !m_TargetBase || !m_Waypoint)
			return;

		m_PendingOrderRecoveryGroup = m_Group;
		m_PendingOrderRecoveryTargetBase = m_TargetBase;
		m_PendingOrderRecoveryWaypoint = m_Waypoint;
		m_sPendingOrderRecoveryCause = failureReason;
		m_bPendingOrderRecoveryCountsAsStuck = countsAsStuckRecovery;
		m_iOrderRecoveryStartedAtMs = System.GetTickCount();
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
			m_Waypoint == m_PendingOrderRecoveryWaypoint;
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
			return true;
		}

		if (distanceMeters > m_fBestDistanceToTarget - minimumProgressMeters)
			return false;

		m_fBestDistanceToTarget = distanceMeters;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iStuckRecoveryCount = 0;
		m_bRecoveringFromStuck = false;
		m_bPersistentStuckReported = false;
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
		m_ProgressTargetBase = targetBase;
		m_fBestDistanceToTarget = distanceMeters;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iStuckRecoveryCount = 0;
		m_bRecoveringFromStuck = false;
		m_bPersistentStuckReported = false;
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
		m_TargetBase = null;
		m_Waypoint = null;
	}

	// Vehicle control temporarily owns the group's waypoint queue while retaining
	// the strategic target needed to restore the infantry order after dismount.
	void SuspendObjectiveWaypoint()
	{
		ClearPendingOrderRecovery();
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
		ClearPendingOrderRecovery();
		m_VehicleRuntime = null;
		m_Group = null;
		m_TargetBase = null;
		m_Waypoint = null;
		m_iReinforcementReadyAtMs = 0;
		m_iSpawnStartedAtMs = 0;
		m_iLastOrderRecoveryAtMs = 0;
		m_bTargetUnavailableReported = false;
		m_bLoadBlockReported = false;
		m_sVehicleTerminalFailure = string.Empty;
		m_sOperationalPosture = string.Empty;
		m_iStrategicAssignmentAtMs = 0;
		m_iUnexplainedMobIdleStartedAtMs = 0;
		m_bUnexplainedMobIdleDeadlineReported = false;
		m_iMeaningfulTaskLostStartedAtMs = 0;
		m_bMeaningfulTaskLossReported = false;
		m_bMeaningfulTaskDeadlineReported = false;
		m_sMobIdleSuppressionReason = string.Empty;
		m_iLastCommanderMotionAtMs = 0;
		m_bHasCommanderMotionSample = false;
		m_vLastCommanderMotionPosition = vector.Zero;
		ClearStrategicCandidate();
		ClearVehicleTripSuppression();
		ResetProgressTracking();
	}

	protected void ResetProgressTracking()
	{
		m_ProgressTargetBase = null;
		m_fBestDistanceToTarget = -1.0;
		m_iLastProgressAtMs = 0;
		m_iStuckRecoveryCount = 0;
		m_bRecoveringFromStuck = false;
		m_bPersistentStuckReported = false;
		m_bPersistentStuckFieldHold = false;
		m_iPersistentStuckFieldHoldStartedAtMs = 0;
		ClearObjectiveHold();
	}
}
