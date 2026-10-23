// The native deployment UI normally joins a player group. Co-op must also
// establish that membership when the player deploys without opening that panel.
modded class SCR_GroupsManagerComponent
{
	override bool IsGroupMenuAllowed()
	{
		if (AICF_MatchRules.FromMissionHeader().GetMode() != AICF_EMatchMode.STANDARD) return true;
		return super.IsGroupMenuAllowed();
	}

	override bool IsGroupLeaderVolunteeringAllowed()
	{
		if (AICF_MatchRules.FromMissionHeader().GetMode() != AICF_EMatchMode.STANDARD) return true;
		return super.IsGroupLeaderVolunteeringAllowed();
	}

	override void OnPlayerFactionChanged(notnull FactionAffiliationComponent owner, Faction previousFaction, Faction newFaction)
	{
		super.OnPlayerFactionChanged(owner, previousFaction, newFaction);
		AICF_MatchRules rules = AICF_MatchRules.FromMissionHeader();
		if (!Replication.IsServer() || !GetGameMode() || !GetGameMode().IsMaster() ||
			rules.GetMode() == AICF_EMatchMode.STANDARD || !newFaction ||
			!rules.AllowsPlayerFaction(newFaction.GetFactionKey())) return;
		// Native faction listeners reset group IDs and attempt reconnect first.
		GetGame().GetCallqueue().CallLater(AICF_EnsureCoopPlayerGroup, 0, false, owner, newFaction);
	}

	protected void AICF_EnsureCoopPlayerGroup(FactionAffiliationComponent owner, Faction faction)
	{
		if (!Replication.IsServer() || !GetGameMode() || !GetGameMode().IsMaster() || !owner || !faction) return;
		AICF_MatchRules rules = AICF_MatchRules.FromMissionHeader();
		if (rules.GetMode() == AICF_EMatchMode.STANDARD || !rules.AllowsPlayerFaction(faction.GetFactionKey()) ||
			owner.GetAffiliatedFaction() != faction) return;
		SCR_PlayerController player = SCR_PlayerController.Cast(owner.GetOwner());
		SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!player || player.GetPlayerId() <= 0 ||
			GetGame().GetPlayerManager().GetPlayerController(player.GetPlayerId()) != player ||
			!factions || factions.GetPlayerFaction(player.GetPlayerId()) != faction) return;
		// Preserve the player's existing group and its elected leader.
		SCR_AIGroup group = GetPlayerGroup(player.GetPlayerId());
		if (group && group.GetFaction() == faction && group.IsPlayerInGroup(player.GetPlayerId())) return;
		SCR_PlayerControllerGroupComponent component = SCR_PlayerControllerGroupComponent.Cast(player.FindComponent(SCR_PlayerControllerGroupComponent));
		if (component) component.CreateAndJoinGroup(faction, true);
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_EnsureCoopPlayerGroup);
		super.OnGameEnd();
	}
}
