// Coordinates one immutable Stage 0 initialization pass. This is not an AI commander.
class AICF_Stage0Controller
{
	protected static const int VERIFY_INTERVAL_MS = 1000;
	protected static const int MAX_VERIFY_ATTEMPTS = 30;

	protected bool m_bStarted;
	protected bool m_bFinished;
	protected int m_iVerifyAttempts;

	protected ref AICF_ConflictAdapter m_ConflictAdapter;
	protected ref AICF_ObjectiveGraph m_ObjectiveGraph;
	protected ref AICF_TestTargetSelector m_TargetSelector;
	protected ref AICF_TestGroupSpawner m_GroupSpawner;
	protected ref AICF_WaypointFactory m_WaypointFactory;

	protected SCR_AIGroup m_USGroup;
	protected SCR_AIGroup m_USSRGroup;
	protected AIWaypoint m_USWaypoint;
	protected AIWaypoint m_USSRWaypoint;

	void Start(SCR_GameModeCampaign campaign)
	{
		if (m_bStarted)
		{
			AICF_Diagnostics.Warning("CONTROLLER_DUPLICATE_SKIPPED", "Stage 0 controller was already started");
			return;
		}

		m_bStarted = true;
		if (!GetGame().InPlayMode() || !Replication.IsServer() || !campaign || !campaign.IsMaster())
		{
			Fail("SERVER_AUTHORITY_REQUIRED", "Stage 0 requires an authoritative server-side Conflict game mode");
			return;
		}

		AICF_Diagnostics.Info("CONTROLLER_START", "Beginning the single Stage 0 initialization pass");
		m_ConflictAdapter = new AICF_ConflictAdapter();
		m_ObjectiveGraph = new AICF_ObjectiveGraph();
		m_TargetSelector = new AICF_TestTargetSelector();
		m_GroupSpawner = new AICF_TestGroupSpawner();
		m_WaypointFactory = new AICF_WaypointFactory();

		array<SCR_CampaignMilitaryBaseComponent> objectiveBases = {};
		array<SCR_CampaignMilitaryBaseComponent> graphBases = {};
		if (!m_ConflictAdapter.CollectBases(campaign, objectiveBases, graphBases))
		{
			Fail("BASE_DISCOVERY_FAILED", "Could not collect the stock Conflict bases");
			return;
		}

		if (!m_ObjectiveGraph.Build(graphBases, objectiveBases))
		{
			Fail("GRAPH_BUILD_FAILED", "Could not build the base graph");
			return;
		}

		SCR_CampaignFaction usFaction;
		SCR_CampaignFaction ussrFaction;
		if (!m_ConflictAdapter.ResolveFaction(campaign, SCR_ECampaignFaction.BLUFOR, "US", usFaction) ||
			!m_ConflictAdapter.ResolveFaction(campaign, SCR_ECampaignFaction.OPFOR, "USSR", ussrFaction))
		{
			Fail("FACTION_RESOLUTION_FAILED", "The stock US/USSR Conflict factions are unavailable");
			return;
		}

		SCR_CampaignMilitaryBaseComponent usTarget = m_TargetSelector.SelectTarget(m_ObjectiveGraph, usFaction);
		SCR_CampaignMilitaryBaseComponent ussrTarget = m_TargetSelector.SelectTarget(m_ObjectiveGraph, ussrFaction);
		if (!usTarget || !ussrTarget)
		{
			Fail("TARGET_SELECTION_FAILED", "Both factions must have a valid graph-reachable objective");
			return;
		}

		m_USGroup = m_GroupSpawner.SpawnTestGroup(usFaction);
		if (!m_USGroup)
		{
			Fail("US_GROUP_FAILED", "The US test group could not be created");
			return;
		}

		m_USWaypoint = m_WaypointFactory.CreateAndAssign(m_USGroup, usTarget);
		if (!m_USWaypoint)
		{
			Fail("US_WAYPOINT_FAILED", "The US test group did not receive a waypoint");
			return;
		}

		m_USSRGroup = m_GroupSpawner.SpawnTestGroup(ussrFaction);
		if (!m_USSRGroup)
		{
			Fail("USSR_GROUP_FAILED", "The USSR test group could not be created");
			return;
		}

		m_USSRWaypoint = m_WaypointFactory.CreateAndAssign(m_USSRGroup, ussrTarget);
		if (!m_USSRWaypoint)
		{
			Fail("USSR_WAYPOINT_FAILED", "The USSR test group did not receive a waypoint");
			return;
		}

		AICF_Diagnostics.Info("GROUP_VERIFY_WAIT", "Waiting up to 30 seconds for both groups to finish spawning members");
		GetGame().GetCallqueue().CallLater(VerifyStage0, VERIFY_INTERVAL_MS, false);
	}

