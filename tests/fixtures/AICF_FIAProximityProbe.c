// Только isolated stage: production spawn/boarding/native lifecycle readback.
class AICF_FIAProximityProbe
{
	static int s_iChecks;
	static int s_iFailures;
	static bool s_bBudget;
	static void Check(string name, bool passed)
	{
		s_iChecks++;
		if (!passed) s_iFailures++;
		Print(string.Format("[AICF][FIA_PROXIMITY_CHECK] case=%1 passed=%2", name, passed));
	}

	static void Place(SCR_AIGroup group, vector position)
	{
		if (!group) return;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (int index, AIAgent agent : agents)
		{
			if (!agent || !AICF_GroupRuntime.IsAliveCharacter(agent.GetControlledEntity())) continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			character.GetDamageManager().EnableDamageHandling(false);
			vector target = position + Vector(index * 2, 0, 0);
			target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]) + 0.1;
			character.SetOrigin(target);
		}
	}
}

modded class SCR_CampaignMilitaryBaseManager
{
	override void SelectHQs(notnull array<SCR_CampaignMilitaryBaseComponent> candidates, notnull array<SCR_CampaignMilitaryBaseComponent> controlPoints, out notnull array<SCR_CampaignMilitaryBaseComponent> selectedHQs)
	{
		if (candidates.Count() < 2) { super.SelectHQs(candidates, controlPoints, selectedHQs); return; }
		SCR_CampaignMilitaryBaseComponent us = candidates[0];
		SCR_CampaignMilitaryBaseComponent ussr;
		float greatest;
		foreach (SCR_CampaignMilitaryBaseComponent candidate : candidates)
		{
			float distance = vector.DistanceSqXZ(us.GetOwner().GetOrigin(), candidate.GetOwner().GetOrigin());
			if (distance <= greatest) continue;
			greatest = distance;
			ussr = candidate;
		}
		selectedHQs = {us, ussr};
		Print(string.Format("[AICF][FIA_PROXIMITY_HQ] us=%1 ussr=%2", us.GetOwner().GetID(), ussr.GetOwner().GetID()));
	}
}

modded class AICF_BaseBuilderService
{
	override void Update()
	{
		if (AICF_FIAProximityProbe.s_bBudget) return;
		super.Update();
	}
}

// Одинаковый вход PRNG в baseline/after; native lifecycle не переопределяется.
modded class AICF_FIAGarrisonRandom
{
	protected bool m_bAICFProbeSeeded;
	override int Next(int count)
	{
		if (!m_bAICFProbeSeeded) { m_iState = 590013; m_bAICFProbeSeeded = true; }
		return super.Next(count);
	}
}

