// Наблюдение не меняет predicate победы. Отдельное окно на каждую runtime faction.
class AICF_VictoryDiagnostics
{
	protected ref map<string, string> m_Signatures = new map<string, string>();
	protected ref map<string, int> m_ReportedAt = new map<string, int>();

	void Report(AICF_ObjectiveGraph graph, FactionKey factionKey, bool rebuild, bool replan)
	{
		int revision;
		int objectives;
		string reason = "NONE";
		string status = "UNMET";
		SCR_CampaignMilitaryBaseComponent blocker;
		bool objective;
		if (graph)
		{
			revision = graph.GetRevision();
			for (int i; i < graph.GetNodeCount(); i++)
			{
				AICF_ObjectiveNode node = graph.GetNode(i);
				if (!node)
				{
					if (reason == "NONE")
						reason = "NODE_MISSING";
					continue;
				}
				if (!node.IsObjective())
					continue;
				SCR_CampaignMilitaryBaseComponent base = node.GetBase();
				bool isHQ = base && base.IsHQ();
				if (!isHQ)
					objectives++;
				string baseReason;
				if (!base || !base.GetOwner())
					baseReason = "BASE_MISSING";
				else if (!base.IsInitialized())
					baseReason = "BASE_UNINITIALIZED";
				else if (isHQ)
					continue;
				else if (!base.GetFaction() || base.GetFaction().GetFactionKey() != factionKey)
					baseReason = "OWNER_MISMATCH";
				else if (base.GetCaptureState() != SCR_EBaseCaptureState.NONE)
					baseReason = "CAPTURE_STATE";
				else if (base.IsBeingCaptured())
					baseReason = "CAPTURING";
				if (reason == "NONE" && !baseReason.IsEmpty())
				{
					reason = baseReason;
					blocker = base;
					objective = true;
				}
			}
		}
		bool ready = graph && revision > 0 && !rebuild && !replan;
		if (!ready)
		{
			status = "NOT_READY";
			reason = "GRAPH_NOT_READY";
		}
		else if (reason == "NODE_MISSING" || reason == "BASE_MISSING" || reason == "BASE_UNINITIALIZED")
			status = "NOT_READY";
		else if (reason == "NONE" && objectives == 0)
			reason = "NO_OBJECTIVES";
		else if (reason == "NONE")
			status = "MET";

		string owner = "NONE";
		bool initialized;
		bool hq;
		int captureState = -1;
		bool capturing;
		bool enemies;
		if (blocker)
		{
			initialized = blocker.IsInitialized();
			hq = blocker.IsHQ();
			if (blocker.GetFaction())
				owner = blocker.GetFaction().GetFactionKey();
			captureState = blocker.GetCaptureState();
			capturing = blocker.IsBeingCaptured();
			enemies = blocker.AreEnemiesPresent();
		}
		string details = string.Format("candidate=%1 revision=%2 graph_ready=%3 rebuild=%4 replan=%5 objectives=%6 status=%7 reason=%8 blocker=%9",
			factionKey, revision, ready, rebuild, replan, objectives, status, reason, AICF_Stage1Diagnostics.BaseKey(blocker));
		details += string.Format(" objective=%1 hq=%2 initialized=%3 owner=%4 capture_state=%5 capturing=%6 enemies_present=%7",
			objective, hq, initialized, owner, captureState, capturing, enemies);
		string previous;
		int reportedAt;
		m_Signatures.Find(factionKey, previous);
		m_ReportedAt.Find(factionKey, reportedAt);
		int now = System.GetTickCount();
		if (previous == details && now - reportedAt < 60000)
			return;
		m_Signatures.Set(factionKey, details);
		m_ReportedAt.Set(factionKey, now);
		AICF_Stage1Diagnostics.Info("VICTORY_CHECK", details);
	}
}
