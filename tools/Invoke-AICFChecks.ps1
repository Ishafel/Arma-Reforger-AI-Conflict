[CmdletBinding()]
param(
    [ValidateSet('All', 'Static', 'Contracts')]
    [string]$Suite = 'All',
    [string[]]$Name = @(),
    [switch]$List,
    [string]$EvidenceRoot
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$checks = @(foreach ($directory in @('static', 'contracts')) {
    if ($Suite -ne 'All' -and $Suite -ne $directory) { continue }
    Get-ChildItem -LiteralPath (Join-Path $repository "tests/$directory") -Filter 'Test-*.ps1' -File
})
$checks = @($checks | Sort-Object Name)
if ($Name.Count) {
    foreach ($requested in $Name) {
        if ($requested -notin $checks.Name) { throw "Unknown check in suite ${Suite}: $requested (use -List)" }
    }
    $checks = @($checks | Where-Object { $_.Name -in $Name })
}
if (-not $checks.Count) { throw 'No checks selected' }
if ($List) {
    $checks | ForEach-Object { $_.FullName.Substring($repository.Length + 1).Replace('\', '/') }
    return
}
$checkShell = (Get-Command powershell.exe -CommandType Application -ErrorAction Stop).Source

# Каждый audit выполняется в отдельном процессе: его exit не прерывает матрицу.
# Runtime, Workbench, watchdog и анализаторы внешних логов сюда не входят.
if (-not $EvidenceRoot) {
    $EvidenceRoot = Join-Path $repository ('.codex-runtime/checks-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
}
$evidence = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
if (Test-Path -LiteralPath $evidence) { throw "EvidenceRoot must be new: $evidence" }
[void](New-Item -ItemType Directory -Path $evidence)
$results = @()
Push-Location $repository
try {
    $commit = & git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read source commit' }
    $status = @(& git status --short --branch)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read source status' }
    [ordered]@{
        commit = $commit
        status = $status
        started_at = (Get-Date).ToString('o')
        repository = $repository
        suite = $Suite
        names = @($checks.Name)
        shell = $checkShell
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'context.json') -Encoding UTF8
    foreach ($check in $checks) {
        $log = Join-Path $evidence ($check.Name + '.txt')
        $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $check.FullName)
        $started = Get-Date
        # Continue сохраняет native stderr в полном логе и позволяет записать exit code.
        $ErrorActionPreference = 'Continue'
        $global:LASTEXITCODE = $null
        & $checkShell @arguments *> $log
        $code = $global:LASTEXITCODE
        if ($null -eq $code) { $code = -1 }
        $ErrorActionPreference = 'Stop'
        $verdict = 'PASS'
        if ($code -ne 0) { $verdict = 'FAIL' }
        $results += [pscustomobject][ordered]@{
            name = $check.Name
            path = $check.FullName.Substring($repository.Length + 1).Replace('\', '/')
            command = @($checkShell) + $arguments
            exit_code = $code
            verdict = $verdict
            started_at = $started.ToString('o')
            finished_at = (Get-Date).ToString('o')
            log = $log
        }
        ConvertTo-Json -InputObject @($results) -Depth 5 | Set-Content -LiteralPath (Join-Path $evidence 'summary.json') -Encoding UTF8
        Write-Output "$verdict $($check.Name) exit=$code"
    }
}
finally {
    Pop-Location
}
Write-Output "AICF_CHECKS_EVIDENCE=$evidence"
if (@($results | Where-Object exit_code -NE 0).Count) { exit 1 }
exit 0
