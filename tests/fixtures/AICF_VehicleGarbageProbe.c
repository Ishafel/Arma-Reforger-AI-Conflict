// Controlled native GC TTL, isolated stage only. No historical GC-cause claim.
class AICF_VehicleGarbageProbeState
{
	static int s_iChecks;
	static int s_iFailures;
	static int s_iDeleteVetoes;
	static int s_iOwnedInsertVetoes;
	static int s_iPrimaryNativeInsert;
	static int s_iTeardownNativeInsert;
	static bool s_bPreleaseArmed;
	static Vehicle s_Primary;
	static Vehicle s_Teardown;
	static int s_iStockInserts;
	static int s_iStockDeletes;
	static int s_iReleasedStockCommits;
	static Vehicle s_Released;
	static Vehicle s_Control;
	static bool Enabled()
	{
		string flag;
		return System.GetCLIParam("aicfVehicleGarbageProbe", flag) && flag == "1";
	}
	static void Check(string name, bool passed, string details = "")
	{
		s_iChecks++;
		if (!passed) s_iFailures++;
		Print(string.Format("[AICF][VEHICLE_GC_CASE] case=%1 passed=%2 details=[%3]", name, passed, details));
	}
}

modded class AICF_VehicleGarbagePermission
{
	override void RecordVeto(string phase)
	{
		if (AICF_VehicleGarbageProbeState.Enabled() &&
			(m_Vehicle == AICF_VehicleGarbageProbeState.s_Primary || m_Vehicle == AICF_VehicleGarbageProbeState.s_Teardown))
		{
			if (phase == "DELETE") AICF_VehicleGarbageProbeState.s_iDeleteVetoes++;
			if (phase == "INSERT" && m_Vehicle == AICF_VehicleGarbageProbeState.s_Primary) AICF_VehicleGarbageProbeState.s_iOwnedInsertVetoes++;
		}
		super.RecordVeto(phase);
	}
	bool AICF_GCProbeStale(int kind)
	{
		if (kind == 0) m_iGroup++;
		if (kind == 1) m_iTrip++;
		if (kind == 2) m_iLease++;
		if (kind == 3) m_iVehicle++;
		bool rejected = !CanProtect(m_Vehicle);
		if (kind == 0) m_iGroup--;
		if (kind == 1) m_iTrip--;
		if (kind == 2) m_iLease--;
		if (kind == 3) m_iVehicle--;
		return rejected && CanProtect(m_Vehicle);
	}
}

modded class AICF_VehicleLease
{
	AICF_VehicleGarbagePermission AICF_GCProbePermission() { return m_GarbagePermission; }
}

modded class SCR_GarbageSystem
{
	override protected float OnInsertRequested(IEntity entity, float lifetime)
	{
		if (AICF_VehicleGarbageProbeState.Enabled() &&
			(entity == AICF_VehicleGarbageProbeState.s_Released || entity == AICF_VehicleGarbageProbeState.s_Control) &&
			!AICF_VehicleGarbagePolicy.Find(entity)) AICF_VehicleGarbageProbeState.s_iStockInserts++;
		float accepted = super.OnInsertRequested(entity, lifetime);
		if (AICF_VehicleGarbageProbeState.Enabled() && entity == AICF_VehicleGarbageProbeState.s_Released && accepted > 0 &&
			!AICF_VehicleGarbagePolicy.Find(entity)) AICF_VehicleGarbageProbeState.s_iReleasedStockCommits++;
		if (AICF_VehicleGarbageProbeState.Enabled() && AICF_VehicleGarbageProbeState.s_bPreleaseArmed && lifetime == 5 && accepted > 0)
		{
			if (entity == AICF_VehicleGarbageProbeState.s_Primary) AICF_VehicleGarbageProbeState.s_iPrimaryNativeInsert++;
			else if (entity == AICF_VehicleGarbageProbeState.s_Teardown) AICF_VehicleGarbageProbeState.s_iTeardownNativeInsert++;
			else return accepted;
			Print(string.Format("[AICF][VEHICLE_GC_NATIVE_INSERT] entity=%1 requested_ttl_s=%2 stock_ttl_s=%3 permission_current=%4",
				entity.GetID(), lifetime, accepted, AICF_VehicleGarbagePolicy.Find(entity) != null));
		}
		return accepted;
	}
	override protected bool OnBeforeDelete(IEntity entity)
	{
		if (AICF_VehicleGarbageProbeState.Enabled() &&
			(entity == AICF_VehicleGarbageProbeState.s_Released || entity == AICF_VehicleGarbageProbeState.s_Control) &&
			!AICF_VehicleGarbagePolicy.Find(entity)) AICF_VehicleGarbageProbeState.s_iStockDeletes++;
		return super.OnBeforeDelete(entity);
	}
}

