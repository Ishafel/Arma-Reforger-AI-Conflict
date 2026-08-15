// Immutable identity carried by every delayed action, retry and reservation.
// A stale/ABA callback can compare the whole fence and safely self-cancel.
class AICF_VehicleAsyncFence
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iFencedLeaseGeneration;
	protected int m_iFencedVehicleGeneration;
	protected EntityID m_EntityId = EntityID.INVALID;
	protected string m_sRplId;
	protected string m_sToken;

	void AICF_VehicleAsyncFence(
		FactionKey factionKey,
		int slotId,
		int groupGeneration,
		int tripGeneration,
		int leaseGeneration,
		int vehicleGeneration,
		EntityID entityId,
		string rplId,
		string token)
	{
		m_sFactionKey = factionKey;
		m_iSlotId = slotId;
		m_iGroupGeneration = groupGeneration;
		m_iTripGeneration = tripGeneration;
		m_iFencedLeaseGeneration = leaseGeneration;
		m_iFencedVehicleGeneration = vehicleGeneration;
		m_EntityId = entityId;
		m_sRplId = rplId;
		m_sToken = token;
	}

	string GetToken() { return m_sToken; }
	int GetLeaseGeneration() { return m_iFencedLeaseGeneration; }
	int GetVehicleGeneration() { return m_iFencedVehicleGeneration; }
	EntityID GetEntityId() { return m_EntityId; }
	string GetRplId() { return m_sRplId; }

	bool MatchesTrip(AICF_TransportTrip trip)
	{
		return trip && trip.MatchesIdentity(
			m_sFactionKey,
			m_iSlotId,
			m_iGroupGeneration,
			m_iTripGeneration);
	}

	bool MatchesLease(AICF_VehicleLease lease)
	{
		return lease && lease.MatchesIdentity(
			m_sFactionKey,
			m_iSlotId,
			m_iGroupGeneration,
			m_iFencedLeaseGeneration,
			m_iFencedVehicleGeneration,
			m_EntityId,
			m_sRplId);
	}
}

class AICF_VehicleRequestState
{
	protected int m_iRequestGeneration;
	protected int m_iStartedAtMs;
	protected int m_iAbsoluteDeadlineMs;
	protected int m_iNextAttemptAtMs;
	protected int m_iAttemptCount;
	protected int m_iTotalAttemptCount;
	protected int m_iMaximumAttempts;
	protected int m_iWaitingStartedAtMs;
	protected int m_iTotalWaitingStartedAtMs;
	protected int m_iContextResetCount;
	protected string m_sContextResetReason;
	protected string m_sLastFailureReason;
	protected int m_iCohesionStartedAtMs;
	protected float m_fCohesionSpreadMeters;
	protected bool m_bCohesionRecoveryAttempted;
	protected int m_iNoRangeStartedAtMs;
	protected int m_iNoRangeLastProgressAtMs;
	protected int m_iNoRangeProbeCount;
	protected vector m_vNoRangePreviousGroupPosition;
	protected bool m_bNoRangeHasPreviousGroupPosition;
	protected float m_fNoRangePreviousCandidateDistanceMeters = -1.0;
	protected float m_fNoRangeBestCandidateDistanceMeters = -1.0;
	protected float m_fNoRangeLastGroupMotionMeters;
	protected float m_fNoRangeLastCandidateDeltaMeters;
	protected string m_sNoRangeCandidateKey;
	protected string m_sNoRangeTrend;
	protected string m_sNoRangeCandidateTrace;

	void Reset()
	{
		m_iRequestGeneration = 0;
		m_iStartedAtMs = 0;
		m_iAbsoluteDeadlineMs = 0;
		m_iNextAttemptAtMs = 0;
		m_iAttemptCount = 0;
		m_iTotalAttemptCount = 0;
		m_iMaximumAttempts = 0;
		m_iWaitingStartedAtMs = 0;
		m_iTotalWaitingStartedAtMs = 0;
		m_iContextResetCount = 0;
		m_sContextResetReason = string.Empty;
		m_sLastFailureReason = string.Empty;
		m_iCohesionStartedAtMs = 0;
		m_fCohesionSpreadMeters = 0;
		m_bCohesionRecoveryAttempted = false;
		ClearNoRangeObservation();
	}

	void Begin(int nowMs, int absoluteDeadlineMs, int maximumAttempts = 4)
	{
		Reset();
		m_iRequestGeneration = 1;
		m_iStartedAtMs = nowMs;
		m_iTotalWaitingStartedAtMs = nowMs;
		m_iAbsoluteDeadlineMs = absoluteDeadlineMs;
		m_iMaximumAttempts = Math.Max(1, maximumAttempts);
	}

	int GetRequestGeneration() { return m_iRequestGeneration; }
	int GetStartedAtMs() { return m_iStartedAtMs; }
	int GetAbsoluteDeadlineMs() { return m_iAbsoluteDeadlineMs; }
	int GetNextAttemptAtMs() { return m_iNextAttemptAtMs; }
	int GetAttemptCount() { return m_iAttemptCount; }
	int GetTotalAttemptCount() { return m_iTotalAttemptCount; }
	int GetMaximumAttempts() { return m_iMaximumAttempts; }
	int GetWaitingStartedAtMs() { return m_iWaitingStartedAtMs; }
	int GetTotalWaitingStartedAtMs() { return m_iTotalWaitingStartedAtMs; }
	int GetContextResetCount() { return m_iContextResetCount; }
	string GetContextResetReason() { return m_sContextResetReason; }
	string GetLastFailureReason() { return m_sLastFailureReason; }
	int GetCohesionStartedAtMs() { return m_iCohesionStartedAtMs; }
	float GetCohesionSpreadMeters() { return m_fCohesionSpreadMeters; }
	bool WasCohesionRecoveryAttempted() { return m_bCohesionRecoveryAttempted; }
	int GetNoRangeStartedAtMs() { return m_iNoRangeStartedAtMs; }
	int GetNoRangeLastProgressAtMs() { return m_iNoRangeLastProgressAtMs; }
	int GetNoRangeProbeCount() { return m_iNoRangeProbeCount; }
	float GetNoRangeBestCandidateDistanceMeters() { return m_fNoRangeBestCandidateDistanceMeters; }
	float GetNoRangeLastGroupMotionMeters() { return m_fNoRangeLastGroupMotionMeters; }
	float GetNoRangeLastCandidateDeltaMeters() { return m_fNoRangeLastCandidateDeltaMeters; }
	string GetNoRangeCandidateKey() { return m_sNoRangeCandidateKey; }
	string GetNoRangeTrend() { return m_sNoRangeTrend; }
	string GetNoRangeCandidateTrace() { return m_sNoRangeCandidateTrace; }

	bool IsDeadlineReached(int nowMs)
	{
		return m_iAbsoluteDeadlineMs > 0 && nowMs >= m_iAbsoluteDeadlineMs;
	}

	bool CanAttempt(int nowMs)
	{
		return !IsDeadlineReached(nowMs) && m_iAttemptCount < m_iMaximumAttempts &&
			(m_iNextAttemptAtMs <= 0 || nowMs >= m_iNextAttemptAtMs);
	}

	bool BeginAttempt(int nowMs)
	{
		if (!CanAttempt(nowMs))
			return false;
		m_iAttemptCount++;
		m_iTotalAttemptCount++;
		m_iNextAttemptAtMs = 0;
		return true;
	}

	void ScheduleRetry(int nextAttemptAtMs, string reason)
	{
		m_sLastFailureReason = reason;
		m_iNextAttemptAtMs = nextAttemptAtMs;
		if (m_iAbsoluteDeadlineMs > 0 && m_iNextAttemptAtMs > m_iAbsoluteDeadlineMs)
			m_iNextAttemptAtMs = m_iAbsoluteDeadlineMs;
	}

	void EnterWaitingForSite(int nowMs, int nextProbeAtMs, string reason)
	{
		if (m_iWaitingStartedAtMs <= 0)
			m_iWaitingStartedAtMs = nowMs;
		ScheduleRetry(nextProbeAtMs, reason);
	}

	void ExitWaitingForSite()
	{
		m_iWaitingStartedAtMs = 0;
	}

	void RecordContextReset(int nowMs, string reason)
	{
		m_iRequestGeneration++;
		m_iContextResetCount++;
		m_sContextResetReason = reason;
		m_iStartedAtMs = nowMs;
		m_iAttemptCount = 0;
		m_iNextAttemptAtMs = 0;
		m_iWaitingStartedAtMs = 0;
		m_sLastFailureReason = string.Empty;
		ClearNoRangeObservation();
	}

