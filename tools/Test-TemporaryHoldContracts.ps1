param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot), [string]$RuntimeLogPath)
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$repair = Get-Content "$core/Orders/AICF_TemporaryHoldRepair.c" -Raw
$match = Get-Content "$core/Bootstrap/AICF_MatchController.c" -Raw
function Check([string]$source, [string]$controller) {
    $fail = @()
    foreach ($guard in @('!Replication.IsServer()', 'current != oldWaypoint',
        'slot.GetSpawnGeneration() == generation', 'slot.GetStrategicAssignmentRevision() == assignment',
        'slot.GetWaypoint() == waypoint', 'group.GetFaction() == faction')) {
        if (!$source.Contains($guard)) { $fail += 'IDENTITY' }
    }
    if ($source -notmatch 'group.AddWaypointAt\(hold, 0\)' -or $source -notmatch 'slot.AssignObjective\(target, hold\)') { $fail += 'REAL_WAYPOINT' }
    if ($source -notmatch '(?s)group.AddWaypointAt.*?IsCurrent.*?slot.AssignObjective.*?group.RemoveWaypoint\(oldWaypoint\).*?DeleteRplEntity\(oldWaypoint') { $fail += 'CALLBACK_AND_DELETE_ORDER' }
    if ($source -match 'BeginTemporaryRouteReplanHold|RecordStrategicAssignment|ResetMeaningfulTask|RearmAfterBoundedHold|\.Begin\(') { $fail += 'DEADLINE_RESET' }
    if ($controller -notmatch '(?s)if \(slot.IsTemporaryRouteReplanHold\(\)\)\s*\{\s*m_OrderPlanner.EnsureTemporaryRouteReplanHold\(slot, faction\);\s*if \(slot.IsTemporaryRouteReplanHoldDue') { $fail += 'RELIABILITY_OWNER' }
    if ($controller -notmatch '(?s)if \(!replanned\)\s*\{\s*slot.BeginTemporaryRouteReplanHold\(holdAnchor\);\s*m_OrderPlanner.EnsureTemporaryRouteReplanHold\(slot, faction\);') { $fail += 'REPLAN_FALLBACK' }
    return $fail
}
$failures = @(Check $repair $match)
$mutations = @(
    @('IDENTITY', 'slot.GetSpawnGeneration() == generation', 'true'),
    @('IDENTITY', 'slot.GetStrategicAssignmentRevision() == assignment', 'true'),
    @('IDENTITY', 'current != oldWaypoint', 'false'),
    @('REAL_WAYPOINT', 'slot.AssignObjective(target, hold)', 'true'),
    @('DEADLINE_RESET', 'return executable;', 'slot.RecordStrategicAssignment(target, "TEST"); return executable;')
)
foreach ($mutation in $mutations) {
    if (!(Check ($repair.Replace($mutation[1], $mutation[2])) $match).Contains($mutation[0])) { $failures += "MUTATION_ESCAPED_$($mutation[0])" }
}
if ($RuntimeLogPath) {
    $log = Get-Content -LiteralPath $RuntimeLogPath -Raw
    foreach ($case in @('INITIAL_HOLD','COMPLETION_REMOVED','REJECTED_REPLAN_EXECUTABLE','RELIABILITY_REPAIRS_REMOVAL','DEADLINES_PRESERVED','IDENTITY_PRESERVED','NATIVE_HOLD_60S','NO_REPAIR_CHURN','NO_TASK_DEADLINE','STALE_ASSIGNMENT_REJECTED','HOLD_AFTER_CONTEXT_TEST','ABSOLUTE_EPISODE_DEADLINE','EXHAUSTED_EXECUTABLE_HOLD')) {
        if ($log -notmatch "\[TEMPORARY_HOLD_PROBE\] case=$case passed=1") { $failures += "MISSING_$case" }
    }
    if ($log -notmatch '\[TEMPORARY_HOLD_PROBE_DONE\] failures=0' -or $log -notmatch 'Game destroyed\.' -or
        $log -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|Application crashed|\[TEMPORARY_HOLD_PROBE\].*passed=0') { $failures += 'RUNTIME_FAILED_OR_INCOMPLETE' }
}
if ($failures.Count) { $failures; exit 1 }
"Temporary hold contracts: PASS; mutations=$($mutations.Count); runtime=$([bool]$RuntimeLogPath)"
exit 0
