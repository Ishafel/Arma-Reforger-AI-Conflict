// Native barracks can sell Guard characters whose prefab forbids recruitment.
// Only a human-requested group on the Co-op player side receives this permission.
modded class SCR_GameModeCampaign
{
	override void OnEntityRequested(notnull IEntity spawnedEntity, IEntity user, SCR_Faction faction, SCR_MilitaryBaseLogicComponent service)
	{
		super.OnEntityRequested(spawnedEntity, user, faction, service);
		if (!Replication.IsServer() || !IsMaster() || !user) return;
		SCR_AIGroup group = SCR_AIGroup.Cast(spawnedEntity);
		if (!group) return;
		// Native GetEntityFaction reads an affiliation component, which groups lack.
		// Resolve their faction through SCR_AIGroup, retaining a non-null mismatch fence.
		Faction requestedFaction = group.GetFaction();
		if (!requestedFaction || (faction && faction != requestedFaction)) return;
		AICF_MatchRules rules = AICF_MatchRules.FromMissionHeader();
		if (rules.GetMode() == AICF_EMatchMode.STANDARD || !rules.AllowsPlayerFaction(requestedFaction.GetFactionKey())) return;
		SCR_CampaignBuildingProviderComponent provider = SCR_CampaignBuildingProviderComponent.Cast(service);
		if (!provider || !provider.CanCommandAI()) return;
		PlayerManager players = GetGame().GetPlayerManager();
		int playerId = players.GetPlayerIdFromControlledEntity(user);
		if (playerId <= 0 || players.GetPlayerControlledEntity(playerId) != user) return;
		SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factions || factions.GetPlayerFaction(playerId) != requestedFaction || SCR_Faction.GetEntityFaction(user) != requestedFaction) return;
		group.AICF_AllowPurchasedRecruitment(requestedFaction);
	}
}

modded class SCR_AIGroup
{
	protected string m_sAICFPurchasedRecruitFaction;

	void AICF_AllowPurchasedRecruitment(Faction faction)
	{
		if (!Replication.IsServer() || !faction || GetFaction() != faction) return;
		AICF_MatchRules rules = AICF_MatchRules.FromMissionHeader();
		if (rules.GetMode() == AICF_EMatchMode.STANDARD || !rules.AllowsPlayerFaction(faction.GetFactionKey())) return;
		m_sAICFPurchasedRecruitFaction = faction.GetFactionKey();
		array<AIAgent> agents = {};
		GetAgents(agents);
		foreach (AIAgent agent : agents) AICF_AllowPurchasedMember(agent);
	}

	override void OnAgentAdded(AIAgent child)
	{
		super.OnAgentAdded(child);
		// RequestSpawn fills the paid group over several frames.
		AICF_AllowPurchasedMember(child);
	}

	protected void AICF_AllowPurchasedMember(AIAgent agent)
	{
		if (!Replication.IsServer() || m_sAICFPurchasedRecruitFaction.IsEmpty() || !agent || agent.GetParentGroup() != this) return;
		SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.IsMaster()) return;
		AICF_MatchRules rules = AICF_MatchRules.FromMissionHeader();
		if (rules.GetMode() == AICF_EMatchMode.STANDARD || !rules.AllowsPlayerFaction(m_sAICFPurchasedRecruitFaction)) return;
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(m_sAICFPurchasedRecruitFaction);
		if (!faction || GetFaction() != faction) return;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
		if (!character || character.GetFaction() != faction || !AICF_GroupRuntime.IsAliveCharacter(character) ||
			GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) != 0) return;
		// SetRecruitable already bumps replication. Avoid a bump for unchanged flags.
		if (!character.IsRecruitable()) character.SetRecruitable(true);
	}
}
