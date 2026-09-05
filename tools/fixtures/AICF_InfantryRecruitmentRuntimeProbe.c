// Test-only: временно копируется в Core/Forces. Готовит настоящие stock/RHS
// казармы и supplies; движение, roster, цены и transfer выполняет production.
modded class AICF_MatchController
{
	protected int m_iRecruitProbeStarted;
	protected int m_iRecruitProbePhase;
	protected int m_iRecruitProbeSample;
	protected EntityID m_RecruitProbeUSGroup;
	protected EntityID m_RecruitProbeUSSRGroup;
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aRecruitProbeBuildings = {};

	override protected void Update()
	{
		string enabled;
		bool probe = System.GetCLIParam("aicfRecruitProbe", enabled) && enabled == "1";
		if (probe && m_bRosterReady && !m_bStopped)
			RecruitProbeUpdate();
		super.Update();
	}

	protected void RecruitProbeUpdate()
	{
		int now = System.GetTickCount();
		if (m_iRecruitProbeStarted == 0)
		{
			m_iRecruitProbeStarted = now;
			m_Construction.Stop();
			m_RecruitProbeUSGroup = m_USState.GetSlot(0).GetGroup().GetID();
			m_RecruitProbeUSSRGroup = m_USSRState.GetSlot(0).GetGroup().GetID();
			for (int i = 1; i < 10; i++)
			{
				m_USState.GetSlot(i).SetDesiredSize(1);
				m_USSRState.GetSlot(i).SetDesiredSize(1);
			}
			RecruitProbePlace(m_USFaction);
			RecruitProbePlace(m_USSRFaction);
			Print("[AICF][RECRUIT_PROBE] prepared=1 selected_slots=US:0,USSR:0 other_slots_desired=1");
		}
		foreach (SCR_CampaignBuildingCompositionComponent composition : m_aRecruitProbeBuildings)
		{
			if (composition && !composition.IsCompositionSpawned() && composition.GetCompositionLayout())
			{
				SCR_CampaignBuildingLayoutComponent layout = composition.GetCompositionLayout();
				layout.AddBuildingValue(layout.GetToBuildValue());
			}
		}
		AICF_GroupSlot us = m_USState.GetSlot(0);
		AICF_GroupSlot ussr = m_USSRState.GetSlot(0);
		int usAlive = AICF_GroupRuntime.CountAliveAgents(us.GetGroup());
		int ussrAlive = AICF_GroupRuntime.CountAliveAgents(ussr.GetGroup());
		string faultFlag;
		bool faults = System.GetCLIParam("aicfRecruitProbeFaults", faultFlag) && faultFlag == "1";
		bool faultsComplete = faults && now - m_iRecruitProbeStarted >= 20000 &&
			m_InfantryRecruitment.RecruitProbeFaultsComplete() && m_InfantryRecruitment.CountPendingAgents() == 0;
		if (now - m_iRecruitProbeSample >= 10000)
		{
			m_iRecruitProbeSample = now;
			Print(string.Format("[AICF][RECRUIT_PROBE] elapsed_ms=%1 us=%2 ussr=%3 phase=%4 pending=%5",
				now - m_iRecruitProbeStarted, usAlive, ussrAlive, m_iRecruitProbePhase, m_InfantryRecruitment.CountPendingAgents()));
		}
		if (m_iRecruitProbePhase == 0 && usAlive == 10 && ussrAlive == 10 && !us.IsRecruitingInfantry() && !ussr.IsRecruitingInfantry())
		{
			bool sameGroups = us.GetGroup().GetID() == m_RecruitProbeUSGroup && ussr.GetGroup().GetID() == m_RecruitProbeUSSRGroup;
			Print(string.Format("[AICF][RECRUIT_PROBE] full_rosters=1 stable_groups=%1 desired_us=%2 desired_ussr=%3", sameGroups, us.GetDesiredSize(), ussr.GetDesiredSize()));
			m_iRecruitProbePhase = 1;
		}
		if (m_iRecruitProbePhase == 1 || faultsComplete || now - m_iRecruitProbeStarted >= 240000)
		{
			if (faults)
				Print(string.Format("[AICF][RECRUIT_PROBE] negative_complete=%1 us=%2 ussr=%3 pending=%4", faultsComplete, usAlive, ussrAlive, m_InfantryRecruitment.CountPendingAgents()));
			Print(string.Format("[AICF][RECRUIT_PROBE] finished=1 full_rosters=%1", m_iRecruitProbePhase == 1));
			Stop(true);
			foreach (SCR_CampaignBuildingCompositionComponent placed : m_aRecruitProbeBuildings)
			{
				if (placed && placed.GetOwner())
					SCR_EntityHelper.DeleteEntityAndChildren(placed.GetOwner());
			}
			m_aRecruitProbeBuildings.Clear();
			GetGame().GetCallqueue().Remove(RecruitProbeClose);
			GetGame().GetCallqueue().CallLater(RecruitProbeClose, 2000, false);
		}
	}

	protected void RecruitProbeClose()
	{
		GetGame().GetCallqueue().Remove(RecruitProbeClose);
		GetGame().RequestClose();
	}

	void ~AICF_MatchController()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(RecruitProbeClose);
	}

	protected void RecruitProbePlace(SCR_CampaignFaction faction)
	{
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		if (!base || !base.GetMasterProvider() || !base.GetSpawnPoint())
			return;
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
		if (!entity)
			return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation)
			affiliation.SetAffiliatedFaction(faction);
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (!composition)
			return;
		composition.SetProviderEntity(base.GetMasterProvider().GetOwner());
		m_aRecruitProbeBuildings.Insert(composition);
		Print(string.Format("[AICF][RECRUIT_PROBE] barracks=%1 faction=%2 position=%3 supplies=%4", entity.GetID(), faction.GetFactionKey(), position, base.GetSupplies()));
	}
}

modded class AICF_InfantryRecruitmentService
{
	protected bool m_bRecruitProbeUSFault;
	protected bool m_bRecruitProbeUSSRFault;

	bool RecruitProbeFaultsComplete()
	{
		return m_bRecruitProbeUSFault && m_bRecruitProbeUSSRFault;
	}

	override protected string Tick(AICF_InfantryRecruitmentOrder order, bool graphReady)
	{
		string flag;
		if (System.GetCLIParam("aicfRecruitProbeFaults", flag) && flag == "1" && order.m_Donor)
		{
			FactionKey side = AICF_ContentProfile.GetActive().GetStableFactionKey(order.m_Faction.GetFactionKey());
			if (side == "US" && !m_bRecruitProbeUSFault)
			{
				IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(order.m_Group);
				if (leader && m_Planner.AssignPlayerPointOrder(order.m_Slot, order.m_Faction, leader.GetOrigin()))
				{
					m_bRecruitProbeUSFault = true;
					order.Log("INFANTRY_RECRUIT_PROBE_FAULT", "fault=NEW_PLAYER_ORDER paid=0");
				}
			}
			if (side == "USSR" && !m_bRecruitProbeUSSRFault)
			{
				order.m_Base.AddSupplies(-order.m_Base.GetSupplies());
				m_bRecruitProbeUSSRFault = true;
				order.Log("INFANTRY_RECRUIT_PROBE_FAULT", "fault=SUPPLIES_REMOVED paid=0");
			}
		}
		return super.Tick(order, graphReady);
	}
}
