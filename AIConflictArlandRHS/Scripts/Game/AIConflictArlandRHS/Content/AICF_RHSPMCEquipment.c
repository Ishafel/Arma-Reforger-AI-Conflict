// Нейтральная экипировка охраны FIA только в RHS Conflict.
// Сначала собирается локальный inventory draft; живой боец меняется целиком
// после проверки вместимости. Фракция, лицо и специализация остаются прежними.
class AICF_RHSPMCEquipmentDraft : AICF_LoadoutDraft
{
	bool BuildEquipment(ResourceName source)
	{
		Clear();
		m_World = BaseWorld.CreateWorld("ChimeraWorld", "AICF_PMC_" + (++s_iWorldSequence).ToString());
		if (!m_World || !m_World.IsValid()) return false;
		ChimeraWorld world = m_World.GetRef();
		world.LoadSystems("{3A1DEDB2A5818B53}Configs/Systems/MapPreviewSystemsConfig.conf");
		Resource prefab = Resource.Load(source);
		Resource preview = Resource.Load("{9F18C476AB860F3B}Prefabs/World/Game/ItemPreviewManager.et");
		if (!prefab || !prefab.IsValid() || !preview || !preview.IsValid()) return false;
		m_Preview = ItemPreviewManagerEntity.Cast(GetGame().SpawnEntityPrefabLocal(preview, world));
		if (!m_Preview) return false;
		m_Character = m_Preview.ResolvePreviewEntityForPrefab(source);
		// У server preview нет сетевой регистрации. Не вызываем damage RPC
		// общего UI draft; этот локальный мир не участвует в симуляции боя.
		return m_Character && m_Character.GetWorld() != GetGame().GetWorld();
	}
}

class AICF_RHSPMCEquipment
{
	static bool Eligible(SCR_ChimeraCharacter character)
	{
		if (!Replication.IsServer() || !character || character.GetWorld() != GetGame().GetWorld() ||
			!AICF_RHSContentProfile.Cast(AICF_ContentProfile.GetActive()) || character.GetFactionKey() != "FIA")
			return false;
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		CharacterControllerComponent controller = character.GetCharacterController();
		return rpl && rpl.IsMaster() && controller && !controller.IsPlayerControlled() &&
			!controller.IsDead() && SCR_ResourceNameUtils.GetPrefabName(character).Contains("/FIA/");
	}

	static int Variant(ResourceName source)
	{
		if (source.Contains("_Medic") || source.Contains("_AMG") || source.Contains("_Scout"))
			return 1;
		if (source.Contains("_MG") || source.Contains("_AT") || source.Contains("_SL") || source.Contains("_Sharpshooter"))
			return 2;
		return 0;
	}

