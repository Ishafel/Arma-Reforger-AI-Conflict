// Только изолированный terminal server/client. Подготовка двух AI в player
// group и смерть игрока; выбор и possession используют production RPC/pipeline.
modded class SCR_LoadoutGallery
{
	bool AICF_ProbeNativeCards(RplId target)
	{
		return m_aLoadoutButtons.Count() > 0 && m_aAICFCharacters.Count() == 1 &&
			m_aAICFCharacters[0].m_AICFCharacter == target &&
			m_aAICFCharacters[0].GetImageWidget() && m_aAICFCharacters[0].GetImageWidget().IsVisible() &&
			m_aWidgets.Contains(m_aAICFCharacters[0].GetRootWidget());
	}
}

modded class SCR_LoadoutRequestUIComponent
{
	bool AICF_ProbeCharacterPreview(RplId target)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(target));
		return rpl && m_bAICFCharacterPreview && m_AICFPreviewSource == rpl.GetEntity() &&
			m_wLoadoutPreview && m_wLoadoutPreview.IsVisible() && m_PreviewComp.GetItemPreviewWidget();
	}

	bool AICF_ProbeNativeCards(RplId target)
	{
		return m_LoadoutSelector && m_LoadoutSelector.AICF_ProbeNativeCards(target);
	}
}

modded class SCR_DeployMenuMain
{
	bool AICF_ProbeSelect(RplId target)
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!m_LoadoutRequestUIHandler || !player) return false;
		m_LoadoutRequestUIHandler.AICF_SyncCharacters(player, RplId.Invalid());
		if (!m_LoadoutRequestUIHandler.AICF_ProbeNativeCards(target)) return false;
		AICF_SelectCharacter(target, "Probe AI");
		return m_AICFSelectedCharacter == target && !GetRootWidget().FindAnyWidget("AICF_SquadRespawn") && m_LoadoutRequestUIHandler.AICF_ProbeCharacterPreview(target);
	}

	void AICF_ProbeSubmit() { RequestRespawn(); }
}

modded class AICF_MatchController
{
	IEntity AICF_RespawnProbeOther(bool enemy)
	{
		if (!m_bRosterReady) return null;
		if (enemy) return AICF_GroupRuntime.ResolveAliveLeader(m_USSRState.GetSlot(0).GetGroup());
		return AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(0).GetGroup());
	}
}

