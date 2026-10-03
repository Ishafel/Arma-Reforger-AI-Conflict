// Победа: тикеты противника исчерпаны ИЛИ все активные точки захвачены без оспаривания.
class AICF_VictorySystem
{
	protected bool m_bEnded;
	protected bool m_bMatchEndConfirmed;
	protected FactionKey m_sWinnerKey;

	bool EvaluateAndEnd(
		SCR_GameModeCampaign campaign,
		AICF_FactionState usState,
		AICF_FactionState ussrState,
		AICF_ObjectiveGraph graph)
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

		bool usWins = ussrState.GetTickets() <= 0 || ControlsAllObjectives(graph, usState.GetFactionKey());
		bool ussrWins = usState.GetTickets() <= 0 || ControlsAllObjectives(graph, ussrState.GetFactionKey());
		if (usWins == ussrWins)
			return false;

		AICF_FactionState winnerState = usState;
		AICF_FactionState loserState = ussrState;
		if (ussrWins)
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
		string reason = "ALL_OBJECTIVES_UNCONTESTED";
		if (loserState.GetTickets() <= 0)
			reason = "ENEMY_TICKETS_EXHAUSTED";
		AICF_Stage1Diagnostics.Info(
			"VICTORY",
			string.Format(
				"winner=%1 loser=%2 loser_tickets=%3 reason=%4",
				m_sWinnerKey,
				loserState.GetFactionKey(),
				loserState.GetTickets(),
				reason));

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

	protected bool ControlsAllObjectives(AICF_ObjectiveGraph graph, FactionKey factionKey)
	{
		if (!graph || graph.GetRevision() <= 0 || factionKey.IsEmpty())
			return false;

		int objectives;
		for (int nodeId = 0; nodeId < graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = graph.GetNode(nodeId);
			if (!node)
				return false;
			if (!node.IsObjective())
				continue;
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			if (!base || !base.GetOwner() || !base.IsInitialized())
				return false;
			// Stock HQ не захватываются; RELAY уже исключены через IsObjective().
			if (base.IsHQ())
				continue;
			objectives++;
			if (!base.GetFaction() || base.GetFaction().GetFactionKey() != factionKey ||
				base.GetCaptureState() != SCR_EBaseCaptureState.NONE ||
				base.IsBeingCaptured() || base.AreEnemiesPresent())
				return false;
		}

		return objectives > 0;
	}
}
