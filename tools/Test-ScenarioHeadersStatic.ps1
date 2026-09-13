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

$stockHeaderPath = Join-Path $RepositoryRoot 'AIConflictArland/Missions/AICF_Conflict_Arland.conf'
$stockMetaPath = $stockHeaderPath + '.meta'
$everonProjectPath = Join-Path $RepositoryRoot 'AIConflictEveron/addon.gproj'
$everonHeaderPath = Join-Path $RepositoryRoot 'AIConflictEveron/Missions/AICF_Conflict_Everon.conf'
$everonMetaPath = $everonHeaderPath + '.meta'
$rhsHeaderPath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Missions/AICF_RHS_Conflict_Arland.conf'
$rhsMetaPath = $rhsHeaderPath + '.meta'
$rhsEveronProjectPath = Join-Path $RepositoryRoot 'AIConflictEveronRHS/addon.gproj'
$rhsEveronHeaderPath = Join-Path $RepositoryRoot 'AIConflictEveronRHS/Missions/AICF_RHS_Conflict_Everon.conf'
$rhsEveronMetaPath = $rhsEveronHeaderPath + '.meta'

foreach ($path in @($stockHeaderPath, $stockMetaPath, $everonProjectPath, $everonHeaderPath, $everonMetaPath, $rhsHeaderPath, $rhsMetaPath, $rhsEveronProjectPath, $rhsEveronHeaderPath, $rhsEveronMetaPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        Add-Failure 'SCENARIO_FILE_MISSING' "Missing required scenario resource $path"
    }
}

