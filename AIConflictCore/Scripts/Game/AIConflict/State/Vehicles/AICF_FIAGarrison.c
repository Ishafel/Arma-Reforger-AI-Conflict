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
	int m_iNode;
	bool m_bStrongpoint;
	ResourceName m_sPassengerTemplate;
	ref array<ResourceName> m_aPassengerPrefabs = {};
	bool m_bPassengers;
	bool m_bStaticDefense;
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
	protected string m_sRecoveryBan;
	protected ref AICF_FIAGarrisonIntegrity m_VehicleIntegrity;
	protected ref array<ref AICF_FIAGarrisonIntegrity> m_aMemberIntegrity = {};
	string m_sPatrolHold;
	ref array<bool> m_aEssentialSeats = {};
	ref AICF_VehicleLease m_Lease;
	ref array<BaseCompartmentSlot> m_aSeats = {};
	ref array<ChimeraCharacter> m_aCrew = {};
	ref array<EntityID> m_aCrewIds = {};
	ref AICF_FIAProximityLifecycle m_Lifecycle;
	ref AICF_FIAProximityLifecycle m_DesantLifecycle;

	void SuspendMemberIntegrity(int index)
	{
		if (index < m_aMemberIntegrity.Count() && m_aMemberIntegrity[index]) m_aMemberIntegrity[index].Stop();
		if (index < m_aMemberIntegrity.Count()) m_aMemberIntegrity[index] = null;
	}

	void EnsureMemberIntegrity(int index, ChimeraCharacter member)
	{
		if (!Replication.IsServer() || index < 0 || index >= m_aCrew.Count() || m_aCrew[index] != member) return;
		while (m_aMemberIntegrity.Count() < m_aCrew.Count()) m_aMemberIntegrity.Insert(null);
		if (m_aMemberIntegrity[index]) return;
		AICF_FIAGarrisonIntegrity watch = new AICF_FIAGarrisonIntegrity();
		m_aMemberIntegrity[index] = watch;
		watch.Start(this, member, false, m_aEssentialSeats[index]);
	}

	bool BaseIdentity()
	{
		return Replication.IsServer() && m_Base && m_Base.GetOwner() && m_Base.GetOwner().GetID() == m_BaseId;
	}

	void BlockRecovery(string reason, EntityID memberId = EntityID.INVALID)
	{
		if (!Replication.IsServer() || !m_sRecoveryBan.IsEmpty()) return;
		m_sRecoveryBan = reason;
		// Не оставляем ходовую машину в бесконечном ожидании запрещённого переноса.
		m_bCrewRecoveryPending = false;
		Log("FIA_GARRISON_RECOVERY_BANNED", string.Format("reason=%1 member=%2 permanent=1", reason, memberId));
	}

	string RecoveryBan() { return m_sRecoveryBan; }

	void WatchVehicleIntegrity()
	{
		if (!Replication.IsServer() || m_VehicleIntegrity) return;
		m_VehicleIntegrity = new AICF_FIAGarrisonIntegrity();
		m_VehicleIntegrity.Start(this, m_Vehicle, true, true);
	}

	void WatchMemberIntegrity(ChimeraCharacter member, bool essential)
	{
		if (!Replication.IsServer()) return;
		AICF_FIAGarrisonIntegrity watch = new AICF_FIAGarrisonIntegrity();
		m_aMemberIntegrity.Insert(watch);
		watch.Start(this, member, false, essential);
	}

	bool RecoveryAllowed()
	{
		if (!Replication.IsServer() || m_bRetired) return false;
		if (!m_sRecoveryBan.IsEmpty()) return false;
		if (!VehicleIdentity() || !GroupIdentity()) { BlockRecovery("STALE_VEHICLE_GROUP_IDENTITY"); return false; }
		if (!m_VehicleIntegrity || !m_VehicleIntegrity.Check()) { BlockRecovery("UNKNOWN_VEHICLE_DAMAGE"); return false; }
		int count = m_aSeats.Count();
		if (count == 0 || m_aCrew.Count() != count || m_aCrewIds.Count() != count ||
			m_aEssentialSeats.Count() != count || m_aMemberIntegrity.Count() != count)
		{
			BlockRecovery("UNKNOWN_ORIGINAL_ROSTER");
			return false;
		}
		for (int i; i < count; i++)
		{
			if (m_aMemberIntegrity[i] && !m_aMemberIntegrity[i].Matches(m_aCrew[i], m_aCrewIds[i]))
			{
				BlockRecovery("MISSING_OR_STALE_MEMBER");
				return false;
			}
			if (!m_aMemberIntegrity[i] || !m_aMemberIntegrity[i].Check()) { BlockRecovery("UNKNOWN_MEMBER_DAMAGE"); return false; }
			if (!m_aCrew[i] || m_aCrew[i].GetID() != m_aCrewIds[i] || !OwnsMember(m_aCrew[i]))
			{
				BlockRecovery("MISSING_OR_STALE_MEMBER");
				return false;
			}
		}
		return m_sRecoveryBan.IsEmpty();
	}

	void StopIntegrity()
	{
		if (m_VehicleIntegrity) m_VehicleIntegrity.Stop();
		foreach (AICF_FIAGarrisonIntegrity watch : m_aMemberIntegrity) { if (watch) watch.Stop(); }
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
		if (index < 0 || index >= m_aCrewIds.Count() || entity.GetID() != m_aCrewIds[index]) return false;
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
