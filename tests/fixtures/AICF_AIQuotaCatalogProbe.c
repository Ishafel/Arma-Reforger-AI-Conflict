// Native catalog workflow. No fabricated player identity, roster or actions.
// Controlled pressure is a real reservation; funding is one native generator request.
// No raw balance, health, pose, AI-counter or AI-limit writes.
class AICF_AIQuotaCatalogProbe
{
	static int s_Started;
	static int s_Last;
	static int s_RequestedAt;
	static bool s_Finished;
	static string s_Mode;
	static SCR_PlayerController s_Player;
	static int s_ClientReadyPlayer;
	static ref AICF_AIQuotaCatalogIdentity s_User;
	static SCR_CatalogEntitySpawnerComponent s_Spawner;
	static SCR_EntityCatalogEntry s_Entry;
	static SCR_EntitySpawnerSlotComponent s_Slot;
	static ref AICF_AIQuotaReservation s_Pressure;
	static AICF_AIWorldQuotaService s_Service;
	static int s_Serial;
	static int s_Expected;
	static int s_Factories;
	static int s_Headers;
	static int s_Callbacks;
	static int s_Debits;
	static float s_Debited;
	static bool s_CallbackInvalid;
	static float s_Supplies;
	static float s_Cost;
	static int s_LastStatus;
	static int s_LastGroupStatus;
	static bool s_FundingAttempted;
	static IEntity s_Produced;
	static ref array<ref AICF_AIQuotaVehicleEntity> s_Bodies = {};
	static ref array<ref AICF_AIQuotaCreation> s_Creations = {};

	static void ObserveCallback(IEntity actor)
	{
		bool bound;
		foreach (AICF_AIQuotaCreation creation : s_Creations)
		{
			if (!creation || creation.m_Entity != actor) continue;
			bound = creation.m_Status == AICF_EAIQuotaBirthStatus.BOUND && creation.m_bFinished && creation.m_Ticket &&
				creation.m_Ticket.m_bClosed && creation.m_Body && creation.m_Body.MatchesIdentity();
			break;
		}
		if (!bound) s_CallbackInvalid = true;
		s_Callbacks++;
		Log("NATIVE_CALLBACK", string.Format("index=%1 bound_receipt_before_native_activation=%2", s_Callbacks, bound));
	}

	static bool Enabled()
	{
		return System.GetCLIParam("aicfQuotaCatalogProbe", s_Mode);
	}

	static void Log(string eventName, string details)
	{
		Print("[AICF][QUOTA_CATALOG_PROBE][" + eventName + "] mode=" + s_Mode + " " + details);
	}

