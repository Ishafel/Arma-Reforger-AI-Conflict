param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$sourceRoot = Join-Path $RepositoryRoot 'AIConflictCore\Scripts\Game\AIConflict'

function Read-Required([string]$RelativePath) {
    $path = Join-Path $sourceRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        $failures.Add("[MAP_POINT_COMPONENT] Missing $RelativePath")
        return ''
    }
    return Get-Content -LiteralPath $path -Raw
}

function Assert-Contains([string]$Rule, [string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) { $failures.Add("[$Rule] $Message") }
}

function Assert-NotContains([string]$Rule, [string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -match $Pattern) { $failures.Add("[$Rule] $Message") }
}

$enums = Read-Required 'State\AICF_Stage1Enums.c'
$slot = Read-Required 'State\AICF_GroupSlot.c'
$snapshot = Read-Required 'State\Vehicles\AICF_StrategicAssignmentSnapshot.c'
$planner = Read-Required 'Orders\AICF_OrderPlanner.c'
$pointRequest = Read-Required 'Orders\AICF_PlayerPointOrderRequest.c'
$controller = Read-Required 'Bootstrap\AICF_MatchController.c'
$rpc = Read-Required 'UI\AICF_StrategicCommandRpc.c'
$ui = Read-Required 'UI\AICF_StrategicUI.c'
$markers = Read-Required 'UI\AICF_GroupMapMarkers.c'
$coordinator = Read-Required 'Vehicles\AICF_VehicleCoordinator.c'
$acquisition = Read-Required 'Vehicles\AICF_VehicleAcquisitionFlow.c'
$tripController = Read-Required 'Vehicles\AICF_TransportTripController.c'
$transit = Read-Required 'Vehicles\AICF_VehicleTransitFlow.c'
$handoff = Read-Required 'Vehicles\AICF_VehicleTaskHandoff.c'

$reconcileAICommander = [regex]::Match(
    $planner,
    'bool\s+ReconcileAICommanderOrder\s*\([\s\S]*?(?=\r?\n\s*bool\s+AssignLossResponseOrder\s*\()').Value
$lossResponseAICommander = [regex]::Match(
    $planner,
    'bool\s+AssignAICommanderLossResponseOrder\s*\([\s\S]*?(?=\r?\n\s*bool\s+IsOrderValid\s*\()').Value
$strategicBaseTargetValidator = [regex]::Match(
    $planner,
    'bool\s+IsStrategicTargetValid\s*\([\s\S]*?(?=\r?\n\s*bool\s+IsCurrentStrategicDestinationValid\s*\()').Value
$currentStrategicDestinationValidator = [regex]::Match(
    $planner,
    'bool\s+IsCurrentStrategicDestinationValid\s*\([\s\S]*?(?=\r?\n\s*string\s+GetOrderFailureReason\s*\()').Value
$buildOrderTargets = [regex]::Match(
    $controller,
    'protected\s+string\s+BuildOrderTargets\s*\([\s\S]*?(?=\r?\n\s*protected\s+string\s+BuildBaseLabel\s*\()').Value

Assert-Contains 'MAP_POINT_MODEL' $enums 'enum\s+AICF_EOrderTargetKind[\s\S]*BASE[\s\S]*POSITION' 'Target identity must explicitly distinguish BASE and POSITION'
Assert-Contains 'MAP_POINT_MODEL' $slot 'm_TargetKind[\s\S]*m_vTargetPosition[\s\S]*CommitStrategicPointIntent' 'Runtime and durable intent must retain point identity and coordinates'
Assert-Contains 'MAP_POINT_MODEL' $snapshot 'm_TargetKind[\s\S]*MatchesDestination[\s\S]*GetTargetPosition' 'Vehicle snapshot must carry and compare point destination identity'

