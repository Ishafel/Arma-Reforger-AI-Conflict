$ErrorActionPreference = "Stop"

$batchPointer = Join-Path $PSScriptRoot "active-parallel-batch.txt"
if (-not (Test-Path -LiteralPath $batchPointer)) {
    throw "No active parallel batch pointer found."
}

$batchRoot = (Get-Content -LiteralPath $batchPointer -Raw).Trim()
$runsPath = Join-Path $batchRoot "runs.csv"
if (-not (Test-Path -LiteralPath $runsPath)) {
    throw "Run manifest not found: $runsPath"
}

$runs = Import-Csv -LiteralPath $runsPath
foreach ($run in $runs) {
    $process = Get-Process -Id ([int]$run.PID) -ErrorAction SilentlyContinue
    if ($process -and $process.ProcessName -like "ArmaReforgerServer*") {
        Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
    }
}

Remove-Item -LiteralPath $batchPointer -Force
"Stopped recorded parallel servers from $batchRoot"
