[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
# Один снимок полного лога: при live-замере все таблицы относятся к одному моменту.
$text = Get-Content -LiteralPath $LogPath -Raw
$snapshot = Join-Path $OutputDirectory 'full-log-snapshot.txt'
$text | Set-Content -LiteralPath $snapshot -Encoding UTF8
& "$PSScriptRoot/Measure-ConstructionSearch.ps1" -LogPath $snapshot -OutputDirectory (Join-Path $OutputDirectory 'search') | Out-Null
$ordersText = Get-Content (Join-Path $OutputDirectory 'search/orders.json') -Raw
$orders = @()
if (-not [string]::IsNullOrWhiteSpace($ordersText)) {
    $orders = @($ordersText | ConvertFrom-Json | Where-Object { $null -ne $_.decision_ms })
}
$bases = @{}
$coverage = @{}
$idle = @{}
$prepared = $null
$done = $null
$lastTime = 0
function Read-Fields([string]$Line) {
    $fields = @{}
    foreach ($match in [regex]::Matches($Line, '(?:^|\s)([a-zA-Z_]+)=("[^"]*"|\S+)')) {
        $fields[$match.Groups[1].Value] = $match.Groups[2].Value.Trim('"')
    }
    return $fields
}
function Seconds($End, $Start) {
    if ($null -eq $End -or $null -eq $Start) { return $null }
    return [math]::Round(([double]$End - [double]$Start) / 1000, 3)
}
foreach ($line in ($text -split '\r?\n')) {
    if ($line -match '\[AICF\].*?t_ms=(\d+)') { $lastTime = [math]::Max($lastTime, [long]$Matches[1]) }
    if ($line -notmatch '\[(CONSTRUCTION_ALL_BASE[A-Z_]*)\]') { continue }
    $event = $Matches[1]
    $fields = Read-Fields $line
    switch ($event) {
        'CONSTRUCTION_ALL_BASE_BASELINE' {
            if ($bases.ContainsKey($fields.base)) { throw "Duplicate base baseline: $($fields.base)" }
            $bases[$fields.base] = $fields
        }
        'CONSTRUCTION_ALL_BASES_PREPARED' {
            if ($null -ne $prepared) { throw 'Only one run per log is supported' }
            $prepared = $fields
        }
        'CONSTRUCTION_ALL_BASES_DONE' { $done = $fields }
        'CONSTRUCTION_ALL_BASE_IDLE' { $idle[$fields.base] = $fields }
        'CONSTRUCTION_ALL_BASE_COVERAGE' {
            $key = "$($fields.base)/$($fields.checked_type)"
            if (-not $coverage.ContainsKey($key)) { $coverage[$key] = $fields }
        }
    }
}
if ($null -eq $prepared) { throw 'CONSTRUCTION_ALL_BASES_PREPARED missing' }
if ($bases.Count -ne [int]$prepared.bases) { throw 'Base inventory differs from prepared count' }
foreach ($order in $orders) {
    if (-not $bases.ContainsKey($order.base)) { throw "Order outside base inventory: $($order.token)" }
}
$rows = @(foreach ($base in $bases.Values) {
    $baseOrders = @($orders | Where-Object base -EQ $base.base | Sort-Object decision_ms)
    $placed = @($baseOrders | Where-Object { $null -ne $_.placed_ms } | Sort-Object placed_ms)
    $completed = @($baseOrders | Where-Object { $null -ne $_.completed_ms } | Sort-Object completed_ms)
    $cancelled = @($baseOrders | Where-Object { $null -ne $_.cancelled_ms })
    $open = @($baseOrders | Where-Object { $null -eq $_.completed_ms -and ($null -eq $_.cancelled_ms -or $_.cancel_reason -eq 'STOP') })
    $status = 'NO_DECISION'
    if ($base.provider -eq 'NONE') { $status = 'NO_PROVIDER' }
    elseif ($completed.Count) { $status = 'BUILT' }
    elseif ($placed.Count) { $status = 'PLACED_NOT_COMPLETED' }
    elseif ($baseOrders.Count) { $status = 'SEARCH_NOT_COMPLETED' }
    $last = $baseOrders | Select-Object -Last 1
    $first = $completed | Select-Object -First 1
    $rejections = @{}
    foreach ($order in $baseOrders) {
        foreach ($property in $order.rejections.PSObject.Properties) {
            $rejections[$property.Name] += [int]$property.Value
        }
    }
    [pscustomobject][ordered]@{
        name = $base.name; base = $base.base; entity_name = $base.entity_name
        hq = [int]$base.hq; control_point = [int]$base.control_point
        owned_us = [int]$base.owned; initialized = [int]$base.initialized
        provider = $base.provider; radius = [double]$base.radius
        supplies_initial = [double]$base.supplies; capacity_initial = [double]$base.capacity
        status = $status
        first_decision_s = $(if ($baseOrders.Count) { Seconds $baseOrders[0].decision_ms $prepared.t_ms } else { $null })
        first_placement_s = $(if ($placed.Count) { Seconds $placed[0].placed_ms $prepared.t_ms } else { $null })
        first_completed_s = $(if ($completed.Count) { Seconds $first.completed_ms $prepared.t_ms } else { $null })
        first_completed_type = $first.type
        first_completed_search_s = $(if ($first) { Seconds $first.placed_ms $first.decision_ms } else { $null })
        first_completed_approach_s = $(if ($first) { Seconds $first.work_started_ms $first.placed_ms } else { $null })
        first_completed_work_to_service_s = $(if ($first) { Seconds $first.completed_ms $first.work_started_ms } else { $null })
        decisions = $baseOrders.Count; placed = $placed.Count; completed = $completed.Count
        completed_types = ($completed.type | Sort-Object -Unique) -join ','
        open_at_end = ($open | ForEach-Object { "$($_.type):$($_.search_status)" }) -join ','
        cancellations = ($cancelled | Group-Object cancel_reason | Sort-Object Name | ForEach-Object { "$($_.Name):$($_.Count)" }) -join ','
        rejection_counts = ($rejections.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object { "$($_.Key):$($_.Value)" }) -join ','
        pending_expired = ($baseOrders | Measure-Object pending_expired -Sum).Sum
        last_type = $last.type; last_reason = $last.cancel_reason
        idle_reason = $idle[$base.base].reason
    }
})
$rows = @($rows | Sort-Object name)
$rows | Export-Csv (Join-Path $OutputDirectory 'bases.csv') -NoTypeInformation -Encoding UTF8
$rows | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDirectory 'bases.json') -Encoding UTF8
$orderRows = @(foreach ($order in $orders | Sort-Object decision_ms) {
    $order | Add-Member -NotePropertyName base_name -NotePropertyValue $bases[$order.base].name -PassThru
})
$orderRows | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDirectory 'orders.json') -Encoding UTF8
$coverage.Values | ForEach-Object { [pscustomobject]@{base=$_.base;name=$bases[$_.base].name;type=$_.checked_type;first_observed_s=(Seconds $_.t_ms $prepared.t_ms);covered=[int]$_.covered} } |
    Sort-Object name,type | Export-Csv (Join-Path $OutputDirectory 'coverage-first-observed.csv') -NoTypeInformation -Encoding UTF8
