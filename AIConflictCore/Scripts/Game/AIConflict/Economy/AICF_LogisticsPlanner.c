// Supply policy и directed HQ priority. Vehicle endpoints проходят отдельный
// road query; Euclidean score не является доказанной длиной дороги.
class AICF_LogisticsCandidate
{
	AICF_LogisticsEndpoint m_Source;
	AICF_LogisticsEndpoint m_Destination;
	float m_fScore;
	float m_fAmount;
	int m_iCategory;
}

class AICF_LogisticsSearch
{
	ref array<ref AICF_LogisticsCandidate> m_aCandidates = {};
	int m_iCursor;
	int m_iRevision;
	int m_iGeneration;
	int m_iStartedAtMs;
	bool m_bReturn;
	bool m_bPrepared;
	ref array<ref AICF_LogisticsEndpoint> m_aEndpoints = {};
	int m_iDestinationCursor;
	int m_iSourceCursor = -1;
	vector m_vOrigin;
	float m_fCapacity;
	float m_fCargo;
}

class AICF_LogisticsPlanner
{
	AICF_ObjectiveGraph m_Graph;
	AICF_LogisticsConfig m_Config;
	AICF_EconomySystem m_Economy;
	AICF_LogisticsLedger m_Book;
	ref array<ref AICF_LogisticsEndpoint> m_aEndpoints = {};
	protected int m_iQueries;
	protected int m_iCandidatesVisited;

	void AICF_LogisticsPlanner(AICF_ObjectiveGraph graph, AICF_LogisticsConfig config, AICF_EconomySystem economy, AICF_LogisticsLedger book)
	{
		m_Graph = graph;
		m_Config = config;
		m_Economy = economy;
		m_Book = book;
	}

	// Production test seams используются runtime policy fixtures.
	static bool DemandOpen(bool open, float actual, float capacity, float below, float target)
	{
		if (capacity <= 0 || actual >= capacity * target / 100) return false;
		return open || actual < capacity * below / 100;
	}

	static float Deficit(float actual, float capacity, float target, float incoming)
	{
		return Math.Max(0, Math.Min(capacity, capacity * target / 100) - actual - incoming);
	}

	static float Donatable(float actual, float capacity, float donate, float keep, float reserved, bool accepted)
	{
		if (capacity <= 0 || (!accepted && actual <= capacity * donate / 100)) return 0;
		return Math.Max(0, actual - capacity * keep / 100 - reserved);
	}

	void BeginTick() { m_iQueries = 0; m_iCandidatesVisited = 0; }

	void Reconcile(SCR_CampaignFaction us, SCR_CampaignFaction ussr)
	{
		array<ref AICF_LogisticsEndpoint> next = {};
		for (int i; i < m_Graph.GetNodeCount(); i++)
		{
			SCR_CampaignMilitaryBaseComponent base = m_Graph.GetNode(i).GetBase();
			if (!base || !base.IsInitialized() || !base.GetOwner() || !base.GetResourceComponent()) continue;
			AICF_LogisticsResourcePool pool = m_Book.m_Resources.Resolve(base.GetResourceComponent());
			if (!pool || pool.Capacity() <= 0) continue;
			AICF_LogisticsEndpoint endpoint = Find(base);
			if (!endpoint || !endpoint.IdentityValid() || endpoint.m_Pool != pool)
			{
				endpoint = new AICF_LogisticsEndpoint();
				endpoint.m_Base = base;
				endpoint.m_BaseId = base.GetOwner().GetID();
				endpoint.m_Owner = base.GetFaction();
				endpoint.m_Resource = base.GetResourceComponent();
				endpoint.m_Pool = pool;
			}
			endpoint.m_iDepth = -1;
			SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(base.GetFaction());
			if (faction == us || faction == ussr)
			{
				array<int> path = {};
				if (m_Graph.FindFriendlyPath(faction.GetMainBase(), base, faction.GetFactionKey(), path)) endpoint.m_iDepth = path.Count() - 1;
			}
			bool wasOpen = endpoint.m_bOpen;
			endpoint.m_bOpen = false;
			if (faction && (faction == us || faction == ussr) && (base == faction.GetMainBase() || base.GetType() != SCR_ECampaignBaseType.SOURCE_BASE))
				endpoint.m_bOpen = DemandOpen(wasOpen, pool.Value(), pool.Capacity(), m_Config.m_fRequestBelowPercent, m_Config.m_fTargetPercent);
			if (!wasOpen && endpoint.m_bOpen) endpoint.m_iOpenedAtMs = System.GetTickCount();
			next.Insert(endpoint);
		}
		m_aEndpoints = next;
	}

