// Только isolated stage, запуск -aicfNorthFailureProbe 1. Production BT inputs
// передаются helper напрямую: wiring портов остаётся отдельным static gate.
class AICF_FailedMoveProbeNode : SCR_AIProcessFailedMovementResult
{
	bool Inject(SCR_AIGroup group, int result, int handler, bool related)
	{
		OnInit(group);
		return AICF_HandleFailedMovement(result, handler, related, group.GetLeaderEntity().GetOrigin());
	}
}

class AICF_RadiusProbeNode : SCR_AICalculateNextCombatMovePos
{
	vector Sample(float distance, vector center) { return RandomizeDestinationPos(distance, center); }
}

class AICF_ExcludeProbeNode : SCR_AIGetRandomPointWithExclude
{
	bool Sample(float radius, float exclude, out vector point)
	{
		return FindPosition2D(point, "100 1 100", radius, "100 1 100", exclude);
	}
}

class AICF_BackoffProbeOrder : AICF_ConstructionOrder
{
	// Исключительно synthetic order: не смешиваем его с production ledger.
	override void Log(string eventName, string extra = "") {}
}

class AICF_BackoffProbePlanner : AICF_ConstructionPlanner
{
	bool CheckBackoff()
	{
		AICF_ConstructionBaseState state = new AICF_ConstructionBaseState();
		array<int> expected = {60000, 120000, 240000, 480000, 480000};
		foreach (int index, int delay : expected)
		{
			AICF_BackoffProbeOrder order = new AICF_BackoffProbeOrder();
			order.m_sToken = "north-backoff-fixture-" + index;
			order.m_eType = AICF_EConstructionType.HEAVY_DEPOT;
			order.m_iAttempts = 256;
			order.m_iSearchOffset = state.m_aSearchOffsets[4];
			state.m_Order = order;
			int before = System.GetTickCount();
			Cancel(state, "SEARCH_BUDGET_EXHAUSTED");
			int remaining = state.m_aSearchRetryAt[4] - before;
			if (remaining < delay || remaining > delay + 100 || state.m_aSearchRetryAt[0] != 0 ||
				state.m_aSearchOffsets[4] != (index + 1) * 256 || state.m_Order || order.m_bPaid)
				return false;
		}
		return true;
	}
}

modded class SCR_AICommsHandler
{
	void AICF_ProbeWaiting(SCR_AITalkRequest request)
	{
		m_CurrentRequest = request;
		m_eState = SCR_EAICommunicationState.WAITING;
	}
}

modded class AICF_GroupSlot
{
	bool AICF_ProbeFailedMoveFences()
	{
		m_iSpawnGeneration++;
		bool generation = !HasFailedMovement();
		m_iSpawnGeneration--;
		m_iStrategicAssignmentRevision++;
		bool assignment = !HasFailedMovement();
		m_iStrategicAssignmentRevision--;
		return generation && assignment && HasFailedMovement();
	}
	void AICF_ExpireRouteCooldown()
	{
		for (int index; index < m_aRejectedRouteUntil.Count(); index++)
			m_aRejectedRouteUntil[index] = 0;
	}
	int AICF_ProbeRouteDeadline(SCR_CampaignMilitaryBaseComponent target)
	{
		int index = m_aRejectedRouteTargets.Find(target);
		if (index < 0) return 0;
		return m_aRejectedRouteUntil[index];
	}
}

