param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-scenario-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$script:checks=0
function Fixture([string]$scenario,[switch]$BudgetProbe){
 $legs=1;$crew=5
 if($scenario -eq 'repeat5'){$legs=2}
 if($scenario -eq 'armed3'){$crew=3}
 if($scenario -eq 'solo1'){$crew=1}
 $argv=@('-gproj','source','-profile','profile','-addonsDir','addons','-addons','CORE,MAP','-aicfVehicleScenario',$scenario)
 if($BudgetProbe){$argv+=@('-aicfVehicleScenarioDismountBudget','1')}
 [IO.File]::WriteAllText("$work/launcher.txt",'AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress))
 $body='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"
 $body+="[VEHICLE_SCENARIO_ENV] scenario=$scenario combat=0 combat_mode=HOLD_FIRE damage_protection=1 truce=1`n"
 $rules=@('GROUP_CREATED','EXACT_RECIPE','INITIAL_ORDER','WAITING_MOUNTED','REMOTE_ORDER')
 foreach($rule in $rules){$body+="[VEHICLE_SCENARIO_CHECK] scenario=$scenario leg=1 rule=$rule passed=1`n"}
 if($scenario -eq 'retarget5'){$body+="[VEHICLE_SCENARIO_CHECK] scenario=$scenario leg=1 rule=FAR_RETARGET passed=1`n"}
 if($legs -eq 2){foreach($rule in @('REPEAT_ORDER','EXPLICIT_READMISSION')){$body+="[VEHICLE_SCENARIO_CHECK] scenario=$scenario leg=2 rule=$rule passed=1`n"}}
 for($leg=1;$leg -le $legs;$leg++){
  $prefix="scenario=$scenario leg=$leg"
  $body+="[VEHICLE_SCENARIO_GOAL] $prefix target=<500,0,0> intent_revision=1 vehicle_start=<0,0,0> dismount_radius_m=150`n"
  if($scenario -eq 'retarget5'){$body+="[VEHICLE_SCENARIO_GOAL] $prefix target=<500,0,0> intent_revision=2 vehicle_start=<0,0,0> dismount_radius_m=150`n"}
  $revision=1;if($scenario -eq 'retarget5'){$revision=2}
  for($i=0;$i -lt $crew;$i++){$body+="[VEHICLE_PRESET_OCCUPANT] operation=O$leg revision=1 position=$i seat=0:$i`:0 member=M$i occupant=M$i`n"}
  $body+="[VEHICLE_SCENARIO_BOARDING] $prefix operation_id=O$leg vehicle=V$leg group=G crew=$crew`n"
  $body+="[GROUP_TRACE] group=G target_pos=<500,0,0>`n"
  for($i=0;$i -lt $crew;$i++){$body+="[MEMBER_TRACE] group=G position=$i selected_action=Drive:RUNNING executed_action=Drive:RUNNING`n"}
  $body+="[VEHICLE_SCENARIO_ARRIVAL] $prefix vehicle=V$leg distance_m=145 displacement_m=355 moving_polls=12 intent_revision=$revision goal=<500,0,0>`n"
  if($BudgetProbe){
   foreach($attempt in 1..3){$body+="[VEHICLE_SCENARIO_DISMOUNT_BUDGET] scenario=ural5 operation_id=O1 event=FORCE_BATCH normal_deadline_ms=20000 terminal_deadline_ms=40000 charged=1 force_exit_attempts=$attempt native_calls=1 accepted_calls=1 logical_before=5 physical_relocation_attempts=0`n"}
   $body+="[VEHICLE_SCENARIO_DISMOUNT_BUDGET] scenario=ural5 operation_id=O1 event=PHYSICAL_GHOST normal_deadline_ms=20000 terminal_deadline_ms=40000 force_exit_attempts=3 logical_occupants=0 transitions=0 inside_bounds=1 exact_ghost=1 native_calls=5 physical_relocation_attempts=0`n"
   $body+="[VEHICLE_SCENARIO_DISMOUNT_BUDGET] scenario=ural5 operation_id=O1 event=NORMAL_RELOCATION normal_deadline_ms=20000 terminal_deadline_ms=40000 force_exit_attempts=3 physical_relocation_attempts=1 ghost_displacement_m=20 ghost_moved_clear=1 unchanged_deadlines=1 terminal_only=0 native_calls=5`n"
  }
  $telemetry='';if($BudgetProbe){$telemetry=' force_attempts=4 force_exit_attempts=3 physical_relocation_attempts=1 maximum_force_exit_attempts=3 maximum_physical_relocation_attempts=3 maximum_combined_batches_per_phase=6'}
  $body+="[DISEMBARK_COMPLETE] operation_id=O$leg logical_occupants=0 transitions=0 inside_bounds=0$telemetry`n"
  $body+="[VEHICLE_STATE_CHANGED] operation_id=O$leg previous_state=HANDOFF state=COMPLETE`n"
  $body+="[VEHICLE_SCENARIO_FOOT_ARRIVAL] $prefix farthest_m=15 alive=$crew goal=<500,0,0>`n"
  foreach($rule in @('EXACT_BOARDING','GATE_RETURNS_COMPLETE','ARRIVAL_AT_ORIGINAL_GOAL','REAL_TRAVEL','NORMAL_DISMOUNT','FOOT_ORDER_RESTORED','ALL_MEMBERS_AT_POINT')){$body+="[VEHICLE_SCENARIO_CHECK] $prefix rule=$rule passed=1`n"}
 }
 $body+="[VEHICLE_SCENARIO_CHECK] scenario=$scenario leg=$legs rule=PURCHASE_LIMIT passed=1`n"
 if($BudgetProbe){$body+="[VEHICLE_SCENARIO_CHECK] scenario=$scenario leg=1 rule=DISMOUNT_OPERATION_BUDGET_CAUSAL passed=1`n"}
 $count=@([regex]::Matches($body,'\[VEHICLE_SCENARIO_CHECK\]')).Count
 $body+="[VEHICLE_SCENARIO_DONE] scenario=$scenario checks=$count failures=0 legs=$legs`nGame destroyed."
 return $body
}
function Run([string]$name,[string]$body,[int]$expected,[string]$scenario='uaz5',[string]$nativeExit='0',[string]$sibling='',[switch]$BudgetProbe){
 [IO.File]::WriteAllText("$work/console.log",$body)
 [IO.File]::WriteAllText("$work/script.log",$sibling)
 [IO.File]::WriteAllText("$work/exit.txt",$nativeExit)
 $result=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleScenarioLog.ps1') -LogPath "$work/console.log" -LauncherPath "$work/launcher.txt" -NativeExitPath "$work/exit.txt" -Scenario $scenario -DismountBudgetProbe:$BudgetProbe
 if($LASTEXITCODE -ne $expected){throw "$name unexpected verdict: $result"}
 $script:checks++
}
try{
 foreach($scenario in @('uaz5','ural5','armed3','solo1','retarget5','repeat5')){Run $scenario (Fixture $scenario) 0 $scenario}
 $good=Fixture 'uaz5'
 Run native-diagnostic-prefix ($good.Replace('[VEHICLE_PRESET_OCCUPANT] operation=','[VEHICLE_PRESET_OCCUPANT] run=stage1-server-9 t_ms=123 operation=')) 0
 $gate="[VEHICLE_SCENARIO_GATE] scenario=uaz5 leg=1 event=STARTED member=M2 seat=0:2:0 elapsed_ms=0`n"
 $returned="[VEHICLE_SCENARIO_GATE] scenario=uaz5 leg=1 event=RETURNED member=M2 seat=0:2:0 elapsed_ms=10000`n"
 $withGate=$good.Replace('[VEHICLE_SCENARIO_ARRIVAL]',($gate+$returned+'[VEHICLE_SCENARIO_ARRIVAL]'))
 Run bounded-gate-return $withGate 0
 Run missing-gate-return ($withGate.Replace($returned,'')) 1
 Run stale-gate-return ($withGate.Replace('event=RETURNED member=M2','event=RETURNED member=M1')) 1
 Run expired-gate-return ($withGate.Replace('elapsed_ms=10000','elapsed_ms=60001')) 1
 foreach($rule in @('GROUP_CREATED','EXACT_RECIPE','INITIAL_ORDER','WAITING_MOUNTED','REMOTE_ORDER','EXACT_BOARDING','ARRIVAL_AT_ORIGINAL_GOAL','REAL_TRAVEL','NORMAL_DISMOUNT','FOOT_ORDER_RESTORED','ALL_MEMBERS_AT_POINT','PURCHASE_LIMIT')){Run $rule ($good.Replace("rule=$rule passed=1","rule=$rule passed=0")) 1}
 foreach($mutation in @(
  @('short-drive','displacement_m=355','displacement_m=25'),
  @('far-away','distance_m=145','distance_m=500'),
  @('no-velocity','moving_polls=12','moving_polls=0'),
  @('wrong-vehicle','ARRIVAL] scenario=uaz5 leg=1 vehicle=V1','ARRIVAL] scenario=uaz5 leg=1 vehicle=V2'),
  @('near-retarget','goal=<500,0,0>','goal=<25,0,0>'),
  @('revision-changed','intent_revision=1 goal=','intent_revision=2 goal='),
  @('foot-incomplete','farthest_m=15','farthest_m=150'),
  @('member-dead','alive=5','alive=4'),
  @('wrong-occupant','member=M3 occupant=M3','member=M3 occupant=M2'),
  @('missing-seat','position=4 seat=','MISSING=4 seat='),
  @('missing-exit','logical_occupants=0','logical_occupants=1'),
  @('no-complete','previous_state=HANDOFF state=COMPLETE','previous_state=DISMOUNT state=FALLBACK'),
  @('no-group-trace','[GROUP_TRACE]','[NO_GROUP_TRACE]'),
  @('no-member-trace','[MEMBER_TRACE]','[NO_MEMBER_TRACE]'),
  @('not-stopped','Game destroyed.',''),
  @('no-roster','[ROSTER_READY]','[NO_ROSTER]'),
  @('undeclared-environment','combat=0 combat_mode=HOLD_FIRE','combat=1 combat_mode=FIRE_AT_WILL')
 )){Run $mutation[0] ($good.Replace($mutation[1],$mutation[2])) 1}
 Run native-failure $good 1 'uaz5' '1'
 Run sibling-exception $good 1 'uaz5' '0' 'SCRIPT (E): Virtual Machine Exception'
 Run missing-reaction-metadata $good 1 'uaz5' '0' "WORLD (E): Unknown class 'SCR_AIGoalReaction_GetOutVehicle'"
 Run latched-failure ($good+"`n[ACCEPTANCE_FAILURE_LATCHED] reason=TIMEOUT") 1
 $repeat=Fixture 'repeat5'
 Run repeat-missing-second-arrival ($repeat.Replace('FOOT_ARRIVAL] scenario=repeat5 leg=2','FOOT_ARRIVAL] scenario=repeat5 leg=9')) 1 'repeat5'
 $budget=Fixture 'ural5' -BudgetProbe
 Run split-operation-budget $budget 0 'ural5' -BudgetProbe
 foreach($mutation in @(
  @('counter-priming-no-native-call','native_calls=1','native_calls=0'),
  @('no-accepted-native-force','accepted_calls=1','accepted_calls=0'),
  @('not-three-real-charges','charged=1','charged=0'),
  @('missing-ghost','event=PHYSICAL_GHOST','event=MISSING_GHOST'),
  @('still-logically-inside','force_exit_attempts=3 logical_occupants=0','force_exit_attempts=3 logical_occupants=1'),
  @('still-transitioning','force_exit_attempts=3 logical_occupants=0 transitions=0','force_exit_attempts=3 logical_occupants=0 transitions=1'),
  @('not-hull-blocker','transitions=0 inside_bounds=1 exact_ghost=1','transitions=0 inside_bounds=0 exact_ghost=0'),
  @('no-normal-relocation','event=NORMAL_RELOCATION','event=MISSING_RELOCATION'),
  @('deadline-rearmed','event=NORMAL_RELOCATION normal_deadline_ms=20000','event=NORMAL_RELOCATION normal_deadline_ms=30000'),
  @('physical-not-moved','ghost_displacement_m=20 ghost_moved_clear=1','ghost_displacement_m=0 ghost_moved_clear=0'),
  @('only-terminal-recovery','terminal_only=0','terminal_only=1'),
  @('exit-over-bound','force_attempts=4 force_exit_attempts=3','force_attempts=5 force_exit_attempts=4'),
  @('physical-over-bound','force_attempts=4 force_exit_attempts=3 physical_relocation_attempts=1','force_attempts=7 force_exit_attempts=3 physical_relocation_attempts=4'),
  @('aggregate-mismatch','force_attempts=4 force_exit_attempts=3','force_attempts=3 force_exit_attempts=3'),
  @('undisclosed-total','maximum_combined_batches_per_phase=6','maximum_combined_batches_per_phase=3'),
  @('lost-causal-check','rule=DISMOUNT_OPERATION_BUDGET_CAUSAL','rule=MISSING_CAUSAL_CHECK')
 )){Run $mutation[0] ($budget.Replace($mutation[1],$mutation[2])) 1 'ural5' -BudgetProbe}
}
finally{
 $resolved=[IO.Path]::GetFullPath($work)
 if(!$resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-scenario-contract-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Vehicle scenario log contracts checks=$checks"