	static void Finish(bool passed, string reason)
	{
		if (s_Finished) return;
		s_Finished = true;
		if (s_Service && s_Pressure) s_Service.CancelReservation(s_Pressure, s_Serial);
		s_Pressure = null;
		SCR_PlayerController donePlayer = s_Player;
		if (!donePlayer && s_ClientReadyPlayer > 0)
			donePlayer = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(s_ClientReadyPlayer));
		if (donePlayer && donePlayer.GetPlayerId() == s_ClientReadyPlayer) donePlayer.AICF_QuotaCatalogProbeDone();
		Log("DONE", string.Format("passed=%1 reason=%2 factories=%3 callbacks=%4 debits=%5 debited=%6 expected=%7 elapsed_ms=%8 headers=%9",
			passed, reason, s_Factories, s_Callbacks, s_Debits, s_Debited, s_Expected, System.GetTickCount(s_Started), s_Headers));
		if (s_RequestedAt) Log("TAIL", string.Format("actual_tail_ms=%1 retained_original_body_count=%2", System.GetTickCount(s_RequestedAt), s_Bodies.Count()));
		GetGame().GetCallqueue().CallLater(Close, 1500, false);
	}

	static void Close() { GetGame().RequestClose(); }
	static void Stop()
	{
		if (s_Service && s_Pressure) s_Service.CancelReservation(s_Pressure, s_Serial);
		s_Pressure = null;
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(Close);
		AICF_AIQuotaCatalogResourceObservation.Stop();
		s_Bodies.Clear();
		s_Creations.Clear();
	}

	static bool CurrentUser()
	{
		if (!s_Player || !s_User || !s_User.IsCurrent()) return false;
		if (!s_User.m_Rpl || !s_User.m_Rpl.Id().IsValid()) return false;
		return s_Player.GetControlledEntity() == s_User.m_Entity &&
			GetGame().GetPlayerManager().GetPlayerController(s_Player.GetPlayerId()) == s_Player;
	}

	static void Tick()
	{
		if (!Replication.IsServer() || !Enabled() || s_Finished) return;
		int now = System.GetTickCount();
		if (!s_Started)
		{
			s_Started = now;
			AICF_AIQuotaCatalogResourceObservation.DumpKnownSources();
		}
		if (System.GetTickCount(s_Started) >= 300000) { Finish(false, "BOUNDED_TIMEOUT"); return; }
		if (now - s_Last < 1000) return;
		s_Last = now;
		if (now - s_LastStatus >= 10000)
		{
			s_LastStatus = now;
			Log("WAIT", string.Format("actual_player=%1 actual_avatar=%2 selected_spawner=%3 requested_at=%4 ready_player=%5", s_Player != null, s_User != null, s_Spawner != null, s_RequestedAt, s_ClientReadyPlayer));
		}
		if (s_RequestedAt)
		{
			if (!CurrentUser()) { Finish(false, "PLAYER_CONTEXT_CHANGED"); return; }
			foreach (AICF_AIQuotaVehicleEntity body : s_Bodies)
				if (!body || !body.IsCurrent()) { Finish(false, "NEWBORN_IDENTITY_LOST"); return; }
			if (s_Mode != "vehicle")
				foreach (AICF_AIQuotaVehicleEntity liveBody : s_Bodies)
					if (!AICF_GroupRuntime.IsAliveCharacter(liveBody.m_Entity)) { Finish(false, "NEWBORN_NOT_ALIVE"); return; }
			if (s_Factories > s_Expected || s_Callbacks > s_Expected || s_Debits > 1) { Finish(false, "DUPLICATE_SIDE_EFFECT"); return; }
			if (now - s_RequestedAt < 25000) return;
			if (s_Mode == "vehicle")
			{
				if (!s_Service.IsConfigured() || !s_Pressure ||
					s_Service.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK) + s_Service.GetLedger().GetReserved(AICF_EAIQuotaBucket.STOCK) != s_Service.GetLimits().GetLimit(AICF_EAIQuotaBucket.STOCK))
				{ Finish(false, "CHARACTER_BUCKET_FULL_NOT_PROVEN"); return; }
				Vehicle vehicle = Vehicle.Cast(s_Produced);
				BaseCompartmentManagerComponent manager;
				if (vehicle) manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
				if (!manager) { Finish(false, "VEHICLE_COMPARTMENTS_UNKNOWN"); return; }
				array<BaseCompartmentSlot> seats = {};
				manager.GetCompartments(seats);
				if (seats.IsEmpty()) { Finish(false, "VEHICLE_EMPTY_NOT_PROVEN"); return; }
				foreach (BaseCompartmentSlot seat : seats)
					if (!seat || seat.GetOccupant() || seat.IsReserved()) { Finish(false, "VEHICLE_NOT_EMPTY"); return; }
				AIControlComponent control = AIControlComponent.Cast(vehicle.FindComponent(AIControlComponent));
				if (control && control.GetControlAIAgent() && control.GetControlAIAgent().CountTowardsAIWorldLimit())
				{ Finish(false, "VEHICLE_AI_AGENT_COUNTED"); return; }
				Log("EMPTY_VEHICLE", "actual_compartments_read=1 occupants=0 reservations=0 counted_vehicle_agent=0 character_bucket_full=1");
			}
			if (s_Factories != s_Expected || (s_Mode != "vehicle" && s_Callbacks != s_Expected) || s_Debits != 1 || s_CallbackInvalid || !(s_Debited > 0 && Math.AbsFloat(s_Debited - s_Cost) <= 0.01))
			{ Finish(false, "PHYSICAL_OR_PAYMENT_INCOMPLETE"); return; }
			int expectedHeaders;
			if (s_Mode == "group") expectedHeaders = 1;
			if (s_Headers != expectedHeaders) { Finish(false, "HEADER_COUNT_MISMATCH"); return; }
			Finish(true, "ACTUAL_NATIVE_CATALOG_TAIL");
			return;
		}
		if (!s_Player)
		{
			if (s_ClientReadyPlayer <= 0) return;
			SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(s_ClientReadyPlayer));
			if (!player || player.GetPlayerId() != s_ClientReadyPlayer || !player.GetControlledEntity())
			{
				Log("READY_LOOKUP", string.Format("ready_player=%1 controller=%2", s_ClientReadyPlayer, player != null));
				return;
			}
			s_User = AICF_AIQuotaCatalogIdentity.Capture(player.GetControlledEntity(), false);
			if (s_User && (!s_User.m_Rpl || !s_User.m_Rpl.Id().IsValid())) s_User = null;
			if (!s_User)
			{
				IEntity actor = player.GetControlledEntity();
				RplComponent rpl;
				bool master;
				bool valid;
				bool mainWorld;
				if (actor) rpl = RplComponent.Cast(actor.FindComponent(RplComponent));
				if (actor) mainWorld = actor.GetWorld() == GetGame().GetWorld();
				if (rpl) master = rpl.IsMaster();
				if (rpl) valid = rpl.Id().IsValid();
				Log("AVATAR_CAPTURE_REJECTED", string.Format("ready_player=%1 actor=%2 rpl_present=%3 rpl_master=%4 rpl_valid=%5 main_world=%6", s_ClientReadyPlayer, actor != null, rpl != null, master, valid, mainWorld));
			}
			if (s_User) s_Player = player;
			if (!s_Player) return;
		}
		if (!CurrentUser()) { Finish(false, "AVATAR_IDENTITY_UNPROVEN"); return; }
		SCR_PlayerControllerGroupComponent groups = SCR_PlayerControllerGroupComponent.Cast(s_Player.FindComponent(SCR_PlayerControllerGroupComponent));
		Faction faction = SCR_FactionManager.SGetPlayerFaction(s_Player.GetPlayerId());
		SCR_GroupsManagerComponent groupManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groups || !faction || !groupManager) return;
		s_Service = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetGame().GetWorld());
		if (!s_Service || !s_Service.IsConfigured()) return;
		bool observeEnumeration = now - s_LastGroupStatus >= 15000;
		if (observeEnumeration)
		{
			s_LastGroupStatus = now;
			SCR_AIGroup observedGroup = groups.GetPlayersGroup();
			EntityID groupId;
			bool slave;
			if (observedGroup) groupId = observedGroup.GetID();
			if (observedGroup) slave = observedGroup.GetSlave() != null;
			bool leader = groups.IsPlayerLeaderOwnGroup();
			Log("GROUP_CONTEXT", string.Format("actual_group=%1 group_id=%2 slave=%3 leader=%4 spawner_count=%5 observation_only=1", observedGroup != null, groupId, slave, leader, SCR_CatalogEntitySpawnerComponent.INSTANCES.Count()));
		}
		if (!groups.GetPlayersGroup())
		{
			SCR_AIGroup empty = groupManager.TryFindEmptyGroup(faction);
			if (empty && (empty.GetAgentsCount() != 0 || s_Service.GetOwners().Find(empty))) { Finish(false, "EMPTY_GROUP_ALREADY_OWNED"); return; }
			if (empty) groups.RPC_AskJoinGroup(empty.GetGroupID());
			else groups.RPC_AskCreateGroup();
			return;
		}
		if (!groups.IsPlayerLeaderOwnGroup() || !groups.GetPlayersGroup().GetSlave()) return;
		if (s_Service.GetOwners().Find(groups.GetPlayersGroup())) { Finish(false, "PLAYER_GROUP_PROJECT_OWNED"); return; }
		EEntityCatalogType wanted = EEntityCatalogType.GROUP;
		if (s_Mode == "character") wanted = EEntityCatalogType.CHARACTER;
		if (s_Mode == "vehicle") wanted = EEntityCatalogType.VEHICLE;
		int checked;
		foreach (SCR_CatalogEntitySpawnerComponent spawner : SCR_CatalogEntitySpawnerComponent.INSTANCES)
		{
			if (++checked > 128) break;
			if (spawner && observeEnumeration) spawner.AICF_QuotaProbeDescribe(wanted, faction);
			if (!spawner || spawner.GetFaction() != faction) continue;
			bool selected = spawner.AICF_QuotaProbeSelect(wanted, s_User.m_Entity, s_Entry, s_Slot);
			if (s_Finished) return;
			if (selected) { s_Spawner = spawner; break; }
		}
		if (!s_Spawner) return;
		SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(s_Entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (!data) { Finish(false, "NATIVE_ENTRY_DATA_MISSING"); return; }
		s_Expected = data.GetEntityCount();
		if (s_Mode == "vehicle") s_Expected = 1;
		if (s_Expected <= 0 || s_Expected > 20) { Finish(false, "UNBOUNDED_NATIVE_ENTRY"); return; }
		s_Supplies = s_Spawner.GetSpawnerResourceValue();
		s_Cost = data.GetSupplyCost();
		Log("SELECTED", string.Format("player=%1 faction=%2 prefab=%3 authored=%4 cost=%5 supplies=%6 actual_avatar=1 actual_group_leader=1",
			s_Player.GetPlayerId(), faction.GetFactionKey(), s_Entry.GetPrefab(), s_Expected, data.GetSupplyCost(), s_Supplies));
		if (s_Spawner) AICF_AIQuotaCatalogCensusObservation.EntityState(s_Spawner.GetOwner(), "SELECTED_PRODUCER");
		if (s_Slot) AICF_AIQuotaCatalogCensusObservation.EntityState(s_Slot.GetOwner(), "SELECTED_SLOT");
		if (s_Mode == "group" || s_Mode == "vehicle")
		{
			int keep;
			if (s_Mode == "group") keep = s_Expected - 1;
			int pressure = s_Service.GetLimits().GetLimit(AICF_EAIQuotaBucket.STOCK) -
				s_Service.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK) - s_Service.GetLedger().GetReserved(AICF_EAIQuotaBucket.STOCK) - keep;
			s_Serial = s_Service.AllocateContextSerial();
			if (pressure <= 0) { Finish(false, "CONTROLLED_PRESSURE_NOT_AVAILABLE"); return; }
			s_Pressure = s_Service.ReserveUnits(AICF_EAIQuotaBucket.STOCK, pressure, s_Serial);
			if (!s_Pressure) { Finish(false, "PRESSURE_RESERVATION_REJECTED"); return; }
			SCR_EEntityRequestStatus state = s_Spawner.GetRequestState(s_Entry, s_User.m_Entity);
			if (s_Mode == "group")
			{
				if (state != SCR_EEntityRequestStatus.AI_LIMIT_REACHED) { Finish(false, "FULL_BASKET_VETO_NOT_PROVEN"); return; }
				s_Spawner.AICF_QuotaProbeRequest(s_Entry, s_User.m_Entity, s_Slot);
				if (s_Factories || s_Headers || s_Callbacks || s_Debits || s_Spawner.GetSpawnerResourceValue() != s_Supplies)
				{ Finish(false, "QUOTA_VETO_SIDE_EFFECT"); return; }
				Log("FULL_BASKET_NEGATIVE", string.Format("remaining=%1 requested=%2 native_factories=0 headers=0 callbacks=0 debits=0", keep, s_Expected));
				if (!s_Service.CancelReservation(s_Pressure, s_Serial)) { Finish(false, "OWN_PRESSURE_RELEASE_FAILED"); return; }
				s_Pressure = null;
			}
			else if (state != SCR_EEntityRequestStatus.CAN_SPAWN) { Finish(false, "EMPTY_HUMAN_VEHICLE_VETOED"); return; }
		}
		s_RequestedAt = now;
		s_Spawner.AICF_QuotaProbeRequest(s_Entry, s_User.m_Entity, s_Slot);
	}
}

