// Temporary combined ticket/supply reservation for one exact spawn attempt.
class AICF_DeploymentReservation
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected int m_iRequestId;
	protected int m_iAttemptToken;
	protected int m_iSlotGeneration;
	protected int m_iGraphRevision;
	protected int m_iSupplyCost;
	protected float m_fSuppliesBefore;
	protected SCR_CampaignMilitaryBaseComponent m_Base;
	protected bool m_bTicketReserved;
	protected bool m_bSuppliesReserved;
	protected bool m_bCommitted;

	void AICF_DeploymentReservation(
		FactionKey factionKey,
		int slotId,
		int requestId,
		int attemptToken,
		int slotGeneration,
		int graphRevision,
		SCR_CampaignMilitaryBaseComponent base,
		int supplyCost,
		float suppliesBefore)
	{
		m_sFactionKey = factionKey;
		m_iSlotId = slotId;
		m_iRequestId = requestId;
		m_iAttemptToken = attemptToken;
		m_iSlotGeneration = slotGeneration;
		m_iGraphRevision = graphRevision;
		m_Base = base;
		m_iSupplyCost = supplyCost;
		m_fSuppliesBefore = suppliesBefore;
		m_bTicketReserved = true;
		m_bSuppliesReserved = true;
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	int GetRequestId() { return m_iRequestId; }
	int GetAttemptToken() { return m_iAttemptToken; }
	int GetSlotGeneration() { return m_iSlotGeneration; }
	int GetGraphRevision() { return m_iGraphRevision; }
	int GetSupplyCost() { return m_iSupplyCost; }
	float GetSuppliesBefore() { return m_fSuppliesBefore; }
	SCR_CampaignMilitaryBaseComponent GetBase() { return m_Base; }
	bool HasTicketReservation() { return m_bTicketReserved; }
	bool HasSupplyReservation() { return m_bSuppliesReserved; }
	bool IsCommitted() { return m_bCommitted; }

	void MarkCommitted()
	{
		m_bCommitted = true;
		m_bTicketReserved = false;
	}

	void ClearSupplyReservation()
	{
		m_bSuppliesReserved = false;
	}
}
