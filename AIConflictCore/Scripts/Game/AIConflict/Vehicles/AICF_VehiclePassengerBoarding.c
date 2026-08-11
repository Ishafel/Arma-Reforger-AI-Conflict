// Exact cargo actions are isolated from the already large vehicle coordinator.
// The service reserves every required cargo compartment before adding the first
// action, keeps one token per living member and owns bounded retry/cancellation.
class AICF_VehiclePassengerBoardingController
{
	protected static const int MAX_RETRIES = 2;
	protected ref AICF_VehicleWatchdog m_Watchdog;

	void AICF_VehiclePassengerBoardingController(AICF_VehicleWatchdog watchdog)
	{
		m_Watchdog = watchdog;
	}

	bool Start(
		AICF_VehicleRuntime runtime,
		SCR_AIGroup group,
		out int issuedCount)
	{
		issuedCount = 0;
		if (!runtime || !group || !runtime.GetVehicle() || !m_Watchdog)
			return false;
		CancelAll(runtime);

		array<AIAgent> pendingAgents = {};
		array<IEntity> pendingEntities = {};
		array<SCR_AIUtilityComponent> pendingUtilities = {};
		array<BaseCompartmentSlot> assignedCompartments = {};
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;
			if (CompartmentAccessComponent.GetVehicleIn(entity) == runtime.GetVehicle())
			{
				if (!IsSupportedSettledCompartment(runtime, entity))
					return false;
				continue;
			}
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
				agent.FindComponent(SCR_AIUtilityComponent));
			if (!utility || utility.m_OwnerEntity != entity)
				return false;
			BaseCompartmentSlot compartment;
			if (!FindAvailableCargoCompartment(
				runtime.GetVehicle(),
				entity,
				assignedCompartments,
				compartment))
			{
				return false;
			}
			pendingAgents.Insert(agent);
			pendingEntities.Insert(entity);
			pendingUtilities.Insert(utility);
			assignedCompartments.Insert(compartment);
		}
		if (pendingAgents.IsEmpty())
			return true;

		for (int reserveIndex = 0; reserveIndex < assignedCompartments.Count(); reserveIndex++)
		{
			BaseCompartmentSlot reservedCompartment = assignedCompartments[reserveIndex];
			IEntity reservedEntity = pendingEntities[reserveIndex];
			reservedCompartment.SetReserved(reservedEntity);
			if (!reservedCompartment.IsReservedBy(reservedEntity))
			{
				RollbackReservations(assignedCompartments, pendingEntities);
				return false;
			}
		}

		for (int actionIndex = 0; actionIndex < pendingAgents.Count(); actionIndex++)
		{
			SCR_AIGetInVehicle action = new SCR_AIGetInVehicle(
				pendingUtilities[actionIndex],
				null,
				runtime.GetVehicle(),
				assignedCompartments[actionIndex],
				EAICompartmentType.Cargo,
				SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE,
				SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
			pendingUtilities[actionIndex].AddAction(action);
			runtime.TrackPassengerBoardingAction(
				pendingAgents[actionIndex],
				action,
				assignedCompartments[actionIndex],
				pendingEntities[actionIndex]);
			issuedCount++;
		}
		return issuedCount == pendingAgents.Count();
	}

	bool Maintain(
		AICF_VehicleRuntime runtime,
		SCR_AIGroup group,
		out string failureReason)
	{
		failureReason = string.Empty;
		if (!runtime || !group || !runtime.GetVehicle() || !m_Watchdog)
		{
			failureReason = "PASSENGER_INPUT_INVALID";
			return false;
		}

		Vehicle vehicle = runtime.GetVehicle();
		for (int tokenIndex = runtime.GetPassengerBoardingActionCount() - 1; tokenIndex >= 0; tokenIndex--)
		{
			AICF_VehiclePassengerActionToken token = runtime.GetPassengerBoardingAction(tokenIndex);
			AIAgent tokenAgent;
			if (token)
				tokenAgent = token.GetAgent();
			IEntity tokenEntity;
			if (tokenAgent)
				tokenEntity = tokenAgent.GetControlledEntity();
			if (!tokenAgent || tokenAgent.GetParentGroup() != group ||
				!AICF_GroupRuntime.IsAliveCharacter(tokenEntity))
			{
				Cancel(runtime, token);
				continue;
			}

			CompartmentAccessComponent access = ResolveAccess(tokenEntity);
			if (m_Watchdog.IsMemberSettledInVehicle(tokenEntity, vehicle))
			{
				// A generic same-vehicle check is not enough here: the action owns an
				// exact cargo reservation and must settle in that same compartment.
				// Accepting a pilot/turret or another cargo seat would hide a stolen
				// reservation and make the next member's retry nondeterministic.
				if (!access || access.GetCompartment() != token.GetCompartment())
				{
					failureReason = string.Format(
						"PASSENGER_MEMBER_%1_WRONG_COMPARTMENT",
						tokenEntity.GetID());
					return false;
				}
				ReleaseTracking(runtime, token);
				continue;
			}
			if (CompartmentAccessComponent.GetVehicleIn(tokenEntity) == vehicle)
				continue;
			if (access && (access.IsGettingIn() || access.IsGettingOut()))
				continue;

			SCR_AIGetInVehicle action = token.GetAction();
			EAIActionState state = EAIActionState.FAILED;
			if (action)
				state = action.GetActionState();
			if (action && state != EAIActionState.COMPLETED && state != EAIActionState.FAILED)
				continue;

			int retryCount = token.GetRetryCount();
			if (retryCount >= MAX_RETRIES)
			{
				failureReason = string.Format("PASSENGER_MEMBER_%1_ACTION_TERMINAL", tokenEntity.GetID());
				return false;
			}
			Cancel(runtime, token);
			if (!IssueOne(runtime, tokenAgent, retryCount + 1))
			{
				failureReason = string.Format("PASSENGER_MEMBER_%1_RETRY_UNAVAILABLE", tokenEntity.GetID());
				return false;
			}
			AICF_Stage3Diagnostics.Warning(
				"PASSENGER_BOARDING_REISSUED",
				string.Format(
					"%1 member=%2 retry=%3 maximum_retries=%4 previous_state=%5 transition_fenced=1",
					runtime.DescribeContext("EXACT_CARGO_ACTION_TERMINAL"),
					tokenEntity.GetID(),
					retryCount + 1,
					MAX_RETRIES,
					typename.EnumToString(EAIActionState, state)));
		}

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != group)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(entity))
				continue;
			AICF_VehiclePassengerActionToken trackedToken = runtime.FindPassengerBoardingAction(agent);
			CompartmentAccessComponent access = ResolveAccess(entity);
			// The token pass above exclusively owns members whose exact action is
			// still pending. GetVehicleIn can already identify the target during the
			// normal enter animation, before the member satisfies the settled gate;
			// do not misclassify that transition as an unsupported pre-mounted seat.
			if (trackedToken || (access && (access.IsGettingIn() || access.IsGettingOut())))
				continue;
			if (CompartmentAccessComponent.GetVehicleIn(entity) == vehicle)
			{
				if (!IsSupportedSettledCompartment(runtime, entity))
				{
					failureReason = string.Format(
						"PASSENGER_MEMBER_%1_UNSUPPORTED_COMPARTMENT",
						entity.GetID());
					return false;
				}
				continue;
			}
			if (!IssueOne(runtime, agent, 0))
			{
				failureReason = string.Format("PASSENGER_MEMBER_%1_ACTION_UNAVAILABLE", entity.GetID());
				return false;
			}
		}
		return true;
	}

	void Cancel(
		AICF_VehicleRuntime runtime,
		AICF_VehiclePassengerActionToken token)
	{
		if (!runtime || !token)
			return;
		AIAgent agent = token.GetAgent();
		IEntity currentEntity;
		SCR_AIUtilityComponent utility;
		if (agent)
		{
			currentEntity = agent.GetControlledEntity();
			utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		}
		SCR_AIGetInVehicle action = token.GetAction();
		if (action)
		{
			EAIActionState state = action.GetActionState();
			bool linked;
			if (currentEntity && runtime.GetVehicle() &&
				CompartmentAccessComponent.GetVehicleIn(currentEntity) == runtime.GetVehicle())
			{
				linked = true;
			}
			if (state != EAIActionState.COMPLETED && state != EAIActionState.FAILED && !linked &&
				currentEntity == token.GetReservedEntity() &&
				AICF_GroupRuntime.IsAliveCharacter(currentEntity) && utility && utility.m_OwnerEntity == currentEntity)
			{
				action.Fail();
			}
		}
		ReleaseReservation(token);
		runtime.RemovePassengerBoardingAction(token);
	}

	void ReleaseTracking(
		AICF_VehicleRuntime runtime,
		AICF_VehiclePassengerActionToken token)
	{
		if (!runtime || !token)
			return;
		ReleaseReservation(token);
		runtime.RemovePassengerBoardingAction(token);
	}

	int CancelAll(AICF_VehicleRuntime runtime)
	{
		if (!runtime)
			return 0;
		int count = runtime.GetPassengerBoardingActionCount();
		for (int index = runtime.GetPassengerBoardingActionCount() - 1; index >= 0; index--)
			Cancel(runtime, runtime.GetPassengerBoardingAction(index));
		return count;
	}

	void ClearTracking(AICF_VehicleRuntime runtime)
	{
		if (!runtime)
			return;
		for (int index = runtime.GetPassengerBoardingActionCount() - 1; index >= 0; index--)
			ReleaseTracking(runtime, runtime.GetPassengerBoardingAction(index));
		runtime.ClearPassengerBoardingActions();
	}

	string Describe(AICF_VehicleRuntime runtime)
	{
		if (!runtime)
			return "INVALID_RUNTIME";
		string details = string.Format("count=%1", runtime.GetPassengerBoardingActionCount());
		for (int index = 0; index < runtime.GetPassengerBoardingActionCount(); index++)
		{
			AICF_VehiclePassengerActionToken token = runtime.GetPassengerBoardingAction(index);
			if (!token)
				continue;
			AIAgent agent = token.GetAgent();
			IEntity entity;
			SCR_AIUtilityComponent utility;
			if (agent)
			{
				entity = agent.GetControlledEntity();
				utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			}
			string entityId = "NONE";
			if (entity)
				entityId = entity.GetID().ToString();
			string state = "MISSING";
			if (token.GetAction())
				state = typename.EnumToString(EAIActionState, token.GetAction().GetActionState());
			bool isCurrent;
			if (utility && token.GetAction() && utility.GetCurrentAction() == token.GetAction())
				isCurrent = true;
			BaseCompartmentSlot assigned = token.GetCompartment();
			int assignedManager = -1;
			int assignedSlot = -1;
			bool reserved;
			if (assigned)
			{
				assignedManager = assigned.GetCompartmentMgrID();
				assignedSlot = assigned.GetCompartmentSlotID();
				if (token.GetReservedEntity() && assigned.IsReservedBy(token.GetReservedEntity()))
					reserved = true;
			}
			CompartmentAccessComponent access = ResolveAccess(entity);
			BaseCompartmentSlot actual;
			if (access)
				actual = access.GetCompartment();
			int actualManager = -1;
			int actualSlot = -1;
			if (actual)
			{
				actualManager = actual.GetCompartmentMgrID();
				actualSlot = actual.GetCompartmentSlotID();
			}
			details += string.Format(
				",member_%1:state_%2:retry_%3:current_%4:assigned_%5/%6:reserved_%7:actual_%8/%9",
				entityId,
				state,
				token.GetRetryCount(),
				isCurrent,
				assignedManager,
				assignedSlot,
				reserved,
				actualManager,
				actualSlot);
		}
		return details;
	}

	protected bool IssueOne(
		AICF_VehicleRuntime runtime,
		AIAgent agent,
		int retryCount)
	{
		if (!runtime || !runtime.GetVehicle() || !agent)
			return false;
		IEntity entity = agent.GetControlledEntity();
		if (!AICF_GroupRuntime.IsAliveCharacter(entity))
			return false;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(
			agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || utility.m_OwnerEntity != entity)
			return false;
		array<BaseCompartmentSlot> excluded = {};
		BaseCompartmentSlot compartment;
		if (!FindAvailableCargoCompartment(runtime.GetVehicle(), entity, excluded, compartment))
			return false;
		compartment.SetReserved(entity);
		if (!compartment.IsReservedBy(entity))
			return false;
		SCR_AIGetInVehicle action = new SCR_AIGetInVehicle(
			utility,
			null,
			runtime.GetVehicle(),
			compartment,
			EAICompartmentType.Cargo,
			SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE,
			SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
		utility.AddAction(action);
		runtime.TrackPassengerBoardingAction(agent, action, compartment, entity, retryCount);
		return true;
	}

	protected bool FindAvailableCargoCompartment(
		Vehicle vehicle,
		IEntity entity,
		array<BaseCompartmentSlot> excluded,
		out BaseCompartmentSlot selected)
	{
		selected = null;
		if (!vehicle || !entity)
			return false;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!manager)
			return false;
		array<BaseCompartmentSlot> compartments = {};
		manager.GetCompartments(compartments);
		foreach (BaseCompartmentSlot compartment : compartments)
		{
			CargoCompartmentSlot cargo = CargoCompartmentSlot.Cast(compartment);
			if (!cargo || excluded.Contains(compartment) || !compartment.IsCompartmentAccessible() ||
				compartment.GetOccupant() || compartment.IsReserved() || compartment.IsGetInLockedFor(entity))
			{
				continue;
			}
			selected = compartment;
			return true;
		}
		return false;
	}

	protected void RollbackReservations(
		array<BaseCompartmentSlot> compartments,
		array<IEntity> entities)
	{
		for (int index = 0; index < compartments.Count() && index < entities.Count(); index++)
		{
			if (compartments[index] && entities[index] && compartments[index].IsReservedBy(entities[index]))
				compartments[index].SetReserved(null);
		}
	}

	protected void ReleaseReservation(AICF_VehiclePassengerActionToken token)
	{
		if (!token || !token.GetCompartment() || !token.GetReservedEntity())
			return;
		if (token.GetCompartment().IsReservedBy(token.GetReservedEntity()))
			token.GetCompartment().SetReserved(null);
	}

	protected CompartmentAccessComponent ResolveAccess(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return null;
		return character.GetCompartmentAccessComponent();
	}

	protected bool IsSupportedSettledCompartment(
		AICF_VehicleRuntime runtime,
		IEntity entity)
	{
		if (!runtime || !entity || !runtime.GetVehicle() ||
			!m_Watchdog.IsMemberSettledInVehicle(entity, runtime.GetVehicle()))
		{
			return false;
		}

		CompartmentAccessComponent access = ResolveAccess(entity);
		BaseCompartmentSlot compartment;
		if (access)
			compartment = access.GetCompartment();
		if (!compartment)
			return false;
		if (PilotCompartmentSlot.Cast(compartment) || CargoCompartmentSlot.Cast(compartment))
			return true;
		return runtime.GetKind() == AICF_EVehicleKind.ARMED_LIGHT &&
			TurretCompartmentSlot.Cast(compartment);
	}
}
