class AICF_FIAGarrisonCrew : AICF_FIAPatrolCrew
{
	bool DiscoverSeats(AICF_FIAGarrison g)
	{
		if (!g.VehicleIdentity()) return false;
		array<BaseCompartmentSlot> seats = {};
		Seats(g.m_Vehicle, seats);
		int pilots, turrets;
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat.IsCompartmentAccessible()) continue;
			if (seat.GetOccupant() || seat.IsReserved()) return false;
			g.m_aSeats.Insert(seat);
			bool essential = IsEssentialSeat(seat, g.m_bTank);
			g.m_aEssentialSeats.Insert(essential);
			g.Log("FIA_GARRISON_SEAT", string.Format("index=%1 name=%2 type=%3 essential=%4", g.m_aSeats.Count() - 1, seat.GetCompartmentName(true), seat.GetType(), essential));
			if (seat.GetType() == ECompartmentType.PILOT) pilots++;
			if (seat.GetType() == ECompartmentType.TURRET) turrets++;
		}
		return pilots == 1 && turrets >= 1 && g.m_aSeats.Count() <= 16;
	}

	bool BoardGarrison(AICF_FIAGarrison g)
	{
		if (!g.VehicleIdentity() || !g.GroupIdentity()) return false;
		int actual, wrong, dead;
		if (!AICF_GroupRuntime.HasExactFactionRoster(g.m_Group, "FIA", g.m_aSeats.Count(), actual, wrong, dead)) return false;
		if (g.m_aCrew.IsEmpty())
		{
			array<AIAgent> agents = {};
			g.m_Group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				ChimeraCharacter member = ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (!member) return false;
				g.m_aCrew.Insert(member);
				g.m_aCrewIds.Insert(member.GetID());
			}
		}
		int seated;
		for (int i; i < g.m_aCrew.Count(); i++)
		{
			ChimeraCharacter member = g.m_aCrew[i];
			BaseCompartmentSlot seat = g.m_aSeats[i];
			if (!g.OwnsMember(member) || !seat || !seat.IsCompartmentAccessible()) return false;
			if (seat.GetOccupant() == member) { seated++; continue; }
			if (seat.GetOccupant() || seat.IsReserved()) return false;
			CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
			if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut()) continue;
			// Первичная посадка; recovery возвращает только исходных живых бойцов.
			access.GetInVehicle(seat.GetOwner(), seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, true);
		}
		return seated == g.m_aSeats.Count();
	}

	static bool IsEssentialSeat(BaseCompartmentSlot seat, bool tank)
	{
		if (!seat) return false;
		if (tank || seat.GetType() != ECompartmentType.CARGO || seat.GetAttachedTurret()) return true;
		string name = seat.GetCompartmentName(true);
		UIInfo info = seat.GetUIInfo();
		if (info) name += " " + info.GetName();
		name.ToLower();
		return name.Contains("commander");
	}

	bool CombatObserved(AICF_FIAGarrison g)
	{
		foreach (ChimeraCharacter member : g.m_aCrew)
		{
			if (!g.OwnsMember(member)) continue;
			AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
			if (!control || !control.GetAIAgent()) continue;
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(control.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
			if (!utility) continue;
			SCR_AIBehaviorBase behavior = utility.GetCurrentBehavior();
			if (SCR_AIAttackBehavior.Cast(behavior) || SCR_AISuppressBehavior.Cast(behavior)) return true;
			if (utility.m_ThreatSystem && utility.m_ThreatSystem.GetState() >= EAIThreatState.ALERTED) return true;
		}
		return false;
	}

	void UpdateGarrison(AICF_FIAGarrison g, int now)
	{
		if (!Replication.IsServer() || !g || g.m_bRetired || !g.m_bReady || !g.VehicleIdentity() || !g.GroupIdentity()) return;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(g.m_Vehicle.FindComponent(SCR_DamageManagerComponent));
		if (damage && damage.IsDestroyed()) return;
		if (!g.m_bTank && !g.m_bPassengersDeployed && CombatObserved(g))
		{
			g.m_bPassengersDeployed = true;
			g.Log("FIA_GARRISON_DESANT_DEPLOY", "reason=COMBAT reboard=0");
		}
		g.m_bDisembarking = false;
		if (g.m_bPassengersDeployed && !g.m_DesantGroup)
		{
			// Отдельный controller без spawn: пешие не должны блокировать vehicle group activity.
			g.m_DesantGroup = Create(g.m_Faction, g.m_Vehicle.GetOrigin(), 1);
			if (g.m_DesantGroup)
			{
				g.m_DesantGroupId = g.m_DesantGroup.GetID();
				g.m_DesantGroup.m_aUnitPrefabSlots.Clear();
			}
		}
		for (int i; i < g.m_aCrew.Count(); i++)
		{
			ChimeraCharacter member = g.m_aCrew[i];
			if (!g.OwnsMember(member)) continue;
			BaseCompartmentSlot seat = g.m_aSeats[i];
			if (!seat || seat.GetVehicle() != g.m_Vehicle) continue;
			CompartmentAccessComponent access = member.GetCompartmentAccessComponent();
			if (!access) continue;
			if (g.m_aEssentialSeats[i])
			{
				if (seat.GetOccupant() == member || now < g.m_iCrewRecoveryNextMs) continue;
				if (!seat.IsCompartmentAccessible() || seat.GetOccupant() || (seat.IsReserved() && !seat.IsReservedBy(member)) ||
					access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut()) continue;
				if (!AICF_FIAGarrisonRecovery.PlayersClear(member.GetOrigin(), g.m_Vehicle.GetOrigin()))
				{
					AICF_FIAGarrisonRecovery.Blocked(g, "PLAYER_CREW");
					continue;
				}
				// Повторяем immutable AI/seat/player fences перед forced exact-seat boarding.
				if (!g.OwnsMember(member) || !g.VehicleIdentity() || seat.GetOccupant() ||
					!AICF_FIAGarrisonRecovery.PlayersClear(member.GetOrigin(), g.m_Vehicle.GetOrigin())) continue;
				access.InterruptVehicleActionQueue(true, true, true);
				bool accepted = access.GetInVehicle(seat.GetOwner(), seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, true);
				g.m_iCrewRecoveryNextMs = now + 2000;
				if (accepted)
				{
					g.m_bCrewRecoveryPending = true;
					g.m_iCrewTeleports++;
				}
				g.Log("FIA_GARRISON_CREW_TELEPORT", string.Format("member=%1 seat=%2 accepted=%3 player_radius_m=50", member.GetID(), i, accepted));
				continue;
			}
			if (!g.m_bPassengersDeployed) continue;
			if (CompartmentAccessComponent.GetVehicleIn(member) != g.m_Vehicle && !access.IsGettingOut())
			{
				AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
				AIAgent agent = control.GetAIAgent();
				if (agent.GetParentGroup() == g.m_Group && g.DesantIdentity())
				{
					g.m_Group.RemoveAgent(agent);
					g.m_DesantGroup.AddAgent(agent);
					if (!agent.GetParentGroup()) g.m_Group.AddAgent(agent);
					g.Log("FIA_GARRISON_DESANT_TRANSFER", string.Format("member=%1 desant_group=%2 success=%3", member.GetID(), g.m_DesantGroupId, agent.GetParentGroup() == g.m_DesantGroup));
				}
				if (!g.DesantIdentity() || agent.GetParentGroup() != g.m_DesantGroup) g.m_bDisembarking = true;
				continue;
			}
			g.m_bDisembarking = true;
			Physics physics = g.m_Vehicle.GetPhysics();
			if (!physics || physics.GetVelocity().Length() > 1 || now < g.m_iDismountNextMs || access.IsGettingOut() || access.IsGettingIn()) continue;
			if (access.GetCompartment() != seat || seat.GetOccupant() != member) continue;
			// Обычная высадка, без teleport: разрешена и рядом с игроком.
			bool issued = access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
			g.m_iDismountNextMs = now + 2000;
			g.Log("FIA_GARRISON_DESANT_EXIT", string.Format("member=%1 seat=%2 accepted=%3", member.GetID(), i, issued));
		}
	}
}

