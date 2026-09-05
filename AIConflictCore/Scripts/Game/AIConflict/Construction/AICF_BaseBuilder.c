// Один постоянный service slot на stock base. Поллинг не хранит отложенных
// callback к старому воплощению группы; generation используется и в evidence.
class AICF_BaseBuilder
{
	SCR_CampaignMilitaryBaseComponent m_Base;
	Faction m_Faction;
	SCR_AIGroup m_Group;
	EntityID m_GroupId = EntityID.INVALID;
	IEntity m_Character;
	EntityID m_CharacterId = EntityID.INVALID;
	AIWaypoint m_Waypoint;
	SCR_CampaignBuildingCompositionComponent m_Target;
	EntityID m_TargetId = EntityID.INVALID;
	int m_iSlotId;
	vector m_vWorkPosition;
	vector m_vTargetPosition;
	vector m_vDestination;
	vector m_vFootprintMin;
	vector m_vFootprintMax;
	int m_iGeneration;
	int m_iSpawnAtMs;
	int m_iIdleAtMs;
	int m_iRetryAtMs;
	int m_iMoveAtMs;
	int m_iLastOrderAtMs;
	int m_iWorkAtMs;
	bool m_bReturning;
	bool m_bRetiring;
	IEntity m_UsedTool;
	SCR_CharacterControllerComponent m_ToolController;
	bool m_bToolActive;
	int m_iToolRequestAtMs;
	ref array<SCR_CampaignBuildingCompositionComponent> m_aDeferredTargets = {};
	ref array<int> m_aDeferredUntilMs = {};

	void OnToolUseBegan(IEntity item, ItemUseParameters params)
	{
		if (!Replication.IsServer() || item != m_UsedTool || !m_Character || m_Character.GetID() != m_CharacterId ||
			!m_Group || m_Group.GetID() != m_GroupId || !m_Target || !m_Target.GetOwner() || m_Target.GetOwner().GetID() != m_TargetId)
			return;
		m_bToolActive = true;
		m_iWorkAtMs = 0;
	}

	void OnToolUseEnded(IEntity item, bool successful, ItemUseParameters params)
	{
		if (item != m_UsedTool)
			return;
		m_bToolActive = false;
		m_iWorkAtMs = 0;
	}

	bool IsOutsideFootprint(vector position)
	{
		return position[0] < m_vFootprintMin[0] - 0.5 || position[0] > m_vFootprintMax[0] + 0.5 ||
			position[2] < m_vFootprintMin[2] - 0.5 || position[2] > m_vFootprintMax[2] + 0.5;
	}
}
