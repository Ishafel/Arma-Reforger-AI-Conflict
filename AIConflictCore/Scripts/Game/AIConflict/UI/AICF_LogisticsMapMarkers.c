// Только представление: не меняет worker, job, cargo, fleet или waypoint.
class AICF_LogisticsMarkerText
{
	static string Status(AICF_LogisticsWorker w)
	{
		if (w.m_bCargoFault || w.m_ePhase == AICF_ELogisticsPhase.FAILED_CLOSED) return "Неисправность";
		if (w.m_bCleanupQueued || w.m_ePhase == AICF_ELogisticsPhase.RETIRING) return "Завершает службу";
		if (AICF_LogisticsFallback.Cast(w.m_DriverInteraction)) return "Восстанавливает рейс";
		if (w.m_DriverInteraction) return "Водитель у препятствия";
		if (w.m_RouteRecovery && w.m_RouteRecovery.m_bActive) return "Выбирается из препятствия";
		switch (w.m_ePhase)
		{
			case AICF_ELogisticsPhase.SPAWN_PENDING: return "Подготовка машины";
			case AICF_ELogisticsPhase.DRIVER_READY: return "Готов к рейсу";
			case AICF_ELogisticsPhase.TO_SOURCE: return "Едет за грузом";
			case AICF_ELogisticsPhase.LOADING: return "Погрузка";
			case AICF_ELogisticsPhase.TO_DESTINATION: return "Доставляет груз";
			case AICF_ELogisticsPhase.UNLOADING: return "Разгрузка";
			case AICF_ELogisticsPhase.RETURN_HOME: return "Возвращается на базу";
			case AICF_ELogisticsPhase.WAIT_RETRY: return "Ожидает повторной попытки";
		}
		return "Ожидает задания";
	}

	static string VehicleName(AICF_LogisticsWorker w)
	{
		if (!w.m_Entry) return "Транспорт";
		string name = WidgetManager.Translate(w.m_Entry.GetEntityName());
		if (!name.IsEmpty()) return name;
		return FilePath.StripExtension(FilePath.StripPath(w.m_Entry.GetPrefab()));
	}

	static string BaseName(SCR_CampaignMilitaryBaseComponent base)
	{
		if (!base || !base.GetOwner()) return "Точка недоступна";
		string name = WidgetManager.Translate(base.GetBaseName());
		if (name.IsEmpty()) name = "База";
		return name;
	}

	static bool HasJob(AICF_LogisticsWorker w)
	{
		return w.m_Job && !w.m_Job.m_bCancelled && w.m_Job.m_iGeneration == w.m_iGeneration;
	}

	static string EndpointName(AICF_LogisticsEndpoint endpoint)
	{
		if (!endpoint || !endpoint.m_Base || !endpoint.m_Base.GetOwner() ||
			endpoint.m_Base.GetOwner().GetID() != endpoint.m_BaseId) return "Точка недоступна";
		return BaseName(endpoint.m_Base);
	}

	static string Route(AICF_LogisticsWorker w)
	{
		if (w.m_ePhase == AICF_ELogisticsPhase.RETURN_HOME) return "Возврат → " + BaseName(w.m_Home);
		if (!HasJob(w)) return "Нет активного рейса";
		if (w.m_Job.m_bReturn) return "Возврат груза → " + EndpointName(w.m_Job.m_Destination);
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
		label = string.Format("Л%1 · %2\n%3 · %4", number, name, status, cargo);
		details = string.Format("Логистика %1 · %2\nСостояние: %3\nПрипасы: %4\nМаршрут: %5",
			number, name, status, cargo, Route(w));
		string destination = "Нет активной цели";
		bool moving = w.m_ePhase == AICF_ELogisticsPhase.TO_SOURCE ||
			w.m_ePhase == AICF_ELogisticsPhase.TO_DESTINATION || w.m_ePhase == AICF_ELogisticsPhase.RETURN_HOME;
		if (moving && w.m_Vehicle)
			destination = string.Format("%1 м по прямой", Math.Round(vector.DistanceXZ(w.m_Vehicle.GetOrigin(), w.m_vEndpoint)));
		int speed;
		if (w.m_Vehicle && w.m_Vehicle.GetPhysics()) speed = Math.Round(w.m_Vehicle.GetPhysics().GetVelocity().Length() * 3.6);
		details += string.Format("\nДо текущей цели: %1\nСкорость: %2 км/ч\nБаза приписки: %3", destination, speed, BaseName(w.m_Home));
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
