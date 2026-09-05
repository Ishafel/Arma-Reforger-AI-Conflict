// Только test-only preparation. Все decisions, поиск, оплата, создание layout
// и worker completion идут через production domain без бесплатного spawn.
class AICF_ConstructionDeferredProbe
{
	protected static ref array<ref AICF_ConstructionDeferredProbe> s_aPending = {};
	protected ref AICF_ConstructionOrder m_Order;
	protected int m_iDue;

	static void Observe(AICF_ConstructionOrder order)
	{
		if (!order || !order.m_Provider || !order.m_Consumer)
			return;
		AICF_ConstructionDeferredProbe probe = new AICF_ConstructionDeferredProbe();
		probe.m_Order = order;
		probe.m_iDue = System.GetTickCount() + 5000;
		s_aPending.Insert(probe);
	}

	static void Update()
	{
		for (int i = s_aPending.Count() - 1; i >= 0; i--)
		{
			AICF_ConstructionDeferredProbe probe = s_aPending[i];
			if (System.GetTickCount() < probe.m_iDue)
				continue;
			AICF_ConstructionOrder order = probe.m_Order;
			if (order.m_Provider && order.m_Provider.GetOwner() && order.m_Provider.GetOwner().GetID() == order.m_ProviderId && order.m_Consumer)
			{
				Print(string.Format("[AICF][CONSTRUCTION_DEFERRED_PROBE] test_only=1 token=%1 accepted=%2 props_current=%3 props_expected=%4 supplies_current=%5 supplies_at_commit=%6 root_present=%7",
					order.m_sToken, order.m_bAccepted, order.m_Provider.GetCurrentPropValue(), order.m_iPropsAfter,
					order.m_Consumer.GetAggregatedResourceValue(), order.m_fAfter, order.m_Composition && order.m_Composition.GetOwner() != null));
			}
			s_aPending.Remove(i);
		}
	}

	static void Clear()
	{
		s_aPending.Clear();
	}
}

