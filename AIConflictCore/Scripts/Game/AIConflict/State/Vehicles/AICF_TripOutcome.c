// Typed flow result. The optional retry timestamp is absolute and therefore
// cannot silently extend a phase deadline when a controller polls again.
class AICF_TripOutcome
{
	protected AICF_ETripOutcomeKind m_Kind;
	protected string m_sReason;
	protected string m_sCausationId;
	protected int m_iRetryAtMs;
	protected AIWaypoint m_WaypointForRemoval;

	protected void AICF_TripOutcome(
		AICF_ETripOutcomeKind kind,
		string reason,
		string causationId,
		int retryAtMs = 0,
		AIWaypoint waypointForRemoval = null)
	{
		m_Kind = kind;
		m_sReason = reason;
		m_sCausationId = causationId;
		m_iRetryAtMs = retryAtMs;
		m_WaypointForRemoval = waypointForRemoval;
	}

	AICF_ETripOutcomeKind GetKind() { return m_Kind; }
	string GetReason() { return m_sReason; }
	string GetCausationId() { return m_sCausationId; }
	int GetRetryAtMs() { return m_iRetryAtMs; }
	AIWaypoint GetWaypointForRemoval() { return m_WaypointForRemoval; }

	bool IsTerminal()
	{
		return m_Kind == AICF_ETripOutcomeKind.COMPLETE_TRIP ||
			m_Kind == AICF_ETripOutcomeKind.FALLBACK_TO_FOOT ||
			m_Kind == AICF_ETripOutcomeKind.TERMINAL_FAIL_CLOSED;
	}

	static AICF_TripOutcome Wait(string reason, string causationId)
	{
		return new AICF_TripOutcome(AICF_ETripOutcomeKind.WAIT, reason, causationId);
	}

	static AICF_TripOutcome Retry(string reason, string causationId, int retryAtMs)
	{
		return new AICF_TripOutcome(AICF_ETripOutcomeKind.RETRY, reason, causationId, retryAtMs);
	}

	static AICF_TripOutcome StartBoarding(string reason, string causationId)
	{
		return new AICF_TripOutcome(AICF_ETripOutcomeKind.START_BOARDING, reason, causationId);
	}

	static AICF_TripOutcome StartMovement(string reason, string causationId)
	{
		return new AICF_TripOutcome(AICF_ETripOutcomeKind.START_MOVEMENT, reason, causationId);
	}

	static AICF_TripOutcome StartDismount(string reason, string causationId)
	{
		return new AICF_TripOutcome(AICF_ETripOutcomeKind.START_DISMOUNT, reason, causationId);
	}

	static AICF_TripOutcome CompleteTrip(string reason, string causationId)
	{
		return new AICF_TripOutcome(AICF_ETripOutcomeKind.COMPLETE_TRIP, reason, causationId);
	}

	static AICF_TripOutcome FallbackToFoot(string reason, string causationId)
	{
		return new AICF_TripOutcome(AICF_ETripOutcomeKind.FALLBACK_TO_FOOT, reason, causationId);
	}

	static AICF_TripOutcome FallbackToFootWithWaypoint(
		string reason,
		string causationId,
		AIWaypoint waypointForRemoval)
	{
		return new AICF_TripOutcome(
			AICF_ETripOutcomeKind.FALLBACK_TO_FOOT,
			reason,
			causationId,
			0,
			waypointForRemoval);
	}

	static AICF_TripOutcome ReleaseLease(string reason, string causationId)
	{
		return new AICF_TripOutcome(AICF_ETripOutcomeKind.RELEASE_LEASE, reason, causationId);
	}

	static AICF_TripOutcome TerminalFailClosed(string reason, string causationId)
	{
		return new AICF_TripOutcome(AICF_ETripOutcomeKind.TERMINAL_FAIL_CLOSED, reason, causationId);
	}

	static AICF_TripOutcome TerminalFailClosedWithWaypoint(
		string reason,
		string causationId,
		AIWaypoint waypointForRemoval)
	{
		return new AICF_TripOutcome(
			AICF_ETripOutcomeKind.TERMINAL_FAIL_CLOSED,
			reason,
			causationId,
			0,
			waypointForRemoval);
	}
}
