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
			reason = "{AICF:AICF_UI_Both_bases_must_belong_to_your_faction_and_23444b3f}";
			return false;
		}
		if (source == destination || source.m_Pool.Overlaps(destination.m_Pool))
		{
			reason = "{AICF:AICF_UI_Choose_another_destination_both_warehouses_c9a02379}";
			return false;
		}
		int available = Math.Floor(m_Planner.ManualAvailable(source, faction));
		if (amount <= 0 || amount > available)
		{
			reason = AICF_Localization.Format("{AICF:AICF_UI_Insufficient_unreserved_supplies_Available_d46fd657}", string.Format("%1", available));
			return false;
		}
		int free = Math.Floor(m_Planner.Need(destination, null, true));
		if (amount > free)
		{
			reason = AICF_Localization.Format("{AICF:AICF_UI_Insufficient_space_at_the_destination_Avai_0fea49df}", string.Format("%1", free));
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
			if (previous.m_Player == player && previous.m_iRequest == token)
			{
				reason = "{AICF:AICF_UI_This_request_has_already_been_accepted_c7655d41}";
				return false;
			}
		}
		int now = System.GetTickCount();
		if (now < m_iNextAdmissionMs)
		{
			reason = "{AICF:AICF_UI_Service_is_busy_Try_again_in_a_few_seconds_83680ad1}";
			return false;
		}
		m_iNextAdmissionMs = now + 500;
		AICF_LogisticsEndpoint source = m_Planner.Find(ResolveBase(sourceId));
		AICF_LogisticsEndpoint destination = m_Planner.Find(ResolveBase(destinationId));
		if (!EndpointsValid(source, destination, faction, amount, reason)) return false;
		AICF_LogisticsWorker selected;
		AICF_LogisticsWorker expansionDepot;
		float bestDistance;
		float bestDepotDistance;
		float maximum;
		bool depotFound;
		foreach (AICF_LogisticsWorker w : m_Registry.m_aWorkers)
		{
			if (w.m_Faction != faction || !w.m_bEligible || w.m_bStopped ||
				!AICF_LogisticsDepotRegistry.Live(w)) continue;
			depotFound = true;
			float newCapacity = w.m_fPrefabCapacity;
			if (m_Planner.m_Config.m_fMaxCargoPerTrip > 0) newCapacity = Math.Min(newCapacity, m_Planner.m_Config.m_fMaxCargoPerTrip);
			maximum = Math.Max(maximum, newCapacity);
			float depotDistance = vector.DistanceXZ(w.m_Depot.GetOrigin(), source.m_Base.GetOwner().GetOrigin());
			if (amount <= newCapacity && (!expansionDepot || depotDistance < bestDepotDistance ||
				(depotDistance == bestDepotDistance && w.m_iSlot < expansionDepot.m_iSlot)))
			{
				expansionDepot = w;
				bestDepotDistance = depotDistance;
			}
			if (w.m_bCargoFault || OwnsWorker(w) || w.m_Job || w.m_DriverInteraction || now < w.m_iRetryAtMs ||
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
		bool addWorker = !selected;
		if (addWorker) selected = expansionDepot;
		if (!selected)
		{
			reason = "{AICF:AICF_UI_The_depot_has_no_suitable_cargo_vehicle_fo_281befcb}";
			if (!depotFound) reason = "{AICF:AICF_UI_An_active_allied_depot_with_a_cargo_vehicl_c76154ed}";
			else if (maximum > 0 && amount > maximum) reason = AICF_Localization.Format("{AICF:AICF_UI_One_trip_can_carry_at_most_supplies_Reduce_babe0465}", string.Format("%1", Math.Floor(maximum)));
			return false;
		}
		vector start = selected.m_Depot.GetOrigin();
		if (!addWorker && selected.m_Lease) start = selected.m_Vehicle.GetOrigin();
		vector load, unload;
		if (!m_Planner.Route(start, source, load) || !m_Planner.Route(load, destination, unload))
		{
			reason = "{AICF:AICF_UI_Unable_to_find_a_road_route_Choose_another_fbf12d5a}";
			return false;
		}
		if (addWorker)
		{
			selected = m_Registry.AddManualWorker(selected);
			if (!selected)
			{
				reason = "{AICF:AICF_UI_Vehicle_depot_is_no_longer_available_Submi_e5657f6f}";
				return false;
			}
		}
		if (!selected.m_Lease && !m_Vehicles.BeginLogisticsSpawn(selected))
		{
			reason = "{AICF:AICF_UI_Unable_to_assign_a_driver_check_the_AI_bud_feda7882}";
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
		player.AICF_SetSupplyStatus(token, true, AICF_Localization.Format("{AICF:AICF_UI_Accepted_supplies_Preparing_vehicle_and_dr_213267a2}", string.Format("%1", amount)));
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
				Finish(i, "{AICF:AICF_UI_Delivery_canceled_vehicle_was_replaced_3338fd9d}", false);
				continue;
			}
			if (!PlayerValid(request))
			{
				Finish(i, "{AICF:AICF_UI_Delivery_canceled_player_disconnected_or_c_7a9689a1}", true);
				continue;
			}
			if (request.m_Job)
			{
				if (w.m_Job != request.m_Job || w.m_bCleanupQueued || w.m_bCargoFault)
				{
					int delivered = Math.Floor(Math.Max(0, w.m_fDelivered - request.m_fDeliveredBefore));
					string result = AICF_Localization.Format("{AICF:AICF_UI_Delivery_complete_Delivered_a0937734}", string.Format("%1", delivered), string.Format("%1", request.m_iAmount));
					if (delivered < request.m_iAmount) result = AICF_Localization.Format("{AICF:AICF_UI_Delivery_interrupted_conditions_changed_De_560a4363}", string.Format("%1", delivered), string.Format("%1", request.m_iAmount));
					Finish(i, result, true);
				}
				else
				{
					string status = AICF_LogisticsMarkerText.Status(w);
					if (request.m_Job.m_bLoaded) status += AICF_Localization.Format("{AICF:AICF_UI_Cargo_073dba54}", string.Format("%1", Math.Floor(w.m_fObservedCargo)), string.Format("%1", request.m_iAmount));
					string arrival = AICF_LogisticsArrivalEstimate.Text(w);
					if (!arrival.IsEmpty()) status += "\n" + arrival;
					request.m_Player.AICF_SetSupplyStatus(request.m_iRequest, true, status);
				}
				continue;
			}
			if (w.m_bCleanupQueued || w.m_bCargoFault || System.GetTickCount() >= request.m_iDeadlineMs)
			{
				Finish(i, "{AICF:AICF_UI_Delivery_canceled_unable_to_prepare_vehicl_200d522b}", true);
				continue;
			}
			if (!AICF_LogisticsPlanner.OwnedSafe(request.m_Source, w.m_Faction) || !AICF_LogisticsPlanner.OwnedSafe(request.m_Destination, w.m_Faction))
			{
				Finish(i, "{AICF:AICF_UI_Delivery_canceled_source_or_destination_wa_54f5ca73}", true);
				continue;
			}
			// Ready() может стать true между vehicle polls. Acquisition должен
			// сначала завершить SPAWN_PENDING, utility/site и readiness diagnostics.
			if (!graphReady || w.m_ePhase == AICF_ELogisticsPhase.SPAWN_PENDING || !w.Ready()) continue;
			string reason;
			if (!EndpointsValid(request.m_Source, request.m_Destination, w.m_Faction, request.m_iAmount, reason) ||
				!AICF_LogisticsDepotRegistry.Live(w))
			{
				if (reason.IsEmpty()) reason = "{AICF:AICF_UI_Vehicle_depot_is_no_longer_available_65f46dde}";
				Finish(i, reason, true);
				continue;
			}
			if (w.m_fObservedCargo > 0 || request.m_iAmount > w.m_CargoPool.Capacity())
			{
				Finish(i, "{AICF:AICF_UI_Cargo_space_is_occupied_or_insufficient_e3dffc53}", true);
				continue;
			}
			vector load, unload;
			if (!m_Planner.Route(w.m_Vehicle.GetOrigin(), request.m_Source, load) || !m_Planner.Route(load, request.m_Destination, unload)) continue;
			request.m_Source.m_vPosition = load;
			request.m_Destination.m_vPosition = unload;
			if (!m_Planner.m_Book.Reserve(w, request.m_Source, request.m_Destination, request.m_iAmount, false, m_Planner.m_Config, m_Planner.m_Graph.GetRevision()))
			{
				Finish(i, "{AICF:AICF_UI_Supplies_or_cargo_space_are_reserved_by_an_d0695a2e}", true);
				continue;
			}
			request.m_Job = w.m_Job;
			request.m_Job.m_bManual = true;
			if (!m_Vehicles.BeginLogisticsLeg(w, load, AICF_ELogisticsPhase.TO_SOURCE))
				Finish(i, "{AICF:AICF_UI_Unable_to_start_moving_to_the_source_59c95a5f}", true);
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
		for (int i = m_aRequests.Count() - 1; i >= 0; i--) Finish(i, "{AICF:AICF_UI_Supply_service_has_stopped_a7d87db2}", true);
	}
}
