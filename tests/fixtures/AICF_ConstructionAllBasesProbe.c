// Только isolated source copy. Полный остров одной стороны, полный supply pool.
// Поиск, общий бюджет, цена, coverage, placement и работа остаются production.
class AICF_ConstructionAllBasesProbe
{
	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfAllBasesProbe", value) && value == "1";
	}
}

// Захват всех control points не должен закрыть измерительную сессию.
modded class SCR_GameModeCampaign
{
	override protected void CheckForWinner()
	{
		if (AICF_ConstructionAllBasesProbe.Enabled())
			return;
		super.CheckForWinner();
	}
}

modded class AICF_VictorySystem
{
	override bool EvaluateAndEnd(SCR_GameModeCampaign campaign, AICF_FactionState usState, AICF_FactionState ussrState)
	{
		if (AICF_ConstructionAllBasesProbe.Enabled())
			return false;
		return super.EvaluateAndEnd(campaign, usState, ussrState);
	}
}

modded class AICF_ConstructionPlanner
{
	protected int m_iAICFAllStarted;
	protected int m_iAICFAllRefill;
	protected int m_iAICFAllSample;
	protected ref array<string> m_aAICFAllCoverage = {};

	protected bool AICF_AllPrepare()
	{
		SCR_CampaignFaction us = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!us || !manager)
			return false;
		// Мирный стенд: сохранившиеся гарнизоны не отвлекают измерение строительства.
		// Меняется реальное отношение фракций; AreEnemiesPresent не подменяется.
		array<Faction> factions = {};
		manager.GetFactionsList(factions);
		foreach (Faction faction : factions)
		{
			SCR_Faction other = SCR_Faction.Cast(faction);
			if (other && other != us)
				manager.SetFactionsFriendly(us, other);
		}
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		m_Campaign.GetBaseManager().GetBases(bases);
		int ownedUSCount;
		int providers;
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			if (!base || !base.GetOwner())
				continue;
			if (base.IsInitialized())
				base.SetFaction(us);
			if (base.GetFaction() == us)
				ownedUSCount++;
			SCR_CampaignBuildingProviderComponent provider = base.GetMasterProvider();
			string providerId = "NONE";
			float radius;
			if (provider && provider.GetOwner())
			{
				providers++;
				providerId = AICF_ConstructionOrder.EntityKey(provider.GetOwner().GetID());
				radius = provider.GetBuildingRadius();
			}
			float missing = base.GetSuppliesMax() - base.GetSupplies();
			if (missing > 0)
				base.AddSupplies(missing);
			string fields = string.Format("test_only=1 base=%1 faction=US owned=%2 initialized=%3 hq=%4 control_point=%5 provider=%6 radius=%7",
				AICF_ConstructionOrder.EntityKey(base.GetOwner().GetID()), base.GetFaction() == us, base.IsInitialized(), base.IsHQ(), base.IsControlPoint(), providerId, radius);
			fields += string.Format(" supplies=%1 capacity=%2 position=%3 name=\"%4\" entity_name=\"%5\"",
				base.GetSupplies(), base.GetSuppliesMax(), base.GetOwner().GetOrigin(), WidgetManager.Translate(base.GetBaseName()), base.GetOwner().GetName());
			AICF_Stage1Diagnostics.Info("CONSTRUCTION_ALL_BASE_BASELINE", fields);
		}
		m_iAICFAllStarted = System.GetTickCount();
		AICF_Stage1Diagnostics.Info("CONSTRUCTION_ALL_BASES_PREPARED", string.Format("test_only=1 bases=%1 owned_us=%2 providers=%3 mode=US peace=1 victory_suspended=1 concurrent_orders=production refill_ms=5000", bases.Count(), ownedUSCount, providers));
		return true;
	}

	override protected bool Covered(AICF_ConstructionOrder order, AICF_EConstructionType type)
	{
		bool covered = super.Covered(order, type);
		if (!AICF_ConstructionAllBasesProbe.Enabled())
			return covered;
		string key = order.m_BaseId.ToString() + ":" + type + ":" + covered;
		if (!m_aAICFAllCoverage.Contains(key))
		{
			m_aAICFAllCoverage.Insert(key);
			order.Log("CONSTRUCTION_ALL_BASE_COVERAGE", string.Format("test_only=1 checked_type=%1 covered=%2", typename.EnumToString(AICF_EConstructionType, type), covered));
		}
		return covered;
	}

	override protected void Decide(AICF_ConstructionBaseState state, int now)
	{
		super.Decide(state, now);
		if (!AICF_ConstructionAllBasesProbe.Enabled() || state.m_Order || !state.m_Base || !state.m_Base.GetOwner())
			return;
		SCR_CampaignBuildingProviderComponent provider = state.m_Base.GetMasterProvider();
		string reason = "NO_ORDER_SEE_COVERAGE_OR_DEFERRED";
		if (!state.m_Base.IsInitialized())
			reason = "NOT_INITIALIZED";
		else if (!provider || !provider.GetOwner())
			reason = "NO_PROVIDER";
		else if (m_Builders.HasUnfinishedWork(state.m_Base))
			reason = "UNFINISHED_WORK";
		AICF_Stage1Diagnostics.Info("CONSTRUCTION_ALL_BASE_IDLE", string.Format("test_only=1 base=%1 reason=%2 supplies=%3 capacity=%4", AICF_ConstructionOrder.EntityKey(state.m_BaseId), reason, state.m_Base.GetSupplies(), state.m_Base.GetSuppliesMax()));
	}

	override void Update()
	{
		if (!AICF_ConstructionAllBasesProbe.Enabled())
		{
			super.Update();
			return;
		}
		if (m_bStopped || !Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster() || !m_Campaign.IsRunning())
			return;
		if (!m_iAICFAllStarted && !AICF_AllPrepare())
			return;
		int now = System.GetTickCount();
		if (now >= m_iAICFAllRefill)
		{
			m_iAICFAllRefill = now + 5000;
			array<SCR_CampaignMilitaryBaseComponent> bases = {};
			m_Campaign.GetBaseManager().GetBases(bases);
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				if (!base || !base.GetOwner())
					continue;
				float missing = base.GetSuppliesMax() - base.GetSupplies();
				if (missing > 0)
					base.AddSupplies(missing);
			}
		}
		super.Update();
		if (now >= m_iAICFAllSample)
		{
			m_iAICFAllSample = now + 30000;
			int active;
			foreach (AICF_ConstructionBaseState state : m_aBases)
			{
				if (!state.m_Order)
					continue;
				active++;
				state.m_Order.LogSearch();
				state.m_Order.Log("CONSTRUCTION_ALL_BASE_SAMPLE", "test_only=1 stage=" + state.m_Order.m_iStage);
			}
			AICF_Stage1Diagnostics.Info("CONSTRUCTION_ALL_BASES_SAMPLE", string.Format("test_only=1 elapsed_ms=%1 active_orders=%2 bases=%3", now - m_iAICFAllStarted, active, m_aBases.Count()));
		}
		string durationCLI;
		int duration = 900000;
		if (System.GetCLIParam("aicfAllBasesProbeMs", durationCLI))
			duration = Math.ClampInt(durationCLI.ToInt(), 60000, 3600000);
		if (now - m_iAICFAllStarted >= duration)
		{
			AICF_Stage1Diagnostics.Info("CONSTRUCTION_ALL_BASES_DONE", "test_only=1 elapsed_ms=" + (now - m_iAICFAllStarted));
			Stop();
			GetGame().RequestClose();
		}
	}
}
