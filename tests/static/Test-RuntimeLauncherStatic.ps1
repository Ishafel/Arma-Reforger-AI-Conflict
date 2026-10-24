param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
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
        '[Text.Encoding]::UTF8', '[IO.FileShare]::ReadWrite', 'Wait-AICFNewClientProcess', 'WaitForExit()',
        'PROCESS_OBSERVED_NO_NATIVE_CODE', 'Assert-AICFResourceDatabases -Databases $resourceDatabases'
    )) {
        if (-not $launcherSource.Contains($requiredContract)) {
            Add-Failure 'RUNTIME_LAUNCHER_READY_GATE' "Launcher omits readiness contract $requiredContract"
        }
    }
    # Проверяем поведение preflight отдельно, не запуская native binary.
    $parseTokens = $null
    $parseErrors = $null
    $launcherAst = [Management.Automation.Language.Parser]::ParseInput($launcherSource, [ref]$parseTokens, [ref]$parseErrors)
    $databaseGuard = $launcherAst.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Assert-AICFResourceDatabases'
    }, $true)
    if (-not $databaseGuard) {
        Add-Failure 'RUNTIME_RESOURCE_DATABASE' 'Missing runtime database preflight'
    }
    else {
        . ([scriptblock]::Create($databaseGuard.Extent.Text))
        foreach ($case in @(
            @{name='missing'; exists=$false; bytes=0; reject=$true},
            @{name='empty'; exists=$true; bytes=0; reject=$true},
            @{name='present'; exists=$true; bytes=32; reject=$false}
        )) {
            $rejected = $false
            try { Assert-AICFResourceDatabases @([pscustomobject]@{path=$case.name; exists=$case.exists; bytes=$case.bytes}) }
            catch { $rejected = $_.Exception.Message -like 'Source resource database*' }
            if ($rejected -ne $case.reject) { Add-Failure 'RUNTIME_RESOURCE_DATABASE' "Unexpected preflight result: $($case.name)" }
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
        (Join-Path $fakeRepository 'AIConflictEveron'),
        (Join-Path $fakeRepository 'AIConflictArlandRHS'),
        (Join-Path $fakeRepository 'AIConflictEveronRHS'),
        (Join-Path $fakeServerRoot 'addons'),
        (Join-Path $fakeGameRoot 'addons'),
        $fakeRhsRoot
    )) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    foreach ($file in @(
        (Join-Path $fakeRepository 'AIConflictArland\addon.gproj'),
        (Join-Path $fakeRepository 'AIConflictEveron\addon.gproj'),
        (Join-Path $fakeRepository 'AIConflictArlandRHS\addon.gproj'),
        (Join-Path $fakeRepository 'AIConflictEveronRHS\addon.gproj'),
        (Join-Path $fakeServerRoot 'ArmaReforgerServerDiag.exe'),
        (Join-Path $fakeGameRoot 'ArmaReforgerSteam.exe'),
        (Join-Path $fakeGameRoot 'ArmaReforgerSteamDiag.exe')
    )) {
        New-Item -ItemType File -Path $file -Force | Out-Null
    }

    $readerFunction = $launcherAst.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Read-AICFConsoleSnapshot'
    }, $true)
    if (-not $readerFunction) { Add-Failure 'RUNTIME_LOG_SNAPSHOT' 'Missing UTF-8 snapshot reader' }
    else {
        . ([scriptblock]::Create($readerFunction.Extent.Text))
        $activeLog = Join-Path $testRoot 'active-console.log'
        $writerStream = [IO.File]::Open($activeLog, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
        $writer = [IO.StreamWriter]::new($writerStream, [Text.UTF8Encoding]::new($false))
        try {
            $writer.AutoFlush = $true
            $writer.WriteLine('CLI Params: Кириллица и пробелы')
            $snapshot = Read-AICFConsoleSnapshot $activeLog
            if ($snapshot -isnot [string] -or -not $snapshot.Contains('Кириллица и пробелы') -or $snapshot.Contains('[ROSTER_READY]')) {
                Add-Failure 'RUNTIME_LOG_SNAPSHOT' 'Incomplete log snapshot was misread'
            }
            $writer.WriteLine('[AICF][STAGE1][INFO][ROSTER_READY]')
            $snapshot = Read-AICFConsoleSnapshot $activeLog
            if ($snapshot -isnot [string] -or -not $snapshot.Contains('[AICF][STAGE1][INFO][ROSTER_READY]')) {
                Add-Failure 'RUNTIME_LOG_SNAPSHOT' 'Growing log snapshot lost readiness'
            }
        }
        finally { $writer.Dispose() }
    }

    $missingDatabaseInvocation = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $launcherPath,
        '-Role', 'Server', '-Variant', 'Stock', '-RepositoryRoot', $fakeRepository,
        '-ServerRoot', $fakeServerRoot, '-ProfileRoot', (Join-Path $testRoot 'Profiles\Missing database')
    )
    $databaseErrorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $missingDatabaseOutput = @(& powershell.exe @missingDatabaseInvocation 2>&1 | ForEach-Object { $_.ToString() })
        $missingDatabaseExit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $databaseErrorPreference }
    if ($missingDatabaseExit -eq 0 -or ($missingDatabaseOutput -join ' ') -notmatch 'Source resource database') {
        Add-Failure 'RUNTIME_RESOURCE_DATABASE' 'Real launch did not reject absent indexes before native invocation'
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
            Require-ArgumentPair $serverManifest '-server' '{7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf' 'RUNTIME_LAUNCHER_RHS_SCENARIO'
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

    $everonProfile = Join-Path $testRoot 'Profiles\Server Everon новый'
    $everonInvocation = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $launcherPath,
        '-Role', 'Server', '-Variant', 'Everon',
        '-RepositoryRoot', $fakeRepository,
        '-ServerRoot', $fakeServerRoot,
        '-ProfileRoot', $everonProfile,
        '-AICommanderMode', 'BOTH',
        '-DryRun'
    )
    $everonOutput = @(& powershell.exe @everonInvocation 2>&1 | ForEach-Object { $_.ToString() })
    $everonExitCode = $LASTEXITCODE
    if ($everonExitCode -ne 0) {
        Add-Failure 'RUNTIME_LAUNCHER_EVERON_DRY_RUN' "Everon server dry-run exited $everonExitCode`: $($everonOutput -join ' | ')"
    }
    else {
        $everonManifest = Get-ManifestFromOutput -Output $everonOutput -Rule 'RUNTIME_LAUNCHER_EVERON_DRY_RUN'
        if ($everonManifest) {
            Require-ArgumentPair $everonManifest '-gproj' (Join-Path $fakeRepository 'AIConflictEveron\addon.gproj') 'RUNTIME_LAUNCHER_EVERON_GRAPH'
            Require-ArgumentPair $everonManifest '-server' '{ECC61978EDCC2B5A}Missions/23_Campaign.conf' 'RUNTIME_LAUNCHER_EVERON_WORLD'
            Require-ArgumentPair $everonManifest '-MissionHeader' '{ECC61978EDCC2B5A}Missions/23_Campaign.conf' 'RUNTIME_LAUNCHER_EVERON_HEADER'
            Require-ArgumentPair $everonManifest '-addons' '9178E5822AFE48EA,B52C5F6AEDBF423E,A4B2E62595F645A4' 'RUNTIME_LAUNCHER_EVERON_GRAPH'
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
            Require-ArgumentPair $stockManifest '-server' '{C41618FD18E9D714}Missions/23_Campaign_Arland.conf' 'RUNTIME_LAUNCHER_STOCK_SCENARIO'
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
            if ($clientManifest.executable -ne (Join-Path $fakeGameRoot 'ArmaReforgerSteamDiag.exe')) { Add-Failure 'CLIENT_DIAG_DEFAULT' 'Direct-connect client must remain compatible with Diag server' }
            $expectedClientAddonsDir = "$fakeRepository,$fakeGameRoot\addons,$fakeRhsRoot"
            Require-ArgumentPair $clientManifest '-addonsDir' $expectedClientAddonsDir 'RUNTIME_LAUNCHER_ARGUMENT_INTEGRITY'
            Require-ArgumentPair $clientManifest '-client' '127.0.0.1' 'RUNTIME_LAUNCHER_CLIENT_TARGET'
        }
    }

    foreach ($frontendVariant in @('Stock','RHS')) {
        $frontendInvocation = @(
            '-NoProfile','-ExecutionPolicy','Bypass','-File',$launcherPath,
            '-Role','Frontend','-Variant',$frontendVariant,'-RepositoryRoot',$fakeRepository,
            '-ServerRoot',(Join-Path $testRoot 'Server not installed'),'-GameRoot',$fakeGameRoot,
            '-RhsAddonsRoot',$fakeRhsRoot,'-ProfileRoot',(Join-Path $testRoot "Profiles/Frontend-$frontendVariant"),'-DryRun'
        )
        $frontendOutput = @(& powershell.exe @frontendInvocation 2>&1 | ForEach-Object { $_.ToString() })
        if ($LASTEXITCODE -ne 0) { Add-Failure 'FRONTEND_PLAN' ($frontendOutput -join ' | '); continue }
        $frontendManifest = Get-ManifestFromOutput $frontendOutput 'FRONTEND_PLAN'
        if (!$frontendManifest) { continue }
        if ($frontendManifest.role -ne 'Frontend' -or $frontendManifest.executable -ne (Join-Path $fakeGameRoot 'ArmaReforgerSteam.exe')) { Add-Failure 'FRONTEND_RETAIL' 'Local frontend must use retail client' }
        if (@($frontendManifest.arguments | Where-Object { $_ -in @('-client','-server','-MissionHeader','-world') }).Count) { Add-Failure 'FRONTEND_LOCAL' 'Frontend contains connection or world startup flags' }
        Require-ArgumentPair $frontendManifest '-addonsDir' "$fakeRepository,$fakeGameRoot\addons,$fakeRhsRoot" 'FRONTEND_SCAN_ROOT'
        foreach ($connectionCase in @(
            @('-ClientAddress','127.0.0.1'), @('-ServerPort','2001'), @('-ServerProfileRoot',$fakeServerRoot),
            @('-RemoteReadinessProbe','unused.ps1'), @('-ExpectedSourceCommit',('a' * 40))
        )) {
            $previousPreference = $ErrorActionPreference
            $ErrorActionPreference = 'Continue'
            try { $rejected = @(& powershell.exe @frontendInvocation @connectionCase 2>&1 | ForEach-Object { $_.ToString() }); $rejectedExit = $LASTEXITCODE }
            finally { $ErrorActionPreference = $previousPreference }
            if ($rejectedExit -eq 0 -or ($rejected -join ' ') -notmatch 'FRONTEND_CONNECTION_PARAMETER') { Add-Failure 'FRONTEND_CONNECTION_REJECTED' ($connectionCase -join ' ') }
        }
    }

    $worldCommand = "& '" + $launcherPath.Replace("'", "''") + "' -Role Frontend -Variant Stock -AdditionalArguments @('-world') -DryRun"
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $worldOutput = @(& powershell.exe -NoProfile -Command $worldCommand 2>&1 | ForEach-Object { $_.ToString() }); $worldExit = $LASTEXITCODE }
    finally { $ErrorActionPreference = $previousPreference }
    if ($worldExit -eq 0 -or ($worldOutput -join ' ') -notmatch 'FRONTEND_WORLD_PARAMETER') { Add-Failure 'FRONTEND_WORLD_REJECTED' 'Frontend accepted a world override' }

    foreach ($address in @('127.0.0.1', '127.0.0.1:23204')) {
        $portInvocation = @($clientInvocation)
        $addressIndex = [array]::IndexOf($portInvocation, '-ClientAddress') + 1
        $portInvocation[$addressIndex] = $address
        $portInvocation += @('-ServerPort', '23204')
        $portOutput = @(& powershell.exe @portInvocation 2>&1 | ForEach-Object { $_.ToString() })
        if ($LASTEXITCODE -ne 0) {
            Add-Failure 'RUNTIME_LAUNCHER_CLIENT_PORT' "Client port dry-run failed: $($portOutput -join ' | ')"
            continue
        }
        $portManifest = Get-ManifestFromOutput -Output $portOutput -Rule 'RUNTIME_LAUNCHER_CLIENT_PORT'
        if ($portManifest) {
            Require-ArgumentPair $portManifest '-client' '127.0.0.1:23204' 'RUNTIME_LAUNCHER_CLIENT_PORT'
        }
    }
    $conflictingInvocation = @($clientInvocation)
    $conflictingInvocation[[array]::IndexOf($conflictingInvocation, '-ClientAddress') + 1] = '127.0.0.1:23205'
    $conflictingInvocation += @('-ServerPort', '23204')
    # WinPS превращает stderr ожидаемого отказа в NativeCommandError.
    # Сохраняем output/exit и проверяем именно отказ, не прерывая весь harness.
    $portErrorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $conflictingOutput = @(& powershell.exe @conflictingInvocation 2>&1 | ForEach-Object { $_.ToString() })
        $conflictingExit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $portErrorPreference }
    if ($conflictingExit -eq 0 -or ($conflictingOutput -join ' ') -notmatch 'ClientAddress.*ServerPort') {
        Add-Failure 'RUNTIME_LAUNCHER_CLIENT_PORT' 'Client must reject a target port different from the checked server port'
    }

    foreach ($northRole in @('Server', 'Client')) {
        $northInvocation = @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $launcherPath,
            '-Role', $northRole, '-Variant', 'EveronNorth',
            '-RepositoryRoot', $fakeRepository,
            '-ServerRoot', $fakeServerRoot, '-GameRoot', $fakeGameRoot,
            '-RhsAddonsRoot', (Join-Path $testRoot 'RHS-not-installed'), '-DryRun'
        )
        $northOutput = @(& powershell.exe @northInvocation 2>&1 | ForEach-Object { $_.ToString() })
        if ($LASTEXITCODE -ne 0) {
            Add-Failure 'RUNTIME_LAUNCHER_NORTH_STOCK' "$northRole must run without installed RHS: $($northOutput -join ' | ')"
            continue
        }
        $northManifest = Get-ManifestFromOutput -Output $northOutput -Rule 'RUNTIME_LAUNCHER_NORTH_STOCK'
        if (-not $northManifest) { continue }
        Require-ArgumentPair $northManifest '-gproj' (Join-Path $fakeRepository 'AIConflictEveron\addon.gproj') 'RUNTIME_LAUNCHER_NORTH_STOCK'
        Require-ArgumentPair $northManifest '-addons' '9178E5822AFE48EA,B52C5F6AEDBF423E,A4B2E62595F645A4' 'RUNTIME_LAUNCHER_NORTH_STOCK'
        $northDirs = "$fakeRepository,$fakeGameRoot\addons"
        if ($northRole -eq 'Server') {
            $northDirs = "$fakeRepository,$fakeServerRoot\addons"
            Require-ArgumentPair $northManifest '-server' '{ECC61978EDCC2B5A}Missions/23_Campaign.conf' 'RUNTIME_LAUNCHER_NORTH_STOCK'
            Require-ArgumentPair $northManifest '-MissionHeader' '{ECC61978EDCC2B5A}Missions/23_Campaign.conf' 'RUNTIME_LAUNCHER_NORTH_STOCK'
        }
        else {
            Require-ArgumentPair $northManifest '-client' '127.0.0.1' 'RUNTIME_LAUNCHER_NORTH_STOCK'
        }
        Require-ArgumentPair $northManifest '-addonsDir' $northDirs 'RUNTIME_LAUNCHER_NORTH_STOCK'
    }

    foreach ($rhsEveronVariant in @('EveronRHS', 'EveronNorthRHS')) {
        foreach ($rhsEveronRole in @('Server', 'Client')) {
            $rhsEveronProfile = Join-Path $testRoot "Profiles\$rhsEveronRole Everon RHS новый"
            $rhsEveronInvocation = @(
                '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $launcherPath,
                '-Role', $rhsEveronRole, '-Variant', $rhsEveronVariant,
                '-RepositoryRoot', $fakeRepository,
                '-ServerRoot', $fakeServerRoot, '-GameRoot', $fakeGameRoot,
                '-RhsAddonsRoot', $fakeRhsRoot, '-ProfileRoot', $rhsEveronProfile, '-DryRun'
            )
            $rhsEveronOutput = @(& powershell.exe @rhsEveronInvocation 2>&1 | ForEach-Object { $_.ToString() })
            $rhsEveronExitCode = $LASTEXITCODE
            if ($rhsEveronExitCode -ne 0) {
                Add-Failure 'RUNTIME_LAUNCHER_RHS_EVERON_DRY_RUN' "$rhsEveronRole dry-run exited $rhsEveronExitCode`: $($rhsEveronOutput -join ' | ')"
                continue
            }
            $rhsEveronManifest = Get-ManifestFromOutput -Output $rhsEveronOutput -Rule 'RUNTIME_LAUNCHER_RHS_EVERON_DRY_RUN'
            if (-not $rhsEveronManifest) { continue }
            Require-ArgumentPair $rhsEveronManifest '-gproj' (Join-Path $fakeRepository 'AIConflictEveronRHS\addon.gproj') 'RUNTIME_LAUNCHER_RHS_EVERON_GRAPH'
            Require-ArgumentPair $rhsEveronManifest '-addons' '9178E5822AFE48EA,B52C5F6AEDBF423E,A4B2E62595F645A4,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C,FA9FDCCA428A43BA' 'RUNTIME_LAUNCHER_RHS_EVERON_GRAPH'
            Require-ArgumentPair $rhsEveronManifest '-profile' $rhsEveronProfile 'RUNTIME_LAUNCHER_FRESH_PROFILE'
            $expectedRhsEveronDirs = "$fakeRepository,$fakeGameRoot\addons,$fakeRhsRoot"
            if ($rhsEveronRole -eq 'Server') {
                $expectedRhsEveronDirs = "$fakeRepository,$fakeServerRoot\addons,$fakeRhsRoot"
                $expectedScenario = '{AAD43C10045857C1}Missions/RHS_Conflict.conf'
                if ($rhsEveronVariant -eq 'EveronNorthRHS') { $expectedScenario = '{AAD43C10045857C1}Missions/RHS_Conflict.conf' }
                Require-ArgumentPair $rhsEveronManifest '-server' $expectedScenario 'RUNTIME_LAUNCHER_RHS_EVERON_WORLD'
                $expectedHeader = '{AAD43C10045857C1}Missions/RHS_Conflict.conf'
                if ($rhsEveronVariant -eq 'EveronNorthRHS') { $expectedHeader = '{AAD43C10045857C1}Missions/RHS_Conflict.conf' }
                Require-ArgumentPair $rhsEveronManifest '-MissionHeader' $expectedHeader 'RUNTIME_LAUNCHER_RHS_EVERON_HEADER'
                Require-ArgumentPair $rhsEveronManifest '-worldSystemsConfig' 'Configs/Systems/ConflictSystems.conf' 'RUNTIME_LAUNCHER_RHS_EVERON_SYSTEMS'
            }
            else {
                Require-ArgumentPair $rhsEveronManifest '-client' '127.0.0.1' 'RUNTIME_LAUNCHER_CLIENT_TARGET'
            }
            Require-ArgumentPair $rhsEveronManifest '-addonsDir' $expectedRhsEveronDirs 'RUNTIME_LAUNCHER_ARGUMENT_INTEGRITY'
        }
    }

    $wcsArlandRoot = Join-Path $fakeRepository 'AIConflictArlandWCSRHS'
    New-Item -ItemType Directory -Force $wcsArlandRoot | Out-Null
    [IO.File]::WriteAllText((Join-Path $wcsArlandRoot 'addon.gproj'), '')
    $wcsProjectRoot = Join-Path $fakeRepository 'AIConflictEveronWCSRHS'
    New-Item -ItemType Directory -Force $wcsProjectRoot | Out-Null
    [IO.File]::WriteAllText((Join-Path $wcsProjectRoot 'addon.gproj'), '')
    foreach ($variant in @('EveronWCSRHS', 'EveronNorthWCSRHS')) {
        foreach ($role in @('Server', 'Client')) {
            $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $launcherPath `
                -Role $role -Variant $variant -RepositoryRoot $fakeRepository `
                -ServerRoot $fakeServerRoot -GameRoot $fakeGameRoot -RhsAddonsRoot $fakeRhsRoot `
                -ProfileRoot (Join-Path $testRoot "Profiles\$variant-$role") -DryRun 2>&1 | ForEach-Object { $_.ToString() })
            if ($LASTEXITCODE -ne 0) {
                Add-Failure 'RUNTIME_WCS_EVERON' "$variant/$role dry-run failed: $($output -join ' | ')"
                continue
            }
            $manifest = Get-ManifestFromOutput $output 'RUNTIME_WCS_EVERON'
            if (-not $manifest) { continue }
            Require-ArgumentPair $manifest '-gproj' (Join-Path $wcsProjectRoot 'addon.gproj') 'RUNTIME_WCS_EVERON'
            $sourceIds = @($manifest.resourceDatabases.addonId)
            $expectedIds = @('9178E5822AFE48EA', 'B52C5F6AEDBF423E', 'A4B2E62595F645A4', '9F88011DA22B471C', 'FA9FDCCA428A43BA', 'A1CF260928100001', 'A1CF261006100001')
            if (@(Compare-Object $sourceIds $expectedIds).Count) {
                Add-Failure 'RUNTIME_WCS_EVERON_GRAPH' 'WCS Everon must include all seven source resource databases'
            }
            if ($role -eq 'Server') {
                $expected = '{AAD43C10045857C1}Missions/RHS_Conflict.conf'
                if ($variant -eq 'EveronNorthWCSRHS') { $expected = '{AAD43C10045857C1}Missions/RHS_Conflict.conf' }
                Require-ArgumentPair $manifest '-server' $expected 'RUNTIME_WCS_EVERON_HEADER'
                Require-ArgumentPair $manifest '-MissionHeader' $expected 'RUNTIME_WCS_EVERON_HEADER'
            }
            else { Require-ArgumentPair $manifest '-client' '127.0.0.1' 'RUNTIME_WCS_EVERON_CLIENT' }
        }
    }

    foreach ($difficultyCase in @(
        @('Stock', 'Medium', '{C41618FD18E9D714}Missions/23_Campaign_Arland.conf', 'AIConflictArland'),
        @('Stock', 'Hard', '{C41618FD18E9D714}Missions/23_Campaign_Arland.conf', 'AIConflictArland'),
        @('Everon', 'Medium', '{ECC61978EDCC2B5A}Missions/23_Campaign.conf', 'AIConflictEveron'),
        @('Everon', 'Hard', '{ECC61978EDCC2B5A}Missions/23_Campaign.conf', 'AIConflictEveron'),
        @('EveronNorth', 'Medium', '{ECC61978EDCC2B5A}Missions/23_Campaign.conf', 'AIConflictEveron'),
        @('EveronNorth', 'Hard', '{ECC61978EDCC2B5A}Missions/23_Campaign.conf', 'AIConflictEveron'),
        @('RHS', 'Medium', '{7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf', 'AIConflictArlandRHS'),
        @('RHS', 'Hard', '{7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf', 'AIConflictArlandRHS'),
        @('EveronRHS', 'Medium', '{AAD43C10045857C1}Missions/RHS_Conflict.conf', 'AIConflictEveronRHS'),
        @('EveronRHS', 'Hard', '{AAD43C10045857C1}Missions/RHS_Conflict.conf', 'AIConflictEveronRHS'),
        @('EveronNorthRHS', 'Medium', '{AAD43C10045857C1}Missions/RHS_Conflict.conf', 'AIConflictEveronRHS'),
        @('EveronNorthRHS', 'Hard', '{AAD43C10045857C1}Missions/RHS_Conflict.conf', 'AIConflictEveronRHS'),
        @('ArlandWCSRHS', 'Medium', '{7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf', 'AIConflictArlandWCSRHS'),
        @('ArlandWCSRHS', 'Hard', '{7577640CD42A00BD}Missions/RHS_Conflict_Arland.conf', 'AIConflictArlandWCSRHS'),
        @('EveronWCSRHS', 'Medium', '{AAD43C10045857C1}Missions/RHS_Conflict.conf', 'AIConflictEveronWCSRHS'),
        @('EveronWCSRHS', 'Hard', '{AAD43C10045857C1}Missions/RHS_Conflict.conf', 'AIConflictEveronWCSRHS'),
        @('EveronNorthWCSRHS', 'Medium', '{AAD43C10045857C1}Missions/RHS_Conflict.conf', 'AIConflictEveronWCSRHS'),
        @('EveronNorthWCSRHS', 'Hard', '{AAD43C10045857C1}Missions/RHS_Conflict.conf', 'AIConflictEveronWCSRHS')
    )) {
        foreach ($difficultyRole in @('Server', 'Client')) {
            $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $launcherPath `
                -Role $difficultyRole -Variant $difficultyCase[0] -Difficulty $difficultyCase[1] `
                -RepositoryRoot $fakeRepository -ServerRoot $fakeServerRoot -GameRoot $fakeGameRoot `
                -RhsAddonsRoot $fakeRhsRoot -ProfileRoot (Join-Path $testRoot "Profiles/difficulty-$difficultyRole-$($difficultyCase[0])-$($difficultyCase[1])") -DryRun 2>&1 | ForEach-Object { $_.ToString() })
            if ($LASTEXITCODE -ne 0) { Add-Failure 'DIFFICULTY_MANIFEST' ($output -join ' | '); continue }
            $manifest = Get-ManifestFromOutput $output 'DIFFICULTY_MANIFEST'
            if (-not $manifest) { continue }
            if ($manifest.difficulty -ne $difficultyCase[1]) { Add-Failure 'DIFFICULTY_MANIFEST' 'Difficulty was not preserved' }
            $expected = $difficultyCase[2]
            if ($difficultyRole -eq 'Server') {
                Require-ArgumentPair $manifest '-server' $expected 'DIFFICULTY_HEADER'
                Require-ArgumentPair $manifest '-MissionHeader' $expected 'DIFFICULTY_HEADER'
            }
            if($difficultyRole -eq 'Server'){
                $level=1;if($difficultyCase[1] -eq 'Hard'){$level=2}
                Require-ArgumentPair $manifest '-aicfScenarioVariant' $difficultyCase[0] 'RUNTIME_SELECTION'
                Require-ArgumentPair $manifest '-aicfMatchMode' '0' 'RUNTIME_SELECTION'
                Require-ArgumentPair $manifest '-aicfDifficulty' ([string]$level) 'RUNTIME_SELECTION'
            }
        }
    }

    $libraryRoot = Join-Path $testRoot 'Библиотека комплектов'
    New-Item -ItemType Directory -Path $libraryRoot -Force | Out-Null
    foreach ($libraryName in @('template_0.json', 'template_255.json', 'template_0_0.json', 'template_255_1.json', 'template_256.json', 'template_01.json', 'template_1_2.json', 'template_256_0.json', 'unrelated.json')) {
        [IO.File]::WriteAllText((Join-Path $libraryRoot $libraryName), '{}')
    }
    $importInvocation = $serverInvocation + @('-LoadoutLibraryPath', $libraryRoot)
    $importOutput = @(& powershell.exe @importInvocation 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { Add-Failure 'LOADOUT_IMPORT' 'Dry-run импорта не выполнен' }
    else {
        $importManifest = Get-ManifestFromOutput $importOutput 'LOADOUT_IMPORT'
        if ($importManifest -and ((@($importManifest.loadoutLibraryFiles.name) -join ',') -cne 'template_0.json,template_0_0.json,template_255.json,template_255_1.json' -or
            $importManifest.loadoutLibrarySource -cne $libraryRoot)) {
            Add-Failure 'LOADOUT_IMPORT_BOUNDS' 'Импорт допускает только canonical filenames 0..255'
        }
        if ($importManifest -and $importManifest.loadoutLibraryFiles[0].sha256 -cne (Get-FileHash (Join-Path $libraryRoot 'template_0.json') -Algorithm SHA256).Hash) {
            Add-Failure 'LOADOUT_IMPORT_HASH' 'В manifest отсутствует точный hash источника'
        }
        if (Test-Path -LiteralPath $rhsProfile) { Add-Failure 'LOADOUT_IMPORT_DRY_RUN' 'Dry-run создал profile' }
    }
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'SilentlyContinue'
    $null = & powershell.exe @clientInvocation -LoadoutLibraryPath $libraryRoot 2>&1
    $clientImportExit = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    if ($clientImportExit -eq 0) { Add-Failure 'LOADOUT_IMPORT_CLIENT' 'Клиент принял серверную библиотеку' }
    [IO.File]::WriteAllText((Join-Path $libraryRoot 'template_0_0.json'), ('x' * 24577))
    $null = & powershell.exe @importInvocation 2>&1
    if ($LASTEXITCODE -ne 0) { Add-Failure 'LOADOUT_IMPORT_ENVELOPE_SIZE' 'Envelope ошибочно ограничен размером member recipe' }
    [IO.File]::WriteAllText((Join-Path $libraryRoot 'template_0_0.json'), ('x' * 600001))
    $ErrorActionPreference = 'SilentlyContinue'
    $null = & powershell.exe @importInvocation 2>&1
    $oversizeEnvelopeExit = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    if ($oversizeEnvelopeExit -eq 0) { Add-Failure 'LOADOUT_IMPORT_ENVELOPE_BOUND' 'Импорт принял слишком большой envelope' }
    [IO.File]::WriteAllText((Join-Path $libraryRoot 'template_0_0.json'), '{}')
    [IO.File]::WriteAllText((Join-Path $libraryRoot 'template_1.json'), ('x' * 24577))
    $ErrorActionPreference = 'SilentlyContinue'
    $null = & powershell.exe @importInvocation 2>&1
    $oversizeImportExit = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    if ($oversizeImportExit -eq 0) { Add-Failure 'LOADOUT_IMPORT_SIZE' 'Импорт принял слишком большой файл' }

    # Exercise the actual remote gate without launching a game or contacting a server.
    $tokens = $null; $parseErrors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($launcherPath, [ref]$tokens, [ref]$parseErrors)
    $branch = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.IfStatementAst] -and
        $node.Clauses[0].Item1.Extent.Text -eq ('$' + "Role -eq 'Client' -and " + '$RemoteReadinessProbe')
    }, $true)
    if (-not $branch) { throw 'Remote readiness branch not found' }
    $body = $branch.Clauses[0].Item2.Extent.Text
    $gate = [scriptblock]::Create($body.Substring(1, $body.Length - 2))
    $resolver = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Resolve-AICFExistingFile' }, $true)
    . ([scriptblock]::Create($resolver.Extent.Text))
    $RemoteReadinessProbe = Join-Path $testRoot 'remote-probe.ps1'
    Set-Content -LiteralPath $RemoteReadinessProbe -Value 'param($Address,$Port,$ExpectedSourceCommit) $global:AICFTestRemoteEvidence' -Encoding UTF8
    $ExpectedSourceCommit = 'a' * 40; $ClientAddress = '192.0.2.1'; $ServerPort = 2201
    try {
        foreach ($case in @('valid','false','string-ok','commit','address','port','pid','stale','future','nan','missing')) {
            $e = @{ok=$true;commit=$ExpectedSourceCommit;address=$ClientAddress;port=$ServerPort;process_id=123;observed_at=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds();log='/test/console.log'}
            switch ($case) {
                'false' {$e.ok=$false}
                'string-ok' {$e.ok='false'}
                'commit' {$e.commit='b'*40}
                'address' {$e.address='192.0.2.2'}
                'port' {$e.port=2001}
                'pid' {$e.process_id=0}
                'stale' {$e.observed_at-=60}
                'future' {$e.observed_at+=60}
                'nan' {$e.observed_at=[double]::NaN}
                'missing' {$e.Remove('observed_at')}
            }
            $global:AICFTestRemoteEvidence = [pscustomobject]$e
            $accepted=$true
            try { $null = & $gate } catch { $accepted=$false }
            if ($accepted -ne ($case -eq 'valid')) { Add-Failure 'REMOTE_READINESS' "Unexpected result for $case" }
        }
    } finally { Remove-Variable AICFTestRemoteEvidence -Scope Global -ErrorAction SilentlyContinue }
    $retailOutput = @(& powershell.exe @clientInvocation -UseRetailClient 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { Add-Failure 'RETAIL_CLIENT' 'Retail dry-run failed' }
    else {
        $retailManifest = Get-ManifestFromOutput -Output $retailOutput -Rule 'RETAIL_CLIENT'
        if ($retailManifest.executable -ne (Join-Path $fakeGameRoot 'ArmaReforgerSteam.exe')) { Add-Failure 'RETAIL_CLIENT' 'Wrong retail executable' }
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

Write-Output '[AICF][RUNTIME_LAUNCHER_STATIC][RESULT][PASS] direct_invocation=PASS argument_integrity=PASS spaces=PASS cyrillic=PASS stock_everon_rhs=PASS rhs_everon_server_client=PASS fresh_profile=PASS ready_gate=PASS'
exit 0
