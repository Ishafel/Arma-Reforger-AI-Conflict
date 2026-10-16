param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$Baseline
)
$ErrorActionPreference='Stop'
$raw=Get-Content -LiteralPath $LogPath -Raw
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
$failures=@()
if ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0' -or $raw -notmatch 'Game destroyed\.') { $failures += 'FULL_STOPPED_NATIVE_LOG' }
if ($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=(\{[^\r\n]+\})') { $failures += 'MANIFEST' }
else {
    $manifest=$Matches[1] | ConvertFrom-Json
    if ($manifest.role -ne 'Server' -or $manifest.arguments -notcontains '-aicfFIADesantTransferProbe' -or
        $raw -notmatch 'CLI Params: [^\r\n]*-aicfFIADesantTransferProbe ') { $failures += 'NATIVE_CLI' }
}
$checks=@([regex]::Matches($raw,'\[FIA_PROXIMITY_CHECK\] case=(\w+) passed=([01])'))
$done=[regex]::Match($raw,'\[FIA_PROXIMITY_DONE\] mode=us checks=(\d+) failures=(\d+)')
$failed=@($checks | Where-Object { $_.Groups[2].Value -eq '0' } | ForEach-Object { $_.Groups[1].Value })
if (!$done.Success -or [int]$done.Groups[1].Value -ne $checks.Count -or [int]$done.Groups[2].Value -ne $failed.Count) { $failures += 'DONE_COUNTS' }
$required=@('TRANSFER_NATIVE_REFUSAL','TRANSFER_ROLLBACK_PARENT','TRANSFER_ROLLBACK_CAPACITY','TRANSFER_ROLLBACK_READY')
if ($Baseline) {
    if ($failed.Count -ne 2 -or 'TRANSFER_ROLLBACK_CAPACITY' -notin $failed -or 'TRANSFER_ROLLBACK_READY' -notin $failed) { $failures += 'BASELINE_REPRO' }
    $required=@('TRANSFER_NATIVE_REFUSAL','TRANSFER_ROLLBACK_PARENT')
}
else {
    $required += @('TRANSFER_RETRY','TRANSFER_SAME_ENTITY','TRANSFER_TOTAL')
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Test-FIAProximityLog.ps1') -LogPath $LogPath -LauncherPath $LauncherPath -NativeExitPath $NativeExitPath
    if ($LASTEXITCODE -ne 0) { $failures += 'FULL_PROXIMITY_REGRESSION' }
    if ($failed.Count) { $failures += 'TRANSFER_FAILURE' }
}
foreach ($case in $required) {
    $caseChecks=@($checks | Where-Object { $_.Groups[1].Value -eq $case })
    if ($caseChecks.Count -ne 1 -or $caseChecks[0].Groups[2].Value -ne '1') { $failures += "CASE_$case" }
}
$ordinary=@($raw -split "\r?\n" | Where-Object { ($_ -match 'SCRIPT\s+\([EF]\)' -and $_ -notmatch "Leaked '|Total Leaks") -or $_ -match 'Virtual Machine Exception|NULL pointer|ENGINE\s+\(F\)|Leaked.*AICF_|\[AICF\].*\[ERROR\]|\[PMC_EQUIPMENT_FAILED\]' })
if ($ordinary.Count) { $failures += 'SCRIPT_OR_FATAL'; $ordinary }
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
"PASS FIA desant transfer; baseline=$Baseline; checks=$($checks.Count); native shutdown diagnostics retained separately"
