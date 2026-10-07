// Только изолированная terminal fixture. 1 — осмотр, 2 — smoke с остановкой.
// Обычный addon этот файл не загружает.
modded class AICF_MatchController
{
	protected bool m_bAICFWardrobeBuilt;
	protected int m_iAICFWardrobeTicks;
	protected vector m_vAICFWardrobeStart;
	protected ref array<IEntity> m_aAICFWardrobeModels = {};
	protected ref array<string> m_aAICFWardrobeCapabilities = {};

	protected bool AICF_WardrobeEnabled()
	{
		string mode;
		return System.GetCLIParam("aicfWardrobeShowcase", mode) && (mode == "1" || mode == "2");
	}

	override protected void Update()
	{
		if (!AICF_WardrobeEnabled() || !m_bRosterReady)
		{
			super.Update();
			return;
		}
		if (!m_bAICFWardrobeBuilt)
		{
			m_bAICFWardrobeBuilt = true;
			AICF_BuildWardrobe();
		}
		m_iAICFWardrobeTicks++;
		foreach (IEntity model : m_aAICFWardrobeModels)
		{
			if (!model) continue;
			AIControlComponent control = AIControlComponent.Cast(model.FindComponent(AIControlComponent));
			if (control) control.DeactivateAI();
		}
		AICF_MoveWardrobeVisitors();
		if (m_iAICFWardrobeTicks == 8)
			AICF_CheckWardrobe();
		string mode;
		if (m_iAICFWardrobeTicks == 15 && System.GetCLIParam("aicfWardrobeShowcase", mode) && mode == "2")
			GetGame().RequestClose();
	}

	override protected void CommanderTick()
	{
		if (!AICF_WardrobeEnabled() || !m_bRosterReady) super.CommanderTick();
	}

	override protected void ReliabilityTick()
	{
		if (!AICF_WardrobeEnabled() || !m_bRosterReady) super.ReliabilityTick();
	}

	protected bool AICF_RowClear(vector start, int count)
	{
		BaseWorld world = GetGame().GetWorld();
		float initialY = world.GetSurfaceY(start[0], start[2]);
		for (int i; i < count; i++)
		{
			vector point = start + Vector(i * 2.5, 0, 0);
			point[1] = world.GetSurfaceY(point[0], point[2]);
			if (Math.AbsFloat(point[1] - initialY) > 1.5 || ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, point)) return false;
			TraceBox trace = new TraceBox();
			trace.Start = point + "0 0.2 -1";
			trace.End = trace.Start;
			trace.Mins = "-0.8 0 -4";
			trace.Maxs = "0.8 2 1.8";
			trace.Flags = TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;
			if (world.TracePosition(trace, null) < 0) return false;
		}
		return true;
	}

	protected void AICF_BuildWardrobe()
	{
		SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		SCR_Faction fia = SCR_Faction.Cast(factions.GetFactionByKey("FIA"));
		factions.SetFactionsFriendly(m_USFaction, m_USSRFaction);
		factions.SetFactionsFriendly(m_USFaction, fia);
		factions.SetFactionsFriendly(m_USSRFaction, fia);
		array<ResourceName> prefabs = {};
		for (int roleIndex; roleIndex < 10; roleIndex++)
		{
			string role;
			prefabs.Insert(m_GroupSpawner.ResolveRecruitPrefab(m_USSRFaction, roleIndex, role));
		}
		SCR_EntityCatalog catalog = fia.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER);
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);
		array<string> roles = {"Rifleman", "SL", "Medic", "MG", "AMG", "LAT", "AT", "AAT", "Sharpshooter", "RTO", "Sapper", "Ammo", "Scout"};
		foreach (string suffix : roles)
		{
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				if (entry.GetPrefab().EndsWith("/Character_FIA_" + suffix + ".et"))
				{
					prefabs.Insert(entry.GetPrefab());
					break;
				}
			}
		}
		IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(m_USState.GetSlot(0).GetGroup());
		if (!leader) return;
		vector anchor = leader.GetOrigin();
		bool found;
		for (int ring = 1; ring <= 30 && !found; ring++)
		{
			for (int direction; direction < 8 && !found; direction++)
			{
				float angle = direction * 45 * Math.DEG2RAD;
				vector start = anchor + Vector(Math.Cos(angle) * ring * 20 - prefabs.Count() * 1.25, 0, Math.Sin(angle) * ring * 20);
				if (AICF_RowClear(start, prefabs.Count()))
				{
					m_vAICFWardrobeStart = start;
					found = true;
				}
			}
		}
		if (!found)
		{
			Print("[AICF][WARDROBE_FAILED] reason=NO_CLEAR_ROW", LogLevel.ERROR);
			return;
		}
		for (int index; index < prefabs.Count(); index++)
		{
			EntitySpawnParams spawn = new EntitySpawnParams();
			spawn.TransformMode = ETransformMode.WORLD;
			Math3D.AnglesToMatrix("180 0 0", spawn.Transform);
			spawn.Transform[3] = m_vAICFWardrobeStart + Vector(index * 2.5, 0, 0);
			spawn.Transform[3][1] = GetGame().GetWorld().GetSurfaceY(spawn.Transform[3][0], spawn.Transform[3][2]) + 0.05;
			IEntity model = GetGame().SpawnEntityPrefab(Resource.Load(prefabs[index]), null, spawn);
			if (!model) continue;
			AIControlComponent control = AIControlComponent.Cast(model.FindComponent(AIControlComponent));
			if (control) control.DeactivateAI();
			DamageManagerComponent damage = DamageManagerComponent.Cast(model.FindComponent(DamageManagerComponent));
			if (damage) damage.EnableDamageHandling(false);
			m_aAICFWardrobeModels.Insert(model);
			m_aAICFWardrobeCapabilities.Insert(AICF_WardrobeCapability(model));
			AICF_WardrobeInventoryAudit.Dump("before", index + 1, model);
			Print(string.Format("[AICF][WARDROBE_MODEL] index=%1 entity=%2 prefab=%3 position=%4", index + 1, model.GetID(), prefabs[index], model.GetOrigin()));
		}
		Print(string.Format("[AICF][WARDROBE_READY] expected=%1 actual=%2 start=%3 spacing=2.5 ai=disabled test_only=1", prefabs.Count(), m_aAICFWardrobeModels.Count(), m_vAICFWardrobeStart));
	}

	protected void AICF_MoveWardrobeVisitors()
	{
		if (m_aAICFWardrobeModels.IsEmpty()) return;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (!player) continue;
			vector position = m_vAICFWardrobeStart + "0 0 -5";
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.1;
			player.AICF_WardrobeVisit(position);
		}
	}

	protected string AICF_WardrobeCapability(IEntity model)
	{
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
		array<IEntity> items = {};
		array<string> capability = {};
		manager.GetItems(items);
		BaseWeaponComponent primary;
		if (SCR_Faction.GetEntityFaction(model).GetFactionKey() == "FIA")
			primary = AICF_RHSPMCArmament.Primary(model);
		foreach (IEntity item : items)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			// Основное оружие ЧВК и его калибр теперь меняются по запросу.
			// Независимо сохраняем медицину, ПТ, пистолет и боезапас помощников.
			if (primary && (primary.GetOwner() == item || AICF_RHSPMCArmament.Compatible(magazine, primary.GetCurrentMuzzle()))) continue;
			if (primary && SCR_ResourceNameUtils.GetPrefabName(model).EndsWith("_AMG.et") &&
				(prefab.Contains("/UK59/Box_") || prefab == AICF_RHSPMCArmament.SupportMagazinePrefab())) continue;
			if (magazine)
				capability.Insert(prefab + ":" + magazine.GetAmmoCount().ToString());
			else if (item.FindComponent(BaseWeaponComponent) || prefab.Contains("/Medicine/"))
				capability.Insert(prefab);
		}
		capability.Sort();
		return SCR_StringHelper.Join(";", capability);
	}

	protected void AICF_CheckWardrobe()
	{
		foreach (int index, IEntity model : m_aAICFWardrobeModels)
		{
			if (!model) continue;
			AICF_WardrobeInventoryAudit.Dump("after", index + 1, model);
			InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(model.FindComponent(InventoryStorageManagerComponent));
			array<IEntity> items = {};
			manager.GetItems(items);
			string reason;
			bool usable = AICF_LoadoutInventory.HasUsableWeapons(model, reason);
			AIControlComponent control = AIControlComponent.Cast(model.FindComponent(AIControlComponent));
			bool capability = AICF_WardrobeCapability(model) == m_aAICFWardrobeCapabilities[index];
			Print(string.Format("[AICF][WARDROBE_CHECK] entity=%1 faction=%2 usable_weapon=%3 items=%4 capability_preserved=%5 ai_active=%6", model.GetID(), SCR_Faction.GetEntityFaction(model).GetFactionKey(), usable, items.Count(), capability, control.IsAIActivated()));
			if (SCR_Faction.GetEntityFaction(model).GetFactionKey() == "FIA")
			{
				ResourceName source = SCR_ResourceNameUtils.GetPrefabName(model);
				BaseWeaponComponent weapon = AICF_RHSPMCArmament.Primary(model);
				BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
				Print(string.Format("[AICF][PMC_ARMAMENT_CHECK] entity=%1 passed=%2 loaded=%3 suppressed=%4 spare_magazines=%5 primary=%6", model.GetID(), AICF_RHSPMCArmament.Validate(model, source, AICF_RHSPMCEquipment.Variant(source)), muzzle.GetAmmoCount(), muzzle.IsMuzzleSuppressed(), AICF_RHSPMCArmament.SpareMagazines(model, muzzle), SCR_ResourceNameUtils.GetPrefabName(weapon.GetOwner())));
			}
			foreach (IEntity item : items)
				Print(string.Format("[AICF][WARDROBE_ITEM] entity=%1 prefab=%2", model.GetID(), SCR_ResourceNameUtils.GetPrefabName(item)));
		}
	}
}

