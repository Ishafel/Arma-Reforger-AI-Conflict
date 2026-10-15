class AICF_EquipmentSlot
{
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	int m_iGeneration;
	int m_iQuietUntil;
	int m_iEvaluateAt;
	int m_iRetryAt;
	bool m_bSeen;
	ref AICF_EquipmentOrder m_Order;
	ref array<ref AICF_EquipmentMember> m_aMembers = {};
}

// Polling в существующем authority Update: нет собственных CallLater или event subscriptions.
class AICF_EquipmentService
{
	protected SCR_GameModeCampaign m_Campaign;
	protected AICF_OrderPlanner m_Planner;
	protected AICF_ObjectiveGraph m_Graph;
	protected AICF_TargetSelector m_Selector;
	protected AICF_GroupSpawner m_RosterSource;
	protected AICF_VehicleCoordinator m_Vehicles;
	protected ref AICF_EquipmentConfig m_Config = new AICF_EquipmentConfig();
	protected ref AICF_EquipmentApplicator m_Applicator = new AICF_EquipmentApplicator();
	protected ref array<ref AICF_EquipmentSlot> m_aSlots = {};
	protected ref array<ref AICF_EquipmentPlan> m_aPlans = {};
	protected ref array<IEntity> m_aCorpses = {};
	protected ref array<SCR_ArsenalComponent> m_aArsenals = {};
	protected ref array<ref AICF_EquipmentMember> m_aRecoveries = {};
	protected int m_iNextToken;
	protected bool m_bStopped;

	void AICF_EquipmentService(SCR_GameModeCampaign campaign, AICF_OrderPlanner planner,
		AICF_ObjectiveGraph graph, AICF_TargetSelector selector, AICF_GroupSpawner rosterSource)
	{
		m_Campaign = campaign;
		m_Planner = planner;
		m_Graph = graph;
		m_Selector = selector;
		m_RosterSource = rosterSource;
	}

	protected AICF_EquipmentPlan Plan(AICF_GroupSlot slot, SCR_CampaignFaction faction, int index)
	{
		string role;
		ResourceName source = m_RosterSource.ResolveRecruitPrefab(faction, index, role);
		AICF_LoadoutBinding binding = slot.GetLoadout(index);
		string key = faction.GetFactionKey() + source;
		if (binding) key += binding.m_sInventory;
		foreach (AICF_EquipmentPlan existing : m_aPlans)
			if (existing.m_sKey == key) return existing;
		AICF_EquipmentPlan plan = new AICF_EquipmentPlan();
		plan.m_sKey = key;
		if (!plan.Build(source, faction, binding)) return null;
		m_aPlans.Insert(plan);
		return plan;
	}

