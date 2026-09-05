// Серверная служба достройки уже оплаченных stock layouts. Единственный slot
// на базу обслуживает всю очередь, включая несколько building providers.
class AICF_BaseBuilderService
{
	protected static const int IDLE_DELAY_MS = 30000;
	protected static const int RETRY_DELAY_MS = 60000;
	protected static const int SPAWN_TIMEOUT_MS = 30000;
	protected static const int MOVE_TIMEOUT_MS = 120000;
	protected static const int WORK_INTERVAL_MS = 3000;
	protected static const float ARRIVAL_METERS = 1.0;
	protected static const float HOME_METERS = 8.0;
	protected static AICF_BaseBuilderService s_Instance;
	protected SCR_GameModeCampaign m_Campaign;
	protected SCR_CampaignBuildingManagerComponent m_BuildingManager;
	protected AICF_OrderPlanner m_Planner;
	protected ref AICF_BaseBuilderSpawner m_Spawner = new AICF_BaseBuilderSpawner();
	protected ref AICF_ManagedAILODPolicy m_LOD = new AICF_ManagedAILODPolicy();
	protected ref array<ref AICF_BaseBuilder> m_aBuilders = {};
	// Дополняет stock registry для layouts в provider radius за границей base radius.
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aPlaced = {};
	protected bool m_bStopped;
	protected int m_iCombatBudget;
	protected int m_iNextSlotId = AICF_Stage1Config.GROUP_SLOTS_PER_FACTION;

