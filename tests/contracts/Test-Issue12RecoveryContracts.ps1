param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$RuntimeLogPath
)
$ErrorActionPreference = 'Stop'
. (Join-Path $RepositoryRoot 'tests/lib/Stage3StaticAudit.Common.ps1')
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($entry in @(
    @('order','Forces/AICF_InfantryRecruitmentOrder.c'),
    @('service','Forces/AICF_InfantryRecruitmentService.c'),
    @('node','Forces/AICF_AINodeLifecycle.c'),
    @('planner','Orders/AICF_OrderPlanner.c'),
    @('controller','Bootstrap/AICF_MatchController.c')
)) { $sources[$entry[0]] = Get-Content (Join-Path $core $entry[1]) -Raw }
function Body($source, $method) {
    Get-AICFMethodBody ([pscustomobject]@{Source=$source; Code=(ConvertTo-AICFCodeText $source)}) $method
}
function Check($s) {
    $fail = [Collections.Generic.List[string]]::new()
    $report = Body $s.order 'ReportFailedMovement'
    if ($report -notmatch 'IsCurrent\(m_Slot\)' -or $report -notmatch 'group.GetCurrentWaypoint\(\) != waypoint') { $fail.Add('VISIT_IDENTITY') }
    $handle = Body $s.node 'AICF_HandleFailedMovement'
    if ($handle -notmatch 'ReportRecruitmentFailedMovement' -or $handle -notmatch 'HasRecruitmentMovementFailure\(failedWaypoint\)' -or
        $handle -notmatch 'GetExecutedAction\(\) == failedAction') { $fail.Add('CALLBACK_OWNER') }
    $approach = Body $s.service 'CheckApproach'
    if ($approach -notmatch '!order.m_bMovementFailed' -or $approach -notmatch 'm_iApproachRepairs >= 2' -or
        $approach -notmatch 'm_bMovementFailed = false') { $fail.Add('BOUNDED_DEFERRED_REPAIR') }
    if ($s.node -notmatch '(?s)modded class SCR_AIGetSmartActionsState.*?!waypoint && Replication.IsServer\(\).*?FindManagedInfantrySlot\(group\).*?super.OnAbort\(owner, null\).*?return ENodeResult.FAIL') { $fail.Add('SMART_ACTION_CLEANUP') }
    $rebuild = Body $s.planner 'RebuildCurrentOrder'
    if ($rebuild -notmatch 'GetRouteRecoveryEpisode\(\).IsBlocked\(slot\)' -or $rebuild -notmatch 'IsPersistentStuckFieldHold') { $fail.Add('REBUILD_HOLD_OWNER') }
    $allowed = Body $s.controller 'ResolveAllowedIdleReason'
    if ($allowed -notmatch '(?s)IsPersistentStuckFieldHold.*?IsWaypointBoundToGroup.*?PERSISTENT_STUCK_FIELD_HOLD') { $fail.Add('EXECUTABLE_HOLD_IDLE') }
    $hidden = Body $s.controller 'TryApplyHiddenMobEgressRecovery'
    if ($hidden -notmatch '(?s)CanRebuildAfterHiddenMobEgress\(slot, faction\).*?return false;.*?Teleport\(') { $fail.Add('EGRESS_PREFLIGHT') }
    $preflight = Body $s.planner 'CanRebuildAfterHiddenMobEgress'
    if ($preflight -notmatch '!slot.IsRouteTargetDeferred\(slot.GetTargetBase\(\)\)' -or $preflight -notmatch '!slot.GetRouteRecoveryEpisode\(\).IsBlocked\(slot\)') { $fail.Add('EGRESS_TARGET_AND_EPISODE') }
    $recover = Body $s.controller 'TryRecoverOrder'
    if ($recover -notmatch '(?s)GetRouteRecoveryEpisode\(\).IsBlocked\(slot\).*?return false;') { $fail.Add('TASK_AUDIT_OWNER') }
    foreach ($method in @('AssignOrder','AssignAICommanderOrder','ReconcileStrategicOrder','ReconcileAICommanderOrder','AssignLossResponseOrder','AssignAICommanderLossResponseOrder')) {
        $boundary = Body $s.planner $method
        if ($boundary -notmatch '(?s)IsTemporaryRouteReplanHold\(\).*?IsPersistentStuckFieldHold\(\).*?GetRouteRecoveryEpisode\(\).IsBlocked\(slot\).*?return false;.*?IsRecruitingInfantry') { $fail.Add('POSTURE_QRF_OWNER') }
    }
    return $fail.ToArray()
}
$failures = @(Check $sources)
$mutations = @(
    @('VISIT_IDENTITY','order','IsCurrent(m_Slot)'),
    @('CALLBACK_OWNER','node','slot.HasRecruitmentMovementFailure(failedWaypoint)'),
    @('BOUNDED_DEFERRED_REPAIR','service','!order.m_bMovementFailed'),
    @('SMART_ACTION_CLEANUP','node','super.OnAbort(owner, null);'),
    @('REBUILD_HOLD_OWNER','planner','slot.GetRouteRecoveryEpisode().IsBlocked(slot)'),
    @('EXECUTABLE_HOLD_IDLE','controller','"PERSISTENT_STUCK_FIELD_HOLD"'),
    @('EGRESS_PREFLIGHT','controller','m_OrderPlanner.CanRebuildAfterHiddenMobEgress(slot, faction)'),
    @('EGRESS_TARGET_AND_EPISODE','planner','!slot.IsRouteTargetDeferred(slot.GetTargetBase())'),
    @('TASK_AUDIT_OWNER','controller','slot.GetRouteRecoveryEpisode().IsBlocked(slot)'),
    @('POSTURE_QRF_OWNER','planner','slot.IsTemporaryRouteReplanHold()')
)
foreach ($mutation in $mutations) {
    $copy = $sources.Clone()
    $copy[$mutation[1]] = $copy[$mutation[1]].Replace($mutation[2], 'false')
    if (-not (@(Check $copy) -contains $mutation[0])) { $failures += "MUTATION_ESCAPED_$($mutation[0])" }
}
if ($RuntimeLogPath) {
    $log = Get-Content -LiteralPath $RuntimeLogPath -Raw
    foreach ($case in @('RECRUITMENT_MOVE_FAILURE_FENCES','EXHAUSTED_OWNER_FENCES','SMART_ACTION_NULL_CLEANUP','POSTURE_AND_QRF_HOLD_OWNER')) {
        if ($log -notmatch "\[EPISODE_PROBE\] case=$case passed=1") { $failures += "MISSING_$case" }
    }
    if ($log -notmatch 'Game destroyed\.' -or $log -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|\[EPISODE_PROBE\].*passed=0') {
        $failures += 'RUNTIME_ERROR_OR_INCOMPLETE'
    }
}
if ($failures.Count) { $failures; exit 1 }
"Issue 12 recovery contracts: PASS; negative mutations=$($mutations.Count)"
if ($RuntimeLogPath) { 'Issue 12 owner/visit runtime fences: PASS' }
