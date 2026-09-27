// Владелец очереди/identity, fairness и lifecycle. Стратегический выбор типа
// делегирован faction commander; worker и stock service остаются своими доменами.
class AICF_ConstructionPlanner
{
	protected static AICF_ConstructionPlanner s_Instance;
	protected SCR_GameModeCampaign m_Campaign;
	protected SCR_CampaignBuildingManagerComponent m_Manager;
	protected SCR_MilitaryBaseSystem m_BaseSystem;
	protected AICF_BaseBuilderService m_Builders;
	protected AICF_EconomySystem m_Economy;
	protected AICF_AICommander m_US;
	protected AICF_AICommander m_USSR;
	protected ref AICF_ConstructionConfig m_Config = new AICF_ConstructionConfig();
	protected ref AICF_ConstructionSiteSearch m_Search;
	protected ref AICF_StockConstructionAdapter m_Adapter = new AICF_StockConstructionAdapter();
	protected ref array<ref AICF_ConstructionBaseState> m_aBases = {};
	protected ref array<ref AICF_ConstructionMetadata> m_aMetadata = {};
	protected ref array<IEntity> m_aInventory = {};
	protected int m_iCursor;
	protected int m_iNextToken;
	protected int m_iCandidatesThisTick;
	protected bool m_bMetadataBatchThisTick;
	protected bool m_bPlacementAttemptedThisTick;
	protected bool m_bStopped;
	protected int m_iTotalCpuMs;
	protected int m_iMaxUpdateMs;
	protected int m_iUpdates;

