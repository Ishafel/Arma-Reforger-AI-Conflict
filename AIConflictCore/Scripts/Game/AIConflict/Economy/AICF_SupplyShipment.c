// Server-only abstract cargo packet. No vehicle entity is created in Stage 4.
class AICF_SupplyShipment
{
	protected int m_iShipmentId;
	protected FactionKey m_sFactionKey;
	protected SCR_CampaignMilitaryBaseComponent m_SourceBase;
	protected SCR_CampaignMilitaryBaseComponent m_DestinationBase;
	protected int m_iCargoSupplies;
	protected int m_iHopCount;
	protected int m_iGraphRevision;
	protected int m_iRemainingTravelMs;
	protected int m_iLastUpdatedAtMs;
	protected AICF_ESupplyShipmentState m_State = AICF_ESupplyShipmentState.IN_TRANSIT;

	void AICF_SupplyShipment(
		int shipmentId,
		FactionKey factionKey,
		SCR_CampaignMilitaryBaseComponent sourceBase,
		SCR_CampaignMilitaryBaseComponent destinationBase,
		int cargoSupplies,
		int hopCount,
		int graphRevision,
		int travelMs)
	{
		m_iShipmentId = shipmentId;
		m_sFactionKey = factionKey;
		m_SourceBase = sourceBase;
		m_DestinationBase = destinationBase;
		m_iCargoSupplies = cargoSupplies;
		m_iHopCount = hopCount;
		m_iGraphRevision = graphRevision;
		m_iRemainingTravelMs = Math.Max(0, travelMs);
		m_iLastUpdatedAtMs = System.GetTickCount();
	}

	int GetShipmentId() { return m_iShipmentId; }
	FactionKey GetFactionKey() { return m_sFactionKey; }
	SCR_CampaignMilitaryBaseComponent GetSourceBase() { return m_SourceBase; }
	SCR_CampaignMilitaryBaseComponent GetDestinationBase() { return m_DestinationBase; }
	int GetCargoSupplies() { return m_iCargoSupplies; }
	int GetHopCount() { return m_iHopCount; }
	int GetGraphRevision() { return m_iGraphRevision; }
	int GetRemainingTravelMs() { return m_iRemainingTravelMs; }
	AICF_ESupplyShipmentState GetState() { return m_State; }

	void SetCargoSupplies(int supplies)
	{
		m_iCargoSupplies = Math.Max(0, supplies);
	}

	void UpdateRoute(int hopCount, int graphRevision)
	{
		m_iHopCount = hopCount;
		m_iGraphRevision = graphRevision;
	}

	bool Tick(bool routeAvailable)
	{
		int nowMs = System.GetTickCount();
		int elapsedMs = System.GetTickCount(m_iLastUpdatedAtMs);
		m_iLastUpdatedAtMs = nowMs;
		if (elapsedMs < 0)
			elapsedMs = 0;
		if (elapsedMs > 5000)
			elapsedMs = 5000;

		if (!routeAvailable)
		{
			m_State = AICF_ESupplyShipmentState.PAUSED_ROUTE;
			return false;
		}

		m_State = AICF_ESupplyShipmentState.IN_TRANSIT;
		m_iRemainingTravelMs -= elapsedMs;
		if (m_iRemainingTravelMs < 0)
			m_iRemainingTravelMs = 0;
		return m_iRemainingTravelMs == 0;
	}

	void MarkReturnPending()
	{
		m_State = AICF_ESupplyShipmentState.RETURN_PENDING;
		m_iLastUpdatedAtMs = System.GetTickCount();
	}

	void MarkDelivered()
	{
		m_State = AICF_ESupplyShipmentState.DELIVERED;
	}

	void MarkReturned()
	{
		m_State = AICF_ESupplyShipmentState.RETURNED;
	}
}
