// Read-only альтернатива внутри прежнего бюджета ремонта. Native connected point
// не доказывает фактический доезд: покупки по-прежнему ждут IsPhysicallyPresent.
class AICF_RecruitmentApproachEndpoint
{
	static const float COMPLETION_RADIUS_METERS = 15;
	static const float MINIMUM_COMPLETION_RADIUS_METERS = 1;
	static const float ARRIVAL_MARGIN_METERS = 1;
	static const float MINIMUM_SEPARATION_METERS = 5;
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

	static bool TryResolve(AICF_InfantryRecruitmentOrder order, AIPathfindingComponent pathfinding,
		out vector endpoint, out float completionRadius)
	{
		completionRadius = 0;
		if (!Replication.IsServer() || !order || !order.IsCurrent(order.m_Slot) ||
			!order.HasSafeBarracks() || !pathfinding || !pathfinding.GetNavmeshComponent() ||
			order.m_iApproachRepairs < 1 || order.m_iApproachRepairs > 2 ||
			order.m_iArrivedAtMs > 0 || order.m_Donor ||
			AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(order.m_Group) > 0)
		{
			return false;
		}
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
		float serviceDistance = vector.DistanceXZ(origin, order.m_vPosition);
		float bestLeg = float.MAX;
		bool found = false;
		for (int attempt; attempt < MAXIMUM_SAMPLES; attempt++)
		{
			int band = attempt;
			while (band >= 3) band -= 3;
			float requestedDistance = Math.Max(MINIMUM_SEPARATION_METERS,
				serviceDistance - maximumServiceDistance + band * 10);
			vector candidate;
			// Actual leader origin: не подменяем источник запроса проекцией через забор.
			if (!navmesh.GetReachablePoint(origin, requestedDistance, candidate))
			{
				order.Log("INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_REJECTED", string.Format(
					"repair=%1 sample=%2 reason=NATIVE_REACHABLE_QUERY_FAILED origin=%3 endpoint=NONE assignment_revision=%4 intent_revision=%5 graph_revision=%6",
					order.m_iApproachRepairs, attempt, origin, order.m_iAssignment, order.m_iIntent, order.m_iGraphRevision));
				continue;
			}
			float remaining = vector.DistanceXZ(candidate, order.m_vPosition);
			float leg = vector.DistanceXZ(origin, candidate);
			float candidateRadius = CompletionRadiusForServiceDistance(remaining);
			string rejection;
			if (remaining > maximumServiceDistance)
				rejection = "OUTSIDE_ARRIVAL_ENVELOPE";
			else if (candidateRadius < MINIMUM_COMPLETION_RADIUS_METERS ||
				remaining + candidateRadius + ARRIVAL_MARGIN_METERS > AICF_InfantryRecruitmentConfig.ARRIVAL_METERS)
				rejection = "COMPLETION_CIRCLE_OUTSIDE_ARRIVAL";
			else if (leg < MINIMUM_SEPARATION_METERS)
				rejection = "NO_VISIBLE_LEG";
			foreach (vector attempted : order.m_aApproachEndpoints)
				if (vector.DistanceXZ(candidate, attempted) < MINIMUM_SEPARATION_METERS)
					rejection = "PREVIOUS_ENDPOINT";
			vector checkedPosition;
			if (rejection.IsEmpty() && !AICF_InfantrySpawnPlacement.IsUsable(order.m_Group, candidate, checkedPosition))
				rejection = "PHYSICAL_CLEARANCE_OR_NAVMESH_REJECTED";
			if (rejection.IsEmpty() && vector.Distance(candidate, checkedPosition) > 0.1)
				rejection = "NATIVE_CONNECTED_PROJECTION_CHANGED";
			if (!rejection.IsEmpty())
			{
				order.Log("INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_REJECTED", string.Format(
					"repair=%1 sample=%2 reason=%3 origin=%4 endpoint=%5 service_distance_m=%6 leader_leg_m=%7 assignment_revision=%8 intent_revision=%9",
					order.m_iApproachRepairs, attempt, rejection, origin, candidate, remaining, leg, order.m_iAssignment, order.m_iIntent) +
					string.Format(" graph_revision=%1", order.m_iGraphRevision));
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
		if (!found || !order.IsCurrent(order.m_Slot) || !order.HasSafeBarracks() ||
			AICF_GroupRuntime.ResolveAliveLeader(order.m_Group) != leader || leader.GetID() != leaderId ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(leader) ||
			vector.Distance(origin, leader.GetOrigin()) > 0.1)
		{
			return false;
		}
		vector recheckedPosition;
		if (!AICF_InfantrySpawnPlacement.IsUsable(order.m_Group, endpoint, recheckedPosition) ||
			vector.Distance(endpoint, recheckedPosition) > 0.1 ||
			!order.IsCurrent(order.m_Slot) || !order.HasSafeBarracks() ||
			AICF_GroupRuntime.ResolveAliveLeader(order.m_Group) != leader || leader.GetID() != leaderId ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(leader) ||
			vector.Distance(origin, leader.GetOrigin()) > 0.1)
		{
			return false;
		}
		order.Log("INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_SELECTED", string.Format(
			"repair=%1 origin=%2 endpoint=%3 service_distance_m=%4 leader_leg_m=%5 assignment_revision=%6 intent_revision=%7 graph_revision=%8 native_connected=1 physically_clear=1 movement_confirmation=PENDING",
			order.m_iApproachRepairs, origin, endpoint, vector.DistanceXZ(endpoint, order.m_vPosition), bestLeg, order.m_iAssignment, order.m_iIntent, order.m_iGraphRevision) +
			string.Format(" completion_radius_m=%1 completion_containment_m=%2", completionRadius,
				vector.DistanceXZ(endpoint, order.m_vPosition) + completionRadius + ARRIVAL_MARGIN_METERS));
		return true;
	}
}
