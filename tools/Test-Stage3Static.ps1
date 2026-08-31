param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage3StaticAudit.Common.ps1')

$failures = [System.Collections.Generic.List[string]]::new()

function Add-Stage3Failures {
    param([string[]]$Items)
    foreach ($item in $Items) {
        if ($item) { $failures.Add($item) }
    }
}

function Get-Stage3RecordByName {
    param([object[]]$Records, [string]$Name)
    $matches = @($Records | Where-Object { $_.Name -eq $Name })
    if ($matches.Count -eq 1) { return $matches[0] }
    return $null
}

function Require-Stage3Record {
    param([object]$Record, [string]$Name)
    if (-not $Record) {
        Add-AICFAuditFailure $failures 'STAGE3_COMPONENT_MISSING' "Missing unique source file $Name"
        return $false
    }
    return $true
}

$records = @(Get-AICFSourceRecords $RepositoryRoot)
if ($records.Count -eq 0) {
    Add-AICFAuditFailure $failures 'SOURCE_TREE_MISSING' 'No AIConflictCore Enforce sources were found'
}

Add-Stage3Failures @(Invoke-AICFVehicleArchitectureAudit -RepositoryRoot $RepositoryRoot)
Add-Stage3Failures @(Invoke-AICFArchitectureNegativeSelfCheck)

$config = Get-Stage3RecordByName $records 'AICF_Stage3Config.c'
if (Require-Stage3Record $config 'AICF_Stage3Config.c') {
    Assert-AICFContains $failures 'STAGE3_ALWAYS_ON' $config.Code 'bool\s+GetVehiclesEnabled\s*\(\s*\)\s*\{\s*return\s+true\s*;\s*\}' 'Vehicle subsystem compatibility accessor must remain permanently enabled'
    Assert-AICFNotContains $failures 'STAGE3_ALWAYS_ON' $config.Code 'm_bVehiclesEnabled' 'Vehicle subsystem must not retain mutable enable state'
    Assert-AICFNotContains $failures 'STAGE3_ALWAYS_ON' $config.Source '"aicfVehiclesEnabled"' 'Vehicle subsystem must not expose a CLI opt-out'
    Assert-AICFContains $failures 'STAGE3_BOUNDED_REQUEST' $config.Code 'DEFAULT_SPAWN_MAX_ATTEMPTS\s*=\s*4\s*;' 'Spawn request attempts must default to four'
    Assert-AICFContains $failures 'STAGE3_BOUNDED_REQUEST' $config.Code 'DEFAULT_WAIT_PROBE_INTERVAL_MS\s*=\s*60000\s*;' 'WAITING_FOR_SITE probe must default to sixty seconds'
	Assert-AICFContains $failures 'STAGE3_APPROACH_DEADLINE' $config.Code 'DEFAULT_BOARDING_APPROACH_TIMEOUT_MS\s*=\s*60000\s*;' 'Per-member boarding approach must have its own bounded sixty-second deadline'
	Assert-AICFContains $failures 'STAGE3_APPROACH_DEADLINE' $config.Source '"aicfVehicleBoardingApproachTimeoutMs"' 'Approach deadline must retain an explicit CLI override'
	Assert-AICFContains $failures 'STAGE3_PHASE_DEADLINES' $config.Code 'DEFAULT_BOARDING_TIMEOUT_MS\s*=\s*25000\s*;' 'Driver and gunner phases must default to twenty-five seconds'
	Assert-AICFContains $failures 'STAGE3_PHASE_DEADLINES' $config.Code 'DEFAULT_PASSENGER_BOARDING_TIMEOUT_MS\s*=\s*30000\s*;' 'Passenger phase must retain its separate thirty-second deadline'
	Assert-AICFContains $failures 'STAGE3_PHASE_DEADLINES' $config.Code 'DEFAULT_DISMOUNT_TIMEOUT_MS\s*=\s*20000\s*;' 'Normal dismount must default to twenty seconds'
    Assert-AICFContains $failures 'STAGE3_PROGRESS_EVIDENCE' $config.Code 'DEFAULT_MOTION_METERS\s*=\s*3\.0' 'Physical motion evidence must retain the independent three-metre threshold'
    Assert-AICFContains $failures 'STAGE3_PROGRESS_EVIDENCE' $config.Code 'DEFAULT_OBJECTIVE_PROGRESS_TIMEOUT_MS\s*=\s*300000' 'Objective progress must retain its independent five-minute timeout'
    Assert-AICFContains $failures 'STAGE3_MINIMUM_REQUEST_ROSTER' $config.Code 'DEFAULT_MINIMUM_VEHICLE_REQUEST_AGENTS\s*=\s*3\s*;' 'New vehicle requests must require the accepted three-member minimum'
    Assert-AICFContains $failures 'STAGE3_FACTION_CAP' $config.Code 'DEFAULT_MAX_VEHICLES_PER_FACTION\s*=\s*10\s*;' 'Faction active/reserved lease cap must cover all ten commander-configurable groups'
    Assert-AICFContains $failures 'STAGE3_COHESION_DEADLINE' $config.Code 'DEFAULT_COHESION_WAIT_TIMEOUT_MS\s*=\s*300000\s*;' 'Fragmented cohesion wait must have a five-minute absolute deadline'
    foreach ($cliName in @('aicfVehicleMinimumRequestAgents', 'aicfVehicleCohesionWaitTimeoutMs', 'aicfMaxVehiclesPerFaction')) {
        Assert-AICFContains $failures 'STAGE3_CONFIG_CLI' $config.Source ([regex]::Escape('"' + $cliName + '"')) "Missing documented CLI option $cliName"
    }
}

