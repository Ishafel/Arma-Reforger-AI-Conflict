// Physical action mutation is allowed only for a live authoritative AI entity.
// The possessing-manager lookup covers both directly controlled and player-main
// entities, so a possession handoff cannot turn a stale AI token into a player
// GetOut/teleport side effect through SCR_AIGetInVehicle.OnActionFailed().
class AICF_VehicleBoardingMutationFence
{
	static bool IsAuthoritativeAIEntity(IEntity entity)
	{
		if (!Replication.IsServer() || !AICF_GroupRuntime.IsAliveCharacter(entity) ||
			!GetGame())
			return false;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || playerManager.GetPlayerIdFromControlledEntity(entity) > 0 ||
			SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(entity) > 0)
		{
			return false;
		}
		return IsAuthoritativeReplicatedEntity(entity);
	}

	static bool IsAuthoritativeReplicatedEntity(IEntity entity)
	{
		if (!Replication.IsServer() || !entity)
			return false;
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		return rpl && rpl.IsMaster() && !rpl.IsProxy();
	}
}

// One exact asynchronous boarding side effect. The immutable fence prevents a
// stale group/trip/lease/vehicle generation from mutating a replacement trip.
// Reservations are released only while the original entity still owns them.
class AICF_VehicleBoardingActionToken
{
	protected ref AICF_VehicleAsyncFence m_Fence;
	protected AIAgent m_Agent;
	protected IEntity m_ReservedEntity;
	protected Vehicle m_TargetVehicle;
	protected ref SCR_AIMoveIndividuallyBehavior m_ApproachAction;
	protected ref SCR_AIGetInVehicle m_GetInAction;
	protected BaseCompartmentSlot m_Compartment;
	protected EAICompartmentType m_CompartmentType;
	protected int m_iRetryCount;
	protected int m_iIssuedAtMs;
	protected int m_iLastProgressAtMs;
	protected float m_fBestDistanceMeters = -1.0;
	protected float m_fCurrentDistanceMeters = -1.0;
	protected bool m_bHasTransitionSample;
	protected EAIActionState m_LastActionState;
	protected bool m_bLastLinked;
	protected bool m_bLastGettingIn;
	protected bool m_bLastGettingOut;
	protected bool m_bHiddenExactSeatRecoveryPending;
	protected bool m_bHiddenExactSeatRecoveryAttempted;
	protected int m_iHiddenExactSeatRecoveryScheduledAtMs;
	protected bool m_bAnimatedExactSeatRecoveryAttempted;
	protected bool m_bAnimatedExactSeatRecoveryAccepted;
	protected int m_iAnimatedExactSeatRecoveryAtMs;
	protected int m_iAnimatedExactSeatRecoveryDoorIndex = -1;
	protected bool m_bVisibleCrewRotationAttempted;
	protected string m_sRecoveryFence = "NOT_EVALUATED";

	void AICF_VehicleBoardingActionToken(
		AICF_VehicleAsyncFence fence,
		AIAgent agent,
		IEntity reservedEntity,
		Vehicle targetVehicle,
		SCR_AIMoveIndividuallyBehavior approachAction,
		SCR_AIGetInVehicle getInAction,
		BaseCompartmentSlot compartment,
		EAICompartmentType compartmentType,
		int retryCount,
		float initialDistanceMeters)
	{
		m_Fence = fence;
		m_Agent = agent;
		m_ReservedEntity = reservedEntity;
		m_TargetVehicle = targetVehicle;
		m_ApproachAction = approachAction;
		m_GetInAction = getInAction;
		m_Compartment = compartment;
		m_CompartmentType = compartmentType;
		m_iRetryCount = retryCount;
		m_iIssuedAtMs = System.GetTickCount();
		m_iLastProgressAtMs = m_iIssuedAtMs;
		m_fBestDistanceMeters = initialDistanceMeters;
		m_fCurrentDistanceMeters = initialDistanceMeters;
	}

