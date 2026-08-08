// Ends a Stage 1 match once exactly one managed faction has exhausted tickets and combat forces.
class AICF_VictorySystem
{
	protected bool m_bEnded;
	protected bool m_bMatchEndConfirmed;
	protected FactionKey m_sWinnerKey;

	bool EvaluateAndEnd(
		SCR_GameModeCampaign campaign,
		AICF_FactionState usState,
		AICF_FactionState ussrState)
	{
		if (m_bEnded)
			return false;

		if (!Replication.IsServer() || !campaign || !campaign.IsMaster() || !campaign.IsRunning())
			return false;

		if (!usState || !ussrState)
		{
			AICF_Stage1Diagnostics.Error("VICTORY_STATE_MISSING", "Both US and USSR faction states are required");
			return false;
		}

		bool usDefeated = IsDefeated(usState);
		bool ussrDefeated = IsDefeated(ussrState);
		if (usDefeated == ussrDefeated)
			return false;

		AICF_FactionState winnerState = usState;
		AICF_FactionState loserState = ussrState;
		if (usDefeated)
		{
			winnerState = ussrState;
			loserState = usState;
		}

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
		{
			AICF_Stage1Diagnostics.Error("VICTORY_FACTION_MANAGER_MISSING", "FactionManager is unavailable");
			return false;
		}

		Faction winnerFaction = factionManager.GetFactionByKey(winnerState.GetFactionKey());
		if (!winnerFaction)
		{
			AICF_Stage1Diagnostics.Error(
				"VICTORY_FACTION_MISSING",
				string.Format("winner=%1", winnerState.GetFactionKey()));
			return false;
		}

		int factionIndex = factionManager.GetFactionIndex(winnerFaction);
		if (factionIndex < 0)
		{
			AICF_Stage1Diagnostics.Error(
				"VICTORY_FACTION_INDEX_INVALID",
				string.Format("winner=%1 faction_index=%2", winnerState.GetFactionKey(), factionIndex));
			return false;
		}

		m_bEnded = true;
		m_sWinnerKey = winnerState.GetFactionKey();
		AICF_Stage1Diagnostics.Info(
			"VICTORY",
			string.Format(
				"winner=%1 loser=%2 loser_tickets=%3 reason=REPLACEMENT_UNAFFORDABLE_NO_COMBAT_GROUPS",
				m_sWinnerKey,
				loserState.GetFactionKey(),
				loserState.GetTickets()));

		campaign.EndGameMode(SCR_GameModeEndData.CreateSimple(
			EGameOverTypes.ENDREASON_SCORELIMIT,
			winnerFactionId: factionIndex));

		return true;
	}

	bool ConfirmMatchEnd(SCR_GameModeCampaign campaign)
	{
		if (!m_bEnded || m_bMatchEndConfirmed || !campaign || campaign.IsRunning())
			return false;

		m_bMatchEndConfirmed = true;
		AICF_Stage1Diagnostics.Info("MATCH_END", string.Format("winner=%1", m_sWinnerKey));
		return true;
	}

	bool IsEnded()
	{
		return m_bEnded;
	}

	FactionKey GetWinnerKey()
	{
		return m_sWinnerKey;
	}

	protected bool IsDefeated(AICF_FactionState factionState)
	{
		if (factionState.CanAffordDeployment(AICF_EDeploymentKind.REPLACEMENT))
			return false;

		return !HasManagedCombatGroups(factionState);
	}

	protected bool HasManagedCombatGroups(AICF_FactionState factionState)
	{
		for (int slotId = 0; slotId < factionState.GetSlotCount(); slotId++)
		{
			AICF_GroupSlot slot = factionState.GetSlot(slotId);
			if (!slot)
				continue;

			AICF_EGroupSlotState state = slot.GetState();
			// SPAWNING also covers the bounded interval before a group is bound. Its reservation
			// must not make the faction look exhausted before the spawn watchdog resolves it.
			if (state == AICF_EGroupSlotState.SPAWNING)
				return true;
			if (state == AICF_EGroupSlotState.READY && slot.GetGroup())
				return true;
		}

		return false;
	}
}
