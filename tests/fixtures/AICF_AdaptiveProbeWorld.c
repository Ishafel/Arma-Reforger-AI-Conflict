// Synthetic radio eligibility/threat inputs on stock bases, only in isolated stage.
// Real groups, live members, production scoring and replicated waypoints remain real.
class AICF_AdaptiveProbeWorld
{
	static SCR_CampaignFaction s_Faction;
	static ref array<SCR_CampaignMilitaryBaseComponent> s_Targets = {};
	static SCR_CampaignMilitaryBaseComponent s_Threat;
}

modded class SCR_CampaignMilitaryBaseComponent
{
	override bool IsValidTarget(notnull SCR_CampaignFaction faction)
	{
		if (faction == AICF_AdaptiveProbeWorld.s_Faction && AICF_AdaptiveProbeWorld.s_Targets.Contains(this))
			return GetFaction() != faction;
		return super.IsValidTarget(faction);
	}
	override bool AreEnemiesPresent()
	{
		if (this == AICF_AdaptiveProbeWorld.s_Threat)
			return true;
		return super.AreEnemiesPresent();
	}
}

modded class AICF_ObjectiveGraph
{
	void AICF_ProbeGraph(SCR_CampaignFaction faction, array<SCR_CampaignMilitaryBaseComponent> targets)
	{
		m_aNodes.Clear();
		m_aNodes.Insert(new AICF_ObjectiveNode(0, faction.GetMainBase(), false));
		for (int i; i < targets.Count(); i++)
		{
			m_aNodes.Insert(new AICF_ObjectiveNode(i + 1, targets[i], true));
			m_aNodes[0].AddOutgoingNodeId(i + 1);
		}
		m_iRevision++;
	}
	void AICF_ProbeRevision() { m_iRevision++; }
	void AICF_ProbeDependency(bool bypass)
	{
		m_aNodes[0].GetOutgoingNodeIds().Clear();
		m_aNodes[0].AddOutgoingNodeId(1);
		if (bypass) m_aNodes[0].AddOutgoingNodeId(2);
		m_aNodes[1].AddOutgoingNodeId(2);
		m_iRevision++;
	}
}
