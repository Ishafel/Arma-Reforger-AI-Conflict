// Назначение слота берётся из native LoadoutArea, а не из занятого места:
// пустой слот шлема продолжает предлагать головные уборы.
class AICF_LoadoutSlotView
{
	static bool HasFixedSlots(BaseInventoryStorageComponent storage)
	{
		if (!storage)
			return false;
		if (WeaponAttachmentsStorageComponent.Cast(storage))
			return true;
		for (int i; i < storage.GetSlotsCount(); i++)
		{
			if (LoadoutSlotInfo.Cast(storage.GetSlot(i)))
				return true;
		}
		return false;
	}

	static void AttachmentCatalog(BaseInventoryStorageComponent storage, int slot, out int category, out int mode)
	{
		category = SCR_EArsenalItemType.WEAPON_ATTACHMENT;
		mode = 0;
		// Крепления одежды RHS имеют собственные arsenal types. Допустимые
		// предметы определяет native слот, без зависимости Core от RHS classes.
		if (!WeaponAttachmentsStorageComponent.Cast(storage))
		{
			string label;
			category = Describe(storage, slot, label);
			return;
		}
		if (storage && slot >= 0 && slot < storage.GetSlotsCount() &&
			BaseMuzzleComponent.Cast(storage.GetSlot(slot).GetParentContainer()))
		{
			category = 0;
			mode = SCR_EArsenalItemMode.AMMUNITION;
		}
	}

	static void WeaponCatalog(string type, out int category, out int mode)
	{
		category = 0;
		mode = SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS;
		if (type == "grenade")
		{
			category = SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE;
			mode = 0;
		}
	}

	static WeaponSlotComponent WeaponSlot(BaseInventoryStorageComponent storage, int slot)
	{
		if (!EquipedWeaponStorageComponent.Cast(storage) || slot < 0 || slot >= storage.GetSlotsCount())
			return null;
		array<Managed> components = {};
		storage.GetOwner().FindComponents(WeaponSlotComponent, components);
		foreach (Managed component : components)
		{
			WeaponSlotComponent weapon = WeaponSlotComponent.Cast(component);
			if (weapon && weapon.GetSlotInfo() == storage.GetSlot(slot))
				return weapon;
		}
		return null;
	}

	static bool VisibleSlot(BaseInventoryStorageComponent storage, int slot)
	{
		if (!storage || slot < 0 || slot >= storage.GetSlotsCount())
			return false;
		if (WeaponAttachmentsStorageComponent.Cast(storage))
		{
			AttachmentSlotComponent attachment = AttachmentSlotComponent.Cast(storage.GetSlot(slot).GetParentContainer());
			if (attachment)
				return attachment.ShouldShowInInspection();
		}
		if (!EquipedWeaponStorageComponent.Cast(storage))
			return true;
		WeaponSlotComponent weapon = WeaponSlot(storage, slot);
		// Те же пользовательские места, что у stock Inventory20. Дополнительный
		// технический weapon slot не является редактируемым местом экипировки.
		return weapon && weapon.GetWeaponSlotIndex() >= 0 && weapon.GetWeaponSlotIndex() < SCR_InventoryMenuUI.WEAPON_SLOTS_COUNT;
	}

	static typename Area(BaseInventoryStorageComponent storage, int slot)
	{
		if (!storage || slot < 0 || slot >= storage.GetSlotsCount())
			return typename.Empty;
		LoadoutSlotInfo info = LoadoutSlotInfo.Cast(storage.GetSlot(slot));
		if (info && info.GetAreaType())
			return info.GetAreaType().Type();
		return typename.Empty;
	}

	// Как в SCR_InventoryStorageBaseUI: у одежды объём принадлежит карманам,
	// а не ClothNode. Не складываем родительскую storage с её compartments.
	static bool Capacity(BaseInventoryStorageComponent storage, out float occupied, out float capacity)
	{
		occupied = 0;
		capacity = 0;
		if (!storage)
			return false;
		if (ClothNodeStorageComponent.Cast(storage))
		{
			array<BaseInventoryStorageComponent> pockets = {};
			storage.GetOwnedStorages(pockets, 1, false);
			foreach (BaseInventoryStorageComponent pocket : pockets)
			{
				if (!SCR_UniversalInventoryStorageComponent.Cast(pocket))
					continue;
				occupied += pocket.GetOccupiedSpace();
				capacity += pocket.GetMaxVolumeCapacity();
			}
		}
		else if (SCR_UniversalInventoryStorageComponent.Cast(storage))
		{
			occupied = storage.GetOccupiedSpace();
			capacity = storage.GetMaxVolumeCapacity();
		}
		return capacity > 0;
	}

