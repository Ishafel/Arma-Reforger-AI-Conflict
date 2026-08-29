[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$issues = [System.Collections.Generic.List[string]]::new()

function Read-RepoFile([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $repoRoot $relativePath) -Raw
}

function Assert-Contains(
    [string]$text,
    [string]$needle,
    [string]$contract
) {
    if (-not $text.Contains($needle)) {
        $script:issues.Add("[$contract] missing: $needle")
    }
}

function Assert-NotContains(
    [string]$text,
    [string]$needle,
    [string]$contract
) {
    if ($text.Contains($needle)) {
        $script:issues.Add("[$contract] forbidden: $needle")
    }
}

function Assert-Ordered(
    [string]$text,
    [string]$first,
    [string]$second,
    [string]$contract
) {
    $firstIndex = $text.IndexOf($first, [StringComparison]::Ordinal)
    $secondIndex = $text.IndexOf($second, [StringComparison]::Ordinal)
    if ($firstIndex -lt 0 -or $secondIndex -lt 0 -or $firstIndex -ge $secondIndex) {
        $script:issues.Add("[$contract] expected '$first' before '$second'")
    }
}

function Assert-OrderedAfter(
    [string]$text,
    [string]$anchor,
    [string]$first,
    [string]$second,
    [string]$contract
) {
    $anchorIndex = $text.IndexOf($anchor, [StringComparison]::Ordinal)
    if ($anchorIndex -lt 0) {
        $script:issues.Add("[$contract] missing anchor: $anchor")
        return
    }

    $firstIndex = $text.IndexOf($first, $anchorIndex, [StringComparison]::Ordinal)
    $secondIndex = $text.IndexOf($second, $anchorIndex, [StringComparison]::Ordinal)
    if ($firstIndex -lt 0 -or $secondIndex -lt 0 -or $firstIndex -ge $secondIndex) {
        $script:issues.Add("[$contract] expected '$first' before '$second' after '$anchor'")
    }
}

$stage2 = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage2Config.c'
$stage3 = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage3Config.c'
$acquisition = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleAcquisitionFlow.c'
$spawner = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleSpawner.c'
$boarding = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBoardingFlow.c'
$tokens = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehicleBoardingTokens.c'
$watchdog = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleWatchdog.c'
$transit = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'
$phaseStates = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehiclePhaseStates.c'
$lease = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehicleLease.c'
$fleet = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_FactionFleet.c'
$dismount = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleDismountFlow.c'
$cleanup = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCleanupManager.c'
$controller = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_TransportTripController.c'
$handoff = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTaskHandoff.c'
$waypointFactory = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleWaypointFactory.c'
$coordinator = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c'
$planner = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_OrderPlanner.c'
$match = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c'
$groupSlot = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c'

Assert-Contains $stage3 'DEFAULT_BOARDING_TIMEOUT_MS = 40000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_STUCK_TIMEOUT_MS = 45000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_PASSENGER_STALL_MS = 8000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_PASSENGER_MAX_RETRIES = 1' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_CLEANUP_DELAY_MS = 10000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_NO_RANGE_PROGRESS_TIMEOUT_MS = 150000' 'NO_RANGE_BOUNDED_WAIT'
Assert-Contains $stage3 'DEFAULT_MAXIMUM_REUSE_DISTANCE_METERS = 1000.0' 'BOARDING_RANGE_DEFAULT'
Assert-Contains $stage3 'aicfVehicleNoRangeProgressTimeoutMs' 'NO_RANGE_BOUNDED_WAIT'
Assert-Contains $stage3 'ClampInt(value.ToInt(), 60000, 300000)' 'NO_RANGE_BOUNDED_WAIT'
Assert-Contains $stage3 'int GetNoRangeProgressTimeoutMs()' 'NO_RANGE_BOUNDED_WAIT'
Assert-Contains $stage2 'DEFAULT_STUCK_TIMEOUT_MS = 60000' 'TIMING_DEFAULTS'
Assert-Contains $stage2 'DEFAULT_MAX_STUCK_RECOVERIES = 2' 'TIMING_DEFAULTS'

Assert-Contains $boarding 'm_Config.GetPassengerStallMs()' 'EXACT_CARGO_RECOVERY'
Assert-Contains $boarding 'm_Config.GetPassengerMaxRetries()' 'EXACT_CARGO_RECOVERY'
Assert-Contains $boarding 'BOARDING_PASSENGER_ALLOCATION_NEXT_TICK' 'POST_CREW_CARGO_ALLOCATION_GATE'
Assert-Contains $boarding 'state.IsPassengerPlanIssued()' 'POST_CREW_CARGO_ALLOCATION_GATE'
Assert-Contains $boarding 'state.GetPassengerAllocationAgeMs(nowMs)' 'POST_CREW_CARGO_ALLOCATION_GATE'
Assert-Contains $boarding 'PASSENGER_EXACT_CARGO_ALLOCATION_TIMEOUT' 'POST_CREW_CARGO_ALLOCATION_GATE'
Assert-Contains $boarding 'compartment.IsGetInLockedFor(entity)' 'POST_CREW_CARGO_ALLOCATION_GATE'
Assert-Contains $boarding 'PASSENGER_EXACT_CARGO_REVALIDATION_PENDING' 'POST_CREW_CARGO_ALLOCATION_GATE'
foreach ($cargoField in @(
    'manager=%1', 'slot=%2', 'accessible=%3', 'occupied=%4', 'reserved=%5',
    'reserved_by_candidate=%6', 'get_in_locked=%7', 'get_in_locked_for=%8', 'mapped=%9'
)) {
    Assert-Contains $boarding $cargoField 'POST_CREW_CARGO_ALLOCATION_DIAGNOSTICS'
}
Assert-Contains $boarding 'required=%2 mapped=%3 cargo_slots=[' 'POST_CREW_CARGO_ALLOCATION_DIAGNOSTICS'
Assert-Contains $phaseStates 'MarkPassengerAllocationFailureReported()' 'POST_CREW_CARGO_ALLOCATION_LOG_BOUND'
$beginPassengerPhase = [regex]::Match(
    $boarding,
    '(?s)protected AICF_TripOutcome BeginPassengerPhase\(.*?(?=\r?\n\tprotected AICF_TripOutcome ProcessPassengers\()'
)
if (-not $beginPassengerPhase.Success) {
    $issues.Add('[POST_CREW_CARGO_ALLOCATION_GATE] BeginPassengerPhase body not found')
} else {
    Assert-NotContains $beginPassengerPhase.Value 'IssuePassengerPlan(' 'POST_CREW_CARGO_ALLOCATION_GATE'
}
Assert-Contains $boarding 'm_Config.GetHiddenRecoveryEnabled()' 'EXACT_CARGO_RECOVERY'
Assert-Contains $boarding 'PASSENGER_HIDDEN_EXACT_CARGO_SCHEDULED' 'EXACT_CARGO_RECOVERY'
Assert-Contains $boarding 'PASSENGER_HIDDEN_EXACT_CARGO_FORCED' 'EXACT_CARGO_RECOVERY'
Assert-Ordered $boarding 'token.ApplyHiddenExactSeatRecovery()' 'token.ScheduleHiddenExactSeatRecovery()' 'NEXT_TICK_EXACT_CARGO'
Assert-Contains $tokens 'm_bHiddenExactSeatRecoveryAttempted = true' 'ONE_SHOT_EXACT_CARGO'
Assert-Contains $tokens 'return access.GetInVehicle(' 'ONE_SHOT_EXACT_CARGO'
Assert-Contains $tokens 'true,' 'ONE_SHOT_EXACT_CARGO'
Assert-Contains $boarding 'EXACT_CARGO_READY_RETRY_STALL_MS = 4000' 'NORMAL_FIRST_EXACT_CARGO'
Assert-Contains $boarding 'PASSENGER_ANIMATED_EXACT_CARGO_REQUESTED' 'NORMAL_FIRST_EXACT_CARGO'
Assert-Contains $boarding 'FindAnimatedExactSeatRecoveryOwner(tokens, token)' 'NORMAL_FIRST_EXACT_CARGO_SERIALIZATION'
Assert-Contains $boarding '(transitioning && !token.WasAnimatedExactSeatRecoveryAttempted())' 'EXACT_CARGO_TRANSITION_RACE'
Assert-Contains $tokens 'RequestAnimatedExactSeatRecovery(' 'NORMAL_FIRST_EXACT_CARGO'
Assert-Contains $tokens 'm_Compartment.GetAvailableDoorIndices(doorIndices)' 'NORMAL_FIRST_EXACT_CARGO_DOOR'
Assert-Contains $tokens 'm_iRetryCount++' 'NORMAL_FIRST_EXACT_CARGO_BUDGET'
Assert-Contains $tokens 'false,' 'NORMAL_FIRST_EXACT_CARGO_NO_TELEPORT'
Assert-Ordered $boarding 'token.RequestAnimatedExactSeatRecovery(' 'token.ScheduleHiddenExactSeatRecovery()' 'NORMAL_BEFORE_HIDDEN_EXACT_CARGO'

Assert-Contains $watchdog 'GetPlayerControlledEntity(playerId)' 'HIDDEN_PLAYER_FENCE'
Assert-Contains $watchdog 'GetPlayerMainEntity(playerId)' 'HIDDEN_PLAYER_FENCE'
Assert-Contains $watchdog 'PLAYER_POSITION_UNKNOWN' 'HIDDEN_PLAYER_FENCE'
Assert-Contains $watchdog 'HIDDEN_RECOVERY_LOS_RADIUS_METERS = 1200.0' 'HIDDEN_PLAYER_FENCE'
Assert-Contains $watchdog 'TraceFlags.ANY_CONTACT' 'HIDDEN_PLAYER_FENCE'
Assert-Contains $watchdog 'IsAuthoritativeAIEntity(character)' 'MUTATION_AUTHORITY'
Assert-Contains $watchdog 'IsAuthoritativeReplicatedEntity(vehicle)' 'MUTATION_AUTHORITY'
Assert-Contains $watchdog 'AreAllAliveMembersRecoveryLinkedToVehicle(' 'BOUNDED_TRANSITION_RECOVERY'
Assert-Contains $watchdog 'allowManagedTransitionRecovery' 'BOUNDED_TRANSITION_RECOVERY'

Assert-Contains $spawner 'class AICF_VehicleSpawnSiteReservation' 'SPAWN_SITE_RESERVATION'
Assert-Contains $spawner 'class AICF_VehicleSpawnPlan' 'SPAWN_PLAN_PERSISTENCE'
Assert-Contains $spawner 'SITE_RESERVATION_SEPARATION_METERS = 20.0' 'SPAWN_SITE_RESERVATION'
Assert-Contains $spawner 'bool RevalidateSpawnCommit(' 'SPAWN_COMMIT_REVALIDATION'
Assert-Contains $spawner 'STAGING_NO_LONGER_CONFIRMED' 'SPAWN_COMMIT_REVALIDATION'
Assert-Contains $spawner 'SPAWN_PAD_OCCUPIED' 'SPAWN_COMMIT_REVALIDATION'
Assert-Contains $acquisition 'm_Config.GetMaximumSpawnDistanceMeters(),' 'SPAWN_PLAN_SITE_SELECTION'
Assert-Contains $acquisition 'TryReserveSelectedSite(' 'SPAWN_PLAN_SITE_RESERVATION'
Assert-Contains $acquisition 'BEGIN_SITE_APPROACH' 'SPAWN_PLAN_APPROACH'
Assert-Contains $acquisition 'MeasureStagingReadiness(' 'SPAWN_PLAN_ALL_ALIVE_STAGING'
Assert-Contains $acquisition 'm_Config.GetSpawnStagingHoldMs()' 'SPAWN_PLAN_STABLE_HOLD'
Assert-Contains $acquisition 'VEHICLE_SPAWN_COMMIT_REQUESTED' 'SPAWN_COMMIT_DIAGNOSTICS'
Assert-Contains $acquisition 'VEHICLE_SPAWN_COMMIT_REJECTED' 'SPAWN_COMMIT_DIAGNOSTICS'
Assert-Contains $controller 'CreateSpawnStagingWaypoint(' 'SPAWN_APPROACH_CONTROLLER_OWNER'
Assert-Contains $controller 'ReleaseSpawnPlan(' 'SPAWN_PLAN_CANCELLATION'
Assert-Contains $waypointFactory 'SetCompletionType(EAIWaypointCompletionType.All)' 'SPAWN_STAGING_ALL_MEMBER_WAYPOINT'
Assert-Contains $acquisition 'MAX_APPROACH_WAYPOINT_REISSUES = 1' 'SPAWN_APPROACH_BOUNDED_REISSUE'
Assert-Contains $match 'IsSpawnStagingRecoveryCompatible' 'SPAWN_APPROACH_MOB_RECOVERY_COMPATIBILITY'
Assert-NotContains $acquisition '.AddWaypointAt(' 'SPAWN_APPROACH_CONTROLLER_OWNER'
Assert-NotContains $acquisition '.RemoveWaypoint(' 'SPAWN_APPROACH_CONTROLLER_OWNER'
Assert-NotContains $acquisition 'Teleport(' 'SPAWN_APPROACH_NO_HIDDEN_MUTATION'

Assert-Contains $transit 'm_Config.GetHiddenRecoveryEnabled()' 'VEHICLE_UNSTUCK_HIDDEN'
Assert-Contains $transit 'originalPosition,' 'VEHICLE_UNSTUCK_DESTINATION_RECHECK'
Assert-Contains $transit 'candidate,' 'VEHICLE_UNSTUCK_DESTINATION_RECHECK'
Assert-Ordered $transit 'm_Watchdog.CanApplyHiddenRecovery(' 'vehicle.SetWorldTransform(relocatedTransform)' 'VEHICLE_UNSTUCK_DESTINATION_RECHECK'
Assert-Contains $transit 'UNSTUCK_OUTER_OFFSET_METERS = 13.0' 'VEHICLE_UNSTUCK_BOUNDED_MULTI_RING'
Assert-Contains $transit '(-targetDirection + rightDirection).Normalized()' 'VEHICLE_UNSTUCK_ALL_DIRECTIONS'
Assert-Contains $transit 'exactManagedOccupant' 'VEHICLE_UNSTUCK_EXACT_OCCUPANT_EXCLUSION'
Assert-Contains $transit 'CompartmentAccessComponent.GetVehicleIn(character) == m_UnstuckHazardVehicle' 'VEHICLE_UNSTUCK_EXACT_OCCUPANT_EXCLUSION'
Assert-Contains $transit 'ACTIVE_PRESSURE_TRIGGER_' 'VEHICLE_UNSTUCK_HAZARD_FENCE'
Assert-Contains $transit 'LIVE_CHARACTER_' 'VEHICLE_UNSTUCK_HAZARD_FENCE'
Assert-Contains $transit 'managed_occupant_observations_ignored=' 'VEHICLE_UNSTUCK_REJECTION_DIAGNOSTICS'
Assert-Contains $transit 'rejection_samples=[' 'VEHICLE_UNSTUCK_REJECTION_DIAGNOSTICS'
Assert-Contains $transit 'VEHICLE_FAILURE_SNAPSHOT' 'VEHICLE_FIRE_CAUSE_CLASSIFICATION'
Assert-Contains $transit 'ENGINE_FIRE_SIGNAL_WITHOUT_INSTIGATOR' 'VEHICLE_FIRE_CAUSE_CLASSIFICATION'
Assert-Contains $transit 'VEHICLE_RECOVERY_DEFERRED' 'MOBILITY_PRECHECK_NO_BUDGET'
Assert-Contains $transit 'attempt_consumed=0 settlement_deferral=' 'MOBILITY_PRECHECK_NO_BUDGET'
Assert-Contains $transit 'next_action=WAIT_MANAGED_MEMBERS_SETTLED' 'MOBILITY_PRECHECK_NO_BUDGET'
Assert-OrderedAfter $transit 'protected AICF_TripOutcome ObserveProgressAndRecover(' 'relocationPreflightReason == "MANAGED_MEMBERS_NOT_SETTLED"' 'state.BeginMobilityRecovery()' 'MOBILITY_PRECHECK_NO_BUDGET'
Assert-Contains $phaseStates 'MarkMobilityRecoveryDeferredDue(' 'MOBILITY_PRECHECK_BACKOFF'
Assert-Contains $phaseStates 'RecordMobilityRecoverySettlementDeferral(' 'MOBILITY_SETTLEMENT_BUDGET'
Assert-Contains $transit 'MANAGED_SETTLEMENT_RECOVERY_GRACE_MS = 5000' 'BOUNDED_TRANSITION_RECOVERY'
Assert-Contains $transit 'MANAGED_SETTLEMENT_RECOVERY_MAX_DEFERRALS = 15' 'MOBILITY_SETTLEMENT_BUDGET'
Assert-Contains $transit 'MOBILITY_RECOVERY_MANAGED_MEMBERS_NOT_SETTLED_EXHAUSTED' 'MOBILITY_SETTLEMENT_FALLBACK'
Assert-OrderedAfter $transit 'relocationPreflightReason == "MANAGED_MEMBERS_NOT_SETTLED"' 'state.MarkMobilityRecoveryDeferredDue(nowMs, MOTION_REPORT_INTERVAL_MS)' 'ReportStuckDetected(' 'MOBILITY_DEFERRED_DIAGNOSTIC_BACKOFF'
Assert-Contains $phaseStates 'RollbackUncommittedMobilityRecovery()' 'MOBILITY_PRECHECK_RACE_ROLLBACK'
Assert-Contains $transit 'unstuckMode == "REJECTED_MANAGED_MEMBERS_NOT_SETTLED"' 'MOBILITY_PRECHECK_RACE_ROLLBACK'
Assert-OrderedAfter $transit 'protected AICF_TripOutcome BeginMobilityRecovery(' 'state.RollbackUncommittedMobilityRecovery()' 'state.SuspendRouteWaypoint()' 'MOBILITY_PRECHECK_RACE_ROLLBACK'
Assert-Contains $phaseStates 'CanReportRecoveryMobilityRestored()' 'TWO_PHASE_VEHICLE_RECOVERY'
Assert-Contains $transit 'VEHICLE_MOBILITY_RESTORED' 'TWO_PHASE_VEHICLE_RECOVERY'
Assert-Contains $transit 'VEHICLE_ROUTE_PROGRESS_CONFIRMED' 'TWO_PHASE_VEHICLE_RECOVERY'
Assert-Contains $transit 'route_progress_confirmed=1 terminal_outcome=SUCCEEDED' 'TWO_PHASE_VEHICLE_RECOVERY'
Assert-OrderedAfter $transit 'protected AICF_TripOutcome ObserveProgressAndRecover(' 'if (state.CanReportRecoveryMobilityRestored())' 'if (state.CanConfirmRecoveryEvidence())' 'TWO_PHASE_VEHICLE_RECOVERY'
Assert-NotContains $transit 'stuckReason == "NO_OBJECTIVE_PROGRESS"' 'ROUTE_PROGRESS_REQUIRED_FOR_ALL_RECOVERY'

Assert-Contains $controller 'ORDER_RESTORE_DEADLINE_MS = 60000' 'HANDOFF_RESTORE_WINDOW'
Assert-Contains $controller 'ORDER_RESTORE_MAX_ATTEMPTS = 4' 'HANDOFF_RESTORE_WINDOW'
Assert-Contains $controller 'failure_scope=ORDER_RESTORE' 'HANDOFF_FAILURE_CLASSIFICATION'
Assert-Contains $controller 'cleanup_vs_proof=INDEPENDENT' 'HANDOFF_FAILURE_CLASSIFICATION'

Assert-Contains $transit 'CREW_SETTLED_LOSS_MIN_POLLS = 3' 'MANDATORY_CREW_SETTLED_LOSS_DEBOUNCE'
Assert-Contains $transit 'CREW_SETTLED_LOSS_GRACE_MS = 3000' 'MANDATORY_CREW_SETTLED_LOSS_DEBOUNCE'
Assert-Contains $phaseStates 'ObserveCrewRoleSettledLoss(' 'MANDATORY_CREW_SETTLED_LOSS_DEBOUNCE'
Assert-Contains $phaseStates 'ResolveCrewRoleSettledLoss(' 'MANDATORY_CREW_SETTLED_LOSS_DEBOUNCE'
Assert-Contains $transit 'polls >= CREW_SETTLED_LOSS_MIN_POLLS' 'MANDATORY_CREW_SETTLED_LOSS_DEBOUNCE'
Assert-Contains $transit 'ageMs >= CREW_SETTLED_LOSS_GRACE_MS' 'MANDATORY_CREW_SETTLED_LOSS_DEBOUNCE'
Assert-Contains $transit 'MANDATORY_CREW_SETTLED_LOSS_SNAPSHOT' 'MANDATORY_CREW_SETTLED_LOSS_TELEMETRY'
Assert-Contains $transit 'MANDATORY_CREW_SETTLED_LOSS_RECOVERED' 'MANDATORY_CREW_SETTLED_LOSS_TELEMETRY'
Assert-Contains $transit 'MANDATORY_CREW_SETTLED_LOSS_IDENTITY_CHANGED' 'MANDATORY_CREW_SETTLED_LOSS_IDENTITY'
Assert-Contains $transit 'MANDATORY_CREW_SETTLED_LOSS_TERMINAL' 'MANDATORY_CREW_SETTLED_LOSS_TELEMETRY'
Assert-Contains $transit 'm_bCharacterInVehicle && m_bExactRoleSlot' 'MANDATORY_CREW_SETTLED_EXACT_ROLE_GATE'
Assert-Contains $transit 'if (observedOccupant != snapshot.GetOccupant())' 'MANDATORY_CREW_SETTLED_LOSS_IDENTITY'
Assert-Contains $transit 'snapshot_immutable=1 predicates=[' 'MANDATORY_CREW_SETTLED_LOSS_TELEMETRY'
Assert-Contains $transit 'alive_managed=%1|linked=%2|in_compartment=%3|get_in=%4|get_out=%5|character_vehicle=%6' 'MANDATORY_CREW_SETTLED_LOSS_PREDICATES'
Assert-Contains $transit 'exact_role_slot=%1|assigned_mgr=%2|assigned_slot=%3|actual_mgr=%4|actual_slot=%5' 'MANDATORY_CREW_SETTLED_LOSS_EXACT_SEAT'
Assert-Contains $transit 'route_generation=%1 route_mode=%2 route_bound=%3 route_waypoint=%4' 'MANDATORY_CREW_SETTLED_LOSS_ROUTE_CONTEXT'
Assert-Contains $transit 'return StartCrewRecovery(trip, EAICompartmentType.Pilot' 'MISSING_DRIVER_USES_CREW_RECOVERY'
Assert-Contains $transit 'loss_kind=%1 role=%2 role_slot_occupant=%3 last_known_occupant=%4 last_known_rpl=%5' 'DRIVER_LOSS_CAUSAL_SNAPSHOT'
Assert-Contains $transit 'last_linked=%1 last_in_compartment=%2 last_get_in=%3 last_get_out=%4 last_character_vehicle=%5' 'DRIVER_LOSS_CAUSAL_SNAPSHOT'
Assert-Contains $transit 'current_action=%1 current_action_state=%2 loss_polls=%3 loss_age_ms=%4 prior_predicates=[%5]' 'DRIVER_LOSS_ACTION_SNAPSHOT'
Assert-Contains $transit 'lossKind = "EXIT_ACTION"' 'DRIVER_LOSS_CLASSIFICATION'
Assert-Contains $transit 'lossKind = "SEAT_CHANGED"' 'DRIVER_LOSS_CLASSIFICATION'
Assert-Contains $transit 'utility.m_OwnerEntity == evidenceEntity' 'DRIVER_LOSS_ACTION_OWNERSHIP'

Assert-Contains $phaseStates 'm_bTerminalClearanceStarted' 'TERMINAL_CLEARANCE_MODE'
Assert-Contains $phaseStates 'void BeginTerminal(' 'TERMINAL_CLEARANCE_MODE'
Assert-Contains $controller 'IsTerminalClearanceStarted()' 'TERMINAL_CLEARANCE_MODE'
Assert-Contains $dismount 'CONTINUOUS_CLEAR_MS = 5000' 'TERMINAL_CLEARANCE_STABILITY'
Assert-Contains $dismount 'ResetGroupVehicleActions(group)' 'TERMINAL_REBOARDING_BREAK'
Assert-Contains $dismount 'CanApplyTerminalHiddenRecovery(' 'TERMINAL_HIDDEN_FENCE'
Assert-Contains $dismount 'TERMINAL_CLEARANCE_RECOVERY_DEFERRED' 'TERMINAL_HIDDEN_FENCE'
Assert-Contains $dismount 'EjectOccupant(true, false, ejectImmediate, true)' 'TERMINAL_EXACT_UNLINK'
Assert-Contains $dismount 'TERMINAL_FORCE_BUDGET_EXHAUSTED' 'TERMINAL_EARLY_CLEANUP_HANDOFF'
Assert-Contains $dismount 'QUEUE_PROTECTED_CLEANUP_IMMEDIATELY' 'TERMINAL_EARLY_CLEANUP_HANDOFF'
Assert-Contains $dismount 'POST_EGRESS_PADDING_METERS = 8.0' 'TARGET_SIDE_POST_DISEMBARK'
Assert-Contains $dismount 'sample.m_iPostEgressBlocked == 0' 'TARGET_SIDE_POST_DISEMBARK'
Assert-Contains $dismount 'sample.m_iPostEgressBlocked);' 'TARGET_SIDE_POST_DISEMBARK_STABILITY'
Assert-Contains $phaseStates 'additionalStabilityBlockers > 0' 'TARGET_SIDE_POST_DISEMBARK_STABILITY'
Assert-Contains $dismount 'ResolveTargetSideEgress(' 'TARGET_SIDE_POST_DISEMBARK'
Assert-Contains $dismount 'POST_DISEMBARK_TARGET_SIDE_EGRESS' 'TARGET_SIDE_POST_DISEMBARK_DIAGNOSTICS'
Assert-Contains $dismount 'NON_ALIVE_FORCE_DISEMBARK_MEMBER' 'NON_ALIVE_EXACT_DETACH'
Assert-Contains $dismount 'compartment.EjectOccupant(true, false, ejectImmediate, true)' 'NON_ALIVE_EXACT_DETACH'
Assert-Contains $phaseStates 'TryBeginExactRelocationProbe(' 'EXACT_RELOCATION_PROBE_COMMIT'
Assert-Contains $dismount 'EXACT_RELOCATION_PROBE_BACKOFF_MS = 2000' 'EXACT_RELOCATION_PROBE_COMMIT'
Assert-Contains $dismount 'if (candidates.IsEmpty())' 'EXACT_RELOCATION_EMPTY_PROBE'
Assert-Contains $dismount 'DISEMBARK_EXACT_RELOCATION_PROBE_EMPTY' 'EXACT_RELOCATION_EMPTY_PROBE'
Assert-Contains $dismount 'budget_consumed=0' 'EXACT_RELOCATION_EMPTY_PROBE'
Assert-OrderedAfter $dismount 'protected void TryRelocateExactManagedMembers(' 'if (candidates.IsEmpty())' 'if (!state.RecordForceClearanceAttempt())' 'EXACT_RELOCATION_EMPTY_PROBE'
Assert-OrderedAfter $dismount 'protected void TryRelocateExactManagedMembers(' 'if (!state.RecordForceClearanceAttempt())' 'm_Watchdog.ResetGroupVehicleActions(group)' 'EXACT_RELOCATION_PROBE_COMMIT'
Assert-Contains $dismount 'trip.GetAssignment().GetGroup() != group' 'EXACT_RELOCATION_COMMIT_REVALIDATION'
Assert-Contains $dismount 'agent.GetControlledEntity() != character' 'EXACT_RELOCATION_COMMIT_REVALIDATION'
Assert-Contains $dismount '!IsPhysicalOnlyBlocker(character, vehicle, boundsMin, boundsMax)' 'EXACT_RELOCATION_COMMIT_REVALIDATION'
Assert-OrderedAfter $dismount 'protected void TryRelocateExactManagedMembers(' 'if (!state.RecordForceClearanceAttempt())' 'character.Teleport(transform)' 'EXACT_RELOCATION_PROBE_COMMIT'

Assert-Contains $cleanup 'HandleProtectedClearanceDeadline(' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_MANAGED_RECOVERY' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_EXACT_RECOVERY' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'PROTECTED_CLEARANCE_DEADLINE_EXCEEDED' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'PROTECTED_CLEARANCE_STABILITY_PENDING_AT_DEADLINE' 'CLEANUP_STABILITY_PENDING'
Assert-Contains $cleanup 'action=CONTINUE_STABLE_CLEAR_PROOF' 'CLEANUP_STABILITY_PENDING'
Assert-OrderedAfter $cleanup 'protected AICF_VehicleCleanupOutcome HandleProtectedClearanceDeadline(' 'ReportProtectedClearanceStabilityPending(' 'ReportProtectedClearanceDeadlineExceeded(job' 'CLEANUP_DEADLINE_CLASSIFICATION'
Assert-Contains $cleanup 'PROTECTED_CLEARANCE_FINAL_OUTCOME' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'PROTECTED_CLEARANCE_TERMINAL_GRACE_MS = 30000' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'TRANSFER_RETAINED_FAIL_CLOSED' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'trip_detach_allowed=1' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_EXACT_MEMBER' 'EXACT_CLEANUP_POSTCONDITION'
Assert-Contains $cleanup 'ejectImmediate,' 'EXACT_CLEANUP_POSTCONDITION'
Assert-Contains $cleanup '"EJECT_ON_THE_SPOT"' 'EXACT_CLEANUP_POSTCONDITION'
Assert-NotContains $cleanup 'RequestSpawn(' 'NO_DESTRUCTIVE_CLEANUP_ROSTER_RECREATE'
Assert-NotContains $cleanup 'RemoveAgent(' 'NO_DESTRUCTIVE_CLEANUP_ROSTER_RECREATE'
Assert-Contains $cleanup 'AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity' 'CLEANUP_MUTATION_AUTHORITY'
Assert-Contains $cleanup 'AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity' 'CLEANUP_MUTATION_AUTHORITY'
Assert-Contains $cleanup 'ObserveLateCleanupFailure(' 'CLEANUP_EXHAUSTION_EVIDENCE'
Assert-Contains $cleanup 'GetManagedLogicalOccupants()' 'LIVE_CLEANUP_AUDIT'
Assert-Contains $cleanup 'GetManagedInsideBounds()' 'LIVE_CLEANUP_AUDIT'
Assert-Contains $cleanup 'NON_ALIVE_DETACH_MAX_ATTEMPTS = 3' 'NON_ALIVE_EXACT_DETACH'
Assert-Contains $cleanup 'TryDetachNonAliveExactOccupants(job, vehicle, nowMs)' 'NON_ALIVE_EXACT_DETACH'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_NON_ALIVE_EXACT_MEMBER' 'NON_ALIVE_EXACT_DETACH_DIAGNOSTICS'
Assert-Contains $cleanup 'managed_non_alive_logical=' 'NON_ALIVE_EXACT_DETACH_DIAGNOSTICS'
Assert-Contains $cleanup 'clock_domain=SYSTEM_TICK' 'CLEANUP_CLOCK_DOMAIN'
Assert-Contains $cleanup 'scan_sequence=%3 scan_safe=%4' 'CLEANUP_SCAN_HISTORY'
Assert-Contains $cleanup 'safe_since_tick_ms=%1' 'CLEANUP_SCAN_HISTORY'
Assert-Contains $cleanup 'current_unsafe_since_ms=%1 current_unsafe_age_ms=%2 current_unsafe_signature=%3' 'CLEANUP_UNSAFE_EPISODE'
Assert-Contains $cleanup 'job.m_sCurrentUnsafeBlockerSignature != scan.m_sBlockerSignature' 'CLEANUP_UNSAFE_EPISODE'
Assert-Contains $cleanup 'job.m_iCurrentUnsafeStartedAtMs = 0' 'CLEANUP_UNSAFE_EPISODE'
Assert-Contains $cleanup 'job.m_sCurrentUnsafeBlockerSignature = string.Empty' 'CLEANUP_UNSAFE_EPISODE'
Assert-Contains $cleanup 'job.m_iLastUnsafeAtMs = scanAtMs' 'CLEANUP_LAST_UNSAFE_SCAN'
Assert-Contains $cleanup 'RETAINED_CLEARANCE_RECHECK_INTERVAL_MS = 5000' 'RETAINED_CLEARANCE_REAPER'
Assert-Contains $cleanup 'ProcessRecoverableRetainedClearance(job, nowMs)' 'RETAINED_CLEARANCE_REAPER'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_RETAINED_RECHECK' 'RETAINED_CLEARANCE_DIAGNOSTICS'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_RETAINED_RECOVERY_STARTED' 'RETAINED_CLEARANCE_DIAGNOSTICS'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_RETAINED_RECOVERED' 'RETAINED_CLEARANCE_DIAGNOSTICS'
Assert-Contains $cleanup 'PROTECTED_CLEARANCE_PLAYER_POSITION_UNKNOWN_GRACE_EXPIRED' 'RETAINED_CLEARANCE_REASON_FIDELITY'
Assert-Contains $cleanup 'job.m_Lease.GetState() == AICF_EVehicleLeaseState.FAILED_CLOSED' 'RETAINED_CLEARANCE_EXACT_LEASE'
Assert-Contains $cleanup 'job.m_Fleet.FindLeaseForSlot(' 'RETAINED_CLEARANCE_EXACT_LEASE'
Assert-OrderedAfter $cleanup 'protected void ProcessRecoverableRetainedClearance(' 'InspectCleanupSafety(job, vehicle, job.m_Scan)' 'ReleaseRecoverableRetainedToWorldPool(job, vehicle, nowMs)' 'RETAINED_CLEARANCE_SAFE_BEFORE_RELEASE'
Assert-OrderedAfter $cleanup '// The second scan and entity lookup are intentionally adjacent to Fleet' 'InspectCleanupSafety(job, vehicle, job.m_Scan)' 'ReleaseRecoverableRetainedToWorldPool(job, vehicle, nowMs)' 'RETAINED_CLEARANCE_IMMEDIATE_RESCAN'
Assert-Contains $lease 'bool BeginRetainedRelease(' 'RETAINED_CLEARANCE_FLEET_TRANSFER'
Assert-Contains $lease 'm_State != AICF_EVehicleLeaseState.FAILED_CLOSED' 'RETAINED_CLEARANCE_FLEET_TRANSFER'
Assert-Contains $fleet 'bool ReleaseRetainedLeaseAt(' 'RETAINED_CLEARANCE_FLEET_TRANSFER'
Assert-Contains $fleet 'bool RetireRetainedLeaseAt(' 'RETAINED_CLEARANCE_FLEET_TRANSFER'
Assert-OrderedAfter $fleet 'bool ReleaseRetainedLeaseAt(' 'lease.GetState() != AICF_EVehicleLeaseState.FAILED_CLOSED' 'm_aLeases.RemoveItem(lease)' 'RETAINED_CLEARANCE_CAP_HELD_UNTIL_TRANSFER'
Assert-Contains $cleanup 'action=KEEP_CAP_HELD_CONTINUE_READ_ONLY_RECHECK' 'RETAINED_CLEARANCE_EXHAUSTION_SAFE'
Assert-Contains $watchdog 'exactCompartmentOwner' 'NON_ALIVE_LOGICAL_OCCUPANT_SCAN'
Assert-Contains $watchdog 'nonAliveLogicalOccupantCount++' 'NON_ALIVE_LOGICAL_OCCUPANT_SCAN'
Assert-Contains $watchdog 'IsAuthoritativeNonPlayerCharacter' 'NON_ALIVE_EXACT_DETACH_AUTHORITY'
Assert-Contains $controller 'CLEANUP_LIVE_SCAN' 'LIVE_CLEANUP_AUDIT'
Assert-Contains $controller 'clearance_source=%1' 'LIVE_CLEANUP_AUDIT'
Assert-Contains $controller 'managed_non_alive_logical=%2' 'LIVE_CLEANUP_AUDIT'

Assert-Contains $planner 'AIWaypoint_Move.et' 'ATTACK_OPERATIONAL_PHASE'
Assert-Contains $planner 'AIWaypoint_SearchAndDestroy.et' 'ATTACK_OBJECTIVE_PHASE'
Assert-Contains $planner 'ATTACK_OBJECTIVE_PROMOTION_RADIUS_METERS = 100.0' 'ATTACK_OBJECTIVE_PHASE'
Assert-Contains $planner 'SetHoldingTime(ATTACK_OBJECTIVE_HOLDING_TIME_SECONDS)' 'ATTACK_OBJECTIVE_PHASE'
Assert-Contains $planner 'SetCompletionType(EAIWaypointCompletionType.All)' 'ATTACK_OPERATIONAL_ALL_COMPLETION'
Assert-Contains $planner 'completion_policy=%4' 'ATTACK_OPERATIONAL_ALL_COMPLETION'
Assert-Contains $planner 'SetHoldingTime(DEFEND_HOLDING_TIME_SECONDS)' 'DEFEND_PERSISTENT_HOLD'
Assert-Contains $match 'PromoteAttackToObjectiveAction(' 'ATTACK_OBJECTIVE_PHASE'

Assert-Contains $groupSlot 'GetOnWaypointCompleted().Insert(OnOwnedWaypointCompleted)' 'WAYPOINT_TERMINAL_PROVENANCE'
Assert-Contains $groupSlot 'GetOnWaypointRemoved().Insert(OnOwnedWaypointRemoved)' 'WAYPOINT_TERMINAL_PROVENANCE'
Assert-Contains $groupSlot 'ORDER_WAYPOINT_TERMINAL_OBSERVED' 'WAYPOINT_TERMINAL_PROVENANCE'
Assert-Contains $groupSlot 'GetOwnedWaypointTerminalOutcome(' 'WAYPOINT_TERMINAL_PROVENANCE'
Assert-Contains $groupSlot 'GROUP_CALLBACK_COMPLETED' 'WAYPOINT_TERMINAL_PROVENANCE'
Assert-Contains $groupSlot 'GROUP_CALLBACK_REMOVED' 'WAYPOINT_TERMINAL_PROVENANCE'
Assert-Contains $match 'WAYPOINT_COMPLETED_OUTSIDE_OBJECTIVE' 'WAYPOINT_TERMINAL_CLASSIFICATION'
Assert-Contains $match 'WAYPOINT_REMOVED_CALLBACK' 'WAYPOINT_TERMINAL_CLASSIFICATION'
Assert-Contains $match 'WAYPOINT_TERMINAL_UNOBSERVED' 'WAYPOINT_TERMINAL_CLASSIFICATION'
Assert-Contains $match 'terminal_provenance=' 'WAYPOINT_TERMINAL_CLASSIFICATION'
Assert-Contains $match 'terminal_age_ms=' 'WAYPOINT_TERMINAL_CLASSIFICATION'
Assert-Contains $match 'terminalOutcome == "GROUP_CALLBACK_COMPLETED"' 'WAYPOINT_PROXIMITY_CONFIRMATION'
Assert-Contains $match 'AICF_Stage2Diagnostics.Warning("FALSE_COMPLETION"' 'FALSE_COMPLETION_PHYSICAL_PROOF'
Assert-Contains $match 'physical_confirmation=REJECTED' 'FALSE_COMPLETION_PHYSICAL_PROOF'
Assert-Contains $match 'GetPendingOrderRecoveryPhysicalProgressMeters()' 'FALSE_COMPLETION_PHYSICAL_PROGRESS'
Assert-Contains $match 'reliability_budget_consumed=0' 'FALSE_COMPLETION_BUDGET_ISOLATION'
Assert-Contains $match '"SUPERSEDED"' 'FALSE_COMPLETION_BUDGET_ISOLATION'
Assert-Contains $match 'FALSE_COMPLETION_ENDPOINT_BUDGET = 3' 'FALSE_COMPLETION_BOUNDED_ENDPOINTS'
Assert-Contains $match 'FALSE_COMPLETION_ENDPOINTS_EXHAUSTED' 'FALSE_COMPLETION_BOUNDED_ENDPOINTS'
Assert-Contains $match 'FALSE_COMPLETION_ROUTE_REPLAN' 'FALSE_COMPLETION_BOUNDED_ENDPOINTS'
Assert-Contains $planner 'GetClosestPositionOnNavmesh(' 'FALSE_COMPLETION_NAVMESH_ENDPOINT'
Assert-Contains $planner 'TryResolveFalseCompletionEndpoint(' 'FALSE_COMPLETION_NAVMESH_ENDPOINT'
Assert-Contains $planner 'HoldPositionForTemporaryRouteReplan(' 'FALSE_COMPLETION_TEMPORARY_HOLD'
Assert-Contains $planner 'LogWaypointRemoved(slot, oldWaypoint, "FALSE_COMPLETION_HOLD", "TEMPORARY_ROUTE_REPLAN_HOLD");' 'FALSE_COMPLETION_HOLD_REMOVAL_REASON'
Assert-NotContains $planner 'LogWaypointRemoved(slot, oldWaypoint, "FALSE_COMPLETION_HOLD", "ROUTE_ENDPOINTS_EXHAUSTED");' 'FALSE_COMPLETION_HOLD_REMOVAL_REASON'
Assert-Contains $planner 'bool endpointResolved = TryResolveFalseCompletionEndpoint(' 'FALSE_COMPLETION_ENDPOINT_RESOLUTION_FALLBACK'
Assert-Contains $planner 'endpoint_resolution=UNAVAILABLE next_action=REQUEST_TEMPORARY_ROUTE_REPLAN_HOLD reliability_budget_consumed=0' 'FALSE_COMPLETION_ENDPOINT_RESOLUTION_FALLBACK'
Assert-Contains $match 'bool falseCompletionRouteRecovery =' 'FALSE_COMPLETION_ISSUE_REJECTION_ISOLATION'
Assert-Contains $match 'slot.GetFalseCompletionEndpointRevision() > 0;' 'FALSE_COMPLETION_PERSISTENT_ROUTE_OWNERSHIP'
Assert-NotContains $match 'failureReason == "FALSE_COMPLETION_ENDPOINT_REJECTED" ||' 'FALSE_COMPLETION_PERSISTENT_ROUTE_OWNERSHIP'
Assert-Contains $match 'RecordImmediateOrderRepairSuperseded(' 'FALSE_COMPLETION_ISSUE_REJECTION_ISOLATION'
Assert-OrderedAfter $match 'protected void RecordImmediateOrderRepairSuperseded(' 'm_iOrderRecoverySuperseded++;' '"ORDER_REPAIR_ATTEMPT_TERMINATED"' 'FALSE_COMPLETION_SUPERSEDED_ACCOUNTING'
Assert-Contains $match 'outcome=SUPERSEDED reason=%4 confirmation_basis=TEMPORARY_ROUTE_REPLAN_HOLD' 'FALSE_COMPLETION_SUPERSEDED_ACCOUNTING'
Assert-Contains $match 'fallbackAction = "TEMPORARY_ROUTE_REPLAN_HOLD"' 'FALSE_COMPLETION_ISSUE_REJECTION_ISOLATION'
Assert-Contains $match 'if (!recovered && !temporaryRouteReplanHoldCommitted)' 'FALSE_COMPLETION_NO_RELIABILITY_BUDGET_CHARGE'
Assert-OrderedAfter $match 'protected bool TryRecoverOrder(' 'falseCompletionRouteRecovery' 'RecordImmediateOrderRepairFailure(' 'FALSE_COMPLETION_ISSUE_REJECTION_ISOLATION'
Assert-OrderedAfter $match 'protected bool TryRecoverOrder(' 'HoldPositionForTemporaryRouteReplan(' 'RecordImmediateOrderRepairFailure(' 'FALSE_COMPLETION_ISSUE_REJECTION_ROUTE_HOLD'
Assert-OrderedAfter $match 'protected bool TryRecoverOrder(' 'm_iOrderRecoveryAttempts++;' 'RecordImmediateOrderRepairSuperseded(' 'FALSE_COMPLETION_SUPERSEDED_ACCOUNTING'
Assert-OrderedAfter $match 'protected bool TryRecoverOrder(' 'if (!recovered && !temporaryRouteReplanHoldCommitted)' 'RecordImmediateOrderRepairFailure(' 'FALSE_COMPLETION_NO_RELIABILITY_BUDGET_CHARGE'
Assert-OrderedAfter $match 'int reliabilityBudgetConsumed = 0;' 'if (!temporaryRouteReplanHoldCommitted &&' 'ApplyOrderReliabilityRepairBudgetFallback(' 'FALSE_COMPLETION_HOLD_BYPASSES_EXHAUSTED_FALLBACK'
Assert-OrderedAfter $match 'protected bool TryHoldCompletedOrderAtObjective(' 'TryResolveSlotTargetPosition(slot, target, targetPosition)' 'slot.BeginObjectiveHold(target)' 'FALSE_COMPLETION_OBJECTIVE_PHYSICAL_PROOF'
Assert-OrderedAfter $match 'protected bool TryHoldCompletedOrderAtObjective(' 'slot.BeginObjectiveHold(target)' 'slot.ResetFalseCompletionRecovery();' 'FALSE_COMPLETION_OBJECTIVE_ROUTE_EPISODE_CLOSE'
Assert-OrderedAfter $match 'protected bool TryHoldCompletedOrderAtObjective(' 'slot.ResetFalseCompletionRecovery();' 'slot.ConfirmAtObjective(target, distanceMeters);' 'FALSE_COMPLETION_OBJECTIVE_ROUTE_EPISODE_CLOSE'
Assert-OrderedAfter $match 'protected bool RevalidateFactionOrders(' 'slot.IsTemporaryRouteReplanHold()' 'slot.HasPendingOrderRecovery()' 'FALSE_COMPLETION_HOLD_OWNERSHIP'
Assert-OrderedAfter $match 'protected void ResumeAfterFalseCompletionHold(' 'SupersedePendingOrderRecovery(' 'slot.ResetFalseCompletionRecovery()' 'FALSE_COMPLETION_REPLAN_TERMINAL_ACCOUNTING'
Assert-OrderedAfter $match 'AICF_Stage2Diagnostics.Warning("FALSE_COMPLETION", falseCompletionDetails);' 'RecordPendingOrderRepairTerminal(' 'slot.ClearPendingOrderRecovery();' 'FALSE_COMPLETION_REJECTION_TERMINAL_ACCOUNTING'
Assert-OrderedAfter $match 'AICF_Stage2Diagnostics.Warning("FALSE_COMPLETION", falseCompletionDetails);' 'slot.ClearPendingOrderRecovery();' 'HoldPositionForTemporaryRouteReplan(' 'FALSE_COMPLETION_REJECTION_TERMINAL_ACCOUNTING'
Assert-Contains $match 'bool consumesStuckBudget = !alreadyCountsAsStuck && !countsAsReliabilityRepair' 'RELIABILITY_STUCK_BUDGET_ISOLATION'
Assert-Contains $match 'bool enforceStuckBudget = alreadyCountsAsStuck || consumesStuckBudget' 'RELIABILITY_STUCK_BUDGET_ISOLATION'
Assert-Contains $groupSlot 'return routeProgressResumed;' 'STUCK_ROUTE_PROGRESS_ONLY'
Assert-NotContains $groupSlot 'return movementResumed || routeProgressResumed' 'STUCK_ROUTE_PROGRESS_ONLY'
Assert-Contains $match 'failedOutcome = "MOVEMENT_ONLY_REGRESSED"' 'STUCK_MOVEMENT_CLASSIFICATION'
Assert-Contains $match 'failedOutcome = "MOVEMENT_ONLY"' 'STUCK_MOVEMENT_CLASSIFICATION'
Assert-Contains $groupSlot 'RecordStuckRecoveryTerminalOutcome("ROUTE_PROGRESS")' 'STUCK_ROUTE_PROGRESS_ONLY'
Assert-Contains $match 'AuditStuckRecoveryAccounting(' 'STUCK_ACCOUNTING_INVARIANT'
Assert-Contains $match 'STUCK_RECOVERY_ACCOUNTING_INVARIANT_FAILED' 'STUCK_ACCOUNTING_INVARIANT'
Assert-Contains $match 'invariant=attempted_equals_all_terminal_outcomes_plus_pending' 'STUCK_ACCOUNTING_INVARIANT'
Assert-Contains $match 'stuck_unaccounted=' 'STUCK_ACCOUNTING_INVARIANT'
Assert-Contains $match 'if (stuckUnaccounted != 0)' 'STUCK_ACCOUNTING_INVARIANT'
Assert-NotContains $match 'IsPersistentStuckFieldHoldRetryDue(' 'NO_TIMED_PERSISTENT_STUCK_RETRY'
Assert-Contains $match 'resume=STRATEGIC_CONTEXT_CHANGE auto_retry=0' 'NO_TIMED_PERSISTENT_STUCK_RETRY'
Assert-Contains $match 'trigger=STRATEGIC_CONTEXT_CHANGE reason=%1 auto_retry=0' 'NO_TIMED_PERSISTENT_STUCK_RETRY'
Assert-Contains $match 'next_action=WAIT_STRATEGIC_CONTEXT_CHANGE offscreen_recovery=NOT_IMPLEMENTED' 'NO_TIMED_PERSISTENT_STUCK_RETRY'

Assert-Contains $coordinator 'slot.HasPendingOrderRecovery()' 'TRANSPORT_ORDER_RECOVERY_ADMISSION_FENCE'
Assert-Contains $coordinator 'return "ORDER_RECOVERY_PENDING"' 'TRANSPORT_ORDER_RECOVERY_ADMISSION_FENCE'
Assert-Contains $coordinator 'AICF_ETransportTripPhase.APPROACHING_SITE' 'TRANSPORT_ORDER_RECOVERY_ADMISSION_FENCE'
Assert-Contains $coordinator 'AICF_ETransportTripPhase.SPAWN_COMMIT' 'TRANSPORT_ORDER_RECOVERY_ADMISSION_FENCE'
Assert-Contains $coordinator 'GetInfantryOrderAdmissionFenceReason(slot)' 'TRANSPORT_FALSE_COMPLETION_HOLD_FENCE'
Assert-Contains $coordinator 'slot.IsTemporaryRouteReplanHold()' 'TRANSPORT_FALSE_COMPLETION_HOLD_FENCE'
Assert-Contains $coordinator 'return "TEMPORARY_ROUTE_REPLAN_HOLD"' 'TRANSPORT_FALSE_COMPLETION_HOLD_FENCE'
Assert-Contains $coordinator 'if (!orderOwnershipFence.IsEmpty() &&' 'TRANSPORT_FALSE_COMPLETION_PREBOARDING_FENCE'
foreach ($phase in @('WAITING_FOR_SITE', 'SITE_PLANNED', 'APPROACHING_SITE', 'STAGING_CONFIRMED', 'SPAWN_COMMIT')) {
    Assert-Contains $coordinator ("activePhase == AICF_ETransportTripPhase.$phase") 'TRANSPORT_FALSE_COMPLETION_PREBOARDING_FENCE'
}
Assert-OrderedAfter $coordinator 'if (!orderOwnershipFence.IsEmpty() &&' 'ReportAdmissionOnce(assignment, orderOwnershipFence);' 'return;' 'TRANSPORT_FALSE_COMPLETION_PREBOARDING_FENCE'
Assert-OrderedAfter $coordinator 'if (!orderOwnershipFence.IsEmpty() &&' 'return;' 'AICF_TripOutcome observed = m_TripController.Tick(' 'TRANSPORT_FALSE_COMPLETION_PREBOARDING_FENCE'
Assert-OrderedAfter $coordinator 'protected bool IsAdmissionEligible(' 'GetInfantryOrderAdmissionFenceReason(slot)' 'reason = orderOwnershipFence;' 'TRANSPORT_FALSE_COMPLETION_ADMISSION_FENCE'
Assert-OrderedAfter $coordinator 'protected bool IsAdmissionEligible(' 'reason = orderOwnershipFence;' 'return false;' 'TRANSPORT_FALSE_COMPLETION_ADMISSION_FENCE'
Assert-OrderedAfter $coordinator 'protected bool IsAdmissionEligible(' 'return false;' 'slot.GetUnitType()' 'TRANSPORT_FALSE_COMPLETION_ADMISSION_FENCE'

Assert-Contains $match 'MOB_EGRESS_SOFT_NUDGE' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_DELAYED_PROGRESSING' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_DEADLINE_DEFERRED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'remaining_hard_ms=' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'player_clearance=' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'los_clearance=' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'combat_clearance=' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_ATTEMPTED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_SUBMITTED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_ACCEPTED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_PHYSICALLY_CONFIRMED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_SUBMISSION_FAILED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_MEMBER' 'BOUNDED_MOB_EGRESS_RECOVERY'
$hiddenMobRecovery = [regex]::Match(
    $match,
    '(?s)protected bool TryApplyHiddenMobEgressRecovery\(.*?(?=\r?\n\tprotected bool TryFindDistinctMobEgressDestination\()'
)
if (-not $hiddenMobRecovery.Success) {
    $issues.Add('[MOB_EGRESS_RUNTIME_REVISION_HANDOFF] TryApplyHiddenMobEgressRecovery body not found')
} else {
    Assert-Contains $hiddenMobRecovery.Value 'int expectedStrategicIntentRevision = slot.GetStrategicIntentRevision();' 'MOB_EGRESS_RUNTIME_REVISION_HANDOFF'
    Assert-Contains $hiddenMobRecovery.Value 'AICF_EStrategicDecisionAuthority expectedDecisionAuthority =' 'MOB_EGRESS_RUNTIME_REVISION_HANDOFF'
    Assert-Contains $hiddenMobRecovery.Value 'slot.GetDecisionAuthority();' 'MOB_EGRESS_RUNTIME_REVISION_HANDOFF'
    Assert-Contains $hiddenMobRecovery.Value 'int resultingAssignmentRevision = slot.GetStrategicAssignmentRevision();' 'MOB_EGRESS_RUNTIME_REVISION_HANDOFF'
    Assert-Contains $hiddenMobRecovery.Value 'resultingAssignmentRevision == expectedAssignmentRevision + 1;' 'MOB_EGRESS_RUNTIME_REVISION_HANDOFF'
    Assert-Contains $hiddenMobRecovery.Value 'slot.GetGroup() == group && slot.GetSpawnGeneration() == expectedGeneration &&' 'MOB_EGRESS_RUNTIME_IDENTITY_FENCE'
    Assert-Contains $hiddenMobRecovery.Value 'slot.GetTargetBase() == expectedTarget &&' 'MOB_EGRESS_STRATEGIC_IDENTITY_FENCE'
    Assert-Contains $hiddenMobRecovery.Value 'slot.GetDecisionAuthority() == expectedDecisionAuthority &&' 'MOB_EGRESS_STRATEGIC_IDENTITY_FENCE'
    Assert-Contains $hiddenMobRecovery.Value 'slot.GetStrategicIntentRevision() == expectedStrategicIntentRevision;' 'MOB_EGRESS_STRATEGIC_IDENTITY_FENCE'
    Assert-Contains $hiddenMobRecovery.Value 'slot.GetDecisionAuthority() != expectedDecisionAuthority ||' 'MOB_EGRESS_COMMIT_BOUNDARY_FENCE'
    Assert-Contains $hiddenMobRecovery.Value 'slot.GetStrategicIntentRevision() != expectedStrategicIntentRevision ||' 'MOB_EGRESS_COMMIT_BOUNDARY_FENCE'
    Assert-Contains $hiddenMobRecovery.Value 'runtimeWaypointRevisionAdvanced && strategicIdentityPreserved &&' 'MOB_EGRESS_RUNTIME_REVISION_HANDOFF'
    Assert-Contains $hiddenMobRecovery.Value 'slot.GetWaypoint() != expectedWaypoint &&' 'MOB_EGRESS_RUNTIME_IDENTITY_FENCE'
    Assert-Contains $hiddenMobRecovery.Value 'result_assignment_revision=%2' 'MOB_EGRESS_RUNTIME_REVISION_DIAGNOSTICS'
    Assert-NotContains $hiddenMobRecovery.Value 'slot.GetStrategicAssignmentRevision() == expectedAssignmentRevision &&' 'MOB_EGRESS_RUNTIME_REVISION_HANDOFF'
    Assert-Ordered $hiddenMobRecovery.Value 'slot.GetStrategicIntentRevision() != expectedStrategicIntentRevision ||' 'slot.MarkMobEgressHiddenMutationConsumed()' 'MOB_EGRESS_COMMIT_BOUNDARY_FENCE'
    Assert-Ordered $hiddenMobRecovery.Value 'm_OrderPlanner.RebuildCurrentOrder(' 'resultingAssignmentRevision == expectedAssignmentRevision + 1;' 'MOB_EGRESS_RUNTIME_REVISION_HANDOFF'
}
Assert-Contains $groupSlot 'm_bMobEgressHiddenMutationConsumed' 'ONE_SHOT_MOB_EGRESS_MUTATION'
Assert-Contains $groupSlot 'if (resetHiddenMutation)' 'ONE_SHOT_MOB_EGRESS_MUTATION'
Assert-Contains $match '!slot.IsMobEgressHiddenMutationConsumed()' 'ONE_SHOT_MOB_EGRESS_MUTATION'
Assert-Ordered $match 'slot.MarkMobEgressHiddenMutationConsumed()' 'characters[relocateIndex].Teleport(transform)' 'ONE_SHOT_MOB_EGRESS_MUTATION'
Assert-Ordered $match 'MOB_EGRESS_HIDDEN_RECOVERY_PHYSICALLY_CONFIRMED' 'slot.ResetMobEgressRecovery(true)' 'MOB_EGRESS_EPISODE_REARM'
Assert-Contains $match 'mobEnvelopeInsideMembers == 0' 'MOB_EGRESS_EPISODE_REARM'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_SEARCH_RADIUS_METERS = 35.0' 'MOB_EGRESS_TERRAIN_SEARCH'
Assert-Contains $match 'TryFindDistinctMobEgressDestination(' 'DISTINCT_MOB_EGRESS_DESTINATIONS'
Assert-Contains $match 'FindAllEmptyTerrainPositions(' 'DISTINCT_MOB_EGRESS_DESTINATIONS'
Assert-Contains $match 'overlapsReserved' 'DISTINCT_MOB_EGRESS_DESTINATIONS'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_OWNERSHIP_TAKEOVER' 'MOB_EGRESS_ORDER_RECOVERY_PREEMPTION'
Assert-Contains $match 'SupersedePendingOrderRecovery(' 'MOB_EGRESS_ORDER_RECOVERY_PREEMPTION'
Assert-Contains $match 'slot.GetRole() == AICF_EGroupRole.RESERVE && slot.GetTargetBase() == mainBase' 'MOB_IDLE_ROLE_CONTRACT'
Assert-Contains $match 'return "HQ_RESERVE"' 'MOB_IDLE_ROLE_CONTRACT'
Assert-Contains $match 'array<AIAgent> relocationAgents' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'array<ChimeraCharacter> characters' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'vector.DistanceXZ(memberOrigin, mobOrigin)' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'total_members=' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'inside_members=' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'relocated=' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'all_members_outside_mob_immediate=' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'before=%5 destination=%6 after=%7' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'IsHiddenMobRecoveryCombatSafe(' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'CanApplyHiddenRecovery(' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'identity_preserved=1 roster_recreated=0' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'physical_confirmation=DEFERRED_TO_NEXT_AUDIT' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_BLOCKED_BY_SAFETY' 'MOB_EGRESS_SAFETY_PAUSE'
Assert-Contains $match 'state=EGRESS_BLOCKED_BY_SAFETY' 'MOB_EGRESS_SAFETY_PAUSE'
Assert-Contains $match 'hard_deadline=PAUSED acceptance_failure=0' 'MOB_EGRESS_SAFETY_PAUSE'
Assert-Contains $match 'MOB_EGRESS_SAFETY_CLEARED' 'MOB_EGRESS_SAFETY_RESUME'
Assert-Contains $match 'deadline_state=RESTARTED' 'MOB_EGRESS_SAFETY_RESUME'
Assert-Contains $match 'IsMobEgressSafetyRejection(' 'MOB_EGRESS_SAFETY_CLASSIFICATION'
Assert-Contains $groupSlot 'ObserveMobEgressSafetyBlock(' 'MOB_EGRESS_SAFETY_STATE'
Assert-Contains $groupSlot 'ShouldReportMobEgressSafetyHeartbeat(' 'MOB_EGRESS_SAFETY_HEARTBEAT'
Assert-Ordered $match 'MOB_EGRESS_SOFT_NUDGE' 'MOB_EGRESS_HIDDEN_RECOVERY_ATTEMPTED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Ordered $match 'MOB_EGRESS_HIDDEN_RECOVERY_ATTEMPTED' 'MOB_EGRESS_HIDDEN_RECOVERY_SUBMITTED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Ordered $match 'MOB_EGRESS_HIDDEN_RECOVERY_SUBMITTED' 'MOB_EGRESS_HIDDEN_RECOVERY_ACCEPTED' 'BOUNDED_MOB_EGRESS_RECOVERY'

Assert-Contains $groupSlot 'm_bPendingOrderRecoveryCountsAsReliabilityAttempt' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $groupSlot 'm_iPendingOrderRecoveryReliabilityAttemptId' 'RELIABILITY_ATTEMPT_IDENTITY'
Assert-Contains $groupSlot 'm_iPendingOrderRecoveryAssignmentRevision' 'RELIABILITY_ATTEMPT_IDENTITY'
Assert-Contains $match 'TryConfirmPendingOrderRecoveryByRelayCapture(' 'RELAY_CAPTURE_TERMINAL_PROOF'
Assert-Contains $match 'exact_target_revision_proof=1' 'RELAY_CAPTURE_TERMINAL_PROOF'
Assert-Contains $match 'ORDER_REPAIR_ATTEMPT_TERMINATED' 'RELIABILITY_ATTEMPT_OUTCOME'
Assert-Contains $match 'ORDER_RELIABILITY_REPAIR_FAILURE_BUDGET = 2' 'RELIABILITY_FAILURE_BUDGET'
Assert-Contains $groupSlot 'RecordOrderReliabilityRepairFailure()' 'RELIABILITY_FAILURE_BUDGET'
Assert-Contains $groupSlot 'ResetOrderReliabilityRepairFailureBudget()' 'RELIABILITY_FAILURE_BUDGET'
Assert-Contains $match 'ORDER_REPAIR_FAILURE_BUDGET_EXHAUSTED' 'RELIABILITY_FAILURE_BUDGET'
Assert-Contains $match 'ORDER_REPAIR_FAILURE_FALLBACK' 'RELIABILITY_FAILURE_BUDGET'
Assert-Contains $match 'budget_domain=RELIABILITY_REPAIR' 'RELIABILITY_FAILURE_BUDGET'
Assert-Contains $match 'repair_retry=BLOCKED_UNTIL_ASSIGNMENT_OR_GENERATION_CHANGE' 'RELIABILITY_FAILURE_BUDGET'
Assert-Contains $match 'order_repair_attempted=' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'order_repair_confirmed=' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'order_repair_failed=' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'order_repair_superseded=' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'order_repair_unaccounted=' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'ORDER_REPAIR_ACCOUNTING_INVARIANT_FAILED' 'RELIABILITY_COUNTER_INVARIANT'
Assert-Contains $match 'invariant=attempted_equals_confirmed_plus_failed_plus_superseded_plus_pending' 'RELIABILITY_COUNTER_INVARIANT'
Assert-Contains $match 'm_iOrderRecoveryAttemptSequence' 'RELIABILITY_ATTEMPT_IDENTITY'
Assert-Ordered $match 'bool recovered = m_OrderPlanner.RecoverOrder(' 'm_iOrderRecoveryAttempts++;' 'RELIABILITY_COUNTER_INVARIANT'
Assert-Ordered $match 'RecordImmediateOrderRepairFailure(' 'AIWaypoint newWaypoint = slot.GetWaypoint()' 'RELIABILITY_COUNTER_INVARIANT'
Assert-Contains $match 'PLAYER_ROLE_CHANGE' 'RELIABILITY_COUNTER_INVARIANT'
Assert-Contains $match 'handoff_verified=' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'ORDER_BINDING_STABLE' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $groupSlot 'string evidenceWaypointId = "NONE"' 'STUCK_EVIDENCE_NULL_SAFE'
Assert-Contains $groupSlot 'if (m_PendingStuckRecoveryWaypoint)' 'STUCK_EVIDENCE_NULL_SAFE'

Assert-Contains $handoff 'AssignLoneSurvivorRetreat(' 'VEHICLE_FALLBACK_LONE_SURVIVOR'
Assert-Contains $handoff 'handoff_mode=%2 alive=%3 transition_owner=VEHICLE_HANDOFF' 'VEHICLE_FALLBACK_SINGLE_OWNER'
Assert-Contains $planner 'VEHICLE_FALLBACK_LONE_SURVIVOR' 'VEHICLE_FALLBACK_LONE_SURVIVOR'
Assert-Contains $planner 'CreateLoneSurvivorRetreatWaypoint(' 'VEHICLE_FALLBACK_LONE_SURVIVOR'
Assert-Contains $match 'LONE_SURVIVOR_RETREAT_COMPLETED' 'LONE_SURVIVOR_OPERATIONAL_RETURN'

if ($issues.Count -gt 0) {
    Write-Host "Stage 3.5 recovery policy audit: FAIL ($($issues.Count) issue(s))" -ForegroundColor Red
    foreach ($issue in $issues) {
        Write-Host " - $issue" -ForegroundColor Red
    }
    exit 1
}

Write-Host 'Stage 3.5 recovery policy audit: PASS' -ForegroundColor Green
