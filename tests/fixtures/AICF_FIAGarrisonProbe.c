// Только stage: состав, локальный патруль, смена владельца базы и потери. Не combat acceptance.
modded class AICF_FIAGarrisonService
{
	protected int m_iAICFProbeStarted;
	protected int m_iAICFProbeReady;
	protected int m_iAICFProbePhase;
	protected int m_iAICFProbeFailures;
	protected int m_iAICFProbeChecks;
	protected bool m_bAICFProbeFinished;
	protected bool m_bAICFProbeEnabled;
	protected int m_iAICFProbeSample;
	protected ref map<AICF_FIAGarrison, float> m_mAICFDistance = new map<AICF_FIAGarrison, float>();
	protected ref map<AICF_FIAGarrison, float> m_mAICFRadius = new map<AICF_FIAGarrison, float>();

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
		if (now - m_iAICFProbeStarted > 420000)
		{
			AICF_ProbeCheck("DEADLINE", false);
			AICF_ProbeFinish();
			return;
		}
		if (m_iAICFProbePhase == 0)
		{
			bool allReady = true;
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				if (g.m_bReady && g.VehicleIdentity())
				{
					DamageManagerComponent vehicleDamage = DamageManagerComponent.Cast(g.m_Vehicle.FindComponent(DamageManagerComponent));
					if (vehicleDamage) vehicleDamage.EnableDamageHandling(false);
					foreach (ChimeraCharacter member : g.m_aCrew)
					{
						if (!member) continue;
						DamageManagerComponent memberDamage = DamageManagerComponent.Cast(member.FindComponent(DamageManagerComponent));
						if (memberDamage) memberDamage.EnableDamageHandling(false);
					}
				}
				if (g.m_bRetired) { AICF_ProbeCheck("INITIALIZATION_" + g.m_iSlot, false); AICF_ProbeFinish(); return; }
				if (!g.m_bReady) allReady = false;
			}
			if (!allReady) return;
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				AICF_ProbeCheck("READY_" + g.m_iSlot, g.VehicleIdentity() && g.GroupIdentity() && g.m_aCrew.Count() == g.m_aSeats.Count());
				AICF_ProbeCheck("BASE_RADIUS_" + g.m_iSlot, vector.DistanceXZ(g.m_vPosition, g.m_Base.GetOwner().GetOrigin()) <= 125);
			}
			m_iAICFProbeReady = now;
			m_iAICFProbePhase = 1;
			return;
		}
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (!g.VehicleIdentity()) continue;
			float distance, radius;
			m_mAICFDistance.Find(g, distance);
			m_mAICFRadius.Find(g, radius);
			m_mAICFDistance.Set(g, Math.Max(distance, vector.DistanceXZ(g.m_Vehicle.GetOrigin(), g.m_vPosition)));
			m_mAICFRadius.Set(g, Math.Max(radius, vector.DistanceXZ(g.m_Vehicle.GetOrigin(), g.m_vHome)));
			if (now >= m_iAICFProbeSample)
			{
				g.Log("FIA_GARRISON_PATROL_SAMPLE", string.Format("position=%1 displacement=%2 radius=%3 leg=%4", g.m_Vehicle.GetOrigin(), distance, radius, g.m_iPatrolLeg));
				SCR_AIGroupUtilityComponent utility = g.m_Group.GetGroupUtilityComponent();
				string groupAction;
				if (utility && utility.GetCurrentAction()) groupAction = utility.GetCurrentAction().Type().ToString();
				foreach (BaseCompartmentSlot seat : g.m_aSeats)
				{
					if (!seat || seat.GetType() != ECompartmentType.PILOT || !seat.GetOccupant()) continue;
					AIControlComponent control = AIControlComponent.Cast(seat.GetOccupant().FindComponent(AIControlComponent));
					if (!control || !control.GetAIAgent()) continue;
					SCR_AIUtilityComponent driver = SCR_AIUtilityComponent.Cast(control.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
					string action, context;
					if (driver && driver.GetCurrentBehavior())
					{
						action = driver.GetCurrentBehavior().Type().ToString();
						if (driver.GetCurrentBehavior().GetGroupActivityContext()) context = driver.GetCurrentBehavior().GetGroupActivityContext().Type().ToString();
					}
					g.Log("FIA_GARRISON_PATROL_AI", string.Format("group_action=%1 driver_action=%2 driver_context=%3 can_drive=%4 arrivals=%5 waypoint=%6", groupAction, action, context, g.CanDrive(), g.m_iPatrolArrivals, g.m_Group.GetCurrentWaypoint()));
				}
			}
		}
		if (now >= m_iAICFProbeSample) m_iAICFProbeSample = now + 15000;
		int duration = 60000;
		if (m_iAICFProbePhase == 1 && !s_aDefenders.IsEmpty()) duration = 240000;
		if (now - m_iAICFProbeReady < duration) return;
		if (m_iAICFProbePhase == 1)
		{
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				float distance, radius;
				m_mAICFDistance.Find(g, distance);
				m_mAICFRadius.Find(g, radius);
				AICF_ProbeCheck("LOCAL_RADIUS_" + g.m_iSlot, g.VehicleIdentity() && radius <= 250 && vector.DistanceXZ(g.m_Vehicle.GetOrigin(), g.m_vHome) <= AICF_FIAGarrisonPatrol.RETURN_RADIUS);
				int seated;
				foreach (BaseCompartmentSlot seat : g.m_aSeats)
				{
					if (seat && g.OwnsMember(seat.GetOccupant())) seated++;
				}
				AICF_ProbeCheck("CREW_REMAINS_" + g.m_iSlot, seated == g.m_aCrew.Count());
				AICF_ProbeCheck("PATROL_MOVED_" + g.m_iSlot, distance >= 25 && g.m_iPatrolArrivals >= 2);
			}
			if (s_aDefenders.IsEmpty()) { AICF_ProbeFinish(); return; }
			AICF_FIAGarrison first = s_aDefenders[0];
			first.m_Base.SetFaction(SCR_GameModeCampaign.Cast(GetGame().GetGameMode()).GetFactionByEnum(SCR_ECampaignFaction.BLUFOR));
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(first.m_aCrew[0].FindComponent(SCR_CharacterDamageManagerComponent));
			if (damage) { damage.EnableDamageHandling(true); damage.Kill(Instigator.CreateInstigatorGM()); }
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
