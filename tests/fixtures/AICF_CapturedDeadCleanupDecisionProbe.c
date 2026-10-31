// Только isolated diagnostics: actual return/stage + cached script values.
// Никаких повторных admission/IsCurrent и native actor/seat/world getters.
class AICF_CapturedDeadDecisionFrame
{
	string m_sKey;
	string m_sSignature;
}

class AICF_CapturedDeadCleanupDecisionProbe
{
	static const int MAX_FRAMES = 64;
	static const int MAX_RECORDS = 512;
	static const int MAX_DETAILS = 2048;
	static bool s_bInitialized;
	static bool s_bEnabled;
	static bool s_bRearmExactRequested;
	static bool s_bRearmExactDone;
	static string s_sControlledFaction;
	static int s_iRecords;
	static ref array<ref AICF_CapturedDeadDecisionFrame> s_aFrames = {};

	static void Initialize()
	{
		if (s_bInitialized) return;
		s_bInitialized = true;
		string enabled;
		s_bEnabled = System.GetCLIParam("aicfCapturedDeadCleanupDecisionProbe", enabled) && enabled == "1";
		string rearm;
		s_bRearmExactRequested = System.GetCLIParam("aicfCapturedDeadCleanupRearmExact", rearm) && rearm == "1";
		System.GetCLIParam("aicfVehicleSoakFaction", s_sControlledFaction);
	}

	static void Stop()
	{
		s_bEnabled = false;
		s_bInitialized = false;
		s_bRearmExactRequested = false;
		s_bRearmExactDone = false;
		s_sControlledFaction = "";
		s_iRecords = 0;
		s_aFrames.Clear();
	}

	static void RearmExactBeforeFactory(AICF_TransportTrip trip)
	{
		if (!s_bEnabled || !s_bRearmExactRequested || s_bRearmExactDone || !trip) return;
		if (s_sControlledFaction != "US" && s_sControlledFaction != "USSR") return;
		if (trip.GetSlotId() != 0 || trip.GetFactionKey() != s_sControlledFaction) return;
		if (AICF_QuotaExactCaptureProbe.s_Frame) return;
		s_bRearmExactDone = true;
		AICF_QuotaExactCaptureProbe.Stop();
		AICF_QuotaExactCaptureProbe.Start();
		Print(string.Format("[AICF][CAPTURED_DEAD_CLEANUP_EXACT_EPOCH] operation_id=%1 epoch=1 cause=BEFORE_FIRST_CONTROLLED_CLEANUP_FACTORY native_capture_pending=0 original_boot_overflow_preserved_in_logs=1 scope=DIAGNOSTIC_ONLY_NOT_CONTINUOUS",
			trip.GetOperationId()));
	}

	static bool Want(string key, string signature)
	{
		if (!s_bEnabled || s_iRecords >= MAX_RECORDS) return false;
		foreach (AICF_CapturedDeadDecisionFrame frame : s_aFrames)
		{
			if (frame.m_sKey != key) continue;
			if (frame.m_sSignature == signature) return false;
			frame.m_sSignature = signature;
			s_iRecords++;
			return true;
		}
		if (s_aFrames.Count() >= MAX_FRAMES) return false;
		AICF_CapturedDeadDecisionFrame created = new AICF_CapturedDeadDecisionFrame();
		created.m_sKey = key;
		created.m_sSignature = signature;
		s_aFrames.Insert(created);
		s_iRecords++;
		return true;
	}

	static void Write(string operation, string methodName, string stage, bool actual, string details)
	{
		if (!s_bEnabled) return;
		bool truncated = details.Length() > MAX_DETAILS;
		if (truncated) details = details.Substring(0, MAX_DETAILS);
		string key = operation + ":" + methodName + ":" + stage;
		string signature = string.Format("%1:%2:%3", actual, truncated, details);
		if (!Want(key, signature)) return;
		Print(string.Format("[AICF][CAPTURED_DEAD_CLEANUP_DECISION] operation_id=%1 method=%2 guard_stage=%3 actual=%4 record=%5 details_truncated=%6 scope=CACHED_AFTER_ORIGINAL_DECISION_NOT_ATOMIC no_extra_native_getters=1 %7",
			operation, methodName, stage, actual, s_iRecords, truncated, details));
	}

	static string TripDetails(AICF_TransportTrip trip)
	{
		if (!trip) return "trip_present=0";
		AICF_StrategicAssignmentSnapshot assignment = trip.GetAssignment();
		AICF_GroupSlot slot = trip.GetVehiclePresetSlot();
		string details = string.Format("trip_present=1 group_generation=%1 trip_generation=%2 terminal=%3 transition=%4 current_cause=%5 assignment_present=%6 slot_present=%7",
			trip.GetGroupGeneration(), trip.GetTripGeneration(), trip.IsTerminal(), trip.GetTransitionCount(), trip.GetCausationId(), assignment != null, slot != null);
		if (assignment) details += string.Format(" current_assignment_revision=%1 current_intent_revision=%2 current_base_revision=%3", assignment.GetAssignmentRevision(), assignment.GetStrategicIntentRevision(), assignment.GetBaseRevision());
		if (slot) details += string.Format(" slot_id=%1 slot_generation=%2 slot_assignment_revision=%3", slot.GetSlotId(), slot.GetSpawnGeneration(), slot.GetStrategicAssignmentRevision());
		return details;
	}
}

