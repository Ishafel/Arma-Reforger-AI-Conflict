[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Server', 'Client')]
    [string]$Role,

    [Parameter(Mandatory = $true)]
    [ValidateSet('Stock', 'Everon', 'EveronNorth', 'RHS', 'EveronRHS', 'EveronNorthRHS', 'ArlandWCSRHS', 'EveronWCSRHS', 'EveronNorthWCSRHS')]
    [string]$Variant,

    [string]$RepositoryRoot,
    [string]$ServerRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server',
    [string]$GameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger',
    [string]$RhsAddonsRoot,
    [string]$ProfileRoot,
    [string]$ServerProfileRoot,
    [switch]$UseRetailClient,
    [string]$RemoteReadinessProbe,
    [string]$ExpectedSourceCommit,
    [string]$LoadoutLibraryPath,
    [string]$ClientAddress = '127.0.0.1',
    [ValidateRange(1, 65535)]
    [int]$ServerPort = 2001,
    [ValidateRange(1, 1800)]
    [int]$ReadyTimeoutSeconds = 300,
    [AllowEmptyString()]
    [string]$AICommanderMode = 'BOTH',
    [switch]$UseDefaultAICommanderMode,
    [string[]]$AdditionalArguments = @(),
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $RepositoryRoot) {
    $RepositoryRoot = Split-Path -Parent $PSScriptRoot
}

function Resolve-AICFExistingDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Description не найден: $Path"
    }

    return (Resolve-Path -LiteralPath $Path).ProviderPath
}

function Resolve-AICFExistingFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description не найден: $Path"
    }

    return (Resolve-Path -LiteralPath $Path).ProviderPath
}

function Resolve-AICFUnresolvedPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
}

function Test-AICFChildPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Candidate,
        [Parameter(Mandatory = $true)]
        [string]$Parent
    )

    $normalizedParent = $Parent.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $Candidate.Equals($Parent, [System.StringComparison]::OrdinalIgnoreCase) -or
        $Candidate.StartsWith($normalizedParent, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-AICFPathCanJoinAddonsDir {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if ($Path.Contains(',')) {
        throw "$Description содержит запятую и не может быть однозначно передан в -addonsDir: $Path"
    }

    if ($Path.Contains("`r") -or $Path.Contains("`n")) {
        throw "$Description содержит перевод строки: $Path"
    }
}

function Test-AICFContainsOrdinalIgnoreCase {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,
        [Parameter(Mandatory = $true)]
        [string]$Expected
    )

    return $Text.IndexOf($Expected, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
}

function Get-AICFLatestConsoleLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProfilePath
    )

    $logsRoot = Join-Path $ProfilePath 'logs'
    if (-not (Test-Path -LiteralPath $logsRoot -PathType Container)) {
        return $null
    }

    return Get-ChildItem -LiteralPath $logsRoot -File -Recurse -Filter 'console.log' |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
}

function Read-AICFConsoleSnapshot {
    param([Parameter(Mandatory = $true)][string]$Path)

    # Native log продолжает расти. Получаем один UTF-8 string, разрешая
    # engine writer держать файл открытым; provider chunks не становятся array.
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8)
    try { return $reader.ReadToEnd() }
    finally { $reader.Dispose() }
}

function Get-AICFLocalServerProcess {
    param(
        [Parameter(Mandatory = $true)]
        [int]$Port,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedProfilePath
    )

    $endpoints = @(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue)
    foreach ($endpoint in $endpoints) {
        $process = Get-Process -Id $endpoint.OwningProcess -ErrorAction SilentlyContinue
        if (-not $process -or $process.ProcessName -notmatch '^ArmaReforgerServer') {
            continue
        }

        try {
            $processInfo = Get-CimInstance Win32_Process -Filter "ProcessId = $($process.Id)" -ErrorAction Stop
            if ($processInfo.CommandLine -and
                -not (Test-AICFContainsOrdinalIgnoreCase $processInfo.CommandLine $ExpectedProfilePath)) {
                continue
            }
        }
        catch {
            # Exact CLI is independently verified from the fresh server log below.
        }

        return $process
    }

    return $null
}

