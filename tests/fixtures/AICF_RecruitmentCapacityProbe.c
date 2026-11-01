// Controlled native capacity fault. Не ordinary-war и не общий ресурсный PASS.
class AICF_RecruitCapacityState
{
	static bool s_bStarted;
	static bool s_bHeld;
	static bool s_bReleased;
	static bool s_bDone;
	static bool s_bPassed;
	static int s_iLimit = -1;
	static int s_iPressureAt;
	static int s_iToken;
	static int s_iStartedAt;
	static int s_iInitialAlive;
	static int s_iNextSample;
	static ref AICF_InfantryRecruitmentOrder s_Order;
	static string s_sReason;
	static bool Enabled()
	{
		string enabled;
		return System.GetCLIParam("aicfRecruitCapacityProbe", enabled) && enabled == "1";
	}
	static void Restore()
	{
		if (s_iLimit < 0 || !GetGame() || !GetGame().GetAIWorld()) return;
		GetGame().GetAIWorld().SetLimitOfActiveAIs(s_iLimit);
		s_iLimit = -1;
	}
	static void Finish(bool passed, string reason)
	{
		if (s_bDone) return;
		s_bDone = true;
		s_bPassed = passed;
		s_sReason = reason;
		Restore();
		Print(string.Format("[AICF][RECRUIT_CAPACITY_DONE] passed=%1 reason=%2 token=%3 held=%4 released=%5 elapsed_ms=%6", passed, reason, s_iToken, s_bHeld, s_bReleased, System.GetTickCount() - s_iPressureAt));
		GetGame().RequestClose();
	}
}

modded class AICF_InfantryRecruitmentService
{
	override protected void ConsiderFaction(AICF_FactionState state, SCR_CampaignFaction faction)
	{
		if (AICF_RecruitCapacityState.Enabled()) return;
		super.ConsiderFaction(state, faction);
	}

	override protected string Tick(AICF_InfantryRecruitmentOrder order, bool graphReady)
	{
		if (!AICF_RecruitCapacityState.Enabled() || !order || order.m_Faction.GetFactionKey() != "USSR" || order.m_Slot.GetSlotId() != 0)
			return super.Tick(order, graphReady);
		int now = System.GetTickCount();
		AIWorld world = GetGame().GetAIWorld();
		if (!AICF_RecruitCapacityState.s_Order && order.IsCurrent(order.m_Slot) && order.IsPhysicallyPresent() &&
			order.IsPurchaseDue(now) && !order.m_Donor && order.m_iSpawnAtMs == 0)
		{
			AICF_RecruitCapacityState.s_Order = order;
			AICF_RecruitCapacityState.s_iToken = order.m_iToken;
			AICF_RecruitCapacityState.s_iStartedAt = order.m_iStartedAtMs;
			AICF_RecruitCapacityState.s_iInitialAlive = AICF_GroupRuntime.CountAliveAgents(order.m_Group);
			AICF_RecruitCapacityState.s_iLimit = world.GetLimitOfActiveAIs();
			AICF_RecruitCapacityState.s_iPressureAt = now;
			world.SetLimitOfActiveAIs(Math.Max(world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount()));
			order.Log("RECRUIT_CAPACITY_PRESSURE", string.Format("active=%1 intended=%2 hard_limit=%3 original_limit=%4 classification=CONTROLLED_NATIVE_CAPACITY_INJECTION physical_presence=1 original_started_at_ms=%5 paid=0", world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount(), world.GetLimitOfActiveAIs(), AICF_RecruitCapacityState.s_iLimit, order.m_iStartedAtMs));
		}
		bool selected = AICF_RecruitCapacityState.s_Order == order;
		int age = now - AICF_RecruitCapacityState.s_iPressureAt;
		if (selected && !AICF_RecruitCapacityState.s_bReleased)
		{
			if (!order.IsCurrent(order.m_Slot) || !order.IsPhysicallyPresent() || order.m_iToken != AICF_RecruitCapacityState.s_iToken ||
				order.m_iStartedAtMs != AICF_RecruitCapacityState.s_iStartedAt || order.m_Donor || order.m_iSpawnAtMs != 0 || order.m_fPaid != 0)
				AICF_RecruitCapacityState.Finish(false, "WAIT_MUTATED_IDENTITY_RESOURCE_OR_DEADLINE");
			if (age >= 35000 && !AICF_RecruitCapacityState.s_bHeld)
			{
				AICF_SpawnAdmission admission = InspectRecruitAdmission(order);
				if (!order.m_bNativeCapacityWaiting || !admission || admission.CanAdmit())
					AICF_RecruitCapacityState.Finish(false, "NATIVE_CAPACITY_WAIT_UNPROVEN");
				else
				{
					AICF_RecruitCapacityState.s_bHeld = true;
					order.Log("RECRUIT_CAPACITY_HOLD", admission.Details() + string.Format(" held_ms=%1 donor_created=0 request_issued=0 paid=0 token_unchanged=1 started_at_ms=%2 visit_deadline_reset=0", age, order.m_iStartedAtMs));
				}
			}
			if (age >= 65000 && AICF_RecruitCapacityState.s_bHeld)
			{
				AICF_RecruitCapacityState.Restore();
				AICF_RecruitCapacityState.s_bReleased = true;
				order.Log("RECRUIT_CAPACITY_RELEASE", string.Format("elapsed_ms=%1 active=%2 intended=%3 hard_limit=%4 mechanism=RESTORE_ORIGINAL_NATIVE_LIMIT started_at_ms=%5 token_unchanged=1", age, world.GetCurrentNumOfActiveAIs(), world.GetActiveGroupIntendedCount(), world.GetLimitOfActiveAIs(), order.m_iStartedAtMs));
			}
		}
		string result = super.Tick(order, graphReady);
		if (selected && AICF_RecruitCapacityState.s_bReleased && !AICF_RecruitCapacityState.s_bDone &&
			AICF_GroupRuntime.CountAliveAgents(order.m_Group) == AICF_RecruitCapacityState.s_iInitialAlive + 1 && !order.m_Donor)
		{
			IEntity member = order.m_Slot.GetRosterMember(order.m_iMemberIndex);
			if (!member || !order.IsCurrent(order.m_Slot) || !order.IsPhysicallyPresent() || order.m_iStartedAtMs != AICF_RecruitCapacityState.s_iStartedAt)
				AICF_RecruitCapacityState.Finish(false, "JOIN_POSTCONDITION_MISMATCH");
			else
			{
				order.Log("RECRUIT_CAPACITY_JOIN", string.Format("member=%1 actual=%2 physical_presence=1 donor_cleared=1 started_at_ms=%3 token_unchanged=1 visit_deadline_reset=0", member.GetID(), AICF_GroupRuntime.CountAliveAgents(order.m_Group), order.m_iStartedAtMs));
				AICF_RecruitCapacityState.Finish(true, "NATIVE_WAIT_RELEASE_PAID_JOIN");
			}
		}
		if (selected && !result.IsEmpty()) AICF_RecruitCapacityState.Finish(false, "PRODUCTION_FINISHED:" + result);
		return result;
	}
}

