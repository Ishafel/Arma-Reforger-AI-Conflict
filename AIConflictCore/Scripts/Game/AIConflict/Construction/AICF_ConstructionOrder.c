enum AICF_EConstructionType
{
	SMALL_BARRACKS,
	ARMORY,
	LIGHT_DEPOT,
	LARGE_BARRACKS,
	HEAVY_DEPOT,
	COUNT
}

class AICF_ConstructionOrder
{
	string m_sToken;
	FactionKey m_sFaction;
	SCR_CampaignFaction m_Faction;
	SCR_CampaignMilitaryBaseComponent m_Base;
	EntityID m_BaseId;
	SCR_CampaignBuildingProviderComponent m_Provider;
	EntityID m_ProviderId;
	vector m_vProviderPosition;
	ref AICF_ConstructionMetadata m_Metadata;
	AICF_EConstructionType m_eType;
	int m_iRevision;
	int m_iGraphRevision;
	int m_iStartedAt;
	int m_iDeadline;
	int m_iResumeAt;
	int m_iSearchWindows = 1;
	int m_iPathSliceAt;
	int m_iCandidateStartedAt;
	int m_iCandidateIndex;
	int m_iPathResumes;
	int m_iNextPathAttempt = 128;
	bool m_bPathAdmitted;
	bool m_bTerrainLiveChecked;
	ref array<vector> m_aPathStarts = {};
	ref array<vector> m_aSpawnSeeds = {};
	AIPathfindingComponent m_PathContext;
	bool m_bPathContextSet;
	ref AICF_ConstructionNavigation m_Navigation = new AICF_ConstructionNavigation();
	int m_iNavCacheHits;
	int m_iPathSteps;
	ref array<ref AICF_ConstructionCandidate> m_aPendingCandidates = {};
	int m_iAttempts;
	int m_iSearchOffset;
	int m_iBroadphaseIgnored;
	int m_iExitOption;
	int m_iExitSample;
	float m_fExitMinHeight;
	float m_fExitMaxHeight;
	ref array<float> m_aExitHeights = {};
	ref array<ref AICF_ConstructionVolume> m_aExits = {};
	ref array<string> m_aRejectReasons = {};
	ref array<int> m_aRejectCounts = {};
	int m_iQueries;
	int m_iPathPruned;
	int m_iBudgetWaitWindows;
	int m_iLastBudgetWaitWindow = -1;
	int m_iQueryPhase;
	ref array<int> m_aPhaseQueries = {0, 0, 0, 0, 0, 0, 0};
	int m_iSearchCpuMs;
	int m_iMaxSliceMs;
	int m_iSample;
	bool m_bCandidateLiveChecked;
	ref array<float> m_aTerrainHeights = {};
	int m_iNavRetry;
	int m_iNavPathCursor;
	ref AICF_ConstructionPath m_Path;
	ref array<vector> m_aWorkCandidates = {};
	vector m_vPathStart;
	bool m_bPathStartSampled;
	bool m_bPathStartReady;
	vector m_vSpawnOrigin;
	int m_iPathStartOption; // Cursor общих spawn seeds заказа, не checkpoint кандидата.
	int m_iPathQueriesAt = -1;
	int m_iStage;
	vector m_aTransform[4];
	vector m_vMin;
	vector m_vMax;
	vector m_vWork;
	float m_fYaw;
	float m_fMinHeight;
	float m_fMaxHeight;
	bool m_bSiteReserved;
	bool m_bCommitStarted;
	bool m_bPaid;
	bool m_bAccepted;
	bool m_bCancelled;
	SCR_CampaignBuildingCompositionComponent m_Composition;
	EntityID m_LayoutId;
	SCR_ResourceConsumer m_Consumer;
	float m_fBefore;
	float m_fAfter;
	int m_iCost;
	int m_iReserve;
	int m_iPropsCost;
	int m_iPropsBefore;
	int m_iPropsAfter;
	int m_iLastBlockedAt;
	string m_sReason;
	string m_sObstacle;
	bool m_bTemporaryObstacle;

	string SearchStatus()
	{
		if (m_sReason == "SEARCH_AREA_EXHAUSTED")
			return "AREA_EXHAUSTED";
		if (m_sReason == "SEARCH_BUDGET_EXHAUSTED" || m_sReason == "CANDIDATE_COMPUTE_TIMEOUT" || m_sReason == "WORKER_PATH_QUERY_LIMIT")
			return "COMPUTE_LIMIT";
		if (m_bTemporaryObstacle && (m_sReason == "PHYSICAL_OBSTRUCTION" || m_sReason == "OBSTACLE_OR_RESERVED_SITE" || m_sReason == "DEPOT_EXIT_BLOCKED" || m_sReason == "WORKER_ENDPOINT_OBSTRUCTED"))
			return "TEMPORARY_OBSTACLE";
		if (m_bAccepted || m_sReason == "SITE_VALIDATED")
			return "SITE_FOUND";
		if (m_bCancelled)
			return "CANCELLED";
		return "COMPUTING";
	}

	void RejectCandidate()
	{
		if (m_sReason == "QUERY_BUDGET")
			return;
		int index = m_aRejectReasons.Find(m_sReason);
		if (index < 0)
		{
			index = m_aRejectReasons.Insert(m_sReason);
			m_aRejectCounts.Insert(0);
		}
		m_aRejectCounts[index] = m_aRejectCounts[index] + 1;
		if (m_aRejectCounts[index] == 1)
			Log("CONSTRUCTION_SITE_REJECTED", "obstacle=" + m_sObstacle);
	}

