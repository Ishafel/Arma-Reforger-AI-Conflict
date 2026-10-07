param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$route = Get-Content (Join-Path $root 'Orders/AICF_InfantryApproachRoute.c') -Raw
$planner = Get-Content (Join-Path $root 'Orders/AICF_OrderPlanner.c') -Raw
$slot = Get-Content (Join-Path $root 'State/AICF_GroupSlot.c') -Raw
$controller = Get-Content (Join-Path $root 'Bootstrap/AICF_MatchController.c') -Raw
function Test-Approach([string]$Route, [string]$Planner, [string]$Slot) {
    $failures = [Collections.Generic.List[string]]::new()
    Assert-AICFContains $failures 'APPROACH_SCOPE' $Planner '!isRelay && role == AICF_EGroupRole.ATTACK && slot.GetUnitType\(\) == AICF_EGroupUnitType.INFANTRY' 'Only infantry attack BASE travel may use corridors'
    Assert-AICFContains $failures 'APPROACH_PENDING' $Planner 'if \(pending\)\s*return null;' 'Unloaded tiles cannot commit an unvalidated route'
    Assert-AICFContains $failures 'APPROACH_PROJECTION' $Route 'vector.DistanceSqXZ\(candidate, endpoint\) > 64.0' 'Projection cannot collapse lane separation'
    Assert-AICFContains $failures 'APPROACH_WATER' $Route 'ChimeraWorldUtils.TryGetWaterSurfaceSimple\(world, endpoint - "0 0.5 0"\)' 'Reject water endpoints'
    Assert-AICFContains $failures 'APPROACH_LOAD_BOUND' $Route 'System.GetTickCount\(m_iTileWaitStartedAtMs\) >= 30000' 'Bound asynchronous tile wait'
    Assert-AICFContains $failures 'APPROACH_LOAD_ONCE' $Route '!navmesh.IsTileRequested\(candidate\) && !navmesh.LoadTileIn\(candidate\)' 'Do not duplicate a pending load'
    Assert-AICFContains $failures 'APPROACH_IDENTITY' $Route 'target.GetOwner\(\).GetID\(\) == m_TargetId' 'Fence target entity identity'
    Assert-AICFContains $failures 'APPROACH_SHARED_FENCE' $Slot 'void MarkApproachRouteWaypoint\(\)\s*\{\s*MarkStuckRouteWaypoint\(\);' 'Reuse group/generation/revision/deadline fence'
    Assert-AICFContains $failures 'APPROACH_CLEANUP' $Slot 'protected void ClearRuntimeReferences\(\)\s*\{\s*m_ApproachRoute = null;' 'New group generation cannot retain route geometry'
    if ((ConvertTo-AICFCodeText $Route) -match 'CallLater|SpawnEntity|AddWaypoint|DeleteRplEntity|SetOrigin') { $failures.Add('[APPROACH_OWNER] Geometry helper cannot mutate entities or schedule callbacks') }
    return $failures.ToArray()
}
$failures = @(Test-Approach $route $planner $slot)
if ($failures.Count) { $failures; exit 1 }
$cases = @(
    @{Rule='APPROACH_PROJECTION';Old='vector.DistanceSqXZ(candidate, endpoint) > 64.0';New='false'},
    @{Rule='APPROACH_WATER';Old='ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, endpoint - "0 0.5 0")';New='false'},
    @{Rule='APPROACH_LOAD_BOUND';Old='System.GetTickCount(m_iTileWaitStartedAtMs) >= 30000';New='false'},
    @{Rule='APPROACH_LOAD_ONCE';Old='!navmesh.IsTileRequested(candidate) && !navmesh.LoadTileIn(candidate)';New='!navmesh.LoadTileIn(candidate)'},
    @{Rule='APPROACH_IDENTITY';Old='target.GetOwner().GetID() == m_TargetId';New='true'}
)
foreach ($case in $cases) {
    if (-not (@(Test-Approach $route.Replace($case.Old,$case.New) $planner $slot) -match $case.Rule)) { throw "Mutation escaped: $($case.Rule)" }
}
'Infantry approach contracts: PASS; negative mutations=5'

