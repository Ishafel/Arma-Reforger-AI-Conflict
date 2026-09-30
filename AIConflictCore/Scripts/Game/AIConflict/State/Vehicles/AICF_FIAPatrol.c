// Стабильный слот FIA. После боевой потери новая машина не создаётся.
class AICF_FIAPatrol
{
	int m_iSlot;
	int m_iGeneration = 1;
	int m_iRequestedAtMs;
	int m_iRetryAtMs;
	int m_iSpawnAttempt;
	int m_iGraphRevision;
	int m_iLeg;
	int m_iLegAtMs;
	int m_iRouteRetries;
	float m_fBestEndpointDistance;
	ref AICF_FIAPatrolMoveFailure m_MoveFailure;
	bool m_bReady;
	bool m_bRetired;
	SCR_CampaignFaction m_Faction;
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	ChimeraCharacter m_Driver;
	ChimeraCharacter m_Gunner;
	EntityID m_DriverId;
	EntityID m_GunnerId;
	BaseCompartmentSlot m_PilotSeat;
	BaseCompartmentSlot m_TurretSeat;
	AIWaypoint m_Waypoint;
	EntityID m_WaypointId;
	SCR_CampaignMilitaryBaseComponent m_Current;
	SCR_CampaignMilitaryBaseComponent m_Target;
	SCR_CampaignMilitaryBaseComponent m_Previous;
	vector m_vEndpoint;
	vector m_vLegStart;
	vector m_vProgress;
	ref map<SCR_CampaignMilitaryBaseComponent, int> m_mVisits = new map<SCR_CampaignMilitaryBaseComponent, int>();
	ref AICF_VehicleLease m_Lease;

	bool VehicleIdentity()
	{
		return Replication.IsServer() && m_Vehicle && m_Vehicle.GetID() == m_VehicleId &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle) &&
			SCR_Faction.GetEntityFaction(m_Vehicle) == m_Faction;
	}

	bool GroupIdentity()
	{
		return Replication.IsServer() && m_Group && m_Group.GetID() == m_GroupId && m_Group.GetFaction() == m_Faction;
	}

	bool CrewIdentity()
	{
		if (!GroupIdentity() || !m_Driver || !m_Gunner || m_Driver.GetID() != m_DriverId || m_Gunner.GetID() != m_GunnerId)
			return false;
		array<AIAgent> agents = {};
		m_Group.GetAgents(agents);
		bool driver, gunner;
		foreach (AIAgent agent : agents)
		{
			if (!agent) continue;
			IEntity entity = agent.GetControlledEntity();
			if (entity == m_Driver) driver = true;
			if (entity == m_Gunner) gunner = true;
		}
		return driver && gunner && AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Driver) &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Gunner);
	}

	bool Seated()
	{
		return VehicleIdentity() && CrewIdentity() && m_PilotSeat && m_TurretSeat &&
			m_PilotSeat.GetOccupant() == m_Driver && m_TurretSeat.GetOccupant() == m_Gunner;
	}

	void Log(string eventName, string details)
	{
		AICF_Stage3Diagnostics.Info(eventName, string.Format("faction=FIA numeric_slot=%1 generation=%2 vehicle=%3 group=%4 graph_revision=%5 leg=%6 ",
			m_iSlot, m_iGeneration, m_VehicleId, m_GroupId, m_iGraphRevision, m_iLeg) + details);
	}
}

// Снимок принадлежит одному waypoint одного участка; новый приказ его отменяет.
class AICF_FIAPatrolMoveFailure
{
	Vehicle m_Vehicle;
	EntityID m_VehicleId;
	EntityID m_DriverId;
	EntityID m_GunnerId;
	SCR_CampaignMilitaryBaseComponent m_Target;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	AIWaypoint m_Waypoint;
	EntityID m_WaypointId;
	int m_iGeneration;
	int m_iLeg;
	int m_iGraphRevision;

	void AICF_FIAPatrolMoveFailure(AICF_FIAPatrol p)
	{
		m_Vehicle = p.m_Vehicle;
		m_VehicleId = p.m_VehicleId;
		m_DriverId = p.m_DriverId;
		m_GunnerId = p.m_GunnerId;
		m_Target = p.m_Target;
		m_Group = p.m_Group;
		m_GroupId = p.m_GroupId;
		m_Waypoint = p.m_Waypoint;
		m_WaypointId = p.m_WaypointId;
		m_iGeneration = p.m_iGeneration;
		m_iLeg = p.m_iLeg;
		m_iGraphRevision = p.m_iGraphRevision;
	}

	bool IsCurrent(AICF_FIAPatrol p)
	{
		if (!Replication.IsServer() || !p || p.m_bRetired || !p.m_bReady) return false;
		if (!p.GroupIdentity() || !p.VehicleIdentity() || !p.CrewIdentity()) return false;
		if (p.m_Vehicle != m_Vehicle || p.m_VehicleId != m_VehicleId ||
			p.m_DriverId != m_DriverId || p.m_GunnerId != m_GunnerId || p.m_Target != m_Target) return false;
		if (p.m_Group != m_Group || p.m_GroupId != m_GroupId ||
			p.m_iGeneration != m_iGeneration || p.m_iLeg != m_iLeg || p.m_iGraphRevision != m_iGraphRevision) return false;
		return m_Waypoint && p.m_Waypoint == m_Waypoint &&
			m_Waypoint.GetID() == m_WaypointId && p.m_WaypointId == m_WaypointId;
	}
}