// Ограничение относится только к зарегистрированным immutable members гарнизона.
// Штатный выбор цели/оружия и стрельба сохраняются; pursuit, retreat и driver
// combat-move не исполняются. Move разрешён только от текущего локального waypoint.
// После выхода из уничтоженной машины боец держится
// на месте и продолжает огонь. Игрок и чужая группа не попадают под эту policy.
modded class SCR_AIUtilityComponent
{
	protected ref SCR_AIWaitBehavior m_AICFGarrisonWait;

	override SCR_AIBehaviorBase EvaluateBehavior(BaseTarget unknownTarget)
	{
		SCR_AIBehaviorBase behavior = super.EvaluateBehavior(unknownTarget);
		if (!behavior || !AICF_FIAGarrisonService.IsDefender(m_OwnerEntity)) return behavior;
		bool stationary = SCR_AIAttackBehavior.Cast(behavior) || SCR_AISuppressBehavior.Cast(behavior) ||
			SCR_AIIdleBehavior.Cast(behavior) || SCR_AIWaitBehavior.Cast(behavior);
		bool patrolMove = (SCR_AIMoveIndividuallyBehavior.Cast(behavior) || SCR_AIMoveInFormationBehavior.Cast(behavior)) &&
			AICF_FIAGarrisonService.IsLocalPatrolMove(m_OwnerEntity, behavior);
		SCR_AIPilotMoveFromIncomingVehicleBehavior avoidance = SCR_AIPilotMoveFromIncomingVehicleBehavior.Cast(behavior);
		bool localAvoidance = avoidance && AICF_FIAGarrisonService.IsLocalPilotAvoidance(m_OwnerEntity, avoidance.m_vMovePos.m_Value);
		bool localBoarding = AICF_FIAGarrisonService.IsLocalBoarding(m_OwnerEntity, SCR_AIGetInVehicle.Cast(behavior));
		if (!stationary && !patrolMove && !localAvoidance && !localBoarding)
		{
			// Отклонённое действие иначе снова выигрывает EvaluateActions и блокирует новый маршрут.
			behavior.Fail();
			if (!m_AICFGarrisonWait) m_AICFGarrisonWait = new SCR_AIWaitBehavior(this, null);
			SetCurrentAction(m_AICFGarrisonWait);
			m_CurrentBehavior = m_AICFGarrisonWait;
			behavior = m_AICFGarrisonWait;
		}
		behavior.m_bUseCombatMove = false;
		return behavior;
	}

	override bool CanIndependentlyMove()
	{
		if (AICF_FIAGarrisonService.IsDefender(m_OwnerEntity)) return false;
		return super.CanIndependentlyMove();
	}
}

// В stock 1.8.0.13 threat invokers живут дольше текущей цели attack node.
// Между сменой behavior и следующим simulate цель может быть null. Без цели
// нечего временно отвлекать от атаки; с целью сохраняется штатная обработка.
modded class SCR_AIUpdateTargetAttackData
{
	override protected void OnThreatSectorEscalation(SCR_AISectorThreatFilter ts, int sectorId, float dangerValue)
	{
		if (!m_Target) return;
		super.OnThreatSectorEscalation(ts, sectorId, dangerValue);
	}

	override protected void OnThreatSectorDamageTaken(SCR_AISectorThreatFilter ts, int sectorId)
	{
		if (!m_Target) return;
		super.OnThreatSectorDamageTaken(ts, sectorId);
	}
}
