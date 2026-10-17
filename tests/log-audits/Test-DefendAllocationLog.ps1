param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$ExpectReproduction
)
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
$log=Get-Content -LiteralPath $LogPath -Raw
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
if ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0') {$failures.Add('NATIVE_EXIT')}
if ($log -notmatch 'Game destroyed\.') {$failures.Add('FULL_STOPPED_LOG_REQUIRED')}
if ($log -match 'ENGINE\s+\(F\)|NULL pointer|Unhandled exception') {$failures.Add('ENGINE_FATAL')}
if ($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=({[^\r\n]+})') {$failures.Add('MANIFEST')}
else {
    $manifest=$matches[1]|ConvertFrom-Json
    $index=[array]::IndexOf($manifest.arguments,'-aicfDefendAllocationProbe')
    if ($manifest.role -ne 'Server' -or $index -lt 0 -or $manifest.arguments[$index+1] -cne '1') {$failures.Add('PROBE_LAUNCH')}
    $cli=[regex]::Match($log,'CLI Params:\s*([^\r\n]+)')
    if (!$cli.Success -or $cli.Groups[1].Value.Trim() -cne ($manifest.arguments -join ' ')) {$failures.Add('EXACT_CLI')}
}
if ($log -notmatch '\[ROSTER_READY\]') {$failures.Add('ROSTER')}
$errors=[regex]::Matches($log,'SCRIPT\s+\([EF]\):').Count
$reasons=[regex]::Matches($log,'Reason: Defend Waypoint not provided to the node!').Count
if ($ExpectReproduction) {
    if ($reasons -lt 2 -or $errors -ne $reasons) {$failures.Add('BASELINE_REPRODUCTION')}
} elseif ($errors -or $log -match 'Virtual Machine Exception') {$failures.Add('SCRIPT_FATAL')}
$expected=@('SETUP','HOLD_CREATED','NATIVE_ALLOCATION_SUCCESS','NATIVE_ACTIVITY','MOVE_RESTORED','OLD_WAYPOINT_REMOVED','NATIVE_INPUT_EMPTY')
if (!$ExpectReproduction) {$expected+=@('STALE_64_FAILS','MOVE_UNCHANGED','GROUP_UNCHANGED','NEW_ORDER_CURRENT')}
foreach($side in @('US','USSR')) {
    foreach($name in $expected) {
        $records=[regex]::Matches($log,('\[DEFEND_ALLOCATION_CHECK\] side='+$side+' name='+$name+' passed=([01])'))
        if ($records.Count -ne 1 -or $records[0].Groups[1].Value -ne '1') {$failures.Add("CHECK_${side}_$name")}
    }
    if ([regex]::Matches($log,('\[DEFEND_ALLOCATION_ATTEMPT\] side='+$side+' calls=64 input=REMOVED_BY_PLANNER')).Count -ne 1) {$failures.Add("ATTEMPT_$side")}
}
$done=[regex]::Matches($log,'\[DEFEND_ALLOCATION_FINISHED\] checks=(\d+) failures=(\d+) attempted=(\d+) returned=(\d+)')
if ($done.Count -ne 1 -or $done[0].Groups[2].Value -ne '0' -or $done[0].Groups[3].Value -ne '2') {$failures.Add('COMPLETION')}
if (!$ExpectReproduction) {
    if ($done.Count -eq 1 -and ($done[0].Groups[1].Value -ne '22' -or $done[0].Groups[4].Value -ne '2')) {$failures.Add('RETURNED_CHECKS')}
    if ([regex]::Matches($log,'\[AI_DEFEND_ALLOCATION_CANCELLED\]').Count -ne 2) {$failures.Add('BOUNDED_CANCELLATION')}
}
if ($failures.Count) {$failures|Select-Object -Unique|ForEach-Object {"FAIL $_"};exit 1}
if ($ExpectReproduction) {Write-Output "BASELINE_REPRODUCED native_defend_exceptions=$reasons runtime_gate=FAIL"}
else {Write-Output 'PASS native defend allocation: 22 checks, 128 stale calls, 2 bounded cancellations, full script/fatal gate'}
exit 0
