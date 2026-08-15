// Cleanup is an asset lifecycle, not a TransportTrip phase. These outcomes let
// the controller detach/finish a trip as soon as release ownership is accepted,
// while protected clearance and physical cleanup continue independently.
enum AICF_EVehicleCleanupOutcomeKind
{
	REJECTED = 0,
	RELEASE_QUEUED,
	RELEASED_TO_WORLD_POOL,
	RETIREMENT_QUARANTINED,
	RESERVATION_CANCELLED,
	DELETE_REQUESTED,
	DELETE_CONFIRMED,
	RETAINED_FAIL_CLOSED,
	STALE_CALLBACK_CANCELLED,
	NO_ACTION
}

enum AICF_EVehicleReleaseDisposition
{
	FUNCTIONAL_WORLD_POOL = 0,
	DESTRUCTIVE_RETIREMENT
}

class AICF_VehicleCleanupOutcome
{
	protected AICF_EVehicleCleanupOutcomeKind m_Kind;
	protected string m_sReason;
	protected string m_sActionToken;
	protected AICF_WorldPoolAsset m_WorldPoolAsset;
	protected AICF_VehicleRetirementAsset m_RetirementAsset;

	protected void AICF_VehicleCleanupOutcome(
		AICF_EVehicleCleanupOutcomeKind kind,
		string reason,
		string actionToken,
		AICF_WorldPoolAsset worldPoolAsset = null,
		AICF_VehicleRetirementAsset retirementAsset = null)
	{
		m_Kind = kind;
		m_sReason = reason;
		m_sActionToken = actionToken;
		m_WorldPoolAsset = worldPoolAsset;
		m_RetirementAsset = retirementAsset;
	}

	AICF_EVehicleCleanupOutcomeKind GetKind() { return m_Kind; }
	string GetReason() { return m_sReason; }
	string GetActionToken() { return m_sActionToken; }
	AICF_WorldPoolAsset GetWorldPoolAsset() { return m_WorldPoolAsset; }
	AICF_VehicleRetirementAsset GetRetirementAsset() { return m_RetirementAsset; }

	bool IsAccepted()
	{
		return m_Kind == AICF_EVehicleCleanupOutcomeKind.RELEASE_QUEUED ||
			m_Kind == AICF_EVehicleCleanupOutcomeKind.RELEASED_TO_WORLD_POOL ||
			m_Kind == AICF_EVehicleCleanupOutcomeKind.RETIREMENT_QUARANTINED ||
			m_Kind == AICF_EVehicleCleanupOutcomeKind.RESERVATION_CANCELLED;
	}

	bool IsReleaseComplete()
	{
		return m_Kind == AICF_EVehicleCleanupOutcomeKind.RELEASED_TO_WORLD_POOL ||
			m_Kind == AICF_EVehicleCleanupOutcomeKind.RETIREMENT_QUARANTINED ||
			m_Kind == AICF_EVehicleCleanupOutcomeKind.RESERVATION_CANCELLED;
	}

	bool IsTerminalFailure()
	{
		return m_Kind == AICF_EVehicleCleanupOutcomeKind.REJECTED ||
			m_Kind == AICF_EVehicleCleanupOutcomeKind.RETAINED_FAIL_CLOSED;
	}

	static AICF_VehicleCleanupOutcome Rejected(string reason)
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.REJECTED,
			reason,
			"NONE");
	}

	static AICF_VehicleCleanupOutcome Queued(string reason, string actionToken)
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.RELEASE_QUEUED,
			reason,
			actionToken);
	}

	static AICF_VehicleCleanupOutcome Released(
		string reason,
		string actionToken,
		AICF_WorldPoolAsset asset)
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.RELEASED_TO_WORLD_POOL,
			reason,
			actionToken,
			asset);
	}

	static AICF_VehicleCleanupOutcome Quarantined(
		string reason,
		string actionToken,
		AICF_VehicleRetirementAsset asset)
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.RETIREMENT_QUARANTINED,
			reason,
			actionToken,
			null,
			asset);
	}

	static AICF_VehicleCleanupOutcome ReservationCancelled(
		string reason,
		string actionToken)
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.RESERVATION_CANCELLED,
			reason,
			actionToken);
	}

	static AICF_VehicleCleanupOutcome DeleteRequested(string reason, string actionToken)
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.DELETE_REQUESTED,
			reason,
			actionToken);
	}

	static AICF_VehicleCleanupOutcome DeleteConfirmed(string reason, string actionToken)
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.DELETE_CONFIRMED,
			reason,
			actionToken);
	}

	static AICF_VehicleCleanupOutcome Retained(string reason, string actionToken)
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.RETAINED_FAIL_CLOSED,
			reason,
			actionToken);
	}

	static AICF_VehicleCleanupOutcome StaleCancelled(string reason, string actionToken)
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.STALE_CALLBACK_CANCELLED,
			reason,
			actionToken);
	}

	static AICF_VehicleCleanupOutcome NoAction(string reason, string actionToken = "NONE")
	{
		return new AICF_VehicleCleanupOutcome(
			AICF_EVehicleCleanupOutcomeKind.NO_ACTION,
			reason,
			actionToken);
	}
}

class AICF_VehicleCleanupQuery
{
	protected bool m_bTracked;
	protected bool m_bReleasePending;
	protected bool m_bReleaseComplete;
	protected bool m_bClearanceSafe;
	protected bool m_bDeletePending;
	protected bool m_bRetainedFailClosed;
	protected int m_iStableClearMs;
	protected int m_iDeleteAttempts;
	protected int m_iProtectedOccupants;
	protected int m_iNearbyPlayers;
	protected int m_iManagedLogicalOccupants;
	protected int m_iManagedNonAliveLogicalOccupants;
	protected int m_iManagedTransitions;
	protected int m_iManagedInsideBounds;
	protected string m_sBlockerSignature;
	protected string m_sManagedSamples;
	protected string m_sActionToken;
	protected string m_sNextAction;

	void AICF_VehicleCleanupQuery(
		bool tracked,
		bool releaseComplete,
		bool clearanceSafe,
		bool deletePending,
		bool retainedFailClosed,
		int stableClearMs,
		int deleteAttempts,
		int protectedOccupants,
		int nearbyPlayers,
		int managedLogicalOccupants,
		int managedTransitions,
		int managedInsideBounds,
		string blockerSignature,
		string managedSamples,
		string actionToken,
		string nextAction)
	{
		m_bTracked = tracked;
		m_bReleasePending = tracked && !releaseComplete && !retainedFailClosed;
		m_bReleaseComplete = releaseComplete;
		m_bClearanceSafe = clearanceSafe;
		m_bDeletePending = deletePending;
		m_bRetainedFailClosed = retainedFailClosed;
		m_iStableClearMs = stableClearMs;
		m_iDeleteAttempts = deleteAttempts;
		m_iProtectedOccupants = protectedOccupants;
		m_iNearbyPlayers = nearbyPlayers;
		m_iManagedLogicalOccupants = managedLogicalOccupants;
		m_iManagedTransitions = managedTransitions;
		m_iManagedInsideBounds = managedInsideBounds;
		m_sBlockerSignature = blockerSignature;
		m_sManagedSamples = managedSamples;
		m_sActionToken = actionToken;
		m_sNextAction = nextAction;
	}

	bool IsTracked() { return m_bTracked; }
	bool IsReleasePending() { return m_bReleasePending; }
	bool IsReleaseComplete() { return m_bReleaseComplete; }
	bool IsClearanceSafe() { return m_bClearanceSafe; }
	bool IsDeletePending() { return m_bDeletePending; }
	bool IsRetainedFailClosed() { return m_bRetainedFailClosed; }
	int GetStableClearMs() { return m_iStableClearMs; }
	int GetDeleteAttempts() { return m_iDeleteAttempts; }
	int GetProtectedOccupants() { return m_iProtectedOccupants; }
	int GetNearbyPlayers() { return m_iNearbyPlayers; }
	int GetManagedLogicalOccupants() { return m_iManagedLogicalOccupants; }
	int GetManagedNonAliveLogicalOccupants() { return m_iManagedNonAliveLogicalOccupants; }
	int GetManagedTransitions() { return m_iManagedTransitions; }
	int GetManagedInsideBounds() { return m_iManagedInsideBounds; }
	string GetBlockerSignature() { return m_sBlockerSignature; }
	string GetManagedSamples() { return m_sManagedSamples; }
	string GetActionToken() { return m_sActionToken; }
	string GetNextAction() { return m_sNextAction; }
	void SetManagedNonAliveLogicalOccupants(int count)
	{
		m_iManagedNonAliveLogicalOccupants = Math.Max(0, count);
	}

	static AICF_VehicleCleanupQuery NotTracked()
	{
		return new AICF_VehicleCleanupQuery(
			false,
			false,
			false,
			false,
			false,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			"NONE",
			"NONE",
			"NONE",
			"NONE");
	}
}

// Immutable fence passed to every one-shot stop callback. It carries the full
// trip/lease/vehicle identity plus a unique cleanup action token, so a pooled
// asset reused by a later trip is an ordinary stale self-cancel, not an ABA delete.
class AICF_VehicleCleanupFence
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iLeaseGeneration;
	protected int m_iVehicleGeneration;
	protected string m_sVehicleLifecycleId;
	protected EntityID m_EntityId = EntityID.INVALID;
	protected string m_sRplId;
	protected string m_sActionToken;

	bool InitializeFromLease(AICF_VehicleLease lease, string actionToken)
	{
		if (!lease)
			return false;
		m_sFactionKey = lease.GetFactionKey();
		m_iSlotId = lease.GetSlotId();
		m_iGroupGeneration = lease.GetGroupGeneration();
		m_iTripGeneration = lease.GetTripGeneration();
		m_iLeaseGeneration = lease.GetLeaseGeneration();
		m_iVehicleGeneration = lease.GetVehicleGeneration();
		m_sVehicleLifecycleId = lease.GetVehicleLifecycleId();
		m_EntityId = lease.GetEntityId();
		m_sRplId = lease.GetRplId();
		m_sActionToken = actionToken;
		return IsComplete();
	}

	bool InitializeFromSnapshot(AICF_VehicleCleanupSnapshot snapshot, string actionToken)
	{
		if (!snapshot || !snapshot.IsComplete())
			return false;
		m_sFactionKey = snapshot.GetFactionKey();
		m_iSlotId = snapshot.GetSlotId();
		m_iGroupGeneration = snapshot.GetGroupGeneration();
		m_iTripGeneration = snapshot.GetTripGeneration();
		m_iLeaseGeneration = snapshot.GetLeaseGeneration();
		m_iVehicleGeneration = snapshot.GetVehicleGeneration();
		m_sVehicleLifecycleId = snapshot.GetVehicleLifecycleId();
		m_EntityId = snapshot.GetLastEntityId();
		m_sRplId = snapshot.GetLastRplId();
		m_sActionToken = actionToken;
		return IsComplete();
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	int GetGroupGeneration() { return m_iGroupGeneration; }
	int GetTripGeneration() { return m_iTripGeneration; }
	int GetLeaseGeneration() { return m_iLeaseGeneration; }
	int GetVehicleGeneration() { return m_iVehicleGeneration; }
	string GetVehicleLifecycleId() { return m_sVehicleLifecycleId; }
	EntityID GetEntityId() { return m_EntityId; }
	string GetRplId() { return m_sRplId; }
	string GetActionToken() { return m_sActionToken; }

	bool IsComplete()
	{
		return !m_sFactionKey.IsEmpty() && m_iSlotId >= 0 &&
			m_iGroupGeneration > 0 && m_iTripGeneration > 0 &&
			m_iLeaseGeneration > 0 && m_iVehicleGeneration > 0 &&
			!m_sVehicleLifecycleId.IsEmpty() && m_EntityId != EntityID.INVALID &&
			!m_sRplId.IsEmpty() && m_sRplId != "NONE" && !m_sActionToken.IsEmpty();
	}

	bool MatchesLease(AICF_VehicleLease lease)
	{
		return IsComplete() && lease && lease.GetFactionKey() == m_sFactionKey &&
			lease.GetSlotId() == m_iSlotId &&
			lease.GetGroupGeneration() == m_iGroupGeneration &&
			lease.GetTripGeneration() == m_iTripGeneration &&
			lease.GetLeaseGeneration() == m_iLeaseGeneration &&
			lease.GetVehicleGeneration() == m_iVehicleGeneration &&
			lease.GetVehicleLifecycleId() == m_sVehicleLifecycleId &&
			lease.GetEntityId() == m_EntityId && lease.GetRplId() == m_sRplId;
	}

	bool MatchesSnapshot(AICF_VehicleCleanupSnapshot snapshot)
	{
		return IsComplete() && snapshot && snapshot.IsComplete() &&
			snapshot.GetFactionKey() == m_sFactionKey &&
			snapshot.GetSlotId() == m_iSlotId &&
			snapshot.GetGroupGeneration() == m_iGroupGeneration &&
			snapshot.GetTripGeneration() == m_iTripGeneration &&
			snapshot.GetLeaseGeneration() == m_iLeaseGeneration &&
			snapshot.GetVehicleGeneration() == m_iVehicleGeneration &&
			snapshot.GetVehicleLifecycleId() == m_sVehicleLifecycleId &&
			snapshot.GetLastEntityId() == m_EntityId &&
			snapshot.GetLastRplId() == m_sRplId;
	}

	bool MatchesFence(AICF_VehicleCleanupFence other)
	{
		return other && IsComplete() && other.IsComplete() &&
			other.GetFactionKey() == m_sFactionKey && other.GetSlotId() == m_iSlotId &&
			other.GetGroupGeneration() == m_iGroupGeneration &&
			other.GetTripGeneration() == m_iTripGeneration &&
			other.GetLeaseGeneration() == m_iLeaseGeneration &&
			other.GetVehicleGeneration() == m_iVehicleGeneration &&
			other.GetVehicleLifecycleId() == m_sVehicleLifecycleId &&
			other.GetEntityId() == m_EntityId && other.GetRplId() == m_sRplId &&
			other.GetActionToken() == m_sActionToken;
	}
}

class AICF_VehicleCleanupScan
{
	bool m_bSafe;
	bool m_bPlayerScanAvailable;
	int m_iProtectedOccupants;
	int m_iPlayerTransitions;
	int m_iNearbyPlayers;
	int m_iManagedLogicalOccupants;
	int m_iManagedNonAliveLogicalOccupants;
	int m_iManagedTransitions;
	int m_iManagedInsideBounds;
	string m_sGlobalSamples;
	string m_sManagedSamples;
	string m_sBlockerSignature;

