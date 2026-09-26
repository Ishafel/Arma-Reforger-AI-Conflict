// Нашивки штатных бойцов RHS: только свободные крепления, без смены экипировки.
// Совместимость и репликация предметов принадлежат native inventory manager.
class AICF_RHSDefaultPatches
{
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
			(character.GetFactionKey() == "RHS_AFRF" && source.Contains("/Factions/OPFOR/RHS_AFRF/"));
	}

	static bool IsPatchSlot(InventoryStorageSlot slot)
	{
		if (!RHS_LoadoutSlotInfo.Cast(slot)) return false;
		string name = slot.GetSourceName();
		// TopChestVelctro — точное имя крепления в RHS 0.16.5208.
		return name.EndsWith("Velcro") || name == "TopChestVelctro";
	}

	static int Variant(ResourceName source)
	{
		if (source.Contains("_Medic") || source.Contains("_AMG")) return 1;
		if (source.Contains("_MG") || source.Contains("_AT") || source.Contains("_LAT") || source.Contains("_AAT")) return 2;
		if (source.Contains("_GL") || source.Contains("_AR") || source.Contains("_AAR")) return 3;
		return 0;
	}

	static ResourceName Flag(FactionKey faction)
	{
		if (faction == "RHS_USAF")
			return "{EAC140671B326F58}Prefabs/Items/Equipment/Patches/Patch_US_Flag.et";
		if (faction == "RHS_AFRF")
			return "{7F43CA3BE4D42E42}Prefabs/Items/Equipment/Patches/Patch_RU_Flag_Camo.et";
		return ResourceName.Empty;
	}

	static ResourceName Preferred(FactionKey faction, ResourceName source, string slotName)
	{
		int variant = Variant(source);
		bool medic = source.Contains("_Medic");
		if (medic && (slotName == "LeftVelcro" || slotName == "SleeveLeftVelcro" || slotName == "TopChestVelctro"))
			return "{B9E98F1464E93A9D}Prefabs/Items/Equipment/Patches/Patch_Blood_APos.et";
		if (faction == "RHS_USAF")
		{
			if (slotName == "RightVelcro" || slotName == "SleeveRightVelcro")
				return "{7122907EFFE9ABD1}Prefabs/Items/Equipment/Patches/Patch_US_Flag_Inverse.et";
			if (slotName == "ChestVelcro" || slotName == "BackVelcro")
			{
				switch (variant)
				{
					case 1: return "{0D07F51A271B0065}Prefabs/Items/Equipment/Patches/Patch_USMC_ID_Bernat.et";
					case 2: return "{8A3F4C98645988CF}Prefabs/Items/Equipment/Patches/Patch_USMC_ID_Kita.et";
					case 3: return "{39C417FDEC69C8A4}Prefabs/Items/Equipment/Patches/Patch_USMC_ID_Vorobiev.et";
				}
				return "{37195C9B2FFB3935}Prefabs/Items/Equipment/Patches/Patch_USMC_ID_Anderson.et";
			}
		}
		else if (faction == "RHS_AFRF")
		{
			if (slotName == "ChestVelcro")
				return "{984946A9BC987C2B}Prefabs/Items/Equipment/Patches/Patch_RU_Russia_Large.et";
			if (slotName == "SleeveRightVelcro" || slotName == "RightVelcro" || slotName == "BackVelcro")
			{
				if (variant == 2)
					return "{30C3A87F16109380}Prefabs/Items/Equipment/Patches/Patch_RU_Flag_Reflective_Orange.et";
				return "{FEDC7DA627D89A65}Prefabs/Items/Equipment/Patches/Patch_RU_Flag_Reflective.et";
			}
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
				ResourceName patch = Preferred(faction, source, slot.GetSourceName());
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
		Print(string.Format("[AICF][RHS_PATCHES] entity=%1 faction=%2 supported=%3 added=%4 retained=%5 unavailable=%6 failed=%7 prefab=%8", identity, faction, supported, added, retained, unavailable, failed, source));
	}
}

modded class SCR_ChimeraCharacter
{
	protected EntityID m_iAICFPatchIdentity;
	protected ResourceName m_sAICFPatchSource;

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
