// Exact factory result/entry vehicle, до native activation. Тело резервирует
// unit и в коротком промежутке до появления counted AIAgent; native census
// после activation заменяет, а не складывает эту occupancy.
class AICF_AIQuotaBody
{
	IEntity m_Entity;
	EntityID m_Id;
	BaseWorld m_World;
	RplComponent m_Rpl;
	string m_sRpl;
	AICF_EAIQuotaBucket m_Bucket;
	AICF_AIQuotaGroupOwner m_GroupOwner;
	ref AICF_AIQuotaCreationGuard m_CreationGuard;
	bool m_bQuarantined;
	bool m_bVehicle;
	bool m_bReleased;

	static AICF_AIQuotaBody Capture(IEntity entity, AICF_EAIQuotaBucket bucket, AICF_AIQuotaGroupOwner owner = null)
	{
		if (!Replication.IsServer() || !GetGame() || !entity || entity.GetWorld() != GetGame().GetWorld() ||
			bucket < 0 || bucket >= AICF_EAIQuotaBucket.COUNT)
			return null;
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl || !rpl.IsMaster() || !rpl.Id().IsValid()) return null;
		AICF_AIQuotaBody body = new AICF_AIQuotaBody();
		body.m_Entity = entity;
		body.m_Id = entity.GetID();
		body.m_World = entity.GetWorld();
		body.m_Rpl = rpl;
		body.m_sRpl = rpl.Id().ToString();
		body.m_Bucket = bucket;
		body.m_GroupOwner = owner;
		body.m_bVehicle = Vehicle.Cast(entity) != null;
		if (!body.MatchesIdentity()) return null;
		return body;
	}

	string Key()
	{
		return string.Format("%1/%2", m_Id, m_sRpl);
	}

	bool MatchesIdentity()
	{
		return Replication.IsServer() && GetGame() && m_Entity && m_World && GetGame().GetWorld() == m_World &&
			m_Entity.GetWorld() == m_World && m_Entity.GetID() == m_Id && m_World.FindEntityByID(m_Id) == m_Entity &&
			m_Rpl && m_Rpl.IsMaster() && m_Entity.FindComponent(RplComponent) == m_Rpl &&
			m_Rpl.Id().IsValid() && m_Rpl.Id().ToString() == m_sRpl && !m_sRpl.IsEmpty();
	}

	bool IsDefinitelyGone()
	{
		if (!Replication.IsServer() || !GetGame() || !m_World || GetGame().GetWorld() != m_World) return false;
		return !m_Entity && !m_World.FindEntityByID(m_Id);
	}

	bool IsNativeDeadAndInactive()
	{
		if (!MatchesIdentity() || m_bVehicle) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Entity);
		if (!character) return false;
		CharacterControllerComponent controller = character.GetCharacterController();
		AIControlComponent control = character.GetAIControlComponent();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.DEAD || !control || controller.IsPlayerControlled())
			return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || players.GetPlayerIdFromControlledEntity(character) != 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(character) > 0) return false;
		AIAgent agent = control.GetControlAIAgent();
		return !agent || (agent.GetControlledEntity() == character && !agent.IsAIActivated() && !agent.CountTowardsAIWorldLimit());
	}
}
