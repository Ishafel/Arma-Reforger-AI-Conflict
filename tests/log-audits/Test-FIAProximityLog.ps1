param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [string]$ClientLogPath,
    [string]$ClientLauncherPath,
    [string]$ClientNativeExitPath,
    [switch]$Baseline,
    [switch]$Player,
    [switch]$Ownership,
    [switch]$Easy
)
$ErrorActionPreference = 'Stop'
$raw = Get-Content -LiteralPath $LogPath -Raw
$launcher = Get-Content -LiteralPath $LauncherPath -Raw
$failures = @()
if ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0') { $failures += 'NATIVE_EXIT' }
if ($raw -notmatch 'Game destroyed\.') { $failures += 'FULL_STOPPED_LOG_REQUIRED' }
if ($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=(\{[^\r\n]+\})') { $failures += 'MANIFEST_REQUIRED' }
else {
    $manifest = $Matches[1] | ConvertFrom-Json
    if ($manifest.role -ne 'Server' -or $manifest.arguments -notcontains '-aicfFIAProximityProbe') { $failures += 'MANIFEST_PROBE' }
    if ($raw -notmatch 'CLI Params: [^\r\n]*-aicfFIAProximityProbe') { $failures += 'NATIVE_CLI' }
}
$ordinaryErrors = @($raw -split "\r?\n" | Where-Object {
    ($_ -match 'SCRIPT\s+\([EF]\)' -and $_ -notmatch "Leaked '|Total Leaks") -or
    $_ -match 'Virtual Machine Exception|NULL pointer|Unhandled exception|ENGINE\s+\(F\)|Leaked.*AICF_'
})
if ($ordinaryErrors.Count) { $failures += 'SCRIPT_VM_FATAL_OR_AICF_LEAK'; $ordinaryErrors | ForEach-Object { Write-Output $_ } }
if ($raw -match '\[AICF\].*\[ERROR\]|\[PMC_EQUIPMENT_FAILED\]') { $failures += 'AICF_ERROR' }
if ($Player) {
    if (-not $ClientLogPath -or -not $ClientLauncherPath -or -not $ClientNativeExitPath) { $failures += 'CLIENT_EVIDENCE_REQUIRED' }
    else {
        $clientRaw = Get-Content -LiteralPath $ClientLogPath -Raw
        $clientLauncher = Get-Content -LiteralPath $ClientLauncherPath -Raw
        if ((Get-Content -LiteralPath $ClientNativeExitPath -Raw).Trim() -ne '0') { $failures += 'CLIENT_NATIVE_EXIT' }
        $destroyed = $clientRaw.IndexOf('Game destroyed.')
        if ($destroyed -lt 0) { $failures += 'FULL_STOPPED_CLIENT_LOG_REQUIRED'; $destroyed = $clientRaw.Length }
        if ($clientRaw -notmatch '\[FIA_PROXIMITY_CLIENT\] connected=1 controlled=1' -or
            $clientRaw -notmatch '\[FIA_PROXIMITY_CLIENT_OCCUPANT\] controlled=1 occupied=1') { $failures += 'CLIENT_NATIVE_READBACK' }
        if ($clientLauncher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=(\{[^\r\n]+\})') { $failures += 'CLIENT_MANIFEST' }
        else {
            $clientManifest = $Matches[1] | ConvertFrom-Json
            if ($clientManifest.role -ne 'Client' -or $clientManifest.arguments -notcontains '-client' -or
                $clientManifest.arguments -notcontains '-aicfFIAProximityProbe') { $failures += 'CLIENT_MANIFEST_GRAPH' }
        }
        if ($clientLauncher -notmatch '\[SERVER_READY\] process_id=\d+ port=\d+ log=' -or
            $clientRaw -notmatch 'CLI Params: [^\r\n]*-client ') { $failures += 'CLIENT_VERIFIED_CONNECTION' }
        $clientPlay = $clientRaw.Substring(0, $destroyed)
        if ($clientPlay -match 'SCRIPT\s+\([EF]\)|\[AICF\].*\[ERROR\]|\[PMC_EQUIPMENT_FAILED\]' -or
            $clientRaw -match 'Virtual Machine Exception|NULL pointer|Unhandled exception|ENGINE\s+\(F\)|Leaked.*AICF_') { $failures += 'CLIENT_RUNTIME_ERROR' }
        $shutdownErrors = @($clientRaw.Substring($destroyed) -split "\r?\n" | Where-Object { $_ -match 'SCRIPT\s+\([EF]\)' })
        Write-Output "Client post-world-destruction script diagnostics=$($shutdownErrors.Count); full log retained; runtime segment checked separately"
    }
}
$done = [regex]::Match($raw,'\[FIA_PROXIMITY_DONE\] mode=(\w+) checks=(\d+) failures=(\d+)')
if (-not $done.Success) { $failures += 'PROBE_COMPLETION_REQUIRED' }
$cases = @{}
$entries = @([regex]::Matches($raw,'\[FIA_PROXIMITY_CHECK\] case=(\w+) passed=([01])'))
foreach ($match in $entries) { $cases[$match.Groups[1].Value] = $match.Groups[2].Value -eq '1' }
if ($done.Success -and ([int]$done.Groups[2].Value -ne $entries.Count -or
    [int]$done.Groups[3].Value -ne @($entries | Where-Object { $_.Groups[2].Value -eq '0' }).Count)) { $failures += 'DONE_COUNTS' }
if ($Baseline) {
    if (-not $done.Success -or $done.Groups[1].Value -ne 'baseline' -or $done.Groups[3].Value -ne '2') { $failures += 'BASELINE_FAILURES' }
    if ($raw -notmatch 'phase=FAR plan=\d+ groups=\d+ materialized=[1-9]\d* active=\d+ dormant=\d+ far_materialized=[1-9]\d*') { $failures += 'BASELINE_REPRO_READBACK' }
}
else {
    $required = @('TARGET','FAR_NO_FAR_MEMBERS','TARGET_FAR_EMPTY','BUDGET_BLOCKS','APPROACH_NO_FAR_MEMBERS','ACTIVE_AFTER_APPROACH','RETENTION','CURRENT_IDENTITY','STALE_GENERATION','STALE_VEHICLE_ID','UNKNOWN_OCCUPANCY_IDENTITY','DORMANT_5','RETURN_6','IMMUTABLE_PLAN_6','DORMANT_7','RETURN_8','IMMUTABLE_PLAN_8','NO_RESURRECTION','ROAD_PLAN','ROAD_ACTIVE','ROAD_DORMANT','ROAD_RETURN')
    if ($Easy) { $required = @('EASY_EMPTY','EASY_NO_PATROLS') }
    elseif ($Ownership) { $required = @('OWNERSHIP_STALE_EXPANSION','OWNERSHIP_QUEUED_SPAWN_CANCELLED') }
    elseif ($Player -and $manifest.arguments -contains '-aicfFIAPlayerOccupancyOnly') {
        $required = @('TARGET','PLAYER_CONTROLLED_ENTITY','FAR_NO_FAR_MEMBERS','TARGET_FAR_EMPTY','BUDGET_BLOCKS','APPROACH_NO_FAR_MEMBERS','ACTIVE_AFTER_APPROACH','RETENTION','DESANT_DEPLOYED','CURRENT_IDENTITY','STALE_GENERATION','STALE_VEHICLE_ID','UNKNOWN_OCCUPANCY_IDENTITY','PLAYER_OCCUPANCY_VETO')
    }
    elseif ($Player) { $required += @('PLAYER_CONTROLLED_ENTITY','PLAYER_OCCUPANCY_VETO') }
    if (-not $Easy -and -not $Ownership -and -not ($Player -and $manifest.arguments -contains '-aicfFIAPlayerOccupancyOnly')) { $required += @('DESANT_DEPLOYED','DESTROYED_VEHICLE_NOT_REPLACED') }
    foreach ($case in $required) { if (-not $cases.ContainsKey($case) -or -not $cases[$case]) { $failures += "CASE_$case" } }
    if ($done.Success -and $done.Groups[3].Value -ne '0') { $failures += 'PROBE_FAILURES' }
}
if ($failures.Count) { $failures | ForEach-Object { Write-Output "FAIL $_" }; exit 1 }
Write-Output "PASS FIA proximity log; baseline=$Baseline player=$Player easy=$Easy checks=$($cases.Count); native resource/shutdown diagnostics retained separately"
