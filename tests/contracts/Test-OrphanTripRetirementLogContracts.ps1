param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tests/log-audits/Test-OrphanTripRetirementLog.ps1') -EvidenceRoot unused
$raw=@'
[ORPHAN_TRIP_START] classification=CONTROLLED_ALL_CREW_DEATH no_heal=1 no_teleport=1 destroy_vehicle=0
[ORPHAN_TRIP_KILL] operation_id=trip-USSR-0-1-1 vehicle=0x4000000000000100 {} rpl_id=CAR group=0x4000000000000200 {} group_generation=1 trip_generation=1 lease_generation=2 vehicle_generation=3 actual=5
'@
$captures='';$dead='';$kills=''
for($i=0;$i -lt 5;$i++){
 $id=1000+$i
 $captures+="[TRIP_RETIREMENT_MEMBER_CAPTURED] operation_id=trip-USSR-0-1-1 member=$id actor_rpl=R$i`n"
 $dead+="[TRIP_RETIREMENT_DEAD_MEMBER] operation_id=trip-USSR-0-1-1 member=$id actor_rpl=R$i native_readback=1 life_state=DEAD`n"
 $kills+="[ORPHAN_TRIP_MEMBER_KILL] member=$id original_group=0x4000000000000200 {} action=NATIVE_KILL`n"
}
$raw=$raw.Replace('[ORPHAN_TRIP_KILL]',$captures+'[ORPHAN_TRIP_KILL]')+"`n"+$kills+$dead
$raw+="[GROUP_ROSTER_READY] faction=USSR numeric_slot=0 generation=2 initial_agents=5 expected_agents=5`n"
$raw+="[DEPLOYMENT_COMMITTED] faction=USSR slot=0 generation=2 roster=5/5 ticket_debit=1 supply_debit=100`n"
$raw+="[CAPTURED_DEATH_RETIREMENT_CONFIRMED] operation_id=trip-USSR-0-1-1 vehicle=0x4000000000000100 {} rpl_id=CAR group_generation=1 trip_generation=1 lease_generation=2 vehicle_generation=3 classification=CONFIRMED captured_count=5 all_native_dead=1 own_cap_at_release=0 release_ack_receipt=1 pool_offer_receipt=1 quarantine_receipt=0 native_absence_receipt=0 release_at_ms=1000 release_ack_at_ms=2000 cleanup_terminal_at_ms=10000`n"
$raw+="[ORPHAN_TRIP_DONE] passed=1 reason=TERMINAL_RELEASE_AND_NEXT_GENERATION_READY before=0 elapsed_since_kill_ms=33000`n"
$positive=@(Test-AICFOrphanRetirement $raw $false $false)
if($positive.Count){throw "Controlled orphan positive rejected: $positive"}
foreach($mutation in @(
 @('false native death','life_state=DEAD','life_state=INCAPACITATED','EXACT_NATIVE_DEAD_COMPONENT_IDENTITY'),
 @('wrong component identity','member=1003 actor_rpl=R3 native','member=1003 actor_rpl=OTHER native','EXACT_NATIVE_DEAD_COMPONENT_IDENTITY'),
 @('no admission capture','[TRIP_RETIREMENT_MEMBER_CAPTURED]','[UNKNOWN]','EXACT_ADMISSION_AND_NATIVE_DEATH_ROSTER'),
 @('wrong release car','vehicle=0x4000000000000100 {} rpl_id=CAR group_generation','vehicle=0x4000000000000111 {} rpl_id=CAR group_generation','EXACT_TYPED_NATIVE_RELEASE'),
 @('wrong release epoch','lease_generation=2 vehicle_generation=3 classification','lease_generation=9 vehicle_generation=3 classification','EXACT_RELEASE_GENERATIONS'),
 @('pending is not confirmed','classification=CONFIRMED','classification=PENDING','EXACT_TYPED_NATIVE_RELEASE'),
 @('own cap retained','own_cap_at_release=0','own_cap_at_release=1','EXACT_TYPED_NATIVE_RELEASE'),
 @('missing acknowledgement','release_ack_receipt=1','release_ack_receipt=0','EXACT_TYPED_NATIVE_RELEASE'),
 @('physical asset unreleased','pool_offer_receipt=1','pool_offer_receipt=0','ACTUAL_PHYSICAL_DISPOSITION'),
 @('release late','release_at_ms=1000','release_at_ms=10001','ORIGINAL_CLEANUP_DEADLINE'),
 @('unready replacement','initial_agents=5','initial_agents=4','EXACT_NEXT_GENERATION_READINESS'),
 @('generation skips','generation=2 initial_agents','generation=3 initial_agents','EXACT_NEXT_GENERATION_READINESS'),
 @('missing debit','ticket_debit=1','ticket_debit=0','EXACT_NEXT_GENERATION_DEBIT')
)){
 if(!(@(Test-AICFOrphanRetirement ($raw.Replace($mutation[1],$mutation[2])) $false $false) -contains $mutation[3])){throw "Controlled orphan negative escaped: $($mutation[0])"}
}
foreach($marker in @('[ACCEPTANCE_FAILURE_LATCHED] reason=EARLIER_UNRELATED_FAILURE','[GROUP_SPAWN_TIMEOUT]','[VEHICLE_CLEANUP_RETAINED]')){
 $changed=$raw.Replace('[ORPHAN_TRIP_DONE]',"$marker`n[ORPHAN_TRIP_DONE]")
 if(!(@(Test-AICFOrphanRetirement $changed $false $false) -contains 'NO_HIDDEN_GAMEPLAY_FAILURE')){throw 'Earlier unrelated failure erased by confirmed retirement'}
}
$badEject=$raw.Replace('[ORPHAN_TRIP_DONE]',"[VEHICLE_CLEANUP_ORPHAN_DEAD_MEMBER] operation_id=trip-USSR-0-1-1 vehicle=256 rpl_id=CAR member=1000 actor_rpl=R0 life_state=INCAPACITATED life_after=DEAD player_fence=PASSED native_helper=KICK_FROM_VEHICLE health_mutated=0 position_mutated=0 corpse_deleted=0 attempt=1`n[ORPHAN_TRIP_DONE]")
if(!(@(Test-AICFOrphanRetirement $badEject $false $false) -contains 'EXACT_DEAD_ONLY_NATIVE_EJECTION')){throw 'Non-DEAD ejection accepted'}
$stop="[VEHICLE_STATE_CHANGED] faction=USSR numeric_slot=0 group_generation=2 trip_generation=2 operation_id=trip-USSR-0-2-2 causation_id=STOP2 reason=COORDINATOR_STOP state=FAILED_CLOSED committed=1`n[ACCEPTANCE_FAILURE_LATCHED] reason=COORDINATOR_STOP domain=IDENTITY faction=USSR slot=0 group_gen=2 trip_gen=2 operation_id=trip-USSR-0-2-2 causation_id=STOP2`n"
if(@(Test-AICFOrphanRetirement ($raw+$stop) $false $false).Count){throw 'Precisely bound next generation post-DONE Stop should be separately reported'}
foreach($change in @(
 @($raw.Replace('[ORPHAN_TRIP_DONE]',$stop+'[ORPHAN_TRIP_DONE]'),'NO_HIDDEN_GAMEPLAY_FAILURE'),
 @(($raw+$stop.Replace('trip-USSR-0-2-2','trip-USSR-0-1-1')),'ONLY_EXACT_POST_DONE_NEXT_GENERATION_STOP'),
 @(($raw+$stop.Replace('committed=1','committed=0')),'ONLY_EXACT_POST_DONE_NEXT_GENERATION_STOP'),
 @(($raw+$stop.Replace('group_gen=2','group_gen=1')),'ONLY_EXACT_POST_DONE_NEXT_GENERATION_STOP')
)){
 if(!(@(Test-AICFOrphanRetirement $change[0] $false $false) -contains $change[1])){throw 'Unproven/early/original-operation Stop escaped'}
}
Write-Output 'PASS Controlled orphan log: positive=2 negatives=21; earlier errors retained; exact post-DONE next-generation Stop separate; node/component Rpl not conflated'
