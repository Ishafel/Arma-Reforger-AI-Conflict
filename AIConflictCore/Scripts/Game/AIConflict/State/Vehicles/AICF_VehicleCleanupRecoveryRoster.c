// Permission state владельца cleanup. Это не diagnostic ActorHistory:
// захватывается из текущих native members при приёме release и требует,
// чтобы те же сущности оставались в том же актуальном managed group.
class AICF_VehicleCleanupRecoveryMember
{
	AIAgent m_Agent;
	ChimeraCharacter m_Character;
	EntityID m_Id;
	string m_sRpl;

	void AICF_VehicleCleanupRecoveryMember(AIAgent agent, ChimeraCharacter character)
	{
		m_Agent = agent;
		m_Character = character;
		m_Id = character.GetID();
		m_sRpl = AICF_VehicleCleanupRecoveryRoster.ComponentRpl(character);
	}

	bool Matches(AIAgent agent, ChimeraCharacter character)
	{
		return agent && character && agent == m_Agent && character == m_Character &&
			character.GetID() == m_Id && m_sRpl != "NONE" &&
			AICF_VehicleCleanupRecoveryRoster.ComponentRpl(character) == m_sRpl &&
			agent.GetControlledEntity() == character && character.GetAIControlComponent() &&
			character.GetAIControlComponent().GetControlAIAgent() == agent;
	}
}

class AICF_VehicleCleanupRecoveryRoster
{
	protected AICF_GroupSlot m_Slot;
	protected SCR_AIGroup m_Group;
	protected EntityID m_GroupId;
	protected string m_sGroupRpl;
	protected FactionKey m_sFaction;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iLeaseGeneration;
	protected int m_iVehicleGeneration;
	protected EntityID m_VehicleId;
	protected string m_sVehicleRpl;
	protected ref array<ref AICF_VehicleCleanupRecoveryMember> m_aMembers = {};

	static bool IsIncapacitatedPhysicalState(ECharacterLifeState life, bool characterInVehicle, bool linked,
		bool inCompartment, bool gettingIn, bool gettingOut, bool switching, bool inside)
	{
		return life == ECharacterLifeState.INCAPACITATED && inside && !characterInVehicle && !linked &&
			!inCompartment && !gettingIn && !gettingOut && !switching;
	}

	static bool IsDetachedHullPhysicalState(ECharacterLifeState life, bool characterInVehicle, bool linked,
		bool inCompartment, bool gettingIn, bool gettingOut, bool switching, bool inside)
	{
		return (life == ECharacterLifeState.ALIVE || life == ECharacterLifeState.INCAPACITATED) &&
			inside && !characterInVehicle && !linked && !inCompartment && !gettingIn && !gettingOut && !switching;
	}

	static string ComponentRpl(IEntity entity)
	{
		RplComponent rpl;
		if (entity) rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl) return "NONE";
		return rpl.Id().ToString();
	}

	static AICF_VehicleCleanupRecoveryRoster Capture(AICF_TransportTrip trip, AICF_VehicleLease lease, SCR_AIGroup group)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.GetLease() != lease ||
			!lease || !group || !trip.GetAssignment() || trip.GetAssignment().GetGroup() != group ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) ||
			!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return null;
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		if (!slot || slot.GetGroup() != group || slot.GetSpawnGeneration() != lease.GetGroupGeneration()) return null;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.IsEmpty() || agents.Count() > AICF_Stage1Config.MAX_GROUP_SIZE) return null;
		AICF_VehicleCleanupRecoveryRoster roster = new AICF_VehicleCleanupRecoveryRoster();
		roster.m_Slot = slot;
		roster.m_Group = group;
		roster.m_GroupId = group.GetID();
		roster.m_sGroupRpl = ComponentRpl(group);
		roster.m_sFaction = lease.GetFactionKey();
		roster.m_iGroupGeneration = lease.GetGroupGeneration();
		roster.m_iTripGeneration = lease.GetTripGeneration();
		roster.m_iLeaseGeneration = lease.GetLeaseGeneration();
		roster.m_iVehicleGeneration = lease.GetVehicleGeneration();
		roster.m_VehicleId = lease.GetEntityId();
		roster.m_sVehicleRpl = lease.GetRplId();
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group) return null;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character || !AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(character)) return null;
			roster.m_aMembers.Insert(new AICF_VehicleCleanupRecoveryMember(agent, character));
		}
		return roster;
	}

	bool IsCurrent(AICF_VehicleCleanupJob job, Vehicle vehicle, AIAgent agent, ChimeraCharacter character)
	{
		if (!Replication.IsServer() || !job || !job.m_Fence || !job.m_Fence.IsComplete() ||
			job.m_Group != m_Group || !m_Group || m_Group.GetID() != m_GroupId ||
			m_sGroupRpl == "NONE" || ComponentRpl(m_Group) != m_sGroupRpl ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group)) return false;
		if (!m_Slot || m_Slot.GetGroup() != m_Group || m_Slot.GetSpawnGeneration() != m_iGroupGeneration ||
			!m_Group.GetFaction() || m_Group.GetFaction().GetFactionKey() != m_sFaction) return false;
		if (!vehicle || vehicle.GetID() != m_VehicleId || ComponentRpl(vehicle) != m_sVehicleRpl ||
			job.m_Fence.GetEntityId() != m_VehicleId || job.m_Fence.GetRplId() != m_sVehicleRpl ||
			job.m_Fence.GetGroupGeneration() != m_iGroupGeneration || job.m_Fence.GetTripGeneration() != m_iTripGeneration ||
			job.m_Fence.GetLeaseGeneration() != m_iLeaseGeneration || job.m_Fence.GetVehicleGeneration() != m_iVehicleGeneration) return false;
		if (!agent || agent.GetParentGroup() != m_Group || !character ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(character)) return false;
		array<AIAgent> current = {};
		m_Group.GetAgents(current);
		if (!current.Contains(agent)) return false;
		foreach (AICF_VehicleCleanupRecoveryMember member : m_aMembers)
			if (member.Matches(agent, character)) return true;
		return false;
	}
}
