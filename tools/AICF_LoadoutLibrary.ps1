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

function Mount-AICFLoadoutLibrary {
    param(
        [Parameter(Mandatory=$true)][string]$LibraryPath,
        [Parameter(Mandatory=$true)][string]$ProfilePath,
        [IO.FileInfo[]]$ImportFiles = @()
    )
    [void](New-Item -ItemType Directory -Path $LibraryPath -Force)
    $null = Get-AICFLoadoutLibraryFiles -Path $LibraryPath
    $lockPath = Join-Path $LibraryPath '.aicf-library.lock'
    if ((Test-Path -LiteralPath $lockPath) -and ((Get-Item -LiteralPath $lockPath).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Invalid library lock: $lockPath"
    }
    $lease = $null
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
        $profileDirectory = Join-Path $ProfilePath 'profile'
        [void](New-Item -ItemType Directory -Path $profileDirectory -Force)
        $mount = Join-Path $profileDirectory 'AICF_Loadouts'
        if (Test-Path -LiteralPath $mount) { throw "Loadout mount already exists: $mount" }
        [void](New-Item -ItemType Junction -Path $mount -Target $LibraryPath)
        return $lease
    }
    catch {
        if ($lease) { $lease.Dispose() }
        throw
    }
}
