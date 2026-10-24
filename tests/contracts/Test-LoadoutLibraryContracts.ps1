param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $RepositoryRoot 'tools/AICF_LoadoutLibrary.ps1')
$taskRoot = Join-Path ([IO.Path]::GetTempPath()) ('aicf-loadout-contracts-' + [guid]::NewGuid().ToString('N'))
$mounts = @()
$lease = $null
$checks = 0
function Check([bool]$Value, [string]$Name) {
    if (-not $Value) { throw "FAIL $Name" }
    $script:checks++
}
try {
    $source = Join-Path $taskRoot 'legacy'
    $library = Join-Path $taskRoot 'persistent'
    [void](New-Item -ItemType Directory -Path $source -Force)
    foreach ($name in @('template_0.json','template_0_0.json','template_0_1.json','template_255_1.json','template_256.json','template_01.json','unrelated.json')) {
        [IO.File]::WriteAllText((Join-Path $source $name), $name)
    }
    $files = @(Get-AICFLoadoutLibraryFiles $source)
    Check ($files.Count -eq 4) 'Canonical legacy/generations/tombstone IDs only'
    $before = @($files | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName).Hash }) -join ','
    $firstProfile = Join-Path $taskRoot 'first-profile'
    $lease = Mount-AICFLoadoutLibrary $library $firstProfile $files
    $firstMount = Join-Path $firstProfile 'profile/AICF_Loadouts'
    $mounts += $firstMount
    Check ((Get-Item -LiteralPath $firstMount).LinkType -eq 'Junction') 'Fresh profile mounts only the loadout directory'
    Check ((Get-ChildItem -LiteralPath $firstMount -Filter '*.json').Count -eq 4) 'All canonical files imported'
    $blocked = $false
    try { $secondLease = Mount-AICFLoadoutLibrary $library (Join-Path $taskRoot 'blocked-profile') }
    catch { $blocked = $true }
    if ($secondLease) { $secondLease.Dispose() }
    Check $blocked 'Concurrent writer refused'
    [IO.File]::WriteAllText((Join-Path $firstMount 'template_7_1.json'), 'saved during first session')
    $lease.Dispose(); $lease = $null
    $secondProfile = Join-Path $taskRoot 'second-profile'
    $lease = Mount-AICFLoadoutLibrary $library $secondProfile
    $secondMount = Join-Path $secondProfile 'profile/AICF_Loadouts'
    $mounts += $secondMount
    Check ([IO.File]::ReadAllText((Join-Path $secondMount 'template_7_1.json')) -eq 'saved during first session') 'Preset survives a new profile'
    Check ([IO.File]::ReadAllText((Join-Path $secondMount 'template_0_1.json')) -eq 'template_0_1.json') 'Deleted generation preserved across restart'
    $lease.Dispose(); $lease = $null
    $after = @($files | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName).Hash }) -join ','
    Check ($before -ceq $after) 'Original legacy library bytes preserved'
    $blocked = $false
    try { $lease = Mount-AICFLoadoutLibrary $library (Join-Path $taskRoot 'conflict-profile') $files }
    catch { $blocked = $true }
    Check $blocked 'Different existing library is not overwritten or merged by ID'
    $negative = Join-Path $source 'template_9.json'
    [IO.File]::WriteAllText($negative, ('x' * 24577))
    $blocked = $false
    try { $null = Get-AICFLoadoutLibraryFiles $source } catch { $blocked = $true }
    Check $blocked 'Oversized legacy file rejected'
    Remove-Item -LiteralPath $negative
    $negative = Join-Path $source 'template_9_0.json'
    [IO.File]::WriteAllText($negative, ('x' * 600001))
    $blocked = $false
    try { $null = Get-AICFLoadoutLibraryFiles $source } catch { $blocked = $true }
    Check $blocked 'Oversized generation rejected'
    $blocked = $false
    try { $null = Get-AICFLoadoutLibraryFiles $firstMount } catch { $blocked = $true }
    Check $blocked 'Reparse source directory rejected'
    Write-Output "PASS Loadout library filesystem contracts; checks=$checks; engine restart=NOT_RUN"
}
finally {
    if ($lease) { $lease.Dispose() }
    foreach ($mount in $mounts) {
        if (-not [IO.Path]::GetFullPath($mount).StartsWith([IO.Path]::GetFullPath($taskRoot) + [IO.Path]::DirectorySeparatorChar)) { throw 'Unsafe mount cleanup' }
        if (Test-Path -LiteralPath $mount) { [IO.Directory]::Delete($mount) }
    }
    $resolved = [IO.Path]::GetFullPath($taskRoot)
    if (-not $resolved.StartsWith([IO.Path]::GetTempPath(), [StringComparison]::OrdinalIgnoreCase) -or (Split-Path $resolved -Leaf) -notlike 'aicf-loadout-contracts-*') { throw 'Unsafe fixture cleanup' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
