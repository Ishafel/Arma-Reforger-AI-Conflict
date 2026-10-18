// Isolated stage only. Observes production bootstrap/roster and replicated rules.
modded class AICF_MatchController
{
	protected int m_iCoopProbeStarted;
	protected bool m_bCoopProbeFailed;
	protected int m_iCoopProbeChecks;
	protected bool m_bCoopProbeDone;

	void AICF_CoopCheck(string name, bool passed)
	{
		m_iCoopProbeChecks++;
		if (!passed) m_bCoopProbeFailed = true;
		Print(string.Format("[AICF][COOP_CASE] case=%1 pass=%2", name, passed));
	}

	override protected void Update()
	{
		super.Update();
		string mode;
		if (!System.GetCLIParam("aicfCoopProbe", mode) || !m_bRosterReady || m_bStopped || m_bCoopProbeDone) return;
		int expectedUSSR = 10;
		if (mode == "USSR") expectedUSSR = 0;
		int now = System.GetTickCount();
		if (!m_iCoopProbeStarted)
		{
			m_iCoopProbeStarted = now;
			AICF_CoopCheck("US_ROSTER", m_USState.GetSlotCount() == 10 && m_USState.CountSlotsByState(AICF_EGroupSlotState.READY) == 10);
			AICF_CoopCheck("USSR_ROSTER", m_USSRState.GetSlotCount() == expectedUSSR);
			AICF_CoopCheck("QUEUE_SETTLED", m_USState.IsInitialSpawnQueueSettled() && m_USSRState.IsInitialSpawnQueueSettled());
			AICF_CoopCheck("US_TICKETS", m_USState.UsesTicketDefeat());
			AICF_CoopCheck("USSR_TICKETS", m_USSRState.UsesTicketDefeat() == (expectedUSSR > 0));
			AICF_CoopCheck("COMMANDERS", m_USAICommander && ((expectedUSSR > 0) == (m_USSRAICommander != null)));
			AICF_CoopCheck("HELPERS", m_BaseBuilders && m_Logistics && m_Construction && m_EconomySystem && m_VehicleCoordinator);
			AICF_CoopCheck("FACTION_ACCESS", m_USFaction.IsPlayable() && m_USSRFaction.IsPlayable() && m_Config.GetMatchRules().AllowsPlayerFaction("US") == (expectedUSSR > 0) && m_Config.GetMatchRules().AllowsPlayerFaction("USSR") && m_Config.GetMatchRules().AllowsPlayerFaction("FIA") == (expectedUSSR > 0));
			AICF_CoopCheck("AUTO_CONSTRUCTION", m_Construction.AICF_CoopProbeAutomatic(m_USFaction) && m_Construction.AICF_CoopProbeAutomatic(m_USSRFaction) == (expectedUSSR > 0));
			AICF_Stage1Config budget = new AICF_Stage1Config();
			budget.SetMaxManagedAgents(1);
			AICF_CoopCheck("BUDGET_FLOOR", budget.GetMaxManagedAgents() == (10 + expectedUSSR) * 10);
			AICF_MatchRules standard = new AICF_MatchRules();
			AICF_CoopCheck("STANDARD_POLICY", standard.GetManagedSlotCount("US") == 10 && standard.GetManagedSlotCount("USSR") == 10 && standard.UsesTicketDefeat("USSR"));
			AICF_MatchRules mirror = new AICF_MatchRules(AICF_EMatchMode.COOP_US);
			AICF_CoopCheck("MIRROR_POLICY", mirror.GetManagedSlotCount("US") == 0 && mirror.GetManagedSlotCount("USSR") == 10);
			if (expectedUSSR == 0)
			{
				budget.SetInitialTickets(1);
				AICF_FactionState liveUS = new AICF_FactionState("US", budget);
				budget.SetInitialTickets(0);
				AICF_FactionState zeroUSSR = new AICF_FactionState("USSR", budget);
				AICF_VictorySystem victory = new AICF_VictorySystem();
				AICF_CoopCheck("ZERO_PLAYER_TICKETS_NO_DEFEAT", !victory.EvaluateAndEnd(m_Campaign, liveUS, zeroUSSR, null) && !victory.IsEnded());
				budget.SetAICommanderMode("BOTH");
				AICF_CoopCheck("CONFLICT_REJECTED", !budget.IsAICommanderModeValid());
			}
		}
		int duration = 90000;
		string durationText;
		if (System.GetCLIParam("aicfCoopProbeDurationMs", durationText)) duration = durationText.ToInt();
		if (now - m_iCoopProbeStarted < duration) return;
		AICF_CoopCheck("STABLE_ROSTER", m_USSRState.GetSlotCount() == expectedUSSR && CountFactionAgents(m_USSRState) >= expectedUSSR);
		AICF_CoopCheck("REPLICATED_MODE", m_Campaign.AICF_IsPlayerOnlyFaction(true) == (expectedUSSR == 0));
		if (expectedUSSR == 0)
			AICF_CoopCheck("NO_GROUP_SUMMARY", BuildGroupSummary(m_USSRState, 0).IsEmpty());
		m_bCoopProbeDone = true;
		string winnerKey;
		if (System.GetCLIParam("aicfCoopProbeWinner", winnerKey))
		{
			SCR_CampaignFaction winner = m_USFaction;
			if (winnerKey == "USSR") winner = m_USSRFaction;
			for (int nodeId; nodeId < m_ObjectiveGraph.GetNodeCount(); nodeId++)
			{
				AICF_ObjectiveNode node = m_ObjectiveGraph.GetNode(nodeId);
				if (node && node.IsObjective() && node.GetBase() && !node.GetBase().IsHQ())
					node.GetBase().SetFaction(winner);
			}
			AICF_CoopCheck("TERRITORIAL_WINNER", m_VictorySystem.EvaluateAndEnd(m_Campaign, m_USState, m_USSRState, m_ObjectiveGraph) && m_VictorySystem.GetWinnerKey() == winner.GetFactionKey());
		}
		Print(string.Format("[AICF][COOP_DONE] mode=%1 checks=%2 failed=%3 elapsed_ms=%4", mode, m_iCoopProbeChecks, m_bCoopProbeFailed, now - m_iCoopProbeStarted));
		// This fixture proves scenario contracts, not the full Stage1 battle matrix.
		// Use its ordinary explicit cleanup, as VictoryRespawnProbe does.
		if (!winnerKey.IsEmpty()) Stop(true);
		m_Campaign.AICF_CoopProbeCloseSoon();
	}
}