$matchController = Find-AICFClassRecord $records 'AICF_MatchController'
if ($matchController) {
    Assert-AICFContains $failures 'STAGE3_ALWAYS_ON' $matchController.Code 'm_VehicleCoordinator\s*=\s*new\s+AICF_VehicleCoordinator' 'Match controller must always compose the vehicle subsystem'
    Assert-AICFNotContains $failures 'STAGE3_ALWAYS_ON' $matchController.Code 'if\s*\(\s*m_Stage3Config\.GetVehiclesEnabled\s*\(' 'Vehicle coordinator construction must not be feature-gated'
}

$spawner = Find-AICFClassRecord $records 'AICF_VehicleSpawner'
if ($spawner) {
    $selectForAcquisitionBody = Get-AICFMethodBody $spawner 'TrySelectSiteForAcquisition'
    $spawnSelectedBody = Get-AICFMethodBody $spawner 'TrySpawnSelectedSiteForAcquisition'
    Assert-AICFContains $failures 'STAGE3_DETERMINISTIC_BASES' $spawner.Code 'preferredDistanceMeters[\s\S]*BaseKey[\s\S]*Compare[\s\S]*InsertAt' 'Friendly spawn bases must be deterministically ordered by distance and stable base key'
    Assert-AICFContains $failures 'STAGE3_SAFE_SITE_PREFLIGHT' $spawner.Code 'FindAllEmptyTerrainPositions[\s\S]*IsWheeledSpawnSurfaceSuitable[\s\S]*(?:MeasureAliveGroupDistancesToPosition|MeasureAliveGroupDistances)[\s\S]*farthestDistanceMeters[\s\S]*SELECTED' 'Every site must pass multi-position surface and all-member distance gates before selection'
    Assert-AICFContains $failures 'STAGE3_SAFE_SITE_PREFLIGHT' (ConvertTo-AICFCodeText $selectForAcquisitionBody) 'waitPreflight|preflightOnly' 'WAITING_FOR_SITE must use the entity-free acquisition selector'
    Assert-AICFNotContains $failures 'STAGE3_SAFE_SITE_PREFLIGHT' (ConvertTo-AICFCodeText $selectForAcquisitionBody) 'SpawnSelectedPrefab|SpawnEntityPrefabEx' 'WAITING_FOR_SITE selector must not allocate an entity'
    Assert-AICFContains $failures 'STAGE3_SAFE_SITE_PREFLIGHT' (ConvertTo-AICFCodeText $spawnSelectedBody) 'SpawnSelectedPrefab' 'ACQUIRING must use the sole entity-creation boundary only after site selection'
    Assert-AICFNotContains $failures 'STAGE3_REJECTED_GENERATION' $spawner.Code '(?:VehicleGeneration|AcceptedGeneration)\s*(?:\+\+|=)' 'Rejected spawn candidates must not advance accepted vehicle generation in the spawner'
}

