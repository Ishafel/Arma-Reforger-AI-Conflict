// Immutable native identities; optional Rpl is only for native controller,
// game-mode and label owners whose authority is established by their owner API.
class AICF_AIQuotaCatalogIdentity
{
	IEntity m_Entity;
	EntityID m_Id;
	BaseWorld m_World;
	RplComponent m_Rpl;
	string m_sRpl;
	bool m_bMaster;

	static AICF_AIQuotaCatalogIdentity Capture(IEntity entity, bool requireMaster)
	{
		if (!Replication.IsServer() || !GetGame() || !entity || entity.GetWorld() != GetGame().GetWorld()) return null;
		AICF_AIQuotaCatalogIdentity identity = new AICF_AIQuotaCatalogIdentity();
		identity.m_Entity = entity;
		identity.m_Id = entity.GetID();
		identity.m_World = entity.GetWorld();
		identity.m_Rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		identity.m_bMaster = requireMaster;
		if (identity.m_Rpl) identity.m_sRpl = identity.m_Rpl.Id().ToString();
		if (!identity.IsCurrent()) return null;
		return identity;
	}

	bool IsCurrent()
	{
		if (!Replication.IsServer() || !GetGame() || !m_Entity || !m_World || GetGame().GetWorld() != m_World) return false;
		if (m_Entity.GetWorld() != m_World || m_Entity.GetID() != m_Id || m_World.FindEntityByID(m_Id) != m_Entity) return false;
		if (m_Entity.FindComponent(RplComponent) != m_Rpl) return false;
		if (m_bMaster && (!m_Rpl || !m_Rpl.IsMaster() || !m_Rpl.Id().IsValid())) return false;
		if (m_Rpl && m_Rpl.Id().ToString() != m_sRpl) return false;
		return true;
	}
}

// One full STOCK basket before native PerformSpawn, not a permission inherited
// by arbitrary nested SpawnEntity/QueueSpawn calls.
class AICF_AIQuotaCatalogParents
{
	protected static ref array<ref AICF_AIQuotaCatalogBasket> s_aParents = {};

	static bool HasParent(SCR_PlayerController controller, SCR_SpawnerRequestComponent request, int playerId, BaseWorld world)
	{
		if (!controller || !request || playerId <= 0 || !world) return true;
		if (s_aParents.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS) return true;
		foreach (AICF_AIQuotaCatalogBasket parent : s_aParents)
		{
			if (!parent || !parent.m_bParentInFlight || !parent.m_User) return true;
			if (parent.m_User.m_World != world) continue;
			// A cancelled or stale parent still owns its native counter window
			// until its frame returns. Partial owner matches are ambiguous.
			if (parent.m_Controller == controller || parent.m_RequestComponent == request || parent.m_iPlayerId == playerId) return true;
		}
		return false;
	}

