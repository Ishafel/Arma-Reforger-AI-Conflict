param(
    [Parameter(Mandatory = $true)]
    [string[]]$LogPath,
    [switch]$Baseline,
    [switch]$RequireVariation
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$selected = [System.Collections.Generic.List[string]]::new()
$rules = @('CONFIGURED_TIME', 'DAY_DURATION_CONFIG', 'AUTO_ADVANCE', 'DATE_UNCHANGED',
    'HEADER_WEATHER', 'START_REQUESTS', 'WEATHER_PROGRESS_ENABLED', 'CLOCK_RUNNING',
    'HEADER_INHERITANCE', 'DUPLICATE_START', 'DAY_DURATION_STABLE', 'CLOCK_PROGRESSION')
$mode = 'baseline'
$requests = 0
if (-not $Baseline) {
    $mode = 'random'
    $requests = 1
    $rules += 'APPLIED_STATE'
}
foreach ($path in $LogPath) {
    $text = Get-Content -LiteralPath $path -Raw
    $checks = [regex]::Matches($text, '\[WEATHER_CHECK\] rule=(\w+) passed=([01])')
    if ($checks.Count -ne $rules.Count) { $failures.Add("${path}: unexpected check count $($checks.Count)") }
    foreach ($rule in $rules) {
        $matchesForRule = @($checks | Where-Object { $_.Groups[1].Value -eq $rule })
        if ($matchesForRule.Count -ne 1 -or $matchesForRule[0].Groups[2].Value -ne '1') {
            $failures.Add("${path}: missing, duplicate or failed rule $rule")
        }
    }
    $done = [regex]::Matches($text, '\[WEATHER_DONE\] mode=(\w+) checks=(\d+) failures=(\d+) requests=(\d+) weather=([^\r\n]*)')
    if ($done.Count -ne 1 -or $done[0].Groups[1].Value -ne $mode -or
        [int]$done[0].Groups[2].Value -ne $rules.Count -or [int]$done[0].Groups[3].Value -ne 0 -or
        [int]$done[0].Groups[4].Value -ne $requests) {
        $failures.Add("${path}: invalid WEATHER_DONE")
    }
    $selections = [regex]::Matches($text, '\[WEATHER_SELECTED\].*?weather=(\S+) candidates=(\d+) looping=([01]) state_hours=([\d.]+)')
    if ($selections.Count -ne $requests) { $failures.Add("${path}: wrong selection count") }
    if (-not $Baseline -and $selections.Count -eq 1 -and $done.Count -eq 1) {
        $weather = $selections[0].Groups[1].Value
        if ($weather -ne $done[0].Groups[5].Value.Trim() -or [int]$selections[0].Groups[2].Value -lt 2) {
            $failures.Add("${path}: invalid selected state")
        }
        $selected.Add($weather)
    }
    foreach ($marker in @('\[ROSTER_READY\]', 'Game destroyed\.', '\[WEATHER_CLOCK\]', '\[WEATHER_PROGRESS\]')) {
        if ($text -notmatch $marker) { $failures.Add("${path}: missing $marker") }
    }
    if ($text -notmatch '\[WEATHER_HEADER\].*passed=1' -or $text -match '\[WEATHER_HEADER\].*passed=0') {
        $failures.Add("${path}: invalid header inheritance")
    }
    if ($text -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|Unhandled exception|\[WEATHER_UNAVAILABLE\]') {
        $failures.Add("${path}: script/fatal error or weather unavailable")
    }
    $otherErrors = [regex]::Matches($text, '(?m)^.*\([EF]\).*$').Count
    Write-Output "Weather log: $path; checks=$($checks.Count); all_EF_lines=$otherErrors"
}
if ($RequireVariation -and @($selected | Sort-Object -Unique).Count -lt 2) {
    $failures.Add('Repeated new matches did not demonstrate different weather states')
}
if ($failures.Count) {
    $failures | ForEach-Object { Write-Output "FAIL $_" }
    exit 1
}
Write-Output "PASS Match weather: logs=$($LogPath.Count) checks_per_log=$($rules.Count) selected=$($selected -join ',')"
exit 0
