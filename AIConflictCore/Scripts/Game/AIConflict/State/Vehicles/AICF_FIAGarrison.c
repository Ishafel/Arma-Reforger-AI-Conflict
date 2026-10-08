// Один initial asset одной базы. Ни смена владельца базы, ни потери не меняют slot.
class AICF_FIAGarrison
{
	int m_iSlot;
	int m_iGeneration = 1;
	int m_iRequestedAtMs;
	int m_iRetryAtMs;
	bool m_bReady;
	bool m_bRetired;
	bool m_bTank;
	SCR_CampaignFaction m_Faction;
	SCR_CampaignMilitaryBaseComponent m_Base;
	EntityID m_BaseId;
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	ResourceName m_sPrefab;
	vector m_vPosition;
	ref AICF_VehicleLease m_Lease;
	ref array<BaseCompartmentSlot> m_aSeats = {};
	ref array<ChimeraCharacter> m_aCrew = {};
	ref array<EntityID> m_aCrewIds = {};

	bool BaseIdentity()
	{
		return Replication.IsServer() && m_Base && m_Base.GetOwner() && m_Base.GetOwner().GetID() == m_BaseId;
	}

	bool VehicleIdentity()
	{
		return Replication.IsServer() && m_Vehicle && m_Vehicle.GetID() == m_VehicleId &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle) &&
			SCR_Faction.GetEntityFaction(m_Vehicle) == m_Faction;
	}

	bool GroupIdentity()
	{
		return Replication.IsServer() && m_Group && m_Group.GetID() == m_GroupId && m_Group.GetFaction() == m_Faction;
	}

	bool OwnsMember(IEntity entity)
	{
		if (m_bRetired || !GroupIdentity() || !entity || !AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(entity)) return false;
		int index = m_aCrew.Find(ChimeraCharacter.Cast(entity));
		if (index < 0 || entity.GetID() != m_aCrewIds[index]) return false;
		AIControlComponent control = AIControlComponent.Cast(entity.FindComponent(AIControlComponent));
		return control && control.GetAIAgent() && control.GetAIAgent().GetParentGroup() == m_Group;
	}

	void Log(string eventName, string details)
	{
		AICF_Stage3Diagnostics.Info(eventName, string.Format("faction=FIA numeric_slot=%1 generation=%2 base=%3 vehicle=%4 group=%5 tank=%6 ",
			m_iSlot, m_iGeneration, m_BaseId, m_VehicleId, m_GroupId, m_bTank) + details);
	}
}
