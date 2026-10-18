enum AICF_EMatchMode
{
	STANDARD,
	COOP_US,
	COOP_USSR
}

modded class SCR_MissionHeaderCampaign
{
	[Attribute("0", UIWidgets.ComboBox, "AI Conflict match mode", "", ParamEnumArray.FromEnum(AICF_EMatchMode))]
	AICF_EMatchMode m_eAICFMatchMode;
}

// Immutable scenario policy; faction state, economy and helpers keep their owners.
class AICF_MatchRules
{
	protected AICF_EMatchMode m_eMode;

	void AICF_MatchRules(AICF_EMatchMode mode = AICF_EMatchMode.STANDARD)
	{
		m_eMode = mode;
	}

	AICF_EMatchMode GetMode()
	{
		return m_eMode;
	}

	bool IsValid()
	{
		return m_eMode >= AICF_EMatchMode.STANDARD && m_eMode <= AICF_EMatchMode.COOP_USSR;
	}

	string GetPlayerOnlyFaction()
	{
		if (m_eMode == AICF_EMatchMode.COOP_US) return "US";
		if (m_eMode == AICF_EMatchMode.COOP_USSR) return "USSR";
		return string.Empty;
	}

	string GetAICommanderMode()
	{
		if (m_eMode == AICF_EMatchMode.COOP_US) return "USSR";
		if (m_eMode == AICF_EMatchMode.COOP_USSR) return "US";
		return "BOTH";
	}

	int GetManagedSlotCount(FactionKey factionKey)
	{
		factionKey = AICF_ContentProfile.GetActive().GetStableFactionKey(factionKey);
		if (!IsValid() || (factionKey != "US" && factionKey != "USSR")) return 0;
		if (factionKey == GetPlayerOnlyFaction()) return 0;
		return AICF_Stage1Config.GROUP_SLOTS_PER_FACTION;
	}

	bool UsesTicketDefeat(FactionKey factionKey)
	{
		return GetManagedSlotCount(factionKey) > 0;
	}
}