	AICF_VehicleAsyncFence GetFence() { return m_Fence; }
	string GetActionToken()
	{
		if (m_Fence)
			return m_Fence.GetToken();
		return string.Empty;
	}
	AIAgent GetAgent() { return m_Agent; }
	IEntity GetReservedEntity() { return m_ReservedEntity; }
	Vehicle GetTargetVehicle() { return m_TargetVehicle; }
	SCR_AIMoveIndividuallyBehavior GetApproachAction() { return m_ApproachAction; }
	SCR_AIGetInVehicle GetGetInAction() { return m_GetInAction; }
	BaseCompartmentSlot GetCompartment() { return m_Compartment; }
	EAICompartmentType GetCompartmentType() { return m_CompartmentType; }
	int GetRetryCount() { return m_iRetryCount; }
	int GetIssuedAtMs() { return m_iIssuedAtMs; }
	float GetBestDistanceMeters() { return m_fBestDistanceMeters; }
	float GetCurrentDistanceMeters() { return m_fCurrentDistanceMeters; }
	string GetRecoveryFence() { return m_sRecoveryFence; }

	bool RecordRecoveryFence(string recoveryFence)
	{
		if (recoveryFence.IsEmpty())
			recoveryFence = "NOT_EVALUATED";
		bool changed = recoveryFence != m_sRecoveryFence;
		m_sRecoveryFence = recoveryFence;
		return changed;
	}

	bool Matches(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group)
	{
		if (!m_Fence || !m_Fence.MatchesTrip(trip) || !m_Fence.MatchesLease(lease) ||
			!MatchesLiveTargetIdentity() || lease.GetVehicle() != m_TargetVehicle ||
			!m_Agent || m_Agent.GetParentGroup() != group ||
			m_Agent.GetControlledEntity() != m_ReservedEntity)
		{
			return false;
		}
		return AICF_GroupRuntime.IsAliveCharacter(m_ReservedEntity);
	}

