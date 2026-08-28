// Economy and supply pacing are permanent parts of the product configuration.
// Runtime tuning remains available, but the subsystem has no opt-out.
class AICF_Stage4Config
{
	static const int DEFAULT_REPLACEMENT_SUPPLY_COST = 500;
	static const int DEFAULT_HEALTHY_STOCK_GROUPS = 2;
	static const int DEFAULT_HEALTHY_PACE_PERCENT = 100;
	static const int DEFAULT_STRAINED_PACE_PERCENT = 67;
	static const int DEFAULT_ISOLATED_PACE_PERCENT = 50;
	static const int DEFAULT_BLOCKED_PACE_PERCENT = 0;
	static const int DEFAULT_RETRY_INTERVAL_MS = 5000;
	static const int DEFAULT_DELIVERY_INTERVAL_MS = 60000;
	static const int DEFAULT_DELIVERY_PACKAGE_SUPPLIES = 500;
	static const int DEFAULT_DELIVERY_BASE_TRAVEL_MS = 30000;
	static const int DEFAULT_DELIVERY_PER_HOP_MS = 15000;
	static const int DEFAULT_MAX_SHIPMENTS_PER_FACTION = 2;
	static const int DEFAULT_SOURCE_RESERVE_GROUPS = 1;
	static const int DEFAULT_HEARTBEAT_INTERVAL_MS = 60000;

	protected int m_iReplacementSupplyCost;
	protected int m_iHealthyStockGroups;
	protected int m_iHealthyPacePercent;
	protected int m_iStrainedPacePercent;
	protected int m_iIsolatedPacePercent;
	protected int m_iBlockedPacePercent;
	protected int m_iRetryIntervalMs;
	protected int m_iDeliveryIntervalMs;
	protected int m_iDeliveryPackageSupplies;
	protected int m_iDeliveryBaseTravelMs;
	protected int m_iDeliveryPerHopMs;
	protected int m_iMaxShipmentsPerFaction;
	protected int m_iSourceReserveGroups;
	protected int m_iHeartbeatIntervalMs;

	void AICF_Stage4Config()
	{
		m_iReplacementSupplyCost = DEFAULT_REPLACEMENT_SUPPLY_COST;
		m_iHealthyStockGroups = DEFAULT_HEALTHY_STOCK_GROUPS;
		m_iHealthyPacePercent = DEFAULT_HEALTHY_PACE_PERCENT;
		m_iStrainedPacePercent = DEFAULT_STRAINED_PACE_PERCENT;
		m_iIsolatedPacePercent = DEFAULT_ISOLATED_PACE_PERCENT;
		m_iBlockedPacePercent = DEFAULT_BLOCKED_PACE_PERCENT;
		m_iRetryIntervalMs = DEFAULT_RETRY_INTERVAL_MS;
		m_iDeliveryIntervalMs = DEFAULT_DELIVERY_INTERVAL_MS;
		m_iDeliveryPackageSupplies = DEFAULT_DELIVERY_PACKAGE_SUPPLIES;
		m_iDeliveryBaseTravelMs = DEFAULT_DELIVERY_BASE_TRAVEL_MS;
		m_iDeliveryPerHopMs = DEFAULT_DELIVERY_PER_HOP_MS;
		m_iMaxShipmentsPerFaction = DEFAULT_MAX_SHIPMENTS_PER_FACTION;
		m_iSourceReserveGroups = DEFAULT_SOURCE_RESERVE_GROUPS;
		m_iHeartbeatIntervalMs = DEFAULT_HEARTBEAT_INTERVAL_MS;
		ApplyCLIOverrides();
	}

