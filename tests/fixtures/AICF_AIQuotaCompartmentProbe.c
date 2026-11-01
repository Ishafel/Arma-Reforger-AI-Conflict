// Дополнительный native gate конкретного producer, не catalog/payment proof.
// Свежая пустая stock машина и реальные новые тела, без heal/TP/limit writes.
class AICF_AIQuotaCompartmentProbe
{
	static AICF_MatchController s_Owner;
	static AICF_AIWorldQuotaService s_Service;
	static Vehicle s_Car;
	static int s_Factories;
	static int s_Births;
	static int s_Entries;
	static int s_UnknownCaptures;
	static int s_LastCountedActive = -1;
	static int s_LastNativeActive = -1;
	static bool s_BoundBeforeActivation = true;
	static bool s_Finished;
	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfQuotaCompartmentProbe", value) && value == "1";
	}
	static void Log(string eventName, string details)
	{
		Print("[AICF][QUOTA_COMPARTMENT_PROBE][" + eventName + "] " + details);
	}
	static void GlobalTimeout()
	{
		if (!s_Finished) Log("DONE", "passed=0 reason=CONTROLLER_OR_FIXTURE_TIMEOUT checks=0 failures=1 factories=0 births=0 entries=0 tail_polls=0");
		GetGame().RequestClose();
	}
}

