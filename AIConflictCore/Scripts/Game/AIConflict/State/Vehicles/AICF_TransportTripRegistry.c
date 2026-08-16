// Scheduler-owned registry. It stores one authoritative Trip object per stable
// faction/slot and never mirrors Trip phase, lease or progress fields.
class AICF_TransportTripRegistry
{
	protected int m_iNextTripGeneration;
	protected ref array<ref AICF_TransportTrip> m_aTrips = {};

	int GetCount() { return m_aTrips.Count(); }

	AICF_TransportTrip Get(int index)
	{
		if (!m_aTrips.IsIndexValid(index))
			return null;
		return m_aTrips[index];
	}

	AICF_TransportTrip Find(FactionKey factionKey, int slotId)
	{
		foreach (AICF_TransportTrip trip : m_aTrips)
		{
			if (trip && trip.GetFactionKey() == factionKey && trip.GetSlotId() == slotId)
				return trip;
		}
		return null;
	}

	AICF_TransportTrip FindCurrent(AICF_StrategicAssignmentSnapshot assignment)
	{
		if (!assignment)
			return null;
		AICF_TransportTrip trip = Find(assignment.GetFactionKey(), assignment.GetSlotId());
		if (!trip || trip.GetGroupGeneration() != assignment.GetGroupGeneration())
			return null;
		return trip;
	}

	bool HasNonTerminal(FactionKey factionKey, int slotId, int groupGeneration)
	{
		AICF_TransportTrip trip = Find(factionKey, slotId);
		return trip && !trip.IsTerminal() && trip.GetGroupGeneration() == groupGeneration;
	}

	// Pre-lease trips are admission tokens.  Counting them before another Trip is
	// created prevents ten groups entering acquisition when the faction cap is
	// three, while leases already holding cap are counted by FactionFleet only.
	int GetPreLeaseNonTerminalCount(FactionKey factionKey)
	{
		int count;
		foreach (AICF_TransportTrip trip : m_aTrips)
		{
			if (!trip || trip.IsTerminal() || trip.GetFactionKey() != factionKey)
				continue;
			AICF_VehicleLease lease = trip.GetLease();
			if (!lease || !lease.IsCapActive())
				count++;
		}
		return count;
	}

	AICF_TransportTrip Create(
		AICF_StrategicAssignmentSnapshot assignment,
		int nowMs,
		int absoluteDeadlineMs,
		string causationId)
	{
		if (!assignment || !assignment.IsValid() || causationId.IsEmpty())
			return null;
		AICF_TransportTrip existing = Find(
			assignment.GetFactionKey(),
			assignment.GetSlotId());
		if (existing && !existing.IsTerminal())
			return null;

		m_iNextTripGeneration++;
		string operationId = string.Format(
			"trip-%1-%2-%3-%4",
			assignment.GetFactionKey(),
			assignment.GetSlotId(),
			assignment.GetGroupGeneration(),
			m_iNextTripGeneration);
		AICF_TransportTrip trip = new AICF_TransportTrip(
			assignment,
			m_iNextTripGeneration,
			operationId,
			causationId,
			nowMs,
			absoluteDeadlineMs);
		if (!trip.IsValid())
			return null;
		if (existing)
			m_aTrips.RemoveItem(existing);
		m_aTrips.Insert(trip);
		return trip;
	}

	void RemoveTerminal(AICF_TransportTrip expected)
	{
		if (expected && expected.IsTerminal())
			m_aTrips.RemoveItem(expected);
	}
}

// Immutable query projection for MatchController, markers and diagnostics.
// It contains no setters and is never an alternative source of lifecycle truth.
class AICF_VehicleSlotView
{
	protected bool m_bPresent;
	protected string m_sPhase;
	protected string m_sStatusText;
	protected bool m_bControlsMovement;
	protected bool m_bExecutableVehicleTask;
	protected bool m_bSafeSpawnWait;
	protected bool m_bRestorePending;
	protected bool m_bOrderRestored;
	protected bool m_bClearanceSafe;
	protected AIWaypoint m_VehicleWaypoint;
	protected string m_sFailureReason;
	protected string m_sTerminalReason;
	protected string m_sOperationId;

