// Временная fixture отдельного stage; не включается в production addons.
modded class SCR_GameModeCampaign
{
	protected int m_iAICFNorthProbeAttempts;
	protected int m_iAICFNorthProbeFailures;
	protected int m_iAICFNorthProbeChecks;

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (System.GetCLIParam("aicfNorthProbe", enabled) && enabled == "1")
			GetGame().GetCallqueue().CallLater(AICF_RunNorthProbe, 15000, false);
	}

	protected void AICF_CheckNorth(string test, bool passed)
	{
		m_iAICFNorthProbeChecks++;
		if (!passed) m_iAICFNorthProbeFailures++;
		Print(string.Format("[AICF][NORTH_PROBE] case=%1 passed=%2", test, passed));
	}

	protected void AICF_RunNorthProbe()
	{
		if (!AICF_HasAICommanderState() && ++m_iAICFNorthProbeAttempts < 45)
		{
			GetGame().GetCallqueue().CallLater(AICF_RunNorthProbe, 2000, false);
			return;
		}
		array<string> expected = {"MilitaryBaseAirfield", "MilitaryHospital", "MainBaseNorth", "SmallBaseMaidensBay", "TownBaseMeaux", "TownBaseTyrone", "TownBaseKermovan", "StartingPos06", "StartingPos12"};
		array<SCR_MilitaryBaseComponent> rawBases = {};
		SCR_MilitaryBaseSystem.GetInstance().GetBases(rawBases);
		array<SCR_CampaignMilitaryBaseComponent> active = {};
		int hqCount;
		int captureCount;
		int excludedActive;
		foreach (SCR_MilitaryBaseComponent rawBase : rawBases)
		{
			SCR_CampaignMilitaryBaseComponent base = SCR_CampaignMilitaryBaseComponent.Cast(rawBase);
			if (!base || !base.GetOwner() || !base.IsInitialized()) continue;
			string name = base.GetOwner().GetName();
			if (!expected.Contains(name)) excludedActive++;
			active.Insert(base);
			if (base.IsHQ())
			{
				hqCount++;
				AICF_CheckNorth("HQ_LOCATION_" + name, name == "MilitaryBaseAirfield" || name == "MilitaryHospital");
			}
			else if (base.IsControlPoint()) captureCount++;
			if (name == "StartingPos06" || name == "StartingPos12")
			{
				AICF_CheckNorth("CAPTURE_LOCATION_" + name, base.IsControlPoint() && !base.CanBeHQ() && base.GetSpawnPoint());
				Print(string.Format("[AICF][NORTH_LOCATION] entity=%1 pos=%2", name, base.GetOwner().GetOrigin()));
			}
			string faction;
			if (base.GetFaction()) faction = base.GetFaction().GetFactionKey();
			Print(string.Format("[AICF][NORTH_BASE] entity=%1 hq=%2 capture=%3 faction=%4", name, base.IsHQ(), base.IsControlPoint(), faction));
		}
		AICF_CheckNorth("ACTIVE_NINE", active.Count() == 9);
		AICF_CheckNorth("HQ_TWO", hqCount == 2);
		AICF_CheckNorth("CAPTURE_SEVEN", captureCount == 7);
		AICF_CheckNorth("SOUTH_INACTIVE", excludedActive == 0);
		AICF_CheckNorth("COMMANDER_READY", AICF_HasAICommanderState());
		SCR_CampaignFaction west = GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		SCR_CampaignFaction east = GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
		AICF_CheckNorth("BOTH_HQ_ASSIGNED", west && east && west.GetMainBase() && east.GetMainBase() && west.GetMainBase() != east.GetMainBase() && active.Contains(west.GetMainBase()) && active.Contains(east.GetMainBase()));
		if (Replication.IsServer())
		{
			AICF_ObjectiveGraph graph = new AICF_ObjectiveGraph();
			AICF_CheckNorth("GRAPH_BUILD", graph.Build(active, active));
			int missingRoutes;
			foreach (SCR_CampaignMilitaryBaseComponent source : active)
			{
				foreach (SCR_CampaignMilitaryBaseComponent target : active)
				{
					if (graph.GetHopDistance(source, target) < 0) missingRoutes++;
				}
			}
			Print(string.Format("[AICF][NORTH_ROUTES] missing=%1 nodes=%2", missingRoutes, graph.GetNodeCount()));
			AICF_CheckNorth("RADIO_CONNECTED", missingRoutes == 0);
		}
		Print(string.Format("[AICF][NORTH_PROBE_FINISHED] server=%1 checks=%2 failures=%3", Replication.IsServer(), m_iAICFNorthProbeChecks, m_iAICFNorthProbeFailures));
		if (Replication.IsServer()) GetGame().GetCallqueue().CallLater(AICF_CloseNorthProbe, 150000, false);
		else AICF_CloseNorthProbe();
	}

	protected void AICF_CloseNorthProbe()
	{
		GetGame().RequestClose();
	}

	void ~SCR_GameModeCampaign()
	{
		if (!GetGame()) return;
		GetGame().GetCallqueue().Remove(AICF_RunNorthProbe);
		GetGame().GetCallqueue().Remove(AICF_CloseNorthProbe);
	}
}