	static bool Enter(AICF_AIQuotaCatalogBasket basket)
	{
		if (!basket || basket.m_bParentInFlight || !basket.m_User || s_aParents.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
		if (!basket.IsCurrent()) return false;
		if (HasParent(basket.m_Controller, basket.m_RequestComponent, basket.m_iPlayerId, basket.m_User.m_World)) return false;
		basket.m_bParentInFlight = true;
		s_aParents.Insert(basket);
		return true;
	}

	static void Leave(AICF_AIQuotaCatalogBasket basket)
	{
		if (!basket || !basket.m_bParentInFlight || !s_aParents.Contains(basket)) return;
		s_aParents.RemoveItem(basket);
		basket.m_bParentInFlight = false;
	}
}

class AICF_AIQuotaCatalogBasket
{
	AICF_AIWorldQuotaService m_Service;
	SCR_CatalogEntitySpawnerComponent m_Spawner;
	SCR_EntityCatalogEntry m_Entry;
	SCR_EntityCatalog m_Catalog;
	SCR_EntityCatalogSpawnerData m_Data;
	SCR_PlayerController m_Controller;
	SCR_SpawnerRequestComponent m_RequestComponent;
	SCR_PlayerControllerGroupComponent m_GroupController;
	ref AICF_AIQuotaCatalogIdentity m_PlayerGroup;
	ref AICF_AIQuotaCatalogIdentity m_PlayerSlave;
	Faction m_Faction;
	EEntityCatalogType m_Type;
	int m_iEpoch;
	int m_iSerial;
	int m_iPlayerId;
	int m_iQuoted;
	int m_iSupplyCost;
	ResourceName m_EntryPrefab;
	int m_iPending;
	bool m_bCancelled;
	bool m_bCounterAdded;
	bool m_bHeaderObserved;
	bool m_bRootFactoryCalled;
	bool m_bParentInFlight;
	IEntity m_Produced;
	ref AICF_AIQuotaReservation m_Reservation;
	ref AICF_AIQuotaCatalogIdentity m_Producer;
	ref AICF_AIQuotaCatalogIdentity m_User;
	ref AICF_AIQuotaCatalogIdentity m_PlayerController;
	ref AICF_AIQuotaCatalogIdentity m_SlotOwner;
	SCR_EntitySpawnerSlotComponent m_Slot;
	SCR_EntityLabelPointComponent m_Rally;
	ref AICF_AIQuotaCatalogIdentity m_RallyOwner;
	ref array<ResourceName> m_aRootPrefabs = {};
	ref array<ResourceName> m_aMemberPrefabs = {};
	ref array<ref AICF_AIQuotaCatalogRequest> m_aRequests = {};

	static AICF_AIQuotaCatalogBasket Capture(AICF_AIWorldQuotaService service,
		SCR_CatalogEntitySpawnerComponent spawner, SCR_EntityCatalogEntry entry, IEntity user)
	{
		string reason;
		return CaptureWithReason(service, spawner, entry, user, reason);
	}

	static AICF_AIQuotaCatalogBasket CaptureWithReason(AICF_AIWorldQuotaService service,
		SCR_CatalogEntitySpawnerComponent spawner, SCR_EntityCatalogEntry entry, IEntity user, out string reason)
	{
		reason = "INVALID_CONTEXT";
		if (!service || !service.IsOwnerCurrent() || !service.IsConfigured() || !spawner || !entry || !user) return null;
		SCR_EntityCatalog catalog = entry.GetCatalogParent();
		if (!catalog) return null;
		EEntityCatalogType type = catalog.GetCatalogType();
		if (type != EEntityCatalogType.CHARACTER && type != EEntityCatalogType.GROUP) return null;
		SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (!data || data.GetEntityCount() <= 0 || data.GetEntityCount() > AICF_AIQuotaMath.MAX_TOTAL_UNITS) return null;
		if (type == EEntityCatalogType.CHARACTER && data.GetEntityCount() != 1) return null;
		SCR_PlayerController controller = spawner.GetPlayerControllerFromEntity(user);
		if (!controller || controller.GetPlayerId() <= 0 || controller.GetControlledEntity() != user) return null;
		AICF_AIQuotaCatalogBasket basket = new AICF_AIQuotaCatalogBasket();
		basket.m_Service = service;
		basket.m_Spawner = spawner;
		basket.m_Entry = entry;
		basket.m_Catalog = catalog;
		basket.m_Data = data;
		basket.m_Type = type;
		basket.m_Controller = controller;
		basket.m_iPlayerId = controller.GetPlayerId();
		basket.m_RequestComponent = spawner.GetRequestComponentFromPlayerEntity(user);
		if (AICF_AIQuotaCatalogParents.HasParent(controller, basket.m_RequestComponent, basket.m_iPlayerId, GetGame().GetWorld()))
		{
			reason = "PLAYER_COUNTER_PARENT_IN_FLIGHT";
			return null;
		}
		basket.m_GroupController = SCR_PlayerControllerGroupComponent.Cast(controller.FindComponent(SCR_PlayerControllerGroupComponent));
		if (!basket.m_GroupController || !basket.m_GroupController.IsPlayerLeaderOwnGroup()) return null;
		SCR_AIGroup playerGroup = basket.m_GroupController.GetPlayersGroup();
		if (!playerGroup || !playerGroup.GetSlave()) return null;
		basket.m_PlayerGroup = AICF_AIQuotaCatalogIdentity.Capture(playerGroup, false);
		basket.m_PlayerSlave = AICF_AIQuotaCatalogIdentity.Capture(playerGroup.GetSlave(), false);
		basket.m_Faction = spawner.GetRequesterFaction(user);
		basket.m_iEpoch = service.GetEpoch();
		basket.m_iSerial = service.AllocateContextSerial();
		basket.m_iQuoted = data.GetEntityCount();
		basket.m_iSupplyCost = data.GetSupplyCost();
		basket.m_EntryPrefab = entry.GetPrefab();
		basket.m_iPending = basket.m_iQuoted;
		basket.m_Producer = AICF_AIQuotaCatalogIdentity.Capture(spawner.GetOwner(), true);
		basket.m_User = AICF_AIQuotaCatalogIdentity.Capture(user, true);
		basket.m_PlayerController = AICF_AIQuotaCatalogIdentity.Capture(controller, false);
		basket.m_aRootPrefabs.Insert(entry.GetPrefab());
		array<ResourceName> variants = {};
		data.GetVariantPrefabList(variants);
		foreach (ResourceName prefab : variants)
			if (!SCR_StringHelper.IsEmptyOrWhiteSpace(prefab) && !basket.m_aRootPrefabs.Contains(prefab)) basket.m_aRootPrefabs.Insert(prefab);
		if (!basket.IsCurrent()) return null;
		reason = "AI_LIMIT_REACHED";
		basket.m_Reservation = service.ReserveUnits(AICF_EAIQuotaBucket.STOCK, basket.m_iQuoted, basket.m_iSerial);
		if (!basket.m_Reservation) return null;
		if (!basket.IsCurrent())
		{
			reason = "CONTEXT_CHANGED_DURING_RESERVATION";
			service.CancelReservation(basket.m_Reservation, basket.m_iSerial);
			return null;
		}
		reason = "READY";
		AICF_Stage3Diagnostics.Info("AI_QUOTA_CATALOG_RESERVED", string.Format(
			"serial=%1 epoch=%2 quoted=%3 player_id=%4 prefab=%5 available=%6 native_called=0 cash_called=0",
			basket.m_iSerial, basket.m_iEpoch, basket.m_iQuoted, basket.m_iPlayerId, basket.m_EntryPrefab, basket.m_Reservation.m_iAvailable));
		return basket;
	}

