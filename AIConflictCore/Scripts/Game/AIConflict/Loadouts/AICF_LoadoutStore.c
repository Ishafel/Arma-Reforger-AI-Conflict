// Неизменяемые файлы: неудачная запись не уничтожает прежнюю версию.
// Библиотека переживает restart; bindings намеренно принадлежат только кампании.
class AICF_LoadoutStore
{
	static const int MAX_TEMPLATES = 256;
	static const string DIRECTORY = "$profile:AICF_Loadouts";
	protected ref array<ref AICF_LoadoutRecipe> m_aTemplates = {};

	void AICF_LoadoutStore()
	{
		m_aTemplates.Resize(MAX_TEMPLATES);
		if (!Replication.IsServer())
			return;
		for (int i; i < MAX_TEMPLATES; i++)
		{
			if (!FileIO.FileExists(Path(i)))
				continue;
			JsonLoadContext context = new JsonLoadContext();
			AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
			if (context.LoadFromFile(Path(i)) && recipe.Read(context) && recipe.Encode().Length() <= AICF_LoadoutRecipe.MAX_BYTES)
				m_aTemplates[i] = recipe;
		}
	}

	protected string Path(int id)
	{
		return DIRECTORY + "/template_" + id.ToString() + ".json";
	}

	AICF_LoadoutRecipe Get(int id)
	{
		if (id < 0 || id >= m_aTemplates.Count())
			return null;
		return m_aTemplates[id];
	}

	bool Save(AICF_LoadoutRecipe recipe, out string reason)
	{
		reason = "LIBRARY_WRITE_FAILED";
		if (!Replication.IsServer() || !recipe || !recipe.HasValidBounds())
			return false;
		if (!FileIO.FileExists(DIRECTORY) && !FileIO.MakeDirectory(DIRECTORY))
			return false;
		for (int i; i < MAX_TEMPLATES; i++)
		{
			if (FileIO.FileExists(Path(i)))
				continue;
			JsonSaveContext context = new JsonSaveContext();
			recipe.Write(context);
			if (!context.SaveToFile(Path(i)))
				return false;
			JsonLoadContext verify = new JsonLoadContext();
			AICF_LoadoutRecipe saved = new AICF_LoadoutRecipe();
			if (!verify.LoadFromFile(Path(i)) || !saved.Read(verify) || saved.Encode() != recipe.Encode())
				return false;
			m_aTemplates[i] = saved;
			reason = string.Empty;
			return true;
		}
		reason = "LIBRARY_FULL";
		return false;
	}

	string List(AICF_LoadoutRecipe target)
	{
		array<string> names = {};
		array<int> ids = {};
		for (int i; i < MAX_TEMPLATES; i++)
		{
			AICF_LoadoutRecipe recipe = m_aTemplates[i];
			if (!recipe || recipe.m_sProfile != target.m_sProfile || recipe.m_sFaction != target.m_sFaction ||
				recipe.m_sCharacter != target.m_sCharacter)
				continue;
			names.Insert(recipe.m_sName);
			ids.Insert(i);
		}
		JsonSaveContext context = new JsonSaveContext();
		context.WriteValue("names", names);
		context.WriteValue("ids", ids);
		return context.SaveToString();
	}
}
