// Только изолированный stage. Проверяет production plan, pure policy и initial boarding.
class AICF_FIAProbeGraph : AICF_ObjectiveGraph
{
	void Init(int count)
	{
		for (int i; i < count; i++) m_aNodes.Insert(new AICF_ObjectiveNode(i, null, true));
	}
	void Link(int from, int to) { m_aNodes[from].AddOutgoingNodeId(to); }
}

modded class AICF_FIAGarrisonService
{
	protected bool m_bRandomProbe;
	protected bool m_bRandomProbeDone;
	protected bool m_bRandomProbePlanOnly;
	protected int m_iRandomProbeStart;
	protected int m_iRandomProbeChecks;
	protected int m_iRandomProbeFailures;
	protected string m_sRandomProbePlan;

	protected void RandomCheck(string name, bool passed)
	{
		m_iRandomProbeChecks++;
		if (!passed) m_iRandomProbeFailures++;
		Print(string.Format("[AICF][FIA_RANDOM_PROBE] case=%1 passed=%2", name, passed));
	}

	protected void RandomPolicyChecks()
	{
		AICF_FIAProbeGraph graph = new AICF_FIAProbeGraph();
		graph.Init(7);
		graph.Link(0, 2); graph.Link(2, 3); graph.Link(3, 4); graph.Link(4, 1);
		graph.Link(3, 5); // тупик
		array<int> hqs = {0, 1};
		RandomCheck("INTERMEDIATE", AICF_FIAStrongpointPolicy.Eligible(graph, 3, hqs));
		RandomCheck("HQ", !AICF_FIAStrongpointPolicy.Eligible(graph, 0, hqs));
		RandomCheck("HQ_EDGE_INBOUND", !AICF_FIAStrongpointPolicy.Eligible(graph, 2, hqs));
		RandomCheck("HQ_EDGE_OUTBOUND", !AICF_FIAStrongpointPolicy.Eligible(graph, 4, hqs));
		RandomCheck("DEAD_END", !AICF_FIAStrongpointPolicy.Eligible(graph, 5, hqs));
		RandomCheck("ISOLATED", !AICF_FIAStrongpointPolicy.Eligible(graph, 6, hqs));
		array<int> eligible = {3};
		// Кандидат достигает обоих HQ, но 6 изолирован, а HQ 1 имеет только вход.
		graph.Link(1, 4);
		RandomCheck("GRAPH_DISCONNECTED_WITH_CANDIDATE", AICF_FIAStrongpointPolicy.Eligible(graph, 3, hqs) && !AICF_FIAStrongpointPolicy.IsGraphReady(graph, hqs, eligible));
		graph.Link(5, 6);
		RandomCheck("GRAPH_CONNECTED_WITH_HQ_EXITS", AICF_FIAStrongpointPolicy.IsGraphReady(graph, hqs, eligible));
		graph.GetNode(1).GetOutgoingNodeIds().Clear();
		RandomCheck("GRAPH_INBOUND_ONLY_HQ_WITH_CANDIDATE", AICF_FIAStrongpointPolicy.Eligible(graph, 3, hqs) && !AICF_FIAStrongpointPolicy.IsGraphReady(graph, hqs, eligible));
		graph.Link(1, 0);
		RandomCheck("GRAPH_HQ_TO_HQ_IS_NOT_EXIT", !AICF_FIAStrongpointPolicy.IsGraphReady(graph, hqs, eligible));
		graph.Link(1, 4);
		array<int> emptyCandidates = {};
		RandomCheck("GRAPH_NO_CANDIDATE", !AICF_FIAStrongpointPolicy.IsGraphReady(graph, hqs, emptyCandidates));
		RandomCheck("GRAPH_NULL", !AICF_FIAStrongpointPolicy.IsGraphReady(null, hqs, eligible));
		graph.Link(0, 3);
		RandomCheck("NO_ELIGIBLE_NODE", !AICF_FIAStrongpointPolicy.Eligible(graph, 3, hqs));
		array<int> invalidHQs = {0};
		RandomCheck("MISSING_HQ", !AICF_FIAStrongpointPolicy.Eligible(graph, 3, invalidHQs));
		int apcs, tanks, passengers;
		bool rules = true;
		for (int difficulty; difficulty < 3; difficulty++)
		{
			for (int strong; strong < 2; strong++)
			{
				for (int roll; roll < 6; roll++)
				{
					AICF_FIAGarrisonPlanner.Composition(difficulty, strong == 1, true, roll, apcs, tanks, passengers);
					if (difficulty == 0 && (apcs != 0 || tanks != 0 || passengers != 0)) rules = false;
					if (difficulty == 1 && (apcs != 1 || tanks != 0 || (passengers != 2 && passengers != 4))) rules = false;
					if (difficulty == 2 && strong == 0 && (apcs + tanks != 2 || tanks != roll % 3)) rules = false;
					if (difficulty == 2 && strong == 1 && (apcs != 2 || (tanks != 1 && tanks != 2))) rules = false;
					int total = apcs + tanks;
					AICF_FIAGarrisonPlanner.Composition(difficulty, strong == 1, false, roll, apcs, tanks, passengers);
					if (tanks != 0 || apcs != total) rules = false;
				}
			}
		}
		RandomCheck("ALL_DIFFICULTY_COMPOSITIONS_AND_FALLBACK", rules);
		AICF_FIAGarrisonRandom first = new AICF_FIAGarrisonRandom(12345);
		AICF_FIAGarrisonRandom second = new AICF_FIAGarrisonRandom(12345);
		array<int> seen = {};
		bool repeatable = true;
		for (int sample; sample < 300; sample++)
		{
			int value = first.Next(3);
			if (value != second.Next(3)) repeatable = false;
			if (!seen.Contains(value)) seen.Insert(value);
		}
		RandomCheck("PRNG_REPEATABLE_AND_ALL_COMBINATIONS", repeatable && seen.Count() == 3);
		AICF_FIAGarrisonRandom sequence = new AICF_FIAGarrisonRandom(1);
		RandomCheck("PRNG_REFERENCE", sequence.Next(20000) == 16807 && sequence.Next(1000000000) == 282475249);
	}

	override void Start(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		super.Start(campaign, graph);
		string enabled;
		m_bRandomProbe = System.GetCLIParam("aicfRandomGarrisonProbe", enabled);
		if (!m_bRandomProbe) return;
		m_bRandomProbePlanOnly = enabled == "plan";
		m_iRandomProbeStart = System.GetTickCount();
		RandomPolicyChecks();
		RandomCheck("PLAN_VALID", IsPlanValid());
		RandomCheck("SEED_RANGE", m_Planner && m_Planner.m_iSeed > 0 && m_Planner.m_iSeed < 1000000);
		if (!IsPlanValid()) { RandomFinish(); return; }
		int before = s_aDefenders.Count();
		super.Start(campaign, graph);
		RandomCheck("START_IDEMPOTENT", before == s_aDefenders.Count());
		array<int> headquarters = {};
		array<int> candidates = {};
		AICF_FIAStrongpointPolicy.Collect(graph, headquarters, candidates);
		int difficulty = AICF_Difficulty.Get();
		RandomCheck("STRONGPOINT_SCOPE", (difficulty == 2 && candidates.Contains(m_Planner.m_iStrongpoint)) || (difficulty != 2 && m_Planner.m_iStrongpoint == -1));
		if (difficulty == 2) RandomCheck("LIVE_GRAPH_READY", AICF_FIAStrongpointPolicy.IsGraphReady(graph, headquarters, candidates));
		for (int i; i < graph.GetNodeCount(); i++)
		{
			AICF_ObjectiveNode node = graph.GetNode(i);
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			if (!node.IsObjective() || !base || base.IsHQ() || !base.GetFaction() || base.GetFaction().GetFactionKey() != "FIA") continue;
			int count, apcs, tanks;
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				if (g.m_Base != base) continue;
				count++;
				if (g.m_bTank) tanks++; else apcs++;
				RandomCheck("ASSET_" + g.m_iSlot, g.m_iNode == i && g.m_bStrongpoint == (i == m_Planner.m_iStrongpoint) && (!g.m_bPassengers || !g.m_aPassengerPrefabs.IsEmpty()));
			}
			int expected = difficulty;
			if (i == m_Planner.m_iStrongpoint) RandomCheck("STRONG_COMPOSITION", count >= 3 && count <= 4 && apcs >= 2 && tanks <= 2);
			else RandomCheck("ORDINARY_" + i, count == expected);
			string snapshot = base.AICF_GetInitialGarrisonPlan();
			base.AICF_SetInitialGarrisonPlan("REJECT_OVERWRITE");
			RandomCheck("IMMUTABLE_SNAPSHOT_" + i, !snapshot.IsEmpty() && snapshot == base.AICF_GetInitialGarrisonPlan());
			m_sRandomProbePlan += snapshot;
		}
		// Не прерываем startup посреди MatchController.Start: road/bridge entities
		// ещё проходят native initialization. Первый Update приходит после ROSTER_READY.
	}

	override void Update()
	{
		if (m_bRandomProbe && m_bRandomProbePlanOnly)
		{
			if (!m_bRandomProbeDone) RandomFinish();
			return;
		}
		super.Update();
		if (!m_bRandomProbe || m_bRandomProbeDone) return;
		if (System.GetTickCount() - m_iRandomProbeStart > 360000) { RandomCheck("DEADLINE", false); RandomFinish(); return; }
		bool ready = true;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (g.m_bRetired) { RandomCheck("RETIRED_" + g.m_iSlot, false); RandomFinish(); return; }
			if (!g.m_bReady) { ready = false; continue; }
			DamageManagerComponent damage = DamageManagerComponent.Cast(g.m_Vehicle.FindComponent(DamageManagerComponent));
			if (damage) damage.EnableDamageHandling(false);
			foreach (ChimeraCharacter member : g.m_aCrew)
			{
				damage = DamageManagerComponent.Cast(member.FindComponent(DamageManagerComponent));
				if (damage) damage.EnableDamageHandling(false);
			}
		}
		if (!ready) return;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			int passengers;
			bool seated = g.VehicleIdentity() && g.GroupIdentity() && g.m_aSeats.Count() == g.m_aCrew.Count();
			for (int i; i < g.m_aSeats.Count(); i++)
			{
				if (!g.m_aEssentialSeats[i]) passengers++;
				if (!g.OwnsMember(g.m_aCrew[i]) || g.m_aSeats[i].GetOccupant() != g.m_aCrew[i]) seated = false;
			}
			RandomCheck("SEATED_" + g.m_iSlot, seated && (!g.m_bPassengers || (passengers > 0 && passengers <= g.m_aPassengerPrefabs.Count())));
			g.Log("FIA_RANDOM_PROBE_SAMPLE", string.Format("template=%1 passengers=%2 seated=%3", g.m_sPassengerTemplate, passengers, seated));
		}
		RandomFinish();
	}

	protected void RandomFinish()
	{
		if (m_bRandomProbeDone) return;
		m_bRandomProbeDone = true;
		Print(string.Format("[AICF][FIA_RANDOM_PROBE_FINISHED] checks=%1 failures=%2", m_iRandomProbeChecks, m_iRandomProbeFailures));
		GetGame().RequestClose();
	}
}
