// Immutable настройки одной quota session. World identity и original capture
// принадлежат отдельному world owner; этот value object не меняет native limit.
class AICF_AIQuotaLimits
{
	static const int MAX_MANAGED_UNITS = 256;
	static const int MAX_BUILDER_SLOTS = 512;
	static const int MAX_FLEET_UNITS = 20;
	protected bool m_bStockCaptured;
	protected bool m_bConfigured;
	protected int m_iOriginalStock = -1;
	protected int m_iInitialStock = -1;
	protected int m_iStockCeiling = -1;
	protected int m_iInitialStockVehicles = -1;
	protected int m_iStockVehicleCeiling = -1;
	protected int m_iTotal = -1;
	protected ref array<int> m_aLimits = {};

	bool CaptureStockSnapshot(int originalNativeLimit, int initialStockUnits, int initialStockVehicleUnits = 0)
	{
		if (originalNativeLimit <= 0 || originalNativeLimit > AICF_AIQuotaMath.MAX_TOTAL_UNITS ||
			initialStockUnits < 0 || initialStockUnits > AICF_AIQuotaMath.MAX_TOTAL_UNITS ||
			initialStockVehicleUnits < 0 || initialStockVehicleUnits > AICF_AIQuotaMath.MAX_TOTAL_UNITS)
			return false;
		if (m_bStockCaptured)
			return originalNativeLimit == m_iOriginalStock;
		m_iOriginalStock = originalNativeLimit;
		m_iInitialStock = initialStockUnits;
		m_iStockCeiling = Math.Max(originalNativeLimit, initialStockUnits);
		m_iInitialStockVehicles = initialStockVehicleUnits;
		m_iStockVehicleCeiling = Math.Max(originalNativeLimit, initialStockVehicleUnits);
		m_bStockCaptured = true;
		return true;
	}

	bool Configure(int managedUnits, int builderSlots, int fleetUnits, int fiaPlanUnits)
	{
		if (!m_bStockCaptured || m_bConfigured || managedUnits <= 0 || managedUnits > MAX_MANAGED_UNITS ||
			builderSlots < 0 || builderSlots > MAX_BUILDER_SLOTS || fleetUnits < 0 || fleetUnits > MAX_FLEET_UNITS ||
			fiaPlanUnits < 0)
			return false;
		array<int> candidate = {};
		candidate.Resize(AICF_EAIQuotaBucket.COUNT);
		candidate[AICF_EAIQuotaBucket.STOCK] = m_iStockCeiling;
		candidate[AICF_EAIQuotaBucket.COMBAT] = managedUnits;
		candidate[AICF_EAIQuotaBucket.BUILDERS] = builderSlots;
		candidate[AICF_EAIQuotaBucket.FLEET] = fleetUnits;
		candidate[AICF_EAIQuotaBucket.FIA_EXTRA] = fiaPlanUnits;
		candidate[AICF_EAIQuotaBucket.STOCK_VEHICLES] = m_iStockVehicleCeiling;
		int total;
		if (!AICF_AIQuotaMath.TotalLimits(candidate, total))
			return false;
		m_aLimits = candidate;
		m_iTotal = total;
		m_bConfigured = true;
		return true;
	}

	bool IsConfigured() { return m_bConfigured; }
	int GetOriginalStockLimit() { return m_iOriginalStock; }
	int GetInitialStockUnits() { return m_iInitialStock; }
	int GetStockCeiling() { return m_iStockCeiling; }
	int GetInitialStockVehicleUnits() { return m_iInitialStockVehicles; }
	int GetStockVehicleCeiling() { return m_iStockVehicleCeiling; }
	int GetTotalLimit() { return m_iTotal; }

	int GetLimit(AICF_EAIQuotaBucket bucket)
	{
		if (!m_bConfigured || bucket < AICF_EAIQuotaBucket.STOCK || bucket >= AICF_EAIQuotaBucket.COUNT)
			return -1;
		return m_aLimits[bucket];
	}
}
