// Flow решает, когда разрешена прежняя physical attempt. Commit только пишет
// прежний counter после всех native/identity/quota/reservation чтений helper.
// Getter-ы ниже принадлежат script state; engine callbacks здесь не вызываются.
class AICF_VehicleShortSeatAttempt
{
	AICF_TransportTrip m_Trip;
	AICF_VehicleLease m_Lease;
	AICF_StrategicAssignmentSnapshot m_Assignment;
	IEntity m_Actor;
	BaseCompartmentSlot m_Seat;
	AICF_ETransportTripPhase m_Phase;
	int m_iTransitions;
	int m_iDeadline;
	int m_iAttempts;
	bool m_bCommitted;
	ref AICF_VehicleBoardingActionToken m_Boarding;
	AICF_VehicleBoardingState m_BoardingState;
	ref AICF_VehicleBehaviorAction m_Behavior;
	AICF_VehicleBehaviorState m_BehaviorState;

	protected static AICF_VehicleShortSeatAttempt Capture(AICF_TransportTrip trip, IEntity actor, BaseCompartmentSlot seat)
	{
		if (!trip || !trip.GetLease() || !trip.GetAssignment() || !actor || !seat) return null;
		AICF_VehicleShortSeatAttempt attempt = new AICF_VehicleShortSeatAttempt();
		attempt.m_Trip = trip;
		attempt.m_Lease = trip.GetLease();
		attempt.m_Assignment = trip.GetAssignment();
		attempt.m_Actor = actor;
		attempt.m_Seat = seat;
		attempt.m_Phase = trip.GetPhase();
		attempt.m_iTransitions = trip.GetTransitionCount();
		return attempt;
	}

	static AICF_VehicleShortSeatAttempt ForBoarding(AICF_TransportTrip trip, AICF_VehicleBoardingState state, AICF_VehicleBoardingActionToken token)
	{
		if (!state || !token || token.WasShortRecoveryAttempted()) return null;
		AICF_VehicleShortSeatAttempt attempt = Capture(trip, token.GetReservedEntity(), token.GetCompartment());
		if (!attempt || trip.GetBoardingState() != state || trip.GetPhase() != AICF_ETransportTripPhase.BOARDING) return null;
		attempt.m_Boarding = token;
		attempt.m_BoardingState = state;
		attempt.m_iDeadline = state.GetAbsoluteDeadlineMs();
		return attempt;
	}

	static AICF_VehicleShortSeatAttempt ForBehavior(AICF_TransportTrip trip, AICF_VehicleBehaviorAction token)
	{
		if (!token || token.m_iShortAttempts < 0 || token.m_iShortAttempts >= 3) return null;
		AICF_VehicleShortSeatAttempt attempt = Capture(trip, token.m_Actor, token.m_Seat);
		if (!attempt || token.m_Trip != trip || !trip.GetBehaviorState() || trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT) return null;
		attempt.m_Behavior = token;
		attempt.m_BehaviorState = trip.GetBehaviorState();
		attempt.m_iDeadline = attempt.m_BehaviorState.m_iReboardDeadline;
		attempt.m_iAttempts = token.m_iShortAttempts;
		return attempt;
	}

	bool IsCurrent(AICF_TransportTrip trip, IEntity actor, BaseCompartmentSlot seat, int nowMs)
	{
		if (m_bCommitted || !m_Trip || !m_Lease || !m_Assignment || !actor || !seat || trip != m_Trip || actor != m_Actor || seat != m_Seat) return false;
		if (trip.GetLease() != m_Lease || trip.GetAssignment() != m_Assignment || trip.GetPhase() != m_Phase || trip.GetTransitionCount() != m_iTransitions) return false;
		if (trip.IsTerminal() || nowMs <= 0 || m_iDeadline <= 0 || nowMs >= m_iDeadline || trip.IsDeadlineReached(nowMs)) return false;
		if (m_Boarding)
		{
			if (m_Behavior || !m_BoardingState || trip.GetBoardingState() != m_BoardingState ||
				m_BoardingState.GetAbsoluteDeadlineMs() != m_iDeadline || m_Boarding.WasShortRecoveryAttempted()) return false;
			if (m_Boarding.GetReservedEntity() != actor || m_Boarding.GetCompartment() != seat || !m_BoardingState.GetTokens()) return false;
			AICF_VehicleBoardingTokenSet tokens = m_BoardingState.GetTokens();
			if (tokens.Count() > AICF_Stage1Config.MAX_GROUP_SIZE) return false;
			bool found;
			for (int index; index < tokens.Count(); index++)
				if (tokens.Get(index) == m_Boarding) found = true;
			if (!found) return false;
			return true;
		}
		if (!m_Behavior || !m_BehaviorState || trip.GetBehaviorState() != m_BehaviorState || m_BehaviorState.m_iReboardDeadline != m_iDeadline) return false;
		if (!m_BehaviorState.m_aActions.Contains(m_Behavior) || m_Behavior.m_Trip != trip || m_Behavior.m_Actor != actor || m_Behavior.m_Seat != seat) return false;
		if (m_Behavior.m_iShortAttempts != m_iAttempts || m_iAttempts < 0 || m_iAttempts >= 3) return false;
		return true;
	}

	bool Commit(AICF_TransportTrip trip, IEntity actor, BaseCompartmentSlot seat, int nowMs)
	{
		if (!IsCurrent(trip, actor, seat, nowMs)) return false;
		m_bCommitted = true;
		if (m_Boarding) m_Boarding.MarkShortRecoveryAttempted();
		else m_Behavior.m_iShortAttempts++;
		return true;
	}
}
