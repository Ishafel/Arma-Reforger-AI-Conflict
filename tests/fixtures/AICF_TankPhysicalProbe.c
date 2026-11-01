// Только canonical Scenario case=tank, WCS, одна сторона на isolated server.
// Начальный crew/scenario truce — controlled setup; покупка/посадка/движение
// выполняются обычными production владельцами. Нет pose/health recovery writes.
modded class AICF_MatchController
{
	protected bool m_bTankProfileRestored;
	protected bool m_bTankSetup;
	protected bool m_bTankSeeded;
	protected bool m_bTankStorageAttempted;
	protected bool m_bTankGateChecked;
	protected int m_iTankNearbyGateCount;
	protected IEntity m_TankStorageEntity;
	protected int m_iTankStorageOriginalMaximum;
	protected bool m_bTankMoved;
	protected int m_iTankStarted;
	protected int m_iTankMovingPolls;
	protected int m_iTankCrewHoldStarted;
	protected int m_iTankLastReport;
	protected Vehicle m_TankPhysicalVehicle;
	protected EntityID m_TankPhysicalId;
	protected ref AICF_VehicleWatchdog m_TankPhysicalWatchdog = new AICF_VehicleWatchdog();

	protected bool AICF_TankPhysicalEnabled()
	{
		string enabled;
		return System.GetCLIParam("aicfTankPhysicalProbe", enabled) && enabled == "1";
	}

	protected FactionKey AICF_TankPhysicalFaction()
	{
		string faction;
		if (System.GetCLIParam("aicfVehicleScenarioFaction", faction) && faction == "US") return "US";
		return "USSR";
	}

	protected int AICF_TankPhysicalSize()
	{
		if (AICF_TankPhysicalFaction() == "US") return 4;
		return 3;
	}

	protected bool AICF_TankNativeStorage()
	{
		string enabled;
		return System.GetCLIParam("aicfTankUseNativeStorage", enabled) && enabled == "1";
	}

	protected bool AICF_TankCollectNearbyGate(IEntity entity)
	{
		SCR_AISmartActionComponent smart = SCR_AISmartActionComponent.Cast(entity.FindComponent(SCR_AISmartActionComponent));
		if (!smart) return true;
		array<string> tags = {};
		smart.GetTags(tags);
		if (!tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)) return true;
		m_iTankNearbyGateCount++;
		Print(string.Format("[AICF][TANK_PHYSICAL_PROBE] event=NEARBY_GATE entity=%1 position=%2 prefab=%3 smart_tag=%4 readonly_discovery=1",
			AICF_GroupTrace.Entity(entity), entity.GetOrigin(), SCR_ResourceNameUtils.GetPrefabName(entity), SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG));
		return true;
	}

	protected void AICF_TankPrepareNativeStorage(SCR_CampaignFaction faction)
	{
		if (m_bTankStorageAttempted) return;
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		m_iTankStorageOriginalMaximum = base.GetSuppliesMax();
		SCR_CampaignBuildingProviderComponent provider = base.GetMasterProvider();
		SCR_CampaignBuildingManagerComponent manager = SCR_CampaignBuildingManagerComponent.Cast(m_Campaign.FindComponent(SCR_CampaignBuildingManagerComponent));
		ResourceName prefab = faction.GetBuildingPrefab(EEditableEntityLabel.SERVICE_SUPPLY_STORAGE);
		if (prefab.IsEmpty() && manager)
		{
			array<ResourceName> registered = manager.GetPlaceablePrefabs();
			registered.Sort();
			foreach (ResourceName candidate : registered)
			{
				SCR_EditableEntityUIInfo candidateInfo = SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(candidate);
				if (!candidateInfo || !candidateInfo.HasEntityLabel(EEditableEntityLabel.SERVICE_SUPPLY_STORAGE) ||
					!candidateInfo.HasEntityLabel(EEditableEntityLabel.TRAIT_SERVICE)) continue;
				Print(string.Format("[AICF][TANK_STORAGE_PROBE] event=CATALOG prefab=%1 composition_id=%2 info_faction=%3 runtime_faction=%4 stable_faction=%5 faction_label=%6 has_runtime_label=%7",
					candidate, manager.GetCompositionId(candidate), candidateInfo.GetFactionKey(), faction.GetFactionKey(),
					AICF_TankPhysicalFaction(), faction.GetFactionLabel(), candidateInfo.HasEntityLabel(faction.GetFactionLabel())));
				if (prefab.IsEmpty() && (candidateInfo.GetFactionKey() == faction.GetFactionKey() ||
					candidateInfo.GetFactionKey() == AICF_TankPhysicalFaction())) prefab = candidate;
			}
		}
		Print(string.Format("[AICF][TANK_STORAGE_PROBE] event=BEFORE faction=%1 capacity=%2 supplies=%3 prefab=%4 provider=%5 manager=%6 controlled_stock_composition=1",
			AICF_TankPhysicalFaction(), base.GetSuppliesMax(), base.GetSupplies(), prefab, provider != null, manager != null));
		if (!provider || !manager || prefab.IsEmpty()) return;
		m_bTankStorageAttempted = true;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid()) return;
		SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(prefab);
		if (info)
		{
			array<EEditableEntityLabel> labels = {};
			info.GetEntityLabels(labels);
			string values;
			foreach (EEditableEntityLabel label : labels) values += label.ToString() + ",";
			Print(string.Format("[AICF][TANK_STORAGE_PROBE] event=METADATA prefab=%1 composition_id=%2 info_faction=%3 runtime_faction=%4 faction_label=%5 has_runtime_label=%6 has_storage_label=%7 has_service_trait=%8 labels=%9",
				prefab, manager.GetCompositionId(prefab), info.GetFactionKey(), faction.GetFactionKey(), faction.GetFactionLabel(),
				info.HasEntityLabel(faction.GetFactionLabel()), info.HasEntityLabel(EEditableEntityLabel.SERVICE_SUPPLY_STORAGE), info.HasEntityLabel(EEditableEntityLabel.TRAIT_SERVICE), values));
		}
		vector position = base.GetOwner().GetOrigin() + "60 0 60";
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD; params.Transform[3] = position;
		IEntity previousProvider = manager.GetTemporaryProvider();
		manager.SetTemporaryProvider(provider.GetOwner());
		m_TankStorageEntity = GetGame().SpawnEntityPrefabEx(prefab, false, params: params);
		manager.SetTemporaryProvider(previousProvider);
		if (!m_TankStorageEntity) return;
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(m_TankStorageEntity.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (composition) composition.SetProviderEntityServer(provider.GetOwner());
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(m_TankStorageEntity.FindComponent(FactionAffiliationComponent));
		if (affiliation) affiliation.SetAffiliatedFaction(faction);
		Print(string.Format("[AICF][TANK_STORAGE_PROBE] event=SPAWNED faction=%1 entity=%2 composition=%3 faction_ready=%4 position=%5 capacity=%6 storage_placement=CONTROLLED_INITIAL_CONDITION storage_purchase_proven=0",
			AICF_TankPhysicalFaction(), AICF_GroupTrace.Entity(m_TankStorageEntity), composition != null,
			SCR_Faction.GetEntityFaction(m_TankStorageEntity) == faction, position, base.GetSuppliesMax()));
	}

	override protected bool SpawnInitialRoster(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		if (AICF_TankPhysicalEnabled() && AICF_VSCanonicalOwner() && Replication.IsServer() && factionState && faction &&
			m_ContentProfile.GetStableFactionKey(factionState.GetFactionKey()) == AICF_TankPhysicalFaction())
		{
			AICF_GroupSlot slot = factionState.GetSlot(0);
			if (!slot || slot.GetState() != AICF_EGroupSlotState.EMPTY || slot.GetGroup()) return false;
			slot.SetUnitType(AICF_EGroupUnitType.MOTORIZED_LIGHT);
			slot.SetDesiredSize(AICF_TankPhysicalSize());
		}
		return super.SpawnInitialRoster(factionState, faction);
	}

	override protected void ProcessFaction(AICF_FactionState factionState, SCR_CampaignFaction faction)
	{
		super.ProcessFaction(factionState, faction);
		if (!AICF_TankPhysicalEnabled() || !AICF_VSCanonicalOwner() || !Replication.IsServer() || m_bStopped ||
			m_bTankProfileRestored || !factionState || !faction ||
			m_ContentProfile.GetStableFactionKey(factionState.GetFactionKey()) != AICF_TankPhysicalFaction()) return;
		AICF_GroupSlot slot = factionState.GetSlot(0);
		if (!slot || !slot.IsCombatReady() || slot.GetSpawnGeneration() != 1 || slot.IsReplacementDeployment() ||
			slot.GetUnitType() != AICF_EGroupUnitType.MOTORIZED_LIGHT || slot.GetDesiredSize() != AICF_TankPhysicalSize() ||
			!slot.GetGroup() || slot.GetGroup().GetAgentsCount() != AICF_TankPhysicalSize()) return;
		string requireGate;
		if (!m_bTankGateChecked && System.GetCLIParam("aicfTankRequireNearbyGate", requireGate) && requireGate == "1")
		{
			m_bTankGateChecked = true;
			vector basePosition = faction.GetMainBase().GetOwner().GetOrigin();
			GetGame().GetWorld().QueryEntitiesBySphere(basePosition, 150, AICF_TankCollectNearbyGate, null, EQueryEntitiesFlags.ALL);
			Print(string.Format("[AICF][TANK_PHYSICAL_PROBE] event=GATE_PRECONDITION faction=%1 base_position=%2 nearby_open_gate_count=%3 radius_m=150 readonly_discovery=1",
				AICF_TankPhysicalFaction(), basePosition, m_iTankNearbyGateCount));
			if (!m_iTankNearbyGateCount)
			{
				Print("[AICF][TANK_PHYSICAL_PROBE] event=NOT_APPLICABLE reason=NATIVE_RANDOM_HQ_WITHOUT_NEARBY_GATE purchase_proven=0 exit_handoff_proven=0");
				AICF_VSFinish();
				return;
			}
		}
		if (AICF_TankNativeStorage() && !m_bTankStorageAttempted)
		{
			AICF_TankPrepareNativeStorage(faction);
			return;
		}
		slot.SetUnitType(AICF_EGroupUnitType.INFANTRY);
		m_bTankProfileRestored = true;
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		int before = base.GetSupplies();
		int maximumBefore = base.GetSuppliesMax();
		// Controlled capacity precondition for the physical gate only. No quote,
		// debit, economy/AI quota decision or production maximum is bypassed.
		AICF_LogisticsResourceAdapter resources = new AICF_LogisticsResourceAdapter();
		AICF_LogisticsResourcePool pool = resources.Resolve(base.GetResourceComponent());
		if (!AICF_TankNativeStorage() && pool && pool.Valid() && !pool.m_aLeaves.IsEmpty())
		{
			SCR_ResourceContainer container = pool.m_aLeaves[0].m_Container;
			if (container && container.GetOwner() && container.GetOwner().GetWorld() == GetGame().GetWorld())
				container.SetMaxResourceValue(10000);
		}
		base.AddSupplies(base.GetSuppliesMax() - before);
		m_bTankSeeded = base.GetSuppliesMax() > 1000 && base.GetSupplies() == base.GetSuppliesMax();
		if (AICF_TankNativeStorage())
		{
			maximumBefore = m_iTankStorageOriginalMaximum;
			SCR_CampaignBuildingCompositionComponent composition;
			if (m_TankStorageEntity) composition = SCR_CampaignBuildingCompositionComponent.Cast(m_TankStorageEntity.FindComponent(SCR_CampaignBuildingCompositionComponent));
			Print(string.Format("[AICF][TANK_STORAGE_PROBE] event=READY faction=%1 original_capacity=%2 actual_capacity=%3 supplies=%4 composition_spawned=%5 direct_capacity_write=0 native_aggregated_capacity=1",
				AICF_TankPhysicalFaction(), maximumBefore, base.GetSuppliesMax(), base.GetSupplies(), composition && composition.IsCompositionSpawned()));
			SCR_ServicePointComponent service = AICF_TankFindStorageService(m_TankStorageEntity);
			if (service)
				Print(string.Format("[AICF][TANK_STORAGE_PROBE] event=SERVICE faction=%1 type=%2 state=%3 label=%4 faction_ready=%5",
					AICF_TankPhysicalFaction(), service.GetType(), service.GetServiceState(), service.GetLabel(),
					SCR_Faction.GetEntityFaction(m_TankStorageEntity) == faction));
			if (pool)
				foreach (AICF_LogisticsLeaf leaf : pool.m_aLeaves)
					Print(string.Format("[AICF][TANK_STORAGE_PROBE] event=LEAF faction=%1 owner=%2 prefab=%3 maximum=%4 value=%5",
						AICF_TankPhysicalFaction(), AICF_GroupTrace.Entity(leaf.m_Container.GetOwner()), SCR_ResourceNameUtils.GetPrefabName(leaf.m_Container.GetOwner()),
						leaf.m_Container.GetMaxResourceValue(), leaf.m_Container.GetResourceValue()));
		}
		Print(string.Format("[AICF][TANK_PHYSICAL_PROBE] event=ECONOMY_SETUP faction=%1 supplies_before=%2 supplies_after=%3 supplies_max=%4 original_max=%5 actual_purchase_required=1 controlled_seed=1 controlled_capacity=1 production_capacity_proven=0",
			AICF_TankPhysicalFaction(), before, base.GetSupplies(), base.GetSuppliesMax(), maximumBefore));
		Print(string.Format("[AICF][TANK_PHYSICAL_PROBE] event=CANONICAL_READY faction=%1 crew=%2 initial_generation=1 before_coordinator_tick=1",
			AICF_TankPhysicalFaction(), AICF_TankPhysicalSize()));
	}

	protected SCR_ServicePointComponent AICF_TankFindStorageService(IEntity entity, int depth = 0)
	{
		if (!entity || depth > 32) return null;
		SCR_ServicePointComponent service = SCR_ServicePointComponent.Cast(entity.FindComponent(SCR_ServicePointComponent));
		if (service && service.GetType() == SCR_EServicePointType.SUPPLY_DEPOT) return service;
		for (IEntity child = entity.GetChildren(); child; child = child.GetSibling())
		{
			SCR_ServicePointComponent found = AICF_TankFindStorageService(child, depth + 1);
			if (found) return found;
		}
		return null;
	}

	override protected void Update()
	{
		super.Update();
		if (!AICF_TankPhysicalEnabled() || !Replication.IsServer() || m_bStopped || m_bVSDone || !m_bRosterReady) return;
		int now = System.GetTickCount();
		if (!m_iTankStarted) m_iTankStarted = now;
		if (now - m_iTankStarted > 600000)
		{
			AICF_VSCheck("TANK_PHYSICAL_TIMEOUT", false); AICF_VSFinish();
		}
	}

	override protected bool AICF_VSAlternativeScenario(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		if (!AICF_TankPhysicalEnabled()) return super.AICF_VSAlternativeScenario(slot, trip);
		int now = System.GetTickCount();
		if (!m_bTankSetup)
		{
			m_bTankSetup = true;
			AICF_VSCheck("TANK_EXPECTED_COMBAT_CREW", m_bTankSeeded && m_bTankProfileRestored && m_VSRecipe &&
				m_VSRecipe.m_iBehavior == AICF_EVehicleBehavior.COMBAT && m_iVSSize == AICF_TankPhysicalSize());
		}
		// Initial native boarding, mounted wait and real distant order belong to
		// the common scaffold. No direct tick/outcome/phase override here.
		if (m_iVSStep < 2) return false;
		if (!trip || trip.IsTerminal()) { AICF_VSCheck("TANK_UNEXPECTED_TERMINAL", false); AICF_VSFinish(); return true; }
		if (!trip.GetLease() || !trip.GetLease().HasPhysicalAsset()) return true;
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!m_TankPhysicalVehicle)
		{
			m_TankPhysicalVehicle = vehicle; m_TankPhysicalId = vehicle.GetID();
			AICF_VSCheck("TANK_EXACT_NATIVE_CREW", AICF_TankPhysicalCrew(slot, trip));
			Print(string.Format("[AICF][TANK_PHYSICAL_PROBE] event=START faction=%1 operation_id=%2 vehicle=%3 crew=%4 prefab=%5 initial_position=%6 deadline=%7",
				m_sVSFaction, trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), m_iVSSize, m_VSRecipe.m_sPrefab, m_vVSStart, trip.GetAbsoluteDeadlineMs()));
		}
		if (vehicle != m_TankPhysicalVehicle || vehicle.GetID() != m_TankPhysicalId || !AICF_VSOwnRecoveryRoster(slot))
		{
			AICF_VSCheck("TANK_IDENTITY_CHANGED", false); AICF_VSFinish(); return true;
		}
		Physics physics = vehicle.GetPhysics();
		if (physics && physics.GetVelocity().Length() >= 0.5) m_iTankMovingPolls++;
		float displacement = vector.DistanceXZ(m_vVSStart, vehicle.GetOrigin());
		if (now - m_iTankLastReport >= 10000)
		{
			m_iTankLastReport = now;
			Print(string.Format("[AICF][TANK_PHYSICAL_PROBE] event=READBACK faction=%1 phase=%2 reason=%3 displacement_m=%4 moving_polls=%5 position=%6 exact_crew=%7",
				m_sVSFaction, trip.GetPhase(), trip.GetLastTransitionReason(), displacement, m_iTankMovingPolls, vehicle.GetOrigin(), AICF_TankPhysicalCrew(slot, trip)));
		}
		if (!m_bTankMoved && displacement >= 50 && m_iTankMovingPolls >= 3)
		{
			m_bTankMoved = true;
			AICF_VSCheck("TANK_REAL_NATIVE_MOVEMENT", AICF_TankPhysicalCrew(slot, trip));
			Print(string.Format("[AICF][TANK_PHYSICAL_PROBE] event=MOVED faction=%1 operation_id=%2 vehicle=%3 displacement_m=%4 moving_polls=%5 same_asset=1 original_crew=1",
				m_sVSFaction, trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), displacement, m_iTankMovingPolls));
		}
		if (trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || trip.GetLastTransitionReason() != "DESTINATION_REACHED") return true;
		if (!m_iTankCrewHoldStarted) m_iTankCrewHoldStarted = now;
		if (!AICF_TankPhysicalCrew(slot, trip)) { AICF_VSCheck("TANK_COMBAT_CREW_DISEMBARKED", false); AICF_VSFinish(); return true; }
		if (now - m_iTankCrewHoldStarted < 5000) return true;
		AICF_VSCheck("TANK_COMBAT_ARRIVAL_CREW_RETAINED", m_bTankMoved && trip.GetBehaviorState().m_aDeployed.IsEmpty());
		Print(string.Format("[AICF][TANK_PHYSICAL_PROBE] event=DONE faction=%1 operation_id=%2 vehicle=%3 displacement_m=%4 moving_polls=%5 crew=%6 loader_retained=%7 cargo_deployed=%8 controlled_truce=1 actual_combat_threat=0",
			m_sVSFaction, trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), displacement, m_iTankMovingPolls, m_iVSSize, m_sVSFaction == "US", trip.GetBehaviorState().m_aDeployed.Count()));
		AICF_VSFinish(); return true;
	}

	protected bool AICF_TankPhysicalCrew(AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		Vehicle vehicle = trip.GetLease().GetVehicle();
		if (!vehicle || !m_TankPhysicalWatchdog.AreAllAliveMembersSettledInVehicle(m_VSGroup, vehicle)) return false;
		array<BaseCompartmentSlot> seats = {};
		if (!AICF_VehiclePreset.CollectCaptured(vehicle, m_VSRecipe, seats) || seats.Count() != m_iVSSize) return false;
		int cargoCrew;
		foreach (int index, BaseCompartmentSlot seat : seats)
		{
			int position = m_VSRecipe.m_aMembers[index];
			IEntity actor = trip.VehiclePresetMember(position);
			if (position < 0 || slot.GetRosterMember(position) != actor || !actor || actor.GetID() != m_aVSMembers[position] ||
				seat.GetOccupant() != actor || !AICF_VehicleBehaviorFlow.IsCrewSeat(seat, m_VSRecipe)) return false;
			if (seat.GetType() == ECompartmentType.CARGO) cargoCrew++;
		}
		if (m_sVSFaction == "US" && cargoCrew != 1) return false;
		return true;
	}
}