	static string CapacityText(BaseInventoryStorageComponent storage)
	{
		float occupied, capacity;
		if (!Capacity(storage, occupied, capacity))
			return "{AICF:AICF_UI_Capacity_879d4588}";
		return FormatCapacity(occupied, capacity);
	}

	static string FormatCapacity(float occupied, float capacity)
	{
		if (capacity <= 0)
			return "{AICF:AICF_UI_Capacity_879d4588}";
		// Native dimensions/volume: сантиметры и см³. В UI показываем литры.
		float usedLitres = Math.Round(occupied / 100) / 10;
		float maxLitres = Math.Round(capacity / 100) / 10;
		float freeLitres = Math.Floor(Math.Max(0, capacity - occupied) / 100) / 10;
		int percent = Math.Round(occupied * 100 / capacity);
		return AICF_Localization.Format("{AICF:AICF_UI_Used_L_Free_L_c153f929}", string.Format("%1", usedLitres), string.Format("%1", maxLitres), string.Format("%1", percent.ToString() + "%"), string.Format("%1", freeLitres));
	}

	static int Describe(BaseInventoryStorageComponent storage, int slot, out string label)
	{
		label = "{AICF:AICF_UI_Attachment_slot_119bfff7}" + (slot + 1).ToString();
		if (!storage || slot < 0 || slot >= storage.GetSlotsCount())
			return 0;
		LoadoutSlotInfo info = LoadoutSlotInfo.Cast(storage.GetSlot(slot));
		if (info && info.GetAreaType())
		{
			bool namedAttachment = !EquipedLoadoutStorageComponent.Cast(storage) && !storage.GetSlot(slot).GetSourceName().IsEmpty();
			if (!storage.GetSlot(slot).GetSourceName().IsEmpty())
				label = WidgetManager.Translate(storage.GetSlot(slot).GetSourceName());
			typename area = info.GetAreaType().Type();
			if (area.IsInherited(LoadoutHeadCoverArea))
			{
				if (!namedAttachment)
					label = "{AICF:AICF_UI_Headgear_6bd918f9}";
				return SCR_EArsenalItemType.HEADWEAR;
			}
			if (area.IsInherited(LoadoutJacketArea))
			{
				label = "{AICF:AICF_UI_Jacket_061db74e}";
				return SCR_EArsenalItemType.TORSO;
			}
			if (area.IsInherited(LoadoutPantsArea))
			{
				label = "{AICF:AICF_UI_Trousers_c51b7b6c}";
				return SCR_EArsenalItemType.LEGS;
			}
			if (area.IsInherited(LoadoutBootsArea))
			{
				label = "{AICF:AICF_UI_Footwear_06a18677}";
				return SCR_EArsenalItemType.FOOTWEAR;
			}
			if (area.IsInherited(LoadoutHandwearSlotArea))
			{
				label = "{AICF:AICF_UI_Gloves_6d1ed0c0}";
				return SCR_EArsenalItemType.HANDWEAR;
			}
			if (area.IsInherited(LoadoutBackpackArea))
			{
				label = "{AICF:AICF_UI_Backpack_5445a612}";
				return SCR_EArsenalItemType.BACKPACK | SCR_EArsenalItemType.RADIO_BACKPACK;
			}
			if (area.IsInherited(LoadoutArmoredVestSlotArea))
			{
				label = "{AICF:AICF_UI_Body_armor_70dbb340}";
				return SCR_EArsenalItemType.VEST_AND_WAIST;
			}
			if (area.IsInherited(LoadoutVestArea))
			{
				label = "{AICF:AICF_UI_Unloading_671156ed}";
				return SCR_EArsenalItemType.VEST_AND_WAIST;
			}
			// Незнакомые mod areas часто имеют технические SourceName вроде
			// slot2/arsbelt. Их адрес остаётся внутри модели, а не в подписи.
			label = "{AICF:AICF_UI_Attachment_slot_119bfff7}" + (slot + 1).ToString();
		}
		if (EquipedWeaponStorageComponent.Cast(storage))
		{
			label = "{AICF:AICF_UI_Utility_slot_5f0dce66}";
			WeaponSlotComponent weapon = WeaponSlot(storage, slot);
			if (!weapon || !VisibleSlot(storage, slot))
				return 0;
			string type = weapon.GetWeaponSlotType();
			if (type == "primary")
			{
				label = "{AICF:AICF_UI_Primary_weapon_64c0e42d}";
				if (weapon.GetWeaponSlotIndex() > 0)
					label = "{AICF:AICF_UI_Second_primary_weapon_1876114c}";
			}
			else if (type == "secondary")
				label = "{AICF:AICF_UI_Holster_handgun_584af3e3}";
			else if (type == "grenade")
				label = "{AICF:AICF_UI_Grenades_22d4f4dc}";
			else
				label = "{AICF:AICF_UI_Secondary_weapon_a1d26ab2}" + type + ")";
		}
		else if (WeaponAttachmentsStorageComponent.Cast(storage))
		{
			label = "{AICF:AICF_UI_Attachment_78fdb568}" + (slot + 1).ToString();
			GenericComponent parent = storage.GetSlot(slot).GetParentContainer();
			if (BaseMuzzleComponent.Cast(parent))
			{
				label = "{AICF:AICF_UI_Magazine_b81150f8}";
				return 0;
			}
			AttachmentSlotComponent attachment = AttachmentSlotComponent.Cast(parent);
			if (attachment && attachment.GetAttachmentSlotType())
			{
				typename attachmentType = attachment.GetAttachmentSlotType().Type();
				if (attachmentType.IsInherited(AttachmentOptics))
					label = "{AICF:AICF_UI_Optic_b7371fec}";
				else if (attachmentType.IsInherited(AttachmentUnderBarrel))
					label = "{AICF:AICF_UI_Underbarrel_attachment_7d6bccfc}";
				else if (attachmentType.IsInherited(AttachmentBayonet))
					label = "{AICF:AICF_UI_Bayonet_0959e142}";
				else if (attachmentType.IsInherited(AttachmentMuzzle))
					label = "{AICF:AICF_UI_Muzzle_attachment_77e6e9c5}";
				else if (attachmentType.IsInherited(AttachmentHandGuard))
					label = "{AICF:AICF_UI_Handguard_32b25d01}";
			}
			return SCR_EArsenalItemType.WEAPON_ATTACHMENT;
		}
		return 0;
	}
}