modded class AICF_AIWorldQuotaService
{
	override protected bool BuildCounts(AICF_AIQuotaCensus snapshot, array<int> counts)
	{
		bool built = super.BuildCounts(snapshot, counts);
		if (!built && this == AICF_AIQuotaCompartmentProbe.s_Service && !AICF_AIQuotaCompartmentProbe.s_Finished)
		{
			AICF_AIQuotaCompartmentProbe.s_UnknownCaptures++;
			string reason = "NO_SNAPSHOT";
			int rows = -1;
			if (snapshot)
			{
				reason = snapshot.GetReason();
				rows = snapshot.Count();
			}
			AICF_AIQuotaCompartmentProbe.Log("BUILD_COUNTS_REJECTED", string.Format(
				"capture_reason=%1 captured_rows=%2 result_unchanged=1", reason, rows));
		}
		return built;
	}

	// Только scalar readback; ни один local/ref не удерживает старый census.
	override AICF_AIQuotaCompartmentCreation BeginCompartmentCreation(BaseCompartmentSlot seat,
		ResourceName characterPrefab, AIGroup group, ResourceName groupPrefab)
	{
		bool target = this == AICF_AIQuotaCompartmentProbe.s_Service && AICF_AIQuotaCompartmentProbe.s_Car && seat && seat.GetVehicle() == AICF_AIQuotaCompartmentProbe.s_Car;
		if (target)
		{
			FactionKey faction;
			bool characterMetadata = AICF_AIQuotaCompartmentCreation.ReadCharacterMetadata(characterPrefab, faction);
			bool emptyGroupMetadata = group || AICF_AIQuotaCompartmentCreation.ReadEmptyGroupMetadata(groupPrefab);
			ChimeraAIWorld nativeWorld = ChimeraAIWorld.Cast(m_World);
			bool canAI = nativeWorld && nativeWorld.CanAIBeActivated();
			bool canGroupNull = nativeWorld && nativeWorld.CanActivateGroup(null);
			bool canFaction = characterMetadata && nativeWorld && nativeWorld.CanLimitedAIBeAddedForFaction(faction);
			AICF_AIQuotaCompartmentProbe.Log("BEFORE_BEGIN", string.Format(
				"character_metadata=%1 empty_group_metadata=%2 can_ai=%3 can_group_null=%4 can_faction=%5 stock=%6 reserved=%7 last_reason=%8",
				characterMetadata, emptyGroupMetadata, canAI, canGroupNull,
				canFaction, GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK),
				GetLedger().GetReserved(AICF_EAIQuotaBucket.STOCK), GetLastReason()));
		}
		AICF_AIQuotaCompartmentCreation result = super.BeginCompartmentCreation(seat, characterPrefab, group, groupPrefab);
		if (target) AICF_AIQuotaCompartmentProbe.Log("BEGIN_RESULT", string.Format("accepted=%1 last_reason=%2", result != null, GetLastReason()));
		return result;
	}

	override void CloseCompartmentCreation(AICF_AIQuotaCompartmentCreation context)
	{
		if (this == AICF_AIQuotaCompartmentProbe.s_Service && context && !context.m_bSourceEnded && context.m_SeatScope &&
			context.m_SeatScope.m_Vehicle == AICF_AIQuotaCompartmentProbe.s_Car)
		{
			AICF_AIQuotaVehicleUse fresh = AICF_AIQuotaVehicleUse.Capture(context.m_SeatScope.m_Vehicle, m_Census);
			int available = -1;
			if (context.m_CharacterReservation) available = context.m_CharacterReservation.m_iAvailable;
			AICF_AIQuotaCompartmentProbe.Log("CLOSE_SOURCE", string.Format(
				"character_reservation=%1 available=%2 context_current=%3 car_hold_current=%4 fresh_use_current=%5 creation=%6 native_issued=%7 source_ended=%8 last_reason=%9",
				context.m_CharacterReservation != null, available, context.IsCurrent(), IsCompartmentCarHoldCurrent(context),
				fresh && fresh.IsCurrent(), context.m_Creation != null, context.m_bNativeIssued, context.m_bSourceEnded, GetLastReason()));
		}
		super.CloseCompartmentCreation(context);
	}

	override bool MarkExternalNativeCallStarted(AICF_AIQuotaCreation creation, bool commitAttempt = false)
	{
		bool accepted = super.MarkExternalNativeCallStarted(creation, commitAttempt);
		AICF_AIQuotaCompartmentCreation source;
		if (creation) source = AICF_AIQuotaCompartmentCreation.Cast(creation.m_Guard);
		if (accepted && this == AICF_AIQuotaCompartmentProbe.s_Service && source && source.m_SeatScope &&
			source.m_SeatScope.m_Vehicle == AICF_AIQuotaCompartmentProbe.s_Car)
			AICF_AIQuotaCompartmentProbe.s_Factories++;
		return accepted;
	}

	override bool CaptureCompartmentNewborn(AICF_AIQuotaCompartmentCreation context, IEntity raw)
	{
		AICF_AIQuotaCompartmentCreation source = context;
		bool accepted = super.CaptureCompartmentNewborn(context, raw);
		if (this != AICF_AIQuotaCompartmentProbe.s_Service || !AICF_AIQuotaCompartmentProbe.s_Car || !source || !source.m_SeatScope ||
			source.m_SeatScope.m_Vehicle != AICF_AIQuotaCompartmentProbe.s_Car) return accepted;
		if (accepted)
		{
			if (!raw || !source.m_Newborn)
			{
				AICF_AIQuotaCompartmentProbe.s_BoundBeforeActivation = false;
				return accepted;
			}
			string actorId = source.m_Newborn.m_Id.ToString();
			string actorRpl = source.m_Newborn.m_sRpl;
			AICF_AIQuotaCompartmentProbe.s_Births++;
			bool bound = source.m_Creation && source.m_Creation.m_Status == AICF_EAIQuotaBirthStatus.BOUND &&
				source.m_Creation.m_Ticket && source.m_Creation.m_Ticket.m_bClosed;
			if (bound) bound = source.m_CharacterBody && source.m_CharacterBody.MatchesIdentity() && source.m_CarBody &&
				!source.m_CarBody.m_bReleased && source.m_CarOwner;
			if (bound) bound = GetLedger().GetBindingBucket(source.m_CharacterBody.Key()) == AICF_EAIQuotaBucket.STOCK;
			if (bound) bound = GetLedger().GetBindingBucket(source.m_CarBody.Key()) == source.m_CarOwner.m_Bucket;
			if (!bound) AICF_AIQuotaCompartmentProbe.s_BoundBeforeActivation = false;
			bool exactCreation = OwnsCurrentCompartmentNewborn(source);
			bool carBound = IsCompartmentCarHoldCurrent(source);
			AICF_AIQuotaCompartmentProbe.Log("BOUND", string.Format(
				"index=%1 entity=%2 rpl=%3 exact_current_creation=%4 character_bound=%5 car_bound=%6 before_activate_getin=1",
				AICF_AIQuotaCompartmentProbe.s_Births, actorId, actorRpl, exactCreation, bound, carBound));
		}
		return accepted;
	}

	override void ObserveCompartmentEntry(AICF_AIQuotaCompartmentCreation context, bool accepted)
	{
		AICF_AIQuotaCompartmentCreation source = context;
		bool target = this == AICF_AIQuotaCompartmentProbe.s_Service && AICF_AIQuotaCompartmentProbe.s_Car && source &&
			source.m_SeatScope && source.m_SeatScope.m_Vehicle == AICF_AIQuotaCompartmentProbe.s_Car && source.m_SeatScope.m_VehicleIdentity;
		string carId;
		bool issued;
		if (target)
		{
			carId = source.m_SeatScope.m_VehicleIdentity.m_Id.ToString();
			issued = source.m_bNativeIssued;
		}
		super.ObserveCompartmentEntry(context, accepted);
		if (!target) return;
		AICF_AIQuotaCompartmentProbe.s_Entries++;
		AICF_AIQuotaCompartmentProbe.Log("GETIN_RESULT", string.Format(
			"index=%1 accepted=%2 marked_before_native=%3 native_bool_observed=1 car=%4",
			AICF_AIQuotaCompartmentProbe.s_Entries, accepted, issued, carId));
	}

	array<ref AICF_AIQuotaBody> AICF_QCBodySnapshot()
	{
		array<ref AICF_AIQuotaBody> snapshot = {};
		foreach (AICF_AIQuotaBody body : m_aBodies) snapshot.Insert(body);
		return snapshot;
	}
}

