// Only this exact trip's handbrake write may later be released. Existing native
// handbrake ownership, human/foreign users and replacement cars are preserved.
class AICF_VehicleParking
{
	protected AICF_TransportTrip m_Trip;
	protected AICF_VehicleLease m_Lease;
	protected Vehicle m_Vehicle;
	protected CarControllerComponent m_Controller;
	protected EntityID m_VehicleId;
	protected string m_sVehicleRpl;
	protected int m_iLeaseGeneration;
	protected int m_iVehicleGeneration;
	protected int m_iGroupGeneration;
	protected int m_iTripGeneration;
	protected int m_iPresetRevision;
	protected bool m_bOwnsBrake;
	protected bool m_bStopped;
	protected ref array<IEntity> m_aMembers = {};
	protected ref array<EntityID> m_aMemberIds = {};

	void AICF_VehicleParking(AICF_TransportTrip trip)
	{
		m_Trip = trip;
		if (!trip || !trip.GetLease()) return;
		m_Lease = trip.GetLease(); m_Vehicle = m_Lease.GetVehicle();
		if (!m_Vehicle) return;
		m_Controller = CarControllerComponent.Cast(m_Vehicle.FindComponent(CarControllerComponent));
		m_VehicleId = m_Vehicle.GetID(); m_sVehicleRpl = AICF_VehicleBehaviorBinding.ComponentRpl(m_Vehicle);
		m_iLeaseGeneration = m_Lease.GetLeaseGeneration(); m_iVehicleGeneration = m_Lease.GetVehicleGeneration();
		m_iGroupGeneration = trip.GetGroupGeneration(); m_iTripGeneration = trip.GetTripGeneration(); m_iPresetRevision = trip.GetVehiclePresetRevision();
		for (int position; position < AICF_Stage1Config.MAX_GROUP_SIZE; position++)
		{
			IEntity member = trip.VehiclePresetMember(position);
			if (!member) continue;
			m_aMembers.Insert(member); m_aMemberIds.Insert(member.GetID());
		}
	}

	protected bool HasCapturedMember(IEntity member)
	{
		int index = m_aMembers.Find(member);
		return member && index >= 0 && member.GetID() == m_aMemberIds[index];
	}

