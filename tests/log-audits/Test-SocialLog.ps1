param(
    [Parameter(Mandatory)][string]$LogPath,
    [string]$ClientLogPath,
    [string]$JIPLogPath,
    [switch]$Policy,
    [switch]$GreetingPriority,
    [switch]$ExitGuard,
    [switch]$ClientExit
)
$ErrorActionPreference = 'Stop'
if ($ClientExit -and !$ClientLogPath) { throw '-ClientExit requires -ClientLogPath' }
function Read-Stopped([string]$Path) {
    $text = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $Path))
    if ($text -notmatch 'Game destroyed\.') { throw "Incomplete log: $Path" }
    if ($text -match 'ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Can.t compile') { throw "Fatal error: $Path" }
    # Native resource diagnostics are listed, never silently promoted to a clean engine gate.
    $scriptErrors = @($text -split '\r?\n' | Where-Object { $_ -match 'SCRIPT\s+\((E|F)\)' })
    foreach ($line in $scriptErrors) {
        if ($line -notmatch 'SCR_SupportStationAreaMeshComponent.*linked support station.*has an offset') { throw "Script error: $line" }
    }
    if ($scriptErrors.Count) { Write-Warning "Known native support-area diagnostics: $($scriptErrors.Count); full engine log is not clean" }
    return $text
}
$log = Read-Stopped $LogPath
if ($log -notmatch '\[ROSTER_READY\]' -or $log -match '\[SOCIAL_PROBE\] case=\S+ pass=0') { throw 'Roster missing or fixture failure' }
$cases = @('FIRST_APPROACH','CONCURRENT_RESERVATION','COMMIT','COOLDOWN_29999','STAY_NO_REPEAT','COOLDOWN_30000',
    'INDEPENDENT_PLAYERS','STALE_RELEASE','FAILED_BEFORE_START','RESPAWN_SAME_LEDGER','TOKEN_MONOTONIC',
    'OUTSIDE_RADIUS','INNER_BOUNDARY','OUTER_BOUNDARY_NO_REARM','OUTER_CROSS_REARMS','FAILED_COMMIT','SPREAD','READINESS',
    'US_INITIAL_WAIT','USSR_INITIAL_WAIT')
0..3 | ForEach-Object { $cases += "NATIVE_VARIANT_$_"; $cases += "NO_RESTART_$_" }
if ($ExitGuard) { 0..3 | ForEach-Object { $cases += "OUT_TRANSITION_$_" } }
if ($GreetingPriority) { $cases += 'SALUTE_ORDER_CANCEL' }
if ($ClientLogPath) {
    $cases += @('LIVE_SALUTE_FREE','GREETING_BOUNDED_4','LIVE_PLAYER','RECONNECT_STABLE','REUSED_NUMERIC_ID','LATE_BACKEND_IDENTITY','REAL_RESPAWN_COOLDOWN')
    0..3 | ForEach-Object { $cases += "VOICE_ONLY_$_"; $cases += "IDLE_CONTINUES_$_"; $cases += "GREETING_BOUNDED_$_" }
    if ($ClientExit) {
        0..3 | ForEach-Object { $cases += "IDLE_ORDER_CANCEL_$_" }
        $cases += 'EARLY_PUSHUPS_ORDER_CANCEL'
    }
}
if ($Policy) {
    $cases += @('POLICY_NEAREST_ALLY','POLICY_ENEMY','POLICY_STABLE_TIE','TWO_BOT_RESERVATION','TWO_BOT_SHARED_COOLDOWN',
        'POLICY_BUSY','POLICY_STALE_GENERATION','POLICY_OUTSIDE','MUSTER_END_CANCEL','MOVEMENT_CANCEL','THREAT_CANCEL',
        'DEATH_REJECTED','REPLACEMENT_STALE','STOP_CLEANUP')
}
foreach ($case in $cases) {
    if ([regex]::Matches($log, "\[SOCIAL_PROBE\] case=$case pass=1\b").Count -ne 1) { throw "Missing/duplicate case: $case" }
}
if ($log -notmatch "\[SOCIAL_PROBE\] finished=1 passed=\d+ failed=0") { throw 'Incomplete matrix' }
if ($ClientLogPath) {
    $client = Read-Stopped $ClientLogPath
    if ($client -notmatch '\[SOCIAL_RESOURCE\] project=1 us=\S+ ussr=\S+ mapped=1') { throw 'Resource/faction mapping unresolved' }
    if ($client -notmatch '\[SOCIAL_CLIENT\] finished=1 jip=0 audio=5 duplicates=5') { throw 'Incomplete client matrix' }
    if ($ClientExit) {
        if ($client -match '\[SOCIAL_EXIT\][^\r\n]*pass=0') { throw 'Client loiter animation survived cancellation' }
        0..3 | ForEach-Object {
            if ($client -notmatch "\[SOCIAL_EXIT\] variant=$_ seen=1 tag=0 command=0 pass=1") { throw "Missing observed client exit: $_" }
            if ($log -notmatch "\[SOCIAL_PROBE\] case=IDLE_ORDER_CANCEL_$_ pass=1") { throw "Missing order cancellation: $_" }
        }
        if ($log -notmatch '\[SOCIAL_PROBE\] case=EARLY_PUSHUPS_ORDER_CANCEL pass=1' -or
            $client -notmatch '\[SOCIAL_EXIT\] variant=4 seen=1 tag=0 command=0 pass=1') { throw 'Missing early pushups cancellation' }
    }
    $starts = [regex]::Matches($log, '\[GREETING_STARTED\][^\r\n]*token=(\d+)[^\r\n]*now_ms=(\d+)')
    if ($starts.Count -ne 5) { throw "Wrong greeting count: $($starts.Count)" }
    $previous = -30000
    $seen = @{}
    foreach ($start in $starts) {
        $token = $start.Groups[1].Value
        $now = [int]$start.Groups[2].Value
        if ($seen.ContainsKey($token) -or $now - $previous -lt 30000) { throw 'Duplicate/early greeting' }
        $seen[$token] = $true; $previous = $now
        if ([regex]::Matches($client, "\[GREETING_AUDIO\] token=$token\b[^\r\n]*started=1").Count -ne 1) { throw "Audio not delivered exactly once: $token" }
        if ($client -notmatch "\[SOCIAL_CLIENT\] token=$token audio_played=1" -or $client -notmatch "\[SOCIAL_CLIENT\] token=$token duplicate_rejected=1") { throw "Missing playback/dedupe: $token" }
        if ($token -eq 5 -and $client -notmatch "\[SOCIAL_CLIENT\] token=$token salute_tag=1") { throw "Missing client SALUTE animation tag: $token" }
    }
    if (!$JIPLogPath) { throw 'JIP log required with client matrix' }
    $jip = Read-Stopped $JIPLogPath
    if ($jip -notmatch '\[SOCIAL_CLIENT\] finished=1 jip=1 audio=0 last_token=0' -or $jip -match '\[GREETING_AUDIO\]') { throw 'JIP replay or incomplete observation' }
}
Write-Output "Social targeted log audit: PASS; cases=$($cases.Count); visual/audio perception=NOT_RUN"
