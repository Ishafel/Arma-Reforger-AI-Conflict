param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-scenario-contract-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$script:checks=0
function Fixture([string]$scenario,[switch]$BudgetProbe,[switch]$Adapter,[string]$Faction=''){
 $legs=1;$crew=5
 if($scenario -eq 'repeat5'){$legs=2}
 if($scenario -eq 'armed3'){$crew=3}
 if($scenario -eq 'solo1'){$crew=1}
 $argv=@('-gproj','source','-profile','profile','-addonsDir','addons','-addons','CORE,MAP','-aicfVehicleScenario',$scenario)
 if($BudgetProbe){$argv+=@('-aicfVehicleScenarioDismountBudget','1')}
 if($Adapter){$argv+=@('-aicfVehicleScenarioDismountBudgetAdapter','1')}
 if($Faction){$argv+=@('-aicfVehicleScenarioFaction',$Faction)}
 [IO.File]::WriteAllText("$work/launcher.txt",'AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress))
 $body='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"
 $body+="[VEHICLE_SCENARIO_ENV] scenario=$scenario combat=0 combat_mode=HOLD_FIRE damage_protection=1 truce=1`n"
 $rules=@('GROUP_CREATED','INITIAL_COHESION_POLICY','EXACT_RECIPE','INITIAL_ORDER','WAITING_MOUNTED','REMOTE_ORDER')
 $body+="[VEHICLE_SCENARIO_COHESION] scenario=$scenario group=G formation=Column displacement=0 native_readback=1 identity_current=1 setup_parity_only=1 causal_foot_fix_proven=0`n"
 $model='Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et'
 if($Faction){$body+="[VEHICLE_SCENARIO_SELECTION] scenario=$scenario faction=$Faction actual_faction=$Faction prefab=$model native_seats=5 native_cost=475`n"}
 foreach($rule in $rules){$body+="[VEHICLE_SCENARIO_CHECK] scenario=$scenario leg=1 rule=$rule passed=1`n"}
 if($scenario -eq 'retarget5'){$body+="[VEHICLE_SCENARIO_CHECK] scenario=$scenario leg=1 rule=FAR_RETARGET passed=1`n"}
 if($legs -eq 2){foreach($rule in @('REPEAT_ORDER','EXPLICIT_READMISSION')){$body+="[VEHICLE_SCENARIO_CHECK] scenario=$scenario leg=2 rule=$rule passed=1`n"}}
 for($leg=1;$leg -le $legs;$leg++){
  $prefix="scenario=$scenario leg=$leg"
  if($Faction){$body+="[VEHICLE_PRESET_PURCHASE] run=native t_ms=100 operation=O$leg prefab=$model supplies=475`n[VEHICLE_SPAWNED] faction=$Faction operation_id=O$leg prefab=$model`n"}
  $body+="[VEHICLE_SCENARIO_GOAL] $prefix target=<500,0,0> intent_revision=1 vehicle_start=<0,0,0> dismount_radius_m=150`n"
  if($scenario -eq 'retarget5'){$body+="[VEHICLE_SCENARIO_GOAL] $prefix target=<500,0,0> intent_revision=2 vehicle_start=<0,0,0> dismount_radius_m=150`n"}
  $revision=1;if($scenario -eq 'retarget5'){$revision=2}
  for($i=0;$i -lt $crew;$i++){$body+="[VEHICLE_PRESET_OCCUPANT] operation=O$leg revision=1 position=$i seat=0:$i`:0 member=M$i occupant=M$i`n"}
  $body+="[VEHICLE_SCENARIO_BOARDING] $prefix operation_id=O$leg vehicle=V$leg group=G crew=$crew`n"
  $body+="[GROUP_TRACE] group=G target_pos=<500,0,0>`n"
  for($i=0;$i -lt $crew;$i++){$body+="[MEMBER_TRACE] group=G position=$i selected_action=Drive:RUNNING executed_action=Drive:RUNNING`n"}
  $body+="[VEHICLE_SCENARIO_ARRIVAL] $prefix vehicle=V$leg distance_m=145 displacement_m=355 moving_polls=12 intent_revision=$revision goal=<500,0,0>`n"
  if($BudgetProbe){
   if($Adapter){$body+="[VEHICLE_SCENARIO_DISMOUNT_BUDGET] scenario=ural5 operation_id=O1 event=ARRIVAL_ADAPTER normal_deadline_ms=0 terminal_deadline_ms=0 classification=CONTROLLED_DISMOUNT_BRANCH_ONLY source_reason=DESTINATION_REACHED native_arrival=1 route_generation=1 target_distance_m=145 dismount_radius_m=150 direct_phase_mutation=0`n"}
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
 if($Faction){foreach($leg in 1..$legs){$body=$body.Replace("operation=O$leg ","operation=trip-$Faction-0-1-$leg ").Replace("operation_id=O$leg ","operation_id=trip-$Faction-0-1-$leg ")}}
 return $body
}
function Run([string]$name,[string]$body,[int]$expected,[string]$scenario='uaz5',[string]$nativeExit='0',[string]$sibling='',[switch]$BudgetProbe,[string]$Faction='USSR'){
 [IO.File]::WriteAllText("$work/console.log",$body)
 [IO.File]::WriteAllText("$work/script.log",$sibling)
 [IO.File]::WriteAllText("$work/exit.txt",$nativeExit)
 $result=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleScenarioLog.ps1') -LogPath "$work/console.log" -LauncherPath "$work/launcher.txt" -NativeExitPath "$work/exit.txt" -Scenario $scenario -Faction $Faction -DismountBudgetProbe:$BudgetProbe
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
 foreach($rule in @('GROUP_CREATED','INITIAL_COHESION_POLICY','EXACT_RECIPE','INITIAL_ORDER','WAITING_MOUNTED','REMOTE_ORDER','EXACT_BOARDING','ARRIVAL_AT_ORIGINAL_GOAL','REAL_TRAVEL','NORMAL_DISMOUNT','FOOT_ORDER_RESTORED','ALL_MEMBERS_AT_POINT','PURCHASE_LIMIT')){Run $rule ($good.Replace("rule=$rule passed=1","rule=$rule passed=0")) 1}
 Run wrong-initial-formation ($good.Replace('formation=Column','formation=Wedge')) 1
 Run missing-initial-formation-readback ($good.Replace('native_readback=1','native_readback=0')) 1
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
 $adapted=Fixture 'ural5' -BudgetProbe -Adapter
 Run controlled-actual-arrival-adapter $adapted 0 'ural5' -BudgetProbe
 foreach($mutation in @(@('adapter-source-reason','source_reason=DESTINATION_REACHED','source_reason=OTHER'),@('adapter-no-route','route_generation=1','route_generation=0'),@('adapter-not-arrived','target_distance_m=145','target_distance_m=151'),@('adapter-wrong-branch','classification=CONTROLLED_DISMOUNT_BRANCH_ONLY','classification=NORMAL_MANUAL'),@('adapter-phase-mutation','direct_phase_mutation=0','direct_phase_mutation=1'),@('adapter-missing-proof','native_arrival=1','native_arrival=0'),@('adapter-missing-receipt','event=ARRIVAL_ADAPTER','event=OTHER'))){Run $mutation[0] ($adapted.Replace($mutation[1],$mutation[2])) 1 'ural5' -BudgetProbe}
 $us=Fixture 'repeat5' -Faction US
 Run US-repeat-native-model-and-two-debits $us 0 'repeat5' -Faction US
 foreach($mutation in @(
  @('foreign-faction','faction=US ','faction=USSR '),
  @('actual-root-faction','actual_faction=US ','actual_faction=USSR '),
  @('wrong-model','M1025_armed_M2HB.et','UAZ469.et'),
  @('wrong-native-capacity','native_seats=5','native_seats=4'),
  @('zero-native-cost','native_cost=475','native_cost=0'),
  @('wrong-operation-faction','trip-US-0-1-2','trip-USSR-0-1-2'),
  @('missing-second-debit','operation=trip-US-0-1-2 prefab=','operation=UNRELATED prefab='),
  @('wrong-debit','supplies=475','supplies=474'),
  @('wrong-cli-faction','-aicfVehicleScenarioFaction US','-aicfVehicleScenarioFaction USSR'),
  @('one-leg-only','FOOT_ARRIVAL] scenario=repeat5 leg=2','FOOT_ARRIVAL] scenario=repeat5 leg=9'),
  @('same-asset-second-leg','vehicle=V2','vehicle=V1'),
  @('changed-group-second-leg','operation_id=trip-US-0-1-2 vehicle=V2 group=G','operation_id=trip-US-0-1-2 vehicle=V2 group=FOREIGN'),
  @('typed-unknown-is-not-complete','previous_state=HANDOFF state=COMPLETE','reason=SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN previous_state=APPROACHING_SITE state=FALLBACK')
 )){Run $mutation[0] ($us.Replace($mutation[1],$mutation[2])) 1 'repeat5' -Faction US}
 $usChangedRoster=$us.Replace('operation=trip-US-0-1-2 revision=1 position=0 seat=0:0:0 member=M0 occupant=M0','operation=trip-US-0-1-2 revision=1 position=0 seat=0:0:0 member=OTHER occupant=OTHER')
 Run US-repeat-original-member-changed $usChangedRoster 1 'repeat5' -Faction US
 $ussr=Fixture 'repeat5' -Faction USSR
 Run explicit-USSR-compatibility $ussr 0 'repeat5'
}
finally{
 $resolved=[IO.Path]::GetFullPath($work)
 if(!$resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-scenario-contract-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
# Execute the actual fixture owner-routing guards and dispatch body. The fake
# owner only observes delegation and models completion; native durability itself
# is still checked by runtime ORDER_BINDING_STABLE, never simulated as PASS here.
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$scenarioSource=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleScenarioProbe.c'))
$damageSource=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_TransportDamageFallbackProbe.c'))
function ScenarioBody([string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$scenarioSource;Code=(ConvertTo-AICFCodeText $scenarioSource)}) $name}
$rosterBody=ScenarioBody 'AICF_VSOwnRecoveryRoster';$guardBody=ScenarioBody 'AICF_VSCanServiceRecovery';$serviceBody=ScenarioBody 'AICF_VSServiceOwnRecovery'
if(!$rosterBody -or !$guardBody -or !$serviceBody){throw 'Scenario own-state recovery service missing'}
$cohesionBody=ScenarioBody 'AICF_VSApplyInitialCohesion'
function CohesionRules([string]$body){
 foreach($needle in @('!Replication.IsServer()','!slot.IsCombatReady()','slot.GetGroup() != m_VSGroup',
  'm_VSGroup.GetID() != m_VSGroupId','!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_VSGroup)',
  'm_GroupCohesionPolicy.Apply(m_VSGroup)','selected.GetName() == AICF_GroupCohesionPolicy.FORMATION_NAME',
  'defaultFormation.GetName() == AICF_GroupCohesionPolicy.FORMATION_NAME','movement.GetFormationDisplacement() == 0',
  'slot.GetGroup() == m_VSGroup && m_VSGroup.GetID() == m_VSGroupId','return readback && current;')){
  if(!$body.Contains($needle)){'INITIAL_COHESION_EXACT_READBACK';return}
 }
}
if(@(CohesionRules $cohesionBody).Count){throw 'Missing production-parity initial cohesion identity/readback'}
foreach($needle in @('m_VSGroup.GetID() != m_VSGroupId','m_GroupCohesionPolicy.Apply(m_VSGroup)',
 'selected.GetName() == AICF_GroupCohesionPolicy.FORMATION_NAME',
 'defaultFormation.GetName() == AICF_GroupCohesionPolicy.FORMATION_NAME','movement.GetFormationDisplacement() == 0','return readback && current;')){
 if('INITIAL_COHESION_EXACT_READBACK' -notin @(CohesionRules ($cohesionBody.Replace($needle,'true')))){throw "Initial cohesion mutation survived: $needle"}
 $script:checks++
}
if(([regex]::Matches($serviceBody,'ProcessPendingOrderRecovery\(m_VSState, slot, m_VSFaction\)')).Count -ne 1 -or
 (ConvertTo-AICFCodeText $serviceBody) -match 'ClearPending|RecordPending|BeginOrderRecovery|SetOrigin|Teleport|AddAction'){throw 'Scenario must delegate once without pending/counter priming'}
if((ConvertTo-AICFCodeText $damageSource) -match 'ProcessPendingOrderRecovery\('){throw 'Damage observer duplicates recovery ownership'}
$scenarioUpdate=ScenarioBody 'Update'
if($scenarioUpdate.IndexOf('AICF_VSServiceOwnRecovery(slot);') -gt $scenarioUpdate.IndexOf('AICF_VSUpdateVehicles();') -or
 $scenarioUpdate.IndexOf('AICF_VSUpdateVehicles();') -gt $scenarioUpdate.IndexOf('if (!trip || trip.GetOperationId() == m_sVSPreviousTrip) return;')){throw 'Own-state service must run before dispatch, including a missing trip'}
function AdaptScenarioBody([string]$body){
 foreach($name in @('m_VSState','slot','m_VSGroup','character','agent','m_VSVehicles')){
  $body=[regex]::Replace($body,'!'+$name+'(?=\s*[|)])',($name+' == null'))
 }
 $body=$body.Replace('!character.GetAIControlComponent()','character.GetAIControlComponent() == null')
 $body=$body.Replace('for (int position;','for (int position = 0;')
 return $body
}
$adapter=@'
using System;
using System.Collections.Generic;
public static class AICF_ScenarioString{public static bool IsEmpty(this string value){return String.IsNullOrEmpty(value);}}
public class AICF_ScenarioArray<T>{public List<T> Values=new List<T>();public int Count(){return Values.Count;}public T this[int i]{get{return Values[i];}}}
public static class Replication{public static bool Server=true;public static bool IsServer(){return Server;}}
public class IEntity{public int Id;public bool Alive=true,Authority=true;public int GetID(){return Id;}}
public class AIAgent{public IEntity Entity;public SCR_AIGroup Parent;public IEntity GetControlledEntity(){return Entity;}public SCR_AIGroup GetParentGroup(){return Parent;}}
public class AICF_ScenarioControl{public AIAgent Agent;public AIAgent GetControlAIAgent(){return Agent;}}
public class ChimeraCharacter:IEntity{public AICF_ScenarioControl Control=new AICF_ScenarioControl();public AICF_ScenarioControl GetAIControlComponent(){return Control;}public static ChimeraCharacter Cast(IEntity entity){return entity as ChimeraCharacter;}}
public class AIWaypoint:IEntity{}
public class SCR_AIGroup:IEntity{public object Faction;public int Inside;public AIWaypoint Waypoint;public object GetFaction(){return Faction;}public AIWaypoint GetCurrentWaypoint(){return Waypoint;}}
public class AICF_GroupSlot{public bool Ready=true,Pending=true,Current=true,Reliability,Stuck;public string Cause="VEHICLE_HANDOFF";public int StableMs=11000,Polls=4;public SCR_AIGroup Group;public AIWaypoint Waypoint;public List<IEntity> Members=new List<IEntity>();public bool IsCombatReady(){return Ready;}public SCR_AIGroup GetGroup(){return Group;}public IEntity GetRosterMember(int i){return Members[i];}public bool HasPendingOrderRecovery(){return Pending;}public bool IsPendingOrderRecoveryContextCurrent(){return Current;}public string GetPendingOrderRecoveryCause(){return Cause;}public bool PendingOrderRecoveryCountsAsReliabilityAttempt(){return Reliability;}public bool PendingOrderRecoveryCountsAsStuckRecovery(){return Stuck;}public AIWaypoint GetPendingOrderRecoveryWaypoint(){return Waypoint;}public int GetPendingOrderRecoveryStableElapsedMs(){return StableMs;}public int GetPendingOrderRecoveryStablePolls(){return Polls;}public AIWaypoint GetWaypoint(){return Waypoint;}}
public class AICF_FactionState{public AICF_GroupSlot Slot;public AICF_GroupSlot GetSlot(int i){return Slot;}}
public class AICF_VehicleCoordinator{public bool Controls,Restores;public bool IsControllingMovement(AICF_GroupSlot slot){return Controls;}public bool IsRestorePending(AICF_GroupSlot slot){return Restores;}}
public static class AICF_GroupRuntime{public static bool IsAliveCharacter(IEntity member){return member!=null&&member.Alive;}public static int CountAliveAgentsInAnyVehicle(SCR_AIGroup group){return group.Inside;}}
public static class AICF_VehicleBoardingMutationFence{public static bool IsAuthoritativeReplicatedEntity(IEntity entity){return entity!=null&&entity.Authority;}public static bool IsAuthoritativeAIEntity(IEntity entity){return entity!=null&&entity.Authority;}}
public static class AICF_GroupTrace{public static string Entity(IEntity entity){return entity.Id.ToString();}}
public class AICF_ScenarioConfig{public int GetReliabilityIntervalMs(){return 5000;}}
public class AICF_ScenarioPlanner{public string Failure="";public string GetOrderFailureReason(AICF_GroupSlot slot,object faction){return Failure;}}
public class AICF_ScenarioOwnerAdapter{
 public AICF_FactionState m_VSState;public SCR_AIGroup m_VSGroup;public int m_VSGroupId=55,m_iVSSize=2,m_iVSLeg=1;public string m_sVSCase="repeat5";public object m_VSFaction;public AICF_VehicleCoordinator m_VSVehicles=new AICF_VehicleCoordinator();public AICF_ScenarioArray<int> m_aVSMembers=new AICF_ScenarioArray<int>();public AICF_ScenarioConfig m_Stage2Config=new AICF_ScenarioConfig();public AICF_ScenarioPlanner m_OrderPlanner=new AICF_ScenarioPlanner();
 public const int ORDER_RECOVERY_MIN_STABLE_MS=10000,ORDER_RECOVERY_STABLE_INTERVALS=2,ORDER_RECOVERY_STABLE_POLLS=3;
 public int Calls,Verified;public bool OwnerClears=true,OwnerChangesRoster;
 public bool AICF_VSOwnRecoveryRoster(AICF_GroupSlot slot) ROSTER
 public bool AICF_VSCanServiceRecovery(AICF_GroupSlot slot) GUARD
 public void AICF_VSServiceOwnRecovery(AICF_GroupSlot slot) SERVICE
 public void ProcessPendingOrderRecovery(AICF_FactionState state,AICF_GroupSlot slot,object faction){Calls++;if(OwnerClears)slot.Pending=false;if(OwnerChangesRoster)slot.Members[0].Id=999;}
 public void AICF_VSOnRecoveryVerified(AICF_GroupSlot slot,AIWaypoint waypoint,int ms,int polls,int required){Verified++;}
 public void Print(string message){}
 public static int[] Evaluate(int scenario){
  Replication.Server=true;var owner=new AICF_ScenarioOwnerAdapter();owner.m_VSFaction=new object();owner.m_VSGroup=new SCR_AIGroup(){Id=55,Faction=owner.m_VSFaction};var waypoint=new AIWaypoint(){Id=70};owner.m_VSGroup.Waypoint=waypoint;var slot=new AICF_GroupSlot(){Group=owner.m_VSGroup,Waypoint=waypoint};owner.m_VSState=new AICF_FactionState(){Slot=slot};
  for(int i=0;i<2;i++){var member=new ChimeraCharacter(){Id=100+i};member.Control.Agent=new AIAgent(){Entity=member,Parent=owner.m_VSGroup};slot.Members.Add(member);owner.m_aVSMembers.Values.Add(member.Id);}
  switch(scenario){
   case 1:Replication.Server=false;break;case 2:owner.m_VSState.Slot=new AICF_GroupSlot();break;case 3:slot.Ready=false;break;
   case 4:owner.m_VSGroup.Id=56;break;case 5:slot.Group=new SCR_AIGroup();break;case 6:owner.m_VSGroup.Faction=new object();break;case 7:owner.m_VSGroup.Authority=false;break;
   case 8:owner.m_aVSMembers.Values.RemoveAt(1);break;case 9:slot.Members[0].Alive=false;break;case 10:slot.Members[0].Id=999;break;case 11:slot.Members[0].Authority=false;break;
   case 12:((ChimeraCharacter)slot.Members[0]).Control=null;break;case 13:((ChimeraCharacter)slot.Members[0]).Control.Agent=null;break;
   case 14:((ChimeraCharacter)slot.Members[0]).Control.Agent.Entity=new IEntity();break;case 15:((ChimeraCharacter)slot.Members[0]).Control.Agent.Parent=new SCR_AIGroup();break;
   case 16:slot.Pending=false;break;case 17:slot.Current=false;break;case 18:slot.Cause="OTHER";break;case 19:slot.Reliability=true;break;case 20:slot.Stuck=true;break;
   case 21:owner.m_VSVehicles.Controls=true;break;case 22:owner.m_VSVehicles.Restores=true;break;case 23:owner.m_VSGroup.Inside=1;break;case 24:owner.m_VSVehicles=null;break;
   case 25:owner.m_VSGroup=null;break;case 26:owner.m_VSState=null;break;case 27:slot=null;break;case 28:slot.Members[0]=null;break;
   case 29:owner.OwnerClears=false;break;case 30:owner.OwnerChangesRoster=true;break;case 31:slot.StableMs=9999;break;case 32:slot.Polls=2;break;
   case 33:owner.m_VSGroup.Waypoint=new AIWaypoint();break;case 34:owner.m_OrderPlanner.Failure="WAYPOINT_MISSING";break;
  }
  bool own=owner.AICF_VSOwnRecoveryRoster(slot),eligible=owner.AICF_VSCanServiceRecovery(slot);owner.AICF_VSServiceOwnRecovery(slot);
  return new int[]{own?1:0,eligible?1:0,owner.Calls,owner.Verified};
 }
}
'@
$adapter=$adapter.Replace('ROSTER',(AdaptScenarioBody $rosterBody)).Replace('GUARD',(AdaptScenarioBody $guardBody)).Replace('SERVICE',(AdaptScenarioBody $serviceBody))
if(!('AICF_ScenarioOwnerAdapter' -as [type])){Add-Type -TypeDefinition $adapter}
for($case=0;$case -le 34;$case++){
 $actual=[AICF_ScenarioOwnerAdapter]::Evaluate($case)
 $own=1;if($case -in 1..15 -or $case -in 25..28){$own=0}
 $eligible=0;$calls=0;$verified=0
 if($case -eq 0 -or $case -ge 29){$eligible=1;$calls=1}
 if($case -eq 0){$verified=1}
 if($actual[0] -ne $own -or $actual[1] -ne $eligible -or $actual[2] -ne $calls -or $actual[3] -ne $verified){throw "Actual scenario owner dispatch case=$case actual=$actual expected=$own,$eligible,$calls,$verified"}
 $script:checks++
}
"PASS Vehicle scenario log and actual owner dispatch contracts checks=$checks; ordinary/casual delegate once, foreign/stale/pending/shortdurability blocked, native runtime required"
