// Единый effective config; invalid CLI останавливает запуск, а не меняет policy.
class AICF_LogisticsConfig
{
	static const float RESOURCE_EPSILON = 0.01;
	static const int SERVICE_SLOT_FIRST = 1000000;
	static const int SPAWN_TIMEOUT_MS = 30000;
	static const int LEG_TIMEOUT_MS = 600000;
	static const int PROGRESS_TIMEOUT_MS = 60000;
	static const int MAX_ROUTE_RETRIES = 2;
	static const int REGISTRY_BUDGET = 4;
	static const int SEARCH_BUDGET = 16;
	float m_fRequestBelowPercent = 40;
	float m_fTargetPercent = 80;
	float m_fDonateAbovePercent = 90;
	float m_fDonorKeepPercent = 80;
	float m_fNeutralSourceReserveSupplies;
	float m_fOwnedSourceReserveSupplies = 500;
	float m_fMinDispatchSupplies = 50;
	float m_fMaxCargoPerTrip;
	int m_iWorkersPerDepot = 1;
	int m_iPlannerIntervalMs = 10000;
	int m_iWorkerPollMs = 1000;
	int m_iReservationTtlMs = 30000;
	int m_iReplacementCooldownMs = 60000;
	int m_iBlockedRetryMs = 60000;
	int m_iReturnSearchMaxAttempts = 3;
	int m_iIdleRetireMs = 120000;
	float m_fArrivalRadiusM = 15;
	float m_fStationarySpeedMps = 0.5;
	int m_iStationaryHoldMs = 3000;
	protected string m_sError;

	void AICF_LogisticsConfig(bool readCLI = true)
	{
		if (!readCLI)
			return;
		ReadFloat("RequestBelowPercent", m_fRequestBelowPercent);
		ReadFloat("TargetPercent", m_fTargetPercent);
		ReadFloat("DonateAbovePercent", m_fDonateAbovePercent);
		ReadFloat("DonorKeepPercent", m_fDonorKeepPercent);
		ReadFloat("NeutralSourceReserveSupplies", m_fNeutralSourceReserveSupplies);
		ReadFloat("OwnedSourceReserveSupplies", m_fOwnedSourceReserveSupplies);
		ReadFloat("MinDispatchSupplies", m_fMinDispatchSupplies);
		ReadFloat("MaxCargoPerTrip", m_fMaxCargoPerTrip);
		ReadInt("WorkersPerDepot", m_iWorkersPerDepot);
		ReadInt("PlannerIntervalMs", m_iPlannerIntervalMs);
		ReadInt("WorkerPollMs", m_iWorkerPollMs);
		ReadInt("ReservationTtlMs", m_iReservationTtlMs);
		ReadInt("ReplacementCooldownMs", m_iReplacementCooldownMs);
		ReadInt("BlockedRetryMs", m_iBlockedRetryMs);
		ReadInt("ReturnSearchMaxAttempts", m_iReturnSearchMaxAttempts);
		ReadInt("IdleRetireMs", m_iIdleRetireMs);
		ReadFloat("ArrivalRadiusM", m_fArrivalRadiusM);
		ReadFloat("StationarySpeedMps", m_fStationarySpeedMps);
		ReadInt("StationaryHoldMs", m_iStationaryHoldMs);
	}

	// Только десятичная запись. Запрещает ToFloat("garbage") == 0 и overflow.
	static bool Decimal(string text, bool integer, out float value)
	{
		value = 0;
		if (text.IsEmpty() || text.Length() > 16)
			return false;
		int digits, dots;
		for (int i; i < text.Length(); i++)
		{
			string c = text.Substring(i, 1);
			if (c == "." && !integer)
			{
				dots++;
				if (dots > 1) return false;
			}
			else if ("0123456789".Contains(c))
				digits++;
			else
				return false;
		}
		value = text.ToFloat();
		return digits > 0 && value >= 0 && value <= 100000000 && value == value;
	}

	protected void ReadFloat(string key, inout float value)
	{
		string text;
		if (!System.GetCLIParam("aicfLogistics" + key, text)) return;
		float parsed;
		if (!Decimal(text, false, parsed))
			m_sError = "aicfLogistics" + key + " requires a finite nonnegative decimal";
		else
			value = parsed;
	}