modded class AICF_FIAGarrisonService
{
	static AICF_FIAGarrison AICF_ProbeSelect(vector us, vector ussr)
	{
		float range = SCR_AmbientPatrolSystem.GetInstance().GetDespawnDistance() + 500;
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (vector.DistanceXZ(g.m_vHome, us) > range && vector.DistanceXZ(g.m_vHome, ussr) > range) return g;
		}
		return null;
	}

	static string AICF_ProbePlan()
	{
		string snapshot;
		foreach (AICF_FIAGarrison g : s_aDefenders)
			snapshot += string.Format("%1:%2:%3:%4:%5;", g.m_iSlot, g.m_iNode, g.m_sPrefab, g.m_sPassengerTemplate, g.m_aPassengerPrefabs.Count());
		return snapshot;
	}

	static void AICF_ProbeSnapshot(string phase)
	{
		int groups, materialized, active, dormant, farActive;
		ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
		float range = SCR_AmbientPatrolSystem.GetInstance().GetDespawnDistance();
		foreach (AICF_FIAGarrison g : s_aDefenders)
		{
			if (!g.m_Group) continue;
			groups++;
			if (g.m_Group.IsDormant()) dormant++;
			array<AIAgent> agents = {};
			g.m_Group.GetAgents(agents);
			if (g.m_DesantGroup)
			{
				array<AIAgent> passengers = {};
				g.m_DesantGroup.GetAgents(passengers);
				agents.InsertAll(passengers);
			}
			foreach (AIAgent agent : agents)
			{
				if (!agent || !AICF_GroupRuntime.IsAliveCharacter(agent.GetControlledEntity())) continue;
				materialized++;
				if (agent.IsAIActivated()) active++;
				vector position = agent.GetControlledEntity().GetOrigin();
				if (!observers.HasObserverWithinRangeSq(position[0], position[2], range * range)) farActive++;
				ChimeraCharacter.Cast(agent.GetControlledEntity()).GetDamageManager().EnableDamageHandling(false);
			}
			if (g.m_Vehicle) DamageManagerComponent.Cast(g.m_Vehicle.FindComponent(DamageManagerComponent)).EnableDamageHandling(false);
		}
		Print(string.Format("[AICF][FIA_PROXIMITY_COUNTS] phase=%1 plan=%2 groups=%3 materialized=%4 active=%5 dormant=%6 far_materialized=%7", phase, s_aDefenders.Count(), groups, materialized, active, dormant, farActive));
		if (phase == "FAR" || phase == "APPROACH") AICF_FIAProximityProbe.Check(phase + "_NO_FAR_MEMBERS", farActive == 0);
	}
}

modded class AICF_FIAPatrolService
{
	int AICF_ProbeRoadCount() { return m_aPatrols.Count(); }
	void AICF_ProbeProtectRoads()
	{
		foreach (AICF_FIAPatrol p : m_aPatrols)
		{
			if (p.m_Vehicle) DamageManagerComponent.Cast(p.m_Vehicle.FindComponent(DamageManagerComponent)).EnableDamageHandling(false);
			if (!p.m_Group) continue;
			array<AIAgent> agents = {};
			p.m_Group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (character) character.GetDamageManager().EnableDamageHandling(false);
			}
		}
	}

	AICF_FIAPatrol AICF_ProbeRoad(vector us, vector ussr)
	{
		float range = SCR_AmbientPatrolSystem.GetInstance().GetDespawnDistance() + 400;
		foreach (AICF_FIAPatrol p : m_aPatrols)
		{
			if (p.m_bRetired || !p.m_Current || !p.m_Current.GetOwner()) continue;
			vector position = p.m_Current.GetOwner().GetOrigin();
			if (p.m_Vehicle) position = p.m_Vehicle.GetOrigin();
			if (vector.DistanceXZ(position, us) > range && vector.DistanceXZ(position, ussr) > range) return p;
		}
		return null;
	}
}

