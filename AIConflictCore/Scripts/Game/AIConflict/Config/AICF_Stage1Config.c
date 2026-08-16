// Server-owned Stage 1 balance settings with conservative bounds for local Arland play.
class AICF_Stage1Config
{
	static const int GROUP_SLOTS_PER_FACTION = 10;
	static const int DEFAULT_GROUP_SIZE = 4;
	static const int DEFAULT_FULL_SIZE_GROUPS_PER_FACTION = 4;
	static const int MIN_GROUP_SIZE = 1;
	static const int MAX_GROUP_SIZE = 10;
	// Compatibility alias for diagnostics and older static contracts. Runtime
	// roster gates use each slot's commander-selected desired size.
	static const int MANAGED_GROUP_SIZE = DEFAULT_GROUP_SIZE;
	static const int ATTACK_SLOTS_PER_FACTION = 6;
	static const int DEFEND_SLOTS_PER_FACTION = 3;
	static const int RESERVE_SLOTS_PER_FACTION = 1;
	static const int LEGACY_ATTACK_SLOTS_PER_FACTION = 5;
	static const int LEGACY_DEFEND_SLOTS_PER_FACTION = 3;
	static const int LEGACY_RESERVE_SLOTS_PER_FACTION = 2;
	static const int ROLE_MINIMUM_DWELL_INTERVALS = 2;

	static const int DEFAULT_INITIAL_TICKETS = 12;
	static const int DEFAULT_REPLACEMENT_TICKET_COST = 1;
	static const int DEFAULT_REINFORCEMENT_DELAY_MS = 30000;
	static const int DEFAULT_COMMANDER_INTERVAL_MS = 15000;
	static const int DEFAULT_MAX_MANAGED_AGENTS = 220;
	static const int DEFAULT_WAR_TEMPO_PERCENT = 100;

	static const int MAX_TICKET_VALUE = 1000000;
	static const int MAX_DELAY_MS = 3600000;
	static const int MIN_COMMANDER_INTERVAL_MS = 1000;
	static const int MAX_COMMANDER_INTERVAL_MS = 600000;
	// Per faction the first four slots start at ten agents and the remaining six
	// at four: (4 * 10 + 6 * 4) * 2 factions = 128 live agents. Do not allow a
	// CLI override to make the configured initial roster permanently inadmissible.
	static const int MIN_MANAGED_AGENTS = 128;
	static const int MAX_MANAGED_AGENTS = 256;

	static int GetDefaultGroupSizeForSlot(int slotId)
	{
		if (slotId >= 0 && slotId < DEFAULT_FULL_SIZE_GROUPS_PER_FACTION)
			return MAX_GROUP_SIZE;
		return DEFAULT_GROUP_SIZE;
	}

	protected int m_iInitialTickets;
	protected int m_iReplacementTicketCost;
	protected int m_iReinforcementDelayMs;
	protected int m_iCommanderIntervalMs;
	protected int m_iMaxManagedAgents;
	protected int m_iWarTempoPercent;
	protected FactionKey m_sExpectedPlayerFaction;
	protected bool m_bRequirePlayerForResult;
	protected bool m_bActiveForcesRolesEnabled;

	void AICF_Stage1Config()
	{
		ResetToDefaults();
		ApplyCLIOverrides();
	}

	void ResetToDefaults()
	{
		m_iInitialTickets = DEFAULT_INITIAL_TICKETS;
		m_iReplacementTicketCost = DEFAULT_REPLACEMENT_TICKET_COST;
		m_iReinforcementDelayMs = DEFAULT_REINFORCEMENT_DELAY_MS;
		m_iCommanderIntervalMs = DEFAULT_COMMANDER_INTERVAL_MS;
		m_iMaxManagedAgents = DEFAULT_MAX_MANAGED_AGENTS;
		m_iWarTempoPercent = DEFAULT_WAR_TEMPO_PERCENT;
		m_sExpectedPlayerFaction = string.Empty;
		m_bRequirePlayerForResult = true;
		m_bActiveForcesRolesEnabled = true;
	}

	int GetInitialTickets()
	{
		return m_iInitialTickets;
	}

	void SetInitialTickets(int value)
	{
		m_iInitialTickets = ClampInt(value, 0, MAX_TICKET_VALUE);
	}

