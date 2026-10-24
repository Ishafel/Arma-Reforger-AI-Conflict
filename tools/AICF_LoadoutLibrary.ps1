# Постоянная библиотека локального Host отделена от одноразовых logs/settings.
# Формат файлов принадлежит AICF_LoadoutStore; launcher не переписывает JSON.
function Get-AICFLoadoutLibraryFiles {
    param([Parameter(Mandatory=$true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return @() }
    $directory = Get-Item -LiteralPath $Path
    if (-not $directory.PSIsContainer -or ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Loadout library must be a real directory: $Path"
    }
    $files = @(Get-ChildItem -LiteralPath $Path -File | Where-Object {
        $_.Name -cmatch '^template_(0|[1-9][0-9]{0,2})(?:_([01]))?\.json$' -and [int]$Matches[1] -lt 256
    } | Sort-Object Name)
    foreach ($file in $files) {
        $limit = 24576
        if ($file.Name -cmatch '^template_[0-9]+_[01]\.json$') { $limit = 600000 }
        if ($file.Length -gt $limit -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Invalid loadout file: $($file.FullName)"
        }
    }
    return $files
}

function Get-AICFPersonalLoadoutFiles {
    param([Parameter(Mandatory=$true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return @() }
    $directory = Get-Item -LiteralPath $Path
    if (-not $directory.PSIsContainer -or ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Personal library must be a real directory' }
    $files = @(Get-ChildItem -LiteralPath $Path -File | Where-Object {
        $_.Name -cmatch '^[A-Za-z0-9_-]+_(?:US|USSR)_[01]\.json$' -or
        ($_.Name -cmatch '^[A-Za-z0-9_-]+_(?:US|USSR)_library_preset_(0|[1-9][0-9]{0,2})_[01]\.json$' -and [int]$Matches[1] -lt 256)
    })
    foreach ($file in $files) {
        if ($file.Length -gt 600000 -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Invalid personal preset file' }
    }
    return @($files | Sort-Object FullName)
}

function Mount-AICFLoadoutLibrary {
    param(
        [Parameter(Mandatory=$true)][string]$LibraryPath,
        [Parameter(Mandatory=$true)][string]$ProfilePath,
        [IO.FileInfo[]]$ImportFiles = @(),
        [string]$LegacyPersonalPath
    )
    [void](New-Item -ItemType Directory -Path $LibraryPath -Force)
    $null = Get-AICFLoadoutLibraryFiles -Path $LibraryPath
    $lockPath = Join-Path $LibraryPath '.aicf-library.lock'
    if ((Test-Path -LiteralPath $lockPath) -and ((Get-Item -LiteralPath $lockPath).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Invalid library lock: $lockPath"
    }
    $lease = $null
    $createdMounts = @()
    try {
        # Второй launcher не может открыть эту библиотеку до завершения первого.
        $lease = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        $existing = @(Get-AICFLoadoutLibraryFiles -Path $LibraryPath)
        if ($ImportFiles.Count -and $existing.Count) {
            # ID разных библиотек нельзя сливать: старшее поколение или tombstone
            # могут относиться к совершенно иному preset. Разрешён точный повтор.
            $same = $true
            foreach ($target in $existing) {
                $source = @($ImportFiles | Where-Object Name -CEQ $target.Name)
                if ($source.Count -ne 1 -or
                    (Get-FileHash -LiteralPath $target.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $source[0].FullName -Algorithm SHA256).Hash) { $same = $false }
            }
            if (-not $same) { throw 'Persistent library already has different presets; choose an empty directory for import.' }
        }
        if ($ImportFiles.Count) {
            foreach ($source in $ImportFiles) {
                $target = Join-Path $LibraryPath $source.Name
                if (Test-Path -LiteralPath $target) { continue }
                Copy-Item -LiteralPath $source.FullName -Destination $target
                if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $source.FullName -Algorithm SHA256).Hash) {
                    throw "Loadout import readback failed: $($source.Name)"
                }
            }
        }
        # Одна lease защищает обе библиотеки. Старый personal active остаётся
        # отдельным файлом; named variants имеют свой *_library_preset_ prefix.
        $personalLibrary = Join-Path $LibraryPath 'personal'
        $personalFiles = @(Get-AICFPersonalLoadoutFiles -Path $personalLibrary)
        [void](New-Item -ItemType Directory -Path $personalLibrary -Force)
        if ($LegacyPersonalPath -and $personalFiles.Count -eq 0) {
            $legacyRoot = [IO.Path]::GetFullPath($LegacyPersonalPath).TrimEnd('\','/')
            foreach ($source in @(Get-AICFPersonalLoadoutFiles -Path $legacyRoot)) {
                $relative = $source.FullName.Substring($legacyRoot.Length + 1)
                $target = Join-Path $personalLibrary $relative
                [void](New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force)
                Copy-Item -LiteralPath $source.FullName -Destination $target
                if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $source.FullName).Hash) { throw 'Personal import readback failed' }
            }
        }
        $profileDirectory = Join-Path $ProfilePath 'profile'
        [void](New-Item -ItemType Directory -Path $profileDirectory -Force)
        $mount = Join-Path $profileDirectory 'AICF_Loadouts'
        if (Test-Path -LiteralPath $mount) { throw "Loadout mount already exists: $mount" }
        [void](New-Item -ItemType Junction -Path $mount -Target $LibraryPath)
        $createdMounts += $mount
        $personalMount = Join-Path $profileDirectory 'AICF_PersonalLoadouts'
        if (Test-Path -LiteralPath $personalMount) { throw "Personal mount already exists: $personalMount" }
        [void](New-Item -ItemType Junction -Path $personalMount -Target $personalLibrary)
        $createdMounts += $personalMount
        return $lease
    }
    catch {
        foreach ($created in $createdMounts) { [IO.Directory]::Delete($created) }
        if ($lease) { $lease.Dispose() }
        throw
    }
}
