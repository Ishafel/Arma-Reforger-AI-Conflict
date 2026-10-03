// Локальный выход при сорванном подходе. Общие hidden-recovery fences и
// generation budget совпадают с isolated-navmesh recovery. Приказами не владеет.
class AICF_InfantryRecruitmentRecovery
{
	static const float MAX_SHIFT_METERS = 8;

	static bool TryRecover(AICF_InfantryRecruitmentOrder order, AICF_VehicleWatchdog watchdog,
		AICF_Stage3Config config)
	{
		if (!Replication.IsServer() || !order || !watchdog || !config || !config.GetHiddenRecoveryEnabled() ||
			!order.IsCurrent(order.m_Slot) || order.m_Slot.HasUsedIsolatedNavmeshRecovery() ||
			order.m_Donor || order.m_iArrivedAtMs > 0 || !order.HasSafeBarracks())
			return false;
		SCR_AIGroup group = order.m_Group;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group))
			return false;
		float threat;
		if (!watchdog.IsHiddenRecoveryCombatSafe(group, threat))
			return Blocked(order, "COMBAT_OR_UNKNOWN");
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.IsEmpty() || agents.Count() != AICF_GroupRuntime.CountAliveAgents(group))
			return Blocked(order, "ROSTER_NOT_FULLY_ALIVE");
		array<ChimeraCharacter> characters = {};
		array<EntityID> identities = {};
		array<vector> origins = {};
		array<vector> destinations = {};
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				return Blocked(order, "MISSING_AGENT");
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!IsMemberCurrent(agent, character, group, order.m_Faction))
				return Blocked(order, "MEMBER_AUTHORITY_OR_VEHICLE");
			vector origin = character.GetOrigin();
			vector destination;
			if (!FindDestination(group, origin, order.m_vPosition, destinations, destination))
				return Blocked(order, "NO_SAFE_LOCAL_POSITION");
			float nearest;
			string reason;
			if (!watchdog.CanApplyHiddenRecovery(origin, destination, config.GetHiddenRecoveryPlayerRadiusMeters(), nearest, reason))
				return Blocked(order, reason);
			characters.Insert(character);
			identities.Insert(character.GetID());
			origins.Insert(origin);
			destinations.Insert(destination);
		}
		int relocated;
		for (int index; index < characters.Count(); index++)
		{
			// Полная повторная проверка перед каждым Teleport, включая смену
			// roster/possession, источник/назначение и фактический размер шага.
			array<AIAgent> currentAgents = {};
			group.GetAgents(currentAgents);
			bool sameRoster = currentAgents.Count() == agents.Count();
			foreach (AIAgent expected : agents)
			{
				if (!currentAgents.Contains(expected))
					sameRoster = false;
			}
			ChimeraCharacter member = characters[index];
			vector checkedPosition;
			float nearest;
			string reason;
			if (!order.IsCurrent(order.m_Slot))
				reason = "COMMIT_ORDER_CHANGED";
			else if (!order.HasSafeBarracks())
				reason = "COMMIT_BARRACKS_UNSAFE";
			else if (!sameRoster)
				reason = "COMMIT_ROSTER_CHANGED";
			else if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group))
				reason = "COMMIT_GROUP_AUTHORITY_CHANGED";
			else if (!IsMemberCurrent(agents[index], member, group, order.m_Faction) || member.GetID() != identities[index])
				reason = "COMMIT_MEMBER_IDENTITY_OR_AUTHORITY_CHANGED";
			else if (member.GetOrigin() != origins[index] || vector.DistanceXZ(member.GetOrigin(), destinations[index]) > MAX_SHIFT_METERS)
				reason = "COMMIT_SOURCE_MOVED";
			else if (!watchdog.IsHiddenRecoveryCombatSafe(group, threat))
				reason = "COMMIT_COMBAT_OR_UNKNOWN";
			else if (!watchdog.CanApplyHiddenRecovery(member.GetOrigin(), destinations[index], config.GetHiddenRecoveryPlayerRadiusMeters(), nearest, reason))
				reason = "COMMIT_" + reason;
			else if (!AICF_InfantrySpawnPlacement.IsUsable(group, destinations[index], checkedPosition))
				reason = "COMMIT_DESTINATION_UNUSABLE";
			else if (vector.Distance(checkedPosition, destinations[index]) > 0.1)
				reason = "COMMIT_DESTINATION_PROJECTION_CHANGED";
			else
				reason = string.Empty;
			if (!reason.IsEmpty())
			{
				Blocked(order, reason);
				break;
			}
			// Общий budget расходуется до первой мутации, даже при частичном
			// результате. Асинхронный Teleport не даёт права повторить перенос.
			order.m_Slot.MarkIsolatedNavmeshRecoveryUsed();
			vector transform[4];
			member.GetWorldTransform(transform);
			transform[3] = destinations[index];
			member.Teleport(transform);
			relocated++;
			order.Log("INFANTRY_RECRUITMENT_HIDDEN_MEMBER", string.Format(
				"entity=%1 before=%2 destination=%3 shift_m=%4 movement_confirmation=PENDING",
				identities[index], origins[index], destinations[index], vector.DistanceXZ(origins[index], destinations[index])));
		}
		if (relocated == 0)
			return false;
		order.Log("INFANTRY_RECRUITMENT_HIDDEN_SUBMITTED", string.Format(
			"relocated=%1 expected=%2 max_shift_m=8 identity_preserved=1 movement_confirmation=PENDING", relocated, characters.Count()));
		return true;
	}

	protected static bool IsMemberCurrent(AIAgent agent, ChimeraCharacter character, SCR_AIGroup group, SCR_CampaignFaction faction)
	{
		if (!agent || !character || agent.GetParentGroup() != group || agent.GetControlledEntity() != character ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character))
			return false;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return affiliation && affiliation.GetAffiliatedFaction() == faction && access &&
			!character.IsInVehicle() && !CompartmentAccessComponent.GetVehicleIn(character) &&
			!access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut();
	}

	protected static bool FindDestination(SCR_AIGroup group, vector origin, vector target,
		array<vector> reserved, out vector destination)
	{
		vector forward = target - origin;
		forward[1] = 0;
		float distance = forward.Length();
		if (distance <= AICF_InfantryRecruitmentConfig.ARRIVAL_METERS + MAX_SHIFT_METERS)
			return false;
		forward.Normalize();
		vector right = Vector(-forward[2], 0, forward[0]);
		for (int ring; ring < 3; ring++)
		{
			for (int direction; direction < 8; direction++)
			{
				float angle = direction * 45 * Math.DEG2RAD;
				vector query = origin + (forward * Math.Cos(angle) + right * Math.Sin(angle)) * (3 + ring * 2);
				vector candidate;
				if (!AICF_InfantrySpawnPlacement.IsUsable(group, query, candidate) ||
					vector.DistanceXZ(origin, candidate) < 2 || vector.DistanceXZ(origin, candidate) > MAX_SHIFT_METERS ||
					Math.AbsFloat(origin[1] - candidate[1]) > 1.5 ||
					vector.DistanceXZ(candidate, target) >= distance - 1 ||
					vector.DistanceXZ(candidate, target) <= AICF_InfantryRecruitmentConfig.ARRIVAL_METERS + 5)
					continue;
				bool overlaps;
				foreach (vector other : reserved)
				{
					if (vector.DistanceSqXZ(other, candidate) < 2.25)
						overlaps = true;
				}
				if (overlaps)
					continue;
				destination = candidate;
				return true;
			}
		}
		return false;
	}

	protected static bool Blocked(AICF_InfantryRecruitmentOrder order, string reason)
	{
		order.Log("INFANTRY_RECRUITMENT_HIDDEN_BLOCKED", "reason=" + reason + " movement_confirmation=NONE");
		return false;
	}
}
