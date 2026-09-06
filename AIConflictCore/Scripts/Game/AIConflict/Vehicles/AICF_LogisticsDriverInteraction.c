// Read-only наблюдение штатной OpenNavlinkDoor цепочки 1.8.0.13.
// Никаких actions, subscriptions или retries: штатный AI сам выходит и возвращается.
class AICF_LogisticsDriverInteraction
{
	SCR_AIUtilityComponent m_Utility;
	ref SCR_AIPerformActionBehavior m_Action;
	ref SCR_AIGetOutVehicle m_Exit;
	ref SCR_AIGetInVehicle m_Return;
	ref SCR_AIActivityBase m_Activity;
	SCR_AISmartActionComponent m_SmartAction;
	IEntity m_Target;
	EntityID m_TargetId;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	ChimeraCharacter m_Driver;
	EntityID m_DriverId;
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	string m_sVehicleRpl;
	string m_sDriverRpl;
	BaseCompartmentSlot m_Seat;
	ref AICF_VehicleLease m_Lease;
	ref AICF_LogisticsJob m_Job;
	string m_sToken;
	int m_iGeneration;
	AICF_ELogisticsPhase m_ePhase;
	int m_iStartedAtMs;
	int m_iProgressAtMs;
	int m_iSampleAtMs;
	int m_iWaitBeforeMs;
	int m_iMilestone;
	float m_fBestDoorDistance;
	float m_fBestReturnDistance;
	float m_fBestDoorState;

	static bool Live(AIActionBase action)
	{
		return action && !action.GetRemoveAction() && action.GetActionState() != EAIActionState.FAILED && action.GetActionState() != EAIActionState.COMPLETED;
	}

