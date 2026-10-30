// Opt-in decision observer. No policy, native state, counters or snapshot lifetime changes.
class AICF_QuotaCensusDecisionFrame
{
	AICF_AIWorldQuotaService m_Service;
	SCR_AIWorld m_World;
	int m_iSerial;
	string m_sContext;
	string m_sBefore;
}

class AICF_QuotaCensusDecisionProbe
{
	static const int MAX_RECORDS = 96;
	static ref AICF_QuotaCensusDecisionFrame s_Frame;
	static ref array<string> s_Keys = {};
	static int s_iSerial;
	static bool s_bOverflow;
	static bool s_bScopeLoss;
	static AICF_AIWorldQuotaService s_ContextService;
	static int s_iContextAt;
	static string s_sContext;
	static bool s_bContextIncomplete;

	static bool Enabled()
	{
		string value;
		return System.GetCLIParam("aicfQuotaCensusDecisionProbe", value) && value == "1";
	}

	static AICF_QuotaCensusDecisionFrame Begin(AICF_AIWorldQuotaService service, SCR_AIWorld world, string context, string before)
	{
		AICF_QuotaCensusDecisionFrame frame = new AICF_QuotaCensusDecisionFrame();
		frame.m_Service = service;
		frame.m_World = world;
		frame.m_iSerial = ++s_iSerial;
		frame.m_sContext = context;
		frame.m_sBefore = before;
		s_Frame = frame;
		return frame;
	}

	static void End(AICF_QuotaCensusDecisionFrame frame, AICF_QuotaCensusDecisionFrame previous)
	{
		if (s_Frame == frame) s_Frame = previous;
		else ScopeLoss(frame);
	}

	static AICF_QuotaCensusDecisionFrame ForService(AICF_AIWorldQuotaService service)
	{
		if (s_Frame && s_Frame.m_Service != service) { ScopeLoss(s_Frame); return null; }
		return s_Frame;
	}

	static AICF_QuotaCensusDecisionFrame ForWorld(SCR_AIWorld world)
	{
		if (s_Frame && s_Frame.m_World != world) { ScopeLoss(s_Frame); return null; }
		return s_Frame;
	}

	static void ScopeLoss(AICF_QuotaCensusDecisionFrame frame)
	{
		if (s_bScopeLoss) return;
		s_bScopeLoss = true;
		Print("[AICF][QUOTA_CENSUS_DECISION] event=SCOPE_LOSS observation_only=1 successor_untouched=1");
	}

	static bool Want(AICF_QuotaCensusDecisionFrame frame, string stage, string identity)
	{
		if (!frame) return false;
		if (s_Frame != frame) { ScopeLoss(frame); return false; }
		string key = frame.m_sContext + "/" + stage + "/" + identity;
		if (s_Keys.Contains(key)) return false;
		if (s_Keys.Count() >= MAX_RECORDS)
		{
			if (!s_bOverflow)
			{
				s_bOverflow = true;
				Print("[AICF][QUOTA_CENSUS_DECISION] event=OVERFLOW record_limit=96 observations_incomplete=1 observation_only=1");
			}
			return false;
		}
		s_Keys.Insert(key);
		return true;
	}

	static void Write(AICF_QuotaCensusDecisionFrame frame, string stage, string identity, string details)
	{
		if (!frame || s_Frame != frame) { ScopeLoss(frame); return; }
		string prefix = string.Format("[AICF][QUOTA_CENSUS_DECISION] event=FAILED serial=%1 stage=%2 identity=%3 context=%4 ", frame.m_iSerial, stage, identity, frame.m_sContext);
		Print(prefix + frame.m_sBefore + " " + details + " actual_result=0 observation_only=1");
	}

	static string GroupId(AIGroup group)
	{
		EntityID id;
		if (group) id = group.GetID();
		return id.ToString();
	}

	static string RowKey(AICF_AIQuotaAgentRow row)
	{
		if (!row) return "NULL_ROW";
		return string.Format("%1/%2", row.m_EntityId, row.m_AgentId);
	}

	static string RowData(AICF_AIQuotaAgentRow row)
	{
		if (!row) return "input_row_present=0";
		return string.Format("input_row_present=1 entity_before=%1 agent_before=%2 parent_before=%3 counted_before=%4 active_before=%5 vehicle_before=%6", row.m_EntityId, row.m_AgentId, GroupId(row.m_Group), row.m_bCounted, row.m_bActivated, row.m_bVehicle);
	}

