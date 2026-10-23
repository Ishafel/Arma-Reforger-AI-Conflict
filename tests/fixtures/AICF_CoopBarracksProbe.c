// Isolated diagnostic: normal building-editor order, followed by native recruitment.
class AICF_BarracksProbeConfig
{
	static string FactionKey()
	{
		string key;
		if (System.GetCLIParam("aicfBarracksFaction", key)) return key;
		return "USSR";
	}
	static ResourceName GuardPrefab()
	{
		if (FactionKey() == "US") return "{0A8E20F50DA233E1}Prefabs/Groups/BLUFOR/Group_US_FireTeam_Guard.et";
		return "{6F72F05752ED62A8}Prefabs/Groups/OPFOR/Group_USSR_FireGroup_Guard.et";
	}
}

modded class SCR_CampaignBuildingPlacingEditorComponent
{
	override protected bool CanPlaceEntityServer(IEntityComponentSource editableEntitySource, out EEditableEntityBudget blockingBudget, bool updatePreview, bool showNotification, int prefabID = -1, int playerID = -1, SCR_EditorPreviewParams params = null)
	{
		bool result = super.CanPlaceEntityServer(editableEntitySource, blockingBudget, updatePreview, showNotification, prefabID, playerID, params);
		Print(string.Format("[BUY_SERVER_VALIDATION] pass=%1 budget=%2 prefab_id=%3 player=%4", result, blockingBudget, prefabID, playerID));
		return result;
	}
	override bool IsThereEnoughBudgetToSpawn(IEntityComponentSource entitySource)
	{
		SCR_CampaignBuildingProviderComponent provider = SCR_CampaignBuildingProviderComponent.Cast(m_Provider.FindComponent(SCR_CampaignBuildingProviderComponent));
		SCR_CampaignBuildingProviderComponent realProvider = provider;
		int before = provider.GetBudgetValue(EEditableEntityBudget.CAMPAIGN, realProvider);
		array<ref SCR_EntityBudgetValue> costs = {};
		SCR_EditableEntityUIInfo info = SCR_EditableEntityComponentClass.GetInfo(entitySource);
		SCR_EditableGroupUIInfo groupInfo = SCR_EditableGroupUIInfo.Cast(info);
		if (groupInfo) groupInfo.GetGroupBudgetCost(costs);
		else info.GetEntityAndChildrenBudgetCost(costs);
		int cost;
		foreach (SCR_EntityBudgetValue value : costs)
			if (value.GetBudgetType() == EEditableEntityBudget.CAMPAIGN) cost += value.GetBudgetValue();
		SCR_GameModeCampaign.GetInstance().AICF_BuyRecordCost(realProvider, before, cost);
		bool result = super.IsThereEnoughBudgetToSpawn(entitySource);
		Print("[BUY_SERVER_BUDGET] pass=" + result);
		return result;
	}
}

