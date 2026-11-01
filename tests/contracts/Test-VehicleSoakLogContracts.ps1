param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-soak-contract-'+[guid]::NewGuid().ToString('N'))
$profile=Join-Path $work 'profile'
$logs=Join-Path $profile 'logs/run'
[void][IO.Directory]::CreateDirectory($logs)
$script:checks=0
function Marker([string]$name,[int]$elapsed,[string]$details,[string]$faction='US'){
 return "[AICF][VEHICLE_SOAK_$name] faction=$faction numeric_slot=0 t_ms=$(20000+$elapsed) elapsed_ms=$elapsed $details`n"
}
function Fixture([string]$faction='US'){
 $prefab='{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et'
 if($faction -eq 'USSR'){$prefab='{UUID}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et'}
 $argv=@('-gproj','source','-profile',$profile,'-addonsDir','addons','-addons','CORE,MAP','-aicfVehicleSoakFaction',$faction,'-aicfVehicleSoakDurationMs','1800000')
 [IO.File]::WriteAllText((Join-Path $work 'launcher.txt'),'AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv;profile=$profile}|ConvertTo-Json -Compress))
 $body='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"
 $body+=Marker START 0 'requested_duration_ms=1800000 initial_alive=1 desired=5 group=G generation=1 config_accepted=1 home_order_accepted=1 truce=0 invulnerability=0 teleport=0' $faction
 $body+=Marker HEARTBEAT 0 'stage=0 alive=1 desired=5 generation=1 group=G trip_phase=NONE trip_reason=NONE recruiting=0 route_complete=0' $faction
 $body+="[INFANTRY_RECRUITMENT_STARTED] faction=$faction slot=0 group=G generation=1 token=1 source=PLAYER_RECRUIT`n"
 $body+=Marker RECRUIT_REQUEST 1000 'accepted=1 reason=RECRUITMENT_STARTED alive=1 desired=5 authority_path=SERVER_FIXTURE_DOWNSTREAM' $faction
 for($i=1;$i -le 4;$i++){$body+="[INFANTRY_RECRUIT_JOINED] faction=$faction slot=0 group=G generation=1 token=1 source=PLAYER_RECRUIT cost=10 character=M$i`n"}
 $body+="[INFANTRY_RECRUITMENT_FINISHED] faction=$faction slot=0 group=G generation=1 token=1 source=PLAYER_RECRUIT reason=ROSTER_COMPLETE_OR_EMPTY`n"
 $body+=Marker RECRUITED 2000 'alive=5 desired=5 initial_alive=1 group=G generation=1' $faction
 for($i=0;$i -lt 5;$i++){$body+=Marker MEMBER 2000 "position=$i member=M$i" $faction}
 $body+=Marker VEHICLE 3000 "prefab=$prefab seats=5 assigned=5 revision=1 cost=475 authority_path=SERVER_FIXTURE_DOWNSTREAM" $faction
 $operation="trip-$faction-0-1-1"
 $body+=Marker TRIP 4000 "operation_id=$operation" $faction
 $body+="[VEHICLE_PRESET_PURCHASE] operation=$operation prefab=$prefab supplies=475`n"
 for($i=0;$i -lt 5;$i++){
  $body+="[VEHICLE_PRESET_SEAT_REQUESTED] operation=$operation position=$i seat=0:$i`:0 member=M$i sequential=1`n"
  $body+="[VEHICLE_PRESET_OCCUPANT] operation=$operation position=$i seat=0:$i`:0 member=M$i occupant=M$i`n"
 }
 $body+=Marker BOARDED 5000 "operation_id=$operation vehicle=0x4000000000002E69{} seats=5 occupants=5 start=<0,0,0>" $faction
 $body+=Marker TARGET 6000 "operation_id=$operation position=<500,0,0> intent_revision=2 vehicle=0x4000000000002E69{} dismount_radius_m=150" $faction
 $body+=Marker ARRIVAL 7000 "operation_id=$operation passed=1 vehicle=0x4000000000002E69 last_position=<355,0,0> distance_m=145 displacement_m=355 moving_polls=12 intent_revision=2" $faction
 $body+="[DISEMBARK_COMPLETE] operation_id=$operation logical_occupants=0 transitions=0 inside_bounds=0`n"
 $body+="[VEHICLE_STATE_CHANGED] operation_id=$operation previous_state=HANDOFF state=COMPLETE`n"
 $body+=Marker DISMOUNTED 8000 "operation_id=$operation alive=5 in_vehicle=0 normal_complete=1" $faction
 $body+=Marker FOOT_ARRIVAL 9000 "operation_id=$operation alive=5 farthest_m=15 position=<500,0,0> intent_revision=2" $faction
 for($elapsed=20000;$elapsed -lt 1800000;$elapsed+=20000){$body+=Marker HEARTBEAT $elapsed 'stage=7 alive=5 desired=5 generation=1 group=G trip_phase=NONE trip_reason=NONE recruiting=0 route_complete=1' $faction}
 $body+=Marker DONE 1800000 'requested_duration_ms=1800000 duration_satisfied=1 failures=0 route_complete=1 phase=7' $faction
 return $body+'Game destroyed.'
}
function RetainedFixture([string]$faction='US'){
 $body=Fixture $faction
 $operation="trip-$faction-0-1-1"
 $body=$body.Replace('truce=0 invulnerability=0 teleport=0','truce=0 invulnerability=0 teleport=0 proof_version=2 teleport_scope=FIXTURE_ONLY production_short_recovery=1 behavior=TRANSPORT return_distance_m=300')
 $deployment="[VEHICLE_STATE_CHANGED] operation_id=$operation reason=DESTINATION_REACHED previous_state=TRANSIT state=TACTICAL_DEPLOYMENT`n"
 $body=$body.Replace('[AICF][VEHICLE_SOAK_ARRIVAL]',$deployment+'[AICF][VEHICLE_SOAK_ARRIVAL]')
 $body=$body.Replace("[DISEMBARK_COMPLETE] operation_id=$operation logical_occupants=0 transitions=0 inside_bounds=0`n",'')
 $body=$body.Replace("[VEHICLE_STATE_CHANGED] operation_id=$operation previous_state=HANDOFF state=COMPLETE`n",'')
 $retained="[ORDER_RESTORE_RESULT] operation_id=$operation success=1 bound_to_group=1 is_current=1 waypoint_in_queue=1 postcondition_meaningful_task=1`n"
 for($position=0;$position -lt 5;$position++){$retained+=Marker MEMBER_FOOT 8000 "operation_id=$operation position=$position member=M$position on_foot=1 transition=0 inside_bounds=0" $faction}
 $retained+=Marker TRIP_RETAINED_FOOT 8000 "operation_id=$operation vehicle=0x4000000000002E69{} phase=TACTICAL_DEPLOYMENT alive=5 in_vehicle=0 logical_occupants=0 transitions=0 inside_bounds=0 order_restored=1 same_lease=1 reboarding=0 clear_polls=2 proof_version=2" $faction
 return $body.Replace((Marker DISMOUNTED 8000 "operation_id=$operation alive=5 in_vehicle=0 normal_complete=1" $faction),$retained)
}
function CombatAwareFixture([string]$faction='US',[switch]$Combined,[switch]$MountedCombat){
 $body=(RetainedFixture $faction).Replace('proof_version=2','proof_version=3')
 $operation="trip-$faction-0-1-1"
 $body=$body.Replace('distance_m=145 displacement_m=355 moving_polls=12 intent_revision=2','distance_m=145 displacement_m=355 moving_polls=12 intent_revision=2 category=DIRECT_VEHICLE_ARRIVAL')
 $held=12900;if($MountedCombat){$held=12300}
 $hold=Marker MOUNTED_HOLD 5900 "operation_id=$operation configured_ms=10000 held_ms=$held exact_settled=1 alive=5" $faction
 $body=$body.Replace('[AICF][VEHICLE_SOAK_TARGET]',$hold+'[AICF][VEHICLE_SOAK_TARGET]')
 if($MountedCombat){
  $beforeHold="[VEHICLE_STATE_CHANGED] operation_id=$operation reason=LOCAL_THREAT previous_state=TRANSIT state=TACTICAL_DEPLOYMENT`n"
  $beforeHold+=Marker MOUNTED_COMBAT_WAIT 5500 "operation_id=$operation reason=LOCAL_THREAT proof=OWN_TACTICAL_DEPLOYMENT" $faction
  $beforeHold+=Marker MOUNTED_HOLD_RESUMED 5600 "operation_id=$operation exact_settled=1 calm=1" $faction
  $body=$body.Replace('[AICF][VEHICLE_SOAK_MOUNTED_HOLD]',$beforeHold+'[AICF][VEHICLE_SOAK_MOUNTED_HOLD]')
 }
 if($Combined){
  $body=$body.Replace('reason=DESTINATION_REACHED','reason=LOCAL_THREAT').Replace('distance_m=145 displacement_m=355','distance_m=300 displacement_m=355').Replace('category=DIRECT_VEHICLE_ARRIVAL','category=COMBAT_INTERRUPTED_COMBINED_ROUTE')
  $interrupted=Marker COMBAT_INTERRUPTION 6500 "operation_id=$operation reason=LOCAL_THREAT phase=TACTICAL_DEPLOYMENT vehicle=0x4000000000002E69{} intent_revision=2" $faction
  $body=$body.Replace('[AICF][VEHICLE_SOAK_ARRIVAL]',$interrupted+'[AICF][VEHICLE_SOAK_ARRIVAL]')
 }
 $body=[regex]::Replace($body,'(?m)(\[VEHICLE_SOAK_(?:MOUNTED_HOLD|TARGET|COMBAT_INTERRUPTION|ARRIVAL|MEMBER_FOOT|TRIP_RETAINED_FOOT|FOOT_ARRIVAL)\][^\r\n]*t_ms=)(\d+)( elapsed_ms=)(\d+)',{param($match) $match.Groups[1].Value+([long]$match.Groups[2].Value+12000).ToString()+$match.Groups[3].Value+([long]$match.Groups[4].Value+12000).ToString()})
 return $body
}
function SurvivorFixture([string]$life='INCAPACITATED'){
 $body=CombatAwareFixture US -Combined
 $body=$body.Replace('return_distance_m=300','return_distance_m=300 casualty_policy=CAPTURED_ORIGINAL_SURVIVORS')
 $body=[regex]::Replace($body,'(?m)^.*\[VEHICLE_SOAK_MEMBER_FOOT\][^\r\n]*\r?\n','')
 $body=$body.Replace('phase=TACTICAL_DEPLOYMENT alive=5 in_vehicle=0','phase=TACTICAL_DEPLOYMENT alive=4 in_vehicle=0')
 $body=$body.Replace('clear_polls=2 proof_version=3','clear_polls=2 proof_version=3 fit_count=4 originals=5 survivor_proof=1')
 $body=$body.Replace('alive=5 farthest_m=15 position=<500,0,0> intent_revision=2','alive=4 farthest_m=15 position=<500,0,0> intent_revision=2 fit_count=4 originals=5 survivor_proof=1')
 foreach($kind in @('MEMBER_FOOT','MEMBER_ARRIVAL')){
  $receipts=''
  for($position=0;$position -lt 5;$position++){
   $state='ALIVE';$fit=1;$distance=15
   if($position -eq 4){$state=$life;$fit=0;$distance=180}
   $receipts+=Marker $kind 20000 "operation_id=trip-US-0-1-1 position=$position member=M$position life=$state conscious=$fit fit=$fit current_member=$fit target_distance_m=$distance captured=1 on_foot=1 transition=0 inside_bounds=0 survivor_proof=1"
  }
  $before='[AICF][VEHICLE_SOAK_TRIP_RETAINED_FOOT]'
  if($kind -eq 'MEMBER_ARRIVAL'){$before='[AICF][VEHICLE_SOAK_FOOT_ARRIVAL]'}
  $body=$body.Replace($before,$receipts+$before)
 }
 return $body
}
function Run([string]$name,[string]$body,[int]$expected,[string]$faction='US',[string]$sibling='',[double]$wall=1830,[string]$nativeExit='0',[string]$durationVerdict='PASS',[string]$livenessVerdict='PASS',[int]$proofVersion=1,[string]$functionalVerdict=''){
 [IO.File]::WriteAllText((Join-Path $logs 'console.log'),$body)
 [IO.File]::WriteAllText((Join-Path $logs 'script.log'),$sibling)
 [IO.File]::WriteAllText((Join-Path $work 'native-exit.txt'),$nativeExit)
 $started=[DateTimeOffset]::Parse('2026-10-26T16:00:00Z')
 @{started_utc=$started.ToString('o');ended_utc=$started.AddSeconds($wall).ToString('o');wall_seconds=$wall;faction=$faction;requested_seconds=1800}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'timing.json')
 $result=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleSoakLog.ps1') -LogPath (Join-Path $logs 'console.log') -EvidenceRoot $work -Faction $faction -MinimumSeconds 1800 -ProofVersion $proofVersion
 if($LASTEXITCODE -ne $expected){throw "$name unexpected verdict: $result"}
 $summary=Get-Content -LiteralPath (Join-Path $work 'audit-summary.json') -Raw|ConvertFrom-Json
 if($summary.gates.duration.verdict -ne $durationVerdict -or $summary.gates.liveness.verdict -ne $livenessVerdict){throw "$name lost separate duration/liveness verdict: $($summary|ConvertTo-Json -Depth 6 -Compress)"}
 if($functionalVerdict -and $summary.gates.functional.verdict -ne $functionalVerdict){throw "$name unexpected functional verdict: $($summary|ConvertTo-Json -Depth 6 -Compress)"}
 $script:checks++
}
try{
 $good=Fixture US
 Run good-US-raw-entity-format $good 0
 $oneMsOffset=[regex]::Replace($good,'(t_ms=)(\d+)( elapsed_ms=)(\d+)',{param($m) $m.Groups[1].Value+([long]$m.Groups[2].Value+1).ToString()+$m.Groups[3].Value+([long]$m.Groups[4].Value+1).ToString()})
 Run good-START-one-ms-offset $oneMsOffset 0
 Run wrong-START-clock-origin ($oneMsOffset.Replace('t_ms=20001 elapsed_ms=1','t_ms=20002 elapsed_ms=1')) 1 US '' 1830 '0' FAIL
 Run negative-START-elapsed ($good.Replace('t_ms=20000 elapsed_ms=0 requested_duration','t_ms=19999 elapsed_ms=-1 requested_duration')) 1 US '' 1830 '0' FAIL
 Run good-US-spaced-entity-format ($good.Replace('vehicle=0x4000000000002E69 last_position','vehicle=0x4000000000002E69 {} last_position')) 0
 $nativeProjection=[regex]::Replace($good,'\bM([0-4])\b',{param($m) '0x4000000000000'+(256+[int]$m.Groups[1].Value).ToString('X3')+' {}'})
 $nativeProjection=[regex]::Replace($nativeProjection,'(?m)(\[VEHICLE_PRESET_OCCUPANT\][^\r\n]*member=)(0x[0-9A-Fa-f]+) \{\} occupant=(0x[0-9A-Fa-f]+) \{\}',{param($m) $m.Groups[1].Value+([Convert]::ToUInt64($m.Groups[2].Value.Substring(2),16) -band 4294967295L).ToString()+' occupant='+([Convert]::ToUInt64($m.Groups[3].Value.Substring(2),16) -band 4294967295L).ToString()})
 Run native-low32-occupant-format $nativeProjection 0
 Run swapped-native-position-members ($nativeProjection.Replace('position=4 seat=0:4:0 member=260 occupant=260','position=4 seat=0:4:0 member=259 occupant=259')) 1
 Run wrong-full-seat-request ($nativeProjection.Replace('position=4 seat=0:4:0 member=0x4000000000000104','position=4 seat=0:4:0 member=0x5000000000000104')) 1
 $ussr=Fixture USSR
 Run good-USSR $ussr 0 USSR
 $good=Fixture US
 foreach($mutation in @(
  @('no-recruit','accepted=1 reason=RECRUITMENT_STARTED','accepted=0 reason=NO_SAFE_BARRACKS'),
  @('fake-recruit','[INFANTRY_RECRUIT_JOINED]','[NO_NATIVE_JOIN]'),
  @('unpaid-recruit','cost=10 character=M1','cost=0 character=M1'),
  @('stale-recruit','generation=1 token=1','generation=2 token=1'),
  @('duplicate-member','position=4 member=M4','position=4 member=M3'),
  @('missing-member','[VEHICLE_SOAK_MEMBER]','[NO_MEMBER]'),
  @('wrong-faction-model','/M1025_armed_M2HB.et','/UAZ469.et'),
  @('wrong-capacity','seats=5 assigned=5','seats=8 assigned=5'),
  @('not-full-boarding','occupants=5','occupants=4'),
  @('wrong-exact-seat','member=M4 occupant=M4','member=M4 occupant=M3'),
  @('duplicate-seat','seat=0:4:0','seat=0:3:0'),
  @('no-purchase','[VEHICLE_PRESET_PURCHASE]','[NO_PURCHASE]'),
  @('wrong-purchase-price','supplies=475','supplies=1'),
  @('swapped-vehicle','vehicle=0x4000000000002E69 last_position','vehicle=0x4000000000002E68 last_position'),
  @('arbitrary-identity-suffix','vehicle=0x4000000000002E69 last_position','vehicle=0x4000000000002E69 {Vehicle} last_position'),
  @('replacement-native-members','position=4 seat=0:4:0 member=M4 occupant=M4','position=4 seat=0:4:0 member=M9 occupant=M9'),
  @('missing-full-seat-request','[VEHICLE_PRESET_SEAT_REQUESTED]','[NO_REQUEST]'),
  @('short-drive','displacement_m=355','displacement_m=99'),
  @('no-motion','moving_polls=12','moving_polls=4'),
  @('far-arrival','distance_m=145','distance_m=161'),
  @('failed-arrival','passed=1 vehicle=','passed=0 vehicle='),
  @('retargeted','moving_polls=12 intent_revision=2','moving_polls=12 intent_revision=3'),
  @('wrong-foot-position','farthest_m=15 position=<500,0,0>','farthest_m=15 position=<25,0,0>'),
  @('wrong-operation','operation_id=trip-US-0-1-1 alive=5 farthest','operation_id=trip-US-0-1-2 alive=5 farthest'),
  @('not-normal-complete','normal_complete=1','normal_complete=0'),
  @('remaining-occupant','logical_occupants=0','logical_occupants=1'),
  @('fallback','previous_state=HANDOFF state=COMPLETE','previous_state=DISMOUNT state=FALLBACK'),
  @('incomplete-foot','farthest_m=15','farthest_m=36'),
  @('dead-member','alive=5 farthest_m=15','alive=4 farthest_m=15'),
  @('protected-game','truce=0 invulnerability=0','truce=1 invulnerability=1'),
  @('not-stopped','Game destroyed.',''),
  @('no-roster','[ROSTER_READY]','[NO_ROSTER]')
 )){Run $mutation[0] ($good.Replace($mutation[1],$mutation[2])) 1}
 Run battle-failure-still-thirty-minutes ($good.Replace('failures=0 route_complete=1','failures=1 route_complete=1')+"`n"+(Marker FAILURE 10000 'reason=CASUALTY')) 1
 Run sibling-script-error $good 1 US 'SCRIPT (E): Virtual Machine Exception'
 Run sibling-engine-fatal $good 1 US 'ENGINE (F): Assertion failed'
 Run sibling-soak-failure $good 1 US '[VEHICLE_SOAK_FAILURE] reason=HIDDEN_FAIL'
 Run sibling-native-resource-visible $good 1 US 'RESOURCES (E): Missing texture'
 Run latched-failure ($good+"`n[ACCEPTANCE_FAILURE_LATCHED] reason=TIMEOUT") 1
 Run native-exit $good 1 US '' 1830 '1'
 Run short-wall-time $good 1 US '' 1799 '0' FAIL
 $early=$good.Replace('t_ms=1820000 elapsed_ms=1800000 requested_duration_ms=1800000 duration_satisfied=1','t_ms=1819999 elapsed_ms=1799999 requested_duration_ms=1800000 duration_satisfied=1')
 Run premature-done $early 1 US '' 1830 '0' FAIL
 $twentyNineFiftyNine=[regex]::Replace($good,'(?m)^\[AICF\]\[VEHICLE_SOAK_HEARTBEAT\][^\r\n]*elapsed_ms=179\d+[^\r\n]*\r?\n','').Replace('t_ms=1820000 elapsed_ms=1800000 requested_duration_ms=1800000 duration_satisfied=1','t_ms=1819000 elapsed_ms=1799000 requested_duration_ms=1800000 duration_satisfied=1')
 Run twenty-nine-minutes-fifty-nine-seconds $twentyNineFiftyNine 1 US '' 1830 '0' FAIL
 Run falsely-short-request ($good.Replace('requested_duration_ms=1800000','requested_duration_ms=1000')) 1 US '' 1830 '0' FAIL
 Run aborted ($good.Replace('[VEHICLE_SOAK_DONE]','[VEHICLE_SOAK_ABORTED]')) 1 US '' 1830 '0' FAIL FAIL
 $gap=[regex]::Replace($good,'(?m)^\[AICF\]\[VEHICLE_SOAK_HEARTBEAT\][^\r\n]*elapsed_ms=(?:700000|720000|740000|760000)[^\r\n]*\r?\n','')
 Run heartbeat-gap $gap 1 US '' 1830 '0' PASS FAIL
 Run heartbeat-clock-regression ($good.Replace('t_ms=60000 elapsed_ms=40000','t_ms=30000 elapsed_ms=10000')) 1 US '' 1830 '0' PASS FAIL
 $noHeartbeats=[regex]::Replace($good,'(?m)^\[AICF\]\[VEHICLE_SOAK_HEARTBEAT\][^\r\n]*\r?\n','')
 Run no-heartbeats $noHeartbeats 1 US '' 1830 '0' PASS FAIL
 $launcherPath=Join-Path $work 'launcher.txt'
 $manifest=[IO.File]::ReadAllText($launcherPath)
 [IO.File]::WriteAllText($launcherPath,$manifest.Replace('"US"','"USSR"'))
 Run wrong-manifest-faction $good 1
 [IO.File]::WriteAllText($launcherPath,$manifest)
 Run console-cli-mismatch ($good.Replace('-aicfVehicleSoakFaction US ','-aicfVehicleSoakFaction USSR ')) 1
 $retained=RetainedFixture US
 Run retained-US $retained 0 -proofVersion 2
 Run retained-short-wall $retained 1 -wall 1799 -durationVerdict FAIL -proofVersion 2
 Run legacy-proof-not-current $good 1 -proofVersion 2
 Run retained-proof-not-legacy $retained 1 -proofVersion 1
 foreach($mutation in @(
  @('retained-no-version','proof_version=2','proof_version=1'),
  @('retained-mode-mismatch','behavior=TRANSPORT','behavior=COMBAT'),
  @('retained-false-teleport-scope','teleport_scope=FIXTURE_ONLY','teleport_scope=GLOBAL'),
  @('retained-wrong-vehicle','vehicle=0x4000000000002E69{} phase=TACTICAL','vehicle=0x4000000000002E68{} phase=TACTICAL'),
  @('retained-wrong-trip','operation_id=trip-US-0-1-1 vehicle=0x4000000000002E69{} phase=TACTICAL','operation_id=trip-US-0-1-2 vehicle=0x4000000000002E69{} phase=TACTICAL'),
  @('retained-not-same-lease','same_lease=1','same_lease=0'),
  @('retained-not-restored','order_restored=1','order_restored=0'),
  @('retained-reboarding','reboarding=0','reboarding=1'),
  @('retained-one-clear-poll','clear_polls=2','clear_polls=1'),
  @('retained-occupants','logical_occupants=0','logical_occupants=1'),
  @('retained-transitions','transitions=0','transitions=1'),
  @('retained-hull-ghost','inside_bounds=0','inside_bounds=1'),
  @('retained-replaced-member','position=4 member=M4 on_foot','position=4 member=M9 on_foot'),
  @('retained-member-transition','transition=0 inside_bounds','transition=1 inside_bounds'),
  @('retained-absent-member-physics','[VEHICLE_SOAK_MEMBER_FOOT]','[NO_MEMBER_FOOT]'),
  @('retained-absent-native-deployment','[VEHICLE_STATE_CHANGED]','[NO_NATIVE_STATE]'),
  @('retained-native-threat-interruption','reason=DESTINATION_REACHED','reason=LOCAL_THREAT'),
  @('retained-absent-native-order','[ORDER_RESTORE_RESULT]','[NO_ORDER_RESTORE]'),
  @('retained-native-order-not-current','is_current=1','is_current=0'),
  @('retained-no-motion','moving_polls=12','moving_polls=4'),
  @('retained-dead-member','alive=5 farthest_m=15','alive=4 farthest_m=15'),
  @('retained-unpaid-recruit','cost=10 character=M1','cost=0 character=M1'),
  @('retained-fallback','state=TACTICAL_DEPLOYMENT','state=FALLBACK')
 )){Run $mutation[0] ($retained.Replace($mutation[1],$mutation[2])) 1 -proofVersion 2}
 Run retained-false-complete ($retained+"`n"+(Marker DISMOUNTED 10000 'normal_complete=1')) 1 -proofVersion 2
 Run retained-resource-strict $retained 1 -sibling 'RESOURCES (E): Missing texture' -proofVersion 2
 Run retained-script-strict $retained 1 -sibling 'SCRIPT (E): native fault' -proofVersion 2
 $retainedUSSR=RetainedFixture USSR
 Run retained-USSR $retainedUSSR 0 -faction USSR -proofVersion 2
 $direct=CombatAwareFixture US
 Run combat-aware-direct $direct 0 -proofVersion 3
 Run combat-aware-direct-still-far ($direct.Replace('distance_m=145','distance_m=300')) 1 -proofVersion 3
 $combined=CombatAwareFixture US -Combined
 Run combat-interrupted-combined-route $combined 0 -proofVersion 3
 foreach($mutation in @(
  @('combined-missing-real-native-threat','reason=LOCAL_THREAT previous_state=TRANSIT','reason=DESTINATION_REACHED previous_state=TRANSIT'),
  @('combined-missing-interruption','[VEHICLE_SOAK_COMBAT_INTERRUPTION]','[NO_INTERRUPTION]'),
  @('combined-intent-changed','phase=TACTICAL_DEPLOYMENT vehicle=0x4000000000002E69{} intent_revision=2','phase=TACTICAL_DEPLOYMENT vehicle=0x4000000000002E69{} intent_revision=3'),
  @('combined-wrong-vehicle','phase=TACTICAL_DEPLOYMENT vehicle=0x4000000000002E69{} intent_revision','phase=TACTICAL_DEPLOYMENT vehicle=0x4000000000002E68{} intent_revision'),
  @('combined-unknown-category','category=COMBAT_INTERRUPTED_COMBINED_ROUTE','category=INTENT_ONLY'),
  @('combined-no-drive','displacement_m=355','displacement_m=99'),
  @('combined-no-moving-polls','moving_polls=12','moving_polls=4'),
  @('combined-no-foot-arrival','farthest_m=15','farthest_m=36'),
  @('combined-member-dead','alive=5 farthest_m=15','alive=4 farthest_m=15'),
  @('combined-missing-calm-hold','[VEHICLE_SOAK_MOUNTED_HOLD]','[NO_CALM_HOLD]'),
  @('combined-short-calm-hold','held_ms=12900','held_ms=9999'),
  @('combined-fabricated-calm-clock','held_ms=12900','held_ms=20000')
 )){Run $mutation[0] ($combined.Replace($mutation[1],$mutation[2])) 1 -proofVersion 3}
 Run combined-not-legacy-proof2 $combined 1 -proofVersion 2
 $mounted=CombatAwareFixture US -MountedCombat
 Run mounted-native-combat-resumed $mounted 0 -proofVersion 3
 Run mounted-no-real-native-combat ($mounted.Replace('reason=LOCAL_THREAT previous_state=TRANSIT','reason=OTHER previous_state=TRANSIT')) 1 -proofVersion 3
 Run mounted-no-exact-return ($mounted.Replace('[VEHICLE_SOAK_MOUNTED_HOLD_RESUMED]','[NO_RETURN]')) 1 -proofVersion 3
 $combinedUSSR=CombatAwareFixture USSR -Combined
 Run combined-USSR $combinedUSSR 0 -faction USSR -proofVersion 3
 $survivors=SurvivorFixture
 Run natural-incapacitated-detached-four-arrive $survivors 0 -proofVersion 3
 Run natural-dead-detached-four-arrive (SurvivorFixture DEAD) 0 -proofVersion 3
 foreach($mutation in @(
  @('survivors-no-explicit-policy',' casualty_policy=CAPTURED_ORIGINAL_SURVIVORS',''),
  @('survivors-unknown-policy','CAPTURED_ORIGINAL_SURVIVORS','ANY_SURVIVOR'),
  @('survivors-replaced-actor','position=4 member=M4 life=INCAPACITATED','position=4 member=M9 life=INCAPACITATED'),
  @('survivors-current-actor-replaced','current_member=1 target_distance','current_member=0 target_distance'),
  @('survivors-incap-still-mounted','target_distance_m=180 captured=1 on_foot=1','target_distance_m=180 captured=1 on_foot=0'),
  @('survivors-incap-still-transition','target_distance_m=180 captured=1 on_foot=1 transition=0','target_distance_m=180 captured=1 on_foot=1 transition=1'),
  @('survivors-incap-inside-hull','target_distance_m=180 captured=1 on_foot=1 transition=0 inside_bounds=0','target_distance_m=180 captured=1 on_foot=1 transition=0 inside_bounds=1'),
  @('survivors-fit-distance-far','target_distance_m=15','target_distance_m=36'),
  @('survivors-fabricated-farthest','farthest_m=15','farthest_m=14'),
  @('survivors-wrong-fit-count','fit_count=4','fit_count=3'),
  @('survivors-no-fit','life=ALIVE conscious=1 fit=1','life=INCAPACITATED conscious=0 fit=0'),
  @('survivors-missing-arrival-readback','[VEHICLE_SOAK_MEMBER_ARRIVAL]','[NO_MEMBER_ARRIVAL]'),
  @('survivors-startup-not-five','alive=5 desired=5 initial_alive','alive=4 desired=5 initial_alive'),
  @('survivors-not-five-boarded','seats=5 occupants=5','seats=5 occupants=4'),
  @('survivors-unpaid','cost=10 character=M1','cost=0 character=M1'),
  @('survivors-order-not-restored','order_restored=1','order_restored=0'),
  @('survivors-wrong-intent','intent_revision=2 fit_count','intent_revision=3 fit_count')
 )){Run $mutation[0] ($survivors.Replace($mutation[1],$mutation[2])) 1 -proofVersion 3}
 Run survivors-native-fatal-strict $survivors 1 -sibling 'SCRIPT (E): native fault' -proofVersion 3
 Run survivors-native-resource-strict $survivors 1 -sibling 'RESOURCES (E): Missing texture' -proofVersion 3
 # Точная source identity позволяет отличить штатный retained teardown от
 # настоящего fallback в наблюдении или чужого/stale shutdown receipt.
 $identity='faction=US numeric_slot=0 group_generation=1 trip_generation=1 lease_generation=1 vehicle_generation=1 vehicle_lifecycle_id=L entity_id=0x4000000000002E69{} rpl_id=R'
 $observed=$direct.Replace('reason=DESTINATION_REACHED previous_state=TRANSIT state=TACTICAL_DEPLOYMENT',"reason=DESTINATION_REACHED previous_state=TRANSIT state=TACTICAL_DEPLOYMENT $identity")
 $stop="[VEHICLE_STATE_CHANGED] operation_id=trip-US-0-1-1 causation_id=trip-US-0-1-1-STOP_ABORT-8 reason=COORDINATOR_STOP previous_state=TACTICAL_DEPLOYMENT state=FAILED_CLOSED $identity"
 $latch='[ACCEPTANCE_FAILURE_LATCHED] reason=COORDINATOR_STOP faction=US slot=0 group_gen=1 trip_gen=1 vehicle_lifecycle_id=L entity_id=0x4000000000002E69{} rpl_id=R operation_id=trip-US-0-1-1 causation_id=trip-US-0-1-1-STOP_ABORT-8'
 $shutdown=$observed.Replace('Game destroyed.',"$stop`n$latch`nGame destroyed.")
 Run exact-post-DONE-stop $shutdown 0 -proofVersion 3 -functionalVerdict PASS
 Run exact-post-DONE-stop-sibling $shutdown 0 -sibling $latch -proofVersion 3 -functionalVerdict PASS
 Run actual-six-ms-logger-difference ($shutdown.Replace('held_ms=12900','held_ms=12906')) 0 -proofVersion 3 -functionalVerdict PASS
 foreach($mutation in @(
  @('hold-tick-below-required','t_ms=37900 elapsed_ms=17900','t_ms=34999 elapsed_ms=17900'),
  @('hold-elapsed-below-required','t_ms=37900 elapsed_ms=17900','t_ms=37900 elapsed_ms=14999'),
  @('hold-logger-mismatch51','held_ms=12900','held_ms=12951'),
  @('stop-wrong-faction','reason=COORDINATOR_STOP faction=US','reason=COORDINATOR_STOP faction=USSR'),
  @('stop-wrong-trip','causation_id=trip-US-0-1-1-STOP_ABORT-8','causation_id=trip-US-0-1-2-STOP_ABORT-8'),
  @('stop-wrong-latch-actor','group_gen=1 trip_gen=1 vehicle_lifecycle_id=L entity_id=0x4000000000002E69','group_gen=1 trip_gen=1 vehicle_lifecycle_id=L entity_id=0x4000000000002E68'),
  @('stop-wrong-native-lease','previous_state=TACTICAL_DEPLOYMENT state=FAILED_CLOSED faction=US numeric_slot=0 group_generation=1 trip_generation=1 lease_generation=1','previous_state=TACTICAL_DEPLOYMENT state=FAILED_CLOSED faction=US numeric_slot=0 group_generation=1 trip_generation=1 lease_generation=2'),
  @('stop-wrong-native-rpl','state=FAILED_CLOSED '+$identity,'state=FAILED_CLOSED '+$identity.Replace('rpl_id=R','rpl_id=OTHER')),
  @('stop-unrelated-reason','reason=COORDINATOR_STOP','reason=NATIVE_ROUTE_FAILURE')
 )){Run $mutation[0] ($shutdown.Replace($mutation[1],$mutation[2])) 1 -proofVersion 3 -functionalVerdict FAIL}
 $preDone=$observed.Replace('[AICF][VEHICLE_SOAK_DONE]',"$stop`n$latch`n[AICF][VEHICLE_SOAK_DONE]")
 Run exact-stop-before-DONE-is-failure $preDone 1 -proofVersion 3 -functionalVerdict FAIL
 Run missing-DONE-no-stop-exemption ($shutdown.Replace('[VEHICLE_SOAK_DONE]','[NO_DONE]')) 1 -proofVersion 3 -durationVerdict FAIL -livenessVerdict FAIL -functionalVerdict FAIL
 Run post-DONE-fatal-still-full ($shutdown+"`nSCRIPT (E): fatal after DONE") 1 -proofVersion 3 -functionalVerdict PASS
 Run post-DONE-resource-still-full ($shutdown+"`nRESOURCES (E): resource after DONE") 1 -proofVersion 3 -functionalVerdict PASS
 Run post-DONE-foreign-sibling-not-hidden $shutdown 1 -sibling ($latch.Replace('rpl_id=R','rpl_id=FOREIGN')) -proofVersion 3 -functionalVerdict FAIL
 $tokens=$null;$errors=$null
 $auditAst=[Management.Automation.Language.Parser]::ParseFile((Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleSoakLog.ps1'),[ref]$tokens,[ref]$errors)
 $clockFunction=$auditAst.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Test-SoakCalmHoldClock'},$true)
 . ([scriptblock]::Create($clockFunction.Extent.Text))
 foreach($case in @(@(10071,10071,10077,10000,$true),@(10000,10000,10050,10000,$true),@(9999,10000,10000,10000,$false),@(10000,9999,10000,10000,$false),@(10071,10071,10122,10000,$false),@(10071,10071,20000,10000,$false))){
  if((Test-SoakCalmHoldClock $case[0] $case[1] $case[2] $case[3]) -ne $case[4]){throw 'Actual extracted calm clock predicate failed'}
  $script:checks++
 }
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 if(!$resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-soak-contract-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Vehicle soak log contracts: synthetic full30min US/USSR, raw EntityID normalization, independent duration/liveness and functional/native rejection; controls=$checks"
exit 0
