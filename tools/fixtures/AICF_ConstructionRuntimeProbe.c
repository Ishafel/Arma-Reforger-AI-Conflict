// Только test-only preparation. Все decisions, поиск, оплата, создание layout
// и worker completion идут через production domain без бесплатного spawn.
class AICF_ConstructionMatrixProbe
{
	protected static ref array<bool> s_aCompleted = {false, false, false, false, false, false, false, false, false, false};
	protected static int s_iType;
	protected static int s_iPhaseStarted;
	protected static bool s_bInitialized;
	protected static int s_iLastType = AICF_EConstructionType.COUNT - 1;

	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfConstructionProbeMatrix", value) && value == "1";
	}

	static int CurrentType()
	{
		if (!s_bInitialized)
		{
			s_bInitialized = true;
			string type;
			if (System.GetCLIParam("aicfConstructionProbeMatrixType", type))
			{
				s_iType = Math.ClampInt(type.ToInt(), 0, AICF_EConstructionType.COUNT - 1);
				s_iLastType = s_iType;
			}
		}
		return s_iType;
	}

	static bool SideCompleted(FactionKey faction, int type)
	{
		if (type < 0 || type >= AICF_EConstructionType.COUNT)
			return true;
		int side;
		if (faction == "USSR")
			side = 1;
		return s_aCompleted[type * 2 + side];
	}

	static void Observe(AICF_ConstructionOrder order)
	{
		if (!Enabled() || !order.m_bAccepted || !order.m_Composition ||
			!AICF_ConstructionMetadata.HasOnlineService(order.m_Composition.GetOwner(), order.m_eType))
			return;
		int side;
		if (order.m_sFaction == "USSR")
			side = 1;
		else if (order.m_sFaction != "US")
			return;
		s_aCompleted[order.m_eType * 2 + side] = true;
		Print(string.Format("[AICF][CONSTRUCTION_MATRIX_CASE] test_only=1 faction=%1 type=%2 token=%3 completed=1",
			order.m_sFaction, typename.EnumToString(AICF_EConstructionType, order.m_eType), order.m_sToken));
	}

	static bool Update(int now)
	{
		if (!Enabled())
			return false;
		if (CurrentType() > s_iLastType)
			return true;
		if (!s_iPhaseStarted)
		{
			s_iPhaseStarted = now;
			Print("[AICF][CONSTRUCTION_MATRIX_PHASE] test_only=1 begin=1 type=" + s_iType);
		}
		int phaseMs = 300000;
		string phaseCLI;
		if (System.GetCLIParam("aicfConstructionProbeMatrixPhaseMs", phaseCLI))
			phaseMs = Math.ClampInt(phaseCLI.ToInt(), 60000, 1200000);
		if ((!SideCompleted("US", s_iType) || !SideCompleted("USSR", s_iType)) && now - s_iPhaseStarted < phaseMs)
			return false;
		Print(string.Format("[AICF][CONSTRUCTION_MATRIX_PHASE] test_only=1 end=1 type=%1 us_completed=%2 ussr_completed=%3 elapsed_ms=%4",
			s_iType, SideCompleted("US", s_iType), SideCompleted("USSR", s_iType), now - s_iPhaseStarted));
		s_iType++;
		s_iPhaseStarted = 0;
		return s_iType > s_iLastType;
	}
}

// Matrix задаёт только проверяемую потребность. Coverage/authority не обходятся.
modded class AICF_AICommander
{
	override int SelectConstructionType(array<bool> covered, int startType)
	{
		if (!AICF_ConstructionMatrixProbe.Enabled())
			return super.SelectConstructionType(covered, startType);
		int type = AICF_ConstructionMatrixProbe.CurrentType();
		if (!covered || covered.Count() != AICF_EConstructionType.COUNT || type >= covered.Count() ||
			covered[type] || AICF_ConstructionMatrixProbe.SideCompleted(AICF_ContentProfile.GetActive().GetStableFactionKey(GetFactionKey()), type))
			return -1;
		return super.SelectConstructionType(covered, type);
	}
}

