// Только stage: header inheritance, состав каждого гарнизона и локальное движение.
// Боевая эффективность, длительная устойчивость recovery и UI здесь не проверяются.
modded class AICF_FIAGarrisonService
{
	protected bool m_bAICFRolloutEnabled;
	protected bool m_bAICFRolloutFinished;
	protected int m_iAICFRolloutStart;
	protected int m_iAICFRolloutReady;
	protected int m_iAICFRolloutFailures;
	protected int m_iAICFRolloutChecks;

	protected void AICF_RolloutCheck(string name, bool passed)
	{
		m_iAICFRolloutChecks++;
		if (!passed) m_iAICFRolloutFailures++;
		Print(string.Format("[AICF][DIFFICULTY_PROBE] case=%1 passed=%2", name, passed));
	}

	override void Start(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		super.Start(campaign, graph);
		string expected;
		m_bAICFRolloutEnabled = System.GetCLIParam("aicfDifficultyProbe", expected);
		if (!m_bAICFRolloutEnabled) return;
		m_iAICFRolloutStart = System.GetTickCount();
		AICF_RolloutCheck("ACTIVE_DIFFICULTY", AICF_Difficulty.Get() == expected.ToInt());
		int bases;
		for (int i; i < graph.GetNodeCount(); i++)
		{
			AICF_ObjectiveNode node = graph.GetNode(i);
			if (!node || !node.IsObjective()) continue;
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			if (base && base.GetOwner() && !base.IsHQ() && base.GetFaction() && base.GetFaction().GetFactionKey() == "FIA") bases++;
		}
		AICF_RolloutCheck("PLAN_ALL_BASES", bases > 0 && s_aDefenders.Count() == bases * expected.ToInt());
		string headers;
		System.GetCLIParam("aicfDifficultyHeaders", headers);
		array<string> paths = {};
		headers.Split(";", paths, true);
		AICF_RolloutCheck("HEADERS_PROVIDED", paths.Count() >= 3);
		foreach (int index, string path : paths)
		{
			SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(MissionHeader.ReadMissionHeader(path));
			bool legacy = path.Contains("AICF_WCS_RHS_Conflict_Everon_North");
			AICF_RolloutCheck("HEADER_" + path, header && header.m_eAICFDifficulty == index % 3 && header.m_bAICFFIATank == legacy && header.m_bAICFFIAPassengers == legacy);
		}
		array<FactionKey> factionKeys = {"US", "USSR"};
		foreach (FactionKey faction : factionKeys)
		{
			int count;
			for (int member; member < 10; member++)
			{
				string role;
				array<string> suffixes = {};
				bool valid = AICF_ContentProfile.GetActive().BuildCharacterRoleCandidates(faction, member, role, suffixes);
				AICF_RolloutCheck("ROLE_" + faction + "_" + member, valid && !suffixes.IsEmpty());
				if (role == "ANTI_TANK") count++;
			}
			int expectedAT = 1;
			if (expected.ToInt() > 0) expectedAT = 3;
			AICF_RolloutCheck("AT_COUNT_" + faction, count == expectedAT);
		}
	}

	override void Update()
	{
		super.Update();
		if (!m_bAICFRolloutEnabled || m_bAICFRolloutFinished) return;
		int now = System.GetTickCount();
		bool allReady = true;
		int arrivals;
		int mobile;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (g.m_bRetired) { AICF_RolloutCheck("RETIRED_" + g.m_iSlot, false); AICF_RolloutFinish(); return; }
			if (!g.m_bReady) { allReady = false; continue; }
			if (!g.VehicleIdentity()) { AICF_RolloutCheck("VEHICLE_" + g.m_iSlot, false); AICF_RolloutFinish(); return; }
			DamageManagerComponent damage = DamageManagerComponent.Cast(g.m_Vehicle.FindComponent(DamageManagerComponent));
			if (damage) damage.EnableDamageHandling(false);
			foreach (ChimeraCharacter member : g.m_aCrew)
			{
				if (!member) continue;
				damage = DamageManagerComponent.Cast(member.FindComponent(DamageManagerComponent));
				if (damage) damage.EnableDamageHandling(false);
			}
			arrivals += g.m_iPatrolArrivals;
			if (!g.m_bStaticDefense) mobile++;
		}
		if (now - m_iAICFRolloutStart > 300000) { AICF_RolloutCheck("DEADLINE", false); AICF_RolloutFinish(); return; }
		if (!allReady) return;
		if (!m_iAICFRolloutReady) m_iAICFRolloutReady = now;
		if (!s_aDefenders.IsEmpty() && now - m_iAICFRolloutReady < 60000) return;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (g.m_bStaticDefense) AICF_RolloutCheck("STATIC_" + g.m_iSlot, !g.m_PatrolWaypoint && !g.m_bCrewRecoveryPending && vector.DistanceXZ(g.m_Vehicle.GetOrigin(), g.m_vPosition) < 10);
			int expectedCrew = 3;
			if (g.m_bPassengers) expectedCrew = 10;
			AICF_RolloutCheck("CREW_" + g.m_iSlot, g.m_aCrew.Count() == expectedCrew && g.m_aSeats.Count() == expectedCrew);
			AICF_RolloutCheck("ASSET_" + g.m_iSlot, g.m_bTank == (AICF_Difficulty.UsesTank() && AICF_Difficulty.Get() == AICF_EDifficulty.HARD && g.m_iSlot % 2 == 1));
			int essentials;
			foreach (bool essential : g.m_aEssentialSeats) { if (essential) essentials++; }
			AICF_RolloutCheck("ESSENTIALS_" + g.m_iSlot, essentials == 3);
			if (!g.m_bPassengers) AICF_RolloutCheck("NO_DESANT_" + g.m_iSlot, !g.m_bPassengersDeployed && !g.m_DesantGroup);
			g.Log("DIFFICULTY_PROBE_SAMPLE", string.Format("crew=%1 arrivals=%2 radius=%3", g.m_aCrew.Count(), g.m_iPatrolArrivals, vector.DistanceXZ(g.m_Vehicle.GetOrigin(), g.m_vHome)));
		}
		AICF_RolloutCheck("PATROL_ARRIVAL_SAMPLE", mobile == 0 || arrivals > 0);
		AICF_RolloutFinish();
	}

	protected void AICF_RolloutFinish()
	{
		m_bAICFRolloutFinished = true;
		Print(string.Format("[AICF][DIFFICULTY_PROBE_FINISHED] checks=%1 failures=%2", m_iAICFRolloutChecks, m_iAICFRolloutFailures));
		GetGame().RequestClose();
	}
}
