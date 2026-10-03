// Только stage: production gameplay, ограничение длительности и проверка
// исключительного владения terminal hold. Клиент и fault injections не нужны.
modded class AICF_MatchController
{
	protected int m_iRecoverySoakStarted;
	protected bool m_bRecoverySoakFinished;
	protected int m_iRecoverySoakOwnerFailures;

	override protected void Update()
	{
		super.Update();
		string duration;
		if (!System.GetCLIParam("aicfRecoverySoakMs", duration) || !m_bRosterReady || m_bStopped || m_bRecoverySoakFinished)
			return;
		if (!m_iRecoverySoakStarted)
		{
			m_iRecoverySoakStarted = System.GetTickCount();
			Print("[AICF][RECOVERY_SOAK_STARTED] injections=NONE client=NONE");
		}
		RecoverySoakCheck(m_USState);
		RecoverySoakCheck(m_USSRState);
		if (System.GetTickCount(m_iRecoverySoakStarted) < Math.ClampInt(duration.ToInt(), 60000, 3600000))
			return;
		m_bRecoverySoakFinished = true;
		Print(string.Format("[AICF][RECOVERY_SOAK_FINISHED] elapsed_ms=%1 owner_failures=%2", System.GetTickCount(m_iRecoverySoakStarted), m_iRecoverySoakOwnerFailures));
		GetGame().RequestClose();
	}

	protected void RecoverySoakCheck(AICF_FactionState state)
	{
		for (int index; index < state.GetSlotCount(); index++)
		{
			AICF_GroupSlot slot = state.GetSlot(index);
			if (slot && slot.IsCombatReady() && slot.GetRouteRecoveryEpisode().IsBlocked(slot) && slot.HasPendingOrderRecovery())
			{
				m_iRecoverySoakOwnerFailures++;
				if (m_iRecoverySoakOwnerFailures == 1)
					Print("[AICF][RECOVERY_SOAK_OWNER_FAILURE] reason=EXHAUSTED_WITH_PENDING_REPAIR", LogLevel.ERROR);
			}
		}
	}
}
