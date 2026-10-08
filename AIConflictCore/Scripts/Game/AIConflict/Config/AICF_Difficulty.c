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
}

class AICF_Difficulty
{
	static AICF_EDifficulty Get()
	{
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		if (!header) return AICF_EDifficulty.EASY;
		return header.m_eAICFDifficulty;
	}
}
