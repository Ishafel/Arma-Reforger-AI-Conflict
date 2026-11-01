// Синхронная identity граница перед допуском к native посадке. Этот объект
// не выдаёт quota units и не меняет seat reservation, faction или AI state.
class AICF_AIQuotaVehicleEntity
{
	IEntity m_Entity;
	BaseWorld m_World;
	EntityID m_Id;
	RplComponent m_Rpl;
	string m_sRpl;

	static AICF_AIQuotaVehicleEntity Capture(IEntity entity)
	{
		if (!Replication.IsServer() || !GetGame() || !entity || entity.GetWorld() != GetGame().GetWorld()) return null;
		AICF_AIQuotaVehicleEntity identity = new AICF_AIQuotaVehicleEntity();
		identity.m_Entity = entity;
		identity.m_World = entity.GetWorld();
		identity.m_Id = entity.GetID();
		identity.m_Rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!identity.m_Rpl) return null;
		identity.m_sRpl = identity.m_Rpl.Id().ToString();
		if (!identity.IsCurrent()) return null;
		return identity;
	}

	bool IsCurrent()
	{
		if (!Replication.IsServer() || !GetGame() || !m_Entity || !m_World || GetGame().GetWorld() != m_World) return false;
		if (m_Entity.GetWorld() != m_World || m_Entity.GetID() != m_Id || m_World.FindEntityByID(m_Id) != m_Entity) return false;
		if (!m_Rpl || !m_Rpl.IsMaster() || m_Rpl.IsProxy() || m_Entity.FindComponent(RplComponent) != m_Rpl) return false;
		return m_Rpl.Id().IsValid() && !m_sRpl.IsEmpty() && m_Rpl.Id().ToString() == m_sRpl;
	}
}

class AICF_AIQuotaVehicleEntry
{
	static const int MAX_COMPARTMENTS = 256;
	ref AICF_AIQuotaVehicleEntity m_ActorIdentity;
	ref AICF_AIQuotaVehicleEntity m_GroupIdentity;
	ref AICF_AIQuotaVehicleEntity m_VehicleIdentity;
	ref AICF_AIQuotaVehicleEntity m_SeatOwnerIdentity;
	ChimeraCharacter m_Actor;
	AIControlComponent m_Control;
	AIAgent m_Agent;
	EntityID m_AgentId;
	SCR_AIGroup m_Group;
	Faction m_Faction;
	FactionKey m_sGroupFactionKey;
	FactionAffiliationComponent m_Affiliation;
	Faction m_ActorFaction;
	FactionKey m_sActorFactionKey;
	IEntity m_NativeVehicle;
	IEntity m_Vehicle;
	BaseCompartmentSlot m_Seat;
	BaseCompartmentManagerComponent m_Manager;
	int m_iManagerId;
	int m_iSlotId;

	static AICF_AIQuotaVehicleEntry Capture(IEntity actor, IEntity nativeVehicle, BaseCompartmentSlot seat)
	{
		if (!actor || !nativeVehicle || !seat) return null;
		AICF_AIQuotaVehicleEntry entry = new AICF_AIQuotaVehicleEntry();
		entry.m_Actor = ChimeraCharacter.Cast(actor);
		if (!entry.m_Actor) return null;
		entry.m_Control = entry.m_Actor.GetAIControlComponent();
		if (!entry.m_Control) return null;
		entry.m_Agent = entry.m_Control.GetControlAIAgent();
		if (!entry.m_Agent) return null;
		entry.m_AgentId = entry.m_Agent.GetID();
		entry.m_Group = SCR_AIGroup.Cast(entry.m_Agent.GetParentGroup());
		if (!entry.m_Group) return null;
		entry.m_Faction = entry.m_Group.GetFaction();
		if (!entry.m_Faction) return null;
		entry.m_sGroupFactionKey = entry.m_Faction.GetFactionKey();
		entry.m_Affiliation = FactionAffiliationComponent.Cast(actor.FindComponent(FactionAffiliationComponent));
		if (entry.m_Affiliation) entry.m_ActorFaction = entry.m_Affiliation.GetAffiliatedFaction();
		if (entry.m_ActorFaction) entry.m_sActorFactionKey = entry.m_ActorFaction.GetFactionKey();
		entry.m_NativeVehicle = nativeVehicle;
		entry.m_Seat = seat;
		entry.m_Vehicle = seat.GetVehicle();
		entry.m_Manager = seat.GetManager();
		entry.m_iManagerId = seat.GetCompartmentMgrID();
		entry.m_iSlotId = seat.GetCompartmentSlotID();
		entry.m_ActorIdentity = AICF_AIQuotaVehicleEntity.Capture(actor);
		entry.m_GroupIdentity = AICF_AIQuotaVehicleEntity.Capture(entry.m_Group);
		entry.m_VehicleIdentity = AICF_AIQuotaVehicleEntity.Capture(entry.m_Vehicle);
		entry.m_SeatOwnerIdentity = AICF_AIQuotaVehicleEntity.Capture(seat.GetOwner());
		if (!entry.IsCurrent()) return null;
		return entry;
	}