	AICF_LogisticsEndpoint Find(SCR_CampaignMilitaryBaseComponent base)
	{
		foreach (AICF_LogisticsEndpoint endpoint : m_aEndpoints)
		{
			if (endpoint.m_Base == base) return endpoint;
		}
		return null;
	}

	static bool OwnedSafe(AICF_LogisticsEndpoint e, SCR_CampaignFaction faction)
	{
		return e && e.IdentityValid() && e.m_Owner == faction && !e.m_Base.IsBeingCaptured() &&
			e.m_Base.GetCaptureState() == SCR_EBaseCaptureState.NONE && !e.m_Base.AreEnemiesPresent();
	}

	static bool NeutralSource(AICF_LogisticsEndpoint e, SCR_CampaignFaction faction)
	{
		if (!e || !e.IdentityValid() || e.m_Base.GetType() != SCR_ECampaignBaseType.SOURCE_BASE ||
			!e.m_Base.GetOwner().FindComponent(SCR_CampaignSourceBaseComponent) || e.m_Base.IsBeingCaptured() ||
			e.m_Base.GetCaptureState() != SCR_EBaseCaptureState.NONE) return false;
		Faction owner = e.m_Base.GetFaction();
		if (owner)
		{
			string stable = AICF_ContentProfile.GetActive().GetStableFactionKey(owner.GetFactionKey());
			return stable.IsEmpty() && !faction.IsFactionEnemy(owner) && !owner.IsFactionEnemy(faction);
		}
		// Unowned допускается только у штатного SOURCE component с unowned default.
		SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(e.m_Base.GetOwner().FindComponent(SCR_FactionAffiliationComponent));
		return affiliation && !affiliation.GetDefaultAffiliatedFaction();
	}

	float Available(AICF_LogisticsEndpoint e, SCR_CampaignFaction faction, AICF_LogisticsJob exclude = null)
	{
		if (!OwnedSafe(e, faction) && !NeutralSource(e, faction)) return 0;
		float reserved = m_Book.Reserved(e.m_Pool, false, exclude);
		float commitments = m_Economy.LogisticsObligations(e.m_Base);
		if (e.m_Base != faction.GetMainBase() && e.m_Base.GetType() == SCR_ECampaignBaseType.SOURCE_BASE)
		{
			float keep = m_Config.m_fNeutralSourceReserveSupplies;
			if (e.m_Owner == faction) keep = Math.Max(m_Config.m_fOwnedSourceReserveSupplies, m_Economy.LogisticsSourceReserve());
			return Math.Max(0, e.m_Pool.Value() - keep - commitments - reserved);
		}
		if (e.m_bOpen) return 0;
		return Donatable(e.m_Pool.Value(), e.m_Pool.Capacity(), m_Config.m_fDonateAbovePercent, m_Config.m_fDonorKeepPercent, reserved + commitments, exclude != null);
	}

	float Need(AICF_LogisticsEndpoint e, AICF_LogisticsJob exclude = null, bool returning = false)
	{
		if (!e || !e.IdentityValid()) return 0;
		float target = m_Config.m_fTargetPercent;
		if (returning) target = 100;
		return Deficit(e.m_Pool.Value(), e.m_Pool.Capacity(), target, m_Book.Reserved(e.m_Pool, true, exclude));
	}