modded class AICF_ConstructionPlanner
{
	protected int m_iAICFProbeStart;
	protected int m_iAICFProbeLastSample;
	protected int m_iAICFProbeMaxTick;
	protected int m_iAICFProbeMaxQueries;
	protected int m_iAICFProbeTicks;
	protected int m_iAICFProbeRefillAt;

	override void Stop()
	{
		AICF_ConstructionDeferredProbe.Clear();
		super.Stop();
	}

	override protected void InitializeBases(int now)
	{
		super.InitializeBases(now);
		AICF_ProbeExitEndpoints();
		AICF_ProbeCandidateCoverage();
		string type;
		if (System.GetCLIParam("aicfConstructionProbeType", type))
		{
			foreach (AICF_ConstructionBaseState state : m_aBases)
				state.m_iNextType = Math.ClampInt(type.ToInt(), 0, 4);
			Print("[AICF][CONSTRUCTION_PROBE_TYPE] test_only=1 first_type=" + type);
		}
	}

	protected void AICF_ProbeExitEndpoints()
	{
		AICF_ConstructionOrder order = new AICF_ConstructionOrder();
		Math3D.MatrixIdentity4(order.m_aTransform);
		int passed;
		if (AICF_ConstructionSiteSearch.WorkClearOfExits(order, "0 0 0"))
			passed++;
		AICF_ConstructionVolume volume = new AICF_ConstructionVolume();
		volume.m_vMin = "-2 0 -10";
		volume.m_vMax = "2 4 -2";
		order.m_aExits.Insert(volume);
		if (!AICF_ConstructionSiteSearch.WorkClearOfExits(order, "0 0 -6"))
			passed++;
		if (!AICF_ConstructionSiteSearch.WorkClearOfExits(order, "3.5 0 -6"))
			passed++;
		if (AICF_ConstructionSiteSearch.WorkClearOfExits(order, "5 0 -6"))
			passed++;
		Math3D.AnglesToMatrix("90 0 0", order.m_aTransform);
		order.m_aTransform[3] = "100 12 200";
		vector inside = order.m_aTransform[3] - order.m_aTransform[2] * 6;
		if (!AICF_ConstructionSiteSearch.WorkClearOfExits(order, inside))
			passed++;
		if (AICF_ConstructionSiteSearch.WorkClearOfExits(order, inside + order.m_aTransform[0] * 5))
			passed++;
		Print(string.Format("[AICF][CONSTRUCTION_ENDPOINT_CONTRACT] test_only=1 passed=%1 total=6", passed));
		if (passed != 6)
			Print("[AICF][CONSTRUCTION_ENDPOINT_CONTRACT] FAILED", LogLevel.ERROR);
	}

	protected void AICF_ProbeCandidateCoverage()
	{
		array<vector> positions = {};
		array<int> orientations = {};
		bool unique = true;
		bool bounded = true;
		bool repeated = true;
		for (int i; i < 128; i++)
		{
			float yaw, nextYaw;
			vector point = AICF_ConstructionSiteSearch.CandidateOffset(i, 12, 120, yaw);
			vector next = AICF_ConstructionSiteSearch.CandidateOffset(i + 128, 12, 120, nextYaw);
			float distance = point.Length();
			if (distance < 19.99 || distance > 120.01)
				bounded = false;
			foreach (vector previous : positions)
			{
				if (vector.DistanceSqXZ(previous, point) < 0.01)
					unique = false;
			}
			positions.Insert(point);
			if (i < 8 && !orientations.Contains(yaw))
				orientations.Insert(yaw);
			if (vector.DistanceSqXZ(point, next) > 0.01 || yaw == nextYaw)
				repeated = false;
		}
		float smallYaw;
		vector small = AICF_ConstructionSiteSearch.CandidateOffset(15, 12, 5, smallYaw);
		int passed;
		if (unique) passed++;
		if (bounded) passed++;
		if (orientations.Count() == 8) passed++;
		if (repeated) passed++;
		if (small.Length() <= 5.01) passed++;
		Print(string.Format("[AICF][CONSTRUCTION_CANDIDATE_CONTRACT] test_only=1 passed=%1 total=5", passed));
		if (passed != 5)
			Print("[AICF][CONSTRUCTION_CANDIDATE_CONTRACT] FAILED", LogLevel.ERROR);
	}

	override void Update()
	{
		if (m_bStopped)
			return;
		string mode;
		if (!System.GetCLIParam("aicfConstructionProbe", mode) || mode != "1")
		{
			super.Update();
			return;
		}
		int now = System.GetTickCount();
		AICF_ConstructionDeferredProbe.Update();
		string refill;
		bool refillDue = System.GetCLIParam("aicfConstructionProbeRefill", refill) && refill == "1" && now - m_iAICFProbeRefillAt >= 60000;
		if (!m_iAICFProbeStart || refillDue)
		{
			if (!m_iAICFProbeStart)
				m_iAICFProbeStart = now;
			m_iAICFProbeRefillAt = now;
			array<SCR_CampaignMilitaryBaseComponent> bases = {};
			m_Campaign.GetBaseManager().GetBases(bases);
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(base.GetFaction());
				if (!faction || !Commander(faction))
					continue;
				int supplyTarget = base.GetSuppliesMax();
				string supplyCLI;
				if (System.GetCLIParam("aicfConstructionProbeSupplies", supplyCLI))
					supplyTarget = Math.ClampInt(supplyCLI.ToInt(), 0, base.GetSuppliesMax());
				base.AddSupplies(supplyTarget - base.GetSupplies());
				Print(string.Format("[AICF][CONSTRUCTION_PROBE_PREPARE] test_only=1 base=%1 supplies=%2", base.GetOwner().GetID(), base.GetSupplies()));
			}
		}
		int before = System.GetTickCount();
		// Optional focused regression: повторять production search данного типа
		// после cooldown, чтобы проверить продолжение cursor за один bounded run.
		string repeatType;
		if (System.GetCLIParam("aicfConstructionProbeRepeatType", repeatType))
		{
			foreach (AICF_ConstructionBaseState state : m_aBases)
			{
				if (!state.m_Order)
					state.m_iNextType = Math.ClampInt(repeatType.ToInt(), 0, 4);
			}
		}
		super.Update();
		int elapsed = System.GetTickCount() - before;
		if (elapsed > 20)
			Print(string.Format("[AICF][CONSTRUCTION_PROBE_SLOW_TICK] test_only=1 elapsed_ms=%1 queries=%2", elapsed, AICF_ConstructionSiteSearch.AICF_ProbeQueries()));
		m_iAICFProbeMaxTick = Math.Max(m_iAICFProbeMaxTick, System.GetTickCount() - before);
		m_iAICFProbeMaxQueries = Math.Max(m_iAICFProbeMaxQueries, AICF_ConstructionSiteSearch.AICF_ProbeQueries());
		m_iAICFProbeTicks++;
		if (now - m_iAICFProbeLastSample >= 10000)
		{
			m_iAICFProbeLastSample = now;
			foreach (AICF_ConstructionBaseState state : m_aBases)
			{
				if (state.m_Order)
					state.m_Order.Log("CONSTRUCTION_PROBE_SAMPLE", "stage=" + state.m_Order.m_iStage);
			}
		}
		int duration = 420000;
		string durationCLI;
		if (System.GetCLIParam("aicfConstructionProbeMs", durationCLI))
			duration = Math.ClampInt(durationCLI.ToInt(), 60000, 3600000);
		if (now - m_iAICFProbeStart >= duration)
		{
			Print(string.Format("[AICF][CONSTRUCTION_PROBE_DONE] stopped=1 test_only=1 ticks=%1 max_tick_ms=%2 max_window_queries=%3 duration_ms=%4",
				m_iAICFProbeTicks, m_iAICFProbeMaxTick, m_iAICFProbeMaxQueries, now - m_iAICFProbeStart));
			Stop();
			GetGame().RequestClose();
		}
	}
}

