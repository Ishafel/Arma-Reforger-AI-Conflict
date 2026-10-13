// Только isolated stage. Реальные группы, казармы, donor, debit и transfer.
// Управляемые входы: arrival, supply/income, роли, admission и часы timeout.
modded class AICF_GroupSlot
{
	void AICF_SequenceInvalidateGeneration()
	{
		m_iSpawnGeneration++;
	}
}

modded class SCR_CampaignMilitaryBaseComponent
{
	override void AICF_GetRecruitmentIncome(out float amount, out float interval, out float nextArrival)
	{
		super.AICF_GetRecruitmentIncome(amount, interval, nextArrival);
		string flag;
		if (System.GetCLIParam("aicfRecruitmentSequenceProbe", flag) && flag == "1")
		{
			amount = 0;
			interval = 0;
			nextArrival = 0;
		}
	}
}

modded class AICF_InfantryRecruitmentService
{
	AICF_InfantryRecruitmentOrder AICF_SequenceOrder(AICF_GroupSlot slot)
	{
		foreach (AICF_InfantryRecruitmentOrder order : m_aOrders)
			if (order.m_Slot == slot && !order.m_bDemandReleased) return order;
		return null;
	}

	// Изолируем scheduling от стратегического выбора цели и подхода.
	AICF_InfantryRecruitmentOrder AICF_SequenceEnqueue(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		m_CombatSafety.Update(m_Campaign, m_Graph);
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		AICF_InfantryRecruitmentOrder order = SelectBarracks(slot, faction, leader.GetOrigin(), true);
		if (!order || order.m_bAwaitingSupplyPlan) return null;
		if (!m_Planner.BeginInfantryRecruitment(order)) return null;
		slot.ClearPlayerStrategicOrder();
		order.m_bPlayerRequested = false;
		m_aOrders.Insert(order);
		order.Log("INFANTRY_RECRUITMENT_STARTED", string.Format("alive=%1 desired=%2 distance_m=0 max_distance_m=500",
			AICF_GroupRuntime.CountAliveAgents(order.m_Group), slot.GetDesiredSize()));
		return order;
	}

	void AICF_SequenceObserve()
	{
		m_CombatSafety.Update(m_Campaign, m_Graph);
	}

	AICF_RecruitmentSupplyForecast AICF_SequenceForecast(AICF_InfantryRecruitmentOrder order)
	{
		int cost, members;
		m_Spawner.QuoteMissingRoster(order.m_Slot, order.m_Faction, m_Config, cost, members);
		AICF_RecruitmentSupplyForecast forecast = Forecast(order.m_Base, order.m_Slot, cost, members, order);
		forecast.Evaluate(0, 300);
		return forecast;
	}

	bool AICF_SequenceStale(AICF_InfantryRecruitmentOrder order)
	{
		float stock = order.m_Base.GetSupplies();
		return !order.ReportFailedMovement(order.m_Group, order.m_Waypoint) && !m_Spawner.BeginRecruit(order) &&
			!m_Economy.DebitInfantryRecruit(order) && order.m_Base.GetSupplies() == stock;
	}
}

