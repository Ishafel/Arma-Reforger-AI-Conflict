// Только исходный живой AI, занявший чужое место в той же текущей машине.
// Scope повторяется после native interrupt; собственного retry/clock здесь нет.
class AICF_ReboardSeatConflictScope
{
	AICF_TransportTrip m_Trip;
	AICF_VehicleBehaviorBinding m_Binding;
	AICF_StrategicAssignmentSnapshot m_Assignment;
	AICF_VehicleBehaviorState m_State;
	BaseCompartmentSlot m_BlockedSeat;
	BaseCompartmentSlot m_AssignedSeat;
	IEntity m_Expected;
	IEntity m_Occupant;
	EntityID m_ExpectedId;
	EntityID m_OccupantId;
	string m_sExpectedRpl;
	string m_sOccupantRpl;
	int m_iExpectedPosition;
	int m_iOccupantPosition;
	int m_iBlockedIndex;
	int m_iAssignedIndex;
	int m_iReboardDeadline;
	int m_iAssignmentRevision;
	int m_iIntentRevision;
	int m_iTransitionCount;
	bool m_bCapturedOwnReservation;
	bool m_bReleasedBeforeExit;
	string m_sBlockedKey;
	string m_sAssignedKey;

	static AICF_ReboardSeatConflictScope Capture(AICF_TransportTrip trip, array<BaseCompartmentSlot> seats,
		BaseCompartmentSlot blocked, IEntity expected)
	{
		if (!trip || !trip.GetLease() || !seats || !blocked || !expected || !blocked.GetOccupant()) return null;
		AICF_ReboardSeatConflictScope scope = new AICF_ReboardSeatConflictScope();
		scope.m_Trip = trip;
		scope.m_Binding = AICF_VehicleBehaviorPolicy.Find(trip.GetLease().GetVehicle());
		scope.m_Assignment = trip.GetAssignment();
		scope.m_State = trip.GetBehaviorState();
		scope.m_BlockedSeat = blocked;
		scope.m_Expected = expected;
		scope.m_Occupant = blocked.GetOccupant();
		scope.m_bCapturedOwnReservation = blocked.IsReservedBy(scope.m_Occupant);
		scope.m_ExpectedId = expected.GetID();
		scope.m_OccupantId = scope.m_Occupant.GetID();
		scope.m_sExpectedRpl = AICF_VehicleBehaviorBinding.ComponentRpl(expected);
		scope.m_sOccupantRpl = AICF_VehicleBehaviorBinding.ComponentRpl(scope.m_Occupant);
		scope.m_iExpectedPosition = -1;
		scope.m_iOccupantPosition = -1;
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
		{
			if (trip.VehiclePresetMember(position) == expected) scope.m_iExpectedPosition = position;
			if (trip.VehiclePresetMember(position) == scope.m_Occupant) scope.m_iOccupantPosition = position;
		}
		scope.m_iBlockedIndex = seats.Find(blocked);
		if (!trip.GetVehiclePreset() || scope.m_iOccupantPosition < 0) return null;
		scope.m_iAssignedIndex = trip.GetVehiclePreset().m_aMembers.Find(scope.m_iOccupantPosition);
		if (scope.m_iAssignedIndex < 0 || scope.m_iAssignedIndex >= seats.Count()) return null;
		scope.m_AssignedSeat = seats[scope.m_iAssignedIndex];
		scope.m_sBlockedKey = AICF_VehiclePreset.SeatKey(blocked);
		scope.m_sAssignedKey = AICF_VehiclePreset.SeatKey(scope.m_AssignedSeat);
		scope.m_iReboardDeadline = trip.GetBehaviorState().m_iReboardDeadline;
		if (!scope.m_Assignment) return null;
		scope.m_iAssignmentRevision = scope.m_Assignment.GetAssignmentRevision();
		scope.m_iIntentRevision = scope.m_Assignment.GetStrategicIntentRevision();
		scope.m_iTransitionCount = trip.GetTransitionCount();
		if (!scope.IsCurrent()) return null;
		return scope;
	}