	void Reset()
	{
		m_bSafe = false;
		m_bPlayerScanAvailable = false;
		m_iProtectedOccupants = 0;
		m_iPlayerTransitions = 0;
		m_iNearbyPlayers = 0;
		m_iManagedLogicalOccupants = 0;
		m_iManagedNonAliveLogicalOccupants = 0;
		m_iManagedTransitions = 0;
		m_iManagedInsideBounds = 0;
		m_sGlobalSamples = "NONE";
		m_sManagedSamples = "NONE";
		m_sBlockerSignature = "NOT_SCANNED";
	}
}

// Manager-owned mutable record. No fact here is duplicated into Trip or Fleet:
// Fleet owns cap/registry, Lease/Snapshot own identity, and this job owns only
// pending clearance, retirement/delete evidence and callback scheduling.
class AICF_VehicleCleanupJob
{
	ref AICF_FactionFleet m_Fleet;
	ref AICF_VehicleLease m_Lease;
	SCR_AIGroup m_Group;
	ref AICF_WorldPoolAsset m_WorldPoolAsset;
	ref AICF_VehicleRetirementAsset m_RetirementAsset;
	ref AICF_VehicleCleanupSnapshot m_Snapshot;
	ref AICF_VehicleCleanupState m_State;
	ref AICF_VehicleCleanupFence m_Fence;
	ref AICF_VehicleCleanupScan m_Scan;
	AICF_EVehicleReleaseDisposition m_Disposition;
	string m_sTrigger;
	string m_sOperationId;
	string m_sCausationId;
	string m_sActionToken;
	string m_sRetirementReason;
	string m_sFailClosedReason;
	string m_sManagedExactRecoveryFenceReason;
	string m_sNonAliveDetachFenceReason;
	string m_sLastReportedBlocker;
	int m_iFirstDeleteRequestedAtMs;
	int m_iLastAuditAtMs;
	int m_iStopStartedAtMs;
	int m_iManagedRecoveryAttempts;
	int m_iLastManagedRecoveryAtMs;
	int m_iManagedExactRecoveryAttempts;
	int m_iLastManagedExactRecoveryAtMs;
	int m_iNonAliveDetachAttempts;
	int m_iLastNonAliveDetachAtMs;
	int m_iProtectedClearanceDeadlineAtMs;
	int m_iProtectedClearanceDeadlineTriggeredAtMs;
	bool m_bReleaseComplete;
	bool m_bClearanceSafe;
	bool m_bRetirementRequested;
	bool m_bQuarantined;
	bool m_bStopMode;
	bool m_bStopPollScheduled;
	bool m_bFailClosed;
	bool m_bCompleted;
	bool m_bControllerAcknowledged;
	bool m_bReleaseReported;
	bool m_bIdentityFailureReported;
	bool m_bAcceptanceFailureReported;
	bool m_bStopRetainedReported;
	bool m_bManagedRecoveryBudgetReported;
	bool m_bManagedExactRecoveryBudgetReported;
	bool m_bNonAliveDetachBudgetReported;
	bool m_bProtectedClearanceDeadlineReported;
	bool m_bProtectedClearanceFinalOutcomeReported;

	void InitializeCommon(
		AICF_FactionFleet fleet,
		AICF_VehicleLease lease,
		SCR_AIGroup group,
		AICF_EVehicleReleaseDisposition disposition,
		string trigger,
		string operationId,
		string causationId,
		string actionToken,
		int nowMs,
		int absoluteDeadlineMs)
	{
		m_Fleet = fleet;
		m_Lease = lease;
		m_Group = group;
		m_Disposition = disposition;
		m_sTrigger = trigger;
		m_sOperationId = operationId;
		m_sCausationId = causationId;
		m_sActionToken = actionToken;
		m_State = new AICF_VehicleCleanupState();
		m_State.Begin(nowMs, absoluteDeadlineMs, 3);
		m_iProtectedClearanceDeadlineAtMs = absoluteDeadlineMs;
		m_Fence = new AICF_VehicleCleanupFence();
		m_Fence.InitializeFromLease(lease, actionToken);
		m_Scan = new AICF_VehicleCleanupScan();
	}
}

class AICF_VehicleCleanupFleetAudit
{
	ref AICF_FactionFleet m_Fleet;
	string m_sLastSignature;
	int m_iLastReportedAtMs;
}

// Sole cleanup composition root. It never restores an order, changes a Trip
// phase, or calls another vehicle flow. Accepting a release request is enough
// for the controller to finish/detach the Trip; all physical work below remains
// independently fenced and safety-gated.
class AICF_VehicleCleanupManager
{
	protected static const int STABLE_CLEAR_MS = 5000;
	protected static const int DELETE_RETRY_INTERVAL_MS = 2000;
	protected static const int DELETE_CONFIRM_TIMEOUT_MS = 10000;
	protected static const int DELETE_MAX_ATTEMPTS = 3;
	protected static const int DEFERRED_AUDIT_INTERVAL_MS = 30000;
	protected static const int STOP_POLL_MS = 1000;
	protected static const int STOP_TIMEOUT_MS = 60000;
	protected static const int MANAGED_CLEARANCE_RECOVERY_MAX_ATTEMPTS = 3;
	protected static const int MANAGED_CLEARANCE_RECOVERY_INTERVAL_MS = 2000;
	protected static const int MANAGED_EXACT_RECOVERY_MAX_ATTEMPTS = 3;
	protected static const int MANAGED_EXACT_RECOVERY_INTERVAL_MS = 2000;
	protected static const int NON_ALIVE_DETACH_MAX_ATTEMPTS = 3;
	protected static const int NON_ALIVE_DETACH_INTERVAL_MS = 1000;
	// The threshold starts recovery. This immutable grace bounds externally
	// fenced clearance without ever authorizing an unsafe release or delete.
	protected static const int PROTECTED_CLEARANCE_TERMINAL_GRACE_MS = 30000;
	protected static const float MANAGED_CLEARANCE_MARGIN_METERS = 1.5;
	protected static const float PLAYER_PROTECTION_RADIUS_METERS = 15.0;

	protected ref AICF_Stage3Config m_Config;
	protected SCR_GameModeCampaign m_Campaign;
	protected ref AICF_VehicleWatchdog m_Watchdog;
	protected AICF_VehicleAcceptanceMonitor m_AcceptanceMonitor;
	protected ref array<ref AICF_VehicleCleanupJob> m_aJobs = {};
	protected ref array<ref AICF_VehicleCleanupFleetAudit> m_aFleetAudits = {};
	protected int m_iNextActionToken;

	void AICF_VehicleCleanupManager(
		AICF_Stage3Config config,
		SCR_GameModeCampaign campaign)
	{
		m_Config = config;
		m_Campaign = campaign;
		m_Watchdog = new AICF_VehicleWatchdog();
	}

	// The composition root may attach the observation-only acceptance monitor.
	// Replaying unreported fail-closed snapshots makes failures that happened
	// after their Trip ended visible without making diagnostics a state owner.
	void SetAcceptanceMonitor(AICF_VehicleAcceptanceMonitor acceptanceMonitor)
	{
		m_AcceptanceMonitor = acceptanceMonitor;
		if (!m_AcceptanceMonitor)
			return;
		foreach (AICF_VehicleCleanupJob job : m_aJobs)
		{
			if (job && job.m_bFailClosed)
				ObserveLateCleanupFailure(job, job.m_sFailClosedReason);
		}
	}

	// Physical assets that are no longer counted as an ACTIVE lease or as a
	// player-available world-pool asset, but are deliberately retained while
	// cleanup/delete confirmation remains pending or fail-closed.
	int GetRetainedPhysicalCount(FactionKey factionKey)
	{
		int count;
		foreach (AICF_VehicleCleanupJob job : m_aJobs)
		{
			if (!job || job.m_bCompleted || !job.m_Fence ||
				job.m_Fence.GetFactionKey() != factionKey)
			{
				continue;
			}
			// When delete confirmation failed, the immutable snapshot remains the
			// authoritative conservative evidence that the entity is retained.
			if (job.m_bFailClosed && job.m_Snapshot)
			{
				count++;
				continue;
			}
			if (job.m_RetirementAsset && job.m_RetirementAsset.GetVehicle())
			{
				count++;
				continue;
			}
			if (job.m_bQuarantined && job.m_WorldPoolAsset &&
				job.m_WorldPoolAsset.GetVehicle())
			{
				count++;
				continue;
			}
			if (!job.m_Lease || !job.m_Lease.GetVehicle())
				continue;
			AICF_EVehicleLeaseState state = job.m_Lease.GetState();
			if (state == AICF_EVehicleLeaseState.RELEASE_PENDING ||
				state == AICF_EVehicleLeaseState.FAILED_CLOSED)
			{
				count++;
			}
		}
		return count;
	}

	AICF_VehicleCleanupOutcome QueueLeaseRelease(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleLease lease,
		AICF_EVehicleReleaseDisposition disposition,
		string trigger,
		string causationId,
		int nowMs)
	{
		if (!IsAuthority() || !trip || !trip.IsValid() || !fleet || !lease ||
			trigger.IsEmpty() || causationId.IsEmpty() || trip.GetLease() != lease ||
			!trip.MatchesIdentity(
				lease.GetFactionKey(),
				lease.GetSlotId(),
				lease.GetGroupGeneration(),
				lease.GetTripGeneration()) ||
			fleet.GetFactionKey() != lease.GetFactionKey() ||
			lease.GetState() != AICF_EVehicleLeaseState.ACTIVE)
		{
			return AICF_VehicleCleanupOutcome.Rejected("LEASE_RELEASE_IDENTITY_INVALID");
		}

		AICF_VehicleCleanupJob job = FindJobByLease(lease);
		if (!job)
		{
			string actionToken = BuildActionToken(lease);
			job = new AICF_VehicleCleanupJob();
			job.InitializeCommon(
				fleet,
				lease,
				trip.GetAssignment().GetGroup(),
				disposition,
				trigger,
				trip.GetOperationId(),
				causationId,
				actionToken,
				nowMs,
				nowMs + Math.Max(STABLE_CLEAR_MS, m_Config.GetCleanupDelayMs()));
			if (!job.m_Fence || !job.m_Fence.IsComplete())
				return AICF_VehicleCleanupOutcome.Rejected("LEASE_RELEASE_FENCE_INVALID");
			m_aJobs.Insert(job);
			ReportReleaseQueued(job);
		}
		else if (!job.m_Fence.MatchesLease(lease))
		{
			return AICF_VehicleCleanupOutcome.Retained(
				"LEASE_RELEASE_FENCE_MISMATCH",
				job.m_sActionToken);
		}

		return ProcessLeaseRelease(job, nowMs);
	}