	void AICF_VehicleSlotView(AICF_TransportTrip trip)
	{
		if (!trip)
			return;
		m_bPresent = true;
		m_sPhase = typename.EnumToString(AICF_ETransportTripPhase, trip.GetPhase());
		m_sStatusText = ResolveStatusText(trip);
		m_bControlsMovement = trip.GetPhase() == AICF_ETransportTripPhase.SITE_PLANNED ||
			trip.GetPhase() == AICF_ETransportTripPhase.APPROACHING_SITE ||
			trip.GetPhase() == AICF_ETransportTripPhase.STAGING_CONFIRMED ||
			trip.GetPhase() == AICF_ETransportTripPhase.SPAWN_COMMIT ||
			trip.GetPhase() == AICF_ETransportTripPhase.BOARDING ||
			trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT ||
			trip.GetPhase() == AICF_ETransportTripPhase.DISMOUNT;
		m_bSafeSpawnWait = trip.GetPhase() == AICF_ETransportTripPhase.WAITING_FOR_SITE &&
			IsSafeSiteReason(trip.GetRequestState().GetLastFailureReason());
		m_bOrderRestored = trip.GetHandoffState().IsOrderRestored();
		m_bClearanceSafe = trip.GetHandoffState().IsClearanceSafe();
		m_bRestorePending = trip.GetHandoffState().IsRestoreRequested() &&
			!m_bOrderRestored;
		AICF_ETransportTripPhase phase = trip.GetPhase();
		if (phase == AICF_ETransportTripPhase.SITE_PLANNED ||
			phase == AICF_ETransportTripPhase.APPROACHING_SITE ||
			phase == AICF_ETransportTripPhase.STAGING_CONFIRMED ||
			phase == AICF_ETransportTripPhase.SPAWN_COMMIT)
		{
			AICF_VehicleSpawnPlan plan = trip.GetRequestState().GetSpawnPlan();
			if (plan)
				m_VehicleWaypoint = plan.GetApproachWaypoint();
			m_bExecutableVehicleTask = m_VehicleWaypoint != null;
		}
		else if (phase == AICF_ETransportTripPhase.BOARDING)
		{
			AICF_VehicleBoardingTokenSet tokens = trip.GetBoardingState().GetTokens();
			m_bExecutableVehicleTask = trip.GetBoardingState().GetStartedAtMs() > 0 &&
				tokens && tokens.Count() > 0;
		}
		else if (phase == AICF_ETransportTripPhase.TRANSIT)
		{
			m_VehicleWaypoint = trip.GetMovementState().GetRouteWaypoint();
			m_bExecutableVehicleTask = m_VehicleWaypoint &&
				trip.GetMovementState().IsRouteWaypointBound();
		}
		else if (phase == AICF_ETransportTripPhase.DISMOUNT)
		{
			m_VehicleWaypoint = trip.GetDismountState().GetDismountWaypoint();
			m_bExecutableVehicleTask = m_VehicleWaypoint &&
				trip.GetDismountState().IsDismountWaypointBound();
		}
		m_sFailureReason = trip.GetRequestState().GetLastFailureReason();
		m_sTerminalReason = trip.GetTerminalReason();
		m_sOperationId = trip.GetOperationId();
	}

	bool IsPresent() { return m_bPresent; }
	string GetPhase() { return m_sPhase; }
	string GetStatusText() { return m_sStatusText; }
	bool IsControllingMovement() { return m_bControlsMovement; }
	bool HasExecutableVehicleTask() { return m_bExecutableVehicleTask; }
	bool IsSafeSpawnWait() { return m_bSafeSpawnWait; }
	bool IsRestorePending() { return m_bRestorePending; }
	bool IsOrderRestored() { return m_bOrderRestored; }
	bool IsClearanceSafe() { return m_bClearanceSafe; }
	AIWaypoint GetVehicleWaypoint() { return m_VehicleWaypoint; }
	string GetFailureReason() { return m_sFailureReason; }
	string GetTerminalReason() { return m_sTerminalReason; }
	string GetOperationId() { return m_sOperationId; }

