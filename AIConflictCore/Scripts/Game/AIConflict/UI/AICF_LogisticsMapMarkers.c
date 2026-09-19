// Только представление: не меняет worker, job, cargo, fleet или waypoint.
class AICF_LogisticsMarkerText
{
	static string Status(AICF_LogisticsWorker w)
	{
		if (w.m_bCargoFault || w.m_ePhase == AICF_ELogisticsPhase.FAILED_CLOSED) return "{AICF:AICF_UI_Fault_f0741213}";
		if (w.m_bCleanupQueued || w.m_ePhase == AICF_ELogisticsPhase.RETIRING) return "{AICF:AICF_UI_Retiring_from_service_7ba75fff}";
		if (AICF_LogisticsFallback.Cast(w.m_DriverInteraction)) return "{AICF:AICF_UI_Recovering_delivery_765fb859}";
		if (w.m_DriverInteraction) return "{AICF:AICF_UI_Driver_at_obstacle_258fe07a}";
		if (w.m_RouteRecovery && w.m_RouteRecovery.m_bActive) return "{AICF:AICF_UI_Navigating_around_obstacle_2972c4e9}";
		switch (w.m_ePhase)
		{
			case AICF_ELogisticsPhase.SPAWN_PENDING: return "{AICF:AICF_UI_Preparing_vehicle_e634fded}";
			case AICF_ELogisticsPhase.DRIVER_READY: return "{AICF:AICF_UI_Ready_for_delivery_f04d67fa}";
			case AICF_ELogisticsPhase.TO_SOURCE: return "{AICF:AICF_UI_Driving_to_source_bf1b7d19}";
			case AICF_ELogisticsPhase.LOADING: return "{AICF:AICF_UI_Loading_0f66eaff}";
			case AICF_ELogisticsPhase.TO_DESTINATION: return "{AICF:AICF_UI_Delivering_cargo_0d134080}";
			case AICF_ELogisticsPhase.UNLOADING: return "{AICF:AICF_UI_Unloading_671156ed}";
			case AICF_ELogisticsPhase.RETURN_HOME: return "{AICF:AICF_UI_Returning_to_base_dacaa882}";
			case AICF_ELogisticsPhase.WAIT_RETRY: return "{AICF:AICF_UI_Waiting_to_retry_30e4795e}";
		}
		return "{AICF:AICF_UI_Awaiting_task_edef5a93}";
	}

	static string VehicleName(AICF_LogisticsWorker w)
	{
		if (!w.m_Entry) return "{AICF:AICF_UI_Vehicle_6a702f38}";
		string name = AICF_Localization.Key(w.m_Entry.GetEntityName());
		if (!name.IsEmpty()) return name;
		return FilePath.StripExtension(FilePath.StripPath(w.m_Entry.GetPrefab()));
	}

	static string BaseName(SCR_CampaignMilitaryBaseComponent base)
	{
		if (!base || !base.GetOwner()) return "{AICF:AICF_UI_Location_unavailable_7c455590}";
		string name = AICF_Localization.Key(base.GetBaseName());
		if (name.IsEmpty()) name = "{AICF:AICF_UI_Base_90760edb}";
		return name;
	}

	static bool HasJob(AICF_LogisticsWorker w)
	{
		return w.m_Job && !w.m_Job.m_bCancelled && w.m_Job.m_iGeneration == w.m_iGeneration;
	}

	static string EndpointName(AICF_LogisticsEndpoint endpoint)
	{
		if (!endpoint || !endpoint.m_Base || !endpoint.m_Base.GetOwner() ||
			endpoint.m_Base.GetOwner().GetID() != endpoint.m_BaseId) return "{AICF:AICF_UI_Location_unavailable_7c455590}";
		return BaseName(endpoint.m_Base);
	}

