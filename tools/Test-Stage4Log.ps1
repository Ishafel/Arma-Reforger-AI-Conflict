param(
    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [switch]$AllowActiveAtEnd
)

$ErrorActionPreference = 'Stop'
$resolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
$lines = Get-Content -LiteralPath $resolvedLog
$failures = [System.Collections.Generic.List[string]]::new()

$config = @($lines | Select-String -Pattern '\[AICF\]\[STAGE4\]\[INFO\]\[CONFIG\].*enabled=1')
$aicfErrors = @($lines | Select-String -Pattern '\[AICF\]\[[^\]]+\]\[ERROR\]')
$scriptErrors = @($lines | Select-String -Pattern 'SCRIPT\s+\((?:E|F)\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer')
$probes = @($lines | Select-String -Pattern '\[AICF\]\[STAGE4\]\[INFO\]\[SUPPLY_PROBE\].*supplies=.*supplies_max=')
$reservations = @($lines | Select-String -Pattern '\[DEPLOYMENT_RESERVED\].*request=([0-9]+) token=([0-9]+).*faction=(US|USSR).*slot=([0-9]).*supply_cost=([0-9]+)')
$commits = @($lines | Select-String -Pattern '\[DEPLOYMENT_COMMITTED\].*request=([0-9]+) token=([0-9]+).*faction=(US|USSR).*slot=([0-9]).*ticket_debit=1 supply_debit=([0-9]+) roster=(10|[1-9])\/\6')
$aborts = @($lines | Select-String -Pattern '\[DEPLOYMENT_ABORTED\].*request=([0-9]+) token=([0-9]+).*ticket_rollback=1 supply_rollback=([0-9]+)')
$balanceFailures = @($lines | Select-String -SimpleMatch '[SHIPMENT_BALANCE_FAILED]')
$badHeartbeatBalance = @($lines | Select-String -Pattern '\[AICF\]\[STAGE4\]\[INFO\]\[HEARTBEAT\].*balance_delta=(?!0(?:\s|$))-?[0-9]+')
$approachReissues = @($lines | Select-String -SimpleMatch '[VEHICLE_SPAWN_APPROACH_STARTED]' | Where-Object { $_.Line -match '\breason=REISSUED\b' })
$occupiedPlans = @($lines | Select-String -SimpleMatch '[VEHICLE_SPAWN_PLAN_CANCELLED]' | Where-Object { $_.Line -match '\breason=[^\s]*SPAWN_PAD_OCCUPIED\b' })
$padProbes = @($lines | Select-String -SimpleMatch '[VEHICLE_SPAWN_PAD_PROBE]')
$identityEvents = @($lines | Select-String -Pattern '\[(?:VEHICLE_SPAWN_PAD_PROBE|STRATEGIC_ASSIGNMENT|WAYPOINT_REMOVED|ORDER_WAYPOINT_TERMINAL_OBSERVED)\]')
$aiLimitBlocks = @($lines | Select-String -SimpleMatch '[REINFORCEMENT_BLOCKED]' | Where-Object { $_.Line -match '\breason=AI_LIMIT\b' })
$falseCompletions = @($lines | Select-String -SimpleMatch '[FALSE_COMPLETION]')
$mobSafetyBlocks = @($lines | Select-String -SimpleMatch '[MOB_EGRESS_BLOCKED_BY_SAFETY]')
$mobDeadlineMisses = @($lines | Select-String -SimpleMatch '[MOB_EGRESS_DEADLINE_MISSED]')

if (-not $config) { $failures.Add('Missing Stage 4 CONFIG enabled=1') }
if (-not $probes) { $failures.Add('Missing stock SUPPLY_PROBE evidence') }
if ($aicfErrors) { $failures.Add("AICF diagnostic ERROR events: $($aicfErrors.Count)") }
if ($scriptErrors) { $failures.Add("Script/VM failures: $($scriptErrors.Count)") }
if ($balanceFailures -or $badHeartbeatBalance) { $failures.Add('Shipment conservation failed') }

foreach ($falseCompletion in $falseCompletions) {
    if ($falseCompletion.Line -notmatch '\bstable_slot=S[0-9]+\b.*\bnumeric_slot=[0-9]+\b.*\bobjective_distance_m=[0-9.]+\b.*\bphysical_progress_m=[0-9.]+\b.*\bphysical_confirmation=REJECTED\b.*\breliability_budget_consumed=0\b') {
        $failures.Add("FALSE_COMPLETION lacks physical proof or consumed repair budget: $($falseCompletion.LineNumber)")
    }
}

foreach ($safetyBlock in $mobSafetyBlocks) {
    if ($safetyBlock.Line -notmatch '\bstate=EGRESS_BLOCKED_BY_SAFETY\b.*\bblocker=(?:PLAYER_FENCE|COMBAT_THREAT)\b.*\bhard_deadline=PAUSED\b.*\bacceptance_failure=0\b') {
        $failures.Add("MOB safety block lacks paused-deadline contract: $($safetyBlock.LineNumber)")
    }
}

