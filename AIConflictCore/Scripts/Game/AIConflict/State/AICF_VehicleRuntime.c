// Exact per-member movement is used only while the whole fireteam approaches
// a vehicle. A group waypoint may complete when its leader arrives even while
// other members are still far away, so every living member owns a separately
// tracked, bounded action instead.
class AICF_VehicleApproachActionToken
{
	protected AIAgent m_Agent;
	protected ref SCR_AIMoveIndividuallyBehavior m_Action;
	protected int m_iRetryCount;
	protected int m_iLastProgressAtMs;
	protected float m_fBestDistanceMeters;

	void AICF_VehicleApproachActionToken(
		AIAgent agent,
		SCR_AIMoveIndividuallyBehavior action,
		float distanceMeters,
		int retryCount = 0)
	{
		m_Agent = agent;
		m_Action = action;
		m_fBestDistanceMeters = distanceMeters;
		m_iRetryCount = retryCount;
		m_iLastProgressAtMs = System.GetTickCount();
	}

	AIAgent GetAgent() { return m_Agent; }
	SCR_AIMoveIndividuallyBehavior GetAction() { return m_Action; }
	int GetRetryCount() { return m_iRetryCount; }
	float GetBestDistanceMeters() { return m_fBestDistanceMeters; }

	bool ObserveProgress(float distanceMeters, float minimumProgressMeters)
	{
		if (distanceMeters < 0 || distanceMeters > m_fBestDistanceMeters - minimumProgressMeters)
			return false;
		m_fBestDistanceMeters = distanceMeters;
		m_iLastProgressAtMs = System.GetTickCount();
		return true;
	}

	int GetProgressAgeMs()
	{
		return System.GetTickCount(m_iLastProgressAtMs);
	}
}

// One runtime belongs to a stable faction/slot/group-generation tuple. The
// coordinator keeps terminal runtimes alive until their vehicle is safely
// cleaned, which makes the per-faction vehicle cap include abandoned entities.
class AICF_VehicleRuntime
{
	protected FactionKey m_sFactionKey;
	protected int m_iSlotId;
	protected int m_iGroupGeneration;
	protected int m_iVehicleGeneration;
	protected AICF_EVehicleKind m_Kind;
	protected AICF_EVehicleState m_State;
	protected AICF_EVehicleBoardingPhase m_BoardingPhase;
	protected AICF_EVehicleCrewRecoveryPhase m_CrewRecoveryPhase;
	protected int m_iStateStartedAtMs;
	protected int m_iBoardingStartedAtMs;
	protected int m_iPlannedBoardingPhaseCount;
	protected int m_iBoardingSettledPollCount;
	protected int m_iBoardingMaxLinkedCount;
	protected int m_iBoardingMaxCompartmentCount;
	protected int m_iBoardingMaxGettingInCount;
	protected int m_iBoardingMaxCharacterVehicleCount;
	protected int m_iBoardingMaxSettledCount;
	protected int m_iLastBoardingProgressAtMs;
	protected float m_fBestBoardingFarthestDistanceMeters = -1.0;
	protected bool m_bBoardingDriverPhasePlanned;
	protected bool m_bBoardingGunnerPhasePlanned;
	protected bool m_bBoardingGraceEvaluated;
	protected bool m_bBoardingGraceGranted;
	protected int m_iNextAttemptAtMs;
	protected int m_iRequestGeneration = 1;
	protected int m_iSpawnAttempt;
	protected int m_iTotalSpawnAttempts;
	protected int m_iObservedBaseRevision;
	protected int m_iRequestStartedAtMs;
	protected int m_iLastWaitReportAtMs;
	protected string m_sLastSpawnFailureReason;
	protected int m_iLastProgressAtMs;
	protected int m_iLastMotionAtMs;
	protected int m_iLastMotionReportAtMs;
	protected int m_iRecoveryCount;
	protected int m_iCleanupAtMs;
	protected float m_fBestDistanceMeters = -1.0;
	protected vector m_vLastMotionPosition;
	protected bool m_bHasMotionSample;
	protected bool m_bCapBlockedReported;
	protected bool m_bCompletedTrip;
	protected bool m_bRouteRecoveryPending;
	protected bool m_bRecoveryRequiresRouteProgress;
	protected bool m_bRecoveryFailureReported;
	protected bool m_bFallbackForceExitReported;
	protected bool m_bFallbackExitFailureReported;
	protected bool m_bInfantryFallbackRestorePending;
	protected bool m_bBoardingRoleResetAttempted;
	protected bool m_bBoardingRoleRetryIssued;
	protected bool m_bDismountReissueAttempted;
	protected bool m_bDismountClearanceRecoveryAttempted;
	protected int m_iBoardingRoleResetAtMs;
	protected int m_iDismountClearanceBlockedAtMs;
	protected int m_iDismountClearPollCount;
	protected int m_iDismountClearanceRecoveryAttempts;
	protected EntityID m_VehicleDeleteEntityId = EntityID.INVALID;
	protected int m_iVehicleDeleteRequestedAtMs;
	protected int m_iLastVehicleDeleteAttemptAtMs;
	protected int m_iVehicleDeleteAttempts;
	protected bool m_bVehicleDeleteFailureReported;
	protected bool m_bWorldPoolRetirementRequested;
	protected int m_iWorldPoolReleasedAtMs;
	protected int m_iCleanupClearStartedAtMs;
	protected int m_iLastCleanupDeferredReportAtMs;
	protected string m_sLastSpawnIssueReportKey;
	protected string m_sTerminalReason;
	protected string m_sVehicleDeleteEntityId;
	protected string m_sVehicleDeleteRplId;
	protected vector m_vVehicleDeleteOrigin;

