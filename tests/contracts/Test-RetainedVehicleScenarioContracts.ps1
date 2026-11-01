param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleScenarioProbe.c'))
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleScenarios.ps1'))
function Body($text,$method){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $method}
function Rules($s,$r){
 $current=Body $s 'AICF_VSRetainedCurrent';$foot=Body $s 'AICF_VSRetainedFoot';$exact=Body $s 'AICF_VSExact'
 foreach($part in @('trip != m_VSRetainedTrip','trip.IsTerminal()','!trip.IsVehiclePresetCurrent()','trip.GetLease() != m_VSRetainedLease','m_VSRetainedLease.GetVehicle() != m_VSVehicle','m_VSVehicle.GetID() != m_VSVehicleId','GetLeaseGeneration() != m_iVSRetainedLeaseGeneration','GetVehicleGeneration() != m_iVSRetainedVehicleGeneration','MatchesEntityIdentity(m_VSVehicle, m_VSVehicleId, m_sVSRetainedRpl)','ComponentRpl(m_VSVehicle) == m_sVSRetainedRpl','AICF_VSOwnRecoveryRoster(slot)','trip.IsVehiclePresetDebited()','GetSpawnedCount(m_VSFaction.GetFactionKey()) == 1')){if(!$current.Contains($part)){'CURRENT_ORIGINAL_PAID_ASSET';break}}
 foreach($part in @('AICF_VSOwnRecoveryRoster(slot)','m_VSGroup.GetCurrentWaypoint() != slot.GetWaypoint()','HasMeaningfulTask(slot, m_VSVehicles.GetSlotView(slot))','queue.Contains(slot.GetWaypoint())','access.GetCompartment()','actor.IsInVehicle()','CompartmentAccessComponent.GetVehicleIn(actor)','access.IsGettingIn()','access.IsGettingOut()','access.IsSwitchingSeatsAnim()','m_VSVehicle.CoordToLocal(actor.GetOrigin())','minimum[0] - 1','maximum[2] + 1')){if(!$foot.Contains($part)){'ACTUAL_ALL_FIVE_OUTSIDE_HULL_WITH_NATIVE_ORDER';break}}
 foreach($part in @('CollectCaptured(m_VSVehicle, m_VSRecipe, seats)','GetCompartment() != seats[seat]','IsGettingIn()','IsSwitchingSeatsAnim()','GetOccupant() != member')){if(!$exact.Contains($part)){'EXACT_PHYSICAL_REBOARDING';break}}
 foreach($part in @('aicfVehicleScenarioRetainedBehavior','version=4 policy=RETAINED_TRANSPORT return_distance_m=300 same_asset=1','m_VSRecipe.m_iBehavior = AICF_EVehicleBehavior.TRANSPORT','m_VSRecipe.m_iReturnDistance = 300','now - m_iVSStarted > 1200000','else if (trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE)','trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT || !AICF_VSRetainedFoot(slot)','returnDistance > m_VSRecipe.m_iReturnDistance && m_iVSGoalRevision != previousIntent','m_iVSStep = 5','vector.DistanceXZ(m_vVSStart, m_VSVehicle.GetOrigin()) >= 100 && m_iVSMoving >= 5 && m_iVSRecoveries == 0','if (farthest > 35) return;')){if(!$s.Contains($part)){'EXPLICIT_BRANCH_ORIGINAL_PHYSICAL_BOUNDS';break}}
 $return=$s.Substring($s.IndexOf('if (m_bVSRetained)', $s.IndexOf('m_iVSLeg = 2;')));$return=$return.Substring(0,$return.IndexOf('m_sVSPreviousTrip ='))
 if($return.Contains('RequestExplicitVehicleAdmission') -or $return -match 'Teleport\(|SetOrigin\(|Set.*Deadline\(|CommitTransition\('){'NO_NEW_ADMISSION_OR_FAKE_MOVEMENT_CLOCK'}
 if(!$r.Contains('[switch]$RetainedBehavior') -or !$r.Contains("'RetainedBehavior requires isolated non-combat repeat5 only'") -or !$r.Contains("'-aicfVehicleScenarioRetainedBehavior','1'") -or !$r.Contains('-RetainedBehavior:$taskRetained') -or !$r.Contains('& $taskAuditor -LogPath') -or !$r.Contains("'Frozen scenario auditor changed'")){'RUNNER_EXPLICIT_OPT_IN'}
}
$bad=@(Rules $source $runner);if($bad.Count){throw "Retained source contract failed: $bad"}
$mutants=@('trip != m_VSRetainedTrip','trip.GetLease() != m_VSRetainedLease','m_VSVehicle.GetID() != m_VSVehicleId','GetLeaseGeneration() != m_iVSRetainedLeaseGeneration','ComponentRpl(m_VSVehicle) == m_sVSRetainedRpl','GetSpawnedCount(m_VSFaction.GetFactionKey()) == 1','queue.Contains(slot.GetWaypoint())','HasMeaningfulTask(slot, m_VSVehicles.GetSlotView(slot))','access.IsGettingOut()','maximum[2] + 1','GetCompartment() != seats[seat]','version=4 policy=RETAINED_TRANSPORT','now - m_iVSStarted > 1200000','else if (trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE)','returnDistance > m_VSRecipe.m_iReturnDistance && m_iVSGoalRevision != previousIntent')
foreach($part in $mutants){if(!$source.Contains($part)){throw "Missing source mutant $part"};if(!@(Rules ($source.Replace($part,'false')) $runner).Count){throw "Escaped source mutant $part"}}
$work=Join-Path $RepositoryRoot ('.codex-runtime/retained-scenario-contract-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$script:checks=0
function Fixture([string]$faction,[switch]$Hierarchy){
 $args=@('-gproj','p','-profile','p','-addonsDir','a','-addons','c','-aicfVehicleScenario','repeat5','-aicfVehicleScenarioFaction',$faction,'-aicfVehicleScenarioRetainedBehavior','1')
 if($Hierarchy){$args+=@('-aicfVehicleScenarioProtectionScope','HIERARCHY')}
 $model='Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et';if($faction -eq 'US'){$model='Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et'}
 $op='trip-'+$faction+'-0-1-1'
 $lines=@(('CLI Params: '+($args -join ' ')),'[ROSTER_READY]','[VEHICLE_SCENARIO_ENV] scenario=repeat5 combat=0 combat_mode=HOLD_FIRE damage_protection=1 truce=1','[VEHICLE_SCENARIO_POLICY] scenario=repeat5 version=4 policy=RETAINED_TRANSPORT return_distance_m=300 same_asset=1',"[VEHICLE_SCENARIO_SELECTION] scenario=repeat5 faction=$faction actual_faction=$faction prefab=$model native_seats=5 native_cost=475",'[VEHICLE_SCENARIO_COHESION] scenario=repeat5 group=G formation=Column displacement=0 native_readback=1 identity_current=1 setup_parity_only=1 causal_foot_fix_proven=0',"[VEHICLE_PRESET_PURCHASE] run=native t_ms=10 operation=$op prefab=$model supplies=475", "[VEHICLE_SPAWNED] faction=$faction operation_id=$op prefab=$model")
 $lines+="[VEHICLE_STATE_CHANGED] operation_id=$op faction=$faction numeric_slot=0 group_generation=1 trip_generation=1 lease_generation=1 vehicle_generation=1 entity_id=V rpl_id=R vehicle_lifecycle_id=L previous_state=BOARDING state=TRANSIT"
 foreach($rule in @('GROUP_CREATED','INITIAL_COHESION_POLICY','EXACT_RECIPE','INITIAL_ORDER','WAITING_MOUNTED','REMOTE_ORDER','PURCHASE_LIMIT')){$lines+="[VEHICLE_SCENARIO_CHECK] scenario=repeat5 leg=1 rule=$rule passed=1"}
 foreach($rule in @('REPEAT_ORDER','DISTANT_RETURN_INTENT','SAME_ASSET_REBOARD','SINGLE_PAID_ASSET')){$lines+="[VEHICLE_SCENARIO_CHECK] scenario=repeat5 leg=2 rule=$rule passed=1"}
 $lines+='[VEHICLE_SCENARIO_TRUCE_PAIR] first=US second=USSR first_friendly=1 second_friendly=1 native_readback=1'
 $lines+='[VEHICLE_SCENARIO_TRUCE_PAIR] first=US second=FIA first_friendly=1 second_friendly=1 native_readback=1'
 $lines+='[VEHICLE_SCENARIO_TRUCE_PAIR] first=USSR second=FIA first_friendly=1 second_friendly=1 native_readback=1'
 $lines+='[VEHICLE_SCENARIO_TRUCE_SUMMARY] factions=3 pairs=3 expected_pairs=3 unavailable_pairs=0 hostile_pairs=0 classification=CONTROLLED_RETAINED_CYCLE_ONLY native_readback=1'
 if($Hierarchy){
  $lines+='[VEHICLE_SCENARIO_PROTECTION_POLICY] scope=HIERARCHY classification=CONTROLLED_RETAINED_CYCLE_ONLY mode=BEFORE_MOVEMENT_ONCE'
  foreach($position in 0..4){$lines+="[VEHICLE_SCENARIO_PROTECTION] event=APPLIED kind=MEMBER position=$position entity=M$position scope=HIERARCHY classification=CONTROLLED_RETAINED_CYCLE_ONLY protection_errors=0 protection_zones=10 protection_managers_covered=1 protection_child_managers=0 protection_enabled_after=0 protection_child_zones=0 protection_child_zones_enabled=0 read_only=1 protection_status=COVERED protection_reason=NONE"}
  $lines+='[VEHICLE_SCENARIO_PROTECTION] event=APPLIED kind=VEHICLE position=-1 entity=V scope=HIERARCHY classification=CONTROLLED_RETAINED_CYCLE_ONLY protection_errors=0 protection_zones=50 protection_managers_covered=5 protection_child_managers=4 protection_enabled_after=0 protection_child_zones=44 protection_child_zones_enabled=0 read_only=1 protection_status=COVERED protection_reason=NONE'
 }
 foreach($leg in 1..2){
  $target='<500,0,0>';if($leg -eq 2){$target='<1000,0,0>';$lines+='[VEHICLE_SCENARIO_RETAINED_RETURN] distance_m=400 threshold_m=300 previous_intent=1 intent_revision=2 target=<1000,0,0>'}
  $lines+="[VEHICLE_SCENARIO_GOAL] scenario=repeat5 leg=$leg target=$target intent_revision=$leg vehicle_start=<0,0,0> dismount_radius_m=150"
  $lines+="[VEHICLE_SCENARIO_BOARDING] scenario=repeat5 leg=$leg operation_id=$op vehicle=V group=G crew=5"
  $lines+="[VEHICLE_SCENARIO_RETAINED_IDENTITY] leg=$leg operation_id=$op vehicle=V vehicle_rpl=R group=G lease_generation=1 vehicle_generation=1 trip_generation=1"
  foreach($position in 0..4){$lines+="[VEHICLE_SCENARIO_RETAINED_SEAT] leg=$leg position=$position seat=0:$position`:0 member=M$position occupant=M$position"}
  $lines+="[VEHICLE_STATE_CHANGED] operation_id=$op reason=DESTINATION_REACHED previous_state=TRANSIT state=TACTICAL_DEPLOYMENT"
  $lines+="[VEHICLE_SCENARIO_ARRIVAL] scenario=repeat5 leg=$leg vehicle=V distance_m=145 displacement_m=355 moving_polls=12 intent_revision=$leg goal=$target"
  $lines+="[VEHICLE_SCENARIO_RETAINED_DISMOUNT] leg=$leg logical_occupants=0 transitions=0 inside_bounds=0 actual_order=1 same_asset=1"
  $lines+="[VEHICLE_SCENARIO_FOOT_ARRIVAL] scenario=repeat5 leg=$leg farthest_m=15 alive=5 goal=$target"
  foreach($rule in @('EXACT_BOARDING','GATE_RETURNS_COMPLETE','ARRIVAL_AT_ORIGINAL_GOAL','REAL_TRAVEL','NORMAL_DISMOUNT','FOOT_ORDER_RESTORED','ALL_MEMBERS_AT_POINT')){$lines+="[VEHICLE_SCENARIO_CHECK] scenario=repeat5 leg=$leg rule=$rule passed=1"}
 }
 $lines+='[GROUP_TRACE] group=G target_pos=<1000,0,0>'
 foreach($position in 0..4){$lines+="[MEMBER_TRACE] group=G position=$position selected_action=Move:RUNNING executed_action=Move:RUNNING"}
 $lines+="[VEHICLE_SCENARIO_RETAINED_FINAL] operation_id=$op spawned=1 paid=1 same_asset=1"
 $lines+='[VEHICLE_SCENARIO_DONE] scenario=repeat5 checks=25 failures=0 legs=2';$lines+='Game destroyed.'
 return [pscustomobject]@{Body=($lines -join "`n");Args=$args}
}
function CurrentFixture([string]$scenario){
 $data=Fixture USSR
 $start=$data.Body.IndexOf('[VEHICLE_SCENARIO_RETAINED_RETURN]');$end=$data.Body.IndexOf('[GROUP_TRACE]')
 $body=$data.Body.Remove($start,$end-$start)
 $body=([regex]::Replace($body,'(?m)^.*rule=(?:REPEAT_ORDER|DISTANT_RETURN_INTENT|SAME_ASSET_REBOARD|SINGLE_PAID_ASSET).*\r?\n','')).Replace('scenario=repeat5','scenario='+$scenario).Replace('version=4 policy=RETAINED_TRANSPORT','version=3 policy=CURRENT_RETAINED').Replace('legs=2','legs=1')
 $crew=5;if($scenario -eq 'armed3'){$crew=3};if($scenario -eq 'solo1'){$crew=1}
 $body=($body -split "`n"|Where-Object{$_ -notmatch '(?:RETAINED_SEAT\] leg=1|MEMBER_TRACE\] group=G) position=(\d+)' -or [int]$Matches[1] -lt $crew}) -join "`n"
 $body=$body.Replace('crew=5','crew='+$crew).Replace('alive=5','alive='+$crew)

 $original=@();foreach($position in 0..($crew-1)){$original+="[VEHICLE_SCENARIO_ROSTER] position=$position member=M$position group=G original_capture=1"}
 $body=$body.Replace('[VEHICLE_SCENARIO_BOARDING]',(($original -join "`n")+"`n"+'[VEHICLE_SCENARIO_BOARDING]'))
 if($scenario -eq 'retarget5'){
  $body=$body.Replace('[VEHICLE_SCENARIO_ARRIVAL]',"[VEHICLE_SCENARIO_CHECK] scenario=retarget5 leg=1 rule=FAR_RETARGET passed=1`n[VEHICLE_SCENARIO_GOAL] scenario=retarget5 leg=1 target=<750,0,0> intent_revision=2 vehicle_start=<0,0,0> dismount_radius_m=150`n[VEHICLE_SCENARIO_ARRIVAL]")
  $body=$body.Replace('moving_polls=12 intent_revision=1 goal=<500,0,0>','moving_polls=12 intent_revision=2 goal=<750,0,0>').Replace('alive=5 goal=<500,0,0>','alive=5 goal=<750,0,0>')
 }
 $body=$body.Replace('[VEHICLE_SCENARIO_RETAINED_FINAL]',('[VEHICLE_SCENARIO_CHECK] scenario='+$scenario+' leg=1 rule=SINGLE_PAID_ASSET passed=1'+"`n"+'[VEHICLE_SCENARIO_RETAINED_FINAL]'))
 $count=@([regex]::Matches($body,'\[VEHICLE_SCENARIO_CHECK\]')).Count;$body=$body.Replace('checks=25','checks='+$count)
 $argv=@($data.Args);$argv[[array]::IndexOf($argv,'-aicfVehicleScenario')+1]=$scenario;$argv[[array]::IndexOf($argv,'-aicfVehicleScenarioRetainedBehavior')]='-aicfVehicleScenarioCurrentBehavior'
 $body=[regex]::Replace($body,'(?m)^CLI Params:[^\r\n]*',('CLI Params: '+($argv -join ' ')))
 return [pscustomobject]@{Body=$body;Args=$argv}
}
function Run($name,$data,$expected,[string]$faction='USSR',[bool]$optIn=$true,[string]$sibling='',[string]$native='0',[string]$protection='ROOT_ONLY',[string]$policy='LEGACY_COMPLETE',[string]$scenario='repeat5'){
 [IO.File]::WriteAllText((Join-Path $work 'console.log'),$data.Body);[IO.File]::WriteAllText((Join-Path $work 'script.log'),$sibling)
 [IO.File]::WriteAllText((Join-Path $work 'launcher.txt'),('AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$data.Args}|ConvertTo-Json -Compress)))
 [IO.File]::WriteAllText((Join-Path $work 'native-exit.txt'),$native)
 $output=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleScenarioLog.ps1') -LogPath (Join-Path $work 'console.log') -LauncherPath (Join-Path $work 'launcher.txt') -NativeExitPath (Join-Path $work 'native-exit.txt') -Scenario $scenario -Faction $faction -RetainedBehavior:$optIn -ReceiptPolicy $policy -ProtectionScope $protection
 if($LASTEXITCODE -ne $expected){throw "$name unexpected exit $LASTEXITCODE expected $expected output=$output"};$script:checks++
}
try{
 foreach($faction in @('US','USSR')){Run ($faction+'-retained') (Fixture $faction) 0 $faction}
 $good=Fixture USSR
 $historical=[regex]::Replace($good.Body.Replace('version=4','version=2'),'(?m)^.*\[VEHICLE_SCENARIO_TRUCE_(?:PAIR|SUMMARY)\].*\r?\n','')
 Run historical-v2-repeat-still-strict ([pscustomobject]@{Body=$historical;Args=$good.Args}) 0
 $mutations=@(
  @('version','version=4','version=1'),@('duplicate-policy','[VEHICLE_SCENARIO_SELECTION]',"[VEHICLE_SCENARIO_POLICY] scenario=repeat5 version=1 policy=RETAINED_TRANSPORT return_distance_m=300 same_asset=1`n[VEHICLE_SCENARIO_SELECTION]"),@('threshold','threshold_m=300','threshold_m=301'),@('no-distant','distance_m=400','distance_m=300'),@('same-intent','previous_intent=1','previous_intent=2'),
  @('lease','leg=2 operation_id=trip-USSR-0-1-1 vehicle=V vehicle_rpl=R group=G lease_generation=1','leg=2 operation_id=trip-USSR-0-1-1 vehicle=V vehicle_rpl=R group=G lease_generation=2'),
  @('rpl','leg=2 operation_id=trip-USSR-0-1-1 vehicle=V vehicle_rpl=R','leg=2 operation_id=trip-USSR-0-1-1 vehicle=V vehicle_rpl=OTHER'),
  @('body','leg=2 position=4 seat=0:4:0 member=M4 occupant=M4','leg=2 position=4 seat=0:4:0 member=OTHER occupant=OTHER'),
  @('occupant','leg=2 position=4 seat=0:4:0 member=M4 occupant=M4','leg=2 position=4 seat=0:4:0 member=M4 occupant=M3'),
  @('duplicate-seat','leg=2 position=4 seat=0:4:0','leg=2 position=4 seat=0:3:0'),@('duplicate-position','leg=2 position=4','leg=2 position=3'),
  @('no-movement','displacement_m=355','displacement_m=99'),@('polls','moving_polls=12','moving_polls=4'),@('too-far-foot','farthest_m=15','farthest_m=35.01'),
  @('in-vehicle','logical_occupants=0','logical_occupants=1'),@('hull-ghost','inside_bounds=0','inside_bounds=1'),@('no-order','actual_order=1','actual_order=0'),
  @('no-tactical','state=TACTICAL_DEPLOYMENT','state=COMPLETE'),@('extra-spawn','spawned=1','spawned=2'),@('not-paid','paid=1','paid=0'),
  @('wrong-goal','goal=<1000,0,0>','goal=<999,0,0>'),@('leg2-missing','FOOT_ARRIVAL] scenario=repeat5 leg=2','FOOT_ARRIVAL] scenario=repeat5 leg=9'),
  @('missing-check','rule=SAME_ASSET_REBOARD passed=1','rule=SAME_ASSET_REBOARD passed=0'),@('two-debits','[GROUP_TRACE]',"[VEHICLE_PRESET_PURCHASE] operation=trip-USSR-0-1-1 prefab=Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et supplies=475`n[GROUP_TRACE]"),
  @('native-complete','[GROUP_TRACE]',"[VEHICLE_STATE_CHANGED] operation_id=trip-USSR-0-1-1 previous_state=HANDOFF state=COMPLETE`n[GROUP_TRACE]"),
  @('gate-unreturned','[VEHICLE_SCENARIO_ARRIVAL] scenario=repeat5 leg=2',"[VEHICLE_SCENARIO_GATE] scenario=repeat5 leg=2 event=STARTED member=M1 seat=0:1:0 elapsed_ms=0`n[VEHICLE_SCENARIO_ARRIVAL] scenario=repeat5 leg=2")
 )
 foreach($m in $mutations){if(!$good.Body.Contains($m[1])){throw "Missing log mutant $($m[0])"};Run $m[0] ([pscustomobject]@{Body=$good.Body.Replace($m[1],$m[2]);Args=$good.Args}) 1}
 Run legacy-caller-cannot-use-v2 $good 1 -optIn $false
 Run fatal-peer $good 1 -sibling 'SCRIPT (E): failure'
 Run native-nonzero $good 1 -native 1
 $badArgs=@($good.Args);$badArgs[-1]='0';Run native-flag-value ([pscustomobject]@{Body=$good.Body;Args=$badArgs}) 1
 $noFlag=@($good.Args[0..($good.Args.Count-3)]);Run missing-native-optin ([pscustomobject]@{Body=$good.Body;Args=$noFlag}) 1
 $protected=Fixture USSR -Hierarchy
 Run hierarchy-actual-coverage $protected 0 -protection HIERARCHY
 Run hierarchy-cannot-pass-root-only $protected 1
 foreach($m in @(@('enabled-child','protection_child_zones_enabled=0','protection_child_zones_enabled=1'),@('enabled-manager','protection_enabled_after=0','protection_enabled_after=1'),@('missing-body','position=4 entity=M4','position=4 entity=FOREIGN'),@('wrong-car','position=-1 entity=V','position=-1 entity=OTHER'),@('partial','protection_status=COVERED','protection_status=REJECTED'),@('zero-zones','protection_zones=50','protection_zones=0'),@('no-child','protection_child_zones=44','protection_child_zones=0'),@('wrong-count','protection_child_managers=4','protection_child_managers=3'),@('classification','classification=CONTROLLED_RETAINED_CYCLE_ONLY','classification=NORMAL_WAR'),@('late-mutation','mode=BEFORE_MOVEMENT_ONCE','mode=REAPPLIED'),@('not-readonly','read_only=1','read_only=0'))){Run $m[0] ([pscustomobject]@{Body=$protected.Body.Replace($m[1],$m[2]);Args=$protected.Args}) 1 -protection HIERARCHY}
 foreach($scenario in @('uaz5','ural5','armed3','solo1','retarget5')){
  $current=CurrentFixture $scenario
  Run ('current-'+$scenario) $current 0 -optIn $false -policy CURRENT_RETAINED -scenario $scenario
  Run ('legacy-cannot-read-current-'+$scenario) $current 1 -optIn $false -scenario $scenario
 }
 $current=CurrentFixture uaz5
 foreach($m in @(@('current-version','version=3','version=2'),@('current-requires-arrival','farthest_m=15','farthest_m=36'),@('current-requires-source-arrival','reason=DESTINATION_REACHED','reason=LOCAL_THREAT'),@('current-requires-native-order','actual_order=1','actual_order=0'),@('current-requires-original-body','member=M4 occupant=M4','member=FOREIGN occupant=FOREIGN'),@('current-requires-travel','displacement_m=355','displacement_m=99'))){Run $m[0] ([pscustomobject]@{Body=$current.Body.Replace($m[1],$m[2]);Args=$current.Args}) 1 -optIn $false -policy CURRENT_RETAINED -scenario uaz5}
 $interrupted=$current.Body.Replace('[VEHICLE_SCENARIO_ARRIVAL]',"[VEHICLE_STATE_CHANGED] operation_id=trip-USSR-0-1-1 reason=LOCAL_THREAT previous_state=TRANSIT state=TACTICAL_DEPLOYMENT`n[VEHICLE_SCENARIO_ARRIVAL]")
 Run current-interruption-does-not-replace-real-arrival ([pscustomobject]@{Body=$interrupted;Args=$current.Args}) 0 -optIn $false -policy CURRENT_RETAINED -scenario uaz5
 foreach($m in @(@('truce-partial','first=US second=FIA','first=US second=UNKNOWN'),@('truce-hostile','first_friendly=1','first_friendly=0'),@('truce-unknown','unavailable_pairs=0','unavailable_pairs=1'),@('truce-duplicate','first=USSR second=FIA','first=US second=FIA'),@('truce-summary','expected_pairs=3','expected_pairs=2'))){Run $m[0] ([pscustomobject]@{Body=$good.Body.Replace($m[1],$m[2]);Args=$good.Args}) 1}
 $teardownState='[VEHICLE_STATE_CHANGED] operation_id=trip-USSR-0-1-1 faction=USSR numeric_slot=0 reason=COORDINATOR_STOP group_generation=1 trip_generation=1 lease_generation=1 vehicle_generation=1 entity_id=V rpl_id=R vehicle_lifecycle_id=L causation_id=trip-USSR-0-1-1-STOP_ABORT-12 previous_state=TACTICAL_DEPLOYMENT state=FAILED_CLOSED'
 $teardownLatch='[ACCEPTANCE_FAILURE_LATCHED] operation_id=trip-USSR-0-1-1 faction=USSR slot=0 reason=COORDINATOR_STOP domain=IDENTITY group_gen=1 trip_gen=1 entity_id=V rpl_id=R vehicle_lifecycle_id=L causation_id=trip-USSR-0-1-1-STOP_ABORT-12 count=1 first_failure=1'
 $cleanup=$teardownState+"`n"+$teardownLatch+"`n"
 $closed=$good.Body.Replace('[VEHICLE_SCENARIO_DONE]',($cleanup+'[VEHICLE_SCENARIO_DONE]'))
 Run exact-final-then-paired-native-stop ([pscustomobject]@{Body=$closed;Args=$good.Args}) 0
 Run early-stop-cannot-be-cleanup ([pscustomobject]@{Body=$good.Body.Replace('[VEHICLE_SCENARIO_RETAINED_FINAL]',($cleanup+'[VEHICLE_SCENARIO_RETAINED_FINAL]'));Args=$good.Args}) 1
 foreach($m in @(@('stop-rpl','rpl_id=R','rpl_id=OTHER'),@('stop-entity','entity_id=V','entity_id=OTHER'),@('stop-lease-generation','lease_generation=1','lease_generation=2'),@('stop-vehicle-generation','vehicle_generation=1','vehicle_generation=2'),@('stop-trip-generation','trip_generation=1','trip_generation=2'),@('stop-group-generation','group_generation=1','group_generation=2'),@('stop-wrong-op','operation_id=trip-USSR-0-1-1','operation_id=trip-USSR-0-1-2'),@('stop-other-reason','reason=COORDINATOR_STOP','reason=REAL_FAILURE'),@('stop-missing-latch','[ACCEPTANCE_FAILURE_LATCHED]','[MISSING_LATCH]'),@('stop-missing-state','[VEHICLE_STATE_CHANGED]','[MISSING_STATE]'),@('stop-wrong-cause','STOP_ABORT-12','OTHER-12'),@('stop-missing-id','entity_id=V','missing_id=V'))){Run $m[0] ([pscustomobject]@{Body=$good.Body.Replace('[VEHICLE_SCENARIO_DONE]',($cleanup.Replace($m[1],$m[2])+'[VEHICLE_SCENARIO_DONE]'));Args=$good.Args}) 1}
 Run stop-missing-final ([pscustomobject]@{Body=$closed.Replace('[VEHICLE_SCENARIO_RETAINED_FINAL]','[MISSING_FINAL]');Args=$good.Args}) 1
 Run stop-fatal-after-final ([pscustomobject]@{Body=$closed;Args=$good.Args}) 1 -sibling 'SCRIPT (E): fatal after FINAL'
 Run stop-after-done-is-not-VSFinish ([pscustomobject]@{Body=$good.Body.Replace('Game destroyed.',($cleanup+'Game destroyed.'));Args=$good.Args}) 1
 Run stop-late-foreign-failure ([pscustomobject]@{Body=$closed.Replace('[VEHICLE_SCENARIO_DONE]',($teardownLatch.Replace('operation_id=trip-USSR-0-1-1','operation_id=trip-USSR-1-1-1')+"`n[VEHICLE_SCENARIO_DONE]"));Args=$good.Args}) 1
 "PASS Retained scenario: $($mutants.Count) source mutants +$checks explicit policy/native identity/physical sequence/debit/legacy-negative auditor controls; native proof REQUIRED separately"
}finally{
 $resolved=[IO.Path]::GetFullPath($work);$allowed=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot '.codex-runtime'))+[IO.Path]::DirectorySeparatorChar
 if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe contract cleanup target'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
