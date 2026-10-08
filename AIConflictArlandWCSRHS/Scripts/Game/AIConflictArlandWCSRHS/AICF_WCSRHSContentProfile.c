// Карта, readiness, roster и транспорт сохраняют владельцев из RHS integration.
class AICF_WCSRHSContentProfile : AICF_RHSContentProfile
{
	override bool BuildCharacterRoleCandidates(FactionKey stableKey, int memberIndex, out string role, out array<string> suffixes)
	{
		AICF_EDifficulty difficulty = AICF_Difficulty.Get();
		// Medium/Hard North: три ПТ на десять бойцов. Те же role prefab,
		// inventory и цены используются начальным roster и пополнением.
		if ((difficulty == AICF_EDifficulty.MEDIUM || difficulty == AICF_EDifficulty.HARD) &&
			(memberIndex == 8 || memberIndex == 9))
			return super.BuildCharacterRoleCandidates(stableKey, 3, role, suffixes);
		return super.BuildCharacterRoleCandidates(stableKey, memberIndex, role, suffixes);
	}

	// Кеш принадлежит экземпляру profile одного матча. Только строки,
	// без entity, preview world, catalog или изменяемых inventory objects.
	protected ref map<string, string> m_mKitSnapshots = new map<string, string>();
	protected ref map<string, string> m_mKitSignatures = new map<string, string>();

	bool FindPreparedKit(FactionKey faction, ResourceName source, out string snapshot, out string signature)
	{
		string key = faction + "|" + source;
		return m_mKitSnapshots.Find(key, snapshot) && m_mKitSignatures.Find(key, signature);
	}

	void StorePreparedKit(FactionKey faction, ResourceName source, string snapshot, string signature)
	{
		if (!Replication.IsServer() || snapshot.IsEmpty() || signature.IsEmpty()) return;
		// Полный concrete prefab включает роль и сезонный/числовой вариант.
		// FIFO не нужен: при заполнении новые варианты работают без кеша.
		string key = faction + "|" + source;
		if (m_mKitSnapshots.Contains(key) || m_mKitSnapshots.Count() >= 64) return;
		m_mKitSnapshots.Insert(key, snapshot);
		m_mKitSignatures.Insert(key, signature);
	}

	void ClearPreparedKits()
	{
		m_mKitSnapshots.Clear();
		m_mKitSignatures.Clear();
	}

	override ResourceName GetFIAGarrisonPrefab(bool tank)
	{
		if (tank) return "{2B9DB09AC8BEA673}Prefabs/Vehicles/Tracked/T72A/T72A_FIA.et";
		return super.GetFIAGarrisonPrefab(tank);
	}

	override string GetProfileKey()
	{
		return "WCS_RHS_ARLAND";
	}

	override void GetLoadoutSources(array<string> sources)
	{
		super.GetLoadoutSources(sources);
		sources.Insert("WCS");
	}

	// Штатный player kit WCS использует командира ИИ той же стороны.
	override ResourceName GetPersonalLoadoutSource(SCR_CampaignFaction faction, ResourceName roleSource)
	{
		AICF_GroupSpawner roster = new AICF_GroupSpawner();
		string role;
		return roster.ResolveRecruitPrefab(faction, 0, role);
	}

	override bool PrepareDefaultLoadout(IEntity model, ResourceName source, FactionKey stableKey)
	{
		if (!super.PrepareDefaultLoadout(model, source, stableKey)) return false;
		if (!AICF_WCSInfantryKit.SupportsRole(source)) return true;
		return AICF_WCSInfantryEquipment.BuildDefault(model, GetRuntimeFactionKey(stableKey), source);
	}

	protected ref AICF_WCSItemOrigins m_LoadoutOrigins;

	override string GetLoadoutItemSource(ResourceName prefab)
	{
		if (!m_LoadoutOrigins) m_LoadoutOrigins = new AICF_WCSItemOrigins();
		string source;
		if (m_LoadoutOrigins.Find(prefab, source)) return source;
		return super.GetLoadoutItemSource(prefab);
	}

	override ResourceName GetPMCPrimaryOverride(ResourceName source, int variant)
	{
		// WCS меняет старые ION variants и canonical spelling PKM.
		// Сохраняем loaded/spare/suppressed проверки прежнего PMC adapter.
		if (source.EndsWith("_MG.et"))
			return "{4BE4931DF7B1ABCD}Prefabs/Weapons/MachineGuns/PKM/MG_PKM_B51_EOT.et";
		if (source.EndsWith("_Sharpshooter.et")) return ResourceName.Empty;
		return "{4E0FB248B9FB1F5B}Prefabs/Weapons/Rifles/M4A1/Variants/Rifle_M4A1_WCS_URGI_Suppressed_SQUAD.et";
	}
}

modded class SCR_GameModeCampaign
{
	override void OnGameEnd()
	{
		AICF_WCSRHSContentProfile profile = AICF_WCSRHSContentProfile.Cast(AICF_ContentProfile.GetActive());
		if (profile) profile.ClearPreparedKits();
		super.OnGameEnd();
	}

	override protected AICF_ContentProfile AICF_CreateContentProfile()
	{
		return new AICF_WCSRHSContentProfile();
	}
}

modded class AICF_MatchController
{
	override protected void Stop(bool cleanupEntities)
	{
		super.Stop(cleanupEntities);
		AICF_WCSRHSContentProfile profile = AICF_WCSRHSContentProfile.Cast(AICF_ContentProfile.GetActive());
		if (profile) profile.ClearPreparedKits();
	}
}
