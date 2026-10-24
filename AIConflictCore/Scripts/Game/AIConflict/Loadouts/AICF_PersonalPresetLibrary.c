// Именованные варианты отдельно от активного personal recipe и библиотеки ИИ.
// Путь берётся только из проверенной сервером identity; без неё — память controller.
class AICF_PersonalPresetLibrary
{
	protected ref AICF_LoadoutStore m_Store;
	protected bool m_bPersistent;

	void AICF_PersonalPresetLibrary(string libraryPath, bool persistent)
	{
		m_bPersistent = persistent;
		array<string> pieces = {};
		libraryPath.Split("/", pieces, true);
		string prefix = pieces[pieces.Count() - 1] + "_preset_";
		// Flat prefix не требует native MakeDirectory внутри junction.
		m_Store = new AICF_LoadoutStore(AICF_PersonalLoadoutStore.DIRECTORY, persistent, prefix);
	}

	static AICF_PersonalPresetLibrary ForPlayer(SCR_PlayerController player, AICF_LoadoutRecipe context)
	{
		if (!Replication.IsServer() || !player || !context) return null;
		string path = AICF_PersonalLoadoutStore.Path(player, context);
		bool persistent = !path.IsEmpty();
		string key = path + "_library";
		if (!persistent) key = "session_" + context.m_sProfile + "_" + context.m_sFaction;
		return player.AICF_PersonalLibrary(key, persistent);
	}

	int GetRevision() { return m_Store.GetRevision(); }
	AICF_LoadoutRecipe Get(int id) { return m_Store.Get(id); }

	bool Save(AICF_LoadoutRecipe recipe, out string reason)
	{
		reason = "LIBRARY_WRITE_FAILED";
		if (!Replication.IsServer()) return false;
		if (m_bPersistent && !FileIO.FileExists(AICF_PersonalLoadoutStore.DIRECTORY) &&
			!FileIO.MakeDirectory(AICF_PersonalLoadoutStore.DIRECTORY)) return false;
		return m_Store.Save(recipe, reason);
	}

	bool Delete(int id, out string reason) { return m_Store.Delete(id, reason); }

	string Snapshot(AICF_LoadoutRecipe target, string rules)
	{
		array<int> ids = {};
		array<string> names = {}, kinds = {}, denied = {};
		JsonLoadContext entries = new JsonLoadContext();
		if (entries.LoadFromString(m_Store.List(target)))
		{
			entries.ReadValue("ids", ids);
			entries.ReadValue("names", names);
			entries.ReadValue("kinds", kinds);
		}
		JsonLoadContext blacklist = new JsonLoadContext();
		if (blacklist.LoadFromString(rules)) blacklist.ReadValue("denied", denied);
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("ids", ids);
		json.WriteValue("names", names);
		json.WriteValue("kinds", kinds);
		json.WriteValue("revision", GetRevision());
		json.WriteValue("denied", denied);
		return json.SaveToString();
	}
}

modded class SCR_PlayerController
{
	protected ref map<string, ref AICF_PersonalPresetLibrary> m_mAICFPersonalLibraries = new map<string, ref AICF_PersonalPresetLibrary>();

	AICF_PersonalPresetLibrary AICF_PersonalLibrary(string key, bool persistent)
	{
		if (!Replication.IsServer()) return null;
		AICF_PersonalPresetLibrary library;
		if (!m_mAICFPersonalLibraries.Find(key, library))
		{
			library = new AICF_PersonalPresetLibrary(key, persistent);
			m_mAICFPersonalLibraries.Set(key, library);
		}
		return library;
	}
}
