// Снимок подтверждения принадлежит точному roster, а не только номеру slot.
class AICF_VehicleCrewPlan
{
	AICF_GroupSlot m_Slot;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	int m_iGeneration;
	int m_iAssignment;
	int m_iLoadout;
	int m_iVehicle;
	int m_iSize;
	int m_iExpires;
	string m_sPreset;
	ref array<EntityID> m_aMembers = {};
	ref array<int> m_aKept = {};
	ref array<int> m_aReturning = {};

	void AICF_VehicleCrewPlan(AICF_GroupSlot slot, AICF_VehiclePreset preset)
	{
		m_Slot = slot;
		m_Group = slot.GetGroup();
		m_GroupId = m_Group.GetID();
		m_iGeneration = slot.GetSpawnGeneration();
		m_iAssignment = slot.GetStrategicAssignmentRevision();
		m_iLoadout = slot.GetLoadoutRevision();
		m_iVehicle = slot.GetVehiclePresetRevision();
		m_iSize = slot.GetDesiredSize();
		m_iExpires = System.GetTickCount() + 60000;
		m_sPreset = preset.Encode();
		for (int index; index < AICF_Stage1Config.MAX_GROUP_SIZE; index++)
		{
			IEntity member = slot.GetRosterMember(index);
			EntityID id = EntityID.INVALID;
			if (member)
			{
				id = member.GetID();
				if (preset.m_aMembers.Contains(index)) m_aKept.Insert(index);
				else m_aReturning.Insert(index);
			}
			m_aMembers.Insert(id);
		}
	}

	bool Current(AICF_GroupSlot slot, string data)
	{
		if (!Replication.IsServer() || slot != m_Slot || data != m_sPreset || !m_Group ||
			m_Group.GetID() != m_GroupId || slot.GetGroup() != m_Group || !slot.IsCombatReady() ||
			slot.GetSpawnGeneration() != m_iGeneration || slot.GetStrategicAssignmentRevision() != m_iAssignment ||
			slot.GetLoadoutRevision() != m_iLoadout || slot.GetVehiclePresetRevision() != m_iVehicle ||
			slot.GetDesiredSize() != m_iSize || System.GetTickCount() > m_iExpires ||
			slot.IsRecruitingInfantry() || slot.IsResupplyingEquipment() || slot.HasEquipmentInventoryTransaction()) return false;
		int alive;
		for (int index; index < m_aMembers.Count(); index++)
		{
			IEntity member = slot.GetRosterMember(index);
			if (!member) { if (m_aMembers[index] != EntityID.INVALID) return false; continue; }
			if (member.GetID() != m_aMembers[index] || !AICF_EquipmentInventory.SafeMember(member, m_Group)) return false;
			alive++;
		}
		return !m_aKept.IsEmpty() && alive == AICF_GroupRuntime.CountAliveAgents(m_Group) &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group);
	}
}

class AICF_ReturningSoldier
{
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	IEntity m_Member;
	EntityID m_MemberId;
	SCR_CampaignFaction m_Faction;
	SCR_CampaignMilitaryBaseComponent m_Base;
	AIWaypoint m_Waypoint;
	int m_iLastOrder;

	bool Current()
	{
		return m_Group && m_Group.GetID() == m_GroupId && m_Group.GetFaction() == m_Faction &&
			m_Member && m_Member.GetID() == m_MemberId && m_Group.GetAgentsCount() == 1 &&
			AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group) &&
			AICF_EquipmentInventory.SafeMember(m_Member, m_Group);
	}
}

// Возврат лишних живых AI. Никаких respawn, economy или vehicle side effects.
class AICF_VehicleCrewReturn
{
	protected AICF_ObjectiveGraph m_Graph;
	protected AICF_OrderPlanner m_Planner;
	protected ref AICF_ManagedAILODPolicy m_LOD = new AICF_ManagedAILODPolicy();
	protected ref array<ref AICF_ReturningSoldier> m_aReturns = {};

	void AICF_VehicleCrewReturn(AICF_ObjectiveGraph graph, AICF_OrderPlanner planner)
	{
		m_Graph = graph;
		m_Planner = planner;
	}