modded class AICF_MatchController
{
	protected bool m_bAICFNorthProbe;
	protected int m_iAICFNorthChecks;
	protected int m_iAICFNorthFailures;
	protected ref array<ref AICF_ConstructionMetadata> m_aAICFNorthGeometry = {};
	protected int m_iAICFNorthGeometryCursor;
	protected vector m_vAICFNorthStart;
	protected SCR_AIGroup m_AICFNorthGroup;
	protected IEntity m_AICFNorthLeader;
	protected float m_fAICFNorthMaxDisplacement;
	protected int m_iAICFNorthMovingSamples;

	protected void AICF_NorthCheck(string name, bool passed)
	{
		m_iAICFNorthChecks++;
		if (!passed) m_iAICFNorthFailures++;
		Print(string.Format("[AICF][NORTH_FAILURE_CHECK] name=%1 passed=%2", name, passed));
	}

	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		string enabled;
		if (!m_bRosterReady || m_bAICFNorthProbe || !System.GetCLIParam("aicfNorthFailureProbe", enabled) || enabled != "1")
			return;
		m_bAICFNorthProbe = true;
		GetGame().GetCallqueue().CallLater(AICF_RunNorthProbe, 10000, false);
	}

	protected void AICF_RunNorthProbe()
	{
		SCR_CampaignBuildingManagerComponent manager = SCR_CampaignBuildingManagerComponent.Cast(m_Campaign.FindComponent(SCR_CampaignBuildingManagerComponent));
		array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
		foreach (SCR_CampaignFaction faction : factions)
		{
			string side = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
			for (int type; type < AICF_EConstructionType.COUNT; type++)
			{
				ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab(side, type);
				AICF_ConstructionMetadata metadata = new AICF_ConstructionMetadata();
				metadata.Load(prefab, type, manager, faction);
				Print(string.Format("[AICF][NORTH_METADATA_PROBE] faction=%1 type=%2 valid=%3 reason=%4 prefab=%5", side, type, metadata.m_bValid, metadata.m_sInvalidReason, prefab));
				AICF_NorthCheck(side + "_METADATA_" + type, metadata.m_bValid);
				if (metadata.m_bValid)
					m_aAICFNorthGeometry.Insert(metadata);
			}
		}
		AICF_RadiusProbeNode radiusNode = new AICF_RadiusProbeNode();
		vector center = "100 1 100";
		AICF_NorthCheck("ZERO_COMBAT_RADIUS", radiusNode.Sample(0, center) == center);
		AICF_NorthCheck("POSITIVE_COMBAT_RADIUS", vector.DistanceXZ(radiusNode.Sample(100, center), center) <= 10.01);
		AICF_ExcludeProbeNode excludeNode = new AICF_ExcludeProbeNode();
		vector point;
		AICF_NorthCheck("ZERO_EXCLUDE_RADIUS", !excludeNode.Sample(0, 0, point));
		AICF_NorthCheck("EMPTY_EXCLUDE_RANGE", !excludeNode.Sample(10, 10, point));
		AICF_NorthCheck("VALID_EXCLUDE_RANGE", excludeNode.Sample(10, 2, point) && vector.DistanceXZ(point, center) >= 1.99);
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		SCR_AIGroup group = slot.GetGroup();
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(group);
		m_AICFNorthGroup = group;
		m_AICFNorthLeader = leader;
		m_vAICFNorthStart = leader.GetOrigin();
		SCR_AITalkRequest request = new SCR_AITalkRequest(ECommunicationType.REPORT_CONTACT, null, vector.Zero, 0, true, true, SCR_EAITalkRequestPreset.MANDATORY);
		SCR_AICommsHandler comms = new SCR_AICommsHandler(leader, group.GetLeaderAgent());
		comms.AddRequest(request);
		comms.Update(16);
		AICF_NorthCheck("STALE_CONTACT_FAILED", request.m_eState == SCR_EAITalkRequestState.FAILED);
		request.m_Entity = leader;
		request.m_vPosition = leader.GetOrigin();
		AICF_NorthCheck("VALID_CONTACT_ALLOWED", !SCR_AICommsHandler.AICF_IsInvalidContact(request));
		SCR_AITalkRequest waiting = new SCR_AITalkRequest(ECommunicationType.REPORT_CONTACT, null, leader.GetOrigin(), 0, true, true, SCR_EAITalkRequestPreset.MANDATORY);
		comms.AICF_ProbeWaiting(waiting);
		comms.Update(16);
		AICF_NorthCheck("WAITING_CONTACT_FAILED", waiting.m_eState == SCR_EAITalkRequestState.FAILED);
		AICF_BackoffProbePlanner backoff = new AICF_BackoffProbePlanner();
		AICF_NorthCheck("BOUNDED_SEARCH_BACKOFF_AND_CURSOR", backoff.CheckBackoff());
		AICF_FailedMoveProbeNode node = new AICF_FailedMoveProbeNode();
		AICF_NorthCheck("NAVLINK_UNCHANGED", !node.Inject(group, EMoveError.WAITING_ON_NAVLINK, AIGroupMovementComponent.DEFAULT_HANDLER_ID, true));
		AICF_NorthCheck("VEHICLE_UNCHANGED", !node.Inject(group, EMoveError.UNKNOWN, AIGroupMovementComponent.DEFAULT_HANDLER_ID + 1, true));
		AICF_NorthCheck("UNRELATED_UNCHANGED", !node.Inject(group, EMoveError.UNKNOWN, AIGroupMovementComponent.DEFAULT_HANDLER_ID, false));
		AICF_NorthCheck("OTHER_NEGATIVE_HANDLER_UNCHANGED", !node.Inject(group, EMoveError.UNKNOWN, -2, true));
		AICF_NorthCheck("UNASSIGNED_INFANTRY_HANDLER", node.Inject(group, EMoveError.UNKNOWN, -1, true));
		AICF_NorthCheck("UNKNOWN_HANDLED", node.Inject(group, EMoveError.UNKNOWN, AIGroupMovementComponent.DEFAULT_HANDLER_ID, true));
		AICF_NorthCheck("PENDING_MOVE_FAILURE", slot.HasFailedMovement());
		AICF_NorthCheck("MOVE_FAILURE_FENCES", slot.AICF_ProbeFailedMoveFences());
		AICF_NorthCheck("FOREIGN_WAYPOINT_REJECTED", !slot.ReportFailedMovement(group, m_USState.GetSlot(1).GetWaypoint()));
		AICF_NorthCheck("FAILURE_VISIBLE_TO_PLANNER", m_OrderPlanner.GetOrderFailureReason(slot, m_USFaction) == "WAYPOINT_MOVE_FAILED");
		SCR_CampaignMilitaryBaseComponent target = slot.GetTargetBase();
		slot.DeferFailedRouteTarget(target);
		slot.DeferFailedRouteTarget(m_USFaction.GetMainBase());
		AICF_NorthCheck("MULTIPLE_REJECTED_TARGETS_RETAINED", slot.IsRouteTargetDeferred(target) && slot.IsRouteTargetDeferred(m_USFaction.GetMainBase()));
		AICF_NorthCheck("FAILED_TARGET_COOLDOWN", slot.IsRouteTargetDeferred(target) && !m_OrderPlanner.RebuildCurrentOrder(slot, m_USFaction, "PROBE_DEFERRED"));
		slot.AICF_ExpireRouteCooldown();
		AICF_NorthCheck("COOLDOWN_EXPIRES", !slot.IsRouteTargetDeferred(target));
		AICF_GroupSlot memory = new AICF_GroupSlot(0, AICF_EGroupRole.ATTACK);
		for (int nodeIndex; nodeIndex < m_ObjectiveGraph.GetNodeCount(); nodeIndex++)
			memory.DeferFailedRouteTarget(m_ObjectiveGraph.GetNode(nodeIndex).GetBase());
		string selectionMode;
		AICF_NorthCheck("SELECTOR_CANNOT_FALLBACK_TO_REJECTED", !m_TargetSelector.SelectAttackTarget(m_ObjectiveGraph, m_USFaction, selectionMode, target, 0, memory));
		memory.RecordPlayerStrategicIntent(target);
		AICF_NorthCheck("PLAYER_INTENT_OVERRIDES_COOLDOWN", !memory.IsRouteTargetDeferred(target));
		AICF_GroupSlot restoreSlot = m_USState.GetSlot(1);
		SCR_CampaignMilitaryBaseComponent restoreTarget = restoreSlot.GetStrategicIntentTargetBase();
		restoreSlot.DeferFailedRouteTarget(restoreTarget);
		int restoreDeadline = restoreSlot.AICF_ProbeRouteDeadline(restoreTarget);
		m_OrderPlanner.ClearOrder(restoreSlot);
		bool held = m_OrderPlanner.AssignOrder(restoreSlot, m_USFaction, m_ObjectiveGraph, m_TargetSelector, "PROBE_LIFECYCLE_RESTORE");
		AICF_NorthCheck("COOLDOWN_LIFECYCLE_RESTORES_HOLD", held && restoreSlot.IsTemporaryRouteReplanHold() && restoreSlot.GetWaypoint() && restoreSlot.GetGroup().GetCurrentWaypoint() == restoreSlot.GetWaypoint());
		AICF_NorthCheck("RESTORE_DOES_NOT_EXTEND_COOLDOWN", restoreDeadline > 0 && restoreSlot.AICF_ProbeRouteDeadline(restoreTarget) == restoreDeadline);
		restoreSlot.AICF_ExpireRouteCooldown();
		restoreSlot.ClearTemporaryRouteReplanHold();
		AICF_NorthCheck("RESTORE_AFTER_COOLDOWN", m_OrderPlanner.AssignOrder(restoreSlot, m_USFaction, m_ObjectiveGraph, m_TargetSelector, "PROBE_COOLDOWN_EXPIRED") && !restoreSlot.IsTemporaryRouteReplanHold());
		GetGame().GetCallqueue().CallLater(AICF_NorthGeometry, 100, true);
		GetGame().GetCallqueue().CallLater(AICF_SampleNorthMovement, 1000, true);
		// Без альтернативной базы законный hold длится до route cooldown 180 с.
		// Проверяем движение после этого окна, не ослабляя порог 30 м.
		GetGame().GetCallqueue().CallLater(AICF_FinishNorthProbe, 270000, false);
	}

	protected void AICF_NorthGeometry()
	{
		if (m_iAICFNorthGeometryCursor >= m_aAICFNorthGeometry.Count())
		{
			GetGame().GetCallqueue().Remove(AICF_NorthGeometry);
			return;
		}
		AICF_ConstructionMetadata metadata = m_aAICFNorthGeometry[m_iAICFNorthGeometryCursor];
		int result = metadata.StepGeometry(8, 4);
		if (result == 0)
			return;
		AICF_NorthCheck("GEOMETRY_" + m_iAICFNorthGeometryCursor, result == 1);
		m_iAICFNorthGeometryCursor++;
	}

	protected void AICF_SampleNorthMovement()
	{
		if (!AICF_GroupRuntime.IsAliveCharacter(m_AICFNorthLeader))
			return;
		float displacement = vector.DistanceXZ(m_AICFNorthLeader.GetOrigin(), m_vAICFNorthStart);
		m_fAICFNorthMaxDisplacement = Math.Max(m_fAICFNorthMaxDisplacement, displacement);
		if (displacement >= 30)
			m_iAICFNorthMovingSamples++;
	}

	protected void AICF_FinishNorthProbe()
	{
		AICF_NorthCheck("GEOMETRY_FINISHED", m_iAICFNorthGeometryCursor == m_aAICFNorthGeometry.Count());
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_AICFNorthGroup);
		float displacement;
		if (leader)
			displacement = vector.DistanceXZ(leader.GetOrigin(), m_vAICFNorthStart);
		Print(string.Format("[AICF][NORTH_FAILURE_MOVEMENT] displacement_m=%1 max_original_leader_displacement_m=%2 samples_beyond_30m=%3 original_leader=%4",
			displacement, m_fAICFNorthMaxDisplacement, m_iAICFNorthMovingSamples, m_AICFNorthLeader));
		// Возврат за recruitment не отменяет уже наблюдавшееся движение.
		// Два раздельных samples относятся к тому же физическому персонажу.
		AICF_NorthCheck("PHYSICAL_MOVEMENT_AFTER_FAILURE", m_fAICFNorthMaxDisplacement >= 30 && m_iAICFNorthMovingSamples >= 2);
		Print(string.Format("[AICF][NORTH_FAILURE_FINISHED] checks=%1 failures=%2", m_iAICFNorthChecks, m_iAICFNorthFailures));
		GetGame().RequestClose();
	}

	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame())
		{
			GetGame().GetCallqueue().Remove(AICF_RunNorthProbe);
			GetGame().GetCallqueue().Remove(AICF_FinishNorthProbe);
			GetGame().GetCallqueue().Remove(AICF_NorthGeometry);
			GetGame().GetCallqueue().Remove(AICF_SampleNorthMovement);
		}
		foreach (AICF_ConstructionMetadata metadata : m_aAICFNorthGeometry)
			metadata.ReleasePreview();
		super.Stop(cleanupEntities);
	}
}
