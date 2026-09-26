// Нашивки штатных бойцов RHS: только свободные крепления, без смены экипировки.
// Совместимость и репликация предметов принадлежат native inventory manager.
class AICF_RHSDefaultPatches
{
	static const int PATCH_SET_COUNT = 8;
	protected static int s_iNextRussianPatchSet;
	protected static int s_iNextAmericanPatchSet;
	protected static int s_iNextPMCPatchSet;

	static int NextPatchSet(FactionKey faction)
	{
		int selected;
		switch (faction)
		{
			case "RHS_AFRF":
				selected = s_iNextRussianPatchSet;
				s_iNextRussianPatchSet = (selected + 1) % PATCH_SET_COUNT;
				break;
			case "RHS_USAF":
				selected = s_iNextAmericanPatchSet;
				s_iNextAmericanPatchSet = (selected + 1) % PATCH_SET_COUNT;
				break;
			case "FIA":
				selected = s_iNextPMCPatchSet;
				s_iNextPMCPatchSet = (selected + 1) % PATCH_SET_COUNT;
				break;
			default: return -1;
		}
		return selected;
	}

	static bool Eligible(SCR_ChimeraCharacter character)
	{
		if (!Replication.IsServer() || !character || character.GetWorld() != GetGame().GetWorld() ||
			!AICF_RHSContentProfile.Cast(AICF_ContentProfile.GetActive()))
			return false;
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		CharacterControllerComponent controller = character.GetCharacterController();
		if (!rpl || !rpl.IsMaster() || !controller || controller.IsDead())
			return false;
		ResourceName source = SCR_ResourceNameUtils.GetPrefabName(character);
		return (character.GetFactionKey() == "RHS_USAF" && source.Contains("/Factions/BLUFOR/RHS_USAF/")) ||
			(character.GetFactionKey() == "RHS_AFRF" && source.Contains("/Factions/OPFOR/RHS_AFRF/")) ||
			AICF_RHSPMCEquipment.Eligible(character);
	}

	static bool IsPatchSlot(InventoryStorageSlot slot)
	{
		if (!RHS_LoadoutSlotInfo.Cast(slot)) return false;
		string name = slot.GetSourceName();
		// TopChestVelctro — точное имя крепления в RHS 0.16.5208.
		return name.EndsWith("Velcro") || name == "TopChestVelctro";
	}

	static ResourceName Flag(FactionKey faction)
	{
		if (faction == "RHS_USAF")
			return "{EAC140671B326F58}Prefabs/Items/Equipment/Patches/Patch_US_Flag.et";
		if (faction == "RHS_AFRF")
			return "{7F43CA3BE4D42E42}Prefabs/Items/Equipment/Patches/Patch_RU_Flag_Camo.et";
		if (faction == "FIA")
			return "{CAFC3C48F7E009C3}Prefabs/Items/Equipment/Patches/Patch_ION_Black.et";
		return ResourceName.Empty;
	}

	static ResourceName RussianEmblem(int variant)
	{
		switch (variant % PATCH_SET_COUNT)
		{
			case 0: return "{FD09AB7A0E9D7E2A}Prefabs/Items/Equipment/Patches/Patch_Ars_Arma_Bear.et";
			case 1: return "{C55FF0BD292542E2}Prefabs/Items/Equipment/Patches/Patch_Ars_Arma_Cube.et";
			case 2: return "{984946A9BC987C2B}Prefabs/Items/Equipment/Patches/Patch_RU_Russia_Large.et";
			case 3: return "{2E2721DB0A4FB249}Prefabs/Items/Equipment/Patches/Patch_RU_Berlinskaya_Large.et";
			case 4: return "{E10AE7ACBF1DE0E0}Prefabs/Items/Equipment/Patches/Patch_Wartech.et";
			case 5: return "{49D40BCE978168E9}Prefabs/Items/Equipment/Patches/Patch_Wartech_square.et";
			case 6: return "{266C9240B1F0165F}Prefabs/Items/Equipment/Patches/Patch_RHS_20.et";
			case 7: return "{C3596C668C12A212}Prefabs/Items/Equipment/Patches/Patch_RHS_oldstyle.et";
		}
		return Flag("RHS_AFRF");
	}

