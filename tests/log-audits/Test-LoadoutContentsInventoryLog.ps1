param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$WCS,
    [switch]$Client,
    [switch]$Baseline
)
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
$log=Get-Content -LiteralPath $LogPath -Raw
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
if ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0') { $failures.Add('NATIVE_EXIT') }
if ($log -notmatch 'Game destroyed\.') { $failures.Add('INCOMPLETE_LOG') }
if ($log -match 'SCRIPT\s+\([EF]\):|ENGINE\s+\(F\):|NULL pointer|Virtual Machine Exception|Unhandled exception') { $failures.Add('SCRIPT_FATAL') }
if ($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=({[^\r\n]+})') { $failures.Add('MANIFEST') }
else {
    $manifest=$matches[1] | ConvertFrom-Json
    $role='Server'; if ($Client) { $role='Client' }
    if ($manifest.role -ne $role -or $manifest.arguments -notcontains '-aicfContentsInventoryProbe') { $failures.Add('WRONG_LAUNCH') }
}
$peer=1; if ($Client) { $peer=0 }
$events=@([regex]::Matches($log,('\[CLOTHING_PROBE\] side=(USSR|US) server='+$peer+' rule=(\w+) passed=([01]) ([^\r\n]*)')))
$failed=@($events | Where-Object { $_.Groups[3].Value -eq '0' })
$done=[regex]::Matches($log,'\[MAG_PROBE_DONE\] failures=(\d+)')
if ($done.Count -ne 1 -or [int]$done[0].Groups[1].Value -ne $failed.Count) { $failures.Add('RESULT_COUNT') }
if ($Baseline) {
    if (-not $WCS -or -not $failed.Count -or @($failed | Where-Object {$_.Groups[2].Value -ne 'MAG_ROWS_EXACT'}).Count) { $failures.Add('BASELINE_NOT_REPRODUCED') }
} elseif ($failed.Count) { $failures.Add("CHECK_FAILURES=$($failed.Count)") }
foreach ($side in @('USSR','US')) {
    $sideEvents=@($events | Where-Object {$_.Groups[1].Value -eq $side})
    foreach ($rule in @('BUILD','MAG_EDIT_CANDIDATE','MAG_REMOVE_EXACT','MAG_UNDO_EXACT','MAG_REMOVED_CAPTURE','MAG_REMOVED_RESTORE','MAG_REMOVED_SIGNATURE','MAG_REMOVED_VALIDATE','MAG_ORIGINAL_CAPTURE','MAG_ORIGINAL_RESTORE','MAG_ORIGINAL_SIGNATURE','MAG_ORIGINAL_VALIDATE')) {
        if (-not @($sideEvents | Where-Object {$_.Groups[2].Value -eq $rule -and $_.Groups[3].Value -eq '1'}).Count) { $failures.Add("MISSING_${side}_$rule") }
    }
    if (@($sideEvents | Where-Object {$_.Groups[2].Value -eq 'MAG_ROWS_EXACT'}).Count -lt 3) { $failures.Add("ROWS_COVERAGE_$side") }
}
if ($WCS) {
    if ($log -notmatch ('side=USSR server='+$peer+' rule=MAG_ORIGINAL_SIX passed=1 count=6 ')) { $failures.Add('ORIGINAL_KIT') }
    $original=[regex]::Matches($log,'\[MAG_NATIVE\] side=USSR entity=(\d+) prefab=\{62DEC638B985B3FE\}[^\r\n]+parent=(\d+) storage=RHS_EquipmentStorageComponent:5222CB03D1147687 slot=(\d+) path=([^\r\n]+)')
    if ($original.Count -lt 6 -or @($original | ForEach-Object {$_.Groups[1].Value} | Select-Object -Unique).Count -lt 6) { $failures.Add('NATIVE_MAGAZINES') }
}
Write-Output "CONTENTS_INVENTORY_COUNTS events=$($events.Count) failed_checks=$($failed.Count) log_errors=$([regex]::Matches($log,'SCRIPT\s+\([EF]\):|ENGINE\s+\(F\):').Count)"
if ($failures.Count) { $failures | ForEach-Object {Write-Output "FAIL $_"}; exit 1 }
Write-Output "PASS contents_inventory events=$($events.Count) expected_baseline_failures=$($failed.Count)"
exit 0