// Полный индекс отказов для воспроизведения проблем конкретной стартовой базы.
// Дополнительных geometry queries и изменений gameplay state нет.
modded class AICF_ConstructionOrder
{
	override void RejectCandidate()
	{
		super.RejectCandidate();
		string trace;
		if (System.GetCLIParam("aicfConstructionProbeTrace", trace) && trace == "1")
			Log("CONSTRUCTION_CANDIDATE_PROBE", string.Format("test_only=1 obstacle=%1 terrain_delta=%2", m_sObstacle, m_fMaxHeight - m_fMinHeight));
	}
}

// Только наблюдение счётчика; дополнительных physics/navmesh queries нет.
modded class AICF_ConstructionSiteSearch
{
	static int AICF_ProbeQueries()
	{
		return s_iQueries;
	}
}

// Частичный сбой в пределах уже открытой транзакции. Оплата, сравнение
// баланса, rollback и удаление layout остаются в production path.
modded class AICF_StockConstructionAdapter
{
	override bool Place(AICF_ConstructionOrder order, AICF_ConstructionConfig config, AICF_EconomySystem economy,
		SCR_CampaignBuildingManagerComponent manager, AICF_BaseBuilderService builders)
	{
		bool result = super.Place(order, config, economy, manager, builders);
		AICF_ConstructionDeferredProbe.Observe(order);
		if (result)
			order.m_Composition.AICF_ProbeMark(order.m_sToken, order.m_eType);
		return result;
	}

	override protected bool PayConstruction(AICF_ConstructionOrder order, AICF_EconomySystem economy, SCR_CampaignBuildingManagerComponent manager)
	{
		string fault;
		if (System.GetCLIParam("aicfConstructionProbeFault", fault) && fault == "partial_debit")
		{
			Print("[AICF][CONSTRUCTION_PROBE_FAULT] test_only=1 fault=partial_debit");
			order.m_Consumer.RequestConsumtion(order.m_iCost * 0.5);
		}
		return super.PayConstruction(order, economy, manager);
	}
}

