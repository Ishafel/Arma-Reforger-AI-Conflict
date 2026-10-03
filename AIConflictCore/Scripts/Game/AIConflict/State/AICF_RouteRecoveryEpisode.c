// Общий срок отсутствия движения не принадлежит assignment/waypoint.
// Перевыдача приказа и временный hold не являются физическим прогрессом.
class AICF_RouteRecoveryEpisode
{
	static const int DEADLINE_MS = 180000;
	protected bool m_bActive;
	protected EntityID m_GroupId;
	protected EntityID m_LeaderId;
	protected int m_iGeneration;
	protected int m_iStartedAtMs;
	protected int m_iGraphRevision = -1;
	protected int m_iPlayerIntentRevision;
	protected bool m_bPlayerIntent;
	protected vector m_vAnchor;
	protected vector m_vDestination;
	protected bool m_bExhausted;
	protected bool m_bHoldAttempted;

	void Begin(AICF_GroupSlot slot)
	{
		if (!slot || !slot.GetGroup())
			return;
		if (m_bActive && m_GroupId == slot.GetGroup().GetID() && m_iGeneration == slot.GetSpawnGeneration())
			return;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (!leader)
			return;
		m_bActive = true;
		m_GroupId = slot.GetGroup().GetID();
		m_LeaderId = leader.GetID();
		m_iGeneration = slot.GetSpawnGeneration();
		m_iStartedAtMs = System.GetTickCount();
		m_iGraphRevision = -1;
		m_iPlayerIntentRevision = slot.GetStrategicIntentRevision();
		m_bPlayerIntent = slot.HasPlayerStrategicIntent();
		m_vAnchor = leader.GetOrigin();
		m_vDestination = m_vAnchor;
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		if (slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION)
			m_vDestination = slot.GetTargetPosition();
		else if (target && target.GetOwner())
			m_vDestination = target.GetOwner().GetOrigin();
		else if (slot.GetWaypoint())
			m_vDestination = slot.GetWaypoint().GetOrigin();
		m_bExhausted = false;
		m_bHoldAttempted = false;
	}

	string Observe(AICF_GroupSlot slot, int graphRevision)
	{
		if (!m_bActive)
			return string.Empty;
		if (!slot.GetGroup() || slot.GetGroup().GetID() != m_GroupId || slot.GetSpawnGeneration() != m_iGeneration)
		{
			m_bActive = false;
			return string.Empty;
		}
		if (m_iGraphRevision < 0)
			m_iGraphRevision = graphRevision;
		// RETURN_TO_AI уже очистил intent: учитываем и прежнего владельца.
		// Автоматическая смена AI intent не продлевает episode без движения.
		if ((m_bExhausted && m_iGraphRevision != graphRevision) ||
			((m_bPlayerIntent || slot.HasPlayerStrategicIntent()) && slot.GetStrategicIntentRevision() != m_iPlayerIntentRevision))
		{
			m_bActive = false;
			return "CONTEXT_CHANGED";
		}
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		// Новая цель может лежать в другой стороне. Срок и исходная физическая
		// точка сохраняются; оцениваем фактическое движение к текущему route leg.
		if (slot.GetWaypoint() && !slot.IsTemporaryRouteReplanHold() && !slot.IsPersistentStuckFieldHold())
			m_vDestination = slot.GetWaypoint().GetOrigin();
		if (leader && leader.GetID() != m_LeaderId)
		{
			// Смена лидера не считается движением и не продлевает срок.
			m_LeaderId = leader.GetID();
			m_vAnchor = leader.GetOrigin();
		}
		if (leader && !slot.IsTemporaryRouteReplanHold() && !slot.IsPersistentStuckFieldHold() &&
			slot.GetWaypoint() && slot.GetGroup().GetCurrentWaypoint() == slot.GetWaypoint() &&
			vector.DistanceSqXZ(leader.GetOrigin(), m_vAnchor) >= 225 &&
			vector.DistanceXZ(m_vAnchor, m_vDestination) - vector.DistanceXZ(leader.GetOrigin(), m_vDestination) >= 5)
		{
			m_bActive = false;
			return "PHYSICAL_PROGRESS";
		}
		if (GetAgeMs() >= DEADLINE_MS)
		{
			m_bExhausted = true;
			return "EXHAUSTED";
		}
		return string.Empty;
	}

	bool TakeHoldAttempt()
	{
		if (m_bHoldAttempted)
			return false;
		m_bHoldAttempted = true;
		return true;
	}

	bool IsBlocked(AICF_GroupSlot slot)
	{
		return m_bActive && m_bExhausted && slot.GetGroup() &&
			slot.GetGroup().GetID() == m_GroupId && slot.GetSpawnGeneration() == m_iGeneration;
	}

	int GetAgeMs()
	{
		return System.GetTickCount(m_iStartedAtMs);
	}
}
