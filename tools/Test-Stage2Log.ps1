param(
    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [int]$MaxRepeatedOrderRecoveries = 3,
    [int]$MaxRouteReplansWithoutProgress = 12,
    [int]$MaxFailedBarracksVisits = 2
)

$ErrorActionPreference = 'Stop'
$resolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
$lines = Get-Content -LiteralPath $resolvedLog
$failures = [System.Collections.Generic.List[string]]::new()

$stage2Errors = $lines | Select-String -SimpleMatch '[AICF][STAGE2][ERROR]'
$stage1Errors = $lines | Select-String -SimpleMatch '[AICF][STAGE1][ERROR]'
$scriptErrors = $lines | Select-String -Pattern 'SCRIPT\s+\(E\)'
$resultFailures = $lines | Select-String -SimpleMatch '[AICF][STAGE1][RESULT][FAIL]'
$bindings = $lines | Select-String -Pattern '\[AICF\]\[STAGE2\]\[INFO\]\[SPAWN_BOUND\].*faction=(US|USSR) slot=([0-9]+) generation=([0-9]+) group=([^ ]+)'
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
    $match = [regex]::Match($binding.Line, 'faction=(US|USSR) slot=([0-9]+) generation=([0-9]+) group=([^ ]+)')
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
    $match = [regex]::Match($recovery.Line, 'faction=(US|USSR) slot=([0-9]+).*target=([^ ]+)')
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

# Счётчики переживают замену waypoint, token и assignment. Только физический
# progress, новое воплощение группы или подтверждённая смена контекста отделяют
# эпизоды. success=1/hold не progress.
$routeReplans = @{}
$failedVisits = @{}
foreach ($line in $lines) {
    if ($line -notmatch '\[AICF\].*\[(FALSE_COMPLETION_ROUTE_REPLAN|RECOVERY_ROUTE_LEG_COMPLETED|ROUTE_RECOVERY_EXHAUSTED|ROUTE_RECOVERY_EPISODE_FINISHED|INFANTRY_RECRUITMENT_(?:FINISHED|PROGRESS|ARRIVED|APPROACH_REARMED))\]') { continue }
    $event = $Matches[1]
    $fields = @{}
    foreach ($field in [regex]::Matches($line, '(\w+)=([^\s]+)')) { $fields[$field.Groups[1].Value] = $field.Groups[2].Value }
    $slot = $fields.numeric_slot
    if ($null -eq $slot) { $slot = $fields.slot }
    $generation = $fields.group_generation
    if ($null -eq $generation) { $generation = $fields.generation }
    if ($null -eq $slot -or $null -eq $generation -or -not $fields.faction) { continue }
    $key = "$($fields.faction):${slot}:$generation"
    if ($event -eq 'ROUTE_RECOVERY_EXHAUSTED' -and $fields.hold_committed -eq '0') {
        $failures.Add("Terminal route hold was rejected: $key")
    }
    if ($event -eq 'FALSE_COMPLETION_ROUTE_REPLAN') {
        $routeReplans[$key] = 1 + [int]$routeReplans[$key]
        if ($routeReplans[$key] -eq $MaxRouteReplansWithoutProgress + 1) {
            $failures.Add("Route replan cycle without physical progress: $key (limit $MaxRouteReplansWithoutProgress)")
        }
    } elseif ($event -eq 'RECOVERY_ROUTE_LEG_COMPLETED' -or
        ($event -eq 'ROUTE_RECOVERY_EPISODE_FINISHED' -and $fields.outcome -in @('PHYSICAL_PROGRESS','CONTEXT_CHANGED'))) {
        $routeReplans[$key] = 0
    }
    if ($event -like 'INFANTRY_RECRUITMENT_*') {
        $service = $fields.service
        if (-not $service) { $service = $fields.base }
        $visitKey = "${key}:$service"
        if ($event -eq 'INFANTRY_RECRUITMENT_FINISHED' -and $fields.reason -match '^APPROACH_') {
            $failedVisits[$visitKey] = 1 + [int]$failedVisits[$visitKey]
            if ($failedVisits[$visitKey] -eq $MaxFailedBarracksVisits + 1) {
                $failures.Add("Repeated failed barracks approach: $visitKey (limit $MaxFailedBarracksVisits)")
            }
        } elseif ($event -in @('INFANTRY_RECRUITMENT_PROGRESS','INFANTRY_RECRUITMENT_ARRIVED') -or
            ($event -eq 'INFANTRY_RECRUITMENT_APPROACH_REARMED' -and $fields.reason -eq 'CONTEXT_CHANGED')) {
            $failedVisits[$visitKey] = 0
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Host 'STAGE2 LOG CHECK: FAIL' -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "- $_" }
    exit 1
}

Write-Host 'STAGE2 LOG CHECK: PASS' -ForegroundColor Green
Write-Host "bindings=$($bindings.Count) heartbeats=$($heartbeats.Count) recoveries=$($orderRecovered.Count) stuck_recycled=$($groupsRecycled.Count)"
