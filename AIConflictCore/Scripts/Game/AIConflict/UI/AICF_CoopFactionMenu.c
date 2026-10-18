modded class SCR_FactionRequestUIComponent
{
	protected void AICF_FilterCoopFactions()
	{
		AICF_MatchRules rules = AICF_MatchRules.FromMissionHeader();
		if (rules.GetMode() == AICF_EMatchMode.STANDARD) return;
		foreach (SCR_DeployButtonBase button : m_aButtons)
		{
			SCR_FactionButton factionButton = SCR_FactionButton.Cast(button);
			if (!factionButton || !factionButton.GetFaction() || rules.AllowsPlayerFaction(factionButton.GetFaction().GetFactionKey())) continue;
			factionButton.SetVisible(false, false);
			factionButton.SetEnabled(false);
			factionButton.SetShouldUnlock(false);
			m_aActiveFactionButtons.RemoveItem(factionButton);
		}
		if (m_wNoFactions) m_wNoFactions.SetVisible(m_aActiveFactionButtons.IsEmpty());
	}

	override void ShowAvailableFactions()
	{
		super.ShowAvailableFactions();
		AICF_FilterCoopFactions();
	}

	override void RequestRandomFaction()
	{
		if (AICF_MatchRules.FromMissionHeader().GetMode() == AICF_EMatchMode.STANDARD)
		{
			super.RequestRandomFaction();
			return;
		}
		SCR_FactionButton button = GetFirstValidFactionButton();
		if (button) RequestPlayerFaction(button);
	}

	override protected void OnPlayableFactionChanged(SCR_Faction faction, bool playable)
	{
		super.OnPlayableFactionChanged(faction, playable);
		AICF_FilterCoopFactions();
	}

	override protected void UpdateFactionButtons(Faction faction, int newCount)
	{
		super.UpdateFactionButtons(faction, newCount);
		AICF_FilterCoopFactions();
	}
}
