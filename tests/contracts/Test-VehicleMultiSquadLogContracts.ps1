param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-multi-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$script:checks=0
function Id([int]$n){return '0x400000000000'+$n.ToString('X4')}
function Marker([string]$name,[int]$elapsed,[string]$faction,[int]$slot,[string]$details){return "[AICF][VEHICLE_MULTI_$name] mode=$script:mode faction=$faction numeric_slot=$slot t_ms=$(20000+$elapsed) elapsed_ms=$elapsed $details concurrency_mode=$script:concurrency classification=$script:classification notmovingproof=$script:notMoving`n"}
function Snapshot([int]$elapsed,[int]$cohort,[bool]$complete=$false){
 $script:sample++
 $body='';$exact=$cohort;if($complete){$exact=0}
 foreach($row in $script:rows){
  $active=$row.slot -lt $cohort -and !$complete;$stage=1;$phase='NONE';$qual=0;$vehicle='NONE';$rpl='NONE';$operation='NONE';$lease=0;$leg=0;$displacement=0;$moving=0;$foot=0;$vp=0;$protected=0;$hold=0
  if($script:mode -eq 'CAPACITY'){$protected=5;$hold=1}
  if($active){$stage=3;$phase='TRANSIT';$qual=1;$vehicle=$row.vehicle;$rpl=$row.rpl;$operation=$row.operation;$lease=1;$leg=1;$displacement=30;$moving=2;if($script:mode -eq 'CAPACITY'){$vp=1}}
  if($complete){$stage=7;$phase='NONE';$foot=1}
  $wait=0;$routeGeneration=-1;$waypoint=0
  if($active){$routeGeneration=1;$waypoint=1;if($script:concurrency -eq 'MOUNTED_STAGING'){$leg=0;$displacement=0;$moving=0;$wait=1;$routeGeneration=0;$waypoint=0}}
  $body+=Marker SLOT $elapsed $row.faction $row.slot "sample_id=$script:sample stage=$stage phase=$phase group=$($row.group) generation=1 alive=5 exact_active=$qual vehicle=$vehicle rpl=$rpl operation_id=$operation lease_generation=$lease vehicle_generation=$lease protected_members=$protected vehicle_protected=$vp hold_fire=$hold route_complete=$foot position=<30,0,0> leg=$leg displacement_m=$displacement moving_polls=$moving preset_destination_wait=$wait route_generation=$routeGeneration route_waypoint_present=$waypoint"
 }
 $completed=0;if($complete){$completed=12}
 $body+=Marker HEARTBEAT $elapsed BOTH -1 "sample_id=$script:sample cohort=$cohort us_exact_active=$exact ussr_exact_active=$exact completed_groups=$completed failures=0 mustered_groups=12 sample_elapsed_ms=$elapsed"
 return $body
}
function NativeJoin($row,[int]$position,[string]$source){
 $body="[INFANTRY_RECRUIT_DEBITED] faction=$($row.faction) slot=$($row.slot) group=$($row.group) generation=1 token=$($row.token) source=$source role=ROLE$position cost=10 supplies_before=1000 supplies_after=990`n"
 $body+="[INFANTRY_RECRUIT_JOINED] faction=$($row.faction) slot=$($row.slot) group=$($row.group) generation=1 token=$($row.token) source=$source role=ROLE$position cost=10 character=$($row.members[$position]) {}`n"
 return $body
}
function Fixture([string]$mode='NORMAL',[int]$autoAdditions=0,[string]$concurrency='MOVING',[int]$durationSeconds=1800,[int]$jitDeficit=0){
 $script:mode=$mode;$script:concurrency=$concurrency;$script:duration=$durationSeconds;$script:sample=0;$script:rows=@();$script:classification='NORMAL_CAMPAIGN';$script:notMoving=0
 if($mode -eq 'CAPACITY'){$script:classification='CONTROLLED_CAPACITY_ONLY'}
 if($concurrency -eq 'MOUNTED_STAGING'){$script:classification='CAPACITY_MOUNTED_ONLY';$script:notMoving=1}
 $durationMs=$durationSeconds*1000
 $argv=@('-gproj','source','-profile',"$work/profile",'-addonsDir','addons','-addons','CORE,MAP','-aicfAICommanderMode','BOTH','-aicfVehicleMultiMode',$mode,'-aicfVehicleMultiConcurrencyMode',$concurrency,'-aicfVehicleMultiDurationMs',[string]$durationMs)
 [IO.File]::WriteAllText("$work/launcher.txt",'AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv;profile="$work/profile"}|ConvertTo-Json -Compress))
 $flag=0;$budget=0;if($mode -eq 'CAPACITY'){$flag=1;$budget=4000}
 $body='CLI Params: '+($argv -join ' ')+"`n[AICF][VEHICLE_MULTI_ENV] mode=$mode truce=$flag hold_fire=$flag damage_protection=$flag grant_budget_per_faction=$budget setup_before_roster=1 test_only=1 concurrency_mode=$concurrency classification=$script:classification notmovingproof=$script:notMoving`n[ROSTER_READY]`n"
 $body+=Marker START 0 BOTH -1 "requested_duration_ms=$durationMs slots_per_faction=6 crew=5 cohorts=2,4,6 stable_required_ms=30000 normal_campaign=$([int]($mode -eq 'NORMAL'))"
 $n=0
 foreach($faction in @('US','USSR')){for($slot=0;$slot -lt 6;$slot++){
  $prefab='{UUID}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et';if($faction -eq 'US'){$prefab='{UUID}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et'}
  $members=@(for($p=0;$p -lt 5;$p++){Id (4096+$n*16+$p)})
  $row=[pscustomobject]@{faction=$faction;slot=$slot;group=(Id (256+$n));vehicle=(Id (1536+$n));rpl="R$n";operation="trip-$faction-$slot-1-1";members=$members;prefab=$prefab;token=(1+$n)}
  $script:rows+=$row;$n++
  $body+=Marker SLOT_START 1000 $faction $slot "group=$($row.group) generation=1 initial_alive=1 initial_deployment_size=1 pre_muster_infantry=1 desired=5 initial_desired=1 configured=1 home_order=1"
 }}
 $grant=0;$jitExtraUS=0
 foreach($faction in @('US','USSR')){
  if($mode -eq 'CAPACITY'){$grant=500;$body+=Marker GRANT 10000 $faction -1 "token=$faction-G1 before=400 requested=500 applied=500 after=900 total_granted=500 budget=4000 quoted_demand=950 owner=ECONOMY_SYSTEM test_only=1"}
  $body+=Marker FUNDING 10000 $faction -1 "stock_before=400 stock_after=900 quoted_hire=0 quoted_vehicles=950 vehicle_unit_price=475 income=10 income_interval_s=5 next_income_s=1 grant=$grant total_granted=$grant budget=$budget"
 }
 foreach($cohort in @(2,4,6)){
  $first=20000+($cohort-2)*20000
  foreach($row in @($script:rows|Where-Object{$_.slot -ge $cohort-2 -and $_.slot -lt $cohort})){
   $body+=Marker ACTIVATED ($first-7000) $row.faction $row.slot "cohort=$cohort desired=5 prior_desired=1 paid_muster=1"
   $body+="[INFANTRY_RECRUITMENT_STARTED] faction=$($row.faction) slot=$($row.slot) group=$($row.group) generation=1 token=$($row.token) source=AUTO`n"
   for($p=1;$p -le $autoAdditions;$p++){$body+=NativeJoin $row $p AUTO}
   $body+="[INFANTRY_RECRUITMENT_PRIORITY_CHANGED] faction=$($row.faction) slot=$($row.slot) group=$($row.group) generation=1 token=$($row.token) source=PLAYER_RECRUIT reason=PLAYER_RECRUIT`n"
   $body+=Marker RECRUIT_REQUEST ($first-6000) $row.faction $row.slot 'accepted=1 reason=RECRUITMENT_STARTED alive=1 desired=5 authority_path=SERVER_FIXTURE_DOWNSTREAM'
   for($p=1+$autoAdditions;$p -lt 5;$p++){$body+=NativeJoin $row $p PLAYER_RECRUIT}
   $body+="[INFANTRY_RECRUITMENT_FINISHED] faction=$($row.faction) slot=$($row.slot) group=$($row.group) generation=1 token=$($row.token) source=PLAYER_RECRUIT reason=ROSTER_COMPLETE_OR_EMPTY`n"
   $body+=Marker RECRUITED ($first-5000) $row.faction $row.slot "alive=5 desired=5 initial_alive=1 group=$($row.group) generation=1"
   for($p=0;$p -lt 5;$p++){$body+=Marker MEMBER ($first-5000) $row.faction $row.slot "position=$p member=$($row.members[$p]){}"}
  }
  $body+=Marker MUSTERED ($first-4000) BOTH -1 "cohort=$cohort groups=$($cohort*2) soldiers=$($cohort*10) paid_muster=1"
  foreach($row in @($script:rows|Where-Object{$_.slot -ge $cohort-2 -and $_.slot -lt $cohort})){
   $body+=Marker VEHICLE ($first-3000) $row.faction $row.slot "prefab=$($row.prefab) seats=5 assigned=5 cost=475 revision=1 paid_muster_before_motorized=1"
   $body+=Marker TRIP ($first-2500) $row.faction $row.slot "operation_id=$($row.operation)"
   if($mode -eq 'CAPACITY'){
    $body+="[VEHICLE_SPAWN_ATTEMPT] operation_id=$($row.operation) lease_generation=1 request_generation=1 attempt=1`n"
    foreach($stage in @('PRESPAWN','PURCHASE')){
     $jitBefore=950;$jitAfter=950;$jitGrant=0;$jitTotal=$grant
     if($row.faction -eq 'US'){$jitTotal+=$jitExtraUS}
     if($row.faction -eq 'US' -and $row.slot -eq 0 -and $stage -eq 'PRESPAWN' -and $jitDeficit -gt 0){
      $jitBefore=475-$jitDeficit;$jitAfter=475;$jitGrant=$jitDeficit;$jitExtraUS+=$jitDeficit;$jitTotal=$grant+$jitExtraUS
      $body+=Marker GRANT ($first-2400) US -1 "token=MULTI-JIT-$($row.operation)-1-1-$stage before=$jitBefore requested=$jitGrant applied=$jitGrant after=$jitAfter total_granted=$jitTotal budget=4000 quoted_demand=475 owner=ECONOMY_SYSTEM test_only=1"
     }
     $body+=Marker JIT_QUOTE ($first-2400) $row.faction $row.slot "operation_id=$($row.operation) stage=$stage prefab=$($row.prefab) quoted_cost=475 token=MULTI-JIT-$($row.operation)-1-1-$stage stock_before=$jitBefore stock_after=$jitAfter obligations=0 required_stock=475 grant=$jitGrant total_granted=$jitTotal base=HQ-$($row.faction) budget=4000 native_debit=PENDING group=$($row.group) generation=1 lease_generation=1 request_generation=1 attempt=1 site_owner=site-$($row.faction)-$($row.slot)-1-1-1"
    }
   }
   $body+="[VEHICLE_PRESET_PURCHASE] operation=$($row.operation) prefab=$($row.prefab) supplies=475 base=HQ-$($row.faction)`n"
   for($p=0;$p -lt 5;$p++){
    $body+="[VEHICLE_PRESET_SEAT_REQUESTED] operation=$($row.operation) position=$p seat=0:$p`:0 member=$($row.members[$p]) {}`n"
    $low=[Convert]::ToUInt64($row.members[$p].Substring(2),16) -band 4294967295L
    $body+="[VEHICLE_PRESET_OCCUPANT] operation=$($row.operation) position=$p seat=0:$p`:0 member=$low occupant=$low`n"
   }
   $body+=Marker BOARDED ($first-2000) $row.faction $row.slot "operation_id=$($row.operation) vehicle=$($row.vehicle) {} rpl=$($row.rpl) lease_generation=1 vehicle_generation=1 seats=5 occupants=5 start=<0,0,0>"
   if($concurrency -eq 'MOVING'){$body+=Marker TARGET ($first-1000) $row.faction $row.slot "operation_id=$($row.operation) vehicle=$($row.vehicle) leg=1 position=<650,0,0> intent_revision=2 dismount_radius_m=150 final=0"}
  }
  for($time=$first;$time -le $first+30000;$time+=5000){$body+=Snapshot $time $cohort}
  $body+=Marker COHORT ($first+30000) BOTH -1 "sample_id=$script:sample cohort=$cohort us_exact_active=$cohort ussr_exact_active=$cohort stable_ms=30000 required_ms=30000 stable_started_elapsed_ms=$first sample_elapsed_ms=$($first+30000)"
 }
 foreach($row in $script:rows){
  $finalLeg=2;if($concurrency -eq 'MOUNTED_STAGING'){$finalLeg=1}
  $body+=Marker FINAL_TARGET 145000 $row.faction $row.slot "operation_id=$($row.operation) vehicle=$($row.vehicle) leg=$finalLeg position=<900,0,0> intent_revision=3 dismount_radius_m=150 final=1"
  $body+=Marker ARRIVAL 160000 $row.faction $row.slot "operation_id=$($row.operation) passed=1 vehicle=$($row.vehicle) distance_m=145 displacement_m=755 moving_polls=15 intent_revision=3 leg=$finalLeg"
  $body+="[DISEMBARK_COMPLETE] operation_id=$($row.operation) logical_occupants=0 transitions=0 inside_bounds=0`n[VEHICLE_STATE_CHANGED] operation_id=$($row.operation) previous_state=HANDOFF state=COMPLETE`n"
  $body+=Marker DISMOUNTED 161000 $row.faction $row.slot "operation_id=$($row.operation) alive=5 in_vehicle=0 normal_complete=1 intent_revision=3 position=<900,0,0>"
  $body+=Marker FOOT_ARRIVAL 165000 $row.faction $row.slot "operation_id=$($row.operation) alive=5 farthest_m=15 position=<900,0,0> intent_revision=3 leg=$finalLeg"
 }
 for($time=170000;$time -lt $durationMs;$time+=50000){$body+=Snapshot $time 8 $true}
 $body+=Marker DONE $durationMs BOTH -1 "requested_duration_ms=$durationMs duration_satisfied=1 failures=0 cohort=8 completed_groups=12 us_granted=$($grant+$jitExtraUS) ussr_granted=$grant"
 # Fixtures собирают lifecycle каждой группы отдельно. Сохраняем реальные
 # межгрупповые timestamps, включая порядок untimed production receipts.
 $tick=-1;$order=0;$timeline=@(foreach($line in ($body -split "`n")){
  if($line -match '\[VEHICLE_MULTI_\w+\][^\r\n]* t_ms=(\d+)'){$tick=[long]$Matches[1]}
  [pscustomobject]@{tick=$tick;order=$order++;line=$line}
 })
 return (($timeline|Sort-Object tick,order|ForEach-Object{$_.line}) -join "`n")+"`nGame destroyed."
}
function Run([string]$name,[string]$body,[int]$expected,[string]$mode='NORMAL',[string]$failedGate='',[string]$sibling='',[double]$wall=1830,[string]$nativeExit='0',[string]$concurrencyMode=$script:concurrency){
 [IO.File]::WriteAllText("$work/console.log",$body);[IO.File]::WriteAllText("$work/script.log",$sibling);[IO.File]::WriteAllText("$work/native-exit.txt",$nativeExit)
 $started=[DateTimeOffset]::Parse('2026-10-26T16:00:00Z');@{started_utc=$started.ToString('o');ended_utc=$started.AddSeconds($wall).ToString('o');wall_seconds=$wall;requested_seconds=$script:duration;mode=$mode;concurrency_mode=$script:concurrency}|ConvertTo-Json|Set-Content -LiteralPath "$work/timing.json"
 $result=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleMultiSquadLog.ps1') -LogPath "$work/console.log" -EvidenceRoot $work -Mode $mode -ConcurrencyMode $concurrencyMode -MinimumSeconds $script:duration
 if($LASTEXITCODE -ne $expected){throw "$name unexpected verdict: $result"}
 $summary=Get-Content -LiteralPath "$work/audit-summary.json" -Raw|ConvertFrom-Json
 if($failedGate -and $summary.gates.$failedGate.verdict -ne 'FAIL'){throw "$name expected $failedGate FAIL: $result"}
 $classification='CONTROLLED_CAPACITY_ONLY';if($concurrencyMode -eq 'MOUNTED_STAGING'){$classification='CAPACITY_MOUNTED_ONLY'}
 if($mode -eq 'CAPACITY' -and ($summary.normal_campaign_evidence -ne $false -or $summary.classification -ne $classification)){throw 'Controlled capacity mislabeled as campaign'}
 if($concurrencyMode -eq 'MOUNTED_STAGING' -and (!$summary.notmovingproof -or $summary.concurrent_moving_cohorts_proved -or $summary.normal_campaign_evidence)){throw 'Mounted-only result mislabeled as moving/campaign proof'}
 $script:checks++
}
try{
 $good=Fixture NORMAL;Run NORMAL-twelve-groups $good 0
 $capacity=Fixture CAPACITY;Run CAPACITY-explicit-funded-protected $capacity 0 CAPACITY
 $capacityJit=Fixture CAPACITY 0 MOVING 1800 10;Run CAPACITY-JIT-465-to-475-real-owner-grant $capacityJit 0 CAPACITY
 $mounted=Fixture CAPACITY 0 MOUNTED_STAGING 900;Run MOUNTED-distinct-physical-twelve-staging $mounted 0 CAPACITY -wall 930
 Run mounted-wrong-oracle $mounted 1 CAPACITY environment -concurrencyMode MOVING
 Run mounted-not-normal-campaign $mounted 1 NORMAL environment
 Run mounted-fifteen-minutes-minus-one ($mounted.Replace('t_ms=920000 elapsed_ms=900000 requested_duration_ms=900000','t_ms=919000 elapsed_ms=899000 requested_duration_ms=900000')) 1 CAPACITY duration
 Run mounted-native-resource-error $mounted 1 CAPACITY native_resources 'RESOURCES (E): Missing texture'
 Run mounted-no-native-destination-wait ($mounted.Replace('preset_destination_wait=1','preset_destination_wait=0')) 1 CAPACITY cohorts
 Run mounted-route-already-started ($mounted.Replace('route_generation=0','route_generation=1')) 1 CAPACITY cohorts
 Run mounted-route-waypoint-present ($mounted.Replace('route_waypoint_present=0','route_waypoint_present=1')) 1 CAPACITY cohorts
 Run mounted-early-leg ($mounted.Replace('leg=0 displacement_m=0','leg=1 displacement_m=0')) 1 CAPACITY cohorts
 Run mounted-fake-seated-count ($mounted.Replace('occupants=5','occupants=4')) 1 CAPACITY functional
 Run mounted-wrong-native-actor ($mounted.Replace('position=4 seat=0:4:0 member=4100 occupant=4100','position=4 seat=0:4:0 member=4099 occupant=4099')) 1 CAPACITY functional
 Run mounted-unpaid-hire ($mounted.Replace('[INFANTRY_RECRUIT_DEBITED]','[FAKE_DEBIT]')) 1 CAPACITY recruitment
 Run mounted-final-does-not-drive ($mounted.Replace('displacement_m=755','displacement_m=0')) 1 CAPACITY functional
 Run mounted-normal-complete-missing ($mounted.Replace('previous_state=HANDOFF state=COMPLETE','previous_state=DISMOUNT state=FALLBACK')) 1 CAPACITY functional
 Run mounted-foot-missing ($mounted.Replace('alive=5 farthest_m=15','alive=4 farthest_m=15')) 1 CAPACITY functional
 Run mounted-mixed-markers ($mounted.Replace('cohort=4 us_exact_active=4 ussr_exact_active=4 stable_ms=30000 required_ms=30000 stable_started_elapsed_ms=60000 sample_elapsed_ms=90000 concurrency_mode=MOUNTED_STAGING','cohort=4 us_exact_active=4 ussr_exact_active=4 stable_ms=30000 required_ms=30000 stable_started_elapsed_ms=60000 sample_elapsed_ms=90000 concurrency_mode=MOVING')) 1 CAPACITY environment
 $early=Marker TARGET 25000 US 0 'operation_id=trip-US-0-1-1 vehicle=0x4000000000000600 leg=1 position=<650,0,0> intent_revision=2 dismount_radius_m=150 final=0'
 Run mounted-early-target ($mounted.Replace('Game destroyed.',$early+'Game destroyed.')) 1 CAPACITY cohorts
 $auto=Fixture NORMAL 2;Run cohort-paid-AUTO-before-manual-muster $auto 0
 $good=Fixture NORMAL
 $offset=[regex]::Replace($good,'(t_ms=)(\d+)( elapsed_ms=)(\d+)',{param($m) $m.Groups[1].Value+([long]$m.Groups[2].Value+1).ToString()+$m.Groups[3].Value+([long]$m.Groups[4].Value+1).ToString()})
 $offset=[regex]::Replace($offset,'((?:sample_elapsed_ms|stable_started_elapsed_ms)=)(\d+)',{param($m) $m.Groups[1].Value+([long]$m.Groups[2].Value+1).ToString()});Run one-ms-offset $offset 0
 foreach($mutation in @(
  @('free-MOTO-muster','initial_deployment_size=1','initial_deployment_size=5','recruitment'),
  @('future-cohort-activated-early','cohort=4 desired=5 prior_desired=1','cohort=2 desired=5 prior_desired=1','recruitment'),
  @('all-hired-before-first-cohort','cohort=2 groups=4 soldiers=20','cohort=2 groups=12 soldiers=60','recruitment'),
  @('no-manual-recruit','accepted=1 reason=RECRUITMENT_STARTED','accepted=0 reason=NO_SAFE_BARRACKS','recruitment'),
  @('fake-hire','[INFANTRY_RECRUIT_JOINED]','[FAKE_JOINED]','recruitment'),
  @('unpaid-hire','[INFANTRY_RECRUIT_DEBITED]','[FAKE_DEBIT]','recruitment'),
  @('wrong-actor','position=4 member=0x4000000000001004{}','position=4 member=0x4000000000001003{}','recruitment'),
  @('wrong-seat-actor','position=4 seat=0:4:0 member=4100 occupant=4100','position=4 seat=0:4:0 member=4099 occupant=4099','functional'),
  @('unpaid-vehicle','supplies=475','supplies=0','funding'),
  @('wrong-model','/M1025_armed_M2HB.et','/UAZ469.et','functional'),
  @('wrong-roster-size','occupants=5','occupants=4','functional'),
  @('cached-final-revision','moving_polls=15 intent_revision=3','moving_polls=15 intent_revision=2','functional'),
  @('changed-final-target','farthest_m=15 position=<900,0,0>','farthest_m=15 position=<901,0,0>','functional'),
  @('short-drive','displacement_m=755','displacement_m=99','functional'),
  @('no-speed','moving_polls=15','moving_polls=0','functional'),
  @('far-arrival','distance_m=145','distance_m=161','functional'),
  @('fallback','previous_state=HANDOFF state=COMPLETE','previous_state=DISMOUNT state=FALLBACK','functional'),
  @('uncleared-compartment','logical_occupants=0','logical_occupants=1','functional'),
  @('foot-not-all','alive=5 farthest_m=15','alive=4 farthest_m=15','functional'),
  @('foot-too-far','farthest_m=15','farthest_m=36','functional'),
  @('CLI-count-only','exact_active=1','exact_active=0','cohorts'),
  @('old-readback-lease','lease_generation=1 vehicle_generation=1 protected_members','lease_generation=2 vehicle_generation=1 protected_members','cohorts'),
  @('unmoved-pads','displacement_m=30 moving_polls=2','displacement_m=0 moving_polls=0','cohorts'),
  @('short-stability','stable_ms=30000','stable_ms=29000','cohorts'),
  @('broken-stable-start','stable_started_elapsed_ms=20000','stable_started_elapsed_ms=21000','cohorts'),
  @('claimed-counts-without-vehicles','us_exact_active=6 ussr_exact_active=6','us_exact_active=7 ussr_exact_active=7','cohorts'),
  @('missing-slot-readbacks','[VEHICLE_MULTI_SLOT]','[NO_SLOT]','cohorts'),
  @('not-stopped','Game destroyed.','','duration'),
  @('not-real-campaign','truce=0 hold_fire=0 damage_protection=0','truce=1 hold_fire=1 damage_protection=1','environment')
 )){Run $mutation[0] ($good.Replace($mutation[1],$mutation[2])) 1 NORMAL $mutation[3]}
 Run ongoing-battle-failure ($good+"`n[ACCEPTANCE_FAILURE_LATCHED] reason=TIMEOUT") 1 NORMAL functional
 Run sibling-script-fatal $good 1 NORMAL native_fatal 'SCRIPT (E): Virtual Machine Exception'
 Run sibling-GetIn-reaction-unknown $good 1 NORMAL native_fatal "Unknown class 'SCR_AIGoalReaction_GetInVehicle'"
 Run sibling-resource-error $good 1 NORMAL native_resources 'RESOURCES (E): Missing texture'
 Run native-exit-failure $good 1 NORMAL duration '' 1830 '-1'
 Run short-wall $good 1 NORMAL duration '' 1799
 Run twenty-nine-fifty-nine ($good.Replace('t_ms=1820000 elapsed_ms=1800000 requested_duration_ms=1800000','t_ms=1819000 elapsed_ms=1799000 requested_duration_ms=1800000')) 1 NORMAL duration
 Run wrong-clock-origin ($good.Replace('t_ms=1820000 elapsed_ms=1800000','t_ms=1820001 elapsed_ms=1800000')) 1 NORMAL duration
 $noHeartbeats=[regex]::Replace($good,'(?m)^\[AICF\]\[VEHICLE_MULTI_HEARTBEAT\][^\r\n]*\r?\n','');Run no-heartbeats $noHeartbeats 1 NORMAL liveness
 $compressed=[regex]::Replace($good,'(?m)^\[AICF\]\[VEHICLE_MULTI_(?:SLOT|HEARTBEAT)\][^\r\n]*',{param($m)
  [regex]::Replace($m.Value,'t_ms=(\d+) elapsed_ms=(\d+)',{param($t)
   $elapsed=[int]$t.Groups[2].Value
   if($elapsed -ge 20000 -and $elapsed -le 50000){$elapsed=20000+[int](($elapsed-20000)/100)}
   't_ms='+[string](20000+$elapsed)+' elapsed_ms='+[string]$elapsed
  })
 });Run compressed-real-cohort-clock $compressed 1 NORMAL cohorts
 $gap=[regex]::Replace($good,'(?m)^\[AICF\]\[VEHICLE_MULTI_HEARTBEAT\][^\r\n]*elapsed_ms=(?:220000|270000)[^\r\n]*\r?\n','');Run missing-sixty-second-coverage $gap 1 NORMAL liveness
 $capacity=Fixture CAPACITY
 Run over-budget ($capacity.Replace('requested=500 applied=500 after=900 total_granted=500','requested=4500 applied=4500 after=4900 total_granted=4500')) 1 CAPACITY funding
 Run missing-grant-ledger ($capacity.Replace('[VEHICLE_MULTI_GRANT]','[NO_GRANT]')) 1 CAPACITY funding
 Run missing-JIT-scopes ($capacity.Replace('[VEHICLE_MULTI_JIT_QUOTE]','[NO_JIT_QUOTE]')) 1 CAPACITY funding
 Run JIT-missing-native-attempt ($capacity.Replace('[VEHICLE_SPAWN_ATTEMPT]','[NO_NATIVE_ATTEMPT]')) 1 CAPACITY funding
 Run JIT-stale-site-owner ($capacity.Replace('site_owner=site-US-0-1-1-1','site_owner=site-US-0-1-2-1')) 1 CAPACITY funding
 Run wrong-JIT-selected-trip ($capacity.Replace('quoted_cost=475 token=MULTI-JIT-trip-US-0-1-1','quoted_cost=1 token=MULTI-JIT-trip-US-0-1-1')) 1 CAPACITY funding
 Run wrong-JIT-native-base ($capacity.Replace('base=HQ-US budget=4000','base=ENEMY budget=4000')) 1 CAPACITY funding
 Run JIT-not-native-debit ($capacity.Replace('native_debit=PENDING','native_debit=SUCCESS_OVERRIDE')) 1 CAPACITY funding
 Run JIT-extra-grant-no-receipt ($capacity.Replace('stock_before=950 stock_after=950','stock_before=940 stock_after=950').Replace('required_stock=475 grant=0','required_stock=475 grant=10')) 1 CAPACITY funding
 Run JIT-ignores-logistics ($capacity.Replace('obligations=0 required_stock=475','obligations=20 required_stock=475')) 1 CAPACITY funding
 Run JIT-replaces-native-debit ($capacity.Replace('[VEHICLE_PRESET_PURCHASE]','[NO_NATIVE_PURCHASE]')) 1 CAPACITY funding
 Run wrong-grant-owner ($capacity.Replace('owner=ECONOMY_SYSTEM','owner=FIXTURE')) 1 CAPACITY funding
 Run missing-native-protection ($capacity.Replace('protected_members=5 vehicle_protected=1','protected_members=4 vehicle_protected=0')) 1 CAPACITY environment
 Run late-truce ($capacity.Replace('setup_before_roster=1','setup_before_roster=0')) 1 CAPACITY environment
 $good=Fixture NORMAL
 Run wrong-mode-CLI ($good.Replace('-aicfVehicleMultiMode NORMAL ','-aicfVehicleMultiMode CAPACITY ')) 1 NORMAL provenance
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 if(!$resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-multi-contract-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Multi-squad log contracts: both modes, twelve real groups, paid AUTO/manual muster, independent duration/funding/native gates and reconstructed 2/4/6 concurrency; controls=$checks"
exit 0