	static ResourceName RussianFlag(int variant)
	{
		switch (variant % 4)
		{
			case 1: return "{140729DAE2388C8E}Prefabs/Items/Equipment/Patches/Patch_RU_Flag_Camo_multicam.et";
			case 2: return "{FEDC7DA627D89A65}Prefabs/Items/Equipment/Patches/Patch_RU_Flag_Reflective.et";
			case 3: return "{30C3A87F16109380}Prefabs/Items/Equipment/Patches/Patch_RU_Flag_Reflective_Orange.et";
		}
		return Flag("RHS_AFRF");
	}

	static ResourceName AmericanName(int variant)
	{
		switch (variant % 4)
		{
			case 1: return "{0D07F51A271B0065}Prefabs/Items/Equipment/Patches/Patch_USMC_ID_Bernat.et";
			case 2: return "{8A3F4C98645988CF}Prefabs/Items/Equipment/Patches/Patch_USMC_ID_Kita.et";
			case 3: return "{39C417FDEC69C8A4}Prefabs/Items/Equipment/Patches/Patch_USMC_ID_Vorobiev.et";
		}
		return "{37195C9B2FFB3935}Prefabs/Items/Equipment/Patches/Patch_USMC_ID_Anderson.et";
	}

	static ResourceName PMCEmblem(int variant)
	{
		switch (variant % PATCH_SET_COUNT)
		{
			case 0: return "{FD09AB7A0E9D7E2A}Prefabs/Items/Equipment/Patches/Patch_Ars_Arma_Bear.et";
			case 1: return "{C55FF0BD292542E2}Prefabs/Items/Equipment/Patches/Patch_Ars_Arma_Cube.et";
			case 2: return "{4B799DDBF2CDF294}Prefabs/Items/Equipment/Patches/Patch_Cat_Blue.et";
			case 3: return "{2F40976E11AD82CA}Prefabs/Items/Equipment/Patches/Patch_Cat_Green.et";
			case 4: return "{1DC57209E16BCD52}Prefabs/Items/Equipment/Patches/Patch_Kapitan_Bomba.et";
			case 5: return "{E10AE7ACBF1DE0E0}Prefabs/Items/Equipment/Patches/Patch_Wartech.et";
			case 6: return "{49D40BCE978168E9}Prefabs/Items/Equipment/Patches/Patch_Wartech_square.et";
			case 7: return "{266C9240B1F0165F}Prefabs/Items/Equipment/Patches/Patch_RHS_20.et";
		}
		return Flag("FIA");
	}

	static ResourceName Preferred(FactionKey faction, ResourceName source, string slotName, int patchSet)
	{
		bool medic = source.Contains("_Medic");
		if (medic && (slotName == "LeftVelcro" || slotName == "SleeveLeftVelcro" || slotName == "TopChestVelctro"))
			return "{B9E98F1464E93A9D}Prefabs/Items/Equipment/Patches/Patch_Blood_APos.et";
		if (faction == "RHS_USAF")
		{
			if (slotName == "RightVelcro" || slotName == "SleeveRightVelcro")
				return "{7122907EFFE9ABD1}Prefabs/Items/Equipment/Patches/Patch_US_Flag_Inverse.et";
			if (slotName == "BackVelcro") return AmericanName(patchSet);
			if (slotName == "ChestVelcro" || slotName == "TopChestVelctro" || slotName == "SleeveLeftVelcro")
			{
				if (patchSet < 4) return "{46822E82DB8151A4}Prefabs/Items/Equipment/Patches/Patch_USMC_Recon.et";
				return "{4135547E507D4471}Prefabs/Items/Equipment/Patches/Patch_USMC_Recon_Black.et";
			}
		}
		else if (faction == "RHS_AFRF")
		{
			// Флаг на левом рукаве, отличительная эмблема справа. Рюкзак и
			// грудь получают другую эмблему того же закреплённого комплекта.
			if (slotName == "SleeveRightVelcro" || slotName == "BackVelcro")
				return RussianEmblem(patchSet);
			if (slotName == "RightVelcro" || slotName == "ChestVelcro" || slotName == "TopChestVelctro")
				return RussianEmblem(patchSet + 3);
			return RussianFlag(patchSet);
		}
		else if (faction == "FIA")
		{
			if (slotName == "SleeveRightVelcro" || slotName == "RightVelcro") return PMCEmblem(patchSet);
			if (slotName == "ChestVelcro" || slotName == "TopChestVelctro" || slotName == "BackVelcro")
				return PMCEmblem(patchSet + 3);
		}
		return Flag(faction);
	}

