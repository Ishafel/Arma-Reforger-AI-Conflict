// Только stage: состав, удержание, смена владельца базы и потери. Не combat acceptance.
modded class AICF_FIAGarrisonService
{
	protected int m_iAICFProbeStarted;
	protected int m_iAICFProbeReady;
	protected int m_iAICFProbePhase;
	protected int m_iAICFProbeFailures;
	protected int m_iAICFProbeChecks;
	protected bool m_bAICFProbeFinished;
	protected bool m_bAICFProbeEnabled;

	override void Start(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		super.Start(campaign, graph);
		string expected;
		m_bAICFProbeEnabled = System.GetCLIParam("aicfGarrisonProbe", expected);
		if (!m_bAICFProbeEnabled) return;
		m_iAICFProbeStarted = System.GetTickCount();
		AICF_ProbeCheck("HEADER_DIFFICULTY", AICF_Difficulty.Get() == expected.ToInt());
		AICF_ProbeCheck("PLAN_COUNT", s_aDefenders.Count() == expected.ToInt() * 5);
	}

	protected void AICF_ProbeCheck(string name, bool passed)
	{
		m_iAICFProbeChecks++;
		if (!passed) m_iAICFProbeFailures++;
		Print(string.Format("[AICF][GARRISON_PROBE] case=%1 passed=%2", name, passed));
	}

	override void Update()
	{
		super.Update();
		if (!m_bAICFProbeEnabled || m_bAICFProbeFinished) return;
		int now = System.GetTickCount();
		if (now - m_iAICFProbeStarted > 240000)
		{
			AICF_ProbeCheck("DEADLINE", false);
			AICF_ProbeFinish();
			return;
		}
		if (m_iAICFProbePhase == 0)
		{
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				if (g.m_bRetired) { AICF_ProbeCheck("INITIALIZATION_" + g.m_iSlot, false); AICF_ProbeFinish(); return; }
				if (!g.m_bReady) return;
			}
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				AICF_ProbeCheck("READY_" + g.m_iSlot, g.VehicleIdentity() && g.GroupIdentity() && g.m_aCrew.Count() == g.m_aSeats.Count());
				AICF_ProbeCheck("BASE_RADIUS_" + g.m_iSlot, vector.DistanceXZ(g.m_vPosition, g.m_Base.GetOwner().GetOrigin()) <= 85);
			}
			m_iAICFProbeReady = now;
			m_iAICFProbePhase = 1;
			return;
		}
		if (now - m_iAICFProbeReady < 60000) return;
		if (m_iAICFProbePhase == 1)
		{
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				AICF_ProbeCheck("HOLD_" + g.m_iSlot, g.VehicleIdentity() && vector.DistanceXZ(g.m_Vehicle.GetOrigin(), g.m_vPosition) <= 5);
				int seated;
				foreach (BaseCompartmentSlot seat : g.m_aSeats)
				{
					if (seat && g.OwnsMember(seat.GetOccupant())) seated++;
				}
				AICF_ProbeCheck("CREW_REMAINS_" + g.m_iSlot, seated == g.m_aCrew.Count());
				AICF_ProbeCheck("NO_ROUTE_" + g.m_iSlot, !g.m_Group.GetCurrentWaypoint());
			}
			if (s_aDefenders.IsEmpty()) { AICF_ProbeFinish(); return; }
			AICF_FIAGarrison first = s_aDefenders[0];
			first.m_Base.SetFaction(SCR_GameModeCampaign.Cast(GetGame().GetGameMode()).GetFactionByEnum(SCR_ECampaignFaction.BLUFOR));
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(first.m_aCrew[0].FindComponent(SCR_CharacterDamageManagerComponent));
			if (damage) damage.Kill(Instigator.CreateInstigatorGM());
			m_iAICFProbeReady = now;
			m_iAICFProbePhase = 2;
			return;
		}
		AICF_FIAGarrison first = s_aDefenders[0];
		AICF_ProbeCheck("CAPTURE_RETAINS_GARRISON", first.VehicleIdentity() && !first.m_bRetired && first.m_Base.GetFaction() != first.m_Faction);
		AICF_ProbeCheck("CASUALTY_NO_REPLACEMENT", first.m_iGeneration == 1 && first.m_Group.GetAgentsCount() < first.m_aCrew.Count());
		AICF_ProbeCheck("SURVIVOR_REMAINS_DEFENDER", IsDefender(first.m_aCrew[1]));
		AICF_ProbeFinish();
	}

	protected void AICF_ProbeFinish()
	{
		m_bAICFProbeFinished = true;
		Print(string.Format("[AICF][GARRISON_PROBE_FINISHED] checks=%1 failures=%2", m_iAICFProbeChecks, m_iAICFProbeFailures));
		GetGame().RequestClose();
	}
}
