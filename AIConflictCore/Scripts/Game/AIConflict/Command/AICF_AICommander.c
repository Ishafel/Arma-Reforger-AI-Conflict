// Faction-scoped facade for autonomous strategic decisions. It never owns
// spawning, tickets, economy, transport, lifecycle, or waypoint entities.
class AICF_AICommander
{
	protected FactionKey m_sFactionKey;
	protected ref AICF_CommandAuthorityPolicy m_AuthorityPolicy;
	protected AICF_FactionState m_FactionState;
	protected SCR_CampaignFaction m_Faction;
	protected AICF_OrderPlanner m_OrderPlanner;
	protected AICF_ObjectiveGraph m_ObjectiveGraph;
	protected AICF_TargetSelector m_TargetSelector;

	void AICF_AICommander(
		AICF_FactionState factionState,
		SCR_CampaignFaction faction,
		AICF_CommandAuthorityPolicy authorityPolicy,
		AICF_OrderPlanner orderPlanner,
		AICF_ObjectiveGraph objectiveGraph,
		AICF_TargetSelector targetSelector)
	{
		m_FactionState = factionState;
		m_Faction = faction;
		m_AuthorityPolicy = authorityPolicy;
		m_OrderPlanner = orderPlanner;
		m_ObjectiveGraph = objectiveGraph;
		m_TargetSelector = targetSelector;
		if (m_Faction)
			m_sFactionKey = m_Faction.GetFactionKey();
	}

	FactionKey GetFactionKey()
	{
		return m_sFactionKey;
	}

	AICF_FactionState GetFactionState()
	{
		return m_FactionState;
	}

	SCR_CampaignFaction GetFaction()
	{
		return m_Faction;
	}

	bool IsEnabled()
	{
		return m_AuthorityPolicy && m_AuthorityPolicy.IsValid() &&
			m_FactionState && m_Faction &&
			m_AuthorityPolicy.IsAICommanderEnabled(m_sFactionKey);
	}

	int SelectConstructionType(array<bool> covered, int startType)
	{
		if (!Replication.IsServer() || !IsEnabled() || !covered || covered.Count() != AICF_EConstructionType.COUNT)
			return -1;
		for (int offset; offset < AICF_EConstructionType.COUNT; offset++)
		{
			int type = (startType + offset) % AICF_EConstructionType.COUNT;
			if (!covered[type])
				return type;
		}
		return -1;
	}

	// Read-only infantry navmesh context; не создаёт group/worker или assignment.
	AIPathfindingComponent GetConstructionPathfinding()
	{
		if (!IsEnabled())
			return null;
		for (int i; i < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION; i++)
		{
			AICF_GroupSlot slot = m_FactionState.GetSlot(i);
			if (slot && slot.GetGroup() && slot.GetGroup().GetFaction() == m_Faction)
				return AIPathfindingComponent.Cast(slot.GetGroup().FindComponent(AIPathfindingComponent));
		}
		return null;
	}

	bool OwnsSlot(AICF_GroupSlot slot)
	{
		return IsEnabled() && slot && slot.GetSlotId() >= 0 &&
			m_FactionState.GetSlot(slot.GetSlotId()) == slot;
	}

	bool Tick(AICF_MatchController matchController, string reason)
	{
		if (!IsEnabled() || !matchController)
			return false;

		return matchController.RunAICommanderTick(this, reason);
	}

	bool AssignOrder(
		AICF_GroupSlot slot,
		string reason,
		SCR_CampaignMilitaryBaseComponent excludedTarget = null,
		bool waypointSuspendedByVehicle = false)
	{
		if (!OwnsSlot(slot) || !m_OrderPlanner || !m_ObjectiveGraph ||
			!m_TargetSelector)
			return false;

		return m_OrderPlanner.AssignAICommanderOrder(
			slot,
			m_Faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			reason,
			excludedTarget,
			waypointSuspendedByVehicle);
	}

	bool ReconcileStrategicOrder(
		AICF_GroupSlot slot,
		string reason,
		int minimumDwellMs,
		int stableCandidateMs,
		bool waypointSuspendedByVehicle = false)
	{
		if (!OwnsSlot(slot) || !m_OrderPlanner || !m_ObjectiveGraph ||
			!m_TargetSelector)
			return false;

		return m_OrderPlanner.ReconcileAICommanderOrder(
			slot,
			m_Faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			reason,
			minimumDwellMs,
			stableCandidateMs,
			waypointSuspendedByVehicle);
	}

	bool AssignLossResponseOrder(
		AICF_GroupSlot slot,
		SCR_CampaignMilitaryBaseComponent lostBase,
		int minimumDwellMs,
		int stableCandidateMs,
		bool waypointSuspendedByVehicle = false)
	{
		if (!OwnsSlot(slot) || !m_OrderPlanner || !m_ObjectiveGraph ||
			!m_TargetSelector || !lostBase)
			return false;

		return m_OrderPlanner.AssignAICommanderLossResponseOrder(
			slot,
			m_Faction,
			m_ObjectiveGraph,
			m_TargetSelector,
			lostBase,
			minimumDwellMs,
			stableCandidateMs,
			waypointSuspendedByVehicle);
	}
}
