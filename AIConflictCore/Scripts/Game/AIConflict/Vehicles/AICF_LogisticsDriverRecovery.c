// Причина неизвестна: не заявляет OpenGate, не вмешивается в живую native chain.
class AICF_LogisticsDriverRecovery : AICF_LogisticsDriverInteraction
{
	int m_iQuietAtMs;
	bool m_bReturnIssued;
	vector m_vVehicleAtExit;
	bool m_bNoVehicleMotion = true;

	static AICF_LogisticsDriverRecovery TryRecover(AICF_LogisticsWorker w, int now)
	{
		if (!Moving(w) || w.Ready() || w.m_bStopped || w.m_bCleanupQueued || !w.VehicleIdentity() || !w.DriverIdentity() ||
			!w.m_Seat || w.m_Seat.GetVehicle() != w.m_Vehicle || w.HasForeignOccupant(true) || !Utility(w) ||
			w.m_iDriverWaitMs >= AICF_LogisticsConfig.DRIVER_INTERACTION_LEG_BUDGET_MS || w.LegAgeMs(now) >= AICF_LogisticsConfig.LEG_TIMEOUT_MS) return null;
		AICF_LogisticsDriverRecovery recovery = new AICF_LogisticsDriverRecovery();
		recovery.Capture(w, now);
		recovery.m_vVehicleAtExit = w.m_Vehicle.GetOrigin();
		if (!recovery.PhysicalContext(w)) return null;
		Diagnose(w, "EXIT_CAUSE_UNRECOGNIZED", now);
		return recovery;
	}

	bool PhysicalContext(AICF_LogisticsWorker w)
	{
		if (!Matches(w)) return false;
		CompartmentAccessComponent access = m_Driver.GetCompartmentAccessComponent();
		IEntity linked = CompartmentAccessComponent.GetVehicleIn(m_Driver);
		Physics physics = m_Vehicle.GetPhysics();
		return access && physics && physics.GetVelocity().Length() <= 1 &&
			(!linked || linked == m_Vehicle) && (!access.GetCompartment() || access.GetCompartment() == m_Seat) &&
			vector.Distance(m_Driver.GetOrigin(), m_Vehicle.GetOrigin()) <= 20;
	}

	override string DeadlineReason(int now)
	{
		if (now - m_iStartedAtMs >= AICF_LogisticsConfig.UNKNOWN_DRIVER_TIMEOUT_MS) return "UNKNOWN_DRIVER_RECOVERY_DEADLINE";
		if (m_iWaitBeforeMs + now - m_iStartedAtMs >= AICF_LogisticsConfig.DRIVER_INTERACTION_LEG_BUDGET_MS) return "DRIVER_INTERACTION_LEG_BUDGET";
		return string.Empty;
	}

	override bool CanRenew(AICF_LogisticsWorker w, int now)
	{
		return PhysicalContext(w) && !w.HasForeignOccupant(true) && DeadlineReason(now).IsEmpty() && now - m_iSampleAtMs < 5000;
	}

	bool NativeBusy(bool allowRoutePause = false)
	{
		array<ref AIActionBase> actions = {};
		m_Utility.GetActions(actions);
		foreach (AIActionBase action : actions)
		{
			if (!Live(action) || action == m_Return) continue;
			if (SCR_AIIdleBehavior.Cast(action)) continue;
			SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(action.GetRelatedGroupActivity());
			if (allowRoutePause && SCR_AIMoveInFormationBehavior.Cast(action) && activity && activity.m_Utility == m_Group.GetGroupUtilityComponent()) continue;
			if (SCR_AIVehicleBehavior.Cast(action) || SCR_AIPerformActionBehavior.Cast(action) || m_Utility.GetCurrentBehavior() == action) return true;
		}
		return false;
	}

