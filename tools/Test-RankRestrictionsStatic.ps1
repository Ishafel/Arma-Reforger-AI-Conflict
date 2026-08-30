param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$PolicyPath
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
if (-not $PolicyPath) {
    $PolicyPath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_RankRestrictions.c'
}
$policyPath = $PolicyPath
$campaignStatePath = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_CampaignState.c'
$stockHeaderPath = Join-Path $RepositoryRoot 'AIConflictArland/Missions/AICF_Conflict_Arland.conf'
$rhsHeaderPath = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Missions/AICF_RHS_Conflict_Arland.conf'

function Require-Match {
    param([string]$Rule, [string]$Text, [string]$Pattern, [string]$Message)
    if ($Text -notmatch $Pattern) {
        $failures.Add("[$Rule] $Message")
    }
}

function Forbid-Match {
    param([string]$Rule, [string]$Text, [string]$Pattern, [string]$Message)
    if ($Text -match $Pattern) {
        $failures.Add("[$Rule] $Message")
    }
}

$requiredPaths = @($policyPath, $campaignStatePath, $stockHeaderPath, $rhsHeaderPath)
$missingPaths = @($requiredPaths | Where-Object { -not (Test-Path -LiteralPath $_) })
if ($missingPaths.Count -gt 0) {
    $failures.Add('[RANK_POLICY_FILE] Missing rank policy, campaign-state, or scenario-header source: ' + ($missingPaths -join ', '))
}
else {
    $policy = Get-Content -LiteralPath $policyPath -Raw
    $campaignState = Get-Content -LiteralPath $campaignStatePath -Raw
    $stockHeader = Get-Content -LiteralPath $stockHeaderPath -Raw
    $rhsHeader = Get-Content -LiteralPath $rhsHeaderPath -Raw

    Require-Match 'RANK_VEHICLE_REQUEST' $campaignState `
        'CanRequestVehicleWithoutRank\s*\(\s*\)[^}]*return\s+true\s*;' `
        'Vehicle requests must bypass the vanilla rank gate'
    foreach ($scenarioHeader in @($stockHeader, $rhsHeader)) {
        Require-Match 'RANK_JOIN_GENERAL' $scenarioHeader `
            'm_eStartingRank\s+GENERAL' `
            'Every AICF scenario must grant the GENERAL XP threshold on initial join'
    }
    Require-Match 'RANK_CHARACTER_VIEW' $policy `
        'modded\s+class\s+SCR_CharacterRankComponent[\s\S]*?GetCharacterRank\s*\(\s*\)[^}]*return\s+SCR_ECharacterRank\.GENERAL\s*;' `
        'Character-based rank checks must observe GENERAL'
    Require-Match 'RANK_XP_VIEW' $policy `
        'modded\s+class\s+SCR_PlayerXPHandlerComponent[\s\S]*?GetPlayerRankByXP\s*\(\s*\)[^}]*return\s+SCR_ECharacterRank\.GENERAL\s*;' `
        'XP-based rank checks must observe GENERAL'
    Require-Match 'RANK_SERIALIZATION_SAFETY' $policy `
        'modded\s+class\s+SCR_CharacterRankComponent' `
        'Rank policy must stay on runtime components'
    Require-Match 'RANK_XP_FLOOR_MUTATION_HOOK' $policy `
        'override\s+void\s+AddPlayerXP\s*\([\s\S]*?super\.AddPlayerXP\s*\([\s\S]*?AICF_ApplyGeneralXPFloor\s*\(\s*\)' `
        'The central XP mutation path must verify the floor after every stock reward, penalty, reconnect restore, or persistence restore'
    Require-Match 'RANK_XP_FLOOR_TRANSITION_GUARD' $policy `
        'override\s+void\s+UpdatePlayerRank\s*\([^)]*\)\s*\{[\s\S]*?AICF_IsGeneralXPFloorAuthority\s*\(\s*\)[\s\S]*?AICF_ApplyGeneralXPFloor\s*\(\s*\)[\s\S]*?rankComponent\.SetCharacterRank\s*\(\s*SCR_ECharacterRank\.GENERAL\s*,\s*!notify\s*\)' `
        'Only authority may clamp XP and set the replicated character state to GENERAL'
    Require-Match 'RANK_XP_FLOOR_THRESHOLD' $policy `
        'GetRequiredRankXP\s*\(\s*SCR_ECharacterRank\.GENERAL\s*\)' `
        'The floor must come from the active faction RankContainer GENERAL threshold'
    Require-Match 'RANK_XP_FLOOR_MAXIMUM_FALLBACK' $policy `
        'highestConfiguredXP\s*=\s*int\.MIN[\s\S]*?foreach\s*\(\s*SCR_RankInfo\s+rankInfo\s*:\s*ranks\.GetAllRanks\s*\(\s*\)\s*\)[\s\S]*?rankInfo\.IsRankRenegade\s*\(\s*\)[\s\S]*?requiredXP\s*<=\s*highestConfiguredXP[\s\S]*?catalogFloorRank\s*=\s*rankInfo\.GetRankID\s*\(\s*\)[\s\S]*?generalXPFloor\s*=\s*highestConfiguredXP' `
        'Containers without a serialized GENERAL entry must use their highest non-renegade threshold as the effective maximum-rank floor'
    Require-Match 'RANK_XP_FLOOR_FACTION_READY' $policy `
        'factionManager\.GetPlayerFaction\s*\(\s*playerController\.GetPlayerId\s*\(\s*\)\s*\)[\s\S]*?factionManager\.GetFactionRanks\s*\(\s*playerController\.GetPlayerId\s*\(\s*\)\s*\)' `
        'Join/JIP enforcement must wait for authoritative faction-manager assignment and use the same rank-container resolution as stock XP'
    Require-Match 'RANK_XP_FLOOR_FACTION_LIFECYCLE' $policy `
        'modded\s+class\s+SCR_FactionManager[\s\S]*?override\s+protected\s+void\s+OnPlayerFactionSet_S\s*\([^)]*\)[\s\S]*?super\.OnPlayerFactionSet_S\s*\(\s*playerComponent\s*,\s*faction\s*\)[\s\S]*?xpHandler\.UpdatePlayerRank\s*\(\s*false\s*\)' `
        'Faction assignment must recheck the floor for initial join, reconnect/JIP, and pre-faction persistence restore'
    Require-Match 'RANK_XP_FLOOR_BOUND' $policy `
        'm_iPlayerXP\s*>=\s*generalXPFloor[\s\S]*?correction\s*=\s*generalXPFloor\s*-\s*previousXP[\s\S]*?AddPlayerXP\s*\(\s*SCR_EXPRewards\.STARTING_RANK\s*,\s*1\.0\s*,\s*false\s*,\s*correction\s*\)' `
        'XP below GENERAL must be restored without truncating accumulated XP above the floor'
    Require-Match 'RANK_XP_FLOOR_AUTHORITY' $policy `
        'AICF_IsGeneralXPFloorAuthority[\s\S]*?Replication\.IsServer\s*\(\s*\)[\s\S]*?gameMode\.IsMaster\s*\(\s*\)' `
        'Only the authoritative server/master may enforce the XP floor'
    Require-Match 'RANK_XP_FLOOR_REPLICATION' $policy `
        'AddPlayerXP\s*\(\s*SCR_EXPRewards\.STARTING_RANK\s*,\s*1\.0\s*,\s*false\s*,\s*correction\s*\)' `
        'The correction must use stock AddPlayerXP so listeners, owner-only replication, and RPC stay synchronized'
    Require-Match 'RANK_XP_FLOOR_RESTORE_EVENT' $policy `
        'AddPlayerXP\s*\(\s*SCR_EXPRewards\.STARTING_RANK\s*,\s*1\.0\s*,\s*false\s*,\s*correction\s*\)' `
        'The authoritative correction must be identified as STARTING_RANK and notify stock XP listeners and the owning client'
    Require-Match 'RANK_XP_FLOOR_RUNTIME_EVIDENCE' $policy `
        'XP_FLOOR_APPLIED[\s\S]*?XP_FLOOR_VERIFIED[\s\S]*?XP_FLOOR_FACTION_RECHECK' `
        'Runtime logs must distinguish actual XP correction, verified GENERAL floor state, and faction lifecycle recheck'

    Forbid-Match 'RANK_SERIALIZATION_SAFETY' $policy `
        'modded\s+class\s+(SCR_EntityCatalog|SCR_CampaignBuildingRankBudget|SCR_BaseRadialCommand|SCR_PlayerArsenalLoadout|SCR_RankContainer)' `
        'Rank policy must not mod serialized catalog/container classes or globally replace SCR_RankContainer.GetRankByXP()'
    Forbid-Match 'RANK_XP_REWARDS_PRESERVED' $policy `
        'm_fXpMultiplier\s*=\s*0' `
        'The policy must preserve positive XP rewards and statistics'
    Forbid-Match 'RANK_PENALTY_AGNOSTIC' $policy `
        'rewardID\s*(==|!=)\s*SCR_EXPRewards\.FRIENDLY_KILL' `
        'The floor must cover every XP penalty instead of special-casing FRIENDLY_KILL'
    Forbid-Match 'RANK_XP_FLOOR_NO_EARLY_POLLING' $policy `
        'CallLater\s*\([^\r\n]*AICF_.*GeneralXPFloor' `
        'Rank enforcement must use stock XP/spawn lifecycle hooks instead of polling an incomplete PlayerController'
    Forbid-Match 'RANK_XP_FLOOR_NO_FACTION_SUBSCRIPTION' $policy `
        'GetOnFactionChanged\s*\(\s*\)\.(Insert|Remove)' `
        'Rank enforcement must use the authoritative game-mode faction transition instead of component-init subscriptions'
    Forbid-Match 'RANK_XP_FLOOR_NO_EARLY_GLOBAL_LOOKUP' $policy `
        'SGetPlayerFaction\s*\(' `
        'Rank enforcement must not query global player-faction state while PlayerController replication is still initializing'
    Forbid-Match 'RANK_XP_FLOOR_STOCK_REPLICATION_OWNER' $policy `
        'm_iPlayerXP\s*=|Rpc\s*\(\s*RpcDo_OnPlayerXPChanged|Replication\.BumpMe' `
        'The mod must not duplicate stock XP mutation, RPC, or replication ownership'
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "[AICF][RANK_STATIC][FAIL] $failure"
    }
    Write-Output "[AICF][RANK_STATIC][RESULT][FAIL] issues=$($failures.Count)"
    exit 1
}

Write-Output '[AICF][RANK_STATIC][RESULT][PASS] join_floor=GENERAL mutation_floor=GENERAL restore_floor=GENERAL character_rank_view=GENERAL xp_rank_view=GENERAL runtime_evidence=PASS rewards_preserved=PASS serialization_safety=PASS'
exit 0
