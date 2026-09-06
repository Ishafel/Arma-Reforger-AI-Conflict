// Service lease использует тот же aggregate и cap, без infantry assignment.




class AICF_LogisticsAcquisitionFlow
{
	protected ref AICF_BaseBuilderSpawner m_Drivers = new AICF_BaseBuilderSpawner();
	protected ref AICF_VehicleSpawner m_Spawner = new AICF_VehicleSpawner();
	protected ref AICF_ManagedAILODPolicy m_LOD = new AICF_ManagedAILODPolicy();

	void CancelSite(AICF_LogisticsWorker w)
	{
		if (w.m_Site) m_Spawner.ReleaseSelectedSite(w.m_Site);
		w.m_Site = null;
	}

	AICF_TripOutcome Tick(AICF_LogisticsWorker w, AICF_LogisticsLedger book)
	{
		string token = string.Format("L%1_G%2", w.m_iSlot, w.m_iGeneration);
		int now = System.GetTickCount();
		if (!AICF_LogisticsDepotRegistry.Live(w) || now - w.m_iPhaseAtMs >= AICF_LogisticsConfig.SPAWN_TIMEOUT_MS)
			return AICF_TripOutcome.TerminalFailClosed("SPAWN_DEPOT_LOST_OR_TIMEOUT", token);
		if (!w.m_Vehicle)
		{
			if (!m_Spawner.ReserveLogisticsSite(w) || !m_Spawner.SpawnLogistics(w, book.m_Resources))
				return AICF_TripOutcome.TerminalFailClosed("EXACT_DEPOT_SPAWN_REJECTED", token);
			w.m_Group = m_Drivers.CreateBuilder(w.m_Faction, w.m_Vehicle.GetOrigin());
			if (!w.m_Group) return AICF_TripOutcome.TerminalFailClosed("DRIVER_CONTROLLER_FAILED", token);
			w.m_GroupId = w.m_Group.GetID();
			if (!m_Drivers.BeginRosterSpawn(w.m_Group, 1)) return AICF_TripOutcome.TerminalFailClosed("DRIVER_ROSTER_FAILED", token);
			w.Log("LOGISTICS_SPAWN_REQUESTED", "agents=1 cargo=0 tickets=0 supplies_cost=0");
			return AICF_TripOutcome.Wait("DRIVER_ROSTER_PENDING", token);
		}
		if (!w.VehicleIdentity() || !w.GroupIdentity()) return AICF_TripOutcome.TerminalFailClosed("SPAWN_IDENTITY_LOST", token);
		int actual, wrong, dead;
		if (!AICF_GroupRuntime.HasExactFactionRoster(w.m_Group, w.m_Faction.GetFactionKey(), 1, actual, wrong, dead))
		{
			if (actual > 1 || wrong > 0 || dead > 0) return AICF_TripOutcome.TerminalFailClosed("INVALID_SINGLE_DRIVER", token);
			return AICF_TripOutcome.Wait("DRIVER_ROSTER_PENDING", token);
		}
		if (!w.m_Driver)
		{
			w.m_Driver = ChimeraCharacter.Cast(AICF_GroupRuntime.ResolveAliveLeader(w.m_Group));
			if (!w.m_Driver) return AICF_TripOutcome.TerminalFailClosed("DRIVER_MISSING", token);
			w.m_DriverId = w.m_Driver.GetID();
		}
		if (!w.DriverIdentity()) return AICF_TripOutcome.TerminalFailClosed("DRIVER_AUTHORITY_LOST", token);
		int count, recovered;
		m_LOD.KeepCaptureEligible(w.m_Group, count, recovered);
		if (w.Ready()) return AICF_TripOutcome.StartMovement("EXACT_DRIVER_SEAT_PROVEN", token);
		CompartmentAccessComponent access = w.m_Driver.GetCompartmentAccessComponent();
		if (!access || access.IsGettingIn() || access.IsGettingOut() || now < w.m_iNextSeatMs) return AICF_TripOutcome.Wait("SPAWN_SEAT_SETTLING", token);
		if (w.m_iSeatAttempts >= 3 || access.IsInCompartment()) return AICF_TripOutcome.TerminalFailClosed("SPAWN_SEAT_REJECTED", token);
		if (!w.m_Seat)
		{
			BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(w.m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
			array<BaseCompartmentSlot> seats = {};
			if (manager) manager.GetCompartments(seats);
			foreach (BaseCompartmentSlot seat : seats)
			{
				if (PilotCompartmentSlot.Cast(seat) && seat.IsCompartmentAccessible() && !seat.GetOccupant() && !seat.IsReserved())
				{
					w.m_Seat = seat;
					break;
				}
			}
		}
		if (!w.m_Seat || w.m_Seat.GetOccupant() || w.m_Seat.IsReserved()) return AICF_TripOutcome.TerminalFailClosed("NO_EXACT_FREE_PILOT", token);
		w.m_iSeatAttempts++;
		w.m_iNextSeatMs = now + 1000;
		access.GetInVehicle(w.m_Vehicle, w.m_Seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, true);
		return AICF_TripOutcome.Wait("SPAWN_SEAT_POSTCONDITION_PENDING", token);
	}
}
