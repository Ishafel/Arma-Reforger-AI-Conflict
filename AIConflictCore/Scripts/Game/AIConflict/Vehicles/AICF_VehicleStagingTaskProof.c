// Actual selected/executing acquisition movement, independent of a group Move
// waypoint already completed by stock AI. Read-only; never grants idle time.
class AICF_VehicleStagingTaskProof
{
	static bool HasCurrentTask(AICF_TransportTrip trip, int currentBaseRevision, int nowMs)
	{
		if (!Replication.IsServer() || !trip || !trip.IsValid() || trip.IsTerminal() || trip.GetLease() ||
			trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE || !trip.IsVehiclePresetCurrent() ||
			trip.IsDeadlineReached(nowMs)) return false;
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		if (!slot || !slot.IsCombatReady() || !assignment || !assignment.IsValid() || currentBaseRevision < 0 ||
			assignment.GetBaseRevision() != currentBaseRevision) return false;
		SCR_AIGroup group = assignment.GetGroup();
		if (!group || slot.GetGroup() != group || slot.GetSlotId() != trip.GetSlotId() ||
			slot.GetSpawnGeneration() != trip.GetGroupGeneration() ||
			slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision() ||
			slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision() ||
			slot.GetTargetKind() != assignment.GetTargetKind() || slot.GetTargetPosition() != assignment.GetTargetPosition() ||
			slot.GetTargetBase() != assignment.GetTargetBase() ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group) || !group.GetFaction() ||
			group.GetFaction().GetFactionKey() != trip.GetFactionKey()) return false;
		AICF_VehicleRequestState request = trip.GetRequestState();
		if (!request || request.GetRequestGeneration() <= 0 || request.IsDeadlineReached(nowMs)) return false;
		AICF_VehicleSpawnPlan plan = request.GetSpawnPlan();
		if (!plan || !plan.IsValid() || !plan.RequiresManualRally() || nowMs >= plan.GetApproachDeadlineMs()) return false;
		AICF_VehicleSpawnSiteReservation reservation = plan.GetReservation();
		if (!reservation || reservation.IsExpired(nowMs) || !reservation.MatchesTrip(trip, request.GetRequestGeneration())) return false;
		for (int index; index < plan.GetStagingActionTokenCount(); index++)
		{
			AICF_VehicleStagingActionToken token = plan.GetStagingActionToken(index);
			if (!token || !token.Matches(trip, request.GetRequestGeneration(), group) || !token.MatchesTaskAssignment(assignment) ||
				token.GetTargetPosition() != plan.GetStagingPosition() || !token.IsActionOwnedByUtility()) continue;
			IEntity actor = token.GetReservedEntity();
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor) || !SCR_AIDamageHandling.IsConscious(actor) ||
				!AICF_VehicleNativeSubgroup.CapturedMember(trip, actor) || CompartmentAccessComponent.GetVehicleIn(actor)) continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(actor);
			if (!character || !character.GetAIControlComponent() || character.GetAIControlComponent().GetControlAIAgent() != token.GetAgent()) continue;
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()) continue;
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(token.GetAgent().FindComponent(SCR_AIUtilityComponent));
			SCR_AIMoveIndividuallyBehavior action = token.GetAction();
			if (!utility || utility.m_OwnerEntity != actor || !action || action.m_Utility != utility ||
				action.GetActionState() != EAIActionState.RUNNING || action.GetRemoveAction() || action.GetActionIsSuspended() ||
				utility.GetCurrentAction() != action || utility.GetExecutedAction() != action ||
				action.m_vPosition.m_Value != token.GetTargetPosition() || action.m_Entity.m_Value) continue;
			return true;
		}
		return false;
	}
}
