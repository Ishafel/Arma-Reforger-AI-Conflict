// Геометрия альтернативы внутри прежнего бюджета ремонта. Native projection и
// свободный объём не доказывают доезд: покупки по-прежнему ждут IsPhysicallyPresent.
class AICF_RecruitmentApproachEndpoint
{
	static const float COMPLETION_RADIUS_METERS = 15;
	static const float MINIMUM_COMPLETION_RADIUS_METERS = 1;
	static const float ARRIVAL_MARGIN_METERS = 1;
	static const float MINIMUM_SEPARATION_METERS = 5;
	static const float OUTER_RING_RADIUS_METERS = 32;
	static const float INNER_RING_RADIUS_METERS = 22;
	static const int SAMPLES_PER_RING = 6;
	static const int MAXIMUM_SAMPLES = 12;

	static float MaximumServiceDistance()
	{
		return Math.Max(0, AICF_InfantryRecruitmentConfig.ARRIVAL_METERS -
			MINIMUM_COMPLETION_RADIUS_METERS - ARRIVAL_MARGIN_METERS);
	}

	static float CompletionRadiusForServiceDistance(float remaining)
	{
		return Math.Min(COMPLETION_RADIUS_METERS, Math.Max(0,
			AICF_InfantryRecruitmentConfig.ARRIVAL_METERS - remaining - ARRIVAL_MARGIN_METERS));
	}

	static bool BuildCandidate(vector origin, vector service, int sample, out vector candidate)
	{
		if (sample < 0 || sample >= MAXIMUM_SAMPLES)
			return false;
		vector direction = origin - service;
		direction[1] = 0;
		float length = direction.Length();
		if (length < 0.1)
			return false;
		direction = direction / length;
		vector lateral = Vector(-direction[2], 0, direction[0]);
		float radius = OUTER_RING_RADIUS_METERS;
		if (sample >= SAMPLES_PER_RING)
			radius = INNER_RING_RADIUS_METERS;
		int directionIndex = sample;
		if (directionIndex >= SAMPLES_PER_RING)
			directionIndex -= SAMPLES_PER_RING;
		float angle = directionIndex * 60 * Math.DEG2RAD;
		candidate = service + direction * (Math.Cos(angle) * radius) + lateral * (Math.Sin(angle) * radius);
		return true;
	}

