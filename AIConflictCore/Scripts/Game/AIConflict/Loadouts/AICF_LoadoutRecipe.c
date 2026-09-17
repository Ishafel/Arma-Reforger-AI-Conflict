// Ограниченный рецепт изменений относительно исходного character prefab.
// Клиент не передаёт stock inventory JSON, цену, character labels или entity ID.
class AICF_LoadoutRecipe
{
	static const int MAX_OPERATIONS = 48;
	static const int MAX_BYTES = 24576;
	string m_sProfile;
	string m_sFaction;
	ResourceName m_sCharacter;
	string m_sName;
	ref array<string> m_aPaths = {};
	ref array<string> m_aStorages = {};
	ref array<int> m_aSlots = {};
	ref array<int> m_aCounts = {};
	ref array<string> m_aPrefabs = {};

	bool Read(JsonLoadContext context)
	{
		int schema;
		if (!context.ReadValue("schemaVersion", schema) || schema != 1 ||
			!context.ReadValue("profile", m_sProfile) || !context.ReadValue("faction", m_sFaction) ||
			!context.ReadValue("character", m_sCharacter) || !context.ReadValue("name", m_sName) ||
			!context.ReadValue("paths", m_aPaths) || !context.ReadValue("storages", m_aStorages) ||
			!context.ReadValue("slots", m_aSlots) || !context.ReadValue("counts", m_aCounts) ||
			!context.ReadValue("prefabs", m_aPrefabs))
			return false;
		return HasValidBounds();
	}

	bool HasValidBounds()
	{
		int count = m_aPaths.Count();
		if (count > MAX_OPERATIONS || m_aStorages.Count() != count || m_aSlots.Count() != count ||
			m_aCounts.Count() != count || m_aPrefabs.Count() != count || m_sName.Length() > 80 ||
			m_sProfile.Length() > 80 || m_sCharacter.Length() > 256 ||
			(m_sFaction != "US" && m_sFaction != "USSR"))
			return false;
		for (int i; i < count; i++)
		{
			if (m_aPaths[i].Length() > 768 || m_aStorages[i].Length() > 128 || m_aStorages[i].IsEmpty() ||
				m_aSlots[i] < -1 || m_aSlots[i] > 255 || m_aCounts[i] < 1 || m_aCounts[i] > 16 ||
				(m_aSlots[i] >= 0 && m_aCounts[i] != 1) || m_aPrefabs[i].Length() > 256 ||
				(m_aPrefabs[i].IsEmpty() && m_aSlots[i] < 0))
				return false;
			array<string> segments = {};
			m_aPaths[i].Split("/", segments, true);
			if (segments.Count() > 6)
				return false;
		}
		return true;
	}

	void Write(JsonSaveContext context)
	{
		context.WriteValue("schemaVersion", 1);
		context.WriteValue("profile", m_sProfile);
		context.WriteValue("faction", m_sFaction);
		context.WriteValue("character", m_sCharacter);
		context.WriteValue("name", m_sName);
		context.WriteValue("paths", m_aPaths);
		context.WriteValue("storages", m_aStorages);
		context.WriteValue("slots", m_aSlots);
		context.WriteValue("counts", m_aCounts);
		context.WriteValue("prefabs", m_aPrefabs);
	}

	string Encode()
	{
		JsonSaveContext context = new JsonSaveContext();
		Write(context);
		return context.SaveToString();
	}

	static AICF_LoadoutRecipe Decode(string value)
	{
		if (value.IsEmpty() || value.Length() > MAX_BYTES)
			return null;
		JsonLoadContext context = new JsonLoadContext();
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		if (!context.LoadFromString(value) || !recipe.Read(context) || recipe.m_sName.IsEmpty() ||
			recipe.m_sName.Contains("\n") || recipe.m_sName.Contains("\r"))
			return null;
		return recipe;
	}

	void Add(string path, string storage, int slot, ResourceName prefab, int count)
	{
		m_aPaths.Insert(path);
		m_aStorages.Insert(storage);
		m_aSlots.Insert(slot);
		m_aPrefabs.Insert(prefab);
		m_aCounts.Insert(count);
	}

	void Undo()
	{
		int last = m_aPaths.Count() - 1;
		if (last < 0)
			return;
		m_aPaths.Remove(last);
		m_aStorages.Remove(last);
		m_aSlots.Remove(last);
		m_aPrefabs.Remove(last);
		m_aCounts.Remove(last);
	}
}

class AICF_LoadoutBinding
{
	ref AICF_LoadoutRecipe m_Recipe;
	string m_sInventory;
	string m_sSignature;
	int m_iCost;
	int m_iRevision;
}
