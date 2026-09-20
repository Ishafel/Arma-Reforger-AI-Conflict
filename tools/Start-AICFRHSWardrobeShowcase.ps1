[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Server', 'Client')]
    [string]$Role,
    [Parameter(Mandatory = $true)]
    [string]$SessionRoot
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$session = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SessionRoot)
$source = Join-Path $session 'source'
$launcher = Join-Path $PSScriptRoot 'Start-AICFRuntime.ps1'

if ($Role -eq 'Server') {
    if (Test-Path -LiteralPath $session) {
        throw 'Для осмотра нужен новый SessionRoot: существующий запуск не перезаписывается.'
    }
    $addons = @('AIConflictCore', 'AIConflictArland', 'AIConflictEveron', 'AIConflictArlandRHS', 'AIConflictEveronRHS')
    foreach ($addon in $addons) {
        if (-not (Test-Path -LiteralPath (Join-Path $repository "$addon/resourceDatabase.rdb"))) {
            throw "Отсутствует локальный индекс $addon/resourceDatabase.rdb. Сначала выполните терминальный Workbench Validate/Compile по docs/DEVELOPMENT.md."
        }
    }
    New-Item -ItemType Directory -Path $source -Force | Out-Null
    foreach ($addon in $addons) {
        $target = Join-Path $source $addon
        New-Item -ItemType Directory -Path $target -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $repository "$addon/addon.gproj") -Destination $target
        # Локальный generated cache после Workbench содержит GUID MissionHeader.
        # Копия остаётся только в игнорируемом каталоге сессии.
        $database = Join-Path $repository "$addon/resourceDatabase.rdb"
        Copy-Item -LiteralPath $database -Destination $target
        foreach ($folder in @('Scripts', 'Missions', 'Language')) {
            $origin = Join-Path $repository "$addon/$folder"
            if (Test-Path -LiteralPath $origin) {
                Copy-Item -LiteralPath $origin -Destination $target -Recurse
            }
        }
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fixtures/AICF_RHSWardrobeShowcase.c') `
        -Destination (Join-Path $source 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/AICF_RHSWardrobeShowcase.c')
    & $launcher -Role Server -Variant EveronNorthRHS -RepositoryRoot $source `
        -ProfileRoot (Join-Path $session 'server') `
        -AdditionalArguments @('-noThrow', '-aicfWardrobeShowcase', '1') |
        Tee-Object -FilePath (Join-Path $session 'server-launch.txt')
    exit $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $source)) {
    throw 'Сначала запустите Server с тем же SessionRoot в отдельном терминале.'
}
& $launcher -Role Client -Variant EveronNorthRHS -RepositoryRoot $source `
    -ProfileRoot (Join-Path $session 'client') -ServerProfileRoot (Join-Path $session 'server') `
    -AdditionalArguments @('-noThrow', '-language', 'ru_ru') |
    Tee-Object -FilePath (Join-Path $session 'client-launch.txt')
exit $LASTEXITCODE