// Native invoker boundary observer: читает actual production proof после
// регистрации receipt. Не меняет action, phase, outcome или физическое положение.
modded class AICF_TransportNativeGate
{
	override protected void OnExitCompleted(SCR_AIActionBase action)
	{
		super.OnExitCompleted(action);
		string enabled;
		if (!System.GetCLIParam("aicfTankPhysicalProbe", enabled) || enabled != "1" ||
			!m_bExitCompleted || !m_Trip || action != m_Exit || !m_Utility) return;
		AIActionBase current = m_Utility.GetCurrentBehavior();
		bool accepted = OwnsTemporaryCrewExit(m_Trip, m_Seat, m_Member);
		array<ref AIActionBase> actions = {};
		m_Utility.GetActions(actions);
		string details = string.Format("event=EXIT_HANDOFF_BOUNDARY operation_id=%1 vehicle=%2 member=%3 accepted=%4 current_is_exit=%5 current_completed=%6 current_removed=%7 exit_receipt=%8 exit_receipt_ms=%9",
			m_sOperation, AICF_GroupTrace.Entity(m_Vehicle), AICF_GroupTrace.Entity(m_Member), accepted,
			current == m_Exit, current && current.GetActionState() == EAIActionState.COMPLETED,
			current && current.GetRemoveAction(), m_bExitCompleted, m_iExitCompletedAtMs);
		details += string.Format(" perform_queued=%1 return_queued=%2 window_start_ms=%3 window_deadline_ms=%4 observer_readonly=1 production_outcome_unchanged=1",
			actions.Contains(m_Perform), actions.Contains(m_Return), m_iStarted, m_iDeadline);
		Print("[AICF][TANK_GATE_EXIT_HANDOFF_PROBE] " + details);
	}
}