// Native arsenal объединяет броню и разгрузки в VEST_AND_WAIST.
// Для UI различаем их по компоненту одежды. Metadata кэшируется на время
// открытия формы; preview создаётся только при первом запросе, вне campaign.
class AICF_LoadoutItemAreas
{
	protected ref map<ResourceName, typename> m_Areas = new map<ResourceName, typename>();
	protected ref map<ResourceName, string> m_WeaponTypes = new map<ResourceName, string>();

	// Только read-only native compatibility в мире draft. Для занятого места
	// проверяем замену, поэтому установленный обвес не делает каталог пустым.
	static bool MatchesAttachment(ResourceName prefab, BaseInventoryStorageComponent storage, int slot,
		ItemPreviewManagerEntity preview, InventoryStorageManagerComponent manager)
	{
		if (!AICF_LoadoutSlotView.HasFixedSlots(storage) || slot < 0 || slot >= storage.GetSlotsCount() || !preview || !manager)
			return false;
		BaseWorld world = storage.GetOwner().GetWorld();
		// Entity менеджера находится снаружи его native prefab-preview world.
		// Проверяем изоляцию обоих миров и совпадение candidate с самой storage.
		if (world == GetGame().GetWorld() || preview.GetWorld() == GetGame().GetWorld() || manager.GetOwner().GetWorld() != world)
			return false;
		IEntity candidate = preview.ResolvePreviewEntityForPrefab(prefab);
		if (!candidate || candidate.GetWorld() != world)
			return false;
		AttachmentSlotComponent attachment = AttachmentSlotComponent.Cast(storage.GetSlot(slot).GetParentContainer());
		if (attachment && !attachment.CanSetAttachment(candidate))
			return false;
		if (storage.Get(slot))
			return manager.CanReplaceItem(candidate, storage, slot);
		return manager.CanInsertItemInStorage(candidate, storage, slot);
	}