modded class AICF_TripRetirementIdentity
{
	override protected void AICF_ObserveTerminalAdmissionDecision(AICF_TransportTrip trip, string stage, bool actual)
	{
		if (!AICF_CapturedDeadCleanupDecisionProbe.s_bEnabled) return;
		AICF_StrategicAssignmentSnapshot current;
		if (trip) current = trip.GetAssignment();
		string details = string.Format("captured=%1 roster_trusted=%2 admission_members=%3 captured_world_present=%4 captured_group_present=%5 captured_group_rpl_present=%6 admission_assignment_present=%7 current_assignment_same=%8",
			m_bCaptured, m_bRosterTrusted, m_aMembers.Count(), m_AdmissionWorld != null, m_CapturedGroup != null, m_AdmissionGroupRpl != null, m_AdmissionAssignment != null, current == m_AdmissionAssignment);
		details += string.Format(" cached_assignment_revision=%1 cached_intent_revision=%2 cached_base_revision=%3 cached_group_generation=%4 cached_trip_generation=%5",
			m_iAdmissionAssignmentRevision, m_iAdmissionIntentRevision, m_iAdmissionBaseRevision, m_iGroupGeneration, m_iTripGeneration);
		details += " " + AICF_CapturedDeadCleanupDecisionProbe.TripDetails(trip);
		AICF_CapturedDeadCleanupDecisionProbe.Write(m_sOperation, "MatchesTerminalCleanupAdmission", stage, actual, details);
	}
}

modded class AICF_CapturedDeadCleanupIdentity
{
	override protected void AICF_ObserveCapturedCleanupDecision(AICF_VehicleCleanupJob job, string methodName, string stage, bool actual)
	{
		if (!AICF_CapturedDeadCleanupDecisionProbe.s_bEnabled) return;
		string details = string.Format("job_present=%1 job_same=%2 admission_present=%3 trip_present=%4 cached_assignment_present=%5 cached_slot_present=%6 cached_lease_present=%7 cached_world_present=%8",
			job != null, job == m_Job, m_Admission != null, m_Trip != null, m_Assignment != null, m_Slot != null, m_Lease != null, m_World != null);
		details += string.Format(" cached_assignment_revision=%1 cached_intent_revision=%2 cached_base_revision=%3 cached_transition=%4 cached_cause=%5", m_iAssignmentRevision, m_iIntentRevision, m_iBaseRevision, m_iTransition, m_sCausation);
		if (m_Trip) details += string.Format(" current_assignment_same=%1 current_slot_same=%2", m_Trip.GetAssignment() == m_Assignment, m_Trip.GetVehiclePresetSlot() == m_Slot);
		if (job) details += string.Format(" job_completed=%1 job_release_complete=%2 job_fail_closed=%3 job_fence_same=%4 job_lease_same=%5 job_fleet_same=%6 action_token=%7",
			job.m_bCompleted, job.m_bReleaseComplete, job.m_bFailClosed, job.m_Fence == m_OriginalFence, job.m_Lease == m_Lease, job.m_Fleet == m_Fleet, job.m_sActionToken);
		if (m_Lease) details += string.Format(" lease_generation=%1 vehicle_generation=%2 lease_state=%3 lease_cap_active=%4 lease_entity_cached=%5 lease_rpl_cached=%6",
			m_Lease.GetLeaseGeneration(), m_Lease.GetVehicleGeneration(), m_Lease.GetState(), m_Lease.IsCapActive(), m_Lease.GetEntityIdString(), m_Lease.GetRplId());
		details += " " + AICF_CapturedDeadCleanupDecisionProbe.TripDetails(m_Trip);
		AICF_CapturedDeadCleanupDecisionProbe.Write(m_sOperation, methodName, stage, actual, details);
	}
}

modded class AICF_TransportTrip
{
	override AICF_CapturedDeadCleanupIdentity CaptureCapturedDeadCleanupIdentity(AICF_VehicleCleanupJob job)
	{
		AICF_CapturedDeadCleanupDecisionProbe.RearmExactBeforeFactory(this);
		AICF_CapturedDeadCleanupIdentity actual = super.CaptureCapturedDeadCleanupIdentity(job);
		if (AICF_CapturedDeadCleanupDecisionProbe.s_bEnabled)
		{
			string details = string.Format("factory_return_present=%1 admission_present=%2 job_present=%3", actual != null, m_RetirementIdentity != null, job != null);
			details += " " + AICF_CapturedDeadCleanupDecisionProbe.TripDetails(this);
			AICF_CapturedDeadCleanupDecisionProbe.Write(GetOperationId(), "CaptureCapturedDeadCleanupIdentity", "FACTORY_RETURN", actual != null, details);
		}
		return actual;
	}
}

modded class AICF_MatchController
{
	override protected void Update()
	{
		AICF_CapturedDeadCleanupDecisionProbe.Initialize();
		super.Update();
	}

	override protected void Stop(bool cleanupEntities)
	{
		AICF_CapturedDeadCleanupDecisionProbe.Stop();
		super.Stop(cleanupEntities);
	}
}
