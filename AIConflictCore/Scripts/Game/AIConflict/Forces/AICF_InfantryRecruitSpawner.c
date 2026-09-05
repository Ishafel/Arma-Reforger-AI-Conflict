class AICF_InfantryRecruitSpawner : AICF_GroupSpawner
{
	protected AICF_GroupSpawner m_RosterSource;

	void SetRosterSource(AICF_GroupSpawner source)
	{
		m_RosterSource = source;
	}
	// Выбирается первая отсутствующая позиция уже заданного roster, а не новый состав.
	bool FindMissingMember(AICF_GroupSlot slot, SCR_CampaignFaction faction, out ResourceName prefab, out string role, out int memberIndex)
	{
		prefab = ResourceName.Empty;
		role = string.Empty;
		if (!slot || !slot.GetGroup() || !faction ||
			AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) >= slot.GetDesiredSize())
			return false;
		if (!m_RosterSource)
			return false;
		for (int index = 0; index < slot.GetDesiredSize(); index++)
		{
			if (slot.HasRosterMember(index))
				continue;
			prefab = m_RosterSource.ResolveRecruitPrefab(faction, index, role);
			if (prefab.IsEmpty())
				return false;
			memberIndex = index;
			return true;
		}
		return false;
	}

	bool BeginRecruit(AICF_InfantryRecruitmentOrder order)
	{
		if (!Replication.IsServer() || !order || order.m_Donor || !order.IsCurrent(order.m_Slot) ||
			!order.HasSafeBarracks() || !order.IsPhysicallyPresent() || order.m_sPrefab.IsEmpty())
			return false;
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(order.m_Group.FindComponent(AIPathfindingComponent));
		vector position;
		if (!pathfinding || !pathfinding.GetClosestPositionOnNavmesh(order.m_vPosition, "15 5 15", position) ||
			vector.DistanceSqXZ(position, order.m_vPosition) > 225)
			return false;
		SCR_AIGroup donor = SpawnGroup(order.m_Faction, order.m_Base, order.m_Slot.GetSlotId(), 1, false);
		if (!donor)
			return false;
		order.m_Donor = donor;
		order.m_DonorId = donor.GetID();
		order.m_iSpawnAtMs = System.GetTickCount();
		donor.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		donor.SetDeleteWhenEmpty(false);
		donor.SetOrigin(position);
		donor.m_aUnitPrefabSlots.Clear();
		donor.m_aUnitPrefabSlots.Insert(order.m_sPrefab);
		return BeginRosterSpawn(donor, 1);
	}

	bool ClearRecruit(AICF_InfantryRecruitmentOrder order)
	{
		if (!Replication.IsServer() || !order)
			return false;
		SCR_AIGroup donor = order.m_Donor;
		if (!donor)
			return true;
		RplComponent rpl = RplComponent.Cast(donor.FindComponent(RplComponent));
		if (donor.GetID() != order.m_DonorId || donor.GetFaction() != order.m_Faction || !rpl || !rpl.IsMaster())
			return false;
		ChimeraAIWorld world = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!world)
			return false;
		world.PurgeSpawnRequestsForGroup(donor);
		array<AIAgent> agents = {};
		donor.GetAgents(agents);
		if (agents.Count() > 1)
			return false;
		foreach (AIAgent agent : agents)
		{
			if (!agent || !agent.GetControlledEntity() || agent.GetParentGroup() != donor)
				return false;
			IEntity character = agent.GetControlledEntity();
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
			RplComponent characterRpl = RplComponent.Cast(character.FindComponent(RplComponent));
			if (!controller || controller.IsPlayerControlled() || !characterRpl || !characterRpl.IsMaster())
				return false;
		}
		donor.DespawnMembers();
		RplComponent.DeleteRplEntity(donor, false);
		order.m_Donor = null;
		order.m_iSpawnAtMs = 0;
		return true;
	}
}
