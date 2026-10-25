// Два поколения файла сохраняют последнюю проверенную версию;
// tombstone запрещает восстановление удалённого legacy пресета.
class AICF_LoadoutPreset
{
	int m_iVersion;
	bool m_bDeleted;
	string m_sKind;
	string m_sProfile;
	string m_sFaction;
	string m_sCharacter;
	string m_sName;
	string m_sData;

	void Write(JsonSaveContext json)
	{
		json.WriteValue("schemaVersion", 2);
		json.WriteValue("version", m_iVersion);
		json.WriteValue("deleted", m_bDeleted);
		json.WriteValue("kind", m_sKind);
		json.WriteValue("profile", m_sProfile);
		json.WriteValue("faction", m_sFaction);
		json.WriteValue("character", m_sCharacter);
		json.WriteValue("name", m_sName);
		json.WriteValue("data", m_sData);
	}

	string Encode()
	{
		JsonSaveContext json = new JsonSaveContext();
		Write(json);
		return json.SaveToString();
	}

	static AICF_LoadoutPreset Read(string path)
	{
		if (!FileIO.FileExists(path)) return null;
		JsonLoadContext json = new JsonLoadContext();
		AICF_LoadoutPreset preset = new AICF_LoadoutPreset();
		int schema;
		if (!json.LoadFromFile(path) || !json.ReadValue("schemaVersion", schema) || schema != 2 ||
			!json.ReadValue("version", preset.m_iVersion) || preset.m_iVersion < 1 ||
			!json.ReadValue("deleted", preset.m_bDeleted) || !json.ReadValue("kind", preset.m_sKind) ||
			!json.ReadValue("profile", preset.m_sProfile) || !json.ReadValue("faction", preset.m_sFaction) ||
			!json.ReadValue("character", preset.m_sCharacter) || !json.ReadValue("name", preset.m_sName) ||
			!json.ReadValue("data", preset.m_sData) || !preset.IsValid()) return null;
		return preset;
	}

	bool IsValid()
	{
		if (!ValidName(m_sName) || m_sProfile.IsEmpty() || m_sProfile.Length() > 80 ||
			(m_sFaction != "US" && m_sFaction != "USSR") || m_sCharacter.Length() > 256) return false;
		if (m_sKind != "member" && m_sKind != "squad") return false;
		if (m_bDeleted) return m_sData.IsEmpty();
		if (m_sKind == "squad")
		{
			AICF_SquadLoadout squad = AICF_SquadLoadout.Decode(m_sData);
			return squad && squad.m_sProfile == m_sProfile && squad.m_sFaction == m_sFaction && squad.m_sName == m_sName;
		}
		AICF_LoadoutRecipe recipe = AICF_LoadoutRecipe.Decode(m_sData);
		return recipe && recipe.m_sProfile == m_sProfile && recipe.m_sFaction == m_sFaction &&
			recipe.m_sCharacter == m_sCharacter && recipe.m_sName == m_sName;
	}

	static bool ValidName(string name)
	{
		string trimmed = name;
		trimmed.TrimInPlace();
		return !trimmed.IsEmpty() && name.Length() <= 80 && !name.Contains("\n") && !name.Contains("\r");
	}

	bool SameKey(AICF_LoadoutPreset other)
	{
		return other && m_sKind == other.m_sKind && m_sProfile == other.m_sProfile &&
			m_sFaction == other.m_sFaction && m_sCharacter == other.m_sCharacter && m_sName == other.m_sName;
	}
}

// Все десять numeric positions, включая неразвёрнутые роли. Размер отряда,
// стратегия и runtime roster не принадлежат пресету экипировки.
class AICF_SquadLoadout
{
	string m_sProfile;
	string m_sFaction;
	string m_sName;
	ref array<string> m_aRecipes = {};

	string Encode()
	{
		JsonSaveContext json = new JsonSaveContext();
		json.WriteValue("schemaVersion", 1);
		json.WriteValue("profile", m_sProfile);
		json.WriteValue("faction", m_sFaction);
		json.WriteValue("name", m_sName);
		json.WriteValue("recipes", m_aRecipes);
		return json.SaveToString();
	}

	static AICF_SquadLoadout Decode(string data)
	{
		if (data.IsEmpty() || data.Length() > 300000) return null;
		JsonLoadContext json = new JsonLoadContext();
		AICF_SquadLoadout squad = new AICF_SquadLoadout();
		int schema;
		if (!json.LoadFromString(data) || !json.ReadValue("schemaVersion", schema) || schema != 1 ||
			!json.ReadValue("profile", squad.m_sProfile) || !json.ReadValue("faction", squad.m_sFaction) ||
			!json.ReadValue("name", squad.m_sName) || !AICF_LoadoutPreset.ValidName(squad.m_sName) ||
			!json.ReadValue("recipes", squad.m_aRecipes) || squad.m_aRecipes.Count() != AICF_Stage1Config.MAX_GROUP_SIZE) return null;
		foreach (string value : squad.m_aRecipes)
		{
			AICF_LoadoutRecipe recipe = AICF_LoadoutRecipe.Decode(value);
			if (!recipe || recipe.m_sProfile != squad.m_sProfile || recipe.m_sFaction != squad.m_sFaction) return null;
		}
		return squad;
	}
}

// Commit повторно сверяет ВСЕ revisions до синхронных field writes;
// callbacks, inventory, spawn и waypoint здесь отсутствуют.
class AICF_LoadoutBatch
{
	ref array<AICF_GroupSlot> m_aSlots = {};
	ref array<int> m_aMembers = {};
	ref array<int> m_aRevisions = {};
	ref array<ref AICF_LoadoutBinding> m_aBindings = {};

	void Add(AICF_GroupSlot slot, int member, AICF_LoadoutBinding binding)
	{
		m_aSlots.Insert(slot);
		m_aMembers.Insert(member);
		m_aRevisions.Insert(slot.GetLoadoutRevision());
		m_aBindings.Insert(binding);
	}

	bool Commit()
	{
		if (!Replication.IsServer() || m_aSlots.IsEmpty()) return false;
		if (m_aMembers.Count() != m_aSlots.Count() || m_aRevisions.Count() != m_aSlots.Count() ||
			m_aBindings.Count() != m_aSlots.Count()) return false;
		for (int i; i < m_aSlots.Count(); i++)
		{
			if (!m_aSlots[i] || !m_aBindings[i] || !m_aBindings[i].m_Recipe || m_aSlots[i].GetLoadoutRevision() != m_aRevisions[i] ||
				m_aMembers[i] < 0 || m_aMembers[i] >= AICF_Stage1Config.MAX_GROUP_SIZE) return false;
			if (!AICF_LoadoutRecipe.Decode(m_aBindings[i].m_Recipe.Encode())) return false;
		}
		for (int j; j < m_aSlots.Count(); j++)
			m_aSlots[j].SetLoadout(m_aMembers[j], m_aBindings[j], m_aSlots[j].GetLoadoutRevision());
		return true;
	}
}
