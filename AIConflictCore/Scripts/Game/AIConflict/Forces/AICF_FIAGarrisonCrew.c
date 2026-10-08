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
			// Только первичная посадка; после READY погибшие и покинувшие машину не заменяются.
			access.GetInVehicle(seat.GetOwner(), seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, true);
		}
		return seated == g.m_aSeats.Count();
	}
}

// Ограничение относится только к зарегистрированным immutable members гарнизона.
// Штатный выбор цели/оружия и стрельба сохраняются; pursuit, retreat и driver
// combat-move не исполняются. После выхода из уничтоженной машины боец держится
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
		if (!stationary)
		{
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