modded class SCR_CatalogEntitySpawnerComponent
{
	bool AICF_QuotaProbeFundingCurrent(AICF_AIQuotaCatalogIdentity producer, SCR_ResourceComponent resource,
		SCR_ResourceConsumer consumer, SCR_ResourceGenerator generator, SCR_EntityCatalogEntry entry, IEntity user, int quote)
	{
		if (!Replication.IsServer() || AICF_AIQuotaCatalogProbe.s_Finished || AICF_AIQuotaCatalogProbe.s_RequestedAt) return false;
		if (!producer || !producer.IsCurrent() || GetOwner() != producer.m_Entity || !INSTANCES.Contains(this)) return false;
		if (!resource || GetSpawnerResourceComponent() != resource || !resource.GetOwner()) return false;
		if (resource.GetOwner().GetWorld() != GetGame().GetWorld()) return false;
		if (!consumer || !generator || resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES) != consumer) return false;
		if (resource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES) != generator) return false;
		if (!AICF_AIQuotaCatalogProbe.CurrentUser() || !AICF_AIQuotaCatalogProbe.s_User || AICF_AIQuotaCatalogProbe.s_User.m_Entity != user) return false;
		if (!m_aAssetList.Contains(entry) || GetRequesterFaction(user) != GetFaction()) return false;
		SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (!data || quote <= 0 || data.GetSupplyCost() != quote || !IsSuppliesConsumptionEnabled()) return false;
		return producer.IsCurrent() && AICF_AIQuotaCatalogProbe.CurrentUser() && !AICF_AIQuotaCatalogProbe.s_Finished;
	}

	bool AICF_QuotaProbeFund(SCR_EntityCatalogEntry entry, IEntity user, int quote)
	{
		if (AICF_AIQuotaCatalogProbe.s_FundingAttempted) return false;
		AICF_AIQuotaCatalogIdentity producer = AICF_AIQuotaCatalogIdentity.Capture(GetOwner(), true);
		SCR_ResourceComponent resource = GetSpawnerResourceComponent();
		if (!resource) return false;
		SCR_ResourceConsumer consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		SCR_ResourceGenerator generator = resource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!AICF_QuotaProbeFundingCurrent(producer, resource, consumer, generator, entry, user, quote)) return false;
		float before = GetSpawnerResourceValue();
		float shortfall = quote - before;
		if (!(before >= 0 && before < quote && shortfall > 0 && shortfall <= quote)) return false;
		string generatorType = generator.Type().ToString();
		EntityID producerId = producer.m_Id;
		AICF_AIQuotaCatalogResourceObservation.Links(generator, consumer, "BEFORE_FUNDING");
		array<SCR_MilitaryBaseComponent> bases = {};
		GetBases(bases);
		AICF_AIQuotaCatalogProbe.Log("RESOURCE_SERVICE_BASES", string.Format("producer=%1 bases=%2 examined=%3 observation_only=1", producerId, bases.Count(), Math.Min(bases.Count(), 8)));
		for (int baseIndex; baseIndex < bases.Count() && baseIndex < 8; baseIndex++)
		{
			SCR_MilitaryBaseComponent baseNode = bases[baseIndex];
			EntityID baseId;
			IEntity baseOwner;
			if (baseNode) baseOwner = baseNode.GetOwner();
			if (baseOwner) baseId = baseOwner.GetID();
			AICF_AIQuotaCatalogProbe.Log("RESOURCE_SERVICE_BASE", string.Format("producer=%1 index=%2 base=%3 present=%4 observation_only=1", producerId, baseIndex, baseId, baseNode != null));
		}
		if (!AICF_QuotaProbeFundingCurrent(producer, resource, consumer, generator, entry, user, quote)) return false;
		AICF_AIQuotaCatalogProbe.s_FundingAttempted = true;
		AICF_AIQuotaCatalogResourceObservation.s_Generator = generator;
		AICF_AIQuotaCatalogResourceObservation.s_Consumer = consumer;
		AddSpawnerSupplies(shortfall);
		if (AICF_AIQuotaCatalogResourceObservation.s_Generator == generator) AICF_AIQuotaCatalogResourceObservation.Stop();
		if (!AICF_QuotaProbeFundingCurrent(producer, resource, consumer, generator, entry, user, quote))
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "CONTROLLED_FUNDING_CONTEXT_CHANGED");
			return false;
		}
		float after = GetSpawnerResourceValue();
		AICF_AIQuotaCatalogProbe.Log("CONTROLLED_RESOURCE_FUNDING", string.Format("producer=%1 generator_type=%2 before=%3 amount=%4 after=%5 effective_cost=%6 native_generator=1 fixture_only=1", producerId, generatorType, before, shortfall, after, quote));
		if (!(after >= quote && Math.AbsFloat(after - before - shortfall) <= 0.01))
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "CONTROLLED_NATIVE_FUNDING_NOT_CONFIRMED");
			return false;
		}
		return true;
	}

	// Describe the native filtered list; never populate or enable catalog entries.
	void AICF_QuotaProbeDescribe(EEntityCatalogType wanted, Faction requesterFaction)
	{
		EntityID ownerId;
		IEntity owner = GetOwner();
		if (owner) ownerId = owner.GetID();
		Faction actualFaction = GetFaction();
		string factionKey = "NONE";
		if (actualFaction) factionKey = actualFaction.GetFactionKey();
		string allowedTypes;
		foreach (EEntityCatalogType allowedType : m_aCatalogTypes)
			allowedTypes += string.Format("%1,", allowedType);
		int examined;
		int wantedCount;
		int enabledData;
		int eligibleCount;
		foreach (SCR_EntityCatalogEntry entry : m_aAssetList)
		{
			if (examined >= 256) break;
			examined++;
			if (!entry) continue;
			SCR_EntityCatalog parent = entry.GetCatalogParent();
			int actualType = -1;
			if (parent) actualType = parent.GetCatalogType();
			SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
			int count;
			int cost;
			if (data) count = data.GetEntityCount();
			if (data) cost = data.GetSupplyCost();
			if (actualType == wanted) wantedCount++;
			if (actualType == wanted && data) enabledData++;
			bool eligible = actualType == wanted && data && cost > 0 && count > 0 && count <= 20;
			if (wanted == EEntityCatalogType.GROUP && count < 2) eligible = false;
			if (eligible) eligibleCount++;
			if (examined <= 16 || (actualType == wanted && wantedCount <= 16))
			{
				string details = string.Format("owner=%1 index=%2 prefab=%3 actual_type=%4 wanted_type=%5", ownerId, examined - 1, entry.GetPrefab(), actualType, wanted);
				details += string.Format(" entry_enabled=%1 enabled_spawner_data=%2 authored=%3 cost=%4 eligible=%5 observation_only=1", entry.IsEnabled(), data != null, count, cost, eligible);
				AICF_AIQuotaCatalogProbe.Log("CATALOG_METADATA", details);
			}
		}
		string summary = string.Format("owner=%1 faction=%2 faction_exact=%3 catalog_types=%4 assets=%5 examined=%6", ownerId, factionKey, actualFaction == requesterFaction, allowedTypes, m_aAssetList.Count(), examined);
		summary += string.Format(" wanted=%1 enabled_data=%2 eligible=%3 scan_complete=%4 observation_only=1", wantedCount, enabledData, eligibleCount, examined == m_aAssetList.Count());
		AICF_AIQuotaCatalogProbe.Log("CATALOG_ENUMERATION", summary);
	}

	bool AICF_QuotaProbeSelect(EEntityCatalogType type, IEntity user, out SCR_EntityCatalogEntry selected, out SCR_EntitySpawnerSlotComponent slot)
	{
		int checked;
		int eligible = 0;
		foreach (SCR_EntityCatalogEntry entry : m_aAssetList)
		{
			if (++checked > 256) return false;
			if (!entry || !entry.GetCatalogParent() || entry.GetCatalogParent().GetCatalogType() != type) continue;
			SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
			if (!data || data.GetSupplyCost() <= 0 || data.GetEntityCount() <= 0 || data.GetEntityCount() > 20) continue;
			if (type == EEntityCatalogType.GROUP && data.GetEntityCount() < 2) continue;
			eligible++;
			SCR_EEntityRequestStatus nativeState = GetRequestState(entry, user);
			if (nativeState == SCR_EEntityRequestStatus.NOT_ENOUGH_SUPPLIES && !AICF_AIQuotaCatalogProbe.s_FundingAttempted)
			{
				if (AICF_QuotaProbeFund(entry, user, data.GetSupplyCost())) nativeState = GetRequestState(entry, user);
				if (AICF_AIQuotaCatalogProbe.s_Finished) return false;
			}
			if (eligible <= 16) AICF_AIQuotaCatalogProbe.Log("ENTRY", string.Format("prefab=%1 authored=%2 cost=%3 native_state=%4", entry.GetPrefab(), data.GetEntityCount(), data.GetSupplyCost(), nativeState));
			if (nativeState != SCR_EEntityRequestStatus.CAN_SPAWN) continue;
			slot = GetFreeSlot(data);
			if (!slot) continue;
			selected = entry;
			return true;
		}
		return false;
	}

	void AICF_QuotaProbeRequest(SCR_EntityCatalogEntry entry, IEntity user, SCR_EntitySpawnerSlotComponent slot)
	{
		InitiateSpawn(entry, GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user), slot);
		AICF_AIQuotaCatalogProbe.s_Produced = m_SpawnedEntity;
	}

	override protected IEntity SpawnEntity(ResourceName entityResourceName, notnull IEntity slotOwner)
	{
		bool target = this == AICF_AIQuotaCatalogProbe.s_Spawner && AICF_AIQuotaCatalogProbe.s_Mode == "vehicle";
		AICF_AIQuotaCatalogIdentity producer;
		AICF_AIQuotaCatalogIdentity slot;
		if (target) producer = AICF_AIQuotaCatalogIdentity.Capture(GetOwner(), true);
		if (target) slot = AICF_AIQuotaCatalogIdentity.Capture(slotOwner, false);
		IEntity raw = super.SpawnEntity(entityResourceName, slotOwner);
		if (!target) return raw;
		if (AICF_AIQuotaCatalogProbe.s_Finished || this != AICF_AIQuotaCatalogProbe.s_Spawner || !producer || !slot)
		{
			AICF_AIQuotaCatalogProbe.s_CallbackInvalid = true;
			return raw;
		}
		if (!producer.IsCurrent() || !slot.IsCurrent() || !Vehicle.Cast(raw))
		{
			AICF_AIQuotaCatalogProbe.s_CallbackInvalid = true;
			return raw;
		}
		AICF_AIQuotaVehicleEntity identity = AICF_AIQuotaVehicleEntity.Capture(raw);
		if (!identity || !producer.IsCurrent() || !slot.IsCurrent() || AICF_AIQuotaCatalogProbe.s_Finished || this != AICF_AIQuotaCatalogProbe.s_Spawner)
		{
			AICF_AIQuotaCatalogProbe.s_CallbackInvalid = true;
			return raw;
		}
		AICF_AIQuotaCatalogProbe.s_Factories++;
		AICF_AIQuotaCatalogProbe.s_Bodies.Insert(identity);
		AICF_AIQuotaCatalogProbe.Log("BODY_IDENTITY", string.Format("entity=%1 rpl=%2", identity.m_Id, identity.m_sRpl));
		AICF_AIQuotaCatalogProbe.Log("NATIVE_VEHICLE_FACTORY", "actual_protected_factory_return=1 actual_vehicle=1 retained_physical_identity=1 character_basket=0");
		return raw;
	}

	override protected IEntity AICF_QuotaPinnedCatalogFactory(AICF_AIQuotaCreation creation, ResourceName entityResourceName,
		notnull IEntity slotOwner, out bool nativeCalled)
	{
		IEntity raw = super.AICF_QuotaPinnedCatalogFactory(creation, entityResourceName, slotOwner, nativeCalled);
		if (this == AICF_AIQuotaCatalogProbe.s_Spawner && nativeCalled && creation.m_bHeaderOnly)
		{
			AICF_AIQuotaCatalogProbe.s_Headers++;
			AICF_AIQuotaCatalogProbe.Log("NATIVE_HEADER", string.Format("actual_native_header=1 nonnull=%1", raw != null));
		}
		if (this == AICF_AIQuotaCatalogProbe.s_Spawner && nativeCalled && !creation.m_bHeaderOnly)
		{
			AICF_AIQuotaCatalogProbe.s_Factories++;
			AICF_AIQuotaCatalogProbe.s_Creations.Insert(creation);
			AICF_AIQuotaVehicleEntity identity = AICF_AIQuotaVehicleEntity.Capture(raw);
			AICF_AIQuotaCatalogProbe.s_Bodies.Insert(identity);
			AICF_AIQuotaCatalogProbe.Log("NATIVE_BODY", string.Format("index=%1 identity=%2 before_native_callback=1",
				AICF_AIQuotaCatalogProbe.s_Factories, identity != null));
			if (identity) AICF_AIQuotaCatalogProbe.Log("BODY_IDENTITY", string.Format("entity=%1 rpl=%2", identity.m_Id, identity.m_sRpl));
		}
		return raw;
	}

	override void AddSpawnerSupplies(float supplies)
	{
		float before = GetSpawnerResourceValue();
		super.AddSpawnerSupplies(supplies);
		if (this != AICF_AIQuotaCatalogProbe.s_Spawner || supplies >= 0) return;
		AICF_AIQuotaCatalogProbe.s_Debits++;
		float actual = before - GetSpawnerResourceValue();
		AICF_AIQuotaCatalogProbe.s_Debited += actual;
		if (!(actual >= 0 && Math.AbsFloat(actual + supplies) <= 0.01)) AICF_AIQuotaCatalogProbe.s_CallbackInvalid = true;
		AICF_AIQuotaCatalogProbe.Log("PAYMENT", string.Format("value=%1 actual_delta=%2 actual_native_debit=1 native_consumer=1", supplies, actual));
	}

	override protected void OnChimeraCharacterSpawned(notnull SCR_ChimeraCharacter ai, notnull IEntity user, SCR_EntityLabelPointComponent rallyPoint = null)
	{
		if (this == AICF_AIQuotaCatalogProbe.s_Spawner) AICF_AIQuotaCatalogProbe.ObserveCallback(ai);
		super.OnChimeraCharacterSpawned(ai, user, rallyPoint);
	}
}

