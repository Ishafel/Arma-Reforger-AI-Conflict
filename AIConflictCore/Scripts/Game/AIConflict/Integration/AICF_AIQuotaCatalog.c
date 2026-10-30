// Active-policy methods are version-locked to Script Diff 1.8.0.13. The native
// factory receives an explicit local basket; no stack/field permission exists.
modded class SCR_CatalogEntitySpawnerComponent
{
	protected ref array<ref AICF_AIQuotaCatalogBasket> m_aAICFQuotaBaskets = {};
	protected bool m_bAICFCatalogStopping;

	bool AICF_QuotaCatalogContains(SCR_EntityCatalogEntry entry)
	{
		return !m_bAICFCatalogStopping && entry && m_aAssetList && m_aAssetList.Contains(entry);
	}

	void AICF_ForgetCatalogBasket(AICF_AIQuotaCatalogBasket basket)
	{
		if (basket && basket.m_bParentInFlight) return;
		m_aAICFQuotaBaskets.RemoveItem(basket);
	}

	bool AICF_QuotaCatalogSlot(SCR_EntityCatalogEntry entry, SCR_EntitySpawnerSlotComponent slot)
	{
		if (!AICF_QuotaCatalogContains(entry) || !slot || !slot.GetOwner()) return false;
		SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (!data || !data.CanSpawnInSlot(slot.GetSlotType())) return false;
		return (m_aChildSlots && m_aChildSlots.Contains(slot)) || (m_aNearSlots && m_aNearSlots.Contains(slot));
	}

	protected bool AICF_QuotaCharacterCatalog(SCR_EntityCatalogEntry entry)
	{
		if (!entry || !entry.GetCatalogParent()) return false;
		EEntityCatalogType type = entry.GetCatalogParent().GetCatalogType();
		return type == EEntityCatalogType.CHARACTER || type == EEntityCatalogType.GROUP;
	}

	override SCR_EEntityRequestStatus GetRequestState(notnull SCR_EntityCatalogEntry entityEntry, IEntity user = null)
	{
		SCR_EEntityRequestStatus nativeState = super.GetRequestState(entityEntry, user);
		if (nativeState != SCR_EEntityRequestStatus.CAN_SPAWN || !AICF_QuotaCharacterCatalog(entityEntry)) return nativeState;
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetOwner().GetWorld());
		// Clients retain native presentation. Only authority admits a purchase;
		// an absent client census is never interpreted as zero occupancy.
		if (!quota) return nativeState;
		SCR_PlayerController quotaController = GetPlayerControllerFromEntity(user);
		SCR_SpawnerRequestComponent quotaRequest = GetRequestComponentFromPlayerEntity(user);
		if (!quotaController || AICF_AIQuotaCatalogParents.HasParent(quotaController, quotaRequest, quotaController.GetPlayerId(), GetOwner().GetWorld()))
			return SCR_EEntityRequestStatus.NOT_AVAILABLE;
		SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entityEntry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (!data || data.GetEntityCount() <= 0) return SCR_EEntityRequestStatus.NOT_AVAILABLE;
		if (entityEntry.GetCatalogParent().GetCatalogType() == EEntityCatalogType.CHARACTER && data.GetEntityCount() != 1)
			return SCR_EEntityRequestStatus.NOT_AVAILABLE;
		if (!quota.CanReserve(AICF_EAIQuotaBucket.STOCK, data.GetEntityCount())) return SCR_EEntityRequestStatus.AI_LIMIT_REACHED;
		return nativeState;
	}

	override protected void PerformSpawn(notnull SCR_EntityCatalogEntry entityEntry, IEntity user = null, SCR_EntitySpawnerSlotComponent preferredSlot = null)
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetOwner().GetWorld());
		if (!quota || !AICF_QuotaCharacterCatalog(entityEntry))
		{
			super.PerformSpawn(entityEntry, user, preferredSlot);
			return;
		}
		if (!user) return;
		SCR_EEntityRequestStatus status = GetRequestState(entityEntry, user);
		if (status != SCR_EEntityRequestStatus.CAN_SPAWN)
		{
			SendNotification(status, user);
			return;
		}
		string quotaReason;
		AICF_AIQuotaCatalogBasket basket = AICF_AIQuotaCatalogBasket.CaptureWithReason(quota, this, entityEntry, user, quotaReason);
		if (!basket)
		{
			SCR_EEntityRequestStatus rejected = SCR_EEntityRequestStatus.NOT_AVAILABLE;
			if (quotaReason == "AI_LIMIT_REACHED") rejected = SCR_EEntityRequestStatus.AI_LIMIT_REACHED;
			AICF_Stage3Diagnostics.Info("AI_QUOTA_CATALOG_REJECTED", string.Format("reason=%1 native_called=0 cash_called=0", quotaReason));
			SendNotification(rejected, user);
			return;
		}
		if (!AICF_AIQuotaCatalogParents.Enter(basket))
		{
			basket.CancelRemaining("PLAYER_COUNTER_PARENT_IN_FLIGHT");
			return;
		}
		m_aAICFQuotaBaskets.Insert(basket);
		AICF_PerformBoundCatalog(basket, entityEntry, user, preferredSlot);
		AICF_AIQuotaCatalogParents.Leave(basket);
		if (!basket.IsCurrent() || basket.m_iPending == 0 || !basket.m_bCounterAdded)
		{
			basket.CancelRemaining("PARENT_FINISHED_OR_SCOPE_LOST");
			m_aAICFQuotaBaskets.RemoveItem(basket);
		}
	}

	protected bool AICF_QuotaParentCurrent(AICF_AIQuotaCatalogBasket basket)
	{
		return basket && basket.IsSlotCurrent() && basket.m_Produced && m_SpawnedEntity == basket.m_Produced;
	}

	// Native private SpawnEntity entry never borrows a pending basket.
	override protected IEntity SpawnEntity(ResourceName entityResourceName, notnull IEntity slotOwner)
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetOwner().GetWorld());
		if (!quota) return super.SpawnEntity(entityResourceName, slotOwner);
		// Empty human vehicles do not consume a character basket. Inspect the
		// authored type without creating a body; any later AI entry still goes
		// through the separate vehicle activation owner.
		int epoch = quota.GetEpoch();
		AICF_AIQuotaCatalogIdentity producer = AICF_AIQuotaCatalogIdentity.Capture(GetOwner(), true);
		AICF_AIQuotaCatalogIdentity slot = AICF_AIQuotaCatalogIdentity.Capture(slotOwner, false);
		if (!AICF_QuotaVehicleFactoryCurrent(quota, epoch, producer, slot)) return null;
		Resource resource = Resource.Load(entityResourceName);
		if (!resource || !resource.IsValid() || !resource.GetResource()) return null;
		IEntitySource source = resource.GetResource().ToEntitySource();
		if (source && source.GetClassName().ToType().IsInherited(Vehicle) && AICF_QuotaVehicleFactoryCurrent(quota, epoch, producer, slot))
		{
			AICF_Stage3Diagnostics.Info("AI_QUOTA_CATALOG_VEHICLE_FACTORY", "verified_prefab_vehicle=1 stock_character_basket=0 actual_crew=UNKNOWN native_factory_delegated=1");
			return super.SpawnEntity(entityResourceName, slotOwner);
		}
		AICF_Stage3Diagnostics.Warning("AI_QUOTA_CATALOG_UNBOUND_FACTORY", "native_called=0 ambient_permission=0");
		return null;
	}

	protected bool AICF_QuotaVehicleFactoryCurrent(AICF_AIWorldQuotaService quota, int epoch,
		AICF_AIQuotaCatalogIdentity producer, AICF_AIQuotaCatalogIdentity slot)
	{
		if (m_bAICFCatalogStopping || !quota || !quota.IsOwnerCurrent() || quota.GetEpoch() != epoch) return false;
		if (!producer || !producer.IsCurrent() || !slot || !slot.IsCurrent() || GetOwner() != producer.m_Entity) return false;
		if (!SCR_CatalogEntitySpawnerComponent.INSTANCES.Contains(this) ||
			AICF_AIWorldQuotaService.GetAuthorityForWorld(producer.m_World) != quota) return false;
		if (slot.m_Rpl && (!slot.m_Rpl.IsMaster() || !slot.m_Rpl.Id().IsValid())) return false;
		if (m_aChildSlots)
		{
			if (m_aChildSlots.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
			foreach (SCR_EntitySpawnerSlotComponent child : m_aChildSlots)
				if (child && child.GetOwner() == slot.m_Entity) return true;
		}
		if (m_aNearSlots)
		{
			if (m_aNearSlots.Count() > AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
			foreach (SCR_EntitySpawnerSlotComponent near : m_aNearSlots)
				if (near && near.GetOwner() == slot.m_Entity) return true;
		}
		return false;
	}

	protected IEntity AICF_QuotaCatalogFactory(AICF_AIQuotaCatalogBasket basket, ResourceName entityResourceName,
		SCR_EntitySpawnerSlotComponent slot, AICF_AIQuotaCatalogRequest request = null)
	{
		if (!basket || !basket.IsSlotCurrent() || basket.m_Spawner != this || slot != basket.m_Slot) return null;
		bool headerOnly = !request && basket.m_Type == EEntityCatalogType.GROUP;
		if (!request && (basket.m_bRootFactoryCalled || !basket.m_aRootPrefabs.Contains(entityResourceName))) return null;
		if (request && (!request.MatchesRequest() || !request.m_bDispatching || request.m_bFactoryCalled || request.m_Prefab != entityResourceName)) return null;
		ref AICF_AIQuotaCatalogFactoryGuard guard = new AICF_AIQuotaCatalogFactoryGuard();
		guard.m_Basket = basket;
		guard.m_Request = request;
		AICF_AIWorldQuotaService quota = basket.m_Service;
		ref AICF_AIQuotaCreation creation = quota.BeginExternalCreation(basket.m_Reservation, basket.m_iSerial,
			entityResourceName, basket.m_Producer.m_Entity, headerOnly, guard);
		if (!creation) return null;
		if (!guard.IsCurrent())
		{
			quota.CancelExternalCreationBeforeFactory(creation);
			return null;
		}
		if (!request) basket.m_bRootFactoryCalled = true;
		// The only native birth call. All admission is consumed before this call;
		// native reentry cannot inherit either this creation or another request.
		bool nativeCalled;
		IEntity entity = AICF_QuotaPinnedCatalogFactory(creation, entityResourceName, slot.GetOwner(), nativeCalled);
		if (request) request.m_bFactoryCalled = nativeCalled;
		if (!nativeCalled)
		{
			if (request) request.m_bFactoryRejected = true;
			quota.CancelExternalCreationBeforeFactory(creation);
			return null;
		}
		if (creation.m_bFinished)
		{
			// A native callback ended this exact creation. Never observe/reject
			// its receipt against a successor creation owned by the service.
			if (request) request.m_bFactoryRejected = true;
			AICF_Stage3Diagnostics.Warning("AI_QUOTA_CATALOG_CREATION_SCOPE_LOST", "native_called=1 retry_factory_allowed=0 successor_untouched=1");
			return null;
		}
		quota.ObserveExternalFactory(creation, entity);
		string bodyKey = "NONE";
		if (creation.m_Body) bodyKey = creation.m_Body.Key();
		AICF_Stage3Diagnostics.Info("AI_QUOTA_CATALOG_NATIVE_FACTORY", string.Format(
			"serial=%1 epoch=%2 header=%3 native_called=1 status=%4 body=%5 available=%6 native_active_before=%7 native_intended_before=%8",
			basket.m_iSerial, basket.m_iEpoch, headerOnly, creation.m_Status, bodyKey, basket.m_Reservation.m_iAvailable,
			creation.m_iNativeActiveBefore, creation.m_iNativeIntendedBefore));
		bool shape = headerOnly || SCR_ChimeraCharacter.Cast(entity);
		if (creation.m_Status == AICF_EAIQuotaBirthStatus.BOUND && guard.IsCurrent() && shape)
		{
			if (headerOnly)
			{
				SCR_AIGroup header = SCR_AIGroup.Cast(entity);
				basket.m_aMemberPrefabs.Copy(header.m_aUnitPrefabSlots);
			}
			if (!quota.EndExternalCreation(creation)) return null;
			if (request) request.m_Body = creation.m_Body;
			if (!request)
			{
				basket.m_Produced = entity;
				if (headerOnly) basket.m_bHeaderObserved = true;
			}
			return entity;
		}
		if (creation.m_Status == AICF_EAIQuotaBirthStatus.NATIVE_NULL)
		{
			quota.EndExternalCreation(creation);
			return null;
		}
		if (quota.CanRollbackExternalCreation(creation))
		{
			// Only the exact returned fresh body, with service repeated authority,
			// fingerprint and no-player/unassigned proof. No old entity adoption.
			if (quota.CanRollbackExternalCreation(creation)) SCR_EntityHelper.DeleteEntityAndChildren(creation.m_Entity);
			if (quota.ObserveExternalAbsence(creation)) return null;
		}
		quota.RejectExternalCreation(creation, "CATALOG_NATIVE_RESULT_OR_SCOPE_REJECTED");
		return null;
	}

	protected bool AICF_QuotaFactoryCurrent(AICF_AIQuotaCreation creation)
	{
		return creation && !creation.m_bFinished && creation.m_Status == AICF_EAIQuotaBirthStatus.IN_NATIVE_FACTORY &&
			creation.m_Service && creation.m_Service.IsCurrentWorld() && creation.IsProducerCurrent();
	}

	protected IEntity AICF_QuotaPinnedCatalogFactory(AICF_AIQuotaCreation creation, ResourceName entityResourceName,
		notnull IEntity slotOwner, out bool nativeCalled)
	{
		// AICF_CATALOG_BEGIN FACTORY_ENTRY
		nativeCalled = false;
		if (!AICF_QuotaFactoryCurrent(creation)) return null;
		// AICF_CATALOG_END FACTORY_ENTRY
		Resource entityResource = Resource.Load(entityResourceName);
		if (!entityResource || !entityResource.IsValid())
		{
			Print("CatalogEntitySpawnerComponent - SpawnEntity cannot spawn new entity without valid resource. Used SCR_EntityCatalogEntry is probably set incorrectly", LogLevel.ERROR);
			return null;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		// AICF_CATALOG_BEGIN FACTORY_SLOT
		if (!slotOwner || !AICF_QuotaFactoryCurrent(creation)) return null;
		// AICF_CATALOG_END FACTORY_SLOT
		slotOwner.GetTransform(params.Transform);
		// AICF_CATALOG_BEGIN FACTORY_CALL
		if (!AICF_QuotaFactoryCurrent(creation)) return null;
		if (!creation.m_Service.MarkExternalNativeCallStarted(creation)) return null;
		nativeCalled = true;
		// AICF_CATALOG_END FACTORY_CALL
		return GetGame().SpawnEntityPrefab(entityResource, GetOwner().GetWorld(), params);
	}

	protected void AICF_PerformBoundCatalog(AICF_AIQuotaCatalogBasket basket, notnull SCR_EntityCatalogEntry entityEntry,
		IEntity user = null, SCR_EntitySpawnerSlotComponent preferredSlot = null)
	{
		if (IsProxy())
			return;

		m_SpawnedEntity = null;

		SCR_EntityCatalogSpawnerData spawnerData = SCR_EntityCatalogSpawnerData.Cast(entityEntry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));

		//Slot from preferredSlot parameter is checked first, should it be occupied, new slot will be looked for (if there is any)
		SCR_EntitySpawnerSlotComponent slot;
		if (preferredSlot && spawnerData.CanSpawnInSlot(preferredSlot.GetSlotType()) && !preferredSlot.IsOccupied())
		{
			slot = preferredSlot;
		}
		else
		{
			ClearKnownSlots();
			slot = GetFreeSlot(spawnerData)
		}

		if (!slot)
		{
			SendNotification(SCR_EEntityRequestStatus.NOT_ENOUGH_SPACE, user);
			return;
		}

		// AICF_CATALOG_BEGIN SLOT
		if (!basket.BindSlot(slot)) return;
		// AICF_CATALOG_END SLOT
		//Prevents next spawned SCR_AIGroup from being fully spawned
		if (entityEntry.GetCatalogParent().GetCatalogType() == EEntityCatalogType.GROUP)
			SCR_AIGroup.IgnoreSpawning(true);

		//Spawns entity using Resource Name from prefab data and slot owning entity
		m_SpawnedEntity = AICF_QuotaCatalogFactory(basket, spawnerData.GetRandomDefaultOrVariantPrefab(), slot);

		if (!m_SpawnedEntity)
		{
			if (entityEntry.GetCatalogParent().GetCatalogType() == EEntityCatalogType.GROUP)
				SCR_AIGroup.IgnoreSpawning(false);

			return;
		}

		// AICF_CATALOG_BEGIN BEFORE_CASH
		if (!AICF_QuotaParentCurrent(basket)) return;
		// AICF_CATALOG_END BEFORE_CASH
		if (m_bSuppliesConsumptionEnabled)
			AddSpawnerSupplies(-spawnerData.GetSupplyCost());
		// AICF_CATALOG_BEGIN AFTER_CASH
		if (!AICF_QuotaParentCurrent(basket)) return;
		// AICF_CATALOG_END AFTER_CASH

		if (m_OnEntitySpawned)
			m_OnEntitySpawned.Invoke(m_SpawnedEntity, user, SCR_Faction.Cast(GetFaction()), this);
		// AICF_CATALOG_BEGIN AFTER_EVENT
		if (!AICF_QuotaParentCurrent(basket)) return;
		// AICF_CATALOG_END AFTER_EVENT

		//Send notification to player, whom requested entity
		SCR_EntityCatalog parentCatalog = entityEntry.GetCatalogParent();
		if (parentCatalog)
		{
			array<SCR_EntityCatalogEntry> entityList = {};
			parentCatalog.GetEntityList(entityList);
			int assetIndex = entityList.Find(entityEntry);

			if (assetIndex > -1)
				SendNotification(0, user, entityList.Find(entityEntry), parentCatalog.GetCatalogType());
		}

		//Called, if spawned entity is vehicle
		if (m_SpawnedEntity.IsInherited(Vehicle))
		{
			CarControllerComponent carController = CarControllerComponent.Cast(m_SpawnedEntity.FindComponent(CarControllerComponent));
			if (carController)
				carController.SetPersistentHandBrake(true);

			Physics physicsComponent = m_SpawnedEntity.GetPhysics();
			if (physicsComponent)
				physicsComponent.SetVelocity("0 -0.1 0"); // Make the entity copy the terrain properly

			LockSpawnedVehicle(user);

			ActionsManagerComponent actionsManagerComponent = ActionsManagerComponent.Cast(m_SpawnedEntity.FindComponent(ActionsManagerComponent));

			if (!actionsManagerComponent)
				return;

			SCR_ResourceEntityRefundAction refundAction;
			array<BaseUserAction> outActions = {};

			actionsManagerComponent.GetActionsList(outActions);

			foreach (BaseUserAction action : outActions)
			{
				refundAction = SCR_ResourceEntityRefundAction.Cast(action);

				if (!refundAction)
					continue;
			}
		}

		//Called, if spawned entity is SCR_ChimeraCharacter or inherits from it.
		if (m_SpawnedEntity.IsInherited(SCR_ChimeraCharacter))
		{
			SCR_SpawnerRequestComponent requestComponent = GetRequestComponentFromPlayerEntity(user);
			if (!requestComponent)
				return;

			OnChimeraCharacterSpawned(SCR_ChimeraCharacter.Cast(m_SpawnedEntity), user, slot.GetRallyPoint());
			// AICF_CATALOG_BEGIN CHARACTER_COUNTER
			if (!AICF_QuotaParentCurrent(basket)) return;
			basket.m_bCounterAdded = true;
			// AICF_CATALOG_END CHARACTER_COUNTER
			requestComponent.AddQueuedAI(spawnerData.GetEntityCount());
			// AICF_CATALOG_BEGIN CHARACTER_DONE
			basket.m_iPending = 0;
			// AICF_CATALOG_END CHARACTER_DONE
		}
		else
		{
			slot.MoveCharactersFromSlot();
		}

		//Called, if spawned entity is SCR_AIGroup or inherits from it.
		if (m_SpawnedEntity.IsInherited(SCR_AIGroup))
			AICF_OnBoundCatalogGroup(basket, SCR_AIGroup.Cast(m_SpawnedEntity), user, slot.GetOwner(), slot.GetRallyPoint());
	}

	protected void AICF_OnBoundCatalogGroup(AICF_AIQuotaCatalogBasket basket, notnull SCR_AIGroup group,
		notnull IEntity user, notnull IEntity slotEntity, SCR_EntityLabelPointComponent rallyPoint = null)
	{
		// AICF_CATALOG_BEGIN GROUP_SCOPE
		if (!AICF_QuotaParentCurrent(basket) || group != basket.m_Produced || !basket.m_bHeaderObserved) return;
		if (group.GetAgentsCount() != 0 || group.GetPlayerCount() != 0 || group.m_aUnitPrefabSlots.Count() != basket.m_iQuoted) return;
		for (int member; member < basket.m_iQuoted; member++)
			if (group.m_aUnitPrefabSlots[member] != basket.m_aMemberPrefabs[member]) return;
		// AICF_CATALOG_END GROUP_SCOPE
		SCR_SpawnerRequestComponent requestComponent = GetRequestComponentFromPlayerEntity(user);
		if (!requestComponent)
			return;

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return;

		SCR_SpawnerAIGroupManagerComponent groupSpawningManager = SCR_SpawnerAIGroupManagerComponent.Cast(gameMode.FindComponent(SCR_SpawnerAIGroupManagerComponent));
		if (!groupSpawningManager)
			return;

		foreach (int i, ResourceName resName : group.m_aUnitPrefabSlots)
		{
			// AICF_CATALOG_BEGIN QUEUE_EXPLICIT
			if (!AICF_QuotaParentCurrent(basket) || !groupSpawningManager.AICF_QueueBoundCatalog(basket, i, resName, user, slotEntity, rallyPoint))
			{
				basket.CancelRemaining("NATIVE_GROUP_QUEUE_SCOPE_LOST");
				return;
			}
			// AICF_CATALOG_END QUEUE_EXPLICIT
		}

		// AICF_CATALOG_BEGIN GROUP_COUNTER
		if (!AICF_QuotaParentCurrent(basket)) return;
		basket.m_bCounterAdded = true;
		// AICF_CATALOG_END GROUP_COUNTER
		requestComponent.AddQueuedAI(group.m_aUnitPrefabSlots.Count());
	}

	override void SpawnAIGroupMember(ResourceName resName, notnull IEntity user, IEntity slotEntity, SCR_EntityLabelPointComponent rallyPoint = null)
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetOwner().GetWorld());
		if (!quota)
		{
			super.SpawnAIGroupMember(resName, user, slotEntity, rallyPoint);
			return;
		}
		AICF_Stage3Diagnostics.Warning("AI_QUOTA_CATALOG_UNBOUND_DISPATCH", "native_called=0 ambient_permission=0");
	}

	bool AICF_DispatchBoundCatalogRequest(AICF_AIQuotaCatalogRequest request)
	{
		if (!request || request.m_bDispatching || !request.MatchesRequest() || !request.m_Manager.AICF_QuotaIsHead(request.m_Request)) return false;
		AICF_AIQuotaCatalogBasket basket = request.m_Basket;
		request.m_bDispatching = true;
		SCR_ChimeraCharacter ai = SCR_ChimeraCharacter.Cast(AICF_QuotaCatalogFactory(basket, request.m_Prefab, basket.m_Slot, request));
		if (!ai || !request.MatchesRequest())
		{
			request.m_bDispatching = false;
			if (!request.m_bFactoryCalled && !request.m_bFactoryRejected && request.MatchesRequest()) return false;
			basket.CancelRemaining("BOUND_MEMBER_NATIVE_NULL_OR_SCOPE_LOST");
			return false;
		}
		request.m_bMaterialized = true;
		AICF_Stage3Diagnostics.Info("AI_QUOTA_CATALOG_DISPATCH", string.Format(
			"serial=%1 index=%2 prefab=%3 bound_before_native_callback=1 exact_request=1 native_body_created=1",
			basket.m_iSerial, request.m_iMemberIndex, request.m_Prefab));
		// Original virtual chain, including native ActivateAI/AddToPlayerGroup,
		// runs only after the exact new body has been bound by the service.
		OnChimeraCharacterSpawned(ai, request.m_Request.m_UserEntity, request.m_Rally);
		request.m_bDispatching = false;
		return true;
	}

	override protected void OnDelete(IEntity owner)
	{
		m_bAICFCatalogStopping = true;
		array<ref AICF_AIQuotaCatalogBasket> basketsSnapshot = {};
		foreach (AICF_AIQuotaCatalogBasket basket : m_aAICFQuotaBaskets)
			basketsSnapshot.Insert(basket);
		m_aAICFQuotaBaskets.Clear();
		foreach (AICF_AIQuotaCatalogBasket basket : basketsSnapshot)
			if (basket) basket.CancelRemaining("PRODUCER_DELETED");
		super.OnDelete(owner);
	}
}
