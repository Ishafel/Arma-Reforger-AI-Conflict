param(
    [Parameter(Mandatory)][string]$LogPath,
    [Parameter(Mandatory)][string]$ManifestPath,
    [Parameter(Mandatory)][string]$NativeExitPath,
    [string]$RepositoryRoot
)
$ErrorActionPreference = 'Stop'
if (!$RepositoryRoot) { $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }
$log = [IO.File]::ReadAllText($LogPath)
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$failures = @()
if ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0') { $failures += 'NATIVE_EXIT' }
if ($log -notmatch 'Game destroyed\.') { $failures += 'STOPPED_LOG' }
$texts = @($log)
foreach ($file in Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log') { $texts += [IO.File]::ReadAllText($file.FullName) }
$full = $texts -join "`n"
if ($full -match 'SCRIPT\s*\([EF]\)|ENGINE\s*\(F\)|SCRIPT EXCEPTION|NULL pointer|VM exception|Unhandled exception|Assertion failed|Access violation') { $failures += 'SCRIPT_VM_FATAL' }
if ($full -match 'Application hangs') { $failures += 'NATIVE_HANG' }
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
$selectedVariant = 'Stock'
$selectedMode = 0
$selectedDifficulty = 0
$crossVariant = '-aicfColdVariant' -in $argv
foreach ($flag in @('-aicfColdVariant','-aicfColdMode','-aicfColdDifficulty')) {
    $indices = @(for ($i = 0; $i -lt $argv.Count; $i++) { if ($argv[$i] -ceq $flag) { $i } })
    if (!$indices.Count) { continue }
    if ($indices.Count -ne 1 -or $indices[0] + 1 -ge $argv.Count) { $failures += 'CLI_SELECTION'; continue }
    $value = $argv[$indices[0] + 1]
    if ($flag -eq '-aicfColdVariant') { $selectedVariant = $value; continue }
    if ($value -cnotmatch '^[0-2]$') { $failures += 'CLI_SELECTION'; continue }
    if ($flag -eq '-aicfColdMode') { $selectedMode = [int]$value }
    else { $selectedDifficulty = [int]$value }
}
if ($crossVariant -and !$cancel) { $expected += @('CONTENT_READY','TARGET_WORLD','TARGET_GRAPH'); $doneChecks += 2 }
if ('-aicfColdRetryHost' -in $argv) {
    $expected += @('RETRY_CANCEL_CONTEXT','RETRY_RETURN')
    if ('-aicfColdHostProbe' -notin $argv -or '-aicfWarmBootstrapControl' -in $argv -or $cancel -or @([regex]::Matches($log, '\[SCENARIO_BOOTSTRAP\] phase=CANCEL\b')).Count -ne 1) { $failures += 'RETRY_FLOW' }
}
$cases = @([regex]::Matches($log, '\[COLD_CASE\] case=(\w+) pass=([01])'))
if ($cases.Count -ne $expected.Count) { $failures += 'CASE_COUNT' }
foreach ($name in $expected) {
    $found = @($cases | Where-Object { $_.Groups[1].Value -ceq $name })
    if ($found.Count -ne 1 -or $found[0].Groups[2].Value -cne '1') { $failures += "CASE_$name" }
}
foreach ($phase in @('OPEN')) {
    $phaseCount = 1
    if ('-aicfWarmBootstrapControl' -in $argv) { $phaseCount = 0 }
    if (@([regex]::Matches($log, '\[SCENARIO_BOOTSTRAP\] phase=' + $phase + '\b')).Count -ne $phaseCount) { $failures += "PHASE_$phase" }
}
if ($log -match '\[COLD_TIMEOUT\]|\[SCENARIO_BOOTSTRAP\]\[FAIL\]' -or @([regex]::Matches($log, "\[COLD_DONE\] checks=$doneChecks failed=0\b")).Count -ne 1) { $failures += 'COMPLETION' }
$handoffPhases = @([regex]::Matches($log, '\[HOST_HANDOFF\] phase=(\w+)') | ForEach-Object { $_.Groups[1].Value })
if ('-aicfExpectHostHandoff' -in $argv -or $handoffPhases.Count) {
    if (($handoffPhases -join ',') -cne 'RETURN,HOST,LEAF,READY') { $failures += 'HOST_HANDOFF_PHASES' }
}
if ($full -match '\[HOST_HANDOFF\]\[FAIL\]') { $failures += 'HOST_HANDOFF_FAILED' }
if ('-aicfHostIntentContracts' -in $argv) {
    $intentExpected = @('ROUNDTRIP','INVALID_JSON','SCHEMA','PHASE','TTL','MAP','CONTENT','MODE','DIFFICULTY','MINUTES','WEATHER','ENTRY','GRAPH','HOST_ONLY')
    $intentCases = @([regex]::Matches($log, '\[HOST_INTENT_CASE\] case=(\w+) pass=([01])'))
    if ($intentCases.Count -ne $intentExpected.Count) { $failures += 'INTENT_CASE_COUNT' }
    foreach ($name in $intentExpected) {
        $found = @($intentCases | Where-Object { $_.Groups[1].Value -ceq $name })
        if ($found.Count -ne 1 -or $found[0].Groups[2].Value -cne '1') { $failures += "INTENT_$name" }
    }
}
if ('-aicfColdReadyProbe' -in $argv) {
    if (@([regex]::Matches($log, '\[COLD_READY\] listen=1 address=\S+ pending=0\b')).Count -ne 1 -or $log -notmatch '\[ROSTER_READY\]') { $failures += 'HOST_ROSTER_READY' }
    $listeners = @([regex]::Matches($log, 'Starting RPL server, listening on address ([^,\r\n]+)'))
    if (!$listeners.Count -or $listeners[-1].Groups[1].Value -cne '127.0.0.1:24073') { $failures += 'HOST_LISTENER' }
    if ($log -notmatch '\[ENVIRONMENT_APPLIED\][^\r\n]*time_minutes=577[^\r\n]*weather_mode=4') { $failures += 'HOST_ENVIRONMENT_APPLIED' }
}
$launch = [regex]::Match($log, '\[AICF\]\[SCENARIO_LAUNCH\]')
if ($cancel) {
    if ($launch.Success -or $log -match '\[BOOTSTRAP_SERVER\]|\[ROSTER_SPAWN_REQUESTED\]') { $failures += 'CANCEL_STARTED_MATCH' }
    if ($log -notmatch '\[SCENARIO_BOOTSTRAP\] phase=CANCEL') { $failures += 'CANCEL_TRANSITION' }
}
elseif (-not $launch.Success) { $failures += 'SELECTED_LAUNCH' }
elseif ($log.Substring(0, $launch.Index) -match '\[BOOTSTRAP_SERVER\]|\[ROSTER_SPAWN_REQUESTED\]|\[SPAWN_ATTEMPT_STARTED\]') { $failures += 'INTERMEDIATE_MATCH' }
if (!$cancel) {
    . (Join-Path $RepositoryRoot 'tools/AICF_Scenarios.ps1')
    $catalog = Get-AICFScenarioCatalog -Path (Join-Path $RepositoryRoot 'tools/config/AICF_Scenarios.json')
    try { $plan = Resolve-AICFScenario -Catalog $catalog -Variant $selectedVariant -MatchMode @('Standard','CoopUS','CoopUSSR')[$selectedMode] -Difficulty @('Easy','Medium','Hard')[$selectedDifficulty] }
    catch { $failures += 'CLI_SELECTION'; $plan = $null }
    if ($plan) {
        $launches = @([regex]::Matches($log, '\[SCENARIO_LAUNCH\] variant=(\S+) mode=(\d+) difficulty=(\d+) resource=(\S+) addons=(\S+)'))
        if ($launches.Count -ne 1 -or $launches[0].Groups[1].Value -cne $plan.baseVariant -or
            $launches[0].Groups[2].Value -cne [string]$selectedMode -or $launches[0].Groups[3].Value -cne [string]$selectedDifficulty -or
            $launches[0].Groups[4].Value -cne $plan.scenario -or $launches[0].Groups[5].Value -cne $plan.addonIds) { $failures += 'LAUNCH_PLAN' }
        if ($crossVariant) {
            $loaded = @([regex]::Matches($log, '\[COLD_ADDON\] id=([A-F0-9]{16}) vanilla=0\b') | ForEach-Object { $_.Groups[1].Value })
            $wanted = @($plan.addonIds -split ',')
            if ($loaded.Count -ne $wanted.Count -or @(Compare-Object $loaded $wanted).Count) { $failures += 'TARGET_GRAPH' }
            $worlds = @([regex]::Matches($log, '\[COLD_WORLD\] actual=([^\r\n]+) expected=([^\r\n]+)'))
            if ($worlds.Count -ne 1) { $failures += 'TARGET_WORLD' }
            else {
                $actual = $worlds[0].Groups[1].Value.Replace('\','/').ToLowerInvariant()
                $wantedWorld = $worlds[0].Groups[2].Value.Replace('\','/').ToLowerInvariant()
                if (!$wantedWorld -or ($actual -cne $wantedWorld -and !$actual.EndsWith('/' + $wantedWorld) -and !$actual.EndsWith(':' + $wantedWorld))) { $failures += 'TARGET_WORLD' }
            }
        }
    }
}
if ($failures.Count) { $failures | Select-Object -Unique; exit 1 }
$flow = 'native entry -> setup -> selected leaf'
if ($cancel) { $flow = 'native entry -> setup -> cancel -> main menu' }
"Workshop bootstrap full stopped log: PASS; cases=$($expected.Count); $flow; no intermediate AICF match"
exit 0
