// Адаптер использует только inventory-часть stock serializer. Character labels,
// лицо, голос и active-weapon metadata не читаются из внешнего рецепта.
class AICF_LoadoutInventory : SCR_PlayerArsenalLoadout
{
	static bool Editable(BaseInventoryStorageComponent storage)
	{
		return storage && ARSENALLOADOUT_COMPONENTS_TO_CHECK && ShouldSaveStorage(storage);
	}

	static string StorageId(IEntity owner, BaseInventoryStorageComponent storage)
	{
		if (!storage.GetComponentSource(owner))
			return string.Empty;
		return GetComponentIdentifier(owner, storage);
	}

	static BaseInventoryStorageComponent FindStorage(IEntity owner, string id)
	{
		if (!owner)
			return null;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		FindStorageComponents(owner, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (Editable(storage) && StorageId(owner, storage) == id)
				return storage;
		}
		return null;
	}

	static IEntity ResolvePath(IEntity root, string path)
	{
		array<string> segments = {};
		path.Split("/", segments, true);
		if (segments.Count() > 6)
			return null;
		IEntity entity = root;
		foreach (string segment : segments)
		{
			array<string> parts = {};
			segment.Split("#", parts, false);
			if (parts.Count() != 2)
				return null;
			BaseInventoryStorageComponent storage = FindStorage(entity, parts[0]);
			int slot = parts[1].ToInt(-1);
			if (!storage || slot < 0 || slot >= storage.GetSlotsCount() || slot.ToString() != parts[1])
				return null;
			entity = storage.Get(slot);
			if (!entity)
				return null;
		}
		return entity;
	}

	static bool Capture(IEntity entity, out string value)
	{
		if (!entity || !ARSENALLOADOUT_COMPONENTS_TO_CHECK)
			return false;
		JsonSaveContext context = new JsonSaveContext();
		if (!ReadEntityStorageString(entity, context))
			return false;
		value = context.SaveToString();
		return !value.IsEmpty() && value.Length() <= 131072;
	}

	// Только server-generated snapshot или собственный локальный черновик.
	static bool Restore(IEntity entity, string value)
	{
		if (!entity)
			return false;
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
		JsonLoadContext context = new JsonLoadContext();
		if (!manager || !context.LoadFromString(value))
			return false;
		if (entity.GetWorld() == GetGame().GetWorld())
		{
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
			RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
			if (!Replication.IsServer() || !controller || controller.IsPlayerControlled() || !rpl || !rpl.IsMaster())
				return false;
			bool cleared = AICF_LoadoutClothing.ClearForRestore(entity, manager);
			bool applied = cleared && ApplyEntityStorageString(entity, context, manager);
			if (!applied)
				Print(string.Format("[AICF][LOADOUT_NATIVE_RESTORE_FAILED] entity=%1 cleared=%2 changing_item=%3 using_item=%4", entity.GetID(), cleared, controller.IsChangingItem(), controller.IsUsingItem()), LogLevel.WARNING);
			return applied;
		}
		return AICF_LoadoutClothing.ClearForRestore(entity, manager) && RestoreLocal(entity, context, manager, 0);
	}

	// В preview-world inventory tasks не исполняются. Как stock preview,
	// используем local entities + AttachEntity, предварительно проверяя storage.
	// Этот путь никогда не допускается для entity кампании.
	static bool DeleteLocal(IEntity item)
	{
		if (!item || item.GetWorld() == GetGame().GetWorld())
			return false;
		InventoryItemComponent component = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		InventoryStorageSlot parent;
		if (component)
			parent = component.GetParentSlot();
		if (parent)
		{
			if (parent.GetAttachedEntity() != item)
				return false;
			// Native storage должна вычесть объём/вес, пока item ещё существует.
			parent.DetachEntity();
			if (component && component.GetParentSlot())
				return false;
		}
		SCR_EntityHelper.DeleteEntityAndChildren(item);
		return true;
	}