	// Retained as a source-compatible accessor for dependent addons.
	bool GetEconomyEnabled() { return true; }
	int GetReplacementSupplyCost() { return m_iReplacementSupplyCost; }
	int GetReplacementSupplyCostForSize(int desiredSize)
	{
		int clampedSize = ClampInt(
			desiredSize,
			AICF_Stage1Config.MIN_GROUP_SIZE,
			AICF_Stage1Config.MAX_GROUP_SIZE);
		// Keep the configured price as the cost of the default four-person
		// roster and round larger/smaller custom rosters up to whole supplies.
		return Math.Max(
			1,
			(m_iReplacementSupplyCost * clampedSize +
				AICF_Stage1Config.DEFAULT_GROUP_SIZE - 1) /
				AICF_Stage1Config.DEFAULT_GROUP_SIZE);
	}
	int GetHealthyStockGroups() { return m_iHealthyStockGroups; }
	int GetHealthyPacePercent() { return m_iHealthyPacePercent; }
	int GetStrainedPacePercent() { return m_iStrainedPacePercent; }
	int GetIsolatedPacePercent() { return m_iIsolatedPacePercent; }
	int GetBlockedPacePercent() { return m_iBlockedPacePercent; }
	int GetRetryIntervalMs() { return m_iRetryIntervalMs; }
	int GetDeliveryIntervalMs() { return m_iDeliveryIntervalMs; }
	int GetDeliveryPackageSupplies() { return m_iDeliveryPackageSupplies; }
	int GetDeliveryBaseTravelMs() { return m_iDeliveryBaseTravelMs; }
	int GetDeliveryPerHopMs() { return m_iDeliveryPerHopMs; }
	int GetMaxShipmentsPerFaction() { return m_iMaxShipmentsPerFaction; }
	int GetSourceReserveGroups() { return m_iSourceReserveGroups; }
	int GetHeartbeatIntervalMs() { return m_iHeartbeatIntervalMs; }

	int GetSourceReserveSupplies()
	{
		return m_iReplacementSupplyCost * m_iSourceReserveGroups;
	}

	int GetPacePercent(AICF_ESupplyNetworkTier tier)
	{
		switch (tier)
		{
			case AICF_ESupplyNetworkTier.HEALTHY: return m_iHealthyPacePercent;
			case AICF_ESupplyNetworkTier.STRAINED: return m_iStrainedPacePercent;
			case AICF_ESupplyNetworkTier.ISOLATED: return m_iIsolatedPacePercent;
		}

		return m_iBlockedPacePercent;
	}

	protected void ApplyCLIOverrides()
	{
		string value;
		if (System.GetCLIParam("aicfReplacementSupplyCost", value))
			m_iReplacementSupplyCost = ClampInt(value.ToInt(), 1, 1000000);
		if (System.GetCLIParam("aicfEconomyHealthyStockGroups", value))
			m_iHealthyStockGroups = ClampInt(value.ToInt(), 2, 100);
		if (System.GetCLIParam("aicfEconomyHealthyPacePercent", value))
			m_iHealthyPacePercent = ClampInt(value.ToInt(), 1, 400);
		if (System.GetCLIParam("aicfEconomyStrainedPacePercent", value))
			m_iStrainedPacePercent = ClampInt(value.ToInt(), 1, 100);
		if (System.GetCLIParam("aicfEconomyIsolatedPacePercent", value))
			m_iIsolatedPacePercent = ClampInt(value.ToInt(), 1, 100);
		if (System.GetCLIParam("aicfEconomyBlockedPacePercent", value))
			m_iBlockedPacePercent = ClampInt(value.ToInt(), 0, 100);
		if (System.GetCLIParam("aicfEconomyRetryMs", value))
			m_iRetryIntervalMs = ClampInt(value.ToInt(), 1000, 600000);
		if (System.GetCLIParam("aicfSupplyDeliveryIntervalMs", value))
			m_iDeliveryIntervalMs = ClampInt(value.ToInt(), 5000, 3600000);
		if (System.GetCLIParam("aicfSupplyDeliveryPackage", value))
			m_iDeliveryPackageSupplies = ClampInt(value.ToInt(), 1, 1000000);
		if (System.GetCLIParam("aicfSupplyDeliveryBaseTravelMs", value))
			m_iDeliveryBaseTravelMs = ClampInt(value.ToInt(), 1000, 3600000);
		if (System.GetCLIParam("aicfSupplyDeliveryPerHopMs", value))
			m_iDeliveryPerHopMs = ClampInt(value.ToInt(), 0, 3600000);
		if (System.GetCLIParam("aicfMaxSupplyShipmentsPerFaction", value))
			m_iMaxShipmentsPerFaction = ClampInt(value.ToInt(), 1, 16);
		if (System.GetCLIParam("aicfSupplySourceReserveGroups", value))
			m_iSourceReserveGroups = ClampInt(value.ToInt(), 0, 100);
		if (System.GetCLIParam("aicfEconomyHeartbeatMs", value))
			m_iHeartbeatIntervalMs = ClampInt(value.ToInt(), 5000, 600000);
	}

	protected int ClampInt(int value, int minimum, int maximum)
	{
		if (value < minimum)
			return minimum;
		if (value > maximum)
			return maximum;
		return value;
	}
}
