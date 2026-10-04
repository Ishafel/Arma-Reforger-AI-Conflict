// Только изолированный runtime stage. Казарма/смерть/баланс подготавливаются fixture;
// replacement и завершение матча проходят production path. Никакого client/visual gate.
class AICF_VictoryProbePolicy : AICF_VictorySystem
{
	bool CheckObjectives(AICF_ObjectiveGraph graph, FactionKey key)
	{
		return ControlsAllObjectives(graph, key);
	}
}

modded class SCR_CampaignMilitaryBaseComponent
{
	// Контролируемый contested input только для проверки production predicate.
	int m_iAICFVictoryProbePresence = -1;
	bool m_bAICFVictoryProbeContested;
	override SCR_EBaseCaptureState GetCaptureState()
	{
		if (m_bAICFVictoryProbeContested)
			return SCR_EBaseCaptureState.CONTESTED;
		return super.GetCaptureState();
	}
	override bool AreEnemiesPresent()
	{
		if (m_iAICFVictoryProbePresence >= 0)
			return m_iAICFVictoryProbePresence == 1;
		return super.AreEnemiesPresent();
	}
}

modded class AICF_MatchController
{
	protected int m_iVictoryProbeStarted;
	protected int m_iVictoryProbeGeneration;
	protected int m_iVictoryProbeTickets;
	protected int m_iVictoryProbePhase;
	protected bool m_bVictoryProbeFailed;
	protected vector m_vVictoryProbeDeath;
	protected SCR_CampaignBuildingCompositionComponent m_VictoryProbeBarracks;
	protected ref AICF_VictoryProbePolicy m_VictoryProbePolicy = new AICF_VictoryProbePolicy();

	override protected void Update()
	{
		string enabled;
		if (System.GetCLIParam("aicfVictoryRespawnProbe", enabled) && enabled == "1" && m_bRosterReady && !m_bStopped)
			VictoryProbeTick();
		super.Update();
	}

	protected void VictoryProbeCheck(string name, bool pass)
	{
		if (!pass)
			m_bVictoryProbeFailed = true;
		Print(string.Format("[AICF][VICTORY_RESPAWN_PROBE] case=%1 pass=%2", name, pass));
	}

	protected void VictoryProbeTick()
	{
		int now = System.GetTickCount();
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		SCR_CampaignMilitaryBaseComponent base = m_USFaction.GetMainBase();
		if (!m_iVictoryProbeStarted)
		{
			m_iVictoryProbeStarted = now;
			m_Construction.Stop();
			m_InfantryRecruitment.Stop();
			for (int index; index < m_USState.GetSlotCount(); index++)
			{
				m_USState.GetSlot(index).SetDesiredSize(1);
				m_USSRState.GetSlot(index).SetDesiredSize(1);
			}
			VictoryProbeCheck("NO_TERRITORY_AT_START", !m_VictoryProbePolicy.CheckObjectives(m_ObjectiveGraph, m_USFaction.GetFactionKey()));
			VictoryProbeCheck("NULL_GRAPH", !m_VictoryProbePolicy.CheckObjectives(null, m_USFaction.GetFactionKey()));
			VictoryProbeCheck("NO_BARRACKS", m_ConflictAdapter.GetReplacementSpawnRejectionReason(base, m_USFaction) == "NO_ONLINE_BARRACKS");
			VictoryProbePlace(base);
		}
		if (m_VictoryProbeBarracks && !m_VictoryProbeBarracks.IsCompositionSpawned() && m_VictoryProbeBarracks.GetCompositionLayout())
		{
			SCR_CampaignBuildingLayoutComponent layout = m_VictoryProbeBarracks.GetCompositionLayout();
			layout.AddBuildingValue(layout.GetToBuildValue());
		}
		base.AddSupplies(base.GetSuppliesMax() - base.GetSupplies());
		if (m_iVictoryProbePhase == 0 && m_ConflictAdapter.GetReplacementSpawnRejectionReason(base, m_USFaction).IsEmpty())
		{
			IEntity member = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			if (!member)
				return;
			m_iVictoryProbeGeneration = slot.GetSpawnGeneration();
			m_iVictoryProbeTickets = m_USState.GetTickets();
			m_vVictoryProbeDeath = member.GetOrigin();
			m_iVictoryProbePhase = 1;
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(member.FindComponent(SCR_CharacterDamageManagerComponent));
			damage.Kill(Instigator.CreateInstigatorGM());
		}
		if (m_iVictoryProbePhase == 1 && slot.GetState() == AICF_EGroupSlotState.WAITING)
		{
			vector origin;
			VictoryProbeCheck("DEATH_ORIGIN", slot.TryGetReplacementOrigin(origin) && vector.DistanceSqXZ(origin, m_vVictoryProbeDeath) < 4);
			m_iVictoryProbePhase = 2;
		}
		if (m_iVictoryProbePhase == 2 && slot.IsCombatReady() && !slot.IsReplacementDeployment() && slot.GetSpawnGeneration() > m_iVictoryProbeGeneration)
		{
			VictoryProbeCheck("ONE_TICKET", m_USState.GetTickets() == m_iVictoryProbeTickets - 1);
			IEntity replacement = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			VictoryProbeCheck("RESPAWN_AT_BARRACKS_BASE", replacement && vector.DistanceSqXZ(replacement.GetOrigin(), base.GetSpawnPoint().GetOrigin()) < 40000);
			VictoryProbeEnd();
			return;
		}
		if (now - m_iVictoryProbeStarted > 180000)
		{
			VictoryProbeCheck("TIMEOUT", false);
			VictoryProbeCloseSoon();
		}
	}

	protected void VictoryProbeEnd()
	{
		m_iVictoryProbePhase = 3;
		// Реальная база в минимальном графе проверяет owner/contested/HQ predicate.
		SCR_CampaignMilitaryBaseComponent objective;
		for (int nodeId; nodeId < m_ObjectiveGraph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode node = m_ObjectiveGraph.GetNode(nodeId);
			if (node.IsObjective() && !node.GetBase().IsHQ() && node.GetBase().GetCaptureState() == SCR_EBaseCaptureState.NONE && !node.GetBase().IsBeingCaptured())
			{
				objective = node.GetBase();
				break;
			}
		}
		if (objective)
		{
			SCR_CampaignFaction oldFaction = SCR_CampaignFaction.Cast(objective.GetFaction());
			objective.SetFaction(m_USFaction);
			objective.m_iAICFVictoryProbePresence = 0;
			array<SCR_CampaignMilitaryBaseComponent> bases = {objective, m_USFaction.GetMainBase(), m_USSRFaction.GetMainBase()};
			AICF_ObjectiveGraph graph = new AICF_ObjectiveGraph();
			graph.Build(bases, bases);
			VictoryProbeCheck("ALL_OWNED_HQ_EXCLUDED", m_VictoryProbePolicy.CheckObjectives(graph, m_USFaction.GetFactionKey()));
			VictoryProbeCheck("OTHER_OWNER", !m_VictoryProbePolicy.CheckObjectives(graph, m_USSRFaction.GetFactionKey()));
			objective.m_iAICFVictoryProbePresence = 1;
			VictoryProbeCheck("PRESENCE_ONLY_ALLOWED", m_VictoryProbePolicy.CheckObjectives(graph, m_USFaction.GetFactionKey()));
			objective.m_bAICFVictoryProbeContested = true;
			VictoryProbeCheck("CONTESTED_BLOCKS", !m_VictoryProbePolicy.CheckObjectives(graph, m_USFaction.GetFactionKey()));
			objective.m_bAICFVictoryProbeContested = false;
			objective.m_iAICFVictoryProbePresence = -1;
			objective.SetFaction(oldFaction);
		}
		else
			VictoryProbeCheck("OBJECTIVE_FIXTURE", false);
		// Списываем настоящие тикеты, сохраняя живые отряды и незахваченные точки.
		while (m_USSRState.CanAffordDeployment(AICF_EDeploymentKind.REPLACEMENT))
		{
			m_USSRState.TryReserveDeployment(AICF_EDeploymentKind.REPLACEMENT);
			m_USSRState.TryCommitDeployment(AICF_EDeploymentKind.REPLACEMENT);
		}
		VictoryProbeCheck("LIVE_ENEMY", m_USSRState.GetSlot(0).IsCombatReady());
		VictoryProbeCheck("TICKETS_OR_TERRITORY", m_VictorySystem.EvaluateAndEnd(m_Campaign, m_USState, m_USSRState, null));
		VictoryProbeCheck("EXACTLY_ONCE", !m_VictorySystem.EvaluateAndEnd(m_Campaign, m_USState, m_USSRState, null));
		VictoryProbeCloseSoon();
	}

	protected void VictoryProbePlace(SCR_CampaignMilitaryBaseComponent base)
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
		if (!entity)
			return;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation)
			affiliation.SetAffiliatedFaction(m_USFaction);
		m_VictoryProbeBarracks = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (m_VictoryProbeBarracks)
			m_VictoryProbeBarracks.SetProviderEntity(base.GetMasterProvider().GetOwner());
	}

	protected void VictoryProbeCloseSoon()
	{
		Print(string.Format("[AICF][VICTORY_RESPAWN_PROBE] finished=1 pass=%1 phase=%2", !m_bVictoryProbeFailed, m_iVictoryProbePhase));
		Stop(true);
		GetGame().GetCallqueue().Remove(VictoryProbeClose);
		GetGame().GetCallqueue().CallLater(VictoryProbeClose, 2000, false);
	}

	protected void VictoryProbeClose()
	{
		GetGame().GetCallqueue().Remove(VictoryProbeClose);
		GetGame().RequestClose();
	}

	void ~AICF_MatchController()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(VictoryProbeClose);
	}
}
