// Только изолированный stage: один ПТ-боец и штатный гарнизонный БТР на линии 350 м.
modded class AICF_FIAGarrisonService
{
	static AICF_FIAGarrison AICF_ATProbeTarget()
	{
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (g.m_bReady && !g.m_bTank && g.VehicleIdentity()) return g;
		}
		return null;
	}
}

modded class SCR_AICombatComponent
{
	float AICF_ATProbeNativeRange() { return m_fSelectedWeaponMaxDist; }
}

class AICF_ATProbeAim : SCR_AIGetAimErrorOffset
{
	bool Check(SCR_AICombatComponent combat, IEntity target)
	{
		IEntity observer = combat.GetOwner();
		m_CombatComponent = null;
		float rifleBefore = GetTolerance(observer, target, 1, 350, EWeaponType.WT_RIFLE);
		float rocketBefore = GetTolerance(observer, target, 1, 350, EWeaponType.WT_ROCKETLAUNCHER);
		m_CombatComponent = combat;
		float rifleAfter = GetTolerance(observer, target, 1, 350, EWeaponType.WT_RIFLE);
		float rocketAfter = GetTolerance(observer, target, 1, 350, EWeaponType.WT_ROCKETLAUNCHER);
		bool pass = rifleBefore == rifleAfter && rocketAfter < rocketBefore && rocketAfter <= 0.25;
		Print(string.Format("[AICF][AT_ENGAGEMENT_AIM] pass=%1 rifle_before=%2 rifle_after=%3 rocket_before=%4 rocket_after=%5", pass, rifleBefore, rifleAfter, rocketBefore, rocketAfter));
		return pass;
	}
}

