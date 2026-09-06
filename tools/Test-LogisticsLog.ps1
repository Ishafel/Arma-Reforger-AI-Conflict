param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [switch]$AllowActiveAtEnd,
    [switch]$RequireDelivery,
    [switch]$RequirePolicy,
    [switch]$RequireLedger,
    [switch]$RequireGraph,
    [switch]$RequireSearch,
    [string]$ClientLogPath,
    [switch]$RequireLoadedClient,
    [int]$MinimumDurationMs = 0
)
$ErrorActionPreference = 'Stop'
$lines = Get-Content -LiteralPath (Resolve-Path -LiteralPath $LogPath)
$failures = [Collections.Generic.List[string]]::new()
$operations = @{}
$ready = @{}
$replicas = @{}
$clientSamples = 0
$loadedClientSamples = 0
$stopped = $false
$configured = $false
$deliveryCount = 0
$balanceCount = 0
$policy = $false
$ledgerContract = $false
$graphContract = $false
$searchContract = $false
$maxTime = 0
$rosterReady = $false
$released = @{}
# Полный console плюс соседние engine logs: dedicated иногда возвращает 0
# при compile error, записанной только в error.log/crash.log.
if ([IO.Path]::GetFileName($LogPath) -eq 'console.log') {
    foreach ($companion in @('error.log','crash.log')) {
        $companionPath = Join-Path (Split-Path -Parent $LogPath) $companion
        if (Test-Path -LiteralPath $companionPath) {
            $fatal = Select-String -LiteralPath $companionPath -Pattern 'SCRIPT\s+\((E|F)\)|Can.t compile|Virtual Machine Exception|Application crashed!|Unable to start replication|Unable to initialize the game'
            foreach ($entry in $fatal) { $failures.Add("ENGINE_COMPANION_ERROR file=$companion line=$($entry.LineNumber)") }
        }
    }
}
function Number([hashtable]$fields, [string]$name, [int]$line) {
    $value = 0.0
    if (-not $fields.ContainsKey($name) -or -not [double]::TryParse($fields[$name], [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$value)) {
        $failures.Add("NUMBER line=$line field=$name"); return 0.0
    }
    if ([double]::IsNaN($value) -or [double]::IsInfinity($value)) { $failures.Add("NONFINITE line=$line field=$name"); return 0.0 }
    return $value
}
for ($index=0; $index -lt $lines.Count; $index++) {
    $line = $lines[$index]
    if ($line -match '\[STAGE1\]\[INFO\]\[ROSTER_READY\]') { $rosterReady = $true }
    if ($line -match 'SCRIPT\s+\((E|F)\)|Virtual Machine Exception|NULL pointer|Application crashed!|Unable to start replication|Unable to initialize the game|\[AICF\].*\[ERROR\]') { $failures.Add("ENGINE_OR_AICF_ERROR line=$index") }
    if ($line -match '\[LOGISTICS_(PROBE_)?STOP\]') { $stopped = $true }
    if ($line -match '\[LOGISTICS_POLICY_CONTRACT\].*passed=15 total=15') { $policy = $true }
    if ($line -match '\[LOGISTICS_PROBE_LEDGER_CONTRACT\].*passed=18 total=18') { $ledgerContract = $true }
    if ($line -match '\[LOGISTICS_PROBE_GRAPH_CONTRACT\].*passed=12 total=12') { $graphContract = $true }
    if ($line -match '\[LOGISTICS_PROBE_SEARCH_CONTRACT\].*passed=6 total=6') { $searchContract = $true }
    if ($line -notmatch '\[(LOGISTICS_[A-Z_]+|HEARTBEAT)\]') { continue }
    $eventName = $Matches[1]
    $fields = @{}
    foreach ($pair in [regex]::Matches($line, '\b([a-z_]+)=([^\s]+)')) { $fields[$pair.Groups[1].Value] = $pair.Groups[2].Value }
    if ($fields.ContainsKey('t_ms')) { $maxTime = [Math]::Max($maxTime, (Number $fields 't_ms' $index)) }
    if ($eventName -eq 'LOGISTICS_CONFIG') {
        $configured = $fields['schema_version'] -eq '2'
        continue
    }
    if ($eventName -match '^LOGISTICS_PROBE|^LOGISTICS_POLICY|^LOGISTICS_CONFIG|^LOGISTICS_STOP') { continue }
    if ($fields['unknown_state'] -eq '1') { $failures.Add("UNKNOWN_RESOURCE_STATE line=$index") }
    if ($eventName -ne 'HEARTBEAT') {
        foreach ($required in @('faction','slot','generation','vehicle','driver','phase')) {
            if (-not $fields.ContainsKey($required)) { $failures.Add("IDENTITY line=$index field=$required") }
        }
    }
    $key = "$($fields['run'])/$($fields['faction'])/$($fields['slot'])/$($fields['generation'])"
    if ($eventName -in @('LOGISTICS_SPAWN_REQUESTED','LOGISTICS_DRIVER_READY','LOGISTICS_JOB_RESERVED','LOGISTICS_LOAD_COMMITTED','LOGISTICS_UNLOAD_COMMITTED')) {
        if ($stopped) { $failures.Add("WORK_AFTER_STOP line=$index") }
        if (-not $rosterReady) { $failures.Add("WORK_BEFORE_ROSTER_READY line=$index") }
    }
    if ($eventName -eq 'LOGISTICS_DRIVER_READY') {
        if ((Number $fields 'agents' $index) -ne 1 -or (Number $fields 'cargo' $index) -ne 0) { $failures.Add("DRIVER_SPAWN_POSTCONDITION line=$index") }
        $ready[$key] = $fields['vehicle']
        if ($fields.ContainsKey('vehicle_rpl') -and $fields.ContainsKey('driver_rpl')) {
            $replicaKey = "$($fields['slot'])/$($fields['generation'])/$($fields['vehicle_rpl'])/$($fields['driver_rpl'])"
            $replicas[$replicaKey] = Number $fields 'capacity' $index
        }
    }
    if ($eventName -in @('LOGISTICS_JOB_RESERVED','LOGISTICS_LOAD_COMMITTED','LOGISTICS_UNLOAD_COMMITTED')) {
        if (-not $ready.ContainsKey($key) -or $ready[$key] -ne $fields['vehicle']) { $failures.Add("WORK_BEFORE_EXACT_READY line=$index") }
    }
    if ($eventName -eq 'LOGISTICS_EXTERNAL_TRANSFER') {
        $externalDelta = (Number $fields 'after' $index) - (Number $fields 'before' $index)
        if ([Math]::Abs($externalDelta - (Number $fields 'delta' $index)) -gt 0.01) { $failures.Add("EXTERNAL_READBACK line=$index") }
    }
    if ($eventName -eq 'LOGISTICS_CARGO_LOST') {
        $loss = (Number $fields 'before' $index) - (Number $fields 'after' $index)
        if ($loss -le 0 -or [Math]::Abs($loss - (Number $fields 'amount' $index)) -gt 0.01) { $failures.Add("LOSS_READBACK line=$index") }
    }
    if ($eventName -eq 'LOGISTICS_CARGO_RELEASED') {
        if ($released.ContainsKey($key) -or (Number $fields 'amount' $index) -lt 0) { $failures.Add("DUPLICATE_OR_INVALID_RELEASE line=$index") }
        $released[$key] = $true
    }
    if ($eventName -in @('LOGISTICS_LOAD_COMMITTED','LOGISTICS_UNLOAD_COMMITTED')) {
        $operation = "$($fields['run'])/$($fields['operation'])"
        if (-not $fields.ContainsKey('operation') -or $operations.ContainsKey($operation)) { $failures.Add("DUPLICATE_OPERATION line=$index") }
        $operations[$operation] = $true
        $amount = Number $fields 'amount' $index
        $debit = (Number $fields 'from_before' $index) - (Number $fields 'from_after' $index)
        $credit = (Number $fields 'to_after' $index) - (Number $fields 'to_before' $index)
        if ($amount -le 0 -or [Math]::Abs($debit - $amount) -gt 0.01 -or [Math]::Abs($credit - $amount) -gt 0.01) { $failures.Add("PAIR_CONSERVATION line=$index") }
        if ([Math]::Abs((Number $fields 'discrepancy' $index)) -gt 0.01) { $failures.Add("TRANSFER_DISCREPANCY line=$index") }
        if ($fields['from_pool'] -eq $fields['to_pool']) { $failures.Add("SELF_TRANSFER line=$index") }
        if ($eventName -eq 'LOGISTICS_UNLOAD_COMMITTED' -and $fields['purpose'] -eq 'DELIVERY') { $deliveryCount++ }
        if ($fields['purpose'] -eq 'RETURN' -and (-not $fields.ContainsKey('return_destination') -or -not $fields.ContainsKey('original_source') -or -not $fields.ContainsKey('same_pool'))) { $failures.Add("RETURN_PROVENANCE line=$index") }
    }
    if ($eventName -eq 'LOGISTICS_TRANSFER_FAILED') { $failures.Add("TRANSFER_FAILED_CLOSED line=$index") }
    if ($eventName -eq 'LOGISTICS_BALANCE' -or ($eventName -eq 'HEARTBEAT' -and $fields['schema_version'] -eq '2')) {
        $balanceCount++
        if ($eventName -eq 'LOGISTICS_BALANCE' -and $fields['fault'] -eq '1') { $failures.Add("CARGO_CUSTODY_FAULT line=$index") }
        $loaded = 'loaded'
        if ($eventName -eq 'HEARTBEAT') { $loaded = 'dispatched' }
        $delta = (Number $fields $loaded $index) + (Number $fields 'external_in' $index)
        foreach ($outflow in @('delivered','returned','in_transit','lost','released','external_out')) { $delta -= Number $fields $outflow $index }
        $reported = Number $fields 'balance_delta' $index
        if ([Math]::Abs($delta) -gt 0.01 -or [Math]::Abs($reported - $delta) -gt 0.01) { $failures.Add("BALANCE_CONSERVATION line=$index") }
        if ([Math]::Abs((Number $fields 'discrepancy' $index)) -gt 0.01) { $failures.Add("PENDING_DISCREPANCY line=$index") }
    }
}
if (-not $configured) { $failures.Add('MISSING_SCHEMA2_CONFIG') }
if ($balanceCount -eq 0) { $failures.Add('MISSING_BALANCE_EVIDENCE') }
if (-not $AllowActiveAtEnd -and -not ($lines -match 'Game destroyed\.|Engine shutdown')) { $failures.Add('FULL_STOPPED_LOG_REQUIRED') }
if ($RequireDelivery -and $deliveryCount -eq 0) { $failures.Add('PHYSICAL_DELIVERY_NOT_OBSERVED') }
if ($RequirePolicy -and -not $policy) { $failures.Add('PRODUCTION_POLICY_CONTRACT_NOT_OBSERVED') }
if ($RequireLedger -and -not $ledgerContract) { $failures.Add('PRODUCTION_LEDGER_CONTRACT_NOT_OBSERVED') }
if ($RequireGraph -and -not $graphContract) { $failures.Add('PRODUCTION_GRAPH_CONTRACT_NOT_OBSERVED') }
if ($RequireSearch -and -not $searchContract) { $failures.Add('PRODUCTION_SEARCH_CONTRACT_NOT_OBSERVED') }
if ($maxTime -lt $MinimumDurationMs) { $failures.Add("DURATION_TOO_SHORT actual=$maxTime required=$MinimumDurationMs") }
if ($ClientLogPath) {
    $clientLines = Get-Content -LiteralPath (Resolve-Path -LiteralPath $ClientLogPath)
    if (-not $AllowActiveAtEnd -and -not ($clientLines -match 'Game destroyed\.|Engine shutdown')) { $failures.Add('FULL_STOPPED_CLIENT_LOG_REQUIRED') }
    foreach ($companion in @('error.log','crash.log')) {
        $clientCompanion = Join-Path (Split-Path -Parent $ClientLogPath) $companion
        if ([IO.Path]::GetFileName($ClientLogPath) -eq 'console.log' -and (Test-Path -LiteralPath $clientCompanion)) {
            foreach ($entry in (Select-String -LiteralPath $clientCompanion -Pattern 'SCRIPT\s+\((E|F)\)|Can.t compile|Virtual Machine Exception|Application crashed!|Unable to initialize the game')) {
                $failures.Add("CLIENT_COMPANION_ERROR file=$companion line=$($entry.LineNumber)")
            }
        }
    }
    for ($index=0; $index -lt $clientLines.Count; $index++) {
        $line = $clientLines[$index]
        if ($line -match 'SCRIPT\s+\((E|F)\)|Virtual Machine Exception|NULL pointer|Application crashed!|\[AICF\].*\[ERROR\]') { $failures.Add("CLIENT_ENGINE_OR_AICF_ERROR line=$index") }
        if ($line -notmatch '\[LOGISTICS_PROBE_CLIENT\]') { continue }
        $fields = @{}
        foreach ($pair in [regex]::Matches($line, '\b([a-z_]+)=([^\s]+)')) { $fields[$pair.Groups[1].Value] = $pair.Groups[2].Value }
        # Despawn/dismount samples допустимы; evidence требует live settled pilot.
        if ($fields['pilot'] -ne '1' -or $fields['supplies'] -ne '1') { continue }
        $replicaKey = "$($fields['slot'])/$($fields['generation'])/$($fields['vehicle_rpl'])/$($fields['driver_rpl'])"
        if (-not $replicas.ContainsKey($replicaKey)) { $failures.Add("CLIENT_EXACT_IDENTITY line=$index"); continue }
        $cargo = Number $fields 'cargo' $index
        $capacity = Number $fields 'capacity' $index
        if ($capacity -le 0 -or $cargo -lt 0 -or $cargo -gt $capacity -or [Math]::Abs($replicas[$replicaKey] - $capacity) -gt 0.01) { $failures.Add("CLIENT_CARGO_CAPACITY line=$index"); continue }
        $clientSamples++
        if ($cargo -gt 0) { $loadedClientSamples++ }
    }
    if ($clientSamples -eq 0) { $failures.Add('CLIENT_REPLICA_NOT_OBSERVED') }
}
if ($RequireLoadedClient -and $loadedClientSamples -eq 0) { $failures.Add('LOADED_CLIENT_REPLICA_NOT_OBSERVED') }
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
"PASS Logistics log: deliveries=$deliveryCount balances=$balanceCount duration_ms=$maxTime client_samples=$clientSamples loaded_client_samples=$loadedClientSamples"