	int ObserveCohesion(bool fragmented, float spreadMeters, int nowMs)
	{
		m_fCohesionSpreadMeters = spreadMeters;
		if (!fragmented)
		{
			m_iCohesionStartedAtMs = 0;
			m_bCohesionRecoveryAttempted = false;
			return 0;
		}
		if (m_iCohesionStartedAtMs <= 0)
			m_iCohesionStartedAtMs = nowMs;
		return nowMs - m_iCohesionStartedAtMs;
	}

	void MarkCohesionRecoveryAttempted()
	{
		m_bCohesionRecoveryAttempted = true;
	}

	int ObserveNoRangeProbe(
		int nowMs,
		vector groupPosition,
		string candidateKey,
		float candidateDistanceMeters,
		string candidateTrace,
		float progressEpsilonMeters)
	{
		m_iNoRangeProbeCount++;
		m_fNoRangeLastGroupMotionMeters = 0;
		m_fNoRangeLastCandidateDeltaMeters = 0;
		m_sNoRangeTrend = "FIRST_PROBE";
		if (m_iNoRangeStartedAtMs <= 0)
		{
			m_iNoRangeStartedAtMs = nowMs;
			m_iNoRangeLastProgressAtMs = nowMs;
		}
		if (m_bNoRangeHasPreviousGroupPosition)
		{
			m_fNoRangeLastGroupMotionMeters = Math.Sqrt(vector.DistanceSqXZ(
				groupPosition,
				m_vNoRangePreviousGroupPosition));
		}

		if (candidateDistanceMeters < 0)
		{
			m_sNoRangeTrend = "NO_CANDIDATE";
		}
		else if (m_fNoRangePreviousCandidateDistanceMeters >= 0)
		{
			m_fNoRangeLastCandidateDeltaMeters =
				m_fNoRangePreviousCandidateDistanceMeters - candidateDistanceMeters;
			if (m_fNoRangeLastCandidateDeltaMeters > progressEpsilonMeters)
			{
				m_sNoRangeTrend = "APPROACHING";
				m_iNoRangeLastProgressAtMs = nowMs;
			}
			else if (m_fNoRangeLastCandidateDeltaMeters < -progressEpsilonMeters)
			{
				m_sNoRangeTrend = "RETREATING";
			}
			else
			{
				m_sNoRangeTrend = "STABLE";
			}
		}

		if (candidateDistanceMeters >= 0 &&
			(m_fNoRangeBestCandidateDistanceMeters < 0 ||
			candidateDistanceMeters < m_fNoRangeBestCandidateDistanceMeters))
		{
			m_fNoRangeBestCandidateDistanceMeters = candidateDistanceMeters;
		}
		m_vNoRangePreviousGroupPosition = groupPosition;
		m_bNoRangeHasPreviousGroupPosition = true;
		m_fNoRangePreviousCandidateDistanceMeters = candidateDistanceMeters;
		m_sNoRangeCandidateKey = candidateKey;
		m_sNoRangeCandidateTrace = candidateTrace;
		return Math.Max(0, nowMs - m_iNoRangeLastProgressAtMs);
	}

	void ClearNoRangeObservation()
	{
		m_iNoRangeStartedAtMs = 0;
		m_iNoRangeLastProgressAtMs = 0;
		m_iNoRangeProbeCount = 0;
		m_vNoRangePreviousGroupPosition = vector.Zero;
		m_bNoRangeHasPreviousGroupPosition = false;
		m_fNoRangePreviousCandidateDistanceMeters = -1.0;
		m_fNoRangeBestCandidateDistanceMeters = -1.0;
		m_fNoRangeLastGroupMotionMeters = 0;
		m_fNoRangeLastCandidateDeltaMeters = 0;
		m_sNoRangeCandidateKey = string.Empty;
		m_sNoRangeTrend = string.Empty;
		m_sNoRangeCandidateTrace = string.Empty;
	}
}

class AICF_VehicleBoardingState
{
	protected int m_iStartedAtMs;
	protected int m_iAbsoluteDeadlineMs;
	protected int m_iPhaseStartedAtMs;
	protected int m_iPhaseDeadlineMs;
	protected int m_iPhaseTimeoutMs;
	protected int m_iPlannedPhaseCount;
	protected int m_iCurrentPhaseIndex;
	protected AICF_EVehicleBoardingPhase m_Phase;
	protected int m_iSettledPollCount;
	protected int m_iStagingPollCount;
	protected int m_iAliveCount;
	protected int m_iMountedCount;
	protected int m_iRetryCount;
	protected int m_iMaximumRetries;
	protected bool m_bDriverPhasePlanned;
	protected bool m_bGunnerPhasePlanned;
	protected int m_iMaxLinkedCount;
	protected int m_iMaxCompartmentCount;
	protected int m_iMaxGettingInCount;
	protected int m_iMaxCharacterVehicleCount;
	protected int m_iMaxSettledCount;
	protected float m_fBestFarthestDistanceMeters = -1.0;
	protected int m_iLastProgressAtMs;
	protected int m_iLastOwnershipAuditAtMs;
	protected bool m_bGraceEvaluated;
	protected bool m_bGraceGranted;
	protected int m_iGraceDeadlineMs;
	protected bool m_bRoleResetAttempted;
	protected bool m_bRoleRetryIssued;
	protected int m_iRoleResetStartedAtMs;
	protected AICF_EVehicleBoardingPhase m_RoleResetNextPhase;
	protected bool m_bExitEffectsApplied;
	protected ref array<ref AICF_VehicleAsyncFence> m_aActionFences = {};
	protected ref AICF_VehicleBoardingTokenSet m_Tokens;

	void Reset()
	{
		if (m_Tokens)
			m_Tokens.CancelAllOwnerSafe();
		m_iStartedAtMs = 0;
		m_iAbsoluteDeadlineMs = 0;
		m_iPhaseStartedAtMs = 0;
		m_iPhaseDeadlineMs = 0;
		m_iPhaseTimeoutMs = 0;
		m_iPlannedPhaseCount = 0;
		m_iCurrentPhaseIndex = 0;
		m_Phase = AICF_EVehicleBoardingPhase.NONE;
		m_iSettledPollCount = 0;
		m_iStagingPollCount = 0;
		m_iAliveCount = 0;
		m_iMountedCount = 0;
		m_iRetryCount = 0;
		m_iMaximumRetries = 0;
		m_bDriverPhasePlanned = false;
		m_bGunnerPhasePlanned = false;
		m_iMaxLinkedCount = 0;
		m_iMaxCompartmentCount = 0;
		m_iMaxGettingInCount = 0;
		m_iMaxCharacterVehicleCount = 0;
		m_iMaxSettledCount = 0;
		m_fBestFarthestDistanceMeters = -1.0;
		m_iLastProgressAtMs = 0;
		m_iLastOwnershipAuditAtMs = 0;
		m_bGraceEvaluated = false;
		m_bGraceGranted = false;
		m_iGraceDeadlineMs = 0;
		m_bRoleResetAttempted = false;
		m_bRoleRetryIssued = false;
		m_iRoleResetStartedAtMs = 0;
		m_RoleResetNextPhase = AICF_EVehicleBoardingPhase.NONE;
		m_bExitEffectsApplied = false;
		m_aActionFences.Clear();
		m_Tokens = null;
	}

	void Begin(int nowMs, int absoluteDeadlineMs, int plannedPhaseCount, int maximumRetries)
	{
		Reset();
		m_iStartedAtMs = nowMs;
		m_iAbsoluteDeadlineMs = absoluteDeadlineMs;
		m_iPlannedPhaseCount = Math.Max(1, plannedPhaseCount);
		m_iMaximumRetries = Math.Max(0, maximumRetries);
		m_iLastProgressAtMs = nowMs;
		m_Tokens = new AICF_VehicleBoardingTokenSet();
	}

