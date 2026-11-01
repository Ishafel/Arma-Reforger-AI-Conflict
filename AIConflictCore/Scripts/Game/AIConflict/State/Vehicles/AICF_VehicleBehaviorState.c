// Долговременное владение одной оплаченной машиной между пешими эпизодами.
// Таймер доставки приостановлен только на бой/пеший приказ, посадка ограничена.
class AICF_VehicleBehaviorState
{
	ref AICF_VehicleParking m_Parking;
	bool m_bThreatDeployment;
	bool m_bSawCombat;
	bool m_bReboarding;
	bool m_bParkedAfterCombat;
	int m_iDeploymentIntent;
	int m_iReboardDeadline;
	int m_iDeploymentDeadline;
	int m_iCrewRestoreDeadline;
	int m_iCombatOverrideIntent = -1;
	int m_iSettledPolls;
	int m_iPausedAt;
	int m_iLastReport;
	int m_iNextReboardTraceAt;
	ref array<IEntity> m_aDeployed = {};
	ref array<ref AICF_VehicleBehaviorAction> m_aActions = {};
	ref AICF_ReboardSeatConflictScope m_ReboardSeatConflict;

	int PausedMs(int nowMs)
	{
		if (m_iPausedAt <= 0) return 0;
		return Math.Max(0, nowMs - m_iPausedAt);
	}

	void SetPaused(bool paused, int nowMs)
	{
		if (paused && m_iPausedAt <= 0) m_iPausedAt = nowMs;
		else if (!paused) m_iPausedAt = 0;
	}
}
