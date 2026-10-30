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

	// A native player avatar can transfer its Rpl role after the initial AI
	// census. This proves human custody only; it grants no AI/factory identity.
	bool HasConfirmedHumanCustody(AICF_AIQuotaCensus snapshot)
	{
		if (m_bVehicle || m_bReleased || m_bQuarantined || m_Bucket != AICF_EAIQuotaBucket.STOCK || m_GroupOwner || m_CreationGuard) return false;
		if (!snapshot || !snapshot.HasCompleteCapture()) return false;
		RplComponent originalRpl = m_Rpl;
		EntityID originalId = m_Id;
		string originalRplId = m_sRpl;
		BaseWorld originalWorld = m_World;
		if (!MatchesHumanEntityIdentity()) return false;
		ChimeraCharacter actor = ChimeraCharacter.Cast(m_Entity);
		if (!actor) return false;
		CharacterControllerComponent character = actor.GetCharacterController();
		if (!actor || !character) return false;
		AIControlComponent control = actor.GetAIControlComponent();
		if (!actor || !character || !control || !GetGame()) return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (!character || !control || !players || !character.IsPlayerControlled()) return false;
		int playerId = players.GetPlayerIdFromControlledEntity(actor);
		if (playerId <= 0 || SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(actor) != playerId) return false;
		PlayerController player = players.GetPlayerController(playerId);
		if (!players || !player || player.GetPlayerId() != playerId || player.GetControlledEntity() != actor) return false;
		if (!actor || !character || !control || !players || !player) return false;
		AIAgent agent = control.GetControlAIAgent();
		bool hadAgent = agent != null;
		EntityID agentId;
		if (agent)
		{
			agentId = agent.GetID();
			if (agentId == EntityID.INVALID || agent.GetWorld() != m_World || m_World.FindEntityByID(agentId) != agent) return false;
		}
		if (agent && (agent.GetControlledEntity() != actor || agent.IsAIActivated() || agent.CountTowardsAIWorldLimit())) return false;
		// An absent current agent cannot hide an old registered AI obligation.
		int matchingRows;
		for (int index; index < snapshot.Count(); index++)
		{
			AICF_AIQuotaAgentRow row = snapshot.GetRow(index);
			if (!row) return false;
			if (row.m_Entity != actor) continue;
			if (!agent || row.m_Agent != agent || row.m_AgentId != agentId || row.m_bCounted || row.m_bActivated) return false;
			matchingRows++;
		}
		if (matchingRows > 1) return false;
		if (!players || !player || !character || !control || !actor || (hadAgent && !agent)) return false;
		if (!MatchesHumanEntityIdentity() || GetGame().GetPlayerManager() != players) return false;
		if (!actor || !players || !player || !character || !control) return false;
		CharacterControllerComponent repeatedCharacter = actor.GetCharacterController();
		if (!actor || !character || !control) return false;
		AIControlComponent repeatedControl = actor.GetAIControlComponent();
		if (!character || !control || repeatedCharacter != character || repeatedControl != control) return false;
		if (!character.IsPlayerControlled() || players.GetPlayerIdFromControlledEntity(actor) != playerId ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(actor) != playerId) return false;
		if (!players || !player || !character || !control || !actor || (hadAgent && !agent)) return false;
		PlayerController repeatedPlayer = players.GetPlayerController(playerId);
		if (!players || !player || !repeatedPlayer || repeatedPlayer != player) return false;
		if (player.GetPlayerId() != playerId || player.GetControlledEntity() != actor) return false;
		if (control.GetControlAIAgent() != agent) return false;
		if (agent && (agent.GetID() != agentId || agent.GetWorld() != m_World || m_World.FindEntityByID(agentId) != agent)) return false;
		if (agent && (agent.GetControlledEntity() != actor || agent.IsAIActivated() || agent.CountTowardsAIWorldLimit())) return false;
		if (!MatchesHumanEntityIdentity()) return false;
		if (!players || !player || !character || !control || !actor || (hadAgent && !agent)) return false;
		if (m_Rpl != originalRpl || m_Id != originalId || m_sRpl != originalRplId || m_World != originalWorld) return false;
		return !m_bReleased && !m_bQuarantined && !m_bVehicle && m_Bucket == AICF_EAIQuotaBucket.STOCK && !m_GroupOwner && !m_CreationGuard && m_Entity == actor;
	}

	protected bool MatchesHumanEntityIdentity()
	{
		if (!Replication.IsServer() || !GetGame() || !m_Entity || !m_World || GetGame().GetWorld() != m_World) return false;
		if (m_Entity.GetWorld() != m_World || m_Entity.GetID() != m_Id || m_World.FindEntityByID(m_Id) != m_Entity) return false;
		return m_Rpl && m_Entity.FindComponent(RplComponent) == m_Rpl && m_Rpl.Id().IsValid() &&
			m_Rpl.Id().ToString() == m_sRpl && !m_sRpl.IsEmpty();
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
