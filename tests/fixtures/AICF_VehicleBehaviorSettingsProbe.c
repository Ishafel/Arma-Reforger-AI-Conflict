// Только isolated stage. Run выполняет реальный JSON/библиотечный код,
// не создавая бойцов, машину или transport intent.
class AICF_VehicleBehaviorSettingsProbe
{
	protected int m_iChecks;
	protected int m_iFailures;

	protected void Check(string name, bool passed)
	{
		m_iChecks++;
		if (!passed) m_iFailures++;
		AICF_Stage4Diagnostics.Info("VEHICLE_BEHAVIOR_SETTINGS_CASE", string.Format("case=%1 passed=%2", name, passed));
	}

	protected string Data(int schema, int behavior = 0, int distance = 300, bool includeBehavior = true, bool includeDistance = true)
	{
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("schemaVersion", schema);
		json.WriteValue("profile", "behavior-settings-probe");
		json.WriteValue("faction", "USSR");
		json.WriteValue("name", "behavior-settings");
		json.WriteValue("prefab", "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et");
		array<string> seats = {"0:0:0"};
		array<int> members = {0};
		json.WriteValue("seats", seats);
		json.WriteValue("members", members);
		if (includeBehavior) json.WriteValue("behavior", behavior);
		if (includeDistance) json.WriteValue("returnDistance", distance);
		return json.SaveToString();
	}

	int Run()
	{
		m_iChecks = 0;
		m_iFailures = 0;
		AICF_VehiclePreset legacy = AICF_VehiclePreset.Decode(Data(1, 0, 300, false, false));
		Check("LEGACY_TRANSPORT300", legacy && legacy.m_iBehavior == AICF_EVehicleBehavior.TRANSPORT && legacy.m_iReturnDistance == 300);
		AICF_VehiclePreset combat = AICF_VehiclePreset.Decode(Data(2, AICF_EVehicleBehavior.COMBAT, 437));
		AICF_VehiclePreset restored;
		if (combat)
		{
			string encoded = combat.Encode();
			restored = AICF_VehiclePreset.Decode(encoded);
			string trace = encoded;
			if (trace.Length() > 1024) trace = trace.Substring(0, 1024);
			AICF_Stage4Diagnostics.Info("VEHICLE_BEHAVIOR_SETTINGS_ROUNDTRIP", string.Format(
				"prefab=%1 encoded_bytes=%2 decoded=%3 encoded=[%4]", combat.m_sPrefab, encoded.Length(), restored != null, trace));
		}
		Check("SCHEMA2_COMBAT_CUSTOM_DISTANCE", restored && restored.SameAssignment(combat) && restored.m_iReturnDistance == 437 && restored.m_iBehavior == AICF_EVehicleBehavior.COMBAT);
		Check("SCHEMA2_MISSING_BEHAVIOR_REJECTED", !AICF_VehiclePreset.Decode(Data(2, 0, 300, false)));
		Check("SCHEMA2_MISSING_DISTANCE_REJECTED", !AICF_VehiclePreset.Decode(Data(2, 0, 300, true, false)));
		Check("FUTURE_SCHEMA_REJECTED", !AICF_VehiclePreset.Decode(Data(3)));
		Check("NEGATIVE_BEHAVIOR_REJECTED", !AICF_VehiclePreset.Decode(Data(2, -1)));
		Check("UNKNOWN_BEHAVIOR_REJECTED", !AICF_VehiclePreset.Decode(Data(2, 2)));
		Check("DISTANCE49_REJECTED", !AICF_VehiclePreset.Decode(Data(2, 0, 49)));
		Check("DISTANCE1001_REJECTED", !AICF_VehiclePreset.Decode(Data(2, 0, 1001)));
		Check("DISTANCE50_ACCEPTED", AICF_VehiclePreset.Decode(Data(2, 0, 50)) != null);
		Check("DISTANCE1000_ACCEPTED", AICF_VehiclePreset.Decode(Data(2, 1, 1000)) != null);
		AICF_VehiclePreset transport = AICF_VehiclePreset.Decode(Data(2, 0, 437));
		AICF_VehiclePreset farther = AICF_VehiclePreset.Decode(Data(2, 1, 438));
		Check("MODE_CHANGE_IS_NEW_ASSIGNMENT", combat && transport && !combat.SameAssignment(transport));
		Check("DISTANCE_CHANGE_IS_NEW_ASSIGNMENT", combat && farther && !combat.SameAssignment(farther));
		AICF_VehiclePreset renamed;
		if (combat) renamed = AICF_VehiclePreset.Decode(combat.Encode());
		if (renamed) renamed.m_sName = "behavior-renamed";
		Check("NAME_ONLY_ASSIGNMENT_IDEMPOTENT", combat && renamed && combat.SameAssignment(renamed));
		string reason;
		if (restored) restored.m_iReturnDistance = 1001;
		Check("MUTATED_OBJECT_VALIDATION_REJECTED", restored && !restored.ValidateShape(reason) && reason == "INVALID_VEHICLE_BEHAVIOR");
		if (Replication.IsServer() && combat)
		{
			string directory = "$profile:AICF_BehaviorSettings";
			AICF_VehiclePresetStore store = new AICF_VehiclePresetStore(directory);
			Check("LIBRARY_SAVE_COMBAT", store.Save(combat, reason));
			int revision = store.Revision();
			Check("LIBRARY_REPEAT_SAVE_IDEMPOTENT", store.Save(combat, reason) && store.Revision() == revision);
			AICF_VehiclePresetStore reread = new AICF_VehiclePresetStore(directory);
			AICF_VehiclePreset saved = reread.Get(0);
			Check("LIBRARY_FRESH_STORE_READBACK", saved && saved.SameAssignment(combat));
			Check("LIBRARY_UPDATE_TRANSPORT", transport && reread.Save(transport, reason));
			AICF_VehiclePresetStore updated = new AICF_VehiclePresetStore(directory);
			AICF_VehiclePreset changed = updated.Get(0);
			Check("LIBRARY_UPDATE_PERSISTS_MODE", changed && changed.SameAssignment(transport) && !changed.SameAssignment(combat));
			Check("LIBRARY_INVALID_UPDATE_REJECTED", restored && !updated.Save(restored, reason) && reason == "INVALID_VEHICLE_BEHAVIOR");
			Check("LIBRARY_DELETE", updated.Delete(0, reason));
			AICF_VehiclePresetStore deleted = new AICF_VehiclePresetStore(directory);
			Check("LIBRARY_TOMBSTONE_SURVIVES_RELOAD", !deleted.Get(0));
		}
		else Check("SERVER_LIBRARY_CONTEXT", false);
		AICF_Stage4Diagnostics.Info("VEHICLE_BEHAVIOR_SETTINGS_FINISHED", string.Format("checks=%1 failures=%2", m_iChecks, m_iFailures));
		return m_iFailures;
	}
}
