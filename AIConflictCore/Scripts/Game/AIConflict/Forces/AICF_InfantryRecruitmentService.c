// Authoritative пополнение живых infantry slots. Один визит и один donor на slot;
// polling работает из общего Update, собственных callbacks и CallLater нет.
class AICF_InfantryRecruitmentService
{
	protected SCR_GameModeCampaign m_Campaign;
	protected AICF_OrderPlanner m_Planner;
	protected AICF_EconomySystem m_Economy;
	protected AICF_ObjectiveGraph m_Graph;
	protected AICF_TargetSelector m_Selector;
	protected ref AICF_InfantryRecruitmentConfig m_Config = new AICF_InfantryRecruitmentConfig();
	protected ref AICF_InfantryRecruitSpawner m_Spawner = new AICF_InfantryRecruitSpawner();
	protected ref AICF_ManagedAILODPolicy m_LOD = new AICF_ManagedAILODPolicy();
	protected ref array<ref AICF_InfantryRecruitmentOrder> m_aOrders = {};
	protected ref array<AICF_GroupSlot> m_aRetrySlots = {};
	protected ref array<int> m_aRetryAtMs = {};
	protected int m_iNextToken = 1;
	protected int m_iAvailableAgents;
	protected bool m_bStopped;

	void AICF_InfantryRecruitmentService(SCR_GameModeCampaign campaign, AICF_OrderPlanner planner,
		AICF_EconomySystem economy, AICF_ObjectiveGraph graph, AICF_TargetSelector selector, AICF_GroupSpawner rosterSource)
	{
		m_Campaign = campaign;
		m_Planner = planner;
		m_Economy = economy;
		m_Graph = graph;
		m_Selector = selector;
		m_Spawner.SetRosterSource(rosterSource);
	}

	int CountPendingAgents()
	{
		int count;
		foreach (AICF_InfantryRecruitmentOrder order : m_aOrders)
		{
			if (order.m_Donor)
				count += Math.Max(1, order.m_Donor.GetAgentsCount());
		}
		return count;
	}

	void Update(AICF_FactionState us, SCR_CampaignFaction usFaction, AICF_FactionState ussr,
		SCR_CampaignFaction ussrFaction, int availableAgents, bool graphReady)
	{
		if (m_bStopped || !Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster() || !m_Campaign.IsRunning())
			return;
		m_iAvailableAgents = Math.Max(0, availableAgents);
		for (int index = m_aOrders.Count() - 1; index >= 0; index--)
		{
			AICF_InfantryRecruitmentOrder order = m_aOrders[index];
			string finished = Tick(order, graphReady);
			if (!finished.IsEmpty())
				Finish(index, finished, true);
		}
		if (graphReady)
		{
			ConsiderFaction(us, usFaction);
			ConsiderFaction(ussr, ussrFaction);
		}
	}