	void Start(SCR_GameModeCampaign campaign, AICF_BaseBuilderService builders, AICF_EconomySystem economy, AICF_AICommander us, AICF_AICommander ussr)
	{
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster())
			return;
		s_Instance = this;
		m_Campaign = campaign;
		m_Builders = builders;
		m_Economy = economy;
		m_US = us;
		m_USSR = ussr;
		m_Manager = SCR_CampaignBuildingManagerComponent.Cast(campaign.FindComponent(SCR_CampaignBuildingManagerComponent));
		m_BaseSystem = SCR_MilitaryBaseSystem.GetInstance();
		if (m_BaseSystem)
			m_BaseSystem.GetOnBaseFactionChanged().Insert(OnOwnerChanged);
		if (m_Manager)
		{
			m_Manager.GetOnEntitySpawnedByProvider().Insert(OnPlayerPlaced);
			m_Manager.GetOnCompositionUnregistered().Insert(OnRemoved);
		}
		m_Search = new AICF_ConstructionSiteSearch(m_Config);
	}

	void Stop()
	{
		if (m_bStopped)
			return;
		m_bStopped = true;
		AICF_Stage1Diagnostics.Info("CONSTRUCTION_SCHEDULER_COST", string.Format("cpu_ms=%1 max_update_ms=%2 updates=%3", m_iTotalCpuMs, m_iMaxUpdateMs, m_iUpdates));
		if (m_BaseSystem)
			m_BaseSystem.GetOnBaseFactionChanged().Remove(OnOwnerChanged);
		if (m_Manager)
		{
			m_Manager.GetOnEntitySpawnedByProvider().Remove(OnPlayerPlaced);
			m_Manager.GetOnCompositionUnregistered().Remove(OnRemoved);
		}
		foreach (AICF_ConstructionBaseState state : m_aBases)
		{
			if (state.m_Order)
				Cancel(state, "STOP");
		}
		m_aBases.Clear();
		foreach (AICF_ConstructionMetadata metadata : m_aMetadata)
			metadata.ReleasePreview();
		m_aMetadata.Clear();
		m_aInventory.Clear();
		if (s_Instance == this)
			s_Instance = null;
		m_Campaign = null;
	}

	protected void OnOwnerChanged(SCR_MilitaryBaseComponent base, Faction faction)
	{
		foreach (AICF_ConstructionBaseState state : m_aBases)
		{
			if (state.m_Base == base && state.m_Order && !state.m_Order.m_bAccepted)
				Cancel(state, "OWNER_CHANGED");
		}
	}

	protected void OnPlayerPlaced(int prefabID, SCR_EditableEntityComponent entity, int playerId, SCR_CampaignBuildingProviderComponent provider)
	{
		if (!provider)
			return;
		foreach (AICF_ConstructionBaseState state : m_aBases)
		{
			if (state.m_Base == provider.GetCampaignMilitaryBaseComponent() && state.m_Order && !state.m_Order.m_bAccepted)
				Cancel(state, "PLAYER_PLACEMENT");
		}
	}

	protected void OnRemoved(SCR_CampaignBuildingCompositionComponent composition)
	{
		foreach (AICF_ConstructionBaseState state : m_aBases)
		{
			if (state.m_Order && state.m_Order.m_bAccepted && state.m_Order.m_Composition == composition)
				Cancel(state, "LAYOUT_REMOVED");
		}
	}

	protected AICF_AICommander Commander(SCR_CampaignFaction faction)
	{
		if (m_US && m_US.GetFaction() == faction && m_US.IsEnabled())
			return m_US;
		if (m_USSR && m_USSR.GetFaction() == faction && m_USSR.IsEnabled())
			return m_USSR;
		return null;
	}

	void Update()
	{
		if (m_bStopped || !Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster() || !m_Campaign.IsRunning() || !m_Manager)
			return;
		int now = System.GetTickCount();
		if (m_aBases.IsEmpty())
			InitializeBases(now);
		// Invalidation всех pending orders каждый tick, независимо от due/cursor.
		foreach (AICF_ConstructionBaseState state : m_aBases)
		{
			AICF_ConstructionOrder order = state.m_Order;
			if (!order)
				continue;
			if (order.m_bAccepted)
			{
				if (!order.m_Composition || !order.m_Composition.GetOwner())
					Cancel(state, "LAYOUT_REMOVED");
				else if (order.m_Composition.IsCompositionSpawned())
				{
					if (!AICF_ConstructionMetadata.HasOnlineService(order.m_Composition.GetOwner(), order.m_eType))
						continue;
					order.m_bSiteReserved = false;
					order.m_sReason = "STOCK_SERVICE_ONLINE";
					order.LogSearch();
					order.Log("CONSTRUCTION_COMPLETED", "service_online=1");
					state.m_aSearchFailures[order.m_eType] = 0;
					state.m_aSearchRetryAt[order.m_eType] = 0;
					state.m_Order = null;
					state.m_iNextType = 0;
					state.m_iSmallFailures = 0;
				}
				continue;
			}
			if (!order.IdentityValid() || !Commander(order.m_Faction) || m_Builders.HasUnfinishedWork(order.m_Base) ||
				order.m_Base.AreEnemiesPresent() || order.m_Base.IsBeingCaptured() || order.m_Base.GetCaptureState() != SCR_EBaseCaptureState.NONE)
				Cancel(state, "LIFECYCLE_INVALIDATED");
			else if (order.m_bPathContextSet && (!order.m_PathContext || order.m_PathContext != Commander(order.m_Faction).GetConstructionPathfinding()))
				// Включает geometry checkpoint и ожидание commit budget: эти
				// состояния не обязаны вызывать ValidatePath в текущем tick.
				Cancel(state, "NAVMESH_CONTEXT_CHANGED");
			else if (now >= order.m_iDeadline)
			{
				// Незавершённое вычисление продолжается без cooldown. Таймер
				// ограничивает вычисление, но не доказывает отсутствие места.
				if (order.m_iStage < 4 && (order.m_iStage > 0 || !order.m_aPendingCandidates.IsEmpty()) && order.m_iSearchWindows < 3)
				{
					order.m_iSearchWindows++;
					order.m_iResumeAt = now;
					order.m_iDeadline = now + m_Config.m_iDeadlineMs;
					order.LogSearch();
					order.Log("CONSTRUCTION_SEARCH_CONTINUED", "status=COMPUTING resume_at=" + order.m_iResumeAt);
				}
				else
					Cancel(state, "SEARCH_BUDGET_EXHAUSTED");
			}
			// Живой ожидающий commit сохраняет место в FIFO, даже если общий
			// slice не позволил посетить его базу. Cancel снимает claim явно.
			if (state.m_Order == order && order.m_iStage == 4)
				AICF_ConstructionSiteSearch.RefreshClaim(order.m_sToken);
		}
		if (m_aBases.IsEmpty())
			return;
		int sliceStarted = System.GetTickCount();
		m_iCandidatesThisTick = 0;
		m_bMetadataBatchThisTick = false;
		m_bPlacementAttemptedThisTick = false;
		int firstBase = m_iCursor;
		m_iCursor = (m_iCursor + 1) % m_aBases.Count();
		bool visitedOrder;
		// Pending orders делят одну квоту queries и времени. Ожидание terrain,
		// metadata или navmesh одной базы не должно останавливать остальные.
		for (int visited; visited < m_aBases.Count() && System.GetTickCount() - sliceStarted < m_Config.m_iSliceMs; visited++)
		{
			AICF_ConstructionBaseState selected = m_aBases[(firstBase + visited) % m_aBases.Count()];
			if (!selected.m_Order && now >= selected.m_iDueAt)
				Decide(selected, now);
			if (selected.m_Order && !selected.m_Order.m_bAccepted && now >= selected.m_Order.m_iResumeAt)
			{
				// Следующий tick начинает со следующей активной базы: длинные
				// промежутки neutral bases не отдают всю квоту одному HQ.
				if (!visitedOrder)
				{
					m_iCursor = (firstBase + visited + 1) % m_aBases.Count();
					visitedOrder = true;
				}
				Search(selected, sliceStarted);
				if (m_bPlacementAttemptedThisTick)
					break;
			}
		}
		int updateMs = System.GetTickCount() - now;
		m_iTotalCpuMs += updateMs;
		m_iMaxUpdateMs = Math.Max(m_iMaxUpdateMs, updateMs);
		m_iUpdates++;
	}

	protected void InitializeBases(int now)
	{
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		m_Campaign.GetBaseManager().GetBases(bases);
		// Детерминированная insertion sort по immutable entity identity.
		for (int i = 1; i < bases.Count(); i++)
		{
			SCR_CampaignMilitaryBaseComponent value = bases[i];
			int j = i - 1;
			while (j >= 0 && bases[j].GetOwner().GetID().ToString().Compare(value.GetOwner().GetID().ToString()) > 0)
			{
				bases[j + 1] = bases[j];
				j--;
			}
			bases[j + 1] = value;
		}
		foreach (int index, SCR_CampaignMilitaryBaseComponent base : bases)
		{
			AICF_ConstructionBaseState state = new AICF_ConstructionBaseState();
			state.m_Base = base;
			state.m_BaseId = base.GetOwner().GetID();
			state.m_iDueAt = now + index * m_Config.m_iDecisionMs / bases.Count();
			m_aBases.Insert(state);
		}
		AICF_Stage1Diagnostics.Info("CONSTRUCTION_STARTED", string.Format("bases=%1 decision_ms=%2 candidates_per_tick=%3 queries_per_tick=%4 deadline_ms=%5 attempts=%6",
			bases.Count(), m_Config.m_iDecisionMs, m_Config.m_iCandidatesPerTick, m_Config.m_iQueriesPerTick, m_Config.m_iDeadlineMs, m_Config.m_iAttempts));
	}

	protected void Decide(AICF_ConstructionBaseState state, int now)
	{
		state.m_iDueAt = now + m_Config.m_iDecisionMs;
		SCR_CampaignMilitaryBaseComponent base = state.m_Base;
		if (!base || !base.GetOwner() || base.GetOwner().GetID() != state.m_BaseId || !base.IsInitialized())
			return;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(base.GetFaction());
		AICF_AICommander commander = Commander(faction);
		SCR_CampaignBuildingProviderComponent provider = base.GetMasterProvider();
		if (!commander || !provider || !provider.GetOwner() || m_Builders.HasUnfinishedWork(base))
			return;
		if (state.m_RetryFaction != faction || state.m_RetryProviderId != provider.GetOwner().GetID())
		{
			state.m_RetryFaction = faction;
			state.m_RetryProviderId = provider.GetOwner().GetID();
			for (int resetType; resetType < AICF_EConstructionType.COUNT; resetType++)
			{
				state.m_aSearchFailures[resetType] = 0;
				state.m_aSearchRetryAt[resetType] = 0;
				state.m_aSearchOffsets[resetType] = 0;
			}
		}
		AICF_ConstructionOrder order = new AICF_ConstructionOrder();
		order.m_sToken = "construction-" + (++m_iNextToken).ToString();
		order.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
		order.m_Faction = faction;
		order.m_Base = base;
		order.m_BaseId = state.m_BaseId;
		order.m_Provider = provider;
		order.m_ProviderId = provider.GetOwner().GetID();
		order.m_vProviderPosition = provider.GetOwner().GetOrigin();
		order.m_iStartedAt = now;
		order.m_iDeadline = now + m_Config.m_iDeadlineMs;
		order.m_iRevision = ++state.m_iRevision;
		if (!order.IdentityValid() || !ScanInventory(order))
		{
			// Общую квоту могли израсходовать другие pending orders. Решение
			// ещё не принято: не превращаем ожидание queries в минутный cooldown.
			if (order.m_sReason == "QUERY_BUDGET")
				state.m_iDueAt = now;
			return;
		}
		array<bool> coverage = {};
		for (int type; type < AICF_EConstructionType.COUNT; type++)
			coverage.Insert(Covered(order, type) || now < state.m_aSearchRetryAt[type]);
		for (int attempt; attempt < AICF_EConstructionType.COUNT; attempt++)
		{
			int type = commander.SelectConstructionType(coverage, state.m_iNextType);
			if (type < 0)
				return;
			coverage[type] = true;
			order.m_eType = type;
			order.m_Metadata = Metadata(order);
			if (!order.m_Metadata || !order.m_Metadata.m_bValid)
			{
				order.m_sReason = "UNSUPPORTED_PREFAB_METADATA";
				// Metadata неизменна в пределах запущенной кампании и уже кешируется.
				// Отказ публикуется один раз на prefab, без новых фиктивных заказов.
				if (order.m_Metadata && !order.m_Metadata.m_bInvalidReported)
				{
					order.m_Metadata.m_bInvalidReported = true;
					order.Log("CONSTRUCTION_DEFERRED", "metadata_reason=" + order.m_Metadata.m_sInvalidReason);
				}
				continue;
			}
			if (!m_Economy.QuoteConstruction(order, m_Config))
			{
				order.Log("CONSTRUCTION_DEFERRED");
				continue;
			}
			state.m_Order = order;
			order.m_iSearchOffset = state.m_aSearchOffsets[type];
			order.m_iStage = -1;
			order.Log("CONSTRUCTION_DECISION");
			return;
		}
	}

	protected AICF_ConstructionMetadata Metadata(AICF_ConstructionOrder order)
	{
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab(order.m_sFaction, order.m_eType);
		if (prefab.IsEmpty())
			return null;
		foreach (AICF_ConstructionMetadata metadata : m_aMetadata)
		{
			if (metadata.m_sPrefab == prefab)
				return metadata;
		}
		AICF_ConstructionMetadata metadata = new AICF_ConstructionMetadata();
		metadata.Load(prefab, order.m_eType, m_Manager, order.m_Faction);
		m_aMetadata.Insert(metadata);
		return metadata;
	}

	protected void Search(AICF_ConstructionBaseState state, int sliceStarted)
	{
		AICF_ConstructionOrder order = state.m_Order;
		int started = System.GetTickCount();
		int previousAttempts;
		// После дешёвого отказа используем оставшуюся часть тех же четырёх
		// кандидатов и общего slice, без рекурсии и нового секундного ожидания.
		while (state.m_Order == order)
		{
			previousAttempts = order.m_iAttempts;
			int previousStage = order.m_iStage;
			int previousQueries = order.m_iQueries;
			int previousPathSteps = order.m_iPathSteps;
			SearchStep(state, sliceStarted);
			if (state.m_Order != order || order.m_bAccepted || order.m_iStage == 4 ||
				(order.m_iAttempts == previousAttempts && order.m_iStage == previousStage && order.m_iQueries == previousQueries && order.m_iPathSteps == previousPathSteps) ||
				order.m_sReason == "QUERY_BUDGET" || System.GetTickCount() - sliceStarted >= m_Config.m_iSliceMs ||
				(order.m_iStage == 0 && m_iCandidatesThisTick >= m_Config.m_iCandidatesPerTick))
				break;
		}
		int elapsed = System.GetTickCount() - started;
		order.m_iSearchCpuMs += elapsed;
		order.m_iMaxSliceMs = Math.Max(order.m_iMaxSliceMs, elapsed);
	}

	protected void SearchStep(AICF_ConstructionBaseState state, int sliceStarted)
	{
		AICF_ConstructionOrder order = state.m_Order;
		if (order.m_iResumeAt > 0)
		{
			order.m_iResumeAt = 0;
			order.Log("CONSTRUCTION_SEARCH_RESUMED", "window=" + order.m_iSearchWindows);
		}
		if (order.m_iStage == -1)
		{
			if (m_bMetadataBatchThisTick)
				return;
			m_bMetadataBatchThisTick = true;
			int geometry = order.m_Metadata.StepGeometry(m_Config.m_iMetadataEntriesPerTick, Math.Max(1, m_Config.m_iSliceMs - (System.GetTickCount() - sliceStarted)));
			// EntriesPerTick ограничивает один chunk; дешёвые chunks используют
			// оставшееся время того же slice, вместо секунд простоя между ними.
			while (geometry == 0 && System.GetTickCount() - sliceStarted < m_Config.m_iSliceMs)
				geometry = order.m_Metadata.StepGeometry(m_Config.m_iMetadataEntriesPerTick, Math.Max(1, m_Config.m_iSliceMs - (System.GetTickCount() - sliceStarted)));
			if (geometry < 0)
				Cancel(state, "UNSUPPORTED_GEOMETRY");
			else if (geometry > 0)
			{
				if (!m_Search.AreaCanFit(order))
				{
					Cancel(state, "SEARCH_AREA_EXHAUSTED");
					return;
				}
				// Холодная metadata больших compositions может занять почти весь
				// deadline. У поиска свой конечный срок после готовности geometry;
				// подготовка уже ограничена исходным deadline из Decide().
				order.m_iDeadline = System.GetTickCount() + m_Config.m_iDeadlineMs;
				order.m_iStage = 0;
				order.Log("CONSTRUCTION_SEARCH_READY", "search_deadline_ms=" + order.m_iDeadline);
			}
			return;
		}
		int candidates;
		// Старые pending states не занимают очередь бесконечно. Время жизни
		// сохраняется при checkpoint/restore; бюджет кандидата также не обнуляется.
		for (int expired = order.m_aPendingCandidates.Count() - 1; expired >= 0; expired--)
		{
			int lifetime = 60000;
			if (order.m_aPendingCandidates[expired].m_bPathPending)
				lifetime = 15000;
			if (System.GetTickCount() - order.m_aPendingCandidates[expired].m_iStartedAt < lifetime)
				continue;
			order.Log("CONSTRUCTION_PENDING_EXPIRED", "candidate_index=" + order.m_aPendingCandidates[expired].m_iIndex + " status=COMPUTE_LIMIT");
			order.m_aPendingCandidates.Remove(expired);
		}
		if (order.m_iStage == 0 && !order.m_aPendingCandidates.IsEmpty())
		{
			// Возобновляем наиболее продвинувшийся подтверждённый путь.
			// FIFO tie-break; дальние тупики не делят квоту поровну с почти
			// достигнутым рабочим endpoint. Проверки пути остаются прежними.
			int best;
			for (int i = 1; i < order.m_aPendingCandidates.Count(); i++)
			{
				if (order.m_aPendingCandidates[i].m_fRemainingDistance < order.m_aPendingCandidates[best].m_fRemainingDistance)
					best = i;
			}
			float nearDistance = vector.DistanceXZ(order.m_Metadata.m_vMin, order.m_Metadata.m_vMax);
			if (order.m_aPendingCandidates[best].m_fRemainingDistance <= nearDistance ||
				order.m_aPendingCandidates.Count() >= 32 || order.m_iAttempts >= m_Config.m_iAttempts || order.m_iAttempts >= order.m_iNextPathAttempt)
			{
				order.m_aPendingCandidates[best].Restore(order);
				order.m_aPendingCandidates.Remove(best);
				order.m_iPathResumes++;
				order.m_iNextPathAttempt = order.m_iAttempts + 4;
				order.m_iPathSliceAt = order.m_iQueries;
			}
		}
		while (order.m_iStage == 0 && m_iCandidatesThisTick < m_Config.m_iCandidatesPerTick && candidates++ < m_Config.m_iCandidatesPerTick && System.GetTickCount() - sliceStarted < m_Config.m_iSliceMs)
		{
			if (order.m_iAttempts >= m_Config.m_iAttempts)
			{
				Cancel(state, "SEARCH_BUDGET_EXHAUSTED");
				return;
			}
			int previousAttempts = order.m_iAttempts;
			bool found = m_Search.BeginCandidate(order);
			order.m_iPathSliceAt = order.m_iQueries;
			if (order.m_sReason == "QUERY_BUDGET")
			{
				order.m_iAttempts = previousAttempts;
				return;
			}
			m_iCandidatesThisTick++;
			if (found && !AccessClear(order))
			{
				order.m_iStage = 0;
				found = false;
			}
			if (!found)
				order.RejectCandidate();
		}
		if (order.m_iStage == 0)
			return;
		AICF_AICommander commander = Commander(order.m_Faction);
		int result = m_Search.Step(order, commander.GetConstructionPathfinding(), Math.Max(0, m_Config.m_iSliceMs - (System.GetTickCount() - sliceStarted)));
		if (result == 2)
		{
			AICF_ConstructionCandidate ready = new AICF_ConstructionCandidate();
			ready.Save(order);
			order.m_aPendingCandidates.Insert(ready);
			order.m_iStage = 0;
			order.m_sReason = "GEOMETRY_READY_PATH_QUEUED";
			return;
		}
		if (result < 0)
		{
			if (order.m_sReason == "NAVMESH_CONTEXT_CHANGED")
			{
				Cancel(state, "NAVMESH_CONTEXT_CHANGED");
				return;
			}
			order.RejectCandidate();
			order.m_iStage = 0;
			return;
		}
		if (result == 0)
		{
			// Короткий первый проход нескольких площадок: трудный путь одной
			// не задерживает прямой путь следующей. Сохраняем именно A* state,
			// поэтому повторный проход не повторяет terrain/physics/nav queries.
			if (order.m_iStage == 3 && order.m_Path && order.m_iQueries - order.m_iPathSliceAt >= 128)
			{
				if (order.m_aPendingCandidates.Count() >= 32)
				{
					order.Log("CONSTRUCTION_PENDING_EXPIRED", "candidate_index=" + order.m_aPendingCandidates[0].m_iIndex + " status=QUEUE_LIMIT");
					order.m_aPendingCandidates.Remove(0);
				}
				AICF_ConstructionCandidate checkpoint = new AICF_ConstructionCandidate();
				checkpoint.Save(order);
				order.m_aPendingCandidates.Insert(checkpoint);
				order.m_Path = null;
				order.m_iStage = 0;
			}
			return;
		}
		order.m_iStage = 4;
		if (!order.m_bSiteReserved)
		{
			order.m_bSiteReserved = true;
			state.m_aSearchOffsets[order.m_eType] = order.m_iSearchOffset + order.m_iAttempts;
			order.LogSearch();
			order.Log("CONSTRUCTION_SITE_SELECTED");
		}
		// Только здесь возможен один layout за общий tick; ни один wait/callback
		// не находится внутри commit. Повторный token не проходит adapter gate.
		if (!order.IdentityValid() || !ScanInventory(order) || Covered(order, order.m_eType) ||
			m_Builders.HasUnfinishedWork(order.m_Base))
		{
			if (order.m_sReason == "QUERY_BUDGET")
				return;
			if (!order.IdentityValid())
				order.m_sReason = "COMMIT_IDENTITY_CHANGED";
			else if (Covered(order, order.m_eType))
				order.m_sReason = "COMMIT_COVERAGE_CHANGED";
			else
				order.m_sReason = "COMMIT_UNFINISHED_WORK";
			order.RejectSelected("COMMIT_IDENTITY_INVENTORY");
			Cancel(state, "COMMIT_REVALIDATION_FAILED");
			return;
		}
		if (!AccessClear(order) || !m_Search.LiveClear(order, null))
		{
			if (order.m_sReason == "QUERY_BUDGET")
				return;
			// Занятый после поиска участок не отменяет весь заказ с минутным
			// cooldown: снимаем только его reservation и проверяем следующий.
			order.RejectSelected("COMMIT_GEOMETRY");
			order.RejectCandidate();
			order.m_bSiteReserved = false;
			order.m_iStage = 0;
			return;
		}
		if (!m_Economy.QuoteConstruction(order, m_Config))
		{
			order.RejectSelected("COMMIT_ECONOMY");
			Cancel(state, "COMMIT_REVALIDATION_FAILED");
			return;
		}
		m_bPlacementAttemptedThisTick = true;
		if (!m_Adapter.Place(order, m_Config, m_Economy, m_Manager, m_Builders))
		{
			order.RejectSelected("PLACEMENT");
			Cancel(state, "PLACEMENT_FAILED");
		}
		else
		{
			order.m_aPendingCandidates.Clear();
			order.m_Path = null;
			order.m_Navigation = null;
			order.m_aPathStarts.Clear();
		}
	}

	protected void Cancel(AICF_ConstructionBaseState state, string reason)
	{
		AICF_ConstructionOrder order = state.m_Order;
		if (!order)
			return;
		state.m_aSearchOffsets[order.m_eType] = order.m_iSearchOffset + order.m_iAttempts;
		order.LogSearch();
		AICF_ConstructionSiteSearch.ReleaseClaim(order.m_sToken);
		order.m_bSiteReserved = false;
		// После конечного числа окон cursor идёт вперёд. Повторное разрушение
		// и восстановление одного дорогого кандидата больше не образует цикл.
		order.m_aPendingCandidates.Clear();
		order.m_Path = null;
		order.m_Navigation = null;
		order.m_aPathStarts.Clear();
		if (!order.m_bAccepted)
			order.m_bCancelled = true;
		string cause = order.m_sReason;
		if (!order.m_bAccepted && (reason == "SEARCH_BUDGET_EXHAUSTED" || reason == "SEARCH_AREA_EXHAUSTED"))
		{
			int failures = Math.Min(4, state.m_aSearchFailures[order.m_eType] + 1);
			state.m_aSearchFailures[order.m_eType] = failures;
			int retryMs = m_Config.m_iCooldownMs;
			for (int backoff = 1; backoff < failures; backoff++)
				retryMs *= 2;
			retryMs = Math.Max(m_Config.m_iCooldownMs, Math.Min(480000, retryMs));
			state.m_aSearchRetryAt[order.m_eType] = System.GetTickCount() + retryMs;
			order.Log("CONSTRUCTION_SEARCH_BACKOFF", string.Format("failures=%1 retry_ms=%2 terminal_reason=%3 cause=%4", failures, retryMs, reason, cause));
		}
		order.m_sReason = reason;
		order.Log("CONSTRUCTION_CANCELLED", "reservation_released=1 cause=" + cause);
		state.m_iNextType = (order.m_eType + 1) % AICF_EConstructionType.COUNT;
		// Временный отказ не вытесняет обязательные казармы другим типом.
		// После трёх ограниченных попыток разрешён один проход остальных типов.
		if (order.m_eType == AICF_EConstructionType.SMALL_BARRACKS && !order.m_bAccepted)
		{
			state.m_iSmallFailures++;
			if (state.m_iSmallFailures < 3)
				state.m_iNextType = AICF_EConstructionType.SMALL_BARRACKS;
			else
				state.m_iSmallFailures = 0;
		}
		state.m_iDueAt = Math.Max(state.m_iDueAt, System.GetTickCount() + m_Config.m_iCooldownMs);
		state.m_Order = null;
	}

	protected bool ScanInventory(AICF_ConstructionOrder order)
	{
		order.m_iQueryPhase = 6;
		if (!AICF_ConstructionSiteSearch.TakeQueries(order, 1))
			return false;
		m_aInventory.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(order.m_vProviderPosition, order.m_Provider.GetBuildingRadius(), InventoryEntity, null, EQueryEntitiesFlags.ALL);
		return true;
	}

	protected bool InventoryEntity(IEntity entity)
	{
		if (entity && (entity.FindComponent(SCR_CampaignBuildingCompositionComponent) || entity.FindComponent(SCR_ServicePointComponent)))
			m_aInventory.Insert(entity);
		return true;
	}

	protected bool Covered(AICF_ConstructionOrder order, AICF_EConstructionType type)
	{
		foreach (IEntity entity : m_aInventory)
		{
			if (!entity)
				continue;
			SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(entity.FindComponent(SCR_EditableEntityComponent));
			SCR_ServicePointComponent service = SCR_ServicePointComponent.Cast(entity.FindComponent(SCR_ServicePointComponent));
			if (service)
				editable = SCR_EditableEntityComponent.Cast(entity.GetRootParent().FindComponent(SCR_EditableEntityComponent));
			SCR_EditableEntityUIInfo info;
			if (editable)
				info = SCR_EditableEntityUIInfo.Cast(editable.GetInfo());
			// Map service может существовать без prefab/ editable metadata. Его реальный
			// type также закрывает потребность; неизвестный barracks tier — оба.
			bool actualService = service && service.GetType() == AICF_ConstructionMetadata.ServiceType(type);
			bool labeledComposition = info && info.HasEntityLabel(EEditableEntityLabel.TRAIT_SERVICE) &&
				info.HasEntityLabel(AICF_ConstructionMetadata.ServiceLabel(type));
			if (info && info.HasEntityLabel(EEditableEntityLabel.TRAIT_MORTAR) && !labeledComposition)
				continue;
			if (!actualService && !labeledComposition)
				continue;
			if (type == AICF_EConstructionType.SMALL_BARRACKS || type == AICF_EConstructionType.LARGE_BARRACKS)
			{
				// Не подменяем два типа одним SERVICE_LIVING_AREA. Unknown tier
				// закрывает оба fail-closed; large service покрывает также small.
				bool small = info && info.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_SMALL);
				if (type == AICF_EConstructionType.LARGE_BARRACKS && small)
					continue;
			}
			return true;
		}
		return false;
	}

	protected bool AccessClear(AICF_ConstructionOrder order)
	{
		array<SCR_ServicePointComponent> services = {};
		order.m_Base.GetServices(services);
		foreach (SCR_ServicePointComponent service : services)
		{
			if (!service || !service.GetOwner())
				continue;
			if (AccessIntersects(order, order.m_vProviderPosition, service.GetOwner().GetOrigin()))
			{
				order.m_sReason = "BASE_ACCESS_CORRIDOR";
				return false;
			}
		}
		if (order.m_Base.GetSpawnPoint())
		{
			array<vector> spawns = {};
			order.m_Base.GetSpawnPoint().AICF_ConstructionPositions(spawns);
			foreach (vector spawn : spawns)
			{
				if (AccessIntersects(order, order.m_vProviderPosition, spawn))
				{
					order.m_sReason = "BASE_ACCESS_CORRIDOR";
					return false;
				}
			}
		}
		return true;
	}

	protected bool AccessIntersects(AICF_ConstructionOrder order, vector start, vector end)
	{
		// Та же полоса 3 м плюс footprint margin; повёрнутый объём вместо
		// пустых углов world AABB. Никаких случайных spawn queries в guard.
		vector from = start - order.m_aTransform[3];
		vector to = end - order.m_aTransform[3];
		from = Vector(vector.Dot(from, order.m_aTransform[0]), 0, vector.Dot(from, order.m_aTransform[2]));
		to = Vector(vector.Dot(to, order.m_aTransform[0]), 0, vector.Dot(to, order.m_aTransform[2]));
		float distance = m_Config.m_fMargin + 3;
		vector margin = Vector(distance, 0, distance);
		return SegmentIntersects(from, to, order.m_Metadata.m_vMin - margin, order.m_Metadata.m_vMax + margin);
	}

	static bool SegmentIntersects(vector start, vector end, vector mins, vector maxs)
	{
		float low = 0;
		float high = 1;
		for (int axis = 0; axis < 3; axis += 2)
		{
			float direction = end[axis] - start[axis];
			if (Math.AbsFloat(direction) < 0.001)
			{
				if (start[axis] < mins[axis] || start[axis] > maxs[axis])
					return false;
				continue;
			}
			float a = (mins[axis] - start[axis]) / direction;
			float b = (maxs[axis] - start[axis]) / direction;
			low = Math.Max(low, Math.Min(a, b));
			high = Math.Min(high, Math.Max(a, b));
			if (low > high)
				return false;
		}
		return true;
	}

	static bool SpatialClear(AICF_ConstructionOrder except, vector mins, vector maxs)
	{
		if (!AICF_VehicleSpawner.ConstructionAreaClear(mins, maxs))
			return false;
		if (!s_Instance)
			return true;
		foreach (AICF_ConstructionBaseState state : s_Instance.m_aBases)
		{
			AICF_ConstructionOrder order = state.m_Order;
			if (!order || !order.m_bSiteReserved || (except && order.m_sToken == except.m_sToken))
				continue;
			if (mins[0] <= order.m_vMax[0] && maxs[0] >= order.m_vMin[0] && mins[2] <= order.m_vMax[2] && maxs[2] >= order.m_vMin[2])
				return false;
			foreach (AICF_ConstructionVolume exitVolume : order.m_aExits)
			{
				vector exitMin, exitMax;
				AICF_ConstructionSiteSearch.TransformBounds(exitVolume.m_vMin, exitVolume.m_vMax, order.m_aTransform, 0, exitMin, exitMax);
				if (mins[0] <= exitMax[0] && maxs[0] >= exitMin[0] && mins[2] <= exitMax[2] && maxs[2] >= exitMin[2])
					return false;
			}
		}
		return true;
	}

	static bool VehicleAreaClear(vector position, float radius)
	{
		if (!s_Instance)
			return true;
		foreach (AICF_ConstructionBaseState state : s_Instance.m_aBases)
		{
			AICF_ConstructionOrder order = state.m_Order;
			if (!order || !order.m_bSiteReserved)
				continue;
			vector nearest = Vector(Math.Clamp(position[0], order.m_vMin[0], order.m_vMax[0]), position[1], Math.Clamp(position[2], order.m_vMin[2], order.m_vMax[2]));
			if (vector.DistanceSqXZ(position, nearest) <= radius * radius)
				return false;
			foreach (AICF_ConstructionVolume exitVolume : order.m_aExits)
			{
				vector exitMin, exitMax;
				AICF_ConstructionSiteSearch.TransformBounds(exitVolume.m_vMin, exitVolume.m_vMax, order.m_aTransform, 0, exitMin, exitMax);
				nearest = Vector(Math.Clamp(position[0], exitMin[0], exitMax[0]), position[1], Math.Clamp(position[2], exitMin[2], exitMax[2]));
				if (vector.DistanceSqXZ(position, nearest) <= radius * radius)
					return false;
			}
		}
		return true;
	}
}