	bool IsActorCurrent()
	{
		if (!Replication.IsServer() || !GetGame()) return false;
		if (!m_Actor || !m_Control || !m_Agent || !m_Group || !m_Faction || !m_ActorIdentity || !m_GroupIdentity) return false;
		if (m_ActorIdentity.m_Entity != m_Actor || m_GroupIdentity.m_Entity != m_Group) return false;
		if (m_Actor.GetAIControlComponent() != m_Control || m_Control.GetControlAIAgent() != m_Agent) return false;
		if (m_Agent.GetControlledEntity() != m_Actor || m_Agent.GetParentGroup() != m_Group || m_Group.GetFaction() != m_Faction) return false;
		if (m_sGroupFactionKey.IsEmpty() || m_Faction.GetFactionKey() != m_sGroupFactionKey) return false;
		if (m_Actor.FindComponent(FactionAffiliationComponent) != m_Affiliation) return false;
		Faction actorFaction;
		if (m_Affiliation) actorFaction = m_Affiliation.GetAffiliatedFaction();
		if (actorFaction != m_ActorFaction) return false;
		if (actorFaction && actorFaction.GetFactionKey() != m_sActorFactionKey) return false;
		if (!m_Agent.GetWorld() || m_Agent.GetWorld() != m_ActorIdentity.m_World || m_Agent.GetID() != m_AgentId) return false;
		if (m_Agent.GetWorld().FindEntityByID(m_AgentId) != m_Agent) return false;
		CharacterControllerComponent controller = m_Actor.GetCharacterController();
		if (!controller || controller.IsPlayerControlled() || controller.GetLifeState() != ECharacterLifeState.ALIVE) return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || players.GetPlayerIdFromControlledEntity(m_Actor) > 0 || SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(m_Actor) > 0) return false;
		return true;
	}

	bool IsSeatCurrent()
	{
		if (!m_Vehicle || !m_Seat || !m_Manager || !m_VehicleIdentity || !m_SeatOwnerIdentity) return false;
		if (m_VehicleIdentity.m_Entity != m_Vehicle || !m_SeatOwnerIdentity.m_Entity) return false;
		if (m_Seat.GetVehicle() != m_Vehicle || m_Seat.GetOwner() != m_SeatOwnerIdentity.m_Entity || m_Seat.GetManager() != m_Manager) return false;
		if (m_NativeVehicle != m_Vehicle && m_NativeVehicle != m_SeatOwnerIdentity.m_Entity) return false;
		if (m_Seat.GetCompartmentMgrID() != m_iManagerId || m_Seat.GetCompartmentSlotID() != m_iSlotId) return false;
		if (m_SeatOwnerIdentity.m_Entity.FindComponent(BaseCompartmentManagerComponent) != m_Manager) return false;
		return true;
	}

	bool IsCurrent()
	{
		if (!m_ActorIdentity || !m_GroupIdentity || !m_VehicleIdentity || !m_SeatOwnerIdentity) return false;
		if (!m_ActorIdentity.IsCurrent() || !m_GroupIdentity.IsCurrent() || !m_VehicleIdentity.IsCurrent() || !m_SeatOwnerIdentity.IsCurrent()) return false;
		if (!IsActorCurrent() || !IsSeatCurrent()) return false;
		array<BaseCompartmentSlot> seats = {};
		m_Manager.GetCompartments(seats);
		if (seats.Count() > MAX_COMPARTMENTS) return false;
		int matches = 0;
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat) return false;
			if (seat == m_Seat) matches++;
		}
		if (matches != 1) return false;
		// Нативные getters могут вызвать callbacks; повторяем физические IDs и
		// control/group после последнего чтения manager. Фракцию машины не выводим
		// из фракции экипажа: CIV и захваченные машины допустимы.
		if (!m_ActorIdentity.IsCurrent() || !m_GroupIdentity.IsCurrent() || !m_VehicleIdentity.IsCurrent() || !m_SeatOwnerIdentity.IsCurrent()) return false;
		return IsActorCurrent() && IsSeatCurrent();
	}
}