modded class AICF_MatchController
{
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aRecruitCapacityBuildings = {};
	protected int m_iRecruitCapacityStart;
	override protected void TryLogRosterReady()
	{
		super.TryLogRosterReady();
		if (!m_bRosterReady || AICF_RecruitCapacityState.s_bStarted || !AICF_RecruitCapacityState.Enabled()) return;
		AICF_RecruitCapacityState.s_bStarted = true;
		m_iRecruitCapacityStart = System.GetTickCount();
		m_Construction.Stop();
		if (m_BaseBuilders) m_BaseBuilders.Stop();
		SCR_CampaignMilitaryBaseComponent base = m_USSRFaction.GetMainBase();
		vector position = base.GetMasterProvider().GetOwner().GetOrigin() + "50 0 30";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab("USSR", AICF_EConstructionType.SMALL_BARRACKS);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_EditorLinkComponent.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_EditorLinkComponent.IgnoreSpawning(false);
		if (entity)
		{
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
			if (affiliation) affiliation.SetAffiliatedFaction(m_USSRFaction);
			SCR_CampaignBuildingCompositionComponent building = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (building) { building.SetProviderEntity(base.GetMasterProvider().GetOwner()); m_aRecruitCapacityBuildings.Insert(building); }
		}
		Print("[AICF][RECRUIT_CAPACITY_START] classification=CONTROLLED_NATIVE_CAPACITY_INJECTION fixture_barracks=1 fixture_supplies=1 automatic_recruit_suppressed=1 no_heal=1 no_teleport=1 hold_ms=65000 native_deadline_ms=30000");
		GetGame().GetCallqueue().CallLater(AICF_RecruitCapacityTick, 250, true);
	}
	protected void AICF_RecruitCapacityTick()
	{
		if (AICF_RecruitCapacityState.s_bDone) return;
		foreach (SCR_CampaignBuildingCompositionComponent building : m_aRecruitCapacityBuildings)
			if (building && !building.IsCompositionSpawned() && building.GetCompositionLayout())
				building.GetCompositionLayout().AddBuildingValue(building.GetCompositionLayout().GetToBuildValue());
		SCR_CampaignMilitaryBaseComponent base = m_USSRFaction.GetMainBase();
		if (base && base.GetSupplies() < 1000) base.AddSupplies(1000 - base.GetSupplies());
		if (System.GetTickCount() - m_iRecruitCapacityStart > 300000) AICF_RecruitCapacityState.Finish(false, "CONTROLLED_SETUP_OR_RELEASE_DEADLINE");
	}
	override protected void Stop(bool cleanupEntities)
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_RecruitCapacityTick);
		AICF_RecruitCapacityState.Restore();
		super.Stop(cleanupEntities);
	}
}
