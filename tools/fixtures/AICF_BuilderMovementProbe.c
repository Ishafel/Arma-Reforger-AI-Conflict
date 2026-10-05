// Только isolated stage: инъекция результата BT в живую строительную группу.
// Проверяет owner/fencing и bounded return; физический возврат остаётся production.
class AICF_BuilderMovementProbeNode : SCR_AIProcessFailedMovementResult
{
	bool Inject(SCR_AIGroup group, int result = EMoveError.UNKNOWN, int handler = AIGroupMovementComponent.DEFAULT_HANDLER_ID)
	{
		OnInit(group);
		return AICF_HandleFailedMovement(result, handler, true, group.GetLeaderEntity().GetOrigin());
	}
}

modded class AICF_BaseBuilderService
{
	protected bool m_bAICFBuilderProbeDone;
	protected int m_iAICFBuilderProbeFailures;
	protected SCR_AIGroup m_AICFProbeReturningGroup;
	protected AICF_BaseBuilder m_AICFProbeCallbackBuilder;

	protected void AICF_InvalidateMoveFailure(int result, IEntity vehicle, bool related, vector location)
	{
		m_AICFProbeCallbackBuilder.m_iGeneration++;
	}

	protected void AICF_BuilderCheck(string name, bool passed)
	{
		if (!passed) m_iAICFBuilderProbeFailures++;
		Print(string.Format("[AICF][BUILDER_MOVEMENT_PROBE] case=%1 passed=%2", name, passed));
	}

	override protected void UpdateBuilder(AICF_BaseBuilder builder)
	{
		super.UpdateBuilder(builder);
		string enabled;
		if (m_bAICFBuilderProbeDone || !System.GetCLIParam("aicfBuilderMovementProbe", enabled) || enabled != "1" ||
			!builder.m_Target || !builder.m_Waypoint || !IsWorkerValid(builder) ||
			builder.m_Group.GetCurrentWaypoint() != builder.m_Waypoint)
			return;
		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(builder.m_Group.GetGroupUtilityComponent().GetCurrentAction());
		if (!activity || activity.m_RelatedWaypoint != builder.m_Waypoint || !builder.m_Group.GetGroupUtilityComponent().GetExecutedAction())
			return;
		m_bAICFBuilderProbeDone = true;
		m_AICFProbeReturningGroup = builder.m_Group;
		AIWaypoint original = builder.m_Waypoint;
		AICF_BaseBuilderMoveFailure snapshot = new AICF_BaseBuilderMoveFailure(builder);
		builder.m_MoveFailure = snapshot;
		AICF_BuilderCheck("CURRENT_IDENTITY", IsMoveFailureCurrent(builder, snapshot));
		builder.m_iGeneration++;
		AICF_BuilderCheck("STALE_GENERATION", !IsMoveFailureCurrent(builder, snapshot));
		builder.m_iGeneration--;
		builder.m_Waypoint = null;
		AICF_BuilderCheck("STALE_WAYPOINT", !IsMoveFailureCurrent(builder, snapshot));
		builder.m_Waypoint = original;
		builder.m_bReturning = !builder.m_bReturning;
		AICF_BuilderCheck("STALE_PHASE", !IsMoveFailureCurrent(builder, snapshot));
		builder.m_bReturning = !builder.m_bReturning;
		EntityID characterId = builder.m_CharacterId;
		builder.m_CharacterId = EntityID.INVALID;
		AICF_BuilderCheck("STALE_CHARACTER", !IsMoveFailureCurrent(builder, snapshot));
		builder.m_CharacterId = characterId;
		SCR_CampaignBuildingCompositionComponent target = builder.m_Target;
		builder.m_Target = null;
		AICF_BuilderCheck("STALE_TARGET", !IsMoveFailureCurrent(builder, snapshot));
		builder.m_Target = target;
		Faction faction = builder.m_Faction;
		builder.m_Faction = null;
		AICF_BuilderCheck("STALE_FACTION", !IsMoveFailureCurrent(builder, snapshot));
		builder.m_Faction = faction;
		builder.m_MoveFailure = null;
		AICF_BuilderCheck("NULL_WAYPOINT", !ReportFailedMovement(builder.m_Group, null));
		AICF_BuilderMovementProbeNode node = new AICF_BuilderMovementProbeNode();
		AICF_BuilderCheck("UNRELATED_ERROR", !node.Inject(builder.m_Group, EMoveError.WAITING_ON_NAVLINK));
		AICF_BuilderCheck("FOREIGN_HANDLER", !node.Inject(builder.m_Group, EMoveError.UNKNOWN, -2));
		SCR_AIGroupUtilityComponent utility = builder.m_Group.GetGroupUtilityComponent();
		AIActionBase beforeAction = utility.GetExecutedAction();
		EAIActionState beforeState = beforeAction.GetActionState();
		m_AICFProbeCallbackBuilder = builder;
		utility.GetOnMoveFailed().Insert(AICF_InvalidateMoveFailure);
		bool handled = node.Inject(builder.m_Group);
		utility.GetOnMoveFailed().Remove(AICF_InvalidateMoveFailure);
		AICF_BuilderCheck("SYNCHRONOUS_STALE_ACTION", handled && beforeAction.GetActionState() == beforeState &&
			!IsMoveFailureCurrent(builder, builder.m_MoveFailure));
		builder.m_iGeneration--;
		builder.m_MoveFailure = null;
		m_AICFProbeCallbackBuilder = null;
		AICF_BuilderCheck("BT_HANDOFF", node.Inject(builder.m_Group));
		AICF_BuilderCheck("DEFERRED_OWNER", builder.m_Target && builder.m_Waypoint == original && builder.m_MoveFailure);
		vector home = builder.m_Base.GetMasterProvider().GetOwner().GetOrigin();
		int now = System.GetTickCount();
		ConsumeMoveFailure(builder, home, now);
		AICF_BuilderCheck("WORK_ABORT_RETURN", !builder.m_Target && !builder.m_MoveFailure && builder.m_bFailedWorkReturning && !builder.m_bRetiring);

		// Два контролируемых отказа return без Teleport и без удаления персонажа.
		// Временный home нужен только для проверки terminal policy вдали от дома.
		vector blockedHome = builder.m_Character.GetOrigin() + Vector(50, 0, 50);
		for (int attempt; attempt < 2; attempt++)
		{
			builder.m_bReturning = true;
			bool placed = m_Planner.SetBuilderWaypoint(builder, builder.m_Character.GetOrigin(), 5);
			AICF_BuilderCheck("RETURN_WAYPOINT_" + attempt, placed);
			if (!placed) break;
			builder.m_MoveFailure = new AICF_BaseBuilderMoveFailure(builder);
			ConsumeMoveFailure(builder, blockedHome, now);
		}
		AICF_BuilderCheck("RETURN_BOUNDED_HOLD", builder.m_bReturnBlocked && builder.m_iReturnMoveFailures == 2 &&
			SCR_DefendWaypoint.Cast(builder.m_Waypoint) && builder.m_Group && IsWorkerValid(builder));
		AIWaypoint hold = builder.m_Waypoint;
		ReturnHome(builder, blockedHome, now + 1000000);
		AICF_BuilderCheck("NO_TIMER_DELETE_OR_RETRY", builder.m_Group && builder.m_Waypoint == hold && builder.m_bReturnBlocked);
		ReturnHome(builder, home, now);
		AICF_BuilderCheck("HOME_CONTEXT_REARMS", !builder.m_bReturnBlocked && builder.m_iReturnMoveFailures == 0);
		Print(string.Format("[AICF][BUILDER_MOVEMENT_PROBE_DONE] failures=%1", m_iAICFBuilderProbeFailures));
	}

	override protected bool Retire(AICF_BaseBuilder builder, string reason)
	{
		bool tracked = m_AICFProbeReturningGroup && builder.m_Group == m_AICFProbeReturningGroup;
		bool result = super.Retire(builder, reason);
		if (tracked && result)
			Print(string.Format("[AICF][BUILDER_MOVEMENT_PROBE_RETIRED] reason=%1", reason));
		return result;
	}
}
