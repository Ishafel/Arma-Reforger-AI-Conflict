enum AICF_ELogisticsPhase
{
	IDLE_AT_DEPOT,
	SPAWN_PENDING,
	DRIVER_READY,
	TO_SOURCE,
	LOADING,
	TO_DESTINATION,
	UNLOADING,
	RETURN_HOME,
	WAIT_RETRY,
	RETIRING,
	FAILED_CLOSED
}

class AICF_LogisticsEndpoint
{
	SCR_CampaignMilitaryBaseComponent m_Base;
	EntityID m_BaseId = EntityID.INVALID;
	Faction m_Owner;
	SCR_ResourceComponent m_Resource;
	ref AICF_LogisticsResourcePool m_Pool;
	vector m_vPosition;
	int m_iDepth = -1;
	bool m_bOpen;
	int m_iOpenedAtMs;
	int m_iBlockedUntilMs;

	bool IdentityValid()
	{
		return m_Base && m_Base.GetOwner() && m_Base.GetOwner().GetID() == m_BaseId && m_Base.IsInitialized() &&
			m_Base.GetFaction() == m_Owner && m_Base.GetResourceComponent() == m_Resource && m_Pool && m_Pool.Valid();
	}

	string Key() { return m_BaseId.ToString(); }
}

class AICF_LogisticsCargoBatch
{
	ref AICF_LogisticsEndpoint m_Source;
	float m_fAmount;
}

class AICF_LogisticsJob
{
	string m_sToken;
	int m_iGeneration;
	int m_iGraphRevision;
	int m_iExpiresAtMs;
	ref AICF_LogisticsEndpoint m_Source;
	ref AICF_LogisticsEndpoint m_Destination;
	ref AICF_LogisticsEndpoint m_OriginalSource;
	vector m_vLoad;
	vector m_vUnload;
	float m_fReservedSource;
	float m_fReservedIncoming;
	bool m_bLoaded;
	bool m_bReturn;
	bool m_bCancelled;
}

// Worker identity переживает jobs, остановку и replacement. Phase меняет
// исключительно TransportTripController. Job/reservations принадлежат logistics.
class AICF_LogisticsWorker
{
	int m_iSlot;
	int m_iOrdinal;
	int m_iGeneration;
	SCR_CampaignFaction m_Faction;
	IEntity m_Depot;
	EntityID m_DepotId = EntityID.INVALID;
	SCR_CampaignMilitaryBaseComponent m_Home;
	EntityID m_HomeId = EntityID.INVALID;
	SCR_CampaignBuildingProviderComponent m_Provider;
	EntityID m_ProviderId = EntityID.INVALID;
	SCR_CatalogEntitySpawnerComponent m_Production;
	EntityID m_ProductionId = EntityID.INVALID;
	SCR_EntityCatalogEntry m_Entry;
	float m_fPrefabCapacity;
	SCR_EntitySpawnerSlotComponent m_SpawnSlot;
	EntityID m_SpawnSlotId = EntityID.INVALID;
	ref AICF_VehicleSpawnSiteReservation m_Site;
	vector m_aSpawnTransform[4];
	vector m_vParking;
	bool m_bEligible;
	bool m_bRetireAfterCargo;
	SCR_AIGroup m_Group;
	EntityID m_GroupId = EntityID.INVALID;
	ChimeraCharacter m_Driver;
	EntityID m_DriverId = EntityID.INVALID;
	Vehicle m_Vehicle;
	EntityID m_VehicleId = EntityID.INVALID;
	string m_sVehicleRpl;
	BaseCompartmentSlot m_Seat;
	ref AICF_VehicleLease m_Lease;
	AICF_FactionFleet m_Fleet;
	ref AICF_LogisticsJob m_Job;
	ref AICF_LogisticsSearch m_Search;
	ref array<ref AICF_LogisticsCargoBatch> m_aCargo = {};
	ref AICF_LogisticsResourcePool m_CargoPool;
	float m_fObservedCargo;
	float m_fLoaded;
	float m_fDelivered;
	float m_fReturned;
	float m_fLost;
	float m_fReleased;
	float m_fExternalIn;
	float m_fExternalOut;
	float m_fDiscrepancy;
	bool m_bCustody;
	bool m_bCargoFault;
	bool m_bStopped;
	bool m_bCleanupComplete;
	bool m_bCleanupQueued;
	AICF_ELogisticsPhase m_ePhase;
	AIWaypoint m_Waypoint;
	EntityID m_WaypointId = EntityID.INVALID;
	vector m_vEndpoint;
	vector m_vProgressPosition;
	int m_iPhaseAtMs;
	int m_iProgressAtMs;
	int m_iDriverWaitMs;
	int m_iProgressWaitBaselineMs;
	ref AICF_LogisticsDriverInteraction m_DriverInteraction;
	int m_iStationaryAtMs;
	int m_iNextPollMs;
	int m_iRetryAtMs;
	int m_iIdleAtMs;
	int m_iReturnAttempts;
	int m_iRouteRetries;
	int m_iTransferFailures;
	int m_iSeatAttempts;
	int m_iNextSeatMs;
	int m_iLastLogMs;
	string m_sLastReason;
	string m_sPlanningReason = "NO_OPEN_DEMAND";

