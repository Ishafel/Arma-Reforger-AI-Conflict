param([Parameter(Mandatory)][string]$LogPath)
$ErrorActionPreference = 'Stop'
$log = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath))
if ($log -notmatch 'Game destroyed\.' -or $log -notmatch '\[ROSTER_READY\]') { throw 'Full stopped roster-ready server log required' }
if ($log -match 'SCRIPT\s+\((E|F)\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception') { throw 'Script/VM error' }
if ($log -match '\[RECRUITMENT_SEQUENCE_PROBE\] case=\S+ pass=0') { throw 'Fixture failure' }
$cases = @('CLOCK_NINE_45000','CLOCK_NO_CATCHUP','FORECAST_NINE_45','FORECAST_QUEUE_50','FORECAST_CAPACITY','FORECAST_DEADLINE',
    'ADMIT_DEFENSE_FIRST','ADMIT_ATTACK_NINE','ADMIT_SECOND_BASE','ADMIT_USSR','LIVE_CROSS_BASE_FORECAST',
    'FACTIONS_INDEPENDENT','NINE_COMPLETE_NO_INTERLEAVING','FIFO_BEFORE_DEFENSE','COMPLETE_RELEASE','PARTIAL_ADMISSION',
    'LATE_ATTACK_ADMIT','LATE_ATTACK_HELD','MANUAL_PROMOTION','MANUAL_PRIORITY','PARTIAL_ONLY_VACANCIES',
    'DEFICIT_OWNER_ADMIT','DEFICIT_WAITER_ADMIT','DEFICIT_NO_BYPASS','TIMEOUT_RELEASE','CANCEL_OWNER_ADMIT',
    'CANCEL_WAITER_ADMIT','CANCEL_RELEASE','CANCEL_NEXT_PROGRESS','GENERATION_OWNER_ADMIT','GENERATION_WAITER_ADMIT',
    'STALE_CALLBACK_NO_MUTATION','GENERATION_NEXT_PROGRESS')
