// Immutable planning-owned input consumed by the vehicle domain. Vehicle code
// may validate this snapshot, but may not choose a role, posture or target.
class AICF_StrategicAssignmentSnapshot
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected string m_sSlotKey;
	protected int m_iGroupGeneration;
	protected SCR_AIGroup m_Group;
	protected AICF_EGroupRole m_Role;
	protected string m_sPosture;
	protected SCR_CampaignMilitaryBaseComponent m_TargetBase;
	protected vector m_vTargetPosition;
	protected int m_iAssignmentRevision;
	protected int m_iBaseRevision;
	protected AIWaypoint m_MeaningfulInfantryWaypoint;
	protected int m_iAssignmentStartedAtMs;

	void AICF_StrategicAssignmentSnapshot(
		FactionKey factionKey,
		int slotId,
		string slotKey,
		int groupGeneration,
		SCR_AIGroup group,
		AICF_EGroupRole role,
		string posture,
		SCR_CampaignMilitaryBaseComponent targetBase,
		vector targetPosition,
		int assignmentRevision,
		int baseRevision,
		AIWaypoint meaningfulInfantryWaypoint = null,
		int assignmentStartedAtMs = 0)
	{
		m_sFactionKey = factionKey;
		m_iSlotId = slotId;
		m_sSlotKey = slotKey;
		m_iGroupGeneration = groupGeneration;
		m_Group = group;
		m_Role = role;
		m_sPosture = posture;
		m_TargetBase = targetBase;
		m_vTargetPosition = targetPosition;
		m_iAssignmentRevision = assignmentRevision;
		m_iBaseRevision = baseRevision;
		m_MeaningfulInfantryWaypoint = meaningfulInfantryWaypoint;
		m_iAssignmentStartedAtMs = assignmentStartedAtMs;
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	string GetSlotKey() { return m_sSlotKey; }
	int GetGroupGeneration() { return m_iGroupGeneration; }
	SCR_AIGroup GetGroup() { return m_Group; }
	AICF_EGroupRole GetRole() { return m_Role; }
	string GetPosture() { return m_sPosture; }
	SCR_CampaignMilitaryBaseComponent GetTargetBase() { return m_TargetBase; }
	vector GetTargetPosition() { return m_vTargetPosition; }
	int GetAssignmentRevision() { return m_iAssignmentRevision; }
	int GetBaseRevision() { return m_iBaseRevision; }
	AIWaypoint GetMeaningfulInfantryWaypoint() { return m_MeaningfulInfantryWaypoint; }
	int GetAssignmentStartedAtMs() { return m_iAssignmentStartedAtMs; }

	bool IsValid()
	{
		return !m_sFactionKey.IsEmpty() && m_iSlotId >= 0 && !m_sSlotKey.IsEmpty() &&
			m_iGroupGeneration > 0 && m_Group && m_TargetBase &&
			m_iAssignmentRevision >= 0 && m_iBaseRevision >= 0;
	}

	bool MatchesCurrent(
		FactionKey factionKey,
		int slotId,
		int groupGeneration,
		SCR_AIGroup group)
	{
		return factionKey == m_sFactionKey && slotId == m_iSlotId &&
			groupGeneration == m_iGroupGeneration && group && group == m_Group;
	}

	bool MatchesAssignmentRevision(AICF_StrategicAssignmentSnapshot other)
	{
		return other && other.GetFactionKey() == m_sFactionKey &&
			other.GetSlotId() == m_iSlotId &&
			other.GetGroupGeneration() == m_iGroupGeneration &&
			other.GetAssignmentRevision() == m_iAssignmentRevision &&
			other.GetBaseRevision() == m_iBaseRevision;
	}
}