// Полный read-only аудит всех стволов, включая пистолет, ГП и ПТ.
// До/после позволяет отдельно заметить потерю снаряжения при смене одежды.
class AICF_WardrobeInventoryAudit
{
	static int ChamberedRounds(BaseMagazineComponent magazine, array<IEntity> items)
	{
		if (!magazine) return 0;
		foreach (IEntity item : items)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (!weapon) continue;
			array<BaseMuzzleComponent> muzzles = {};
			weapon.GetMuzzlesList(muzzles);
			foreach (BaseMuzzleComponent muzzle : muzzles)
			{
				if (muzzle.GetMagazine() != magazine) continue;
				int count;
				for (int barrel; barrel < muzzle.GetBarrelsCount(); barrel++)
					if (muzzle.IsBarrelChambered(barrel)) count++;
				return count;
			}
		}
		return 0;
	}

	static void Dump(string phase, int index, IEntity model)
	{
		array<IEntity> items = {};
		AICF_RHSPMCArmament.Items(model, items);
		Print(string.Format("[AICF][INVENTORY_MODEL] phase=%1 index=%2 count=%3 source=%4", phase, index, items.Count(), SCR_ResourceNameUtils.GetPrefabName(model)));
		foreach (IEntity item : items)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			string well = "NONE";
			int ammo = -1;
			int capacity = -1;
			if (magazine)
			{
				ammo = magazine.GetAmmoCount();
				capacity = magazine.GetMaxAmmoCount();
				if (magazine.GetMagazineWell()) well = magazine.GetMagazineWell().Type().ToString();
			}
			bool cloth = item.FindComponent(BaseLoadoutClothComponent) != null;
			Print(string.Format("[AICF][INVENTORY_ITEM] phase=%1 index=%2 item=%3 cloth=%4 well=%5 ammo=%6 capacity=%7 chambered=%8 prefab=%9", phase, index, item.GetID(), cloth, well, ammo, capacity, ChamberedRounds(magazine, items), prefab));
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (!weapon) continue;
			array<BaseMuzzleComponent> muzzles = {};
			weapon.GetMuzzlesList(muzzles);
			foreach (int muzzleIndex, BaseMuzzleComponent muzzle : muzzles)
			{
				int spares;
				int spareRounds;
				well = "NONE";
				if (muzzle.GetMagazineWell()) well = muzzle.GetMagazineWell().Type().ToString();
				foreach (IEntity candidate : items)
				{
					BaseMagazineComponent spare = BaseMagazineComponent.Cast(candidate.FindComponent(BaseMagazineComponent));
					if (!AICF_RHSPMCArmament.Compatible(spare, muzzle) || spare == muzzle.GetMagazine()) continue;
					if (spare.GetAmmoCount() > 0) spares++;
					spareRounds += spare.GetAmmoCount();
				}
				string line = string.Format("[AICF][INVENTORY_MUZZLE] phase=%1 index=%2 item=%3 muzzle=%4 well=%5 loaded=%6 spares=%7 spare_rounds=%8 disposable=%9", phase, index, item.GetID(), muzzleIndex, well, muzzle.GetAmmoCount(), spares, spareRounds, muzzle.IsDisposable());
				Print(line + " prefab=" + prefab);
			}
		}
	}
}

