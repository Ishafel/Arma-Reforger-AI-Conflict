// Локальные участки вокруг неизменной домашней базы, без radio edges.
class AICF_FIAGarrisonPatrol
{
	static const float ROUTE_RADIUS = 140;
	static const float RETURN_RADIUS = 200;
	protected ref AICF_VehicleTaskHandoff m_Handoff = new AICF_VehicleTaskHandoff(null, null, null);

	void Update(AICF_FIAGarrison g, int now)
	{
		if (!Replication.IsServer() || !g || g.m_bRetired) return;
		if (!g.BaseIdentity() || !g.CanDrive()) { Stop(g); return; }
		vector position = g.m_Vehicle.GetOrigin();
		if (!g.m_bReturning && vector.DistanceXZ(position, g.m_vHome) > RETURN_RADIUS)
		{
			m_Handoff.ClearFIAGarrisonWaypoint(g);
			g.m_bReturning = true;
			g.m_iPatrolRetryAtMs = now;
			g.Log("FIA_GARRISON_RETURN", string.Format("distance_m=%1 home=%2", vector.DistanceXZ(position, g.m_vHome), g.m_vHome));
		}
		if (g.PatrolWaypointIdentity())
		{
			if (vector.DistanceXZ(position, g.m_vPatrolTarget) <= 15)
			{
				g.m_iPatrolArrivals++;
				g.Log("FIA_GARRISON_PATROL_ARRIVED", string.Format("leg=%1 position=%2 home_distance_m=%3", g.m_iPatrolLeg, position, vector.DistanceXZ(position, g.m_vHome)));
				Stop(g);
				g.m_bReturning = false;
				g.m_iPatrolRetryAtMs = now + 2000;
				return;
			}
			if (vector.DistanceXZ(position, g.m_vPatrolProgress) >= 5)
			{
				g.m_vPatrolProgress = position;
				g.m_iPatrolProgressAtMs = now;
			}
			array<AIWaypoint> queue = {};
			g.m_Group.GetWaypoints(queue);
			if (!g.m_bPatrolMoveFailed && queue.Contains(g.m_PatrolWaypoint) && now - g.m_iPatrolProgressAtMs < 45000) return;
			g.Log("FIA_GARRISON_PATROL_RETRY", string.Format("leg=%1 position=%2 target=%3", g.m_iPatrolLeg, position, g.m_vPatrolTarget));
			g.m_iPatrolDirection = -g.m_iPatrolDirection;
			Stop(g);
			g.m_iPatrolRetryAtMs = now + 2000;
			return;
		}
		if (now < g.m_iPatrolRetryAtMs) return;
		g.m_iPatrolRetryAtMs = now + 10000;
		vector endpoint;
		if (g.m_bReturning) endpoint = g.m_vPosition;
		else if (!SelectEndpoint(g, endpoint)) return;
		if (!m_Handoff.MoveFIAGarrison(g, endpoint)) return;
		g.m_iPatrolLeg++;
		g.m_vPatrolProgress = position;
		g.m_iPatrolProgressAtMs = now;
		g.Log("FIA_GARRISON_PATROL_LEG", string.Format("leg=%1 target=%2 home=%3 radius=%4 returning=%5", g.m_iPatrolLeg, endpoint, g.m_vHome, ROUTE_RADIUS, g.m_bReturning));
	}