	override AICF_TripOutcome Poll(AICF_LogisticsWorker w, int now)
	{
		if (m_Driver && w.m_Driver == m_Driver && !AICF_GroupRuntime.IsAliveCharacter(m_Driver))
			return AICF_TripOutcome.TerminalFailClosed("UNKNOWN_DRIVER_DEAD", m_sToken);
		if (!Matches(w)) return AICF_TripOutcome.TerminalFailClosed("UNKNOWN_DRIVER_IDENTITY_CHANGED", m_sToken);
		if (w.HasForeignOccupant(true)) return AICF_TripOutcome.TerminalFailClosed("UNKNOWN_DRIVER_FOREIGN_OCCUPANT", m_sToken);
		if (!PhysicalContext(w))
		{
			Diagnose(w, "UNKNOWN_DRIVER_PHYSICAL_CONTEXT_LOST", now);
			return AICF_TripOutcome.TerminalFailClosed("UNKNOWN_DRIVER_PHYSICAL_CONTEXT_LOST", m_sToken);
		}
		if (vector.DistanceXZ(m_vVehicleAtExit, m_Vehicle.GetOrigin()) > 3) m_bNoVehicleMotion = false;
		string deadline = DeadlineReason(now);
		if (!deadline.IsEmpty()) return AICF_TripOutcome.TerminalFailClosed(deadline, m_sToken);
		w.m_iDriverWaitMs += now - m_iSampleAtMs;
		m_iSampleAtMs = now;
		w.m_iStationaryAtMs = 0;
		if (w.Ready()) return AICF_TripOutcome.CompleteTrip("EXACT_DRIVER_RETURNED_CAUSE_UNKNOWN", m_sToken);
		// Снимаем только свой route до того, как вышедший driver пойдёт по нему
		// пешком. Живая native vehicle/interaction chain по-прежнему имеет приоритет.
		if (w.m_Waypoint && !NativeBusy(true)) return AICF_TripOutcome.Retry("SUSPEND_UNKNOWN_EXIT_ROUTE", m_sToken, now);
		CompartmentAccessComponent access = m_Driver.GetCompartmentAccessComponent();
		if (m_bReturnIssued)
		{
			array<ref AIActionBase> actions = {};
			m_Utility.GetActions(actions);
			if (!Live(m_Return) || !actions.Contains(m_Return))
			{
				// Native action завершается раньше animated exact-seat postcondition.
				// Ничего не переиздаём, общий 60-секундный deadline не меняется.
				if (access.IsGettingIn() || (m_Seat.GetOccupant() == m_Driver && CompartmentAccessComponent.GetVehicleIn(m_Driver) == m_Vehicle))
					return AICF_TripOutcome.Wait("EXACT_RETURN_SETTLING", m_sToken);
				Diagnose(w, "UNKNOWN_DRIVER_RETURN_FAILED", now);
				return AICF_TripOutcome.TerminalFailClosed("UNKNOWN_DRIVER_RETURN_FAILED", m_sToken);
			}
			return AICF_TripOutcome.Wait("EXACT_RETURN_PENDING", m_sToken);
		}
		if (access.IsGettingIn() || access.IsGettingOut() || access.IsInCompartment() || NativeBusy(true))
		{
			m_iQuietAtMs = 0;
			return AICF_TripOutcome.Wait("UNKNOWN_EXIT_NATIVE_ACTION_PENDING", m_sToken);
		}
		if (!m_iQuietAtMs) m_iQuietAtMs = now;
		if (now - m_iQuietAtMs < 2000 || now - m_iStartedAtMs < 5000) return AICF_TripOutcome.Wait("UNKNOWN_EXIT_SETTLING", m_sToken);
		return AICF_TripOutcome.Retry("ISSUE_EXACT_DRIVER_RETURN", m_sToken, now);
	}
}

// Обычное animated get-in; failure не вызывает stock teleport/get-out callback.
class AICF_LogisticsReturnAction : SCR_AIGetInVehicle
{
	AICF_LogisticsWorker m_Worker;
	int m_iGeneration;
	EntityID m_DriverId;
	EntityID m_VehicleId;
	string m_sToken;

	override void OnActionCompleted()
	{
		BaseCompartmentSlot seat = m_CompartmentToGetIn.m_Value;
		bool same = m_Worker && m_Worker.m_iGeneration == m_iGeneration && m_Worker.m_DriverId == m_DriverId &&
			m_Worker.m_VehicleId == m_VehicleId && m_Worker.VehicleIdentity() && m_Worker.m_Seat == seat &&
			m_Worker.m_Driver == m_Utility.m_OwnerEntity;
		string token = "NONE";
		if (m_Worker && m_Worker.m_Job) token = m_Worker.m_Job.m_sToken;
		if (token != m_sToken) same = false;
		if (!same || (seat && ((seat.IsReserved() && !seat.IsReservedBy(m_Utility.m_OwnerEntity)) || (seat.GetOccupant() && seat.GetOccupant() != m_Utility.m_OwnerEntity))))
			m_CompartmentToGetIn.m_Value = null;
		super.OnActionCompleted();
	}

	override void OnActionFailed()
	{
		OnActionCompleted();
	}
}