	bool IsCounterOwnerCurrent()
	{
		if (!m_PlayerController || !m_PlayerController.IsCurrent() || !m_Controller || !GetGame()) return false;
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || players.GetPlayerController(m_iPlayerId) != m_Controller || m_Controller.GetPlayerId() != m_iPlayerId) return false;
		return m_RequestComponent && m_Controller.FindComponent(SCR_SpawnerRequestComponent) == m_RequestComponent;
	}

	bool IsCurrent()
	{
		// Identity only: unavailable quota/census must never authorize rollback
		// of an otherwise valid native body.
		if (m_bCancelled || !m_Service || !m_Service.IsOwnerCurrent() || m_Service.GetEpoch() != m_iEpoch) return false;
		if (m_iSerial <= 0 || !m_Producer || !m_Producer.IsCurrent() || !m_User || !m_User.IsCurrent() || !IsCounterOwnerCurrent()) return false;
		if (!m_Spawner || m_Spawner.GetOwner() != m_Producer.m_Entity || !SCR_CatalogEntitySpawnerComponent.INSTANCES.Contains(m_Spawner)) return false;
		if (!m_GroupController || m_Controller.FindComponent(SCR_PlayerControllerGroupComponent) != m_GroupController || !m_GroupController.IsPlayerLeaderOwnGroup()) return false;
		if (!m_PlayerGroup || !m_PlayerGroup.IsCurrent() || !m_PlayerSlave || !m_PlayerSlave.IsCurrent()) return false;
		if (m_GroupController.GetPlayersGroup() != m_PlayerGroup.m_Entity || SCR_AIGroup.Cast(m_PlayerGroup.m_Entity).GetSlave() != m_PlayerSlave.m_Entity) return false;
		if (!m_Entry || m_Entry.GetCatalogParent() != m_Catalog || !m_Data || m_Data.GetEntityCount() != m_iQuoted) return false;
		if (m_iSupplyCost < 0 || m_Data.GetSupplyCost() != m_iSupplyCost || m_Entry.GetPrefab() != m_EntryPrefab || m_EntryPrefab.IsEmpty()) return false;
		if (m_Entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData) != m_Data || !m_Catalog || m_Catalog.GetCatalogType() != m_Type) return false;
		if (m_Controller.GetControlledEntity() != m_User.m_Entity || GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(m_User.m_Entity) != m_iPlayerId) return false;
		if (!m_Faction || m_Spawner.GetFaction() != m_Faction || m_Spawner.GetRequesterFaction(m_User.m_Entity) != m_Faction) return false;
		return m_Spawner.AICF_QuotaCatalogContains(m_Entry);
	}

	bool BindSlot(SCR_EntitySpawnerSlotComponent slot)
	{
		if (!IsCurrent() || !slot || !m_Spawner.AICF_QuotaCatalogSlot(m_Entry, slot)) return false;
		if (m_Slot) return m_Slot == slot && IsSlotCurrent();
		// Child/near slot authority is the captured master producer's exact
		// native registry membership. A static slot need not own its own Rpl.
		m_SlotOwner = AICF_AIQuotaCatalogIdentity.Capture(slot.GetOwner(), false);
		if (!m_SlotOwner) return false;
		m_Slot = slot;
		m_Rally = slot.GetRallyPoint();
		if (m_Type == EEntityCatalogType.GROUP && !m_Rally) return false;
		if (m_Rally)
		{
			m_RallyOwner = AICF_AIQuotaCatalogIdentity.Capture(m_Rally.GetOwner(), false);
			if (!m_RallyOwner) return false;
		}
		return IsSlotCurrent();
	}

	bool IsSlotCurrent()
	{
		if (!IsCurrent() || !m_Slot || !m_SlotOwner || !m_SlotOwner.IsCurrent()) return false;
		if (m_SlotOwner.m_Rpl && (!m_SlotOwner.m_Rpl.IsMaster() || !m_SlotOwner.m_Rpl.Id().IsValid())) return false;
		if (m_Slot.GetOwner() != m_SlotOwner.m_Entity || !m_Spawner.AICF_QuotaCatalogSlot(m_Entry, m_Slot)) return false;
		if (m_Slot.GetRallyPoint() != m_Rally) return false;
		if (m_Rally && (!m_RallyOwner || !m_RallyOwner.IsCurrent() || m_Rally.GetOwner() != m_RallyOwner.m_Entity)) return false;
		return true;
	}

	void CancelRemaining(string reason)
	{
		if (m_bCancelled) return;
		int removed;
		foreach (AICF_AIQuotaCatalogRequest request : m_aRequests)
		{
			if (!request || request.m_bFinished || request.m_bDispatching || !request.m_Manager) continue;
			if (request.m_Manager.AICF_RemoveBoundCatalogRequest(request))
			{
				request.m_bFinished = true;
				removed++;
			}
		}
		m_bCancelled = true;
		// Only this basket's unspawned requests, never another order's counter.
		if (m_bCounterAdded && removed > 0 && removed <= m_iPending && IsCounterOwnerCurrent())
			m_RequestComponent.AddQueuedAI(-removed);
		m_iPending = Math.Max(0, m_iPending - removed);
		if (m_Service && m_Reservation) m_Service.CancelReservation(m_Reservation, m_iSerial);
		AICF_Stage3Diagnostics.Info("AI_QUOTA_CATALOG_CANCELLED", string.Format(
			"serial=%1 quoted=%2 removed_own_pending=%3 remaining=%4 reason=%5 identity_inference=0",
			m_iSerial, m_iQuoted, removed, m_iPending, reason));
	}

	void RequestFinished(AICF_AIQuotaCatalogRequest request, bool materialized)
	{
		if (!request || request.m_Basket != this || request.m_bFinished || m_iPending <= 0) return;
		request.m_bFinished = true;
		m_iPending--;
		if (!materialized && m_bCounterAdded && IsCounterOwnerCurrent()) m_RequestComponent.AddQueuedAI(-1);
		if (m_iPending == 0)
		{
			CancelRemaining("ALL_NATIVE_REQUESTS_FINISHED");
			if (m_Spawner) m_Spawner.AICF_ForgetCatalogBasket(this);
		}
	}
}

