param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$failures = [Collections.Generic.List[string]]::new()
function Check([string]$rule, [bool]$condition) {
    if (-not $condition) { $failures.Add($rule) }
}
$addon = Join-Path $RepositoryRoot 'AIConflictArlandWCSRHS'
$project = Get-Content "$addon/addon.gproj" -Raw
$header = Get-Content "$addon/Missions/AICF_WCS_RHS_Conflict_Arland.conf" -Raw
$metadata = Get-Content "$addon/Missions/AICF_WCS_RHS_Conflict_Arland.conf.meta" -Raw
$catalog = Get-Content "$addon/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSVehicleCatalog.c" -Raw
$profile = Get-Content "$addon/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSRHSContentProfile.c" -Raw
$equipment = Get-Content "$addon/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSInfantryEquipment.c" -Raw
$kit = Get-Content "$addon/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSInfantryKit.c" -Raw
$playerKit = Get-Content "$addon/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSPlayerEquipment.c" -Raw
$pmc = Get-Content (Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Content/AICF_RHSPMCArmament.c') -Raw
$launcher = Get-Content (Join-Path $RepositoryRoot 'tools/Start-AICFRuntime.ps1') -Raw
Check 'WCS_PROJECT_ID' ($project -match 'GUID "A1CF260928100001"')
foreach ($dependency in @('6A717EEA63A35E67', '3B8081D9CAD979D2')) {
    Check "WCS_83_DEPENDENCY_$dependency" ($project.Contains('"' + $dependency + '"') -and $launcher.Contains($dependency))
}
foreach ($dependency in @('58D0FB3206B6F859','9178E5822AFE48EA','B52C5F6AEDBF423E','9F88011DA22B471C','615806DC6C57AF02','615818DA7C0343FD','629B2BA37EFFD577','5E389BB9F58B79A6','5D1880C4AD410C14','5E0AB16BEB16D6A4','65CF7AE8574E06D2','65F929DF622BAD50','6602C1EC7E5A4A87','6152CB0BD0684837','5B383D4CB27E0D54','63120AE07E6C0966')) {
    Check "WCS_DEPENDENCY_$dependency" ($project.Contains('"' + $dependency + '"'))
}
Check 'WCS_HEADER_INHERITANCE' ($header.Contains('{97E4BCB73F044C66}Missions/AICF_RHS_Conflict_Arland.conf'))
Check 'WCS_HEADER_ID' ($metadata.Contains('{A1CF260928100002}Missions/AICF_WCS_RHS_Conflict_Arland.conf'))
Check 'WCS_HEADER_MENU' ($header -match 'm_bShowInScenarioMenu\s+1')
Check 'WCS_HEADER_PERSISTENCE' ($header -match 'm_eSaveTypes\s+0')
Check 'WCS_WORLD_OWNERSHIP' ($header -notmatch '(?m)^\s*(World|SystemsConfig|m_aCampaignCustomBaseList)\b')
Check 'WCS_RHS_PROFILE_REUSE' ($profile -match 'class AICF_WCSRHSContentProfile : AICF_RHSContentProfile')
Check 'WCS_NO_SECOND_LIFECYCLE' (($profile + $catalog) -notmatch 'CallLater|OnGameStart|new AICF_MatchController|SpawnEntity|DeleteEntity|SetAffiliatedFaction|Replication.BumpMe')
Check 'WCS_ORIGINAL_ORDER' ($catalog -match 'foreach \(SCR_EntityCatalogEntry entry : existing\)[\s\S]*m_aEntityEntryList.Insert\(entry\);[\s\S]*foreach \(SCR_EntityCatalogEntry candidate : candidates\)')
Check 'WCS_ADMISSION' ($catalog -match '!candidate.IsEnabled\(\) \|\| !IsSupported\(prefab, faction\) \|\| known.Contains\(prefab\)')
Check 'WCS_US_CATALOG_ID' ($catalog.Contains('{D83A5EFEEF6CBF61}Configs/EntityCatalog/US/Vehicles_EntityCatalog_US.conf'))
Check 'WCS_USSR_CATALOG_ID' ($catalog.Contains('{09C2ED5F617A0390}Configs/EntityCatalog/USSR/Vehicles_EntityCatalog_USSR.conf'))
Check 'WCS_CLIENT_CATALOG_PARITY' ($catalog -notmatch 'Replication.IsServer')
Check 'WCS_NATIVE_INIT' ($catalog -match 'additions.Build[\s\S]*m_aEntityCatalogs.Insert\(additions\);[\s\S]*super.Init\(owner\);')
Check 'WCS_IFV_CATALOGS' ($catalog.Contains('Prefabs/Vehicles/Tracked/BMP3/') -and $catalog.Contains('Prefabs/Vehicles/Tracked/M2A2/'))
Check 'WCS_KIT_AUTHORITY' ($equipment.Contains('!Replication.IsServer()') -and $equipment.Contains('rpl.IsMaster()') -and $equipment.Contains('!controller.IsPlayerControlled()') -and $equipment.Contains('!controller.IsDead()'))
Check 'WCS_KIT_IDENTITY' ($equipment.Contains('character.GetID() != identity') -and $equipment.Contains('group.GetID() != groupIdentity') -and $equipment.Contains('GetParentGroup() == group') -and $equipment.Contains('current != before'))
Check 'WCS_KIT_DRAFT' ($equipment.Contains('draft.Build(recipe, catalog, reason)') -and $equipment.Contains('draft.Clear();') -and $equipment.Contains('model.GetWorld() == GetGame().GetWorld()'))
Check 'WCS_KIT_TRANSACTION' ($equipment.Contains('AICF_LoadoutInventory.Signature(character, catalog, ignored) == expected') -and $equipment.Contains('AICF_LoadoutInventory.Restore(character, before)'))
Check 'WCS_KIT_NO_DELAYED_OVERWRITE' ($equipment -notmatch 'CallLater|RequestSpawn|SetAffiliatedFaction|Replication.BumpMe' -and $equipment.Contains('character.m_bAICFWCSKitApplied'))
Check 'WCS_CACHE_BOUNDARY' ($profile.Contains('map<string, string>') -and $profile.Contains('faction + "|" + source') -and $profile.Contains('m_mKitSnapshots.Count() >= 64') -and $profile.Contains('m_mKitSnapshots.Contains(key)') -and $profile.Contains('override protected void Stop(bool cleanupEntities)') -and $profile.Contains('override void OnGameEnd()') -and $profile.Contains('profile.ClearPreparedKits();'))
Check 'WCS_CACHE_COMMIT_VALIDATION' ($equipment.Contains('profile.FindPreparedKit(faction, source, snapshot, expected)') -and $equipment.Contains('profile.StorePreparedKit(faction, source, snapshot, expected)') -and $equipment.Contains('ValidateWCS(character, faction, source)'))
Check 'WCS_KIT_ROLES_AND_AMMO' ($kit.Contains('_MG.') -and $kit.Contains('_AR.') -and $kit.Contains('_GL.') -and $equipment.Contains('GetMuzzlesList') -and $equipment.Contains('SpareMagazines(model, muzzle)'))
Check 'WCS_KIT_CARGO' ($equipment.Contains('CollectCargo(model, retained)') -and $equipment.Contains('StoreLocal(model, cargo, manager)'))
Check 'WCS_KIT_ROLE_SCOPE' ($equipment.Contains('AICF_WCSInfantryKit.SupportsRole(source)') -and $kit.Contains('static bool SupportsRole'))
Check 'WCS_PMC_COMPATIBILITY' ($pmc.Contains('profile.GetPMCPrimaryOverride(source, variant)') -and $profile.Contains('override ResourceName GetPMCPrimaryOverride') -and $pmc.Contains('muzzle.IsMuzzleSuppressed()') -and $pmc -notmatch 'WCS')
Check 'WCS_LAUNCHER' ($launcher.Contains("'ArlandWCSRHS'") -and $launcher.Contains("'A1CF260928100001' = 'AIConflictArlandWCSRHS'") -and $launcher.Contains('{A1CF260928100002}Missions/AICF_WCS_RHS_Conflict_Arland.conf'))
foreach ($name in @('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS')) {
    $other = Get-Content (Join-Path $RepositoryRoot "$name/addon.gproj") -Raw
    Check "WCS_OPTIONAL_$name" ($other -notmatch 'A1CF260928100001|615806DC6C57AF02|615818DA7C0343FD|5D1880C4AD410C14|5E0AB16BEB16D6A4')
}
Check 'WCS_NO_COPIED_ASSETS' (@(Get-ChildItem $addon -Recurse -File | Where-Object { $_.Extension -in '.et','.ent','.pak','.xob' }).Count -eq 0)
Check 'WCS_NO_PRODUCTION_PROBE' (@(Get-ChildItem "$addon/Scripts" -Recurse -File -Filter '*Probe*').Count -eq 0)
$itemCatalog = Get-Content "$addon/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSItemCatalog.c" -Raw
$building = Get-Content "$addon/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSBuildingVehicles.c" -Raw
$patches = Get-Content "$addon/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSPatches.c" -Raw
$inventory = Get-Content (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Loadouts/AICF_LoadoutInventory.c') -Raw
$editor = Get-Content (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_LoadoutEditor.c') -Raw
Check 'WCS_SHARED_DEFAULT' ($inventory -match 'PrepareDefaultLoadout[\s\S]*Signature\(m_Character[\s\S]*Replay\(m_Character' -and $profile.Contains('AICF_WCSInfantryEquipment.BuildDefault'))
Check 'WCS_PATCH_BASELINE' ($equipment.Contains('AICF_WCSPatches.ApplyLocal') -and $patches.Contains('IsPatchSlot') -and $patches.Contains('slot.GetAttachedEntity()') -and $patches.Contains('model.GetWorld() == GetGame().GetWorld()'))
Check 'WCS_COMPACT_BACKPACK' ($kit.Contains('Backpack_Rush12_MC.et') -and $kit.Contains('Backpack_Rush12_Olive.et') -and $kit -notmatch '6sh118|FILBE|ronin_1_AOR2')
Check 'WCS_ROLE_BACKPACKS' ($kit.Contains('static bool NeedsBackpack') -and ([regex]::Matches($kit, 'if \(NeedsBackpack\(faction, source\)\) clothes.Insert').Count -eq 2))
Check 'WCS_NO_DEFAULT_NVG' ($equipment.Contains('RemoveOptics(model, false)') -and $equipment.Contains('AICF_WCSInfantryKit.IsNightVision(prefab)) return false'))
Check 'WCS_BALLISTIC_EYEWEAR' ($kit.Contains('Eyewear_6b50_strap_on_6b47.et') -and $kit.Contains('Eyewear_6b50_wear_on_6b47.et') -and $kit.Contains('Eyewear_ess_crossbow_blk.et') -and $equipment.Contains('glasses.GetParentSlot().GetStorage().GetOwner() != expectedOwner'))
Check 'WCS_PLAYER_PREPOSSESSION' ($playerKit.Contains('modded class SCR_SpawnPointSpawnHandlerComponent') -and $playerKit.Contains('super.PrepareEntity_S(requestComponent, entity, data)') -and $playerKit.Contains('!controller.IsPlayerControlled()') -and $playerKit -notmatch 'override.*OnPlayerSpawned|override.*OnLoadoutSpawned|CallLater')
Check 'WCS_PLAYER_SAVED_EXCLUDED' ($playerKit.Contains('SCR_PlayerArsenalLoadout.Cast(loadout)') -and $playerKit.Contains('character.GetFactionKey() != loadout.GetFactionKey()'))
Check 'WCS_PLAYER_LEADER_BASELINE' ($playerKit.Contains('roster.ResolveRecruitPrefab(faction, 0, role)') -and $playerKit.Contains('role != "SQUAD_LEADER"') -and $playerKit.Contains('draft.Build(recipe, catalog, reason)'))
Check 'WCS_PLAYER_TRANSACTION' ($playerKit.Contains('rpl.IsMaster()') -and $playerKit.Contains('character.GetID() != identity') -and $playerKit.Contains('current != before') -and $playerKit.Contains('AICF_LoadoutInventory.Restore(character, before)') -and $playerKit.Contains('AICF_LoadoutInventory.Signature(character, catalog, ignored) == expected'))
Check 'WCS_ARSENAL_CATALOG' ($catalog.Contains('items.Build(GetFactionKey(), m_aEntityCatalogs)') -and $itemCatalog.Contains('EEntityCatalogType.ITEM') -and $itemCatalog.Contains('template.GetEntityDataOfType(SCR_ArsenalItem)'))
Check 'WCS_ARSENAL_PRESERVE_METADATA' ($itemCatalog.Contains('BaseContainerTools.CreateContainerFromInstance(template)') -and $itemCatalog -notmatch 'm_iSupplyCost|m_eRequiredRank')
Check 'WCS_SOURCE_TABS' ($editor.Contains('GetLoadoutSources(m_aSources)') -and $editor.Contains('action.StartsWith("source_")') -and $editor.Contains('m_aSources[m_iSourceIndex]'))
Check 'WCS_BUILDING_REGISTRY' ($building.Contains('modded class SCR_PlaceableEntitiesRegistry') -and $building.Contains('BaseContainerProps(configRoot: true)') -and $building.Contains('additions.Sort()') -and $building.Contains('prefabs.InsertAll(additions)'))
Check 'WCS_BUILDING_FACTION' ($building.Contains('modded class SCR_EditableVehicleUIInfo') -and $building.Contains('faction.GetFactionLabel()') -and $building.Contains('rpl.IsMaster()') -and $building.Contains('super.OnEntityCreatedServer(entities)'))
Check 'WCS_NATIVE_PLACEMENT_GATES' ($building -notmatch 'override.*CanPlaceEntity|override.*AreLabelsMatching|SpawnEntity|DeleteEntity|SetBudgetValue|CallLater')
$origins = Get-Content "$addon/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSItemOrigins.c" -Raw
Check 'WCS_CONCRETE_RESOURCE_SOURCE' ($origins.Contains('ResourceDatabase.SearchResources(filter, RecordResource)') -and $origins.Contains('filter.rootPath = "$" + addon + ":Prefabs"') -and $origins.Contains('!m_mSources.Contains(prefab)'))
Check 'WCS_SOURCE_PRECEDENCE' ($origins.IndexOf('AddAddon("ArmaReforger", "Vanilla")') -lt $origins.IndexOf('AddAddon(addon, "RHS")') -and $origins.IndexOf('AddAddon(addon, "RHS")') -lt $origins.IndexOf('AddAddon(addon, "WCS")'))
Check 'WCS_SOURCE_PROFILE_CACHE' ($profile.Contains('if (!m_LoadoutOrigins)') -and $profile.Contains('m_LoadoutOrigins.Find(prefab, source)') -and $profile -notmatch 'GetResourceAddons')
foreach ($failure in $failures) { Write-Output "[AICF][WCS_STATIC][FAIL] $failure" }
if ($failures.Count) { exit 1 }
Write-Output '[AICF][WCS_STATIC][PASS] scope=project_vehicle_catalog_infantry separate_runtime_gate=REQUIRED'
exit 0
