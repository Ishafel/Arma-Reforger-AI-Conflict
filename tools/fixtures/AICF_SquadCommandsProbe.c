// Только terminal fixture. Реальные barracks, движение, покупка и planner;
// подготовка службы бесплатна. Не доказывает клиентский RPC или UI.
modded class AICF_MatchController
{
	protected int m_iSquadProbeStart;
	protected int m_iSquadProbePhase;
	protected int m_iSquadProbeRetry;
	protected int m_iSquadProbeIntent;
	protected int m_iSquadProbeAssignment;
	protected EntityID m_SquadProbeGroup;
	protected vector m_vSquadProbePoint;
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aSquadProbeBuildings = {};

	override protected void Update()
	{
		super.Update();
		string flag;
		if (System.GetCLIParam("aicfSquadProbe", flag) && flag == "1" && m_bRosterReady && !m_bStopped)
			SquadProbeUpdate();
	}

	protected void SquadProbeCheck(bool passed, string name)
	{
		Print(string.Format("[AICF][SQUAD_PROBE] check=%1 passed=%2", name, passed));
	}

	protected void SquadProbeUpdate()
	{
		int now = System.GetTickCount();
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		if (m_iSquadProbeStart == 0)
		{
			m_iSquadProbeStart = now;
			m_Construction.Stop();
			for (int i = 0; i < 10; i++)
			{
				m_USState.GetSlot(i).SetDesiredSize(1);
				m_USSRState.GetSlot(i).SetDesiredSize(1);
			}
			slot.SetDesiredSize(2);
			m_SquadProbeGroup = slot.GetGroup().GetID();
			m_vSquadProbePoint = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup()).GetOrigin();
			SquadProbeCheck(m_OrderPlanner.AssignPlayerPointOrder(slot, m_USFaction, m_vSquadProbePoint), "takeover");
			m_iSquadProbeIntent = slot.GetStrategicIntentRevision();
			m_iSquadProbeAssignment = slot.GetStrategicAssignmentRevision();
			string rejection;
			SquadProbeCheck(!RequestPlayerSquadCommand(null, 0, AICF_ESquadCommand.RETURN_TO_AI, rejection), "null_requester_rejected");
			SquadProbePlace(m_USFaction);
		}
		foreach (SCR_CampaignBuildingCompositionComponent composition : m_aSquadProbeBuildings)
		{
			if (composition && !composition.IsCompositionSpawned() && composition.GetCompositionLayout())
			{
				SCR_CampaignBuildingLayoutComponent layout = composition.GetCompositionLayout();
				layout.AddBuildingValue(layout.GetToBuildValue());
			}
		}
		if (m_iSquadProbePhase == 0 && now >= m_iSquadProbeRetry)
		{
			m_iSquadProbeRetry = now + 3000;
			string reason;
			if (m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason))
			{
				SquadProbeCheck(slot.IsRecruitingInfantry() && slot.HasPlayerStrategicIntent() &&
					slot.GetStrategicIntentRevision() == m_iSquadProbeIntent && slot.GetStrategicAssignmentRevision() != m_iSquadProbeAssignment,
					"manual_visit_preserves_intent_invalidates_old_assignment");
				int revision = slot.GetStrategicAssignmentRevision();
				m_USAICommander.AssignOrder(slot, "SQUAD_PROBE");
				SquadProbeCheck(slot.GetStrategicAssignmentRevision() == revision, "commander_cannot_interrupt_visit");
				SquadProbeCheck(!m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason), "duplicate_rejected");
				m_iSquadProbePhase = 1;
			}
			else
				Print("[AICF][SQUAD_PROBE] waiting=" + reason);
		}
		if (m_iSquadProbePhase == 1 && AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) == 2 && !slot.IsRecruitingInfantry())
		{
			SquadProbeCheck(slot.GetGroup().GetID() == m_SquadProbeGroup && slot.HasPlayerStrategicIntent() &&
				slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION && slot.GetTargetPosition() == m_vSquadProbePoint,
				"paid_recruit_and_previous_point_restored");
			string reason;
			SquadProbeCheck(!m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason), "full_squad_rejected");
			// Второй визит отменяется при pending donor до оплаты.
			slot.SetDesiredSize(3);
			if (!m_InfantryRecruitment.RequestPlayerRecruitment(slot, m_USFaction, reason))
			{
				slot.SetDesiredSize(2);
				return;
			}
			m_iSquadProbePhase = 2;
		}
		if (m_iSquadProbePhase == 2 && m_InfantryRecruitment.CountPendingAgents() > 0)
		{
			SquadProbeCheck(m_InfantryRecruitment.CancelForSlot(slot), "cancel_pending_spawn");
			slot.SetDesiredSize(2);
			m_OrderPlanner.ReleasePlayerCommand(slot);
			SquadProbeCheck(m_USAICommander.AssignOrder(slot, "PLAYER_RELEASE"), "ai_assignment");
			SquadProbeCheck(!slot.HasPlayerStrategicIntent() && !slot.HasPlayerStrategicOrder() &&
				slot.GetDecisionAuthority() == AICF_EStrategicDecisionAuthority.AI_COMMANDER &&
				slot.GetGroup().GetID() == m_SquadProbeGroup && m_InfantryRecruitment.CountPendingAgents() == 0,
				"control_returned_stable_group_no_pending");
			m_iSquadProbePhase = 3;
		}
		if (m_iSquadProbePhase == 3 || now - m_iSquadProbeStart > 240000)
		{
			SquadProbeCheck(m_iSquadProbePhase == 3, "finished");
			Stop(true);
			foreach (SCR_CampaignBuildingCompositionComponent placed : m_aSquadProbeBuildings)
			{
				if (placed && placed.GetOwner())
					SCR_EntityHelper.DeleteEntityAndChildren(placed.GetOwner());
			}
			m_aSquadProbeBuildings.Clear();
			GetGame().RequestClose();
		}
	}

	protected void SquadProbePlace(SCR_CampaignFaction faction)
	{
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		if (!base || !base.GetMasterProvider() || !base.GetSpawnPoint())
			return;
		base.AddSupplies(base.GetSuppliesMax() - base.GetSupplies());
		vector position, rotation;
		base.GetSpawnPoint().GetPositionAndRotation(position, rotation);
		position += "70 0 25";
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
		m_aSquadProbeBuildings.Insert(composition);
	}
}
