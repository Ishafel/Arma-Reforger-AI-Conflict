// Bounded abstract logistics. Every packet is debited from a stock source and
// remains accounted as delivered, in transit, or returned.
class AICF_SupplyDeliverySystem
{
	protected AICF_Stage4Config m_Config;
	protected AICF_SupplyNetwork m_SupplyNetwork;
	protected AICF_ObjectiveGraph m_Graph;
	protected ref array<ref AICF_SupplyShipment> m_aShipments = {};
	protected int m_iNextShipmentId = 1;
	protected int m_iLastUSDispatchAtMs;
	protected int m_iLastUSSRDispatchAtMs;
	protected int m_iUSDispatchedSupplies;
	protected int m_iUSDeliveredSupplies;
	protected int m_iUSReturnedSupplies;
	protected int m_iUSSRDispatchedSupplies;
	protected int m_iUSSRDeliveredSupplies;
	protected int m_iUSSRReturnedSupplies;

	void AICF_SupplyDeliverySystem(
		AICF_Stage4Config config,
		AICF_SupplyNetwork supplyNetwork,
		AICF_ObjectiveGraph graph)
	{
		m_Config = config;
		m_SupplyNetwork = supplyNetwork;
		m_Graph = graph;
	}

	void Update(SCR_CampaignFaction faction)
	{
		if (!faction || !m_Config || !m_SupplyNetwork || !m_Graph)
			return;

		UpdateShipments(faction);
		TryDispatch(faction);
	}

	int GetInTransitCount(FactionKey factionKey)
	{
		int count;
		foreach (AICF_SupplyShipment shipment : m_aShipments)
		{
			if (shipment && shipment.GetFactionKey() == factionKey)
				count++;
		}
		return count;
	}

	int GetInTransitSupplies(FactionKey factionKey)
	{
		int total;
		foreach (AICF_SupplyShipment shipment : m_aShipments)
		{
			if (shipment && shipment.GetFactionKey() == factionKey)
				total += shipment.GetCargoSupplies();
		}
		return total;
	}

	int GetDispatchedSupplies(FactionKey factionKey)
	{
		if (factionKey == "US")
			return m_iUSDispatchedSupplies;
		return m_iUSSRDispatchedSupplies;
	}

	int GetDeliveredSupplies(FactionKey factionKey)
	{
		if (factionKey == "US")
			return m_iUSDeliveredSupplies;
		return m_iUSSRDeliveredSupplies;
	}

	int GetReturnedSupplies(FactionKey factionKey)
	{
		if (factionKey == "US")
			return m_iUSReturnedSupplies;
		return m_iUSSRReturnedSupplies;
	}

	void Stop(SCR_CampaignFaction usFaction, SCR_CampaignFaction ussrFaction)
	{
		for (int index = m_aShipments.Count() - 1; index >= 0; index--)
		{
			AICF_SupplyShipment shipment = m_aShipments[index];
			SCR_CampaignFaction faction = usFaction;
			if (shipment.GetFactionKey() == "USSR")
				faction = ussrFaction;
			if (!ReturnShipmentCargo(shipment, faction, "SYSTEM_STOP"))
			{
				AICF_Stage4Diagnostics.Error(
					"SHIPMENT_STOP_RETURN_FAILED",
					string.Format(
						"shipment=%1 faction=%2 cargo=%3",
						shipment.GetShipmentId(),
						shipment.GetFactionKey(),
						shipment.GetCargoSupplies()));
			}
			m_aShipments.Remove(index);
		}
	}

	protected void UpdateShipments(SCR_CampaignFaction faction)
	{
		FactionKey factionKey = faction.GetFactionKey();
		for (int index = m_aShipments.Count() - 1; index >= 0; index--)
		{
			AICF_SupplyShipment shipment = m_aShipments[index];
			if (!shipment || shipment.GetFactionKey() != factionKey)
				continue;
			if (shipment.GetState() == AICF_ESupplyShipmentState.RETURN_PENDING)
			{
				if (ReturnShipmentCargo(shipment, faction, "DESTINATION_LOST_RETURN_RETRY"))
					m_aShipments.Remove(index);
				continue;
			}

			SCR_CampaignMilitaryBaseComponent destination = shipment.GetDestinationBase();
			if (!m_SupplyNetwork.IsOperationalOwnedBase(destination, faction))
			{
				if (ReturnShipmentCargo(shipment, faction, "DESTINATION_LOST"))
					m_aShipments.Remove(index);
				else
					shipment.MarkReturnPending();
				continue;
			}

			int hopDistance;
			bool routeAvailable = m_SupplyNetwork.TryGetOperationalPath(
				shipment.GetSourceBase(),
				destination,
				faction,
				hopDistance);
			AICF_ESupplyShipmentState previousState = shipment.GetState();
			if (routeAvailable)
				shipment.UpdateRoute(hopDistance, m_Graph.GetRevision());

			bool arrived = shipment.Tick(routeAvailable);
			if (!routeAvailable && previousState != AICF_ESupplyShipmentState.PAUSED_ROUTE)
			{
				AICF_Stage4Diagnostics.Warning(
					"SHIPMENT_PAUSED",
					string.Format(
						"shipment=%1 faction=%2 destination=%3 remaining_ms=%4 reason=ROUTE_BROKEN",
						shipment.GetShipmentId(),
						factionKey,
						AICF_Stage1Diagnostics.BaseKey(destination),
						shipment.GetRemainingTravelMs()));
			}
			else if (routeAvailable && previousState == AICF_ESupplyShipmentState.PAUSED_ROUTE)
			{
				AICF_Stage4Diagnostics.Info(
					"SHIPMENT_RESUMED",
					string.Format(
						"shipment=%1 faction=%2 destination=%3 hops=%4 remaining_ms=%5",
						shipment.GetShipmentId(),
						factionKey,
						AICF_Stage1Diagnostics.BaseKey(destination),
						hopDistance,
						shipment.GetRemainingTravelMs()));
			}

			if (!arrived)
				continue;

			if (CompleteArrival(shipment, faction))
				m_aShipments.Remove(index);
		}
	}