if ($failures.Count -eq 0) {
    $stockHeader = Get-Content -LiteralPath $stockHeaderPath -Raw
    $stockMeta = Get-Content -LiteralPath $stockMetaPath -Raw
    $everonProject = Get-Content -LiteralPath $everonProjectPath -Raw
    $everonHeader = Get-Content -LiteralPath $everonHeaderPath -Raw
    $everonMeta = Get-Content -LiteralPath $everonMetaPath -Raw
    $rhsHeader = Get-Content -LiteralPath $rhsHeaderPath -Raw
    $rhsMeta = Get-Content -LiteralPath $rhsMetaPath -Raw
    $rhsEveronProject = Get-Content -LiteralPath $rhsEveronProjectPath -Raw
    $rhsEveronHeader = Get-Content -LiteralPath $rhsEveronHeaderPath -Raw
    $rhsEveronMeta = Get-Content -LiteralPath $rhsEveronMetaPath -Raw

    Require-Match 'SCENARIO_STOCK_PARENT' $stockHeader `
        'SCR_MissionHeaderCampaign\s*:\s*"\{C41618FD18E9D714\}Missions/23_Campaign_Arland\.conf"' `
        'Stock scenario must inherit the official Conflict - Arland mission header'
    Require-Match 'SCENARIO_RHS_PARENT' $rhsHeader `
        'SCR_MissionHeaderCampaign\s*:\s*"\{7577640CD42A00BD\}Missions/RHS_Conflict_Arland\.conf"' `
        'RHS scenario must inherit the official RHS Conflict - Arland mission header'
    Require-Match 'SCENARIO_EVERON_PARENT' $everonHeader `
        'SCR_MissionHeaderCampaign\s*:\s*"\{ECC61978EDCC2B5A\}Missions/23_Campaign\.conf"' `
        'Everon scenario must inherit the official Conflict - Everon mission header'
    Require-Match 'SCENARIO_EVERON_PROJECT' $everonProject `
        'ID\s+"AIConflictEveron"[\s\S]*GUID\s+"A4B2E62595F645A4"[\s\S]*Dependencies\s*\{[\s\S]*"58D0FB3206B6F859"[\s\S]*"9178E5822AFE48EA"[\s\S]*"B52C5F6AEDBF423E"' `
        'Everon project must keep its stable identity and load the reviewed stock integration graph'

    Require-Match 'SCENARIO_RHS_EVERON_PARENT' $rhsEveronHeader `
        'SCR_MissionHeaderCampaign\s*:\s*"\{AAD43C10045857C1\}Missions/RHS_Conflict\.conf"' `
        'RHS Everon must inherit the official full-island RHS Conflict mission'
    Require-Match 'SCENARIO_RHS_EVERON_PROJECT' $rhsEveronProject `
        'ID\s+"AIConflictEveronRHS"[\s\S]*GUID\s+"FA9FDCCA428A43BA"' `
        'RHS Everon must keep its stable project identity'
    $expectedDependencies = @('58D0FB3206B6F859', '9178E5822AFE48EA', 'B52C5F6AEDBF423E',
        'A4B2E62595F645A4', '1337C0DE5DABBEEF', 'BADC0DEDABBEDA5E', '595F2BF2F44836FB', '9F88011DA22B471C')
    $dependencyBlock = [regex]::Match($rhsEveronProject, 'Dependencies\s*\{([^}]*)\}').Groups[1].Value
    $actualDependencies = @([regex]::Matches($dependencyBlock, '"([A-F0-9]{16})"') | ForEach-Object { $_.Groups[1].Value })
    if (($actualDependencies -join ',') -cne ($expectedDependencies -join ',')) {
        Add-Failure 'SCENARIO_RHS_EVERON_GRAPH' 'RHS Everon must compose the existing Everon radio policy and RHS content/compatibility graph'
    }
    foreach ($platform in @('PC', 'HEADLESS', 'XBOX_ONE', 'XBOX_SERIES', 'PS4')) {
        Require-Match 'SCENARIO_PLATFORM_CONFIG' $rhsEveronProject `
            ("GameProjectConfig\s+" + [regex]::Escape($platform) + '\b') `
            "RHS Everon project omits $platform configuration"
    }

    foreach ($scenario in @($stockHeader, $everonHeader, $rhsHeader, $rhsEveronHeader)) {
        Require-Match 'SCENARIO_MENU_VISIBILITY' $scenario `
            'm_bShowInScenarioMenu\s+1' `
            'Scenario header must be visible in the in-game Scenarios menu'
        Require-Match 'SCENARIO_PERSISTENCE_DISABLED' $scenario `
            '(?m)^\s*m_eSaveTypes\s+0\s*$' `
            'Scenario header must disable unsupported session persistence so in-game hosting always starts a fresh campaign'
        Require-Match 'SCENARIO_IDENTITY' $scenario `
            'm_sName\s+"AI Conflict[^"\r\n]*"[\s\S]*m_sGameMode\s+"AI Conflict"' `
            'Scenario header must expose an AI Conflict name and game-mode label'
        Require-Match 'SCENARIO_RANK_UNLOCKS' $scenario `
            'm_bIgnoreMinimumVehicleRank\s+1' `
            'Scenario header must disable vehicle rank requirements'
        Require-Match 'SCENARIO_RANK_UNLOCKS' $scenario `
            'm_eStartingRank\s+GENERAL' `
            'Scenario header must grant the highest stock rank for building and all other rank-gated actions'
        Forbid-Match 'SCENARIO_VANILLA_OWNERSHIP' $scenario `
            '(?m)^\s*(World|SystemsConfig|m_aCampaignCustomBaseList)\b' `
            'Scenario header must inherit world, systems, and base topology from its official parent'
    }

    Require-Match 'SCENARIO_STOCK_META' $stockMeta `
        'Name\s+"\{BC2437E4861B4FD2\}Missions/AICF_Conflict_Arland\.conf"' `
        'Stock scenario metadata GUID or resource path changed'
    Require-Match 'SCENARIO_RHS_META' $rhsMeta `
        'Name\s+"\{97E4BCB73F044C66\}Missions/AICF_RHS_Conflict_Arland\.conf"' `
        'RHS scenario metadata GUID or resource path changed'
    Require-Match 'SCENARIO_EVERON_META' $everonMeta `
        'Name\s+"\{4C5D73A5614F41D9\}Missions/AICF_Conflict_Everon\.conf"' `
        'Everon scenario metadata GUID or resource path changed'

    Require-Match 'SCENARIO_RHS_EVERON_META' $rhsEveronMeta `
        'Name\s+"\{57FA3D0337BE47E5\}Missions/AICF_RHS_Conflict_Everon\.conf"' `
        'RHS Everon scenario metadata GUID or resource path changed'

    foreach ($meta in @($stockMeta, $everonMeta, $rhsMeta, $rhsEveronMeta)) {
        foreach ($platform in @('PC', 'HEADLESS', 'XBOX_ONE', 'XBOX_SERIES', 'PS4')) {
            Require-Match 'SCENARIO_PLATFORM_CONFIG' $meta `
                ("CONFResourceClass\s+" + [regex]::Escape($platform) + '\b') `
                "Scenario metadata omits $platform resource configuration"
        }
    }
}

