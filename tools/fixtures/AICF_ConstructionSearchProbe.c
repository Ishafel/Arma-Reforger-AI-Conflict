// Только изолированные source copies. Диагностика и fault injection не входят
// в production addon. Fixtures используют настоящие physics/provider guards.
modded class AICF_ConstructionPath
{
	protected static ref array<string> s_aAICFTraced = {};
	override int Step(AICF_ConstructionOrder order, AIPathfindingComponent pathfinding, int sliceMs)
	{
		int result = super.Step(order, pathfinding, sliceMs);
		string enabled;
		if (result < 0 && order.m_sFaction == "US" && !s_aAICFTraced.Contains(order.m_sToken) &&
			System.GetCLIParam("aicfConstructionSearchTrace", enabled) && enabled == "1")
		{
			s_aAICFTraced.Insert(order.m_sToken);
			order.Log("CONSTRUCTION_PATH_PROBE", "test_only=1 " + Describe() + " cache_hits=" + order.m_iNavCacheHits);
			foreach (AICF_ConstructionPathNode node : m_aNodes)
				Print(string.Format("[AICF][CONSTRUCTION_PATH_NODE_PROBE] test_only=1 position=%1 root=%2 closed=%3 cost=%4 parent=%5", node.m_vPosition, node.m_vRoot, node.m_bClosed, node.m_fCost, node.m_iParent));
			foreach (vector goal : m_aGoals)
				Print("[AICF][CONSTRUCTION_PATH_GOAL_PROBE] test_only=1 position=" + goal);
		}
		return result;
	}
}

modded class AICF_ConstructionSiteSearch
{
	protected bool m_bAICFDynamicInjected;
	protected IEntity m_AICFBlocker;
	protected string m_sAICFBlockerToken;

	override bool LiveClear(AICF_ConstructionOrder order, IEntity excludedRoot)
	{
		string fault;
		if (!m_bAICFDynamicInjected && order.m_iStage == 4 && !excludedRoot &&
			System.GetCLIParam("aicfConstructionSearchFault", fault) && fault == "dynamic")
		{
			m_bAICFDynamicInjected = true;
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.MatrixCopy(order.m_aTransform, params.Transform);
			AICF_ConstructionVolume volume = order.m_Metadata.m_aCollisionVolumes[0];
			vector center = (volume.m_vMin + volume.m_vMax) * 0.5;
			params.Transform[3] = order.m_aTransform[3] + order.m_aTransform[0] * center[0] + order.m_aTransform[1] * center[1] + order.m_aTransform[2] * center[2];
			m_AICFBlocker = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
			if (m_AICFBlocker)
			{
				m_sAICFBlockerToken = order.m_sToken;
				m_AICFBlocker.SetFlags(EntityFlags.TRACEABLE, false);
				ref PhysicsGeomDef geoms[] = {PhysicsGeomDef("construction-probe", PhysicsGeom.CreateBox("2 2 2"), "{D745FD8FC67DB26A}Common/Materials/Game/stone.gamemat", 0xffffffff)};
				Physics.CreateStaticEx(m_AICFBlocker, geoms);
				order.Log("CONSTRUCTION_FAULT_PROBE", "test_only=1 fault=dynamic injected=1");
			}
		}
		bool result = super.LiveClear(order, excludedRoot);
		if (m_AICFBlocker && order.m_sToken == m_sAICFBlockerToken && order.m_iStage == 4 && !excludedRoot && order.m_sReason != "QUERY_BUDGET")
		{
			bool matched = order.m_sObstacle == Describe(m_AICFBlocker);
			order.Log("CONSTRUCTION_FAULT_PROBE", string.Format("test_only=1 fault=dynamic clear=%1 expected_clear=0 paid=%2 blocker_match=%3", result, order.m_bPaid, matched));
			delete m_AICFBlocker;
			m_AICFBlocker = null;
		}
		return result;
	}

	void ~AICF_ConstructionSiteSearch()
	{
		if (m_AICFBlocker)
			delete m_AICFBlocker;
	}
}

