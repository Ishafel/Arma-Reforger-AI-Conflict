enum AICF_ETripRetirementObservation
{
	NONE = 0,
	PENDING,
	CONFIRMED,
	REJECTED
}

// Immutable явно переданная projection. AcceptanceMonitor не читает native world.
class AICF_TripRetirementObservation
{
	protected AICF_TripRetirementIdentity m_Identity;
	protected AICF_ETripRetirementObservation m_Status;
	protected string m_sOperation;
	protected FactionKey m_sFaction;
	protected int m_iSlot;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected string m_sDetails;

	void AICF_TripRetirementObservation(AICF_TripRetirementIdentity identity, AICF_TransportTrip trip)
	{
		if (!identity || !trip || !trip.GetAssignment() || !trip.OwnsRetirementIdentity(identity)) return;
		m_Identity = identity;
		m_Status = identity.ClassifyObservation(trip);
		m_sOperation = trip.GetOperationId();
		m_sFaction = trip.GetFactionKey();
		m_iSlot = trip.GetSlotId();
		m_iGroupGeneration = trip.GetGroupGeneration();
		m_iTripGeneration = trip.GetTripGeneration();
		m_sDetails = identity.EvidenceDetails() + string.Format(" classification=%1 observed_at_ms=%2", typename.EnumToString(AICF_ETripRetirementObservation, m_Status), System.GetTickCount());
	}

	AICF_ETripRetirementObservation GetStatus() { return m_Status; }
	bool IsConfirmed() { return m_Status == AICF_ETripRetirementObservation.CONFIRMED; }
	string Details() { return m_sDetails; }
	bool Matches(AICF_TransportTrip trip)
	{
		return trip && m_Identity && trip.OwnsRetirementIdentity(m_Identity) && trip.GetOperationId() == m_sOperation &&
			trip.MatchesIdentity(m_sFaction, m_iSlot, m_iGroupGeneration, m_iTripGeneration);
	}
	bool MatchesCapturedActor(EntityID id, string rpl) { return m_Identity && m_Identity.MatchesCapturedActor(id, rpl); }
}
