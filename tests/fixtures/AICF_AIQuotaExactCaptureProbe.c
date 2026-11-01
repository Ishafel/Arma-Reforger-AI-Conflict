// Opt-in diagnostic. Original predicate inputs only; no extra native readback.
class AICF_QuotaExactCaptureFrame
{
	AICF_AIQuotaCensus m_Snapshot;
	int m_iSerial;
	bool m_bSuperseded;
	bool m_bPredicateSeen;
	bool m_bActual;
	int m_iReturned;
	int m_iExamined;
	int m_iInvalid;
	int m_iCountedActive;
	int m_iBeforeActive;
	int m_iAfterActive;
	int m_iBeforeIntended;
	int m_iAfterIntended;
	int m_iInitialChecks;
	int m_iInitialFalse;
	int m_iRepeatedChecks;
	int m_iRepeatedFalse;
	int m_iQ11;
	int m_iQ10;
	int m_iQ01;
	int m_iQ00;
	int m_iQ10Samples;
	int m_iInvalidSamples;
	string m_sQ10;
	string m_sInvalid;
	bool m_bSnapshotSeen;
	int m_iSnapshotRows;
	int m_iSnapshotNull;
	int m_iSnapshotQ10;
	int m_iSnapshotQ10Samples;
	bool m_bSnapshotTruncated;
	string m_sSnapshotQ10;
}

class AICF_QuotaExactCaptureProbe
{
	static const int MAX_RECORDS = 384;
	static const int MAX_SAMPLES = 8;
	static ref AICF_QuotaExactCaptureFrame s_Frame;
	static ref array<string> s_Keys = {};
	static int s_iSerial;
	static int s_iPublished;
	static bool s_bOverflow;
	static bool s_bScopeLoss;
	static bool s_bStopped;

	static void Start()
	{
		if (!s_bStopped) return;
		s_bStopped = false;
	}

	static bool Enabled()
	{
		if (s_bStopped) return false;
		string value;
		return System.GetCLIParam("aicfQuotaCensusExactCaptureProbe", value) && value == "1";
	}

	static AICF_QuotaExactCaptureFrame Begin(AICF_AIQuotaCensus snapshot)
	{
		if (s_bStopped) return null;
		AICF_QuotaExactCaptureFrame frame = new AICF_QuotaExactCaptureFrame();
		frame.m_Snapshot = snapshot;
		frame.m_iSerial = ++s_iSerial;
		if (s_Frame)
		{
			s_Frame.m_bSuperseded = true;
			s_bScopeLoss = true;
		}
		s_Frame = frame;
		return frame;
	}

	static bool Owns(AICF_QuotaExactCaptureFrame frame)
	{
		return !s_bStopped && frame && s_Frame == frame && !frame.m_bSuperseded;
	}

	// Parameters are cached row fields from BEFORE the one actual IsCurrent.
	// This is a dynamic Capture-scope observation, not a second invalid counter.
	static void Row(AICF_QuotaExactCaptureFrame frame, EntityID agentId, EntityID entityId, bool counted, bool active, bool actual)
	{
		if (!Owns(frame)) return;
		if (frame.m_bPredicateSeen)
		{
			frame.m_iRepeatedChecks++;
			if (!actual) frame.m_iRepeatedFalse++;
			return;
		}
		frame.m_iInitialChecks++;
		if (!actual) frame.m_iInitialFalse++;
		if (counted && active) frame.m_iQ11++;
		if (counted && !active) frame.m_iQ10++;
		if (!counted && active) frame.m_iQ01++;
		if (!counted && !active) frame.m_iQ00++;
		if (counted && !active && frame.m_iQ10Samples < MAX_SAMPLES)
		{
			frame.m_iQ10Samples++;
			frame.m_sQ10 += string.Format("%1/%2/counted%3/active%4/row_current%5;", agentId, entityId, counted, active, actual);
		}
		if (!actual && frame.m_iInvalidSamples < MAX_SAMPLES)
		{
			frame.m_iInvalidSamples++;
			frame.m_sInvalid += string.Format("%1/%2/counted%3/active%4/row_current%5;", agentId, entityId, counted, active, actual);
		}
	}

