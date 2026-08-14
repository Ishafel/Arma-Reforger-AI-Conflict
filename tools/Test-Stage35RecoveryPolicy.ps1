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
$planner = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_OrderPlanner.c'
$match = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c'
$groupSlot = Read-RepoFile 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c'

Assert-Contains $stage3 'DEFAULT_BOARDING_TIMEOUT_MS = 40000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_STUCK_TIMEOUT_MS = 45000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_PASSENGER_STALL_MS = 8000' 'TIMING_DEFAULTS'
Assert-Contains $stage3 'DEFAULT_PASSENGER_MAX_RETRIES = 1' 'TIMING_DEFAULTS'
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

Assert-Contains $phaseStates 'm_bTerminalClearanceStarted' 'TERMINAL_CLEARANCE_MODE'
Assert-Contains $phaseStates 'void BeginTerminal(' 'TERMINAL_CLEARANCE_MODE'
Assert-Contains $controller 'IsTerminalClearanceStarted()' 'TERMINAL_CLEARANCE_MODE'
Assert-Contains $dismount 'CONTINUOUS_CLEAR_MS = 5000' 'TERMINAL_CLEARANCE_STABILITY'
Assert-Contains $dismount 'ResetGroupVehicleActions(group)' 'TERMINAL_REBOARDING_BREAK'
Assert-Contains $dismount 'CanApplyTerminalHiddenRecovery(' 'TERMINAL_HIDDEN_FENCE'
Assert-Contains $dismount 'TERMINAL_CLEARANCE_RECOVERY_DEFERRED' 'TERMINAL_HIDDEN_FENCE'

Assert-Contains $cleanup 'PROTECTED_CLEARANCE_DEADLINE_POLLING' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_MANAGED_RECOVERY' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_EXACT_RECOVERY' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'VEHICLE_CLEANUP_RECOVERY_REARMED' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'MANAGED_EXACT_RECOVERY_REARM_MS = 30000' 'RECOVERABLE_CLEANUP_DEADLINE'
Assert-Contains $cleanup 'AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity' 'CLEANUP_MUTATION_AUTHORITY'
Assert-Contains $cleanup 'AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity' 'CLEANUP_MUTATION_AUTHORITY'
Assert-Contains $cleanup 'ObserveLateCleanupFailure(' 'CLEANUP_EXHAUSTION_EVIDENCE'

Assert-Contains $planner 'AIWaypoint_Move.et' 'ATTACK_OPERATIONAL_PHASE'
Assert-Contains $planner 'AIWaypoint_SearchAndDestroy.et' 'ATTACK_OBJECTIVE_PHASE'
Assert-Contains $planner 'ATTACK_OBJECTIVE_PROMOTION_RADIUS_METERS = 100.0' 'ATTACK_OBJECTIVE_PHASE'
Assert-Contains $planner 'SetHoldingTime(ATTACK_OBJECTIVE_HOLDING_TIME_SECONDS)' 'ATTACK_OBJECTIVE_PHASE'
Assert-Contains $match 'PromoteAttackToObjectiveAction(' 'ATTACK_OBJECTIVE_PHASE'

Assert-Contains $groupSlot 'm_bPendingOrderRecoveryCountsAsReliabilityAttempt' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'order_repair_attempted=' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'order_repair_confirmed=' 'RELIABILITY_COUNTER_DOMAINS'
Assert-Contains $match 'order_repair_failed=' 'RELIABILITY_COUNTER_DOMAINS'
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