	bool IsOwnerValid()
	{
		if (!m_Agent || m_Agent.GetControlledEntity() != m_ReservedEntity ||
			!AICF_GroupRuntime.IsAliveCharacter(m_ReservedEntity))
		{
			return false;
		}
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			m_Agent.FindComponent(SCR_AIUtilityComponent));
		return utility && utility.m_OwnerEntity == m_ReservedEntity;
	}

	bool IsPhysicalMutationOwnerSafe()
	{
		return IsOwnerValid() &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_ReservedEntity) &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_TargetVehicle);
	}

	bool IsExactCompartmentTarget()
	{
		if (!m_GetInAction)
			return m_Compartment == null;
		return m_Compartment && m_TargetVehicle &&
			m_Compartment.GetVehicle() == m_TargetVehicle;
	}

	bool IsExactCompartmentMutationSafe()
	{
		return m_GetInAction && m_Compartment && IsExactCompartmentTarget() &&
			m_Compartment.IsCompartmentAccessible() &&
			!m_Compartment.GetOccupant() && m_ReservedEntity &&
			m_Compartment.IsReservedBy(m_ReservedEntity) &&
			!m_Compartment.IsGetInLockedFor(m_ReservedEntity);
	}

	// A non-teleport exact-seat recovery is permitted only when every immutable
	// ownership condition is still true and the stock action owns the utility,
	// yet has not begun a compartment transition near an available door.
	bool IsReadyExactCargoWithoutTransition(float maximumDistanceMeters)
	{
		return IsReadyExactSeatWithoutTransition(
			maximumDistanceMeters,
			EAICompartmentType.Cargo);
	}

	bool IsReadyExactSeatWithoutTransition(
		float maximumDistanceMeters,
		EAICompartmentType requiredType)
	{
		if (maximumDistanceMeters <= 0 || m_fCurrentDistanceMeters < 0 ||
			m_fCurrentDistanceMeters > maximumDistanceMeters ||
			m_CompartmentType != requiredType ||
			!MatchesLiveTargetIdentity() || !IsPhysicalMutationOwnerSafe() ||
			!IsExactCompartmentMutationSafe() || !IsTrackedActionCurrent() ||
			!IsTrackedActionOwnedByUtility())
		{
			return false;
		}
		CompartmentAccessComponent access = ResolveAccess(m_ReservedEntity);
		if (!access || access.IsInCompartment() || access.IsGettingIn() ||
			access.IsGettingOut() ||
			CompartmentAccessComponent.GetVehicleIn(m_ReservedEntity))
		{
			return false;
		}
		array<int> doorIndices = {};
		return m_Compartment.GetAvailableDoorIndices(doorIndices) > 0;
	}

	// Replace a stalled stock behavior with a new tracked SCR_AIGetInVehicle.
	// Calling CompartmentAccessComponent.GetInVehicle(false) beside the still
	// current behavior lets the engine deselect the tracked action and release
	// its exact reservation without beginning a compartment transition. Failing
	// the old action first follows the stock lifecycle, after which this token
	// reacquires the same exact seat and owns the replacement behavior.
	bool ReissueTrackedExactSeatAction(
		out bool replacementOwned,
		out int doorIndex)
	{
		replacementOwned = false;
		doorIndex = -1;
		if (m_bAnimatedExactSeatRecoveryAttempted ||
			!MatchesLiveTargetIdentity() || !IsPhysicalMutationOwnerSafe() ||
			!IsExactCompartmentMutationSafe() || !IsTrackedActionCurrent() ||
			!IsTrackedActionOwnedByUtility())
		{
			return false;
		}
		CompartmentAccessComponent access = ResolveAccess(m_ReservedEntity);
		if (!access || access.IsInCompartment() || access.IsGettingIn() ||
			access.IsGettingOut() ||
			CompartmentAccessComponent.GetVehicleIn(m_ReservedEntity))
		{
			return false;
		}
		array<int> doorIndices = {};
		if (m_Compartment.GetAvailableDoorIndices(doorIndices) <= 0)
		{
			return false;
		}
		doorIndex = doorIndices[0];
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			m_Agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != m_ReservedEntity)
			return false;

		SCR_AIGetInVehicle previousAction = m_GetInAction;
		previousAction.Fail();
		if (!m_Compartment.IsCompartmentAccessible() ||
			m_Compartment.GetOccupant() ||
			m_Compartment.IsGetInLockedFor(m_ReservedEntity) ||
			(m_Compartment.IsReserved() &&
			!m_Compartment.IsReservedBy(m_ReservedEntity)))
		{
			return false;
		}
		if (!m_Compartment.IsReservedBy(m_ReservedEntity))
			m_Compartment.SetReserved(m_ReservedEntity);
		if (!m_Compartment.IsReservedBy(m_ReservedEntity))
			return false;

		SCR_AIGetInVehicle replacementAction = new SCR_AIGetInVehicle(
			utility,
			null,
			m_TargetVehicle,
			m_Compartment,
			m_CompartmentType,
			SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE,
			SCR_AIActionBase.PRIORITY_LEVEL_NORMAL);
		m_GetInAction = replacementAction;
		m_bAnimatedExactSeatRecoveryAttempted = true;
		m_iAnimatedExactSeatRecoveryAtMs = System.GetTickCount();
		m_iAnimatedExactSeatRecoveryDoorIndex = doorIndex;
		m_iRetryCount++;
		m_iLastProgressAtMs = m_iAnimatedExactSeatRecoveryAtMs;
		utility.AddAction(replacementAction);
		replacementOwned = IsActionOwnedByUtility(replacementAction);
		m_bAnimatedExactSeatRecoveryAccepted = replacementOwned;
		return true;
	}

	bool WasAnimatedExactSeatRecoveryAttempted()
	{
		return m_bAnimatedExactSeatRecoveryAttempted;
	}

	bool WasAnimatedExactSeatRecoveryAccepted()
	{
		return m_bAnimatedExactSeatRecoveryAccepted;
	}

	int GetAnimatedExactSeatRecoveryAgeMs()
	{
		if (!m_bAnimatedExactSeatRecoveryAttempted ||
			m_iAnimatedExactSeatRecoveryAtMs <= 0)
		{
			return 0;
		}
		return System.GetTickCount(m_iAnimatedExactSeatRecoveryAtMs);
	}

	int GetAnimatedExactSeatRecoveryDoorIndex()
	{
		return m_iAnimatedExactSeatRecoveryDoorIndex;
	}

	bool MarkVisibleCrewRotationAttempted()
	{
		if (m_bVisibleCrewRotationAttempted)
			return false;
		m_bVisibleCrewRotationAttempted = true;
		return true;
	}

	// A repeated RUNNING stall arms exactly one forced exact-seat operation.
	// Scheduling and applying are deliberately split across scheduler ticks so
	// player proximity and all physical ownership fences are sampled afresh.
	bool ScheduleHiddenExactSeatRecovery()
	{
		if (m_bHiddenExactSeatRecoveryPending || m_bHiddenExactSeatRecoveryAttempted ||
			!IsSupportedExactSeatType())
		{
			return false;
		}
		m_bHiddenExactSeatRecoveryPending = true;
		m_iHiddenExactSeatRecoveryScheduledAtMs = System.GetTickCount();
		return true;
	}

	bool IsHiddenExactSeatRecoveryPending()
	{
		return m_bHiddenExactSeatRecoveryPending;
	}

	bool WasHiddenExactSeatRecoveryAttempted()
	{
		return m_bHiddenExactSeatRecoveryAttempted;
	}

	int GetHiddenExactSeatRecoveryPendingAgeMs()
	{
		if (!m_bHiddenExactSeatRecoveryPending ||
			m_iHiddenExactSeatRecoveryScheduledAtMs <= 0)
		{
			return 0;
		}
		return System.GetTickCount(m_iHiddenExactSeatRecoveryScheduledAtMs);
	}

	bool ApplyHiddenExactSeatRecovery()
	{
		if (!m_bHiddenExactSeatRecoveryPending || m_bHiddenExactSeatRecoveryAttempted ||
			!IsSupportedExactSeatType() ||
			!MatchesLiveTargetIdentity() || !IsPhysicalMutationOwnerSafe() ||
			!IsExactCompartmentTarget())
		{
			return false;
		}
		bool stockActionOwned = IsTrackedActionCurrent() &&
			IsTrackedActionOwnedByUtility();
		if (!stockActionOwned && !m_bAnimatedExactSeatRecoveryAttempted)
			return false;

		CompartmentAccessComponent access = ResolveAccess(m_ReservedEntity);
		if (!access || access.IsInCompartment() ||
			CompartmentAccessComponent.GetVehicleIn(m_ReservedEntity))
		{
			return false;
		}
		bool transitioning = access.IsGettingIn() || access.IsGettingOut();
		if (transitioning)
		{
			if (!m_bAnimatedExactSeatRecoveryAttempted)
				return false;
			// The tracked replacement received its full observation window. This
			// interrupt is reached only after the caller has passed the player/LOS
			// fence for the hidden one-shot correction.
			access.InterruptVehicleActionQueue(true, true, true);
		}
		if (!m_Compartment.IsCompartmentAccessible() ||
			m_Compartment.GetOccupant() ||
			m_Compartment.IsGetInLockedFor(m_ReservedEntity) ||
			(m_Compartment.IsReserved() &&
			!m_Compartment.IsReservedBy(m_ReservedEntity)))
		{
			return false;
		}
		if (!m_Compartment.IsReservedBy(m_ReservedEntity))
			m_Compartment.SetReserved(m_ReservedEntity);
		if (!m_Compartment.IsReservedBy(m_ReservedEntity))
			return false;

		// Consume the allowance before invoking the engine mutation. A rejected
		// GetInVehicle call must never be retried by this token/trip identity.
		m_bHiddenExactSeatRecoveryPending = false;
		m_bHiddenExactSeatRecoveryAttempted = true;
		return access.GetInVehicle(
			m_TargetVehicle,
			m_Compartment,
			true,
			-1,
			ECloseDoorAfterActions.INVALID,
			false);
	}

	bool ObserveSpatialProgress(float distanceMeters, float minimumProgressMeters)
	{
		m_fCurrentDistanceMeters = distanceMeters;
		if (distanceMeters < 0 || m_fBestDistanceMeters < 0 ||
			distanceMeters > m_fBestDistanceMeters - minimumProgressMeters)
		{
			return false;
		}
		m_fBestDistanceMeters = distanceMeters;
		m_iLastProgressAtMs = System.GetTickCount();
		return true;
	}

	bool IsTrackedActionCurrent()
	{
		if (!m_Agent)
			return false;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			m_Agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != m_ReservedEntity)
			return false;
		AIActionBase currentAction = utility.GetCurrentAction();
		if (m_GetInAction)
			return currentAction == m_GetInAction;
		if (m_ApproachAction)
			return currentAction == m_ApproachAction;
		return false;
	}

	bool IsTrackedActionOwnedByUtility()
	{
		AIActionBase tracked = m_GetInAction;
		if (!tracked)
			tracked = m_ApproachAction;
		return IsActionOwnedByUtility(tracked);
	}

	bool IsActionOwnedByUtility(AIActionBase expectedAction)
	{
		if (!m_Agent || !expectedAction)
			return false;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			m_Agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != m_ReservedEntity)
			return false;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		return actions.Contains(expectedAction);
	}

	int GetProgressAgeMs()
	{
		return System.GetTickCount(m_iLastProgressAtMs);
	}

	int GetTransitionAgeMs()
	{
		return System.GetTickCount(m_iIssuedAtMs);
	}

	EAIActionState GetActionState()
	{
		if (m_GetInAction)
			return m_GetInAction.GetActionState();
		if (m_ApproachAction)
			return m_ApproachAction.GetActionState();
		return EAIActionState.FAILED;
	}

	bool ObserveTransitionChange(
		Vehicle vehicle,
		out bool linked,
		out bool gettingIn,
		out bool gettingOut,
		out EAIActionState actionState)
	{
		linked = false;
		gettingIn = false;
		gettingOut = false;
		actionState = GetActionState();
		CompartmentAccessComponent access = ResolveAccess(m_ReservedEntity);
		if (m_ReservedEntity && vehicle)
			linked = CompartmentAccessComponent.GetVehicleIn(m_ReservedEntity) == vehicle;
		if (access)
		{
			gettingIn = access.IsGettingIn();
			gettingOut = access.IsGettingOut();
		}
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

	bool IsExactCompartmentSettled(AICF_VehicleWatchdog watchdog, Vehicle vehicle)
	{
		if (!watchdog || !m_Compartment || !m_ReservedEntity ||
			!watchdog.IsMemberSettledInVehicle(m_ReservedEntity, vehicle))
		{
			return false;
		}
		CompartmentAccessComponent access = ResolveAccess(m_ReservedEntity);
		return access && access.GetCompartment() == m_Compartment;
	}

	// Never fail an action after its character linked to the target vehicle or
	// while a compartment transition is in progress. Stock action failure may
	// eject an already-mounted character and can dereference a detached owner.
	void CancelOwnerSafe()
	{
		// An ABA/live replicated-identity mismatch invalidates ownership. Drop
		// only this token's references; never fail an action or mutate a seat on
		// an entity whose current EntityID/RplId is no longer the fenced pair.
		if (!MatchesLiveTargetIdentity())
		{
			m_ApproachAction = null;
			m_GetInAction = null;
			m_Compartment = null;
			return;
		}
		CompartmentAccessComponent access = ResolveAccess(m_ReservedEntity);
		IEntity linkedVehicle;
		if (m_ReservedEntity)
			linkedVehicle = CompartmentAccessComponent.GetVehicleIn(m_ReservedEntity);
		bool linked = linkedVehicle != null;
		bool transitioning = access && (access.IsGettingIn() || access.IsGettingOut());
		// An action that has already disappeared from the utility is orphaned even
		// if its object still reports RUNNING.  Never call Fail on that stale
		// action; exact reservation release below is still live-identity fenced and
		// lets the supervisor issue a fresh action safely.
		if (IsPhysicalMutationOwnerSafe() && !linked && !transitioning)
		{
			// Failing a still-owned action is an action-lifecycle operation, not a
			// seat mutation. Do not require the exact reservation here: the engine
			// can drop that reservation while leaving SCR_AIGetInVehicle RUNNING.
			// Keeping such an action alive across terminal infantry handoff lets the
			// member enter the vehicle after the trip already fell back to foot.
			if (m_GetInAction && IsActionOwnedByUtility(m_GetInAction))
			{
				EAIActionState getInState = m_GetInAction.GetActionState();
				if (getInState != EAIActionState.COMPLETED && getInState != EAIActionState.FAILED)
					m_GetInAction.Fail();
			}
			if (m_ApproachAction && IsActionOwnedByUtility(m_ApproachAction))
			{
				EAIActionState approachState = m_ApproachAction.GetActionState();
				if (approachState != EAIActionState.COMPLETED && approachState != EAIActionState.FAILED)
					m_ApproachAction.Fail();
			}
		}
		ReleaseReservationOwnerSafe();
	}

	void ReleaseTrackingOwnerSafe()
	{
		ReleaseReservationOwnerSafe();
		m_ApproachAction = null;
		m_GetInAction = null;
	}

	void ReleaseReservationOwnerSafe()
	{
		if (!MatchesLiveTargetIdentity())
			return;
		if (m_Compartment && m_TargetVehicle &&
			m_Compartment.GetVehicle() == m_TargetVehicle && m_ReservedEntity &&
			m_Compartment.IsReservedBy(m_ReservedEntity))
			m_Compartment.SetReserved(null);
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
		CompartmentAccessComponent access = ResolveAccess(m_ReservedEntity);
		if (access && access.GetCompartment())
			return access.GetCompartment().GetCompartmentMgrID();
		return -1;
	}

	int GetActualSlotId()
	{
		CompartmentAccessComponent access = ResolveAccess(m_ReservedEntity);
		if (access && access.GetCompartment())
			return access.GetCompartment().GetCompartmentSlotID();
		return -1;
	}

	protected CompartmentAccessComponent ResolveAccess(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return null;
		return character.GetCompartmentAccessComponent();
	}

	protected bool IsSupportedExactSeatType()
	{
		return m_CompartmentType == EAICompartmentType.Pilot ||
			m_CompartmentType == EAICompartmentType.Turret ||
			m_CompartmentType == EAICompartmentType.Cargo;
	}

	protected bool MatchesLiveTargetIdentity()
	{
		if (!m_Fence || !m_TargetVehicle ||
			m_TargetVehicle.GetID() != m_Fence.GetEntityId())
		{
			return false;
		}
		RplComponent rpl = RplComponent.Cast(
			m_TargetVehicle.FindComponent(RplComponent));
		return rpl && rpl.Id().ToString() == m_Fence.GetRplId();
	}
}