	int GetStartedAtMs() { return m_iStartedAtMs; }
	int GetAbsoluteDeadlineMs() { return m_iAbsoluteDeadlineMs; }
	int GetPhaseStartedAtMs() { return m_iPhaseStartedAtMs; }
	int GetPhaseDeadlineMs() { return m_iPhaseDeadlineMs; }
	int GetPhaseTimeoutMs() { return m_iPhaseTimeoutMs; }
	int GetPlannedPhaseCount() { return m_iPlannedPhaseCount; }
	int GetCurrentPhaseIndex() { return m_iCurrentPhaseIndex; }
	AICF_EVehicleBoardingPhase GetPhase() { return m_Phase; }
	int GetSettledPollCount() { return m_iSettledPollCount; }
	int GetAliveCount() { return m_iAliveCount; }
	int GetMountedCount() { return m_iMountedCount; }
	int GetRetryCount() { return m_iRetryCount; }
	int GetActionFenceCount() { return m_aActionFences.Count(); }
	bool IsDriverPhasePlanned() { return m_bDriverPhasePlanned; }
	bool IsGunnerPhasePlanned() { return m_bGunnerPhasePlanned; }
	int GetMaxLinkedCount() { return m_iMaxLinkedCount; }
	int GetMaxCompartmentCount() { return m_iMaxCompartmentCount; }
	int GetMaxGettingInCount() { return m_iMaxGettingInCount; }
	int GetMaxCharacterVehicleCount() { return m_iMaxCharacterVehicleCount; }
	int GetMaxSettledCount() { return m_iMaxSettledCount; }
	float GetBestFarthestDistanceMeters() { return m_fBestFarthestDistanceMeters; }
	bool IsGraceEvaluated() { return m_bGraceEvaluated; }
	bool IsGraceGranted() { return m_bGraceGranted; }
	int GetGraceDeadlineMs() { return m_iGraceDeadlineMs; }
	bool IsRoleResetAttempted() { return m_bRoleResetAttempted; }
	bool IsRoleRetryIssued() { return m_bRoleRetryIssued; }
	AICF_EVehicleBoardingPhase GetRoleResetNextPhase() { return m_RoleResetNextPhase; }
	AICF_VehicleBoardingTokenSet GetTokens() { return m_Tokens; }
	bool AreExitEffectsApplied() { return m_bExitEffectsApplied; }

	void ConfigureImmutablePlan(
		int phaseTimeoutMs,
		bool driverPhasePlanned,
		bool gunnerPhasePlanned)
	{
		if (m_iPhaseTimeoutMs > 0)
			return;
		m_iPhaseTimeoutMs = Math.Max(1000, phaseTimeoutMs);
		m_bDriverPhasePlanned = driverPhasePlanned;
		m_bGunnerPhasePlanned = gunnerPhasePlanned;
	}

	void BeginPhase(AICF_EVehicleBoardingPhase phase, int phaseIndex, int nowMs)
	{
		if (m_Tokens)
			m_Tokens.CancelAllOwnerSafe();
		m_aActionFences.Clear();
		m_Phase = phase;
		m_iCurrentPhaseIndex = Math.Max(0, phaseIndex);
		m_iPhaseStartedAtMs = nowMs;
		m_iPhaseDeadlineMs = nowMs + m_iPhaseTimeoutMs;
		if (m_iAbsoluteDeadlineMs > 0 && m_iPhaseDeadlineMs > m_iAbsoluteDeadlineMs)
			m_iPhaseDeadlineMs = m_iAbsoluteDeadlineMs;
		m_iSettledPollCount = 0;
		m_iStagingPollCount = 0;
	}

	void SetCurrentPhaseIndex(int phaseIndex)
	{
		m_iCurrentPhaseIndex = Math.Max(0, phaseIndex);
		m_iSettledPollCount = 0;
	}

	int ObserveStaging(bool staged)
	{
		if (!staged)
		{
			m_iStagingPollCount = 0;
			return 0;
		}
		m_iStagingPollCount++;
		return m_iStagingPollCount;
	}

	bool ObserveProgress(
		int linkedCount,
		int compartmentCount,
		int gettingInCount,
		int characterVehicleCount,
		int settledCount,
		float farthestDistanceMeters,
		int nowMs)
	{
		bool advanced;
		if (linkedCount > m_iMaxLinkedCount)
		{
			m_iMaxLinkedCount = linkedCount;
			advanced = true;
		}
		if (compartmentCount > m_iMaxCompartmentCount)
		{
			m_iMaxCompartmentCount = compartmentCount;
			advanced = true;
		}
		if (gettingInCount > m_iMaxGettingInCount)
		{
			m_iMaxGettingInCount = gettingInCount;
			advanced = true;
		}
		if (characterVehicleCount > m_iMaxCharacterVehicleCount)
		{
			m_iMaxCharacterVehicleCount = characterVehicleCount;
			advanced = true;
		}
		if (settledCount > m_iMaxSettledCount)
		{
			m_iMaxSettledCount = settledCount;
			advanced = true;
		}
		if (farthestDistanceMeters >= 0 &&
			(m_fBestFarthestDistanceMeters < 0 || farthestDistanceMeters < m_fBestFarthestDistanceMeters - 2.0))
		{
			m_fBestFarthestDistanceMeters = farthestDistanceMeters;
			advanced = true;
		}
		if (advanced)
			m_iLastProgressAtMs = nowMs;
		return advanced;
	}

	bool HasRecentProgress(int nowMs, int freshnessMs)
	{
		return m_iLastProgressAtMs > 0 && nowMs - m_iLastProgressAtMs <= freshnessMs;
	}

	bool MarkOwnershipAuditDue(int nowMs, int intervalMs)
	{
		if (m_iLastOwnershipAuditAtMs > 0 && nowMs - m_iLastOwnershipAuditAtMs < intervalMs)
			return false;
		m_iLastOwnershipAuditAtMs = nowMs;
		return true;
	}

	bool IsSoftDeadlineReached(int nowMs)
	{
		return (m_iPhaseDeadlineMs > 0 && nowMs >= m_iPhaseDeadlineMs) ||
			(m_iAbsoluteDeadlineMs > 0 && nowMs >= m_iAbsoluteDeadlineMs);
	}

	bool EvaluateOneProgressGrace(bool eligible, int nowMs, int graceMs)
	{
		if (m_bGraceEvaluated)
			return m_bGraceGranted;
		m_bGraceEvaluated = true;
		m_bGraceGranted = eligible;
		if (eligible)
			m_iGraceDeadlineMs = nowMs + Math.Max(0, graceMs);
		return m_bGraceGranted;
	}

	bool CanWaitInGrace(int nowMs)
	{
		return m_bGraceGranted && m_iGraceDeadlineMs > 0 && nowMs < m_iGraceDeadlineMs;
	}

	void BeginRoleReset(int nowMs, AICF_EVehicleBoardingPhase nextPhase)
	{
		m_bRoleResetAttempted = true;
		m_iRoleResetStartedAtMs = nowMs;
		m_RoleResetNextPhase = nextPhase;
	}

	int GetRoleResetAgeMs(int nowMs)
	{
		if (m_iRoleResetStartedAtMs <= 0)
			return 0;
		return nowMs - m_iRoleResetStartedAtMs;
	}

	void MarkRoleRetryIssued()
	{
		m_bRoleRetryIssued = true;
	}

	bool ApplyExitEffectsOwnerSafe()
	{
		if (m_bExitEffectsApplied)
			return false;
		m_bExitEffectsApplied = true;
		if (m_Tokens)
			m_Tokens.CancelAllOwnerSafe();
		m_aActionFences.Clear();
		return true;
	}

	bool RecordRetry()
	{
		if (m_iRetryCount >= m_iMaximumRetries)
			return false;
		m_iRetryCount++;
		return true;
	}

	int ObserveSettled(int aliveCount, int mountedCount, bool transitionsClear)
	{
		m_iAliveCount = aliveCount;
		m_iMountedCount = mountedCount;
		if (!transitionsClear || aliveCount <= 0 || mountedCount != aliveCount)
		{
			m_iSettledPollCount = 0;
			return 0;
		}
		m_iSettledPollCount++;
		return m_iSettledPollCount;
	}

	bool TrackActionFence(AICF_VehicleAsyncFence fence, int maximumTrackedActions)
	{
		if (!fence || maximumTrackedActions < 1 || m_aActionFences.Count() >= maximumTrackedActions)
			return false;
		m_aActionFences.Insert(fence);
		return true;
	}

	AICF_VehicleAsyncFence GetActionFence(int index)
	{
		if (!m_aActionFences.IsIndexValid(index))
			return null;
		return m_aActionFences[index];
	}

	void RemoveActionFence(AICF_VehicleAsyncFence expected)
	{
		if (expected)
			m_aActionFences.RemoveItem(expected);
	}
}

