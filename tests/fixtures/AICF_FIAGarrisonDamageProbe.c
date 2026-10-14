// Только stage. Детерминированные assets из настоящего FIA catalog;
// HandleDamage/Kill/DeleteRplEntity воздействуют на настоящие server entities.
modded class AICF_FIAGarrison
{
	void AICF_ProbeLoseDamageObserver()
	{
		m_VehicleIntegrity.Stop();
		m_VehicleIntegrity = null;
	}
}

modded class AICF_FIAGarrisonRecovery
{
	static bool s_bAICFDamagePlayerVeto;
	static AICF_FIAGarrison s_AICFDamageAtCrewFence;
	bool m_bAICFDamageAtCandidate;

	static override bool PlayersClear(vector source, vector destination)
	{
		if (s_AICFDamageAtCrewFence)
		{
			AICF_FIAGarrison g = s_AICFDamageAtCrewFence;
			s_AICFDamageAtCrewFence = null;
			AICF_FIAGarrisonDamageProbe.Damage(g.m_Vehicle, false);
		}
		return !s_bAICFDamagePlayerVeto && super.PlayersClear(source, destination);
	}

	override bool Candidate(AICF_FIAGarrison g, int index, out vector pose[4])
	{
		bool found = super.Candidate(g, index, pose);
		if (found && m_bAICFDamageAtCandidate)
		{
			m_bAICFDamageAtCandidate = false;
			AICF_FIAGarrisonDamageProbe.Damage(g.m_Vehicle, false);
		}
		return found;
	}
}

class AICF_FIAGarrisonDamageProbe
{
	static bool Damage(IEntity entity, bool component)
	{
		SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		HitZone zone = manager.GetDefaultHitZone();
		if (component)
		{
			array<HitZone> zones = {};
			manager.GetAllHitZonesInHierarchy(zones);
			zone = null;
			foreach (HitZone candidate : zones)
			{
				if (candidate != manager.GetDefaultHitZone() && candidate.GetMaxHealth() > 0 && candidate.HasColliderNodes()) { zone = candidate; break; }
			}
		}
		if (!zone) return false;
		float health = zone.GetHealth();
		manager.EnableDamageHandling(true);
		vector hit[3];
		hit[0] = entity.GetOrigin();
		SCR_DamageContext context = new SCR_DamageContext(EDamageType.TRUE, zone.GetMaxHealth() * 0.05, hit, entity, zone, Instigator.CreateInstigatorGM(), null, -1, -1);
		manager.HandleDamage(context);
		Print(string.Format("[AICF][DAMAGE_PROBE_DAMAGE] entity=%1 zone=%2 before=%3 after=%4 component=%5 alive=%6", entity.GetID(), zone.GetName(), health, zone.GetHealth(), component, !manager.IsDestroyed()));
		return zone.GetHealth() < health && !manager.IsDestroyed();
	}

	static void Repair(IEntity entity)
	{
		SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		array<HitZone> zones = {};
		manager.GetAllHitZonesInHierarchy(zones);
		foreach (HitZone zone : zones) zone.SetHealthScaled(1);
	}
}