	protected bool IsOwnFaction(IEntity member)
	{
		if (!member) return false;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(member.FindComponent(FactionAffiliationComponent));
		Faction faction;
		if (affiliation) faction = affiliation.GetAffiliatedFaction();
		return faction && faction.GetFactionKey() == m_Trip.GetFactionKey();
	}

	bool IsContextCurrent()
	{
		if (!m_Trip || !AICF_VehicleBehaviorPolicy.IsCurrentReboarding(m_Binding) || m_Binding.m_Trip != m_Trip) return false;
		if (AICF_VehicleBehaviorPolicy.Find(m_Binding.m_Vehicle) != m_Binding) return false;
		if (m_Trip.GetAssignment() != m_Assignment || m_Trip.GetBehaviorState().m_iReboardDeadline != m_iReboardDeadline) return false;
		if (m_Trip.GetBehaviorState() != m_State || m_Trip.GetTransitionCount() != m_iTransitionCount) return false;
		if (m_Assignment.GetAssignmentRevision() != m_iAssignmentRevision || m_Assignment.GetStrategicIntentRevision() != m_iIntentRevision) return false;
		return true;
	}

	protected bool AreMembersCurrent()
	{
		if (!IsContextCurrent()) return false;
		if (!m_Expected || !m_Occupant || m_Expected == m_Occupant || m_iExpectedPosition < 0 || m_iOccupantPosition < 0) return false;
		if (m_Expected.GetID() != m_ExpectedId || m_Occupant.GetID() != m_OccupantId ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_Expected) != m_sExpectedRpl ||
			AICF_VehicleBehaviorBinding.ComponentRpl(m_Occupant) != m_sOccupantRpl) return false;
		if (m_sExpectedRpl.IsEmpty() || m_sExpectedRpl == "NONE" || m_sOccupantRpl.IsEmpty() || m_sOccupantRpl == "NONE") return false;
		if (!AICF_VehicleShortRecovery.IsCurrentMember(m_Trip, m_Expected) ||
			!AICF_VehicleShortRecovery.IsCurrentMember(m_Trip, m_Occupant) || !IsOwnFaction(m_Expected) || !IsOwnFaction(m_Occupant)) return false;
		if (m_Trip.VehiclePresetMember(m_iExpectedPosition) != m_Expected || m_Trip.VehiclePresetMember(m_iOccupantPosition) != m_Occupant) return false;
		AICF_VehiclePreset preset = m_Trip.GetVehiclePreset();
		if (!preset.m_aMembers.IsIndexValid(m_iBlockedIndex) || !preset.m_aMembers.IsIndexValid(m_iAssignedIndex) ||
			preset.m_aMembers[m_iBlockedIndex] != m_iExpectedPosition || preset.m_aMembers[m_iAssignedIndex] != m_iOccupantPosition ||
			m_iAssignedIndex == m_iBlockedIndex || !m_AssignedSeat || m_AssignedSeat == m_BlockedSeat) return false;
		Vehicle vehicle = m_Binding.m_Vehicle;
		if (!m_BlockedSeat || m_BlockedSeat.GetVehicle() != vehicle || m_AssignedSeat.GetVehicle() != vehicle ||
			!AICF_VehicleShortRecovery.IsSlowVehicle(vehicle)) return false;
		if (AICF_VehiclePreset.SeatKey(m_BlockedSeat) != m_sBlockedKey || AICF_VehiclePreset.SeatKey(m_AssignedSeat) != m_sAssignedKey) return false;
		if (SCR_AIVehicleUsability.VehicleIsOnFire(vehicle) || !SCR_AIVehicleUsability.VehicleCanMove(vehicle)) return false;
		return true;
	}

	bool IsCurrent()
	{
		if (!AreMembersCurrent() || m_BlockedSeat.GetOccupant() != m_Occupant) return false;
		CompartmentAccessComponent access = ChimeraCharacter.Cast(m_Occupant).GetCompartmentAccessComponent();
		return access && access.GetCompartment() == m_BlockedSeat && access.IsInCompartment() &&
			CompartmentAccessComponent.GetVehicleIn(m_Occupant) == m_Binding.m_Vehicle;
	}

	protected bool IsExitedActorCurrent()
	{
		if (!m_State || m_State.m_ReboardSeatConflict != this) return false;
		if (!AreMembersCurrent()) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Occupant);
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return false;
		IEntity linked = CompartmentAccessComponent.GetVehicleIn(m_Occupant);
		if (linked && linked != m_Binding.m_Vehicle) return false;
		if (!linked) return !character.IsInVehicle() && !access.IsInCompartment() && !access.GetCompartment();
		return character.IsInVehicle() && access.IsInCompartment() && access.GetCompartment() == m_AssignedSeat &&
			m_AssignedSeat.GetOccupant() == m_Occupant;
	}

	protected bool IsAfterExitCurrent()
	{
		return IsExitedActorCurrent() && !m_BlockedSeat.GetOccupant();
	}

	bool IsBeforeExitCurrent()
	{
		if (!IsCurrent() || !m_State || m_State.m_ReboardSeatConflict) return false;
		CompartmentAccessComponent access = ChimeraCharacter.Cast(m_Occupant).GetCompartmentAccessComponent();
		return access && !access.IsGettingIn() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim();
	}

	bool CanBeginOwnedExit()
	{
		if (!IsBeforeExitCurrent() || HasLiveWrongSeatEntry()) return false;
		return IsBeforeExitCurrent();
	}

	// Stock GetIn.OnActionCompleted также снимает reservation занятого места.
	// Здесь occupancy остаётся у того же captured wrong actor до native GetOut.
	bool ReleaseOwnedReservationBeforeExit()
	{
		if (!CanBeginOwnedExit()) return false;
		if (!m_BlockedSeat.IsReserved()) return true;
		// Fresh correct-Pilot reservation is preserved; it authorizes no clear.
		if (m_BlockedSeat.IsReservedBy(m_Expected)) return IsBeforeExitCurrent();
		if (!m_bCapturedOwnReservation || !m_BlockedSeat.IsReservedBy(m_Occupant)) return false;
		AICF_Stage3Diagnostics.Info("VEHICLE_REBOARD_OWN_RESERVATION", string.Format(
			"operation_id=%1 occupant=%2 seat=%3 captured_own_reservation=1 before_clear_own=1 occupied_same=1 native_wrong_entry=0 phase=BEFORE_EXIT_CLEAR readonly=1",
			m_Trip.GetOperationId(), AICF_GroupTrace.Entity(m_Occupant), m_sBlockedKey));
		if (!CanBeginOwnedExit()) return false;
		if (!IsBeforeExitCurrent() || !m_bCapturedOwnReservation || !m_BlockedSeat.IsReservedBy(m_Occupant)) return false;
		m_BlockedSeat.SetReserved(null);
		if (!IsBeforeExitCurrent() || m_BlockedSeat.IsReserved()) return false;
		m_bReleasedBeforeExit = true;
		AICF_Stage3Diagnostics.Info("VEHICLE_REBOARD_OWN_RESERVATION", string.Format(
			"operation_id=%1 occupant=%2 seat=%3 captured_own_reservation=1 after_clear_unreserved=1 occupied_same=1 phase=AFTER_EXIT_CLEAR readonly=1",
			m_Trip.GetOperationId(), AICF_GroupTrace.Entity(m_Occupant), m_sBlockedKey));
		return true;
	}

	protected bool HasLiveWrongSeatEntry()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Occupant);
		if (!character || !character.GetAIControlComponent()) return true;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent || agent.GetControlledEntity() != m_Occupant) return true;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != m_Occupant) return true;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		if (!IsContextCurrent()) return true;
		foreach (AIActionBase action : actions)
		{
			SCR_AIGetInVehicle entry = SCR_AIGetInVehicle.Cast(action);
			if (!entry || entry.m_Utility != utility || entry.m_Vehicle.m_Value != m_Binding.m_Vehicle ||
				entry.m_CompartmentToGetIn.m_Value != m_BlockedSeat) continue;
			if (!entry.GetRemoveAction() && entry.GetActionState() != EAIActionState.COMPLETED && entry.GetActionState() != EAIActionState.FAILED) return true;
		}
		return false;
	}

	// -1: потерян owner; 0: только bounded native ожидание; 1: actual exit и
	// освобождённая собственная reservation. Не трогает чужие/new-intent места.
	int ObserveExitedReservation()
	{
		if (!m_State || m_State.m_ReboardSeatConflict != this) return -1;
		if (!AreMembersCurrent()) return -1;
		CompartmentAccessComponent access = ChimeraCharacter.Cast(m_Occupant).GetCompartmentAccessComponent();
		if (!access) return -1;
		if (access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return 0;
		if (m_BlockedSeat.GetOccupant() == m_Occupant) return 0;
		if (m_bReleasedBeforeExit)
		{
			if (!IsExitedActorCurrent()) return -1;
			if (HasLiveWrongSeatEntry())
			{
				if (!IsExitedActorCurrent()) return -1;
				return 0;
			}
			if (!IsExitedActorCurrent()) return -1;
			if (m_BlockedSeat.GetOccupant() && m_BlockedSeat.GetOccupant() != m_Expected) return -1;
			// Новый корректный Pilot intent не принадлежит старому receipt.
			if (m_BlockedSeat.IsReserved() && !m_BlockedSeat.IsReservedBy(m_Expected)) return -1;
			return 1;
		}
		if (!IsAfterExitCurrent()) return -1;
		if (HasLiveWrongSeatEntry())
		{
			if (!IsAfterExitCurrent()) return -1;
			return 0;
		}
		if (!IsAfterExitCurrent()) return -1;
		if (!m_BlockedSeat.IsReserved()) return 1;
		if (m_BlockedSeat.IsReservedBy(m_Expected)) return 1;
		if (!m_bCapturedOwnReservation || !m_BlockedSeat.IsReservedBy(m_Occupant)) return -1;
		AICF_Stage3Diagnostics.Info("VEHICLE_REBOARD_OWN_RESERVATION", string.Format(
			"operation_id=%1 occupant=%2 seat=%3 captured_own_reservation=%4 before_clear_own=1 empty=1 native_wrong_entry=0 phase=BEFORE_CLEAR readonly=1",
			m_Trip.GetOperationId(), AICF_GroupTrace.Entity(m_Occupant), m_sBlockedKey, m_bCapturedOwnReservation));
		if (!IsAfterExitCurrent() || HasLiveWrongSeatEntry()) return -1;
		if (!IsAfterExitCurrent() || !m_bCapturedOwnReservation || !m_BlockedSeat.IsReservedBy(m_Occupant)) return -1;
		m_BlockedSeat.SetReserved(null);
		if (!IsContextCurrent() || !m_State || m_State.m_ReboardSeatConflict != this) return -1;
		bool cleared = !m_BlockedSeat.GetOccupant() && !m_BlockedSeat.IsReserved();
		AICF_Stage3Diagnostics.Info("VEHICLE_REBOARD_OWN_RESERVATION", string.Format(
			"operation_id=%1 occupant=%2 seat=%3 captured_own_reservation=%4 after_clear_unreserved=%5 phase=AFTER_CLEAR readonly=1",
			m_Trip.GetOperationId(), AICF_GroupTrace.Entity(m_Occupant), m_sBlockedKey, m_bCapturedOwnReservation, cleared));
		if (!cleared) return 0;
		return 1;
	}
}