modded class AICF_MatchController
{
	protected SCR_CampaignBuildingCompositionComponent m_AICFBarracks;
	protected bool m_bAICFBarracksPrepared;
	protected bool m_bAICFEditorPrepared;
	protected SCR_AIGroup m_AICFUnpaidGuard;
	protected bool m_bAICFUnpaidChecked;
	protected SCR_CampaignBuildingProviderComponent AICF_BuyFindProvider(IEntity entity)
	{
		if (!entity) return null;
		SCR_CampaignBuildingProviderComponent provider = SCR_CampaignBuildingProviderComponent.Cast(entity.FindComponent(SCR_CampaignBuildingProviderComponent));
		if (provider)
		{
			Print(string.Format("[BUY_PROVIDER] prefab=%1 command_ai=%2", SCR_ResourceNameUtils.GetPrefabName(entity), provider.CanCommandAI()));
			if (provider.CanCommandAI()) return provider;
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			provider = AICF_BuyFindProvider(child);
			if (provider) return provider;
			child = child.GetSibling();
		}
		return null;
	}
	override protected void Update()
	{
		super.Update();
		if (!m_bRosterReady || m_bStopped) return;
		if (!m_bAICFBarracksPrepared)
		{
			m_bAICFBarracksPrepared = true;
			m_Construction.Stop();
			SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(AICF_BarracksProbeConfig.FactionKey()));
			SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
			base.AddSupplies(base.GetSuppliesMax() - base.GetSupplies());
			vector position, rotation;
			base.GetSpawnPoint().GetPositionAndRotation(position, rotation);
			position += "50 0 25";
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = position;
			ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab(faction.GetFactionKey(), AICF_EConstructionType.LARGE_BARRACKS);
			SCR_EditorLinkComponent.IgnoreSpawning(true);
			IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
			SCR_EditorLinkComponent.IgnoreSpawning(false);
			if (entity)
			{
				FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent)).SetAffiliatedFaction(faction);
				m_AICFBarracks = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
				if (m_AICFBarracks) m_AICFBarracks.SetProviderEntity(base.GetMasterProvider().GetOwner());
			}
			Print("[BUY_SETUP] barracks=" + (m_AICFBarracks != null));
			params.Transform[3] = position + "20 0 20";
			m_AICFUnpaidGuard = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(AICF_BarracksProbeConfig.GuardPrefab()), null, params));
			if (m_AICFUnpaidGuard) m_AICFUnpaidGuard.RequestSpawn();
		}
		if (m_AICFUnpaidGuard && !m_bAICFUnpaidChecked)
		{
			array<AIAgent> unpaid = {};
			m_AICFUnpaidGuard.GetAgents(unpaid);
			if (unpaid.Count() == 4)
			{
				bool unchanged = true;
				foreach (AIAgent member : unpaid)
				{
					SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(member.GetControlledEntity());
					if (!character || character.IsRecruitable()) unchanged = false;
				}
				Print(string.Format("[BUY_UNPAID_GUARD] pass=%1 count=%2", unchanged, unpaid.Count()));
				m_bAICFUnpaidChecked = true;
			}
		}
		if (!m_AICFBarracks) return;
		if (!m_AICFBarracks.IsCompositionSpawned())
		{
			SCR_CampaignBuildingLayoutComponent layout = m_AICFBarracks.GetCompositionLayout();
			if (layout) layout.AddBuildingValue(layout.GetToBuildValue());
			return;
		}
		m_Campaign.AICF_BuyObserve();
		if (m_bAICFEditorPrepared) return;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int player : players)
		{
			IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(player);
			if (!controlled) continue;
			SCR_CharacterRankComponent rank = SCR_CharacterRankComponent.Cast(controlled.FindComponent(SCR_CharacterRankComponent));
			if (rank) rank.SetCharacterRank(SCR_ECharacterRank.GENERAL, true);
			SCR_CampaignBuildingProviderComponent provider = AICF_BuyFindProvider(m_AICFBarracks.GetOwner());
			if (!provider) continue;
			vector playerPosition = provider.GetOwner().GetOrigin() + "3 0 3";
			playerPosition[1] = GetGame().GetWorld().GetSurfaceY(playerPosition[0], playerPosition[2]);
			controlled.SetOrigin(playerPosition);
			m_Campaign.AICF_BuySetProvider(provider);
			m_bAICFEditorPrepared = true;
			Print(string.Format("[BUY_EDITOR] player=%1 provider=%2 command_ai=%3", player, SCR_ResourceNameUtils.GetPrefabName(provider.GetOwner()), provider.CanCommandAI()));
			break;
		}
	}
}

