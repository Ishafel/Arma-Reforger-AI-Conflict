param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [switch]$RequireFullRosters,
    [switch]$RequireMuster,
    [switch]$RequireSupplyPlanning,
    [switch]$RequireSupplyBounds
)
$ErrorActionPreference = 'Stop'
$lines = Get-Content -LiteralPath $LogPath
$failures = [System.Collections.Generic.List[string]]::new()
$visits = @{}
$joined = 0
$full = @{}
foreach ($line in $lines) {
    if ($line -match 'SCRIPT\s+\((E|F)\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|\[AICF\]\[[^\]]+\]\[ERROR\]') {
        $failures.Add("Engine/script/AICF error: $line")
    }
    if ($line -notmatch '\[(INFANTRY_RECRUIT(?:MENT)?_[A-Z_]+)\]') { continue }
    $event = $Matches[1]
    $fields = @{}
    foreach ($field in [regex]::Matches($line, '(\w+)=([^\s]+)')) { $fields[$field.Groups[1].Value] = $field.Groups[2].Value }
    if ($event -eq 'INFANTRY_RECRUITMENT_DEFERRED') {
        if (!$fields.faction -or $fields.slot -notmatch '^\d+$' -or $fields.retry_ms -ne '60000' -or
            $fields.reason -ne 'NO_FEASIBLE_SAFE_BARRACKS') { $failures.Add('Invalid deferred recruitment context') }
        continue
    }
    # Rearm относится к сохранённой истории службы после завершения визита;
    # у него намеренно нет visit token или group, только stable identity.
    if ($event -eq 'INFANTRY_RECRUITMENT_APPROACH_REARMED') {
        if ($fields.faction -notin @('US','USSR') -or $fields.slot -notmatch '^\d+$' -or
            $fields.generation -notmatch '^\d+$' -or !$fields.service -or
            $fields.reason -ne 'CONTEXT_CHANGED' -or $fields.movement_confirmation -ne 'NONE') {
            $failures.Add('Invalid approach rearm context')
        }
        continue
    }
    $key = "$($fields.faction):$($fields.slot):$($fields.generation):$($fields.token)"
    if ($event -eq 'INFANTRY_RECRUITMENT_STARTED') {
        if ($visits.ContainsKey($key)) { $failures.Add("Duplicate visit $key") }
        $manual = $fields.player_requested -eq '1'
        if ((!$manual -and [double]$fields.distance_m -gt 500) -or [double]$fields.distance_m -lt 0 -or [int]$fields.alive -ge [int]$fields.desired) { $failures.Add("Invalid admission $key") }
        if ($manual -and $fields.max_distance_m -ne '-1') { $failures.Add("Invalid manual distance policy $key") }
        $visits[$key] = @{ Arrived = $false; Pending = $null; Debit = 0; Finished = $false; Group = $fields.group; Joins = 0 }
        continue
    }
    if (!$visits.ContainsKey($key)) { $failures.Add("Event without visit $key $event"); continue }
    $visit = $visits[$key]
    if ($visit.Finished -or $visit.Group -ne $fields.group) { $failures.Add("Stale identity $key $event") }
    switch ($event) {
        { $_ -in @('INFANTRY_RECRUITMENT_PLAN', 'INFANTRY_RECRUITMENT_REEVALUATED') } {
            foreach ($name in @('stock','income','income_interval_s','next_income_s','own_demand','other_demand','travel_s','supply_s','wait_s','completion_s')) {
                $value = 0.0
                if (!$fields.ContainsKey($name) -or ![double]::TryParse($fields[$name], [Globalization.NumberStyles]::Float,
                    [Globalization.CultureInfo]::InvariantCulture, [ref]$value) -or $value -lt 0 -or [double]::IsNaN($value)) {
                    $failures.Add("Invalid planning field $key $name")
                }
            }
        }
        'INFANTRY_RECRUITMENT_ARRIVED' { $visit.Arrived = $true }
        'INFANTRY_RECRUIT_SPAWN_REQUESTED' {
            if (!$visit.Arrived -or $visit.Pending -or [int]$fields.paid -ne 0) { $failures.Add("Invalid spawn $key") }
            $visit.Pending = $fields
        }
        'INFANTRY_RECRUIT_DEBITED' {
            if (!$visit.Pending -or $visit.Debit -ne 0) { $failures.Add("Debit without unique pending recruit $key") }
            if ($visit.Pending -and ($visit.Pending.role -ne $fields.role -or $visit.Pending.cost -ne $fields.cost)) { $failures.Add("Changed quote $key") }
            $delta = [double]$fields.supplies_before - [double]$fields.supplies_after
            if ([Math]::Abs($delta - [int]$fields.cost) -gt 0.01 -or [int]$fields.ticket_cost -ne 0) { $failures.Add("Wrong debit $key") }
            $visit.Debit = [int]$fields.cost
        }
        'INFANTRY_RECRUIT_REFUNDED' {
            if ($visit.Debit -ne [double]$fields.amount) { $failures.Add("Wrong refund $key") }
            $visit.Debit = 0
        }
        'INFANTRY_RECRUIT_JOINED' {
            if (!$visit.Pending -or $visit.Debit -ne [int]$fields.cost -or $visit.Debit -le 0) { $failures.Add("Unpaid recruit $key") }
            if ([int]$fields.alive -gt [int]$fields.desired -or [int]$fields.alive -gt 10 -or $fields.skill -ne 'EXPERT') { $failures.Add("Roster/skill violation $key") }
            $visit.Debit = 0
            $visit.Pending = $null
            $visit.Joins++
            $joined++
            if ([int]$fields.alive -eq 10) { $full[$fields.faction] = $true }
        }
        'INFANTRY_RECRUITMENT_FINISHED' {
            if ($visit.Debit -ne 0) { $failures.Add("Unsettled payment $key") }
            $visit.Pending = $null
            $visit.Finished = $true
        }
    }
}
if ($RequireFullRosters) {
    foreach ($faction in @('US', 'USSR')) { if (!$full.ContainsKey($faction)) { $failures.Add("No full roster for $faction") } }
    if (!($lines -match '\[RECRUIT_PROBE\].*full_rosters=1 stable_groups=1')) { $failures.Add('Missing stable-group end-to-end evidence') }
    if (!($lines -match '\[RECRUIT_PROBE\].*finished=1 full_rosters=1')) { $failures.Add('Probe did not complete') }
}
if (!$visits.Count) { $failures.Add('No recruitment visits') }
if ($RequireSupplyBounds -and !($lines -match '\[RECRUITMENT_BOUNDS_CHECKS\] passed=14 total=14')) { $failures.Add('Missing capacity/approach boundary checks') }
if ($RequireSupplyPlanning) {
    if (!($lines -match '\[RECRUITMENT_SUPPLY_CHECKS\] passed=15 total=15')) { $failures.Add('Missing demand/forecast checks') }
    if (!($lines -match '\[RECRUITMENT_SUPPLY_PROBE\] finished=1 full=6 stable=1 demand_checks=1')) { $failures.Add('Competing rosters did not complete') }
    if (!($lines -match '\[INFANTRY_RECRUITMENT_PLAN\].*other_demand=[1-9]\d*')) { $failures.Add('No competing demand observed') }
}
if ($RequireMuster) {
    if (!($lines -match '\[MUSTER_PROBE\] waited_ms=\d+ held=1 us=1 ussr=1')) { $failures.Add('Missing physical hold before barracks') }
    if (!($lines -match '\[MUSTER_PROBE\] finished=1 held_until_full=1 full_rosters=1')) { $failures.Add('Muster did not hold until full rosters') }
    if ($lines -match '\[MUSTER_PROBE\].*(?:held=0|held_until_full=0|full_rosters=0)') { $failures.Add('Muster probe reported a failed gate') }
}
foreach ($key in $visits.Keys) {
    if (!$visits[$key].Finished) { $failures.Add("Unfinished visit $key") }
}
if ($failures.Count) {
    Write-Output "Infantry recruitment log: FAIL ($($failures.Count))"
    $failures | ForEach-Object { Write-Output " - $_" }
    exit 1
}
Write-Output "Infantry recruitment log: PASS visits=$($visits.Count) joined=$joined full_factions=$($full.Count)"