// Движение управляемого character принадлежит owner. Серверный TeleportPlayer
// менял только серверную копию, которую затем перезаписывал клиент.
modded class SCR_PlayerController
{
	protected IEntity m_AICFWardrobeVisitor;
	protected int m_iAICFWardrobeVisitTicks;
	protected int m_iAICFWardrobeVisitAttempts;
	protected int m_iAICFWardrobeNearSamples;
	protected bool m_bAICFWardrobeVisitConfirmed;

	void AICF_WardrobeVisit(vector position)
	{
		if (!Replication.IsServer()) return;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetControlledEntity());
		if (!character || character.GetCharacterController().IsDead()) return;
		if (m_AICFWardrobeVisitor != character)
		{
			m_AICFWardrobeVisitor = character;
			m_iAICFWardrobeVisitTicks = 0;
			m_iAICFWardrobeVisitAttempts = 0;
			m_iAICFWardrobeNearSamples = 0;
			m_bAICFWardrobeVisitConfirmed = false;
		}
		if (m_bAICFWardrobeVisitConfirmed) return;
		m_iAICFWardrobeVisitTicks++;
		// Дать штатному deployment завершить установку transform.
		if (m_iAICFWardrobeVisitTicks < 5) return;
		float distance = vector.Distance(character.GetOrigin(), position);
		if (m_iAICFWardrobeVisitAttempts > 0 && distance < 3)
		{
			m_iAICFWardrobeNearSamples++;
			if (m_iAICFWardrobeNearSamples >= 2)
			{
				m_bAICFWardrobeVisitConfirmed = true;
				Print(string.Format("[AICF][WARDROBE_VISITOR_VERIFIED] peer=server entity=%1 actual=%2 target=%3 distance_m=%4", Replication.FindItemId(character), character.GetOrigin(), position, distance));
			}
			return;
		}
		m_iAICFWardrobeNearSamples = 0;
		if (m_iAICFWardrobeVisitTicks % 5 != 0 || m_iAICFWardrobeVisitAttempts >= 3) return;
		m_iAICFWardrobeVisitAttempts++;
		Rpc(AICF_WardrobeTeleportOwner, Replication.FindItemId(character), position);
		Print(string.Format("[AICF][WARDROBE_VISITOR_REQUEST] entity=%1 attempt=%2 before=%3 target=%4", Replication.FindItemId(character), m_iAICFWardrobeVisitAttempts, character.GetOrigin(), position));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AICF_WardrobeTeleportOwner(RplId identity, vector position)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetControlledEntity());
		if (GetGame().GetPlayerController() != this || !character ||
			Replication.FindItemId(character) != identity || character.GetCharacterController().IsDead()) return;
		if (!SCR_Global.TeleportLocalPlayer(position)) return;
		GetGame().GetCallqueue().Remove(AICF_WardrobeVerifyOwner);
		GetGame().GetCallqueue().CallLater(AICF_WardrobeVerifyOwner, 2000, false, identity, position);
	}

	protected void AICF_WardrobeVerifyOwner(RplId identity, vector position)
	{
		IEntity character = GetControlledEntity();
		if (GetGame().GetPlayerController() != this || !character || Replication.FindItemId(character) != identity) return;
		float distance = vector.Distance(character.GetOrigin(), position);
		Print(string.Format("[AICF][WARDROBE_VISITOR_CHECK] peer=client entity=%1 actual=%2 target=%3 distance_m=%4 passed=%5", identity, character.GetOrigin(), position, distance, distance < 3));
	}

	void ~SCR_PlayerController()
	{
		if (GetGame()) GetGame().GetCallqueue().Remove(AICF_WardrobeVerifyOwner);
	}
}
