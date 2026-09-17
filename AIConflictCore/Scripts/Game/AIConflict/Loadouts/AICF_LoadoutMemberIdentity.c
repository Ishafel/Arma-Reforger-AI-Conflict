// Индекс берётся в точном stock SpawnGroupMember boundary, включая randomizer.
// Сравнение множеств до/после не зависит от порядка GetAgents().
modded class SCR_AIGroup
{
	protected bool m_bAICFTrackMembers;
	protected ref array<IEntity> m_aAICFSpawnMembers = {};

	void AICF_TrackMemberPositions()
	{
		if (!Replication.IsServer())
			return;
		m_bAICFTrackMembers = true;
		m_aAICFSpawnMembers.Resize(AICF_Stage1Config.MAX_GROUP_SIZE);
	}

	IEntity AICF_GetSpawnMember(int index)
	{
		if (!m_bAICFTrackMembers || index < 0 || index >= m_aAICFSpawnMembers.Count())
			return null;
		return m_aAICFSpawnMembers[index];
	}

	override protected bool SpawnGroupMember(bool snapToTerrain, int index, ResourceName res, bool editMode, bool isLast)
	{
		if (!m_bAICFTrackMembers || !Replication.IsServer() || editMode)
			return super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);
		array<AIAgent> before = {};
		GetAgents(before);
		bool result = super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);
		if (!result || index < 0 || index >= m_aAICFSpawnMembers.Count())
			return result;
		array<AIAgent> after = {};
		GetAgents(after);
		IEntity member;
		int added;
		foreach (AIAgent agent : after)
		{
			if (agent && !before.Contains(agent) && agent.GetParentGroup() == this)
			{
				member = agent.GetControlledEntity();
				added++;
			}
		}
		m_aAICFSpawnMembers[index] = null;
		if (added == 1 && member && index < m_aUnitPrefabSlots.Count() && m_aUnitPrefabSlots[index] == res)
			m_aAICFSpawnMembers[index] = member;
		return result;
	}
}
