// Primitive ticket values live on the already-replicated stock game mode, including JIP snapshots.
modded class SCR_GameModeCampaign
{
	[RplProp(onRplName: "AICF_OnTicketsReplicated")]
	protected int m_iAICFUSTickets;

	[RplProp(onRplName: "AICF_OnTicketsReplicated")]
	protected int m_iAICFUSSRTickets;

	void AICF_SetTickets(int usTickets, int ussrTickets)
	{
		if (!Replication.IsServer() || !IsMaster())
			return;

		if (usTickets < 0)
			usTickets = 0;
		if (ussrTickets < 0)
			ussrTickets = 0;

		if (m_iAICFUSTickets == usTickets && m_iAICFUSSRTickets == ussrTickets)
			return;

		m_iAICFUSTickets = usTickets;
		m_iAICFUSSRTickets = ussrTickets;
		Replication.BumpMe();

		// onRpl callbacks execute only on proxies, so notify/log on authority explicitly.
		AICF_OnTicketsReplicated();
	}

	int AICF_GetUSTickets()
	{
		return m_iAICFUSTickets;
	}

	int AICF_GetUSSRTickets()
	{
		return m_iAICFUSSRTickets;
	}

	protected void AICF_OnTicketsReplicated()
	{
		if (!AICF_Stage1Diagnostics.IsConfigured())
			AICF_Stage1Diagnostics.Configure(string.Format("stage1-proxy-%1", System.GetTickCount()));

		AICF_Stage1Diagnostics.Info(
			"TICKETS_REPLICATED",
			string.Format("US=%1 USSR=%2", m_iAICFUSTickets, m_iAICFUSSRTickets));
	}
}
