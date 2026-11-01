param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
function Body($s,$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
$s=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_UnstuckPathHintProbe.c'))
$parking=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleParking.c'))
function Rules($source){
 $begin=Body $source 'AICF_UPBeginDispatchBlock';$release=Body $source 'AICF_UPReleaseDispatchBlock';$tick=Body $source 'AICF_UPBrakeTick';$callback=Body $source 'ReportVehicleTransitMoveFailure'
 foreach($g in @('aicfUnstuckBrakeDispatch','value == "1"','CONTROLLED_BRAKE_BLOCK','cycle_case_proven=0')){if(!$source.Contains($g)){'EXPLICIT_OPTIN_SCOPED_PROOF';break}}
 foreach($g in @('IsExactVehicle(trip)','HasNoForeignOrHumanUse()','m_Controller.GetPersistentHandBrake()','trip.GetBehaviorState().m_Parking','AICF_ETransportTripPhase.TRANSIT','SetPersistentHandBrake(true)','m_bOwnsBrake = m_Controller.GetPersistentHandBrake()')){if(!$begin.Contains($g)){'INITIAL_FALSE_OWN_ONLY_EXACT_HEALTHY_TRANSIT_BLOCK';break}}
 if(([regex]::Matches($begin,'m_Controller.GetPersistentHandBrake\(\)')).Count-lt3){'REPEAT_NATIVE_FALSE_BEFORE_OWN_WRITE'}
 foreach($g in @('trip.GetBehaviorState().m_Parking','!IsExactVehicle(trip) || !HasNoForeignOrHumanUse()','Stop(trip)')){if(!$release.Contains($g)){'SAFE_RELEASE_PRESERVES_FOREIGN_OR_PRODUCTION_PARKING';break}}
 foreach($g in @('AICF_UPOriginalsCurrent(trip, car)','AICF_UnstuckProbe.s_Purchases != 1','AICF_UnstuckMutationScope.Capture(trip, car)','GetMobilityRecoveryAttempts() != 2','s_Repairs != 2','GetUnstuckFailedSites().Count() != 2','!movement.IsRouteWaypointBound()','controller.GetPersistentHandBrake()','now - m_iUPDispatchPollAt < 500','distance >= 10','m_iUPDispatchMotionPolls >= 3')){if(!$tick.Contains($g)){'REAL_TWO_CORRECTIONS_OWN_ROUTE_OLD_CLOCK_THEN_NATIVE_MOTION';break}}
 foreach($g in @('candidate.AICF_UPNativeBrakeFalse(trip)','AICF_UPMoveActivityReady(trip)','nativeBefore < 10','now - m_iUPDispatchReadyPollAt < 500','m_iUPDispatchReadyPolls < 3','AICF_UPObserveDispatchBlock(trip, blockDetails)','bool blockCurrent','AICF_TransportNativeGate gate = movement.GetNativeGate()','UNSTUCK_BRAKE_OBSERVE')){if(!$tick.Contains($g)){'POST_NATIVE_MOTION_READY_AND_EXACT_BLOCK_PREDICATES';break}}
 if($callback.IndexOf('AICF_UPReleaseBrake("BEFORE_UNCHANGED_NATIVE_FAILURE_SUPER")')-gt$callback.IndexOf('return super.ReportVehicleTransitMoveFailure')){'RELEASE_BEFORE_UNCHANGED_NATIVE_FAILURE_SUPER'}
 foreach($g in @('AICF_UPReleaseBrake("FINISH_" + reason)','AICF_UPReleaseBrake("STOP")','unreleased_is_not_pass=1','BRAKE_ACTUAL_NATIVE_FAILURE_PREEMPTED_WATCHDOG_COVERAGE')){if(!$source.Contains($g)){'LIFECYCLE_OR_PREEMPTION_CANNOT_CLAIM_PASS';break}}
 if((ConvertTo-AICFCodeText ($begin+$release+$tick))-match'\b(?:SetOrigin|SetWorldTransform|SetHealth|SetVelocity|SetAngularVelocity|SetPhase|CommitTransition|BeginMobilityRecovery|ActivateAI|SetLOD|MarkProgress|CreditNativeRouteProgress)\s*\('){'NO_FAKE_STUCK_CLOCK_PATH_PHASE_HEALTH_OR_AI_WRITES'}
}
$bad=@(Rules $s);if($bad.Count){throw "Brake probe $bad"};$sourceChecks=1
foreach($g in @('trip.GetBehaviorState().m_Parking','!IsExactVehicle(trip) || !HasNoForeignOrHumanUse()','m_Controller.GetPersistentHandBrake()','AICF_UPOriginalsCurrent(trip, car)','GetMobilityRecoveryAttempts() != 2','GetUnstuckFailedSites().Count() != 2','now - m_iUPDispatchPollAt < 500','distance >= 10','m_iUPDispatchMotionPolls >= 3','AICF_UPReleaseBrake("STOP")')){if(!@(Rules $s.Replace($g,'false')).Count){throw "Brake unsafe source mutant escaped $g"};$sourceChecks++}
$shim=@'
using System;
namespace AICFBrakeActual {
public class N {public static bool operator!(N n){return ReferenceEquals(n,null);}public static implicit operator bool(N n){return !ReferenceEquals(n,null);}}
public enum AICF_ETransportTripPhase {TRANSIT,TACTICAL_DEPLOYMENT}
public class Behavior:N {public N m_Parking;}
public enum EAIActionState {RUNNING,FAILED}public class AIWaypoint:N {}public class AIAction:N {public EAIActionState State;public EAIActionState GetActionState(){return State;}}public class SCR_AIMoveActivity:AIAction {public AIWaypoint m_RelatedWaypoint;public static SCR_AIMoveActivity Cast(AIAction a){return a as SCR_AIMoveActivity;}}
public class SCR_AIGroupUtilityComponent:N {public AIAction Current,Executed;public AIAction GetCurrentAction(){return Current;}public AIAction GetExecutedAction(){return Executed;}}
public class Group:N {public SCR_AIGroupUtilityComponent Utility=new SCR_AIGroupUtilityComponent();public SCR_AIGroupUtilityComponent GetGroupUtilityComponent(){return Utility;}}
public class Assignment:N {public Group Group=new Group();public Group GetGroup(){return Group;}}
public class Movement:N {public AIWaypoint Waypoint;public bool Bound=true;public AIWaypoint GetRouteWaypoint(){return Waypoint;}public bool IsRouteWaypointBound(){return Bound;}}
public class AICF_TransportTrip:N {public Assignment Assignment=new Assignment();public Movement Movement=new Movement();public bool Valid=true,Terminal,Preset=true;public Behavior Behavior=new Behavior();public AICF_ETransportTripPhase Phase;public bool IsValid(){return Valid;}public bool IsTerminal(){return Terminal;}public bool IsVehiclePresetCurrent(){return Preset;}public AICF_ETransportTripPhase GetPhase(){return Phase;}public Behavior GetBehaviorState(){return Behavior;}public Assignment GetAssignment(){return Assignment;}public Movement GetMovementState(){return Movement;}}
public class Controller {public bool Brake;public int Writes,Reads;public bool GetPersistentHandBrake(){Reads++;return Brake;}public void SetPersistentHandBrake(bool v){Brake=v;Writes++;}}
// Controlled upstream ownership/health predicates; production ParkingContracts execute their real implementations.
public class Parking {public bool Safe=true,Foreign,Usable=true,m_bOwnsBrake,m_bStopped;public int Reads,ForeignReads;public Action<int> OnRead;public Controller m_Controller=new Controller();public bool IsExactVehicle(AICF_TransportTrip t){Reads++;if(OnRead!=null)OnRead(Reads);return Safe&&t!=null&&!m_bStopped;}public bool HasNoForeignOrHumanUse(){ForeignReads++;return !Foreign;}public bool IsUsableVehicle(){return Usable;}public void Log(string s){}
public bool Begin(AICF_TransportTrip trip) __BEGIN__
public bool Wait(AICF_TransportTrip trip) __WAIT__
public bool Ready(AICF_TransportTrip trip) __READY__
public bool Observe(AICF_TransportTrip trip,out string details) __OBSERVE__
public bool Release(AICF_TransportTrip trip) __RELEASE__
public bool AICF_UPDispatchBlockReleased(){return m_bStopped&&!m_bOwnsBrake;}
public void Stop(AICF_TransportTrip trip) __STOP__
}
public static class Run {public static int Execute(){int n=0;Action<bool,string> c=(ok,label)=>{if(!ok)throw new Exception(label);n++;};var t=new AICF_TransportTrip();var p=new Parking();c(p.Begin(t)&&p.m_Controller.Writes==1&&p.m_bOwnsBrake,"one initial false-to-true write");c(p.Release(t)&&!p.m_Controller.Brake&&p.m_Controller.Writes==2,"own safe release");c(p.Release(t)&&p.m_Controller.Writes==2,"idempotent receipt without duplicate stopped write");
 Action<Action<Parking,AICF_TransportTrip>,string> reject=(change,label)=>{var q=new Parking();var trip=new AICF_TransportTrip();change(q,trip);c(!q.Begin(trip)&&q.m_Controller.Writes==0,label);};reject((q,x)=>q.m_Controller.Brake=true,"preserve preexisting native brake");reject((q,x)=>x.Behavior.m_Parking=new N(),"preserve production parking owner");reject((q,x)=>q.Foreign=true,"human or foreign use");reject((q,x)=>q.Safe=false,"stale lease/Rpl");reject((q,x)=>q.Usable=false,"damaged/fire");reject((q,x)=>x.Phase=AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT,"not transit");reject((q,x)=>x.Preset=false,"stale preset");reject((q,x)=>q.OnRead=i=>{if(i==2)q.Safe=false;},"identity changed before write");
 p=new Parking();t=new AICF_TransportTrip();p.Begin(t);p.Foreign=true;c(!p.Release(t)&&p.m_Controller.Brake&&p.m_Controller.Writes==1&&!p.m_bStopped,"unsafe release does not mutate or consume token");p.Foreign=false;c(p.Release(t)&&!p.m_Controller.Brake,"later safe cleanup releases own write");p=new Parking();t=new AICF_TransportTrip();p.Begin(t);t.Behavior.m_Parking=new N();c(!p.Release(t)&&p.m_Controller.Brake,"production parking takeover preserves brake");
 p=new Parking();t=new AICF_TransportTrip();p.m_Controller.Brake=true;c(p.Wait(t)&&p.m_Controller.Writes==0,"initial native brake waits without clearing");p.m_Controller.Brake=false;c(!p.Wait(t)&&p.Begin(t),"only actual native false permits begin");p=new Parking();t=new AICF_TransportTrip();p.m_Controller.Brake=true;p.Foreign=true;c(!p.Wait(t)&&p.m_Controller.Writes==0,"foreign owner is not readiness wait");
 p=new Parking();t=new AICF_TransportTrip();var wp=new AIWaypoint();var move=new SCR_AIMoveActivity{m_RelatedWaypoint=wp};t.Movement.Waypoint=wp;t.Assignment.Group.Utility.Current=move;t.Assignment.Group.Utility.Executed=move;c(p.Ready(t),"same exact running current/executed move bound waypoint");move.State=EAIActionState.FAILED;c(!p.Ready(t),"failed activity not readiness");move.State=EAIActionState.RUNNING;t.Assignment.Group.Utility.Executed=new SCR_AIMoveActivity{m_RelatedWaypoint=wp};c(!p.Ready(t),"different executed move object");t.Assignment.Group.Utility.Executed=move;t.Movement.Bound=false;c(!p.Ready(t),"unbound waypoint");t.Movement.Bound=true;move.m_RelatedWaypoint=new AIWaypoint();c(!p.Ready(t),"different actual waypoint");t.Assignment.Group.Utility.Current=new AIAction();c(!p.Ready(t),"non-move current activity");
 p=new Parking();t=new AICF_TransportTrip();p.Begin(t);p.Reads=0;p.ForeignReads=0;p.m_Controller.Reads=0;string details;c(p.Observe(t,out details)&&p.Reads==1&&p.ForeignReads==1&&p.m_Controller.Reads==1,"post-block predicates each computed once");p.m_Controller.Brake=false;c(!p.Observe(t,out details)&&details.Contains("failed=NATIVE_BRAKE_FALSE"),"exact actual raw-brake failure receipt");p.Foreign=true;c(!p.Observe(t,out details)&&details.Contains("failed=FOREIGN_OR_HUMAN"),"exact foreign failure receipt");p.Foreign=false;p.m_Controller=null;c(!p.Observe(t,out details)&&details.Contains("failed=CONTROLLER_MISSING"),"missing controller no getter dereference");return n;}}
}
'@
$observe=(Body $s 'AICF_UPObserveDispatchBlock').Replace('%1','{0}').Replace('%2','{1}').Replace('%3','{2}').Replace('%4','{3}').Replace('%5','{4}').Replace('%6','{5}')
$shim=$shim.Replace('__BEGIN__',(Body $s 'AICF_UPBeginDispatchBlock')).Replace('__WAIT__',(Body $s 'AICF_UPWaitForNativeRelease')).Replace('__READY__',(Body $s 'AICF_UPMoveActivityReady')).Replace('__OBSERVE__',$observe).Replace('__RELEASE__',(Body $s 'AICF_UPReleaseDispatchBlock')).Replace('__STOP__',(Body $parking 'Stop'))
Add-Type -TypeDefinition $shim
$actual=[AICFBrakeActual.Run]::Execute()
. (Join-Path $PSScriptRoot '../lib/AICF_UnstuckBrakeLog.ps1')
$positive=@(
 '[UNSTUCK_BRAKE_PRE_NATIVE_MOTION] operation=x car=c native_distance=11 distinct_polls=1 observed_at=1000 current_executed_move=1 native_brake=0 attempts=0',
 '[UNSTUCK_BRAKE_PRE_NATIVE_MOTION] operation=x car=c native_distance=12 distinct_polls=2 observed_at=1500 current_executed_move=1 native_brake=0 attempts=0',
 '[UNSTUCK_BRAKE_PRE_NATIVE_MOTION] operation=x car=c native_distance=13 distinct_polls=3 observed_at=2000 current_executed_move=1 native_brake=0 attempts=0',
 '[UNSTUCK_BRAKE_APPLIED] operation=x car=c rpl=r deadline=1000000 attempts=0 initial_flag=0 own_write=1 fake_stuck=0',
 '[VEHICLE_UNSTUCK_ATTEMPT] operation_id=x reason=NO_PHYSICAL_MOVEMENT attempt=1 relocated=1',
 '[UNSTUCK_PROBE_CORRECTION] operation=x car=c attempt=1 actual_distance=8',
 '[VEHICLE_UNSTUCK_ATTEMPT] operation_id=x reason=NO_PHYSICAL_MOVEMENT attempt=2 relocated=1',
 '[UNSTUCK_PROBE_CORRECTION] operation=x car=c attempt=2 actual_distance=8',
 '[UNSTUCK_BRAKE_RELEASED] operation=x car=c deadline=1000000 attempts=2 failed_sites=2 persistent_brake=0 physical_relocations=2 cycle_case_proven=0',
 '[UNSTUCK_BRAKE_NATIVE_MOTION] operation=x car=c deadline=1000000 attempts=2 actual_distance=10 distinct_polls=3 health_unchanged=1 purchases=1 cycle_case_proven=0',
 '[UNSTUCK_BRAKE_RELEASE_RECEIPT] released=1 native_preempted=0',
 '[UNSTUCK_PROBE_FINISHED] passed=1 reason=CONTROLLED_BRAKE_TWO_REAL_CORRECTIONS_AND_NATIVE_10M'
)
if(!(Test-AICFUnstuckBrakeLog $positive)){throw 'Positive scoped brake log rejected'};$logCases=1
if(!(Test-AICFUnstuckBrakeLog (@('')+$positive))){throw 'Full native log blank-line adapter rejected'};$logCases++
foreach($pair in @(@('initial_flag=0','initial_flag=1'),@('own_write=1','own_write=0'),@('fake_stuck=0','fake_stuck=1'),@('attempt=2','attempt=3'),@('failed_sites=2','failed_sites=1'),@('persistent_brake=0','persistent_brake=1'),@('actual_distance=10','actual_distance=NaN'),@('actual_distance=10','actual_distance=9.99'),@('distinct_polls=3','distinct_polls=2'),@('health_unchanged=1','health_unchanged=0'),@('purchases=1','purchases=2'),@('cycle_case_proven=0','cycle_case_proven=1'),@('NO_PHYSICAL_MOVEMENT','NATIVE_UNREACHABLE'),@('released=1','released=0'),@('native_preempted=0','native_preempted=1'))){$mutated=@($positive|ForEach-Object{$_.Replace($pair[0],$pair[1])});if(Test-AICFUnstuckBrakeLog $mutated){throw "Unsafe brake log accepted $pair"};$logCases++}
for($i=0;$i-lt$positive.Count;$i++){if($positive[$i]-match'RELEASE_RECEIPT'){continue};$mutated=@();for($j=0;$j-lt$positive.Count;$j++){if($j-ne$i){$mutated+=$positive[$j]}};if(Test-AICFUnstuckBrakeLog $mutated){throw "Missing brake evidence accepted $i"};$logCases++}
foreach($value in @('100','15.001','NaN','Infinity')){if(Test-AICFUnstuckBrakeLog @($positive|ForEach-Object{$_.Replace('actual_distance=8','actual_distance='+$value)})){throw "Oversized/nonfinite actual correction accepted $value"};$logCases++}
foreach($pair in @(@(3,4),@(5,6),@(7,8),@(4,5),@(2,3))){$mutated=@($positive);$swap=$mutated[$pair[0]];$mutated[$pair[0]]=$mutated[$pair[1]];$mutated[$pair[1]]=$swap;if(Test-AICFUnstuckBrakeLog $mutated){throw "Reordered actual correction outside owned block accepted $pair"};$logCases++}
foreach($pair in @(@('native_distance=11','native_distance=9'),@('observed_at=1500','observed_at=1001'),@('current_executed_move=1','current_executed_move=0'),@('native_brake=0','native_brake=1'))){if(Test-AICFUnstuckBrakeLog @($positive|ForEach-Object{$_.Replace($pair[0],$pair[1])})){throw "Unproven native pre-motion accepted $pair"};$logCases++}
"PASS Unstuck brake probe: source_controls=$sourceChecks actual_own_brake_controls=$actual log_cases=$logCases native=NOT_RUN"
