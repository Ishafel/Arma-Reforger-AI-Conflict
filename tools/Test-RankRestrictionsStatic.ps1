param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$policyPath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_RankRestrictions.c'
$campaignStatePath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_CampaignState.c'

function Require-Match {
    param([string]$Rule, [string]$Text, [string]$Pattern, [string]$Message)
    if ($Text -notmatch $Pattern) {
        $failures.Add("[$Rule] $Message")
    }
}

if (-not (Test-Path -LiteralPath $policyPath) -or -not (Test-Path -LiteralPath $campaignStatePath)) {
    $failures.Add('[RANK_POLICY_FILE] Missing rank policy or campaign-state source')
}
else {
    $policy = Get-Content -LiteralPath $policyPath -Raw
    $campaignState = Get-Content -LiteralPath $campaignStatePath -Raw

    Require-Match 'RANK_VEHICLE_REQUEST' $campaignState `
        'CanRequestVehicleWithoutRank\s*\(\s*\)[^}]*return\s+true\s*;' `
        'Vehicle requests must bypass the vanilla rank gate'
    Require-Match 'RANK_CHARACTER_VIEW' $policy `
        'modded\s+class\s+SCR_CharacterRankComponent[\s\S]*?GetCharacterRank\s*\(\s*\)[^}]*return\s+SCR_ECharacterRank\.GENERAL\s*;' `
        'Character-based rank checks must observe GENERAL'
    Require-Match 'RANK_XP_VIEW' $policy `
        'modded\s+class\s+SCR_PlayerXPHandlerComponent[\s\S]*?GetPlayerRankByXP\s*\(\s*\)[^}]*return\s+SCR_ECharacterRank\.GENERAL\s*;' `
        'XP-based rank checks must observe GENERAL'
    Require-Match 'RANK_SERIALIZATION_SAFETY' $policy `
        'modded\s+class\s+SCR_CharacterRankComponent' `
        'Rank policy must stay on runtime components'

    if ($policy -match 'modded\s+class\s+(SCR_EntityCatalog|SCR_CampaignBuildingRankBudget|SCR_BaseRadialCommand|SCR_PlayerArsenalLoadout)') {
        $failures.Add('[RANK_SERIALIZATION_SAFETY] Rank policy must not mod serialized catalog/container classes')
    }
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "[AICF][RANK_STATIC][FAIL] $failure"
    }
    Write-Output "[AICF][RANK_STATIC][RESULT][FAIL] issues=$($failures.Count)"
    exit 1
}

Write-Output '[AICF][RANK_STATIC][RESULT][PASS] vehicle_override=PASS character_rank_view=GENERAL xp_rank_view=GENERAL serialization_safety=PASS'
exit 0
