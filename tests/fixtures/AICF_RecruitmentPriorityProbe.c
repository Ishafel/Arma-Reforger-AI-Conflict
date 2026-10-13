// Только isolated stage. Настоящие roster, казарма, donor и экономика;
// fixture задаёт запас/доход, роли и позиции, приостанавливает стратегический loop.
modded class SCR_CampaignMilitaryBaseComponent
{
	override void AICF_GetRecruitmentIncome(out float amount, out float interval, out float nextArrival)
	{
		super.AICF_GetRecruitmentIncome(amount, interval, nextArrival);
		string flag;
		if (System.GetCLIParam("aicfRecruitmentPriorityProbe", flag) && flag == "1")
		{
			amount = 0;
			interval = 0;
			nextArrival = 0;
		}
	}
}

modded class AICF_InfantryRecruitmentService
{
	AICF_InfantryRecruitmentOrder AICF_PriorityOrder(AICF_GroupSlot slot)
	{
		foreach (AICF_InfantryRecruitmentOrder order : m_aOrders)
			if (order.m_Slot == slot && !order.m_bDemandReleased) return order;
		return null;
	}

	int AICF_PriorityDemand(SCR_CampaignMilitaryBaseComponent base, AICF_InfantryRecruitmentOrder requester = null)
	{
		AICF_GroupSlot excluded;
		if (requester) excluded = requester.m_Slot;
		return OtherDemand(base, excluded, null, requester);
	}

	void AICF_PriorityObserve()
	{
		m_CombatSafety.Update(m_Campaign, m_Graph);
	}

	int AICF_PriorityQuote(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		int cost, members;
		m_Spawner.QuoteMissingRoster(slot, faction, m_Config, cost, members);
		return cost;
	}

	void AICF_PriorityCheck(string name, bool pass)
	{
		Print(string.Format("[AICF][RECRUITMENT_PRIORITY_PROBE] case=%1 pass=%2", name, pass));
	}

	void AICF_PriorityContracts(AICF_GroupSlot defense, AICF_GroupSlot attack, AICF_GroupSlot second)
	{
		AICF_InfantryRecruitmentOrder d = AICF_PriorityOrder(defense);
		AICF_InfantryRecruitmentOrder a = AICF_PriorityOrder(attack);
		AICF_InfantryRecruitmentOrder b = AICF_PriorityOrder(second);
		if (!d || !a || !b)
		{
			AICF_PriorityCheck("THREE_REQUESTS", false);
			return;
		}
		AICF_PriorityCheck("THREE_REQUESTS", d.m_Base == a.m_Base && a.m_Base == b.m_Base);
		int price = a.m_Forecast.m_iOwnDemand;
		AICF_PriorityCheck("AUTO_ORDER", AICF_RecruitmentPriority.Precedes(a, b) && AICF_RecruitmentPriority.Precedes(b, d));
		AICF_PriorityCheck("PENDING_DEMAND", b.m_bAwaitingSupplyPlan && d.m_bAwaitingSupplyPlan && AICF_PriorityDemand(a.m_Base, d) == 2 * price);
		AICF_PriorityCheck("ATTACK_FORECAST", a.m_Forecast.m_iOtherDemand == 0 && d.m_Forecast.m_iOtherDemand == 2 * price);
		defense.SetRoleAndIndex(AICF_EGroupRole.ATTACK, 8);
		AICF_PriorityCheck("ROLE_CHANGE", AICF_RecruitmentPriority.Rank(defense, false) == 1 && AICF_PriorityDemand(a.m_Base, d) == 2 * price);
		attack.SetRoleAndIndex(AICF_EGroupRole.DEFEND, 7);
		AICF_PriorityCheck("ROLE_REALLOCATION", AICF_PriorityDemand(a.m_Base, b) == 0 && AICF_PriorityDemand(a.m_Base, a) == 2 * price);
		defense.SetRoleAndIndex(AICF_EGroupRole.DEFEND, 0);
		attack.SetRoleAndIndex(AICF_EGroupRole.ATTACK, 0);
		defense.BeginPlayerStrategicOrder();
		AICF_PriorityCheck("UNRELATED_MANUAL", AICF_RecruitmentPriority.Rank(defense, d.m_bPlayerRequested) == 2);
		defense.ClearPlayerStrategicOrder();
		int total = AICF_PriorityDemand(a.m_Base);
		a.m_iGeneration++;
		AICF_PriorityCheck("STALE_GENERATION", AICF_PriorityDemand(a.m_Base) == total - price);
		a.m_iGeneration--;
		a.m_iAssignment++;
		AICF_PriorityCheck("STALE_ASSIGNMENT", AICF_PriorityDemand(a.m_Base) == total - price);
		a.m_iAssignment--;
		a.m_iIntent++;
		AICF_PriorityCheck("STALE_INTENT", AICF_PriorityDemand(a.m_Base) == total - price);
		a.m_iIntent--;
		a.m_iGraphRevision++;
		AICF_PriorityCheck("STALE_GRAPH", AICF_PriorityDemand(a.m_Base) == total - price);
		a.m_iGraphRevision--;
		SCR_AIGroup original = a.m_Group;
		a.m_Group = b.m_Group;
		AICF_PriorityCheck("REPLACED_GROUP", AICF_PriorityDemand(a.m_Base) == total - price);
		a.m_Group = original;
		a.m_bDemandReleased = true;
		AICF_PriorityCheck("RELEASE_BEFORE_CLEANUP", AICF_PriorityDemand(a.m_Base) == total - price);
		a.m_bDemandReleased = false;
		int started = a.m_iStartedAtMs;
		a.m_iStartedAtMs = System.GetTickCount() - AICF_InfantryRecruitmentConfig.VISIT_TIMEOUT_MS;
		AICF_PriorityCheck("EXPIRED_DEMAND", AICF_PriorityDemand(a.m_Base) == total - price);
		a.m_iStartedAtMs = started;
		float stock = a.m_Base.GetSupplies();
		a.m_Base.AddSupplies(-stock);
		AICF_PriorityCheck("NO_STOCK_DEBIT", !m_Economy.DebitInfantryRecruit(a) && a.m_Base.GetSupplies() == 0);
		a.m_Base.AddSupplies(stock);
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(a.m_Group);
		vector position = leader.GetOrigin();
		leader.SetOrigin(position + "100 0 0");
		AICF_PriorityCheck("PHYSICAL_GATE", !m_Economy.DebitInfantryRecruit(a) && a.m_Base.GetSupplies() == stock);
		leader.SetOrigin(position);
		EntityID baseId = a.m_BaseId;
		a.m_BaseId = EntityID.INVALID;
		AICF_PriorityCheck("BASE_IDENTITY_GATE", !m_Economy.DebitInfantryRecruit(a) && a.m_Base.GetSupplies() == stock);
		a.m_BaseId = baseId;
		m_Economy.SetGraphContextReady(false);
		AICF_PriorityCheck("ECONOMY_GRAPH_GATE", !m_Economy.DebitInfantryRecruit(a) && a.m_Base.GetSupplies() == stock);
		m_Economy.SetGraphContextReady(true);
	}
}