	protected bool IsExactVehicle(AICF_TransportTrip trip)
	{
		if (!(Replication.IsServer() && !m_bStopped && trip && trip == m_Trip)) return false;
		if (!(m_Lease && trip.GetLease() == m_Lease && m_Lease.HasPhysicalAsset())) return false;
		if (!(trip.GetGroupGeneration() == m_iGroupGeneration && trip.GetTripGeneration() == m_iTripGeneration &&
			m_Lease.GetLeaseGeneration() == m_iLeaseGeneration && m_Lease.GetVehicleGeneration() == m_iVehicleGeneration)) return false;
		if (!m_Lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), m_iGroupGeneration, m_iTripGeneration)) return false;
		if (!(m_Vehicle && m_Lease.GetVehicle() == m_Vehicle && m_Vehicle.GetID() == m_VehicleId)) return false;
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle) ||
			!m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)) return false;
		if (!(AICF_VehicleBehaviorBinding.ComponentRpl(m_Vehicle) == m_sVehicleRpl && m_Controller)) return false;
		return CarControllerComponent.Cast(m_Vehicle.FindComponent(CarControllerComponent)) == m_Controller;
	}

	protected bool HasNoForeignOrHumanUse()
	{
		if (!GetGame() || !GetGame().GetPlayerManager()) return false;
		PlayerManager players = GetGame().GetPlayerManager();
		array<int> playerIds = {}; players.GetAllPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity controlled = players.GetPlayerControlledEntity(playerId);
			IEntity main = SCR_PossessingManagerComponent.GetPlayerMainEntity(playerId);
			if (!controlled && !main) return false;
			array<IEntity> observed = {controlled, main};
			foreach (IEntity entity : observed)
			{
				if (!entity) continue;
				if (entity == m_Vehicle || CompartmentAccessComponent.GetVehicleIn(entity) == m_Vehicle) return false;
				ChimeraCharacter character = ChimeraCharacter.Cast(entity);
				if (!character) continue;
				CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
				if (access && access.GetVehicleCompartmentManagerOwner() == m_Vehicle) return false;
			}
		}
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager) return false;
		array<BaseCompartmentSlot> seats = {}; manager.GetCompartments(seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat) return false;
			IEntity occupant = seat.GetOccupant();
			if (!occupant) continue;
			if (!HasCapturedMember(occupant) || !AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(occupant) ||
				players.GetPlayerIdFromControlledEntity(occupant) > 0 || SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(occupant) > 0) return false;
			if (AICF_GroupRuntime.IsAliveCharacter(occupant))
			{
				ChimeraCharacter original = ChimeraCharacter.Cast(occupant);
				if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(occupant) || !original || !original.GetAIControlComponent()) return false;
				AIAgent agent = original.GetAIControlComponent().GetControlAIAgent();
				if (!agent || agent.GetControlledEntity() != occupant || !m_Trip.GetAssignment() || agent.GetParentGroup() != m_Trip.GetAssignment().GetGroup()) return false;
			}
		}
		return true;
	}

	protected bool IsUsableVehicle()
	{
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		return usage && usage.GetDamageState() != EDamageState.DESTROYED && !SCR_AIVehicleUsability.VehicleIsOnFire(m_Vehicle) &&
			SCR_AIVehicleUsability.VehicleCanMove(m_Vehicle);
	}

	bool EnsureApplied(AICF_TransportTrip trip)
	{
		if (!IsExactVehicle(trip) || !trip.IsValid() || trip.IsTerminal() || !trip.IsVehiclePresetCurrent() ||
			trip.GetVehiclePresetRevision() != m_iPresetRevision || !trip.GetVehiclePreset() ||
			trip.GetVehiclePreset().m_iBehavior != AICF_EVehicleBehavior.TRANSPORT ||
			trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || !IsUsableVehicle() || !HasNoForeignOrHumanUse()) return false;
		if (!m_Controller.GetPersistentHandBrake())
		{
			if (!IsExactVehicle(trip) || !IsUsableVehicle() || !HasNoForeignOrHumanUse()) return false;
			m_Controller.SetPersistentHandBrake(true);
			m_bOwnsBrake = m_Controller.GetPersistentHandBrake();
			Log("APPLIED");
		}
		return m_Controller.GetPersistentHandBrake();
	}

	bool IsReadyForExit(AICF_TransportTrip trip)
	{
		return EnsureApplied(trip) && AICF_VehicleShortRecovery.IsSlowVehicle(m_Vehicle);
	}

	bool ReleaseForResume(AICF_TransportTrip trip)
	{
		if (!IsExactVehicle(trip) || !trip.IsValid() || trip.IsTerminal() || !trip.IsVehiclePresetCurrent() || !HasNoForeignOrHumanUse()) return false;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		array<BaseCompartmentSlot> seats = {}; manager.GetCompartments(seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat || seat.GetType() != ECompartmentType.PILOT) continue;
			IEntity pilot = seat.GetOccupant();
			if (!HasCapturedMember(pilot) || !AICF_VehicleShortRecovery.IsCurrentMember(trip, pilot) || !AICF_VehicleNativeSubgroup.Settled(pilot, m_Vehicle)) return false;
			ChimeraCharacter character = ChimeraCharacter.Cast(pilot);
			AIAgent agent = character.GetAIControlComponent().GetControlAIAgent();
			AICarMovementComponent car = AICarMovementComponent.Cast(agent.GetMovementComponent());
			if (!car || !car.GetAIAgent() || car.GetAIAgent().GetControlledEntity() != m_Vehicle) return false;
			Stop(trip);
			return m_bStopped && !m_bOwnsBrake;
		}
		return false;
	}

	void Stop(AICF_TransportTrip trip)
	{
		if (m_bStopped) return;
		if (m_bOwnsBrake && IsExactVehicle(trip) && HasNoForeignOrHumanUse())
		{
			if (m_Controller.GetPersistentHandBrake()) m_Controller.SetPersistentHandBrake(false);
			m_bOwnsBrake = false;
			Log("RELEASED");
		}
		m_bStopped = true;
	}

	protected void Log(string eventName)
	{
		AICF_Stage3Diagnostics.Info("VEHICLE_PARKING_" + eventName, string.Format("operation_id=%1 vehicle=%2 component_rpl=%3 owns_brake=%4 persistent_brake=%5 lease_generation=%6 vehicle_generation=%7 direct_velocity_pose_mutation=0",
			m_Trip.GetOperationId(), AICF_GroupTrace.Entity(m_Vehicle), m_sVehicleRpl, m_bOwnsBrake, m_Controller.GetPersistentHandBrake(), m_iLeaseGeneration, m_iVehicleGeneration));
	}
}