$ownedWorldResources = @(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'AIConflictArland'), `
    (Join-Path $RepositoryRoot 'AIConflictEveron'), `
    (Join-Path $RepositoryRoot 'AIConflictEveronRHS'), `
    (Join-Path $RepositoryRoot 'AIConflictArlandRHS') -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in @('.ent', '.layer') })
if ($ownedWorldResources.Count -gt 0) {
    Add-Failure 'SCENARIO_VANILLA_OWNERSHIP' `
        ('Scenario launch must not copy vanilla/RHS world resources: ' +
        (($ownedWorldResources | ForEach-Object FullName) -join ', '))
}

$rhsEveronScripts = @(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'AIConflictEveronRHS') -Recurse -File -Filter '*.c' -ErrorAction SilentlyContinue)
if ($rhsEveronScripts.Count -ne 1 -or $rhsEveronScripts[0].Name -cne 'AICF_RHSEveronCallsignPool.c') {
    Add-Failure 'SCENARIO_RHS_EVERON_COMPOSITION' 'RHS Everon must contain only the local callsign compatibility adapter; lifecycle, radio policy and RHS profile remain in dependencies'
}
else {
    $callsignAdapter = Get-Content -LiteralPath $rhsEveronScripts[0].FullName -Raw
    Require-Match 'SCENARIO_RHS_EVERON_CALLSIGN' $callsignAdapter `
        'modded\s+class\s+SCR_CampaignMilitaryBaseManager[\s\S]*override\s+protected\s+void\s+GetSharedCallsignPool\(notnull array<int> outIndexes\)' `
        'RHS Everon callsign adapter must use the pinned stock pool boundary'
    Require-Match 'SCENARIO_RHS_EVERON_CALLSIGN_AUTHORITY' $callsignAdapter `
        'super\.GetSharedCallsignPool\(outIndexes\);[\s\S]*!GetGame\(\)\.InPlayMode\(\)[\s\S]*!Replication\.IsServer\(\)[\s\S]*!m_Campaign\.IsMaster\(\)[\s\S]*return;' `
        'RHS Everon callsign extension must preserve the stock pool and require server/master authority'
    Require-Match 'SCENARIO_RHS_EVERON_CALLSIGN_SCOPE' $callsignAdapter `
        '!GetGame\(\)\.GetWorldFile\(\)\.Contains\("CTI_Campaign_Eden_RHS"\)[\s\S]*return;' `
        'The compatibility adapter must leave Arland and stock Everon unchanged'
    Require-Match 'SCENARIO_RHS_EVERON_CALLSIGN_CAPACITY' $callsignAdapter `
        'originalCount == 0[\s\S]*return;[\s\S]*base && base\.IsInitialized\(\)[\s\S]*initializedBases\+\+[\s\S]*initializedBases <= originalCount[\s\S]*return;[\s\S]*int index = originalCount; index < initializedBases; index\+\+[\s\S]*outIndexes\.Insert\(index\)' `
        'Extend only an insufficient nonempty pool, with unique indexes bounded by initialized bases'
    Forbid-Match 'SCENARIO_RHS_EVERON_COMPOSITION' $callsignAdapter `
        'OnGameStart|OnGameEnd|CallLater|new\s+AICF_MatchController|SetFaction\(|SetCallsignIndex\(|Replication\.BumpMe|SpawnEntity|DeleteEntity' `
        'The adapter must only compose pool data; stock owns initialization and replicated callsign assignment'
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "[AICF][SCENARIO_STATIC][FAIL] $failure"
    }
    Write-Output "[AICF][SCENARIO_STATIC][RESULT][FAIL] issues=$($failures.Count)"
    exit 1
}

Write-Output '[AICF][SCENARIO_STATIC][RESULT][PASS] arland_header=PASS everon_header=PASS rhs_header=PASS rhs_everon_header=PASS menu_visibility=PASS persistence=DISABLED rank_unlocks=PASS inheritance=PASS metadata=PASS world_ownership=PASS'
exit 0
