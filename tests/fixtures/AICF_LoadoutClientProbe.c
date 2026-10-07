// Только terminal fixture. Проверяет реальный owner RPC/snapshot без открытия UI.
class AICF_LoadoutProbePreview : AICF_LoadoutPreview
{
	LightEntity GetFillLight() { return m_FillLight; }
	IEntity GetModel() { return m_Model; }
}

modded class SCR_GameModeCampaign
{
	protected int m_iAICFLoadoutProbeStep;
	protected int m_iAICFLoadoutProbeToken;
	protected int m_iAICFLoadoutProbeRevision;
	protected int m_iAICFLoadoutProbeFailures;
	protected ref AICF_LoadoutRecipe m_AICFLoadoutProbeRecipe;
	protected ref AICF_LoadoutProbePreview m_AICFLifetimePreview;
	protected ref AICF_LoadoutDraft m_AICFLifetimeDraft;
	protected int m_iAICFLifetimeTicks;

	override void OnGameStart()
	{
		super.OnGameStart();
		string enabled;
		if (Replication.IsServer() || !System.GetCLIParam("aicfLoadoutClientProbe", enabled) || enabled != "1")
			return;
		GetGame().GetCallqueue().CallLater(AICF_LoadoutClientTick, 2000, true);
		GetGame().GetCallqueue().CallLater(AICF_LoadoutClientClose, 150000, false);
	}

	protected void AICF_LoadoutClientCheck(string rule, bool passed)
	{
		if (!passed)
			m_iAICFLoadoutProbeFailures++;
		Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] rule=%1 passed=%2", rule, passed));
	}

	protected void AICF_LoadoutClientTick()
	{
		if (m_iAICFLoadoutProbeStep == 7)
		{
			AICF_LoadoutLifetimeTick();
			return;
		}
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player)
			return;
		Faction desired = GetGame().GetFactionManager().GetFactionByKey(AICF_ContentProfile.GetActive().GetRuntimeFactionKey("US"));
		SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(player.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!desired || !affiliation)
			return;
		if (affiliation.GetAffiliatedFaction() != desired)
		{
			affiliation.RequestFaction(desired);
			return;
		}
		if (m_iAICFLoadoutProbeToken)
		{
			if (player.AICF_LoadoutToken() != m_iAICFLoadoutProbeToken)
				return;
			Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] step=%1 token=%2 revision=%3 accepted=%4 status=%5",
				m_iAICFLoadoutProbeStep, player.AICF_LoadoutToken(), player.AICF_LoadoutRevision(), player.AICF_LoadoutAccepted(), player.AICF_LoadoutStatus()));
			if (m_iAICFLoadoutProbeStep == 1)
			{
				m_AICFLoadoutProbeRecipe = AICF_LoadoutRecipe.Decode(player.AICF_LoadoutData());
				AICF_LoadoutClientCheck("OWNER_SNAPSHOT", player.AICF_LoadoutAccepted() && m_AICFLoadoutProbeRecipe && player.AICF_LoadoutMember() == 0);
				if (!m_AICFLoadoutProbeRecipe)
				{
					AICF_LoadoutClientClose();
					return;
				}
				m_iAICFLoadoutProbeRevision = player.AICF_LoadoutRevision();
				AICF_LoadoutClientCheck("FREE_TEMPLATE_OWNER_VIEW", player.AICF_LoadoutCost() == 0);
				AICF_LoadoutCheckDraftMemory();
				AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(SCR_CampaignFaction.Cast(desired));
				AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
				string reason;
				AICF_LoadoutClientCheck("CLIENT_ISOLATED_DRAFT", draft.Build(m_AICFLoadoutProbeRecipe, catalog, reason) && draft.GetPreview() && draft.GetCharacter().GetWorld() != GetGame().GetWorld());
				AICF_LoadoutCheckCatalog("US", draft);
				AICF_LoadoutCheckCatalog("USSR", draft);
				ItemPreviewManagerEntity thumbnails = AICF_LoadoutItemGrid.ResolveThumbnailManager();
				ChimeraWorld uiWorld = GetGame().GetWorld();
				AICF_LoadoutClientCheck("THUMBNAIL_STOCK_UI_MANAGER", thumbnails && thumbnails == uiWorld.GetItemPreviewManager() && thumbnails != draft.GetPreview());
				AICF_LoadoutCheckSlotNames(draft);
				AICF_LoadoutCheckAppearance(draft, catalog);
				AICF_LoadoutCheckRemoval(draft, catalog);
				AICF_LoadoutCheckAttachments("US");
				AICF_LoadoutCheckAttachments("USSR");
				AICF_LoadoutClientCheck("GRID_ODD_LAST_ROW", AICF_LoadoutItemGrid.LastPageOffset(7, 2, 6) == 2 && AICF_LoadoutItemGrid.LastPageOffset(9, 2, 6) == 4);
				AICF_LoadoutClientCheck("GRID_FULL_AND_EMPTY_PAGE", AICF_LoadoutItemGrid.LastPageOffset(6, 2, 6) == 0 && AICF_LoadoutItemGrid.LastPageOffset(0, 2, 6) == 0);
				// Без открытия GUI: строим именно production visual copy и проверяем
				// её мир/камеру, повторную сборку и освобождение ресурсов.
				AICF_LoadoutProbePreview preview = new AICF_LoadoutProbePreview();
				bool viewBuilt = preview.Build(draft.GetCharacter());
				AICF_LoadoutClientCheck("PREVIEW_VISUAL_BUILD", viewBuilt);
				if (viewBuilt)
				{
					BaseWorld viewWorld = preview.GetWorld();
					AICF_LoadoutClientCheck("PREVIEW_OWN_WORLD", viewWorld != GetGame().GetWorld() && viewWorld != draft.GetCharacter().GetWorld());
					AICF_LoadoutClientCheck("PREVIEW_LOCAL_LIGHTS", preview.HasLighting());
					LightEntity fill = preview.GetFillLight();
					AICF_LoadoutClientCheck("PREVIEW_FILL_OWN_WORLD", preview.HasFillLight() && fill.GetWorld() == viewWorld);
					AICF_LoadoutClientCheck("PREVIEW_FILL_DYNAMIC_NO_SHADOW", fill && fill.IsDynamic() && !fill.IsCastShadow());
					AICF_LoadoutCheckLightPose(preview, "INITIAL");
					for (int angle = 90; angle <= 360; angle += 90)
					{
						preview.Rotate(90);
						AICF_LoadoutCheckLightPose(preview, angle.ToString());
					}
					vector beforeFill;
					if (fill)
						beforeFill = fill.GetOrigin();
					vector beforeCamera[4], afterCamera[4];
					viewWorld.GetCamera(0, beforeCamera);
					preview.Rotate(45);
					preview.Zoom(-3);
					viewWorld.GetCamera(0, afterCamera);
					AICF_LoadoutClientCheck("PREVIEW_CAMERA_ROTATES", vector.Distance(beforeCamera[3], afterCamera[3]) > 0.1);
					AICF_LoadoutClientCheck("PREVIEW_FILL_FOLLOWS_CAMERA", fill && vector.Distance(beforeFill, fill.GetOrigin()) > 0.1);
					vector fillBeforeZoom;
					if (fill)
						fillBeforeZoom = fill.GetOrigin();
					preview.Zoom(3);
					AICF_LoadoutClientCheck("PREVIEW_FILL_ZOOM_STABLE", fill && vector.Distance(fillBeforeZoom, fill.GetOrigin()) < 0.001);
					viewWorld.GetCamera(0, beforeCamera);
					preview.Pan(0.1);
					viewWorld.GetCamera(0, afterCamera);
					AICF_LoadoutClientCheck("PREVIEW_CAMERA_PANS_VERTICAL", afterCamera[3][1] > beforeCamera[3][1] + 0.05 && Math.AbsFloat(afterCamera[3][0] - beforeCamera[3][0]) < 0.001 && Math.AbsFloat(afterCamera[3][2] - beforeCamera[3][2]) < 0.001);
					AICF_LoadoutClientCheck("PREVIEW_FILL_PAN_STABLE", fill && vector.Distance(fillBeforeZoom, fill.GetOrigin()) < 0.001);
					AICF_LoadoutClientCheck("PREVIEW_REUSES_WORLD", preview.Build(draft.GetCharacter()) && preview.GetWorld() == viewWorld);
					viewWorld.GetCamera(0, beforeCamera);
					AICF_LoadoutClientCheck("PREVIEW_PAN_SURVIVES_REBUILD", Math.AbsFloat(beforeCamera[3][1] - afterCamera[3][1]) < 0.001);
					preview.Pan(100);
					viewWorld.GetCamera(0, beforeCamera);
					preview.Pan(100);
					viewWorld.GetCamera(0, afterCamera);
					AICF_LoadoutClientCheck("PREVIEW_PAN_CLAMPED", vector.Distance(beforeCamera[3], afterCamera[3]) < 0.001 && afterCamera[3][1] < 3);
					preview.Pan(-100);
					viewWorld.GetCamera(0, afterCamera);
					AICF_LoadoutClientCheck("PREVIEW_PAN_BOTH_DIRECTIONS", afterCamera[3][1] < -0.1);
					AICF_LoadoutClientCheck("PREVIEW_REUSES_FILL", fill && preview.GetFillLight() == fill);
					AICF_LoadoutCheckLightPose(preview, "REBUILD");
				}
				string previewOnly;
				if (System.GetCLIParam("aicfLoadoutPreviewOnly", previewOnly) && previewOnly == "1")
				{
					// Проверки после выхода из callback: мгновенная сборка/очистка
					// не обнаруживает неподдерживаемое хранение native pointer.
					m_AICFLifetimePreview = preview;
					m_AICFLifetimeDraft = draft;
					m_iAICFLoadoutProbeStep = 7;
					m_iAICFLoadoutProbeToken = 0;
					return;
				}
				preview.Clear();
				AICF_LoadoutClientCheck("PREVIEW_CLEANUP", preview.GetWorld() == null);
				AICF_LoadoutClientCheck("PREVIEW_LIGHTS_CLEANUP", !preview.HasLighting());
				AICF_LoadoutClientCheck("PREVIEW_FILL_CLEANUP", !preview.GetFillLight());
				draft.Clear();
				m_AICFLoadoutProbeRecipe.m_sName = "Client RPC probe";
			}
			else if (m_iAICFLoadoutProbeStep == 2)
			{
				AICF_LoadoutClientCheck("SAVE_ACK", player.AICF_LoadoutAccepted() && player.AICF_LoadoutStatus() == "SAVED" && player.AICF_LoadoutRevision() > m_iAICFLoadoutProbeRevision);
				AICF_LoadoutClientCheck("FREE_TEMPLATE_SAVED_VIEW", player.AICF_LoadoutCost() == 0);
			}
			else if (m_iAICFLoadoutProbeStep == 3)
				AICF_LoadoutClientCheck("STALE_RPC_REJECTED", !player.AICF_LoadoutAccepted() && player.AICF_LoadoutStatus() == "REVISION_CONFLICT");
			else if (m_iAICFLoadoutProbeStep == 4)
				AICF_LoadoutClientCheck("WRONG_FACTION_REJECTED", !player.AICF_LoadoutAccepted());
			else if (m_iAICFLoadoutProbeStep == 5)
			{
				AICF_LoadoutClientCheck("OUT_OF_RANGE_REJECTED", !player.AICF_LoadoutAccepted());
				m_iAICFLoadoutProbeStep = 6;
				AICF_LoadoutClientClose();
				return;
			}
			m_iAICFLoadoutProbeToken = 0;
		}
		if (m_iAICFLoadoutProbeStep == 0)
			m_iAICFLoadoutProbeToken = player.AICF_RequestLoadout(0, 0, -1, 0);
		else if (m_iAICFLoadoutProbeStep == 1 || m_iAICFLoadoutProbeStep == 2)
			m_iAICFLoadoutProbeToken = player.AICF_RequestLoadout(0, 0, m_iAICFLoadoutProbeRevision, 1, m_AICFLoadoutProbeRecipe.Encode());
		else if (m_iAICFLoadoutProbeStep == 3)
		{
			m_AICFLoadoutProbeRecipe.m_sFaction = "USSR";
			m_iAICFLoadoutProbeToken = player.AICF_RequestLoadout(0, 0, player.AICF_LoadoutRevision(), 1, m_AICFLoadoutProbeRecipe.Encode());
		}
		else if (m_iAICFLoadoutProbeStep == 4)
			m_iAICFLoadoutProbeToken = player.AICF_RequestLoadout(0, 10, player.AICF_LoadoutRevision(), 0);
		m_iAICFLoadoutProbeStep++;
	}

	protected void AICF_LoadoutCheckDraftMemory()
	{
		AICF_LoadoutDraftMemory memory = new AICF_LoadoutDraftMemory();
		bool stale;
		memory.Remember(0, 0, 1, "A0 commander");
		memory.Remember(6, 0, 1, "D0 commander");
		memory.Remember(6, 1, 1, "D0 medic");
		AICF_LoadoutClientCheck("DRAFT_GROUP_ISOLATION", memory.Recover(0, 0, 1, stale) == "A0 commander" && memory.Recover(6, 0, 1, stale) == "D0 commander");
		AICF_LoadoutClientCheck("DRAFT_MEMBER_ISOLATION", memory.Recover(6, 1, 1, stale) == "D0 medic" && memory.Recover(0, 1, 1, stale).IsEmpty());
		memory.Remember(6, 0, 1, "D0 updated");
		AICF_LoadoutClientCheck("DRAFT_REMEMBER_LATEST", memory.Recover(6, 0, 1, stale) == "D0 updated");
		string outdated = memory.Recover(6, 0, 2, stale);
		AICF_LoadoutClientCheck("DRAFT_STALE_REVISION_REJECTED", outdated.IsEmpty() && stale);
		AICF_LoadoutClientCheck("DRAFT_STALE_REMOVED", memory.Recover(6, 0, 1, stale).IsEmpty() && !stale);
		memory.Forget(0, 0);
		AICF_LoadoutClientCheck("DRAFT_FORGET_ONE_TARGET", memory.Recover(0, 0, 1, stale).IsEmpty() && memory.Recover(6, 1, 1, stale) == "D0 medic");
		memory.Remember(-1, 0, 1, "invalid");
		memory.Remember(0, AICF_Stage1Config.MAX_GROUP_SIZE, 1, "invalid");
		memory.Remember(1, 0, -1, "invalid");
		AICF_LoadoutClientCheck("DRAFT_INVALID_TARGET_REJECTED", memory.Recover(-1, 0, 1, stale).IsEmpty() && memory.Recover(1, 0, 1, stale).IsEmpty());
		memory.Clear();
		AICF_LoadoutClientCheck("DRAFT_MEMORY_CLEANUP", memory.Recover(6, 1, 1, stale).IsEmpty());
	}

	protected void AICF_LoadoutCheckSlotNames(AICF_LoadoutDraft draft)
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(draft.GetCharacter(), storages);
		bool head, jacket, armor, container, contents;
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!AICF_LoadoutInventory.Editable(storage))
				continue;
			for (int i; i < storage.GetSlotsCount(); i++)
			{
				string label;
				int category = AICF_LoadoutSlotView.Describe(storage, i, label);
				LoadoutSlotInfo info = LoadoutSlotInfo.Cast(storage.GetSlot(i));
				string area;
				if (info && info.GetAreaType())
					area = info.GetAreaType().Type().ToString();
				Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] slot=%1 area=%2 category=%3 label=%4", i, area, category, label));
				if (category == SCR_EArsenalItemType.HEADWEAR)
					head = !label.IsEmpty();
				if (category == SCR_EArsenalItemType.TORSO)
					jacket = !label.IsEmpty();
				if (label == "Бронежилет")
					armor = category == SCR_EArsenalItemType.VEST_AND_WAIST;
				IEntity item = storage.Get(i);
				if (!item)
					continue;
				set<BaseInventoryStorageComponent> pockets = new set<BaseInventoryStorageComponent>();
				SCR_PlayerArsenalLoadout.FindStorageComponents(item, pockets);
				foreach (BaseInventoryStorageComponent pocket : pockets)
				{
					float used, max;
					if (!AICF_LoadoutInventory.Editable(pocket) || !AICF_LoadoutSlotView.Capacity(pocket, used, max))
						continue;
					container = true;
					contents = contents || used > 0;
					Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] container=%1 occupied=%2 capacity=%3 text=%4", item.GetPrefabData().GetPrefabName(), used, max, AICF_LoadoutSlotView.CapacityText(pocket)));
				}
			}
		}
		AICF_LoadoutClientCheck("NATIVE_HEAD_SLOT_CONTEXT", head);
		AICF_LoadoutClientCheck("NATIVE_JACKET_SLOT_CONTEXT", jacket);
		AICF_LoadoutClientCheck("NATIVE_ARMOR_SLOT_CONTEXT", armor);
		AICF_LoadoutClientCheck("NATIVE_CONTAINER_CAPACITY", container);
		AICF_LoadoutClientCheck("NATIVE_CONTAINER_OCCUPANCY", contents);
		BaseInventoryStorageComponent weaponStorage = BaseInventoryStorageComponent.Cast(draft.GetCharacter().FindComponent(EquipedWeaponStorageComponent));
		int visible, secondary, grenades, hidden;
		if (weaponStorage)
		{
			for (int n; n < weaponStorage.GetSlotsCount(); n++)
			{
				WeaponSlotComponent weapon = AICF_LoadoutSlotView.WeaponSlot(weaponStorage, n);
				string type;
				if (weapon)
					type = weapon.GetWeaponSlotType();
				bool shown = AICF_LoadoutSlotView.VisibleSlot(weaponStorage, n);
				Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] weapon_slot=%1 type=%2 visible=%3", n, type, shown));
				if (!shown)
				{
					hidden++;
					continue;
				}
				visible++;
				if (type == "secondary")
					secondary++;
				if (type == "grenade")
					grenades++;
			}
		}
		AICF_LoadoutClientCheck("WEAPON_USER_SLOTS", visible == SCR_InventoryMenuUI.WEAPON_SLOTS_COUNT);
		AICF_LoadoutClientCheck("WEAPON_NATIVE_HOLSTER", secondary == 1);
		AICF_LoadoutClientCheck("WEAPON_NATIVE_GRENADES", grenades == 1);
		AICF_LoadoutClientCheck("WEAPON_INTERNAL_SLOT_HIDDEN", hidden > 0);
	}

	protected ResourceName AICF_LoadoutHead(IEntity entity)
	{
		if (!entity)
			return string.Empty;
		CharacterIdentityComponent identity = CharacterIdentityComponent.Cast(entity.FindComponent(CharacterIdentityComponent));
		if (!identity || !identity.GetIdentity() || !identity.GetIdentity().GetVisualIdentity())
			return string.Empty;
		return identity.GetIdentity().GetVisualIdentity().GetHead();
	}

	protected ResourceName AICF_LoadoutHeadMesh(IEntity entity)
	{
		if (!entity)
			return string.Empty;
		CharacterIdentityComponent identity = CharacterIdentityComponent.Cast(entity.FindComponent(CharacterIdentityComponent));
		if (!identity || !identity.GetHeadEntity() || !identity.GetHeadEntity().GetVObject())
			return string.Empty;
		return identity.GetHeadEntity().GetVObject().GetResourceName();
	}

	protected void AICF_LoadoutCheckAppearance(AICF_LoadoutDraft draft, AICF_LoadoutCatalog catalog)
	{
		ResourceName originalHead = AICF_LoadoutHead(draft.GetCharacter());
		ResourceName originalHeadMesh = AICF_LoadoutHeadMesh(draft.GetCharacter());
		string storageId;
		int headSlot = -1;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(draft.GetCharacter(), storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			for (int i; i < storage.GetSlotsCount(); i++)
			{
				typename area = AICF_LoadoutSlotView.Area(storage, i);
				if (area != typename.Empty && area.IsInherited(LoadoutHeadCoverArea))
				{
					storageId = AICF_LoadoutInventory.StorageId(draft.GetCharacter(), storage);
					headSlot = i;
				}
			}
		}
		array<string> hats = {}, weapons = {};
		catalog.List(SCR_EArsenalItemType.HEADWEAR, hats);
		catalog.List(0, weapons, SCR_EArsenalItemMode.WEAPON);
		bool unchanged = !originalHead.IsEmpty() && !originalHeadMesh.IsEmpty() && headSlot >= 0 && hats.Count() >= 3;
		string reason;
		if (unchanged)
		{
			for (int change; change < 3; change++)
			{
				AICF_LoadoutRecipe changed = AICF_LoadoutRecipe.Decode(m_AICFLoadoutProbeRecipe.Encode());
				changed.Add("", storageId, headSlot, hats[change], 1);
				unchanged = draft.Build(changed, catalog, reason) && AICF_LoadoutHead(draft.GetCharacter()) == originalHead &&
					AICF_LoadoutHeadMesh(draft.GetCharacter()) == originalHeadMesh && unchanged;
			}
		}
		AICF_LoadoutClientCheck("APPEARANCE_THREE_HEADGEAR_CHANGES", unchanged);
		AICF_LoadoutProbePreview model = new AICF_LoadoutProbePreview();
		bool rendered = model.Build(draft.GetCharacter());
		AICF_LoadoutClientCheck("APPEARANCE_VISUAL_COPY", rendered && AICF_LoadoutHead(model.GetModel()) == originalHead);
		// Native preview head — GenericEntity с mesh, без исходного prefab.
		Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] head_identity=%1 original_mesh=%2 draft_mesh=%3 rendered_mesh=%4", originalHead, originalHeadMesh, AICF_LoadoutHeadMesh(draft.GetCharacter()), AICF_LoadoutHeadMesh(model.GetModel())));
		AICF_LoadoutClientCheck("APPEARANCE_RENDERED_HEAD_ENTITY", rendered && !originalHeadMesh.IsEmpty() && AICF_LoadoutHeadMesh(model.GetModel()) == originalHeadMesh);
		model.Clear();
		if (headSlot >= 0 && !weapons.IsEmpty())
		{
			AICF_LoadoutRecipe invalid = AICF_LoadoutRecipe.Decode(m_AICFLoadoutProbeRecipe.Encode());
			invalid.Add("", storageId, headSlot, weapons[0], 1);
			AICF_LoadoutClientCheck("APPEARANCE_INCOMPATIBLE_REJECTED", !draft.Build(invalid, catalog, reason));
		}
		AICF_LoadoutClientCheck("APPEARANCE_ROLLBACK", draft.Build(m_AICFLoadoutProbeRecipe, catalog, reason) && AICF_LoadoutHead(draft.GetCharacter()) == originalHead);
	}

	// У ALICE магазины находятся в storage подсумка, прикреплённого к ClothNode.
	// Ищем реальный предмет внутри кармана по тому же пути, что редактор.
	protected bool AICF_LoadoutRemovalTarget(IEntity owner, string path, out string targetPath, out string targetStorage,
		out int targetSlot, int depth = 0)
	{
		if (!owner || depth > 6)
			return false;
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(owner, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!AICF_LoadoutInventory.Editable(storage))
				continue;
			string id = AICF_LoadoutInventory.StorageId(owner, storage);
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				if (!item || !item.GetParentSlot() || item.GetParentSlot().GetStorage() != storage)
					continue;
				int slot = item.GetParentSlot().GetID();
				float occupied, capacity;
				if (SCR_UniversalInventoryStorageComponent.Cast(storage) && item.GetTotalVolume() > 0 &&
					AICF_LoadoutSlotView.Capacity(storage, occupied, capacity))
				{
					targetPath = path;
					targetStorage = id;
					targetSlot = slot;
					return true;
				}
				if (AICF_LoadoutRemovalTarget(item.GetOwner(), path + id + "#" + slot.ToString() + "/",
					targetPath, targetStorage, targetSlot, depth + 1))
					return true;
			}
		}
		return false;
	}

	protected void AICF_LoadoutCheckRemoval(AICF_LoadoutDraft draft, AICF_LoadoutCatalog catalog)
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(draft.GetCharacter(), storages);
		foreach (BaseInventoryStorageComponent root : storages)
		{
			for (int i; i < root.GetSlotsCount(); i++)
			{
				typename area = AICF_LoadoutSlotView.Area(root, i);
				IEntity vest = root.Get(i);
				if (!vest || area == typename.Empty || !area.IsInherited(LoadoutVestArea))
					continue;
				string vestPath = AICF_LoadoutInventory.StorageId(draft.GetCharacter(), root) + "#" + i.ToString() + "/";
				string path, id;
				int slot;
				if (!AICF_LoadoutRemovalTarget(vest, vestPath, path, id, slot))
					continue;
				BaseInventoryStorageComponent pocket = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(draft.GetCharacter(), path), id);
				float beforeUsed, capacity;
				AICF_LoadoutSlotView.Capacity(pocket, beforeUsed, capacity);
				array<InventoryItemComponent> items = {};
				pocket.GetOwnedItems(items, false);
				int beforeCount = items.Count();
				float removedVolume = pocket.GetItem(slot).GetTotalVolume();
				Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] removal_storage=%1 item=%2 volume=%3 slot=%4", pocket, SCR_ResourceNameUtils.GetPrefabName(pocket.Get(slot)), removedVolume, slot));
				AICF_LoadoutRecipe changed = AICF_LoadoutRecipe.Decode(m_AICFLoadoutProbeRecipe.Encode());
				changed.Add(path, id, slot, "", 1);
				items.Clear();
				string reason;
				bool built = draft.Build(changed, catalog, reason);
				BaseInventoryStorageComponent result = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(draft.GetCharacter(), path), id);
				float afterUsed, afterCapacity;
				if (result)
					result.GetOwnedItems(items, false);
				AICF_LoadoutSlotView.Capacity(result, afterUsed, afterCapacity);
				Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] removed_volume=%1 before_volume=%2 after_volume=%3 capacity_before=%4 capacity_after=%5", removedVolume, beforeUsed, afterUsed, capacity, afterCapacity));
				AICF_LoadoutClientCheck("RIG_ITEM_REMOVED", built && result && items.Count() == beforeCount - 1);
				AICF_LoadoutClientCheck("RIG_CAPACITY_RELEASED", AICF_LoadoutSlotView.Capacity(result, afterUsed, afterCapacity) &&
					afterUsed < beforeUsed && Math.AbsFloat(beforeUsed - afterUsed - removedVolume) < 0.01 && afterCapacity == capacity);
				items.Clear();
				bool restored = draft.Build(m_AICFLoadoutProbeRecipe, catalog, reason);
				result = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(draft.GetCharacter(), path), id);
				if (result)
					result.GetOwnedItems(items, false);
				AICF_LoadoutClientCheck("RIG_REMOVAL_UNDO", restored && items.Count() == beforeCount &&
					AICF_LoadoutSlotView.Capacity(result, afterUsed, afterCapacity) && afterUsed == beforeUsed && afterCapacity == capacity);
				return;
			}
		}
		AICF_LoadoutClientCheck("RIG_ITEM_REMOVED", false);
	}

	protected void AICF_LoadoutCheckAttachments(string stableFaction)
	{
		FactionKey key = AICF_ContentProfile.GetActive().GetRuntimeFactionKey(stableFaction);
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(key));
		AICF_GroupSpawner source = new AICF_GroupSpawner();
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
		string role, reason;
		recipe.m_sCharacter = source.ResolveRecruitPrefab(faction, 0, role);
		recipe.m_sProfile = AICF_ContentProfile.GetActive().GetProfileKey();
		recipe.m_sFaction = stableFaction;
		recipe.m_sName = "Attachment probe";
		AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
		bool built = draft.Build(recipe, catalog, reason);
		AICF_LoadoutClientCheck("ATTACHMENT_DRAFT_" + stableFaction, built);
		if (!built)
			return;
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
		int ignored;
		string signature = AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, ignored);
		array<string> paths = {}, ids = {}, replacements = {}, hats = {};
		array<int> slotIds = {};
		catalog.List(SCR_EArsenalItemType.HEADWEAR, hats);
		set<BaseInventoryStorageComponent> roots = new set<BaseInventoryStorageComponent>();
		SCR_PlayerArsenalLoadout.FindStorageComponents(draft.GetCharacter(), roots);
		int attachments, magazines, rejected;
		foreach (BaseInventoryStorageComponent root : roots)
		{
			if (!EquipedWeaponStorageComponent.Cast(root))
				continue;
			for (int weaponSlot; weaponSlot < root.GetSlotsCount(); weaponSlot++)
			{
				IEntity weapon = root.Get(weaponSlot);
				if (!weapon)
					continue;
				string path = AICF_LoadoutInventory.StorageId(draft.GetCharacter(), root) + "#" + weaponSlot.ToString() + "/";
				set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
				SCR_PlayerArsenalLoadout.FindStorageComponents(weapon, storages);
				foreach (BaseInventoryStorageComponent storage : storages)
				{
					if (!WeaponAttachmentsStorageComponent.Cast(storage) || !AICF_LoadoutInventory.Editable(storage))
						continue;
					for (int slot; slot < storage.GetSlotsCount(); slot++)
					{
						if (!AICF_LoadoutSlotView.VisibleSlot(storage, slot))
							continue;
						int category, mode;
						AICF_LoadoutSlotView.AttachmentCatalog(storage, slot, category, mode);
						array<string> prefabs = {};
						catalog.List(category, prefabs, mode);
						int available = prefabs.Count();
						int resourceFits, itemFits, replaceFits, slotFits, sameWorld;
						foreach (string prefab : prefabs)
						{
							IEntity candidate = draft.GetPreview().ResolvePreviewEntityForPrefab(prefab);
							if (!candidate)
								continue;
							if (candidate.GetWorld() == storage.GetOwner().GetWorld())
								sameWorld++;
							if (manager.CanInsertResourceInStorage(prefab, storage, slot))
								resourceFits++;
							if (manager.CanInsertItemInStorage(candidate, storage, slot))
								itemFits++;
							if (manager.CanReplaceItem(candidate, storage, slot))
								replaceFits++;
							AttachmentSlotComponent nativeSlot = AttachmentSlotComponent.Cast(storage.GetSlot(slot).GetParentContainer());
							if (nativeSlot && nativeSlot.CanSetAttachment(candidate))
								slotFits++;
						}
						Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] attachment_query=%1 slot=%2 preview_world=%3 owner_world=%4 candidates_same_world=%5 resource_fit=%6 item_fit=%7 replace_fit=%8 slot_fit=%9", stableFaction, slot, draft.GetPreview().GetWorld() == storage.GetOwner().GetWorld(), manager.GetOwner().GetWorld() == storage.GetOwner().GetWorld(), sameWorld, resourceFits, itemFits, replaceFits, slotFits));
						AICF_LoadoutItemAreas.FilterAttachments(prefabs, storage, slot, draft.GetPreview(), manager);
						string label;
						AICF_LoadoutSlotView.Describe(storage, slot, label);
						Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] attachments=%1 weapon=%2 slot=%3 label=%4 occupied=%5 candidates=%6 matches=%7", stableFaction, SCR_ResourceNameUtils.GetPrefabName(weapon), slot, label, storage.Get(slot) != null, available, prefabs.Count()));
						if (!hats.IsEmpty() && !AICF_LoadoutItemAreas.MatchesAttachment(hats[0], storage, slot, draft.GetPreview(), manager))
							rejected++;
						if (prefabs.IsEmpty())
							continue;
						if (mode == SCR_EArsenalItemMode.AMMUNITION)
							magazines++;
						else
							attachments++;
						paths.Insert(path);
						ids.Insert(AICF_LoadoutInventory.StorageId(weapon, storage));
						slotIds.Insert(slot);
						replacements.Insert(prefabs[0]);
					}
				}
			}
		}
		AICF_LoadoutClientCheck("ATTACHMENT_CATALOG_" + stableFaction, attachments > 0 && magazines > 0 && rejected > 0);
		AICF_LoadoutClientCheck("ATTACHMENT_FILTER_READ_ONLY_" + stableFaction, signature == AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, ignored));
		for (int change; change < paths.Count(); change++)
		{
			AICF_LoadoutRecipe changed = AICF_LoadoutRecipe.Decode(recipe.Encode());
			changed.Add(paths[change], ids[change], slotIds[change], replacements[change], 1);
			bool applied = draft.Build(changed, catalog, reason);
			BaseInventoryStorageComponent target = AICF_LoadoutInventory.FindStorage(AICF_LoadoutInventory.ResolvePath(draft.GetCharacter(), paths[change]), ids[change]);
			AICF_LoadoutClientCheck("ATTACHMENT_APPLY_" + stableFaction + "_" + change.ToString(), applied && target && SCR_ResourceNameUtils.GetPrefabName(target.Get(slotIds[change])) == replacements[change]);
			if (applied && target)
			{
				manager = InventoryStorageManagerComponent.Cast(draft.GetCharacter().FindComponent(InventoryStorageManagerComponent));
				AICF_LoadoutClientCheck("ATTACHMENT_OCCUPIED_REPLACE_" + stableFaction + "_" + change.ToString(), AICF_LoadoutItemAreas.MatchesAttachment(replacements[change], target, slotIds[change], draft.GetPreview(), manager));
			}
		}
		draft.Clear();
	}

	// Только данные и изолированные preview entities: форма не открывается,
	// визуальные карточки и обработка ввода требуют ручного verdict.
	protected void AICF_LoadoutCheckCatalog(string stableFaction, AICF_LoadoutDraft draft)
	{
		FactionKey runtimeKey = AICF_ContentProfile.GetActive().GetRuntimeFactionKey(stableFaction);
		SCR_CampaignFaction faction = SCR_CampaignFaction.Cast(GetGame().GetFactionManager().GetFactionByKey(runtimeKey));
		AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
		array<string> all = {}, weapons = {}, ammo = {}, uniforms = {};
		catalog.List(0, all);
		array<string> categoryNames = {};
		array<int> categoryTypes = {}, categoryModes = {};
		AICF_LoadoutContentCategories.Fill(categoryNames, categoryTypes, categoryModes);
		bool categoriesValid = categoryNames.Count() == 8 && categoryTypes.Count() == 8 && categoryModes.Count() == 8;
		for (int filter; filter < categoryNames.Count(); filter++)
		{
			array<string> entries = {};
			catalog.List(categoryTypes[filter], entries, categoryModes[filter]);
			if (entries.IsEmpty())
				categoriesValid = false;
			foreach (string entry : entries)
			{
				SCR_ArsenalItem data = SCR_ArsenalItem.Cast(catalog.Find(entry).GetEntityDataOfType(SCR_ArsenalItem));
				if (!all.Contains(entry) || (categoryTypes[filter] != 0 && (data.GetItemType() & categoryTypes[filter]) == 0) ||
					(categoryModes[filter] != 0 && (data.GetItemMode() & categoryModes[filter]) == 0))
					categoriesValid = false;
			}
			Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] contents=%1 category=%2 matches=%3", stableFaction, categoryNames[filter], entries.Count()));
		}
		AICF_LoadoutClientCheck("CONTENTS_CATEGORIES_" + stableFaction, categoriesValid);
		catalog.List(0, weapons, SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS);
		catalog.List(0, ammo, SCR_EArsenalItemMode.AMMUNITION);
		catalog.List(SCR_EArsenalItemType.TORSO, uniforms);
		array<string> armor = {}, rigs = {}, vests = {};
		catalog.List(SCR_EArsenalItemType.VEST_AND_WAIST, vests);
		armor.Copy(vests);
		rigs.Copy(vests);
		AICF_LoadoutItemAreas areas = new AICF_LoadoutItemAreas();
		areas.Filter(armor, LoadoutArmoredVestSlotArea, draft.GetPreview());
		areas.Filter(rigs, LoadoutVestArea, draft.GetPreview());
		bool armorOnly = !armor.IsEmpty(), rigsOnly = !rigs.IsEmpty();
		foreach (string armorPrefab : armor)
		{
			IEntity armorEntity = draft.GetPreview().ResolvePreviewEntityForPrefab(armorPrefab);
			BaseLoadoutClothComponent armorCloth = BaseLoadoutClothComponent.Cast(armorEntity.FindComponent(BaseLoadoutClothComponent));
			armorOnly = armorOnly && armorCloth && armorCloth.GetAreaType() && armorCloth.GetAreaType().IsInherited(LoadoutArmoredVestSlotArea) && !rigs.Contains(armorPrefab);
		}
		foreach (string rigPrefab : rigs)
		{
			IEntity rigEntity = draft.GetPreview().ResolvePreviewEntityForPrefab(rigPrefab);
			BaseLoadoutClothComponent rigCloth = BaseLoadoutClothComponent.Cast(rigEntity.FindComponent(BaseLoadoutClothComponent));
			rigsOnly = rigsOnly && rigCloth && rigCloth.GetAreaType() && rigCloth.GetAreaType().IsInherited(LoadoutVestArea);
		}
		Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] faction=%1 armor=%2 rigs=%3 native_vests=%4", stableFaction, armor.Count(), rigs.Count(), vests.Count()));
		AICF_LoadoutClientCheck("CATALOG_ARMOR_ONLY_" + stableFaction, armorOnly);
		AICF_LoadoutClientCheck("CATALOG_RIGS_ONLY_" + stableFaction, rigsOnly);
		AICF_LoadoutClientCheck("CATALOG_VEST_PARTITION_" + stableFaction, armor.Count() + rigs.Count() == vests.Count());
		array<string> weaponTypes = {"primary", "secondary", "grenade"};
		foreach (string weaponType : weaponTypes)
		{
			array<string> matches = {};
			int typeFilter, modeFilter;
			AICF_LoadoutSlotView.WeaponCatalog(weaponType, typeFilter, modeFilter);
			catalog.List(typeFilter, matches, modeFilter);
			areas.FilterWeapons(matches, weaponType, draft.GetPreview());
			bool correct = !matches.IsEmpty();
			foreach (string match : matches)
			{
				IEntity entity = draft.GetPreview().ResolvePreviewEntityForPrefab(match);
				BaseWeaponComponent component = BaseWeaponComponent.Cast(entity.FindComponent(BaseWeaponComponent));
				correct = correct && component && component.GetWeaponSlotType() == weaponType;
			}
			Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] faction=%1 weapon_type=%2 matches=%3", stableFaction, weaponType, matches.Count()));
			AICF_LoadoutClientCheck("CATALOG_WEAPON_SLOT_" + stableFaction + "_" + weaponType, correct);
		}
		Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] catalog=%1 all=%2 weapons=%3 ammunition=%4 uniforms=%5", stableFaction, all.Count(), weapons.Count(), ammo.Count(), uniforms.Count()));
		AICF_LoadoutClientCheck("CATALOG_WEAPONS_" + stableFaction, !weapons.IsEmpty());
		AICF_LoadoutClientCheck("CATALOG_AMMUNITION_" + stableFaction, !ammo.IsEmpty());
		AICF_LoadoutClientCheck("CATALOG_UNIFORMS_" + stableFaction, !uniforms.IsEmpty());
		if (!draft.GetPreview())
			return;
		array<string> samples = {};
		if (!weapons.IsEmpty())
			samples.Insert(weapons[0]);
		if (!ammo.IsEmpty())
			samples.Insert(ammo[0]);
		if (!uniforms.IsEmpty())
			samples.Insert(uniforms[0]);
		bool localPreviews = samples.Count() == 3;
		foreach (string prefab : samples)
		{
			IEntity preview = draft.GetPreview().ResolvePreviewEntityForPrefab(prefab);
			if (!preview || preview.GetWorld() != draft.GetCharacter().GetWorld() || !all.Contains(prefab))
				localPreviews = false;
		}
		AICF_LoadoutClientCheck("CATALOG_LOCAL_PREVIEW_RESOURCES_" + stableFaction, localPreviews);
	}

	protected void AICF_LoadoutCheckLightPose(AICF_LoadoutProbePreview preview, string step)
	{
		LightEntity light = preview.GetFillLight();
		if (!preview.GetWorld() || !light)
		{
			AICF_LoadoutClientCheck("PREVIEW_FILL_POSE_" + step, false);
			return;
		}
		vector camera[4], transform[4];
		preview.GetWorld().GetCamera(0, camera);
		light.GetWorldTransform(transform);
		vector toTarget = -transform[3];
		toTarget.Normalize();
		vector cameraSide = camera[3];
		cameraSide[1] = 0;
		cameraSide.Normalize();
		vector lightSide = transform[3];
		lightSide[1] = 0;
		lightSide.Normalize();
		// Проверяем фактическую ось entity, а не только переданный API аргумент.
		AICF_LoadoutClientCheck("PREVIEW_FILL_AIMS_AT_MODEL_" + step, vector.Dot(transform[2], toTarget) > 0.999);
		AICF_LoadoutClientCheck("PREVIEW_FILL_ON_CAMERA_SIDE_" + step, vector.Dot(cameraSide, lightSide) > 0.999);
		Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] light_pose=%1 position=%2 forward=%3 camera=%4", step, transform[3], transform[2], camera[3]));
	}

	protected void AICF_LoadoutLifetimeTick()
	{
		m_iAICFLifetimeTicks++;
		bool ready = m_AICFLifetimePreview && m_AICFLifetimePreview.IsReady();
		AICF_LoadoutClientCheck("PREVIEW_RETAINED_" + m_iAICFLifetimeTicks.ToString(), ready);
		AICF_LoadoutClientCheck("PREVIEW_FILL_RETAINED_" + m_iAICFLifetimeTicks.ToString(), ready && m_AICFLifetimePreview.HasFillLight());
		if (ready)
		{
			AICF_LoadoutCheckLightPose(m_AICFLifetimePreview, "RETAINED_" + m_iAICFLifetimeTicks.ToString());
			m_AICFLifetimePreview.Rotate(30);
			m_AICFLifetimePreview.Zoom(1);
			AICF_LoadoutCheckLightPose(m_AICFLifetimePreview, "ROTATED_" + m_iAICFLifetimeTicks.ToString());
		}
		if (m_iAICFLifetimeTicks < 3)
			return;
		if (m_AICFLifetimePreview)
			m_AICFLifetimePreview.Clear();
		AICF_LoadoutClientCheck("PREVIEW_CLEANUP", m_AICFLifetimePreview && !m_AICFLifetimePreview.GetWorld());
		AICF_LoadoutClientCheck("PREVIEW_LIGHTS_CLEANUP", m_AICFLifetimePreview && !m_AICFLifetimePreview.HasLighting());
		AICF_LoadoutClientCheck("PREVIEW_FILL_CLEANUP", m_AICFLifetimePreview && !m_AICFLifetimePreview.GetFillLight());
		m_iAICFLoadoutProbeStep = 6;
		AICF_LoadoutClientClose();
	}

	protected void AICF_LoadoutClientClose()
	{
		GetGame().GetCallqueue().Remove(AICF_LoadoutClientTick);
		GetGame().GetCallqueue().Remove(AICF_LoadoutClientClose);
		m_AICFLifetimePreview = null;
		m_AICFLifetimeDraft = null;
		AICF_LoadoutClientCheck("COMPLETED", m_iAICFLoadoutProbeStep == 6);
		Print(string.Format("[AICF][LOADOUT_CLIENT_PROBE] finished=1 failures=%1", m_iAICFLoadoutProbeFailures));
		string keepOpen;
		if (System.GetCLIParam("aicfLoadoutClientProbeKeepOpen", keepOpen) && keepOpen == "1")
			return;
		GetGame().RequestClose();
	}

	void ~SCR_GameModeCampaign()
	{
		if (!GetGame())
			return;
		GetGame().GetCallqueue().Remove(AICF_LoadoutClientTick);
		GetGame().GetCallqueue().Remove(AICF_LoadoutClientClose);
	}
}
