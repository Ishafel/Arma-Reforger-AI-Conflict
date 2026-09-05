// Owns only AICF-created waypoints and replaces them without touching prefab orders.
class AICF_OrderPlanner
{
	protected static const ResourceName ATTACK_OPERATIONAL_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
	protected static const ResourceName ATTACK_OBJECTIVE_ACTION_PREFAB = "{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et";
	protected static const ResourceName DEFEND_WAYPOINT_PREFAB = "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et";
	protected static const ResourceName RELAY_WAYPOINT_PREFAB = "{EAAE93F98ED5D218}Prefabs/AI/Waypoints/AIWaypoint_CaptureRelay.et";
	protected static const string RELAY_SMART_ACTION_TAG = "CapturePoint";
	protected static const float ATTACK_OPERATIONAL_RADIUS_METERS = 20.0;
	protected static const float ATTACK_OBJECTIVE_ACTION_RADIUS_METERS = 20.0;
	protected static const float ATTACK_OBJECTIVE_PROMOTION_RADIUS_METERS = 100.0;
	protected static const float ATTACK_OBJECTIVE_HOLDING_TIME_SECONDS = 600.0;
	protected static const float DEFEND_RADIUS_METERS = 50.0;
	protected static const float DEFEND_HOLDING_TIME_SECONDS = 3600.0;
	protected static const float RELAY_RADIUS_METERS = 20.0;
	protected static const float FALSE_COMPLETION_ROUTE_LEG_METERS = 140.0;
	protected static const float FALSE_COMPLETION_ROUTE_MIN_PROGRESS_METERS = 30.0;
	protected static const float FALSE_COMPLETION_ROUTE_MAX_DETOUR_METERS = 80.0;
	protected static const int FALSE_COMPLETION_ROUTE_SAMPLE_COUNT = 16;
	protected static const vector NAVMESH_ENDPOINT_SEARCH_HALF_EXTENTS = "20 20 20";
	protected static const vector PLAYER_POINT_NAVMESH_SEARCH_HALF_EXTENTS = "25 30 25";
	protected static const float PLAYER_POINT_MAX_NAVMESH_OFFSET_METERS = 25.0;
	protected static const float POSITION_HOLD_RADIUS_METERS = 20.0;
	protected static const string POSTURE_ATTACK_PRIMARY = "ATTACK_PRIMARY";
	protected static const string POSTURE_ATTACK_SECONDARY = "ATTACK_SECONDARY";
	protected static const string POSTURE_ATTACK_SUPPORT = "ATTACK_SUPPORT";
	protected static const string POSTURE_FORWARD_DEFEND = "FORWARD_DEFEND";
	protected static const string POSTURE_QRF = "QRF";
	protected static const string POSTURE_IDLE_RESERVE = "IDLE_RESERVE";
	protected static const string POSTURE_PLAYER_ATTACK = "PLAYER_ATTACK";
	protected static const string POSTURE_PLAYER_DEFEND = "PLAYER_DEFEND";
	protected static const string POSTURE_PLAYER_RESERVE = "PLAYER_RESERVE";
	protected static const string POSTURE_SYSTEM_HOLD = "SYSTEM_HOLD";
	protected static const string POSTURE_MOVE_AND_HOLD = "MOVE_AND_HOLD";

	protected ref AICF_CommandAuthorityPolicy m_AuthorityPolicy;

	void AICF_OrderPlanner(AICF_CommandAuthorityPolicy authorityPolicy)
	{
		m_AuthorityPolicy = authorityPolicy;
	}

	// Вспомогательный строитель использует тот же boundary владения waypoint,
	// но не получает стратегический армейский assignment или vehicle admission.
	bool SetBuilderWaypoint(AICF_BaseBuilder builder, vector position, float radius)
	{
		if (!Replication.IsServer() || !builder || !builder.m_Group || builder.m_Group.GetID() != builder.m_GroupId)
			return false;
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(builder.m_Group.FindComponent(AIPathfindingComponent));
		if (!pathfinding)
			return false;
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		if (!navmesh)
			return false;
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		navmesh.LoadTileIn(position);
		vector endpoint;
		vector searchExtents = Vector(radius, 5, radius);
		if (!pathfinding.GetClosestPositionOnNavmesh(position, searchExtents, endpoint) || vector.DistanceSqXZ(endpoint, position) > radius * radius)
			return false;
		Resource resource = Resource.Load(ATTACK_OPERATIONAL_WAYPOINT_PREFAB);
		if (!resource || !resource.IsValid())
			return false;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = endpoint;
		IEntity entity = GetGame().SpawnEntityPrefabEx(ATTACK_OPERATIONAL_WAYPOINT_PREFAB, false, params: params);
		AIWaypoint waypoint = AIWaypoint.Cast(entity);
		if (!waypoint)
		{
			if (entity)
				RplComponent.DeleteRplEntity(entity, false);
			return false;
		}
		waypoint.SetCompletionRadius(radius);
		waypoint.SetCompletionType(EAIWaypointCompletionType.All);
		ClearBuilderWaypoint(builder);
		builder.m_Waypoint = waypoint;
		builder.m_Group.AddWaypoint(waypoint);
		return true;
	}

	void ClearBuilderWaypoint(AICF_BaseBuilder builder)
	{
		if (!Replication.IsServer() || !builder || !builder.m_Waypoint)
			return;
		if (builder.m_Group && builder.m_Group.GetID() != builder.m_GroupId)
			return;
		if (builder.m_Group && builder.m_Group.GetID() == builder.m_GroupId)
			builder.m_Group.RemoveWaypoint(builder.m_Waypoint);
		RplComponent.DeleteRplEntity(builder.m_Waypoint, false);
		builder.m_Waypoint = null;
	}

