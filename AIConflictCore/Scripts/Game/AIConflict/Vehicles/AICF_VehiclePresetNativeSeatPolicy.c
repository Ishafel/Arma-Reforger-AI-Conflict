// Слабая связь с живым trip: phase/lease/preset остаются у прежних владельцев.
class AICF_VehiclePresetNativeSeatBinding
{
	AICF_TransportTrip m_Trip;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	string m_sGroupRpl;
	AICF_VehicleLease m_Lease;
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	string m_sVehicleRpl;
	string m_sOperation;
	int m_iGroupGeneration;
	int m_iTripGeneration;
	int m_iLeaseGeneration;
	int m_iVehicleGeneration;
	int m_iPresetRevision;
	ref array<IEntity> m_aMembers = {};
	ref array<EntityID> m_aMemberIds = {};
	ref array<string> m_aMemberRpls = {};
	ref array<int> m_aPositions = {};
	ref array<string> m_aSeatKeys = {};

	static string ComponentRpl(IEntity entity)
	{
		if (!entity) return "NONE";
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl) return "NONE";
		return rpl.Id().ToString();
	}

	static AICF_VehiclePresetNativeSeatBinding Capture(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() ||
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.GetVehiclePreset() ||
			!trip.IsVehiclePresetCurrent() || !trip.GetLease() || !trip.GetLease().HasPhysicalAsset()) return null;
		AICF_VehiclePresetNativeSeatBinding binding = new AICF_VehiclePresetNativeSeatBinding();
		binding.m_Trip = trip;
		binding.m_Group = trip.GetAssignment().GetGroup();
		binding.m_Lease = trip.GetLease();
		binding.m_Vehicle = binding.m_Lease.GetVehicle();
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(binding.m_Group) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(binding.m_Vehicle)) return null;
		binding.m_GroupId = binding.m_Group.GetID();
		binding.m_sGroupRpl = ComponentRpl(binding.m_Group);
		binding.m_VehicleId = binding.m_Vehicle.GetID();
		binding.m_sVehicleRpl = ComponentRpl(binding.m_Vehicle);
		binding.m_sOperation = trip.GetOperationId();
		binding.m_iGroupGeneration = trip.GetGroupGeneration();
		binding.m_iTripGeneration = trip.GetTripGeneration();
		binding.m_iLeaseGeneration = binding.m_Lease.GetLeaseGeneration();
		binding.m_iVehicleGeneration = binding.m_Lease.GetVehicleGeneration();
		binding.m_iPresetRevision = trip.GetVehiclePresetRevision();
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(binding.m_Vehicle, seats);
		if (!preset.MatchesSeats(seats)) return null;
		for (int index; index < seats.Count(); index++)
		{
			int position = preset.m_aMembers[index];
			if (position < 0) continue;
			IEntity member = trip.VehiclePresetMember(position);
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) return null;
			binding.m_aMembers.Insert(member);
			binding.m_aMemberIds.Insert(member.GetID());
			binding.m_aMemberRpls.Insert(ComponentRpl(member));
			binding.m_aPositions.Insert(position);
			binding.m_aSeatKeys.Insert(AICF_VehiclePreset.SeatKey(seats[index]));
		}
		if (!binding.IsCurrent()) return null;
		return binding;
	}

	bool IsCurrent()
	{
		if (!Replication.IsServer() || !m_Trip || !m_Trip.IsValid() || m_Trip.IsTerminal() ||
			m_Trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || m_Trip.IsDeadlineReached(System.GetTickCount()) ||
			m_Trip.GetOperationId() != m_sOperation || m_Trip.GetGroupGeneration() != m_iGroupGeneration ||
			m_Trip.GetTripGeneration() != m_iTripGeneration || !m_Trip.GetVehiclePreset() ||
			m_Trip.GetVehiclePresetRevision() != m_iPresetRevision || !m_Trip.IsVehiclePresetCurrent()) return false;
		AICF_GroupSlot slot = m_Trip.GetVehiclePresetSlot();
		AICF_StrategicAssignmentSnapshot assignment = m_Trip.GetAssignment();
		if (!slot || !slot.IsCombatReady() ||
			slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision() ||
			slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()) return false;
		if (m_Trip.GetAssignment().GetGroup() != m_Group ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) || m_Group.GetID() != m_GroupId ||
			ComponentRpl(m_Group) != m_sGroupRpl || m_Trip.GetLease() != m_Lease || !m_Lease || !m_Lease.HasPhysicalAsset() ||
			m_Lease.GetLeaseGeneration() != m_iLeaseGeneration || m_Lease.GetVehicleGeneration() != m_iVehicleGeneration ||
			!m_Lease.MatchesTripIdentity(m_Trip.GetFactionKey(), m_Trip.GetSlotId(), m_iGroupGeneration, m_iTripGeneration) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle) || m_Vehicle.GetID() != m_VehicleId ||
			ComponentRpl(m_Vehicle) != m_sVehicleRpl || !m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)) return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || usage.GetDamageState() == EDamageState.DESTROYED || SCR_AIVehicleUsability.VehicleIsOnFire(m_Vehicle) ||
			!SCR_AIVehicleUsability.VehicleCanMove(m_Vehicle)) return false;
		vector transform[4];
		m_Vehicle.GetWorldTransform(transform);
		if (transform[1][1] < 0.25) return false;
		if (m_aMembers.IsEmpty()) return false;
		for (int index; index < m_aMembers.Count(); index++)
		{
			IEntity member = m_aMembers[index];
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member) || member.GetID() != m_aMemberIds[index] ||
				!SCR_AIDamageHandling.IsConscious(member) ||
				ComponentRpl(member) != m_aMemberRpls[index] || m_Trip.VehiclePresetMember(m_aPositions[index]) != member ||
				m_Trip.GetVehiclePresetSlot().GetRosterMember(m_aPositions[index]) != member) return false;
			ChimeraCharacter character = ChimeraCharacter.Cast(member);
			if (!character || !character.GetAIControlComponent()) return false;
			AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
			if (!agent || agent.GetControlledEntity() != member || agent.GetParentGroup() != m_Group) return false;
			IEntity linkedVehicle = CompartmentAccessComponent.GetVehicleIn(member);
			if (linkedVehicle && linkedVehicle != m_Vehicle) return false;
		}
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(m_Vehicle, seats);
		AICF_VehiclePreset preset = m_Trip.GetVehiclePreset();
		if (!preset.MatchesSeats(seats)) return false;
		for (int seatIndex; seatIndex < seats.Count(); seatIndex++)
		{
			IEntity occupant = seats[seatIndex].GetOccupant();
			if (!occupant) continue;
			int position = preset.m_aMembers[seatIndex];
			if (position < 0 || m_Trip.VehiclePresetMember(position) != occupant) return false;
		}
		return true;
	}

	BaseCompartmentSlot FindSeat(IEntity member)
	{
		int index = m_aMembers.Find(member);
		if (index < 0 || !IsCurrent()) return null;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(m_Vehicle, seats);
		AICF_VehiclePreset preset = m_Trip.GetVehiclePreset();
		if (!preset.MatchesSeats(seats)) return null;
		for (int seatIndex; seatIndex < seats.Count(); seatIndex++)
			if (AICF_VehiclePreset.SeatKey(seats[seatIndex]) == m_aSeatKeys[index] &&
				preset.m_aMembers[seatIndex] == m_aPositions[index]) return seats[seatIndex];
		return null;
	}
}