function Wait-AICFServerReady {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProfilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$RequiredCliFragments,
        [Parameter(Mandatory = $true)]
        [int]$Port,
        [Parameter(Mandatory = $true)]
        [int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastState = 'console.log ещё не создан'

    while ([DateTime]::UtcNow -lt $deadline) {
        $log = Get-AICFLatestConsoleLog -ProfilePath $ProfilePath
        if (-not $log) {
            Start-Sleep -Milliseconds 1000
            continue
        }

        $logText = Read-AICFConsoleSnapshot -Path $log.FullName
        $cliLine = @($logText -split "`r?`n" | Where-Object { $_ -match 'CLI Params:' } | Select-Object -First 1)
        if ($cliLine.Count -eq 0) {
            $lastState = "CLI Params ещё не появился в $($log.FullName)"
            Start-Sleep -Milliseconds 1000
            continue
        }

        foreach ($fragment in $RequiredCliFragments) {
            if (-not (Test-AICFContainsOrdinalIgnoreCase $cliLine[0] $fragment)) {
                throw "Server CLI validation failed: отсутствует точный фрагмент '$fragment'. Строка: $($cliLine[0])"
            }
        }

        if (-not (Test-AICFContainsOrdinalIgnoreCase $logText '[AICF][STAGE1][INFO][ROSTER_READY]')) {
            $lastState = "CLI Params подтверждён, ожидается ROSTER_READY в $($log.FullName)"
            Start-Sleep -Milliseconds 1000
            continue
        }

        $process = Get-AICFLocalServerProcess -Port $Port -ExpectedProfilePath $ProfilePath
        if (-not $process) {
            $lastState = "ROSTER_READY найден, но живой ArmaReforgerServer process на UDP $Port не найден"
            Start-Sleep -Milliseconds 1000
            continue
        }

        return [pscustomobject]@{
            LogPath = $log.FullName
            ProcessId = $process.Id
        }
    }

    throw "Server readiness timeout через $TimeoutSeconds секунд: $lastState"
}

function Wait-AICFNewClientProcess {
    param(
        [int[]]$ExistingProcessIds,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedProfilePath,
        [ValidateRange(1, 60)]
        [int]$TimeoutSeconds = 15
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $newProcesses = @(Get-Process -Name 'ArmaReforgerSteamDiag','ArmaReforgerSteam' -ErrorAction SilentlyContinue |
            Where-Object { $ExistingProcessIds -notcontains $_.Id })
        foreach ($process in $newProcesses) {
            try {
                $processInfo = Get-CimInstance Win32_Process -Filter "ProcessId = $($process.Id)" -ErrorAction Stop
                if ($processInfo.CommandLine -and
                    (Test-AICFContainsOrdinalIgnoreCase $processInfo.CommandLine $ExpectedProfilePath)) {
                    return $process
                }
            }
            catch {
                # A single new process is still identity-safe relative to the pre-launch snapshot.
            }
        }

        if ($newProcesses.Count -eq 1) {
            return $newProcesses[0]
        }

        Start-Sleep -Milliseconds 250
    }

    return $null
}

function Assert-AICFResourceDatabases {
    param([object[]]$Databases)

    foreach ($database in $Databases) {
        if (-not $database.exists -or $database.bytes -le 0) {
            throw "Source resource database отсутствует или пуста: $($database.path). Выполни терминальный Workbench Validate/Compile для выбранного проекта и повтори запуск."
        }
    }
}

$repositoryPath = Resolve-AICFExistingDirectory -Path $RepositoryRoot -Description 'Repository root'
$serverRootPath = Resolve-AICFExistingDirectory -Path $ServerRoot -Description 'Arma Reforger Server root'
$gameRootPath = $null
if ($Role -eq 'Client') {
    $gameRootPath = Resolve-AICFExistingDirectory -Path $GameRoot -Description 'Arma Reforger game root'
}

$rhsRootPath = $null
if ($Variant -in @('RHS', 'EveronRHS', 'EveronNorthRHS', 'ArlandWCSRHS', 'EveronWCSRHS', 'EveronNorthWCSRHS')) {
    if (-not $RhsAddonsRoot) {
        $rhsCandidates = [System.Collections.Generic.List[string]]::new()
        $documentsRoot = [Environment]::GetFolderPath('MyDocuments')
        if ($documentsRoot) {
            $rhsCandidates.Add((Join-Path $documentsRoot 'My Games\ArmaReforger\addons'))
        }
        if ($env:OneDrive -and (Test-Path -LiteralPath $env:OneDrive -PathType Container)) {
            foreach ($oneDriveChild in Get-ChildItem -LiteralPath $env:OneDrive -Directory -ErrorAction SilentlyContinue) {
                $rhsCandidates.Add((Join-Path $oneDriveChild.FullName 'My Games\ArmaReforger\addons'))
            }
        }

        $resolvedRhsCandidates = @($rhsCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Container } | Select-Object -First 1)
        if ($resolvedRhsCandidates.Count -eq 0) {
            throw 'RHS addons root не найден автоматически; укажи -RhsAddonsRoot явно.'
        }
        $RhsAddonsRoot = $resolvedRhsCandidates[0]
    }
    $rhsRootPath = Resolve-AICFExistingDirectory -Path $RhsAddonsRoot -Description 'RHS addons root'
}

