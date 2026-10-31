enum AICF_EAIQuotaGroupOwner
{
	SLOT,
	DONOR,
	BUILDER,
	LOGISTICS,
	FIA_GARRISON,
	FIA_DESANT,
	FIA_PATROL
}

// Это registered ownership, а не вывод из faction/prefab/group name. Физическое
// старое воплощение сохраняет свой bucket после Stop; новые dispatch требуют
// также текущий exact domain owner и session epoch.
class AICF_AIQuotaGroupOwner
{
	AICF_MatchController m_Controller;
	AICF_AIQuotaWorldState m_WorldState;
	SCR_AIGroup m_Group;
	BaseWorld m_MainWorld;
	EntityID m_GroupId;
	RplComponent m_GroupRpl;
	string m_sGroupRpl;
	Faction m_Faction;
	int m_iEpoch;
	int m_iSerial;
	AICF_EAIQuotaGroupOwner m_Kind;
	AICF_FactionState m_FactionState;
	AICF_GroupSlot m_Slot;
	AICF_InfantryRecruitmentOrder m_Recruit;
	AICF_BaseBuilder m_Builder;
	AICF_LogisticsWorker m_Logistics;
	AICF_FIAGarrison m_Garrison;
	AICF_FIAPatrol m_Patrol;
	int m_iGeneration;
	int m_iRequestToken;

	static AICF_AIQuotaGroupOwner Capture(AICF_MatchController controller, AICF_AIQuotaWorldState worldState,
		int epoch, int serial, SCR_AIGroup group, AICF_EAIQuotaGroupOwner kind)
	{
		if (!controller || !worldState || !worldState.IsSessionCurrent(controller, epoch) || serial <= 0 || !group ||
			kind < AICF_EAIQuotaGroupOwner.SLOT || kind > AICF_EAIQuotaGroupOwner.FIA_PATROL)
			return null;
		RplComponent rpl = RplComponent.Cast(group.FindComponent(RplComponent));
		if (!rpl || !rpl.IsMaster() || !rpl.Id().IsValid() || !group.GetFaction()) return null;
		AICF_AIQuotaGroupOwner record = new AICF_AIQuotaGroupOwner();
		record.m_Controller = controller;
		record.m_WorldState = worldState;
		record.m_iEpoch = epoch;
		record.m_iSerial = serial;
		record.m_Group = group;
		record.m_GroupId = group.GetID();
		record.m_MainWorld = group.GetWorld();
		record.m_GroupRpl = rpl;
		record.m_sGroupRpl = rpl.Id().ToString();
		record.m_Faction = group.GetFaction();
		record.m_Kind = kind;
		if (!record.MatchesIdentity()) return null;
		return record;
	}

	AICF_EAIQuotaBucket GetBucket()
	{
		if (m_Kind == AICF_EAIQuotaGroupOwner.SLOT || m_Kind == AICF_EAIQuotaGroupOwner.DONOR ||
			m_Kind == AICF_EAIQuotaGroupOwner.LOGISTICS)
			return AICF_EAIQuotaBucket.COMBAT;
		if (m_Kind == AICF_EAIQuotaGroupOwner.BUILDER) return AICF_EAIQuotaBucket.BUILDERS;
		if (m_Kind == AICF_EAIQuotaGroupOwner.FIA_GARRISON || m_Kind == AICF_EAIQuotaGroupOwner.FIA_DESANT ||
			m_Kind == AICF_EAIQuotaGroupOwner.FIA_PATROL) return AICF_EAIQuotaBucket.FIA_EXTRA;
		return -1;
	}

	bool MatchesIdentity()
	{
		if (!Replication.IsServer() || !GetGame() || !m_WorldState || !m_WorldState.IsCurrentWorld() ||
			!m_Group || !m_MainWorld || GetGame().GetWorld() != m_MainWorld || m_Group.GetWorld() != m_MainWorld ||
			m_Group.GetID() != m_GroupId || m_MainWorld.FindEntityByID(m_GroupId) != m_Group ||
			!m_Faction || m_Group.GetFaction() != m_Faction || !m_GroupRpl || !m_GroupRpl.IsMaster())
			return false;
		return m_Group.FindComponent(RplComponent) == m_GroupRpl && m_GroupRpl.Id().IsValid() &&
			!m_sGroupRpl.IsEmpty() && m_GroupRpl.Id().ToString() == m_sGroupRpl && m_iEpoch > 0 && m_iSerial > 0;
	}

	bool CanDispatch()
	{
		if (m_Kind == AICF_EAIQuotaGroupOwner.LOGISTICS && (!m_Logistics || m_Logistics.m_bQuotaSpawnClosed)) return false;
		return MatchesIdentity() && m_Controller && m_WorldState.IsSessionCurrent(m_Controller, m_iEpoch) &&
			m_Controller.OwnsAIQuotaGroupContext(this);
	}
}
