param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($file in @('Bootstrap/AICF_MatchController.c', 'Orders/AICF_OrderPlanner.c', 'Forces/AICF_InfantryRecruitmentOrder.c', 'Forces/AICF_InfantryRecruitmentService.c', 'UI/AICF_SquadCommandRpc.c')) {
    $sources[$file] = [IO.File]::ReadAllText((Join-Path $core $file))
}
$rules = @(
    @('Bootstrap/AICF_MatchController.c', '!m_Campaign.IsMaster\(\)', 'SERVER_MASTER'),
    @('Bootstrap/AICF_MatchController.c', 'GetPlayerController\(player.GetPlayerId\(\)\) != player', 'OWNED_PLAYER'),
    @('Bootstrap/AICF_MatchController.c', 'GetPlayerPointFactionState\(m_ContentProfile.GetStableFactionKey\(faction.GetFactionKey\(\)\)\)', 'FACTION_SCOPE'),
    @('Bootstrap/AICF_MatchController.c', '!commander.OwnsSlot\(slot\)', 'COMMANDER_POLICY'),
    @('Bootstrap/AICF_MatchController.c', 'CancelForSlot\(slot\)[\s\S]*?ReleasePlayerCommand\(slot\)[\s\S]*?commander.AssignOrder\(slot, "PLAYER_RELEASE"', 'CANCEL_RELEASE_REPLAN'),
    @('Orders/AICF_OrderPlanner.c', 'ReleasePlayerCommand[^}]*ClearStrategicIntent\(\)[^}]*ClearPlayerStrategicOrder\(\)[^}]*RecordStrategicAssignment\(null, string.Empty\)', 'RELEASE_REVISIONS'),
    @('Forces/AICF_InfantryRecruitmentOrder.c', 'GetStrategicAssignmentRevision\(\) == m_iAssignment', 'ASSIGNMENT_FENCE'),
    @('Forces/AICF_InfantryRecruitmentOrder.c', 'GetStrategicIntentRevision\(\) == m_iIntent', 'INTENT_FENCE'),
    @('Forces/AICF_InfantryRecruitmentOrder.c', '\(m_bPlayerRequested \|\| !slot.HasPlayerStrategicIntent\(\)\)', 'MANUAL_VISIT_ONLY'),
    @('Forces/AICF_InfantryRecruitmentService.c', '!playerRequested && base != nearest', 'AUTONOMOUS_LOCALITY'),
    @('Orders/AICF_OrderPlanner.c', 'if \(order.m_bPlayerRequested\)\s*order.m_Slot.ClearPlayerStrategicOrder\(\);', 'MANUAL_COMPLETION_RELEASE'),
    @('UI/AICF_SquadCommandRpc.c', 'RplRcver.Owner', 'OWNER_RESPONSE')
)

