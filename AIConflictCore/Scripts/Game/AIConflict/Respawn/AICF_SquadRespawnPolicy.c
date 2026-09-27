// Только существующий AI из текущей player group. Никакого spawn, оплаты
// или изменения состава: передачей персонажа владеет штатный respawn pipeline.
class AICF_SquadRespawnAttempt
{
	IEntity m_Character;
	EntityID m_CharacterId;
	RplId m_RplId;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	SCR_AIGroup m_MemberGroup;
	EntityID m_MemberGroupId;
	int m_iDeathRevision;
}

class AICF_SquadRespawnPolicy
{
	static bool IsActive()
	{
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
		return Replication.IsServer() && AICF_MatchController.GetActiveController() &&
			campaign && campaign.IsMaster() && campaign.IsRunning();
	}

	static SCR_AIGroup PlayerGroup(SCR_PlayerController player)
	{
		if (!IsActive() || !player || player.GetPlayerId() <= 0 || !player.AICF_CanChooseSquadRespawn())
			return null;
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!groups)
			return null;
		SCR_AIGroup group = groups.GetPlayerGroup(player.GetPlayerId());
		if (!group || !group.IsPlayerInGroup(player.GetPlayerId()) ||
			group.GetFaction() != SCR_FactionManager.SGetPlayerFaction(player.GetPlayerId()))
			return null;
		return group;
	}

	static SCR_AIGroup SlaveGroup(SCR_AIGroup group)
	{
		if (!group) return null;
		SCR_AIGroup slave = group.GetSlave();
		if (!slave || slave == group || slave.GetMaster() != group || slave.GetFaction() != group.GetFaction()) return null;
		return slave;
	}

	static void GetSquadAgents(SCR_AIGroup group, out array<AIAgent> agents)
	{
		agents.Clear();
		if (!group) return;
		group.GetAgents(agents);
		SCR_AIGroup slave = SlaveGroup(group);
		if (!slave) return;
		array<AIAgent> recruits = {};
		slave.GetAgents(recruits);
		foreach (AIAgent agent : recruits)
		{
			if (agent && !agents.Contains(agent)) agents.Insert(agent);
		}
	}

	static SCR_AIGroup MemberGroup(SCR_AIGroup group, IEntity entity)
	{
		if (!group || !entity) return null;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetControlledEntity() == entity) return group;
		}
		SCR_AIGroup slave = SlaveGroup(group);
		if (!slave) return null;
		agents.Clear();
		slave.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetControlledEntity() == entity) return slave;
		}
		return null;
	}

	static bool CanTake(SCR_PlayerController player, SCR_AIGroup group, IEntity entity)
	{
		if (!group || PlayerGroup(player) != group || !entity ||
			!AICF_GroupRuntime.IsAliveCharacter(entity) || SCR_Faction.GetEntityFaction(entity) != group.GetFaction())
			return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (players.GetPlayerIdFromControlledEntity(entity) != 0 || SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(entity) != 0)
			return false;
		return MemberGroup(group, entity) != null;
	}

	static bool IsCurrent(SCR_PlayerController player, AICF_SquadRespawnAttempt attempt, IEntity entity)
	{
		return attempt && entity && entity == attempt.m_Character && entity.GetID() == attempt.m_CharacterId &&
			attempt.m_Group && attempt.m_Group.GetID() == attempt.m_GroupId &&
			attempt.m_MemberGroup && attempt.m_MemberGroup.GetID() == attempt.m_MemberGroupId &&
			MemberGroup(attempt.m_Group, entity) == attempt.m_MemberGroup &&
			player.AICF_GetSquadDeathRevision() == attempt.m_iDeathRevision &&
			CanTake(player, attempt.m_Group, entity);
	}
}

// Штатный callback смерти: initial join не открывает выбор чужого персонажа.
[BaseContainerProps(category: "Respawn")]
modded class SCR_SpawnLogic
{
	override void OnPlayerKilled_S(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (AICF_SquadRespawnPolicy.IsActive() && player && playerEntity && !AICF_GroupRuntime.IsAliveCharacter(playerEntity))
			player.AICF_RecordSquadDeath();
		super.OnPlayerKilled_S(playerId, playerEntity, killerEntity, killer);
	}

	override void OnPlayerSpawned_S(int playerId, IEntity entity)
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (Replication.IsServer() && player)
			player.AICF_ClearSquadDeath();
		super.OnPlayerSpawned_S(playerId, entity);
	}
}