	protected void VerifyStage0()
	{
		if (m_bFinished)
			return;

		m_iVerifyAttempts++;
		if (!m_USGroup || !m_USSRGroup || !m_USWaypoint || !m_USSRWaypoint)
		{
			Fail("ENTITY_LOST", "A test group or waypoint was deleted before verification completed");
			return;
		}

		bool usReady = m_USGroup.GetAgentsCount() > 0 && HasPriorityWaypoint(m_USGroup, m_USWaypoint);
		bool ussrReady = m_USSRGroup.GetAgentsCount() > 0 && HasPriorityWaypoint(m_USSRGroup, m_USSRWaypoint);
		if (usReady && ussrReady)
		{
			AICF_Diagnostics.Info(
				"GROUP_READY",
				string.Format("US_agents=%1 USSR_agents=%2", m_USGroup.GetAgentsCount(), m_USSRGroup.GetAgentsCount()));
			m_bFinished = true;
			AICF_Diagnostics.Result(true, "base graph logged; one US group and one USSR group have graph-reachable targets and waypoints");
			return;
		}

		if (m_iVerifyAttempts >= MAX_VERIFY_ATTEMPTS)
		{
			Fail(
				"GROUP_VERIFY_TIMEOUT",
				string.Format("US_agents=%1 US_waypoint=%2 USSR_agents=%3 USSR_waypoint=%4",
					m_USGroup.GetAgentsCount(),
					HasPriorityWaypoint(m_USGroup, m_USWaypoint),
					m_USSRGroup.GetAgentsCount(),
					HasPriorityWaypoint(m_USSRGroup, m_USSRWaypoint)));
			return;
		}

		GetGame().GetCallqueue().CallLater(VerifyStage0, VERIFY_INTERVAL_MS, false);
	}

	protected bool HasPriorityWaypoint(SCR_AIGroup group, AIWaypoint expectedWaypoint)
	{
		if (!group || !expectedWaypoint)
			return false;

		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		return !waypoints.IsEmpty() && waypoints[0] == expectedWaypoint;
	}

	protected void Fail(string eventName, string message)
	{
		if (m_bFinished)
			return;

		m_bFinished = true;
		AICF_Diagnostics.Error(eventName, message);
		CleanupSpawnedEntities();
		AICF_Diagnostics.Result(false, string.Format("%1: %2", eventName, message));
	}

	protected void CleanupSpawnedEntities()
	{
		bool hadSpawnedEntities = m_USWaypoint || m_USSRWaypoint || m_USGroup || m_USSRGroup;
		if (m_USWaypoint)
			RplComponent.DeleteRplEntity(m_USWaypoint, false);
		if (m_USSRWaypoint)
			RplComponent.DeleteRplEntity(m_USSRWaypoint, false);
		if (m_USGroup)
			RplComponent.DeleteRplEntity(m_USGroup, false);
		if (m_USSRGroup)
			RplComponent.DeleteRplEntity(m_USSRGroup, false);

		m_USWaypoint = null;
		m_USSRWaypoint = null;
		m_USGroup = null;
		m_USSRGroup = null;
		if (hadSpawnedEntities)
			AICF_Diagnostics.Info("FAILURE_CLEANUP", "Removed Stage 0 entities created before the failure");
	}
}
