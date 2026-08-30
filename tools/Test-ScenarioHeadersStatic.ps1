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
$rhsHeaderPath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Missions/AICF_RHS_Conflict_Arland.conf'
$rhsMetaPath = $rhsHeaderPath + '.meta'

foreach ($path in @($stockHeaderPath, $stockMetaPath, $rhsHeaderPath, $rhsMetaPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        Add-Failure 'SCENARIO_FILE_MISSING' "Missing required scenario resource $path"
    }
}

if ($failures.Count -eq 0) {
    $stockHeader = Get-Content -LiteralPath $stockHeaderPath -Raw
    $stockMeta = Get-Content -LiteralPath $stockMetaPath -Raw
    $rhsHeader = Get-Content -LiteralPath $rhsHeaderPath -Raw
    $rhsMeta = Get-Content -LiteralPath $rhsMetaPath -Raw

    Require-Match 'SCENARIO_STOCK_PARENT' $stockHeader `
        'SCR_MissionHeaderCampaign\s*:\s*"\{C41618FD18E9D714\}Missions/23_Campaign_Arland\.conf"' `
        'Stock scenario must inherit the official Conflict - Arland mission header'
    Require-Match 'SCENARIO_RHS_PARENT' $rhsHeader `
        'SCR_MissionHeaderCampaign\s*:\s*"\{7577640CD42A00BD\}Missions/RHS_Conflict_Arland\.conf"' `
        'RHS scenario must inherit the official RHS Conflict - Arland mission header'

    foreach ($scenario in @($stockHeader, $rhsHeader)) {
        Require-Match 'SCENARIO_MENU_VISIBILITY' $scenario `
            'm_bShowInScenarioMenu\s+1' `
            'Scenario header must be visible in the in-game Scenarios menu'
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

    foreach ($meta in @($stockMeta, $rhsMeta)) {
        foreach ($platform in @('PC', 'HEADLESS', 'XBOX_ONE', 'XBOX_SERIES', 'PS4')) {
            Require-Match 'SCENARIO_PLATFORM_CONFIG' $meta `
                ("CONFResourceClass\s+" + [regex]::Escape($platform) + '\b') `
                "Scenario metadata omits $platform resource configuration"
        }
    }
}

$ownedWorldResources = @(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'AIConflictArland'), `
    (Join-Path $RepositoryRoot 'AIConflictArlandRHS') -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in @('.ent', '.layer') })
if ($ownedWorldResources.Count -gt 0) {
    Add-Failure 'SCENARIO_VANILLA_OWNERSHIP' `
        ('Scenario launch must not copy vanilla/RHS world resources: ' +
        (($ownedWorldResources | ForEach-Object FullName) -join ', '))
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "[AICF][SCENARIO_STATIC][FAIL] $failure"
    }
    Write-Output "[AICF][SCENARIO_STATIC][RESULT][FAIL] issues=$($failures.Count)"
    exit 1
}

Write-Output '[AICF][SCENARIO_STATIC][RESULT][PASS] stock_header=PASS rhs_header=PASS menu_visibility=PASS rank_unlocks=PASS inheritance=PASS metadata=PASS world_ownership=PASS'
exit 0