modded class SCR_PlayerController
{
	void AICF_QuotaCatalogClientReady() { Rpc(RpcAsk_AICFQuotaCatalogClientReady); }
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFQuotaCatalogClientReady()
	{
		Print(string.Format("[AICF][QUOTA_CLIENT_READY_RPC] server=%1 actual_player=%2 actual_actor=%3", Replication.IsServer(), GetPlayerId(), GetControlledEntity() != null));
		if (AICF_AIQuotaCatalogProbe.Enabled()) AICF_AIQuotaCatalogProbe.s_ClientReadyPlayer = GetPlayerId();
	}
	void AICF_QuotaCatalogProbeDone() { Rpc(RpcDo_AICFQuotaCatalogProbeDone); }
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFQuotaCatalogProbeDone() { GetGame().RequestClose(); }
}

class AICF_AIQuotaCatalogClientBootstrap
{
	protected static int m_iQuotaClientAt;
	protected static int m_iQuotaClientPhase;
	protected static bool m_bQuotaClientSpawnRequested;
	static void Start()
	{
		m_iQuotaClientAt = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(AICF_QuotaClientBootstrap, 1000, true);
	}
	static void Stop() { GetGame().GetCallqueue().Remove(AICF_QuotaClientBootstrap); }
	static void AICF_QuotaClientBootstrap()
	{
		if (System.GetTickCount(m_iQuotaClientAt) >= 240000)
		{
			Print("[AICF][QUOTA_CLIENT_BOOTSTRAP] failed=1 reason=BOUNDED_TIMEOUT");
			GetGame().GetCallqueue().Remove(AICF_QuotaClientBootstrap);
			GetGame().RequestClose();
			return;
		}
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		string stableKey = "USSR";
		string runtimeKey = AICF_ContentProfile.GetActive().GetRuntimeFactionKey(stableKey);
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(runtimeKey));
		if (!player || player.GetPlayerId() <= 0 || !faction || !faction.GetMainBase()) return;
		if (player.GetControlledEntity())
		{
			if (SCR_Faction.GetEntityFaction(player.GetControlledEntity()) != faction) return;
			Print(string.Format("[AICF][QUOTA_CLIENT_BOOTSTRAP] phase=4 actual_actor=1 actual_faction=%1 player=%2", runtimeKey, player.GetPlayerId()));
			player.AICF_QuotaCatalogClientReady();
			GetGame().GetCallqueue().Remove(AICF_QuotaClientBootstrap);
			return;
		}
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!affiliation) return;
		if (affiliation.GetAffiliatedFaction() != faction)
		{
			if (m_iQuotaClientPhase == 0)
			{
				m_iQuotaClientPhase = 1;
				Print("[AICF][QUOTA_CLIENT_BOOTSTRAP] phase=1 native_faction_request=1 faction=" + runtimeKey);
				affiliation.RequestFaction(faction);
			}
			return;
		}
		array<ref SCR_BasePlayerLoadout> loadouts = {};
		GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(faction, loadouts);
		SCR_BasePlayerLoadout loadout;
		foreach (SCR_BasePlayerLoadout candidate : loadouts)
			if (candidate && !SCR_PlayerArsenalLoadout.Cast(candidate)) { loadout = candidate; break; }
		SCR_PlayerLoadoutComponent loadoutComponent = SCR_PlayerLoadoutComponent.Cast(player.FindComponent(SCR_PlayerLoadoutComponent));
		if (!loadoutComponent || !loadout) return;
		if (loadoutComponent.GetLoadout() != loadout)
		{
			if (m_iQuotaClientPhase < 2)
			{
				m_iQuotaClientPhase = 2;
				Print(string.Format("[AICF][QUOTA_CLIENT_BOOTSTRAP] phase=2 native_loadout_request=1 actual_loadout_index=%1 count=%2 prefab=%3", GetGame().GetLoadoutManager().GetLoadoutIndex(loadout), loadouts.Count(), loadout.GetLoadoutResource()));
				loadoutComponent.RequestLoadout(loadout);
			}
			return;
		}
		if (m_bQuotaClientSpawnRequested) return;
		array<SCR_SpawnPoint> points = SCR_SpawnPoint.GetSpawnPointsForFaction(runtimeKey);
		if (!points || points.IsEmpty() || loadout.GetLoadoutResource().IsEmpty()) return;
		SCR_SpawnPoint chosen;
		foreach (SCR_SpawnPoint point : points)
			if (point && point.IsSpawnPointActive() && point.IsSpawnPointEnabled() && point.GetRplId().IsValid()) { chosen = point; break; }
		SCR_RespawnComponent respawn = SCR_RespawnComponent.Cast(player.GetRespawnComponent());
		if (!chosen || !respawn) return;
		Print(string.Format("[AICF][QUOTA_CLIENT_BOOTSTRAP] phase=3 native_spawn_request=1 spawnpoint_count=%1 rpl=%2 prefab=%3", points.Count(), chosen.GetRplId(), loadout.GetLoadoutResource()));
		m_bQuotaClientSpawnRequested = respawn.RequestSpawn(new SCR_SpawnPointSpawnData(loadout.GetLoadoutResource(), chosen.GetRplId()));
	}
}