class AICF_VehicleMovementState
{
	protected int m_iStartedAtMs;
	protected int m_iAbsoluteDeadlineMs;
	protected int m_iLastRouteProgressAtMs;
	protected int m_iLastPhysicalMotionAtMs;
	protected int m_iLastMotionReportAtMs;
	protected float m_fBestRouteDistanceMeters = -1.0;
	protected vector m_vLastPhysicalPosition;
	protected bool m_bHasPhysicalSample;
	protected int m_iCrewRecoveryAttempts;
	protected int m_iMaximumCrewRecoveryAttempts;
	protected int m_iMobilityRecoveryAttempts;
	protected int m_iMaximumMobilityRecoveryAttempts;
	protected bool m_bRecoveryEvidencePending;
	protected bool m_bRecoveryRequiresRouteProgress;
	protected bool m_bRecoveryPhysicalEvidence;
	protected bool m_bRecoveryRouteEvidence;
	protected bool m_bRecoveryMobilityRestoredReported;
	protected bool m_bPendingUnstuckRelocated;
	protected int m_iLastMobilityRecoveryDeferredAtMs;
	protected int m_iRecoveryEvidenceArmedAtMs;
	protected string m_sPendingRecoveryReason;
	protected AIWaypoint m_RouteWaypoint;
	protected AIWaypoint m_SupersededRouteWaypoint;
	protected bool m_bRouteWaypointBound;
	protected int m_iRouteGeneration;
	protected int m_iAssignmentRevision = -1;
	protected vector m_vTacticalTarget;
	protected vector m_vRouteEndpoint;
	protected string m_sRouteMode;
	protected IEntity m_LastDriver;
	protected IEntity m_LastGunner;
	protected IEntity m_DriverSettledLossOccupant;
	protected int m_iDriverSettledLossStartedAtMs;
	protected int m_iDriverSettledLossPolls;
	protected string m_sDriverSettledLossPredicateSnapshot;
	protected IEntity m_GunnerSettledLossOccupant;
	protected int m_iGunnerSettledLossStartedAtMs;
	protected int m_iGunnerSettledLossPolls;
	protected string m_sGunnerSettledLossPredicateSnapshot;
	protected bool m_bCrewRecoveryRoutePending;
	protected ref AICF_VehicleCrewRecoveryToken m_CrewRecoveryToken;

	void Reset()
	{
		if (m_CrewRecoveryToken)
			m_CrewRecoveryToken.CancelOwnerSafe();
		m_iStartedAtMs = 0;
		m_iAbsoluteDeadlineMs = 0;
		m_iLastRouteProgressAtMs = 0;
		m_iLastPhysicalMotionAtMs = 0;
		m_iLastMotionReportAtMs = 0;
		m_fBestRouteDistanceMeters = -1.0;
		m_vLastPhysicalPosition = vector.Zero;
		m_bHasPhysicalSample = false;
		m_iCrewRecoveryAttempts = 0;
		m_iMaximumCrewRecoveryAttempts = 0;
		m_iMobilityRecoveryAttempts = 0;
		m_iMaximumMobilityRecoveryAttempts = 0;
		m_bRecoveryEvidencePending = false;
		m_bRecoveryRequiresRouteProgress = false;
		m_bRecoveryPhysicalEvidence = false;
		m_bRecoveryRouteEvidence = false;
		m_bRecoveryMobilityRestoredReported = false;
		m_bPendingUnstuckRelocated = false;
		m_iLastMobilityRecoveryDeferredAtMs = 0;
		m_iRecoveryEvidenceArmedAtMs = 0;
		m_sPendingRecoveryReason = string.Empty;
		m_RouteWaypoint = null;
		m_SupersededRouteWaypoint = null;
		m_bRouteWaypointBound = false;
		m_iRouteGeneration = 0;
		m_iAssignmentRevision = -1;
		m_vTacticalTarget = vector.Zero;
		m_vRouteEndpoint = vector.Zero;
		m_sRouteMode = string.Empty;
		m_LastDriver = null;
		m_LastGunner = null;
		m_DriverSettledLossOccupant = null;
		m_iDriverSettledLossStartedAtMs = 0;
		m_iDriverSettledLossPolls = 0;
		m_sDriverSettledLossPredicateSnapshot = string.Empty;
		m_GunnerSettledLossOccupant = null;
		m_iGunnerSettledLossStartedAtMs = 0;
		m_iGunnerSettledLossPolls = 0;
		m_sGunnerSettledLossPredicateSnapshot = string.Empty;
		m_bCrewRecoveryRoutePending = false;
		m_CrewRecoveryToken = null;
	}

	void Begin(int nowMs, int absoluteDeadlineMs, int maximumCrewAttempts, int maximumMobilityAttempts)
	{
		Reset();
		m_iStartedAtMs = nowMs;
		m_iAbsoluteDeadlineMs = absoluteDeadlineMs;
		m_iLastRouteProgressAtMs = nowMs;
		m_iLastPhysicalMotionAtMs = nowMs;
		m_iMaximumCrewRecoveryAttempts = Math.Max(0, maximumCrewAttempts);
		m_iMaximumMobilityRecoveryAttempts = Math.Max(0, maximumMobilityAttempts);
	}

	int GetStartedAtMs() { return m_iStartedAtMs; }
	int GetAbsoluteDeadlineMs() { return m_iAbsoluteDeadlineMs; }
	int GetLastRouteProgressAtMs() { return m_iLastRouteProgressAtMs; }
	int GetLastPhysicalMotionAtMs() { return m_iLastPhysicalMotionAtMs; }
	int GetLastMotionReportAtMs() { return m_iLastMotionReportAtMs; }
	int GetCrewRecoveryAttempts() { return m_iCrewRecoveryAttempts; }
	int GetMobilityRecoveryAttempts() { return m_iMobilityRecoveryAttempts; }
	int GetMaximumCrewRecoveryAttempts() { return m_iMaximumCrewRecoveryAttempts; }
	int GetMaximumMobilityRecoveryAttempts() { return m_iMaximumMobilityRecoveryAttempts; }
	bool IsRecoveryEvidencePending() { return m_bRecoveryEvidencePending; }
	bool RecoveryRequiresRouteProgress() { return m_bRecoveryRequiresRouteProgress; }
	bool HasRecoveryPhysicalEvidence() { return m_bRecoveryPhysicalEvidence; }
	bool HasRecoveryRouteEvidence() { return m_bRecoveryRouteEvidence; }
	bool HasReportedRecoveryMobilityRestored()
	{
		return m_bRecoveryMobilityRestoredReported;
	}
	bool WasPendingUnstuckRelocated() { return m_bPendingUnstuckRelocated; }
	int GetRecoveryEvidenceArmedAtMs() { return m_iRecoveryEvidenceArmedAtMs; }
	string GetPendingRecoveryReason() { return m_sPendingRecoveryReason; }
	AIWaypoint GetRouteWaypoint() { return m_RouteWaypoint; }
	AIWaypoint GetSupersededRouteWaypoint() { return m_SupersededRouteWaypoint; }
	bool IsRouteWaypointBound() { return m_bRouteWaypointBound; }
	int GetRouteGeneration() { return m_iRouteGeneration; }
	int GetAssignmentRevision() { return m_iAssignmentRevision; }
	vector GetTacticalTarget() { return m_vTacticalTarget; }
	vector GetRouteEndpoint() { return m_vRouteEndpoint; }
	string GetRouteMode() { return m_sRouteMode; }
	float GetBestRouteDistanceMeters() { return m_fBestRouteDistanceMeters; }
	IEntity GetLastDriver() { return m_LastDriver; }
	IEntity GetLastGunner() { return m_LastGunner; }
	bool IsCrewRecoveryRoutePending() { return m_bCrewRecoveryRoutePending; }
	AICF_VehicleCrewRecoveryToken GetCrewRecoveryToken() { return m_CrewRecoveryToken; }

	bool StageRouteWaypoint(
		AIWaypoint waypoint,
		vector tacticalTarget,
		vector routeEndpoint,
		string routeMode,
		int assignmentRevision,
		vector vehiclePosition,
		int nowMs)
	{
		if (!waypoint || routeMode.IsEmpty() || assignmentRevision < 0 ||
			(m_SupersededRouteWaypoint && m_RouteWaypoint))
			return false;
		if (m_RouteWaypoint && m_RouteWaypoint != waypoint)
			m_SupersededRouteWaypoint = m_RouteWaypoint;
		m_RouteWaypoint = waypoint;
		m_bRouteWaypointBound = false;
		m_iRouteGeneration++;
		m_iAssignmentRevision = assignmentRevision;
		m_vTacticalTarget = tacticalTarget;
		m_vRouteEndpoint = routeEndpoint;
		m_sRouteMode = routeMode;
		m_fBestRouteDistanceMeters = vector.DistanceXZ(vehiclePosition, routeEndpoint);
		m_iLastRouteProgressAtMs = nowMs;
		m_vLastPhysicalPosition = vehiclePosition;
		m_bHasPhysicalSample = true;
		m_iLastPhysicalMotionAtMs = nowMs;
		return true;
	}

	bool ConfirmRouteWaypointBound(AIWaypoint expected, bool bound)
	{
		if (!expected || expected != m_RouteWaypoint)
			return false;
		m_bRouteWaypointBound = bound;
		return bound;
	}

