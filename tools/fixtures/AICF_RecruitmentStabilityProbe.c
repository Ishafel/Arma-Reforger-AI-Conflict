// Isolated stage вместе с LifecycleProbe. Две настоящие базы/казармы/waypoint.
// Только income snapshot управляется fixture (0.1 supply/s), чтобы ±1 supply
// давал воспроизводимые 10 s изменения ETA. Production выбор/replan не подменяются.
modded class AICF_InfantryRecruitmentService
{
	bool m_bAICFStabilityInput;

	override protected AICF_RecruitmentSupplyForecast Forecast(SCR_CampaignMilitaryBaseComponent base,
		AICF_GroupSlot slot, int ownDemand, int members)
	{
		AICF_RecruitmentSupplyForecast forecast = super.Forecast(base, slot, ownDemand, members);
		if (m_bAICFStabilityInput)
		{
			forecast.m_fIncome = 0.1;
			forecast.m_fInterval = 1;
			forecast.m_fNextArrival = 1;
		}
		return forecast;
	}

	AICF_InfantryRecruitmentOrder AICF_StabilitySelect(AICF_GroupSlot slot, SCR_CampaignFaction faction, vector position)
	{
		return SelectBarracks(slot, faction, position, true);
	}
}

modded class AICF_MatchController
{
	protected int m_iStabilityStarted;
	protected int m_iStabilityVisitStarted;
	protected int m_iStabilityPhase;
	protected int m_iStabilityToken;
	protected int m_iStabilityStock;
	protected int m_iStabilityOwn;
	protected int m_iStabilityChallengerSamples;
	protected int m_iStabilityChallengeEvaluations;
	protected int m_iStabilitySamples;
	protected bool m_bStabilityFailed;
	protected SCR_CampaignMilitaryBaseComponent m_StabilitySecond;
	protected SCR_CampaignMilitaryBaseComponent m_StabilityInitial;
	protected SCR_CampaignMilitaryBaseComponent m_StabilityAlternative;
	protected SCR_CampaignBuildingCompositionComponent m_StabilityFirstBuilding;
	protected vector m_vStabilityPosition;
	protected vector m_vStabilityDirection;

	override protected void Update()
	{
		string enabled;
		if (System.GetCLIParam("aicfRecruitmentStabilityProbe", enabled) && enabled == "1" && m_bRosterReady && !m_bStopped)
			AICF_StabilityTick();
		super.Update();
	}

	protected void AICF_StabilityCheck(string name, bool pass)
	{
		if (!pass) m_bStabilityFailed = true;
		Print(string.Format("[AICF][RECRUITMENT_STABILITY_PROBE] case=%1 pass=%2", name, pass));
	}

	protected SCR_ServicePointComponent AICF_StabilityService(SCR_CampaignMilitaryBaseComponent base)
	{
		array<SCR_ServicePointComponent> services = {};
		base.GetServices(services);
		foreach (SCR_ServicePointComponent service : services)
		{
			if (service.GetType() == SCR_EServicePointType.BARRACKS && service.GetServiceState() == SCR_EServicePointStatus.ONLINE)
				return service;
		}
		return null;
	}

	protected void AICF_StabilityTick()
	{
		int now = System.GetTickCount();
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		SCR_CampaignMilitaryBaseComponent hq = m_USFaction.GetMainBase();
		if (!m_iStabilityStarted)
		{
			m_iStabilityStarted = now;
			m_Construction.Stop();
			for (int i; i < m_USState.GetSlotCount(); i++)
			{
				m_USState.GetSlot(i).SetDesiredSize(1);
				m_USSRState.GetSlot(i).SetDesiredSize(1);
			}
			float nearest = float.MAX;
			for (int n; n < m_ObjectiveGraph.GetNodeCount(); n++)
			{
				SCR_CampaignMilitaryBaseComponent candidate = m_ObjectiveGraph.GetNode(n).GetBase();
				if (!candidate || candidate.IsHQ() || !candidate.IsInitialized() || !candidate.GetMasterProvider() || !candidate.GetSpawnPoint()) continue;
				float distance = vector.DistanceSqXZ(hq.GetOwner().GetOrigin(), candidate.GetOwner().GetOrigin());
				if (distance >= nearest) continue;
				nearest = distance;
				m_StabilitySecond = candidate;
			}
			if (!m_StabilitySecond)
			{
				AICF_StabilityCheck("SECOND_BASE", false);
				AICF_StabilityClose();
				return;
			}
			m_StabilitySecond.SetFaction(m_USFaction);
			hq.AddSupplies(hq.GetSuppliesMax() - hq.GetSupplies());
			m_StabilitySecond.AddSupplies(m_StabilitySecond.GetSuppliesMax() - m_StabilitySecond.GetSupplies());
			AICF_LifecyclePlace(hq);
			m_StabilityFirstBuilding = m_LifecycleBuilding;
			AICF_LifecyclePlace(m_StabilitySecond);
			m_InfantryRecruitment.m_bAICFStabilityInput = true;
		}
		if (now - m_iStabilityStarted > 360000)
		{
			AICF_StabilityCheck("TIMEOUT", false);
			AICF_StabilityClose();
			return;
		}
		array<SCR_CampaignBuildingCompositionComponent> buildings = {m_StabilityFirstBuilding, m_LifecycleBuilding};
		foreach (SCR_CampaignBuildingCompositionComponent composition : buildings)
		{
			if (composition && !composition.IsCompositionSpawned() && composition.GetCompositionLayout())
				composition.GetCompositionLayout().AddBuildingValue(composition.GetCompositionLayout().GetToBuildValue());
		}
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (!leader) return;
		if (m_iStabilityPhase == 0)
		{
			SCR_ServicePointComponent first = AICF_StabilityService(hq);
			SCR_ServicePointComponent second = AICF_StabilityService(m_StabilitySecond);
			if (!first || !second) return;
			vector a = first.GetOwner().GetOrigin();
			vector b = second.GetOwner().GetOrigin();
			m_vStabilityPosition = (a + b) * 0.5;
			m_vStabilityPosition[1] = GetGame().GetWorld().GetSurfaceY(m_vStabilityPosition[0], m_vStabilityPosition[2]);
			leader.SetOrigin(m_vStabilityPosition);
			slot.SetDesiredSize(10);
			AICF_InfantryRecruitmentOrder candidate = m_InfantryRecruitment.AICF_StabilitySelect(slot, m_USFaction, m_vStabilityPosition);
			if (!candidate) return;
			m_iStabilityOwn = candidate.m_Forecast.m_iOwnDemand;
			float travel = Math.Max(0, vector.DistanceXZ(a, b) * 0.5 - 35) / 3;
			m_iStabilityStock = m_iStabilityOwn - Math.Ceil((travel + 65) * 0.1);
			if (m_iStabilityStock < 2)
			{
				AICF_StabilityCheck("INPUT_RANGE", false);
				AICF_StabilityClose();
				return;
			}
			hq.SetSupplies(m_iStabilityStock + 1);
			m_StabilitySecond.SetSupplies(m_iStabilityStock);
			string reason;
			m_InfantryRecruitment.CancelForSlot(slot);
			if (!m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason)) return;
			AICF_InfantryRecruitmentOrder order = m_InfantryRecruitment.AICF_LifecycleOrder(slot);
			m_StabilityInitial = order.m_Base;
			m_StabilityAlternative = m_StabilitySecond;
			if (m_StabilityInitial == m_StabilitySecond) m_StabilityAlternative = hq;
			m_vStabilityDirection = order.m_vPosition - m_vStabilityPosition;
			m_vStabilityDirection[1] = 0;
			m_vStabilityDirection.Normalize();
			m_iStabilityToken = order.m_iToken;
			m_iStabilityVisitStarted = now;
			m_iStabilityPhase = 1;
			AICF_StabilityCheck("TWO_REGISTERED_BASES", first != second && hq != m_StabilitySecond);
			return;
		}
		int elapsed = now - m_iStabilityVisitStarted;
		// Контролируемое физическое приближение предотвращает unrelated stuck recovery.
		vector position = m_vStabilityPosition + m_vStabilityDirection * (elapsed / 1000.0 * 0.3);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		leader.SetOrigin(position);
		AICF_InfantryRecruitmentOrder active = m_InfantryRecruitment.AICF_LifecycleOrder(slot);
		if (!active)
		{
			AICF_StabilityCheck("VISIT_ALIVE", false);
			AICF_StabilityClose();
			return;
		}
		if (elapsed < 125000)
		{
			int swing = 1;
			if (((elapsed + 10000) / 20000) % 2 == 0) swing = -1;
			m_StabilityInitial.SetSupplies(m_iStabilityStock);
			m_StabilityAlternative.SetSupplies(m_iStabilityStock + swing);
			AICF_InfantryRecruitmentOrder choice = m_InfantryRecruitment.AICF_StabilitySelect(slot, m_USFaction, position);
			if (!choice) return;
			bool deterministic = true;
			for (int repeat; repeat < 10; repeat++)
			{
				AICF_InfantryRecruitmentOrder same = m_InfantryRecruitment.AICF_StabilitySelect(slot, m_USFaction, position);
				if (!same || same.m_Base != choice.m_Base || same.m_Service != choice.m_Service || same.m_Forecast.m_fCompletionSeconds != choice.m_Forecast.m_fCompletionSeconds)
					deterministic = false;
			}
			if (!deterministic || active.m_iToken != m_iStabilityToken) m_bStabilityFailed = true;
			if (choice.m_Base != active.m_Base) m_iStabilityChallengerSamples++;
			bool challengeEvaluation = choice.m_Base != active.m_Base && now >= active.m_iNextEvaluationAtMs &&
				now - active.m_iLastSelectionAtMs >= AICF_InfantryRecruitmentConfig.REPLAN_COOLDOWN_MS;
			if (challengeEvaluation) m_iStabilityChallengeEvaluations++;
			m_iStabilitySamples++;
			Print(string.Format("[AICF][RECRUITMENT_STABILITY_SAMPLE] elapsed_ms=%1 swing=%2 deterministic=%3 token_stable=%4 challenger=%5 challenger_reevaluation=%6", elapsed, swing, deterministic, active.m_iToken == m_iStabilityToken, choice.m_Base != active.m_Base, challengeEvaluation));
		}
		else
		{
			m_StabilityInitial.SetSupplies(m_iStabilityStock);
			m_StabilityAlternative.SetSupplies(m_iStabilityOwn + 10);
			if (active.m_Base == m_StabilityAlternative)
			{
				AICF_StabilityCheck("DETERMINISTIC_SAMPLES", m_iStabilitySamples >= 20 && !m_bStabilityFailed);
				AICF_StabilityCheck("SMALL_GAIN_HELD", m_iStabilityChallengeEvaluations >= 2 && !m_bStabilityFailed);
				AICF_StabilityCheck("LARGE_GAIN_REPLAN", active.m_iToken != m_iStabilityToken && elapsed >= 125000);
				AICF_StabilityCheck("REPLAN_DEMAND_TRANSFER", m_InfantryRecruitment.AICF_LifecycleDemand(m_StabilityInitial) == 0 && m_InfantryRecruitment.AICF_LifecycleDemand(m_StabilityAlternative) == m_iStabilityOwn);
				AICF_StabilityClose();
			}
		}
	}

	protected void AICF_StabilityClose()
	{
		Print(string.Format("[AICF][RECRUITMENT_STABILITY_PROBE] finished=1 pass=%1 samples=%2 challenger_samples=%3", !m_bStabilityFailed, m_iStabilitySamples, m_iStabilityChallengerSamples));
		Stop(true);
		GetGame().RequestClose();
	}
}