	static void Clothes(int variant, array<ResourceName> clothes, bool radioBackpack)
	{
		if (variant == 1)
			clothes.Insert("{883CC56ECDAA76EB}Prefabs/Characters/Uniforms/Crye_Shirt/Jacket_Crye_Combat_Shirt_Rolled_CoyoteBrown.et");
		else if (variant == 2)
			clothes.Insert("{E95480D14AEE3EF4}Prefabs/Characters/Uniforms/Crye_Shirt/Jacket_Crye_Combat_Shirt_Rolled_mcblack.et");
		else
			clothes.Insert("{82C659FB6237E828}Prefabs/Characters/Uniforms/Crye_Shirt/Jacket_Crye_Combat_Shirt_OD.et");
		if (variant == 1)
			clothes.Insert("{7BDF3DD1CB3730A3}Prefabs/Characters/Uniforms/Pants_CP_G3/CP_G3_Pants_KpadsG3_MC.et");
		else
			clothes.Insert("{0431EA00E8B1CA6A}Prefabs/Characters/Uniforms/Pants_CP_G3/MC_BLK_camo/CP_G3_Pants_KpadsG3_MC_BLK.et");
		// Совместимая пара из штатного ION Medic: AVS + отдельный пояс.
		// Вариант Nobelt имеет ZipOnPanel, конфликтующий с отдельным рюкзаком.
		clothes.Insert("{D4ACFC8F94EDE501}Prefabs/Characters/Vests/Vest_AVS/Vest_AVS_radio_MCBlk.et");
		clothes.Insert("{9FB6D7969BB672AD}Prefabs/Characters/Vests/Vest_AVS/Variants/Vest_AVS_MCBlk_ronin.et");
		clothes.Insert("{9E45240EDBE0C963}Prefabs/Characters/HeadGear/Helmet_OPSCORE/Helmet_OPSCORE_SF_BLK_AMP.et");
		clothes.Insert("{CB379AD8EC48690C}Prefabs/Characters/HeadGear/Balaclavas/Balaclava_v1/Balaclava_v1_BLK.et");
		clothes.Insert("{608F66AB8B36B85D}Prefabs/Characters/Eyewear/crossbow/Eyewear_ess_crossbow_blk.et");
		clothes.Insert("{057E63449502EBCA}Prefabs/Characters/Footwear/Boots_Salomon/Footwear_Salomon.et");
		clothes.Insert("{1E991CD19C6F7659}Prefabs/Characters/Handwear/Gloves_MechanixMpact/Gloves_mpack_Brown.et");
		if (!radioBackpack)
			clothes.Insert("{DF886C6B22F49827}Prefabs/Items/Equipment/Backpacks/backpack_511_rush12/Backpack_511_rush12_black.et");
	}