	bool ConfirmSupersededRouteWaypointRemoved(AIWaypoint expected)
	{
		if (!expected || expected != m_SupersededRouteWaypoint)
			return false;
		m_SupersededRouteWaypoint = null;
		return true;
	}

	bool ClearRouteWaypoint(AIWaypoint expected)
	{
		if (!expected || expected != m_RouteWaypoint)
			return false;
		m_RouteWaypoint = null;
		m_bRouteWaypointBound = false;
		return true;
	}

	bool SuspendRouteWaypoint()
	{
		if (!m_RouteWaypoint || m_SupersededRouteWaypoint)
			return false;
		m_SupersededRouteWaypoint = m_RouteWaypoint;
		m_RouteWaypoint = null;
		m_bRouteWaypointBound = false;
		return true;
	}

	bool ObserveRouteProgress(float distanceMeters, float minimumProgressMeters, int nowMs)
	{
		if (distanceMeters < 0)
			return false;
		if (m_fBestRouteDistanceMeters >= 0 &&
			distanceMeters > m_fBestRouteDistanceMeters - minimumProgressMeters)
			return false;
		m_fBestRouteDistanceMeters = distanceMeters;
		m_iLastRouteProgressAtMs = nowMs;
		if (m_bRecoveryEvidencePending)
			m_bRecoveryRouteEvidence = true;
		return true;
	}

	bool ObservePhysicalMotion(vector position, float minimumMovementMeters, int nowMs)
	{
		if (!m_bHasPhysicalSample)
		{
			m_vLastPhysicalPosition = position;
			m_bHasPhysicalSample = true;
			return false;
		}
		if (vector.DistanceSqXZ(position, m_vLastPhysicalPosition) <
			minimumMovementMeters * minimumMovementMeters)
			return false;
		m_vLastPhysicalPosition = position;
		m_iLastPhysicalMotionAtMs = nowMs;
		if (m_bRecoveryEvidencePending)
			m_bRecoveryPhysicalEvidence = true;
		return true;
	}

	bool MarkMotionReportDue(int nowMs, int intervalMs)
	{
		if (m_iLastMotionReportAtMs > 0 && nowMs - m_iLastMotionReportAtMs < intervalMs)
			return false;
		m_iLastMotionReportAtMs = nowMs;
		return true;
	}

	bool BeginCrewRecovery()
	{
		if (m_iCrewRecoveryAttempts >= m_iMaximumCrewRecoveryAttempts)
			return false;
		m_iCrewRecoveryAttempts++;
		return true;
	}

	bool BeginMobilityRecovery()
	{
		if (m_iMobilityRecoveryAttempts >= m_iMaximumMobilityRecoveryAttempts)
			return false;
		m_iMobilityRecoveryAttempts++;
		return true;
	}

	bool RollbackUncommittedMobilityRecovery()
	{
		if (m_iMobilityRecoveryAttempts <= 0 || m_bRecoveryEvidencePending)
			return false;
		m_iMobilityRecoveryAttempts--;
		return true;
	}

	bool MarkMobilityRecoveryDeferredDue(int nowMs, int intervalMs)
	{
		if (m_iLastMobilityRecoveryDeferredAtMs > 0 &&
			nowMs - m_iLastMobilityRecoveryDeferredAtMs < intervalMs)
		{
			return false;
		}
		m_iLastMobilityRecoveryDeferredAtMs = nowMs;
		return true;
	}

	void TrackCrewRecoveryToken(AICF_VehicleCrewRecoveryToken token)
	{
		m_CrewRecoveryToken = token;
	}

	bool ClearCrewRecoveryToken(AICF_VehicleCrewRecoveryToken expected)
	{
		if (!expected || expected != m_CrewRecoveryToken)
			return false;
		m_CrewRecoveryToken = null;
		return true;
	}

	void SetLastDriver(IEntity driver) { m_LastDriver = driver; }
	void SetLastGunner(IEntity gunner) { m_LastGunner = gunner; }

	int ObserveCrewRoleSettledLoss(
		EAICompartmentType role,
		IEntity occupant,
		string predicateSnapshot,
		int nowMs,
		out int ageMs,
		out bool episodeStarted,
		out bool predicateChanged)
	{
		ageMs = 0;
		episodeStarted = false;
		predicateChanged = false;
		if (!occupant)
			return 0;
		if (role == EAICompartmentType.Pilot)
		{
			episodeStarted = m_DriverSettledLossOccupant != occupant ||
				m_iDriverSettledLossStartedAtMs <= 0;
			predicateChanged = episodeStarted ||
				m_sDriverSettledLossPredicateSnapshot != predicateSnapshot;
			if (episodeStarted)
			{
				m_DriverSettledLossOccupant = occupant;
				m_iDriverSettledLossStartedAtMs = nowMs;
				m_iDriverSettledLossPolls = 0;
			}
			m_iDriverSettledLossPolls++;
			m_sDriverSettledLossPredicateSnapshot = predicateSnapshot;
			ageMs = Math.Max(0, nowMs - m_iDriverSettledLossStartedAtMs);
			return m_iDriverSettledLossPolls;
		}
		if (role != EAICompartmentType.Turret)
			return 0;
		episodeStarted = m_GunnerSettledLossOccupant != occupant ||
			m_iGunnerSettledLossStartedAtMs <= 0;
		predicateChanged = episodeStarted ||
			m_sGunnerSettledLossPredicateSnapshot != predicateSnapshot;
		if (episodeStarted)
		{
			m_GunnerSettledLossOccupant = occupant;
			m_iGunnerSettledLossStartedAtMs = nowMs;
			m_iGunnerSettledLossPolls = 0;
		}
		m_iGunnerSettledLossPolls++;
		m_sGunnerSettledLossPredicateSnapshot = predicateSnapshot;
		ageMs = Math.Max(0, nowMs - m_iGunnerSettledLossStartedAtMs);
		return m_iGunnerSettledLossPolls;
	}

	bool ResolveCrewRoleSettledLoss(
		EAICompartmentType role,
		int nowMs,
		out IEntity observedOccupant,
		out int polls,
		out int ageMs,
		out string predicateSnapshot)
	{
		observedOccupant = null;
		polls = 0;
		ageMs = 0;
		predicateSnapshot = string.Empty;
		if (role == EAICompartmentType.Pilot)
		{
			if (!m_DriverSettledLossOccupant || m_iDriverSettledLossStartedAtMs <= 0)
				return false;
			observedOccupant = m_DriverSettledLossOccupant;
			polls = m_iDriverSettledLossPolls;
			ageMs = Math.Max(0, nowMs - m_iDriverSettledLossStartedAtMs);
			predicateSnapshot = m_sDriverSettledLossPredicateSnapshot;
			ClearCrewRoleSettledLoss(role);
			return true;
		}
		if (role != EAICompartmentType.Turret || !m_GunnerSettledLossOccupant ||
			m_iGunnerSettledLossStartedAtMs <= 0)
		{
			return false;
		}
		observedOccupant = m_GunnerSettledLossOccupant;
		polls = m_iGunnerSettledLossPolls;
		ageMs = Math.Max(0, nowMs - m_iGunnerSettledLossStartedAtMs);
		predicateSnapshot = m_sGunnerSettledLossPredicateSnapshot;
		ClearCrewRoleSettledLoss(role);
		return true;
	}

	void ClearCrewRoleSettledLoss(EAICompartmentType role)
	{
		if (role == EAICompartmentType.Pilot)
		{
			m_DriverSettledLossOccupant = null;
			m_iDriverSettledLossStartedAtMs = 0;
			m_iDriverSettledLossPolls = 0;
			m_sDriverSettledLossPredicateSnapshot = string.Empty;
			return;
		}
		if (role != EAICompartmentType.Turret)
			return;
		m_GunnerSettledLossOccupant = null;
		m_iGunnerSettledLossStartedAtMs = 0;
		m_iGunnerSettledLossPolls = 0;
		m_sGunnerSettledLossPredicateSnapshot = string.Empty;
	}

	void MarkCrewRecoveryRoutePending() { m_bCrewRecoveryRoutePending = true; }
	void ClearCrewRecoveryRoutePending() { m_bCrewRecoveryRoutePending = false; }