	protected void Discover(AICF_FactionState state, SCR_CampaignFaction faction, int now)
	{
		if (!state || !faction) return;
		for (int index; index < state.GetSlotCount(); index++)
		{
			AICF_GroupSlot slot = state.GetSlot(index);
			if (!slot || !slot.IsCombatReady() || slot.GetUnitType() != AICF_EGroupUnitType.INFANTRY) continue;
			AICF_EquipmentSlot record;
			foreach (AICF_EquipmentSlot existing : m_aSlots)
				if (existing.m_Slot == slot && existing.m_Group == slot.GetGroup() &&
					existing.m_iGeneration == slot.GetSpawnGeneration()) record = existing;
			if (!record)
			{
				record = new AICF_EquipmentSlot();
				record.m_Slot = slot;
				record.m_Group = slot.GetGroup();
				record.m_GroupId = record.m_Group.GetID();
				record.m_iGeneration = slot.GetSpawnGeneration();
				record.m_iQuietUntil = now + AICF_EquipmentConfig.QUIET_MS;
				m_aSlots.Insert(record);
			}
			record.m_bSeen = true;
			foreach (AICF_EquipmentMember unseen : record.m_aMembers) unseen.m_bSeen = false;
			for (int memberIndex; memberIndex < slot.GetDesiredSize(); memberIndex++)
			{
				IEntity character = slot.GetRosterMember(memberIndex);
				if (!character || !AICF_EquipmentInventory.SafeMember(character, record.m_Group)) continue;
				AICF_EquipmentMember member;
				foreach (AICF_EquipmentMember found : record.m_aMembers)
					if (found.m_iIndex == memberIndex && found.m_Character == character && found.m_CharacterId == character.GetID()) member = found;
				if (!member)
				{
					member = new AICF_EquipmentMember();
					member.m_Slot = slot;
					member.m_Group = record.m_Group;
					member.m_GroupId = record.m_GroupId;
					member.m_iGeneration = record.m_iGeneration;
					member.m_iIndex = memberIndex;
					member.m_Character = character;
					member.m_CharacterId = character.GetID();
					member.m_Faction = faction;
					member.m_iRevision = -1;
					record.m_aMembers.Insert(member);
				}
				member.m_bSeen = true;
				int revision;
				AICF_LoadoutBinding binding = slot.GetLoadout(memberIndex);
				if (binding) revision = binding.m_iRevision;
				if (member.m_iRevision != revision || !member.m_Plan)
				{
					member.m_Plan = Plan(slot, faction, memberIndex);
					member.m_iRevision = revision;
					record.m_iEvaluateAt = 0;
				}
			}
			for (int removeIndex = record.m_aMembers.Count() - 1; removeIndex >= 0; removeIndex--)
				if (!record.m_aMembers[removeIndex].m_bSeen) record.m_aMembers.Remove(removeIndex);
			for (int memberSort = 1; memberSort < record.m_aMembers.Count(); memberSort++)
			{
				AICF_EquipmentMember value = record.m_aMembers[memberSort];
				int previous = memberSort - 1;
				while (previous >= 0 && record.m_aMembers[previous].m_iIndex > value.m_iIndex)
				{
					record.m_aMembers[previous + 1] = record.m_aMembers[previous];
					previous--;
				}
				record.m_aMembers[previous + 1] = value;
			}
		}
	}

	protected bool AddCorpse(IEntity entity)
	{
		if (AICF_EquipmentInventory.Corpse(entity) && !m_aCorpses.Contains(entity)) m_aCorpses.Insert(entity);
		return true;
	}

	protected bool AddArsenal(IEntity entity)
	{
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(entity.FindComponent(SCR_ArsenalComponent));
		if (arsenal && !m_aArsenals.Contains(arsenal)) m_aArsenals.Insert(arsenal);
		return true;
	}

	protected bool HasUsefulItem(AICF_EquipmentOrder order, IEntity corpse)
	{
		array<IEntity> items = {};
		AICF_EquipmentInventory.Manager(corpse).GetItems(items);
		foreach (AICF_EquipmentMember member : order.m_aMembers)
		{
			if (!member.m_bPending || !member.Current()) continue;
			foreach (IEntity item : items)
			{
				if (!member.Wants(item)) continue;
				EStoragePurpose purpose = EStoragePurpose.PURPOSE_DEPOSIT;
				BaseWeaponComponent carriedWeapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
				if (carriedWeapon && carriedWeapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER) purpose = EStoragePurpose.PURPOSE_WEAPON_PROXY;
				InventoryStorageManagerComponent target = AICF_EquipmentInventory.Manager(member.m_Character);
				BaseInventoryStorageComponent storage = target.FindStorageForItem(item, purpose);
				if (storage && target.CanMoveItemToStorage(item, storage)) return true;
			}
		}
		return false;
	}

	protected bool SelectCorpse(AICF_EquipmentOrder order)
	{
		m_aCorpses.Clear();
		foreach (AICF_EquipmentMember member : order.m_aMembers)
			if (member.Current() && member.m_bPending)
				GetGame().GetWorld().QueryEntitiesBySphere(member.m_Character.GetOrigin(), AICF_EquipmentConfig.SEARCH_METERS, AddCorpse, null, EQueryEntitiesFlags.DYNAMIC);
		IEntity best;
		float bestDistance = float.MAX;
		foreach (IEntity corpse : m_aCorpses)
		{
			if (!AICF_EquipmentInventory.Corpse(corpse) || order.m_aRejected.Contains(corpse.GetID()) || !HasUsefulItem(order, corpse)) continue;
			float distance = float.MAX;
			foreach (AICF_EquipmentMember recipient : order.m_aMembers)
				if (recipient.Current() && recipient.m_bPending) distance = Math.Min(distance, vector.DistanceSq(recipient.m_Character.GetOrigin(), corpse.GetOrigin()));
			if (distance > AICF_EquipmentConfig.SEARCH_METERS * AICF_EquipmentConfig.SEARCH_METERS ||
				!m_Planner.CanEquipmentRoute(order.m_Group, corpse.GetOrigin(), 1)) continue;
			if (!best || AICF_EquipmentConfig.Prefer(distance, corpse.GetID(), bestDistance, best.GetID()))
			{
				best = corpse;
				bestDistance = distance;
			}
		}
		m_aCorpses.Clear();
		if (!best) return false;
		order.m_Corpse = best;
		order.m_CorpseId = best.GetID();
		order.m_vPosition = best.GetOrigin();
		order.m_iDeadline = System.GetTickCount() + AICF_EquipmentConfig.CORPSE_TIMEOUT_MS;
		if (!m_Planner.BeginEquipmentOrder(order)) return false;
		order.Log("EQUIPMENT_SOURCE", "CORPSE");
		return true;
	}