	bool GroupIdentity()
	{
		return m_Group && m_Group.GetID() == m_GroupId && m_Group.GetFaction() == m_Faction;
	}

	bool VehicleIdentity()
	{
		if (!m_Vehicle || m_Vehicle.GetID() != m_VehicleId || !m_Lease || !m_Fleet ||
			m_Fleet.FindLeaseForSlot(m_iSlot, m_iGeneration) != m_Lease ||
			!m_Lease.MatchesTripIdentity(m_Faction.GetFactionKey(), m_iSlot, m_iGeneration, m_iGeneration)) return false;
		RplComponent rpl = RplComponent.Cast(m_Vehicle.FindComponent(RplComponent));
		return rpl && rpl.IsMaster() && rpl.Id().ToString() == m_sVehicleRpl &&
			m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl);
	}

	bool DriverIdentity()
	{
		if (!GroupIdentity() || !m_Driver || m_Driver.GetID() != m_DriverId || !AICF_GroupRuntime.IsAliveCharacter(m_Driver)) return false;
		CharacterControllerComponent controller = m_Driver.GetCharacterController();
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || players.GetPlayerIdFromControlledEntity(m_Driver) > 0 || SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(m_Driver) > 0) return false;
		RplComponent rpl = RplComponent.Cast(m_Driver.FindComponent(RplComponent));
		return controller && !controller.IsPlayerControlled() && rpl && rpl.IsMaster() && SCR_Faction.GetEntityFaction(m_Driver) == m_Faction;
	}

	bool Ready()
	{
		if (!VehicleIdentity() || !DriverIdentity() || !m_Seat || m_Seat.GetVehicle() != m_Vehicle || m_Seat.GetOccupant() != m_Driver) return false;
		array<AIAgent> members = {};
		m_Group.GetAgents(members);
		if (members.Count() != 1 || !members[0] || members[0].GetControlledEntity() != m_Driver) return false;
		CompartmentAccessComponent access = m_Driver.GetCompartmentAccessComponent();
		return access && access.GetCompartment() == m_Seat && access.IsInCompartment() && !access.IsGettingIn() &&
			!access.IsGettingOut() && CompartmentAccessComponent.GetVehicleIn(m_Driver) == m_Vehicle;
	}

	// Учитывается только подтверждённое ожидание, отдельно от физического progress.
	void MarkProgress(int now)
	{
		m_iProgressAtMs = now;
		m_iProgressWaitBaselineMs = m_iDriverWaitMs;
	}

	int ProgressAgeMs(int now)
	{
		return now - m_iProgressAtMs - (m_iDriverWaitMs - m_iProgressWaitBaselineMs);
	}

	int LegAgeMs(int now)
	{
		return now - m_iPhaseAtMs - m_iDriverWaitMs;
	}

	float BalanceDelta()
	{
		float cargo;
		if (m_bCustody) cargo = m_fObservedCargo;
		return m_fLoaded + m_fExternalIn - m_fDelivered - m_fReturned - cargo - m_fLost - m_fReleased - m_fExternalOut;
	}

	// Cleanup считает protected любого живого occupant, включая нашего driver.
	// Для движения/операций защищается чужое использование; exact driver уже
	// проверен Ready/DriverIdentity и не является причиной прекратить свой trip.
	bool HasForeignOccupant(bool allowExactDriverReservation = false)
	{
		if (!VehicleIdentity()) return true;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager) return true;
		array<BaseCompartmentSlot> seats = {};
		manager.GetCompartments(seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat) return true;
			IEntity occupant = seat.GetOccupant();
			if (occupant && occupant != m_Driver) return true;
			if (!occupant && seat.IsReserved() && !(allowExactDriverReservation && seat == m_Seat && seat.IsReservedBy(m_Driver))) return true;
		}
		return false;
	}

	void Log(string eventName, string details = "")
	{
		if (!details.Contains("reason=")) details += " reason=STATE_TRANSITION";
		string token = "NONE";
		if (m_Job) token = m_Job.m_sToken;
		string identity = string.Format("schema_version=2 faction=%1 slot=%2 depot=%3 depot_entity=%4 job=%5 token=%6 generation=%7 vehicle=%8 driver=%9",
			m_Faction.GetFactionKey(), m_iSlot, m_DepotId, m_DepotId, token, token, m_iGeneration, m_VehicleId, m_DriverId);
		identity += string.Format(" phase=%1 vehicle_rpl=%2", typename.EnumToString(AICF_ELogisticsPhase, m_ePhase), m_sVehicleRpl);
		AICF_Stage4Diagnostics.Info(eventName, identity + " " + details);
	}
}
