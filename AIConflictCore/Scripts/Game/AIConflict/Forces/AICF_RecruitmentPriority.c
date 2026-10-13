// Порядок заявок не зависит от role-local имени и порядка service polling.
class AICF_RecruitmentPriority
{
	static int Rank(AICF_GroupSlot slot, bool playerRequested)
	{
		if (playerRequested)
			return 0;
		if (slot && slot.GetRole() == AICF_EGroupRole.ATTACK)
			return 1;
		return 2;
	}

	static string Source(bool playerRequested)
	{
		if (playerRequested)
			return "PLAYER_RECRUIT";
		return "AUTO";
	}

	static bool Precedes(AICF_InfantryRecruitmentOrder first, AICF_InfantryRecruitmentOrder second)
	{
		int firstRank = Rank(first.m_Slot, first.m_bPlayerRequested);
		int secondRank = Rank(second.m_Slot, second.m_bPlayerRequested);
		if (firstRank != secondRank)
			return firstRank < secondRank;
		return first.m_iQueueSequence < second.m_iQueueSequence;
	}
}
