// Full basket survives the empty-controller creation boundary without being
// available to unrelated native callbacks. Stable slot/request is the owner.
class AICF_AIQuotaPlannedGroup
{
	AICF_AIWorldQuotaService m_Service;
	AICF_MatchController m_Controller;
	int m_iEpoch;
	int m_iSerial;
	int m_iUnits;
	AICF_FactionState m_State;
	AICF_GroupSlot m_Slot;
	AICF_InfantryRecruitmentOrder m_Recruit;
	int m_iGeneration;
	int m_iToken;
	ref AICF_AIQuotaReservation m_Reservation;
	bool m_bBound;

	bool IsOwnerCurrent()
	{
		return m_Service && m_Controller && m_Service.IsOwnerCurrent() && m_Service.GetQuotaController() == m_Controller &&
			m_Service.GetEpoch() == m_iEpoch && m_iSerial > 0 && m_iUnits > 0 && !m_bBound;
	}

	bool IsWaitingCurrent()
	{
		if (!IsOwnerCurrent()) return false;
		if (m_Recruit)
			return m_Recruit.IsCurrent(m_Recruit.m_Slot) && m_Recruit.m_RecruitmentService &&
				m_Recruit.m_RecruitmentService.IsCurrentOrder(m_Recruit) && !m_Recruit.m_Donor &&
				m_Recruit.m_iGeneration == m_iGeneration && m_Recruit.m_iToken == m_iToken;
		return m_Controller.IsAIQuotaSlotCurrent(m_State, m_Slot) && !m_Slot.GetGroup() &&
			m_Slot.GetSpawnGeneration() == m_iGeneration && m_Slot.GetDeploymentSize() == m_iUnits;
	}

	bool IsPendingCurrent()
	{
		if (!IsOwnerCurrent()) return false;
		if (m_Recruit)
			return m_Recruit.m_RecruitmentService && m_Recruit.m_RecruitmentService.IsCurrentOrder(m_Recruit) &&
				m_Recruit.m_iGeneration == m_iGeneration && m_Recruit.m_iToken == m_iToken;
		if (!m_Controller.IsAIQuotaSlotCurrent(m_State, m_Slot) || m_Slot.GetDeploymentSize() != m_iUnits) return false;
		int generation = m_Slot.GetSpawnGeneration();
		return generation == m_iGeneration || (m_iGeneration < int.MAX && generation == m_iGeneration + 1);
	}

	bool CanBind(AICF_AIQuotaGroupOwner owner)
	{
		if (!IsOwnerCurrent() || !owner || !owner.CanDispatch() || owner.m_Controller != m_Controller ||
			owner.m_iEpoch != m_iEpoch || owner.GetBucket() != AICF_EAIQuotaBucket.COMBAT) return false;
		if (m_Recruit)
			return owner.m_Kind == AICF_EAIQuotaGroupOwner.DONOR && owner.m_Recruit == m_Recruit &&
				owner.m_iGeneration == m_iGeneration && owner.m_iRequestToken == m_iToken &&
				m_Recruit.m_Donor == owner.m_Group && m_Recruit.m_DonorId == owner.m_GroupId;
		return owner.m_Kind == AICF_EAIQuotaGroupOwner.SLOT && owner.m_FactionState == m_State && owner.m_Slot == m_Slot &&
			m_iGeneration < int.MAX && owner.m_iGeneration == m_iGeneration + 1 &&
			m_Slot.GetDeploymentSize() == m_iUnits && m_Slot.GetGroup() == owner.m_Group;
	}
}
