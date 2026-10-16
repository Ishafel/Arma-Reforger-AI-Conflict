// Узкий adapter ambientSystem SCR_AIGroup lifecycle. Не владеет техникой/waypoints.
class AICF_FIAProximityLifecycle
{
	protected SCR_AIGroup m_Group;
	protected EntityID m_GroupId;
	protected Vehicle m_Vehicle;
	protected EntityID m_VehicleId;
	protected AICF_FIAGarrison m_Garrison;
	protected AICF_FIAPatrol m_Patrol;
	protected int m_iGeneration;
	protected bool m_bStopped;
	protected bool m_bDespawning;
	protected bool m_bReactivating;
	protected string m_sHold;
	protected ref array<ref AICF_FIALifecycleMember> m_aMembers = {};

	static bool CanAcquire(vector position)
	{
		if (!Replication.IsServer()) return false;
		SCR_AmbientPatrolSystem ambientSystem = SCR_AmbientPatrolSystem.GetInstance();
		AIWorld ai = GetGame().GetAIWorld();
		ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
		if (!ambientSystem || !ai || !observers || !ai.CanActivateGroup(null)) return false;
		float range = ambientSystem.GetDespawnDistance();
		return observers.HasObserverWithinRangeSq(position[0], position[2], range * range);
	}

	void Start(SCR_AIGroup group, AICF_FIAGarrison g, AICF_FIAPatrol p, bool empty = false)
	{
		if (!Replication.IsServer() || !group || (!g && !p) || m_Group || m_bStopped) return;
		m_Group = group;
		m_GroupId = group.GetID();
		m_Garrison = g;
		m_Patrol = p;
		if (g) m_iGeneration = g.m_iGeneration;
		else m_iGeneration = p.m_iGeneration;
		if (g) { m_Vehicle = g.m_Vehicle; m_VehicleId = g.m_VehicleId; }
		else { m_Vehicle = p.m_Vehicle; m_VehicleId = p.m_VehicleId; }
		if (!empty)
		{
			foreach (int index, ResourceName prefab : group.m_aUnitPrefabSlots)
			{
				AICF_FIALifecycleMember member = new AICF_FIALifecycleMember();
				member.m_iSeat = index;
				member.m_sPrefab = prefab;
				m_aMembers.Insert(member);
			}
		}
		group.SetDeleteWhenEmpty(false);
		group.AICF_SetFIALifecycle(this);
		SCR_AmbientPatrolSystem ambientSystem = SCR_AmbientPatrolSystem.GetInstance();
		if (!ambientSystem) { Stop(); return; }
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.ProximityDriven, ambientSystem.GetSpawnDistance(), ambientSystem.GetDespawnDistance());
	}

	bool Current()
	{
		if (!Replication.IsServer() || m_bStopped || !m_Group || m_Group.GetID() != m_GroupId) return false;
		if (!m_Vehicle || m_Vehicle.GetID() != m_VehicleId ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle)) return false;
		RplComponent rpl = RplComponent.Cast(m_Group.FindComponent(RplComponent));
		if (!rpl || !rpl.IsMaster()) return false;
		if (m_Garrison)
			return m_Garrison.m_iGeneration == m_iGeneration && m_Garrison.m_Vehicle == m_Vehicle && m_Garrison.m_VehicleId == m_VehicleId && m_Group.GetFaction() == m_Garrison.m_Faction &&
				(m_Garrison.m_Group == m_Group || m_Garrison.m_DesantGroup == m_Group);
		return m_Patrol && m_Patrol.m_iGeneration == m_iGeneration && m_Patrol.m_Vehicle == m_Vehicle && m_Patrol.m_VehicleId == m_VehicleId && m_Patrol.m_Group == m_Group && m_Group.GetFaction() == m_Patrol.m_Faction;
	}

	int LivingCapacity()
	{
		int count;
		foreach (AICF_FIALifecycleMember member : m_aMembers) { if (!member.m_bLost) count++; }
		return count;
	}

	bool IsReactivating() { return m_bReactivating; }
	void Boarded() { m_bReactivating = false; }

	bool Ready()
	{
		if (!Current() || m_Group.GetAgentsCount() != LivingCapacity() || LivingCapacity() == 0) return false;
		foreach (AICF_FIALifecycleMember member : m_aMembers)
		{
			if (member.m_bLost) continue;
			if (!member.m_Entity || member.m_Entity.GetID() != member.m_Id || member.m_bRestoring ||
				!AICF_GroupRuntime.IsAliveCharacter(member.m_Entity) || !AICF_ContentProfile.GetActive().IsFIAEquipmentReady(member.m_Entity)) return false;
		}
		return true;
	}

	// Перед ambientSystem proximity readback якорь следует за живым составом/техникой.
	void Update()
	{
		if (!Current()) return;
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_Group);
		if (leader) m_Group.SetOrigin(leader.GetOrigin());
		foreach (AICF_FIALifecycleMember member : m_aMembers)
		{
			if (!Current()) return;
			if (member.m_bLost || !member.m_Entity) continue;
			if (!AICF_GroupRuntime.IsAliveCharacter(member.m_Entity)) { member.m_bLost = true; continue; }
			bool restored = member.Restore();
			if (!Current()) return;
			if (restored && m_Garrison) m_Garrison.EnsureMemberIntegrity(member.m_iSeat, member.m_Entity);
		}
	}

	bool PrepareExpansion()
	{
		if (!Current()) return false;
		// Новые entries не переживают retirement/ownership до первого появления.
		if (m_Garrison && (m_Garrison.m_bRetired || (!m_Garrison.m_bReady &&
			(!m_Garrison.BaseIdentity() || m_Garrison.m_Base.GetFaction() != m_Garrison.m_Faction)))) return false;
		if (m_Patrol && (m_Patrol.m_bRetired || (!m_Patrol.m_bReady &&
			(!m_Patrol.m_Current || !m_Patrol.m_Current.GetOwner() || m_Patrol.m_Current.GetFaction() != m_Patrol.m_Faction)))) return false;
		if (LivingCapacity() <= m_Group.GetAgentsCount()) return false;
		m_Group.m_aUnitPrefabSlots.Clear();
		// Native выбирает следующий prefab по active count. Сначала уже живые,
		// затем ещё не материализованные logical seats, без погибших и дубликатов.
		foreach (AICF_FIALifecycleMember member : m_aMembers)
			if (!member.m_bLost && member.m_Entity) m_Group.m_aUnitPrefabSlots.Insert(member.m_sPrefab);
		foreach (AICF_FIALifecycleMember member : m_aMembers)
			if (!member.m_bLost && !member.m_Entity) m_Group.m_aUnitPrefabSlots.Insert(member.m_sPrefab);
		m_Group.SetDormantCounts(LivingCapacity(), 0);
		return true;
	}

	void Added(AIAgent agent)
	{
		if (!Current() || !agent) return;
		ChimeraCharacter entity = ChimeraCharacter.Cast(agent.GetControlledEntity());
		if (!entity || agent.GetParentGroup() != m_Group || !AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(entity)) return;
		foreach (AICF_FIALifecycleMember member : m_aMembers)
		{
			if (member.m_Entity == entity) return;
		}
		foreach (AICF_FIALifecycleMember member : m_aMembers)
		{
			if (member.m_bLost || member.m_Entity) continue;
			if (SCR_ResourceNameUtils.GetPrefabName(entity) != member.m_sPrefab) continue;
			member.m_Entity = entity;
			member.m_Id = entity.GetID();
			member.m_Agent = agent;
			member.m_bRestoring = !member.m_sInventory.IsEmpty();
			member.m_bRestoreIssued = false;
			if (m_Garrison)
			{
				m_Garrison.m_aCrew[member.m_iSeat] = entity;
				m_Garrison.m_aCrewIds[member.m_iSeat] = member.m_Id;
			}
			else if (member.m_iSeat == 0) { m_Patrol.m_Driver = entity; m_Patrol.m_DriverId = member.m_Id; }
			else { m_Patrol.m_Gunner = entity; m_Patrol.m_GunnerId = member.m_Id; }
			return;
		}
	}

	void Removed(AIAgent agent)
	{
		if (!Current() || m_bDespawning || !agent) return;
		foreach (AICF_FIALifecycleMember member : m_aMembers)
		{
			if (member.m_Agent != agent) continue;
			// Любая неплановая потеря ownership закрывает место навсегда.
			member.m_bLost = true;
			if (LivingCapacity() == 0) m_Group.SetDormantCounts(0, m_aMembers.Count());
			return;
		}
	}

	bool PrepareDormancy()
	{
		if (!Current()) return false;
		Vehicle vehicle;
		EntityID vehicleId;
		if (m_Garrison) { vehicle = m_Garrison.m_Vehicle; vehicleId = m_Garrison.m_VehicleId; }
		else { vehicle = m_Patrol.m_Vehicle; vehicleId = m_Patrol.m_VehicleId; }
		array<ChimeraCharacter> known = {};
		array<EntityID> knownIds = {};
		SCR_AIGroup companion;
		if (m_Garrison)
		{
			known.InsertAll(m_Garrison.m_aCrew);
			knownIds.InsertAll(m_Garrison.m_aCrewIds);
			companion = m_Garrison.m_DesantGroup;
			if (m_Group == companion) companion = m_Garrison.m_Group;
		}
		else
		{
			known = {m_Patrol.m_Driver, m_Patrol.m_Gunner};
			knownIds = {m_Patrol.m_DriverId, m_Patrol.m_GunnerId};
		}
		string reason;
		if (!AICF_VehicleCleanupManager.CanDormantFIA(vehicle, vehicleId, m_Group, known, knownIds, companion, reason)) return Hold(reason);
		int bound;
		foreach (AICF_FIALifecycleMember member : m_aMembers)
		{
			if (member.m_bLost || !member.m_Entity) continue;
			if (member.m_bRestoring) return Hold("RESTORING");
			if (!member.Capture()) return Hold("SNAPSHOT");
			bound++;
		}
		if (bound != m_Group.GetAgentsCount()) return Hold("UNKNOWN_MEMBER");
		// Native destructive loop начинается только после повторного identity/occupancy.
		if (!Current() || !AICF_VehicleCleanupManager.CanDormantFIA(vehicle, vehicleId, m_Group, known, knownIds, companion, reason)) return false;
		if (m_Garrison)
		{
			foreach (AICF_FIALifecycleMember member : m_aMembers)
				if (!member.m_bLost) m_Garrison.SuspendMemberIntegrity(member.m_iSeat);
		}
		m_bDespawning = true;
		m_sHold = string.Empty;
		return true;
	}

	protected bool Hold(string reason)
	{
		if (m_sHold != reason)
		{
			m_sHold = reason;
			AICF_Stage3Diagnostics.Info("FIA_DORMANCY_HELD", string.Format("group=%1 generation=%2 reason=%3", m_GroupId, m_iGeneration, reason));
		}
		return false;
	}

	void Dormant()
	{
		if (!Current() || !m_bDespawning) return;
		m_bDespawning = false;
		m_bReactivating = true;
		foreach (AICF_FIALifecycleMember member : m_aMembers)
		{
			if (member.m_bLost) continue;
			member.m_Entity = null;
			member.m_Agent = null;
			if (m_Garrison) m_Garrison.m_aCrew[member.m_iSeat] = null;
		}
		m_Group.SetDormantCounts(LivingCapacity(), 0);
		AICF_Stage3Diagnostics.Info("FIA_DORMANT", string.Format("group=%1 generation=%2 alive=%3 vehicle=%4", m_GroupId, m_iGeneration, LivingCapacity(), VehicleIdentity()));
	}

	protected EntityID VehicleIdentity()
	{
		if (m_Garrison) return m_Garrison.m_VehicleId;
		return m_Patrol.m_VehicleId;
	}

	// Передача десанта меняет group ownership, сохраняя logical seat и snapshot.
	void Transfer(ChimeraCharacter entity, AICF_FIAProximityLifecycle target)
	{
		if (!Current() || !target || !target.Current()) return;
		foreach (AICF_FIALifecycleMember member : m_aMembers)
		{
			if (member.m_Entity != entity || entity.GetID() != member.m_Id) continue;
			m_aMembers.RemoveItem(member);
			target.m_aMembers.Insert(member);
			member.m_bLost = false;
			return;
		}
	}

	void Stop()
	{
		if (m_bStopped) return;
		m_bStopped = true;
		if (m_Group && m_Group.GetID() == m_GroupId)
		{
			m_Group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
			ChimeraAIWorld world = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
			if (world) world.PurgeSpawnRequestsForGroup(m_Group);
			// Adapter остаётся закрытым fence для уже выданного ambientSystem callback.
		}
		m_aMembers.Clear();
		m_Garrison = null;
		m_Patrol = null;
	}
}

