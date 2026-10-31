// Isolated test-only: 2 -> 4 -> 6 настоящих managed slots на каждую сторону.
// NORMAL сохраняет кампанию. CAPACITY явно включает truce/защиту и finite grant.
// HIERARCHY — отдельный opt-in controlled стенд, не доказательство обычной войны.
// Opt-in read-only receipt: только script fields, actual outcome и исходный nowMs.
// Weak domain refs не задерживают destruction заменённого request/plan/guard.
class AICF_MultiAcquisitionContext
{
	AICF_TransportTrip m_Trip;
	AICF_StrategicAssignmentSnapshot m_Assignment;
	AICF_StrategicAssignmentSnapshot m_InputAssignment;
	SCR_AIGroup m_Group;
	AICF_VehicleRequestState m_Request;
	AICF_VehicleSpawnPlan m_Plan;
	AICF_VehicleSpawnSiteReservation m_Reservation;
	AICF_VehicleLease m_Lease;
	bool m_bTrip;
	bool m_bAssignment;
	bool m_bGroup;
	bool m_bRequest;
	bool m_bPlan;
	bool m_bReservation;
	bool m_bLease;
	int m_iGroupGeneration;
	int m_iTripGeneration;
	int m_iAssignmentRevision;
	int m_iInputRevision;
	int m_iIntentRevision;
	int m_iRequestGeneration;
	string m_sReservationToken;
	string m_sRun;
	string m_sKey;
	string m_sFields;

	bool SameSource(AICF_MultiAcquisitionContext other)
	{
		if (!other || !m_bTrip || !m_bAssignment || !m_bGroup) return false;
		if (m_bTrip != other.m_bTrip || m_Trip != other.m_Trip) return false;
		if (m_bAssignment != other.m_bAssignment || m_Assignment != other.m_Assignment) return false;
		if (m_InputAssignment != other.m_InputAssignment || m_bGroup != other.m_bGroup || m_Group != other.m_Group) return false;
		if (m_sRun != other.m_sRun || m_sKey != other.m_sKey) return false;
		if (m_iGroupGeneration != other.m_iGroupGeneration || m_iTripGeneration != other.m_iTripGeneration) return false;
		if (m_iAssignmentRevision != other.m_iAssignmentRevision || m_iInputRevision != other.m_iInputRevision) return false;
		return m_iIntentRevision == other.m_iIntentRevision;
	}

	bool SameAcquisitionState(AICF_MultiAcquisitionContext other)
	{
		if (!other || m_bRequest != other.m_bRequest || m_Request != other.m_Request) return false;
		if (m_bPlan != other.m_bPlan || m_Plan != other.m_Plan) return false;
		if (m_bReservation != other.m_bReservation || m_Reservation != other.m_Reservation) return false;
		if (m_bLease != other.m_bLease || m_Lease != other.m_Lease) return false;
		return m_iRequestGeneration == other.m_iRequestGeneration && m_sReservationToken == other.m_sReservationToken;
	}
}

class AICF_MultiAcquisitionFrame
{
	bool m_bCompleted;
	int m_iQuoteSequence;
}

modded class AICF_TransportTrip
{
	int AICF_MultiStoredDeadlineMs() { return m_iAbsoluteDeadlineMs; }
}

class AICF_MultiAcquisitionProbe
{
	static const int MAX_RECORDS = 384;
	static const int MAX_CONTEXTS = 32;
	static ref array<string> s_Keys = {};
	static ref array<string> s_Signatures = {};
	static ref array<int> s_At = {};
	static int s_iRecords;
	static ref AICF_MultiAcquisitionFrame s_Frame;
	static bool s_bOverflow;
	static bool s_bScopeLost;

	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfVehicleAcquisitionOutcomeProbe", value) && value == "1";
	}

	static AICF_MultiAcquisitionFrame Begin()
	{
		bool unfinished = s_Frame && !s_Frame.m_bCompleted;
		s_Frame = new AICF_MultiAcquisitionFrame();
		if (unfinished) ScopeLoss("PREVIOUS_FRAME_UNFINISHED");
		return s_Frame;
	}

	static bool TakeRecord()
	{
		if (s_iRecords >= MAX_RECORDS)
		{
			if (!s_bOverflow) { s_bOverflow = true; Print("[AICF][VEHICLE_ACQUISITION_OUTCOME] event=OVERFLOW record_limit=384 context_limit=32 observations_incomplete=1 diagnostic_only=1"); }
			return false;
		}
		s_iRecords++;
		return true;
	}

	static void ScopeLoss(string cause)
	{
		s_bScopeLost = true;
		if (TakeRecord()) Print("[AICF][VEHICLE_ACQUISITION_OUTCOME] event=SCOPE_LOSS cause=" + cause + " result_saved=0 successor_cache_untouched=1 diagnostic_only=1");
	}

	static AICF_MultiAcquisitionContext Read(AICF_TransportTrip trip, AICF_StrategicAssignmentSnapshot input)
	{
		AICF_MultiAcquisitionContext context = new AICF_MultiAcquisitionContext();
		context.m_sRun = AICF_Stage1Diagnostics.GetRunId();
		context.m_Trip = trip;
		context.m_bTrip = trip != null;
		context.m_InputAssignment = input;
		if (input) context.m_iInputRevision = input.GetAssignmentRevision();
		context.m_sKey = context.m_sRun + "/MISSING_TRIP";
		context.m_sFields = "trip_present=0";
		if (!trip) return context;
		context.m_Assignment = trip.GetAssignment();
		context.m_Request = trip.GetRequestState();
		context.m_Lease = trip.GetLease();
		context.m_bAssignment = context.m_Assignment != null;
		context.m_bRequest = context.m_Request != null;
		context.m_bLease = context.m_Lease != null;
		context.m_iTripGeneration = trip.GetTripGeneration();
		context.m_sFields = string.Format("trip_present=1 operation_id=%1 phase=%2 transitions=%3 phase_started_ms=%4 trip_deadline_stored_ms=%5 input_assignment_revision=%6 deadline_scope=RAW_SCRIPT_FIELDS_NOT_EFFECTIVE_CLOCK", trip.GetOperationId(), trip.GetPhase(), trip.GetTransitionCount(), trip.GetPhaseStartedAtMs(), trip.AICF_MultiStoredDeadlineMs(), context.m_iInputRevision);
		if (context.m_Assignment)
		{
			AICF_StrategicAssignmentSnapshot assignment = context.m_Assignment;
			context.m_Group = assignment.GetGroup();
			context.m_bGroup = context.m_Group != null;
			context.m_iGroupGeneration = assignment.GetGroupGeneration();
			context.m_iAssignmentRevision = assignment.GetAssignmentRevision();
			context.m_iIntentRevision = assignment.GetStrategicIntentRevision();
			context.m_sKey = string.Format("%1/%2/%3/%4/%5/%6", context.m_sRun, assignment.GetFactionKey(), assignment.GetSlotId(), context.m_iGroupGeneration, context.m_iTripGeneration, trip.GetOperationId());
			context.m_sFields += string.Format(" faction=%1 numeric_slot=%2 group_generation=%3 trip_generation=%4 assignment_revision=%5 intent_revision=%6 base_revision=%7 group_ref_present=%8 group_identity_scope=FACTION_NUMERIC_SLOT_GENERATION_WITH_WEAK_REF", assignment.GetFactionKey(), assignment.GetSlotId(), context.m_iGroupGeneration, context.m_iTripGeneration, context.m_iAssignmentRevision, context.m_iIntentRevision, assignment.GetBaseRevision(), context.m_bGroup);
		}
		if (context.m_Request)
		{
			AICF_VehicleRequestState request = context.m_Request;
			context.m_iRequestGeneration = request.GetRequestGeneration();
			context.m_Plan = request.GetSpawnPlan();
			context.m_bPlan = context.m_Plan != null;
			context.m_sFields += string.Format(" request_generation=%1 request_deadline_ms=%2 next_attempt_ms=%3 attempt=%4 total_attempts=%5 maximum_attempts=%6 request_last_failure=%7 quota_guard_present=%8", context.m_iRequestGeneration, request.GetAbsoluteDeadlineMs(), request.GetNextAttemptAtMs(), request.GetAttemptCount(), request.GetTotalAttemptCount(), request.GetMaximumAttempts(), request.GetLastFailureReason(), request.GetQuotaCreation() != null);
		}
		if (context.m_Plan)
		{
			context.m_Reservation = context.m_Plan.GetReservation();
			context.m_bReservation = context.m_Reservation != null;
			context.m_sFields += string.Format(" plan_planned_ms=%1 plan_approach_deadline_ms=%2 plan_all_staged_since_ms=%3 plan_staged=%4 plan_alive=%5", context.m_Plan.GetPlannedAtMs(), context.m_Plan.GetApproachDeadlineMs(), context.m_Plan.GetAllStagedSinceMs(), context.m_Plan.GetStagedCount(), context.m_Plan.GetAliveCount());
		}
		if (context.m_Reservation)
		{
			AICF_VehicleSpawnSiteReservation reservation = context.m_Reservation;
			context.m_sReservationToken = reservation.GetOwnerToken();
			context.m_sFields += string.Format(" reservation_owner=%1 reservation_request_generation=%2 reservation_reserved_ms=%3 reservation_expires_ms=%4", context.m_sReservationToken, reservation.GetRequestGeneration(), reservation.GetReservedAtMs(), reservation.GetExpiresAtMs());
		}
		if (context.m_Lease)
			context.m_sFields += string.Format(" lease_generation=%1 vehicle_generation=%2 vehicle_lifecycle=%3 lease_state=%4", context.m_Lease.GetLeaseGeneration(), context.m_Lease.GetVehicleGeneration(), context.m_Lease.GetVehicleLifecycleId(), context.m_Lease.GetState());
		context.m_sFields += string.Format(" request_present=%1 plan_present=%2 reservation_present=%3 lease_present=%4 script_state_only=1", context.m_bRequest, context.m_bPlan, context.m_bReservation, context.m_bLease);
		return context;
	}

	static bool Want(string key, string signature, int nowMs)
	{
		int index = s_Keys.Find(key);
		if (index >= 0 && s_Signatures[index] == signature && nowMs >= s_At[index] && nowMs - s_At[index] < 10000) return false;
		if (index < 0 && s_Keys.Count() >= MAX_CONTEXTS)
		{
			if (!s_bOverflow) { s_bOverflow = true; Print("[AICF][VEHICLE_ACQUISITION_OUTCOME] event=OVERFLOW record_limit=384 context_limit=32 observations_incomplete=1 diagnostic_only=1"); }
			return false;
		}
		if (!TakeRecord()) return false;
		if (index < 0)
		{
			index = s_Keys.Count();
			s_Keys.Insert(key);
			s_Signatures.Insert(signature);
			s_At.Insert(nowMs);
		}
		else { s_Signatures[index] = signature; s_At[index] = nowMs; }
		return true;
	}

	static void Record(AICF_MultiAcquisitionContext before, AICF_MultiAcquisitionContext after, AICF_TripOutcome actual, int nowMs, AICF_MultiAcquisitionFrame frame)
	{
		if (!before || !after) return;
		int kind = -1;
		string reason = "NO_OUTCOME";
		string causation = "NONE";
		int retryAt;
		if (actual)
		{
			kind = actual.GetKind();
			reason = actual.GetReason();
			causation = actual.GetCausationId();
			retryAt = actual.GetRetryAtMs();
		}
		// Вложенный observer, включая reentry из getter, владеет свежим cache.
		if (!frame || frame != s_Frame) return;
		frame.m_bCompleted = true;
		bool same = before.SameSource(after);
		if (!same)
		{
			ScopeLoss("STABLE_IDENTITY_CHANGED");
			return;
		}
		string result = string.Format("actual_present=%1 actual_kind=%2 actual_reason=%3 actual_causation=%4 actual_retry_at_ms=%5 stable_identity_unchanged=%6 acquisition_context_changed=%7", actual != null, kind, reason, causation, retryAt, same, !before.SameAcquisitionState(after));
		string signature = result + before.m_sFields + after.m_sFields;
		if (!Want(before.m_sKey, signature, nowMs)) return;
		Print(string.Format("[AICF][VEHICLE_ACQUISITION_OUTCOME] event=RESULT sequence=%1 input_now_ms=%2 diagnostic_only=1 state_scope=SEQUENTIAL_BEFORE_AFTER ", s_iRecords, nowMs) + result + " BEFORE{" + before.m_sFields + "} AFTER{" + after.m_sFields + "}");
	}

	static void Stop()
	{
		s_Frame = null;
		s_Keys.Clear();
		s_Signatures.Clear();
		s_At.Clear();
		s_iRecords = 0;
		s_bOverflow = false;
		s_bScopeLost = false;
	}
}

