// Read-only logistics view over the live stock Conflict bases and AICF graph.
class AICF_SupplyNetwork
{
	protected AICF_ObjectiveGraph m_Graph;
	protected AICF_ConflictAdapter m_ConflictAdapter;
	protected AICF_Stage4Config m_Config;

	void AICF_SupplyNetwork(
		AICF_ObjectiveGraph graph,
		AICF_ConflictAdapter conflictAdapter,
		AICF_Stage4Config config)
	{
		m_Graph = graph;
		m_ConflictAdapter = conflictAdapter;
		m_Config = config;
	}

	bool IsOperationalOwnedBase(
		SCR_CampaignMilitaryBaseComponent base,
		SCR_CampaignFaction faction)
	{
		if (!base || !faction || !base.GetOwner() || !base.IsInitialized())
			return false;
		if (base.GetFaction() != faction)
			return false;
		if (base.GetCaptureState() != SCR_EBaseCaptureState.NONE ||
			base.IsBeingCaptured() || base.AreEnemiesPresent())
			return false;
		return true;
	}

	bool IsSupplySource(
		SCR_CampaignMilitaryBaseComponent base,
		SCR_CampaignFaction faction)
	{
		return base && faction &&
			(base == faction.GetMainBase() || base.GetType() == SCR_ECampaignBaseType.SOURCE_BASE);
	}

	bool TryGetOperationalPath(
		SCR_CampaignMilitaryBaseComponent sourceBase,
		SCR_CampaignMilitaryBaseComponent targetBase,
		SCR_CampaignFaction faction,
		out int hopDistance)
	{
		hopDistance = -1;
		if (!m_Graph || !IsOperationalOwnedBase(sourceBase, faction) ||
			!IsOperationalOwnedBase(targetBase, faction))
			return false;

		array<int> path = {};
		if (!m_Graph.FindFriendlyPath(sourceBase, targetBase, faction.GetFactionKey(), path))
			return false;

		foreach (int nodeId : path)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(nodeId);
			if (!node || !IsOperationalOwnedBase(node.GetBase(), faction))
				return false;
		}

		hopDistance = Math.Max(0, path.Count() - 1);
		return true;
	}

	bool IsConnectedToSource(
		SCR_CampaignMilitaryBaseComponent base,
		SCR_CampaignFaction faction,
		out SCR_CampaignMilitaryBaseComponent sourceBase,
		out int hopDistance)
	{
		sourceBase = null;
		hopDistance = -1;
		if (!m_Graph || !IsOperationalOwnedBase(base, faction))
			return false;

		for (int nodeId = 0; nodeId < m_Graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(nodeId);
			SCR_CampaignMilitaryBaseComponent candidateSource = null;
			if (node)
				candidateSource = node.GetBase();
			if (!IsSupplySource(candidateSource, faction) ||
				!IsOperationalOwnedBase(candidateSource, faction))
				continue;

			int candidateHops;
			if (!TryGetOperationalPath(candidateSource, base, faction, candidateHops))
				continue;
			if (sourceBase && candidateHops >= hopDistance)
				continue;

			sourceBase = candidateSource;
			hopDistance = candidateHops;
		}

		return sourceBase != null;
	}

	AICF_ESupplyNetworkTier EvaluateReinforcementTier(
		SCR_CampaignFaction faction,
		int supplyCost)
	{
		if (!m_Graph || !m_ConflictAdapter || !faction || supplyCost <= 0)
			return AICF_ESupplyNetworkTier.BLOCKED;

		bool connectedAffordable;
		bool isolatedAffordable;
		for (int nodeId = 0; nodeId < m_Graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(nodeId);
			SCR_CampaignMilitaryBaseComponent base = null;
			if (node)
				base = node.GetBase();
			if (!base || !m_ConflictAdapter.GetSpawnRejectionReason(base, faction).IsEmpty())
				continue;
			if (base.GetSupplies() < supplyCost)
				continue;

			SCR_CampaignMilitaryBaseComponent sourceBase;
			int sourceHops;
			if (IsConnectedToSource(base, faction, sourceBase, sourceHops))
			{
				connectedAffordable = true;
				if (base.GetSupplies() >= supplyCost * m_Config.GetHealthyStockGroups())
					return AICF_ESupplyNetworkTier.HEALTHY;
			}
			else
			{
				isolatedAffordable = true;
			}
		}

		if (connectedAffordable)
			return AICF_ESupplyNetworkTier.STRAINED;
		if (isolatedAffordable)
			return AICF_ESupplyNetworkTier.ISOLATED;
		return AICF_ESupplyNetworkTier.BLOCKED;
	}

	int GetFactionTotalSupplies(SCR_CampaignFaction faction, bool connectedOnly)
	{
		int total;
		if (!m_Graph || !faction)
			return total;

		for (int nodeId = 0; nodeId < m_Graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(nodeId);
			SCR_CampaignMilitaryBaseComponent base = null;
			if (node)
				base = node.GetBase();
			if (!base || base.GetFaction() != faction || base.GetSuppliesMax() <= 0)
				continue;
			if (connectedOnly)
			{
				SCR_CampaignMilitaryBaseComponent sourceBase;
				int sourceHops;
				if (!IsConnectedToSource(base, faction, sourceBase, sourceHops))
					continue;
			}
			total += base.GetSupplies();
		}

		return total;
	}

	SCR_CampaignMilitaryBaseComponent FindReturnBase(SCR_CampaignFaction faction)
	{
		if (!faction)
			return null;
		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (IsOperationalOwnedBase(mainBase, faction) && mainBase.GetSuppliesMax() > 0)
			return mainBase;

		if (!m_Graph)
			return null;
		for (int nodeId = 0; nodeId < m_Graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(nodeId);
			SCR_CampaignMilitaryBaseComponent base = null;
			if (node)
				base = node.GetBase();
			if (IsSupplySource(base, faction) && IsOperationalOwnedBase(base, faction) &&
				base.GetSuppliesMax() > 0)
				return base;
		}

		return null;
	}

	bool HasInitializedFactionSupplyPool(SCR_CampaignFaction faction)
	{
		if (!m_Graph || !faction)
			return false;
		for (int nodeId = 0; nodeId < m_Graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(nodeId);
			if (node && node.GetBase() && node.GetBase().GetFaction() == faction &&
				node.GetBase().GetSuppliesMax() > 0)
				return true;
		}
		return false;
	}

	void ProbeInitialSupplies()
	{
		if (!m_Graph)
			return;
		for (int nodeId = 0; nodeId < m_Graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(nodeId);
			if (!node || !node.GetBase())
				continue;
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			Faction owner = base.GetFaction();
			FactionKey ownerKey = "NONE";
			if (owner)
				ownerKey = owner.GetFactionKey();
			AICF_Stage4Diagnostics.Info(
				"SUPPLY_PROBE",
				string.Format(
					"node=%1 base=%2 owner=%3 type=%4 supplies=%5 supplies_max=%6",
					nodeId,
					AICF_Stage1Diagnostics.BaseKey(base),
					ownerKey,
					AICF_Diagnostics.BaseTypeToString(base.GetType()),
					base.GetSupplies(),
					base.GetSuppliesMax()));
		}
	}
}