modded class ArmaReforgerScripted
{
	override bool OnGameStart()
	{
		bool started = super.OnGameStart();
		if (!AICF_AIQuotaCatalogProbe.Enabled()) return started;
		if (Replication.IsServer()) GetGame().GetCallqueue().CallLater(AICF_AIQuotaCatalogProbe.Tick, 1000, true);
		else AICF_AIQuotaCatalogClientBootstrap.Start();
		return started;
	}
	override void OnGameEnd()
	{
		AICF_AIQuotaCatalogClientBootstrap.Stop();
		if (AICF_AIQuotaCatalogProbe.Enabled()) AICF_AIQuotaCatalogProbe.Stop();
		super.OnGameEnd();
	}
}

// Metadata exports stay under the fresh own profile; native resources are read only.
class AICF_AIQuotaCatalogResourceObservation
{
	static SCR_ResourceGenerator s_Generator;
	static SCR_ResourceConsumer s_Consumer;
	static void Stop() { s_Generator = null; s_Consumer = null; }

	static void DumpKnownSources()
	{
		if (!Replication.IsServer() || !GetGame()) return;
		array<ResourceName> paths = {
			"Prefabs/MP/Campaign/RequestTable_US_Barracks.et",
			"Prefabs/MP/Campaign/RequestTable_USSR_Barracks.et",
			"Prefabs/Systems/Compositions/EntitySpawnerSlot/EntitySpawnerSlot_Character.et",
			"Prefabs/Systems/Logic/CharacterSpawnerSlot.et"
		};
		foreach (int index, ResourceName path : paths)
		{
			Resource resource = Resource.Load(path);
			IEntitySource source;
			if (resource && resource.IsValid()) source = SCR_BaseContainerTools.FindEntitySource(resource);
			if (!source)
			{
				AICF_AIQuotaCatalogProbe.Log("RESOURCE_SOURCE", string.Format("index=%1 path=%2 source_present=0 observation_only=1", index, path));
				continue;
			}
			string fileName = string.Format("$profile:AICF_CatalogMetadata_%1.et", index);
			bool saved = BaseContainerTools.SaveContainer(source, string.Empty, fileName);
			int count = source.GetComponentCount();
			AICF_AIQuotaCatalogProbe.Log("RESOURCE_SOURCE", string.Format("index=%1 path=%2 source_present=1 saved=%3 file=%4 components=%5 observation_only=1", index, path, saved, fileName, count));
			for (int componentIndex; componentIndex < count && componentIndex < 32; componentIndex++)
			{
				IEntityComponentSource component = source.GetComponent(componentIndex);
				if (!component) continue;
				AICF_AIQuotaCatalogProbe.Log("RESOURCE_SOURCE_COMPONENT", string.Format("resource_index=%1 component_index=%2 type=%3 observation_only=1", index, componentIndex, component.GetClassName()));
			}
		}
	}

