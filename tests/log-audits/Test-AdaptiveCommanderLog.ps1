param([Parameter(Mandatory=$true)][string]$LogPath, [switch]$Baseline)
$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw -Encoding UTF8
$failures = [Collections.Generic.List[string]]::new()
if ($log -notmatch '\[ROSTER_READY\]' -or $log -notmatch 'Game destroyed\.') { $failures.Add('Incomplete stopped runtime lifecycle') }
if ($log -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Unhandled exception|NULL pointer|PMC_EQUIPMENT_FAILED') { $failures.Add('SCRIPT/fatal gate failed') }
if ($Baseline) {
    foreach ($snapshot in 0,1) {
        foreach ($slot in 0,1,2) {
            $target=2
            if ($slot -eq 0) { $target=1 }
            if ($log -notmatch "\[ADAPTIVE_CONTROLLED_BASELINE\] snapshot=$snapshot faction=US slot=$slot target=$target ") {
                $failures.Add("Missing original selector snapshot=$snapshot slot=$slot target=$target")
            }
        }
    }
    foreach ($slot in 0,1,2) {
        $target=2
        if ($slot -eq 0) { $target=1 }
        if ($log -notmatch "\[ADAPTIVE_BASELINE_APPLIED\] slot=$slot target=$target passed=1 ") {
            $failures.Add("Missing actual baseline assignment slot=$slot")
        }
    }
    if ($log -notmatch '\[ADAPTIVE_BASELINE\] finished=1') { $failures.Add('Baseline did not finish') }
} else {
    $cases = @(
        'stable_tie','repeat_snapshot','victory_value','open_direction','connectivity_value',
        'approach_near','approach_position_changed','unavailable_near','deferred_near','known_route_cost',
        'unknown_resistance_nonzero','spread_same_cycle','concentrate_deficit','hq_threat','friendly_base_threat',
        'stale_threat_fallback','unknown_threat_fallback','full_force','weakened_force','critical_force','not_ready_force',
        'critical_no_attack','not_ready_no_assignment','small_oscillation','progress_bonus','no_candidates',
        'exact_async_rosters','live_distribution_waypoints','live_same_cycle_allocation','live_joint_attack_waypoints',
        'live_dwell_held','live_stable_candidate_held','live_gates_pass_change','live_urgent_hq_bypasses_gates',
        'live_urgent_defend_waypoints','live_manual_preserved','live_manual_allocated','live_manual_spread',
        'stale_generation_rejected','stale_graph_rejected','stale_assignment_rejected','live_not_ready_rejected',
        'live_failed_route_excluded','live_failure_expired','live_controller_origin_ignored',
        'live_invalid_target_bypasses_gates','loss_signal_survives_rebuild','live_vehicle_suspension',
        'live_faction_independence','foreign_slot_rejected','live_graph_dependency','live_graph_bypass',
        'live_no_attack_fallback','live_friendly_base_threat','loss_signal_expires',
        'same_target_hq_prepared','same_target_hq_decision_urgent','same_target_hq_immediate','same_target_hq_repeat_stable','same_target_hq_return_held',
        'same_target_base_prepared','same_target_base_decision_urgent','same_target_base_immediate','same_target_base_repeat_stable','same_target_base_return_held',
        'manual_owner_same_pass','manual_owner_old_waypoint_removed','manual_role_same_pass','manual_role_old_waypoint_removed',
        'manual_suspended_same_pass','manual_suspended_old_waypoint_removed'
    )
    foreach ($case in $cases) {
        if ([regex]::Matches($log, '\[ADAPTIVE_PROBE\] case=' + [regex]::Escape($case) + ' passed=1 ').Count -ne 1) { $failures.Add("Missing or repeated case $case") }
    }
    if ([regex]::Matches($log,'\[ADAPTIVE_PROBE\] case=async_roster_request_\S+ passed=1 ').Count -ne 4) { $failures.Add('Missing four async roster requests') }
    if ($log -match '\[ADAPTIVE_PROBE\].*passed=0') { $failures.Add('Failed probe assertion') }
    if ($log -notmatch '\[ADAPTIVE_PROBE_FINISHED\] checks=75 failures=0 phase=16') { $failures.Add('No successful full probe completion') }
    if ($log -notmatch '\[ADAPTIVE_DECISION\].*allocated=.*strategic=.*approach=.*reason=') { $failures.Add('Missing production decision trace') }
}
$other = [regex]::Matches($log, '\([EF]\):').Count
Write-Output "Full-log error lines retained: $other"
if ($failures.Count) { $failures; exit 1 }
Write-Output 'Adaptive commander full stopped log: PASS'
exit 0
