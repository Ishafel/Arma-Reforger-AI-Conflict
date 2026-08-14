// Exact Pilot/Turret recovery side effect owned only by VehicleTransitFlow.
// The full immutable fence prevents a delayed token from mutating a replacement
// group, trip, lease or physical vehicle generation.
class AICF_VehicleCrewRecoveryToken
{
	protected ref AICF_VehicleAsyncFence m_Fence;
	protected AIAgent m_Agent;
	protected IEntity m_ReservedEntity;
	protected Vehicle m_TargetVehicle;
	protected ref SCR_AIGetInVehicle m_Action;
	protected BaseCompartmentSlot m_Compartment;
	protected EAICompartmentType m_Role;
	protected int m_iAttempt;
	protected int m_iIssuedAtMs;
	protected int m_iAbsoluteDeadlineMs;
	protected int m_iLastTelemetryAtMs;
	protected bool m_bHasTransitionSample;
	protected EAIActionState m_LastActionState;
	protected bool m_bLastLinked;
	protected bool m_bLastGettingIn;
	protected bool m_bLastGettingOut;

	void AICF_VehicleCrewRecoveryToken(
		AICF_VehicleAsyncFence fence,
		AIAgent agent,
		IEntity reservedEntity,
		Vehicle targetVehicle,
		SCR_AIGetInVehicle action,
		BaseCompartmentSlot compartment,
		EAICompartmentType role,
		int attempt,
		int issuedAtMs,
		int absoluteDeadlineMs)
	{
		m_Fence = fence;
		m_Agent = agent;
		m_ReservedEntity = reservedEntity;
		m_TargetVehicle = targetVehicle;
		m_Action = action;
		m_Compartment = compartment;
		m_Role = role;
		m_iAttempt = attempt;
		m_iIssuedAtMs = issuedAtMs;
		m_iAbsoluteDeadlineMs = absoluteDeadlineMs;
	}

	AICF_VehicleAsyncFence GetFence() { return m_Fence; }
	string GetActionToken()
	{
		string actionToken;
		if (m_Fence)
			actionToken = m_Fence.GetToken();
		return actionToken;
	}
	AIAgent GetAgent() { return m_Agent; }
	IEntity GetReservedEntity() { return m_ReservedEntity; }
	Vehicle GetTargetVehicle() { return m_TargetVehicle; }
	SCR_AIGetInVehicle GetAction() { return m_Action; }
	BaseCompartmentSlot GetCompartment() { return m_Compartment; }
	EAICompartmentType GetRole() { return m_Role; }
	int GetAttempt() { return m_iAttempt; }
	int GetIssuedAtMs() { return m_iIssuedAtMs; }
	int GetAbsoluteDeadlineMs() { return m_iAbsoluteDeadlineMs; }

	bool Matches(AICF_TransportTrip trip, AICF_VehicleLease lease, SCR_AIGroup group)
	{
		return m_Fence && m_Fence.MatchesTrip(trip) && m_Fence.MatchesLease(lease) &&
			m_TargetVehicle && lease && lease.GetVehicle() == m_TargetVehicle &&
			m_Agent && m_Agent.GetParentGroup() == group &&
			m_Agent.GetControlledEntity() == m_ReservedEntity;
	}

