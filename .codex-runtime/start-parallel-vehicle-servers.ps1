param(
    [int]$StartupTimeoutSeconds = 45
)

$ErrorActionPreference = "Stop"

$serverRoot = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server"
$repoRoot = Split-Path -Parent $PSScriptRoot
$serverExe = Join-Path $serverRoot "ArmaReforgerServerDiag.exe"
$addonDirs = "$repoRoot,$serverRoot\addons"

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$batchRoot = Join-Path $env:LOCALAPPDATA "AICF\Parallel-Config-$stamp"
New-Item -ItemType Directory -Path $batchRoot -Force | Out-Null

$plans = @(
    [pscustomobject]@{
        Name = "Transport"
        Config = Join-Path $PSScriptRoot "parallel-transport-server.json"
        GamePort = 21001
        A2SPort = 21002
        Transport = 1
        Armed = 0
    },
    [pscustomobject]@{
        Name = "Armed"
        Config = Join-Path $PSScriptRoot "parallel-armed-server.json"
        GamePort = 21011
        A2SPort = 21012
        Transport = 0
        Armed = 1
    }
)

$requestedPorts = $plans | ForEach-Object { $_.GamePort; $_.A2SPort }
$occupiedPorts = Get-NetUDPEndpoint -ErrorAction SilentlyContinue |
    Where-Object { $requestedPorts -contains $_.LocalPort }
if ($occupiedPorts) {
    $details = $occupiedPorts |
        Select-Object LocalAddress, LocalPort, OwningProcess |
        Format-Table -AutoSize |
        Out-String
    throw "Required UDP ports are occupied:`n$details"
}

$runs = @()
try {
    foreach ($plan in $plans) {
        $profile = Join-Path $batchRoot $plan.Name
        New-Item -ItemType Directory -Path $profile -Force | Out-Null

        $argumentLine = @(
            "-gproj `"$repoRoot\AIConflictArland\addon.gproj`""
            "-config `"$($plan.Config)`""
            "-addonsDir `"$addonDirs`""
            "-profile `"$profile`""
            "-aicfVehiclesEnabled 1"
            "-aicfTransportVehiclesPerFaction $($plan.Transport)"
            "-aicfArmedLightVehiclesPerFaction $($plan.Armed)"
            "-aicfMaxVehiclesPerFaction 1"
            "-aicfInitialTickets 20"
            "-aicfRequirePlayerForResult 0"
            "-worldTime 8"
            "-backendFreshSession"
            "-maxFPS 60"
            "-logStats 10000"
            "-noThrow"
        ) -join " "

        $process = Start-Process `
            -FilePath $serverExe `
            -ArgumentList $argumentLine `
            -WorkingDirectory $serverRoot `
            -WindowStyle Hidden `
            -PassThru

        $deadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)
        $boundPorts = @()
        do {
            Start-Sleep -Milliseconds 500
            if (-not (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)) {
                $log = Get-ChildItem -LiteralPath $profile -Recurse -Filter console.log -ErrorAction SilentlyContinue |
                    Sort-Object LastWriteTime -Descending |
                    Select-Object -First 1
                $tail = if ($log) { Get-Content -LiteralPath $log.FullName -Tail 40 | Out-String } else { "No console.log found." }
                throw "$($plan.Name) exited before binding ports.`n$tail"
            }

            $boundPorts = @(
                Get-NetUDPEndpoint -ErrorAction SilentlyContinue |
                    Where-Object { $_.OwningProcess -eq $process.Id } |
                    Select-Object -ExpandProperty LocalPort
            )
        } while (
            (Get-Date) -lt $deadline -and
            (-not ($boundPorts -contains $plan.GamePort) -or -not ($boundPorts -contains $plan.A2SPort))
        )

        if (-not ($boundPorts -contains $plan.GamePort) -or -not ($boundPorts -contains $plan.A2SPort)) {
            throw "$($plan.Name) PID $($process.Id) did not bind expected ports $($plan.GamePort)/$($plan.A2SPort). Actual: $($boundPorts -join ', ')"
        }

        $runs += [pscustomobject]@{
            Name = $plan.Name
            PID = $process.Id
            GamePort = $plan.GamePort
            A2SPort = $plan.A2SPort
            Profile = $profile
            Config = $plan.Config
        }
    }

    $runs | Export-Csv -LiteralPath (Join-Path $batchRoot "runs.csv") -NoTypeInformation -Encoding UTF8
    $runs | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $batchRoot "runs.json") -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $PSScriptRoot "active-parallel-batch.txt") -Value $batchRoot -Encoding UTF8
    $runs | Format-Table -AutoSize
    "Batch: $batchRoot"
}
catch {
    $runs | ForEach-Object { Stop-Process -Id $_.PID -ErrorAction SilentlyContinue }
    if ($process -and (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)) {
        Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
    }
    throw
}