// Immutable member -> Cargo seat mapping for one passenger phase. A plan entry
// can wait behind a foreign/stale reservation without preventing ready pairs
// from receiving their exact actions. It never clears a reservation owned by
// another entity.
class AICF_VehiclePassengerSeatPlanEntry
{
	protected ref AICF_VehicleAsyncFence m_Fence;
	protected AIAgent m_Agent;
	protected IEntity m_Entity;
	protected Vehicle m_Vehicle;
	protected BaseCompartmentSlot m_Compartment;

	void AICF_VehiclePassengerSeatPlanEntry(
		AICF_VehicleAsyncFence fence,
		AIAgent agent,
		IEntity entity,
		Vehicle vehicle,
		BaseCompartmentSlot compartment)
	{
		m_Fence = fence;
		m_Agent = agent;
		m_Entity = entity;
		m_Vehicle = vehicle;
		m_Compartment = compartment;
	}

	AICF_VehicleAsyncFence GetFence() { return m_Fence; }
	AIAgent GetAgent() { return m_Agent; }
	IEntity GetEntity() { return m_Entity; }
	Vehicle GetVehicle() { return m_Vehicle; }
	BaseCompartmentSlot GetCompartment() { return m_Compartment; }

	bool MatchesIdentity(
		AICF_TransportTrip trip,
		AICF_VehicleLease lease,
		SCR_AIGroup group)
	{
		return m_Fence && m_Fence.MatchesTrip(trip) && m_Fence.MatchesLease(lease) &&
			m_Agent && m_Agent.GetParentGroup() == group &&
			m_Agent.GetControlledEntity() == m_Entity && m_Vehicle && lease &&
			lease.GetVehicle() == m_Vehicle && m_Compartment &&
			m_Compartment.GetVehicle() == m_Vehicle &&
			CargoCompartmentSlot.Cast(m_Compartment) != null;
	}

