// Разрешает только доказанную штатную цепочку OpenGate → exact-seat return.
// Transit observer не выдаёт actions и не меняет deadlines/progress trip.
class AICF_TransportNativeGate
{
	IEntity m_Member;
	EntityID m_Id;
	string m_sMemberRpl;
	AIAgent m_Agent;
	BaseCompartmentSlot m_Seat;
	ECompartmentType m_SeatType;
	SCR_AIUtilityComponent m_Utility;
	ref SCR_AIGetInVehicle m_Return;
	ref SCR_AIGetOutVehicle m_Exit;
	ref SCR_AIPerformActionBehavior m_Perform;
	AICF_TransportTrip m_Trip;
	bool m_bStopped;
	bool m_bReturnListenerAttached;
	bool m_bPerformListenerAttached;
	bool m_bPerformCompleted;
	int m_iPerformCompletedAtMs;
	bool m_bReturnCompleted;
	int m_iReturnCompletedAtMs;
	SCR_AIActivityBase m_Activity;
	SCR_AISmartActionComponent m_SmartAction;
	IEntity m_SmartOwner;
	EntityID m_SmartOwnerId;
	int m_iStarted;
	int m_iDeadline;
	bool m_bObservedExit;
	int m_iReturnedAtMs;
	vector m_vReturnPosition;
	string m_sOperation;
	string m_sSeatKey;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	string m_sVehicleRpl;
	int m_iAssignmentRevision;
	int m_iRouteGeneration;
	int m_iIntentRevision;