modded class AICF_FIAGarrisonService
{
	protected string m_sDamageProbe;
	protected bool m_bDamageProbeReady;
	protected bool m_bDamageProbeDone;
	protected int m_iDamageProbeStart;
	protected int m_iDamageProbePhase;
	protected int m_iDamageProbeChecks;
	protected int m_iDamageProbeFailures;
	protected ref AICF_FIAGarrisonRecovery m_DamageRecovery = new AICF_FIAGarrisonRecovery();

	override void Start(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		super.Start(campaign, graph);
		if (!System.GetCLIParam("aicfDamageRecoveryProbe", m_sDamageProbe)) return;
		m_iDamageProbeStart = System.GetTickCount();
		AICF_FIAGarrisonCatalog catalog = new AICF_FIAGarrisonCatalog();
		if (!catalog.Build(campaign.GetFactionByEnum(SCR_ECampaignFaction.INDFOR), AICF_ContentProfile.GetActive()) || catalog.m_aTanks.IsEmpty())
		{
			DamageCheck("CATALOG_TANK_APC", false);
			DamageFinish();
			return;
		}
		for (int i; i < s_aDefenders.Count(); i++)
		{
			AICF_FIAGarrison g = s_aDefenders[i];
			g.m_bTank = i == 0 || i == 2 || i == 4 || i == 6 || i == 7;
			g.m_bPassengers = !g.m_bTank;
			g.m_aPassengerPrefabs.Clear();
			if (g.m_bTank) g.m_sPrefab = catalog.m_aTanks[0];
			else
			{
				g.m_sPrefab = catalog.m_aAPCs[0];
				g.m_sPassengerTemplate = catalog.m_aPassengers[0].m_sPrefab;
				foreach (ResourceName prefab : catalog.m_aPassengers[0].m_aMembers) g.m_aPassengerPrefabs.Insert(prefab);
			}
			g.Log("DAMAGE_PROBE_ASSET", "prefab=" + g.m_sPrefab);
		}
	}

	protected void DamageCheck(string name, bool passed)
	{
		m_iDamageProbeChecks++;
		if (!passed) m_iDamageProbeFailures++;
		Print(string.Format("[AICF][DAMAGE_RECOVERY_PROBE] mode=%1 case=%2 passed=%3", m_sDamageProbe, name, passed));
	}

	protected void DamageFinish()
	{
		m_bDamageProbeDone = true;
		AICF_FIAGarrisonRecovery.s_bAICFDamagePlayerVeto = false;
		AICF_FIAGarrisonRecovery.s_AICFDamageAtCrewFence = null;
		Print(string.Format("[AICF][DAMAGE_RECOVERY_PROBE_FINISHED] mode=%1 checks=%2 failures=%3", m_sDamageProbe, m_iDamageProbeChecks, m_iDamageProbeFailures));
		GetGame().RequestClose();
	}

	protected void Prepare(AICF_FIAGarrison g, int now)
	{
		m_Patrol.Stop(g);
		g.m_iPatrolRetryAtMs = now + 600000;
		g.m_iCrewRecoveryNextMs = 0;
		g.m_iRecoveryNextMs = 0;
		g.m_Vehicle.GetPhysics().SetVelocity(vector.Zero);
		g.m_Vehicle.GetPhysics().SetAngularVelocity(vector.Zero);
		SCR_AIWorld ai = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		vector endpoint;
		if (AICF_FIAGarrisonPatrol.FindRoadEndpoint(ai.GetRoadNetworkManager(), g.m_Vehicle.GetOrigin(), g.m_vHome, g.m_iPatrolDirection, endpoint)) g.m_vPatrolTarget = endpoint;
	}

	protected bool BlockedBoth(AICF_FIAGarrison g, int now)
	{
		vector before[4], after[4];
		g.m_Vehicle.GetWorldTransform(before);
		int vehicleCount = g.m_iVehicleTeleports;
		int crewCount = g.m_iCrewTeleports;
		EntityID vehicleId = g.m_Vehicle.GetID();
		EntityID groupId = g.m_Group.GetID();
		g.m_iRecoveryNextMs = 0;
		bool first = m_DamageRecovery.TryRelocate(g, now, "CREW_RETURN");
		g.m_iRecoveryNextMs = 0;
		bool second = m_DamageRecovery.TryRelocate(g, now, "ROUTE_STALLED");
		m_Crew.UpdateGarrison(g, now);
		g.m_Vehicle.GetWorldTransform(after);
		bool same = true;
		for (int i; i < 4; i++) { if (vector.Distance(before[i], after[i]) > 0.001) same = false; }
		return !first && !second && same && vehicleCount == g.m_iVehicleTeleports && crewCount == g.m_iCrewTeleports && vehicleId == g.m_Vehicle.GetID() && groupId == g.m_Group.GetID();
	}

	protected int SeatIndex(AICF_FIAGarrison g, int role)
	{
		for (int i; i < g.m_aSeats.Count(); i++)
		{
			BaseCompartmentSlot seat = g.m_aSeats[i];
			if (role == 0 && seat.GetType() == ECompartmentType.PILOT) return i;
			if (role == 1 && seat.GetType() == ECompartmentType.TURRET) return i;
			string name = seat.GetCompartmentName(true);
			name.ToLower();
			if (role == 2 && name.Contains("commander")) return i;
			if (role == 3 && !g.m_aEssentialSeats[i]) return i;
		}
		return -1;
	}

	override void Update()
	{
		if (m_sDamageProbe.IsEmpty()) { super.Update(); return; }
		if (m_bDamageProbeDone) return;
		int now = System.GetTickCount();
		if (now - m_iDamageProbeStart > 420000) { DamageCheck("DEADLINE", false); DamageFinish(); return; }
		if (!m_bDamageProbeReady)
		{
			super.Update();
			bool ready = s_aDefenders.Count() >= 11;
			foreach (AICF_FIAGarrison g : s_aDefenders)
			{
				if (g.m_bRetired) { DamageCheck("INITIALIZATION_" + g.m_iSlot, false); DamageFinish(); return; }
				if (!g.m_bReady) { ready = false; continue; }
				Prepare(g, now);
				SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.Cast(g.m_Vehicle.FindComponent(SCR_DamageManagerComponent));
				manager.EnableDamageHandling(false);
				foreach (ChimeraCharacter member : g.m_aCrew) member.GetDamageManager().EnableDamageHandling(false);
			}
			if (!ready) return;
			foreach (AICF_FIAGarrison g : s_aDefenders) DamageCheck("PRISTINE_" + g.m_iSlot, g.RecoveryAllowed() && g.CanDrive());
			m_bDamageProbeReady = true;
			return;
		}
		if (m_sDamageProbe == "identity")
		{
			if (m_iDamageProbePhase == 0) { IdentityCases(now); m_iDamageProbePhase = 1; return; }
			AICF_FIAGarrison crewGate = s_aDefenders[8];
			ChimeraCharacter member = crewGate.m_aCrew[SeatIndex(crewGate, 0)];
			CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
			if (access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut()) return;
			AICF_FIAGarrisonRecovery.s_AICFDamageAtCrewFence = crewGate;
			m_Crew.UpdateGarrison(crewGate, now);
			DamageCheck("FINAL_CREW_GATE", crewGate.m_iCrewTeleports == 0 && crewGate.RecoveryBan() == "VEHICLE_DAMAGED" && !AICF_FIAGarrisonRecovery.s_AICFDamageAtCrewFence);
			DamageFinish();
			return;
		}
		if (m_iDamageProbePhase == 0)
		{
			for (int i; i < 4; i++)
			{
				AICF_FIAGarrison g = s_aDefenders[i];
				string name = "TANK";
				if (!g.m_bTank) name = "APC";
				if (i >= 2) name += "_COMPONENT";
				else name += "_HULL";
				g.m_bCrewRecoveryPending = true;
				DamageCheck(name + "_ACTUAL_DAMAGE", AICF_FIAGarrisonDamageProbe.Damage(g.m_Vehicle, i >= 2));
				DamageCheck(name + "_DRIVE_ALLOWED", g.CanDrive() && !g.m_bCrewRecoveryPending);
				DamageCheck(name + "_BLOCKED", BlockedBoth(g, now) && g.RecoveryBan() == "VEHICLE_DAMAGED");
				string reason = g.RecoveryBan();
				AICF_FIAGarrisonDamageProbe.Repair(g.m_Vehicle);
				g.m_iPatrolLeg++;
				bool blocked = true;
				for (int retry; retry < 3; retry++) blocked = blocked && BlockedBoth(g, now + retry * 10000);
				DamageCheck(name + "_REPAIR_RETRY_BANNED", blocked && reason == g.RecoveryBan());
			}
			AICF_FIAGarrison wounded = s_aDefenders[4];
			int index = SeatIndex(wounded, 1);
			if (index < 0) { DamageCheck("WOUNDED_SEAT", false); DamageFinish(); return; }
			ChimeraCharacter member = wounded.m_aCrew[index];
			member.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, true, true);
			DamageCheck("CREW_WOUND_ACTUAL", AICF_FIAGarrisonDamageProbe.Damage(member, false));
			DamageCheck("CREW_WOUND_BLOCKED", BlockedBoth(wounded, now) && wounded.RecoveryBan() == "CREW_WOUNDED");
			AICF_FIAGarrisonDamageProbe.Repair(member);
			DamageCheck("CREW_HEAL_BANNED", BlockedBoth(wounded, now + 10000) && wounded.RecoveryBan() == "CREW_WOUNDED");
			for (int role; role < 4; role++)
			{
				AICF_FIAGarrison g = s_aDefenders[5 + role];
				int seatIndex = SeatIndex(g, role);
				if (seatIndex < 0) { DamageCheck("DEATH_SEAT_" + role, false); continue; }
				ChimeraCharacter victim = g.m_aCrew[seatIndex];
				SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(victim.GetDamageManager());
				damage.EnableDamageHandling(true);
				damage.Kill(Instigator.CreateInstigatorGM());
				DamageCheck("DEATH_" + role + "_BLOCKED", damage.IsDestroyed() && BlockedBoth(g, now) && !g.RecoveryBan().IsEmpty());
			}
			AICF_FIAGarrison deployed = s_aDefenders[9];
			deployed.m_bPassengersDeployed = true;
			m_iDamageProbePhase = 1;
			return;
		}
		AICF_FIAGarrison deployed = s_aDefenders[9];
		m_Crew.UpdateGarrison(deployed, now);
		int outside;
		int passengers;
		for (int i; i < deployed.m_aCrew.Count(); i++)
		{
			if (deployed.m_aEssentialSeats[i]) continue;
			passengers++;
			if (!deployed.m_aCrew[i].IsInVehicle()) outside++;
		}
		if (outside != passengers || deployed.m_bDisembarking) return;
		DamageCheck("PLANNED_DISMOUNT_ALLOWED", passengers > 0 && deployed.RecoveryAllowed());
		Prepare(deployed, now);
		if (!m_DamageRecovery.TryRelocate(deployed, now, "ROUTE_STALLED")) return;
		DamageCheck("PRISTINE_DEPLOYED_RECOVERY", deployed.m_iVehicleTeleports > 0 && deployed.RecoveryBan().IsEmpty());
		DamageFinish();
	}

	protected void IdentityCases(int now)
	{
		AICF_FIAGarrison positive = s_aDefenders[0];
		vector before = positive.m_Vehicle.GetOrigin();
		AICF_FIAGarrisonRecovery.s_bAICFDamagePlayerVeto = true;
		DamageCheck("PLAYER_VETO", !m_DamageRecovery.TryRelocate(positive, now, "CREW_RETURN") && vector.Distance(before, positive.m_Vehicle.GetOrigin()) < 0.01 && positive.m_iVehicleTeleports == 0);
		AICF_FIAGarrisonRecovery.s_bAICFDamagePlayerVeto = false;
		positive.m_iRecoveryNextMs = 0;
		DamageCheck("PRISTINE_RECOVERY", m_DamageRecovery.TryRelocate(positive, now, "ROUTE_STALLED") && positive.m_iVehicleTeleports == 1);
		AICF_FIAGarrison missing = s_aDefenders[1];
		RplComponent.DeleteRplEntity(missing.m_aCrew[0], false);
		DamageCheck("DELETED_MEMBER", BlockedBoth(missing, now) && !missing.RecoveryBan().IsEmpty());
		AICF_FIAGarrison stale = s_aDefenders[2];
		stale.m_aCrewIds[0] = EntityID.INVALID;
		DamageCheck("STALE_MEMBER", BlockedBoth(stale, now) && stale.RecoveryBan() == "MISSING_OR_STALE_MEMBER");
		AICF_FIAGarrison vehicle = s_aDefenders[3];
		vehicle.m_VehicleId = EntityID.INVALID;
		DamageCheck("STALE_VEHICLE", BlockedBoth(vehicle, now) && vehicle.RecoveryBan() == "STALE_VEHICLE_GROUP_IDENTITY");
		AICF_FIAGarrison unknown = s_aDefenders[4];
		unknown.AICF_ProbeLoseDamageObserver();
		DamageCheck("UNKNOWN_DAMAGE", BlockedBoth(unknown, now) && unknown.RecoveryBan() == "UNKNOWN_VEHICLE_DAMAGE");
		AICF_FIAGarrison roster = s_aDefenders[5];
		roster.m_aCrewIds.Remove(0);
		DamageCheck("UNKNOWN_ROSTER", BlockedBoth(roster, now) && roster.RecoveryBan() == "UNKNOWN_ORIGINAL_ROSTER");
		AICF_FIAGarrison generation = s_aDefenders[6];
		generation.m_iGeneration++;
		DamageCheck("STALE_GENERATION", BlockedBoth(generation, now) && generation.RecoveryBan() == "STALE_DAMAGE_IDENTITY");
		AICF_FIAGarrison finalGate = s_aDefenders[7];
		m_DamageRecovery.m_bAICFDamageAtCandidate = true;
		DamageCheck("FINAL_TRANSFORM_GATE", BlockedBoth(finalGate, now) && finalGate.RecoveryBan() == "VEHICLE_DAMAGED" && !m_DamageRecovery.m_bAICFDamageAtCandidate);
		AICF_FIAGarrison crewGate = s_aDefenders[8];
		ChimeraCharacter member = crewGate.m_aCrew[SeatIndex(crewGate, 0)];
		member.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, true, true);
		AICF_FIAGarrison replacement = s_aDefenders[9];
		replacement.m_aCrew[0] = positive.m_aCrew[0];
		replacement.m_aCrewIds[0] = positive.m_aCrewIds[0];
		DamageCheck("REPLACED_MEMBER", BlockedBoth(replacement, now) && replacement.RecoveryBan() == "MISSING_OR_STALE_MEMBER");
		AICF_FIAGarrison geometry = s_aDefenders[10];
		geometry.m_vPatrolTarget = geometry.m_Vehicle.GetOrigin();
		vector unchanged = geometry.m_Vehicle.GetOrigin();
		DamageCheck("NO_GEOMETRY_CANDIDATE", !m_DamageRecovery.TryRelocate(geometry, now, "ROUTE_STALLED") && geometry.m_iVehicleTeleports == 0 && vector.Distance(unchanged, geometry.m_Vehicle.GetOrigin()) < 0.001 && geometry.RecoveryBan().IsEmpty());
		DamageCheck("RADIUS_49_50_51", !AICF_FIAGarrisonRecovery.OutsidePlayerRadius("49 0 0", vector.Zero, "200 0 0") && !AICF_FIAGarrisonRecovery.OutsidePlayerRadius("50 0 0", vector.Zero, "200 0 0") && AICF_FIAGarrisonRecovery.OutsidePlayerRadius("51 0 0", vector.Zero, "200 0 0"));
	}
}
