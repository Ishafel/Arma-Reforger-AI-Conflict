// Isolated stage: реальные corpses и остановленный native movement имитируют
// недоступное тело. Timeout течёт в реальном времени; arrival не подменяется.
modded class AICF_MatchController
{
	protected int m_iEQSearchStarted;
	protected int m_iEQSearchPhase;
	protected int m_iEQSearchChecks;
	protected int m_iEQSearchFailures;
	protected ref AICF_EquipmentOrder m_EQSearchOrder;
	protected string m_sEQSearchInventory;
	protected ref array<IEntity> m_aEQSearchCorpses = {};

	protected void AICF_EQSearchCheck(string name, bool passed)
	{
		m_iEQSearchChecks++;
		if (!passed) m_iEQSearchFailures++;
		Print(string.Format("[AICF][EQUIPMENT_SEARCH] case=%1 passed=%2 phase=%3", name, passed, m_iEQSearchPhase));
	}

	protected void AICF_EQSearchClose()
	{
		CharacterControllerComponent.Cast(m_EquipmentProbeState.GetSlot(0).GetRosterMember(0).FindComponent(CharacterControllerComponent)).SetDisableMovementControls(false);
		m_EquipmentProbeService.Stop();
		Print(string.Format("[AICF][EQUIPMENT_SEARCH_FINISHED] checks=%1 failures=%2 phase=%3", m_iEQSearchChecks, m_iEQSearchFailures, m_iEQSearchPhase));
		m_iEQSearchPhase = 99;
		GetGame().RequestClose();
	}

	override protected void Update()
	{
		super.Update();
		string mode;
		if (!System.GetCLIParam("aicfEquipmentProbe", mode) || mode != "search" || !m_bRosterReady || m_iEQSearchPhase == 99) return;
		int now = System.GetTickCount();
		if (!m_iEQSearchStarted)
		{
			m_iEQSearchStarted = now;
			m_EquipmentService.Stop();
			m_Construction.Stop();
			m_EquipmentProbeState = new AICF_FactionState(m_USFaction.GetFactionKey(), m_Config);
			m_EquipmentProbeService = new AICF_EquipmentService(m_Campaign, m_OrderPlanner, m_ObjectiveGraph, m_TargetSelector, m_GroupSpawner);
			vector position = m_USFaction.GetMainBase().GetSpawnPoint().GetOrigin();
			AICF_EQSpawn(m_USFaction, 0, position);
			AICF_EQSpawn(m_USFaction, 2, position + "25 0 0");
			AICF_EQSpawn(m_USFaction, 3, position + "25 0 0");
			AICF_EQSpawn(m_USFaction, 4, position + "90 0 0");
			return;
		}
		if (now - m_iEQSearchStarted > 180000) { AICF_EQSearchCheck("TIMEOUT", false); AICF_EQSearchClose(); return; }
		AICF_GroupSlot slot = m_EquipmentProbeState.GetSlot(0);
		if (m_iEQSearchPhase == 0)
		{
			foreach (SCR_AIGroup group : m_aEquipmentProbeGroups) if (group.GetAgentsCount() != 1) return;
			array<int> slots = {0, 2, 3, 4};
			foreach (int index : slots)
			{
				AICF_GroupSlot ready = m_EquipmentProbeState.GetSlot(index);
				ready.MarkReady();
				ready.RecordDeploymentMembers();
				int active, recovered;
				m_ManagedAILODPolicy.KeepCaptureEligible(ready.GetGroup(), active, recovered);
				m_OrderPlanner.AssignPlayerPointOrder(ready, m_USFaction, ready.GetRosterMember(0).GetOrigin());
				if (index > 0)
				{
					IEntity corpse = ready.GetRosterMember(0);
					m_aEQSearchCorpses.Insert(corpse);
					SCR_CharacterDamageManagerComponent.Cast(corpse.FindComponent(SCR_CharacterDamageManagerComponent)).Kill(Instigator.CreateInstigatorGM());
				}
			}
			AICF_EQDeplete(slot.GetRosterMember(0));
			CharacterControllerComponent.Cast(slot.GetRosterMember(0).FindComponent(CharacterControllerComponent)).SetDisableMovementControls(true);
			m_sEQSearchInventory = AICF_EQInventoryIdentity(slot.GetRosterMember(0));
			m_iEQSearchPhase = 1;
			return;
		}
		m_EquipmentProbeService.Update(m_EquipmentProbeState, m_USFaction, null, null, null, true);
		AICF_EquipmentSlot record = m_EquipmentProbeService.AICF_ProbeRecord(slot);
		if (!record || record.m_aMembers.IsEmpty()) return;
		if (m_iEQSearchPhase == 1 && record.m_Order)
		{
			m_EQSearchOrder = record.m_Order;
			IEntity a = m_aEQSearchCorpses[0];
			IEntity b = m_aEQSearchCorpses[1];
			AICF_EQSearchCheck("INSIDE_SEARCH_RADIUS", record.m_Order.m_Corpse == a || record.m_Order.m_Corpse == b);
			bool ab = AICF_EquipmentConfig.Prefer(10, a.GetID(), 10, b.GetID());
			bool ba = AICF_EquipmentConfig.Prefer(10, b.GetID(), 10, a.GetID());
			AICF_EQSearchCheck("STABLE_ID_TIE_BREAK", ab != ba && ab == (a.GetID().ToString().Compare(b.GetID().ToString()) < 0));
			AICF_EquipmentOrder bounded = new AICF_EquipmentOrder();
			bounded.m_Group = slot.GetGroup();
			bounded.m_aMembers.Insert(record.m_aMembers[0]);
			bounded.m_aRejected.Insert(a.GetID());
			bounded.m_aRejected.Insert(b.GetID());
			AICF_EQSearchCheck("OUTSIDE_RADIUS_NOT_SELECTED", !m_EquipmentProbeService.AICF_ProbeCorpse(bounded));
			m_iEQSearchPhase = 2;
		}
		if (m_iEQSearchPhase == 2 && !record.m_Order)
		{
			AICF_EQSearchCheck("REAL_CORPSE_TIMEOUT", now >= m_EQSearchOrder.m_iDeadline && record.m_aMembers[0].m_bPending && m_EQSearchOrder.m_aRejected.Contains(m_EQSearchOrder.m_CorpseId));
			AICF_EQSearchCheck("NO_DISTANT_LOOT_OR_STUCK_SLOT", AICF_EQInventoryIdentity(slot.GetRosterMember(0)) == m_sEQSearchInventory && !slot.IsResupplyingEquipment() && record.m_iRetryAt > now);
			int retry = record.m_iRetryAt;
			m_EquipmentProbeService.AICF_ProbeTick(record, retry - 1);
			AICF_EQSearchCheck("SEARCH_RETRY_BOUNDED", !record.m_Order && record.m_iRetryAt == retry);
			AICF_EQSearchClose();
		}
	}
}
