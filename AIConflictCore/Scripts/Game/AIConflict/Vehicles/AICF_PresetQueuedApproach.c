// Waiting for a free entry door must not leave a distant assigned member idle.
// This owns one native foot action; it never enters, reserves or teleports.
class AICF_PresetQueuedApproach
{
	static const float RADIUS_METERS = 8.0;
	protected AICF_TransportTrip m_Trip;
	protected AICF_VehicleLease m_Lease;
	protected AICF_VehicleBoardingState m_State;
	protected AICF_VehicleBoardingActionToken m_Token;
	protected SCR_AIGroup m_Group;
	protected IEntity m_Actor;
	protected EntityID m_ActorId;
	protected string m_sActorRpl;
	protected ref SCR_AIMoveIndividuallyBehavior m_Action;
	protected bool m_bIssued;
	protected bool m_bStopped;

	void AICF_PresetQueuedApproach(AICF_TransportTrip trip, AICF_VehicleLease lease,
		AICF_VehicleBoardingState state, AICF_VehicleBoardingActionToken token)
	{
		m_Trip = trip;
		m_Lease = lease;
		m_State = state;
		m_Token = token;
		if (trip && trip.GetAssignment()) m_Group = trip.GetAssignment().GetGroup();
		if (!token) return;
		m_Actor = token.GetReservedEntity();
		if (!m_Actor) return;
		m_ActorId = m_Actor.GetID();
		m_sActorRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_Actor);
	}

	AICF_VehicleBoardingActionToken GetToken() { return m_Token; }
	SCR_AIMoveIndividuallyBehavior GetAction() { return m_Action; }

	protected bool IsOwnerSafe()
	{
		if (!m_Token || !m_Actor || m_Actor.GetID() != m_ActorId ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_Actor) != m_sActorRpl ||
			m_Token.GetReservedEntity() != m_Actor || !m_Token.GetAgent()) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Actor);
		return character && character.GetAIControlComponent() &&
			character.GetAIControlComponent().GetControlAIAgent() == m_Token.GetAgent() &&
			m_Token.GetAgent().GetParentGroup() == m_Group && m_Token.IsPhysicalMutationOwnerSafe();
	}

	bool IsCurrent()
	{
		if (m_bStopped || !Replication.IsServer() || !m_Trip || !m_Trip.IsValid() || m_Trip.IsTerminal() ||
			m_Trip.GetPhase() != AICF_ETransportTripPhase.BOARDING || !m_Trip.IsVehiclePresetCurrent()) return false;
		if (m_Trip.GetLease() != m_Lease || m_Trip.GetBoardingState() != m_State ||
			!m_State || m_State.AreExitEffectsApplied() || m_Trip.IsDeadlineReached(System.GetTickCount()) ||
			System.GetTickCount() >= m_State.GetAbsoluteDeadlineMs()) return false;
		if (!IsOwnerSafe() ||
			m_Token.WasPresetQueued() || !m_Token.Matches(m_Trip, m_Lease, m_Group) ||
			!m_State.GetTokens() || m_State.GetTokens().FindByAgent(m_Token.GetAgent()) != m_Token ||
			!m_Token.IsExactCompartmentMutationSafe()) return false;
		ChimeraCharacter actor = ChimeraCharacter.Cast(m_Actor);
		if (!actor || !SCR_AIDamageHandling.IsConscious(actor)) return false;
		CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
		return access && !actor.IsInVehicle() && !CompartmentAccessComponent.GetVehicleIn(actor) &&
			!access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim();
	}

	static bool ShouldIssue(bool current, bool issued, float distanceMeters, int nowMs, int deadlineMs)
	{
		return current && !issued && distanceMeters > RADIUS_METERS && nowMs >= 0 && deadlineMs > nowMs;
	}

	void Tick()
	{
		if (!IsCurrent())
		{
			Stop();
			return;
		}
		Vehicle vehicle = m_Lease.GetVehicle();
		float distance = vector.DistanceXZ(m_Actor.GetOrigin(), vehicle.GetOrigin());
		if (!ShouldIssue(true, m_bIssued, distance, System.GetTickCount(), m_State.GetAbsoluteDeadlineMs())) return;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(m_Token.GetAgent().FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != m_Actor || !IsCurrent()) return;
		m_Action = new SCR_AIMoveIndividuallyBehavior(utility, null, vehicle.GetOrigin(),
			SCR_AIActionBase.PRIORITY_BEHAVIOR_MOVE_INDIVIDUALLY, SCR_AIActionBase.PRIORITY_LEVEL_PLAYER,
			vehicle, RADIUS_METERS);
		m_bIssued = true;
		utility.AddAction(m_Action);
		AICF_Stage3Diagnostics.Info("VEHICLE_PRESET_QUEUE_APPROACH", string.Format(
			"operation_id=%1 member=%2 seat=%3 distance_m=%4 radius_m=%5 deadline_ms=%6 action_owned=%7 entry_queued=0 physical_relocation=0",
			m_Trip.GetOperationId(), AICF_GroupTrace.Entity(m_Actor), AICF_VehiclePreset.SeatKey(m_Token.GetCompartment()),
			distance, RADIUS_METERS, m_State.GetAbsoluteDeadlineMs(), m_Token.IsActionOwnedByUtility(m_Action)));
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		if (m_Action && IsOwnerSafe() && m_Token.IsActionOwnedByUtility(m_Action))
		{
			EAIActionState state = m_Action.GetActionState();
			if (state != EAIActionState.COMPLETED && state != EAIActionState.FAILED) m_Action.Fail();
		}
		m_Action = null;
	}
}
