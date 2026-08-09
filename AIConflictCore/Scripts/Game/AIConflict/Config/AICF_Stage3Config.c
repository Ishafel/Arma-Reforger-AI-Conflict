// Ground-vehicle settings are disabled by default so loading the Stage 3 code
// without explicit CLI opt-in remains behaviorally equivalent to Stage 2.
class AICF_Stage3Config
{
	static const int DEFAULT_TRANSPORTS_PER_FACTION = 1;
	static const int DEFAULT_ARMED_LIGHT_PER_FACTION = 0;
	static const int DEFAULT_MAX_VEHICLES_PER_FACTION = 2;
	static const int DEFAULT_BOARDING_TIMEOUT_MS = 60000;
	static const int DEFAULT_STUCK_TIMEOUT_MS = 120000;
	static const float DEFAULT_PROGRESS_METERS = 25.0;
	static const int DEFAULT_MAX_RECOVERIES = 2;
	static const float DEFAULT_DISEMBARK_DISTANCE_METERS = 150.0;
	static const int DEFAULT_RETRY_INTERVAL_MS = 10000;
	static const int DEFAULT_CLEANUP_DELAY_MS = 60000;
	static const float DEFAULT_MINIMUM_ROUTE_METERS = 400.0;
	static const float DEFAULT_MAXIMUM_REUSE_DISTANCE_METERS = 250.0;
	static const float DEFAULT_MAXIMUM_SPAWN_DISTANCE_METERS = 2000.0;
	static const float DEFAULT_COHESION_DISTANCE_METERS = 100.0;

	protected bool m_bVehiclesEnabled;
	protected int m_iTransportVehiclesPerFaction;
	protected int m_iArmedLightVehiclesPerFaction;
	protected int m_iMaxVehiclesPerFaction;
	protected int m_iBoardingTimeoutMs;
	protected int m_iStuckTimeoutMs;
	protected float m_fProgressMeters;
	protected int m_iMaxRecoveries;
	protected float m_fDismountDistanceMeters;
	protected int m_iRetryIntervalMs;
	protected int m_iCleanupDelayMs;
	protected float m_fMinimumRouteMeters;
	protected float m_fMaximumReuseDistanceMeters;
	protected float m_fMaximumSpawnDistanceMeters;
	protected float m_fCohesionDistanceMeters;

	void AICF_Stage3Config()
	{
		m_bVehiclesEnabled = false;
		m_iTransportVehiclesPerFaction = DEFAULT_TRANSPORTS_PER_FACTION;
		m_iArmedLightVehiclesPerFaction = DEFAULT_ARMED_LIGHT_PER_FACTION;
		m_iMaxVehiclesPerFaction = DEFAULT_MAX_VEHICLES_PER_FACTION;
		m_iBoardingTimeoutMs = DEFAULT_BOARDING_TIMEOUT_MS;
		m_iStuckTimeoutMs = DEFAULT_STUCK_TIMEOUT_MS;
		m_fProgressMeters = DEFAULT_PROGRESS_METERS;
		m_iMaxRecoveries = DEFAULT_MAX_RECOVERIES;
		m_fDismountDistanceMeters = DEFAULT_DISEMBARK_DISTANCE_METERS;
		m_iRetryIntervalMs = DEFAULT_RETRY_INTERVAL_MS;
		m_iCleanupDelayMs = DEFAULT_CLEANUP_DELAY_MS;
		m_fMinimumRouteMeters = DEFAULT_MINIMUM_ROUTE_METERS;
		m_fMaximumReuseDistanceMeters = DEFAULT_MAXIMUM_REUSE_DISTANCE_METERS;
		m_fMaximumSpawnDistanceMeters = DEFAULT_MAXIMUM_SPAWN_DISTANCE_METERS;
		m_fCohesionDistanceMeters = DEFAULT_COHESION_DISTANCE_METERS;
		ApplyCLIOverrides();
	}

