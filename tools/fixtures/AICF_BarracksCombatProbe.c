// Test-only: временно копируется в Core/Forces. Готовит настоящие stock/RHS
// казармы и supplies, размещает лидеров у службы. Roster, цены и transfer — production.
modded class AICF_MatchController
{
	protected int m_iBarracksProbeStarted;
	protected int m_iBarracksProbePhase;
	protected int m_iBarracksProbeSample;
	protected EntityID m_BarracksProbeUSGroup;
	protected EntityID m_BarracksProbeUSSRGroup;
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aBarracksProbeBuildings = {};

	override protected void Update()
	{
		string enabled;
		bool probe = System.GetCLIParam("aicfBarracksCombatProbe", enabled) && enabled == "1";
		if (probe && m_bRosterReady && !m_bStopped)
			BarracksProbeUpdate();
		super.Update();
	}

	protected void BarracksProbeUpdate()
	{
		int now = System.GetTickCount();
		if (m_iBarracksProbeStarted == 0)
		{
			m_iBarracksProbeStarted = now;
			m_Construction.Stop();
			m_USState.GetSlot(0).SetDesiredSize(2);
			m_USSRState.GetSlot(0).SetDesiredSize(2);
			m_BarracksProbeUSGroup = m_USState.GetSlot(0).GetGroup().GetID();
			m_BarracksProbeUSSRGroup = m_USSRState.GetSlot(0).GetGroup().GetID();
			for (int i = 1; i < 10; i++)
			{
				m_USState.GetSlot(i).SetDesiredSize(1);
				m_USSRState.GetSlot(i).SetDesiredSize(1);
			}
			BarracksProbePlace(m_USFaction);
			BarracksProbePlace(m_USSRFaction);
			Print("[AICF][BARRACKS_PROBE] prepared=1 selected_slots=US:0,USSR:0 other_slots_desired=1");
		}
		foreach (SCR_CampaignBuildingCompositionComponent composition : m_aBarracksProbeBuildings)
		{
			if (composition && !composition.IsCompositionSpawned() && composition.GetCompositionLayout())
			{
				SCR_CampaignBuildingLayoutComponent layout = composition.GetCompositionLayout();
				layout.AddBuildingValue(layout.GetToBuildValue());
			}
		}
		BarracksProbePin(m_USState.GetSlot(0), m_USFaction);
		BarracksProbePin(m_USSRState.GetSlot(0), m_USSRFaction);
		AICF_GroupSlot us = m_USState.GetSlot(0);
		AICF_GroupSlot ussr = m_USSRState.GetSlot(0);
		int usAlive = AICF_GroupRuntime.CountAliveAgents(us.GetGroup());
		int ussrAlive = AICF_GroupRuntime.CountAliveAgents(ussr.GetGroup());
		if (now - m_iBarracksProbeSample >= 10000)
		{
			m_iBarracksProbeSample = now;
			Print(string.Format("[AICF][BARRACKS_PROBE] elapsed_ms=%1 us=%2 ussr=%3 phase=%4 pending=%5",
				now - m_iBarracksProbeStarted, usAlive, ussrAlive, m_iBarracksProbePhase, m_InfantryRecruitment.CountPendingAgents()));
		}
		if (m_iBarracksProbePhase == 0 && usAlive == 2 && ussrAlive == 2 && m_InfantryRecruitment.BarracksProbeComplete() && !us.IsRecruitingInfantry() && !ussr.IsRecruitingInfantry())
		{
			bool sameGroups = us.GetGroup().GetID() == m_BarracksProbeUSGroup && ussr.GetGroup().GetID() == m_BarracksProbeUSSRGroup;
			Print(string.Format("[AICF][BARRACKS_PROBE] full_rosters=1 stable_groups=%1 desired_us=%2 desired_ussr=%3", sameGroups, us.GetDesiredSize(), ussr.GetDesiredSize()));
			m_iBarracksProbePhase = 1;
		}
		if (m_iBarracksProbePhase == 1 || now - m_iBarracksProbeStarted >= 240000)
		{
			Print(string.Format("[AICF][BARRACKS_PROBE] finished=1 full_rosters=%1", m_iBarracksProbePhase == 1));
			AICF_BarracksProbe.Check("FULL_ROSTERS_STABLE_GROUPS", m_iBarracksProbePhase == 1 && us.GetGroup().GetID() == m_BarracksProbeUSGroup && ussr.GetGroup().GetID() == m_BarracksProbeUSSRGroup);
			Stop(true);
			AICF_BarracksProbe.Check("STOP_FAIL_CLOSED", !AICF_BarracksCombatSafety.IsSafe(AICF_BarracksProbe.s_Service));
			Print(string.Format("[AICF][BARRACKS_PROBE_RESULT] checks=%1 failures=%2", AICF_BarracksProbe.s_iChecks, AICF_BarracksProbe.s_iFailures));
			foreach (SCR_CampaignBuildingCompositionComponent placed : m_aBarracksProbeBuildings)
			{
				if (placed && placed.GetOwner())
					SCR_EntityHelper.DeleteEntityAndChildren(placed.GetOwner());
			}
			m_aBarracksProbeBuildings.Clear();
			GetGame().GetCallqueue().Remove(BarracksProbeClose);
			GetGame().GetCallqueue().CallLater(BarracksProbeClose, 2000, false);
		}
	}

	protected void BarracksProbePin(AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
		if (!leader || !faction.GetMainBase())
			return;
		array<SCR_ServicePointComponent> services = {};
		faction.GetMainBase().GetServices(services);
		foreach (SCR_ServicePointComponent service : services)
		{
			if (service && service.GetOwner() && service.GetType() == SCR_EServicePointType.BARRACKS && service.GetServiceState() == SCR_EServicePointStatus.ONLINE)
			{
				leader.SetOrigin(service.GetOwner().GetOrigin() + "2 0 2");
				return;
			}
		}
	}
	protected void BarracksProbeClose()
	{
		GetGame().GetCallqueue().Remove(BarracksProbeClose);
		GetGame().RequestClose();
	}

	void ~AICF_MatchController()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(BarracksProbeClose);
	}

	protected void BarracksProbePlace(SCR_CampaignFaction faction)
	{
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		if (!base || !base.GetMasterProvider() || !base.GetSpawnPoint())
			return;
		base.AddSupplies(base.GetSuppliesMax() - base.GetSupplies());
		vector position, rotation;
		base.GetSpawnPoint().GetPositionAndRotation(position, rotation);
		position += "35 0 25";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab(
			AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()), AICF_EConstructionType.SMALL_BARRACKS);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (!entity)
			return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation)
			affiliation.SetAffiliatedFaction(faction);
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (!composition)
			return;
		composition.SetProviderEntity(base.GetMasterProvider().GetOwner());
		m_aBarracksProbeBuildings.Insert(composition);
		Print(string.Format("[AICF][BARRACKS_PROBE] barracks=%1 faction=%2 position=%3 supplies=%4", entity.GetID(), faction.GetFactionKey(), position, base.GetSupplies()));
	}
}

