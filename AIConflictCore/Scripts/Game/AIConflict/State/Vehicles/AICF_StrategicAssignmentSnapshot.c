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
	protected AICF_EGroupUnitType m_UnitType;
	protected string m_sPosture;
	protected AICF_EOrderTargetKind m_TargetKind;
	protected SCR_CampaignMilitaryBaseComponent m_TargetBase;
	protected vector m_vTargetPosition;
	protected int m_iAssignmentRevision;
	protected int m_iStrategicIntentRevision;
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
		AICF_EGroupUnitType unitType,
		string posture,
		AICF_EOrderTargetKind targetKind,
		SCR_CampaignMilitaryBaseComponent targetBase,
		vector targetPosition,
		int assignmentRevision,
		int strategicIntentRevision,
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
		m_UnitType = unitType;
		m_sPosture = posture;
		m_TargetKind = targetKind;
		m_TargetBase = targetBase;
		m_vTargetPosition = targetPosition;
		m_iAssignmentRevision = assignmentRevision;
		m_iStrategicIntentRevision = strategicIntentRevision;
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
	AICF_EGroupUnitType GetUnitType() { return m_UnitType; }
	string GetPosture() { return m_sPosture; }
	AICF_EOrderTargetKind GetTargetKind() { return m_TargetKind; }
	SCR_CampaignMilitaryBaseComponent GetTargetBase() { return m_TargetBase; }
	vector GetTargetPosition() { return m_vTargetPosition; }
	int GetAssignmentRevision() { return m_iAssignmentRevision; }
	int GetStrategicIntentRevision() { return m_iStrategicIntentRevision; }
	int GetBaseRevision() { return m_iBaseRevision; }
	AIWaypoint GetMeaningfulInfantryWaypoint() { return m_MeaningfulInfantryWaypoint; }
	int GetAssignmentStartedAtMs() { return m_iAssignmentStartedAtMs; }

	bool IsValid()
	{
		bool positionValid = m_vTargetPosition[0] == m_vTargetPosition[0] &&
			m_vTargetPosition[1] == m_vTargetPosition[1] &&
			m_vTargetPosition[2] == m_vTargetPosition[2] &&
			Math.AbsFloat(m_vTargetPosition[0]) < float.MAX &&
			Math.AbsFloat(m_vTargetPosition[1]) < float.MAX &&
			Math.AbsFloat(m_vTargetPosition[2]) < float.MAX;
		bool destinationValid = m_TargetKind == AICF_EOrderTargetKind.POSITION ||
			(m_TargetKind == AICF_EOrderTargetKind.BASE && m_TargetBase);
		return !m_sFactionKey.IsEmpty() && m_iSlotId >= 0 && !m_sSlotKey.IsEmpty() &&
			m_iGroupGeneration > 0 && m_Group && destinationValid && positionValid &&
			m_iAssignmentRevision >= 0 && m_iStrategicIntentRevision >= 0 &&
			m_iBaseRevision >= 0;
	}

	bool MatchesDestination(AICF_StrategicAssignmentSnapshot other)
	{
		if (!other || other.GetTargetKind() != m_TargetKind ||
			other.GetTargetBase() != m_TargetBase)
		{
			return false;
		}
		if (m_TargetKind == AICF_EOrderTargetKind.POSITION)
			return other.GetTargetPosition() == m_vTargetPosition;
		return true;
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
			other.GetStrategicIntentRevision() == m_iStrategicIntentRevision &&
			other.GetBaseRevision() == m_iBaseRevision &&
			MatchesDestination(other);
	}
}
