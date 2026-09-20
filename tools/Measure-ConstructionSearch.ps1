[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$orders = @{}
$events = [System.Collections.Generic.List[object]]::new()
$workers = @{}
$lines = @(Get-Content -LiteralPath $LogPath)
foreach ($line in $lines) {
    if ($line -notmatch '\[(BUILDER_WORK_STARTED|BUILDER_COMPLETED)\].*?t_ms=(\d+).*?target=(\S+)') { continue }
    $kind = $Matches[1]; $time = [int]$Matches[2]; $target = $Matches[3]
    if (-not $workers.ContainsKey($target)) { $workers[$target] = @{} }
    if (-not $workers[$target].ContainsKey($kind)) { $workers[$target][$kind] = $time }
}
foreach ($line in $lines) {
    if ($line -notmatch '\[(CONSTRUCTION_[A-Z_]+)\].*?token=(\S+)') { continue }
    $eventName = $Matches[1]
    $token = $Matches[2]
    $fields = @{}
    foreach ($match in [regex]::Matches($line, '(?:^|\s)([a-zA-Z_]+)=([^\s]+)')) { $fields[$match.Groups[1].Value] = $match.Groups[2].Value }
    if (-not $orders.ContainsKey($token)) {
        $orders[$token] = [ordered]@{ token=$token; faction=$fields.faction; base=$fields.base; type=$fields.type; decision_ms=$null; ready_ms=$null; selected_ms=$null; placed_ms=$null; completed_ms=$null; cancelled_ms=$null; cancel_reason=''; pause_count=0; resume_count=0; candidates=0; budget_queries=0; cause=''; offset=$null }
    }
    $order = $orders[$token]
    # Диагностика coverage может появиться до выбора типа заказа.
    if ($eventName -eq 'CONSTRUCTION_DECISION') {
        foreach ($identityField in @('faction', 'base', 'type')) { $order[$identityField] = $fields[$identityField] }
    }
    $mapping = @{ CONSTRUCTION_DECISION='decision_ms'; CONSTRUCTION_SEARCH_READY='ready_ms'; CONSTRUCTION_SITE_SELECTED='selected_ms'; CONSTRUCTION_PLACED='placed_ms'; CONSTRUCTION_COMPLETED='completed_ms'; CONSTRUCTION_CANCELLED='cancelled_ms' }
    if ($mapping.ContainsKey($eventName)) { $order[$mapping[$eventName]] = [int]$fields.t_ms }
    if ($eventName -eq 'CONSTRUCTION_CANCELLED') { $order.cancel_reason = $fields.reason }
    if ($eventName -eq 'CONSTRUCTION_SEARCH_PAUSED') { $order.pause_count++ }
    if ($eventName -eq 'CONSTRUCTION_SEARCH_RESUMED') { $order.resume_count++ }
    if ($fields.ContainsKey('queries')) { $order.budget_queries = [int]$fields.queries }
    if ($fields.ContainsKey('candidates')) { $order.candidates = [int]$fields.candidates }
    if ($fields.ContainsKey('cause')) { $order.cause = $fields.cause }
    if ($fields.ContainsKey('search_offset')) { $order.offset = [int]$fields.search_offset }
    if ($fields.ContainsKey('search_status')) { $order['search_status'] = $fields.search_status }
    if ($eventName -eq 'CONSTRUCTION_SEARCH_SUMMARY') {
        $order['rejections'] = @{}
        foreach ($key in $fields.Keys) {
            if ($key.StartsWith('rejected_')) { $order.rejections[$key.Substring(9)] = [int]$fields[$key] }
        }
    }
    if ($eventName -eq 'CONSTRUCTION_NAV_CACHE') { $order['cache_hits'] = [int]$fields.hits }
    if ($eventName -eq 'CONSTRUCTION_PENDING_EXPIRED') {
        if (-not $order.Contains('pending_expired')) { $order['pending_expired'] = 0 }
        $order['pending_expired']++
    }
    if ($fields.ContainsKey('layout') -and $fields.layout -ne '0x0000000000000000') { $order['layout'] = $fields.layout }
    if ($eventName -eq 'CONSTRUCTION_SELECTED_REJECTED') {
        if (-not $order.Contains('selected_rejections')) { $order['selected_rejections'] = 0 }
        $order['selected_rejections']++
    }
    if ($eventName -eq 'CONSTRUCTION_SEARCH_COST') {
        $order['phase_cost_ms'] = [int]$fields.t_ms
        foreach ($name in @('candidate_queries','terrain_queries','exit_queries','path_queries','commit_queries','completion_queries','inventory_queries','budget_wait_windows','path_pruned','search_cpu_ms','max_slice_ms','checkpoints','search_windows')) {
            $order[$name] = $fields[$name]
        }
    }
    $events.Add([pscustomobject]@{ event=$eventName; token=$token; time_ms=$fields.t_ms; stage=$fields.stage; candidates=$fields.candidates; budget_queries=$fields.queries; reason=$fields.reason })
}
$rows = foreach ($order in $orders.Values) {
    $order['work_started_ms'] = $null
    $order['worker_completed_ms'] = $null
    if ($order.layout -and $workers.ContainsKey($order.layout)) {
        $order.work_started_ms = $workers[$order.layout]['BUILDER_WORK_STARTED']
        $order.worker_completed_ms = $workers[$order.layout]['BUILDER_COMPLETED']
    }
    foreach ($pair in @(@('metadata_ms','ready_ms','decision_ms'), @('search_ms','selected_ms','ready_ms'), @('to_placement_ms','placed_ms','decision_ms'), @('build_ms','completed_ms','placed_ms'), @('approach_ms','work_started_ms','placed_ms'), @('work_ms','worker_completed_ms','work_started_ms'), @('work_to_service_ms','completed_ms','work_started_ms'))) {
        $order[$pair[0]] = $null
        if ($null -ne $order[$pair[1]] -and $null -ne $order[$pair[2]]) { $order[$pair[0]] = $order[$pair[1]] - $order[$pair[2]] }
    }
    $order['completion_cost_observed'] = $null -ne $order.completed_ms -and $null -ne $order.phase_cost_ms -and $order.phase_cost_ms -gt $order.placed_ms
    [pscustomobject]$order
}
$rows | Sort-Object decision_ms | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'orders.json') -Encoding UTF8
$events | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $OutputDirectory 'events.csv') -Encoding UTF8
$text = Get-Content -LiteralPath $LogPath -Raw
$scheduler = [regex]::Match($text, '\[CONSTRUCTION_SCHEDULER_COST\].*?cpu_ms=(\d+) max_update_ms=(\d+) updates=(\d+)')
$frames = @([regex]::Matches($text, 'FPS:\s*([\d.]+), frame time \(avg:\s*([\d.]+) ms, min:\s*([\d.]+) ms, max:\s*([\d.]+) ms') | ForEach-Object {
    [pscustomobject]@{fps=[double]$_.Groups[1].Value;average_ms=[double]$_.Groups[2].Value;max_ms=[double]$_.Groups[4].Value}
})
$frames | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $OutputDirectory 'frames.csv') -Encoding UTF8
$seenCandidates = [System.Collections.Generic.HashSet[string]]::new()
$repeatedCandidates = 0
foreach ($order in $rows) {
    if ($null -eq $order.offset) { continue }
    for ($i = 0; $i -lt $order.candidates; $i++) {
        $key = "$($order.faction)/$($order.base)/$($order.type)/$($order.offset + $i)"
        if (-not $seenCandidates.Add($key)) { $repeatedCandidates++ }
    }
}
$summary = [ordered]@{
    log = (Resolve-Path $LogPath).Path
    stopped = $text.Contains('Game destroyed.')
    orders = @($rows | Where-Object { $null -ne $_.decision_ms }).Count
    placed = @($rows | Where-Object { $null -ne $_.placed_ms }).Count
    completed = @($rows | Where-Object { $null -ne $_.completed_ms }).Count
    cancelled = @($rows | Where-Object { $null -ne $_.cancelled_ms }).Count
    no_safe_site = @($rows | Where-Object { $_.cancel_reason -eq 'NO_SAFE_SITE' }).Count
    paused = ($rows | Measure-Object pause_count -Sum).Sum
    budget_queries = ($rows | Measure-Object budget_queries -Sum).Sum
    completion_budget_queries_observed = ($rows | Where-Object { $_.completion_cost_observed } | Measure-Object completion_queries -Sum).Sum
    completed_orders_with_cost = @($rows | Where-Object { $_.completion_cost_observed }).Count
    candidate_attempts = ($rows | Measure-Object candidates -Sum).Sum
    repeated_candidate_indices = $repeatedCandidates
    completion_waits = [regex]::Matches($text, '\[CONSTRUCTION_COMPLETION_WAIT\]').Count
    selected_rejections = [regex]::Matches($text, '\[CONSTRUCTION_SELECTED_REJECTED\]').Count
    compute_exhausted = @($rows | Where-Object { $_.cancel_reason -eq 'SEARCH_BUDGET_EXHAUSTED' }).Count
    area_exhausted = @($rows | Where-Object { $_.cancel_reason -eq 'SEARCH_AREA_EXHAUSTED' }).Count
    frame_samples = $frames.Count
    average_fps = ($frames | Measure-Object fps -Average).Average
    max_engine_frame_ms = ($frames | Measure-Object max_ms -Maximum).Maximum
    search_cpu_ms = ($rows | Measure-Object search_cpu_ms -Sum).Sum
    scheduler_cpu_ms = $(if ($scheduler.Success) { [int]$scheduler.Groups[1].Value } else { $null })
    scheduler_max_update_ms = $(if ($scheduler.Success) { [int]$scheduler.Groups[2].Value } else { $null })
    scheduler_updates = $(if ($scheduler.Success) { [int]$scheduler.Groups[3].Value } else { $null })
    script_errors = [regex]::Matches($text, 'SCRIPT\s*\([EF]\)').Count
    engine_fatal = [regex]::Matches($text, 'ENGINE\s*\(F\)').Count
    note = 'queries — списанные единицы общей квоты, а не число площадок; окон ожидания нельзя считать CPU временем. Отсутствующее время означает незавершённый этап, не ноль.'
}
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'summary.json') -Encoding UTF8
$summary | ConvertTo-Json