class AICF_BarracksProbe
{
	static int s_iChecks;
	static int s_iFailures;
	static bool s_bPolicyComplete;
	static SCR_ServicePointComponent s_Service;

	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfBarracksCombatProbe", value) && value == "1";
	}

	static void Check(string name, bool passed)
	{
		s_iChecks++;
		if (!passed)
			s_iFailures++;
		Print(string.Format("[AICF][BARRACKS_PROBE_CHECK] name=%1 passed=%2", name, passed));
	}
}

modded class AICF_BarracksCombatSafety
{
	protected int m_iProbePhase;
	protected int m_iProbeFirstShot;
	protected int m_iProbeLastShot;
	protected ref AICF_BarracksCombatZone m_ProbeZone;

	static bool BarracksProbeObserved(IEntity character)
	{
		if (!s_Active)
			return false;
		foreach (AICF_BarracksShotObserver observer : s_Active.m_aObservers)
		{
			if (observer.m_Character == character && observer.m_Id == character.GetID())
				return true;
		}
		return false;
	}

	override void Update(SCR_GameModeCampaign campaign, AICF_ObjectiveGraph graph)
	{
		super.Update(campaign, graph);
		if (!AICF_BarracksProbe.Enabled() || AICF_BarracksProbe.s_bPolicyComplete)
			return;
		int now = System.GetTickCount();
		if (m_iProbePhase == 0)
		{
			foreach (AICF_BarracksCombatZone candidate : m_aZones)
			{
				if (IsSafe(candidate.m_Service) && candidate.m_Service.GetServiceState() == SCR_EServicePointStatus.ONLINE)
				{
					m_ProbeZone = candidate;
					break;
				}
			}
			if (!m_ProbeZone)
				return;
			AICF_BarracksProbe.s_Service = m_ProbeZone.m_Service;
			RecordCombat(m_ProbeZone.m_vPosition + "100.5 0 0", "PROBE_OUTSIDE");
			AICF_BarracksProbe.Check("OUTSIDE_100M_ALLOWED", IsSafe(m_ProbeZone.m_Service));
			RecordCombat(m_ProbeZone.m_vPosition + "100 0 0", "PROBE_BOUNDARY");
			AICF_BarracksProbe.Check("AT_100M_BLOCKED", !IsSafe(m_ProbeZone.m_Service));
			m_iProbeFirstShot = now;
			m_iProbePhase = 1;
		}
		if (m_iProbePhase == 1 && now - m_iProbeFirstShot >= 10000)
		{
			RecordCombat(m_ProbeZone.m_vPosition, "PROBE_REPEAT");
			m_iProbeLastShot = now;
			AICF_BarracksProbe.Check("REPEAT_RESETS_30_SECONDS", m_ProbeZone.m_iQuietUntilMs == now + 30000);
			m_iProbePhase = 2;
		}
		if (m_iProbePhase == 2 && now - m_iProbeFirstShot >= 31000)
		{
			AICF_BarracksProbe.Check("FIRST_DEADLINE_STILL_BLOCKED", !IsSafe(m_ProbeZone.m_Service));
			m_iProbePhase = 3;
		}
		if (m_iProbePhase == 3 && now - m_iProbeLastShot >= 30000)
		{
			AICF_BarracksProbe.Check("LAST_SHOT_30_SECONDS_RESUMED", IsSafe(m_ProbeZone.m_Service));
			AICF_BarracksProbe.s_bPolicyComplete = true;
		}
	}
}

