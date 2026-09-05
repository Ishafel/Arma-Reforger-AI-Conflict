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
	protected bool m_bStopped;

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
					order.Log("CONSTRUCTION_COMPLETED", "service_online=1");
					state.m_Order = null;
					state.m_iNextType = 0;
				}
				continue;
			}
			if (!order.IdentityValid() || !Commander(order.m_Faction) || m_Builders.HasUnfinishedWork(order.m_Base) ||
				order.m_Base.AreEnemiesPresent() || order.m_Base.IsBeingCaptured() || order.m_Base.GetCaptureState() != SCR_EBaseCaptureState.NONE)
				Cancel(state, "LIFECYCLE_INVALIDATED");
			else if (now >= order.m_iDeadline)
				Cancel(state, "NO_SAFE_SITE");
		}
		if (m_aBases.IsEmpty())
			return;
		for (int visited; visited < m_aBases.Count(); visited++)
		{
			AICF_ConstructionBaseState selected = m_aBases[m_iCursor++ % m_aBases.Count()];
			if (!selected.m_Order && now >= selected.m_iDueAt)
				Decide(selected, now);
			if (selected.m_Order && !selected.m_Order.m_bAccepted)
			{
				Search(selected);
				break;
			}
		}
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
			return;
		array<bool> coverage = {};
		for (int type; type < AICF_EConstructionType.COUNT; type++)
			coverage.Insert(Covered(order, type));
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
				order.Log("CONSTRUCTION_DEFERRED");
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

	protected void Search(AICF_ConstructionBaseState state)
	{
		AICF_ConstructionOrder order = state.m_Order;
		if (order.m_iStage == -1)
		{
			int geometry = order.m_Metadata.StepGeometry(m_Config.m_iMetadataEntriesPerTick, m_Config.m_iSliceMs);
			if (geometry < 0)
				Cancel(state, "UNSUPPORTED_GEOMETRY");
			else if (geometry > 0)
				order.m_iStage = 0;
			return;
		}
		int candidates;
		int sliceStarted = System.GetTickCount();
		while (order.m_iStage == 0 && candidates++ < m_Config.m_iCandidatesPerTick && System.GetTickCount() - sliceStarted < m_Config.m_iSliceMs)
		{
			if (order.m_iAttempts >= m_Config.m_iAttempts)
			{
				Cancel(state, "NO_SAFE_SITE");
				return;
			}
			int previousAttempts = order.m_iAttempts;
			bool found = m_Search.BeginCandidate(order);
			if (order.m_sReason == "QUERY_BUDGET")
			{
				order.m_iAttempts = previousAttempts;
				return;
			}
			if (!found)
				order.RejectCandidate();
		}
		if (order.m_iStage == 0)
			return;
		AICF_AICommander commander = Commander(order.m_Faction);
		int result = m_Search.Step(order, commander.GetConstructionPathfinding());
		if (result < 0)
		{
			order.RejectCandidate();
			order.m_iStage = 0;
			return;
		}
		if (result == 0)
			return;
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
			m_Builders.HasUnfinishedWork(order.m_Base) || !AccessClear(order) ||
			!m_Search.LiveClear(order, null) || !m_Economy.QuoteConstruction(order, m_Config))
		{
			if (order.m_sReason == "QUERY_BUDGET")
				return;
			Cancel(state, "COMMIT_REVALIDATION_FAILED");
			return;
		}
		if (!m_Adapter.Place(order, m_Config, m_Economy, m_Manager, m_Builders))
			Cancel(state, "PLACEMENT_FAILED");
	}

	protected void Cancel(AICF_ConstructionBaseState state, string reason)
	{
		AICF_ConstructionOrder order = state.m_Order;
		if (!order)
			return;
		state.m_aSearchOffsets[order.m_eType] = order.m_iSearchOffset + order.m_iAttempts;
		order.LogSearch();
		order.m_bSiteReserved = false;
		// Deadline мог застать пригодный footprint на terrain/exit/navmesh стадии.
		// Повторить его с полной свежей проверкой, а не навсегда пропустить точку.
		if (reason == "NO_SAFE_SITE" && order.m_iStage > 0 && !order.m_bAccepted && order.m_iAttempts > 0)
			state.m_aSearchOffsets[order.m_eType] = state.m_aSearchOffsets[order.m_eType] - 1;
		if (!order.m_bAccepted)
			order.m_bCancelled = true;
		string cause = order.m_sReason;
		order.m_sReason = reason;
		order.Log("CONSTRUCTION_CANCELLED", "reservation_released=1 cause=" + cause);
		state.m_iNextType = (order.m_eType + 1) % AICF_EConstructionType.COUNT;
		state.m_iDueAt = Math.Max(state.m_iDueAt, System.GetTickCount() + m_Config.m_iCooldownMs);
		state.m_Order = null;
	}

	protected bool ScanInventory(AICF_ConstructionOrder order)
	{
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
			if (SegmentIntersects(order.m_vProviderPosition, service.GetOwner().GetOrigin(), order.m_vMin - "3 0 3", order.m_vMax + "3 0 3"))
			{
				order.m_sReason = "BASE_ACCESS_CORRIDOR";
				return false;
			}
		}
		vector spawn, rotation;
		if (order.m_Base.GetSpawnPoint())
		{
			order.m_Base.GetSpawnPoint().GetPositionAndRotation(spawn, rotation);
			if (SegmentIntersects(order.m_vProviderPosition, spawn, order.m_vMin - "3 0 3", order.m_vMax + "3 0 3"))
			{
				order.m_sReason = "BASE_ACCESS_CORRIDOR";
				return false;
			}
		}
		return true;
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