	static string BodyKey(AICF_AIQuotaBody body)
	{
		if (!body) return "NULL_BODY";
		return body.Key();
	}

	static string BodyData(AICF_AIQuotaBody body)
	{
		if (!body) return "input_body_present=0";
		EntityID ownerGroup;
		int generation = -1;
		int kind = -1;
		if (body.m_GroupOwner)
		{
			ownerGroup = body.m_GroupOwner.m_GroupId;
			generation = body.m_GroupOwner.m_iGeneration;
			kind = body.m_GroupOwner.m_Kind;
		}
		return string.Format("input_body_present=1 body_key_before=%1 bucket_before=%2 captured_owner_group=%3 owner_generation=%4 owner_kind=%5 vehicle_before=%6 released_before=%7 quarantined_before=%8", body.Key(), body.m_Bucket, ownerGroup, generation, kind, body.m_bVehicle, body.m_bReleased, body.m_bQuarantined);
	}

	static string NativeAfter(SCR_AIWorld world)
	{
		int active = -1;
		int intended = -1;
		if (world) active = world.GetCurrentNumOfActiveAIs();
		if (world) intended = world.GetActiveGroupIntendedCount();
		return string.Format("native_active_after=%1 native_intended_after=%2 native_scope=AFTER_DECISION_NOT_CAPTURED", active, intended);
	}

	static void Stop()
	{
		s_Frame = null;
		s_ContextService = null;
		s_Keys.Clear();
		s_sContext = string.Empty;
		s_iContextAt = 0;
		s_iSerial = 0;
		s_bOverflow = false;
		s_bScopeLoss = false;
		s_bContextIncomplete = false;
	}
}

modded class AICF_AIQuotaCensus
{
	override bool Capture(SCR_AIWorld world)
	{
		AICF_QuotaCensusDecisionFrame frame = AICF_QuotaCensusDecisionProbe.ForWorld(world);
		bool actual = super.Capture(world);
		if (!actual && frame && AICF_QuotaCensusDecisionProbe.Want(frame, "CAPTURE", m_sReason))
		{
			int flags;
			int countedNull;
			int nullRows;
			int examined;
			int duplicates;
			EntityID firstDuplicate;
			array<EntityID> entities = {};
			foreach (AICF_AIQuotaAgentRow row : m_aRows)
			{
				if (examined >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) break;
				examined++;
				if (!row) { nullRows++; continue; }
				if (row.m_bCounted && row.m_bActivated) flags++;
				if (row.m_bCounted && row.m_EntityId == EntityID.INVALID) countedNull++;
				if (row.m_EntityId != EntityID.INVALID)
				{
					if (entities.Contains(row.m_EntityId)) { duplicates++; firstDuplicate = row.m_EntityId; }
					else entities.Insert(row.m_EntityId);
				}
			}
			string details = string.Format("capture_reason=%1 stored_rows=%2 examined_rows=%3 cached_counted_active=%4 cached_counted_no_entity=%5 null_rows=%6", m_sReason, m_aRows.Count(), examined, flags, countedNull, nullRows);
			details += string.Format(" cached_duplicate_entities=%1 first_duplicate_entity=%2 cached_scan_complete=%3 original_local_invalid_count=UNOBSERVED", duplicates, firstDuplicate, examined == m_aRows.Count());
			AICF_QuotaCensusDecisionProbe.Write(frame, "CAPTURE", m_sReason, details + " " + AICF_QuotaCensusDecisionProbe.NativeAfter(world));
		}
		return actual;
	}

	override bool IsCurrent()
	{
		AICF_QuotaCensusDecisionFrame frame = AICF_QuotaCensusDecisionProbe.ForWorld(m_World);
		string before;
		if (frame) before = string.Format("snapshot_complete_before=%1 stored_active_before=%2 stored_intended_before=%3", m_bComplete, m_iActive, m_iIntended);
		bool actual = super.IsCurrent();
		if (!actual && AICF_QuotaCensusDecisionProbe.Want(frame, "SNAPSHOT_CURRENT", m_sReason))
			AICF_QuotaCensusDecisionProbe.Write(frame, "SNAPSHOT_CURRENT", m_sReason, before + " " + AICF_QuotaCensusDecisionProbe.NativeAfter(m_World));
		return actual;
	}
}

