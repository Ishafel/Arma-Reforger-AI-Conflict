// Isolated coverage gate: hold one distant actor's GetIn admission until actual
// prequeue walking. No actor/vehicle transform, health or AI cap is changed.
modded class AICF_VehicleBoardingFlow
{
	protected AICF_VehicleBoardingActionToken m_QueueProbeToken;
	protected vector m_vQueueProbeStart;
	protected int m_iQueueProbeDeadline;
	protected int m_iQueueProbeWalkingPolls;
	protected int m_iQueueProbeLastPoll;
	protected EntityID m_QueueProbeActorId;
	protected string m_sQueueProbeActorRpl;
	protected float m_fQueueProbeHealth;
	protected ref SCR_AIMoveIndividuallyBehavior m_QueueProbeApproach;
	protected Vehicle m_QueueProbeVehicle;
	protected EntityID m_QueueProbeVehicleId;
	protected string m_sQueueProbeVehicleRpl;
	protected bool m_bQueueProbeWalkProven;
	protected bool m_bQueueProbeReleased;
	protected bool m_bQueueProbeEntryLogged;

	override protected AICF_TripOutcome BeginPreset(AICF_TransportTrip trip, AICF_VehicleLease lease,
		SCR_AIGroup group, AICF_VehicleBoardingState state, string causationId)
	{
		string flag;
		if (!System.GetCLIParam("aicfPresetQueuedApproachProbe", flag))
			return super.BeginPreset(trip, lease, group, state, causationId);
		m_QueueProbeToken = null;
		m_QueueProbeApproach = null;
		m_QueueProbeVehicle = null;
		m_bQueueProbeWalkProven = false;
		m_bQueueProbeReleased = false;
		m_bQueueProbeEntryLogged = false;
		m_iQueueProbeWalkingPolls = 0;
		m_iQueueProbeLastPoll = 0;
		return super.BeginPreset(trip, lease, group, state, causationId);
	}

	override protected AICF_TripOutcome TickPreset(AICF_TransportTrip trip, AICF_VehicleLease lease,
		SCR_AIGroup group, AICF_VehicleBoardingState state, string causationId)
	{
		ObserveQueueProbe(trip, state);
		AICF_TripOutcome outcome = super.TickPreset(trip, lease, group, state, causationId);
		ObserveQueueProbe(trip, state);
		return outcome;
	}

	protected bool QueueProbeCurrent(AICF_TransportTrip trip, AICF_VehicleBoardingState state)
	{
		if (!m_QueueProbeToken || !trip || !trip.IsValid() || trip.IsTerminal() || !trip.GetAssignment()) return false;
		if (trip.GetPhase() != AICF_ETransportTripPhase.BOARDING || trip.GetBoardingState() != state || !state ||
			!trip.GetLease() || trip.GetLease().GetVehicle() != m_QueueProbeVehicle || !m_QueueProbeVehicle) return false;
		if (
			m_QueueProbeVehicle.GetID() != m_QueueProbeVehicleId ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_QueueProbeVehicle) != m_sQueueProbeVehicleRpl ||
			state.GetAbsoluteDeadlineMs() != m_iQueueProbeDeadline) return false;
		if (System.GetTickCount() >= m_iQueueProbeDeadline || trip.IsDeadlineReached(System.GetTickCount()) ||
			m_QueueProbeToken.WasPresetQueued()) return false;
		if (
			!m_QueueProbeToken.Matches(trip, trip.GetLease(), trip.GetAssignment().GetGroup()) ||
			!m_QueueProbeToken.IsPhysicalMutationOwnerSafe() || !m_QueueProbeToken.IsExactCompartmentMutationSafe()) return false;
		IEntity actor = m_QueueProbeToken.GetReservedEntity();
		if (!actor || actor.GetID() != m_QueueProbeActorId ||
			AICF_VehicleBehaviorBinding.ComponentRpl(actor) != m_sQueueProbeActorRpl) return false;
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(actor.FindComponent(SCR_CharacterDamageManagerComponent));
		return damage && Math.AbsFloat(damage.GetHealthScaled() - m_fQueueProbeHealth) <= 0.001;
	}

	static bool CanLatchQueueWalk(bool exact, bool observed, EAIActionState actionState, bool runningCurrent,
		float motion, float beforeDistance, float afterDistance, int polls)
	{
		return exact && observed && (actionState == EAIActionState.COMPLETED ||
			(actionState == EAIActionState.RUNNING && runningCurrent)) &&
			motion >= 10 && afterDistance < beforeDistance - 5 && polls >= 3;
	}

	static bool CanCountQueuePoll(bool runningCurrent, int nowMs, int lastMs)
	{
		return runningCurrent && nowMs >= lastMs && nowMs - lastMs >= 500;
	}

	protected void ObserveQueueProbe(AICF_TransportTrip trip, AICF_VehicleBoardingState state)
	{
		if (m_bQueueProbeWalkProven || !QueueProbeCurrent(trip, state)) return;
		IEntity actor = m_QueueProbeToken.GetReservedEntity();
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(m_QueueProbeToken.GetAgent().FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != actor) return;
		SCR_AIMoveIndividuallyBehavior current = SCR_AIMoveIndividuallyBehavior.Cast(utility.GetCurrentAction());
		if (!m_QueueProbeApproach && current && utility.GetExecutedAction() == current &&
			m_QueueProbeToken.IsActionOwnedByUtility(current) && current.GetActionState() == EAIActionState.RUNNING &&
			current.m_Entity.m_Value == m_QueueProbeVehicle)
			m_QueueProbeApproach = current;
		if (!m_QueueProbeApproach || m_QueueProbeApproach.m_Entity.m_Value != m_QueueProbeVehicle ||
			m_QueueProbeApproach.m_Utility != utility) return;
		bool runningCurrent = current == m_QueueProbeApproach && utility.GetExecutedAction() == m_QueueProbeApproach &&
			m_QueueProbeToken.IsActionOwnedByUtility(m_QueueProbeApproach) && m_QueueProbeApproach.GetActionState() == EAIActionState.RUNNING;
		int nowMs = System.GetTickCount();
		if (CanCountQueuePoll(runningCurrent, nowMs, m_iQueueProbeLastPoll))
		{
			m_iQueueProbeWalkingPolls++;
			m_iQueueProbeLastPoll = nowMs;
		}
		float motion = vector.DistanceXZ(m_vQueueProbeStart, actor.GetOrigin());
		if (!CanLatchQueueWalk(true, m_QueueProbeApproach != null, m_QueueProbeApproach.GetActionState(), runningCurrent,
			motion, vector.DistanceXZ(m_vQueueProbeStart, m_QueueProbeVehicle.GetOrigin()),
			vector.DistanceXZ(actor.GetOrigin(), m_QueueProbeVehicle.GetOrigin()), m_iQueueProbeWalkingPolls)) return;
		m_bQueueProbeWalkProven = true;
		Print(string.Format("[AICF][PREQUEUE_PROBE_WALK] operation_id=%1 member=%2 displacement_m=%3 native_polls=%4 entry_queued=%5 deadline_unchanged=%6 original_member=%7 health_unchanged=%8 pos=%9 pose_writes=0",
			trip.GetOperationId(), actor.GetID(), motion, m_iQueueProbeWalkingPolls, m_QueueProbeToken.WasPresetQueued(),
			m_iQueueProbeDeadline == state.GetAbsoluteDeadlineMs(), true, true, actor.GetOrigin()));
	}

	override protected bool QueuePresetSeat(AICF_TransportTrip trip, AICF_VehicleBoardingState state,
		AICF_VehicleBoardingActionToken token)
	{
		string flag;
		if (!System.GetCLIParam("aicfPresetQueuedApproachProbe", flag)) return super.QueuePresetSeat(trip, state, token);
		IEntity actor = token.GetReservedEntity();
		Vehicle vehicle = token.GetTargetVehicle();
		if (!actor || !vehicle) return super.QueuePresetSeat(trip, state, token);
		if (!m_QueueProbeToken && token.GetCompartmentType() == EAICompartmentType.Cargo &&
			vector.DistanceXZ(actor.GetOrigin(), vehicle.GetOrigin()) > 18)
		{
			SCR_CharacterDamageManagerComponent initialDamage = SCR_CharacterDamageManagerComponent.Cast(actor.FindComponent(SCR_CharacterDamageManagerComponent));
			if (!initialDamage) return false;
			m_QueueProbeToken = token;
			m_vQueueProbeStart = actor.GetOrigin();
			m_QueueProbeActorId = actor.GetID();
			m_sQueueProbeActorRpl = AICF_VehicleBehaviorBinding.ComponentRpl(actor);
			m_QueueProbeVehicle = vehicle;
			m_QueueProbeVehicleId = vehicle.GetID();
			m_sQueueProbeVehicleRpl = AICF_VehicleBehaviorBinding.ComponentRpl(vehicle);
			m_fQueueProbeHealth = initialDamage.GetHealthScaled();
			m_iQueueProbeDeadline = state.GetAbsoluteDeadlineMs();
			Print(string.Format("[AICF][PREQUEUE_PROBE_START] operation_id=%1 member=%2 start=%3 vehicle=%4 distance_m=%5 deadline_ms=%6 pose_writes=0 health_writes=0",
				trip.GetOperationId(), actor.GetID(), m_vQueueProbeStart, vehicle.GetID(),
				vector.DistanceXZ(actor.GetOrigin(), vehicle.GetOrigin()), m_iQueueProbeDeadline));
		}
		if (token == m_QueueProbeToken && !m_bQueueProbeReleased)
		{
			ObserveQueueProbe(trip, state);
			if (!m_bQueueProbeWalkProven || !QueueProbeCurrent(trip, state)) return false;
			m_bQueueProbeReleased = true;
		}
		bool accepted = super.QueuePresetSeat(trip, state, token);
		if (accepted && token == m_QueueProbeToken && !m_bQueueProbeEntryLogged)
		{
			m_bQueueProbeEntryLogged = true;
			Print(string.Format("[AICF][PREQUEUE_PROBE_ENTRY] operation_id=%1 member=%2 accepted=1 deadline_unchanged=%3 native_walk_before_entry=%4",
				trip.GetOperationId(), actor.GetID(), state.GetAbsoluteDeadlineMs() == m_iQueueProbeDeadline, m_bQueueProbeReleased));
		}
		return accepted;
	}
}
