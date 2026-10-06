// Только isolated stage. Native ownership/death/replacement и production visits.
// Fixture строит казармы и перемещает seed к ним; это не проверка навигации.
modded class AICF_InfantryRecruitmentService
{
	AICF_InfantryRecruitmentOrder AICF_LifecycleOrder(AICF_GroupSlot slot)
	{
		foreach (AICF_InfantryRecruitmentOrder order : m_aOrders)
		{
			if (order.m_Slot == slot && !order.m_bDemandReleased) return order;
		}
		return null;
	}

	int AICF_LifecycleDemand(SCR_CampaignMilitaryBaseComponent base)
	{
		return OtherDemand(base, null);
	}
}

modded class AICF_MatchController
{
	protected int m_iLifecycleStarted;
	protected int m_iLifecyclePhase;
	protected int m_iLifecycleGeneration;
	protected EntityID m_LifecycleGroupId;
	protected int m_iLifecycleDemand;
	protected int m_iLifecyclePassed;
	protected bool m_bLifecycleReduced;
	protected bool m_bLifecycleDeathObserved;
	protected ref AICF_InfantryRecruitmentOrder m_LifecycleOldOrder;
	protected SCR_CampaignBuildingCompositionComponent m_LifecycleBuilding;

	override protected void Update()
	{
		string enabled;
		if (System.GetCLIParam("aicfRecruitmentLifecycleProbe", enabled) && enabled == "1" && m_bRosterReady && !m_bStopped)
			AICF_LifecycleTick();
		super.Update();
	}

	protected void AICF_LifecycleCheck(string name, bool pass)
	{
		if (pass) m_iLifecyclePassed++;
		Print(string.Format("[AICF][RECRUITMENT_LIFECYCLE_PROBE] case=%1 pass=%2 phase=%3", name, pass, m_iLifecyclePhase));
	}

	protected void AICF_LifecycleTick()
	{
		int now = System.GetTickCount();
		if (m_iLifecycleStarted > 0 && now - m_iLifecycleStarted > 360000)
		{
			AICF_LifecycleCheck("TIMEOUT", false);
			AICF_LifecycleClose();
			return;
		}
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		SCR_CampaignMilitaryBaseComponent base = m_USFaction.GetMainBase();
		if (!m_iLifecycleStarted)
		{
			m_iLifecycleStarted = now;
			m_Construction.Stop();
			for (int i; i < m_USState.GetSlotCount(); i++)
			{
				m_USState.GetSlot(i).SetDesiredSize(1);
				m_USSRState.GetSlot(i).SetDesiredSize(1);
			}
			AICF_LifecyclePlace(base);
		}
		if (m_LifecycleBuilding && !m_LifecycleBuilding.IsCompositionSpawned() && m_LifecycleBuilding.GetCompositionLayout())
		{
			SCR_CampaignBuildingLayoutComponent layout = m_LifecycleBuilding.GetCompositionLayout();
			layout.AddBuildingValue(layout.GetToBuildValue());
		}
		base.AddSupplies(base.GetSuppliesMax() - base.GetSupplies());
		AICF_InfantryRecruitmentOrder order = m_InfantryRecruitment.AICF_LifecycleOrder(slot);
		if (m_iLifecyclePhase == 4 && !m_bLifecycleDeathObserved && AICF_GroupRuntime.CountAliveAgents(m_LifecycleOldOrder.m_Group) == 0)
		{
			m_bLifecycleDeathObserved = true;
			AICF_LifecycleCheck("DEATH_RELEASE", m_InfantryRecruitment.AICF_LifecycleDemand(base) == 0);
		}
		if (m_iLifecyclePhase == 0 && m_ConflictAdapter.GetReplacementSpawnRejectionReason(base, m_USFaction).IsEmpty())
		{
			slot.SetDesiredSize(10);
			string reason;
			if (!order && !m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason))
			{
				Print(string.Format("[AICF][RECRUITMENT_LIFECYCLE_WAIT] phase=%1 reason=%2 alive=%3 desired=%4", m_iLifecyclePhase, reason, AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()), slot.GetDesiredSize()));
				return;
			}
			order = m_InfantryRecruitment.AICF_LifecycleOrder(slot);
			if (!order || order.m_Donor) return;
			AICF_LifecycleCheck("LIVE_DEMAND", m_InfantryRecruitment.AICF_LifecycleDemand(base) > 0);
			m_InfantryRecruitment.CancelForSlot(slot);
			order = null;
			AICF_LifecycleCheck("CANCEL_RELEASE", !m_InfantryRecruitment.AICF_LifecycleOrder(slot) && m_InfantryRecruitment.AICF_LifecycleDemand(base) == 0);
			m_iLifecyclePhase = 1;
		}
		if (m_iLifecyclePhase == 1)
		{
			string reason;
			if (!order && !m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason))
			{
				Print(string.Format("[AICF][RECRUITMENT_LIFECYCLE_WAIT] phase=%1 reason=%2 alive=%3 desired=%4", m_iLifecyclePhase, reason, AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()), slot.GetDesiredSize()));
				return;
			}
			order = m_InfantryRecruitment.AICF_LifecycleOrder(slot);
			if (!order || order.m_Donor) return;
			m_LifecycleOldOrder = order;
			base.SetFaction(m_USSRFaction);
			AICF_LifecycleCheck("OWNER_CHANGED_RELEASE", base.GetFaction() == m_USSRFaction && m_InfantryRecruitment.AICF_LifecycleDemand(base) == 0 && !order.HasSafeBarracks());
			// Оставляем нового владельца до следующего production Update.
			m_iLifecyclePhase = 2;
		}
		else if (m_iLifecyclePhase == 2)
		{
			AICF_LifecycleCheck("OWNER_CHANGED_FINISHED", m_LifecycleOldOrder.m_bDemandReleased && !order && m_InfantryRecruitment.AICF_LifecycleDemand(base) == 0);
			base.SetFaction(m_USFaction);
			m_iLifecyclePhase = 3;
		}
		else if (m_iLifecyclePhase == 3)
		{
			string reason;
			if (!order && !m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason))
			{
				Print(string.Format("[AICF][RECRUITMENT_LIFECYCLE_WAIT] phase=%1 reason=%2 alive=%3 desired=%4", m_iLifecyclePhase, reason, AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()), slot.GetDesiredSize()));
				return;
			}
			order = m_InfantryRecruitment.AICF_LifecycleOrder(slot);
			if (!order || order.m_Donor) return;
			IEntity member = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			if (!member || AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) != 1) return;
			m_iLifecycleGeneration = slot.GetSpawnGeneration();
			m_LifecycleGroupId = slot.GetGroup().GetID();
			m_LifecycleOldOrder = order;
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(member.FindComponent(SCR_CharacterDamageManagerComponent));
			damage.Kill(Instigator.CreateInstigatorGM());
			m_iLifecyclePhase = 4;
		}
		else if (m_iLifecyclePhase == 4 && slot.IsCombatReady() && !slot.IsReplacementDeployment() && slot.GetSpawnGeneration() > m_iLifecycleGeneration)
		{
			AICF_LifecycleCheck("REPLACEMENT_IDENTITY", slot == m_USState.GetSlot(0) && slot.GetGroup().GetID() != m_LifecycleGroupId && !m_LifecycleOldOrder.IsCurrent(slot));
			AICF_LifecycleCheck("OLD_VISIT_RELEASED", m_LifecycleOldOrder.m_bDemandReleased);
			m_iLifecyclePhase = 5;
		}
		else if (m_iLifecyclePhase == 5)
		{
			string reason;
			if (!order && !m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason))
			{
				Print(string.Format("[AICF][RECRUITMENT_LIFECYCLE_WAIT] phase=%1 reason=%2 alive=%3 desired=%4", m_iLifecyclePhase, reason, AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()), slot.GetDesiredSize()));
				return;
			}
			order = m_InfantryRecruitment.AICF_LifecycleOrder(slot);
			if (!order) return;
			m_iLifecycleDemand = m_InfantryRecruitment.AICF_LifecycleDemand(base);
			AICF_LifecycleCheck("REPLACEMENT_NEW_DEMAND", m_iLifecycleDemand > 0 && order.m_iGeneration > m_iLifecycleGeneration && order != m_LifecycleOldOrder);
			m_LifecycleOldOrder = order;
			IEntity member = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			if (member) member.SetOrigin(order.m_vPosition + "2 0 2");
			m_iLifecyclePhase = 6;
		}
		else if (m_iLifecyclePhase == 6)
		{
			int demand = m_InfantryRecruitment.AICF_LifecycleDemand(base);
			int alive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
			if (!m_bLifecycleReduced && alive > 1 && demand > 0 && demand < m_iLifecycleDemand)
			{
				m_bLifecycleReduced = true;
				AICF_LifecycleCheck("RECRUIT_REDUCES_DEMAND", true);
			}
			if (alive == 10 && !order)
			{
				AICF_LifecycleCheck("COMPLETION_RELEASE", demand == 0 && m_LifecycleOldOrder.m_bDemandReleased);
				AICF_LifecycleClose();
				return;
			}
		}
		if (now - m_iLifecycleStarted > 360000)
		{
			AICF_LifecycleCheck("TIMEOUT", false);
			AICF_LifecycleClose();
		}
	}

	protected void AICF_LifecyclePlace(SCR_CampaignMilitaryBaseComponent base)
	{
		vector position = base.GetSpawnPoint().GetOrigin() + "35 0 25";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab("US", AICF_EConstructionType.SMALL_BARRACKS);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (!entity) return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation) affiliation.SetAffiliatedFaction(m_USFaction);
		m_LifecycleBuilding = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (m_LifecycleBuilding) m_LifecycleBuilding.SetProviderEntity(base.GetMasterProvider().GetOwner());
	}

	protected void AICF_LifecycleClose()
	{
		Print(string.Format("[AICF][RECRUITMENT_LIFECYCLE_PROBE] finished=1 passed=%1 total=10 phase=%2", m_iLifecyclePassed, m_iLifecyclePhase));
		Stop(true);
		GetGame().RequestClose();
	}
}
