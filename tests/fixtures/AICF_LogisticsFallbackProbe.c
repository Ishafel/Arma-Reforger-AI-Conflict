// Только временный source runtime. Один воспроизводимый отказ на сторону после
// реальной погрузки: остановка маршрута и выход exact AI. Recovery исполняет Core.
modded class AICF_TransportTripController
{
	protected ref array<string> m_aAICFFallbackInjected = {};

	override void TickLogisticsVehicle(AICF_LogisticsWorker w, AICF_LogisticsConfig config, AICF_LogisticsLedger book, bool graphReady)
	{
		if (AICF_TryInjectFallback(w, graphReady)) return;
		super.TickLogisticsVehicle(w, config, book, graphReady);
	}

	protected bool AICF_TryInjectFallback(AICF_LogisticsWorker w, bool graphReady)
	{
		string flag;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfLogisticsProbeFallback", flag) || flag != "1") return false;
		if (!graphReady || !w || !w.Ready() || w.m_DriverInteraction || w.HasForeignOccupant()) return false;
		if (w.m_ePhase != AICF_ELogisticsPhase.TO_DESTINATION || !w.m_Job || !w.m_Job.m_bLoaded) return false;
		if (
			w.m_fObservedCargo > 50 && w.m_CargoPool && w.m_CargoPool.Valid() && w.m_RouteRecovery &&
			!m_aAICFFallbackInjected.Contains(w.m_Faction.GetFactionKey()) &&
			vector.DistanceXZ(w.m_Vehicle.GetOrigin(), w.m_vEndpoint) > 150)
		{
			Physics physics = w.m_Vehicle.GetPhysics();
			if (physics && physics.GetVelocity().Length() <= 1 &&
				AICF_LogisticsFallback.PlayersClear(w.m_Vehicle.GetOrigin(), w.m_Vehicle.GetOrigin()))
			{
				m_aAICFFallbackInjected.Insert(w.m_Faction.GetFactionKey());
				bool started = BeginLogisticsFallback(w, "BOUNDED_ROUTE_RECOVERY_EXHAUSTED");
				bool issued;
				if (started)
				{
					CompartmentAccessComponent access = w.m_Driver.GetCompartmentAccessComponent();
					issued = access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.INVALID, true);
				}
				w.Log("LOGISTICS_FALLBACK_PROBE_INJECTED", string.Format("reason=TEST_LOADED_DRIVER_EXIT test_only=1 started=%1 exit_issued=%2 cargo=%3 position=%4", started, issued, w.m_CargoPool.Value(), w.m_Vehicle.GetOrigin()));
				return started;
			}
		}
		return false;
	}
}