modded class SCR_GameModeCampaign
{
	protected SCR_AIGroup m_AICFBuyGroup;
	[RplProp()]
	protected RplId m_AICFBuyTarget = RplId.Invalid();
	protected bool m_bAICFBuyRequested;
	protected bool m_bAICFBuyRecruitRequested;
	protected bool m_bAICFBuyServerDone;
	protected bool m_bAICFBuyBrowserPrepared;
	[RplProp()]
	protected RplId m_AICFBuyProvider = RplId.Invalid();
	protected bool m_bAICFBuyEditorRequested;
	protected RplId m_AICFBuyLastRequested = RplId.Invalid();
	protected ref array<SCR_ChimeraCharacter> m_aAICFBought = {};
	protected SCR_CampaignBuildingProviderComponent m_AICFBuyCostProvider;
	protected int m_iAICFBuyBefore;
	protected int m_iAICFBuyCost;
	protected bool m_bAICFBuyPaymentReported;
	protected bool m_bAICFBuyClosing;
	protected SCR_ResourceConsumer m_AICFBuyConsumer;
	protected float m_fAICFBuyDebited;
	protected float m_fAICFBuyCredited;
	void AICF_BuyRecordCost(SCR_CampaignBuildingProviderComponent provider, int before, int cost)
	{
		m_AICFBuyCostProvider = provider;
		m_iAICFBuyBefore = before;
		m_iAICFBuyCost = cost;
		if (!m_AICFBuyConsumer)
		{
			m_AICFBuyConsumer = provider.GetResourceComponent().GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
			m_AICFBuyConsumer.GetOnResourcesChanged().Insert(AICF_BuyResourcesChanged);
		}
	}
	protected void AICF_BuyResourcesChanged(SCR_ResourceInteractor interactor, float previousValue)
	{
		if (!Replication.IsServer() || interactor != m_AICFBuyConsumer) return;
		float delta = m_AICFBuyConsumer.GetAggregatedResourceValue() - previousValue;
		if (delta < 0) m_fAICFBuyDebited -= delta;
		else m_fAICFBuyCredited += delta;
	}
	void AICF_BuySetProvider(SCR_CampaignBuildingProviderComponent provider)
	{
		if (!Replication.IsServer() || m_AICFBuyProvider.IsValid()) return;
		m_AICFBuyProvider = SCR_EntityHelper.GetEntityRplComponent(provider.GetOwner()).Id();
		Replication.BumpMe();
	}
	override void OnEntityRequested(notnull IEntity spawnedEntity, IEntity user, SCR_Faction faction, SCR_MilitaryBaseLogicComponent service)
	{
		super.OnEntityRequested(spawnedEntity, user, faction, service);
		if (!Replication.IsServer() || !SCR_AIGroup.Cast(spawnedEntity)) return;
		m_AICFBuyGroup = SCR_AIGroup.Cast(spawnedEntity);
		Print("[BUY_CREATED] prefab=" + SCR_ResourceNameUtils.GetPrefabName(spawnedEntity));
		Faction actual = m_AICFBuyGroup.GetFaction();
		Print(string.Format("[BUY_FACTION] pass=%1 faction=%2", actual && actual.GetFactionKey() == AICF_BarracksProbeConfig.FactionKey() && SCR_Faction.GetEntityFaction(user) == actual, AICF_BarracksProbeConfig.FactionKey()));
		Print(string.Format("[BUY_REQUEST_CONTEXT] event_faction_present=%1 group_faction_present=%2", faction != null, m_AICFBuyGroup.GetFaction() != null));
	}
	void AICF_BuyObserve()
	{
		if (m_bAICFBuyServerDone) return;
		if (m_AICFBuyCostProvider && m_AICFBuyGroup && !m_bAICFBuyPaymentReported)
		{
			SCR_CampaignBuildingProviderComponent realProvider = m_AICFBuyCostProvider;
			int after = m_AICFBuyCostProvider.GetBudgetValue(EEditableEntityBudget.CAMPAIGN, realProvider);
			if (m_iAICFBuyCost > 0 && m_fAICFBuyDebited >= m_iAICFBuyCost)
			{
				bool balanced = float.AlmostEqual(m_iAICFBuyBefore - after, m_fAICFBuyDebited - m_fAICFBuyCredited, 0.01);
				Print(string.Format("[BUY_PAYMENT] pass=%1 before=%2 after=%3 cost=%4 debited=%5 credited=%6", balanced, m_iAICFBuyBefore, after, m_iAICFBuyCost, m_fAICFBuyDebited, m_fAICFBuyCredited));
				m_bAICFBuyPaymentReported = true;
			}
		}
		array<AIAgent> agents = {};
		if (m_AICFBuyGroup) m_AICFBuyGroup.GetAgents(agents);
		if (m_aAICFBought.IsEmpty() && agents.Count() == 4)
		{
			int recruitable;
			foreach (AIAgent agent : agents)
			{
				SCR_ChimeraCharacter bought = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (!bought) return;
				m_aAICFBought.Insert(bought);
				if (bought.IsRecruitable()) recruitable++;
			}
			Print(string.Format("[BUY_ROSTER] count=%1 recruitable=%2", m_aAICFBought.Count(), recruitable));
		}
		if (m_aAICFBought.Count() != 4) return;
		foreach (SCR_ChimeraCharacter character : m_aAICFBought)
		{
			if (!character || character.IsRecruited()) continue;
			RplId target = Replication.FindItemId(character);
			if (m_AICFBuyTarget != target)
			{
				m_AICFBuyTarget = target;
				Replication.BumpMe();
				Print(string.Format("[BUY_TARGET] recruitable=%1 recruited=%2 prefab=%3", character.IsRecruitable(), character.IsRecruited(), SCR_ResourceNameUtils.GetPrefabName(character)));
			}
			return;
		}
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int player : players)
		{
			SCR_AIGroup group = SCR_GroupsManagerComponent.GetInstance().GetPlayerGroup(player);
			if (!group || !group.GetSlave()) continue;
			bool belongsToPlayer = group.IsPlayerLeader(player);
			foreach (SCR_ChimeraCharacter bought : m_aAICFBought)
			{
				AIControlComponent control = AIControlComponent.Cast(bought.FindComponent(AIControlComponent));
				if (!bought.IsRecruited() || !group.GetSlave().GetAIMembers().Contains(bought) || !control || !control.GetAIAgent() || control.GetAIAgent().GetParentGroup() != group.GetSlave()) belongsToPlayer = false;
			}
			Print(string.Format("[BUY_SERVER] pass=%1 members=%2", belongsToPlayer, m_aAICFBought.Count()));
			m_bAICFBuyServerDone = true;
		}
	}
	protected bool AICF_BuyPreparePlayer()
	{
		PlayerController player = GetGame().GetPlayerController();
		if (!player) return false;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(AICF_BarracksProbeConfig.FactionKey()));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation || !faction) return false;
		if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return false; }
		SCR_PlayerControllerGroupComponent groups = SCR_PlayerControllerGroupComponent.Cast(player.FindComponent(SCR_PlayerControllerGroupComponent));
		if (!groups) return false;
		if (!groups.GetPlayersGroup()) { groups.CreateAndJoinGroup(faction, true); return false; }
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
		if (m_bAICFBuyClosing) return;
		m_iCoopClientSamples++;
		if (m_iCoopClientSamples > 120) { Print("[BUY_TIMEOUT]"); AICF_BuyFinishClient(); return; }
		if (m_iCoopClientSamples % 5 != 0 || !AICF_BuyPreparePlayer() || !AICF_CoopClientSpawn()) return;
		SCR_PlayerControllerGroupComponent groups = SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
		SCR_AIGroup group = groups.GetPlayersGroup();
		SCR_AIGroup slave = group.GetSlave();
		if (!slave) return;
		if (!m_bAICFBuyRequested)
		{
			if (!m_bAICFBuyEditorRequested)
			{
				RplComponent providerRpl = RplComponent.Cast(Replication.FindItem(m_AICFBuyProvider));
				if (!providerRpl || !providerRpl.GetEntity()) return;
				PlayerController player = GetGame().GetPlayerController();
				SCR_CampaignBuildingNetworkComponent network = SCR_CampaignBuildingNetworkComponent.Cast(player.FindComponent(SCR_CampaignBuildingNetworkComponent));
				network.RequestEnterBuildingMode(providerRpl.GetEntity(), player.GetPlayerId(), true, true);
				m_bAICFBuyEditorRequested = true;
				Print("[BUY_ENTER_REQUEST] sent=1");
				return;
			}
			SCR_EditorManagerEntity editor = SCR_EditorManagerEntity.GetInstance();
			if (!editor) return;
			SCR_EditorModeEntity mode = editor.FindModeEntity(EEditorMode.BUILDING);
			if (!mode) return;
			SCR_CampaignBuildingEditorComponent building = SCR_CampaignBuildingEditorComponent.Cast(mode.FindComponent(SCR_CampaignBuildingEditorComponent));
			SCR_PlacingEditorComponent placing = SCR_PlacingEditorComponent.Cast(mode.FindComponent(SCR_PlacingEditorComponent));
			if (!building || !placing) return;
			if (!building.GetProviderEntity() || !building.GetTrigger())
			{
				Print(string.Format("[BUY_WAIT] provider=%1 trigger=%2", building.GetProviderEntity(), building.GetTrigger()));
				return;
			}
			array<ResourceName> prefabs = {};
			SCR_PlacingEditorComponentClass.Cast(placing.GetComponentData(mode)).GetPrefabs(prefabs);
			ResourceName selected;
			foreach (ResourceName prefab : prefabs)
			{
				if (prefab == AICF_BarracksProbeConfig.GuardPrefab()) selected = prefab;
			}
			Print(string.Format("[BUY_CATALOG] count=%1 selected=%2", prefabs.Count(), selected));
			if (selected.IsEmpty()) { GetGame().RequestClose(); return; }
			SCR_ContentBrowserEditorComponent browser = SCR_ContentBrowserEditorComponent.Cast(mode.FindComponent(SCR_ContentBrowserEditorComponent));
			if (!m_bAICFBuyBrowserPrepared)
			{
				array<SCR_EditorContentBrowserSaveStateDataUI> tabs = {};
				browser.GetContentBrowserTabStates(tabs);
				foreach (int index, SCR_EditorContentBrowserSaveStateDataUI tab : tabs)
				{
					array<EEditableEntityLabel> labels = {};
					tab.GetLabels(labels);
					Print(string.Format("[BUY_TAB] index=%1 group=%2 character=%3", index, labels.Contains(EEditableEntityLabel.ENTITYTYPE_GROUP), labels.Contains(EEditableEntityLabel.ENTITYTYPE_CHARACTER)));
					if (labels.Contains(EEditableEntityLabel.ENTITYTYPE_GROUP)) browser.SetBrowserState(index);
				}
				m_bAICFBuyBrowserPrepared = true;
				return;
			}
			bool available = browser.IsPrefabIDAvailable(placing.GetPrefabID(selected));
			Print("[BUY_AVAILABLE] pass=" + available);
			if (!available) return;
			vector transform[4];
			Math3D.MatrixIdentity4(transform);
			transform[3] = building.GetTrigger().GetOrigin() + "10 0 10";
			transform[3][1] = GetGame().GetWorld().GetSurfaceY(transform[3][0], transform[3][2]);
			SCR_EditorPreviewParams params = SCR_EditorPreviewParams.CreateParams(transform);
			bool sent = placing.CreateEntity(selected, params);
			ENotification notification;
			SCR_EPreviewState state;
			bool valid = placing.CanCreateEntity(notification, state, params, placing.GetPrefabID(selected));
			Print(string.Format("[BUY_VALIDATION] pass=%1 notification=%2 state=%3 blocked=%4", valid, notification, state, placing.IsPlacingBlocked()));
			Print(string.Format("[BUY_SENT] pass=%1 leader=%2 members=%3 max=%4", sent, groups.IsPlayerLeaderOwnGroup(), slave.GetServerAgentsCount(), SCR_CommandingManagerComponent.GetInstance().GetMaxAIPerGroup()));
			m_bAICFBuyRequested = true;
			return;
		}
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(Replication.FindItem(m_AICFBuyTarget));
		if (!character) return;
		if (!m_bAICFBuyRecruitRequested || m_AICFBuyLastRequested != m_AICFBuyTarget)
		{
			SCR_RecruitAIGroupCommand command;
			foreach (SCR_BaseRadialCommand candidate : SCR_CommandingManagerComponent.GetInstance().GetCommandsConfig().GetCommands())
			{
				command = SCR_RecruitAIGroupCommand.Cast(candidate);
				if (command) break;
			}
			bool permitted = command && command.CanBeExecuted(character);
			Print(string.Format("[BUY_ADMISSION] pass=%1 leader=%2 recruitable=%3 members=%4 max=%5", permitted, groups.IsPlayerLeaderOwnGroup(), character.IsRecruitable(), slave.GetServerAgentsCount(), SCR_CommandingManagerComponent.GetInstance().GetMaxAIPerGroup()));
			if (!permitted) { AICF_BuyFinishClient(); return; }
			groups.RequestAddAIAgent(character);
			m_bAICFBuyRecruitRequested = true;
			m_AICFBuyLastRequested = m_AICFBuyTarget;
			return;
		}
		bool joined = character.IsRecruited() && slave.GetAIMembers().Contains(character) && slave.GetAIMembers().Count() == 4;
		if (!joined) return;
		Print(string.Format("[BUY_RESULT] pass=1 recruited=1 members=%1", slave.GetAIMembers().Count()));
		SCR_RecruitAIGroupCommand recruit;
		foreach (SCR_BaseRadialCommand candidate : SCR_CommandingManagerComponent.GetInstance().GetCommandsConfig().GetCommands())
		{
			recruit = SCR_RecruitAIGroupCommand.Cast(candidate);
			if (recruit) break;
		}
		int maxAI = SCR_CommandingManagerComponent.GetInstance().GetMaxAIPerGroup();
		Print(string.Format("[BUY_LIMIT] pass=%1 members=%2 max=%3", maxAI == 4 && slave.GetServerAgentsCount() == 4 && recruit && !recruit.CanBeExecuted(character), slave.GetServerAgentsCount(), maxAI));
		AICF_BuyFinishClient();
	}
	protected void AICF_BuyFinishClient()
	{
		if (m_bAICFBuyClosing) return;
		m_bAICFBuyClosing = true;
		GetGame().GetCallqueue().Remove(AICF_CoopClientReadback);
		SCR_EditorManagerEntity editor = SCR_EditorManagerEntity.GetInstance();
		if (editor) editor.Close();
		GetGame().GetCallqueue().CallLater(AICF_BuyCloseClient, 3000, false);
	}
	protected void AICF_BuyCloseClient()
	{
		GetGame().RequestClose();
	}
	override void OnGameEnd()
	{
		if (m_AICFBuyConsumer) m_AICFBuyConsumer.GetOnResourcesChanged().Remove(AICF_BuyResourcesChanged);
		m_AICFBuyConsumer = null;
		GetGame().GetCallqueue().Remove(AICF_BuyCloseClient);
		super.OnGameEnd();
	}
}