	static IEntity InsertLocal(ResourceName prefab, BaseInventoryStorageComponent storage, int slot, InventoryStorageManagerComponent manager)
	{
		if (!storage || storage.GetOwner().GetWorld() == GetGame().GetWorld() ||
			!manager.CanInsertResourceInStorage(prefab, storage, slot))
			return null;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;
		IEntity item = GetGame().SpawnEntityPrefabLocal(resource, storage.GetOwner().GetWorld());
		if (!item)
			return null;
		InventoryStorageSlot target;
		if (slot >= 0)
			target = storage.GetSlot(slot);
		if (!target)
			target = storage.FindSuitableSlotForItem(item);
		if (!target || target.GetAttachedEntity() || (slot >= 0 && target.GetID() != slot))
		{
			DeleteLocal(item);
			return null;
		}
		target.AttachEntity(item);
		if (target.GetAttachedEntity() == item)
			return item;
		DeleteLocal(item);
		return null;
	}

	protected static bool RestoreLocal(IEntity entity, LoadContext context, InventoryStorageManagerComponent manager, int depth)
	{
		if (!entity || entity.GetWorld() == GetGame().GetWorld() || depth > 6)
			return false;
		int storageCount;
		context.StartArray("storages", storageCount);
		if (storageCount == 0)
			return true;
		for (int n; n < storageCount; n++)
		{
			string id;
			if (!context.StartObject() || !context.ReadValue("id", id))
				return false;
			BaseInventoryStorageComponent storage = FindStorage(entity, id);
			int count;
			if (!storage || !context.StartMap("slots", count))
				return false;
			array<int> wanted = {};
			for (int i; i < count; i++)
			{
				string key;
				ResourceName prefab;
				if (!context.ReadMapKey(i, key) || !context.StartObject(key) || !context.ReadValue("prefab", prefab))
					return false;
				int slot = key.ToInt(-1);
				if (slot < 0)
					return false;
				wanted.Insert(slot);
				IEntity item = storage.Get(slot);
				if (SCR_ResourceNameUtils.GetPrefabName(item) != prefab)
				{
					if (item && !DeleteLocal(item))
						return false;
					item = InsertLocal(prefab, storage, slot, manager);
				}
				if (!item)
				{
					AICF_Stage4Diagnostics.Warning("LOADOUT_LOCAL_RESTORE_FAILED", string.Format("storage=%1 slot=%2 prefab=%3", id, slot, prefab));
					return false;
				}
				if (!RestoreLocal(item, context, manager, depth + 1) || !context.EndObject())
					return false;
			}
			array<InventoryItemComponent> existing = {};
			storage.GetOwnedItems(existing, false);
			foreach (InventoryItemComponent extra : existing)
			{
				if (!wanted.Contains(extra.GetParentSlot().GetID()) && !DeleteLocal(extra.GetOwner()))
					return false;
			}
			if (!context.EndMap() || !context.EndObject())
				return false;
		}
		return context.EndArray();
	}

