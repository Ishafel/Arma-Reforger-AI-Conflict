// Арифметика partition не читает world и не выдаёт spawn/activation permissions.
// Native hard/faction gates остаются отдельными обязательными ограничениями.
enum AICF_EAIQuotaBucket
{
	STOCK,
	COMBAT,
	BUILDERS,
	FLEET,
	FIA_EXTRA,
	STOCK_VEHICLES,
	COUNT
}

class AICF_AIQuotaMath
{
	static const int MAX_TOTAL_UNITS = 4096;

	static bool AddBounded(int current, int amount, int ceiling, out int result)
	{
		result = -1;
		if (current < 0 || amount < 0 || ceiling < 0 || current > ceiling)
			return false;
		if (amount > ceiling - current)
			return false;
		result = current + amount;
		return true;
	}

	static bool FitsBucket(int physical, int reserved, int limit, int requested)
	{
		if (physical < 0 || reserved < 0 || limit < 0 || requested <= 0 || physical > limit)
			return false;
		int remaining = limit - physical;
		return reserved <= remaining && requested <= remaining - reserved;
	}

	static bool TotalLimits(array<int> limits, out int total)
	{
		total = -1;
		if (!limits || limits.Count() != AICF_EAIQuotaBucket.COUNT)
			return false;
		int sum;
		foreach (int value : limits)
		{
			int next;
			if (!AddBounded(sum, value, MAX_TOTAL_UNITS, next))
				return false;
			sum = next;
		}
		if (limits[AICF_EAIQuotaBucket.STOCK] <= 0 || limits[AICF_EAIQuotaBucket.COMBAT] <= 0)
			return false;
		total = sum;
		return true;
	}

	static bool FitsPartition(array<int> physical, array<int> reserved, array<int> limits,
		AICF_EAIQuotaBucket bucket, int requested)
	{
		int total;
		if (!TotalLimits(limits, total) || !physical || !reserved ||
			physical.Count() != AICF_EAIQuotaBucket.COUNT || reserved.Count() != AICF_EAIQuotaBucket.COUNT ||
			bucket < AICF_EAIQuotaBucket.STOCK || bucket >= AICF_EAIQuotaBucket.COUNT || requested <= 0)
			return false;
		for (int index; index < AICF_EAIQuotaBucket.COUNT; index++)
		{
			int occupied;
			if (!AddBounded(physical[index], reserved[index], limits[index], occupied))
				return false;
		}
		return FitsBucket(physical[bucket], reserved[bucket], limits[bucket], requested);
	}
}