	protected bool SelectEndpoint(AICF_FIAGarrison g, out vector endpoint)
	{
		SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!ai || !ai.GetRoadNetworkManager()) return false;
		if (FindRoadEndpoint(ai.GetRoadNetworkManager(), g.m_Vehicle.GetOrigin(), g.m_vHome, g.m_iPatrolDirection, endpoint)) return true;
		for (int attempt; attempt < 32; attempt++)
		{
			int bearing = (g.m_iPatrolCandidate++ + g.m_iSlot * 3) % 16;
			float angle = bearing * 22.5 * Math.DEG2RAD;
			float radius = 60 + (attempt / 16) * 40;
			vector requested = g.m_Vehicle.GetOrigin() + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * radius;
			if (!ai.GetRoadNetworkManager().GetReachableWaypointInRoad(g.m_Vehicle.GetOrigin(), requested, 40, endpoint)) continue;
			float distance = vector.DistanceXZ(endpoint, g.m_Vehicle.GetOrigin());
			if (vector.DistanceXZ(endpoint, g.m_vHome) > ROUTE_RADIUS || distance < 45 || distance > 100) continue;
			return true;
		}
		g.Log("FIA_GARRISON_PATROL_WAIT", "reason=NO_LOCAL_ROAD");
		return false;
	}

	// Сначала продолжаем текущую дорогу: близкая по прямой соседняя дорога
	// может находиться за стеной/обрывом и требовать большого объезда.
	static bool FindRoadEndpoint(RoadNetworkManager roads, vector position, vector home, inout int patrolDirection, out vector endpoint)
	{
		BaseRoad road;
		float distance;
		roads.GetClosestRoad(position, road, distance, true);
		if (!road || distance > 20) return false;
		array<vector> points = {};
		road.GetPoints(points);
		float best = float.MAX;
		int segmentIndex = -1;
		vector projected;
		for (int i = 1; i < points.Count(); i++)
		{
			vector delta = points[i] - points[i - 1];
			vector flat = delta;
			flat[1] = 0;
			if (flat.LengthSq() < 1) continue;
			float fraction = Math.Clamp(vector.Dot(position - points[i - 1], flat) / flat.LengthSq(), 0, 1);
			vector candidate = points[i - 1] + delta * fraction;
			float separation = vector.DistanceXZ(position, candidate);
			if (separation >= best) continue;
			best = separation;
			segmentIndex = i - 1;
			projected = candidate;
		}
		if (segmentIndex < 0) return false;
		for (int attempt; attempt < 2; attempt++)
		{
			int direction = patrolDirection;
			if (attempt == 1) direction = -direction;
			int index = segmentIndex;
			if (direction > 0) index++;
			vector cursor = projected;
			float travelled;
			while (index >= 0 && index < points.Count() && travelled < 70)
			{
				vector delta = points[index] - cursor;
				float length = delta.Length();
				if (length < 0.1) { index += direction; continue; }
				float step = Math.Min(10, Math.Min(length, 70 - travelled));
				vector next = cursor + delta * (step / length);
				if (vector.DistanceXZ(next, home) > ROUTE_RADIUS) break;
				cursor = next;
				travelled += step;
				if (length <= step + 0.1) index += direction;
			}
			if (vector.DistanceXZ(cursor, position) < 45) continue;
			if (!roads.GetReachableWaypointInRoad(position, cursor, 10, endpoint) || vector.DistanceXZ(endpoint, cursor) > 10 ||
				vector.DistanceXZ(endpoint, home) > ROUTE_RADIUS) continue;
			patrolDirection = direction;
			return true;
		}
		return false;
	}

	void Stop(AICF_FIAGarrison g)
	{
		m_Handoff.ClearFIAGarrisonWaypoint(g);
	}
}

// Stock failure иначе назначает GetOutActivity: гарнизон теряет патрульную машину.
class AICF_FIAGarrisonMovementPolicy
{
	static bool Handle(SCR_AIGroup group, SCR_AIGroupUtilityComponent utility, int result, int handler, bool related, vector location)
	{
		if (!Replication.IsServer() || !related || !group || !utility || group.GetGroupUtilityComponent() != utility) return false;
		if (result != EMoveError.UNKNOWN && result != EMoveError.STUCK && result != EMoveError.UNREACHABLE &&
			result != EMoveError.STOPPED && result != EMoveError.ENTITY_CANT_MOVE && result != EMoveError.ENTITY_NOT_MOVABLE) return false;
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(utility.GetCurrentAction());
		if (!activity || !activity.m_RelatedWaypoint) return false;
		IEntity vehicle;
		if (handler != AIGroupMovementComponent.DEFAULT_HANDLER_ID)
		{
			if (!utility.m_VehicleMgr) return false;
			SCR_AIGroupVehicle groupVehicle = utility.m_VehicleMgr.FindVehicleBySubgroupId(handler);
			if (groupVehicle) vehicle = groupVehicle.GetEntity();
			else if (handler != -1) return false;
		}
		AIWaypoint waypoint = activity.m_RelatedWaypoint;
		AICF_FIAGarrison g = AICF_FIAGarrisonService.FindPatrol(group, waypoint, vehicle);
		if (!g) return false;
		int leg = g.m_iPatrolLeg;
		EntityID waypointId = g.m_PatrolWaypointId;
		AIActionBase failedAction = utility.GetExecutedAction();
		utility.OnMoveFailed(result, vehicle, related, location);
		// Синхронный invoker может сменить приказ; старый результат его не отменяет.
		if (AICF_FIAGarrisonService.FindPatrol(group, waypoint, vehicle) == g &&
			g.m_iPatrolLeg == leg && g.m_PatrolWaypointId == waypointId)
		{
			g.m_bPatrolMoveFailed = true;
			g.Log("FIA_GARRISON_MOVE_FAILED", string.Format("leg=%1 result=%2 target=%3", leg, result, location));
			if (failedAction && utility.GetExecutedAction() == failedAction) failedAction.Fail();
		}
		return true;
	}
}