$acquisition = Find-AICFClassRecord $records 'AICF_VehicleAcquisitionFlow'
if ($acquisition) {
    $waitHeartbeat = Get-AICFMethodBody $acquisition 'ReportWaitHeartbeat'
    $waitHeartbeatCode = ConvertTo-AICFCodeText $waitHeartbeat
    $processWaiting = ConvertTo-AICFCodeText (Get-AICFMethodBody $acquisition 'ProcessWaitingForSite')
    Assert-AICFContains $failures 'STAGE3_SPAWN_WAIT_TELEMETRY' $waitHeartbeat 'VEHICLE_SPAWN_WAIT_HEARTBEAT' 'Acquisition must emit the documented spawn-wait heartbeat'
    foreach ($field in @(
        'reason=', 'wait_age_ms=', 'total_wait_age_ms=', 'next_probe_at_ms=',
        'cumulative_attempts=', 'context_resets=', 'context_reset_reason=',
        'cohesion_spread_m=', 'cohesion_wait_age_ms=', 'active_or_reserved=',
        'limit=', 'cap_reserved=0'
    )) {
        Assert-AICFContains $failures 'STAGE3_SPAWN_WAIT_TELEMETRY' $waitHeartbeat ([regex]::Escape($field)) "Spawn-wait heartbeat omits truthful field $field"
    }
    foreach ($evidence in @(
        'GetWaitAgeMs\s*\(', 'GetTotalWaitAgeMs\s*\(', 'GetTotalAttemptCount\s*\(',
        'GetContextResetCount\s*\(', 'GetContextResetReason\s*\(',
        'GetCohesionSpreadMeters\s*\(', 'GetCohesionWaitAgeMs\s*\(',
        'GetActiveOrReservedCount\s*\(', 'GetMaximumActiveOrReserved\s*\('
    )) {
        Assert-AICFContains $failures 'STAGE3_SPAWN_WAIT_TELEMETRY' $waitHeartbeatCode $evidence 'Spawn-wait telemetry must derive values from request/fleet state'
    }
    Assert-AICFContains $failures 'STAGE3_SPAWN_WAIT_RATE_LIMIT' $processWaiting 'GetNextAttemptAtMs\s*\(\s*\)\s*>\s*nowMs[\s\S]*ReportWaitHeartbeat\s*\(' 'Spawn-wait heartbeat must be gated by the absolute next-probe deadline'
    Assert-AICFContains $failures 'STAGE3_SPAWN_WAIT_RATE_LIMIT' $processWaiting 'EnterWaitingForSite\s*\([\s\S]*nextProbeAtMs[\s\S]*ReportWaitHeartbeat\s*\(' 'Every emitted heartbeat must schedule the next bounded probe first'
} else {
    Add-AICFAuditFailure $failures 'STAGE3_COMPONENT_MISSING' 'Missing AICF_VehicleAcquisitionFlow'
}

$boarding = Find-AICFClassRecord $records 'AICF_VehicleBoardingFlow'
if ($boarding) {
    $boardingHelpers = @($records | Where-Object { $_.Name -match 'Boarding|Passenger' })
    $boardingCode = ($boardingHelpers | ForEach-Object { $_.Code }) -join "`n"
    $maintainPassengers = Get-AICFMethodBody $boarding 'MaintainPassengerActions'
    $tokenContext = Get-AICFMethodBody $boarding 'DescribeTokenContext'
    $tokenContextStrings = $boarding.Strings
    Assert-AICFContains $failures 'STAGE3_BOARDING_PHASE_ORDER' $boardingCode 'DRIVER[\s\S]*(?:GUNNER[\s\S]*)?PASSENGERS' 'Boarding must preserve DRIVER, optional GUNNER, then PASSENGERS ordering'
    Assert-AICFContains $failures 'STAGE3_EXACT_CARGO' $boardingCode 'CargoCompartmentSlot[\s\S]*SetReserved[\s\S]*SCR_AIGetInVehicle' 'Passenger boarding must reserve exact Cargo slots before exact GetIn actions'
    Assert-AICFContains $failures 'STAGE3_EXACT_CARGO' $boardingCode 'IsReservedBy[\s\S]*SetReserved\s*\(\s*null\s*\)' 'Reservation cancellation must be exact-owner safe'
    Assert-AICFContains $failures 'STAGE3_TRANSITION_FENCE' $boardingCode 'IsGettingIn[\s\S]*IsGettingOut' 'Passenger retries/cancellation must fence compartment transitions'
    Assert-AICFContains $failures 'STAGE3_ALL_OR_FALLBACK' $boardingCode 'settled|Settled' 'Boarding must retain authoritative settled evidence'
    Assert-AICFContains $failures 'STAGE3_ALL_OR_FALLBACK' $boardingCode '(?:SETTLED_POLLS_REQUIRED\s*=\s*2|settledPolls\s*>?=\s*2)' 'Boarding completion must require two settled polls'
    Assert-AICFNotContains $failures 'STAGE3_NO_GROUP_BOARDING_WAYPOINT' $boardingCode 'CreatePassengerBoardingWaypoint|SCR_BoardingEntityWaypoint' 'Boarding must not reintroduce a generic group vehicle waypoint'
    Assert-AICFContains $failures 'STAGE3_PER_MEMBER_APPROACH' $boardingCode 'MoveIndividually|ApproachAction|TrackBoardingApproach' 'Boarding approach must remain per-member and token-owned'
	Assert-AICFContains $failures 'STAGE3_APPROACH_DEADLINE' $boarding.Code 'GetPassengerBoardingTimeoutMs[\s\S]*GetBoardingApproachTimeoutMs[\s\S]*totalTimeoutMs\s*\+=\s*approachTimeoutMs' 'Approach and passenger phases must contribute independent immutable total-budget slices'
	Assert-AICFContains $failures 'STAGE3_RUNNING_CARGO_STALL' $boarding.Code 'EXACT_CARGO_READY_RETRY_STALL_MS\s*=\s*5000' 'Exact Cargo must start bounded recovery after five seconds without useful progress'
	Assert-AICFContains $failures 'STAGE3_RUNNING_CARGO_STALL' $maintainPassengers 'ObserveSpatialProgress[\s\S]*GetProgressAgeMs[\s\S]*EAIActionState\.RUNNING[\s\S]*configuredStallMs[\s\S]*ReissueExactCargo' 'Every unlinked non-transitioning RUNNING Cargo token must retain a bounded progress-aware exact retry before hidden recovery'
    Assert-AICFContains $failures 'STAGE3_RUNNING_CARGO_STALL' $maintainPassengers 'IsGettingIn[\s\S]*IsGettingOut[\s\S]*continue;' 'Cargo retries must remain fenced while a compartment transition is active'
	foreach ($field in @('current_distance_m=', 'best_distance_m=', 'progress_age_ms=', 'accessible=', 'get_in_locked=', 'occupied=')) {
        Assert-AICFContains $failures 'STAGE3_CARGO_TOKEN_DIAGNOSTICS' $tokenContextStrings ([regex]::Escape($field)) "Exact Cargo token diagnostics omit $field"
	}
	foreach ($field in @('phase=', 'blocker_member=', 'distance_m=', 'action_state=', 'retry=', 'seat=', 'linked=', 'getting_in=', 'recovery_fence=', 'deadline_remaining_ms=')) {
		Assert-AICFContains $failures 'STAGE3_BOARDING_BLOCKER_DIAGNOSTICS' $boarding.Strings ([regex]::Escape($field)) "Boarding blocker diagnostics omit $field"
	}
	Assert-AICFContains $failures 'STAGE3_PARTIAL_EXACT_CARGO' $boarding.Code 'AICF_VehiclePassengerSeatPlan[\s\S]*IssueReadyPassengerPlanActions[\s\S]*blockedCount' 'Passenger boarding must retain one immutable exact-seat plan while ready pairs progress independently'
	Assert-AICFContains $failures 'STAGE3_HIDDEN_RECOVERY_WAIT' $boarding.Source 'PASSENGER_HIDDEN_EXACT_CARGO_DEFERRED[\s\S]*WAIT_UNTIL_PHASE_DEADLINE' 'An unsafe hidden-recovery fence must wait until the phase deadline instead of failing the trip immediately'
}