foreach ($pathCheck in @(
    @{ Path = $repositoryPath; Description = 'Repository root' },
    @{ Path = $serverRootPath; Description = 'Arma Reforger Server root' }
)) {
    Assert-AICFPathCanJoinAddonsDir -Path $pathCheck.Path -Description $pathCheck.Description
}
if ($gameRootPath) {
    Assert-AICFPathCanJoinAddonsDir -Path $gameRootPath -Description 'Arma Reforger game root'
}
if ($rhsRootPath) {
    Assert-AICFPathCanJoinAddonsDir -Path $rhsRootPath -Description 'RHS addons root'
}

if ($UseDefaultAICommanderMode -and $PSBoundParameters.ContainsKey('AICommanderMode')) {
    throw 'Нельзя одновременно задавать -UseDefaultAICommanderMode и -AICommanderMode.'
}

$managedArguments = @(
    '-gproj', '-server', '-MissionHeader', '-worldSystemsConfig', '-client',
    '-addonsDir', '-addons', '-profile', '-backendFreshSession',
    '-aicfAICommanderMode', '-maxFPS', '-logStats'
)
foreach ($argument in $AdditionalArguments) {
    if ($managedArguments -contains $argument) {
        throw "AdditionalArguments не может переопределять управляемый параметр $argument."
    }
}