	static void Publish(AICF_QuotaExactCaptureFrame frame, bool result, string reason)
	{
		if (!Owns(frame)) return;
		string data = string.Format("predicate_seen=%1 predicate_actual=%2 returned=%3 examined=%4 invalid=%5 counted_active=%6 before_active=%7 after_active=%8", frame.m_bPredicateSeen, frame.m_bActual, frame.m_iReturned, frame.m_iExamined, frame.m_iInvalid, frame.m_iCountedActive, frame.m_iBeforeActive, frame.m_iAfterActive);
		data += string.Format(" before_intended=%1 after_intended=%2 capture_result=%3 capture_reason=%4 initial_row_checks=%5 initial_row_false=%6 repeated_row_checks=%7 repeated_row_false=%8", frame.m_iBeforeIntended, frame.m_iAfterIntended, result, reason, frame.m_iInitialChecks, frame.m_iInitialFalse, frame.m_iRepeatedChecks, frame.m_iRepeatedFalse);
		data += string.Format(" q11=%1 q10=%2 q01=%3 q00=%4 q10_samples=[%5] invalid_samples=[%6] q10_samples_truncated=%7 invalid_samples_truncated=%8", frame.m_iQ11, frame.m_iQ10, frame.m_iQ01, frame.m_iQ00, frame.m_sQ10, frame.m_sInvalid, frame.m_iQ10 > MAX_SAMPLES, frame.m_iInitialFalse > MAX_SAMPLES);
		data += string.Format(" snapshot_scope=CAPTURE_SNAPSHOT_CACHED_AFTER_RETURN snapshot_seen=%1 snapshot_rows_scanned=%2 snapshot_null_rows=%3 snapshot_q10=%4 snapshot_q10_samples=[%5] snapshot_rows_truncated=%6 snapshot_samples_truncated=%7 snapshot_row_limit=%8", frame.m_bSnapshotSeen, frame.m_iSnapshotRows, frame.m_iSnapshotNull, frame.m_iSnapshotQ10, frame.m_sSnapshotQ10, frame.m_bSnapshotTruncated, frame.m_iSnapshotQ10 > MAX_SAMPLES, AICF_AIQuotaMath.MAX_TOTAL_UNITS);
		if (s_Keys.Contains(data)) return;
		if (s_Keys.Count() >= MAX_RECORDS)
		{
			if (!s_bOverflow)
			{
				s_bOverflow = true;
				Print("[AICF][QUOTA_CENSUS_EXACT] event=OVERFLOW observations_incomplete=1 record_limit=384");
			}
			return;
		}
		s_Keys.Insert(data);
		s_iPublished++;
		string header = string.Format("[AICF][QUOTA_CENSUS_EXACT] event=CAPTURE serial=%1 observations_incomplete=%2 original_inputs=1 row_scope=DYNAMIC_CAPTURE_CALLS cached_identity=AGENT_ENTITY_IDS_ONLY life_lod_rpl=NOT_OBSERVED predicate_unseen_values=UNOBSERVED sample_limit=8 ", frame.m_iSerial, s_bScopeLoss || s_bOverflow);
		Print(header + data);
	}

	static void End(AICF_QuotaExactCaptureFrame frame)
	{
		if (Owns(frame)) s_Frame = null;
	}

	static void Stop()
	{
		if (s_bStopped) return;
		s_bStopped = true;
		int records = s_iPublished;
		bool pending = s_Frame != null;
		bool incomplete = pending || s_bScopeLoss || s_bOverflow;
		if (s_Frame)
		{
			s_Frame.m_bSuperseded = true;
			if (s_Frame.m_Snapshot) s_Frame.m_Snapshot.AICF_ExactForgetFrame(s_Frame);
		}
		s_Frame = null;
		s_Keys.Clear();
		s_iSerial = 0;
		s_iPublished = 0;
		s_bOverflow = false;
		s_bScopeLoss = false;
		if (records > 0 || incomplete)
			Print(string.Format("[AICF][QUOTA_CENSUS_EXACT] event=STOP published=%1 pending_capture=%2 observations_incomplete=%3", records, pending, incomplete));
	}
}