	// Каждый отказ уже выбранной площадки виден, даже при повторе категории.
	void RejectSelected(string phase)
	{
		Log("CONSTRUCTION_SELECTED_REJECTED", "phase=" + phase + " obstacle=" + m_sObstacle + " candidate_index=" + m_iCandidateIndex);
	}

	void LogSearch()
	{
		string counts;
		foreach (int index, string reason : m_aRejectReasons)
			counts += " rejected_" + reason + "=" + m_aRejectCounts[index];
		Log("CONSTRUCTION_SEARCH_SUMMARY", string.Format("search_offset=%1 exits=%2 terrain_delta=%3 broadphase_ignored=%4", m_iSearchOffset, m_aExits.Count(), m_fMaxHeight - m_fMinHeight, m_iBroadphaseIgnored) + counts);
		string cost = string.Format("candidate_queries=%1 terrain_queries=%2 exit_queries=%3 path_queries=%4 commit_queries=%5 completion_queries=%6 inventory_queries=%7",
			m_aPhaseQueries[0], m_aPhaseQueries[1], m_aPhaseQueries[2], m_aPhaseQueries[3], m_aPhaseQueries[4], m_aPhaseQueries[5], m_aPhaseQueries[6]);
		cost += string.Format(" budget_wait_windows=%1 path_pruned=%2 stage=%3 search_cpu_ms=%4 max_slice_ms=%5 checkpoints=%6 search_windows=%7", m_iBudgetWaitWindows, m_iPathPruned, m_iStage, m_iSearchCpuMs, m_iMaxSliceMs, m_aPendingCandidates.Count(), m_iSearchWindows);
		Log("CONSTRUCTION_SEARCH_COST", cost);
		Log("CONSTRUCTION_NAV_CACHE", "hits=" + m_iNavCacheHits);
	}

	static string EntityKey(EntityID id)
	{
		string key = id.ToString();
		key.Replace(" ", "");
		key.Replace("{}", "");
		return key;
	}

	bool IdentityValid()
	{
		if (m_bCancelled || !Replication.IsServer() || !m_Base || !m_Provider)
			return false;
		IEntity baseOwner = m_Base.GetOwner();
		IEntity providerOwner = m_Provider.GetOwner();
		// Вызов с notnull argument должен находиться после явного guard.
		// Stock service teardown может оставить component без owner entity.
		if (!baseOwner || !providerOwner)
			return false;
		return baseOwner.GetID() == m_BaseId && m_Base.IsInitialized() && m_Base.GetFaction() == m_Faction &&
			providerOwner.GetID() == m_ProviderId &&
			m_Base.GetMasterProvider() == m_Provider && m_Provider.GetCampaignMilitaryBaseComponent() == m_Base &&
			SCR_Faction.GetEntityFaction(providerOwner) == m_Faction &&
			vector.DistanceSq(providerOwner.GetOrigin(), m_vProviderPosition) < 0.01;
	}

	bool PlacementUnchanged()
	{
		if (!m_Composition || !m_Composition.GetOwner() || m_Composition.GetOwner().GetID() != m_LayoutId)
			return false;
		vector current[4];
		m_Composition.GetOwner().GetWorldTransform(current);
		for (int axis; axis < 4; axis++)
		{
			if (vector.DistanceSq(current[axis], m_aTransform[axis]) > 0.01)
				return false;
		}
		return true;
	}

	void Log(string eventName, string extra = "")
	{
		string fields = string.Format("token=%1 faction=%2 base=%3 provider=%4 type=%5 revision=%6 graph_revision=%7",
			m_sToken, m_sFaction, EntityKey(m_BaseId), EntityKey(m_ProviderId), typename.EnumToString(AICF_EConstructionType, m_eType), m_iRevision, m_iGraphRevision);
		if (m_Metadata)
			fields += " prefab=" + m_Metadata.m_sPrefab;
		fields += string.Format(" cost=%1 supplies_before=%2 supplies_after=%3 reserve=%4 candidates=%5 queries=%6 duration_ms=%7",
			m_iCost, m_fBefore, m_fAfter, m_iReserve, m_iAttempts, m_iQueries, System.GetTickCount() - m_iStartedAt);
		fields += string.Format(" props_cost=%1 props_before=%2 props_after=%3", m_iPropsCost, m_iPropsBefore, m_iPropsAfter);
		fields += string.Format(" position=%1 yaw=%2 layout=%3 reason=%4 %5", m_aTransform[3], m_fYaw, EntityKey(m_LayoutId), m_sReason, extra);
		fields += " search_status=" + SearchStatus();
		AICF_Stage1Diagnostics.Info(eventName, fields);
	}
}

class AICF_ConstructionBaseState
{
	SCR_CampaignMilitaryBaseComponent m_Base;
	EntityID m_BaseId;
	int m_iDueAt;
	int m_iRevision;
	int m_iNextType;
	int m_iSmallFailures;
	ref array<int> m_aSearchOffsets = {0, 0, 0, 0, 0};
	ref AICF_ConstructionOrder m_Order;
}
