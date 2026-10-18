// IsPlayable also defines native Conflict opponents/HQs. Keep that state intact;
// filter only the player's menu and independently validate authority requests.
modded class SCR_PlayerFactionAffiliationComponent
{
	override protected bool CanRequestFaction_S(Faction faction)
	{
		if (faction && !AICF_MatchRules.FromMissionHeader().AllowsPlayerFaction(faction.GetFactionKey())) return false;
		return super.CanRequestFaction_S(faction);
	}

	override bool SetFaction_S(Faction faction)
	{
		if (faction && !AICF_MatchRules.FromMissionHeader().AllowsPlayerFaction(faction.GetFactionKey())) return false;
		return super.SetFaction_S(faction);
	}
}
