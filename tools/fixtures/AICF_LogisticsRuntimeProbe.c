// Test-only preparation/observation. Копируется временно в Core/Economy.
// Не подменяет planner, spawn водителя/машины, движение, transfer или cleanup.
modded class SCR_GameModeCampaign
{
	protected int m_iAICFClientSpawnAttempts;
	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() && System.GetCLIParam("aicfLogisticsClientProbe", enabled) && enabled == "1")
		{
			Print("[AICF][LOGISTICS_PROBE_CLIENT_STARTED] test_only=1");
			GetGame().GetCallqueue().CallLater(AICF_LogisticsProbeClientClose, 300000, false);
			GetGame().GetCallqueue().CallLater(AICF_LogisticsProbeClientSpawn, 10000, false);
		}
		if (Replication.IsServer() && System.GetCLIParam("aicfLogisticsProbe", enabled) && enabled == "1")
		{
			string durationText;
			int duration = 300000;
			if (System.GetCLIParam("aicfLogisticsProbeDurationMs", durationText)) duration = durationText.ToInt();
			GetGame().GetCallqueue().CallLater(AICF_LogisticsProbeDeadline, duration + 120000, false);
		}
	}
	protected void AICF_LogisticsProbeDeadline()
	{
		Print("[AICF][LOGISTICS_PROBE_DEADLINE] test_only=1 reason=MATCH_OR_SERVICE_UPDATES_ENDED");
		GetGame().RequestClose();
	}
	protected void AICF_LogisticsProbeClientClose()
	{
		GetGame().GetCallqueue().Remove(AICF_LogisticsProbeClientSpawn);
		Print("[AICF][LOGISTICS_PROBE_CLIENT_FINISHED] test_only=1");
		GetGame().RequestClose();
	}
	// Не управляет logistics driver. Обычный local player запрашивает faction и
	// spawn через native RPC/respawn validation; streaming остаётся штатным.
	protected void AICF_LogisticsProbeClientSpawn()
	{
		string stable;
		if (Replication.IsServer() || !System.GetCLIParam("aicfLogisticsClientSpawnFaction", stable) || (stable != "US" && stable != "USSR")) return;
		PlayerController player = GetGame().GetPlayerController();
		if (player && player.GetControlledEntity())
		{
			Print(string.Format("[AICF][LOGISTICS_PROBE_CLIENT_PLAYER] test_only=1 entity=%1 faction=%2 position=%3", Replication.FindItemId(player.GetControlledEntity()), SCR_Faction.GetEntityFaction(player.GetControlledEntity()), player.GetControlledEntity().GetOrigin()));
			return;
		}
		if (++m_iAICFClientSpawnAttempts > 12)
		{
			Print("[AICF][LOGISTICS_PROBE_CLIENT_SPAWN_UNAVAILABLE] test_only=1 attempts=12");
			return;
		}
		GetGame().GetCallqueue().CallLater(AICF_LogisticsProbeClientSpawn, 10000, false);
		if (!player) return;
		AICF_ContentProfile profile = AICF_ContentProfile.GetActive();
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(profile.GetRuntimeFactionKey(stable)));
		if (!faction || !faction.GetMainBase()) return;
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation) return;
		if (affiliation.GetAffiliatedFaction() != faction)
		{
			affiliation.RequestFaction(faction);
			return;
		}
		SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		if (!catalog) return;
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		array<string> suffixes = {};
		string role;
		if (!profile.BuildCharacterRoleCandidates(stable, 9, role, suffixes)) return;
		ResourceName prefab;
		foreach (string suffix : suffixes)
		{
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				if (entry && entry.GetPrefab().EndsWith(suffix)) { prefab = entry.GetPrefab(); break; }
			}
			if (!prefab.IsEmpty()) break;
		}
		if (prefab.IsEmpty()) return;
		SCR_SpawnPoint chosen;
		float nearest = float.MAX;
		foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPointsForFaction(faction.GetFactionKey()))
		{
			if (!point || !point.IsSpawnPointActive()) continue;
			float distance = vector.DistanceXZ(point.GetOrigin(), faction.GetMainBase().GetOwner().GetOrigin());
			if (distance >= nearest) continue;
			nearest = distance;
			chosen = point;
		}
		SCR_RespawnComponent respawn = SCR_RespawnComponent.Cast(player.GetRespawnComponent());
		if (!respawn || !chosen) return;
		bool sent = respawn.RequestSpawn(new SCR_SpawnPointSpawnData(prefab, chosen.GetRplId()));
		Print(string.Format("[AICF][LOGISTICS_PROBE_CLIENT_SPAWN_REQUEST] test_only=1 faction=%1 prefab=%2 spawn_point=%3 sent=%4 native_validation=1", faction.GetFactionKey(), prefab, chosen.GetRplId(), sent));
	}
	void ~SCR_GameModeCampaign()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_LogisticsProbeClientClose);
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_LogisticsProbeClientSpawn);
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_LogisticsProbeDeadline);
	}
}

class AICF_LogisticsProbeFailureAdapter : AICF_LogisticsResourceAdapter
{
	int m_iWrites;
	bool m_bRejectCompensation;
	AICF_LogisticsResourcePool m_RejectedPool;
	override protected void WriteLeaf(AICF_LogisticsLeaf leaf, float value)
	{
		m_iWrites++;
		if (m_RejectedPool)
		{
			foreach (AICF_LogisticsLeaf rejected : m_RejectedPool.m_aLeaves)
			{
				if (rejected.m_Container == leaf.m_Container) return;
			}
		}
		if (m_bRejectCompensation && value > leaf.m_Container.GetResourceValue()) return;
		super.WriteLeaf(leaf, value);
	}
}

// Unit fixture: mock только precondition driver readiness, реальные production
// Reserve/Renew/Cancel/Stop и реальные физические pools. Не выдаётся за рейс.
class AICF_LogisticsProbeLedgerWorker : AICF_LogisticsWorker
{
	override bool Ready() { return true; }
	override void Log(string eventName, string details = "") {}
}

class AICF_LogisticsProbeGraph : AICF_ObjectiveGraph
{
	ref array<bool> m_aFixtureOwned = {};
	void Prepare(array<ref AICF_LogisticsEndpoint> endpoints)
	{
		for (int i; i < 6; i++)
		{
			m_aNodes.Insert(new AICF_ObjectiveNode(i, endpoints[i].m_Base, true));
			m_aFixtureOwned.Insert(true);
		}
		m_aNodes[0].AddOutgoingNodeId(1);
		m_aNodes[0].AddOutgoingNodeId(2);
		m_aNodes[1].AddOutgoingNodeId(2);
		m_aNodes[1].AddOutgoingNodeId(3);
		m_aNodes[2].AddOutgoingNodeId(0);
		m_aNodes[2].AddOutgoingNodeId(3);
		m_aNodes[3].AddOutgoingNodeId(4);
		m_aNodes[4].AddOutgoingNodeId(1);
	}
	protected override bool IsNodeOwnedByFaction(int nodeId, FactionKey factionKey)
	{
		return nodeId >= 0 && nodeId < m_aFixtureOwned.Count() && m_aFixtureOwned[nodeId];
	}
}

