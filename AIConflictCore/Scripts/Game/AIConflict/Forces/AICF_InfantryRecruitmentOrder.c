// Один ограниченный визит stable slot. Служба владеет записью, slot — weak reference.
class AICF_InfantryRecruitmentOrder
{
	AICF_GroupSlot m_Slot;
	SCR_CampaignFaction m_Faction;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	int m_iGeneration;
	int m_iAssignment;
	int m_iIntent;
	int m_iGraphRevision;
	int m_iToken;
	SCR_CampaignMilitaryBaseComponent m_Base;
	EntityID m_BaseId;
	SCR_ServicePointComponent m_Service;
	EntityID m_ServiceId;
	vector m_vPosition;
	AIWaypoint m_Waypoint;
	int m_iStartedAtMs;
	int m_iArrivedAtMs;
	int m_iNextPurchaseAtMs;
	SCR_AIGroup m_Donor;
	EntityID m_DonorId;
	int m_iSpawnAtMs;
	ResourceName m_sPrefab;
	int m_iMemberIndex;
	string m_sRole;
	int m_iCost;
	float m_fPaid;

	bool IsCurrent(AICF_GroupSlot slot)
	{
		return slot && slot == m_Slot && slot.IsCombatReady() &&
			slot.GetGroup() == m_Group && m_Group && m_Group.GetID() == m_GroupId &&
			m_Group.GetFaction() == m_Faction && slot.GetSpawnGeneration() == m_iGeneration &&
			slot.GetStrategicAssignmentRevision() == m_iAssignment &&
			slot.GetStrategicIntentRevision() == m_iIntent && slot.GetWaypoint() == m_Waypoint &&
			slot.GetUnitType() == AICF_EGroupUnitType.INFANTRY && !slot.HasPlayerStrategicIntent();
	}

	bool HasSafeBarracks()
	{
		if (!m_Base || !m_Base.GetOwner() || m_Base.GetOwner().GetID() != m_BaseId ||
			!m_Base.IsInitialized() || m_Base.GetFaction() != m_Faction || m_Base.IsBeingCaptured() ||
			m_Base.GetCaptureState() != SCR_EBaseCaptureState.NONE || !m_Service ||
			!m_Service.GetOwner() || m_Service.GetOwner().GetID() != m_ServiceId ||
			m_Service.GetFaction() != m_Faction ||
			m_Service.GetType() != SCR_EServicePointType.BARRACKS ||
			m_Service.GetServiceState() != SCR_EServicePointStatus.ONLINE ||
			vector.DistanceSqXZ(m_Service.GetOwner().GetOrigin(), m_vPosition) > 1)
			return false;
		array<SCR_ServicePointComponent> services = {};
		m_Base.GetServices(services);
		return services.Contains(m_Service);
	}

	bool IsPhysicallyPresent()
	{
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_Group);
		return leader && vector.DistanceSqXZ(leader.GetOrigin(), m_vPosition) <=
			AICF_InfantryRecruitmentConfig.ARRIVAL_METERS * AICF_InfantryRecruitmentConfig.ARRIVAL_METERS &&
			AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(m_Group) == 0;
	}

	void Log(string eventName, string details)
	{
		AICF_Stage4Diagnostics.Info(eventName, string.Format(
			"faction=%1 slot=%2 generation=%3 token=%4 base=%5 group=%6",
			m_Faction.GetFactionKey(), m_Slot.GetSlotId(), m_iGeneration, m_iToken,
			AICF_Stage1Diagnostics.BaseKey(m_Base), m_GroupId) + " " + details);
	}
}
