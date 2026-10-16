// Только isolated stage. Реальный native spawn/lifecycle, без замены их решений.
class AICF_AmbientProbe
{
	static int s_iChecks;
	static int s_iFailures;
	static bool s_bBudget;
	static ref map<SCR_AmbientPatrolSpawnPointComponent, int> s_mCalls = new map<SCR_AmbientPatrolSpawnPointComponent, int>();
	static void Check(string rule, bool passed)
	{
		s_iChecks++;
		if (!passed) s_iFailures++;
		Print(string.Format("[AICF][AMBIENT_CHECK] rule=%1 passed=%2", rule, passed));
	}

	static void Protect(SCR_AIGroup group)
	{
		if (!group) return;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			ChimeraCharacter member = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (member) member.GetDamageManager().EnableDamageHandling(false);
		}
	}

	static bool Active(SCR_AIGroup group)
	{
		if (!group || group.IsDormant() || AICF_GroupRuntime.CountAliveAgents(group) == 0) return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (AICF_GroupRuntime.IsAliveCharacter(agent.GetControlledEntity()) && !agent.IsAIActivated()) return false;
		}
		return true;
	}

	static void Place(SCR_AIGroup group, vector position)
	{
		if (!group) return;
		Protect(group);
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (int index, AIAgent agent : agents)
		{
			IEntity member = agent.GetControlledEntity();
			if (!AICF_GroupRuntime.IsAliveCharacter(member)) continue;
			vector target = position + Vector(index * 2, 0, 0);
			target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]) + 0.1;
			member.SetOrigin(target);
		}
	}
}

modded class AICF_BaseBuilderService
{
	override void Update()
	{
		// В controlled budget case builder не восстанавливает свой capacity floor.
		if (AICF_AmbientProbe.s_bBudget) return;
		super.Update();
	}
}

modded class SCR_AmbientPatrolSpawnPointComponent
{
	override void SpawnPatrol()
	{
		int calls;
		AICF_AmbientProbe.s_mCalls.Find(this, calls);
		AICF_AmbientProbe.s_mCalls.Set(this, calls + 1);
		super.SpawnPatrol();
	}
}

modded class SCR_CampaignMilitaryBaseComponent
{
	bool AICF_AmbientOwns(SCR_AmbientPatrolSpawnPointComponent point) { return m_aRemnants.Contains(point); }
}

modded class AICF_FIAGarrisonService
{
	static string AICF_AmbientComposition()
	{
		string snapshot;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			// Issue #59: immutable plan сохраняется, entity могут ещё не появиться
			// либо находиться в native dormancy. Это не повод ждать всю карту.
			snapshot += string.Format("%1:%2:%3:%4:", g.m_iSlot, g.m_iGeneration, g.m_sPrefab, g.m_sPassengerTemplate);
			SCR_DamageManagerComponent damage;
			if (g.m_Vehicle) damage = SCR_DamageManagerComponent.Cast(g.m_Vehicle.FindComponent(SCR_DamageManagerComponent));
			if (damage) damage.EnableDamageHandling(false);
			foreach (ChimeraCharacter member : g.m_aCrew)
			{
				if (member) member.GetDamageManager().EnableDamageHandling(false);
			}
		}
		return snapshot;
	}
}

