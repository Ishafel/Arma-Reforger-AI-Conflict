// Isolated stage с общим EquipmentProbe; client подключается только canonical launcher.
modded class AICF_MatchController
{
	protected int m_iEQPlayerStarted;
	protected int m_iEQPlayerPhase;
	protected int m_iEQPlayerChecks;
	protected int m_iEQPlayerFailures;
	protected int m_iEQPlayerAt;
	protected SCR_PlayerController m_EQPlayer;
	protected ref AICF_EquipmentMember m_EQPlayerMember;
	protected string m_sEQPlayerInventory;
	protected int m_iEQPlayerRestoreCalls;
	protected int m_iEQPlayerRounds;
	protected ref array<IEntity> m_aEQPlayerItems = {};
	protected ref array<string> m_aEQPlayerIds = {};

	protected void AICF_EQPlayerCheck(string name, bool passed)
	{
		m_iEQPlayerChecks++;
		if (!passed) m_iEQPlayerFailures++;
		Print(string.Format("[AICF][EQUIPMENT_PLAYER] case=%1 passed=%2 phase=%3", name, passed, m_iEQPlayerPhase));
	}

	protected void AICF_EQPlayerClose()
	{
		if (m_EquipmentProbeService) m_EquipmentProbeService.Stop();
		Print(string.Format("[AICF][EQUIPMENT_PLAYER_FINISHED] checks=%1 failures=%2 phase=%3", m_iEQPlayerChecks, m_iEQPlayerFailures, m_iEQPlayerPhase));
		m_iEQPlayerPhase = 99;
		GetGame().RequestClose();
	}

	override protected void Update()
	{
		super.Update();
		string mode;
		if (!System.GetCLIParam("aicfEquipmentProbe", mode) || mode != "player" || !m_bRosterReady || m_iEQPlayerPhase == 99) return;
		int now = System.GetTickCount();
		if (!m_iEQPlayerStarted)
		{
			m_iEQPlayerStarted = now;
			m_EquipmentService.Stop();
			m_Construction.Stop();
			m_EquipmentProbeState = new AICF_FactionState(m_USFaction.GetFactionKey(), m_Config);
			m_EquipmentProbeService = new AICF_EquipmentService(m_Campaign, m_OrderPlanner, m_ObjectiveGraph, m_TargetSelector, m_GroupSpawner);
			vector position = m_USFaction.GetMainBase().GetSpawnPoint().GetOrigin() + "65 0 20";
			AICF_EQPlaceArmory(m_USFaction.GetMainBase(), position);
			AICF_EQSpawn(m_USFaction, 0, position);
			AICF_EQSpawn(m_USFaction, 2, position + "20 0 0");
			return;
		}
		if (now - m_iEQPlayerStarted > 300000)
		{
			AICF_EQPlayerCheck("TIMEOUT", false);
			AICF_EQPlayerClose();
			return;
		}
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aEquipmentProbeBuildings)
			if (building && !building.IsCompositionSpawned() && building.GetCompositionLayout()) building.GetCompositionLayout().AddBuildingValue(building.GetCompositionLayout().GetToBuildValue());
		AICF_GroupSlot slot = m_EquipmentProbeState.GetSlot(0);
		if (m_iEQPlayerPhase == 0)
		{
			if (!slot.GetGroup() || slot.GetGroup().GetAgentsCount() != 1 || m_EquipmentProbeState.GetSlot(2).GetGroup().GetAgentsCount() != 1) return;
			array<int> players = {};
			GetGame().GetPlayerManager().GetPlayers(players);
			foreach (int id : players) m_EQPlayer = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
			if (!m_EQPlayer) return;
			foreach (int index : {0, 2})
			{
				AICF_GroupSlot ready = m_EquipmentProbeState.GetSlot(index);
				ready.MarkReady();
				ready.RecordDeploymentMembers();
				int active, recovered;
				m_ManagedAILODPolicy.KeepCaptureEligible(ready.GetGroup(), active, recovered);
				m_OrderPlanner.AssignPlayerPointOrder(ready, m_USFaction, ready.GetRosterMember(0).GetOrigin());
			}
			m_EquipmentProbeCorpse = m_EquipmentProbeState.GetSlot(2).GetRosterMember(0);
			m_EQPlayer.SetInitialMainEntity(m_EquipmentProbeCorpse);
			m_iEQPlayerPhase = 1;
			return;
		}
		if (m_iEQPlayerPhase == 1)
		{
			AICF_EQPlayerCheck("CONNECTED_PLAYER_CONTROL", m_EQPlayer.GetControlledEntity() == m_EquipmentProbeCorpse && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(m_EquipmentProbeCorpse) == m_EQPlayer.GetPlayerId());
			AICF_EQPlayerCheck("LIVE_PLAYER_SOURCE_REFUSED", !AICF_EquipmentInventory.Corpse(m_EquipmentProbeCorpse));
			SCR_CharacterDamageManagerComponent.Cast(m_EquipmentProbeCorpse.FindComponent(SCR_CharacterDamageManagerComponent)).Kill(Instigator.CreateInstigatorGM());
			m_iEQPlayerPhase = 2;
			return;
		}
		if (m_iEQPlayerPhase == 2)
		{
			if (AICF_GroupRuntime.IsAliveCharacter(m_EquipmentProbeCorpse)) return;
			AICF_EQPlayerCheck("DEAD_PLAYER_MAIN_SOURCE_REFUSED", SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(m_EquipmentProbeCorpse) == m_EQPlayer.GetPlayerId() && !AICF_EquipmentInventory.Corpse(m_EquipmentProbeCorpse));
			AICF_EQBinding(slot, "player-takeover");
			m_iEQPlayerPhase = 3;
		}
		m_EquipmentProbeService.Update(m_EquipmentProbeState, m_USFaction, null, null, null, true);
		AICF_EquipmentSlot record = m_EquipmentProbeService.AICF_ProbeRecord(slot);
		if (m_iEQPlayerPhase == 3 && record && !record.m_aMembers.IsEmpty())
		{
			AICF_EquipmentMember member = record.m_aMembers[0];
			if (!member.m_Transaction || member.m_Transaction.m_iIndex < 2) return;
			m_EQPlayerMember = member;
			m_sEQPlayerInventory = AICF_EQInventoryIdentity(member.m_Character);
			AICF_EquipmentInventory.Manager(member.m_Character).GetItems(m_aEQPlayerItems);
			foreach (IEntity item : m_aEQPlayerItems) m_aEQPlayerIds.Insert(item.GetID().ToString());
			m_iEQPlayerRestoreCalls = AICF_EquipmentApplicator.s_iAICFRestoreCalls;
			m_iEQPlayerRounds = AICF_EquipmentInventory.Rounds(member.m_Character, AICF_EquipmentInventory.Weapon(member.m_Character, false));
			m_EQPlayer.SetInitialMainEntity(member.m_Character);
			Print("[AICF][EQUIPMENT_PLAYER_INVENTORY] phase=BEFORE " + m_sEQPlayerInventory);
			m_iEQPlayerAt = now;
			m_iEQPlayerPhase = 4;
			return;
		}
		if (m_iEQPlayerPhase == 4 && now - m_iEQPlayerAt > 5000)
		{
			AICF_EQPlayerCheck("PLAYER_TAKEOVER_CANCELS", m_EQPlayer.GetControlledEntity() == m_EQPlayerMember.m_Character && !m_EQPlayerMember.Current() && !slot.IsResupplyingEquipment() && !slot.HasEquipmentInventoryTransaction());
			InventoryStorageManagerComponent playerInventory = AICF_EquipmentInventory.Manager(m_EQPlayerMember.m_Character);
			array<IEntity> actualItems = {};
			array<string> actualIds = {};
			playerInventory.GetItems(actualItems);
			foreach (IEntity actualItem : actualItems) actualIds.Insert(actualItem.GetID().ToString());
			bool retained = true;
			foreach (string originalId : m_aEQPlayerIds) if (!actualIds.Contains(originalId)) retained = false;
			// Native player initialization добавляет персональный radio gadget.
			// Проверяем все исходные identities и запрет stale RestoreNew отдельно.
			AICF_EQPlayerCheck("PLAYER_INVENTORY_NO_STALE_MUTATION", retained && AICF_EquipmentApplicator.s_iAICFRestoreCalls == m_iEQPlayerRestoreCalls &&
				AICF_EquipmentInventory.Rounds(m_EQPlayerMember.m_Character, AICF_EquipmentInventory.Weapon(m_EQPlayerMember.m_Character, false)) == m_iEQPlayerRounds &&
				slot.GetMemberLoadoutRevision(0) != slot.GetLoadout(0).m_iRevision && !CharacterControllerComponent.Cast(m_EQPlayerMember.m_Character.FindComponent(CharacterControllerComponent)).GetDisableMovementControls());
			Print("[AICF][EQUIPMENT_PLAYER_INVENTORY] phase=AFTER " + AICF_EQInventoryIdentity(m_EQPlayerMember.m_Character));
			Print(string.Format("[AICF][EQUIPMENT_PLAYER_FENCE] applied=%1 revision=%2 locked=%3", slot.GetMemberLoadoutRevision(0), slot.GetLoadout(0).m_iRevision, CharacterControllerComponent.Cast(m_EQPlayerMember.m_Character.FindComponent(CharacterControllerComponent)).GetDisableMovementControls()));
			AICF_EQPlayerClose();
		}
	}
}