	bool IsAliveCurrentMember(SCR_AIGroup group)
	{
		return m_Agent && m_Agent.GetParentGroup() == group &&
			m_Agent.GetControlledEntity() == m_Entity &&
			AICF_GroupRuntime.IsAliveCharacter(m_Entity);
	}

	bool IsExactCompartmentSettled(AICF_VehicleWatchdog watchdog)
	{
		if (!watchdog || !m_Entity || !m_Vehicle || !m_Compartment ||
			!watchdog.IsMemberSettledInVehicle(m_Entity, m_Vehicle))
		{
			return false;
		}
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Entity);
		CompartmentAccessComponent access;
		if (character)
			access = character.GetCompartmentAccessComponent();
		return access && access.GetCompartment() == m_Compartment;
	}

	bool IsSeatReadyForAction()
	{
		if (!m_Entity || !m_Compartment || !m_Vehicle ||
			m_Compartment.GetVehicle() != m_Vehicle ||
			!m_Compartment.IsCompartmentAccessible() ||
			m_Compartment.GetOccupant() ||
			m_Compartment.IsGetInLockedFor(m_Entity))
		{
			return false;
		}
		return !m_Compartment.IsReserved() || m_Compartment.IsReservedBy(m_Entity);
	}

	void ReleaseReservationOwnerSafe()
	{
		if (m_Compartment && m_Vehicle &&
			m_Compartment.GetVehicle() == m_Vehicle && m_Entity &&
			m_Compartment.IsReservedBy(m_Entity))
		{
			m_Compartment.SetReserved(null);
		}
	}
}

