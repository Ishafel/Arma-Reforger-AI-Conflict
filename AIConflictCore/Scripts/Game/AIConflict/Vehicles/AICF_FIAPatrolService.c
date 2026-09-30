// Только orchestration FIA: один initial fleet, обход направленных radio edges.
class AICF_FIAPatrolService
{
	protected ref array<ref AICF_FIAPatrol> m_aPatrols = {};
	protected ref AICF_FactionFleet m_Fleet;
	protected ref AICF_VehicleSpawner m_Spawner = new AICF_VehicleSpawner();
	protected ref AICF_FIAPatrolCrew m_Crew = new AICF_FIAPatrolCrew();
	protected ref AICF_VehicleTaskHandoff m_Handoff = new AICF_VehicleTaskHandoff(null, null, null);
	protected ref AICF_ManagedAILODPolicy m_LOD = new AICF_ManagedAILODPolicy();
	protected AICF_ObjectiveGraph m_Graph;
	protected int m_iObjectives;
	protected bool m_bStopped;
	protected int m_iNextSpawnMs;

	AICF_FIAPatrol ReportFailedMovement(SCR_AIGroup group, AIWaypoint waypoint, IEntity vehicle = null)
	{
		if (!Replication.IsServer() || m_bStopped || !m_Graph || !group || !waypoint) return null;
		foreach (AICF_FIAPatrol p : m_aPatrols)
		{
			if (p.m_Group != group || p.m_Waypoint != waypoint || (vehicle && vehicle != p.m_Vehicle) ||
				p.m_iGraphRevision != m_Graph.GetRevision() || group.GetCurrentWaypoint() != waypoint) continue;
			AICF_FIAPatrolMoveFailure failure = new AICF_FIAPatrolMoveFailure(p);
			if (!failure.IsCurrent(p)) return null;
			p.m_MoveFailure = failure;
			p.Log("FIA_PATROL_MOVE_FAILED", string.Format("waypoint=%1 retries=%2 position=%3 endpoint=%4 seated=%5 next_action=BOUNDED_ROUTE_RECOVERY",
				p.m_WaypointId, p.m_iRouteRetries, p.m_Vehicle.GetOrigin(), p.m_vEndpoint, p.Seated()));
			return p;
		}
		return null;
	}

	protected void RecoverRoute(AICF_FIAPatrol p, int now, string reason)
	{
		p.m_MoveFailure = null;
		if (p.m_iRouteRetries >= 2)
		{
			Retire(p, "ROUTE_RECOVERY_EXHAUSTED");
			return;
		}
		p.m_iRouteRetries++;
		m_Handoff.ClearFIAPatrolWaypoint(p);
		p.m_iLegAtMs = now;
		p.Log("FIA_PATROL_ROUTE_RETRY", string.Format("attempt=%1 reason=%2 position=%3 endpoint=%4 seated=%5",
			p.m_iRouteRetries, reason, p.m_Vehicle.GetOrigin(), p.m_vEndpoint, p.Seated()));
		// Новый Move выдаётся следующим tick, после teardown старого activity.
	}

	void Start(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		if (!Replication.IsServer() || !campaign || !graph || m_Fleet) return;
		SCR_CampaignFaction faction = campaign.GetFactionByEnum(SCR_ECampaignFaction.INDFOR);
		if (!faction || faction.GetFactionKey() != "FIA") return;
		m_Graph = graph;
		for (int i; i < graph.GetNodeCount(); i++)
		{
			AICF_ObjectiveNode node = graph.GetNode(i);
			if (node && node.IsObjective() && node.GetBase() && !node.GetBase().IsHQ()) m_iObjectives++;
		}
		m_Fleet = new AICF_FactionFleet("FIA");
		int count = m_iObjectives / 2;
		for (int slot; slot < count; slot++)
		{
			AICF_FIAPatrol p = new AICF_FIAPatrol();
			p.m_iSlot = slot;
			p.m_Faction = faction;
			if (m_Fleet.TryReserveFIAPatrol(p, m_iObjectives)) m_aPatrols.Insert(p);
		}
		AICF_Stage3Diagnostics.Info("FIA_PATROL_PLAN", string.Format("faction=FIA objectives=%1 vehicles=%2 crew_per_vehicle=2 rounding=FLOOR replacement=NONE", m_iObjectives, m_aPatrols.Count()));
	}