	static void Links(SCR_ResourceGenerator generator, SCR_ResourceConsumer consumer, string stage)
	{
		SCR_ResourceContainerQueueBase generated;
		SCR_ResourceContainerQueueBase consumed;
		if (generator) generated = generator.GetContainerQueue();
		if (consumer) consumed = consumer.GetContainerQueue();
		int generatorCount;
		int consumerCount;
		if (generated) generatorCount = generated.GetContainerCount();
		if (consumed) consumerCount = consumed.GetContainerCount();
		bool complete = generated && consumed && generatorCount <= 32 && consumerCount <= 32;
		array<SCR_ResourceContainer> generatorContainers = {};
		for (int g; g < generatorCount && g < 32; g++)
		{
			SCR_ResourceContainer container = generated.GetContainerAt(g);
			if (!container) complete = false;
			if (container) generatorContainers.Insert(container);
			Container(container, generator, consumer, stage, "GENERATOR", g);
		}
		int intersection;
		for (int c; c < consumerCount && c < 32; c++)
		{
			SCR_ResourceContainer consumedContainer = consumed.GetContainerAt(c);
			if (!consumedContainer) complete = false;
			if (consumedContainer && generatorContainers.Contains(consumedContainer)) intersection++;
			Container(consumedContainer, generator, consumer, stage, "CONSUMER", c);
		}
		string summary = string.Format("stage=%1 generator_containers=%2 consumer_containers=%3 exact_pointer_intersection=%4", stage, generatorCount, consumerCount, intersection);
		summary += string.Format(" generator_queue_present=%1 consumer_queue_present=%2 scan_complete=%3 observation_only=1", generated != null, consumed != null, complete);
		AICF_AIQuotaCatalogProbe.Log("RESOURCE_LINK_SUMMARY", summary);
	}

