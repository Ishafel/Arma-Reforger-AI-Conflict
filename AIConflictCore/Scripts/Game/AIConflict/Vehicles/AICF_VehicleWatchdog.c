// Pure runtime queries used by the coordinator. Recovery policy stays in the
// coordinator so this class cannot create waypoints or mutate a group.
class AICF_VehicleWatchdog
{
	protected static const float DISMOUNT_CLEARANCE_MARGIN_METERS = 0.5;
	protected static const float BOARDING_TRANSITION_SCOPE_MARGIN_METERS = 6.0;

	bool IsDestroyed(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicle())
			return true;

		SCR_AIVehicleUsageComponent usage = runtime.GetVehicleUsage();
		return !usage || usage.GetDamageState() == EDamageState.DESTROYED;
	}

	bool CanMove(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicle())
			return false;

		return SCR_AIVehicleUsability.VehicleCanMove(runtime.GetVehicle());
	}

	bool IsOnFire(AICF_VehicleRuntime runtime)
	{
		return runtime && runtime.GetVehicle() &&
			SCR_AIVehicleUsability.VehicleIsOnFire(runtime.GetVehicle());
	}

	float GetMovementDamage(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicle())
			return 1.0;

		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.GetDamageManager(runtime.GetVehicle());
		if (!damageManager)
			return 1.0;

		return damageManager.GetMovementDamage();
	}

	bool IsOverturned(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicle())
			return false;

		vector transform[4];
		runtime.GetVehicle().GetWorldTransform(transform);
		return transform[1][1] < 0.25;
	}

	IEntity ResolveAliveDriver(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicleUsage())
			return null;

		PilotCompartmentSlot pilotSlot = runtime.GetVehicleUsage().GetPilotCompartmentSlot();
		if (!pilotSlot)
			return null;

		IEntity driver = pilotSlot.GetOccupant();
		if (!AICF_GroupRuntime.IsAliveCharacter(driver))
			return null;

		return driver;
	}

	IEntity ResolveAliveGunner(AICF_VehicleRuntime runtime)
	{
		if (!runtime || !runtime.GetVehicleUsage())
			return null;

		TurretCompartmentSlot turretSlot = runtime.GetVehicleUsage().GetTurretCompartmentSlot();
		if (!turretSlot)
			return null;

		IEntity gunner = turretSlot.GetOccupant();
		if (!AICF_GroupRuntime.IsAliveCharacter(gunner))
			return null;

		return gunner;
	}

	int CountAccessibleSeats(AICF_VehicleRuntime runtime, out bool hasPilot, out bool hasTurret)
	{
		hasPilot = false;
		hasTurret = false;
		if (!runtime || !runtime.GetVehicle())
			return 0;

		return CountAccessibleSeatsForVehicle(
			runtime.GetVehicle(),
			runtime.GetKind(),
			hasPilot,
			hasTurret);
	}

	int CountAccessibleSeatsForVehicle(
		Vehicle vehicle,
		AICF_EVehicleKind kind,
		out bool hasPilot,
		out bool hasTurret)
	{
		hasPilot = false;
		hasTurret = false;
		if (!vehicle)
			return 0;

		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager)
			return 0;

		array<BaseCompartmentSlot> compartments = {};
		manager.GetCompartments(compartments);
		int count;
		foreach (BaseCompartmentSlot compartment : compartments)
		{
			if (!compartment || !compartment.IsCompartmentAccessible() ||
				compartment.GetOccupant() || compartment.IsReserved())
				continue;

			bool supportedSeat;
			if (PilotCompartmentSlot.Cast(compartment))
			{
				hasPilot = true;
				supportedSeat = true;
			}
			else if (CargoCompartmentSlot.Cast(compartment))
			{
				supportedSeat = true;
			}
			else if (TurretCompartmentSlot.Cast(compartment))
			{
				hasTurret = true;
				supportedSeat = kind == AICF_EVehicleKind.ARMED_LIGHT;
			}
			if (supportedSeat)
				count++;
		}
		return count;
	}

	bool InspectVehicleCapacity(
		Vehicle vehicle,
		AICF_EVehicleKind kind,
		int requiredSeats,
		out int availableSeats,
		out bool hasPilot,
		out bool hasTurret)
	{
		availableSeats = CountAccessibleSeatsForVehicle(vehicle, kind, hasPilot, hasTurret);
		if (requiredSeats <= 0 || availableSeats < requiredSeats || !hasPilot)
			return false;

		return kind != AICF_EVehicleKind.ARMED_LIGHT || (hasTurret && requiredSeats >= 2);
	}

	int CountAliveGroupMembersInVehicle(SCR_AIGroup group, Vehicle vehicle)
	{
		if (!group || !vehicle)
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int count;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;

			if (CompartmentAccessComponent.GetVehicleIn(entity) == vehicle)
				count++;
		}
		return count;
	}

	bool AreAllAliveMembersInVehicle(SCR_AIGroup group, Vehicle vehicle)
	{
		int alive = AICF_GroupRuntime.CountAliveAgents(group);
		return alive > 0 && CountAliveGroupMembersInVehicle(group, vehicle) == alive;
	}

	bool IsMemberSettledInVehicle(IEntity entity, Vehicle vehicle)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!AICF_GroupRuntime.IsAliveCharacter(character) || !vehicle)
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return access && CompartmentAccessComponent.GetVehicleIn(character) == vehicle &&
			access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut() &&
			character.IsInVehicle();
	}

	bool AreAllAliveMembersSettledInVehicle(SCR_AIGroup group, Vehicle vehicle)
	{
		if (!group || !vehicle)
			return false;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int alive;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;
			alive++;
			if (!IsMemberSettledInVehicle(entity, vehicle))
				return false;
		}

		return alive > 0;
	}

	// Produces one authoritative physical snapshot for timeout, grace and
	// completion decisions. Keeping all signals in the same sample prevents a
	// visually active GetIn transition from being mistaken for no progress.
	bool InspectBoardingProgress(
		SCR_AIGroup group,
		Vehicle vehicle,
		out int aliveCount,
		out int linkedCount,
		out int compartmentCount,
		out int gettingInCount,
		out int gettingOutCount,
		out int characterVehicleCount,
		out int settledCount,
		out float nearestDistanceMeters,
		out float farthestDistanceMeters,
		out string memberSamples)
	{
		aliveCount = 0;
		linkedCount = 0;
		compartmentCount = 0;
		gettingInCount = 0;
		gettingOutCount = 0;
		characterVehicleCount = 0;
		settledCount = 0;
		nearestDistanceMeters = float.MAX;
		farthestDistanceMeters = -1.0;
		memberSamples = string.Empty;
		if (!group || !vehicle)
			return false;
		vector boundsMin;
		vector boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!AICF_GroupRuntime.IsAliveCharacter(character))
				continue;

			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			bool linked = CompartmentAccessComponent.GetVehicleIn(character) == vehicle;
			vector localOrigin = vehicle.CoordToLocal(character.GetOrigin());
			bool insideTransitionScope =
				localOrigin[0] >= boundsMin[0] - BOARDING_TRANSITION_SCOPE_MARGIN_METERS &&
				localOrigin[0] <= boundsMax[0] + BOARDING_TRANSITION_SCOPE_MARGIN_METERS &&
				localOrigin[1] >= boundsMin[1] - BOARDING_TRANSITION_SCOPE_MARGIN_METERS &&
				localOrigin[1] <= boundsMax[1] + BOARDING_TRANSITION_SCOPE_MARGIN_METERS &&
				localOrigin[2] >= boundsMin[2] - BOARDING_TRANSITION_SCOPE_MARGIN_METERS &&
				localOrigin[2] <= boundsMax[2] + BOARDING_TRANSITION_SCOPE_MARGIN_METERS;
			bool targetScoped = linked || insideTransitionScope;
			bool inCompartment = linked && access && access.IsInCompartment();
			bool gettingIn = targetScoped && access && access.IsGettingIn();
			bool gettingOut = targetScoped && access && access.IsGettingOut();
			bool characterVehicle = linked && character.IsInVehicle();
			bool settled = linked && inCompartment && !gettingIn && !gettingOut && characterVehicle;
			float distanceMeters = Math.Sqrt(vector.DistanceSqXZ(character.GetOrigin(), vehicle.GetOrigin()));
			string currentActionType = "NONE";
			string currentActionState = "NONE";
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
				agent.FindComponent(SCR_AIUtilityComponent));
			if (utility)
			{
				AIActionBase currentAction = utility.GetCurrentAction();
				if (currentAction)
				{
					currentActionType = currentAction.Type().ToString();
					currentActionState = typename.EnumToString(
						EAIActionState,
						currentAction.GetActionState());
				}
			}

			aliveCount++;
			if (linked)
				linkedCount++;
			if (inCompartment)
				compartmentCount++;
			if (gettingIn)
				gettingInCount++;
			if (gettingOut)
				gettingOutCount++;
			if (characterVehicle)
				characterVehicleCount++;
			if (settled)
				settledCount++;
			nearestDistanceMeters = Math.Min(nearestDistanceMeters, distanceMeters);
			farthestDistanceMeters = Math.Max(farthestDistanceMeters, distanceMeters);

			if (!memberSamples.IsEmpty())
				memberSamples += ",";
			memberSamples += string.Format(
				"%1:distance_m=%2|linked=%3|compartment=%4|getting_in=%5|getting_out=%6|character_vehicle=%7|settled=%8|target_scope=%9",
				character.GetID(),
				distanceMeters,
				linked,
				inCompartment,
				gettingIn,
				gettingOut,
				characterVehicle,
				settled,
				targetScoped);
			memberSamples += string.Format(
				"|ai_action=%1|ai_action_state=%2",
				currentActionType,
				currentActionState);
		}

		if (aliveCount <= 0)
		{
			nearestDistanceMeters = -1.0;
			return false;
		}
		return true;
	}

	bool IsAliveGroupMember(SCR_AIGroup group, IEntity entity)
	{
		if (!group || !AICF_GroupRuntime.IsAliveCharacter(entity))
			return false;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetControlledEntity() == entity)
				return true;
		}

		return false;
	}

	bool MeasureAliveGroupDistances(
		SCR_AIGroup group,
		Vehicle vehicle,
		out int aliveCount,
		out float leaderDistanceMeters,
		out float nearestDistanceMeters,
		out float farthestDistanceMeters,
		out string memberSamples)
	{
		aliveCount = 0;
		leaderDistanceMeters = -1.0;
		nearestDistanceMeters = float.MAX;
		farthestDistanceMeters = -1.0;
		memberSamples = string.Empty;
		if (!group || !vehicle)
			return false;

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;

			float distanceMeters = Math.Sqrt(vector.DistanceSqXZ(entity.GetOrigin(), vehicle.GetOrigin()));
			aliveCount++;
			nearestDistanceMeters = Math.Min(nearestDistanceMeters, distanceMeters);
			farthestDistanceMeters = Math.Max(farthestDistanceMeters, distanceMeters);
			if (entity == leader)
				leaderDistanceMeters = distanceMeters;

			if (!memberSamples.IsEmpty())
				memberSamples += ",";
			memberSamples += string.Format("%1:%2", entity.GetID(), distanceMeters);
		}

		if (aliveCount <= 0)
		{
			nearestDistanceMeters = -1.0;
			return false;
		}

		return true;
	}

	bool MeasureAliveGroupSpread(
		SCR_AIGroup group,
		out int aliveCount,
		out float farthestFromLeaderMeters,
		out float maximumPairDistanceMeters,
		out string memberSamples)
	{
		aliveCount = 0;
		farthestFromLeaderMeters = -1.0;
		maximumPairDistanceMeters = -1.0;
		memberSamples = string.Empty;
		if (!group)
			return false;

		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		if (!leader)
			return false;
		array<IEntity> aliveMembers = {};
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity member = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(member))
				continue;

			float leaderDistanceMeters = Math.Sqrt(vector.DistanceSqXZ(member.GetOrigin(), leader.GetOrigin()));
			aliveMembers.Insert(member);
			aliveCount++;
			farthestFromLeaderMeters = Math.Max(farthestFromLeaderMeters, leaderDistanceMeters);
			if (!memberSamples.IsEmpty())
				memberSamples += ",";
			memberSamples += string.Format("%1:%2", member.GetID(), leaderDistanceMeters);
		}

		for (int firstIndex = 0; firstIndex < aliveMembers.Count(); firstIndex++)
		{
			for (int secondIndex = firstIndex + 1; secondIndex < aliveMembers.Count(); secondIndex++)
			{
				float pairDistanceMeters = Math.Sqrt(vector.DistanceSqXZ(
					aliveMembers[firstIndex].GetOrigin(),
					aliveMembers[secondIndex].GetOrigin()));
				maximumPairDistanceMeters = Math.Max(maximumPairDistanceMeters, pairDistanceMeters);
			}
		}

		if (aliveCount == 1)
			maximumPairDistanceMeters = 0.0;
		return aliveCount > 0;
	}

	int ResetGroupVehicleActions(SCR_AIGroup group)
	{
		if (!group)
			return 0;

		group.ReleaseCompartments();
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int interrupted;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character)
				continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller || controller.GetLifeState() == ECharacterLifeState.DEAD)
				continue;

			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access)
				continue;

			access.InterruptVehicleActionQueue(true, true, true);
			interrupted++;
		}

		return interrupted;
	}

	string DescribeGroupVehicleOccupants(SCR_AIGroup group, AICF_VehicleRuntime runtime)
	{
		if (!group || !runtime || !runtime.GetVehicle())
			return "NONE";

		IEntity pilot;
		IEntity gunner;
		SCR_AIVehicleUsageComponent usage = runtime.GetVehicleUsage();
		if (usage)
		{
			PilotCompartmentSlot pilotSlot = usage.GetPilotCompartmentSlot();
			if (pilotSlot)
				pilot = pilotSlot.GetOccupant();
			TurretCompartmentSlot gunnerSlot = usage.GetTurretCompartmentSlot();
			if (gunnerSlot)
				gunner = gunnerSlot.GetOccupant();
		}

		string samples;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character || CompartmentAccessComponent.GetVehicleIn(character) != runtime.GetVehicle())
				continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller || controller.GetLifeState() == ECharacterLifeState.DEAD)
				continue;

			string role = "CARGO";
			if (character == pilot)
				role = "DRIVER";
			else if (character == gunner)
				role = "GUNNER";
			if (!samples.IsEmpty())
				samples += ",";
			samples += string.Format("%1:%2:life_%3", character.GetID(), role, controller.GetLifeState());
		}

		if (samples.IsEmpty())
			return "NONE";
		return samples;
	}

	bool AreAllProtectedMembersOutOfVehicle(SCR_AIGroup group, Vehicle vehicle)
	{
		if (!group || !vehicle)
			return false;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (IsProtectedCharacter(entity) && CompartmentAccessComponent.GetVehicleIn(entity) == vehicle)
				return false;
		}
		return true;
	}

	bool InspectProtectedMemberDismountClearance(
		SCR_AIGroup group,
		Vehicle vehicle,
		out int logicalOccupantCount,
		out int transitionCount,
		out int insideBoundsCount,
		out string memberSamples)
	{
		logicalOccupantCount = 0;
		transitionCount = 0;
		insideBoundsCount = 0;
		memberSamples = string.Empty;
		if (!group || !vehicle)
		{
			memberSamples = "INVALID_INPUT";
			return false;
		}

		vector boundsMin;
		vector boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group)
				continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!IsProtectedCharacter(character))
				continue;

			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			bool linkedToVehicle = CompartmentAccessComponent.GetVehicleIn(character) == vehicle;
			vector localOrigin = vehicle.CoordToLocal(character.GetOrigin());
			bool insideBounds = IsInsideExpandedVehicleBounds(localOrigin, boundsMin, boundsMax);
			bool inCompartment = false;
			bool gettingIn = false;
			bool gettingOut = false;
			if (access)
			{
				inCompartment = linkedToVehicle && access.IsInCompartment();
				// Get-in/out transition state is character-global. Attribute it to
				// this vehicle only while linked to it or physically inside its
				// bounds; boarding another vehicle must not pin stale cleanup.
				gettingIn = (linkedToVehicle || insideBounds) && access.IsGettingIn();
				gettingOut = (linkedToVehicle || insideBounds) && access.IsGettingOut();
			}

			bool characterInVehicle = linkedToVehicle && character.IsInVehicle();
			bool logicalOccupant = linkedToVehicle || inCompartment || characterInVehicle;
			bool inTransition = gettingIn || gettingOut;

			if (logicalOccupant)
				logicalOccupantCount++;
			if (inTransition)
				transitionCount++;
			if (insideBounds)
				insideBoundsCount++;

			if (!memberSamples.IsEmpty())
				memberSamples += ",";
			memberSamples += string.Format(
				"%1:logical_%2:linked_%3:compartment_%4:character_vehicle_%5:getting_in_%6:getting_out_%7:inside_bounds_%8:local_%9",
				character.GetID(),
				logicalOccupant,
				linkedToVehicle,
				inCompartment,
				characterInVehicle,
				gettingIn,
				gettingOut,
				insideBounds,
				localOrigin);
		}

		if (memberSamples.IsEmpty())
			memberSamples = "NONE";
		return logicalOccupantCount == 0 && transitionCount == 0 && insideBoundsCount == 0;
	}

	bool AreAllProtectedMembersSafelyClear(SCR_AIGroup group, Vehicle vehicle)
	{
		int logicalOccupantCount;
		int transitionCount;
		int insideBoundsCount;
		string memberSamples;
		return InspectProtectedMemberDismountClearance(
			group,
			vehicle,
			logicalOccupantCount,
			transitionCount,
			insideBoundsCount,
			memberSamples);
	}

	protected bool IsInsideExpandedVehicleBounds(vector localOrigin, vector boundsMin, vector boundsMax)
	{
		return localOrigin[0] >= boundsMin[0] - DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[0] <= boundsMax[0] + DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[1] >= boundsMin[1] - DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[1] <= boundsMax[1] + DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[2] >= boundsMin[2] - DISMOUNT_CLEARANCE_MARGIN_METERS &&
			localOrigin[2] <= boundsMax[2] + DISMOUNT_CLEARANCE_MARGIN_METERS;
	}

	bool HasProtectedOccupant(Vehicle vehicle)
	{
		if (!vehicle)
			return false;

		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager)
			return false;

		array<BaseCompartmentSlot> compartments = {};
		manager.GetCompartments(compartments);
		foreach (BaseCompartmentSlot compartment : compartments)
		{
			if (compartment && IsProtectedCharacter(compartment.GetOccupant()))
				return true;
		}
		return false;
	}

	// Moving a whole vehicle is a last-resort server recovery. It is only safe
	// while every living managed member is settled in this vehicle, no foreign
	// occupant or compartment transition exists, and no player is linked or close
	// enough to be affected by the relocation.
	bool CanSafelyRelocateVehicle(
		SCR_AIGroup group,
		Vehicle vehicle,
		float playerProtectionRadiusMeters,
		out string rejectionReason)
	{
		rejectionReason = string.Empty;
		if (!group || !vehicle)
		{
			rejectionReason = "INVALID_INPUT";
			return false;
		}
		if (!AreAllAliveMembersSettledInVehicle(group, vehicle))
		{
			rejectionReason = "MANAGED_MEMBERS_NOT_SETTLED";
			return false;
		}

		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager)
		{
			rejectionReason = "COMPARTMENT_MANAGER_MISSING";
			return false;
		}

		array<BaseCompartmentSlot> compartments = {};
		manager.GetCompartments(compartments);
		foreach (BaseCompartmentSlot compartment : compartments)
		{
			if (!compartment)
				continue;
			IEntity occupant = compartment.GetOccupant();
			if (!occupant)
				continue;
			if (!IsAliveGroupMember(group, occupant))
			{
				rejectionReason = "FOREIGN_OR_UNMANAGED_OCCUPANT";
				return false;
			}
			ChimeraCharacter character = ChimeraCharacter.Cast(occupant);
			CompartmentAccessComponent access;
			if (character)
				access = character.GetCompartmentAccessComponent();
			if (access && (access.IsGettingIn() || access.IsGettingOut()))
			{
				rejectionReason = "COMPARTMENT_TRANSITION_ACTIVE";
				return false;
			}
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return true;
		array<int> playerIds = {};
		playerManager.GetAllPlayers(playerIds);
		float playerProtectionRadiusSq = playerProtectionRadiusMeters * playerProtectionRadiusMeters;
		foreach (int playerId : playerIds)
		{
			ChimeraCharacter player = ChimeraCharacter.Cast(
				playerManager.GetPlayerControlledEntity(playerId));
			if (!IsProtectedCharacter(player))
				continue;
			if (CompartmentAccessComponent.GetVehicleIn(player) == vehicle ||
				vector.DistanceSqXZ(player.GetOrigin(), vehicle.GetOrigin()) <= playerProtectionRadiusSq)
			{
				rejectionReason = "PLAYER_LINKED_OR_NEARBY";
				return false;
			}
		}

		return true;
	}

	// Cleanup cannot rely on compartment occupancy alone. During the first part
	// of a player GetIn animation the character is not necessarily linked to a
	// compartment yet, so deleting the vehicle can leave the client action and
	// animation without their target. A small authoritative protection radius is
	// intentionally conservative: an abandoned vehicle is not worth risking a
	// stuck player state for.
	bool InspectProtectedCleanupUse(
		Vehicle vehicle,
		float playerProtectionRadiusMeters,
		out int protectedOccupantCount,
		out int playerTransitionCount,
		out int nearbyPlayerCount,
		out string samples)
	{
		protectedOccupantCount = 0;
		playerTransitionCount = 0;
		nearbyPlayerCount = 0;
		int linkedPlayerCount;
		int sampleCount;
		samples = string.Empty;
		if (!vehicle)
		{
			samples = "INVALID_VEHICLE";
			return false;
		}

		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (manager)
		{
			array<BaseCompartmentSlot> compartments = {};
			manager.GetCompartments(compartments);
			foreach (BaseCompartmentSlot compartment : compartments)
			{
				if (!compartment)
					continue;
				IEntity occupant = compartment.GetOccupant();
				if (!IsProtectedCharacter(occupant))
					continue;

				protectedOccupantCount++;
				if (sampleCount < 8)
				{
					if (!samples.IsEmpty())
						samples += ",";
					samples += string.Format("occupant:%1", occupant.GetID());
					sampleCount++;
				}
			}
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			array<int> playerIds = {};
			playerManager.GetAllPlayers(playerIds);
			float protectionRadiusSq = playerProtectionRadiusMeters * playerProtectionRadiusMeters;
			foreach (int playerId : playerIds)
			{
				ChimeraCharacter character = ChimeraCharacter.Cast(
					playerManager.GetPlayerControlledEntity(playerId));
				if (!IsProtectedCharacter(character))
					continue;

				float distanceSq = vector.DistanceSqXZ(character.GetOrigin(), vehicle.GetOrigin());
				bool nearby = distanceSq <= protectionRadiusSq;
				bool linked = CompartmentAccessComponent.GetVehicleIn(character) == vehicle;
				CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
				bool transition = access && (access.IsGettingIn() || access.IsGettingOut()) && (nearby || linked);
				if (nearby)
					nearbyPlayerCount++;
				if (linked)
					linkedPlayerCount++;
				if (transition)
					playerTransitionCount++;
				if (!nearby && !linked && !transition)
					continue;

				if (sampleCount < 8)
				{
					if (!samples.IsEmpty())
						samples += ",";
					samples += string.Format(
						"player:%1:entity_%2:distance_m_%3:nearby_%4:linked_%5:getting_in_%6:getting_out_%7",
						playerId,
						character.GetID(),
						Math.Sqrt(distanceSq),
						nearby,
						linked,
						access && access.IsGettingIn(),
						access && access.IsGettingOut());
					sampleCount++;
				}
			}
		}

		if (samples.IsEmpty())
			samples = "NONE";
		return protectedOccupantCount == 0 && linkedPlayerCount == 0 &&
			playerTransitionCount == 0 && nearbyPlayerCount == 0;
	}

	protected bool IsProtectedCharacter(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.GetLifeState() != ECharacterLifeState.DEAD;
	}

	bool IsGroupCohesiveAroundVehicle(SCR_AIGroup group, Vehicle vehicle, float maximumDistanceMeters)
	{
		if (!group || !vehicle)
			return false;

		float maximumDistanceSq = maximumDistanceMeters * maximumDistanceMeters;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity) || CompartmentAccessComponent.GetVehicleIn(entity) == vehicle)
				continue;

			if (vector.DistanceSqXZ(entity.GetOrigin(), vehicle.GetOrigin()) > maximumDistanceSq)
				return false;
		}

		return true;
	}
}