modded class SCR_GameModeCampaign
{
	protected int m_iAICFRPPhase;
	protected int m_iAICFRPStart;
	protected int m_iAICFRPAt;
	protected int m_iAICFRPFailures;
	protected int m_iAICFRPClientAttempts;
	protected SCR_PlayerController m_AICFRPPlayer;
	protected SCR_AIGroup m_AICFRPGroup;
	protected IEntity m_AICFRPTarget;
	protected IEntity m_AICFRPDead;
	protected EntityID m_AICFRPTargetId;
	protected RplId m_AICFRPSelected;
	protected ref array<IEntity> m_aAICFRPItems = {};

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!System.GetCLIParam("aicfSquadRespawnProbe", enabled) || enabled != "1") return;
		m_iAICFRPStart = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_RPTick, 1000, true);
	}

	protected void AICF_RPCheck(string name, bool passed)
	{
		if (!passed) m_iAICFRPFailures++;
		Print(string.Format("[AICF][SQUAD_RESPAWN_PROBE] server=%1 check=%2 passed=%3", Replication.IsServer(), name, passed));
	}

	protected void AICF_RPTick()
	{
		if (System.GetTickCount() - m_iAICFRPStart > 300000)
		{
			AICF_RPCheck("completed_before_deadline", false);
			AICF_RPClose();
			return;
		}
		if (!Replication.IsServer()) { AICF_RPClient(); return; }
		if (m_iAICFRPPhase == 4)
		{
			if (System.GetTickCount() - m_iAICFRPAt > 10000) AICF_RPClose();
			return;
		}
		AICF_MatchController match = AICF_MatchController.GetActiveController();
		if (!match || !match.AICF_RespawnProbeOther(false)) return;
		if (!m_AICFRPPlayer)
		{
			array<int> players = {};
			GetGame().GetPlayerManager().GetPlayers(players);
			foreach (int id : players)
			{
				SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
				if (pc && AICF_GroupRuntime.IsAliveCharacter(pc.GetControlledEntity())) { m_AICFRPPlayer = pc; break; }
			}
			if (!m_AICFRPPlayer) return;
		}
		if (m_iAICFRPPhase == 0)
		{
			m_AICFRPGroup = SCR_GroupsManagerComponent.GetInstance().GetPlayerGroup(m_AICFRPPlayer.GetPlayerId());
			if (!m_AICFRPGroup) return;
			if (!m_AICFRPGroup.GetSlave())
			{
				SCR_PlayerControllerGroupComponent.Cast(m_AICFRPPlayer.FindComponent(SCR_PlayerControllerGroupComponent)).RequestCreateSlaveGroup(RplComponent.Cast(m_AICFRPGroup.FindComponent(RplComponent)).Id());
				return;
			}
			AICF_RPCheck("native_request_component", m_AICFRPPlayer.FindComponent(SCR_PossessSpawnRequestComponent) != null);
			AICF_RPCheck("native_handler_component", FindComponent(SCR_PossessSpawnHandlerComponent) != null);
			IEntity body = m_AICFRPPlayer.GetControlledEntity();
			// Campaign player shell не содержит экипировки до native spawn finalization.
			// Для AI нужен готовый catalog prefab штатного бойца.
			ResourceName aiPrefab = match.AICF_RespawnProbeOther(false).GetPrefabData().GetPrefabName();
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = body.GetOrigin() + "4 0 0";
			m_AICFRPTarget = GetGame().SpawnEntityPrefab(Resource.Load(aiPrefab), GetGame().GetWorld(), params);
			params.Transform[3] = body.GetOrigin() + "8 0 0";
			m_AICFRPDead = GetGame().SpawnEntityPrefab(Resource.Load(aiPrefab), GetGame().GetWorld(), params);
			m_iAICFRPAt = System.GetTickCount();
			m_iAICFRPPhase = 1;
		}
		if (m_iAICFRPPhase == 1 && System.GetTickCount() - m_iAICFRPAt > 5000)
		{
			if (!m_AICFRPTarget || !m_AICFRPDead) { AICF_RPCheck("spawn_fixture_ai", false); AICF_RPClose(); return; }
			AIControlComponent control = AIControlComponent.Cast(m_AICFRPTarget.FindComponent(AIControlComponent));
			AIControlComponent deadControl = AIControlComponent.Cast(m_AICFRPDead.FindComponent(AIControlComponent));
			if (!control || !control.GetAIAgent() || !deadControl || !deadControl.GetAIAgent()) return;
			SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.Cast(m_AICFRPPlayer.FindComponent(SCR_PlayerControllerGroupComponent));
			groupController.AddAIToSlaveGroup(SCR_ChimeraCharacter.Cast(m_AICFRPTarget), m_AICFRPGroup);
			groupController.AddAIToSlaveGroup(SCR_ChimeraCharacter.Cast(m_AICFRPDead), m_AICFRPGroup);
			array<AIAgent> direct = {};
			m_AICFRPGroup.GetAgents(direct);
			AICF_RPCheck("legacy_direct_scan_misses_recruited_ai", !direct.Contains(control.GetAIAgent()));
			AICF_RPCheck("native_recruit_in_slave", m_AICFRPGroup.IsAIControlledCharacterMember(SCR_ChimeraCharacter.Cast(m_AICFRPTarget)));
			m_AICFRPTargetId = m_AICFRPTarget.GetID();
			InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(m_AICFRPTarget.FindComponent(InventoryStorageManagerComponent));
			if (inventory) inventory.GetItems(m_aAICFRPItems);
			foreach (IEntity beforeItem : m_aAICFRPItems)
				Print(string.Format("[AICF][SQUAD_RESPAWN_INVENTORY] phase=BEFORE id=%1 prefab=%2", beforeItem.GetID(), beforeItem.GetPrefabData().GetPrefabName()));
			AICF_RPCheck("fixture_has_equipment", !m_aAICFRPItems.IsEmpty());
			AICF_RPCheck("live_player_rejected", !AICF_SquadRespawnPolicy.CanTake(m_AICFRPPlayer, m_AICFRPGroup, m_AICFRPTarget));
			SCR_CharacterDamageManagerComponent.Cast(m_AICFRPDead.FindComponent(SCR_CharacterDamageManagerComponent)).Kill(Instigator.CreateInstigatorGM());
			SCR_CharacterDamageManagerComponent.Cast(m_AICFRPPlayer.GetControlledEntity().FindComponent(SCR_CharacterDamageManagerComponent)).Kill(Instigator.CreateInstigatorGM());
			m_iAICFRPPhase = 2;
		}
		if (m_iAICFRPPhase == 2 && m_AICFRPPlayer.AICF_CanChooseSquadRespawn())
		{
			AICF_RPCheck("dead_player_own_ai_allowed", AICF_SquadRespawnPolicy.CanTake(m_AICFRPPlayer, m_AICFRPGroup, m_AICFRPTarget));
			AICF_RPCheck("dead_target_rejected", !AICF_SquadRespawnPolicy.CanTake(m_AICFRPPlayer, m_AICFRPGroup, m_AICFRPDead));
			AICF_RPCheck("other_squad_rejected", !AICF_SquadRespawnPolicy.CanTake(m_AICFRPPlayer, m_AICFRPGroup, match.AICF_RespawnProbeOther(false)));
			AICF_RPCheck("enemy_rejected", !AICF_SquadRespawnPolicy.CanTake(m_AICFRPPlayer, m_AICFRPGroup, match.AICF_RespawnProbeOther(true)));
			AICF_SquadRespawnAttempt stale = new AICF_SquadRespawnAttempt();
			stale.m_Character = m_AICFRPTarget;
			stale.m_CharacterId = m_AICFRPTargetId;
			stale.m_Group = m_AICFRPGroup;
			stale.m_GroupId = m_AICFRPGroup.GetID();
			stale.m_MemberGroup = m_AICFRPGroup.GetSlave();
			stale.m_MemberGroupId = stale.m_MemberGroup.GetID();
			stale.m_iDeathRevision = m_AICFRPPlayer.AICF_GetSquadDeathRevision() - 1;
			AICF_RPCheck("old_death_rejected", !AICF_SquadRespawnPolicy.IsCurrent(m_AICFRPPlayer, stale, m_AICFRPTarget));
			stale.m_iDeathRevision = m_AICFRPPlayer.AICF_GetSquadDeathRevision();
			AICF_RPCheck("current_slave_identity_allowed", AICF_SquadRespawnPolicy.IsCurrent(m_AICFRPPlayer, stale, m_AICFRPTarget));
			stale.m_MemberGroup = m_AICFRPGroup;
			stale.m_MemberGroupId = m_AICFRPGroup.GetID();
			AICF_RPCheck("wrong_member_group_rejected", !AICF_SquadRespawnPolicy.IsCurrent(m_AICFRPPlayer, stale, m_AICFRPTarget));
			m_iAICFRPPhase = 3;
		}
		if (m_iAICFRPPhase == 3 && m_AICFRPPlayer.GetControlledEntity() == m_AICFRPTarget)
		{
			array<IEntity> items = {};
			InventoryStorageManagerComponent.Cast(m_AICFRPTarget.FindComponent(InventoryStorageManagerComponent)).GetItems(items);
			// Native possession добавляет PersonalBelongings_US; GetItems может
			// перечислять одну entity несколько раз через вложенные storages.
			bool same = !m_aAICFRPItems.IsEmpty();
			array<IEntity> added = {};
			bool nativeAddition = true;
			foreach (IEntity candidate : items)
			{
				if (m_aAICFRPItems.Contains(candidate) || added.Contains(candidate)) continue;
				added.Insert(candidate);
				if (!candidate || candidate.GetPrefabData().GetPrefabName() != "{6676FF9B716402CC}Prefabs/Items/PersonalBelongings/PersonalBelongings_US.et") nativeAddition = false;
			}
			Print(string.Format("[AICF][SQUAD_RESPAWN_PROBE] inventory_before=%1 inventory_after=%2", m_aAICFRPItems.Count(), items.Count()));
			foreach (IEntity afterItem : items)
				Print(string.Format("[AICF][SQUAD_RESPAWN_INVENTORY] phase=AFTER id=%1 prefab=%2", afterItem.GetID(), afterItem.GetPrefabData().GetPrefabName()));
			foreach (IEntity item : m_aAICFRPItems) { if (!item || !items.Contains(item)) same = false; }
			AICF_RPCheck("same_character", m_AICFRPTarget.GetID() == m_AICFRPTargetId);
			AICF_RPCheck("same_inventory_entities", same);
			AICF_RPCheck("only_native_personal_belongings_added", nativeAddition && added.Count() <= 1);
			AICF_RPCheck("same_player_group", SCR_GroupsManagerComponent.GetInstance().GetPlayerGroup(m_AICFRPPlayer.GetPlayerId()) == m_AICFRPGroup);
			AICF_RPCheck("alive_after_possession", AICF_GroupRuntime.IsAliveCharacter(m_AICFRPTarget));
			m_iAICFRPAt = System.GetTickCount();
			m_iAICFRPPhase = 4;
		}
	}

	protected void AICF_RPClient()
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		if (m_iAICFRPPhase == 0)
		{
			if (AICF_GroupRuntime.IsAliveCharacter(player.GetControlledEntity())) { m_iAICFRPPhase = 1; return; }
			if (System.GetTickCount() - m_iAICFRPAt >= 10000) { m_iAICFRPAt = System.GetTickCount(); AICF_RPSpawnClient(); }
			return;
		}
		if (m_iAICFRPPhase == 1 && !AICF_GroupRuntime.IsAliveCharacter(player.GetControlledEntity()))
		{
			player.AICF_RequestSquadRespawnList();
			if (player.m_aAICFSquadRespawnIds.IsEmpty()) return;
			AICF_RPCheck("owner_list_contains_only_live_ai", player.m_aAICFSquadRespawnIds.Count() == 1);
			m_AICFRPSelected = player.m_aAICFSquadRespawnIds[0];
			player.AICF_RequestSquadRespawn(RplId.Invalid(), player.m_iAICFSquadListDeathRevision);
			m_iAICFRPPhase = 2;
		}
		if (m_iAICFRPPhase == 2 && player.m_sAICFSquadRespawnResult == "UNAVAILABLE")
		{
			SCR_DeployMenuMain menu = SCR_DeployMenuMain.GetDeployMenu();
			if (!menu) return;
			AICF_RPCheck("invalid_rpc_rejected", true);
			AICF_RPCheck("native_gallery_presets_and_ai_no_overlay", menu.AICF_ProbeSelect(m_AICFRPSelected));
			menu.AICF_ProbeSubmit();
			m_iAICFRPPhase = 3;
		}
		if (m_iAICFRPPhase == 3)
		{
			if (!AICF_GroupRuntime.IsAliveCharacter(player.GetControlledEntity()))
			{
				SCR_DeployMenuMain menu = SCR_DeployMenuMain.GetDeployMenu();
				if (menu) menu.AICF_ProbeSubmit();
				return;
			}
			RplComponent rpl = RplComponent.Cast(player.GetControlledEntity().FindComponent(RplComponent));
			AICF_RPCheck("owner_controls_selected_ai", rpl && rpl.Id() == m_AICFRPSelected);
			m_iAICFRPPhase = 4;
			AICF_RPClose();
		}
	}

	protected void AICF_RPSpawnClient()
	{
		PlayerController player = GetGame().GetPlayerController();
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey("US"));
		if (!player || !faction || !faction.GetMainBase()) return;
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return; }
		if (!SCR_GroupsManagerComponent.GetInstance().GetPlayerGroup(player.GetPlayerId()))
		{
			SCR_PlayerControllerGroupComponent.Cast(player.FindComponent(SCR_PlayerControllerGroupComponent)).RequestCreateGroup();
			return;
		}
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		ResourceName prefab;
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (entry && entry.GetPrefab().EndsWith("Campaign_US_AI.et")) { prefab = entry.GetPrefab(); break; }
		}
		SCR_SpawnPoint chosen;
		float nearest = float.MAX;
		foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPointsForFaction("US"))
		{
			if (!point || !point.IsSpawnPointActive()) continue;
			float distance = vector.DistanceXZ(point.GetOrigin(), faction.GetMainBase().GetOwner().GetOrigin());
			if (distance >= nearest) continue;
			nearest = distance;
			chosen = point;
		}
		if (chosen && !prefab.IsEmpty()) SCR_RespawnComponent.Cast(player.GetRespawnComponent()).RequestSpawn(new SCR_SpawnPointSpawnData(prefab, chosen.GetRplId()));
	}

	protected void AICF_RPClose()
	{
		GetGame().GetCallqueue().Remove(AICF_RPTick);
		Print(string.Format("[AICF][SQUAD_RESPAWN_PROBE] finished=1 server=%1 phase=%2 failures=%3", Replication.IsServer(), m_iAICFRPPhase, m_iAICFRPFailures));
		GetGame().RequestClose();
	}

	void ~SCR_GameModeCampaign()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_RPTick);
	}
}
