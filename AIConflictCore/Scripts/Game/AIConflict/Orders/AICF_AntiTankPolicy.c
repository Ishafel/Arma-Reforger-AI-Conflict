// Тактическая политика пешего AI с выбранным РПГ против наземной бронетехники.
// Не меняет skill, восприятие, приказы группы, threat memory или свойства боеприпаса.
class AICF_AntiTankPolicy
{
	static const float ENGAGEMENT_RANGE = 400.0;
	static const float AIM_WINDOW_SECONDS = 10.0;
	static const float REPOSITION_WAIT_SECONDS = 2.0;
	static const float MOVE_SECONDS = 2.0;
	static const float COVER_SEARCH_METERS = 15.0;
	static const float MAX_AIM_TOLERANCE = 0.25;

	static bool IsArmoredTarget(BaseTarget target)
	{
		if (!target || !target.GetTargetEntity() || target.GetTargetCategory() != ETargetCategory.ENEMY)
			return false;
		EAIUnitType type = target.GetUnitType();
		return type == EAIUnitType.UnitType_VehicleMedium || type == EAIUnitType.UnitType_VehicleHeavy;
	}

	static bool Applies(SCR_AICombatComponent combat, BaseTarget target)
	{
		if (!Replication.IsServer() || !AICF_MatchController.GetActiveController() || !combat ||
			combat.GetSelectedWeaponType() != EWeaponType.WT_ROCKETLAUNCHER ||
			target != combat.GetCurrentTarget() || !IsArmoredTarget(target))
			return false;
		IEntity owner = combat.GetOwner();
		if (!AICF_GroupRuntime.IsAliveCharacter(owner)) return false;
		CharacterControllerComponent control = CharacterControllerComponent.Cast(owner.FindComponent(CharacterControllerComponent));
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(owner.FindComponent(CompartmentAccessComponent));
		return control && !control.IsPlayerControlled() && rpl && rpl.IsMaster() && access &&
			!access.IsInCompartment() && !access.IsGettingIn() && !access.IsGettingOut();
	}

	static bool CanReposition(SCR_AIUtilityComponent utility, SCR_AICombatComponent combat, BaseTarget target)
	{
		return Applies(combat, target) && utility && utility.m_ThreatSystem &&
			utility.m_ThreatSystem.GetState() != EAIThreatState.THREATENED &&
			!utility.ShouldKeepFormation() && combat.GetCombatMode() != EAIGroupCombatMode.HOLD_FIRE;
	}

	static bool HasFiringSolution(SCR_AICombatComponent combat, BaseTarget target)
	{
		return combat.IsTargetVisible(target) && target.GetDistance() >= combat.GetSelectedWeaponMinDist() &&
			target.GetDistance() <= combat.GetSelectedWeaponMaxDist() && !combat.IsFriendlyInAim();
	}
}

modded class SCR_AICombatComponent
{
	override void GetSelectedWeaponProperties(out float outMinDistance, out float outMaxDistance, out bool outDirectDamage)
	{
		super.GetSelectedWeaponProperties(outMinDistance, outMaxDistance, outDirectDamage);
		if (outMinDistance < AICF_AntiTankPolicy.ENGAGEMENT_RANGE && AICF_AntiTankPolicy.Applies(this, GetCurrentTarget()))
			outMaxDistance = AICF_AntiTankPolicy.ENGAGEMENT_RANGE;
	}

	override float GetSelectedWeaponMaxDist()
	{
		float nativeRange = super.GetSelectedWeaponMaxDist();
		if (GetSelectedWeaponMinDist() < AICF_AntiTankPolicy.ENGAGEMENT_RANGE && AICF_AntiTankPolicy.Applies(this, GetCurrentTarget()))
			return AICF_AntiTankPolicy.ENGAGEMENT_RANGE;
		return nativeRange;
	}
}

