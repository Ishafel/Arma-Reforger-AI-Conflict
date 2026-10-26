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
function Run([string]$name,[string]$body,[int]$expected,[string]$faction='US',[string]$sibling='',[double]$wall=1830,[string]$nativeExit='0',[string]$durationVerdict='PASS',[string]$livenessVerdict='PASS'){
 [IO.File]::WriteAllText((Join-Path $logs 'console.log'),$body)
 [IO.File]::WriteAllText((Join-Path $logs 'script.log'),$sibling)
 [IO.File]::WriteAllText((Join-Path $work 'native-exit.txt'),$nativeExit)
 $started=[DateTimeOffset]::Parse('2026-10-26T16:00:00Z')
 @{started_utc=$started.ToString('o');ended_utc=$started.AddSeconds($wall).ToString('o');wall_seconds=$wall;faction=$faction;requested_seconds=1800}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'timing.json')
 $result=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleSoakLog.ps1') -LogPath (Join-Path $logs 'console.log') -EvidenceRoot $work -Faction $faction -MinimumSeconds 1800
 if($LASTEXITCODE -ne $expected){throw "$name unexpected verdict: $result"}
 $summary=Get-Content -LiteralPath (Join-Path $work 'audit-summary.json') -Raw|ConvertFrom-Json
 if($summary.gates.duration.verdict -ne $durationVerdict -or $summary.gates.liveness.verdict -ne $livenessVerdict){throw "$name lost separate duration/liveness verdict: $($summary|ConvertTo-Json -Depth 6 -Compress)"}
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
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 if(!$resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-soak-contract-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Vehicle soak log contracts: synthetic full30min US/USSR, raw EntityID normalization, independent duration/liveness and functional/native rejection; controls=$checks"
exit 0
