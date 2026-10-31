// Pinned1.8.0.13 stock defender factory/admission adapter. Original stock
// cleanup, proximity, importance and timing remain inverse-projected verbatim.
modded class SCR_DefenderSpawnerComponent
{
	protected ref AICF_AIQuotaDefenderBasket m_AICFDefenderBasket;

	bool AICF_QuotaDefenderOwns(AICF_AIQuotaDefenderBasket basket)
	{
		return basket && m_AIgroup == basket.m_Group && m_GroupEntry == basket.m_Entry && m_aRefillQueue == basket.m_Queue;
	}

	bool AICF_QuotaDefenderHasSlot(SCR_EntitySpawnerSlotComponent slot)
	{
		return slot && ((m_aChildSlots && m_aChildSlots.Contains(slot)) || (m_aNearSlots && m_aNearSlots.Contains(slot)));
	}

	int AICF_QuotaDefenderFreeCredit() { return m_iDespawnedGroupMembers; }

	protected void AICF_CancelDefenderBasket()
	{
		AICF_AIQuotaDefenderBasket basket = m_AICFDefenderBasket;
		m_AICFDefenderBasket = null;
		if (basket) basket.Cancel();
	}

	protected AICF_AIQuotaDefenderBasket AICF_DefenderBasket(AICF_AIWorldQuotaService quota)
	{
		if (m_AICFDefenderBasket && !m_AICFDefenderBasket.IsCurrent()) AICF_CancelDefenderBasket();
		if (!m_AICFDefenderBasket)
			m_AICFDefenderBasket = AICF_AIQuotaDefenderBasket.Capture(quota, this, m_AIgroup, m_GroupEntry, m_aRefillQueue);
		return m_AICFDefenderBasket;
	}

	override protected void ReinforceGroup()
	{
		AICF_CancelDefenderBasket();
		super.ReinforceGroup();
	}

	override protected void OnFactionChanged(FactionAffiliationComponent owner, Faction previousFaction, Faction faction)
	{
		AICF_CancelDefenderBasket();
		super.OnFactionChanged(owner, previousFaction, faction);
	}

	void ~SCR_DefenderSpawnerComponent()
	{
		AICF_CancelDefenderBasket();
	}

	override protected void SpawnUnit(ResourceName unitResource, bool consumeSupplies = true)
	{
		AICF_AIWorldQuotaService quota;
		if (GetOwner()) quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetOwner().GetWorld());
		if (!quota) { super.SpawnUnit(unitResource, consumeSupplies); return; }
		AICF_AIQuotaDefenderAttempt attempt;
		AICF_QuotaSpawnDefenderUnit(unitResource, consumeSupplies, quota, attempt);
	}

	override protected void HandleGroup()
	{
		AICF_AIWorldQuotaService quota;
		if (GetOwner()) quota = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetOwner().GetWorld());
		if (!quota) { super.HandleGroup(); return; }
		AICF_QuotaHandleDefenderGroup(quota);
		if (m_AICFDefenderBasket && (!m_AICFDefenderBasket.IsCurrent() || !m_bEnableSpawning ||
			!m_aRefillQueue || m_aRefillQueue.IsEmpty())) AICF_CancelDefenderBasket();
	}

	protected void AICF_RejectDefenderBirth(AICF_AIWorldQuotaService quota, AICF_AIQuotaCreation creation)
	{
		if (quota.ObserveExternalAbsence(creation)) return;
		if (quota.CanRollbackExternalCreation(creation))
		{
			IEntity created = creation.m_Entity;
			if (quota.CanRollbackExternalCreation(creation) && creation.m_Body.MatchesIdentity())
				RplComponent.DeleteRplEntity(created, false);
			if (quota.ObserveExternalAbsence(creation)) return;
		}
		quota.RejectExternalCreation(creation, "DEFENDER_NATIVE_CONTEXT_CHANGED");
	}

	protected void AICF_QuotaSpawnDefenderUnit(ResourceName unitResource, bool consumeSupplies,
		AICF_AIWorldQuotaService quota, out AICF_AIQuotaDefenderAttempt attempt)
	{
		if (IsProxy())
			return;

		if (!m_GroupEntry || !m_AIgroup || !m_AIgroup.m_aUnitPrefabSlots || m_AIgroup.m_aUnitPrefabSlots.IsEmpty())
			return;

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return;

		SCR_EntityCatalogSpawnerData entityData = SCR_EntityCatalogSpawnerData.Cast(m_GroupEntry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (!entityData)
			return;

		SCR_EntitySpawnerSlotComponent slot = GetFreeSlot();
		if (!slot)
			return;

		// Calculate unit spawn cost from overall group member count
		int spawnCost;
		if (m_AIgroup.m_aUnitPrefabSlots.Count() > 0)
			spawnCost = entityData.GetSupplyCost() / m_AIgroup.m_aUnitPrefabSlots.Count();

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		slot.GetOwner().GetTransform(spawnParams.Transform);
		Resource res = Resource.Load(unitResource);

				// AICF_QUOTA_BLOCK_BEGIN FACTORY_BEGIN
		attempt = null;
		AICF_AIQuotaDefenderBasket basket = AICF_DefenderBasket(quota);
		if (!basket || !basket.EnsureRemaining()) return;
		attempt = AICF_AIQuotaDefenderAttempt.Capture(basket, slot, unitResource, GetSpawnerSupplies(), m_iDespawnedGroupMembers);
		if (!attempt) return;
		AICF_AIQuotaCreation creation = quota.BeginExternalCreation(basket.NextReservation(), basket.m_iSerial,
			unitResource, GetOwner(), false, attempt);
		if (!creation) return;
		if (!attempt.IsCurrent() || !quota.IsConfigured())
		{
			quota.CancelExternalCreationBeforeFactory(creation);
			return;
		}
		if (!quota.MarkExternalNativeCallStarted(creation))
		{
			quota.CancelExternalCreationBeforeFactory(creation);
			return;
		}
		// AICF_QUOTA_BLOCK_END FACTORY_BEGIN
		IEntity ai = GetGame().SpawnEntityPrefab(res, null, spawnParams);
		// AICF_QUOTA_BLOCK_BEGIN FACTORY_OBSERVE
		quota.ObserveExternalFactory(creation, ai);
		if (creation.m_Status == AICF_EAIQuotaBirthStatus.NATIVE_NULL)
		{
			quota.EndExternalCreation(creation);
			return;
		}
		if (creation.m_Status != AICF_EAIQuotaBirthStatus.BOUND || !attempt.IsCurrent())
		{
			AICF_RejectDefenderBirth(quota, creation);
			return;
		}
		if (!quota.EndExternalCreation(creation))
		{
			quota.RejectExternalCreation(creation, "DEFENDER_CREATION_FINISH_CHANGED");
			return;
		}
		attempt.m_bBound = true;
		// AICF_QUOTA_BLOCK_END FACTORY_OBSERVE
		if (!ai)
			return;

		if (m_aRefillQueue)
			m_aRefillQueue.RemoveItem(unitResource);

		// consume supplies, if their consumption is enabled and revents spawning, if there is not enough supplies available.
		if (consumeSupplies && m_bSuppliesConsumptionEnabled && GetSpawnerSupplies() >= spawnCost)
			AddSupplies(-spawnCost);

		AIControlComponent control = AIControlComponent.Cast(ai.FindComponent(AIControlComponent));
		if (control)
			control.ActivateAI();

		AIAgent agent = control.GetAIAgent();
		if (!agent)
			return;

		//Remove group waypoint to reduce performance impact due to new members being added to it.
		if (m_Waypoint)
			m_AIgroup.RemoveWaypoint(m_Waypoint);

		SCR_DefenderSpawnerComponentClass prefabData = SCR_DefenderSpawnerComponentClass.Cast(GetComponentData(GetOwner()));

		SCR_EntityLabelPointComponent rallyPointComponent = slot.GetRallyPoint();
		if (rallyPointComponent && prefabData)
		{
			IEntity rallyPointEntity = rallyPointComponent.GetOwner();

			// Create temporary group for unit and later rally point waypoint
			SCR_AIUtilityComponent utilityComponent = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));

			vector rpTransform[4];
			rallyPointEntity.GetWorldTransform(rpTransform);

			AIFormationDefinition formationDefiniton = aiWorld.GetFormation(prefabData.GetExitFormation());
			vector posLocal;

			if (m_aUnitsOnMove && formationDefiniton)
			{
				vector formationOffset = 0.5 * formationDefiniton.GetOffsetPosition(m_aUnitsOnMove.Count());
				posLocal = posLocal + formationOffset;
			}

			vector posWorld = posLocal.Multiply4(rpTransform);

			SCR_AIMoveIndividuallyBehavior moveBehavior = new SCR_AIMoveIndividuallyBehavior(utilityComponent, null, posWorld, SCR_AIActionBase.PRIORITY_BEHAVIOR_MOVE, SCR_AIActionBase.PRIORITY_LEVEL_GAMEMASTER, radius: 0.5);
			if (!moveBehavior)
			{
				m_AIgroup.AddAgent(agent);
				return;
			}

			utilityComponent.AddAction(moveBehavior);

			if (!m_aUnitsOnMove)
				m_aUnitsOnMove = {};

			ChimeraWorld world = GetOwner().GetWorld();

			m_aUnitsOnMove.Insert(new Tuple3<SCR_AIActionBase, AIAgent, WorldTimestamp>(moveBehavior, agent, world.GetServerTimestamp()));

			moveBehavior.m_OnActionCompleted.Insert(OnMoveFinished);

			SCR_CharacterControllerComponent charController = SCR_CharacterControllerComponent.Cast(ai.FindComponent(SCR_CharacterControllerComponent));
			if (!charController)
				return;

			charController.m_OnPlayerDeathWithParam.Insert(OnMovingCharacterDeath);
		}
		else
		{
			// Rally point not found, add spawned ai directly to controlled group
			m_AIgroup.AddAgent(agent);
		}
	}

	protected void AICF_QuotaHandleDefenderGroup(AICF_AIWorldQuotaService quota)
	{
		ChimeraWorld world = GetOwner().GetWorld();
		WorldTimestamp replicationTime = world.GetServerTimestamp();

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return;

		//If another Ai would be over limit, stop requesting and prevent players from enabling it again.
		if (!aiWorld.CanActivateGroup(m_AIgroup))
		{
			if (m_bEnableSpawning)
			{
				m_bEnableSpawning = false; //disable spawner
				Replication.BumpMe();
			}
			return;
		}

		SCR_EDefenderSpawnerState distanceState = GetPlayerDistanceState();

		if (m_bEnableSpawning && (distanceState == SCR_EDefenderSpawnerState.DEFENDERS_ENABLED))
		{
			// Add any stray or stuck units to group
			if (m_aUnitsOnMove)
			{
				int count = m_aUnitsOnMove.Count()-1;
				for (int i = count; i >= 0; i--)
				{
					if (!m_aUnitsOnMove[i].param2)
					{
						m_aUnitsOnMove.Remove(i);
						continue;
					}

					if (!m_aUnitsOnMove[i].param3.PlusSeconds(SPAWN_GROUP_JOIN_TIMEOUT).GreaterEqual(replicationTime))
						OnMoveFinished(m_aUnitsOnMove[i].param1);
				}
			}

			// Reinforce existing or create new defender group. Also handles respawning of despawned defenders
			if (m_iDespawnedGroupMembers > 0 || replicationTime.Greater(m_fNextRespawnTime))
			{
				m_fNextRespawnTime = replicationTime.PlusSeconds(m_fRespawnDelay);
				if (m_AIgroup && (m_AIgroup.m_aUnitPrefabSlots.Count() != m_AIgroup.GetAgentsCount()))
					ReinforceGroup();

				if (!m_AIgroup)
					SpawnGroup();
			}

			//Spawn queued units
			if (m_aRefillQueue && !m_aRefillQueue.IsEmpty())
			{
				if (m_iDespawnedGroupMembers > 0)
				{
					// AICF_QUOTA_BLOCK_BEGIN FREE_CREDIT
					AICF_AIQuotaDefenderAttempt attempt;
					AICF_QuotaSpawnDefenderUnit(m_aRefillQueue[0], false, quota, attempt);
					if (attempt && attempt.CanConsumeFreeCredit()) m_iDespawnedGroupMembers--;
					// AICF_QUOTA_BLOCK_END FREE_CREDIT
				}
				else
				{
					// AICF_QUOTA_BLOCK_BEGIN PAID_DISPATCH
					AICF_AIQuotaDefenderAttempt paidAttempt;
					AICF_QuotaSpawnDefenderUnit(m_aRefillQueue[0], true, quota, paidAttempt);
					// AICF_QUOTA_BLOCK_END PAID_DISPATCH
				}
			}
		}

		// AICF_QUOTA_BLOCK_BEGIN DISTANT_CANCEL
		if (distanceState == SCR_EDefenderSpawnerState.DEFENDERS_DESPAWN) AICF_CancelDefenderBasket();
		// AICF_QUOTA_BLOCK_END DISTANT_CANCEL
		//Despawn units that are too far away from players
		if (distanceState == SCR_EDefenderSpawnerState.DEFENDERS_DESPAWN)
		{
			if (m_AIgroup && !m_AIgroup.HasHeldMember())
			{
				// Keep the group entity and drop it to Dormant so GM still sees it, and a later
				// SpawnGroup / ReinforceGroup trickles members back from the refill queue.
				// m_iDespawnedGroupMembers tracks "spawn these without charging supplies"; it stays
				// in addition to the group's own dormant alive-count.
				// HasHeldMember() guard: skip auto-despawn while any member is force-held (driver
				// in vehicle, player subordinate, permanent-LOD set). Same protection as the
				// proximity-driven LifecycleTick path.
				m_iDespawnedGroupMembers = m_AIgroup.GetAgentsCount();
				m_AIgroup.DespawnMembers();
			}

			//If leftover groups from previous owners remains alive, they will be despawned
			if (m_aPreviousGroups)
			{
				foreach (SCR_AIGroup aiGroup : m_aPreviousGroups)
				{
					SCR_EntityHelper.DeleteEntityAndChildren(aiGroup);
				}

				m_aPreviousGroups = null;
			}

			//Delete units from previous owners that were spawning in process of faction change. (edge case, but might happen)
			if (m_aOldUnits)
			{
				int index = m_aOldUnits.Count()-1;
				for (index; index >= 0; index--)
				{
					SCR_EntityHelper.DeleteEntityAndChildren(m_aOldUnits[index]);
				}

				m_aOldUnits = null;
			}

			//Despawn any units that are currently walking to RP
			if (m_aUnitsOnMove)
			{
				foreach (Tuple3<SCR_AIActionBase, AIAgent, WorldTimestamp> groupWaypoint : m_aUnitsOnMove)
				{
					SCR_EntityHelper.DeleteEntityAndChildren(groupWaypoint.param2.GetControlledEntity());
					m_iDespawnedGroupMembers++;
				}

				m_aUnitsOnMove = null;
			}
		}
	}
}
