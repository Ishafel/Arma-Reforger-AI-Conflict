param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure {
    param([string]$Rule, [string]$Message)
    $failures.Add("[$Rule] $Message")
}

function Require-Match {
    param([string]$Rule, [string]$Text, [string]$Pattern, [string]$Message)
    if ($Text -notmatch $Pattern) {
        Add-Failure $Rule $Message
    }
}

function Forbid-Match {
    param([string]$Rule, [string]$Text, [string]$Pattern, [string]$Message)
    if ($Text -match $Pattern) {
        Add-Failure $Rule $Message
    }
}

function Read-Tree {
    param([string]$RelativeRoot)
    $root = Join-Path $RepositoryRoot $RelativeRoot
    if (-not (Test-Path -LiteralPath $root)) {
        return ''
    }
    return (@(Get-ChildItem -LiteralPath $root -Recurse -File -Filter '*.c' |
        Sort-Object FullName |
        ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n")
}

$coreProjectPath = Join-Path $RepositoryRoot 'AIConflictCore/addon.gproj'
$arlandProjectPath = Join-Path $RepositoryRoot 'AIConflictArland/addon.gproj'
$rhsProjectPath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/addon.gproj'
$profilePath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Content/AICF_ContentProfile.c'
$groupSpawnerPath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_GroupSpawner.c'
$vehicleCatalogPath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCatalog.c'
$acquisitionPath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleAcquisitionFlow.c'
$arlandBootstrapPath = Join-Path $RepositoryRoot 'AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_ArlandCampaignBootstrap.c'
$radioNormalizerPath = Join-Path $RepositoryRoot 'AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_ArlandRadioBridgeNormalizer.c'
$rhsProfilePath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Content/AICF_RHSContentProfile.c'
$rhsBootstrapPath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Integration/AICF_RHSArlandBootstrap.c'
$rhsDeployIconFixPath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Integration/AICF_RHSDeployMapIconFix.c'
$rhsFactionVoiceFixPath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Integration/AICF_RHSFactionVoiceFix.c'
$rhsPersonnelCatalogFixPath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Integration/AICF_RHSPersonnelCatalogFix.c'
$rhsLoadoutUIFixPath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Integration/AICF_RHSLoadoutUIFix.c'

foreach ($path in @(
    $coreProjectPath, $arlandProjectPath, $rhsProjectPath, $profilePath,
    $groupSpawnerPath, $vehicleCatalogPath, $acquisitionPath,
    $arlandBootstrapPath, $radioNormalizerPath, $rhsProfilePath, $rhsBootstrapPath,
    $rhsDeployIconFixPath, $rhsFactionVoiceFixPath, $rhsPersonnelCatalogFixPath,
    $rhsLoadoutUIFixPath
)) {
    if (-not (Test-Path -LiteralPath $path)) {
        Add-Failure 'RHS_FILE_MISSING' "Missing required file $path"
    }
}

if ($failures.Count -eq 0) {
    $coreProject = Get-Content -LiteralPath $coreProjectPath -Raw
    $arlandProject = Get-Content -LiteralPath $arlandProjectPath -Raw
    $rhsProject = Get-Content -LiteralPath $rhsProjectPath -Raw
    $stockProfile = Get-Content -LiteralPath $profilePath -Raw
    $rhsProfile = Get-Content -LiteralPath $rhsProfilePath -Raw
    $groupSpawner = Get-Content -LiteralPath $groupSpawnerPath -Raw
    $vehicleCatalog = Get-Content -LiteralPath $vehicleCatalogPath -Raw
    $acquisition = Get-Content -LiteralPath $acquisitionPath -Raw
    $arlandBootstrap = Get-Content -LiteralPath $arlandBootstrapPath -Raw
    $radioNormalizer = Get-Content -LiteralPath $radioNormalizerPath -Raw
    $rhsBootstrap = Get-Content -LiteralPath $rhsBootstrapPath -Raw
    $rhsDeployIconFix = Get-Content -LiteralPath $rhsDeployIconFixPath -Raw
    $rhsFactionVoiceFix = Get-Content -LiteralPath $rhsFactionVoiceFixPath -Raw
    $rhsPersonnelCatalogFix = Get-Content -LiteralPath $rhsPersonnelCatalogFixPath -Raw
    $rhsLoadoutUIFix = Get-Content -LiteralPath $rhsLoadoutUIFixPath -Raw
    $coreSources = Read-Tree 'AIConflictCore/Scripts/Game'
    $rhsSources = Read-Tree 'AIConflictArlandRHS/Scripts/Game'

    foreach ($guid in @(
        '58D0FB3206B6F859', '9178E5822AFE48EA', 'B52C5F6AEDBF423E',
        '1337C0DE5DABBEEF', 'BADC0DEDABBEDA5E', '595F2BF2F44836FB'
    )) {
        Require-Match 'RHS_DEPENDENCY_GRAPH' $rhsProject ([regex]::Escape('"' + $guid + '"')) "RHS addon omits dependency $guid"
    }
    Require-Match 'RHS_ADDON_ID' $rhsProject 'GUID\s+"9F88011DA22B471C"' 'RHS addon permanent GUID changed or is missing'
    foreach ($forbidden in @('1337C0DE5DABBEEF', 'BADC0DEDABBEDA5E', '595F2BF2F44836FB')) {
        Forbid-Match 'RHS_OPTIONALITY' ($coreProject + $arlandProject) $forbidden "Stock dependency graph contains RHS GUID $forbidden"
        Forbid-Match 'RHS_CORE_ISOLATION' $coreSources $forbidden "Core source contains RHS GUID $forbidden"
    }
    Forbid-Match 'RHS_CORE_ISOLATION' $coreSources 'RHS_USAF|RHS_AFRF|RHS_USMC|VKPO_Demiseason|K4386' 'Core contains RHS-specific faction or prefab identifiers'

    Require-Match 'RHS_RADIO_BRIDGE_MAPPING' $radioNormalizer 'AICF_ContentProfile\.GetActive\(\)\.GetStableFactionKey\s*\(\s*faction\.GetFactionKey\(\)\s*\)' 'Arland radio normalization does not admit runtime RHS factions through stable side identity'
    Forbid-Match 'RHS_RADIO_BRIDGE_MAPPING' $radioNormalizer 'return\s+factionKey\s*==\s*"US"' 'Arland radio normalization still filters only literal stock faction keys'

    Require-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'modded\s+class\s+SCR_MapUISpawnPoint[\s\S]*override\s+void\s+UpdateIcon' 'RHS addon does not own its deployment-map spawn icon compatibility override'
    Require-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'GetStableFactionKey\s*\(\s*faction\.GetFactionKey\(\)\s*\)' 'RHS deploy-map icon fix does not resolve runtime sides through the content profile'
    Require-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'stableFactionKey\s*==\s*"US"[\s\S]*EMilitarySymbolIdentity\.BLUFOR[\s\S]*Friend_Select[\s\S]*Friend_Focus' 'RHS US deploy-map icon does not use complete BLUFOR quads'
    Require-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'stableFactionKey\s*==\s*"USSR"[\s\S]*EMilitarySymbolIdentity\.OPFOR[\s\S]*Hostile_Select[\s\S]*Hostile_Focus' 'RHS RF deploy-map icon does not use complete OPFOR quads'
    Require-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'ASSUMED_BLUFOR[\s\S]*return\s+EMilitarySymbolIdentity\.BLUFOR[\s\S]*ASSUMED_OPFOR[\s\S]*return\s+EMilitarySymbolIdentity\.OPFOR[\s\S]*ASSUMED_INDFOR[\s\S]*return\s+EMilitarySymbolIdentity\.INDFOR' 'RHS deploy-map icon fix does not normalize assumed military identities'
    Require-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'string\s+selection\s*=\s*"Unknown_Select"[\s\S]*string\s+highlight\s*=\s*"Unknown_Focus"' 'RHS deploy-map icon fix does not provide non-empty fallback quads'
    Forbid-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'super\.UpdateIcon\(\)' 'RHS deploy-map icon fix still delegates unrecognized identities to the empty-quad vanilla path'
    Require-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'modded\s+class\s+SCR_CampaignMapUIBase[\s\S]*override\s+void\s+SetImage' 'RHS addon does not repair campaign base icons whose faction keys contain underscores'
    Require-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'image\.StartsWith\(runtimePrefix\)[\s\S]*imageSuffix\.Replace\(runtimePrefix,\s*""\)' 'RHS base icon parser does not strip the complete runtime faction key before tokenization'
    Require-Match 'RHS_DEPLOY_MAP_ICONS' $rhsDeployIconFix 'string\s+selection\s*=\s*"Unknown_Select"[\s\S]*string\s+highlight\s*=\s*"Unknown_Installation_Focus_Land"' 'RHS base icon fix does not provide non-empty fallback quads'

    Require-Match 'RHS_FACTION_VOICE' $rhsFactionVoiceFix 'modded\s+class\s+SCR_Faction[\s\S]*override\s+int\s+GetIndentityVoiceSignal\s*\(\s*\)' 'RHS addon does not own its faction identity-voice compatibility override'
    Require-Match 'RHS_FACTION_VOICE' $rhsFactionVoiceFix 'GetFactionKey\s*\(\s*\)\s*==\s*"RHS_AFRF"[\s\S]*return\s+1\s*;' 'RHS_AFRF does not map to the stock USSR identity-voice signal'
    Require-Match 'RHS_FACTION_VOICE' $rhsFactionVoiceFix 'return\s+super\.GetIndentityVoiceSignal\s*\(\s*\)\s*;' 'Non-RHS factions do not preserve their configured identity-voice signal'
    Forbid-Match 'RHS_FACTION_VOICE' $rhsFactionVoiceFix 'RHS_USAF|GetStableFactionKey|SetFaction' 'RHS faction voice override exceeds its RHS_AFRF compatibility boundary'

    Require-Match 'RHS_PERSONNEL_CATALOG' $rhsPersonnelCatalogFix 'modded\s+class\s+SCR_CatalogEntitySpawnerComponent[\s\S]*override\s+protected\s+void\s+SetCurrentFactionCatalog' 'RHS addon does not own its stock personnel-service catalog adapter'
    Require-Match 'RHS_PERSONNEL_CATALOG' $rhsPersonnelCatalogFix 'super\.SetCurrentFactionCatalog\(\)[\s\S]*m_aCatalogTypes\.Count\(\)\s*!=\s*1[\s\S]*m_aCatalogTypes\.Contains\(EEntityCatalogType\.CHARACTER\)' 'RHS personnel adapter does not preserve vanilla behavior or constrain itself to character-only spawners'
    Require-Match 'RHS_PERSONNEL_CATALOG' $rhsPersonnelCatalogFix 'GetStableFactionKey[\s\S]*GetFactionEntityCatalogOfType\([\s\r\n]*EEntityCatalogType\.CHARACTER\)' 'RHS personnel adapter does not resolve the current RHS faction character catalog through stable identity'
    Require-Match 'RHS_PERSONNEL_CATALOG' $rhsPersonnelCatalogFix 'BuildCharacterRoleCandidates[\s\S]*GetEntityUiInfo\(\)[\s\S]*GetEntityDataOfType\(SCR_EntityCatalogSpawnerData\)' 'RHS personnel adapter does not reuse the supported role table or require stock spawner data and UI info'
    Require-Match 'RHS_PERSONNEL_CATALOG' $rhsPersonnelCatalogFix 'm_aAssetList\.Clear\(\)[\s\S]*m_aAssetList\.InsertAll\(personnelEntries\)[\s\S]*AssignUserActions\(\)' 'RHS personnel adapter does not atomically replace the filtered list and refresh stock user actions'
    Forbid-Match 'RHS_PERSONNEL_CATALOG' $rhsPersonnelCatalogFix 'Character_US_|Character_USSR_|AddAssetsFromCatalog\(' 'RHS personnel adapter contains stock fallback or bypasses the supported RHS role table'

    Require-Match 'RHS_LOADOUT_UI' $rhsLoadoutUIFix 'modded\s+class\s+SCR_LoadoutButton[\s\S]*override\s+void\s+SetLoadout' 'RHS addon does not own its factionless loadout UI compatibility guard'
    Require-Match 'RHS_LOADOUT_UI' $rhsLoadoutUIFix 'Faction\s+faction\s*=\s*entityUIInfo\.GetFaction\(\)[\s\S]*if\s*\(\s*!m_wBadge\s*\|\|\s*!faction\s*\)[\s\S]*faction\.GetFactionColor\(\)' 'RHS loadout UI guard does not validate the optional faction before painting the badge'
    Forbid-Match 'RHS_LOADOUT_UI' $rhsLoadoutUIFix 'entityUIInfo\.GetFaction\(\)\.GetFactionColor\(\)' 'RHS loadout UI still dereferences the optional faction directly'

    Require-Match 'CONTENT_PROFILES' $stockProfile 'class\s+AICF_ContentProfile' 'Stock content profile class is missing'
    Require-Match 'CONTENT_PROFILES' $stockProfile 'return\s+"STOCK"\s*;' 'Stock profile key is missing'
    Require-Match 'CONTENT_PROFILES' $rhsProfile 'class\s+AICF_RHSContentProfile\s*:\s*AICF_ContentProfile' 'RHS profile does not extend the Core boundary'
    Require-Match 'CONTENT_PROFILES' $arlandBootstrap 'AICF_CreateContentProfile\s*\(' 'Arland bootstrap lacks the stock profile factory'
    Require-Match 'CONTENT_PROFILES' $rhsBootstrap 'override\s+protected\s+AICF_ContentProfile\s+AICF_CreateContentProfile' 'RHS addon does not override only the profile factory'
    Require-Match 'RHS_DIAGNOSTIC_IDENTITY' $stockProfile 'message\.Replace\("="\s*\+\s*runtimeUS,\s*"=US"\)' 'Stable-side normalization must be limited to diagnostic field values'
    Require-Match 'RHS_DIAGNOSTIC_IDENTITY' $stockProfile 's_LastDiagnosticProfile' 'Late teardown diagnostics must retain the selected profile mapping'
    Forbid-Match 'RHS_DIAGNOSTIC_IDENTITY' $stockProfile 'message\.Replace\(runtimeUS,' 'Diagnostic normalization must not rewrite RHS identifiers inside prefab paths'

    Require-Match 'RHS_NO_STOCK_FALLBACK' $rhsProfile 'AllowsSourceRosterFallback[\s\S]*return\s+false\s*;' 'RHS profile must deny source-roster fallback'
    Require-Match 'RHS_NO_STOCK_FALLBACK' $groupSpawner 'AllowsSourceRosterFallback\(\)[\s\S]*CONTENT_ROLE_PREFAB_MISSING[\s\S]*fallback=DENIED[\s\S]*return\s+false' 'GroupSpawner must fail closed before RHS source-roster fallback'
    Forbid-Match 'RHS_NO_STOCK_FALLBACK' $rhsProfile 'Character_US_|Character_USSR_' 'RHS role mapping contains stock character suffixes'
    Require-Match 'RHS_GROUP_FACTION' $rhsProfile 'AllowsGroupFactionRebinding[\s\S]*return\s+true\s*;' 'RHS profile must explicitly authorize the empty controller affiliation repair'
    Require-Match 'RHS_GROUP_FACTION' $groupSpawner 'AllowsGroupFactionRebinding\(\)[\s\S]*group\.SetFaction\(faction\)[\s\S]*GROUP_FACTION_REBOUND[\s\S]*GROUP_FACTION_MISMATCH' 'Group affiliation repair must occur before the existing fail-closed guard and roster request'

    $lastRoleOffset = -1
    foreach ($role in @(
        'SQUAD_LEADER', 'MEDIC', 'MACHINE_GUNNER', 'ANTI_TANK', 'GRENADIER',
        'AUTOMATIC_RIFLEMAN', 'TEAM_LEADER', 'SENIOR_RIFLEMAN',
        'MACHINE_GUNNER_ASSISTANT', 'ANTI_TANK_ASSISTANT', 'RIFLEMAN'
    )) {
        $offset = $rhsProfile.IndexOf('role = "' + $role + '"', [System.StringComparison]::Ordinal)
        if ($offset -lt 0) {
            Add-Failure 'RHS_ROLE_MAPPING' "RHS deterministic table omits $role"
        }
        if ($role -notin @('SENIOR_RIFLEMAN') -and $offset -ge 0 -and $offset -lt $lastRoleOffset) {
            Add-Failure 'RHS_ROLE_MAPPING' "RHS role $role is outside stable member-index order"
        }
        if ($role -notin @('TEAM_LEADER')) {
            $lastRoleOffset = [Math]::Max($lastRoleOffset, $offset)
        }
    }
    foreach ($family in @('RHS_USAF_USMC_MEF', 'MSV/VKPO_Demiseason')) {
        Require-Match 'RHS_ROLE_MAPPING' $rhsProfile ([regex]::Escape($family)) "RHS roster omits catalog family $family"
    }
    Require-Match 'RHS_ROLE_DIAGNOSTICS' $groupSpawner 'GROUP_ROSTER_CONFIGURED[\s\S]*profile=%4[\s\S]*roles=%5[\s\S]*fallback_slots=%6[\s\S]*prefabs=%1' 'Roster diagnostics must publish profile, roles, fallback count and selected prefabs'

    foreach ($vehicle in @(
        'M998_covered_long_USAF.et', 'M923A1_transport.et',
        'M1025_armed_M2HB_USAF.et', 'K4386.et',
        'Ural4320_transport.et', 'K4386_Armed.et'
    )) {
        Require-Match 'RHS_VEHICLE_CANDIDATES' $rhsProfile ([regex]::Escape($vehicle)) "RHS vehicle profile omits $vehicle"
    }
    foreach ($capacity in @('accessibleSeats = 4;', 'accessibleSeats = 5;', 'accessibleSeats = 7;', 'accessibleSeats = 8;', 'accessibleSeats = 15;')) {
        Require-Match 'RHS_VEHICLE_METADATA' $rhsProfile ([regex]::Escape($capacity)) "RHS conservative capacity table omits $capacity"
    }
    Require-Match 'RHS_VEHICLE_BOUNDARY' $vehicleCatalog 'm_ContentProfile\.BuildVehicleSuffixPreference' 'VehicleCatalog does not delegate candidate paths to the profile'
    Require-Match 'RHS_VEHICLE_BOUNDARY' $vehicleCatalog 'm_ContentProfile\.TryGetConservativeVehicleCapacity' 'VehicleCatalog does not delegate metadata to the profile'
    $spawnOffset = $acquisition.IndexOf('TrySpawnSelectedSiteForAcquisition', [System.StringComparison]::Ordinal)
    $liveOffset = $acquisition.IndexOf('InspectVehicleCapacity', $spawnOffset, [System.StringComparison]::Ordinal)
    if ($spawnOffset -lt 0 -or $liveOffset -le $spawnOffset) {
        Add-Failure 'RHS_LIVE_CAPACITY' 'Live compartment validation must occur after candidate entity spawn'
    }

    Forbid-Match 'RHS_SINGLE_LIFECYCLE' $rhsSources '\bOnGameStart\s*\(' 'RHS addon must not add a second OnGameStart lifecycle'
    Forbid-Match 'RHS_SINGLE_LIFECYCLE' $rhsSources 'new\s+AICF_MatchController|CallLater\s*\(|GetOn\w*\(\)\.Insert\s*\(' 'RHS addon must not add a controller, loop or event subscription'
    $controllerCount = ([regex]::Matches($arlandBootstrap, 'new\s+AICF_MatchController\s*\(')).Count
    if ($controllerCount -ne 1) {
        Add-Failure 'RHS_SINGLE_LIFECYCLE' "Arland bootstrap must construct exactly one controller; found $controllerCount"
    }
    Require-Match 'RHS_LIFECYCLE_CLEANUP' $arlandBootstrap 'GetOnStarted\(\)\.Insert\(AICF_OnConflictStarted\)[\s\S]*GetOnStarted\(\)\.Remove\(AICF_OnConflictStarted\)' 'Conflict-start subscription lacks matching removal'
    Require-Match 'RHS_LIFECYCLE_CLEANUP' $arlandBootstrap 'GetOnAllBasesInitialized\(\)\.Insert\(AICF_OnAllBasesInitialized\)[\s\S]*GetOnAllBasesInitialized\(\)\.Remove\(AICF_OnAllBasesInitialized\)' 'Base-ready subscription lacks matching removal'
    Require-Match 'RHS_LIFECYCLE_CLEANUP' $arlandBootstrap 'AICF_ContentProfile\.ClearActive\(m_AICFContentProfile\)' 'Active content profile lacks OnGameEnd cleanup'
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "[AICF][RHS_STATIC][FAIL] $failure"
    }
    Write-Output "[AICF][RHS_STATIC][RESULT][FAIL] issues=$($failures.Count)"
    exit 1
}

Write-Output '[AICF][RHS_STATIC][RESULT][PASS] dependency_graph=PASS core_isolation=PASS profiles=PASS roles=PASS vehicles=PASS personnel_catalog=PASS loadout_ui=PASS radio_bridge_mapping=PASS deploy_map_icons=PASS faction_voice=PASS lifecycle=PASS cleanup=PASS'
exit 0