modded class SCR_ChimeraCharacter
{
	[RplProp(onRplName: "AICF_ProbeReplicaChanged")]
	protected int m_iAICFLogisticsProbeSlot;
	[RplProp()]
	protected int m_iAICFLogisticsProbeGeneration;
	protected int m_iAICFReplicaPolls;
	protected ref SCR_ResourceSystemSubscriptionHandleBase m_AICFReplicaSubscription;
	protected SCR_ResourceGenerator m_AICFReplicaGenerator;
	int AICF_ProbeDriverSlot() { return m_iAICFLogisticsProbeSlot; }

	void AICF_ProbeMarkDriver(int slot, int generation)
	{
		if (!Replication.IsServer() || (m_iAICFLogisticsProbeSlot == slot && m_iAICFLogisticsProbeGeneration == generation)) return;
		m_iAICFLogisticsProbeSlot = slot;
		m_iAICFLogisticsProbeGeneration = generation;
		Replication.BumpMe();
	}

	protected void AICF_ProbeReplicaChanged()
	{
		if (Replication.IsServer() || m_iAICFLogisticsProbeSlot <= 0) return;
		GetGame().GetCallqueue().Remove(AICF_ProbeReplica);
		AICF_ProbeReplica();
	}

	protected void AICF_ProbeReplica()
	{
		Vehicle vehicle = Vehicle.Cast(CompartmentAccessComponent.GetVehicleIn(this));
		float cargo, capacity;
		bool supplies = AICF_LogisticsResourceAdapter.VehicleCargo(vehicle, cargo, capacity);
		SCR_ResourceComponent resource = SCR_ResourceComponent.FindResourceComponent(vehicle);
		SCR_ResourceGenerator generator;
		if (resource) generator = resource.GetGenerator(EResourceGeneratorID.VEHICLE_LOAD, EResourceType.SUPPLIES);
		if (generator != m_AICFReplicaGenerator)
		{
			m_AICFReplicaSubscription = null;
			m_AICFReplicaGenerator = generator;
		}
		if (generator)
		{
			PlayerController player = GetGame().GetPlayerController();
			if (!m_AICFReplicaSubscription && player)
			{
				SCR_ResourcePlayerControllerInventoryComponent inventory = SCR_ResourcePlayerControllerInventoryComponent.Cast(player.FindComponent(SCR_ResourcePlayerControllerInventoryComponent));
				if (inventory) m_AICFReplicaSubscription = GetGame().GetResourceSystemSubscriptionManager().RequestSubscriptionListenerHandle(generator, Replication.FindItemId(inventory));
			}
			cargo = generator.GetAggregatedResourceValue();
			capacity = generator.GetAggregatedMaxResourceValue();
			supplies = capacity > 0 && cargo >= 0 && cargo <= capacity;
		}
		RplComponent vehicleRpl;
		string vehicleRplId = "NONE";
		if (vehicle) vehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (vehicleRpl) vehicleRplId = vehicleRpl.Id().ToString();
		CompartmentAccessComponent access = GetCompartmentAccessComponent();
		bool pilot = access && PilotCompartmentSlot.Cast(access.GetCompartment()) && access.GetCompartment().GetOccupant() == this;
		Print(string.Format("[AICF][LOGISTICS_PROBE_CLIENT] test_only=1 slot=%1 generation=%2 driver_rpl=%3 vehicle_rpl=%4 pilot=%5 supplies=%6 cargo=%7 capacity=%8 position=%9", m_iAICFLogisticsProbeSlot, m_iAICFLogisticsProbeGeneration, Replication.FindItemId(this), vehicleRplId, pilot, supplies, cargo, capacity, GetOrigin()));
		if (++m_iAICFReplicaPolls < 30) GetGame().GetCallqueue().CallLater(AICF_ProbeReplica, 10000, false);
		else m_AICFReplicaSubscription = null;
	}

	void ~SCR_ChimeraCharacter()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_ProbeReplica);
	}
}

modded class SCR_CharacterDamageManagerComponent
{
	protected int m_iAICFProbeDamageLogs;
	protected override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetOwner());
		if (!Replication.IsServer() || !character || character.AICF_ProbeDriverSlot() <= 0 || m_iAICFProbeDamageLogs++ >= 30) return;
		IEntity dealer;
		if (damageContext.instigator) dealer = damageContext.instigator.GetInstigatorEntity();
		Print(string.Format("[AICF][LOGISTICS_PROBE_DRIVER_DAMAGE] test_only=1 slot=%1 type=%2 value=%3 dealer=%4 source=%5 position=%6", character.AICF_ProbeDriverSlot(), typename.EnumToString(EDamageType, damageContext.damageType), damageContext.damageValue, dealer, damageContext.damageSource, character.GetOrigin()));
	}
}

modded class SCR_VehicleDamageManagerComponent
{
	protected int m_iAICFProbeVehicleDamageLogs;
	protected override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);
		if (!Replication.IsServer() || !m_AICFLogisticsCustody || m_iAICFProbeVehicleDamageLogs++ >= 30) return;
		IEntity dealer;
		if (damageContext.instigator) dealer = damageContext.instigator.GetInstigatorEntity();
		Print(string.Format("[AICF][LOGISTICS_PROBE_VEHICLE_DAMAGE] test_only=1 slot=%1 type=%2 value=%3 dealer=%4 source=%5 position=%6", m_AICFLogisticsCustody.m_iSlot, typename.EnumToString(EDamageType, damageContext.damageType), damageContext.damageValue, dealer, damageContext.damageSource, GetOwner().GetOrigin()));
	}
}

modded class AICF_LogisticsPlanner
{
	void AICF_ProbeSearch(AICF_LogisticsWorker live)
	{
		AICF_LogisticsWorker probe = new AICF_LogisticsWorker();
		probe.m_Depot = live.m_Depot;
		probe.m_Home = live.m_Home;
		probe.m_Faction = live.m_Faction;
		probe.m_fPrefabCapacity = live.m_fPrefabCapacity;
		PrepareSearch(probe, true, false);
		// Расширяем только test snapshot, чтобы бюджет проверялся также на
		// Arland с небольшим числом stock resource endpoints.
		while (probe.m_Search.m_aEndpoints.Count() <= AICF_LogisticsConfig.SEARCH_BUDGET)
			probe.m_Search.m_aEndpoints.Insert(m_aEndpoints[0]);
		BeginTick();
		bool first = PrepareCandidates(probe);
		bool bounded = m_iCandidatesVisited <= AICF_LogisticsConfig.SEARCH_BUDGET;
		int steps = 1;
		int size = probe.m_Search.m_aEndpoints.Count();
		int limit = size * (size + 1) + 1;
		while (!probe.m_Search.m_bPrepared && steps < limit)
		{
			BeginTick();
			PrepareCandidates(probe);
			if (m_iCandidatesVisited > AICF_LogisticsConfig.SEARCH_BUDGET) bounded = false;
			steps++;
		}
		int passed;
		if (bounded) passed++;
		if (!first && steps > 1) passed++;
		if (probe.m_Search.m_bPrepared && steps < limit) passed++;
		AICF_LogisticsEndpoint a = new AICF_LogisticsEndpoint();
		AICF_LogisticsEndpoint b = new AICF_LogisticsEndpoint();
		a.m_Pool = m_aEndpoints[0].m_Pool;
		b.m_Pool = a.m_Pool;
		a.m_iDepth = 0;
		b.m_iDepth = 1;
		if (Before(a, b) && !Before(b, a)) passed++;
		a.m_iDepth = 1;
		b.m_iDepth = 2;
		if (Before(a, b) && !Before(b, a)) passed++;
		b.m_iDepth = 1;
		a.m_iOpenedAtMs = 100;
		b.m_iOpenedAtMs = 200;
		if (Before(a, b) && !Before(b, a)) passed++;
		Print(string.Format("[AICF][LOGISTICS_PROBE_SEARCH_CONTRACT] test_only=1 passed=%1 total=6 steps=%2 nodes=%3 candidates=%4 production_prepare_and_priority=1", passed, steps, m_aEndpoints.Count(), probe.m_Search.m_aCandidates.Count()));
	}
}

