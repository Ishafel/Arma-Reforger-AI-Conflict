param([Parameter(Mandatory)][string]$LogPath)
$ErrorActionPreference = 'Stop'
$log = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath))
if ($log -notmatch 'Game destroyed\.') { throw 'Full stopped server log required.' }
if ($log -match 'SCRIPT\s+\((E|F)\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception') { throw 'Script/VM error in full log.' }
if ($log -match '\[RECRUITMENT_PRIORITY_PROBE\] case=\S+ pass=0') { throw 'Fixture failure.' }
$cases = @('THREE_REQUESTS','AUTO_ORDER','PENDING_DEMAND','ATTACK_FORECAST','ROLE_CHANGE','ROLE_REALLOCATION',
    'UNRELATED_MANUAL','STALE_GENERATION','STALE_ASSIGNMENT','STALE_INTENT','STALE_GRAPH','REPLACED_GROUP',
    'RELEASE_BEFORE_CLEANUP','EXPIRED_DEMAND','NO_STOCK_DEBIT','PHYSICAL_GATE','BASE_IDENTITY_GATE','ECONOMY_GRAPH_GATE',
    'SCARCE_ATTACK_FIRST','MANUAL_PROMOTION','PENDING_MANUAL_PROTECTED','MANUAL_CANCEL','MANUAL_REQUEUE','MANUAL_DEFENSE_FIRST','FIFO_EVENTUAL',
    'COMPLETE_RELEASE','NEW_ATTACK_PREEMPTS','PAID_MEMBERS_PRESERVED','RICH_BOTH_ROLES','STABLE_GROUPS')
foreach ($case in $cases) {
    if ([regex]::Matches($log,"\[RECRUITMENT_PRIORITY_PROBE\] case=$case pass=1\b").Count -ne 1) { throw "Missing/duplicate case: $case" }
}
if ([regex]::Matches($log,'\[RECRUITMENT_PRIORITY_PROBE\] finished=1 phase=8').Count -ne 1) { throw 'Incomplete priority matrix.' }
$joins = [regex]::Matches($log,'\[INFANTRY_RECRUIT_JOINED\][^\r\n]+')
$debits = [regex]::Matches($log,'\[INFANTRY_RECRUIT_DEBITED\][^\r\n]+')
if ($joins.Count -ne 6 -or $debits.Count -ne 6) { throw "Expected six paid transfers, joins=$($joins.Count), debits=$($debits.Count)" }
$expected = @(1,0,2,1)
for ($i=0; $i -lt $expected.Count; $i++) {
    if ($joins[$i].Value -notmatch ("\bslot="+$expected[$i]+"\b")) { throw "Wrong join order at $i" }
}
foreach ($debit in $debits) {
    if ($debit.Value -notmatch 'cost=(\d+) supplies_before=([\d.]+) supplies_after=([\d.]+) ticket_cost=0') { throw 'Missing debit amounts.' }
    if ([Math]::Abs(([double]$Matches[2]-[double]$Matches[3])-[double]$Matches[1]) -gt 0.01) { throw 'Incorrect debit.' }
    if ($debit.Value -notmatch 'source=(AUTO|PLAYER_RECRUIT) priority=[012] queue=\d+') { throw 'Missing priority diagnostics.' }
}
if ($log -notmatch '\[INFANTRY_RECRUITMENT_PRIORITY_WAIT\][^\r\n]+reason=PRIORITY_DEMAND') { throw 'No live priority wait.' }
if ([regex]::Matches($log,'\[INFANTRY_RECRUIT_SPAWN_CANCELLED\][^\r\n]+reason=PRIORITY_DEMAND paid=0').Count -ne 1) { throw 'Missing unique unpaid donor preemption.' }
Write-Output "PASS priority cases=$($cases.Count) debits=6 transfers=6 stopped_log=$LogPath"