$projectRelativePath = 'AIConflictArland\addon.gproj'
$scenario = '{BC2437E4861B4FD2}Missions/AICF_Conflict_Arland.conf'
$missionHeader = 'Missions/AICF_Conflict_Arland.conf'
$addonIds = '9178E5822AFE48EA,B52C5F6AEDBF423E'
$profileVariant = ''
if ($Variant -in @('Everon', 'EveronNorth')) {
    $projectRelativePath = 'AIConflictEveron\addon.gproj'
    $scenario = '{4C5D73A5614F41D9}Missions/AICF_Conflict_Everon.conf'
    $missionHeader = 'Missions/AICF_Conflict_Everon.conf'
    $addonIds = '9178E5822AFE48EA,B52C5F6AEDBF423E,A4B2E62595F645A4'
    $profileVariant = '-Everon'
    if ($Variant -eq 'EveronNorth') {
        $missionHeader = 'Missions/AICF_Conflict_Everon_North.conf'
        $scenario = '{A1CF190919300000}Missions/AICF_Conflict_Everon_North.conf'
        $profileVariant = '-EveronNorth'
    }
}
elseif ($Variant -in @('RHS', 'ArlandWCSRHS', 'EveronWCSRHS', 'EveronNorthWCSRHS')) {
    $projectRelativePath = 'AIConflictArlandRHS\addon.gproj'
    $scenario = '{97E4BCB73F044C66}Missions/AICF_RHS_Conflict_Arland.conf'
    $missionHeader = 'Missions/AICF_RHS_Conflict_Arland.conf'
    $addonIds = '9178E5822AFE48EA,B52C5F6AEDBF423E,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C'
    $profileVariant = '-RHS'
    if ($Variant -in @('ArlandWCSRHS', 'EveronWCSRHS', 'EveronNorthWCSRHS')) {
        $projectRelativePath = 'AIConflictArlandWCSRHS\addon.gproj'
        $scenario = '{A1CF260928100002}Missions/AICF_WCS_RHS_Conflict_Arland.conf'
        $missionHeader = 'Missions/AICF_WCS_RHS_Conflict_Arland.conf'
        $addonIds += ',68F006D910E7546F,61C74A8B647617DA,62A668F513428630,631C3C1AEE9C90BC,615806DC6C57AF02,615818DA7C0343FD,629B2BA37EFFD577,5E389BB9F58B79A6,5D1880C4AD410C14,5E0AB16BEB16D6A4,65CF7AE8574E06D2,65F929DF622BAD50,6602C1EC7E5A4A87,6152CB0BD0684837,5B383D4CB27E0D54,63120AE07E6C0966,A1CF260928100001'
        $profileVariant = '-ArlandWCSRHS'
        if ($Variant -in @('EveronWCSRHS', 'EveronNorthWCSRHS')) {
            $projectRelativePath = 'AIConflictEveronWCSRHS\addon.gproj'
            $scenario = '{A1CF261006100002}Missions/AICF_WCS_RHS_Conflict_Everon.conf'
            $missionHeader = 'Missions/AICF_WCS_RHS_Conflict_Everon.conf'
            $addonIds += ',A4B2E62595F645A4,FA9FDCCA428A43BA,A1CF261006100001'
            $profileVariant = '-EveronWCSRHS'
            if ($Variant -eq 'EveronNorthWCSRHS') {
                $scenario = '{A1CF261006100003}Missions/AICF_WCS_RHS_Conflict_Everon_North.conf'
                $missionHeader = 'Missions/AICF_WCS_RHS_Conflict_Everon_North.conf'
                $profileVariant = '-EveronNorthWCSRHS'
            }
        }
    }
}
elseif ($Variant -in @('EveronRHS', 'EveronNorthRHS')) {
    $projectRelativePath = 'AIConflictEveronRHS\addon.gproj'
    $scenario = '{57FA3D0337BE47E5}Missions/AICF_RHS_Conflict_Everon.conf'
    $missionHeader = 'Missions/AICF_RHS_Conflict_Everon.conf'
    $addonIds = '9178E5822AFE48EA,B52C5F6AEDBF423E,A4B2E62595F645A4,1337C0DE5DABBEEF,BADC0DEDABBEDA5E,595F2BF2F44836FB,9F88011DA22B471C,FA9FDCCA428A43BA'
    $profileVariant = '-EveronRHS'
    if ($Variant -eq 'EveronNorthRHS') {
        $missionHeader = 'Missions/AICF_RHS_Conflict_Everon_North.conf'
        # Native -server принимает MissionHeader; запуск .ent не применяет whitelist.
        $scenario = '{A1CF190919100000}Missions/AICF_RHS_Conflict_Everon_North.conf'
        $profileVariant = '-EveronNorthRHS'
    }
}

