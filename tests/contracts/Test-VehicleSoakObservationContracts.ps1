param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
. (Join-Path $PSScriptRoot '../log-audits/Test-VehicleSoakObservationLog.ps1') -LogPath unused -EvidenceRoot unused -Faction US
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleSoakRecoveryObservationProbe.c'))
function Rules([string]$Text){
 $record=[pscustomobject]@{Source=$Text;Code=(ConvertTo-AICFCodeText $Text)}
 $damage=Get-AICFMethodBody $record 'AICF_SoakObservationAcceptDamage'
 $dead=Get-AICFMethodBody $record 'AICF_SoakObservationAcceptCapturedDeath'
 foreach($guard in @('evidence.Matches(trip)','evidence.GetStatus() != AICF_ETripRetirementObservation.PENDING && !evidence.IsConfirmed()','evidence.MatchesCapturedActor(m_aVehicleSoakMembers[index], m_aSoakObservationComponentRpl[index])','trip.GetGroupGeneration() != m_iVehicleSoakGeneration','m_SoakObservationLease.GetEntityId() != m_VehicleSoakVehicleId','m_bSoakObservationCleanup && evidence.IsConfirmed()')){if(!$dead.Contains($guard)){'TYPED_NATIVE_DEAD_EXACT_IDENTITY_AND_CONFIRMED_RELEASE';break}}
 foreach($guard in @('trip.GetLease() != m_SoakObservationLease','car.GetID() != m_VehicleSoakVehicleId','m_SoakObservationLease.GetRplId() != m_sSoakObservationCarRpl','trip.GetGroupGeneration() != m_iVehicleSoakGeneration','rpl.Id().ToString() != m_sSoakObservationCarRpl','damage.GetMovementDamage()','SCR_AIVehicleUsability.VehicleIsOnFire(car, damage)','!trip.GetHandoffState().IsOrderRestored()','slot.GetGroup().GetCurrentWaypoint() != waypoint')){if(!$damage.Contains($guard)){'ACTUAL_NATIVE_DAMAGE_IDENTITY';break}}
 $actors=Get-AICFMethodBody $record 'AICF_SoakObservationActors'
 foreach($guard in @('actor.GetID() != m_aVehicleSoakMembers[index]','Replication.FindItemId(actor).ToString() != m_aSoakObservationActorRpl[index]','GetCharacterController().GetLifeState()','access.IsInCompartment()','access.IsSwitchingSeatsAnim()')){if(!$actors.Contains($guard)){'EXACT_ALL_ORIGINALS';break}}
 $tick=Get-AICFMethodBody $record 'AICF_SoakObservationTick'
 foreach($guard in @('receipt.m_iConfirmedAt > receipt.m_iTerminalAt','m_SoakObservationLease.IsCapActive()','slot.GetSpawnGeneration() != m_iVehicleSoakGeneration + 1','HasExactFactionRoster','leader.GetID() != m_SoakObservationReplacementLeaderId','m_aSoakObservationReplacementMembers[position]')){if(!$tick.Contains($guard)){'BOUNDED_CLEANUP_AND_REPLACEMENT';break}}
 foreach($guard in @('footLeader.GetID() != m_SoakObservationSurvivorLeaderId','m_vSoakObservationSurvivorStart = footLeader.GetOrigin()','survivorMoved < 10','slot.GetRosterMember(member) != original','SCR_AIDamageHandling.IsConscious(original)')){if(!$tick.Contains($guard)){'ACTUAL_SURVIVOR_PROGRESS';break}}
 $callback=Get-AICFMethodBody $record 'ReportCleanupConfirmed'
 if(([regex]::Matches($callback,'super.ReportCleanupConfirmed\(job, reason\)')).Count -ne 1 -or !$callback.Contains('job.m_iProtectedClearanceDeadlineAtMs + PROTECTED_CLEARANCE_TERMINAL_GRACE_MS')){'ORIGINAL_CLEANUP_OWNER_AND_DEADLINE'}
 if($record.Code -match '\.(SetHealth|SetHealthScaled|Heal|Kill|SetOrigin|SetVelocity|RequestSpawn|SetLimitOfActiveAIs)\s*\('){'READ_ONLY_OBSERVATION'}
 $enabled=Get-AICFMethodBody $record 'AICF_SoakObservationEnabled'
 if(!$enabled.Contains('aicfVehicleSoakObservation4') -or !$Text.Contains('EXPLAINED_NATIVE_DAMAGE_RECOVERY') -or !$Text.Contains('original_full_route_separate=1')){'EXPLICIT_FRESH_VERSION'}
}
if(@(Rules $source).Count){throw "Observation source failed: $(Rules $source)"}
$mutations=@(
 @('wrong lease','trip.GetLease() != m_SoakObservationLease','false'),
 @('wrong car','car.GetID() != m_VehicleSoakVehicleId','false'),
 @('wrong Rpl','rpl.Id().ToString() != m_sSoakObservationCarRpl','false'),
 @('fabricated damage','damage.GetMovementDamage()','1'),
 @('unknown actor','actor.GetID() != m_aVehicleSoakMembers[index]','false'),
 @('cleanup deadline','receipt.m_iConfirmedAt > receipt.m_iTerminalAt','false'),
 @('generation skip','slot.GetSpawnGeneration() != m_iVehicleSoakGeneration + 1','false'),
 @('leader swap','leader.GetID() != m_SoakObservationReplacementLeaderId','false'),
 @('callback bypass','super.ReportCleanupConfirmed(job, reason);',''),
 @('healing observer','float movement = damage.GetMovementDamage();','float movement = damage.GetMovementDamage(); damage.SetHealthScaled(1);'),
 @('survivor promotion fake progress','footLeader.GetID() != m_SoakObservationSurvivorLeaderId','false'),
 @('unconscious survivor','SCR_AIDamageHandling.IsConscious(original)','true'),
 @('foreign survivor','slot.GetRosterMember(member) != original','false')
 ,@('foreign retirement receipt','evidence.Matches(trip)','true')
 ,@('unconfirmed retirement cleanup','m_bSoakObservationCleanup && evidence.IsConfirmed()','m_bSoakObservationCleanup')
 ,@('wrong captured component identity','evidence.MatchesCapturedActor(m_aVehicleSoakMembers[index], m_aSoakObservationComponentRpl[index])','true')
)
foreach($mutation in $mutations){if(@(Rules ($source.Replace($mutation[1],$mutation[2]))).Count -eq 0){throw "Source mutation escaped: $($mutation[0])"};Write-Output "PASS source negative=$($mutation[0])"}
function M([string]$Name,[string]$Fields){return "[VEHICLE_SOAK_$Name] faction=US numeric_slot=0 $Fields`n"}
$raw=M START 'proof_version=4 observation_policy=ACTUAL_DAMAGE_BOUNDED_RECOVERY group=0xB generation=1'
for($i=0;$i -lt 5;$i++){
 $raw+=M MEMBER "position=$i member=M$i"
 $raw+=M OBS_ACTOR_CAPTURE "position=$i member=M$i actor_rpl=R$i native_capture=1"
 if($i){$raw+="[INFANTRY_RECRUIT_JOINED] faction=US slot=0 group=0xB generation=1 source=PLAYER_RECRUIT cost=20 character=M$i`n"}
}
$raw+=M RECRUITED 'alive=5 desired=5 initial_alive=1 group=0xB generation=1'
$raw+=M VEHICLE 'prefab=Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et cost=100 seats=5 assigned=5 revision=1 authority_path=SERVER_FIXTURE_DOWNSTREAM'
$raw+=M TRIP 'operation_id=O'
$raw+="[VEHICLE_PRESET_PURCHASE] operation=O prefab=Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et supplies=100`n"
for($i=0;$i -lt 5;$i++){$raw+="[VEHICLE_PRESET_OCCUPANT] operation=O position=$i member=M$i occupant=M$i`n"}
$raw+=M BOARDED 'operation_id=O vehicle=0xA occupants=5 seats=5'
$raw+="[VEHICLE_STATE_CHANGED] operation_id=O state=FALLBACK reason=FIRE entity_id=0xA rpl_id=R group_generation=1 trip_generation=2 lease_generation=3 vehicle_generation=4`n"
$raw+="[ORDER_RESTORE_RESULT] operation_id=O reason=FIRE rpl_id=R group_generation=1 trip_generation=2 lease_generation=3 vehicle_generation=4 success=1 is_current=1 bound_to_group=1 waypoint_in_queue=1 postcondition_meaningful_task=1 new_waypoint=0xD`n"
$raw+=M OBS_DAMAGE 'operation_id=O vehicle=0xA rpl_id=R group=0xB generation=1 trip_generation=2 lease_generation=3 vehicle_generation=4 reason=FIRE native_health=1 on_fire=1 movement_damage=1 handoff_restored=1 actual_current_waypoint=0xD intent_revision=7 target=<100,0,100>'
$raw+="[GROUP_TRACE] operation_id=O health_vehicle=0xA health_component_rpl=R health_status=READ_ONLY health_on_fire=1 health_movement_damage=1`n"
for($i=0;$i -lt 5;$i++){$raw+=M OBS_ACTOR_STATE "position=$i member=M$i actor_rpl=R$i native_readback=1 life=DEAD detached=1"}
$raw+="[VEHICLE_CLEANUP_DEFERRED] operation_id=O rpl_id=R terminal_at_ms=100000`n"
$raw+="[VEHICLE_CLEANUP_CONFIRMED] operation_id=O last_entity_id=0xA rpl_id=R group_generation=1 trip_generation=2 lease_generation=3 vehicle_generation=4`n"
$raw+=M OBS_CLEANUP 'operation_id=O vehicle=0xA rpl_id=R confirmed_at_ms=90000 terminal_at_ms=100000 absent=1 cap_held=0 native_completed=1'
$raw+="[DEPLOYMENT_COMMITTED] faction=US slot=0 generation=2 ticket_debit=1 supply_debit=100 roster=5/5`n[GROUP_ROSTER_READY] faction=US numeric_slot=0 generation=2 initial_agents=5 expected_agents=5`n"
$raw+="[SPAWN_BOUND] faction=US slot=0 generation=2 group=0xC`n"
for($i=0;$i -lt 5;$i++){$raw+=M OBS_REPLACEMENT_MEMBER "position=$i member=N$i actor_rpl=NR$i group=0xC generation=2"}
$raw+=M OBS_REPLACEMENT_READY 'operation_id=O group=0xC generation=2 actual=5 faction_correct=1 current_waypoint=0xD intent_revision=7 leader=N0 leader_position=<0,0,0> actual_waypoint_position=<100,0,100>'
$raw+="[GROUP_TRACE] trigger=SOAK_OBSERVATION_RECOVERY group=0xC group_generation=2 intent_revision=7 leader=N0 leader_pos=<20,0,0> current_wp=0xD owned_wp=0xD queue_count=1 wp_pos=<100,0,100>`n"
$raw+=M OBS_RECOVERY_COMPLETE 'operation_id=O group=0xC generation=2 actual=5 moved_m=20 current_waypoint=0xD intent_revision=7 cleanup_complete=1 original_full_route=0 recovery_kind=REPLACEMENT leader=N0 leader_position=<20,0,0> actual_waypoint_position=<100,0,100>'
$raw+=M DONE 'failures=0 route_complete=0 observation_complete=1 observation_category=EXPLAINED_NATIVE_DAMAGE_RECOVERY original_full_route=0'
$raw=$raw.Replace('operation_id=O','operation_id=trip-US-0-1-1').Replace('operation=O','operation=trip-US-0-1-1')
$issues=@(Test-AICFObservationRecovery $raw US)
if($issues.Count){throw "Positive recovery rejected: $issues"}
$deadRaw=$raw.Replace('generation=1'+"`n",'generation=1'+"`n")
$deadRaw=$deadRaw.Replace('observation_policy=ACTUAL_DAMAGE_BOUNDED_RECOVERY','observation_policy=ACTUAL_DAMAGE_BOUNDED_RECOVERY captured_death_policy=TYPED_NATIVE_ALL_DEAD_RELEASE_V1').Replace('observation_category=EXPLAINED_NATIVE_DAMAGE_RECOVERY','observation_category=CONFIRMED_NATIVE_ALL_DEAD_RETIREMENT')
$nativeMembers=''
for($i=0;$i -lt 5;$i++){
 $deadRaw=$deadRaw.Replace("actor_rpl=R$i native_capture=1","actor_rpl=R$i native_capture=1 component_rpl=CR$i")
 $nativeMembers+="[TRIP_RETIREMENT_MEMBER_CAPTURED] operation_id=trip-US-0-1-1 member=M$i actor_rpl=CR$i group_generation=1 trip_generation=2`n"
}
$deadRaw=$deadRaw.Replace('[VEHICLE_SOAK_BOARDED]',$nativeMembers+'[VEHICLE_SOAK_BOARDED]')
$nativeDead=''
for($i=0;$i -lt 5;$i++){$nativeDead+="[TRIP_RETIREMENT_DEAD_MEMBER] operation_id=trip-US-0-1-1 member=M$i actor_rpl=CR$i group_generation=1 trip_generation=2 native_readback=1 life_state=DEAD`n"}
$typedIdentity='operation_id=trip-US-0-1-1 vehicle=0xA rpl_id=R group_generation=1 trip_generation=2 lease_generation=3 vehicle_generation=4 captured_count=5 all_native_dead=1'
$deadRaw=[regex]::Replace($deadRaw,'(?m)^\[VEHICLE_SOAK_OBS_DAMAGE\][^\r\n]*',$nativeDead+(M OBS_CAPTURED_DEATH ($typedIdentity+' generation=1 group=0xB typed_members=5 classification=PENDING intent_revision=7 target=<100,0,100>')).TrimEnd())
$releaseIdentity=$typedIdentity+' classification=CONFIRMED own_cap_at_release=0 release_ack_receipt=1 pool_offer_receipt=1 quarantine_receipt=0 native_absence_receipt=0 cleanup_terminal_at_ms=100000 release_at_ms=88000 release_ack_at_ms=89000'
$deadRaw=[regex]::Replace($deadRaw,'(?m)^\[VEHICLE_SOAK_OBS_CLEANUP\][^\r\n]*',"[CAPTURED_DEATH_RETIREMENT_CONFIRMED] $releaseIdentity`n"+(M OBS_CAPTURED_DEATH_RELEASE ($releaseIdentity+' typed_confirmed=1')).TrimEnd())
$deadIssues=@(Test-AICFObservationRecovery $deadRaw US)
if($deadIssues.Count){throw "Positive typed all-DEAD retirement rejected: $deadIssues"}
foreach($mutation in @(
 @('missing explicit policy','captured_death_policy=TYPED_NATIVE_ALL_DEAD_RELEASE_V1','captured_death_policy=UNKNOWN','OBS_EXPLICIT_TYPED_ALL_DEAD_POLICY'),
 @('fabricated DEAD','life_state=DEAD','life_state=INCAPACITATED','OBS_ALL_ORIGINALS_NATIVE_DEAD_COMPONENT_IDENTITY'),
 @('wrong captured component','component_rpl=CR3','component_rpl=FOREIGN','OBS_ALL_ORIGINALS_NATIVE_DEAD_COMPONENT_IDENTITY'),
 @('missing admission capture','[TRIP_RETIREMENT_MEMBER_CAPTURED]','[UNKNOWN_CAPTURE]','OBS_ALL_ORIGINALS_NATIVE_DEAD_COMPONENT_IDENTITY'),
 @('wrong car release','vehicle=0xA rpl_id=R group_generation=1','vehicle=0xF rpl_id=R group_generation=1','OBS_TYPED_EXACT_RELEASE_IDENTITY'),
 @('missing production confirmation','[CAPTURED_DEATH_RETIREMENT_CONFIRMED]','[UNCONFIRMED]','OBS_INDEPENDENT_TYPED_DEATH_CONFIRMATION'),
 @('pending cleanup cannot pass','classification=CONFIRMED','classification=PENDING','OBS_TYPED_EXACT_RELEASE_IDENTITY'),
 @('missing release acknowledgment','release_ack_receipt=1','release_ack_receipt=0','OBS_TYPED_EXACT_RELEASE_IDENTITY'),
 @('missing physical release','pool_offer_receipt=1','pool_offer_receipt=0','OBS_TYPED_EXACT_RELEASE_IDENTITY'),
 @('late actual release','release_at_ms=88000','release_at_ms=100001','OBS_TYPED_BOUNDED_RELEASE'),
 @('retained own cap','own_cap_at_release=0','own_cap_at_release=1','OBS_TYPED_EXACT_RELEASE_IDENTITY'),
 @('wrong next generation','generation=2 actual=5','generation=3 actual=5','OBS_EXACT_NEXT_GENERATION'),
 @('no physical movement','moved_m=20','moved_m=0','OBS_CURRENT_PHYSICAL_ORDER')
)){
 if(!(@(Test-AICFObservationRecovery ($deadRaw.Replace($mutation[1],$mutation[2])) US) -contains $mutation[3])){throw "Typed retirement mutation escaped: $($mutation[0])"}
}
if(!(@(Test-AICFObservationRecovery ($deadRaw+"`n[GROUP_SPAWN_TIMEOUT]") US) -contains 'OBS_NO_TIMEOUT_OR_ACCOUNTING_FAILURE')){throw 'Typed retirement hid spawn timeout'}
Write-Output 'PASS Typed all-DEAD Proof4: positive=1 independent native/admission/release identities; negatives=14'
$negative=@(
 @('fabricated health','health_on_fire=1 health_movement_damage=1','health_on_fire=0 health_movement_damage=0','OBS_INDEPENDENT_NATIVE_HEALTH'),
 @('wrong damaged car','vehicle=0xA rpl_id=R group=0xB','vehicle=0xF rpl_id=R group=0xB','OBS_DAMAGE_EXACT_IDENTITY'),
 @('missing cleanup','[VEHICLE_CLEANUP_CONFIRMED]','[UNPROVEN_CLEANUP]','OBS_EXACT_BOUNDED_CLEANUP'),
 @('late cleanup','confirmed_at_ms=90000','confirmed_at_ms=100001','OBS_EXACT_BOUNDED_CLEANUP'),
 @('wrong original Rpl','actor_rpl=R3 native_readback=1','actor_rpl=FOREIGN native_readback=1','OBS_ACTOR_NATIVE_STATE'),
 @('incap still seated','life=DEAD detached=1','life=INCAPACITATED detached=0','OBS_EVERY_ORIGINAL_DETACHED'),
 @('replacement skips generation','generation=2 actual=5','generation=3 actual=5','OBS_EXACT_NEXT_GENERATION'),
 @('fabricated movement','moved_m=20','moved_m=0','OBS_CURRENT_PHYSICAL_ORDER'),
 @('missing debit','ticket_debit=1','ticket_debit=0','OBS_NATIVE_REPLACEMENT_ECONOMY_AND_READINESS'),
 @('route falsely claimed','original_full_route=0','original_full_route=1','OBS_EXPLICIT_NON_ROUTE_POLICY'),
 @('old actor reused','member=N3 actor_rpl=NR3','member=M3 actor_rpl=NR3','OBS_NEW_MEMBER_DISTINCT_FROM_ORIGINALS'),
 @('different group bound','generation=2 group=0xC','generation=2 group=0xF','OBS_NATIVE_REPLACEMENT_GROUP_IDENTITY'),
 @('promoted leader false movement','leader=N0 leader_position=<20','leader=N1 leader_position=<20','OBS_MEASURED_SAME_LEADER_PHYSICAL_MOVEMENT'),
 @('native order missing','trigger=SOAK_OBSERVATION_RECOVERY','trigger=UNRELATED','OBS_INDEPENDENT_CURRENT_PHYSICAL_ORDER'),
 @('original seat switched','position=3 member=M3 occupant=M3','position=2 member=M3 occupant=M3','OBS_SETTLED_ORIGINAL_POSITION'),
 @('current waypoint wrong target','actual_waypoint_position=<100,0,100>','actual_waypoint_position=<200,0,100>','OBS_MEASURED_SAME_LEADER_PHYSICAL_MOVEMENT')
)
foreach($mutation in $negative){$changed=$raw.Replace($mutation[1],$mutation[2]);if(!(@(Test-AICFObservationRecovery $changed US) -contains $mutation[3])){throw "Log mutation escaped: $($mutation[0])"};Write-Output "PASS log negative=$($mutation[0])"}
if(!(@(Test-AICFObservationRecovery ($raw+"`n[GROUP_SPAWN_TIMEOUT]") US) -contains 'OBS_NO_TIMEOUT_OR_ACCOUNTING_FAILURE')){throw 'Spawn timeout accepted'}
$survivors=[regex]::Replace($raw,'(?m)^.*\[(?:DEPLOYMENT_COMMITTED|GROUP_ROSTER_READY|SPAWN_BOUND|VEHICLE_SOAK_OBS_REPLACEMENT_MEMBER)\][^\r\n]*\r?\n','')
$survivors=$survivors.Replace('position=0 member=M0 actor_rpl=R0 native_readback=1 life=DEAD','position=0 member=M0 actor_rpl=R0 native_readback=1 life=ALIVE')
$survivors=$survivors.Replace('OBS_REPLACEMENT_READY','OBS_SURVIVORS_READY').Replace('group=0xC generation=2 actual=5 faction_correct=1','group=0xB generation=1 actual=1').Replace('group=0xC generation=2 actual=5 moved_m','group=0xB generation=1 actual=1 moved_m').Replace('group=0xC group_generation=2','group=0xB group_generation=1').Replace('leader=N0','leader=M0').Replace('recovery_kind=REPLACEMENT','recovery_kind=SURVIVORS')
$survivorIssues=@(Test-AICFObservationRecovery $survivors US)
if($survivorIssues.Count){throw "Positive original survivor rejected: $survivorIssues"}
foreach($mutation in @(
 @('fake survivor membership','group=0xB generation=1 actual=1 moved_m','group=0xF generation=1 actual=1 moved_m','OBS_ACTUAL_ORIGINAL_SURVIVORS'),
 @('fake survivor life','native_readback=1 life=ALIVE','native_readback=1 life=DEAD','OBS_ACTUAL_ORIGINAL_SURVIVORS'),
 @('fake survivor promotion progress','leader=M0 leader_position=<20','leader=M1 leader_position=<20','OBS_MEASURED_SAME_LEADER_PHYSICAL_MOVEMENT')
)){
 if(!(@(Test-AICFObservationRecovery ($survivors.Replace($mutation[1],$mutation[2])) US) -contains $mutation[3])){throw "Survivor log mutation escaped: $($mutation[0])"}
 Write-Output "PASS survivor log negative=$($mutation[0])"
}
Write-Output 'Soak observation4 contracts: PASS; source_negatives=13 log_negatives=20 survivor_positive=1'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-observation4-'+[guid]::NewGuid().ToString('N'))
$logs=Join-Path $work 'profile/logs/test'
[void][IO.Directory]::CreateDirectory($logs)
try{
 $argv=@('-gproj','source','-profile',(Join-Path $work 'profile'),'-addonsDir','addons','-addons','CORE,MAP','-aicfVehicleSoakFaction','US','-aicfVehicleSoakDurationMs','1800000')
 [IO.File]::WriteAllText((Join-Path $work 'launcher.txt'),'AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv;profile=(Join-Path $work 'profile')}|ConvertTo-Json -Compress))
 [IO.File]::WriteAllText((Join-Path $work 'native-exit.txt'),'0')
 $started=[DateTimeOffset]::Parse('2026-10-29T00:00:00Z')
 @{started_utc=$started.ToString('o');ended_utc=$started.AddSeconds(1830).ToString('o');wall_seconds=1830;faction='US';requested_seconds=1800}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'timing.json')
 $body=$raw.Replace('proof_version=4 observation_policy=','requested_duration_ms=1800000 initial_alive=1 desired=5 config_accepted=1 home_order_accepted=1 truce=0 invulnerability=0 teleport=0 teleport_scope=FIXTURE_ONLY production_short_recovery=1 behavior=TRANSPORT return_distance_m=300 proof_version=4 observation_policy=')
 $body=[regex]::Replace($body,'(\[VEHICLE_SOAK_([A-Z_]+)\][^\r\n]*)',{param($m) $elapsed=1000;if($m.Groups[2].Value -eq 'START'){$elapsed=0};if($m.Groups[2].Value -eq 'DONE'){$elapsed=1800000};return $m.Groups[1].Value+" t_ms=$(20000+$elapsed) elapsed_ms=$elapsed"})
 $heartbeats='';for($elapsed=20000;$elapsed -lt 1800000;$elapsed+=20000){$heartbeats+="[VEHICLE_SOAK_HEARTBEAT] faction=US numeric_slot=0 t_ms=$(20000+$elapsed) elapsed_ms=$elapsed`n"}
 $body=$body.Replace('[VEHICLE_SOAK_DONE]',$heartbeats+'[VEHICLE_SOAK_DONE]').Replace('failures=0 route_complete=0','requested_duration_ms=1800000 duration_satisfied=1 failures=0 route_complete=0')
 $body='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"+$body+"Game destroyed.`n"
 foreach($case in @(
  @{name='full-positive';sibling='';body=$body;expected=0;gate='functional';verdict='PASS'},
  @{name='sibling-native-assertion';sibling='Assertion failed enf_entity.cpp280';body=$body;expected=1;gate='fatal_errors';verdict='FAIL'},
  @{name='sibling-resource-errors';sibling='RESOURCES (E): Missing texture';body=$body;expected=1;gate='resource_errors';verdict='FAIL'},
  @{name='missing-launcher-profile';sibling='';body=$body.Replace('-profile '+(Join-Path $work 'profile'),'-profile wrong');expected=1;gate='provenance';verdict='FAIL'},
  @{name='short-engine-observation';sibling='';body=$body.Replace('t_ms=1820000 elapsed_ms=1800000','t_ms=1819000 elapsed_ms=1799000');expected=1;gate='duration';verdict='FAIL'},
  @{name='missing-heartbeats';sibling='';body=([regex]::Replace($body,'(?m)^\[VEHICLE_SOAK_HEARTBEAT\][^\r\n]*\r?\n',''));expected=1;gate='liveness';verdict='FAIL'},
  @{name='native-log-not-stopped';sibling='';body=$body.Replace('Game destroyed.','');expected=1;gate='stopped_native';verdict='FAIL'}
 )){
  [IO.File]::WriteAllText((Join-Path $logs 'console.log'),$case.body)
  [IO.File]::WriteAllText((Join-Path $logs 'crash.log'),$case.sibling)
  $result=& pwsh -NoProfile -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleSoakObservationLog.ps1') -LogPath (Join-Path $logs 'console.log') -EvidenceRoot $work -Faction US -ProofVersion 4
  $nativeExit=$LASTEXITCODE
  $summary=Get-Content -LiteralPath (Join-Path $work 'audit-summary.json') -Raw|ConvertFrom-Json
  if($nativeExit -ne $case.expected -or $summary.gates.($case.gate).verdict -ne $case.verdict){throw "Full observation case failed: $($case.name) $result"}
  Write-Output "PASS full observation case=$($case.name)"
 }
}finally{if(Test-Path -LiteralPath $work){Remove-Item -LiteralPath $work -Recurse -Force}}
