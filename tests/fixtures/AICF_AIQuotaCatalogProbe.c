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
	static ref AICF_QuotaCatalogVehicleSelection s_VehicleSelection;
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
	static bool s_FundingConfirmed;
	static SCR_CatalogEntitySpawnerComponent s_FundingSpawner;
	static SCR_EntityCatalogEntry s_FundingEntry;
	static ref AICF_AIQuotaCatalogIdentity s_FundingProducer;
	static SCR_ResourceComponent s_FundingResource;
	static SCR_ResourceConsumer s_FundingConsumer;
	static SCR_ResourceGenerator s_FundingGenerator;
	static int s_ResourceSerial;
	static int s_ResourceAck;
	static int s_ResourceLast;
	static float s_FundingBefore;
	static float s_FundingShortfall;
	static int s_FundingQuote;
	static string s_FundingGeneratorType;
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
		AICF_QuotaCatalogSlotObserver.Stop();
		if (s_Player) s_Player.AICF_QuotaCatalogResourceStop(s_ResourceSerial);
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
		AICF_QuotaCatalogDepotSetup.Stop();
		AICF_QuotaCatalogSlotObserver.Stop();
		s_VehicleSelection = null;
		if (s_Player && s_ResourceSerial) s_Player.AICF_QuotaCatalogResourceStop(s_ResourceSerial);
		AICF_AIQuotaCatalogResourceClient.Stop();
		AICF_AIQuotaCatalogResourceClient.ClearNotes();
		if (s_Service && s_Pressure) s_Service.CancelReservation(s_Pressure, s_Serial);
		s_Pressure = null;
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(Close);
		AICF_AIQuotaCatalogResourceObservation.Stop();
		s_FundingSpawner = null;
		s_FundingEntry = null;
		s_FundingProducer = null;
		s_FundingResource = null;
		s_FundingConsumer = null;
		s_FundingGenerator = null;
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
			if (s_Finished) return;
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
		if (!AICF_QuotaCatalogDepotSetup.Ready()) return;
		if (s_FundingSpawner)
		{
			if (!s_FundingSpawner.AICF_QuotaProbeFund(s_FundingEntry, s_User.m_Entity, s_FundingQuote)) return;
			if (s_Finished) return;
		}
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
			if (!AICF_QuotaCatalogDepotSetup.AllowsSpawner(spawner)) continue;
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
		if (s_Mode == "vehicle" && (!s_VehicleSelection || s_VehicleSelection.m_Slot != s_Slot ||
			!s_VehicleSelection.IsCurrent(s_Spawner, s_Entry, s_User.m_Entity, s_VehicleSelection.m_iQuote)))
		{ Finish(false, "VEHICLE_SELECTION_STALE_BEFORE_REQUEST"); return; }
		s_RequestedAt = now;
		s_Spawner.AICF_QuotaProbeRequest(s_Entry, s_User.m_Entity, s_Slot);
	}
}