	AICF_VehicleCleanupOutcome CancelEmptyReservation(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleLease lease,
		string causationId)
	{
		if (!IsAuthority() || !trip || !fleet || !lease || causationId.IsEmpty() ||
			trip.GetLease() != lease || lease.GetState() != AICF_EVehicleLeaseState.RESERVED ||
			!lease.MatchesTripIdentity(
				trip.GetFactionKey(),
				trip.GetSlotId(),
				trip.GetGroupGeneration(),
				trip.GetTripGeneration()) ||
			fleet.GetFactionKey() != lease.GetFactionKey())
		{
			return AICF_VehicleCleanupOutcome.Rejected("RESERVATION_CANCEL_IDENTITY_INVALID");
		}
		string token = string.Format(
			"%1:CLEANUP:RESERVATION:%2",
			trip.GetOperationId(),
			lease.GetLeaseGeneration());
		if (!fleet.ReleaseEmptyReservation(lease))
			return AICF_VehicleCleanupOutcome.Retained("RESERVATION_CANCEL_FAILED", token);
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_RESERVATION_CANCELLED",
			string.Format(
				"faction=%1 slot=%2 group_generation=%3 trip_generation=%4 lease_generation=%5 operation_id=%6 causation_id=%7 action_token=%8",
				trip.GetFactionKey(),
				trip.GetSlotKey(),
				trip.GetGroupGeneration(),
				trip.GetTripGeneration(),
				lease.GetLeaseGeneration(),
				trip.GetOperationId(),
				causationId,
				token));
		return AICF_VehicleCleanupOutcome.ReservationCancelled("RESERVATION_CANCELLED", token);
	}

	void UpdateFleet(AICF_FactionFleet fleet, int nowMs)
	{
		if (!IsAuthority() || !fleet)
			return;
		EnsureWorldPoolJobs(fleet, nowMs);
		for (int jobIndex = m_aJobs.Count() - 1; jobIndex >= 0; jobIndex--)
		{
			AICF_VehicleCleanupJob job = m_aJobs[jobIndex];
			if (!job || job.m_Fleet != fleet || job.m_bCompleted)
				continue;
			if (job.m_bStopMode || job.m_bFailClosed)
				continue;
			if (!job.m_bReleaseComplete)
				ProcessLeaseRelease(job, nowMs);
			if (job.m_bReleaseComplete)
				AuditReleasedAsset(job, nowMs);
		}
		SelectOldestSafeOverflow(fleet, nowMs);
		for (int retirementIndex = m_aJobs.Count() - 1; retirementIndex >= 0; retirementIndex--)
		{
			AICF_VehicleCleanupJob retirementJob = m_aJobs[retirementIndex];
			if (!retirementJob || retirementJob.m_Fleet != fleet ||
				retirementJob.m_bStopMode || retirementJob.m_bCompleted ||
				retirementJob.m_bFailClosed || !retirementJob.m_bRetirementRequested)
			{
				continue;
			}
			ProcessRetirement(retirementJob, nowMs);
		}
		ReportSoftOverflow(fleet, nowMs);
		PruneCompletedJobs();
	}

	AICF_VehicleCleanupQuery QueryLease(AICF_VehicleLease lease)
	{
		return BuildQuery(FindJobByLease(lease), System.GetTickCount());
	}

	AICF_VehicleCleanupQuery QueryAsset(AICF_WorldPoolAsset asset)
	{
		return BuildQuery(FindJobByWorldPoolAsset(asset), System.GetTickCount());
	}

	AICF_VehicleCleanupQuery QueryRetirementAsset(AICF_VehicleRetirementAsset asset)
	{
		return BuildQuery(FindJobByRetirementAsset(asset), System.GetTickCount());
	}

	// Completed lease jobs remain immutable tombstones until the controller has
	// observed the result. This prevents a long scheduler interval from turning a
	// confirmed cleanup into an ambiguous NotTracked response.
	bool AcknowledgeLeaseRelease(AICF_VehicleLease lease, string actionToken)
	{
		AICF_VehicleCleanupJob job = FindJobByLease(lease);
		if (!IsAuthority() || !job || !job.m_bReleaseComplete || actionToken.IsEmpty() ||
			actionToken != job.m_sActionToken ||
			!job.m_Fence.MatchesLease(lease))
		{
			return false;
		}
		job.m_bControllerAcknowledged = true;
		return true;
	}

	// Retained ownership is not a release.  It only proves that this exact
	// manager job and Fleet keep the fail-closed physical asset after the Trip
	// drops its reference.  The lease remains FAILED_CLOSED and cap-active.
	bool AcknowledgeRetainedLease(AICF_VehicleLease lease, string actionToken)
	{
		AICF_VehicleCleanupJob job = FindJobByLease(lease);
		if (!IsAuthority() || !lease || !job || !job.m_Fleet || !job.m_Fence ||
			job.m_Lease != lease || !job.m_bFailClosed ||
			job.m_bReleaseComplete || lease.GetState() != AICF_EVehicleLeaseState.FAILED_CLOSED ||
			!FleetContainsLease(job.m_Fleet, lease) ||
			job.m_Fleet.FindLeaseForSlot(lease.GetSlotId(), lease.GetGroupGeneration()) != lease ||
			actionToken.IsEmpty() || actionToken != job.m_sActionToken ||
			!job.m_Fence.MatchesLease(lease))
		{
			return false;
		}
		job.m_bControllerAcknowledged = true;
		return true;
	}

	AICF_VehicleCleanupOutcome BeginStopLease(
		AICF_TransportTrip trip,
		AICF_FactionFleet fleet,
		AICF_VehicleLease lease,
		int nowMs)
	{
		if (!IsAuthority() || !trip || !trip.IsValid() || !fleet || !lease ||
			(lease.GetState() != AICF_EVehicleLeaseState.ACTIVE &&
				lease.GetState() != AICF_EVehicleLeaseState.RESERVED) ||
			!lease.MatchesTripIdentity(
				trip.GetFactionKey(),
				trip.GetSlotId(),
				trip.GetGroupGeneration(),
				trip.GetTripGeneration()) ||
			fleet.GetFactionKey() != lease.GetFactionKey())
		{
			return AICF_VehicleCleanupOutcome.Rejected("STOP_LEASE_IDENTITY_INVALID");
		}
		AICF_VehicleCleanupJob existing = FindJobByLease(lease);
		AICF_VehicleCleanupOutcome outcome;
		if (existing)
		{
			outcome = AICF_VehicleCleanupOutcome.Queued(
				"EXISTING_RELEASE_PROMOTED_TO_STOP",
				existing.m_sActionToken);
		}
		else if (lease && lease.GetState() == AICF_EVehicleLeaseState.RESERVED)
		{
			outcome = CancelEmptyReservation(
				trip,
				fleet,
				lease,
				trip.GetOperationId() + ":STOP");
			return outcome;
		}
		else
		{
			outcome = QueueLeaseRelease(
				trip,
				fleet,
				lease,
				AICF_EVehicleReleaseDisposition.DESTRUCTIVE_RETIREMENT,
				"COORDINATOR_STOP",
				trip.GetOperationId() + ":STOP",
				nowMs);
		}
		if (!outcome || !outcome.IsAccepted())
			return outcome;
		AICF_VehicleCleanupJob job = FindJobByLease(lease);
		if (!job)
			return AICF_VehicleCleanupOutcome.Rejected("STOP_JOB_MISSING");
		PromoteToStop(job, nowMs);
		return outcome;
	}

	void BeginStopFleet(AICF_FactionFleet fleet, int nowMs)
	{
		if (!IsAuthority() || !fleet)
			return;
		EnsureWorldPoolJobs(fleet, nowMs);
		foreach (AICF_VehicleCleanupJob job : m_aJobs)
		{
			if (!job || job.m_Fleet != fleet || job.m_bCompleted || job.m_bFailClosed)
				continue;
			if (!job.m_bReleaseComplete && !job.m_Lease)
				continue;
			PromoteToStop(job, nowMs);
		}
	}

	protected bool IsAuthority()
	{
		return Replication.IsServer() && m_Config && m_Campaign &&
			m_Campaign.IsMaster() && GetGame() && GetGame().GetWorld();
	}

	protected string BuildActionToken(AICF_VehicleLease lease)
	{
		m_iNextActionToken++;
		return string.Format(
			"CLEANUP:%1:%2:%3:%4",
			lease.GetVehicleLifecycleId(),
			lease.GetLeaseGeneration(),
			lease.GetVehicleGeneration(),
			m_iNextActionToken);
	}

	protected string BuildSnapshotActionToken(AICF_VehicleCleanupSnapshot snapshot)
	{
		m_iNextActionToken++;
		return string.Format(
			"CLEANUP:%1:%2:%3:%4",
			snapshot.GetVehicleLifecycleId(),
			snapshot.GetLeaseGeneration(),
			snapshot.GetVehicleGeneration(),
			m_iNextActionToken);
	}

	protected AICF_VehicleCleanupOutcome ProcessLeaseRelease(
		AICF_VehicleCleanupJob job,
		int nowMs)
	{
		if (!job || job.m_bFailClosed)
			return AICF_VehicleCleanupOutcome.Retained("RELEASE_JOB_FAIL_CLOSED", "NONE");
		if (job.m_bReleaseComplete)
		{
			if (job.m_WorldPoolAsset)
				return AICF_VehicleCleanupOutcome.Released(
					"ALREADY_RELEASED",
					job.m_sActionToken,
					job.m_WorldPoolAsset);
			return AICF_VehicleCleanupOutcome.Quarantined(
				"ALREADY_QUARANTINED",
				job.m_sActionToken,
				job.m_RetirementAsset);
		}
		if (!IsJobCurrent(job))
			return RetainFailClosed(job, "LEASE_RELEASE_STALE_IDENTITY", "NONE", nowMs);

		Vehicle vehicle;
		string actualRplId;
		string identityFailure;
		if (!ResolveExpectedVehicle(job, vehicle, actualRplId, identityFailure))
			return RetainFailClosed(job, identityFailure, actualRplId, nowMs);

		TryDetachNonAliveExactOccupants(job, vehicle, nowMs);
		InspectCleanupSafety(job, vehicle, job.m_Scan);
		int stableClearMs = job.m_State.ObserveSafeClear(
			job.m_Scan.m_bSafe,
			job.m_Scan.m_sBlockerSignature,
			nowMs);
		job.m_bClearanceSafe = false;
		if (!job.m_Scan.m_bSafe || stableClearMs < STABLE_CLEAR_MS)
		{
			AuditDeferred(job, "LEASE_RELEASE_CLEARANCE", stableClearMs, nowMs);
			if (job.m_State.GetAbsoluteDeadlineMs() > 0 &&
				nowMs >= job.m_State.GetAbsoluteDeadlineMs())
			{
				return HandleProtectedClearanceDeadline(
					job,
					vehicle,
					actualRplId,
					nowMs);
			}
			return AICF_VehicleCleanupOutcome.Queued(
				"PROTECTED_CLEARANCE_PENDING",
				job.m_sActionToken);
		}

		// Immediate repeat scan immediately before the ownership/cap release.
		InspectCleanupSafety(job, vehicle, job.m_Scan);
		if (!job.m_Scan.m_bSafe)
		{
			job.m_State.ObserveSafeClear(false, job.m_Scan.m_sBlockerSignature, nowMs);
			AuditDeferred(job, "LEASE_RELEASE_RESCAN_BLOCKED", 0, nowMs);
			return AICF_VehicleCleanupOutcome.Queued(
				"PROTECTED_CLEARANCE_RESCAN_BLOCKED",
				job.m_sActionToken);
		}
		if (!ResolveExpectedVehicle(job, vehicle, actualRplId, identityFailure))
			return RetainFailClosed(job, identityFailure, actualRplId, nowMs);

		job.m_bClearanceSafe = true;
		if (IsVehicleUnusable(vehicle))
			job.m_Disposition = AICF_EVehicleReleaseDisposition.DESTRUCTIVE_RETIREMENT;
		if (job.m_Disposition == AICF_EVehicleReleaseDisposition.DESTRUCTIVE_RETIREMENT)
			return ReleaseToRetirement(job, vehicle, nowMs);
		return ReleaseToWorldPool(job, vehicle, nowMs);
	}

	// The release deadline starts bounded recovery; it is never permission to
	// release or delete an occupied asset. Managed blockers get queue reset plus
	// exact native exit/relocation. External safety fences get a short immutable
	// grace. Exhaustion transfers the still-protected asset to manager-owned,
	// cap-held fail-closed retention so the Trip itself can terminate.
	protected AICF_VehicleCleanupOutcome HandleProtectedClearanceDeadline(
		AICF_VehicleCleanupJob job,
		Vehicle vehicle,
		string actualRplId,
		int nowMs)
	{
		if (!job || !job.m_Scan || !vehicle)
		{
			return AICF_VehicleCleanupOutcome.NoAction(
				"PROTECTED_CLEARANCE_HANDLER_INVALID",
				"NONE");
		}
		ReportProtectedClearanceDeadlineExceeded(job, nowMs);
		int deadlineAgeMs = Math.Max(
			0,
			nowMs - job.m_iProtectedClearanceDeadlineAtMs);
		int terminalAtMs = job.m_iProtectedClearanceDeadlineAtMs +
			PROTECTED_CLEARANCE_TERMINAL_GRACE_MS;
		bool managedBlocked = job.m_Scan.m_iManagedLogicalOccupants > 0 ||
			job.m_Scan.m_iManagedTransitions > 0 ||
			job.m_Scan.m_iManagedInsideBounds > 0;
		// The global occupant count includes managed AI. It is a foreign blocker
		// only when it exceeds the exact current-group logical occupant count.
		bool foreignProtected = job.m_Scan.m_iProtectedOccupants >
			job.m_Scan.m_iManagedLogicalOccupants;
		bool playerProtected = !job.m_Scan.m_bPlayerScanAvailable ||
			job.m_Scan.m_iPlayerTransitions > 0 ||
			job.m_Scan.m_iNearbyPlayers > 0 ||
			job.m_Scan.m_sGlobalSamples.Contains("position_unknown");
		if (nowMs >= terminalAtMs)
		{
			string terminalReason = "PROTECTED_CLEARANCE_GRACE_EXPIRED";
			if (playerProtected)
				terminalReason = "PROTECTED_CLEARANCE_PLAYER_GRACE_EXPIRED";
			else if (foreignProtected)
				terminalReason = "PROTECTED_CLEARANCE_FOREIGN_GRACE_EXPIRED";
			else if (!job.m_sManagedExactRecoveryFenceReason.IsEmpty())
				terminalReason = "PROTECTED_CLEARANCE_HIDDEN_FENCE_GRACE_EXPIRED:" +
					job.m_sManagedExactRecoveryFenceReason;
			else if (managedBlocked)
				terminalReason = "PROTECTED_CLEARANCE_MANAGED_GRACE_EXPIRED";
			return RetainFailClosed(
				job,
				terminalReason + ":" + job.m_Scan.m_sBlockerSignature,
				actualRplId,
				nowMs);
		}
		if (playerProtected || foreignProtected)
		{
			string protectionReason = "PROTECTED_CLEARANCE_PLAYER_POLL";
			if (!playerProtected)
				protectionReason = "PROTECTED_CLEARANCE_FOREIGN_OCCUPANT_POLL";
			AuditDeferred(job, protectionReason, 0, nowMs);
			return AICF_VehicleCleanupOutcome.Queued(
				protectionReason,
				job.m_sActionToken);
		}
		if (!managedBlocked)
		{
			AuditDeferred(job, "PROTECTED_CLEARANCE_STABLE_WINDOW", 0, nowMs);
			return AICF_VehicleCleanupOutcome.Queued(
				"PROTECTED_CLEARANCE_STABLE_WINDOW",
				job.m_sActionToken);
		}
		if (!job.m_Group ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(job.m_Group))
		{
			return RetainFailClosed(
				job,
				"MANAGED_GROUP_AUTHORITY_LOST:" + job.m_Scan.m_sBlockerSignature,
				actualRplId,
				nowMs);
		}
		if (job.m_iManagedRecoveryAttempts >= MANAGED_CLEARANCE_RECOVERY_MAX_ATTEMPTS)
		{
			if (!job.m_bManagedRecoveryBudgetReported)
			{
				job.m_bManagedRecoveryBudgetReported = true;
				string exhaustedDetails = BuildJobIdentity(job);
				exhaustedDetails += string.Format(
					" attempts=%1 maximum_attempts=%2 action=ESCALATE_EXACT_HIDDEN_CLEARANCE blocker_signature=%3 managed_samples=[%4]",
					job.m_iManagedRecoveryAttempts,
					MANAGED_CLEARANCE_RECOVERY_MAX_ATTEMPTS,
					job.m_Scan.m_sBlockerSignature,
					job.m_Scan.m_sManagedSamples);
				exhaustedDetails += string.Format(
					" deadline_at_ms=%1 deadline_age_ms=%2 terminal_at_ms=%3",
					job.m_iProtectedClearanceDeadlineAtMs,
					deadlineAgeMs,
					terminalAtMs);
				AICF_Stage3Diagnostics.Warning(
					"VEHICLE_CLEANUP_MANAGED_RECOVERY_EXHAUSTED",
					exhaustedDetails);
			}
			return TryExactManagedCleanupRecovery(
				job,
				vehicle,
				actualRplId,
				nowMs);
		}
		if (job.m_iLastManagedRecoveryAtMs > 0 &&
			nowMs - job.m_iLastManagedRecoveryAtMs < MANAGED_CLEARANCE_RECOVERY_INTERVAL_MS)
		{
			return AICF_VehicleCleanupOutcome.Queued(
				"MANAGED_QUEUE_RESET_INTERVAL",
				job.m_sActionToken);
		}
		job.m_iManagedRecoveryAttempts++;
		job.m_iLastManagedRecoveryAtMs = nowMs;
		int interrupted = m_Watchdog.ResetGroupVehicleActions(job.m_Group);
		job.m_State.ObserveSafeClear(false, job.m_Scan.m_sBlockerSignature, nowMs);
		string recoveryDetails = BuildJobIdentity(job);
		recoveryDetails += string.Format(
			" attempt=%1 maximum_attempts=%2 interrupted_actions=%3 player_scan_available=%4 protected_occupants=%5 player_transitions=%6 nearby_players=%7",
			job.m_iManagedRecoveryAttempts,
			MANAGED_CLEARANCE_RECOVERY_MAX_ATTEMPTS,
			interrupted,
			job.m_Scan.m_bPlayerScanAvailable,
			job.m_Scan.m_iProtectedOccupants,
			job.m_Scan.m_iPlayerTransitions,
			job.m_Scan.m_iNearbyPlayers);
		recoveryDetails += string.Format(
			" managed_logical=%1 managed_transitions=%2 managed_inside_bounds=%3 action=RESET_GROUP_VEHICLE_ACTIONS_THEN_CONTINUE_SCAN managed_samples=[%4]",
			job.m_Scan.m_iManagedLogicalOccupants,
			job.m_Scan.m_iManagedTransitions,
			job.m_Scan.m_iManagedInsideBounds,
			job.m_Scan.m_sManagedSamples);
		recoveryDetails += string.Format(
			" deadline_at_ms=%1 deadline_age_ms=%2 terminal_at_ms=%3",
			job.m_iProtectedClearanceDeadlineAtMs,
			deadlineAgeMs,
			terminalAtMs);
		AICF_Stage3Diagnostics.Warning(
			"VEHICLE_CLEANUP_MANAGED_RECOVERY",
			recoveryDetails);
		return AICF_VehicleCleanupOutcome.Queued(
			"MANAGED_QUEUE_RESET_APPLIED",
			job.m_sActionToken);
	}

	protected AICF_VehicleCleanupOutcome TryExactManagedCleanupRecovery(
		AICF_VehicleCleanupJob job,
		Vehicle vehicle,
		string actualRplId,
		int nowMs)
	{
		if (!job)
		{
			return AICF_VehicleCleanupOutcome.NoAction(
				"MANAGED_EXACT_RECOVERY_JOB_MISSING",
				"NONE");
		}
		if (!job.m_Group || !vehicle ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(job.m_Group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) ||
			!m_Config.GetHiddenRecoveryEnabled())
		{
			job.m_sManagedExactRecoveryFenceReason =
				"MANAGED_EXACT_RECOVERY_DISABLED_OR_INVALID";
			AuditDeferred(job, "MANAGED_EXACT_RECOVERY_DISABLED_OR_INVALID", 0, nowMs);
			return AICF_VehicleCleanupOutcome.Queued(
				"MANAGED_EXACT_RECOVERY_DISABLED_OR_INVALID",
				job.m_sActionToken);
		}
		if (job.m_iManagedExactRecoveryAttempts >= MANAGED_EXACT_RECOVERY_MAX_ATTEMPTS)
		{
			int deadlineAgeMs = Math.Max(
				0,
				nowMs - job.m_iProtectedClearanceDeadlineAtMs);
			if (!job.m_bManagedExactRecoveryBudgetReported)
			{
				job.m_bManagedExactRecoveryBudgetReported = true;
				string exactExhaustedDetails = BuildJobIdentity(job);
				exactExhaustedDetails += string.Format(
					" attempts=%1 maximum_attempts=%2 deadline_at_ms=%3 deadline_age_ms=%4 action=TRANSFER_RETAINED_FAIL_CLOSED retention=CAP_HELD_PROTECTED blocker_signature=%5 managed_samples=[%6]",
					job.m_iManagedExactRecoveryAttempts,
					MANAGED_EXACT_RECOVERY_MAX_ATTEMPTS,
					job.m_iProtectedClearanceDeadlineAtMs,
					deadlineAgeMs,
					job.m_Scan.m_sBlockerSignature,
					job.m_Scan.m_sManagedSamples);
				AICF_Stage3Diagnostics.Warning(
					"VEHICLE_CLEANUP_EXACT_RECOVERY_EXHAUSTED",
					exactExhaustedDetails);
			}
			return RetainFailClosed(
				job,
				"MANAGED_EXACT_RECOVERY_EXHAUSTED:" + job.m_Scan.m_sBlockerSignature,
				actualRplId,
				nowMs);
		}
		if (job.m_iLastManagedExactRecoveryAtMs > 0 &&
			nowMs - job.m_iLastManagedExactRecoveryAtMs < MANAGED_EXACT_RECOVERY_INTERVAL_MS)
		{
			return AICF_VehicleCleanupOutcome.Queued(
				"MANAGED_EXACT_RECOVERY_INTERVAL",
				job.m_sActionToken);
		}
		float nearestPlayerMeters;
		string rejectionReason;
		float hiddenRadiusMeters = m_Config.GetHiddenRecoveryPlayerRadiusMeters();
		if (!m_Watchdog.CanApplyHiddenRecovery(
			vehicle.GetOrigin(),
			vehicle.GetOrigin(),
			hiddenRadiusMeters,
			nearestPlayerMeters,
			rejectionReason))
		{
			job.m_sManagedExactRecoveryFenceReason = rejectionReason;
			AuditDeferred(
				job,
				"MANAGED_EXACT_RECOVERY_PLAYER_FENCED:" + rejectionReason,
				0,
				nowMs);
			return AICF_VehicleCleanupOutcome.Queued(
				"MANAGED_EXACT_RECOVERY_PLAYER_FENCED:" + rejectionReason,
				job.m_sActionToken);
		}

		job.m_sManagedExactRecoveryFenceReason = string.Empty;
		job.m_iManagedExactRecoveryAttempts++;
		job.m_iLastManagedExactRecoveryAtMs = nowMs;
		int interrupted = m_Watchdog.ResetGroupVehicleActions(job.m_Group);
		int forced;
		int relocated;
		int stillBlocked;
		int positionFailures;
		ForceAndRelocateExactManagedMembers(
			job,
			vehicle,
			hiddenRadiusMeters,
			nowMs,
			forced,
			relocated,
			stillBlocked,
			positionFailures);
		InspectCleanupSafety(job, vehicle, job.m_Scan);
		int stableClearMs = job.m_State.ObserveSafeClear(
			job.m_Scan.m_bSafe,
			job.m_Scan.m_sBlockerSignature,
			nowMs);
		int deadlineAgeMs = Math.Max(
			0,
			nowMs - job.m_iProtectedClearanceDeadlineAtMs);
		int terminalAtMs = job.m_iProtectedClearanceDeadlineAtMs +
			PROTECTED_CLEARANCE_TERMINAL_GRACE_MS;
		string exactRecoveryDetails = BuildJobIdentity(job);
		exactRecoveryDetails += string.Format(
			" attempt=%1 maximum_attempts=%2 interrupted_actions=%3 forced=%4 relocated=%5 still_blocked=%6 position_failures=%7",
			job.m_iManagedExactRecoveryAttempts,
			MANAGED_EXACT_RECOVERY_MAX_ATTEMPTS,
			interrupted,
			forced,
			relocated,
			stillBlocked,
			positionFailures);
		exactRecoveryDetails += string.Format(
			" hidden_radius_m=%1 nearest_player_m=%2 immediate_clear=%3 stable_clear_ms=%4 action=CONTINUE_FIVE_SECOND_PROTECTED_SCAN blocker_signature=%5 managed_samples=[%6]",
			hiddenRadiusMeters,
			nearestPlayerMeters,
			job.m_Scan.m_bSafe,
			stableClearMs,
			job.m_Scan.m_sBlockerSignature,
			job.m_Scan.m_sManagedSamples);
		exactRecoveryDetails += string.Format(
			" deadline_at_ms=%1 deadline_age_ms=%2 terminal_at_ms=%3",
			job.m_iProtectedClearanceDeadlineAtMs,
			deadlineAgeMs,
			terminalAtMs);
		AICF_Stage3Diagnostics.Warning(
			"VEHICLE_CLEANUP_EXACT_RECOVERY",
			exactRecoveryDetails);
		return AICF_VehicleCleanupOutcome.Queued(
			"MANAGED_EXACT_RECOVERY_APPLIED",
			job.m_sActionToken);
	}

	// A burning/destroyed vehicle can leave incapacitated or dead exact members
	// linked after the live-AI action fence correctly stops accepting them.  This
	// path performs only a native exact-compartment detach: no corpse teleport,
	// no foreign occupant mutation, and no mutation at all unless the strong
	// player proximity/LOS fence passes.
	protected void TryDetachNonAliveExactOccupants(
		AICF_VehicleCleanupJob job,
		Vehicle vehicle,
		int nowMs)
	{
		if (!job || !job.m_Group || !vehicle || !m_Config ||
			!m_Config.GetHiddenRecoveryEnabled() ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(job.m_Group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle))
		{
			return;
		}
		array<AIAgent> agents = {};
		job.m_Group.GetAgents(agents);
		int candidates;
		foreach (AIAgent candidateAgent : agents)
		{
			if (!candidateAgent || candidateAgent.GetParentGroup() != job.m_Group)
				continue;
			ChimeraCharacter candidate = ChimeraCharacter.Cast(
				candidateAgent.GetControlledEntity());
			if (!candidate || AICF_GroupRuntime.IsAliveCharacter(candidate) ||
				!IsExactCleanupOccupantOfVehicle(candidate, vehicle))
			{
				continue;
			}
			candidates++;
		}
		if (candidates <= 0)
		{
			job.m_sNonAliveDetachFenceReason = string.Empty;
			return;
		}
		if (job.m_iNonAliveDetachAttempts >= NON_ALIVE_DETACH_MAX_ATTEMPTS)
		{
			if (!job.m_bNonAliveDetachBudgetReported)
			{
				job.m_bNonAliveDetachBudgetReported = true;
				string budgetDetails = BuildJobIdentity(job);
				budgetDetails += string.Format(
					" candidates=%1 attempts=%2 maximum_attempts=%3 action=CONTINUE_PROTECTED_CLEARANCE_NO_UNSAFE_MUTATION",
					candidates,
					job.m_iNonAliveDetachAttempts,
					NON_ALIVE_DETACH_MAX_ATTEMPTS);
				AICF_Stage3Diagnostics.Warning(
					"VEHICLE_CLEANUP_NON_ALIVE_DETACH_EXHAUSTED",
					budgetDetails);
			}
			return;
		}
		if (job.m_iLastNonAliveDetachAtMs > 0 &&
			nowMs - job.m_iLastNonAliveDetachAtMs < NON_ALIVE_DETACH_INTERVAL_MS)
		{
			return;
		}
		float nearestPlayerMeters;
		string rejectionReason;
		float hiddenRadiusMeters = m_Config.GetHiddenRecoveryPlayerRadiusMeters();
		if (!m_Watchdog.CanApplyHiddenRecovery(
			vehicle.GetOrigin(),
			vehicle.GetOrigin(),
			hiddenRadiusMeters,
			nearestPlayerMeters,
			rejectionReason))
		{
			if (job.m_sNonAliveDetachFenceReason != rejectionReason)
			{
				job.m_sNonAliveDetachFenceReason = rejectionReason;
				string fencedDetails = BuildJobIdentity(job);
				fencedDetails += string.Format(
					" candidates=%1 hidden_radius_m=%2 nearest_player_m=%3 fence_reason=%4 action=DEFER_WITHOUT_MUTATION",
					candidates,
					hiddenRadiusMeters,
					nearestPlayerMeters,
					rejectionReason);
				AICF_Stage3Diagnostics.Info(
					"VEHICLE_CLEANUP_NON_ALIVE_DETACH_DEFERRED",
					fencedDetails);
			}
			return;
		}

		job.m_sNonAliveDetachFenceReason = string.Empty;
		job.m_iNonAliveDetachAttempts++;
		job.m_iLastNonAliveDetachAtMs = nowMs;
		int requested;
		int immediate;
		int rejected;
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != job.m_Group)
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character || AICF_GroupRuntime.IsAliveCharacter(character) ||
				!IsExactCleanupOccupantOfVehicle(character, vehicle))
			{
				continue;
			}
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			BaseCompartmentSlot compartment;
			if (access)
				compartment = access.GetCompartment();
			bool exactOwner = compartment && compartment.GetVehicle() == vehicle &&
				compartment.GetOccupant() == character;
			bool authoritySafe = m_Watchdog.IsAuthoritativeNonPlayerCharacter(character);
			bool ejectRequested;
			bool ejectImmediate;
			if (exactOwner && authoritySafe)
				ejectRequested = compartment.EjectOccupant(true, false, ejectImmediate, true);
			bool linkedAfter = IsExactCleanupOccupantOfVehicle(character, vehicle);
			if (ejectRequested)
				requested++;
			if (!linkedAfter)
				immediate++;
			if ((!ejectRequested && linkedAfter) || !exactOwner || !authoritySafe)
				rejected++;
			CharacterControllerComponent controller = character.GetCharacterController();
			ECharacterLifeState lifeState;
			string lifeStateName = "NO_CONTROLLER";
			if (controller)
			{
				lifeState = controller.GetLifeState();
				lifeStateName = typename.EnumToString(ECharacterLifeState, lifeState);
			}
			string memberDetails = BuildJobIdentity(job);
			memberDetails += string.Format(
				" member=%1 alive=0 life_state=%2 exact_owner=%3 authority_safe=%4 eject_requested=%5 eject_immediate=%6 linked_after=%7",
				character.GetID(),
				lifeStateName,
				exactOwner,
				authoritySafe,
				ejectRequested,
				ejectImmediate,
				linkedAfter);
			memberDetails += string.Format(
				" attempt=%1 maximum_attempts=%2 hidden_radius_m=%3 nearest_player_m=%4 player_fence=PASSED",
				job.m_iNonAliveDetachAttempts,
				NON_ALIVE_DETACH_MAX_ATTEMPTS,
				hiddenRadiusMeters,
				nearestPlayerMeters);
			AICF_Stage35Diagnostics.Info(
				"VEHICLE_CLEANUP_NON_ALIVE_EXACT_MEMBER",
				memberDetails);
		}
		string resultDetails = BuildJobIdentity(job);
		resultDetails += string.Format(
			" candidates=%1 eject_requested=%2 immediate_unlinked=%3 rejected=%4 attempt=%5 maximum_attempts=%6",
			candidates,
			requested,
			immediate,
			rejected,
			job.m_iNonAliveDetachAttempts,
			NON_ALIVE_DETACH_MAX_ATTEMPTS);
		resultDetails += string.Format(
			" hidden_radius_m=%1 nearest_player_m=%2 player_fence=PASSED action=CONTINUE_EXACT_CLEARANCE_SCAN",
			hiddenRadiusMeters,
			nearestPlayerMeters);
		AICF_Stage3Diagnostics.Warning(
			"VEHICLE_CLEANUP_NON_ALIVE_DETACH",
			resultDetails);
	}

	protected void ForceAndRelocateExactManagedMembers(
		AICF_VehicleCleanupJob job,
		Vehicle vehicle,
		float hiddenRadiusMeters,
		int nowMs,
		out int forced,
		out int relocated,
		out int stillBlocked,
		out int positionFailures)
	{
		forced = 0;
		relocated = 0;
		stillBlocked = 0;
		positionFailures = 0;
		int deadlineAgeMs = Math.Max(
			0,
			nowMs - job.m_iProtectedClearanceDeadlineAtMs);
		vector boundsMin;
		vector boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);
		float clearanceRadius = ResolveManagedClearanceRadius(boundsMin, boundsMax);
		array<AIAgent> agents = {};
		job.m_Group.GetAgents(agents);
		int directionIndex;
		foreach (AIAgent agent : agents)
		{
			if (!IsExactAliveCleanupMember(job.m_Group, agent))
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access)
				continue;
			vector localOrigin = vehicle.CoordToLocal(character.GetOrigin());
			bool insideBounds = IsInsideManagedClearanceBounds(
				localOrigin,
				boundsMin,
				boundsMax);
			access.InterruptVehicleActionQueue(true, true, true);
			bool linked = CompartmentAccessComponent.GetVehicleIn(character) == vehicle;
			bool transitioning = (linked || insideBounds) &&
				(access.IsGettingIn() || access.IsGettingOut());
			if (linked || transitioning)
			{
				BaseCompartmentSlot compartment = access.GetCompartment();
				bool exactOwner = compartment && compartment.GetVehicle() == vehicle &&
					compartment.GetOccupant() == character;
				bool useEject = exactOwner &&
					job.m_iManagedExactRecoveryAttempts > 1;
				bool directAccepted;
				vector exitTransform[4];
				if (!useEject && access.FindSuitableTeleportLocation(exitTransform))
					directAccepted = access.GetOutVehicle_NoDoor(
						exitTransform,
						false,
						false,
						true);
				if (!useEject && !directAccepted)
					directAccepted = access.GetOutVehicle(
						EGetOutType.TELEPORT,
						-1,
						ECloseDoorAfterActions.INVALID,
						false,
						true);
				bool ejectRequested;
				bool ejectImmediate;
				bool ejectAttempted;
				if (exactOwner && (useEject || !directAccepted))
				{
					ejectAttempted = true;
					ejectRequested = compartment.EjectOccupant(
						true,
						false,
						ejectImmediate,
						true);
				}
				if (directAccepted || ejectRequested)
					forced++;

				bool linkedAfterExit =
					CompartmentAccessComponent.GetVehicleIn(character) == vehicle;
				bool transitioningAfterExit = linkedAfterExit &&
					(access.IsGettingIn() || access.IsGettingOut());
				int compartmentSlot = -1;
				int compartmentManager = -1;
				if (compartment)
				{
					compartmentSlot = compartment.GetCompartmentSlotID();
					compartmentManager = compartment.GetCompartmentMgrID();
				}
				string exitMethod = "NONE";
				if (directAccepted)
					exitMethod = "DIRECT_TELEPORT_EXIT";
				if (ejectAttempted)
					exitMethod = "EJECT_ON_THE_SPOT";
				string memberDetails = BuildJobIdentity(job);
				memberDetails += string.Format(
					" member=%1 compartment_slot=%2 compartment_manager=%3 exact_owner_valid=%4 method=%5 direct_accepted=%6 eject_attempted=%7 eject_requested=%8 eject_immediate=%9",
					character.GetID(),
					compartmentSlot,
					compartmentManager,
					exactOwner,
					exitMethod,
					directAccepted,
					ejectAttempted,
					ejectRequested,
					ejectImmediate);
				memberDetails += string.Format(
					" linked_before=%1 transitioning_before=%2 linked_after=%3 transitioning_after=%4 immediate_unlink=%5 attempt=%6 deadline_at_ms=%7 deadline_age_ms=%8",
					linked,
					transitioning,
					linkedAfterExit,
					transitioningAfterExit,
					!linkedAfterExit && !transitioningAfterExit,
					job.m_iManagedExactRecoveryAttempts,
					job.m_iProtectedClearanceDeadlineAtMs,
					deadlineAgeMs);
				AICF_Stage35Diagnostics.Info(
					"VEHICLE_CLEANUP_EXACT_MEMBER",
					memberDetails);
			}

			linked = CompartmentAccessComponent.GetVehicleIn(character) == vehicle;
			transitioning = (linked || insideBounds) &&
				(access.IsGettingIn() || access.IsGettingOut());
			if (linked || transitioning)
			{
				stillBlocked++;
				continue;
			}
			localOrigin = vehicle.CoordToLocal(character.GetOrigin());
			if (!IsInsideManagedClearanceBounds(localOrigin, boundsMin, boundsMax))
				continue;
			vector safePosition;
			if (!FindManagedClearancePosition(
				vehicle,
				character,
				boundsMin,
				boundsMax,
				clearanceRadius,
				directionIndex,
				safePosition))
			{
				positionFailures++;
				stillBlocked++;
				continue;
			}
			directionIndex++;
			float nearestPlayerMeters;
			string rejectionReason;
			if (!m_Watchdog.CanApplyHiddenRecovery(
				character.GetOrigin(),
				safePosition,
				hiddenRadiusMeters,
				nearestPlayerMeters,
				rejectionReason))
			{
				stillBlocked++;
				continue;
			}
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character) ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle))
			{
				stillBlocked++;
				continue;
			}
			vector transform[4];
			character.GetWorldTransform(transform);
			transform[3] = safePosition;
			character.Teleport(transform);
			relocated++;
		}
	}

	protected bool FindManagedClearancePosition(
		Vehicle vehicle,
		IEntity entity,
		vector boundsMin,
		vector boundsMax,
		float clearanceRadius,
		int directionIndex,
		out vector safePosition)
	{
		vector localOrigin = vehicle.CoordToLocal(entity.GetOrigin());
		vector direction = Vector(localOrigin[0], 0, localOrigin[2]);
		if (direction.LengthSq() < 0.01)
		{
			switch (directionIndex % 4)
			{
				case 0: direction = "1 0 0"; break;
				case 1: direction = "-1 0 0"; break;
				case 2: direction = "0 0 1"; break;
				default: direction = "0 0 -1"; break;
			}
		}
		direction.Normalize();
		for (int attempt; attempt < 4; attempt++)
		{
			vector center = vehicle.CoordToParent(
				direction * (clearanceRadius + attempt * 3.0));
			if (!SCR_WorldTools.FindEmptyTerrainPosition(
				safePosition,
				center,
				3.0,
				0.75,
				2.0))
			{
				continue;
			}
			if (IsInsideManagedClearanceBounds(
				vehicle.CoordToLocal(safePosition),
				boundsMin,
				boundsMax) ||
				ChimeraWorldUtils.TryGetWaterSurfaceSimple(vehicle.GetWorld(), safePosition))
			{
				continue;
			}
			return true;
		}
		return false;
	}

	protected bool IsInsideManagedClearanceBounds(
		vector localOrigin,
		vector boundsMin,
		vector boundsMax)
	{
		return localOrigin[0] >= boundsMin[0] - MANAGED_CLEARANCE_MARGIN_METERS &&
			localOrigin[0] <= boundsMax[0] + MANAGED_CLEARANCE_MARGIN_METERS &&
			localOrigin[1] >= boundsMin[1] - MANAGED_CLEARANCE_MARGIN_METERS &&
			localOrigin[1] <= boundsMax[1] + MANAGED_CLEARANCE_MARGIN_METERS &&
			localOrigin[2] >= boundsMin[2] - MANAGED_CLEARANCE_MARGIN_METERS &&
			localOrigin[2] <= boundsMax[2] + MANAGED_CLEARANCE_MARGIN_METERS;
	}

	protected float ResolveManagedClearanceRadius(vector boundsMin, vector boundsMax)
	{
		return Math.Max(
			Math.Max(Math.AbsFloat(boundsMin[0]), Math.AbsFloat(boundsMax[0])),
			Math.Max(Math.AbsFloat(boundsMin[2]), Math.AbsFloat(boundsMax[2]))) + 3.0;
	}

	protected bool IsExactAliveCleanupMember(SCR_AIGroup group, AIAgent agent)
	{
		return group && agent && agent.GetParentGroup() == group &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(
				agent.GetControlledEntity());
	}

	protected bool IsExactCleanupOccupantOfVehicle(
		ChimeraCharacter character,
		Vehicle vehicle)
	{
		if (!character || !vehicle)
			return false;
		if (CompartmentAccessComponent.GetVehicleIn(character) == vehicle)
			return true;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return false;
		BaseCompartmentSlot compartment = access.GetCompartment();
		return compartment && compartment.GetVehicle() == vehicle &&
			compartment.GetOccupant() == character;
	}

	protected AICF_VehicleCleanupOutcome ReleaseToWorldPool(
		AICF_VehicleCleanupJob job,
		Vehicle vehicle,
		int nowMs)
	{
		AICF_WorldPoolAsset asset;
		if (!job.m_Fleet.ReleaseLeaseAt(
			job.m_Lease,
			true,
			job.m_sTrigger,
			vehicle.GetOrigin(),
			nowMs,
			asset) || !asset || !asset.IsValid())
		{
			return RetainFailClosed(job, "FUNCTIONAL_POOL_RELEASE_FAILED", "NONE", nowMs);
		}
		job.m_WorldPoolAsset = asset;
		job.m_Snapshot = asset.GetCleanupSnapshot();
		job.m_State = asset.GetCleanupState();
		job.m_State.Begin(nowMs, 0, DELETE_MAX_ATTEMPTS);
		job.m_bReleaseComplete = true;
		if (!job.m_Fence.MatchesSnapshot(job.m_Snapshot))
			return RetainFailClosed(job, "FUNCTIONAL_POOL_SNAPSHOT_MISMATCH", "NONE", nowMs);
		ReportWorldPoolReleased(job);
		return AICF_VehicleCleanupOutcome.Released(
			"FUNCTIONAL_WORLD_POOL",
			job.m_sActionToken,
			asset);
	}

	protected AICF_VehicleCleanupOutcome ReleaseToRetirement(
		AICF_VehicleCleanupJob job,
		Vehicle vehicle,
		int nowMs)
	{
		AICF_VehicleRetirementAsset asset;
		if (!job.m_Fleet.RetireLeaseAt(
			job.m_Lease,
			true,
			job.m_sTrigger,
			vehicle.GetOrigin(),
			nowMs,
			asset) || !asset || !asset.IsValid())
		{
			return RetainFailClosed(job, "RETIREMENT_QUARANTINE_FAILED", "NONE", nowMs);
		}
		job.m_RetirementAsset = asset;
		job.m_Snapshot = asset.GetCleanupSnapshot();
		job.m_State = asset.GetCleanupState();
		job.m_State.Begin(nowMs, 0, DELETE_MAX_ATTEMPTS);
		job.m_bReleaseComplete = true;
		job.m_bQuarantined = true;
		job.m_bRetirementRequested = true;
		job.m_sRetirementReason = "LEASE_UNUSABLE_OR_EXPLICIT_RETIREMENT";
		if (!job.m_Fence.MatchesSnapshot(job.m_Snapshot))
			return RetainFailClosed(job, "RETIREMENT_SNAPSHOT_MISMATCH", "NONE", nowMs);
		ReportRetirementQuarantined(job);
		return AICF_VehicleCleanupOutcome.Quarantined(
			"DESTRUCTIVE_RETIREMENT_QUARANTINE",
			job.m_sActionToken,
			asset);
	}

	protected void EnsureWorldPoolJobs(AICF_FactionFleet fleet, int nowMs)
	{
		for (int assetIndex; assetIndex < fleet.GetWorldPoolCount(); assetIndex++)
		{
			AICF_WorldPoolAsset asset = fleet.GetWorldPoolAsset(assetIndex);
			if (!asset || FindJobByWorldPoolAsset(asset))
				continue;
			AICF_VehicleCleanupSnapshot snapshot = asset.GetCleanupSnapshot();
			if (!snapshot || !snapshot.IsComplete())
				continue;
			AICF_VehicleCleanupJob job = new AICF_VehicleCleanupJob();
			job.m_Fleet = fleet;
			job.m_WorldPoolAsset = asset;
			job.m_Snapshot = snapshot;
			job.m_State = asset.GetCleanupState();
			job.m_State.Begin(nowMs, 0, DELETE_MAX_ATTEMPTS);
			job.m_Fence = new AICF_VehicleCleanupFence();
			job.m_sActionToken = BuildSnapshotActionToken(snapshot);
			job.m_Fence.InitializeFromSnapshot(snapshot, job.m_sActionToken);
			job.m_Scan = new AICF_VehicleCleanupScan();
			job.m_Disposition = AICF_EVehicleReleaseDisposition.FUNCTIONAL_WORLD_POOL;
			job.m_sTrigger = snapshot.GetCleanupTrigger();
			job.m_sOperationId = "CLEANUP:" + snapshot.GetVehicleLifecycleId();
			job.m_sCausationId = "WORLD_POOL_DISCOVERY";
			job.m_bReleaseComplete = true;
			job.m_bClearanceSafe = true;
			m_aJobs.Insert(job);
		}
	}

	protected void AuditReleasedAsset(AICF_VehicleCleanupJob job, int nowMs)
	{
		if (!IsJobCurrent(job))
		{
			CancelStaleJob(job, "WORLD_POOL_ASSET_REBOUND");
			return;
		}
		Vehicle vehicle;
		string actualRplId;
		string failure;
		if (!ResolveExpectedVehicle(job, vehicle, actualRplId, failure))
		{
			if (failure == "ENTITY_ABSENT")
				ConfirmAbsentAsset(job, "EXTERNAL_ENTITY_ABSENT");
			else
				RetainFailClosed(job, failure, actualRplId, nowMs);
			return;
		}
		if (IsVehicleUnusable(vehicle))
			MarkRetirement(job, "WORLD_POOL_UNUSABLE");
	}

	protected void SelectOldestSafeOverflow(AICF_FactionFleet fleet, int nowMs)
	{
		int poolLimit = m_Config.GetAbandonedWorldPoolPerFaction();
		int pendingRemovals = CountPendingPoolRemovals(fleet);
		int needed = Math.Max(0, fleet.GetWorldPoolCount() - poolLimit - pendingRemovals);
		while (needed > 0)
		{
			AICF_VehicleCleanupJob candidate = FindOldestSafePoolCandidate(fleet, nowMs);
			if (!candidate)
				return;
			MarkRetirement(candidate, "WORLD_POOL_CAPACITY");
			needed--;
		}
	}

	protected int CountPendingPoolRemovals(AICF_FactionFleet fleet)
	{
		int count;
		foreach (AICF_VehicleCleanupJob job : m_aJobs)
		{
			if (job && job.m_Fleet == fleet && job.m_WorldPoolAsset &&
				!job.m_bQuarantined && job.m_bRetirementRequested && !job.m_bCompleted)
			{
				count++;
			}
		}
		return count;
	}

	protected AICF_VehicleCleanupJob FindOldestSafePoolCandidate(
		AICF_FactionFleet fleet,
		int nowMs)
	{
		AICF_VehicleCleanupJob oldest;
		int oldestReleaseAtMs;
		for (int assetIndex; assetIndex < fleet.GetWorldPoolCount(); assetIndex++)
		{
			AICF_WorldPoolAsset asset = fleet.GetWorldPoolAsset(assetIndex);
			AICF_VehicleCleanupJob job = FindJobByWorldPoolAsset(asset);
			if (!job || job.m_bRetirementRequested || job.m_bFailClosed || job.m_bCompleted)
				continue;
			Vehicle vehicle;
			string actualRplId;
			string failure;
			if (!ResolveExpectedVehicle(job, vehicle, actualRplId, failure))
				continue;
			InspectCleanupSafety(job, vehicle, job.m_Scan);
			if (!job.m_Scan.m_bSafe)
			{
				AuditDeferred(job, "WORLD_POOL_OVERFLOW_PROTECTED", 0, nowMs);
				continue;
			}
			int releaseAtMs = job.m_Snapshot.GetReleaseAtMs();
			if (!oldest || releaseAtMs < oldestReleaseAtMs ||
				(releaseAtMs == oldestReleaseAtMs &&
					job.m_Fence.GetVehicleLifecycleId().Compare(
						oldest.m_Fence.GetVehicleLifecycleId()) < 0))
			{
				oldest = job;
				oldestReleaseAtMs = releaseAtMs;
			}
		}
		return oldest;
	}

	protected void MarkRetirement(AICF_VehicleCleanupJob job, string reason)
	{
		if (!job || job.m_bRetirementRequested)
			return;
		job.m_bRetirementRequested = true;
		job.m_sRetirementReason = reason;
		job.m_State.ObserveSafeClear(false, "RETIREMENT_STARTED", System.GetTickCount());
	}

	protected AICF_VehicleCleanupOutcome ProcessRetirement(
		AICF_VehicleCleanupJob job,
		int nowMs)
	{
		if (job.m_State.IsDeleteConfirmationPending())
			return ProcessDeleteConfirmation(job, nowMs);

		Vehicle vehicle;
		string actualRplId;
		string failure;
		if (!ResolveExpectedVehicle(job, vehicle, actualRplId, failure))
		{
			if (failure == "ENTITY_ABSENT")
				return ConfirmAbsentAsset(job, "ENTITY_ABSENT_BEFORE_DELETE");
			return RetainFailClosed(job, failure, actualRplId, nowMs);
		}

		// First protected scan establishes a continuous five-second window.
		InspectCleanupSafety(job, vehicle, job.m_Scan);
		int stableClearMs = job.m_State.ObserveSafeClear(
			job.m_Scan.m_bSafe,
			job.m_Scan.m_sBlockerSignature,
			nowMs);
		if (!job.m_Scan.m_bSafe || stableClearMs < STABLE_CLEAR_MS)
		{
			AuditDeferred(job, job.m_sRetirementReason, stableClearMs, nowMs);
			return AICF_VehicleCleanupOutcome.NoAction(
				"RETIREMENT_CLEARANCE_PENDING",
				job.m_sActionToken);
		}

		if (!QuarantinePoolAsset(job, nowMs))
			return AICF_VehicleCleanupOutcome.Retained(
				"POOL_QUARANTINE_FAILED",
				job.m_sActionToken);

		// Mandatory immediate second scan immediately before destructive delete.
		InspectCleanupSafety(job, vehicle, job.m_Scan);
		if (!job.m_Scan.m_bSafe)
		{
			job.m_State.ObserveSafeClear(false, job.m_Scan.m_sBlockerSignature, nowMs);
			AuditDeferred(job, "PRE_DELETE_RESCAN_BLOCKED", 0, nowMs);
			return AICF_VehicleCleanupOutcome.NoAction(
				"PRE_DELETE_RESCAN_BLOCKED",
				job.m_sActionToken);
		}
		if (!ResolveExpectedVehicle(job, vehicle, actualRplId, failure))
			return RetainFailClosed(job, failure, actualRplId, nowMs);
		if (!job.m_State.BeginDeleteAttempt(nowMs))
			return RetainFailClosed(job, "DELETE_ATTEMPTS_EXHAUSTED", actualRplId, nowMs);
		if (job.m_iFirstDeleteRequestedAtMs <= 0)
			job.m_iFirstDeleteRequestedAtMs = nowMs;
		if (!DeleteExpectedEntity(job, vehicle))
			return RetainFailClosed(job, "DELETE_AUTHORITY_OR_IDENTITY_REJECTED", actualRplId, nowMs);
		ReportDeleteRequested(job, false);
		job.m_State.ObserveSafeClear(false, "DELETE_CONFIRMATION_PENDING", nowMs);
		return AICF_VehicleCleanupOutcome.DeleteRequested(
			job.m_sRetirementReason,
			job.m_sActionToken);
	}

	protected AICF_VehicleCleanupOutcome ProcessDeleteConfirmation(
		AICF_VehicleCleanupJob job,
		int nowMs)
	{
		Vehicle vehicle;
		string actualRplId;
		string failure;
		if (!ResolveExpectedVehicle(job, vehicle, actualRplId, failure))
		{
			if (failure == "ENTITY_ABSENT")
				return ConfirmDelete(job, "AUTHORITY_DELETE_CONFIRMED");
			return RetainFailClosed(job, failure, actualRplId, nowMs);
		}
		if (nowMs - job.m_iFirstDeleteRequestedAtMs >= DELETE_CONFIRM_TIMEOUT_MS)
			return RetainFailClosed(job, "VEHICLE_DELETE_NOT_CONFIRMED", actualRplId, nowMs);
		if (nowMs - job.m_State.GetLastDeleteAttemptAtMs() < DELETE_RETRY_INTERVAL_MS)
			return AICF_VehicleCleanupOutcome.NoAction(
				"DELETE_CONFIRMATION_PENDING",
				job.m_sActionToken);

		InspectCleanupSafety(job, vehicle, job.m_Scan);
		int stableClearMs = job.m_State.ObserveSafeClear(
			job.m_Scan.m_bSafe,
			job.m_Scan.m_sBlockerSignature,
			nowMs);
		if (!job.m_Scan.m_bSafe || stableClearMs < STABLE_CLEAR_MS)
		{
			AuditDeferred(job, "DELETE_RETRY_CLEARANCE", stableClearMs, nowMs);
			return AICF_VehicleCleanupOutcome.NoAction(
				"DELETE_RETRY_CLEARANCE_PENDING",
				job.m_sActionToken);
		}
		InspectCleanupSafety(job, vehicle, job.m_Scan);
		if (!job.m_Scan.m_bSafe)
		{
			job.m_State.ObserveSafeClear(false, job.m_Scan.m_sBlockerSignature, nowMs);
			return AICF_VehicleCleanupOutcome.NoAction(
				"DELETE_RETRY_RESCAN_BLOCKED",
				job.m_sActionToken);
		}
		if (!ResolveExpectedVehicle(job, vehicle, actualRplId, failure))
			return RetainFailClosed(job, failure, actualRplId, nowMs);
		if (!job.m_State.BeginDeleteAttempt(nowMs))
			return RetainFailClosed(job, "DELETE_ATTEMPTS_EXHAUSTED", actualRplId, nowMs);
		if (!DeleteExpectedEntity(job, vehicle))
			return RetainFailClosed(job, "DELETE_AUTHORITY_OR_IDENTITY_REJECTED", actualRplId, nowMs);
		ReportDeleteRequested(job, true);
		job.m_State.ObserveSafeClear(false, "DELETE_CONFIRMATION_PENDING", nowMs);
		return AICF_VehicleCleanupOutcome.DeleteRequested(
			"DELETE_RETRY",
			job.m_sActionToken);
	}

	protected bool QuarantinePoolAsset(AICF_VehicleCleanupJob job, int nowMs)
	{
		if (!job.m_WorldPoolAsset || job.m_bQuarantined)
			return true;
		if (!FleetContainsWorldPoolAsset(job.m_Fleet, job.m_WorldPoolAsset) ||
			!job.m_Fleet.RemoveWorldPoolAsset(job.m_WorldPoolAsset))
		{
			RetainFailClosed(job, "WORLD_POOL_QUARANTINE_OWNERSHIP_FAILED", "NONE", nowMs);
			return false;
		}
		job.m_bQuarantined = true;
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_WORLD_POOL_RETIREMENT_QUARANTINED",
			BuildJobIdentity(job) + " reason=" + job.m_sRetirementReason +
			" ai_cap_reserved=0 player_available=0");
		return true;
	}

	protected void InspectCleanupSafety(
		AICF_VehicleCleanupJob job,
		Vehicle vehicle,
		AICF_VehicleCleanupScan scan)
	{
		scan.Reset();
		// Player enumeration is performed by InspectProtectedCleanupUse. Treat an
		// unavailable PlayerManager as an unsafe/incomplete scan, never as clear.
		PlayerManager playerManager;
		if (GetGame())
			playerManager = GetGame().GetPlayerManager();
		scan.m_bPlayerScanAvailable = playerManager != null;
		bool globalClear = m_Watchdog.InspectProtectedCleanupUse(
			vehicle,
			PLAYER_PROTECTION_RADIUS_METERS,
			scan.m_iProtectedOccupants,
			scan.m_iPlayerTransitions,
			scan.m_iNearbyPlayers,
			scan.m_sGlobalSamples);
		bool managedClear = true;
		if (job.m_Group)
		{
			managedClear = m_Watchdog.InspectProtectedMemberDismountClearance(
				job.m_Group,
				vehicle,
				scan.m_iManagedLogicalOccupants,
				scan.m_iManagedTransitions,
				scan.m_iManagedInsideBounds,
				scan.m_iManagedNonAliveLogicalOccupants,
				scan.m_sManagedSamples);
		}
		scan.m_bSafe = scan.m_bPlayerScanAvailable && globalClear && managedClear;
		scan.m_sBlockerSignature = string.Format(
			"player_scan_%1:occupants_%2:player_transitions_%3:nearby_players_%4:managed_logical_%5:managed_transitions_%6",
			playerManager != null,
			scan.m_iProtectedOccupants,
			scan.m_iPlayerTransitions,
			scan.m_iNearbyPlayers,
			scan.m_iManagedLogicalOccupants,
			scan.m_iManagedTransitions);
		scan.m_sBlockerSignature += string.Format(
			":managed_bounds_%1:managed_non_alive_logical_%2",
			scan.m_iManagedInsideBounds,
			scan.m_iManagedNonAliveLogicalOccupants);
	}

	protected bool ResolveExpectedVehicle(
		AICF_VehicleCleanupJob job,
		out Vehicle vehicle,
		out string actualRplId,
		out string failure)
	{
		vehicle = null;
		actualRplId = "NONE";
		failure = "WORLD_UNAVAILABLE";
		if (!job || !job.m_Fence || !job.m_Fence.IsComplete() || !GetGame() ||
			!GetGame().GetWorld())
		{
			return false;
		}
		IEntity entity = GetGame().GetWorld().FindEntityByID(job.m_Fence.GetEntityId());
		if (!entity)
		{
			failure = "ENTITY_ABSENT";
			return false;
		}
		if (entity.GetID() != job.m_Fence.GetEntityId())
		{
			failure = "ENTITY_ID_MISMATCH";
			return false;
		}
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (rpl)
			actualRplId = rpl.Id().ToString();
		if (actualRplId != job.m_Fence.GetRplId())
		{
			failure = "RPL_IDENTITY_MISMATCH";
			return false;
		}
		vehicle = Vehicle.Cast(entity);
		if (!vehicle)
		{
			failure = "ENTITY_NOT_VEHICLE";
			return false;
		}
		if (job.m_Lease && !job.m_bReleaseComplete &&
			job.m_Lease.GetVehicle() != vehicle)
		{
			failure = "LEASE_VEHICLE_REFERENCE_MISMATCH";
			return false;
		}
		if (job.m_WorldPoolAsset && job.m_WorldPoolAsset.GetVehicle() &&
			job.m_WorldPoolAsset.GetVehicle() != vehicle)
		{
			failure = "WORLD_POOL_REFERENCE_MISMATCH";
			return false;
		}
		if (job.m_RetirementAsset && job.m_RetirementAsset.GetVehicle() &&
			job.m_RetirementAsset.GetVehicle() != vehicle)
		{
			failure = "RETIREMENT_REFERENCE_MISMATCH";
			return false;
		}
		failure = string.Empty;
		return true;
	}

	protected bool IsVehicleUnusable(Vehicle vehicle)
	{
		if (!vehicle)
			return true;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(
			vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || usage.GetDamageState() == EDamageState.DESTROYED ||
			SCR_AIVehicleUsability.VehicleIsOnFire(vehicle) ||
			!SCR_AIVehicleUsability.VehicleCanMove(vehicle))
		{
			return true;
		}
		vector transform[4];
		vehicle.GetWorldTransform(transform);
		return transform[1][1] < 0.25;
	}

	protected bool IsJobCurrent(AICF_VehicleCleanupJob job)
	{
		if (!job || !job.m_Fleet || !job.m_Fence || job.m_bCompleted)
			return false;
		if (!job.m_bReleaseComplete)
			return FleetContainsLease(job.m_Fleet, job.m_Lease) &&
				job.m_Fence.MatchesLease(job.m_Lease);
		if (!job.m_Snapshot || !job.m_Fence.MatchesSnapshot(job.m_Snapshot))
			return false;
		if (job.m_WorldPoolAsset && !job.m_bQuarantined)
			return FleetContainsWorldPoolAsset(job.m_Fleet, job.m_WorldPoolAsset);
		return job.m_RetirementAsset != null || job.m_bQuarantined;
	}

	protected bool FleetContainsLease(AICF_FactionFleet fleet, AICF_VehicleLease lease)
	{
		if (!fleet || !lease)
			return false;
		for (int leaseIndex; leaseIndex < fleet.GetLeaseCount(); leaseIndex++)
		{
			if (fleet.GetLease(leaseIndex) == lease)
				return true;
		}
		return false;
	}

	protected bool FleetContainsWorldPoolAsset(
		AICF_FactionFleet fleet,
		AICF_WorldPoolAsset asset)
	{
		if (!fleet || !asset)
			return false;
		for (int assetIndex; assetIndex < fleet.GetWorldPoolCount(); assetIndex++)
		{
			if (fleet.GetWorldPoolAsset(assetIndex) == asset)
				return true;
		}
		return false;
	}

	protected AICF_VehicleCleanupOutcome RetainFailClosed(
		AICF_VehicleCleanupJob job,
		string reason,
		string actualRplId,
		int nowMs)
	{
		if (!job)
			return AICF_VehicleCleanupOutcome.Retained(reason, "NONE");
		if (!job.m_Snapshot && job.m_Lease && job.m_Lease.GetCleanupSnapshot())
			job.m_Snapshot = job.m_Lease.GetCleanupSnapshot();
		if (job.m_WorldPoolAsset && !job.m_bQuarantined &&
			FleetContainsWorldPoolAsset(job.m_Fleet, job.m_WorldPoolAsset))
		{
			if (job.m_Fleet.RemoveWorldPoolAsset(job.m_WorldPoolAsset))
				job.m_bQuarantined = true;
		}
		if (job.m_Lease && !job.m_bReleaseComplete &&
			job.m_Lease.GetState() != AICF_EVehicleLeaseState.RELEASED)
		{
			job.m_Lease.FailClosed();
		}
		job.m_bFailClosed = true;
		job.m_sFailClosedReason = reason;
		job.m_State.StopFailClosed();
		ReportProtectedClearanceFinalOutcome(
			job,
			"RETAINED_FAIL_CLOSED:" + reason,
			nowMs);
		if (!job.m_bIdentityFailureReported)
		{
			job.m_bIdentityFailureReported = true;
			string details = BuildJobIdentity(job);
			details += string.Format(
				" reason=%1 expected_rpl_id=%2 actual_rpl_id=%3 action=FAIL_CLOSED",
				reason,
				job.m_Fence.GetRplId(),
				actualRplId);
			int terminalAtMs = job.m_iProtectedClearanceDeadlineAtMs +
				PROTECTED_CLEARANCE_TERMINAL_GRACE_MS;
			int terminalAgeMs = Math.Max(0, nowMs - terminalAtMs);
			if (job.m_Scan)
			{
				details += string.Format(
					" protected_occupants=%1 nearby_players=%2 managed_logical=%3 managed_transitions=%4 managed_inside_bounds=%5 terminal_at_ms=%6 terminal_age_ms=%7",
					job.m_Scan.m_iProtectedOccupants,
					job.m_Scan.m_iNearbyPlayers,
					job.m_Scan.m_iManagedLogicalOccupants,
					job.m_Scan.m_iManagedTransitions,
					job.m_Scan.m_iManagedInsideBounds,
					terminalAtMs,
					terminalAgeMs);
				details += " managed_samples=[" + job.m_Scan.m_sManagedSamples + "]";
			}
			string ownershipSummary =
				"ownership=CleanupManager cap=HELD trip_detach_allowed=1";
			if (job.m_bReleaseComplete)
			{
				ownershipSummary =
					"ownership=CleanupManager cap=RELEASED release_complete=1 trip_detach_allowed=1";
			}
			details += " " + ownershipSummary;
			AICF_Stage3Diagnostics.Error("VEHICLE_CLEANUP_RETAINED", details);
			if (reason.Contains("MISMATCH"))
				AICF_Stage3Diagnostics.Warning("VEHICLE_DELETE_IDENTITY_REPLACED", details);
			if (reason == "VEHICLE_DELETE_NOT_CONFIRMED")
				AICF_Stage3Diagnostics.Error("VEHICLE_DELETE_NOT_CONFIRMED", details);
		}
		ObserveLateCleanupFailure(job, reason);
		if (job.m_bStopMode)
			ReportStopRetained(job, nowMs, reason);
		return AICF_VehicleCleanupOutcome.Retained(reason, job.m_sActionToken);
	}

	protected void ObserveLateCleanupFailure(
		AICF_VehicleCleanupJob job,
		string reason)
	{
		if (!job || job.m_bAcceptanceFailureReported || !m_AcceptanceMonitor)
		{
			return;
		}
		job.m_bAcceptanceFailureReported = true;
		if (job.m_Snapshot && job.m_Snapshot.IsComplete())
		{
			m_AcceptanceMonitor.ObserveCleanupFailure(
				job.m_Snapshot,
				reason,
				job.m_sOperationId,
				job.m_sCausationId);
			return;
		}
		m_AcceptanceMonitor.ObserveCleanupFailureFromFence(
			job.m_Fence,
			reason,
			job.m_sOperationId,
			job.m_sCausationId);
	}

	protected bool DeleteExpectedEntity(AICF_VehicleCleanupJob job, Vehicle vehicle)
	{
		// The only destructive primitive. Callers reached this method only after
		// full identity validation, continuous stable clear and immediate re-scan.
		if (!IsAuthority() || !job || !job.m_Fence || !vehicle ||
			vehicle.GetID() != job.m_Fence.GetEntityId())
		{
			return false;
		}
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl || rpl.Id().ToString() != job.m_Fence.GetRplId())
			return false;
		if (job.m_WorldPoolAsset && job.m_WorldPoolAsset.GetVehicle() &&
			!job.m_WorldPoolAsset.MatchesIdentity(
				job.m_Fence.GetEntityId(),
				job.m_Fence.GetRplId()))
		{
			return false;
		}
		if (job.m_RetirementAsset && job.m_RetirementAsset.GetVehicle() &&
			!job.m_RetirementAsset.MatchesLiveIdentity(
				job.m_Fence.GetEntityId(),
				job.m_Fence.GetRplId()))
		{
			return false;
		}
		RplComponent.DeleteRplEntity(vehicle, false);
		if (job.m_WorldPoolAsset && job.m_WorldPoolAsset.GetVehicle())
		{
			job.m_WorldPoolAsset.ClearVehicleReferenceAfterDeleteRequest(
				job.m_Fence.GetEntityId(),
				job.m_Fence.GetRplId());
		}
		if (job.m_RetirementAsset && job.m_RetirementAsset.GetVehicle())
		{
			job.m_RetirementAsset.ClearVehicleReferenceAfterDeleteRequest(
				job.m_Fence.GetEntityId(),
				job.m_Fence.GetRplId());
		}
		return true;
	}

	protected AICF_VehicleCleanupOutcome ConfirmDelete(
		AICF_VehicleCleanupJob job,
		string reason)
	{
		job.m_State.ConfirmDelete();
		job.m_bCompleted = true;
		ReportCleanupConfirmed(job, reason);
		return AICF_VehicleCleanupOutcome.DeleteConfirmed(reason, job.m_sActionToken);
	}

	protected AICF_VehicleCleanupOutcome ConfirmAbsentAsset(
		AICF_VehicleCleanupJob job,
		string reason)
	{
		if (job.m_WorldPoolAsset && !job.m_bQuarantined &&
			FleetContainsWorldPoolAsset(job.m_Fleet, job.m_WorldPoolAsset))
		{
			job.m_Fleet.RemoveWorldPoolAsset(job.m_WorldPoolAsset);
			job.m_bQuarantined = true;
		}
		job.m_State.ConfirmDelete();
		job.m_bCompleted = true;
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_WORLD_POOL_STALE_REMOVED",
			BuildJobIdentity(job) + " reason=" + reason + " policy=BOOKKEEPING_ONLY");
		ReportCleanupConfirmed(job, reason);
		return AICF_VehicleCleanupOutcome.DeleteConfirmed(reason, job.m_sActionToken);
	}

	protected void CancelStaleJob(AICF_VehicleCleanupJob job, string reason)
	{
		if (!job)
			return;
		job.m_bCompleted = true;
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_CLEANUP_STALE_JOB_CANCELLED",
			BuildJobIdentity(job) + " reason=" + reason + " action=SELF_CANCEL");
	}

	protected void PromoteToStop(AICF_VehicleCleanupJob job, int nowMs)
	{
		if (!job || job.m_bStopMode || job.m_bCompleted || job.m_bFailClosed)
			return;
		job.m_bStopMode = true;
		job.m_iStopStartedAtMs = nowMs;
		job.m_Disposition = AICF_EVehicleReleaseDisposition.DESTRUCTIVE_RETIREMENT;
		if (job.m_bReleaseComplete)
			MarkRetirement(job, "COORDINATOR_STOP");
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_STOP_CLEANUP_STARTED",
			BuildJobIdentity(job) + string.Format(
				" stable_clear_ms=%1 player_radius_m=%2 acquire_timeout_ms=%3",
				STABLE_CLEAR_MS,
				PLAYER_PROTECTION_RADIUS_METERS,
				STOP_TIMEOUT_MS));
		ScheduleStopPoll(job);
	}

	protected void ScheduleStopPoll(AICF_VehicleCleanupJob job)
	{
		if (!job || job.m_bStopPollScheduled || job.m_bCompleted || job.m_bFailClosed)
			return;
		if (!GetGame())
		{
			RetainFailClosed(
				job,
				"STOP_CALLQUEUE_UNAVAILABLE",
				"NONE",
				System.GetTickCount());
			return;
		}
		job.m_bStopPollScheduled = true;
		GetGame().GetCallqueue().CallLater(
			PollStoppedCleanup,
			STOP_POLL_MS,
			false,
			job.m_Fence);
	}

	protected void PollStoppedCleanup(AICF_VehicleCleanupFence callbackFence)
	{
		if (!callbackFence)
			return;
		AICF_VehicleCleanupJob job = FindJobByActionToken(callbackFence.GetActionToken());
		if (!IsAuthority())
		{
			if (job)
			{
				job.m_bStopPollScheduled = false;
				RetainFailClosed(
					job,
					"STOP_AUTHORITY_LOST",
					"NONE",
					System.GetTickCount());
			}
			else
			{
				ReportStaleCallback(callbackFence);
			}
			return;
		}
		if (!job || !job.m_Fence.MatchesFence(callbackFence) || !IsJobCurrent(job))
		{
			ReportStaleCallback(callbackFence);
			if (job)
			{
				job.m_bStopPollScheduled = false;
				job.m_bCompleted = true;
			}
			return;
		}
		job.m_bStopPollScheduled = false;
		int nowMs = System.GetTickCount();
		if (nowMs - job.m_iStopStartedAtMs >= STOP_TIMEOUT_MS)
		{
			RetainFailClosed(job, "STOP_CLEANUP_RETAINED_TIMEOUT", "NONE", nowMs);
			return;
		}
		if (!job.m_bReleaseComplete)
			ProcessLeaseRelease(job, nowMs);
		if (job.m_bReleaseComplete && !job.m_bFailClosed)
		{
			MarkRetirement(job, "COORDINATOR_STOP");
			ProcessRetirement(job, nowMs);
		}
		if (job.m_bCompleted)
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_STOP_CLEANUP_CONFIRMED",
				BuildJobIdentity(job) + " reason=AUTHORITY_ENTITY_ABSENT");
			return;
		}
		if (!job.m_bFailClosed)
			ScheduleStopPoll(job);
	}

	protected void ReportStaleCallback(AICF_VehicleCleanupFence fence)
	{
		if (!fence)
			return;
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_CLEANUP_STALE_CALLBACK_CANCELLED",
			string.Format(
				"faction=%1 slot=%2 group_generation=%3 trip_generation=%4 lease_generation=%5 vehicle_generation=%6",
				fence.GetFactionKey(),
				fence.GetSlotId(),
				fence.GetGroupGeneration(),
				fence.GetTripGeneration(),
				fence.GetLeaseGeneration(),
				fence.GetVehicleGeneration()) + string.Format(
				" vehicle_lifecycle_id=%1 entity_id=%2 rpl_id=%3 action_token=%4 action=SELF_CANCEL",
				fence.GetVehicleLifecycleId(),
				fence.GetEntityId(),
				fence.GetRplId(),
				fence.GetActionToken()));
	}

	protected AICF_VehicleCleanupQuery BuildQuery(AICF_VehicleCleanupJob job, int nowMs)
	{
		if (!job)
			return AICF_VehicleCleanupQuery.NotTracked();
		int stableClearMs;
		if (job.m_State.GetStableClearStartedAtMs() > 0)
			stableClearMs = Math.Max(0, nowMs - job.m_State.GetStableClearStartedAtMs());
		string nextAction = "WORLD_POOL_AVAILABLE";
		if (job.m_bCompleted)
			nextAction = "CLEANUP_COMPLETE";
		else if (job.m_bFailClosed)
			nextAction = "RETAIN_FAIL_CLOSED";
		else if (!job.m_bReleaseComplete)
			nextAction = "WAIT_PROTECTED_CLEARANCE";
		else if (job.m_State.IsDeleteConfirmationPending())
			nextAction = "CONFIRM_DELETE_IDENTITY";
		else if (job.m_bRetirementRequested)
			nextAction = "WAIT_STABLE_CLEAR_DELETE";
		int protectedOccupants;
		int nearbyPlayers;
		int managedLogicalOccupants;
		int managedTransitions;
		int managedInsideBounds;
		string managedSamples = "NONE";
		if (job.m_Scan)
		{
			protectedOccupants = job.m_Scan.m_iProtectedOccupants;
			nearbyPlayers = job.m_Scan.m_iNearbyPlayers;
			managedLogicalOccupants = job.m_Scan.m_iManagedLogicalOccupants;
			managedTransitions = job.m_Scan.m_iManagedTransitions;
			managedInsideBounds = job.m_Scan.m_iManagedInsideBounds;
			managedSamples = job.m_Scan.m_sManagedSamples;
		}
		AICF_VehicleCleanupQuery query = new AICF_VehicleCleanupQuery(
			true,
			job.m_bReleaseComplete,
			job.m_bClearanceSafe,
			job.m_State.IsDeleteConfirmationPending(),
			job.m_bFailClosed,
			stableClearMs,
			job.m_State.GetDeleteAttempts(),
			protectedOccupants,
			nearbyPlayers,
			managedLogicalOccupants,
			managedTransitions,
			managedInsideBounds,
			job.m_State.GetBlockerSignature(),
			managedSamples,
			job.m_sActionToken,
			nextAction);
		if (job.m_Scan)
			query.SetManagedNonAliveLogicalOccupants(
				job.m_Scan.m_iManagedNonAliveLogicalOccupants);
		return query;
	}

	protected AICF_VehicleCleanupJob FindJobByLease(AICF_VehicleLease lease)
	{
		foreach (AICF_VehicleCleanupJob job : m_aJobs)
		{
			if (job && job.m_Lease == lease)
				return job;
		}
		return null;
	}

	protected AICF_VehicleCleanupJob FindJobByWorldPoolAsset(AICF_WorldPoolAsset asset)
	{
		foreach (AICF_VehicleCleanupJob job : m_aJobs)
		{
			if (job && job.m_WorldPoolAsset == asset && !job.m_bCompleted)
				return job;
		}
		return null;
	}

	protected AICF_VehicleCleanupJob FindJobByRetirementAsset(
		AICF_VehicleRetirementAsset asset)
	{
		foreach (AICF_VehicleCleanupJob job : m_aJobs)
		{
			if (job && job.m_RetirementAsset == asset && !job.m_bCompleted)
				return job;
		}
		return null;
	}

	protected AICF_VehicleCleanupJob FindJobByActionToken(string actionToken)
	{
		foreach (AICF_VehicleCleanupJob job : m_aJobs)
		{
			if (job && job.m_sActionToken == actionToken && !job.m_bCompleted)
				return job;
		}
		return null;
	}

	protected void PruneCompletedJobs()
	{
		for (int jobIndex = m_aJobs.Count() - 1; jobIndex >= 0; jobIndex--)
		{
			AICF_VehicleCleanupJob job = m_aJobs[jobIndex];
			if (job && job.m_bCompleted && !job.m_bStopPollScheduled &&
				(!job.m_Lease || job.m_bControllerAcknowledged))
			{
				m_aJobs.RemoveOrdered(jobIndex);
			}
		}
	}

	protected string BuildJobIdentity(AICF_VehicleCleanupJob job)
	{
		AICF_VehicleCleanupFence fence = job.m_Fence;
		string details = string.Format(
			"faction=%1 slot=%2 group_generation=%3 trip_generation=%4 lease_generation=%5 vehicle_generation=%6",
			fence.GetFactionKey(),
			fence.GetSlotId(),
			fence.GetGroupGeneration(),
			fence.GetTripGeneration(),
			fence.GetLeaseGeneration(),
			fence.GetVehicleGeneration());
		details += string.Format(
			" vehicle_lifecycle_id=%1 entity_id=%2 rpl_id=%3 action_token=%4",
			fence.GetVehicleLifecycleId(),
			fence.GetEntityId(),
			fence.GetRplId(),
			fence.GetActionToken());
		details += string.Format(
			" vehicle=%1 vehicle_kind=%2 vehicle_state=%3",
			fence.GetEntityId(),
			ResolveVehicleKind(job),
			ResolveVehicleState(job));
		details += string.Format(
			" operation_id=%1 causation_id=%2 cleanup_trigger=%3 order_restore_gate=0",
			job.m_sOperationId,
			job.m_sCausationId,
			job.m_sTrigger);
		if (job.m_Snapshot)
		{
			details += string.Format(
				" last_entity_id=%1 last_rpl_id=%2 last_origin=%3 prefab=%4 release_at_ms=%5",
				job.m_Snapshot.GetLastEntityIdString(),
				job.m_Snapshot.GetLastRplId(),
				job.m_Snapshot.GetLastOrigin(),
				job.m_Snapshot.GetPrefab(),
				job.m_Snapshot.GetReleaseAtMs());
		}
		return details;
	}

	protected string ResolveVehicleKind(AICF_VehicleCleanupJob job)
	{
		if (job.m_Lease)
			return AICF_Stage3Diagnostics.KindToString(job.m_Lease.GetKind());
		if (job.m_WorldPoolAsset)
			return AICF_Stage3Diagnostics.KindToString(job.m_WorldPoolAsset.GetKind());
		return "NONE";
	}

	protected string ResolveVehicleState(AICF_VehicleCleanupJob job)
	{
		if (job.m_bFailClosed)
			return "RETAINED_FAIL_CLOSED";
		if (job.m_bCompleted)
			return "CLEANUP_CONFIRMED";
		if (job.m_State && job.m_State.IsDeleteConfirmationPending())
			return "DELETE_CONFIRMATION_PENDING";
		if (job.m_bQuarantined)
			return "RETIREMENT_QUARANTINE";
		if (job.m_bRetirementRequested)
			return "RETIREMENT_PENDING";
		if (job.m_bReleaseComplete)
			return "WORLD_POOL";
		if (job.m_Lease)
		{
			return "LEASE_" + typename.EnumToString(
				AICF_EVehicleLeaseState,
				job.m_Lease.GetState());
		}
		return "UNKNOWN";
	}

	protected void ReportReleaseQueued(AICF_VehicleCleanupJob job)
	{
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_LEASE_RELEASE_QUEUED",
			BuildJobIdentity(job) + string.Format(
				" disposition=%1 stable_clear_required_ms=%2 player_radius_m=%3",
				job.m_Disposition,
				STABLE_CLEAR_MS,
				PLAYER_PROTECTION_RADIUS_METERS));
	}

	protected void ReportWorldPoolReleased(AICF_VehicleCleanupJob job)
	{
		if (job.m_bReleaseReported)
			return;
		job.m_bReleaseReported = true;
		string details = BuildJobIdentity(job);
		details += " disposition=FUNCTIONAL_WORLD_POOL ai_cap_reserved=0 player_available=1";
		AICF_Stage3Diagnostics.Info("VEHICLE_WORLD_POOL_RELEASED", details);
		ReportProtectedClearanceFinalOutcome(
			job,
			"RELEASED_TO_WORLD_POOL",
			System.GetTickCount());
	}

	protected void ReportRetirementQuarantined(AICF_VehicleCleanupJob job)
	{
		if (job.m_bReleaseReported)
			return;
		job.m_bReleaseReported = true;
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_RETIREMENT_QUARANTINED",
			BuildJobIdentity(job) +
			" disposition=DESTRUCTIVE_RETIREMENT_QUARANTINE ai_cap_reserved=0 player_available=0");
		ReportProtectedClearanceFinalOutcome(
			job,
			"QUARANTINED_FOR_RETIREMENT",
			System.GetTickCount());
	}

	protected void ReportProtectedClearanceDeadlineExceeded(
		AICF_VehicleCleanupJob job,
		int nowMs)
	{
		if (!job || job.m_bProtectedClearanceDeadlineReported)
			return;
		job.m_bProtectedClearanceDeadlineReported = true;
		job.m_iProtectedClearanceDeadlineTriggeredAtMs = nowMs;
		int deadlineAgeMs = Math.Max(
			0,
			nowMs - job.m_iProtectedClearanceDeadlineAtMs);
		int terminalAtMs = job.m_iProtectedClearanceDeadlineAtMs +
			PROTECTED_CLEARANCE_TERMINAL_GRACE_MS;
		int terminalRemainingMs = Math.Max(0, terminalAtMs - nowMs);
		string deadlineDetails = BuildJobIdentity(job);
		deadlineDetails += string.Format(
			" deadline_at_ms=%1 deadline_age_ms=%2 terminal_at_ms=%3 terminal_remaining_ms=%4 protected_occupants=%5 player_transitions=%6 nearby_players=%7",
			job.m_iProtectedClearanceDeadlineAtMs,
			deadlineAgeMs,
			terminalAtMs,
			terminalRemainingMs,
			job.m_Scan.m_iProtectedOccupants,
			job.m_Scan.m_iPlayerTransitions,
			job.m_Scan.m_iNearbyPlayers);
		deadlineDetails += string.Format(
			" managed_logical=%1 managed_non_alive_logical=%2 managed_transitions=%3 managed_inside_bounds=%4 blocker_signature=%5 action=START_BOUNDED_PLAYER_FENCED_RECOVERY",
			job.m_Scan.m_iManagedLogicalOccupants,
			job.m_Scan.m_iManagedNonAliveLogicalOccupants,
			job.m_Scan.m_iManagedTransitions,
			job.m_Scan.m_iManagedInsideBounds,
			job.m_Scan.m_sBlockerSignature);
		AICF_Stage3Diagnostics.Warning(
			"PROTECTED_CLEARANCE_DEADLINE_EXCEEDED",
			deadlineDetails);
	}

	protected void ReportProtectedClearanceFinalOutcome(
		AICF_VehicleCleanupJob job,
		string outcome,
		int nowMs)
	{
		if (!job || !job.m_bProtectedClearanceDeadlineReported ||
			job.m_bProtectedClearanceFinalOutcomeReported)
		{
			return;
		}
		job.m_bProtectedClearanceFinalOutcomeReported = true;
		string blockerSignature = "NONE";
		if (job.m_Scan)
			blockerSignature = job.m_Scan.m_sBlockerSignature;
		int deadlineAgeMs = Math.Max(
			0,
			nowMs - job.m_iProtectedClearanceDeadlineAtMs);
		int recoveryAgeMs = Math.Max(
			0,
			nowMs - job.m_iProtectedClearanceDeadlineTriggeredAtMs);
		string ownershipSummary =
			"ownership=CleanupManager cap=HELD_UNTIL_SAFE trip_detach_allowed=1";
		if (job.m_bReleaseComplete)
		{
			ownershipSummary =
				"ownership=CleanupManager cap=RELEASED release_complete=1 trip_detach_allowed=1";
		}
		string finalDetails = BuildJobIdentity(job);
		finalDetails += string.Format(
			" outcome=%1 deadline_at_ms=%2 deadline_age_ms=%3 terminal_grace_ms=%4 recovery_age_ms=%5 managed_recovery_attempts=%6 exact_recovery_attempts=%7 non_alive_detach_attempts=%8",
			outcome,
			job.m_iProtectedClearanceDeadlineAtMs,
			deadlineAgeMs,
			PROTECTED_CLEARANCE_TERMINAL_GRACE_MS,
			recoveryAgeMs,
			job.m_iManagedRecoveryAttempts,
			job.m_iManagedExactRecoveryAttempts,
			job.m_iNonAliveDetachAttempts);
		finalDetails += string.Format(
			" managed_non_alive_logical=%1 blocker_signature=%2",
			job.m_Scan.m_iManagedNonAliveLogicalOccupants,
			blockerSignature);
		finalDetails += " " + ownershipSummary;
		AICF_Stage3Diagnostics.Info(
			"PROTECTED_CLEARANCE_FINAL_OUTCOME",
			finalDetails);
	}

	protected void AuditDeferred(
		AICF_VehicleCleanupJob job,
		string reason,
		int stableClearMs,
		int nowMs)
	{
		string signature = reason + ":" + job.m_Scan.m_sBlockerSignature;
		if (signature == job.m_sLastReportedBlocker &&
			nowMs - job.m_iLastAuditAtMs < DEFERRED_AUDIT_INTERVAL_MS)
		{
			return;
		}
		job.m_sLastReportedBlocker = signature;
		job.m_iLastAuditAtMs = nowMs;
		int deadlineAgeMs = Math.Max(
			0,
			nowMs - job.m_iProtectedClearanceDeadlineAtMs);
		int terminalAtMs = job.m_iProtectedClearanceDeadlineAtMs +
			PROTECTED_CLEARANCE_TERMINAL_GRACE_MS;
		int terminalRemainingMs = Math.Max(0, terminalAtMs - nowMs);
		string hiddenFenceReason = job.m_sManagedExactRecoveryFenceReason;
		if (hiddenFenceReason.IsEmpty())
			hiddenFenceReason = "NONE";
		string nonAliveFenceReason = job.m_sNonAliveDetachFenceReason;
		if (nonAliveFenceReason.IsEmpty())
			nonAliveFenceReason = "NONE";
		string details = BuildJobIdentity(job);
		details += string.Format(
			" reason=%1 protected_occupants=%2 player_transitions=%3 nearby_players=%4 stable_clear_ms=%5",
			reason,
			job.m_Scan.m_iProtectedOccupants,
			job.m_Scan.m_iPlayerTransitions,
			job.m_Scan.m_iNearbyPlayers,
			stableClearMs);
		details += string.Format(
			" managed_logical=%1 managed_non_alive_logical=%2 managed_transitions=%3 managed_inside_bounds=%4 protection_radius_m=%5 managed_recovery_attempts=%6 managed_recovery_maximum=%7",
			job.m_Scan.m_iManagedLogicalOccupants,
			job.m_Scan.m_iManagedNonAliveLogicalOccupants,
			job.m_Scan.m_iManagedTransitions,
			job.m_Scan.m_iManagedInsideBounds,
			PLAYER_PROTECTION_RADIUS_METERS,
			job.m_iManagedRecoveryAttempts,
			MANAGED_CLEARANCE_RECOVERY_MAX_ATTEMPTS);
		details += string.Format(
			" exact_recovery_attempts=%1 exact_recovery_maximum=%2 non_alive_detach_attempts=%3 non_alive_detach_maximum=%4",
			job.m_iManagedExactRecoveryAttempts,
			MANAGED_EXACT_RECOVERY_MAX_ATTEMPTS,
			job.m_iNonAliveDetachAttempts,
			NON_ALIVE_DETACH_MAX_ATTEMPTS);
		details += string.Format(
			" deadline_at_ms=%1 deadline_age_ms=%2 terminal_at_ms=%3 terminal_remaining_ms=%4",
			job.m_iProtectedClearanceDeadlineAtMs,
			deadlineAgeMs,
			terminalAtMs,
			terminalRemainingMs);
		details += string.Format(
			" hidden_fence_reason=%1 non_alive_fence_reason=%2 global_samples=[%3] managed_samples=[%4]",
			hiddenFenceReason,
			nonAliveFenceReason,
			job.m_Scan.m_sGlobalSamples,
			job.m_Scan.m_sManagedSamples);
		AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP_DEFERRED", details);
	}

	protected void ReportDeleteRequested(AICF_VehicleCleanupJob job, bool retry)
	{
		string eventName = "VEHICLE_DELETE_REQUESTED";
		if (retry)
			eventName = "VEHICLE_DELETE_RETRIED";
		AICF_Stage3Diagnostics.Info(
			eventName,
			BuildJobIdentity(job) + string.Format(
				" reason=%1 attempt=%2 confirmation_timeout_ms=%3",
				job.m_sRetirementReason,
				job.m_State.GetDeleteAttempts(),
				DELETE_CONFIRM_TIMEOUT_MS));
	}

	protected void ReportCleanupConfirmed(AICF_VehicleCleanupJob job, string reason)
	{
		string details = BuildJobIdentity(job);
		details += string.Format(
			" reason=%1 delete_attempts=%2",
			reason,
			job.m_State.GetDeleteAttempts());
		AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP_CONFIRMED", details);
		AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP", details);
	}

	protected void ReportStopRetained(
		AICF_VehicleCleanupJob job,
		int nowMs,
		string reason)
	{
		if (job.m_bStopRetainedReported)
			return;
		job.m_bStopRetainedReported = true;
		AICF_Stage3Diagnostics.Warning(
			"VEHICLE_STOP_CLEANUP_RETAINED",
			BuildJobIdentity(job) + string.Format(
				" stop_age_ms=%1 reason=%2 action=FAIL_CLOSED",
				nowMs - job.m_iStopStartedAtMs,
				reason));
	}

	protected void ReportSoftOverflow(AICF_FactionFleet fleet, int nowMs)
	{
		int poolLimit = m_Config.GetAbandonedWorldPoolPerFaction();
		if (fleet.GetWorldPoolCount() <= poolLimit)
			return;
		AICF_VehicleCleanupFleetAudit audit = GetOrCreateFleetAudit(fleet);
		string signature = string.Format(
			"%1:%2:%3",
			fleet.GetFactionKey(),
			fleet.GetWorldPoolCount(),
			CountPendingPoolRemovals(fleet));
		if (signature == audit.m_sLastSignature &&
			nowMs - audit.m_iLastReportedAtMs < DEFERRED_AUDIT_INTERVAL_MS)
		{
			return;
		}
		audit.m_sLastSignature = signature;
		audit.m_iLastReportedAtMs = nowMs;
		AICF_Stage3Diagnostics.Info(
			"VEHICLE_WORLD_POOL_SOFT_OVERFLOW",
			string.Format(
				"faction=%1 pool_size=%2 pool_limit=%3 retirement_candidates=%4 policy=OLDEST_SAFE_PLAYER_PROTECTED",
				fleet.GetFactionKey(),
				fleet.GetWorldPoolCount(),
				poolLimit,
				CountPendingPoolRemovals(fleet)));
	}

	protected AICF_VehicleCleanupFleetAudit GetOrCreateFleetAudit(AICF_FactionFleet fleet)
	{
		foreach (AICF_VehicleCleanupFleetAudit audit : m_aFleetAudits)
		{
			if (audit && audit.m_Fleet == fleet)
				return audit;
		}
		AICF_VehicleCleanupFleetAudit created = new AICF_VehicleCleanupFleetAudit();
		created.m_Fleet = fleet;
		m_aFleetAudits.Insert(created);
		return created;
	}
}
