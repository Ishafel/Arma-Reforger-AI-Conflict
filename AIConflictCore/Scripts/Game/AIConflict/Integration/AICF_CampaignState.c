// Primitive ticket values live on the already-replicated stock game mode, including JIP snapshots.
modded class SCR_GameModeCampaign
{
	[RplProp(onRplName: "AICF_OnTicketsReplicated")]
	protected int m_iAICFUSTickets;

	[RplProp(onRplName: "AICF_OnTicketsReplicated")]
	protected int m_iAICFUSSRTickets;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected bool m_bAICFStage4Enabled;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSTotalSupplies;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSConnectedSupplies;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSLogisticsTier;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSPendingReinforcements;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSShipmentsInTransit;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSSRTotalSupplies;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSSRConnectedSupplies;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSSRLogisticsTier;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSSRPendingReinforcements;

	[RplProp(onRplName: "AICF_OnStage4StateReplicated")]
	protected int m_iAICFUSSRShipmentsInTransit;

	[RplProp()]
	protected string m_sAICFUSStrategicObjective;

	[RplProp()]
	protected string m_sAICFUSOrderTargets;

	[RplProp()]
	protected string m_sAICFUSGroup0;

	[RplProp()]
	protected string m_sAICFUSGroup1;

	[RplProp()]
	protected string m_sAICFUSGroup2;

	[RplProp()]
	protected string m_sAICFUSGroup3;

	[RplProp()]
	protected int m_iAICFUSCombatGroups;

	[RplProp()]
	protected int m_iAICFUSManagedAgents;

	[RplProp()]
	protected string m_sAICFUSSRStrategicObjective;

	[RplProp()]
	protected string m_sAICFUSSROrderTargets;

	[RplProp()]
	protected string m_sAICFUSSRGroup0;

	[RplProp()]
	protected string m_sAICFUSSRGroup1;

	[RplProp()]
	protected string m_sAICFUSSRGroup2;

	[RplProp()]
	protected string m_sAICFUSSRGroup3;

	[RplProp()]
	protected int m_iAICFUSSRCombatGroups;

	[RplProp()]
	protected int m_iAICFUSSRManagedAgents;

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

	void AICF_SetStage4State(
		bool enabled,
		int usTotalSupplies,
		int usConnectedSupplies,
		int usLogisticsTier,
		int usPendingReinforcements,
		int usShipmentsInTransit,
		int ussrTotalSupplies,
		int ussrConnectedSupplies,
		int ussrLogisticsTier,
		int ussrPendingReinforcements,
		int ussrShipmentsInTransit)
	{
		if (!Replication.IsServer() || !IsMaster())
			return;
		if (m_bAICFStage4Enabled == enabled &&
			m_iAICFUSTotalSupplies == usTotalSupplies &&
			m_iAICFUSConnectedSupplies == usConnectedSupplies &&
			m_iAICFUSLogisticsTier == usLogisticsTier &&
			m_iAICFUSPendingReinforcements == usPendingReinforcements &&
			m_iAICFUSShipmentsInTransit == usShipmentsInTransit &&
			m_iAICFUSSRTotalSupplies == ussrTotalSupplies &&
			m_iAICFUSSRConnectedSupplies == ussrConnectedSupplies &&
			m_iAICFUSSRLogisticsTier == ussrLogisticsTier &&
			m_iAICFUSSRPendingReinforcements == ussrPendingReinforcements &&
			m_iAICFUSSRShipmentsInTransit == ussrShipmentsInTransit)
			return;

		m_bAICFStage4Enabled = enabled;
		m_iAICFUSTotalSupplies = Math.Max(0, usTotalSupplies);
		m_iAICFUSConnectedSupplies = Math.Max(0, usConnectedSupplies);
		m_iAICFUSLogisticsTier = usLogisticsTier;
		m_iAICFUSPendingReinforcements = Math.Max(0, usPendingReinforcements);
		m_iAICFUSShipmentsInTransit = Math.Max(0, usShipmentsInTransit);
		m_iAICFUSSRTotalSupplies = Math.Max(0, ussrTotalSupplies);
		m_iAICFUSSRConnectedSupplies = Math.Max(0, ussrConnectedSupplies);
		m_iAICFUSSRLogisticsTier = ussrLogisticsTier;
		m_iAICFUSSRPendingReinforcements = Math.Max(0, ussrPendingReinforcements);
		m_iAICFUSSRShipmentsInTransit = Math.Max(0, ussrShipmentsInTransit);
		Replication.BumpMe();
		AICF_OnStage4StateReplicated();
	}

	bool AICF_GetStage4Enabled() { return m_bAICFStage4Enabled; }
	int AICF_GetUSTotalSupplies() { return m_iAICFUSTotalSupplies; }
	int AICF_GetUSConnectedSupplies() { return m_iAICFUSConnectedSupplies; }
	int AICF_GetUSLogisticsTier() { return m_iAICFUSLogisticsTier; }
	int AICF_GetUSPendingReinforcements() { return m_iAICFUSPendingReinforcements; }
	int AICF_GetUSShipmentsInTransit() { return m_iAICFUSShipmentsInTransit; }
	int AICF_GetUSSRTotalSupplies() { return m_iAICFUSSRTotalSupplies; }
	int AICF_GetUSSRConnectedSupplies() { return m_iAICFUSSRConnectedSupplies; }
	int AICF_GetUSSRLogisticsTier() { return m_iAICFUSSRLogisticsTier; }
	int AICF_GetUSSRPendingReinforcements() { return m_iAICFUSSRPendingReinforcements; }
	int AICF_GetUSSRShipmentsInTransit() { return m_iAICFUSSRShipmentsInTransit; }

	void AICF_SetStrategicFactionState(
		bool isUSSR,
		string objective,
		string orderTargets,
		string group0,
		string group1,
		string group2,
		string group3,
		int combatGroups,
		int managedAgents)
	{
		if (!Replication.IsServer() || !IsMaster())
			return;

		combatGroups = Math.Max(0, combatGroups);
		managedAgents = Math.Max(0, managedAgents);
		bool changed;
		if (!isUSSR)
		{
			changed = m_sAICFUSStrategicObjective != objective ||
				m_sAICFUSOrderTargets != orderTargets ||
				m_sAICFUSGroup0 != group0 || m_sAICFUSGroup1 != group1 ||
				m_sAICFUSGroup2 != group2 || m_sAICFUSGroup3 != group3 ||
				m_iAICFUSCombatGroups != combatGroups ||
				m_iAICFUSManagedAgents != managedAgents;
			if (!changed)
				return;

			m_sAICFUSStrategicObjective = objective;
			m_sAICFUSOrderTargets = orderTargets;
			m_sAICFUSGroup0 = group0;
			m_sAICFUSGroup1 = group1;
			m_sAICFUSGroup2 = group2;
			m_sAICFUSGroup3 = group3;
			m_iAICFUSCombatGroups = combatGroups;
			m_iAICFUSManagedAgents = managedAgents;
		}
		else
		{
			changed = m_sAICFUSSRStrategicObjective != objective ||
				m_sAICFUSSROrderTargets != orderTargets ||
				m_sAICFUSSRGroup0 != group0 || m_sAICFUSSRGroup1 != group1 ||
				m_sAICFUSSRGroup2 != group2 || m_sAICFUSSRGroup3 != group3 ||
				m_iAICFUSSRCombatGroups != combatGroups ||
				m_iAICFUSSRManagedAgents != managedAgents;
			if (!changed)
				return;

			m_sAICFUSSRStrategicObjective = objective;
			m_sAICFUSSROrderTargets = orderTargets;
			m_sAICFUSSRGroup0 = group0;
			m_sAICFUSSRGroup1 = group1;
			m_sAICFUSSRGroup2 = group2;
			m_sAICFUSSRGroup3 = group3;
			m_iAICFUSSRCombatGroups = combatGroups;
			m_iAICFUSSRManagedAgents = managedAgents;
		}
		Replication.BumpMe();
	}

	string AICF_GetStrategicObjective(bool isUSSR)
	{
		if (isUSSR)
			return m_sAICFUSSRStrategicObjective;
		return m_sAICFUSStrategicObjective;
	}

	string AICF_GetOrderTargets(bool isUSSR)
	{
		if (isUSSR)
			return m_sAICFUSSROrderTargets;
		return m_sAICFUSOrderTargets;
	}

	string AICF_GetStrategicGroupSummary(bool isUSSR, int slotId)
	{
		if (isUSSR)
		{
			switch (slotId)
			{
				case 0: return m_sAICFUSSRGroup0;
				case 1: return m_sAICFUSSRGroup1;
				case 2: return m_sAICFUSSRGroup2;
				case 3: return m_sAICFUSSRGroup3;
			}
			return string.Empty;
		}

		switch (slotId)
		{
			case 0: return m_sAICFUSGroup0;
			case 1: return m_sAICFUSGroup1;
			case 2: return m_sAICFUSGroup2;
			case 3: return m_sAICFUSGroup3;
		}
		return string.Empty;
	}

	int AICF_GetCombatGroups(bool isUSSR)
	{
		if (isUSSR)
			return m_iAICFUSSRCombatGroups;
		return m_iAICFUSCombatGroups;
	}

	int AICF_GetManagedAgents(bool isUSSR)
	{
		if (isUSSR)
			return m_iAICFUSSRManagedAgents;
		return m_iAICFUSManagedAgents;
	}

	protected void AICF_OnTicketsReplicated()
	{
		if (!AICF_Stage1Diagnostics.IsConfigured())
			AICF_Stage1Diagnostics.Configure(string.Format("stage1-proxy-%1", System.GetTickCount()));

		AICF_Stage1Diagnostics.Info(
			"TICKETS_REPLICATED",
			string.Format("US=%1 USSR=%2", m_iAICFUSTickets, m_iAICFUSSRTickets));
	}

	protected void AICF_OnStage4StateReplicated()
	{
		if (!AICF_Stage1Diagnostics.IsConfigured())
			AICF_Stage1Diagnostics.Configure(string.Format("stage4-proxy-%1", System.GetTickCount()));

		string stateLine = string.Format(
			"enabled=%1 us_total=%2 us_connected=%3 us_tier=%4 us_pending=%5 us_shipments=%6",
			m_bAICFStage4Enabled,
			m_iAICFUSTotalSupplies,
			m_iAICFUSConnectedSupplies,
			m_iAICFUSLogisticsTier,
			m_iAICFUSPendingReinforcements,
			m_iAICFUSShipmentsInTransit);
		stateLine += string.Format(
			" ussr_total=%1 ussr_connected=%2 ussr_tier=%3 ussr_pending=%4 ussr_shipments=%5",
			m_iAICFUSSRTotalSupplies,
			m_iAICFUSSRConnectedSupplies,
			m_iAICFUSSRLogisticsTier,
			m_iAICFUSSRPendingReinforcements,
			m_iAICFUSSRShipmentsInTransit);
		AICF_Stage4Diagnostics.Info("STATE_REPLICATED", stateLine);
	}
}