$transit = Find-AICFClassRecord $records 'AICF_VehicleTransitFlow'
if ($transit) {
    $transitTick = Get-AICFMethodBody $transit 'Tick'
    $prepareRoute = Get-AICFMethodBody $transit 'PrepareRoute'
    Assert-AICFContains $failures 'STAGE3_TRANSIT_PROGRESS' $transit.Code 'routeProgress|RouteProgress' 'Transit must retain route-progress evidence'
    Assert-AICFContains $failures 'STAGE3_TRANSIT_PROGRESS' $transit.Code 'physicalMotion|PhysicalMotion|ObserveMotion' 'Transit must retain independent physical-motion evidence'
    Assert-AICFContains $failures 'STAGE3_RECOVERY_BUDGETS' $transit.Code 'crewRecovery|CrewRecovery' 'Transit must retain a separate crew-recovery budget'
    Assert-AICFContains $failures 'STAGE3_RECOVERY_BUDGETS' $transit.Code 'mobility|Mobility|Unstuck' 'Transit must retain a separate mobility-recovery budget'
    Assert-AICFContains $failures 'STAGE3_RECOVERY_PROOF' $transit.Code 'pending|Pending' 'Recovery success must retain a pending post-action evidence state'
    Assert-AICFContains $failures 'STAGE3_SAFE_REUSE' $transit.Code 'retarget|Retarget|STRATEGIC_TARGET_CHANGED' 'Target changes must reroute the current usable asset'
    Assert-AICFContains $failures 'STAGE3_TRANSIT_HEALTH_GATE' $transitTick 'InspectRouteAssetFailure[\s\S]*GetSupersededRouteWaypoint[\s\S]*PrepareRoute' 'Transit must classify terminal vehicle health before route reconciliation or creation'
    Assert-AICFContains $failures 'STAGE3_TRANSIT_HEALTH_GATE' $prepareRoute 'InspectRouteAssetFailure[\s\S]*CreateAndStageRoute' 'Route preparation must reject an unusable asset before staging a waypoint'
}

