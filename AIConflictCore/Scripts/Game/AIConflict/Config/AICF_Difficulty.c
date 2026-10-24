enum AICF_EDifficulty
{
	EASY,
	MEDIUM,
	HARD
}

modded class SCR_MissionHeaderCampaign
{
	AICF_EDifficulty m_eAICFDifficulty;

}

class AICF_Difficulty
{
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