	static void Apply(SCR_ChimeraCharacter character)
	{
		if (!Eligible(character)) return;
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!manager) return;
		EntityID identity = character.GetID();
		ResourceName source = SCR_ResourceNameUtils.GetPrefabName(character);
		FactionKey faction = character.GetFactionKey();
		int patchSet = character.AICF_GetPatchSet();
		if (patchSet < 0) return;
		array<BaseInventoryStorageComponent> storages = {};
		manager.GetStorages(storages);
		int supported;
		int added;
		int retained;
		int unavailable;
		int failed;
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage) continue;
			for (int i; i < storage.GetSlotsCount(); i++)
			{
				InventoryStorageSlot slot = storage.GetSlot(i);
				if (!IsPatchSlot(slot)) continue;
				supported++;
				if (slot.GetAttachedEntity())
				{
					retained++;
					continue;
				}
				ResourceName patch = Preferred(faction, source, slot.GetSourceName(), patchSet);
				if (!manager.CanInsertResourceInStorage(patch, storage, i)) patch = Flag(faction);
				// Некоторые унаследованные Velcro в RHS отключены либо закрыты
				// подсумками. Их нельзя заполнять в обход native admission.
				if (slot.IsLocked() || !manager.CanInsertResourceInStorage(patch, storage, i))
				{
					unavailable++;
					continue;
				}
				// Повторная проверка непосредственно перед единственным spawn.
				if (!Eligible(character) || character.GetID() != identity ||
					SCR_ResourceNameUtils.GetPrefabName(character) != source || character.GetFactionKey() != faction)
					return;
				if (!slot.IsLocked() && !slot.GetAttachedEntity() && manager.CanInsertResourceInStorage(patch, storage, i) &&
					manager.TrySpawnPrefabToStorage(patch, storage, i) && slot.GetAttachedEntity() &&
					SCR_ResourceNameUtils.GetPrefabName(slot.GetAttachedEntity()) == patch)
					added++;
				else
					failed++;
			}
		}
		Print(string.Format("[AICF][RHS_PATCHES] entity=%1 faction=%2 supported=%3 added=%4 retained=%5 unavailable=%6 failed=%7 prefab=%8 set=%9", identity, faction, supported, added, retained, unavailable, failed, source, patchSet));
	}
}

modded class SCR_ChimeraCharacter
{
	protected EntityID m_iAICFPatchIdentity;
	protected ResourceName m_sAICFPatchSource;
	protected int m_iAICFPatchSet = -1;

	int AICF_GetPatchSet()
	{
		if (!AICF_RHSDefaultPatches.Eligible(this)) return -1;
		// Индивидуальный набор назначается один раз на authority. Клиент/JIP
		// получает реальные patch entities через native inventory replication.
		if (m_iAICFPatchSet < 0)
			m_iAICFPatchSet = AICF_RHSDefaultPatches.NextPatchSet(GetFactionKey());
		return m_iAICFPatchSet;
	}

	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (!Replication.IsServer() || GetWorld() != GetGame().GetWorld()) return;
		ResourceName source = SCR_ResourceNameUtils.GetPrefabName(this);
		if (!source.Contains("/Factions/BLUFOR/RHS_USAF/") && !source.Contains("/Factions/OPFOR/RHS_AFRF/")) return;
		m_iAICFPatchIdentity = GetID();
		m_sAICFPatchSource = source;
		GetGame().GetCallqueue().CallLater(AICF_ApplyDefaultPatches, 1500, false);
	}

	protected void AICF_ApplyDefaultPatches()
	{
		if (GetID() == m_iAICFPatchIdentity && SCR_ResourceNameUtils.GetPrefabName(this) == m_sAICFPatchSource)
			AICF_RHSDefaultPatches.Apply(this);
	}

	void ~SCR_ChimeraCharacter()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_ApplyDefaultPatches);
	}
}