	static void Container(SCR_ResourceContainer container, SCR_ResourceGenerator generator, SCR_ResourceConsumer consumer, string stage, string queue, int index)
	{
		if (!container)
		{
			AICF_AIQuotaCatalogProbe.Log("RESOURCE_CONTAINER", string.Format("stage=%1 queue=%2 index=%3 present=0 observation_only=1", stage, queue, index));
			return;
		}
		EntityID ownerId;
		IEntity owner = container.GetOwner();
		if (owner) ownerId = owner.GetID();
		bool linkedGenerator;
		bool linkedConsumer;
		if (generator) linkedGenerator = container.IsInteractorLinked(generator);
		if (consumer) linkedConsumer = container.IsInteractorLinked(consumer);
		string details = string.Format("stage=%1 queue=%2 index=%3 present=1 container=%4 owner=%5 type=%6", stage, queue, index, container, ownerId, container.GetResourceType());
		details += string.Format(" value=%1 max=%2 free=%3 linked_generator=%4 linked_consumer=%5 observation_only=1", container.GetResourceValue(), container.GetMaxResourceValue(), container.ComputeResourceDifference(), linkedGenerator, linkedConsumer);
		AICF_AIQuotaCatalogProbe.Log("RESOURCE_CONTAINER", details);
	}
}

modded class SCR_ResourceGeneratorServicePoint
{
	override SCR_ResourceGenerationResponse RequestGeneration(float resourceAmount, SCR_ResourceGenerator generator = null)
	{
		bool target = this == AICF_AIQuotaCatalogResourceObservation.s_Generator;
		SCR_ResourceConsumer consumer = AICF_AIQuotaCatalogResourceObservation.s_Consumer;
		int resourceType = -1;
		float storageRange;
		float multiplier;
		string actions;
		if (target)
		{
			resourceType = GetResourceType();
			storageRange = GetStorageRange();
			multiplier = GetResourceMultiplier();
			int actionIndex;
			if (m_aActions != null)
			{
				foreach (SCR_ResourceGeneratorActionBase action : m_aActions)
				{
					if (++actionIndex > 16) break;
					if (action) actions += action.Type().ToString() + ",";
				}
			}
		}
		SCR_ResourceGenerationResponse response = super.RequestGeneration(resourceAmount, generator);
		if (!target) return response;
		int reason = -1;
		float space = -1;
		if (response) reason = response.GetReason();
		if (response) space = response.GetAvailableResourceSpace();
		string details = string.Format("response_present=%1 reason=%2 available_space=%3 requested=%4 type=%5", response != null, reason, space, resourceAmount, resourceType);
		details += string.Format(" storage_range_before=%1 multiplier_before=%2 actions_before=%3 native_calls=1 original_response_returned=1 observation_only=1", storageRange, multiplier, actions);
		AICF_AIQuotaCatalogProbe.Log("RESOURCE_GENERATION_RESPONSE", details);
		AICF_AIQuotaCatalogResourceObservation.Links(this, consumer, "AFTER_NATIVE_GENERATION_RETURN");
		return response;
	}
}

// Diagnostic projection only: every overridden method returns its single real
// super result. Telemetry never grants quota or repeats BuildCounts/Capture.
class AICF_AIQuotaCatalogCensusObservation
{
	static int s_CaptureAt;
	static int s_CustodyAt;
	static int s_CountsAt;

	static bool Due(int previous)
	{
		return AICF_AIQuotaCatalogProbe.Enabled() && System.GetTickCount(previous) >= 15000;
	}

	static void EntityState(IEntity entity, string captured, AIAgent expectedAgent = null, RplComponent expectedRpl = null)
	{
		EntityID actualId;
		RplComponent rpl;
		bool master;
		string actualRpl = "UNKNOWN";
		int player = -1;
		if (entity) actualId = entity.GetID();
		if (entity) rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (rpl) master = rpl.IsMaster();
		if (rpl) actualRpl = rpl.Id().ToString();
		PlayerManager players;
		if (GetGame()) players = GetGame().GetPlayerManager();
		if (players && entity) player = players.GetPlayerIdFromControlledEntity(entity);
		Print(string.Format("[AICF][QUOTA_CATALOG_CENSUS_ENTITY] captured=%1 present=%2 actual_id=%3 actual_rpl=%4 master=%5 player=%6 observation_after_decision=1", captured, entity != null, actualId, actualRpl, master, player));
		AIControlComponent control;
		AIAgent actualAgent;
		EntityID agentId;
		if (entity) control = AIControlComponent.Cast(entity.FindComponent(AIControlComponent));
		if (control) actualAgent = control.GetControlAIAgent();
		if (actualAgent) agentId = actualAgent.GetID();
		Print(string.Format("[AICF][QUOTA_CATALOG_CENSUS_CONTROL] captured=%1 control=%2 actual_agent=%3 expected_known=%4 exact_expected=%5 observation_after_decision=1", captured, control != null, agentId, expectedAgent != null, actualAgent == expectedAgent));
		bool nativeActive;
		bool nativeCounted;
		if (actualAgent) nativeActive = actualAgent.IsAIActivated();
		if (actualAgent) nativeCounted = actualAgent.CountTowardsAIWorldLimit();
		Print(string.Format("[AICF][QUOTA_CATALOG_CENSUS_AGENT_FLAGS] captured=%1 agent_present=%2 active=%3 counted=%4 captured_rpl_present=%5 rpl_pointer_same=%6 observation_after_decision=1", captured, actualAgent != null, nativeActive, nativeCounted, expectedRpl != null, rpl == expectedRpl));
		ChimeraCharacter actor = ChimeraCharacter.Cast(entity);
		CharacterControllerComponent characterController;
		PlayerController playerController;
		bool nativePlayer;
		bool mapsBody;
		int mainPlayer = -1;
		if (actor) characterController = actor.GetCharacterController();
		if (characterController) nativePlayer = characterController.IsPlayerControlled();
		if (actor) mainPlayer = SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(actor);
		if (players && player > 0) playerController = players.GetPlayerController(player);
		if (playerController) mapsBody = playerController.GetControlledEntity() == entity;
		Print(string.Format("[AICF][QUOTA_CATALOG_CENSUS_HUMAN] captured=%1 player_flag=%2 pm_player=%3 main_player=%4 pm_maps_exact_body=%5 observation_after_decision=1", captured, nativePlayer, player, mainPlayer, mapsBody));
	}
}