modded class AICF_AIQuotaVehicleUse
{
	override bool IsCurrent()
	{
		bool current = super.IsCurrent();
		if (!current && AICF_AIQuotaCompartmentProbe.s_Car && m_Identity && m_Identity.m_Entity == AICF_AIQuotaCompartmentProbe.s_Car)
			AICF_AIQuotaCompartmentProbe.Log("USE_REJECTED", string.Format(
				"car=%1 had_native_row=%2 native_row_present=%3 captured_active=%4 captured_counted=%5 owner_identity_present=%6",
				m_Identity.m_Id, m_bHadNative, m_Native != null, m_bActive, m_bCounted, m_Identity.m_Entity != null));
		return current;
	}
}

modded class AICF_AIQuotaCensus
{
	override bool Capture(SCR_AIWorld world)
	{
		bool captured = super.Capture(world);
		if (!captured && AICF_AIQuotaCompartmentProbe.s_Service && !AICF_AIQuotaCompartmentProbe.s_Finished)
		{
			AICF_AIQuotaCompartmentProbe.s_UnknownCaptures++;
			int countedActive;
			int missing;
			foreach (AICF_AIQuotaAgentRow row : m_aRows)
			{
				if (!row) missing++;
				else if (row.m_bCounted && row.m_bActivated) countedActive++;
			}
			int nativeActive = -1;
			if (world) nativeActive = world.GetCurrentNumOfActiveAIs();
			AICF_AIQuotaCompartmentProbe.s_LastCountedActive = countedActive;
			AICF_AIQuotaCompartmentProbe.s_LastNativeActive = nativeActive;
			AICF_AIQuotaCompartmentProbe.Log("CENSUS_REJECTED", string.Format(
				"reason=%1 rows=%2 counted_active=%3 native_active=%4 missing_rows=%5 result_unchanged=1",
				m_sReason, m_aRows.Count(), countedActive, nativeActive, missing));
		}
		return captured;
	}
}

