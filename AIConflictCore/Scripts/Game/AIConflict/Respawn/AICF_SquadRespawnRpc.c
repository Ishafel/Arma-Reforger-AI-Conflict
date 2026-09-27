modded class SCR_PlayerController
{
	protected bool m_bAICFSquadDeath;
	protected int m_iAICFSquadDeathRevision;
	protected int m_iAICFSquadListAt;
	protected int m_iAICFListedDeath = -1;
	protected int m_iAICFListedCount = -1;
	protected ref AICF_SquadRespawnAttempt m_AICFSquadRespawn;
	ref array<RplId> m_aAICFSquadRespawnIds = {};
	ref array<string> m_aAICFSquadRespawnNames = {};
	ref array<ResourceName> m_aAICFSquadRespawnPrefabs = {};
	int m_iAICFSquadListRevision;
	int m_iAICFSquadListDeathRevision;
	string m_sAICFSquadRespawnResult;

	void AICF_RecordSquadDeath()
	{
		if (!Replication.IsServer()) return;
		m_bAICFSquadDeath = true;
		m_iAICFSquadDeathRevision++;
	}

	void AICF_ClearSquadDeath()
	{
		if (Replication.IsServer()) m_bAICFSquadDeath = false;
	}

	int AICF_GetSquadDeathRevision() { return m_iAICFSquadDeathRevision; }
	AICF_SquadRespawnAttempt AICF_GetSquadRespawnAttempt() { return m_AICFSquadRespawn; }

	bool AICF_CanChooseSquadRespawn()
	{
		return m_bAICFSquadDeath && !AICF_GroupRuntime.IsAliveCharacter(GetControlledEntity());
	}

	void AICF_RequestSquadRespawnList() { Rpc(RpcAsk_AICFSquadRespawnList); }
	void AICF_RequestSquadRespawn(RplId character, int deathRevision)
	{
		m_sAICFSquadRespawnResult = "PENDING";
		Rpc(RpcAsk_AICFSquadRespawn, character, deathRevision);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFSquadRespawnList()
	{
		if (!Replication.IsServer() || System.GetTickCount() < m_iAICFSquadListAt) return;
		m_iAICFSquadListAt = System.GetTickCount() + 1000;
		array<RplId> ids = {};
		array<string> names = {};
		array<ResourceName> prefabs = {};
		SCR_AIGroup group = AICF_SquadRespawnPolicy.PlayerGroup(this);
		if (group)
		{
			array<AIAgent> agents = {};
			AICF_SquadRespawnPolicy.GetSquadAgents(group, agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent) continue;
				IEntity entity = agent.GetControlledEntity();
				if (!AICF_SquadRespawnPolicy.CanTake(this, group, entity)) continue;
				RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
				if (!rpl || !rpl.Id().IsValid()) continue;
				string label;
				CharacterIdentityComponent identity = CharacterIdentityComponent.Cast(entity.FindComponent(CharacterIdentityComponent));
				if (identity && identity.GetIdentity())
					label = identity.GetIdentity().GetName() + " " + identity.GetIdentity().GetSurname();
				ids.Insert(rpl.Id());
				names.Insert(label);
				prefabs.Insert(entity.GetPrefabData().GetPrefabName());
				if (ids.Count() >= 32) break;
			}
		}
		if (m_bAICFSquadDeath && (m_iAICFListedDeath != m_iAICFSquadDeathRevision || m_iAICFListedCount != ids.Count()))
		{
			m_iAICFListedDeath = m_iAICFSquadDeathRevision;
			m_iAICFListedCount = ids.Count();
			AICF_Stage4Diagnostics.Info("SQUAD_RESPAWN_LIST", string.Format("player=%1 death_revision=%2 group=%3 eligible=%4 authority=SERVER", GetPlayerId(), m_iAICFSquadDeathRevision, group, ids.Count()));
		}
		Rpc(RpcDo_AICFSquadRespawnList, ids, names, prefabs, m_iAICFSquadDeathRevision);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFSquadRespawnList(array<RplId> ids, array<string> names, array<ResourceName> prefabs, int deathRevision)
	{
		m_aAICFSquadRespawnIds.Copy(ids);
		m_aAICFSquadRespawnNames.Copy(names);
		m_aAICFSquadRespawnPrefabs.Copy(prefabs);
		m_iAICFSquadListDeathRevision = deathRevision;
		m_iAICFSquadListRevision++;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFSquadRespawn(RplId character, int deathRevision)
	{
		if (!Replication.IsServer()) return;
		if (m_AICFSquadRespawn)
		{
			Rpc(RpcDo_AICFSquadRespawnResult, "PENDING");
			return;
		}
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(character));
		SCR_AIGroup group = AICF_SquadRespawnPolicy.PlayerGroup(this);
		IEntity entity;
		if (rpl) entity = rpl.GetEntity();
		if (deathRevision != m_iAICFSquadDeathRevision || !AICF_SquadRespawnPolicy.CanTake(this, group, entity))
		{
			Rpc(RpcDo_AICFSquadRespawnResult, "UNAVAILABLE");
			return;
		}
		SCR_RespawnComponent respawn = SCR_RespawnComponent.Cast(GetRespawnComponent());
		if (!respawn || !FindComponent(SCR_PossessSpawnRequestComponent))
		{
			Rpc(RpcDo_AICFSquadRespawnResult, "UNAVAILABLE");
			return;
		}
		m_AICFSquadRespawn = new AICF_SquadRespawnAttempt();
		m_AICFSquadRespawn.m_Character = entity;
		m_AICFSquadRespawn.m_CharacterId = entity.GetID();
		m_AICFSquadRespawn.m_RplId = character;
		m_AICFSquadRespawn.m_Group = group;
		m_AICFSquadRespawn.m_GroupId = group.GetID();
		m_AICFSquadRespawn.m_MemberGroup = AICF_SquadRespawnPolicy.MemberGroup(group, entity);
		m_AICFSquadRespawn.m_MemberGroupId = m_AICFSquadRespawn.m_MemberGroup.GetID();
		m_AICFSquadRespawn.m_iDeathRevision = deathRevision;
		AICF_Stage4Diagnostics.Info("SQUAD_RESPAWN_REQUEST", string.Format("player=%1 character=%2 group=%3 death_revision=%4 authority=SERVER", GetPlayerId(), character, group.GetID(), deathRevision));
		if (!respawn.RequestSpawn(SCR_PossessSpawnData.FromRplId(character)))
			AICF_FinishSquadRespawn(SCR_ESpawnResult.CANNOT_POSSES);
	}

	void AICF_FinishSquadRespawn(SCR_ESpawnResult result)
	{
		if (!Replication.IsServer() || !m_AICFSquadRespawn) return;
		AICF_Stage4Diagnostics.Info("SQUAD_RESPAWN_RESULT", string.Format("player=%1 character=%2 result=%3 authority=SERVER", GetPlayerId(), m_AICFSquadRespawn.m_RplId, typename.EnumToString(SCR_ESpawnResult, result)));
		m_AICFSquadRespawn = null;
		string reason = "UNAVAILABLE";
		if (result == SCR_ESpawnResult.OK) reason = "OK";
		if (result == SCR_ESpawnResult.NOT_ALLOWED_TIMER) reason = "TIMER";
		Rpc(RpcDo_AICFSquadRespawnResult, reason);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFSquadRespawnResult(string result) { m_sAICFSquadRespawnResult = result; }
}