	static AICF_TransportNativeGate Begin(AICF_TransportTrip trip, IEntity member, BaseCompartmentSlot seat)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (!character || !character.GetAIControlComponent()) return null;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		if (!agent || agent.GetParentGroup() != trip.GetAssignment().GetGroup()) return null;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != member) return null;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		int performCandidates;
		SCR_AIPerformActionBehavior firstPerform;
		foreach (AIActionBase possible : actions)
		{
			SCR_AIPerformActionBehavior eligible = SCR_AIPerformActionBehavior.Cast(possible);
			if (!IsEligiblePerform(trip, seat, eligible)) continue;
			performCandidates++;
			if (!firstPerform) firstPerform = eligible;
		}
		if (performCandidates != 1)
		{
			if (performCandidates > 1) ReportCandidate(trip, member, seat, utility, firstPerform, "AMBIGUOUS_PERFORM", performCandidates, 0, 0);
			return null;
		}
		foreach (AIActionBase candidate : actions)
		{
			SCR_AIPerformActionBehavior perform = SCR_AIPerformActionBehavior.Cast(candidate);
			if (!IsEligiblePerform(trip, seat, perform)) continue;
			SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(perform.GetRelatedGroupActivity());
			SCR_AISmartActionComponent smart = perform.m_SmartActionComponent.m_Value;
			SCR_AIGetInVehicle returnAction;
			SCR_AIGetOutVehicle exitAction;
			int returnCandidates, exitCandidates;
			foreach (AIActionBase sibling : actions)
			{
				if (!Live(sibling) || sibling.GetRelatedGroupActivity() != activity) continue;
				SCR_AIGetInVehicle getIn = SCR_AIGetInVehicle.Cast(sibling);
				if (getIn && getIn.m_Vehicle.m_Value == seat.GetOwner() && getIn.m_CompartmentToGetIn.m_Value == seat && getIn.GetPriority() == perform.GetPriority())
				{
					returnCandidates++;
					returnAction = getIn;
				}
				SCR_AIGetOutVehicle getOut = SCR_AIGetOutVehicle.Cast(sibling);
				if (getOut && getOut.m_Vehicle.m_Value == seat.GetOwner() && getOut.GetPriority() == perform.GetPriority() + 100)
				{
					exitCandidates++;
					exitAction = getOut;
				}
			}
			if (returnCandidates != 1 || exitCandidates > 1)
			{
				ReportCandidate(trip, member, seat, utility, perform, "AMBIGUOUS_OR_MISSING_SIBLINGS", performCandidates, returnCandidates, exitCandidates);
				continue;
			}
			if (!returnAction || (utility.GetCurrentBehavior() != perform && (!exitAction || utility.GetCurrentBehavior() != exitAction))) continue;
			AICF_TransportNativeGate gate = new AICF_TransportNativeGate();
			gate.m_Member = member;
			gate.m_Id = member.GetID();
			gate.m_sMemberRpl = AICF_VehicleBehaviorBinding.ComponentRpl(member);
			gate.m_Agent = agent;
			gate.m_Seat = seat;
			gate.m_SeatType = seat.GetType();
			gate.m_Utility = utility;
			gate.m_Return = returnAction;
			gate.m_Exit = exitAction;
			gate.m_Perform = perform;
			gate.m_Activity = activity;
			gate.m_SmartAction = smart;
			gate.m_SmartOwner = smart.GetOwner();
			gate.m_SmartOwnerId = gate.m_SmartOwner.GetID();
			gate.m_sOperation = trip.GetOperationId();
			gate.m_sSeatKey = AICF_VehiclePreset.SeatKey(seat);
			gate.m_Group = trip.GetAssignment().GetGroup();
			gate.m_GroupId = gate.m_Group.GetID();
			gate.m_Vehicle = trip.GetLease().GetVehicle();
			gate.m_VehicleId = gate.m_Vehicle.GetID();
			gate.m_sVehicleRpl = Replication.FindItemId(gate.m_Vehicle).ToString();
			gate.m_iAssignmentRevision = trip.GetAssignment().GetAssignmentRevision();
			gate.m_iRouteGeneration = trip.GetMovementState().GetRouteGeneration();
			gate.m_iIntentRevision = trip.GetAssignment().GetStrategicIntentRevision();
			if (!gate.Matches(trip)) continue;
			string windowKey = string.Format("%1:%2:%3:%4:%5:%6:%7:%8",
				gate.m_sOperation, gate.m_GroupId, gate.m_iAssignmentRevision, gate.m_iIntentRevision,
				gate.m_iRouteGeneration, trip.GetLease().GetLeaseGeneration(), trip.GetLease().GetVehicleGeneration(), gate.m_SmartOwnerId);
			gate.m_iStarted = trip.GetMovementState().CaptureNativeGateWindowStart(windowKey, System.GetTickCount());
			if (gate.m_iStarted <= 0) continue;
			gate.m_iDeadline = Math.Min(gate.m_iStarted + 60000, trip.GetAbsoluteDeadlineMs());
			if (!gate.AttachReturnCompletion(trip)) continue;
			return gate;
		}
		return null;
	}

	protected static bool IsEligiblePerform(AICF_TransportTrip trip, BaseCompartmentSlot seat, SCR_AIPerformActionBehavior perform)
	{
		if (!Live(perform) || perform.GetPriority() != SCR_AIActionBase.PRIORITY_BEHAVIOR_OPEN_NAVLINK_DOOR) return false;
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(perform.GetRelatedGroupActivity());
		if (!activity || activity.m_Utility != trip.GetAssignment().GetGroup().GetGroupUtilityComponent()) return false;
		if (activity != activity.m_Utility.GetCurrentAction()) return false;
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(activity);
		if (!move || move.m_RelatedWaypoint != trip.GetMovementState().GetRouteWaypoint()) return false;
		SCR_AISmartActionComponent smart = perform.m_SmartActionComponent.m_Value;
		if (!smart || !smart.GetOwner()) return false;
		if (vector.Distance(seat.GetVehicle().GetOrigin(), smart.GetOwner().GetOrigin()) > 50) return false;
		array<string> tags = {};
		smart.GetTags(tags);
		return tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG);
	}

	protected static void ReportCandidate(AICF_TransportTrip trip, IEntity member, BaseCompartmentSlot seat,
		SCR_AIUtilityComponent utility, SCR_AIPerformActionBehavior perform, string reason, int performs, int returns, int exits)
	{
		if (!trip.IsVehiclePresetCurrent() || !perform || !perform.m_SmartActionComponent.m_Value) return;
		if (!trip.GetMovementState().MarkNativeGateCandidateReportDue(System.GetTickCount())) return;
		SCR_AISmartActionComponent smart = perform.m_SmartActionComponent.m_Value;
		array<string> tags = {};
		smart.GetTags(tags);
		string actualTags;
		foreach (string tag : tags)
		{
			tag.Replace(" ", "_");
			tag.Replace("=", ":");
			actualTags += tag + ";";
		}
		string details = string.Format("operation_id=%1 member=%2 seat=%3 reason=%4 perform_candidates=%5 return_candidates=%6 exit_candidates=%7",
			trip.GetOperationId(), AICF_GroupTrace.Entity(member), AICF_VehiclePreset.SeatKey(seat), reason, performs, returns, exits);
		details += string.Format(" smart_owner=%1 smart_component=%2 actual_smart_tags=%3 current_behavior=%4 related_activity=%5 current_group_activity=%6 sampled_candidate_only=1 accepted=0",
			AICF_GroupTrace.Entity(smart.GetOwner()), smart, actualTags, AICF_GroupTrace.Action(utility.GetCurrentBehavior()),
			AICF_GroupTrace.Action(SCR_AIActionBase.Cast(perform.GetRelatedGroupActivity())), AICF_GroupTrace.Action(trip.GetAssignment().GetGroup().GetGroupUtilityComponent().GetCurrentAction()));
		AICF_Stage3Diagnostics.Info("TRANSPORT_NATIVE_GATE_CANDIDATE_REJECTED", details);
	}
	static bool Live(AIActionBase action)
	{
		return action && !action.GetRemoveAction() && action.GetActionState() != EAIActionState.FAILED && action.GetActionState() != EAIActionState.COMPLETED;
	}

	static AICF_TransportNativeGate TryBegin(AICF_TransportTrip trip)
	{
		if (!Replication.IsServer() || !trip || !trip.IsVehiclePresetCurrent() || !trip.GetVehiclePreset() ||
			!trip.GetLease() || !trip.GetLease().HasPhysicalAsset() || !trip.GetMovementState().IsRouteWaypointBound()) return null;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(trip.GetLease().GetVehicle(), seats);
		if (!trip.GetVehiclePreset().MatchesSeats(seats)) return null;
		for (int index; index < seats.Count(); index++)
		{
			int position = trip.GetVehiclePreset().m_aMembers[index];
			if (position < 0) continue;
			IEntity member = trip.VehiclePresetMember(position);
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)) continue;
			AICF_TransportNativeGate gate = Begin(trip, member, seats[index]);
			if (gate) return gate;
		}
		return null;
	}

	bool Matches(AICF_TransportTrip trip)
	{
		if (m_bStopped) return false;
		if (!Replication.IsServer() || !trip || !trip.IsVehiclePresetCurrent() || trip.GetOperationId() != m_sOperation ||
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || trip.GetAssignment().GetAssignmentRevision() != m_iAssignmentRevision ||
			trip.GetMovementState().GetRouteGeneration() != m_iRouteGeneration || trip.GetAssignment().GetGroup() != m_Group ||
			!m_Group || m_Group.GetID() != m_GroupId) return false;
		if (!m_Member || m_Member.GetID() != m_Id || !AICF_GroupRuntime.IsAliveCharacter(m_Member) ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Member)) return false;
		if (!m_Vehicle || !trip.GetLease() || trip.GetLease().GetVehicle() != m_Vehicle || m_Vehicle.GetID() != m_VehicleId ||
			Replication.FindItemId(m_Vehicle).ToString() != m_sVehicleRpl) return false;
		if (!m_Seat || m_Seat.GetVehicle() != m_Vehicle ||
			(m_Seat.GetOccupant() && m_Seat.GetOccupant() != m_Member) || !m_Utility || m_Utility.m_OwnerEntity != m_Member) return false;
		if (trip.GetAssignment().GetStrategicIntentRevision() != m_iIntentRevision) return false;
		if (!m_Activity || m_Activity.m_Utility != m_Group.GetGroupUtilityComponent()) return false;
		if (m_Activity.m_Utility.GetCurrentAction() != m_Activity) return false;
		SCR_AIMoveActivity move = SCR_AIMoveActivity.Cast(m_Activity);
		if (!move || move.m_RelatedWaypoint != trip.GetMovementState().GetRouteWaypoint()) return false;
		if (!m_SmartOwner || m_SmartOwner.GetID() != m_SmartOwnerId || !m_SmartAction) return false;
		if (m_SmartAction.GetOwner() != m_SmartOwner) return false;
		if (!m_Return || m_Return.m_Vehicle.m_Value != m_Seat.GetOwner()) return false;
		if (m_Return.m_CompartmentToGetIn.m_Value != m_Seat) return false;
		if (m_Return.GetRelatedGroupActivity() != m_Activity || m_Return.GetActionState() == EAIActionState.FAILED) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Member);
		if (!character || !character.GetAIControlComponent()) return false;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		return agent && agent.GetParentGroup() == m_Group && SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent)) == m_Utility;
	}

	// Пустое исходное crew seat допустимо только пока его живой original AI
	// исполняет собственную уже захваченную native OpenGate/return цепочку.
	bool OwnsTemporaryCrewExit(AICF_TransportTrip trip, BaseCompartmentSlot seat, IEntity assignedMember)
	{
		if (!trip || !seat || !assignedMember || !trip.GetMovementState() ||
			trip.GetMovementState().GetNativeGate() != this) return false;
		int nowMs = System.GetTickCount();
		if (trip.IsDeadlineReached(nowMs) || IsExpired(nowMs) || !Matches(trip)) return false;
		if (seat != m_Seat || seat.GetType() != m_SeatType || assignedMember != m_Member ||
			AICF_VehiclePreset.SeatKey(seat) != m_sSeatKey || seat.GetOccupant()) return false;
		if (!AICF_VehicleShortRecovery.IsCurrentMember(trip, assignedMember) ||
			!SCR_AIDamageHandling.IsConscious(assignedMember)) return false;
		if (m_sMemberRpl.IsEmpty() || m_sMemberRpl == "NONE" ||
			AICF_VehicleBehaviorBinding.ComponentRpl(assignedMember) != m_sMemberRpl) return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(assignedMember);
		if (!character || !character.GetAIControlComponent()) return false;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || access.IsSwitchingSeatsAnim()) return false;
		IEntity linkedVehicle = CompartmentAccessComponent.GetVehicleIn(assignedMember);
		IEntity accessVehicle = access.GetVehicleCompartmentManagerOwner();
		if ((linkedVehicle && linkedVehicle != m_Vehicle) || (accessVehicle && accessVehicle != m_Vehicle)) return false;
		if (access.GetCompartment() && access.GetCompartment() != m_Seat) return false;
		AIAgent currentAgent = character.GetAIControlComponent().GetControlAIAgent();
		if (!currentAgent || currentAgent != m_Agent || currentAgent.GetControlledEntity() != assignedMember ||
			currentAgent.GetParentGroup() != m_Group || SCR_AIUtilityComponent.Cast(currentAgent.FindComponent(SCR_AIUtilityComponent)) != m_Utility) return false;
		if (!m_Perform || m_Perform.m_SmartActionComponent.m_Value != m_SmartAction ||
			m_Perform.GetPriority() != SCR_AIActionBase.PRIORITY_BEHAVIOR_OPEN_NAVLINK_DOOR) return false;
		array<string> tags = {};
		m_SmartAction.GetTags(tags);
		if (!tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG) ||
			vector.Distance(m_Vehicle.GetOrigin(), m_SmartOwner.GetOrigin()) > 50) return false;
		AIActionBase current = m_Utility.GetCurrentBehavior();
		if (!current || current.GetRelatedGroupActivity() != m_Activity) return false;
		if (!Live(current)) return OwnsCompletedPerformHandoff(current, nowMs);
		if (current == m_Perform) return true;
		if (current == m_Return) return true;
		return current == m_Exit && m_Exit.m_Vehicle.m_Value == m_Seat.GetOwner() &&
			m_Exit.GetPriority() == m_Perform.GetPriority() + 100;
	}

	// Native utility может ещё хранить завершённый Perform до выбора Return.
	// Это только продолжение исходной цепочки в её прежнем абсолютном окне.
	protected bool OwnsCompletedPerformHandoff(AIActionBase current, int nowMs)
	{
		if (m_bStopped || !m_bPerformListenerAttached || !m_bPerformCompleted || !m_bObservedExit) return false;
		if (m_iPerformCompletedAtMs < m_iStarted || m_iPerformCompletedAtMs <= 0 ||
			m_iPerformCompletedAtMs > nowMs || m_iPerformCompletedAtMs >= m_iDeadline || IsExpired(nowMs)) return false;
		if (current != m_Perform || current.GetActionState() != EAIActionState.COMPLETED || !current.GetRemoveAction()) return false;
		if (!m_Utility || !Live(m_Return) || m_Return.GetActionIsSuspended() ||
			m_Return.GetRelatedGroupActivity() != m_Activity) return false;
		array<ref AIActionBase> actions = {};
		m_Utility.GetActions(actions);
		return actions.Contains(m_Return);
	}

	protected bool AttachReturnCompletion(AICF_TransportTrip trip)
	{
		if (m_bStopped || m_bReturnListenerAttached || !m_Return || !Matches(trip)) return false;
		if (m_bPerformListenerAttached || !m_Perform || !m_Return.m_OnActionCompleted || !m_Perform.m_OnActionCompleted) return false;
		m_Trip = trip;
		m_bReturnListenerAttached = true;
		m_bPerformListenerAttached = true;
		m_Return.m_OnActionCompleted.Insert(OnReturnCompleted);
		m_Perform.m_OnActionCompleted.Insert(OnPerformCompleted);
		return true;
	}

	protected void OnPerformCompleted(SCR_AIActionBase action)
	{
		if (m_bStopped || !m_bPerformListenerAttached || m_bPerformCompleted || !m_Trip || action != m_Perform) return;
		int nowMs = System.GetTickCount();
		if (m_Trip.IsDeadlineReached(nowMs) || IsExpired(nowMs) || !Matches(m_Trip)) return;
		if (m_Trip.GetMovementState().GetNativeGate() != this || action.GetActionState() != EAIActionState.COMPLETED) return;
		m_bPerformCompleted = true;
		m_iPerformCompletedAtMs = nowMs;
		Log("TRANSPORT_NATIVE_GATE_PERFORM_COMPLETED");
	}

	protected void OnReturnCompleted(SCR_AIActionBase action)
	{
		if (m_bStopped || !m_bReturnListenerAttached || !m_Trip || action != m_Return) return;
		if (!Matches(m_Trip) || m_Trip.GetMovementState().GetNativeGate() != this) return;
		// Native OnActionDeselected может немедленно вернуть EVALUATED. Только
		// exact native completion callback сохраняет положительный receipt.
		m_bReturnCompleted = true;
		m_iReturnCompletedAtMs = System.GetTickCount();
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		if (m_bReturnListenerAttached && m_Return)
			m_Return.m_OnActionCompleted.Remove(OnReturnCompleted);
		if (m_bPerformListenerAttached && m_Perform)
			m_Perform.m_OnActionCompleted.Remove(OnPerformCompleted);
		m_bReturnListenerAttached = false;
		m_bPerformListenerAttached = false;
		m_bPerformCompleted = false;
		m_iPerformCompletedAtMs = 0;
		m_bReturnCompleted = false;
		m_Trip = null;
		m_Return = null;
		m_Exit = null;
		m_Perform = null;
	}

	bool IsReturned()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Member);
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access) return false;
		if (!character.IsInVehicle() && !access.IsInCompartment() && !CompartmentAccessComponent.GetVehicleIn(character)) m_bObservedExit = true;
		if (!m_bObservedExit || !m_Return || !m_bReturnCompleted) return false;
		bool settled = character.IsInVehicle() && access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut();
		if (!settled || access.IsSwitchingSeatsAnim()) return false;
		bool exactSeat = access.GetCompartment() == m_Seat && m_Seat.GetOccupant() == m_Member;
		if (!exactSeat) return false;
		if (CompartmentAccessComponent.GetVehicleIn(character) != m_Vehicle) return false;
		if (m_iReturnedAtMs <= 0)
		{
			m_iReturnedAtMs = System.GetTickCount();
			m_vReturnPosition = m_Vehicle.GetOrigin();
		}
		return true;
	}

	bool HasResumed(float minimumMotionMeters)
	{
		if (m_iReturnedAtMs <= 0 || !m_Vehicle || minimumMotionMeters <= 0) return false;
		Physics physics = m_Vehicle.GetPhysics();
		if (!physics || physics.GetVelocity().Length() < 0.5) return false;
		return vector.DistanceXZ(m_Vehicle.GetOrigin(), m_vReturnPosition) >= minimumMotionMeters;
	}

	bool IsExpired(int now) { return now - m_iStarted >= 60000 || now >= m_iDeadline; }

	// Завершённая native цепочка не даёт бесконечного ожидания следующего OpenGate.
	// Только существующий transit flow может потратить свой прежний recovery budget.
	bool CanRecoverExpiredReturn(AICF_TransportTrip trip, int nowMs)
	{
		if (!Matches(trip) || !IsExpired(nowMs) || !m_bObservedExit || !m_bReturnCompleted ||
			m_iReturnCompletedAtMs <= 0 || m_iReturnedAtMs < m_iReturnCompletedAtMs || m_iReturnedAtMs > nowMs) return false;
		if (!AICF_VehicleShortRecovery.IsSlowVehicle(m_Vehicle)) return false;
		AICF_VehiclePresetNativeSeatBinding binding = AICF_VehiclePresetNativeSeatBinding.Capture(trip);
		if (!binding || !binding.IsCurrent() || m_Group.GetAgentsCount() != binding.m_aMembers.Count()) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(m_Vehicle, seats);
		string rosterReason;
		if (!trip.GetVehiclePreset().ValidateRoster(trip.GetVehiclePresetSlot(), seats, rosterReason)) return false;
		for (int index; index < binding.m_aMembers.Count(); index++)
		{
			IEntity member = binding.m_aMembers[index];
			BaseCompartmentSlot seat = binding.FindSeat(member);
			if (!seat || !AICF_VehicleShortRecovery.IsCurrentMember(trip, member) ||
				vector.Distance(member.GetOrigin(), seat.GetPosition()) > AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS) return false;
		}
		return binding.IsCurrent() && Matches(trip) && AICF_VehicleShortRecovery.IsSlowVehicle(m_Vehicle);
	}

	void Log(string eventName)
	{
		string details = string.Format("operation_id=%1 member=%2 seat=%3 vehicle=%4 elapsed_ms=%5 route_generation=%6 assignment_revision=%7",
			m_sOperation, AICF_GroupTrace.Entity(m_Member), m_sSeatKey, AICF_GroupTrace.Entity(m_Vehicle),
			System.GetTickCount() - m_iStarted, m_iRouteGeneration, m_iAssignmentRevision);
		details += string.Format(" smart_owner=%1 smart_component=%2 smart_tag=%3 perform=%4 exit=%5 native_return=%6",
			AICF_GroupTrace.Entity(m_SmartOwner), m_SmartAction, SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG,
			AICF_GroupTrace.Action(m_Perform), AICF_GroupTrace.Action(m_Exit), AICF_GroupTrace.Action(m_Return));
		details += string.Format(" window_start_ms=%1 window_deadline_ms=%2 observed_exit=%3 returned_at_ms=%4 return_position=%5 progress_written=0",
			m_iStarted, m_iDeadline, m_bObservedExit, m_iReturnedAtMs, m_vReturnPosition);
		details += string.Format(" return_completed_receipt=%1 return_completed_at_ms=%2 return_listener_attached=%3",
			m_bReturnCompleted, m_iReturnCompletedAtMs, m_bReturnListenerAttached);
		details += string.Format(" perform_completed_receipt=%1 perform_completed_at_ms=%2 perform_listener_attached=%3",
			m_bPerformCompleted, m_iPerformCompletedAtMs, m_bPerformListenerAttached);
		AICF_Stage3Diagnostics.Info(eventName, details);
	}
}
