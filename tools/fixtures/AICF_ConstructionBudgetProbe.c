// Только isolated runtime: синхронная проверка общей квоты без geometry calls.
class AICF_ConstructionBudgetProbe : AICF_ConstructionSiteSearch
{
	static void Run()
	{
		int passed;
		int oldLimit = s_iLimit;
		s_iLimit = 96;
		s_iWindow = System.GetTickCount() / 1000;
		s_iQueries = 96;
		AICF_ConstructionOrder first = new AICF_ConstructionOrder();
		first.m_sToken = "budget-first";
		AICF_ConstructionOrder second = new AICF_ConstructionOrder();
		second.m_sToken = "budget-second";
		AICF_ConstructionOrder search = new AICF_ConstructionOrder();
		search.m_sToken = "budget-search";
		if (!BeginLiveBudget(first, 28) && !s_AtomicOrder) passed++;
		if (!BeginLiveBudget(second, 30) && s_aClaims.Count() == 2) passed++;
		s_iQueries = 0;
		if (!TakeQueries(search, 69)) passed++;
		if (TakeQueries(search, 68)) passed++;
		if (!BeginLiveBudget(second, 30)) passed++;
		if (BeginLiveBudget(first, 28)) passed++;
		if (TakeQueries(first, 28) && s_iQueries == 96) passed++;
		s_AtomicOrder = null;
		if (!BeginLiveBudget(second, 30)) passed++;
		s_iQueries = 0;
		if (BeginLiveBudget(second, 30)) passed++;
		if (TakeQueries(second, 30) && s_iQueries == 30) passed++;
		s_AtomicOrder = null;
		if (s_aClaims.IsEmpty()) passed++;
		if (!BeginLiveBudget(first, 97) && first.m_sReason == "LIVE_QUERY_LIMIT_TOO_SMALL") passed++;
		s_iQueries = 96;
		BeginLiveBudget(first, 28);
		ReleaseClaim(first.m_sToken);
		if (s_aClaims.IsEmpty()) passed++;
		BeginLiveBudget(second, 30);
		s_aClaims[0].m_iExpires = System.GetTickCount();
		PruneClaims();
		if (s_aClaims.IsEmpty()) passed++;
		s_iLimit = oldLimit;
		s_iQueries = 0;
		s_AtomicOrder = null;
		s_aClaims.Clear();
		Print(string.Format("[AICF][CONSTRUCTION_BUDGET_CONTRACT] test_only=1 passed=%1 total=14", passed));
		if (passed != 14)
			Print("[AICF][CONSTRUCTION_BUDGET_CONTRACT] FAILED", LogLevel.ERROR);
		RunCheckpoint();
		RunCommitReserve();
	}

	static void RunCommitReserve()
	{
		int oldLimit = s_iLimit;
		s_iLimit = 96;
		s_iWindow = System.GetTickCount() / 1000;
		s_iQueries = 96;
		AICF_ConstructionOrder commit = new AICF_ConstructionOrder();
		commit.m_sToken = "commit-inventory";
		commit.m_iStage = 4;
		commit.m_iQueryPhase = 6;
		AICF_ConstructionOrder search = new AICF_ConstructionOrder();
		search.m_sToken = "competing-search";
		int passed;
		if (!BeginLiveBudget(commit, 28) && s_aClaims[0].m_iCount == 29) passed++;
		s_aClaims[0].m_iExpires = System.GetTickCount() - 1;
		RefreshClaim(commit.m_sToken);
		PruneClaims();
		if (s_aClaims.Count() == 1) passed++;
		s_iQueries = 0;
		if (TakeQueries(search, 67)) passed++;
		if (!TakeQueries(search, 1)) passed++;
		if (TakeQueries(commit, 1) && s_iQueries == 68) passed++;
		if (BeginLiveBudget(commit, 28)) passed++;
		if (TakeQueries(commit, 28) && s_iQueries == 96 && s_aClaims.IsEmpty()) passed++;
		s_AtomicOrder = null;
		s_aClaims.Clear();
		s_iQueries = 0;
		s_iLimit = oldLimit;
		Print(string.Format("[AICF][CONSTRUCTION_COMMIT_BUDGET_CONTRACT] test_only=1 passed=%1 total=7", passed));
		if (passed != 7)
			Print("[AICF][CONSTRUCTION_COMMIT_BUDGET_CONTRACT] FAILED", LogLevel.ERROR);
	}