modded class SCR_CatalogEntitySpawnerComponent
{
	bool AICF_QuotaProbeFundingCurrent(AICF_AIQuotaCatalogIdentity producer, SCR_ResourceComponent resource,
		SCR_ResourceConsumer consumer, SCR_ResourceGenerator generator, SCR_EntityCatalogEntry entry, IEntity user, int quote)
	{
		if (AICF_AIQuotaCatalogProbe.s_Mode == "vehicle" && (!AICF_AIQuotaCatalogProbe.s_VehicleSelection ||
			!AICF_AIQuotaCatalogProbe.s_VehicleSelection.IsCurrent(this, entry, user, quote))) return false;
		if (!Replication.IsServer() || AICF_AIQuotaCatalogProbe.s_Finished || AICF_AIQuotaCatalogProbe.s_RequestedAt) return false;
		if (!producer || !producer.IsCurrent() || GetOwner() != producer.m_Entity || !INSTANCES.Contains(this)) return false;
		if (!resource || GetSpawnerResourceComponent() != resource || resource.GetOwner() != producer.m_Entity) return false;
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
		if (AICF_AIQuotaCatalogProbe.s_Finished) return false;
		if (AICF_AIQuotaCatalogProbe.s_FundingAttempted && !AICF_AIQuotaCatalogProbe.s_FundingSpawner) return false;
		if (AICF_AIQuotaCatalogProbe.s_FundingSpawner)
		{
			if (AICF_AIQuotaCatalogProbe.s_FundingSpawner != this || AICF_AIQuotaCatalogProbe.s_FundingEntry != entry ||
				AICF_AIQuotaCatalogProbe.s_FundingQuote != quote || !AICF_QuotaProbeFundingCurrent(
				AICF_AIQuotaCatalogProbe.s_FundingProducer, AICF_AIQuotaCatalogProbe.s_FundingResource,
				AICF_AIQuotaCatalogProbe.s_FundingConsumer, AICF_AIQuotaCatalogProbe.s_FundingGenerator, entry, user, quote))
			{
				AICF_AIQuotaCatalogProbe.Finish(false, "CONTROLLED_FUNDING_CONTEXT_CHANGED");
				return false;
			}
			if (AICF_AIQuotaCatalogProbe.s_FundingConfirmed) return true;
			if (AICF_AIQuotaCatalogProbe.s_ResourceAck != AICF_AIQuotaCatalogProbe.s_ResourceSerial)
			{
				if (AICF_AIQuotaCatalogProbe.s_ResourceLast != AICF_AIQuotaCatalogProbe.s_ResourceSerial)
				{
					AICF_AIQuotaCatalogProbe.s_ResourceLast = AICF_AIQuotaCatalogProbe.s_ResourceSerial;
					AICF_AIQuotaCatalogProbe.Log("RESOURCE_HANDLES_PENDING", string.Format("serial=%1 ack=%2 funding_spawner_present=%3 generation_attempted=%4", AICF_AIQuotaCatalogProbe.s_ResourceSerial, AICF_AIQuotaCatalogProbe.s_ResourceAck, AICF_AIQuotaCatalogProbe.s_FundingSpawner != null, AICF_AIQuotaCatalogProbe.s_FundingAttempted));
				}
				return false;
			}
			if (AICF_AIQuotaCatalogProbe.s_FundingAttempted) return AICF_QuotaProbeFundingReadback();
			if (AICF_QuotaProbeConfirmAlreadyFunded()) return true;
			if (AICF_AIQuotaCatalogProbe.s_Finished) return false;
		}
		AICF_AIQuotaCatalogIdentity producer = AICF_AIQuotaCatalogIdentity.Capture(GetOwner(), true);
		SCR_ResourceComponent resource = GetSpawnerResourceComponent();
		if (!resource) return false;
		SCR_ResourceConsumer consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		SCR_ResourceGenerator generator = resource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!AICF_QuotaProbeFundingCurrent(producer, resource, consumer, generator, entry, user, quote)) return false;
		float before = GetSpawnerResourceValue();
		float shortfall = quote - before;
		if (!(before >= 0 && before < quote && shortfall > 0 && shortfall <= quote))
		{
			AICF_AIQuotaCatalogResourceClient.Trace("FUND_BEFORE_REJECT", string.Format("producer=%1 before=%2 quote=%3 shortfall=%4 observation_only=1", producer.m_Id, before, quote, shortfall));
			return false;
		}
		if (!AICF_AIQuotaCatalogProbe.s_FundingSpawner)
		{
			RplId resourceId = Replication.FindItemId(resource);
			if (!producer.m_Rpl || !producer.m_Rpl.Id().IsValid() || !resourceId.IsValid()) return false;
			AICF_AIQuotaCatalogProbe.s_FundingSpawner = this;
			AICF_AIQuotaCatalogProbe.s_FundingEntry = entry;
			AICF_AIQuotaCatalogProbe.s_FundingProducer = producer;
			AICF_AIQuotaCatalogProbe.s_FundingResource = resource;
			AICF_AIQuotaCatalogProbe.s_FundingConsumer = consumer;
			AICF_AIQuotaCatalogProbe.s_FundingGenerator = generator;
			AICF_AIQuotaCatalogProbe.s_FundingQuote = quote;
			AICF_AIQuotaCatalogProbe.s_ResourceSerial = AICF_AIQuotaCatalogProbe.s_Service.AllocateContextSerial();
			RplId producerRpcId = producer.m_Rpl.Id();
			AICF_AIQuotaCatalogProbe.Log("RESOURCE_SUBSCRIBE_DISPATCH", string.Format("serial=%1 producer=%2 producer_rpl=%3 resource_rpl=%4 funding_spawner_present=%5", AICF_AIQuotaCatalogProbe.s_ResourceSerial, producer.m_Id, producerRpcId, resourceId, AICF_AIQuotaCatalogProbe.s_FundingSpawner != null));
			AICF_AIQuotaCatalogProbe.s_Player.AICF_QuotaCatalogResourceSubscribe(producerRpcId, resourceId, AICF_AIQuotaCatalogProbe.s_ResourceSerial);
			return false;
		}
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
		AICF_AIQuotaCatalogProbe.s_FundingBefore = before;
		AICF_AIQuotaCatalogProbe.s_FundingShortfall = shortfall;
		AICF_AIQuotaCatalogProbe.s_FundingGeneratorType = generatorType;
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
		return AICF_QuotaProbeFundingReadback();
	}

	bool AICF_QuotaProbeConfirmAlreadyFunded()
	{
		AICF_AIQuotaCatalogIdentity producer = AICF_AIQuotaCatalogProbe.s_FundingProducer;
		AICF_AIQuotaCatalogIdentity user = AICF_AIQuotaCatalogProbe.s_User;
		SCR_EntityCatalogEntry entry = AICF_AIQuotaCatalogProbe.s_FundingEntry;
		SCR_ResourceComponent resource = AICF_AIQuotaCatalogProbe.s_FundingResource;
		SCR_ResourceConsumer consumer = AICF_AIQuotaCatalogProbe.s_FundingConsumer;
		SCR_ResourceGenerator generator = AICF_AIQuotaCatalogProbe.s_FundingGenerator;
		SCR_PlayerController player = AICF_AIQuotaCatalogProbe.s_Player;
		int serial = AICF_AIQuotaCatalogProbe.s_ResourceSerial;
		int quote = AICF_AIQuotaCatalogProbe.s_FundingQuote;
		if (!producer || !user || !player || serial <= 0 || quote <= 0 ||
			AICF_AIQuotaCatalogProbe.s_FundingSpawner != this || AICF_AIQuotaCatalogProbe.s_FundingAttempted ||
			AICF_AIQuotaCatalogProbe.s_FundingConfirmed || AICF_AIQuotaCatalogProbe.s_Finished ||
			AICF_AIQuotaCatalogProbe.s_RequestedAt || AICF_AIQuotaCatalogProbe.s_ResourceAck != serial)
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "ALREADY_FUNDED_CONTEXT_UNAVAILABLE");
			return false;
		}
		float available = GetSpawnerResourceValue();
		if (!AICF_QuotaProbeFundingCurrent(producer, resource, consumer, generator, entry, user.m_Entity, quote))
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "ALREADY_FUNDED_CONTEXT_CHANGED");
			return false;
		}
		// Pure repeat after all native getter/callback boundaries; never confirm a successor.
		if (AICF_AIQuotaCatalogProbe.s_Finished || AICF_AIQuotaCatalogProbe.s_RequestedAt ||
			AICF_AIQuotaCatalogProbe.s_FundingSpawner != this || AICF_AIQuotaCatalogProbe.s_FundingAttempted ||
			AICF_AIQuotaCatalogProbe.s_FundingConfirmed || AICF_AIQuotaCatalogProbe.s_Player != player ||
			AICF_AIQuotaCatalogProbe.s_User != user || AICF_AIQuotaCatalogProbe.s_FundingProducer != producer ||
			AICF_AIQuotaCatalogProbe.s_FundingEntry != entry || AICF_AIQuotaCatalogProbe.s_FundingResource != resource ||
			AICF_AIQuotaCatalogProbe.s_FundingConsumer != consumer || AICF_AIQuotaCatalogProbe.s_FundingGenerator != generator ||
			AICF_AIQuotaCatalogProbe.s_ResourceSerial != serial || AICF_AIQuotaCatalogProbe.s_ResourceAck != serial ||
			AICF_AIQuotaCatalogProbe.s_FundingQuote != quote)
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "ALREADY_FUNDED_CONTEXT_STALE");
			return false;
		}
		if (!(available >= quote && available <= 1000000000)) return false;
		AICF_AIQuotaCatalogProbe.s_FundingConfirmed = true;
		AICF_AIQuotaCatalogProbe.Log("NATIVE_ALREADY_FUNDED", string.Format("producer=%1 serial=%2 available=%3 effective_cost=%4 amount=0 generation_attempted=0 native_consumer=1 fixture_only=1", producer.m_Id, serial, available, quote));
		return true;
	}

	bool AICF_QuotaProbeFundingReadback()
	{
		// Only cached consumer reads after the single native generation request.
		AICF_AIQuotaCatalogIdentity producer = AICF_AIQuotaCatalogProbe.s_FundingProducer;
		AICF_AIQuotaCatalogIdentity user = AICF_AIQuotaCatalogProbe.s_User;
		SCR_EntityCatalogEntry entry = AICF_AIQuotaCatalogProbe.s_FundingEntry;
		SCR_ResourceComponent resource = AICF_AIQuotaCatalogProbe.s_FundingResource;
		SCR_ResourceConsumer consumer = AICF_AIQuotaCatalogProbe.s_FundingConsumer;
		SCR_ResourceGenerator generator = AICF_AIQuotaCatalogProbe.s_FundingGenerator;
		SCR_PlayerController player = AICF_AIQuotaCatalogProbe.s_Player;
		int serial = AICF_AIQuotaCatalogProbe.s_ResourceSerial;
		int quote = AICF_AIQuotaCatalogProbe.s_FundingQuote;
		float before = AICF_AIQuotaCatalogProbe.s_FundingBefore;
		float shortfall = AICF_AIQuotaCatalogProbe.s_FundingShortfall;
		float after = GetSpawnerResourceValue();
		if (AICF_AIQuotaCatalogProbe.s_FundingSpawner != this || !AICF_AIQuotaCatalogProbe.s_FundingAttempted ||
			AICF_AIQuotaCatalogProbe.s_FundingProducer != producer || AICF_AIQuotaCatalogProbe.s_User != user ||
			AICF_AIQuotaCatalogProbe.s_FundingEntry != entry || AICF_AIQuotaCatalogProbe.s_FundingResource != resource ||
			AICF_AIQuotaCatalogProbe.s_FundingConsumer != consumer || AICF_AIQuotaCatalogProbe.s_FundingGenerator != generator ||
			AICF_AIQuotaCatalogProbe.s_ResourceSerial != serial || AICF_AIQuotaCatalogProbe.s_ResourceAck != serial ||
			!AICF_AIQuotaCatalogProbe.CurrentUser())
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "CONTROLLED_FUNDING_READBACK_PLAYER_CHANGED");
			return false;
		}
		if (!AICF_QuotaProbeFundingCurrent(producer, resource, consumer, generator, entry,
			user.m_Entity, quote))
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "CONTROLLED_FUNDING_READBACK_CONTEXT_CHANGED");
			return false;
		}
		if (!(after >= 0 && after <= 1000000000))
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "CONTROLLED_NATIVE_FUNDING_NONFINITE");
			return false;
		}
		if (after < quote) return false;
		if (!(Math.AbsFloat(after - before - shortfall) <= 0.01))
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "CONTROLLED_NATIVE_FUNDING_DELTA_MISMATCH");
			return false;
		}
		// Pure final comparison after all getter/callback boundaries; never confirm a successor.
		if (AICF_AIQuotaCatalogProbe.s_Finished || AICF_AIQuotaCatalogProbe.s_FundingSpawner != this ||
			AICF_AIQuotaCatalogProbe.s_Player != player || AICF_AIQuotaCatalogProbe.s_User != user ||
			AICF_AIQuotaCatalogProbe.s_FundingProducer != producer || AICF_AIQuotaCatalogProbe.s_FundingEntry != entry ||
			AICF_AIQuotaCatalogProbe.s_FundingResource != resource || AICF_AIQuotaCatalogProbe.s_FundingConsumer != consumer ||
			AICF_AIQuotaCatalogProbe.s_FundingGenerator != generator || AICF_AIQuotaCatalogProbe.s_ResourceSerial != serial ||
			AICF_AIQuotaCatalogProbe.s_ResourceAck != serial || AICF_AIQuotaCatalogProbe.s_FundingQuote != quote ||
			AICF_AIQuotaCatalogProbe.s_FundingBefore != before || AICF_AIQuotaCatalogProbe.s_FundingShortfall != shortfall)
		{
			AICF_AIQuotaCatalogProbe.Finish(false, "CONTROLLED_FUNDING_READBACK_STALE");
			return false;
		}
		AICF_AIQuotaCatalogProbe.s_FundingConfirmed = true;
		AICF_AIQuotaCatalogProbe.Log("CONTROLLED_RESOURCE_FUNDING", string.Format("producer=%1 generator_type=%2 before=%3 amount=%4 after=%5 effective_cost=%6 native_generator=1 fixture_only=1", AICF_AIQuotaCatalogProbe.s_FundingProducer.m_Id, AICF_AIQuotaCatalogProbe.s_FundingGeneratorType, AICF_AIQuotaCatalogProbe.s_FundingBefore, AICF_AIQuotaCatalogProbe.s_FundingShortfall, after, AICF_AIQuotaCatalogProbe.s_FundingQuote));
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
		if (type == EEntityCatalogType.VEHICLE) return AICF_QuotaProbeSelectVehicle(user, selected, slot);
		if (AICF_AIQuotaCatalogProbe.s_FundingSpawner && AICF_AIQuotaCatalogProbe.s_FundingSpawner != this) return false;
		int checked;
		int eligible = 0;
		foreach (SCR_EntityCatalogEntry entry : m_aAssetList)
		{
			if (++checked > 256) return false;
			if (AICF_AIQuotaCatalogProbe.s_FundingEntry && AICF_AIQuotaCatalogProbe.s_FundingEntry != entry) continue;
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
				if (AICF_AIQuotaCatalogProbe.s_FundingSpawner && !AICF_AIQuotaCatalogProbe.s_FundingConfirmed) return false;
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
		AICF_QuotaCatalogSlotFrame frame;
		if (AICF_AIQuotaCatalogProbe.s_Mode == "vehicle")
		{
			AICF_QuotaCatalogVehicleSelection candidate = AICF_AIQuotaCatalogProbe.s_VehicleSelection;
			if (!candidate || candidate.m_Slot != slot || !candidate.IsCurrent(this, entry, user, candidate.m_iQuote))
			{ AICF_AIQuotaCatalogProbe.Finish(false, "VEHICLE_SELECTION_STALE_AT_REQUEST"); return; }
			frame = AICF_QuotaCatalogSlotObserver.Begin(this, entry, candidate.m_Data, user, "REQUEST");
			if (!frame || AICF_AIQuotaCatalogProbe.s_Finished || AICF_AIQuotaCatalogProbe.s_VehicleSelection != candidate || !candidate.IsCurrent(this, entry, user, candidate.m_iQuote))
			{ AICF_AIQuotaCatalogProbe.Finish(false, "VEHICLE_REQUEST_SCOPE_CHANGED"); return; }
		}
		InitiateSpawn(entry, GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user), slot);
		AICF_QuotaCatalogSlotObserver.End(frame, null);
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

	bool AICF_QuotaProbeSlotCurrent(SCR_EntitySpawnerSlotComponent slot)
	{
		return slot && INSTANCES.Contains(this) && (m_aChildSlots.Contains(slot) || m_aNearSlots.Contains(slot));
	}

	bool AICF_QuotaProbeUnfundedVehicleEligible(SCR_EntityCatalogEntry entry, IEntity user)
	{
		if (!entry || !m_aAssetList.Contains(entry) || GetRequesterFaction(user) != GetFaction()) return false;
		if (SCR_GameModeCampaign.GetInstance())
		{
			if (!RankCheck(entry, user) || !CooldownCheck(user)) return false;
		}
		return AICF_AIQuotaCatalogProbe.CurrentUser() && !AICF_AIQuotaCatalogProbe.s_Finished;
	}

	bool AICF_QuotaProbeSelectVehicle(IEntity user, out SCR_EntityCatalogEntry selected, out SCR_EntitySpawnerSlotComponent slot)
	{
		if (AICF_AIQuotaCatalogProbe.s_FundingSpawner && AICF_AIQuotaCatalogProbe.s_FundingSpawner != this) return false;
		int checked;
		foreach (SCR_EntityCatalogEntry entry : m_aAssetList)
		{
			if (++checked > 256) return false;
			if (AICF_AIQuotaCatalogProbe.s_FundingEntry && AICF_AIQuotaCatalogProbe.s_FundingEntry != entry) continue;
			if (!entry || !entry.GetCatalogParent() || entry.GetCatalogParent().GetCatalogType() != EEntityCatalogType.VEHICLE) continue;
			SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
			if (!data || data.GetSupplyCost() <= 0 || data.GetEntityCount() <= 0 || data.GetEntityCount() > 20) continue;
			SCR_EEntityRequestStatus nativeState = GetRequestState(entry, user);
			if (nativeState != SCR_EEntityRequestStatus.CAN_SPAWN && nativeState != SCR_EEntityRequestStatus.NOT_ENOUGH_SUPPLIES) continue;
			if (!AICF_QuotaProbeUnfundedVehicleEligible(entry, user)) continue;
			AICF_QuotaCatalogSlotFrame frame = AICF_QuotaCatalogSlotObserver.Begin(this, entry, data, user, "SELECTION");
			if (!frame || AICF_AIQuotaCatalogProbe.s_Finished || !AICF_AIQuotaCatalogProbe.CurrentUser()) return false;
			SCR_EntitySpawnerSlotComponent freeSlot = GetFreeSlot(data);
			AICF_QuotaCatalogSlotObserver.End(frame, freeSlot);
			if (AICF_AIQuotaCatalogProbe.s_Finished || !AICF_AIQuotaCatalogProbe.CurrentUser()) return false;
			AICF_QuotaCatalogVehicleSelection candidate = AICF_AIQuotaCatalogProbe.s_VehicleSelection;
			if (AICF_AIQuotaCatalogProbe.s_FundingSpawner)
			{
				if (!candidate || candidate.m_Slot != freeSlot || !candidate.IsCurrent(this, entry, user, data.GetSupplyCost()))
				{ AICF_AIQuotaCatalogProbe.Finish(false, "BOUND_NATIVE_SLOT_CHANGED"); return false; }
			}
			else
			{
				if (!freeSlot) continue;
				candidate = AICF_QuotaCatalogVehicleSelection.Capture(this, entry, user, freeSlot);
				if (!candidate) continue;
				AICF_AIQuotaCatalogProbe.s_VehicleSelection = candidate;
			}
			if (nativeState == SCR_EEntityRequestStatus.NOT_ENOUGH_SUPPLIES && !AICF_AIQuotaCatalogProbe.s_FundingAttempted)
			{
				if (AICF_QuotaProbeFund(entry, user, data.GetSupplyCost())) nativeState = GetRequestState(entry, user);
				if (AICF_AIQuotaCatalogProbe.s_Finished) return false;
				if (AICF_AIQuotaCatalogProbe.s_FundingSpawner && !AICF_AIQuotaCatalogProbe.s_FundingConfirmed) return false;
			}
			if (nativeState != SCR_EEntityRequestStatus.CAN_SPAWN)
			{
				if (!AICF_AIQuotaCatalogProbe.s_FundingSpawner) AICF_AIQuotaCatalogProbe.s_VehicleSelection = null;
				continue;
			}
			if (AICF_AIQuotaCatalogProbe.s_VehicleSelection != candidate || !candidate.IsCurrent(this, entry, user, data.GetSupplyCost())) return false;
			selected = entry;
			slot = freeSlot;
			return true;
		}
		return false;
	}


	override protected SCR_EntitySpawnerSlotComponent GetFreeSlotFromArray(notnull array<SCR_EntitySpawnerSlotComponent> slots, notnull SCR_EntityCatalogSpawnerData spawnerData, notnull array<SCR_EntitySpawnerSlotComponent> occupiedSlots)
	{
		AICF_QuotaCatalogSlotFrame frame = AICF_QuotaCatalogSlotObserver.s_Frame;
		string source = "OTHER";
		if (slots == m_aChildSlots) source = "CHILD";
		else if (slots == m_aNearSlots) source = "NEAR";
		int count = slots.Count();
		int occupied = occupiedSlots.Count();
		if (frame) frame.m_OccupiedSnapshot = occupiedSlots;
		SCR_EntitySpawnerSlotComponent result = super.GetFreeSlotFromArray(slots, spawnerData, occupiedSlots);
		if (frame)
		{
			SCR_SlotServiceComponentClass serviceData = SCR_SlotServiceComponentClass.Cast(GetComponentData(GetOwner()));
			float radius;
			if (serviceData) radius = serviceData.GetMaxSlotDistance();
			AICF_QuotaCatalogSlotObserver.Write(frame, "RANGE_METADATA", string.Format("range_known=%1 effective_max_slot_distance=%2 near_range_rule=STRICT_XZ child_range_rule=UNRESTRICTED metadata_snapshot_after_native=1", serviceData != null, radius));
		}
		AICF_QuotaCatalogSlotObserver.Write(frame, "ARRAY", string.Format("source=%1 registered_count_before=%2 occupied_cache_count_before=%3 returned=%4 native_order_unchanged=1", source, count, occupied, result != null));
		return result;
	}
	override protected SCR_EntitySpawnerSlotComponent GetLastKnownSuitableSlot(notnull SCR_EntityCatalogSpawnerData spawnerData)
	{
		AICF_QuotaCatalogSlotFrame frame = AICF_QuotaCatalogSlotObserver.s_Frame;
		SCR_EntitySpawnerSlotComponent result = super.GetLastKnownSuitableSlot(spawnerData);
		AICF_QuotaCatalogSlotObserver.Write(frame, "FREE_CACHE", string.Format("native_cache_hit=%1 fresh_occupancy_claim=0", result != null));
		return result;
	}

}

