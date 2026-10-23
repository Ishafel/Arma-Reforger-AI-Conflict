// Isolated stage: real owner RPC, saved personal preset and native player spawn.
modded class SCR_GameModeCampaign
{
	protected int m_iAICFQuickslotNetworkStart;
	protected int m_iAICFQuickslotNetworkPhase;
	protected int m_iAICFQuickslotNetworkToken;
	protected int m_iAICFQuickslotNetworkAt;
	protected int m_iAICFQuickslotNetworkChecks;
	protected int m_iAICFQuickslotNetworkFailures;
	protected bool m_bAICFQuickslotNetworkClient;
	protected ResourceName m_sAICFQuickslotSVD;

	override void OnGameStart()
	{
		super.OnGameStart();
		if (!System.IsCLIParam("aicfQuickslotNetworkProbe")) return;
		m_iAICFQuickslotNetworkStart = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_QuickslotNetworkTick, 1000, true);
	}
	protected void AICF_QuickslotNetworkCheck(string name, bool passed)
	{
		m_iAICFQuickslotNetworkChecks++;
		if (!passed) m_iAICFQuickslotNetworkFailures++;
		Print(string.Format("[AICF][QUICKSLOT_NETWORK_CASE] case=%1 pass=%2", name, passed));
	}
	protected void AICF_QuickslotNetworkClose()
	{
		Print(string.Format("[AICF][QUICKSLOT_NETWORK_DONE] checks=%1 failed=%2", m_iAICFQuickslotNetworkChecks, m_iAICFQuickslotNetworkFailures));
		GetGame().GetCallqueue().Remove(AICF_QuickslotNetworkTick);
		GetGame().RequestClose();
	}
	protected AICF_LoadoutRecipe AICF_QuickslotNetworkRecipe(SCR_PlayerController player, SCR_CampaignFaction faction)
	{
		AICF_LoadoutRecipe recipe = AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData());
		if (!recipe) return null;
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		catalog.SetPersonalRules();
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		string reason;
		if (!draft.Build(recipe, catalog, reason)) return null;
		IEntity character = draft.GetCharacter();
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		array<WeaponSlotComponent> slots = {};
		weapons.GetWeaponsSlots(slots);
		BaseInventoryStorageComponent target;
		int targetSlot = -1;
		foreach (WeaponSlotComponent slot : slots)
		{
			if (slot.GetWeaponSlotIndex() != 1 || slot.GetWeaponSlotType() != "primary" || !slot.GetSlotInfo()) continue;
			target = slot.GetSlotInfo().GetStorage();
			targetSlot = slot.GetSlotInfo().GetID();
		}
		if (!target) return null;
		array<string> prefabs = {};
		catalog.List(0, prefabs, SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS);
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		foreach (string prefab : prefabs)
		{
			string lower = prefab;
			lower.ToLower();
			if (!lower.Contains("svd") || !inventory.CanInsertResourceInStorage(prefab, target, targetSlot)) continue;
			m_sAICFQuickslotSVD = prefab;
			break;
		}
		if (m_sAICFQuickslotSVD.IsEmpty()) return null;
		recipe.Add("", AICF_LoadoutInventory.StorageId(character, target), targetSlot, m_sAICFQuickslotSVD, 1);
		recipe.m_sName = "SVD second weapon native RPC probe";
		draft.Clear();
		return recipe;
	}
	protected void AICF_QuickslotNetworkTick()
	{
		int now = System.GetTickCount();
		if (System.GetTickCount(m_iAICFQuickslotNetworkStart) > 240000)
		{
			AICF_QuickslotNetworkCheck("TIMEOUT", false);
			AICF_QuickslotNetworkClose();
			return;
		}
		if (Replication.IsServer())
		{
			int players = GetGame().GetPlayerManager().GetPlayerCount();
			if (players > 0) m_bAICFQuickslotNetworkClient = true;
			if (m_bAICFQuickslotNetworkClient && players == 0)
			{
				Print("[AICF][QUICKSLOT_NETWORK_SERVER_STOP] disconnected=1");
				AICF_QuickslotNetworkClose();
			}
			return;
		}
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player) return;
		FactionKey key = AICF_ContentProfile.GetActive().GetRuntimeFactionKey("USSR");
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(key));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!faction || !affiliation) return;
		if (affiliation.GetAffiliatedFaction() != faction) { affiliation.RequestFaction(faction); return; }
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!groups || !groups.GetPlayerGroup(player.GetPlayerId())) { SCR_PlayerControllerGroupComponent.Cast(player.FindComponent(SCR_PlayerControllerGroupComponent)).RequestCreateGroup(); return; }
		array<ref SCR_BasePlayerLoadout> loadouts = {};
		GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(faction, loadouts);
		SCR_BasePlayerLoadout loadout;
		foreach (SCR_BasePlayerLoadout candidate : loadouts)
			if (!SCR_PlayerArsenalLoadout.Cast(candidate)) { loadout = candidate; break; }
		SCR_PlayerLoadoutComponent component = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
		if (!loadout || !component) return;
		if (!player.GetControlledEntity() && component.GetLoadout() != loadout) { component.RequestLoadout(loadout); return; }
		if (m_iAICFQuickslotNetworkToken)
		{
			if (player.AICF_LoadoutToken() != m_iAICFQuickslotNetworkToken) return;
			AICF_QuickslotNetworkCheck("RPC_" + m_iAICFQuickslotNetworkPhase.ToString(), player.AICF_LoadoutAccepted());
			Print("[AICF][QUICKSLOT_NETWORK_STATUS] " + player.AICF_LoadoutStatus());
			m_iAICFQuickslotNetworkToken = 0;
			if (!player.AICF_LoadoutAccepted()) { AICF_QuickslotNetworkClose(); return; }
			m_iAICFQuickslotNetworkPhase++;
		}
		if (m_iAICFQuickslotNetworkPhase == 0)
		{
			m_iAICFQuickslotNetworkToken = player.AICF_RequestLoadout(AICF_PersonalLoadout.SLOT, 0, player.AICF_LoadoutRevision(), 0, "");
			return;
		}
		if (m_iAICFQuickslotNetworkPhase == 1)
		{
			AICF_LoadoutRecipe recipe = AICF_QuickslotNetworkRecipe(player, faction);
			AICF_QuickslotNetworkCheck("RECIPE", recipe != null);
			if (!recipe) { AICF_QuickslotNetworkClose(); return; }
			m_iAICFQuickslotNetworkToken = player.AICF_RequestLoadout(AICF_PersonalLoadout.SLOT, 0, player.AICF_LoadoutRevision(), 1, recipe.Encode());
			return;
		}
		if (m_iAICFQuickslotNetworkPhase == 2)
		{
			AICF_QuickslotNetworkCheck("SAVED_SELECTED", player.AICF_PersonalAvailable() && player.AICF_PersonalSelected());
			SCR_SpawnPoint chosen;
			float nearest = float.MAX;
			foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPointsForFaction(key))
			{
				if (!point || !point.IsSpawnPointActive()) continue;
				float distance = vector.DistanceXZ(point.GetOrigin(), faction.GetMainBase().GetOwner().GetOrigin());
				if (distance >= nearest) continue;
				nearest = distance;
				chosen = point;
			}
			if (!chosen) return;
			SCR_RespawnComponent.Cast(player.GetRespawnComponent()).RequestSpawn(new SCR_SpawnPointSpawnData(loadout.GetLoadoutResource(), chosen.GetRplId()));
			m_iAICFQuickslotNetworkPhase = 3;
			m_iAICFQuickslotNetworkAt = now;
			return;
		}
		IEntity character = player.GetControlledEntity();
		if (!AICF_GroupRuntime.IsAliveCharacter(character) || System.GetTickCount(m_iAICFQuickslotNetworkAt) < 5000) return;
		SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		IEntity secondary = storage.GetItemFromQuickSlot(1);
		if (m_iAICFQuickslotNetworkPhase == 3)
		{
			bool svd = secondary && SCR_ResourceNameUtils.GetPrefabName(secondary) == m_sAICFQuickslotSVD;
			AICF_QuickslotNetworkCheck("DEPLOYED_SVD_QUICKSLOT", svd);
			if (!svd) { AICF_QuickslotNetworkClose(); return; }
			AICF_QuickslotNetworkCheck("USE_SECONDARY", storage.UseItem(secondary, ESlotFunction.TYPE_WEAPON));
			m_iAICFQuickslotNetworkPhase = 4;
			m_iAICFQuickslotNetworkAt = now;
			return;
		}
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		AICF_QuickslotNetworkCheck("SECONDARY_EQUIPPED", secondary && controller.GetCurrentItemInHands() == secondary);
		AICF_QuickslotNetworkClose();
	}
	void ~SCR_GameModeCampaign()
	{
		GetGame().GetCallqueue().Remove(AICF_QuickslotNetworkTick);
	}
}
