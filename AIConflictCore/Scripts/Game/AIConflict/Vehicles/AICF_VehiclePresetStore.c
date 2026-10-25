class AICF_VehiclePresetFile
{
	int m_iVersion;
	bool m_bDeleted;
	string m_sData;

	string Encode()
	{
		JsonSaveContext json = new JsonSaveContext();
		Write(json);
		return json.SaveToString();
	}

	void Write(JsonSaveContext json)
	{
		json.WriteValue("schemaVersion", 1);
		json.WriteValue("version", m_iVersion);
		json.WriteValue("deleted", m_bDeleted);
		json.WriteValue("data", m_sData);
	}

	static AICF_VehiclePresetFile Read(string path)
	{
		if (!FileIO.FileExists(path)) return null;
		JsonLoadContext json = new JsonLoadContext();
		AICF_VehiclePresetFile file = new AICF_VehiclePresetFile();
		int schema;
		if (!json.LoadFromFile(path) || !json.ReadValue("schemaVersion", schema) || schema != 1 ||
			!json.ReadValue("version", file.m_iVersion) || file.m_iVersion < 1 ||
			!json.ReadValue("deleted", file.m_bDeleted) || !json.ReadValue("data", file.m_sData)) return null;
		if (file.m_bDeleted && !file.m_sData.IsEmpty()) return null;
		if (!file.m_bDeleted && !AICF_VehiclePreset.Decode(file.m_sData)) return null;
		return file;
	}
}

// Отдельный namespace в существующей постоянной библиотеке. Два поколения
// и tombstone сохраняют проверенную запись при прерывании записи/удаления.
class AICF_VehiclePresetStore
{
	static const int CAPACITY = 128;
	protected string m_sDirectory;
	protected ref array<ref AICF_VehiclePresetFile> m_aFiles = {};
	protected int m_iRevision;
	int Revision() { return m_iRevision; }

	void AICF_VehiclePresetStore(string directory = "$profile:AICF_Loadouts")
	{
		m_sDirectory = directory;
		m_aFiles.Resize(CAPACITY);
		if (!Replication.IsServer()) return;
		for (int i; i < CAPACITY; i++)
		{
			AICF_VehiclePresetFile first = AICF_VehiclePresetFile.Read(Path(i, 0));
			AICF_VehiclePresetFile second = AICF_VehiclePresetFile.Read(Path(i, 1));
			if (first && (!second || first.m_iVersion >= second.m_iVersion)) m_aFiles[i] = first;
			else m_aFiles[i] = second;
		}
	}

	protected string Path(int id, int generation)
	{
		return m_sDirectory + "/vehicle_" + id.ToString() + "_" + generation.ToString() + ".json";
	}

	AICF_VehiclePreset Get(int id)
	{
		if (id < 0 || id >= CAPACITY || !m_aFiles[id] || m_aFiles[id].m_bDeleted) return null;
		return AICF_VehiclePreset.Decode(m_aFiles[id].m_sData);
	}

	bool Save(AICF_VehiclePreset preset, out string reason)
	{
		reason = "INVALID_PRESET";
		if (!Replication.IsServer() || !preset || !preset.ValidateShape(reason)) return false;
		int free = -1;
		for (int i; i < CAPACITY; i++)
		{
			AICF_VehiclePreset current = Get(i);
			if (!current) { if (free < 0) free = i; continue; }
			if (current.m_sProfile != preset.m_sProfile || current.m_sFaction != preset.m_sFaction || current.m_sName != preset.m_sName) continue;
			if (current.Encode() == preset.Encode()) { reason = "SAVED"; return true; }
			return Write(i, preset.Encode(), false, reason);
		}
		reason = "LIBRARY_FULL";
		if (free < 0) return false;
		return Write(free, preset.Encode(), false, reason);
	}

	bool Delete(int id, out string reason)
	{
		reason = "PRESET_UNAVAILABLE";
		if (!Get(id)) return false;
		return Write(id, "", true, reason);
	}

	protected bool Write(int id, string data, bool deleted, out string reason)
	{
		reason = "LIBRARY_WRITE_FAILED";
		if (!Replication.IsServer() || id < 0 || id >= CAPACITY) return false;
		AICF_VehiclePresetFile next = new AICF_VehiclePresetFile();
		next.m_iVersion = 1;
		if (m_aFiles[id]) next.m_iVersion = m_aFiles[id].m_iVersion + 1;
		if (next.m_iVersion < 1 || next.m_iVersion >= 2147483646) return false;
		next.m_bDeleted = deleted;
		next.m_sData = data;
		if (!FileIO.FileExists(m_sDirectory) && !FileIO.MakeDirectory(m_sDirectory)) return false;
		JsonSaveContext json = new JsonSaveContext();
		next.Write(json);
		string path = Path(id, next.m_iVersion % 2);
		if (!json.SaveToFile(path)) return false;
		AICF_VehiclePresetFile saved = AICF_VehiclePresetFile.Read(path);
		if (!saved || saved.Encode() != next.Encode()) return false;
		m_aFiles[id] = saved;
		m_iRevision++;
		reason = "SAVED";
		return true;
	}

	string List(string profile, string faction)
	{
		array<int> ids = {};
		array<string> names = {};
		for (int i; i < CAPACITY; i++)
		{
			AICF_VehiclePreset preset = Get(i);
			if (!preset || preset.m_sProfile != profile || preset.m_sFaction != faction) continue;
			ids.Insert(i);
			names.Insert(preset.m_sName);
		}
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("ids", ids);
		json.WriteValue("names", names);
		json.WriteValue("revision", m_iRevision);
		return json.SaveToString();
	}
}