	static void FilterAttachments(array<string> prefabs, BaseInventoryStorageComponent storage, int slot,
		ItemPreviewManagerEntity preview, InventoryStorageManagerComponent manager)
	{
		for (int i = prefabs.Count() - 1; i >= 0; i--)
		{
			if (!MatchesAttachment(prefabs[i], storage, slot, preview, manager))
				prefabs.Remove(i);
		}
	}

	string WeaponType(ResourceName prefab, ItemPreviewManagerEntity preview)
	{
		string type;
		if (m_WeaponTypes.Find(prefab, type))
			return type;
		if (!preview)
			return string.Empty;
		IEntity entity = preview.ResolvePreviewEntityForPrefab(prefab);
		if (!entity || entity.GetWorld() == GetGame().GetWorld())
			return string.Empty;
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(entity.FindComponent(BaseWeaponComponent));
		if (weapon)
			type = weapon.GetWeaponSlotType();
		m_WeaponTypes.Insert(prefab, type);
		return type;
	}

	void FilterWeapons(array<string> prefabs, string type, ItemPreviewManagerEntity preview)
	{
		for (int i = prefabs.Count() - 1; i >= 0; i--)
		{
			if (type.IsEmpty() || WeaponType(prefabs[i], preview) != type)
				prefabs.Remove(i);
		}
	}

	typename Get(ResourceName prefab, ItemPreviewManagerEntity preview)
	{
		typename area;
		if (m_Areas.Find(prefab, area))
			return area;
		if (!preview)
			return typename.Empty;
		IEntity entity = preview.ResolvePreviewEntityForPrefab(prefab);
		if (!entity || entity.GetWorld() == GetGame().GetWorld())
			return typename.Empty;
		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(entity.FindComponent(BaseLoadoutClothComponent));
		if (cloth && cloth.GetAreaType())
			area = cloth.GetAreaType().Type();
		m_Areas.Insert(prefab, area);
		return area;
	}

	void Filter(array<string> prefabs, typename area, ItemPreviewManagerEntity preview)
	{
		if (area == typename.Empty)
			return;
		for (int i = prefabs.Count() - 1; i >= 0; i--)
		{
			if (!MatchesArea(prefabs[i], area, preview))
				prefabs.Remove(i);
		}
	}

	bool MatchesArea(ResourceName prefab, typename area, ItemPreviewManagerEntity preview)
	{
		typename actual = Get(prefab, preview);
		if (actual != typename.Empty && actual.IsInherited(area))
			return true;
		// Блокировка разгрузкой слота брони не превращает её в бронежилет.
		// Объединённая одежда отображается в обеих областях тела.
		if (area != LoadoutJacketArea && area != LoadoutPantsArea)
			return false;
		if (!preview)
			return false;
		IEntity item = preview.ResolvePreviewEntityForPrefab(prefab);
		return item && item.GetWorld() != GetGame().GetWorld() &&
			AICF_LoadoutClothing.Covers(BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent)), area);
	}
}

// Категории только для наполнения контейнера. Места надевания и обвесов
// всегда используют собственный native контекст.
class AICF_LoadoutContentCategories
{
	static void Fill(array<string> names, array<int> types, array<int> modes)
	{
		array<string> labels = {"{AICF:AICF_UI_All_items_63c8f01b}", "{AICF:AICF_UI_Ammunition_1e3e21cd}", "{AICF:AICF_UI_Grenades_and_smoke_7db7b67f}", "{AICF:AICF_UI_Medical_a7375e8d}", "{AICF:AICF_UI_Equipment_286b1543}", "{AICF:AICF_UI_Attachments_13f6ca69}", "{AICF:AICF_UI_Weapons_b16fba51}", "{AICF:AICF_UI_Clothing_and_containers_6fc79c8e}"};
		array<int> categories = {0, 0, SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE,
			SCR_EArsenalItemType.HEAL, SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemType.WEAPON_ATTACHMENT, 0,
			SCR_EArsenalItemType.HEADWEAR | SCR_EArsenalItemType.TORSO | SCR_EArsenalItemType.LEGS | SCR_EArsenalItemType.FOOTWEAR |
			SCR_EArsenalItemType.HANDWEAR | SCR_EArsenalItemType.VEST_AND_WAIST | SCR_EArsenalItemType.BACKPACK | SCR_EArsenalItemType.RADIO_BACKPACK};
		array<int> filters = {0, SCR_EArsenalItemMode.AMMUNITION, 0, 0, 0, 0, SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS, 0};
		names.Copy(labels);
		types.Copy(categories);
		modes.Copy(filters);
	}
}
