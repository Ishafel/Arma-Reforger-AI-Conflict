// История служит только подписью диагностики. Она не разрешает recovery/delete.
class AICF_VehicleCleanupTraceActorIdentity
{
	IEntity m_Entity;
	EntityID m_Id;
	string m_sRplId;
	string m_sSource;

	void AICF_VehicleCleanupTraceActorIdentity(IEntity entity, string source)
	{
		m_Entity = entity;
		m_Id = entity.GetID();
		m_sRplId = AICF_VehicleCleanupTrace.RplId(entity);
		m_sSource = source;
	}

	bool Matches(IEntity entity)
	{
		return entity && entity == m_Entity && entity.GetID() == m_Id &&
			m_sRplId != "NONE" && AICF_VehicleCleanupTrace.RplId(entity) == m_sRplId;
	}
}

class AICF_VehicleCleanupActorHistory
{
	protected IEntity m_Group;
	protected EntityID m_GroupId;
	protected string m_sGroupRplId;
	protected EntityID m_VehicleId;
	protected string m_sVehicleRplId;
	protected int m_iLeaseGeneration;
	protected FactionKey m_sFaction;
	protected int m_iSlot;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iVehicleGeneration;
	protected int m_iCapturedAtMs;
	protected ref array<ref AICF_VehicleCleanupTraceActorIdentity> m_aActors = {};

	static AICF_VehicleCleanupActorHistory Capture(AICF_TransportTrip trip, AICF_VehicleLease lease, SCR_AIGroup group, int nowMs)
	{
		if (!Replication.IsServer() || !trip || !lease || !group || trip.GetLease() != lease ||
			!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration()))
			return null;
		AICF_VehicleCleanupActorHistory history = new AICF_VehicleCleanupActorHistory();
		history.m_Group = group;
		history.m_GroupId = group.GetID();
		history.m_sGroupRplId = AICF_VehicleCleanupTrace.RplId(group);
		history.m_VehicleId = lease.GetEntityId();
		history.m_sVehicleRplId = lease.GetRplId();
		history.m_iLeaseGeneration = lease.GetLeaseGeneration();
		history.m_sFaction = lease.GetFactionKey();
		history.m_iSlot = lease.GetSlotId();
		history.m_iGroupGeneration = lease.GetGroupGeneration();
		history.m_iTripGeneration = lease.GetTripGeneration();
		history.m_iVehicleGeneration = lease.GetVehicleGeneration();
		history.m_iCapturedAtMs = nowMs;
		// Preset хранит исходные entity даже после incapacitation/выхода из group.
		for (int index; index < AICF_Stage1Config.MAX_GROUP_SIZE; index++)
			history.Add(trip.VehiclePresetMember(index), "TRIP_PRESET");
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetParentGroup() == group)
				history.Add(agent.GetControlledEntity(), "RELEASE_GROUP");
		}
		return history;
	}

	protected void Add(IEntity entity, string source)
	{
		if (!entity)
			return;
		foreach (AICF_VehicleCleanupTraceActorIdentity existing : m_aActors)
		{
			if (existing.Matches(entity))
				return;
		}
		m_aActors.Insert(new AICF_VehicleCleanupTraceActorIdentity(entity, source));
	}

	int GetCapturedAtMs() { return m_iCapturedAtMs; }

	bool MatchesFence(AICF_VehicleCleanupFence fence)
	{
		return fence && fence.IsComplete() && fence.GetEntityId() == m_VehicleId && fence.GetRplId() == m_sVehicleRplId &&
			fence.GetLeaseGeneration() == m_iLeaseGeneration && fence.GetFactionKey() == m_sFaction &&
			fence.GetSlotId() == m_iSlot && fence.GetGroupGeneration() == m_iGroupGeneration &&
			fence.GetTripGeneration() == m_iTripGeneration && fence.GetVehicleGeneration() == m_iVehicleGeneration;
	}

	bool IsGroupCurrent(SCR_AIGroup group)
	{
		return group && group == m_Group && group.GetID() == m_GroupId &&
			m_sGroupRplId != "NONE" && AICF_VehicleCleanupTrace.RplId(group) == m_sGroupRplId;
	}

	string Classify(IEntity entity)
	{
		if (!entity)
			return "NONE";
		bool mismatch;
		foreach (AICF_VehicleCleanupTraceActorIdentity captured : m_aActors)
		{
			if (captured.Matches(entity))
				return captured.m_sSource;
			mismatch = mismatch || captured.m_Id == entity.GetID();
		}
		if (mismatch)
			return "IDENTITY_MISMATCH";
		return "NONE";
	}
}