$dismount = Find-AICFClassRecord $records 'AICF_VehicleDismountFlow'
if ($dismount) {
    $terminalClearance = Get-AICFMethodBody $dismount 'ProcessTerminalClearance'
    Assert-AICFContains $failures 'STAGE3_DISMOUNT_CLEARANCE' $dismount.Code 'logical|Logical' 'Dismount must observe logical occupants'
    Assert-AICFContains $failures 'STAGE3_DISMOUNT_CLEARANCE' $dismount.Code 'transition|Transition|IsGettingOut' 'Dismount must observe transitions'
    Assert-AICFContains $failures 'STAGE3_DISMOUNT_CLEARANCE' $dismount.Code 'bounds|Bounds|CoordToLocal' 'Dismount must observe oriented physical bounds'
    Assert-AICFContains $failures 'STAGE3_TERMINAL_EXACT_CLEARANCE' $dismount.Code 'EjectOccupant|Teleport' 'Only the isolated terminal branch must retain exact bounded clearance escalation'
    Assert-AICFContains $failures 'STAGE3_TERMINAL_CLEARANCE_ESCALATION' $terminalClearance 'TERMINAL_DEADLINE_REACHED[\s\S]*AICF_TripOutcome\.ReleaseLease' 'Managed terminal deadline must escalate to CleanupManager through a typed release-scan request instead of waiting forever'
    Assert-AICFNotContains $failures 'STAGE3_TERMINAL_CLEARANCE_ESCALATION' $terminalClearance 'TERMINAL_DEADLINE_REACHED[\s\S]{0,500}AICF_TripOutcome\.TerminalFailClosed' 'Managed terminal deadline must not stop permanently before requesting the independent cleanup scan'
}

$handoff = Find-AICFClassRecord $records 'AICF_VehicleTaskHandoff'
if ($handoff) {
    Assert-AICFContains $failures 'STAGE3_HANDOFF_OWNERSHIP' $handoff.Code 'AddUsableVehicle|AttachVehicle' 'Task handoff must own vehicle utility attachment'
    Assert-AICFContains $failures 'STAGE3_HANDOFF_OWNERSHIP' $handoff.Code 'RemoveUsableVehicle|DetachVehicle' 'Task handoff must own vehicle utility detachment'
    Assert-AICFContains $failures 'STAGE3_HANDOFF_OWNERSHIP' $handoff.Code 'RemoveWaypoint' 'Task handoff must remove owned waypoints from the group queue'
    Assert-AICFContains $failures 'STAGE3_WAYPOINT_DELETE_ORDER' $handoff.Code 'DetachVehicleWaypoint[\s\S]*DeleteDetachedVehicleWaypoint' 'Waypoint handoff must expose detach and delete as separate ownership operations'
}

$tripController = Find-AICFClassRecord $records 'AICF_TransportTripController'
if ($tripController) {
	$canRetireTrip = ConvertTo-AICFCodeText (Get-AICFMethodBody $tripController 'CanRetireTrip')
	$advanceCleanup = ConvertTo-AICFCodeText (Get-AICFMethodBody $tripController 'AdvanceLeaseCleanup')
	Assert-AICFContains $failures 'STAGE3_RETAINED_CLEANUP_OWNERSHIP' $canRetireTrip 'IsCleanupRetainedFailClosed[\s\S]*IsCleanupOwnershipAcceptedTerminal[\s\S]*return true' 'A Trip may retire from retained cleanup only after explicit independent-manager ownership proof'
	Assert-AICFContains $failures 'STAGE3_RETAINED_CLEANUP_OWNERSHIP' $advanceCleanup 'IsRetainedFailClosed[\s\S]*AcknowledgeRetainedLease[\s\S]*RecordCleanupOwnershipAcceptedTerminal[\s\S]*DetachLease' 'Retained cleanup must transfer exact lease ownership before detaching the Trip reference'
	foreach ($methodName in @('ReleaseSupersededTransitWaypoint', 'ReleaseCurrentTransitWaypoint', 'ReleaseSupersededDismountWaypoint', 'ReleaseCurrentDismountWaypoint')) {
        $lifecycleBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $tripController $methodName)
        Assert-AICFContains $failures 'STAGE3_WAYPOINT_DELETE_ORDER' $lifecycleBody 'DetachVehicleWaypoint' "$methodName must first detach the exact waypoint from the authoritative group queue"
        Assert-AICFContains $failures 'STAGE3_WAYPOINT_DELETE_ORDER' $lifecycleBody 'Confirm[A-Za-z]*WaypointRemoved[\s\S]*DeleteDetachedVehicleWaypoint' "$methodName must clear exact state before destructive entity deletion"
    }
}

