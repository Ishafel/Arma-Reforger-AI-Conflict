// Server-owned Stage 1 balance settings with conservative bounds for local Arland play.
class AICF_Stage1Config
{
	static const int GROUP_SLOTS_PER_FACTION = 4;
	static const int ATTACK_SLOTS_PER_FACTION = 2;
	static const int DEFEND_SLOTS_PER_FACTION = 1;
	static const int RESERVE_SLOTS_PER_FACTION = 1;

	static const int DEFAULT_INITIAL_TICKETS = 12;
	static const int DEFAULT_REPLACEMENT_TICKET_COST = 1;
	static const int DEFAULT_REINFORCEMENT_DELAY_MS = 30000;
	static const int DEFAULT_COMMANDER_INTERVAL_MS = 15000;
	static const int DEFAULT_MAX_MANAGED_AGENTS = 64;

	static const int MAX_TICKET_VALUE = 1000000;
	static const int MAX_DELAY_MS = 3600000;
	static const int MIN_COMMANDER_INTERVAL_MS = 1000;
	static const int MAX_COMMANDER_INTERVAL_MS = 600000;
	// Seven live stock squads plus one conservative eight-agent replacement projection need 29;
	// keep a small explicit margin so the first valid replacement cannot deadlock at the clamp.
	static const int MIN_MANAGED_AGENTS = 32;
	static const int MAX_MANAGED_AGENTS = 128;

	protected int m_iInitialTickets;
	protected int m_iReplacementTicketCost;
	protected int m_iReinforcementDelayMs;
	protected int m_iCommanderIntervalMs;
	protected int m_iMaxManagedAgents;
	protected FactionKey m_sExpectedPlayerFaction;

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
		m_sExpectedPlayerFaction = string.Empty;
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

	protected void ApplyCLIOverrides()
	{
		string value;
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
