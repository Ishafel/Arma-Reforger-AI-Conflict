modded class SCR_AIGetInVehicle
{
	protected ref AICF_AIQuotaVehicleDispatch m_AICFQuotaEntry;
	protected ref AICF_AIQuotaVehicleFailure m_AICFQuotaFailure = new AICF_AIQuotaVehicleFailure();

	protected bool AICF_PeekQuotaEntry()
	{
		IEntity actor;
		if (m_Utility) actor = m_Utility.m_OwnerEntity;
		return AICF_AIQuotaVehicleDispatch.Peek(actor, m_Vehicle.m_Value, m_CompartmentToGetIn.m_Value);
	}

	override float CustomEvaluate()
	{
		float nativePriority = super.CustomEvaluate();
		if (!AICF_PeekQuotaEntry()) return -1000000;
		return nativePriority;
	}

	override float EvaluatePriorityLevel()
	{
		float nativeLevel = super.EvaluatePriorityLevel();
		if (!AICF_PeekQuotaEntry()) return -1000000;
		return nativeLevel;
	}

	override void OnActionSelected()
	{
		IEntity actor;
		if (m_Utility) actor = m_Utility.m_OwnerEntity;
		AICF_AIQuotaVehicleDispatch captured = m_AICFQuotaEntry;
		if (!captured || !captured.IsCurrent())
		{
			if (captured) captured.Cancel();
			captured = AICF_AIQuotaVehicleDispatch.Begin(actor, m_Vehicle.m_Value, m_CompartmentToGetIn.m_Value);
			m_AICFQuotaEntry = captured;
		}
		if (!captured || !captured.IsCurrent())
		{
			if (captured) captured.Cancel();
			m_AICFQuotaFailure.FailBeforeDispatch(this, m_Utility);
			return;
		}
		// Void selection не является bool GetIn acceptance.
		captured.MarkDispatched();
		if (!captured.IsCurrent())
		{
			captured.Cancel();
			m_AICFQuotaFailure.FailBeforeDispatch(this, m_Utility);
			return;
		}
		m_AICFQuotaFailure.AllowDispatch();
		super.OnActionSelected();
	}

	protected void AICF_CancelQuotaEntry(AICF_AIQuotaVehicleDispatch captured)
	{
		if (captured) captured.Cancel();
		if (m_AICFQuotaEntry == captured) m_AICFQuotaEntry = null;
	}

	override void OnActionFailed()
	{
		AICF_AIQuotaVehicleDispatch captured = m_AICFQuotaEntry;
		if (!m_AICFQuotaFailure.ApplyFailure(this, m_Utility)) super.OnActionFailed();
		AICF_CancelQuotaEntry(captured);
	}

	override void OnActionCompleted()
	{
		AICF_AIQuotaVehicleDispatch captured = m_AICFQuotaEntry;
		super.OnActionCompleted();
		AICF_CancelQuotaEntry(captured);
	}

	override void OnActionDeselected()
	{
		AICF_AIQuotaVehicleDispatch captured = m_AICFQuotaEntry;
		super.OnActionDeselected();
		AICF_CancelQuotaEntry(captured);
	}

	override void OnActionRemoved()
	{
		AICF_AIQuotaVehicleDispatch captured = m_AICFQuotaEntry;
		super.OnActionRemoved();
		AICF_CancelQuotaEntry(captured);
	}
}
