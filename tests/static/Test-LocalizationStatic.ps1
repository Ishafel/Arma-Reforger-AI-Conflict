param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../../tools/lib/AICFLocalization.Common.ps1')
$entries = Read-AICFLocalization $RepositoryRoot
$failures = [Collections.Generic.List[string]]::new()
$cyrillicPattern = '[\u0410-\u044F\u0401\u0451]'
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
    Require ($entries[$id].en_us -notmatch $cyrillicPattern) "Russian text in English: $id"
}
foreach ($codepoint in @(0x0410, 0x044F, 0x0401, 0x0451)) {
    Require (([string][char]$codepoint) -match $cyrillicPattern) "Cyrillic detection failed: $codepoint"
}
Require ('English' -notmatch $cyrillicPattern) 'English incorrectly detected as Cyrillic'
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
$setup=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArland/Missions/AICF_Conflict_Arland_Setup.conf'))
Require ($setup.Contains('m_sName "AI Conflict"')) 'Literal universal entry identity'
foreach($key in @('AICF_Setup_Map_Arland','AICF_Setup_Map_Everon','AICF_Setup_Map_EveronNorth')){Require ($entries.Contains($key)) "Map label: $key"}
$sourceFiles = @(Get-ChildItem (Join-Path $core 'Scripts') -Recurse -Filter '*.c' |
    Where-Object { $_.Name -ne 'AICF_LocalizationProbe.c' })
foreach ($file in $sourceFiles) {
    $source = Get-Content $file.FullName -Raw -Encoding UTF8
    foreach ($match in [regex]::Matches($source, '\{AICF:(AICF_[A-Za-z0-9_]+)\}')) {
        Require ($entries.Contains($match.Groups[1].Value)) "Unresolved key $($match.Groups[1].Value): $($file.Name)"
    }
    foreach ($token in [regex]::Matches($source, '(?s)//[^\r\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"')) {
        if ($token.Value.StartsWith('"')) {
            Require ($token.Value -notmatch $cyrillicPattern) "Hardcoded Russian literal: $($file.Name): $($token.Value)"
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
try { & (Join-Path $PSScriptRoot '../../tools/Build-AICFLocalization.ps1') -RepositoryRoot $RepositoryRoot -Check }
catch { $failures.Add($_.Exception.Message) }
if ($failures.Count) { $failures | ForEach-Object { "[AICF][LOCALIZATION_STATIC][FAIL] $_" }; exit 1 }
Write-Output "[AICF][LOCALIZATION_STATIC][PASS] entries=$($entries.Count) references=PASS placeholders=PASS fallback=PASS platform=PASS authority=PASS runtime_tables=PASS"
exit 0
