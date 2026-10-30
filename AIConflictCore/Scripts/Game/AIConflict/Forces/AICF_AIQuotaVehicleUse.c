// Read-only evidence удержания unit. Отсутствие AI occupant доказывается по
// полной bounded compartment topology; null/смена/неполнота не означает empty.
class AICF_AIQuotaVehicleOccupant
{
	IEntity m_Entity;
	EntityID m_Id;
	BaseWorld m_World;
	CharacterControllerComponent m_Controller;
	AIControlComponent m_Control;
	AIAgent m_Agent;
	ECharacterLifeState m_Life;
	bool m_bHuman;
	bool m_bAI;
	bool m_bActive;
	bool m_bCounted;

	static AICF_AIQuotaVehicleOccupant Capture(IEntity entity)
	{
		if (!Replication.IsServer() || !GetGame() || !entity || entity.GetWorld() != GetGame().GetWorld()) return null;
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		PlayerManager players = GetGame().GetPlayerManager();
		if (!character || !players || !character.GetCharacterController()) return null;
		AICF_AIQuotaVehicleOccupant row = new AICF_AIQuotaVehicleOccupant();
		row.m_Entity = entity;
		row.m_Id = entity.GetID();
		row.m_World = entity.GetWorld();
		row.m_Controller = character.GetCharacterController();
		row.m_Control = character.GetAIControlComponent();
		if (row.m_Control) row.m_Agent = row.m_Control.GetControlAIAgent();
		row.m_Life = row.m_Controller.GetLifeState();
		row.m_bHuman = row.m_Controller.IsPlayerControlled() || players.GetPlayerIdFromControlledEntity(entity) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(entity) > 0;
		if (row.m_Agent)
		{
			if (row.m_Agent.GetControlledEntity() != entity || row.m_Agent.GetWorld() != row.m_World) return null;
			row.m_bActive = row.m_Agent.IsAIActivated();
			row.m_bCounted = row.m_Agent.CountTowardsAIWorldLimit();
		}
		row.m_bAI = !row.m_bHuman && (row.m_Life != ECharacterLifeState.DEAD || row.m_bActive || row.m_bCounted);
		if (!row.IsCurrent()) return null;
		return row;
	}

	bool IsCurrent()
	{
		if (!Replication.IsServer() || !GetGame() || !m_World || m_World != GetGame().GetWorld() || !m_Entity) return false;
		if (m_Entity.GetID() != m_Id || m_Entity.GetWorld() != m_World || m_World.FindEntityByID(m_Id) != m_Entity) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Entity);
		PlayerManager players = GetGame().GetPlayerManager();
		if (!character || !players || !m_Controller || character.GetCharacterController() != m_Controller || character.GetAIControlComponent() != m_Control) return false;
		if (m_Controller.GetLifeState() != m_Life) return false;
		bool human = m_Controller.IsPlayerControlled() || players.GetPlayerIdFromControlledEntity(m_Entity) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(m_Entity) > 0;
		if (human != m_bHuman) return false;
		AIAgent agent;
		if (m_Control) agent = m_Control.GetControlAIAgent();
		if (agent != m_Agent) return false;
		if (agent && (agent.GetControlledEntity() != m_Entity || agent.GetWorld() != m_World ||
			agent.IsAIActivated() != m_bActive || agent.CountTowardsAIWorldLimit() != m_bCounted)) return false;
		return true;
	}
}

class AICF_AIQuotaVehicleUse
{
	static const int MAX_TOPOLOGY = 256;
	ref AICF_AIQuotaVehicleEntity m_Identity;
	AICF_AIQuotaAgentRow m_Native;
	bool m_bHadNative;
	bool m_bActive;
	bool m_bCounted;
	bool m_bHasAI;
	ref array<BaseCompartmentSlot> m_aSeats = {};
	ref array<ref AICF_AIQuotaVehicleOccupant> m_aOccupants = {};