function Test-ApproachAuditOrder([string]$Source) {
    $body = Get-AICFMethodBody ([pscustomobject]@{Source=$Source;Code=(ConvertTo-AICFCodeText $Source)}) 'AuditActiveFactionTasking'
    return $body -match '(?s)slot.IsApproachRouteWaypoint\(\) && !slot.HasPendingOrderRecovery\(\).*?!m_VehicleCoordinator.IsControllingMovement\(slot\) && !m_VehicleCoordinator.IsRestorePending\(slot\).*?m_OrderPlanner.TryAdvanceStuckRoute\(slot, faction\);.*?bool meaningfulTask = HasMeaningfulTask'
}
if (!(Test-ApproachAuditOrder $controller)) { throw '[APPROACH_AUDIT_ORDER] Advance a physically confirmed leg before task-loss repair, respecting vehicle ownership' }
if (Test-ApproachAuditOrder $controller.Replace('m_OrderPlanner.TryAdvanceStuckRoute(slot, faction);', 'Print("removed");')) { throw 'Audit-order mutation escaped' }
'Approach task audit ordering: PASS; negative mutations=1'

function Test-RecoverySafety([string]$Planner, [string]$Controller) {
    $failures = [Collections.Generic.List[string]]::new()
    $endpoint = Get-AICFMethodBody ([pscustomobject]@{Source=$Planner;Code=(ConvertTo-AICFCodeText $Planner)}) 'TryResolveFalseCompletionEndpoint'
    $arrival = Get-AICFMethodBody ([pscustomobject]@{Source=$Planner;Code=(ConvertTo-AICFCodeText $Planner)}) 'HasCompletedApproachLeg'
    Assert-AICFContains $failures 'RECOVERY_WATER' $endpoint 'ChimeraWorldUtils.TryGetWaterSurfaceSimple\(world, candidate - "0 0.5 0"\)' 'Reachable does not imply dry infantry endpoint'
    Assert-AICFContains $failures 'RECOVERY_REJECTED' $endpoint 'vector.DistanceXZ\(candidate, rejectedEndpoint\) <= ATTACK_OPERATIONAL_RADIUS_METERS' 'Do not retry the same failed endpoint neighborhood'
    Assert-AICFContains $failures 'RECOVERY_ORIGIN' $endpoint 'vector.DistanceSqXZ\(origin, navmeshOrigin\) > 64.0' 'Do not snap origin onto a remote surface'
    Assert-AICFContains $failures 'LEG_CALLBACK' $arrival 'GetOwnedWaypointTerminalOutcome\(waypoint\) == "GROUP_CALLBACK_COMPLETED"' 'A removed waypoint is not completion'
    Assert-AICFContains $failures 'LEG_PHYSICAL' $arrival 'vector.DistanceXZ\(leader.GetOrigin\(\), waypoint.GetOrigin\(\)\) <= waypoint.GetCompletionRadius\(\)' 'A remote callback is not physical arrival'
    Assert-AICFContains $failures 'LEG_FENCE' $arrival '!slot.IsStuckRouteContextCurrent\(\)' 'Require generation and assignment identity'
    Assert-AICFContains $failures 'LEG_PENDING' $Controller '(?s)if \(m_OrderPlanner.HasCompletedApproachLeg\(slot, faction\)\).*?RecordPendingOrderRepairTerminal.*?slot.ClearPendingOrderRecovery\(\);.*?m_OrderPlanner.TryAdvanceStuckRoute\(slot, faction\);.*?string verificationFailureReason' 'Close accounting before replacing a completed pending leg'
    return $failures.ToArray()
}
$safety = @(Test-RecoverySafety $planner $controller)
if ($safety.Count) { $safety; exit 1 }
foreach ($mutation in @(
    @('RECOVERY_WATER','ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, candidate - "0 0.5 0")'),
    @('RECOVERY_REJECTED','vector.DistanceXZ(candidate, rejectedEndpoint) <= ATTACK_OPERATIONAL_RADIUS_METERS'),
    @('RECOVERY_ORIGIN','vector.DistanceSqXZ(origin, navmeshOrigin) > 64.0'),
    @('LEG_CALLBACK','slot.GetOwnedWaypointTerminalOutcome(waypoint) == "GROUP_CALLBACK_COMPLETED"'),
    @('LEG_PHYSICAL','vector.DistanceXZ(leader.GetOrigin(), waypoint.GetOrigin()) <= waypoint.GetCompletionRadius()'),
    @('LEG_FENCE','!slot.IsStuckRouteContextCurrent()')
)) {
    if (-not (@(Test-RecoverySafety $planner.Replace($mutation[1], 'false') $controller) -match $mutation[0])) { throw "Mutation escaped: $($mutation[0])" }
}
'Recovery safety contracts: PASS; negative mutations=6'
