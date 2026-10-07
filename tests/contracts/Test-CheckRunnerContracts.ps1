$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$evidenceBase = [IO.Path]::GetFullPath((Join-Path $repository '.codex-runtime'))
$evidence = Join-Path $evidenceBase ('runner-contracts-' + [guid]::NewGuid().ToString('N'))
$fixture = Join-Path $evidence 'source'
$utf8 = [Text.UTF8Encoding]::new($true)
foreach ($directory in @('tools', 'tests/static', 'tests/contracts', 'tests/log-audits')) {
    [void](New-Item -ItemType Directory -Path (Join-Path $fixture $directory) -Force)
}
Copy-Item -LiteralPath (Join-Path $repository 'tools/Invoke-AICFChecks.ps1') -Destination (Join-Path $fixture 'tools')
function Write-Fixture([string]$Path, [string]$Source) {
    [IO.File]::WriteAllText((Join-Path $fixture $Path), $Source, $utf8)
}
Write-Fixture 'tests/static/Test-AFail.ps1' 'Write-Output "EXPECTED_FAILURE"; exit 7'
Write-Fixture 'tests/static/Test-BPass.ps1' 'Write-Output "EXPECTED_SUCCESS"; exit 0'
Write-Fixture 'tests/contracts/Test-CContract.ps1' 'Write-Output "EXPECTED_CONTRACT"; exit 0'
Write-Fixture 'tests/log-audits/Test-ExcludedLog.ps1' 'throw "Log analyzers must never run automatically"'
$runner = Join-Path $fixture 'tools/Invoke-AICFChecks.ps1'
function Run-Case([string]$Name, [string[]]$Arguments, [int]$ExpectedExit) {
    $ErrorActionPreference = 'Continue'
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner @Arguments 2>&1
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $output | Set-Content -LiteralPath (Join-Path $evidence "$Name.txt") -Encoding UTF8
    if ($code -ne $ExpectedExit) { throw "${Name}: exit=$code expected=$ExpectedExit; $output" }
    Write-Host "PASS $Name exit=$code"
    return ($output -join "`n")
}
try {
    & git -C $fixture init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Fixture git init failed' }
    & git -C $fixture -c user.name=AICF -c user.email=aicf@example.invalid -c commit.gpgsign=false -c core.hooksPath="$fixture/.no-hooks" commit --allow-empty --quiet -m fixture
    if ($LASTEXITCODE -ne 0) { throw 'Fixture git commit failed' }
    $listEvidence = Join-Path $evidence 'must-not-exist'
    $listing = Run-Case 'list' @('-List', '-EvidenceRoot', $listEvidence) 0
    if ($listing -notmatch 'Test-AFail' -or $listing -notmatch 'Test-CContract' -or $listing -match 'ExcludedLog' -or (Test-Path $listEvidence)) { throw 'List selection or side effects invalid' }
    [void](Run-Case 'unknown' @('-Name', 'Test-Missing.ps1', '-EvidenceRoot', $listEvidence) 1)
    if (Test-Path $listEvidence) { throw 'Unknown check created evidence' }
    [void](Run-Case 'all' @('-EvidenceRoot', (Join-Path $evidence 'all')) 1)
    $summary = Get-Content (Join-Path $evidence 'all/summary.json') -Raw | ConvertFrom-Json
    if ($summary.Count -ne 3 -or $summary[0].exit_code -ne 7 -or $summary[1].exit_code -ne 0 -or $summary[2].exit_code -ne 0) { throw 'Runner failed to preserve exits or continue after failure' }
    foreach ($row in $summary) {
        if (-not (Test-Path -LiteralPath $row.log) -or -not $row.command -or -not $row.started_at -or -not $row.finished_at) { throw 'Incomplete evidence' }
    }
    $context = Get-Content (Join-Path $evidence 'all/context.json') -Raw | ConvertFrom-Json
    if ($context.commit -notmatch '^[0-9a-f]{40}$' -or $context.repository -ne $fixture) { throw 'Wrong repository context' }
    [void](Run-Case 'contracts' @('-Suite', 'Contracts', '-EvidenceRoot', (Join-Path $evidence 'contracts')) 0)
    [void](Run-Case 'selection' @('-Name', 'Test-BPass.ps1', '-EvidenceRoot', (Join-Path $evidence 'selection')) 0)
    Write-Fixture 'tools/Invoke-Nested.ps1' 'param($Runner, $EvidenceRoot) & $Runner -Name Test-BPass.ps1 -EvidenceRoot $EvidenceRoot; exit $LASTEXITCODE'
    foreach ($shellName in @('powershell.exe', 'pwsh.exe')) {
        $shell = Get-Command $shellName -CommandType Application -ErrorAction SilentlyContinue
        if (-not $shell) { Write-Host "NOT RUN nested invocation: $shellName unavailable"; continue }
        $nestedEvidence = Join-Path $evidence ("nested-$shellName")
        & $shell.Source -NoProfile -ExecutionPolicy Bypass -File (Join-Path $fixture 'tools/Invoke-Nested.ps1') -Runner $runner -EvidenceRoot $nestedEvidence *> (Join-Path $evidence "nested-$shellName.txt")
        if ($LASTEXITCODE -ne 0) { throw "Nested invocation lost native exit code: $shellName" }
        $nestedSummary = Get-Content (Join-Path $nestedEvidence 'summary.json') -Raw | ConvertFrom-Json
        if ($nestedSummary[0].exit_code -ne 0) { throw "Nested invocation reported false FAIL: $shellName" }
        Write-Host "PASS nested invocation: $shellName"
    }
    $before = (Get-FileHash (Join-Path $evidence 'all/summary.json')).Hash
    [void](Run-Case 'no-overwrite' @('-EvidenceRoot', (Join-Path $evidence 'all')) 1)
    if ((Get-FileHash (Join-Path $evidence 'all/summary.json')).Hash -ne $before) { throw 'Existing evidence overwritten' }
}
finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    if (-not $resolved.StartsWith($evidenceBase + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -ne 'source') { throw "Unsafe cleanup: $resolved" }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
Write-Output "Check runner contracts: PASS; evidence=$evidence"