	static bool CollectSeats(IEntity vehicle, array<BaseCompartmentSlot> seats)
	{
		if (!vehicle || !GetGame() || vehicle.GetWorld() != GetGame().GetWorld() || !seats) return false;
		seats.Clear();
		array<IEntity> nodes = {vehicle};
		for (int index; index < nodes.Count(); index++)
		{
			IEntity node = nodes[index];
			if (!node || node.GetWorld() != vehicle.GetWorld() || nodes.Count() > MAX_TOPOLOGY) return false;
			IEntity child = node.GetChildren();
			while (child)
			{
				if (nodes.Count() >= MAX_TOPOLOGY || nodes.Contains(child) || child.GetParent() != node) return false;
				nodes.Insert(child);
				child = child.GetSibling();
			}
		}
		bool foundManager;
		int examinedSeats;
		foreach (IEntity owner : nodes)
		{
			if (!owner) return false;
			BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(owner.FindComponent(BaseCompartmentManagerComponent));
			if (!manager) continue;
			foundManager = true;
			array<BaseCompartmentSlot> local = {};
			manager.GetCompartments(local);
			if (local.Count() > MAX_TOPOLOGY - examinedSeats) return false;
			examinedSeats += local.Count();
			foreach (BaseCompartmentSlot seat : local)
			{
				if (!seat || !seat.GetOwner() || !nodes.Contains(seat.GetOwner()) || !seat.GetManager() || seat.GetVehicle() != vehicle) return false;
				// A manager may expose a child manager's same exact slot. It is
				// one physical compartment, not a second occupant/cost.
				if (!seats.Contains(seat)) seats.Insert(seat);
			}
		}
		return foundManager;
	}

	static AICF_AIQuotaVehicleUse Capture(IEntity vehicle, AICF_AIQuotaCensus snapshot)
	{
		if (!snapshot) return null;
		AICF_AIQuotaAgentRow found;
		for (int index; index < snapshot.Count(); index++)
		{
			AICF_AIQuotaAgentRow row = snapshot.GetRow(index);
			if (!row) return null;
			if (row.m_Entity != vehicle) continue;
			if (found || !row.m_bVehicle || !row.IsCurrent(vehicle.GetWorld())) return null;
			found = row;
		}
		return CaptureRow(vehicle, found);
	}

	static AICF_AIQuotaVehicleUse CaptureRow(IEntity vehicle, AICF_AIQuotaAgentRow row)
	{
		AICF_AIQuotaVehicleUse use = new AICF_AIQuotaVehicleUse();
		use.m_Identity = AICF_AIQuotaVehicleEntity.Capture(vehicle);
		if (!use.m_Identity || !CollectSeats(vehicle, use.m_aSeats)) return null;
		if (row)
		{
			if (!row.m_bVehicle || row.m_Entity != vehicle || !row.IsCurrent(vehicle.GetWorld())) return null;
			use.m_Native = row;
			use.m_bHadNative = true;
			use.m_bActive = row.m_bActivated;
			use.m_bCounted = row.m_bCounted;
		}
		foreach (BaseCompartmentSlot seat : use.m_aSeats)
		{
			AICF_AIQuotaVehicleOccupant occupant;
			IEntity entity = seat.GetOccupant();
			if (entity)
			{
				occupant = AICF_AIQuotaVehicleOccupant.Capture(entity);
				if (!occupant) return null;
				if (occupant.m_bAI) use.m_bHasAI = true;
			}
			use.m_aOccupants.Insert(occupant);
		}
		if (!use.IsCurrent()) return null;
		return use;
	}

	bool IsCurrent()
	{
		if (!m_Identity || !m_Identity.IsCurrent() || m_aSeats.Count() != m_aOccupants.Count()) return false;
		if (m_bHadNative && !m_Native) return false;
		if (m_Native && !m_Native.IsCurrent(m_Identity.m_World)) return false;
		array<BaseCompartmentSlot> repeated = {};
		if (!CollectSeats(m_Identity.m_Entity, repeated) || repeated.Count() != m_aSeats.Count()) return false;
		for (int index; index < repeated.Count(); index++)
		{
			BaseCompartmentSlot seat = m_aSeats[index];
			if (!seat || repeated[index] != seat) return false;
			AICF_AIQuotaVehicleOccupant occupant = m_aOccupants[index];
			if (occupant)
			{
				if (seat.GetOccupant() != occupant.m_Entity || !occupant.IsCurrent()) return false;
			}
			else if (seat.GetOccupant()) return false;
		}
		return m_Identity.IsCurrent();
	}
}
