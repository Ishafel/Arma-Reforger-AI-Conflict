param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [switch]$Check
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AICFLocalization.Common.ps1')
$entries = Read-AICFLocalization $RepositoryRoot
foreach ($language in @('en_us', 'ru_ru')) {
    $lines = @('StringTableRuntime', '{', "`tIds", "`t{")
    $lines += @($entries.Keys | ForEach-Object { "`t`t`"$_`"" })
    $lines += @("`t}", "`tTexts", "`t{")
    $lines += @($entries.Keys | ForEach-Object { "`t`t`"$($entries[$_][$language])`"" })
    $lines += @("`t}", '}')
    $text = ($lines -join "`n") + "`n"
    $path = Join-Path $RepositoryRoot "AIConflictCore/Language/AICF_Localization.$language.conf"
    if ($Check) {
        $actual = [IO.File]::ReadAllText($path).Replace("`r`n", "`n")
        if ($actual -cne $text) { throw "Stale runtime table: $path" }
    } else {
        [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($false))
    }
}
Write-Output "PASS Localization runtime tables: $($entries.Count) entries, en_us/ru_ru, check=$Check"