	static bool Describe(IEntity entity, string path, array<string> signature, AICF_LoadoutCatalog catalog, inout int cost, int depth = 0)
	{
		if (!entity || depth > 6 || signature.Count() > 160)
			return false;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!Editable(storage))
				continue;
			string id = StorageId(entity, storage);
			if (id.IsEmpty())
				return false;
			array<InventoryItemComponent> items = GetSlotItems(storage);
			foreach (InventoryItemComponent item : items)
			{
				if (!item || !item.GetParentSlot())
					return false;
				string childPath = path + id + "#" + item.GetParentSlot().GetID().ToString() + "/";
				ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item.GetOwner());
				signature.Insert(childPath + "=" + prefab);
				cost += catalog.Cost(prefab);
				if (!Describe(item.GetOwner(), childPath, signature, catalog, cost, depth + 1))
					return false;
			}
		}
		return true;
	}

	static string Signature(IEntity entity, AICF_LoadoutCatalog catalog, out int cost)
	{
		cost = 0;
		array<string> signature = {};
		if (!Describe(entity, string.Empty, signature, catalog, cost))
			return string.Empty;
		signature.Sort();
		return SCR_StringHelper.Join("\n", signature);
	}

	// Включает вложенные предметы готового prefab, а не только список RPC.
	static bool AllowsPersonalInventory(IEntity entity, AICF_LoadoutCatalog catalog, int depth = 0)
	{
		if (!entity || depth > 6) return false;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		FindStorageComponents(entity, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!Editable(storage)) continue;
			array<InventoryItemComponent> items = GetSlotItems(storage);
			foreach (InventoryItemComponent item : items)
			{
				if (!item) return false;
				ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item.GetOwner());
				if (catalog.IsDenied(prefab) || !AllowsPersonalInventory(item.GetOwner(), catalog, depth + 1)) return false;
			}
		}
		return true;
	}

	static bool HasUsableWeapons(IEntity entity, out string reason)
	{
		reason = "WEAPON_OR_AMMO_MISSING";
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(entity.FindComponent(BaseWeaponManagerComponent));
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
		if (!weapons || !inventory)
			return false;
		array<WeaponSlotComponent> slots = {};
		weapons.GetWeaponsSlots(slots);
		bool primary;
		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot.GetWeaponEntity() || slot.GetWeaponSlotIndex() >= 3)
				continue;
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(slot.GetWeaponEntity().FindComponent(BaseWeaponComponent));
			if (!weapon)
				continue;
			BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
			if (!muzzle || (muzzle.GetAmmoCount() <= 0 && inventory.GetMagazineCountByWeapon(weapon) <= 0))
				return false;
			if (weapon.GetWeaponType() == EWeaponType.WT_RIFLE || weapon.GetWeaponType() == EWeaponType.WT_MACHINEGUN ||
				weapon.GetWeaponType() == EWeaponType.WT_SNIPERRIFLE)
				primary = true;
		}
		if (primary)
			reason = string.Empty;
		return primary;
	}

	static bool Replay(IEntity entity, AICF_LoadoutRecipe recipe, AICF_LoadoutCatalog catalog, out string reason)
	{
		reason = "INVENTORY_INCOMPATIBLE";
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
		if (!manager || !recipe.HasValidBounds())
			return false;
		// Проверка всего внешнего списка до первой inventory mutation.
		foreach (string candidate : recipe.m_aPrefabs)
		{
			if (!candidate.IsEmpty() && !catalog.Allows(candidate))
			{
				reason = "ITEM_NOT_ALLOWED";
				return false;
			}
		}
		for (int i; i < recipe.m_aPaths.Count(); i++)
		{
			IEntity owner = ResolvePath(entity, recipe.m_aPaths[i]);
			BaseInventoryStorageComponent storage = FindStorage(owner, recipe.m_aStorages[i]);
			int slot = recipe.m_aSlots[i];
			if (!storage || slot >= storage.GetSlotsCount())
			{
				reason = "STORAGE_NOT_FOUND:" + recipe.m_aStorages[i];
				return false;
			}
			ResourceName prefab = recipe.m_aPrefabs[i];
			if (slot >= 0 && EquipedLoadoutStorageComponent.Cast(storage))
			{
				if (prefab.IsEmpty())
				{
					IEntity worn = AICF_LoadoutClothing.CoveringItem(storage, slot);
					if (worn && !DeleteLocal(worn))
						return false;
					continue;
				}
				Resource resource = Resource.Load(prefab);
				if (!resource || !resource.IsValid() || owner.GetWorld() == GetGame().GetWorld())
					return false;
				IEntity candidateItem = GetGame().SpawnEntityPrefabLocal(resource, owner.GetWorld());
				int targetSlot = AICF_LoadoutClothing.TargetSlot(storage, slot, candidateItem);
				bool prepared = targetSlot >= 0 && AICF_LoadoutClothing.RemoveConflictsLocal(storage, candidateItem);
				DeleteLocal(candidateItem);
				if (!prepared)
					return false;
				slot = targetSlot;
			}
			if (slot >= 0 && storage.Get(slot))
			{
				if (!DeleteLocal(storage.Get(slot)) || storage.Get(slot))
				{
					reason = "ITEM_DELETE_FAILED";
					return false;
				}
			}
			if (prefab.IsEmpty())
				continue;
			for (int n; n < recipe.m_aCounts[i]; n++)
			{
				if (!manager.CanInsertResourceInStorage(prefab, storage, slot))
				{
					reason = "ITEM_CAPACITY_OR_COMPATIBILITY";
					return false;
				}
				if (!InsertLocal(prefab, storage, slot, manager))
				{
					reason = "ITEM_INSERT_FAILED";
					return false;
				}
			}
			if (slot >= 0 && SCR_ResourceNameUtils.GetPrefabName(storage.Get(slot)) != prefab)
				return false;
		}
		reason = string.Empty;
		return true;
	}
}

