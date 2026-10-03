// Неудачный подход не разрешается снова одним лишь истечением cooldown.
// Запись не владеет waypoint/donor и никогда не изменяет группу.
class AICF_RecruitmentApproachFailure
{
	AICF_GroupSlot m_Slot;
	FactionKey m_sFaction;
	EntityID m_GroupId;
	int m_iGeneration;
	EntityID m_ServiceId;
	SCR_ServicePointComponent m_Service;
	vector m_vServicePosition;
	vector m_vGroupPosition;
	int m_iGraphRevision;

	void LogRearmed()
	{
		if (!m_Slot)
			return;
		AICF_Stage4Diagnostics.Info("INFANTRY_RECRUITMENT_APPROACH_REARMED", string.Format(
			"faction=%1 slot=%2 generation=%3 service=%4 reason=CONTEXT_CHANGED movement_confirmation=NONE",
			m_sFaction, m_Slot.GetSlotId(), m_iGeneration, m_ServiceId));
	}

	bool IsContextCurrent(int graphRevision)
	{
		if (!m_Slot || !m_Slot.GetGroup() || m_Slot.GetGroup().GetID() != m_GroupId ||
			m_Slot.GetSpawnGeneration() != m_iGeneration || graphRevision != m_iGraphRevision ||
			!m_Service || !m_Service.GetOwner() || m_Service.GetOwner().GetID() != m_ServiceId ||
			m_Service.GetServiceState() != SCR_EServicePointStatus.ONLINE ||
			vector.DistanceSqXZ(m_Service.GetOwner().GetOrigin(), m_vServicePosition) > 1)
			return false;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_Slot.GetGroup());
		return leader && vector.DistanceSqXZ(leader.GetOrigin(), m_vGroupPosition) < 1225;
	}
}
