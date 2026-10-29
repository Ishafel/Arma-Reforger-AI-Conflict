// Authoritative state for one bounded transport attempt. Only
// AICF_TransportTripController may call CommitTransition or CommitRetarget.
class AICF_TransportTrip
{
	protected AICF_GroupSlot m_VehiclePresetSlot;
	protected ref AICF_VehiclePreset m_VehiclePreset;
	protected int m_iVehiclePresetRevision;
	protected ref array<IEntity> m_aVehiclePresetMembers = {};
	protected ref array<EntityID> m_aVehiclePresetMemberIds = {};
	protected bool m_bVehiclePresetDebited;
	protected ref AICF_VehicleBehaviorState m_BehaviorState = new AICF_VehicleBehaviorState();
	AICF_VehicleBehaviorState GetBehaviorState() { return m_BehaviorState; }
	AICF_VehiclePreset GetVehiclePreset() { return m_VehiclePreset; }
	int GetVehiclePresetRevision() { return m_iVehiclePresetRevision; }
	AICF_GroupSlot GetVehiclePresetSlot() { return m_VehiclePresetSlot; }
	bool IsVehiclePresetDebited() { return m_bVehiclePresetDebited; }
	void MarkVehiclePresetDebited() { m_bVehiclePresetDebited = true; }

	void CaptureVehiclePreset(AICF_GroupSlot slot)
	{
		m_VehiclePresetSlot = slot;
		m_iVehiclePresetRevision = slot.GetVehiclePresetRevision();
		m_VehiclePreset = AICF_VehiclePreset.Decode(slot.GetVehiclePresetData());
		if (!m_VehiclePreset) return;
		for (int i; i < AICF_Stage1Config.MAX_GROUP_SIZE; i++)
		{
			IEntity member = slot.GetRosterMember(i);
			m_aVehiclePresetMembers.Insert(member);
			EntityID id = EntityID.INVALID;
			if (member) id = member.GetID();
			m_aVehiclePresetMemberIds.Insert(id);
		}
	}