class AICF_AIQuotaCatalogRequest
{
	AICF_AIQuotaCatalogBasket m_Basket;
	SCR_SpawnerAIGroupManagerComponent m_Manager;
	SCR_SpawnerAIRequest m_Request;
	ref AICF_AIQuotaCatalogIdentity m_ManagerOwner;
	ref AICF_AIQuotaCatalogIdentity m_RallyOwner;
	SCR_EntityLabelPointComponent m_Rally;
	ResourceName m_Prefab;
	int m_iMemberIndex;
	bool m_bFinished;
	bool m_bDispatching;
	bool m_bFactoryCalled;
	bool m_bFactoryRejected;
	bool m_bMaterialized;
	ref AICF_AIQuotaBody m_Body;

	bool MatchesNativeRequest()
	{
		if (m_bFinished || !m_Basket || !m_Request || !m_Manager || !m_ManagerOwner || !m_ManagerOwner.IsCurrent()) return false;
		if (!m_Manager.AICF_QuotaManagerCurrent() || m_Manager.GetOwner() != m_ManagerOwner.m_Entity) return false;
		if (m_Request.m_AICFQuotaRequest != this || m_Request.m_Spawner != m_Basket.m_Spawner) return false;
		if (!m_Basket.m_User || !m_Basket.m_SlotOwner) return false;
		return m_Request.m_UserEntity == m_Basket.m_User.m_Entity && m_Request.m_SlotEntity == m_Basket.m_SlotOwner.m_Entity &&
			m_Request.m_ResourceName == m_Prefab && m_Request.m_RallyPoint == m_Rally;
	}