// Test-only marker связывает server token с наблюдаемой клиентом stock entity.
// Provider, layout progress и service state читаются из stock replication.
modded class SCR_CampaignBuildingCompositionComponent
{
	[RplProp(onRplName: "AICF_ProbeReplicated")]
	protected string m_sAICFProbeToken;
	[RplProp()]
	protected AICF_EConstructionType m_eAICFProbeType;
	protected int m_iAICFProbeSamples;

	// Stock component имеет собственный RplSave/RplLoad. Marker также явно
	// включён в JIP stream; provider и gameplay state сериализует только super.
	override bool RplSave(ScriptBitWriter writer)
	{
		if (!super.RplSave(writer))
			return false;
		writer.WriteString(m_sAICFProbeToken);
		writer.WriteInt(m_eAICFProbeType);
		if (!m_sAICFProbeToken.IsEmpty())
			Print("[AICF][CONSTRUCTION_RPL_PROBE_SAVE] test_only=1 token=" + m_sAICFProbeToken);
		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		if (!super.RplLoad(reader))
			return false;
		reader.ReadString(m_sAICFProbeToken);
		reader.ReadInt(m_eAICFProbeType);
		AICF_ProbeReplicated();
		return true;
	}

	void AICF_ProbeMark(string token, AICF_EConstructionType type)
	{
		m_sAICFProbeToken = token;
		m_eAICFProbeType = type;
		Replication.BumpMe();
	}

	protected void AICF_ProbeReplicated()
	{
		if (Replication.IsServer() || m_sAICFProbeToken.IsEmpty())
			return;
		GetGame().GetCallqueue().Remove(AICF_ProbeClientSample);
		GetGame().GetCallqueue().CallLater(AICF_ProbeClientSample, 1000, true);
	}

	protected void AICF_ProbeClientSample()
	{
		if (!GetOwner() || m_iAICFProbeSamples++ >= 180)
		{
			GetGame().GetCallqueue().Remove(AICF_ProbeClientSample);
			return;
		}
		if (m_iAICFProbeSamples % 10 != 1)
			return;
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		SCR_CampaignBuildingLayoutComponent layout;
		IEntity child = GetOwner().GetChildren();
		while (child)
		{
			layout = SCR_CampaignBuildingLayoutComponent.Cast(child.FindComponent(SCR_CampaignBuildingLayoutComponent));
			if (layout)
				break;
			child = child.GetSibling();
		}
		Print(string.Format("[AICF][CONSTRUCTION_CLIENT_PROBE] test_only=1 token=%1 type=%2 root_rpl=%3 proxy=%4 provider_present=%5 layout_present=%6 spawned=%7 service_online=%8 position=%9",
			m_sAICFProbeToken, typename.EnumToString(AICF_EConstructionType, m_eAICFProbeType), rpl.Id(), rpl.IsProxy(),
			GetProviderEntity() != null, layout != null, IsCompositionSpawned(),
			AICF_ConstructionMetadata.HasOnlineService(GetOwner(), m_eAICFProbeType), GetOwner().GetOrigin()));
	}

	void ~SCR_CampaignBuildingCompositionComponent()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AICF_ProbeClientSample);
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() && System.GetCLIParam("aicfConstructionClientProbe", enabled) && enabled == "1")
		{
			Print("[AICF][CONSTRUCTION_CLIENT_PROBE_STARTED] test_only=1");
			GetGame().GetCallqueue().CallLater(AICF_ProbeClientClose, 180000, false);
		}
	}

	protected void AICF_ProbeClientClose()
	{
		Print("[AICF][CONSTRUCTION_CLIENT_PROBE_DONE] test_only=1");
		GetGame().RequestClose();
	}

	void ~SCR_GameModeCampaign()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AICF_ProbeClientClose);
	}
}