modded class SCR_PlayerController
{
	void AICF_QuotaCatalogResourceSubscribe(RplId producer, RplId resource, int serial) { Rpc(RpcDo_AICFQuotaCatalogResourceSubscribe, producer, resource, serial); }
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFQuotaCatalogResourceSubscribe(RplId producer, RplId resource, int serial)
	{
		AICF_AIQuotaCatalogResourceClient.Trace("RPC_RECEIVED_" + serial.ToString(), string.Format("requested_serial=%1 producer_rpl=%2 resource_rpl=%3", serial, producer, resource));
		AICF_AIQuotaCatalogResourceClient.Start(this, producer, resource, serial);
	}
	void AICF_QuotaCatalogResourceReady(int serial) { Rpc(RpcAsk_AICFQuotaCatalogResourceReady, serial); }
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AICFQuotaCatalogResourceReady(int serial)
	{
		if (!AICF_AIQuotaCatalogProbe.Enabled() || AICF_AIQuotaCatalogProbe.s_Finished) return;
		if (AICF_AIQuotaCatalogProbe.s_Player != this || serial != AICF_AIQuotaCatalogProbe.s_ResourceSerial) return;
		if (!AICF_AIQuotaCatalogProbe.CurrentUser()) return;
		AICF_AIQuotaCatalogProbe.s_ResourceAck = serial;
		AICF_AIQuotaCatalogProbe.Log("RESOURCE_HANDLES_REQUESTED", string.Format("serial=%1 player=%2 client_exact_handles=1 server_registration=UNOBSERVED", serial, GetPlayerId()));
	}
	void AICF_QuotaCatalogResourceStop(int serial) { Rpc(RpcDo_AICFQuotaCatalogResourceStop, serial); }
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AICFQuotaCatalogResourceStop(int serial)
	{
		if (serial == AICF_AIQuotaCatalogResourceClient.s_Serial) AICF_AIQuotaCatalogResourceClient.Stop();
	}
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

// Client observation identity only. It never grants server quota/factory authority.
class AICF_QuotaCatalogClientIdentity
{
	IEntity m_Entity;
	EntityID m_Id;
	BaseWorld m_World;
	RplComponent m_Rpl;
	string m_sRpl;

	static AICF_QuotaCatalogClientIdentity Capture(IEntity entity)
	{
		if (Replication.IsServer() || !GetGame() || !entity || entity.GetWorld() != GetGame().GetWorld()) return null;
		AICF_QuotaCatalogClientIdentity identity = new AICF_QuotaCatalogClientIdentity();
		identity.m_Entity = entity;
		identity.m_Id = entity.GetID();
		identity.m_World = entity.GetWorld();
		identity.m_Rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!identity.m_Rpl) return null;
		RplId rplId = identity.m_Rpl.Id();
		if (!rplId.IsValid()) return null;
		identity.m_sRpl = rplId.ToString();
		if (!identity.IsCurrent()) return null;
		return identity;
	}

	bool IsCurrent()
	{
		if (Replication.IsServer() || !GetGame() || !m_Entity || !m_World || GetGame().GetWorld() != m_World) return false;
		if (m_Id == EntityID.INVALID || m_Entity.GetWorld() != m_World || m_Entity.GetID() != m_Id || m_World.FindEntityByID(m_Id) != m_Entity) return false;
		if (!m_Rpl || m_Entity.FindComponent(RplComponent) != m_Rpl) return false;
		RplId rplId = m_Rpl.Id();
		return rplId.IsValid() && rplId.ToString() == m_sRpl && !m_sRpl.IsEmpty();
	}
}

class AICF_AIQuotaCatalogResourceClient
{
	static SCR_PlayerController s_Player;
	static ref AICF_QuotaCatalogClientIdentity s_User;
	static ref AICF_QuotaCatalogClientIdentity s_Producer;
	static SCR_ResourceComponent s_Resource;
	static SCR_ResourceConsumer s_Consumer;
	static SCR_ResourceGenerator s_Generator;
	static SCR_ResourcePlayerControllerInventoryComponent s_Inventory;
	static RplId s_ResourceId;
	static RplId s_InventoryId;
	static int s_Serial;
	static ref SCR_ResourceSystemSubscriptionHandleBase s_ConsumerHandle;
	static ref SCR_ResourceSystemSubscriptionHandleBase s_GeneratorHandle;
	static ref array<string> s_Notes = {};
	static bool s_bNotesOverflow;

	static void Trace(string stage, string details)
	{
		string mode;
		if (!System.GetCLIParam("aicfQuotaCatalogProbe", mode)) return;
		string key = s_Serial.ToString() + "/" + stage;
		if (s_Notes.Contains(key)) return;
		if (s_Notes.Count() >= 64)
		{
			if (!s_bNotesOverflow) { s_bNotesOverflow = true; Print("[AICF][CATALOG_RESOURCE_CLIENT] stage=OVERFLOW observation_incomplete=1 limit=64"); }
			return;
		}
		s_Notes.Insert(key);
		Print(string.Format("[AICF][CATALOG_RESOURCE_CLIENT] serial=%1 stage=%2 resource_rpl_cached=%3 inventory_rpl_cached=%4 ", s_Serial, stage, s_ResourceId, s_InventoryId) + details);
	}

	static bool Reject(string reason)
	{
		Trace("CURRENT_REJECT_" + reason, "actual_first_false_guard=1 cached_branch=1");
		return false;
	}

	static void ClearNotes() { s_Notes.Clear(); s_bNotesOverflow = false; }