modded class AICF_ConstructionPlanner
{
	protected bool m_bAICFPendingFault;
	protected ref AICF_ConstructionOrder m_AICFInvalidated;
	protected int m_iAICFInvalidatedAt;
	protected bool m_bAICFArmoriesPrepared;

	override protected void Decide(AICF_ConstructionBaseState state, int now)
	{
		string fault;
		if (System.GetCLIParam("aicfConstructionSearchFault", fault) && fault == "owner")
		{
			SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(state.m_Base.GetFaction());
			if (faction && faction.GetMainBase() == state.m_Base)
			{
				state.m_iDueAt = now + 1000;
				return;
			}
		}
		super.Decide(state, now);
	}

	protected void AICF_PrepareMissingArmories()
	{
		string enabled;
		bool relocate = System.GetCLIParam("aicfConstructionProbeMoveArmories", enabled) && enabled == "1";
		bool remove = System.GetCLIParam("aicfConstructionProbeRemoveArmories", enabled) && enabled == "1";
		if (m_bAICFArmoriesPrepared || (!relocate && !remove))
			return;
		m_bAICFArmoriesPrepared = true;
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		m_Campaign.GetBaseManager().GetBases(bases);
		array<IEntity> entities = {};
		float clearance = 100;
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			if (base.GetMasterProvider())
				clearance = Math.Max(clearance, base.GetMasterProvider().GetBuildingRadius());
			array<SCR_ServicePointComponent> services = {};
			base.GetServices(services);
			foreach (SCR_ServicePointComponent service : services)
			{
				if (!service || service.GetType() != AICF_ConstructionMetadata.ServiceType(AICF_EConstructionType.ARMORY))
					continue;
				IEntity entity = service.GetOwner();
				if (!entity || entity == base.GetOwner() || entity.FindComponent(SCR_CampaignBuildingProviderComponent))
					continue;
				IEntity root = entity.GetRootParent();
				if (root && root != base.GetOwner() && root.FindComponent(SCR_CampaignBuildingCompositionComponent) && !root.FindComponent(SCR_CampaignBuildingProviderComponent) &&
					(remove || !AICF_ProtectedEntity(root, bases)))
					entity = root;
				if (!entities.Contains(entity))
					entities.Insert(entity);
			}
		}
		foreach (IEntity entity : entities)
		{
			if (remove)
			{
				// Отдельный negative test stale provider owner; не positive setup.
				Print("[AICF][CONSTRUCTION_ARMORY_PREPARE] test_only=1 remove_existing_service=" + AICF_ConstructionOrder.EntityKey(entity.GetID()));
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				continue;
			}
			if (AICF_ProtectedEntity(entity, bases))
				continue;
			// Entity/resource/faction links остаются живыми. Coverage проверяет
			// реальное отсутствие сервиса в sphere provider; обхода Covered нет.
			entity.SetOrigin(entity.GetOrigin() + Vector(0, clearance * 4, 0));
			Print("[AICF][CONSTRUCTION_ARMORY_PREPARE] test_only=1 moved_existing_service=" + AICF_ConstructionOrder.EntityKey(entity.GetID()));
		}
	}

	protected bool AICF_ProtectedEntity(IEntity entity, array<SCR_CampaignMilitaryBaseComponent> bases)
	{
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			if (AICF_ContainsEntity(entity, base.GetOwner()) ||
				(base.GetMasterProvider() && AICF_ContainsEntity(entity, base.GetMasterProvider().GetOwner())))
				return true;
		}
		return false;
	}

	protected bool AICF_ContainsEntity(IEntity ancestor, IEntity entity)
	{
		while (entity)
		{
			if (entity == ancestor)
				return true;
			entity = entity.GetParent();
		}
		return false;
	}

	override void Update()
	{
		AICF_PrepareMissingArmories();
		string fault;
		if (!m_bAICFPendingFault && System.GetCLIParam("aicfConstructionSearchFault", fault) && fault != "dynamic")
		{
			foreach (AICF_ConstructionBaseState state : m_aBases)
			{
				AICF_ConstructionOrder order = state.m_Order;
				if (!order || order.m_bAccepted || (order.m_iStage != 3 && order.m_aPendingCandidates.IsEmpty()))
					continue;
				// Смена обычной базы не должна закончить сам тест потерей HQ.
				if (fault == "owner" && order.m_Faction && order.m_Faction.GetMainBase() == order.m_Base)
					continue;
				m_bAICFPendingFault = true;
				m_AICFInvalidated = order;
				m_iAICFInvalidatedAt = System.GetTickCount();
				order.Log("CONSTRUCTION_FAULT_PROBE", "test_only=1 fault=" + fault + " injected=1");
				if (fault == "owner")
				{
					SCR_CampaignFaction changedOwner = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
					if (order.m_sFaction == "US")
						changedOwner = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
					order.m_Base.SetFaction(changedOwner);
				}
				else if (fault == "provider")
					order.m_Provider.GetOwner().SetOrigin(order.m_vProviderPosition + "1 0 0");
				else if (fault == "cancel")
					Cancel(state, "TEST_CANCEL_PENDING");
				else if (fault == "context")
				{
					// Моделируем auto-null удалённого managed component, не удаляя
					// gameplay group. Следующая production проверка обязана отменить order.
					order.m_bPathContextSet = true;
					order.m_PathContext = null;
				}
				break;
			}
		}
		super.Update();
		if (m_AICFInvalidated && System.GetTickCount() - m_iAICFInvalidatedAt >= 10000)
		{
			m_AICFInvalidated.Log("CONSTRUCTION_FAULT_PROBE", string.Format("test_only=1 cancelled=%1 accepted=%2 paid=%3 reserved=%4 checkpoints=%5 path_present=%6", m_AICFInvalidated.m_bCancelled, m_AICFInvalidated.m_bAccepted, m_AICFInvalidated.m_bPaid, m_AICFInvalidated.m_bSiteReserved, m_AICFInvalidated.m_aPendingCandidates.Count(), m_AICFInvalidated.m_Path != null));
			m_AICFInvalidated = null;
		}
	}
}
