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

$stage2 = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage2Config.c'
$stage3 = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage3Config.c'
$boarding = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBoardingFlow.c'
$tokens = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehicleBoardingTokens.c'
$watchdog = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleWatchdog.c'
$transit = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'
$phaseStates = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehiclePhaseStates.c'
$dismount = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleDismountFlow.c'
$cleanup = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCleanupManager.c'
$controller = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_TransportTripController.c'
$coordinator = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c'
$planner = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_OrderPlanner.c'
$match = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c'
$groupSlot = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c'

Assert-Contains $stage3 'DEFAULT_BOARDING_TIMEOUT_MS = 40000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_STUCK_TIMEOUT_MS = 45000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_PASSENGER_STALL_MS = 8000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_PASSENGER_MAX_RETRIES = 1' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_CLEANUP_DELAY_MS = 10000' 'TIMING_DEFAULTS'
Assert-Contains $stage2 'DEFAULT_STUCK_TIMEOUT_MS = 60000' 'TIMING_DEFAULTS'
Assert-Contains $stage2 'DEFAULT_MAX_STUCK_RECOVERIES = 2' 'TIMING_DEFAULTS'

Assert-Contains $boarding 'm_Config.GetPassengerStallMs()' 'EXACT_CARGO_RECOVERY'
Assert-Contains $boarding 'm_Config.GetPassengerMaxRetries()' 'EXACT_CARGO_RECOVERY'
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

Assert-Contains $cleanup 'HandleProtectedClearanceDeadline(' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_MANAGED_RECOVERY' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_EXACT_RECOVERY' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'PROTECTED_CLEARANCE_DEADLINE_EXCEEDED' 'RECOVERABLE_CLEANUP_DEADLINE'
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
Assert-Contains $planner 'SetCompletionType(EAIWaypointCompletionType.Leader)' 'ATTACK_OPERATIONAL_LEADER_COMPLETION'
Assert-Contains $planner 'completion_policy=%4' 'ATTACK_OPERATIONAL_LEADER_COMPLETION'
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
Assert-Contains $match 'bool consumesStuckBudget = !alreadyCountsAsStuck && !countsAsReliabilityRepair' 'RELIABILITY_STUCK_BUDGET_ISOLATION'
Assert-Contains $match 'bool enforceStuckBudget = alreadyCountsAsStuck || consumesStuckBudget' 'RELIABILITY_STUCK_BUDGET_ISOLATION'

Assert-Contains $coordinator 'slot.HasPendingOrderRecovery()' 'TRANSPORT_ORDER_RECOVERY_ADMISSION_FENCE'
Assert-Contains $coordinator 'reason = "ORDER_RECOVERY_PENDING"' 'TRANSPORT_ORDER_RECOVERY_ADMISSION_FENCE'
Assert-Contains $coordinator 'AICF_ETransportTripPhase.ACQUIRING' 'TRANSPORT_ORDER_RECOVERY_ADMISSION_FENCE'

Assert-Contains $match 'MOB_EGRESS_SOFT_NUDGE' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_DELAYED_PROGRESSING' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_DEADLINE_DEFERRED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'remaining_hard_ms=' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'player_clearance=' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'los_clearance=' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'combat_clearance=' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_ATTEMPTED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_COMMITTED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_VERIFIED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_PARTIAL' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'MOB_EGRESS_HIDDEN_RECOVERY_MEMBER' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $groupSlot 'm_bMobEgressHiddenMutationConsumed' 'ONE_SHOT_MOB_EGRESS_MUTATION'
Assert-Contains $match '!slot.IsMobEgressHiddenMutationConsumed()' 'ONE_SHOT_MOB_EGRESS_MUTATION'
Assert-Ordered $match 'slot.MarkMobEgressHiddenMutationConsumed()' 'characters[relocateIndex].Teleport(transform)' 'ONE_SHOT_MOB_EGRESS_MUTATION'
Assert-Contains $match 'array<AIAgent> relocationAgents' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'array<ChimeraCharacter> characters' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'vector.DistanceXZ(memberOrigin, mobOrigin)' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'total_members=' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'inside_members=' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'relocated=' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'all_members_outside_mob=' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'before=%5 destination=%6 after=%7' 'EXACT_MOB_EGRESS_MEMBERS'
Assert-Contains $match 'IsHiddenMobRecoveryCombatSafe(' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'CanApplyHiddenRecovery(' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Contains $match 'identity_preserved=1 roster_recreated=0' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Ordered $match 'MOB_EGRESS_SOFT_NUDGE' 'MOB_EGRESS_HIDDEN_RECOVERY_ATTEMPTED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Ordered $match 'MOB_EGRESS_HIDDEN_RECOVERY_ATTEMPTED' 'MOB_EGRESS_HIDDEN_RECOVERY_VERIFIED' 'BOUNDED_MOB_EGRESS_RECOVERY'
Assert-Ordered $match 'MOB_EGRESS_HIDDEN_RECOVERY_VERIFIED' 'MOB_EGRESS_HIDDEN_RECOVERY_COMMITTED' 'BOUNDED_MOB_EGRESS_RECOVERY'

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
Assert-Contains $match 'handoff_verified=' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'ORDER_BINDING_STABLE' 'RELIABILITY_COUNTER_DOMAINS'

if ($issues.Count -gt 0) {
    Write-Host "Stage 3.5 recovery policy audit: FAIL ($($issues.Count) issue(s))" -ForegroundColor Red
    foreach ($issue in $issues) {
        Write-Host " - $issue" -ForegroundColor Red
    }
    exit 1
}

Write-Host 'Stage 3.5 recovery policy audit: PASS' -ForegroundColor Green