	static string Route(AICF_LogisticsWorker w)
	{
		if (w.m_ePhase == AICF_ELogisticsPhase.RETURN_HOME) return "{AICF:AICF_UI_Return_64cd0550}" + BaseName(w.m_Home);
		if (!HasJob(w)) return "{AICF:AICF_UI_No_active_delivery_1f9269e4}";
		if (w.m_Job.m_bReturn) return "{AICF:AICF_UI_Cargo_return_78c69b39}" + EndpointName(w.m_Job.m_Destination);
		return EndpointName(w.m_Job.m_Source) + " → " + EndpointName(w.m_Job.m_Destination);
	}

	static void Build(AICF_LogisticsWorker w, out string label, out string details)
	{
		int number = w.m_iSlot - AICF_LogisticsConfig.SERVICE_SLOT_FIRST + 1;
		string name = VehicleName(w);
		string status = Status(w);
		string cargo = "? / ?";
		// Не выдаём последнее наблюдение или prefab estimate за текущий груз.
		if (w.m_CargoPool && w.m_CargoPool.Valid())
			cargo = string.Format("%1 / %2", Math.Round(w.m_CargoPool.Value()), Math.Round(w.m_CargoPool.Capacity()));
		label = AICF_Localization.Format("{AICF:AICF_UI_L_e9f5a504}", string.Format("%1", number), string.Format("%1", name), string.Format("%1", status), string.Format("%1", cargo));
		details = AICF_Localization.Format("{AICF:AICF_UI_Logistics_Status_Supplies_Route_907d4ddf}",
			string.Format("%1", number), string.Format("%1", name), string.Format("%1", status), string.Format("%1", cargo), string.Format("%1", Route(w)));
		string destination = "{AICF:AICF_UI_No_active_destination_9787eb65}";
		bool moving = w.m_ePhase == AICF_ELogisticsPhase.TO_SOURCE ||
			w.m_ePhase == AICF_ELogisticsPhase.TO_DESTINATION || w.m_ePhase == AICF_ELogisticsPhase.RETURN_HOME;
		if (moving && w.m_Vehicle)
			destination = AICF_Localization.Format("{AICF:AICF_UI_m_straight_line_c03d1ee5}", string.Format("%1", Math.Round(vector.DistanceXZ(w.m_Vehicle.GetOrigin(), w.m_vEndpoint))));
		int speed;
		if (w.m_Vehicle && w.m_Vehicle.GetPhysics()) speed = Math.Round(w.m_Vehicle.GetPhysics().GetVelocity().Length() * 3.6);
		details += AICF_Localization.Format("{AICF:AICF_UI_To_current_destination_Speed_km_h_Home_bas_d954fdf8}", string.Format("%1", destination), string.Format("%1", speed), string.Format("%1", BaseName(w.m_Home)));
		string arrival = AICF_LogisticsArrivalEstimate.Text(w);
		if (!arrival.IsEmpty()) details += "\n" + arrival;
	}
}

// Снимок identity не хранит mutable worker: replacement в том же slot удаляет
// старый marker, даже если registry переиспользовала объект worker.
class AICF_LogisticsMapMarkerRecord
{
	Faction m_Faction;
	int m_iSlot;
	int m_iGeneration;
	EntityID m_VehicleId;
	string m_sVehicleRpl;
	SCR_MapMarkerEntity m_Marker;
	bool m_bSeen;

	bool Matches(AICF_LogisticsWorker w)
	{
		return m_Faction == w.m_Faction && m_iSlot == w.m_iSlot && m_iGeneration == w.m_iGeneration &&
			m_VehicleId == w.m_VehicleId && m_sVehicleRpl == w.m_sVehicleRpl &&
			m_Marker && m_Marker.GetTarget() == w.m_Vehicle;
	}
}

class AICF_LogisticsMapMarkerSystem
{
	static const int MARKER_KIND = 2;
	static const int UPDATE_INTERVAL_MS = 2000;
	protected ref array<ref AICF_LogisticsMapMarkerRecord> m_aMarkers = {};
	protected SCR_MapMarkerManagerComponent m_Manager;
	protected int m_iNextUpdateMs;
	protected bool m_bStopped;