	static bool Current()
	{
		if (!s_Player) return Reject("PLAYER_NULL");
		if (GetGame().GetPlayerController() != s_Player) return Reject("PLAYER_NOT_LOCAL");
		if (!s_User) return Reject("USER_NULL");
		if (!s_User.IsCurrent()) return Reject("USER_IDENTITY");
		if (s_Player.GetControlledEntity() != s_User.m_Entity) return Reject("CONTROLLED_ENTITY");
		if (s_Player.GetPlayerId() <= 0) return Reject("PLAYER_ID");
		if (GetGame().GetPlayerManager().GetPlayerController(s_Player.GetPlayerId()) != s_Player) return Reject("PLAYER_MANAGER");
		if (!s_Producer) return Reject("PRODUCER_NULL");
		if (!s_Producer.IsCurrent()) return Reject("PRODUCER_IDENTITY");
		if (!s_Resource) return Reject("RESOURCE_NULL");
		if (!s_ResourceId.IsValid()) return Reject("RESOURCE_ID_INVALID");
		if (Replication.FindItem(s_ResourceId) != s_Resource) return Reject("RESOURCE_REPLICATION");
		if (s_Resource.GetOwner() != s_Producer.m_Entity) return Reject("RESOURCE_OWNER");
		if (!s_Inventory) return Reject("INVENTORY_NULL");
		if (s_Player.FindComponent(SCR_ResourcePlayerControllerInventoryComponent) != s_Inventory) return Reject("INVENTORY_COMPONENT");
		if (!s_InventoryId.IsValid()) return Reject("INVENTORY_ID_INVALID");
		if (Replication.FindItemId(s_Inventory) != s_InventoryId) return Reject("INVENTORY_REPLICATION");
		if (s_Resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES) != s_Consumer) return Reject("CONSUMER_POINTER");
		if (!s_Consumer) return Reject("CONSUMER_NULL");
		if (!s_Generator) return Reject("GENERATOR_NULL");
		if (s_Resource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES) != s_Generator) return Reject("GENERATOR_POINTER");
		return true;
	}

	static bool ExactHandle(SCR_ResourceSystemSubscriptionHandleBase handle, SCR_ResourceInteractor interactor)
	{
		if (!handle) { Trace("HANDLE_NULL", "cached_branch=1"); return false; }
		if (!interactor) { Trace("HANDLE_INTERACTOR_NULL", "cached_branch=1"); return false; }
		RplId ownerId = handle.GetOwnerRplId();
		if (ownerId != s_InventoryId) { Trace("HANDLE_OWNER_MISMATCH", "actual_owner=" + ownerId.ToString()); return false; }
		RplId resourceId = handle.GetResourceComponentRplId();
		if (resourceId != s_ResourceId) { Trace("HANDLE_RESOURCE_MISMATCH", "actual_resource=" + resourceId.ToString()); return false; }
		typename actualType = handle.GetInteractorType();
		typename expectedType = interactor.Type();
		if (actualType != expectedType) { Trace("HANDLE_TYPE_MISMATCH", string.Format("actual_type=%1 expected_type=%2", actualType, expectedType)); return false; }
		EResourceType resourceType = handle.GetResourceType();
		if (resourceType != EResourceType.SUPPLIES) { Trace("HANDLE_RESOURCE_TYPE_MISMATCH", "actual_resource_type=" + resourceType.ToString()); return false; }
		EResourceGeneratorID identifier = handle.GetResourceIdentifier();
		if (identifier != EResourceGeneratorID.DEFAULT) { Trace("HANDLE_IDENTIFIER_MISMATCH", "actual_identifier=" + identifier.ToString()); return false; }
		Trace("HANDLE_EXACT_" + actualType.ToString(), string.Format("actual_owner=%1 actual_resource=%2 actual_type=%3 expected_type=%4 resource_type=%5 identifier=%6", ownerId, resourceId, actualType, expectedType, resourceType, identifier));
		return true;
	}

	static void Start(SCR_PlayerController player, RplId producerId, RplId resourceId, int serial)
	{
		if (s_Serial) { Trace("START_ALREADY_ACTIVE", "requested_serial=" + serial.ToString()); return; }
		if (!player) { Trace("START_PLAYER_NULL", "cached_branch=1"); return; }
		if (GetGame().GetPlayerController() != player) { Trace("START_PLAYER_NOT_LOCAL", "cached_branch=1"); return; }
		if (serial <= 0) { Trace("START_SERIAL_INVALID", "cached_branch=1"); return; }
		if (!producerId.IsValid()) { Trace("START_PRODUCER_ID_INVALID", "cached_branch=1"); return; }
		if (!resourceId.IsValid()) { Trace("START_RESOURCE_ID_INVALID", "cached_branch=1"); return; }
		s_Player = player;
		s_Serial = serial;
		Trace("START_BOUND", string.Format("producer_rpl=%1 resource_rpl=%2", producerId, resourceId));
		s_User = AICF_QuotaCatalogClientIdentity.Capture(player.GetControlledEntity());
		s_ResourceId = resourceId;
		s_Resource = SCR_ResourceComponent.Cast(Replication.FindItem(resourceId));
		if (s_Resource) s_Producer = AICF_QuotaCatalogClientIdentity.Capture(s_Resource.GetOwner());
		if (!s_Producer) { Trace("START_PRODUCER_CAPTURE_NULL", string.Format("resource_present=%1", s_Resource != null)); Stop(); return; }
		if (!s_Producer.m_Rpl) { Trace("START_PRODUCER_RPL_NULL", "cached_branch=1"); Stop(); return; }
		RplId actualProducerId = s_Producer.m_Rpl.Id();
		if (actualProducerId != producerId) { Trace("START_PRODUCER_RPL_MISMATCH", "actual_producer_rpl=" + actualProducerId.ToString()); Stop(); return; }
		s_Inventory = SCR_ResourcePlayerControllerInventoryComponent.Cast(player.FindComponent(SCR_ResourcePlayerControllerInventoryComponent));
		if (s_Inventory) s_InventoryId = Replication.FindItemId(s_Inventory);
		if (s_Resource) s_Consumer = s_Resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (s_Resource) s_Generator = s_Resource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		Trace("START_COMPONENTS_CACHED", string.Format("user_present=%1 producer_present=%2 resource_present=%3 inventory_present=%4 consumer_present=%5 generator_present=%6", s_User != null, s_Producer != null, s_Resource != null, s_Inventory != null, s_Consumer != null, s_Generator != null));
		if (!Current()) { Trace("START_CURRENT_REJECTED", "actual_current=0"); Stop(); return; }
		s_ConsumerHandle = GetGame().GetResourceSystemSubscriptionManager().RequestSubscriptionListenerHandleGraceful(s_Consumer, s_InventoryId);
		Trace("CONSUMER_HANDLE_RETURN", string.Format("handle_present=%1", s_ConsumerHandle != null));
		if (!Current()) { Trace("START_CURRENT_AFTER_CONSUMER_REJECTED", "actual_current=0"); Stop(); return; }
		s_GeneratorHandle = GetGame().GetResourceSystemSubscriptionManager().RequestSubscriptionListenerHandleGraceful(s_Generator, s_InventoryId);
		Trace("GENERATOR_HANDLE_RETURN", string.Format("handle_present=%1", s_GeneratorHandle != null));
		if (!Current() || !ExactHandle(s_ConsumerHandle, s_Consumer) || !ExactHandle(s_GeneratorHandle, s_Generator)) { Trace("START_HANDLES_REJECTED", "actual_handle_gate=0"); Stop(); return; }
		SCR_ResourceSystemSubscriptionHandleBase consumerHandle = s_ConsumerHandle;
		SCR_ResourceSystemSubscriptionHandleBase generatorHandle = s_GeneratorHandle;
		Tick();
		if (!Current() || s_Serial != serial || s_Player != player || s_ConsumerHandle != consumerHandle || s_GeneratorHandle != generatorHandle) { Trace("START_POST_TICK_SCOPE_REJECTED", "cached_branch=1"); return; }
		Trace("ACK_DISPATCH", "requested_serial=" + serial.ToString());
		player.AICF_QuotaCatalogResourceReady(serial);
		if (s_Serial != serial || s_Player != player || s_ConsumerHandle != consumerHandle || s_GeneratorHandle != generatorHandle) { Trace("START_POST_ACK_SCOPE_REJECTED", "cached_branch=1"); return; }
		GetGame().GetCallqueue().CallLater(Tick, 1000, true);
		Trace("KEEPALIVE_SCHEDULED", "interval_ms=1000");
	}

	static void Tick()
	{
		SCR_ResourceSystemSubscriptionHandleBase consumerHandle = s_ConsumerHandle;
		SCR_ResourceSystemSubscriptionHandleBase generatorHandle = s_GeneratorHandle;
		int serial = s_Serial;
		if (!Current() || !ExactHandle(s_ConsumerHandle, s_Consumer) || !ExactHandle(s_GeneratorHandle, s_Generator)) { Trace("TICK_CURRENT_OR_HANDLE_REJECTED", "cached_branch=1"); Stop(); return; }
		if (s_Serial != serial || s_ConsumerHandle != consumerHandle || s_GeneratorHandle != generatorHandle) { Trace("TICK_PRE_POKE_SCOPE_REJECTED", "cached_branch=1"); return; }
		consumerHandle.Poke();
		if (!Current() || s_Serial != serial || s_ConsumerHandle != consumerHandle || s_GeneratorHandle != generatorHandle) { Trace("TICK_POST_FIRST_POKE_REJECTED", "cached_branch=1"); return; }
		generatorHandle.Poke();
	}

	static void Stop()
	{
		GetGame().GetCallqueue().Remove(Tick);
		SCR_ResourcePlayerControllerInventoryComponent inventory = s_Inventory;
		RplId resourceId = s_ResourceId;
		SCR_ResourceConsumer consumer = s_Consumer;
		SCR_ResourceGenerator generator = s_Generator;
		bool exact = Current();
		Trace("STOP", string.Format("actual_current=%1 consumer_handle_present=%2 generator_handle_present=%3", exact, s_ConsumerHandle != null, s_GeneratorHandle != null));
		s_ConsumerHandle = null;
		s_GeneratorHandle = null;
		s_Player = null;
		s_User = null;
		s_Producer = null;
		s_Resource = null;
		s_Consumer = null;
		s_Generator = null;
		s_Inventory = null;
		s_Serial = 0;
		if (exact && inventory && consumer) inventory.RequestUnsubscription(resourceId, consumer.Type(), EResourceType.SUPPLIES, EResourceGeneratorID.DEFAULT);
		if (exact && inventory && generator) inventory.RequestUnsubscription(resourceId, generator.Type(), EResourceType.SUPPLIES, EResourceGeneratorID.DEFAULT);
		// Graceful manager retention expires normally; no further Poke after Stop.
	}
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
		AICF_QuotaCatalogDepotSetup.Initialize();
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
	static bool ExactSourceResource(Resource resource, ResourceName expected, out ResourceName actualName)
	{
		actualName = string.Empty;
		if (!resource || !resource.IsValid()) return false;
		BaseResourceObject loaded = resource.GetResource();
		if (!loaded) return false;
		actualName = loaded.GetResourceName();
		return actualName == expected;
	}

	static void DumpKnownSources()
	{
		if (!Replication.IsServer() || !GetGame()) return;
		array<ResourceName> paths = {
			"{F8A337C38BB8D4EA}Prefabs/MP/Campaign/RequestTable_US_Barracks.et",
			"{2337DFDD05C26996}Prefabs/MP/Campaign/RequestTable_USSR_Barracks.et",
			"{4B1971606FBC57DF}Prefabs/Systems/Compositions/EntitySpawnerSlot/EntitySpawnerSlot_Character.et",
			"{9540DC5C8AC4E6EE}Prefabs/Systems/Logic/CharacterSpawnerSlot.et"
		};
		foreach (int index, ResourceName path : paths)
		{
			Resource resource = Resource.Load(path);
			ResourceName actualName;
			bool exactName = ExactSourceResource(resource, path, actualName);
			AICF_AIQuotaCatalogProbe.Log("RESOURCE_SOURCE_NAME", string.Format("index=%1 expected=%2 actual=%3 exact_name=%4 observation_only=1", index, path, actualName, exactName));
			if (!exactName) { AICF_AIQuotaCatalogProbe.Finish(false, "METADATA_RESOURCE_IDENTITY_REJECTED"); return; }
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
// Fixture-only immutable candidate. No reservation, registration or physical query.
class AICF_QuotaCatalogVehicleSelection
{
	ref AICF_AIQuotaCatalogIdentity m_Producer;
	ref AICF_AIQuotaCatalogIdentity m_User;
	ref AICF_AIQuotaCatalogIdentity m_SlotOwner;
	SCR_CatalogEntitySpawnerComponent m_Spawner;
	SCR_EntityCatalogEntry m_Entry;
	SCR_EntityCatalogSpawnerData m_Data;
	SCR_EntitySpawnerSlotComponent m_Slot;
	int m_iQuote;

	static AICF_QuotaCatalogVehicleSelection Capture(SCR_CatalogEntitySpawnerComponent spawner, SCR_EntityCatalogEntry entry, IEntity user, SCR_EntitySpawnerSlotComponent slot)
	{
		if (!spawner || !entry || !slot || !AICF_AIQuotaCatalogProbe.CurrentUser()) return null;
		if (!spawner || !entry || !slot) return null;
		AICF_QuotaCatalogVehicleSelection selection = new AICF_QuotaCatalogVehicleSelection();
		selection.m_Spawner = spawner;
		selection.m_Entry = entry;
		selection.m_Slot = slot;
		selection.m_User = AICF_AIQuotaCatalogProbe.s_User;
		selection.m_Producer = AICF_AIQuotaCatalogIdentity.Capture(spawner.GetOwner(), true);
		selection.m_SlotOwner = AICF_AIQuotaCatalogIdentity.Capture(slot.GetOwner(), true);
		selection.m_Data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
		if (!selection.m_Data) return null;
		if (!selection.m_Producer || !selection.m_SlotOwner)
		{
			if (!AICF_AIQuotaCatalogProbe.s_Finished) AICF_AIQuotaCatalogProbe.Log("NATIVE_SLOT_CANDIDATE_REJECT", string.Format("producer_identity_present=%1 slot_owner_identity_present=%2 stage=IDENTITY_CAPTURE native_slot_returned=1", selection.m_Producer != null, selection.m_SlotOwner != null));
			return null;
		}
		selection.m_iQuote = selection.m_Data.GetSupplyCost();
		if (!selection.IsCurrent(spawner, entry, user, selection.m_iQuote)) return null;
		return selection;
	}

	bool IsCurrent(SCR_CatalogEntitySpawnerComponent spawner, SCR_EntityCatalogEntry entry, IEntity user, int quote)
	{
		if (!Replication.IsServer() || AICF_AIQuotaCatalogProbe.s_Finished || !m_User || !m_Producer || !m_SlotOwner) return false;
		if (spawner != m_Spawner || entry != m_Entry || user != m_User.m_Entity || quote <= 0 || quote != m_iQuote) return false;
		if (AICF_AIQuotaCatalogProbe.s_User != m_User || !AICF_AIQuotaCatalogProbe.CurrentUser()) return false;
		if (!m_Producer.IsCurrent() || !m_SlotOwner.IsCurrent() || !m_User.IsCurrent()) return false;
		if (m_Producer.m_Entity.FindComponent(SCR_CatalogEntitySpawnerComponent) != m_Spawner || m_SlotOwner.m_Entity.FindComponent(SCR_EntitySpawnerSlotComponent) != m_Slot) return false;
		if (!m_Spawner || m_Spawner.GetOwner() != m_Producer.m_Entity || !m_Slot || m_Slot.GetOwner() != m_SlotOwner.m_Entity) return false;
		if (m_Producer.m_Entity.FindComponent(SCR_CatalogEntitySpawnerComponent) != m_Spawner) return false;
		if (m_SlotOwner.m_Entity.FindComponent(SCR_EntitySpawnerSlotComponent) != m_Slot) return false;
		if (!m_Spawner.AICF_QuotaProbeSlotCurrent(m_Slot) || !m_Data || m_Entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData) != m_Data) return false;
		if (m_Data.GetSupplyCost() != m_iQuote || m_Spawner.GetRequesterFaction(user) != m_Spawner.GetFaction()) return false;
		if (!m_Producer.IsCurrent() || !m_SlotOwner.IsCurrent() || !m_User.IsCurrent()) return false;
		if (m_Producer.m_Entity.FindComponent(SCR_CatalogEntitySpawnerComponent) != m_Spawner || m_SlotOwner.m_Entity.FindComponent(SCR_EntitySpawnerSlotComponent) != m_Slot) return false;
		return !AICF_AIQuotaCatalogProbe.s_Finished && AICF_AIQuotaCatalogProbe.s_User == m_User && m_User.m_Entity == user && m_Spawner.AICF_QuotaProbeSlotCurrent(m_Slot);
	}
}



// Observation of existing native calls only. IsOccupied may delete native wrecks;
// the observer must never call it, TracePosition or GetFreeSlot for logging.
class AICF_QuotaCatalogSlotFrame
{
	ref AICF_AIQuotaCatalogIdentity m_Producer;
	ref AICF_AIQuotaCatalogIdentity m_User;
	SCR_CatalogEntitySpawnerComponent m_Spawner;
	SCR_EntityCatalogEntry m_Entry;
	SCR_EntityCatalogSpawnerData m_Data;
	SCR_EntitySpawnerSlotComponent m_LastSlot;
	array<SCR_EntitySpawnerSlotComponent> m_OccupiedSnapshot;
	string m_sPhase;
	string m_sEntryPrefab;
	bool m_bLossReported;
	int m_iTypeCalls;
	int m_iCompatibilityCalls;
	int m_iOccupancyCalls;
	int m_iSerial;
	int m_iFundingSerial;
	int m_iRequestedAt;
	bool IsCurrent()
	{
		if (!Replication.IsServer() || AICF_AIQuotaCatalogProbe.s_Finished || !m_Producer || !m_User) return false;
		if (AICF_AIQuotaCatalogProbe.s_Mode != "vehicle" || AICF_AIQuotaCatalogProbe.s_User != m_User) return false;
		if (AICF_AIQuotaCatalogProbe.s_ResourceSerial != m_iFundingSerial || AICF_AIQuotaCatalogProbe.s_RequestedAt != m_iRequestedAt) return false;
		if (!m_Producer.IsCurrent() || !m_User.IsCurrent() || !m_Spawner || m_Spawner.GetOwner() != m_Producer.m_Entity) return false;
		if (m_Producer.m_Entity.FindComponent(SCR_CatalogEntitySpawnerComponent) != m_Spawner || !SCR_CatalogEntitySpawnerComponent.INSTANCES.Contains(m_Spawner)) return false;
		if (!AICF_AIQuotaCatalogProbe.CurrentUser() || !m_Entry || m_Entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData) != m_Data) return false;
		return !AICF_AIQuotaCatalogProbe.s_Finished && AICF_AIQuotaCatalogProbe.s_User == m_User;
	}
}

