// Acceptance is an observation-only projection of committed vehicle-domain
// facts. It never advances a trip, changes a lease, or issues gameplay work.
enum AICF_EVehicleAcceptanceFailureDomain
{
	NONE = 0,
	BOARDING,
	DISMOUNT,
	RECOVERY,
	CLEANUP,
	IDENTITY,
	DIAGNOSTICS
}

// One unified record is keyed by faction + stable slot. Completion evidence is
// intentionally independent of the current group generation: a replacement
// group must not erase an already demonstrated configured acceptance slice.
class AICF_VehicleAcceptanceRecord
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected bool m_bTransportComplete;
	protected bool m_bArmedComplete;
	protected AICF_EGroupUnitType m_ExpectedUnitType;
	protected int m_iLastGroupGeneration;
	protected int m_iLastTripGeneration;
	protected AICF_ETripOutcomeKind m_LastOutcomeKind;
	protected string m_sLastOperationId;
	protected string m_sLastCausationId;

	void AICF_VehicleAcceptanceRecord(FactionKey factionKey, int slotId)
	{
		m_sFactionKey = factionKey;
		m_iSlotId = slotId;
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	bool IsTransportComplete() { return m_bTransportComplete; }
	bool IsArmedComplete() { return m_bArmedComplete; }
	bool IsConfiguredMotorized()
	{
		return m_ExpectedUnitType == AICF_EGroupUnitType.MOTORIZED_LIGHT ||
			m_ExpectedUnitType == AICF_EGroupUnitType.MOTORIZED_TRUCK ||
			m_ExpectedUnitType == AICF_EGroupUnitType.MOTORIZED_ARMED_LIGHT;
	}
	bool IsExpectedComplete()
	{
		if (m_ExpectedUnitType == AICF_EGroupUnitType.MOTORIZED_ARMED_LIGHT)
			return m_bArmedComplete;
		return !IsConfiguredMotorized() || m_bTransportComplete;
	}
	void SetExpectedUnitType(AICF_EGroupUnitType unitType)
	{
		m_ExpectedUnitType = unitType;
	}
	int GetLastGroupGeneration() { return m_iLastGroupGeneration; }
	int GetLastTripGeneration() { return m_iLastTripGeneration; }
	string GetLastOperationId() { return m_sLastOperationId; }

	bool IsDuplicateTerminal(AICF_TransportTrip trip, AICF_TripOutcome outcome)
	{
		if (!trip || !outcome)
			return false;
		return trip.GetGroupGeneration() == m_iLastGroupGeneration &&
			trip.GetTripGeneration() == m_iLastTripGeneration &&
			trip.GetOperationId() == m_sLastOperationId &&
			outcome.GetKind() == m_LastOutcomeKind;
	}

	bool IsStaleOrCollidingTerminal(AICF_TransportTrip trip)
	{
		if (!trip || m_iLastGroupGeneration <= 0)
			return false;
		if (trip.GetGroupGeneration() < m_iLastGroupGeneration)
			return true;
		if (trip.GetGroupGeneration() > m_iLastGroupGeneration)
			return false;
		if (trip.GetTripGeneration() < m_iLastTripGeneration)
			return true;
		// The caller has already removed an exact duplicate. Any second
		// terminal observation for the same generation is an identity collision,
		// even if it reuses the operation id with a different outcome.
		return trip.GetTripGeneration() == m_iLastTripGeneration;
	}

	void RecordTerminal(AICF_TransportTrip trip, AICF_TripOutcome outcome)
	{
		m_iLastGroupGeneration = trip.GetGroupGeneration();
		m_iLastTripGeneration = trip.GetTripGeneration();
		m_LastOutcomeKind = outcome.GetKind();
		m_sLastOperationId = trip.GetOperationId();
		m_sLastCausationId = outcome.GetCausationId();
	}

	bool MarkComplete(AICF_EVehicleKind kind)
	{
		if (kind == AICF_EVehicleKind.TRANSPORT ||
			kind == AICF_EVehicleKind.LIGHT_TRANSPORT)
		{
			if (m_bTransportComplete)
				return false;
			m_bTransportComplete = true;
			return true;
		}
		if (kind != AICF_EVehicleKind.ARMED_LIGHT || m_bArmedComplete)
			return false;
		m_bArmedComplete = true;
		return true;
	}
}