modded class AICF_MatchController
{
	protected int m_iGCStarted;
	protected int m_iGCPhase;
	protected int m_iGCPhaseAt;
	protected int m_iGCHeartbeatAt;
	protected int m_iGCOwnedInsertBaseline;
	protected bool m_bGCFinished;
	protected ref AICF_FactionFleet m_GCPrimaryFleet;
	protected ref AICF_FactionFleet m_GCTeardownFleet;
	protected AICF_VehicleLease m_GCPrimaryLease;
	protected AICF_VehicleLease m_GCTeardownLease;
	protected Vehicle m_GCPrimary;
	protected Vehicle m_GCTeardown;
	protected Vehicle m_GCControl;
	protected Vehicle m_GCNoAuthority;
	protected EntityID m_GCPrimaryId;
	protected EntityID m_GCTeardownId;
	protected EntityID m_GCControlId;

	override protected void Update()
	{
		if (!AICF_VehicleGarbageProbeState.Enabled() || !m_bRosterReady || m_bStopped)
		{
			super.Update(); return;
		}
		int now = System.GetTickCount();
		if (!m_iGCStarted)
		{
			m_iGCStarted = now;
			m_Construction.Stop();
			m_GCPrimaryFleet = new AICF_FactionFleet("US", 10);
			m_GCTeardownFleet = new AICF_FactionFleet("US", 10);
			m_GCPrimary = AICF_GCSpawn(0);
			m_GCTeardown = AICF_GCSpawn(1);
			if (!m_GCPrimary || !m_GCTeardown) { AICF_VehicleGarbageProbeState.Check("VEHICLE_SETUP", false); AICF_GCFinish(); return; }
			m_GCPrimaryId = m_GCPrimary.GetID(); m_GCTeardownId = m_GCTeardown.GetID();
			AICF_VehicleGarbageProbeState.s_Primary = m_GCPrimary;
			AICF_VehicleGarbageProbeState.s_Teardown = m_GCTeardown;
			m_iGCPhase = -1;
		}
		if (m_iGCPhase == -1 && now - m_iGCStarted >= 2000)
		{
			SCR_GarbageSystem garbage = SCR_GarbageSystem.GetByEntityWorld(m_GCPrimary);
			if (!garbage) { AICF_VehicleGarbageProbeState.Check("NATIVE_GC_AVAILABLE", false); AICF_GCFinish(); return; }
			if (garbage.IsInserted(m_GCPrimary)) garbage.Withdraw(m_GCPrimary);
			if (garbage.IsInserted(m_GCTeardown)) garbage.Withdraw(m_GCTeardown);
			AICF_VehicleGarbageProbeState.s_bPreleaseArmed = true;
			bool first = garbage.Insert(m_GCPrimary, 5);
			bool second = garbage.Insert(m_GCTeardown, 5);
			if (!first || !second) { AICF_VehicleGarbageProbeState.Check("PRELEASE_NATIVE_INSERT_REQUEST", false); AICF_GCFinish(); return; }
			m_iGCPhase = -2; m_iGCPhaseAt = now;
		}
		if (m_iGCPhase == -2 && now > m_iGCPhaseAt && AICF_VehicleGarbageProbeState.s_iPrimaryNativeInsert > 0 && AICF_VehicleGarbageProbeState.s_iTeardownNativeInsert > 0)
		{
			SCR_GarbageSystem garbage = SCR_GarbageSystem.GetByEntityWorld(m_GCPrimary);
			AICF_VehicleGarbageProbeState.Check("PRELEASE_NATIVE_TTL_FIVE", garbage.IsInserted(m_GCPrimary) &&
				garbage.IsInserted(m_GCTeardown) && garbage.GetLifetime(m_GCPrimary) == 5 && garbage.GetLifetime(m_GCTeardown) == 5 &&
				garbage.GetRemainingLifetime(m_GCPrimary) > 1 && garbage.GetRemainingLifetime(m_GCTeardown) > 1);
			bool bound = AICF_GCBind(m_GCPrimaryFleet, m_GCPrimary, 0, m_GCPrimaryLease) && AICF_GCBind(m_GCTeardownFleet, m_GCTeardown, 1, m_GCTeardownLease);
			AICF_VehicleGarbageProbeState.Check("REAL_FLEET_ACTIVE_BIND", bound && m_GCPrimaryLease && m_GCTeardownLease &&
				m_GCPrimaryLease.GetState() == AICF_EVehicleLeaseState.ACTIVE && AICF_VehicleGarbagePolicy.Find(m_GCPrimary));
			if (!bound) { AICF_GCFinish(); return; }
			m_iGCPhase = 1;
		}
		if (now - m_iGCStarted >= 180000) { AICF_VehicleGarbageProbeState.Check("TIMEOUT", false); AICF_GCFinish(); return; }
		if (now >= m_iGCHeartbeatAt)
		{
			m_iGCHeartbeatAt = now + 5000;
			Print(string.Format("[AICF][VEHICLE_GC_HEARTBEAT] phase=%1 elapsed_ms=%2 delete_vetoes=%3 primary=[%4] teardown=[%5] control=[%6] primary_native_inserts=%7 teardown_native_inserts=%8",
				m_iGCPhase, now - m_iGCStarted, AICF_VehicleGarbageProbeState.s_iDeleteVetoes,
				AICF_VehicleGarbagePolicy.Describe(m_GCPrimary), AICF_VehicleGarbagePolicy.Describe(m_GCTeardown), AICF_VehicleGarbagePolicy.Describe(m_GCControl),
				AICF_VehicleGarbageProbeState.s_iPrimaryNativeInsert, AICF_VehicleGarbageProbeState.s_iTeardownNativeInsert));
		}
		if (m_iGCPhase == 1 && AICF_VehicleGarbageProbeState.s_iDeleteVetoes >= 2)
		{
			SCR_GarbageSystem garbage = SCR_GarbageSystem.GetByEntityWorld(m_GCPrimary);
			AICF_VehicleGarbagePermission permission = AICF_VehicleGarbagePolicy.Find(m_GCPrimary);
			AICF_VehicleGarbageProbeState.Check("OWNED_DELETE_VETO", AICF_VehicleGarbageProbeState.s_iDeleteVetoes == 2);
			AICF_VehicleGarbageProbeState.Check("OWNED_SURVIVES_NATIVE_TTL", m_GCPrimary && m_GCTeardown && m_GCPrimary.GetID() == m_GCPrimaryId && m_GCTeardown.GetID() == m_GCTeardownId && now - m_iGCStarted >= 1000);
			AICF_VehicleGarbageProbeState.Check("OWNED_TRACKING_ENDED", permission && permission.EndedTracking() && !garbage.IsInserted(m_GCPrimary));
			m_iGCOwnedInsertBaseline = AICF_VehicleGarbageProbeState.s_iOwnedInsertVetoes;
			garbage.Insert(m_GCPrimary, 1);
			m_iGCPhase = 4; m_iGCPhaseAt = now;
		}
		if (m_iGCPhase == 4 && now - m_iGCPhaseAt >= 1000)
		{
			SCR_GarbageSystem garbage = SCR_GarbageSystem.GetByEntityWorld(m_GCPrimary);
			AICF_VehicleGarbagePermission permission = AICF_VehicleGarbagePolicy.Find(m_GCPrimary);
			AICF_VehicleGarbageProbeState.Check("OWNED_INSERT_VETO", AICF_VehicleGarbageProbeState.s_iOwnedInsertVetoes > m_iGCOwnedInsertBaseline && !garbage.IsInserted(m_GCPrimary));
			if (!permission) { AICF_GCFinish(); return; }
			AICF_VehicleGarbageProbeState.Check("STALE_GROUP_GENERATION_REJECTED", permission.AICF_GCProbeStale(0));
			AICF_VehicleGarbageProbeState.Check("STALE_TRIP_GENERATION_REJECTED", permission.AICF_GCProbeStale(1));
			AICF_VehicleGarbageProbeState.Check("STALE_LEASE_GENERATION_REJECTED", permission.AICF_GCProbeStale(2));
			AICF_VehicleGarbageProbeState.Check("STALE_VEHICLE_GENERATION_REJECTED", permission.AICF_GCProbeStale(3));
			AICF_VehicleGarbageProbeState.Check("FAKE_ENTITY_REJECTED", !permission.CanProtect(m_GCTeardown));
			m_GCNoAuthority = Vehicle.Cast(GetGame().SpawnEntity(Vehicle, GetGame().GetWorld()));
			Print("[AICF][VEHICLE_GC_BARE_AUTHORITY_SETUP] " + AICF_GCAuthorityDescription(m_GCNoAuthority));
			if (m_GCNoAuthority) SCR_EntityHelper.DeleteEntityAndChildren(m_GCNoAuthority);
			EntitySpawnParams localParams = new EntitySpawnParams(); localParams.TransformMode = ETransformMode.WORLD;
			localParams.Transform[3] = m_GCPrimary.GetOrigin() + "0 0 15";
			Resource localResource = Resource.Load("{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et");
			bool canLocal = ChimeraGame.CanSpawnEntityPrefab(localResource, localParams);
			if (canLocal) m_GCNoAuthority = Vehicle.Cast(GetGame().SpawnEntityPrefabLocal(localResource, GetGame().GetWorld(), localParams));
			ref AICF_FactionFleet noAuthorityFleet = new AICF_FactionFleet("US", 10);
			AICF_VehicleLease noAuthorityLease;
			bool forgedBound = m_GCNoAuthority && AICF_GCBind(noAuthorityFleet, m_GCNoAuthority, 9, noAuthorityLease, "FORGED_NO_NATIVE_RPL");
			string authorityDetails = string.Format("can_local=%1 forged_bound=%2 lease_present=%3 ", canLocal, forgedBound, noAuthorityLease != null);
			authorityDetails += AICF_GCAuthorityDescription(m_GCNoAuthority);
			RplComponent localRpl;
			if (m_GCNoAuthority) localRpl = RplComponent.Cast(m_GCNoAuthority.FindComponent(RplComponent));
			AICF_VehicleGarbageProbeState.Check("LOCAL_NATIVE_AUTHORITY_READBACK", canLocal && forgedBound && noAuthorityLease &&
				noAuthorityLease.GetVehicle() == m_GCNoAuthority && !noAuthorityLease.AICF_GCProbePermission() && localRpl &&
				localRpl.IsMaster() && !localRpl.IsProxy() && AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_GCNoAuthority), authorityDetails);
			bool pending = m_GCPrimaryLease.BeginRelease(true, "GC_CONTROLLED_RELEASE", m_GCPrimary.GetOrigin(), now);
			AICF_VehicleGarbageProbeState.Check("RELEASE_PENDING_PROTECTED", pending && permission.CanProtect(m_GCPrimary));
			m_GCPrimaryLease.FailClosed();
			AICF_VehicleGarbageProbeState.Check("FAILED_CLOSED_PROTECTED", permission.CanProtect(m_GCPrimary));
			AICF_WorldPoolAsset released;
			AICF_VehicleGarbageProbeState.s_Released = m_GCPrimary;
			bool transferred = m_GCPrimaryFleet.ReleaseRetainedLeaseAt(m_GCPrimaryLease, true, "GC_CONTROLLED_POOL", m_GCPrimary.GetOrigin(), now, released);
			AICF_VehicleGarbageProbeState.Check("RELEASE_TO_WORLD_POOL", transferred && released && released.GetVehicle() == m_GCPrimary && m_GCPrimaryFleet.GetWorldPoolCount() == 1);
			AICF_VehicleGarbageProbeState.Check("RELEASED_NO_PERMISSION", !AICF_VehicleGarbagePolicy.Find(m_GCPrimary));
			// Drop the sole strong fleet owner. Lease and permission fields are weak.
			m_GCTeardownLease = null;
			m_GCTeardownFleet = null;
			m_iGCPhaseAt = now; m_iGCPhase = 2;
		}
		if (m_iGCPhase == 2 && now - m_iGCPhaseAt >= 1000 && AICF_VehicleGarbageProbeState.s_iReleasedStockCommits > 0)
		{
			SCR_GarbageSystem garbage = SCR_GarbageSystem.GetByEntityWorld(m_GCTeardown);
			AICF_VehicleGarbageProbeState.Check("RELEASED_NATIVE_TRACKING_RESTORED", garbage.IsInserted(m_GCPrimary) &&
				garbage.GetLifetime(m_GCPrimary) > 0 && garbage.GetRemainingLifetime(m_GCPrimary) > 0, AICF_VehicleGarbagePolicy.Describe(m_GCPrimary));
			AICF_VehicleGarbageProbeState.Check("LEASE_TEARDOWN_TRACKING_RESTORED", m_GCTeardown && !AICF_VehicleGarbagePolicy.Find(m_GCTeardown) && garbage.IsInserted(m_GCTeardown), AICF_VehicleGarbagePolicy.Describe(m_GCTeardown));
			if (garbage.IsInserted(m_GCPrimary)) garbage.Withdraw(m_GCPrimary);
			bool releasedTTL = garbage.Insert(m_GCPrimary, 1);
			m_GCControl = AICF_GCSpawn(2);
			AICF_VehicleGarbageProbeState.s_Control = m_GCControl;
			if (!m_GCControl) { AICF_VehicleGarbageProbeState.Check("UNOWNED_SETUP", false); AICF_GCFinish(); return; }
			m_GCControlId = m_GCControl.GetID();
			if (garbage.IsInserted(m_GCControl)) garbage.Withdraw(m_GCControl);
			bool unownedTTL = garbage.Insert(m_GCControl, 1);
			if (!releasedTTL || !unownedTTL) { AICF_VehicleGarbageProbeState.Check("CONTROL_TTL_SETUP", false); AICF_GCFinish(); return; }
			m_iGCPhaseAt = now; m_iGCPhase = 3;
		}
		if (m_iGCPhase == 3 && !m_GCPrimary && !m_GCControl && now - m_iGCPhaseAt >= 1000)
		{
			AICF_VehicleGarbageProbeState.Check("RELEASED_NATIVE_TTL_DELETE", !GetGame().GetWorld().FindEntityByID(m_GCPrimaryId));
			AICF_VehicleGarbageProbeState.Check("UNOWNED_NATIVE_TTL_DELETE", !GetGame().GetWorld().FindEntityByID(m_GCControlId));
			AICF_VehicleGarbageProbeState.Check("STOCK_CALLBACKS_PRESERVED", AICF_VehicleGarbageProbeState.s_iStockInserts >= 2 && AICF_VehicleGarbageProbeState.s_iStockDeletes == 2);
			AICF_GCFinish(); return;
		}
		super.Update();
	}

	protected Vehicle AICF_GCSpawn(int index)
	{
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(0).GetGroup());
		if (!leader) return null;
		vector position = leader.GetOrigin() + Vector(20 + index * 8, 0, 20);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams(); params.TransformMode = ETransformMode.WORLD; params.Transform[3] = position;
		return Vehicle.Cast(GetGame().SpawnEntityPrefab(Resource.Load("{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et"), GetGame().GetWorld(), params));
	}
	protected string AICF_GCAuthorityDescription(Vehicle vehicle)
	{
		if (!vehicle) return "native_present=0";
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		string details = string.Format("native_present=1 entity=%1 rpl_present=%2 native_authority=%3 component_rpl=%4",
			vehicle.GetID(), rpl != null, AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle), AICF_VehicleBehaviorBinding.ComponentRpl(vehicle));
		if (rpl) details += string.Format(" native_master=%1 native_proxy=%2 native_role=%3", rpl.IsMaster(), rpl.IsProxy(), rpl.Role());
		return details;
	}
	protected bool AICF_GCBind(AICF_FactionFleet fleet, Vehicle vehicle, int slotId, out AICF_VehicleLease lease, string forcedRpl = "")
	{
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		AICF_StrategicAssignmentSnapshot assignment = new AICF_StrategicAssignmentSnapshot("US", slotId, "GC" + slotId.ToString(),
			slot.GetSpawnGeneration(), slot.GetGroup(), slot.GetRole(), slot.GetUnitType(), "GC_CONTROLLED", AICF_EOrderTargetKind.POSITION,
			null, vehicle.GetOrigin(), slot.GetStrategicAssignmentRevision(), slot.GetStrategicIntentRevision(), 1);
		string rpl = forcedRpl;
		if (rpl.IsEmpty()) rpl = AICF_VehicleBehaviorBinding.ComponentRpl(vehicle);
		return fleet.TryReserveLease(assignment, 1000 + slotId, lease) && fleet.BindReservedLeaseVehicle(lease, vehicle,
			rpl, "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et", AICF_EVehicleKind.LIGHT_TRANSPORT, 5, vehicle.GetOrigin());
	}
	protected void AICF_GCFinish()
	{
		if (m_bGCFinished) return;
		m_bGCFinished = true;
		Print(string.Format("[AICF][VEHICLE_GC_FINISHED] checks=%1 failures=%2", AICF_VehicleGarbageProbeState.s_iChecks, AICF_VehicleGarbageProbeState.s_iFailures));
		Stop(true);
		GetGame().GetCallqueue().CallLater(AICF_GCClose, 1000, false);
	}
	protected void AICF_GCClose() { GetGame().GetCallqueue().Remove(AICF_GCClose); GetGame().RequestClose(); }
	override protected void Stop(bool cleanupEntities)
	{
		GetGame().GetCallqueue().Remove(AICF_GCClose);
		super.Stop(cleanupEntities);
		m_GCPrimaryLease = null; m_GCTeardownLease = null;
		m_GCPrimaryFleet = null; m_GCTeardownFleet = null;
		if (m_GCPrimary) SCR_EntityHelper.DeleteEntityAndChildren(m_GCPrimary);
		if (m_GCTeardown) SCR_EntityHelper.DeleteEntityAndChildren(m_GCTeardown);
		if (m_GCControl) SCR_EntityHelper.DeleteEntityAndChildren(m_GCControl);
		if (m_GCNoAuthority) SCR_EntityHelper.DeleteEntityAndChildren(m_GCNoAuthority);
	}
}
