// Isolated stage вместе с AICF_EquipmentProbe.c. Реальные service entities,
// смена faction, удаление службы, native смерть и replacement roster.
modded class AICF_MatchController
{
	protected int m_iEQLifeStarted;
	protected int m_iEQLifePhase;
	protected int m_iEQLifeChecks;
	protected int m_iEQLifeFailures;
	protected int m_iEQLifeRetry;
	protected SCR_ServicePointComponent m_EQLifeOldService;
	protected EntityID m_EQLifeOldServiceId;
	protected ref AICF_EquipmentOrder m_EQLifeStale;
	protected ref AICF_EquipmentMember m_EQLifeOldMember;
	protected SCR_AIGroup m_EQLifeReplacement;

	protected void AICF_EQLifeCheck(string name, bool passed)
	{
		m_iEQLifeChecks++;
		if (!passed) m_iEQLifeFailures++;
		Print(string.Format("[AICF][EQUIPMENT_LIFECYCLE] case=%1 passed=%2 phase=%3", name, passed, m_iEQLifePhase));
	}

	protected void AICF_EQLifeClose()
	{
		if (m_EquipmentProbeService) m_EquipmentProbeService.Stop();
		foreach (SCR_AIGroup group : m_aEquipmentProbeGroups)
		{
			if (!group) continue;
			group.DespawnMembers();
			RplComponent.DeleteRplEntity(group, false);
		}
		Print(string.Format("[AICF][EQUIPMENT_LIFECYCLE_FINISHED] checks=%1 failures=%2 phase=%3", m_iEQLifeChecks, m_iEQLifeFailures, m_iEQLifePhase));
		m_iEQLifePhase = 99;
		GetGame().RequestClose();
	}

	override protected void Update()
	{
		super.Update();
		string mode;
		if (!System.GetCLIParam("aicfEquipmentProbe", mode) || mode != "lifecycle" || !m_bRosterReady || m_iEQLifePhase == 99) return;
		int now = System.GetTickCount();
		if (!m_iEQLifeStarted)
		{
			m_iEQLifeStarted = now;
			m_EquipmentService.Stop();
			m_Construction.Stop();
			m_EquipmentProbeState = new AICF_FactionState(m_USFaction.GetFactionKey(), m_Config);
			m_EquipmentProbeService = new AICF_EquipmentService(m_Campaign, m_OrderPlanner, m_ObjectiveGraph, m_TargetSelector, m_GroupSpawner);
			vector position = m_USFaction.GetMainBase().GetSpawnPoint().GetOrigin();
			AICF_EQPlaceArmory(m_USFaction.GetMainBase(), position + "65 0 20");
			AICF_EQPlaceArmory(m_USFaction.GetMainBase(), position + "100 0 40");
			AICF_EQSpawn(m_USFaction, 0, position);
			return;
		}
		if (now - m_iEQLifeStarted > 240000)
		{
			AICF_EQLifeCheck("TIMEOUT", false);
			AICF_EQLifeClose();
			return;
		}
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aEquipmentProbeBuildings)
			if (building && !building.IsCompositionSpawned() && building.GetCompositionLayout()) building.GetCompositionLayout().AddBuildingValue(building.GetCompositionLayout().GetToBuildValue());
		AICF_GroupSlot slot = m_EquipmentProbeState.GetSlot(0);
		if (m_iEQLifePhase == 0)
		{
			if (!slot.GetGroup() || slot.GetGroup().GetAgentsCount() != 1) return;
			slot.MarkReady();
			slot.RecordDeploymentMembers();
			int active, recovered;
			m_ManagedAILODPolicy.KeepCaptureEligible(slot.GetGroup(), active, recovered);
			m_OrderPlanner.AssignPlayerPointOrder(slot, m_USFaction, slot.GetRosterMember(0).GetOrigin());
			AICF_EQBinding(slot, "lifecycle");
			AICF_InfantryRecruitmentOrder recruit = new AICF_InfantryRecruitmentOrder();
			recruit.m_Slot = slot;
			recruit.m_Group = slot.GetGroup();
			recruit.m_GroupId = slot.GetGroup().GetID();
			recruit.m_Faction = m_USFaction;
			recruit.m_iGeneration = slot.GetSpawnGeneration();
			recruit.m_iAssignment = slot.GetStrategicAssignmentRevision();
			recruit.m_iIntent = slot.GetStrategicIntentRevision();
			recruit.m_Waypoint = slot.GetWaypoint();
			recruit.m_bPlayerRequested = true;
			slot.SetRecruitmentOrder(recruit);
			AICF_EQLifeCheck("ACTIVE_RECRUITMENT_FENCES_SERVICE", recruit.IsCurrent(slot) && !m_OrderPlanner.CanEquipmentService(slot, m_USFaction));
			slot.SetRecruitmentOrder(null);
			AICF_EQLifeCheck("ACTIVE_TRANSPORT_FENCES_SERVICE", m_VehicleCoordinator.AICF_ProbeActiveTripFence(m_EquipmentProbeService, m_EquipmentProbeState, m_USFaction, slot));
			m_iEQLifePhase = 1;
		}
		if (m_iEQLifePhase == 6 && m_EQLifeReplacement && m_EQLifeReplacement.GetAgentsCount() == 1)
		{
			AICF_EQLifeCheck("DEAD_MEMBER_REJECTED", !m_EQLifeOldMember.Current() && !AICF_GroupRuntime.IsAliveCharacter(m_EQLifeOldMember.m_Character));
			slot.MarkReady();
			slot.RecordDeploymentMembers();
			AICF_EQLifeCheck("REPLACEMENT_STALE_REJECTED", !m_EQLifeOldMember.Current() && !m_EQLifeStale.Current() && slot.GetGroup() != m_EQLifeStale.m_Group);
			string replacementBefore = AICF_EQInventoryIdentity(slot.GetRosterMember(0));
			m_EquipmentProbeService.Stop();
			m_EquipmentProbeService.Update(m_EquipmentProbeState, m_USFaction, null, null, null, true);
			AICF_EQLifeCheck("STOP_CLEARS_AND_NO_MUTATIONS", m_EquipmentProbeService.AICF_ProbeStopped() && AICF_EQInventoryIdentity(slot.GetRosterMember(0)) == replacementBefore);
			m_iEQLifePhase = 7;
			AICF_EQLifeClose();
			return;
		}
		m_EquipmentProbeService.Update(m_EquipmentProbeState, m_USFaction, null, null, null, true);
		AICF_EquipmentSlot record = m_EquipmentProbeService.AICF_ProbeRecord(slot);
		if (!record || record.m_aMembers.IsEmpty()) return;
		AICF_EquipmentMember member = record.m_aMembers[0];
		if (m_iEQLifePhase == 1 && record.m_Order && record.m_Order.m_Service && !member.m_Transaction)
		{
			AICF_EquipmentOrder first = record.m_Order;
			AICF_EQLifeCheck("SOURCE_NEAREST_ARMORY", vector.Distance(first.m_vPosition, m_aEquipmentProbeBuildings[0].GetOwner().GetOrigin()) < 20);
			m_EQLifeOldService = first.m_Service;
			m_EQLifeOldServiceId = first.m_ServiceId;
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(m_EQLifeOldService.GetOwner().FindComponent(FactionAffiliationComponent));
			affiliation.SetAffiliatedFaction(m_USSRFaction);
			AICF_EQLifeCheck("OWNER_LOSS_REJECTED", !first.HasService());
			m_iEQLifePhase = 2;
			return;
		}
		if (m_iEQLifePhase == 2 && record.m_Order && record.m_Order.m_ServiceId != m_EQLifeOldServiceId)
		{
			AICF_EQLifeCheck("OWNER_LOSS_ALTERNATIVE", record.m_Order.HasService());
			FactionAffiliationComponent.Cast(m_EQLifeOldService.GetOwner().FindComponent(FactionAffiliationComponent)).SetAffiliatedFaction(m_USFaction);
			m_EQLifeOldServiceId = record.m_Order.m_ServiceId;
			IEntity disappearing = record.m_Order.m_Service.GetOwner();
			RplComponent.DeleteRplEntity(disappearing, false);
			AICF_EQLifeCheck("REMOVED_SERVICE_REJECTED", !record.m_Order.HasService());
			m_iEQLifePhase = 3;
			return;
		}
		if (m_iEQLifePhase == 3 && record.m_Order && record.m_Order.HasService() && record.m_Order.m_ServiceId != m_EQLifeOldServiceId)
		{
			AICF_EQLifeCheck("REMOVED_SERVICE_ALTERNATIVE", true);
			array<SCR_ServicePointComponent> services = {};
			m_USFaction.GetMainBase().GetServices(services);
			foreach (SCR_ServicePointComponent service : services)
				if (service && service.GetType() == SCR_EServicePointType.ARMORY && service.GetServiceState() == SCR_EServicePointStatus.ONLINE) service.SetServiceState(SCR_EServicePointStatus.OFFLINE);
			m_iEQLifePhase = 4;
			return;
		}
		if (m_iEQLifePhase == 4 && !record.m_Order)
		{
			AICF_EQLifeCheck("NO_ALTERNATIVE_NEED_RETAINED", member.m_bPending && now < record.m_iRetryAt && !slot.IsResupplyingEquipment());
			m_iEQLifeRetry = record.m_iRetryAt;
			AIWaypoint current = slot.GetWaypoint();
			m_EquipmentProbeService.AICF_ProbeTick(record, m_iEQLifeRetry - 1);
			AICF_EQLifeCheck("BOUNDED_RETRY_CLOCK", record.m_iRetryAt == m_iEQLifeRetry && !record.m_Order && slot.GetWaypoint() == current);
			array<SCR_ServicePointComponent> services = {};
			m_USFaction.GetMainBase().GetServices(services);
			foreach (SCR_ServicePointComponent service : services)
				if (service && service.GetType() == SCR_EServicePointType.ARMORY && service.GetFaction() == m_USFaction && service.GetServiceState() == SCR_EServicePointStatus.OFFLINE) service.SetServiceState(SCR_EServicePointStatus.ONLINE);
			record.m_iRetryAt = now;
			record.m_iEvaluateAt = now;
			m_iEQLifePhase = 5;
			return;
		}
		if (m_iEQLifePhase == 5 && record.m_Order)
		{
			m_EQLifeStale = record.m_Order;
			m_EQLifeOldMember = member;
			m_OrderPlanner.AssignPlayerPointOrder(slot, m_USFaction, member.m_Character.GetOrigin() + "15 0 0");
			AIWaypoint playerWaypoint = slot.GetWaypoint();
			m_EquipmentProbeService.Update(m_EquipmentProbeState, m_USFaction, null, null, null, true);
			AICF_EQLifeCheck("NEW_MANUAL_ORDER_PRESERVED", slot.GetWaypoint() == playerWaypoint && !m_EQLifeStale.Current());
			SCR_CharacterDamageManagerComponent.Cast(member.m_Character.FindComponent(SCR_CharacterDamageManagerComponent)).Kill(Instigator.CreateInstigatorGM());
			slot.MarkDestroyed();
			slot.BeginReinforcementWait(0);
			slot.BeginReplacementSpawn(now);
			m_EQLifeReplacement = m_GroupSpawner.SpawnGroup(m_USFaction, m_USFaction.GetMainBase(), 0, 1, false);
			m_aEquipmentProbeGroups.Insert(m_EQLifeReplacement);
			slot.BindSpawnedGroup(m_EQLifeReplacement);
			m_EQLifeReplacement.AICF_TrackMemberPositions();
			m_GroupSpawner.BeginRosterSpawn(m_EQLifeReplacement, 1);
			m_iEQLifePhase = 6;
			return;
		}
	}
}