// Только наблюдение: native decision и GetOut activity остаются без изменений.
modded class SCR_AIGroupUtilityComponent
{
	override void OnMoveFailed(int moveResult, IEntity vehicleUsed, bool isWaypointRelated, vector moveLocation)
	{
		SCR_ChimeraCharacter driver;
		if (m_Owner) driver = SCR_ChimeraCharacter.Cast(m_Owner.GetLeaderEntity());
		if (driver && driver.AICF_ProbeDriverSlot() > 0)
		{
			vector position = driver.GetOrigin();
			if (vehicleUsed) position = vehicleUsed.GetOrigin();
			Print(string.Format("[AICF][LOGISTICS_PROBE_MOVE_FAILED] test_only=1 slot=%1 result=%2 vehicle=%3 waypoint_related=%4 position=%5 target=%6", driver.AICF_ProbeDriverSlot(), typename.EnumToString(EMoveError, moveResult), vehicleUsed, isWaypointRelated, position, moveLocation));
		}
		super.OnMoveFailed(moveResult, vehicleUsed, isWaypointRelated, moveLocation);
	}
}

modded class AICF_LogisticsService
{
	protected int m_iAICFProbeStart;
	protected int m_iAICFProbeNext;
	protected bool m_bAICFProbePlaced;
	protected bool m_bAICFProbeStopped;
	protected bool m_bAICFProbeSupplies;
	protected bool m_bAICFHeavyPrepared;
	protected ref set<ResourceName> m_aAICFProbedPrefabs = new set<ResourceName>();
	protected ref array<SCR_CampaignBuildingCompositionComponent> m_aAICFProbeLayouts = {};
	protected Faction m_AICFProbeOwner;
	protected bool m_bAICFAdapterChecked;
	protected int m_iAICFWarmupNext;
	protected int m_iAICFDemandNext;
	protected int m_iAICFMeasuredCalls;
	protected int m_iAICFMeasuredTotalMs;
	protected int m_iAICFMeasuredMaxMs;
	protected bool m_bAICFSearchChecked;
	protected AICF_LogisticsWorker m_AICFReturnWorker;
	protected ref AICF_LogisticsEndpoint m_AICFReturnSource;
	protected int m_iAICFReturnState;
	protected int m_iAICFReturnStarted;
	protected int m_iAICFReturnNext;
	protected float m_fAICFReturnedBefore;
	protected ref array<ref AICF_ConstructionOrder> m_aAICFPreparations = {};
	protected ref AICF_ConstructionSiteSearch m_AICFPreparationSearch;

	override void Update(SCR_CampaignFaction us, SCR_CampaignFaction ussr, bool graphReady)
	{
		string enabled;
		if (!System.GetCLIParam("aicfLogisticsProbe", enabled) || enabled != "1")
		{
			super.Update(us, ussr, graphReady);
			return;
		}
		int now = System.GetTickCount();
		if (!m_iAICFProbeStart)
		{
			m_iAICFProbeStart = now;
			AICF_ProbePolicy();
			AICF_ProbeDriverInteractionClocks();
			string peaceful;
			if (System.GetCLIParam("aicfLogisticsProbePeace", peaceful) && peaceful == "1")
			{
				SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
				SCR_Faction fia = SCR_Faction.Cast(factions.GetFactionByKey("FIA"));
				if (fia)
				{
					factions.SetFactionsFriendly(us, fia);
					factions.SetFactionsFriendly(ussr, fia);
					Print("[AICF][LOGISTICS_PROBE_RELATIONS] test_only=1 FIA_US_friendly=1 FIA_USSR_friendly=1 driver_unmodified=1");
				}
			}
		}
		string durationText;
		int duration = 300000;
		if (System.GetCLIParam("aicfLogisticsProbeDurationMs", durationText)) duration = durationText.ToInt();
		if (now - m_iAICFProbeStart >= duration)
		{
			if (!m_bAICFProbeStopped)
			{
				m_bAICFProbeStopped = true;
				Stop();
				Stop();
				foreach (AICF_ConstructionOrder unfinished : m_aAICFPreparations) unfinished.m_Metadata.ReleasePreview();
				m_aAICFPreparations.Clear();
				Print("[AICF][LOGISTICS_PROBE_STOP] test_only=1 repeated=1");
			}
			if (now - m_iAICFProbeStart >= duration + 60000)
			{
				Print("[AICF][LOGISTICS_PROBE_FINISHED] test_only=1");
				GetGame().RequestClose();
			}
			return;
		}
		string sourceDepotText;
		int placeDelay = 10000;
		if (System.GetCLIParam("aicfLogisticsProbeDepotAtSource", sourceDepotText) && sourceDepotText == "1") placeDelay = 35000;
		if (graphReady && !m_bAICFProbePlaced && now - m_iAICFProbeStart >= placeDelay)
		{
			m_bAICFProbePlaced = true;
			string prepare;
			if (System.GetCLIParam("aicfLogisticsProbePrepare", prepare) && prepare == "1")
			{
				AICF_ProbePlace(us, AICF_EConstructionType.LIGHT_DEPOT, 0);
				AICF_ProbePlace(ussr, AICF_EConstructionType.LIGHT_DEPOT, 0);
			}
		}
		AICF_ProbeAdvancePreparation();
		foreach (SCR_CampaignBuildingCompositionComponent composition : m_aAICFProbeLayouts)
		{
			if (!composition || composition.IsCompositionSpawned()) continue;
			SCR_CampaignBuildingLayoutComponent layout = composition.GetCompositionLayout();
			if (layout) layout.AddBuildingValue(layout.GetToBuildValue());
		}
		int productionStarted = System.GetTickCount();
		super.Update(us, ussr, graphReady);
		int productionDuration = System.GetTickCount() - productionStarted;
		m_iAICFMeasuredCalls++;
		m_iAICFMeasuredTotalMs += productionDuration;
		m_iAICFMeasuredMaxMs = Math.Max(m_iAICFMeasuredMaxMs, productionDuration);
		AICF_ProbeReturnScenario(now);
		string supplyPreparation;
		if (!m_bAICFProbeSupplies && graphReady && now - m_iAICFProbeStart >= 25000 && System.GetCLIParam("aicfLogisticsProbePrepare", supplyPreparation) && supplyPreparation == "1")
		{
			m_bAICFProbeSupplies = true;
			AICF_ProbeSupply(us);
			AICF_ProbeSupply(ussr);
		}
		string repeatText;
		bool repeatSource = System.GetCLIParam("aicfLogisticsProbeRepeatSource", repeatText) && repeatText == "1";
		if (m_bAICFProbeSupplies && (repeatSource || now - m_iAICFProbeStart < 300000) && now >= m_iAICFWarmupNext)
		{
			m_iAICFWarmupNext = now + 5000;
			if (repeatSource) m_iAICFWarmupNext = now + 60000;
			bool usLoaded, ussrLoaded;
			foreach (AICF_LogisticsWorker warmupWorker : m_Registry.m_aWorkers)
			{
				if (warmupWorker.m_fLoaded > 0 && warmupWorker.m_Faction == us) usLoaded = true;
				if (warmupWorker.m_fLoaded > 0 && warmupWorker.m_Faction == ussr) ussrLoaded = true;
			}
			if (repeatSource || !usLoaded) AICF_ProbeSupply(us);
			if (repeatSource || !ussrLoaded) AICF_ProbeSupply(ussr);
		}
		if (m_bAICFProbeSupplies && !m_bAICFHeavyPrepared && now - m_iAICFProbeStart >= 45000)
		{
			m_bAICFHeavyPrepared = true;
			AICF_ProbePlace(us, AICF_EConstructionType.HEAVY_DEPOT, 1);
			AICF_ProbePlace(ussr, AICF_EConstructionType.HEAVY_DEPOT, 1);
		}
		string maintain;
		if (m_bAICFProbeSupplies && now >= m_iAICFDemandNext && System.GetCLIParam("aicfLogisticsProbeMaintainDemand", maintain) && maintain == "1")
		{
			m_iAICFDemandNext = now + 30000;
			AICF_ProbeDemand(us);
			AICF_ProbeDemand(ussr);
		}
		if (now < m_iAICFProbeNext) return;
		m_iAICFProbeNext = now + 10000;
		Print(string.Format("[AICF][LOGISTICS_PROBE_COST] test_only=1 calls=%1 total_ms=%2 max_ms=%3 workers=%4 nodes=%5", m_iAICFMeasuredCalls, m_iAICFMeasuredTotalMs, m_iAICFMeasuredMaxMs, m_Registry.m_aWorkers.Count(), m_Graph.GetNodeCount()));
		AICF_ProbePools(us, ussr);
		foreach (AICF_LogisticsWorker worker : m_Registry.m_aWorkers)
		{
			SCR_ChimeraCharacter marked = SCR_ChimeraCharacter.Cast(worker.m_Driver);
			if (marked && worker.Ready()) marked.AICF_ProbeMarkDriver(worker.m_iSlot, worker.m_iGeneration);
			vector position;
			if (worker.m_Vehicle) position = worker.m_Vehicle.GetOrigin();
			worker.Log("LOGISTICS_PROBE_WORKER", string.Format("test_only=1 position=%1 destination_position=%2 eligible=%3 ready=%4", position, worker.m_vEndpoint, worker.m_bEligible, worker.Ready()));
			if (worker.Ready())
			{
				if (!m_bAICFSearchChecked)
				{
					m_bAICFSearchChecked = true;
					m_Planner.AICF_ProbeSearch(worker);
				}
				SCR_ResourceComponent cargoResource = SCR_ResourceComponent.FindResourceComponent(worker.m_Vehicle);
				AICF_ProbeOperation("CARGO_LOAD", cargoResource.GetGenerator(EResourceGeneratorID.VEHICLE_LOAD, EResourceType.SUPPLIES), worker.m_CargoPool);
				AICF_ProbeOperation("CARGO_UNLOAD", cargoResource.GetConsumer(EResourceGeneratorID.VEHICLE_UNLOAD, EResourceType.SUPPLIES), worker.m_CargoPool);
				if (worker.m_Job)
				{
					AICF_LogisticsEndpoint target = worker.m_Job.m_Destination;
					AICF_ProbeOperation("DESTINATION_GENERATOR", target.m_Resource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES), target.m_Pool);
				}
			}
		}
	}

	protected void AICF_ProbeDemand(SCR_CampaignFaction faction)
	{
		if (m_iAICFReturnState == 1 && m_AICFReturnWorker.m_Faction == faction) return;
		AICF_LogisticsEndpoint home = m_Planner.Find(faction.GetMainBase());
		if (!home || !home.IdentityValid() || !home.m_Pool.Valid()) return;
		float before = home.m_Pool.Value();
		foreach (AICF_LogisticsLeaf leaf : home.m_Pool.m_aLeaves)
		{
			leaf.m_Container.SetResourceValue(0);
			leaf.m_Container.GetComponent().Replicate();
		}
		Print(string.Format("[AICF][LOGISTICS_PROBE_CONSUMPTION] test_only=1 base=%1 before=%2 after=%3 consumed=%4", home.Key(), before, home.m_Pool.Value(), before - home.m_Pool.Value()));
	}

	protected void AICF_ProbeSupply(SCR_CampaignFaction faction)
	{
		if (m_iAICFReturnState == 1 && m_AICFReturnWorker.m_Faction == faction) return;
		AICF_LogisticsEndpoint home = m_Planner.Find(faction.GetMainBase());
		if (!home) return;
		AICF_LogisticsEndpoint source;
		float best = float.MAX;
		foreach (AICF_LogisticsEndpoint e : m_Planner.m_aEndpoints)
		{
			if (e.m_Base.GetType() != SCR_ECampaignBaseType.BASE || e.m_Base == faction.GetMainBase()) continue;
			SCR_CampaignFaction owner = SCR_CampaignFaction.Cast(e.m_Owner);
			if (owner && owner.GetMainBase() == e.m_Base) continue;
			SCR_AIWorld routeWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			vector roadSource, roadHome;
			if (!routeWorld || !routeWorld.GetRoadNetworkManager().GetReachableWaypointInRoad(faction.GetMainBase().GetOwner().GetOrigin(), e.m_Base.GetOwner().GetOrigin(), e.m_Base.GetRadius() - 5, roadSource)) continue;
			if (!routeWorld.GetRoadNetworkManager().GetReachableWaypointInRoad(roadSource, faction.GetMainBase().GetOwner().GetOrigin(), faction.GetMainBase().GetRadius() - 5, roadHome)) continue;
			float distance = vector.DistanceXZ(e.m_Base.GetOwner().GetOrigin(), faction.GetMainBase().GetOwner().GetOrigin());
			if (distance >= best) continue;
			best = distance;
			source = e;
		}
		if (!source) return;
		source.m_Base.SetFaction(faction);
		m_AICFProbeOwner = faction;
		GetGame().GetWorld().QueryEntitiesBySphere(source.m_Base.GetOwner().GetOrigin(), source.m_Base.GetRadius() + 100, AICF_ProbePrepareGuard, null, EQueryEntitiesFlags.DYNAMIC);
		foreach (AICF_LogisticsLeaf leaf : source.m_Pool.m_aLeaves)
		{
			leaf.m_Container.SetResourceValue(leaf.m_Container.GetMaxResourceValue());
			leaf.m_Container.GetComponent().Replicate();
		}
		foreach (AICF_LogisticsLeaf drain : home.m_Pool.m_aLeaves)
		{
			drain.m_Container.SetResourceValue(0);
			drain.m_Container.GetComponent().Replicate();
		}
		Print(string.Format("[AICF][LOGISTICS_PROBE_SUPPLY_SETUP] test_only=1 source=%1 home=%2 source_amount=%3 home_amount=%4 owner_changed=1", source.Key(), home.Key(), source.m_Pool.Value(), home.m_Pool.Value()));
		if (!m_bAICFAdapterChecked)
		{
			m_bAICFAdapterChecked = true;
			AICF_ProbeAdapter(source, home);
		}
	}

	// После успешной доставки создаём физически полный destination следующего
	// рейса. Production должен сохранить cargo и сам выбрать/проехать RETURN.
	protected void AICF_ProbeReturnScenario(int now)
	{
		string enabled;
		if (!System.GetCLIParam("aicfLogisticsProbeReturn", enabled) || enabled != "1" || m_iAICFReturnState == 2) return;
		if (m_iAICFReturnState == 0)
		{
			foreach (AICF_LogisticsWorker worker : m_Registry.m_aWorkers)
			{
				if (!worker.Ready() || worker.m_fDelivered <= 0 || worker.m_fObservedCargo <= 0 || worker.m_aCargo.IsEmpty()) continue;
				AICF_LogisticsEndpoint source = worker.m_aCargo[0].m_Source;
				if (!source || !source.IdentityValid() || source.m_Owner != worker.m_Faction) continue;
				m_AICFReturnWorker = worker;
				m_AICFReturnSource = source;
				m_fAICFReturnedBefore = worker.m_fReturned;
				m_iAICFReturnStarted = now;
				m_iAICFReturnState = 1;
				worker.Log("LOGISTICS_PROBE_RETURN_SETUP", "test_only=1 destinations_filled=1 original_source_space_maintained=1 production_job_unchanged=1");
				break;
			}
		}
		if (m_iAICFReturnState != 1) return;
		if (m_AICFReturnWorker.m_fReturned > m_fAICFReturnedBefore)
		{
			m_iAICFReturnState = 2;
			m_AICFReturnWorker.Log("LOGISTICS_PROBE_RETURN_FINISHED", string.Format("test_only=1 passed=1 amount=%1", m_AICFReturnWorker.m_fReturned - m_fAICFReturnedBefore));
			return;
		}
		if (!m_AICFReturnSource.IdentityValid() || !m_AICFReturnWorker.m_bCustody || now - m_iAICFReturnStarted >= 600000)
		{
			m_iAICFReturnState = 2;
			m_AICFReturnWorker.Log("LOGISTICS_PROBE_RETURN_FINISHED", "test_only=1 passed=0 reason=SOURCE_OR_CUSTODY_LOST_OR_DEADLINE");
			return;
		}
		if (now < m_iAICFReturnNext) return;
		m_iAICFReturnNext = now + 30000;
		foreach (AICF_LogisticsEndpoint endpoint : m_Planner.m_aEndpoints)
		{
			if (!endpoint.IdentityValid() || endpoint.m_Owner != m_AICFReturnWorker.m_Faction) continue;
			float before = endpoint.m_Pool.Value();
			float target = endpoint.m_Pool.Capacity();
			if (endpoint.m_Pool.Overlaps(m_AICFReturnSource.m_Pool)) target = Math.Max(0, target - m_AICFReturnWorker.m_fObservedCargo - 100);
			float remaining = target;
			foreach (AICF_LogisticsLeaf leaf : endpoint.m_Pool.m_aLeaves)
			{
				float value = Math.Min(remaining, leaf.m_Container.GetMaxResourceValue());
				leaf.m_Container.SetResourceValue(value);
				leaf.m_Container.GetComponent().Replicate();
				remaining -= value;
			}
			Print(string.Format("[AICF][LOGISTICS_PROBE_RETURN_STORAGE] test_only=1 base=%1 before=%2 after=%3 delta=%4", endpoint.Key(), before, endpoint.m_Pool.Value(), endpoint.m_Pool.Value() - before));
		}
	}

	protected void AICF_ProbeAdapter(AICF_LogisticsEndpoint source, AICF_LogisticsEndpoint destination)
	{
		// Настоящие отдельные stock pools, synchronous production adapter.
		// Снимок fixture восстанавливается до следующего logistics scheduler tick.
		AICF_LogisticsResourceAdapter adapter = new AICF_LogisticsResourceAdapter();
		AICF_LogisticsResourcePool from = adapter.Resolve(source.m_Resource);
		AICF_LogisticsResourcePool to = adapter.Resolve(destination.m_Resource);
		if (!from || !to || from.Overlaps(to)) return;
		AICF_ProbeOperation("BASE_CONSUMER", destination.m_Resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES), to);
		AICF_ProbeOperation("BASE_GENERATOR", destination.m_Resource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES), to);
		array<float> sourceValues = {};
		array<float> destinationValues = {};
		foreach (AICF_LogisticsLeaf s : from.m_aLeaves) sourceValues.Insert(s.m_Container.GetResourceValue());
		foreach (AICF_LogisticsLeaf d : to.m_aLeaves) destinationValues.Insert(d.m_Container.GetResourceValue());
		int passed;
		float original = from.Value();
		AICF_LogisticsReceipt first = adapter.Transfer("fixture-fraction", "TEST_ONLY", from, to, 0.125);
		if (first && first.m_bCommitted && first.m_fAmount == 0.125 && original - from.Value() == 0.125) passed++;
		AICF_LogisticsReceipt replay = adapter.Transfer("fixture-fraction", "TEST_ONLY", from, to, 0.125);
		if (replay == first && original - from.Value() == 0.125) passed++;
		AICF_LogisticsReceipt reverse = adapter.Transfer("fixture-restore", "TEST_ONLY", to, from, 0.125);
		if (reverse && reverse.m_bCommitted && reverse.m_fAmount == 0.125 && from.Value() == original) passed++;
		if (adapter.Resolve(source.m_Resource) == from && !adapter.Transfer("fixture-alias", "TEST_ONLY", from, from, 1)) passed++;
		AICF_LogisticsProbeFailureAdapter failure = new AICF_LogisticsProbeFailureAdapter();
		failure.m_RejectedPool = to;
		AICF_LogisticsReceipt compensated = failure.Transfer("fixture-credit-denied", "TEST_ONLY", from, to, 0.125);
		if (compensated && compensated.m_bCommitted && compensated.m_fAmount == 0 && from.Value() == original && compensated.m_fDiscrepancy == 0) passed++;
		failure = new AICF_LogisticsProbeFailureAdapter();
		failure.m_RejectedPool = to;
		failure.m_bRejectCompensation = true;
		AICF_LogisticsReceipt pending = failure.Transfer("fixture-compensation-denied", "TEST_ONLY", from, to, 0.125);
		if (pending && !pending.m_bCommitted && pending.m_fAmount == 0 && pending.m_fDiscrepancy == 0.125 && original - from.Value() == 0.125) passed++;
		adapter.Stop();
		if (!adapter.Transfer("fixture-after-stop", "TEST_ONLY", from, to, 1)) passed++;
		foreach (int si, AICF_LogisticsLeaf restoreS : from.m_aLeaves) restoreS.m_Container.SetResourceValue(sourceValues[si]);
		foreach (int di, AICF_LogisticsLeaf restoreD : to.m_aLeaves) restoreD.m_Container.SetResourceValue(destinationValues[di]);
		Print(string.Format("[AICF][LOGISTICS_PROBE_ADAPTER_CONTRACT] test_only=1 passed=%1 total=7 actual_stock_pools=1 snapshot_restored=1", passed));
		if (passed != 7) Print("[AICF][ERROR][LOGISTICS_PROBE_ADAPTER_FAILED]");
		AICF_ProbeLedger(source, destination);
	}

	protected void AICF_ProbeLedger(AICF_LogisticsEndpoint source, AICF_LogisticsEndpoint destination)
	{
		AICF_LogisticsEndpoint from = new AICF_LogisticsEndpoint();
		from.m_Base = source.m_Base;
		from.m_BaseId = source.m_BaseId;
		from.m_Owner = source.m_Base.GetFaction();
		from.m_Resource = source.m_Resource;
		from.m_Pool = source.m_Pool;
		AICF_LogisticsEndpoint to = new AICF_LogisticsEndpoint();
		to.m_Base = destination.m_Base;
		to.m_BaseId = destination.m_BaseId;
		to.m_Owner = destination.m_Base.GetFaction();
		to.m_Resource = destination.m_Resource;
		to.m_Pool = destination.m_Pool;
		float stock = from.m_Pool.Value();
		float room = to.m_Pool.Capacity() - to.m_Pool.Value();
		float amount = Math.Min(stock, room) * 0.5;
		if (amount <= 0) return;
		AICF_LogisticsLedger ledger = new AICF_LogisticsLedger();
		AICF_LogisticsConfig config = new AICF_LogisticsConfig(false);
		AICF_LogisticsProbeLedgerWorker first = new AICF_LogisticsProbeLedgerWorker();
		first.m_iSlot = 9000001;
		first.m_iGeneration = 1;
		first.m_iProgressAtMs = System.GetTickCount();
		AICF_LogisticsProbeLedgerWorker second = new AICF_LogisticsProbeLedgerWorker();
		second.m_iSlot = 9000002;
		second.m_iGeneration = 1;
		second.m_iProgressAtMs = System.GetTickCount();
		int passed;
		if (!ledger.Reserve(first, from, to, amount, false, config, 7))
		{
			Print("[AICF][ERROR][LOGISTICS_PROBE_LEDGER_FAILED] reason=FIRST_RESERVE");
			return;
		}
		passed++;
		string token = first.m_Job.m_sToken;
		if (from.m_Pool.Value() == stock && to.m_Pool.Value() == destination.m_Pool.Value()) passed++;
		if (!ledger.Reserve(first, from, to, 1, false, config, 7)) passed++;
		if (ledger.Reserved(from.m_Pool, false) == amount && ledger.Reserved(to.m_Pool, true) == amount) passed++;
		if (!ledger.Reserve(second, from, to, stock + 1, false, config, 7)) passed++;
		if (!ledger.Reserve(second, null, to, room + 1, true, config, 7)) passed++;
		if (!ledger.Reserve(second, null, to, amount, true, config, 7))
		{
			Print("[AICF][ERROR][LOGISTICS_PROBE_LEDGER_FAILED] reason=COMPETING_RETURN");
			return;
		}
		passed++;
		if (ledger.Reserved(to.m_Pool, true) == amount * 2 && ledger.Reserved(from.m_Pool, false) == amount) passed++;
		AICF_LogisticsResourcePool alias = new AICF_LogisticsResourcePool();
		foreach (AICF_LogisticsLeaf leaf : to.m_Pool.m_aLeaves) alias.Add(leaf.m_Container);
		if (ledger.Reserved(alias, true) == amount * 2) passed++;
		ledger.Cancel(first);
		ledger.Cancel(first);
		if (ledger.Reserved(from.m_Pool, false) == 0 && ledger.Reserved(to.m_Pool, true) == amount) passed++;
		second.m_Job.m_iExpiresAtMs = System.GetTickCount() - 1;
		if (ledger.Reserved(to.m_Pool, true) == 0 && !ledger.Renew(second, config, System.GetTickCount())) passed++;
		second.m_Job.m_iExpiresAtMs = System.GetTickCount() + config.m_iReservationTtlMs;
		second.m_iGeneration++;
		if (!ledger.Renew(second, config, System.GetTickCount())) passed++;
		ledger.Cancel(second);
		if (ledger.Reserve(first, from, to, amount, false, config, 8) && first.m_Job.m_sToken != token) passed++;
		if (!first.m_Job)
		{
			Print("[AICF][ERROR][LOGISTICS_PROBE_LEDGER_FAILED] reason=NEW_TOKEN");
			return;
		}
		if (ledger.Renew(first, config, System.GetTickCount())) passed++;
		first.m_iProgressAtMs = System.GetTickCount() - AICF_LogisticsConfig.PROGRESS_TIMEOUT_MS - 1;
		if (!ledger.Renew(first, config, System.GetTickCount())) passed++;
		ledger.Stop();
		ledger.Stop();
		if (ledger.m_aJobs.IsEmpty() && ledger.Reserved(to.m_Pool, true) == 0 && first.m_Job.m_bCancelled) passed++;
		if (!ledger.Reserve(second, from, to, amount, false, config, 8)) passed++;
		if (from.m_Pool.Value() == stock) passed++;
		Print(string.Format("[AICF][LOGISTICS_PROBE_LEDGER_CONTRACT] test_only=1 passed=%1 total=18 readiness_mock=1 production_ledger=1 physical_pools=1", passed));
		if (passed != 18) Print("[AICF][ERROR][LOGISTICS_PROBE_LEDGER_FAILED]");
		AICF_ProbeGraph();
	}

	protected void AICF_ProbeGraph()
	{
		if (m_Planner.m_aEndpoints.Count() < 6) return;
		AICF_LogisticsProbeGraph graph = new AICF_LogisticsProbeGraph();
		graph.Prepare(m_Planner.m_aEndpoints);
		SCR_CampaignMilitaryBaseComponent start = graph.GetNode(0).GetBase();
		SCR_CampaignMilitaryBaseComponent target = graph.GetNode(3).GetBase();
		array<int> path = {};
		int passed;
		if (graph.GetFriendlyHopDistance(start, start, "US") == 0) passed++;
		if (graph.GetFriendlyHopDistance(start, graph.GetNode(1).GetBase(), "US") == 1) passed++;
		if (graph.GetFriendlyHopDistance(start, target, "US") == 2) passed++;
		if (graph.GetFriendlyHopDistance(start, graph.GetNode(4).GetBase(), "US") == 3) passed++;
		if (graph.FindFriendlyPath(start, target, "US", path) && path.Count() == 3 && path[1] == 1) passed++;
		if (graph.GetFriendlyHopDistance(target, start, "US") == 4) passed++;
		if (graph.GetFriendlyHopDistance(start, graph.GetNode(5).GetBase(), "US") == -1) passed++;
		graph.m_aFixtureOwned[1] = false;
		if (graph.FindFriendlyPath(start, target, "US", path) && path.Count() == 3 && path[1] == 2) passed++;
		graph.m_aFixtureOwned[2] = false;
		if (!graph.FindFriendlyPath(start, target, "US", path)) passed++;
		graph.m_aFixtureOwned[1] = true;
		if (graph.GetFriendlyHopDistance(start, target, "US") == 2) passed++;
		graph.m_aFixtureOwned[3] = false;
		if (!graph.FindFriendlyPath(start, target, "US", path)) passed++;
		if (!graph.FindFriendlyPath(start, start, "", path)) passed++;
		Print(string.Format("[AICF][LOGISTICS_PROBE_GRAPH_CONTRACT] test_only=1 passed=%1 total=12 ownership_mock=1 production_bfs=1 directed_cycle_multiple_parents=1", passed));
		if (passed != 12) Print("[AICF][ERROR][LOGISTICS_PROBE_GRAPH_FAILED]");
	}

	protected bool AICF_ProbePrepareGuard(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().IsPlayerControlled()) return true;
		Faction owner = SCR_Faction.GetEntityFaction(character);
		if (!owner || owner.GetFactionKey() != "FIA") return true;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (affiliation) affiliation.SetAffiliatedFaction(m_AICFProbeOwner);
		Print(string.Format("[AICF][LOGISTICS_PROBE_GUARD_OWNER] test_only=1 entity=%1 previous=FIA owner=%2", entity.GetID(), m_AICFProbeOwner.GetFactionKey()));
		return true;
	}

	protected void AICF_ProbeOperation(string label, SCR_ResourceInteractor operation, AICF_LogisticsResourcePool expected)
	{
		if (!operation || !expected)
		{
			Print("[AICF][LOGISTICS_PROBE_OPERATION] test_only=1 label=" + label + " missing=1");
			return;
		}
		GetGame().GetResourceGrid().UpdateInteractor(operation);
		AICF_LogisticsResourcePool actual = new AICF_LogisticsResourcePool();
		bool allowed = true;
		SCR_ResourceContainerQueueBase queue = operation.GetContainerQueue();
		if (queue && operation.GetContainerCount() <= 128)
		{
			for (int i; i < operation.GetContainerCount(); i++)
			{
				SCR_ResourceContainer container = queue.GetContainerAt(i);
				if (!container) { allowed = false; continue; }
				if (!operation.CanInteractWith(container)) allowed = false;
				actual.Add(container);
				Print(string.Format("[AICF][LOGISTICS_PROBE_OPERATION_CONTAINER] test_only=1 label=%1 owner=%2 allowed=%3 value=%4 capacity=%5 rights=%6", label, container.GetOwner(), operation.CanInteractWith(container), container.GetResourceValue(), container.GetMaxResourceValue(), container.GetResourceRight()));
			}
		}
		float multiplier = -1;
		SCR_ResourceConsumer consumer = SCR_ResourceConsumer.Cast(operation);
		SCR_ResourceGenerator generator = SCR_ResourceGenerator.Cast(operation);
		if (consumer) multiplier = consumer.GetBuyMultiplier();
		if (generator) multiplier = generator.GetResourceMultiplier();
		Print(string.Format("[AICF][LOGISTICS_PROBE_OPERATION] test_only=1 label=%1 same_pool=%2 allowed=%3 expected_leaves=%4 actual_leaves=%5 queue=%6 actual=%7 capacity=%8 multiplier=%9", label, expected.Same(actual), allowed, expected.m_aLeaves.Count(), actual.m_aLeaves.Count(), operation.GetContainerCount(), actual.Value(), actual.Capacity(), multiplier));
	}

	protected void AICF_ProbePools(SCR_CampaignFaction us, SCR_CampaignFaction ussr)
	{
		foreach (AICF_LogisticsEndpoint e : m_Planner.m_aEndpoints)
		{
			SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			SCR_CampaignFaction ownerFaction = SCR_CampaignFaction.Cast(e.m_Owner);
			if (world && ownerFaction && ownerFaction.GetMainBase())
			{
				vector road;
				float radius = e.m_Base.GetRadius() - 5;
				bool reachable = world.GetRoadNetworkManager().GetReachableWaypointInRoad(ownerFaction.GetMainBase().GetOwner().GetOrigin(), e.m_Base.GetOwner().GetOrigin(), radius, road);
				Print(string.Format("[AICF][LOGISTICS_PROBE_ROAD] base=%1 from=%2 goal=%3 radius=%4 reachable=%5 result=%6 distance_goal=%7", e.Key(), ownerFaction.GetMainBase().GetOwner().GetOrigin(), e.m_Base.GetOwner().GetOrigin(), radius, reachable, road, vector.DistanceXZ(road, e.m_Base.GetOwner().GetOrigin())));
			}
			Print(string.Format("[AICF][LOGISTICS_PROBE_POOL] test_only=1 base=%1 owner=%2 type=%3 pool=%4 actual=%5 capacity=%6 depth=%7 neutral_us=%8 neutral_ussr=%9",
				e.Key(), e.m_Owner, e.m_Base.GetType(), e.m_Pool.m_sKey, e.m_Pool.Value(), e.m_Pool.Capacity(), e.m_iDepth,
				AICF_LogisticsPlanner.NeutralSource(e, us), AICF_LogisticsPlanner.NeutralSource(e, ussr)));
		}
		foreach (SCR_CatalogEntitySpawnerComponent production : SCR_CatalogEntitySpawnerComponent.INSTANCES)
		{
			if (!production || !production.GetOwner()) continue;
			if (production.GetType() != SCR_EServicePointType.LIGHT_VEHICLE_DEPOT && production.GetType() != SCR_EServicePointType.HEAVY_VEHICLE_DEPOT) continue;
			IEntity root = AICF_LogisticsDepotRegistry.BuildingRoot(production.GetOwner());
			Print(string.Format("[AICF][LOGISTICS_PROBE_DEPOT] test_only=1 root=%1 component_owner=%2 tier=%3 faction=%4 state=%5", root.GetID(), production.GetOwner().GetID(), production.GetType(), production.GetFaction(), production.GetServiceState()));
			for (int i; i < 256; i++)
			{
				SCR_EntityCatalogEntry entry = production.GetEntryAtIndex(i);
				if (!entry) break;
				float capacity = AICF_LogisticsDepotRegistry.PrefabCapacity(entry.GetPrefab());
				if (!m_aAICFProbedPrefabs.Contains(entry.GetPrefab()))
				{
					m_aAICFProbedPrefabs.Insert(entry.GetPrefab());
					Print(string.Format("[AICF][LOGISTICS_PROBE_ENTRY] test_only=1 root=%1 prefab=%2 capacity=%3", root.GetID(), entry.GetPrefab(), capacity));

				}
				if (capacity <= 0) continue;
				SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
				SCR_EntitySpawnerSlotComponent slot;
				if (data) slot = production.AICF_LogisticsFreeSlot(entry);
				Print(string.Format("[AICF][LOGISTICS_PROBE_CATALOG] test_only=1 root=%1 prefab=%2 capacity=%3 allowed_slot=%4 compatible_slot=%5", root.GetID(), entry.GetPrefab(), capacity, production.AICF_AllowsLogisticsEntry(entry, slot), production.AICF_LogisticsSupports(entry)));
			}
		}
	}

	protected void AICF_ProbeContainer(BaseContainer container, int depth)
	{
		if (!container || depth > 3) return;
		for (int i; i < container.GetNumVars(); i++)
		{
			string name = container.GetVarName(i);
			DataVarType type = container.GetDataVarType(i);
			if (type == DataVarType.OBJECT_ARRAY)
			{
				BaseContainerList list = container.GetObjectArray(name);
				if (list)
				{
					Print(string.Format("[AICF][LOGISTICS_PROBE_METADATA] class=%1 field=%2 count=%3 depth=%4", container.GetClassName(), name, list.Count(), depth));
					for (int n; n < list.Count(); n++) AICF_ProbeContainer(list.Get(n), depth + 1);
				}
			}
			else if (type == DataVarType.OBJECT) AICF_ProbeContainer(container.GetObject(name), depth + 1);
			else
			{
				string value;
				if (type == DataVarType.STRING || type == DataVarType.RESOURCE_NAME) container.Get(name, value);
				Print(string.Format("[AICF][LOGISTICS_PROBE_METADATA] class=%1 field=%2 type=%3 value=%4 depth=%5", container.GetClassName(), name, type, value, depth));
			}
		}
	}

	protected void AICF_ProbePlace(SCR_CampaignFaction faction, AICF_EConstructionType type, int ordinal, bool forceHQ = false)
	{
		if (!faction || !faction.GetMainBase()) return;
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		string sourceDepot;
		if (!forceHQ && (type == AICF_EConstructionType.HEAVY_DEPOT || (System.GetCLIParam("aicfLogisticsProbeDepotAtSource", sourceDepot) && sourceDepot == "1")))
		{
			foreach (AICF_LogisticsEndpoint endpoint : m_Planner.m_aEndpoints)
			{
				if (endpoint.m_Base != base && endpoint.m_Base.GetFaction() == faction && endpoint.m_Base.GetMasterProvider())
				{
					base = endpoint.m_Base;
					break;
				}
			}
		}
		SCR_CampaignBuildingProviderComponent provider = base.GetMasterProvider();
		SCR_CampaignBuildingManagerComponent manager = SCR_CampaignBuildingManagerComponent.Cast(m_Campaign.FindComponent(SCR_CampaignBuildingManagerComponent));
		if (!provider || !manager) return;
		ResourceName prefab = AICF_ContentProfile.GetActive().GetConstructionPrefab(AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey()), type);
		if (prefab.IsEmpty()) return;
		AICF_ConstructionOrder order = new AICF_ConstructionOrder();
		order.m_sToken = string.Format("logistics-fixture-%1-%2", faction.GetFactionKey(), ordinal);
		order.m_Faction = faction;
		order.m_sFaction = faction.GetFactionKey();
		order.m_Base = base;
		order.m_BaseId = base.GetOwner().GetID();
		order.m_Provider = provider;
		order.m_ProviderId = provider.GetOwner().GetID();
		order.m_vProviderPosition = provider.GetOwner().GetOrigin();
		order.m_eType = type;
		order.m_iStartedAt = System.GetTickCount();
		order.m_iSearchOffset = ordinal * 128;
		order.m_Metadata = new AICF_ConstructionMetadata();
		order.m_Metadata.Load(prefab, type, manager, faction);
		m_aAICFPreparations.Insert(order);
	}

	protected void AICF_ProbeAdvancePreparation()
	{
		if (!m_AICFPreparationSearch) m_AICFPreparationSearch = new AICF_ConstructionSiteSearch(new AICF_ConstructionConfig());
		for (int i = m_aAICFPreparations.Count() - 1; i >= 0; i--)
		{
			AICF_ConstructionOrder order = m_aAICFPreparations[i];
			int geometry = order.m_Metadata.StepGeometry(24, 8);
			if (geometry < 0 || order.m_iAttempts >= 512)
			{
				Print(string.Format("[AICF][LOGISTICS_PROBE_PREPARE_UNAVAILABLE] test_only=1 faction=%1 tier=%2 reason=%3 attempts=%4", order.m_sFaction, order.m_eType, order.m_sReason, order.m_iAttempts));
				order.m_Metadata.ReleasePreview();
				m_aAICFPreparations.Remove(i);
				if (order.m_eType == AICF_EConstructionType.LIGHT_DEPOT && order.m_Base != order.m_Faction.GetMainBase())
				{
					Print(string.Format("[AICF][LOGISTICS_PROBE_PREPARE_FALLBACK] test_only=1 faction=%1 destination=HQ source_search_exhausted=1", order.m_sFaction));
					AICF_ProbePlace(order.m_Faction, order.m_eType, 0, true);
				}
				continue;
			}
			if (geometry == 0) continue;
			if (order.m_iStage == 0 && !m_AICFPreparationSearch.BeginCandidate(order)) continue;
			int terrainAndExits = m_AICFPreparationSearch.Step(order, null);
			if (terrainAndExits == 0) continue;
			// Fixture не создаёт строителя: terrain и оба stock slot exits проверены,
			// worker foot-path stage ей не нужен. Vehicle admission остаётся production.
			if (order.m_iStage != 3 || order.m_sReason != "NAVMESH_UNAVAILABLE")
			{
				order.m_iStage = 0;
				continue;
			}
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			for (int axis; axis < 4; axis++) params.Transform[axis] = order.m_aTransform[axis];
			SCR_EditorLinkComponent.IgnoreSpawning(true);
			IEntity entity = GetGame().SpawnEntityPrefabEx(order.m_Metadata.m_sPrefab, false, params: params);
			SCR_EditorLinkComponent.IgnoreSpawning(false);
			if (!entity) continue;
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
			if (affiliation) affiliation.SetAffiliatedFaction(order.m_Faction);
			SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (composition)
			{
				composition.SetProviderEntity(order.m_Provider.GetOwner());
				m_aAICFProbeLayouts.Insert(composition);
			}
			Print(string.Format("[AICF][LOGISTICS_PROBE_PREPARE] test_only=1 prefab=%1 root=%2 faction=%3 position=%4 attempts=%5 bypass=placement_cost_construction_time logistics_geometry_gate_unchanged=1", order.m_Metadata.m_sPrefab, entity.GetID(), order.m_sFaction, entity.GetOrigin(), order.m_iAttempts));
			m_aAICFPreparations.Remove(i);
		}
	}
	// Production clock methods с контролируемым временем; не заменяет physical gate run.
	protected void AICF_ProbeDriverInteractionClocks()
	{
		int passed;
		AICF_LogisticsWorker worker = new AICF_LogisticsWorker();
		worker.m_iPhaseAtMs = 1000;
		worker.MarkProgress(1000);
		if (worker.ProgressAgeMs(61000) == 60000) passed++;
		if (worker.LegAgeMs(61000) == 60000) passed++;
		worker.m_iDriverWaitMs = 20000;
		if (worker.ProgressAgeMs(61000) == 40000) passed++;
		if (worker.LegAgeMs(61000) == 40000) passed++;
		worker.MarkProgress(61000);
		if (worker.ProgressAgeMs(62000) == 1000) passed++;
		worker.m_iDriverWaitMs += 10000;
		if (worker.ProgressAgeMs(72000) == 1000) passed++;
		AICF_LogisticsDriverInteraction wait = new AICF_LogisticsDriverInteraction();
		wait.m_iStartedAtMs = 1000;
		wait.m_iProgressAtMs = 1000;
		if (wait.DeadlineReason(30999).IsEmpty()) passed++;
		if (wait.DeadlineReason(31000) == "DRIVER_INTERACTION_NO_PROGRESS") passed++;
		if (wait.DeadlineReason(32000) == "DRIVER_INTERACTION_NO_PROGRESS") passed++;
		wait.m_iProgressAtMs = 120000;
		if (wait.DeadlineReason(120999).IsEmpty()) passed++;
		if (wait.DeadlineReason(121000) == "DRIVER_INTERACTION_DEADLINE") passed++;
		wait.m_iProgressAtMs = 1000;
		wait.m_iWaitBeforeMs = 110000;
		if (wait.DeadlineReason(10999).IsEmpty()) passed++;
		if (wait.DeadlineReason(11000) == "DRIVER_INTERACTION_LEG_BUDGET") passed++;
		Print(string.Format("[AICF][LOGISTICS_PROBE_DRIVER_CLOCK_CONTRACT] test_only=1 passed=%1 total=13 production_clocks=1 physical_interaction=0", passed));
	}

	protected void AICF_ProbePolicy()
	{
		int passed;
		if (!AICF_LogisticsPlanner.DemandOpen(false, 40, 100, 40, 80)) passed++;
		if (AICF_LogisticsPlanner.DemandOpen(false, 39.9, 100, 40, 80)) passed++;
		if (AICF_LogisticsPlanner.DemandOpen(true, 79.9, 100, 40, 80)) passed++;
		if (!AICF_LogisticsPlanner.DemandOpen(true, 80, 100, 40, 80)) passed++;
		if (AICF_LogisticsPlanner.Deficit(30, 100, 80, 45) == 5) passed++;
		if (AICF_LogisticsPlanner.Deficit(90, 100, 80, 0) == 0) passed++;
		if (AICF_LogisticsPlanner.Donatable(90, 100, 90, 80, 0, false) == 0) passed++;
		if (AICF_LogisticsPlanner.Donatable(95, 100, 90, 80, 10, false) == 5) passed++;
		if (AICF_LogisticsPlanner.Donatable(85, 100, 90, 80, 0, true) == 5) passed++;
		float value;
		if (!AICF_LogisticsConfig.Decimal("NaN", false, value)) passed++;
		if (!AICF_LogisticsConfig.Decimal("garbage", false, value)) passed++;
		if (!AICF_LogisticsConfig.Decimal("1.5", true, value)) passed++;
		if (AICF_LogisticsConfig.Decimal("0.125", false, value) && value == 0.125) passed++;
		AICF_LogisticsConfig config = new AICF_LogisticsConfig(false);
		string reason;
		if (config.Validate(reason)) passed++;
		config.m_fTargetPercent = 95;
		if (!config.Validate(reason)) passed++;
		Print(string.Format("[AICF][LOGISTICS_POLICY_CONTRACT] test_only=1 passed=%1 total=15", passed));
	}
}
