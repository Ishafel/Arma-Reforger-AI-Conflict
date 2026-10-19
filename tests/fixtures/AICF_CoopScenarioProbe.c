// Isolated stage only. Observes production bootstrap/roster and replicated rules.
modded class AICF_MatchController
{
	protected int m_iCoopProbeStarted;
	protected bool m_bCoopProbeFailed;
	protected int m_iCoopProbeChecks;
	protected bool m_bCoopProbeDone;
	protected ref array<AICF_GroupSlot> m_aCoopUSSlots = {};
	protected ref array<AICF_GroupSlot> m_aCoopUSSRSlots = {};

	protected void AICF_CoopCaptureSlots(AICF_FactionState state, array<AICF_GroupSlot> slots)
	{
		for (int i; i < state.GetSlotCount(); i++) slots.Insert(state.GetSlot(i));
	}

	protected bool AICF_CoopSameSlots(AICF_FactionState state, int expected, array<AICF_GroupSlot> slots)
	{
		if (state.GetSlotCount() != expected || slots.Count() != expected) return false;
		for (int i; i < expected; i++)
		{
			AICF_GroupSlot slot = state.GetSlot(i);
			if (!slot || slot != slots[i] || slot.GetSlotId() != i) return false;
		}
		return true;
	}

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
		int expectedUS = 10;
		if (mode == "USSR") expectedUSSR = 0;
		if (mode == "US") expectedUS = 0;
		bool coop = mode != "STANDARD";
		int now = System.GetTickCount();
		if (!m_iCoopProbeStarted)
		{
			m_iCoopProbeStarted = now;
			AICF_CoopCheck("US_ROSTER", m_USState.GetSlotCount() == expectedUS && m_USState.CountSlotsByState(AICF_EGroupSlotState.READY) == expectedUS && CountFactionAgents(m_USState) >= expectedUS);
			AICF_CoopCheck("USSR_ROSTER", m_USSRState.GetSlotCount() == expectedUSSR && m_USSRState.CountSlotsByState(AICF_EGroupSlotState.READY) == expectedUSSR && CountFactionAgents(m_USSRState) >= expectedUSSR);
			AICF_CoopCaptureSlots(m_USState, m_aCoopUSSlots);
			AICF_CoopCaptureSlots(m_USSRState, m_aCoopUSSRSlots);
			AICF_ContentProfile profile = AICF_ContentProfile.GetActive();
			AICF_CoopCheck("CONTENT_FACTIONS", m_USFaction.GetFactionKey() == profile.GetRuntimeFactionKey("US") && m_USSRFaction.GetFactionKey() == profile.GetRuntimeFactionKey("USSR"));
			AICF_EMatchMode expectedMode = AICF_EMatchMode.STANDARD;
			if (mode == "US") expectedMode = AICF_EMatchMode.COOP_US;
			if (mode == "USSR") expectedMode = AICF_EMatchMode.COOP_USSR;
			AICF_CoopCheck("HEADER_MODE", m_Config.GetMatchRules().GetMode() == expectedMode);
			Print(string.Format("[AICF][COOP_CONTEXT] mode=%1 us_faction=%2 ussr_faction=%3 nodes=%4", mode, m_USFaction.GetFactionKey(), m_USSRFaction.GetFactionKey(), m_ObjectiveGraph.GetNodeCount()));
			AICF_CoopCheck("QUEUE_SETTLED", m_USState.IsInitialSpawnQueueSettled() && m_USSRState.IsInitialSpawnQueueSettled());
			AICF_CoopCheck("US_TICKETS", m_USState.UsesTicketDefeat() == (expectedUS > 0));
			AICF_CoopCheck("USSR_TICKETS", m_USSRState.UsesTicketDefeat() == (expectedUSSR > 0));
			AICF_CoopCheck("COMMANDERS", ((expectedUS > 0) == (m_USAICommander != null)) && ((expectedUSSR > 0) == (m_USSRAICommander != null)));
			AICF_CoopCheck("HELPERS", m_BaseBuilders && m_Logistics && m_Construction && m_EconomySystem && m_VehicleCoordinator);
			AICF_CoopCheck("FACTION_ACCESS", m_USFaction.IsPlayable() && m_USSRFaction.IsPlayable() && m_Config.GetMatchRules().AllowsPlayerFaction(m_USFaction.GetFactionKey()) == (!coop || expectedUS == 0) && m_Config.GetMatchRules().AllowsPlayerFaction(m_USSRFaction.GetFactionKey()) == (!coop || expectedUSSR == 0) && m_Config.GetMatchRules().AllowsPlayerFaction("FIA") == !coop);
			AICF_CoopCheck("AUTO_CONSTRUCTION", m_Construction.AICF_CoopProbeAutomatic(m_USFaction) == (expectedUS > 0) && m_Construction.AICF_CoopProbeAutomatic(m_USSRFaction) == (expectedUSSR > 0));
			AICF_Stage1Config budget = new AICF_Stage1Config();
			budget.SetMaxManagedAgents(1);
			AICF_CoopCheck("BUDGET_FLOOR", budget.GetMaxManagedAgents() == (expectedUS + expectedUSSR) * 10);
			AICF_MatchRules standard = new AICF_MatchRules();
			AICF_CoopCheck("STANDARD_POLICY", standard.GetManagedSlotCount("US") == 10 && standard.GetManagedSlotCount("USSR") == 10 && standard.UsesTicketDefeat("USSR"));
			AICF_MatchRules mirror = new AICF_MatchRules(AICF_EMatchMode.COOP_US);
			AICF_CoopCheck("MIRROR_POLICY", mirror.GetManagedSlotCount("US") == 0 && mirror.GetManagedSlotCount("USSR") == 10);
			if (coop)
			{
				budget.SetInitialTickets(1);
				if (expectedUS == 0) budget.SetInitialTickets(0);
				AICF_FactionState testUS = new AICF_FactionState("US", budget);
				budget.SetInitialTickets(0);
				if (expectedUSSR > 0) budget.SetInitialTickets(1);
				AICF_FactionState testUSSR = new AICF_FactionState("USSR", budget);
				AICF_VictorySystem victory = new AICF_VictorySystem();
				AICF_CoopCheck("ZERO_PLAYER_TICKETS_NO_DEFEAT", !victory.EvaluateAndEnd(m_Campaign, testUS, testUSSR, null) && !victory.IsEnded());
				budget.SetAICommanderMode("BOTH");
				AICF_CoopCheck("CONFLICT_REJECTED", !budget.IsAICommanderModeValid());
			}
		}
		int duration = 90000;
		string durationText;
		if (System.GetCLIParam("aicfCoopProbeDurationMs", durationText)) duration = durationText.ToInt();
		if (now - m_iCoopProbeStarted < duration) return;
		// Combat losses/replacements may change live counts; numeric slot identity must persist.
		bool stable = AICF_CoopSameSlots(m_USState, expectedUS, m_aCoopUSSlots) && AICF_CoopSameSlots(m_USSRState, expectedUSSR, m_aCoopUSSRSlots);
		if (expectedUS == 0) stable = stable && CountFactionAgents(m_USState) == 0;
		if (expectedUSSR == 0) stable = stable && CountFactionAgents(m_USSRState) == 0;
		AICF_CoopCheck("STABLE_ROSTER", stable);
		Print(string.Format("[AICF][COOP_ROSTER_OBSERVATION] us_slots=%1 us_agents=%2 ussr_slots=%3 ussr_agents=%4", m_USState.GetSlotCount(), CountFactionAgents(m_USState), m_USSRState.GetSlotCount(), CountFactionAgents(m_USSRState)));
		AICF_CoopCheck("REPLICATED_MODE", m_Campaign.AICF_IsPlayerOnlyFaction(true) == (expectedUSSR == 0) && m_Campaign.AICF_IsPlayerOnlyFaction(false) == (expectedUS == 0));
		if (coop)
		{
			AICF_FactionState humanState = m_USSRState;
			if (expectedUS == 0) humanState = m_USState;
			AICF_CoopCheck("NO_GROUP_SUMMARY", BuildGroupSummary(humanState, 0).IsEmpty());
		}
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
	protected Faction m_AICFCoopExpectedAffiliation;

	protected string AICF_CoopDeniedSide()
	{
		string mode;
		if (System.GetCLIParam("aicfCoopProbe", mode) && mode == "US") return "USSR";
		return "US";
	}

	protected void AICF_CoopFactionResponse(SCR_PlayerFactionAffiliationComponent component, int factionIndex, bool response)
	{
		Faction faction = GetGame().GetFactionManager().GetFactionByIndex(factionIndex);
		string expected = AICF_CoopDeniedSide();
		if (m_iAICFCoopDenied == 1) expected = "FIA";
		string runtimeKey = expected;
		if (expected != "FIA") runtimeKey = AICF_ContentProfile.GetActive().GetRuntimeFactionKey(expected);
		bool passed = faction && faction.GetFactionKey() == runtimeKey && !response && component.GetAffiliatedFaction() == m_AICFCoopExpectedAffiliation;
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
		string key = AICF_ContentProfile.GetActive().GetRuntimeFactionKey(AICF_CoopDeniedSide());
		if (m_iAICFCoopDenied == 1) key = "FIA";
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(key);
		if (!faction) return false;
		m_bAICFCoopRequestPending = true;
		m_AICFCoopExpectedAffiliation = m_AICFCoopAffiliation.GetAffiliatedFaction();
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
			string mode;
			System.GetCLIParam("aicfCoopProbe", mode);
			bool humanUSSR = mode == "USSR";
			bool passed = AICF_IsPlayerOnlyFaction(humanUSSR) && !AICF_IsPlayerOnlyFaction(!humanUSSR) &&
				AICF_GetAICommanderEnabled(!humanUSSR) && !AICF_GetAICommanderEnabled(humanUSSR) &&
				AICF_GetCombatGroups(humanUSSR) == 0 && AICF_GetManagedAgents(humanUSSR) == 0 && AICF_GetStrategicGroupSummary(humanUSSR, 0).IsEmpty();
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
		string mode;
		if (System.GetCLIParam("aicfCoopProbe", mode) && mode == "US") factionKey = "US";
		string testFaction;
		if (System.GetCLIParam("aicfBarracksFaction", testFaction)) factionKey = testFaction;
		string runtimeKey = AICF_ContentProfile.GetActive().GetRuntimeFactionKey(factionKey);
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(runtimeKey));
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
		array<ref SCR_BasePlayerLoadout> loadouts = {};
		GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(faction, loadouts);
		SCR_BasePlayerLoadout loadout;
		foreach (SCR_BasePlayerLoadout candidate : loadouts)
			if (!SCR_PlayerArsenalLoadout.Cast(candidate)) { loadout = candidate; break; }
		SCR_PlayerLoadoutComponent loadoutComponent = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
		if (!loadoutComponent || !loadout) return false;
		if (loadoutComponent.GetLoadout() != loadout) { loadoutComponent.RequestLoadout(loadout); return false; }
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
		foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPointsForFaction(runtimeKey))
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

modded class SCR_WelcomeScreenFactionContent
{
	override protected void FillFactionWidget(notnull SCR_Faction faction, notnull Widget content, Color color)
	{
		int previous = m_aFactionWidgets.Count();
		super.FillFactionWidget(faction, content, color);
		if (m_aFactionWidgets.Count() == previous) return;
		Print(string.Format("[AICF][COOP_WELCOME] faction=%1 visible=%2 playable=%3", faction.GetFactionKey(), m_aFactionWidgets[previous].IsVisible(), faction.IsPlayable()));
	}
}

modded class SCR_FactionRequestUIComponent
{
	override void ShowAvailableFactions()
	{
		super.ShowAvailableFactions();
		string mode;
		if (!System.GetCLIParam("aicfCoopProbe", mode) || mode == "STANDARD") return;
		bool passed = m_aActiveFactionButtons.Count() == 1;
		foreach (SCR_FactionButton button : m_aActiveFactionButtons)
			passed = passed && button && button.GetFaction() && AICF_ContentProfile.GetActive().GetStableFactionKey(button.GetFaction().GetFactionKey()) == mode;
		Print(string.Format("[AICF][COOP_FACTION_MENU] active=%1 pass=%2", m_aActiveFactionButtons.Count(), passed));
	}
}