// Один синхронный dispatch. Исходное message не меняется и не rebroadcast-ится.
class AICF_VehiclePresetNativeSeatRequest
{
	ref AICF_VehiclePresetNativeSeatBinding m_Binding;
	SCR_AIUtilityComponent m_Utility;
	IEntity m_Member;
	AIAgent m_Agent;
	BaseCompartmentSlot m_Seat;
	ref SCR_AIActivityBase m_Activity;
	int m_iAssignmentRevision;
	int m_iRouteGeneration;
	bool m_bCreatedReservation;
	bool m_bDispatch;
	ref SCR_AIMessage_GetIn m_Message;

	bool IsCurrentContext()
	{
		return m_Binding && AICF_VehiclePresetNativeSeatPolicy.Contains(m_Binding) && m_Binding.IsCurrent() &&
			m_Utility && m_Utility.m_OwnerEntity == m_Member && m_Agent && m_Agent.GetControlledEntity() == m_Member &&
			SCR_AIUtilityComponent.Cast(m_Agent.FindComponent(SCR_AIUtilityComponent)) == m_Utility &&
			m_Agent.GetParentGroup() == m_Binding.m_Group && m_Seat && m_Seat.GetVehicle() == m_Binding.m_Vehicle &&
			m_Binding.FindSeat(m_Member) == m_Seat &&
			m_Binding.m_Trip.GetAssignment().GetAssignmentRevision() == m_iAssignmentRevision &&
			m_Binding.m_Trip.GetMovementState().GetRouteGeneration() == m_iRouteGeneration;
	}