if ($handoff) {
	$detachWaypoint = ConvertTo-AICFCodeText (Get-AICFMethodBody $handoff 'DetachVehicleWaypoint')
	$exactWaypointOwner = ConvertTo-AICFCodeText (Get-AICFMethodBody $handoff 'IsExactTripOwnedVehicleWaypoint')
	Assert-AICFContains $failures 'STAGE3_TERMINAL_ASSET_LOSS_DETACH' $detachWaypoint 'IsExactTripOwnedVehicleWaypoint[\s\S]*RemoveWaypoint' 'Terminal asset loss must detach only the exact phase-owned waypoint without requiring a live lease'
	Assert-AICFNotContains $failures 'STAGE3_TERMINAL_ASSET_LOSS_DETACH' $detachWaypoint 'IsTripLeaseCurrent|HasPhysicalAsset|GetVehicle' 'Waypoint detachment must not be blocked after the physical asset is destroyed or missing'
	Assert-AICFContains $failures 'STAGE3_TERMINAL_ASSET_LOSS_DETACH' $exactWaypointOwner 'TRANSIT[\s\S]*GetRouteWaypoint[\s\S]*GetSupersededRouteWaypoint[\s\S]*DISMOUNT[\s\S]*GetDismountWaypoint[\s\S]*GetSupersededDismountWaypoint' 'Asset-independent detach must prove exact current/superseded ownership in the active phase state'
}

$vehicleCoordinator = Find-AICFClassRecord $records 'AICF_VehicleCoordinator'
if ($vehicleCoordinator) {
	$terminalObservation = ConvertTo-AICFCodeText (Get-AICFMethodBody $vehicleCoordinator 'ObserveTerminalCommit')
	Assert-AICFNotContains $failures 'STAGE3_ALWAYS_ON' $vehicleCoordinator.Code 'GetVehiclesEnabled\s*\(' 'Vehicle authority must not be feature-gated'
	Assert-AICFContains $failures 'STAGE3_TERMINAL_OUTCOME_COMMIT' $terminalObservation 'observed[\s\S]*observed\.IsTerminal[\s\S]*!trip\.IsTerminal' 'Coordinator must explicitly detect a returned terminal outcome that was not committed by the controller'
	Assert-AICFContains $failures 'STAGE3_TERMINAL_OUTCOME_COMMIT' $vehicleCoordinator.Strings 'UNCOMMITTED_TERMINAL_OUTCOME:' 'An uncommitted terminal outcome must fail observably instead of being silently ignored'
}

$cleanup = Find-AICFClassRecord $records 'AICF_VehicleCleanupManager'
if ($cleanup) {
	$leaseRelease = ConvertTo-AICFCodeText (Get-AICFMethodBody $cleanup 'ProcessLeaseRelease')
	$retainedAck = ConvertTo-AICFCodeText (Get-AICFMethodBody $cleanup 'AcknowledgeRetainedLease')
    Assert-AICFContains $failures 'STAGE3_CLEANUP_PROTECTION' $cleanup.Code 'GetAllPlayers|PlayerManager' 'Cleanup must inspect players'
    Assert-AICFContains $failures 'STAGE3_CLEANUP_PROTECTION' $cleanup.Code '15\.0|PLAYER_SAFE_DISTANCE|PROXIMITY' 'Cleanup must retain the fifteen-metre player proximity gate'
    Assert-AICFContains $failures 'STAGE3_CLEANUP_PROTECTION' $cleanup.Code 'IsGettingIn|IsGettingOut|transition|Transition' 'Cleanup must protect active vehicle transitions'
    Assert-AICFContains $failures 'STAGE3_WORLD_POOL' $cleanup.Code 'WorldPool|WORLD_POOL' 'Cleanup must retain world-pool processing outside active AI leases'
	Assert-AICFContains $failures 'STAGE3_STOP_CLEANUP' $cleanup.Code 'CallLater|STOP_CLEANUP|StopCleanup' 'Stop cleanup must continue through bounded deferred polls'
	Assert-AICFContains $failures 'STAGE3_BOUNDED_PROTECTED_CLEARANCE' $leaseRelease 'GetAbsoluteDeadlineMs[\s\S]*RetainFailClosed' 'Protected clearance must terminate in a retained fail-closed outcome at its absolute deadline'
	Assert-AICFContains $failures 'STAGE3_BOUNDED_PROTECTED_CLEARANCE' $cleanup.Strings 'PROTECTED_CLEARANCE_DEADLINE_EXCEEDED:' 'Protected-clearance deadline must preserve an explicit blocker-bearing failure reason'
	foreach ($proof in @('job\.m_bReleaseComplete', 'FAILED_CLOSED', 'FleetContainsLease', 'FindLeaseForSlot', 'MatchesLease')) {
		Assert-AICFContains $failures 'STAGE3_RETAINED_CLEANUP_OWNERSHIP' $retainedAck $proof 'Retained ownership acknowledgement must prove an unreleased, exact, cap-holding Fleet lease'
	}
	Assert-AICFContains $failures 'STAGE3_RETAINED_CLEANUP_ACCEPTANCE' $cleanup.Code 'ObserveCleanupFailureFromFence' 'Pre-release retained failure must reach acceptance through immutable cleanup-fence identity'
}