modded class SCR_AIGetAimErrorOffset
{
	override float GetTolerance(IEntity observer, IEntity target, float angularSize, float distance, EWeaponType weaponType)
	{
		float nativeTolerance = super.GetTolerance(observer, target, angularSize, distance, weaponType);
		if (weaponType != EWeaponType.WT_ROCKETLAUNCHER || !m_CombatComponent ||
			!AICF_AntiTankPolicy.Applies(m_CombatComponent, m_CombatComponent.GetCurrentTarget()) ||
			m_CombatComponent.GetCurrentTarget().GetTargetEntity() != target)
			return nativeTolerance;
		return Math.Max(MINIMAL_TOLERANCE, Math.Min(nativeTolerance * 0.5, AICF_AntiTankPolicy.MAX_AIM_TOLERANCE));
	}
}

modded class SCR_AICombatMoveLogic_Attack
{
	protected bool AICF_CanRepositionAT()
	{
		return AICF_AntiTankPolicy.CanReposition(m_Utility, m_CombatComp, m_Target);
	}

	protected override bool ResolveFailMoveIfNoCover()
	{
		if (AICF_CanRepositionAT() && !AICF_AntiTankPolicy.HasFiringSolution(m_CombatComp, m_Target))
			return false;
		return super.ResolveFailMoveIfNoCover();
	}

	protected override bool MoveToNextPosCondition()
	{
		if (!AICF_CanRepositionAT()) return super.MoveToNextPosCondition();
		if (m_State.IsExecutingRequest() || m_CharacterController.IsReloading()) return false;
		// Штатная ветка MoveFromTarget выше этой проверки сохраняет минимальную дистанцию.
		float wait = AICF_AntiTankPolicy.REPOSITION_WAIT_SECONDS;
		if (AICF_AntiTankPolicy.HasFiringSolution(m_CombatComp, m_Target))
			wait = AICF_AntiTankPolicy.AIM_WINDOW_SECONDS;
		return m_State.m_fTimerStopped_s >= wait;
	}

	protected override void PushRequestMove()
	{
		if (!AICF_CanRepositionAT())
		{
			super.PushRequestMove();
			return;
		}
		SCR_AICombatMoveRequest_Move rq = new SCR_AICombatMoveRequest_Move();
		rq.m_eReason = SCR_EAICombatMoveReason.STANDARD;
		rq.m_vTargetPos = ResolveRequestTargetPos();
		// Сохраняем штатные границы waypoint/защиты базы и направление обхода.
		ResolveMoveRequestMovePosAndDir(rq.m_vTargetPos, rq.m_vMovePos, rq.m_vAvoidStraightPathDir,
			rq.m_eDirection, rq.m_fCoverSearchSectorHalfAngleRad);
		if (AICF_AntiTankPolicy.HasFiringSolution(m_CombatComp, m_Target) &&
			vector.DistanceSq(rq.m_vMovePos, rq.m_vTargetPos) < 0.01)
		{
			// Не подменяем возврат к waypoint, когда боец вышел за его радиус.
			// При движении к видимому танку вместо сближения ищем соседнее укрытие.
			rq.m_eDirection = SCR_EAICombatMoveDirection.ANYWHERE;
			rq.m_vAvoidStraightPathDir = vector.Zero;
			rq.m_fCoverSearchSectorHalfAngleRad = Math.PI;
		}
		rq.m_bTryFindCover = true;
		rq.m_bUseCoverSearchDirectivity = true;
		rq.m_bCheckCoverVisibility = true;
		rq.m_bFailIfNoCover = ResolveFailMoveIfNoCover();
		rq.m_fCoverSearchDistMin = 0;
		rq.m_fCoverSearchDistMax = AICF_AntiTankPolicy.COVER_SEARCH_METERS;
		rq.m_fMoveDuration_s = AICF_AntiTankPolicy.MOVE_SECONDS;
		rq.m_eMovementType = EMovementType.RUN;
		rq.m_eStanceMoving = ECharacterStance.STAND;
		rq.m_eStanceEnd = ECharacterStance.CROUCH;
		rq.m_bAimAtTarget = false;
		rq.m_bAimAtTargetEnd = true;
		// Request принадлежит stock combat move state; новых callbacks/таймеров нет.
		m_State.ApplyNewRequest(rq);
	}
}
