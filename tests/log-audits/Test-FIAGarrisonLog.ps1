param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [Parameter(Mandatory = $true)][ValidateRange(0, 2)][int]$Difficulty
)
$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw
$failures = @()
$count = $Difficulty * 5
if ($log -notmatch 'Game destroyed\.') { $failures += 'STOPPED_LOG_REQUIRED' }
if ($log -notmatch '\[ROSTER_READY\]') { $failures += 'ROSTER_READY' }
if ($log -match 'SCRIPT\s*\((E|F)\)|ENGINE\s*\(F\)|NULL pointer|Null pointer|Unhandled exception') { $failures += 'SCRIPT_OR_FATAL_ERROR' }
$plans = [regex]::Matches($log, '\[FIA_GARRISON_PLAN\][^\r\n]*difficulty=(\d+) bases=(\d+) vehicles=(\d+) per_base=(\d+)')
if ($plans.Count -ne 1 -or [int]$plans[0].Groups[1].Value -ne $Difficulty -or
    [int]$plans[0].Groups[2].Value -ne 5 -or [int]$plans[0].Groups[3].Value -ne $count -or
    [int]$plans[0].Groups[4].Value -ne $Difficulty) { $failures += 'PLAN' }
$ready = [regex]::Matches($log, '\[FIA_GARRISON_READY\][^\r\n]*numeric_slot=(\d+)[^\r\n]*tank=(\d+) crew=(\d+) seats=(\d+)')
if ($ready.Count -ne $count -or @($ready | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique).Count -ne $count) { $failures += 'READY_COUNT' }
foreach ($entry in $ready) {
    $expectedCrew = 10
    if ($entry.Groups[2].Value -eq '1') { $expectedCrew = 3 }
    if ([int]$entry.Groups[3].Value -ne $expectedCrew -or [int]$entry.Groups[4].Value -ne $expectedCrew) { $failures += 'FULL_CREW' }
}
if (@($ready | Where-Object { $_.Groups[2].Value -eq '1' }).Count -ne ([Math]::Max(0, $Difficulty - 1) * 5)) { $failures += 'TANK_COUNT' }
if ([regex]::Matches($log, '\[GARRISON_PROBE\] case=PATROL_MOVED_\d+ passed=1').Count -ne $count) { $failures += 'PHYSICAL_PATROL' }
if ([regex]::Matches($log, '\[GARRISON_PROBE\] case=LOCAL_RADIUS_\d+ passed=1').Count -ne $count) { $failures += 'LOCAL_RADIUS' }
$finished = [regex]::Matches($log, '\[GARRISON_PROBE_FINISHED\] checks=(\d+) failures=(\d+)')
$expectedChecks = 2 + $count * 5
if ($count) { $expectedChecks += 3 }
if ($finished.Count -ne 1 -or [int]$finished[0].Groups[1].Value -ne $expectedChecks -or
    [int]$finished[0].Groups[2].Value -ne 0) { $failures += 'PROBE_FINISHED' }
if ($log -match '\[GARRISON_PROBE\][^\r\n]*passed=0') { $failures += 'PROBE_CASE' }
$resourceErrors = [regex]::Matches($log, '(?m)^.*\(E\).*$').Count
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
"PASS FIA garrison difficulty=$Difficulty vehicles=$count checks=$expectedChecks other_error_lines=$resourceErrors; combat/client/JIP=NOT_RUN"
