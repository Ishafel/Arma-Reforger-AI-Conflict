param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($file in @('Forces/AICF_AINodeLifecycle.c','State/AICF_GroupSlot.c','Orders/AICF_OrderPlanner.c','Objectives/AICF_TargetSelector.c','Bootstrap/AICF_MatchController.c','Construction/AICF_StockConstructionAdapter.c','Construction/AICF_ConstructionPlanner.c','Forces/AICF_AIContactRequestGuard.c','Forces/AICF_AIRandomRadiusGuards.c')) {
    $sources[$file] = ConvertTo-AICFCodeText (Get-Content (Join-Path $root $file) -Raw -Encoding UTF8)
}
$rules = @(
    @('MOVE_NATIVE','Forces/AICF_AINodeLifecycle.c','return super.EOnTaskSimulate\(owner, dt\);'),
    @('MOVE_SCOPE','Forces/AICF_AINodeLifecycle.c','result != EMoveError.UNKNOWN'),
    @('MOVE_VEHICLE','Forces/AICF_AINodeLifecycle.c','handler != -1 \|\| !m_GroupUtilityComponent.m_VehicleMgr'),
    @('MOVE_SENTINEL_VEHICLE','Forces/AICF_AINodeLifecycle.c','m_GroupUtilityComponent.m_VehicleMgr.FindVehicleBySubgroupId\(handler\)'),
    @('MOVE_PHYSICAL_TRANSITION','Forces/AICF_AINodeLifecycle.c','access.IsInCompartment\(\) \|\| access.IsGettingIn\(\) \|\| access.IsGettingOut\(\)'),
    @('MOVE_CALLBACK_FENCE','Forces/AICF_AINodeLifecycle.c','slot.HasFailedMovement\(\) && m_GroupUtilityComponent.GetExecutedAction\(\) == failedAction'),
    @('MOVE_AUTHORITY','Forces/AICF_AINodeLifecycle.c','!Replication.IsServer\(\)'),
    @('MOVE_ACTIVITY','Forces/AICF_AINodeLifecycle.c','slot.ReportFailedMovement\(m_Group, activity.m_RelatedWaypoint\)'),
    @('MOVE_GENERATION','State/AICF_GroupSlot.c','m_iFailedMoveGeneration == m_iSpawnGeneration'),
    @('MOVE_ASSIGNMENT','State/AICF_GroupSlot.c','m_iFailedMoveAssignment == m_iStrategicAssignmentRevision'),
    @('MOVE_WAYPOINT','State/AICF_GroupSlot.c','m_FailedMoveWaypoint == m_Waypoint'),
    @('MOVE_OWNERSHIP','State/AICF_GroupSlot.c','waypoint != m_Waypoint'),
    @('MOVE_RECOVERY','Orders/AICF_OrderPlanner.c','if \(slot.HasFailedMovement\(\)\)\s*return'),
    @('MOVE_BOUNDED','Orders/AICF_OrderPlanner.c','slot.GetFalseCompletionNoProgressCount\(\) >= 3'),
    @('ROUTE_MEMORY','Orders/AICF_OrderPlanner.c','slot.DeferFailedRouteTarget\(target\)'),
    @('ROUTE_LIFECYCLE_HOLD','Orders/AICF_OrderPlanner.c','HoldPositionForTemporaryRouteReplan\(slot, faction, target, leader.GetOrigin\(\), false\)'),
    @('ROUTE_NO_COOLDOWN_EXTENSION','Orders/AICF_OrderPlanner.c','if \(deferTarget\)\s*slot.DeferFailedRouteTarget\(target\)'),
    @('ROUTE_FALLBACK','Bootstrap/AICF_MatchController.c','!replanned && !slot.IsRouteTargetDeferred\(failedTarget\)'),
    @('ROUTE_EXPIRY','State/AICF_GroupSlot.c','System.GetTickCount\(\) < m_aRejectedRouteUntil\[index\]'),
    @('ROUTE_SELECTOR','Objectives/AICF_TargetSelector.c','!routeSlot.IsRouteTargetDeferred\(currentBase\)'),
    @('ROUTE_SELECTOR_FALLBACK','Objectives/AICF_TargetSelector.c','SelectAttackTarget\(graph, faction, selectionMode, null, preferredIndex, routeSlot\)'),
    @('OUTLINE_ROAD','Construction/AICF_StockConstructionAdapter.c','(?s)SLOT_ROAD_SMALL.*?m_sSlotRoadSmallLayout.*?SLOT_ROAD_MEDIUM.*?m_sSlotRoadMediumLayout.*?SLOT_ROAD_LARGE.*?m_sSlotRoadLargeLayout'),
    @('SEARCH_BACKOFF','Construction/AICF_ConstructionPlanner.c','now < state.m_aSearchRetryAt\[type\]'),
    @('CONTACT_WAITING','Forces/AICF_AIContactRequestGuard.c','m_eState == SCR_EAICommunicationState.WAITING && AICF_IsInvalidContact\(m_CurrentRequest\)'),
    @('CONTACT_FAILURE','Forces/AICF_AIContactRequestGuard.c','FailRequest\(m_CurrentRequest\)'),
    @('CONTACT_NATIVE','Forces/AICF_AIContactRequestGuard.c','super.Update\(timeSlice\)'),
    @('RADIUS_NATIVE','Forces/AICF_AIRandomRadiusGuards.c','return super.FindPosition2D\(')
)
$failures = [Collections.Generic.List[string]]::new()
foreach ($rule in $rules) { Assert-AICFContains $failures $rule[0] $sources[$rule[1]] $rule[2] 'North failure contract missing' }
if ($failures.Count) { $failures; exit 1 }
# Каждая отрицательная мутация удаляет проверяемый guard из code-only текста.
foreach ($rule in $rules) {
    $mutated = [regex]::Replace($sources[$rule[1]], $rule[2], '')
    $rejected = [Collections.Generic.List[string]]::new()
    Assert-AICFContains $rejected $rule[0] $mutated $rule[2] 'Removed guard'
    if (!$rejected.Count) { throw "Mutation escaped: $($rule[0])" }
    Write-Output "PASS negative mutation=$($rule[0])"
}
Write-Output "North failure contracts: PASS; guards=$($rules.Count); runtime=NOT_RUN"
