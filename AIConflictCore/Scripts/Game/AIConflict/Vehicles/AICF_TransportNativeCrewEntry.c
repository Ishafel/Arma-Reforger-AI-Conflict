// Наблюдение чужой native action, не токен владения ею или её reservation.
// Engine entities/actions слабы; MovementState владеет только этим record.
class AICF_TransportNativeCrewEntry
{
	protected ref AICF_VehiclePresetNativeSeatBinding m_Binding;
	protected BaseCompartmentSlot m_Seat;
	protected string m_sSeatKey;
	protected IEntity m_Member;
	protected SCR_AIUtilityComponent m_Utility;
	protected SCR_AIGetInVehicle m_LastAction;
	protected EAICompartmentType m_Role;
	protected int m_iAssignmentRevision;
	protected int m_iIntentRevision;
	protected int m_iRouteGeneration;
	protected int m_iStartedAtMs;
	protected int m_iDeadlineMs;
	protected int m_iReportAtMs;

	static AICF_TransportNativeCrewEntry Capture(AICF_TransportTrip trip, BaseCompartmentSlot seat, EAICompartmentType role, int nowMs, int timeoutMs)
	{
		if (!seat || seat.GetOccupant() || timeoutMs <= 0) return null;
		if (role != EAICompartmentType.Pilot && role != EAICompartmentType.Turret) return null;
		if (role == EAICompartmentType.Pilot && seat.GetType() != ECompartmentType.PILOT) return null;
		if (role == EAICompartmentType.Turret && seat.GetType() != ECompartmentType.TURRET) return null;
		AICF_VehiclePresetNativeSeatBinding binding = AICF_VehiclePresetNativeSeatBinding.Capture(trip);
		if (!binding || seat.GetVehicle() != binding.m_Vehicle) return null;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(binding.m_Vehicle, seats);
		int index = seats.Find(seat);
		if (index < 0 || !trip.GetVehiclePreset().MatchesSeats(seats)) return null;
		int position = trip.GetVehiclePreset().m_aMembers[index];
		if (position < 0) return null;
		IEntity member = trip.VehiclePresetMember(position);
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member) || !seat.IsReservedBy(member)) return null;
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (!character || !character.GetAIControlComponent()) return null;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent || agent.GetControlledEntity() != member || agent.GetParentGroup() != binding.m_Group) return null;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != member) return null;
		AICF_TransportNativeCrewEntry entry = new AICF_TransportNativeCrewEntry();
		entry.m_Binding = binding;
		entry.m_Seat = seat;
		entry.m_sSeatKey = AICF_VehiclePreset.SeatKey(seat);
		entry.m_Member = member;
		entry.m_Utility = utility;
		entry.m_Role = role;
		entry.m_iAssignmentRevision = trip.GetAssignment().GetAssignmentRevision();
		entry.m_iIntentRevision = trip.GetAssignment().GetStrategicIntentRevision();
		entry.m_iRouteGeneration = trip.GetMovementState().GetRouteGeneration();
		entry.m_iStartedAtMs = nowMs;
		entry.m_iDeadlineMs = Math.Min(trip.GetAbsoluteDeadlineMs(), nowMs + timeoutMs);
		entry.m_iReportAtMs = nowMs;
		if (!entry.IsCurrentContext(trip) || entry.IsDeadlineReached(nowMs)) return null;
		entry.m_LastAction = entry.ResolveRunningAction();
		if (!entry.m_LastAction || !entry.IsCurrentContext(trip) || !seat.IsReservedBy(member)) return null;
		return entry;
	}

	EAICompartmentType GetRole() { return m_Role; }
	int GetStartedAtMs() { return m_iStartedAtMs; }
	int GetDeadlineMs() { return m_iDeadlineMs; }
	bool IsDeadlineReached(int nowMs) { return nowMs >= m_iDeadlineMs; }

	bool IsCurrentContext(AICF_TransportTrip trip)
	{
		if (!m_Binding || m_Binding.m_Trip != trip || !m_Binding.IsCurrent()) return false;
		if (trip.GetAssignment().GetAssignmentRevision() != m_iAssignmentRevision) return false;
		if (trip.GetAssignment().GetStrategicIntentRevision() != m_iIntentRevision) return false;
		if (trip.GetMovementState().GetRouteGeneration() != m_iRouteGeneration) return false;
		if (!m_Member || m_Binding.FindSeat(m_Member) != m_Seat || !m_Seat) return false;
		if (!m_Utility || m_Utility.m_OwnerEntity != m_Member) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Member);
		if (!character || !character.GetAIControlComponent()) return false;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent || agent.GetControlledEntity() != m_Member || agent.GetParentGroup() != m_Binding.m_Group) return false;
		if (SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent)) != m_Utility) return false;
		return true;
	}

	bool IsSettled()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Member);
		if (!character || m_Seat.GetOccupant() != m_Member) return false;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || !character.IsInVehicle() || !access.IsInCompartment()) return false;
		if (access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) return false;
		return access.GetCompartment() == m_Seat && CompartmentAccessComponent.GetVehicleIn(character) == m_Binding.m_Vehicle;
	}

	string PendingFailure()
	{
		IEntity occupant = m_Seat.GetOccupant();
		if (occupant && occupant != m_Member) return "NATIVE_CREW_ENTRY_FOREIGN_OCCUPANT";
		if (m_Seat.IsReserved() && !m_Seat.IsReservedBy(m_Member)) return "NATIVE_CREW_ENTRY_RESERVATION_CHANGED";
		SCR_AIGetInVehicle action = ResolveRunningAction();
		if (action)
		{
			if (!occupant && !m_Seat.IsReservedBy(m_Member)) return "NATIVE_CREW_ENTRY_RESERVATION_RELEASED";
			m_LastAction = action;
			return string.Empty;
		}
		if (m_LastAction && m_LastAction.GetActionState() == EAIActionState.FAILED) return "NATIVE_CREW_ENTRY_ACTION_FAILED";
		// Native completion и физическое завершение animation могут разделяться
		// одним scheduler tick. Этот WAIT остаётся в исходном окне, не новом grace.
		if (m_LastAction && m_LastAction.GetActionState() == EAIActionState.COMPLETED &&
			AICF_VehicleCrewRoleOwnership.IsCurrentExactPresetOccupant(m_Binding.m_Trip, m_Seat, m_Role, System.GetTickCount())) return string.Empty;
		return "NATIVE_CREW_ENTRY_ACTION_NOT_CURRENT";
	}

	protected SCR_AIGetInVehicle ResolveRunningAction()
	{
		SCR_AIGroupUtilityComponent groupUtility = m_Binding.m_Group.GetGroupUtilityComponent();
		if (!groupUtility) return null;
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(groupUtility.GetCurrentAction());
		if (!AICF_VehiclePresetNativeSeatPolicy.IsCurrentActivity(m_Binding, activity)) return null;
		array<ref AIActionBase> actions = {};
		m_Utility.GetActions(actions);
		SCR_AIGetInVehicle current = SCR_AIGetInVehicle.Cast(m_Utility.GetCurrentAction());
		SCR_AIGetInVehicle executed = SCR_AIGetInVehicle.Cast(m_Utility.GetExecutedAction());
		foreach (AIActionBase candidate : actions)
		{
			SCR_AIGetInVehicle action = SCR_AIGetInVehicle.Cast(candidate);
			if (!action || (action != current && action != executed) || action.m_Utility != m_Utility) continue;
			if (action.GetActionState() != EAIActionState.RUNNING || action.GetRemoveAction() || action.GetActionIsSuspended()) continue;
			if (action.m_Vehicle.m_Value != m_Binding.m_Vehicle || action.m_CompartmentToGetIn.m_Value != m_Seat) continue;
			if (action.GetRelatedGroupActivity() && action.GetRelatedGroupActivity() != activity) continue;
			return action;
		}
		return null;
	}

	bool MarkReportDue(int nowMs)
	{
		if (nowMs - m_iReportAtMs < 5000) return false;
		m_iReportAtMs = nowMs;
		return true;
	}
	string FormatDetails(int nowMs)
	{
		return string.Format(" role=%1 member=%2 seat=%3 first_observed_ms=%4 entry_deadline_ms=%5 entry_age_ms=%6 action=%7 ownership=READ_ONLY_NATIVE reservation_mutated=0 attempt_consumed=0",
			typename.EnumToString(EAICompartmentType, m_Role), AICF_GroupTrace.Entity(m_Member), m_sSeatKey,
			m_iStartedAtMs, m_iDeadlineMs, nowMs - m_iStartedAtMs, AICF_GroupTrace.Action(m_LastAction));
	}
}
