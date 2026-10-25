// Разрешает только доказанную штатную цепочку OpenGate → exact-seat return.
// Transit observer не выдаёт actions и не меняет deadlines/progress trip.
class AICF_TransportNativeGate
{
	IEntity m_Member;
	EntityID m_Id;
	BaseCompartmentSlot m_Seat;
	SCR_AIUtilityComponent m_Utility;
	ref SCR_AIGetInVehicle m_Return;
	int m_iStarted;
	string m_sOperation;
	string m_sSeatKey;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	string m_sVehicleRpl;
	int m_iAssignmentRevision;
	int m_iRouteGeneration;

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
		foreach (AIActionBase candidate : actions)
		{
			SCR_AIPerformActionBehavior perform = SCR_AIPerformActionBehavior.Cast(candidate);
			if (!Live(perform) || perform.GetPriority() != SCR_AIActionBase.PRIORITY_BEHAVIOR_OPEN_NAVLINK_DOOR) continue;
			SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(perform.GetRelatedGroupActivity());
			if (!activity || activity.m_Utility != trip.GetAssignment().GetGroup().GetGroupUtilityComponent()) continue;
			SCR_AISmartActionComponent smart = perform.m_SmartActionComponent.m_Value;
			if (!smart || !smart.GetOwner() || vector.Distance(seat.GetVehicle().GetOrigin(), smart.GetOwner().GetOrigin()) > 50) continue;
			array<string> tags = {};
			smart.GetTags(tags);
			if (!tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)) continue;
			SCR_AIGetInVehicle returnAction;
			SCR_AIGetOutVehicle exitAction;
			foreach (AIActionBase sibling : actions)
			{
				if (!Live(sibling) || sibling.GetRelatedGroupActivity() != activity) continue;
				SCR_AIGetInVehicle getIn = SCR_AIGetInVehicle.Cast(sibling);
				if (getIn && getIn.m_Vehicle.m_Value == seat.GetOwner() && getIn.m_CompartmentToGetIn.m_Value == seat && getIn.GetPriority() == perform.GetPriority()) returnAction = getIn;
				SCR_AIGetOutVehicle getOut = SCR_AIGetOutVehicle.Cast(sibling);
				if (getOut && getOut.m_Vehicle.m_Value == seat.GetOwner() && getOut.GetPriority() == perform.GetPriority() + 100) exitAction = getOut;
			}
			if (!returnAction || (utility.GetCurrentBehavior() != perform && (!exitAction || utility.GetCurrentBehavior() != exitAction))) continue;
			AICF_TransportNativeGate gate = new AICF_TransportNativeGate();
			gate.m_Member = member;
			gate.m_Id = member.GetID();
			gate.m_Seat = seat;
			gate.m_Utility = utility;
			gate.m_Return = returnAction;
			gate.m_iStarted = System.GetTickCount();
			gate.m_sOperation = trip.GetOperationId();
			gate.m_sSeatKey = AICF_VehiclePreset.SeatKey(seat);
			gate.m_Group = trip.GetAssignment().GetGroup();
			gate.m_GroupId = gate.m_Group.GetID();
			gate.m_Vehicle = trip.GetLease().GetVehicle();
			gate.m_VehicleId = gate.m_Vehicle.GetID();
			gate.m_sVehicleRpl = Replication.FindItemId(gate.m_Vehicle).ToString();
			gate.m_iAssignmentRevision = trip.GetAssignment().GetAssignmentRevision();
			gate.m_iRouteGeneration = trip.GetMovementState().GetRouteGeneration();
			return gate;
		}
		return null;
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
			if (seats[index].GetType() != ECompartmentType.PILOT && seats[index].GetType() != ECompartmentType.TURRET) continue;
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
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Member);
		if (!character || !character.GetAIControlComponent()) return false;
		AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
		return agent && agent.GetParentGroup() == m_Group && SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent)) == m_Utility;
	}

	bool IsReturned()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Member);
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return access && character.IsInVehicle() && access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() &&
			access.GetCompartment() == m_Seat && m_Seat.GetOccupant() == m_Member && CompartmentAccessComponent.GetVehicleIn(character) == m_Vehicle;
	}

	bool IsExpired(int now) { return now - m_iStarted >= 60000; }

	void Log(string eventName)
	{
		AICF_Stage3Diagnostics.Info(eventName, string.Format("operation_id=%1 member=%2 seat=%3 vehicle=%4 elapsed_ms=%5 route_generation=%6 assignment_revision=%7",
			m_sOperation, AICF_GroupTrace.Entity(m_Member), m_sSeatKey, AICF_GroupTrace.Entity(m_Vehicle),
			System.GetTickCount() - m_iStarted, m_iRouteGeneration, m_iAssignmentRevision));
	}
}