	protected bool SelectService(AICF_EquipmentOrder order)
	{
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(order.m_Group);
		if (!leader) return false;
		AICF_EquipmentOrder best;
		float bestDistance = float.MAX;
		for (int index; index < m_Graph.GetNodeCount(); index++)
		{
			AICF_ObjectiveNode node = m_Graph.GetNode(index);
			if (!node || !node.GetBase()) continue;
			SCR_CampaignMilitaryBaseComponent base = node.GetBase();
			if (base.GetFaction() != order.m_Faction) continue;
			array<SCR_ServicePointComponent> services = {};
			base.GetServices(services);
			foreach (SCR_ServicePointComponent service : services)
			{
				if (!service || !service.GetOwner() || service.GetType() != SCR_EServicePointType.ARMORY ||
					order.m_aRejected.Contains(service.GetOwner().GetID())) continue;
				m_aArsenals.Clear();
				GetGame().GetWorld().QueryEntitiesBySphere(service.GetOwner().GetOrigin(), 30, AddArsenal, null, EQueryEntitiesFlags.ALL);
				foreach (SCR_ArsenalComponent arsenal : m_aArsenals)
				{
					AICF_EquipmentOrder candidate = new AICF_EquipmentOrder();
					candidate.m_Faction = order.m_Faction;
					candidate.m_Base = base;
					candidate.m_BaseId = base.GetOwner().GetID();
					candidate.m_Service = service;
					candidate.m_ServiceId = service.GetOwner().GetID();
					candidate.m_Arsenal = arsenal;
					candidate.m_ArsenalId = arsenal.GetOwner().GetID();
					candidate.m_vPosition = arsenal.GetOwner().GetOrigin();
					bool canIssue;
					foreach (AICF_EquipmentMember member : order.m_aMembers)
						if (member.m_bPending && candidate.CanIssue(member)) canIssue = true;
					float distance = vector.DistanceSq(leader.GetOrigin(), candidate.m_vPosition);
					if (!canIssue || !candidate.OwnsArsenal(arsenal) || distance > AICF_EquipmentConfig.MAX_SERVICE_METERS * AICF_EquipmentConfig.MAX_SERVICE_METERS ||
						!m_Planner.CanEquipmentRoute(order.m_Group, candidate.m_vPosition, 10)) continue;
					if (!best || AICF_EquipmentConfig.Prefer(distance, candidate.m_ArsenalId, bestDistance, best.m_ArsenalId))
					{
						best = candidate;
						bestDistance = distance;
					}
				}
			}
		}
		m_aArsenals.Clear();
		if (!best) return false;
		order.m_Corpse = null;
		order.m_Base = best.m_Base;
		order.m_BaseId = best.m_BaseId;
		order.m_Service = best.m_Service;
		order.m_ServiceId = best.m_ServiceId;
		order.m_Arsenal = best.m_Arsenal;
		order.m_ArsenalId = best.m_ArsenalId;
		order.m_vPosition = best.m_vPosition;
		order.m_iDeadline = System.GetTickCount() + AICF_EquipmentConfig.VISIT_TIMEOUT_MS;
		if (!m_Planner.BeginEquipmentOrder(order)) return false;
		order.Log("EQUIPMENT_SOURCE", "ARMORY");
		return true;
	}