class AICF_MultiHierarchyProtection
{
	static string SelectedScope()
	{
		string scope;
		if (!System.GetCLIParam("aicfVehicleMultiProtectionScope", scope)) return "ROOT_ONLY";
		return scope;
	}

	static bool ScopeAllowed(string mode, string scope)
	{
		if (scope == "ROOT_ONLY") return true;
		return mode == "CAPACITY" && scope == "HIERARCHY";
	}

	protected static bool UnderRoot(DamageManagerComponent root, DamageManagerComponent manager)
	{
		HitZoneContainerComponent current = manager;
		for (int depth; current && depth < 64; depth++)
		{
			if (current == root) return true;
			current = current.GetParentHitZoneContainer();
		}
		return false;
	}

	protected static bool Collect(IEntity entity, DamageManagerComponent root, array<HitZone> zones,
		array<DamageManagerComponent> managers, array<IEntity> owners, out int nativeCount, out string reason)
	{
		zones.Clear();
		managers.Clear();
		owners.Clear();
		nativeCount = root.GetAllHitZonesInHierarchy(zones);
		if (nativeCount <= 0 || nativeCount != zones.Count() || zones.Count() > 512)
		{
			reason = "INCOMPLETE_HIERARCHY";
			return false;
		}
		managers.Insert(root);
		owners.Insert(entity);
		foreach (HitZone zone : zones)
		{
			if (!zone) { reason = "NULL_ZONE"; return false; }
			DamageManagerComponent manager = DamageManagerComponent.Cast(zone.GetHitZoneContainer());
			if (!manager) { reason = "NON_DAMAGE_CONTAINER"; return false; }
			IEntity owner = manager.GetOwner();
			if (!owner || owner.GetWorld() != entity.GetWorld() || owner.FindComponent(DamageManagerComponent) != manager)
			{
				reason = "MANAGER_OWNER_MISMATCH";
				return false;
			}
			if (!UnderRoot(root, manager)) { reason = "FOREIGN_HIERARCHY"; return false; }
			if (managers.Contains(manager)) continue;
			managers.Insert(manager);
			owners.Insert(owner);
		}
		return true;
	}

	static bool Apply(IEntity entity, out string details)
	{
		details = "protection_status=REJECTED protection_errors=1 protection_reason=CONTEXT";
		string mode;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehicleMultiMode", mode) || mode != "CAPACITY") return false;
		if (SelectedScope() != "HIERARCHY" || !entity || !GetGame() || entity.GetWorld() != GetGame().GetWorld()) return false;
		EntityID identity = entity.GetID();
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl || !rpl.IsMaster() || rpl.IsProxy()) return false;
		DamageManagerComponent root = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		if (!root || root.GetOwner() != entity) return false;
		array<HitZone> zones = {};
		array<DamageManagerComponent> managers = {};
		array<IEntity> owners = {};
		int nativeCount;
		string reason;
		if (!Collect(entity, root, zones, managers, owners, nativeCount, reason))
		{
			details = "protection_status=REJECTED protection_errors=1 protection_reason=" + reason;
			return false;
		}
		int enabledBefore;
		foreach (int index, DamageManagerComponent manager : managers)
		{
			if (manager.GetOwner() != owners[index]) return false;
			if (!manager.IsDamageHandlingEnabled()) continue;
			enabledBefore++;
			manager.EnableDamageHandling(false);
		}
		array<HitZone> repeatedZones = {};
		array<DamageManagerComponent> repeatedManagers = {};
		array<IEntity> repeatedOwners = {};
		int repeatedCount;
		if (!Collect(entity, root, repeatedZones, repeatedManagers, repeatedOwners, repeatedCount, reason))
		{
			details = "protection_status=REJECTED protection_errors=1 protection_reason=READBACK_" + reason;
			return false;
		}
		int errors;
		int enabledAfter;
		if (nativeCount != repeatedCount || zones.Count() != repeatedZones.Count() || managers.Count() != repeatedManagers.Count()) errors++;
		foreach (HitZone originalZone : zones)
			if (!repeatedZones.Contains(originalZone)) errors++;
		foreach (int managerIndex, DamageManagerComponent originalManager : managers)
		{
			int repeatedIndex = repeatedManagers.Find(originalManager);
			if (repeatedIndex < 0) { errors++; continue; }
			if (repeatedOwners[repeatedIndex] != owners[managerIndex] || originalManager.GetOwner() != owners[managerIndex]) errors++;
			if (originalManager.IsDamageHandlingEnabled()) enabledAfter++;
		}
		if (entity.GetID() != identity || entity.FindComponent(RplComponent) != rpl || !rpl.IsMaster() || rpl.IsProxy()) errors++;
		if (entity.FindComponent(DamageManagerComponent) != root || root.GetOwner() != entity) errors++;
		details = string.Format("protection_errors=%1 protection_zones=%2 protection_zones_repeated=%3 protection_managers_covered=%4 protection_child_managers=%5 protection_enabled_before=%6 protection_enabled_after=%7",
			errors, nativeCount, repeatedCount, managers.Count(), managers.Count() - 1, enabledBefore, enabledAfter);
		bool complete = errors == 0 && enabledAfter == 0;
		if (complete) details += " protection_status=COVERED protection_reason=NONE";
		else details += " protection_status=REJECTED protection_reason=READBACK_FAILED";
		return complete;
	}
}

class AICF_MultiConcurrency
{
	static string Selected()
	{
		string value;
		if (!System.GetCLIParam("aicfVehicleMultiConcurrencyMode", value)) return "MOVING";
		return value;
	}

	static string Metadata(string mode)
	{
		string selected = Selected();
		string classification = "NORMAL_CAMPAIGN";
		if (mode == "CAPACITY") classification = "CONTROLLED_CAPACITY_ONLY";
		bool notMovingProof = selected == "MOUNTED_STAGING";
		if (notMovingProof) classification = "CAPACITY_MOUNTED_ONLY";
		string result = string.Format(" concurrency_mode=%1 classification=%2 notmovingproof=%3 muster_policy=PER_GROUP_BEFORE_VEHICLE", selected, classification, notMovingProof);
		string scope = AICF_MultiHierarchyProtection.SelectedScope();
		result += " protection_scope=" + scope + " test_only=1";
		return result;
	}
}

