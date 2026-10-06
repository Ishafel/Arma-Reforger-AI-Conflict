// Только isolated stage: шесть реальных конкурирующих отрядов, штатные
// казармы, roster и транзакции. Supplies/стройка готовятся fixture.
modded class AICF_MatchController
{
	protected int m_iDemandProbeStarted;
	protected int m_iDemandProbeSample;
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aDemandProbeBuildings = {};
	protected ref array<EntityID> m_aDemandProbeGroups = {};

	override protected void Update()
	{
		string enabled;
		if (System.GetCLIParam("aicfRecruitmentSupplyProbe", enabled) && enabled == "1" && m_bRosterReady && !m_bStopped)
			AICF_DemandProbeUpdate();
		super.Update();
	}

	protected void AICF_DemandProbeUpdate()
	{
		int now = System.GetTickCount();
		if (!m_iDemandProbeStarted)
		{
			m_iDemandProbeStarted = now;
			m_Construction.Stop();
			for (int i = 0; i < 10; i++)
			{
				if (i < 3)
				{
					m_aDemandProbeGroups.Insert(m_USState.GetSlot(i).GetGroup().GetID());
					m_aDemandProbeGroups.Insert(m_USSRState.GetSlot(i).GetGroup().GetID());
				}
				else
				{
					m_USState.GetSlot(i).SetDesiredSize(1);
					m_USSRState.GetSlot(i).SetDesiredSize(1);
				}
			}
			AICF_DemandProbePlace(m_USFaction);
			AICF_DemandProbePlace(m_USSRFaction);
			Print("[AICF][RECRUITMENT_SUPPLY_PROBE] prepared=1 competing_slots_per_faction=3");
		}
		foreach (SCR_CampaignBuildingCompositionComponent composition : m_aDemandProbeBuildings)
		{
			if (composition && !composition.IsCompositionSpawned() && composition.GetCompositionLayout())
			{
				SCR_CampaignBuildingLayoutComponent layout = composition.GetCompositionLayout();
				layout.AddBuildingValue(layout.GetToBuildValue());
			}
		}
		int full;
		bool stable = true;
		for (int slotId = 0; slotId < 3; slotId++)
		{
			AICF_GroupSlot us = m_USState.GetSlot(slotId);
			AICF_GroupSlot ussr = m_USSRState.GetSlot(slotId);
			if (AICF_GroupRuntime.CountAliveAgents(us.GetGroup()) == 10 && !us.IsRecruitingInfantry()) full++;
			if (AICF_GroupRuntime.CountAliveAgents(ussr.GetGroup()) == 10 && !ussr.IsRecruitingInfantry()) full++;
			if (!us.GetGroup() || !ussr.GetGroup() || us.GetGroup().GetID() != m_aDemandProbeGroups[slotId * 2] ||
				ussr.GetGroup().GetID() != m_aDemandProbeGroups[slotId * 2 + 1]) stable = false;
		}
		if (now - m_iDemandProbeSample >= 10000)
		{
			m_iDemandProbeSample = now;
			Print(string.Format("[AICF][RECRUITMENT_SUPPLY_PROBE] full=%1 stable=%2 elapsed_ms=%3", full, stable, now - m_iDemandProbeStarted));
		}
		if (full == 6 || now - m_iDemandProbeStarted >= 300000)
		{
			Print(string.Format("[AICF][RECRUITMENT_SUPPLY_PROBE] finished=1 full=%1 stable=%2 demand_checks=%3", full, stable,
				m_InfantryRecruitment.AICF_DemandProbeComplete()));
			Stop(true);
			GetGame().RequestClose();
		}
	}

	protected void AICF_DemandProbePlace(SCR_CampaignFaction faction)
	{
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		if (!base || !base.GetMasterProvider() || !base.GetSpawnPoint()) return;
		base.AddSupplies(base.GetSuppliesMax() - base.GetSupplies());
		vector position, rotation;
		base.GetSpawnPoint().GetPositionAndRotation(position, rotation);
		position += "35 0 25";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab(
			AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()), AICF_EConstructionType.SMALL_BARRACKS);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (!entity) return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation) affiliation.SetAffiliatedFaction(faction);
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (!composition) return;
		composition.SetProviderEntity(base.GetMasterProvider().GetOwner());
		m_aDemandProbeBuildings.Insert(composition);
	}
}

