// Append only to staging AICF_SquadRespawnUI.c. Native save/spawn/menu paths;
// setup creates one equipped recruit and kills test characters on the server.
class AICF_PresetRespawnProbe
{
	static int s_iFailures;
	static int s_iOverlap;
	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfPresetRespawnProbe", value) && value == "1";
	}
	static void Check(string name, bool passed)
	{
		if (!passed) s_iFailures++;
		Print(string.Format("[AICF][PRESET_RESPAWN_PROBE] server=%1 check=%2 passed=%3", Replication.IsServer(), name, passed));
	}
	static SCR_CampaignFaction ResolveFaction()
	{
		string key = "US";
		System.GetCLIParam("aicfPresetProbeFaction", key);
		return SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(key));
	}
	static SCR_SpawnPoint Point()
	{
		SCR_CampaignFaction faction = ResolveFaction();
		if (!faction || !faction.GetMainBase()) return null;
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
		return chosen;
	}
	static SCR_BasePlayerLoadout Loadout(bool saved)
	{
		array<ref SCR_BasePlayerLoadout> loadouts = {};
		GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(ResolveFaction(), loadouts);
		foreach (SCR_BasePlayerLoadout loadout : loadouts)
		{
			bool custom = SCR_PlayerArsenalLoadout.Cast(loadout) != null;
			if (custom == saved && loadout.IsLoadoutAvailableClient()) return loadout;
		}
		return null;
	}
	static void Inventory(IEntity entity, out array<string> prefabs)
	{
		prefabs.Clear();
		array<IEntity> items = {};
		array<IEntity> unique = {};
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
		if (!inventory) return;
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			if (!item || unique.Contains(item)) continue;
			unique.Insert(item);
			string prefab = item.GetPrefabData().GetPrefabName();
			if (prefab.Contains("PersonalBelongings")) continue;
			prefabs.Insert(prefab);
		}
		prefabs.Sort();
	}
}

modded class SCR_PossessSpawnRequestComponent
{
	override void StartSpawnPreload(vector position)
	{
		super.StartSpawnPreload(position);
		if (AICF_PresetRespawnProbe.Enabled()) Rpc(RpcDo_AICFProbeOverlap);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFProbeOverlap()
	{
		SCR_SpawnPoint point = AICF_PresetRespawnProbe.Point();
		SCR_BasePlayerLoadout loadout = AICF_PresetRespawnProbe.Loadout(true);
		if (!point || !loadout) { AICF_PresetRespawnProbe.Check("overlap_setup", false); return; }
		bool sent = GetRespawnComponent().CanSpawn(new SCR_SpawnPointSpawnData(loadout.GetLoadoutResource(), point.GetRplId()));
		AICF_PresetRespawnProbe.s_iOverlap++;
		AICF_PresetRespawnProbe.Check("overlapping_base_query_serialized", !sent);
	}
}

modded class SCR_LoadoutRequestUIComponent
{
	bool AICF_ProbeRequestSaved()
	{
		SCR_BasePlayerLoadout loadout = AICF_PresetRespawnProbe.Loadout(true);
		if (!loadout || !m_LoadoutSelector) return false;
		SCR_LoadoutButton button = m_LoadoutSelector.GetButtonForLoadout(loadout);
		if (!button) return false;
		OnRequestPlayerLoadout(button);
		return true;
	}
}

modded class SCR_DeployMenuMain
{
	protected SCR_ESpawnResult m_eAICFProbeLastResult = SCR_ESpawnResult.OK;
	override protected void OnCanRespawnRequestResponse(SCR_SpawnRequestComponent requestComponent, SCR_ESpawnResult response, SCR_SpawnData data)
	{
		if (AICF_PresetRespawnProbe.Enabled() && m_eAICFProbeLastResult != response)
		{
			Print(string.Format("[AICF][PRESET_RESPAWN_CAN] result=%1", typename.EnumToString(SCR_ESpawnResult, response)));
			m_eAICFProbeLastResult = response;
		}
		super.OnCanRespawnRequestResponse(requestComponent, response, data);
	}
	bool AICF_ProbeSavedAtBase()
	{
		SCR_SpawnPoint point = AICF_PresetRespawnProbe.Point();
		if (!point || !m_LoadoutRequestUIHandler) return false;
		if (!SCR_PlayerArsenalLoadout.Cast(m_LoadoutRequestUIHandler.GetPlayerLoadout()))
		{
			m_LoadoutRequestUIHandler.AICF_ProbeRequestSaved();
			return false;
		}
		if (m_iSelectedSpawnPointId != point.GetRplId()) SetSpawnPoint(point.GetRplId(), false);
		AICF_ClearCharacter();
		UpdateRespawnButton();
		Print(string.Format("[AICF][PRESET_RESPAWN_MENU] enabled=%1 allowed=%2 requested=%3 preset=%4", m_RespawnButton.IsEnabled(), m_bCanRespawnAtSpawnPoint, m_bRespawnRequested, m_LoadoutRequestUIHandler.GetPlayerLoadout().Type()));
		RequestRespawn();
		return true;
	}
	void AICF_ProbeTake(RplId character)
	{
		AICF_SelectCharacter(character, "Preset probe recruit");
		RequestRespawn();
	}
}

modded class AICF_MatchController
{
	IEntity AICF_PresetProbeReference()
	{
		if (!m_bRosterReady) return null;
		return AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(0).GetGroup());
	}
}

