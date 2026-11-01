modded class SCR_AIGroup
{
	protected string m_sAICFRosterSpawnOwner = "SCR_AIWORLD_REQUEST";

	void AICF_RecordRosterBatchOwner(bool queueFallback)
	{
		if (!Replication.IsServer()) return;
		m_sAICFRosterSpawnOwner = "AICF_BOUND_NATIVE_BATCH";
		if (queueFallback) m_sAICFRosterSpawnOwner = "AICF_BOUND_NATIVE_BATCH_THEN_SCR_AIWORLD_REQUEST";
	}

	string AICF_GetRosterSpawnOwner()
	{
		return m_sAICFRosterSpawnOwner;
	}

	// RequestSpawn also initializes authored vehicles/waypoints. A direct member
	// batch is allowed only when all three stock side-effect collections are empty.
	bool AICF_HasEmptyRosterAttachments()
	{
		return m_aStaticVehicles && m_aStaticVehicles.IsEmpty() &&
			m_aStaticWaypoints && m_aStaticWaypoints.IsEmpty() &&
			m_aSpawnedWaypoints && m_aSpawnedWaypoints.IsEmpty();
	}
}
