// Подтверждённое намерение stable slot; не разрешает spawn, transfer или debit.
class AICF_DeferredVehiclePreset
{
	AICF_GroupSlot m_Slot;
	SCR_CampaignFaction m_Faction;
	string m_sPreset;
	int m_iVehicleRevision;
	int m_iGeneration;
	int m_iAssignment;
	int m_iLoadout;
	int m_iSize;
	int m_iAssigned;
	int m_iExpires;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	ref array<EntityID> m_aMembers = {};

	void AICF_DeferredVehiclePreset(AICF_GroupSlot slot, SCR_CampaignFaction faction, AICF_VehiclePreset preset)
	{
		m_Slot = slot;
		m_Faction = faction;
		m_sPreset = preset.Encode();
		m_iVehicleRevision = slot.GetVehiclePresetRevision();
		m_iGeneration = slot.GetSpawnGeneration();
		m_iAssignment = slot.GetStrategicAssignmentRevision();
		m_iLoadout = slot.GetLoadoutRevision();
		m_iSize = slot.GetDesiredSize();
		m_iExpires = System.GetTickCount() + 60000;
		m_Group = slot.GetGroup();
		if (m_Group) m_GroupId = m_Group.GetID();
		for (int index; index < AICF_Stage1Config.MAX_GROUP_SIZE; index++)
		{
			IEntity member = slot.GetRosterMember(index);
			EntityID id = EntityID.INVALID;
			if (member) id = member.GetID();
			m_aMembers.Insert(id);
			if (preset.m_aMembers.Contains(index)) m_iAssigned++;
		}
	}

	static bool NeedsRoster(AICF_GroupSlot slot)
	{
		return slot && (!slot.IsCombatReady() || !slot.GetGroup() || slot.IsRecruitingInfantry() ||
			AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) < slot.GetDesiredSize());
	}

	bool ConfirmationCurrent(AICF_GroupSlot slot, string data)
	{
		if (!Replication.IsServer() || slot != m_Slot || data != m_sPreset ||
			slot.GetVehiclePresetRevision() != m_iVehicleRevision || slot.GetSpawnGeneration() != m_iGeneration ||
			slot.GetStrategicAssignmentRevision() != m_iAssignment || slot.GetLoadoutRevision() != m_iLoadout ||
			slot.GetDesiredSize() != m_iSize || slot.GetGroup() != m_Group ||
			(m_Group && m_Group.GetID() != m_GroupId) || System.GetTickCount() > m_iExpires) return false;
		for (int index; index < m_aMembers.Count(); index++)
		{
			IEntity member = slot.GetRosterMember(index);
			if (!member)
			{
				if (m_aMembers[index] != EntityID.INVALID) return false;
			}
			else if (member.GetID() != m_aMembers[index]) return false;
		}
		return true;
	}

	// После подтверждения разрешение относится к numeric positions будущего
	// полного состава. Новый snapshot перед transfer создаёт VehicleCrewPlan.
	bool BindingCurrent()
	{
		return Replication.IsServer() && m_Slot && m_Faction &&
			m_Slot.GetVehiclePresetRevision() == m_iVehicleRevision &&
			m_Slot.GetVehiclePresetData() == m_sPreset && m_Slot.GetDesiredSize() == m_iSize &&
			(!m_Slot.GetGroup() || m_Slot.GetGroup().GetFaction() == m_Faction);
	}
}
