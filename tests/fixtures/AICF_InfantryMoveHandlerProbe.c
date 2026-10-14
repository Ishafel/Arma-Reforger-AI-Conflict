// Только isolated stage: реальная пешая группа и свободная машина в registry.
// Вход failed movement инъецируется; native BT wiring проверяется отдельно.
modded class AICF_GroupSlot
{
	override int GetDeploymentSize()
	{
		string enabled;
		if (GetSlotId() == 0 && System.GetCLIParam("aicfInfantryMoveHandlerProbe", enabled) && enabled == "1")
		{
			SetDesiredSize(9);
			return 9;
		}
		return super.GetDeploymentSize();
	}

	bool AICF_HandlerProbeStaleFences()
	{
		m_iSpawnGeneration++;
		bool generation = !HasFailedMovement();
		m_iSpawnGeneration--;
		m_iStrategicAssignmentRevision++;
		bool assignment = !HasFailedMovement();
		m_iStrategicAssignmentRevision--;
		return generation && assignment && HasFailedMovement();
	}
}

class AICF_InfantryMoveHandlerProbeNode : SCR_AIProcessFailedMovementResult
{
	bool Classify(SCR_AIGroup group, int handler)
	{
		OnInit(group);
		return AICF_IsInfantryMoveHandler(handler);
	}

	bool Inject(SCR_AIGroup group, int handler, bool related)
	{
		OnInit(group);
		return AICF_HandleFailedMovement(EMoveError.UNKNOWN, handler, related, group.GetLeaderEntity().GetOrigin());
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFHandlerProbeStarted;
	protected int m_iAICFHandlerProbePolls;
	protected int m_iAICFHandlerProbeChecks;
	protected int m_iAICFHandlerProbeFailures;
	protected IEntity m_AICFHandlerProbeVehicle;
	protected SCR_AIVehicleUsageComponent m_AICFHandlerProbeUsage;
	protected SCR_AIGroupVehicleManager m_AICFHandlerProbeManager;
	protected SCR_AIGroup m_AICFHandlerProbeGroup;
	protected IEntity m_AICFHandlerProbeMember;
	protected vector m_vAICFHandlerProbeTransform[4];
	protected int m_iAICFHandlerProbePhase;

	protected void AICF_HandlerCheck(string name, bool passed)
	{
		m_iAICFHandlerProbeChecks++;
		if (!passed) m_iAICFHandlerProbeFailures++;
		Print(string.Format("[AICF][INFANTRY_HANDLER_CHECK] name=%1 passed=%2", name, passed));
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFHandlerProbeStarted || !System.GetCLIParam("aicfInfantryMoveHandlerProbe", enabled) || enabled != "1")
			return;
		m_bAICFHandlerProbeStarted = true;
		GetGame().GetCallqueue().CallLater(AICF_HandlerPrepare, 1000, false);
	}

	protected void AICF_HandlerPrepare()
	{
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		SCR_AIGroup group = slot.GetGroup();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		AICF_HandlerCheck("ROSTER_NINE", group && leader && AICF_GroupRuntime.CountAliveAgents(group) == 9);
		AICF_HandlerCheck("MUSTER_COMPLETE", slot.CompleteInfantryMusterIfReady() && !slot.IsRecruitingInfantry());
		if (!leader) { AICF_HandlerFinish(); return; }
		AICF_HandlerCheck("POINT_ORDER", m_OrderPlanner.AssignPlayerPointOrder(slot, m_USFaction, leader.GetOrigin() + "100 0 100"));
		GetGame().GetCallqueue().CallLater(AICF_HandlerRun, 250, true);
	}

