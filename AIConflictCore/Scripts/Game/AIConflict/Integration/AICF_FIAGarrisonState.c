// Исходный выбор не меняется при capture/JIP и не запускает клиентский spawn.
modded class SCR_CampaignMilitaryBaseComponent
{
	[RplProp()]
	protected string m_sAICFInitialGarrisonPlan;

	string AICF_GetInitialGarrisonPlan() { return m_sAICFInitialGarrisonPlan; }

	void AICF_SetInitialGarrisonPlan(string plan)
	{
		if (!Replication.IsServer() || IsProxy() || !m_sAICFInitialGarrisonPlan.IsEmpty() || plan.IsEmpty()) return;
		m_sAICFInitialGarrisonPlan = plan;
		Replication.BumpMe();
	}
}
