param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AICFLocalization.Common.ps1')
$entries = Read-AICFLocalization $RepositoryRoot
$failures = [Collections.Generic.List[string]]::new()
function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { $failures.Add($Message) }
}
function Same-Placeholders([string]$English, [string]$Russian) {
    $left = @([regex]::Matches($English, '%[1-9]') | ForEach-Object Value | Sort-Object) -join ','
    $right = @([regex]::Matches($Russian, '%[1-9]') | ForEach-Object Value | Sort-Object) -join ','
    return $left -ceq $right
}
foreach ($id in $entries.Keys) {
    Require ($id -cmatch '^AICF_[A-Za-z0-9_]+$') "Invalid ID: $id"
    Require (-not [string]::IsNullOrWhiteSpace($entries[$id].en_us)) "Empty English: $id"
    Require (-not [string]::IsNullOrWhiteSpace($entries[$id].ru_ru)) "Empty Russian: $id"
    Require (Same-Placeholders $entries[$id].en_us $entries[$id].ru_ru) "Placeholder mismatch: $id"
    Require ($entries[$id].en_us -notmatch '[А-Яа-яЁё]') "Russian text in English: $id"
}
Require (-not (Same-Placeholders '%1 / %2' '%1')) 'Negative missing parameter was not detected'
Require (-not (Same-Placeholders '%1 / %2' '%1 / %1')) 'Negative duplicate parameter was not detected'

$core = Join-Path $RepositoryRoot 'AIConflictCore'
$project = Get-Content (Join-Path $core 'addon.gproj') -Raw -Encoding UTF8
foreach ($language in @('en_us', 'ru_ru', 'fr_fr', 'it_it', 'de_de', 'es_es', 'cs_cz',
    'pl_pl', 'ja_jp', 'ko_kr', 'pt_br', 'zh_cn', 'uk_ua')) {
    $runtime = 'en_us'
    if ($language -eq 'ru_ru') { $runtime = 'ru_ru' }
    Require ($project -match ('Code "' + $language + '"\s+StringTableRuntime "[^"]+AICF_Localization\.' + $runtime + '\.conf"')) "Language registration: $language"
}
foreach ($platform in @('HEADLESS', 'XBOX_ONE', 'XBOX_SERIES', 'PS4')) {
    Require ($project -match ("GameProjectConfig $platform : PC")) "Platform inheritance: $platform"
}
$scenarioPaths = [ordered]@{
    EveronWCSRHS = 'AIConflictEveronWCSRHS/Missions/AICF_WCS_RHS_Conflict_Everon.conf'
    EveronNorthWCSRHS = 'AIConflictEveronWCSRHS/Missions/AICF_WCS_RHS_Conflict_Everon_North.conf'
    Arland = 'AIConflictArland/Missions/AICF_Conflict_Arland.conf'
    Everon = 'AIConflictEveron/Missions/AICF_Conflict_Everon.conf'
    ArlandRHS = 'AIConflictArlandRHS/Missions/AICF_RHS_Conflict_Arland.conf'
    EveronRHS = 'AIConflictEveronRHS/Missions/AICF_RHS_Conflict_Everon.conf'
    EveronNorthRHS = 'AIConflictEveronRHS/Missions/AICF_RHS_Conflict_Everon_North.conf'
    EveronNorth = 'AIConflictEveron/Missions/AICF_Conflict_Everon_North.conf'
}
foreach ($variant in $scenarioPaths.Keys) {
    $header = Get-Content (Join-Path $RepositoryRoot $scenarioPaths[$variant]) -Raw -Encoding UTF8
    foreach ($field in @('Name', 'Description', 'Details')) {
        $id = "AICF_Scenario_${variant}_$field"
        Require ($header.Contains("m_s$field `"#$id`"") -and $entries.Contains($id)) "Header key: $id"
    }
    Require ($entries["AICF_Scenario_${variant}_Name"].en_us.StartsWith('AI Conflict')) "Scenario identity: $variant"
}
$sourceFiles = @(Get-ChildItem (Join-Path $core 'Scripts') -Recurse -Filter '*.c' |
    Where-Object { $_.Name -ne 'AICF_LocalizationProbe.c' })
foreach ($file in $sourceFiles) {
    $source = Get-Content $file.FullName -Raw -Encoding UTF8
    foreach ($match in [regex]::Matches($source, '\{AICF:(AICF_[A-Za-z0-9_]+)\}')) {
        Require ($entries.Contains($match.Groups[1].Value)) "Unresolved key $($match.Groups[1].Value): $($file.Name)"
    }
    foreach ($token in [regex]::Matches($source, '(?s)//[^\r\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"')) {
        if ($token.Value.StartsWith('"')) {
            Require ($token.Value -notmatch '[А-Яа-яЁё]') "Hardcoded Russian literal: $($file.Name): $($token.Value)"
        }
    }
    Require ($source -notmatch 'string\.Format\(\s*"\{AICF:') "Native formatting loses localized parameters: $($file.Name)"
}
$producerPaths = @('UI/AICF_LogisticsMapMarkers.c', 'Bootstrap/AICF_MatchController.c')
foreach ($path in $producerPaths) {
    $source = Get-Content (Join-Path $core "Scripts/Game/AIConflict/$path") -Raw -Encoding UTF8
    Require ($source -notmatch 'WidgetManager.Translate') "Server must keep untranslated stock keys: $path"
}
$helper = Get-Content (Join-Path $core 'Scripts/Game/AIConflict/UI/AICF_Localization.c') -Raw -Encoding UTF8
Require ($helper -notmatch 'SetLanguage|Replication.BumpMe|RplRpc') 'Localization must not change language or gameplay state'
foreach ($path in @('UI/AICF_StrategicUI.c', 'UI/AICF_MapMarkerCardWidget.c', 'UI/AICF_SupplyMapUI.c', 'UI/AICF_LoadoutEditor.c', 'UI/AICF_LocalizedStaticMarker.c')) {
    $source = Get-Content (Join-Path $core "Scripts/Game/AIConflict/$path") -Raw -Encoding UTF8
    Require ($source.Contains('AICF_Localization.Resolve(')) "Missing client rendering boundary: $path"
}
$probe = Join-Path $core 'Scripts/Game/AIConflict/UI/AICF_LocalizationProbe.c'
Require (-not (Test-Path $probe)) 'Remove runtime probe from production sources'
try { & (Join-Path $PSScriptRoot 'Build-AICFLocalization.ps1') -RepositoryRoot $RepositoryRoot -Check }
catch { $failures.Add($_.Exception.Message) }
if ($failures.Count) { $failures | ForEach-Object { "[AICF][LOCALIZATION_STATIC][FAIL] $_" }; exit 1 }
Write-Output "[AICF][LOCALIZATION_STATIC][PASS] entries=$($entries.Count) references=PASS placeholders=PASS fallback=PASS platform=PASS authority=PASS runtime_tables=PASS"
exit 0