$fleet = Find-AICFClassRecord $records 'AICF_FactionFleet'
$coordinator = Find-AICFClassRecord $records 'AICF_VehicleCoordinator'
$domainDiagnostics = Find-AICFClassRecord $records 'AICF_VehicleDomainDiagnostics'
if ($fleet -and $coordinator -and $domainDiagnostics) {
    $reservedCount = ConvertTo-AICFCodeText (Get-AICFMethodBody $fleet 'GetReservedCount')
    $coordinatorHeartbeat = Get-AICFMethodBody $coordinator 'Heartbeat'
    $fleetHeartbeat = Get-AICFMethodBody $domainDiagnostics 'FleetHeartbeat'
    Assert-AICFContains $failures 'STAGE3_FLEET_HEARTBEAT_TRUTH' $reservedCount 'AICF_EVehicleLeaseState\.RESERVED' 'Reserved count must mean the exact RESERVED lease state'
    Assert-AICFNotContains $failures 'STAGE3_FLEET_HEARTBEAT_TRUTH' $reservedCount 'IsCapActive|ACTIVE|RELEASE_PENDING|FAILED_CLOSED' 'Reserved count must not hide other cap-holding lease states'
    foreach ($field in @('reserved=', 'release_pending=', 'failed_closed=', 'cap_held=', 'retained_physical=')) {
        Assert-AICFContains $failures 'STAGE3_FLEET_HEARTBEAT_TRUTH' $coordinatorHeartbeat ([regex]::Escape($field)) "Stage 3 heartbeat omits truthful asset field $field"
        Assert-AICFContains $failures 'STAGE3_FLEET_HEARTBEAT_TRUTH' $fleetHeartbeat ([regex]::Escape($field)) "Aggregate fleet heartbeat omits truthful asset field $field"
    }
} else {
    Add-AICFAuditFailure $failures 'STAGE3_COMPONENT_MISSING' 'Missing fleet/coordinator/domain diagnostics for truthful heartbeat audit'
}

$slot = Find-AICFClassRecord $records 'AICF_GroupSlot'
if ($slot) {
    Assert-AICFNotContains $failures 'STAGE3_NO_SLOT_VEHICLE_STATE' $slot.Code 'AICF_VehicleRuntime|SetVehicleRuntime|GetVehicleRuntime' 'GroupSlot must not duplicate mutable vehicle runtime ownership'
}

$marker = Find-AICFClassRecord $records 'AICF_GroupMapMarkerSystem'
if ($marker) {
    Assert-AICFContains $failures 'STAGE3_MARKER_STATE' $marker.Source 'VEH ' 'Gameplay marker must expose VEH <state>'
}