Assert-Contains 'MAP_POINT_RPC_TRUST' $rpc 'AICF_RequestStrategicPointOrder\s*\(\s*int\s+slotId\s*,\s*vector\s+clientPosition\s*\)[\s\S]*RpcAsk_AICFStrategicPointOrder' 'Client RPC must carry only slot and coordinate intent'
Assert-NotContains 'MAP_POINT_RPC_TRUST' $rpc 'AICF_RequestStrategicPointOrder\s*\([^\)]*(?:playerId|faction|targetKind|label|waypoint)' 'Client point RPC must not submit authoritative identity or marker metadata'
Assert-Contains 'MAP_POINT_RPC_TRUST' $controller 'RequestPlayerPointOrder[\s\S]*SGetPlayerFaction\s*\(\s*playerId\s*\)[\s\S]*GetSlot\s*\(\s*slotId\s*\)' 'Authority must re-resolve player faction and stable slot'
Assert-Contains 'MAP_POINT_RPC_TRUST' $controller 'ConsumePlayerOrderRateLimit\s*\(\s*playerId\s*\)[\s\S]*TryResolvePlayerPointTarget' 'Point commands must share the authoritative order rate limit before validation'
Assert-Contains 'MAP_POINT_RPC_RESULT' $controller 'RequestPlayerPointOrder\s*\([\s\S]*out\s+string\s+rejectionReason[\s\S]*out\s+vector\s+resolvedPosition' 'Authority must return its exact point-order decision to the owned RPC boundary'
Assert-Contains 'MAP_POINT_RPC_RESULT' $rpc 'RplRcver\.Owner[\s\S]*RpcDo_AICFStrategicPointOrderResult[\s\S]*m_sAICFStrategicPointOrderResultReason[\s\S]*m_iAICFStrategicPointOrderResultSequence\+\+' 'Accepted and rejected point decisions must return only to the requesting controller owner'
Assert-Contains 'MAP_POINT_RPC_RESULT' $rpc 'responseDeferred[\s\S]*AICF_SendStrategicPointOrderResult' 'The RPC boundary must suppress its immediate reply while authoritative navmesh validation is pending'

Assert-Contains 'MAP_POINT_VALIDATION' $planner 'TryResolvePlayerPointTarget[\s\S]*COORDINATE_NOT_FINITE[\s\S]*GetTerrainBoundBox[\s\S]*OUTSIDE_WORLD_BOUNDS' 'Server must reject non-finite and out-of-world coordinates'
Assert-Contains 'MAP_POINT_VALIDATION' $planner 'GetSurfaceY\s*\([\s\S]*GetClosestPositionOnNavmesh[\s\S]*PLAYER_POINT_MAX_NAVMESH_OFFSET_METERS' 'Server must rebuild terrain Y and require a bounded nearby navmesh endpoint'
Assert-Contains 'MAP_POINT_NAVMESH_STREAMING' $planner 'GetNavmeshComponent[\s\S]*IsTileLoaded[\s\S]*IsTileRequested[\s\S]*LoadTileIn[\s\S]*NAVMESH_TILE_LOADING' 'Remote point validation must request its streamed navmesh tile before querying the endpoint'
Assert-Contains 'MAP_POINT_NAVMESH_STREAMING' $controller 'PLAYER_POINT_NAVMESH_TIMEOUT_MS[\s\S]*ProcessPendingPlayerPointOrders[\s\S]*NAVMESH_TILE_TIMEOUT[\s\S]*SchedulePendingPlayerPointOrders' 'Navmesh validation retries must be asynchronous and bounded'
Assert-Contains 'MAP_POINT_NAVMESH_IDENTITY' $pointRequest 'm_Requester[\s\S]*m_Group[\s\S]*m_iGroupGeneration[\s\S]*m_iAssignmentRevision[\s\S]*m_iIntentRevision[\s\S]*m_iRequestToken' 'A delayed point request must retain immutable requester, entity, generation, revision and token identity'
Assert-Contains 'MAP_POINT_NAVMESH_IDENTITY' $controller 'REQUESTER_IDENTITY_CHANGED[\s\S]*GetGroupGeneration[\s\S]*GetAssignmentRevision[\s\S]*GetIntentRevision' 'Every delayed retry must compare requester and slot identity'
Assert-Contains 'MAP_POINT_NAVMESH_IDENTITY' $controller 'GetIntentRevision[\s\S]*REQUEST_IDENTITY_CHANGED' 'A changed delayed-request identity must terminate fail-closed before assignment'
Assert-Contains 'MAP_POINT_NAVMESH_CLEANUP' $controller 'void\s+Stop[\s\S]*Remove\s*\(\s*ProcessPendingPlayerPointOrders\s*\)[\s\S]*CancelPendingPlayerPointOrders' 'Controller Stop must cancel the navmesh retry callback and resolve pending owner responses'
Assert-Contains 'MAP_POINT_VALIDATION' $controller 'PLAYER_ORDER_REJECTED[\s\S]*reason=%[0-9][\s\S]*authority=SERVER' 'Rejected point commands must extend the existing event with a machine-readable authority reason'