foreach ($deadlineMiss in $mobDeadlineMisses) {
    if ($deadlineMiss.Line -match '\blast_hidden_rejection=[^\s]*(?:PLAYER_FENCE|COMBAT_THREAT)') {
        $failures.Add("Safety-blocked MOB egress leaked into hard acceptance failure: $($deadlineMiss.LineNumber)")
    }
}

foreach ($aiLimitBlock in $aiLimitBlocks) {
    $capacityMatch = [regex]::Match(
        $aiLimitBlock.Line,
        '\bstable_slot=S[0-9]+\b.*\bmanaged_agents=([0-9]+)\b.*\brequested_agents=([0-9]+)\b.*\bprojected_managed_agents=([0-9]+)\b.*\blimit=([0-9]+)\b.*\bgeneration_unchanged=([0-9]+)\b')
    if (-not $capacityMatch.Success) {
        $failures.Add("AI_LIMIT block lacks exact capacity projection: $($aiLimitBlock.LineNumber)")
        continue
    }
    $projectedAgents = [int]$capacityMatch.Groups[3].Value
    $agentLimit = [int]$capacityMatch.Groups[4].Value
    if ($projectedAgents -le $agentLimit) {
        $failures.Add("False AI_LIMIT block: projected=$projectedAgents limit=$agentLimit line=$($aiLimitBlock.LineNumber)")
    }
}

# A plan may retry one staging waypoint after a delayed external queue loss.
# Repeated reissues for the same reservation reproduce the per-second command
# and voice churn seen in Full-Stage3-4-20260816-094529.
$approachReissuesByReservation = @{}
foreach ($reissue in $approachReissues) {
    $reservationMatch = [regex]::Match($reissue.Line, '\breservation=([^\s]+)')
    if (-not $reservationMatch.Success) {
        $failures.Add('Vehicle approach reissue has no reservation identity')
        continue
    }
    $reservationId = $reservationMatch.Groups[1].Value
    if (-not $approachReissuesByReservation.ContainsKey($reservationId)) {
        $approachReissuesByReservation[$reservationId] = 0
    }
    $approachReissuesByReservation[$reservationId]++
}
foreach ($reservationId in $approachReissuesByReservation.Keys) {
    if ($approachReissuesByReservation[$reservationId] -gt 1) {
        $failures.Add("Vehicle staging waypoint reissue churn: reservation=$reservationId count=$($approachReissuesByReservation[$reservationId]) maximum=1")
    }
}

# Every exact spawn-pad rejection must identify its reservation and carry the
# probe geometry plus nearby blocker candidates. This makes a false collision
# distinguishable from an AI, player, vehicle or world object in one log.
$occupiedProbeByReservation = @{}
foreach ($probe in $padProbes) {
    $probeMatch = [regex]::Match($probe.Line, '\breservation=([^\s]+).*\bresult=OCCUPIED\b')
    if ($probeMatch.Success) {
        $occupiedProbeByReservation[$probeMatch.Groups[1].Value] = $probe.Line
    }
}
$checkedOccupiedReservations = @{}
foreach ($occupiedPlan in $occupiedPlans) {
    $reservationMatch = [regex]::Match($occupiedPlan.Line, '\breservation=([^\s]+)')
    if (-not $reservationMatch.Success) {
        $failures.Add('SPAWN_PAD_OCCUPIED plan has no reservation identity')
        continue
    }
    $reservationId = $reservationMatch.Groups[1].Value
    if ($checkedOccupiedReservations.ContainsKey($reservationId)) {
        continue
    }
    $checkedOccupiedReservations[$reservationId] = $true
    if (-not $occupiedProbeByReservation.ContainsKey($reservationId)) {
        $failures.Add("SPAWN_PAD_OCCUPIED has no OCCUPIED pad probe: reservation=$reservationId")
        continue
    }
    $probeLine = $occupiedProbeByReservation[$reservationId]
    if ($probeLine -notmatch '\bstable_slot=S[0-9]+\b.*\bnumeric_slot=[0-9]+\b.*\bprobe=EXACT_TRACE_CYLINDER\b.*\bclearance_radius_m=[0-9.]+\b.*\bclearance_height_m=[0-9.]+\b.*\bblocking_trace_count=[1-9][0-9]*\b.*\bblocking_traces=\[.*\bnearby_entity_count=[0-9]+\b.*\bnearby_entities=\[') {
        $failures.Add("Incomplete spawn-pad blocker diagnostics: reservation=$reservationId")
    }
}

