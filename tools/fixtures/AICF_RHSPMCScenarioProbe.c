// Только изолированная test-копия RHS-addon. Не включать в production.
// Проверяет AC-охрану без исходного primary и обычного FIA стрелка.
modded class SCR_GameModeCampaign
{
	protected ref array<SCR_ChimeraCharacter> m_aAICFPMCProbeCharacters = {};
	protected ref array<ResourceName> m_aAICFPMCProbePrefabs = {
		"{273F0239FE94FDA5}Prefabs/Characters/Factions/INDFOR/FIA/Special Units/Character_FIA_AC_Partisan_Grenadier.et",
		"{4E29194BA809DF32}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_AC_Medic.et",
		"{B4977616CD19191A}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_AC_Scout.et",
		"{84B40583F4D1B7A3}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et"
	};

	override void OnGameStart()
	{
		super.OnGameStart();
		if (!Replication.IsServer()) return;
		GetGame().GetCallqueue().CallLater(AICF_PMCProbeSpawn, 5000, false);
		GetGame().GetCallqueue().CallLater(AICF_PMCProbeFinish, 20000, false);
	}

	protected void AICF_PMCProbeSpawn()
	{
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		GetBaseManager().GetBases(bases);
		if (bases.IsEmpty()) return;
		SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		SCR_Faction fia = SCR_Faction.Cast(factions.GetFactionByKey("FIA"));
		factions.SetFactionsFriendly(fia, SCR_Faction.Cast(factions.GetFactionByKey("RHS_USAF")));
		factions.SetFactionsFriendly(fia, SCR_Faction.Cast(factions.GetFactionByKey("RHS_AFRF")));
		foreach (int index, ResourceName prefab : m_aAICFPMCProbePrefabs)
		{
			EntitySpawnParams params();
			params.TransformMode = ETransformMode.WORLD;
			vector position = bases[0].GetOwner().GetOrigin() + "30 0 30";
			position[0] = position[0] + index * 3;
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			params.Transform[3] = position;
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
			m_aAICFPMCProbeCharacters.Insert(character);
		}
	}

	protected void AICF_PMCProbeFinish()
	{
		int passed;
		foreach (int index, SCR_ChimeraCharacter character : m_aAICFPMCProbeCharacters)
		{
			ResourceName source = m_aAICFPMCProbePrefabs[index];
			bool valid = character && SCR_ResourceNameUtils.GetPrefabName(character) == source &&
				AICF_RHSPMCEquipment.Eligible(character) && !character.AICF_IsPMCEquipmentPending() &&
				AICF_RHSPMCArmament.Validate(character, source, AICF_RHSPMCEquipment.Variant(source));
			if (valid) passed++;
			Print(string.Format("[AICF][PMC_SCENARIO_CASE] source=%1 passed=%2", source, valid));
		}
		Print(string.Format("[AICF][PMC_SCENARIO_RESULT] expected=4 passed=%1", passed));
		GetGame().RequestClose();
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_PMCProbeSpawn);
		GetGame().GetCallqueue().Remove(AICF_PMCProbeFinish);
		m_aAICFPMCProbeCharacters.Clear();
		super.OnGameEnd();
	}
}
