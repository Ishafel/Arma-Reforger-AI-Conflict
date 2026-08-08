// Stable faction-local identity for one managed group across all replacements.
class AICF_GroupSlot
{
	protected int m_iSlotId;
	protected AICF_EGroupRole m_Role;
	protected AICF_EGroupSlotState m_State;
	protected int m_iReinforcementReadyAtMs;
	protected int m_iSpawnStartedAtMs;
	protected int m_iSpawnGeneration;
	protected int m_iLastOrderRecoveryAtMs;
	protected int m_iLastProgressAtMs;
	protected int m_iStuckRecoveryCount;
	protected int m_iObjectiveHoldStartedAtMs;
	protected bool m_bReplacementDeployment;
	protected bool m_bTargetUnavailableReported;
	protected bool m_bRecoveringFromStuck;
	protected bool m_bPersistentStuckReported;
	protected bool m_bObjectiveHoldReported;
	protected bool m_bLoadBlockReported;
	protected float m_fBestDistanceToTarget = -1.0;

	protected SCR_AIGroup m_Group;
	protected SCR_CampaignMilitaryBaseComponent m_TargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_ProgressTargetBase;
	protected SCR_CampaignMilitaryBaseComponent m_ObjectiveHoldTargetBase;
	protected AIWaypoint m_Waypoint;

	void AICF_GroupSlot(int slotId, AICF_EGroupRole role)
	{
		m_iSlotId = slotId;
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

	int GetStuckRecoveryCount()
	{
		return m_iStuckRecoveryCount;
	}

	bool IsRecoveringFromStuck()
	{
		return m_bRecoveringFromStuck;
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
		m_TargetBase = null;
		m_Waypoint = null;
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
		m_Group = null;
		m_TargetBase = null;
		m_Waypoint = null;
		m_iReinforcementReadyAtMs = 0;
		m_iSpawnStartedAtMs = 0;
		m_iLastOrderRecoveryAtMs = 0;
		m_bTargetUnavailableReported = false;
		m_bLoadBlockReported = false;
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
		ClearObjectiveHold();
	}
}
