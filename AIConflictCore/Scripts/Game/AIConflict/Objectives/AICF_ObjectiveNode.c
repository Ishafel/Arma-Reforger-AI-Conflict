class AICF_ObjectiveNode
{
	protected int m_iId;
	protected SCR_CampaignMilitaryBaseComponent m_Base;
	protected bool m_bObjective;
	protected ref array<int> m_aOutgoingNodeIds = {};

	void AICF_ObjectiveNode(int id, SCR_CampaignMilitaryBaseComponent base, bool objective)
	{
		m_iId = id;
		m_Base = base;
		m_bObjective = objective;
	}

	int GetId()
	{
		return m_iId;
	}

	SCR_CampaignMilitaryBaseComponent GetBase()
	{
		return m_Base;
	}

	bool IsObjective()
	{
		return m_bObjective;
	}

	array<int> GetOutgoingNodeIds()
	{
		return m_aOutgoingNodeIds;
	}

	void AddOutgoingNodeId(int nodeId)
	{
		if (!m_aOutgoingNodeIds.Contains(nodeId))
			m_aOutgoingNodeIds.Insert(nodeId);
	}
}