	bool AssignOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		string reason,
		SCR_CampaignMilitaryBaseComponent excludedTarget = null,
		bool waypointSuspendedByVehicle = false)
	{
		if (slot && slot.IsRecruitingInfantry())
			return true;
		if (!Replication.IsServer() || !slot || !faction || !graph || !targetSelector ||
			!m_AuthorityPolicy || !m_AuthorityPolicy.IsValid())
			return false;

		if (slot.HasPlayerStrategicIntent())
		{
			if (!slot.IsPlayerStrategicIntentRoleCurrent() ||
				(slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.BASE &&
				!IsTargetValidForRole(
					slot,
					faction,
					slot.GetPlayerStrategicIntentTargetBase())) ||
				(slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.POSITION &&
				!IsStrategicIntentDestinationValid(slot, faction)))
			{
				InvalidatePlayerStrategicIntent(slot, faction, reason);
			}
			else
			{
				if (waypointSuspendedByVehicle)
				{
					if (slot.GetPlayerStrategicIntentTargetKind() ==
						AICF_EOrderTargetKind.POSITION)
					{
						return ApplySuspendedPointAssignment(
							slot,
							faction,
							slot.GetPlayerStrategicIntentTargetPosition(),
							AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
							reason,
							"PLAYER_INTENT_RESTORE",
							false);
					}
					return ApplySuspendedStrategicAssignment(
						slot,
						faction,
						slot.GetPlayerStrategicIntentTargetBase(),
						ResolvePlayerPosture(slot),
						AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
						reason,
						"PLAYER_INTENT_RESTORE",
						false);
				}
				return RestorePlayerStrategicIntent(slot, faction, reason);
			}
		}

		if (!m_AuthorityPolicy.IsAICommanderEnabled(faction.GetFactionKey()))
		{
			return AssignSystemHold(
				slot,
				faction,
				reason,
				waypointSuspendedByVehicle);
		}

		// Generic lifecycle/reliability/vehicle callers may restore a durable AI
		// intent, but only the faction-scoped AICF_AICommander may select a new one.
		if (slot.HasStrategicIntent() &&
			slot.GetStrategicIntentAuthority() ==
				AICF_EStrategicDecisionAuthority.AI_COMMANDER)
		{
			SCR_CampaignMilitaryBaseComponent intentTarget =
				slot.GetStrategicIntentTargetBase();
			if (slot.IsStrategicIntentRoleCurrent() &&
				IsTargetValidForRole(
					slot,
					faction,
					intentTarget))
			{
				if (intentTarget != excludedTarget)
				{
					if (waypointSuspendedByVehicle)
					{
						return ApplySuspendedStrategicAssignment(
							slot,
							faction,
							intentTarget,
							slot.GetStrategicIntentPosture(),
							AICF_EStrategicDecisionAuthority.AI_COMMANDER,
							reason,
							"STRATEGIC_INTENT_RESTORE",
							false);
					}
					return RestoreStrategicIntent(slot, faction, reason);
				}
			}
			else
			{
				InvalidateStrategicIntent(slot, faction, reason);
			}
		}

		return false;
	}

	bool AssignAICommanderOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		string reason,
		SCR_CampaignMilitaryBaseComponent excludedTarget = null,
		bool waypointSuspendedByVehicle = false)
	{
		if (slot && slot.IsRecruitingInfantry())
			return true;
		if (!Replication.IsServer() || !slot || !faction || !graph || !targetSelector ||
			!m_AuthorityPolicy || !m_AuthorityPolicy.IsValid() ||
			!m_AuthorityPolicy.IsAICommanderEnabled(faction.GetFactionKey()))
		{
			return false;
		}

		if (slot.HasPlayerStrategicIntent())
		{
			if (!slot.IsPlayerStrategicIntentRoleCurrent() ||
				(slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.BASE &&
				!IsTargetValidForRole(
					slot,
					faction,
					slot.GetPlayerStrategicIntentTargetBase())) ||
				(slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.POSITION &&
				!IsStrategicIntentDestinationValid(slot, faction)))
			{
				InvalidatePlayerStrategicIntent(slot, faction, reason);
			}
			else
			{
				if (waypointSuspendedByVehicle)
				{
					if (slot.GetPlayerStrategicIntentTargetKind() ==
						AICF_EOrderTargetKind.POSITION)
					{
						return ApplySuspendedPointAssignment(
							slot,
							faction,
							slot.GetPlayerStrategicIntentTargetPosition(),
							AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
							reason,
							"PLAYER_INTENT_RESTORE",
							false);
					}
					return ApplySuspendedStrategicAssignment(
						slot,
						faction,
						slot.GetPlayerStrategicIntentTargetBase(),
						ResolvePlayerPosture(slot),
						AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
						reason,
						"PLAYER_INTENT_RESTORE",
						false);
				}
				return RestorePlayerStrategicIntent(slot, faction, reason);
			}
		}
		if (slot.HasPlayerStrategicOrder())
			return false;
		if (slot.HasStrategicIntent() &&
			slot.GetStrategicIntentAuthority() ==
				AICF_EStrategicDecisionAuthority.AI_COMMANDER)
		{
			SCR_CampaignMilitaryBaseComponent intentTarget =
				slot.GetStrategicIntentTargetBase();
			if (slot.IsStrategicIntentRoleCurrent() &&
				IsTargetValidForRole(
					slot,
					faction,
					intentTarget))
			{
				if (intentTarget != excludedTarget)
				{
					if (waypointSuspendedByVehicle)
					{
						return ApplySuspendedStrategicAssignment(
							slot,
							faction,
							intentTarget,
							slot.GetStrategicIntentPosture(),
							AICF_EStrategicDecisionAuthority.AI_COMMANDER,
							reason,
							"STRATEGIC_INTENT_RESTORE",
							false);
					}
					return RestoreStrategicIntent(slot, faction, reason);
				}
			}
			else
			{
				InvalidateStrategicIntent(slot, faction, reason);
			}
		}

		string posture;
		string trigger;
		SCR_CampaignMilitaryBaseComponent target = SelectOperationalTarget(
			slot,
			faction,
			graph,
			targetSelector,
			excludedTarget,
			posture,
			trigger);

		if (!target)
		{
			if (slot.MarkTargetUnavailableReported())
			{
				AICF_Stage1Diagnostics.Warning(
					"ORDER_TARGET_UNAVAILABLE",
					string.Format(
						"faction=%1 slot=%2 role=%3 reason=%4",
						faction.GetFactionKey(),
						slot.GetSlotId(),
						AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
						reason));
			}
			return false;
		}

		if (waypointSuspendedByVehicle)
		{
			return ApplySuspendedStrategicAssignment(
				slot,
				faction,
				target,
				posture,
				AICF_EStrategicDecisionAuthority.AI_COMMANDER,
				reason,
				trigger);
		}

		return ReplaceOrder(
			slot,
			faction,
			target,
			reason,
			posture,
			trigger,
			false,
			AICF_EStrategicDecisionAuthority.AI_COMMANDER);
	}

	// Applies an allied player's explicit strategic target. The role remains
	// server-owned: ATTACK slots may only receive legal enemy objectives,
	// DEFEND slots friendly spawn bases, and RESERVE slots their faction HQ.
	bool AssignPlayerOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target,
		bool waypointSuspendedByVehicle = false)
	{
		if (!Replication.IsServer() || !slot || !faction || !target ||
			!slot.IsCombatReady() || !m_AuthorityPolicy ||
			!m_AuthorityPolicy.IsValid() ||
			!IsTargetValidForRole(slot, faction, target))
		{
			return false;
		}

		string posture = ResolvePlayerPosture(slot);
		if (slot.HasPlayerStrategicIntent() &&
			slot.IsPlayerStrategicIntentRoleCurrent() &&
			slot.GetPlayerStrategicIntentTargetBase() == target &&
			slot.GetDecisionAuthority() ==
				AICF_EStrategicDecisionAuthority.PLAYER_COMMAND &&
			slot.GetTargetBase() == target &&
			slot.GetOperationalPosture() == posture &&
			(waypointSuspendedByVehicle ||
				GetOrderFailureReason(slot, faction).IsEmpty()))
		{
			slot.RecordPlayerStrategicIntent(target);
			return true;
		}

		if (waypointSuspendedByVehicle)
		{
			if (!ApplySuspendedStrategicAssignment(
				slot,
				faction,
				target,
				posture,
				AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
				"PLAYER_COMMAND",
				"PLAYER_COMMAND"))
			{
				return false;
			}
			slot.RecordPlayerStrategicIntent(target);
			return true;
		}

		if (!ReplaceOrder(
			slot,
			faction,
			target,
			"PLAYER_COMMAND",
			posture,
			"PLAYER_COMMAND",
			false,
			AICF_EStrategicDecisionAuthority.PLAYER_COMMAND))
			return false;

		slot.RecordPlayerStrategicIntent(target);
		return true;
	}

	bool AssignPlayerPointOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		vector targetPosition,
		bool waypointSuspendedByVehicle = false)
	{
		if (!Replication.IsServer() || !slot || !faction ||
			!slot.IsCombatReady() || !m_AuthorityPolicy ||
			!m_AuthorityPolicy.IsValid())
		{
			return false;
		}

		if (slot.HasPlayerStrategicIntent() &&
			slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.POSITION &&
			slot.GetPlayerStrategicIntentTargetPosition() == targetPosition &&
			slot.GetDecisionAuthority() ==
				AICF_EStrategicDecisionAuthority.PLAYER_COMMAND &&
			slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION &&
			slot.GetTargetPosition() == targetPosition &&
			(waypointSuspendedByVehicle || GetOrderFailureReason(slot, faction).IsEmpty()))
		{
			slot.RecordPlayerStrategicPointIntent(targetPosition);
			return true;
		}

		if (waypointSuspendedByVehicle)
		{
			if (!ApplySuspendedPointAssignment(
				slot,
				faction,
				targetPosition,
				AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
				"PLAYER_COMMAND",
				"PLAYER_COMMAND"))
			{
				return false;
			}
			slot.RecordPlayerStrategicPointIntent(targetPosition);
			return true;
		}

		if (!ReplacePointOrder(
			slot,
			faction,
			targetPosition,
			"PLAYER_COMMAND",
			"PLAYER_COMMAND",
			AICF_EStrategicDecisionAuthority.PLAYER_COMMAND))
		{
			return false;
		}

		slot.RecordPlayerStrategicPointIntent(targetPosition);
		return true;
	}

	bool AssignSystemHold(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason,
		bool waypointSuspendedByVehicle = false)
	{
		if (!Replication.IsServer() || !slot || !faction || !slot.IsCombatReady() ||
			!m_AuthorityPolicy || !m_AuthorityPolicy.IsValid() ||
			m_AuthorityPolicy.IsAICommanderEnabled(faction.GetFactionKey()))
			return false;

		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (!mainBase || !mainBase.GetOwner() || !mainBase.IsInitialized() ||
			mainBase.GetFaction() != faction)
		{
			return false;
		}

		if (waypointSuspendedByVehicle)
		{
			bool suspendedAssigned = ApplySuspendedStrategicAssignment(
				slot,
				faction,
				mainBase,
				POSTURE_SYSTEM_HOLD,
				AICF_EStrategicDecisionAuthority.SYSTEM_HOLD,
				reason,
				"NO_PLAYER_ORDER");
			if (!suspendedAssigned && (!slot.IsSystemHoldOrder() ||
				slot.GetTargetBase() != mainBase))
			{
				return false;
			}
			slot.BeginAwaitingPlayerCommand();
			LogCommandWaiting(slot, faction);
			return true;
		}

		if (slot.IsSystemHoldOrder() && slot.GetTargetBase() == mainBase &&
			GetOrderFailureReason(slot, faction).IsEmpty())
		{
			slot.CommitStrategicIntent(
				mainBase,
				POSTURE_SYSTEM_HOLD,
				AICF_EStrategicDecisionAuthority.SYSTEM_HOLD);
			slot.BeginAwaitingPlayerCommand();
			LogCommandWaiting(slot, faction);
			return true;
		}

		bool assigned = ReplaceOrder(
			slot,
			faction,
			mainBase,
			reason,
			POSTURE_SYSTEM_HOLD,
			"NO_PLAYER_ORDER",
			false,
			AICF_EStrategicDecisionAuthority.SYSTEM_HOLD,
			true);
		if (!assigned)
			return false;

		slot.BeginAwaitingPlayerCommand();
		LogCommandWaiting(slot, faction);
		return true;
	}

	protected bool RestorePlayerStrategicIntent(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason)
	{
		if (!slot || !faction || !slot.HasPlayerStrategicIntent() ||
			!slot.IsPlayerStrategicIntentRoleCurrent())
		{
			return false;
		}

		if (slot.GetPlayerStrategicIntentTargetKind() ==
			AICF_EOrderTargetKind.POSITION)
		{
			bool pointRestored = ReplacePointOrder(
				slot,
				faction,
				slot.GetPlayerStrategicIntentTargetPosition(),
				reason,
				"PLAYER_INTENT_RESTORE",
				AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
				false);
			if (!pointRestored)
				return false;
			slot.ClearAwaitingPlayerCommand();
			LogPlayerPointIntentRestored(slot, faction, reason);
			return true;
		}

		SCR_CampaignMilitaryBaseComponent target =
			slot.GetPlayerStrategicIntentTargetBase();
		if (!IsTargetValidForRole(slot, faction, target))
			return false;

		string posture = POSTURE_PLAYER_RESERVE;
		if (slot.GetRole() == AICF_EGroupRole.ATTACK)
			posture = POSTURE_PLAYER_ATTACK;
		else if (slot.GetRole() == AICF_EGroupRole.DEFEND)
			posture = POSTURE_PLAYER_DEFEND;

		if (!ReplaceOrder(
			slot,
			faction,
			target,
			reason,
			posture,
			"PLAYER_INTENT_RESTORE",
			false,
			AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
			false,
			false))
		{
			return false;
		}

		slot.ClearAwaitingPlayerCommand();
		AICF_Stage1Diagnostics.Info(
			"PLAYER_INTENT_RESTORED",
			string.Format(
				"faction=%1 slot=%2 stable_slot=%3 target=%4 intent_revision=%5 reason=%6",
				faction.GetFactionKey(),
				slot.GetSlotId(),
				slot.GetStableSlotKey(),
				AICF_Stage1Diagnostics.BaseKey(target),
				slot.GetPlayerStrategicIntentRevision(),
				reason));
		return true;
	}

	protected bool RestoreStrategicIntent(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason)
	{
		if (!slot || !faction || !slot.HasStrategicIntent() ||
			!slot.IsStrategicIntentRoleCurrent())
		{
			return false;
		}

		SCR_CampaignMilitaryBaseComponent target =
			slot.GetStrategicIntentTargetBase();
		AICF_EStrategicDecisionAuthority authority =
			slot.GetStrategicIntentAuthority();
		if (!IsTargetValidForRole(slot, faction, target) ||
			authority == AICF_EStrategicDecisionAuthority.NONE ||
			authority == AICF_EStrategicDecisionAuthority.SYSTEM_HOLD)
		{
			return false;
		}

		return ReplaceOrder(
			slot,
			faction,
			target,
			reason,
			slot.GetStrategicIntentPosture(),
			"STRATEGIC_INTENT_RESTORE",
			false,
			authority,
			false,
			false);
	}

	protected string ResolvePlayerPosture(AICF_GroupSlot slot)
	{
		if (slot && slot.GetRole() == AICF_EGroupRole.ATTACK)
			return POSTURE_PLAYER_ATTACK;
		if (slot && slot.GetRole() == AICF_EGroupRole.DEFEND)
			return POSTURE_PLAYER_DEFEND;
		return POSTURE_PLAYER_RESERVE;
	}

	protected bool IsStrategicIntentDestinationValid(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction)
	{
		if (!slot || !faction || !slot.HasStrategicIntent())
			return false;
		if (slot.GetStrategicIntentTargetKind() == AICF_EOrderTargetKind.POSITION)
			return slot.GetStrategicIntentAuthority() ==
				AICF_EStrategicDecisionAuthority.PLAYER_COMMAND;
		return IsTargetValidForRole(
			slot,
			faction,
			slot.GetStrategicIntentTargetBase());
	}

	protected void LogPlayerPointIntentRestored(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason)
	{
		vector targetPosition = slot.GetPlayerStrategicIntentTargetPosition();
		string details = string.Format(
			"faction=%1 slot=%2 stable_slot=%3 target=MAP_POINT target_kind=POSITION target_x=%4 target_z=%5 intent_revision=%6 reason=%7",
			faction.GetFactionKey(),
			slot.GetSlotId(),
			slot.GetStableSlotKey(),
			Math.Round(targetPosition[0]),
			Math.Round(targetPosition[2]),
			slot.GetPlayerStrategicIntentRevision(),
			reason);
		AICF_Stage1Diagnostics.Info("PLAYER_INTENT_RESTORED", details);
	}

	protected bool ApplySuspendedPointAssignment(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		vector targetPosition,
		AICF_EStrategicDecisionAuthority decisionAuthority,
		string reason,
		string trigger,
		bool updateStrategicIntent = true)
	{
		if (!slot || !faction || !slot.IsCombatReady() || slot.GetWaypoint() ||
			decisionAuthority != AICF_EStrategicDecisionAuthority.PLAYER_COMMAND)
		{
			return false;
		}

		AICF_EOrderTargetKind oldKind = slot.GetTargetKind();
		vector oldPosition = slot.GetTargetPosition();
		string oldPosture = slot.GetOperationalPosture();
		AICF_EStrategicDecisionAuthority oldAuthority = slot.GetDecisionAuthority();
		int oldIntentRevision = slot.GetStrategicIntentRevision();
		if ((oldKind != AICF_EOrderTargetKind.POSITION ||
			oldPosition != targetPosition) &&
			!slot.AssignSuspendedPointObjective(targetPosition))
		{
			return false;
		}

		slot.SetDecisionAuthority(decisionAuthority);
		slot.ClearAwaitingPlayerCommand();
		if (updateStrategicIntent)
			slot.CommitStrategicPointIntent(targetPosition, POSTURE_MOVE_AND_HOLD, decisionAuthority);
		slot.ResetTargetUnavailableReport();
		bool changed = oldKind != AICF_EOrderTargetKind.POSITION ||
			oldPosition != targetPosition || oldPosture != POSTURE_MOVE_AND_HOLD ||
			oldAuthority != decisionAuthority ||
			oldIntentRevision != slot.GetStrategicIntentRevision();
		if (!changed)
			return true;
		slot.RecordStrategicAssignment(null, POSTURE_MOVE_AND_HOLD);

		string details = BuildPointAssignmentDetails(
			slot,
			faction,
			targetPosition,
			trigger,
			reason,
			"NONE",
			decisionAuthority);
		details += " vehicle_control=1";
		AICF_Stage35Diagnostics.Info("STRATEGIC_ASSIGNMENT", details);
		return true;
	}

	// Vehicle handoff owns the waypoint queue during BOARDING/TRANSIT/DISMOUNT.
	// Strategic planning may advance its target/authority snapshot, but may not
	// create an infantry waypoint until RestoreInfantryOrder regains ownership.
	protected bool ApplySuspendedStrategicAssignment(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target,
		string posture,
		AICF_EStrategicDecisionAuthority decisionAuthority,
		string reason,
		string trigger,
		bool updateStrategicIntent = true)
	{
		if (!slot || !faction || !target || !slot.IsCombatReady() ||
			slot.GetWaypoint() ||
			decisionAuthority == AICF_EStrategicDecisionAuthority.NONE)
		{
			return false;
		}
		if (decisionAuthority != AICF_EStrategicDecisionAuthority.SYSTEM_HOLD &&
			!IsTargetValidForRole(slot, faction, target))
		{
			return false;
		}

		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		string oldPosture = slot.GetOperationalPosture();
		AICF_EStrategicDecisionAuthority oldDecisionAuthority =
			slot.GetDecisionAuthority();
		int oldIntentRevision = slot.GetStrategicIntentRevision();
		if (oldTarget != target && !slot.AssignSuspendedObjective(target))
			return false;

		slot.SetDecisionAuthority(decisionAuthority);
		if (decisionAuthority == AICF_EStrategicDecisionAuthority.SYSTEM_HOLD)
			slot.BeginAwaitingPlayerCommand();
		else
			slot.ClearAwaitingPlayerCommand();
		if (updateStrategicIntent)
			slot.CommitStrategicIntent(target, posture, decisionAuthority);
		slot.ResetTargetUnavailableReport();

		bool changed = oldTarget != target || oldPosture != posture ||
			oldDecisionAuthority != decisionAuthority ||
			oldIntentRevision != slot.GetStrategicIntentRevision();
		if (!changed)
			return false;

		slot.RecordStrategicAssignment(target, posture);
		string assignmentLine = string.Format(
			"faction=%1 slot=%2 stable_slot=%3 numeric_slot=%4 role=%5 posture=%6 target=%7 target_kind=BASE trigger=%8 reason=%9",
			faction.GetFactionKey(),
			slot.GetSlotKey(),
			slot.GetStableSlotKey(),
			slot.GetSlotId(),
			AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
			posture,
			AICF_Stage1Diagnostics.BaseKey(target),
			trigger,
			reason);
		assignmentLine += string.Format(
			" waypoint=NONE group=%1 group_generation=%2 assignment_revision=%3 assignment_age_ms=%4",
			slot.GetGroup().GetID(),
			slot.GetSpawnGeneration(),
			slot.GetStrategicAssignmentRevision(),
			slot.GetStrategicAssignmentAgeMs());
		assignmentLine += string.Format(
			" decision_authority=%1 intent_revision=%2 vehicle_control=1",
			AICF_StrategicDecisionAuthority.ToString(decisionAuthority),
			slot.GetStrategicIntentRevision());
		AICF_Stage35Diagnostics.Info("STRATEGIC_ASSIGNMENT", assignmentLine);
		return true;
	}

	protected void InvalidatePlayerStrategicIntent(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason)
	{
		if (!slot || !faction || !slot.HasPlayerStrategicIntent())
			return;

		string details = BuildDestinationDetails(
			slot.GetPlayerStrategicIntentTargetKind(),
			slot.GetPlayerStrategicIntentTargetBase(),
			slot.GetPlayerStrategicIntentTargetPosition());
		details += string.Format(
			" faction=%1 slot=%2 stable_slot=%3 role=%4 intent_revision=%5 reason=%6 next=AUTHORITY_REPLAN",
			faction.GetFactionKey(),
			slot.GetSlotId(),
			slot.GetStableSlotKey(),
			AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
			slot.GetPlayerStrategicIntentRevision(),
			reason);
		AICF_Stage1Diagnostics.Info("PLAYER_INTENT_INVALIDATED", details);
		slot.ClearPlayerStrategicIntent();
		slot.ClearPlayerStrategicOrder();
	}

	protected void InvalidateStrategicIntent(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason)
	{
		if (!slot || !faction || !slot.HasStrategicIntent())
			return;

		string details = BuildDestinationDetails(
			slot.GetStrategicIntentTargetKind(),
			slot.GetStrategicIntentTargetBase(),
			slot.GetStrategicIntentTargetPosition());
		details += string.Format(
			" faction=%1 slot=%2 stable_slot=%3 authority=%4 intent_revision=%5 reason=%6",
			faction.GetFactionKey(),
			slot.GetSlotId(),
			slot.GetStableSlotKey(),
			AICF_StrategicDecisionAuthority.ToString(
				slot.GetStrategicIntentAuthority()),
			slot.GetStrategicIntentRevision(),
			reason);
		AICF_Stage1Diagnostics.Info("STRATEGIC_INTENT_INVALIDATED", details);
		slot.ClearStrategicIntent();
	}

	protected void LogCommandWaiting(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction)
	{
		if (!slot || !faction || !slot.MarkCommandWaitingReported())
			return;

		AICF_Stage1Diagnostics.Info(
			"COMMAND_WAITING",
			string.Format(
				"faction=%1 reason=NO_PLAYER_ORDER slot=%2 stable_slot=%3 numeric_slot=%4 hold_target=%5 decision_authority=SYSTEM_HOLD group_generation=%6 assignment_revision=%7",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetStableSlotKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
				slot.GetSpawnGeneration(),
				slot.GetStrategicAssignmentRevision()));
	}

	// Re-evaluates forward defense/QRF posture without rebuilding a stable attack
	// waypoint. QRF escalation is immediate; a new forward position or the return
	// from QRF must remain stable for one commander interval and respect the
	// assignment's two-interval minimum dwell.
	bool ReconcileStrategicOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		string reason,
		int minimumDwellMs,
		int stableCandidateMs,
		bool waypointSuspendedByVehicle = false)
	{
		if (slot && slot.IsRecruitingInfantry())
			return true;
		if (!Replication.IsServer() || !slot || !faction || !graph ||
			!targetSelector || !slot.IsCombatReady())
			return false;

		if (!m_AuthorityPolicy || !m_AuthorityPolicy.IsValid())
			return false;
		if (waypointSuspendedByVehicle)
		{
			return AssignOrder(
				slot,
				faction,
				graph,
				targetSelector,
				reason,
				null,
				true);
		}
		if (GetOrderFailureReason(slot, faction).IsEmpty())
		{
			slot.ClearStrategicCandidate();
			return false;
		}

		// This compatibility boundary may repair only the already-committed
		// strategic intent. New target selection belongs to AICF_AICommander.
		return AssignOrder(
			slot,
			faction,
			graph,
			targetSelector,
			reason);
	}

	bool ReconcileAICommanderOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		string reason,
		int minimumDwellMs,
		int stableCandidateMs,
		bool waypointSuspendedByVehicle = false)
	{
		if (slot && slot.IsRecruitingInfantry())
			return true;
		if (!Replication.IsServer() || !slot || !faction || !graph ||
			!targetSelector || !slot.IsCombatReady() ||
			!m_AuthorityPolicy || !m_AuthorityPolicy.IsValid() ||
			!m_AuthorityPolicy.IsAICommanderEnabled(faction.GetFactionKey()))
		{
			return false;
		}

		if (slot.HasPlayerStrategicIntent())
		{
			if (!slot.IsPlayerStrategicIntentRoleCurrent() ||
				(slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.BASE &&
				!IsTargetValidForRole(
					slot,
					faction,
					slot.GetPlayerStrategicIntentTargetBase())) ||
				(slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.POSITION &&
				!IsStrategicIntentDestinationValid(slot, faction)))
			{
				InvalidatePlayerStrategicIntent(slot, faction, reason);
			}
			else if (slot.HasPlayerStrategicOrder() &&
				GetOrderFailureReason(slot, faction).IsEmpty())
			{
				slot.ClearStrategicCandidate();
				return false;
			}
			else
			{
				if (waypointSuspendedByVehicle)
				{
					if (slot.GetPlayerStrategicIntentTargetKind() ==
						AICF_EOrderTargetKind.POSITION)
					{
						return ApplySuspendedPointAssignment(
							slot,
							faction,
							slot.GetPlayerStrategicIntentTargetPosition(),
							AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
							reason,
							"PLAYER_INTENT_RESTORE",
							false);
					}
					return ApplySuspendedStrategicAssignment(
						slot,
						faction,
						slot.GetPlayerStrategicIntentTargetBase(),
						ResolvePlayerPosture(slot),
						AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
						reason,
						"PLAYER_INTENT_RESTORE",
						false);
				}
				return RestorePlayerStrategicIntent(slot, faction, reason);
			}
		}
		if (slot.HasPlayerStrategicOrder())
		{
			slot.ClearStrategicCandidate();
			return false;
		}

		string desiredPosture;
		string trigger;
		SCR_CampaignMilitaryBaseComponent desiredTarget = SelectOperationalTarget(
			slot,
			faction,
			graph,
			targetSelector,
			null,
			desiredPosture,
			trigger);
		if (!desiredTarget)
			return false;

		SCR_CampaignMilitaryBaseComponent currentTarget = slot.GetTargetBase();
		string currentPosture = slot.GetOperationalPosture();
		if (!IsTargetValidForRole(slot, faction, currentTarget))
		{
			if (waypointSuspendedByVehicle)
			{
				return ApplySuspendedStrategicAssignment(
					slot,
					faction,
					desiredTarget,
					desiredPosture,
					AICF_EStrategicDecisionAuthority.AI_COMMANDER,
					reason,
					trigger);
			}
			return ReplaceOrder(
				slot,
				faction,
				desiredTarget,
				reason,
				desiredPosture,
				trigger,
				false,
				AICF_EStrategicDecisionAuthority.AI_COMMANDER);
		}
		// Ranked ATTACK selection is deterministic at assignment time. Preserve a
		// still-valid target so graph churn cannot make all three groups rotate every
		// commander tick merely because another slot completed an objective.
		if (slot.GetRole() == AICF_EGroupRole.ATTACK)
			return false;

		if (currentTarget == desiredTarget && currentPosture == desiredPosture)
		{
			slot.ClearStrategicCandidate();
			return false;
		}

		bool urgentQRF = desiredPosture == POSTURE_QRF && currentPosture != POSTURE_QRF;
		if (!urgentQRF)
		{
			bool candidateReady = slot.IsStrategicCandidateReady(
				desiredTarget,
				desiredPosture,
				minimumDwellMs,
				stableCandidateMs);
			if (!candidateReady)
			{
				int assignmentAgeMs = slot.GetStrategicAssignmentAgeMs();
				int candidateAgeMs = slot.GetStrategicCandidateAgeMs();
				int remainingDwellMs = Math.Max(0, minimumDwellMs - assignmentAgeMs);
				int remainingStableMs = Math.Max(0, stableCandidateMs - candidateAgeMs);
				string heldCandidateLine = string.Format(
					"faction=%1 slot=%2 numeric_slot=%3 current_posture=%4 desired_posture=%5 current_target=%6 desired_target=%7 trigger=%8 assignment_age_ms=%9",
					faction.GetFactionKey(),
					slot.GetSlotKey(),
					slot.GetSlotId(),
					currentPosture,
					desiredPosture,
					AICF_Stage1Diagnostics.BaseKey(currentTarget),
					AICF_Stage1Diagnostics.BaseKey(desiredTarget),
					trigger,
					assignmentAgeMs);
				heldCandidateLine += string.Format(
					" candidate_age_ms=%1 remaining_dwell_ms=%2 remaining_stable_ms=%3",
					candidateAgeMs,
					remainingDwellMs,
					remainingStableMs);
				AICF_Stage35Diagnostics.Info("STRATEGIC_CANDIDATE_HELD", heldCandidateLine);
				return false;
			}
		}

		if (currentTarget == desiredTarget)
		{
			int previousDwellMs = slot.GetStrategicAssignmentAgeMs();
			slot.SetDecisionAuthority(
				AICF_EStrategicDecisionAuthority.AI_COMMANDER);
			slot.CommitStrategicIntent(
				desiredTarget,
				desiredPosture,
				AICF_EStrategicDecisionAuthority.AI_COMMANDER);
			slot.RecordStrategicAssignment(desiredTarget, desiredPosture);
			AICF_Stage35Diagnostics.Info(
				"DEFEND_POSTURE_CHANGED",
				string.Format(
					"faction=%1 slot=%2 numeric_slot=%3 old_posture=%4 new_posture=%5 target=%6 trigger=%7 dwell_ms=%8 minimum_dwell_ms=%9 waypoint_replaced=0",
					faction.GetFactionKey(),
					slot.GetSlotKey(),
					slot.GetSlotId(),
					currentPosture,
					desiredPosture,
					AICF_Stage1Diagnostics.BaseKey(desiredTarget),
					trigger,
					previousDwellMs,
					minimumDwellMs));
			return true;
		}

		if (waypointSuspendedByVehicle)
		{
			return ApplySuspendedStrategicAssignment(
				slot,
				faction,
				desiredTarget,
				desiredPosture,
				AICF_EStrategicDecisionAuthority.AI_COMMANDER,
				reason,
				trigger);
		}

		return ReplaceOrder(
			slot,
			faction,
			desiredTarget,
			reason,
			desiredPosture,
			trigger,
			false,
			AICF_EStrategicDecisionAuthority.AI_COMMANDER);
	}

	bool AssignLossResponseOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		SCR_CampaignMilitaryBaseComponent lostBase,
		int minimumDwellMs,
		int stableCandidateMs,
		bool waypointSuspendedByVehicle = false)
	{
		if (slot && slot.IsRecruitingInfantry())
			return true;
		if (!Replication.IsServer() || !slot ||
			slot.GetRole() != AICF_EGroupRole.DEFEND || !faction ||
			!graph || !targetSelector || !lostBase || !slot.IsCombatReady())
		{
			return false;
		}

		if (!m_AuthorityPolicy || !m_AuthorityPolicy.IsValid())
			return false;
		if (GetOrderFailureReason(slot, faction).IsEmpty())
		{
			return false;
		}

		// Loss notifications outside the faction commander may restore or hold,
		// but never choose a response objective themselves.
		return AssignOrder(
			slot,
			faction,
			graph,
			targetSelector,
			"BASE_OWNER_CHANGED_AUTHORITY_BOUNDARY",
			lostBase,
			waypointSuspendedByVehicle);
	}

	bool AssignAICommanderLossResponseOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		SCR_CampaignMilitaryBaseComponent lostBase,
		int minimumDwellMs,
		int stableCandidateMs,
		bool waypointSuspendedByVehicle = false)
	{
		if (slot && slot.IsRecruitingInfantry())
			return true;
		if (!Replication.IsServer() || !slot ||
			slot.GetRole() != AICF_EGroupRole.DEFEND || !faction ||
			!graph || !targetSelector || !lostBase || !slot.IsCombatReady() ||
			!m_AuthorityPolicy || !m_AuthorityPolicy.IsValid() ||
			!m_AuthorityPolicy.IsAICommanderEnabled(faction.GetFactionKey()))
		{
			return false;
		}

		if (slot.HasPlayerStrategicIntent())
		{
			if (!slot.IsPlayerStrategicIntentRoleCurrent() ||
				(slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.BASE &&
				!IsTargetValidForRole(
					slot,
					faction,
					slot.GetPlayerStrategicIntentTargetBase())) ||
				(slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.POSITION &&
				!IsStrategicIntentDestinationValid(slot, faction)))
			{
				InvalidatePlayerStrategicIntent(
					slot,
					faction,
					"BASE_OWNER_CHANGED_QRF");
			}
			else if (slot.HasPlayerStrategicOrder() &&
				GetOrderFailureReason(slot, faction).IsEmpty())
			{
				return false;
			}
			else
			{
				if (waypointSuspendedByVehicle)
				{
					if (slot.GetPlayerStrategicIntentTargetKind() ==
						AICF_EOrderTargetKind.POSITION)
					{
						return ApplySuspendedPointAssignment(
							slot,
							faction,
							slot.GetPlayerStrategicIntentTargetPosition(),
							AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
							"BASE_OWNER_CHANGED_QRF",
							"PLAYER_INTENT_RESTORE",
							false);
					}
					return ApplySuspendedStrategicAssignment(
						slot,
						faction,
						slot.GetPlayerStrategicIntentTargetBase(),
						ResolvePlayerPosture(slot),
						AICF_EStrategicDecisionAuthority.PLAYER_COMMAND,
						"BASE_OWNER_CHANGED_QRF",
						"PLAYER_INTENT_RESTORE",
						false);
				}
				return RestorePlayerStrategicIntent(
					slot,
					faction,
					"BASE_OWNER_CHANGED_QRF");
			}
		}
		if (slot.HasPlayerStrategicOrder())
			return false;

		string selectedPosture;
		string responseTrigger;
		SCR_CampaignMilitaryBaseComponent responseTarget = targetSelector.SelectDefendTarget(
			graph,
			faction,
			selectedPosture,
			responseTrigger);
		if (!responseTarget || selectedPosture != POSTURE_QRF)
		{
			responseTrigger = "NEIGHBOR_LOST";
			responseTarget = targetSelector.SelectLossResponseTarget(graph, faction, lostBase);
		}
		if (!responseTarget)
			return false;

		string oldPosture = slot.GetOperationalPosture();
		if (slot.GetTargetBase() == responseTarget &&
			IsTargetValidForRole(slot, faction, responseTarget))
		{
			if (oldPosture == POSTURE_QRF)
			{
				slot.ClearStrategicCandidate();
				AICF_Stage35Diagnostics.Info(
					"STRATEGIC_CANDIDATE_HELD",
					string.Format(
						"faction=%1 slot=%2 numeric_slot=%3 current_posture=QRF desired_posture=QRF current_target=%4 desired_target=%4 trigger=%5 lost_base=%6 assignment_age_ms=%7 candidate_age_ms=0 remaining_dwell_ms=0 remaining_stable_ms=0 decision=KEEP_CURRENT_QRF",
						faction.GetFactionKey(),
						slot.GetSlotKey(),
						slot.GetSlotId(),
						AICF_Stage1Diagnostics.BaseKey(responseTarget),
						responseTrigger,
						AICF_Stage1Diagnostics.BaseKey(lostBase),
						slot.GetStrategicAssignmentAgeMs()));
				return false;
			}

			int dwellMs = slot.GetStrategicAssignmentAgeMs();
			slot.SetDecisionAuthority(
				AICF_EStrategicDecisionAuthority.AI_COMMANDER);
			slot.CommitStrategicIntent(
				responseTarget,
				POSTURE_QRF,
				AICF_EStrategicDecisionAuthority.AI_COMMANDER);
			slot.RecordStrategicAssignment(responseTarget, POSTURE_QRF);
			AICF_Stage35Diagnostics.Info(
				"DEFEND_POSTURE_CHANGED",
				string.Format(
					"faction=%1 slot=%2 numeric_slot=%3 old_posture=%4 new_posture=%5 target=%6 trigger=%7 lost_base=%8 dwell_ms=%9 waypoint_replaced=0",
					faction.GetFactionKey(),
					slot.GetSlotKey(),
					slot.GetSlotId(),
					oldPosture,
					POSTURE_QRF,
					AICF_Stage1Diagnostics.BaseKey(responseTarget),
					responseTrigger,
					AICF_Stage1Diagnostics.BaseKey(lostBase),
					dwellMs));
			return true;
		}

		bool immediateHQEscalation = responseTrigger == "HQ_THREAT";
		if (oldPosture == POSTURE_QRF && !immediateHQEscalation &&
			!slot.IsStrategicCandidateReady(
				responseTarget,
				POSTURE_QRF,
				minimumDwellMs,
				stableCandidateMs))
		{
			int assignmentAgeMs = slot.GetStrategicAssignmentAgeMs();
			int candidateAgeMs = slot.GetStrategicCandidateAgeMs();
			string heldLossCandidateLine = string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 current_posture=QRF desired_posture=QRF current_target=%4 desired_target=%5 trigger=%6 lost_base=%7 assignment_age_ms=%8 candidate_age_ms=%9",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
				AICF_Stage1Diagnostics.BaseKey(responseTarget),
				responseTrigger,
				AICF_Stage1Diagnostics.BaseKey(lostBase),
				assignmentAgeMs,
				candidateAgeMs);
			heldLossCandidateLine += string.Format(
				" remaining_dwell_ms=%1 remaining_stable_ms=%2 decision=HYSTERESIS",
				Math.Max(0, minimumDwellMs - assignmentAgeMs),
				Math.Max(0, stableCandidateMs - candidateAgeMs));
			AICF_Stage35Diagnostics.Info("STRATEGIC_CANDIDATE_HELD", heldLossCandidateLine);
			return false;
		}

		if (waypointSuspendedByVehicle)
		{
			return ApplySuspendedStrategicAssignment(
				slot,
				faction,
				responseTarget,
				POSTURE_QRF,
				AICF_EStrategicDecisionAuthority.AI_COMMANDER,
				string.Format("%1_QRF", responseTrigger),
				responseTrigger);
		}

		return ReplaceOrder(
			slot,
			faction,
			responseTarget,
			string.Format("%1_QRF", responseTrigger),
			POSTURE_QRF,
			responseTrigger,
			false,
			AICF_EStrategicDecisionAuthority.AI_COMMANDER);
	}

	bool IsOrderValid(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		return GetOrderFailureReason(slot, faction).IsEmpty();
	}

	bool IsStrategicTargetValid(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target)
	{
		return IsTargetValidForRole(slot, faction, target);
	}

	bool IsCurrentStrategicDestinationValid(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction)
	{
		return IsCurrentTargetValid(slot, faction);
	}

	string GetOrderFailureReason(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		if (!slot || !faction)
			return "INPUT_INVALID";
		if (!slot.IsCombatReady() || !slot.GetGroup())
			return "GROUP_NOT_READY";
		// Strategic ownership/eligibility changes supersede waypoint lifecycle.
		// A completed waypoint on a freshly captured target is a retarget event,
		// not an unstable movement recovery that should consume stuck budget.
		if (!IsCurrentTargetValid(slot, faction))
			return "TARGET_INVALID";
		if (!slot.GetWaypoint())
			return "WAYPOINT_REFERENCE_MISSING";
		if (slot.GetGroup().GetCurrentWaypoint() != slot.GetWaypoint())
			return "WAYPOINT_NOT_CURRENT";
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION &&
			!SCR_DefendWaypoint.Cast(slot.GetWaypoint()))
		{
			return "WAYPOINT_TYPE_INVALID";
		}
		if (slot.IsSystemHoldOrder() &&
			!SCR_DefendWaypoint.Cast(slot.GetWaypoint()))
		{
			return "WAYPOINT_TYPE_INVALID";
		}
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.BASE &&
			slot.GetRole() == AICF_EGroupRole.ATTACK &&
			slot.GetTargetBase().GetType() == SCR_ECampaignBaseType.RELAY &&
			!SCR_SmartActionWaypoint.Cast(slot.GetWaypoint()))
		{
			return "WAYPOINT_TYPE_INVALID";
		}

		return string.Empty;
	}

	bool RecoverOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		string failureReason,
		bool countsAsStuckRecovery = false)
	{
		if (slot && slot.IsRecruitingInfantry())
			return true;
		if (!slot || !faction || !graph || !targetSelector || !slot.IsCombatReady())
			return false;

		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		bool recovered;
		AICF_EStrategicDecisionAuthority decisionAuthority =
			slot.GetDecisionAuthority();
		if (IsCurrentTargetValid(slot, faction))
		{
			if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION)
				recovered = ReplacePointOrder(
					slot,
					faction,
					slot.GetTargetPosition(),
					"ORDER_RECOVERY",
					"ORDER_RECOVERY",
					decisionAuthority,
					false);
			else
				recovered = ReplaceOrder(
					slot,
					faction,
					oldTarget,
					"ORDER_RECOVERY",
					slot.GetOperationalPosture(),
					"ORDER_RECOVERY",
					false,
					decisionAuthority,
					slot.IsSystemHoldOrder(),
					false);
		}
		else
			recovered = AssignOrder(slot, faction, graph, targetSelector, "ORDER_RECOVERY", oldTarget);

		if (recovered)
		{
			slot.BeginOrderRecoveryVerification(failureReason, countsAsStuckRecovery);
			AICF_Stage2Diagnostics.Info(
				"ORDER_RECOVERY_ISSUED",
				string.Format(
					"faction=%1 slot=%2 role=%3 cause=%4 target=%5 waypoint=%6 counts_as_stuck=%7 verification=PENDING_DURABILITY",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					failureReason,
					DescribeDestination(slot),
					slot.GetWaypoint().GetID(),
					countsAsStuckRecovery));
		}
		else
		{
			AICF_Stage2Diagnostics.Warning(
				"ORDER_RECOVERY_DEFERRED",
				string.Format(
					"faction=%1 slot=%2 cause=%3",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					failureReason));
		}

		return recovered;
	}

	bool RebuildCurrentOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason)
	{
		if (slot && slot.IsRecruitingInfantry())
			return true;
		if (!slot || !faction || !slot.IsCombatReady() ||
			!IsCurrentTargetValid(slot, faction))
		{
			return false;
		}

		if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION)
		{
			return ReplacePointOrder(
				slot,
				faction,
				slot.GetTargetPosition(),
				reason,
				"ORDER_REBUILD",
				slot.GetDecisionAuthority(),
				false);
		}

		return ReplaceOrder(
			slot,
			faction,
			slot.GetTargetBase(),
			reason,
			slot.GetOperationalPosture(),
			"ORDER_REBUILD",
			false,
			slot.GetDecisionAuthority(),
			slot.IsSystemHoldOrder(),
			false);
	}

	bool AssignLoneSurvivorRetreat(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason)
	{
		if (!slot || !faction || !slot.IsCombatReady() ||
			AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) != 1)
		{
			return false;
		}
		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		if (!mainBase || !mainBase.GetOwner() || mainBase.GetFaction() != faction)
			return false;
		return ReplaceOrder(
			slot,
			faction,
			mainBase,
			reason,
			"LONE_SURVIVOR_RETREAT",
			"VEHICLE_FALLBACK_LONE_SURVIVOR",
			true,
			slot.GetDecisionAuthority(),
			false,
			false);
	}

	// Non-relay ATTACK assignments travel under a stock Move waypoint. The
	// objective action is deliberately created only after the alive leader enters
	// the local objective envelope, so long operational movement cannot start the
	// stock search grid (and its holding clock) at assignment time.
	//
	// Returns true when the local S&D action is current, including an idempotent
	// call for an action that this planner has already promoted.
	bool PromoteAttackToObjectiveAction(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		string reason)
	{
		if (!Replication.IsServer() || !slot || !faction || !slot.IsCombatReady() ||
			slot.GetRole() != AICF_EGroupRole.ATTACK ||
			slot.GetTargetKind() != AICF_EOrderTargetKind.BASE)
		{
			return false;
		}

		SCR_AIGroup group = slot.GetGroup();
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		AIWaypoint operationalWaypoint = slot.GetWaypoint();
		if (!group || !operationalWaypoint || !IsTargetValidForRole(slot, faction, target) ||
			target.GetType() == SCR_ECampaignBaseType.RELAY)
		{
			return false;
		}

		SCR_SearchAndDestroyWaypoint activeObjectiveAction =
			SCR_SearchAndDestroyWaypoint.Cast(operationalWaypoint);
		if (activeObjectiveAction)
			return group.GetCurrentWaypoint() == operationalWaypoint;

		// The only promotable ATTACK order is the non-timed stock Move prefab.
		// This prevents a field-hold/relay/foreign prefab from being silently
		// rewritten into an objective action.
		if (SCR_TimedWaypoint.Cast(operationalWaypoint) ||
			SCR_SmartActionWaypoint.Cast(operationalWaypoint))
		{
			return false;
		}

		vector targetPosition;
		if (!TryResolveTargetPosition(target, AICF_EGroupRole.ATTACK, targetPosition))
			return false;

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!leader)
			return false;

		float targetDistanceMeters = Math.Sqrt(vector.DistanceSqXZ(
			leader.GetOrigin(),
			targetPosition));
		if (targetDistanceMeters > ATTACK_OBJECTIVE_PROMOTION_RADIUS_METERS)
			return false;

		SCR_SearchAndDestroyWaypoint objectiveAction = CreateAttackObjectiveAction(target);
		if (!objectiveAction)
			return false;

		AICF_Stage35Diagnostics.Info(
			"ATTACK_OBJECTIVE_ACTION_CREATED",
			string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 target=%5 waypoint=%6 waypoint_kind=ATTACK_OBJECTIVE_ACTION distance_m=%7 completion_radius_m=%8 holding_time_s=%9",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				AICF_Stage1Diagnostics.BaseKey(target),
				objectiveAction.GetID(),
				targetDistanceMeters,
				ATTACK_OBJECTIVE_ACTION_RADIUS_METERS,
				ATTACK_OBJECTIVE_HOLDING_TIME_SECONDS));

		// Add first and commit the slot reference before deleting the old Move.
		// If the slot rejects the assignment, removing the candidate restores the
		// original operational queue without a taskless interval.
		group.AddWaypointAt(objectiveAction, 0);
		if (!slot.AssignObjective(target, objectiveAction))
		{
			group.RemoveWaypoint(objectiveAction);
			RplComponent.DeleteRplEntity(objectiveAction, false);
			AICF_Stage35Diagnostics.Warning(
				"ATTACK_OBJECTIVE_PROMOTION_REJECTED",
				string.Format(
					"faction=%1 slot=%2 numeric_slot=%3 target=%4 old_waypoint=%5 reason=SLOT_ASSIGNMENT_REJECTED",
					faction.GetFactionKey(),
					slot.GetSlotKey(),
					slot.GetSlotId(),
					AICF_Stage1Diagnostics.BaseKey(target),
					operationalWaypoint.GetID()));
			return false;
		}

		EntityID operationalWaypointId = operationalWaypoint.GetID();
		group.RemoveWaypoint(operationalWaypoint);
		RplComponent.DeleteRplEntity(operationalWaypoint, false);
		LogWaypointRemoved(
			slot,
			operationalWaypoint,
			"ATTACK_OBJECTIVE_PROMOTION",
			reason);

		string promotionLine = string.Format(
			"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 target=%5 old_waypoint=%6 old_waypoint_kind=ATTACK_OPERATIONAL_MOVE new_waypoint=%7 new_waypoint_kind=ATTACK_OBJECTIVE_ACTION",
			faction.GetFactionKey(),
			slot.GetSlotKey(),
			slot.GetSlotId(),
			slot.GetSpawnGeneration(),
			AICF_Stage1Diagnostics.BaseKey(target),
			operationalWaypointId,
			objectiveAction.GetID());
		promotionLine += string.Format(
			" reason=%1 distance_m=%2 promotion_radius_m=%3 holding_time_s=%4",
			reason,
			targetDistanceMeters,
			ATTACK_OBJECTIVE_PROMOTION_RADIUS_METERS,
			ATTACK_OBJECTIVE_HOLDING_TIME_SECONDS);
		AICF_Stage35Diagnostics.Info(
			"ATTACK_OBJECTIVE_ACTION_PROMOTED",
			promotionLine);
		return true;
	}

	// Persistent movement failure is a local navigation failure, not a casualty.
	// Keep the same group, target and world position under a durable defend
	// waypoint. A later strategic graph revision may resume the objective from
	// this field position without spending tickets or spawning at a MOB.
	bool HoldPositionForPersistentStuck(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target,
		vector fieldPosition)
	{
		if (!slot || !faction || !slot.HasStrategicDestination() ||
			!slot.IsCombatReady() || !slot.GetGroup())
			return false;
		AICF_EOrderTargetKind targetKind = slot.GetTargetKind();
		vector targetPosition = slot.GetTargetPosition();

		Resource waypointResource = Resource.Load(DEFEND_WAYPOINT_PREFAB);
		if (!waypointResource || !waypointResource.IsValid())
		{
			AICF_Stage1Diagnostics.Error("WAYPOINT_PREFAB_INVALID", DEFEND_WAYPOINT_PREFAB);
			return false;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = fieldPosition;
		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(
			DEFEND_WAYPOINT_PREFAB,
			false,
			params: spawnParams);
		AIWaypoint fieldHold = AIWaypoint.Cast(spawnedEntity);
		if (!fieldHold)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);
			return false;
		}
		fieldHold.SetCompletionRadius(DEFEND_RADIUS_METERS);

		SCR_AIGroup group = slot.GetGroup();
		AIWaypoint oldWaypoint = slot.GetWaypoint();
		if (oldWaypoint)
		{
			group.RemoveWaypoint(oldWaypoint);
			RplComponent.DeleteRplEntity(oldWaypoint, false);
			LogWaypointRemoved(slot, oldWaypoint, "PERSISTENT_STUCK_HOLD", "ORDER_REPLACED");
		}

		group.AddWaypointAt(fieldHold, 0);
		slot.ClearObjective();
		bool assigned;
		if (targetKind == AICF_EOrderTargetKind.POSITION)
			assigned = slot.AssignPointObjective(targetPosition, fieldHold);
		else
			assigned = slot.AssignObjective(target, fieldHold);
		if (!assigned)
		{
			group.RemoveWaypoint(fieldHold);
			RplComponent.DeleteRplEntity(fieldHold, false);
			return false;
		}

		slot.BeginPersistentStuckFieldHold(fieldPosition);
		return true;
	}

	void ClearOrder(AICF_GroupSlot slot)
	{
		if (!slot)
			return;

		SCR_AIGroup group = slot.GetGroup();
		AIWaypoint waypoint = slot.GetWaypoint();
		if (waypoint)
		{
			if (group)
				group.RemoveWaypoint(waypoint);

			RplComponent.DeleteRplEntity(waypoint, false);
			LogWaypointRemoved(slot, waypoint, "CLEAR_ORDER", "STRATEGIC_ORDER_CLEARED");
		}

		slot.ClearObjective();
	}

	void SuspendOrderForVehicle(AICF_GroupSlot slot)
	{
		if (!slot)
			return;

		SCR_AIGroup group = slot.GetGroup();
		AIWaypoint waypoint = slot.GetWaypoint();
		if (waypoint)
		{
			if (group)
				group.RemoveWaypoint(waypoint);

			RplComponent.DeleteRplEntity(waypoint, false);
			LogWaypointRemoved(slot, waypoint, "SUSPEND_FOR_VEHICLE", "VEHICLE_CONTROL_ACQUIRED");
		}

		slot.SuspendObjectiveWaypoint();
	}

	bool TryResolveTargetPosition(
		SCR_CampaignMilitaryBaseComponent target,
		AICF_EGroupRole role,
		out vector targetPosition)
	{
		targetPosition = "0 0 0";
		if (!target || !target.GetOwner())
			return false;

		bool isRelay = role == AICF_EGroupRole.ATTACK && target.GetType() == SCR_ECampaignBaseType.RELAY;
		if (isRelay)
		{
			targetPosition = target.GetOwner().GetOrigin();
			return true;
		}

		if (role == AICF_EGroupRole.ATTACK)
		{
			array<SCR_SeizingComponent> capturePoints = {};
			target.GetCapturePoints(capturePoints);
			if (!capturePoints.IsEmpty() && capturePoints[0] && capturePoints[0].GetOwner())
				targetPosition = capturePoints[0].GetOwner().GetOrigin();
			else
				targetPosition = target.GetOwner().GetOrigin();
			return true;
		}

		SCR_SpawnPoint spawnPoint = target.GetSpawnPoint();
		if (!spawnPoint)
			return false;

		vector targetRotation;
		spawnPoint.GetPositionAndRotation(targetPosition, targetRotation);
		return true;
	}

	// The client supplies only X/Z intent. Authority reconstructs terrain Y and
	// accepts only a nearby navmesh endpoint inside the current world bounds.
	bool TryResolvePlayerPointTarget(
		AICF_GroupSlot slot,
		vector clientPosition,
		out vector resolvedPosition,
		out string rejectionReason)
	{
		resolvedPosition = vector.Zero;
		rejectionReason = string.Empty;
		if (!Replication.IsServer() || !slot || !slot.IsCombatReady() || !slot.GetGroup())
		{
			rejectionReason = "GROUP_NOT_READY";
			return false;
		}
		if (!IsFiniteCoordinate(clientPosition[0]) ||
			!IsFiniteCoordinate(clientPosition[2]))
		{
			rejectionReason = "COORDINATE_NOT_FINITE";
			return false;
		}

		BaseWorld world = GetGame().GetWorld();
		GenericWorldEntity worldEntity = GetGame().GetWorldEntity();
		GenericTerrainEntity terrain;
		if (worldEntity)
			terrain = worldEntity.GetTerrain(0, 0);
		if (!world || !worldEntity || !terrain)
		{
			rejectionReason = "WORLD_UNAVAILABLE";
			return false;
		}
		vector boundsMin;
		vector boundsMax;
		terrain.GetTerrainBoundBox(boundsMin, boundsMax);
		if (clientPosition[0] < boundsMin[0] || clientPosition[0] > boundsMax[0] ||
			clientPosition[2] < boundsMin[2] || clientPosition[2] > boundsMax[2])
		{
			rejectionReason = "OUTSIDE_WORLD_BOUNDS";
			return false;
		}

		vector terrainPosition = clientPosition;
		terrainPosition[1] = world.GetSurfaceY(clientPosition[0], clientPosition[2]);
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(
			slot.GetGroup().FindComponent(AIPathfindingComponent));
		if (!pathfinding)
		{
			rejectionReason = "PATHFINDING_UNAVAILABLE";
			return false;
		}
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		if (!navmesh)
		{
			rejectionReason = "NAVMESH_UNAVAILABLE";
			return false;
		}
		if (!navmesh.IsTileLoaded(terrainPosition))
		{
			if (!navmesh.IsTileRequested(terrainPosition) &&
				!navmesh.LoadTileIn(terrainPosition))
			{
				rejectionReason = "NAVMESH_TILE_UNAVAILABLE";
				return false;
			}
			rejectionReason = "NAVMESH_TILE_LOADING";
			return false;
		}
		vector navmeshPosition;
		if (!pathfinding.GetClosestPositionOnNavmesh(
			terrainPosition,
			PLAYER_POINT_NAVMESH_SEARCH_HALF_EXTENTS,
			navmeshPosition))
		{
			rejectionReason = "NO_NAVMESH_ENDPOINT_NEARBY";
			return false;
		}
		if (!IsFiniteCoordinate(navmeshPosition[0]) ||
			!IsFiniteCoordinate(navmeshPosition[1]) ||
			!IsFiniteCoordinate(navmeshPosition[2]) ||
			navmeshPosition[0] < boundsMin[0] || navmeshPosition[0] > boundsMax[0] ||
			navmeshPosition[2] < boundsMin[2] || navmeshPosition[2] > boundsMax[2] ||
			vector.DistanceXZ(terrainPosition, navmeshPosition) >
				PLAYER_POINT_MAX_NAVMESH_OFFSET_METERS)
		{
			rejectionReason = "NAVMESH_ENDPOINT_OUT_OF_RANGE";
			return false;
		}

		resolvedPosition = navmeshPosition;
		return true;
	}

	protected bool IsFiniteCoordinate(float value)
	{
		return value == value && Math.AbsFloat(value) < float.MAX;
	}

	bool TryResolveSlotTargetPosition(
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent target,
		out vector targetPosition)
	{
		if (!slot)
		{
			targetPosition = vector.Zero;
			return false;
		}
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION)
		{
			targetPosition = slot.GetTargetPosition();
			return true;
		}
		AICF_EGroupRole positionRole = slot.GetRole();
		if (slot.IsLoneSurvivorRetreat() || slot.IsSystemHoldOrder())
			positionRole = AICF_EGroupRole.DEFEND;
		return TryResolveTargetPosition(target, positionRole, targetPosition);
	}

	bool HoldPositionForTemporaryRouteReplan(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target,
		vector fieldPosition)
	{
		if (!slot || !faction || !target || !slot.IsCombatReady() || !slot.GetGroup())
			return false;
		Resource waypointResource = Resource.Load(DEFEND_WAYPOINT_PREFAB);
		if (!waypointResource || !waypointResource.IsValid())
			return false;
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = fieldPosition;
		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(
			DEFEND_WAYPOINT_PREFAB,
			false,
			params: spawnParams);
		AIWaypoint routeHold = AIWaypoint.Cast(spawnedEntity);
		if (!routeHold)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);
			return false;
		}
		routeHold.SetCompletionRadius(DEFEND_RADIUS_METERS);
		routeHold.SetCompletionType(EAIWaypointCompletionType.All);
		SCR_TimedWaypoint timedHold = SCR_TimedWaypoint.Cast(routeHold);
		if (timedHold)
			timedHold.SetHoldingTime(DEFEND_HOLDING_TIME_SECONDS);

		SCR_AIGroup group = slot.GetGroup();
		AIWaypoint oldWaypoint = slot.GetWaypoint();
		if (oldWaypoint)
		{
			group.RemoveWaypoint(oldWaypoint);
			RplComponent.DeleteRplEntity(oldWaypoint, false);
			LogWaypointRemoved(slot, oldWaypoint, "FALSE_COMPLETION_HOLD", "TEMPORARY_ROUTE_REPLAN_HOLD");
		}
		group.AddWaypointAt(routeHold, 0);
		slot.ClearObjective();
		if (!slot.AssignObjective(target, routeHold))
		{
			group.RemoveWaypoint(routeHold);
			RplComponent.DeleteRplEntity(routeHold, false);
			return false;
		}
		slot.BeginTemporaryRouteReplanHold(fieldPosition);
		return true;
	}

	// Planning is the sole writer of strategic intent. Vehicle orchestration
	// receives this point-in-time value object and may only validate or retarget
	// through a later snapshot with a newer planning/base revision.
	bool TryCreateAssignmentSnapshot(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		int baseRevision,
		out AICF_StrategicAssignmentSnapshot snapshot)
	{
		snapshot = null;
		if (!slot || !faction || !slot.IsCombatReady() ||
			!slot.HasStrategicDestination() ||
			slot.IsSystemHoldOrder() || slot.IsAwaitingPlayerCommand())
			return false;

		vector targetPosition;
		if (!TryResolveSlotTargetPosition(slot, slot.GetTargetBase(), targetPosition))
			return false;

		int assignmentStartedAtMs;
		int assignmentAgeMs = slot.GetStrategicAssignmentAgeMs();
		if (assignmentAgeMs > 0)
			assignmentStartedAtMs = Math.Max(1, System.GetTickCount() - assignmentAgeMs);

		snapshot = new AICF_StrategicAssignmentSnapshot(
			faction.GetFactionKey(),
			slot.GetSlotId(),
			slot.GetSlotKey(),
			slot.GetSpawnGeneration(),
			slot.GetGroup(),
			slot.GetRole(),
			slot.GetUnitType(),
			slot.GetOperationalPosture(),
			slot.GetTargetKind(),
			slot.GetTargetBase(),
			targetPosition,
			slot.GetStrategicAssignmentRevision(),
			slot.GetStrategicIntentRevision(),
			Math.Max(0, baseRevision),
			slot.GetWaypoint(),
			assignmentStartedAtMs);
		return snapshot.IsValid();
	}

	protected bool ReplacePointOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		vector targetPosition,
		string reason,
		string trigger,
		AICF_EStrategicDecisionAuthority decisionAuthority,
		bool updateStrategicIntent = true)
	{
		if (!slot || !faction || decisionAuthority !=
			AICF_EStrategicDecisionAuthority.PLAYER_COMMAND)
		{
			return false;
		}
		SCR_AIGroup group = slot.GetGroup();
		if (!group || !slot.IsCombatReady())
			return false;

		AIWaypoint newWaypoint = CreatePositionWaypoint(targetPosition);
		if (!newWaypoint)
			return false;
		AIWaypoint oldWaypoint = slot.GetWaypoint();
		AICF_EOrderTargetKind oldKind = slot.GetTargetKind();
		vector oldPosition = slot.GetTargetPosition();
		string oldPosture = slot.GetOperationalPosture();
		AICF_EStrategicDecisionAuthority oldAuthority = slot.GetDecisionAuthority();
		if (oldWaypoint)
		{
			group.RemoveWaypoint(oldWaypoint);
			RplComponent.DeleteRplEntity(oldWaypoint, false);
			LogWaypointRemoved(slot, oldWaypoint, trigger, reason);
		}

		group.AddWaypointAt(newWaypoint, 0);
		slot.ClearObjective();
		if (!slot.AssignPointObjective(targetPosition, newWaypoint))
		{
			group.RemoveWaypoint(newWaypoint);
			RplComponent.DeleteRplEntity(newWaypoint, false);
			return false;
		}
		slot.SetDecisionAuthority(decisionAuthority);
		slot.ClearAwaitingPlayerCommand();
		if (updateStrategicIntent)
			slot.CommitStrategicPointIntent(
				targetPosition,
				POSTURE_MOVE_AND_HOLD,
				decisionAuthority);
		slot.ResetTargetUnavailableReport();
		if (oldKind != AICF_EOrderTargetKind.POSITION ||
			oldPosition != targetPosition || oldPosture != POSTURE_MOVE_AND_HOLD ||
			oldAuthority != decisionAuthority)
		{
			slot.RecordStrategicAssignment(null, POSTURE_MOVE_AND_HOLD);
		}
		else
		{
			slot.RecordRuntimeWaypointReplacement();
		}

		string details = BuildPointAssignmentDetails(
			slot,
			faction,
			targetPosition,
			trigger,
			reason,
			newWaypoint.GetID().ToString(),
			decisionAuthority);
		AICF_Stage35Diagnostics.Info("STRATEGIC_ASSIGNMENT", details);
		AICF_Stage1Diagnostics.Info("ORDER_ASSIGNED", details);
		LogOrderWaypointCreated(slot, faction, null, newWaypoint, reason, trigger);
		return true;
	}

	protected AIWaypoint CreatePositionWaypoint(vector targetPosition)
	{
		Resource waypointResource = Resource.Load(DEFEND_WAYPOINT_PREFAB);
		if (!waypointResource || !waypointResource.IsValid())
		{
			AICF_Stage1Diagnostics.Error("WAYPOINT_PREFAB_INVALID", DEFEND_WAYPOINT_PREFAB);
			return null;
		}
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = targetPosition;
		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(
			DEFEND_WAYPOINT_PREFAB,
			false,
			params: spawnParams);
		AIWaypoint waypoint = AIWaypoint.Cast(spawnedEntity);
		SCR_TimedWaypoint timedWaypoint = SCR_TimedWaypoint.Cast(waypoint);
		if (!waypoint || !timedWaypoint)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);
			return null;
		}
		waypoint.SetCompletionType(EAIWaypointCompletionType.All);
		waypoint.SetCompletionRadius(POSITION_HOLD_RADIUS_METERS);
		timedWaypoint.SetHoldingTime(DEFEND_HOLDING_TIME_SECONDS);
		SCR_AIWaypoint scriptedWaypoint = SCR_AIWaypoint.Cast(waypoint);
		if (scriptedWaypoint)
		{
			scriptedWaypoint.AddSetting(SCR_AIGroupFormationSetting.Create(
				SCR_EAISettingOrigin.WAYPOINT,
				SCR_EAIGroupFormation.Column));
		}
		return waypoint;
	}

	protected string BuildPointAssignmentDetails(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		vector targetPosition,
		string trigger,
		string reason,
		string waypoint,
		AICF_EStrategicDecisionAuthority authority)
	{
		string details = string.Format(
			"faction=%1 slot=%2 stable_slot=%3 numeric_slot=%4 role=%5 posture=%6 target=MAP_POINT target_kind=POSITION",
			faction.GetFactionKey(),
			slot.GetSlotKey(),
			slot.GetStableSlotKey(),
			slot.GetSlotId(),
			AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
			POSTURE_MOVE_AND_HOLD);
		details += string.Format(
			" target_x=%1 target_y=%2 target_z=%3 trigger=%4 reason=%5 waypoint=%6",
			Math.Round(targetPosition[0]),
			Math.Round(targetPosition[1]),
			Math.Round(targetPosition[2]),
			trigger,
			reason,
			waypoint);
		details += string.Format(
			" group_generation=%1 assignment_revision=%2 decision_authority=%3 intent_revision=%4 authority=SERVER",
			slot.GetSpawnGeneration(),
			slot.GetStrategicAssignmentRevision(),
			AICF_StrategicDecisionAuthority.ToString(authority),
			slot.GetStrategicIntentRevision());
		return details;
	}

	protected string BuildDestinationDetails(
		AICF_EOrderTargetKind targetKind,
		SCR_CampaignMilitaryBaseComponent targetBase,
		vector targetPosition)
	{
		if (targetKind == AICF_EOrderTargetKind.POSITION)
		{
			return string.Format(
				"target=MAP_POINT target_kind=POSITION target_x=%1 target_y=%2 target_z=%3",
				Math.Round(targetPosition[0]),
				Math.Round(targetPosition[1]),
				Math.Round(targetPosition[2]));
		}
		return string.Format(
			"target=%1 target_kind=%2",
			AICF_Stage1Diagnostics.BaseKey(targetBase),
			AICF_OrderTargetKind.ToString(targetKind));
	}

	protected string DescribeDestination(AICF_GroupSlot slot)
	{
		if (!slot)
			return "NONE";
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION)
		{
			vector position = slot.GetTargetPosition();
			return string.Format(
				"MAP_POINT_%1_%2",
				Math.Round(position[0]),
				Math.Round(position[2]));
		}
		return AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase());
	}

	protected bool ReplaceOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target,
		string reason,
		string posture,
		string trigger,
		bool loneSurvivorRetreat = false,
		AICF_EStrategicDecisionAuthority decisionAuthority = AICF_EStrategicDecisionAuthority.AI_COMMANDER,
		bool systemHold = false,
		bool updateStrategicIntent = true)
	{
		SCR_AIGroup group = slot.GetGroup();
		if (!group || !slot.IsCombatReady())
			return false;

		AIWaypoint newWaypoint;
		if (loneSurvivorRetreat)
			newWaypoint = CreateLoneSurvivorRetreatWaypoint(slot.GetGroup(), target);
		else
		{
			AICF_EGroupRole waypointRole = slot.GetRole();
			int endpointRevision = slot.GetFalseCompletionEndpointRevision();
			if (systemHold)
			{
				waypointRole = AICF_EGroupRole.DEFEND;
				endpointRevision = 0;
			}
			newWaypoint = CreateWaypoint(
				target,
				waypointRole,
				slot.GetGroup(),
				endpointRevision,
				slot);
		}
		if (!newWaypoint)
			return false;

		AIWaypoint oldWaypoint = slot.GetWaypoint();
		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		string oldPosture = slot.GetOperationalPosture();
		AICF_EStrategicDecisionAuthority oldDecisionAuthority =
			slot.GetDecisionAuthority();
		if (oldWaypoint)
		{
			group.RemoveWaypoint(oldWaypoint);
			RplComponent.DeleteRplEntity(oldWaypoint, false);
			LogWaypointRemoved(slot, oldWaypoint, trigger, reason);
		}

		group.AddWaypointAt(newWaypoint, 0);
		slot.ClearObjective();
		if (!slot.AssignObjective(target, newWaypoint))
		{
			group.RemoveWaypoint(newWaypoint);
			RplComponent.DeleteRplEntity(newWaypoint, false);
			return false;
		}
		slot.SetDecisionAuthority(decisionAuthority);
		if (decisionAuthority == AICF_EStrategicDecisionAuthority.SYSTEM_HOLD)
			slot.BeginAwaitingPlayerCommand();
		else
			slot.ClearAwaitingPlayerCommand();
		if (updateStrategicIntent)
			slot.CommitStrategicIntent(target, posture, decisionAuthority);
		slot.ResetTargetUnavailableReport();
		if (oldTarget != target || oldPosture != posture ||
			oldDecisionAuthority != decisionAuthority)
		{
			slot.RecordStrategicAssignment(target, posture);
		}
		else
		{
			// A new replicated waypoint is a new asynchronous identity even when
			// target/posture/authority are unchanged. Advance the revision so active
			// vehicle/recovery snapshots cannot retain the deleted waypoint.
			slot.RecordRuntimeWaypointReplacement();
		}
		if (loneSurvivorRetreat)
			slot.BeginLoneSurvivorRetreat();
		LogOrderWaypointCreated(slot, faction, target, newWaypoint, reason, trigger);

		if (oldTarget && oldTarget != target)
		{
			AICF_Stage1Diagnostics.Info(
				"TARGET_REASSIGNED",
				string.Format(
					"faction=%1 slot=%2 slot_key=%3 role=%4 posture=%5 old_target=%6 new_target=%7 reason=%8 trigger=%9",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					slot.GetSlotKey(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					posture,
					AICF_Stage1Diagnostics.BaseKey(oldTarget),
					AICF_Stage1Diagnostics.BaseKey(target),
					reason,
					trigger));
		}
		else
		{
			AICF_Stage1Diagnostics.Info(
				"ORDER_ASSIGNED",
				string.Format(
					"faction=%1 slot=%2 slot_key=%3 role=%4 posture=%5 target=%6 reason=%7 trigger=%8",
					faction.GetFactionKey(),
					slot.GetSlotId(),
					slot.GetSlotKey(),
					AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
					posture,
					AICF_Stage1Diagnostics.BaseKey(target),
					reason,
					trigger));
		}
		string strategicAssignmentLine = string.Format(
				"faction=%1 slot=%2 stable_slot=%3 numeric_slot=%4 role=%5 posture=%6",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetStableSlotKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
				posture);
		strategicAssignmentLine += string.Format(
			" target=%1 target_kind=BASE trigger=%2 reason=%3 waypoint=%4",
				AICF_Stage1Diagnostics.BaseKey(target),
				trigger,
				reason,
				newWaypoint.GetID());
		strategicAssignmentLine += string.Format(
			" group=%1 group_generation=%2 assignment_revision=%3 assignment_age_ms=%4",
			group.GetID(),
			slot.GetSpawnGeneration(),
			slot.GetStrategicAssignmentRevision(),
			slot.GetStrategicAssignmentAgeMs());
		strategicAssignmentLine += string.Format(
			" decision_authority=%1 intent_revision=%2",
			AICF_StrategicDecisionAuthority.ToString(decisionAuthority),
			slot.GetStrategicIntentRevision());
		AICF_Stage35Diagnostics.Info("STRATEGIC_ASSIGNMENT", strategicAssignmentLine);
		return true;
	}

	protected void LogOrderWaypointCreated(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target,
		AIWaypoint waypoint,
		string reason,
		string trigger)
	{
		if (!slot || !faction || !waypoint)
			return;

		string createdLine = string.Format(
			"faction=%1 slot=%2 stable_slot=%3 numeric_slot=%4 group_generation=%5 waypoint=%6 waypoint_kind=%7",
			faction.GetFactionKey(),
			slot.GetSlotKey(),
			slot.GetStableSlotKey(),
			slot.GetSlotId(),
			slot.GetSpawnGeneration(),
			waypoint.GetID(),
			GetWaypointKind(slot, waypoint));
		createdLine += " " + BuildDestinationDetails(
			slot.GetTargetKind(),
			target,
			slot.GetTargetPosition());
		createdLine += string.Format(
			" role=%1 reason=%2 trigger=%3 completion_policy=%4 endpoint_revision=%5 endpoint=%6",
			AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
			reason,
			trigger,
			GetWaypointCompletionPolicy(slot, waypoint),
			slot.GetFalseCompletionEndpointRevision(),
			waypoint.GetOrigin());
		AICF_Stage35Diagnostics.Info("ORDER_WAYPOINT_CREATED", createdLine);
	}

	protected string GetWaypointCompletionPolicy(AICF_GroupSlot slot, AIWaypoint waypoint)
	{
		if (!waypoint)
			return "NONE";
		if (SCR_SmartActionWaypoint.Cast(waypoint))
			return "ANY";
		if (slot && slot.IsLoneSurvivorRetreat())
			return "ALL_PHYSICAL_RETREAT";
		if (slot && slot.IsSystemHoldOrder() && SCR_DefendWaypoint.Cast(waypoint))
			return "ALL_HOLD_3600S";
		if (slot && slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION &&
			SCR_DefendWaypoint.Cast(waypoint))
		{
			return "ALL_HOLD_3600S";
		}
		if (slot && slot.GetRole() == AICF_EGroupRole.ATTACK &&
			!SCR_SearchAndDestroyWaypoint.Cast(waypoint))
		{
			return "ALL";
		}
		if (SCR_DefendWaypoint.Cast(waypoint))
			return "ALL_HOLD_3600S";
		return "PREFAB_DEFAULT";
	}

	protected void LogWaypointRemoved(
		AICF_GroupSlot slot,
		AIWaypoint waypoint,
		string trigger,
		string reason)
	{
		if (!slot || !waypoint)
			return;

		string factionKey = "NONE";
		SCR_AIGroup group = slot.GetGroup();
		if (group && group.GetFaction())
			factionKey = group.GetFaction().GetFactionKey();
		string removalLine = string.Format(
				"faction=%1 slot=%2 stable_slot=%3 numeric_slot=%4 group_generation=%5 assignment_revision=%6",
				factionKey,
				slot.GetSlotKey(),
				slot.GetStableSlotKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				slot.GetStrategicAssignmentRevision());
		removalLine += string.Format(
				" waypoint=%1 waypoint_kind=%2 owner=ORDER_PLANNER remove_trigger=%3 remove_reason=%4",
				waypoint.GetID(),
				GetWaypointKind(slot, waypoint),
				trigger,
				reason);
		removalLine += " " + BuildDestinationDetails(
			slot.GetTargetKind(),
			slot.GetTargetBase(),
			slot.GetTargetPosition());
		AICF_Stage35Diagnostics.Info("WAYPOINT_REMOVED", removalLine);
	}

	protected string GetWaypointKind(AICF_GroupSlot slot, AIWaypoint waypoint)
	{
		if (!waypoint)
			return "NONE";
		if (SCR_SmartActionWaypoint.Cast(waypoint))
			return "ATTACK_RELAY_ACTION";
		if (SCR_SearchAndDestroyWaypoint.Cast(waypoint))
			return "ATTACK_OBJECTIVE_ACTION";
		if (slot && slot.IsLoneSurvivorRetreat())
			return "LONE_SURVIVOR_RETREAT";
		if (slot && slot.IsTemporaryRouteReplanHold())
			return "TEMPORARY_ROUTE_REPLAN_HOLD";
		if (slot && slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION &&
			SCR_DefendWaypoint.Cast(waypoint))
		{
			return "MOVE_AND_HOLD";
		}
		if (SCR_DefendWaypoint.Cast(waypoint))
		{
			if (slot && slot.IsSystemHoldOrder())
				return "SYSTEM_HOLD";
			if (slot && slot.IsPersistentStuckFieldHold())
				return "PERSISTENT_STUCK_FIELD_HOLD";

			return "DEFEND_ACTION";
		}
		if (slot && slot.GetRole() == AICF_EGroupRole.ATTACK)
			return "ATTACK_OPERATIONAL_MOVE";

		return "INFANTRY_WAYPOINT";
	}

	protected bool IsTargetValidForRole(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target)
	{
		if (!slot || !faction || !target || !target.GetOwner() || !target.IsInitialized())
			return false;
		if (slot.IsLoneSurvivorRetreat())
			return target == faction.GetMainBase() && target.GetFaction() == faction;

		if (slot.GetRole() == AICF_EGroupRole.ATTACK)
		{
			if (target.GetFaction() == faction || !target.IsValidTarget(faction))
				return false;

			if (target.GetType() == SCR_ECampaignBaseType.RELAY)
				return true;

			return !target.IsHQ();
		}

		if (slot.GetRole() == AICF_EGroupRole.DEFEND)
			return target.GetFaction() == faction && target.GetSpawnPoint();

		return target == faction.GetMainBase() && target.GetFaction() == faction;
	}

	protected bool IsCurrentTargetValid(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction)
	{
		if (!slot || !faction)
			return false;
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION)
		{
			return slot.GetDecisionAuthority() ==
				AICF_EStrategicDecisionAuthority.PLAYER_COMMAND;
		}
		if (!slot.IsSystemHoldOrder())
			return IsTargetValidForRole(slot, faction, slot.GetTargetBase());

		SCR_CampaignMilitaryBaseComponent mainBase = faction.GetMainBase();
		return mainBase && mainBase.GetOwner() && mainBase.IsInitialized() &&
			mainBase.GetFaction() == faction && mainBase.GetSpawnPoint() &&
			slot.GetTargetBase() == mainBase;
	}

	protected SCR_CampaignMilitaryBaseComponent SelectOperationalTarget(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		SCR_CampaignMilitaryBaseComponent excludedTarget,
		out string posture,
		out string trigger)
	{
		posture = string.Empty;
		trigger = "ASSIGNMENT";
		if (!slot || !faction || !graph || !targetSelector)
			return null;

		if (slot.GetRole() == AICF_EGroupRole.ATTACK)
		{
			int preferredIndex = slot.GetRoleIndex();
			posture = GetAttackPosture(preferredIndex);
			return targetSelector.SelectAttackTarget(
				graph,
				faction,
				trigger,
				excludedTarget,
				preferredIndex);
		}

		if (slot.GetRole() == AICF_EGroupRole.DEFEND)
			return targetSelector.SelectDefendTarget(graph, faction, posture, trigger);

		posture = POSTURE_IDLE_RESERVE;
		trigger = "LEGACY_BASELINE";
		return faction.GetMainBase();
	}

	protected string GetAttackPosture(int roleIndex)
	{
		if (roleIndex == 0)
			return POSTURE_ATTACK_PRIMARY;
		if (roleIndex == 1)
			return POSTURE_ATTACK_SECONDARY;

		return POSTURE_ATTACK_SUPPORT;
	}

	protected AIWaypoint CreateWaypoint(
		SCR_CampaignMilitaryBaseComponent target,
		AICF_EGroupRole role,
		SCR_AIGroup group,
		int endpointRevision,
		AICF_GroupSlot slot)
	{
		if (!target || !target.GetOwner())
			return null;

		ResourceName waypointPrefab = DEFEND_WAYPOINT_PREFAB;
		float completionRadius = DEFEND_RADIUS_METERS;
		bool isRelay = role == AICF_EGroupRole.ATTACK && target.GetType() == SCR_ECampaignBaseType.RELAY;
		if (isRelay)
		{
			waypointPrefab = RELAY_WAYPOINT_PREFAB;
			completionRadius = RELAY_RADIUS_METERS;
		}
		else if (role == AICF_EGroupRole.ATTACK)
		{
			waypointPrefab = ATTACK_OPERATIONAL_WAYPOINT_PREFAB;
			completionRadius = ATTACK_OPERATIONAL_RADIUS_METERS;
		}

		Resource waypointResource = Resource.Load(waypointPrefab);
		if (!waypointResource || !waypointResource.IsValid())
		{
			AICF_Stage1Diagnostics.Error("WAYPOINT_PREFAB_INVALID", waypointPrefab);
			return null;
		}

		vector targetPosition;
		if (!TryResolveTargetPosition(target, role, targetPosition))
			return null;
		if (!isRelay && endpointRevision > 0)
		{
			vector recoveryEndpoint;
			bool endpointResolved = TryResolveFalseCompletionEndpoint(
				group,
				targetPosition,
				endpointRevision,
				recoveryEndpoint);
			if (endpointResolved)
			{
				targetPosition = recoveryEndpoint;
			}
			else
			{
				// The controller converts this route-local miss into a temporary hold and
				// a full strategic replan. It must not leak into the unrelated generic
				// reliability-repair failure budget.
				string factionKey = "NONE";
				if (group && group.GetFaction())
					factionKey = group.GetFaction().GetFactionKey();
				string endpointFallbackDetails = string.Format(
					"faction=%1 slot=%2 stable_slot=%3 numeric_slot=%4 group_generation=%5 assignment_revision=%6 target=%7 endpoint_revision=%8",
					factionKey,
					slot.GetSlotKey(),
					slot.GetStableSlotKey(),
					slot.GetSlotId(),
					slot.GetSpawnGeneration(),
					slot.GetStrategicAssignmentRevision(),
					AICF_Stage1Diagnostics.BaseKey(target),
					endpointRevision);
				endpointFallbackDetails +=
					" endpoint_resolution=UNAVAILABLE next_action=REQUEST_TEMPORARY_ROUTE_REPLAN_HOLD reliability_budget_consumed=0";
				AICF_Stage2Diagnostics.Warning(
					"FALSE_COMPLETION_ROUTE_ENDPOINT_UNAVAILABLE",
					endpointFallbackDetails);
				return null;
			}
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = targetPosition;

		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(waypointPrefab, false, params: spawnParams);
		AIWaypoint waypoint = AIWaypoint.Cast(spawnedEntity);
		if (!waypoint)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);
			return null;
		}

		if (isRelay)
		{
			SCR_SmartActionWaypoint relayWaypoint = SCR_SmartActionWaypoint.Cast(waypoint);
			if (!relayWaypoint)
			{
				RplComponent.DeleteRplEntity(waypoint, false);
				AICF_Stage1Diagnostics.Error("RELAY_WAYPOINT_INVALID", RELAY_WAYPOINT_PREFAB);
				return null;
			}

			relayWaypoint.SetSmartActionEntity(target.GetOwner(), RELAY_SMART_ACTION_TAG);
			waypoint.SetCompletionType(EAIWaypointCompletionType.Any);
		}
		else if (role == AICF_EGroupRole.ATTACK)
		{
			// A long strategic leg is complete only after the whole living formation
			// reaches it. Leader completion allowed a separated leader to terminate a
			// waypoint while the group was still hundreds of metres from the target.
			waypoint.SetCompletionType(EAIWaypointCompletionType.All);
		}
		else
		{
			waypoint.SetCompletionType(EAIWaypointCompletionType.All);
			SCR_TimedWaypoint defendWaypoint = SCR_TimedWaypoint.Cast(waypoint);
			if (defendWaypoint)
				defendWaypoint.SetHoldingTime(DEFEND_HOLDING_TIME_SECONDS);
		}

		// The setting is copied into the group only while this AICF waypoint is
		// current. Column reduces travel spread while stock combat behavior remains
		// free to seek cover near the objective.
		SCR_AIWaypoint scriptedWaypoint = SCR_AIWaypoint.Cast(waypoint);
		if (scriptedWaypoint)
		{
			scriptedWaypoint.AddSetting(SCR_AIGroupFormationSetting.Create(
				SCR_EAISettingOrigin.WAYPOINT,
				SCR_EAIGroupFormation.Column));
		}

		waypoint.SetCompletionRadius(completionRadius);
		return waypoint;
	}

	protected bool TryResolveFalseCompletionEndpoint(
		SCR_AIGroup group,
		vector objectivePosition,
		int endpointRevision,
		out vector endpoint)
	{
		endpoint = vector.Zero;
		if (!group || endpointRevision <= 0)
			return false;
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(
			group.FindComponent(AIPathfindingComponent));
		if (!pathfinding)
			return false;
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!navmesh || !leader)
			return false;
		vector origin = leader.GetOrigin();
		vector navmeshOrigin;
		// GetReachablePoint rejects an origin which is not sufficiently close to
		// navmesh. A formation stopped against an obstacle can leave its living
		// leader just outside that tolerance even though a usable connected point
		// exists beside it. Snap only the route query origin; never move the entity.
		if (!pathfinding.GetClosestPositionOnNavmesh(
			origin,
			NAVMESH_ENDPOINT_SEARCH_HALF_EXTENTS,
			navmeshOrigin))
		{
			return false;
		}
		float objectiveDistanceMeters = vector.DistanceXZ(origin, objectivePosition);
		if (objectiveDistanceMeters <= FALSE_COMPLETION_ROUTE_MIN_PROGRESS_METERS)
		{
			// A hidden egress recovery can legitimately place the formation inside the
			// minimum route-leg distance. Finish against the real objective here; the
			// controller still requires its independent physical completion radius.
			endpoint = objectivePosition;
			return true;
		}
		float requestedLegMeters = Math.Min(
			FALSE_COMPLETION_ROUTE_LEG_METERS,
			Math.Max(
				FALSE_COMPLETION_ROUTE_MIN_PROGRESS_METERS,
				objectiveDistanceMeters - ATTACK_OPERATIONAL_RADIUS_METERS));
		// A fence or another concave obstacle can make every connected escape leg
		// temporarily increase straight-line objective distance. Permit only a
		// bounded detour and still choose the candidate with the best remaining
		// distance, so recovery can leave that local minimum without wandering.
		float bestRemainingMeters = objectiveDistanceMeters +
			FALSE_COMPLETION_ROUTE_MAX_DETOUR_METERS;
		bool found;
		for (int candidateAttempt; candidateAttempt < FALSE_COMPLETION_ROUTE_SAMPLE_COUNT; candidateAttempt++)
		{
			int distanceBand = candidateAttempt;
			while (distanceBand >= 3)
				distanceBand -= 3;
			float sampleDistanceMeters = requestedLegMeters -
				distanceBand * 20.0;
			vector candidate;
			if (!navmesh.GetReachablePoint(navmeshOrigin, sampleDistanceMeters, candidate))
				continue;
			float legMeters = vector.DistanceXZ(origin, candidate);
			float remainingMeters = vector.DistanceXZ(candidate, objectivePosition);
			if (legMeters < FALSE_COMPLETION_ROUTE_MIN_PROGRESS_METERS ||
				remainingMeters > objectiveDistanceMeters +
					FALSE_COMPLETION_ROUTE_MAX_DETOUR_METERS ||
				(found && remainingMeters >= bestRemainingMeters - 5.0))
			{
				continue;
			}
			endpoint = candidate;
			bestRemainingMeters = remainingMeters;
			found = true;
		}
		return found;
	}

	protected AIWaypoint CreateLoneSurvivorRetreatWaypoint(
		SCR_AIGroup group,
		SCR_CampaignMilitaryBaseComponent target)
	{
		Resource waypointResource = Resource.Load(ATTACK_OPERATIONAL_WAYPOINT_PREFAB);
		if (!waypointResource || !waypointResource.IsValid())
			return null;
		vector targetPosition;
		if (!TryResolveTargetPosition(target, AICF_EGroupRole.DEFEND, targetPosition))
			return null;
		AIPathfindingComponent pathfinding;
		if (group)
			pathfinding = AIPathfindingComponent.Cast(group.FindComponent(AIPathfindingComponent));
		vector navmeshPosition;
		if (pathfinding && pathfinding.GetClosestPositionOnNavmesh(
			targetPosition,
			NAVMESH_ENDPOINT_SEARCH_HALF_EXTENTS,
			navmeshPosition))
		{
			targetPosition = navmeshPosition;
		}
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = targetPosition;
		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(
			ATTACK_OPERATIONAL_WAYPOINT_PREFAB,
			false,
			params: spawnParams);
		AIWaypoint waypoint = AIWaypoint.Cast(spawnedEntity);
		if (!waypoint)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);
			return null;
		}
		waypoint.SetCompletionType(EAIWaypointCompletionType.All);
		waypoint.SetCompletionRadius(ATTACK_OPERATIONAL_RADIUS_METERS);
		return waypoint;
	}

	// This is the sole creation path for the local non-relay ATTACK action. Keep
	// it separate from CreateWaypoint(), which is used for operational assignment
	// and recovery and must therefore always rebuild an ATTACK Move waypoint.
	protected SCR_SearchAndDestroyWaypoint CreateAttackObjectiveAction(
		SCR_CampaignMilitaryBaseComponent target)
	{
		if (!target || !target.GetOwner() || target.GetType() == SCR_ECampaignBaseType.RELAY)
			return null;

		Resource waypointResource = Resource.Load(ATTACK_OBJECTIVE_ACTION_PREFAB);
		if (!waypointResource || !waypointResource.IsValid())
		{
			AICF_Stage1Diagnostics.Error(
				"WAYPOINT_PREFAB_INVALID",
				ATTACK_OBJECTIVE_ACTION_PREFAB);
			return null;
		}

		vector targetPosition;
		if (!TryResolveTargetPosition(target, AICF_EGroupRole.ATTACK, targetPosition))
			return null;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = targetPosition;

		IEntity spawnedEntity = GetGame().SpawnEntityPrefabEx(
			ATTACK_OBJECTIVE_ACTION_PREFAB,
			false,
			params: spawnParams);
		SCR_SearchAndDestroyWaypoint objectiveAction =
			SCR_SearchAndDestroyWaypoint.Cast(spawnedEntity);
		if (!objectiveAction)
		{
			if (spawnedEntity)
				RplComponent.DeleteRplEntity(spawnedEntity, false);

			AICF_Stage1Diagnostics.Error(
				"ATTACK_OBJECTIVE_ACTION_INVALID",
				ATTACK_OBJECTIVE_ACTION_PREFAB);
			return null;
		}

		objectiveAction.SetHoldingTime(ATTACK_OBJECTIVE_HOLDING_TIME_SECONDS);
		objectiveAction.SetCompletionRadius(ATTACK_OBJECTIVE_ACTION_RADIUS_METERS);

		SCR_AIWaypoint scriptedWaypoint = SCR_AIWaypoint.Cast(objectiveAction);
		if (scriptedWaypoint)
		{
			scriptedWaypoint.AddSetting(SCR_AIGroupFormationSetting.Create(
				SCR_EAISettingOrigin.WAYPOINT,
				SCR_EAIGroupFormation.Column));
		}

		return objectiveAction;
	}

	bool CanRecruitInfantry(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		return Replication.IsServer() && slot && faction && slot.IsCombatReady() &&
			slot.GetUnitType() == AICF_EGroupUnitType.INFANTRY && !slot.HasPlayerStrategicIntent() &&
			!slot.IsAwaitingPlayerCommand() && !slot.IsLoneSurvivorRetreat() &&
			m_AuthorityPolicy && m_AuthorityPolicy.IsAICommanderEnabled(faction.GetFactionKey()) &&
			slot.HasStrategicIntent() && slot.GetStrategicIntentTargetKind() == AICF_EOrderTargetKind.BASE;
	}

	bool BeginInfantryRecruitment(AICF_InfantryRecruitmentOrder order)
	{
		if (!order || !CanRecruitInfantry(order.m_Slot, order.m_Faction) || !order.HasSafeBarracks())
			return false;
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(order.m_Group.FindComponent(AIPathfindingComponent));
		vector endpoint;
		if (!pathfinding || !pathfinding.GetNavmeshComponent())
			return false;
		pathfinding.GetNavmeshComponent().LoadTileIn(order.m_vPosition);
		if (!pathfinding.GetClosestPositionOnNavmesh(order.m_vPosition, "15 5 15", endpoint) ||
			vector.DistanceSqXZ(endpoint, order.m_vPosition) > 225)
			return false;
		Resource resource = Resource.Load(DEFEND_WAYPOINT_PREFAB);
		if (!resource || !resource.IsValid())
			return false;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = endpoint;
		IEntity entity = GetGame().SpawnEntityPrefabEx(DEFEND_WAYPOINT_PREFAB, false, params: params);
		SCR_DefendWaypoint waypoint = SCR_DefendWaypoint.Cast(entity);
		if (!waypoint)
		{
			if (entity)
				RplComponent.DeleteRplEntity(entity, false);
			return false;
		}
		waypoint.SetCompletionRadius(15);
		waypoint.SetCompletionType(EAIWaypointCompletionType.All);
		waypoint.SetHoldingTime(3600);
		ClearOrder(order.m_Slot);
		order.m_Group.AddWaypointAt(waypoint, 0);
		order.m_Slot.AssignObjective(order.m_Base, waypoint);
		order.m_Slot.RecordStrategicAssignment(order.m_Base, "INFANTRY_RECRUITMENT");
		order.m_Waypoint = waypoint;
		order.m_iAssignment = order.m_Slot.GetStrategicAssignmentRevision();
		order.m_Slot.SetRecruitmentOrder(order);
		return true;
	}

	void EndInfantryRecruitment(AICF_InfantryRecruitmentOrder order, AICF_ObjectiveGraph graph,
		AICF_TargetSelector selector, bool restore)
	{
		if (!Replication.IsServer() || !order)
			return;
		bool current = order.IsCurrent(order.m_Slot);
		order.m_Slot.SetRecruitmentOrder(null);
		if (current)
		{
			ClearOrder(order.m_Slot);
			if (restore)
				AssignOrder(order.m_Slot, order.m_Faction, graph, selector, "INFANTRY_RECRUITMENT_FINISHED");
		}
		else if (order.m_Waypoint && order.m_Group && order.m_Group.GetID() == order.m_GroupId)
		{
			order.m_Group.RemoveWaypoint(order.m_Waypoint);
			RplComponent.DeleteRplEntity(order.m_Waypoint, false);
		}
		order.m_Waypoint = null;
	}
}
