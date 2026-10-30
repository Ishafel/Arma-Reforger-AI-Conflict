// Слабая связь с текущим lease. Только блокирует конкурирующие native intents;
// phase, рассадка, восстановление и lifecycle остаются у controller/flows.
class AICF_VehicleBehaviorBinding
{
	AICF_TransportTrip m_Trip;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	string m_sGroupRpl;
	AICF_VehicleLease m_Lease;
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	string m_sVehicleRpl;
	int m_iGroupGeneration;
	int m_iTripGeneration;
	int m_iLeaseGeneration;
	int m_iVehicleGeneration;
	int m_iPresetRevision;
	string m_sOperation;
	ref array<IEntity> m_aDeployed = {};
	ref array<EntityID> m_aDeployedIds = {};
	ref array<string> m_aDeployedRpls = {};

	static string ComponentRpl(IEntity entity)
	{
		if (!entity) return "NONE";
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl) return "NONE";
		return rpl.Id().ToString();
	}

	bool IsCurrent()
	{
		if (!Replication.IsServer() || !m_Trip || !m_Trip.IsValid() || m_Trip.IsTerminal() ||
			!AICF_VehicleBehaviorPolicy.IsBehaviorPhase(m_Trip.GetPhase()) ||
			!m_Trip.GetVehiclePreset() || !m_Trip.IsVehiclePresetCurrent() ||
			m_Trip.GetOperationId() != m_sOperation || m_Trip.GetGroupGeneration() != m_iGroupGeneration ||
			m_Trip.GetTripGeneration() != m_iTripGeneration || m_Trip.GetVehiclePresetRevision() != m_iPresetRevision) return false;
		AICF_StrategicAssignmentSnapshot assignment = m_Trip.GetAssignment();
		AICF_GroupSlot slot = m_Trip.GetVehiclePresetSlot();
		if (!assignment || assignment.GetGroup() != m_Group || !slot || slot.GetGroup() != m_Group ||
			slot.GetSpawnGeneration() != m_iGroupGeneration || slot.GetVehiclePresetRevision() != m_iPresetRevision ||
			slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) ||
			m_Group.GetID() != m_GroupId || ComponentRpl(m_Group) != m_sGroupRpl ||
			m_Trip.GetLease() != m_Lease || !m_Lease || !m_Lease.HasPhysicalAsset() ||
			m_Lease.GetLeaseGeneration() != m_iLeaseGeneration || m_Lease.GetVehicleGeneration() != m_iVehicleGeneration) return false;
		if (!m_Lease.MatchesTripIdentity(m_Trip.GetFactionKey(), m_Trip.GetSlotId(), m_iGroupGeneration, m_iTripGeneration) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle) ||
			m_Vehicle.GetID() != m_VehicleId || ComponentRpl(m_Vehicle) != m_sVehicleRpl ||
			!m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)) return false;
		return true;
	}

	bool OwnsActor(IEntity member)
	{
		if (!IsCurrent() || !AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (!character || !character.GetAIControlComponent()) return false;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent || agent.GetControlledEntity() != member || agent.GetParentGroup() != m_Group) return false;
		AICF_GroupSlot slot = m_Trip.GetVehiclePresetSlot();
		for (int index; index < AICF_Stage1Config.MAX_GROUP_SIZE; index++)
			if (m_Trip.VehiclePresetMember(index) == member && slot.GetRosterMember(index) == member) return true;
		return false;
	}

	bool IsDeployed(IEntity member)
	{
		int index = m_aDeployed.Find(member);
		return index >= 0 && OwnsActor(member) && member.GetID() == m_aDeployedIds[index] &&
			ComponentRpl(member) == m_aDeployedRpls[index];
	}
}

class AICF_VehicleBehaviorPolicy
{
	// Радиус боя независим от настраиваемого порога N для нового маршрута.
	static const float COMBAT_RADIUS_METERS = 300.0;
	static const float TARGET_MEMORY_SECONDS = 20.0;
	protected static ref array<ref AICF_VehicleBehaviorBinding> s_aBindings = {};

	static bool IsBehaviorPhase(AICF_ETransportTripPhase phase)
	{
		return phase == AICF_ETransportTripPhase.TRANSIT || phase == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT;
	}

	static void Prune()
	{
		for (int index = s_aBindings.Count() - 1; index >= 0; index--)
			if (!s_aBindings[index] || !s_aBindings[index].m_Trip || s_aBindings[index].m_Trip.IsTerminal() ||
				!s_aBindings[index].m_Group || !s_aBindings[index].m_Lease || !s_aBindings[index].m_Vehicle) s_aBindings.Remove(index);
	}

