// Immutable match-scoped authority boundary for autonomous strategic selection.
class AICF_CommandAuthorityPolicy
{
	protected string m_sMode;
	protected bool m_bValid;

	void AICF_CommandAuthorityPolicy(AICF_Stage1Config config)
	{
		m_sMode = string.Empty;
		m_bValid = config && config.IsAICommanderModeValid();
		if (m_bValid)
			m_sMode = config.GetAICommanderMode();
	}

	string GetMode()
	{
		return m_sMode;
	}

	bool IsValid()
	{
		return m_bValid;
	}

	bool IsAICommanderEnabled(FactionKey factionKey)
	{
		if (!m_bValid)
			return false;
		factionKey = AICF_ContentProfile.GetActive().GetStableFactionKey(factionKey);
		if (factionKey == "US")
		{
			return m_sMode == AICF_Stage1Config.AI_COMMANDER_MODE_BOTH ||
				m_sMode == AICF_Stage1Config.AI_COMMANDER_MODE_US;
		}
		if (factionKey == "USSR")
		{
			return m_sMode == AICF_Stage1Config.AI_COMMANDER_MODE_BOTH ||
				m_sMode == AICF_Stage1Config.AI_COMMANDER_MODE_USSR;
		}

		return false;
	}

	AICF_EStrategicDecisionAuthority GetFactionAuthority(FactionKey factionKey)
	{
		factionKey = AICF_ContentProfile.GetActive().GetStableFactionKey(factionKey);
		if (!m_bValid || (factionKey != "US" && factionKey != "USSR"))
			return AICF_EStrategicDecisionAuthority.NONE;
		if (IsAICommanderEnabled(factionKey))
			return AICF_EStrategicDecisionAuthority.AI_COMMANDER;
		return AICF_EStrategicDecisionAuthority.PLAYER_COMMAND;
	}
}
