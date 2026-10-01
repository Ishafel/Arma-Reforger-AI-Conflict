// Только отдельный stage. Два полных roster через production async spawn.
modded class AICF_RHSPMCArmament
{
	override static bool Validate(IEntity model, ResourceName source, int variant)
	{
		bool valid = super.Validate(model, source, variant);
		if (!valid)
		{
			BaseWeaponComponent weapon = Primary(model);
			if (weapon && weapon.GetCurrentMuzzle())
			{
				BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
				Print(string.Format("[AICF][WCS_PMC_DIAG] actual=%1 expected=%2 ammo=%3 spare=%4 minimum=%5 suppressed=%6 source=%7", SCR_ResourceNameUtils.GetPrefabName(weapon.GetOwner()), PrimaryPrefab(source, variant), muzzle.GetAmmoCount(), SpareMagazines(model, muzzle), MinimumSpareMagazines(source), muzzle.IsMuzzleSuppressed(), source));
			}
		}
		return valid;
	}
}

modded class AICF_MatchController
{
	protected void AICF_WCSCacheCases()
	{
		AICF_WCSRHSContentProfile cache = new AICF_WCSRHSContentProfile();
		string snapshot;
		string signature;
		cache.StorePreparedKit("RHS_USAF", "role.et", "inventory", "signature");
		bool isolated = !cache.FindPreparedKit("RHS_AFRF", "role.et", snapshot, signature) &&
			!cache.FindPreparedKit("RHS_USAF", "other.et", snapshot, signature);
		cache.StorePreparedKit("RHS_USAF", "role.et", "overwrite", "overwrite");
		bool immutable = cache.FindPreparedKit("RHS_USAF", "role.et", snapshot, signature) && snapshot == "inventory" && signature == "signature";
		for (int index; index < 63; index++) cache.StorePreparedKit("RHS_USAF", index.ToString(), "inventory", "signature");
		cache.StorePreparedKit("RHS_USAF", "overflow.et", "inventory", "signature");
		bool bounded = !cache.FindPreparedKit("RHS_USAF", "overflow.et", snapshot, signature);
		AICF_WCSRHSContentProfile other = new AICF_WCSRHSContentProfile();
		bool fresh = !other.FindPreparedKit("RHS_USAF", "role.et", snapshot, signature);
		cache.ClearPreparedKits();
		bool cleared = !cache.FindPreparedKit("RHS_USAF", "role.et", snapshot, signature);
		Print(string.Format("[AICF][WCS_CACHE_RESULT] isolated=%1 immutable=%2 bounded=%3 fresh=%4 cleared=%5", isolated, immutable, bounded, fresh, cleared));
	}

	protected int m_iAICFWCSInfantryTicks;
	protected ref array<SCR_AIGroup> m_aAICFWCSInfantryGroups = {};
	protected ref array<SCR_ChimeraCharacter> m_aAICFWCSPMC = {};

	protected void AICF_WCSPlayerCases()
	{
		int passed;
		array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
		foreach (SCR_CampaignFaction faction : factions)
		{
			string role;
			ResourceName rifleman = m_GroupSpawner.ResolveRecruitPrefab(faction, 9, role);
			ResourceName leader = m_GroupSpawner.ResolveRecruitPrefab(faction, 0, role);
			EntitySpawnParams params();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = faction.GetMainBase().GetOwner().GetOrigin();
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefabEx(rifleman, false, params: params));
			SCR_FactionPlayerLoadout loadout = new SCR_FactionPlayerLoadout();
			loadout.m_sAffiliatedFaction = faction.GetFactionKey();
			SCR_PlayerArsenalLoadout saved = new SCR_PlayerArsenalLoadout();
			saved.m_sAffiliatedFaction = faction.GetFactionKey();
			bool savedExcluded = !AICF_WCSPlayerEquipment.IsDefault(saved);
			AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(faction);
			int ignored;
			string before = AICF_LoadoutInventory.Signature(character, catalog, ignored);
			SCR_FactionPlayerLoadout wrong = new SCR_FactionPlayerLoadout();
			wrong.m_sAffiliatedFaction = "FIA";
			bool rejected = !AICF_WCSPlayerEquipment.Apply(character, wrong) && before == AICF_LoadoutInventory.Signature(character, catalog, ignored);
			bool applied = AICF_WCSPlayerEquipment.Apply(character, loadout);
			AICF_LoadoutDraft draft = new AICF_LoadoutDraft();
			AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
			recipe.m_sCharacter = leader;
			recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction.GetFactionKey());
			string reason;
			bool equal = draft.Build(recipe, catalog, reason) && AICF_LoadoutInventory.Signature(character, catalog, ignored) == AICF_LoadoutInventory.Signature(draft.GetCharacter(), catalog, ignored);
			draft.Clear();
			if (savedExcluded && rejected && applied && equal) passed++;
			Print(string.Format("[AICF][WCS_PLAYER_CASE] faction=%1 applied=%2 leader_equal=%3 saved_excluded=%4 wrong_faction_rejected=%5", faction.GetFactionKey(), applied, equal, savedExcluded, rejected));
			if (character) RplComponent.DeleteRplEntity(character, false);
		}
		Print(string.Format("[AICF][WCS_PLAYER_RESULT] expected=2 passed=%1", passed));
	}

	protected void AICF_WCSProtectedItems(IEntity model, array<string> retained)
	{
		array<IEntity> items = {};
		AICF_RHSPMCArmament.Items(model, items);
		foreach (IEntity item : items)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
			if (prefab.Contains("/Medical/") || prefab.Contains("/Medicine/") || prefab.Contains("/Launchers/") ||
				prefab.Contains("/Grenades/") || prefab.Contains("/Radios/") || prefab.Contains("/Binoculars/")) retained.Insert(prefab);
		}
		retained.Sort();
	}

	override protected void Update()
	{
		super.Update();
		string enabled;
		if (!System.GetCLIParam("aicfWCSInfantryProbe", enabled) || enabled != "1" || !m_bRosterReady) return;
		m_iAICFWCSInfantryTicks++;
		if (m_iAICFWCSInfantryTicks == 1)
		{
			SCR_FactionManager manager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			SCR_Faction fia = SCR_Faction.Cast(manager.GetFactionByKey("FIA"));
			manager.SetFactionsFriendly(fia, m_USFaction);
			manager.SetFactionsFriendly(fia, m_USSRFaction);
			array<SCR_EntityCatalogEntry> entries = {};
			fia.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER).GetEntityList(entries);
			array<string> roles = {"Rifleman", "SL", "Medic", "MG", "AMG", "LAT", "AT", "AAT", "Sharpshooter", "RTO", "Sapper", "Ammo", "Scout"};
			foreach (int i, string role : roles)
			{
				foreach (SCR_EntityCatalogEntry entry : entries)
				{
					if (!entry.GetPrefab().EndsWith("/Character_FIA_" + role + ".et")) continue;
					EntitySpawnParams params();
					params.TransformMode = ETransformMode.WORLD;
					vector pos = m_USFaction.GetMainBase().GetOwner().GetOrigin() + Vector(30 + i * 2, 0, 30);
					pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
					params.Transform[3] = pos;
					SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefabEx(entry.GetPrefab(), false, params: params));
					m_aAICFWCSPMC.Insert(character);
					if (character)
					{
						AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
						if (control) control.DeactivateAI();
					}
					break;
				}
			}
			array<SCR_CampaignFaction> factions = {m_USFaction, m_USSRFaction};
			foreach (SCR_CampaignFaction faction : factions)
			{
				SCR_AIGroup group = m_GroupSpawner.SpawnGroup(faction, faction.GetMainBase(), 0, 10, false);
				m_aAICFWCSInfantryGroups.Insert(group);
				if (!group) continue;
				group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
				group.SetDeleteWhenEmpty(false);
				group.AICF_TrackMemberPositions();
				m_GroupSpawner.BeginRosterSpawn(group, 10);
			}
		}
		if (m_iAICFWCSInfantryTicks < 5) return;
		int total;
		foreach (SCR_AIGroup pending : m_aAICFWCSInfantryGroups) total += AICF_GroupRuntime.CountAliveAgents(pending);
		if (total < 20 && m_iAICFWCSInfantryTicks < 90) return;
		int passed;
		foreach (SCR_AIGroup group : m_aAICFWCSInfantryGroups)
		{
			if (!group) continue;
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (!character) continue;
				FactionKey faction = character.GetFactionKey();
				ResourceName source = SCR_ResourceNameUtils.GetPrefabName(character);
				RplComponent probeRpl = RplComponent.Cast(character.FindComponent(RplComponent));
				Print(string.Format("[WCS_ELIGIBLE] result=%1 faction=%2 role=%3 master=%4 player=%5 dead=%6 group=%7 active=%8 source=%9", AICF_WCSInfantryEquipment.EligibleWCS(character, group), faction, AICF_WCSInfantryKit.SupportsRole(source), probeRpl.IsMaster(), character.GetCharacterController().IsPlayerControlled(), character.GetCharacterController().IsDead(), agent.GetParentGroup() == group, AICF_ContentProfile.GetActive().GetProfileKey(), source));
				bool kit = character.m_bAICFWCSKitApplied && AICF_WCSInfantryEquipment.ValidateWCS(character, faction, source);
				AICF_RHSPMCEquipmentDraft draft = new AICF_RHSPMCEquipmentDraft();
				array<string> expected = {};
				array<string> actual = {};
				bool built = draft.BuildEquipment(source);
				if (built) AICF_WCSProtectedItems(draft.GetCharacter(), expected);
				AICF_WCSProtectedItems(character, actual);
				bool retained = built && expected.Count() == actual.Count();
				foreach (int i, string value : expected)
				{
					if (!actual.IsIndexValid(i) || actual[i] != value) retained = false;
				}
				draft.Clear();
				// Повторная выдача не пересоздаёт inventory.
				AICF_LoadoutCatalog catalog = new AICF_LoadoutCatalog(SCR_CampaignFaction.Cast(SCR_Faction.GetEntityFaction(character)));
				int ignored;
				string before = AICF_LoadoutInventory.Signature(character, catalog, ignored);
				bool idempotent = AICF_WCSInfantryEquipment.ApplyWCS(character, group) &&
					before == AICF_LoadoutInventory.Signature(character, catalog, ignored);
				int memberIndex = -1;
				for (int position; position < 10; position++)
				{
					if (group.AICF_GetSpawnMember(position) == character) memberIndex = position;
				}
				string role;
				SCR_CampaignFaction campaignFaction = SCR_CampaignFaction.Cast(SCR_Faction.GetEntityFaction(character));
				AICF_LoadoutRecipe recipe = new AICF_LoadoutRecipe();
				recipe.m_sCharacter = m_GroupSpawner.ResolveRecruitPrefab(campaignFaction, memberIndex, role);
				recipe.m_sFaction = AICF_ContentProfile.GetActive().GetStableFactionKey(faction);
				AICF_LoadoutDraft editor = new AICF_LoadoutDraft();
				string reason;
				bool editorEqual = memberIndex >= 0 && editor.Build(recipe, catalog, reason) &&
					before == AICF_LoadoutInventory.Signature(editor.GetCharacter(), catalog, ignored);
				editor.Clear();
				array<ResourceName> required = {AICF_WCSInfantryKit.Weapon(faction, source)};
				AICF_WCSInfantryKit.Clothes(faction, source, required);
				bool catalogComplete = true;
				foreach (ResourceName prefab : required)
				{
					if (!catalog.Allows(prefab))
					{
						catalogComplete = false;
						Print("[AICF][WCS_CATALOG_MISSING] prefab=" + prefab);
					}
				}
				int patches;
				int nightVision;
				int backpacks;
				int eyewear;
				array<IEntity> inventory = {};
				AICF_RHSPMCArmament.Items(character, inventory);
				foreach (IEntity item : inventory)
				{
					ResourceName itemPrefab = SCR_ResourceNameUtils.GetPrefabName(item);
					if (itemPrefab.Contains("/Patches/")) patches++;
					if (AICF_WCSInfantryKit.IsNightVision(itemPrefab)) nightVision++;
					if (itemPrefab.Contains("/Backpacks/")) backpacks++;
					if (itemPrefab == AICF_WCSInfantryKit.Eyewear(faction, source)) eyewear++;
				}
				bool backpackValid = backpacks == 0;
				if (AICF_WCSInfantryKit.NeedsBackpack(faction, source)) backpackValid = backpacks == 1;
				if (kit && retained && idempotent && editorEqual && catalogComplete && patches > 0 && nightVision == 0 && eyewear == 1 && backpackValid) passed++;
				Print(string.Format("[AICF][WCS_REALISM_CASE] faction=%1 member=%2 nvg=%3 eyewear=%4 backpacks=%5 backpack_valid=%6 raised=%7", faction, memberIndex, nightVision, eyewear, backpacks, backpackValid, AICF_WCSInfantryKit.EyewearOnHelmet(faction, source)));
				Print(string.Format("[AICF][WCS_EDITOR_CASE] faction=%1 member=%2 equal=%3 catalog=%4 patches=%5 reason=%6", faction, memberIndex, editorEqual, catalogComplete, patches, reason));
				Print(string.Format("[AICF][WCS_INFANTRY_CASE] faction=%1 kit=%2 protected=%3 idempotent=%4 expected_items=%5 actual_items=%6 source=%7", faction, kit, retained, idempotent, expected.Count(), actual.Count(), source));
			}
		}
		Print(string.Format("[AICF][WCS_INFANTRY_RESULT] expected=20 actual=%1 passed=%2", total, passed));
		int pmcPassed;
		foreach (SCR_ChimeraCharacter character : m_aAICFWCSPMC)
		{
			if (!character) continue;
			ResourceName source = SCR_ResourceNameUtils.GetPrefabName(character);
			bool valid = AICF_RHSPMCArmament.Validate(character, source, AICF_RHSPMCEquipment.Variant(source));
			if (valid) pmcPassed++;
			BaseWeaponComponent weapon = AICF_RHSPMCArmament.Primary(character);
			int ammo;
			bool suppressed;
			if (weapon && weapon.GetCurrentMuzzle())
			{
				ammo = weapon.GetCurrentMuzzle().GetAmmoCount();
				suppressed = weapon.GetCurrentMuzzle().IsMuzzleSuppressed();
			}
			Print(string.Format("[AICF][WCS_PMC_CASE] valid=%1 ammo=%2 suppressed=%3 source=%4", valid, ammo, suppressed, source));
		}
		Print(string.Format("[AICF][WCS_PMC_RESULT] expected=13 actual=%1 passed=%2", m_aAICFWCSPMC.Count(), pmcPassed));
		AICF_WCSPlayerCases();
		AICF_WCSCacheCases();
		GetGame().RequestClose();
	}
}
