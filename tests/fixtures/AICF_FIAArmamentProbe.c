// Только stage. Общая stock/RHS матрица через настоящие initialization hooks.
modded class SCR_GameModeCampaign
{
	protected ref array<SCR_ChimeraCharacter> m_aARMCharacters = {};
	protected ref array<string> m_aARMBefore = {};
	protected ref array<int> m_aARMVariants = {};
	protected int m_iARMChecks;
	protected int m_iARMFailures;

	protected void AICF_ARMCheck(string name, bool passed, string detail = "")
	{
		m_iARMChecks++;
		if (!passed) m_iARMFailures++;
		Print(string.Format("[AICF][FIA_ARMAMENT_CASE] case=%1 passed=%2 %3", name, passed, detail));
	}

	// Полный weapon/ammo readback, без случайной внешности и entity IDs.
	protected string AICF_ARMInventory(SCR_ChimeraCharacter character, bool secondaryOnly = false)
	{
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		array<IEntity> items = {};
		manager.GetItems(items);
		array<string> rows = {};
		foreach (IEntity item : items)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (secondaryOnly)
			{
				if (!weapon || weapon.GetWeaponType() == EWeaponType.WT_RIFLE || weapon.GetWeaponType() == EWeaponType.WT_MACHINEGUN || weapon.GetWeaponType() == EWeaponType.WT_SNIPERRIFLE) continue;
			}
			if (!weapon && !magazine) continue;
			int ammo = -1;
			if (magazine) ammo = magazine.GetAmmoCount();
			if (weapon && weapon.GetCurrentMuzzle()) ammo = weapon.GetCurrentMuzzle().GetAmmoCount();
			rows.Insert(SCR_ResourceNameUtils.GetPrefabName(item) + ":" + ammo.ToString());
		}
		rows.Sort();
		return SCR_StringHelper.Join(";", rows);
	}

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (!Replication.IsServer() || !System.GetCLIParam("aicfFIAArmamentProbe", enabled)) return;
		GetGame().GetCallqueue().CallLater(AICF_ARMSpawn, 5000, false);
		GetGame().GetCallqueue().CallLater(AICF_ARMFinish, 35000, false);
	}

	protected int AICF_ARMCount(ResourceName source) { return 1; }
	protected void AICF_ARMSpawned(SCR_ChimeraCharacter character, int variant) {}

	protected void AICF_ARMSpawnOne(ResourceName prefab, int variant)
	{
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		GetBaseManager().GetBases(bases);
		if (bases.IsEmpty()) return;
		EntitySpawnParams spawn = new EntitySpawnParams();
		spawn.TransformMode = ETransformMode.WORLD;
		vector position = bases[0].GetOwner().GetOrigin() + Vector(30 + m_aARMCharacters.Count() * 2, 0, 30);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.1;
		spawn.Transform[3] = position;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), null, spawn));
		AICF_ARMCheck("SPAWN", character != null, "source=" + prefab);
		if (!character) return;
		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (control) control.DeactivateAI();
		DamageManagerComponent damage = DamageManagerComponent.Cast(character.FindComponent(DamageManagerComponent));
		if (damage) damage.EnableDamageHandling(false);
		m_aARMCharacters.Insert(character);
		m_aARMVariants.Insert(variant);
		m_aARMBefore.Insert(AICF_ARMInventory(character));
		AICF_ARMSpawned(character, variant);
	}

	protected void AICF_ARMSpawn()
	{
		SCR_Faction fia = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey("FIA"));
		array<SCR_EntityCatalogEntry> entries = {};
		fia.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER).GetEntityList(entries);
		array<string> roles = {"Rifleman", "SL", "Medic", "MG", "AMG", "LAT", "AT", "AAT", "Sharpshooter", "RTO", "Sapper", "Ammo", "Scout"};
		foreach (string role : roles)
		{
			bool found;
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				ResourceName source = entry.GetPrefab();
				if (!source.EndsWith("/Character_FIA_" + role + ".et")) continue;
				found = true;
				for (int variant; variant < AICF_ARMCount(source); variant++) AICF_ARMSpawnOne(source, variant);
				break;
			}
			AICF_ARMCheck("ROLE_FOUND", found, "role=" + role);
		}
		array<ResourceName> ac = {
			"{273F0239FE94FDA5}Prefabs/Characters/Factions/INDFOR/FIA/Special Units/Character_FIA_AC_Partisan_Grenadier.et",
			"{4E29194BA809DF32}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_AC_Medic.et",
			"{B4977616CD19191A}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_AC_Scout.et"
		};
		foreach (ResourceName acSource : ac)
		{
			for (int acVariant; acVariant < AICF_ARMCount(acSource); acVariant++) AICF_ARMSpawnOne(acSource, acVariant);
		}
		AICF_ARMExtra();
	}

	protected void AICF_ARMExtra() {}
	protected void AICF_ARMExtraChecks() {}

	protected void AICF_ARMVerify(SCR_ChimeraCharacter character, int index)
	{
		string actual = AICF_ARMInventory(character);
		AICF_ARMCheck("STOCK_UNCHANGED", actual == m_aARMBefore[index], "source=" + SCR_ResourceNameUtils.GetPrefabName(character) + " inventory=" + actual);
	}

	protected void AICF_ARMFinish()
	{
		foreach (int index, SCR_ChimeraCharacter character : m_aARMCharacters) AICF_ARMVerify(character, index);
		AICF_ARMExtraChecks();
		Print(string.Format("[AICF][FIA_ARMAMENT_FINISHED] checks=%1 failures=%2 models=%3 profile=%4", m_iARMChecks, m_iARMFailures, m_aARMCharacters.Count(), AICF_ContentProfile.GetActive().GetProfileKey()));
		GetGame().RequestClose();
	}

	override void OnGameEnd()
	{
		GetGame().GetCallqueue().Remove(AICF_ARMSpawn);
		GetGame().GetCallqueue().Remove(AICF_ARMFinish);
		m_aARMCharacters.Clear();
		super.OnGameEnd();
	}
}