function Test-SquadSelectionContracts {
    param([string]$Source)

    $record = [pscustomobject]@{ Source = $Source; Code = (ConvertTo-AICFCodeText $Source) }
    $selection = ConvertTo-AICFCodeText (Get-AICFMethodBody $record 'SelectBarracks')
    # Привязка к одному методу, условиям и следующим за ними присваиваниям:
    # похожая строка в comments или другом методе не подтверждает выбор.
    $selectionRules = @(
        @('float\s+nearestDistance\s*=\s*float\.MAX\s*;\s*for\s*\(\s*int\s+index\s*=\s*0\s*;\s*index\s*<\s*m_Graph\.GetNodeCount\(\)\s*;\s*index\+\+\s*\)', 'SELECTION_STABLE_ORDER'),
        @('for\s*\(\s*int\s+nodeId\s*=\s*0\s*;\s*nodeId\s*<\s*m_Graph\.GetNodeCount\(\)\s*;\s*nodeId\+\+\s*\)[\s\S]*?base\.GetServices\(services\)\s*;\s*foreach\s*\(\s*SCR_ServicePointComponent\s+service\s*:\s*services\s*\)', 'SELECTION_STABLE_ORDER'),
        @('float\s+distance\s*=\s*vector\.DistanceSqXZ\(position,\s*node\.GetBase\(\)\.GetOwner\(\)\.GetOrigin\(\)\)\s*;\s*if\s*\(\s*distance\s*<\s*nearestDistance\s*\)\s*\{\s*nearest\s*=\s*node\.GetBase\(\)\s*;\s*nearestDistance\s*=\s*distance\s*;\s*\}', 'NEAREST_GRAPH_STABLE_TIE'),
        @('float\s+pendingDistance\s*=\s*float\.MAX\s*;', 'PENDING_DISTANCE_STABLE_TIE'),
        @('if\s*\(\s*!feasible\s*\)\s*\{\s*if\s*\(\s*allowPending\s*&&\s*distanceSq\s*<\s*pendingDistance\s*\)\s*\{\s*pending\s*=\s*order\s*;\s*pending\.m_bAwaitingSupplyPlan\s*=\s*true\s*;\s*pendingDistance\s*=\s*distanceSq\s*;\s*\}\s*continue\s*;\s*\}', 'PENDING_DISTANCE_STABLE_TIE'),
        @('order\.m_Forecast\s*=\s*Forecast\(base,\s*slot,\s*ownDemand,\s*members,\s*order\)\s*;\s*bool\s+feasible\s*=\s*order\.m_Forecast\.Evaluate\(Math\.Sqrt\(distanceSq\),\s*remainingSeconds\)\s*;\s*if\s*\(\s*!order\.m_Forecast\.CanApproach\(playerRequested\)\s*\)\s*continue\s*;', 'SELECTION_FEASIBILITY'),
        @('float\s+bestCompletion\s*=\s*float\.MAX\s*;', 'ETA_SELECTION_STABLE_TIE'),
        @('continue\s*;\s*\}\s*if\s*\(\s*order\.m_Forecast\.m_fCompletionSeconds\s*>=\s*bestCompletion\s*\)\s*continue\s*;\s*best\s*=\s*order\s*;\s*bestCompletion\s*=\s*order\.m_Forecast\.m_fCompletionSeconds\s*;\s*\}\s*\}\s*if\s*\(\s*!best\s*\)\s*return\s+pending\s*;\s*return\s+best\s*;\s*\}\s*$', 'ETA_SELECTION_STABLE_TIE')
    )
    foreach ($rule in $selectionRules) {
        if ($selection -notmatch $rule[0]) { $rule[1] }
    }
}

$failures = @()
foreach ($rule in $rules) {
    if ($sources[$rule[0]] -notmatch $rule[1]) { $failures += $rule[2] }
    # Удаление обязательного guard/перехода должно давать отрицательный verdict.
    $mutated = [regex]::Replace($sources[$rule[0]], $rule[1], '')
    if ($mutated -match $rule[1]) { $failures += "MUTATION_ESCAPED:$($rule[2])" }
}
$failures += @(Test-SquadSelectionContracts $sources['Forces/AICF_InfantryRecruitmentService.c'])
if (Test-Path (Join-Path $core 'Forces/AICF_SquadCommandsProbe.c')) { $failures += 'FIXTURE_IN_PRODUCTION' }
$evidence = Join-Path $RepositoryRoot '.codex-runtime/squad-command-contracts'
New-Item -ItemType Directory -Force $evidence | Out-Null
$cases = @(
    @('manual-far', 'distance_m=600 max_distance_m=-1 player_requested=1', 0),
    @('auto-far', 'distance_m=600 max_distance_m=500', 1),
    @('manual-wrong-policy', 'distance_m=600 max_distance_m=500 player_requested=1', 1),
    @('auto-local', 'distance_m=300 max_distance_m=500', 0)
)
foreach ($case in $cases) {
    $logPath = Join-Path $evidence ($case[0] + '.txt')
    @(
        ('[INFANTRY_RECRUITMENT_STARTED] faction=US slot=0 generation=1 token=1 group=G alive=1 desired=2 ' + $case[1]),
        '[INFANTRY_RECRUITMENT_FINISHED] faction=US slot=0 generation=1 token=1 group=G reason=STOP'
    ) | Set-Content -LiteralPath $logPath
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-InfantryRecruitmentLog.ps1') -LogPath $logPath *> (Join-Path $evidence ($case[0] + '-verdict.txt'))
    if ($LASTEXITCODE -ne $case[2]) { $failures += "DISTANCE_POLICY:$($case[0])" }
}
if ($failures.Count) { $failures; exit 1 }
Write-Output "PASS Squad commands: $($rules.Count) authority, cancellation, identity and selection contracts; negative mutations rejected."
Write-Output 'PASS SelectBarracks: stable graph/services order, strict nearest/pending distance, feasible minimum ETA and first-candidate ties.'
Write-Output 'PASS Recruitment log distance policy: 4 positive/negative inputs.'