	protected void ConsiderFaction(AICF_FactionState state, SCR_CampaignFaction faction)
	{
		if (!state || !faction)
			return;
		for (int index = 0; index < state.GetSlotCount(); index++)
		{
			AICF_GroupSlot slot = state.GetSlot(index);
			if (!m_Planner.CanRecruitInfantry(slot, faction) || HasOrder(slot))
				continue;
			int retry = m_aRetrySlots.Find(slot);
			if (retry >= 0 && System.GetTickCount() < m_aRetryAtMs[retry])
				continue;
			ScheduleRetry(slot);
			IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());
			if (!leader || AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(slot.GetGroup()) > 0)
				continue;
			ResourceName prefab;
			string role;
			int memberIndex;
			if (!m_Spawner.FindMissingMember(slot, faction, prefab, role, memberIndex))
				continue;
			AICF_InfantryRecruitmentOrder order = SelectBarracks(slot, faction, leader.GetOrigin(), m_Config.Cost(role));
			if (!order || !m_Planner.BeginInfantryRecruitment(order))
				continue;
			m_aOrders.Insert(order);
			order.Log("INFANTRY_RECRUITMENT_STARTED", string.Format(
				"alive=%1 desired=%2 distance_m=%3 max_distance_m=500 intent_revision=%4 graph_revision=%5",
				AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()), slot.GetDesiredSize(),
				vector.DistanceXZ(leader.GetOrigin(), order.m_vPosition), order.m_iIntent, order.m_iGraphRevision));
		}
	}

	protected AICF_InfantryRecruitmentOrder SelectBarracks(AICF_GroupSlot slot, SCR_CampaignFaction faction,
		vector position, int cost)
	{
		SCR_CampaignMilitaryBaseComponent nearest;
		float nearestDistance = float.MAX;
		for (int index = 0; index < m_Graph.GetNodeCount(); index++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(index);
			if (!node || !node.GetBase() || !node.GetBase().GetOwner())
				continue;
			float distance = vector.DistanceSqXZ(position, node.GetBase().GetOwner().GetOrigin());
			if (distance < nearestDistance)
			{
				nearest = node.GetBase();
				nearestDistance = distance;
			}
		}
		SCR_CampaignMilitaryBaseComponent target = slot.GetStrategicIntentTargetBase();
		vector targetPosition;
		if (!nearest || !target || !m_Planner.TryResolveSlotTargetPosition(slot, target, targetPosition))
			return null;
		float targetDistance = vector.DistanceSqXZ(position, targetPosition);
		AICF_InfantryRecruitmentOrder best;
		float bestDistance = float.MAX;
		for (int nodeId = 0; nodeId < m_Graph.GetNodeCount(); nodeId++)
		{
			AICF_ObjectiveNode candidateNode = m_Graph.GetNode(nodeId);
			if (!candidateNode)
				continue;
			SCR_CampaignMilitaryBaseComponent base = candidateNode.GetBase();
			if (!base || !base.GetOwner() || base.GetFaction() != faction || base.GetSupplies() < cost)
				continue;
			if (base != nearest && m_Graph.GetHopDistance(nearest, base) != 1 && m_Graph.GetHopDistance(base, nearest) != 1)
				continue;
			array<SCR_ServicePointComponent> services = {};
			base.GetServices(services);
			foreach (SCR_ServicePointComponent service : services)
			{
				if (!service || !service.GetOwner())
					continue;
				vector servicePosition = service.GetOwner().GetOrigin();
				float distanceSq = vector.DistanceSqXZ(position, servicePosition);
				if (distanceSq > AICF_InfantryRecruitmentConfig.MAX_DISTANCE_METERS * AICF_InfantryRecruitmentConfig.MAX_DISTANCE_METERS ||
					(base != nearest && distanceSq > AICF_InfantryRecruitmentConfig.ARRIVAL_METERS * AICF_InfantryRecruitmentConfig.ARRIVAL_METERS && distanceSq >= targetDistance) ||
					distanceSq >= bestDistance)
					continue;
				AICF_InfantryRecruitmentOrder order = new AICF_InfantryRecruitmentOrder();
				order.m_Slot = slot;
				order.m_Faction = faction;
				order.m_Group = slot.GetGroup();
				order.m_GroupId = order.m_Group.GetID();
				order.m_iGeneration = slot.GetSpawnGeneration();
				order.m_iIntent = slot.GetStrategicIntentRevision();
				order.m_iGraphRevision = m_Graph.GetRevision();
				order.m_iToken = m_iNextToken++;
				order.m_Base = base;
				order.m_BaseId = base.GetOwner().GetID();
				order.m_Service = service;
				order.m_ServiceId = service.GetOwner().GetID();
				order.m_vPosition = servicePosition;
				order.m_iStartedAtMs = System.GetTickCount();
				if (!order.HasSafeBarracks())
					continue;
				best = order;
				bestDistance = distanceSq;
			}
		}
		return best;
	}

	protected string Tick(AICF_InfantryRecruitmentOrder order, bool graphReady)
	{
		if (!order.IsCurrent(order.m_Slot))
			return "IDENTITY_CHANGED";
		if (!graphReady || order.m_iGraphRevision != m_Graph.GetRevision())
			return "GRAPH_CHANGED";
		if (!order.HasSafeBarracks())
			return "BARRACKS_UNAVAILABLE";
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(order.m_Group);
		if (!leader || vector.DistanceSqXZ(leader.GetOrigin(), order.m_vPosition) >
			AICF_InfantryRecruitmentConfig.MAX_DISTANCE_METERS * AICF_InfantryRecruitmentConfig.MAX_DISTANCE_METERS)
			return "BARRACKS_OUT_OF_RANGE";
		if (order.m_iSpawnAtMs > 0 && !order.m_Donor)
			return "DONOR_LOST";
		int now = System.GetTickCount();
		if (now - order.m_iStartedAtMs >= AICF_InfantryRecruitmentConfig.VISIT_TIMEOUT_MS)
			return "VISIT_TIMEOUT";
		int alive = AICF_GroupRuntime.CountAliveAgents(order.m_Group);
		if (alive <= 0 || alive >= order.m_Slot.GetDesiredSize())
			return "ROSTER_COMPLETE_OR_EMPTY";
		if (!order.m_Donor && !m_Spawner.FindMissingMember(order.m_Slot, order.m_Faction, order.m_sPrefab, order.m_sRole, order.m_iMemberIndex))
			return "ROSTER_UNAVAILABLE";
		order.m_iCost = m_Config.Cost(order.m_sRole);
		if (!m_Economy.QuoteInfantryRecruit(order))
			return "SUPPLIES_UNAVAILABLE";
		if (!order.IsPhysicallyPresent())
		{
			if (order.m_iArrivedAtMs > 0 || now - order.m_iStartedAtMs >= AICF_InfantryRecruitmentConfig.APPROACH_TIMEOUT_MS)
				return "APPROACH_INTERRUPTED_OR_TIMEOUT";
			return string.Empty;
		}
		if (order.m_iArrivedAtMs == 0)
		{
			order.m_iArrivedAtMs = now;
			order.Log("INFANTRY_RECRUITMENT_ARRIVED", "physical_presence=1");
		}
		if (!order.m_Donor)
		{
			if (now < order.m_iNextPurchaseAtMs || CountPendingAgents() >= 2)
				return string.Empty;
			if (m_iAvailableAgents <= 0)
				return "AGENT_LIMIT";
			if (!m_Spawner.BeginRecruit(order))
				return "SPAWN_REJECTED";
			m_iAvailableAgents--;
			order.Log("INFANTRY_RECRUIT_SPAWN_REQUESTED", string.Format(
				"role=%1 prefab=%2 cost=%3 donor=%4 paid=0", order.m_sRole, order.m_sPrefab, order.m_iCost, order.m_DonorId));
			return string.Empty;
		}
		if (order.m_Donor.GetID() != order.m_DonorId)
			return "DONOR_IDENTITY_CHANGED";
		int actual, wrongFaction, dead;
		if (!AICF_GroupRuntime.HasExactFactionRoster(order.m_Donor, order.m_Faction.GetFactionKey(), 1, actual, wrongFaction, dead))
		{
			if (wrongFaction > 0 || dead > 0 || actual > 1 || now - order.m_iSpawnAtMs >= AICF_InfantryRecruitmentConfig.SPAWN_TIMEOUT_MS)
				return "SPAWN_INVALID_OR_TIMEOUT";
			return string.Empty;
		}
		array<AIAgent> recruits = {};
		order.m_Donor.GetAgents(recruits);
		AIAgent recruit = recruits[0];
		// Stock campaign randomizer законно материализует другой concrete prefab.
		// Provenance задаётся exact donor + pending recipe position и живой faction roster.
		if (!recruit || !recruit.GetControlledEntity() || recruit.GetParentGroup() != order.m_Donor ||
			order.m_Slot.HasRosterMember(order.m_iMemberIndex))
			return "RECRUIT_IDENTITY_CHANGED";
		if (vector.DistanceSqXZ(recruit.GetControlledEntity().GetOrigin(), order.m_vPosition) >
			AICF_InfantryRecruitmentConfig.ARRIVAL_METERS * AICF_InfantryRecruitmentConfig.ARRIVAL_METERS)
			return "RECRUIT_OUTSIDE_BARRACKS";
		CharacterControllerComponent characterController = CharacterControllerComponent.Cast(recruit.GetControlledEntity().FindComponent(CharacterControllerComponent));
		RplComponent recruitRpl = RplComponent.Cast(recruit.GetControlledEntity().FindComponent(RplComponent));
		if (!characterController || characterController.IsPlayerControlled() || !recruitRpl || !recruitRpl.IsMaster())
			return "RECRUIT_AUTHORITY_CHANGED";
		int managed, recovered;
		if (AICF_ManagedAICombatPolicy.Apply(order.m_Donor) != 1 || !m_LOD.KeepCaptureEligible(order.m_Donor, managed, recovered))
			return "RECRUIT_NOT_READY";
		if (!m_Economy.DebitInfantryRecruit(order))
			return "PAYMENT_REJECTED";
		order.m_Donor.RemoveAgent(recruit);
		order.m_Group.AddAgent(recruit);
		if (recruit.GetParentGroup() != order.m_Group)
		{
			if (!recruit.GetParentGroup())
				order.m_Donor.AddAgent(recruit);
			m_Economy.RefundInfantryRecruit(order);
			return "TRANSFER_FAILED";
		}
		m_Economy.CommitInfantryRecruit(order);
		order.m_Slot.RecordRecruitedMember(order.m_iMemberIndex, recruit.GetControlledEntity());
		order.Log("INFANTRY_RECRUIT_JOINED", string.Format(
			"role=%1 cost=%2 alive=%3 desired=%4 character=%5 skill=VETERAN",
			order.m_sRole, order.m_iCost, AICF_GroupRuntime.CountAliveAgents(order.m_Group),
			order.m_Slot.GetDesiredSize(), recruit.GetControlledEntity().GetID()));
		if (!m_Spawner.ClearRecruit(order))
			return "DONOR_CLEANUP_BLOCKED";
		order.m_iNextPurchaseAtMs = now + AICF_InfantryRecruitmentConfig.PURCHASE_INTERVAL_MS;
		return string.Empty;
	}

	protected bool HasOrder(AICF_GroupSlot slot)
	{
		foreach (AICF_InfantryRecruitmentOrder order : m_aOrders)
		{
			if (order.m_Slot == slot)
				return true;
		}
		return false;
	}

	protected void ScheduleRetry(AICF_GroupSlot slot)
	{
		int index = m_aRetrySlots.Find(slot);
		if (index < 0)
		{
			index = m_aRetrySlots.Insert(slot);
			m_aRetryAtMs.Insert(0);
		}
		m_aRetryAtMs[index] = System.GetTickCount() + AICF_InfantryRecruitmentConfig.RETRY_MS;
	}

	protected void Finish(int index, string reason, bool restore)
	{
		AICF_InfantryRecruitmentOrder order = m_aOrders[index];
		m_Economy.RefundInfantryRecruit(order);
		m_Planner.EndInfantryRecruitment(order, m_Graph, m_Selector, restore);
		if (!m_Spawner.ClearRecruit(order))
		{
			order.Log("INFANTRY_RECRUIT_CLEANUP_BLOCKED", "reason=IDENTITY_OR_PLAYER_CONTROL");
			return;
		}
		order.Log("INFANTRY_RECRUITMENT_FINISHED", "reason=" + reason);
		ScheduleRetry(order.m_Slot);
		m_aOrders.Remove(index);
	}

	void Stop()
	{
		if (!Replication.IsServer())
			return;
		m_bStopped = true;
		for (int index = m_aOrders.Count() - 1; index >= 0; index--)
			Finish(index, "STOP", false);
		m_aRetrySlots.Clear();
		m_aRetryAtMs.Clear();
	}
}
