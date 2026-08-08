param(
    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [int]$MaxRepeatedOrderRecoveries = 3
)

$ErrorActionPreference = 'Stop'
$resolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
$lines = Get-Content -LiteralPath $resolvedLog
$failures = [System.Collections.Generic.List[string]]::new()

$stage2Errors = $lines | Select-String -SimpleMatch '[AICF][STAGE2][ERROR]'
$stage1Errors = $lines | Select-String -SimpleMatch '[AICF][STAGE1][ERROR]'
$scriptErrors = $lines | Select-String -Pattern 'SCRIPT\s+\(E\)'
$resultFailures = $lines | Select-String -SimpleMatch '[AICF][STAGE1][RESULT][FAIL]'
$bindings = $lines | Select-String -Pattern '\[AICF\]\[STAGE2\]\[INFO\]\[SPAWN_BOUND\].*faction=(US|USSR) slot=([0-3]) generation=([0-9]+) group=([^ ]+)'
$heartbeats = $lines | Select-String -SimpleMatch '[AICF][STAGE2][INFO][RELIABILITY_HEARTBEAT]'
$testConfigured = $lines | Select-String -SimpleMatch '[AICF][STAGE2][WARNING][TEST_HOOK_CONFIGURED]'
$testDropped = $lines | Select-String -SimpleMatch '[AICF][STAGE2][WARNING][TEST_ORDER_DROPPED]'
$orderRecovered = $lines | Select-String -SimpleMatch '[AICF][STAGE2][INFO][ORDER_RECOVERED]'
$persistentLegacy = $lines | Select-String -Pattern '\[GROUP_STUCK_PERSISTENT\].*action=CONTINUE_ROUTE_REBUILDS'
$persistentRecycle = $lines | Select-String -Pattern '\[GROUP_STUCK_PERSISTENT\].*action=RECYCLE_GROUP'
$groupsRecycled = $lines | Select-String -SimpleMatch '[AICF][STAGE2][INFO][GROUP_RECYCLED]'

if ($stage2Errors) {
    $failures.Add("Stage 2 errors: $($stage2Errors.Count)")
}
if ($stage1Errors) {
    $failures.Add("Stage 1 errors: $($stage1Errors.Count)")
}
if ($scriptErrors) {
    $failures.Add("Server SCRIPT (E) lines: $($scriptErrors.Count)")
}
if ($resultFailures) {
    $failures.Add("Stage 1 RESULT FAIL lines: $($resultFailures.Count)")
}
if ($persistentLegacy) {
    $failures.Add("Persistent stuck still continues route rebuilds: $($persistentLegacy.Count)")
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

$recoveryCounts = @{}
foreach ($recovery in $orderRecovered) {
    $match = [regex]::Match($recovery.Line, 'faction=(US|USSR) slot=([0-3]).*target=([^ ]+)')
    if (-not $match.Success) {
        continue
    }

    $key = "$($match.Groups[1].Value):$($match.Groups[2].Value):$($match.Groups[3].Value)"
    if (-not $recoveryCounts.ContainsKey($key)) {
        $recoveryCounts[$key] = 0
    }
    $recoveryCounts[$key]++
}

foreach ($key in $recoveryCounts.Keys) {
    if ($recoveryCounts[$key] -gt $MaxRepeatedOrderRecoveries) {
        $failures.Add("Order recovery churn for $key`: $($recoveryCounts[$key]) (limit $MaxRepeatedOrderRecoveries)")
    }
}

if ($persistentRecycle.Count -gt $groupsRecycled.Count) {
    $failures.Add("Persistent stuck recycle was not completed: persistent=$($persistentRecycle.Count) recycled=$($groupsRecycled.Count)")
}

if ($failures.Count -gt 0) {
    Write-Host 'STAGE2 LOG CHECK: FAIL' -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "- $_" }
    exit 1
}

Write-Host 'STAGE2 LOG CHECK: PASS' -ForegroundColor Green
Write-Host "bindings=$($bindings.Count) heartbeats=$($heartbeats.Count) recoveries=$($orderRecovered.Count) stuck_recycled=$($groupsRecycled.Count)"
