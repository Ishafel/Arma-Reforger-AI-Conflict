param(
    [Parameter(Mandatory = $true)]
    [string]$LogPath
)

$ErrorActionPreference = 'Stop'
$resolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
$lines = Get-Content -LiteralPath $resolvedLog
$failures = [System.Collections.Generic.List[string]]::new()

$stage2Errors = $lines | Select-String -SimpleMatch '[AICF][STAGE2][ERROR]'
$scriptErrors = $lines | Select-String -Pattern 'SCRIPT\s+\(E\)'
$bindings = $lines | Select-String -Pattern '\[AICF\]\[STAGE2\]\[INFO\]\[SPAWN_BOUND\].*faction=(US|USSR) slot=([0-3]) generation=([0-9]+) group=([^ ]+)'
$heartbeats = $lines | Select-String -SimpleMatch '[AICF][STAGE2][INFO][RELIABILITY_HEARTBEAT]'
$testConfigured = $lines | Select-String -SimpleMatch '[AICF][STAGE2][WARNING][TEST_HOOK_CONFIGURED]'
$testDropped = $lines | Select-String -SimpleMatch '[AICF][STAGE2][WARNING][TEST_ORDER_DROPPED]'
$orderRecovered = $lines | Select-String -SimpleMatch '[AICF][STAGE2][INFO][ORDER_RECOVERED]'

if ($stage2Errors) {
    $failures.Add("Stage 2 errors: $($stage2Errors.Count)")
}
if ($scriptErrors) {
    $failures.Add("Server SCRIPT (E) lines: $($scriptErrors.Count)")
}
if ($bindings.Count -lt 8) {
    $failures.Add("Expected at least 8 SPAWN_BOUND events, found $($bindings.Count)")
}
if (-not $heartbeats) {
    $failures.Add('No RELIABILITY_HEARTBEAT found')
}

$bindingKeys = @{}
$groupOwners = @{}
foreach ($binding in $bindings) {
    $match = [regex]::Match($binding.Line, 'faction=(US|USSR) slot=([0-3]) generation=([0-9]+) group=([^ ]+)')
    if (-not $match.Success) {
        continue
    }

    $bindingKey = "$($match.Groups[1].Value):$($match.Groups[2].Value):$($match.Groups[3].Value)"
    $groupKey = $match.Groups[4].Value
    if ($bindingKeys.ContainsKey($bindingKey)) {
        $failures.Add("Duplicate slot-generation binding: $bindingKey")
    } else {
        $bindingKeys[$bindingKey] = $groupKey
    }

    if ($groupOwners.ContainsKey($groupKey) -and $groupOwners[$groupKey] -ne $bindingKey) {
        $failures.Add("Group $groupKey bound to multiple slot-generations")
    } else {
        $groupOwners[$groupKey] = $bindingKey
    }
}

if ($testConfigured) {
    if (-not $testDropped) {
        $failures.Add('Test hook configured but TEST_ORDER_DROPPED is missing')
    }
    if (-not $orderRecovered) {
        $failures.Add('Test hook configured but ORDER_RECOVERED is missing')
    }
}

if ($failures.Count -gt 0) {
    Write-Host 'STAGE2 LOG CHECK: FAIL' -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "- $_" }
    exit 1
}

Write-Host 'STAGE2 LOG CHECK: PASS' -ForegroundColor Green
Write-Host "bindings=$($bindings.Count) heartbeats=$($heartbeats.Count) recoveries=$($orderRecovered.Count)"