modded class AICF_AIQuotaCensus
{
	override bool Capture(SCR_AIWorld world)
	{
		bool actual = super.Capture(world);
		if (!actual && AICF_AIQuotaCatalogCensusObservation.Due(AICF_AIQuotaCatalogCensusObservation.s_CaptureAt))
		{
			AICF_AIQuotaCatalogCensusObservation.s_CaptureAt = System.GetTickCount();
			Print(string.Format("[AICF][QUOTA_CATALOG_CENSUS_CAPTURE] actual=0 reason=%1 captured_rows=%2 observation_after_decision=1", GetReason(), m_aRows.Count()));
			int activeRows = 0;
			int countedWithoutEntity = 0;
			int countedWithoutCapturedEntity = 0;
			int summarized = 0;
			foreach (AICF_AIQuotaAgentRow row : m_aRows)
			{
				if (!row) continue;
				if (row.m_bCounted && row.m_bActivated) activeRows++;
				if (row.m_bCounted && !row.m_Entity) countedWithoutEntity++;
				if (row.m_bCounted && row.m_EntityId == EntityID.INVALID) countedWithoutCapturedEntity++;
				if (summarized >= 16) continue;
				summarized++;
				string captured = string.Format("ROW_%1", row.m_EntityId);
				Print(string.Format("[AICF][QUOTA_CATALOG_CAPTURED_ROW] captured_entity=%1 captured_agent=%2 counted=%3 activated=%4 observation_after_decision=1", row.m_EntityId, row.m_AgentId, row.m_bCounted, row.m_bActivated));
				AICF_AIQuotaCatalogCensusObservation.EntityState(row.m_Entity, captured, row.m_Agent);
			}
			Print(string.Format("[AICF][QUOTA_CATALOG_CENSUS_ROWS] stored_native_active=%1 stored_native_intended=%2 counted_active_flags=%3 counted_no_entity_now=%4 counted_no_captured_entity=%5 summarized=%6 summary_complete=%7 counter_negative_means_unknown=1 observation_after_decision=1", GetNativeActive(), GetNativeIntended(), activeRows, countedWithoutEntity, countedWithoutCapturedEntity, summarized, summarized == m_aRows.Count()));
			int afterActive = -1;
			int afterIntended = -1;
			if (world) afterActive = world.GetCurrentNumOfActiveAIs();
			if (world) afterIntended = world.GetActiveGroupIntendedCount();
			Print(string.Format("[AICF][QUOTA_CATALOG_CENSUS_NATIVE_AFTER] active=%1 intended=%2 counter_scope=AFTER_DECISION_NOT_CAPTURED", afterActive, afterIntended));
		}
		return actual;
	}
}

modded class AICF_AIWorldQuotaService
{
	override protected bool ReadQuotaCharacterCustody(AICF_AIQuotaAgentRow row, out bool hold)
	{
		bool actual = super.ReadQuotaCharacterCustody(row, hold);
		if (!actual && AICF_AIQuotaCatalogCensusObservation.Due(AICF_AIQuotaCatalogCensusObservation.s_CustodyAt))
		{
			AICF_AIQuotaCatalogCensusObservation.s_CustodyAt = System.GetTickCount();
			if (row)
			{
				Print(string.Format("[AICF][QUOTA_CATALOG_CUSTODY] actual=0 hold=%1 captured_agent=%2 captured_entity=%3 counted=%4 activated=%5 observation_after_decision=1", hold, row.m_AgentId, row.m_EntityId, row.m_bCounted, row.m_bActivated));
				AICF_AIQuotaCatalogCensusObservation.EntityState(row.m_Entity, "ROW", row.m_Agent);
			}
			else Print("[AICF][QUOTA_CATALOG_CUSTODY] actual=0 row=NONE observation_after_decision=1");
		}
		return actual;
	}

	override protected bool BuildCounts(AICF_AIQuotaCensus snapshot, array<int> counts)
	{
		bool actual = super.BuildCounts(snapshot, counts);
		if (!actual && AICF_AIQuotaCatalogCensusObservation.Due(AICF_AIQuotaCatalogCensusObservation.s_CountsAt))
		{
			AICF_AIQuotaCatalogCensusObservation.s_CountsAt = System.GetTickCount();
			Print(string.Format("[AICF][QUOTA_CATALOG_COUNTS] actual=0 stored_bodies=%1 observation_after_decision=1", m_aBodies.Count()));
			int inspected = 0;
			int badCount = 0;
			int badLogged = 0;
			int currentLogged = 0;
			int storedCount = m_aBodies.Count();
			foreach (AICF_AIQuotaBody body : m_aBodies)
			{
				if (++inspected > AICF_AIQuotaMath.MAX_TOTAL_UNITS) break;
				if (!body) continue;
				string captured = body.Key();
				bool current = body.MatchesIdentity();
				if (!body) continue;
				bool bad = !current || body.m_bReleased || body.m_bQuarantined;
				if (bad) badCount++;
				if (bad && badLogged >= 24) continue;
				if (!bad && currentLogged >= 16) continue;
				if (bad) badLogged++;
				else currentLogged++;
				Print(string.Format("[AICF][QUOTA_CATALOG_STORED_BODY] captured=%1 bucket=%2 current=%3 released=%4 quarantined=%5 observation_after_decision=1", captured, body.m_Bucket, current, body.m_bReleased, body.m_bQuarantined));
				AICF_AIQuotaCatalogCensusObservation.EntityState(body.m_Entity, captured, null, body.m_Rpl);
			}
			Print(string.Format("[AICF][QUOTA_CATALOG_BODY_SCAN] stored=%1 examined=%2 bad=%3 bad_logged=%4 current_examples=%5 scan_complete=%6 observation_after_decision=1", storedCount, inspected, badCount, badLogged, currentLogged, inspected == storedCount));
		}
		return actual;
	}
}