foreach ($case in $cases) {
    if ([regex]::Matches($log,"\[RECRUITMENT_SEQUENCE_PROBE\] case=$case pass=1\b").Count -ne 1) { throw "Missing/duplicate case $case" }
}
if ($log -notmatch "\[RECRUITMENT_SEQUENCE_PROBE\] finished=1 passed=$($cases.Count) failed=0 phase=15") { throw 'Incomplete sequence matrix' }
$visits = @{}; $owners = @{}; $autoJoins = @(); $intervals = @(); $spawnLatencies = @(); $phase = 0
foreach ($line in ($log -split '\r?\n')) {
    if ($line -match '\[RECRUITMENT_SEQUENCE_PHASE\] phase=(\d+)') { $phase = [int]$Matches[1] }
    if ($line -notmatch '\[(INFANTRY_RECRUIT(?:MENT)?_[A-Z_]+)\]') { continue }
    $event = $Matches[1]; $f = @{}
    foreach ($field in [regex]::Matches($line,'(\w+)=([^\s]+)')) { $f[$field.Groups[1].Value] = $field.Groups[2].Value }
    if (!$f.token) { continue }
    $key = "$($f.faction):$($f.token)"
    if ($f.now_ms -notmatch '^\d+$') { throw "Missing timestamp: $event" }
    $now = [int]$f.now_ms
    if ($event -eq 'INFANTRY_RECRUITMENT_STARTED') {
        $visits[$key] = @{ Due=0; LastJoin=0; Spawn=0; Debit=0; Joins=0; Alive=[int]$f.alive; Desired=[int]$f.desired; Finished=$false; Slot=$f.slot; Phase=$phase }
        continue
    }
    if (!$visits.ContainsKey($key)) { throw "Event without visit $key" }
    $v = $visits[$key]
    if ($v.Finished) { throw "Stale event $key" }
    switch ($event) {
        'INFANTRY_RECRUITMENT_SERVICE_GRANTED' {
            if ($owners.ContainsKey($f.faction) -and $owners[$f.faction] -ne $key) {
                # Истекшая/неактуальная заявка освобождается перед её Finish в том же tick.
                $old = $owners[$f.faction]
                if ($visits[$old].Alive + $visits[$old].Joins -lt $visits[$old].Desired -and $log -notmatch ("\[INFANTRY_RECRUITMENT_FINISHED\][^\r\n]*faction="+$f.faction+"[^\r\n]*token="+($old -split ':')[1]+"\b[^\r\n]*reason=(SUPPLY_WAIT_TIMEOUT|IDENTITY_CHANGED)")) { throw "Concurrent auto owners: $old $key" }
            }
            $owners[$f.faction] = $key
        }
        'INFANTRY_RECRUITMENT_STEP_WAIT' { $v.Due=[int]$f.due_ms; if ($v.Due-[int]$f.admitted_ms -ne 5000) { throw 'Wrong first delay' } }
        'INFANTRY_RECRUIT_SPAWN_REQUESTED' {
            if ($f.source -eq 'AUTO' -and $owners[$f.faction] -ne $key) { throw "Non-owner spawn $key" }
            if ($v.Due -eq 0 -or $now -lt $v.Due -or $v.Spawn) { throw "Early/duplicate spawn $key" }
            if ($v.LastJoin) { $intervals += $now-$v.LastJoin; if ($now-$v.LastJoin -lt 5000) { throw 'Short purchase interval' } }
            $v.Spawn=$now
        }
        'INFANTRY_RECRUIT_DEBITED' {
            if (!$v.Spawn -or $v.Debit) { throw 'Debit without unique spawn' }
            if ($f.source -eq 'AUTO' -and $owners[$f.faction] -ne $key) { throw 'Non-owner debit' }
            if ([Math]::Abs(([double]$f.supplies_before-[double]$f.supplies_after)-[double]$f.cost) -gt 0.01) { throw 'Incorrect debit' }
            $v.Debit=[int]$f.cost
        }
        'INFANTRY_RECRUIT_JOINED' {
            if (!$v.Spawn -or $v.Debit -ne [int]$f.cost) { throw 'Unpaid or duplicate transfer' }
            if ($f.role -eq 'SQUAD_LEADER') { throw 'Existing commander purchased again' }
            $spawnLatencies += $now-$v.Spawn
            $v.Spawn=0; $v.Debit=0; $v.Joins++; $v.LastJoin=$now; $v.Due=$now+5000
            if ([int]$f.alive -ne $v.Alive+$v.Joins) { throw 'Unexpected roster membership' }
            if ($f.faction -eq 'US' -and $phase -le 4) { $autoJoins += [int]$f.slot }
        }
        'INFANTRY_RECRUIT_SPAWN_CANCELLED' { if ($v.Debit -ne 0) { throw 'Cancelled paid donor' }; $v.Spawn=0 }
        'INFANTRY_RECRUITMENT_FINISHED' {
            if ($v.Debit) { throw 'Unsettled debit' }
            if ($f.reason -eq 'ROSTER_COMPLETE_OR_EMPTY' -and $v.Joins -ne $v.Desired-$v.Alive) { throw 'Incorrect vacancy count' }
            $v.Finished=$true
            if ($owners[$f.faction] -eq $key) { $owners.Remove($f.faction) }
        }
    }
}
$expected = @(@(1)*9) + @(2,2,0)
if (($autoJoins -join ',') -ne ($expected -join ',')) { throw "Wrong initial automatic order: $($autoJoins -join ',')" }
foreach ($v in $visits.Values) { if (!$v.Finished) { throw 'Unfinished visit' } }
$joinTotal=($visits.Values | ForEach-Object { $_.Joins } | Measure-Object -Sum).Sum
if ($joinTotal -ne 21) { throw "Expected 21 paid transfers, got $joinTotal" }
$nine = @($visits.Values | Where-Object { $_.Slot -eq '1' -and $_.Desired -eq 10 })
if ($nine.Count -ne 1 -or $nine[0].Joins -ne 9) { throw 'Expected nine paid recruits for 1/10' }
Write-Output "PASS sequence cases=$($cases.Count) visits=$($visits.Count) first_roster_joins=9"
Write-Output "Observed join-to-next-spawn ms: $($intervals -join ','); spawn-to-join ms: $($spawnLatencies -join ',')"