	protected void TryDispatch(SCR_CampaignFaction faction)
	{
		FactionKey factionKey = faction.GetFactionKey();
		int nowMs = System.GetTickCount();
		int lastDispatchAtMs = m_iLastUSDispatchAtMs;
		if (factionKey == "USSR")
			lastDispatchAtMs = m_iLastUSSRDispatchAtMs;
		if (lastDispatchAtMs > 0 && System.GetTickCount(lastDispatchAtMs) < m_Config.GetDeliveryIntervalMs())
			return;

		if (factionKey == "US")
			m_iLastUSDispatchAtMs = nowMs;
		else
			m_iLastUSSRDispatchAtMs = nowMs;
		if (GetInTransitCount(factionKey) >= m_Config.GetMaxShipmentsPerFaction())
			return;

		SCR_CampaignMilitaryBaseComponent sourceBase;
		SCR_CampaignMilitaryBaseComponent destinationBase;
		int hopDistance;
		if (!SelectDispatchPair(faction, sourceBase, destinationBase, hopDistance))
			return;

		int cargo = m_Config.GetDeliveryPackageSupplies();
		float sourceBefore = sourceBase.GetSupplies();
		if (sourceBefore < m_Config.GetSourceReserveSupplies() + cargo)
			return;

		sourceBase.AddSupplies(-cargo);
		int travelMs = m_Config.GetDeliveryBaseTravelMs() + hopDistance * m_Config.GetDeliveryPerHopMs();
		ref AICF_SupplyShipment shipment = new AICF_SupplyShipment(
			m_iNextShipmentId++,
			factionKey,
			sourceBase,
			destinationBase,
			cargo,
			hopDistance,
			m_Graph.GetRevision(),
			travelMs);
		m_aShipments.Insert(shipment);
		RecordDispatched(factionKey, cargo);
		AICF_Stage4Diagnostics.Info(
			"SHIPMENT_DISPATCHED",
			string.Format(
				"shipment=%1 faction=%2 source=%3 destination=%4 cargo=%5 hops=%6 eta_ms=%7 source_before=%8 source_after_expected=%9",
				shipment.GetShipmentId(),
				factionKey,
				AICF_Stage1Diagnostics.BaseKey(sourceBase),
				AICF_Stage1Diagnostics.BaseKey(destinationBase),
				cargo,
				hopDistance,
				travelMs,
				sourceBefore,
				sourceBefore - cargo));
	}

	protected bool SelectDispatchPair(
		SCR_CampaignFaction faction,
		out SCR_CampaignMilitaryBaseComponent selectedSource,
		out SCR_CampaignMilitaryBaseComponent selectedDestination,
		out int selectedHops)
	{
		selectedSource = null;
		selectedDestination = null;
		selectedHops = -1;
		float bestDestinationSupplies = 1000000000.0;
		float bestSourceSupplies = -1.0;
		int bestDestinationNode = 1000000;
		int cargo = m_Config.GetDeliveryPackageSupplies();
		int targetStock = m_Config.GetReplacementSupplyCost() * m_Config.GetHealthyStockGroups();

		for (int destinationNodeId = 0; destinationNodeId < m_Graph.GetNodeCount(); destinationNodeId++)
		{
			AICF_ObjectiveNode destinationNode = m_Graph.GetNode(destinationNodeId);
			SCR_CampaignMilitaryBaseComponent destination = null;
			if (destinationNode)
				destination = destinationNode.GetBase();
			if (!destination || destination.GetType() != SCR_ECampaignBaseType.BASE ||
				!m_SupplyNetwork.IsOperationalOwnedBase(destination, faction))
				continue;
			if (HasDestinationShipment(destination) || destination.GetSupplies() >= targetStock)
				continue;
			if (destination.GetSuppliesMax() - destination.GetSupplies() < cargo)
				continue;

			for (int sourceNodeId = 0; sourceNodeId < m_Graph.GetNodeCount(); sourceNodeId++)
			{
				AICF_ObjectiveNode sourceNode = m_Graph.GetNode(sourceNodeId);
				SCR_CampaignMilitaryBaseComponent source = null;
				if (sourceNode)
					source = sourceNode.GetBase();
				if (!m_SupplyNetwork.IsSupplySource(source, faction) ||
					!m_SupplyNetwork.IsOperationalOwnedBase(source, faction))
					continue;
				if (source == destination)
					continue;
				float sourceSupplies = source.GetSupplies();
				if (sourceSupplies < m_Config.GetSourceReserveSupplies() + cargo)
					continue;
				int hops;
				if (!m_SupplyNetwork.TryGetOperationalPath(source, destination, faction, hops))
					continue;

				float destinationSupplies = destination.GetSupplies();
				bool better = !selectedDestination || destinationSupplies < bestDestinationSupplies;
				if (!better && destinationSupplies == bestDestinationSupplies)
					better = destinationNodeId < bestDestinationNode;
				if (!better && destination == selectedDestination && sourceSupplies > bestSourceSupplies)
					better = true;
				if (!better)
					continue;

				selectedSource = source;
				selectedDestination = destination;
				selectedHops = hops;
				bestDestinationSupplies = destinationSupplies;
				bestDestinationNode = destinationNodeId;
				bestSourceSupplies = sourceSupplies;
			}
		}

		return selectedSource && selectedDestination;
	}