class AICF_QuotaCatalogSlotObserver
{
	static ref AICF_QuotaCatalogSlotFrame s_Frame;
	static int s_iSerial;
	static bool s_bOverflow;
	static ref array<string> s_Keys = {};
	static void Stop() { s_Frame = null; s_Keys.Clear(); s_bOverflow = false; }
	static AICF_QuotaCatalogSlotFrame Begin(SCR_CatalogEntitySpawnerComponent spawner, SCR_EntityCatalogEntry entry, SCR_EntityCatalogSpawnerData data, IEntity user, string phase)
	{
		if (s_Frame || AICF_AIQuotaCatalogProbe.s_Mode != "vehicle" || AICF_AIQuotaCatalogProbe.s_Finished) return null;
		if (!spawner || !entry || !data || !user || !AICF_AIQuotaCatalogProbe.CurrentUser()) return null;
		if (!spawner || !entry || !data || !user) return null;
		AICF_QuotaCatalogSlotFrame frame = new AICF_QuotaCatalogSlotFrame();
		frame.m_Spawner = spawner; frame.m_Entry = entry; frame.m_Data = data; frame.m_sPhase = phase;
		frame.m_sEntryPrefab = entry.GetPrefab();
		frame.m_Producer = AICF_AIQuotaCatalogIdentity.Capture(spawner.GetOwner(), true);
		frame.m_User = AICF_AIQuotaCatalogProbe.s_User;
		frame.m_iSerial = ++s_iSerial;
		frame.m_iFundingSerial = AICF_AIQuotaCatalogProbe.s_ResourceSerial;
		frame.m_iRequestedAt = AICF_AIQuotaCatalogProbe.s_RequestedAt;
		if (!frame.IsCurrent() || user != frame.m_User.m_Entity || s_Frame) return null;
		s_Frame = frame;
		return frame;
	}
	static void Lost(AICF_QuotaCatalogSlotFrame frame)
	{
		if (!frame || frame.m_bLossReported) return;
		frame.m_bLossReported = true;
		AICF_AIQuotaCatalogProbe.Log("SLOT_OBSERVER_SCOPE_LOST", string.Format("serial=%1 funding_serial=%2 phase=%3 entry_prefab_snapshot=%4 shared_keys_modified=0", frame.m_iSerial, frame.m_iFundingSerial, frame.m_sPhase, frame.m_sEntryPrefab));
	}
	static void Write(AICF_QuotaCatalogSlotFrame frame, string kind, string detail)
	{
		if (!frame) return;
		if (s_Frame != frame) { Lost(frame); return; }
		bool current = frame.IsCurrent();
		if (s_Frame != frame || !current) { Lost(frame); return; }
		string key = string.Format("%1:%2:%3:%4", frame.m_Producer.m_Id, frame.m_sEntryPrefab, kind, detail);
		if (s_Frame != frame || !frame.IsCurrent() || s_Frame != frame) { Lost(frame); return; }
		if (s_Keys.Contains(key)) return;
		if (s_Keys.Count() >= 512)
		{
			if (!s_bOverflow) { s_bOverflow = true; AICF_AIQuotaCatalogProbe.Log("SLOT_OBSERVER_OVERFLOW", "maximum_distinct=512 full_census_proven=0"); }
			return;
		}
		s_Keys.Insert(key);
		AICF_AIQuotaCatalogProbe.Log("NATIVE_SLOT_OBSERVATION", string.Format("serial=%1 funding_serial=%2 phase=%3 producer=%4 entry_prefab_snapshot=%5 kind=%6 %7", frame.m_iSerial, frame.m_iFundingSerial, frame.m_sPhase, frame.m_Producer.m_Id, frame.m_sEntryPrefab, kind, detail));
	}
	static void End(AICF_QuotaCatalogSlotFrame frame, SCR_EntitySpawnerSlotComponent result)
	{
		if (!frame) return;
		EntityID id;
		if (result && result.GetOwner()) id = result.GetOwner().GetID();
		string kind = "RETURN";
		if (frame && frame.m_sPhase == "REQUEST") kind = "REQUEST_END";
		Write(frame, kind, string.Format("returned_slot=%1 nonnull=%2 fresh_occupancy_claim=0 type_calls=%3 compatibility_calls=%4 occupancy_calls=%5", id, result != null, frame.m_iTypeCalls, frame.m_iCompatibilityCalls, frame.m_iOccupancyCalls));
		if (s_Frame == frame) s_Frame = null;
	}
	static void Type(SCR_EntitySpawnerSlotComponent slot, SCR_EEntitySpawnerSlotType result)
	{
		AICF_QuotaCatalogSlotFrame frame = s_Frame;
		if (!frame || !frame.m_Spawner.AICF_QuotaProbeSlotCurrent(slot)) return;
		IEntity owner = slot.GetOwner();
		if (!owner) return;
		EntityID id = owner.GetID();
		vector position = owner.GetOrigin();
		vector producerPosition = frame.m_Producer.m_Entity.GetOrigin();
		IEntity parent = owner.GetParent();
		EntityID parentId;
		bool otherService;
		if (parent) { parentId = parent.GetID(); otherService = parent != frame.m_Producer.m_Entity && parent.FindComponent(SCR_SlotServiceComponent) != null; }
		if (s_Frame != frame || !frame.IsCurrent() || s_Frame != frame) { Lost(frame); return; }
		frame.m_LastSlot = slot;
		frame.m_iTypeCalls++;
		Write(frame, "TYPE", string.Format("slot=%1 native_type=%2 position_snapshot=%3 producer_position_snapshot=%4 distance_sq_xz=%5 parent=%6 parent_other_service_snapshot=%7 atomic_snapshot=0", id, result, position, producerPosition, vector.DistanceSqXZ(position, producerPosition), parentId, otherService));
	}
	static void Compatibility(SCR_EntityCatalogSpawnerData data, SCR_EEntitySpawnerSlotType type, bool result)
	{
		AICF_QuotaCatalogSlotFrame frame = s_Frame;
		if (!frame || frame.m_Data != data) return;
		if (s_Frame != frame || !frame.IsCurrent() || s_Frame != frame) { Lost(frame); return; }
		frame.m_iCompatibilityCalls++;
		EntityID id;
		if (frame.m_LastSlot && frame.m_LastSlot.GetOwner()) id = frame.m_LastSlot.GetOwner().GetID();
		Write(frame, "COMPATIBILITY", string.Format("last_actual_slot=%1 native_type=%2 native_compatible=%3 cached_occupied_membership_snapshot=%4 occupied_cache_snapshot_available=%5", id, type, result, frame.m_OccupiedSnapshot && frame.m_OccupiedSnapshot.Contains(frame.m_LastSlot), frame.m_OccupiedSnapshot != null));
	}
	static void Occupied(SCR_EntitySpawnerSlotComponent slot, bool result)
	{
		AICF_QuotaCatalogSlotFrame frame = s_Frame;
		if (!frame || !frame.m_Spawner.AICF_QuotaProbeSlotCurrent(slot)) return;
		if (s_Frame != frame || !frame.IsCurrent() || s_Frame != frame) { Lost(frame); return; }
		frame.m_iOccupancyCalls++;
		EntityID id;
		if (slot.GetOwner()) id = slot.GetOwner().GetID();
		Write(frame, "OCCUPANCY", string.Format("slot=%1 actual_native_call=1 native_occupied=%2 additional_trace=0", id, result));
	}
	static bool CurrentUnknownSlot(AICF_QuotaCatalogSlotFrame frame, SCR_EntitySpawnerSlotComponent slot)
	{
		if (!frame || s_Frame != frame || frame.m_LastSlot != slot) return false;
		bool current = frame.IsCurrent();
		return current && s_Frame == frame && frame.m_LastSlot == slot;
	}
	static void UnknownHit(SCR_EntitySpawnerSlotComponent slot, IEntity ent, string reason)
	{
		AICF_QuotaCatalogSlotFrame frame = s_Frame;
		if (!frame || frame.m_LastSlot != slot) return;
		if (!CurrentUnknownSlot(frame, slot)) { Lost(frame); return; }
		bool present = ent != null;
		EntityID id;
		string entityClass = "UNKNOWN";
		ResourceName prefab;
		bool metadataAvailable;
		if (ent)
		{
			id = ent.GetID();
			if (!CurrentUnknownSlot(frame, slot) || !ent) { Lost(frame); return; }
			BaseWorld world = ent.GetWorld();
			if (!CurrentUnknownSlot(frame, slot) || !ent) { Lost(frame); return; }
			if (world && world == frame.m_Producer.m_World && world.FindEntityByID(id) == ent)
			{
				if (!CurrentUnknownSlot(frame, slot) || !ent) { Lost(frame); return; }
				entityClass = ent.ClassName();
				if (!CurrentUnknownSlot(frame, slot) || !ent) { Lost(frame); return; }
				EntityPrefabData prefabData = ent.GetPrefabData();
				if (!CurrentUnknownSlot(frame, slot) || !ent) { Lost(frame); return; }
				if (prefabData) prefab = prefabData.GetPrefabName();
				if (!CurrentUnknownSlot(frame, slot) || !ent) { Lost(frame); return; }
				BaseWorld finalWorld = ent.GetWorld();
				if (!CurrentUnknownSlot(frame, slot) || !ent) { Lost(frame); return; }
				if (world && finalWorld == world) metadataAvailable = world.FindEntityByID(id) == ent;
			}
		}
		if (!CurrentUnknownSlot(frame, slot)) { Lost(frame); return; }
		Write(frame, "UNKNOWN_OCCUPANCY_HIT", string.Format("reason=%1 entity_present_before_metadata=%2 entity_id_snapshot=%3 class_after_decision=%4 prefab_after_decision=%5 metadata_available_snapshot=%6 entity_present_after_metadata=%7 metadata_after_decision=1 atomic_snapshot=0 additional_trace=0", reason, present, id, entityClass, prefab, metadataAvailable, ent != null));
	}
}