	bool IsCurrent()
	{
		return IsCurrentContext() && AICF_VehiclePresetNativeSeatPolicy.IsCurrentActivity(m_Binding, m_Activity);
	}

	void RollbackReservation()
	{
		if (!m_bCreatedReservation || !IsCurrent() || !m_Seat || m_Seat.GetOccupant() || !m_Seat.IsReservedBy(m_Member)) return;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Member);
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim() ||
			CompartmentAccessComponent.GetVehicleIn(m_Member)) return;
		m_Seat.SetReserved(null);
		m_bCreatedReservation = false;
	}

	void Log(string reason)
	{
		string seatKey = "NONE";
		if (m_Seat) seatKey = AICF_VehiclePreset.SeatKey(m_Seat);
		AICF_Stage3Diagnostics.Info("VEHICLE_PRESET_NATIVE_SEAT", string.Format(
			"operation_id=%1 member=%2 vehicle=%3 seat=%4 reason=%5 assignment_revision=%6 route_generation=%7 dispatch=%8",
			m_Binding.m_sOperation, AICF_GroupTrace.Entity(m_Member), AICF_GroupTrace.Entity(m_Binding.m_Vehicle),
			seatKey, reason, m_iAssignmentRevision, m_iRouteGeneration, m_bDispatch));
	}
}

class AICF_VehiclePresetNativeSeatPolicy
{
	protected static ref array<ref AICF_VehiclePresetNativeSeatBinding> s_aBindings = {};