	static bool Trackable(AICF_LogisticsWorker w)
	{
		if (!w || !w.m_Faction || w.m_bStopped || !w.m_bCustody || w.m_bCleanupComplete || !w.VehicleIdentity()) return false;
		if (SCR_Faction.GetEntityFaction(w.m_Vehicle) != w.m_Faction) return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(w.m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		return usage && usage.GetDamageState() != EDamageState.DESTROYED;
	}

	void Sync(array<ref AICF_LogisticsWorker> workers, int now)
	{
		if (m_bStopped || !Replication.IsServer() || now < m_iNextUpdateMs) return;
		m_iNextUpdateMs = now + UPDATE_INTERVAL_MS;
		if (!m_Manager) m_Manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!m_Manager) return;
		foreach (AICF_LogisticsMapMarkerRecord old : m_aMarkers) old.m_bSeen = false;
		foreach (AICF_LogisticsWorker w : workers)
		{
			if (!Trackable(w)) continue;
			AICF_LogisticsMapMarkerRecord record;
			for (int i = m_aMarkers.Count() - 1; i >= 0; i--)
			{
				AICF_LogisticsMapMarkerRecord candidate = m_aMarkers[i];
				if (candidate.m_Faction != w.m_Faction || candidate.m_iSlot != w.m_iSlot) continue;
				if (candidate.Matches(w)) record = candidate;
				else RemoveAt(i);
			}
			if (!record) record = Create(w);
			if (!record) continue;
			record.m_bSeen = true;
			string label, details;
			AICF_LogisticsMarkerText.Build(w, label, details);
			record.m_Marker.AICF_SetLogisticsMarkerData(label, details);
		}
		for (int stale = m_aMarkers.Count() - 1; stale >= 0; stale--)
		{
			if (!m_aMarkers[stale].m_bSeen) RemoveAt(stale);
		}
	}

	protected AICF_LogisticsMapMarkerRecord Create(AICF_LogisticsWorker w)
	{
		int packed = MARKER_KIND;
		string key = AICF_ContentProfile.GetActive().GetStableFactionKey(w.m_Faction.GetFactionKey());
		if (key == "USSR") packed += 100000;
		else if (key != "US") return null;
		SCR_MapMarkerEntity marker = m_Manager.InsertDynamicMarker(SCR_EMapMarkerType.DYNAMIC_EXAMPLE, w.m_Vehicle, packed);
		if (!marker) return null;
		// Stream rules назначаются до публикации, включая клиентов уже на сервере.
		marker.SetFaction(w.m_Faction);
		marker.SetGlobalVisible(true);
		AICF_LogisticsMapMarkerRecord record = new AICF_LogisticsMapMarkerRecord();
		record.m_Faction = w.m_Faction;
		record.m_iSlot = w.m_iSlot;
		record.m_iGeneration = w.m_iGeneration;
		record.m_VehicleId = w.m_VehicleId;
		record.m_sVehicleRpl = w.m_sVehicleRpl;
		record.m_Marker = marker;
		m_aMarkers.Insert(record);
		w.Log("LOGISTICS_MAP_MARKER_CREATED", "target=VEHICLE visibility=FACTION");
		return record;
	}

	protected void RemoveAt(int index)
	{
		AICF_LogisticsMapMarkerRecord record = m_aMarkers[index];
		if (m_Manager && record.m_Marker) m_Manager.RemoveDynamicMarker(record.m_Marker);
		m_aMarkers.Remove(index);
	}

	void Stop()
	{
		if (m_bStopped || !Replication.IsServer()) return;
		m_bStopped = true;
		for (int i = m_aMarkers.Count() - 1; i >= 0; i--) RemoveAt(i);
	}
}