modded class AICF_MatchController
{
	protected SCR_AmbientPatrolSpawnPointComponent m_AICFAmbientPoint;
	protected SCR_AIGroup m_AICFAmbientAttack;
	protected int m_iAICFAmbientPhase;
	protected int m_iAICFAmbientAt;
	protected int m_iAICFAmbientBegan;
	protected string m_sAICFAmbientMode;
	protected vector m_vAICFAmbientNear;
	protected vector m_vAICFAmbientFar;
	protected SCR_AIGroup m_AICFAmbientIdentity;
	protected int m_iAICFAmbientAlive;
	protected int m_iAICFAmbientLimit;
	protected bool m_bAICFAmbientUSSR;
	protected string m_sAICFAmbientGarrisons;
	protected SCR_CampaignMilitaryBaseComponent m_AICFAmbientBase;

	protected void AmbientPolicyNegatives() {}

	protected bool AmbientChangeOwner()
	{
		array<SCR_AmbientPatrolSpawnPointComponent> points = {};
		SCR_AmbientPatrolSystem.GetInstance().GetPatrols(points);
		for (int i; i < m_ObjectiveGraph.GetNodeCount(); i++)
		{
			SCR_CampaignMilitaryBaseComponent base = m_ObjectiveGraph.GetNode(i).GetBase();
			if (!base || base.IsHQ() || !base.GetFaction() || base.GetFaction().GetFactionKey() != "FIA") continue;
			foreach (SCR_AmbientPatrolSpawnPointComponent point : points)
			{
				if (!point || point.IsEliminated() || point.GetIsSpawned() || !base.AICF_AmbientOwns(point)) continue;
				m_AICFAmbientPoint = point;
				m_vAICFAmbientNear = point.GetOwner().GetOrigin() + Vector(200, 0, 0);
				m_AICFAmbientBase = base;
				base.SetFaction(m_USFaction);
				return true;
			}
		}
		return false;
	}

	override protected void CommanderTick()
	{
		string mode;
		if (System.GetCLIParam("aicfAmbientProbe", mode)) return;
		super.CommanderTick();
	}

	protected void AmbientSnapshot(string phase)
	{
		SCR_AIGroup group = m_AICFAmbientPoint.GetSpawnedGroup();
		int count = AICF_GroupRuntime.CountAliveAgents(group);
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_AICFAmbientAttack);
		vector position;
		if (leader) position = leader.GetOrigin();
		ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
		vector anchor = m_AICFAmbientPoint.GetOwner().GetOrigin();
		float observerRange = SCR_AmbientPatrolSystem.GetInstance().GetDespawnDistance();
		if (m_AICFAmbientPoint.GetDespawnDistanceOverride() >= 0) observerRange = m_AICFAmbientPoint.GetDespawnDistanceOverride();
		bool observer = observers.HasObserverWithinRangeSq(anchor[0], anchor[2], observerRange * observerRange);
		Print(string.Format("[AICF][AMBIENT_SAMPLE] phase=%1 point=%2 spawned=%3 eliminated=%4 group=%5 alive=%6 observer=%7 attacker=%8 position=%9", phase, anchor, m_AICFAmbientPoint.GetIsSpawned(), m_AICFAmbientPoint.IsEliminated(), AICF_GroupRuntime.FormatEntityId(group), count, observer, AICF_GroupRuntime.CountAliveAgents(m_AICFAmbientAttack), position) + string.Format(" observer_range_m=%1 distance_m=%2 budget_allowed=%3", observerRange, vector.DistanceXZ(anchor, position), GetGame().GetAIWorld().CanActivateGroup(null)));
		if (group)
		{
			Print(string.Format("[AICF][AMBIENT_GROUP] phase=%1 dormant=%2 stored_alive=%3 stored_dead=%4 spawn_m=%5 despawn_m=%6", phase, group.IsDormant(), group.GetDormantAliveCount(), group.GetDormantDeadCount(), group.GetSpawnDistance(), group.GetDespawnDistance()));
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				IEntity entity = agent.GetControlledEntity();
				Print(string.Format("[AICF][AMBIENT_MEMBER] phase=%1 id=%2 alive=%3 active=%4 prefab=%5 position=%6", phase, AICF_GroupRuntime.FormatEntityId(entity), AICF_GroupRuntime.IsAliveCharacter(entity), agent.IsAIActivated(), SCR_ResourceNameUtils.GetPrefabName(entity), entity.GetOrigin()));
			}
		}
	}

	protected bool AmbientSelect(AICF_FactionState state)
	{
		m_AICFAmbientAttack = state.GetSlot(0).GetGroup();
		if (!AICF_GroupRuntime.ResolveAliveLeader(m_AICFAmbientAttack)) return false;
		SCR_AmbientPatrolSystem system = SCR_AmbientPatrolSystem.GetInstance();
		array<SCR_AmbientPatrolSpawnPointComponent> points = {};
		system.GetPatrols(points);
		foreach (SCR_AmbientPatrolSpawnPointComponent point : points)
		{
			if (!point || point.IsEliminated() || point.GetIsSpawned()) continue;
			Faction faction = SCR_Faction.GetEntityFaction(point.GetOwner());
			if (!faction || faction.GetFactionKey() != "FIA") continue;
			vector position = point.GetOwner().GetOrigin();
			bool far = true;
			array<AICF_FactionState> states = {m_USState, m_USSRState};
			foreach (AICF_FactionState current : states)
			{
				for (int i; i < current.GetSlotCount(); i++)
				{
					IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(current.GetSlot(i).GetGroup());
					if (leader && vector.DistanceXZ(position, leader.GetOrigin()) < 1600) far = false;
				}
			}
			if (!far) continue;
			m_AICFAmbientPoint = point;
			float spawnDistance = system.GetSpawnDistance();
			if (point.GetSpawnDistanceOverride() >= 0) spawnDistance = point.GetSpawnDistanceOverride();
			bool dry;
			for (int angle; angle < 8; angle++)
			{
				float radians = angle * Math.PI / 4;
				vector target = position + Vector(Math.Cos(radians) * spawnDistance * 0.7, 0, Math.Sin(radians) * spawnDistance * 0.7);
				if (GetGame().GetWorld().GetSurfaceY(target[0], target[2]) < 1) continue;
				m_vAICFAmbientNear = target;
				dry = true;
				break;
			}
			if (!dry) continue;
			m_vAICFAmbientFar = AICF_GroupRuntime.ResolveAliveLeader(m_AICFAmbientAttack).GetOrigin();
			return true;
		}
		return false;
	}

	protected void AmbientFinish()
	{
		Print(string.Format("[AICF][AMBIENT_DONE] mode=%1 checks=%2 failures=%3", m_sAICFAmbientMode, AICF_AmbientProbe.s_iChecks, AICF_AmbientProbe.s_iFailures));
		m_iAICFAmbientPhase = 99;
		GetGame().RequestClose();
	}

	override protected void Update()
	{
		super.Update();
		if (!System.GetCLIParam("aicfAmbientProbe", m_sAICFAmbientMode) || !m_bRosterReady || m_iAICFAmbientPhase == 99) return;
		int now = System.GetTickCount();
		if (!m_iAICFAmbientBegan) m_iAICFAmbientBegan = now;
		if (now - m_iAICFAmbientBegan > 900000)
		{
			AICF_AmbientProbe.Check("TIMEOUT", false);
			AmbientFinish();
			return;
		}
		if (m_iAICFAmbientPhase == 0)
		{
			string garrisons = AICF_FIAGarrisonService.AICF_AmbientComposition();
			if (garrisons == "PENDING") return;
			m_sAICFAmbientGarrisons = garrisons;
			Print("[AICF][AMBIENT_GARRISONS] phase=BEFORE snapshot=" + garrisons);
			array<int> players = {};
			GetGame().GetPlayerManager().GetPlayers(players);
			AICF_AmbientProbe.Check("NO_PLAYERS", players.IsEmpty());
			AICF_FactionState state = m_USState;
			if (m_sAICFAmbientMode == "ussr" || m_bAICFAmbientUSSR) state = m_USSRState;
			if (!AmbientSelect(state))
			{
				AICF_AmbientProbe.Check("NATIVE_POSITION", false);
				AmbientFinish();
				return;
			}
			AICF_AmbientProbe.Check("NATIVE_POSITION", true);
			AmbientPolicyNegatives();
			AmbientSnapshot("BEFORE");
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 1;
		}
		if (m_iAICFAmbientPhase >= 2 && m_iAICFAmbientPhase != 5 && m_iAICFAmbientPhase != 8) AICF_AmbientProbe.Place(m_AICFAmbientAttack, m_vAICFAmbientNear);
		else AICF_AmbientProbe.Place(m_AICFAmbientAttack, m_vAICFAmbientFar);
		AICF_AmbientProbe.Protect(m_AICFAmbientPoint.GetSpawnedGroup());
		int age = now - m_iAICFAmbientAt;
		if (m_iAICFAmbientPhase == 1 && age >= 10000)
		{
			AmbientSnapshot("FAR");
			AICF_AmbientProbe.Check("FAR_NO_SPAWN", !m_AICFAmbientPoint.GetIsSpawned() && !m_AICFAmbientPoint.GetSpawnedGroup());
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 2;
			if (m_sAICFAmbientMode != "baseline")
			{
				m_iAICFAmbientLimit = GetGame().GetAIWorld().GetLimitOfActiveAIs();
				AICF_AmbientProbe.s_bBudget = true;
				GetGame().GetAIWorld().SetLimitOfActiveAIs(1);
				m_iAICFAmbientPhase = 10;
			}
		}
		else if (m_iAICFAmbientPhase == 10 && age >= 10000)
		{
			AmbientSnapshot("BUDGET");
			AIWorld ai = GetGame().GetAIWorld();
			bool blocked = !ai.CanActivateGroup(null);
			Print(string.Format("[AICF][AMBIENT_BUDGET] reason=AI_BUDGET active=%1 limit=%2 allowed=%3", ai.GetCurrentNumOfActiveAIs(), ai.GetLimitOfActiveAIs(), !blocked));
			AICF_AmbientProbe.Check("BUDGET_NATIVE_REFUSAL", blocked && !m_AICFAmbientPoint.GetIsSpawned() && !m_AICFAmbientPoint.GetSpawnedGroup());
			ai.SetLimitOfActiveAIs(m_iAICFAmbientLimit);
			AICF_AmbientProbe.s_bBudget = false;
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 2;
		}
		else if (m_iAICFAmbientPhase == 2 && age >= 20000)
		{
			AmbientSnapshot("APPROACH");
			IEntity attacker = AICF_GroupRuntime.ResolveAliveLeader(m_AICFAmbientAttack);
			AICF_AmbientProbe.Check("PHYSICAL_APPROACH", attacker && vector.DistanceXZ(attacker.GetOrigin(), m_vAICFAmbientNear) < 15);
			SCR_AIGroup group = m_AICFAmbientPoint.GetSpawnedGroup();
			int alive = AICF_GroupRuntime.CountAliveAgents(group);
			bool active = AICF_AmbientProbe.Active(group);
			if (m_sAICFAmbientMode == "baseline")
			{
				Print("[AICF][AMBIENT_REPRO] expected_active=1 actual_active=0 reason=NO_NATIVE_OBSERVER");
				AICF_AmbientProbe.Check("REPRO_NO_CONTROLLER_OR_MEMBERS", !group && alive == 0 && AICF_GroupRuntime.CountAliveAgents(m_AICFAmbientAttack) > 0);
				AmbientFinish();
				return;
			}
			AICF_AmbientProbe.Check("LIVE_ACTIVE_NATIVE_FIA", active);
			if (!active) { AmbientFinish(); return; }
			m_AICFAmbientIdentity = group;
			m_iAICFAmbientAlive = alive;
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 3;
		}
		else if (m_iAICFAmbientPhase == 3 && age >= 15000)
		{
			AmbientSnapshot("HELD");
			SCR_AIGroup group = m_AICFAmbientPoint.GetSpawnedGroup();
			AICF_AmbientProbe.Check("HELD_15_LIFECYCLE_INTERVALS", group == m_AICFAmbientIdentity && AICF_GroupRuntime.CountAliveAgents(group) == m_iAICFAmbientAlive && AICF_AmbientProbe.Active(group));
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 5;
		}
		else if (m_iAICFAmbientPhase == 5 && age >= 10000)
		{
			AmbientSnapshot("WITHDRAWN");
			SCR_AIGroup group = m_AICFAmbientPoint.GetSpawnedGroup();
			AICF_AmbientProbe.Check("NATIVE_DEMATERIALIZE", group == m_AICFAmbientIdentity && group.IsDormant() && AICF_GroupRuntime.CountAliveAgents(group) == 0 && group.GetDormantAliveCount() == m_iAICFAmbientAlive);
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 6;
		}
		else if (m_iAICFAmbientPhase == 6 && age >= 20000)
		{
			AmbientSnapshot("REAPPROACH");
			SCR_AIGroup group = m_AICFAmbientPoint.GetSpawnedGroup();
			int calls;
			AICF_AmbientProbe.s_mCalls.Find(m_AICFAmbientPoint, calls);
			AICF_AmbientProbe.Check("NO_DUPLICATE_CONTROLLER_OR_MEMBERS", group == m_AICFAmbientIdentity && calls == 1 && AICF_GroupRuntime.CountAliveAgents(group) == m_iAICFAmbientAlive);
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				ChimeraCharacter member = ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (!member) continue;
				SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(member.GetDamageManager());
				damage.EnableDamageHandling(true);
				damage.Kill(Instigator.CreateInstigatorGM());
			}
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 7;
		}
		else if (m_iAICFAmbientPhase == 7 && age >= 10000)
		{
			AmbientSnapshot("KILLED");
			AICF_AmbientProbe.Check("KILLED_NO_LIVE_FIA", AICF_GroupRuntime.CountAliveAgents(m_AICFAmbientPoint.GetSpawnedGroup()) == 0);
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 8;
		}
		else if (m_iAICFAmbientPhase == 8 && age >= 10000)
		{
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 9;
		}
		else if (m_iAICFAmbientPhase == 9 && age >= 15000)
		{
			AmbientSnapshot("KILLED_REAPPROACH");
			int calls;
			AICF_AmbientProbe.s_mCalls.Find(m_AICFAmbientPoint, calls);
			AICF_AmbientProbe.Check("NO_RESURRECTION", AICF_GroupRuntime.CountAliveAgents(m_AICFAmbientPoint.GetSpawnedGroup()) == 0 && calls == 1);
			string garrisons = AICF_FIAGarrisonService.AICF_AmbientComposition();
			Print("[AICF][AMBIENT_GARRISONS] phase=AFTER snapshot=" + garrisons);
			AICF_AmbientProbe.Check("EXTRA_GARRISON_IDENTITIES", garrisons == m_sAICFAmbientGarrisons);
			AICF_AmbientProbe.Place(m_AICFAmbientAttack, m_vAICFAmbientFar);
			if (!m_bAICFAmbientUSSR && m_sAICFAmbientMode != "ussr")
			{
				m_bAICFAmbientUSSR = true;
				m_iAICFAmbientPhase = 0;
				Print("[AICF][AMBIENT_SIDE] faction=USSR");
				return;
			}
			bool owner = AmbientChangeOwner();
			AICF_AmbientProbe.Check("NATIVE_BASE_REMNANT_SELECTED", owner);
			if (!owner) { AmbientFinish(); return; }
			m_iAICFAmbientAt = now;
			m_iAICFAmbientPhase = 11;
		}
		else if (m_iAICFAmbientPhase == 11 && age >= 20000)
		{
			AmbientSnapshot("OWNER_CHANGED");
			AICF_AmbientProbe.Check("NATIVE_FACTION_RETIRES_REMNANT", m_AICFAmbientBase.GetFaction() == m_USFaction && m_AICFAmbientPoint.IsEliminated() && !m_AICFAmbientPoint.GetSpawnedGroup());
			AmbientFinish();
		}
	}
}
