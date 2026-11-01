// Read-only причины retained combat. Вызывается существующим Report раз в 10s;
// решение HasLocalCombat, native actions и любые gameplay clocks не меняет.
class AICF_VehicleCombatTrace
{
	static void Snapshot(AICF_TransportTrip trip, bool localCombat)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() ||
			!trip.IsVehiclePresetCurrent() || !trip.GetAssignment() || !trip.GetLease() ||
			!trip.GetLease().HasPhysicalAsset()) return;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!group || !vehicle || !trip.GetLease().MatchesTripIdentity(trip.GetFactionKey(),
			trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())) return;
		SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
		string prefix = string.Format("operation_id=%1 group=%2 vehicle=%3 group_generation=%4 trip_generation=%5",
			trip.GetOperationId(), AICF_GroupTrace.Entity(group), AICF_GroupTrace.Entity(vehicle),
			trip.GetGroupGeneration(), trip.GetTripGeneration());
		string details = prefix + string.Format(" local_combat=%1 group_utility=%2 group_utility_owner_current=%3 readonly=1",
			localCombat, groupUtility != null, groupUtility && groupUtility.m_Owner == group);
		if (groupUtility)
			details += " selected_action=" + AICF_GroupTrace.Action(groupUtility.GetCurrentAction());
		AICF_Stage3Diagnostics.Info("VEHICLE_COMBAT_READBACK", details);
		if (groupUtility && groupUtility.m_Perception && groupUtility.m_Perception.m_MostDangerousCluster)
		{
			SCR_AIGroupTargetCluster cluster = groupUtility.m_Perception.m_MostDangerousCluster;
			if (cluster.m_State)
			{
				float clusterDistance = vector.DistanceXZ(vehicle.GetOrigin(), cluster.m_State.GetCenterPosition());
				float clusterAge = cluster.m_State.GetTimeSinceLastNewInformation();
				AICF_Stage3Diagnostics.Info("VEHICLE_COMBAT_CLUSTER", prefix + string.Format(
					" position=%1 vehicle_distance_m=%2 information_age_s=%3 policy_local=%4 readonly=1",
					cluster.m_State.GetCenterPosition(), clusterDistance, clusterAge,
					AICF_VehicleBehaviorPolicy.IsLocalThreat(clusterDistance, clusterAge)));
			}
		}
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		for (int index; index < agents.Count() && index < AICF_VehiclePreset.MAX_SEATS; index++)
		{
			AIAgent agent = agents[index];
			if (!agent) continue;
			IEntity actor = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(actor)) continue;
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			string row = prefix + string.Format(" index=%1 actor=%2 position=%3 vehicle_distance_m=%4 parent_current=%5 conscious=%6 utility=%7 utility_owner_current=%8",
				index, AICF_GroupTrace.Entity(actor), actor.GetOrigin(), vector.DistanceXZ(actor.GetOrigin(), vehicle.GetOrigin()),
				agent.GetParentGroup() == group, SCR_AIDamageHandling.IsConscious(actor), utility != null,
				utility && utility.m_OwnerEntity == actor);
			row += " in_vehicle=" + AICF_GroupTrace.Entity(CompartmentAccessComponent.GetVehicleIn(actor));
			if (utility)
			{
				row += string.Format(" threat_system=%1 combat_component=%2 selected_action=%3 executed_action=%4 behavior=%5",
					utility.m_ThreatSystem != null, utility.m_CombatComponent != null,
					AICF_GroupTrace.Action(utility.GetCurrentAction()), AICF_GroupTrace.Action(utility.GetExecutedAction()),
					AICF_GroupTrace.Action(utility.GetCurrentBehavior()));
				if (utility.m_ThreatSystem)
					row += string.Format(" threat_state=%1 threat_without_injury=%2",
						utility.m_ThreatSystem.GetState(), utility.m_ThreatSystem.GetThreatMeasureWithoutInjuryFactor());
				BaseTarget target;
				if (utility.m_CombatComponent) target = utility.m_CombatComponent.GetCurrentTarget();
				row += string.Format(" current_target=%1", target != null);
				if (target)
				{
					IEntity targetEntity = target.GetTargetEntity();
					float targetDistance = vector.DistanceXZ(vehicle.GetOrigin(), target.GetLastSeenPosition());
					float targetAge = target.GetTimeSinceSeen();
					row += string.Format(" target_position=%1 target_age_s=%2 target_vehicle_distance_m=%3 target_policy_local=%4",
						target.GetLastSeenPosition(), targetAge, targetDistance,
						AICF_VehicleBehaviorPolicy.IsLocalThreat(targetDistance, targetAge));
					row += string.Format(" target_entity=%1 target_alive_character=%2 target_endangering=%3 target_exposure=%4",
						AICF_GroupTrace.Entity(targetEntity), AICF_GroupRuntime.IsAliveCharacter(targetEntity),
						target.IsEndangering(), target.GetExposure());
				}
			}
			AICF_Stage3Diagnostics.Info("VEHICLE_COMBAT_MEMBER", row + " readonly=1");
		}
	}
}