class AICF_VehiclePassengerSeatPlan
{
	protected ref array<ref AICF_VehiclePassengerSeatPlanEntry> m_aEntries = {};

	int Count() { return m_aEntries.Count(); }

	AICF_VehiclePassengerSeatPlanEntry Get(int index)
	{
		if (!m_aEntries.IsIndexValid(index))
			return null;
		return m_aEntries[index];
	}

	bool Track(AICF_VehiclePassengerSeatPlanEntry entry, int maximumEntries = 16)
	{
		if (!entry || maximumEntries < 1 || m_aEntries.Count() >= maximumEntries)
			return false;
		m_aEntries.Insert(entry);
		return true;
	}

	AICF_VehiclePassengerSeatPlanEntry FindByAgent(AIAgent agent)
	{
		foreach (AICF_VehiclePassengerSeatPlanEntry entry : m_aEntries)
		{
			if (entry && entry.GetAgent() == agent)
				return entry;
		}
		return null;
	}

	void ReleaseReservationsOwnerSafe()
	{
		foreach (AICF_VehiclePassengerSeatPlanEntry entry : m_aEntries)
		{
			if (entry)
				entry.ReleaseReservationOwnerSafe();
		}
		m_aEntries.Clear();
	}
}

// Sole owner of the concrete action references for one boarding phase. The
// enclosing VehicleBoardingState is reset on the authoritative phase exit.
class AICF_VehicleBoardingTokenSet
{
	protected ref array<ref AICF_VehicleBoardingActionToken> m_aTokens = {};
	protected int m_iNextActionSequence = 1;