	static bool SafeBase(SCR_CampaignMilitaryBaseComponent base, SCR_CampaignFaction faction)
	{
		if (!base || !base.GetOwner() || !base.IsInitialized() || base.GetFaction() != faction ||
			base.GetCaptureState() != SCR_EBaseCaptureState.NONE || base.IsBeingCaptured() || base.AreEnemiesPresent()) return false;
		SCR_SpawnPoint spawn = base.GetSpawnPoint();
		return spawn && spawn.IsSpawnPointEnabled() && spawn.IsSpawnPointActive() && spawn.GetFactionKey() == faction.GetFactionKey();
	}

	protected SCR_CampaignMilitaryBaseComponent Nearest(vector position, SCR_CampaignFaction faction)
	{
		SCR_CampaignMilitaryBaseComponent best;
		float distance = float.MAX;
		for (int i; i < m_Graph.GetNodeCount(); i++)
		{
			SCR_CampaignMilitaryBaseComponent base = m_Graph.GetNode(i).GetBase();
			if (!SafeBase(base, faction)) continue;
			float next = vector.DistanceSqXZ(position, base.GetSpawnPoint().GetOrigin());
			if (next >= distance) continue;
			distance = next;
			best = base;
		}
		return best;
	}

	bool CanReturn(AICF_VehicleCrewPlan plan, SCR_CampaignFaction faction, out string reason)
	{
		reason = "CREW_RETURN_UNAVAILABLE";
		if (!plan || !plan.Current(plan.m_Slot, plan.m_sPreset)) return false;
		foreach (int index : plan.m_aReturning)
			if (!Nearest(plan.m_Slot.GetRosterMember(index).GetOrigin(), faction)) return false;
		reason = "READY";
		return true;
	}