	void Start(SCR_GameModeCampaign campaign, AICF_OrderPlanner planner, int combatBudget)
	{
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster() || !planner)
			return;
		m_Campaign = campaign;
		s_Instance = this;
		m_Planner = planner;
		m_iCombatBudget = combatBudget;
		m_BuildingManager = SCR_CampaignBuildingManagerComponent.Cast(campaign.FindComponent(SCR_CampaignBuildingManagerComponent));
		if (m_BuildingManager)
			m_BuildingManager.GetOnEntitySpawnedByProvider().Insert(OnPlaced);
		AICF_Stage1Diagnostics.Info("BUILDERS_STARTED", "per_base_limit=1 idle_delay_ms=30000 work_interval_ms=3000");
	}

	void Stop()
	{
		if (m_bStopped)
			return;
		m_bStopped = true;
		if (s_Instance == this)
			s_Instance = null;
		if (m_BuildingManager)
			m_BuildingManager.GetOnEntitySpawnedByProvider().Remove(OnPlaced);
		foreach (AICF_BaseBuilder builder : m_aBuilders)
			Retire(builder, "STOP");
		m_aBuilders.Clear();
		m_aPlaced.Clear();
		m_Campaign = null;
	}

	protected void OnPlaced(int prefabID, SCR_EditableEntityComponent editableEntity, int playerId, SCR_CampaignBuildingProviderComponent provider)
	{
		if (m_bStopped || !Replication.IsServer() || !editableEntity || !provider)
			return;
		IEntity entity = editableEntity.GetOwner();
		if (!entity)
			return;
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (composition && !m_aPlaced.Contains(composition))
			m_aPlaced.Insert(composition);
	}

	void Update()
	{
		if (m_bStopped || !Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster() || !m_BuildingManager)
			return;
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		m_Campaign.GetBaseManager().GetBases(bases);
		// Отдельная ограниченная квота: не отнимает места у армейского roster.
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld && aiWorld.GetLimitOfActiveAIs() < m_iCombatBudget + bases.Count())
			aiWorld.SetLimitOfActiveAIs(m_iCombatBudget + bases.Count());
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			if (!base || !base.IsInitialized())
				continue;
			AICF_BaseBuilder builder = FindBuilder(base);
			if (!builder)
			{
				builder = new AICF_BaseBuilder();
				builder.m_Base = base;
				builder.m_iSlotId = m_iNextSlotId++;
				m_aBuilders.Insert(builder);
			}
			UpdateBuilder(builder);
		}
		for (int i = m_aBuilders.Count() - 1; i >= 0; i--)
		{
			if (!m_aBuilders[i].m_Base || !bases.Contains(m_aBuilders[i].m_Base))
			{
				if (Retire(m_aBuilders[i], "BASE_REMOVED"))
					m_aBuilders.Remove(i);
			}
		}
		for (int p = m_aPlaced.Count() - 1; p >= 0; p--)
		{
			if (!IsUnfinished(m_aPlaced[p]))
				m_aPlaced.Remove(p);
		}
	}

	protected AICF_BaseBuilder FindBuilder(SCR_CampaignMilitaryBaseComponent base)
	{
		foreach (AICF_BaseBuilder builder : m_aBuilders)
		{
			if (builder.m_Base == base)
				return builder;
		}
		return null;
	}

	protected bool IsUnfinished(SCR_CampaignBuildingCompositionComponent composition)
	{
		if (!composition || !composition.GetOwner() || composition.IsCompositionSpawned())
			return false;
		SCR_CampaignBuildingLayoutComponent layout = composition.GetCompositionLayout();
		return layout && layout.GetOwner() && layout.GetPrefabId() >= 0 &&
			layout.GetToBuildValue() > 0 && layout.GetCurrentBuildValue() < layout.GetToBuildValue();
	}

	protected bool IsTargetValid(AICF_BaseBuilder builder, SCR_CampaignBuildingCompositionComponent composition)
	{
		if (!IsUnfinished(composition) || !builder.m_Base || builder.m_Base.GetFaction() != builder.m_Faction)
			return false;
		IEntity providerEntity = composition.GetProviderEntity();
		if (!providerEntity)
			return false;
		SCR_CampaignBuildingProviderComponent provider = SCR_CampaignBuildingProviderComponent.Cast(providerEntity.FindComponent(SCR_CampaignBuildingProviderComponent));
		if (!provider || provider.GetCampaignMilitaryBaseComponent() != builder.m_Base)
			return false;
		Faction faction = SCR_Faction.GetEntityFaction(composition.GetOwner());
		if (faction && faction != builder.m_Faction)
			return false;
		float radius = provider.GetBuildingRadius();
		return radius > 0 && vector.DistanceSqXZ(providerEntity.GetOrigin(), composition.GetOwner().GetOrigin()) <= radius * radius;
	}

	protected SCR_CampaignBuildingCompositionComponent SelectTarget(AICF_BaseBuilder builder, vector origin)
	{
		array<SCR_CampaignBuildingCompositionComponent> candidates = {};
		m_BuildingManager.GetBuildingCompositions(builder.m_Base, candidates);
		foreach (SCR_CampaignBuildingCompositionComponent placed : m_aPlaced)
		{
			if (!candidates.Contains(placed))
				candidates.Insert(placed);
		}
		SCR_CampaignBuildingCompositionComponent selected;
		float bestDistance = float.MAX;
		int now = System.GetTickCount();
		for (int i = builder.m_aDeferredTargets.Count() - 1; i >= 0; i--)
		{
			if (!builder.m_aDeferredTargets[i] || now >= builder.m_aDeferredUntilMs[i])
			{
				builder.m_aDeferredTargets.Remove(i);
				builder.m_aDeferredUntilMs.Remove(i);
			}
		}
		foreach (SCR_CampaignBuildingCompositionComponent candidate : candidates)
		{
			if (!IsTargetValid(builder, candidate) || builder.m_aDeferredTargets.Contains(candidate))
				continue;
			float distance = vector.DistanceSqXZ(origin, candidate.GetOwner().GetOrigin());
			if (selected && (distance > bestDistance || (distance == bestDistance && candidate.GetOwner().GetID().ToString().Compare(selected.GetOwner().GetID().ToString()) >= 0)))
				continue;
			selected = candidate;
			bestDistance = distance;
		}
		return selected;
	}

	protected void UpdateBuilder(AICF_BaseBuilder builder)
	{
		int now = System.GetTickCount();
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(builder.m_Base.GetFaction());
		if (builder.m_bRetiring || (builder.m_Faction && builder.m_Faction != faction))
		{
			Retire(builder, "OWNER_CHANGED_OR_RETIRING");
			return;
		}
		if (!faction || AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()).IsEmpty())
			return;
		builder.m_Faction = faction;
		SCR_CampaignBuildingProviderComponent master = builder.m_Base.GetMasterProvider();
		if (!master || !master.GetOwner())
		{
			if (builder.m_Group || builder.m_iSpawnAtMs != 0)
				Retire(builder, "MAIN_TENT_UNAVAILABLE");
			return;
		}
		vector home = master.GetOwner().GetOrigin();
		if (builder.m_iSpawnAtMs != 0 && (!builder.m_Group || (builder.m_CharacterId && !builder.m_Character)))
		{
			Retire(builder, "WORKER_LOST");
			return;
		}
		if (!builder.m_Group)
		{
			if (now < builder.m_iRetryAtMs || !SelectTarget(builder, home))
				return;
			SCR_SpawnPoint spawnPoint = builder.m_Base.GetSpawnPoint();
			if (!spawnPoint)
				return;
			vector position, rotation;
			spawnPoint.GetPositionAndRotation(position, rotation);
			builder.m_iRetryAtMs = now + RETRY_DELAY_MS;
			builder.m_Group = m_Spawner.CreateBuilder(faction, position);
			if (!builder.m_Group)
			{
				Log(builder, "BUILDER_SPAWN_FAILED");
				return;
			}
			builder.m_GroupId = builder.m_Group.GetID();
			builder.m_iGeneration++;
			builder.m_iSpawnAtMs = now;
			// Сначала bind exact identity, только затем асинхронный запрос одного AI.
			if (!m_Spawner.BeginRosterSpawn(builder.m_Group, 1))
				Retire(builder, "ROSTER_REQUEST_FAILED");
			else
				Log(builder, "BUILDER_SPAWN_REQUESTED");
			return;
		}
		if (builder.m_Group.GetID() != builder.m_GroupId || builder.m_Group.GetFaction() != faction)
		{
			Retire(builder, "GROUP_IDENTITY_CHANGED");
			return;
		}
		if (!builder.m_Character)
		{
			int actual, wrongFaction, dead;
			if (!AICF_GroupRuntime.HasExactFactionRoster(builder.m_Group, faction.GetFactionKey(), 1, actual, wrongFaction, dead))
			{
				if (actual > 1 || wrongFaction > 0 || dead > 0 || now - builder.m_iSpawnAtMs >= SPAWN_TIMEOUT_MS)
					Retire(builder, "ROSTER_INVALID_OR_TIMEOUT");
				return;
			}
			builder.m_Character = AICF_GroupRuntime.ResolveAliveLeader(builder.m_Group);
			builder.m_CharacterId = builder.m_Character.GetID();
			SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(builder.m_Character);
			bool hasTool = gadgets && gadgets.GetGadgetByType(EGadgetType.BUILDING_TOOL);
			Log(builder, "BUILDER_READY", string.Format("agents=1 tool_present=%1", hasTool));
		}
		if (!IsWorkerValid(builder))
		{
			Retire(builder, "WORKER_UNAVAILABLE");
			return;
		}
		int count, recovered;
		m_LOD.KeepCaptureEligible(builder.m_Group, count, recovered);
		if (builder.m_Target && (!IsTargetValid(builder, builder.m_Target) ||
			builder.m_Target.GetOwner().GetID() != builder.m_TargetId ||
			vector.DistanceSq(builder.m_Target.GetOwner().GetOrigin(), builder.m_vTargetPosition) > 0.01))
			ClearTarget(builder);
		if (!builder.m_Target)
		{
			builder.m_Target = SelectTarget(builder, builder.m_Character.GetOrigin());
			if (builder.m_Target)
			{
				builder.m_TargetId = builder.m_Target.GetOwner().GetID();
				builder.m_vTargetPosition = builder.m_Target.GetOwner().GetOrigin();
				builder.m_bReturning = false;
				builder.m_iIdleAtMs = 0;
				builder.m_iMoveAtMs = now;
				builder.m_iLastOrderAtMs = 0;
				SelectWorkPosition(builder);
				Log(builder, "BUILDER_TARGET_ASSIGNED");
			}
		}
		if (!builder.m_Target)
		{
			ReturnHome(builder, home, now);
			return;
		}
		if (!MoveTo(builder, builder.m_vWorkPosition, now))
		{
			StopTool(builder);
			builder.m_iWorkAtMs = 0;
			if (now - builder.m_iMoveAtMs >= MOVE_TIMEOUT_MS)
			{
				builder.m_aDeferredTargets.Insert(builder.m_Target);
				builder.m_aDeferredUntilMs.Insert(now + RETRY_DELAY_MS);
				Log(builder, "BUILDER_TARGET_DEFERRED");
				ClearTarget(builder);
			}
			return;
		}
		Build(builder, now);
	}

	// Ближайшая сторона footprint: строитель стоит у проекта, а не за полным
	// диаметром его stock danger area. Порядок сторон задаёт устойчивый tie-break.
	protected void SelectWorkPosition(AICF_BaseBuilder builder)
	{
		builder.m_Target.GetCompositionLayout().GetOwner().GetWorldBounds(builder.m_vFootprintMin, builder.m_vFootprintMax);
		vector mins = builder.m_vFootprintMin;
		vector maxs = builder.m_vFootprintMax;
		vector origin = builder.m_Character.GetOrigin();
		float x = Math.Clamp(origin[0], mins[0], maxs[0]);
		float z = Math.Clamp(origin[2], mins[2], maxs[2]);
		array<vector> candidates = {
			Vector(mins[0] - 2, origin[1], z), Vector(maxs[0] + 2, origin[1], z),
			Vector(x, origin[1], mins[2] - 2), Vector(x, origin[1], maxs[2] + 2)
		};
		float distance = float.MAX;
		foreach (vector candidate : candidates)
		{
			float candidateDistance = vector.DistanceSqXZ(origin, candidate);
			if (candidateDistance >= distance)
				continue;
			distance = candidateDistance;
			builder.m_vWorkPosition = candidate;
		}
		builder.m_vWorkPosition[1] = GetGame().GetWorld().GetSurfaceY(builder.m_vWorkPosition[0], builder.m_vWorkPosition[2]);
	}

	static AICF_BaseBuilder FindWorkingOnLayout(SCR_CampaignBuildingLayoutComponent layout)
	{
		if (!Replication.IsServer() || !s_Instance || s_Instance.m_bStopped || !layout)
			return null;
		foreach (AICF_BaseBuilder builder : s_Instance.m_aBuilders)
		{
			if (s_Instance.IsWorkerValid(builder) && builder.m_Target &&
				builder.m_Target.GetOwner().GetID() == builder.m_TargetId &&
				s_Instance.IsTargetValid(builder, builder.m_Target) && builder.m_Target.GetCompositionLayout() == layout &&
				builder.m_bToolActive && builder.IsOutsideFootprint(builder.m_Character.GetOrigin()) &&
				vector.DistanceSqXZ(builder.m_Character.GetOrigin(), builder.m_vWorkPosition) <= ARRIVAL_METERS * ARRIVAL_METERS)
				return builder;
		}
		return null;
	}

	protected bool IsWorkerValid(AICF_BaseBuilder builder)
	{
		if (!builder.m_Group || builder.m_Group.GetID() != builder.m_GroupId ||
			!builder.m_Character || builder.m_Character.GetID() != builder.m_CharacterId ||
			!AICF_GroupRuntime.IsAliveCharacter(builder.m_Character))
			return false;
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(builder.m_Character.FindComponent(CharacterControllerComponent));
		RplComponent rpl = RplComponent.Cast(builder.m_Character.FindComponent(RplComponent));
		AIControlComponent control = AIControlComponent.Cast(builder.m_Character.FindComponent(AIControlComponent));
		return controller && !controller.IsPlayerControlled() && rpl && rpl.IsMaster() &&
			control && control.GetAIAgent() && control.GetAIAgent().GetParentGroup() == builder.m_Group &&
			SCR_Faction.GetEntityFaction(builder.m_Character) == builder.m_Faction;
	}

	protected bool MoveTo(AICF_BaseBuilder builder, vector destination, int now, float arrivalRadius = ARRIVAL_METERS)
	{
		if (vector.DistanceSqXZ(builder.m_Character.GetOrigin(), destination) <= arrivalRadius * arrivalRadius &&
			Math.AbsFloat(builder.m_Character.GetOrigin()[1] - GetGame().GetWorld().GetSurfaceY(destination[0], destination[2])) <= ARRIVAL_METERS)
		{
			m_Planner.ClearBuilderWaypoint(builder);
			return true;
		}
		if ((builder.m_iLastOrderAtMs == 0 || now - builder.m_iLastOrderAtMs >= 5000) &&
			(!builder.m_Waypoint || vector.DistanceSqXZ(builder.m_vDestination, destination) > 1 || now - builder.m_iLastOrderAtMs >= 15000))
		{
			m_Planner.SetBuilderWaypoint(builder, destination, arrivalRadius * 0.5);
			builder.m_vDestination = destination;
			builder.m_iLastOrderAtMs = now;
		}
		return false;
	}

	protected void ReturnHome(AICF_BaseBuilder builder, vector home, int now)
	{
		if (!builder.m_bReturning)
		{
			builder.m_bReturning = true;
			builder.m_iMoveAtMs = now;
			Log(builder, "BUILDER_RETURNING");
		}
		if (!MoveTo(builder, home, now, HOME_METERS))
		{
			builder.m_iIdleAtMs = 0;
			// Никакого despawn по одному лишь таймеру: сначала физическое прибытие.
			return;
		}
		if (builder.m_iIdleAtMs == 0)
		{
			builder.m_iIdleAtMs = now;
			Log(builder, "BUILDER_HOME");
		}
		if (now - builder.m_iIdleAtMs >= IDLE_DELAY_MS)
			Retire(builder, "IDLE_AT_MAIN_TENT");
	}

	protected void Build(AICF_BaseBuilder builder, int now)
	{
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(builder.m_Character.FindComponent(CharacterControllerComponent));
		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(builder.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
		if (!IsWorkerValid(builder) || !IsTargetValid(builder, builder.m_Target) || controller.IsUnconscious() ||
			!utility || utility.GetThreatMeasure() > 0.01 || CompartmentAccessComponent.GetVehicleIn(builder.m_Character) ||
			!builder.IsOutsideFootprint(builder.m_Character.GetOrigin()))
		{
			StopTool(builder);
			builder.m_iWorkAtMs = 0;
			return;
		}
		if (!StartTool(builder, controller))
		{
			builder.m_iWorkAtMs = 0;
			return;
		}
		if (builder.m_iWorkAtMs == 0)
		{
			builder.m_iWorkAtMs = now;
			Log(builder, "BUILDER_WORK_STARTED");
		}
		if (now - builder.m_iWorkAtMs < WORK_INTERVAL_MS)
			return;
		builder.m_iWorkAtMs = now;
		SCR_CampaignBuildingLayoutComponent layout = builder.m_Target.GetCompositionLayout();
		SCR_CampaignBuildingGadgetToolComponent tool = SCR_CampaignBuildingGadgetToolComponent.Cast(builder.m_UsedTool.FindComponent(SCR_CampaignBuildingGadgetToolComponent));
		if (!tool || tool.GetToolConstructionValue() <= 0)
			return;
		int value = tool.GetToolConstructionValue();
		bool completing = layout.GetCurrentBuildValue() + value >= layout.GetToBuildValue();
		Log(builder, "BUILDER_PROGRESS", string.Format("value=%1 before=%2 total=%3", value, layout.GetCurrentBuildValue(), layout.GetToBuildValue()));
		// Stock вызывает replication, service activation и удаляет layout при completion.
		// После вызова не обращаться к layout: он мог быть синхронно уничтожен.
		layout.AddBuildingValue(value);
		if (completing)
		{
			Log(builder, "BUILDER_COMPLETED");
			ClearTarget(builder);
		}
	}

	protected bool StartTool(AICF_BaseBuilder builder, CharacterControllerComponent controller)
	{
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(builder.m_Character);
		if (!gadgets)
			return false;
		IEntity tool = gadgets.GetGadgetByType(EGadgetType.BUILDING_TOOL);
		CharacterAnimationComponent animation = controller.GetAnimationComponent();
		if (!tool || !animation)
			return false;
		if (!builder.m_ToolController)
		{
			builder.m_ToolController = SCR_CharacterControllerComponent.Cast(controller);
			if (!builder.m_ToolController)
				return false;
			builder.m_UsedTool = tool;
			builder.m_ToolController.m_OnItemUseBeganInvoker.Insert(builder.OnToolUseBegan);
			builder.m_ToolController.m_OnItemUseEndedInvoker.Insert(builder.OnToolUseEnded);
		}
		// Штатный AI сначала достаёт gadget и ждёт завершения equip-анимации.
		if (controller.GetAttachedGadgetAtLeftHandSlot() != tool)
		{
			if (controller.CanEquipGadget(tool) && System.GetTickCount() - builder.m_iToolRequestAtMs >= 3000)
			{
				gadgets.SetGadgetMode(tool, EGadgetMode.IN_HAND);
				builder.m_iToolRequestAtMs = System.GetTickCount();
				Log(builder, "BUILDER_TOOL_EQUIP");
			}
			return false;
		}
		if (builder.m_bToolActive && controller.IsUsingItem())
			return true;
		if (controller.IsUsingItem() || !controller.CanUseItem())
			return false;
		ItemUseParameters params = new ItemUseParameters();
		params.SetEntity(tool);
		params.SetAllowMovementDuringAction(false);
		params.SetKeepInHandAfterSuccess(true);
		params.SetCommandID(animation.BindCommand("CMD_Item_Action"));
		params.SetCommandIntArg(1);
		if (controller.TryUseItemOverrideParams(params))
			Log(builder, "BUILDER_TOOL_REQUESTED");
		return false;
	}

	protected void StopTool(AICF_BaseBuilder builder)
	{
		if (builder.m_ToolController)
		{
			builder.m_ToolController.m_OnItemUseBeganInvoker.Remove(builder.OnToolUseBegan);
			builder.m_ToolController.m_OnItemUseEndedInvoker.Remove(builder.OnToolUseEnded);
			builder.m_ToolController = null;
		}
		builder.m_bToolActive = false;
		builder.m_iToolRequestAtMs = 0;
		if (!builder.m_UsedTool || !builder.m_Character)
			return;
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(builder.m_Character.FindComponent(CharacterControllerComponent));
		if (controller && IsWorkerValid(builder) &&
			controller.GetAttachedGadgetAtLeftHandSlot() == builder.m_UsedTool)
		{
			CharacterAnimationComponent animation = controller.GetAnimationComponent();
			if (animation && animation.GetCommandHandler())
				animation.GetCommandHandler().FinishItemUse(true);
			SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(builder.m_Character);
			if (gadgets)
				gadgets.SetGadgetMode(builder.m_UsedTool, EGadgetMode.IN_STORAGE);
		}
		builder.m_UsedTool = null;
	}

	protected void ClearTarget(AICF_BaseBuilder builder)
	{
		StopTool(builder);
		m_Planner.ClearBuilderWaypoint(builder);
		builder.m_Target = null;
		builder.m_iWorkAtMs = 0;
		builder.m_iLastOrderAtMs = 0;
	}

	protected bool Retire(AICF_BaseBuilder builder, string reason)
	{
		if (!Replication.IsServer())
			return false;
		builder.m_bRetiring = true;
		ClearTarget(builder);
		SCR_AIGroup group = builder.m_Group;
		if (!group && AICF_GroupRuntime.IsAliveCharacter(builder.m_Character))
			return false;
		if (group)
		{
			RplComponent groupRpl = RplComponent.Cast(group.FindComponent(RplComponent));
			if (group.GetID() != builder.m_GroupId || group.GetFaction() != builder.m_Faction || !groupRpl || !groupRpl.IsMaster())
				return false;
			ChimeraAIWorld world = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
			if (world)
				world.PurgeSpawnRequestsForGroup(group);
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			if (agents.Count() > 1)
				return false;
			foreach (AIAgent agent : agents)
			{
				if (!agent || !agent.GetControlledEntity())
					continue;
				IEntity entity = agent.GetControlledEntity();
				if (builder.m_CharacterId != EntityID.INVALID && entity.GetID() != builder.m_CharacterId)
					return false;
				CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
				RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
				if (!controller || controller.IsPlayerControlled() || !rpl || !rpl.IsMaster() || agent.GetParentGroup() != group)
					return false;
			}
			m_LOD.Release(group);
			group.DespawnMembers();
			if (group)
				RplComponent.DeleteRplEntity(group, false);
		}
		Log(builder, "BUILDER_RETIRED", "reason=" + reason);
		builder.m_Group = null;
		builder.m_Character = null;
		builder.m_GroupId = EntityID.INVALID;
		builder.m_CharacterId = EntityID.INVALID;
		builder.m_Faction = null;
		builder.m_iSpawnAtMs = 0;
		builder.m_iIdleAtMs = 0;
		builder.m_iRetryAtMs = System.GetTickCount() + RETRY_DELAY_MS;
		if (reason == "IDLE_AT_MAIN_TENT")
			builder.m_iRetryAtMs = 0;
		builder.m_bReturning = false;
		builder.m_bRetiring = false;
		return true;
	}

	protected void Log(AICF_BaseBuilder builder, string eventName, string details = "")
	{
		FactionKey factionKey;
		string groupId = "NONE";
		string targetId = "NONE";
		if (builder.m_Faction)
			factionKey = builder.m_Faction.GetFactionKey();
		if (builder.m_GroupId != EntityID.INVALID)
			groupId = builder.m_GroupId.ToString();
		if (builder.m_TargetId != EntityID.INVALID)
			targetId = builder.m_TargetId.ToString();
		if (builder.m_Character)
		{
			RplComponent rpl = RplComponent.Cast(builder.m_Character.FindComponent(RplComponent));
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(builder.m_Character.FindComponent(CharacterControllerComponent));
			details += string.Format(" character=%1 position=%2 work=%3 tool_active=%4 item_using=%5", builder.m_CharacterId,
				builder.m_Character.GetOrigin(), builder.m_vWorkPosition, builder.m_bToolActive, controller && controller.IsUsingItem());
			if (rpl)
				details += " character_rpl=" + rpl.Id().ToString();
		}
		AICF_Stage1Diagnostics.Info(eventName, string.Format("base=%1 faction=%2 numeric_slot=%3 generation=%4 group=%5 target=%6 %7",
			AICF_Stage1Diagnostics.BaseKey(builder.m_Base), factionKey, builder.m_iSlotId, builder.m_iGeneration,
			groupId, targetId, details));
	}
}
