param([Parameter(Mandatory)][string]$LogPath)
$ErrorActionPreference = 'Stop'
$text = Get-Content -LiteralPath $LogPath -Raw
$failures = [Collections.Generic.List[string]]::new()
$required = @('PLAYER_49_BLOCKED','PLAYER_50_BLOCKED','PLAYER_DESTINATION_BLOCKED','PLAYER_HEIGHT_BLOCKED','PLAYER_51_CLEAR','PLAYER_VETO_NO_VEHICLE_MUTATION','ROUTE_TELEPORT_DISTANCE','HOME_BOUND','ENTITY_IDS_UNCHANGED','DESANT_SEVEN_OUT','ESSENTIAL_THREE_REMAIN','ROSTER_IDENTITY_UNCHANGED','DESANT_NOT_REBOARDED','PATROL_RESUMED_AFTER_DESANT','STALLED_ROUTE_RECOVERY_TRIGGER','STALE_MEMBER_BLOCKS_TELEPORT','DEAD_DRIVER_NOT_RECOVERED','NO_REPLACEMENT')
foreach ($slot in 10000..10009) { $required += "ESSENTIAL_THREE_$slot" }
foreach ($pair in @(@(10000,0),@(10000,8),@(10000,9),@(10001,0),@(10001,1),@(10001,2))) {
 foreach ($prefix in @('PLAYER_VETO_CREW','EXACT_SEAT_RETURN','VEHICLE_ADVANCE_AFTER_RETURN')) { $required += "${prefix}_$($pair[0])_$($pair[1])" }
}
foreach ($case in $required) {
 if ($text -notmatch ('\[RECOVERY_PROBE\] case=' + [regex]::Escape($case) + ' passed=1\b')) { $failures.Add("MISSING_$case") }
}
if ($text -notmatch '\[RECOVERY_PROBE_FINISHED\] checks=\d+ failures=0\b' -or $text -match '\[RECOVERY_PROBE\].*passed=0\b') { $failures.Add('PROBE_INCOMPLETE_OR_FAILED') }
if ($text -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception') { $failures.Add('FULL_LOG_SCRIPT_OR_FATAL') }
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
"FIA recovery log: PASS; cases=$($required.Count); synthetic_player_veto=1; connected_player_and_JIP=NOT_RUN"
