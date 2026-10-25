// Только isolated stage. Два native запуска с разными profiles и одной библиотекой.
modded class AICF_MatchController
{
	protected bool m_bPersistenceProbed;
	protected int m_iPersistenceFailures;

	protected void AICF_PersistenceCheck(string name, bool passed)
	{
		if (!passed) m_iPersistenceFailures++;
		Print(string.Format("[AICF][PERSISTENCE_CHECK] rule=%1 passed=%2", name, passed));
	}

	override protected void Update()
	{
		super.Update();
		string phase;
		if (!m_bRosterReady || m_bPersistenceProbed || !System.GetCLIParam("aicfPersistenceProbe", phase)) return;
		m_bPersistenceProbed = true;
		string role;
		ResourceName source = m_GroupSpawner.ResolveRecruitPrefab(m_USFaction, 0, role);
		AICF_LoadoutRecipe recipe = AICF_LoadoutService.DefaultRecipe(m_USFaction, source, 0);
		recipe.m_sName = "Persistent commander";
		string reason;
		if (phase == "seed")
		{
			if (!FileIO.FileExists(AICF_LoadoutStore.DIRECTORY)) FileIO.MakeDirectory(AICF_LoadoutStore.DIRECTORY);
			JsonSaveContext legacy = new JsonSaveContext();
			recipe.Write(legacy);
			AICF_PersistenceCheck("LEGACY_WRITE", legacy.SaveToFile(AICF_LoadoutStore.DIRECTORY + "/template_42.json"));
			AICF_LoadoutStore store = new AICF_LoadoutStore();
			AICF_PersistenceCheck("LEGACY_READ", store.Get(42) && store.Get(42).m_sName == recipe.m_sName);
			recipe.m_sName = "Persistent commander updated";
			AICF_PersistenceCheck("MEMBER_SAVE", store.Save(recipe, reason));
			AICF_SquadLoadout squad = new AICF_SquadLoadout();
			squad.m_sProfile = recipe.m_sProfile;
			squad.m_sFaction = recipe.m_sFaction;
			squad.m_sName = "Persistent squad";
			for (int i; i < AICF_Stage1Config.MAX_GROUP_SIZE; i++)
			{
				source = m_GroupSpawner.ResolveRecruitPrefab(m_USFaction, i, role);
				AICF_LoadoutRecipe member = AICF_LoadoutService.DefaultRecipe(m_USFaction, source, i);
				squad.m_aRecipes.Insert(member.Encode());
			}
			AICF_LoadoutPreset preset = new AICF_LoadoutPreset();
			preset.m_sKind = "squad";
			preset.m_sProfile = squad.m_sProfile;
			preset.m_sFaction = squad.m_sFaction;
			preset.m_sName = squad.m_sName;
			preset.m_sData = squad.Encode();
			AICF_PersistenceCheck("SQUAD_SAVE", store.SavePreset(preset, reason));
			AICF_PersistenceCheck("TOMBSTONE_SAVE", store.Delete(42, reason));
		}
		else if (phase == "reload")
		{
			AICF_LoadoutStore loaded = new AICF_LoadoutStore();
			AICF_LoadoutRecipe member = loaded.Get(0);
			AICF_PersistenceCheck("MEMBER_RESTART", member && member.m_sName == "Persistent commander updated");
			AICF_LoadoutPreset squad = loaded.GetPreset(1);
			AICF_PersistenceCheck("SQUAD_RESTART", squad && squad.m_sKind == "squad" && AICF_SquadLoadout.Decode(squad.m_sData));
			AICF_PersistenceCheck("TOMBSTONE_RESTART", !loaded.GetPreset(42));
			JsonLoadContext legacy = new JsonLoadContext();
			AICF_LoadoutRecipe original = new AICF_LoadoutRecipe();
			AICF_PersistenceCheck("LEGACY_PRESERVED", legacy.LoadFromFile(AICF_LoadoutStore.DIRECTORY + "/template_42.json") &&
				original.Read(legacy) && original.m_sName == "Persistent commander");
		}
		else if (phase == "legacy")
		{
			AICF_LoadoutStore imported = new AICF_LoadoutStore();
			for (int id; id < 10; id++)
			{
				AICF_LoadoutRecipe old = imported.Get(id);
				AICF_PersistenceCheck("LEGACY_IMPORTED_" + id.ToString(), old && old.m_sProfile == recipe.m_sProfile && old.HasValidBounds());
			}
		}
		else AICF_PersistenceCheck("VALID_PHASE", false);
		Print(string.Format("[AICF][PERSISTENCE_DONE] phase=%1 failures=%2", phase, m_iPersistenceFailures));
		GetGame().RequestClose();
	}
}
