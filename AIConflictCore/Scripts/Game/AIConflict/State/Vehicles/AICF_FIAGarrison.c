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
	SCR_AIGroup m_DesantGroup;
	EntityID m_DesantGroupId;
	ResourceName m_sPrefab;
	vector m_vPosition;
	vector m_vHome;
	vector m_vPatrolTarget;
	vector m_vPatrolProgress;
	AIWaypoint m_PatrolWaypoint;
	EntityID m_PatrolWaypointId;
	int m_iPatrolCandidate;
	int m_iPatrolDirection = 1;
	int m_iPatrolLeg;
	int m_iPatrolArrivals;
	int m_iPatrolProgressAtMs;
	int m_iPatrolRetryAtMs;
	bool m_bReturning;
	bool m_bPatrolMoveFailed;
	bool m_bPassengersDeployed;
	bool m_bDisembarking;
	bool m_bCrewRecoveryPending;
	int m_iCrewRecoveryNextMs;
	int m_iDismountNextMs;
	int m_iRecoveryNextMs;
	int m_iPatrolFailures;
	int m_iVehicleTeleports;
	int m_iCrewTeleports;
	string m_sRecoveryBlocked;
	string m_sPatrolHold;
	ref array<bool> m_aEssentialSeats = {};
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
		if (!control || !control.GetAIAgent()) return false;
		if (control.GetAIAgent().GetParentGroup() == m_Group) return true;
		return IsDeployedPassenger(entity) && DesantIdentity() && control.GetAIAgent().GetParentGroup() == m_DesantGroup;
	}

	bool DesantIdentity()
	{
		return Replication.IsServer() && m_DesantGroup && m_DesantGroup.GetID() == m_DesantGroupId && m_DesantGroup.GetFaction() == m_Faction;
	}

	bool CanDrive()
	{
		if (!m_bReady || !VehicleIdentity() || !GroupIdentity()) return false;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(m_Vehicle.FindComponent(SCR_DamageManagerComponent));
		if (damage && damage.IsDestroyed()) return false;
		bool driver;
		foreach (BaseCompartmentSlot seat : m_aSeats)
		{
			if (!seat || !seat.GetOwner()) return false;
			IEntity occupant = seat.GetOccupant();
			if (occupant && !OwnsMember(occupant)) return false;
			if (seat.GetType() == ECompartmentType.PILOT) driver = OwnsMember(occupant);
		}
		return driver;
	}

	bool IsDeployedPassenger(IEntity entity)
	{
		int index = m_aCrew.Find(ChimeraCharacter.Cast(entity));
		return m_bPassengersDeployed && index >= 0 && index < m_aEssentialSeats.Count() && !m_aEssentialSeats[index];
	}

	bool PatrolWaypointIdentity()
	{
		return GroupIdentity() && m_PatrolWaypoint && m_PatrolWaypoint.GetID() == m_PatrolWaypointId;
	}

	void Log(string eventName, string details)
	{
		AICF_Stage3Diagnostics.Info(eventName, string.Format("faction=FIA numeric_slot=%1 generation=%2 base=%3 vehicle=%4 group=%5 tank=%6 ",
			m_iSlot, m_iGeneration, m_BaseId, m_VehicleId, m_GroupId, m_bTank) + details);
	}
}
