modded class SCR_SpawnerAIRequest
{
	ref AICF_AIQuotaCatalogRequest m_AICFQuotaRequest;
}

// Explicit native request pointers preserve FIFO. Public unbound calls never
// borrow a basket, including synchronous calls made by another producer.
modded class SCR_SpawnerAIGroupManagerComponent
{
	bool AICF_QuotaManagerCurrent()
	{
		if (!Replication.IsServer() || !GetGame() || !GetGameMode() || GetGame().GetGameMode() != GetGameMode()) return false;
		return GetGameMode().IsMaster() && GetOwner() == GetGameMode() && GetOwner().GetWorld() == GetGame().GetWorld();
	}

	bool AICF_QuotaHasNativeRequests()
	{
		return m_aAIQueue && !m_aAIQueue.IsEmpty();
	}

	bool AICF_IsNativeQuotaQueueEmpty()
	{
		return AICF_QuotaManagerCurrent() && (!m_aAIQueue || m_aAIQueue.IsEmpty());
	}

	bool AICF_QuotaIsHead(SCR_SpawnerAIRequest request)
	{
		return AICF_QuotaManagerCurrent() && request && m_aAIQueue && !m_aAIQueue.IsEmpty() && m_aAIQueue[0] == request;
	}

	bool AICF_RemoveBoundCatalogRequest(AICF_AIQuotaCatalogRequest context)
	{
		if (!context || context.m_Manager != this || !context.MatchesNativeRequest() || !m_aAIQueue) return false;
		int index = m_aAIQueue.Find(context.m_Request);
		if (index < 0 || !context.MatchesNativeRequest() || !m_aAIQueue.IsIndexValid(index) || m_aAIQueue[index] != context.m_Request) return false;
		m_aAIQueue.RemoveOrdered(index);
		if (m_aAIQueue.IsEmpty()) ClearEventMask(GetOwner(), EntityEvent.FRAME);
		return true;
	}

	bool AICF_QueueBoundCatalog(AICF_AIQuotaCatalogBasket basket, int index, ResourceName resName,
		IEntity user, IEntity slotEntity, SCR_EntityLabelPointComponent rallyPoint)
	{
		if (!AICF_QuotaManagerCurrent() || !basket || !basket.IsSlotCurrent() || !basket.m_bHeaderObserved) return false;
		if (basket.m_Type != EEntityCatalogType.GROUP || index != basket.m_aRequests.Count()) return false;
		if (!basket.m_aMemberPrefabs.IsIndexValid(index) || basket.m_aMemberPrefabs[index] != resName || resName.IsEmpty()) return false;
		if (user != basket.m_User.m_Entity || slotEntity != basket.m_SlotOwner.m_Entity || rallyPoint != basket.m_Rally || !rallyPoint) return false;
		if (!m_aAIQueue) m_aAIQueue = new array<ref SCR_SpawnerAIRequest>();
		if (m_aAIQueue.Count() >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) return false;
		ref SCR_SpawnerAIRequest request = new SCR_SpawnerAIRequest(basket.m_Spawner, resName, user, slotEntity, rallyPoint);
		ref AICF_AIQuotaCatalogRequest context = new AICF_AIQuotaCatalogRequest();
		context.m_Basket = basket;
		context.m_Manager = this;
		context.m_Request = request;
		context.m_ManagerOwner = AICF_AIQuotaCatalogIdentity.Capture(GetOwner(), false);
		context.m_Rally = rallyPoint;
		context.m_RallyOwner = AICF_AIQuotaCatalogIdentity.Capture(rallyPoint.GetOwner(), false);
		context.m_Prefab = resName;
		context.m_iMemberIndex = index;
		request.m_AICFQuotaRequest = context;
		if (!context.MatchesRequest()) return false;
		basket.m_aRequests.Insert(context);
		m_aAIQueue.Insert(request);
		AICF_Stage3Diagnostics.Info("AI_QUOTA_CATALOG_QUEUED", string.Format(
			"serial=%1 index=%2 quoted=%3 prefab=%4 exact_request=1 native_queued=1 native_body_created=0",
			basket.m_iSerial, index, basket.m_iQuoted, resName));
		if (m_aAIQueue.Count() == 1) SetEventMask(GetOwner(), EntityEvent.FRAME);
		return context.MatchesRequest() && m_aAIQueue.Contains(request);
	}

	override void QueueSpawn(notnull SCR_CatalogEntitySpawnerComponent spawner, ResourceName resName,
		notnull IEntity user, notnull IEntity slotEntity, notnull SCR_EntityLabelPointComponent rallyPoint)
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetOwner().GetWorld());
		if (!quota)
		{
			super.QueueSpawn(spawner, resName, user, slotEntity, rallyPoint);
			return;
		}
		AICF_Stage3Diagnostics.Warning("AI_QUOTA_CATALOG_UNBOUND_QUEUE", "native_queued=0 ambient_permission=0");
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		AICF_AIWorldQuotaService quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetOwner().GetWorld());
		if (!quota)
		{
			super.EOnFrame(owner, timeSlice);
			return;
		}
		if (!AICF_QuotaManagerCurrent() || owner != GetOwner() || !m_aAIQueue || m_aAIQueue.IsEmpty()) return;
		m_fCurrentGroupMemberSpawnDelay += timeSlice;
		if (m_fCurrentGroupMemberSpawnDelay < m_fGroupMemberSpawnDelay) return;
		ref SCR_SpawnerAIRequest head = m_aAIQueue[0];
		ref AICF_AIQuotaCatalogRequest context;
		if (head) context = head.m_AICFQuotaRequest;
		// Unknown/pre-policy requests cannot be adopted after the native floor
		// changes. Bootstrap must publish policy only after native FIFO is empty.
		if (!context || !context.m_Basket) return;
		ref AICF_AIQuotaCatalogBasket basket = context.m_Basket;
		if (!basket.m_bCounterAdded) return;
		if (!context.MatchesRequest())
		{
			basket.CancelRemaining("FIFO_OWNER_OR_REQUEST_CHANGED");
			return;
		}
		bool dispatched = context.m_Basket.m_Spawner.AICF_DispatchBoundCatalogRequest(context);
		if (!dispatched && context.MatchesRequest() && !context.m_bFactoryCalled && !context.m_bFactoryRejected)
		{
			// A transient census/native capacity veto preserves the same head and
			// reservation. Retry uses the original native cadence, not a busy loop.
			m_fCurrentGroupMemberSpawnDelay = 0;
			return;
		}
		if (AICF_QuotaIsHead(head) && context.MatchesNativeRequest())
		{
			m_aAIQueue.RemoveOrdered(0);
			m_fCurrentGroupMemberSpawnDelay = 0;
			basket.RequestFinished(context, context.m_bMaterialized);
		}
		if (!AICF_QuotaManagerCurrent() || owner != GetOwner()) return;
		if (m_aAIQueue && m_aAIQueue.IsEmpty()) ClearEventMask(owner, EntityEvent.FRAME);
	}
}