modded class AICF_AIWorldQuotaService
{
	protected string AICF_DecisionContext()
	{
		int now = System.GetTickCount();
		if (AICF_QuotaCensusDecisionProbe.s_ContextService != this || now - AICF_QuotaCensusDecisionProbe.s_iContextAt >= 1000)
		{
			AICF_QuotaCensusDecisionProbe.s_ContextService = this;
			AICF_QuotaCensusDecisionProbe.s_iContextAt = now;
			string cars;
			int found;
			int examined;
			AICF_QuotaCensusDecisionProbe.s_bContextIncomplete = false;
			foreach (AICF_AIQuotaBody body : m_aBodies)
			{
				if (++examined > AICF_AIQuotaMath.MAX_TOTAL_UNITS) { AICF_QuotaCensusDecisionProbe.s_bContextIncomplete = true; break; }
				if (!body || !body.m_bVehicle || body.m_bReleased) continue;
				cars += body.Key() + ",";
				if (++found >= 4) { AICF_QuotaCensusDecisionProbe.s_bContextIncomplete = true; break; }
			}
			AICF_QuotaCensusDecisionProbe.s_sContext = string.Format("epoch%1/cars%2", m_iEpoch, cars);
		}
		return AICF_QuotaCensusDecisionProbe.s_sContext;
	}

	override bool RefreshCensus()
	{
		ref AICF_QuotaCensusDecisionFrame previous = AICF_QuotaCensusDecisionProbe.s_Frame;
		ref AICF_QuotaCensusDecisionFrame frame;
		if (AICF_QuotaCensusDecisionProbe.Enabled())
		{
			string before = string.Format("epoch_before=%1 native_birth_busy_before=%2 census_busy_before=%3 quarantined_before=%4 bodies_before=%5", m_iEpoch, m_bNativeBirthBusy, m_bQuotaCensusBusy, m_bQuarantined, m_aBodies.Count());
			before += string.Format(" policy_active_before=%1 limits_present_before=%2 ledger_present_before=%3 world_state_present_before=%4", m_bPolicyActive, m_Limits != null, m_Ledger != null, m_WorldState != null);
			string context = AICF_DecisionContext();
			before += string.Format(" context_cached_age_ms=%1 context_observation_limited=%2", System.GetTickCount() - AICF_QuotaCensusDecisionProbe.s_iContextAt, AICF_QuotaCensusDecisionProbe.s_bContextIncomplete);
			frame = AICF_QuotaCensusDecisionProbe.Begin(this, m_World, context, before);
		}
		bool actual = super.RefreshCensus();
		if (!actual && AICF_QuotaCensusDecisionProbe.Want(frame, "REFRESH", m_sLastReason))
		{
			string after = string.Format("stored_reason_after=%1 native_birth_busy_after=%2 census_busy_after=%3", m_sLastReason, m_bNativeBirthBusy, m_bQuotaCensusBusy);
			AICF_QuotaCensusDecisionProbe.Write(frame, "REFRESH", m_sLastReason, after + " " + AICF_QuotaCensusDecisionProbe.NativeAfter(m_World));
		}
		if (frame) AICF_QuotaCensusDecisionProbe.End(frame, previous);
		return actual;
	}

	override protected bool RefreshCurrentCensus()
	{
		AICF_QuotaCensusDecisionFrame frame = AICF_QuotaCensusDecisionProbe.ForService(this);
		bool actual = super.RefreshCurrentCensus();
		if (!actual && AICF_QuotaCensusDecisionProbe.Want(frame, "REFRESH_CURRENT", m_sLastReason))
			AICF_QuotaCensusDecisionProbe.Write(frame, "REFRESH_CURRENT", m_sLastReason, "stored_reason_after=" + m_sLastReason);
		return actual;
	}

	override protected bool BuildCounts(AICF_AIQuotaCensus snapshot, array<int> counts)
	{
		AICF_QuotaCensusDecisionFrame frame = AICF_QuotaCensusDecisionProbe.ForService(this);
		bool actual = super.BuildCounts(snapshot, counts);
		if (!actual && AICF_QuotaCensusDecisionProbe.Want(frame, "BUILD_COUNTS", "COUNTS"))
			AICF_QuotaCensusDecisionProbe.Write(frame, "BUILD_COUNTS", "COUNTS", string.Format("body_slots_after=%1", m_aBodies.Count()));
		return actual;
	}

	override protected bool ReadQuotaCharacterCustody(AICF_AIQuotaAgentRow row, out bool hold)
	{
		AICF_QuotaCensusDecisionFrame frame = AICF_QuotaCensusDecisionProbe.ForService(this);
		string key;
		string before;
		if (frame) { key = AICF_QuotaCensusDecisionProbe.RowKey(row); before = AICF_QuotaCensusDecisionProbe.RowData(row); }
		bool actual = super.ReadQuotaCharacterCustody(row, hold);
		if (!actual && AICF_QuotaCensusDecisionProbe.Want(frame, "CHARACTER_CUSTODY", key))
			AICF_QuotaCensusDecisionProbe.Write(frame, "CHARACTER_CUSTODY", key, before + " hold_return=" + hold.ToString());
		return actual;
	}

	override protected bool ClassifyRow(AICF_AIQuotaAgentRow row, out AICF_EAIQuotaBucket bucket)
	{
		AICF_QuotaCensusDecisionFrame frame = AICF_QuotaCensusDecisionProbe.ForService(this);
		string key;
		string before;
		if (frame) { key = AICF_QuotaCensusDecisionProbe.RowKey(row); before = AICF_QuotaCensusDecisionProbe.RowData(row); }
		bool actual = super.ClassifyRow(row, bucket);
		if (!actual && AICF_QuotaCensusDecisionProbe.Want(frame, "CLASSIFY_ROW", key))
			AICF_QuotaCensusDecisionProbe.Write(frame, "CLASSIFY_ROW", key, before + " bucket_return=" + bucket.ToString());
		return actual;
	}

	override protected bool ClassifyCapturedActor(AICF_AIQuotaBody body, SCR_AIGroup parent, out AICF_EAIQuotaBucket bucket)
	{
		AICF_QuotaCensusDecisionFrame frame = AICF_QuotaCensusDecisionProbe.ForService(this);
		string key;
		string before;
		if (frame) { key = AICF_QuotaCensusDecisionProbe.BodyKey(body); before = AICF_QuotaCensusDecisionProbe.BodyData(body) + " input_parent_before=" + AICF_QuotaCensusDecisionProbe.GroupId(parent); }
		bool actual = super.ClassifyCapturedActor(body, parent, bucket);
		if (!actual && AICF_QuotaCensusDecisionProbe.Want(frame, "CLASSIFY_CAPTURED_ACTOR", key))
			AICF_QuotaCensusDecisionProbe.Write(frame, "CLASSIFY_CAPTURED_ACTOR", key, before + " bucket_return=" + bucket.ToString());
		return actual;
	}

	override protected bool ClassifyQuotaVehicleRow(AICF_AIQuotaAgentRow row, out AICF_EAIQuotaBucket bucket, out bool charge)
	{
		AICF_QuotaCensusDecisionFrame frame = AICF_QuotaCensusDecisionProbe.ForService(this);
		string key;
		string before;
		if (frame)
		{
			key = AICF_QuotaCensusDecisionProbe.RowKey(row);
			before = AICF_QuotaCensusDecisionProbe.RowData(row);
			int examined;
			bool matched;
			foreach (AICF_AIQuotaBody body : m_aBodies)
			{
				if (examined >= AICF_AIQuotaMath.MAX_TOTAL_UNITS) break;
				examined++;
				if (!row || !body || body.m_Id != row.m_EntityId) continue;
				before += " " + AICF_QuotaCensusDecisionProbe.BodyData(body);
				matched = true;
				break;
			}
			before += string.Format(" cached_body_match_before=%1 body_slots_examined_before=%2", matched, examined);
		}
		bool actual = super.ClassifyQuotaVehicleRow(row, bucket, charge);
		if (!actual && AICF_QuotaCensusDecisionProbe.Want(frame, "CLASSIFY_VEHICLE_ROW", key))
			AICF_QuotaCensusDecisionProbe.Write(frame, "CLASSIFY_VEHICLE_ROW", key, before + string.Format(" bucket_return=%1 charge_return=%2", bucket, charge));
		return actual;
	}

	override protected bool AddQuotaVehicleHolds(AICF_AIQuotaCensus snapshot, array<int> counts, array<IEntity> counted)
	{
		AICF_QuotaCensusDecisionFrame frame = AICF_QuotaCensusDecisionProbe.ForService(this);
		bool actual = super.AddQuotaVehicleHolds(snapshot, counts, counted);
		if (!actual && AICF_QuotaCensusDecisionProbe.Want(frame, "VEHICLE_HOLDS", "HOLDS"))
			AICF_QuotaCensusDecisionProbe.Write(frame, "VEHICLE_HOLDS", "HOLDS", string.Format("body_slots_after=%1", m_aBodies.Count()));
		return actual;
	}
}

modded class ArmaReforgerScripted
{
	override void OnGameEnd()
	{
		AICF_QuotaCensusDecisionProbe.Stop();
		super.OnGameEnd();
	}
}
