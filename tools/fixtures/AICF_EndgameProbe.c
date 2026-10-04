// Только изолированный stage: контролируем территории/presence и время hold.
// Production выполняет выбор, fallback, waypoint и завершение настоящего матча.
modded class SCR_CampaignMilitaryBaseComponent
{
	int m_iAICFEndgamePresence = -1;
	bool m_bAICFEndgameContested;
	override SCR_EBaseCaptureState GetCaptureState()
	{
		if (m_bAICFEndgameContested)
			return SCR_EBaseCaptureState.CONTESTED;
		return super.GetCaptureState();
	}
	override bool AreEnemiesPresent()
	{
		if (m_iAICFEndgamePresence >= 0)
			return m_iAICFEndgamePresence == 1;
		return super.AreEnemiesPresent();
	}
}

modded class AICF_OrderPlanner
{
	bool EndgameProbeOldAttack(AICF_GroupSlot slot, SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent base)
	{
		return ReplaceOrder(slot, faction, base, "PROBE_OLD_ATTACK", POSTURE_ATTACK_PRIMARY, "PROBE");
	}
}

modded class AICF_GroupSlot
{
	protected int m_iEndgameProbeHoldAge = -1;
	void EndgameProbeAgeHold(int ageMs)
	{
		m_iEndgameProbeHoldAge = ageMs;
	}
	override bool IsPersistentStuckFieldHoldRetryDue(int holdMs)
	{
		if (m_iEndgameProbeHoldAge >= 0)
			return IsPersistentStuckContextCurrent() && m_iEndgameProbeHoldAge >= holdMs;
		return super.IsPersistentStuckFieldHoldRetryDue(holdMs);
	}
}

modded class AICF_RouteRecoveryEpisode
{
	protected bool m_bEndgameProbeExpired;
	void EndgameProbeExpire()
	{
		m_bEndgameProbeExpired = true;
	}
	override int GetAgeMs()
	{
		if (m_bEndgameProbeExpired)
			return DEADLINE_MS + 1;
		return super.GetAgeMs();
	}
}

