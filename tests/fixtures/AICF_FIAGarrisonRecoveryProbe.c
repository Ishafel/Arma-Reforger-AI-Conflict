// Только изолированный stage. Synthetic player veto проверяет запрет мутаций;
// реальное подключение/позиционирование игрока этим probe не моделируется.
modded class AICF_FIAGarrisonRecovery
{
	static bool s_bAICFProbePlayer;
	static override bool PlayersClear(vector source, vector destination)
	{
		if (s_bAICFProbePlayer) return false;
		return super.PlayersClear(source, destination);
	}
}

modded class AICF_FIAGarrisonService
{
	protected bool m_bRecoveryProbe;
	protected bool m_bRecoveryProbeDone;
	protected int m_iProbeStart;
	protected int m_iProbePhase;
	protected int m_iProbeAt;
	protected int m_iProbeChecks;
	protected int m_iProbeFailures;
	protected int m_iProbeSeat;
	protected int m_iProbeVehicleCount;
	protected int m_iProbeCrewCount;
	protected AICF_FIAGarrison m_ProbeG;
	protected EntityID m_ProbeVehicleId;
	protected EntityID m_ProbeGroupId;
	protected ref AICF_FIAGarrisonRecovery m_ProbeRecovery = new AICF_FIAGarrisonRecovery();

	override void Start(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		super.Start(campaign, graph);
		string value;
		m_bRecoveryProbe = System.GetCLIParam("aicfRecoveryProbe", value) && value == "1";
		m_iProbeStart = System.GetTickCount();
	}

	protected void Check(string name, bool passed)
	{
		m_iProbeChecks++;
		if (!passed) m_iProbeFailures++;
		Print(string.Format("[AICF][RECOVERY_PROBE] case=%1 passed=%2", name, passed));
	}

	protected void Finish()
	{
		m_bRecoveryProbeDone = true;
		AICF_FIAGarrisonRecovery.s_bAICFProbePlayer = false;
		Print(string.Format("[AICF][RECOVERY_PROBE_FINISHED] checks=%1 failures=%2", m_iProbeChecks, m_iProbeFailures));
		GetGame().RequestClose();
	}

	override void Update()
	{
		super.Update();
		if (!m_bRecoveryProbe || m_bRecoveryProbeDone) return;
		int now = System.GetTickCount();
		if (now - m_iProbeStart > 480000) { Check("DEADLINE_PHASE_" + m_iProbePhase, false); Finish(); return; }
		if (m_iProbePhase == 0)
		{
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				if (!g.m_bReady) return;
				DamageManagerComponent damage = DamageManagerComponent.Cast(g.m_Vehicle.FindComponent(DamageManagerComponent));
				if (damage) damage.EnableDamageHandling(false);
				foreach (ChimeraCharacter member : g.m_aCrew)
				{
					DamageManagerComponent memberDamage = DamageManagerComponent.Cast(member.FindComponent(DamageManagerComponent));
					if (memberDamage) memberDamage.EnableDamageHandling(false);
				}
			}
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				int essential;
				foreach (bool seat : g.m_aEssentialSeats) { if (seat) essential++; }
				Check("ESSENTIAL_THREE_" + g.m_iSlot, essential == 3);
			}
			Check("PLAYER_49_BLOCKED", !AICF_FIAGarrisonRecovery.OutsidePlayerRadius("49 0 0", "0 0 0", "100 0 0"));
			Check("PLAYER_50_BLOCKED", !AICF_FIAGarrisonRecovery.OutsidePlayerRadius("50 0 0", "0 0 0", "200 0 0"));
			Check("PLAYER_DESTINATION_BLOCKED", !AICF_FIAGarrisonRecovery.OutsidePlayerRadius("200 0 49", "0 0 0", "200 0 0"));
			Check("PLAYER_HEIGHT_BLOCKED", !AICF_FIAGarrisonRecovery.OutsidePlayerRadius("0 100 0", "0 0 0", "200 0 0"));
			Check("PLAYER_51_CLEAR", AICF_FIAGarrisonRecovery.OutsidePlayerRadius("0 0 51", "0 0 0", "200 0 0"));
			m_ProbeG = s_aDefenders[0];
			m_ProbeVehicleId = m_ProbeG.m_VehicleId;
			m_ProbeGroupId = m_ProbeG.m_GroupId;
			m_iProbePhase = 1;
			m_iProbeAt = now;
		}
		AICF_FIAGarrison g = m_ProbeG;
		if (m_iProbePhase == 1)
		{
			m_Patrol.Stop(g);
			g.m_iPatrolRetryAtMs = now + 10000;
			g.m_Vehicle.GetPhysics().SetVelocity(vector.Zero);
			SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			vector endpoint;
			if (!AICF_FIAGarrisonPatrol.FindRoadEndpoint(ai.GetRoadNetworkManager(), g.m_Vehicle.GetOrigin(), g.m_vHome, g.m_iPatrolDirection, endpoint)) return;
			g.m_vPatrolTarget = endpoint;
			vector before = g.m_Vehicle.GetOrigin();
			AICF_FIAGarrisonRecovery.s_bAICFProbePlayer = true;
			g.m_iRecoveryNextMs = 0;
			bool blocked = !m_ProbeRecovery.TryRelocate(g, now, "PROBE_PLAYER") && vector.Distance(before, g.m_Vehicle.GetOrigin()) < 0.1;
			AICF_FIAGarrisonRecovery.s_bAICFProbePlayer = false;
			g.m_iRecoveryNextMs = 0;
			if (!m_ProbeRecovery.TryRelocate(g, now, "PROBE_ROUTE")) return;
			Check("PLAYER_VETO_NO_VEHICLE_MUTATION", blocked);
			Check("ROUTE_TELEPORT_DISTANCE", vector.DistanceXZ(before, g.m_Vehicle.GetOrigin()) >= 15);
			Check("HOME_BOUND", vector.DistanceXZ(g.m_Vehicle.GetOrigin(), g.m_vHome) <= 140);
			Check("ENTITY_IDS_UNCHANGED", g.m_VehicleId == m_ProbeVehicleId && g.m_GroupId == m_ProbeGroupId && g.m_iGeneration == 1);
			m_iProbePhase = 2;
			m_iProbeSeat = 0;
			m_iProbeAt = now;
			return;
		}
		if (m_iProbePhase == 2)
		{
			if (now - m_iProbeAt < 5000) return;
			while (m_iProbeSeat < g.m_aSeats.Count() && !g.m_aEssentialSeats[m_iProbeSeat]) m_iProbeSeat++;
			if (m_iProbeSeat >= g.m_aSeats.Count())
			{
				if (!g.m_bTank)
				{
					m_ProbeG = s_aDefenders[1];
					m_iProbeSeat = 0;
					return;
				}
				m_ProbeG = s_aDefenders[0];
				m_iProbePhase = 5;
				m_iProbeAt = now;
				return;
			}
			ChimeraCharacter member = g.m_aCrew[m_iProbeSeat];
			m_Patrol.Stop(g);
			g.m_iPatrolRetryAtMs = now + 10000;
			g.m_Vehicle.GetPhysics().SetVelocity(vector.Zero);
			m_iProbeVehicleCount = g.m_iVehicleTeleports;
			m_iProbeCrewCount = g.m_iCrewTeleports;
			AICF_FIAGarrisonRecovery.s_bAICFProbePlayer = true;
			member.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, true, true);
			m_iProbePhase = 3;
			m_iProbeAt = now;
			return;
		}
		if (m_iProbePhase == 3)
		{
			if (now - m_iProbeAt < 4000) return;
			Check("PLAYER_VETO_CREW_" + g.m_iSlot + "_" + m_iProbeSeat, g.m_aSeats[m_iProbeSeat].GetOccupant() != g.m_aCrew[m_iProbeSeat] && g.m_iCrewTeleports == m_iProbeCrewCount && g.m_iVehicleTeleports == m_iProbeVehicleCount);
			AICF_FIAGarrisonRecovery.s_bAICFProbePlayer = false;
			m_iProbePhase = 4;
			m_iProbeAt = now;
			return;
		}
		if (m_iProbePhase == 4)
		{
			if (g.m_aSeats[m_iProbeSeat].GetOccupant() != g.m_aCrew[m_iProbeSeat] || g.m_iVehicleTeleports <= m_iProbeVehicleCount) return;
			Check("EXACT_SEAT_RETURN_" + g.m_iSlot + "_" + m_iProbeSeat, g.m_iCrewTeleports > m_iProbeCrewCount);
			Check("VEHICLE_ADVANCE_AFTER_RETURN_" + g.m_iSlot + "_" + m_iProbeSeat, g.CanDrive() && !g.m_bCrewRecoveryPending);
			m_iProbeSeat++;
			m_iProbePhase = 2;
			m_iProbeAt = now;
			return;
		}
		if (m_iProbePhase == 5)
		{
			// Реальный threat system, искусственно заданный обстрел; не доказательство огня по противнику.
			foreach (ChimeraCharacter member : g.m_aCrew)
			{
				AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(control.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
				utility.m_ThreatSystem.SetThreatValues(1, 1, 0, 1);
			}
			int seated, deployed;
			for (int i; i < g.m_aCrew.Count(); i++)
			{
				if (g.m_aEssentialSeats[i]) { if (g.m_aSeats[i].GetOccupant() == g.m_aCrew[i]) seated++; }
				else if (!g.m_aCrew[i].IsInVehicle()) deployed++;
			}
			if (g.m_bPassengersDeployed && !g.m_bDisembarking && deployed == 7)
			{
				Check("DESANT_SEVEN_OUT", deployed == 7);
				Check("ESSENTIAL_THREE_REMAIN", seated == 3);
				Check("ROSTER_IDENTITY_UNCHANGED", g.m_aCrew.Count() == 10 && g.m_iGeneration == 1);
				m_iProbePhase = 6;
				m_iProbeAt = now;
				m_iProbeVehicleCount = g.m_iPatrolArrivals;
			}
			return;
		}
		if (m_iProbePhase == 6 && g.m_iPatrolArrivals > m_iProbeVehicleCount)
		{
			int onFoot;
			for (int i; i < g.m_aCrew.Count(); i++)
			{
				if (!g.m_aEssentialSeats[i] && !g.m_aCrew[i].IsInVehicle()) onFoot++;
			}
			Check("DESANT_NOT_REBOARDED", onFoot == 7);
			Check("PATROL_RESUMED_AFTER_DESANT", true);
			m_iProbePhase = 7;
			m_iProbeAt = now;
			m_iProbeVehicleCount = g.m_iVehicleTeleports;
			return;
		}
		if (m_iProbePhase == 7)
		{
			SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			vector target;
			if (!AICF_FIAGarrisonPatrol.FindRoadEndpoint(ai.GetRoadNetworkManager(), g.m_Vehicle.GetOrigin(), g.m_vHome, g.m_iPatrolDirection, target)) return;
			AICF_VehicleTaskHandoff handoff = new AICF_VehicleTaskHandoff(null, null, null);
			if (!handoff.MoveFIAGarrison(g, target)) return;
			g.m_Vehicle.GetPhysics().SetVelocity(vector.Zero);
			g.m_iRecoveryNextMs = 0;
			g.m_iPatrolFailures = 1;
			g.m_bPatrolMoveFailed = true;
			m_Patrol.Update(g, now);
			if (g.m_iVehicleTeleports <= m_iProbeVehicleCount) return;
			Check("STALLED_ROUTE_RECOVERY_TRIGGER", true);
			EntityID previous = g.m_aCrewIds[0];
			g.m_aCrewIds[0] = EntityID.INVALID;
			g.m_iRecoveryNextMs = 0;
			Check("STALE_MEMBER_BLOCKS_TELEPORT", !m_ProbeRecovery.TryRelocate(g, now, "PROBE_STALE"));
			g.m_aCrewIds[0] = previous;
			ChimeraCharacter driver = g.m_aCrew[0];
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(driver.FindComponent(SCR_CharacterDamageManagerComponent));
			damage.EnableDamageHandling(true);
			damage.Kill(Instigator.CreateInstigatorGM());
			m_iProbeCrewCount = g.m_iCrewTeleports;
			m_iProbePhase = 8;
			m_iProbeAt = now;
			return;
		}
		if (m_iProbePhase == 8 && now - m_iProbeAt >= 10000)
		{
			Check("DEAD_DRIVER_NOT_RECOVERED", !g.OwnsMember(g.m_aCrew[0]) && g.m_iCrewTeleports == m_iProbeCrewCount && !g.CanDrive());
			Check("NO_REPLACEMENT", g.m_iGeneration == 1 && g.m_VehicleId == m_ProbeVehicleId && g.m_GroupId == m_ProbeGroupId);
			Finish();
		}
	}
}
