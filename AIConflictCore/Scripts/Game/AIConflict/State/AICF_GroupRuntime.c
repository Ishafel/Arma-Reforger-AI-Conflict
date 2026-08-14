// Shared runtime queries for managed groups. SCR_AIGroup is a controller entity;
// its own origin is not guaranteed to follow the soldiers after deployment.
class AICF_GroupRuntime
{
	static IEntity ResolveAliveLeader(SCR_AIGroup group)
	{
		if (!group)
			return null;

		IEntity leader = group.GetLeaderEntity();
		if (IsAliveCharacter(leader))
			return leader;

		// Leader promotion is asynchronous. Use a surviving member until the group
		// appoints its next leader, then callers automatically switch on the next tick.
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity controlledEntity = agent.GetControlledEntity();
			if (IsAliveCharacter(controlledEntity))
				return controlledEntity;
		}

		return null;
	}

	static int CountAliveAgents(SCR_AIGroup group)
	{
		if (!group)
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int alive;
		foreach (AIAgent agent : agents)
		{
			if (agent && IsAliveCharacter(agent.GetControlledEntity()))
				alive++;
		}

		return alive;
	}

	static bool HasExactFactionRoster(
		SCR_AIGroup group,
		FactionKey expectedFactionKey,
		int expectedCount,
		out int actualCount,
		out int factionMismatchCount,
		out int nonAliveCount)
	{
		actualCount = 0;
		factionMismatchCount = 0;
		nonAliveCount = 0;
		if (!group || expectedFactionKey.IsEmpty() || expectedCount <= 0)
			return false;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		actualCount = agents.Count();
		foreach (AIAgent agent : agents)
		{
			IEntity controlledEntity;
			if (agent)
				controlledEntity = agent.GetControlledEntity();
			if (!controlledEntity)
			{
				factionMismatchCount++;
				nonAliveCount++;
				continue;
			}
			if (!IsAliveCharacter(controlledEntity))
				nonAliveCount++;

			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(
				controlledEntity.FindComponent(FactionAffiliationComponent));
			Faction affiliatedFaction;
			if (affiliation)
				affiliatedFaction = affiliation.GetAffiliatedFaction();
			if (!affiliatedFaction || affiliatedFaction.GetFactionKey() != expectedFactionKey)
				factionMismatchCount++;
		}

		return actualCount == expectedCount && factionMismatchCount == 0 && nonAliveCount == 0;
	}

	// Builds a pre-cleanup, self-contained spawn snapshot. In Reforger 1.8 the
	// per-group delayed queue was removed and GetSpawnQueueSize() is a compatibility
	// stub that always returns zero, so spawning_pending is deliberately owned by
	// AICF and means expected minus materialised agents.
	static string BuildSpawnSnapshot(
		SCR_CampaignFaction expectedFaction,
		AICF_GroupSlot slot,
		int expectedCount)
	{
		if (!expectedFaction || !slot)
			return "snapshot_invalid=1 incomplete_reason=GROUP_SLOT_CONTEXT_MISSING";

		FactionKey expectedFactionKey = expectedFaction.GetFactionKey();
		SCR_AIGroup group = slot.GetGroup();
		string deployment = "INITIAL";
		if (slot.IsReplacementDeployment())
			deployment = "REPLACEMENT";

		int spawnAgeMs;
		if (slot.GetSpawnStartedAtMs() > 0)
			spawnAgeMs = System.GetTickCount(slot.GetSpawnStartedAtMs());

		string groupEntityId = FormatEntityId(group);
		int groupExists;
		string groupFactionKey = "NONE";
		int groupFactionCorrect;
		int configuredSlots;
		int membersToSpawn;
		string groupRplId;
		int groupRplMaster;
		int groupRplProxy;
		int groupReplicationReady;
		if (group)
		{
			if (IsEntityPresent(group))
				groupExists = 1;
			Faction groupFaction = group.GetFaction();
			if (groupFaction)
			{
				groupFactionKey = groupFaction.GetFactionKey();
				if (groupFactionKey == expectedFactionKey)
					groupFactionCorrect = 1;
			}
			if (group.m_aUnitPrefabSlots)
				configuredSlots = group.m_aUnitPrefabSlots.Count();
			membersToSpawn = group.GetNumberOfMembersToSpawn();
			ResolveReplication(
				group,
				groupRplId,
				groupRplMaster,
				groupRplProxy,
				groupReplicationReady);
		}

		array<AIAgent> agents = {};
		if (group)
			group.GetAgents(agents);
		int actualCount = agents.Count();
		int pendingCount = Math.Max(expectedCount - actualCount, 0);
		int aliveCount;
		int factionCorrectCount;
		int factionMismatchCount;
		int nonAliveCount;
		int uncontrolledCount;
		int memberEntityMissingCount;
		int memberReplicationNotReadyCount;
		string roster = "[";

		for (int memberIndex = 0; memberIndex < actualCount; memberIndex++)
		{
			AIAgent agent = agents[memberIndex];
			slot.ObserveRosterAgent(agent);
			IEntity memberEntity;
			if (agent)
				memberEntity = agent.GetControlledEntity();

			int memberEntityExists;
			if (memberEntity && IsEntityPresent(memberEntity))
				memberEntityExists = 1;
			else if (memberEntity)
				memberEntityMissingCount++;

			int memberAlive;
			string lifeState = DescribeLifeState(memberEntity);
			if (IsAliveCharacter(memberEntity))
			{
				memberAlive = 1;
				aliveCount++;
			}
			else
				nonAliveCount++;

			string memberFactionKey = "NONE";
			int memberFactionCorrect;
			if (memberEntity)
			{
				FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(
					memberEntity.FindComponent(FactionAffiliationComponent));
				Faction memberFaction;
				if (affiliation)
					memberFaction = affiliation.GetAffiliatedFaction();
				if (memberFaction)
				{
					memberFactionKey = memberFaction.GetFactionKey();
					if (memberFactionKey == expectedFactionKey)
						memberFactionCorrect = 1;
				}
			}
			if (memberFactionCorrect)
				factionCorrectCount++;
			else
				factionMismatchCount++;
			if (!memberEntity)
				uncontrolledCount++;

			string memberRplId;
			int memberRplMaster;
			int memberRplProxy;
			int memberReplicationReady;
			ResolveReplication(
				memberEntity,
				memberRplId,
				memberRplMaster,
				memberRplProxy,
				memberReplicationReady);
			if (!memberReplicationReady)
				memberReplicationNotReadyCount++;

			string ageSource = "POLL_FALLBACK";
			if (slot.WasRosterAgentObservedFromCallback(agent))
				ageSource = "AGENT_ADDED_CALLBACK";
			string memberReason = ResolveMemberSpawnReason(
				memberEntity,
				memberEntityExists,
				memberAlive,
				memberFactionCorrect,
				memberReplicationReady);

			if (memberIndex > 0)
				roster += "|";
			roster += string.Format(
				"{index=%1 agent=%2 entity=%3 observed_age_ms=%4 age_source=%5",
				memberIndex,
				FormatEntityId(agent),
				FormatEntityId(memberEntity),
				slot.GetRosterAgentObservedAgeMs(agent),
				ageSource);
			roster += string.Format(
				" alive=%1 life_state=%2 faction=%3 faction_correct=%4 entity_exists=%5",
				memberAlive,
				lifeState,
				memberFactionKey,
				memberFactionCorrect,
				memberEntityExists);
			roster += string.Format(
				" rpl_id=%1 rpl_master=%2 rpl_proxy=%3 replication_ready=%4 reason=%5}",
				memberRplId,
				memberRplMaster,
				memberRplProxy,
				memberReplicationReady,
				memberReason);
		}
		roster += "]";

		int aiFactionCurrent = -1;
		int aiFactionLimit = -1;
		int aiFactionCanAdd = -1;
		int aiLastTickEvictions = -1;
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
		{
			aiFactionCurrent = aiWorld.GetCurrentAmountOfLimitedAIsForFaction(expectedFactionKey);
			aiFactionLimit = aiWorld.GetAILimitForFaction(expectedFactionKey);
			aiFactionCanAdd = 0;
			if (aiWorld.CanLimitedAIBeAddedForFaction(expectedFactionKey))
				aiFactionCanAdd = 1;
			aiLastTickEvictions = aiWorld.GetLastTickEvictions();
		}

		int requested;
		if (slot.IsRosterSpawnRequested())
			requested = 1;
		int requestedCount = slot.GetRosterExpectedCount();
		int completionCallback;
		if (slot.WasRosterCompletionCallbackObserved())
			completionCallback = 1;
		int factionCorrectAll;
		if (actualCount == expectedCount && factionMismatchCount == 0)
			factionCorrectAll = 1;

		string details = string.Format(
			"faction=%1 slot=%2 slot_key=%3 generation=%4 deployment=%5",
			expectedFactionKey,
			slot.GetSlotId(),
			slot.GetSlotKey(),
			slot.GetSpawnGeneration(),
			deployment);
		details += string.Format(
			" group_entity_id=%1 group_exists=%2 group_faction=%3 group_faction_correct=%4",
			groupEntityId,
			groupExists,
			groupFactionKey,
			groupFactionCorrect);
		details += string.Format(
			" group_rpl_id=%1 group_rpl_master=%2 group_rpl_proxy=%3 group_replication_ready=%4",
			groupRplId,
			groupRplMaster,
			groupRplProxy,
			groupReplicationReady);
		details += string.Format(
			" expected=%1 configured_slots=%2 members_to_spawn=%3 actual=%4 alive=%5",
			expectedCount,
			configuredSlots,
			membersToSpawn,
			actualCount,
			aliveCount);
		details += string.Format(
			" faction_correct=%1 faction_correct_all=%2 faction_mismatches=%3 non_alive=%4 uncontrolled=%5",
			factionCorrectCount,
			factionCorrectAll,
			factionMismatchCount,
			nonAliveCount,
			uncontrolledCount);
		details += string.Format(
			" member_entity_missing=%1 member_replication_not_ready=%2 spawning_pending=%3 pending_source=AICF_EXPECTED_MINUS_ACTUAL engine_queue_observable=0",
			memberEntityMissingCount,
			memberReplicationNotReadyCount,
			pendingCount);
		details += string.Format(
			" spawn_request_issued=%1 requested=%2 spawn_owner=SCR_AIWORLD_REQUEST request_age_ms=%3 spawn_age_ms=%4 progress_age_ms=%5",
			requested,
			requestedCount,
			slot.GetRosterSpawnRequestAgeMs(),
			spawnAgeMs,
			slot.GetRosterProgressAgeMs());
		details += string.Format(
			" callback_agents=%1 completion_callback=%2 ai_faction_current=%3 ai_faction_limit=%4 ai_faction_can_add=%5",
			slot.GetRosterCallbackAgentCount(),
			completionCallback,
			aiFactionCurrent,
			aiFactionLimit,
			aiFactionCanAdd);
		details += string.Format(
			" ai_last_tick_evictions=%1 incomplete_reason=%2 members=%3",
			aiLastTickEvictions,
			ResolveSpawnIncompleteReason(expectedFaction, slot, expectedCount),
			roster);
		return details;
	}

	static string ResolveSpawnIncompleteReason(
		SCR_CampaignFaction expectedFaction,
		AICF_GroupSlot slot,
		int expectedCount)
	{
		if (!expectedFaction || !slot)
			return "GROUP_SLOT_CONTEXT_MISSING";

		SCR_AIGroup group = slot.GetGroup();
		if (!group)
			return "GROUP_NOT_BOUND";
		if (!IsEntityPresent(group))
			return "GROUP_ENTITY_MISSING";

		Faction groupFaction = group.GetFaction();
		if (!groupFaction || groupFaction.GetFactionKey() != expectedFaction.GetFactionKey())
			return "GROUP_FACTION_MISMATCH";
		if (!slot.IsRosterSpawnRequested())
			return "SPAWN_REQUEST_NOT_ISSUED";

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int actualCount = agents.Count();
		if (actualCount < expectedCount)
		{
			ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
			if (aiWorld && !aiWorld.CanLimitedAIBeAddedForFaction(expectedFaction.GetFactionKey()))
				return "AI_FACTION_LIMIT_BLOCKED";
			if (slot.WasRosterCompletionCallbackObserved())
				return "SPAWN_QUEUE_COMPLETED_SHORT";
			if (actualCount == 0)
				return "NO_AGENTS_MATERIALIZED";
			return "PARTIAL_ROSTER";
		}
		if (actualCount > expectedCount)
			return "OVERSIZED_ROSTER";

		foreach (AIAgent agent : agents)
		{
			IEntity memberEntity;
			if (agent)
				memberEntity = agent.GetControlledEntity();
			if (!memberEntity)
				return "MEMBER_CONTROLLED_ENTITY_MISSING";
			if (!IsEntityPresent(memberEntity))
				return "MEMBER_ENTITY_MISSING";
			if (!IsAliveCharacter(memberEntity))
				return "NON_ALIVE_MEMBER";

			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(
				memberEntity.FindComponent(FactionAffiliationComponent));
			Faction memberFaction;
			if (affiliation)
				memberFaction = affiliation.GetAffiliatedFaction();
			if (!memberFaction || memberFaction.GetFactionKey() != expectedFaction.GetFactionKey())
				return "MEMBER_FACTION_MISMATCH";
		}

		return "ROSTER_COMPLETE_PENDING_READY_GATE";
	}

	static string FormatEntityId(IEntity entity)
	{
		if (!entity)
			return "NONE";

		string value = entity.GetID().ToString();
		int suffixStart = value.IndexOf(" ");
		if (suffixStart > 0)
			return value.Substring(0, suffixStart);
		return value;
	}

	protected static bool IsEntityPresent(IEntity entity)
	{
		return entity && GetGame() && GetGame().GetWorld() &&
			GetGame().GetWorld().FindEntityByID(entity.GetID()) == entity;
	}

	protected static void ResolveReplication(
		IEntity entity,
		out string rplId,
		out int isMaster,
		out int isProxy,
		out int replicationReady)
	{
		rplId = "NONE";
		isMaster = 0;
		isProxy = 0;
		replicationReady = 0;
		if (!entity)
			return;

		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl)
			return;
		if (rpl.IsMaster())
			isMaster = 1;
		if (rpl.IsProxy())
			isProxy = 1;

		RplId identity = rpl.Id();
		if (!identity || !identity.IsValid())
			return;
		rplId = identity.AsString();
		if (Replication.FindItem(identity) == rpl)
			replicationReady = 1;
	}

	protected static string DescribeLifeState(IEntity entity)
	{
		if (!entity)
			return "NO_CONTROLLED_ENTITY";
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return "NOT_CHARACTER";
		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return "NO_CHARACTER_CONTROLLER";
		if (controller.GetLifeState() == ECharacterLifeState.ALIVE)
			return "ALIVE";
		if (controller.GetLifeState() == ECharacterLifeState.INCAPACITATED)
			return "INCAPACITATED";
		if (controller.GetLifeState() == ECharacterLifeState.DEAD)
			return "DEAD";
		return "UNKNOWN";
	}

	protected static string ResolveMemberSpawnReason(
		IEntity entity,
		int entityExists,
		int alive,
		int factionCorrect,
		int replicationReady)
	{
		if (!entity)
			return "CONTROLLED_ENTITY_MISSING";
		if (!entityExists)
			return "ENTITY_MISSING";
		if (!alive)
			return "NON_ALIVE";
		if (!factionCorrect)
			return "FACTION_MISMATCH";
		if (!replicationReady)
			return "REPLICATION_NOT_READY";
		return "OK";
	}

	static bool IsAliveCharacter(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.GetLifeState() == ECharacterLifeState.ALIVE;
	}
}