modded class AICF_MatchController
{
	protected int m_iEndgameProbeStart;
	protected int m_iEndgameProbePhase;
	protected int m_iEndgameProbePhaseAt;
	protected bool m_bEndgameProbeFailed;
	protected bool m_bEndgameProbeEndObserved;
	protected SCR_CampaignMilitaryBaseComponent m_EndgameProbeBase;
	protected AICF_GroupSlot m_EndgameProbeSlot;

	override protected void Update()
	{
		string enabled;
		bool probe = System.GetCLIParam("aicfEndgameProbe", enabled) && enabled == "1";
		if (probe && m_bRosterReady && !m_bStopped)
			EndgameProbeTick();
		super.Update();
		if (probe && m_VictorySystem && m_VictorySystem.IsEnded() && !m_Campaign.IsRunning())
		{
			// Следующий production Update должен сам подтвердить MATCH_END.
			if (!m_bEndgameProbeEndObserved)
			{
				m_bEndgameProbeEndObserved = true;
				return;
			}
			EndgameProbeCheck("TERRITORIAL_MATCH_END", m_iEndgameProbePhase == 3 && m_VictorySystem.GetWinnerKey() == m_USSRFaction.GetFactionKey());
			EndgameProbeCheck("POSITIVE_TICKETS", m_USState.GetTickets() > 0 && m_USSRState.GetTickets() > 0);
			EndgameProbeCheck("PRESENCE_DOES_NOT_BLOCK", m_EndgameProbeBase.AreEnemiesPresent());
			Print(string.Format("[AICF][ENDGAME_PROBE] finished=1 pass=%1", !m_bEndgameProbeFailed));
			GetGame().RequestClose();
		}
	}

	protected void EndgameProbeCheck(string name, bool pass)
	{
		if (!pass)
			m_bEndgameProbeFailed = true;
		Print(string.Format("[AICF][ENDGAME_PROBE] case=%1 pass=%2", name, pass));
	}

	protected void EndgameProbeTick()
	{
		int now = System.GetTickCount();
		if (!m_iEndgameProbeStart)
		{
			m_iEndgameProbeStart = now;
			m_Construction.Stop();
			m_InfantryRecruitment.Stop();
			for (int s; s < m_USSRState.GetSlotCount(); s++)
			{
				AICF_GroupSlot slot = m_USSRState.GetSlot(s);
				slot.SetDesiredSize(1);
				m_USState.GetSlot(s).SetDesiredSize(1);
				if (!m_EndgameProbeSlot && slot.GetRole() == AICF_EGroupRole.ATTACK && slot.GetUnitType() == AICF_EGroupUnitType.INFANTRY)
					m_EndgameProbeSlot = slot;
			}
			for (int n; n < m_ObjectiveGraph.GetNodeCount(); n++)
			{
				AICF_ObjectiveNode node = m_ObjectiveGraph.GetNode(n);
				SCR_CampaignMilitaryBaseComponent base = node.GetBase();
				base.m_iAICFEndgamePresence = 0;
				if (base.IsHQ())
					continue;
				if (!m_EndgameProbeBase && node.IsObjective())
				{
					m_EndgameProbeBase = base;
					base.m_iAICFEndgamePresence = 1;
					base.m_bAICFEndgameContested = true;
				}
				base.SetFaction(m_USSRFaction);
			}
			m_iEndgameProbePhaseAt = now;
			return;
		}
		if (now - m_iEndgameProbeStart > 180000)
		{
			EndgameProbeCheck("TIMEOUT", false);
			GetGame().RequestClose();
			return;
		}
		if (m_bGraphRebuildNeeded || m_bReplanScheduled || now - m_iEndgameProbePhaseAt < 10000)
			return;
		if (m_iEndgameProbePhase == 0)
		{
			EndgameProbeCheck("CONTESTED_NO_VICTORY", !m_VictorySystem.IsEnded());
			EndgameProbeCheck("FIXTURE_SLOT", m_EndgameProbeSlot && m_EndgameProbeBase);
			if (!m_EndgameProbeSlot || !m_EndgameProbeBase)
				return;
			// Проигрывающая сторона: exhausted episode должен пройти полный
			// Reliability path, а не только прямой вызов hold timer из fixture.
			AICF_GroupSlot losingSlot = m_USState.GetSlot(0);
			EndgameProbeCheck("LOSING_SLOT_READY", losingSlot.CompleteInfantryMusterIfReady() &&
				m_OrderPlanner.EndgameProbeOldAttack(losingSlot, m_USFaction, m_EndgameProbeBase) && !losingSlot.IsRecruitingInfantry());
			SCR_AIGroup losingGroup = losingSlot.GetGroup();
			int losingGeneration = losingSlot.GetSpawnGeneration();
			AICF_RouteRecoveryEpisode episode = losingSlot.GetRouteRecoveryEpisode();
			episode.Begin(losingSlot);
			episode.EndgameProbeExpire();
			ProcessRouteRecoveryEpisode(losingSlot, m_USFaction);
			EndgameProbeCheck("EXHAUSTED_EPISODE_HOLD", episode.IsBlocked(losingSlot) && losingSlot.IsPersistentStuckFieldHold());
			ProcessFactionReliability(m_USState, m_USFaction);
			EndgameProbeCheck("EXHAUSTED_NO_EARLY_REVIEW", episode.IsBlocked(losingSlot));
			losingSlot.EndgameProbeAgeHold(300001);
			ProcessFactionReliability(m_USState, m_USFaction);
			EndgameProbeCheck("EXHAUSTED_RELIABILITY_REARM", !episode.IsBlocked(losingSlot) && !losingSlot.IsPersistentStuckFieldHold());
			EndgameProbeCheck("LOSING_SIDE_ATTACK", !losingSlot.IsPersistentStuckFieldHold() && losingSlot.GetOperationalPosture() != "AREA_SECURITY" && m_OrderPlanner.IsOrderValid(losingSlot, m_USFaction));
			EndgameProbeCheck("REVIEW_IDENTITY_PRESERVED", losingSlot.GetGroup() == losingGroup && losingSlot.GetSpawnGeneration() == losingGeneration);
			losingSlot.EndgameProbeAgeHold(-1);
			SCR_AIGroup group = m_EndgameProbeSlot.GetGroup();
			EndgameProbeCheck("OLD_ATTACK", m_OrderPlanner.EndgameProbeOldAttack(m_EndgameProbeSlot, m_USSRFaction, m_EndgameProbeBase));
			m_EndgameProbeSlot.RecordOrderReliabilityRepairFailure();
			m_EndgameProbeSlot.RecordOrderReliabilityRepairFailure();
			ApplyOrderReliabilityRepairBudgetFallback(m_USSRState, m_EndgameProbeSlot, m_USSRFaction, "PROBE_TWO_FAILURES");
			EndgameProbeCheck("BUDGET_SECURITY_TASK", m_EndgameProbeSlot.GetOperationalPosture() == "AREA_SECURITY" &&
				m_OrderPlanner.IsOrderValid(m_EndgameProbeSlot, m_USSRFaction) && group == m_EndgameProbeSlot.GetGroup());
			EndgameProbeCheck("PLAYER_ATTACK_STILL_REJECTED", !m_OrderPlanner.IsStrategicTargetValid(m_EndgameProbeSlot, m_USSRFaction, m_EndgameProbeBase));
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
			if (leader && m_OrderPlanner.HoldPositionForPersistentStuck(m_EndgameProbeSlot, m_USSRFaction, m_EndgameProbeSlot.GetTargetBase(), leader.GetOrigin()))
			{
				EndgameProbeCheck("HOLD_NO_EARLY_RETRY", !m_EndgameProbeSlot.ConsumePersistentStuckReview());
				m_EndgameProbeSlot.EndgameProbeAgeHold(300001);
				EndgameProbeCheck("HOLD_REVIEW_DUE", m_EndgameProbeSlot.ConsumePersistentStuckReview());
				EndgameProbeCheck("HOLD_BACKOFF", !m_EndgameProbeSlot.ConsumePersistentStuckReview());
				m_EndgameProbeSlot.EndgameProbeAgeHold(-1);
				ResumePersistentStuckFieldHold(m_EndgameProbeSlot, m_USSRFaction, "PROBE_REVIEW");
				AssignFactionStrategicOrder(m_EndgameProbeSlot, m_USSRFaction, "PROBE_REVIEW");
			}
			else
				EndgameProbeCheck("HOLD_CREATED", false);
			m_EndgameProbeBase.SetFaction(m_USFaction);
			m_iEndgameProbePhase = 1;
			m_iEndgameProbePhaseAt = now;
		}
		else if (m_iEndgameProbePhase == 1)
		{
			m_USSRAICommander.Tick(this, "PROBE_NEW_ATTACK");
			EndgameProbeCheck("ATTACK_RESUMED", m_EndgameProbeSlot.GetOperationalPosture() != "AREA_SECURITY" && m_OrderPlanner.IsOrderValid(m_EndgameProbeSlot, m_USSRFaction));
			m_EndgameProbeBase.SetFaction(m_USSRFaction);
			m_iEndgameProbePhase = 2;
			m_iEndgameProbePhaseAt = now;
		}
		else if (m_iEndgameProbePhase == 2)
		{
			EndgameProbeCheck("STILL_CONTESTED", !m_VictorySystem.IsEnded());
			m_EndgameProbeBase.m_bAICFEndgameContested = false;
			m_iEndgameProbePhase = 3;
			m_iEndgameProbePhaseAt = now;
		}
	}
}