$usesStableIdentityContract = @($lines | Select-String -Pattern '\bstable_slot=S[0-9]+\b').Count -gt 0
if ($usesStableIdentityContract) {
    foreach ($identityEvent in $identityEvents) {
        if ($identityEvent.Line -notmatch '\bstable_slot=S[0-9]+\b.*\bnumeric_slot=[0-9]+\b') {
            $failures.Add("Dynamic slot event lacks stable identity: $($identityEvent.LineNumber)")
        }
    }
}

$terminalByKey = @{}
foreach ($terminal in @($commits + $aborts)) {
    $match = [regex]::Match($terminal.Line, 'request=([0-9]+) token=([0-9]+)')
    if (-not $match.Success) { continue }
    $key = "$($match.Groups[1].Value):$($match.Groups[2].Value)"
    if ($terminalByKey.ContainsKey($key)) {
        $failures.Add("Duplicate deployment terminal outcome: $key")
    } else {
        $terminalByKey[$key] = $terminal.Line
    }
}

$reservationByKey = @{}
foreach ($reservationLine in $reservations) {
    $match = [regex]::Match($reservationLine.Line, 'request=([0-9]+) token=([0-9]+).*supply_cost=([0-9]+)')
    if (-not $match.Success) { continue }
    $key = "$($match.Groups[1].Value):$($match.Groups[2].Value)"
    if ($reservationByKey.ContainsKey($key)) {
        $failures.Add("Duplicate deployment reservation: $key")
    } else {
        $reservationByKey[$key] = $reservationLine.Line
    }
    if (-not $terminalByKey.ContainsKey($key) -and -not $AllowActiveAtEnd) {
        $failures.Add("Reservation has no commit/abort terminal outcome: $key")
    }
    if ($terminalByKey.ContainsKey($key)) {
        $terminalSupply = [regex]::Match($terminalByKey[$key], '(?:supply_debit|supply_rollback)=([0-9]+)')
        if ($terminalSupply.Success -and $terminalSupply.Groups[1].Value -ne $match.Groups[3].Value) {
            $failures.Add("Supply amount mismatch for $key")
        }
    }
}

foreach ($key in $terminalByKey.Keys) {
    if (-not $reservationByKey.ContainsKey($key)) {
        $failures.Add("Deployment terminal outcome has no reservation: $key")
    }
}

$dispatches = @($lines | Select-String -Pattern '\[SHIPMENT_DISPATCHED\].*shipment=([0-9]+).*cargo=([0-9]+)')
$shipmentTerminals = @($lines | Select-String -Pattern '\[(SHIPMENT_DELIVERED|SHIPMENT_RETURNED)\].*shipment=([0-9]+)')
$shipmentTerminalById = @{}
foreach ($terminal in $shipmentTerminals) {
    $match = [regex]::Match($terminal.Line, '\[(?:SHIPMENT_DELIVERED|SHIPMENT_RETURNED)\].*shipment=([0-9]+)')
    if (-not $match.Success) { continue }
    $shipmentId = $match.Groups[1].Value
    if ($shipmentTerminalById.ContainsKey($shipmentId)) {
        $failures.Add("Duplicate shipment terminal outcome: $shipmentId")
    } else {
        $shipmentTerminalById[$shipmentId] = $terminal.Line
    }
}

$dispatchById = @{}
foreach ($dispatch in $dispatches) {
    $match = [regex]::Match($dispatch.Line, '\[SHIPMENT_DISPATCHED\].*shipment=([0-9]+).*cargo=([0-9]+)')
    if (-not $match.Success) { continue }
    $shipmentId = $match.Groups[1].Value
    if ($dispatchById.ContainsKey($shipmentId)) {
        $failures.Add("Duplicate shipment dispatch: $shipmentId")
    } else {
        $dispatchById[$shipmentId] = $dispatch.Line
    }
    if (-not $shipmentTerminalById.ContainsKey($shipmentId) -and -not $AllowActiveAtEnd) {
        $failures.Add("Shipment has no delivery/return terminal outcome: $shipmentId")
    }
}

foreach ($shipmentId in $shipmentTerminalById.Keys) {
    if (-not $dispatchById.ContainsKey($shipmentId)) {
        $failures.Add("Shipment terminal outcome has no dispatch: $shipmentId")
    }
}

if ($failures.Count -gt 0) {
    Write-Host "Stage 4 log audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host 'Stage 4 log audit: PASS' -ForegroundColor Green
Write-Host "probes=$($probes.Count) reservations=$($reservations.Count) commits=$($commits.Count) aborts=$($aborts.Count) shipments=$($dispatches.Count) approach_reissues=$($approachReissues.Count) pad_probes=$($padProbes.Count) ai_limit_blocks=$($aiLimitBlocks.Count) false_completions=$($falseCompletions.Count) mob_safety_blocks=$($mobSafetyBlocks.Count)"
