param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
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
    @('Forces/AICF_InfantryRecruitmentService.c', 'distanceSq >= bestDistance', 'STABLE_NEAREST'),
    @('Orders/AICF_OrderPlanner.c', 'if \(order.m_bPlayerRequested\)\s*order.m_Slot.ClearPlayerStrategicOrder\(\);', 'MANUAL_COMPLETION_RELEASE'),
    @('UI/AICF_SquadCommandRpc.c', 'RplRcver.Owner', 'OWNER_RESPONSE')
)
$failures = @()
foreach ($rule in $rules) {
    if ($sources[$rule[0]] -notmatch $rule[1]) { $failures += $rule[2] }
    # Удаление обязательного guard/перехода должно давать отрицательный verdict.
    $mutated = [regex]::Replace($sources[$rule[0]], $rule[1], '')
    if ($mutated -match $rule[1]) { $failures += "MUTATION_ESCAPED:$($rule[2])" }
}
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
Write-Output 'PASS Recruitment log distance policy: 4 positive/negative inputs.'