modded class AICF_MatchController
{
	protected int m_iPriorityStarted;
	protected int m_iPriorityPhase;
	protected int m_iPriorityPhaseStarted;
	protected SCR_CampaignBuildingCompositionComponent m_PriorityBuilding;
	protected SCR_ServicePointComponent m_PriorityBarracks;
	protected ref AICF_InfantryRecruitmentOrder m_PriorityPreempted;
	protected ref array<EntityID> m_aPriorityGroupIds = {};
	protected float m_fPriorityStock;
	protected int m_iPriorityFirstCost;
	protected int m_iPriorityNextCost;
	protected bool m_bPriorityPreemptionObserved;

	override protected void Update()
	{
		string flag;
		if (!System.GetCLIParam("aicfRecruitmentPriorityProbe", flag) || flag != "1" || !m_bRosterReady)
		{
			super.Update();
			return;
		}
		if (!m_bStopped) AICF_PriorityTick();
	}

	protected void AICF_PriorityNext(int phase)
	{
		m_iPriorityPhase = phase;
		m_iPriorityPhaseStarted = System.GetTickCount();
	}

	protected void AICF_PriorityStock(float value)
	{
		SCR_CampaignMilitaryBaseComponent base = m_USFaction.GetMainBase();
		base.AddSupplies(value - base.GetSupplies());
	}

	protected void AICF_PriorityTick()
	{
		int now = System.GetTickCount();
		if (m_iPriorityStarted > 0 && now - m_iPriorityStarted > 600000)
		{
			m_InfantryRecruitment.AICF_PriorityCheck("TIMEOUT", false);
			AICF_PriorityClose();
			return;
		}
		SCR_CampaignMilitaryBaseComponent base = m_USFaction.GetMainBase();
		AICF_GroupSlot d = m_USState.GetSlot(0);
		AICF_GroupSlot a = m_USState.GetSlot(1);
		AICF_GroupSlot b = m_USState.GetSlot(2);
		if (!m_iPriorityStarted)
		{
			m_iPriorityStarted = now;
			m_Construction.Stop();
			for (int i = 0; i < m_USState.GetSlotCount(); i++)
			{
				m_USState.GetSlot(i).SetDesiredSize(1);
				m_USSRState.GetSlot(i).SetDesiredSize(1);
				if (i < 3) m_aPriorityGroupIds.Insert(m_USState.GetSlot(i).GetGroup().GetID());
			}
			AICF_PriorityPlace(base);
		}
		if (m_PriorityBuilding && !m_PriorityBuilding.IsCompositionSpawned() && m_PriorityBuilding.GetCompositionLayout())
		{
			SCR_CampaignBuildingLayoutComponent layout = m_PriorityBuilding.GetCompositionLayout();
			layout.AddBuildingValue(layout.GetToBuildValue());
		}
		if (!m_PriorityBarracks)
		{
			array<SCR_ServicePointComponent> services = {};
			base.GetServices(services);
			foreach (SCR_ServicePointComponent service : services)
				if (service.GetType() == SCR_EServicePointType.BARRACKS && service.GetServiceState() == SCR_EServicePointStatus.ONLINE)
					m_PriorityBarracks = service;
			return;
		}
		m_InfantryRecruitment.AICF_PriorityObserve();
		// Только controlled arrival: fixture не доказывает navigation после
		// восстановления исходного стратегического приказа.
		for (int parked = 0; parked < 3; parked++)
		{
			array<AIAgent> members = {};
			m_USState.GetSlot(parked).GetGroup().GetAgents(members);
			foreach (AIAgent member : members)
				if (member.GetControlledEntity()) member.GetControlledEntity().SetOrigin(m_PriorityBarracks.GetOwner().GetOrigin() + "2 0 2");
		}
		// Внешний native income не участвует в контролируемом дефиците.
		if (m_iPriorityPhase > 0) AICF_PriorityStock(m_fPriorityStock);
		if (m_iPriorityPhase == 0)
		{
			if (!AICF_BarracksCombatSafety.IsSafe(m_PriorityBarracks)) return;
			d.SetRoleAndIndex(AICF_EGroupRole.DEFEND, 0);
			a.SetRoleAndIndex(AICF_EGroupRole.ATTACK, 0);
			b.SetRoleAndIndex(AICF_EGroupRole.ATTACK, 1);
			for (int n = 0; n < 3; n++)
			{
				AICF_GroupSlot slot = m_USState.GetSlot(n);
				slot.SetDesiredSize(2);
				IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
				leader.SetOrigin(m_PriorityBarracks.GetOwner().GetOrigin() + "2 0 2");
			}
			m_iPriorityFirstCost = m_InfantryRecruitment.AICF_PriorityQuote(a, m_USFaction);
			AICF_PriorityStock(m_iPriorityFirstCost);
			m_InfantryRecruitment.Update(m_USState, m_USFaction, null, null, 100, true);
			m_InfantryRecruitment.AICF_PriorityContracts(d, a, b);
			m_fPriorityStock = base.GetSupplies();
			AICF_PriorityNext(1);
			return;
		}
		int da = AICF_GroupRuntime.CountAliveAgents(d.GetGroup());
		int aa = AICF_GroupRuntime.CountAliveAgents(a.GetGroup());
		int ba = AICF_GroupRuntime.CountAliveAgents(b.GetGroup());
		if (m_iPriorityPhase == 1 && aa == 2)
		{
			m_InfantryRecruitment.AICF_PriorityCheck("SCARCE_ATTACK_FIRST", da == 1 && ba == 1 && m_fPriorityStock == 0);
			string reason;
			bool requested = m_InfantryRecruitment.RequestPlayerRecruitment(d, m_USFaction, reason);
			AICF_InfantryRecruitmentOrder manual = m_InfantryRecruitment.AICF_PriorityOrder(d);
			m_InfantryRecruitment.AICF_PriorityCheck("MANUAL_PROMOTION", requested && manual && manual.m_bPlayerRequested && AICF_RecruitmentPriority.Rank(d, manual.m_bPlayerRequested) == 0);
			int assignment = d.GetStrategicAssignmentRevision();
			m_USAICommander.AssignOrder(d, "PRIORITY_PROBE");
			m_InfantryRecruitment.AICF_PriorityCheck("PENDING_MANUAL_PROTECTED", d.HasActiveRecruitmentOrder() && d.HasPlayerStrategicOrder() && d.GetStrategicAssignmentRevision() == assignment && manual.IsCurrent(d));
			AIWaypoint previous = d.GetWaypoint();
			bool cancelled = m_InfantryRecruitment.CancelForSlot(d);
			m_InfantryRecruitment.AICF_PriorityCheck("MANUAL_CANCEL", cancelled && manual.m_bDemandReleased && !d.HasActiveRecruitmentOrder() && !d.HasPlayerStrategicOrder() && d.GetWaypoint() == previous && m_InfantryRecruitment.AICF_PriorityDemand(base) == m_iPriorityFirstCost);
			requested = m_InfantryRecruitment.RequestPlayerRecruitment(d, m_USFaction, reason);
			m_InfantryRecruitment.AICF_PriorityCheck("MANUAL_REQUEUE", requested);
			m_fPriorityStock = m_iPriorityFirstCost;
			AICF_PriorityNext(2);
		}
		else if (m_iPriorityPhase == 2 && da == 2)
		{
			m_InfantryRecruitment.AICF_PriorityCheck("MANUAL_DEFENSE_FIRST", ba == 1 && aa == 2 && m_fPriorityStock == 0);
			m_fPriorityStock = m_iPriorityFirstCost;
			AICF_PriorityNext(3);
		}
		else if (m_iPriorityPhase == 3 && ba == 2)
		{
			m_InfantryRecruitment.AICF_PriorityCheck("FIFO_EVENTUAL", da == 2 && aa == 2 && m_fPriorityStock == 0);
			AICF_PriorityNext(4);
		}
		else if (m_iPriorityPhase == 4 && !m_InfantryRecruitment.AICF_PriorityOrder(d) && !m_InfantryRecruitment.AICF_PriorityOrder(a) && !m_InfantryRecruitment.AICF_PriorityOrder(b))
		{
			m_InfantryRecruitment.AICF_PriorityCheck("COMPLETE_RELEASE", m_InfantryRecruitment.AICF_PriorityDemand(base) == 0 && !d.HasPlayerStrategicOrder());
			d.SetDesiredSize(3);
			m_iPriorityNextCost = m_InfantryRecruitment.AICF_PriorityQuote(d, m_USFaction);
			m_fPriorityStock = 100;
			AICF_PriorityNext(5);
		}
		else if (m_iPriorityPhase == 5)
		{
			m_PriorityPreempted = m_InfantryRecruitment.AICF_PriorityOrder(d);
			if (m_PriorityPreempted && m_PriorityPreempted.m_Donor)
			{
				a.SetDesiredSize(3);
				m_fPriorityStock = m_iPriorityNextCost;
				AICF_PriorityNext(6);
			}
		}
		else if (m_iPriorityPhase == 6 && aa == 3)
		{
			m_InfantryRecruitment.AICF_PriorityCheck("NEW_ATTACK_PREEMPTS", da == 2 && m_bPriorityPreemptionObserved && !m_PriorityPreempted.m_Donor && m_fPriorityStock == 0);
			m_InfantryRecruitment.AICF_PriorityCheck("PAID_MEMBERS_PRESERVED", aa == 3 && ba == 2 && da == 2);
			m_fPriorityStock = 100;
			b.SetDesiredSize(3);
			AICF_PriorityNext(7);
		}
		else if (m_iPriorityPhase == 7 && da == 3 && aa == 3 && ba == 3)
		{
			m_InfantryRecruitment.AICF_PriorityCheck("RICH_BOTH_ROLES", m_fPriorityStock == 100 - 2 * m_iPriorityNextCost);
			bool stable = true;
			for (int g = 0; g < 3; g++)
				if (m_USState.GetSlot(g).GetGroup().GetID() != m_aPriorityGroupIds[g]) stable = false;
			m_InfantryRecruitment.AICF_PriorityCheck("STABLE_GROUPS", stable);
			AICF_PriorityNext(8);
		}
		else if (m_iPriorityPhase == 8 && m_InfantryRecruitment.CountPendingAgents() == 0 && m_InfantryRecruitment.AICF_PriorityDemand(base) == 0)
		{
			AICF_PriorityClose();
			return;
		}
		AICF_PriorityStock(m_fPriorityStock);
		m_InfantryRecruitment.Update(m_USState, m_USFaction, null, null, 100, true);
		if (m_iPriorityPhase == 6 && m_PriorityPreempted.m_sPriorityWaitReason == "PRIORITY_DEMAND" && !m_PriorityPreempted.m_Donor)
			m_bPriorityPreemptionObserved = true;
		m_fPriorityStock = base.GetSupplies();
		if (now % 10000 < 1100)
			Print(string.Format("[AICF][RECRUITMENT_PRIORITY_STATE] phase=%1 defense=%2 attack=%3 second=%4 stock=%5", m_iPriorityPhase, da, aa, ba, m_fPriorityStock));
	}

	protected void AICF_PriorityPlace(SCR_CampaignMilitaryBaseComponent base)
	{
		vector position = base.GetSpawnPoint().GetOrigin() + "35 0 25";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab("US", AICF_EConstructionType.SMALL_BARRACKS);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (!entity) return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation) affiliation.SetAffiliatedFaction(m_USFaction);
		m_PriorityBuilding = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (m_PriorityBuilding) m_PriorityBuilding.SetProviderEntity(base.GetMasterProvider().GetOwner());
	}

	protected void AICF_PriorityClose()
	{
		Print(string.Format("[AICF][RECRUITMENT_PRIORITY_PROBE] finished=1 phase=%1", m_iPriorityPhase));
		Stop(true);
		GetGame().RequestClose();
	}
}
