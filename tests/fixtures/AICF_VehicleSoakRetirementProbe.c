// Controlled all-crew death. Только fixture; не ordinary-war/route proof.
class AICF_OrphanTripNativeConfirmation
{
	static ref array<string> s_aConfirmedOperations = {};
}

modded class AICF_MatchController
{
	protected bool m_bOrphanProbeStarted;
	protected bool m_bOrphanProbeKilled;
	protected bool m_bOrphanProbeDone;
	protected bool m_bOrphanProbeBefore;
	protected bool m_bOrphanProbeDestroyVehicle;
	protected bool m_bOrphanProbeVehicleKilled;
	protected int m_iOrphanProbeStart;
	protected int m_iOrphanProbeKillAt;
	protected EntityID m_OrphanProbeGroupId;
	protected EntityID m_OrphanProbeCarId;
	protected string m_sOrphanProbeRpl;
	protected ref AICF_TransportTrip m_OrphanProbeTrip;
	protected ref AICF_VehicleLease m_OrphanProbeLease;
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aOrphanProbeBuildings = {};
	protected ref array<EntityID> m_aOrphanProbeOriginalIds = {};

	protected bool AICF_OrphanProbeEnabled()
	{
		string value;
		return System.GetCLIParam("aicfOrphanTripProbe", value) && value == "1";
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		if (!m_bRosterReady || m_bOrphanProbeStarted || !AICF_OrphanProbeEnabled()) return;
		m_bOrphanProbeStarted = true;
		m_iOrphanProbeStart = System.GetTickCount();
		string before;
		m_bOrphanProbeBefore = System.GetCLIParam("aicfOrphanExpectStuck", before) && before == "1";
		string destructive;
		m_bOrphanProbeDestroyVehicle = System.GetCLIParam("aicfOrphanDestroyVehicle", destructive) && destructive == "1";
		m_Construction.Stop();
		SCR_CampaignMilitaryBaseComponent base = m_USSRFaction.GetMainBase();
		vector position = base.GetMasterProvider().GetOwner().GetOrigin() + "50 0 30";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab("USSR", AICF_EConstructionType.SMALL_BARRACKS);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (entity)
		{
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
			if (affiliation) affiliation.SetAffiliatedFaction(m_USSRFaction);
			SCR_CampaignBuildingCompositionComponent building = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (building) { building.SetProviderEntity(base.GetMasterProvider().GetOwner()); m_aOrphanProbeBuildings.Insert(building); }
		}
		Print(string.Format("[AICF][ORPHAN_TRIP_START] classification=CONTROLLED_ALL_CREW_DEATH fixture_barracks=1 fixture_supplies=1 no_heal=1 no_teleport=1 destroy_vehicle=%1", m_bOrphanProbeDestroyVehicle));
		GetGame().GetCallqueue().CallLater(AICF_OrphanProbeTick, 250, true);
	}

	override protected void AICF_VehicleSoakFail(string reason)
	{
		if (AICF_OrphanProbeEnabled() && m_bOrphanProbeKilled &&
			(reason == "GROUP_UNAVAILABLE" || reason == "GROUP_REPLACED_BEFORE_ARRIVAL" || reason == "ROUTE_NOT_COMPLETE_AT_DURATION")) return;
		super.AICF_VehicleSoakFail(reason);
	}

	override protected void AICF_VehicleSoakLog(string eventName, string details)
	{
		super.AICF_VehicleSoakLog(eventName, details);
		if (!AICF_OrphanProbeEnabled() || eventName != "BOARDED" || !m_VehicleSoakObservedTrip || !m_VehicleSoakObservedTrip.GetLease()) return;
		m_OrphanProbeTrip = m_VehicleSoakObservedTrip;
		m_OrphanProbeLease = m_OrphanProbeTrip.GetLease();
		m_OrphanProbeGroupId = m_OrphanProbeTrip.GetAssignment().GetGroup().GetID();
		m_OrphanProbeCarId = m_OrphanProbeLease.GetEntityId();
		m_sOrphanProbeRpl = m_OrphanProbeLease.GetRplId();
	}

	protected void AICF_OrphanProbeFinish(bool passed, string reason)
	{
		if (m_bOrphanProbeDone) return;
		m_bOrphanProbeDone = true;
		Print(string.Format("[AICF][ORPHAN_TRIP_DONE] passed=%1 reason=%2 before=%3 elapsed_since_kill_ms=%4", passed, reason, m_bOrphanProbeBefore, System.GetTickCount() - m_iOrphanProbeKillAt));
		GetGame().GetCallqueue().Remove(AICF_OrphanProbeTick);
		GetGame().RequestClose();
	}

	protected void AICF_OrphanProbeTick()
	{
		if (m_bOrphanProbeDone || m_bStopped) return;
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aOrphanProbeBuildings)
		{
			if (building && !building.IsCompositionSpawned() && building.GetCompositionLayout())
			{
				SCR_CampaignBuildingLayoutComponent layout = building.GetCompositionLayout();
				layout.AddBuildingValue(layout.GetToBuildValue());
			}
		}
		SCR_CampaignMilitaryBaseComponent base = m_USSRFaction.GetMainBase();
		if (base) base.AddSupplies(base.GetSuppliesMax() - base.GetSupplies());
		if (System.GetTickCount() - m_iOrphanProbeStart > 600000) { AICF_OrphanProbeFinish(false, "SETUP_OR_RELEASE_DEADLINE"); return; }
		if (!m_OrphanProbeTrip || !m_OrphanProbeLease) return;
		if (!m_bOrphanProbeKilled)
		{
			SCR_AIGroup group = m_OrphanProbeTrip.GetAssignment().GetGroup();
			array<AIAgent> agents = {};
			if (group) group.GetAgents(agents);
			if (agents.Count() != 5 || !m_OrphanProbeLease.IsCapActive()) { AICF_OrphanProbeFinish(false, "INITIAL_EXACT_FIVE_MISSING"); return; }
			m_bOrphanProbeKilled = true;
			m_iOrphanProbeKillAt = System.GetTickCount();
			Print(string.Format("[AICF][ORPHAN_TRIP_KILL] operation_id=%1 group=%2 vehicle=%3 rpl_id=%4 group_generation=%5 trip_generation=%6 lease_generation=%7 vehicle_generation=%8 actual=5", m_OrphanProbeTrip.GetOperationId(), m_OrphanProbeGroupId, m_OrphanProbeCarId, m_sOrphanProbeRpl, m_OrphanProbeTrip.GetGroupGeneration(), m_OrphanProbeTrip.GetTripGeneration(), m_OrphanProbeLease.GetLeaseGeneration(), m_OrphanProbeLease.GetVehicleGeneration()));
			foreach (AIAgent agent : agents)
			{
				ChimeraCharacter actor;
				if (agent) actor = ChimeraCharacter.Cast(agent.GetControlledEntity());
				SCR_CharacterDamageManagerComponent damage;
				if (actor) damage = SCR_CharacterDamageManagerComponent.Cast(actor.GetDamageManager());
				if (!damage) { AICF_OrphanProbeFinish(false, "EXACT_MEMBER_DAMAGE_MISSING"); return; }
				m_aOrphanProbeOriginalIds.Insert(actor.GetID());
				Print(string.Format("[AICF][ORPHAN_TRIP_MEMBER_KILL] member=%1 actor_node_rpl=%2 original_group=%3 action=NATIVE_KILL", actor.GetID(), Replication.FindItemId(actor).ToString(), m_OrphanProbeGroupId));
				damage.Kill(Instigator.CreateInstigatorGM());
			}
			return;
		}
		int elapsed = System.GetTickCount() - m_iOrphanProbeKillAt;
		if (m_bOrphanProbeDestroyVehicle && !m_bOrphanProbeVehicleKilled)
		{
			if (elapsed < 2000) return;
			for (int corpseIndex; corpseIndex < m_aOrphanProbeOriginalIds.Count(); corpseIndex++)
			{
				ChimeraCharacter corpse = ChimeraCharacter.Cast(GetGame().GetWorld().FindEntityByID(m_aOrphanProbeOriginalIds[corpseIndex]));
				if (!corpse || !corpse.GetCharacterController() || corpse.GetCharacterController().GetLifeState() != ECharacterLifeState.DEAD)
				{ AICF_OrphanProbeFinish(false, "NATIVE_ALL_DEAD_BEFORE_CAR_KILL_UNPROVEN"); return; }
			}
			Vehicle car = m_OrphanProbeLease.GetVehicle();
			SCR_DamageManagerComponent vehicleDamage = SCR_DamageManagerComponent.GetDamageManager(car);
			if (!car || !vehicleDamage || car.GetID() != m_OrphanProbeCarId) { AICF_OrphanProbeFinish(false, "EXACT_CAR_DAMAGE_MISSING"); return; }
			m_bOrphanProbeVehicleKilled = true;
			Print(string.Format("[AICF][ORPHAN_TRIP_CAR_KILL_REQUEST] operation_id=%1 vehicle=%2 rpl_id=%3 original_native_dead=5 action=NATIVE_KILL", m_OrphanProbeTrip.GetOperationId(), m_OrphanProbeCarId, m_sOrphanProbeRpl));
			vehicleDamage.Kill(Instigator.CreateInstigatorGM());
			string health = AICF_VehicleHealthTrace.Capture(car);
			string killedCar = string.Format("[AICF][ORPHAN_TRIP_CAR_KILL] operation_id=%1 vehicle=%2 rpl_id=%3 action=NATIVE_KILL ", m_OrphanProbeTrip.GetOperationId(), m_OrphanProbeCarId, m_sOrphanProbeRpl);
			Print(killedCar + health);
		}
		bool groupAbsent = !GetGame().GetWorld().FindEntityByID(m_OrphanProbeGroupId);
		AICF_GroupSlot slot = m_USSRState.GetSlot(0);
		int nextAlive;
		if (slot && slot.GetSpawnGeneration() == m_OrphanProbeTrip.GetGroupGeneration() + 1) nextAlive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
		int exactCount, wrongFaction, nonAlive;
		bool nextReady = slot && slot.IsCombatReady() && slot.GetSpawnGeneration() == m_OrphanProbeTrip.GetGroupGeneration() + 1 &&
			AICF_GroupRuntime.HasExactFactionRoster(slot.GetGroup(), "USSR", 5, exactCount, wrongFaction, nonAlive);
		if (elapsed % 5000 < 300)
			Print(string.Format("[AICF][ORPHAN_TRIP_READBACK] elapsed_ms=%1 group_absent=%2 trip_terminal=%3 cap_held=%4 vehicle_absent=%5 next_generation=%6 next_alive=%7 phase=%8", elapsed, groupAbsent, m_OrphanProbeTrip.IsTerminal(), m_OrphanProbeLease.IsCapActive(), !GetGame().GetWorld().FindEntityByID(m_OrphanProbeCarId), slot.GetSpawnGeneration(), nextAlive, AICF_Stage3Diagnostics.TripPhaseToString(m_OrphanProbeTrip.GetPhase())));
		if (m_bOrphanProbeBefore && elapsed >= 90000)
			AICF_OrphanProbeFinish(groupAbsent && !m_OrphanProbeTrip.IsTerminal() && m_OrphanProbeLease.IsCapActive() && nextAlive == 5, "EXPECTED_BASELINE_LEAK");
		bool requiredDisposition = !m_bOrphanProbeDestroyVehicle || !GetGame().GetWorld().FindEntityByID(m_OrphanProbeCarId);
		if (!m_bOrphanProbeBefore && groupAbsent && m_OrphanProbeTrip.IsTerminal() && !m_OrphanProbeLease.IsCapActive() && nextAlive == 5 && nextReady && requiredDisposition)
		{
			if (AICF_OrphanTripNativeConfirmation.s_aConfirmedOperations.Contains(m_OrphanProbeTrip.GetOperationId()))
				AICF_OrphanProbeFinish(true, "TERMINAL_RELEASE_AND_NEXT_GENERATION_READY");
		}
		if (elapsed >= 180000) AICF_OrphanProbeFinish(false, "ORPHAN_RELEASE_DEADLINE");
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_OrphanProbeTick);
		super.Stop(cleanupEntities);
		AICF_OrphanTripNativeConfirmation.s_aConfirmedOperations.Clear();
	}
}