	protected ResourceName m_VehiclePrefab;
	protected SCR_AIGroup m_Group;
	protected Vehicle m_Vehicle;
	protected SCR_AIVehicleUsageComponent m_VehicleUsage;
	protected SCR_CampaignMilitaryBaseComponent m_SpawnBase;
	protected SCR_CampaignMilitaryBaseComponent m_TargetBase;
	protected AIWaypoint m_ActiveWaypoint;
	protected IEntity m_LastDriver;
	protected IEntity m_LastGunner;
	protected AIAgent m_CrewRecoveryAgent;
	protected ref SCR_AIGetInVehicle m_CrewRecoveryAction;
	protected ref array<ref AICF_VehicleApproachActionToken> m_aBoardingApproachActions = {};

	void AICF_VehicleRuntime(
		FactionKey factionKey,
		int slotId,
		int groupGeneration,
		AICF_EVehicleKind kind)
	{
		m_sFactionKey = factionKey;
		m_iSlotId = slotId;
		m_iGroupGeneration = groupGeneration;
		m_Kind = kind;
		m_iVehicleGeneration = 1;
		m_iRequestStartedAtMs = System.GetTickCount();
		SetState(AICF_EVehicleState.REQUESTED);
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	int GetGroupGeneration() { return m_iGroupGeneration; }
	int GetVehicleGeneration() { return m_iVehicleGeneration; }
	AICF_EVehicleKind GetKind() { return m_Kind; }
	AICF_EVehicleState GetState() { return m_State; }
	AICF_EVehicleBoardingPhase GetBoardingPhase() { return m_BoardingPhase; }
	AICF_EVehicleCrewRecoveryPhase GetCrewRecoveryPhase() { return m_CrewRecoveryPhase; }
	int GetStateStartedAtMs() { return m_iStateStartedAtMs; }
	int GetBoardingStartedAtMs() { return m_iBoardingStartedAtMs; }
	int GetPlannedBoardingPhaseCount() { return m_iPlannedBoardingPhaseCount; }
	int GetBoardingSettledPollCount() { return m_iBoardingSettledPollCount; }
	int GetBoardingMaxLinkedCount() { return m_iBoardingMaxLinkedCount; }
	int GetBoardingMaxCompartmentCount() { return m_iBoardingMaxCompartmentCount; }
	int GetBoardingMaxGettingInCount() { return m_iBoardingMaxGettingInCount; }
	int GetBoardingMaxCharacterVehicleCount() { return m_iBoardingMaxCharacterVehicleCount; }
	int GetBoardingMaxSettledCount() { return m_iBoardingMaxSettledCount; }
	float GetBestBoardingFarthestDistanceMeters() { return m_fBestBoardingFarthestDistanceMeters; }
	bool IsBoardingDriverPhasePlanned() { return m_bBoardingDriverPhasePlanned; }
	bool IsBoardingGunnerPhasePlanned() { return m_bBoardingGunnerPhasePlanned; }
	bool IsBoardingGraceGranted() { return m_bBoardingGraceGranted; }
	int GetNextAttemptAtMs() { return m_iNextAttemptAtMs; }
	int GetRequestGeneration() { return m_iRequestGeneration; }
	int GetSpawnAttempt() { return m_iSpawnAttempt; }
	int GetTotalSpawnAttempts() { return m_iTotalSpawnAttempts; }
	int GetObservedBaseRevision() { return m_iObservedBaseRevision; }
	int GetRequestAgeMs() { return System.GetTickCount(m_iRequestStartedAtMs); }
	string GetLastSpawnFailureReason() { return m_sLastSpawnFailureReason; }
	int GetRecoveryCount() { return m_iRecoveryCount; }
	int GetCleanupAtMs() { return m_iCleanupAtMs; }
	ResourceName GetVehiclePrefab() { return m_VehiclePrefab; }
	SCR_AIGroup GetGroup() { return m_Group; }
	Vehicle GetVehicle() { return m_Vehicle; }
	SCR_AIVehicleUsageComponent GetVehicleUsage() { return m_VehicleUsage; }
	SCR_CampaignMilitaryBaseComponent GetSpawnBase() { return m_SpawnBase; }
	SCR_CampaignMilitaryBaseComponent GetTargetBase() { return m_TargetBase; }
	AIWaypoint GetActiveWaypoint() { return m_ActiveWaypoint; }
	IEntity GetLastDriver() { return m_LastDriver; }
	IEntity GetLastGunner() { return m_LastGunner; }
	AIAgent GetCrewRecoveryAgent() { return m_CrewRecoveryAgent; }
	SCR_AIGetInVehicle GetCrewRecoveryAction() { return m_CrewRecoveryAction; }
	int GetBoardingApproachActionCount() { return m_aBoardingApproachActions.Count(); }
	AICF_VehicleApproachActionToken GetBoardingApproachAction(int index)
	{
		if (!m_aBoardingApproachActions.IsIndexValid(index))
			return null;
		return m_aBoardingApproachActions[index];
	}
	bool HasCompletedTrip() { return m_bCompletedTrip; }
	bool IsRecoveringDriver() { return m_CrewRecoveryPhase == AICF_EVehicleCrewRecoveryPhase.DRIVER; }
	bool HasPendingRouteRecovery() { return m_bRouteRecoveryPending; }
	bool RecoveryRequiresRouteProgress() { return m_bRecoveryRequiresRouteProgress; }
	bool IsInfantryFallbackRestorePending() { return m_bInfantryFallbackRestorePending; }
	bool IsBoardingRoleResetAttempted() { return m_bBoardingRoleResetAttempted; }
	bool IsBoardingRoleRetryIssued() { return m_bBoardingRoleRetryIssued; }
	bool IsDismountReissueAttempted() { return m_bDismountReissueAttempted; }
	bool IsDismountClearanceRecoveryAttempted() { return m_bDismountClearanceRecoveryAttempted; }
	int GetDismountClearanceRecoveryAttempts() { return m_iDismountClearanceRecoveryAttempts; }
	int GetDismountClearPollCount() { return m_iDismountClearPollCount; }
	EntityID GetVehicleDeleteEntityId() { return m_VehicleDeleteEntityId; }
	int GetVehicleDeleteAttempts() { return m_iVehicleDeleteAttempts; }
	string GetVehicleDeleteEntityIdString() { return m_sVehicleDeleteEntityId; }
	string GetVehicleDeleteRplId() { return m_sVehicleDeleteRplId; }
	vector GetVehicleDeleteOrigin() { return m_vVehicleDeleteOrigin; }
	string GetTerminalReason() { return m_sTerminalReason; }
	bool IsWorldPoolRetirementRequested() { return m_bWorldPoolRetirementRequested; }
	int GetWorldPoolAgeMs()
	{
		if (m_iWorldPoolReleasedAtMs <= 0)
			return 0;
		return System.GetTickCount(m_iWorldPoolReleasedAtMs);
	}

	void SetGroup(SCR_AIGroup group)
	{
		m_Group = group;
	}

	void SetTerminalReason(string reason)
	{
		m_sTerminalReason = reason;
	}

	void SetState(AICF_EVehicleState state)
	{
		if (m_State == state)
			return;

		AICF_EVehicleState previousState = m_State;
		m_State = state;
		if (previousState == AICF_EVehicleState.RECOVERING && state != AICF_EVehicleState.RECOVERING)
			ResetCrewRecoveryPhase();
		m_iStateStartedAtMs = System.GetTickCount();
		m_bCapBlockedReported = false;
		if (previousState != state)
		{
			AICF_Stage3Diagnostics.Info(
				"VEHICLE_STATE_CHANGED",
				string.Format("%1 from=%2 to=%3", DescribeContext("STATE_TRANSITION"), AICF_Stage3Diagnostics.StateToString(previousState), AICF_Stage3Diagnostics.StateToString(state)));
		}
	}

	void SetNextAttemptAtMs(int timestamp)
	{
		m_iNextAttemptAtMs = timestamp;
	}

	int RecordSpawnAttempt()
	{
		m_iSpawnAttempt++;
		m_iTotalSpawnAttempts++;
		return m_iSpawnAttempt;
	}

	void RecordSpawnFailure(string reason)
	{
		m_sLastSpawnFailureReason = reason;
	}

	void ResetSpawnRequestContext(
		SCR_CampaignMilitaryBaseComponent target,
		int baseRevision)
	{
		m_iRequestGeneration++;
		m_iSpawnAttempt = 0;
		m_iObservedBaseRevision = baseRevision;
		m_iRequestStartedAtMs = System.GetTickCount();
		m_iLastWaitReportAtMs = 0;
		m_sLastSpawnFailureReason = string.Empty;
		m_iNextAttemptAtMs = 0;
		ClearSpawnIssueReport();
		SetTargetBase(target);
		SetState(AICF_EVehicleState.REQUESTED);
	}

	void SetObservedBaseRevision(int baseRevision)
	{
		m_iObservedBaseRevision = baseRevision;
	}

	bool MarkWaitReportDue(int intervalMs)
	{
		if (m_iLastWaitReportAtMs > 0 && System.GetTickCount(m_iLastWaitReportAtMs) < intervalMs)
			return false;
		m_iLastWaitReportAtMs = System.GetTickCount();
		return true;
	}

	bool MarkCapBlockedReported()
	{
		if (m_bCapBlockedReported)
			return false;
		m_bCapBlockedReported = true;
		return true;
	}

	// The key survives REQUESTED -> SPAWNING -> REQUESTED retries. The same
	// reason for the same runtime generation is reported once and a genuinely
	// changed reason becomes visible immediately.
	bool MarkSpawnIssueReported(string reportKey)
	{
		if (reportKey.IsEmpty() || m_sLastSpawnIssueReportKey == reportKey)
			return false;
		m_sLastSpawnIssueReportKey = reportKey;
		return true;
	}

	void ClearSpawnIssueReport()
	{
		m_sLastSpawnIssueReportKey = string.Empty;
	}

	bool BindVehicle(
		Vehicle vehicle,
		SCR_AIVehicleUsageComponent usage,
		ResourceName prefab,
		SCR_CampaignMilitaryBaseComponent spawnBase)
	{
		if (!vehicle || !usage || prefab.IsEmpty() || !spawnBase)
			return false;

		CancelBoardingApproachActions();
		m_Vehicle = vehicle;
		m_VehicleUsage = usage;
		m_VehiclePrefab = prefab;
		m_SpawnBase = spawnBase;
		m_iRecoveryCount = 0;
		m_fBestDistanceMeters = -1.0;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iLastMotionAtMs = System.GetTickCount();
		m_iLastMotionReportAtMs = 0;
		m_bHasMotionSample = false;
		m_bRouteRecoveryPending = false;
		m_bRecoveryRequiresRouteProgress = false;
		m_bRecoveryFailureReported = false;
		m_bFallbackForceExitReported = false;
		m_bFallbackExitFailureReported = false;
		m_bInfantryFallbackRestorePending = false;
		m_bBoardingRoleResetAttempted = false;
		m_bBoardingRoleRetryIssued = false;
		m_bDismountReissueAttempted = false;
		m_bDismountClearanceRecoveryAttempted = false;
		m_iDismountClearanceBlockedAtMs = 0;
		m_iDismountClearPollCount = 0;
		m_iDismountClearanceRecoveryAttempts = 0;
		m_iBoardingRoleResetAtMs = 0;
		ClearVehicleDeleteConfirmation();
		ResetCrewRecoveryPhase();
		m_BoardingPhase = AICF_EVehicleBoardingPhase.NONE;
		m_iBoardingStartedAtMs = 0;
		m_iPlannedBoardingPhaseCount = 0;
		m_iBoardingSettledPollCount = 0;
		m_iBoardingMaxLinkedCount = 0;
		m_iBoardingMaxCompartmentCount = 0;
		m_iBoardingMaxGettingInCount = 0;
		m_iBoardingMaxCharacterVehicleCount = 0;
		m_iBoardingMaxSettledCount = 0;
		m_iLastBoardingProgressAtMs = 0;
		m_fBestBoardingFarthestDistanceMeters = -1.0;
		m_bBoardingDriverPhasePlanned = false;
		m_bBoardingGunnerPhasePlanned = false;
		m_bBoardingGraceEvaluated = false;
		m_bBoardingGraceGranted = false;
		m_LastDriver = null;
		m_LastGunner = null;
		ClearCrewRecoveryTracking();
		ClearSpawnIssueReport();
		return true;
	}

	void SetTargetBase(SCR_CampaignMilitaryBaseComponent target)
	{
		m_TargetBase = target;
		m_fBestDistanceMeters = -1.0;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iLastMotionAtMs = System.GetTickCount();
		m_iLastMotionReportAtMs = 0;
		m_bHasMotionSample = false;
	}

	void SetActiveWaypoint(AIWaypoint waypoint)
	{
		m_ActiveWaypoint = waypoint;
	}

	void ClearActiveWaypoint(AIWaypoint expected = null)
	{
		if (expected && expected != m_ActiveWaypoint)
			return;
		m_ActiveWaypoint = null;
	}

	void SetLastDriver(IEntity driver)
	{
		m_LastDriver = driver;
	}

	void SetLastGunner(IEntity gunner)
	{
		m_LastGunner = gunner;
	}

	void SetBoardingPhase(AICF_EVehicleBoardingPhase phase)
	{
		m_BoardingPhase = phase;
	}

	void SetRecoveringDriver(bool recoveringDriver)
	{
		if (recoveringDriver)
			SetCrewRecoveryPhase(AICF_EVehicleCrewRecoveryPhase.DRIVER);
		else
			SetCrewRecoveryPhase(AICF_EVehicleCrewRecoveryPhase.GUNNER);
	}

	void SetCrewRecoveryPhase(AICF_EVehicleCrewRecoveryPhase phase)
	{
		m_CrewRecoveryPhase = phase;
	}

	void ResetCrewRecoveryPhase()
	{
		m_CrewRecoveryPhase = AICF_EVehicleCrewRecoveryPhase.NONE;
	}

	void TrackCrewRecovery(AIAgent agent, SCR_AIGetInVehicle action)
	{
		m_CrewRecoveryAgent = agent;
		m_CrewRecoveryAction = action;
	}

	void ClearCrewRecoveryTracking()
	{
		m_CrewRecoveryAgent = null;
		m_CrewRecoveryAction = null;
	}

	AICF_VehicleApproachActionToken FindBoardingApproachAction(AIAgent agent)
	{
		foreach (AICF_VehicleApproachActionToken token : m_aBoardingApproachActions)
		{
			if (token && token.GetAgent() == agent)
				return token;
		}
		return null;
	}

	void TrackBoardingApproachAction(
		AIAgent agent,
		SCR_AIMoveIndividuallyBehavior action,
		float distanceMeters,
		int retryCount = 0)
	{
		if (!agent || !action)
			return;
		m_aBoardingApproachActions.Insert(new AICF_VehicleApproachActionToken(
			agent,
			action,
			distanceMeters,
			retryCount));
	}

	void RemoveBoardingApproachAction(AICF_VehicleApproachActionToken expected)
	{
		if (!expected)
			return;
		m_aBoardingApproachActions.RemoveItem(expected);
	}

	int CancelBoardingApproachActions()
	{
		int cancelled;
		foreach (AICF_VehicleApproachActionToken token : m_aBoardingApproachActions)
		{
			if (!token || !token.GetAction())
				continue;
			EAIActionState state = token.GetAction().GetActionState();
			if (state == EAIActionState.COMPLETED || state == EAIActionState.FAILED)
				continue;
			token.GetAction().Fail();
			cancelled++;
		}
		m_aBoardingApproachActions.Clear();
		return cancelled;
	}

	void RestartPhaseDeadline()
	{
		m_iStateStartedAtMs = System.GetTickCount();
	}

	void BeginBoardingDeadline(
		int plannedPhaseCount,
		bool driverPhasePlanned,
		bool gunnerPhasePlanned)
	{
		// The total plan is immutable for this attempt. A phase transition must
		// never be able to reset the clock or add another timeout budget.
		if (m_iBoardingStartedAtMs > 0)
			return;
		if (plannedPhaseCount < 1)
			plannedPhaseCount = 1;
		if (plannedPhaseCount > 4)
			plannedPhaseCount = 4;

		m_iBoardingStartedAtMs = System.GetTickCount();
		m_iPlannedBoardingPhaseCount = plannedPhaseCount;
		m_iBoardingSettledPollCount = 0;
		m_iBoardingMaxLinkedCount = 0;
		m_iBoardingMaxCompartmentCount = 0;
		m_iBoardingMaxGettingInCount = 0;
		m_iBoardingMaxCharacterVehicleCount = 0;
		m_iBoardingMaxSettledCount = 0;
		m_iLastBoardingProgressAtMs = 0;
		m_fBestBoardingFarthestDistanceMeters = -1.0;
		m_bBoardingDriverPhasePlanned = driverPhasePlanned;
		m_bBoardingGunnerPhasePlanned = gunnerPhasePlanned;
		m_bBoardingGraceEvaluated = false;
		m_bBoardingGraceGranted = false;
		m_bBoardingRoleResetAttempted = false;
		m_bBoardingRoleRetryIssued = false;
		m_iBoardingRoleResetAtMs = 0;
	}

	int GetBoardingAgeMs()
	{
		if (m_iBoardingStartedAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iBoardingStartedAtMs);
	}

	bool ObserveBoardingProgress(
		int linkedCount,
		int compartmentCount,
		int gettingInCount,
		int characterVehicleCount,
		int settledCount)
	{
		bool advanced;
		if (linkedCount > m_iBoardingMaxLinkedCount)
		{
			m_iBoardingMaxLinkedCount = linkedCount;
			advanced = true;
		}
		if (compartmentCount > m_iBoardingMaxCompartmentCount)
		{
			m_iBoardingMaxCompartmentCount = compartmentCount;
			advanced = true;
		}
		if (gettingInCount > m_iBoardingMaxGettingInCount)
		{
			m_iBoardingMaxGettingInCount = gettingInCount;
			advanced = true;
		}
		if (characterVehicleCount > m_iBoardingMaxCharacterVehicleCount)
		{
			m_iBoardingMaxCharacterVehicleCount = characterVehicleCount;
			advanced = true;
		}
		if (settledCount > m_iBoardingMaxSettledCount)
		{
			m_iBoardingMaxSettledCount = settledCount;
			advanced = true;
		}

		if (advanced)
			m_iLastBoardingProgressAtMs = System.GetTickCount();
		return advanced;
	}

	bool HasRecentBoardingProgress(int maximumAgeMs)
	{
		return m_iLastBoardingProgressAtMs > 0 &&
			System.GetTickCount(m_iLastBoardingProgressAtMs) <= maximumAgeMs;
	}

	bool ObserveBoardingApproachProgress(float farthestDistanceMeters, float minimumProgressMeters)
	{
		if (farthestDistanceMeters < 0)
			return false;
		if (m_fBestBoardingFarthestDistanceMeters < 0)
		{
			m_fBestBoardingFarthestDistanceMeters = farthestDistanceMeters;
			return false;
		}
		if (farthestDistanceMeters > m_fBestBoardingFarthestDistanceMeters - minimumProgressMeters)
			return false;

		m_fBestBoardingFarthestDistanceMeters = farthestDistanceMeters;
		m_iLastBoardingProgressAtMs = System.GetTickCount();
		return true;
	}

	int RecordBoardingSettledPoll(bool allSettled)
	{
		if (!allSettled)
		{
			m_iBoardingSettledPollCount = 0;
			return 0;
		}

		m_iBoardingSettledPollCount++;
		return m_iBoardingSettledPollCount;
	}

	void ResetBoardingSettledPolls()
	{
		m_iBoardingSettledPollCount = 0;
	}

	bool EvaluateBoardingTransitionGrace(bool eligible)
	{
		if (m_bBoardingGraceEvaluated)
			return m_bBoardingGraceGranted;

		m_bBoardingGraceEvaluated = true;
		m_bBoardingGraceGranted = eligible;
		return m_bBoardingGraceGranted;
	}

	void BeginReuse(int groupGeneration, SCR_CampaignMilitaryBaseComponent target)
	{
		CancelBoardingApproachActions();
		m_iGroupGeneration = groupGeneration;
		m_iVehicleGeneration++;
		m_TargetBase = target;
		m_iRecoveryCount = 0;
		m_fBestDistanceMeters = -1.0;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iLastMotionAtMs = System.GetTickCount();
		m_iLastMotionReportAtMs = 0;
		m_bHasMotionSample = false;
		m_bCompletedTrip = false;
		m_sTerminalReason = string.Empty;
		m_bRouteRecoveryPending = false;
		m_bRecoveryRequiresRouteProgress = false;
		m_bRecoveryFailureReported = false;
		m_bFallbackForceExitReported = false;
		m_bFallbackExitFailureReported = false;
		m_bInfantryFallbackRestorePending = false;
		m_bBoardingRoleResetAttempted = false;
		m_bBoardingRoleRetryIssued = false;
		m_bDismountReissueAttempted = false;
		m_bDismountClearanceRecoveryAttempted = false;
		m_iDismountClearanceBlockedAtMs = 0;
		m_iDismountClearPollCount = 0;
		m_iDismountClearanceRecoveryAttempts = 0;
		m_iBoardingRoleResetAtMs = 0;
		ClearVehicleDeleteConfirmation();
		ResetCrewRecoveryPhase();
		m_BoardingPhase = AICF_EVehicleBoardingPhase.NONE;
		m_iBoardingStartedAtMs = 0;
		m_iPlannedBoardingPhaseCount = 0;
		m_iBoardingSettledPollCount = 0;
		m_iBoardingMaxLinkedCount = 0;
		m_iBoardingMaxCompartmentCount = 0;
		m_iBoardingMaxGettingInCount = 0;
		m_iBoardingMaxCharacterVehicleCount = 0;
		m_iBoardingMaxSettledCount = 0;
		m_iLastBoardingProgressAtMs = 0;
		m_fBestBoardingFarthestDistanceMeters = -1.0;
		m_bBoardingDriverPhasePlanned = false;
		m_bBoardingGunnerPhasePlanned = false;
		m_bBoardingGraceEvaluated = false;
		m_bBoardingGraceGranted = false;
		m_LastDriver = null;
		m_LastGunner = null;
		ClearCrewRecoveryTracking();
		ClearSpawnIssueReport();
		SetState(AICF_EVehicleState.REQUESTED);
	}

	bool ObserveProgress(float distanceMeters, float minimumProgressMeters)
	{
		if (distanceMeters < 0)
			return false;

		if (m_fBestDistanceMeters < 0 || distanceMeters <= m_fBestDistanceMeters - minimumProgressMeters)
		{
			m_fBestDistanceMeters = distanceMeters;
			m_iLastProgressAtMs = System.GetTickCount();
			return true;
		}
		return false;
	}

	bool ObserveMotion(vector position, float minimumMovementMeters)
	{
		if (!m_bHasMotionSample)
		{
			m_vLastMotionPosition = position;
			m_iLastMotionAtMs = System.GetTickCount();
			m_bHasMotionSample = true;
			return false;
		}

		if (vector.DistanceSqXZ(position, m_vLastMotionPosition) < minimumMovementMeters * minimumMovementMeters)
			return false;

		m_vLastMotionPosition = position;
		m_iLastMotionAtMs = System.GetTickCount();
		return true;
	}

	bool IsStationary(int timeoutMs)
	{
		return m_iLastMotionAtMs > 0 && System.GetTickCount(m_iLastMotionAtMs) >= timeoutMs;
	}

	bool MarkMotionReportDue(int intervalMs)
	{
		if (m_iLastMotionReportAtMs > 0 && System.GetTickCount(m_iLastMotionReportAtMs) < intervalMs)
			return false;

		m_iLastMotionReportAtMs = System.GetTickCount();
		return true;
	}

	bool IsRouteStalled(int timeoutMs)
	{
		return m_iLastProgressAtMs > 0 && System.GetTickCount(m_iLastProgressAtMs) >= timeoutMs;
	}

	int GetRouteProgressAgeMs()
	{
		if (m_iLastProgressAtMs <= 0)
			return int.MAX;

		return System.GetTickCount(m_iLastProgressAtMs);
	}

	int GetMotionAgeMs()
	{
		if (m_iLastMotionAtMs <= 0)
			return int.MAX;

		return System.GetTickCount(m_iLastMotionAtMs);
	}

	void RecordRecovery(float distanceMeters, vector position, bool requireRouteProgress)
	{
		m_iRecoveryCount++;
		m_bRouteRecoveryPending = true;
		m_bRecoveryRequiresRouteProgress = requireRouteProgress;
		m_fBestDistanceMeters = distanceMeters;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iLastMotionAtMs = System.GetTickCount();
		m_vLastMotionPosition = position;
		m_bHasMotionSample = true;
	}

	void RecordCrewRecovery()
	{
		m_iRecoveryCount++;
		m_iLastProgressAtMs = System.GetTickCount();
		m_iLastMotionAtMs = System.GetTickCount();
	}

	void ConfirmRouteRecovery()
	{
		m_bRouteRecoveryPending = false;
		m_bRecoveryRequiresRouteProgress = false;
	}

	bool MarkRecoveryFailureReported()
	{
		if (m_bRecoveryFailureReported)
			return false;
		m_bRecoveryFailureReported = true;
		return true;
	}

	bool MarkFallbackExitFailureReported()
	{
		if (m_bFallbackExitFailureReported)
			return false;
		m_bFallbackExitFailureReported = true;
		return true;
	}

	bool MarkFallbackForceExitReported()
	{
		if (m_bFallbackForceExitReported)
			return false;
		m_bFallbackForceExitReported = true;
		return true;
	}

	bool BeginBoardingRoleReset()
	{
		if (m_bBoardingRoleResetAttempted)
			return false;

		m_bBoardingRoleResetAttempted = true;
		m_bBoardingRoleRetryIssued = false;
		m_iBoardingRoleResetAtMs = System.GetTickCount();
		return true;
	}

	void MarkBoardingRoleRetryIssued()
	{
		m_bBoardingRoleRetryIssued = true;
	}

	int GetBoardingRoleResetAgeMs()
	{
		if (m_iBoardingRoleResetAtMs <= 0)
			return 0;

		return System.GetTickCount(m_iBoardingRoleResetAtMs);
	}

	void ResetDismountReissue()
	{
		m_bDismountReissueAttempted = false;
		m_bDismountClearanceRecoveryAttempted = false;
		m_iDismountClearanceBlockedAtMs = 0;
		m_iDismountClearPollCount = 0;
		m_iDismountClearanceRecoveryAttempts = 0;
	}

	bool MarkDismountReissueAttempted()
	{
		if (m_bDismountReissueAttempted)
			return false;

		m_bDismountReissueAttempted = true;
		return true;
	}

	int ObserveDismountClearance(bool safelyClear, bool physicalOnlyBlocked)
	{
		if (safelyClear)
		{
			m_iDismountClearPollCount++;
			m_iDismountClearanceBlockedAtMs = 0;
			return m_iDismountClearPollCount;
		}

		m_iDismountClearPollCount = 0;
		if (!physicalOnlyBlocked)
			m_iDismountClearanceBlockedAtMs = 0;
		else if (m_iDismountClearanceBlockedAtMs <= 0)
			m_iDismountClearanceBlockedAtMs = System.GetTickCount();
		return 0;
	}

	int GetDismountClearanceBlockedAgeMs()
	{
		if (m_iDismountClearanceBlockedAtMs <= 0)
			return 0;
		return System.GetTickCount(m_iDismountClearanceBlockedAtMs);
	}

	bool CanAttemptDismountClearanceRecovery(int maximumAttempts)
	{
		return m_iDismountClearanceRecoveryAttempts < maximumAttempts;
	}

	void RecordDismountClearanceRecoveryAttempt(bool relocated)
	{
		m_iDismountClearanceRecoveryAttempts++;
		if (relocated)
			m_bDismountClearanceRecoveryAttempted = true;
		// Rate-limit another terrain search. A successful relocation may still
		// leave another protected member inside the vehicle bounds, so success is
		// not a global one-shot latch; the bounded attempt counter remains final.
		m_iDismountClearanceBlockedAtMs = System.GetTickCount();
	}

	void MarkInfantryFallbackRestorePending()
	{
		m_bInfantryFallbackRestorePending = true;
	}

	void ClearInfantryFallbackRestorePending()
	{
		m_bInfantryFallbackRestorePending = false;
	}

	void MarkTripCompleted()
	{
		m_bCompletedTrip = true;
	}

	void BeginVehicleDeleteConfirmation(EntityID entityId, string entityIdString, string rplId, vector origin)
	{
		m_VehicleDeleteEntityId = entityId;
		m_sVehicleDeleteEntityId = entityIdString;
		m_sVehicleDeleteRplId = rplId;
		m_vVehicleDeleteOrigin = origin;
		m_iVehicleDeleteRequestedAtMs = System.GetTickCount();
		m_iLastVehicleDeleteAttemptAtMs = m_iVehicleDeleteRequestedAtMs;
		m_iVehicleDeleteAttempts = 1;
		m_bVehicleDeleteFailureReported = false;
	}

	bool HasVehicleDeleteConfirmationPending()
	{
		return m_iVehicleDeleteRequestedAtMs > 0;
	}

	int GetVehicleDeleteAgeMs()
	{
		if (m_iVehicleDeleteRequestedAtMs <= 0)
			return 0;
		return System.GetTickCount(m_iVehicleDeleteRequestedAtMs);
	}

	bool CanRetryVehicleDelete(int retryIntervalMs, int maximumAttempts)
	{
		return HasVehicleDeleteConfirmationPending() && m_iVehicleDeleteAttempts < maximumAttempts &&
			System.GetTickCount(m_iLastVehicleDeleteAttemptAtMs) >= retryIntervalMs;
	}

	void RecordVehicleDeleteRetry()
	{
		m_iVehicleDeleteAttempts++;
		m_iLastVehicleDeleteAttemptAtMs = System.GetTickCount();
	}

	bool MarkVehicleDeleteFailureReported()
	{
		if (m_bVehicleDeleteFailureReported)
			return false;
		m_bVehicleDeleteFailureReported = true;
		return true;
	}

	void ClearVehicleReferenceAfterDeleteRequest()
	{
		m_Vehicle = null;
		m_VehicleUsage = null;
	}

	void ClearVehicleDeleteConfirmation()
	{
		m_VehicleDeleteEntityId = EntityID.INVALID;
		m_sVehicleDeleteEntityId = string.Empty;
		m_sVehicleDeleteRplId = string.Empty;
		m_vVehicleDeleteOrigin = vector.Zero;
		m_iVehicleDeleteRequestedAtMs = 0;
		m_iLastVehicleDeleteAttemptAtMs = 0;
		m_iVehicleDeleteAttempts = 0;
		m_bVehicleDeleteFailureReported = false;
	}

	void ScheduleCleanup(int cleanupAtMs)
	{
		if (m_iCleanupAtMs <= 0 || cleanupAtMs < m_iCleanupAtMs)
			m_iCleanupAtMs = cleanupAtMs;
	}

	void MarkReleasedToWorldPool()
	{
		m_iWorldPoolReleasedAtMs = System.GetTickCount();
		m_bWorldPoolRetirementRequested = false;
		m_iCleanupClearStartedAtMs = 0;
		m_iLastCleanupDeferredReportAtMs = 0;
	}

	void RequestWorldPoolRetirement()
	{
		if (!m_bWorldPoolRetirementRequested)
			m_iCleanupClearStartedAtMs = 0;
		m_bWorldPoolRetirementRequested = true;
	}

	void ClearWorldPoolRetirementRequest()
	{
		m_bWorldPoolRetirementRequested = false;
		m_iCleanupClearStartedAtMs = 0;
	}

	int ObserveCleanupClear(bool safelyClear)
	{
		if (!safelyClear)
		{
			m_iCleanupClearStartedAtMs = 0;
			return 0;
		}
		if (m_iCleanupClearStartedAtMs <= 0)
			m_iCleanupClearStartedAtMs = System.GetTickCount();
		return System.GetTickCount(m_iCleanupClearStartedAtMs);
	}

	bool MarkCleanupDeferredReportDue(int intervalMs)
	{
		if (m_iLastCleanupDeferredReportAtMs > 0 &&
			System.GetTickCount(m_iLastCleanupDeferredReportAtMs) < intervalMs)
		{
			return false;
		}
		m_iLastCleanupDeferredReportAtMs = System.GetTickCount();
		return true;
	}

	string GetVehicleId()
	{
		if (!m_Vehicle)
		{
			if (!m_sVehicleDeleteEntityId.IsEmpty())
				return m_sVehicleDeleteEntityId;
			return "NONE";
		}
		return m_Vehicle.GetID().ToString();
	}

	string DescribeContext(string reason)
	{
		return string.Format(
			"faction=%1 slot=%2 group_generation=%3 vehicle_generation=%4 vehicle=%5 kind=%6 state=%7 reason=%8",
			m_sFactionKey,
			m_iSlotId,
			m_iGroupGeneration,
			m_iVehicleGeneration,
			GetVehicleId(),
			AICF_Stage3Diagnostics.KindToString(m_Kind),
			AICF_Stage3Diagnostics.StateToString(m_State),
			reason);
	}
}
