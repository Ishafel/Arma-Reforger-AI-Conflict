// Неисполняемый own GetIn под двумя exact passive Idle. Только read-only proof;
// flow отменяет свою команду, helper сохраняет прежние physical/quota gates.
class AICF_PassiveBoardingRecovery
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_VehicleLease m_Lease;
	protected AICF_StrategicAssignmentSnapshot m_Assignment;
	protected AICF_VehicleBoardingState m_State;
	protected AICF_VehicleBoardingTokenSet m_Tokens;
	protected ref AICF_VehicleBoardingActionToken m_Token;
	protected SCR_AIGroup m_Group;
	protected AIAgent m_Agent;
	protected ChimeraCharacter m_Actor;
	protected AIControlComponent m_Control;
	protected SCR_AIUtilityComponent m_Utility;
	protected SCR_AIGroupUtilityComponent m_GroupUtility;
	protected ref SCR_AIGetInVehicle m_Entry;
	protected ref AIActionBase m_Idle;
	protected ref AIActionBase m_GroupIdle;
	protected int m_iTransitions;
	protected int m_iDeadline;
	protected bool m_bOwnerCancelled;

	static AICF_PassiveBoardingRecovery Capture(AICF_TransportTrip trip, AICF_VehicleBoardingState state,
		AICF_VehicleBoardingActionToken token)
	{
		if (!trip || !trip.GetAssignment() || !state || !token) return null;
		AICF_PassiveBoardingRecovery proof = new AICF_PassiveBoardingRecovery();
		proof.m_Trip = trip;
		proof.m_Lease = trip.GetLease();
		proof.m_Assignment = trip.GetAssignment();
		proof.m_State = state;
		proof.m_Tokens = state.GetTokens();
		proof.m_Token = token;
		proof.m_Group = trip.GetAssignment().GetGroup();
		proof.m_Agent = token.GetAgent();
		proof.m_Actor = ChimeraCharacter.Cast(token.GetReservedEntity());
		proof.m_Entry = token.GetGetInAction();
		proof.m_iTransitions = trip.GetTransitionCount();
		proof.m_iDeadline = state.GetAbsoluteDeadlineMs();
		if (!proof.m_Agent || !proof.m_Actor || !proof.m_Group) return null;
		proof.m_Control = proof.m_Actor.GetAIControlComponent();
		proof.m_Utility = SCR_AIUtilityComponent.Cast(proof.m_Agent.FindComponent(SCR_AIUtilityComponent));
		proof.m_GroupUtility = SCR_AIGroupUtilityComponent.Cast(proof.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
		if (!proof.m_Utility || !proof.m_GroupUtility) return null;
		proof.m_Idle = proof.m_Utility.GetCurrentAction();
		proof.m_GroupIdle = proof.m_GroupUtility.GetCurrentAction();
		if (!proof.IsCurrent()) return null;
		return proof;
	}

	protected bool ContextCurrent()
	{
		if (!m_Trip || !m_Lease || !m_Assignment || !m_State || !m_Tokens) return false;
		if (!m_Token || !m_Group || !m_Agent || !m_Actor || !m_Control) return false;
		if (!m_Trip.IsValid() || m_Trip.IsTerminal() || !m_Trip.IsVehiclePresetCurrent() || m_Trip.GetPhase() != AICF_ETransportTripPhase.BOARDING) return false;
		if (m_Trip.GetLease() != m_Lease || m_Trip.GetAssignment() != m_Assignment || m_Assignment.GetGroup() != m_Group ||
			m_Trip.GetTransitionCount() != m_iTransitions || m_Trip.GetBoardingState() != m_State || m_State.AreExitEffectsApplied()) return false;
		int now = System.GetTickCount();
		if (now <= 0 || m_iDeadline <= 0 || now >= m_iDeadline || m_Trip.IsDeadlineReached(now) || m_State.GetAbsoluteDeadlineMs() != m_iDeadline) return false;
		if (m_State.GetTokens() != m_Tokens || m_Tokens.Count() > AICF_Stage1Config.MAX_GROUP_SIZE || m_Tokens.FindByAgent(m_Agent) != m_Token) return false;
		if (!m_Token.WasPresetQueued() || m_Token.WasShortRecoveryAttempted() || m_Token.GetProgressAgeMs() < 5000 ||
			m_Token.GetAgent() != m_Agent || m_Token.GetReservedEntity() != m_Actor || m_Token.GetGetInAction() != m_Entry) return false;
		if (!m_Entry || !m_Token.Matches(m_Trip, m_Lease, m_Group) || !m_Token.IsPhysicalMutationOwnerSafe()) return false;
		return m_Actor.GetAIControlComponent() == m_Control && m_Control.GetControlAIAgent() == m_Agent &&
			m_Agent.GetControlledEntity() == m_Actor && m_Agent.GetParentGroup() == m_Group;
	}

	// Null entries или непроверенный suffix не являются отсутствием конкурента.
	protected static bool QueueSafe(SCR_AIBaseUtilityComponent utility, AIActionBase idle, AIActionBase ownEntry)
	{
		if (!utility || !idle) return false;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		if (actions.Count() > 64) return false;
		bool foundIdle;
		foreach (AIActionBase action : actions)
		{
			if (!action) return false;
			if (action == idle) foundIdle = true;
			if (action == idle || action == ownEntry) continue;
			EAIActionState state = action.GetActionState();
			if (!action.GetRemoveAction() && state != EAIActionState.FAILED && state != EAIActionState.COMPLETED) return false;
		}
		return foundIdle;
	}

	protected bool PassiveCurrent()
	{
		if (!m_Utility || !m_GroupUtility || !m_Idle || !m_GroupIdle || !m_Actor || !m_Agent || !m_Group) return false;
		if (m_Agent.FindComponent(SCR_AIUtilityComponent) != m_Utility || m_Utility.m_OwnerEntity != m_Actor ||
			m_Group.FindComponent(SCR_AIGroupUtilityComponent) != m_GroupUtility) return false;
		if (m_Idle.Type() != SCR_AIIdleBehavior || m_GroupIdle.Type() != SCR_AIIdleActivity) return false;
		if (m_Idle.GetActionState() != EAIActionState.RUNNING || m_GroupIdle.GetActionState() != EAIActionState.RUNNING ||
			m_Idle.GetActionIsSuspended() || m_GroupIdle.GetActionIsSuspended() || m_Idle.GetRemoveAction() || m_GroupIdle.GetRemoveAction()) return false;
		return m_Utility.GetCurrentAction() == m_Idle && m_Utility.GetExecutedAction() == m_Idle &&
			m_GroupUtility.GetCurrentAction() == m_GroupIdle && m_GroupUtility.GetExecutedAction() == m_GroupIdle;
	}

	bool IsCurrent()
	{
		if (!ContextCurrent() || !PassiveCurrent() || !m_Entry) return false;
		EAIActionState state = m_Entry.GetActionState();
		if (!m_bOwnerCancelled)
		{
			if (state != EAIActionState.EVALUATED || m_Entry.GetActionIsSuspended() || m_Entry.GetRemoveAction() ||
				!m_Token.IsTrackedActionOwnedByUtility() || m_Token.IsTrackedActionCurrent()) return false;
		}
		else if (state != EAIActionState.FAILED && state != EAIActionState.COMPLETED) return false;
		if (!QueueSafe(m_Utility, m_Idle, m_Entry) || !QueueSafe(m_GroupUtility, m_GroupIdle, null)) return false;
		return ContextCurrent() && PassiveCurrent();
	}

	bool ConfirmOwnerCancellation()
	{
		m_bOwnerCancelled = true;
		return IsCurrent();
	}

	bool Matches(AICF_TransportTrip trip, AICF_VehicleBoardingState state, AICF_VehicleBoardingActionToken token)
	{
		return trip == m_Trip && state == m_State && token == m_Token;
	}
}