	int GetReplacementTicketCost()
	{
		return m_iReplacementTicketCost;
	}

	void SetReplacementTicketCost(int value)
	{
		m_iReplacementTicketCost = ClampInt(value, 1, MAX_TICKET_VALUE);
	}

	int GetReinforcementDelayMs()
	{
		return m_iReinforcementDelayMs;
	}

	void SetReinforcementDelayMs(int value)
	{
		m_iReinforcementDelayMs = ClampInt(value, 0, MAX_DELAY_MS);
	}

	int GetCommanderIntervalMs()
	{
		return m_iCommanderIntervalMs;
	}

	void SetCommanderIntervalMs(int value)
	{
		m_iCommanderIntervalMs = ClampInt(value, MIN_COMMANDER_INTERVAL_MS, MAX_COMMANDER_INTERVAL_MS);
	}

	int GetMaxManagedAgents()
	{
		return m_iMaxManagedAgents;
	}

	void SetMaxManagedAgents(int value)
	{
		m_iMaxManagedAgents = ClampInt(value, MIN_MANAGED_AGENTS, MAX_MANAGED_AGENTS);
	}

	int GetWarTempoPercent()
	{
		return m_iWarTempoPercent;
	}

	void SetWarTempoPercent(int value)
	{
		m_iWarTempoPercent = ClampInt(value, 25, 400);
		m_iReinforcementDelayMs = ClampInt(
			DEFAULT_REINFORCEMENT_DELAY_MS * 100 / m_iWarTempoPercent,
			0,
			MAX_DELAY_MS);
		m_iCommanderIntervalMs = ClampInt(
			DEFAULT_COMMANDER_INTERVAL_MS * 100 / m_iWarTempoPercent,
			MIN_COMMANDER_INTERVAL_MS,
			MAX_COMMANDER_INTERVAL_MS);
	}

	FactionKey GetExpectedPlayerFaction()
	{
		return m_sExpectedPlayerFaction;
	}

	void SetExpectedPlayerFaction(FactionKey factionKey)
	{
		m_sExpectedPlayerFaction = string.Empty;
		if (factionKey == "US" || factionKey == "USSR")
			m_sExpectedPlayerFaction = factionKey;
	}

	bool GetRequirePlayerForResult()
	{
		return m_bRequirePlayerForResult;
	}

	void SetRequirePlayerForResult(bool required)
	{
		m_bRequirePlayerForResult = required;
	}

	bool GetActiveForcesRolesEnabled()
	{
		return m_bActiveForcesRolesEnabled;
	}

	void SetActiveForcesRolesEnabled(bool enabled)
	{
		m_bActiveForcesRolesEnabled = enabled;
	}

	int GetRoleMinimumDwellMs()
	{
		return m_iCommanderIntervalMs * ROLE_MINIMUM_DWELL_INTERVALS;
	}

	protected void ApplyCLIOverrides()
	{
		string value;
		// Apply the coarse preset first. Explicit timing overrides below always win.
		if (System.GetCLIParam("aicfWarTempoPercent", value))
			SetWarTempoPercent(value.ToInt());
		if (System.GetCLIParam("aicfInitialTickets", value))
			SetInitialTickets(value.ToInt());
		if (System.GetCLIParam("aicfReplacementTicketCost", value))
			SetReplacementTicketCost(value.ToInt());
		if (System.GetCLIParam("aicfReinforcementDelayMs", value))
			SetReinforcementDelayMs(value.ToInt());
		if (System.GetCLIParam("aicfCommanderIntervalMs", value))
			SetCommanderIntervalMs(value.ToInt());
		if (System.GetCLIParam("aicfMaxManagedAgents", value))
			SetMaxManagedAgents(value.ToInt());
		if (System.GetCLIParam("aicfExpectedPlayerFaction", value))
			SetExpectedPlayerFaction(value);
		if (System.GetCLIParam("aicfRequirePlayerForResult", value))
			SetRequirePlayerForResult(value.ToInt() > 0);
		if (System.GetCLIParam("aicfActiveForcesRolesEnabled", value))
			SetActiveForcesRolesEnabled(value.ToInt() > 0);
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