	protected string ResolveStatusText(AICF_TransportTrip trip)
	{
		if (!trip)
			return "Пешком";
		AICF_VehicleSpawnPlan plan = trip.GetRequestState().GetSpawnPlan();
		switch (trip.GetPhase())
		{
			case AICF_ETransportTripPhase.WAITING_FOR_SITE:
				return ResolveWaitingForSiteStatus(
					trip.GetRequestState().GetLastFailureReason());
			case AICF_ETransportTripPhase.SITE_PLANNED:
				return "Площадка выбрана";
			case AICF_ETransportTripPhase.APPROACHING_SITE:
				if (plan)
				{
					return string.Format(
						"Следует к месту выдачи %1/%2",
						plan.GetStagedCount(),
						plan.GetAliveCount());
				}
				return "Следует к месту выдачи";
			case AICF_ETransportTripPhase.STAGING_CONFIRMED:
				if (plan)
					return string.Format("Ожидание бойцов %1/%2", plan.GetStagedCount(), plan.GetAliveCount());
				return "Ожидание бойцов";
			case AICF_ETransportTripPhase.SPAWN_COMMIT:
				return "Выдача техники";
			case AICF_ETransportTripPhase.BOARDING:
				return "Посадка";
			case AICF_ETransportTripPhase.TRANSIT:
				return "Движение на технике";
			case AICF_ETransportTripPhase.DISMOUNT:
				return "Высадка";
			case AICF_ETransportTripPhase.HANDOFF:
				return "Возврат к пешему приказу";
			case AICF_ETransportTripPhase.COMPLETE:
				return "Задача техники завершена";
			case AICF_ETransportTripPhase.FALLBACK:
				return "Переход на пеший порядок";
			case AICF_ETransportTripPhase.FAILED_CLOSED:
				return "Техника недоступна";
		}
		return "Пешком";
	}

	protected string ResolveWaitingForSiteStatus(string reason)
	{
		switch (reason)
		{
			case "VEHICLE_CAP_UNAVAILABLE":
				return "Ожидание свободной техники";
			case "SPAWN_PAD_OCCUPIED":
			case "NO_EMPTY_TERRAIN":
			case "WATER_OR_UNDRIVABLE_SURFACE":
				return "Поиск свободной площадки";
			case "ENEMY_OWNED":
			case "CONTESTED":
			case "SPAWN_POINT_MISSING":
			case "SPAWN_POINT_DISABLED":
			case "SPAWN_POINT_INACTIVE":
			case "SPAWN_FACTION_INITIALIZING":
			case "SPAWN_FACTION_MISMATCH":
			case "NO_SAFE_SPAWN_AVAILABLE":
				return "Ожидание безопасной базы";
			case "TOO_FAR":
			case "NO_BOARDING_SITE_WITHIN_RANGE":
				return "Ожидание отряда у базы";
			case "GROUP_NOT_READY":
				return "Ожидание готовности отряда";
		}
		return "Ожидание площадки";
	}

	protected bool IsSafeSiteReason(string reason)
	{
		switch (reason)
		{
			case "BASE_MISSING":
			case "BASE_NOT_INITIALIZED":
			case "ENEMY_OWNED":
			case "CONTESTED":
			case "SPAWN_POINT_MISSING":
			case "SPAWN_POINT_DISABLED":
			case "SPAWN_POINT_INACTIVE":
			case "SPAWN_FACTION_INITIALIZING":
			case "SPAWN_FACTION_MISMATCH":
			case "NO_SAFE_SPAWN_AVAILABLE":
			case "TOO_FAR":
			case "NO_EMPTY_TERRAIN":
			case "WATER_OR_UNDRIVABLE_SURFACE":
			case "NO_BOARDING_SITE_WITHIN_RANGE":
				return true;
		}
		return false;
	}
}
