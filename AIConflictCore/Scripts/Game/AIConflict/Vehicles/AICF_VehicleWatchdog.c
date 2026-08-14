// Pure runtime queries used by the coordinator. Recovery policy stays in the
// coordinator so this class cannot create waypoints or mutate a group.
class AICF_VehicleWatchdog
{
	protected static const float HIDDEN_RECOVERY_LOS_RADIUS_METERS = 1200.0;
	protected static const float HIDDEN_RECOVERY_TARGET_HEIGHT_METERS = 1.5;
	protected static const float DISMOUNT_CLEARANCE_MARGIN_METERS = 0.5;
	protected static const float BOARDING_TRANSITION_SCOPE_MARGIN_METERS = 6.0;

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
			if (!character ||
				!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(character))
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

	// Hidden recovery is a server-side correction, not gameplay-visible movement.
	// Check both the currently controlled entity and the possession/main entity so
	// a GM camera or possession hand-off cannot bypass the privacy radius or the
	// conservative 1.2 km line-of-sight fence. A connected player without either
	// position makes the scan fail closed.
	bool CanApplyHiddenRecovery(
		vector source,
		vector destination,
		float playerProtectionRadiusMeters,
		out float nearestPlayerMeters,
		out string rejectionReason)
	{
		nearestPlayerMeters = -1.0;
		rejectionReason = string.Empty;
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetWorld())
		{
			rejectionReason = "SERVER_AUTHORITY_REQUIRED";
			return false;
		}
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
		{
			rejectionReason = "PLAYER_MANAGER_UNAVAILABLE";
			return false;
		}
		array<int> playerIds = {};
		playerManager.GetAllPlayers(playerIds);
		float protectionRadiusSq = playerProtectionRadiusMeters * playerProtectionRadiusMeters;
		float losRadiusSq = HIDDEN_RECOVERY_LOS_RADIUS_METERS *
			HIDDEN_RECOVERY_LOS_RADIUS_METERS;
		foreach (int playerId : playerIds)
		{
			IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
			IEntity mainEntity = SCR_PossessingManagerComponent.GetPlayerMainEntity(playerId);
			if (!controlled && !mainEntity)
			{
				rejectionReason = "PLAYER_POSITION_UNKNOWN";
				return false;
			}
			if (controlled)
			{
				float sourceDistanceSq = vector.DistanceSqXZ(controlled.GetOrigin(), source);
				float destinationDistanceSq = vector.DistanceSqXZ(controlled.GetOrigin(), destination);
				float nearestSq = Math.Min(sourceDistanceSq, destinationDistanceSq);
				float distanceMeters = Math.Sqrt(nearestSq);
				if (nearestPlayerMeters < 0 || distanceMeters < nearestPlayerMeters)
					nearestPlayerMeters = distanceMeters;
				if (nearestSq <= protectionRadiusSq)
				{
					rejectionReason = "PLAYER_CONTROLLED_ENTITY_NEARBY";
					return false;
				}
				if (nearestSq <= losRadiusSq &&
					(IsHiddenRecoveryPointVisible(controlled, source) ||
						IsHiddenRecoveryPointVisible(controlled, destination)))
				{
					rejectionReason = "PLAYER_CONTROLLED_ENTITY_HAS_LINE_OF_SIGHT";
					return false;
				}
			}
			if (mainEntity && mainEntity != controlled)
			{
				float mainSourceDistanceSq = vector.DistanceSqXZ(mainEntity.GetOrigin(), source);
				float mainDestinationDistanceSq = vector.DistanceSqXZ(mainEntity.GetOrigin(), destination);
				float mainNearestSq = Math.Min(mainSourceDistanceSq, mainDestinationDistanceSq);
				float mainDistanceMeters = Math.Sqrt(mainNearestSq);
				if (nearestPlayerMeters < 0 || mainDistanceMeters < nearestPlayerMeters)
					nearestPlayerMeters = mainDistanceMeters;
				if (mainNearestSq <= protectionRadiusSq)
				{
					rejectionReason = "PLAYER_MAIN_ENTITY_NEARBY";
					return false;
				}
				if (mainNearestSq <= losRadiusSq &&
					(IsHiddenRecoveryPointVisible(mainEntity, source) ||
						IsHiddenRecoveryPointVisible(mainEntity, destination)))
				{
					rejectionReason = "PLAYER_MAIN_ENTITY_HAS_LINE_OF_SIGHT";
					return false;
				}
			}
		}
		return true;
	}

	protected bool IsHiddenRecoveryPointVisible(IEntity observer, vector target)
	{
		if (!observer || !GetGame() || !GetGame().GetWorld())
			return true;

		vector observerPosition = observer.GetOrigin();
		ChimeraCharacter character = ChimeraCharacter.Cast(observer);
		if (character)
			observerPosition = character.EyePosition();
		else
			observerPosition[1] = observerPosition[1] + HIDDEN_RECOVERY_TARGET_HEIGHT_METERS;
		target[1] = target[1] + HIDDEN_RECOVERY_TARGET_HEIGHT_METERS;

		TraceParam trace = new TraceParam();
		trace.Start = observerPosition;
		trace.End = target;
		trace.Exclude = observer;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		trace.Flags = TraceFlags.ENTS | TraceFlags.OCEAN |
			TraceFlags.WORLD | TraceFlags.ANY_CONTACT;
		return GetGame().GetWorld().TraceMove(trace, null) >= 0.98;
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
		if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle))
		{
			rejectionReason = "VEHICLE_AUTHORITY_REQUIRED";
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

		float nearestPlayerMeters;
		return CanApplyHiddenRecovery(
			vehicle.GetOrigin(),
			vehicle.GetOrigin(),
			playerProtectionRadiusMeters,
			nearestPlayerMeters,
			rejectionReason);
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
		bool playerPositionUnknown;
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
				IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
				IEntity mainEntity = SCR_PossessingManagerComponent.GetPlayerMainEntity(playerId);
				array<IEntity> observedEntities = {};
				if (controlled)
					observedEntities.Insert(controlled);
				if (mainEntity && mainEntity != controlled)
					observedEntities.Insert(mainEntity);
				if (observedEntities.IsEmpty())
				{
					playerPositionUnknown = true;
					if (sampleCount < 8)
					{
						if (!samples.IsEmpty())
							samples += ",";
						samples += string.Format("player:%1:position_unknown", playerId);
						sampleCount++;
					}
					continue;
				}

				bool playerNearby;
				bool playerLinked;
				bool playerTransition;
				foreach (IEntity observed : observedEntities)
				{
					float distanceSq = vector.DistanceSqXZ(observed.GetOrigin(), vehicle.GetOrigin());
					bool nearby = distanceSq <= protectionRadiusSq;
					ChimeraCharacter character = ChimeraCharacter.Cast(observed);
					bool linked;
					bool transition;
					CompartmentAccessComponent access;
					if (character)
					{
						linked = CompartmentAccessComponent.GetVehicleIn(character) == vehicle;
						access = character.GetCompartmentAccessComponent();
						transition = access && (access.IsGettingIn() || access.IsGettingOut()) &&
							(nearby || linked);
					}
					playerNearby = playerNearby || nearby;
					playerLinked = playerLinked || linked;
					playerTransition = playerTransition || transition;
					if (!nearby && !linked && !transition)
						continue;
					if (sampleCount < 8)
					{
						if (!samples.IsEmpty())
							samples += ",";
						samples += string.Format(
							"player:%1:entity_%2:distance_m_%3:nearby_%4:linked_%5:getting_in_%6:getting_out_%7",
							playerId,
							observed.GetID(),
							Math.Sqrt(distanceSq),
							nearby,
							linked,
							access && access.IsGettingIn(),
							access && access.IsGettingOut());
						sampleCount++;
					}
				}
				if (playerNearby)
					nearbyPlayerCount++;
				if (playerLinked)
					linkedPlayerCount++;
				if (playerTransition)
					playerTransitionCount++;
			}
		}

		if (samples.IsEmpty())
			samples = "NONE";
		return !playerPositionUnknown && protectedOccupantCount == 0 && linkedPlayerCount == 0 &&
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
