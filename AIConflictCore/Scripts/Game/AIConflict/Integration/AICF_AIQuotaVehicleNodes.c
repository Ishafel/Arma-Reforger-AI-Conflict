// Точки script dispatch из pinned1.8.0.13. Native GetInVehicle не является
// overrideable script seam; admission выполняется до каждого известного вызова.
modded class SCR_AIChangeCompartment
{
	override ENodeResult UpdateCompartmentSwitchState(int currentState, out int newState)
	{
		if (currentState != COMPARTMENT_EMPTY || !AICF_AIQuotaVehicleDispatch.FindPolicy(m_target))
			return super.UpdateCompartmentSwitchState(currentState, newState);
		ChimeraCharacter actor = m_target;
		IEntity vehicle = m_vehicle;
		BaseCompartmentSlot seat = m_compartmentSlot;
		SCR_CompartmentAccessComponent access = m_compartmentAccess;
		AICF_AIQuotaVehicleDispatch dispatch = AICF_AIQuotaVehicleDispatch.Begin(actor, vehicle, seat);
		if (!dispatch) return ENodeResult.FAIL;
		if (!dispatch.IsCurrent() || !actor || !access || !seat || seat.GetOccupant() || m_target != actor ||
			m_vehicle != vehicle || m_compartmentSlot != seat || m_compartmentAccess != access)
		{
			dispatch.Cancel();
			return ENodeResult.FAIL;
		}
		if (actor.GetCompartmentAccessComponent() != access || !dispatch.IsCurrent())
		{
			dispatch.Cancel();
			return ENodeResult.FAIL;
		}
		bool accepted = access.GetInVehicle(vehicle, seat, m_TeleportInside, -1, ECloseDoorAfterActions.INVALID, false);
		dispatch.ObserveNativeResult(accepted);
		dispatch.Cancel();
		newState = COMPARTMENT_CHANGING;
		return ENodeResult.RUNNING;
	}
}

modded class SCR_AISetPosition
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		// Остальные vector/smart-action ветки полностью сохраняют native цепочку.
		if (GetVariableType(true, TARGET_PORT) == vector) return super.EOnTaskSimulate(owner, dt);
		if (!GetVariableType(true, TARGET_PORT).IsInherited(Managed)) return super.EOnTaskSimulate(owner, dt);
		Managed target;
		GetVariableIn(TARGET_PORT, target);
		BaseCompartmentSlot seat = BaseCompartmentSlot.Cast(target);
		if (!seat || SCR_AISmartActionComponent.Cast(target)) return super.EOnTaskSimulate(owner, dt);
		IEntity actor;
		if (!GetVariableIn(ENTITY_PORT, actor))
		{
			AIAgent agent;
			if (!GetVariableIn(AGENT_PORT, agent) || !agent) return super.EOnTaskSimulate(owner, dt);
			actor = agent.GetControlledEntity();
		}
		if (!AICF_AIQuotaVehicleDispatch.FindPolicy(actor)) return super.EOnTaskSimulate(owner, dt);
		CompartmentAccessComponent access;
		if (actor) access = CompartmentAccessComponent.Cast(actor.FindComponent(CompartmentAccessComponent));
		IEntity vehicle = seat.GetOwner();
		if (!access || !vehicle) return NodeError(this, owner, "Entity has no compartment access or target owner!");
		AICF_AIQuotaVehicleDispatch dispatch = AICF_AIQuotaVehicleDispatch.Begin(actor, vehicle, seat);
		if (!dispatch) return ENodeResult.FAIL;
		if (!dispatch.IsCurrent() || actor.FindComponent(CompartmentAccessComponent) != access)
		{
			dispatch.Cancel();
			return ENodeResult.FAIL;
		}
		bool accepted = access.GetInVehicle(vehicle, seat, true, -1, ECloseDoorAfterActions.INVALID, false);
		dispatch.ObserveNativeResult(accepted);
		dispatch.Cancel();
		if (accepted) return ENodeResult.SUCCESS;
		// Штатная bool-false ветка. Никакого повторного GetIn при RUNNING.
		if (!m_charContr)
		{
			m_charContr = CharacterControllerComponent.Cast(actor.FindComponent(CharacterControllerComponent));
			if (!m_charContr) return NodeError(this, owner, "Entity has no character controller!");
		}
		if (m_charContr.IsSwimming()) return ENodeResult.FAIL;
		if (m_charContr.IsFalling()) return ENodeResult.RUNNING;
		return ENodeResult.FAIL;
	}
}

modded class SCR_AIAllocateActionsForDefendActivity
{
	override protected bool OccupyTurret(AIAgent who, BaseCompartmentSlot slot, bool teleport)
	{
		IEntity actor;
		if (who) actor = who.GetControlledEntity();
		if (!AICF_AIQuotaVehicleDispatch.FindPolicy(actor)) return super.OccupyTurret(who, slot, teleport);
		if (!slot) return false;
		IEntity vehicle = slot.GetVehicle();
		if (!vehicle) return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.FindOnNearestParent(vehicle, vehicle);
		if (!usage) return false;
		SCR_AIGroup group = m_groupOwner;
		SCR_MailboxComponent mailbox = m_Mailbox;
		SCR_AIDefendActivity activity = m_RelatedActivity;
		SCR_DefendWaypoint waypoint = m_RelatedWaypoint;
		if (!group || !mailbox || who.GetParentGroup() != group) return false;
		AICF_AIQuotaVehicleDispatch dispatch = AICF_AIQuotaVehicleDispatch.Begin(actor, vehicle, slot);
		if (!dispatch) return false;
		if (!dispatch.IsCurrent() || !who || who.GetControlledEntity() != actor || slot.GetVehicle() != vehicle)
		{
			dispatch.Cancel();
			return false;
		}
		bool accepted;
		bool hasNativeResult;
		if (teleport)
		{
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(actor.FindComponent(CompartmentAccessComponent));
			if (!access || !dispatch.IsCurrent())
			{
				dispatch.Cancel();
				return false;
			}
			accepted = access.GetInVehicle(vehicle, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
			hasNativeResult = true;
			if (!accepted) Print("Unable to teleport to Turret", LogLevel.WARNING);
		}
		// Native messaging и ownership добавляются только для той же admission.
		if (!dispatch.IsCurrent() || m_groupOwner != group || m_Mailbox != mailbox ||
			m_RelatedActivity != activity || m_RelatedWaypoint != waypoint)
		{
			if (hasNativeResult) dispatch.ObserveNativeResult(accepted);
			dispatch.Cancel();
			return false;
		}
		SCR_AIMessage_GetIn message = SCR_AIMessage_GetIn.Create(vehicle, null, EAICompartmentType.Turret, m_bWaypointRelated, m_fPriorityLevel, waypoint, activity, slot);
		mailbox.RequestBroadcast(message, who);
		if (dispatch.IsCurrent() && m_groupOwner == group && group.GetGroupUtilityComponent())
			group.GetGroupUtilityComponent().AddUsableVehicle(usage);
		if (hasNativeResult) dispatch.ObserveNativeResult(accepted);
		dispatch.Cancel();
		return true;
	}
}