modded class AICF_EconomySystem
{
	protected int m_iAICFMultiUSGrant;
	protected int m_iAICFMultiUSSRGrant;
	protected ref array<string> m_aAICFMultiGrantTokens = {};
	protected ref AICF_MultiAcquisitionFrame m_AICFMultiPreflightFrame;
	protected int m_iAICFMultiPreflightSequence;

	int AICF_MultiGranted(string faction)
	{
		if (faction == "US") return m_iAICFMultiUSGrant;
		if (faction == "USSR") return m_iAICFMultiUSSRGrant;
		return 0;
	}

	// Exact preflight boundary. Metadata quote is readonly, not pure/atomic;
	// the original decision below owns the authoritative quote and classification.
	override AICF_VehiclePresetEconomyDecision InspectVehiclePresetPreflight(
		AICF_TransportTrip trip, SCR_CampaignFaction faction, SCR_CampaignMilitaryBaseComponent base, AICF_FactionFleet fleet)
	{
		string mode;
		bool enabled = Replication.IsServer() && System.GetCLIParam("aicfVehicleMultiMode", mode) && mode == "CAPACITY";
		if (enabled)
		{
			if (m_AICFMultiPreflightFrame) m_AICFMultiPreflightFrame.m_bCompleted = true;
			else
			{
				AICF_MultiAcquisitionFrame frame = new AICF_MultiAcquisitionFrame();
				frame.m_iQuoteSequence = ++m_iAICFMultiPreflightSequence;
				m_AICFMultiPreflightFrame = frame;
				if (trip && trip.GetVehiclePreset())
					AICF_MultiJitVehicleQuote(trip, faction, base, trip.GetVehiclePreset().m_sPrefab, "PREFLIGHT", fleet, frame);
				if (m_AICFMultiPreflightFrame == frame) m_AICFMultiPreflightFrame = null;
			}
		}
		return super.InspectVehiclePresetPreflight(trip, faction, base, fleet);
	}

	protected bool AICF_MultiPreflightScopeCurrent(AICF_TransportTrip trip, AICF_MultiAcquisitionContext before,
		AICF_MultiAcquisitionFrame frame)
	{
		if (!frame || frame != m_AICFMultiPreflightFrame || frame.m_bCompleted || !before || !trip) return false;
		if (!trip.IsVehiclePresetCurrent()) return false;
		AICF_MultiAcquisitionContext after = AICF_MultiAcquisitionProbe.Read(trip, trip.GetAssignment());
		if (frame != m_AICFMultiPreflightFrame || frame.m_bCompleted) return false;
		return before.SameSource(after) && before.SameAcquisitionState(after) && before.m_sFields == after.m_sFields;
	}

	void AICF_MultiJitVehicleQuote(AICF_TransportTrip trip, SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent base, ResourceName prefab, string stage,
		AICF_FactionFleet fleet = null, AICF_MultiAcquisitionFrame quoteFrame = null)
	{
		string mode;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehicleMultiMode", mode) || mode != "CAPACITY") return;
		if (!IsEnabled() || !m_Campaign || !m_Campaign.IsMaster() || !m_Campaign.IsRunning()) return;
		if (!m_bGraphContextReady || !m_SupplyNetwork || !m_ConflictAdapter) return;
		if (stage != "PREFLIGHT" && stage != "PURCHASE") return;
		if (!faction || base != faction.GetMainBase()) return;
		if (!m_SupplyNetwork.IsOperationalOwnedBase(base, faction)) return;
		if (!m_ConflictAdapter.GetSpawnRejectionReason(base, faction).IsEmpty()) return;
		AICF_MatchController controller = AICF_MatchController.GetActiveController();
		AICF_MultiAcquisitionContext quoteContext;
		if (stage == "PREFLIGHT")
		{
			if (!controller || !controller.AICF_MultiOwnPreflightFleet(trip, faction, fleet)) return;
			quoteContext = AICF_MultiAcquisitionProbe.Read(trip, trip.GetAssignment());
			if (!AICF_MultiPreflightScopeCurrent(trip, quoteContext, quoteFrame)) return;
		}
		int cost, started;
		if (!controller || !controller.AICF_MultiOwnVehicleQuote(trip, faction, base, prefab, cost, started)) return;
		float obligations = LogisticsObligations(base);
		int required = cost + Math.Ceil(Math.Max(0, obligations));
		int before = base.GetSupplies();
		if (stage == "PREFLIGHT" && !AICF_MultiPreflightScopeCurrent(trip, quoteContext, quoteFrame)) return;
		string token = string.Format("MULTI-JIT-%1-%2-%3-%4", trip.GetOperationId(),
			trip.GetRequestState().GetRequestGeneration(), trip.GetRequestState().GetAttemptCount(), stage);
		if (stage == "PREFLIGHT") token += "-" + quoteFrame.m_iQuoteSequence.ToString();
		int leaseGeneration = -1;
		if (trip.GetLease()) leaseGeneration = trip.GetLease().GetLeaseGeneration();
		string siteOwner = trip.GetRequestState().GetSpawnPlan().GetReservation().GetOwnerToken();
		string operation = trip.GetOperationId();
		int numericSlot = trip.GetSlotId();
		string groupId = AICF_GroupTrace.Entity(trip.GetAssignment().GetGroup());
		int generation = trip.GetGroupGeneration();
		int requestGeneration = trip.GetRequestState().GetRequestGeneration();
		int attempt = trip.GetRequestState().GetAttemptCount();
		string factionKey = faction.GetFactionKey();
		string baseKey = AICF_Stage1Diagnostics.BaseKey(base);
		if (stage == "PREFLIGHT" && !AICF_MultiPreflightScopeCurrent(trip, quoteContext, quoteFrame)) return;
		int grant = AICF_MultiFund(faction, required, token, started);
		bool scopeAfter = stage != "PREFLIGHT" || AICF_MultiPreflightScopeCurrent(trip, quoteContext, quoteFrame);
		int now = System.GetTickCount();
		string details = string.Format("[AICF][VEHICLE_MULTI_JIT_QUOTE] mode=CAPACITY faction=%1 numeric_slot=%2 t_ms=%3 elapsed_ms=%4 operation_id=%5 stage=%6 prefab=%7 quoted_cost=%8",
			factionKey, numericSlot, now, now - started, operation, stage, prefab, cost);
		details += string.Format(" token=%1 stock_before=%2 stock_after=%3 obligations=%4 required_stock=%5 grant=%6 total_granted=%7 base=%8 budget=4000 native_debit=PENDING",
			token, before, base.GetSupplies(), obligations, required, grant, AICF_MultiGranted(factionKey), baseKey);
		details += string.Format(" group=%1 generation=%2 lease_generation=%3 request_generation=%4 attempt=%5 site_owner=%6",
			groupId, generation, leaseGeneration, requestGeneration, attempt, siteOwner);
		details += string.Format(" helper_quote_scope=BEFORE_METADATA_SNAPSHOT authoritative_decision=ORIGINAL_PREFLIGHT helper_scope_after=%1", scopeAfter);
		Print(details + AICF_MultiConcurrency.Metadata(mode));
	}

	override bool PurchaseVehiclePreset(AICF_TransportTrip trip, SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent base, ResourceName prefab, out int paid)
	{
		// Никакого success override: последняя проверка цены/обязательств и debit
		// остаются в native downstream owner, включая отказ при исчерпании4k.
		AICF_MultiJitVehicleQuote(trip, faction, base, prefab, "PURCHASE");
		return super.PurchaseVehiclePreset(trip, faction, base, prefab, paid);
	}

	int AICF_MultiFund(SCR_CampaignFaction faction, int quotedDemand, string token, int started)
	{
		string mode;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfVehicleMultiMode", mode) || mode != "CAPACITY" ||
			!faction || !m_Campaign || !m_Campaign.IsMaster() || !m_Campaign.IsRunning() ||
			!m_bGraphContextReady || !m_SupplyNetwork || quotedDemand <= 0 || token.IsEmpty() ||
			m_aAICFMultiGrantTokens.Contains(token)) return 0;
		string key = faction.GetFactionKey();
		if (key != "US" && key != "USSR") return 0;
		SCR_CampaignMilitaryBaseComponent base = faction.GetMainBase();
		if (!m_SupplyNetwork.IsOperationalOwnedBase(base, faction) || base.GetSuppliesMax() <= 0) return 0;
		int before = base.GetSupplies();
		int remaining = Math.Max(0, 4000 - AICF_MultiGranted(key));
		int requested = Math.Min(remaining, Math.Max(0, Math.Min(quotedDemand, base.GetSuppliesMax()) - before));
		if (requested <= 0) return 0;
		m_aAICFMultiGrantTokens.Insert(token);
		base.AddSupplies(requested);
		int applied = Math.Max(0, base.GetSupplies() - before);
		if (key == "US") m_iAICFMultiUSGrant += applied;
		else m_iAICFMultiUSSRGrant += applied;
		int now = System.GetTickCount();
		string details = string.Format("[AICF][VEHICLE_MULTI_GRANT] mode=CAPACITY faction=%1 numeric_slot=-1 t_ms=%2 elapsed_ms=%3 token=%4 before=%5 requested=%6 applied=%7 after=%8",
			key, now, now - started, token, before, requested, applied, base.GetSupplies());
		details += string.Format(" total_granted=%1 budget=4000 quoted_demand=%2 owner=ECONOMY_SYSTEM test_only=1", AICF_MultiGranted(key), quotedDemand);
		Print(details + AICF_MultiConcurrency.Metadata(mode));
		return applied;
	}
}

modded class ArmaReforgerScripted
{
	override void OnGameEnd()
	{
		AICF_MultiAcquisitionProbe.Stop();
		super.OnGameEnd();
	}
}

modded class AICF_InfantryRecruitmentService
{
	bool AICF_MultiQuote(AICF_GroupSlot slot, SCR_CampaignFaction faction, out int cost, out int members)
	{
		return m_Spawner.QuoteMissingRoster(slot, faction, m_Config, cost, members);
	}
}

class AICF_MultiSquadCase
{
	int m_iProtectionNextLog;
	string m_sFaction;
	int m_iSlot;
	int m_iGeneration;
	EntityID m_GroupId;
	int m_iInitialAlive;
	int m_iPhase;
	int m_iNextCommand;
	bool m_bRecruitAccepted;
	ref array<EntityID> m_aMembers = {};
	ref AICF_VehiclePreset m_Recipe;
	ref AICF_TransportTrip m_Trip;
	string m_sOperation;
	EntityID m_VehicleId;
	string m_sRpl;
	int m_iLeaseGeneration;
	int m_iVehicleGeneration;
	vector m_vStart;
	vector m_vLast;
	vector m_vGoal;
	int m_iRevision;
	int m_iLeg;
	int m_iMoving;
	float m_fDisplacement;
	int m_iLastAlive = -1;
	bool m_bFinalTarget;
	bool m_bFootComplete;
	bool m_bRetainedFoot;
	ref array<string> m_aFailures = {};
}

