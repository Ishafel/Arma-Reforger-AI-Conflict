// Authoritative ticket accounting. Callers must only create and mutate this ledger on the server.
class AICF_TicketLedger
{
	protected int m_iTickets;
	protected int m_iReplacementTicketCost;
	protected int m_iSpentTickets;
	protected int m_iReservedTickets;

	void AICF_TicketLedger(int initialTickets, int replacementTicketCost)
	{
		if (initialTickets < 0)
			initialTickets = 0;
		if (replacementTicketCost < 1)
			replacementTicketCost = 1;

		m_iTickets = initialTickets;
		m_iReplacementTicketCost = replacementTicketCost;
	}

	int GetTickets()
	{
		return m_iTickets;
	}

	int GetReplacementTicketCost()
	{
		return m_iReplacementTicketCost;
	}

	int GetSpentTickets()
	{
		return m_iSpentTickets;
	}

	int GetReservedTickets()
	{
		return m_iReservedTickets;
	}

	bool CanAffordDeployment(AICF_EDeploymentKind deploymentKind)
	{
		if (deploymentKind == AICF_EDeploymentKind.INITIAL)
			return true;

		return m_iTickets - m_iReservedTickets >= m_iReplacementTicketCost;
	}

	bool TryReserveDeployment(AICF_EDeploymentKind deploymentKind)
	{
		if (!Replication.IsServer())
			return false;
		if (deploymentKind == AICF_EDeploymentKind.INITIAL)
			return true;
		if (!CanAffordDeployment(deploymentKind))
			return false;

		m_iReservedTickets += m_iReplacementTicketCost;
		return true;
	}

	void ReleaseDeploymentReservation(AICF_EDeploymentKind deploymentKind)
	{
		if (!Replication.IsServer() || deploymentKind == AICF_EDeploymentKind.INITIAL)
			return;

		m_iReservedTickets -= m_iReplacementTicketCost;
		if (m_iReservedTickets < 0)
			m_iReservedTickets = 0;
	}

	bool TryCommitDeployment(AICF_EDeploymentKind deploymentKind)
	{
		if (!Replication.IsServer())
			return false;

		// The four initial groups are part of setup and never consume tickets.
		if (deploymentKind == AICF_EDeploymentKind.INITIAL)
			return true;

		if (m_iReservedTickets < m_iReplacementTicketCost)
			return false;

		m_iReservedTickets -= m_iReplacementTicketCost;
		m_iTickets -= m_iReplacementTicketCost;
		m_iSpentTickets += m_iReplacementTicketCost;
		return true;
	}
}