modded class AICF_AIQuotaCensus
{
	protected ref AICF_QuotaExactCaptureFrame m_AICFExactFrame;

	BaseWorld AICF_ExactRawMainWorld() { return m_MainWorld; }
	bool AICF_ExactOwnsFrame(AICF_QuotaExactCaptureFrame frame) { return m_AICFExactFrame == frame; }
	void AICF_ExactForgetFrame(AICF_QuotaExactCaptureFrame frame)
	{
		if (m_AICFExactFrame == frame) m_AICFExactFrame = null;
	}

	// Only this snapshot's stored script rows, after the actual Capture returned.
	// No IsCurrent re-evaluation, native getter or inference from dynamic calls.
	void AICF_ExactSampleSnapshot(AICF_QuotaExactCaptureFrame frame)
	{
		if (!AICF_QuotaExactCaptureProbe.Owns(frame)) return;
		if (!AICF_ExactOwnsFrame(frame) || frame.m_Snapshot != this) return;
		int scanned;
		int nullRows;
		int q10;
		int samples;
		string ids;
		int rowCount = m_aRows.Count();
		for (int index; index < rowCount && index < AICF_AIQuotaMath.MAX_TOTAL_UNITS; index++)
		{
			scanned++;
			AICF_AIQuotaAgentRow row = m_aRows[index];
			if (!row) { nullRows++; continue; }
			if (!row.m_bCounted || row.m_bActivated) continue;
			q10++;
			if (samples >= AICF_QuotaExactCaptureProbe.MAX_SAMPLES) continue;
			samples++;
			ids += string.Format("%1/%2/counted%3/active%4;", row.m_AgentId, row.m_EntityId, row.m_bCounted, row.m_bActivated);
		}
		if (!AICF_QuotaExactCaptureProbe.Owns(frame)) return;
		if (!AICF_ExactOwnsFrame(frame) || frame.m_Snapshot != this) return;
		frame.m_bSnapshotSeen = true;
		frame.m_iSnapshotRows = scanned;
		frame.m_iSnapshotNull = nullRows;
		frame.m_iSnapshotQ10 = q10;
		frame.m_iSnapshotQ10Samples = samples;
		frame.m_bSnapshotTruncated = rowCount > AICF_AIQuotaMath.MAX_TOTAL_UNITS;
		frame.m_sSnapshotQ10 = ids;
	}

	override bool Capture(SCR_AIWorld world)
	{
		if (!AICF_QuotaExactCaptureProbe.Enabled()) return super.Capture(world);
		AICF_QuotaExactCaptureFrame frame = AICF_QuotaExactCaptureProbe.Begin(this);
		m_AICFExactFrame = frame;
		bool result = super.Capture(world);
		AICF_ExactSampleSnapshot(frame);
		if (AICF_QuotaExactCaptureProbe.Owns(frame))
			AICF_QuotaExactCaptureProbe.Publish(frame, result, m_sReason);
		AICF_QuotaExactCaptureProbe.End(frame);
		if (m_AICFExactFrame == frame) m_AICFExactFrame = null;
		return result;
	}

	override protected void AICF_ObserveCaptureDecision(bool actual, int returned, int examined, int invalid,
		int countedActive, int beforeActive, int afterActive, int beforeIntended, int afterIntended)
	{
		super.AICF_ObserveCaptureDecision(actual, returned, examined, invalid, countedActive, beforeActive, afterActive, beforeIntended, afterIntended);
		AICF_QuotaExactCaptureFrame frame = m_AICFExactFrame;
		if (!AICF_QuotaExactCaptureProbe.Owns(frame) || frame.m_Snapshot != this) return;
		frame.m_bPredicateSeen = true;
		frame.m_bActual = actual;
		frame.m_iReturned = returned;
		frame.m_iExamined = examined;
		frame.m_iInvalid = invalid;
		frame.m_iCountedActive = countedActive;
		frame.m_iBeforeActive = beforeActive;
		frame.m_iAfterActive = afterActive;
		frame.m_iBeforeIntended = beforeIntended;
		frame.m_iAfterIntended = afterIntended;
	}
}

modded class AICF_AIQuotaAgentRow
{
	override bool IsCurrent(BaseWorld world)
	{
		AICF_QuotaExactCaptureFrame frame = AICF_QuotaExactCaptureProbe.s_Frame;
		bool scoped = AICF_QuotaExactCaptureProbe.Owns(frame) && frame.m_Snapshot && frame.m_Snapshot.AICF_ExactOwnsFrame(frame) && frame.m_Snapshot.AICF_ExactRawMainWorld() == world;
		EntityID agentId = m_AgentId;
		EntityID entityId = m_EntityId;
		bool counted = m_bCounted;
		bool active = m_bActivated;
		bool result = super.IsCurrent(world);
		if (scoped) AICF_QuotaExactCaptureProbe.Row(frame, agentId, entityId, counted, active, result);
		return result;
	}
}

modded class ArmaReforgerScripted
{
	override bool OnGameStart()
	{
		AICF_QuotaExactCaptureProbe.Start();
		return super.OnGameStart();
	}

	override void OnGameEnd()
	{
		AICF_QuotaExactCaptureProbe.Stop();
		super.OnGameEnd();
	}
}
