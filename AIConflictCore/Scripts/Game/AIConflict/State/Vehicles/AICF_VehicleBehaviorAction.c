// Собственная native action с immutable actor/asset identity. Complete/Fail
// освобождают action, но не заменяют physical postcondition в flow.
class AICF_VehicleBehaviorAction
{
	AICF_TransportTrip m_Trip;
	AICF_VehicleLease m_Lease;
	SCR_AIGroup m_Group;
	IEntity m_Actor;
	EntityID m_ActorId;
	string m_sActorRpl;
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	string m_sVehicleRpl;
	BaseCompartmentSlot m_Seat;
	int m_iGroupGeneration;
	int m_iTripGeneration;
	int m_iLeaseGeneration;
	int m_iVehicleGeneration;
	ref SCR_AIGetInVehicle m_Entry;
	ref SCR_AIGetOutVehicle m_Exit;
	vector m_vLastProgressPosition;
	int m_iLastProgressAt;
	int m_iRetries;
	int m_iShortAttempts;

	void AICF_VehicleBehaviorAction(AICF_TransportTrip trip, IEntity actor, BaseCompartmentSlot seat)
	{
		m_Trip = trip;
		m_Lease = trip.GetLease();
		m_Group = trip.GetAssignment().GetGroup();
		m_Actor = actor;
		m_ActorId = actor.GetID();
		m_sActorRpl = AICF_VehicleBehaviorBinding.ComponentRpl(actor);
		m_Vehicle = m_Lease.GetVehicle();
		m_VehicleId = m_Vehicle.GetID();
		m_sVehicleRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_Vehicle);
		m_Seat = seat;
		m_iGroupGeneration = trip.GetGroupGeneration();
		m_iTripGeneration = trip.GetTripGeneration();
		m_iLeaseGeneration = m_Lease.GetLeaseGeneration();
		m_iVehicleGeneration = m_Lease.GetVehicleGeneration();
		m_vLastProgressPosition = actor.GetOrigin();
		m_iLastProgressAt = System.GetTickCount();
	}

	SCR_AIUtilityComponent ResolveOwner()
	{
		if (!Replication.IsServer() || !m_Trip || !m_Trip.GetAssignment() || !m_Lease || !m_Group || !m_Actor || !m_Vehicle ||
			m_Trip.GetLease() != m_Lease || m_Trip.GetAssignment().GetGroup() != m_Group ||
			m_Trip.GetGroupGeneration() != m_iGroupGeneration || m_Trip.GetTripGeneration() != m_iTripGeneration) return null;
		if (!m_Lease.HasPhysicalAsset() || m_Lease.GetLeaseGeneration() != m_iLeaseGeneration ||
			m_Lease.GetVehicleGeneration() != m_iVehicleGeneration ||
			!m_Lease.MatchesTripIdentity(m_Trip.GetFactionKey(), m_Trip.GetSlotId(), m_iGroupGeneration, m_iTripGeneration) ||
			!m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)) return null;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Actor) ||
			m_Actor.GetID() != m_ActorId || AICF_VehicleBehaviorBinding.ComponentRpl(m_Actor) != m_sActorRpl) return null;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Actor);
		if (!character || !character.GetAIControlComponent()) return null;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent || agent.GetControlledEntity() != m_Actor || agent.GetParentGroup() != m_Group) return null;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != m_Actor) return null;
		return utility;
	}

	bool OwnsAction(SCR_AIUtilityComponent utility, SCR_AIVehicleBehavior action)
	{
		if (!utility || !action || action.m_Utility != utility || action.m_Vehicle.m_Value != m_Vehicle) return false;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		return actions.Contains(action);
	}

	bool IsSettled()
	{
		if (!ResolveOwner() || !m_Seat || m_Seat.GetVehicle() != m_Vehicle || m_Seat.GetOccupant() != m_Actor) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Actor);
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return access && access.IsInCompartment() && character.IsInVehicle() && access.GetCompartment() == m_Seat &&
			!access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim() &&
			CompartmentAccessComponent.GetVehicleIn(m_Actor) == m_Vehicle;
	}

	bool EntryBlocked()
	{
		SCR_AIUtilityComponent utility = ResolveOwner();
		if (!utility) return false;
		if (!m_Entry || !OwnsAction(utility, m_Entry) || m_Entry.GetRemoveAction() ||
			m_Entry.GetActionState() == EAIActionState.FAILED || m_Entry.GetActionState() == EAIActionState.COMPLETED) return true;
		if (vector.DistanceXZ(m_vLastProgressPosition, m_Actor.GetOrigin()) >= 0.25)
		{
			m_vLastProgressPosition = m_Actor.GetOrigin();
			m_iLastProgressAt = System.GetTickCount();
		}
		// Native failure обрабатывается сразу; RUNNING без движения — отдельное
		// короткое наблюдение, animated entry не прерывается по одному poll.
		CompartmentAccessComponent access = ChimeraCharacter.Cast(m_Actor).GetCompartmentAccessComponent();
		return access && !access.IsGettingIn() && !access.IsGettingOut() &&
			System.GetTickCount() - m_iLastProgressAt >= 2000;
	}

	void Retire()
	{
		SCR_AIUtilityComponent utility = ResolveOwner();
		if (!utility) { m_Entry = null; m_Exit = null; return; }
		CompartmentAccessComponent access = ChimeraCharacter.Cast(m_Actor).GetCompartmentAccessComponent();
		bool transitioning = access && (access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim());
		bool linked = CompartmentAccessComponent.GetVehicleIn(m_Actor) != null;
		if (m_Entry && OwnsAction(utility, m_Entry) && m_Entry.m_CompartmentToGetIn.m_Value == m_Seat)
		{
			if (m_Entry.GetActionState() != EAIActionState.FAILED && m_Entry.GetActionState() != EAIActionState.COMPLETED)
			{
				if (IsSettled()) m_Entry.Complete();
				else if (!linked && !transitioning) m_Entry.Fail();
				else m_Entry.SetRemoveAction(true);
			}
		}
		if (m_Exit && OwnsAction(utility, m_Exit) && m_Exit.GetActionState() != EAIActionState.FAILED && m_Exit.GetActionState() != EAIActionState.COMPLETED)
			m_Exit.SetRemoveAction(true);
		// Native Complete/Fail освобождают reservation сами. После их invokers
		// она уже может принадлежать новому intent того же actor: не трогаем её.
		// Linked/transitioning SetRemoveAction оставляет физическую связь native.
		m_Entry = null;
		m_Exit = null;
	}
}