modded class AICF_MatchController
{
	protected int m_iAICFFIAProbePhase;
	protected int m_iAICFFIAProbeAt;
	protected int m_iAICFFIAProbeStarted;
	protected int m_iAICFFIAProbeLimit;
	protected int m_iAICFFIAProbeAlive;
	protected int m_iAICFFIAProbeSample;
	protected bool m_bAICFFIARetentionChecked;
	protected AICF_FIAGarrison m_AICFFIAProbeTarget;
	protected SCR_AIGroup m_AICFFIAProbeActor;
	protected SCR_AIGroup m_AICFFIAProbeGroup;
	protected Vehicle m_AICFFIAProbeVehicle;
	protected string m_sAICFFIAProbePlan;
	protected string m_sAICFFIAProbeMode;
	protected vector m_vAICFFIAProbeUS;
	protected vector m_vAICFFIAProbeUSSR;
	protected vector m_vAICFFIAProbeNear;
	protected AICF_FIAPatrol m_AICFFIAProbeRoad;
	protected SCR_AIGroup m_AICFFIAProbeRoadGroup;
	protected Vehicle m_AICFFIAProbeRoadVehicle;

	override protected void CommanderTick()
	{
		string mode;
		if (System.GetCLIParam("aicfFIAProximityProbe", mode)) return;
		super.CommanderTick();
	}

	protected void AICF_FIAProbeFinish()
	{
		Print(string.Format("[AICF][FIA_PROXIMITY_DONE] mode=%1 checks=%2 failures=%3", m_sAICFFIAProbeMode, AICF_FIAProximityProbe.s_iChecks, AICF_FIAProximityProbe.s_iFailures));
		m_iAICFFIAProbePhase = 99;
		AICF_FIAProximityProbe.s_bBudget = false;
		GetGame().RequestClose();
	}

	protected bool AICF_FIAProbeActive()
	{
		if (!m_AICFFIAProbeTarget || !m_AICFFIAProbeTarget.m_bReady || !m_AICFFIAProbeTarget.m_Group) return false;
		array<AIAgent> agents = {};
		m_AICFFIAProbeTarget.m_Group.GetAgents(agents);
		if (agents.IsEmpty()) return false;
		foreach (AIAgent agent : agents)
			if (!agent || !agent.IsAIActivated() || !AICF_GroupRuntime.IsAliveCharacter(agent.GetControlledEntity())) return false;
		return true;
	}

	protected bool AICF_FIAProbePrepareActor() { return true; }
	protected void AICF_FIAProbePlaceActor(vector position) { AICF_FIAProximityProbe.Place(m_AICFFIAProbeActor, position); }
	protected void AICF_FIAProbeSafetyChecks() {}
	protected bool AICF_FIAProbeSafetyPending() { return false; }

	protected int AICF_FIAProbeAlive()
	{
		return AICF_GroupRuntime.CountAliveAgents(m_AICFFIAProbeTarget.m_Group) + AICF_GroupRuntime.CountAliveAgents(m_AICFFIAProbeTarget.m_DesantGroup);
	}

	override protected void Update()
	{
		super.Update();
		if (!System.GetCLIParam("aicfFIAProximityProbe", m_sAICFFIAProbeMode) || m_iAICFFIAProbePhase == 99) return;
		int now = System.GetTickCount();
		if (!m_iAICFFIAProbeStarted) m_iAICFFIAProbeStarted = now;
		if (now - m_iAICFFIAProbeStarted > 500000) { AICF_FIAProximityProbe.Check("TIMEOUT", false); AICF_FIAProbeFinish(); return; }
		if (!m_bRosterReady) return;
		m_FIAPatrols.AICF_ProbeProtectRoads();
		if (m_iAICFFIAProbePhase == 0)
		{
			m_vAICFFIAProbeUS = AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(0).GetGroup()).GetOrigin();
			m_vAICFFIAProbeUSSR = AICF_GroupRuntime.ResolveAliveLeader(m_USSRState.GetSlot(0).GetGroup()).GetOrigin();
			m_sAICFFIAProbePlan = AICF_FIAGarrisonService.AICF_ProbePlan();
			if (AICF_Difficulty.Get() == AICF_EDifficulty.EASY)
			{
				AICF_FIAProximityProbe.Check("EASY_EMPTY", m_sAICFFIAProbePlan.IsEmpty());
				AICF_FIAProximityProbe.Check("EASY_NO_PATROLS", m_FIAPatrols.AICF_ProbeRoadCount() == 0);
				AICF_FIAProbeFinish();
				return;
			}
			m_AICFFIAProbeTarget = AICF_FIAGarrisonService.AICF_ProbeSelect(m_vAICFFIAProbeUS, m_vAICFFIAProbeUSSR);
			AICF_FIAProximityProbe.Check("TARGET", m_AICFFIAProbeTarget != null);
			if (!m_AICFFIAProbeTarget) { AICF_FIAProbeFinish(); return; }
			m_AICFFIAProbeActor = m_USState.GetSlot(0).GetGroup();
			if (m_sAICFFIAProbeMode == "ussr") m_AICFFIAProbeActor = m_USSRState.GetSlot(0).GetGroup();
			float distance = SCR_AmbientPatrolSystem.GetInstance().GetSpawnDistance() * 0.65;
			for (int direction; direction < 8; direction++)
			{
				float angle = direction * Math.PI / 4;
				vector near = m_AICFFIAProbeTarget.m_vHome + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
				if (GetGame().GetWorld().GetSurfaceY(near[0], near[2]) < 1) continue;
				m_vAICFFIAProbeNear = near;
				break;
			}
			Print("[AICF][FIA_PROXIMITY_PLAN] controlled_seed=590013 plan=" + m_sAICFFIAProbePlan);
			if (!AICF_FIAProbePrepareActor()) return;
			m_iAICFFIAProbePhase = 1;
			m_iAICFFIAProbeAt = now;
		}
		for (int slot; slot < 10; slot++)
		{
			SCR_AIGroup us = m_USState.GetSlot(slot).GetGroup();
			SCR_AIGroup ussr = m_USSRState.GetSlot(slot).GetGroup();
			if (us != m_AICFFIAProbeActor) AICF_FIAProximityProbe.Place(us, m_vAICFFIAProbeUS);
			if (ussr != m_AICFFIAProbeActor) AICF_FIAProximityProbe.Place(ussr, m_vAICFFIAProbeUSSR);
		}
		vector actorPosition = m_vAICFFIAProbeNear;
		if (m_iAICFFIAProbePhase == 1 || m_iAICFFIAProbePhase == 5 || m_iAICFFIAProbePhase == 7 || m_iAICFFIAProbePhase == 9 || m_iAICFFIAProbePhase == 12)
		{
			actorPosition = m_vAICFFIAProbeUS;
			if (m_sAICFFIAProbeMode == "ussr") actorPosition = m_vAICFFIAProbeUSSR;
		}
		if (m_iAICFFIAProbePhase == 4 && m_AICFFIAProbeTarget.m_Group)
		{
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_AICFFIAProbeTarget.m_Group);
			if (leader)
			{
				SCR_AmbientPatrolSystem system = SCR_AmbientPatrolSystem.GetInstance();
				float band = (system.GetSpawnDistance() + system.GetDespawnDistance()) * 0.5;
				for (int direction; direction < 8; direction++)
				{
					float angle = direction * Math.PI / 4;
					vector candidate = leader.GetOrigin() + Vector(Math.Cos(angle) * band, 0, Math.Sin(angle) * band);
					if (GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]) < 1) continue;
					actorPosition = candidate;
					break;
				}
			}
		}
		AICF_FIAProbePlaceActor(actorPosition);
		if (now >= m_iAICFFIAProbeSample)
		{
			m_iAICFFIAProbeSample = now + 10000;
			IEntity actor = AICF_GroupRuntime.ResolveAliveLeader(m_AICFFIAProbeActor);
			vector actual;
			if (actor) actual = actor.GetOrigin();
			ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
			vector home = m_AICFFIAProbeTarget.m_vHome;
			float range = SCR_AmbientPatrolSystem.GetInstance().GetDespawnDistance();
			Print(string.Format("[AICF][FIA_PROXIMITY_SAMPLE] phase=%1 actor=%2 requested=%3 home=%4 observed=%5 group=%6 agents=%7 ready=%8", m_iAICFFIAProbePhase, actual, actorPosition, home,
				observers.HasObserverWithinRangeSq(home[0], home[2], range * range), m_AICFFIAProbeTarget.m_Group, AICF_GroupRuntime.CountAliveAgents(m_AICFFIAProbeTarget.m_Group), m_AICFFIAProbeTarget.m_bReady));
			if (m_sAICFFIAProbeMode == "player")
			{
				array<vector> positions = {};
				observers.GetObserversMP(positions);
				Print(string.Format("[AICF][FIA_PROXIMITY_NATIVE_PLAYERS] phase=%1 count=%2 positions=%3", m_iAICFFIAProbePhase, positions.Count(), positions));
			}
			if (m_iAICFFIAProbePhase == 5 && m_AICFFIAProbeTarget.m_Group)
			{
				array<AIAgent> agents = {};
				m_AICFFIAProbeTarget.m_Group.GetAgents(agents);
				foreach (AIAgent agent : agents)
				{
					string inventory;
					Print(string.Format("[AICF][FIA_PROXIMITY_HELD] member=%1 lod=%2 permanent=%3 capture=%4 observed=%5 policy=%6", agent.GetControlledEntity(), agent.GetLOD(), agent.GetPermanentLOD(),
						AICF_LoadoutInventory.Capture(agent.GetControlledEntity(), inventory), m_AICFFIAProbeTarget.m_Group.HasObserverInRange(range), m_AICFFIAProbeTarget.m_Group.GetLifecyclePolicy()));
				}
			}
		}
		int age = now - m_iAICFFIAProbeAt;
		int farWait = 20000;
		if (m_sAICFFIAProbeMode == "baseline" || m_sAICFFIAProbeMode == "scale") farWait = 80000;
		if (m_iAICFFIAProbePhase == 1 && age >= farWait)
		{
			AICF_FIAGarrisonService.AICF_ProbeSnapshot("FAR");
			AICF_FIAProximityProbe.Check("TARGET_FAR_EMPTY", !m_AICFFIAProbeTarget.m_Group || AICF_GroupRuntime.CountAliveAgents(m_AICFFIAProbeTarget.m_Group) == 0);
			if (m_sAICFFIAProbeMode == "baseline") { AICF_FIAProbeFinish(); return; }
			m_iAICFFIAProbeLimit = GetGame().GetAIWorld().GetLimitOfActiveAIs();
			AICF_FIAProximityProbe.s_bBudget = true;
			GetGame().GetAIWorld().SetLimitOfActiveAIs(1);
			m_iAICFFIAProbePhase = 2;
			m_iAICFFIAProbeAt = now;
		}
		else if (m_iAICFFIAProbePhase == 2 && age >= 10000)
		{
			AICF_FIAProximityProbe.Check("BUDGET_BLOCKS", !GetGame().GetAIWorld().CanActivateGroup(null) && (!m_AICFFIAProbeTarget.m_Group || !m_AICFFIAProbeTarget.m_Group.GetAgentsCount()));
			GetGame().GetAIWorld().SetLimitOfActiveAIs(m_iAICFFIAProbeLimit);
			AICF_FIAProximityProbe.s_bBudget = false;
			m_iAICFFIAProbePhase = 3;
			m_iAICFFIAProbeAt = now;
		}
		else if (m_iAICFFIAProbePhase == 3 && (AICF_FIAProbeActive() || age >= 90000))
		{
			AICF_FIAGarrisonService.AICF_ProbeSnapshot("APPROACH");
			AICF_FIAProximityProbe.Check("ACTIVE_AFTER_APPROACH", AICF_FIAProbeActive());
			if (!AICF_FIAProbeActive()) { AICF_FIAProbeFinish(); return; }
			m_AICFFIAProbeGroup = m_AICFFIAProbeTarget.m_Group;
			m_AICFFIAProbeVehicle = m_AICFFIAProbeTarget.m_Vehicle;
			m_iAICFFIAProbeAlive = AICF_FIAProbeAlive();
			if (m_AICFFIAProbeTarget.m_bPassengers) m_AICFFIAProbeTarget.m_bPassengersDeployed = true;
			m_iAICFFIAProbePhase = 4;
			m_iAICFFIAProbeAt = now;
		}
		else if (m_iAICFFIAProbePhase == 4 && age >= 15000)
		{
			if (!m_bAICFFIARetentionChecked)
			{
				m_bAICFFIARetentionChecked = true;
				AICF_FIAProximityProbe.Check("RETENTION", AICF_FIAProbeActive() && AICF_FIAProbeAlive() == m_iAICFFIAProbeAlive);
				if (m_AICFFIAProbeTarget.m_bPassengers)
					AICF_FIAProximityProbe.Check("DESANT_DEPLOYED", AICF_GroupRuntime.CountAliveAgents(m_AICFFIAProbeTarget.m_DesantGroup) > 0);
			}
			AICF_FIAProbeSafetyChecks();
			if (AICF_FIAProbeSafetyPending()) return;
			m_iAICFFIAProbePhase = 5;
			m_iAICFFIAProbeAt = now;
		}
		else if ((m_iAICFFIAProbePhase == 5 || m_iAICFFIAProbePhase == 7) && age >= 15000)
		{
			AICF_FIAGarrisonService.AICF_ProbeSnapshot("WITHDRAWN");
			AICF_FIAProximityProbe.Check("DORMANT_" + m_iAICFFIAProbePhase, m_AICFFIAProbeGroup && m_AICFFIAProbeGroup.IsDormant() && !AICF_FIAProbeAlive());
			m_iAICFFIAProbePhase++;
			m_iAICFFIAProbeAt = now;
		}
		else if ((m_iAICFFIAProbePhase == 6 || m_iAICFFIAProbePhase == 8) && age >= 10000)
		{
			int expected = m_iAICFFIAProbeAlive;
			if (m_iAICFFIAProbePhase == 8) expected--;
			if ((!AICF_FIAProbeActive() || AICF_FIAProbeAlive() != expected) && age < 60000) return;
			bool returned = AICF_FIAProbeActive() && m_AICFFIAProbeTarget.m_Group == m_AICFFIAProbeGroup && m_AICFFIAProbeTarget.m_Vehicle == m_AICFFIAProbeVehicle;
			AICF_FIAProximityProbe.Check("RETURN_" + m_iAICFFIAProbePhase, returned && AICF_FIAProbeAlive() == expected);
			AICF_FIAProximityProbe.Check("IMMUTABLE_PLAN_" + m_iAICFFIAProbePhase, m_sAICFFIAProbePlan == AICF_FIAGarrisonService.AICF_ProbePlan());
			array<AIAgent> agents = {};
			m_AICFFIAProbeGroup.GetAgents(agents);
			if (m_iAICFFIAProbePhase == 6 && !agents.IsEmpty())
			{
				ChimeraCharacter victim = ChimeraCharacter.Cast(agents[agents.Count() - 1].GetControlledEntity());
				victim.GetDamageManager().EnableDamageHandling(true);
				SCR_CharacterDamageManagerComponent.Cast(victim.GetDamageManager()).Kill(Instigator.CreateInstigatorGM());
			}
			else
			{
				if (m_AICFFIAProbeTarget.m_DesantGroup)
				{
					array<AIAgent> passengers = {};
					m_AICFFIAProbeTarget.m_DesantGroup.GetAgents(passengers);
					agents.InsertAll(passengers);
				}
				foreach (AIAgent agent : agents)
				{
					ChimeraCharacter victim = ChimeraCharacter.Cast(agent.GetControlledEntity());
					victim.GetDamageManager().EnableDamageHandling(true);
					SCR_CharacterDamageManagerComponent.Cast(victim.GetDamageManager()).Kill(Instigator.CreateInstigatorGM());
				}
				DamageManagerComponent damage = DamageManagerComponent.Cast(m_AICFFIAProbeVehicle.FindComponent(DamageManagerComponent));
				damage.EnableDamageHandling(true);
				damage.GetDefaultHitZone().SetHealthScaled(0);
			}
			m_iAICFFIAProbePhase++;
			m_iAICFFIAProbeAt = now;
		}
		else if (m_iAICFFIAProbePhase == 9 && age >= 15000)
		{
			m_iAICFFIAProbePhase = 10;
			m_iAICFFIAProbeAt = now;
		}
		else if (m_iAICFFIAProbePhase == 10 && age >= 20000)
		{
			AICF_FIAProximityProbe.Check("NO_RESURRECTION", !AICF_FIAProbeAlive() && m_AICFFIAProbeTarget.m_Vehicle == m_AICFFIAProbeVehicle);
			AICF_FIAProximityProbe.Check("DESTROYED_VEHICLE_NOT_REPLACED", DamageManagerComponent.Cast(m_AICFFIAProbeVehicle.FindComponent(DamageManagerComponent)).IsDestroyed());
			m_AICFFIAProbeRoad = m_FIAPatrols.AICF_ProbeRoad(m_vAICFFIAProbeUS, m_vAICFFIAProbeUSSR);
			AICF_FIAProximityProbe.Check("ROAD_PLAN", m_AICFFIAProbeRoad != null);
			if (!m_AICFFIAProbeRoad) { AICF_FIAProbeFinish(); return; }
			vector center = m_AICFFIAProbeRoad.m_Current.GetOwner().GetOrigin();
			if (m_AICFFIAProbeRoad.m_Vehicle) center = m_AICFFIAProbeRoad.m_Vehicle.GetOrigin();
			float distance = SCR_AmbientPatrolSystem.GetInstance().GetSpawnDistance() * 0.65;
			for (int direction; direction < 8; direction++)
			{
				float angle = direction * Math.PI / 4;
				vector near = center + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
				if (GetGame().GetWorld().GetSurfaceY(near[0], near[2]) < 1) continue;
				m_vAICFFIAProbeNear = near;
				break;
			}
			m_iAICFFIAProbePhase = 11;
			m_iAICFFIAProbeAt = now;
		}
		else if (m_iAICFFIAProbePhase == 11 && ((m_AICFFIAProbeRoad.m_bReady && m_AICFFIAProbeRoad.Seated()) || age >= 90000))
		{
			AICF_FIAProximityProbe.Check("ROAD_ACTIVE", m_AICFFIAProbeRoad.m_bReady && m_AICFFIAProbeRoad.Seated() && m_AICFFIAProbeRoad.m_Group.GetAgentsCount() == 2);
			m_AICFFIAProbeRoadGroup = m_AICFFIAProbeRoad.m_Group;
			m_AICFFIAProbeRoadVehicle = m_AICFFIAProbeRoad.m_Vehicle;
			if (!m_AICFFIAProbeRoadGroup) { AICF_FIAProbeFinish(); return; }
			m_iAICFFIAProbePhase = 12;
			m_iAICFFIAProbeAt = now;
		}
		else if (m_iAICFFIAProbePhase == 12 && age >= 15000)
		{
			AICF_FIAProximityProbe.Check("ROAD_DORMANT", m_AICFFIAProbeRoadGroup.IsDormant() && !m_AICFFIAProbeRoadGroup.GetAgentsCount());
			vector center = m_AICFFIAProbeRoadVehicle.GetOrigin();
			m_vAICFFIAProbeNear = center + Vector(-SCR_AmbientPatrolSystem.GetInstance().GetSpawnDistance() * 0.5, 0, 0);
			m_iAICFFIAProbePhase = 13;
			m_iAICFFIAProbeAt = now;
		}
		else if (m_iAICFFIAProbePhase == 13 && age >= 10000)
		{
			if (!m_AICFFIAProbeRoad.Seated() && age < 60000) return;
			AICF_FIAProximityProbe.Check("ROAD_RETURN", m_AICFFIAProbeRoad.Seated() && m_AICFFIAProbeRoad.m_Group == m_AICFFIAProbeRoadGroup &&
				m_AICFFIAProbeRoad.m_Vehicle == m_AICFFIAProbeRoadVehicle && m_AICFFIAProbeRoadGroup.GetAgentsCount() == 2);
			AICF_FIAProbeFinish();
		}
	}
}