	protected bool Before(AICF_LogisticsEndpoint a, AICF_LogisticsEndpoint b)
	{
		if (a.m_iDepth != b.m_iDepth) return a.m_iDepth < b.m_iDepth;
		float capacityA = a.m_Pool.Capacity();
		float capacityB = b.m_Pool.Capacity();
		if (capacityA <= 0) return false;
		if (capacityB <= 0) return true;
		float fractionA = a.m_Pool.Value() / capacityA;
		float fractionB = b.m_Pool.Value() / capacityB;
		if (fractionA != fractionB) return fractionA < fractionB;
		if (a.m_iOpenedAtMs != b.m_iOpenedAtMs) return a.m_iOpenedAtMs < b.m_iOpenedAtMs;
		return a.Key().Compare(b.Key()) < 0;
	}

	bool Route(vector from, AICF_LogisticsEndpoint e, out vector endpoint)
	{
		if (!e || !e.IdentityValid() || m_iQueries >= AICF_LogisticsConfig.SEARCH_BUDGET) return false;
		m_iQueries++;
		SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!world || !world.GetRoadNetworkManager() || e.m_Base.GetRadius() <= 5) return false;
		float radius = e.m_Base.GetRadius() - 5;
		return world.GetRoadNetworkManager().GetReachableWaypointInRoad(from, e.m_Base.GetOwner().GetOrigin(), radius, endpoint) &&
			vector.DistanceXZ(endpoint, e.m_Base.GetOwner().GetOrigin()) <= radius;
	}

	protected bool CandidateBefore(AICF_LogisticsCandidate a, AICF_LogisticsCandidate b, bool returning)
	{
		if (returning && a.m_iCategory != b.m_iCategory) return a.m_iCategory < b.m_iCategory;
		if (!returning && a.m_Destination != b.m_Destination) return Before(a.m_Destination, b.m_Destination);
		if (a.m_fScore != b.m_fScore) return a.m_fScore < b.m_fScore;
		if (a.m_fAmount != b.m_fAmount) return a.m_fAmount > b.m_fAmount;
		if (a.m_Source && b.m_Source) return a.m_Source.Key().Compare(b.m_Source.Key()) < 0;
		return a.m_Destination.Key().Compare(b.m_Destination.Key()) < 0;
	}

	protected void InsertCandidate(AICF_LogisticsSearch search, AICF_LogisticsCandidate candidate)
	{
		int index;
		while (index < search.m_aCandidates.Count() && !CandidateBefore(candidate, search.m_aCandidates[index], search.m_bReturn)) index++;
		search.m_aCandidates.InsertAt(candidate, index);
	}

	protected void PrepareSearch(AICF_LogisticsWorker w, bool preflight, bool returning)
	{
		w.m_sPlanningReason = "NO_OPEN_DEMAND";
		AICF_LogisticsSearch search = new AICF_LogisticsSearch();
		search.m_bReturn = returning;
		search.m_iRevision = m_Graph.GetRevision();
		search.m_iGeneration = w.m_iGeneration;
		search.m_iStartedAtMs = System.GetTickCount();
		w.m_Search = search;
		foreach (AICF_LogisticsEndpoint snapshot : m_aEndpoints) search.m_aEndpoints.Insert(snapshot);
		search.m_vOrigin = w.m_Depot.GetOrigin();
		search.m_fCapacity = w.m_fPrefabCapacity;
		if (!preflight)
		{
			search.m_vOrigin = w.m_Vehicle.GetOrigin();
			search.m_fCapacity = w.m_CargoPool.Capacity();
			search.m_fCargo = w.m_CargoPool.Value();
		}
	}

	// Даже подготовка пар source/destination имеет общий бюджет на tick.
	// Snapshot удерживает порядок endpoints при очередной reconciliation.
	protected bool PrepareCandidates(AICF_LogisticsWorker w)
	{
		AICF_LogisticsSearch search = w.m_Search;
		AICF_LogisticsEndpoint original;
		if (!w.m_aCargo.IsEmpty()) original = w.m_aCargo[0].m_Source;
		while (search.m_iDestinationCursor < search.m_aEndpoints.Count())
		{
			if (m_iCandidatesVisited >= AICF_LogisticsConfig.SEARCH_BUDGET) return false;
			AICF_LogisticsEndpoint destination = search.m_aEndpoints[search.m_iDestinationCursor];
			float need = Need(destination, null, search.m_bReturn);
			if (search.m_iSourceCursor < 0)
			{
				m_iCandidatesVisited++;
				bool own = OwnedSafe(destination, w.m_Faction);
				bool same = original && original.m_Pool == destination.m_Pool;
				bool allowed = own || (same && NeutralSource(destination, w.m_Faction));
				if (!search.m_bReturn)
				{
					allowed = destination.m_bOpen && destination.m_Owner == w.m_Faction && own && destination.m_iDepth >= 0;
					if (destination.m_bOpen && destination.m_Owner == w.m_Faction)
					{
						w.m_sPlanningReason = "NO_SOURCE_SURPLUS_OR_MINIMUM_BATCH";
						if (!own) w.m_sPlanningReason = "DESTINATION_UNSAFE";
						else if (destination.m_iDepth < 0) w.m_sPlanningReason = "NO_HQ_GRAPH_PATH";
					}
				}
				if (!allowed || need <= 0)
				{
					search.m_iDestinationCursor++;
					continue;
				}
				if (search.m_fCargo > 0)
				{
					AICF_LogisticsCandidate loaded = new AICF_LogisticsCandidate();
					loaded.m_Destination = destination;
					loaded.m_fAmount = Math.Min(search.m_fCargo, need);
					loaded.m_fScore = vector.DistanceXZ(search.m_vOrigin, destination.m_Base.GetOwner().GetOrigin());
					loaded.m_iCategory = 3;
					if (same) loaded.m_iCategory = 0;
					else if (destination.m_Base.GetType() == SCR_ECampaignBaseType.SOURCE_BASE) loaded.m_iCategory = 1;
					else if (destination.m_Base == w.m_Faction.GetMainBase()) loaded.m_iCategory = 2;
					if (!own) loaded.m_fAmount = Math.Min(loaded.m_fAmount, w.m_aCargo[0].m_fAmount);
					InsertCandidate(search, loaded);
					search.m_iDestinationCursor++;
					continue;
				}
				search.m_iSourceCursor = 0;
			}
			while (search.m_iSourceCursor < search.m_aEndpoints.Count())
			{
				if (m_iCandidatesVisited >= AICF_LogisticsConfig.SEARCH_BUDGET) return false;
				m_iCandidatesVisited++;
				AICF_LogisticsEndpoint source = search.m_aEndpoints[search.m_iSourceCursor++];
				if (!source.IdentityValid() || !destination.IdentityValid()) continue;
				if (source.m_Pool.Overlaps(destination.m_Pool)) continue;
				float q = Math.Min(search.m_fCapacity, Math.Min(Available(source, w.m_Faction), need));
				if (m_Config.m_fMaxCargoPerTrip > 0) q = Math.Min(q, m_Config.m_fMaxCargoPerTrip);
				if (q <= 0 || q < m_Config.m_fMinDispatchSupplies) continue;
				AICF_LogisticsCandidate candidate = new AICF_LogisticsCandidate();
				candidate.m_Source = source;
				candidate.m_Destination = destination;
				candidate.m_fAmount = q;
				candidate.m_fScore = vector.DistanceXZ(search.m_vOrigin, source.m_Base.GetOwner().GetOrigin()) + vector.DistanceXZ(source.m_Base.GetOwner().GetOrigin(), destination.m_Base.GetOwner().GetOrigin());
				// Первый донор — текущая база worker, включая промежуточную
				// после предыдущего рейса; home не имеет отдельной привилегии.
				if (vector.DistanceXZ(search.m_vOrigin, source.m_Base.GetOwner().GetOrigin()) < source.m_Base.GetRadius()) candidate.m_fScore = -1;
				InsertCandidate(search, candidate);
			}
			search.m_iSourceCursor = -1;
			search.m_iDestinationCursor++;
		}
		search.m_bPrepared = true;
		search.m_iStartedAtMs = System.GetTickCount();
		return true;
	}

	bool Select(AICF_LogisticsWorker w, bool preflight = false, bool returning = false)
	{
		if (!w || (!preflight && (!w.Ready() || w.m_Job))) return false;
		if (w.m_Search && (w.m_Search.m_iRevision != m_Graph.GetRevision() || w.m_Search.m_iGeneration != w.m_iGeneration ||
			w.m_Search.m_bReturn != returning || (w.m_Search.m_bPrepared && System.GetTickCount() - w.m_Search.m_iStartedAtMs > m_Config.m_iBlockedRetryMs))) w.m_Search = null;
		if (!w.m_Search) PrepareSearch(w, preflight, returning);
		AICF_LogisticsSearch search = w.m_Search;
		if (!search.m_bPrepared && !PrepareCandidates(w)) return false;
		vector origin = w.m_Depot.GetOrigin();
		if (!preflight) origin = w.m_Vehicle.GetOrigin();
		AICF_LogisticsEndpointSafety safety = new AICF_LogisticsEndpointSafety();
		while (search.m_iCursor < search.m_aCandidates.Count())
		{
			// Оставшийся cursor продолжается на следующем tick без return failure.
			if (m_iQueries + 2 > AICF_LogisticsConfig.SEARCH_BUDGET || m_iCandidatesVisited >= AICF_LogisticsConfig.SEARCH_BUDGET) return false;
			m_iCandidatesVisited++;
			AICF_LogisticsCandidate candidate = search.m_aCandidates[search.m_iCursor++];
			AICF_LogisticsEndpoint destination = candidate.m_Destination;
			AICF_LogisticsEndpoint source = candidate.m_Source;
			if (!destination.IdentityValid() || !safety.Safe(destination, w.m_Faction)) { w.m_sPlanningReason = "DESTINATION_UNSAFE"; continue; }
			if (returning)
			{
				AICF_LogisticsEndpoint original;
				if (!w.m_aCargo.IsEmpty()) original = w.m_aCargo[0].m_Source;
				if (!OwnedSafe(destination, w.m_Faction) && !(original && original.m_Pool == destination.m_Pool && NeutralSource(destination, w.m_Faction))) continue;
			}
			else if (!OwnedSafe(destination, w.m_Faction) || !destination.m_bOpen || destination.m_iDepth < 0) continue;
			float amount = Math.Min(candidate.m_fAmount, Need(destination, null, returning));
			vector load, unload;
			if (source)
			{
				amount = Math.Min(amount, Available(source, w.m_Faction));
				if (amount <= 0 || amount < m_Config.m_fMinDispatchSupplies) { w.m_sPlanningReason = "SOURCE_DEPLETED_OR_RESERVED"; continue; }
				if (!safety.Safe(source, w.m_Faction)) { w.m_sPlanningReason = "SOURCE_UNSAFE"; continue; }
				if (!Route(origin, source, load) || !Route(load, destination, unload)) { w.m_sPlanningReason = "ROAD_ROUTE_NOT_PROVEN"; continue; }
			}
			else
			{
				if (!preflight) amount = Math.Min(amount, w.m_CargoPool.Value());
				if (amount <= 0 || !Route(origin, destination, unload)) continue;
			}
			w.m_Search = null;
			if (preflight) return true;
			if (source) source.m_vPosition = load;
			destination.m_vPosition = unload;
			return m_Book.Reserve(w, source, destination, amount, returning, m_Config, m_Graph.GetRevision());
		}
		w.m_Search = null;
		return false;
	}

	bool SelectReturn(AICF_LogisticsWorker w)
	{
		if (!w.Ready() || !w.m_bCustody || w.m_fObservedCargo <= 0 || w.m_Job) return false;
		return Select(w, false, true);
	}
}
