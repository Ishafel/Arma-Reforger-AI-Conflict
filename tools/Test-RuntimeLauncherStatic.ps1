param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$testRoot = $null

function Add-Failure {
    param([string]$Rule, [string]$Message)
    $failures.Add("[$Rule] $Message")
}

function Get-ManifestFromOutput {
    param(
        [string[]]$Output,
        [string]$Rule
    )

    $line = @($Output | Where-Object { $_ -like 'AICF_RUNTIME_MANIFEST_JSON=*' })
    if ($line.Count -ne 1) {
        Add-Failure $Rule "Expected exactly one manifest line, got $($line.Count)"
        return $null
    }

    try {
        return $line[0].Substring('AICF_RUNTIME_MANIFEST_JSON='.Length) | ConvertFrom-Json
    }
    catch {
        Add-Failure $Rule "Manifest is not valid JSON: $($_.Exception.Message)"
        return $null
    }
}

function Require-ArgumentPair {
    param(
        [object]$Manifest,
        [string]$Name,
        [string]$ExpectedValue,
        [string]$Rule
    )

    $arguments = @($Manifest.arguments)
    $indices = @()
    for ($index = 0; $index -lt $arguments.Count; $index++) {
        if ($arguments[$index] -ceq $Name) {
            $indices += $index
        }
    }

    if ($indices.Count -ne 1) {
        Add-Failure $Rule "Expected one $Name argument, got $($indices.Count)"
        return
    }

    $valueIndex = $indices[0] + 1
    if ($valueIndex -ge $arguments.Count -or $arguments[$valueIndex] -cne $ExpectedValue) {
        Add-Failure $Rule "Unexpected $Name value. Expected '$ExpectedValue', got '$($arguments[$valueIndex])'"
    }
}

$launcherPath = Join-Path $RepositoryRoot 'tools/Start-AICFRuntime.ps1'
if (-not (Test-Path -LiteralPath $launcherPath -PathType Leaf)) {
    Add-Failure 'RUNTIME_LAUNCHER_FILE' "Missing launcher $launcherPath"
}
else {
    $launcherSource = Get-Content -LiteralPath $launcherPath -Raw
    if ($launcherSource -match '(?i)Start-Process') {
        Add-Failure 'RUNTIME_LAUNCHER_DIRECT_INVOCATION' 'Launcher must not use Start-Process argument reserialization'
    }
    if ($launcherSource -notmatch '&\s+\$executable\s+@nativeArguments') {
        Add-Failure 'RUNTIME_LAUNCHER_DIRECT_INVOCATION' 'Launcher must invoke the executable directly with array splatting'
    }
    foreach ($requiredContract in @(
        'CLI Params:', '[AICF][STAGE1][INFO][ROSTER_READY]', 'Get-NetUDPEndpoint',
        '-Encoding UTF8', 'Wait-AICFNewClientProcess', 'WaitForExit()',
        'PROCESS_OBSERVED_NO_NATIVE_CODE'
    )) {
        if (-not $launcherSource.Contains($requiredContract)) {
            Add-Failure 'RUNTIME_LAUNCHER_READY_GATE' "Launcher omits readiness contract $requiredContract"
        }
    }
}