// Проверяет production task на отдельном move state: не отправляет запрос живому AI.
class AICF_ATProbeMove : SCR_AICombatMoveLogic_Attack
{
	bool Check(SCR_AICombatComponent combat, BaseTarget target)
	{
		m_CombatComp = combat;
		m_Target = target;
		m_MyEntity = combat.GetOwner();
		m_CharacterController = CharacterControllerComponent.Cast(m_MyEntity.FindComponent(CharacterControllerComponent));
		AIControlComponent control = AIControlComponent.Cast(m_MyEntity.FindComponent(AIControlComponent));
		m_Utility = SCR_AIUtilityComponent.Cast(control.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
		SCR_AICombatMoveState isolatedState = new SCR_AICombatMoveState();
		m_State = isolatedState;
		if (!AICF_CanRepositionAT() || !AICF_AntiTankPolicy.HasFiringSolution(combat, target)) return false;
		m_State.m_fTimerStopped_s = 9;
		bool waitsForAim = !MoveToNextPosCondition();
		m_State.m_fTimerStopped_s = 10;
		bool canMove = MoveToNextPosCondition();
		PushRequestMove();
		SCR_AICombatMoveRequest_Move rq = SCR_AICombatMoveRequest_Move.Cast(m_State.GetRequest());
		bool pass = waitsForAim && canMove && rq && rq.m_fMoveDuration_s == 2 &&
			!rq.m_bAimAtTarget && rq.m_bAimAtTargetEnd && rq.m_eStanceEnd == ECharacterStance.CROUCH;
		Print(string.Format("[AICF][AT_ENGAGEMENT_MOVE] pass=%1 waits_at_9=%2 moves_at_10=%3", pass, waitsForAim, canMove));
		return pass;
	}
}

modded class AICF_MatchController
{
	override void Start(SCR_GameModeCampaign campaign, AICF_Stage1Config immutableConfig = null, AICF_ContentProfile contentProfile = null, string mapKey = "UNKNOWN")
	{
		super.Start(campaign, immutableConfig, contentProfile, mapKey);
		string enabled;
		Print(string.Format("[AICF][AT_ENGAGEMENT_START] enabled=%1", System.GetCLIParam("aicfAntiTankProbe", enabled)));
		if (enabled == "1") GetGame().GetCallqueue().CallLater(AICF_ATProbeTick, 1000, true);
	}

	override protected void Stop(bool cleanupEntities)
	{
		GetGame().GetCallqueue().Remove(AICF_ATProbeTick);
		if (m_AICFATProbeEffect) m_AICFATProbeEffect.GetOnWeaponFired().Remove(AICF_ATProbeShot);
		super.Stop(cleanupEntities);
	}

	protected int m_iAICFATProbeTick;
	protected SCR_AIGroup m_AICFATProbeGroup;
	protected IEntity m_AICFATProbeShooter;
	protected AICF_FIAGarrison m_AICFATProbeTarget;
	protected bool m_bAICFATProbePlaced;
	protected bool m_bAICFATProbeSelected;
	protected bool m_bAICFATProbeFired;
	protected bool m_bAICFATProbeContracts;
	protected int m_iAICFATProbePreviousAmmo = -1;
	protected float m_fAICFATProbeShotDistance;
	protected BaseMuzzleComponent m_AICFATProbeMuzzle;
	protected AIWaypoint m_AICFATProbeWaypoint;
	protected SCR_MuzzleEffectComponent m_AICFATProbeEffect;

	protected void AICF_ATProbeShot(IEntity effectEntity, BaseMuzzleComponent muzzle, IEntity projectileEntity)
	{
		if (muzzle != m_AICFATProbeMuzzle || !projectileEntity || !m_AICFATProbeShooter || !m_AICFATProbeTarget) return;
		SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(m_AICFATProbeShooter.FindComponent(SCR_AICombatComponent));
		BaseTarget target = combat.GetCurrentTarget();
		if (!target || target.GetTargetEntity() != m_AICFATProbeTarget.m_Vehicle) return;
		m_bAICFATProbeFired = true;
		m_fAICFATProbeShotDistance = vector.Distance(m_AICFATProbeShooter.GetOrigin(), m_AICFATProbeTarget.m_Vehicle.GetOrigin());
		Print(string.Format("[AICF][AT_ENGAGEMENT_PROJECTILE] distance=%1 projectile=%2", m_fAICFATProbeShotDistance, projectileEntity));
	}

	protected bool AICF_ATProbeLine(out vector shooter, out vector target)
	{
		BaseWorld world = GetGame().GetWorld();
		vector center = m_USFaction.GetMainBase().GetOwner().GetOrigin();
		vector other = m_USSRFaction.GetMainBase().GetOwner().GetOrigin();
		if (other[2] > center[2]) center = other;
		for (int ix; ix <= 12; ix++)
		{
			int x = (ix + 1) / 2;
			if (ix % 2 == 0) x = -x;
			for (int iz; iz <= 12; iz++)
			{
				int z = (iz + 1) / 2;
				if (iz % 2 == 0) z = -z;
				shooter = center + Vector(x * 100, 0, z * 100);
				shooter[1] = world.GetSurfaceY(shooter[0], shooter[2]) + 0.1;
				if (shooter[1] < 3) continue;
				for (int direction; direction < 8; direction++)
				{
					float angle = direction * Math.PI / 4;
					target = shooter + Vector(Math.Sin(angle) * 350, 0, Math.Cos(angle) * 350);
					target[1] = world.GetSurfaceY(target[0], target[2]) + 0.2;
					if (target[1] < 3 || Math.AbsFloat(target[1] - shooter[1]) > 8) continue;
					TraceParam trace = new TraceParam();
					trace.Start = shooter + Vector(0, 0.8, 0);
					trace.End = target + Vector(0, 0.8, 0);
					trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
					trace.LayerMask = EPhysicsLayerPresets.Projectile;
					if (world.TraceMove(trace, null) == 1) return true;
				}
			}
		}
		return false;
	}

	void AICF_ATProbeTick()
	{
		string enabled;
		if (!System.GetCLIParam("aicfAntiTankProbe", enabled) || !m_bRosterReady) return;
		m_iAICFATProbeTick++;
		if (m_iAICFATProbeTick == 1)
		{
			Print("[AICF][AT_ENGAGEMENT_STARTED] roster_ready=1");
			m_AICFATProbeGroup = m_GroupSpawner.SpawnGroup(m_USSRFaction, m_USSRFaction.GetMainBase(), 0, 1, false);
			if (m_AICFATProbeGroup)
			{
				string role;
				m_AICFATProbeGroup.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
				m_AICFATProbeGroup.m_aUnitPrefabSlots.Clear();
				m_AICFATProbeGroup.m_aUnitPrefabSlots.Insert(m_GroupSpawner.ResolveRecruitPrefab(m_USSRFaction, 3, role));
				m_AICFATProbeGroup.AICF_TrackMemberPositions();
				m_GroupSpawner.BeginRosterSpawn(m_AICFATProbeGroup, 1);
			}
		}
		if (!m_bAICFATProbePlaced && m_iAICFATProbeTick > 25 && m_AICFATProbeGroup)
		{
			m_AICFATProbeShooter = m_AICFATProbeGroup.AICF_GetSpawnMember(0);
			m_AICFATProbeTarget = AICF_FIAGarrisonService.AICF_ATProbeTarget();
			if (m_iAICFATProbeTick % 10 == 0) Print(string.Format("[AICF][AT_ENGAGEMENT_WAIT] shooter=%1 target=%2", m_AICFATProbeShooter, m_AICFATProbeTarget));
			if (m_AICFATProbeShooter && m_AICFATProbeTarget)
			{
				vector shooterPos, targetPos;
				if (AICF_ATProbeLine(shooterPos, targetPos))
				{
					m_AICFATProbeShooter.SetOrigin(shooterPos);
					vector angles = (targetPos - shooterPos).VectorToAngles();
					m_AICFATProbeShooter.SetAngles(angles);
					EntitySpawnParams wpParams = new EntitySpawnParams();
					wpParams.TransformMode = ETransformMode.WORLD;
					wpParams.Transform[3] = shooterPos;
					m_AICFATProbeWaypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et"), GetGame().GetWorld(), wpParams));
					m_AICFATProbeWaypoint.SetCompletionRadius(10);
					m_AICFATProbeGroup.AddWaypoint(m_AICFATProbeWaypoint);
					m_AICFATProbeTarget.m_bStaticDefense = true;
					m_AICFATProbeTarget.m_vHome = targetPos;
					m_AICFATProbeTarget.m_vPosition = targetPos;
					Physics targetPhysics = m_AICFATProbeTarget.m_Vehicle.GetPhysics();
					targetPhysics.SetVelocity(vector.Zero);
					targetPhysics.SetAngularVelocity(vector.Zero);
					targetPhysics.ChangeSimulationState(SimulationState.COLLISION);
					m_AICFATProbeTarget.m_Vehicle.SetOrigin(targetPos);
					m_AICFATProbeTarget.m_Vehicle.SetAngles(vector.Zero);
					array<AIAgent> crew = {};
					m_AICFATProbeTarget.m_Group.GetAgents(crew);
					foreach (AIAgent member : crew) member.DeactivateAI();
					AIControlComponent ai = AIControlComponent.Cast(m_AICFATProbeShooter.FindComponent(AIControlComponent));
					ai.GetAIAgent().SetPermanentLOD(0);
					ai.GetAIAgent().ActivateAI();
					SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(ai.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
					utility.LookAt(targetPos + Vector(0, 1.5, 0), 60);
					DamageManagerComponent damage = DamageManagerComponent.Cast(m_AICFATProbeShooter.FindComponent(DamageManagerComponent));
					damage.EnableDamageHandling(false);
					m_bAICFATProbePlaced = true;
					Print(string.Format("[AICF][AT_ENGAGEMENT_PLACED] shooter=%1 target=%2 distance=%3", shooterPos, targetPos, vector.Distance(shooterPos, targetPos)));
				}
			}
		}
		if (m_bAICFATProbePlaced)
		{
			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(m_AICFATProbeShooter.FindComponent(SCR_AICombatComponent));
			BaseTarget target = combat.GetCurrentTarget();
			BaseWeaponComponent launcher = combat.m_WeaponTargetSelector.FindWeaponOfType(EWeaponType.WT_ROCKETLAUNCHER, false);
			if (!m_AICFATProbeMuzzle && launcher)
			{
				array<BaseMuzzleComponent> muzzles = {};
				launcher.GetMuzzlesList(muzzles);
				if (!muzzles.IsEmpty()) m_AICFATProbeMuzzle = muzzles[0];
				m_AICFATProbeEffect = SCR_MuzzleEffectComponent.Cast(launcher.GetOwner().FindComponent(SCR_MuzzleEffectComponent));
				if (m_AICFATProbeEffect) m_AICFATProbeEffect.GetOnWeaponFired().Insert(AICF_ATProbeShot);
			}
			float distance = vector.Distance(m_AICFATProbeShooter.GetOrigin(), m_AICFATProbeTarget.m_Vehicle.GetOrigin());
			int ammo = -1;
			if (m_AICFATProbeMuzzle) ammo = m_AICFATProbeMuzzle.GetAmmoCount();
			if (m_iAICFATProbePreviousAmmo > 0 && ammo == 0)
			{
				Print(string.Format("[AICF][AT_ENGAGEMENT_AMMO_USED] distance=%1", distance));
			}
			m_iAICFATProbePreviousAmmo = ammo;
			bool applies = AICF_AntiTankPolicy.Applies(combat, target);
			if (applies && distance > 300) m_bAICFATProbeSelected = true;
			if (applies && !m_bAICFATProbeContracts && AICF_AntiTankPolicy.HasFiringSolution(combat, target))
			{
				AICF_ATProbeAim aimProbe = new AICF_ATProbeAim();
				AICF_ATProbeMove moveProbe = new AICF_ATProbeMove();
				bool aimPass = aimProbe.Check(combat, target.GetTargetEntity());
				bool movePass = moveProbe.Check(combat, target);
				m_bAICFATProbeContracts = aimPass && movePass;
			}
			if (m_iAICFATProbeTick % 5 == 0)
			{
				Print(string.Format("[AICF][AT_ENGAGEMENT_SAMPLE] tick=%1 distance=%2 selected=%3 native_max=%4 max=%5 ammo=%6 target=%7 visible=%8", m_iAICFATProbeTick, distance, applies, combat.AICF_ATProbeNativeRange(), combat.GetSelectedWeaponMaxDist(), ammo, target, target && combat.IsTargetVisible(target)));
				PerceptionComponent perception = PerceptionComponent.Cast(m_AICFATProbeShooter.FindComponent(PerceptionComponent));
				BaseTarget known = perception.FindTargetPerceptionObject(m_AICFATProbeTarget.m_Vehicle);
				Print(string.Format("[AICF][AT_ENGAGEMENT_PERCEPTION] known=%1 enemies=%2 shooter=%3 target=%4", known, perception.GetTargetCount(ETargetCategory.ENEMY), m_AICFATProbeShooter.GetOrigin(), m_AICFATProbeTarget.m_Vehicle.GetOrigin()));
				if (known) Print(string.Format("[AICF][AT_ENGAGEMENT_KNOWN] type=%1 category=%2 seen=%3 trace=%4 disarmed=%5", typename.EnumToString(EAIUnitType, known.GetUnitType()), typename.EnumToString(ETargetCategory, known.GetTargetCategory()), known.GetTimeSinceSeen(), known.GetTraceFraction(), known.IsDisarmed()));
			}
			if (m_iAICFATProbeTick == 30)
			{
				AIWeaponTargetSelector selector = new AIWeaponTargetSelector();
				selector.Init(m_AICFATProbeShooter);
				bool selected = selector.SelectWeaponAgainstUnitType(EAIUnitType.UnitType_VehicleMedium, false);
				float minDist, maxDist;
				bool direct;
				selector.GetSelectedWeaponProperties(minDist, maxDist, direct);
				Print(string.Format("[AICF][AT_ENGAGEMENT_SELECTOR] selected=%1 min=%2 max=%3 direct=%4", selected, minDist, maxDist, direct));
			}
		}
		if (m_iAICFATProbeTick >= 180 || (m_bAICFATProbeFired && m_bAICFATProbeContracts))
		{
			GetGame().GetCallqueue().Remove(AICF_ATProbeTick);
			if (m_AICFATProbeEffect) m_AICFATProbeEffect.GetOnWeaponFired().Remove(AICF_ATProbeShot);
			Print(string.Format("[AICF][AT_ENGAGEMENT_FINISHED] placed=%1 selected=%2 fired=%3 shot_distance=%4", m_bAICFATProbePlaced, m_bAICFATProbeSelected, m_bAICFATProbeFired, m_fAICFATProbeShotDistance));
			GetGame().RequestClose();
		}
	}
}
