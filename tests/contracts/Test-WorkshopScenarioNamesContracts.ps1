param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]'\/')
$work = Join-Path $tempBase ('aicf-workshop-names-' + [guid]::NewGuid().ToString('N'))
$languagePath = Join-Path $work 'AIConflictCore/Language/AICF_Localization.st'
$headerPath = Join-Path $work 'AIConflictArland/Missions/fixture.conf'
$audit = Join-Path $PSScriptRoot '../static/Test-WorkshopScenarioNamesStatic.ps1'
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = 0
function Check-Names([string]$Case, [string]$Name, [string]$Id, [string]$English, [string]$Russian, [int]$Exit, [string]$Rule) {
    [IO.File]::WriteAllText($headerPath, ('SCR_MissionHeaderCampaign { m_sName "' + $Name + '" }'), $utf8)
    [IO.File]::WriteAllText($languagePath, ('Id "' + $Id + '" Target_en_us "' + $English + '" Target_ru_ru "' + $Russian + '"'), $utf8)
    $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $audit -RepositoryRoot $work 2>&1)
    if ($LASTEXITCODE -ne $Exit -or ($Rule -and ($output -join "`n") -notmatch [regex]::Escape($Rule))) {
        throw "Unexpected result for ${Case}: $output"
    }
    $script:checks++
    "PASS workshop names case=$Case audit_exit=$LASTEXITCODE"
}
try {
    [void](New-Item -ItemType Directory -Path (Split-Path -Parent $languagePath), (Split-Path -Parent $headerPath) -Force)
    $id = 'AICF_' + ('X' * 44)
    Check-Names 'exact-50' ('#' + $id) $id ('E' * 50) ('R' * 50) 0 ''
    Check-Names 'raw-51' ('#' + $id + 'X') ($id + 'X') ('E' * 50) ('R' * 50) 1 'WORKSHOP_SCENARIO_RAW_NAME_LENGTH'
    Check-Names 'english-51' ('#' + $id) $id ('E' * 51) ('R' * 50) 1 'en_us: 51'
    Check-Names 'russian-51' ('#' + $id) $id ('E' * 50) ('R' * 51) 1 'ru_ru: 51'
    Check-Names 'unresolved' '#AICF_Missing' $id 'English' 'Russian' 1 'WORKSHOP_SCENARIO_NAME_UNRESOLVED'
    Check-Names 'literal-50' ('L' * 50) $id 'English' 'Russian' 0 ''
    Check-Names 'literal-51' ('L' * 51) $id 'English' 'Russian' 1 'WORKSHOP_SCENARIO_RAW_NAME_LENGTH'
    Check-Names 'empty-translation' ('#' + $id) $id '' 'Russian' 1 'WORKSHOP_SCENARIO_LOCALIZED_NAME_LENGTH'
} finally {
    $target = [IO.Path]::GetFullPath($work)
    if (-not $target.StartsWith($tempBase + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $target) -notlike 'aicf-workshop-names-*') { throw 'Unsafe fixture cleanup path' }
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
}
"Workshop scenario names contracts: PASS; cases=$checks"
exit 0
