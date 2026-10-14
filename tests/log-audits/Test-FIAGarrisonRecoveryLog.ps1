param([Parameter(Mandatory)][string]$LogPath, [ValidateSet('Historical','damage','identity')][string]$DamageMode = 'Historical')
$ErrorActionPreference = 'Stop'
$text = Get-Content -LiteralPath $LogPath -Raw
$failures = [Collections.Generic.List[string]]::new()
if ($DamageMode -ne 'Historical') {
    $required = @()
    if ($DamageMode -eq 'damage') {
        foreach ($asset in @('TANK_HULL','APC_HULL','TANK_COMPONENT','APC_COMPONENT')) {
            foreach ($outcome in @('ACTUAL_DAMAGE','DRIVE_ALLOWED','BLOCKED','REPAIR_RETRY_BANNED')) { $required += "${asset}_$outcome" }
        }
        $required += @('CREW_WOUND_ACTUAL','CREW_WOUND_BLOCKED','CREW_HEAL_BANNED','PLANNED_DISMOUNT_ALLOWED','PRISTINE_DEPLOYED_RECOVERY')
        foreach ($role in 0..3) { $required += "DEATH_${role}_BLOCKED" }
        if ([regex]::Matches($text, '\[DAMAGE_PROBE_DAMAGE\].*before=\S+ after=\S+.*alive=1').Count -lt 5) { $failures.Add('REAL_DAMAGE_EVIDENCE') }
    } else {
        $required = @('PLAYER_VETO','PRISTINE_RECOVERY','DELETED_MEMBER','STALE_MEMBER','STALE_VEHICLE','UNKNOWN_DAMAGE','UNKNOWN_ROSTER','STALE_GENERATION','FINAL_TRANSFORM_GATE','FINAL_CREW_GATE','REPLACED_MEMBER','NO_GEOMETRY_CANDIDATE','RADIUS_49_50_51')
    }
    foreach ($case in $required) {
        if ($text -notmatch ('\[DAMAGE_RECOVERY_PROBE\] mode=' + $DamageMode + ' case=' + [regex]::Escape($case) + ' passed=1\b')) { $failures.Add("MISSING_$case") }
    }
    if ([regex]::Matches($text, '\[DAMAGE_RECOVERY_PROBE\].*case=PRISTINE_\d+ passed=1\b').Count -lt 11) { $failures.Add('ORIGINAL_PRISTINE_ROSTERS') }
    if ($text -notmatch ('\[DAMAGE_RECOVERY_PROBE_FINISHED\] mode=' + $DamageMode + ' checks=\d+ failures=0\b') -or $text -match '\[DAMAGE_RECOVERY_PROBE\].*passed=0\b') { $failures.Add('PROBE_INCOMPLETE_OR_FAILED') }
    $banned = @{}
    foreach ($line in ($text -split "`r?`n")) {
        if ($line -notmatch '\[(FIA_GARRISON_RECOVERY_BANNED|FIA_GARRISON_VEHICLE_TELEPORTED|FIA_GARRISON_CREW_TELEPORT)\].*numeric_slot=(\d+) generation=(\d+).*vehicle=(0x[0-9A-Fa-f]+)') { continue }
        $eventName = $Matches[1]
        $identity = "$($Matches[2])/$($Matches[3])/$($Matches[4])"
        if ($eventName -eq 'FIA_GARRISON_RECOVERY_BANNED') {
            if ($line -notmatch 'reason=\S+ member=\S+.*permanent=1') { $failures.Add('BAN_DIAGNOSTICS') }
            $banned[$identity] = $true
        } elseif ($banned.ContainsKey($identity)) { $failures.Add("TELEPORT_AFTER_BAN_$identity") }
    }
    if ($banned.Count -lt 8) { $failures.Add('BAN_IDENTITIES') }
    if ($text -notmatch 'Game destroyed\.') { $failures.Add('SERVER_NOT_STOPPED') }
    if ($text -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|Unable to initialize the game') { $failures.Add('FULL_LOG_SCRIPT_OR_FATAL') }
    if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
    "FIA damage recovery log: PASS; mode=$DamageMode; cases=$($required.Count); banned_identities=$($banned.Count); connected_player_and_JIP=NOT_RUN"
    exit 0
}
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