Assert-Contains 'MAP_POINT_WAYPOINT' $planner 'CreatePositionWaypoint[\s\S]*DEFEND_WAYPOINT_PREFAB[\s\S]*EAIWaypointCompletionType\.All[\s\S]*SetHoldingTime' 'Point orders must create a durable move-and-hold waypoint'
Assert-Contains 'MAP_POINT_WAYPOINT' $planner 'ReplacePointOrder[\s\S]*RemoveWaypoint[\s\S]*DeleteRplEntity[\s\S]*AddWaypointAt[\s\S]*AssignPointObjective' 'Planner must remain the ordered remove/delete/add owner for point waypoints'
Assert-NotContains 'MAP_POINT_WAYPOINT' ($rpc + $ui + $controller) '(?:SpawnEntityPrefabEx|AddWaypointAt|RemoveWaypoint)\s*\(' 'UI, RPC and MatchController must not become a second infantry waypoint writer'
Assert-Contains 'MAP_POINT_WAYPOINT' $planner 'PromoteAttackToObjectiveAction[\s\S]*GetTargetKind\s*\(\s*\)\s*!=\s*AICF_EOrderTargetKind\.BASE' 'Point orders must never enter base capture/SearchAndDestroy promotion'
Assert-Contains 'MAP_POINT_BASE_REGRESSION' $planner 'AssignPlayerOrder[\s\S]*IsTargetValidForRole\s*\([\s\S]*ReplaceOrder' 'Existing BASE player orders must retain role/ownership validation'
Assert-Contains 'MAP_POINT_BASE_TARGET_FILTER' $strategicBaseTargetValidator 'return\s+IsTargetValidForRole\s*\(' 'A BASE candidate must always pass the selected role and ownership filter'
Assert-NotContains 'MAP_POINT_BASE_TARGET_FILTER' $strategicBaseTargetValidator 'GetTargetKind|POSITION|IsCurrentTargetValid' 'A current POSITION order must not make every BASE candidate valid'
Assert-Contains 'MAP_POINT_CURRENT_DESTINATION' $currentStrategicDestinationValidator 'return\s+IsCurrentTargetValid\s*\(' 'Current BASE/POSITION maintenance must use destination-aware validity'
Assert-Contains 'MAP_POINT_BASE_TARGET_FILTER' $buildOrderTargets 'IsStrategicTargetValid\s*\(\s*attackSlot[\s\S]*IsStrategicTargetValid\s*\(\s*defendSlot[\s\S]*IsStrategicTargetValid\s*\(\s*reserveSlot' 'Replicated ATTACK/DEFEND/RESERVE lists must filter each BASE candidate independently of the current destination kind'
Assert-Contains 'MAP_POINT_CURRENT_DESTINATION' $controller 'IsPersistentStuckFieldHold[\s\S]*IsCurrentStrategicDestinationValid' 'Persistent current-destination maintenance must preserve valid POSITION orders without weakening BASE candidate filtering'

