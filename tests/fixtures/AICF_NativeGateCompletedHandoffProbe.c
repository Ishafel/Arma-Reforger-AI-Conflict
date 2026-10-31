// Наблюдение настоящего completion invoker после production receipt listener.
// Возвращаемый Owns bool только записывается; observer не выбирает outcome.
modded class AICF_TransportNativeGate
{
	protected bool m_bG3ProbeAttached;

	override void Log(string eventName)
	{
		super.Log(eventName);
		string enabled;
		if (!System.GetCLIParam("aicfNativeGateCompletedHandoffProbe", enabled) || enabled != "1") return;
		if (eventName != "TRANSPORT_NATIVE_GATE_STARTED" || m_bG3ProbeAttached || m_bStopped ||
			AICF_GateCrewProbe.s_Gate != this || !m_Perform || !m_Perform.m_OnActionCompleted) return;
		m_bG3ProbeAttached = true;
		m_Perform.m_OnActionCompleted.Insert(AICF_G3ObserveCompletedPerform);
	}

	protected void AICF_G3ObserveCompletedPerform(SCR_AIActionBase action)
	{
		if (!m_bG3ProbeAttached || m_bStopped || action != m_Perform || !m_Trip ||
			AICF_GateCrewProbe.s_Gate != this || m_Trip.GetMovementState().GetNativeGate() != this) return;
		if (!m_Utility || !m_Seat || !m_Member || !m_Vehicle) return;
		int nowMs = System.GetTickCount();
		AIActionBase current = m_Utility.GetCurrentBehavior();
		array<ref AIActionBase> actions = {};
		m_Utility.GetActions(actions);
		bool accepted = OwnsTemporaryCrewExit(m_Trip, m_Seat, m_Member);
		bool exactCurrent = current == m_Perform;
		bool returnQueued = actions.Contains(m_Return);
		bool related = m_Return && m_Return.GetRelatedGroupActivity() == m_Activity;
		bool returnLive = Live(m_Return);
		bool suspended;
		if (m_Return) suspended = m_Return.GetActionIsSuspended();
		bool currentRemoved;
		if (current) currentRemoved = current.GetRemoveAction();
		EntityID actualMemberId = m_Member.GetID();
		EntityID actualVehicleId = m_Vehicle.GetID();
		string details = string.Format("accepted=%1 operation=%2 observed_ms=%3 window_start=%4 window_deadline=%5 exact_current_perform=%6 current=%7 current_removed=%8",
			accepted, m_sOperation, nowMs, m_iStarted, m_iDeadline, exactCurrent, AICF_GroupTrace.Action(current), currentRemoved);
		details += string.Format(" exact_return_queued=%1 native_return=%2 return_live=%3 return_suspended=%4 return_related_exact=%5 observed_exit=%6 member=%7 member_rpl=%8",
			returnQueued, AICF_GroupTrace.Action(m_Return), returnLive, suspended, related, m_bObservedExit, actualMemberId, AICF_VehicleBehaviorBinding.ComponentRpl(m_Member));
		details += string.Format(" vehicle=%1 vehicle_rpl=%2 actor_health=%3 vehicle_health=%4 current_deadline=%5 attempts=%6 original_seat_empty=%7",
			actualVehicleId, AICF_VehicleBehaviorBinding.ComponentRpl(m_Vehicle), AICF_GateCrewProbe.Health(m_Member), AICF_GateCrewProbe.Health(m_Vehicle),
			m_Trip.GetAbsoluteDeadlineMs(), m_Trip.GetMovementState().GetMobilityRecoveryAttempts(), !m_Seat.GetOccupant());
		details += " observer_scope=NATIVE_COMPLETION_INVOKER_DIAG_ONLY actual_controller_decision=0 successful_native_completion_invoker=1";
		AICF_GateCrewProbe.Log("COMPLETED_HANDOFF_INVOKER", details);
	}

	override void Stop()
	{
		if (m_bG3ProbeAttached && m_Perform && m_Perform.m_OnActionCompleted)
			m_Perform.m_OnActionCompleted.Remove(AICF_G3ObserveCompletedPerform);
		m_bG3ProbeAttached = false;
		super.Stop();
	}
}