	void Update(bool graphReady)
	{
		if (!Replication.IsServer() || m_bStopped || !m_Graph || !m_Fleet) return;
		int now = System.GetTickCount();
		foreach (AICF_FIAPatrol p : m_aPatrols)
		{
			if (p.m_bRetired) continue;
			if (!p.m_Vehicle)
			{
				if (p.m_iRequestedAtMs > 0) { Retire(p, "VEHICLE_LOST"); continue; }
				if (graphReady && now >= p.m_iRetryAtMs && now >= m_iNextSpawnMs)
				{
					m_iNextSpawnMs = now + 2000;
					Acquire(p, now);
				}
				continue;
			}
			if (!p.VehicleIdentity() || !p.GroupIdentity()) { Retire(p, "IDENTITY_LOST"); continue; }
			SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(p.m_Vehicle.FindComponent(SCR_DamageManagerComponent));
			if (damage && damage.IsDestroyed()) { Retire(p, "VEHICLE_DESTROYED"); continue; }
			int agents, recovered;
			m_LOD.KeepCaptureEligible(p.m_Group, agents, recovered);
			if (!p.m_bReady)
			{
				if (now - p.m_iRequestedAtMs >= 90000) { Retire(p, "CREW_TIMEOUT"); continue; }
				if (!m_Crew.Board(p)) continue;
				p.m_bReady = true;
				p.Log("FIA_PATROL_READY", "driver=1 gunner=1 agents=2 armed=1");
			}
			if (!p.CrewIdentity()) { Retire(p, "CREW_LOST"); continue; }
			if (!graphReady)
			{
				m_Handoff.ClearFIAPatrolWaypoint(p);
				continue;
			}
			// Бой/выход экипажа не вызывает принудительный teleport обратно.
			// Сохраняем native Move activity: она возвращает экипаж после боя.
			Route(p, now);
		}
	}

	protected void Acquire(AICF_FIAPatrol p, int now)
	{
		p.m_iRetryAtMs = now + 30000;
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		for (int i; i < m_Graph.GetNodeCount(); i++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(i);
			if (!node || !node.IsObjective() || !node.GetBase() || node.GetBase().IsHQ() ||
				node.GetBase().GetFaction() != p.m_Faction || node.GetOutgoingNodeIds().IsEmpty()) continue;
			bases.Insert(node.GetBase());
		}
		if (bases.IsEmpty())
		{
			p.Log("FIA_PATROL_WAIT", "reason=NO_FIA_RADIO_BASE");
			return;
		}
		int attempt = p.m_iSpawnAttempt++ + p.m_iSlot * 2;
		p.m_Current = bases[attempt % bases.Count()];
		vector crewPosition;
		if (!m_Spawner.SpawnFIAPatrol(p, m_Fleet, crewPosition))
		{
			if (p.m_Vehicle) Retire(p, "SPAWN_REJECTED_AFTER_ENTITY");
			else p.Log("FIA_PATROL_WAIT", "reason=NO_CLEAR_ROAD_SITE");
			return;
		}
		p.m_iRequestedAtMs = now;
		p.m_Group = m_Crew.Create(p.m_Faction, crewPosition);
		if (!p.m_Group) { Retire(p, "CREW_CONTROLLER_FAILED"); return; }
		p.m_GroupId = p.m_Group.GetID();
		if (!m_Crew.BeginRosterSpawn(p.m_Group, 2)) { Retire(p, "CREW_REQUEST_FAILED"); return; }
		p.Log("FIA_PATROL_SPAWN_REQUESTED", string.Format("agents=2 position=%1 base=%2", p.m_Vehicle.GetOrigin(), p.m_Current.GetOwner().GetID()));
	}