	void ArmRecoveryEvidence(
		string reason,
		bool requireRouteProgress,
		bool unstuckRelocated,
		vector vehiclePosition,
		float routeDistanceMeters,
		int nowMs)
	{
		m_bRecoveryEvidencePending = true;
		m_bRecoveryRequiresRouteProgress = requireRouteProgress;
		m_bRecoveryPhysicalEvidence = false;
		m_bRecoveryRouteEvidence = false;
		m_bRecoveryMobilityRestoredReported = false;
		m_bPendingUnstuckRelocated = unstuckRelocated;
		m_iRecoveryEvidenceArmedAtMs = nowMs;
		m_sPendingRecoveryReason = reason;
		m_vLastPhysicalPosition = vehiclePosition;
		m_bHasPhysicalSample = true;
		m_iLastPhysicalMotionAtMs = nowMs;
		m_fBestRouteDistanceMeters = routeDistanceMeters;
		m_iLastRouteProgressAtMs = nowMs;
	}

	bool CanConfirmRecoveryEvidence()
	{
		return m_bRecoveryEvidencePending && m_bRecoveryPhysicalEvidence &&
			(!m_bRecoveryRequiresRouteProgress || m_bRecoveryRouteEvidence);
	}

	bool CanReportRecoveryMobilityRestored()
	{
		return m_bRecoveryEvidencePending && m_bRecoveryPhysicalEvidence &&
			!m_bRecoveryMobilityRestoredReported;
	}

	void MarkRecoveryMobilityRestoredReported()
	{
		if (m_bRecoveryEvidencePending && m_bRecoveryPhysicalEvidence)
			m_bRecoveryMobilityRestoredReported = true;
	}

	void ConfirmRecoveryEvidence()
	{
		m_bRecoveryEvidencePending = false;
		m_bRecoveryRequiresRouteProgress = false;
		m_bRecoveryPhysicalEvidence = false;
		m_bRecoveryRouteEvidence = false;
		m_bRecoveryMobilityRestoredReported = false;
		m_bPendingUnstuckRelocated = false;
		m_iRecoveryEvidenceArmedAtMs = 0;
		m_sPendingRecoveryReason = string.Empty;
	}
}

class AICF_VehicleDismountActionToken
{
	protected AIAgent m_Agent;
	protected IEntity m_ReservedEntity;
	protected SCR_AIMoveIndividuallyBehavior m_Action;
	protected ref AICF_VehicleAsyncFence m_Fence;
	protected vector m_vTargetPosition;
	protected int m_iIssuedAtMs;
	protected int m_iLastProgressAtMs;
	protected int m_iLastAuditAtMs;
	protected float m_fBestDistanceMeters = -1.0;
	protected float m_fCurrentDistanceMeters = -1.0;
	protected float m_fMinimumProjectionMeters;

	void AICF_VehicleDismountActionToken(
		AIAgent agent,
		IEntity reservedEntity,
		SCR_AIMoveIndividuallyBehavior action,
		AICF_VehicleAsyncFence fence,
		vector targetPosition,
		float initialDistanceMeters,
		float minimumProjectionMeters)
	{
		m_Agent = agent;
		m_ReservedEntity = reservedEntity;
		m_Action = action;
		m_Fence = fence;
		m_vTargetPosition = targetPosition;
		m_iIssuedAtMs = System.GetTickCount();
		m_iLastProgressAtMs = m_iIssuedAtMs;
		m_iLastAuditAtMs = m_iIssuedAtMs;
		m_fBestDistanceMeters = initialDistanceMeters;
		m_fCurrentDistanceMeters = initialDistanceMeters;
		m_fMinimumProjectionMeters = minimumProjectionMeters;
	}

	AIAgent GetAgent() { return m_Agent; }
	IEntity GetReservedEntity() { return m_ReservedEntity; }
	SCR_AIMoveIndividuallyBehavior GetAction() { return m_Action; }
	AICF_VehicleAsyncFence GetFence() { return m_Fence; }
	vector GetTargetPosition() { return m_vTargetPosition; }
	int GetIssuedAtMs() { return m_iIssuedAtMs; }
	int GetLastProgressAtMs() { return m_iLastProgressAtMs; }
	float GetBestDistanceMeters() { return m_fBestDistanceMeters; }
	float GetCurrentDistanceMeters() { return m_fCurrentDistanceMeters; }
	float GetMinimumProjectionMeters() { return m_fMinimumProjectionMeters; }

	void ObserveDistance(float distanceMeters, int nowMs, float progressMeters)
	{
		m_fCurrentDistanceMeters = distanceMeters;
		if (m_fBestDistanceMeters < 0 ||
			distanceMeters <= m_fBestDistanceMeters - progressMeters)
		{
			m_fBestDistanceMeters = distanceMeters;
			m_iLastProgressAtMs = nowMs;
		}
	}

	bool ShouldAudit(int nowMs, int intervalMs)
	{
		if (m_iLastAuditAtMs > 0 && nowMs - m_iLastAuditAtMs < intervalMs)
			return false;
		m_iLastAuditAtMs = nowMs;
		return true;
	}
}

class AICF_VehicleDismountState
{
	protected int m_iStartedAtMs;
	protected int m_iNormalDeadlineMs;
	protected int m_iTerminalDeadlineMs;
	protected bool m_bNormalReissueAttempted;
	protected int m_iGuidanceAttempts;
	protected int m_iMaximumGuidanceAttempts;
	protected int m_iLastGuidanceAttemptAtMs;
	protected int m_iForceClearanceAttempts;
	protected int m_iMaximumForceClearanceAttempts;
	protected int m_iLastExactRelocationProbeAtMs;
	protected int m_iLogicalOccupants;
	protected int m_iTransitions;
	protected int m_iInsideBounds;
	protected int m_iClearPollCount;
	protected int m_iContinuousClearStartedAtMs;
	protected bool m_bTerminalClearanceStarted;
	protected bool m_bTerminalClearanceStopped;
	protected int m_iLastHiddenRecoveryAuditAtMs;
	protected string m_sLastHiddenRecoveryRejection;
	protected AIWaypoint m_DismountWaypoint;
	protected AIWaypoint m_SupersededDismountWaypoint;
	protected bool m_bDismountWaypointBound;
	protected ref array<ref AICF_VehicleDismountActionToken> m_aGuidanceTokens = {};

	void Reset()
	{
		m_iStartedAtMs = 0;
		m_iNormalDeadlineMs = 0;
		m_iTerminalDeadlineMs = 0;
		m_bNormalReissueAttempted = false;
		m_iGuidanceAttempts = 0;
		m_iMaximumGuidanceAttempts = 0;
		m_iLastGuidanceAttemptAtMs = 0;
		m_iForceClearanceAttempts = 0;
		m_iMaximumForceClearanceAttempts = 0;
		m_iLastExactRelocationProbeAtMs = 0;
		m_iLogicalOccupants = 0;
		m_iTransitions = 0;
		m_iInsideBounds = 0;
		m_iClearPollCount = 0;
		m_iContinuousClearStartedAtMs = 0;
		m_bTerminalClearanceStarted = false;
		m_bTerminalClearanceStopped = false;
		m_iLastHiddenRecoveryAuditAtMs = 0;
		m_sLastHiddenRecoveryRejection = string.Empty;
		m_DismountWaypoint = null;
		m_SupersededDismountWaypoint = null;
		m_bDismountWaypointBound = false;
		m_aGuidanceTokens.Clear();
	}

	void Begin(int nowMs, int normalDeadlineMs, int terminalDeadlineMs, int guidanceBudget, int forceBudget)
	{
		Reset();
		m_iStartedAtMs = nowMs;
		m_iNormalDeadlineMs = normalDeadlineMs;
		m_iTerminalDeadlineMs = terminalDeadlineMs;
		m_iMaximumGuidanceAttempts = Math.Max(0, guidanceBudget);
		m_iMaximumForceClearanceAttempts = Math.Max(0, forceBudget);
	}

	void BeginTerminal(int nowMs, int terminalDeadlineMs, int forceBudget)
	{
		Reset();
		m_iStartedAtMs = nowMs;
		m_iNormalDeadlineMs = nowMs;
		m_iTerminalDeadlineMs = terminalDeadlineMs;
		m_iMaximumForceClearanceAttempts = Math.Max(0, forceBudget);
		m_bTerminalClearanceStarted = true;
	}