Assert-Contains 'MAP_POINT_DURABILITY' $planner 'RestorePlayerStrategicIntent[\s\S]*POSITION[\s\S]*ReplacePointOrder' 'Replacement and replan paths must restore durable point intent'
Assert-Contains 'MAP_POINT_DURABILITY' $slot 'IsPlayerStrategicIntentRoleCurrent[\s\S]*POSITION[\s\S]*m_StrategicIntentRole\s*==\s*m_Role' 'POSITION intent must survive role changes while BASE intent remains role-bound'
Assert-NotContains 'MAP_POINT_DURABILITY' ([regex]::Match($slot, 'protected\s+void\s+ClearRuntimeReferences[\s\S]*?\n\s*\}').Value) 'ClearStrategicIntent|ClearPlayerStrategicIntent' 'Replacement runtime cleanup must preserve point intent'
Assert-Contains 'MAP_POINT_DURABILITY' $planner 'AssignOrder[\s\S]*HasPlayerStrategicIntent[\s\S]*POSITION[\s\S]*RestorePlayerStrategicIntent' 'AI commander entry must restore valid player point intent before autonomous selection'
Assert-Contains 'MAP_POINT_REPLAN_PRIORITY' $reconcileAICommander 'GetPlayerStrategicIntentTargetKind\s*\(\s*\)\s*==\s*AICF_EOrderTargetKind\.POSITION[\s\S]*IsStrategicIntentDestinationValid[\s\S]*HasPlayerStrategicOrder[\s\S]*GetOrderFailureReason[\s\S]*ApplySuspendedPointAssignment[\s\S]*RestorePlayerStrategicIntent' 'Periodic commander reconcile must preserve or restore valid POSITION player intent instead of selecting a BASE'
Assert-Contains 'MAP_POINT_REPLAN_PRIORITY' $lossResponseAICommander 'GetPlayerStrategicIntentTargetKind\s*\(\s*\)\s*==\s*AICF_EOrderTargetKind\.POSITION[\s\S]*IsStrategicIntentDestinationValid[\s\S]*HasPlayerStrategicOrder[\s\S]*GetOrderFailureReason[\s\S]*ApplySuspendedPointAssignment[\s\S]*RestorePlayerStrategicIntent' 'Base-loss response must preserve or restore valid POSITION player intent instead of selecting a QRF BASE'
Assert-Contains 'MAP_POINT_DURABILITY' $controller 'roleChanged[\s\S]*waypointSuspendedByVehicle[\s\S]*POSITION[\s\S]*TouchCommanderConfiguration' 'A role change under vehicle ownership must advance point assignment identity without clearing intent'
Assert-Contains 'MAP_POINT_DURABILITY' $controller 'HasMeaningfulTask[\s\S]*HasStrategicDestination' 'Reliability must treat a point destination as a meaningful strategic task'
Assert-Contains 'MAP_POINT_DURABILITY' $controller 'ObserveCurrentDestinationProgress[\s\S]*RebuildCurrentOrder' 'Point progress must use the normal bounded stuck-recovery path'
Assert-Contains 'MAP_POINT_DURABILITY' $controller 'RefreshCompletedPointHold[\s\S]*ConfirmAtCurrentDestination[\s\S]*RebuildCurrentOrder' 'A completed point hold must receive another meaningful hold task instead of becoming taskless'

Assert-Contains 'MAP_POINT_VEHICLE' $coordinator 'MatchesDestination|GetTargetKind\s*\(\s*\)\s*==\s*slot\.GetTargetKind' 'Vehicle coordination must reject destination-kind or coordinate drift'
Assert-Contains 'MAP_POINT_VEHICLE' $acquisition '!currentAssignment\.MatchesDestination\s*\(\s*observed\s*\)' 'Acquisition must reset stale point requests by full destination identity'
Assert-Contains 'MAP_POINT_VEHICLE' $tripController '!currentAssignment\.MatchesDestination\s*\(\s*observed\s*\)' 'Trip controller must fail closed on point destination changes'
Assert-NotContains 'MAP_POINT_VEHICLE' $transit '!assignment\.GetTargetBase\s*\(\s*\)' 'Transit validity must not require a synthetic base for a point order'
Assert-Contains 'MAP_POINT_VEHICLE' $handoff 'HasStrategicDestination\s*\(\s*\)' 'Vehicle handoff must restore BASE or POSITION assignments'

