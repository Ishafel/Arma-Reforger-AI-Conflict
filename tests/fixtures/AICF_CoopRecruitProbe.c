// Isolated UI stage, after AICF_CoopScenarioProbe. Native group catalogue and recruitment RPC; not a paid barracks-order test.
modded class AICF_MatchController
{
	protected SCR_AIGroup m_AICFRecruitSource;
	protected bool m_bAICFRecruitPrepared;
	override protected void Update()
	{
		super.Update();
		if (!m_bRosterReady || m_bStopped) return;
		m_Campaign.AICF_JoinObserve();
		if (!m_bAICFRecruitPrepared)
		{
			m_bAICFRecruitPrepared = true;
			m_Construction.Stop(); // Isolate recruitment from unrelated stock service mesh diagnostics.
			if (!m_AICFRecruitSource)
			{
				array<SCR_EntityCatalogEntry> entries = {};
				m_USSRFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.GROUP).GetEntityList(entries);
				foreach (SCR_EntityCatalogEntry entry : entries)
				{
					if (!entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData)) continue;
					EntitySpawnParams spawn = new EntitySpawnParams();
					spawn.TransformMode = ETransformMode.WORLD;
					vector point, angle;
					m_USSRFaction.GetMainBase().GetSpawnPoint().GetPositionAndRotation(point, angle);
					point += "15 0 15";
					point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
					spawn.Transform[3] = point;
					m_AICFRecruitSource = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(entry.GetPrefab()), null, spawn));
					if (!m_AICFRecruitSource) continue;
					m_AICFRecruitSource.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
					m_AICFRecruitSource.SetDeleteWhenEmpty(false);
					m_AICFRecruitSource.RequestSpawn();
					Print("[JOIN_SOURCE] kind=STOCK_GROUP_CATALOG prefab=" + entry.GetPrefab());
					break;
				}
			}
		}
		if (!m_AICFRecruitSource) return;
		array<AIAgent> agents = {};
		m_AICFRecruitSource.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character || !AICF_GroupRuntime.IsAliveCharacter(character)) continue;
			m_Campaign.AICF_JoinSetTarget(character);
			break;
		}
	}
}