modded class AICF_MatchController
{
	protected bool m_bQCFinished;
	protected int m_iQCStarted;
	protected int m_iQCStage;
	protected int m_iQCChecks;
	protected int m_iQCFailures;
	protected int m_iQCTailStarted;
	protected int m_iQCTailPolls;
	protected int m_iQCLastPoll;
	protected int m_iQCTailUnknownGeneration;
	protected int m_iQCTailResets;
	protected bool m_bQCBothConfirmed;
	protected int m_iQCInitialCensusWaits;
	protected int m_iQCStockBefore;
	protected int m_iQCVehiclesBefore;
	protected ResourceName m_sQCCharacter;
	protected ref AICF_AIQuotaVehicleEntity m_QCCarIdentity;
	protected ref AICF_AIQuotaReservation m_QCPressure;
	protected int m_iQCSerial;
	protected AIGroup m_QCGroup;
	protected BaseCompartmentSlot m_QCFirst;
	protected BaseCompartmentSlot m_QCSecond;
	protected ref array<ref AICF_AIQuotaVehicleEntity> m_aQCActors = {};
	protected ref array<float> m_aQCHealth = {};

	protected void AICF_QCCheck(string rule, bool passed, string details = "")
	{
		m_iQCChecks++;
		if (!passed) m_iQCFailures++;
		AICF_AIQuotaCompartmentProbe.Log("CHECK", string.Format("rule=%1 passed=%2 details=[%3]", rule, passed, details));
	}

	protected void AICF_QCFinish(bool passed, string reason)
	{
		if (m_bQCFinished) return;
		m_bQCFinished = true;
		AICF_AIQuotaCompartmentProbe.s_Finished = true;
		AICF_AIWorldQuotaService service = AICF_AIQuotaCompartmentProbe.s_Service;
		if (service && m_QCPressure) service.CancelReservation(m_QCPressure, m_iQCSerial);
		m_QCPressure = null;
		AICF_AIQuotaCompartmentProbe.Log("DONE", string.Format(
			"passed=%1 reason=%2 checks=%3 failures=%4 factories=%5 births=%6 entries=%7 tail_polls=%8 elapsed_ms=%9",
			passed && m_iQCFailures == 0, reason, m_iQCChecks, m_iQCFailures, AICF_AIQuotaCompartmentProbe.s_Factories,
			AICF_AIQuotaCompartmentProbe.s_Births, AICF_AIQuotaCompartmentProbe.s_Entries, m_iQCTailPolls, System.GetTickCount(m_iQCStarted)));
		GetGame().GetCallqueue().CallLater(AICF_QCClose, 1500, false);
	}
	protected void AICF_QCClose() { GetGame().RequestClose(); }

	protected bool AICF_QCSafePad(vector origin, out vector result)
	{
		World world = GetGame().GetWorld();
		array<vector> directions = {"0 0 -1", "1 0 0", "0 0 1", "-1 0 0", "0.707107 0 0.707107", "-0.707107 0 0.707107", "0.707107 0 -0.707107", "-0.707107 0 -0.707107"};
		array<float> rings = {24, 40, 56};
		int queries;
		foreach (float ring : rings)
		{
			foreach (vector direction : directions)
			{
				vector center = origin + direction * ring;
				center[1] = world.GetSurfaceY(center[0], center[2]);
				vector candidate;
				queries++;
				bool found = SCR_WorldTools.FindEmptyTerrainPosition(candidate, center, 8, 8, 2, TraceFlags.ENTS | TraceFlags.OCEAN, world);
				bool clear = found && SCR_WorldTools.TraceCilinderUtil(candidate + "0 1 0", 8, 2, TraceFlags.ENTS | TraceFlags.OCEAN, world);
				AICF_AIQuotaCompartmentProbe.Log("PAD", string.Format("query=%1 center=%2 candidate=%3 found=%4 cylinder8=%5 initial_spawn_only=1", queries, center, candidate, found, clear));
				if (!clear) continue;
				result = candidate;
				return true;
			}
		}
		return false;
	}

	protected bool AICF_QCInitial()
	{
		AICF_AIWorldQuotaService service = AICF_AIWorldQuotaService.GetAuthorityForWorld(GetGame().GetWorld());
		if (!service || !service.IsOwnerCurrent() || !service.IsConfigured() || !m_USState) return false;
		AICF_GroupSlot slot = m_USState.GetSlot(0);
		IEntity original;
		if (slot) original = slot.GetRosterMember(0);
		if (!AICF_GroupRuntime.IsAliveCharacter(original)) return false;
		m_sQCCharacter = SCR_ResourceNameUtils.GetPrefabName(original);
		FactionKey faction;
		if (!AICF_AIQuotaCompartmentCreation.ReadCharacterMetadata(m_sQCCharacter, faction))
		{
			AICF_QCFinish(false, "ACTUAL_ROSTER_PREFAB_METADATA_UNKNOWN");
			return false;
		}
		vector position;
		if (!AICF_QCSafePad(original.GetOrigin(), position))
		{
			AICF_QCFinish(false, "NO_SAFE_PAD_TESTED24");
			return false;
		}
		ResourceName prefab = "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et";
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		Vehicle car = Vehicle.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), null, params));
		m_QCCarIdentity = AICF_AIQuotaVehicleEntity.Capture(car);
		if (!m_QCCarIdentity)
		{
			AICF_QCFinish(false, "FRESH_CAR_NATIVE_IDENTITY_MISSING");
			return false;
		}
		AICF_AIQuotaCompartmentProbe.s_Owner = this;
		AICF_AIQuotaCompartmentProbe.s_Service = service;
		AICF_AIQuotaCompartmentProbe.s_Car = car;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(car, seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat || seat.GetOccupant() || seat.IsReserved()) continue;
			if (seat.GetType() == ECompartmentType.PILOT) m_QCFirst = seat;
			else if (seat.GetType() == ECompartmentType.CARGO && !m_QCSecond) m_QCSecond = seat;
		}
		AICF_QCCheck("FRESH_EMPTY_TWO_SEATS", m_QCFirst && m_QCSecond && m_QCCarIdentity.IsCurrent());
		AICF_AIQuotaCompartmentProbe.Log("SETUP", string.Format("car=%1 car_rpl=%2 character_prefab=%3 prefab_faction=%4 car_prefab=%5 initial_spawn=%6",
			car.GetID(), m_QCCarIdentity.m_sRpl, m_sQCCharacter, faction, prefab, position));
		return m_iQCFailures == 0;
	}

	protected bool AICF_QCActor(IEntity actor, BaseCompartmentSlot seat)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(actor);
		AICF_AIQuotaVehicleEntity identity = AICF_AIQuotaVehicleEntity.Capture(actor);
		SCR_DamageManagerComponent damage;
		if (actor) damage = SCR_DamageManagerComponent.Cast(actor.FindComponent(SCR_DamageManagerComponent));
		CompartmentAccessComponent access;
		if (character) access = character.GetCompartmentAccessComponent();
		if (!character || !identity || !damage || !damage.GetDefaultHitZone() || !access || !AICF_GroupRuntime.IsAliveCharacter(actor)) return false;
		m_aQCActors.Insert(identity);
		m_aQCHealth.Insert(damage.GetDefaultHitZone().GetHealthScaled());
		AICF_AIQuotaCompartmentProbe.Log("BODY", string.Format("index=%1 entity=%2 rpl=%3 health=%4 exact_seat=%5 assigned_slot=%6",
			m_aQCActors.Count(), identity.m_Id, identity.m_sRpl, damage.GetDefaultHitZone().GetHealthScaled(), access.GetCompartment() == seat, seat.GetCompartmentSlotID()));
		return true;
	}

	// GetInVehicle true подтверждает dispatch, а не завершение native посадки.
	// Повторная factory не вызывается до реального exact-seat/census readback.
	protected bool AICF_QCEntrySettled(int index, BaseCompartmentSlot seat)
	{
		AICF_AIQuotaVehicleEntity identity = m_aQCActors[index];
		ChimeraCharacter actor;
		if (identity) actor = ChimeraCharacter.Cast(identity.m_Entity);
		SCR_DamageManagerComponent damage;
		CompartmentAccessComponent access;
		if (actor)
		{
			damage = SCR_DamageManagerComponent.Cast(actor.FindComponent(SCR_DamageManagerComponent));
			access = actor.GetCompartmentAccessComponent();
		}
		if (!identity || !identity.IsCurrent() || !AICF_GroupRuntime.IsAliveCharacter(actor) || !damage ||
			!damage.GetDefaultHitZone() || damage.GetDefaultHitZone().GetHealthScaled() != m_aQCHealth[index] || !access || !seat)
		{
			AICF_QCFinish(false, "PENDING_ENTRY_BODY_IDENTITY_OR_HEALTH_LOST");
			return false;
		}
		bool exact = access.GetCompartment() == seat && seat.GetOccupant() == actor;
		bool gettingIn = access.IsGettingIn();
		bool gettingOut = access.IsGettingOut();
		string actorId = identity.m_Id.ToString();
		string capturedRpl = identity.m_sRpl;
		string actualRpl = identity.m_Rpl.Id().ToString();
		AICF_AIQuotaCompartmentProbe.Log("ENTRY_READINESS", string.Format(
			"index=%1 entity=%2 captured_rpl=%3 actual_rpl=%4 exact_seat=%5 getting_in=%6 getting_out=%7 stage=%8",
			index + 1, actorId, capturedRpl, actualRpl, exact, gettingIn, gettingOut, m_iQCStage));
		return exact && !gettingIn && !gettingOut;
	}

	override protected void Update()
	{
		super.Update();
		if (!AICF_AIQuotaCompartmentProbe.Enabled() || !Replication.IsServer() || m_bQCFinished || m_bStopped ||
			AICF_MatchController.GetActiveController() != this) return;
		if (!m_iQCStarted) m_iQCStarted = System.GetTickCount();
		if (System.GetTickCount(m_iQCStarted) >= 300000) { AICF_QCFinish(false, "BOUNDED_TIMEOUT"); return; }
		if (!m_bRosterReady) return;
		if (m_iQCStage == 0)
		{
			if (!AICF_QCInitial()) return;
			m_iQCStage = 1;
			return;
		}
		AICF_AIWorldQuotaService service = AICF_AIQuotaCompartmentProbe.s_Service;
		if (!service || !service.IsOwnerCurrent() || !service.IsConfigured() || !m_QCCarIdentity || !m_QCCarIdentity.IsCurrent())
		{ AICF_QCFinish(false, "CURRENT_SERVICE_OR_CAR_LOST"); return; }
		if (m_iQCStage == 1)
		{
			if (!service.RefreshCensus())
			{
				m_iQCInitialCensusWaits++;
				AICF_AIQuotaCompartmentProbe.Log("INITIAL_CENSUS_WAIT", string.Format(
					"wait_count=%1 elapsed_ms=%2 factories=%3 births=%4 entries=%5 no_reservation_or_factory=1",
					m_iQCInitialCensusWaits, System.GetTickCount(m_iQCStarted), AICF_AIQuotaCompartmentProbe.s_Factories,
					AICF_AIQuotaCompartmentProbe.s_Births, AICF_AIQuotaCompartmentProbe.s_Entries));
				return;
			}
			m_iQCStockBefore = service.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK);
			m_iQCVehiclesBefore = service.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK_VEHICLES);
			int units = service.GetLimits().GetLimit(AICF_EAIQuotaBucket.STOCK) - m_iQCStockBefore - service.GetLedger().GetReserved(AICF_EAIQuotaBucket.STOCK);
			m_iQCSerial = service.AllocateContextSerial();
			if (units <= 0) { AICF_QCFinish(false, "NO_KNOWN_ROOM_FOR_CONTROLLED_PRESSURE"); return; }
			m_QCPressure = service.ReserveUnits(AICF_EAIQuotaBucket.STOCK, units, m_iQCSerial);
			if (!m_QCPressure) { AICF_QCFinish(false, "PRESSURE_RESERVATION_REJECTED"); return; }
			int physicalBefore = service.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK);
			IEntity rejected = m_QCFirst.SpawnCharacterInCompartment(m_sQCCharacter, m_QCGroup);
			AICF_QCCheck("FULL_STOCK_ZERO_NATIVE", !rejected && !m_QCGroup && AICF_AIQuotaCompartmentProbe.s_Factories == 0 &&
				AICF_AIQuotaCompartmentProbe.s_Births == 0 && AICF_AIQuotaCompartmentProbe.s_Entries == 0 && !m_QCFirst.GetOccupant());
			AICF_QCCheck("FULL_STOCK_PRESERVED", service.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK) == physicalBefore &&
				service.GetLedger().GetReserved(AICF_EAIQuotaBucket.STOCK) + physicalBefore == service.GetLimits().GetLimit(AICF_EAIQuotaBucket.STOCK));
			AICF_QCCheck("EXACT_PRESSURE_RELEASE", service.CancelReservation(m_QCPressure, m_iQCSerial));
			m_QCPressure = null;
			if (m_iQCFailures) { AICF_QCFinish(false, "FULL_STOCK_NEGATIVE_FAILED"); return; }
			IEntity first = m_QCFirst.SpawnCharacterInCompartment(m_sQCCharacter, m_QCGroup);
			AICF_QCCheck("FIRST_ACTUAL_NEWBORN", AICF_QCActor(first, m_QCFirst) && m_QCGroup && m_QCGroup.GetAgentsCount() == 1);
			if (m_iQCFailures) { AICF_QCFinish(false, "FIRST_NEWBORN_OR_EMPTY_GROUP_FAILED"); return; }
			m_iQCStage = 2;
			return;
		}
		if (m_iQCStage == 2)
		{
			bool firstSettled = AICF_QCEntrySettled(0, m_QCFirst);
			bool firstCensus = service.RefreshCensus();
			if (!firstSettled || !firstCensus || m_bQCFinished) return;
			AICF_QCCheck("FIRST_ENTRY_SETTLED", true);
			IEntity second = m_QCSecond.SpawnCharacterInCompartment(m_sQCCharacter, m_QCGroup);
			AICF_QCCheck("SECOND_SAME_CAR", AICF_QCActor(second, m_QCSecond) && m_QCGroup.GetAgentsCount() == 2 && m_aQCActors[0].m_Entity != second);
			AICF_QCCheck("ACTUAL_FACTORY_ENTRY_COUNT", AICF_AIQuotaCompartmentProbe.s_Factories == 2 &&
				AICF_AIQuotaCompartmentProbe.s_Births == 2 && AICF_AIQuotaCompartmentProbe.s_Entries == 2 && AICF_AIQuotaCompartmentProbe.s_BoundBeforeActivation);
			if (m_iQCFailures) { AICF_QCFinish(false, "SECOND_NEWBORN_OR_RECEIPT_FAILED"); return; }
			m_iQCStage = 3;
			return;
		}
		if (m_iQCStage == 3)
		{
			bool firstReady = AICF_QCEntrySettled(0, m_QCFirst);
			bool secondReady = AICF_QCEntrySettled(1, m_QCSecond);
			bool bothCensus = service.RefreshCensus();
			if (!firstReady || !secondReady || !bothCensus || m_bQCFinished) return;
			if (!m_bQCBothConfirmed)
			{
				AICF_QCCheck("BOTH_ENTRIES_SETTLED", true);
				m_bQCBothConfirmed = true;
			}
			m_iQCTailStarted = System.GetTickCount();
			m_iQCTailUnknownGeneration = AICF_AIQuotaCompartmentProbe.s_UnknownCaptures;
			AICF_AIQuotaCompartmentProbe.Log("TAIL_WINDOW_STARTED", string.Format(
				"window=%1 started_elapsed_ms=%2 unknown_generation=%3 reset_count=%4 known_census=1",
				m_iQCTailResets + 1, System.GetTickCount(m_iQCStarted), m_iQCTailUnknownGeneration, m_iQCTailResets));
			m_iQCStage = 4;
			return;
		}
		if (System.GetTickCount(m_iQCLastPoll) < 500) return;
		m_iQCLastPoll = System.GetTickCount();
		if (m_aQCActors.Count() != 2) { AICF_QCFinish(false, "TAIL_BODY_COUNT_CHANGED"); return; }
		for (int index; index < m_aQCActors.Count(); index++)
		{
			AICF_AIQuotaVehicleEntity identity = m_aQCActors[index];
			ChimeraCharacter actor;
			if (identity) actor = ChimeraCharacter.Cast(identity.m_Entity);
			BaseCompartmentSlot seat = m_QCFirst;
			if (index == 1) seat = m_QCSecond;
			SCR_DamageManagerComponent damage;
			if (actor) damage = SCR_DamageManagerComponent.Cast(actor.FindComponent(SCR_DamageManagerComponent));
			CompartmentAccessComponent access;
			if (actor) access = actor.GetCompartmentAccessComponent();
			if (!identity || !identity.IsCurrent() || !AICF_GroupRuntime.IsAliveCharacter(actor) || !damage || !damage.GetDefaultHitZone() ||
				damage.GetDefaultHitZone().GetHealthScaled() != m_aQCHealth[index] || !access || access.GetCompartment() != seat ||
				seat.GetOccupant() != actor || access.IsGettingIn() || access.IsGettingOut())
			{ AICF_QCFinish(false, "TAIL_EXACT_BODY_SEAT_HEALTH_FAILED"); return; }
		}
		// Физический proof проверен даже при UNKNOWN. Сбрасывается только
		// окно наблюдения; абсолютные300s и уже выполненные factory не меняются.
		bool tailCensus = service.RefreshCensus();
		if (!tailCensus || m_iQCTailUnknownGeneration != AICF_AIQuotaCompartmentProbe.s_UnknownCaptures)
		{
			m_iQCTailResets++;
			AICF_AIQuotaCompartmentProbe.Log("TAIL_RESET", string.Format(
				"window=%1 held_ms=%2 unknown_before=%3 unknown_after=%4 reset_count=%5 counted_active=%6 native_active=%7 exact_bodies_seats_health=1",
				m_iQCTailResets, System.GetTickCount(m_iQCTailStarted), m_iQCTailUnknownGeneration,
				AICF_AIQuotaCompartmentProbe.s_UnknownCaptures, m_iQCTailResets,
				AICF_AIQuotaCompartmentProbe.s_LastCountedActive, AICF_AIQuotaCompartmentProbe.s_LastNativeActive));
			m_iQCTailPolls = 0;
			m_iQCStage = 3;
			return;
		}
		bool finalFirst = AICF_QCEntrySettled(0, m_QCFirst);
		bool finalSecond = AICF_QCEntrySettled(1, m_QCSecond);
		if (!finalFirst || !finalSecond || !m_QCCarIdentity.IsCurrent() || !service.IsOwnerCurrent())
		{
			AICF_QCFinish(false, "TAIL_POST_CENSUS_IDENTITY_SEAT_HEALTH_FAILED");
			return;
		}
		m_iQCTailPolls++;
		if (System.GetTickCount(m_iQCTailStarted) < 15000) return;
		AICF_AIQuotaVehicleOwner owner = AICF_AIQuotaVehicleOwner.Capture(service, AICF_AIQuotaCompartmentProbe.s_Car);
		int exactCarBodies;
		int exactCharacterBodies;
		foreach (AICF_AIQuotaBody body : service.AICF_QCBodySnapshot())
		{
			if (body && !body.m_bReleased && body.m_bVehicle && body.m_Entity == AICF_AIQuotaCompartmentProbe.s_Car) exactCarBodies++;
			foreach (AICF_AIQuotaVehicleEntity identity : m_aQCActors)
				if (body && identity && !body.m_bReleased && !body.m_bVehicle && body.m_Entity == identity.m_Entity &&
					body.MatchesIdentity() && service.GetLedger().GetBindingBucket(body.Key()) == AICF_EAIQuotaBucket.STOCK) exactCharacterBodies++;
		}
		AICF_QCCheck("EXACT_TWO_STOCK_CHARACTERS", exactCharacterBodies == 2 && service.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK) >= 2,
			string.Format("exact_bindings=%1 global_stock_before=%2 global_stock_now=%3 global_delta_not_assumed=1", exactCharacterBodies, m_iQCStockBefore, service.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK)));
		AICF_QCCheck("ONE_STOCK_VEHICLE_HOLD", owner && owner.m_Bucket == AICF_EAIQuotaBucket.STOCK_VEHICLES && exactCarBodies == 1 &&
			service.GetLedger().GetPhysical(AICF_EAIQuotaBucket.STOCK_VEHICLES) >= 1);
		AICF_QCCheck("SETTLED_TAIL", m_iQCTailPolls >= 3 && m_QCGroup && m_QCGroup.GetAgentsCount() == 2);
		AICF_AIQuotaCompartmentProbe.Log("TAIL", string.Format("actual_tail_ms=%1 distinct_polls=%2 exact_healthy_bodies=2 exact_seats=2 exact_character_bindings=%3 exact_car_bodies=%4",
			System.GetTickCount(m_iQCTailStarted), m_iQCTailPolls, exactCharacterBodies, exactCarBodies));
		AICF_AIQuotaCompartmentProbe.Log("TAIL_WINDOW_COMPLETED", string.Format(
			"window=%1 unknown_generation=%2 reset_count=%3 elapsed_ms=%4 known_census=1",
			m_iQCTailResets + 1, m_iQCTailUnknownGeneration, m_iQCTailResets, System.GetTickCount(m_iQCStarted)));
		AICF_QCFinish(m_iQCFailures == 0, "ACTUAL_NEWBORN_HETEROGENEOUS_BASKET");
	}

	override protected void Stop(bool cleanupEntities)
	{
		AICF_AIWorldQuotaService service = AICF_AIQuotaCompartmentProbe.s_Service;
		if (service && m_QCPressure) service.CancelReservation(m_QCPressure, m_iQCSerial);
		m_QCPressure = null;
		GetGame().GetCallqueue().Remove(AICF_QCClose);
		m_aQCActors.Clear();
		super.Stop(cleanupEntities);
	}
}

modded class ArmaReforgerScripted
{
	override bool OnGameStart()
	{
		bool started = super.OnGameStart();
		if (Replication.IsServer() && AICF_AIQuotaCompartmentProbe.Enabled())
			GetGame().GetCallqueue().CallLater(AICF_AIQuotaCompartmentProbe.GlobalTimeout, 330000, false);
		return started;
	}
	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_AIQuotaCompartmentProbe.GlobalTimeout);
		super.OnGameEnd();
	}
}