	bool MatchesRequest()
	{
		if (m_bFinished || !m_Basket || !m_Request || !m_Manager || !m_ManagerOwner || !m_ManagerOwner.IsCurrent()) return false;
		if (!m_Basket.IsSlotCurrent() || m_Manager.GetOwner() != m_ManagerOwner.m_Entity || !m_Manager.AICF_QuotaManagerCurrent()) return false;
		if (m_Request.m_AICFQuotaRequest != this || m_Request.m_Spawner != m_Basket.m_Spawner || m_Request.m_UserEntity != m_Basket.m_User.m_Entity) return false;
		if (m_Request.m_SlotEntity != m_Basket.m_SlotOwner.m_Entity || m_Request.m_ResourceName != m_Prefab || m_Request.m_RallyPoint != m_Rally) return false;
		if (!m_Basket.m_aMemberPrefabs.IsIndexValid(m_iMemberIndex) || m_Basket.m_aMemberPrefabs[m_iMemberIndex] != m_Prefab) return false;
		if (m_Rally && (!m_RallyOwner || !m_RallyOwner.IsCurrent() || m_Rally.GetOwner() != m_RallyOwner.m_Entity)) return false;
		return true;
	}
}

class AICF_AIQuotaCatalogFactoryGuard : AICF_AIQuotaCreationGuard
{
	AICF_AIQuotaCatalogBasket m_Basket;
	AICF_AIQuotaCatalogRequest m_Request;

	override bool IsCurrent()
	{
		if (!m_Basket || !m_Basket.IsSlotCurrent()) return false;
		if (!m_Request) return !m_Basket.m_bHeaderObserved;
		return m_Request.m_Basket == m_Basket && m_Request.m_bDispatching && m_Request.MatchesRequest() &&
			m_Request.m_Manager.AICF_QuotaIsHead(m_Request.m_Request);
	}

	override FactionKey GetFactionKey()
	{
		if (!m_Basket || !m_Basket.m_Faction) return string.Empty;
		return m_Basket.m_Faction.GetFactionKey();
	}
}
