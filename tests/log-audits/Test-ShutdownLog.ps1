param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$Batches,
    [switch]$Client,
    [switch]$RequireStop
)
$ErrorActionPreference='Stop'
function Test-ManifestArgument([string[]]$Arguments,[string]$Name,[string]$Value) {
    $indices=@(for ($i=0; $i -lt $Arguments.Count; $i++) { if ($Arguments[$i] -ceq $Name) { $i } })
    return ($indices.Count -eq 1 -and $indices[0]+1 -lt $Arguments.Count -and $Arguments[$indices[0]+1] -ceq $Value)
}
$failures=[Collections.Generic.List[string]]::new()
$log=Get-Content -LiteralPath $LogPath -Raw
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
if ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0') { $failures.Add('NATIVE_EXIT') }
if ($log -notmatch 'Game destroyed\.') { $failures.Add('FULL_STOPPED_LOG_REQUIRED') }
if ($log -match 'SCRIPT\s+\([EF]\):|ENGINE\s+\(F\):|NULL pointer|Virtual Machine Exception|Unhandled exception') { $failures.Add('SCRIPT_FATAL') }
if ($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=({[^\r\n]+})') { $failures.Add('MANIFEST') }
else {
    $manifest=$matches[1]|ConvertFrom-Json
    $role='Server'; if ($Client) {$role='Client'}
    if ($manifest.role -ne $role -or $manifest.arguments -notcontains '-aicfShutdownMode') { $failures.Add('WRONG_LAUNCH') }
    if ($Batches) {
        if (-not (Test-ManifestArgument $manifest.arguments '-aicfShutdownMode' 'batches')) { $failures.Add('BATCH_MODE_REQUIRED') }
        if (-not (Test-ManifestArgument $manifest.arguments '-aicfShutdownEquipmentBatch' '1')) { $failures.Add('EQUIPMENT_BATCH_REQUIRED') }
    }
}
if ($Client) {
    if ($launcher -notmatch '\[SERVER_READY\] process_id=\d+ port=\d+ log=') { $failures.Add('READINESS_GATE') }
    if ($log -notmatch '\[SHUTDOWN_CLIENT_DONE\]') { $failures.Add('CLIENT_COMPLETION') }
} else {
    $done=[regex]::Matches($log,'\[SHUTDOWN_DONE\] mode=(idle|batches) steps=4 connected=([01]) elapsed_ms=(\d+)')
    if ($done.Count -ne 1 -or [int]$done[0].Groups[3].Value -lt 55000) { $failures.Add('SERVER_COMPLETION') }
    if ($Batches -and $done.Count -eq 1 -and $done[0].Groups[1].Value -ne 'batches') { $failures.Add('BATCH_COMPLETION_MODE') }
    foreach ($phase in @('ready','cleanup_0','cleanup_1','cleanup_2','cleanup_3','rest')) {
        if ($log -notmatch ('\[SHUTDOWN_MEMORY\] phase='+$phase+' name=MemC value=\d+')) { $failures.Add("MEMORY_$phase") }
    }
    if ($RequireStop) {
        if ($log -notmatch '\[SHUTDOWN_RETAINED\] phase=rest sampled=\d+ alive=0' -or $log -notmatch '\[SHUTDOWN_RETAINED\] phase=after_stop sampled=\d+ alive=0') { $failures.Add('DELAYED_NATIVE_ITEMS_RETAINED') }
        $stop=[regex]::Matches($log,'\[SHUTDOWN_OBJECTS\] phase=after_stop drafts=(\d+) created=(\d+) destroyed=(\d+)')
        if (-not $stop.Count) { $failures.Add('STOP_READBACK') }
        foreach ($result in $stop) {
            if ([int]$result.Groups[1].Value -ne 0 -or $result.Groups[2].Value -ne $result.Groups[3].Value) { $failures.Add('DRAFTS_RETAINED') }
        }
        $clear=[regex]::Matches($log,'\[SHUTDOWN_EQUIPMENT_CLEAR\] items=(\d+) alive=(\d+)')
        if (-not $clear.Count) { $failures.Add('NATIVE_EQUIPMENT_CLEAR') }
        foreach ($result in $clear) { if ([int]$result.Groups[2].Value -ne 0) { $failures.Add('NATIVE_ITEMS_RETAINED') } }
    }
    if ($Batches) {
        $operations=[regex]::Matches($log,'\[SHUTDOWN_OPERATION\] batch=(\d) iteration=(\d) side=(USSR|US) passed=([01])')
        if ($operations.Count -ne 24 -or @($operations|Where-Object {$_.Groups[4].Value -ne '1'}).Count) { $failures.Add('OPERATIONS') }
        $keys=@($operations|ForEach-Object {$_.Groups[1].Value+':'+$_.Groups[2].Value+':'+$_.Groups[3].Value}|Select-Object -Unique)
        if ($keys.Count -ne 24) { $failures.Add('OPERATION_IDENTITIES') }
        foreach ($batch in 1..3) {
            if ($log -notmatch ('\[SHUTDOWN_BATCH\] batch='+$batch+' operations=8 failures=0')) { $failures.Add("BATCH_$batch") }
        }
    }
}
Write-Output "SHUTDOWN_COUNTS script_errors=$([regex]::Matches($log,'SCRIPT\s+\([EF]\):').Count) leaked_objects=$((@([regex]::Matches($log,"Leaked '[^']+' script instance \((\d+)x\)")|ForEach-Object {[int]$_.Groups[1].Value})|Measure-Object -Sum).Sum)"
if ($failures.Count) {$failures|Select-Object -Unique|ForEach-Object {"FAIL $_"};exit 1}
Write-Output 'PASS shutdown full script/fatal gate and required native readbacks'
exit 0
