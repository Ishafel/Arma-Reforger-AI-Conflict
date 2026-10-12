param([Parameter(Mandatory=$true)][string]$LogPath, [switch]$PlanOnly)
$ErrorActionPreference = 'Stop'
$log = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath))
$failures = @()
if ($log -notmatch 'Game destroyed\.') { $failures += 'STOPPED_LOG_REQUIRED' }
$finished = [regex]::Matches($log, '\[FIA_RANDOM_PROBE_FINISHED\] checks=(\d+) failures=(\d+)')
if ($finished.Count -ne 1 -or [int]$finished[0].Groups[2].Value -ne 0 -or [int]$finished[0].Groups[1].Value -lt 13) { $failures += 'PROBE_FINISHED' }
if ($log -match '\[FIA_RANDOM_PROBE\][^\r\n]*passed=0') { $failures += 'PROBE_CASE' }
$plans = [regex]::Matches($log, '\[FIA_GARRISON_PLAN\][^\r\n]*difficulty=(\d+) bases=(\d+) vehicles=(\d+)')
if ($plans.Count -ne 1) { $failures += 'ONE_PLAN_REQUIRED' }
if ($log -notmatch '\[ROSTER_READY\]') { $failures += 'ROSTER_READY' }
if (-not $PlanOnly -and $plans.Count -eq 1) {
    $count = [int]$plans[0].Groups[3].Value
    $ready = [regex]::Matches($log, '\[FIA_GARRISON_READY\][^\r\n]*numeric_slot=(\d+)')
    if ($ready.Count -ne $count -or @($ready | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique).Count -ne $count) { $failures += 'READY_COUNT' }
}
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" } }
else { 'PASS FIA random garrison targeted probe; client/JIP/combat/soak=NOT_RUN' }
$errors = [regex]::Matches($log, '(?m)^.*(?:SCRIPT\s*\([EF]\)|ENGINE\s*\(F\)|[Nn]ull pointer|Unhandled exception).*$')
if ($errors.Count) { "FAIL FULL_LOG_GATE script_or_fatal_lines=$($errors.Count)"; exit 1 }
if ($failures.Count) { exit 1 }
'PASS FULL_LOG_GATE'
