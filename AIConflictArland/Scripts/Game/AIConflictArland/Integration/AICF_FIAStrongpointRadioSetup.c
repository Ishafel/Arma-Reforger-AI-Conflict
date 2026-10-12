// Настройка живого stock radio graph до запуска AICF. Ни имён, ни координат карт.
modded class SCR_CampaignMilitaryBaseComponent
{
	float AICF_GetDefaultRadioRange() { return m_fRadioRangeDefault; }

	bool AICF_SetGarrisonRadioRange(float range, float defaultRange)
	{
		if (!Replication.IsServer() || IsProxy() || !m_RadioComponent || range <= 0) return false;
		bool updated;
		for (int i; i < m_RadioComponent.TransceiversCount(); i++)
		{
			RelayTransceiver transceiver = RelayTransceiver.Cast(m_RadioComponent.GetTransceiver(i));
			if (!transceiver) continue;
			transceiver.SetRange(range);
			updated = true;
		}
		if (!updated) return false;
		m_fRadioRangeDefault = defaultRange;
		if (m_fRadioRange != range)
		{
			m_fRadioRange = range;
			Replication.BumpMe();
			OnRadioRangeChanged();
		}
		return true;
	}
}

class AICF_FIAStrongpointRadioSetup
{
	static bool Prepare(SCR_GameModeCampaign campaign)
	{
		if (!Replication.IsServer() || !campaign || !campaign.IsMaster()) return false;
		if (AICF_Difficulty.Get() != AICF_EDifficulty.HARD) return true;
		AICF_ConflictAdapter adapter = new AICF_ConflictAdapter();
		array<SCR_CampaignMilitaryBaseComponent> objectives = {};
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		AICF_ObjectiveGraph graph = new AICF_ObjectiveGraph();
		if (!adapter.CollectBases(campaign, objectives, bases) || !graph.Build(bases, objectives)) return false;
		array<int> headquarters = {};
		array<int> candidates = {};
		AICF_FIAStrongpointPolicy.Collect(graph, headquarters, candidates);
		if (AICF_FIAStrongpointPolicy.IsGraphReady(graph, headquarters, candidates)) return true;
		// Randomized Everon HQs могут начинать в разных компонентах. Сокращение
		// ranges их не соединит: сначала создаём минимальные физические bridges.
		array<float> originalRanges = {};
		array<float> originalDefaults = {};
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			originalRanges.Insert(base.GetRadioRange());
			originalDefaults.Insert(base.AICF_GetDefaultRadioRange());
		}
		if (!ConnectComponents(graph, bases, objectives))
		{
			Restore(bases, originalRanges, originalDefaults);
			return false;
		}
		AICF_FIAStrongpointPolicy.Collect(graph, headquarters, candidates);
		if (AICF_FIAStrongpointPolicy.IsGraphReady(graph, headquarters, candidates)) return true;
		string reason = "NO_ELIGIBLE_NODE";
		if (!candidates.IsEmpty()) reason = "GRAPH_NOT_READY";
		AICF_Stage1Diagnostics.Warning("FIA_STRONGPOINT_RADIO_SETUP", string.Format("reason=%1 action=TRY_BOUNDED_RANGE_REDUCTION", reason));
		if (headquarters.Count() != 2) { Restore(bases, originalRanges, originalDefaults); return false; }
		foreach (SCR_CampaignMilitaryBaseComponent candidate : objectives)
		{
			if (!candidate || !candidate.GetOwner() || candidate.IsHQ() || !candidate.GetFaction() || candidate.GetFaction().GetFactionKey() != "FIA") continue;
			array<SCR_CampaignMilitaryBaseComponent> changed = {candidate};
			array<float> ranges = {candidate.GetRadioRange()};
			array<float> defaults = {candidate.AICF_GetDefaultRadioRange()};
			array<float> proposed = {candidate.GetRadioRange()};
			foreach (int hqId : headquarters)
			{
				SCR_CampaignMilitaryBaseComponent hq = graph.GetNode(hqId).GetBase();
				float cap = Math.Floor(vector.DistanceXZ(candidate.GetOwner().GetOrigin(), hq.GetOwner().GetOrigin()) - 50);
				changed.Insert(hq);
				ranges.Insert(hq.GetRadioRange());
				defaults.Insert(hq.AICF_GetDefaultRadioRange());
				proposed.Insert(Math.Min(hq.GetRadioRange(), cap));
				proposed[0] = Math.Min(proposed[0], cap);
			}
			bool applied = true;
			for (int i; i < changed.Count(); i++)
			{
				if (!changed[i].AICF_SetGarrisonRadioRange(proposed[i], Math.Min(defaults[i], proposed[i]))) applied = false;
			}
			SCR_RadioCoverageSystem.UpdateAll(true);
			graph.Build(bases, objectives);
			AICF_FIAStrongpointPolicy.Collect(graph, headquarters, candidates);
			bool connected = applied && AICF_FIAStrongpointPolicy.IsGraphReady(graph, headquarters, candidates);
			if (connected)
			{
				for (int i; i < changed.Count(); i++)
				{
					AICF_Stage1Diagnostics.Info("FIA_STRONGPOINT_RADIO_CONFIGURED", string.Format("base=%1 old_range=%2 new_range=%3 candidates=%4", changed[i].GetOwner().GetID(), ranges[i], proposed[i], candidates.Count()));
				}
				return true;
			}
			// Не оставляем частично применённую конфигурацию при любом отказе.
			for (int i; i < changed.Count(); i++) changed[i].AICF_SetGarrisonRadioRange(ranges[i], defaults[i]);
			SCR_RadioCoverageSystem.UpdateAll(true);
			graph.Build(bases, objectives);
		}
		AICF_Stage1Diagnostics.Warning("FIA_STRONGPOINT_RADIO_REJECTED", "action=RECONFIGURE_HQ_OR_BASE_WHITELIST ranges_restored=1");
		Restore(bases, originalRanges, originalDefaults);
		return false;
	}

	protected static void Restore(array<SCR_CampaignMilitaryBaseComponent> bases, array<float> ranges, array<float> defaults)
	{
		for (int i; i < bases.Count(); i++) bases[i].AICF_SetGarrisonRadioRange(ranges[i], defaults[i]);
		SCR_RadioCoverageSystem.UpdateAll(true);
	}

	protected static bool ConnectComponents(AICF_ObjectiveGraph graph, array<SCR_CampaignMilitaryBaseComponent> bases, array<SCR_CampaignMilitaryBaseComponent> objectives)
	{
		// Не больше N-1 bridges. Каждый обязан увеличить компоненту node=0.
		for (int attempt; attempt < graph.GetNodeCount(); attempt++)
		{
			array<int> connected = {};
			for (int i; i < graph.GetNodeCount(); i++)
			{
				if (AICF_FIAStrongpointPolicy.Reachable(graph, 0, i)) connected.Insert(i);
			}
			if (connected.Count() == graph.GetNodeCount()) return true;
			int from = -1;
			int to = -1;
			float distance = float.MAX;
			foreach (int sourceId : connected)
			{
				SCR_CampaignMilitaryBaseComponent source = graph.GetNode(sourceId).GetBase();
				if (!source || !source.GetOwner()) continue;
				for (int targetId; targetId < graph.GetNodeCount(); targetId++)
				{
					if (connected.Contains(targetId)) continue;
					SCR_CampaignMilitaryBaseComponent target = graph.GetNode(targetId).GetBase();
					if (!target || !target.GetOwner() || (source.IsHQ() && target.IsHQ())) continue;
					float candidateDistance = vector.DistanceXZ(source.GetOwner().GetOrigin(), target.GetOwner().GetOrigin());
					if (candidateDistance >= distance) continue;
					distance = candidateDistance;
					from = sourceId;
					to = targetId;
				}
			}
			if (from < 0 || to < 0) return false;
			SCR_CampaignMilitaryBaseComponent a = graph.GetNode(from).GetBase();
			SCR_CampaignMilitaryBaseComponent b = graph.GetNode(to).GetBase();
			float rangeA = Math.Max(a.GetRadioRange(), Math.Ceil(distance + 50));
			float rangeB = Math.Max(b.GetRadioRange(), Math.Ceil(distance + 50));
			if (!a.AICF_SetGarrisonRadioRange(rangeA, Math.Max(a.AICF_GetDefaultRadioRange(), rangeA)) ||
				!b.AICF_SetGarrisonRadioRange(rangeB, Math.Max(b.AICF_GetDefaultRadioRange(), rangeB))) return false;
			SCR_RadioCoverageSystem.UpdateAll(true);
			graph.Build(bases, objectives);
			if (!AICF_FIAStrongpointPolicy.Reachable(graph, 0, to)) return false;
			AICF_Stage1Diagnostics.Info("FIA_STRONGPOINT_COMPONENT_BRIDGE", string.Format("from=%1 to=%2 distance=%3 range_a=%4 range_b=%5", from, to, distance, rangeA, rangeB));
		}
		return false;
	}
}
