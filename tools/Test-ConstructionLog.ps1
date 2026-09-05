[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [ValidateSet('BOTH','US','USSR')][string]$ExpectedMode = 'BOTH',
    [switch]$RequireCompletion,
    [switch]$RequireAllTypes
)
$ErrorActionPreference = 'Stop'
$lines = Get-Content -LiteralPath $LogPath
$raw = $lines -join "`n"
$failures = [System.Collections.Generic.List[string]]::new()
if ($raw -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer') { $failures.Add('CONSTRUCTION_ENGINE_ERROR') }
if ($raw -notmatch 'Game destroyed\.') { $failures.Add('CONSTRUCTION_LOG_NOT_STOPPED') }
$ready = $false
$orders = @{}
$active = @{}
$lastDecision = @{}
$completedTypes = @{}
$placed = 0
$completed = 0
$placementsPerTick = @{}
$builderProgress = @{}
$builderCompleted = @{}
foreach ($line in $lines) {
    if ($line -match '\[CONSTRUCTION_DEFERRED_PROBE\].*?accepted=(\d+).*?props_current=(-?\d+) props_expected=(-?\d+).*?root_present=(\d+)') {
        if ($Matches[2] -ne $Matches[3]) { $failures.Add('CONSTRUCTION_DEFERRED_PROP_BALANCE') }
        if ($Matches[1] -eq '0' -and $Matches[4] -ne '0') { $failures.Add('CONSTRUCTION_FAILED_ROOT_RETAINED') }
    }
    if ($line -match '\[ROSTER_READY\]') { $ready = $true }
    if ($line -match '\[(BUILDER_PROGRESS|BUILDER_COMPLETED)\].*?target=(\S+)') {
        $builderEvent = $Matches[1]; $layout = $Matches[2]
        if ($line -match 'tool_active=1 item_using=1') {
            if ($builderEvent -eq 'BUILDER_PROGRESS') { $builderProgress[$layout] = $true }
            elseif ($builderProgress.ContainsKey($layout)) { $builderCompleted[$layout] = $true }
        }
    }
    if ($line -notmatch '\[(CONSTRUCTION_[A-Z_]+)\].*?t_ms=(\d+)\s') { continue }
    $event = $Matches[1]; $time = [int]$Matches[2]
    $fields = @{}
    foreach ($match in [regex]::Matches($line, '(\w+)=(\S+)')) { $fields[$match.Groups[1].Value] = $match.Groups[2].Value }
    if (-not $fields.ContainsKey('token')) { continue }
    $token = $fields.token; $base = $fields.base
    switch ($event) {
        'CONSTRUCTION_DECISION' {
            if (-not $ready) { $failures.Add('CONSTRUCTION_BEFORE_ROSTER') }
            if ($ExpectedMode -ne 'BOTH' -and $fields.faction -ne $ExpectedMode) { $failures.Add('CONSTRUCTION_PLAYER_SIDE') }
            if ($orders.ContainsKey($token) -or $active.ContainsKey($base)) { $failures.Add('CONSTRUCTION_DUPLICATE_ORDER') }
            if ($lastDecision.ContainsKey($base) -and $time - $lastDecision[$base] -lt 60000) { $failures.Add('CONSTRUCTION_DECISION_TOO_EARLY') }
            $orders[$token] = @{ Paid=0; Placed=0; Completed=0; Reserved=0; Base=$base; Type=$fields.type; Site=$false }
            $active[$base] = $token
            $lastDecision[$base] = $time
        }
        'CONSTRUCTION_SITE_SELECTED' {
            if (-not $orders.ContainsKey($token)) { $failures.Add('CONSTRUCTION_SITE_WITHOUT_ORDER') }
            else { $orders[$token].Site = $true }
        }
        'CONSTRUCTION_RESERVED' {
            if (-not $orders.ContainsKey($token) -or -not $orders[$token].Site) { $failures.Add('CONSTRUCTION_RESERVE_WITHOUT_SITE') }
            else { $orders[$token].Reserved++ }
            if ($fields.supply_debited -ne '0') { $failures.Add('CONSTRUCTION_EARLY_DEBIT') }
        }
        'CONSTRUCTION_PAYMENT' {
            if (-not $orders.ContainsKey($token) -or $orders[$token].Reserved -ne 1) { $failures.Add('CONSTRUCTION_PAYMENT_WITHOUT_RESERVATION') }
            else { $orders[$token].Paid++; $orders[$token].Reserved = 0 }
            if ([math]::Abs([double]$fields.supplies_before - [double]$fields.supplies_after - [double]$fields.cost) -gt 0.01) { $failures.Add('CONSTRUCTION_DEBIT_BALANCE') }
            if ($fields.ContainsKey('props_cost') -and [int]$fields.props_after - [int]$fields.props_before -ne [int]$fields.props_cost) { $failures.Add('CONSTRUCTION_PROP_BALANCE') }
            if ([double]$fields.supplies_after -lt [double]$fields.reserve) { $failures.Add('CONSTRUCTION_RESERVE_BREACHED') }
            if ($fields.debit_count -ne '1' -or $fields.tickets -ne '0' -or $fields.reservation_released -ne '1') { $failures.Add('CONSTRUCTION_PAYMENT_CONTRACT') }
        }
        'CONSTRUCTION_PLACED' {
            if (-not $orders.ContainsKey($token) -or $orders[$token].Paid -ne 1 -or $orders[$token].Placed -ne 0) { $failures.Add('CONSTRUCTION_PLACEMENT_PAYMENT') }
            else { $orders[$token].Placed++; $orders[$token].Layout = $fields.layout; $placed++ }
            if ($fields.service_online -ne '0') { $failures.Add('CONSTRUCTION_PREMATURE_SERVICE') }
            $tick = [math]::Floor($time / 1000)
            if ($placementsPerTick.ContainsKey($tick)) { $failures.Add('CONSTRUCTION_PLACEMENT_BUDGET') }
            $placementsPerTick[$tick] = $true
        }
        'CONSTRUCTION_ROLLBACK' {
            if ($fields.restored -ne '1' -or $fields.stock_refund -ne '0' -or [math]::Abs([double]$fields.supplies_before - [double]$fields.supplies_after) -gt 0.01) { $failures.Add('CONSTRUCTION_ROLLBACK_BALANCE') }
            if ($fields.ContainsKey('props_after') -and $fields.props_after -ne $fields.props_before) { $failures.Add('CONSTRUCTION_ROLLBACK_PROPS') }
            if ($orders.ContainsKey($token)) { $orders[$token].Reserved = 0 }
        }
        'CONSTRUCTION_COMPLETED' {
            if (-not $orders.ContainsKey($token) -or $orders[$token].Placed -ne 1 -or $orders[$token].Completed -ne 0) { $failures.Add('CONSTRUCTION_INVALID_COMPLETION') }
            else { $orders[$token].Completed++; $completed++; $completedTypes[$fields.type] = $true }
            if ($fields.service_online -ne '1') { $failures.Add('CONSTRUCTION_SERVICE_NOT_ONLINE') }
            if ($orders.ContainsKey($token) -and $orders[$token].Layout -ne $fields.layout) { $failures.Add('CONSTRUCTION_LAYOUT_IDENTITY') }
            if ($RequireCompletion -and -not $builderCompleted.ContainsKey($fields.layout)) { $failures.Add('CONSTRUCTION_NO_BUILDER_COMPLETION') }
            $active.Remove($base)
        }
        'CONSTRUCTION_CANCELLED' {
            if ($orders.ContainsKey($token) -and $orders[$token].Reserved -ne 0) { $failures.Add('CONSTRUCTION_RESERVATION_LEAK') }
            if ($fields.reservation_released -ne '1') { $failures.Add('CONSTRUCTION_SITE_LEAK') }
            $active.Remove($base)
        }
    }
}
if ($orders.Count -eq 0) { $failures.Add('CONSTRUCTION_NO_DECISIONS') }
if ($RequireCompletion -and $completed -eq 0) { $failures.Add('CONSTRUCTION_NO_COMPLETION') }
if ($RequireAllTypes -and $completedTypes.Count -ne 5) { $failures.Add('CONSTRUCTION_MISSING_TYPES') }
if ($active.Count -gt 0) { $failures.Add('CONSTRUCTION_PENDING_AT_STOP') }
if ($failures.Count) {
    Write-Output "Construction log audit: FAIL ($($failures.Count) issues) placed=$placed completed=$completed"
    $failures | Sort-Object -Unique | ForEach-Object { Write-Output " - [$_]" }
    exit 1
}
Write-Output "Construction log audit: PASS orders=$($orders.Count) placed=$placed completed=$completed types=$($completedTypes.Count)"