modded class AICF_InfantryRecruitmentService
{
	protected ref array<ref AICF_InfantryRecruitmentOrder> m_aProbeCancelled = {};
	protected ref array<string> m_aProbeSides = {};
	protected ref array<string> m_aProbeChecked = {};
	protected ref array<int> m_aProbeCounts = {};

	bool BarracksProbeComplete()
	{
		return m_aProbeChecked.Count() == 2 && AICF_BarracksProbe.s_bPolicyComplete;
	}

	override protected void ConsiderFaction(AICF_FactionState state, SCR_CampaignFaction faction)
	{
		if (AICF_BarracksProbe.Enabled())
		{
			if (!AICF_BarracksProbe.s_bPolicyComplete)
				return;
			// Не тестируем штатные 60 секунд retry; реальные 30 секунд safety не меняем.
			m_aRetrySlots.Clear();
			m_aRetryAtMs.Clear();
		}
		super.ConsiderFaction(state, faction);
	}

	override protected string Tick(AICF_InfantryRecruitmentOrder order, bool graphReady)
	{
		if (!AICF_BarracksProbe.Enabled())
			return super.Tick(order, graphReady);
		string side = AICF_ContentProfile.GetActive().GetStableFactionKey(order.m_Faction.GetFactionKey());
		if (order.m_Donor && !m_aProbeSides.Contains(side))
		{
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(order.m_Group);
			AICF_BarracksProbe.Check(side + "_CHARACTER_OBSERVED", AICF_BarracksCombatSafety.BarracksProbeObserved(leader));
			IEntity recruit = AICF_GroupRuntime.ResolveAliveLeader(order.m_Donor);
			if (recruit)
				AICF_BarracksProbe.Check(side + "_NEW_CHARACTER_OBSERVED", AICF_BarracksCombatSafety.BarracksProbeObserved(recruit));
			EventHandlerManagerComponent events = EventHandlerManagerComponent.Cast(leader.FindComponent(EventHandlerManagerComponent));
			m_aProbeSides.Insert(side);
			m_aProbeCancelled.Insert(order);
			m_aProbeCounts.Insert(AICF_GroupRuntime.CountAliveAgents(order.m_Group));
			if (side == "US")
			{
				AICF_BarracksProbe.Check(side + "_NATIVE_EVENT_MANAGER", events != null);
				if (events)
				{
					BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(leader.FindComponent(BaseWeaponManagerComponent));
					BaseWeaponComponent weapon;
					if (weapons)
						weapon = weapons.GetCurrentWeapon();
					AICF_BarracksProbe.Check(side + "_SHOT_PAYLOAD_WEAPON", weapon != null);
					int playerID = -1;
					events.RaiseEvent("OnProjectileShot", 3, playerID, weapon, leader);
				}
			}
			else
			{
				SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(leader.FindComponent(SCR_DamageManagerComponent));
				AICF_BarracksProbe.Check(side + "_NATIVE_DAMAGE_MANAGER", damage && damage.GetDefaultHitZone());
				if (damage && damage.GetDefaultHitZone())
				{
					vector hit[3];
					hit[0] = leader.GetOrigin();
					SCR_DamageContext context = new SCR_DamageContext(EDamageType.KINETIC, 1, hit, leader,
						damage.GetDefaultHitZone(), Instigator.CreateInstigator(leader), null, -1, -1);
					damage.HandleDamage(context);
				}
			}
			// RaiseEvent может доставляться на следующем frame. До доставки не продолжаем transfer.
			return string.Empty;
		}
		if (m_aProbeCancelled.Contains(order) && !m_aProbeChecked.Contains(side))
		{
			AICF_BarracksProbe.Check(side + "_COMBAT_EVENT_BLOCKED_PENDING", !order.HasSafeBarracks());
			float supplies = order.m_Base.GetSupplies();
			AICF_BarracksProbe.Check(side + "_QUOTE_REJECTED", !m_Economy.QuoteInfantryRecruit(order));
			AICF_BarracksProbe.Check(side + "_DEBIT_REJECTED", !m_Economy.DebitInfantryRecruit(order));
			AICF_BarracksProbe.Check(side + "_SUPPLIES_UNCHANGED", order.m_fPaid == 0 && order.m_Base.GetSupplies() == supplies);
		}
		return super.Tick(order, graphReady);
	}

	override void Update(AICF_FactionState us, SCR_CampaignFaction usFaction, AICF_FactionState ussr,
		SCR_CampaignFaction ussrFaction, int availableAgents, bool graphReady)
	{
		super.Update(us, usFaction, ussr, ussrFaction, availableAgents, graphReady);
		if (!AICF_BarracksProbe.Enabled())
			return;
		for (int i = 0; i < m_aProbeCancelled.Count(); i++)
		{
			AICF_InfantryRecruitmentOrder order = m_aProbeCancelled[i];
			string side = m_aProbeSides[i];
			if (m_aProbeChecked.Contains(side) || m_aOrders.Contains(order))
				continue;
			AICF_BarracksProbe.Check(side + "_PENDING_CANCELLED", !order.m_Donor && !order.m_Slot.IsRecruitingInfantry());
			AICF_BarracksProbe.Check(side + "_NO_JOIN_NO_PAYMENT", AICF_GroupRuntime.CountAliveAgents(order.m_Group) == m_aProbeCounts[i] && order.m_fPaid == 0);
			m_aProbeChecked.Insert(side);
		}
	}
}