// Отчёт после сохранённого retained verdict. Никаких gates, actions или таймеров.
class AICF_VehicleCleanupTrace
{
	static string RplId(IEntity entity)
	{
		if (!entity)
			return "NONE";
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl)
			return "NONE";
		return rpl.Id().ToString();
	}

	static void Report(AICF_VehicleCleanupJob job, Vehicle vehicle, string identity, int nowMs, float playerRadius)
	{
		if (!Replication.IsServer() || !job || !job.m_Fence || !job.m_Fence.IsComplete() || !vehicle ||
			job.m_Fence.GetEntityId() != vehicle.GetID() || job.m_Fence.GetRplId() != RplId(vehicle))
			return;
		array<IEntity> reported = {};
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (seat && seat.GetVehicle() == vehicle && seat.GetOccupant())
				ReportActor(job, vehicle, seat.GetOccupant(), seat, "EXACT_SEAT", identity, nowMs, reported);
		}
		array<AIAgent> agents = {};
		if (job.m_Group)
			job.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetParentGroup() == job.m_Group && agent.GetControlledEntity())
				ReportActor(job, vehicle, agent.GetControlledEntity(), null, "MANAGED_GROUP", identity, nowMs, reported);
		}
		PlayerManager players;
		if (GetGame())
			players = GetGame().GetPlayerManager();
		if (!players)
		{
			AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP_BLOCKING_ACTOR", identity + " observation_scope=PLAYER_SCAN player_status=UNKNOWN actor_entity_id=NONE reason=PLAYER_MANAGER_UNAVAILABLE readonly=1");
			return;
		}
		array<int> ids = {};
		players.GetAllPlayers(ids);
		foreach (int id : ids)
		{
			IEntity controlled = players.GetPlayerControlledEntity(id);
			IEntity mainEntity = SCR_PossessingManagerComponent.GetPlayerMainEntity(id);
			if (!controlled && !mainEntity)
				AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP_BLOCKING_ACTOR", identity + string.Format(" observation_scope=PLAYER_SCAN player_status=PLAYER player_id=%1 actor_entity_id=NONE reason=PLAYER_POSITION_UNKNOWN readonly=1", id));
			ReportNearbyPlayer(job, vehicle, controlled, playerRadius, identity, nowMs, reported);
			if (mainEntity != controlled)
				ReportNearbyPlayer(job, vehicle, mainEntity, playerRadius, identity, nowMs, reported);
		}
	}

	protected static void ReportNearbyPlayer(AICF_VehicleCleanupJob job, Vehicle vehicle, IEntity entity, float radius, string identity, int nowMs, array<IEntity> reported)
	{
		if (!entity)
			return;
		if (CompartmentAccessComponent.GetVehicleIn(entity) == vehicle ||
			vector.DistanceSqXZ(entity.GetOrigin(), vehicle.GetOrigin()) <= radius * radius)
			ReportActor(job, vehicle, entity, null, "PLAYER_PROXIMITY_OR_LINK", identity, nowMs, reported);
	}

	protected static void ReportActor(AICF_VehicleCleanupJob job, Vehicle vehicle, IEntity entity, BaseCompartmentSlot seat, string scope, string identity, int nowMs, array<IEntity> reported)
	{
		if (!entity || reported.Contains(entity))
			return;
		reported.Insert(entity);
		EntityID actorId = entity.GetID();
		string actorRpl = RplId(entity);
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		CharacterControllerComponent controller;
		CompartmentAccessComponent access;
		AIAgent agent;
		AIGroup parent;
		if (character)
		{
			controller = character.GetCharacterController();
			access = character.GetCompartmentAccessComponent();
			if (character.GetAIControlComponent())
				agent = character.GetAIControlComponent().GetControlAIAgent();
		}
		if (agent)
			parent = agent.GetParentGroup();
		string life = "NO_CONTROLLER";
		if (controller)
			life = typename.EnumToString(ECharacterLifeState, controller.GetLifeState());
		PlayerManager players;
		if (GetGame())
			players = GetGame().GetPlayerManager();
		int controlledPlayer;
		int mainPlayer;
		string playerStatus = "UNKNOWN";
		if (players)
		{
			controlledPlayer = players.GetPlayerIdFromControlledEntity(entity);
			mainPlayer = SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(entity);
			playerStatus = "NON_PLAYER";
		}
		if (controlledPlayer > 0 || mainPlayer > 0)
			playerStatus = "PLAYER";
		string currentManaged = "UNKNOWN";
		string historical = "UNKNOWN";
		int capturedAtMs;
		AICF_VehicleCleanupActorHistory history = job.m_ActorHistory;
		if (history && history.MatchesFence(job.m_Fence))
		{
			capturedAtMs = history.GetCapturedAtMs();
			historical = history.Classify(entity);
			if (history.IsGroupCurrent(job.m_Group))
			{
				currentManaged = "0";
				array<AIAgent> members = {};
				job.m_Group.GetAgents(members);
				if (agent && parent == job.m_Group && members.Contains(agent) && agent.GetControlledEntity() == entity)
					currentManaged = "1";
			}
		}
		if (!seat && access)
			seat = access.GetCompartment();
		string seatKey = "NONE";
		IEntity seatVehicle;
		IEntity occupant;
		if (seat)
		{
			seatKey = AICF_VehiclePreset.SeatKey(seat);
			seatVehicle = seat.GetVehicle();
			occupant = seat.GetOccupant();
		}
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		string details = identity + string.Format(" observation_scope=%1 observed_at_ms=%2 actor_entity_id=%3 actor_rpl_id=%4 life_state=%5 player_status=%6 controlled_player_id=%7 main_player_id=%8",
			scope, nowMs, actorId, actorRpl, life, playerStatus, controlledPlayer, mainPlayer);
		details += string.Format(" current_parent_group=%1 current_managed=%2 historical_assignment=%3 history_capture_ms=%4 actor_rpl_available=%5 actor_rpl_master=%6 actor_rpl_proxy=%7",
			AICF_GroupTrace.Entity(parent), currentManaged, historical, capturedAtMs, rpl != null, rpl && rpl.IsMaster(), rpl && rpl.IsProxy());
		details += string.Format(" seat=%1 seat_owner_vehicle=%2 seat_occupant=%3 exact_seat_occupant=%4 linked_vehicle=%5 in_compartment=%6 getting_in=%7 getting_out=%8 switching_seat=%9",
			seatKey, AICF_GroupTrace.Entity(seatVehicle), AICF_GroupTrace.Entity(occupant), seatVehicle == vehicle && occupant == entity,
			AICF_GroupTrace.Entity(CompartmentAccessComponent.GetVehicleIn(entity)), access && access.IsInCompartment(),
			access && access.IsGettingIn(), access && access.IsGettingOut(), access && access.IsSwitchingSeatsAnim());
		details += " actor_pos=" + AICF_GroupTrace.Position(entity) + " readonly=1";
		// Только подпись уже вынесенного verdict; изменённая identity не получает метку.
		if (entity.GetID() != actorId || RplId(entity) != actorRpl)
			return;
		AICF_Stage3Diagnostics.Info("VEHICLE_CLEANUP_BLOCKING_ACTOR", details);
	}
}
