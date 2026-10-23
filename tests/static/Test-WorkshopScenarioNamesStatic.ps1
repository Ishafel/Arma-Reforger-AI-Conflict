param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$CsvPath
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../../tools/lib/AICFLocalization.Common.ps1')
$entries = Read-AICFLocalization $RepositoryRoot
$failures = [Collections.Generic.List[string]]::new()
$rows = [Collections.Generic.List[object]]::new()
$headers = @(Get-ChildItem -LiteralPath $RepositoryRoot -Directory | Where-Object Name -Like 'AIConflict*' | ForEach-Object {
    $missions = Join-Path $_.FullName 'Missions'
    if (Test-Path -LiteralPath $missions) { Get-ChildItem -LiteralPath $missions -File -Filter '*.conf' }
})
if (-not $headers.Count) { $failures.Add('[WORKSHOP_SCENARIO_NAMES_MISSING] No project mission headers found') }
foreach ($header in $headers) {
    $match = [regex]::Match([IO.File]::ReadAllText($header.FullName), 'm_sName\s+"([^"\r\n]+)"')
    if (-not $match.Success) {
        $failures.Add("[WORKSHOP_SCENARIO_NAME_MISSING] $($header.Name)")
        continue
    }
    $name = $match.Groups[1].Value
    $rows.Add([pscustomobject]@{ header = $header.FullName; language = 'raw'; name = $name; length = $name.Length })
    if ($name.Length -gt 50) { $failures.Add("[WORKSHOP_SCENARIO_RAW_NAME_LENGTH] $($header.Name): $($name.Length) > 50 ($name)") }
    if (-not $name.StartsWith('#')) { continue }
    $id = $name.Substring(1)
    if (-not $entries.Contains($id)) {
        $failures.Add("[WORKSHOP_SCENARIO_NAME_UNRESOLVED] $($header.Name): $id")
        continue
    }
    foreach ($language in @('en_us', 'ru_ru')) {
        $text = $entries[$id][$language]
        $rows.Add([pscustomobject]@{ header = $header.FullName; language = $language; name = $text; length = $text.Length })
        if ([string]::IsNullOrWhiteSpace($text) -or $text.Length -gt 50) {
            $failures.Add("[WORKSHOP_SCENARIO_LOCALIZED_NAME_LENGTH] $($header.Name) ${language}: $($text.Length) outside 1..50 ($text)")
        }
    }
}
if ($CsvPath) { $rows | Export-Csv -LiteralPath $CsvPath -NoTypeInformation -Encoding UTF8 }
if ($failures.Count) {
    $failures | ForEach-Object { "[AICF][WORKSHOP_SCENARIO_NAMES][FAIL] $_" }
    exit 1
}
$maximum = ($rows | Measure-Object -Property length -Maximum).Maximum
"[AICF][WORKSHOP_SCENARIO_NAMES][PASS] headers=$($headers.Count) names=$($rows.Count) max_length=$maximum limit=50"
exit 0