	protected static void CollectCargo(IEntity entity, array<IEntity> cargo, int depth = 0)
	{
		if (!entity || depth > 6) return;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item.GetOwner());
				bool gadget = prefab.Contains("/Items/Equipment/") && !prefab.Contains("/Backpacks/");
				// Фонарь/лопатка могут висеть на gadget slot прежней одежды.
				// Их надо снять до удаления родительской storage, как содержимое кармана.
				bool deposit = (storage.GetPurpose() & EStoragePurpose.PURPOSE_DEPOSIT) && !EquipedLoadoutStorageComponent.Cast(storage);
				if (depth > 0 && (deposit || gadget) &&
					(gadget || !item.GetOwner().FindComponent(BaseLoadoutClothComponent)))
				{
					if (!cargo.Contains(item.GetOwner())) cargo.Insert(item.GetOwner());
				}
				else
					CollectCargo(item.GetOwner(), cargo, depth + 1);
			}
		}
	}

	protected static bool StoreLocal(IEntity entity, IEntity payload, InventoryStorageManagerComponent manager, int depth = 0)
	{
		if (!entity || depth > 6) return false;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (depth > 0 && (storage.GetPurpose() & EStoragePurpose.PURPOSE_DEPOSIT) &&
				!EquipedLoadoutStorageComponent.Cast(storage) && manager.CanInsertItemInStorage(payload, storage))
			{
				InventoryStorageSlot target = storage.FindSuitableSlotForItem(payload);
				if (target && !target.GetAttachedEntity())
				{
					target.AttachEntity(payload);
					if (target.GetAttachedEntity() == payload) return true;
				}
			}
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				if (item.GetOwner() != payload && StoreLocal(item.GetOwner(), payload, manager, depth + 1)) return true;
			}
		}
		return false;
	}

	protected static bool DressLocal(IEntity model, int variant, ResourceName magazine, int addMagazines, ResourceName source)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
		if (!manager) return false;
		// Preview inventory не регистрирует storages в runtime manager.
		// Обходим реальные компоненты дерева, как штатный inventory serializer.
		array<IEntity> retained = {};
		CollectCargo(model, retained);
		foreach (IEntity payload : retained)
		{
			InventoryItemComponent component = InventoryItemComponent.Cast(payload.FindComponent(InventoryItemComponent));
			if (!component || !component.GetParentSlot()) return false;
			component.GetParentSlot().DetachEntity();
			if (component.GetParentSlot()) return false;
		}
		set<BaseInventoryStorageComponent> roots = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(model, roots);
		bool radioBackpack;
		foreach (BaseInventoryStorageComponent root : roots)
		{
			array<InventoryItemComponent> worn = {};
			root.GetOwnedItems(worn, false);
			foreach (InventoryItemComponent old : worn)
			{
				BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(old.GetOwner().FindComponent(BaseLoadoutClothComponent));
				ResourceName oldPrefab = SCR_ResourceNameUtils.GetPrefabName(old.GetOwner());
				// Часы, фонарь, бинокль, лопатка и ранцевая рация тоже имеют
				// BaseLoadoutClothComponent. Сохраняем их в штатных gadget slots.
				if (cloth && oldPrefab.Contains("/Items/Equipment/") && !oldPrefab.Contains("/Backpacks/"))
				{
					if (old.GetOwner().FindComponent(BaseRadioComponent)) radioBackpack = true;
					continue;
				}
				if (cloth && !AICF_LoadoutInventory.DeleteLocal(old.GetOwner())) return false;
			}
		}
		array<ResourceName> clothes = {};
		Clothes(variant, clothes, radioBackpack);
		foreach (ResourceName prefab : clothes)
		{
			if (!EquipLocal(model, prefab, manager))
			{
				Print("[AICF][PMC_DRAFT_FAILED] clothing=" + prefab, LogLevel.WARNING);
				return false;
			}
		}
		foreach (IEntity cargo : retained)
		{
			if (!StoreLocal(model, cargo, manager))
			{
				Print("[AICF][PMC_DRAFT_FAILED] cargo=" + SCR_ResourceNameUtils.GetPrefabName(cargo), LogLevel.WARNING);
				return false;
			}
		}
		if (!AddMagazinesLocal(model, manager, magazine, addMagazines)) return false;
		if (source.EndsWith("_AMG.et") && !AddMagazinesLocal(model, manager, AICF_RHSPMCArmament.SupportMagazinePrefab(), 3)) return false;
		return true;
	}

	protected static bool AddMagazinesLocal(IEntity model, InventoryStorageManagerComponent manager, ResourceName magazine, int addMagazines)
	{
		if (!model || model.GetWorld() == GetGame().GetWorld()) return false;
		Resource magazineResource = Resource.Load(magazine);
		if (!magazineResource || !magazineResource.IsValid()) return false;
		for (int magIndex; magIndex < addMagazines; magIndex++)
		{
			IEntity spare = GetGame().SpawnEntityPrefabLocal(magazineResource, model.GetWorld());
			if (!spare) return false;
			if (!StoreLocal(model, spare, manager))
			{
				AICF_LoadoutInventory.DeleteLocal(spare);
				Print(string.Format("[AICF][PMC_DRAFT_FAILED] reason=MAGAZINE_SPACE index=%1 prefab=%2", magIndex, magazine), LogLevel.WARNING);
				return false;
			}
		}
		return true;
	}

	protected static bool EquipLocal(IEntity entity, ResourceName prefab, InventoryStorageManagerComponent manager)
	{
		if (!entity || entity.GetWorld() == GetGame().GetWorld()) return false;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid()) return false;
		IEntity preview = GetGame().SpawnEntityPrefabLocal(resource, entity.GetWorld());
		if (!preview) return false;
		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(preview.FindComponent(BaseLoadoutClothComponent));
		typename area;
		if (cloth && cloth.GetAreaType()) area = cloth.GetAreaType().Type();
		AICF_LoadoutInventory.DeleteLocal(preview);
		// Вторичная blocked area не является местом надевания. Иначе маска
		// может попасть в слот очков, даже когда native CanInsert возвращает true.
		bool equipped = area != typename.Empty && EquipLocalInArea(entity, prefab, manager, area);
		if (!equipped)
			Print(string.Format("[AICF][PMC_CLOTHING_SLOT_FAILED] prefab=%1 area=%2", prefab, area), LogLevel.WARNING);
		return equipped;
	}

	protected static bool EquipLocalInArea(IEntity entity, ResourceName prefab, InventoryStorageManagerComponent manager, typename area, int depth = 0)
	{
		if (!entity || depth > 6) return false;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			for (int slot; slot < storage.GetSlotsCount(); slot++)
			{
				// RHS рюкзак крепится к бронежилету. Не кладём одежду в карман.
				typename slotArea = AICF_LoadoutClothing.Area(storage, slot);
				if (slotArea != typename.Empty && area.IsInherited(slotArea) && !storage.Get(slot) &&
					AICF_LoadoutInventory.InsertLocal(prefab, storage, slot, manager)) return true;
			}
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				if (EquipLocalInArea(item.GetOwner(), prefab, manager, area, depth + 1)) return true;
			}
		}
		return false;
	}
	static bool Apply(SCR_ChimeraCharacter character)
	{
		if (!Eligible(character))
			return false;
		EntityID identity = character.GetID();
		ResourceName source = SCR_ResourceNameUtils.GetPrefabName(character);
		string before;
		if (!AICF_LoadoutInventory.Capture(character, before))
			return false;
		SCR_CampaignFaction catalogFaction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey("RHS_AFRF"));
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(catalogFaction);
		AICF_RHSPMCEquipmentDraft draft = new AICF_RHSPMCEquipmentDraft();
		string snapshot;
		int variant = Variant(source);
		ResourceName magazine;
		int addMagazines;
		bool built = draft.BuildEquipment(source) && AICF_LoadoutInventory.Restore(draft.GetCharacter(), before);
		bool armed = built && AICF_RHSPMCArmament.PrepareLocal(draft.GetCharacter(), source, variant, magazine, addMagazines);
		bool dressed = armed && DressLocal(draft.GetCharacter(), variant, magazine, addMagazines, source);
		bool ready = dressed && AICF_LoadoutInventory.Capture(draft.GetCharacter(), snapshot);
		int ignored;
		string expected;
		if (ready)
			expected = AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, ignored);
		draft.Clear();
		if (!ready || expected.IsEmpty())
		{
			Print(string.Format("[AICF][PMC_DRAFT_FAILED] entity=%1 source=%2 built=%3 armed=%4 dressed=%5 captured=%6", identity, source, built, armed, dressed, ready), LogLevel.WARNING);
			return false;
		}
		if (!Eligible(character) || character.GetID() != identity || SCR_ResourceNameUtils.GetPrefabName(character) != source)
			return false;
		bool applied = AICF_LoadoutInventory.Restore(character, snapshot) &&
			AICF_LoadoutInventory.Signature(character, catalog, ignored) == expected &&
			AICF_RHSPMCArmament.Validate(character, source, variant);
		if (!applied)
		{
			bool restored = AICF_LoadoutInventory.Restore(character, before);
			Print(string.Format("[AICF][PMC_EQUIPMENT_FAILED] entity=%1 rollback=%2", identity, restored), LogLevel.ERROR);
			return false;
		}
		Print(string.Format("[AICF][PMC_EQUIPMENT_APPLIED] entity=%1 faction=FIA variant=%2 source=%3 inventory_verified=1", identity, variant, source));
		return true;
	}
}

modded class SCR_ChimeraCharacter
{
	protected EntityID m_iAICFPMCIdentity;

	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (!Replication.IsServer() || GetWorld() != GetGame().GetWorld() || !SCR_ResourceNameUtils.GetPrefabName(this).Contains("/FIA/"))
			return;
		m_iAICFPMCIdentity = GetID();
		GetGame().GetCallqueue().CallLater(AICF_ApplyPMCEquipment, 1500, false);
	}

	protected void AICF_ApplyPMCEquipment()
	{
		if (GetID() == m_iAICFPMCIdentity)
			AICF_RHSPMCEquipment.Apply(this);
	}

	void ~SCR_ChimeraCharacter()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AICF_ApplyPMCEquipment);
	}
}