	protected void AICF_HandlerRun()
	{
		if (m_iAICFHandlerProbePhase > 0) { AICF_HandlerSeat(); return; }
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		SCR_AIGroup group = slot.GetGroup();
		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		SCR_AIMoveActivity activity = SCR_AIMoveActivity.Cast(utility.GetCurrentAction());
		if (!activity || activity.m_RelatedWaypoint != slot.GetWaypoint() || group.GetCurrentWaypoint() != slot.GetWaypoint())
		{
			m_iAICFHandlerProbePolls++;
			if (m_iAICFHandlerProbePolls < 120) return;
			AICF_HandlerCheck("CURRENT_MOVE_ACTIVITY", false);
			AICF_HandlerFinish();
			return;
		}
		GetGame().GetCallqueue().Remove(AICF_HandlerRun);
		AICF_HandlerCheck("CURRENT_MOVE_ACTIVITY", true);
		AICF_InfantryMoveHandlerProbeNode node = new AICF_InfantryMoveHandlerProbeNode();
		AICF_HandlerCheck("FOOT_SENTINEL", node.Classify(group, -1));
		AICF_HandlerCheck("FOREIGN_HANDLER", !node.Classify(group, -2) && !node.Classify(group, 1));
		AICF_HandlerCheck("UNRELATED_REJECTED", !node.Inject(group, -1, false) && !slot.HasFailedMovement());
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = group.GetLeaderEntity().GetOrigin() + "40 0 40";
		m_AICFHandlerProbeVehicle = GetGame().SpawnEntityPrefab(Resource.Load(AICF_ContentProfile.GetActive().GetFIAGarrisonPrefab(false)), GetGame().GetWorld(), params);
		if (m_AICFHandlerProbeVehicle)
			m_AICFHandlerProbeUsage = SCR_AIVehicleUsageComponent.Cast(m_AICFHandlerProbeVehicle.FindComponent(SCR_AIVehicleUsageComponent));
		m_AICFHandlerProbeManager = utility.m_VehicleMgr;
		SCR_AIGroupVehicle registered;
		if (m_AICFHandlerProbeUsage && m_AICFHandlerProbeManager)
			registered = m_AICFHandlerProbeManager.TryAddVehicle(m_AICFHandlerProbeUsage);
		AICF_HandlerCheck("UNASSIGNED_REGISTRY", registered && registered.GetSubgroupHandleId() == -1 && m_AICFHandlerProbeManager.FindVehicleBySubgroupId(-1) == registered);
		if (!registered) { AICF_HandlerFinish(); return; }
		AICF_HandlerCheck("FOOT_WITH_UNUSED_VEHICLE", node.Classify(group, -1));
		registered.SetSubgroupHandleId(1);
		AICF_HandlerCheck("ASSIGNED_VEHICLE_REJECTED", !node.Classify(group, 1));
		registered.SetSubgroupHandleId(-1);
		m_AICFHandlerProbeGroup = group;
		m_AICFHandlerProbeMember = group.GetLeaderEntity();
		m_AICFHandlerProbeMember.GetWorldTransform(m_vAICFHandlerProbeTransform);
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(m_AICFHandlerProbeMember.FindComponent(CompartmentAccessComponent));
		BaseCompartmentSlot seat;
		if (access) seat = access.FindFreeCompartment(m_AICFHandlerProbeVehicle, ECompartmentType.CARGO);
		bool boarded = seat && access.GetInVehicle(m_AICFHandlerProbeVehicle, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, true);
		AICF_HandlerCheck("BOARD_REQUEST", boarded);
		if (!boarded) { AICF_HandlerFinish(); return; }
		m_iAICFHandlerProbePhase = 1;
		m_iAICFHandlerProbePolls = 0;
		GetGame().GetCallqueue().CallLater(AICF_HandlerRun, 250, true);
	}