[BaseContainerProps(configRoot: true), BaseContainerCustomCheckIntTitleField("m_bEnabled", "SpawnerData", "DISABLED - SpawnerData", 1)]
modded class SCR_EntityCatalogSpawnerData
{
	override bool CanSpawnInSlot(SCR_EEntitySpawnerSlotType slotSize)
	{
		bool result = super.CanSpawnInSlot(slotSize);
		AICF_QuotaCatalogSlotObserver.Compatibility(this, slotSize, result);
		return result;
	}
}
modded class SCR_EntitySpawnerSlotComponent
{
	override protected void AICF_ObserveUnknownOccupancyHit(IEntity ent, string reason)
	{
		super.AICF_ObserveUnknownOccupancyHit(ent, reason);
		AICF_QuotaCatalogSlotObserver.UnknownHit(this, ent, reason);
	}
	override SCR_EEntitySpawnerSlotType GetSlotType()
	{
		SCR_EEntitySpawnerSlotType result = super.GetSlotType();
		AICF_QuotaCatalogSlotObserver.Type(this, result);
		return result;
	}
	override bool IsOccupied()
	{
		bool result = super.IsOccupied();
		AICF_QuotaCatalogSlotObserver.Occupied(this, result);
		return result;
	}
}

// Controlled infrastructure preparation, not an ordinary construction proof.
class AICF_QuotaCatalogDepotScope
{
	AICF_ConstructionBaseState m_State;
	AICF_ConstructionPlanner m_Planner;
	SCR_CampaignFaction m_Faction;
	SCR_CampaignMilitaryBaseComponent m_Base;
	SCR_CampaignBuildingProviderComponent m_Provider;
	EntityID m_BaseId;
	EntityID m_ProviderId;
	BaseWorld m_World;
	int m_iEpoch;
	bool m_bTarget;
}

class AICF_QuotaCatalogDepotScan
{
	AICF_ConstructionOrder m_Order;
	IEntity m_Root;
	int m_iEpoch;
	int m_iVisited;
	ref array<ref AICF_AIQuotaCatalogIdentity> m_Producers = {};
}

class AICF_QuotaCatalogDepotSetup
{
	static bool s_Initialized;
	static bool s_Enabled;
	static bool s_Stopped;
	static bool s_Paid;
	static bool s_CompletionRequested;
	static bool s_Ready;
	static int s_Epoch;
	static ref AICF_QuotaCatalogDepotScope s_Scope;
	static ref AICF_ConstructionOrder s_Order;
	static AICF_ConstructionPlanner s_Planner;
	static AICF_ConstructionBaseState s_State;
	static SCR_CampaignFaction s_Faction;
	static BaseWorld s_World;
	static string s_Token;
	static int s_Revision;
	static EntityID s_BaseId;
	static EntityID s_ProviderId;
	static ResourceName s_Prefab;
	static ref AICF_ConstructionMetadata s_Metadata;
	static IEntity s_Root;
	static EntityID s_RootId;
	static SCR_CampaignBuildingLayoutComponent s_Layout;
	static ref AICF_AIQuotaCatalogIdentity s_RootProof;
	static ref AICF_AIQuotaCatalogIdentity s_LayoutProof;
	static ref AICF_QuotaCatalogDepotScan s_Scan;
	static ref array<ref AICF_AIQuotaCatalogIdentity> s_Producers = {};
	static ref array<string> s_Waits = {};

