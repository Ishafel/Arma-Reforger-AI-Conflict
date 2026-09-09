// Test-only: отдельные catalog vehicles и fleet leases, без driver/planner.
// Проверяет production marker/read model, не доказывает доставку или client UI.
class AICF_LogisticsMapMarkerProbeSystem : AICF_LogisticsMapMarkerSystem
{
	int Count() { return m_aMarkers.Count(); }
	SCR_MapMarkerEntity First()
	{
		if (m_aMarkers.IsEmpty()) return null;
		return m_aMarkers[0].m_Marker;
	}
}

modded class SCR_GameModeCampaign
{
	protected int m_iAICFMarkerProbeChecks;
	protected int m_iAICFMarkerProbeFailures;
	protected ref array<ref AICF_FactionFleet> m_aAICFMarkerProbeFleets = {};
	protected ref array<Vehicle> m_aAICFMarkerProbeVehicles = {};

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfLogisticsMarkerProbe", enabled) || enabled != "1") return;
		GetGame().GetCallqueue().CallLater(AICF_RunMarkerProbe, 20000, false);
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_RunMarkerProbe);
		super.OnGameEnd();
	}

	protected void AICF_MarkerCheck(bool passed, string name)
	{
		m_iAICFMarkerProbeChecks++;
		if (!passed) m_iAICFMarkerProbeFailures++;
		Print(string.Format("[AICF][LOGISTICS_MARKER_PROBE_CASE] test_only=1 name=%1 passed=%2", name, passed));
	}

	protected AICF_LogisticsWorker AICF_MarkerWorker(string stable, int generation, int slotOffset = 0)
	{
		AICF_ContentProfile profile = AICF_ContentProfile.GetActive();
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(profile.GetRuntimeFactionKey(stable)));
		if (!faction || !faction.GetMainBase()) return null;
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE);
		if (!catalog) return null;
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		array<string> suffixes = {};
		profile.BuildLogisticsSuffixPreference(stable, suffixes);
		SCR_EntityCatalogEntry chosen;
		foreach (string suffix : suffixes)
		{
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				if (entry && entry.GetPrefab().EndsWith(suffix)) { chosen = entry; break; }
			}
			if (chosen) break;
		}
		if (!chosen) return null;
		AICF_LogisticsWorker w = new AICF_LogisticsWorker();
		w.m_Faction = faction;
		w.m_iSlot = AICF_LogisticsConfig.SERVICE_SLOT_FIRST + slotOffset;
		w.m_iGeneration = generation;
		w.m_Entry = chosen;
		w.m_Home = faction.GetMainBase();
		w.m_HomeId = w.m_Home.GetOwner().GetID();
		AICF_FactionFleet fleet = new AICF_FactionFleet(faction.GetFactionKey());
		m_aAICFMarkerProbeFleets.Insert(fleet);
		if (!fleet.TryReserveLogistics(w)) return null;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = w.m_Home.GetOwner().GetOrigin() + Vector(generation * 15, 2, 35);
		w.m_Vehicle = Vehicle.Cast(GetGame().SpawnEntityPrefab(Resource.Load(chosen.GetPrefab()), GetGame().GetWorld(), params));
		if (!w.m_Vehicle) return null;
		m_aAICFMarkerProbeVehicles.Insert(w.m_Vehicle);
		SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(w.m_Vehicle.FindComponent(SCR_FactionAffiliationComponent));
		if (!affiliation) return null;
		affiliation.SetAffiliatedFaction(faction);
		w.m_VehicleId = w.m_Vehicle.GetID();
		RplComponent rpl = RplComponent.Cast(w.m_Vehicle.FindComponent(RplComponent));
		if (!rpl || !rpl.IsMaster()) return null;
		w.m_sVehicleRpl = rpl.Id().ToString();
		if (!fleet.BindReservedLeaseVehicle(w.m_Lease, w.m_Vehicle, w.m_sVehicleRpl, chosen.GetPrefab(), AICF_EVehicleKind.TRANSPORT, 1, w.m_Vehicle.GetOrigin())) return null;
		AICF_LogisticsResourceAdapter resources = new AICF_LogisticsResourceAdapter();
		w.m_CargoPool = resources.Resolve(SCR_ResourceComponent.FindResourceComponent(w.m_Vehicle));
		w.m_bCustody = true;
		return w;
	}

	protected void AICF_RunMarkerProbe()
	{
		array<string> factions = {"US", "USSR"};
		foreach (string stable : factions) AICF_TestFactionMarkers(stable);
		Print(string.Format("[AICF][LOGISTICS_MARKER_PROBE_DONE] test_only=1 checks=%1 failures=%2 client_ui=NOT_RUN delivery=NOT_RUN", m_iAICFMarkerProbeChecks, m_iAICFMarkerProbeFailures));
		// Изолированная сессия завершается штатно; её test vehicles уходят с world.
		GetGame().RequestClose();
	}

	protected void AICF_TestFactionMarkers(string stable)
	{
		AICF_LogisticsWorker w = AICF_MarkerWorker(stable, 1);
		AICF_MarkerCheck(w && w.VehicleIdentity(), stable + "_REAL_LEASE_IDENTITY");
		if (!w) return;
		AICF_LogisticsMapMarkerProbeSystem markers = new AICF_LogisticsMapMarkerProbeSystem();
		array<ref AICF_LogisticsWorker> workers = {w};
		int now = System.GetTickCount();
		markers.Sync(workers, now);
		SCR_MapMarkerEntity marker = markers.First();
		AICF_MarkerCheck(markers.Count() == 1 && marker && marker.GetTarget() == w.m_Vehicle && marker.GetFaction() == w.m_Faction, stable + "_VEHICLE_TARGET_FACTION");
		if (!marker) { markers.Stop(); return; }
		AICF_MarkerCheck(marker.GetMarkerConfigID() % 10 == 2 && !marker.AICF_GetGroupMarkerText().IsEmpty() && marker.AICF_GetLogisticsDetails().Contains("Припасы:"), stable + "_SNAPSHOT");
		AICF_MarkerCheck(w.m_CargoPool && w.m_CargoPool.Valid() && !marker.AICF_GetLogisticsDetails().Contains("? / ?"), stable + "_LIVE_CARGO");
		Print(string.Format("[AICF][LOGISTICS_MARKER_PROBE_TEXT] test_only=1 faction=%1 details=%2", stable, marker.AICF_GetLogisticsDetails()));
		w.m_ePhase = AICF_ELogisticsPhase.LOADING;
		markers.Sync(workers, now + 1);
		AICF_MarkerCheck(!marker.AICF_GetGroupMarkerText().Contains("Погрузка"), stable + "_THROTTLE");
		now += 2000;
		markers.Sync(workers, now);
		AICF_MarkerCheck(markers.Count() == 1 && markers.First() == marker && marker.AICF_GetGroupMarkerText().Contains("Погрузка"), stable + "_UPDATE_WITHOUT_DUPLICATE");
		w.m_RouteRecovery = new AICF_LogisticsRouteRecovery();
		w.m_RouteRecovery.m_bActive = true;
		AICF_MarkerCheck(AICF_LogisticsMarkerText.Status(w).Contains("Выбирается"), stable + "_RECOVERY_STATUS");
		w.m_Job = new AICF_LogisticsJob();
		w.m_Job.m_iGeneration = w.m_iGeneration + 1;
		AICF_MarkerCheck(!AICF_LogisticsMarkerText.HasJob(w), stable + "_STALE_JOB");
		w.m_Job.m_iGeneration = w.m_iGeneration;
		w.m_Job.m_bCancelled = true;
		AICF_MarkerCheck(!AICF_LogisticsMarkerText.HasJob(w), stable + "_CANCELLED_JOB");
		w.m_bCustody = false;
		now += 2000;
		markers.Sync(workers, now);
		AICF_MarkerCheck(markers.Count() == 0, stable + "_RELEASE_REMOVAL");
		w.m_bCustody = true;
		now += 2000;
		markers.Sync(workers, now);
		AICF_MarkerCheck(markers.Count() == 1, stable + "_RESTORE");
		w.m_iGeneration++;
		now += 2000;
		markers.Sync(workers, now);
		AICF_MarkerCheck(markers.Count() == 0, stable + "_GENERATION_REJECTED");
		w.m_iGeneration--;
		now += 2000;
		markers.Sync(workers, now);
		AICF_LogisticsWorker second = AICF_MarkerWorker(stable, 1, 1);
		if (second)
		{
			workers.Insert(second);
			now += 2000;
			markers.Sync(workers, now);
			AICF_MarkerCheck(markers.Count() == 2, stable + "_MULTIPLE_VEHICLES");
			workers.Remove(1);
			now += 2000;
			markers.Sync(workers, now);
			AICF_MarkerCheck(markers.Count() == 1 && markers.First().GetTarget() == w.m_Vehicle, stable + "_REMOVED_WORKER");
		}
		else AICF_MarkerCheck(false, stable + "_SECOND_SETUP");
		AICF_LogisticsWorker replacement = AICF_MarkerWorker(stable, 2);
		if (replacement)
		{
			workers[0] = replacement;
			now += 2000;
			markers.Sync(workers, now);
			AICF_MarkerCheck(markers.Count() == 1 && markers.First().GetTarget() == replacement.m_Vehicle, stable + "_REPLACEMENT");
			replacement.m_bCleanupComplete = true;
			now += 2000;
			markers.Sync(workers, now);
			AICF_MarkerCheck(markers.Count() == 0, stable + "_CLEANUP_REMOVAL");
		}
		else AICF_MarkerCheck(false, stable + "_REPLACEMENT_SETUP");
		workers[0] = w;
		now += 2000;
		markers.Sync(workers, now);
		markers.Stop();
		AICF_MarkerCheck(markers.Count() == 0, stable + "_STOP");
		now += 2000;
		markers.Sync(workers, now);
		AICF_MarkerCheck(markers.Count() == 0, stable + "_NO_RECREATE_AFTER_STOP");
	}
}
