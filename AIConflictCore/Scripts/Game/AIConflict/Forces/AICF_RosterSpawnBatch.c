// One synchronous dispatch boundary; READY and economy remain with the owner poll.
class AICF_RosterSpawnBatch
{
	protected AICF_MatchController m_Controller;
	protected AICF_FactionState m_State;
	protected AICF_GroupSlot m_Slot;
	protected SCR_AIGroup m_Group;
	protected EntityID m_Id;
	protected string m_sRpl;
	protected Faction m_Faction;
	protected int m_iGeneration;
	protected int m_iStartedAt;
	protected int m_iExpected;
	protected ref array<ResourceName> m_aPrefabs = {};

	void AICF_RosterSpawnBatch(AICF_MatchController controller, AICF_FactionState state,
		AICF_GroupSlot slot, SCR_AIGroup group, int expected)
	{
		m_Controller = controller;
		m_State = state;
		m_Slot = slot;
		m_Group = group;
		m_iExpected = expected;
		if (!slot || !group) return;
		m_iGeneration = slot.GetSpawnGeneration();
		m_iStartedAt = slot.GetSpawnStartedAtMs();
		m_Id = group.GetID();
		m_sRpl = AICF_VehicleBehaviorBinding.ComponentRpl(group);
		m_Faction = group.GetFaction();
		if (group.m_aUnitPrefabSlots) m_aPrefabs.Copy(group.m_aUnitPrefabSlots);
	}

	bool IsCurrent(SCR_AIGroup group, int expected)
	{
		if (!Replication.IsServer() || !m_Controller || !m_Group || group != m_Group ||
			expected != m_iExpected || expected < 1 || expected > 10)
			return false;
		if (!m_Controller.IsCurrentRosterSpawnOwner(m_State, m_Slot, group, m_iGeneration) ||
			m_Slot.GetSpawnStartedAtMs() != m_iStartedAt || !m_Slot.IsRosterSpawnRequested() ||
			m_Slot.GetRosterExpectedCount() != expected || m_Slot.GetDeploymentSize() != expected)
			return false;
		if (group.GetID() != m_Id || !m_Faction || group.GetFaction() != m_Faction ||
			AICF_VehicleBehaviorBinding.ComponentRpl(group) != m_sRpl ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group))
			return false;
		if (!group.m_aUnitPrefabSlots || group.m_aUnitPrefabSlots.Count() != expected ||
			m_aPrefabs.Count() != expected)
			return false;
		for (int index; index < expected; index++)
		{
			if (m_aPrefabs[index].IsEmpty() || group.m_aUnitPrefabSlots[index] != m_aPrefabs[index]) return false;
		}
		return true;
	}

	bool IsForGroup(SCR_AIGroup group)
	{
		return group == m_Group;
	}

	static bool FitsRemaining(int active, int intended, int hardLimit, int factionCurrent,
		int factionLimit, int remaining)
	{
		if (active < 0 || intended < 0 || hardLimit <= 0 || remaining <= 0 ||
			hardLimit - Math.Max(active, intended) < remaining || factionLimit < -1)
			return false;
		if (factionLimit >= 0 && (factionCurrent < 0 || factionLimit - factionCurrent < remaining))
			return false;
		return true;
	}

	protected bool CanDispatch(ChimeraAIWorld world, int remaining)
	{
		if (!world || !IsCurrent(m_Group, m_iExpected) || !world.CanAIBeActivated() ||
			!world.CanActivateGroup(m_Group) || !world.CanLimitedAIBeAddedForFaction(m_Faction.GetFactionKey()))
			return false;
		return FitsRemaining(world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount(),
			world.GetLimitOfActiveAIs(), world.GetCurrentAmountOfLimitedAIsForFaction(m_Faction.GetFactionKey()),
			world.GetAILimitForFaction(m_Faction.GetFactionKey()), remaining);
	}

	// False means this optimization did not take ownership: retain stock queue.
	bool TryDispatch(SCR_AIGroup group, int expected)
	{
		ChimeraAIWorld world = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!IsCurrent(group, expected) || group.GetAgentsCount() != 0 ||
			!group.AICF_HasEmptyRosterAttachments() || !CanDispatch(world, expected))
			return false;
		int startedAt = System.GetTickCount();
		int activeBefore = world.GetCurrentNumOfActiveAIs();
		int intendedBefore = world.GetActiveGroupIntendedCount();
		int limitBefore = world.GetLimitOfActiveAIs();
		int factionBefore = world.GetCurrentAmountOfLimitedAIsForFaction(m_Faction.GetFactionKey());
		int factionLimit = world.GetAILimitForFaction(m_Faction.GetFactionKey());
		world.PurgeSpawnRequestsForGroup(group);
		if (!IsCurrent(group, expected)) return true;
		group.AICF_RecordRosterBatchOwner(false);
		group.SetNumberOfMembersToSpawn(expected);
		int created;
		for (int index; index < expected; index++)
		{
			int before = group.GetAgentsCount();
			if (!IsCurrent(group, expected) || !group.AICF_HasEmptyRosterAttachments() ||
				!CanDispatch(world, expected - before)) break;
			bool expanded = group.ExpandOneMember();
			// A callback may replace/stop the owner. Never mutate its successor.
			if (!IsCurrent(group, expected) || !group.AICF_HasEmptyRosterAttachments()) return true;
			if (!expanded || group.GetAgentsCount() != before + 1) break;
			created++;
		}
		if (!IsCurrent(group, expected) || !group.AICF_HasEmptyRosterAttachments()) return true;
		int remaining = expected - group.GetAgentsCount();
		group.AICF_RecordRosterBatchOwner(remaining > 0);
		if (remaining > 0 && IsCurrent(group, expected)) group.RequestSpawn(remaining);
		string details = string.Format("group=%1 group_rpl=%2 generation=%3 expected=%4 created=%5 remaining=%6 queue_fallback=%7 purges=1 elapsed_ms=%8",
			m_Id, m_sRpl, m_iGeneration, expected, created, remaining, remaining > 0, System.GetTickCount(startedAt));
		details += string.Format(" active_before=%1 intended_before=%2 hard_before=%3 faction_before=%4 faction_limit=%5 active_after=%6 intended_after=%7 hard_after=%8 faction_after=%9",
			activeBefore, intendedBefore, limitBefore, factionBefore, factionLimit, world.GetCurrentNumOfActiveAIs(),
			world.GetActiveGroupIntendedCount(), world.GetLimitOfActiveAIs(), world.GetCurrentAmountOfLimitedAIsForFaction(m_Faction.GetFactionKey()));
		AICF_Stage1Diagnostics.Info("ROSTER_NATIVE_BATCH_DISPATCHED",
			details);
		return true;
	}
}