	protected bool Evaluate(AICF_EquipmentSlot record)
	{
		bool pending;
		foreach (AICF_EquipmentMember member : record.m_aMembers)
			if (member.Evaluate(m_Config)) pending = true;
		return pending;
	}

	protected void Finish(AICF_EquipmentSlot record, string reason, bool restore = true)
	{
		if (record.m_Order)
		{
			foreach (AICF_EquipmentMember member : record.m_Order.m_aMembers)
			{
				if (!member.m_Transaction) continue;
				m_Applicator.Cancel(member);
				if (!m_aRecoveries.Contains(member)) m_aRecoveries.Insert(member);
			}
			record.m_Order.Log("EQUIPMENT_FINISHED", reason);
			m_Planner.EndEquipmentOrder(record.m_Order, m_Graph, m_Selector, restore);
			record.m_Order = null;
		}
		record.m_iRetryAt = System.GetTickCount() + AICF_EquipmentConfig.RETRY_MS;
	}

	protected void Tick(AICF_EquipmentSlot record, int now, bool graphReady)
	{
		if (!AICF_EquipmentInventory.Quiet(record.m_Group))
		{
			record.m_iQuietUntil = now + AICF_EquipmentConfig.QUIET_MS;
			if (record.m_Order) Finish(record, "COMBAT_OR_MEMBER_UNAVAILABLE");
			return;
		}
		if (record.m_Order && (!record.m_Order.Current() || !graphReady || record.m_Order.m_iGraphRevision != m_Graph.GetRevision() ||
			(m_Vehicles && m_Vehicles.GetSlotView(record.m_Slot))))
		{
			Finish(record, "IDENTITY_OR_ORDER_CHANGED");
			return;
		}
		if (now < record.m_iQuietUntil) return;
		if (!record.m_Order)
		{
			if (!graphReady || now < record.m_iEvaluateAt || now < record.m_iRetryAt || record.m_aMembers.IsEmpty()) return;
			record.m_iEvaluateAt = now + AICF_EquipmentConfig.EVALUATE_MS;
			if (!Evaluate(record) || !m_Planner.CanEquipmentService(record.m_Slot, record.m_aMembers[0].m_Faction) ||
				(m_Vehicles && m_Vehicles.GetSlotView(record.m_Slot))) return;
			AICF_EquipmentOrder order = new AICF_EquipmentOrder();
			order.m_Slot = record.m_Slot;
			order.m_Group = record.m_Group;
			order.m_GroupId = record.m_GroupId;
			order.m_iGeneration = record.m_iGeneration;
			order.m_Faction = record.m_aMembers[0].m_Faction;
			order.m_iIntent = order.m_Slot.GetStrategicIntentRevision();
			order.m_iAssignment = order.m_Slot.GetStrategicAssignmentRevision();
			order.m_Waypoint = order.m_Slot.GetWaypoint();
			order.m_iGraphRevision = m_Graph.GetRevision();
			order.m_iToken = ++m_iNextToken;
			foreach (AICF_EquipmentMember enrolled : record.m_aMembers) order.m_aMembers.Insert(enrolled);
			foreach (AICF_EquipmentMember demand : order.m_aMembers)
				if (demand.m_bPending) order.Log("EQUIPMENT_NEED", demand.m_sNeedReason, demand);
			record.m_Order = order;
			if (!SelectCorpse(order) && !SelectService(order))
			{
				// До первого Begin запись ещё не владеет исходным waypoint.
				order.m_Waypoint = null;
				Finish(record, "NO_SOURCE");
			}
			return;
		}
		AICF_EquipmentOrder active = record.m_Order;
		if (!Evaluate(record))
		{
			Finish(record, "SATISFIED");
			return;
		}
		if (!active.m_Service)
		{
			if (!AICF_EquipmentInventory.Corpse(active.m_Corpse) || active.m_Corpse.GetID() != active.m_CorpseId || now >= active.m_iDeadline ||
				!HasUsefulItem(active, active.m_Corpse))
			{
				active.m_aRejected.Insert(active.m_CorpseId);
				active.m_Corpse = null;
				if (!SelectService(active)) Finish(record, "CORPSE_UNAVAILABLE_NO_SERVICE");
				return;
			}
			array<IEntity> items = {};
			AICF_EquipmentInventory.Manager(active.m_Corpse).GetItems(items);
			// Native GetItems order не задаёт tie-break: сортируем identity перед изъятием.
			for (int i = 1; i < items.Count(); i++)
			{
				IEntity candidateItem = items[i];
				int j = i - 1;
				while (j >= 0 && items[j].GetID().ToString().Compare(candidateItem.GetID().ToString()) > 0)
				{
					items[j + 1] = items[j];
					j--;
				}
				items[j + 1] = candidateItem;
			}
			foreach (AICF_EquipmentMember recipient : active.m_aMembers)
			{
				if (!recipient.m_bPending) continue;
				foreach (IEntity item : items)
				{
					if (!recipient.Wants(item)) continue;
					string reason;
					if (AICF_EquipmentInventory.Transfer(active, recipient, active.m_Corpse, active.m_CorpseId, item, item.GetID(), reason))
					{
						active.Log("EQUIPMENT_TRANSFERRED", "CORPSE", recipient, item);
						Evaluate(record);
						return;
					}
				}
			}
			return;
		}
		if (!active.HasService() || now >= active.m_iDeadline)
		{
			active.m_aRejected.Insert(active.m_ServiceId);
			if (!SelectService(active)) Finish(record, "SERVICE_UNAVAILABLE");
			return;
		}
		foreach (AICF_EquipmentMember pending : active.m_aMembers)
		{
			if (!pending.m_bPending || !pending.Current() || !active.CanIssue(pending)) continue;
			string result;
			if (m_Applicator.Apply(active, pending, result))
			{
				Evaluate(record);
				return;
			}
			if (result.StartsWith("ISSUE_FAILED"))
			{
				Finish(record, result);
				return;
			}
			if (pending.m_Transaction) return;
		}
	}