try {
    $testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('AICF Launcher Тест ' + [Guid]::NewGuid().ToString('N'))
    $fakeRepository = Join-Path $testRoot 'Репозиторий с пробелами'
    $fakeServerRoot = Join-Path $testRoot 'Program Files (x86)\Arma Reforger Server'
    $fakeGameRoot = Join-Path $testRoot 'Program Files (x86)\Arma Reforger'
    $fakeRhsRoot = Join-Path $testRoot 'OneDrive\Документы\My Games\ArmaReforger\addons'

    foreach ($directory in @(
        $fakeRepository,
        (Join-Path $fakeRepository 'AIConflictArland'),
        (Join-Path $fakeRepository 'AIConflictArlandRHS'),
        (Join-Path $fakeServerRoot 'addons'),
        (Join-Path $fakeGameRoot 'addons'),
        $fakeRhsRoot
    )) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    foreach ($file in @(
        (Join-Path $fakeRepository 'AIConflictArland\addon.gproj'),
        (Join-Path $fakeRepository 'AIConflictArlandRHS\addon.gproj'),
        (Join-Path $fakeServerRoot 'ArmaReforgerServerDiag.exe'),
        (Join-Path $fakeGameRoot 'ArmaReforgerSteamDiag.exe')
    )) {
        New-Item -ItemType File -Path $file -Force | Out-Null
    }

    $rhsProfile = Join-Path $testRoot 'Profiles\Server RHS новый'
    $serverInvocation = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $launcherPath,
        '-Role', 'Server', '-Variant', 'RHS',
        '-RepositoryRoot', $fakeRepository,
        '-ServerRoot', $fakeServerRoot,
        '-GameRoot', $fakeGameRoot,
        '-RhsAddonsRoot', $fakeRhsRoot,
        '-ProfileRoot', $rhsProfile,
        '-AICommanderMode', 'USSR',
        '-DryRun'
    )
    $serverOutput = @(& powershell.exe @serverInvocation 2>&1 | ForEach-Object { $_.ToString() })
    $serverExitCode = $LASTEXITCODE
    if ($serverExitCode -ne 0) {
        Add-Failure 'RUNTIME_LAUNCHER_RHS_DRY_RUN' "RHS server dry-run exited $serverExitCode`: $($serverOutput -join ' | ')"
    }
    else {
        $serverManifest = Get-ManifestFromOutput -Output $serverOutput -Rule 'RUNTIME_LAUNCHER_RHS_DRY_RUN'
        if ($serverManifest) {
            $expectedServerAddonsDir = "$fakeRepository,$fakeServerRoot\addons,$fakeRhsRoot"
            Require-ArgumentPair $serverManifest '-addonsDir' $expectedServerAddonsDir 'RUNTIME_LAUNCHER_ARGUMENT_INTEGRITY'
            Require-ArgumentPair $serverManifest '-gproj' (Join-Path $fakeRepository 'AIConflictArlandRHS\addon.gproj') 'RUNTIME_LAUNCHER_RHS_GRAPH'
            Require-ArgumentPair $serverManifest '-aicfAICommanderMode' 'USSR' 'RUNTIME_LAUNCHER_COMMAND_MODE'
            Require-ArgumentPair $serverManifest '-profile' $rhsProfile 'RUNTIME_LAUNCHER_FRESH_PROFILE'
            if ($serverManifest.addonsDir -cne $expectedServerAddonsDir) {
                Add-Failure 'RUNTIME_LAUNCHER_ARGUMENT_INTEGRITY' 'Manifest addonsDir does not preserve spaces and Cyrillic as one value'
            }
        }
    }

    $stockProfile = Join-Path $testRoot 'Profiles\Server Stock новый'
    $stockInvocation = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $launcherPath,
        '-Role', 'Server', '-Variant', 'Stock',
        '-RepositoryRoot', $fakeRepository,
        '-ServerRoot', $fakeServerRoot,
        '-ProfileRoot', $stockProfile,
        '-UseDefaultAICommanderMode',
        '-DryRun'
    )
    $stockOutput = @(& powershell.exe @stockInvocation 2>&1 | ForEach-Object { $_.ToString() })
    $stockExitCode = $LASTEXITCODE
    if ($stockExitCode -ne 0) {
        Add-Failure 'RUNTIME_LAUNCHER_STOCK_DRY_RUN' "Stock server dry-run exited $stockExitCode`: $($stockOutput -join ' | ')"
    }
    else {
        $stockManifest = Get-ManifestFromOutput -Output $stockOutput -Rule 'RUNTIME_LAUNCHER_STOCK_DRY_RUN'
        if ($stockManifest) {
            $expectedStockAddonsDir = "$fakeRepository,$fakeServerRoot\addons"
            Require-ArgumentPair $stockManifest '-addonsDir' $expectedStockAddonsDir 'RUNTIME_LAUNCHER_ARGUMENT_INTEGRITY'
            Require-ArgumentPair $stockManifest '-gproj' (Join-Path $fakeRepository 'AIConflictArland\addon.gproj') 'RUNTIME_LAUNCHER_STOCK_GRAPH'
            if (@($stockManifest.arguments) -ccontains '-aicfAICommanderMode') {
                Add-Failure 'RUNTIME_LAUNCHER_COMMAND_MODE' 'Default-mode dry-run must omit -aicfAICommanderMode'
            }
        }
    }

    $clientProfile = Join-Path $testRoot 'Profiles\Client RHS новый'
    $clientInvocation = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $launcherPath,
        '-Role', 'Client', '-Variant', 'RHS',
        '-RepositoryRoot', $fakeRepository,
        '-ServerRoot', $fakeServerRoot,
        '-GameRoot', $fakeGameRoot,
        '-RhsAddonsRoot', $fakeRhsRoot,
        '-ProfileRoot', $clientProfile,
        '-ClientAddress', '127.0.0.1',
        '-DryRun'
    )
    $clientOutput = @(& powershell.exe @clientInvocation 2>&1 | ForEach-Object { $_.ToString() })
    $clientExitCode = $LASTEXITCODE
    if ($clientExitCode -ne 0) {
        Add-Failure 'RUNTIME_LAUNCHER_CLIENT_DRY_RUN' "RHS client dry-run exited $clientExitCode`: $($clientOutput -join ' | ')"
    }
    else {
        $clientManifest = Get-ManifestFromOutput -Output $clientOutput -Rule 'RUNTIME_LAUNCHER_CLIENT_DRY_RUN'
        if ($clientManifest) {
            $expectedClientAddonsDir = "$fakeRepository,$fakeGameRoot\addons,$fakeRhsRoot"
            Require-ArgumentPair $clientManifest '-addonsDir' $expectedClientAddonsDir 'RUNTIME_LAUNCHER_ARGUMENT_INTEGRITY'
            Require-ArgumentPair $clientManifest '-client' '127.0.0.1' 'RUNTIME_LAUNCHER_CLIENT_TARGET'
        }
    }

    $existingProfile = Join-Path $testRoot 'Profiles\Already exists'
    New-Item -ItemType Directory -Path $existingProfile -Force | Out-Null
    $existingProfileInvocation = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $launcherPath,
        '-Role', 'Server', '-Variant', 'Stock',
        '-RepositoryRoot', $fakeRepository,
        '-ServerRoot', $fakeServerRoot,
        '-ProfileRoot', $existingProfile,
        '-DryRun'
    )
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'SilentlyContinue'
    $null = & powershell.exe @existingProfileInvocation 2>&1
    $ErrorActionPreference = $previousErrorActionPreference
    if ($LASTEXITCODE -eq 0) {
        Add-Failure 'RUNTIME_LAUNCHER_FRESH_PROFILE' 'Launcher accepted an existing runtime profile'
    }
}
catch {
    Add-Failure 'RUNTIME_LAUNCHER_TEST_HARNESS' $_.Exception.Message
}
finally {
    if ($testRoot -and
        $testRoot.StartsWith([System.IO.Path]::GetTempPath(), [System.StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $testRoot)) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Output "[AICF][RUNTIME_LAUNCHER_STATIC][FAIL] $failure"
    }
    Write-Output "[AICF][RUNTIME_LAUNCHER_STATIC][RESULT][FAIL] issues=$($failures.Count)"
    exit 1
}

Write-Output '[AICF][RUNTIME_LAUNCHER_STATIC][RESULT][PASS] direct_invocation=PASS argument_integrity=PASS spaces=PASS cyrillic=PASS stock_rhs=PASS fresh_profile=PASS ready_gate=PASS'
exit 0