$projectPath = Resolve-AICFExistingFile -Path (Join-Path $repositoryPath $projectRelativePath) -Description "$Variant gproj"
$executable = $null
$addonsDirectories = @($repositoryPath)
if ($Role -eq 'Server') {
    $executable = Resolve-AICFExistingFile -Path (Join-Path $serverRootPath 'ArmaReforgerServerDiag.exe') -Description 'Diag server executable'
    $addonsDirectories += (Join-Path $serverRootPath 'addons')
}
else {
    $clientBinary = if ($UseRetailClient) { 'ArmaReforgerSteam.exe' } else { 'ArmaReforgerSteamDiag.exe' }
    $executable = Resolve-AICFExistingFile -Path (Join-Path $gameRootPath $clientBinary) -Description 'Client executable'
    $addonsDirectories += (Join-Path $gameRootPath 'addons')
}
if ($rhsRootPath) {
    $addonsDirectories += $rhsRootPath
}
$addonsDir = $addonsDirectories -join ','

# Source-копия без индексов может стартовать как server, но client не разрешит
# MissionHeader/world GUID. DryRun остаётся доступен для подготовки Workbench.
$localProjects = [ordered]@{
    '9178E5822AFE48EA' = 'AIConflictCore'
    'B52C5F6AEDBF423E' = 'AIConflictArland'
    'A4B2E62595F645A4' = 'AIConflictEveron'
    '9F88011DA22B471C' = 'AIConflictArlandRHS'
    'FA9FDCCA428A43BA' = 'AIConflictEveronRHS'
    'A1CF260928100001' = 'AIConflictArlandWCSRHS'
    'A1CF261006100001' = 'AIConflictEveronWCSRHS'
}
$resourceDatabases = @(
    foreach ($addonId in $addonIds.Split(',')) {
        if (-not $localProjects.Contains($addonId)) { continue }
        $databasePath = Join-Path (Join-Path $repositoryPath $localProjects[$addonId]) 'resourceDatabase.rdb'
        $databaseFile = Get-Item -LiteralPath $databasePath -ErrorAction SilentlyContinue
        $databaseBytes = 0
        $databaseHash = $null
        if ($databaseFile -and -not $databaseFile.PSIsContainer) {
            $databaseBytes = $databaseFile.Length
            $databaseHash = (Get-FileHash -LiteralPath $databasePath -Algorithm SHA256).Hash
        }
        [pscustomobject]@{ addonId = $addonId; path = $databasePath; exists = ($null -ne $databaseHash); bytes = $databaseBytes; sha256 = $databaseHash }
    }
)

if (-not $ProfileRoot) {
    $localAppData = [Environment]::GetFolderPath('LocalApplicationData')
    if (-not $localAppData) {
        throw 'Не удалось определить LocalApplicationData для fresh runtime profile.'
    }

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $ProfileRoot = Join-Path $localAppData "AICF\$Role$profileVariant-$stamp"
}
$profilePath = Resolve-AICFUnresolvedPath -Path $ProfileRoot
if (Test-AICFChildPath -Candidate $profilePath -Parent $repositoryPath) {
    throw "Runtime profile нельзя создавать внутри репозитория: $profilePath"
}
if (Test-Path -LiteralPath $profilePath) {
    throw "Runtime profile уже существует; укажи новый путь: $profilePath"
}

