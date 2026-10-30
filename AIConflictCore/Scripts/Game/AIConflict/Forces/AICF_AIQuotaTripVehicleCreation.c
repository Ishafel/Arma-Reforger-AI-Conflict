// All catalog candidates in one acquisition attempt share this pure receipt.
class AICF_AIQuotaTripFactoryAttempt
{
	AICF_VehicleRequestState m_Request;
	int m_iRequestGeneration;
	int m_iAttemptAtMs;
	int m_iAttemptBefore;
	int m_iTotalBefore;
	bool m_bCommitted;

	bool Commit()
	{
		if (!m_Request || m_Request.GetRequestGeneration() != m_iRequestGeneration) return false;
		if (m_bCommitted)
			return m_Request.GetAttemptCount() == m_iAttemptBefore + 1 && m_Request.GetTotalAttemptCount() == m_iTotalBefore + 1;
		if (m_Request.GetAttemptCount() != m_iAttemptBefore || m_Request.GetTotalAttemptCount() != m_iTotalBefore) return false;
		if (!m_Request.BeginAttempt(m_iAttemptAtMs)) return false;
		m_bCommitted = true;
		return true;
	}
}

class AICF_AIQuotaTripVehicleCreation : AICF_AIQuotaVehicleCreationGuard
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_VehicleRequestState m_Request;
	protected AICF_VehicleSpawnPlan m_Plan;
	protected int m_iRequestGeneration;
	protected int m_iTripGeneration;
	protected int m_iGroupGeneration;
	protected int m_iTransitions;
	protected int m_iDeadline;
	protected ref AICF_AIQuotaTripFactoryAttempt m_Attempt;

	static AICF_AIQuotaTripVehicleCreation Capture(AICF_TransportTrip trip, AICF_VehicleRequestState request)
	{
		if (!trip || !request || trip.GetRequestState() != request || !trip.GetAssignment()) return null;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		if (!group) return null;
		AICF_AIWorldQuotaService service = AICF_AIWorldQuotaService.GetAuthorityForWorld(group.GetWorld());
		if (!service) return null;
		AICF_AIQuotaTripVehicleCreation guard = new AICF_AIQuotaTripVehicleCreation();
		guard.m_Trip = trip;
		guard.m_Request = request;
		guard.m_Plan = request.GetSpawnPlan();
		guard.m_iRequestGeneration = request.GetRequestGeneration();
		guard.m_iTripGeneration = trip.GetTripGeneration();
		guard.m_iGroupGeneration = trip.GetGroupGeneration();
		guard.m_iTransitions = trip.GetTransitionCount();
		guard.m_iDeadline = request.GetAbsoluteDeadlineMs();
		if (!guard.Initialize(service, AICF_EAIQuotaBucket.FLEET, 1, trip.GetFactionKey(), group)) return null;
		return guard;
	}

	override bool IsDomainCurrent()
	{
		if (!m_Controller || !m_Trip || !m_Controller.IsAIQuotaVehicleTripCurrent(m_Trip)) return false;
		if (m_Trip.GetRequestState() != m_Request || !m_Request || m_Request.GetRequestGeneration() != m_iRequestGeneration) return false;
		if (m_Trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT || m_Trip.GetTransitionCount() != m_iTransitions) return false;
		if (m_Request.GetAbsoluteDeadlineMs() != m_iDeadline || m_iDeadline <= 0 || m_Request.IsDeadlineReached(System.GetTickCount())) return false;
		return m_Trip.GetTripGeneration() == m_iTripGeneration && m_Trip.GetGroupGeneration() == m_iGroupGeneration &&
			m_Trip.GetFactionKey() == m_sFaction && m_Request.GetSpawnPlan() == m_Plan && m_Plan && m_Plan.IsValid();
	}

	bool PrepareAttempt(int nowMs)
	{
		if (!IsCurrent() || !m_Request.CanAttempt(nowMs)) return false;
		m_Attempt = new AICF_AIQuotaTripFactoryAttempt();
		m_Attempt.m_Request = m_Request;
		m_Attempt.m_iRequestGeneration = m_iRequestGeneration;
		m_Attempt.m_iAttemptAtMs = nowMs;
		m_Attempt.m_iAttemptBefore = m_Request.GetAttemptCount();
		m_Attempt.m_iTotalBefore = m_Request.GetTotalAttemptCount();
		return true;
	}

	AICF_AIQuotaTripFactoryAttempt GetAttempt() { return m_Attempt; }
	void AdoptAttempt(AICF_AIQuotaTripFactoryAttempt attempt) { m_Attempt = attempt; }

	override bool CommitNativeAttempt()
	{
		// Project value reads only. All native/virtual domain reads happen in
		// MarkExternalNativeCallStarted before this synchronous final commit.
		return m_Attempt && m_Attempt.Commit();
	}
}