modded class SCR_GameModeCampaign
{
	protected int m_iAICFPPhase;
	protected int m_iAICFPStart;
	protected int m_iAICFPAt;
	protected SCR_PlayerController m_AICFPPlayer;
	protected IEntity m_AICFPTarget;
	protected RplId m_AICFPTargetRpl = RplId.Invalid();
	protected string m_sAICFPSaved;
	protected ref array<string> m_aAICFPInventory = {};
	protected bool m_bAICFPEnding;
	protected IEntity m_AICFPArmory;
	override void OnPlayerSpawnFinalize_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, IEntity entity)
	{
		if (AICF_PresetRespawnProbe.Enabled())
		{
			SCR_PlayerLoadoutComponent selected = SCR_PlayerLoadoutComponent.Cast(requestComponent.GetPlayerController().FindComponent(SCR_PlayerLoadoutComponent));
			Print(string.Format("[AICF][PRESET_RESPAWN_FINALIZE] phase=%1 data=%2 loadout=%3 entity=%4 faction=%5", m_iAICFPPhase, data.Type(), selected.GetLoadout(), entity.GetID(), SCR_Faction.GetEntityFaction(entity).GetFactionKey()));
		}
		super.OnPlayerSpawnFinalize_S(requestComponent, handlerComponent, data, entity);
	}

	override void OnGameStart()
	{
		super.OnGameStart();
		if (!AICF_PresetRespawnProbe.Enabled()) return;
		m_iAICFPStart = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_PresetTick, 2000, true);
	}
	protected void AICF_PresetTick()
	{
		if (m_bAICFPEnding) return;
		if (System.GetTickCount() - m_iAICFPStart > 240000)
		{
			AICF_PresetRespawnProbe.Check("completed_before_deadline", false);
			AICF_PresetClose();
			return;
		}
		if (!Replication.IsServer()) { AICF_PresetClient(); return; }
		if (m_iAICFPPhase == 5)
		{
			if (System.GetTickCount() - m_iAICFPAt > 15000) AICF_PresetClose();
			return;
		}
		if (!m_AICFPPlayer)
		{
			array<int> players = {};
			GetGame().GetPlayerManager().GetPlayers(players);
			foreach (int id : players)
			{
				SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
				if (player && AICF_GroupRuntime.IsAliveCharacter(player.GetControlledEntity())) { m_AICFPPlayer = player; m_iAICFPAt = System.GetTickCount(); break; }
			}
			return;
		}
		IEntity body = m_AICFPPlayer.GetControlledEntity();
		SCR_ArsenalManagerComponent arsenal;
		if (!SCR_ArsenalManagerComponent.GetArsenalManager(arsenal)) return;
		SCR_ArsenalPlayerLoadout saved;
		int playerId = m_AICFPPlayer.GetPlayerId();
		SCR_AIGroup group = SCR_GroupsManagerComponent.GetInstance().GetPlayerGroup(playerId);
		if (m_iAICFPPhase == 0 && System.GetTickCount() - m_iAICFPAt > 5000)
		{
			if (!group) return;
			SCR_CampaignMilitaryBaseComponent base = AICF_PresetRespawnProbe.ResolveFaction().GetMainBase();
			SCR_ServicePointDelegateComponent armory = base.GetServiceDelegateByType(SCR_EServicePointType.ARMORY);
			if (!armory)
			{
				if (!m_AICFPArmory)
				{
					EntitySpawnParams armoryParams = new EntitySpawnParams();
					armoryParams.TransformMode = ETransformMode.WORLD;
					armoryParams.Transform[3] = base.GetOwner().GetOrigin() + "30 0 30";
					armoryParams.Transform[3][1] = GetGame().GetWorld().GetSurfaceY(armoryParams.Transform[3][0], armoryParams.Transform[3][2]);
					ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab("US", AICF_EConstructionType.ARMORY);
					// Только подготовка теста: готовая штатная постройка с настоящей
					// service registration; respawn eligibility не подменяется.
					m_AICFPArmory = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), armoryParams);
					if (m_AICFPArmory)
					{
						SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(m_AICFPArmory.FindComponent(SCR_CampaignBuildingCompositionComponent));
						if (composition) composition.SetProviderEntity(base.GetMasterProvider().GetOwner());
					}
				}
				return;
			}
			AICF_PresetRespawnProbe.Check("real_armory_registered_at_base", armory != null);
			SCR_PlayerControllerGroupComponent controller = SCR_PlayerControllerGroupComponent.Cast(m_AICFPPlayer.FindComponent(SCR_PlayerControllerGroupComponent));
			if (!group.GetSlave()) { controller.RequestCreateSlaveGroup(RplComponent.Cast(group.FindComponent(RplComponent)).Id()); return; }
			AICF_MatchController match = AICF_MatchController.GetActiveController();
			if (!match || !match.AICF_PresetProbeReference()) return;
			arsenal.SetPlayerArsenalLoadout(playerId, GameEntity.Cast(body), null, SCR_EArsenalSupplyCostType.RESPAWN_COST);
			if (!arsenal.GetPlayerArsenalLoadout(SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId), saved)) { AICF_PresetRespawnProbe.Check("preset_saved", false); AICF_PresetClose(); return; }
			m_sAICFPSaved = saved.loadout;
			AICF_PresetRespawnProbe.Inventory(body, m_aAICFPInventory);
			AICF_PresetRespawnProbe.Check("preset_saved_with_equipment", !m_sAICFPSaved.IsEmpty() && m_aAICFPInventory.Count() > 5);
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = body.GetOrigin() + "4 0 0";
			m_AICFPTarget = GetGame().SpawnEntityPrefab(Resource.Load(match.AICF_PresetProbeReference().GetPrefabData().GetPrefabName()), GetGame().GetWorld(), params);
			m_iAICFPPhase = 1;
			return;
		}
		if (m_iAICFPPhase == 1)
		{
			AIControlComponent control = AIControlComponent.Cast(m_AICFPTarget.FindComponent(AIControlComponent));
			if (!control || !control.GetAIAgent()) return;
			SCR_PlayerControllerGroupComponent.Cast(m_AICFPPlayer.FindComponent(SCR_PlayerControllerGroupComponent)).AddAIToSlaveGroup(SCR_ChimeraCharacter.Cast(m_AICFPTarget), group);
			AICF_PresetRespawnProbe.Check("recruit_in_native_slave", group.IsAIControlledCharacterMember(SCR_ChimeraCharacter.Cast(m_AICFPTarget)));
			SCR_CharacterDamageManagerComponent.Cast(body.FindComponent(SCR_CharacterDamageManagerComponent)).Kill(Instigator.CreateInstigatorGM());
			m_iAICFPPhase = 2;
			return;
		}
		if (m_iAICFPPhase == 2 && body == m_AICFPTarget && AICF_GroupRuntime.IsAliveCharacter(body))
		{
			AICF_PresetRespawnProbe.Check("server_controls_recruit", true);
			AICF_PresetRespawnProbe.Check("preset_retained_after_takeover", arsenal.GetPlayerArsenalLoadout(SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId), saved) && saved.loadout == m_sAICFPSaved);
			m_iAICFPAt = System.GetTickCount();
			m_iAICFPPhase = 3;
		}
		if (m_iAICFPPhase == 3 && System.GetTickCount() - m_iAICFPAt > 10000)
		{
			SCR_CharacterDamageManagerComponent.Cast(body.FindComponent(SCR_CharacterDamageManagerComponent)).Kill(Instigator.CreateInstigatorGM());
			m_iAICFPPhase = 4;
			return;
		}
		if (m_iAICFPPhase == 4 && body != m_AICFPTarget && AICF_GroupRuntime.IsAliveCharacter(body))
		{
			array<string> actual = {};
			AICF_PresetRespawnProbe.Inventory(body, actual);
			bool same = actual.Count() == m_aAICFPInventory.Count();
			for (int i = 0; i < actual.Count() && same; i++) same = actual[i] == m_aAICFPInventory[i];
			AICF_PresetRespawnProbe.Check("new_character_at_base", vector.DistanceXZ(body.GetOrigin(), AICF_PresetRespawnProbe.Point().GetOrigin()) < 100);
			AICF_PresetRespawnProbe.Check("saved_preset_selected_on_server", SCR_PlayerArsenalLoadout.Cast(SCR_PlayerLoadoutComponent.Cast(m_AICFPPlayer.FindComponent(SCR_PlayerLoadoutComponent)).GetLoadout()) != null);
			AICF_PresetRespawnProbe.Check("saved_inventory_prefabs_and_counts_restored", same);
			AICF_PresetRespawnProbe.Check("preset_retained_after_base_respawn", arsenal.GetPlayerArsenalLoadout(SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId), saved) && saved.loadout == m_sAICFPSaved);
			m_iAICFPPhase = 5;
			m_iAICFPAt = System.GetTickCount();
		}
	}
	protected void AICF_PresetClient()
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		IEntity body = player.GetControlledEntity();
		if (m_iAICFPPhase == 0)
		{
			if (AICF_GroupRuntime.IsAliveCharacter(body)) { m_iAICFPPhase = 1; return; }
			SCR_CampaignFaction faction = AICF_PresetRespawnProbe.ResolveFaction();
			SCR_SpawnPoint point = AICF_PresetRespawnProbe.Point();
			if (!faction || !point) return;
			SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
			if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return; }
			if (!SCR_GroupsManagerComponent.GetInstance().GetPlayerGroup(player.GetPlayerId())) { SCR_PlayerControllerGroupComponent.Cast(player.FindComponent(SCR_PlayerControllerGroupComponent)).RequestCreateGroup(); return; }
			SCR_BasePlayerLoadout loadout = AICF_PresetRespawnProbe.Loadout(false);
			if (!loadout) return;
			SCR_PlayerLoadoutComponent component = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
			if (component.GetLoadout() != loadout) { component.RequestLoadout(loadout); return; }
			SCR_RespawnComponent.Cast(player.GetRespawnComponent()).RequestSpawn(new SCR_SpawnPointSpawnData(loadout.GetLoadoutResource(), point.GetRplId()));
			return;
		}
		if (m_iAICFPPhase == 1 && !AICF_GroupRuntime.IsAliveCharacter(body))
		{
			player.AICF_RequestSquadRespawnList();
			if (player.m_aAICFSquadRespawnIds.IsEmpty()) return;
			SCR_DeployMenuMain menu = SCR_DeployMenuMain.GetDeployMenu();
			if (!menu) return;
			m_AICFPTargetRpl = player.m_aAICFSquadRespawnIds[0];
			menu.AICF_ProbeTake(m_AICFPTargetRpl);
		}
		if (m_iAICFPPhase == 1 && AICF_GroupRuntime.IsAliveCharacter(body) && m_AICFPTargetRpl.IsValid())
		{
			AICF_PresetRespawnProbe.Check("owner_controls_same_recruit", RplComponent.Cast(body.FindComponent(RplComponent)).Id() == m_AICFPTargetRpl);
			AICF_PresetRespawnProbe.Check("overlap_exercised", AICF_PresetRespawnProbe.s_iOverlap > 0);
			m_iAICFPPhase = 2;
		}
		if (m_iAICFPPhase == 2 && !AICF_GroupRuntime.IsAliveCharacter(body))
		{
			SCR_DeployMenuMain menu = SCR_DeployMenuMain.GetDeployMenu();
			if (!menu) return;
			menu.AICF_ProbeSavedAtBase();
			m_iAICFPPhase = 3;
		}
		if (m_iAICFPPhase == 3)
		{
			if (!AICF_GroupRuntime.IsAliveCharacter(body))
			{
				SCR_DeployMenuMain menu = SCR_DeployMenuMain.GetDeployMenu();
				if (menu) menu.AICF_ProbeSavedAtBase();
				return;
			}
			AICF_PresetRespawnProbe.Check("owner_controls_new_base_character", RplComponent.Cast(body.FindComponent(RplComponent)).Id() != m_AICFPTargetRpl);
			m_iAICFPPhase = 4;
			m_iAICFPAt = System.GetTickCount();
		}
		if (m_iAICFPPhase == 4 && System.GetTickCount() - m_iAICFPAt > 8000)
		{
			AICF_PresetRespawnProbe.Check("owner_saved_loadout_selected", SCR_PlayerArsenalLoadout.Cast(SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent)).GetLoadout()) != null);
			AICF_PresetClose();
		}
	}
	protected void AICF_PresetClose()
	{
		m_bAICFPEnding = true;
		GetGame().GetCallqueue().Remove(AICF_PresetTick);
		Print(string.Format("[AICF][PRESET_RESPAWN_PROBE] finished=1 server=%1 phase=%2 failures=%3", Replication.IsServer(), m_iAICFPPhase, AICF_PresetRespawnProbe.s_iFailures));
		GetGame().RequestClose();
	}
	void ~SCR_GameModeCampaign()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_PresetTick);
	}
}