	int Count() { return m_aTokens.Count(); }

	AICF_VehicleBoardingActionToken Get(int index)
	{
		if (!m_aTokens.IsIndexValid(index))
			return null;
		return m_aTokens[index];
	}

	bool Track(AICF_VehicleBoardingActionToken token, int maximumTokens = 16)
	{
		if (!token || maximumTokens < 1 || m_aTokens.Count() >= maximumTokens)
			return false;
		m_aTokens.Insert(token);
		return true;
	}

	AICF_VehicleBoardingActionToken FindByAgent(AIAgent agent)
	{
		foreach (AICF_VehicleBoardingActionToken token : m_aTokens)
		{
			if (token && token.GetAgent() == agent)
				return token;
		}
		return null;
	}

	void Remove(AICF_VehicleBoardingActionToken expected)
	{
		if (expected)
			m_aTokens.RemoveItem(expected);
	}

	int NextActionSequence()
	{
		int sequence = m_iNextActionSequence;
		m_iNextActionSequence++;
		return sequence;
	}

	void CancelAllOwnerSafe()
	{
		for (int index = m_aTokens.Count() - 1; index >= 0; index--)
		{
			AICF_VehicleBoardingActionToken token = m_aTokens[index];
			if (token)
				token.CancelOwnerSafe();
		}
		m_aTokens.Clear();
	}
}