$allStrings = ($records | ForEach-Object { $_.Strings }) -join "`n"
$requiredEvents = @(
    'CONFIG', 'VEHICLE_STATE_CHANGED', 'VEHICLE_REQUESTED', 'VEHICLE_SPAWN_SITE_SELECTED',
    'VEHICLE_SPAWN_CANDIDATES_EVALUATED', 'VEHICLE_SPAWN_SITE_REJECTED', 'VEHICLE_SPAWN_DEFERRED',
    'VEHICLE_SPAWNED', 'VEHICLE_ASSIGNED', 'DRIVER_ASSIGNED', 'GUNNER_ASSIGNED', 'PASSENGERS_ASSIGNED',
    'BOARDING_STARTED', 'BOARDING_PHASE_STARTED', 'BOARDING_REJECTED', 'BOARDING_ROLE_RESET',
    'BOARDING_ROLE_RETRY', 'BOARDING_ROLE_VIOLATION', 'BOARDING_ACTION_OWNERSHIP', 'BOARDING_CREW_ROLE_LOST',
    'BOARDING_APPROACH_REISSUED', 'BOARDING_APPROACH_COMPLETE', 'PASSENGER_BOARDING_REISSUED',
    'PASSENGER_EXACT_CARGO_ALLOCATION_WAIT', 'BOARDING_BLOCKER',
    'PASSENGER_BOARDING_ACTION_FAILED', 'BOARDING_TRANSITION_GRACE', 'BOARDING_PROGRESS', 'BOARDING_COMPLETE',
    'BOARDING_TIMEOUT', 'VEHICLE_ROUTE_ASSIGNED', 'VEHICLE_PROGRESS', 'VEHICLE_MOTION', 'DISEMBARK_STARTED',
    'DISEMBARK_CLEARANCE_GUIDANCE', 'DISEMBARK_CLEARANCE_RECOVERY', 'DISEMBARK_ANIMATED_EXACT_RETRY', 'DISEMBARK_TIMEOUT',
    'DISEMBARK_COMPLETE', 'VEHICLE_STUCK_DETECTED', 'VEHICLE_RECOVERY_STARTED', 'VEHICLE_RECOVERY_SUCCEEDED',
    'VEHICLE_CREW_RECOVERY_SUCCEEDED', 'VEHICLE_RECOVERY_FAILED', 'VEHICLE_UNSTUCK_STARTED',
    'VEHICLE_UNSTUCK_ATTEMPT', 'VEHICLE_UNSTUCK_SUCCEEDED', 'VEHICLE_UNSTUCK_FAILED', 'DRIVER_LOST',
    'DRIVER_REASSIGNED', 'GUNNER_LOST', 'GUNNER_REASSIGNED', 'FALLBACK_FORCE_DISEMBARK',
    'FALLBACK_DISEMBARK_FAILED', 'VEHICLE_ABANDONED', 'INFANTRY_FALLBACK', 'VEHICLE_DESTROYED',
    'VEHICLE_CAP_BLOCKED', 'VEHICLE_REQUEST_WAITING', 'VEHICLE_REQUEST_RESUMED',
    'VEHICLE_SPAWN_WAIT_HEARTBEAT', 'VEHICLE_WORLD_POOL_RELEASED', 'VEHICLE_WORLD_POOL_SOFT_OVERFLOW',
    'VEHICLE_WORLD_POOL_STALE_REMOVED', 'VEHICLE_CLEANUP_DEFERRED', 'VEHICLE_STOP_CLEANUP_STARTED',
    'VEHICLE_STOP_CLEANUP_CONFIRMED', 'VEHICLE_STOP_CLEANUP_RETAINED', 'VEHICLE_DELETE_REQUESTED',
    'VEHICLE_DELETE_RETRIED', 'VEHICLE_DELETE_NOT_CONFIRMED', 'VEHICLE_CLEANUP_CONFIRMED', 'VEHICLE_CLEANUP',
    'ACCEPTANCE_FAILURE_LATCHED', 'ORDER_RECOVERY_ISSUED', 'ORDER_RECOVERY_UNSTABLE',
    'GROUP_STUCK_FIELD_HOLD', 'GROUP_STUCK_FIELD_RESUMED', 'ORDER_RECOVERED', 'HEARTBEAT',
    'RESULT_CANDIDATE', 'RESULT'
)
foreach ($eventName in $requiredEvents) {
    if ($allStrings -notmatch ('(?m)^' + [regex]::Escape($eventName) + '\r?$')) {
        Add-AICFAuditFailure $failures 'STAGE3_EVENT_CONTRACT' "Missing live string literal for Stage 3 event $eventName"
    }
}

foreach ($identityField in @('vehicle_lifecycle_id', 'operation_id', 'causation_id', 'trip_generation', 'lease_generation')) {
    if ($allStrings -notmatch [regex]::Escape($identityField)) {
        Add-AICFAuditFailure $failures 'STAGE3_DIAGNOSTIC_IDENTITY' "Diagnostics omit required identity field $identityField"
    }
}
foreach ($restoreField in @('bound_to_group', 'is_current', 'postcondition_meaningful_task')) {
    if ($allStrings -notmatch [regex]::Escape($restoreField)) {
        Add-AICFAuditFailure $failures 'STAGE3_ORDER_RESTORE_PROOF' "Order restore telemetry omits $restoreField"
    }
}

Assert-AICFContains $failures 'STAGE3_ACCEPTANCE_CANDIDATE' $allStrings 'status=READY' 'Automated evidence may emit only a READY result candidate'
Assert-AICFContains $failures 'STAGE3_ACCEPTANCE_CANDIDATE' $allStrings 'final=0' 'READY result candidate must remain non-final'
Assert-AICFNotContains $failures 'STAGE3_NO_AUTOMATIC_PASS' $allStrings 'status=PASS' 'Runtime/static code must not manufacture Stage 3 PASS'

if ($failures.Count -gt 0) {
    Write-Host "Stage 3 static audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
}

Write-Host 'Stage 3 static audit: PASS' -ForegroundColor Green
Write-Host 'Negative fixture self-check: PASS (COORDINATOR_SIDE_EFFECT, FLOW_CROSS_CALL, WAYPOINT_SIDE_EFFECT_OWNER, TRANSITION_OUTSIDE_CONTROLLER, TRANSITION_EFFECT_ORDER, WAITING_WITH_LEASE, HANDOFF_CLEARANCE_GATE, CLEANUP_CLEARANCE_OWNER, CLEANUP_IDENTITY_SAFETY, VEHICLE_LIVENESS_OWNERSHIP)'
Write-Host 'Checked: domain ownership, typed trip mutations and transition effect order, exclusive vehicle-waypoint queue ownership, lease/cap/generation ownership, cap-free waiting, phase-local reset, server/surface-safe acquisition, exact Cargo boarding, independent recovery evidence, non-destructive normal dismount, independent order handoff, cleanup-only physical-clearance proof, identity-safe cleanup, configuration, diagnostics identities/events, and non-final acceptance.'