	protected void ReadInt(string key, inout int value)
	{
		string text;
		if (!System.GetCLIParam("aicfLogistics" + key, text)) return;
		float parsed;
		if (!Decimal(text, true, parsed))
			m_sError = "aicfLogistics" + key + " requires a nonnegative integer";
		else
			value = text.ToInt();
	}

	bool Validate(out string reason)
	{
		reason = m_sError;
		if (!reason.IsEmpty()) return false;
		if (!(m_fRequestBelowPercent >= 0 && m_fRequestBelowPercent < m_fTargetPercent &&
			m_fTargetPercent <= m_fDonorKeepPercent && m_fDonorKeepPercent < m_fDonateAbovePercent && m_fDonateAbovePercent <= 100))
			reason = "required: 0 <= request < target <= keep < donate <= 100";
		else if (m_iWorkersPerDepot < 1 || m_iWorkersPerDepot > 4 || m_iReturnSearchMaxAttempts < 1 || m_iReturnSearchMaxAttempts > 10)
			reason = "WorkersPerDepot=1..4; ReturnSearchMaxAttempts=1..10";
		else if (m_iPlannerIntervalMs <= 0 || m_iWorkerPollMs <= 0 || m_iReservationTtlMs <= 0 ||
			m_iReplacementCooldownMs <= 0 || m_iBlockedRetryMs <= 0 || m_iIdleRetireMs <= 0 || m_iStationaryHoldMs <= 0 ||
			!(m_fArrivalRadiusM > 0) || !(m_fStationarySpeedMps > 0))
			reason = "intervals, arrival radius and stationary speed must be positive";
		else if (!(m_fNeutralSourceReserveSupplies >= 0 && m_fOwnedSourceReserveSupplies >= 0 && m_fMinDispatchSupplies >= 0 && m_fMaxCargoPerTrip >= 0))
			reason = "supply limits must be nonnegative";
		return reason.IsEmpty();
	}

	void Log()
	{
		array<string> obsolete = {"aicfSupplyDeliveryIntervalMs", "aicfSupplyDeliveryPackage", "aicfSupplyDeliveryBaseTravelMs", "aicfSupplyDeliveryPerHopMs", "aicfMaxSupplyShipmentsPerFaction"};
		foreach (string key : obsolete)
		{
			string ignored;
			if (System.GetCLIParam(key, ignored)) AICF_Stage4Diagnostics.Warning("LOGISTICS_CONFIG_DEPRECATED", "option=" + key + " ignored=1 replacement=aicfLogistics_config");
		}
		string details = string.Format("schema_version=2 request_below_percent=%1 target_percent=%2 donate_above_percent=%3 donor_keep_percent=%4 neutral_reserve=%5 owned_reserve=%6 min_dispatch=%7 max_cargo=%8 workers_per_depot=%9",
			m_fRequestBelowPercent, m_fTargetPercent, m_fDonateAbovePercent, m_fDonorKeepPercent, m_fNeutralSourceReserveSupplies,
			m_fOwnedSourceReserveSupplies, m_fMinDispatchSupplies, m_fMaxCargoPerTrip, m_iWorkersPerDepot);
		details += string.Format(" planner_ms=%1 poll_ms=%2 reservation_ttl_ms=%3 replacement_ms=%4 blocked_retry_ms=%5 return_attempts=%6 idle_retire_ms=%7 arrival_m=%8 stationary_mps=%9",
			m_iPlannerIntervalMs, m_iWorkerPollMs, m_iReservationTtlMs, m_iReplacementCooldownMs, m_iBlockedRetryMs,
			m_iReturnSearchMaxAttempts, m_iIdleRetireMs, m_fArrivalRadiusM, m_fStationarySpeedMps);
		details += string.Format(" stationary_hold_ms=%1 resource_epsilon=%2 auxiliary_cost=0 shared_fleet=1 route_cost=EUCLIDEAN_APPROXIMATION", m_iStationaryHoldMs, RESOURCE_EPSILON);
		AICF_Stage4Diagnostics.Info("LOGISTICS_CONFIG", details);
	}
}
