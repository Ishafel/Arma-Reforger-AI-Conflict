// Read-only снимок global native budget. У ожидающих групп резервируется только
// ещё не материализованный roster; живые actors уже входят в native counters.
class AICF_SpawnAdmission
{
	protected int m_iActive = -1;
	protected int m_iIntended = -1;
	protected int m_iLimit = -1;
	protected int m_iPending = -1;
	protected int m_iRequested;
	protected bool m_bCanActivate;

	static bool FitsCounts(int active, int intended, int limit, int pending, int requested)
	{
		if (active < 0 || intended < 0 || limit <= 0 || pending < 0 || requested <= 0) return false;
		int occupied = Math.Max(active, intended);
		if (occupied > limit) return false;
		int free = limit - occupied;
		return pending <= free && requested <= free - pending;
	}

	static int ProjectedCounts(int active, int intended, int pending, int requested)
	{
		if (active < 0 || intended < 0 || pending < 0 || requested < 0) return -1;
		int occupied = Math.Max(active, intended);
		if (pending > int.MAX - occupied) return int.MAX;
		int reserved = occupied + pending;
		if (requested > int.MAX - reserved) return int.MAX;
		return reserved + requested;
	}

	protected static int MissingFaction(AICF_FactionState state, AICF_GroupSlot requestedSlot)
	{
		if (!state) return 0;
		int missing = 0;
		for (int index = 0; index < state.GetSlotCount(); index++)
		{
			AICF_GroupSlot slot = state.GetSlot(index);
			if (!slot || slot == requestedSlot || slot.GetState() != AICF_EGroupSlotState.SPAWNING) continue;
			int expected = slot.GetRosterExpectedCount();
			if (expected < 1) expected = slot.GetDeploymentSize();
			if (expected < 1 || expected > AICF_Stage1Config.MAX_GROUP_SIZE) return -1;
			int actual = 0;
			SCR_AIGroup group = slot.GetGroup();
			if (group) actual = group.GetAgentsCount();
			if (actual < 0) return -1;
			missing += Math.Max(0, expected - actual);
		}
		return missing;
	}

	static AICF_SpawnAdmission Inspect(AIWorld world, AICF_FactionState us, AICF_FactionState ussr,
		AICF_GroupSlot requestedSlot, int unmaterializedRecruits)
	{
		AICF_SpawnAdmission result = new AICF_SpawnAdmission();
		if (requestedSlot) result.m_iRequested = requestedSlot.GetDeploymentSize();
		if (!world || !requestedSlot || unmaterializedRecruits < 0) return result;
		result.m_iActive = world.GetCurrentNumOfActiveAIs();
		result.m_iIntended = world.GetActiveGroupIntendedCount();
		result.m_iLimit = world.GetLimitOfActiveAIs();
		result.m_bCanActivate = world.CanAIBeActivated();
		int usPending = MissingFaction(us, requestedSlot);
		int ussrPending = MissingFaction(ussr, requestedSlot);
		if (usPending >= 0 && ussrPending >= 0)
			result.m_iPending = usPending + ussrPending + unmaterializedRecruits;
		return result;
	}

	bool CanAdmit()
	{
		return m_bCanActivate && FitsCounts(m_iActive, m_iIntended, m_iLimit, m_iPending, m_iRequested);
	}

	static AICF_SpawnAdmission InspectRecruit(AIWorld world, AICF_FactionState us, AICF_FactionState ussr, int unmaterializedRecruits)
	{
		AICF_SpawnAdmission result = new AICF_SpawnAdmission();
		result.m_iRequested = 1;
		if (!world || !us || !ussr || unmaterializedRecruits < 0) return result;
		result.m_iActive = world.GetCurrentNumOfActiveAIs();
		result.m_iIntended = world.GetActiveGroupIntendedCount();
		result.m_iLimit = world.GetLimitOfActiveAIs();
		result.m_bCanActivate = world.CanAIBeActivated();
		// Donor ещё не создан; его own reservation отсутствует. Живые donor actors
		// уже учтены native counters, добавляются только отсутствующие materialisations.
		int usPending = MissingFaction(us, null);
		int ussrPending = MissingFaction(ussr, null);
		if (usPending >= 0 && ussrPending >= 0)
			result.m_iPending = usPending + ussrPending + unmaterializedRecruits;
		return result;
	}

	string Details()
	{
		return string.Format("active=%1 intended=%2 hard_limit=%3 pending=%4 requested=%5 projected=%6 global_can_activate=%7 reservation_policy=CONSERVATIVE_MISSING_MEMBERS",
			m_iActive, m_iIntended, m_iLimit, m_iPending, m_iRequested,
			ProjectedCounts(m_iActive, m_iIntended, m_iPending, m_iRequested), m_bCanActivate);
	}
}