modded class AICF_InfantryRecruitmentService
{
	protected bool m_bDemandProbeChecked;
	protected bool m_bDemandProbeReduced;
	protected int m_iDemandProbePassed;
	protected int m_iDemandProbePrevious;
	protected AICF_GroupSlot m_DemandProbeSlot;
	protected SCR_CampaignMilitaryBaseComponent m_DemandProbeBase;

	bool AICF_DemandProbeComplete()
	{
		return m_iDemandProbePassed == 15 && m_bDemandProbeReduced;
	}

	override void Update(AICF_FactionState us, SCR_CampaignFaction usFaction, AICF_FactionState ussr,
		SCR_CampaignFaction ussrFaction, int availableAgents, bool graphReady)
	{
		super.Update(us, usFaction, ussr, ussrFaction, availableAgents, graphReady);
		string enabled;
		if (!System.GetCLIParam("aicfRecruitmentSupplyProbe", enabled) || enabled != "1") return;
		if (m_bDemandProbeChecked)
		{
			int remaining = OtherDemand(m_DemandProbeBase, m_DemandProbeSlot);
			if (remaining > 0 && remaining < m_iDemandProbePrevious) m_bDemandProbeReduced = true;
			return;
		}
		foreach (AICF_InfantryRecruitmentOrder order : m_aOrders)
		{
			if (!order.IsCurrent(order.m_Slot) || order.m_Donor || !order.HasSafeBarracks()) continue;
			int other = OtherDemand(order.m_Base, order.m_Slot);
			int own, members;
			if (other <= 0 || !m_Spawner.QuoteMissingRoster(order.m_Slot, order.m_Faction, m_Config, own, members)) continue;
			m_bDemandProbeChecked = true;
			m_DemandProbeSlot = order.m_Slot;
			m_DemandProbeBase = order.m_Base;
			m_iDemandProbePrevious = other;
			int total = OtherDemand(order.m_Base, null);
			if (total == own + other) m_iDemandProbePassed++;
			order.m_iGeneration++;
			if (OtherDemand(order.m_Base, null) == other) m_iDemandProbePassed++;
			order.m_iGeneration--;
			order.m_iIntent++;
			if (OtherDemand(order.m_Base, null) == other) m_iDemandProbePassed++;
			order.m_iIntent--;
			order.m_iGraphRevision++;
			if (OtherDemand(order.m_Base, null) == other) m_iDemandProbePassed++;
			order.m_iGraphRevision--;
			order.m_bDemandReleased = true;
			if (OtherDemand(order.m_Base, null) == other) m_iDemandProbePassed++;
			order.m_bDemandReleased = false;
			AICF_DemandForecastChecks();
			CancelForSlot(order.m_Slot);
			if (OtherDemand(order.m_Base, null) == other) m_iDemandProbePassed++;
			Print(string.Format("[AICF][RECRUITMENT_SUPPLY_CHECKS] passed=%1 total=15 own=%2 other=%3 cancel_released=1",
				m_iDemandProbePassed, own, other));
			break;
		}
	}

	protected void AICF_DemandForecastChecks()
	{
		AICF_RecruitmentSupplyForecast poor = new AICF_RecruitmentSupplyForecast();
		poor.m_iOwnDemand = 100;
		poor.m_iMembers = 9;
		poor.m_fStock = 10;
		if (!poor.Evaluate(35, 300)) m_iDemandProbePassed++;
		poor.m_fIncome = 10;
		poor.m_fInterval = 60;
		poor.m_fNextArrival = 60;
		if (!poor.Evaluate(35, 300)) m_iDemandProbePassed++;
		poor.m_fIncome = 100;
		if (poor.Evaluate(35, 300) && poor.m_fCompletionSeconds == 87) m_iDemandProbePassed++;
		AICF_RecruitmentSupplyForecast rich = new AICF_RecruitmentSupplyForecast();
		rich.m_iOwnDemand = 100;
		rich.m_iMembers = 9;
		rich.m_fStock = 150;
		if (rich.Evaluate(185, 300) && rich.m_fCompletionSeconds == 77 && rich.m_fCompletionSeconds < poor.m_fCompletionSeconds) m_iDemandProbePassed++;
		rich.m_iOtherDemand = 100;
		if (!rich.Evaluate(185, 300)) m_iDemandProbePassed++;
		rich.m_fIncome = 50;
		rich.m_fInterval = 60;
		rich.m_fNextArrival = 60;
		if (rich.Evaluate(185, 300) && rich.m_fWaitSeconds == 10 && rich.m_fCompletionSeconds == 87) m_iDemandProbePassed++;
		rich.m_iOtherDemand = 151;
		if (!rich.Evaluate(185, 300)) m_iDemandProbePassed++;
		rich.m_iOtherDemand = 0;
		if (!rich.Evaluate(185, 76)) m_iDemandProbePassed++;
		if (rich.Evaluate(185, 300) && rich.m_fCompletionSeconds == 77) m_iDemandProbePassed++;
	}
}