// Собственный мир исключает физические, AI и preview-cache эффекты в кампании.
// Lifetime SharedItemRef заканчивается только после удаления preview widgets.
class AICF_LoadoutDraft
{
	protected static int s_iWorldSequence;
	protected ref SharedItemRef m_World;
	protected IEntity m_Character;
	protected ItemPreviewManagerEntity m_Preview;
	protected int m_iBaselineCost;
	protected ResourceName m_sAppearanceSource;
	protected ResourceName m_sHead;
	protected ResourceName m_sBody;

	IEntity GetCharacter() { return m_Character; }
	ItemPreviewManagerEntity GetPreview() { return m_Preview; }
	int GetBaselineCost() { return m_iBaselineCost; }

	bool Build(AICF_LoadoutRecipe recipe, AICF_LoadoutCatalog catalog, out string reason)
	{
		Clear();
		reason = "PREVIEW_WORLD_UNAVAILABLE";
		m_World = BaseWorld.CreateWorld("ChimeraWorld", "AICF_Loadout_" + (++s_iWorldSequence).ToString());
		if (!m_World || !m_World.IsValid())
			return false;
		ChimeraWorld world = m_World.GetRef();
		world.LoadSystems("{3A1DEDB2A5818B53}Configs/Systems/MapPreviewSystemsConfig.conf");
		Resource resource = Resource.Load(recipe.m_sCharacter);
		if (!resource || !resource.IsValid())
		{
			reason = "CHARACTER_RESOURCE_MISSING";
			return false;
		}
		Resource preview = Resource.Load("{9F18C476AB860F3B}Prefabs/World/Game/ItemPreviewManager.et");
		if (!preview || !preview.IsValid())
			return false;
		m_Preview = ItemPreviewManagerEntity.Cast(GetGame().SpawnEntityPrefabLocal(preview, world));
		if (!m_Preview)
			return false;
		m_Character = m_Preview.ResolvePreviewEntityForPrefab(recipe.m_sCharacter);
		if (!m_Character || m_Character.GetWorld() == GetGame().GetWorld())
			return false;
		KeepAppearance(recipe.m_sCharacter);
		DamageManagerComponent damage = DamageManagerComponent.Cast(m_Character.FindComponent(DamageManagerComponent));
		if (damage && !Replication.IsServer())
			damage.EnableDamageHandling(false);
		reason = "DEFAULT_LOADOUT_FAILED";
		if (!AICF_ContentProfile.GetActive().PrepareDefaultLoadout(m_Character, recipe.m_sCharacter, recipe.m_sFaction))
			return false;
		AICF_LoadoutInventory.Signature(m_Character, catalog, m_iBaselineCost);
		if (!AICF_LoadoutInventory.Replay(m_Character, recipe, catalog, reason))
			return false;
		return true;
	}

	// Rebuild создаёт новую entity, но не нового человека. Храним только
	// ресурсы внешности собственного preview, не принимаем identity из RPC.
	protected void KeepAppearance(ResourceName source)
	{
		if (!m_Character || m_Character.GetWorld() == GetGame().GetWorld())
			return;
		CharacterIdentityComponent component = CharacterIdentityComponent.Cast(m_Character.FindComponent(CharacterIdentityComponent));
		if (!component || !component.GetIdentity())
			return;
		VisualIdentity visual = component.GetIdentity().GetVisualIdentity();
		if (!visual)
			return;
		if (m_sAppearanceSource != source || m_sHead.IsEmpty())
		{
			m_sAppearanceSource = source;
			m_sHead = visual.GetHead();
			m_sBody = visual.GetBody();
			return;
		}
		visual.SetHead(m_sHead);
		visual.SetBody(m_sBody);
		component.CommitChanges();
	}

	void Clear()
	{
		if (m_Preview)
			delete m_Preview;
		if (m_Character)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Character);
		m_Preview = null;
		m_Character = null;
		m_World = null;
	}

	void ~AICF_LoadoutDraft()
	{
		Clear();
	}
}
