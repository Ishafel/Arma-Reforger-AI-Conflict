param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$Client,
	[switch]$Files,
	[switch]$LibraryPreview
)
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
$log=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath))
$launcher=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LauncherPath))
$peer='Server'; if ($Client) { $peer='Client' }
if ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0') { $failures.Add('NATIVE_EXIT') }
if ($log -notmatch 'Game destroyed\.') { $failures.Add('INCOMPLETE_LOG') }
$diagnostics=@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')
foreach ($file in $diagnostics) {
    $text=[IO.File]::ReadAllText($file.FullName)
    if ($text -match 'SCRIPT\s+\([EF]\):|ENGINE\s+\(F\):|NULL pointer|Virtual Machine Exception|Unhandled exception|Recursive call of Invoke') { $failures.Add("SCRIPT_FATAL:$($file.Name)") }
}
if ($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=({[^\r\n]+})') { $failures.Add('MANIFEST') }
else {
    $manifest=$matches[1] | ConvertFrom-Json
    if ($manifest.role -ne $peer -or $manifest.arguments -notcontains '-aicfLoadoutQolProbe') { $failures.Add('WRONG_LAUNCH') }
    if ($Files -and $manifest.arguments -notcontains '-aicfLoadoutFilesProbe') { $failures.Add('FILE_PROBE_LAUNCH') }
    $cli=[regex]::Match($log,'CLI Params:([^\r\n]+)').Groups[1].Value.Replace('"','').Replace('/','\')
    foreach ($argument in $manifest.arguments) {
        if ($cli.IndexOf(([string]$argument).Replace('"','').Replace('/','\'),[StringComparison]::OrdinalIgnoreCase) -lt 0) { $failures.Add("CLI_MISMATCH:$argument") }
    }
}
if ($Client -and $launcher -notmatch '\[SERVER_READY\] process_id=\d+ port=2001 log=') { $failures.Add('READINESS') }
$checks=@([regex]::Matches($log,'\[QOL_CHECK\] rule=(\w+) passed=([01])'))
$failed=@($checks | Where-Object { $_.Groups[2].Value -eq '0' })
if ($failed.Count) { $failures.Add("FAILED_CASES=$($failed.Count)") }
$applyCount=10
if($LibraryPreview){$applyCount=12}
if ($Files -and -not $Client -and @($checks | Where-Object { $_.Groups[1].Value.StartsWith('FILE_IMAGE_UNCHANGED_') }).Count -ne $applyCount) { $failures.Add('FILE_IMAGE_COVERAGE') }
$done=[regex]::Matches($log,('\[QOL_'+$peer.ToUpper()+'_DONE\] failures=(\d+)'))
if ($done.Count -ne 1 -or $done[0].Groups[1].Value -ne '0') { $failures.Add('DONE') }
$required=@()
if ($Client) {
    $required=@(1..28 | ForEach-Object { "RPC_STEP_$_" }) + @('CLIENT_CHANGED_DRAFT','REPEATED_APPLY_NO_REVISION','REPEATED_SAVE_NO_REVISION','UPDATE_SAME_ID','DELETED_FROM_LIST','SQUAD_DELETED_FROM_LIST')
    if ($log -notmatch '\[QOL_CLIENT_DONE\] failures=0 step=29') { $failures.Add('CLIENT_COMPLETION') }
    foreach ($step in 1..28) {
        if (@($checks | Where-Object { $_.Groups[1].Value -ceq "RPC_STEP_$step" }).Count -ne 1) { $failures.Add("RPC_COUNT_$step") }
    }
    $negative=@{12='LIBRARY_CONFLICT';13='INVALID_RECIPE';20='MATCH_OR_FACTION_UNAVAILABLE';25='TEMPLATE_UNAVAILABLE';28='MATCH_OR_FACTION_UNAVAILABLE'}
    foreach ($step in $negative.Keys) {
        if ($log -notmatch ('\[QOL_RPC\] step='+$step+' accepted=0 reason='+$negative[$step]+' ')) { $failures.Add("NEGATIVE_$step") }
    }
} else {
    if (@($checks | Where-Object { $_.Groups[1].Value.StartsWith('APPLY_NO_LIBRARY_WRITE_') }).Count -ne $applyCount) { $failures.Add('APPLY_WRITE_COVERAGE') }
    foreach ($side in @('US','USSR')) {
        foreach ($rule in @('STORAGE_CHANGED','STORAGE_VALIDATED','LEGACY_WRITE','LEGACY_READ','UPSERT','DEDUPE','RELOAD','PARTIAL_WRITE_PRESERVES','DELETE','NO_LEGACY_REVIVAL','BATCH_STALE_ATOMIC','BATCH_INVALID_ATOMIC','BATCH_COMMIT','BINDING_INDEPENDENCE')) { $required += "${rule}_$side" }
    }
    foreach ($i in 0..9) {
        $required += "ROLE_SLOT_$i"; $required += "SQUAD_POSITION_$i"
        if (@($checks | Where-Object { $_.Groups[1].Value -ceq "SQUAD_POSITION_$i" }).Count -ne 2) { $failures.Add("SQUAD_REEDIT_$i") }
    }
    $required += @('SQUAD_BAD_LAST_ATOMIC','ENEMY_DENIED','ADMIN_GRANT','ENEMY_ADMIN_READ','ADMIN_REVOKE','ENEMY_REVOKED','CLIENT_CONNECTED')
    if ($log -notmatch '\[QOL_STORAGE_DONE\] failures=0') { $failures.Add('STORAGE_DONE') }
}
foreach ($rule in $required) {
    if (-not @($checks | Where-Object { $_.Groups[1].Value -ceq $rule -and $_.Groups[2].Value -eq '1' }).Count) { $failures.Add("MISSING_$rule") }
}
Write-Output "QOL_COUNTS peer=$peer checks=$($checks.Count) failed=$($failed.Count) resource_errors=$([regex]::Matches($log,'RESOURCES\s*\(E\)').Count)"
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
Write-Output "PASS loadout_qol peer=$peer full_stopped_log=True native_exit=0"
exit 0