Assert-Contains 'MAP_POINT_UI' $ui 'MOVE TO MAP POINT / УКАЗАТЬ ТОЧКУ НА КАРТЕ' 'Command panel must expose the bilingual point-order action'
Assert-Contains 'MAP_POINT_UI' $ui 'new\s+SCR_MapCommandCursor[\s\S]*GetOnCommandExecuted\s*\(\s*\)\.Insert[\s\S]*ShowCursor' 'Selection must reuse the stock map command cursor'
Assert-Contains 'MAP_POINT_UI_INPUT' $ui 'BeginMapPointSelection[\s\S]*CallLater\s*\(\s*ActivateMapPointCursor[\s\S]*protected\s+void\s+ActivateMapPointCursor' 'Cursor activation must be deferred beyond the button click input event'
Assert-Contains 'MAP_POINT_UI_INPUT' $ui 'DisableMapPointCursor[\s\S]*Remove\s*\(\s*ActivateMapPointCursor\s*\)' 'All cursor cleanup paths must also cancel deferred activation'
Assert-Contains 'MAP_POINT_UI' $ui 'CANCEL_MAP_POINT[\s\S]*DisableSelection[\s\S]*RemoveMapPointPrompt' 'Map selection must provide explicit cleanup and cancellation'
Assert-Contains 'MAP_POINT_UI' $ui 'OnMapClose[\s\S]*RemoveMapUI[\s\S]*RemoveMapUI[\s\S]*DisableMapPointCursor' 'Map close and shared UI teardown must cancel cursor ownership'
Assert-Contains 'MAP_POINT_UI' $ui 'void\s+Stop[\s\S]*RemoveMapUI' 'Controller Stop must use the same complete map-selection teardown'
Assert-Contains 'MAP_POINT_UI' $ui 'ObservePendingMapPointOrder[\s\S]*intentRevision|m_iMapPointPendingIntentRevision' 'UI confirmation must wait for replicated authoritative state'
Assert-Contains 'MAP_POINT_UI_RESULT' $ui 'ObservePointOrderServerResult[\s\S]*AICF_GetStrategicPointOrderResultSlot[\s\S]*AICF_GetStrategicPointOrderResultRequest[\s\S]*BuildPointOrderRejectionStatus' 'UI must correlate and present the exact owner-only server result instead of waiting for rejection timeout'
Assert-Contains 'MAP_POINT_UI_RESULT' $ui 'NO_NAVMESH_ENDPOINT_NEARBY[\s\S]*NAVMESH_ENDPOINT_OUT_OF_RANGE' 'Non-navigable map clicks must produce an actionable rejection status'
Assert-NotContains 'MAP_POINT_UI_RESULT' $ui 'REJECTED OR TIMED OUT' 'A server rejection must not be conflated with transport or replication timeout'
Assert-Contains 'MAP_POINT_UI_COLOR' $ui 'RefreshVisualStyles[\s\S]*SetRectColor\s*\(\s*m_wMapPointPrompt\s*,\s*Color\.FromSRGBA\s*\(\s*6\s*,\s*18\s*,\s*25[\s\S]*SetRectColor\s*\(\s*m_wMapPointCancel' 'Prompt and cancel colors must be restored after Enfusion widget initialization'
Assert-Contains 'MAP_POINT_UI_COLOR' $ui 'targetButton\s*==\s*m_wMapPointButton[\s\S]*Color\.FromSRGBA\s*\(\s*24\s*,\s*79\s*,\s*105' 'The blue map-point action must not be overwritten by the amber base-target style'

Assert-Contains 'MAP_POINT_MARKER' $markers 'PrepareMilitaryMarker[\s\S]*SetWorldPos[\s\S]*AddMarkerFactionFlags[\s\S]*InsertStaticMarker\s*\(\s*marker\s*,\s*false\s*,\s*true\s*\)' 'Destination marker must be faction-filtered static server state for JIP'
Assert-Contains 'MAP_POINT_MARKER' $markers 'RemoveDestinationMarker[\s\S]*RemoveStaticMarker' 'Point marker lifecycle must remove superseded and stopped state'
Assert-Contains 'MAP_POINT_MARKER' $markers 'SyncDestinationMarker[\s\S]*RemoveDestinationMarker\s*\(\s*markerIndex\s*\)[\s\S]*PrepareMilitaryMarker' 'A new/base/cleared intent must remove the previous slot marker before replacement'
Assert-Contains 'MAP_POINT_MARKER' $markers 'void\s+Stop[\s\S]*RemoveDestinationMarker\s*\(' 'Marker system Stop must delete all custom static markers'

Assert-Contains 'MAP_POINT_DIAGNOSTICS' ($planner + $controller + $markers) 'target_kind=POSITION' 'Point lifecycle diagnostics must identify POSITION explicitly'
Assert-Contains 'MAP_POINT_DIAGNOSTICS' ($planner + $controller + $markers) 'stable_slot=%[0-9][\s\S]*numeric_slot=%[0-9][\s\S]*(?:assignment_revision|intent_revision)' 'Diagnostics must carry stable/numeric slot identity and revisions'

if ($failures.Count -gt 0) {
    Write-Host "Map point orders static audit: FAIL ($($failures.Count) issue(s))"
    foreach ($failure in $failures) { Write-Host " - $failure" }
    exit 1
}

Write-Host 'Map point orders static audit: PASS'
Write-Host 'model=PASS rpc_trust=PASS rpc_result=PASS validation=PASS navmesh_streaming=PASS waypoint=PASS base_target_filter=PASS current_destination=PASS durability=PASS replan_priority=PASS vehicle=PASS ui=PASS marker_jip=PASS diagnostics=PASS'
