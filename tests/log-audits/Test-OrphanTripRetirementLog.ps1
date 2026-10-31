[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot,[switch]$Before,[switch]$DestroyVehicle)
$ErrorActionPreference='Stop'
$orphanRequestedEvidence=$EvidenceRoot
. (Join-Path $PSScriptRoot 'Test-VehicleSoakObservationLog.ps1') -LogPath unused -EvidenceRoot unused -Faction USSR
$EvidenceRoot=$orphanRequestedEvidence
function SameOrphanEntity($a,$b){return (NativeEntityMatches $a $b) -or (NativeEntityMatches $b $a)}
function Test-AICFOrphanRetirement([string]$Raw,[bool]$ExpectedBefore,[bool]$Destructive){
 $issues=[Collections.Generic.List[string]]::new()
 function Need([bool]$ok,[string]$why){if(!$ok -and !$issues.Contains($why)){$issues.Add($why)}}
 $events=@([regex]::Matches($Raw,'\[(ORPHAN_TRIP_[A-Z_]+|TRIP_RETIREMENT_MEMBER_CAPTURED|TRIP_RETIREMENT_DEAD_MEMBER|CAPTURED_DEATH_RETIREMENT_CONFIRMED|VEHICLE_CLEANUP_ORPHAN_DEAD_MEMBER|GROUP_ROSTER_READY|DEPLOYMENT_COMMITTED)\]([^\r\n]*)')|ForEach-Object{[pscustomobject]@{name=$_.Groups[1].Value;index=$_.Index;fields=(Fields $_.Groups[2].Value)}})
 function One($name){$items=@($events|Where-Object name -eq $name);Need ($items.Count -eq 1) ('ONCE_'+$name);if($items.Count){return $items[0]};return [pscustomobject]@{index=-1;fields=@{}}}
 $start=One ORPHAN_TRIP_START;$kill=One ORPHAN_TRIP_KILL;$done=One ORPHAN_TRIP_DONE
 Need ($start.fields.classification -eq 'CONTROLLED_ALL_CREW_DEATH' -and $start.fields.no_heal -eq '1' -and $start.fields.no_teleport -eq '1' -and $start.fields.destroy_vehicle -eq [string][int]$Destructive) 'CONTROLLED_CASE_POLICY'
 Need ($kill.fields.operation_id -match '^trip-USSR-0-\d+-\d+$' -and $kill.fields.actual -eq '5' -and $kill.index -gt $start.index -and $done.index -gt $kill.index -and $done.fields.passed -eq '1' -and $done.fields.before -eq [string][int]$ExpectedBefore) 'EXACT_KILL_AND_FINISH'
 $killed=@($events|Where-Object name -eq ORPHAN_TRIP_MEMBER_KILL)
 Need ($killed.Count -eq 5 -and @($killed.fields.member|Sort-Object -Unique).Count -eq 5 -and @($killed|Where-Object{$_.fields.action -ne 'NATIVE_KILL' -or !(SameOrphanEntity $_.fields.original_group $kill.fields.group)}).Count -eq 0) 'FIVE_ORIGINAL_NATIVE_KILLS'
 $readbacks=@($events|Where-Object name -eq ORPHAN_TRIP_READBACK)
 if($ExpectedBefore){
  Need ($done.fields.reason -eq 'EXPECTED_BASELINE_LEAK' -and (Number $done.fields.elapsed_since_kill_ms) -ge 90000 -and @($readbacks|Where-Object{(Number $_.fields.elapsed_ms) -ge 90000 -and $_.fields.group_absent -eq '1' -and $_.fields.trip_terminal -eq '0' -and $_.fields.cap_held -eq '1' -and $_.fields.next_alive -eq '5' -and (Number $_.fields.next_generation) -eq (Number $kill.fields.group_generation)+1}).Count -ge 1) 'BASELINE_LEAK_REPRODUCED'
 }else{
  Need ($done.fields.reason -eq 'TERMINAL_RELEASE_AND_NEXT_GENERATION_READY' -and (Number $done.fields.elapsed_since_kill_ms) -le 180000) 'BOUNDED_NATIVE_FINISH'
  $confirmed=One CAPTURED_DEATH_RETIREMENT_CONFIRMED
  Need ($confirmed.fields.operation_id -eq $kill.fields.operation_id -and (SameOrphanEntity $confirmed.fields.vehicle $kill.fields.vehicle) -and $confirmed.fields.rpl_id -eq $kill.fields.rpl_id -and $confirmed.fields.classification -eq 'CONFIRMED' -and $confirmed.fields.captured_count -eq '5' -and $confirmed.fields.all_native_dead -eq '1' -and $confirmed.fields.own_cap_at_release -eq '0' -and $confirmed.fields.release_ack_receipt -eq '1' -and $confirmed.index -gt $kill.index -and $confirmed.index -lt $done.index) 'EXACT_TYPED_NATIVE_RELEASE'
  foreach($key in @('group_generation','trip_generation','lease_generation','vehicle_generation')){Need ($confirmed.fields[$key] -eq $kill.fields[$key]) 'EXACT_RELEASE_GENERATIONS'}
  Need (($confirmed.fields.pool_offer_receipt -eq '1' -and !$Destructive) -or ($confirmed.fields.quarantine_receipt -eq '1' -and $confirmed.fields.native_absence_receipt -eq '1')) 'ACTUAL_PHYSICAL_DISPOSITION'
  foreach($key in @('release_at_ms','release_ack_at_ms')){Need ((Number $confirmed.fields[$key]) -gt 0 -and (Number $confirmed.fields[$key]) -le (Number $confirmed.fields.cleanup_terminal_at_ms)) 'ORIGINAL_CLEANUP_DEADLINE'}
  if($Destructive){Need ((Number $confirmed.fields.absence_at_ms) -gt 0 -and (Number $confirmed.fields.absence_at_ms) -le (Number $confirmed.fields.cleanup_terminal_at_ms)) 'ORIGINAL_CLEANUP_DEADLINE'}
  $captures=@($events|Where-Object{$_.name -eq 'TRIP_RETIREMENT_MEMBER_CAPTURED' -and $_.fields.operation_id -eq $kill.fields.operation_id})
  $dead=@($events|Where-Object{$_.name -eq 'TRIP_RETIREMENT_DEAD_MEMBER' -and $_.fields.operation_id -eq $kill.fields.operation_id})
  Need ($captures.Count -eq 5 -and $dead.Count -eq 5) 'EXACT_ADMISSION_AND_NATIVE_DEATH_ROSTER'
  foreach($member in $killed){
   $capture=@($captures|Where-Object{SameOrphanEntity $_.fields.member $member.fields.member})
   $death=@($dead|Where-Object{(SameOrphanEntity $_.fields.member $member.fields.member) -and $_.fields.life_state -eq 'DEAD' -and $_.fields.native_readback -eq '1' -and $_.index -gt $kill.index -and $_.index -lt $confirmed.index})
   Need ($capture.Count -eq 1 -and $death.Count -eq 1 -and $capture[0].fields.actor_rpl -eq $death[0].fields.actor_rpl -and $capture[0].index -lt $kill.index) 'EXACT_NATIVE_DEAD_COMPONENT_IDENTITY'
  }
  foreach($eject in @($events|Where-Object name -eq VEHICLE_CLEANUP_ORPHAN_DEAD_MEMBER)){
   Need ($eject.fields.operation_id -eq $kill.fields.operation_id -and (SameOrphanEntity $eject.fields.vehicle $kill.fields.vehicle) -and $eject.fields.rpl_id -eq $kill.fields.rpl_id -and $eject.fields.life_state -eq 'DEAD' -and $eject.fields.life_after -eq 'DEAD' -and $eject.fields.player_fence -eq 'PASSED' -and $eject.fields.native_helper -eq 'KICK_FROM_VEHICLE' -and $eject.fields.health_mutated -eq '0' -and $eject.fields.position_mutated -eq '0' -and $eject.fields.corpse_deleted -eq '0' -and (Number $eject.fields.attempt) -le 3 -and @($dead|Where-Object{(SameOrphanEntity $_.fields.member $eject.fields.member) -and $_.fields.actor_rpl -eq $eject.fields.actor_rpl}).Count -eq 1) 'EXACT_DEAD_ONLY_NATIVE_EJECTION'
  }
  $next=[string]((Number $kill.fields.group_generation)+1)
  Need (@($events|Where-Object{$_.name -eq 'GROUP_ROSTER_READY' -and $_.fields.faction -eq 'USSR' -and $_.fields.numeric_slot -eq '0' -and $_.fields.generation -eq $next -and $_.fields.initial_agents -eq '5' -and $_.fields.expected_agents -eq '5' -and $_.index -gt $kill.index -and $_.index -lt $done.index}).Count -eq 1) 'EXACT_NEXT_GENERATION_READINESS'
  Need (@($events|Where-Object{$_.name -eq 'DEPLOYMENT_COMMITTED' -and $_.fields.faction -eq 'USSR' -and $_.fields.slot -eq '0' -and $_.fields.generation -eq $next -and $_.fields.ticket_debit -eq '1' -and $_.fields.roster -eq '5/5' -and (Number $_.fields.supply_debit) -gt 0}).Count -eq 1) 'EXACT_NEXT_GENERATION_DEBIT'
  $observed=$Raw.Substring(0,[Math]::Max(0,$done.index))
  Need ($observed -notmatch '\[ACCEPTANCE_FAILURE_LATCHED\]|\[GROUP_SPAWN_TIMEOUT\]|\[MEANINGFUL_TASK_DEADLINE_MISSED\]|\[VEHICLE_CLEANUP_RETAINED\]') 'NO_HIDDEN_GAMEPLAY_FAILURE'
  $stops=@([regex]::Matches($Raw,'\[(VEHICLE_STATE_CHANGED|ACCEPTANCE_FAILURE_LATCHED)\]([^\r\n]*)')|ForEach-Object{[pscustomobject]@{name=$_.Groups[1].Value;index=$_.Index;fields=(Fields $_.Groups[2].Value)}})
  foreach($latch in @($stops|Where-Object name -eq ACCEPTANCE_FAILURE_LATCHED)){
   $matched=@($stops|Where-Object{$_.name -eq 'VEHICLE_STATE_CHANGED' -and $_.index -gt $done.index -and $_.index -lt $latch.index -and $_.fields.reason -eq 'COORDINATOR_STOP' -and $_.fields.state -eq 'FAILED_CLOSED' -and $_.fields.committed -eq '1' -and $_.fields.operation_id -eq $latch.fields.operation_id -and $_.fields.causation_id -eq $latch.fields.causation_id -and $_.fields.faction -eq 'USSR' -and $_.fields.numeric_slot -eq '0' -and $_.fields.group_generation -eq $next -and $_.fields.trip_generation -eq $latch.fields.trip_gen})
   Need ($latch.index -gt $done.index -and $latch.fields.reason -eq 'COORDINATOR_STOP' -and $latch.fields.domain -eq 'IDENTITY' -and $latch.fields.faction -eq 'USSR' -and $latch.fields.slot -eq '0' -and $latch.fields.group_gen -eq $next -and $latch.fields.operation_id -ne $kill.fields.operation_id -and $matched.Count -eq 1) 'ONLY_EXACT_POST_DONE_NEXT_GENERATION_STOP'
  }
 }
 if($Destructive){
  $request=One ORPHAN_TRIP_CAR_KILL_REQUEST
  Need ($request.fields.operation_id -eq $kill.fields.operation_id -and (SameOrphanEntity $request.fields.vehicle $kill.fields.vehicle) -and $request.fields.rpl_id -eq $kill.fields.rpl_id -and $request.fields.original_native_dead -eq '5' -and $request.index -gt $kill.index) 'DESTRUCTIVE_NATIVE_REQUEST_IDENTITY'
  $health=@([regex]::Matches($Raw,'health_status=READ_ONLY[^\r\n]*')|Where-Object{ $_.Index -gt $request.index -and $_.Index -lt $done.index -and $_.Value -match ('health_vehicle='+[regex]::Escape([string]$kill.fields.vehicle)) -and $_.Value -match ('health_component_rpl='+[regex]::Escape([string]$kill.fields.rpl_id)+'(?:\s|$)') -and $_.Value -match 'health_can_move=0' -and $_.Value -match 'health_movement_damage=1' -and $_.Value -match 'health_default_scaled=0'})
  Need ($health.Count -gt 0) 'ACTUAL_DESTROYED_NATIVE_CAR_READBACK'
 }
 return $issues.ToArray()
}
if($MyInvocation.InvocationName -eq '.'){return}
$root=(Resolve-Path -LiteralPath $EvidenceRoot).Path
$console=@(Get-ChildItem -LiteralPath (Join-Path $root 'profile') -Recurse -File -Filter console.log)
if($console.Count -ne 1){throw 'Exactly one native console log required'}
$raw=[IO.File]::ReadAllText($console[0].FullName)
$functional=@(Test-AICFOrphanRetirement $raw ([bool]$Before) ([bool]$DestroyVehicle))
$fatal=@();$resources=@()
foreach($file in @(Get-ChildItem -LiteralPath $console[0].DirectoryName -File -Filter '*.log')){
 $n=0;foreach($line in [IO.File]::ReadLines($file.FullName)){
  $n++;$item=[pscustomobject]@{file=$file.FullName;line=$n;text=$line}
  if($line -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Access violation|Attempt to modify locked entity|memory leaks?'){ $fatal+=$item }
  elseif($line -match '\([EF]\)\s*:'){$resources+=$item}
 }
}
$summary=Get-Content -LiteralPath (Join-Path $root 'summary.json') -Raw|ConvertFrom-Json
$source=Get-Content -LiteralPath (Join-Path $root 'source-manifest.json') -Raw|ConvertFrom-Json
$drift=@($source.files|Where-Object{!(Test-Path -LiteralPath $_.path) -or (Get-FileHash -LiteralPath $_.path).Hash -ne $_.sha256})
$stopped=$summary.native_exit -eq 0 -and $raw -match 'ENGINE\s*: Game destroyed\.'
$donePosition=$raw.LastIndexOf('[ORPHAN_TRIP_DONE]')
$shutdownFailures=@();if($donePosition -ge 0){$shutdownFailures=@([regex]::Matches($raw.Substring($donePosition),'\[ACCEPTANCE_FAILURE_LATCHED\][^\r\n]*')|ForEach-Object Value)}
$result=[ordered]@{classification='CONTROLLED_ALL_CREW_DEATH';before=[bool]$Before;destructive=[bool]$DestroyVehicle;functional=($functional.Count -eq 0);functional_issues=$functional;source_freeze=($drift.Count -eq 0 -and $summary.source_drift -eq 0);stopped_native=$stopped;fatal_count=$fatal.Count;resource_count=$resources.Count;shutdown_acceptance_failures=$shutdownFailures;fatal_errors=$fatal;resource_errors=$resources}
$result|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $root 'orphan-audit.json')
$result|ConvertTo-Json -Depth 3
if($functional.Count -or $drift.Count -or !$stopped -or $fatal.Count -or $resources.Count){exit 1}