modded class AICF_MatchController
{
	protected int m_iSequenceStarted;
	protected int m_iSequencePhase;
	protected int m_iSequencePhaseAt;
	protected int m_iSequencePassed;
	protected int m_iSequenceFailed;
	protected SCR_CampaignMilitaryBaseComponent m_SequenceSecond;
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aSequenceBuildings = {};
	protected ref array<SCR_ServicePointComponent> m_aSequenceServices = {};
	protected ref array<ref AICF_InfantryRecruitmentOrder> m_aSequenceOrders = {};
	protected ref array<float> m_aSequenceStock = {1000, 1000, 1000};
	protected ref AICF_InfantryRecruitmentOrder m_SequenceOld;
	protected bool m_bSequencePreempted;

	override protected void Update()
	{
		string flag;
		if (!System.GetCLIParam("aicfRecruitmentSequenceProbe", flag) || flag != "1" || !m_bRosterReady)
		{
			super.Update();
			return;
		}
		if (!m_bStopped) AICF_SequenceTick();
	}

	protected void AICF_SequenceCheck(string name, bool pass)
	{
		if (pass) m_iSequencePassed++;
		else m_iSequenceFailed++;
		Print(string.Format("[AICF][RECRUITMENT_SEQUENCE_PROBE] case=%1 pass=%2 phase=%3", name, pass, m_iSequencePhase));
	}

	protected void AICF_SequenceNext(int phase)
	{
		m_iSequencePhase = phase;
		m_iSequencePhaseAt = System.GetTickCount();
		Print(string.Format("[AICF][RECRUITMENT_SEQUENCE_PHASE] phase=%1 now_ms=%2", phase, m_iSequencePhaseAt));
	}

	protected void AICF_SequenceClock()
	{
		AICF_InfantryRecruitmentOrder clock = new AICF_InfantryRecruitmentOrder();
		bool pass = !clock.IsPurchaseDue(100000);
		int start = 1000;
		for (int i = 0; i < 9; i++)
		{
			clock.DelayPurchase(start + i * 5000);
			pass = pass && !clock.IsPurchaseDue(start + (i + 1) * 5000 - 1) && clock.IsPurchaseDue(start + (i + 1) * 5000);
		}
		AICF_SequenceCheck("CLOCK_NINE_45000", pass && clock.m_iNextPurchaseAtMs - start == 45000);
		clock.DelayPurchase(100000);
		AICF_SequenceCheck("CLOCK_NO_CATCHUP", !clock.IsPurchaseDue(104999) && clock.IsPurchaseDue(105000));
		AICF_RecruitmentSupplyForecast own = new AICF_RecruitmentSupplyForecast();
		own.m_bSequential = true;
		own.m_fStock = 1000;
		own.m_fCeiling = 1000;
		own.m_iMembers = 9;
		own.m_iOwnDemand = 90;
		for (int p = 0; p < 9; p++) own.m_aOwnCosts.Insert(10);
		AICF_SequenceCheck("FORECAST_NINE_45", own.Evaluate(0, 300) && own.m_fCompletionSeconds == 45);
		AICF_RecruitmentSupplyForecast next = new AICF_RecruitmentSupplyForecast();
		next.m_bSequential = true;
		next.m_fStock = 1000;
		next.m_fCeiling = 1000;
		next.m_iMembers = 1;
		next.m_iOwnDemand = 10;
		next.m_aOwnCosts.Insert(10);
		next.m_aPredecessors.Insert(own);
		AICF_SequenceCheck("FORECAST_QUEUE_50", next.Evaluate(0, 300) && next.m_fQueueSeconds == 45 && next.m_fCompletionSeconds == 50);
		own.m_fStock = 20;
		own.m_fCeiling = 20;
		own.m_fIncome = 20;
		own.m_fInterval = 20;
		own.m_fNextArrival = 20;
		AICF_SequenceCheck("FORECAST_CAPACITY", own.Evaluate(0, 300) && own.m_fCompletionSeconds == 80);
		AICF_SequenceCheck("FORECAST_DEADLINE", !own.Evaluate(0, 79));
	}

	protected void AICF_SequencePark(AICF_GroupSlot slot, SCR_ServicePointComponent service)
	{
		array<AIAgent> members = {};
		slot.GetGroup().GetAgents(members);
		foreach (AIAgent member : members)
			if (member.GetControlledEntity()) member.GetControlledEntity().SetOrigin(service.GetOwner().GetOrigin() + "2 0 2");
	}

	protected bool AICF_SequenceAdd(AICF_GroupSlot slot, SCR_CampaignFaction faction, int size, int service)
	{
		AICF_InfantryRecruitmentOrder existing = m_InfantryRecruitment.AICF_SequenceOrder(slot);
		if (existing) return existing.m_Service == m_aSequenceServices[service];
		slot.SetDesiredSize(size);
		AICF_SequencePark(slot, m_aSequenceServices[service]);
		AICF_InfantryRecruitmentOrder order = m_InfantryRecruitment.AICF_SequenceEnqueue(slot, faction);
		if (!order) return false;
		m_aSequenceOrders.Insert(order);
		return order.m_Service == m_aSequenceServices[service];
	}

	protected void AICF_SequenceTick()
	{
		int now = System.GetTickCount();
		SCR_CampaignMilitaryBaseComponent hq = m_USFaction.GetMainBase();
		SCR_CampaignMilitaryBaseComponent red = m_USSRFaction.GetMainBase();
		AICF_GroupSlot d = m_USState.GetSlot(0);
		AICF_GroupSlot a = m_USState.GetSlot(1);
		AICF_GroupSlot b = m_USState.GetSlot(2);
		AICF_GroupSlot g = m_USState.GetSlot(3);
		AICF_GroupSlot c = m_USState.GetSlot(4);
		AICF_GroupSlot r = m_USSRState.GetSlot(0);
		if (!m_iSequenceStarted)
		{
			m_iSequenceStarted = now;
			m_Construction.Stop();
			for (int i = 0; i < m_USState.GetSlotCount(); i++)
			{
				m_USState.GetSlot(i).SetDesiredSize(1);
				m_USSRState.GetSlot(i).SetDesiredSize(1);
			}
			float nearest = float.MAX;
			for (int n = 0; n < m_ObjectiveGraph.GetNodeCount(); n++)
			{
				SCR_CampaignMilitaryBaseComponent candidate = m_ObjectiveGraph.GetNode(n).GetBase();
				if (!candidate || candidate.IsHQ() || !candidate.IsInitialized() || !candidate.GetMasterProvider() || !candidate.GetSpawnPoint()) continue;
				float distance = vector.DistanceSqXZ(hq.GetOwner().GetOrigin(), candidate.GetOwner().GetOrigin());
				if (distance >= nearest) continue;
				nearest = distance;
				m_SequenceSecond = candidate;
			}
			if (!m_SequenceSecond) { AICF_SequenceCheck("SECOND_BASE", false); AICF_SequenceClose(); return; }
			m_SequenceSecond.SetFaction(m_USFaction);
			AICF_SequencePlace(hq, m_USFaction);
			AICF_SequencePlace(m_SequenceSecond, m_USFaction);
			AICF_SequencePlace(red, m_USSRFaction);
			AICF_SequenceClock();
		}
		if (now - m_iSequenceStarted > 600000) { AICF_SequenceCheck("TIMEOUT", false); AICF_SequenceClose(); return; }
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aSequenceBuildings)
			if (building && !building.IsCompositionSpawned() && building.GetCompositionLayout())
				building.GetCompositionLayout().AddBuildingValue(building.GetCompositionLayout().GetToBuildValue());
		array<SCR_CampaignMilitaryBaseComponent> bases = {hq, m_SequenceSecond, red};
		for (int stock = 0; stock < 3; stock++) bases[stock].SetSupplies(m_aSequenceStock[stock]);
		m_InfantryRecruitment.AICF_SequenceObserve();
		if (m_aSequenceServices.Count() < 3)
		{
			m_aSequenceServices.Clear();
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				SCR_ServicePointComponent barracks;
				array<SCR_ServicePointComponent> services = {};
				base.GetServices(services);
				foreach (SCR_ServicePointComponent service : services)
					if (service.GetType() == SCR_EServicePointType.BARRACKS && service.GetServiceState() == SCR_EServicePointStatus.ONLINE && AICF_BarracksCombatSafety.IsSafe(service)) barracks = service;
				if (!barracks) return;
				m_aSequenceServices.Insert(barracks);
			}
		}
		AICF_SequencePark(a, m_aSequenceServices[0]);
		AICF_SequencePark(d, m_aSequenceServices[0]);
		AICF_SequencePark(b, m_aSequenceServices[1]);
		AICF_SequencePark(r, m_aSequenceServices[2]);
		if (m_iSequencePhase >= 14) AICF_SequencePark(g, m_aSequenceServices[0]);
		if (m_iSequencePhase >= 12) AICF_SequencePark(c, m_aSequenceServices[0]);
		int da = AICF_GroupRuntime.CountAliveAgents(d.GetGroup());
		int aa = AICF_GroupRuntime.CountAliveAgents(a.GetGroup());
		int ba = AICF_GroupRuntime.CountAliveAgents(b.GetGroup());
		int ra = AICF_GroupRuntime.CountAliveAgents(r.GetGroup());
		if (m_iSequencePhase == 0)
		{
			d.SetRoleAndIndex(AICF_EGroupRole.DEFEND, 0);
			a.SetRoleAndIndex(AICF_EGroupRole.ATTACK, 0);
			b.SetRoleAndIndex(AICF_EGroupRole.ATTACK, 1);
			if (!AICF_SequenceAdd(d, m_USFaction, 2, 0) || !AICF_SequenceAdd(a, m_USFaction, 10, 0) ||
				!AICF_SequenceAdd(b, m_USFaction, 3, 1) || !AICF_SequenceAdd(r, m_USSRFaction, 3, 2)) return;
			AICF_SequenceCheck("ADMIT_DEFENSE_FIRST", true);
			AICF_SequenceCheck("ADMIT_ATTACK_NINE", true);
			AICF_SequenceCheck("ADMIT_SECOND_BASE", true);
			AICF_SequenceCheck("ADMIT_USSR", true);
			AICF_RecruitmentSupplyForecast forecast = m_InfantryRecruitment.AICF_SequenceForecast(m_aSequenceOrders[2]);
			AICF_SequenceCheck("LIVE_CROSS_BASE_FORECAST", forecast.m_fQueueSeconds == 45 && forecast.m_fCompletionSeconds == 55);
			AICF_SequenceNext(1);
		}
		else if (m_iSequencePhase == 1 && ra == 3)
		{
			AICF_SequenceCheck("FACTIONS_INDEPENDENT", aa < 10 && ba == 1 && da == 1);
			AICF_SequenceNext(2);
		}
		else if (m_iSequencePhase == 2 && aa == 10)
		{
			AICF_SequenceCheck("NINE_COMPLETE_NO_INTERLEAVING", ba == 1 && da == 1);
			AICF_SequenceNext(3);
		}
		else if (m_iSequencePhase == 3 && ba == 3)
		{
			AICF_SequenceCheck("FIFO_BEFORE_DEFENSE", da == 1);
			AICF_SequenceNext(4);
		}
		else if (m_iSequencePhase == 4 && da == 2 && !m_InfantryRecruitment.AICF_SequenceOrder(d))
		{
			AICF_SequenceCheck("COMPLETE_RELEASE", !m_InfantryRecruitment.AICF_SequenceOrder(a) && !m_InfantryRecruitment.AICF_SequenceOrder(b));
			AICF_SequenceCheck("PARTIAL_ADMISSION", AICF_SequenceAdd(d, m_USFaction, 4, 0));
			m_SequenceOld = m_InfantryRecruitment.AICF_SequenceOrder(d);
			AICF_SequenceNext(5);
		}
		else if (m_iSequencePhase == 5 && m_SequenceOld.m_Donor)
		{
			// Новый ATTACK не вытесняет начатую оборону, даже на общей базе.
			AICF_SequencePark(b, m_aSequenceServices[0]);
			AICF_SequenceCheck("LATE_ATTACK_ADMIT", AICF_SequenceAdd(b, m_USFaction, 5, 0));
			AICF_SequenceNext(6);
		}
		else if (m_iSequencePhase == 6)
		{
			AICF_SequenceCheck("LATE_ATTACK_HELD", ba == 3 && m_SequenceOld.m_bAutoServing && !m_InfantryRecruitment.AICF_SequenceOrder(b).m_Donor);
			string reason;
			AICF_SequenceCheck("MANUAL_PROMOTION", m_InfantryRecruitment.RequestPlayerRecruitment(b, m_USFaction, reason));
			m_aSequenceStock[0] = m_InfantryRecruitment.AICF_SequenceForecast(m_InfantryRecruitment.AICF_SequenceOrder(b)).m_iOwnDemand;
			AICF_SequenceNext(7);
		}
		else if (m_iSequencePhase == 7 && ba == 5)
		{
			AICF_SequenceCheck("MANUAL_PRIORITY", da < 4 && m_aSequenceStock[0] == 0);
			m_aSequenceStock[0] = 1000;
			AICF_SequenceNext(8);
		}
		else if (m_iSequencePhase == 8 && da == 4 && !m_InfantryRecruitment.AICF_SequenceOrder(d))
		{
			AICF_SequenceCheck("PARTIAL_ONLY_VACANCIES", ba == 5 && aa == 10);
			AICF_SequenceCheck("DEFICIT_OWNER_ADMIT", AICF_SequenceAdd(d, m_USFaction, 5, 0));
			m_SequenceOld = m_InfantryRecruitment.AICF_SequenceOrder(d);
			// Сначала даём DEFEND обслуживание, следующая заявка появляется позже.
			AICF_SequenceNext(9);
		}
		else if (m_iSequencePhase == 9)
		{
			AICF_SequenceCheck("DEFICIT_WAITER_ADMIT", AICF_SequenceAdd(b, m_USFaction, 6, 1));
			m_aSequenceStock[0] = 0;
			AICF_SequenceNext(10);
		}
		else if (m_iSequencePhase == 10 && now - m_iSequencePhaseAt >= 7000)
		{
			AICF_SequenceCheck("DEFICIT_NO_BYPASS", ba == 5 && da == 4 && m_SequenceOld.m_bAutoServing);
			m_SequenceOld.m_iSupplyWaitStartedAtMs = now - AICF_InfantryRecruitmentConfig.SUPPLY_WAIT_TIMEOUT_MS;
			AICF_SequenceNext(11);
		}
		else if (m_iSequencePhase == 11 && ba == 6 && !m_InfantryRecruitment.AICF_SequenceOrder(b))
		{
			AICF_SequenceCheck("TIMEOUT_RELEASE", m_SequenceOld.m_bDemandReleased && da == 4 && m_InfantryRecruitment.AICF_SequenceStale(m_SequenceOld));
			m_aSequenceStock[0] = 1000;
			hq.SetSupplies(1000);
			c.SetRoleAndIndex(AICF_EGroupRole.DEFEND, 2);
			AICF_SequenceCheck("CANCEL_OWNER_ADMIT", AICF_SequenceAdd(c, m_USFaction, 2, 0));
			m_SequenceOld = m_InfantryRecruitment.AICF_SequenceOrder(c);
			AICF_SequenceNext(12);
		}
		else if (m_iSequencePhase == 12)
		{
			AICF_SequenceCheck("CANCEL_WAITER_ADMIT", AICF_SequenceAdd(b, m_USFaction, 7, 1));
			AICF_SequenceCheck("CANCEL_RELEASE", m_InfantryRecruitment.CancelForSlot(c) && m_SequenceOld.m_bDemandReleased && m_InfantryRecruitment.AICF_SequenceStale(m_SequenceOld));
			AICF_SequenceNext(13);
		}
		else if (m_iSequencePhase == 13 && ba == 7 && !m_InfantryRecruitment.AICF_SequenceOrder(b))
		{
			AICF_SequenceCheck("CANCEL_NEXT_PROGRESS", da == 4);
			// Отдельный живой slot: отменённый d вправе ждать восстановления приказа.
			g.SetRoleAndIndex(AICF_EGroupRole.DEFEND, 1);
			AICF_SequenceCheck("GENERATION_OWNER_ADMIT", AICF_SequenceAdd(g, m_USFaction, 2, 0));
			m_SequenceOld = m_InfantryRecruitment.AICF_SequenceOrder(g);
			AICF_SequenceNext(14);
		}
		else if (m_iSequencePhase == 14)
		{
			AICF_SequenceCheck("GENERATION_WAITER_ADMIT", AICF_SequenceAdd(b, m_USFaction, 8, 1));
			g.AICF_SequenceInvalidateGeneration();
			AICF_SequenceCheck("STALE_CALLBACK_NO_MUTATION", m_InfantryRecruitment.AICF_SequenceStale(m_SequenceOld));
			AICF_SequenceNext(15);
		}
		else if (m_iSequencePhase == 15 && ba == 8 && !m_InfantryRecruitment.AICF_SequenceOrder(b))
		{
			AICF_SequenceCheck("GENERATION_NEXT_PROGRESS", da == 4 && m_SequenceOld.m_bDemandReleased);
			AICF_SequenceClose();
			return;
		}
		// В ручном тесте b посещает общую с d казарму.
		if (m_iSequenceFailed > 0) { AICF_SequenceClose(); return; }
		if (m_iSequencePhase >= 6 && m_iSequencePhase <= 8) AICF_SequencePark(b, m_aSequenceServices[0]);
		for (int stockIndex = 0; stockIndex < 3; stockIndex++) bases[stockIndex].SetSupplies(m_aSequenceStock[stockIndex]);
		m_InfantryRecruitment.Update(null, null, null, null, 100, true);
		for (int save = 0; save < 3; save++) m_aSequenceStock[save] = bases[save].GetSupplies();
	}

	protected void AICF_SequencePlace(SCR_CampaignMilitaryBaseComponent base, SCR_CampaignFaction faction)
	{
		vector position = base.GetSpawnPoint().GetOrigin() + "35 0 25";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab(faction.GetFactionKey(), AICF_EConstructionType.SMALL_BARRACKS);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (!entity) return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation) affiliation.SetAffiliatedFaction(faction);
		SCR_CampaignBuildingCompositionComponent building = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (building) building.SetProviderEntity(base.GetMasterProvider().GetOwner());
		m_aSequenceBuildings.Insert(building);
	}

	protected void AICF_SequenceClose()
	{
		Print(string.Format("[AICF][RECRUITMENT_SEQUENCE_PROBE] finished=1 passed=%1 failed=%2 phase=%3", m_iSequencePassed, m_iSequenceFailed, m_iSequencePhase));
		Stop(true);
		GetGame().RequestClose();
	}
}