	static void Initialize()
	{
		if (s_Initialized) return;
		s_Initialized = true;
		string value;
		bool opted = System.GetCLIParam("aicfQuotaCatalogControlledDepotSetup", value) && value == "1";
		bool probe = AICF_AIQuotaCatalogProbe.Enabled();
		s_Enabled = opted && probe && AICF_AIQuotaCatalogProbe.s_Mode == "vehicle";
		if (s_Enabled && Replication.IsServer()) AICF_AIQuotaCatalogProbe.Log("CONTROLLED_SETUP", "phase=ENABLED kind=PAID_NATIVE_LIGHT_DEPOT ordinary_proof=0 clock_reset=0");
	}
	static bool Active()
	{
		if (!s_Enabled || s_Stopped || !Replication.IsServer()) return false;
		return !AICF_AIQuotaCatalogProbe.s_Finished && !AICF_AIQuotaCatalogProbe.s_Pressure;
	}
	static void Stop()
	{
		s_Stopped = true;
		s_Epoch++;
		s_Scope = null;
		s_Order = null;
		s_Planner = null;
		s_State = null;
		s_Faction = null;
		s_World = null;
		s_Metadata = null;
		s_Root = null;
		s_Layout = null;
		s_RootProof = null;
		s_LayoutProof = null;
		s_Scan = null;
		s_Producers.Clear();
		s_Waits.Clear();
	}
	static void NoteWait(string reason, int epoch)
	{
		if (epoch != s_Epoch || !Active() || s_Waits.Contains(reason)) return;
		if (s_Waits.Count() >= 16) return;
		s_Waits.Insert(reason);
		AICF_AIQuotaCatalogProbe.Log("CONTROLLED_SETUP", "phase=WAIT ordinary_proof=0 reason=" + reason);
	}
	static void PlannerStopped(AICF_ConstructionPlanner planner)
	{
		if (!s_Enabled) return;
		if (planner == s_Planner || (s_Scope && s_Scope.m_Planner == planner)) Stop();
	}
	static bool ScopePinned(AICF_QuotaCatalogDepotScope frame)
	{
		if (!frame || s_Scope != frame || !frame.m_bTarget || !Active()) return false;
		if (frame.m_iEpoch != s_Epoch || !frame.m_State || !frame.m_Planner) return false;
		return frame.m_State.m_Base == frame.m_Base && frame.m_State.m_BaseId == frame.m_BaseId;
	}
	static AICF_QuotaCatalogDepotScope Enter(AICF_ConstructionPlanner planner, AICF_ConstructionBaseState state)
	{
		AICF_QuotaCatalogDepotScope frame = new AICF_QuotaCatalogDepotScope();
		frame.m_State = state; frame.m_Planner = planner; frame.m_iEpoch = s_Epoch;
		s_Scope = frame;
		if (!Active() || s_Order || !state || state.m_Order || !state.m_Base) return frame;
		SCR_CampaignMilitaryBaseComponent base = state.m_Base;
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(base.GetFaction());
		if (!faction || faction.GetFactionKey() != AICF_ContentProfile.GetActive().GetRuntimeFactionKey("USSR")) return frame;
		if (faction.GetMainBase() != base || !base.IsInitialized()) return frame;
		SCR_CampaignBuildingProviderComponent provider = base.GetMasterProvider();
		if (!provider || !provider.GetOwner() || !base.GetOwner() || !GetGame()) return frame;
		frame.m_Faction = faction; frame.m_Base = base; frame.m_Provider = provider;
		frame.m_BaseId = base.GetOwner().GetID(); frame.m_ProviderId = provider.GetOwner().GetID();
		frame.m_World = GetGame().GetWorld();
		if (s_Scope == frame && Active() && frame.m_iEpoch == s_Epoch) frame.m_bTarget = true;
		return frame;
	}
	static bool ScopeCurrent(AICF_QuotaCatalogDepotScope frame)
	{
		if (!frame || s_Scope != frame || !frame.m_bTarget || !Active()) return false;
		if (frame.m_iEpoch != s_Epoch || !GetGame() || GetGame().GetWorld() != frame.m_World) return false;
		if (!frame.m_Base || !frame.m_Provider || !frame.m_Faction || !frame.m_State) return false;
		IEntity baseOwner = frame.m_Base.GetOwner();
		IEntity providerOwner = frame.m_Provider.GetOwner();
		if (!baseOwner || !providerOwner || !frame.m_World) return false;
		if (frame.m_World.FindEntityByID(frame.m_BaseId) != baseOwner) return false;
		if (frame.m_World.FindEntityByID(frame.m_ProviderId) != providerOwner) return false;
		if (frame.m_Base.GetFaction() != frame.m_Faction || frame.m_Faction.GetMainBase() != frame.m_Base) return false;
		if (frame.m_Base.GetMasterProvider() != frame.m_Provider || frame.m_Provider.GetCampaignMilitaryBaseComponent() != frame.m_Base) return false;
		return ScopePinned(frame);
	}
	static int Select(AICF_AICommander commander, array<bool> covered, int original)
	{
		AICF_QuotaCatalogDepotScope frame = s_Scope;
		if (!frame || !frame.m_bTarget) return original;
		if (!commander) return -1;
		if (commander.GetFaction() != frame.m_Faction) return original;
		if (!ScopeCurrent(frame)) return -1;
		if (!covered || covered.Count() != AICF_EConstructionType.COUNT || covered[AICF_EConstructionType.LIGHT_DEPOT]) return -1;
		return AICF_EConstructionType.LIGHT_DEPOT;
	}
	static void Leave(AICF_QuotaCatalogDepotScope frame, AICF_QuotaCatalogDepotScope previous)
	{
		if (ScopeCurrent(frame) && !s_Order && frame.m_State.m_Order)
		{
			AICF_ConstructionOrder order = frame.m_State.m_Order;
			if (OrderInScope(frame, order) && order.IdentityValid() && OrderInScope(frame, order))
			{
				if (ScopePinned(frame) && frame.m_State.m_Order == order)
				{
					s_Order = order; s_State = frame.m_State; s_Faction = frame.m_Faction; s_World = frame.m_World;
					s_Planner = frame.m_Planner;
					s_Token = order.m_sToken; s_Revision = order.m_iRevision;
					s_BaseId = order.m_BaseId; s_ProviderId = order.m_ProviderId;
					s_Metadata = order.m_Metadata;
					if (s_Metadata) s_Prefab = s_Metadata.m_sPrefab;
					AICF_AIQuotaCatalogProbe.Log("CONTROLLED_SETUP", "phase=ORDER_CAPTURED kind=PAID_NATIVE_LIGHT_DEPOT ordinary_proof=0 token=" + s_Token);
				}
			}
		}
		if (s_Scope == frame) s_Scope = previous;
	}
	static bool OrderInScope(AICF_QuotaCatalogDepotScope frame, AICF_ConstructionOrder order)
	{
		if (!ScopePinned(frame) || !order || frame.m_State.m_Order != order) return false;
		if (order.m_eType != AICF_EConstructionType.LIGHT_DEPOT || order.m_Faction != frame.m_Faction) return false;
		if (order.m_Base != frame.m_Base || order.m_Provider != frame.m_Provider) return false;
		return order.m_BaseId == frame.m_BaseId && order.m_ProviderId == frame.m_ProviderId;
	}
	static bool Same(AICF_ConstructionOrder order, int epoch)
	{
		if (!Active() || !order || s_Order != order || s_Epoch != epoch) return false;
		if (order.m_sToken != s_Token || order.m_iRevision != s_Revision) return false;
		if (!s_Metadata || order.m_Metadata != s_Metadata) return false;
		if (order.m_BaseId != s_BaseId || order.m_ProviderId != s_ProviderId || order.m_Faction != s_Faction) return false;
		return order.m_eType == AICF_EConstructionType.LIGHT_DEPOT && !order.m_bCancelled;
	}
	static bool Current(AICF_ConstructionOrder order)
	{
		int epoch = s_Epoch;
		if (!Same(order, epoch) || !GetGame() || GetGame().GetWorld() != s_World) return false;
		if (!order.m_Metadata || order.m_Metadata.m_sPrefab != s_Prefab) return false;
		bool identity = order.IdentityValid();
		if (!identity || !Same(order, epoch) || !order.m_Faction) return false;
		if (order.m_Faction.GetMainBase() != order.m_Base) return false;
		if (!s_Paid && (!s_State || s_State.m_Order != order)) return false;
		return Same(order, epoch);
	}
	static void Placed(AICF_ConstructionOrder order, bool nativeResult)
	{
		if (order != s_Order || !nativeResult) return;
		int epoch = s_Epoch;
		if (!Current(order) || s_Paid) { AICF_AIQuotaCatalogProbe.Finish(false, "SETUP_PAID_SCOPE_OR_DUPLICATE"); return; }
		if (!order.m_bAccepted || !order.m_bPaid || !order.m_bCommitStarted || order.m_iCost <= 0) { AICF_AIQuotaCatalogProbe.Finish(false, "SETUP_PAYMENT_NOT_PROVEN"); return; }
		if (Math.AbsFloat(order.m_fBefore - order.m_fAfter - order.m_iCost) > 0.01) { AICF_AIQuotaCatalogProbe.Finish(false, "SETUP_PAYMENT_DELTA"); return; }
		if (!order.m_Composition || order.m_Composition.m_AICFConstructionReceipt != order) { AICF_AIQuotaCatalogProbe.Finish(false, "SETUP_RECEIPT_LOST"); return; }
		IEntity root = order.m_Composition.GetOwner();
		SCR_CampaignBuildingLayoutComponent layout = order.m_Composition.GetCompositionLayout();
		if (!root || !layout || !Same(order, epoch)) return;
		EntityID rootId = root.GetID();
		if (!Same(order, epoch) || !root || !layout) return;
		if (rootId != order.m_LayoutId || !order.PlacementUnchanged()) { AICF_AIQuotaCatalogProbe.Finish(false, "SETUP_PLACEMENT_UNPROVEN"); return; }
		if (!Current(order) || !Same(order, epoch) || !root || !layout) return;
		s_Root = root; s_RootId = order.m_LayoutId; s_Layout = layout; s_Paid = true;
		AICF_AIQuotaCatalogProbe.Log("CONTROLLED_SETUP", string.Format("phase=PAID_PLACED ordinary_proof=0 token=%1 root=%2 cost=%3 before=%4 after=%5 completion_forced=0", s_Token, s_RootId, order.m_iCost, order.m_fBefore, order.m_fAfter));
	}
	static bool PlacedCurrent(AICF_ConstructionOrder order)
	{
		int epoch = s_Epoch;
		IEntity root = s_Root;
		SCR_CampaignBuildingLayoutComponent layout = s_Layout;
		BaseWorld world = s_World;
		if (!s_Paid || !Current(order) || !order || !order.m_Composition) return false;
		SCR_CampaignBuildingCompositionComponent composition = order.m_Composition;
		if (!PurePlaced(order, epoch, root, layout, composition, world)) return false;
		IEntity owner = composition.GetOwner();
		if (!PurePlaced(order, epoch, root, layout, composition, world) || owner != root) return false;
		IEntity found = world.FindEntityByID(s_RootId);
		if (!PurePlaced(order, epoch, root, layout, composition, world) || found != root) return false;
		BaseWorld actualWorld = root.GetWorld();
		if (!PurePlaced(order, epoch, root, layout, composition, world) || actualWorld != world) return false;
		SCR_CampaignBuildingLayoutComponent actualLayout = composition.GetCompositionLayout();
		if (!PurePlaced(order, epoch, root, layout, composition, world) || actualLayout != layout) return false;
		bool unchanged = order.PlacementUnchanged();
		if (!GetGame() || GetGame().GetWorld() != world) return false;
		return PurePlaced(order, epoch, root, layout, composition, world) && unchanged;
	}
	static bool PurePlaced(AICF_ConstructionOrder order, int epoch, IEntity root, SCR_CampaignBuildingLayoutComponent layout, SCR_CampaignBuildingCompositionComponent composition, BaseWorld world)
	{
		if (!Same(order, epoch) || !s_Paid || !root || !layout || !composition || !world) return false;
		if (root != s_Root || layout != s_Layout || world != s_World || composition != order.m_Composition) return false;
		return order.m_bPaid && order.m_bAccepted && composition.m_AICFConstructionReceipt == order;
	}
	static bool EnsureMaster(AICF_ConstructionOrder order)
	{
		int epoch = s_Epoch;
		if (!PlacedCurrent(order)) return false;
		IEntity root = s_Root;
		SCR_CampaignBuildingLayoutComponent layout = s_Layout;
		if (!s_RootProof)
		{
			AICF_AIQuotaCatalogIdentity proof = AICF_AIQuotaCatalogIdentity.Capture(root, true);
			if (!Same(order, epoch) || s_Root != root || !root || !proof) return false;
			s_RootProof = proof;
		}
		if (!s_LayoutProof)
		{
			if (!layout || !Same(order, epoch)) return false;
			IEntity owner = layout.GetOwner();
			if (!Same(order, epoch) || !owner || layout != s_Layout) return false;
			AICF_AIQuotaCatalogIdentity proofLayout = AICF_AIQuotaCatalogIdentity.Capture(owner, true);
			if (!Same(order, epoch) || layout != s_Layout || !proofLayout) return false;
			s_LayoutProof = proofLayout;
		}
		return MasterCurrent(order);
	}
	static bool MasterCurrent(AICF_ConstructionOrder order)
	{
		int epoch = s_Epoch;
		AICF_AIQuotaCatalogIdentity rootProof = s_RootProof;
		AICF_AIQuotaCatalogIdentity layoutProof = s_LayoutProof;
		if (!rootProof || !layoutProof || !AuthorityCurrent(order) || !PlacedCurrent(order)) return false;
		SCR_CampaignBuildingLayoutComponent layout = s_Layout;
		if (!layout) return false;
		IEntity layoutOwner = layout.GetOwner();
		if (!Same(order, epoch) || !layout || layout != s_Layout || layoutProof.m_Entity != layoutOwner) return false;
		if (!rootProof.IsCurrent()) return false;
		if (!Same(order, epoch) || rootProof != s_RootProof || layoutProof != s_LayoutProof) return false;
		if (!layoutProof.IsCurrent()) return false;
		if (!Same(order, epoch) || rootProof != s_RootProof || layoutProof != s_LayoutProof) return false;
		return s_Root && s_Layout && rootProof.m_Entity == s_Root;
	}
	static bool AuthorityCurrent(AICF_ConstructionOrder order)
	{
		int epoch = s_Epoch;
		if (!Same(order, epoch) || !GetGame()) return false;
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.Cast(GetGame().GetGameMode());
		if (!Same(order, epoch) || !campaign) return false;
		bool master = campaign.IsMaster();
		if (!Same(order, epoch) || !campaign) return false;
		bool running = campaign.IsRunning();
		return Same(order, epoch) && campaign && master && running;
	}
	static bool ScanPinned(AICF_QuotaCatalogDepotScan frame)
	{
		return frame && s_Scan == frame && Same(frame.m_Order, frame.m_iEpoch) && s_Root == frame.m_Root;
	}
	static bool ScanCurrent(AICF_QuotaCatalogDepotScan frame)
	{
		if (!ScanPinned(frame) || !MasterCurrent(frame.m_Order)) return false;
		return ScanPinned(frame);
	}
	static bool Scan(IEntity entity, SCR_PlayerController player, AICF_QuotaCatalogDepotScan frame)
	{
		if (!ScanCurrent(frame) || !entity || ++frame.m_iVisited > 256) return false;
		SCR_DefenderSpawnerComponent defender = SCR_DefenderSpawnerComponent.Cast(entity.FindComponent(SCR_DefenderSpawnerComponent));
		if (!ScanCurrent(frame) || !entity) return false;
		bool defenderEnabled;
		if (defender) defenderEnabled = defender.IsSpawningEnabled();
		if (!ScanCurrent(frame) || !entity) return false;
		if (defender && defenderEnabled)
		{
			if (!player || !AICF_AIQuotaCatalogProbe.CurrentUser() || !ScanCurrent(frame)) return false;
			AICF_AIQuotaCatalogIdentity defenderProof = AICF_AIQuotaCatalogIdentity.Capture(entity, true);
			if (!defenderProof || !ScanCurrent(frame) || !entity || !defender) return false;
			if (entity.FindComponent(SCR_DefenderSpawnerComponent) != defender || defender.GetOwner() != entity) return false;
			int playerId = player.GetPlayerId();
			if (!ScanCurrent(frame) || !player || !defender) return false;
			if (!defenderProof.IsCurrent() || !ScanPinned(frame) || !defender || !player) return false;
			defender.EnableSpawning(false, playerId);
			if (!defender || !ScanCurrent(frame)) return false;
			bool stillEnabled = defender.IsSpawningEnabled();
			if (!ScanCurrent(frame)) return false;
			if (stillEnabled) { NoteWait("NATIVE_DEFENDER_TOGGLE_REJECTED", frame.m_iEpoch); return false; }
			AICF_AIQuotaCatalogProbe.Log("CONTROLLED_SETUP", "phase=NATIVE_DEFENDER_DISABLED actual_player_validation=1 existing_bodies_retained=1");
		}
		SCR_CatalogEntitySpawnerComponent producer = SCR_CatalogEntitySpawnerComponent.Cast(entity.FindComponent(SCR_CatalogEntitySpawnerComponent));
		if (!ScanCurrent(frame) || !entity) return false;
		bool light;
		bool sameFaction;
		if (producer)
		{
			light = producer.GetType() == SCR_EServicePointType.LIGHT_VEHICLE_DEPOT;
			if (!ScanCurrent(frame) || !entity || !producer) return false;
			sameFaction = producer.GetFaction() == s_Faction;
			if (!ScanCurrent(frame) || !entity || !producer) return false;
		}
		if (producer && light && sameFaction)
		{
			AICF_AIQuotaCatalogIdentity identity = AICF_AIQuotaCatalogIdentity.Capture(entity, true);
			if (!identity || !ScanCurrent(frame) || !entity) { NoteWait("PRODUCER_IDENTITY_PENDING_OR_REJECTED", frame.m_iEpoch); return false; }
			frame.m_Producers.Insert(identity);
		}
		IEntity child = entity.GetChildren();
		if (!ScanCurrent(frame) || !entity) return false;
		while (child)
		{
			IEntity next = child.GetSibling();
			if (!ScanCurrent(frame)) return false;
			if (!Scan(child, player, frame)) return false;
			child = next;
		}
		return ScanCurrent(frame);
	}
	static bool Ready()
	{
		if (!s_Enabled) return true;
		if (!Active()) return false;
		int epoch = s_Epoch;
		AICF_ConstructionOrder order = s_Order;
		if (!order) return false;
		if (!Current(order)) { AICF_AIQuotaCatalogProbe.Finish(false, "SETUP_ORDER_IDENTITY_LOST"); return false; }
		if (!s_Paid) return false;
		if (!PlacedCurrent(order)) { AICF_AIQuotaCatalogProbe.Finish(false, "SETUP_PAID_LAYOUT_LOST"); return false; }
		if (!EnsureMaster(order)) { NoteWait("MASTER_IDENTITY_OR_CAMPAIGN_PENDING", epoch); return false; }
		bool spawned = order.m_Composition.IsCompositionSpawned();
		if (!PlacedCurrent(order)) return false;
		if (!spawned)
		{
			if (s_CompletionRequested) { NoteWait("COMPOSITION_SPAWN_PENDING_OR_BLOCKED", epoch); return false; }
			if (!AICF_ConstructionSiteSearch.CompletionClear(order)) { NoteWait("ORIGINAL_COMPLETION_CLEAR_BLOCKED", epoch); return false; }
			if (!MasterCurrent(order)) return false;
			SCR_CampaignBuildingLayoutComponent layout = s_Layout;
			if (!layout) return false;
			int total = layout.GetToBuildValue();
			if (total <= 0 || !MasterCurrent(order) || !layout || layout != s_Layout) return false;
			s_CompletionRequested = true;
			layout.SetBuildingValue(total);
			AICF_AIQuotaCatalogProbe.Log("CONTROLLED_SETUP", "phase=COMPLETION_REQUESTED controlled_public_SetBuildingValue=1 original_completion_clear=1 ordinary_worker_proof=0");
			return false;
		}
		if (!AICF_ConstructionMetadata.HasOnlineService(s_Root, AICF_EConstructionType.LIGHT_DEPOT)) { NoteWait("NATIVE_SERVICE_OFFLINE", epoch); return false; }
		if (!s_Ready)
		{
			if (s_Scan) return false;
			AICF_QuotaCatalogDepotScan frame = new AICF_QuotaCatalogDepotScan();
			frame.m_Order = order; frame.m_Root = s_Root; frame.m_iEpoch = s_Epoch;
			s_Scan = frame;
			bool scanned = Scan(s_Root, AICF_AIQuotaCatalogProbe.s_Player, frame);
			bool current = ScanCurrent(frame);
			if (scanned && current && !frame.m_Producers.IsEmpty())
			{
				s_Producers = frame.m_Producers;
				s_Ready = true;
			}
			if (s_Scan == frame) s_Scan = null;
			if (!s_Ready || !current) { NoteWait("NATIVE_PRODUCER_OR_DEFENDER_PENDING", frame.m_iEpoch); return false; }
			AICF_AIQuotaCatalogProbe.Log("CONTROLLED_SETUP", string.Format("phase=ONLINE ordinary_proof=0 paid_construction=1 producers=%1 native_slots_registration=REQUIRED_BY_SELECTION stock_bodies_retained=1 stock_ledger_recomputed_before_pressure=1", s_Producers.Count()));
		}
		return PlacedCurrent(order);
	}
	static bool AllowsSpawner(SCR_CatalogEntitySpawnerComponent producer)
	{
		if (!s_Enabled) return true;
		if (!s_Ready || !producer || !PlacedCurrent(s_Order)) return false;
		foreach (AICF_AIQuotaCatalogIdentity identity : s_Producers)
		{
			if (!identity || !identity.IsCurrent() || !producer) continue;
			if (producer.GetOwner() == identity.m_Entity && producer.GetFaction() == s_Faction && PlacedCurrent(s_Order)) return true;
		}
		return false;
	}
}

