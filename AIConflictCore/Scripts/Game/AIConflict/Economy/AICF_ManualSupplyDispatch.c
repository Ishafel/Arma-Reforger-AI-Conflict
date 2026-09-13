// Заявка не является job или экономической транзакцией. До готовности машины
// сохраняется только намерение; перед Reserve повторно проверяются все данные.
class AICF_ManualSupplyRequest
{
	SCR_PlayerController m_Player;
	int m_iPlayer;
	int m_iRequest;
	int m_iGeneration;
	int m_iDeadlineMs;
	int m_iAmount;
	float m_fDeliveredBefore;
	AICF_LogisticsWorker m_Worker;
	ref AICF_LogisticsEndpoint m_Source;
	ref AICF_LogisticsEndpoint m_Destination;
	ref AICF_LogisticsJob m_Job;
}

class AICF_ManualSupplyDispatch
{
	protected AICF_LogisticsPlanner m_Planner;
	protected AICF_LogisticsDepotRegistry m_Registry;
	protected AICF_VehicleCoordinator m_Vehicles;
	protected ref array<ref AICF_ManualSupplyRequest> m_aRequests = {};
	protected int m_iNextAdmissionMs;
	protected bool m_bStopped;

	void AICF_ManualSupplyDispatch(AICF_LogisticsPlanner planner, AICF_LogisticsDepotRegistry registry, AICF_VehicleCoordinator vehicles)
	{
		m_Planner = planner;
		m_Registry = registry;
		m_Vehicles = vehicles;
	}

	static SCR_CampaignMilitaryBaseComponent ResolveBase(RplId id)
	{
		if (!id.IsValid()) return null;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if (!rpl || !rpl.GetEntity()) return null;
		return SCR_CampaignMilitaryBaseComponent.Cast(rpl.GetEntity().FindComponent(SCR_CampaignMilitaryBaseComponent));
	}

	bool OwnsWorker(AICF_LogisticsWorker worker)
	{
		foreach (AICF_ManualSupplyRequest request : m_aRequests)
		{
			if (request.m_Worker == worker && request.m_iGeneration == worker.m_iGeneration) return true;
		}
		return false;
	}

	protected bool PlayerValid(AICF_ManualSupplyRequest request)
	{
		return request.m_Player && GetGame().GetPlayerManager().GetPlayerController(request.m_iPlayer) == request.m_Player &&
			SCR_FactionManager.SGetPlayerFaction(request.m_iPlayer) == request.m_Worker.m_Faction;
	}

	protected bool EndpointsValid(AICF_LogisticsEndpoint source, AICF_LogisticsEndpoint destination, SCR_CampaignFaction faction, int amount, out string reason)
	{
		if (!AICF_LogisticsPlanner.OwnedSafe(source, faction) || !AICF_LogisticsPlanner.OwnedSafe(destination, faction))
		{
			reason = "Обе базы должны принадлежать вашей фракции и быть вне боя.";
			return false;
		}
		if (source == destination || source.m_Pool.Overlaps(destination.m_Pool))
		{
			reason = "Выберите другую базу назначения: склады совпадают.";
			return false;
		}
		int available = Math.Floor(m_Planner.ManualAvailable(source, faction));
		if (amount <= 0 || amount > available)
		{
			reason = string.Format("Недостаточно свободных припасов. С учётом резервов доступно: %1.", available);
			return false;
		}
		int free = Math.Floor(m_Planner.Need(destination, null, true));
		if (amount > free)
		{
			reason = string.Format("На базе назначения недостаточно места. Доступно: %1.", free);
			return false;
		}
		return true;
	}