	static void RunCheckpoint()
	{
		AICF_ConstructionOrder order = new AICF_ConstructionOrder();
		order.m_sToken = "identity-retained";
		order.m_iQueries = 500;
		order.m_iPathQueriesAt = 100;
		order.m_iPathStartOption = 3;
		order.m_iNavPathCursor = 16;
		order.m_vSpawnOrigin = "10 2 30";
		order.m_vPathStart = "11 2 31";
		order.m_Path = new AICF_ConstructionPath();
		AICF_ConstructionPath path = order.m_Path;
		order.m_aWorkCandidates.Insert("40 3 50");
		AICF_ConstructionVolume volume = new AICF_ConstructionVolume();
		order.m_aExits.Insert(volume);
		AICF_ConstructionCandidate checkpoint = new AICF_ConstructionCandidate();
		checkpoint.Save(order);
		order.m_iQueries = 2000;
		order.m_iPathQueriesAt = 1900;
		order.m_iPathStartOption = 7;
		order.m_iNavPathCursor = 0;
		order.m_aWorkCandidates.Clear();
		order.m_aExits.Clear();
		order.m_Path = null;
		checkpoint.Restore(order);
		int passed;
		if (order.m_sToken == "identity-retained") passed++;
		if (order.m_iQueries == 2000 && order.m_iQueries - order.m_iPathQueriesAt == 400) passed++;
		if (order.m_Path == path && order.m_iStage == 3) passed++;
		if (order.m_iPathStartOption == 7 && order.m_iNavPathCursor == 16) passed++;
		if (order.m_aExits.Count() == 1 && order.m_aExits[0] == volume) passed++;
		if (order.m_aWorkCandidates.Count() == 1 && vector.DistanceSq(order.m_aWorkCandidates[0], "40 3 50") == 0) passed++;
		Print(string.Format("[AICF][CONSTRUCTION_CHECKPOINT_CONTRACT] test_only=1 passed=%1 total=6", passed));
		if (passed != 6)
			Print("[AICF][CONSTRUCTION_CHECKPOINT_CONTRACT] FAILED", LogLevel.ERROR);
	}
}

modded class AICF_ConstructionPlanner
{
	protected bool m_bAICFBudgetProbeRan;
	override void Update()
	{
		if (!m_bAICFBudgetProbeRan)
		{
			m_bAICFBudgetProbeRan = true;
			string enabled;
			if (System.GetCLIParam("aicfConstructionBudgetProbe", enabled) && enabled == "1")
				AICF_ConstructionBudgetProbe.Run();
		}
		super.Update();
	}
}

// Сохраняет выбранную baseline пару HQ до stock initialization; только fixture.
modded class SCR_CampaignMilitaryBaseManager
{
	override void SelectHQs(notnull array<SCR_CampaignMilitaryBaseComponent> candidates, notnull array<SCR_CampaignMilitaryBaseComponent> controlPoints, out notnull array<SCR_CampaignMilitaryBaseComponent> selectedHQs)
	{
		string requestedUS, requestedUSSR;
		if (System.GetCLIParam("aicfConstructionProbeUSBase", requestedUS) && System.GetCLIParam("aicfConstructionProbeUSSRBase", requestedUSSR))
		{
			SCR_CampaignMilitaryBaseComponent us, ussr;
			// Campaign manager ещё не зарегистрировал неинициализированные HQ.
			// System содержит все штатные базы мира до выбора сторон.
			array<SCR_MilitaryBaseComponent> worldBases = {};
			SCR_MilitaryBaseSystem.GetInstance().GetBases(worldBases);
			foreach (SCR_MilitaryBaseComponent worldBase : worldBases)
			{
				SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(worldBase);
				if (!base || !base.GetOwner())
					continue;
				string id = AICF_ConstructionOrder.EntityKey(base.GetOwner().GetID());
				if (id == requestedUS)
					us = base;
				if (id == requestedUSSR)
					ussr = base;
			}
			if (us && ussr && us != ussr)
			{
				selectedHQs = {us, ussr};
				Print("[AICF][CONSTRUCTION_PROBE_HQ] test_only=1 exact_pair=1 us_base=" + requestedUS + " ussr_base=" + requestedUSSR);
				return;
			}
			Print("[AICF][CONSTRUCTION_PROBE_HQ] requested pair unavailable", LogLevel.ERROR);
		}
		super.SelectHQs(candidates, controlPoints, selectedHQs);
		string desired;
		if (!System.GetCLIParam("aicfConstructionProbeUSBase", desired) || selectedHQs.Count() != 2)
			return;
		if (AICF_ConstructionOrder.EntityKey(selectedHQs[1].GetOwner().GetID()) == desired)
		{
			SCR_CampaignMilitaryBaseComponent swap = selectedHQs[0];
			selectedHQs[0] = selectedHQs[1];
			selectedHQs[1] = swap;
		}
		Print("[AICF][CONSTRUCTION_PROBE_HQ] test_only=1 us_base=" + AICF_ConstructionOrder.EntityKey(selectedHQs[0].GetOwner().GetID()));
	}
}

// Сужение, а не обход bounds: при radius=1 полный footprint не помещается.
modded class SCR_CampaignBuildingProviderComponent
{
	override float GetBuildingRadius()
	{
		float radius = super.GetBuildingRadius();
		string requested;
		if (System.GetCLIParam("aicfConstructionProbeRadius", requested))
			return Math.Min(radius, Math.Max(1, requested.ToFloat()));
		return radius;
	}
}