	protected void Route(AICF_FIAPatrol p, int now)
	{
		int sourceId = m_Graph.FindNodeId(p.m_Current);
		AICF_ObjectiveNode source = m_Graph.GetNode(sourceId);
		if (!source) { m_Handoff.ClearFIAPatrolWaypoint(p); return; }
		int revision = m_Graph.GetRevision();
		if (p.m_iGraphRevision != revision)
		{
			p.m_MoveFailure = null;
			m_Handoff.ClearFIAPatrolWaypoint(p);
			int targetId = m_Graph.FindNodeId(p.m_Target);
			if (!source.GetOutgoingNodeIds().Contains(targetId)) p.m_Target = null;
			p.m_iGraphRevision = revision;
		}
		if (p.m_Target && p.Seated() && vector.DistanceXZ(p.m_Vehicle.GetOrigin(), p.m_vEndpoint) <= 30)
		{
			p.Log("FIA_PATROL_ARRIVED", string.Format("from=%1 to=%2 displacement_m=%3 endpoint_distance_m=%4", sourceId,
				m_Graph.FindNodeId(p.m_Target), vector.DistanceXZ(p.m_vLegStart, p.m_Vehicle.GetOrigin()), vector.DistanceXZ(p.m_Vehicle.GetOrigin(), p.m_vEndpoint)));
			m_Handoff.ClearFIAPatrolWaypoint(p);
			p.m_Previous = p.m_Current;
			p.m_Current = p.m_Target;
			p.m_Target = null;
			int visits;
			p.m_mVisits.Find(p.m_Current, visits);
			p.m_mVisits.Set(p.m_Current, visits + 1);
			return;
		}
		if (!p.m_Target)
		{
			if (!SelectLeg(p, source)) return;
			p.m_iLeg++;
			p.m_iLegAtMs = now;
			p.m_vLegStart = p.m_Vehicle.GetOrigin();
			p.m_vProgress = p.m_vLegStart;
			p.m_fBestEndpointDistance = vector.DistanceXZ(p.m_vLegStart, p.m_vEndpoint);
			p.m_iRouteRetries = 0;
			p.Log("FIA_PATROL_LEG", string.Format("from=%1 to=%2 endpoint=%3 directed_edge=1", sourceId, m_Graph.FindNodeId(p.m_Target), p.m_vEndpoint));
		}
		array<AIWaypoint> queue = {};
		p.m_Group.GetWaypoints(queue);
		float endpointDistance = vector.DistanceXZ(p.m_Vehicle.GetOrigin(), p.m_vEndpoint);
		if (p.m_fBestEndpointDistance - endpointDistance >= 15)
		{
			p.m_vProgress = p.m_Vehicle.GetOrigin();
			p.m_iLegAtMs = now;
			p.m_fBestEndpointDistance = endpointDistance;
			p.m_iRouteRetries = 0;
		}
		if (p.m_MoveFailure)
		{
			if (p.m_MoveFailure.IsCurrent(p))
			{
				RecoverRoute(p, now, "UNKNOWN_MOVE");
				return;
			}
			p.m_MoveFailure = null;
		}
		if (now - p.m_iLegAtMs >= 120000 || (p.m_Waypoint && !queue.Contains(p.m_Waypoint)))
		{
			RecoverRoute(p, now, "NO_PHYSICAL_PROGRESS");
			return;
		}
		if (!p.m_Waypoint)
		{
			m_Handoff.MoveFIAPatrol(p, p.m_vEndpoint);
		}
	}

	protected bool SelectLeg(AICF_FIAPatrol p, AICF_ObjectiveNode source)
	{
		SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!ai || !ai.GetRoadNetworkManager()) return false;
		int bestScore = int.MAX;
		foreach (int nextId : source.GetOutgoingNodeIds())
		{
			AICF_ObjectiveNode next = m_Graph.GetNode(nextId);
			if (!next || !next.GetBase() || !next.GetBase().GetOwner() || next.GetBase().IsHQ()) continue;
			SCR_CampaignMilitaryBaseComponent target = next.GetBase();
			int score;
			p.m_mVisits.Find(target, score);
			score = score * 2;
			if (target == p.m_Previous) score++;
			if (score >= bestScore) continue;
			vector endpoint;
			vector targetPosition = target.GetOwner().GetOrigin();
			if (!ai.GetRoadNetworkManager().GetReachableWaypointInRoad(p.m_Vehicle.GetOrigin(), targetPosition, 100, endpoint)) continue;
			if (vector.DistanceXZ(endpoint, targetPosition) > 100 || vector.DistanceXZ(endpoint, p.m_Vehicle.GetOrigin()) < 50) continue;
			bestScore = score;
			p.m_Target = target;
			p.m_vEndpoint = endpoint;
		}
		return p.m_Target != null;
	}

	protected void Retire(AICF_FIAPatrol p, string reason)
	{
		if (p.m_bRetired) return;
		p.m_MoveFailure = null;
		m_Handoff.DetachFIAPatrol(p);
		AICF_VehicleCleanupManager.RetainFIAPatrol(p);
		p.m_bRetired = true;
		p.Log("FIA_PATROL_RETIRED", string.Format("reason=%1 entities_retained=1 replacement=0", reason));
	}

	void Stop()
	{
		if (!Replication.IsServer() || m_bStopped) return;
		m_bStopped = true;
		foreach (AICF_FIAPatrol p : m_aPatrols) Retire(p, "MATCH_STOP");
	}
}
