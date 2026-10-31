// Полный bounded readback registered native agents. Snapshot не назначает
// ownership по faction и не меняет native counters/activation/LOD.
class AICF_AIQuotaAgentRow
{
	AIAgent m_Agent;
	EntityID m_AgentId;
	IEntity m_Entity;
	EntityID m_EntityId;
	AIGroup m_Group;
	bool m_bCounted;
	bool m_bActivated;
	bool m_bVehicle;

	bool IsCurrent(BaseWorld world)
	{
		if (!world || !m_Agent || m_Agent.GetWorld() != world || m_Agent.GetID() != m_AgentId ||
			m_Agent.GetControlledEntity() != m_Entity || m_Agent.GetParentGroup() != m_Group ||
			m_Agent.CountTowardsAIWorldLimit() != m_bCounted || m_Agent.IsAIActivated() != m_bActivated)
			return false;
		if (m_Entity && (m_Entity.GetWorld() != world || m_Entity.GetID() != m_EntityId ||
			world.FindEntityByID(m_EntityId) != m_Entity))
			return false;
		if (m_Group && (m_Group.GetWorld() != world || world.FindEntityByID(m_Group.GetID()) != m_Group))
			return false;
		return true;
	}
}

class AICF_AIQuotaCensus
{
	protected SCR_AIWorld m_World;
	protected BaseWorld m_MainWorld;
	protected EntityID m_WorldId;
	protected int m_iActive = -1;
	protected int m_iIntended = -1;
	protected bool m_bComplete;

	// A cached capture flag only; dispatch still requires fresh IsCurrent and
	// a ledger whose latest census has not been invalidated.
	bool HasCompleteCapture() { return m_bComplete; }
	protected string m_sReason = "NOT_CAPTURED";
	protected ref array<ref AICF_AIQuotaAgentRow> m_aRows = {};

	static bool IsComplete(int returned, int examined, int invalid, int countedActive,
		int beforeActive, int afterActive, int beforeIntended, int afterIntended)
	{
		return returned >= 0 && returned <= AICF_AIQuotaMath.MAX_TOTAL_UNITS && examined == returned && invalid == 0 &&
			beforeActive >= 0 && beforeActive <= AICF_AIQuotaMath.MAX_TOTAL_UNITS && beforeActive == afterActive &&
			countedActive == afterActive && beforeIntended >= 0 && beforeIntended <= AICF_AIQuotaMath.MAX_TOTAL_UNITS &&
			beforeIntended == afterIntended;
	}

	// Diagnostic seam: default is a no-op; receives the one actual predicate.
	protected void AICF_ObserveCaptureDecision(bool actual, int returned, int examined, int invalid,
		int countedActive, int beforeActive, int afterActive, int beforeIntended, int afterIntended)
	{
	}

	bool Capture(SCR_AIWorld world)
	{
		m_bComplete = false;
		m_sReason = "WORLD_IDENTITY";
		m_aRows.Clear();
		if (!Replication.IsServer() || !GetGame() || !world || GetGame().GetAIWorld() != world ||
			!GetGame().GetWorld() || world.GetWorld() != GetGame().GetWorld())
			return false;
		m_World = world;
		m_MainWorld = world.GetWorld();
		m_WorldId = world.GetID();
		int beforeActive = world.GetCurrentNumOfActiveAIs();
		int beforeIntended = world.GetActiveGroupIntendedCount();
		array<AIAgent> agents = {};
		world.GetAIAgents(agents);
		if (agents.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS)
		{
			m_sReason = "AGENT_BOUND";
			return false;
		}
		array<AIAgent> uniqueAgents = {};
		array<IEntity> uniqueEntities = {};
		int invalid;
		int examined;
		int countedActive;
		foreach (AIAgent agent : agents)
		{
			examined++;
			if (!agent || uniqueAgents.Contains(agent) || agent.GetWorld() != m_MainWorld)
			{
				invalid++;
				continue;
			}
			uniqueAgents.Insert(agent);
			AICF_AIQuotaAgentRow row = new AICF_AIQuotaAgentRow();
			row.m_Agent = agent;
			row.m_AgentId = agent.GetID();
			row.m_Entity = agent.GetControlledEntity();
			row.m_Group = agent.GetParentGroup();
			row.m_bCounted = agent.CountTowardsAIWorldLimit();
			row.m_bActivated = agent.IsAIActivated();
			if (row.m_Entity)
			{
				row.m_EntityId = row.m_Entity.GetID();
				row.m_bVehicle = Vehicle.Cast(row.m_Entity) != null;
				if (uniqueEntities.Contains(row.m_Entity)) invalid++;
				uniqueEntities.Insert(row.m_Entity);
			}
			else if (row.m_bCounted)
				invalid++;
			if (row.m_bCounted && row.m_bActivated) countedActive++;
			if (!row.IsCurrent(m_MainWorld)) invalid++;
			m_aRows.Insert(row);
		}
		int afterActive = world.GetCurrentNumOfActiveAIs();
		int afterIntended = world.GetActiveGroupIntendedCount();
		m_sReason = "NATIVE_CENSUS_MISMATCH";
		int returned = agents.Count();
		bool actual = IsComplete(returned, examined, invalid, countedActive, beforeActive, afterActive, beforeIntended, afterIntended);
		AICF_ObserveCaptureDecision(actual, returned, examined, invalid, countedActive, beforeActive, afterActive, beforeIntended, afterIntended);
		if (!actual)
			return false;
		m_iActive = afterActive;
		m_iIntended = afterIntended;
		m_bComplete = true;
		if (!IsCurrent())
		{
			m_bComplete = false;
			m_sReason = "REPEATED_CENSUS_CHANGED";
			return false;
		}
		m_sReason = "COMPLETE";
		return true;
	}

	bool IsCurrent()
	{
		if (!m_bComplete || !Replication.IsServer() || !GetGame() || !m_World || !m_MainWorld ||
			GetGame().GetAIWorld() != m_World || GetGame().GetWorld() != m_MainWorld ||
			m_World.GetWorld() != m_MainWorld || m_World.GetID() != m_WorldId ||
			m_World.GetCurrentNumOfActiveAIs() != m_iActive || m_World.GetActiveGroupIntendedCount() != m_iIntended)
			return false;
		array<AIAgent> repeated = {};
		m_World.GetAIAgents(repeated);
		if (repeated.Count() != m_aRows.Count() || repeated.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS)
			return false;
		array<AIAgent> seen = {};
		foreach (AICF_AIQuotaAgentRow row : m_aRows)
		{
			if (!row || !row.IsCurrent(m_MainWorld) || !repeated.Contains(row.m_Agent) || seen.Contains(row.m_Agent))
				return false;
			seen.Insert(row.m_Agent);
		}
		return m_World.GetCurrentNumOfActiveAIs() == m_iActive && m_World.GetActiveGroupIntendedCount() == m_iIntended;
	}

	int Count() { return m_aRows.Count(); }
	AICF_AIQuotaAgentRow GetRow(int index)
	{
		if (!m_bComplete || index < 0 || index >= m_aRows.Count()) return null;
		return m_aRows[index];
	}
	int GetNativeActive() { return m_iActive; }
	int GetNativeIntended() { return m_iIntended; }
	string GetReason() { return m_sReason; }
}