modded class AICF_ConstructionPlanner
{
	override void Stop()
	{
		AICF_QuotaCatalogDepotSetup.PlannerStopped(this);
		super.Stop();
	}
	override protected void Decide(AICF_ConstructionBaseState state, int now)
	{
		AICF_QuotaCatalogDepotScope previous = AICF_QuotaCatalogDepotSetup.s_Scope;
		AICF_QuotaCatalogDepotScope frame;
		if (AICF_QuotaCatalogDepotSetup.s_Enabled) frame = AICF_QuotaCatalogDepotSetup.Enter(this, state);
		super.Decide(state, now);
		if (frame) AICF_QuotaCatalogDepotSetup.Leave(frame, previous);
	}
}
modded class AICF_AICommander
{
	override int SelectConstructionType(array<bool> covered, int startType)
	{
		int chosen = AICF_QuotaCatalogDepotSetup.Select(this, covered, startType);
		if (chosen < 0) return -1;
		return super.SelectConstructionType(covered, chosen);
	}
}
modded class AICF_StockConstructionAdapter
{
	override bool Place(AICF_ConstructionOrder order, AICF_ConstructionConfig config, AICF_EconomySystem economy, SCR_CampaignBuildingManagerComponent manager, AICF_BaseBuilderService builders)
	{
		bool result = super.Place(order, config, economy, manager, builders);
		AICF_QuotaCatalogDepotSetup.Placed(order, result);
		return result;
	}
}
