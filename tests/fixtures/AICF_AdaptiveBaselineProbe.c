// Isolated stage only. Records the unmodified production selector and live input.
modded class AICF_MatchController
{
	protected bool m_bAdaptiveBaselineDone;
	protected ref AICF_ObjectiveGraph m_AdaptiveBaselineGraph;
	protected ref AICF_AICommander m_AdaptiveBaselineCommander;
	protected int m_iAdaptiveBaselineAt;
	override protected void Update()
	{
		super.Update();
		string mode;
		if (!System.GetCLIParam("aicfAdaptiveBaseline", mode) || mode != "1" ||
			!m_bRosterReady || m_bStopped || m_bAdaptiveBaselineDone)
			return;
		if (m_AdaptiveBaselineGraph)
		{
			AICF_AdaptiveBaselineApply();
			return;
		}
		m_iAdaptiveBaselineAt = System.GetTickCount();
		Print(string.Format("[AICF][ADAPTIVE_BASELINE] graph_revision=%1 nodes=%2", m_ObjectiveGraph.GetRevision(), m_ObjectiveGraph.GetNodeCount()));
		for (int n; n < m_ObjectiveGraph.GetNodeCount(); n++)
		{
			AICF_ObjectiveNode node = m_ObjectiveGraph.GetNode(n);
			Print(string.Format("[AICF][ADAPTIVE_BASELINE_NODE] node=%1 objective=%2 owner=%3 position=%4 edges=%5 valid_us=%6",
				n, node.IsObjective(), node.GetBase().GetFaction().GetFactionKey(), node.GetBase().GetOwner().GetOrigin(),
				node.GetOutgoingNodeIds().Count(), node.GetBase().IsValidTarget(m_USFaction)));
		}
		for (int i; i < 3; i++)
		{
			AICF_GroupSlot slot = m_USState.GetSlot(i);
			string trigger;
			SCR_CampaignMilitaryBaseComponent selected = m_TargetSelector.SelectAttackTarget(m_ObjectiveGraph, m_USFaction, trigger, null, i, slot);
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			Print(string.Format("[AICF][ADAPTIVE_BASELINE_SLOT] faction=US slot=%1 generation=%2 alive=%3 ready=%4 position=%5 target=%6 trigger=%7",
				i, slot.GetSpawnGeneration(), AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()), slot.IsCombatReady(), leader.GetOrigin(),
				m_ObjectiveGraph.FindNodeId(selected), trigger));
		}
		AICF_AdaptiveProbeWorld.s_Faction = m_USFaction;
		for (int candidateId; candidateId < m_ObjectiveGraph.GetNodeCount(); candidateId++)
		{
			SCR_CampaignMilitaryBaseComponent base = m_ObjectiveGraph.GetNode(candidateId).GetBase();
			if (!base.IsHQ() && m_ObjectiveGraph.GetNode(candidateId).IsObjective() && base.GetFaction() != m_USFaction)
				AICF_AdaptiveProbeWorld.s_Targets.Insert(base);
			if (AICF_AdaptiveProbeWorld.s_Targets.Count() == 3)
				break;
		}
		AICF_ObjectiveGraph controlled = new AICF_ObjectiveGraph();
		controlled.AICF_ProbeGraph(m_USFaction, AICF_AdaptiveProbeWorld.s_Targets);
		m_AdaptiveBaselineGraph = controlled;
		m_AdaptiveBaselineCommander = new AICF_AICommander(m_USState, m_USFaction, m_CommandAuthorityPolicy, m_OrderPlanner, controlled, m_TargetSelector);
		for (int sample; sample < 2; sample++)
		{
			for (int preferred; preferred < 3; preferred++)
			{
				string modeSelected;
				SCR_CampaignMilitaryBaseComponent target = m_TargetSelector.SelectAttackTarget(controlled, m_USFaction, modeSelected, null, preferred, m_USState.GetSlot(preferred));
				Print(string.Format("[AICF][ADAPTIVE_CONTROLLED_BASELINE] snapshot=%1 faction=US slot=%2 target=%3 trigger=%4 nodes=4 edges=0>1,0>2,0>3",
					sample, preferred, controlled.FindNodeId(target), modeSelected));
			}
		}
		for (int actual; actual < 3; actual++)
		{
			m_OrderPlanner.ReleasePlayerCommand(m_USState.GetSlot(actual));
			m_USState.GetSlot(actual).SetDesiredSize(1);
			m_USState.GetSlot(actual).CompleteInfantryMusterIfReady();
		}
		AICF_AdaptiveBaselineApply();
	}
	protected void AICF_AdaptiveBaselineApply()
	{
		bool complete = true;
		for (int i; i < 3; i++)
		{
			AICF_GroupSlot slot = m_USState.GetSlot(i);
			if (!slot.GetWaypoint()) m_AdaptiveBaselineCommander.AssignOrder(slot, "CONTROLLED_BASELINE");
			array<AIWaypoint> waypoints = {};
			slot.GetGroup().GetWaypoints(waypoints);
			if (!slot.GetWaypoint() || !waypoints.Contains(slot.GetWaypoint())) complete = false;
		}
		if (!complete && System.GetTickCount(m_iAdaptiveBaselineAt) < 180000) return;
		for (int report; report < 3; report++)
		{
			AICF_GroupSlot reported = m_USState.GetSlot(report);
			int expected = 2;
			if (report == 0) expected = 1;
			int node = m_AdaptiveBaselineGraph.FindNodeId(reported.GetTargetBase());
			bool passed = complete && node == expected && reported.GetDecisionAuthority() == AICF_EStrategicDecisionAuthority.AI_COMMANDER;
			Print(string.Format("[AICF][ADAPTIVE_BASELINE_APPLIED] slot=%1 target=%2 passed=%3 generation=%4 waypoint=%5",
				report, node, passed, reported.GetSpawnGeneration(), AICF_GroupRuntime.FormatEntityId(reported.GetWaypoint())));
		}
		m_bAdaptiveBaselineDone = true;
		Print("[AICF][ADAPTIVE_BASELINE] finished=1");
		Stop(true);
		GetGame().RequestClose();
	}
}