// The monitor owns only acceptance projection state. Callers explicitly feed
// committed terminal outcomes and classified failures; there is no event bus
// and no polling of mutable gameplay objects behind the domain's back.
class AICF_VehicleAcceptanceMonitor
{
	static const int DEFAULT_REQUIRED_FACTIONS = 2;

	protected bool m_bVehiclesEnabled;
	protected int m_iTransportSlotsPerFaction;
	protected int m_iArmedSlotsPerFaction;
	protected int m_iRequiredFactionCount;
	protected ref array<FactionKey> m_aFactions = {};
	protected ref array<ref AICF_VehicleAcceptanceRecord> m_aRecords = {};

	protected bool m_bFailureLatched;
	protected int m_iFailureCount;
	protected string m_sFirstFailureDomain;
	protected string m_sFirstFailureReason;
	protected string m_sLastFailureFingerprint;
	protected bool m_bReadyCandidateEmitted;
	protected bool m_bCandidateInvalidated;
	protected bool m_bStopped;

	void AICF_VehicleAcceptanceMonitor(
		AICF_Stage3Config config,
		int requiredFactionCount = DEFAULT_REQUIRED_FACTIONS)
	{
		m_iRequiredFactionCount = Math.Max(1, requiredFactionCount);
		if (!config)
			return;
		m_bVehiclesEnabled = config.GetVehiclesEnabled();
		m_iTransportSlotsPerFaction = config.GetTransportVehiclesPerFaction();
		m_iArmedSlotsPerFaction = config.GetArmedLightVehiclesPerFaction();
	}

	bool RegisterFaction(FactionKey factionKey)
	{
		if (m_bStopped || factionKey.IsEmpty())
			return false;
		if (m_aFactions.Find(factionKey) >= 0)
			return true;
		if (m_aFactions.Count() >= m_iRequiredFactionCount)
			return false;

		m_aFactions.Insert(factionKey);
		for (int slotId = 0; slotId < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION; slotId++)
			m_aRecords.Insert(new AICF_VehicleAcceptanceRecord(factionKey, slotId));
		EvaluateCandidate();
		return true;
	}

