// Только отдельный stage любого профиля. Проверяет выдачу ПТ-оружия и recruitment donor.
modded class AICF_MatchController
{
	protected int m_iAICFATTicks;
	protected int m_iAICFATFailures;
	protected int m_iAICFATChecks;
	protected bool m_bAICFATFinished;
	protected ref array<SCR_AIGroup> m_aAICFATGroups = {};
	protected ref array<int> m_aAICFATIndices = {};

	protected void AICF_ATCheck(string name, bool passed)
	{
		m_iAICFATChecks++;
		if (!passed) m_iAICFATFailures++;
		Print(string.Format("[AICF][AT_PROBE] case=%1 passed=%2", name, passed));
	}

	protected void AICF_ATFinish()
	{
		m_bAICFATFinished = true;
		Print(string.Format("[AICF][AT_PROBE_FINISHED] difficulty=%1 checks=%2 failures=%3", AICF_Difficulty.Get(), m_iAICFATChecks, m_iAICFATFailures));
		GetGame().RequestClose();
	}

	protected bool AICF_ATLoaded(IEntity character, FactionKey faction, int index)
	{
		array<IEntity> items = {};
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!inventory) return false;
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (!weapon || weapon.GetWeaponType() != EWeaponType.WT_ROCKETLAUNCHER) continue;
			array<BaseMuzzleComponent> muzzles = {};
			weapon.GetMuzzlesList(muzzles);
			foreach (BaseMuzzleComponent muzzle : muzzles)
			{
				if (!muzzle) continue;
				Print(string.Format("[AICF][AT_WEAPON] faction=%1 index=%2 prefab=%3 loaded=%4", faction, index, SCR_ResourceNameUtils.GetPrefabName(item), muzzle.GetAmmoCount()));
				if (muzzle.GetAmmoCount() > 0) return true;
			}
		}
		return false;
	}

	protected void AICF_ATSpawn(SCR_CampaignFaction faction, int index)
	{
		int size = 10;
		if (index >= 0) size = 1;
		SCR_AIGroup group = m_GroupSpawner.SpawnGroup(faction, faction.GetMainBase(), 0, size, false);
		m_aAICFATGroups.Insert(group);
		m_aAICFATIndices.Insert(index);
		if (!group) return;
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		group.SetDeleteWhenEmpty(false);
		if (index >= 0)
		{
			// Тот же resolver и one-member donor roster, что у recruitment.
			string role;
			ResourceName prefab = m_GroupSpawner.ResolveRecruitPrefab(faction, index, role);
			group.m_aUnitPrefabSlots.Clear();
			group.m_aUnitPrefabSlots.Insert(prefab);
		}
		group.AICF_TrackMemberPositions();
		AICF_ATCheck("REQUEST_" + faction.GetFactionKey() + "_" + index, m_GroupSpawner.BeginRosterSpawn(group, size));
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfATProbe", enabled) || enabled != "1" || !m_bRosterReady || m_bAICFATFinished) return;
		m_iAICFATTicks++;
		bool reinforced = AICF_Difficulty.Get() == AICF_EDifficulty.MEDIUM || AICF_Difficulty.Get() == AICF_EDifficulty.HARD;
		if (m_iAICFATTicks == 1)
		{
			array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
			foreach (SCR_CampaignFaction faction : factions)
			{
				SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
				array<SCR_EntityCatalogEntry> entries = {};
				if (catalog) catalog.GetEntityList(entries);
				foreach (SCR_EntityCatalogEntry entry : entries)
				{
					string prefab = entry.GetPrefab();
					if (prefab.Contains("Character_US_")) Print("[AICF][AT_CATALOG] prefab=" + prefab);
				}
				AICF_ATSpawn(faction, -1);
				AICF_ATSpawn(faction, 8);
				AICF_ATSpawn(faction, 9);
			}
			return;
		}
		int total;
		foreach (SCR_AIGroup pending : m_aAICFATGroups) total += AICF_GroupRuntime.CountAliveAgents(pending);
		if (total < 24 && m_iAICFATTicks < 90) return;
		AICF_ATCheck("ROSTER_24", total == 24);
		AICF_ContentProfile baseline = AICF_ContentProfile.GetActive();
		AICF_ContentProfile stock = new AICF_ContentProfile();
		foreach (int groupIndex, SCR_AIGroup group : m_aAICFATGroups)
		{
			if (!group) { AICF_ATCheck("GROUP_" + groupIndex, false); continue; }
			FactionKey stable = AICF_ContentProfile.GetActive().GetStableFactionKey(group.GetFaction().GetFactionKey());
			int atCount;
			int size = 10;
			int donorIndex = m_aAICFATIndices[groupIndex];
			if (donorIndex >= 0) size = 1;
			for (int position; position < size; position++)
			{
				int index = position;
				if (donorIndex >= 0) index = donorIndex;
				string tag = stable + "_" + groupIndex + "_" + index;
				SCR_ChimeraCharacter member = SCR_ChimeraCharacter.Cast(group.AICF_GetSpawnMember(position));
				if (!member) { AICF_ATCheck("MEMBER_" + tag, false); continue; }
				int baselineIndex = index;
				if (reinforced && (index == 8 || index == 9)) baselineIndex = 3;
				string expectedRole;
				array<string> suffixes = {};
				baseline.BuildCharacterRoleCandidates(stable, baselineIndex, expectedRole, suffixes);
				string role;
				ResourceName expected = m_GroupSpawner.ResolveRecruitPrefab(SCR_CampaignFaction.Cast(group.GetFaction()), index, role);
				ResourceName actual = SCR_ResourceNameUtils.GetPrefabName(member);
				AICF_ATCheck("ROLE_" + tag, role == expectedRole && suffixes.Count() == 1 && actual.EndsWith(suffixes[0]) && actual == expected);
				if (role == "ANTI_TANK")
				{
					atCount++;
					AICF_ATCheck("LOADED_" + tag, AICF_ATLoaded(member, stable, index));
				}
			}
			int expectedAT = 1;
			if (reinforced) expectedAT = 3;
			if (donorIndex >= 0)
			{
				expectedAT = 0;
				if (reinforced) expectedAT = 1;
			}
			AICF_ATCheck("AT_COUNT_" + stable + "_" + groupIndex, atCount == expectedAT);
			array<string> stockSuffixes = {};
			string stockRole;
			stock.BuildCharacterRoleCandidates(stable, 9, stockRole, stockSuffixes);
			string expectedLastRole = "RIFLEMAN";
			if (reinforced) expectedLastRole = "ANTI_TANK";
			AICF_ATCheck("STOCK_DIFFICULTY_" + groupIndex, stockRole == expectedLastRole);
			baseline.BuildCharacterRoleCandidates(stable, 9, stockRole, stockSuffixes);
			AICF_ATCheck("PROFILE_DIFFICULTY_" + groupIndex, stockRole == expectedLastRole);
		}
		array<string> invalidSuffixes = {};
		string invalidRole;
		AICF_ATCheck("FIA_REJECTED", !AICF_ContentProfile.GetActive().BuildCharacterRoleCandidates("FIA", 8, invalidRole, invalidSuffixes));
		AICF_ATFinish();
	}
}
