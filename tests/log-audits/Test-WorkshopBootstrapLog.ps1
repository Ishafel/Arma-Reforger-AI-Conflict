param(
    [Parameter(Mandatory)][string]$LogPath,
    [Parameter(Mandatory)][string]$ManifestPath,
    [Parameter(Mandatory)][string]$NativeExitPath
)
$ErrorActionPreference = 'Stop'
$log = [IO.File]::ReadAllText($LogPath)
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$failures = @()
if ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0') { $failures += 'NATIVE_EXIT' }
if ($log -notmatch 'Game destroyed\.') { $failures += 'STOPPED_LOG' }
$texts = @($log)
foreach ($file in Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log') { $texts += [IO.File]::ReadAllText($file.FullName) }
$full = $texts -join "`n"
if ($full -match 'SCRIPT\s*\([EF]\)|ENGINE\s*\(F\)|SCRIPT EXCEPTION|NULL pointer|VM exception|Unhandled exception|Assertion failed|Access violation') { $failures += 'SCRIPT_VM_FATAL' }
$argv = @($manifest.arguments)
if (@($argv | Where-Object { $_ -ceq '-aicfColdBootstrapProbe' }).Count -ne 1) { $failures += 'PROBE_CLI' }
$cli = @([regex]::Matches($log, 'CLI Params: ([^\r\n]+)'))
if (-not $cli.Count) { $failures += 'CLI_MISSING' }
foreach ($entry in $cli) {
    foreach ($fragment in @('-aicfColdBootstrapProbe', ('-profile ' + $manifest.profile), ('-gproj ' + $argv[[array]::IndexOf($argv, '-gproj') + 1]))) {
        if (-not $entry.Groups[1].Value.Contains($fragment)) { $failures += 'CLI_MISMATCH' }
    }
}
$cancel = '-aicfColdCancelProbe' -in $argv
$expected = @('LITERAL_NAME','FRONTEND_GRAPH','NATIVE_ENTRY','SETUP_OPEN','BOOTSTRAP_CONTEXT')
$doneChecks = 3
if ($cancel) { $expected += @('CANCEL_RETURN','CANCEL_CLOSED'); $doneChecks = 2 }
else { $expected += @('DEFAULTS','SELECT','SELECT_ENVIRONMENT','CAN_START','LAUNCH','SELECTED_HEADER','NO_BOOTSTRAP','APPLIED_ENVIRONMENT') }
if ('-aicfColdHostProbe' -in $argv -and !$cancel) { $expected += @('HOST_REQUEST','HOST_CONTEXT','HOST_LISTEN'); $doneChecks = 4 }
$cases = @([regex]::Matches($log, '\[COLD_CASE\] case=(\w+) pass=([01])'))
if ($cases.Count -ne $expected.Count) { $failures += 'CASE_COUNT' }
foreach ($name in $expected) {
    $found = @($cases | Where-Object { $_.Groups[1].Value -ceq $name })
    if ($found.Count -ne 1 -or $found[0].Groups[2].Value -cne '1') { $failures += "CASE_$name" }
}
foreach ($phase in @('OPEN')) {
    if (@([regex]::Matches($log, '\[SCENARIO_BOOTSTRAP\] phase=' + $phase + '\b')).Count -ne 1) { $failures += "PHASE_$phase" }
}
if ($log -match '\[COLD_TIMEOUT\]|\[SCENARIO_BOOTSTRAP\]\[FAIL\]' -or $log -notmatch "\[COLD_DONE\] checks=$doneChecks failed=0") { $failures += 'COMPLETION' }
$launch = [regex]::Match($log, '\[AICF\]\[SCENARIO_LAUNCH\]')
if ($cancel) {
    if ($launch.Success -or $log -match '\[BOOTSTRAP_SERVER\]|\[ROSTER_SPAWN_REQUESTED\]') { $failures += 'CANCEL_STARTED_MATCH' }
    if ($log -notmatch '\[SCENARIO_BOOTSTRAP\] phase=CANCEL') { $failures += 'CANCEL_TRANSITION' }
}
elseif (-not $launch.Success) { $failures += 'SELECTED_LAUNCH' }
elseif ($log.Substring(0, $launch.Index) -match '\[BOOTSTRAP_SERVER\]|\[ROSTER_SPAWN_REQUESTED\]|\[SPAWN_ATTEMPT_STARTED\]') { $failures += 'INTERMEDIATE_MATCH' }
if ($failures.Count) { $failures | Select-Object -Unique; exit 1 }
$flow = 'native entry -> setup -> selected leaf'
if ($cancel) { $flow = 'native entry -> setup -> cancel -> main menu' }
"Workshop bootstrap full stopped log: PASS; cases=$($expected.Count); $flow; no intermediate AICF match"
exit 0
