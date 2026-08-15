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
	protected static const float RELAY_RADIUS_METERS = 20.0;
	protected static const string POSTURE_ATTACK_PRIMARY = "ATTACK_PRIMARY";
	protected static const string POSTURE_ATTACK_SECONDARY = "ATTACK_SECONDARY";
	protected static const string POSTURE_ATTACK_SUPPORT = "ATTACK_SUPPORT";
	protected static const string POSTURE_FORWARD_DEFEND = "FORWARD_DEFEND";
	protected static const string POSTURE_QRF = "QRF";
	protected static const string POSTURE_IDLE_RESERVE = "IDLE_RESERVE";
	protected static const string POSTURE_PLAYER_ATTACK = "PLAYER_ATTACK";
	protected static const string POSTURE_PLAYER_DEFEND = "PLAYER_DEFEND";
	protected static const string POSTURE_PLAYER_RESERVE = "PLAYER_RESERVE";

	bool AssignOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		string reason,
		SCR_CampaignMilitaryBaseComponent excludedTarget = null)
	{
		if (!Replication.IsServer() || !slot || !faction || !graph || !targetSelector)
			return false;

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

		return ReplaceOrder(slot, faction, target, reason, posture, trigger);
	}

	// Applies an allied player's explicit strategic target. The role remains
	// server-owned: ATTACK slots may only receive legal enemy objectives,
	// DEFEND slots friendly spawn bases, and RESERVE slots their faction HQ.
	bool AssignPlayerOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target)
	{
		if (!Replication.IsServer() || !slot || !faction || !target ||
			!slot.IsCombatReady() || !IsTargetValidForRole(slot, faction, target))
		{
			return false;
		}

		string posture = POSTURE_PLAYER_RESERVE;
		if (slot.GetRole() == AICF_EGroupRole.ATTACK)
			posture = POSTURE_PLAYER_ATTACK;
		else if (slot.GetRole() == AICF_EGroupRole.DEFEND)
			posture = POSTURE_PLAYER_DEFEND;

		if (!ReplaceOrder(slot, faction, target, "PLAYER_COMMAND", posture, "PLAYER_COMMAND"))
			return false;

		slot.BeginPlayerStrategicOrder();
		return true;
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
		int stableCandidateMs)
	{
		if (!slot || !faction || !graph || !targetSelector || !slot.IsCombatReady())
			return false;

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
			return ReplaceOrder(slot, faction, desiredTarget, reason, desiredPosture, trigger);
		if (slot.HasPlayerStrategicOrder())
		{
			slot.ClearStrategicCandidate();
			return false;
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

		return ReplaceOrder(slot, faction, desiredTarget, reason, desiredPosture, trigger);
	}

	bool AssignLossResponseOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		AICF_ObjectiveGraph graph,
		AICF_TargetSelector targetSelector,
		SCR_CampaignMilitaryBaseComponent lostBase,
		int minimumDwellMs,
		int stableCandidateMs)
	{
		if (!slot || slot.GetRole() != AICF_EGroupRole.DEFEND || !faction ||
			!graph || !targetSelector || !lostBase || !slot.IsCombatReady())
		{
			return false;
		}

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

		return ReplaceOrder(
			slot,
			faction,
			responseTarget,
			string.Format("%1_QRF", responseTrigger),
			POSTURE_QRF,
			responseTrigger);
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

	string GetOrderFailureReason(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		if (!slot || !faction)
			return "INPUT_INVALID";
		if (!slot.IsCombatReady() || !slot.GetGroup())
			return "GROUP_NOT_READY";
		// Strategic ownership/eligibility changes supersede waypoint lifecycle.
		// A completed waypoint on a freshly captured target is a retarget event,
		// not an unstable movement recovery that should consume stuck budget.
		if (!IsTargetValidForRole(slot, faction, slot.GetTargetBase()))
			return "TARGET_INVALID";
		if (!slot.GetWaypoint())
			return "WAYPOINT_REFERENCE_MISSING";
		if (slot.GetGroup().GetCurrentWaypoint() != slot.GetWaypoint())
			return "WAYPOINT_NOT_CURRENT";
		if (slot.GetRole() == AICF_EGroupRole.ATTACK &&
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
		if (!slot || !faction || !graph || !targetSelector || !slot.IsCombatReady())
			return false;

		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		bool recovered;
		if (IsTargetValidForRole(slot, faction, oldTarget))
				recovered = ReplaceOrder(
					slot,
					faction,
					oldTarget,
					"ORDER_RECOVERY",
					slot.GetOperationalPosture(),
					"ORDER_RECOVERY");
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
					AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase()),
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
		if (!slot || !faction || !slot.IsCombatReady() ||
			!IsTargetValidForRole(slot, faction, slot.GetTargetBase()))
		{
			return false;
		}

		return ReplaceOrder(
			slot,
			faction,
			slot.GetTargetBase(),
			reason,
			slot.GetOperationalPosture(),
			"ORDER_REBUILD");
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
			slot.GetRole() != AICF_EGroupRole.ATTACK)
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
		if (!slot || !faction || !target || !slot.IsCombatReady() || !slot.GetGroup())
			return false;

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
		if (!slot.AssignObjective(target, fieldHold))
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
		if (!slot || !faction || !slot.IsCombatReady() || !slot.GetTargetBase())
			return false;

		vector targetPosition;
		if (!TryResolveTargetPosition(slot.GetTargetBase(), slot.GetRole(), targetPosition))
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
			slot.GetOperationalPosture(),
			slot.GetTargetBase(),
			targetPosition,
			slot.GetStrategicAssignmentRevision(),
			Math.Max(0, baseRevision),
			slot.GetWaypoint(),
			assignmentStartedAtMs);
		return snapshot.IsValid();
	}

	protected bool ReplaceOrder(
		AICF_GroupSlot slot,
		SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent target,
		string reason,
		string posture,
		string trigger)
	{
		SCR_AIGroup group = slot.GetGroup();
		if (!group || !slot.IsCombatReady())
			return false;

		AIWaypoint newWaypoint = CreateWaypoint(target, slot.GetRole());
		if (!newWaypoint)
			return false;

		AIWaypoint oldWaypoint = slot.GetWaypoint();
		SCR_CampaignMilitaryBaseComponent oldTarget = slot.GetTargetBase();
		string oldPosture = slot.GetOperationalPosture();
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
		slot.ClearPlayerStrategicOrder();
		slot.ResetTargetUnavailableReport();
		if (oldTarget != target || oldPosture != posture)
			slot.RecordStrategicAssignment(target, posture);
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
				"faction=%1 slot=%2 numeric_slot=%3 role=%4 posture=%5 target=%6 trigger=%7 reason=%8 waypoint=%9",
				faction.GetFactionKey(),
				slot.GetSlotKey(),
				slot.GetSlotId(),
				AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
				posture,
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
		if (!slot || !faction || !target || !waypoint)
			return;

		string createdLine = string.Format(
			"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 target=%5 waypoint=%6 waypoint_kind=%7",
			faction.GetFactionKey(),
			slot.GetSlotKey(),
			slot.GetSlotId(),
			slot.GetSpawnGeneration(),
			AICF_Stage1Diagnostics.BaseKey(target),
			waypoint.GetID(),
			GetWaypointKind(slot, waypoint));
		createdLine += string.Format(
			" role=%1 reason=%2 trigger=%3 completion_policy=%4",
			AICF_Stage1Diagnostics.RoleToString(slot.GetRole()),
			reason,
			trigger,
			GetWaypointCompletionPolicy(slot, waypoint));
		AICF_Stage35Diagnostics.Info("ORDER_WAYPOINT_CREATED", createdLine);
	}

	protected string GetWaypointCompletionPolicy(AICF_GroupSlot slot, AIWaypoint waypoint)
	{
		if (!waypoint)
			return "NONE";
		if (SCR_SmartActionWaypoint.Cast(waypoint))
			return "ANY";
		if (slot && slot.GetRole() == AICF_EGroupRole.ATTACK &&
			!SCR_SearchAndDestroyWaypoint.Cast(waypoint))
		{
			return "LEADER";
		}
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
		AICF_Stage35Diagnostics.Info(
			"WAYPOINT_REMOVED",
			string.Format(
				"faction=%1 slot=%2 numeric_slot=%3 group_generation=%4 waypoint=%5 waypoint_kind=%6 owner=ORDER_PLANNER remove_trigger=%7 remove_reason=%8 target=%9",
				factionKey,
				slot.GetSlotKey(),
				slot.GetSlotId(),
				slot.GetSpawnGeneration(),
				waypoint.GetID(),
				GetWaypointKind(slot, waypoint),
				trigger,
				reason,
				AICF_Stage1Diagnostics.BaseKey(slot.GetTargetBase())));
	}

	protected string GetWaypointKind(AICF_GroupSlot slot, AIWaypoint waypoint)
	{
		if (!waypoint)
			return "NONE";
		if (SCR_SmartActionWaypoint.Cast(waypoint))
			return "ATTACK_RELAY_ACTION";
		if (SCR_SearchAndDestroyWaypoint.Cast(waypoint))
			return "ATTACK_OBJECTIVE_ACTION";
		if (SCR_DefendWaypoint.Cast(waypoint))
		{
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
		AICF_EGroupRole role)
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
			// The stock Move prefab's completion policy is prefab-owned. Make the
			// operational contract explicit: a distant attack leg completes only when
			// the authoritative group leader arrives, never because one separated
			// member happens to enter the radius first.
			waypoint.SetCompletionType(EAIWaypointCompletionType.Leader);
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
}