	static SCR_AIUtilityComponent Utility(AICF_LogisticsWorker w)
	{
		if (!w.DriverIdentity()) return null;
		array<AIAgent> agents = {};
		w.m_Group.GetAgents(agents);
		if (agents.Count() != 1 || !agents[0] || agents[0].GetControlledEntity() != w.m_Driver) return null;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agents[0].FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != w.m_Driver) return null;
		return utility;
	}

	static AICF_LogisticsDriverInteraction TryBegin(AICF_LogisticsWorker w, int now)
	{
		if (!w.VehicleIdentity() || !w.DriverIdentity() || w.m_bStopped || w.m_bCleanupQueued ||
			!w.m_Seat || w.m_Seat.GetVehicle() != w.m_Vehicle || w.HasForeignOccupant(true)) return null;
		if (w.m_ePhase != AICF_ELogisticsPhase.TO_SOURCE && w.m_ePhase != AICF_ELogisticsPhase.TO_DESTINATION && w.m_ePhase != AICF_ELogisticsPhase.RETURN_HOME) return null;
		if (w.m_Job && (w.m_Job.m_bCancelled || w.m_Job.m_iGeneration != w.m_iGeneration)) return null;
		if (w.ProgressAgeMs(now) >= AICF_LogisticsConfig.PROGRESS_TIMEOUT_MS || w.LegAgeMs(now) >= AICF_LogisticsConfig.LEG_TIMEOUT_MS) return null;
		SCR_AIUtilityComponent utility = Utility(w);
		if (!utility) return null;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		SCR_AIBehaviorBase current = utility.GetCurrentBehavior();
		foreach (AIActionBase candidate : actions)
		{
			SCR_AIPerformActionBehavior perform = SCR_AIPerformActionBehavior.Cast(candidate);
			if (!Live(perform) || perform.GetPriority() != SCR_AIActionBase.PRIORITY_BEHAVIOR_OPEN_NAVLINK_DOOR) continue;
			SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(perform.GetRelatedGroupActivity());
			if (!activity || activity.m_Utility != w.m_Group.GetGroupUtilityComponent()) continue;
			SCR_AISmartActionComponent smart = perform.m_SmartActionComponent.m_Value;
			if (!smart || !smart.GetOwner()) continue;
			array<string> tags = {};
			smart.GetTags(tags);
			if (!tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)) continue;
			if (vector.Distance(w.m_Vehicle.GetOrigin(), smart.GetOwner().GetOrigin()) > AICF_LogisticsConfig.DRIVER_INTERACTION_RADIUS_M) continue;
			SCR_AIGetInVehicle returnAction;
			SCR_AIGetOutVehicle exitAction;
			foreach (AIActionBase sibling : actions)
			{
				if (!Live(sibling) || sibling.GetRelatedGroupActivity() != activity) continue;
				SCR_AIGetInVehicle getIn = SCR_AIGetInVehicle.Cast(sibling);
				if (getIn && getIn.m_Vehicle.m_Value == w.m_Seat.GetOwner() && getIn.m_CompartmentToGetIn.m_Value == w.m_Seat && getIn.GetPriority() == perform.GetPriority()) returnAction = getIn;
				SCR_AIGetOutVehicle getOut = SCR_AIGetOutVehicle.Cast(sibling);
				if (getOut && getOut.m_Vehicle.m_Value == w.m_Seat.GetOwner() && getOut.GetPriority() == perform.GetPriority() + 100) exitAction = getOut;
			}
			// Pending OpenGate сам по себе не объясняет произвольный выход/эвакуацию.
			if (!returnAction || (current != perform && (!exitAction || current != exitAction))) continue;
			AICF_LogisticsDriverInteraction wait = new AICF_LogisticsDriverInteraction();
			wait.m_Utility = utility;
			wait.m_Action = perform;
			wait.m_Exit = exitAction;
			wait.m_Return = returnAction;
			wait.m_Activity = activity;
			wait.m_SmartAction = smart;
			wait.m_Target = smart.GetOwner();
			wait.m_TargetId = wait.m_Target.GetID();
			wait.m_Group = w.m_Group;
			wait.m_GroupId = w.m_GroupId;
			wait.m_Driver = w.m_Driver;
			wait.m_DriverId = w.m_DriverId;
			wait.m_Vehicle = w.m_Vehicle;
			wait.m_VehicleId = w.m_VehicleId;
			wait.m_sVehicleRpl = w.m_sVehicleRpl;
			wait.m_sDriverRpl = Replication.FindItemId(w.m_Driver).ToString();
			wait.m_Seat = w.m_Seat;
			wait.m_Lease = w.m_Lease;
			wait.m_Job = w.m_Job;
			wait.m_sToken = "NONE";
			if (w.m_Job) wait.m_sToken = w.m_Job.m_sToken;
			wait.m_iGeneration = w.m_iGeneration;
			wait.m_ePhase = w.m_ePhase;
			wait.m_iStartedAtMs = now;
			wait.m_iProgressAtMs = now;
			wait.m_iSampleAtMs = now;
			wait.m_iWaitBeforeMs = w.m_iDriverWaitMs;
			wait.m_fBestDoorDistance = vector.Distance(w.m_Driver.GetOrigin(), wait.m_Target.GetOrigin());
			wait.m_fBestReturnDistance = AICF_LogisticsConfig.DRIVER_INTERACTION_RADIUS_M;
			return wait;
		}
		return null;
	}

	bool Matches(AICF_LogisticsWorker w)
	{
		if (w.m_bStopped || w.m_bCleanupQueued || w.m_iGeneration != m_iGeneration || w.m_ePhase != m_ePhase ||
			w.m_Job != m_Job || w.m_Lease != m_Lease || w.m_Group != m_Group || w.m_GroupId != m_GroupId ||
			w.m_Driver != m_Driver || w.m_DriverId != m_DriverId || w.m_Vehicle != m_Vehicle || w.m_VehicleId != m_VehicleId ||
			w.m_sVehicleRpl != m_sVehicleRpl || w.m_Seat != m_Seat) return false;
		if (m_Job && (m_Job.m_sToken != m_sToken || m_Job.m_bCancelled || m_Job.m_iGeneration != m_iGeneration)) return false;
		return w.VehicleIdentity() && w.DriverIdentity() && m_Utility && Utility(w) == m_Utility &&
			Replication.FindItemId(w.m_Driver).ToString() == m_sDriverRpl && m_Seat && m_Seat.GetVehicle() == m_Vehicle;
	}

	// Pure clock policy также используется Enforce contract probe; бюджет не переармляется.
	string DeadlineReason(int now)
	{
		int elapsed = now - m_iStartedAtMs;
		if (elapsed >= AICF_LogisticsConfig.DRIVER_INTERACTION_TIMEOUT_MS) return "DRIVER_INTERACTION_DEADLINE";
		if (m_iWaitBeforeMs + elapsed >= AICF_LogisticsConfig.DRIVER_INTERACTION_LEG_BUDGET_MS) return "DRIVER_INTERACTION_LEG_BUDGET";
		if (now - m_iProgressAtMs >= AICF_LogisticsConfig.DRIVER_INTERACTION_STALL_MS) return "DRIVER_INTERACTION_NO_PROGRESS";
		return string.Empty;
	}

	bool CanRenew(AICF_LogisticsWorker w, int now)
	{
		return Matches(w) && DeadlineReason(now).IsEmpty() && !w.HasForeignOccupant(true) &&
			m_iSampleAtMs <= now && now - m_iSampleAtMs < AICF_LogisticsConfig.DRIVER_INTERACTION_STALL_MS;
	}

	AICF_TripOutcome Poll(AICF_LogisticsWorker w, int now)
	{
		if (w.m_Driver == m_Driver && m_Driver && !AICF_GroupRuntime.IsAliveCharacter(m_Driver))
			return AICF_TripOutcome.TerminalFailClosed("DRIVER_INTERACTION_DRIVER_DEAD", m_sToken);
		if (!Matches(w)) return AICF_TripOutcome.TerminalFailClosed("DRIVER_INTERACTION_IDENTITY_CHANGED", m_sToken);
		if (w.HasForeignOccupant(true)) return AICF_TripOutcome.TerminalFailClosed("DRIVER_INTERACTION_FOREIGN_OCCUPANT", m_sToken);
		string deadline = DeadlineReason(now);
		if (!deadline.IsEmpty()) return AICF_TripOutcome.TerminalFailClosed(deadline, m_sToken);
		if (!m_Target || m_Target.GetID() != m_TargetId || !m_SmartAction || m_SmartAction.GetOwner() != m_Target ||
			m_Action.m_SmartActionComponent.m_Value != m_SmartAction) return AICF_TripOutcome.TerminalFailClosed("DRIVER_INTERACTION_TARGET_LOST", m_sToken);
		if (!Live(m_Activity) || m_Activity.m_Utility != m_Group.GetGroupUtilityComponent() ||
			m_Action.GetRelatedGroupActivity() != m_Activity || m_Return.GetRelatedGroupActivity() != m_Activity)
			return AICF_TripOutcome.TerminalFailClosed("DRIVER_INTERACTION_GROUP_ACTIVITY_LOST", m_sToken);
		CompartmentAccessComponent access = m_Driver.GetCompartmentAccessComponent();
		IEntity linkedVehicle = CompartmentAccessComponent.GetVehicleIn(m_Driver);
		if (!access || (linkedVehicle && linkedVehicle != m_Vehicle) || (access.IsInCompartment() && access.GetCompartment() != m_Seat) ||
			vector.Distance(m_Driver.GetOrigin(), m_Vehicle.GetOrigin()) > AICF_LogisticsConfig.DRIVER_INTERACTION_RADIUS_M)
			return AICF_TripOutcome.TerminalFailClosed("DRIVER_INTERACTION_PHYSICAL_CONTEXT_LOST", m_sToken);
		if (m_Action.GetActionState() == EAIActionState.FAILED || m_Return.GetActionState() == EAIActionState.FAILED ||
			(m_Exit && m_Exit.GetActionState() == EAIActionState.FAILED)) return AICF_TripOutcome.TerminalFailClosed("DRIVER_INTERACTION_NATIVE_ACTION_FAILED", m_sToken);
		bool opened = m_Action.GetActionState() == EAIActionState.COMPLETED;
		array<ref AIActionBase> actions = {};
		m_Utility.GetActions(actions);
		if ((!opened && (!Live(m_Action) || !actions.Contains(m_Action))) ||
			(!w.Ready() && (!Live(m_Return) || !actions.Contains(m_Return))) ||
			m_Return.m_CompartmentToGetIn.m_Value != m_Seat || m_Return.m_Vehicle.m_Value != m_Seat.GetOwner() ||
			m_Return.GetRelatedGroupActivity() != m_Action.GetRelatedGroupActivity())
			return AICF_TripOutcome.TerminalFailClosed("DRIVER_INTERACTION_NATIVE_CHAIN_LOST", m_sToken);
		// Компенсируем только наблюдаемое штатное ожидание. Общий budget остаётся конечным.
		w.m_iDriverWaitMs += now - m_iSampleAtMs;
		m_iSampleAtMs = now;
		w.m_iStationaryAtMs = 0;
		if (opened && w.Ready() && !Live(m_Exit)) return AICF_TripOutcome.CompleteTrip("EXACT_DRIVER_RETURNED", m_sToken);
		int milestone;
		if (!access.IsInCompartment() && !access.IsGettingOut()) milestone = 1;
		if (m_Utility.GetCurrentBehavior() == m_Action) milestone = 2;
		if (opened) milestone = 3;
		if (opened && access.IsGettingIn()) milestone = 4;
		if (milestone > m_iMilestone)
		{
			m_iMilestone = milestone;
			m_iProgressAtMs = now;
		}
		float distance = vector.Distance(m_Driver.GetOrigin(), m_Target.GetOrigin());
		if (!opened && distance + 1 < m_fBestDoorDistance)
		{
			m_fBestDoorDistance = distance;
			m_iProgressAtMs = now;
		}
		distance = vector.Distance(m_Driver.GetOrigin(), m_Vehicle.GetOrigin());
		if (opened && distance + 1 < m_fBestReturnDistance)
		{
			m_fBestReturnDistance = distance;
			m_iProgressAtMs = now;
		}
		BaseDoorComponent door = BaseDoorComponent.Cast(m_Target.FindComponent(BaseDoorComponent));
		if (door && Math.AbsFloat(door.GetNormalizedDoorState()) > m_fBestDoorState + 0.05)
		{
			m_fBestDoorState = Math.AbsFloat(door.GetNormalizedDoorState());
			m_iProgressAtMs = now;
		}
		return AICF_TripOutcome.Wait("NATIVE_OPEN_GATE", m_sToken);
	}

	void Log(AICF_LogisticsWorker w, string eventName, string reason, int now)
	{
		string details = string.Format("reason=%1 elapsed_ms=%2 progress_age_ms=%3 wait_job=%4 wait_generation=%5 wait_group=%6 wait_vehicle=%7 wait_driver=%8 target=%9",
			reason, now - m_iStartedAtMs, now - m_iProgressAtMs, m_sToken, m_iGeneration, m_GroupId, m_VehicleId, m_DriverId, m_TargetId);
		details += string.Format(" wait_vehicle_rpl=%1 milestone=%2 leg_wait_ms=%3 exact_return=%4 wait_driver_rpl=%5", m_sVehicleRpl, m_iMilestone, w.m_iDriverWaitMs, w.Ready(), m_sDriverRpl);
		w.Log(eventName, details);
	}
}
