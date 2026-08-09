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
	protected int m_iStateStartedAtMs;
	protected int m_iNextAttemptAtMs;
	protected int m_iLastProgressAtMs;
	protected int m_iRecoveryCount;
	protected int m_iCleanupAtMs;
	protected float m_fBestDistanceMeters = -1.0;
	protected bool m_bCapBlockedReported;
	protected bool m_bSpawnBlockedReported;
	protected bool m_bCompletedTrip;
	protected bool m_bRecoveringDriver;
	protected bool m_bRouteRecoveryPending;
	protected bool m_bRecoveryFailureReported;
	protected string m_sTerminalReason;

	protected ResourceName m_VehiclePrefab;
	protected SCR_AIGroup m_Group;
	protected Vehicle m_Vehicle;
	protected SCR_AIVehicleUsageComponent m_VehicleUsage;
	protected SCR_CampaignMilitaryBaseComponent m_SpawnBase;
	protected SCR_CampaignMilitaryBaseComponent m_TargetBase;
	protected AIWaypoint m_ActiveWaypoint;
	protected IEntity m_LastDriver;
	protected IEntity m_LastGunner;

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
		SetState(AICF_EVehicleState.REQUESTED);
	}

	FactionKey GetFactionKey() { return m_sFactionKey; }
	int GetSlotId() { return m_iSlotId; }
	int GetGroupGeneration() { return m_iGroupGeneration; }
	int GetVehicleGeneration() { return m_iVehicleGeneration; }
	AICF_EVehicleKind GetKind() { return m_Kind; }
	AICF_EVehicleState GetState() { return m_State; }
	int GetStateStartedAtMs() { return m_iStateStartedAtMs; }
	int GetNextAttemptAtMs() { return m_iNextAttemptAtMs; }
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
	bool HasCompletedTrip() { return m_bCompletedTrip; }
	bool IsRecoveringDriver() { return m_bRecoveringDriver; }
	bool HasPendingRouteRecovery() { return m_bRouteRecoveryPending; }
	string GetTerminalReason() { return m_sTerminalReason; }

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
		AICF_EVehicleState previousState = m_State;
		m_State = state;
		m_iStateStartedAtMs = System.GetTickCount();
		m_bCapBlockedReported = false;
		m_bSpawnBlockedReported = false;
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

	bool MarkCapBlockedReported()
	{
		if (m_bCapBlockedReported)
			return false;
		m_bCapBlockedReported = true;
		return true;
	}

	bool MarkSpawnBlockedReported()
	{
		if (m_bSpawnBlockedReported)
			return false;
		m_bSpawnBlockedReported = true;
		return true;
	}

	bool BindVehicle(
		Vehicle vehicle,
		SCR_AIVehicleUsageComponent usage,
		ResourceName prefab,
		SCR_CampaignMilitaryBaseComponent spawnBase)
	{
		if (!vehicle || !usage || prefab.IsEmpty() || !spawnBase)
			return false;

		m_Vehicle = vehicle;
		m_VehicleUsage = usage;
		m_VehiclePrefab = prefab;
		m_SpawnBase = spawnBase;
		m_iRecoveryCount = 0;
		m_fBestDistanceMeters = -1.0;
		m_iLastProgressAtMs = System.GetTickCount();
		m_bRouteRecoveryPending = false;
		m_bRecoveryFailureReported = false;
		m_bRecoveringDriver = false;
		m_LastDriver = null;
		m_LastGunner = null;
		return true;
	}

	void SetTargetBase(SCR_CampaignMilitaryBaseComponent target)
	{
		m_TargetBase = target;
		m_fBestDistanceMeters = -1.0;
		m_iLastProgressAtMs = System.GetTickCount();
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

	void SetRecoveringDriver(bool recoveringDriver)
	{
		m_bRecoveringDriver = recoveringDriver;
	}

	void BeginReuse(int groupGeneration, SCR_CampaignMilitaryBaseComponent target)
	{
		m_iGroupGeneration = groupGeneration;
		m_iVehicleGeneration++;
		m_TargetBase = target;
		m_iRecoveryCount = 0;
		m_fBestDistanceMeters = -1.0;
		m_iLastProgressAtMs = System.GetTickCount();
		m_bCompletedTrip = false;
		m_sTerminalReason = string.Empty;
		m_bRouteRecoveryPending = false;
		m_bRecoveryFailureReported = false;
		m_bRecoveringDriver = false;
		m_LastDriver = null;
		m_LastGunner = null;
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

	bool IsStuck(int timeoutMs)
	{
		return m_iLastProgressAtMs > 0 && System.GetTickCount(m_iLastProgressAtMs) >= timeoutMs;
	}

	void RecordRecovery(float distanceMeters)
	{
		m_iRecoveryCount++;
		m_bRouteRecoveryPending = true;
		m_fBestDistanceMeters = distanceMeters;
		m_iLastProgressAtMs = System.GetTickCount();
	}

	void RecordCrewRecovery()
	{
		m_iRecoveryCount++;
		m_iLastProgressAtMs = System.GetTickCount();
	}

	void ConfirmRouteRecovery()
	{
		m_bRouteRecoveryPending = false;
	}

	bool MarkRecoveryFailureReported()
	{
		if (m_bRecoveryFailureReported)
			return false;
		m_bRecoveryFailureReported = true;
		return true;
	}

	void MarkTripCompleted()
	{
		m_bCompletedTrip = true;
	}

	void ScheduleCleanup(int cleanupAtMs)
	{
		m_iCleanupAtMs = cleanupAtMs;
	}

	string GetVehicleId()
	{
		if (!m_Vehicle)
			return "NONE";
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