	int GetStartedAtMs() { return m_iStartedAtMs; }
	int GetNormalDeadlineMs() { return m_iNormalDeadlineMs; }
	int GetTerminalDeadlineMs() { return m_iTerminalDeadlineMs; }
	int GetGuidanceAttempts() { return m_iGuidanceAttempts; }
	int GetLastGuidanceAttemptAtMs() { return m_iLastGuidanceAttemptAtMs; }
	int GetForceClearanceAttempts() { return m_iForceClearanceAttempts; }
	int GetLogicalOccupants() { return m_iLogicalOccupants; }
	int GetTransitions() { return m_iTransitions; }
	int GetInsideBounds() { return m_iInsideBounds; }
	int GetClearPollCount() { return m_iClearPollCount; }
	bool WasNormalReissueAttempted() { return m_bNormalReissueAttempted; }
	bool IsTerminalClearanceStarted() { return m_bTerminalClearanceStarted; }
	bool IsTerminalClearanceStopped() { return m_bTerminalClearanceStopped; }
	AIWaypoint GetDismountWaypoint() { return m_DismountWaypoint; }
	AIWaypoint GetSupersededDismountWaypoint() { return m_SupersededDismountWaypoint; }
	bool IsDismountWaypointBound() { return m_bDismountWaypointBound; }
	int GetGuidanceTokenCount() { return m_aGuidanceTokens.Count(); }

	bool StageDismountWaypoint(AIWaypoint waypoint)
	{
		if (!waypoint || m_SupersededDismountWaypoint)
			return false;
		if (m_DismountWaypoint)
			m_SupersededDismountWaypoint = m_DismountWaypoint;
		m_DismountWaypoint = waypoint;
		m_bDismountWaypointBound = false;
		return true;
	}

	bool ConfirmDismountWaypointBound(AIWaypoint expected, bool bound)
	{
		if (!expected || expected != m_DismountWaypoint || !bound)
			return false;
		m_bDismountWaypointBound = true;
		return true;
	}

	bool ConfirmSupersededDismountWaypointRemoved(AIWaypoint expected)
	{
		if (!expected || expected != m_SupersededDismountWaypoint)
			return false;
		m_SupersededDismountWaypoint = null;
		return true;
	}

	bool ConfirmDismountWaypointRemoved(AIWaypoint expected)
	{
		if (!expected || expected != m_DismountWaypoint)
			return false;
		m_DismountWaypoint = null;
		m_bDismountWaypointBound = false;
		return true;
	}

	AICF_VehicleDismountActionToken GetGuidanceToken(int index)
	{
		if (!m_aGuidanceTokens.IsIndexValid(index))
			return null;
		return m_aGuidanceTokens[index];
	}

	AICF_VehicleDismountActionToken FindGuidanceToken(AIAgent agent)
	{
		foreach (AICF_VehicleDismountActionToken token : m_aGuidanceTokens)
		{
			if (token && token.GetAgent() == agent)
				return token;
		}
		return null;
	}

	void TrackGuidanceToken(AICF_VehicleDismountActionToken token)
	{
		if (token && !m_aGuidanceTokens.Contains(token))
			m_aGuidanceTokens.Insert(token);
	}

	void RemoveGuidanceToken(AICF_VehicleDismountActionToken expected)
	{
		if (expected)
			m_aGuidanceTokens.RemoveItem(expected);
	}

	bool MarkNormalReissueAttempted()
	{
		if (m_bNormalReissueAttempted)
			return false;
		m_bNormalReissueAttempted = true;
		return true;
	}

	bool RecordGuidanceAttempt(int nowMs)
	{
		if (m_iGuidanceAttempts >= m_iMaximumGuidanceAttempts)
			return false;
		m_iGuidanceAttempts++;
		m_iLastGuidanceAttemptAtMs = nowMs;
		return true;
	}

	bool RecordForceClearanceAttempt()
	{
		if (m_iForceClearanceAttempts >= m_iMaximumForceClearanceAttempts)
			return false;
		m_iForceClearanceAttempts++;
		return true;
	}

	bool TryBeginExactRelocationProbe(int nowMs, int backoffMs)
	{
		if (m_iLastExactRelocationProbeAtMs > 0 &&
			nowMs - m_iLastExactRelocationProbeAtMs < backoffMs)
		{
			return false;
		}
		m_iLastExactRelocationProbeAtMs = nowMs;
		return true;
	}

	bool ShouldAuditHiddenRecoveryRejection(
		string rejectionReason,
		int nowMs,
		int auditIntervalMs)
	{
		if (rejectionReason == m_sLastHiddenRecoveryRejection &&
			m_iLastHiddenRecoveryAuditAtMs > 0 &&
			nowMs - m_iLastHiddenRecoveryAuditAtMs < auditIntervalMs)
		{
			return false;
		}
		m_sLastHiddenRecoveryRejection = rejectionReason;
		m_iLastHiddenRecoveryAuditAtMs = nowMs;
		return true;
	}

	void StopTerminalClearance()
	{
		m_bTerminalClearanceStopped = true;
	}

	int RecordClearanceSample(
		int logicalOccupants,
		int transitions,
		int insideBounds,
		int nowMs,
		int additionalStabilityBlockers = 0)
	{
		m_iLogicalOccupants = Math.Max(0, logicalOccupants);
		m_iTransitions = Math.Max(0, transitions);
		m_iInsideBounds = Math.Max(0, insideBounds);
		if (m_iLogicalOccupants > 0 || m_iTransitions > 0 || m_iInsideBounds > 0 ||
			additionalStabilityBlockers > 0)
		{
			m_iClearPollCount = 0;
			m_iContinuousClearStartedAtMs = 0;
			return 0;
		}
		if (m_iContinuousClearStartedAtMs <= 0)
			m_iContinuousClearStartedAtMs = nowMs;
		m_iClearPollCount++;
		return m_iClearPollCount;
	}

	int GetContinuousClearMs(int nowMs)
	{
		if (m_iContinuousClearStartedAtMs <= 0 || m_iClearPollCount <= 0)
			return 0;
		return Math.Max(0, nowMs - m_iContinuousClearStartedAtMs);
	}
}

class AICF_VehicleHandoffState
{
	protected int m_iStartedAtMs;
	protected int m_iAbsoluteOrderDeadlineMs;
	protected int m_iRestoreAttempts;
	protected int m_iMaximumRestoreAttempts;
	protected bool m_bRestoreRequested;
	protected bool m_bBoundToGroup;
	protected bool m_bIsCurrent;
	protected bool m_bWaypointInQueue;
	protected bool m_bMeaningfulTask;
	protected bool m_bOrderRestored;
	protected bool m_bLeaseReleaseRequested;
	protected bool m_bCleanupQueueAttempted;
	protected bool m_bCleanupQueueAccepted;
	protected bool m_bCleanupReleaseComplete;
	protected bool m_bCleanupRetainedFailClosed;
	protected bool m_bCleanupOwnershipAcceptedTerminal;
	protected bool m_bClearanceSafe;
	protected string m_sCleanupActionToken;
	protected string m_sCleanupFailureReason;
	protected string m_sLastAbandonedAuditSignature;
	protected int m_iLastAbandonedAuditAtMs;
	protected int m_iDurablePollCount;
	protected int m_iFirstDurablePollAtMs;
	protected AIWaypoint m_RestoredWaypoint;

	void Reset()
	{
		m_iStartedAtMs = 0;
		m_iAbsoluteOrderDeadlineMs = 0;
		m_iRestoreAttempts = 0;
		m_iMaximumRestoreAttempts = 0;
		m_bRestoreRequested = false;
		m_bBoundToGroup = false;
		m_bIsCurrent = false;
		m_bWaypointInQueue = false;
		m_bMeaningfulTask = false;
		m_bOrderRestored = false;
		m_bLeaseReleaseRequested = false;
		m_bCleanupQueueAttempted = false;
		m_bCleanupQueueAccepted = false;
		m_bCleanupReleaseComplete = false;
		m_bCleanupRetainedFailClosed = false;
		m_bCleanupOwnershipAcceptedTerminal = false;
		m_bClearanceSafe = false;
		m_sCleanupActionToken = string.Empty;
		m_sCleanupFailureReason = string.Empty;
		m_sLastAbandonedAuditSignature = string.Empty;
		m_iLastAbandonedAuditAtMs = 0;
		m_iDurablePollCount = 0;
		m_iFirstDurablePollAtMs = 0;
		m_RestoredWaypoint = null;
	}

	void Begin(int nowMs, int absoluteOrderDeadlineMs, int maximumRestoreAttempts)
	{
		Reset();
		m_iStartedAtMs = nowMs;
		m_iAbsoluteOrderDeadlineMs = absoluteOrderDeadlineMs;
		m_iMaximumRestoreAttempts = Math.Max(1, maximumRestoreAttempts);
	}