	protected SCR_AIGroup EmptyGroup(SCR_CampaignFaction faction, vector position)
	{
		ResourceName prefab = faction.GetDefendersGroupPrefab();
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid()) return null;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		SCR_AIGroup.IgnoreSpawning(true);
		IEntity entity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		SCR_AIGroup.IgnoreSpawning(false);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (group && group.GetFaction() != faction && AICF_ContentProfile.GetActive().AllowsGroupFactionRebinding()) group.SetFaction(faction);
		if (!group || group.GetFaction() != faction || group.GetAgentsCount() != 0 || !group.m_aUnitPrefabSlots ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group))
		{
			if (entity) RplComponent.DeleteRplEntity(entity, false);
			return null;
		}
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		group.SetSpawnImmediately(false);
		group.m_aUnitPrefabSlots.Clear();
		return group;
	}

	bool Commit(AICF_VehicleCrewPlan plan, SCR_CampaignFaction faction, AICF_VehiclePreset preset, out string reason)
	{
		if (!CanReturn(plan, faction, reason)) return false;
		array<ref AICF_ReturningSoldier> prepared = {};
		foreach (int index : plan.m_aReturning)
		{
			AICF_ReturningSoldier returning = new AICF_ReturningSoldier();
			returning.m_Member = plan.m_Slot.GetRosterMember(index);
			returning.m_MemberId = returning.m_Member.GetID();
			returning.m_Faction = faction;
			returning.m_Base = Nearest(returning.m_Member.GetOrigin(), faction);
			returning.m_Group = EmptyGroup(faction, returning.m_Member.GetOrigin());
			prepared.Insert(returning);
			if (!returning.m_Group) { Rollback(plan, prepared); reason = "CREW_RETURN_UNAVAILABLE"; return false; }
			returning.m_GroupId = returning.m_Group.GetID();
			if (!m_Planner.SetReturnWaypoint(returning)) { Rollback(plan, prepared); reason = "CREW_RETURN_UNAVAILABLE"; return false; }
		}
		if (!plan.Current(plan.m_Slot, plan.m_sPreset)) { Rollback(plan, prepared); reason = "REVISION_CONFLICT"; return false; }
		foreach (AICF_ReturningSoldier transfer : prepared)
		{
			AIControlComponent control = AIControlComponent.Cast(transfer.m_Member.FindComponent(AIControlComponent));
			AIAgent agent = control.GetControlAIAgent();
			plan.m_Group.RemoveAgent(agent);
			transfer.m_Group.AddAgent(agent);
			if (agent.GetParentGroup() != transfer.m_Group) { Rollback(plan, prepared); reason = "CREW_TRANSFER_FAILED"; return false; }
		}
		if (!plan.m_Slot.CommitVehicleCrew(plan.m_aKept, preset)) { Rollback(plan, prepared); reason = "CREW_TRANSFER_FAILED"; return false; }
		foreach (AICF_ReturningSoldier committed : prepared)
		{
			m_aReturns.Insert(committed);
			int managed, recovered;
			m_LOD.KeepCaptureEligible(committed.m_Group, managed, recovered);
		}
		AICF_Stage4Diagnostics.Info("VEHICLE_CREW_REDUCED", string.Format("slot=%1 kept=%2 returning=%3 generation=%4",
			plan.m_Slot.GetStableSlotKey(), plan.m_aKept.Count(), prepared.Count(), plan.m_Slot.GetSpawnGeneration()));
		reason = "WAITING_FOR_VEHICLE_LIFECYCLE";
		return true;
	}

	protected void Rollback(AICF_VehicleCrewPlan plan, array<ref AICF_ReturningSoldier> prepared)
	{
		foreach (AICF_ReturningSoldier returning : prepared)
		{
			if (!returning.m_Group) continue;
			AIControlComponent control = AIControlComponent.Cast(returning.m_Member.FindComponent(AIControlComponent));
			AIAgent agent = control.GetControlAIAgent();
			if (agent.GetParentGroup() == returning.m_Group && returning.Current())
			{
				returning.m_Group.RemoveAgent(agent);
				plan.m_Group.AddAgent(agent);
			}
			if (returning.m_Group.GetAgentsCount() == 0)
			{
				m_Planner.ClearReturnWaypoint(returning);
				RplComponent.DeleteRplEntity(returning.m_Group, false);
			}
			else m_aReturns.Insert(returning); // fail-closed: не теряем владельца оставшегося AI.
		}
	}

	void Update()
	{
		if (!Replication.IsServer()) return;
		for (int i = m_aReturns.Count() - 1; i >= 0; i--)
		{
			AICF_ReturningSoldier returning = m_aReturns[i];
			if (!returning.m_Group || returning.m_Group.GetID() != returning.m_GroupId)
			{
				m_aReturns.RemoveOrdered(i);
				continue;
			}
			if (returning.m_Group.GetAgentsCount() == 0 && AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(returning.m_Group))
			{
				m_Planner.ClearReturnWaypoint(returning);
				RplComponent.DeleteRplEntity(returning.m_Group, false);
				m_aReturns.RemoveOrdered(i);
				continue;
			}
			if (!returning.Current()) continue; // possession/transfer/death не дают права delete.
			if (!SafeBase(returning.m_Base, returning.m_Faction))
			{
				m_Planner.ClearReturnWaypoint(returning);
				returning.m_Base = Nearest(returning.m_Member.GetOrigin(), returning.m_Faction);
				returning.m_iLastOrder = 0;
			}
			if (!returning.m_Base) continue;
			vector destination = returning.m_Base.GetSpawnPoint().GetOrigin();
			int managed, recovered;
			m_LOD.KeepCaptureEligible(returning.m_Group, managed, recovered);
			if (vector.DistanceSqXZ(returning.m_Member.GetOrigin(), destination) <= 225 &&
				AICF_EquipmentInventory.Quiet(returning.m_Group))
			{
				// Непосредственно повторяем identity/occupancy/ownership перед удалением.
				if (!returning.Current() || !SafeBase(returning.m_Base, returning.m_Faction)) continue;
				m_Planner.ClearReturnWaypoint(returning);
				m_LOD.Release(returning.m_Group);
				returning.m_Group.DespawnMembers();
				RplComponent.DeleteRplEntity(returning.m_Group, false);
				AICF_Stage4Diagnostics.Info("VEHICLE_CREW_RETURNED", string.Format("member=%1 base=%2 reason=ARRIVED", returning.m_MemberId, destination));
				m_aReturns.RemoveOrdered(i);
				continue;
			}
			if (System.GetTickCount() - returning.m_iLastOrder >= 15000)
			{
				m_Planner.SetReturnWaypoint(returning);
				returning.m_iLastOrder = System.GetTickCount();
			}
		}
	}

	int CountAgents()
	{
		int count;
		foreach (AICF_ReturningSoldier returning : m_aReturns)
			if (returning.m_Group && returning.m_Group.GetID() == returning.m_GroupId) count += AICF_GroupRuntime.CountAliveAgents(returning.m_Group);
		return count;
	}

	void Stop()
	{
		foreach (AICF_ReturningSoldier returning : m_aReturns)
		{
			m_Planner.ClearReturnWaypoint(returning);
			if (returning.Current()) m_LOD.Release(returning.m_Group);
		}
		m_aReturns.Clear();
	}
}