	bool IsOwnerValid()
	{
		if (!m_Agent || m_Agent.GetControlledEntity() != m_ReservedEntity ||
			!AICF_GroupRuntime.IsAliveCharacter(m_ReservedEntity))
			return false;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			m_Agent.FindComponent(SCR_AIUtilityComponent));
		return utility && utility.m_OwnerEntity == m_ReservedEntity;
	}

	bool IsDeadlineReached(int nowMs)
	{
		return m_iAbsoluteDeadlineMs > 0 && nowMs >= m_iAbsoluteDeadlineMs;
	}

	int GetTransitionAgeMs(int nowMs)
	{
		return Math.Max(0, nowMs - m_iIssuedAtMs);
	}

	EAIActionState GetActionState()
	{
		if (!m_Action)
			return EAIActionState.FAILED;
		return m_Action.GetActionState();
	}

	bool ObserveTransitionChange(
		out bool linked,
		out bool gettingIn,
		out bool gettingOut,
		out EAIActionState actionState)
	{
		linked = m_ReservedEntity && m_TargetVehicle &&
			CompartmentAccessComponent.GetVehicleIn(m_ReservedEntity) == m_TargetVehicle;
		gettingIn = false;
		gettingOut = false;
		CompartmentAccessComponent access = ResolveAccess();
		if (access)
		{
			gettingIn = access.IsGettingIn();
			gettingOut = access.IsGettingOut();
		}
		actionState = GetActionState();
		bool changed = !m_bHasTransitionSample || actionState != m_LastActionState ||
			linked != m_bLastLinked || gettingIn != m_bLastGettingIn ||
			gettingOut != m_bLastGettingOut;
		m_bHasTransitionSample = true;
		m_LastActionState = actionState;
		m_bLastLinked = linked;
		m_bLastGettingIn = gettingIn;
		m_bLastGettingOut = gettingOut;
		return changed;
	}

	bool MarkTelemetryDue(bool changed, int nowMs, int intervalMs)
	{
		if (!changed && m_iLastTelemetryAtMs > 0 && nowMs - m_iLastTelemetryAtMs < intervalMs)
			return false;
		m_iLastTelemetryAtMs = nowMs;
		return true;
	}

	bool IsExactCompartmentSettled(AICF_VehicleWatchdog watchdog)
	{
		if (!watchdog || !m_Compartment || !m_ReservedEntity ||
			!watchdog.IsMemberSettledInVehicle(m_ReservedEntity, m_TargetVehicle))
			return false;
		CompartmentAccessComponent access = ResolveAccess();
		return access && access.GetCompartment() == m_Compartment;
	}

	bool IsReservationOwned()
	{
		return m_Compartment && m_ReservedEntity &&
			m_Compartment.IsReservedBy(m_ReservedEntity);
	}

	int GetAssignedManagerId()
	{
		if (m_Compartment)
			return m_Compartment.GetCompartmentMgrID();
		return -1;
	}

	int GetAssignedSlotId()
	{
		if (m_Compartment)
			return m_Compartment.GetCompartmentSlotID();
		return -1;
	}

	int GetActualManagerId()
	{
		CompartmentAccessComponent access = ResolveAccess();
		if (access && access.GetCompartment())
			return access.GetCompartment().GetCompartmentMgrID();
		return -1;
	}

	int GetActualSlotId()
	{
		CompartmentAccessComponent access = ResolveAccess();
		if (access && access.GetCompartment())
			return access.GetCompartment().GetCompartmentSlotID();
		return -1;
	}

	// Stock Fail() dereferences its utility owner. Never invoke it after the
	// agent detached, linked to the target vehicle, or entered a transition.
	void CancelOwnerSafe()
	{
		CompartmentAccessComponent access = ResolveAccess();
		bool linked = m_ReservedEntity && m_TargetVehicle &&
			CompartmentAccessComponent.GetVehicleIn(m_ReservedEntity) == m_TargetVehicle;
		bool transitioning = access && (access.IsGettingIn() || access.IsGettingOut());
		if (m_Action && IsOwnerValid() && !linked && !transitioning)
		{
			EAIActionState state = m_Action.GetActionState();
			if (state != EAIActionState.COMPLETED && state != EAIActionState.FAILED)
				m_Action.Fail();
		}
		ReleaseReservationOwnerSafe();
		m_Action = null;
	}

	void ReleaseTrackingOwnerSafe()
	{
		ReleaseReservationOwnerSafe();
		m_Action = null;
	}

	protected void ReleaseReservationOwnerSafe()
	{
		if (m_Compartment && m_ReservedEntity && m_Compartment.IsReservedBy(m_ReservedEntity))
			m_Compartment.SetReserved(null);
	}

	protected CompartmentAccessComponent ResolveAccess()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_ReservedEntity);
		if (!character)
			return null;
		return character.GetCompartmentAccessComponent();
	}
}
