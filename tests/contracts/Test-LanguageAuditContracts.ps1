param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $RepositoryRoot 'tests/lib/Stage3StaticAudit.Common.ps1')
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]"\/")
$fixtureRoot = Join-Path $tempBase ('aicf-language-audit-' + [guid]::NewGuid().ToString('N'))
$sourceRoot = Join-Path $fixtureRoot 'AIConflictCore/Scripts'
[void](New-Item -ItemType Directory -Path $sourceRoot -Force)
$script:caseCount = 0
function Check-LanguageCase([string]$Name, [string]$Source, [string[]]$ExpectedRules) {
    [IO.File]::WriteAllText((Join-Path $sourceRoot 'AICF_LanguageFixture.c'), $Source)
    $failures = @(Invoke-AICFLanguageAudit -RepositoryRoot $fixtureRoot)
    $actualRules = @($failures | ForEach-Object { if ($_ -match '^\[([^\]]+)\]') { $Matches[1] } } | Sort-Object)
    $expected = @($ExpectedRules | Sort-Object)
    if (($actualRules -join ',') -ne ($expected -join ',')) {
        throw "FAIL $Name expected=$($expected -join ',') actual=$($failures -join '; ')"
    }
    $script:caseCount++
    "PASS case=$Name rules=$($actualRules -join ',')"
}
try {
    Check-LanguageCase 'percent-encoding' 'class AICF_Test { void Encode(string name) { name.Replace("%", "%25"); name.Replace("%25", "%"); } }' @()
    Check-LanguageCase 'plain-data' 'class AICF_Test { void Run() { Print("%25"); } }' @()
    Check-LanguageCase 'format-data-argument' 'class AICF_Test { void Run() { string.Format("%1", "%25"); } }' @()
    Check-LanguageCase 'placeholder-ten' 'class AICF_Test { void Run() { string.Format("%10", 0); } }' @('STRING_FORMAT_PLACEHOLDER')
    Check-LanguageCase 'placeholder-ninety-nine' 'class AICF_Test { void Run() { string.Format("%99", 0); } }' @('STRING_FORMAT_PLACEHOLDER')
    Check-LanguageCase 'nine-supported' 'class AICF_Test { void Run() { string.Format("%1%2%3%4%5%6%7%8%9", 1,2,3,4,5,6,7,8,9); } }' @()
    Check-LanguageCase 'ten-arguments' 'class AICF_Test { void Run() { string.Format("%1", 1,2,3,4,5,6,7,8,9,10); } }' @('STRING_FORMAT_ARITY')
    Check-LanguageCase 'nested-invalid-format' 'class AICF_Test { void Run() { string.Format("%1", string.Format("%10", 0)); } }' @('STRING_FORMAT_PLACEHOLDER')
    Check-LanguageCase 'wrapped-format' 'class AICF_Test { void Run() { string.Format(("%10"), 0); } }' @('STRING_FORMAT_PLACEHOLDER')
    Check-LanguageCase 'comma-in-format' 'class AICF_Test { void Run() { string.Format("a,b %10", 0); } }' @('STRING_FORMAT_PLACEHOLDER')
    Check-LanguageCase 'comment-decoy' "class AICF_Test { void Run() { /* string.Format(`"%10`",0); */ string.Format(`"%1`",0); } }" @()
    Check-LanguageCase 'comment-in-first-argument' 'class AICF_Test { void Run() { string.Format(/* "%25" */ "%1", 0); } }' @()
    Check-LanguageCase 'line-comment-in-first-argument' "class AICF_Test { void Run() { string.Format(// `"%25`"`n`"%1`", 0); } }" @()
    Check-LanguageCase 'escaped-quote' 'class AICF_Test { void Run() { string.Format("quote \" %10", 0); } }' @('STRING_FORMAT_PLACEHOLDER')
    Check-LanguageCase 'concatenated-format' 'class AICF_Test { void Run() { string.Format("prefix" + "%10", 0); } }' @('STRING_FORMAT_PLACEHOLDER')
    Check-LanguageCase 'nested-data-with-comma' 'class AICF_Test { void Run() { string.Format("%1", Pair("%25", 1)); } }' @()
    Check-LanguageCase 'unclosed-call' 'class AICF_Test { void Run() { string.Format("%1", 0;' @('STRING_FORMAT_ARITY')
    "Language audit contracts: PASS; cases=$script:caseCount"
}
finally {
    $resolved = [IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolved.StartsWith($tempBase + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe fixture cleanup path' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