# Перенос только библиотеки рецептов; campaign bindings в новый матч не входят.
$loadoutFiles = @()
$loadoutLibrarySource = $null
if ($LoadoutLibraryPath) {
    if ($Role -ne 'Server') { throw '-LoadoutLibraryPath поддерживается только для Server.' }
    $loadoutLibrarySource = Resolve-AICFExistingDirectory -Path $LoadoutLibraryPath -Description 'Loadout library'
    $loadoutFiles = @(Get-ChildItem -LiteralPath $loadoutLibrarySource -File | Where-Object {
        $_.Name -cmatch '^template_(0|[1-9][0-9]{0,2})\.json$' -and [int]$Matches[1] -lt 256
    } | Sort-Object Name)
    foreach ($loadoutFile in $loadoutFiles) {
        if ($loadoutFile.Length -gt 24576 -or ($loadoutFile.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Недопустимый файл библиотеки: $($loadoutFile.FullName)"
        }
    }
}

$nativeArguments = @('-gproj', $projectPath)
if ($Role -eq 'Server') {
    $nativeArguments += @(
        '-server', $scenario,
        '-MissionHeader', $missionHeader,
        '-worldSystemsConfig', 'Configs/Systems/ConflictSystems.conf'
    )
}
else {
    $clientTarget = $ClientAddress
    if ($ClientAddress -match '^(\[[^\]]+\]|[^:]+):(\d+)$') {
        if ([int]$Matches[2] -ne $ServerPort) {
            throw 'Порт в ClientAddress должен совпадать с ServerPort, который проверяет readiness сервера.'
        }
    }
    elseif ($ServerPort -ne 2001) {
        if ($ClientAddress.Contains(':') -and -not $ClientAddress.StartsWith('[')) {
            $clientTarget = "[$ClientAddress]"
        }
        $clientTarget = "${clientTarget}:$ServerPort"
    }
    $nativeArguments += @('-client', $clientTarget)
}
$nativeArguments += @(
    '-addonsDir', $addonsDir,
    '-addons', $addonIds,
    '-profile', $profilePath,
    '-backendFreshSession'
)
if ($Role -eq 'Server') {
    if (-not $UseDefaultAICommanderMode) {
        $nativeArguments += @('-aicfAICommanderMode', $AICommanderMode)
    }
    $nativeArguments += @('-maxFPS', '60', '-logStats', '10000')
}
if (@($AdditionalArguments).Count -gt 0) {
    $nativeArguments += $AdditionalArguments
}

$manifest = [ordered]@{
    schema = 1
    role = $Role
    variant = $Variant
    executable = $executable
    profile = $profilePath
    addonsDir = $addonsDir
    arguments = @($nativeArguments)
    loadoutLibrarySource = $loadoutLibrarySource
    resourceDatabases = $resourceDatabases
    loadoutLibraryFiles = @($loadoutFiles | ForEach-Object {
        [ordered]@{ name = $_.Name; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
}
$manifestJson = $manifest | ConvertTo-Json -Depth 4 -Compress
Write-Output "[AICF][RUNTIME_LAUNCHER][PLAN] role=$Role variant=$Variant profile=$profilePath argument_count=$($nativeArguments.Count)"
Write-Output "AICF_RUNTIME_PROFILE=$profilePath"
Write-Output "AICF_RUNTIME_MANIFEST_JSON=$manifestJson"

if ($DryRun) {
    Write-Output '[AICF][RUNTIME_LAUNCHER][RESULT][PASS] mode=DRY_RUN'
    exit 0
}

Assert-AICFResourceDatabases -Databases $resourceDatabases

if ($loadoutFiles.Count -gt 0) {
    $loadoutDestination = Join-Path $profilePath 'profile\AICF_Loadouts'
    New-Item -ItemType Directory -Path $loadoutDestination -Force | Out-Null
    foreach ($loadoutFile in $loadoutFiles) {
        Copy-Item -LiteralPath $loadoutFile.FullName -Destination (Join-Path $loadoutDestination $loadoutFile.Name)
    }
}

if ($Role -eq 'Client' -and $RemoteReadinessProbe) {
    if (-not $ExpectedSourceCommit -or $ExpectedSourceCommit -notmatch '^[a-f0-9]{40}$') { throw 'Remote readiness requires exact source commit' }
    $probe = Resolve-AICFExistingFile -Path $RemoteReadinessProbe -Description 'Remote readiness probe'
    $ready = & $probe -Address $ClientAddress -Port $ServerPort -ExpectedSourceCommit $ExpectedSourceCommit
    $age = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - [double]$ready.observed_at
    if ($ready.ok -isnot [bool] -or $ready.ok -ne $true -or [double]::IsNaN($age) -or [double]::IsInfinity($age) -or $ready.commit -ne $ExpectedSourceCommit -or $ready.address -ne $ClientAddress -or $ready.port -ne $ServerPort -or $ready.process_id -le 0 -or $age -lt -5 -or $age -gt 30) { throw 'Invalid or stale remote readiness evidence' }
    Write-Output "[AICF][RUNTIME_LAUNCHER][REMOTE_SERVER_READY] process_id=$($ready.process_id) commit=$($ready.commit) log=$($ready.log)"
}
elseif ($Role -eq 'Client') {
    if (-not $ServerProfileRoot) {
        throw 'Для Client обязателен -ServerProfileRoot: launcher проверяет server CLI, process и ROSTER_READY до подключения.'
    }

    $serverProfilePath = Resolve-AICFExistingDirectory -Path $ServerProfileRoot -Description 'Server profile root'
    $serverAddonsDirectories = @($repositoryPath, (Join-Path $serverRootPath 'addons'))
    if ($rhsRootPath) {
        $serverAddonsDirectories += $rhsRootPath
    }
    $serverAddonsDir = $serverAddonsDirectories -join ','
    $requiredCliFragments = @(
        "-gproj $projectPath",
        "-server $scenario",
        "-MissionHeader $missionHeader",
        "-addonsDir $serverAddonsDir -addons $addonIds",
        "-profile $serverProfilePath"
    )
    $ready = Wait-AICFServerReady `
        -ProfilePath $serverProfilePath `
        -RequiredCliFragments $requiredCliFragments `
        -Port $ServerPort `
        -TimeoutSeconds $ReadyTimeoutSeconds
    Write-Output "[AICF][RUNTIME_LAUNCHER][SERVER_READY] process_id=$($ready.ProcessId) port=$ServerPort log=$($ready.LogPath)"
}

$existingClientProcessIds = @()
if ($Role -eq 'Client') {
    $existingClientProcessIds = @(Get-Process -Name 'ArmaReforgerSteamDiag','ArmaReforgerSteam' -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Id)
}

& $executable @nativeArguments
$lastExitCodeVariable = Get-Variable -Name LASTEXITCODE -ErrorAction SilentlyContinue
$nativeExitCode = $null
$exitCodeSource = 'NATIVE'
if ($Role -eq 'Client') {
    $clientProcess = Wait-AICFNewClientProcess `
        -ExistingProcessIds $existingClientProcessIds `
        -ExpectedProfilePath $profilePath
    if ($clientProcess) {
        Write-Output "[AICF][RUNTIME_LAUNCHER][PROCESS_STARTED] role=Client process_id=$($clientProcess.Id) profile=$profilePath"
        $null = $clientProcess.Handle
        $clientProcess.WaitForExit()
        $clientProcess.Refresh()
        try {
            $nativeExitCode = $clientProcess.ExitCode
        }
        catch {
            $nativeExitCode = $null
        }
        if ($null -eq $nativeExitCode) {
            $nativeExitCode = 0
            $exitCodeSource = 'PROCESS_OBSERVED_NO_NATIVE_CODE'
        }
        else {
            $exitCodeSource = 'PROCESS_API'
        }
    }
    elseif ($lastExitCodeVariable) {
        $nativeExitCode = [int]$lastExitCodeVariable.Value
    }
    else {
        throw "Client invocation returned without LASTEXITCODE and no new process matched profile $profilePath."
    }
}
elseif ($lastExitCodeVariable) {
    $nativeExitCode = [int]$lastExitCodeVariable.Value
}
else {
    throw 'Server process returned without LASTEXITCODE.'
}
Write-Output "[AICF][RUNTIME_LAUNCHER][EXIT] role=$Role variant=$Variant exit_code=$nativeExitCode exit_code_source=$exitCodeSource profile=$profilePath"
exit $nativeExitCode