	static void Register(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() || !trip.GetAssignment()) return;
		if (!IsBehaviorPhase(trip.GetPhase()) || !trip.GetVehiclePreset() || !trip.IsVehiclePresetCurrent() ||
			!trip.GetLease() || !trip.GetLease().HasPhysicalAsset()) return;
		Prune();
		foreach (AICF_VehicleBehaviorBinding existing : s_aBindings)
			if (existing.m_Trip == trip) return;
		AICF_VehicleBehaviorBinding binding = new AICF_VehicleBehaviorBinding();
		binding.m_Trip = trip;
		binding.m_Group = trip.GetAssignment().GetGroup();
		binding.m_Lease = trip.GetLease();
		binding.m_Vehicle = binding.m_Lease.GetVehicle();
		if (!binding.m_Group || !binding.m_Vehicle) return;
		binding.m_GroupId = binding.m_Group.GetID();
		binding.m_sGroupRpl = AICF_VehicleBehaviorBinding.ComponentRpl(binding.m_Group);
		binding.m_VehicleId = binding.m_Vehicle.GetID();
		binding.m_sVehicleRpl = AICF_VehicleBehaviorBinding.ComponentRpl(binding.m_Vehicle);
		binding.m_iGroupGeneration = trip.GetGroupGeneration();
		binding.m_iTripGeneration = trip.GetTripGeneration();
		binding.m_iLeaseGeneration = binding.m_Lease.GetLeaseGeneration();
		binding.m_iVehicleGeneration = binding.m_Lease.GetVehicleGeneration();
		binding.m_iPresetRevision = trip.GetVehiclePresetRevision();
		binding.m_sOperation = trip.GetOperationId();
		if (binding.IsCurrent()) s_aBindings.Insert(binding);
	}

	static void Update(AICF_TransportTrip trip, bool localCombat, array<IEntity> deployedActors)
	{
		// localCombat — observation caller-а. Dispatch всегда читает threat заново,
		// чтобы последний scheduler tick не разрешал замену в начавшемся бою.
		Register(trip);
		foreach (AICF_VehicleBehaviorBinding binding : s_aBindings)
		{
			if (binding.m_Trip != trip || !binding.IsCurrent()) continue;
			binding.m_aDeployed.Clear();
			binding.m_aDeployedIds.Clear();
			binding.m_aDeployedRpls.Clear();
			if (!deployedActors) return;
			foreach (IEntity actor : deployedActors)
			{
				if (!binding.OwnsActor(actor) || binding.m_aDeployed.Contains(actor)) continue;
				binding.m_aDeployed.Insert(actor);
				binding.m_aDeployedIds.Insert(actor.GetID());
				binding.m_aDeployedRpls.Insert(AICF_VehicleBehaviorBinding.ComponentRpl(actor));
			}
			return;
		}
	}

	static void Unregister(AICF_TransportTrip trip)
	{
		for (int index = s_aBindings.Count() - 1; index >= 0; index--)
			if (!s_aBindings[index] || !s_aBindings[index].m_Trip || s_aBindings[index].m_Trip == trip) s_aBindings.Remove(index);
	}

	static AICF_VehicleBehaviorBinding Find(Vehicle vehicle)
	{
		if (!Replication.IsServer() || !vehicle) return null;
		Prune();
		AICF_VehicleBehaviorBinding found;
		foreach (AICF_VehicleBehaviorBinding binding : s_aBindings)
		{
			if (binding.m_Vehicle != vehicle || !binding.IsCurrent()) continue;
			if (found) return null;
			found = binding;
		}
		return found;
	}

	static bool HasLocalCombat(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || !trip.GetAssignment() || !trip.GetLease() || !trip.GetLease().HasPhysicalAsset()) return true;
		SCR_AIGroup group = trip.GetAssignment().GetGroup();
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!group || !vehicle) return true;
		SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
		if (!groupUtility || groupUtility.m_Owner != group) return true;
		if (groupUtility.m_Perception && groupUtility.m_Perception.m_MostDangerousCluster)
		{
			SCR_AIGroupTargetCluster cluster = groupUtility.m_Perception.m_MostDangerousCluster;
			if (IsLocalThreat(vector.DistanceXZ(vehicle.GetOrigin(), cluster.m_State.GetCenterPosition()),
				cluster.m_State.GetTimeSinceLastNewInformation())) return true;
		}
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || !AICF_GroupRuntime.IsAliveCharacter(agent.GetControlledEntity())) continue;
			if (agent.GetParentGroup() != group) return true;
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!utility || utility.m_OwnerEntity != agent.GetControlledEntity() || !utility.m_ThreatSystem || !utility.m_CombatComponent) return true;
			BaseTarget target = utility.m_CombatComponent.GetCurrentTarget();
			if (target && target.GetTimeSinceSeen() <= TARGET_MEMORY_SECONDS)
			{
				if (IsLocalThreat(vector.DistanceXZ(vehicle.GetOrigin(), target.GetLastSeenPosition()), target.GetTimeSinceSeen())) return true;
				continue;
			}
			// При неизвестном источнике нельзя объявлять бой законченным.
			if (utility.m_ThreatSystem.GetState() >= EAIThreatState.ALERTED &&
				utility.m_ThreatSystem.GetThreatMeasureWithoutInjuryFactor() > 0.01) return true;
			SCR_AIBehaviorBase behavior = utility.GetCurrentBehavior();
			if (SCR_AIAttackBehavior.Cast(behavior) || SCR_AISuppressBehavior.Cast(behavior)) return true;
		}
		return false;
	}

	static bool IsLocalThreat(float distanceMeters, float ageSeconds)
	{
		return distanceMeters >= 0 && distanceMeters <= COMBAT_RADIUS_METERS && ageSeconds >= 0 && ageSeconds <= TARGET_MEMORY_SECONDS;
	}

	static bool KeepCombatCrew(SCR_AIUtilityComponent utility, IEntity vehicle)
	{
		if (!utility || !vehicle) return false;
		AICF_VehicleBehaviorBinding binding = Find(Vehicle.Cast(vehicle));
		// Explicit distant TRANSPORT intent уже подтвердил физический возврат.
		// Только его current receipt удерживает crew от новой native высадки.
		if (!binding || (binding.m_Trip.GetVehiclePreset().m_iBehavior != AICF_EVehicleBehavior.COMBAT &&
			!HasTransportCombatOverride(binding.m_Trip)) ||
			!binding.OwnsActor(utility.m_OwnerEntity) || !SCR_AIDamageHandling.IsConscious(utility.m_OwnerEntity)) return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || usage.GetDamageState() == EDamageState.DESTROYED || SCR_AIVehicleUsability.VehicleIsOnFire(vehicle) ||
			!SCR_AIVehicleUsability.VehicleCanMove(vehicle)) return false;
		vector transform[4];
		vehicle.GetWorldTransform(transform);
		if (transform[1][1] < 0.25) return false;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(utility.m_OwnerEntity.FindComponent(CompartmentAccessComponent));
		if (!access || !access.IsInCompartment() || access.IsGettingOut() || CompartmentAccessComponent.GetVehicleIn(utility.m_OwnerEntity) != vehicle) return false;
		BaseCompartmentSlot seat = access.GetCompartment();
		return seat && seat.GetVehicle() == vehicle && seat.GetOccupant() == utility.m_OwnerEntity &&
			(seat.GetType() == ECompartmentType.PILOT || seat.GetType() == ECompartmentType.TURRET || seat.GetAttachedTurret());
	}

	static bool KeepCombatTurret(SCR_AIUtilityComponent utility, IEntity vehicle)
	{
		return KeepCombatCrew(utility, vehicle);
	}

	static bool SuppressGetOut(SCR_AIUtilityComponent utility, SCR_AIMessage_GetOut message)
	{
		return message && message.m_Vehicle && KeepCombatCrew(utility, message.m_Vehicle);
	}

	static bool ShouldDeferCrewReplacement(Vehicle vehicle)
	{
		AICF_VehicleBehaviorBinding binding = Find(vehicle);
		return binding && binding.m_Trip.GetVehiclePreset().m_iBehavior == AICF_EVehicleBehavior.COMBAT && HasLocalCombat(binding.m_Trip);
	}

	static bool IsCurrentReboarding(AICF_VehicleBehaviorBinding binding)
	{
		if (!binding || !binding.IsCurrent()) return false;
		AICF_TransportTrip trip = binding.m_Trip;
		if (trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || !trip.GetBehaviorState().m_bReboarding) return false;
		int nowMs = System.GetTickCount();
		if (trip.IsDeadlineReached(nowMs) || nowMs >= trip.GetBehaviorState().m_iReboardDeadline) return false;
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		return slot && assignment && slot.GetGroup() == assignment.GetGroup() &&
			slot.GetStrategicAssignmentRevision() == assignment.GetAssignmentRevision() &&
			slot.GetStrategicIntentRevision() == assignment.GetStrategicIntentRevision();
	}

	static bool HasTransportCombatOverride(AICF_TransportTrip trip)
	{
		if (!trip || !trip.GetVehiclePreset() || trip.GetVehiclePreset().m_iBehavior != AICF_EVehicleBehavior.TRANSPORT ||
			!trip.GetLease() || !trip.GetAssignment()) return false;
		AICF_VehicleBehaviorBinding binding = Find(trip.GetLease().GetVehicle());
		return binding && binding.m_Trip == trip && trip.GetBehaviorState().m_iCombatOverrideIntent >= 0 &&
			trip.GetBehaviorState().m_iCombatOverrideIntent == trip.GetAssignment().GetStrategicIntentRevision();
	}

	static bool ShouldYieldNativeCombat(SCR_AIVehicleCombatActivity activity)
	{
		if (!Replication.IsServer() || !activity || !activity.m_Utility) return false;
		Prune();
		foreach (AICF_VehicleBehaviorBinding binding : s_aBindings)
		{
			if (!binding.IsCurrent() || !HasTransportCombatOverride(binding.m_Trip)) continue;
			SCR_AIGroupUtilityComponent utility = binding.m_Group.GetGroupUtilityComponent();
			if (activity.m_Utility == utility && utility && utility.m_VehicleMgr && utility.m_VehicleMgr.FindVehicle(binding.m_Vehicle)) return true;
		}
		return false;
	}

	static bool SuppressGetIn(SCR_AIUtilityComponent utility, SCR_AIMessage_GetIn message)
	{
		if (!utility || !message || !message.m_Vehicle) return false;
		AICF_VehicleBehaviorBinding binding = Find(Vehicle.Cast(message.m_Vehicle));
		if (!binding || !binding.OwnsActor(utility.m_OwnerEntity)) return false;
		// Flow выдаёт собственные exact-seat actions напрямую, без native message.
		// Новая автоматическая посадка не должна менять роли в bounded reboard.
		if (binding.m_Trip.GetVehiclePreset().m_iBehavior == AICF_EVehicleBehavior.TRANSPORT && IsCurrentReboarding(binding)) return true;
		if (binding.IsDeployed(utility.m_OwnerEntity)) return true;
		if (!ShouldDeferCrewReplacement(binding.m_Vehicle)) return false;
		EAICompartmentType role = message.m_eRoleInVehicle;
		if (message.m_CompartmentSlot) role = SCR_AICompartmentHandling.CompartmentClassToType(message.m_CompartmentSlot.Type());
		// Сохранить сидящего бойца допустимо; посадка/замена во время боя запрещена.
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(utility.m_OwnerEntity.FindComponent(CompartmentAccessComponent));
		if (access && access.IsInCompartment() && !access.IsGettingOut() && CompartmentAccessComponent.GetVehicleIn(utility.m_OwnerEntity) == binding.m_Vehicle)
		{
			BaseCompartmentSlot currentSeat = access.GetCompartment();
			if (currentSeat && ((message.m_CompartmentSlot && message.m_CompartmentSlot == currentSeat) ||
				(!message.m_CompartmentSlot && SCR_AICompartmentHandling.CompartmentClassToType(currentSeat.Type()) == role))) return false;
		}
		return role == EAICompartmentType.Pilot || role == EAICompartmentType.Turret || role == EAICompartmentType.None;
	}

	static bool OwnsCombatActivity(SCR_AIVehicleCombatActivity activity, SCR_AIGroupVehicle vehicle)
	{
		if (!activity || !vehicle) return false;
		AICF_VehicleBehaviorBinding binding = Find(Vehicle.Cast(vehicle.GetEntity()));
		if (!binding || binding.m_Trip.GetVehiclePreset().m_iBehavior != AICF_EVehicleBehavior.COMBAT) return false;
		SCR_AIGroupUtilityComponent utility = binding.m_Group.GetGroupUtilityComponent();
		return utility && activity.m_Utility == utility && utility.m_Owner == binding.m_Group &&
			utility.GetCurrentAction() == activity && !activity.GetRemoveAction() && !activity.GetActionIsSuspended();
	}
}