	int GetStartedAtMs() { return m_iStartedAtMs; }
	int GetAbsoluteOrderDeadlineMs() { return m_iAbsoluteOrderDeadlineMs; }
	int GetRestoreAttempts() { return m_iRestoreAttempts; }
	int GetMaximumRestoreAttempts() { return m_iMaximumRestoreAttempts; }
	bool IsRestoreRequested() { return m_bRestoreRequested; }
	bool IsBoundToGroup() { return m_bBoundToGroup; }
	bool IsCurrent() { return m_bIsCurrent; }
	bool IsWaypointInQueue() { return m_bWaypointInQueue; }
	bool HasMeaningfulTask() { return m_bMeaningfulTask; }
	bool IsOrderRestored()
	{
		return m_bOrderRestored && m_bBoundToGroup && m_bIsCurrent &&
			m_bWaypointInQueue && m_bMeaningfulTask;
	}
	bool IsLeaseReleaseRequested() { return m_bLeaseReleaseRequested; }
	bool IsCleanupQueueAttempted() { return m_bCleanupQueueAttempted; }
	bool IsCleanupQueueAccepted() { return m_bCleanupQueueAccepted; }
	bool IsCleanupReleaseComplete() { return m_bCleanupReleaseComplete; }
	bool IsCleanupRetainedFailClosed() { return m_bCleanupRetainedFailClosed; }
	bool IsCleanupOwnershipAcceptedTerminal() { return m_bCleanupOwnershipAcceptedTerminal; }
	bool IsClearanceSafe() { return m_bClearanceSafe; }
	string GetCleanupActionToken() { return m_sCleanupActionToken; }
	string GetCleanupFailureReason() { return m_sCleanupFailureReason; }
	string GetLastAbandonedAuditSignature() { return m_sLastAbandonedAuditSignature; }
	int GetLastAbandonedAuditAtMs() { return m_iLastAbandonedAuditAtMs; }
	int GetDurablePollCount() { return m_iDurablePollCount; }
	AIWaypoint GetRestoredWaypoint() { return m_RestoredWaypoint; }

	bool BeginOrderRestoreRequest()
	{
		if (m_iRestoreAttempts >= m_iMaximumRestoreAttempts)
			return false;
		m_iRestoreAttempts++;
		m_bRestoreRequested = true;
		return true;
	}

	bool RecordOrderRestoreResult(
		AIWaypoint waypoint,
		bool boundToGroup,
		bool isCurrent,
		bool waypointInQueue,
		bool postconditionMeaningfulTask)
	{
		m_RestoredWaypoint = waypoint;
		m_bBoundToGroup = boundToGroup;
		m_bIsCurrent = isCurrent;
		m_bWaypointInQueue = waypointInQueue;
		m_bMeaningfulTask = postconditionMeaningfulTask;
		m_bOrderRestored = waypoint && boundToGroup && isCurrent && waypointInQueue &&
			postconditionMeaningfulTask;
		if (!m_bOrderRestored)
		{
			m_iDurablePollCount = 0;
			m_iFirstDurablePollAtMs = 0;
		}
		return m_bOrderRestored;
	}

	int ObserveDurableOrder(bool currentAndQueued, int nowMs)
	{
		if (!m_bOrderRestored || !currentAndQueued)
		{
			m_iDurablePollCount = 0;
			m_iFirstDurablePollAtMs = 0;
			return 0;
		}
		if (m_iFirstDurablePollAtMs <= 0)
			m_iFirstDurablePollAtMs = nowMs;
		m_iDurablePollCount++;
		return m_iDurablePollCount;
	}

	bool IsOrderRestoreDurable(int nowMs, int minimumDurationMs)
	{
		return m_bOrderRestored && m_iDurablePollCount >= 3 &&
			m_iFirstDurablePollAtMs > 0 && nowMs - m_iFirstDurablePollAtMs >= minimumDurationMs;
	}

	void RecordClearanceResult(bool clearanceSafe)
	{
		// Intentionally independent from order_restored.
		m_bClearanceSafe = clearanceSafe;
	}

	void RequestLeaseRelease()
	{
		// This is only a cleanup request. It is deliberately not evidence that
		// protected occupants, players, proximity and stable-clear are safe.
		m_bLeaseReleaseRequested = true;
	}

	bool BeginCleanupQueueAttempt()
	{
		if (!m_bLeaseReleaseRequested || m_bCleanupQueueAttempted)
			return false;
		m_bCleanupQueueAttempted = true;
		return true;
	}

	void RecordCleanupQueueResult(
		bool accepted,
		bool releaseComplete,
		string actionToken,
		string failureReason)
	{
		m_bCleanupQueueAccepted = accepted;
		m_bCleanupReleaseComplete = accepted && releaseComplete;
		m_sCleanupActionToken = actionToken;
		m_sCleanupFailureReason = failureReason;
	}

	void RecordCleanupObservation(
		bool clearanceSafe,
		bool releaseComplete,
		bool retainedFailClosed,
		string failureReason)
	{
		m_bClearanceSafe = clearanceSafe;
		m_bCleanupReleaseComplete = releaseComplete;
		m_bCleanupRetainedFailClosed = retainedFailClosed;
		if (!failureReason.IsEmpty())
			m_sCleanupFailureReason = failureReason;
	}

	void RecordCleanupOwnershipAcceptedTerminal()
	{
		m_bCleanupOwnershipAcceptedTerminal = m_bCleanupRetainedFailClosed;
	}

	// Diagnostics is observation-only. The lifecycle owner stores and commits
	// the on-change/rate-limit decision so a log formatter cannot become a
	// second source of timing state.
	bool ShouldAuditAbandonedExit(string signature, int nowMs, int intervalMs)
	{
		if (signature.IsEmpty() || nowMs <= 0 || intervalMs <= 0)
			return false;
		if (signature == m_sLastAbandonedAuditSignature &&
			m_iLastAbandonedAuditAtMs > 0 &&
			nowMs - m_iLastAbandonedAuditAtMs < intervalMs)
		{
			return false;
		}
		m_sLastAbandonedAuditSignature = signature;
		m_iLastAbandonedAuditAtMs = nowMs;
		return true;
	}
}

class AICF_VehicleCleanupState
{
	protected int m_iStartedAtMs;
	protected int m_iAbsoluteDeadlineMs;
	protected int m_iStableClearStartedAtMs;
	protected int m_iDeleteAttempts;
	protected int m_iMaximumDeleteAttempts;
	protected int m_iLastDeleteAttemptAtMs;
	protected bool m_bDeleteConfirmationPending;
	protected bool m_bStoppedFailClosed;
	protected string m_sBlockerSignature;

	void Reset()
	{
		m_iStartedAtMs = 0;
		m_iAbsoluteDeadlineMs = 0;
		m_iStableClearStartedAtMs = 0;
		m_iDeleteAttempts = 0;
		m_iMaximumDeleteAttempts = 0;
		m_iLastDeleteAttemptAtMs = 0;
		m_bDeleteConfirmationPending = false;
		m_bStoppedFailClosed = false;
		m_sBlockerSignature = string.Empty;
	}

	void Begin(int nowMs, int absoluteDeadlineMs, int maximumDeleteAttempts)
	{
		Reset();
		m_iStartedAtMs = nowMs;
		m_iAbsoluteDeadlineMs = absoluteDeadlineMs;
		m_iMaximumDeleteAttempts = Math.Max(1, maximumDeleteAttempts);
	}

	int GetStartedAtMs() { return m_iStartedAtMs; }
	int GetAbsoluteDeadlineMs() { return m_iAbsoluteDeadlineMs; }
	int GetStableClearStartedAtMs() { return m_iStableClearStartedAtMs; }
	int GetDeleteAttempts() { return m_iDeleteAttempts; }
	int GetLastDeleteAttemptAtMs() { return m_iLastDeleteAttemptAtMs; }
	bool IsDeleteConfirmationPending() { return m_bDeleteConfirmationPending; }
	bool IsStoppedFailClosed() { return m_bStoppedFailClosed; }
	string GetBlockerSignature() { return m_sBlockerSignature; }

	int ObserveSafeClear(bool safeClear, string blockerSignature, int nowMs)
	{
		m_sBlockerSignature = blockerSignature;
		if (!safeClear)
		{
			m_iStableClearStartedAtMs = 0;
			return 0;
		}
		if (m_iStableClearStartedAtMs <= 0)
			m_iStableClearStartedAtMs = nowMs;
		return nowMs - m_iStableClearStartedAtMs;
	}

	bool BeginDeleteAttempt(int nowMs)
	{
		if (m_bStoppedFailClosed || m_iDeleteAttempts >= m_iMaximumDeleteAttempts)
			return false;
		m_iDeleteAttempts++;
		m_iLastDeleteAttemptAtMs = nowMs;
		m_bDeleteConfirmationPending = true;
		return true;
	}

	void ConfirmDelete()
	{
		m_bDeleteConfirmationPending = false;
	}

	void StopFailClosed()
	{
		m_bStoppedFailClosed = true;
	}
}