class AICF_ConstructionMetadataProbe : AICF_ConstructionMetadata
{
	static void Run()
	{
		AICF_ConstructionMetadataProbe metadata = new AICF_ConstructionMetadataProbe();
		for (int i; i < 400; i++)
		{
			AICF_ConstructionVolume volume = new AICF_ConstructionVolume();
			int column = i % 20;
			int row = i / 20;
			volume.m_vMin = Vector(column * 2, 0.3, row * 2);
			volume.m_vMax = volume.m_vMin + "1 2 1";
			metadata.m_aCollisionVolumes.Insert(volume);
		}
		bool done;
		bool bounded = true;
		for (int tick; tick < 2000 && !done; tick++)
		{
			done = metadata.CompactCollisionVolumes(1);
			if (metadata.m_aCollisionVolumes.Count() > 48)
				bounded = false;
		}
		int covered;
		for (int i; i < 400; i++)
		{
			int column = i % 20;
			int row = i / 20;
			vector mins = Vector(column * 2, 0.3, row * 2);
			vector maxs = mins + "1 2 1";
			foreach (AICF_ConstructionVolume volume : metadata.m_aCollisionVolumes)
			{
				if (volume.m_vMin[0] <= mins[0] && volume.m_vMin[1] <= mins[1] && volume.m_vMin[2] <= mins[2] &&
					volume.m_vMax[0] >= maxs[0] && volume.m_vMax[1] >= maxs[1] && volume.m_vMax[2] >= maxs[2])
				{
					covered++;
					break;
				}
			}
		}
		int passed;
		if (done) passed++;
		if (bounded) passed++;
		if (metadata.m_aCollisionVolumes.Count() <= 24) passed++;
		if (covered == 400) passed++;
		Print(string.Format("[AICF][CONSTRUCTION_METADATA_CONTRACT] test_only=1 passed=%1 total=4 covered=%2", passed, covered));
		if (passed != 4)
			Print("[AICF][CONSTRUCTION_METADATA_CONTRACT] FAILED", LogLevel.ERROR);
	}
}

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
	protected bool m_bAICFMatrixPrepared;
	protected ref array<string> m_aAICFMatrixCoverage = {};

	override protected bool Covered(AICF_ConstructionOrder order, AICF_EConstructionType type)
	{
		bool covered = super.Covered(order, type);
		if (!AICF_ConstructionMatrixProbe.Enabled())
			return covered;
		string key = order.m_BaseId.ToString() + ":" + order.m_sFaction + ":" + type + ":" + covered;
		if (!m_aAICFMatrixCoverage.Contains(key))
		{
			m_aAICFMatrixCoverage.Insert(key);
			Print(string.Format("[AICF][CONSTRUCTION_MATRIX_COVERAGE] test_only=1 base=%1 faction=%2 type=%3 covered=%4",
				order.m_BaseId, order.m_sFaction, typename.EnumToString(AICF_EConstructionType, type), covered));
			if (type == AICF_EConstructionType.LARGE_BARRACKS && covered)
			{
				foreach (IEntity entity : m_aInventory)
				{
					if (!entity)
						continue;
					SCR_ServicePointComponent service = SCR_ServicePointComponent.Cast(entity.FindComponent(SCR_ServicePointComponent));
					if (!service || service.GetType() != SCR_EServicePointType.BARRACKS)
						continue;
					IEntity root = entity.GetRootParent();
					if (!root)
						continue;
					SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(root.FindComponent(SCR_EditableEntityComponent));
					SCR_EditableEntityUIInfo info;
					if (editable)
						info = SCR_EditableEntityUIInfo.Cast(editable.GetInfo());
					bool small = info && info.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_SMALL);
					bool large = info && info.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_LARGE);
					ResourceName prefab;
					if (root.GetPrefabData())
						prefab = root.GetPrefabData().GetPrefabName();
					Print(string.Format("[AICF][CONSTRUCTION_MATRIX_BARRACKS] test_only=1 base=%1 entity=%2 root=%3 small=%4 large=%5 prefab=%6",
						order.m_BaseId, entity.GetID(), root.GetID(), small, large, prefab));
				}
			}
		}
		return covered;
	}

	protected void AICF_PrepareMatrix()
	{
		if (m_bAICFMatrixPrepared || !AICF_ConstructionMatrixProbe.Enabled())
			return;
		m_bAICFMatrixPrepared = true;
		SCR_CampaignFaction us = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		SCR_CampaignFaction ussr = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
		if (!us || !ussr || !us.GetMainBase() || !ussr.GetMainBase())
			return;
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		m_Campaign.GetBaseManager().GetBases(bases);
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			if (!base || !base.GetOwner() || !base.IsInitialized() || base.IsControlPoint() || base == us.GetMainBase() || base == ussr.GetMainBase())
				continue;
			// Только подготовка isolated world: базы распределяются по близости
			// к HQ, без координат карты и без изменения installed world resources.
			vector position = base.GetOwner().GetOrigin();
			SCR_CampaignFaction owner = us;
			if (vector.DistanceSqXZ(position, ussr.GetMainBase().GetOwner().GetOrigin()) < vector.DistanceSqXZ(position, us.GetMainBase().GetOwner().GetOrigin()))
				owner = ussr;
			base.SetFaction(owner);
			Print(string.Format("[AICF][CONSTRUCTION_MATRIX_PREPARE] test_only=1 base=%1 faction=%2 position=%3",
				base.GetOwner().GetID(), owner.GetFactionKey(), position));
		}
	}

	override protected void Decide(AICF_ConstructionBaseState state, int now)
	{
		if (AICF_ConstructionMatrixProbe.Enabled())
		{
			// Одна проверяемая стройка на сторону: не расходуем квоту десятком
			// одинаковых orders. После отказа доступна следующая штатная база.
			foreach (AICF_ConstructionBaseState other : m_aBases)
			{
				if (other != state && other.m_Order && other.m_Base.GetFaction() == state.m_Base.GetFaction())
				{
					state.m_iDueAt = now + 1000;
					return;
				}
			}
			state.m_iNextType = AICF_ConstructionMatrixProbe.CurrentType();
		}
		super.Decide(state, now);
	}

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
		AICF_ProbeOrientedBounds();
		AICF_ProbeTerrainSlope();
		AICF_ConstructionMetadataProbe.Run();
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

	protected void AICF_ProbeTerrainSlope()
	{
		int passed;
		if (AICF_ConstructionSiteSearch.TerrainEdgeSupported(1, 1, 3, 0.8, 3)) passed++;
		if (AICF_ConstructionSiteSearch.TerrainEdgeSupported(1.6, 1, 3, 0.8, 3)) passed++;
		if (!AICF_ConstructionSiteSearch.TerrainEdgeSupported(2, 1, 3, 0.8, 3)) passed++;
		if (!AICF_ConstructionSiteSearch.TerrainEdgeSupported(1.6, 1, 1, 0.8, 3)) passed++;
		if (!AICF_ConstructionSiteSearch.TerrainEdgeSupported(1, 1, 0, 0.8, 3)) passed++;
		bool gentle = true;
		for (int i = 1; i <= 5; i++)
			gentle = gentle && AICF_ConstructionSiteSearch.TerrainEdgeSupported(i * 0.6, (i - 1) * 0.6, 3, 0.8, 3);
		if (gentle) passed++;
		Print(string.Format("[AICF][CONSTRUCTION_TERRAIN_CONTRACT] test_only=1 passed=%1 total=6", passed));
		if (passed != 6)
			Print("[AICF][CONSTRUCTION_TERRAIN_CONTRACT] FAILED", LogLevel.ERROR);
	}

	protected void AICF_ProbeCandidateCoverage()
	{
		array<vector> positions = {};
		array<int> orientations = {};
		bool unique = true;
		bool bounded = true;
		bool coverage = true;
		bool nearProvider;
		bool progressive = true;
		bool deterministic = true;
		for (int i; i < 256; i++)
		{
			float yaw, nextYaw;
			vector point = AICF_ConstructionSiteSearch.CandidateOffset(i, 12, 120, yaw);
			vector next = AICF_ConstructionSiteSearch.CandidateOffset(i + 64, 12, 120, nextYaw);
			float distance = point.Length();
			if (distance < 7.99 || distance > 120.01)
				bounded = false;
			if (distance < 20)
				nearProvider = true;
			if (vector.DistanceSqXZ(point, next) < 0.01)
				progressive = false;
			foreach (vector previous : positions)
			{
				if (vector.DistanceSqXZ(previous, point) < 0.01)
					unique = false;
			}
			positions.Insert(point);
			if (!orientations.Contains(yaw))
				orientations.Insert(yaw);
			next = AICF_ConstructionSiteSearch.CandidateOffset(i, 12, 120, nextYaw);
			if (vector.DistanceSqXZ(point, next) > 0.01 || yaw != nextYaw)
				deterministic = false;
		}
		// Требование к покрытию площади, не повторение формулы генератора.
		for (int x = -80; x <= 80; x += 20)
		{
			for (int z = -80; z <= 80; z += 20)
			{
				float nearest = float.MAX;
				foreach (vector position : positions)
					nearest = Math.Min(nearest, vector.DistanceSqXZ(Vector(x, 0, z), position));
				if (nearest > 20 * 20)
					coverage = false;
			}
		}
		float smallYaw;
		vector small = AICF_ConstructionSiteSearch.CandidateOffset(15, 12, 5, smallYaw);
		int passed;
		if (unique) passed++;
		if (bounded) passed++;
		if (orientations.Count() == 8) passed++;
		if (coverage) passed++;
		if (small.Length() <= 5.01) passed++;
		if (nearProvider) passed++;
		if (progressive) passed++;
		if (deterministic) passed++;
		Print(string.Format("[AICF][CONSTRUCTION_CANDIDATE_CONTRACT] test_only=1 passed=%1 total=8", passed));
		if (passed != 8)
			Print("[AICF][CONSTRUCTION_CANDIDATE_CONTRACT] FAILED", LogLevel.ERROR);
	}

	protected void AICF_ProbeOrientedBounds()
	{
		vector transform[4];
		Math3D.AnglesToMatrix("45 0 0", transform);
		transform[3] = "100 0 100";
		int passed;
		// Повернутый квадрат помещается в круг 12 м, его world AABB — нет.
		if (AICF_ConstructionSiteSearch.VolumeInsideBounds("-8 0 -8", "8 4 8", transform, "0 0 0", "200 0 200", "100 0 100", 12)) passed++;
		if (!AICF_ConstructionSiteSearch.VolumeInsideBounds("-8 0 -8", "8 4 8", transform, "0 0 0", "200 0 200", "100 0 100", 10)) passed++;
		if (!AICF_ConstructionSiteSearch.VolumeInsideBounds("-8 0 -8", "8 4 8", transform, "90 0 90", "200 0 200", "100 0 100", 12)) passed++;
		if (!AICF_ConstructionSiteSearch.VolumeInsideBounds("-8 0 -8", "8 4 8", transform, "0 0 0", "110 0 110", "100 0 100", 12)) passed++;
		// Смещённый provider и выход за пределы круга остаются отказом.
		if (!AICF_ConstructionSiteSearch.VolumeInsideBounds("-8 0 -8", "8 4 8", transform, "0 0 0", "200 0 200", "110 0 100", 12)) passed++;
		if (!AICF_ConstructionSiteSearch.VolumeInsideBounds("-2 0 8", "2 4 28", transform, "0 0 0", "200 0 200", "100 0 100", 12)) passed++;
		Print(string.Format("[AICF][CONSTRUCTION_BOUNDS_CONTRACT] test_only=1 passed=%1 total=6", passed));
		if (passed != 6)
			Print("[AICF][CONSTRUCTION_BOUNDS_CONTRACT] FAILED", LogLevel.ERROR);
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
		AICF_PrepareMatrix();
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
		if (m_iCandidatesThisTick > m_Config.m_iCandidatesPerTick)
			Print("[AICF][CONSTRUCTION_SCHEDULER_CONTRACT] candidate budget exceeded", LogLevel.ERROR);
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
		if (AICF_ConstructionMatrixProbe.Update(now) || now - m_iAICFProbeStart >= duration)
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
	override void Log(string eventName, string extra = "")
	{
		super.Log(eventName, extra);
		if (eventName == "CONSTRUCTION_COMPLETED")
			AICF_ConstructionMatrixProbe.Observe(this);
	}

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