	static void Prune()
	{
		for (int index = s_aBindings.Count() - 1; index >= 0; index--)
			if (!s_aBindings[index] || !s_aBindings[index].m_Trip || s_aBindings[index].m_Trip.IsTerminal() ||
				!s_aBindings[index].m_Group || !s_aBindings[index].m_Lease || !s_aBindings[index].m_Vehicle) s_aBindings.Remove(index);
	}
	static bool Contains(AICF_VehiclePresetNativeSeatBinding binding) { return s_aBindings.Contains(binding); }
	static void Register(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip) return;
		Prune();
		foreach (AICF_VehiclePresetNativeSeatBinding current : s_aBindings)
			if (current.m_Trip == trip) return;
		AICF_VehiclePresetNativeSeatBinding binding = AICF_VehiclePresetNativeSeatBinding.Capture(trip);
		if (binding) s_aBindings.Insert(binding);
	}
	static void Unregister(AICF_TransportTrip trip)
	{
		for (int index = s_aBindings.Count() - 1; index >= 0; index--)
			if (!s_aBindings[index] || !s_aBindings[index].m_Trip || s_aBindings[index].m_Trip == trip) s_aBindings.Remove(index);
	}

	static bool IsSelectedActivity(AICF_VehiclePresetNativeSeatBinding binding, SCR_AIActivityBase activity)
	{
		if (!binding || !activity || !binding.m_Group) return false;
		SCR_AIGroupUtilityComponent utility = binding.m_Group.GetGroupUtilityComponent();
		if (!utility || utility.m_Owner != binding.m_Group || utility.GetCurrentAction() != activity ||
			activity.m_Utility != utility || activity.GetRemoveAction() || activity.GetActionIsSuspended()) return false;
		EAIActionState state = activity.GetActionState();
		if (state != EAIActionState.RUNNING && state != EAIActionState.EVALUATED) return false;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		return actions.Contains(activity) && utility.m_VehicleMgr && utility.m_VehicleMgr.FindVehicle(binding.m_Vehicle);
	}

	static bool IsCurrentActivity(AICF_VehiclePresetNativeSeatBinding binding, SCR_AIActivityBase activity)
	{
		if (!IsSelectedActivity(binding, activity)) return false;
		SCR_AIGetInActivity getIn = SCR_AIGetInActivity.Cast(activity);
		if (getIn) return getIn.m_Vehicle.m_Value == binding.m_Vehicle;
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(activity);
		AICF_VehicleMovementState movement = binding.m_Trip.GetMovementState();
		return move && movement.IsRouteWaypointBound() && movement.GetRouteWaypoint() &&
			move.m_RelatedWaypoint == movement.GetRouteWaypoint() && binding.m_Group.GetCurrentWaypoint() == movement.GetRouteWaypoint();
	}

	static bool IsGenuineOtherActivity(AICF_VehiclePresetNativeSeatBinding binding, SCR_AIActivityBase activity,
		SCR_AIMessage_GetIn message)
	{
		// Native VehicleCombat передаёт this в SendGetInMessage. NONE не
		// доказывает текущую задачу и может быть запоздалым route intent.
		return activity && message.m_RelatedGroupActivity == activity &&
			!SCR_AIGetInActivity.Cast(activity) && !SCR_AIMoveActivity.Cast(activity) && IsSelectedActivity(binding, activity);
	}

	static bool HasOwnedExactAction(AICF_VehiclePresetNativeSeatRequest request)
	{
		array<ref AIActionBase> actions = {};
		request.m_Utility.GetActions(actions);
		foreach (AIActionBase action : actions)
		{
			SCR_AIGetInVehicle getIn = SCR_AIGetInVehicle.Cast(action);
			if (getIn && getIn.m_Utility == request.m_Utility && !getIn.GetRemoveAction() && !getIn.GetActionIsSuspended() &&
				getIn.GetActionState() != EAIActionState.FAILED && getIn.GetActionState() != EAIActionState.COMPLETED &&
				getIn.m_Vehicle.m_Value == request.m_Binding.m_Vehicle && getIn.m_CompartmentToGetIn.m_Value == request.m_Seat) return true;
		}
		return false;
	}

	// null — unrelated native traffic; непустой result всегда handled, fail-closed.
	static AICF_VehiclePresetNativeSeatRequest Prepare(SCR_AIUtilityComponent utility, SCR_AIMessage_GetIn message)
	{
		if (!Replication.IsServer() || !utility || !message || !utility.m_OwnerEntity) return null;
		Prune();
		IEntity member = utility.m_OwnerEntity;
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		AIAgent agent;
		if (character && character.GetAIControlComponent()) agent = character.GetAIControlComponent().GetControlAIAgent();
		AICF_VehiclePresetNativeSeatBinding binding;
		bool ambiguous;
		foreach (AICF_VehiclePresetNativeSeatBinding candidate : s_aBindings)
		{
			bool currentGroup = candidate.m_Group && agent && agent.GetParentGroup() == candidate.m_Group;
			bool originalActorMessage = candidate.m_Group && candidate.m_aMembers.Find(member) >= 0;
			if (!currentGroup && !originalActorMessage) continue;
			if (binding) ambiguous = true;
			binding = candidate;
		}
		if (!binding) return null;
		SCR_AIActivityBase activity;
		if (binding.m_Group && binding.m_Group.GetGroupUtilityComponent())
			activity = SCR_AIActivityBase.Cast(binding.m_Group.GetGroupUtilityComponent().GetCurrentAction());
		AICF_VehiclePresetNativeSeatRequest request = new AICF_VehiclePresetNativeSeatRequest();
		request.m_Binding = binding;
		request.m_Utility = utility;
		request.m_Member = member;
		request.m_Agent = agent;
		request.m_Activity = activity;
		if (binding.m_Trip && binding.m_Trip.GetAssignment())
		{
			request.m_iAssignmentRevision = binding.m_Trip.GetAssignment().GetAssignmentRevision();
			request.m_iRouteGeneration = binding.m_Trip.GetMovementState().GetRouteGeneration();
		}
		request.m_Seat = binding.FindSeat(member);
		if (ambiguous || message.m_Vehicle != binding.m_Vehicle || !request.IsCurrentContext() ||
			message.GetSender() != binding.m_Group ||
			(message.GetReceiver() && message.GetReceiver() != agent && message.GetReceiver() != binding.m_Group) ||
			(message.m_RelatedGroupActivity && message.m_RelatedGroupActivity != activity))
		{ request.Log("CURRENT_MANUAL_CONTEXT_REJECTED"); return request; }
		if (!IsCurrentActivity(binding, activity))
		{
			if (IsGenuineOtherActivity(binding, activity, message)) return null;
			request.Log("CURRENT_MANUAL_ACTIVITY_REJECTED");
			return request;
		}
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		BaseCompartmentSlot seat = request.m_Seat;
		if (seat.GetOccupant() == member && access && access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() &&
			!access.IsSwitchingSeatsAnim() && access.GetCompartment() == seat && CompartmentAccessComponent.GetVehicleIn(member) == binding.m_Vehicle)
		{ request.Log("EXACT_MEMBER_ALREADY_SETTLED"); return request; }
		if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim() ||
			CompartmentAccessComponent.GetVehicleIn(member) || seat.GetOccupant() || !seat.IsCompartmentAccessible() ||
			seat.IsGetInLockedFor(member) || (seat.IsReserved() && !seat.IsReservedBy(member)))
		{ request.Log("EXACT_SEAT_OR_MEMBER_BUSY"); return request; }
		if (HasOwnedExactAction(request)) { request.Log("EXACT_OWNED_ACTION_ALREADY_PENDING"); return request; }
		if (!request.IsCurrent()) { request.Log("PRE_RESERVATION_IDENTITY_CHANGED"); return request; }
		request.m_bCreatedReservation = !seat.IsReserved();
		if (request.m_bCreatedReservation) seat.SetReserved(member);
		if (!request.IsCurrent() || !seat.IsReservedBy(member))
		{ request.RollbackReservation(); request.Log("RESERVATION_REJECTED"); return request; }
		request.m_Message = SCR_AIMessage_GetIn.Create(message.m_Vehicle, message.m_BoardingParams,
			SCR_AICompartmentHandling.CompartmentClassToType(seat.Type()), message.m_bIsWaypointRelated,
			message.m_fPriorityLevel, message.m_RelatedWaypoint, message.m_RelatedGroupActivity, seat);
		request.m_Message.SetReceiver(message.GetReceiver());
		request.m_Message.SetText(message.GetText());
		request.m_Message.SetPosition(message.GetPosition());
		request.m_Message.SetObject(message.GetObject());
#ifdef AI_DEBUG
		request.m_Message.m_sSentFromBt = message.m_sSentFromBt;
#endif
		request.m_bDispatch = true;
		request.Log("EXACT_SLOT_NATIVE_MESSAGE");
		return request;
	}
}
