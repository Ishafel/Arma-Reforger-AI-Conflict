// Pinned SCR_AIGetInVehicle.OnActionFailed очищает compartment даже когда
// OnActionSelected ещё не был передан native. Quota veto не владеет этим
// side effect: Fail/OnFail и invoker сохраняются, чужой seat не трогаем.
class AICF_AIQuotaVehicleFailure
{
	protected bool m_bAICFQuotaSelectionVeto;
	protected SCR_AIUtilityComponent m_AICFQuotaVetoUtility;
	protected IEntity m_AICFQuotaVetoActor;

	void FailBeforeDispatch(AIActionBase action, SCR_AIUtilityComponent utility)
	{
		m_bAICFQuotaSelectionVeto = true;
		m_AICFQuotaVetoUtility = utility;
		if (utility) m_AICFQuotaVetoActor = utility.m_OwnerEntity;
		if (action) action.Fail();
	}

	void AllowDispatch()
	{
		m_bAICFQuotaSelectionVeto = false;
		m_AICFQuotaVetoUtility = null;
		m_AICFQuotaVetoActor = null;
	}

	bool ApplyFailure(AIActionBase action, SCR_AIUtilityComponent utility)
	{
		if (!m_bAICFQuotaSelectionVeto || !action) return false;
		SCR_AIBehaviorBase behavior = SCR_AIBehaviorBase.Cast(action);
		if (!behavior) return false;
		// Штатный SCR_AIVehicleBehavior/SCR_AIActionBase failure lifecycle
		// без SCR_AIGetInVehicle compartment cleanup.
		if (utility && utility == m_AICFQuotaVetoUtility && utility.m_AIInfo &&
			utility.m_OwnerEntity == m_AICFQuotaVetoActor && AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_AICFQuotaVetoActor))
			utility.m_AIInfo.SetAIState(EUnitAIState.AVAILABLE);
		action.SetActionState(EAIActionState.FAILED);
		SCR_AIActivityBase relatedActivity = SCR_AIActivityBase.Cast(action.GetRelatedGroupActivity());
		if (relatedActivity) relatedActivity.OnChildBehaviorFinished(behavior);
		return true;
	}
}