	void Update(AICF_FactionState us, SCR_CampaignFaction usFaction, AICF_FactionState ussr,
		SCR_CampaignFaction ussrFaction, AICF_VehicleCoordinator vehicles, bool graphReady)
	{
		if (m_bStopped || !Replication.IsServer() || !m_Campaign || !m_Campaign.IsMaster() || !m_Campaign.IsRunning()) return;
		m_Vehicles = vehicles;
		for (int recovery = m_aRecoveries.Count() - 1; recovery >= 0; recovery--)
		{
			AICF_EquipmentMember restoring = m_aRecoveries[recovery];
			if (!m_Applicator.Recover(restoring)) continue;
			restoring.m_Slot.ClearEquipmentInventoryTransaction(restoring.m_Transaction);
			restoring.m_Transaction = null;
			m_aRecoveries.Remove(recovery);
		}
		int now = System.GetTickCount();
		foreach (AICF_EquipmentSlot unseen : m_aSlots) unseen.m_bSeen = false;
		Discover(us, usFaction, now);
		Discover(ussr, ussrFaction, now);
		for (int index = m_aSlots.Count() - 1; index >= 0; index--)
		{
			AICF_EquipmentSlot record = m_aSlots[index];
			if (!record.m_bSeen || !record.m_Group || record.m_Group.GetID() != record.m_GroupId)
			{
				Finish(record, "LIFECYCLE_CHANGED", false);
				m_aSlots.Remove(index);
				continue;
			}
			Tick(record, now, graphReady);
		}
		// Устаревший preset не удерживает свой preview world до конца матча.
		for (int planIndex = m_aPlans.Count() - 1; planIndex >= 0; planIndex--)
		{
			bool used;
			foreach (AICF_EquipmentSlot live : m_aSlots)
				foreach (AICF_EquipmentMember member : live.m_aMembers)
					if (member.m_Plan == m_aPlans[planIndex]) used = true;
			if (!used) m_aPlans.Remove(planIndex);
		}
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		foreach (AICF_EquipmentSlot record : m_aSlots) Finish(record, "STOP", false);
		m_aSlots.Clear();
		m_aPlans.Clear();
		m_aCorpses.Clear();
		m_aArsenals.Clear();
		m_Vehicles = null;
		foreach (AICF_EquipmentMember remaining : m_aRecoveries)
		{
			m_Applicator.Release(remaining);
			remaining.m_Slot.ClearEquipmentInventoryTransaction(remaining.m_Transaction);
			remaining.m_Transaction = null;
		}
		m_aRecoveries.Clear();
		m_Campaign = null;
	}
}