// One-shot fault только в native bookkeeping текущей полной healthy crew.
// Вызов super остаётся обычным production Attach/Reconcile, outcome не заменяется.
modded class SCR_AIGroupUtilityComponent
{
	protected bool m_bTankPositiveFaultAttempted;
	override bool AICF_ReconcileExactVehicleSubgroup(AICF_TransportTrip trip)
	{
		string enabled;
		bool fault = !m_bTankPositiveFaultAttempted && System.GetCLIParam("aicfTankPositiveHandlerFault", enabled) && enabled == "1";
		SCR_AIGroup group;
		Vehicle vehicle;
		SCR_AIGroupVehicle registered;
		AIAgent pilot;
		AIGroupMovementComponent movement;
		string reason;
		int originalHandler = -1;
		int createdHandler = -1;
		int deadline;
		int attempts;
		int leaseGeneration;
		int vehicleGeneration;
		bool injected;
		if (fault && AICF_VehicleNativeSubgroup.Read(trip, this, group, vehicle, registered, pilot, movement, reason) && pilot)
		{
			originalHandler = registered.GetSubgroupHandleId();
			SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
			SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
			AICF_VehicleWatchdog watchdog = new AICF_VehicleWatchdog();
			bool healthy = usage && usage.GetDamageState() == EDamageState.UNDAMAGED && damage && damage.GetDefaultHitZone() &&
				damage.GetDefaultHitZone().GetHealthScaled() == 1.0 && damage.GetMovementDamage() == 0 &&
				SCR_AIVehicleUsability.VehicleCanMove(vehicle) && !SCR_AIVehicleUsability.VehicleIsOnFire(vehicle);
			bool full = group.GetAgentsCount() == trip.GetVehiclePresetSlot().GetDesiredSize() && watchdog.AreAllAliveMembersSettledInVehicle(group, vehicle);
			if (healthy && full && originalHandler > AIGroupMovementComponent.DEFAULT_HANDLER_ID &&
				movement.GetAgentMoveHandlerId(pilot) == originalHandler && movement.GetFormationDefinition(originalHandler))
			{
				m_bTankPositiveFaultAttempted = true;
				deadline = trip.GetAbsoluteDeadlineMs();
				attempts = trip.GetMovementState().GetMobilityRecoveryAttempts();
				leaseGeneration = trip.GetLease().GetLeaseGeneration();
				vehicleGeneration = trip.GetLease().GetVehicleGeneration();
				createdHandler = movement.CreateGroupMoveHandler(movement.GetFormationDefinition(originalHandler).GetName());
				if (createdHandler > AIGroupMovementComponent.DEFAULT_HANDLER_ID && createdHandler != originalHandler && movement.GetMoveHandlerAgentCount(createdHandler) == 0)
				{
					movement.SetMoveHandlerLeader(pilot, originalHandler, createdHandler);
					injected = movement.GetAgentMoveHandlerId(pilot) == createdHandler && registered.GetSubgroupHandleId() == originalHandler;
					Print(string.Format("[AICF][TANK_POSITIVE_HANDLER_PROBE] event=INJECTED operation_id=%1 vehicle=%2 record=%3 pilot_handler=%4 deadline=%5 attempts=%6 lease_generation=%7 vehicle_generation=%8 injected=%9 healthy=1 full_settled=1 controlled_fault=1 physical_mutation=0",
						trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), originalHandler, movement.GetAgentMoveHandlerId(pilot), deadline, attempts, leaseGeneration, vehicleGeneration, injected));
				}
			}
		}
		bool result = super.AICF_ReconcileExactVehicleSubgroup(trip);
		if (injected)
		{
			string details = string.Format("event=DECISION operation_id=%1 vehicle=%2 accepted=%3 record=%4 pilot_handler=%5 deadline=%6 attempts=%7 lease_generation=%8 vehicle_generation=%9",
				trip.GetOperationId(), AICF_GroupTrace.Entity(vehicle), result, registered.GetSubgroupHandleId(), movement.GetAgentMoveHandlerId(pilot),
				trip.GetAbsoluteDeadlineMs(), trip.GetMovementState().GetMobilityRecoveryAttempts(), trip.GetLease().GetLeaseGeneration(), trip.GetLease().GetVehicleGeneration());
			details += string.Format(" original_record=%1 created_handler=%2 same_identity=%3 controlled_fault=1 production_outcome_unchanged=1 physical_mutation=0",
				originalHandler, createdHandler, deadline == trip.GetAbsoluteDeadlineMs() && attempts == trip.GetMovementState().GetMobilityRecoveryAttempts() &&
				leaseGeneration == trip.GetLease().GetLeaseGeneration() && vehicleGeneration == trip.GetLease().GetVehicleGeneration());
			Print("[AICF][TANK_POSITIVE_HANDLER_PROBE] " + details);
		}
		// Удаляется только созданный этим callback уже пустой handler без record claim.
		if (createdHandler > AIGroupMovementComponent.DEFAULT_HANDLER_ID && movement.GetFormationDefinition(createdHandler) &&
			movement.GetMoveHandlerAgentCount(createdHandler) == 0 && !m_VehicleMgr.FindVehicleBySubgroupId(createdHandler))
		{
			movement.RemoveGroupMoveHandler(createdHandler);
			Print(string.Format("[AICF][TANK_POSITIVE_HANDLER_PROBE] event=OWN_EMPTY_HANDLER_REMOVED handler=%1 physical_mutation=0", createdHandler));
		}
		return result;
	}
}