	static bool TryResolve(AICF_InfantryRecruitmentOrder order, AIPathfindingComponent pathfinding,
		out vector endpoint, out float completionRadius)
	{
		completionRadius = 0;
		if (!Replication.IsServer() || !order || !order.IsCurrent(order.m_Slot))
			return false;
		if (!order.HasSafeBarracks() || !pathfinding || !pathfinding.GetNavmeshComponent())
			return false;
		if (order.m_iApproachRepairs < 1 || order.m_iApproachRepairs > 2)
			return false;
		if (order.m_iArrivedAtMs > 0 || order.m_Donor ||
			AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(order.m_Group) > 0)
			return false;
		if (AIPathfindingComponent.Cast(order.m_Group.FindComponent(AIPathfindingComponent)) != pathfinding)
			return false;
		BaseWorld world = order.m_Group.GetWorld();
		if (!world)
			return false;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(order.m_Group);
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(leader))
			return false;
		array<AIAgent> agents = {};
		order.m_Group.GetAgents(agents);
		bool currentLeader = false;
		foreach (AIAgent agent : agents)
			if (agent && agent.GetParentGroup() == order.m_Group && agent.GetControlledEntity() == leader)
				currentLeader = true;
		if (!currentLeader)
			return false;
		EntityID leaderId = leader.GetID();
		vector origin = leader.GetOrigin();
		NavmeshWorldComponent navmesh = pathfinding.GetNavmeshComponent();
		if (!navmesh.IsTileLoaded(origin))
			return false;
		float maximumServiceDistance = MaximumServiceDistance();
		float bestLeg = float.MAX;
		bool found = false;
		for (int attempt; attempt < MAXIMUM_SAMPLES; attempt++)
		{
			vector query;
			if (!BuildCandidate(origin, order.m_vPosition, attempt, query))
				continue;
			query[1] = world.GetSurfaceY(query[0], query[2]);
			vector candidate = query;
			string rejection;
			if (!navmesh.IsTileLoaded(query))
				rejection = "NATIVE_ENDPOINT_TILE_UNAVAILABLE";
			else if (!pathfinding.GetClosestPositionOnNavmesh(query, "2 3 2", candidate))
				rejection = "NATIVE_ENDPOINT_PROJECTION_FAILED";
			else if (vector.DistanceSqXZ(query, candidate) > 4 || Math.AbsFloat(query[1] - candidate[1]) > 1)
				rejection = "NATIVE_ENDPOINT_PROJECTION_CHANGED";
			float remaining = vector.DistanceXZ(candidate, order.m_vPosition);
			float leg = vector.DistanceXZ(origin, candidate);
			float candidateRadius = CompletionRadiusForServiceDistance(remaining);
			if (rejection.IsEmpty() && remaining > maximumServiceDistance)
				rejection = "OUTSIDE_ARRIVAL_ENVELOPE";
			if (rejection.IsEmpty() && (candidateRadius < MINIMUM_COMPLETION_RADIUS_METERS ||
				remaining + candidateRadius + ARRIVAL_MARGIN_METERS > AICF_InfantryRecruitmentConfig.ARRIVAL_METERS))
				rejection = "COMPLETION_CIRCLE_OUTSIDE_ARRIVAL";
			if (rejection.IsEmpty() && leg < MINIMUM_SEPARATION_METERS)
				rejection = "NO_VISIBLE_LEG";
			foreach (vector attempted : order.m_aApproachEndpoints)
				if (rejection.IsEmpty() && vector.DistanceXZ(candidate, attempted) < MINIMUM_SEPARATION_METERS)
					rejection = "PREVIOUS_ENDPOINT";
			vector checkedPosition;
			if (rejection.IsEmpty() && !AICF_InfantrySpawnPlacement.IsUsable(order.m_Group, candidate, checkedPosition))
				rejection = "PHYSICAL_CLEARANCE_OR_NAVMESH_REJECTED";
			if (rejection.IsEmpty() && vector.Distance(candidate, checkedPosition) > 0.1)
				rejection = "PHYSICAL_ENDPOINT_REPROJECTION_CHANGED";
			if (!rejection.IsEmpty())
			{
				order.Log("INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_REJECTED", string.Format(
					"repair=%1 sample=%2 reason=%3 origin=%4 endpoint=%5 service_distance_m=%6 leader_leg_m=%7 assignment_revision=%8 intent_revision=%9",
					order.m_iApproachRepairs, attempt, rejection, origin, candidate, remaining, leg, order.m_iAssignment, order.m_iIntent) +
					string.Format(" graph_revision=%1 query=%2 reachability=UNPROVEN movement_confirmation=NONE", order.m_iGraphRevision, query));
				continue;
			}
			bool better = !found || leg < bestLeg;
			if (found && leg == bestLeg)
				better = candidate[0] < endpoint[0] || (candidate[0] == endpoint[0] && candidate[2] < endpoint[2]);
			if (!better) continue;
			endpoint = candidate;
			completionRadius = candidateRadius;
			bestLeg = leg;
			found = true;
		}
		if (!found || !order.IsCurrent(order.m_Slot) || !order.HasSafeBarracks())
			return false;
		if (AICF_GroupRuntime.ResolveAliveLeader(order.m_Group) != leader || leader.GetID() != leaderId ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(leader) ||
			vector.Distance(origin, leader.GetOrigin()) > 0.1)
			return false;
		if (AIPathfindingComponent.Cast(order.m_Group.FindComponent(AIPathfindingComponent)) != pathfinding ||
			pathfinding.GetNavmeshComponent() != navmesh)
			return false;
		vector recheckedPosition;
		if (!AICF_InfantrySpawnPlacement.IsUsable(order.m_Group, endpoint, recheckedPosition) ||
			vector.Distance(endpoint, recheckedPosition) > 0.1)
			return false;
		if (!order.IsCurrent(order.m_Slot) || !order.HasSafeBarracks())
			return false;
		if (AICF_GroupRuntime.ResolveAliveLeader(order.m_Group) != leader || leader.GetID() != leaderId ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(leader) ||
			vector.Distance(origin, leader.GetOrigin()) > 0.1)
			return false;
		if (AIPathfindingComponent.Cast(order.m_Group.FindComponent(AIPathfindingComponent)) != pathfinding ||
			pathfinding.GetNavmeshComponent() != navmesh)
			return false;
		order.Log("INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_SELECTED", string.Format(
			"repair=%1 origin=%2 endpoint=%3 service_distance_m=%4 leader_leg_m=%5 assignment_revision=%6 intent_revision=%7 graph_revision=%8 native_projected=1 physically_clear=1 reachability=UNPROVEN movement_confirmation=PENDING",
			order.m_iApproachRepairs, origin, endpoint, vector.DistanceXZ(endpoint, order.m_vPosition), bestLeg, order.m_iAssignment, order.m_iIntent, order.m_iGraphRevision) +
			string.Format(" completion_radius_m=%1 completion_containment_m=%2", completionRadius,
				vector.DistanceXZ(endpoint, order.m_vPosition) + completionRadius + ARRIVAL_MARGIN_METERS));
		return true;
	}
}