	bool Submit(SCR_PlayerController player, int token, RplId sourceId, RplId destinationId, int amount, out string reason)
	{
		if (m_bStopped || !Replication.IsServer() || !player || token <= 0) return false;
		int playerId = player.GetPlayerId();
		if (GetGame().GetPlayerManager().GetPlayerController(playerId) != player) return false;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(SCR_FactionManager.SGetPlayerFaction(playerId));
		if (!faction) return false;
		foreach (AICF_ManualSupplyRequest previous : m_aRequests)
		{
			if (previous.m_Player == player)
			{
				reason = "Дождитесь завершения текущей перевозки.";
				return false;
			}
		}
		int now = System.GetTickCount();
		if (now < m_iNextAdmissionMs || m_aRequests.Count() >= 16)
		{
			reason = "Служба занята. Повторите запрос через несколько секунд.";
			return false;
		}
		m_iNextAdmissionMs = now + 500;
		AICF_LogisticsEndpoint source = m_Planner.Find(ResolveBase(sourceId));
		AICF_LogisticsEndpoint destination = m_Planner.Find(ResolveBase(destinationId));
		if (!EndpointsValid(source, destination, faction, amount, reason)) return false;
		AICF_LogisticsWorker selected;
		float bestDistance;
		float maximum;
		bool depotFound;
		foreach (AICF_LogisticsWorker w : m_Registry.m_aWorkers)
		{
			if (w.m_Faction != faction || !w.m_bEligible || w.m_bStopped || w.m_bCargoFault ||
				!AICF_LogisticsDepotRegistry.Live(w)) continue;
			depotFound = true;
			if (OwnsWorker(w) || w.m_Job || w.m_DriverInteraction || now < w.m_iRetryAtMs ||
				(w.m_bCleanupQueued && !w.m_bCleanupComplete)) continue;
			if (w.m_Lease && (!w.Ready() || w.m_fObservedCargo > 0)) continue;
			if (!w.m_Lease && (w.m_Group || w.m_Vehicle || w.m_bCustody)) continue;
			if (w.m_bCleanupComplete && now < w.m_iRetryAtMs + m_Planner.m_Config.m_iReplacementCooldownMs) continue;
			float capacity = w.m_fPrefabCapacity;
			if (w.m_Lease) capacity = w.m_CargoPool.Capacity();
			if (m_Planner.m_Config.m_fMaxCargoPerTrip > 0) capacity = Math.Min(capacity, m_Planner.m_Config.m_fMaxCargoPerTrip);
			maximum = Math.Max(maximum, capacity);
			if (amount > capacity) continue;
			vector origin = w.m_Depot.GetOrigin();
			if (w.m_Lease) origin = w.m_Vehicle.GetOrigin();
			float distance = vector.DistanceXZ(origin, source.m_Base.GetOwner().GetOrigin());
			if (selected && (distance > bestDistance || (distance == bestDistance && w.m_iSlot >= selected.m_iSlot))) continue;
			selected = w;
			bestDistance = distance;
		}
		if (!selected)
		{
			reason = "Нет свободной машины. Дождитесь завершения рейса или освобождения автопарка.";
			if (!depotFound) reason = "Нужен действующий союзный автопарк с грузовой машиной. Постройте его на своей базе.";
			else if (maximum > 0 && amount > maximum) reason = string.Format("За один рейс можно перевезти не более %1 припасов. Уменьшите количество.", Math.Floor(maximum));
			return false;
		}
		vector start = selected.m_Depot.GetOrigin();
		if (selected.m_Lease) start = selected.m_Vehicle.GetOrigin();
		vector load, unload;
		if (!m_Planner.Route(start, source, load) || !m_Planner.Route(load, destination, unload))
		{
			reason = "Не удалось проложить дорожный маршрут. Выберите другую базу или повторите позже.";
			return false;
		}
		if (!selected.m_Lease && !m_Vehicles.BeginLogisticsSpawn(selected))
		{
			reason = "Машину создать не удалось: достигнут лимит техники или AI. Повторите позже.";
			return false;
		}
		AICF_ManualSupplyRequest request = new AICF_ManualSupplyRequest();
		request.m_Player = player;
		request.m_iPlayer = playerId;
		request.m_iRequest = token;
		request.m_Worker = selected;
		request.m_iGeneration = selected.m_iGeneration;
		request.m_iDeadlineMs = now + AICF_LogisticsConfig.SPAWN_TIMEOUT_MS + 30000;
		request.m_Source = source;
		request.m_Destination = destination;
		request.m_iAmount = amount;
		request.m_fDeliveredBefore = selected.m_fDelivered;
		m_aRequests.Insert(request);
		player.AICF_SetSupplyRoute(source.m_Base.GetBaseName(), destination.m_Base.GetBaseName());
		player.AICF_SetSupplyStatus(token, true, string.Format("Принято: %1 припасов. Подготовка машины и водителя…", amount));
		selected.Log("LOGISTICS_MANUAL_ACCEPTED", string.Format("player=%1 request=%2 source=%3 destination=%4 amount=%5 automatic_dispatch=0", playerId, token, source.Key(), destination.Key(), amount));
		return true;
	}

