// Только checkpoint завершённых geometry checks и незавершённого пути.
// Identity, экономика и reservations остаются у одного authoritative order.
class AICF_ConstructionCandidate
{
	float m_fRemainingDistance;
	protected vector m_aTransform[4];
	protected vector m_vMin;
	protected vector m_vMax;
	protected vector m_vStart;
	protected vector m_vSpawnOrigin;
	protected float m_fYaw;
	protected float m_fMinHeight;
	protected float m_fMaxHeight;
	protected int m_iPathQueries;
	protected int m_iPathStartOption;
	protected int m_iNavRetry;
	protected int m_iNavPathCursor;
	protected ref array<vector> m_aWorkCandidates = {};
	protected ref AICF_ConstructionPath m_Path;
	protected ref array<ref AICF_ConstructionVolume> m_aExits = {};

	void Save(AICF_ConstructionOrder order)
	{
		Math3D.MatrixCopy(order.m_aTransform, m_aTransform);
		m_vMin = order.m_vMin;
		m_vMax = order.m_vMax;
		m_vStart = order.m_vPathStart;
		m_vSpawnOrigin = order.m_vSpawnOrigin;
		m_iPathStartOption = order.m_iPathStartOption;
		m_iNavRetry = order.m_iNavRetry;
		m_iNavPathCursor = order.m_iNavPathCursor;
		foreach (vector work : order.m_aWorkCandidates)
			m_aWorkCandidates.Insert(work);
		m_fYaw = order.m_fYaw;
		m_fMinHeight = order.m_fMinHeight;
		m_fMaxHeight = order.m_fMaxHeight;
		m_iPathQueries = order.m_iQueries - order.m_iPathQueriesAt;
		m_Path = order.m_Path;
		m_fRemainingDistance = m_Path.RemainingDistance();
		foreach (AICF_ConstructionVolume exitVolume : order.m_aExits)
			m_aExits.Insert(exitVolume);
	}

	void Restore(AICF_ConstructionOrder order)
	{
		Math3D.MatrixCopy(m_aTransform, order.m_aTransform);
		order.m_vMin = m_vMin;
		order.m_vMax = m_vMax;
		order.m_vPathStart = m_vStart;
		order.m_vSpawnOrigin = m_vSpawnOrigin;
		order.m_iPathStartOption = m_iPathStartOption;
		order.m_iNavRetry = m_iNavRetry;
		order.m_iNavPathCursor = m_iNavPathCursor;
		order.m_aWorkCandidates.Clear();
		foreach (vector work : m_aWorkCandidates)
			order.m_aWorkCandidates.Insert(work);
		order.m_fYaw = m_fYaw;
		order.m_fMinHeight = m_fMinHeight;
		order.m_fMaxHeight = m_fMaxHeight;
		order.m_iPathQueriesAt = order.m_iQueries - m_iPathQueries;
		order.m_Path = m_Path;
		order.m_aExits.Clear();
		foreach (AICF_ConstructionVolume exitVolume : m_aExits)
			order.m_aExits.Insert(exitVolume);
		order.m_bCandidateLiveChecked = true;
		order.m_bPathStartSampled = true;
		order.m_bPathStartReady = true;
		order.m_iStage = 3;
		order.m_sReason = "PATH_CHECKPOINT_RESUMED";
		order.m_sObstacle = "NONE";
	}
}