modded class SCR_AIGroup
{
	protected ref AICF_FIAProximityLifecycle m_AICFFIALifecycle;
	void AICF_SetFIALifecycle(AICF_FIAProximityLifecycle lifecycle) { m_AICFFIALifecycle = lifecycle; }

	override protected void LifecycleTick()
	{
		if (m_AICFFIALifecycle)
		{
			if (!m_AICFFIALifecycle.Current()) return;
			m_AICFFIALifecycle.Update();
		}
		super.LifecycleTick();
	}

	override bool ExpandOneMember()
	{
		if (m_AICFFIALifecycle && !m_AICFFIALifecycle.PrepareExpansion()) return false;
		return super.ExpandOneMember();
	}

	override bool IsExpandComplete()
	{
		if (m_AICFFIALifecycle)
			return !m_AICFFIALifecycle.Current() || GetAgentsCount() >= m_AICFFIALifecycle.LivingCapacity();
		return super.IsExpandComplete();
	}

	override void SpawnMembers()
	{
		if (m_AICFFIALifecycle && !m_AICFFIALifecycle.PrepareExpansion()) return;
		super.SpawnMembers();
	}

	override void OnAgentAdded(AIAgent child)
	{
		if (m_AICFFIALifecycle) m_AICFFIALifecycle.Added(child);
		super.OnAgentAdded(child);
	}

	override void OnAgentRemoved(AIAgent child)
	{
		if (m_AICFFIALifecycle) m_AICFFIALifecycle.Removed(child);
		super.OnAgentRemoved(child);
	}

	override void DespawnMembers()
	{
		if (m_AICFFIALifecycle && !m_AICFFIALifecycle.PrepareDormancy()) return;
		super.DespawnMembers();
		if (m_AICFFIALifecycle) m_AICFFIALifecycle.Dormant();
	}
}
