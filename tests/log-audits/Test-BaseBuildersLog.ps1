param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [int]$MinimumCompleted = 3,
    [switch]$RequireLifecycle,
    [switch]$RequireToolUse,
    [string]$ClientLogPath
)

$ErrorActionPreference = 'Stop'
$lines = Get-Content -LiteralPath $LogPath
$raw = $lines -join "`n"
$failures = [System.Collections.Generic.List[string]]::new()
$active = @{}
$ready = @{}
$homes = @{}
$work = @{}
$completions = @{}
$retired = @{}
$reused = $null
$killed = $null
$ownerChange = $null
$reuseConfirmed = $false
$replacementConfirmed = $false
$ownerRetired = $false
$idleCount = 0
$workerRplIds = @{}

if ($raw -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer') { $failures.Add('BUILDERS_RUNTIME_SCRIPT_ERROR') }
if ($raw -notmatch 'Game destroyed\.') { $failures.Add('BUILDERS_LOG_NOT_STOPPED') }
if ($raw -notmatch '\[ROSTER_READY\]') { $failures.Add('BUILDERS_ARMY_NOT_READY') }

foreach ($line in $lines) {
    if ($line -notmatch '\[(BUILDER_[A-Z_]+)\].*?t_ms=(\d+)\s+base=(\S+).*?generation=(\d+)\s+group=(\S+)') { continue }
    $event = $Matches[1]; $time = [long]$Matches[2]; $base = $Matches[3]; $generation = [int]$Matches[4]; $group = $Matches[5]
    $key = "$base/$generation"
    switch ($event) {
        'BUILDER_SPAWN_REQUESTED' {
            if ($active.ContainsKey($base)) { $failures.Add("BUILDERS_DUPLICATE_BASE:$base") }
            $active[$base] = $group
            if ($killed -and $killed.Base -eq $base -and $generation -gt $killed.Generation) {
                if (-not $retired.ContainsKey($killed.Key) -or $time - $retired[$killed.Key] -lt 60000) { $failures.Add('BUILDERS_REPLACEMENT_TOO_EARLY') }
                else { $replacementConfirmed = $true }
            }
        }
        'BUILDER_READY' {
            if ($active[$base] -ne $group -or $line -notmatch 'agents=1(?:\s|$)') { $failures.Add("BUILDERS_INVALID_READY:$base") }
            $ready[$key] = $true
            if ($line -match 'character_rpl=(\S+)') { $workerRplIds[$Matches[1]] = $true }
        }
        'BUILDER_TARGET_ASSIGNED' {
            $homes.Remove($key)
            if ($reused -and $reused.Key -eq $key -and $reused.Group -eq $group) { $reuseConfirmed = $true }
        }
        'BUILDER_WORK_STARTED' {
            if (-not $ready.ContainsKey($key)) { $failures.Add("BUILDERS_WORK_BEFORE_READY:$base") }
            $work[$key] = $time
        }
        'BUILDER_PROGRESS' {
            if (-not $work.ContainsKey($key) -or $time - $work[$key] -lt 3000) { $failures.Add("BUILDERS_PROGRESS_BEFORE_WORK:$base") }
            if ($RequireToolUse -and $line -notmatch 'tool_active=1 item_using=1') { $failures.Add("BUILDERS_PROGRESS_WITHOUT_ANIMATION:$base") }
        }
        'BUILDER_COMPLETED' {
            if ($line -match 'target=(\S+)') {
                $target = $Matches[1]
                if ($completions.ContainsKey($target)) { $failures.Add("BUILDERS_DUPLICATE_COMPLETION:$target") }
                $completions[$target] = $true
            }
        }
        'BUILDER_HOME' { $homes[$key] = $time }
        'BUILDER_PROBE_REUSE' { $reused = @{ Key=$key; Group=$group } }
        'BUILDER_PROBE_KILL' { $killed = @{ Key=$key; Base=$base; Generation=$generation } }
        'BUILDER_PROBE_OWNER_CHANGE' { $ownerChange = $key }
        'BUILDER_RETIRED' {
            $active.Remove($base)
            $retired[$key] = $time
            if ($line -match 'reason=IDLE_AT_MAIN_TENT') {
                $idleCount++
                if (-not $homes.ContainsKey($key) -or $time - $homes[$key] -lt 30000) { $failures.Add("BUILDERS_IDLE_TOO_EARLY:$base") }
            }
            if ($ownerChange -eq $key -and $line -match 'reason=OWNER_CHANGED_OR_RETIRING') { $ownerRetired = $true }
        }
        'BUILDER_PROBE' {
            if ($line -notmatch 'agents=1(?:\s|$)') { $failures.Add("BUILDERS_INVALID_LIVE_COUNT:$base") }
        }
    }
}
if ($ClientLogPath) {
    $clientLines = Get-Content -LiteralPath $ClientLogPath
    $clientRaw = $clientLines -join "`n"
    if ($clientRaw -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer') { $failures.Add('BUILDERS_CLIENT_SCRIPT_ERROR') }
    if ($clientRaw -notmatch 'Game destroyed\.') { $failures.Add('BUILDERS_CLIENT_LOG_NOT_STOPPED') }
    $firstPositions = @{}
    $clientMoved = $false
    $clientWorked = $false
    foreach ($line in $clientLines) {
        if ($line -notmatch '\[BUILDER_CLIENT_PROBE\].*?rpl=(\S+) position=<([^,]+),([^,]+),([^>]+)> using=(\d) tool_attached=(\d) proxy=(\d)') { continue }
        $id = $Matches[1]
        if (-not $workerRplIds.ContainsKey($id) -or $Matches[7] -ne '1') { continue }
        $x = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
        $z = [double]::Parse($Matches[4], [cultureinfo]::InvariantCulture)
        if ($Matches[5] -eq '1' -and $Matches[6] -eq '1') { $clientWorked = $true }
        if (-not $firstPositions.ContainsKey($id)) { $firstPositions[$id] = @($x, $z) }
        $dx = $x - $firstPositions[$id][0]; $dz = $z - $firstPositions[$id][1]
        if ($dx * $dx + $dz * $dz -ge 4) { $clientMoved = $true }
    }
    if (-not $clientMoved) { $failures.Add('BUILDERS_CLIENT_MOVEMENT_COVERAGE') }
    if (-not $clientWorked) { $failures.Add('BUILDERS_CLIENT_TOOL_ANIMATION_COVERAGE') }
}
if ($completions.Count -lt $MinimumCompleted) { $failures.Add('BUILDERS_COMPLETION_COVERAGE') }
if ($idleCount -lt 1) { $failures.Add('BUILDERS_IDLE_COVERAGE') }
if ($RequireLifecycle -and (-not $reuseConfirmed -or -not $replacementConfirmed -or -not $ownerRetired)) { $failures.Add('BUILDERS_LIFECYCLE_COVERAGE') }
if ($failures.Count) {
    Write-Output "Base builders log audit: FAIL ($($failures.Count) issue(s))"
    $failures | ForEach-Object { Write-Output " - [$_]" }
    exit 1
}
Write-Output "Base builders log audit: PASS completed=$($completions.Count) idle_retirements=$idleCount lifecycle=$([bool]$RequireLifecycle) tool_use=$([bool]$RequireToolUse) client=$([bool]$ClientLogPath)"
