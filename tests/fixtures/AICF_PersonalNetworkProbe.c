// Только stage WCS. Проверяет native owner RPC без управления интерфейсом.
modded class SCR_GameModeCampaign
{
	protected int m_iAICFNetworkTick;
	protected int m_iAICFNetworkAt;
	protected int m_iAICFNetworkStep;
	protected int m_iAICFNetworkToken;
	protected int m_iAICFNetworkPassed;
	protected int m_iAICFNetworkChecked;
	protected ref array<int> m_aAICFNetworkOperations = {0, 1, 3, 4};
	protected bool m_bAICFNetworkStarted;
	protected bool m_bAICFNetworkSpawned;

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!System.GetCLIParam("aicfPersonalNetworkProbe", enabled) || enabled != "1") return;
		string mode;
		if (System.GetCLIParam("aicfProbePersonal", mode) && mode == "1") m_aAICFNetworkOperations.Insert(3);
		GetGame().GetCallqueue().CallLater(AICF_NetworkTick, 2000, true);
	}

	protected void AICF_NetworkCheck(bool ok, string label)
	{
		m_iAICFNetworkChecked++;
		if (ok) m_iAICFNetworkPassed++;
		Print(string.Format("[AICF][PERSONAL_NETWORK_CHECK] check=%1 passed=%2", label, ok));
	}

	protected void AICF_NetworkTick()
	{
		if (Replication.IsServer()) return;
		m_iAICFNetworkTick++;
		if (m_iAICFNetworkTick > 65) { AICF_NetworkCheck(false, "TIMEOUT"); AICF_NetworkClose(); return; }
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		if (m_bAICFNetworkSpawned)
		{
			if (m_iAICFNetworkTick - m_iAICFNetworkAt < 8) return;
			IEntity deployed = player.GetControlledEntity();
			AICF_NetworkCheck(AICF_GroupRuntime.IsAliveCharacter(deployed), "DEPLOYED");
			string deployedFaction;
			System.GetCLIParam("aicfProbeFaction", deployedFaction);
			AICF_NetworkCheck(AICF_WCSInfantryEquipment.HasEquippedArmor(deployed, deployedFaction), "PLAYER_ARMOR");
			AICF_NetworkClose();
			return;
		}
		string factionKey = "RHS_AFRF";
		System.GetCLIParam("aicfProbeFaction", factionKey);
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(factionKey));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!faction || !affiliation) return;
		if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return; }
		if (!SCR_GroupsManagerComponent.GetInstance().GetPlayerGroup(player.GetPlayerId())) { SCR_PlayerControllerGroupComponent.Cast(player.FindComponent(SCR_PlayerControllerGroupComponent)).RequestCreateGroup(); return; }
		array<ref SCR_BasePlayerLoadout> loadouts = {};
		GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(faction, loadouts);
		SCR_BasePlayerLoadout loadout;
		foreach (SCR_BasePlayerLoadout candidate : loadouts)
			if (!SCR_PlayerArsenalLoadout.Cast(candidate)) { loadout = candidate; break; }
		SCR_PlayerLoadoutComponent component = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
		if (!component || !loadout) return;
		if (component.GetLoadout() != loadout) { component.RequestLoadout(loadout); return; }
		if (!m_bAICFNetworkStarted)
		{
			if (player.AICF_PersonalContext().IsEmpty()) return;
			m_bAICFNetworkStarted = true;
			m_iAICFNetworkAt = m_iAICFNetworkTick;
			return;
		}
		if (m_iAICFNetworkTick - m_iAICFNetworkAt < 2) return;
		if (m_iAICFNetworkToken > 0)
		{
			if (player.AICF_LoadoutToken() != m_iAICFNetworkToken) return;
			int operation = m_aAICFNetworkOperations[m_iAICFNetworkStep];
			bool accepted = player.AICF_LoadoutAccepted();
			AICF_NetworkCheck(accepted, "OP_" + operation.ToString() + "_" + player.AICF_LoadoutStatus());
			SCR_CampaignFaction ignored;
			AICF_LoadoutRecipe local = AICF_PersonalLoadout.Context(player, ignored);
			AICF_NetworkCheck(AICF_PersonalLoadout.SameContext(AICF_LoadoutRecipe.Decode(player.AICF_PersonalContext()), local), "CONTEXT_GUID_PATH");
			if (operation == 1) AICF_NetworkCheck(player.AICF_PersonalAvailable(), "AVAILABLE");
			if (operation == 3) AICF_NetworkCheck(player.AICF_PersonalSelected(), "SELECTED");
			if (operation == 4) AICF_NetworkCheck(!player.AICF_PersonalSelected(), "DEFAULT_SELECTED");
			m_iAICFNetworkStep++;
			m_iAICFNetworkToken = 0;
			if (!accepted) { AICF_NetworkClose(); return; }
		}
		if (m_iAICFNetworkStep < m_aAICFNetworkOperations.Count())
		{
			m_iAICFNetworkToken = player.AICF_RequestLoadout(AICF_PersonalLoadout.SLOT, 0, player.AICF_LoadoutRevision(), m_aAICFNetworkOperations[m_iAICFNetworkStep], player.AICF_LoadoutData());
			m_iAICFNetworkAt = m_iAICFNetworkTick;
			return;
		}
		if (!m_bAICFNetworkSpawned)
		{
			AICF_NetworkDrafts();
			SCR_SpawnPoint chosen;
			float nearest = float.MAX;
			foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPointsForFaction(faction.GetFactionKey()))
			{
				if (!point || !point.IsSpawnPointActive()) continue;
				float distance = vector.DistanceXZ(point.GetOrigin(), faction.GetMainBase().GetOwner().GetOrigin());
				if (distance >= nearest) continue;
				nearest = distance;
				chosen = point;
			}
			AICF_NetworkCheck(chosen != null, "SPAWN_POINT");
			if (!chosen) { AICF_NetworkClose(); return; }
			SCR_RespawnComponent.Cast(player.GetRespawnComponent()).RequestSpawn(new SCR_SpawnPointSpawnData(loadout.GetLoadoutResource(), chosen.GetRplId()));
			m_bAICFNetworkSpawned = true;
			m_iAICFNetworkAt = m_iAICFNetworkTick;
			return;
		}
	}

	protected void AICF_NetworkDrafts()
	{
		array<string> sides = {"RHS_USAF", "RHS_AFRF"};
		AICF_GroupSpawner roster = new AICF_GroupSpawner();
		foreach (string side : sides)
		{
			SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(side));
			AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
			for (int member; member < 10; member++)
			{
				string role, reason;
				AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
				recipe.m_sCharacter = roster.ResolveRecruitPrefab(faction, member, role);
				recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(side);
				AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
				AICF_NetworkCheck(draft.Build(recipe, catalog, reason) && AICF_WCSInfantryEquipment.HasEquippedArmor(draft.GetCharacter(), side), side + "_DRAFT_" + role);
				draft.Clear();
			}
		}
	}

	protected void AICF_NetworkClose()
	{
		Print(string.Format("[AICF][PERSONAL_NETWORK_RESULT] total=%1 passed=%2", m_iAICFNetworkChecked, m_iAICFNetworkPassed));
		GetGame().GetCallqueue().Remove(AICF_NetworkTick);
		GetGame().RequestClose();
	}

	void ~SCR_GameModeCampaign()
	{
		GetGame().GetCallqueue().Remove(AICF_NetworkTick);
	}
}