modded class SCR_GameModeCampaign
{
	protected int m_iCoopClientSamples;
	protected bool m_bCoopClientFinished;
	protected SCR_PlayerFactionAffiliationComponent m_AICFCoopAffiliation;
	protected int m_iAICFCoopDenied;
	protected bool m_bAICFCoopRequestPending;
	protected bool m_bAICFCoopAccessFailed;

	protected void AICF_CoopFactionResponse(SCR_PlayerFactionAffiliationComponent component, int factionIndex, bool response)
	{
		Faction faction = GetGame().GetFactionManager().GetFactionByIndex(factionIndex);
		string expected = "US";
		if (m_iAICFCoopDenied == 1) expected = "FIA";
		bool passed = faction && faction.GetFactionKey() == expected && !response && !component.GetAffiliatedFaction();
		Print(string.Format("[AICF][COOP_CLIENT_FACTION] faction=%1 accepted=%2 pass=%3", expected, response, passed));
		if (!passed) m_bAICFCoopAccessFailed = true;
		m_bAICFCoopRequestPending = false;
		m_iAICFCoopDenied++;
		if (m_iAICFCoopDenied == 2) component.GetOnPlayerFactionResponseInvoker_O().Remove(AICF_CoopFactionResponse);
	}

	protected bool AICF_CoopClientAccess()
	{
		if (m_iAICFCoopDenied == 2) return !m_bAICFCoopAccessFailed;
		PlayerController player = GetGame().GetPlayerController();
		if (!player) return false;
		if (!m_AICFCoopAffiliation)
		{
			m_AICFCoopAffiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
			if (!m_AICFCoopAffiliation) return false;
			m_AICFCoopAffiliation.GetOnPlayerFactionResponseInvoker_O().Insert(AICF_CoopFactionResponse);
		}
		if (m_bAICFCoopRequestPending) return false;
		string key = "US";
		if (m_iAICFCoopDenied == 1) key = "FIA";
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(key);
		if (!faction) return false;
		m_bAICFCoopRequestPending = true;
		if (!m_AICFCoopAffiliation.RequestFaction(faction)) m_bAICFCoopRequestPending = false;
		return false;
	}

	void AICF_CoopProbeCloseSoon()
	{
		// Allow native campaign end and inventory actions to finish before teardown.
		GetGame().GetCallqueue().CallLater(AICF_CoopProbeClose, 5000, false);
	}

	protected void AICF_CoopProbeClose()
	{
		GetGame().RequestClose();
	}

	override void OnGameStart()
	{
		super.OnGameStart();
		string mode;
		if (Replication.IsServer() && System.GetCLIParam("aicfCoopProbe", mode))
			GetGame().GetCallqueue().CallLater(AICF_CoopProbeDeadline, 240000, false);
		if (!Replication.IsServer() && System.GetCLIParam("aicfCoopProbe", mode))
			GetGame().GetCallqueue().CallLater(AICF_CoopClientReadback, 1000, true);
	}

	protected void AICF_CoopProbeDeadline()
	{
		Print("[AICF][COOP_TIMEOUT] bootstrap or observation did not complete");
		GetGame().RequestClose();
	}

	protected void AICF_CoopClientReadback()
	{
		m_iCoopClientSamples++;
		string playerFlag;
		bool requirePlayer = System.GetCLIParam("aicfCoopProbePlayer", playerFlag) && playerFlag == "1";
		bool playerReady = !requirePlayer;
		if (requirePlayer && m_iCoopClientSamples % 5 == 0 && AICF_CoopClientAccess()) playerReady = AICF_CoopClientSpawn();
		if (AICF_HasAICommanderState() && m_iCoopClientSamples >= 15 && playerReady)
		{
			bool passed = AICF_IsPlayerOnlyFaction(true) && !AICF_IsPlayerOnlyFaction(false) &&
				AICF_GetAICommanderEnabled(false) && !AICF_GetAICommanderEnabled(true) &&
				AICF_GetCombatGroups(true) == 0 && AICF_GetManagedAgents(true) == 0 && AICF_GetStrategicGroupSummary(true, 0).IsEmpty();
			Print(string.Format("[AICF][COOP_CLIENT_DONE] pass=%1 samples=%2", passed, m_iCoopClientSamples));
			m_bCoopClientFinished = true;
		}
		if (m_bCoopClientFinished || m_iCoopClientSamples > 90)
		{
			GetGame().GetCallqueue().Remove(AICF_CoopClientReadback);
			GetGame().RequestClose();
		}
	}

	// Native faction/respawn intents only; no UI input or server validation bypass.
	protected bool AICF_CoopClientSpawn()
	{
		PlayerController player = GetGame().GetPlayerController();
		string factionKey = "USSR";
		string testFaction;
		if (System.GetCLIParam("aicfBarracksFaction", testFaction)) factionKey = testFaction;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(factionKey));
		if (!player || !faction || !faction.GetMainBase()) return false;
		if (player.GetControlledEntity())
		{
			bool correct = SCR_Faction.GetEntityFaction(player.GetControlledEntity()) == faction;
			Print(string.Format("[AICF][COOP_CLIENT_PLAYER] pass=%1 entity=%2", correct, Replication.FindItemId(player.GetControlledEntity())));
			return correct;
		}
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation) return false;
		if (affiliation.GetAffiliatedFaction() != faction)
		{
			affiliation.RequestFaction(faction);
			return false;
		}
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		if (!catalog) return false;
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		array<string> suffixes = {};
		string role;
		if (!AICF_ContentProfile.GetActive().BuildCharacterRoleCandidates(factionKey, 9, role, suffixes)) return false;
		ResourceName prefab;
		foreach (string suffix : suffixes)
		{
			foreach (SCR_EntityCatalogEntry entry : entries)
				if (entry && entry.GetPrefab().EndsWith(suffix)) { prefab = entry.GetPrefab(); break; }
			if (!prefab.IsEmpty()) break;
		}
		if (prefab.IsEmpty()) return false;
		SCR_SpawnPoint chosen;
		float nearest = float.MAX;
		foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPointsForFaction(factionKey))
		{
			if (!point || !point.IsSpawnPointActive()) continue;
			float distance = vector.DistanceXZ(point.GetOrigin(), faction.GetMainBase().GetOwner().GetOrigin());
			if (distance >= nearest) continue;
			nearest = distance;
			chosen = point;
		}
		SCR_RespawnComponent respawn = SCR_RespawnComponent.Cast(player.GetRespawnComponent());
		if (respawn && chosen)
			respawn.RequestSpawn(new SCR_SpawnPointSpawnData(prefab, chosen.GetRplId()));
		return false;
	}

	override void OnGameEnd()
	{
		if (m_AICFCoopAffiliation) m_AICFCoopAffiliation.GetOnPlayerFactionResponseInvoker_O().Remove(AICF_CoopFactionResponse);
		GetGame().GetCallqueue().Remove(AICF_CoopClientReadback);
		GetGame().GetCallqueue().Remove(AICF_CoopProbeDeadline);
		GetGame().GetCallqueue().Remove(AICF_CoopProbeClose);
		super.OnGameEnd();
	}
}

modded class AICF_ConstructionPlanner
{
	bool AICF_CoopProbeAutomatic(SCR_CampaignFaction faction)
	{
		return Commander(faction) != null;
	}
}

modded class SCR_FactionRequestUIComponent
{
	override void ShowAvailableFactions()
	{
		super.ShowAvailableFactions();
		string mode;
		if (!System.GetCLIParam("aicfCoopProbe", mode) || mode != "USSR") return;
		bool passed = m_aActiveFactionButtons.Count() == 1;
		foreach (SCR_FactionButton button : m_aActiveFactionButtons)
			passed = passed && button && button.GetFaction() && button.GetFaction().GetFactionKey() == "USSR";
		Print(string.Format("[AICF][COOP_FACTION_MENU] active=%1 pass=%2", m_aActiveFactionButtons.Count(), passed));
	}
}