	protected void AICF_HandlerSeat()
	{
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		if (slot.GetGroup() != m_AICFHandlerProbeGroup || !AICF_GroupRuntime.IsAliveCharacter(m_AICFHandlerProbeMember))
		{ AICF_HandlerCheck("SEAT_IDENTITY", false); AICF_HandlerFinish(); return; }
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(m_AICFHandlerProbeMember.FindComponent(CompartmentAccessComponent));
		m_iAICFHandlerProbePolls++;
		if (m_iAICFHandlerProbePolls > 80) { AICF_HandlerCheck("SEAT_TIMEOUT", false); AICF_HandlerFinish(); return; }
		AICF_InfantryMoveHandlerProbeNode node = new AICF_InfantryMoveHandlerProbeNode();
		if (m_iAICFHandlerProbePhase == 1)
		{
			if (!access.IsInCompartment() || CompartmentAccessComponent.GetVehicleIn(m_AICFHandlerProbeMember) != m_AICFHandlerProbeVehicle) return;
			AICF_HandlerCheck("PHYSICAL_OCCUPANT_REJECTED", !node.Classify(m_AICFHandlerProbeGroup, -1) && !node.Inject(m_AICFHandlerProbeGroup, -1, true) && !slot.HasFailedMovement());
			AICF_HandlerCheck("EXIT_REQUEST", access.GetOutVehicle_NoDoor(m_vAICFHandlerProbeTransform, false, true, true));
			m_iAICFHandlerProbePhase = 2;
			m_iAICFHandlerProbePolls = 0;
			return;
		}
		if (access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || CompartmentAccessComponent.GetVehicleIn(m_AICFHandlerProbeMember)) return;
		if (m_iAICFHandlerProbePhase == 2)
		{
			// Stock compartment callbacks могут завершить старый point waypoint.
			// Новый Move создаётся planner, а не подменой utility/action в fixture.
			AICF_HandlerCheck("POINT_RESTORED_AFTER_EXIT", m_OrderPlanner.AssignPlayerPointOrder(slot, m_USFaction, m_AICFHandlerProbeMember.GetOrigin() + "110 0 110"));
			m_iAICFHandlerProbePhase = 3;
			m_iAICFHandlerProbePolls = 0;
			return;
		}
		// OnCompartmentLeft может снять запись. Восстанавливаем только test registry.
		SCR_AIGroupVehicle registered = m_AICFHandlerProbeManager.TryAddVehicle(m_AICFHandlerProbeUsage);
		registered.SetSubgroupHandleId(-1);
		SCR_AIMoveActivity activity = SCR_AIMoveActivity.Cast(m_AICFHandlerProbeGroup.GetGroupUtilityComponent().GetCurrentAction());
		if (!activity || activity.m_RelatedWaypoint != slot.GetWaypoint()) return;
		AICF_HandlerCheck("FOOT_AFTER_EXIT", node.Classify(m_AICFHandlerProbeGroup, -1));
		AICF_HandlerCheck("FOREIGN_WAYPOINT_REJECTED", !slot.ReportFailedMovement(m_AICFHandlerProbeGroup, m_USState.GetSlot(1).GetWaypoint()));
		AICF_HandlerCheck("FAILURE_REPORTED", node.Inject(m_AICFHandlerProbeGroup, -1, true));
		AICF_HandlerCheck("FAILURE_VISIBLE", slot.HasFailedMovement() && m_OrderPlanner.GetOrderFailureReason(slot, m_USFaction) == "WAYPOINT_MOVE_FAILED");
		AICF_HandlerCheck("STALE_FAILURE_FENCES", slot.AICF_HandlerProbeStaleFences());
		AICF_HandlerFinish();
	}

	protected void AICF_HandlerCleanup()
	{
		GetGame().GetCallqueue().Remove(AICF_HandlerPrepare);
		GetGame().GetCallqueue().Remove(AICF_HandlerRun);
		if (m_AICFHandlerProbeManager && m_AICFHandlerProbeUsage)
			m_AICFHandlerProbeManager.RemoveVehicle(m_AICFHandlerProbeUsage);
		m_AICFHandlerProbeManager = null;
		m_AICFHandlerProbeUsage = null;
		if (m_AICFHandlerProbeVehicle && (!m_AICFHandlerProbeMember || CompartmentAccessComponent.GetVehicleIn(m_AICFHandlerProbeMember) != m_AICFHandlerProbeVehicle))
			SCR_EntityHelper.DeleteEntityAndChildren(m_AICFHandlerProbeVehicle);
		m_AICFHandlerProbeVehicle = null;
	}

	protected void AICF_HandlerFinish()
	{
		AICF_HandlerCleanup();
		Print(string.Format("[AICF][INFANTRY_HANDLER_FINISHED] checks=%1 failures=%2", m_iAICFHandlerProbeChecks, m_iAICFHandlerProbeFailures));
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		AICF_HandlerCleanup();
		super.Stop(cleanupEntities);
	}
}
