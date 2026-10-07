// Только isolated source copy. Сажает одного muster-бойца на свободную
// штатную турель. Выход, постройка казарм, набор и движение — production.
modded class AICF_MatchController
{
	protected int m_iTurretProbeStarted;
	protected int m_iTurretProbeSample;
	protected bool m_bTurretProbeInjected;
	protected bool m_bTurretProbeSeated;
	protected bool m_bTurretProbeExited;
	protected bool m_bTurretProbeFull;
	protected bool m_bTurretProbeMoved;
	protected AICF_GroupSlot m_TurretProbeSlot;
	protected SCR_AIGroup m_TurretProbeGroup;
	protected ChimeraCharacter m_TurretProbeCharacter;
	protected BaseCompartmentSlot m_TurretProbeSeat;
	protected vector m_vTurretProbeOrigin;
	protected vector m_vTurretProbeFullPosition;
	protected float m_fTurretProbeDistance;

	override protected void Update()
	{
		string enabled;
		if (System.GetCLIParam("aicfMusterTurretProbe", enabled) && enabled == "1" && m_bRosterReady && !m_bStopped)
			TurretProbeUpdate();
		super.Update();
	}

	protected bool TurretProbeFind(IEntity entity)
	{
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(entity.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || usage.GetVehicleType() != EAIVehicleType.STATIC_WEAPON)
			return true;
		BaseCompartmentSlot seat = usage.GetTurretCompartmentSlot();
		if (!seat || seat.GetOccupant() || seat.IsReserved() || !seat.IsCompartmentAccessible())
			return true;
		float distance = vector.DistanceSqXZ(m_vTurretProbeOrigin, entity.GetOrigin());
		if (!m_TurretProbeSeat || distance < m_fTurretProbeDistance)
		{
			m_TurretProbeSeat = seat;
			m_fTurretProbeDistance = distance;
		}
		return true;
	}

	protected void TurretProbeUpdate()
	{
		int now = System.GetTickCount();
		if (!m_iTurretProbeStarted)
			m_iTurretProbeStarted = now;
		if (!m_bTurretProbeInjected && now - m_iTurretProbeStarted >= 10000)
		{
			m_TurretProbeSlot = m_USSRState.GetSlot(3);
			m_TurretProbeGroup = m_TurretProbeSlot.GetGroup();
			m_TurretProbeCharacter = ChimeraCharacter.Cast(AICF_GroupRuntime.ResolveAliveLeader(m_TurretProbeGroup));
			if (m_TurretProbeCharacter && m_TurretProbeSlot.IsWaitingForInfantryMuster())
			{
				m_vTurretProbeOrigin = m_TurretProbeCharacter.GetOrigin();
				GetGame().GetWorld().QueryEntitiesBySphere(m_vTurretProbeOrigin, 150, TurretProbeFind, null, EQueryEntitiesFlags.ALL);
				if (m_TurretProbeSeat)
				{
					CompartmentAccessComponent access = m_TurretProbeCharacter.GetCompartmentAccessComponent();
					m_bTurretProbeInjected = access.GetInVehicle(m_TurretProbeSeat.GetOwner(), m_TurretProbeSeat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, true);
					Print(string.Format("[AICF][MUSTER_TURRET_PROBE] test_only=1 injected=%1 faction=%2 slot=3 group=%3 character=%4 turret=%5",
						m_bTurretProbeInjected, m_USSRFaction.GetFactionKey(), m_TurretProbeGroup.GetID(), m_TurretProbeCharacter.GetID(), m_TurretProbeSeat.GetOwner().GetID()));
				}
			}
		}
		bool same = m_TurretProbeSlot && m_TurretProbeGroup && m_TurretProbeSlot.GetGroup() == m_TurretProbeGroup &&
			AICF_GroupRuntime.IsAliveCharacter(m_TurretProbeCharacter);
		if (m_bTurretProbeInjected && same)
		{
			bool seated = m_TurretProbeSeat.GetOccupant() == m_TurretProbeCharacter;
			m_bTurretProbeSeated = m_bTurretProbeSeated || seated;
			if (m_bTurretProbeSeated && !CompartmentAccessComponent.GetVehicleIn(m_TurretProbeCharacter))
				m_bTurretProbeExited = true;
			int alive = AICF_GroupRuntime.CountAliveAgents(m_TurretProbeGroup);
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_TurretProbeGroup);
			if (alive == 10 && !m_TurretProbeSlot.IsRecruitingInfantry() && !m_bTurretProbeFull)
			{
				m_bTurretProbeFull = true;
				m_vTurretProbeFullPosition = leader.GetOrigin();
			}
			if (m_bTurretProbeFull && leader && vector.DistanceXZ(leader.GetOrigin(), m_vTurretProbeFullPosition) >= 100)
				m_bTurretProbeMoved = true;
			if (now - m_iTurretProbeSample >= 10000)
			{
				m_iTurretProbeSample = now;
				SCR_DefendWaypoint defend = SCR_DefendWaypoint.Cast(m_TurretProbeSlot.GetWaypoint());
				bool useTurrets;
				if (defend && defend.GetCurrentDefendPreset())
					useTurrets = defend.GetCurrentDefendPreset().GetUseTurrets();
				Print(string.Format("[AICF][MUSTER_TURRET_PROBE] test_only=1 seated=%1 exited=%2 alive=%3 full=%4 moved_100m=%5 same_group=%6 use_turrets=%7 elapsed_ms=%8",
					seated, m_bTurretProbeExited, alive, m_bTurretProbeFull, m_bTurretProbeMoved, same, useTurrets, now - m_iTurretProbeStarted));
			}
		}
		bool usFull = AICF_GroupRuntime.CountAliveAgents(m_USState.GetSlot(0).GetGroup()) == 10;
		if (now - m_iTurretProbeStarted >= 480000 || (m_bTurretProbeExited && m_bTurretProbeFull && m_bTurretProbeMoved && usFull))
		{
			Print(string.Format("[AICF][MUSTER_TURRET_PROBE] test_only=1 finished=1 injected=%1 seated_once=%2 exited=%3 full=%4 moved_100m=%5 same_group=%6 us_full=%7",
				m_bTurretProbeInjected, m_bTurretProbeSeated, m_bTurretProbeExited, m_bTurretProbeFull, m_bTurretProbeMoved, same, usFull));
			Stop(true);
			GetGame().RequestClose();
		}
	}
}