	// Commander configuration, rather than legacy numeric-slot allocation,
	// defines which live slots belong to the transport acceptance surface.
	void SyncFactionConfiguration(AICF_FactionState factionState)
	{
		if (m_bStopped || !factionState)
			return;
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			AICF_VehicleAcceptanceRecord record = FindRecord(
				factionState.GetFactionKey(),
				slotId);
			if (slot && record)
				record.SetExpectedUnitType(slot.GetUnitType());
		}
	}

	// Call exactly once after TripController commits the terminal transition and
	// before an accepted completion lease is detached. failureDomain is NONE for
	// a valid bounded fallback and identifies a tested fault when the terminal
	// outcome itself is acceptance-negative.
	void ObserveCommittedTerminal(
		AICF_TransportTrip trip,
		AICF_TripOutcome outcome,
		AICF_EVehicleAcceptanceFailureDomain failureDomain = AICF_EVehicleAcceptanceFailureDomain.NONE)
	{
		if (m_bStopped || !trip || !outcome || !outcome.IsTerminal())
			return;
		if (!trip.IsTerminal() || !TerminalPairMatches(trip, outcome))
		{
			ObserveTripFailure(
				trip,
				AICF_EVehicleAcceptanceFailureDomain.IDENTITY,
				"TERMINAL_OUTCOME_NOT_COMMITTED");
			return;
		}

		AICF_VehicleAcceptanceRecord record = FindRecord(trip.GetFactionKey(), trip.GetSlotId());
		if (!record)
		{
			ObserveTripFailure(
				trip,
				AICF_EVehicleAcceptanceFailureDomain.IDENTITY,
				"TERMINAL_SLOT_NOT_REGISTERED");
			return;
		}
		if (record.IsDuplicateTerminal(trip, outcome))
			return;
		if (record.IsStaleOrCollidingTerminal(trip))
		{
			ObserveTripFailure(
				trip,
				AICF_EVehicleAcceptanceFailureDomain.IDENTITY,
				"STALE_OR_COLLIDING_TERMINAL_IDENTITY");
			return;
		}

		record.RecordTerminal(trip, outcome);
		if (failureDomain != AICF_EVehicleAcceptanceFailureDomain.NONE)
		{
			ObserveTripFailure(trip, failureDomain, outcome.GetReason());
			return;
		}
		if (outcome.GetKind() == AICF_ETripOutcomeKind.TERMINAL_FAIL_CLOSED)
		{
			ObserveTripFailure(
				trip,
				AICF_EVehicleAcceptanceFailureDomain.IDENTITY,
				outcome.GetReason());
			return;
		}
		if (outcome.GetKind() != AICF_ETripOutcomeKind.COMPLETE_TRIP)
		{
			EvaluateCandidate();
			return;
		}

		AICF_VehicleLease lease = trip.GetLease();
		if (!HasCompletionIdentity(trip, lease))
		{
			ObserveTripFailure(
				trip,
				AICF_EVehicleAcceptanceFailureDomain.IDENTITY,
				"COMPLETION_LEASE_IDENTITY_MISSING");
			return;
		}
		record.MarkComplete(lease.GetKind());
		EvaluateCandidate();
	}

	// Boarding, dismount and recovery owners call this only for a committed
	// acceptance-negative observation. Repeated delivery of the same immutable
	// identity/reason is deduplicated, including calls made after READY.
	void ObserveTripFailure(
		AICF_TransportTrip trip,
		AICF_EVehicleAcceptanceFailureDomain domain,
		string reason)
	{
		if (m_bStopped || domain == AICF_EVehicleAcceptanceFailureDomain.NONE)
			return;
		string operationId = "NONE";
		string causationId = "NONE";
		string faction = "NONE";
		int slotId = -1;
		int groupGeneration;
		int tripGeneration;
		string lifecycleId = "NONE";
		string entityId = "NONE";
		string rplId = "NONE";
		if (trip)
		{
			operationId = trip.GetOperationId();
			causationId = trip.GetCausationId();
			faction = trip.GetFactionKey();
			slotId = trip.GetSlotId();
			groupGeneration = trip.GetGroupGeneration();
			tripGeneration = trip.GetTripGeneration();
			AICF_VehicleLease lease = trip.GetLease();
			if (lease)
			{
				lifecycleId = lease.GetVehicleLifecycleId();
				entityId = lease.GetEntityIdString();
				rplId = lease.GetRplId();
			}
		}
		LatchFailure(
			domain,
			reason,
			faction,
			slotId,
			groupGeneration,
			tripGeneration,
			lifecycleId,
			entityId,
			rplId,
			operationId,
			causationId);
	}

	// Cleanup can outlive its Trip. Its immutable pre-null/delete snapshot is
	// therefore the only accepted physical identity source for late failures.
	void ObserveCleanupFailure(
		AICF_VehicleCleanupSnapshot snapshot,
		string reason,
		string operationId,
		string causationId)
	{
		if (m_bStopped)
			return;
		if (!snapshot || !snapshot.IsComplete())
		{
			LatchFailure(
				AICF_EVehicleAcceptanceFailureDomain.IDENTITY,
				"CLEANUP_FAILURE_SNAPSHOT_INCOMPLETE",
				"NONE", -1, 0, 0, "NONE", "NONE", "NONE",
				operationId, causationId);
			return;
		}
		LatchFailure(
			AICF_EVehicleAcceptanceFailureDomain.CLEANUP,
			reason,
			snapshot.GetFactionKey(),
			snapshot.GetSlotId(),
			snapshot.GetGroupGeneration(),
			snapshot.GetTripGeneration(),
			snapshot.GetVehicleLifecycleId(),
			snapshot.GetLastEntityIdString(),
			snapshot.GetLastRplId(),
			operationId,
			causationId);
	}

	void ObserveCleanupFailureFromFence(
		AICF_VehicleCleanupFence fence,
		string reason,
		string operationId,
		string causationId)
	{
		if (m_bStopped || !fence || !fence.IsComplete())
			return;
		LatchFailure(
			AICF_EVehicleAcceptanceFailureDomain.CLEANUP,
			reason,
			fence.GetFactionKey(),
			fence.GetSlotId(),
			fence.GetGroupGeneration(),
			fence.GetTripGeneration(),
			fence.GetVehicleLifecycleId(),
			fence.GetEntityId().ToString(),
			fence.GetRplId(),
			operationId,
			causationId);
	}

	// Safe to call from an aggregate heartbeat. It emits only on the first new
	// error state or on the one committed READY transition.
	void Audit()
	{
		if (m_bStopped)
			return;
		if (AICF_Stage3Diagnostics.HasErrors() || AICF_Stage35Diagnostics.HasErrors())
		{
			LatchFailure(
				AICF_EVehicleAcceptanceFailureDomain.DIAGNOSTICS,
				"DIAGNOSTIC_ERROR_STATE",
				"NONE", -1, 0, 0, "NONE", "NONE", "NONE", "NONE", "NONE");
			return;
		}
		EvaluateCandidate();
	}

	// Stop never manufactures a success verdict. READY_NOT_FINALIZED means only that the
	// automated candidate remained uninvalidated; controlled log review and the
	// documented runtime matrix remain external acceptance gates.
	void Stop(string reason)
	{
		if (m_bStopped)
			return;
		Audit();
		m_bStopped = true;
		string stopReason = reason;
		if (stopReason.IsEmpty())
			stopReason = "MONITOR_STOPPED";

		if (m_bFailureLatched || AICF_Stage3Diagnostics.HasErrors() ||
			AICF_Stage35Diagnostics.HasErrors())
		{
			string details = string.Format(
				"status=FAIL reason=%1 first_failure_domain=%2 first_failure_reason=%3",
				stopReason,
				m_sFirstFailureDomain,
				m_sFirstFailureReason);
			details += string.Format(
				" acceptance_failure_count=%1 candidate_ready=%2 invalidated=%3 final=0 requires_log_review=1",
				m_iFailureCount,
				m_bReadyCandidateEmitted,
				m_bCandidateInvalidated);
			AICF_Stage3Diagnostics.Warning("RESULT", details);
			return;
		}

		if (m_bReadyCandidateEmitted && !m_bCandidateInvalidated)
		{
			AICF_Stage3Diagnostics.Info(
				"RESULT",
				string.Format(
					"status=READY_NOT_FINALIZED reason=%1 candidate_ready=1 invalidated=0 final=0 requires_log_review=1",
					stopReason));
			return;
		}

		if (m_bVehiclesEnabled && CountConfiguredMotorizedSlots() == 0)
		{
			AICF_Stage3Diagnostics.Info(
				"RESULT",
				string.Format(
					"status=NOT_APPLICABLE reason=%1 configured_motorized_slots=0 final=0 requires_log_review=0",
					stopReason));
			return;
		}

		AICF_Stage3Diagnostics.Warning(
			"RESULT",
			string.Format(
				"status=FAIL reason=%1 candidate_ready=0 configured_slices_incomplete=1 final=0 requires_log_review=1",
				stopReason));
	}

	bool HasFailureLatch() { return m_bFailureLatched; }
	int GetFailureCount() { return m_iFailureCount; }
	bool HasReadyCandidate() { return m_bReadyCandidateEmitted; }
	bool IsCandidateInvalidated() { return m_bCandidateInvalidated; }
	bool IsStopped() { return m_bStopped; }

	bool IsSlotComplete(FactionKey factionKey, int slotId, AICF_EVehicleKind kind)
	{
		AICF_VehicleAcceptanceRecord record = FindRecord(factionKey, slotId);
		if (!record)
			return false;
		if (kind == AICF_EVehicleKind.ARMED_LIGHT)
			return record.IsArmedComplete();
		return record.IsTransportComplete();
	}

	protected bool TerminalPairMatches(AICF_TransportTrip trip, AICF_TripOutcome outcome)
	{
		switch (outcome.GetKind())
		{
			case AICF_ETripOutcomeKind.COMPLETE_TRIP:
				return trip.GetPhase() == AICF_ETransportTripPhase.COMPLETE;
			case AICF_ETripOutcomeKind.FALLBACK_TO_FOOT:
				return trip.GetPhase() == AICF_ETransportTripPhase.FALLBACK;
			case AICF_ETripOutcomeKind.TERMINAL_FAIL_CLOSED:
				return trip.GetPhase() == AICF_ETransportTripPhase.FAILED_CLOSED;
		}
		return false;
	}

	protected bool HasCompletionIdentity(AICF_TransportTrip trip, AICF_VehicleLease lease)
	{
		if (!trip || !lease || !lease.MatchesTripIdentity(
			trip.GetFactionKey(),
			trip.GetSlotId(),
			trip.GetGroupGeneration(),
			trip.GetTripGeneration()))
			return false;
		return lease.GetLeaseGeneration() > 0 && lease.GetVehicleGeneration() > 0 &&
			!lease.GetVehicleLifecycleId().IsEmpty() &&
			lease.GetEntityId() != EntityID.INVALID &&
			!lease.GetEntityIdString().IsEmpty() && !lease.GetRplId().IsEmpty() &&
			lease.GetRplId() != "NONE";
	}

	protected AICF_VehicleAcceptanceRecord FindRecord(FactionKey factionKey, int slotId)
	{
		foreach (AICF_VehicleAcceptanceRecord record : m_aRecords)
		{
			if (record && record.GetFactionKey() == factionKey && record.GetSlotId() == slotId)
				return record;
		}
		return null;
	}

	protected bool AreConfiguredSlicesComplete()
	{
		if (!m_bVehiclesEnabled || m_iRequiredFactionCount <= 0 ||
			m_aFactions.Count() != m_iRequiredFactionCount ||
			CountConfiguredMotorizedSlots() <= 0)
			return false;

		foreach (AICF_VehicleAcceptanceRecord record : m_aRecords)
		{
			if (record && !record.IsExpectedComplete())
				return false;
		}
		return true;
	}

	protected int CountConfiguredMotorizedSlots()
	{
		int count;
		foreach (AICF_VehicleAcceptanceRecord record : m_aRecords)
		{
			if (record && record.IsConfiguredMotorized())
				count++;
		}
		return count;
	}

	protected void EvaluateCandidate()
	{
		if (m_bStopped || m_bReadyCandidateEmitted || m_bFailureLatched ||
			AICF_Stage3Diagnostics.HasErrors() || AICF_Stage35Diagnostics.HasErrors() ||
			!AreConfiguredSlicesComplete())
			return;

		m_bReadyCandidateEmitted = true;
		string details = string.Format(
			"status=READY configured_profiles_complete=1 configured_motorized_slots=%1",
			CountConfiguredMotorizedSlots());
		details += string.Format(
			" required_factions=%1 registered_factions=%2 scope=AUTOMATED_TRIP_INVARIANTS final=0 requires_log_review=1",
			m_iRequiredFactionCount,
			m_aFactions.Count());
		AICF_Stage3Diagnostics.Info("RESULT_CANDIDATE", details);
	}

	protected void LatchFailure(
		AICF_EVehicleAcceptanceFailureDomain domain,
		string reason,
		string faction,
		int slotId,
		int groupGeneration,
		int tripGeneration,
		string lifecycleId,
		string entityId,
		string rplId,
		string operationId,
		string causationId)
	{
		if (m_bStopped || domain == AICF_EVehicleAcceptanceFailureDomain.NONE)
			return;
		string safeReason = reason;
		if (safeReason.IsEmpty())
			safeReason = "UNSPECIFIED_FAILURE";
		string domainName = typename.EnumToString(AICF_EVehicleAcceptanceFailureDomain, domain);
		string fingerprint = string.Format(
			"%1|%2|%3|%4|%5|%6|%7|%8",
			domainName,
			safeReason,
			faction,
			slotId,
			groupGeneration,
			tripGeneration,
			lifecycleId,
			operationId);
		fingerprint += string.Format("|%1|%2|%3", entityId, rplId, causationId);
		if (fingerprint == m_sLastFailureFingerprint)
			return;
		m_sLastFailureFingerprint = fingerprint;
		m_iFailureCount++;
		bool firstFailure = !m_bFailureLatched;
		m_bFailureLatched = true;
		if (!firstFailure)
			return;

		m_sFirstFailureDomain = domainName;
		m_sFirstFailureReason = safeReason;
		string details = string.Format(
			"reason=%1 domain=%2 faction=%3 slot=%4 group_gen=%5 trip_gen=%6",
			safeReason,
			domainName,
			faction,
			slotId,
			groupGeneration,
			tripGeneration);
		details += string.Format(
			" vehicle_lifecycle_id=%1 entity_id=%2 rpl_id=%3 operation_id=%4 causation_id=%5",
			lifecycleId,
			entityId,
			rplId,
			operationId,
			causationId);
		details += " count=1 first_failure=1";
		AICF_Stage3Diagnostics.Warning("ACCEPTANCE_FAILURE_LATCHED", details);

		if (!m_bReadyCandidateEmitted || m_bCandidateInvalidated)
			return;
		m_bCandidateInvalidated = true;
		AICF_Stage3Diagnostics.Warning(
			"RESULT_CANDIDATE",
			string.Format(
				"status=INVALIDATED reason=%1 failure_domain=%2 acceptance_failure_count=%3 final=0 requires_log_review=1",
				safeReason,
				domainName,
				m_iFailureCount));
	}
}
