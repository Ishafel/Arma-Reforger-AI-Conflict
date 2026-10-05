param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$BuilderLogPath,
    [string]$DefendLogPath
)
$ErrorActionPreference = 'Stop'
. (Join-Path $RepositoryRoot 'tools/Stage3StaticAudit.Common.ps1')
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($entry in @(
    @('identity','Construction/AICF_BaseBuilder.c'),
    @('service','Construction/AICF_BaseBuilderService.c'),
    @('node','Forces/AICF_AINodeLifecycle.c'),
    @('planner','Orders/AICF_OrderPlanner.c'),
    @('defend','Orders/AICF_DefendArrivalHandoff.c')
)) { $sources[$entry[0]] = Get-Content (Join-Path $core $entry[1]) -Raw }
function Body($source, $method) {
    Get-AICFMethodBody ([pscustomobject]@{Source=$source; Code=(ConvertTo-AICFCodeText $source)}) $method
}
function Check($s) {
    $fail = [Collections.Generic.List[string]]::new()
    $identity = Body $s.identity 'IsCurrent'
    foreach ($guard in @('builder.m_iGeneration != m_iGeneration','builder.m_Waypoint != m_Waypoint','builder.m_Target != m_Target','builder.m_CharacterId != m_CharacterId','builder.m_bReturning != m_bReturning','m_Group.GetID() != m_GroupId')) {
        if (!$identity.Contains($guard)) { $fail.Add('BUILDER_IDENTITY') }
    }
    $report = Body $s.service 'ReportFailedMovement'
    foreach ($guard in @('!Replication.IsServer()','s_Instance.m_bStopped','builder.m_Group != group','group.GetCurrentWaypoint() != waypoint','!s_Instance.IsWorkerValid(builder)')) {
        if (!$report.Contains($guard)) { $fail.Add('BUILDER_OWNER') }
    }
    if ($report -match 'ClearTarget|SetBuilderWaypoint|Retire\(') { $fail.Add('DEFERRED_OWNER') }
    $node = Body $s.node 'AICF_HandleBuilderFailedMovement'
    if ($node -notmatch 'IsMoveFailureCurrent\(builder, failure\)' -or $node -notmatch 'GetExecutedAction\(\) == failedAction') { $fail.Add('SYNCHRONOUS_CALLBACK_FENCE') }
    $consume = Body $s.service 'ConsumeMoveFailure'
    if ($consume -notmatch 'IsMoveFailureCurrent' -or $consume -notmatch 'm_iReturnMoveFailures >= 2' -or $consume -notmatch 'SetBuilderFieldHold') { $fail.Add('BOUNDED_RETURN') }
    $returnHomeBody = Body $s.service 'ReturnHome'
    if ($returnHomeBody -notmatch '(?s)m_bReturnBlocked.*?m_vBlockedHome.*?HOME_METERS \* HOME_METERS.*?return;.*?MoveTo.*?IDLE_AT_MAIN_TENT') { $fail.Add('PHYSICAL_HOME') }
    $hold = Body $s.planner 'SetBuilderFieldHold'
    if ($hold -notmatch 'SCR_DefendWaypoint.Cast' -or $hold -notmatch 'ConfigureInfantryServiceWaypoint' -or $hold -notmatch 'builder.m_Group.AddWaypoint\(waypoint\)') { $fail.Add('REAL_HOLD') }
    $arrival = Body $s.defend 'TryHandle'
    foreach ($guard in @('slot.GetGroup() != group','group.GetCurrentWaypoint() != waypoint','!IsFormationAtWaypoint(group, waypoint)','move.m_RelatedWaypoint != waypoint','utility.GetExecutedAction() != move')) {
        if (!$arrival.Contains($guard)) { $fail.Add('DEFEND_ARRIVAL_OWNER') }
    }
    if ($arrival -notmatch 'new SCR_AIDefendActivity' -or $arrival -notmatch 'utility.AddAction\(activity\)' -or
        $arrival -notmatch 'defend.GetActionState\(\) != EAIActionState.FAILED' -or $arrival -match 'CompleteWaypoint|SetHoldingTime|RecordStrategicAssignment|AssignObjective') { $fail.Add('DEFEND_REAL_ACTIVITY') }
    $physical = Body $s.defend 'IsFormationAtWaypoint'
    if ($physical -notmatch 'GetAgents\(agents\)' -or $physical -notmatch 'member.GetOrigin\(\)' -or
        $physical -notmatch 'alive > 0' -or $physical -match 'group.GetOrigin\(') { $fail.Add('DEFEND_PHYSICAL_FORMATION') }
    $identity = Body $s.defend 'IsCurrent'
    if ($identity -notmatch 'slot.GetSpawnGeneration\(\) == generation' -or $identity -notmatch 'slot.GetStrategicAssignmentRevision\(\) == assignment') { $fail.Add('DEFEND_CALLBACK_FENCE') }
    $gate = Body $s.node 'AICF_HandleDefendArrival'
    if ($gate -notmatch '!related' -or $gate -notmatch '!AICF_IsInfantryMoveHandler\(handler\)' -or $gate -notmatch 'result != EMoveError.STOPPED' -or $gate -notmatch 'FindManagedInfantrySlot') { $fail.Add('DEFEND_SCOPE') }
    $failed = Body $s.node 'AICF_HandleFailedMovement'
    if ($failed -notmatch 'failedWaypoint != slot.GetWaypoint\(\)' -or $failed -notmatch 'ReportFailedMovement\(m_Group, failedWaypoint, result\)') { $fail.Add('DEFEND_APPROACH_OWNER') }
    return $fail.ToArray()
}
$failures = @(Check $sources)
$mutations = @(
    @('BUILDER_IDENTITY','identity','builder.m_iGeneration != m_iGeneration'),
    @('BUILDER_IDENTITY','identity','builder.m_Waypoint != m_Waypoint'),
    @('BUILDER_IDENTITY','identity','builder.m_Target != m_Target'),
    @('BUILDER_IDENTITY','identity','builder.m_CharacterId != m_CharacterId'),
    @('BUILDER_IDENTITY','identity','builder.m_bReturning != m_bReturning'),
    @('BUILDER_OWNER','service','group.GetCurrentWaypoint() != waypoint'),
    @('BUILDER_OWNER','service','!s_Instance.IsWorkerValid(builder)'),
    @('SYNCHRONOUS_CALLBACK_FENCE','node','AICF_BaseBuilderService.IsMoveFailureCurrent(builder, failure)'),
    @('SYNCHRONOUS_CALLBACK_FENCE','node','m_GroupUtilityComponent.GetExecutedAction() == failedAction'),
    @('BOUNDED_RETURN','service','builder.m_iReturnMoveFailures >= 2'),
    @('PHYSICAL_HOME','service','HOME_METERS * HOME_METERS'),
    @('REAL_HOLD','planner','builder.m_Group.AddWaypoint(waypoint);'),
    @('DEFEND_ARRIVAL_OWNER','defend','!IsFormationAtWaypoint(group, waypoint)'),
    @('DEFEND_ARRIVAL_OWNER','defend','move.m_RelatedWaypoint != waypoint'),
    @('DEFEND_ARRIVAL_OWNER','defend','utility.GetExecutedAction() != move'),
    @('DEFEND_REAL_ACTIVITY','defend','utility.AddAction(activity);'),
    @('DEFEND_PHYSICAL_FORMATION','defend','member.GetOrigin()'),
    @('DEFEND_CALLBACK_FENCE','defend','slot.GetStrategicAssignmentRevision() == assignment'),
    @('DEFEND_SCOPE','node','result != EMoveError.STOPPED'),
    @('DEFEND_APPROACH_OWNER','node','failedWaypoint != slot.GetWaypoint()')
)
foreach ($mutation in $mutations) {
    $copy = $sources.Clone()
    $copy[$mutation[1]] = $copy[$mutation[1]].Replace($mutation[2], 'false')
    if (-not (@(Check $copy) -contains $mutation[0])) { $failures += "MUTATION_ESCAPED_$($mutation[0])" }
}
if ($BuilderLogPath) {
    $log = Get-Content -LiteralPath $BuilderLogPath -Raw
    foreach ($case in @('CURRENT_IDENTITY','STALE_GENERATION','STALE_WAYPOINT','STALE_PHASE','STALE_CHARACTER','STALE_TARGET','STALE_FACTION','SYNCHRONOUS_STALE_ACTION','NULL_WAYPOINT','UNRELATED_ERROR','FOREIGN_HANDLER','BT_HANDOFF','DEFERRED_OWNER','WORK_ABORT_RETURN','RETURN_WAYPOINT_0','RETURN_WAYPOINT_1','RETURN_BOUNDED_HOLD','NO_TIMER_DELETE_OR_RETRY','HOME_CONTEXT_REARMS')) {
        if ($log -notmatch "\[BUILDER_MOVEMENT_PROBE\] case=$case passed=1") { $failures += "MISSING_$case" }
    }
    if ($log -notmatch '\[BUILDER_MOVEMENT_PROBE_RETIRED\] reason=IDLE_AT_MAIN_TENT') { $failures += 'PHYSICAL_RETURN_NOT_OBSERVED' }
    if ($log -notmatch 'Game destroyed\.' -or $log -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|\[BUILDER_MOVEMENT_PROBE\].*passed=0') { $failures += 'RUNTIME_ERROR_OR_INCOMPLETE' }
}
if ($DefendLogPath) {
    $log = Get-Content -LiteralPath $DefendLogPath -Raw
    foreach ($case in @('WAITING_UNCHANGED','UNKNOWN_UNCHANGED','UNRELATED_UNCHANGED','VEHICLE_UNCHANGED','FOREIGN_WAYPOINT','DISTANT_FORMATION','SYNCHRONOUS_REPLACEMENT','ARRIVED_MOVE_HANDOFF','WAYPOINT_RETAINED','OLD_MOVE_FAILED','NEW_ACTIVITY_SUBMITTED','APPROACH_ORDER_CREATED','APPROACH_FAILURE_OWNED','APPROACH_NOT_COMPLETED','NATIVE_HOLD_60S','NO_ASSIGNMENT_CHURN')) {
        if ($log -notmatch "\[DEFEND_HANDOFF_PROBE\] case=$case passed=1") { $failures += "MISSING_$case" }
    }
    if ($log -notmatch 'Game destroyed\.' -or $log -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|\[DEFEND_HANDOFF_PROBE\].*passed=0') { $failures += 'DEFEND_RUNTIME_ERROR_OR_INCOMPLETE' }
}
if ($failures.Count) { $failures; exit 1 }
"Issue 14 recovery contracts: PASS; negative mutations=$($mutations.Count)"
if ($BuilderLogPath) { 'Builder movement runtime: PASS' }
if ($DefendLogPath) { 'Defend activity runtime: PASS' }
exit 0