	bool IsVehiclePresetCurrent()
	{
		if (!m_Assignment || !m_VehiclePresetSlot || m_VehiclePresetSlot.GetVehiclePresetRevision() != m_iVehiclePresetRevision ||
			m_VehiclePresetSlot.GetGroup() != m_Assignment.GetGroup() || m_VehiclePresetSlot.GetSpawnGeneration() != GetGroupGeneration()) return false;
		if (!m_VehiclePreset) return !m_VehiclePresetSlot.HasVehiclePreset();
		if (m_aVehiclePresetMembers.Count() != AICF_Stage1Config.MAX_GROUP_SIZE ||
			m_aVehiclePresetMemberIds.Count() != AICF_Stage1Config.MAX_GROUP_SIZE) return false;
		// Identity охватывает весь captured roster, включая утратившие роль позиции.
		for (int index; index < AICF_Stage1Config.MAX_GROUP_SIZE; index++)
		{
			IEntity member = VehiclePresetMember(index);
			// Погибший original actor не является сменой пресета. Живой новый
			// roster на той же позиции по-прежнему требует новой incarnation.
			IEntity currentMember = m_VehiclePresetSlot.GetRosterMember(index);
			if (!AICF_GroupRuntime.IsAliveCharacter(member))
			{
				if (AICF_GroupRuntime.IsAliveCharacter(currentMember)) return false;
				continue;
			}
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member) || currentMember != member) return false;
		}
		return true;
	}

	IEntity VehiclePresetMember(int index)
	{
		if (!m_aVehiclePresetMembers.IsIndexValid(index)) return null;
		IEntity member = m_aVehiclePresetMembers[index];
		if (!member || member.GetID() != m_aVehiclePresetMemberIds[index]) return null;
		return member;
	}

	// Меняется только захваченная trip-копия; library/slot recipe не трогаются.
	bool ReassignVehiclePresetSeat(int seatIndex, int memberPosition)
	{
		if (!Replication.IsServer() || !m_VehiclePreset || !IsVehiclePresetCurrent() ||
			(memberPosition >= 0 && !AICF_VehicleShortRecovery.IsCurrentMember(this, VehiclePresetMember(memberPosition)))) return false;
		return m_VehiclePreset.Assign(seatIndex, memberPosition);
	}

	bool CommitBehaviorClockPause(bool paused, int nowMs)
	{
		if (!Replication.IsServer() || !m_VehiclePreset) return false;
		if (paused && m_Phase != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT) return false;
		if (!paused) m_iAbsoluteDeadlineMs += m_BehaviorState.PausedMs(nowMs);
		m_BehaviorState.SetPaused(paused, nowMs);
		return true;
	}

	protected ref AICF_StrategicAssignmentSnapshot m_Assignment;
	protected int m_iTripGeneration;
	protected string m_sOperationId;
	protected string m_sCausationId;
	protected AICF_ETransportTripPhase m_Phase;
	protected AICF_ETransportTripPhase m_PreviousPhase;
	protected int m_iStartedAtMs;
	protected int m_iPhaseStartedAtMs;
	protected int m_iAbsoluteDeadlineMs;
	protected int m_iTransitionCount;
	protected string m_sLastTransitionReason;
	protected string m_sTerminalReason;
	protected ref AICF_VehicleLease m_Lease;
	protected ref AICF_VehicleRequestState m_RequestState;
	protected ref AICF_VehicleBoardingState m_BoardingState;
	protected ref AICF_VehicleMovementState m_MovementState;
	protected ref AICF_VehicleDismountState m_DismountState;
	protected ref AICF_VehicleHandoffState m_HandoffState;
	protected ref AICF_VehicleCleanupState m_CleanupState;
	protected ref AICF_TripRetirementIdentity m_RetirementIdentity;

	void AICF_TransportTrip(
		AICF_StrategicAssignmentSnapshot assignment,
		int tripGeneration,
		string operationId,
		string causationId,
		int nowMs,
		int absoluteDeadlineMs)
	{
		m_Assignment = assignment;
		m_iTripGeneration = tripGeneration;
		m_sOperationId = operationId;
		m_sCausationId = causationId;
		m_Phase = AICF_ETransportTripPhase.WAITING_FOR_SITE;
		m_iStartedAtMs = nowMs;
		m_iPhaseStartedAtMs = nowMs;
		m_iAbsoluteDeadlineMs = absoluteDeadlineMs;
		m_RequestState = new AICF_VehicleRequestState();
		m_BoardingState = new AICF_VehicleBoardingState();
		m_MovementState = new AICF_VehicleMovementState();
		m_DismountState = new AICF_VehicleDismountState();
		m_HandoffState = new AICF_VehicleHandoffState();
		m_CleanupState = new AICF_VehicleCleanupState();
		m_RetirementIdentity = new AICF_TripRetirementIdentity(assignment, tripGeneration, operationId);
	}

	AICF_StrategicAssignmentSnapshot GetAssignment() { return m_Assignment; }
	FactionKey GetFactionKey() { return m_Assignment.GetFactionKey(); }
	int GetSlotId() { return m_Assignment.GetSlotId(); }
	string GetSlotKey() { return m_Assignment.GetSlotKey(); }
	int GetGroupGeneration() { return m_Assignment.GetGroupGeneration(); }
	int GetTripGeneration() { return m_iTripGeneration; }
	string GetOperationId() { return m_sOperationId; }
	string GetCausationId() { return m_sCausationId; }
	AICF_ETransportTripPhase GetPhase() { return m_Phase; }
	AICF_ETransportTripPhase GetPreviousPhase() { return m_PreviousPhase; }
	int GetStartedAtMs() { return m_iStartedAtMs; }
	int GetPhaseStartedAtMs() { return m_iPhaseStartedAtMs; }
	int GetAbsoluteDeadlineMs() { return m_iAbsoluteDeadlineMs + m_MovementState.GetPresetDestinationWaitMs(System.GetTickCount()) + m_BehaviorState.PausedMs(System.GetTickCount()); }
	bool IsPresetWaitingForDestination() { return m_MovementState.IsPresetDestinationWaiting(); }

	bool CommitPresetDestinationWait(bool waiting, int nowMs)
	{
		if (!Replication.IsServer()) return false;
		if (waiting && (m_Phase != AICF_ETransportTripPhase.TRANSIT || !m_VehiclePreset || !IsVehiclePresetCurrent() ||
			m_MovementState.GetRouteGeneration() != 0 || m_MovementState.GetRouteWaypoint() || m_MovementState.GetSupersededRouteWaypoint())) return false;
		if (!waiting) m_iAbsoluteDeadlineMs += m_MovementState.GetPresetDestinationWaitMs(nowMs);
		m_MovementState.SetPresetDestinationWaiting(waiting, nowMs);
		return true;
	}
	int GetTransitionCount() { return m_iTransitionCount; }
	string GetLastTransitionReason() { return m_sLastTransitionReason; }
	string GetTerminalReason() { return m_sTerminalReason; }
	AICF_VehicleLease GetLease() { return m_Lease; }
	bool HasLease() { return m_Lease != null; }
	AICF_VehicleRequestState GetRequestState() { return m_RequestState; }
	AICF_VehicleBoardingState GetBoardingState() { return m_BoardingState; }
	AICF_VehicleMovementState GetMovementState() { return m_MovementState; }
	AICF_VehicleDismountState GetDismountState() { return m_DismountState; }
	AICF_VehicleHandoffState GetHandoffState() { return m_HandoffState; }
	AICF_VehicleCleanupState GetCleanupState() { return m_CleanupState; }

	bool IsValid()
	{
		return m_Assignment && m_Assignment.IsValid() && m_iTripGeneration > 0 &&
			!m_sOperationId.IsEmpty() && !m_sCausationId.IsEmpty() &&
			m_iAbsoluteDeadlineMs > m_iStartedAtMs;
	}

	bool HasAbsentGroupRetirementIdentity()
	{
		return m_RetirementIdentity && m_RetirementIdentity.MatchesAbsentGroup(this) &&
			m_iAbsoluteDeadlineMs > m_iStartedAtMs && !m_sCausationId.IsEmpty();
	}

	AICF_TripRetirementIdentity CaptureOrphanCleanupIdentity(AICF_FactionFleet fleet, AICF_VehicleLease lease, int terminalAt)
	{
		if (!m_RetirementIdentity || !m_RetirementIdentity.CaptureCleanup(this, fleet, lease, terminalAt)) return null;
		return m_RetirementIdentity;
	}

	bool OwnsRetirementIdentity(AICF_TripRetirementIdentity identity) { return identity && identity == m_RetirementIdentity; }
	void RecordCapturedDeadRetirement(AICF_FactionFleet fleet, int observationBudget)
	{
		if (m_RetirementIdentity) m_RetirementIdentity.RecordAllCapturedDead(this, fleet, observationBudget);
	}
	bool HasCapturedDeadRetirementEvidence() { return m_RetirementIdentity && m_RetirementIdentity.HasCapturedDeadEvidence(); }
	void RecordCapturedDeadRetirementCommit(AICF_FactionFleet fleet)
	{
		if (m_RetirementIdentity) m_RetirementIdentity.RecordOrphanTerminalCommit(this, fleet);
	}
	AICF_TripRetirementObservation ObserveCapturedDeadRetirement()
	{
		return new AICF_TripRetirementObservation(m_RetirementIdentity, this);
	}

	bool IsValidForRetirement()
	{
		return IsValid() || HasAbsentGroupRetirementIdentity();
	}

	bool MatchesIdentity(
		FactionKey factionKey,
		int slotId,
		int groupGeneration,
		int tripGeneration)
	{
		return m_Assignment && factionKey == GetFactionKey() && slotId == GetSlotId() &&
			groupGeneration == GetGroupGeneration() && tripGeneration == m_iTripGeneration;
	}

	bool IsCurrent(AICF_StrategicAssignmentSnapshot current)
	{
		return current && current.GetGroup() == m_Assignment.GetGroup() &&
			MatchesIdentity(
				current.GetFactionKey(),
				current.GetSlotId(),
				current.GetGroupGeneration(),
				m_iTripGeneration);
	}

	bool IsTerminal()
	{
		return m_Phase == AICF_ETransportTripPhase.COMPLETE ||
			m_Phase == AICF_ETransportTripPhase.FALLBACK ||
			m_Phase == AICF_ETransportTripPhase.FAILED_CLOSED;
	}

	bool IsDeadlineReached(int nowMs)
	{
		return m_iAbsoluteDeadlineMs > 0 && nowMs >= m_iAbsoluteDeadlineMs + m_MovementState.GetPresetDestinationWaitMs(nowMs) + m_BehaviorState.PausedMs(nowMs);
	}

	bool TryAttachLease(AICF_VehicleLease lease)
	{
		if (m_Lease || !lease || m_Phase != AICF_ETransportTripPhase.SPAWN_COMMIT)
			return false;
		if (!lease.MatchesSlotIdentity(GetFactionKey(), GetSlotId(), GetGroupGeneration()))
			return false;
		if (lease.GetTripGeneration() != m_iTripGeneration)
			return false;
		if (lease.GetState() != AICF_EVehicleLeaseState.RESERVED &&
			lease.GetState() != AICF_EVehicleLeaseState.ACTIVE)
			return false;
		m_Lease = lease;
		return true;
	}

	bool DetachLease(AICF_VehicleLease expected)
	{
		if (!expected || expected != m_Lease)
			return false;
		m_Lease = null;
		return true;
	}

	bool CommitRetarget(AICF_StrategicAssignmentSnapshot assignment, string causationId)
	{
		if (IsTerminal() || !assignment || !assignment.IsValid() || causationId.IsEmpty())
			return false;
		if (!assignment.MatchesCurrent(
			GetFactionKey(),
			GetSlotId(),
			GetGroupGeneration(),
			m_Assignment.GetGroup()))
			return false;
		if (assignment.GetAssignmentRevision() < m_Assignment.GetAssignmentRevision())
			return false;
		if (assignment.GetBaseRevision() < m_Assignment.GetBaseRevision())
			return false;
		m_Assignment = assignment;
		m_sCausationId = causationId;
		return true;
	}

	bool CommitTransition(
		AICF_ETransportTripPhase nextPhase,
		string reason,
		string causationId,
		int nowMs)
	{
		if ((!IsValid() && !(nextPhase == AICF_ETransportTripPhase.FAILED_CLOSED && HasAbsentGroupRetirementIdentity())) || reason.IsEmpty() || causationId.IsEmpty())
			return false;
		if (!CanTransitionTo(nextPhase))
			return false;

		AICF_ETransportTripPhase previousPhase = m_Phase;
		ResetExitedPhase(previousPhase, nextPhase);
		m_PreviousPhase = previousPhase;
		m_Phase = nextPhase;
		m_iPhaseStartedAtMs = nowMs;
		m_iTransitionCount++;
		m_sLastTransitionReason = reason;
		m_sCausationId = causationId;
		if (IsTerminal() && m_sTerminalReason.IsEmpty())
			m_sTerminalReason = reason;
		return true;
	}

	// Pure preflight used by the controller before it executes phase exit
	// effects. It never mutates phase-local or identity state.
	bool CanTransitionTo(AICF_ETransportTripPhase nextPhase)
	{
		if (!IsTransitionAllowedTo(nextPhase))
			return false;
		// WAITING_FOR_SITE is cap-free by construction.
		if (nextPhase == AICF_ETransportTripPhase.WAITING_FOR_SITE && m_Lease)
			return false;
		return true;
	}

	// Matrix-only pure check for transitions whose controller-owned entry
	// precondition (for example cap-free WAITING) must be committed first.
	bool IsTransitionAllowedTo(AICF_ETransportTripPhase nextPhase)
	{
		return (IsValid() || (nextPhase == AICF_ETransportTripPhase.FAILED_CLOSED && HasAbsentGroupRetirementIdentity())) && !IsTerminal() && nextPhase != m_Phase &&
			IsAllowedTransition(m_Phase, nextPhase);
	}

	protected bool IsAllowedTransition(
		AICF_ETransportTripPhase fromPhase,
		AICF_ETransportTripPhase toPhase)
	{
		switch (fromPhase)
		{
		case AICF_ETransportTripPhase.WAITING_FOR_SITE:
				return toPhase == AICF_ETransportTripPhase.SITE_PLANNED ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
			case AICF_ETransportTripPhase.SITE_PLANNED:
				return toPhase == AICF_ETransportTripPhase.APPROACHING_SITE ||
					toPhase == AICF_ETransportTripPhase.WAITING_FOR_SITE ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
			case AICF_ETransportTripPhase.APPROACHING_SITE:
				return toPhase == AICF_ETransportTripPhase.STAGING_CONFIRMED ||
					toPhase == AICF_ETransportTripPhase.WAITING_FOR_SITE ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
			case AICF_ETransportTripPhase.STAGING_CONFIRMED:
				return toPhase == AICF_ETransportTripPhase.SPAWN_COMMIT ||
					toPhase == AICF_ETransportTripPhase.APPROACHING_SITE ||
					toPhase == AICF_ETransportTripPhase.WAITING_FOR_SITE ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
			case AICF_ETransportTripPhase.SPAWN_COMMIT:
				return toPhase == AICF_ETransportTripPhase.WAITING_FOR_SITE ||
					toPhase == AICF_ETransportTripPhase.APPROACHING_SITE ||
					toPhase == AICF_ETransportTripPhase.BOARDING ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
			case AICF_ETransportTripPhase.BOARDING:
				return toPhase == AICF_ETransportTripPhase.TRANSIT ||
					toPhase == AICF_ETransportTripPhase.WAITING_FOR_SITE ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
			case AICF_ETransportTripPhase.TRANSIT:
				return toPhase == AICF_ETransportTripPhase.DISMOUNT ||
					toPhase == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
			case AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT:
				return toPhase == AICF_ETransportTripPhase.TRANSIT ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
			case AICF_ETransportTripPhase.DISMOUNT:
				return toPhase == AICF_ETransportTripPhase.HANDOFF ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
			case AICF_ETransportTripPhase.HANDOFF:
				return toPhase == AICF_ETransportTripPhase.COMPLETE ||
					toPhase == AICF_ETransportTripPhase.FALLBACK ||
					toPhase == AICF_ETransportTripPhase.FAILED_CLOSED;
		}
		return false;
	}

	protected void ResetExitedPhase(
		AICF_ETransportTripPhase exitedPhase,
		AICF_ETransportTripPhase nextPhase)
	{
		bool acquisitionToAcquisition = IsAcquisitionPhase(exitedPhase) &&
			IsAcquisitionPhase(nextPhase);
		if (IsAcquisitionPhase(exitedPhase) && !acquisitionToAcquisition)
			m_RequestState.Reset();
		else if (exitedPhase == AICF_ETransportTripPhase.BOARDING)
			m_BoardingState.Reset();
		else if (exitedPhase == AICF_ETransportTripPhase.TRANSIT)
			m_MovementState.Reset();
		else if (exitedPhase == AICF_ETransportTripPhase.DISMOUNT)
			m_DismountState.Reset();
		// Handoff evidence persists into terminal state so order_restored and
		// clearance_safe remain independently auditable.
	}

	protected bool IsAcquisitionPhase(AICF_ETransportTripPhase phase)
	{
		return phase == AICF_ETransportTripPhase.WAITING_FOR_SITE ||
			phase == AICF_ETransportTripPhase.SITE_PLANNED ||
			phase == AICF_ETransportTripPhase.APPROACHING_SITE ||
			phase == AICF_ETransportTripPhase.STAGING_CONFIRMED ||
			phase == AICF_ETransportTripPhase.SPAWN_COMMIT;
	}
}
