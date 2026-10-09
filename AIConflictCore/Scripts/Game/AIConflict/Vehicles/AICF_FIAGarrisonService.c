// Scheduler вызывается composition root: без собственного CallLater/event loop.
class AICF_FIAGarrisonService
{
	protected static ref array<ref AICF_FIAGarrison> s_aDefenders = {};
	protected ref AICF_FactionFleet m_Fleet;
	protected ref AICF_FIAGarrisonCrew m_Crew = new AICF_FIAGarrisonCrew();
	protected ref AICF_VehicleSpawner m_Spawner = new AICF_VehicleSpawner();
	protected ref AICF_ManagedAILODPolicy m_LOD = new AICF_ManagedAILODPolicy();
	protected ref AICF_FIAGarrisonPatrol m_Patrol = new AICF_FIAGarrisonPatrol();
	protected bool m_bStopped;
	protected int m_iNextSpawnMs;

	static bool IsDefender(IEntity entity)
	{
		if (!Replication.IsServer() || !entity) return false;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (g.OwnsMember(entity)) return true;
		}
		return false;
	}

	static bool IsLocalPatrolMove(IEntity entity, SCR_AIBehaviorBase behavior)
	{
		if (!Replication.IsServer() || !entity || !behavior) return false;
		SCR_AIMoveActivity activity = SCR_AIMoveActivity.Cast(behavior.GetGroupActivityContext());
		if (!activity) return false;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (g.OwnsMember(entity) && !g.IsDeployedPassenger(entity) && g.CanDrive() && g.PatrolWaypointIdentity() &&
				activity.m_RelatedWaypoint == g.m_PatrolWaypoint &&
				vector.DistanceXZ(g.m_vPatrolTarget, g.m_vHome) <= AICF_FIAGarrisonPatrol.ROUTE_RADIUS) return true;
		}
		return false;
	}

	static AICF_FIAGarrison FindPatrol(SCR_AIGroup group, AIWaypoint waypoint, IEntity vehicle)
	{
		if (!Replication.IsServer() || !group || !waypoint) return null;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (g.m_bRetired || !g.CanDrive() || !g.PatrolWaypointIdentity()) continue;
			if (g.m_Group == group && g.m_PatrolWaypoint == waypoint && group.GetCurrentWaypoint() == waypoint &&
				(!vehicle || vehicle == g.m_Vehicle)) return g;
		}
		return null;
	}

	static bool IsLocalPilotAvoidance(IEntity entity, vector position)
	{
		if (!Replication.IsServer() || !entity) return false;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (!g.OwnsMember(entity) || !g.CanDrive()) continue;
			if (vector.DistanceXZ(position, g.m_vHome) > AICF_FIAGarrisonPatrol.RETURN_RADIUS ||
				vector.DistanceXZ(position, g.m_Vehicle.GetOrigin()) > 25) return false;
			foreach (BaseCompartmentSlot seat : g.m_aSeats)
			{
				if (seat && seat.GetType() == ECompartmentType.PILOT && seat.GetOccupant() == entity) return true;
			}
		}
		return false;
	}

	static bool IsLocalBoarding(IEntity entity, SCR_AIGetInVehicle behavior)
	{
		if (!Replication.IsServer() || !entity || !behavior) return false;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (!g.OwnsMember(entity) || g.IsDeployedPassenger(entity) || !g.VehicleIdentity() || behavior.m_Vehicle.m_Value != g.m_Vehicle) continue;
			BaseCompartmentSlot seat = behavior.m_CompartmentToGetIn.m_Value;
			if (!seat || !g.m_aSeats.Contains(seat) || (seat.GetOccupant() && seat.GetOccupant() != entity)) return false;
			int index = g.m_aCrew.Find(ChimeraCharacter.Cast(entity));
			if (index < 0 || g.m_aSeats[index] != seat) return false;
			return vector.DistanceXZ(entity.GetOrigin(), g.m_Vehicle.GetOrigin()) <= 25 &&
				vector.DistanceXZ(g.m_Vehicle.GetOrigin(), g.m_vHome) <= AICF_FIAGarrisonPatrol.RETURN_RADIUS;
		}
		return false;
	}

	void Start(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		if (!Replication.IsServer() || !campaign || !graph || m_Fleet) return;
		AICF_EDifficulty difficulty = AICF_Difficulty.Get();
		int perBase;
		if (difficulty == AICF_EDifficulty.MEDIUM) perBase = 1;
		if (difficulty == AICF_EDifficulty.HARD) perBase = 2;
		SCR_CampaignFaction faction = campaign.GetFactionByEnum(SCR_ECampaignFaction.INDFOR);
		if (!faction || faction.GetFactionKey() != "FIA") return;
		m_Fleet = new AICF_FactionFleet("FIA");
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		for (int i; i < graph.GetNodeCount(); i++)
		{
			AICF_ObjectiveNode node = graph.GetNode(i);
			if (!node || !node.IsObjective()) continue;
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			if (!base || !base.GetOwner() || base.IsHQ() || base.GetFaction() != faction) continue;
			bases.Insert(base);
		}
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			for (int kind; kind < perBase; kind++)
			{
				AICF_FIAGarrison g = new AICF_FIAGarrison();
				g.m_iSlot = 10000 + s_aDefenders.Count();
				g.m_Base = base;
				g.m_BaseId = base.GetOwner().GetID();
				g.m_vHome = base.GetOwner().GetOrigin();
				g.m_Faction = faction;
				g.m_bTank = kind == 1 && AICF_Difficulty.UsesTank();
				g.m_bPassengers = !g.m_bTank && AICF_Difficulty.HasPassengers();
				g.m_sPrefab = AICF_ContentProfile.GetActive().GetFIAGarrisonPrefab(g.m_bTank);
				if (m_Fleet.TryReserveFIAGarrison(g, bases.Count() * perBase)) s_aDefenders.Insert(g);
			}
		}
		AICF_Stage3Diagnostics.Info("FIA_GARRISON_PLAN", string.Format("difficulty=%1 bases=%2 vehicles=%3 per_base=%4 replacement=NONE", difficulty, bases.Count(), s_aDefenders.Count(), perBase));
		ReserveCrewCapacity(campaign);
	}

	// Однократная квота initial plan: новые экипажи не вытесняют прежний roster.
	protected void ReserveCrewCapacity(SCR_GameModeCampaign campaign)
	{
		if (s_aDefenders.IsEmpty()) return;
		AIWorld world = GetGame().GetAIWorld();
		if (!world || !campaign.GetBaseManager()) return;
		int crew;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (g.m_bPassengers) crew += 10;
			else crew += 3;
		}
		array<SCR_CampaignMilitaryBaseComponent> allBases = {};
		campaign.GetBaseManager().GetBases(allBases);
		int previous = world.GetLimitOfActiveAIs();
		// BaseBuilderService позднее обеспечивает combat + bases; сохраняем этот запас.
		int required = previous + crew + allBases.Count();
		world.SetLimitOfActiveAIs(required);
		AICF_Stage3Diagnostics.Info("FIA_GARRISON_CAPACITY", string.Format("previous_limit=%1 crew=%2 builders=%3 required_limit=%4 effective_limit=%5", previous, crew, allBases.Count(), required, world.GetLimitOfActiveAIs()));
	}

	void Update()
	{
		if (!Replication.IsServer() || m_bStopped || !m_Fleet) return;
		int now = System.GetTickCount();
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (g.m_bRetired) continue;
			if (!g.m_Vehicle && g.m_iRequestedAtMs == 0)
			{
				if (now < g.m_iRetryAtMs || now < m_iNextSpawnMs) continue;
				m_iNextSpawnMs = now + 2000;
				Acquire(g, now);
				continue;
			}
			// Потеря машины не прекращает оборону оставшихся бойцов.
			if (!g.GroupIdentity()) { Retire(g, "GROUP_IDENTITY_LOST"); continue; }
			int agents, recovered;
			m_LOD.KeepCaptureEligible(g.m_Group, agents, recovered);
			if (g.DesantIdentity()) m_LOD.KeepCaptureEligible(g.m_DesantGroup, agents, recovered);
			if (g.m_bReady) { m_Crew.UpdateGarrison(g, now); m_Patrol.Update(g, now); continue; }
			if (!g.VehicleIdentity() || now - g.m_iRequestedAtMs >= 90000) { Retire(g, "INITIAL_CREW_FAILED"); continue; }
			if (!m_Crew.BoardGarrison(g)) continue;
			g.m_bReady = true;
			g.Log("FIA_GARRISON_READY", string.Format("crew=%1 seats=%2 position=%3 stationary=%4 replacement=0", g.m_aCrew.Count(), g.m_aSeats.Count(), g.m_vPosition, g.m_bStaticDefense));
		}
	}

	protected void Acquire(AICF_FIAGarrison g, int now)
	{
		g.m_iRetryAtMs = now + 30000;
		if (!g.BaseIdentity() || g.m_Base.GetFaction() != g.m_Faction) { Retire(g, "BASE_LOST_BEFORE_SPAWN"); return; }
		if (g.m_sPrefab.IsEmpty()) { Retire(g, "PREFAB_UNAVAILABLE"); return; }
		Resource resource = Resource.Load(g.m_sPrefab);
		if (!resource || !resource.IsValid() || resource.GetResource().GetResourceName() != g.m_sPrefab) { Retire(g, "PREFAB_UNAVAILABLE"); return; }
		vector crewPosition;
		if (!m_Spawner.SpawnFIAGarrison(g, m_Fleet, crewPosition))
		{
			if (g.m_Vehicle) Retire(g, "SPAWN_REJECTED_AFTER_ENTITY");
			else g.Log("FIA_GARRISON_WAIT", "reason=NO_CLEAR_SITE");
			return;
		}
		g.m_iRequestedAtMs = now;
		if (!m_Crew.DiscoverSeats(g)) { Retire(g, "SEATS_INVALID"); return; }
		g.m_Group = m_Crew.Create(g.m_Faction, crewPosition, g.m_aSeats.Count());
		if (!g.m_Group) { Retire(g, "CREW_CONTROLLER_FAILED"); return; }
		g.m_GroupId = g.m_Group.GetID();
		if (!m_Crew.BeginRosterSpawn(g.m_Group, g.m_aSeats.Count())) { Retire(g, "CREW_REQUEST_FAILED"); return; }
		g.Log("FIA_GARRISON_SPAWN_REQUESTED", string.Format("crew=%1 position=%2 prefab=%3", g.m_aSeats.Count(), g.m_vPosition, g.m_sPrefab));
	}

	protected void Retire(AICF_FIAGarrison g, string reason)
	{
		if (g.m_bRetired) return;
		m_Patrol.Stop(g);
		AICF_VehicleCleanupManager.RetainFIAGarrison(g);
		g.m_bRetired = true;
		g.Log("FIA_GARRISON_RETIRED", "reason=" + reason + " entities_retained=1 replacement=0");
	}

	void Stop()
	{
		if (!Replication.IsServer() || m_bStopped) return;
		m_bStopped = true;
		foreach (AICF_FIAGarrison g : s_aDefenders) Retire(g, "MATCH_STOP");
		s_aDefenders.Clear();
	}
}