modded class AICF_MatchController
{
	protected string m_sAICFMultiMode;
	protected string m_sAICFMultiConcurrency = "MOVING";
	protected bool m_bAICFMultiEnabled;
	protected bool m_bAICFMultiEnvironment;
	protected bool m_bAICFMultiStarted;
	protected bool m_bAICFMultiDone;
	protected bool m_bAICFMultiMustered;
	protected int m_iAICFMultiStart;
	protected int m_iAICFMultiSetupStarted;
	protected int m_iAICFMultiDuration = 1800000;
	protected int m_iAICFMultiCohort = 2;
	protected int m_iAICFMultiStableAt = -1;
	protected int m_iAICFMultiNextSample;
	protected int m_iAICFMultiNextFunding;
	protected int m_iAICFMultiSample;
	protected int m_iAICFMultiSnapshotAt = -1;
	protected int m_iAICFMultiFundingToken;
	protected int m_iAICFMultiFailures;
	protected ResourceName m_sAICFMultiUSPrefab;
	protected ResourceName m_sAICFMultiUSSRPrefab;
	protected int m_iAICFMultiUSPrice;
	protected int m_iAICFMultiUSSRPrice;
	protected ref array<ref AICF_MultiSquadCase> m_aAICFMultiCases = {};
	protected ref array<string> m_aAICFMultiFailures = {};

	override void Start(SCR_GameModeCampaign campaign, AICF_Stage1Config immutableConfig = null,
		AICF_ContentProfile contentProfile = null, string mapKey = "UNKNOWN")
	{
		m_bAICFMultiEnabled = System.GetCLIParam("aicfVehicleMultiMode", m_sAICFMultiMode) &&
			(m_sAICFMultiMode == "NORMAL" || m_sAICFMultiMode == "CAPACITY");
		m_sAICFMultiConcurrency = AICF_MultiConcurrency.Selected();
		m_iAICFMultiSetupStarted = System.GetTickCount();
		// Отношения меняются до managed spawn и первых команд, а не после контакта.
		if (m_bAICFMultiEnabled && Replication.IsServer()) AICF_MultiEnvironment();
		super.Start(campaign, immutableConfig, contentProfile, mapKey);
	}

	protected void AICF_MultiLog(string name, string faction, int slot, string details)
	{
		int now = System.GetTickCount();
		if (m_iAICFMultiSnapshotAt >= 0) now = m_iAICFMultiSnapshotAt;
		string line = string.Format("[AICF][VEHICLE_MULTI_%1] mode=%2 faction=%3 numeric_slot=%4 t_ms=%5 elapsed_ms=%6 %7",
			name, m_sAICFMultiMode, faction, slot, now, now - m_iAICFMultiStart, details);
		string metadata = AICF_MultiConcurrency.Metadata(m_sAICFMultiMode);
		Print(line + metadata);
	}

	protected void AICF_MultiFail(AICF_MultiSquadCase item, string reason)
	{
		string key = "BOTH:" + reason;
		if (item) key = item.m_sFaction + ":" + item.m_iSlot.ToString() + ":" + reason;
		if (m_aAICFMultiFailures.Contains(key)) return;
		m_aAICFMultiFailures.Insert(key);
		m_iAICFMultiFailures++;
		string faction = "BOTH";
		int slot = -1;
		if (item)
		{
			faction = item.m_sFaction;
			slot = item.m_iSlot;
			item.m_aFailures.Insert(reason);
			item.m_iPhase = 9;
		}
		AICF_MultiLog("FAILURE", faction, slot, "reason=" + reason);
	}

	protected void AICF_MultiEnvironment()
	{
		if (m_bAICFMultiEnvironment || !m_bAICFMultiEnabled || !Replication.IsServer() || !GetGame()) return;
		if (!AICF_MultiHierarchyProtection.ScopeAllowed(m_sAICFMultiMode, AICF_MultiHierarchyProtection.SelectedScope())) return;
		bool truce = false;
		if (m_sAICFMultiMode == "CAPACITY")
		{
			SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			if (!manager) return;
			array<Faction> factions = {};
			manager.GetFactionsList(factions);
			if (factions.Count() < 2) return;
			truce = true;
			for (int first; first < factions.Count(); first++)
				for (int second = first + 1; second < factions.Count(); second++)
				{
					SCR_Faction a = SCR_Faction.Cast(factions[first]);
					SCR_Faction b = SCR_Faction.Cast(factions[second]);
					if (!a || !b) continue;
					manager.SetFactionsFriendly(a, b);
					if (!a.IsFactionFriendly(b) || !b.IsFactionFriendly(a)) truce = false;
				}
			if (!truce) return;
		}
		m_bAICFMultiEnvironment = true;
		int budget;
		if (m_sAICFMultiMode == "CAPACITY") budget = 4000;
		string line = string.Format("[AICF][VEHICLE_MULTI_ENV] mode=%1 truce=%2 hold_fire=%3 damage_protection=%4 grant_budget_per_faction=%5 setup_before_roster=%6 test_only=1",
			m_sAICFMultiMode, truce, truce, truce, budget, !m_bRosterReady);
		string metadata = AICF_MultiConcurrency.Metadata(m_sAICFMultiMode);
		Print(line + metadata);
	}

	protected AICF_GroupSlot AICF_MultiSlot(AICF_MultiSquadCase item)
	{
		if (item.m_sFaction == "US" && m_USState) return m_USState.GetSlot(item.m_iSlot);
		if (item.m_sFaction == "USSR" && m_USSRState) return m_USSRState.GetSlot(item.m_iSlot);
		return null;
	}

	protected SCR_CampaignFaction AICF_MultiFaction(AICF_MultiSquadCase item)
	{
		if (item.m_sFaction == "US") return m_USFaction;
		return m_USSRFaction;
	}

	protected void AICF_MultiProtectHierarchy(IEntity entity, AICF_MultiSquadCase item, string kind, bool emit)
	{
		string details;
		bool covered = AICF_MultiHierarchyProtection.Apply(entity, details);
		bool firstFailure = !covered && !item.m_aFailures.Contains("HIERARCHY_PROTECTION_FAILED");
		if (emit || firstFailure)
		{
			string snapshot = "kind=" + kind + " entity=" + AICF_GroupTrace.Entity(entity) + " " + details;
			AICF_MultiLog("PROTECTION", item.m_sFaction, item.m_iSlot,
				snapshot);
		}
		if (!covered) AICF_MultiFail(item, "HIERARCHY_PROTECTION_FAILED");
	}

	protected void AICF_MultiProtect(AICF_GroupSlot slot, AICF_TransportTrip trip, AICF_MultiSquadCase item)
	{
		if (m_sAICFMultiMode != "CAPACITY" || !slot || !slot.GetGroup() || !Replication.IsServer()) return;
		SCR_AIGroupUtilityComponent utility = slot.GetGroup().GetGroupUtilityComponent();
		if (utility && utility.GetCombatModeExternal() != EAIGroupCombatMode.HOLD_FIRE)
			utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
		bool hierarchy = AICF_MultiHierarchyProtection.SelectedScope() == "HIERARCHY";
		bool emit = item && System.GetTickCount() >= item.m_iProtectionNextLog;
		if (emit) item.m_iProtectionNextLog = System.GetTickCount() + 5000;
		array<AIAgent> agents = {};
		slot.GetGroup().GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || agent.GetParentGroup() != slot.GetGroup()) continue;
			ChimeraCharacter member = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (hierarchy && member && item)
			{
				AICF_MultiProtectHierarchy(member, item, "MEMBER", emit);
				continue;
			}
			if (member && member.GetDamageManager() && member.GetDamageManager().IsDamageHandlingEnabled())
				member.GetDamageManager().EnableDamageHandling(false);
		}
		if (trip && trip.GetLease() && trip.GetLease().GetVehicle())
		{
			if (hierarchy && item)
			{
				AICF_MultiProtectHierarchy(trip.GetLease().GetVehicle(), item, "VEHICLE", emit);
				return;
			}
			DamageManagerComponent damage = DamageManagerComponent.Cast(trip.GetLease().GetVehicle().FindComponent(DamageManagerComponent));
			if (damage && damage.IsDamageHandlingEnabled()) damage.EnableDamageHandling(false);
		}
	}

	protected bool AICF_MultiModel(SCR_CampaignFaction faction, out ResourceName selected, out int cost)
	{
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		JsonLoadContext list = new JsonLoadContext();
		array<ResourceName> prefabs = {};
		if (!list.LoadFromString(catalog.List()) || !list.ReadValue("prefabs", prefabs)) { catalog.Clear(); return false; }
		foreach (ResourceName prefab : prefabs)
		{
			bool wanted = prefab.EndsWith("/UAZ469.et");
			if (faction.GetFactionKey() == "US") wanted = prefab.Contains("/M1025_") && prefab.Contains("M2HB");
			string reason;
			if (!wanted || catalog.Source(prefab) != AICF_EVehicleCatalogSource.FACTION ||
				!catalog.Inspect(prefab, reason) || catalog.Seats().Count() != 5 || catalog.Cost(prefab) <= 0) continue;
			selected = prefab;
			cost = catalog.Cost(prefab);
			catalog.Clear();
			return true;
		}
		catalog.Clear();
		return false;
	}

	protected bool AICF_MultiInitialize()
	{
		if (!m_bRosterReady || !m_USState || !m_USSRState || !m_USFaction || !m_USSRFaction) return false;
		for (int id; id < 6; id++)
			if (!m_USState.GetSlot(id) || !m_USState.GetSlot(id).IsCombatReady() ||
				!m_USSRState.GetSlot(id) || !m_USSRState.GetSlot(id).IsCombatReady()) return false;
		m_iAICFMultiStart = System.GetTickCount();
		m_bAICFMultiStarted = true;
		string duration;
		if (System.GetCLIParam("aicfVehicleMultiDurationMs", duration)) m_iAICFMultiDuration = Math.Clamp(duration.ToInt(), 1000, 3600000);
		AICF_MultiLog("START", "BOTH", -1, string.Format("requested_duration_ms=%1 slots_per_faction=6 crew=5 cohorts=2,4,6 stable_required_ms=30000 normal_campaign=%2", m_iAICFMultiDuration, m_sAICFMultiMode == "NORMAL"));
		if ((m_sAICFMultiConcurrency != "MOVING" && m_sAICFMultiConcurrency != "MOUNTED_STAGING") ||
			(m_sAICFMultiConcurrency == "MOUNTED_STAGING" && m_sAICFMultiMode != "CAPACITY"))
			AICF_MultiFail(null, "CONCURRENCY_MODE_INVALID");
		if (!m_bAICFMultiEnvironment) AICF_MultiFail(null, "ENVIRONMENT_NOT_CONFIGURED_BEFORE_ROSTER");
		if (!AICF_MultiModel(m_USFaction, m_sAICFMultiUSPrefab, m_iAICFMultiUSPrice) ||
			!AICF_MultiModel(m_USSRFaction, m_sAICFMultiUSSRPrefab, m_iAICFMultiUSSRPrice)) AICF_MultiFail(null, "FIVE_SEAT_FACTION_MODEL_UNAVAILABLE");
		for (int side; side < 2; side++)
			for (int slotId; slotId < 6; slotId++)
			{
				AICF_MultiSquadCase item = new AICF_MultiSquadCase();
				item.m_sFaction = "US";
				AICF_FactionState state = m_USState;
				SCR_CampaignFaction faction = m_USFaction;
				if (side == 1) { item.m_sFaction = "USSR"; state = m_USSRState; faction = m_USSRFaction; }
				item.m_iSlot = slotId;
				AICF_GroupSlot slot = state.GetSlot(slotId);
				item.m_iGeneration = slot.GetSpawnGeneration();
				item.m_GroupId = slot.GetGroup().GetID();
				item.m_iInitialAlive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
				bool infantry = slot.GetUnitType() == AICF_EGroupUnitType.INFANTRY;
				int deploymentSize = slot.GetDeploymentSize();
				// Ещё не активированные ступени остаются по одному бойцу: сначала
				// предыдущая ступень должна освободить казарму и площадки техники.
				bool configured = state.ApplyCommanderConfiguration(slotId, slot.GetRole(), AICF_EGroupUnitType.INFANTRY, 1);
				bool held = configured && faction.GetMainBase() && faction.GetMainBase().GetSpawnPoint() &&
					m_OrderPlanner.AssignPlayerPointOrder(slot, faction, faction.GetMainBase().GetSpawnPoint().GetOrigin(), false);
				m_aAICFMultiCases.Insert(item);
				AICF_MultiLog("SLOT_START", item.m_sFaction, slotId, string.Format("group=%1 generation=%2 initial_alive=%3 initial_deployment_size=%4 pre_muster_infantry=%5 desired=5 initial_desired=1 configured=%6 home_order=%7",
					AICF_GroupTrace.Entity(slot.GetGroup()), item.m_iGeneration, item.m_iInitialAlive, deploymentSize, infantry, configured, held));
				if (!infantry || deploymentSize != 1 || item.m_iInitialAlive < 1 || item.m_iInitialAlive >= 5 || !configured || !held)
					AICF_MultiFail(item, "PAID_MUSTER_INITIAL_STATE_INVALID");
			}
		return true;
	}

	protected bool AICF_MultiRoster(AICF_MultiSquadCase item, AICF_GroupSlot slot)
	{
		if (!slot || !slot.IsCombatReady() || !slot.GetGroup() || slot.GetGroup().GetID() != item.m_GroupId ||
			slot.GetSpawnGeneration() != item.m_iGeneration || AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()) != 5) return false;
		for (int position; position < 5; position++)
		{
			IEntity actor = slot.GetRosterMember(position);
			if (!AICF_GroupRuntime.IsAliveCharacter(actor) ||
				(item.m_aMembers.Count() == 5 && actor.GetID() != item.m_aMembers[position])) return false;
		}
		return true;
	}

	bool AICF_MultiOwnVehicleQuote(AICF_TransportTrip trip, SCR_CampaignFaction faction,
		SCR_CampaignMilitaryBaseComponent base, ResourceName prefab, out int cost, out int started)
	{
		if (!Replication.IsServer() || m_bStopped || !m_bAICFMultiStarted || m_bAICFMultiDone) return false;
		if (m_sAICFMultiMode != "CAPACITY" || !faction) return false;
		if (base != faction.GetMainBase()) return false;
		if (!trip || !trip.IsValid() || trip.IsTerminal()) return false;
		if (trip.IsDeadlineReached(System.GetTickCount())) return false;
		if (trip.GetPhase() != AICF_ETransportTripPhase.SPAWN_COMMIT) return false;
		if (trip.GetSlotId() < 0 || trip.GetSlotId() >= 6) return false;
		if (trip.GetFactionKey() != faction.GetFactionKey() || !trip.IsVehiclePresetCurrent()) return false;
		if (trip.IsVehiclePresetDebited()) return false;
		AICF_VehiclePreset preset = trip.GetVehiclePreset();
		if (!preset || preset.m_sPrefab != prefab) return false;
		AICF_VehicleRequestState request = trip.GetRequestState();
		if (!request) return false;
		AICF_VehicleSpawnPlan plan = request.GetSpawnPlan();
		if (!plan) return false;
		AICF_VehicleSpawnSiteSelection selection = plan.GetSelection();
		AICF_VehicleSpawnSiteReservation reservation = plan.GetReservation();
		if (!selection || !reservation) return false;
		if (selection.m_Base != base || reservation.GetBase() != base) return false;
		if (reservation.IsExpired(System.GetTickCount())) return false;
		if (!reservation.MatchesTrip(trip, request.GetRequestGeneration())) return false;
		AICF_VehicleLease lease = trip.GetLease();
		if (lease && !lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(),
			trip.GetGroupGeneration(), trip.GetTripGeneration())) return false;
		foreach (AICF_MultiSquadCase item : m_aAICFMultiCases)
		{
			if (item.m_sFaction != faction.GetFactionKey() || item.m_iSlot != trip.GetSlotId()) continue;
			if (item.m_iSlot >= m_iAICFMultiCohort || item.m_iPhase != 2 || !item.m_bRecruitAccepted) continue;
			if (item.m_aMembers.Count() != 5 || !item.m_Recipe) continue;
			if (item.m_Recipe.m_sPrefab != prefab) continue;
			AICF_GroupSlot slot = AICF_MultiSlot(item);
			if (!AICF_MultiRoster(item, slot) || trip.GetVehiclePresetSlot() != slot ||
				m_VehicleCoordinator.GetTrip(slot) != trip || trip.GetAssignment().GetGroup() != slot.GetGroup() ||
				trip.GetGroupGeneration() != item.m_iGeneration) return false;
			if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(slot.GetGroup())) return false;
			for (int member; member < 5; member++)
				if (!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(slot.GetRosterMember(member))) return false;
			AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
			cost = catalog.Cost(prefab);
			catalog.Clear();
			started = m_iAICFMultiStart;
			return cost > 0;
		}
		return false;
	}

	bool AICF_MultiOwnPreflightFleet(AICF_TransportTrip trip, SCR_CampaignFaction faction, AICF_FactionFleet fleet)
	{
		if (!trip || !faction || !fleet || !m_VehicleCoordinator) return false;
		AICF_FleetRegistry registry = m_VehicleCoordinator.GetAIQuotaFleetRegistry();
		if (!registry || registry.FindFleet(faction.GetFactionKey()) != fleet) return false;
		if (fleet.GetFactionKey() != trip.GetFactionKey()) return false;
		AICF_VehicleLease lease = trip.GetLease();
		if (!lease) return !fleet.FindLeaseForSlot(trip.GetSlotId());
		return AICF_VehiclePresetEconomyDecision.IsOwnUnusedReservation(trip, fleet, lease, lease.GetLeaseGeneration());
	}

	protected bool AICF_MultiExact(AICF_MultiSquadCase item, AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		if (!AICF_MultiRoster(item, slot) || !trip || trip.GetOperationId() != item.m_sOperation ||
			trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT || !trip.IsVehiclePresetCurrent() || !trip.GetLease() || !item.m_Recipe) return false;
		AICF_VehicleLease lease = trip.GetLease();
		Vehicle vehicle = lease.GetVehicle();
		if (!vehicle || lease.GetState() != AICF_EVehicleLeaseState.ACTIVE ||
			!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle) ||
			!lease.MatchesTripIdentity(item.m_sFaction, item.m_iSlot, item.m_iGeneration, trip.GetTripGeneration()) ||
			!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), lease.GetRplId())) return false;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl || rpl.Id().ToString() != lease.GetRplId()) return false;
		if (item.m_iPhase >= 3 && (vehicle.GetID() != item.m_VehicleId || lease.GetRplId() != item.m_sRpl ||
			lease.GetLeaseGeneration() != item.m_iLeaseGeneration || lease.GetVehicleGeneration() != item.m_iVehicleGeneration)) return false;
		array<BaseCompartmentSlot> seats = {};
		AICF_VehiclePreset.Collect(vehicle, seats);
		if (seats.Count() != 5 || !item.m_Recipe.MatchesSeats(seats)) return false;
		for (int index; index < seats.Count(); index++)
		{
			int position = item.m_Recipe.m_aMembers[index];
			if (position < 0 || position >= 5) return false;
			ChimeraCharacter actor = ChimeraCharacter.Cast(slot.GetRosterMember(position));
			CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
			if (!access || !actor.IsInVehicle() || !access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim() ||
				access.GetCompartment() != seats[index] || seats[index].GetOccupant() != actor || CompartmentAccessComponent.GetVehicleIn(actor) != vehicle) return false;
		}
		return true;
	}

	protected void AICF_MultiFunding()
	{
		for (int side; side < 2; side++)
		{
			SCR_CampaignFaction faction = m_USFaction;
			int price = m_iAICFMultiUSPrice;
			if (side == 1) { faction = m_USSRFaction; price = m_iAICFMultiUSSRPrice; }
			if (!faction || !faction.GetMainBase()) continue;
			int hire;
			int vehicles;
			foreach (AICF_MultiSquadCase item : m_aAICFMultiCases)
			{
				if (item.m_sFaction != faction.GetFactionKey() || item.m_iPhase == 9) continue;
				if (item.m_iSlot >= m_iAICFMultiCohort) continue;
				int cost, missing;
				if (item.m_iPhase == 0 && m_InfantryRecruitment.AICF_MultiQuote(AICF_MultiSlot(item), faction, cost, missing)) hire += cost;
				if (item.m_iPhase >= 1 && item.m_iPhase <= 2 && item.m_bRecruitAccepted && item.m_aMembers.Count() == 5)
				{
					if (!item.m_Trip || !item.m_Trip.IsVehiclePresetDebited()) vehicles += price;
				}
			}
			int before = faction.GetMainBase().GetSupplies();
			float income, interval, next;
			faction.GetMainBase().AICF_GetRecruitmentIncome(income, interval, next);
			int grant;
			if (m_sAICFMultiMode == "CAPACITY")
			{
				m_iAICFMultiFundingToken++;
				grant = m_EconomySystem.AICF_MultiFund(faction, hire + vehicles,
					"MULTI-" + faction.GetFactionKey() + "-" + m_iAICFMultiFundingToken.ToString(), m_iAICFMultiStart);
			}
			string details = string.Format("stock_before=%1 stock_after=%2 quoted_hire=%3 quoted_vehicles=%4 vehicle_unit_price=%5 income=%6 income_interval_s=%7 next_income_s=%8 grant=%9",
				before, faction.GetMainBase().GetSupplies(), hire, vehicles, price, income, interval, next, grant);
			int budget;
			if (m_sAICFMultiMode == "CAPACITY") budget = 4000;
			details += string.Format(" total_granted=%1 budget=%2", m_EconomySystem.AICF_MultiGranted(faction.GetFactionKey()), budget);
			AICF_MultiLog("FUNDING", faction.GetFactionKey(), -1, details);
		}
	}

	protected bool AICF_MultiMountedWait(AICF_MultiSquadCase item, AICF_TransportTrip trip)
	{
		return item.m_iLeg == 0 && trip && !trip.IsDeadlineReached(System.GetTickCount()) &&
			trip.IsPresetWaitingForDestination() && trip.GetMovementState().GetRouteGeneration() == 0 &&
			!trip.GetMovementState().GetRouteWaypoint() && !trip.GetMovementState().GetSupersededRouteWaypoint();
	}

	protected bool AICF_MultiApply(AICF_MultiSquadCase item, AICF_GroupSlot slot, SCR_CampaignFaction faction)
	{
		if (item.m_iPhase != 1 || !item.m_bRecruitAccepted || item.m_aMembers.Count() != 5 || !AICF_MultiRoster(item, slot))
		{
			AICF_MultiFail(item, "OWN_PAID_MUSTER_REQUIRED_BEFORE_VEHICLE");
			return false;
		}
		ResourceName prefab = m_sAICFMultiUSPrefab;
		int cost = m_iAICFMultiUSPrice;
		AICF_FactionState state = m_USState;
		if (item.m_sFaction == "USSR") { prefab = m_sAICFMultiUSSRPrefab; cost = m_iAICFMultiUSSRPrice; state = m_USSRState; }
		AICF_VehiclePresetCatalog catalog = new AICF_VehiclePresetCatalog(faction);
		string reason;
		if (!catalog.Inspect(prefab, reason) || catalog.Seats().Count() != 5) { catalog.Clear(); AICF_MultiFail(item, "MODEL_INSPECTION_FAILED"); return false; }
		AICF_VehiclePreset recipe = catalog.Draft(prefab);
		int member;
		for (int mandatory; mandatory < catalog.Seats().Count(); mandatory++)
			if (catalog.Seats()[mandatory].GetType() == ECompartmentType.PILOT || catalog.Seats()[mandatory].GetType() == ECompartmentType.TURRET) recipe.Assign(mandatory, member++);
		for (int seat; seat < catalog.Seats().Count(); seat++) if (recipe.m_aMembers[seat] < 0) recipe.Assign(seat, member++);
		bool valid = member == 5 && recipe.ValidateRoster(slot, catalog.Seats(), reason) && PreflightVehiclePreset(slot, faction, recipe, reason);
		catalog.Clear();
		if (!valid) { AICF_MultiLog("VEHICLE_WAIT", item.m_sFaction, item.m_iSlot, "reason=" + reason); return false; }
		bool configured = state.ApplyCommanderConfiguration(item.m_iSlot, slot.GetRole(), AICF_EGroupUnitType.MOTORIZED_LIGHT, 5);
		if (configured) slot.TouchCommanderConfiguration();
		if (!configured || !slot.SetVehiclePreset(recipe, slot.GetVehiclePresetRevision())) { AICF_MultiFail(item, "PRESET_APPLY_FAILED"); return false; }
		item.m_Recipe = recipe;
		item.m_iPhase = 2;
		AICF_MultiLog("VEHICLE", item.m_sFaction, item.m_iSlot, string.Format("prefab=%1 seats=5 assigned=5 cost=%2 revision=%3 paid_muster_before_motorized=1", prefab, cost, slot.GetVehiclePresetRevision()));
		return true;
	}

	protected bool AICF_MultiRoad(AICF_MultiSquadCase item, AICF_GroupSlot slot, SCR_CampaignFaction faction, vector origin, out vector target)
	{
		AIPathfindingComponent path = AIPathfindingComponent.Cast(slot.GetGroup().FindComponent(AIPathfindingComponent));
		SCR_AIWorld world = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!path || !path.GetNavmeshComponent() || !world || !world.GetRoadNetworkManager() || !faction.GetMainBase().GetOwner()) return false;
		array<BaseRoad> roads = {};
		world.GetRoadNetworkManager().GetRoadsInAABB(origin - "1500 1500 1500", origin + "1500 1500 1500", roads);
		float best = 1400;
		bool found;
		foreach (BaseRoad road : roads)
		{
			if (!road) continue;
			array<vector> points = {};
			road.GetPoints(points);
			foreach (vector query : points)
			{
				float distance = vector.DistanceXZ(origin, query);
				if (distance < 650 || distance >= best || vector.DistanceXZ(query, faction.GetMainBase().GetOwner().GetOrigin()) > 2000 ||
					(item.m_iLeg > 0 && vector.DistanceXZ(query, item.m_vGoal) < 250)) continue;
				bool blocked;
				for (int node; node < m_ObjectiveGraph.GetNodeCount(); node++)
				{
					SCR_CampaignMilitaryBaseComponent base = m_ObjectiveGraph.GetNode(node).GetBase();
					if (base && base.GetOwner() && base != faction.GetMainBase() && vector.DistanceXZ(query, base.GetOwner().GetOrigin()) < 300) blocked = true;
				}
				foreach (AICF_MultiSquadCase other : m_aAICFMultiCases)
					if (other != item && other.m_sFaction == item.m_sFaction && other.m_iLeg > 0 && vector.DistanceXZ(query, other.m_vGoal) < 75) blocked = true;
				if (blocked) continue;
				path.GetNavmeshComponent().LoadTileIn(query);
				vector nav;
				if (!path.GetClosestPositionOnNavmesh(query, "20 10 20", nav) ||
					ChimeraWorldUtils.TryGetWaterSurfaceSimple(GetGame().GetWorld(), nav - "0 0.5 0")) continue;
				target = nav;
				best = distance;
				found = true;
			}
		}
		return found;
	}

	protected bool AICF_MultiOrder(AICF_MultiSquadCase item, AICF_GroupSlot slot, SCR_CampaignFaction faction, bool finalTarget)
	{
		vector target;
		if (!AICF_MultiRoad(item, slot, faction, item.m_vLast, target)) return false;
		if (!m_OrderPlanner.AssignPlayerPointOrder(slot, faction, target, m_VehicleCoordinator.IsInfantryOrderSuspended(slot))) return false;
		item.m_vGoal = target;
		item.m_iRevision = slot.GetStrategicIntentRevision();
		item.m_iLeg++;
		item.m_bFinalTarget = finalTarget;
		string name = "TARGET";
		if (finalTarget) { name = "FINAL_TARGET"; item.m_iPhase = 4; }
		AICF_MultiLog(name, item.m_sFaction, item.m_iSlot, string.Format("operation_id=%1 vehicle=%2 leg=%3 position=%4 intent_revision=%5 dismount_radius_m=%6 final=%7",
			item.m_sOperation, item.m_VehicleId.ToString(), item.m_iLeg, target, item.m_iRevision, m_Stage3Config.GetDismountDistanceMeters(), finalTarget));
		return true;
	}

	protected bool AICF_MultiCurrentFinalIntent(AICF_MultiSquadCase item, AICF_GroupSlot slot)
	{
		return slot && item.m_bFinalTarget && slot.HasPlayerStrategicIntent() &&
			slot.GetDecisionAuthority() == AICF_EStrategicDecisionAuthority.PLAYER_COMMAND &&
			slot.GetPlayerStrategicIntentTargetKind() == AICF_EOrderTargetKind.POSITION &&
			slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION &&
			slot.GetStrategicIntentRevision() == item.m_iRevision &&
			slot.GetPlayerStrategicIntentTargetPosition() == item.m_vGoal &&
			slot.GetTargetPosition() == item.m_vGoal;
	}

	// Новый manual endpoint сохраняет оплаченный lease. Это physical FOOT
	// при восстановленном приказе, а не выдуманный normal COMPLETE.
	protected bool AICF_MultiRetainedFoot(AICF_MultiSquadCase item, AICF_GroupSlot slot, AICF_TransportTrip trip, bool publish = true)
	{
		if (!AICF_MultiRoster(item, slot) || !AICF_MultiCurrentFinalIntent(item, slot) || !trip ||
			trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || trip.GetOperationId() != item.m_sOperation ||
			!trip.IsVehiclePresetCurrent() || !trip.GetVehiclePreset() || !trip.GetLease() ||
			trip.GetBehaviorState().m_bReboarding || !trip.GetHandoffState().IsOrderRestored()) return false;
		AICF_VehicleLease lease = trip.GetLease();
		Vehicle vehicle = lease.GetVehicle();
		if (!vehicle || lease.GetState() != AICF_EVehicleLeaseState.ACTIVE || vehicle.GetID() != item.m_VehicleId ||
			lease.GetRplId() != item.m_sRpl || lease.GetLeaseGeneration() != item.m_iLeaseGeneration ||
			lease.GetVehicleGeneration() != item.m_iVehicleGeneration) return false;
		if (!lease.MatchesTripIdentity(item.m_sFaction, item.m_iSlot, item.m_iGeneration, trip.GetTripGeneration()) ||
			!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), AICF_VehicleBehaviorBinding.ComponentRpl(vehicle))) return false;
		AICF_VehicleWatchdog watchdog = new AICF_VehicleWatchdog();
		int logical, transitions, insideBounds, nonAlive;
		string samples;
		if (!watchdog.InspectProtectedMemberDismountClearance(slot.GetGroup(), vehicle, logical, transitions, insideBounds, nonAlive, samples)) return false;
		vector boundsMin, boundsMax;
		vehicle.GetBounds(boundsMin, boundsMax);
		array<string> receipts = {};
		for (int position; position < 5; position++)
		{
			ChimeraCharacter actor = ChimeraCharacter.Cast(slot.GetRosterMember(position));
			if (!actor || actor.GetID() != item.m_aMembers[position] || !AICF_VehicleShortRecovery.IsCurrentMember(trip, actor)) return false;
			CompartmentAccessComponent access = actor.GetCompartmentAccessComponent();
			if (!access) return false;
			bool linked = CompartmentAccessComponent.GetVehicleIn(actor) != null;
			bool compartment = access.IsInCompartment();
			bool gettingIn = access.IsGettingIn();
			bool gettingOut = access.IsGettingOut();
			bool characterVehicle = actor.IsInVehicle();
			vector local = vehicle.CoordToLocal(actor.GetOrigin());
			bool inside = local[0] >= boundsMin[0] - 0.5 && local[0] <= boundsMax[0] + 0.5 &&
				local[1] >= boundsMin[1] - 0.5 && local[1] <= boundsMax[1] + 0.5 &&
				local[2] >= boundsMin[2] - 0.5 && local[2] <= boundsMax[2] + 0.5;
			if (linked || compartment || gettingIn || gettingOut || characterVehicle || inside) return false;
			if (trip.GetLastTransitionReason() == "LOCAL_THREAT" && vector.DistanceXZ(actor.GetOrigin(), item.m_vGoal) > 35) return false;
			string receipt = string.Format("operation_id=%1 position=%2 member=%3 linked=%4 in_compartment=%5 getting_in=%6 getting_out=%7 character_vehicle=%8 inside_bounds=%9",
				item.m_sOperation, position, actor.GetID(), linked, compartment, gettingIn, gettingOut, characterVehicle, inside);
			receipts.Insert(receipt);
		}
		if (!publish) return true;
		foreach (string receipt : receipts) AICF_MultiLog("RETAINED_MEMBER", item.m_sFaction, item.m_iSlot, receipt);
		string details = string.Format("operation_id=%1 vehicle=%2 rpl=%3 lease_generation=%4 vehicle_generation=%5 logical_occupants=%6 transitions=%7 inside_bounds=%8 non_alive=%9",
			item.m_sOperation, vehicle.GetID(), lease.GetRplId(), lease.GetLeaseGeneration(), lease.GetVehicleGeneration(), logical, transitions, insideBounds, nonAlive);
		details += " phase=TACTICAL_DEPLOYMENT order_restored=1 original_members=5";
		AICF_MultiLog("RETAINED_CLEARANCE", item.m_sFaction, item.m_iSlot, details);
		return true;
	}

	protected bool AICF_MultiTemporaryDeployment(AICF_MultiSquadCase item, AICF_GroupSlot slot, AICF_TransportTrip trip)
	{
		if (!AICF_MultiRoster(item, slot) || !trip || !trip.IsVehiclePresetCurrent() || !trip.GetLease() ||
			trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || trip.GetOperationId() != item.m_sOperation) return false;
		AICF_VehicleLease lease = trip.GetLease();
		Vehicle vehicle = lease.GetVehicle();
		if (!vehicle || vehicle.GetID() != item.m_VehicleId || lease.GetRplId() != item.m_sRpl ||
			lease.GetLeaseGeneration() != item.m_iLeaseGeneration || lease.GetVehicleGeneration() != item.m_iVehicleGeneration ||
			lease.GetState() != AICF_EVehicleLeaseState.ACTIVE) return false;
		if (!lease.MatchesTripIdentity(item.m_sFaction, item.m_iSlot, item.m_iGeneration, trip.GetTripGeneration()) ||
			!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), AICF_VehicleBehaviorBinding.ComponentRpl(vehicle))) return false;
		if (trip.GetAssignment().GetStrategicIntentRevision() != slot.GetStrategicIntentRevision() ||
			trip.GetAssignment().GetTargetPosition() != slot.GetTargetPosition()) return false;
		return item.m_iLeg == 0 || (slot.GetStrategicIntentRevision() == item.m_iRevision && slot.GetTargetPosition() == item.m_vGoal);
	}

	protected void AICF_MultiTick(AICF_MultiSquadCase item, int now)
	{
		AICF_GroupSlot slot = AICF_MultiSlot(item);
		SCR_CampaignFaction faction = AICF_MultiFaction(item);
		if (!slot || !slot.GetGroup() || !faction) { AICF_MultiFail(item, "GROUP_UNAVAILABLE"); return; }
		int alive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup());
		if (item.m_iLastAlive >= 0 && alive < item.m_iLastAlive)
			AICF_MultiLog("ALIVE_CHANGE", item.m_sFaction, item.m_iSlot, string.Format("previous_alive=%1 alive=%2 cause=UNDETERMINED route_complete=%3", item.m_iLastAlive, alive, item.m_bFootComplete));
		item.m_iLastAlive = alive;
		if (!item.m_bFootComplete && (slot.GetGroup().GetID() != item.m_GroupId || slot.GetSpawnGeneration() != item.m_iGeneration)) AICF_MultiFail(item, "GROUP_REPLACED");
		if (item.m_iPhase == 9 || item.m_iPhase == 7) return;
		if (item.m_iPhase >= 4 && !AICF_MultiCurrentFinalIntent(item, slot))
		{
			AICF_MultiFail(item, "FINAL_PLAYER_POINT_INTENT_CHANGED");
			return;
		}
		if (item.m_iPhase == 0)
		{
			if (item.m_iSlot >= m_iAICFMultiCohort) return;
			if (slot.GetDesiredSize() != 5)
			{
				AICF_FactionState state = m_USState;
				if (item.m_sFaction == "USSR") state = m_USSRState;
				if (!state.ApplyCommanderConfiguration(item.m_iSlot, slot.GetRole(), AICF_EGroupUnitType.INFANTRY, 5))
				{
					AICF_MultiFail(item, "COHORT_MUSTER_CONFIGURATION_FAILED");
					return;
				}
				AICF_MultiLog("ACTIVATED", item.m_sFaction, item.m_iSlot, string.Format("cohort=%1 desired=5 prior_desired=1 paid_muster=1", m_iAICFMultiCohort));
			}
			if (alive == 5 && !slot.IsRecruitingInfantry() && item.m_bRecruitAccepted && AICF_MultiRoster(item, slot))
			{
				for (int position; position < 5; position++) item.m_aMembers.Insert(slot.GetRosterMember(position).GetID());
				item.m_iPhase = 1;
				item.m_iNextCommand = now;
				AICF_MultiLog("RECRUITED", item.m_sFaction, item.m_iSlot, string.Format("alive=5 desired=5 initial_alive=%1 group=%2 generation=%3", item.m_iInitialAlive, AICF_GroupTrace.Entity(slot.GetGroup()), item.m_iGeneration));
				for (int p; p < 5; p++) AICF_MultiLog("MEMBER", item.m_sFaction, item.m_iSlot, string.Format("position=%1 member=%2", p, AICF_GroupTrace.Entity(slot.GetRosterMember(p))));
			}
			else if (now >= item.m_iNextCommand)
			{
				item.m_iNextCommand = now + 10000;
				string reason;
				bool accepted = m_InfantryRecruitment.RequestPlayerRecruitment(slot, faction, reason);
				if (accepted) item.m_bRecruitAccepted = true;
				AICF_MultiLog("RECRUIT_REQUEST", item.m_sFaction, item.m_iSlot, string.Format("accepted=%1 reason=%2 alive=%3 desired=5 authority_path=SERVER_FIXTURE_DOWNSTREAM", accepted, reason, alive));
			}
			return;
		}
		if (!AICF_MultiRoster(item, slot)) { AICF_MultiFail(item, "ORIGINAL_ROSTER_CHANGED"); return; }
		if (item.m_iPhase == 1)
		{
			if (item.m_iSlot < m_iAICFMultiCohort && now >= item.m_iNextCommand)
			{
				item.m_iNextCommand = now + 10000;
				AICF_MultiApply(item, slot, faction);
			}
			return;
		}
		AICF_TransportTrip active = m_VehicleCoordinator.GetTrip(slot);
		if (active && item.m_sOperation.IsEmpty())
		{
			item.m_Trip = active;
			item.m_sOperation = active.GetOperationId();
			AICF_MultiLog("TRIP", item.m_sFaction, item.m_iSlot, "operation_id=" + item.m_sOperation);
		}
		if (active && active.GetOperationId() != item.m_sOperation) { AICF_MultiFail(item, "TRIP_REPLACED"); return; }
		AICF_TransportTrip trip = item.m_Trip;
		if (!trip) return;
		if (trip.GetLease() && trip.GetLease().GetVehicle())
		{
			Vehicle vehicle = trip.GetLease().GetVehicle();
			item.m_vLast = vehicle.GetOrigin();
			if (item.m_iPhase >= 3)
			{
				if (vehicle.GetID() != item.m_VehicleId) { AICF_MultiFail(item, "VEHICLE_REPLACED"); return; }
				item.m_fDisplacement = Math.Max(item.m_fDisplacement, vector.DistanceXZ(item.m_vStart, item.m_vLast));
				if (vehicle.GetPhysics() && vehicle.GetPhysics().GetVelocity().Length() > 1) item.m_iMoving++;
			}
		}
		if (trip.IsTerminal() && trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE) { AICF_MultiFail(item, trip.GetTerminalReason()); return; }
		if (item.m_iPhase == 2 && trip.GetPhase() == AICF_ETransportTripPhase.TRANSIT)
		{
			if (!AICF_MultiExact(item, slot, trip)) { AICF_MultiFail(item, "EXACT_BOARDING_NOT_CONFIRMED"); return; }
			AICF_VehicleLease lease = trip.GetLease();
			item.m_VehicleId = lease.GetVehicle().GetID();
			item.m_sRpl = lease.GetRplId();
			item.m_iLeaseGeneration = lease.GetLeaseGeneration();
			item.m_iVehicleGeneration = lease.GetVehicleGeneration();
			item.m_vStart = item.m_vLast;
			item.m_iPhase = 3;
			string details = string.Format("operation_id=%1 vehicle=%2 rpl=%3 lease_generation=%4 vehicle_generation=%5 seats=5 occupants=5 start=%6",
				item.m_sOperation, item.m_VehicleId.ToString(), item.m_sRpl, item.m_iLeaseGeneration, item.m_iVehicleGeneration, item.m_vStart);
			AICF_MultiLog("BOARDED", item.m_sFaction, item.m_iSlot, details);
			item.m_iNextCommand = now;
		}
		if (item.m_iPhase == 3)
		{
			// Временный пеший бой сохраняет trip. Пока он не вернулся в TRANSIT,
			// Sample не считает машину активной и честно сбрасывает stable30s.
			if (AICF_MultiTemporaryDeployment(item, slot, trip)) return;
			if (trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT) { AICF_MultiFail(item, "TRIP_ENDED_BEFORE_CAPACITY_COHORTS"); return; }
			// Отдельный stationary oracle: штатный preset ждёт первую цель.
			// Site selection/clearance остаются production; занятый pad не обходится.
			if (m_sAICFMultiConcurrency == "MOUNTED_STAGING" && m_iAICFMultiCohort <= 6)
			{
				if (item.m_iLeg != 0) AICF_MultiFail(item, "MOUNTED_STAGING_EARLY_TARGET");
				return;
			}
			if (now >= item.m_iNextCommand && (item.m_iLeg == 0 || m_iAICFMultiCohort == 8 ||
				vector.DistanceXZ(item.m_vLast, item.m_vGoal) <= m_Stage3Config.GetDismountDistanceMeters() * 2 + 100))
			{
				item.m_iNextCommand = now + 2000;
				if (!AICF_MultiOrder(item, slot, faction, m_iAICFMultiCohort == 8))
					AICF_MultiLog("TARGET_WAIT", item.m_sFaction, item.m_iSlot, "reason=NO_DISTINCT_RUNTIME_ROAD_NAVMESH_TARGET");
			}
		}
		if (item.m_iPhase == 4 && trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT)
		{
			float distance = vector.DistanceXZ(item.m_vLast, item.m_vGoal);
			bool passed = distance <= m_Stage3Config.GetDismountDistanceMeters() + 10 && item.m_fDisplacement >= 100 && item.m_iMoving >= 5 && slot.GetStrategicIntentRevision() == item.m_iRevision;
			AICF_MultiLog("ARRIVAL", item.m_sFaction, item.m_iSlot, string.Format("operation_id=%1 passed=%2 vehicle=%3 distance_m=%4 displacement_m=%5 moving_polls=%6 intent_revision=%7 leg=%8", item.m_sOperation, passed, item.m_VehicleId.ToString(), distance, item.m_fDisplacement, item.m_iMoving, slot.GetStrategicIntentRevision(), item.m_iLeg));
			if (!passed) { AICF_MultiFail(item, "PREMATURE_FINAL_TRANSIT_EXIT"); return; }
			item.m_iPhase = 5;
		}
		if (item.m_iPhase == 5 && trip.GetPhase() == AICF_ETransportTripPhase.COMPLETE)
		{
			if (AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(slot.GetGroup()) != 0) { AICF_MultiFail(item, "NORMAL_COMPLETE_STILL_MOUNTED"); return; }
			item.m_iPhase = 6;
			AICF_MultiLog("DISMOUNTED", item.m_sFaction, item.m_iSlot, string.Format("operation_id=%1 alive=5 in_vehicle=0 normal_complete=1 intent_revision=%2 position=%3", item.m_sOperation, slot.GetStrategicIntentRevision(), slot.GetTargetPosition()));
		}
		if (item.m_iPhase == 5 && AICF_MultiRetainedFoot(item, slot, trip))
		{
			item.m_iPhase = 6;
			item.m_bRetainedFoot = true;
			AICF_MultiLog("DISMOUNTED", item.m_sFaction, item.m_iSlot, string.Format("operation_id=%1 alive=5 in_vehicle=0 normal_complete=0 retained_manual=1 phase=TACTICAL_DEPLOYMENT order_restored=1 intent_revision=%2 position=%3", item.m_sOperation, slot.GetStrategicIntentRevision(), slot.GetTargetPosition()));
		}
		if (item.m_iPhase == 6)
		{
			if (item.m_bRetainedFoot && !AICF_MultiRetainedFoot(item, slot, trip, false)) return;
			float farthest;
			for (int actor; actor < 5; actor++) farthest = Math.Max(farthest, vector.DistanceXZ(slot.GetRosterMember(actor).GetOrigin(), item.m_vGoal));
			if (farthest <= 35)
			{
				item.m_bFootComplete = true;
				item.m_iPhase = 7;
				string retainedDetails;
				if (item.m_bRetainedFoot) retainedDetails = " retained_manual=1 in_vehicle=0 phase=TACTICAL_DEPLOYMENT order_restored=1";
				AICF_MultiLog("FOOT_ARRIVAL", item.m_sFaction, item.m_iSlot, string.Format("operation_id=%1 alive=5 farthest_m=%2 position=%3 intent_revision=%4 leg=%5", item.m_sOperation, farthest, slot.GetTargetPosition(), slot.GetStrategicIntentRevision(), item.m_iLeg) + retainedDetails);
			}
		}
	}

	protected void AICF_MultiSample(int now)
	{
		// Один настоящий timestamp решения связывает все строки sample и cohort.
		now = System.GetTickCount();
		m_iAICFMultiSnapshotAt = now;
		m_iAICFMultiSample++;
		int usExact, ussrExact, completed, mustered;
		array<string> vehicles = {};
		array<string> rplIds = {};
		foreach (AICF_MultiSquadCase item : m_aAICFMultiCases)
		{
			AICF_GroupSlot slot = AICF_MultiSlot(item);
			AICF_TransportTrip trip;
			if (slot) trip = m_VehicleCoordinator.GetTrip(slot);
			bool concurrencyProof = item.m_iLeg > 0 && item.m_fDisplacement >= 25 && item.m_iMoving > 0;
			if (m_sAICFMultiConcurrency == "MOUNTED_STAGING") concurrencyProof = AICF_MultiMountedWait(item, trip);
			bool exact = item.m_iPhase == 3 && concurrencyProof && AICF_MultiExact(item, slot, trip);
			if (exact)
			{
				string vehicle = item.m_VehicleId.ToString();
				if (vehicles.Contains(vehicle) || rplIds.Contains(item.m_sRpl)) { AICF_MultiFail(item, "VEHICLE_OR_RPL_SHARED_BETWEEN_SLOTS"); exact = false; }
				else { vehicles.Insert(vehicle); rplIds.Insert(item.m_sRpl); }
			}
			if (exact) { if (item.m_sFaction == "US") usExact++; else ussrExact++; }
			if (item.m_iPhase >= 1 && item.m_iPhase != 9) mustered++;
			if (item.m_bFootComplete) completed++;
			int alive;
			string group = "NONE";
			if (slot) { alive = AICF_GroupRuntime.CountAliveAgents(slot.GetGroup()); group = AICF_GroupTrace.Entity(slot.GetGroup()); }
			string phase = "NONE";
			if (trip) phase = AICF_Stage3Diagnostics.TripPhaseToString(trip.GetPhase());
			int protectedMembers;
			bool holdFire, vehicleProtected;
			if (slot && slot.GetGroup())
			{
				SCR_AIGroupUtilityComponent utility = slot.GetGroup().GetGroupUtilityComponent();
				holdFire = utility && utility.GetCombatModeExternal() == EAIGroupCombatMode.HOLD_FIRE;
				for (int actor; actor < 5; actor++)
				{
					ChimeraCharacter member = ChimeraCharacter.Cast(slot.GetRosterMember(actor));
					if (member && member.GetDamageManager() && !member.GetDamageManager().IsDamageHandlingEnabled()) protectedMembers++;
				}
			}
			if (trip && trip.GetLease() && trip.GetLease().GetVehicle())
			{
				DamageManagerComponent damage = DamageManagerComponent.Cast(trip.GetLease().GetVehicle().FindComponent(DamageManagerComponent));
				vehicleProtected = damage && !damage.IsDamageHandlingEnabled();
			}
			string details = string.Format("sample_id=%1 stage=%2 phase=%3 group=%4 generation=%5 alive=%6 exact_active=%7 vehicle=%8 rpl=%9",
				m_iAICFMultiSample, item.m_iPhase, phase, group, item.m_iGeneration, alive, exact, item.m_VehicleId.ToString(), item.m_sRpl);
			details += string.Format(" operation_id=%1 lease_generation=%2 vehicle_generation=%3 protected_members=%4 vehicle_protected=%5 hold_fire=%6 route_complete=%7 position=%8",
				item.m_sOperation, item.m_iLeaseGeneration, item.m_iVehicleGeneration, protectedMembers, vehicleProtected, holdFire, item.m_bFootComplete, item.m_vLast);
			details += string.Format(" leg=%1 displacement_m=%2 moving_polls=%3", item.m_iLeg, item.m_fDisplacement, item.m_iMoving);
			int routeGeneration = -1;
			bool destinationWait, routeWaypoint;
			if (trip)
			{
				destinationWait = trip.IsPresetWaitingForDestination();
				routeGeneration = trip.GetMovementState().GetRouteGeneration();
				routeWaypoint = trip.GetMovementState().GetRouteWaypoint() || trip.GetMovementState().GetSupersededRouteWaypoint();
			}
			details += string.Format(" preset_destination_wait=%1 route_generation=%2 route_waypoint_present=%3", destinationWait, routeGeneration, routeWaypoint);
			AICF_MultiLog("SLOT", item.m_sFaction, item.m_iSlot, details);
			if (m_iAICFMultiSample % 5 == 0 && slot) AICF_GroupTrace.Snapshot(slot, item.m_sFaction, trip, 0, "VEHICLE_MULTI_READBACK");
		}
		if (!m_bAICFMultiMustered && mustered == m_iAICFMultiCohort * 2)
		{
			m_bAICFMultiMustered = true;
			AICF_MultiLog("MUSTERED", "BOTH", -1, string.Format("cohort=%1 groups=%2 soldiers=%3 paid_muster=1", m_iAICFMultiCohort, mustered, mustered * 5));
		}
		AICF_MultiLog("HEARTBEAT", "BOTH", -1, string.Format("sample_id=%1 cohort=%2 us_exact_active=%3 ussr_exact_active=%4 completed_groups=%5 failures=%6 mustered_groups=%7 sample_elapsed_ms=%8",
			m_iAICFMultiSample, m_iAICFMultiCohort, usExact, ussrExact, completed, m_iAICFMultiFailures, mustered, now - m_iAICFMultiStart));
		if (m_iAICFMultiCohort <= 6 && usExact >= m_iAICFMultiCohort && ussrExact >= m_iAICFMultiCohort)
		{
			if (m_iAICFMultiStableAt < 0) m_iAICFMultiStableAt = now;
			if (now - m_iAICFMultiStableAt >= 30000)
			{
				AICF_MultiLog("COHORT", "BOTH", -1, string.Format("sample_id=%1 cohort=%2 us_exact_active=%3 ussr_exact_active=%4 stable_ms=%5 required_ms=30000 stable_started_elapsed_ms=%6 sample_elapsed_ms=%7", m_iAICFMultiSample, m_iAICFMultiCohort, usExact, ussrExact, now - m_iAICFMultiStableAt, m_iAICFMultiStableAt - m_iAICFMultiStart, now - m_iAICFMultiStart));
				m_iAICFMultiCohort += 2;
				m_bAICFMultiMustered = false;
				m_iAICFMultiStableAt = -1;
			}
		}
		else m_iAICFMultiStableAt = -1;
		m_iAICFMultiSnapshotAt = -1;
	}

	protected void AICF_MultiAbort(string reason)
	{
		AICF_MultiFail(null, reason);
		m_bAICFMultiDone = true;
		AICF_MultiLog("ABORTED", "BOTH", -1, string.Format("reason=%1 duration_satisfied=0 started=%2 setup_elapsed_ms=%3", reason, m_bAICFMultiStarted, System.GetTickCount() - m_iAICFMultiSetupStarted));
		if (!m_bStopped) Stop(true);
		GetGame().RequestClose();
	}

	override protected void Update()
	{
		if (m_bAICFMultiEnabled && !m_bAICFMultiDone)
		{
			if (!AICF_MultiHierarchyProtection.ScopeAllowed(m_sAICFMultiMode, AICF_MultiHierarchyProtection.SelectedScope()))
			{
				AICF_MultiAbort("PROTECTION_SCOPE_INVALID");
				return;
			}
			AICF_MultiEnvironment();
			if (m_sAICFMultiMode == "CAPACITY")
				foreach (AICF_MultiSquadCase protectedItem : m_aAICFMultiCases)
				{
					AICF_GroupSlot protectedSlot = AICF_MultiSlot(protectedItem);
					AICF_TransportTrip protectedTrip;
					if (protectedSlot && m_VehicleCoordinator) protectedTrip = m_VehicleCoordinator.GetTrip(protectedSlot);
					AICF_MultiProtect(protectedSlot, protectedTrip, protectedItem);
				}
		}
		super.Update();
		if (!Replication.IsServer() || !m_bAICFMultiEnabled || m_bAICFMultiDone) return;
		int now = System.GetTickCount();
		if (m_bStopped || !m_Campaign)
		{
			AICF_MultiAbort("MATCH_STOPPED_BEFORE_DURATION");
			return;
		}
		if (!m_bAICFMultiStarted && now - m_iAICFMultiSetupStarted >= 180000)
		{
			AICF_MultiAbort("SETUP_TIMEOUT");
			return;
		}
		if (!m_Campaign.IsRunning())
		{
			if (m_bCampaignWasRunning || m_bAICFMultiStarted)
				AICF_MultiAbort("MATCH_STOPPED_BEFORE_DURATION");
			return;
		}
		if (!m_bAICFMultiStarted)
		{
			if (!AICF_MultiInitialize()) return;
			now = System.GetTickCount();
		}
		if (now - m_iAICFMultiStart >= m_iAICFMultiDuration)
		{
			int completed;
			foreach (AICF_MultiSquadCase doneItem : m_aAICFMultiCases) if (doneItem.m_bFootComplete) completed++;
			if (m_iAICFMultiCohort != 8 || completed != 12) AICF_MultiFail(null, "COHORT_OR_FUNCTIONAL_ROUTE_NOT_COMPLETE_AT_DURATION");
			m_bAICFMultiDone = true;
			AICF_MultiLog("DONE", "BOTH", -1, string.Format("requested_duration_ms=%1 duration_satisfied=1 failures=%2 cohort=%3 completed_groups=%4 us_granted=%5 ussr_granted=%6",
				m_iAICFMultiDuration, m_iAICFMultiFailures, m_iAICFMultiCohort, completed, m_EconomySystem.AICF_MultiGranted("US"), m_EconomySystem.AICF_MultiGranted("USSR")));
			Stop(true);
			GetGame().RequestClose();
			return;
		}
		foreach (AICF_MultiSquadCase item : m_aAICFMultiCases) AICF_MultiTick(item, now);
		if (now >= m_iAICFMultiNextFunding)
		{
			m_iAICFMultiNextFunding = now + 10000;
			AICF_MultiFunding();
		}
		if (now >= m_iAICFMultiNextSample)
		{
			m_iAICFMultiNextSample = now + 2000;
			AICF_MultiSample(now);
		}
	}
}

modded class AICF_VehicleAcquisitionFlow
{
	override AICF_TripOutcome Update(AICF_TransportTrip trip, AICF_StrategicAssignmentSnapshot currentAssignment,
		AICF_FactionFleet fleet, SCR_CampaignFaction faction, int nowMs)
	{
		bool observed = AICF_MultiAcquisitionProbe.Enabled();
		ref AICF_MultiAcquisitionFrame frame;
		AICF_MultiAcquisitionContext before;
		if (observed)
		{
			frame = AICF_MultiAcquisitionProbe.Begin();
			before = AICF_MultiAcquisitionProbe.Read(trip, currentAssignment);
		}
		AICF_TripOutcome actual = super.Update(trip, currentAssignment, fleet, faction, nowMs);
		if (observed) AICF_MultiAcquisitionProbe.Record(before, AICF_MultiAcquisitionProbe.Read(trip, currentAssignment), actual, nowMs, frame);
		return actual;
	}

}
