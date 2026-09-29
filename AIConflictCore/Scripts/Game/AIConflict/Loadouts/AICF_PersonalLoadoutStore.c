// Два поколения личного файла: прерванная запись не уничтожает предыдущий
// пресет. Identity берётся только на сервере, numeric playerId не сохраняется.
class AICF_PersonalLoadoutStore
{
	static const string DIRECTORY = "$profile:AICF_PersonalLoadouts";

	static bool SafeKey(string value)
	{
		if (value.IsEmpty() || value.Length() > 80) return false;
		string allowed = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
		for (int i; i < value.Length(); i++)
			if (!allowed.Contains(value.Substring(i, 1))) return false;
		return true;
	}

	static string Path(SCR_PlayerController player, AICF_LoadoutRecipe context)
	{
		if (!Replication.IsServer() || !player || !context) return string.Empty;
		// Без backend identity недоступен общий файл; остаётся session storage.
		string uid = GetGame().GetBackendApi().GetPlayerIdentityId(player.GetPlayerId());
		if (uid == "00000000-0000-0000-0000-000000000000") return string.Empty;
		if (!SafeKey(uid) || !SafeKey(context.m_sProfile) || !SafeKey(context.m_sFaction)) return string.Empty;
		return DIRECTORY + "/" + uid + "_" + context.m_sProfile + "_" + context.m_sFaction;
	}

	static AICF_LoadoutRecipe LoadFor(SCR_PlayerController player, AICF_LoadoutRecipe context, out int revision)
	{
		revision = 0;
		if (!Replication.IsServer() || !player || !context) return null;
		string path = Path(player, context);
		if (!path.IsEmpty()) return Load(path, revision);
		return player.AICF_PersonalSession().Load(context, revision);
	}

	static bool SaveFor(SCR_PlayerController player, AICF_LoadoutRecipe context, AICF_LoadoutRecipe recipe, int revision)
	{
		if (!Replication.IsServer() || !player || !AICF_PersonalLoadout.SameContext(recipe, context)) return false;
		string path = Path(player, context);
		if (!path.IsEmpty()) return Save(path, recipe, revision);
		return player.AICF_PersonalSession().Save(context, recipe, revision);
	}

	static AICF_LoadoutRecipe Read(string path, out int revision)
	{
		revision = 0;
		if (!Replication.IsServer() || !FileIO.FileExists(path)) return null;
		JsonLoadContext json = new JsonLoadContext();
		string data;
		int version;
		if (!json.LoadFromFile(path) || !json.ReadValue("revision", version) || version < 1 ||
			!json.ReadValue("recipe", data)) return null;
		AICF_LoadoutRecipe recipe = AICF_LoadoutRecipe.Decode(data);
		if (recipe) revision = version;
		return recipe;
	}

	static AICF_LoadoutRecipe Load(string path, out int revision)
	{
		revision = 0;
		if (path.IsEmpty()) return null;
		int firstRevision, secondRevision;
		AICF_LoadoutRecipe first = Read(path + "_0.json", firstRevision);
		AICF_LoadoutRecipe second = Read(path + "_1.json", secondRevision);
		if (secondRevision > firstRevision)
		{
			revision = secondRevision;
			return second;
		}
		revision = firstRevision;
		return first;
	}

	static bool Save(string path, AICF_LoadoutRecipe recipe, int revision)
	{
		if (!Replication.IsServer() || path.IsEmpty() || !recipe || revision < 1 || !recipe.HasValidBounds()) return false;
		if (!FileIO.FileExists(DIRECTORY) && !FileIO.MakeDirectory(DIRECTORY)) return false;
		string file = path + "_" + (revision % 2).ToString() + ".json";
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("revision", revision);
		json.WriteValue("recipe", recipe.Encode());
		if (!json.SaveToFile(file)) return false;
		int actualRevision;
		AICF_LoadoutRecipe actual = Read(file, actualRevision);
		return actual && actualRevision == revision && actual.Encode() == recipe.Encode();
	}
}

// Нет backend identity — нет общего файла по имени или numeric playerId.
// Рецепты принадлежат конкретному controller и исчезают с его подключением.
class AICF_PersonalSessionStore
{
	protected ref map<string, ref AICF_LoadoutBinding> m_mPresets = new map<string, ref AICF_LoadoutBinding>();

	AICF_LoadoutRecipe Load(AICF_LoadoutRecipe context, out int revision)
	{
		revision = 0;
		AICF_LoadoutBinding saved;
		if (!Replication.IsServer() || !context || !m_mPresets.Find(context.Encode(), saved)) return null;
		revision = saved.m_iRevision;
		return AICF_LoadoutRecipe.Decode(saved.m_Recipe.Encode());
	}

	bool Save(AICF_LoadoutRecipe context, AICF_LoadoutRecipe recipe, int revision)
	{
		if (!Replication.IsServer() || !AICF_PersonalLoadout.SameContext(recipe, context) || !recipe.HasValidBounds()) return false;
		int current;
		Load(context, current);
		if (current >= 2147483646 || revision != current + 1) return false;
		AICF_LoadoutBinding saved = new AICF_LoadoutBinding();
		saved.m_Recipe = AICF_LoadoutRecipe.Decode(recipe.Encode());
		if (!saved.m_Recipe) return false;
		saved.m_iRevision = revision;
		m_mPresets.Set(context.Encode(), saved);
		return true;
	}
}
