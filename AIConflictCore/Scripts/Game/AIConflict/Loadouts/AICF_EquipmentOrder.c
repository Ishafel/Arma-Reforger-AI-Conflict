// Служба владеет записью; slot ссылается на неё до освобождения waypoint planner.
class AICF_EquipmentOrder
{
	AICF_GroupSlot m_Slot;
	SCR_CampaignFaction m_Faction;
	SCR_AIGroup m_Group;
	EntityID m_GroupId;
	int m_iGeneration;
	int m_iIntent;
	int m_iAssignment;
	int m_iGraphRevision;
	int m_iToken;
	int m_iDeadline;
	AIWaypoint m_Waypoint;
	vector m_vPosition;
	IEntity m_Corpse;
	EntityID m_CorpseId;
	SCR_CampaignMilitaryBaseComponent m_Base;
	EntityID m_BaseId;
	SCR_ServicePointComponent m_Service;
	EntityID m_ServiceId;
	SCR_ArsenalComponent m_Arsenal;
	EntityID m_ArsenalId;
	ref array<ref AICF_EquipmentMember> m_aMembers = {};
	ref array<EntityID> m_aRejected = {};
	protected ref array<ResourceName> m_aAvailable = {};

	bool Current()
	{
		return Replication.IsServer() && m_Slot && m_Slot.IsCombatReady() && m_Slot.GetGroup() == m_Group &&
			m_Slot.GetEquipmentOrder() == this &&
			m_Group && m_Group.GetID() == m_GroupId && m_Group.GetFaction() == m_Faction &&
			m_Slot.GetSpawnGeneration() == m_iGeneration && m_Slot.GetStrategicIntentRevision() == m_iIntent &&
			m_Slot.GetStrategicAssignmentRevision() == m_iAssignment && m_Slot.GetWaypoint() == m_Waypoint &&
			!m_Slot.IsRecruitingInfantry();
	}

	bool HasService()
	{
		if (!m_Base || !m_Base.GetOwner() || m_Base.GetOwner().GetID() != m_BaseId ||
			!m_Base.IsInitialized() || m_Base.GetFaction() != m_Faction || m_Base.IsBeingCaptured() ||
			m_Base.GetCaptureState() != SCR_EBaseCaptureState.NONE) return false;
		if (!m_Service || !m_Service.GetOwner() ||
			m_Service.GetOwner().GetID() != m_ServiceId || m_Service.GetFaction() != m_Faction ||
			m_Service.GetType() != SCR_EServicePointType.ARMORY || m_Service.GetServiceState() != SCR_EServicePointStatus.ONLINE) return false;
		if (!m_Arsenal || !m_Arsenal.GetOwner() || m_Arsenal.GetOwner().GetID() != m_ArsenalId ||
			!m_Arsenal.IsArsenalEnabled() || !m_Arsenal.IsArsenalEnabledByType() || m_Arsenal.GetAssignedFaction() != m_Faction ||
			!AICF_EquipmentConfig.InRange(m_Arsenal.GetOwner().GetOrigin(), m_Service.GetOwner().GetOrigin(), 30)) return false;
		array<SCR_ServicePointComponent> services = {};
		m_Base.GetServices(services);
		return services.Contains(m_Service) && OwnsArsenal(m_Arsenal);
	}

	bool CanIssue(AICF_EquipmentMember member)
	{
		if (!HasService() || !member || !member.Current() || !member.m_Plan) return false;
		m_aAvailable.Clear();
		// Штатный armory разделяет оружие и одежду по соседним ящикам.
		GetGame().GetWorld().QueryEntitiesBySphere(m_Service.GetOwner().GetOrigin(), 30, CollectAvailable, null, EQueryEntitiesFlags.ALL);
		foreach (ResourceName prefab : member.m_Plan.m_aRequired)
			if (!m_aAvailable.Contains(prefab)) return false;
		return true;
	}

	protected bool CollectAvailable(IEntity entity)
	{
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(entity.FindComponent(SCR_ArsenalComponent));
		if (!arsenal || !OwnsArsenal(arsenal) || arsenal.GetAssignedFaction() != m_Faction || !arsenal.IsArsenalEnabled() || !arsenal.IsArsenalEnabledByType()) return true;
		array<ResourceName> prefabs = {};
		if (arsenal.GetAvailablePrefabs(prefabs))
			foreach (ResourceName prefab : prefabs)
				if (!m_aAvailable.Contains(prefab)) m_aAvailable.Insert(prefab);
		return true;
	}

	bool OwnsArsenal(SCR_ArsenalComponent arsenal)
	{
		if (!m_Base || !arsenal || !arsenal.GetOwner()) return false;
		array<SCR_ServicePointComponent> services = {};
		m_Base.GetServices(services);
		foreach (SCR_ServicePointComponent service : services)
		{
			if (!service || !service.GetOwner() || service.GetFaction() != m_Faction || service.GetServiceState() != SCR_EServicePointStatus.ONLINE) continue;
			IEntity owner = arsenal.GetOwner();
			for (int depth; depth < 4 && owner; depth++)
			{
				if (owner == service.GetOwner()) return true;
				owner = owner.GetParent();
			}
		}
		return false;
	}

	void Log(string eventName, string reason, AICF_EquipmentMember member = null, IEntity item = null)
	{
		string details = string.Format("faction=%1 slot=%2 generation=%3 group=%4 token=%5 assignment=%6 graph=%7 reason=%8",
			m_Faction.GetFactionKey(), m_Slot.GetSlotId(), m_iGeneration, m_GroupId, m_iToken, m_iAssignment, m_iGraphRevision, reason);
		if (member) details += string.Format(" member=%1 index=%2 revision=%3 primary_before=%4 launcher_before=%5 primary_after=%6 launcher_after=%7",
			member.m_CharacterId, member.m_iIndex, member.m_iRevision, member.m_iPrimaryBefore, member.m_iLauncherBefore,
			AICF_EquipmentInventory.Rounds(member.m_Character, AICF_EquipmentInventory.Weapon(member.m_Character, false)),
			AICF_EquipmentInventory.Rounds(member.m_Character, AICF_EquipmentInventory.Weapon(member.m_Character, true)));
		EntityID itemId;
		if (item) itemId = item.GetID();
		details += string.Format(" corpse=%1 service=%2 arsenal=%3 item=%4 supply_cost=0 ticket_cost=0", m_CorpseId, m_ServiceId, m_ArsenalId, itemId);
		AICF_Stage4Diagnostics.Info(eventName, details);
	}
}