$summary = [ordered]@{
    log = (Resolve-Path $LogPath).Path
    stopped = $text.Contains('Game destroyed.')
    duration_reached = $null -ne $done
    elapsed_s = $(if ($done) { [math]::Round([double]$done.elapsed_ms / 1000, 3) } else { Seconds $lastTime $prepared.t_ms })
    bases = $rows.Count; owned_us = @($rows | Where-Object owned_us -EQ 1).Count
    initialized = @($rows | Where-Object initialized -EQ 1).Count
    hq = @($rows | Where-Object hq -EQ 1).Count
    control_points = @($rows | Where-Object control_point -EQ 1).Count
    fully_supplied_initially = @($rows | Where-Object { $_.supplies_initial -eq $_.capacity_initial }).Count
    providers = @($rows | Where-Object provider -NE 'NONE').Count
    bases_with_decision = @($rows | Where-Object decisions -GT 0).Count
    bases_with_placement = @($rows | Where-Object placed -GT 0).Count
    bases_with_completion = @($rows | Where-Object completed -GT 0).Count
    non_hq_with_completion = @($rows | Where-Object { $_.hq -eq 0 -and $_.completed -gt 0 }).Count
    placements = @($orders | Where-Object { $null -ne $_.placed_ms }).Count
    completed = @($orders | Where-Object { $null -ne $_.completed_ms }).Count
    first_completion_min_s = ($rows | Where-Object completed -GT 0 | Measure-Object first_completed_s -Minimum).Minimum
    first_completion_max_s = ($rows | Where-Object completed -GT 0 | Measure-Object first_completed_s -Maximum).Maximum
    script_errors = [regex]::Matches($text, 'SCRIPT\s*\([EF]\)').Count
    engine_fatal = [regex]::Matches($text, 'ENGINE\s*\(F\)').Count
    note = 'Время от PREPARED. BUILT означает хотя бы одну новую завершённую постройку. Нет времени — этап не завершён. Coverage — первое наблюдение типа, не обязательно начальное состояние. STOP — обрыв незавершённой работы границей замера.'
}
$summary | ConvertTo-Json | Set-Content (Join-Path $OutputDirectory 'summary.json') -Encoding UTF8
$markdown = [System.Collections.Generic.List[string]]::new()
$markdown.Add('Время в секундах от передачи всех точек US. Завершение означает CONSTRUCTION_COMPLETED с доступной службой; пустое значение — этап не завершён.')
$markdown.Add('')
$markdown.Add('| Точка | HQ | Первая площадка, с | Первая готовая постройка, с | Тип | Всего готово | Статус |')
$markdown.Add('|---|---:|---:|---:|---|---:|---|')
foreach ($row in $rows) {
    $label = $row.name.Replace('|', '\|')
    if (@($rows | Where-Object name -EQ $row.name).Count -gt 1) { $label += " ($($row.entity_name))" }
    $markdown.Add("| $label | $($row.hq) | $($row.first_placement_s) | $($row.first_completed_s) | $($row.first_completed_type) | $($row.completed) | $($row.status) |")
}
$markdown | Set-Content (Join-Path $OutputDirectory 'bases.md') -Encoding UTF8
$summary | ConvertTo-Json
