// Библиотека переживает restart; bindings принадлежат только кампании.
class AICF_LoadoutStore
{
	static const int MAX_TEMPLATES = 256;
	static const string DIRECTORY = "$profile:AICF_Loadouts";
	protected ref array<ref AICF_LoadoutPreset> m_aTemplates = {};
	protected int m_iRevision;
	int GetRevision() { return m_iRevision; }

	void AICF_LoadoutStore()
	{
		m_aTemplates.Resize(MAX_TEMPLATES);
		if (!Replication.IsServer()) return;
		for (int i; i < MAX_TEMPLATES; i++)
		{
			AICF_LoadoutPreset first = AICF_LoadoutPreset.Read(Path(i, 0));
			AICF_LoadoutPreset second = AICF_LoadoutPreset.Read(Path(i, 1));
			if (first && (!second || first.m_iVersion >= second.m_iVersion)) m_aTemplates[i] = first;
			else if (second) m_aTemplates[i] = second;
			else
			{
				// Legacy файлы не переписываются. Tombstone v2 имеет приоритет.
				JsonLoadContext json = new JsonLoadContext();
				AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
				string legacy = DIRECTORY + "/template_" + i.ToString() + ".json";
				if (FileIO.FileExists(legacy) && json.LoadFromFile(legacy) && recipe.Read(json) &&
					AICF_LoadoutRecipe.Decode(recipe.Encode())) m_aTemplates[i] = MemberPreset(recipe);
			}
		}
	}

	protected string Path(int id, int generation)
	{
		return DIRECTORY + "/template_" + id.ToString() + "_" + generation.ToString() + ".json";
	}

	AICF_LoadoutPreset GetPreset(int id)
	{
		if (id < 0 || id >= m_aTemplates.Count() || !m_aTemplates[id] || m_aTemplates[id].m_bDeleted) return null;
		return m_aTemplates[id];
	}

	AICF_LoadoutRecipe Get(int id)
	{
		AICF_LoadoutPreset preset = GetPreset(id);
		if (!preset || preset.m_sKind != "member") return null;
		return AICF_LoadoutRecipe.Decode(preset.m_sData);
	}

	static AICF_LoadoutPreset MemberPreset(AICF_LoadoutRecipe recipe)
	{
		AICF_LoadoutPreset preset = new AICF_LoadoutPreset();
		preset.m_sKind = "member";
		preset.m_sProfile = recipe.m_sProfile;
		preset.m_sFaction = recipe.m_sFaction;
		preset.m_sCharacter = recipe.m_sCharacter;
		preset.m_sName = recipe.m_sName;
		preset.m_sData = recipe.Encode();
		return preset;
	}

	bool Save(AICF_LoadoutRecipe recipe, out string reason)
	{
		if (!recipe) return false;
		return SavePreset(MemberPreset(recipe), reason);
	}

	bool SavePreset(AICF_LoadoutPreset preset, out string reason)
	{
		reason = "INVALID_RECIPE";
		if (!Replication.IsServer() || !preset || preset.m_bDeleted || !preset.IsValid()) return false;
		int free = -1;
		for (int i; i < MAX_TEMPLATES; i++)
		{
			AICF_LoadoutPreset current = m_aTemplates[i];
			if (!current || current.m_bDeleted)
			{
				if (free < 0) free = i;
				continue;
			}
			if (!current.SameKey(preset)) continue;
			if (current.m_sData == preset.m_sData) { reason = string.Empty; return true; }
			return Write(i, preset, reason);
		}
		reason = "LIBRARY_FULL";
		if (free < 0) return false;
		return Write(free, preset, reason);
	}

	bool Delete(int id, out string reason)
	{
		reason = "TEMPLATE_UNAVAILABLE";
		AICF_LoadoutPreset current = GetPreset(id);
		if (!Replication.IsServer() || !current) return false;
		AICF_LoadoutPreset tombstone = new AICF_LoadoutPreset();
		tombstone.m_sKind = current.m_sKind;
		tombstone.m_sProfile = current.m_sProfile;
		tombstone.m_sFaction = current.m_sFaction;
		tombstone.m_sCharacter = current.m_sCharacter;
		tombstone.m_sName = current.m_sName;
		tombstone.m_bDeleted = true;
		return Write(id, tombstone, reason);
	}

	protected bool Write(int id, AICF_LoadoutPreset preset, out string reason)
	{
		reason = "LIBRARY_WRITE_FAILED";
		if (!Replication.IsServer() || id < 0 || id >= MAX_TEMPLATES || !preset || !preset.IsValid()) return false;
		int version = 1;
		if (m_aTemplates[id]) version = m_aTemplates[id].m_iVersion + 1;
		if (version < 1 || version >= 2147483646) return false;
		preset.m_iVersion = version;
		if (!FileIO.FileExists(DIRECTORY) && !FileIO.MakeDirectory(DIRECTORY)) return false;
		string path = Path(id, version % 2);
		JsonSaveContext json = new JsonSaveContext();
		preset.Write(json);
		if (!json.SaveToFile(path)) return false;
		AICF_LoadoutPreset saved = AICF_LoadoutPreset.Read(path);
		if (!saved || saved.Encode() != preset.Encode()) return false;
		m_aTemplates[id] = saved;
		m_iRevision++;
		reason = string.Empty;
		return true;
	}

	string List(AICF_LoadoutRecipe target)
	{
		array<string> names = {};
		array<string> kinds = {};
		array<int> ids = {};
		for (int i; i < MAX_TEMPLATES; i++)
		{
			AICF_LoadoutPreset preset = GetPreset(i);
			if (!preset || !target || preset.m_sProfile != target.m_sProfile || preset.m_sFaction != target.m_sFaction ||
				(preset.m_sKind == "member" && preset.m_sCharacter != target.m_sCharacter)) continue;
			names.Insert(preset.m_sName);
			kinds.Insert(preset.m_sKind);
			ids.Insert(i);
		}
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("names", names);
		json.WriteValue("ids", ids);
		json.WriteValue("kinds", kinds);
		json.WriteValue("revision", m_iRevision);
		return json.SaveToString();
	}
}