	// Вызывается после vehicle/job tick, до автоматического возврата/idle cleanup.
	void Update(bool graphReady)
	{
		if (m_bStopped || !Replication.IsServer()) return;
		for (int i = m_aRequests.Count() - 1; i >= 0; i--)
		{
			AICF_ManualSupplyRequest request = m_aRequests[i];
			AICF_LogisticsWorker w = request.m_Worker;
			if (!w || w.m_iGeneration != request.m_iGeneration)
			{
				Finish(i, "Рейс отменён: машина была заменена.", false);
				continue;
			}
			if (!PlayerValid(request))
			{
				Finish(i, "Рейс отменён: игрок вышел или сменил фракцию.", true);
				continue;
			}
			if (request.m_Job)
			{
				if (w.m_Job != request.m_Job || w.m_bCleanupQueued || w.m_bCargoFault)
				{
					int delivered = Math.Floor(Math.Max(0, w.m_fDelivered - request.m_fDeliveredBefore));
					string result = string.Format("Рейс завершён. Доставлено: %1 / %2.", delivered, request.m_iAmount);
					if (delivered < request.m_iAmount) result = string.Format("Рейс прерван: изменились условия. Доставлено: %1 / %2. Сохранившаяся машина возвращает остаток.", delivered, request.m_iAmount);
					Finish(i, result, true);
				}
				else
				{
					string status = AICF_LogisticsMarkerText.Status(w);
					if (request.m_Job.m_bLoaded) status += string.Format(". Груз: %1 / %2.", Math.Floor(w.m_fObservedCargo), request.m_iAmount);
					string arrival = AICF_LogisticsArrivalEstimate.Text(w);
					if (!arrival.IsEmpty()) status += "\n" + arrival;
					request.m_Player.AICF_SetSupplyStatus(request.m_iRequest, true, status);
				}
				continue;
			}
			if (w.m_bCleanupQueued || w.m_bCargoFault || System.GetTickCount() >= request.m_iDeadlineMs)
			{
				Finish(i, "Рейс отменён: подготовить машину и водителя не удалось. Проверьте свободное место у автопарка.", true);
				continue;
			}
			if (!AICF_LogisticsPlanner.OwnedSafe(request.m_Source, w.m_Faction) || !AICF_LogisticsPlanner.OwnedSafe(request.m_Destination, w.m_Faction))
			{
				Finish(i, "Рейс отменён: источник или назначение потеряны либо находятся под атакой.", true);
				continue;
			}
			// Ready() может стать true между vehicle polls. Acquisition должен
			// сначала завершить SPAWN_PENDING, utility/site и readiness diagnostics.
			if (!graphReady || w.m_ePhase == AICF_ELogisticsPhase.SPAWN_PENDING || !w.Ready()) continue;
			string reason;
			if (!EndpointsValid(request.m_Source, request.m_Destination, w.m_Faction, request.m_iAmount, reason) ||
				!AICF_LogisticsDepotRegistry.Live(w))
			{
				if (reason.IsEmpty()) reason = "Автопарк больше недоступен.";
				Finish(i, reason, true);
				continue;
			}
			if (w.m_fObservedCargo > 0 || request.m_iAmount > w.m_CargoPool.Capacity())
			{
				Finish(i, "Грузовой отсек занят или его вместимости недостаточно.", true);
				continue;
			}
			vector load, unload;
			if (!m_Planner.Route(w.m_Vehicle.GetOrigin(), request.m_Source, load) || !m_Planner.Route(load, request.m_Destination, unload)) continue;
			request.m_Source.m_vPosition = load;
			request.m_Destination.m_vPosition = unload;
			if (!m_Planner.m_Book.Reserve(w, request.m_Source, request.m_Destination, request.m_iAmount, false, m_Planner.m_Config, m_Planner.m_Graph.GetRevision()))
			{
				Finish(i, "Запас или свободное место уже заняты другой перевозкой. Повторите запрос.", true);
				continue;
			}
			request.m_Job = w.m_Job;
			request.m_Job.m_bManual = true;
			if (!m_Vehicles.BeginLogisticsLeg(w, load, AICF_ELogisticsPhase.TO_SOURCE))
				Finish(i, "Не удалось начать движение к источнику.", true);
		}
	}

	protected void Finish(int index, string status, bool cancel)
	{
		AICF_ManualSupplyRequest request = m_aRequests[index];
		AICF_LogisticsWorker w = request.m_Worker;
		if (w && w.m_iGeneration == request.m_iGeneration)
		{
			if (cancel && request.m_Job && w.m_Job == request.m_Job)
			{
				m_Planner.m_Book.Cancel(w);
				m_Vehicles.CancelLogisticsLeg(w, "MANUAL_REQUEST_CANCELLED");
			}
			if (cancel && !request.m_Job && w.m_Lease && !w.Ready())
				m_Vehicles.RetireLogistics(w, m_Planner.m_Book, "MANUAL_PREPARATION_CANCELLED");
			w.Log("LOGISTICS_MANUAL_FINISHED", string.Format("player=%1 request=%2 requested=%3 delivered=%4", request.m_iPlayer, request.m_iRequest, request.m_iAmount, w.m_fDelivered - request.m_fDeliveredBefore));
		}
		if (request.m_Player) request.m_Player.AICF_SetSupplyStatus(request.m_iRequest, false, status);
		m_aRequests.Remove(index);
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		for (int i = m_aRequests.Count() - 1; i >= 0; i--) Finish(i, "Служба снабжения остановлена.", true);
	}
}
