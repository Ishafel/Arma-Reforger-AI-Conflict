param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][ValidateSet('Stock','RHS','WCS')][string]$Profile,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath
)
$ErrorActionPreference = 'Stop'
$lines = [IO.File]::ReadAllLines((Resolve-Path $LogPath))
$text = $lines -join "`n"
if (([IO.File]::ReadAllText((Resolve-Path $NativeExitPath))).Trim() -ne '0') { throw 'Native exit is not 0' }
$manifestLine = @([IO.File]::ReadAllLines((Resolve-Path $LauncherPath)) | Where-Object { $_.StartsWith('AICF_RUNTIME_MANIFEST_JSON=') })
if ($manifestLine.Count -ne 1) { throw 'Missing unique launcher manifest' }
$manifest = $manifestLine[0].Substring('AICF_RUNTIME_MANIFEST_JSON='.Length) | ConvertFrom-Json
$cli = @($lines | Where-Object { $_ -match 'CLI Params:' })
if ($cli.Count -ne 1) { throw 'Missing unique native CLI Params' }
foreach ($argument in $manifest.arguments) {
    if (-not $cli[0].Contains([string]$argument)) { throw "Native CLI missing argument: $argument" }
}
if ($Profile -eq 'RHS' -and ($manifest.arguments -join ',') -match '6A717EEA63A35E67|65F929DF622BAD50|A1CF260928100001') { throw 'RHS probe unexpectedly loads WCS' }
if ($text -match 'VM Exception|NULL pointer|division by zero|error:|ENGINE\s+\(F\)|SCRIPT\s+\(F\)') { throw 'Script/VM/fatal failure in full log' }
$finished = @([regex]::Matches($text, '\[FIA_ARMAMENT_FINISHED\] checks=(\d+) failures=(\d+) models=(\d+) profile=(\S+)'))
if ($finished.Count -ne 1 -or $finished[0].Groups[2].Value -ne '0') { throw 'Missing unique successful result' }
$models = 16
$checks = 45
if ($Profile -eq 'RHS') { $models=61; $checks=381 }
if ($Profile -eq 'WCS') { $models=54; $checks=340 }
if ([int]$finished[0].Groups[1].Value -ne $checks -or [int]$finished[0].Groups[3].Value -ne $models) { throw 'Unexpected matrix size' }
$cases = @([regex]::Matches($text, '\[FIA_ARMAMENT_CASE\] case=(\S+) passed=([01])'))
if ($cases.Count -ne $checks -or @($cases | Where-Object { $_.Groups[2].Value -ne '1' }).Count) { throw 'Missing/failed cases' }
$roles = @('Rifleman','SL','Medic','MG','AMG','LAT','AT','AAT','Sharpshooter','RTO','Sapper','Ammo','Scout')
foreach ($role in $roles) {
    if ($text -notmatch ('case=ROLE_FOUND passed=1 role=' + $role + '(?:\r?$|\n)')) { throw "Missing role $role" }
}
if ($Profile -eq 'Stock') {
    if ([regex]::Matches($text, 'case=STOCK_UNCHANGED passed=1 ').Count -ne 16 -or $text -match '\[PMC_EQUIPMENT_APPLIED\]') { throw 'Stock inventory changed or matrix incomplete' }
} else {
    foreach ($case in @('INVALID_SELECTION_UNCHANGED','UNAVAILABLE_CANDIDATE_UNCHANGED','NATIVE_REFUSAL_ROLLBACK','RETRY_STABLE','AUTOMATIC_DIVERSITY','PATROL_PATH','GARRISON_DESANT_PATH','PATHS_NO_FAILURES')) {
        if ($text -notmatch ('case=' + $case + ' passed=1')) { throw "Missing case $case" }
    }
    if ($text -notmatch '\[FIA_ARMAMENT_INSERTION_FAULT\] matched=1') { throw 'Native insertion fault was not injected' }
    foreach ($kind in @('PATROL','GARRISON')) {
        foreach ($phase in @('PENDING','READY')) {
            if ($text -notmatch ("\[FIA_ARMAMENT_PATH\] kind=$kind phase=$phase passed=1")) { throw "Missing path $kind/$phase" }
        }
    }
    $samples = [regex]::Matches($text, 'case=CANONICAL_RESOLVED passed=1 source=(.+?) variant=(\d+) weapon=(.+)')
    if ($samples.Count -ne $models) { throw 'Missing canonical live readback' }
    $matrix = @{}
    foreach ($sample in $samples) {
        $source = $sample.Groups[1].Value
        if (-not $matrix.ContainsKey($source)) { $matrix[$source] = @{} }
        $matrix[$source][$sample.Groups[2].Value] = $true
    }
    if ($matrix.Count -ne 16) { throw 'Missing ordinary/AC source prefab' }
    foreach ($source in $matrix.Keys) {
        $count = 4
        if ($Profile -eq 'WCS') { $count=3 }
        if ($source.EndsWith('_MG.et')) { $count=1 }
        if ($source.EndsWith('_Sharpshooter.et')) { $count=1; if ($Profile -eq 'WCS') { $count=2 } }
        if ($source.EndsWith('_Scout.et') -and $Profile -eq 'RHS') { $count=1 }
        for ($variant=0; $variant -lt $count; $variant++) {
            if (-not $matrix[$source].ContainsKey([string]$variant)) { throw "Missing candidate $source/$variant" }
        }
    }
    $negative = $false
    foreach ($line in $lines) {
        if ($line.Contains('[FIA_ARMAMENT_NEGATIVE] phase=BEGIN')) { $negative=$true }
        if (-not $negative -and $line -match '\[PMC_DRAFT_FAILED\]|\[PMC_EQUIPMENT_FAILED\]|\[PMC_ARMAMENT_INVALID\]|\[LOADOUT_NATIVE_RESTORE_FAILED\]') { throw "Unexpected inventory failure: $line" }
        if ($line.Contains('[FIA_ARMAMENT_NEGATIVE] phase=END')) { $negative=$false }
    }
    if ($negative) { throw 'Unclosed negative section' }
}
$engineErrors = @($lines | Where-Object { $_ -match '\(E\)' }).Count
$unexpectedScriptErrors = @($lines | Where-Object { $_ -match 'SCRIPT\s+\(E\)' -and $_ -notmatch "Leaked '|Total Leaks|\[PMC_EQUIPMENT_FAILED\]" })
if ($unexpectedScriptErrors.Count) { throw ('Unexpected script errors: ' + ($unexpectedScriptErrors -join "`n")) }
"PASS FIA armament $Profile checks=$checks models=$models; full_log_errors=$engineErrors (dependency/negative/shutdown diagnostics retained)"
