enum AICF_EDifficulty
{
	EASY,
	MEDIUM,
	HARD
}

modded class SCR_MissionHeaderCampaign
{
	[Attribute("0", UIWidgets.ComboBox, "FIA garrison difficulty", "", ParamEnumArray.FromEnum(AICF_EDifficulty))]
	AICF_EDifficulty m_eAICFDifficulty;

	// Legacy header fields сохранены для совместимости ресурсов; planner использует catalog.
	[Attribute("0", UIWidgets.CheckBox, "Legacy FIA tank flag (composition uses difficulty and catalog)")]
	bool m_bAICFFIATank;

	[Attribute("0", UIWidgets.CheckBox, "Legacy FIA passenger flag (composition uses difficulty and catalog)")]
	bool m_bAICFFIAPassengers;
}

class AICF_Difficulty
{
	static bool UsesTank()
	{
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		return header && header.m_bAICFFIATank;
	}

	static bool HasPassengers()
	{
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		return header && header.m_bAICFFIAPassengers;
	}

	static int GetInfantryRoleIndex(int memberIndex)
	{
		AICF_EDifficulty difficulty = Get();
		if ((difficulty == AICF_EDifficulty.MEDIUM || difficulty == AICF_EDifficulty.HARD) &&
			(memberIndex == 8 || memberIndex == 9)) return 3;
		return memberIndex;
	}

	static AICF_EDifficulty Get()
	{
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		if (!header) return AICF_EDifficulty.EASY;
		return header.m_eAICFDifficulty;
	}
}
