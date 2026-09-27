// Contact должен оставаться валидным как в очереди, так и во время ожидания канала.
// Остальные типы запросов и штатная передача не изменяются.
modded class SCR_AICommsHandler
{
	static bool AICF_IsInvalidContact(SCR_AITalkRequest request)
	{
		return request && request.m_eCommType == ECommunicationType.REPORT_CONTACT &&
			(!request.m_Entity || request.m_vPosition == vector.Zero);
	}

	override void Update(float timeSlice)
	{
		for (int index = m_aRequestQueue.Count() - 1; index >= 0; index--)
		{
			SCR_AITalkRequest request = m_aRequestQueue[index];
			if (!AICF_IsInvalidContact(request))
				continue;
			m_aRequestQueue.RemoveOrdered(index);
			FailRequest(request);
			Print("[AICF][CONTACT_REQUEST_REJECTED] phase=QUEUE reason=MISSING_ENTITY_OR_POSITION", LogLevel.WARNING);
		}
		if (m_eState == SCR_EAICommunicationState.WAITING && AICF_IsInvalidContact(m_CurrentRequest))
		{
			FailRequest(m_CurrentRequest);
			m_CurrentRequest = null;
			SwitchToState(SCR_EAICommunicationState.IDLE);
			Print("[AICF][CONTACT_REQUEST_REJECTED] phase=WAITING reason=MISSING_ENTITY_OR_POSITION", LogLevel.WARNING);
		}
		super.Update(timeSlice);
	}
}