modded class SCR_GameModeCampaign
{
	[RplProp()]
	protected RplId m_AICFJoinTarget = RplId.Invalid();
	protected bool m_bAICFJoinRequested;
	protected bool m_bAICFJoinServerDone;
	[RplProp()]
	protected int m_iAICFJoinLeaderPhase;
	protected bool m_bAICFJoinVoteRequested;
	void AICF_JoinObserve()
	{
		if (!Replication.IsServer() || m_bAICFJoinServerDone || !m_AICFJoinTarget.IsValid()) return;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(Replication.FindItem(m_AICFJoinTarget));
		if (!character) return;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int player : players)
		{
			SCR_AIGroup group = SCR_GroupsManagerComponent.GetInstance().GetPlayerGroup(player);
			if (!group || !GetGame().GetPlayerManager().GetPlayerControlledEntity(player)) continue;
			if (m_iAICFJoinLeaderPhase == 0 && group.IsPlayerLeader(player))
			{
				// Test setup: exercise the reported non-leader state without another human.
				group.SetGroupLeader(-1);
				m_iAICFJoinLeaderPhase = 1;
				Replication.BumpMe();
				Print("[JOIN_LEADER_SETUP] removed=1");
				return;
			}
			if (m_iAICFJoinLeaderPhase == 1 && group.IsPlayerLeader(player))
			{
				m_iAICFJoinLeaderPhase = 2;
				Replication.BumpMe();
				Print("[JOIN_LEADER_RECOVERED] pass=1");
			}
			if (!character.IsRecruited()) continue;
			if (!group || !group.GetSlave() || !group.GetSlave().GetAIMembers().Contains(character)) continue;
			AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
			bool passed = group.IsPlayerLeader(player) && control && control.GetAIAgent() && control.GetAIAgent().GetParentGroup() == group.GetSlave();
			Print(string.Format("[JOIN_SERVER] pass=%1 player=%2 recruited=%3", passed, player, character.IsRecruited()));
			m_bAICFJoinServerDone = true;
		}
	}
	void AICF_JoinSetTarget(SCR_ChimeraCharacter character)
	{
		if (!Replication.IsServer() || m_AICFJoinTarget.IsValid()) return;
		m_AICFJoinTarget = Replication.FindItemId(character);
		Replication.BumpMe();
		Print(string.Format("[JOIN_TARGET] entity=%1 recruitable=%2 prefab=%3", m_AICFJoinTarget, character.IsRecruitable(), SCR_ResourceNameUtils.GetPrefabName(character)));
	}
	protected bool AICF_JoinPreparePlayer()
	{
		PlayerController player = GetGame().GetPlayerController();
		if (!player) return false;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey("USSR"));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation || !faction) return false;
		if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return false; }
		SCR_PlayerControllerGroupComponent groups = SCR_PlayerControllerGroupComponent.Cast(player.FindComponent(SCR_PlayerControllerGroupComponent));
		if (!groups.GetPlayersGroup()) { Print("[JOIN_NO_GROUP]"); return false; }
		array<ref SCR_BasePlayerLoadout> loadouts = {};
		GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(faction, loadouts);
		SCR_BasePlayerLoadout loadout;
		foreach (SCR_BasePlayerLoadout candidate : loadouts)
			if (!SCR_PlayerArsenalLoadout.Cast(candidate)) { loadout = candidate; break; }
		SCR_PlayerLoadoutComponent component = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
		if (!component || !loadout) return false;
		if (component.GetLoadout() != loadout) { component.RequestLoadout(loadout); return false; }
		return true;
	}
	override protected void AICF_CoopClientReadback()
	{
		m_iCoopClientSamples++;
		if (m_iCoopClientSamples > 90) { Print("[JOIN_TIMEOUT]"); GetGame().RequestClose(); return; }
		if (m_iCoopClientSamples % 5 != 0 || !AICF_JoinPreparePlayer() || !AICF_CoopClientSpawn()) return;
		SCR_PlayerControllerGroupComponent groups = SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
		SCR_AIGroup group = groups.GetPlayersGroup();
		SCR_AIGroup slave = group.GetSlave();
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(Replication.FindItem(m_AICFJoinTarget));
		if (!slave || !character) return;
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		if (m_iAICFJoinLeaderPhase < 2)
		{
			if (m_iAICFJoinLeaderPhase == 1 && !m_bAICFJoinVoteRequested && !groups.IsPlayerLeaderOwnGroup())
			{
				bool available = manager.IsGroupMenuAllowed() && manager.IsGroupLeaderVolunteeringAllowed();
				Print(string.Format("[JOIN_VOLUNTEER] pass=%1 leader=0", available));
				if (!available) { GetGame().RequestClose(); return; }
				groups.RequestGroupLeaderVote(GetGame().GetPlayerController().GetPlayerId());
				m_bAICFJoinVoteRequested = true;
			}
			return;
		}
		if (!m_bAICFJoinRequested)
		{
			SCR_RecruitAIGroupCommand command;
			foreach (SCR_BaseRadialCommand candidate : SCR_CommandingManagerComponent.GetInstance().GetCommandsConfig().GetCommands())
			{
				command = SCR_RecruitAIGroupCommand.Cast(candidate);
				if (command) break;
			}
			bool passed = groups.IsPlayerLeaderOwnGroup() && manager.IsGroupMenuAllowed() && manager.IsGroupLeaderVolunteeringAllowed() && character.IsRecruitable() && command && command.CanBeExecuted(character);
			Print(string.Format("[JOIN_ADMISSION] pass=%1 leader=%2 menu=%3 volunteer=%4 recruitable=%5", passed, groups.IsPlayerLeaderOwnGroup(), manager.IsGroupMenuAllowed(), manager.IsGroupLeaderVolunteeringAllowed(), character.IsRecruitable()));
			if (!passed) { GetGame().GetCallqueue().Remove(AICF_CoopClientReadback); GetGame().RequestClose(); return; }
			groups.RequestAddAIAgent(character);
			m_bAICFJoinRequested = true;
			return;
		}
		bool joined = character.IsRecruited() && slave.GetAIMembers().Contains(character) && groups.IsAICharacterInAnyGroup(character, SCR_Faction.Cast(group.GetFaction()));
		Print(string.Format("[JOIN_RESULT] pass=%1 recruited=%2 members=%3", joined, character.IsRecruited(), slave.GetAIMembers().Count()));
		if (!joined) return;
		GetGame().GetCallqueue().Remove(AICF_CoopClientReadback);
		GetGame().RequestClose();
	}
}

modded class SCR_WelcomeScreenFactionContent
{
	override protected void FillFactionWidget(notnull SCR_Faction faction, notnull Widget content, Color color)
	{
		int before = m_aFactionWidgets.Count();
		super.FillFactionWidget(faction, content, color);
		if (m_aFactionWidgets.Count() > before) Print(string.Format("[JOIN_WELCOME] faction=%1 visible=%2", faction.GetFactionKey(), m_aFactionWidgets[before].IsVisible()));
	}
}