	protected bool HasDestinationShipment(SCR_CampaignMilitaryBaseComponent destination)
	{
		foreach (AICF_SupplyShipment shipment : m_aShipments)
		{
			if (shipment && shipment.GetDestinationBase() == destination)
				return true;
		}
		return false;
	}

	protected bool CompleteArrival(AICF_SupplyShipment shipment, SCR_CampaignFaction faction)
	{
		SCR_CampaignMilitaryBaseComponent destination = shipment.GetDestinationBase();
		int cargo = shipment.GetCargoSupplies();
		int availableCapacity = destination.GetSuppliesMax() - destination.GetSupplies();
		if (availableCapacity < 0)
			availableCapacity = 0;
		int delivered = Math.Min(cargo, availableCapacity);
		int overflow = cargo - delivered;
		if (delivered > 0)
			destination.AddSupplies(delivered);
		RecordDelivered(shipment.GetFactionKey(), delivered);
		if (overflow > 0)
		{
			SCR_CampaignMilitaryBaseComponent returnBase = m_SupplyNetwork.FindReturnBase(faction);
			if (returnBase)
			{
				returnBase.AddSupplies(overflow);
				RecordReturned(shipment.GetFactionKey(), overflow);
			}
			else
			{
				shipment.SetCargoSupplies(overflow);
				shipment.MarkReturnPending();
				AICF_Stage4Diagnostics.Warning(
					"SHIPMENT_RETURN_PENDING",
					string.Format("shipment=%1 overflow=%2 reason=RETURN_BASE_UNAVAILABLE", shipment.GetShipmentId(), overflow));
				return false;
			}
		}
		shipment.MarkDelivered();
		AICF_Stage4Diagnostics.Info(
			"SHIPMENT_DELIVERED",
			string.Format(
				"shipment=%1 faction=%2 destination=%3 cargo=%4 delivered=%5 returned_overflow=%6",
				shipment.GetShipmentId(),
				shipment.GetFactionKey(),
				AICF_Stage1Diagnostics.BaseKey(destination),
				cargo,
				delivered,
				overflow));
		return true;
	}

	protected bool ReturnShipmentCargo(
		AICF_SupplyShipment shipment,
		SCR_CampaignFaction faction,
		string reason)
	{
		SCR_CampaignMilitaryBaseComponent returnBase = m_SupplyNetwork.FindReturnBase(faction);
		if (!returnBase)
			return false;

		returnBase.AddSupplies(shipment.GetCargoSupplies());
		RecordReturned(shipment.GetFactionKey(), shipment.GetCargoSupplies());
		shipment.MarkReturned();
		AICF_Stage4Diagnostics.Info(
			"SHIPMENT_RETURNED",
			string.Format(
				"shipment=%1 faction=%2 cargo=%3 return_base=%4 reason=%5",
				shipment.GetShipmentId(),
				shipment.GetFactionKey(),
				shipment.GetCargoSupplies(),
				AICF_Stage1Diagnostics.BaseKey(returnBase),
				reason));
		return true;
	}

	protected void RecordDispatched(FactionKey factionKey, int supplies)
	{
		if (factionKey == "US")
			m_iUSDispatchedSupplies += supplies;
		else
			m_iUSSRDispatchedSupplies += supplies;
	}

	protected void RecordDelivered(FactionKey factionKey, int supplies)
	{
		if (factionKey == "US")
			m_iUSDeliveredSupplies += supplies;
		else
			m_iUSSRDeliveredSupplies += supplies;
	}

	protected void RecordReturned(FactionKey factionKey, int supplies)
	{
		if (factionKey == "US")
			m_iUSReturnedSupplies += supplies;
		else
			m_iUSSRReturnedSupplies += supplies;
	}
}
