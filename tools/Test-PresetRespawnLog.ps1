param(
    [Parameter(Mandatory = $true)][string]$ServerLogPath,
    [Parameter(Mandatory = $true)][string]$ClientLogPath
)
$ErrorActionPreference = 'Stop'
$logs = @([IO.File]::ReadAllText((Resolve-Path $ServerLogPath)), [IO.File]::ReadAllText((Resolve-Path $ClientLogPath)))
$required = @(
    @('real_armory_registered_at_base','preset_saved_with_equipment','recruit_in_native_slave','server_controls_recruit','preset_retained_after_takeover','new_character_at_base','saved_preset_selected_on_server','saved_inventory_prefabs_and_counts_restored','preset_retained_after_base_respawn'),
    @('overlapping_base_query_serialized','owner_controls_same_recruit','overlap_exercised','owner_controls_new_base_character','owner_saved_loadout_selected')
)
$failures = @()
for ($i = 0; $i -lt 2; $i++) {
    foreach ($check in $required[$i]) {
        if ($logs[$i] -notmatch ('\[PRESET_RESPAWN_PROBE\].*check=' + [regex]::Escape($check) + ' passed=1')) { $failures += "log${i}:missing:$check" }
    }
    if ($logs[$i] -match '\[PRESET_RESPAWN_PROBE\].*passed=0') { $failures += "log${i}:failed_assertion" }
    if ($logs[$i] -notmatch '\[PRESET_RESPAWN_PROBE\] finished=1 server=\d phase=[45] failures=0') { $failures += "log${i}:incomplete" }
    if ($logs[$i] -notmatch 'Game destroyed\.') { $failures += "log${i}:not_stopped" }
    $errors = [regex]::Matches($logs[$i], '(?m)^.*(?:\(E\)|\(F\)|Virtual Machine Exception|NULL pointer).*$')
    Write-Output "log=$i full_log_error_lines=$($errors.Count)"
    if ($logs[$i] -match 'Virtual Machine Exception|NULL pointer|ENGINE\s+\(F\)|SCRIPT\s+\(F\)') { $failures += "log${i}:fatal_or_vm_error" }
}
if ($failures.Count) { Write-Output "Preset respawn functional gate: FAIL; $($failures -join ', ')"; exit 1 }
Write-Output 'Preset respawn functional gate: PASS; 14 required assertions; inspect full engine/resource diagnostics separately; visual=NOT_RUN'
