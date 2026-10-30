// Original native capacity живёт столько же, сколько exact AIWorld, а не
// переиспользуемый MatchController. Этот класс не устанавливает native limit.
class AICF_AIQuotaWorldState
{
	protected SCR_AIWorld m_World;
	protected BaseWorld m_MainWorld;
	protected EntityID m_WorldId;
	protected int m_iOriginalNativeLimit = -1;
	protected bool m_bStockCaptured;
	protected int m_iInitialStockUnits = -1;
	protected int m_iInitialStockVehicleUnits = -1;
	protected AICF_MatchController m_Owner;
	protected int m_iActiveEpoch;
	protected int m_iLastEpoch;

	void AICF_AIQuotaWorldState(SCR_AIWorld world)
	{
		if (!Replication.IsServer() || !GetGame() || !world || GetGame().GetAIWorld() != world ||
			world.GetWorld() != GetGame().GetWorld())
			return;
		m_World = world;
		m_MainWorld = world.GetWorld();
		m_WorldId = world.GetID();
		int original = world.GetLimitOfActiveAIs();
		if (original > 0 && original <= AICF_AIQuotaMath.MAX_TOTAL_UNITS)
			m_iOriginalNativeLimit = original;
	}

	bool IsCurrentWorld()
	{
		return Replication.IsServer() && GetGame() && m_World && m_MainWorld &&
			GetGame().GetAIWorld() == m_World && GetGame().GetWorld() == m_MainWorld &&
			m_World.GetWorld() == m_MainWorld && m_World.GetID() == m_WorldId && m_iOriginalNativeLimit > 0;
	}

	int GetOriginalNativeLimit() { return m_iOriginalNativeLimit; }
	int GetActiveEpoch() { return m_iActiveEpoch; }

	bool IsSessionCurrent(AICF_MatchController owner, int epoch)
	{
		return IsCurrentWorld() && owner && owner == m_Owner && epoch > 0 && epoch == m_iActiveEpoch &&
			owner.IsAIQuotaOwnerCurrent(m_World);
	}

	bool BeginSession(AICF_MatchController owner, int initialStockUnits, int initialStockVehicleUnits,
		out AICF_AIQuotaLimits limits, out int epoch)
	{
		limits = null;
		epoch = 0;
		if (!IsCurrentWorld() || !owner || !owner.IsAIQuotaOwnerCurrent(m_World) ||
			initialStockUnits < 0 || initialStockUnits > AICF_AIQuotaMath.MAX_TOTAL_UNITS ||
			initialStockVehicleUnits < 0 || initialStockVehicleUnits > AICF_AIQuotaMath.MAX_TOTAL_UNITS ||
			(m_Owner == owner && m_iActiveEpoch > 0) || m_iLastEpoch == int.MAX)
			return false;
		int stockUnits = m_iInitialStockUnits;
		int vehicleUnits = m_iInitialStockVehicleUnits;
		if (!m_bStockCaptured)
		{
			stockUnits = initialStockUnits;
			vehicleUnits = initialStockVehicleUnits;
		}
		AICF_AIQuotaLimits candidate = new AICF_AIQuotaLimits();
		if (!candidate.CaptureStockSnapshot(m_iOriginalNativeLimit, stockUnits, vehicleUnits))
			return false;
		if (!IsCurrentWorld() || !owner.IsAIQuotaOwnerCurrent(m_World))
			return false;
		m_iInitialStockUnits = stockUnits;
		m_iInitialStockVehicleUnits = vehicleUnits;
		m_bStockCaptured = true;
		m_iLastEpoch++;
		m_iActiveEpoch = m_iLastEpoch;
		m_Owner = owner;
		limits = candidate;
		epoch = m_iActiveEpoch;
		return true;
	}

	void EndSession(AICF_MatchController owner, int epoch)
	{
		if (!owner || owner != m_Owner || epoch <= 0 || epoch != m_iActiveEpoch)
			return;
		m_Owner = null;
		m_iActiveEpoch = 0;
	}
}

modded class SCR_AIWorld
{
	protected ref AICF_AIQuotaWorldState m_AICFQuotaWorldState;

	AICF_AIQuotaWorldState AICF_GetQuotaWorldState()
	{
		if (!Replication.IsServer() || !GetGame() || GetGame().GetAIWorld() != this || GetWorld() != GetGame().GetWorld())
			return null;
		if (!m_AICFQuotaWorldState)
			m_AICFQuotaWorldState = new AICF_AIQuotaWorldState(this);
		return m_AICFQuotaWorldState;
	}

	AICF_AIQuotaWorldState AICF_FindQuotaWorldState()
	{
		if (!Replication.IsServer() || !GetGame() || GetGame().GetAIWorld() != this || GetWorld() != GetGame().GetWorld()) return null;
		return m_AICFQuotaWorldState;
	}
}
