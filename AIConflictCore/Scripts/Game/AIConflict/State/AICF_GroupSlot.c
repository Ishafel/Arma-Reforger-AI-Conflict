// Stable faction-local identity for one managed group across all replacements.
class AICF_GroupSlot
{
	protected int m_iSlotId;
	protected AICF_EGroupRole m_Role;
	protected AICF_EGroupSlotState m_State;
	protected int m_iReinforcementReadyAtMs;
	protected int m_iSpawnStartedAtMs;
	protected bool m_bReplacementDeployment;

	protected SCR_AIGroup m_Group;
	protected SCR_CampaignMilitaryBaseComponent m_TargetBase;
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

	bool IsReplacementDeployment()
	{
		return m_bReplacementDeployment;
	}

	bool BeginInitialSpawn()
	{
		if (m_State != AICF_EGroupSlotState.EMPTY)
			return false;

		ClearRuntimeReferences();
		m_bReplacementDeployment = false;
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
		m_iSpawnStartedAtMs = System.GetTickCount();
		m_State = AICF_EGroupSlotState.SPAWNING;
		return true;
	}

	bool BindSpawnedGroup(SCR_AIGroup group)
	{
		if (m_State != AICF_EGroupSlotState.SPAWNING || !group)
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

		m_TargetBase = targetBase;
		m_Waypoint = waypoint;
		return true;
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
	}
}