	bool GetVehiclesEnabled() { return m_bVehiclesEnabled; }
	int GetTransportVehiclesPerFaction() { return m_iTransportVehiclesPerFaction; }
	int GetArmedLightVehiclesPerFaction() { return m_iArmedLightVehiclesPerFaction; }
	int GetMaxVehiclesPerFaction() { return m_iMaxVehiclesPerFaction; }
	int GetBoardingTimeoutMs() { return m_iBoardingTimeoutMs; }
	int GetStuckTimeoutMs() { return m_iStuckTimeoutMs; }
	float GetProgressMeters() { return m_fProgressMeters; }
	int GetMaxRecoveries() { return m_iMaxRecoveries; }
	float GetDismountDistanceMeters() { return m_fDismountDistanceMeters; }
	int GetRetryIntervalMs() { return m_iRetryIntervalMs; }
	int GetCleanupDelayMs() { return m_iCleanupDelayMs; }
	float GetMinimumRouteMeters() { return m_fMinimumRouteMeters; }
	float GetMaximumReuseDistanceMeters() { return m_fMaximumReuseDistanceMeters; }
	float GetMaximumSpawnDistanceMeters() { return m_fMaximumSpawnDistanceMeters; }
	float GetCohesionDistanceMeters() { return m_fCohesionDistanceMeters; }

	protected void ApplyCLIOverrides()
	{
		string value;
		if (System.GetCLIParam("aicfVehiclesEnabled", value))
			m_bVehiclesEnabled = value.ToInt() > 0;
		if (System.GetCLIParam("aicfTransportVehiclesPerFaction", value))
			m_iTransportVehiclesPerFaction = ClampInt(value.ToInt(), 0, AICF_Stage1Config.ATTACK_SLOTS_PER_FACTION);
		if (System.GetCLIParam("aicfArmedLightVehiclesPerFaction", value))
			m_iArmedLightVehiclesPerFaction = ClampInt(value.ToInt(), 0, AICF_Stage1Config.ATTACK_SLOTS_PER_FACTION);
		if (System.GetCLIParam("aicfMaxVehiclesPerFaction", value))
			m_iMaxVehiclesPerFaction = ClampInt(value.ToInt(), 0, AICF_Stage1Config.GROUP_SLOTS_PER_FACTION);
		if (System.GetCLIParam("aicfVehicleBoardingTimeoutMs", value))
			m_iBoardingTimeoutMs = ClampInt(value.ToInt(), 10000, 600000);
		if (System.GetCLIParam("aicfVehicleStuckTimeoutMs", value))
			m_iStuckTimeoutMs = ClampInt(value.ToInt(), 30000, 3600000);
		if (System.GetCLIParam("aicfVehicleProgressMeters", value))
			m_fProgressMeters = ClampFloat(value.ToFloat(), 1.0, 500.0);
		if (System.GetCLIParam("aicfVehicleMaxRecoveries", value))
			m_iMaxRecoveries = ClampInt(value.ToInt(), 0, 20);
		if (System.GetCLIParam("aicfVehicleDismountDistanceMeters", value))
			m_fDismountDistanceMeters = ClampFloat(value.ToFloat(), 30.0, 500.0);
		if (System.GetCLIParam("aicfVehicleRetryIntervalMs", value))
			m_iRetryIntervalMs = ClampInt(value.ToInt(), 1000, 600000);
		if (System.GetCLIParam("aicfVehicleCleanupDelayMs", value))
			m_iCleanupDelayMs = ClampInt(value.ToInt(), 0, 1800000);
		if (System.GetCLIParam("aicfVehicleMinimumRouteMeters", value))
			m_fMinimumRouteMeters = ClampFloat(value.ToFloat(), 100.0, 5000.0);
		if (System.GetCLIParam("aicfVehicleMaximumReuseDistanceMeters", value))
			m_fMaximumReuseDistanceMeters = ClampFloat(value.ToFloat(), 25.0, 1000.0);
		if (System.GetCLIParam("aicfVehicleMaximumSpawnDistanceMeters", value))
			m_fMaximumSpawnDistanceMeters = ClampFloat(value.ToFloat(), 100.0, 10000.0);
		if (System.GetCLIParam("aicfVehicleCohesionDistanceMeters", value))
			m_fCohesionDistanceMeters = ClampFloat(value.ToFloat(), 25.0, 500.0);
	}

	protected int ClampInt(int value, int minimum, int maximum)
	{
		if (value < minimum)
			return minimum;
		if (value > maximum)
			return maximum;
		return value;
	}

	protected float ClampFloat(float value, float minimum, float maximum)
	{
		if (value < minimum)
			return minimum;
		if (value > maximum)
			return maximum;
		return value;
	}
}
