// Отдельный post-fix runtime: native queued request перед сменой владельца.
modded class AICF_FIAGarrisonService
{
	protected SCR_GameModeCampaign m_AICFOwnershipCampaign;
	protected AICF_FIAGarrison m_AICFOwnershipTarget;
	protected int m_iAICFOwnershipAt;
	protected bool m_bAICFOwnershipDone;

	override void Start(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		m_AICFOwnershipCampaign = campaign;
		super.Start(campaign, graph);
	}

	override protected void Acquire(AICF_FIAGarrison g, int now)
	{
		super.Acquire(g, now);
		string mode;
		if (!System.GetCLIParam("aicfFIAProximityProbe", mode) || mode != "ownership" || m_AICFOwnershipTarget || !g.m_Group || g.m_bReady) return;
		m_AICFOwnershipTarget = g;
		m_iAICFOwnershipAt = now;
		g.m_Group.RequestSpawn(1, SCR_AmbientPatrolSystem.GetInstance().GetSpawnDistance());
		g.m_Base.SetFaction(m_AICFOwnershipCampaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR));
		AICF_FIAProximityProbe.Check("OWNERSHIP_STALE_EXPANSION", !g.m_Group.ExpandOneMember() && !g.m_Group.GetAgentsCount());
	}

	override void Update()
	{
		super.Update();
		if (!m_AICFOwnershipTarget || m_bAICFOwnershipDone || System.GetTickCount() - m_iAICFOwnershipAt < 2000) return;
		m_bAICFOwnershipDone = true;
		AICF_FIAGarrison g = m_AICFOwnershipTarget;
		AICF_FIAProximityProbe.Check("OWNERSHIP_QUEUED_SPAWN_CANCELLED", g.m_bRetired && g.m_Lifecycle && !g.m_Lifecycle.Current() && !g.m_Group.GetAgentsCount());
		g.m_Base.SetFaction(g.m_Faction);
		Print(string.Format("[AICF][FIA_PROXIMITY_DONE] mode=ownership checks=%1 failures=%2", AICF_FIAProximityProbe.s_iChecks, AICF_FIAProximityProbe.s_iFailures));
		GetGame().RequestClose();
	}
}
