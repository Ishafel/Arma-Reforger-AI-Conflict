param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$prefix=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles'
$gate=[IO.File]::ReadAllText((Join-Path $prefix 'AICF_TransportNativeGate.c'))
$flow=[IO.File]::ReadAllText((Join-Path $prefix 'AICF_VehicleBehaviorFlow.c'))
function Body($s,$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules($g,$f){
 $owned=Body $g 'OwnsTemporaryCrewExit';$inspect=Body $f 'InspectTransit';$begin=Body $g 'Begin'
 if(!$owned){'EXACT_NATIVE_GATE_CREW_EXIT_PROOF_MISSING';return}
 foreach($guard in @('trip.GetMovementState().GetNativeGate() != this','trip.IsDeadlineReached(nowMs)','IsExpired(nowMs)','!Matches(trip)',
  'seat != m_Seat','seat.GetType() != m_SeatType','assignedMember != m_Member','SeatKey(seat) != m_sSeatKey','seat.GetOccupant()',
  'IsCurrentMember(trip, assignedMember)','IsConscious(assignedMember)','ComponentRpl(assignedMember) != m_sMemberRpl',
  'currentAgent != m_Agent','currentAgent.GetControlledEntity() != assignedMember','currentAgent.GetParentGroup() != m_Group',
  'linkedVehicle != m_Vehicle','accessVehicle != m_Vehicle','access.GetCompartment() != m_Seat','access.IsSwitchingSeatsAnim()',
  'm_Perform.m_SmartActionComponent.m_Value != m_SmartAction','SMART_ACTION_TAG','!Live(current)','OwnsCompletedPerformHandoff(current, nowMs)','current.GetRelatedGroupActivity() != m_Activity',
  'current == m_Perform','current == m_Return','current == m_Exit','m_Exit.m_Vehicle.m_Value == m_Seat.GetOwner()')){
  if(!$owned.Contains($guard)){'CURRENT_EXACT_ASSIGNED_FIT_NATIVE_CHAIN';break}
 }
 foreach($guard in @('gate.m_sMemberRpl = AICF_VehicleBehaviorBinding.ComponentRpl(member);','gate.m_Agent = agent;','gate.m_SeatType = seat.GetType();')){if(!$begin.Contains($guard)){'CAPTURE_IMMUTABLE_ACTOR_AND_SEAT';break}}
 if($owned -match '\b(?:Set\w*|AddAction|CallLater|Fail|Complete|Mark\w*|Commit\w*)\s*\('){'READONLY_PROOF_NO_NEW_WAIT_OR_MUTATION'}
 if(!$inspect.Contains('gate.OwnsTemporaryCrewExit(trip, seat, assignedMember)') -or !$inspect.Contains('trip.VehiclePresetMember(trip.GetVehiclePreset().m_aMembers[index])') -or
  $inspect.IndexOf('StartTacticalDeployment("LOCAL_THREAT"') -gt $inspect.IndexOf('OwnsTemporaryCrewExit(') -or
  !$inspect.Contains('StartTacticalDeployment("CREW_UNAVAILABLE"')){'THREAT_PRIORITY_AND_OTHER_CREW_FAILURE_PRESERVED'}
 $handoff=Body $g 'OwnsCompletedPerformHandoff';$callback=Body $g 'OnPerformCompleted';$attach=Body $g 'AttachReturnCompletion';$stop=Body $g 'Stop'
 foreach($guard in @('!m_bPerformCompleted','!m_bPerformListenerAttached','!m_bObservedExit','m_iPerformCompletedAtMs > nowMs','m_iPerformCompletedAtMs >= m_iDeadline','IsExpired(nowMs)',
  'current != m_Perform','current.GetActionState() != EAIActionState.COMPLETED','!current.GetRemoveAction()','!Live(m_Return)','m_Return.GetActionIsSuspended()',
  'm_Return.GetRelatedGroupActivity() != m_Activity','m_Utility.GetActions(actions)','actions.Contains(m_Return)')){if(!$handoff.Contains($guard)){'EXACT_COMPLETED_PERFORM_WITH_QUEUED_NATIVE_RETURN';break}}
 foreach($guard in @('m_bPerformCompleted','action != m_Perform','m_Trip.IsDeadlineReached(nowMs)','IsExpired(nowMs)','!Matches(m_Trip)',
  'm_Trip.GetMovementState().GetNativeGate() != this','action.GetActionState() != EAIActionState.COMPLETED','m_iPerformCompletedAtMs = nowMs;')){if(!$callback.Contains($guard)){'IMMUTABLE_OWN_NATIVE_PERFORM_COMPLETION';break}}
 if($attach.IndexOf('!m_Perform.m_OnActionCompleted') -gt $attach.IndexOf('.Insert(') -or !$attach.Contains('m_Perform.m_OnActionCompleted.Insert(OnPerformCompleted);') -or
  !$stop.Contains('m_Perform.m_OnActionCompleted.Remove(OnPerformCompleted);') -or !$stop.Contains('m_bPerformCompleted = false;') -or !$stop.Contains('m_iPerformCompletedAtMs = 0;')){'BALANCED_PERFORM_LISTENER_LIFECYCLE'}
 if($handoff -match '\b(?:Set\w*|AddAction|CallLater|Fail|Complete|Mark\w*|Commit\w*)\s*\('){'READONLY_COMPLETION_HANDOFF_NO_RENEWAL'}
}
$fail=@(Rules $gate $flow);if($fail.Count){throw "Native gate crew exit: $fail"}
$sourceMutants=0
foreach($guard in @('trip.GetMovementState().GetNativeGate() != this','IsExpired(nowMs)','!Matches(trip)','seat != m_Seat','assignedMember != m_Member','IsConscious(assignedMember)','currentAgent != m_Agent','current.GetRelatedGroupActivity() != m_Activity')){
 if(!@(Rules ($gate.Replace($guard,'false')) $flow).Count){throw "Source mutant escaped $guard"};$sourceMutants++
}
function Cs($s){
 $s=$s.Replace('System.GetTickCount()','Clock.Now').Replace('array<string> tags = {};','var tags = new List<string>();').Replace('array<ref AIActionBase> actions = {};','var actions = new List<AIActionBase>();').Replace('vector.Distance','V.Distance')
 $s=$s.Replace('ChimeraCharacter.Cast(assignedMember)','assignedMember as ChimeraCharacter')
 $s=$s.Replace('SCR_AIUtilityComponent.Cast(currentAgent.FindComponent(SCR_AIUtilityComponent))','currentAgent.Utility')
 $s=$s.Replace('m_sMemberRpl.IsEmpty()','string.IsNullOrEmpty(m_sMemberRpl)')
 return $s
}
$code=@'
using System; using System.Collections.Generic;
public class N {public static implicit operator bool(N n){return !Object.ReferenceEquals(n,null);} public static bool operator !(N n){return Object.ReferenceEquals(n,null);}}
public static class Clock {public static int Now=100;}
public struct V {public double X;public V(double x){X=x;}public static double Distance(V a,V b){return Math.Abs(a.X-b.X);}}
public enum ECompartmentType {PILOT,TURRET,CARGO}
public enum EAIActionState {EVALUATED,RUNNING,COMPLETED,FAILED}
public class IEntity:N {public bool Current=true,Conscious=true;public string Rpl="actor-rpl";public V Origin;public V GetOrigin(){return Origin;}}
public class Group:N {}
public class AIControl:N {public AIAgent Agent;public AIAgent GetControlAIAgent(){return Agent;}}
public class ChimeraCharacter:IEntity {public AIControl Control=new AIControl();public CompartmentAccessComponent Access=new CompartmentAccessComponent();public AIControl GetAIControlComponent(){return Control;}public CompartmentAccessComponent GetCompartmentAccessComponent(){return Access;}}
public class CompartmentAccessComponent:N {public IEntity Owner,Linked;public BaseCompartmentSlot Seat;public bool Switching;public bool IsSwitchingSeatsAnim(){return Switching;}public IEntity GetVehicleCompartmentManagerOwner(){return Owner;}public BaseCompartmentSlot GetCompartment(){return Seat;}public static IEntity GetVehicleIn(IEntity e){return ((ChimeraCharacter)e).Access.Linked;}}
public class AIAgent:N {public IEntity Entity;public Group Group;public SCR_AIUtilityComponent Utility;public IEntity GetControlledEntity(){return Entity;}public Group GetParentGroup(){return Group;}}
public class SCR_AIUtilityComponent:N {public AIActionBase Current;public List<AIActionBase> Actions=new List<AIActionBase>();public AIActionBase GetCurrentBehavior(){return Current;}public void GetActions(List<AIActionBase> dest){dest.AddRange(Actions);}}
public class Ref<T> {public T m_Value;}
public class Smart:N {public List<string> Tags=new List<string>{"OpenGate"};public void GetTags(List<string> dest){dest.AddRange(Tags);}}
public class AIActionBase:N {public bool Removed,Suspended;public Invoker m_OnActionCompleted=new Invoker();public bool GetActionIsSuspended(){return Suspended;}public EAIActionState State=EAIActionState.RUNNING;public N Activity;public int Priority=10;public Ref<IEntity> m_Vehicle=new Ref<IEntity>();public Ref<Smart> m_SmartActionComponent=new Ref<Smart>();public N GetRelatedGroupActivity(){return Activity;}public int GetPriority(){return Priority;}public bool GetRemoveAction(){return Removed;}public EAIActionState GetActionState(){return State;}}
public class SCR_AIActionBase:AIActionBase {public const int PRIORITY_BEHAVIOR_OPEN_NAVLINK_DOOR=10;}
public class Invoker:N {public int RemoveCalls;public List<Action<SCR_AIActionBase>> List=new List<Action<SCR_AIActionBase>>();public void Insert(Action<SCR_AIActionBase> a){List.Add(a);}public void Remove(Action<SCR_AIActionBase> a){RemoveCalls++;List.Remove(a);}public void Fire(SCR_AIActionBase a){foreach(var f in new List<Action<SCR_AIActionBase>>(List))f(a);}}
public static class SCR_AIGoalReaction_OpenNavlinkDoor {public const string SMART_ACTION_TAG="OpenGate";}
public class BaseCompartmentSlot:N {public ECompartmentType Type=ECompartmentType.PILOT;public IEntity Occupant;public IEntity Owner=new IEntity();public string Key="0:2:0";public new ECompartmentType GetType(){return Type;}public IEntity GetOccupant(){return Occupant;}public IEntity GetOwner(){return Owner;}}
public class Movement:N {public Gate Gate;public Gate GetNativeGate(){return Gate;}}
public class AICF_TransportTrip:N {public bool Expired;public Movement Movement=new Movement();public Movement GetMovementState(){return Movement;}public bool IsDeadlineReached(int now){return Expired;}}
public static class AICF_VehiclePreset {public static string SeatKey(BaseCompartmentSlot s){return s.Key;}}
public static class AICF_VehicleShortRecovery {public static bool IsCurrentMember(AICF_TransportTrip t,IEntity e){return e.Current;}}
public static class SCR_AIDamageHandling {public static bool IsConscious(IEntity e){return e.Conscious;}}
public static class AICF_VehicleBehaviorBinding {public static string ComponentRpl(IEntity e){return e.Rpl;}}
public class Gate:N {
 public AICF_TransportTrip Trip;public BaseCompartmentSlot Seat;public ChimeraCharacter Actor;
 public BaseCompartmentSlot m_Seat;public ECompartmentType m_SeatType=ECompartmentType.PILOT;public IEntity m_Member;
 public string m_sSeatKey="0:2:0",m_sMemberRpl="actor-rpl";public AIAgent m_Agent;public Group m_Group;public SCR_AIUtilityComponent m_Utility;
 public AIActionBase m_Perform,m_Exit,m_Return;public Smart m_SmartAction;public IEntity m_SmartOwner,m_Vehicle;public N m_Activity;
 public AICF_TransportTrip m_Trip;public bool m_bStopped,m_bReturnListenerAttached,m_bPerformListenerAttached,m_bPerformCompleted,m_bReturnCompleted,m_bObservedExit;public int m_iStarted=50,m_iDeadline=500,m_iPerformCompletedAtMs,m_iReturnCompletedAtMs;public int LogCalls;public Action<Gate> Reenter;public void Log(string e){LogCalls++;if(Reenter!=null)Reenter(this);}
 public bool Expired,MatchesNow=true;public bool IsExpired(int now){return Expired || now>=m_iDeadline || now-m_iStarted>=60000;}public bool Matches(AICF_TransportTrip t){return MatchesNow;}
 public static bool Live(AIActionBase action) LIVE
 public bool OwnsCompletedPerformHandoff(AIActionBase current,int nowMs) HANDOFF
 public bool AttachReturnCompletion(AICF_TransportTrip trip) ATTACH
 public void OnPerformCompleted(SCR_AIActionBase action) PERFORMCALLBACK
 public void OnReturnCompleted(SCR_AIActionBase action) RETURNCALLBACK
 public void Stop() STOPBODY
 public bool OwnsTemporaryCrewExit(AICF_TransportTrip trip,BaseCompartmentSlot seat,IEntity assignedMember) OWNED
 public static Gate Fresh(){
  var g=new Gate();g.Trip=new AICF_TransportTrip();g.Trip.Movement.Gate=g;g.Seat=new BaseCompartmentSlot();g.m_Seat=g.Seat;
  g.Actor=new ChimeraCharacter();g.m_Member=g.Actor;g.m_Group=new Group();g.m_Utility=new SCR_AIUtilityComponent();
  g.m_Agent=new AIAgent{Entity=g.Actor,Group=g.m_Group,Utility=g.m_Utility};g.Actor.Control.Agent=g.m_Agent;
  g.m_SmartAction=new Smart();g.m_SmartOwner=new IEntity();g.m_Vehicle=new IEntity();g.m_Activity=new N();
  g.m_Perform=new SCR_AIActionBase{Activity=g.m_Activity};g.m_Perform.m_SmartActionComponent.m_Value=g.m_SmartAction;
  g.m_Return=new SCR_AIActionBase{Activity=g.m_Activity};g.m_Exit=new SCR_AIActionBase{Activity=g.m_Activity,Priority=110};g.m_Exit.m_Vehicle.m_Value=g.Seat.Owner;
  g.m_Utility.Current=g.m_Exit;return g;
 }
 public bool Actual(){return OwnsTemporaryCrewExit(Trip,Seat,Actor);}
}
public static class NativeGateCrewTests {
 public static Gate Handoff(){
  Clock.Now=100;var g=Gate.Fresh();g.m_Utility.Current=g.m_Perform;g.m_Utility.Actions.Add(g.m_Return);g.m_bObservedExit=true;
  g.m_Perform.State=EAIActionState.COMPLETED;g.m_Perform.Removed=true;
  if(!g.AttachReturnCompletion(g.Trip))throw new Exception("Native listener setup failed");
  g.m_Perform.m_OnActionCompleted.Fire((SCR_AIActionBase)g.m_Perform);return g;
 }
 public static int Run(){int n=0;Action<bool,string> check=(ok,label)=>{n++;if(!ok)throw new Exception(label);};
  foreach(int stage in new[]{0,1,2}){var g=Gate.Fresh();g.m_Utility.Current=stage==0?g.m_Exit:stage==1?g.m_Perform:g.m_Return;check(g.Actual(),"actual owned native stage");}
  var negatives=new Action<Gate>[] {
   g=>g.Trip.Movement.Gate=null,g=>g.Trip.Movement.Gate=Gate.Fresh(),g=>g.Trip.Expired=true,g=>g.Expired=true,g=>g.MatchesNow=false,
   g=>g.m_Seat=new BaseCompartmentSlot(),g=>g.Seat.Type=ECompartmentType.CARGO,g=>g.m_Member=new ChimeraCharacter(),g=>g.Seat.Key="other",g=>g.Seat.Occupant=new IEntity(),
   g=>g.Actor.Current=false,g=>g.Actor.Conscious=false,g=>g.m_sMemberRpl="",g=>g.m_sMemberRpl="NONE",g=>g.Actor.Rpl="replaced",
   g=>g.Actor.Access=null,g=>g.Actor.Access.Switching=true,g=>g.Actor.Access.Linked=new IEntity(),g=>g.Actor.Access.Owner=new IEntity(),g=>g.Actor.Access.Seat=new BaseCompartmentSlot(),
   g=>g.Actor.Control=null,g=>g.Actor.Control.Agent=null,g=>g.Actor.Control.Agent=new AIAgent(),g=>g.m_Agent.Entity=new IEntity(),g=>g.m_Agent.Group=new Group(),g=>g.m_Agent.Utility=new SCR_AIUtilityComponent(),
   g=>g.m_Perform=null,g=>g.m_Perform.m_SmartActionComponent.m_Value=new Smart(),g=>g.m_Perform.Priority=11,g=>g.m_SmartAction.Tags.Clear(),g=>g.m_SmartOwner.Origin=new V(50.001),
   g=>g.m_Utility.Current=null,g=>g.m_Utility.Current.Removed=true,g=>g.m_Utility.Current.State=EAIActionState.COMPLETED,g=>g.m_Utility.Current.State=EAIActionState.FAILED,
   g=>g.m_Utility.Current.Activity=new N(),g=>g.m_Utility.Current=new AIActionBase{Activity=g.m_Activity},g=>g.m_Exit.m_Vehicle.m_Value=new IEntity(),g=>g.m_Exit.Priority=109
  };
  foreach(var mutate in negatives){var g=Gate.Fresh();mutate(g);check(!g.Actual(),"actual wrong/stale/foreign/dead/unowned gate crew exit");}
  var edge=Gate.Fresh();edge.m_SmartOwner.Origin=new V(50);check(edge.Actual(),"original native50m boundary");
  var transition=Gate.Fresh();transition.Actor.Access.Linked=transition.m_Vehicle;transition.Actor.Access.Owner=transition.m_Vehicle;transition.Actor.Access.Seat=transition.Seat;check(transition.Actual(),"original exact vehicle/seat transition linkage");
  var zero=Gate.Fresh();check(!zero.OwnsTemporaryCrewExit(null,zero.Seat,zero.Actor),"missing trip");check(!zero.OwnsTemporaryCrewExit(zero.Trip,null,zero.Actor),"missing seat");check(!zero.OwnsTemporaryCrewExit(zero.Trip,zero.Seat,null),"missing assigned original");
  var completed=Handoff();check(completed.Actual(),"actual successful completion receipt bridges cached COMPLETED Perform");
  var handoffNegatives=new Action<Gate>[] {
   g=>g.m_bStopped=true,g=>g.m_bPerformListenerAttached=false,g=>g.m_bPerformCompleted=false,g=>g.m_bObservedExit=false,
   g=>g.m_iPerformCompletedAtMs=0,g=>g.m_iPerformCompletedAtMs=49,g=>g.m_iPerformCompletedAtMs=101,g=>g.m_iPerformCompletedAtMs=500,
   g=>g.Expired=true,g=>g.Trip.Expired=true,g=>g.MatchesNow=false,g=>g.Trip.Movement.Gate=Gate.Fresh(),
   g=>g.m_Utility.Current=new SCR_AIActionBase{State=EAIActionState.COMPLETED,Removed=true,Activity=g.m_Activity},
   g=>g.m_Perform.State=EAIActionState.EVALUATED,g=>g.m_Perform.State=EAIActionState.FAILED,g=>g.m_Perform.Removed=false,
   g=>g.m_Return.Removed=true,g=>g.m_Return.State=EAIActionState.FAILED,g=>g.m_Return.State=EAIActionState.COMPLETED,
   g=>g.m_Return.Suspended=true,g=>g.m_Return.Activity=new N(),g=>g.m_Utility.Actions.Clear(),
   g=>g.m_Utility.Actions=new List<AIActionBase>{new SCR_AIActionBase{Activity=g.m_Activity}},
   g=>g.Actor.Conscious=false,g=>g.Actor.Current=false,g=>g.Actor.Rpl="foreign",g=>g.Actor.Access.Linked=new IEntity(),g=>g.Seat.Occupant=new IEntity()
  };
  foreach(var mutate in handoffNegatives){var g=Handoff();mutate(g);check(!g.Actual(),"completed handoff remains bounded and exact");}
  Clock.Now=100;
  var duplicate=Handoff();Clock.Now=120;duplicate.m_Perform.m_OnActionCompleted.Fire((SCR_AIActionBase)duplicate.m_Perform);
  check(duplicate.m_iPerformCompletedAtMs==100 && duplicate.LogCalls==1,"duplicate actual completion cannot renew receipt");
  var callbackNegatives=new Action<Gate>[] {
   g=>g.m_bStopped=true,g=>g.m_bPerformListenerAttached=false,g=>g.m_Trip=null,g=>g.Trip.Expired=true,g=>g.Expired=true,
   g=>g.MatchesNow=false,g=>g.Trip.Movement.Gate=Gate.Fresh(),g=>g.m_Perform.State=EAIActionState.FAILED,g=>g.m_Perform.State=EAIActionState.EVALUATED
  };
  foreach(var mutate in callbackNegatives){var g=Handoff();g.m_bPerformCompleted=false;g.m_iPerformCompletedAtMs=0;mutate(g);g.OnPerformCompleted((SCR_AIActionBase)g.m_Perform);check(!g.m_bPerformCompleted,"invalid callback cannot create receipt");}
  var foreign=Handoff();foreign.m_bPerformCompleted=false;foreign.m_iPerformCompletedAtMs=0;foreign.OnPerformCompleted(new SCR_AIActionBase{State=EAIActionState.COMPLETED});check(!foreign.m_bPerformCompleted,"foreign callback rejected");
  var deadline=Handoff();deadline.m_bPerformCompleted=false;Clock.Now=deadline.m_iDeadline;deadline.OnPerformCompleted((SCR_AIActionBase)deadline.m_Perform);check(!deadline.m_bPerformCompleted,"actual original window deadline rejects late callback");
  var stopped=Handoff();var performInvoker=stopped.m_Perform.m_OnActionCompleted;var returnInvoker=stopped.m_Return.m_OnActionCompleted;
  stopped.Stop();stopped.Stop();check(performInvoker.List.Count==0 && returnInvoker.List.Count==0 && performInvoker.RemoveCalls==1 && returnInvoker.RemoveCalls==1,"idempotent Stop removes both listeners exactly once");
  check(!stopped.m_bPerformCompleted && stopped.m_iPerformCompletedAtMs==0 && !stopped.m_bPerformListenerAttached && !stopped.m_bReturnListenerAttached && stopped.m_Trip==null,"Stop clears captured receipt and ownership cycle");
  Clock.Now=100;
  var missing=Gate.Fresh();missing.m_Perform.m_OnActionCompleted=null;check(!missing.AttachReturnCompletion(missing.Trip) && missing.m_Return.m_OnActionCompleted.List.Count==0 && !missing.m_bReturnListenerAttached,"missing Perform invoker causes no partial subscription");
  var attach=Handoff();check(!attach.AttachReturnCompletion(attach.Trip) && attach.m_Perform.m_OnActionCompleted.List.Count==1 && attach.m_Return.m_OnActionCompleted.List.Count==1,"reentry cannot duplicate subscriptions");
  var reentrant=Gate.Fresh();reentrant.m_Perform.State=EAIActionState.COMPLETED;reentrant.AttachReturnCompletion(reentrant.Trip);reentrant.Reenter=g=>g.Stop();reentrant.m_Perform.m_OnActionCompleted.Fire((SCR_AIActionBase)reentrant.m_Perform);
  check(reentrant.m_bStopped && !reentrant.m_bPerformCompleted && reentrant.m_iPerformCompletedAtMs==0,"synchronous callback Stop reentry retains cleanup");
  return n;
 }
}
'@
$template=$code.Replace('LIVE',(Cs (Body $gate 'Live')))
foreach($entry in @(@('HANDOFF','OwnsCompletedPerformHandoff'),@('ATTACH','AttachReturnCompletion'),@('PERFORMCALLBACK','OnPerformCompleted'),@('RETURNCALLBACK','OnReturnCompleted'),@('STOPBODY','Stop'))){$template=$template.Replace($entry[0],(Cs (Body $gate $entry[1])))}
$code=$template.Replace('OWNED',(Cs (Body $gate 'OwnsTemporaryCrewExit')))
Add-Type -TypeDefinition $code -Language CSharp
$actual=[NativeGateCrewTests]::Run()
$unsafeMutants=0
foreach($guard in @('IsExpired(nowMs)','!Matches(trip)','assignedMember != m_Member')){
 $unsafeMutants++;$variant='NativeGateUnsafe'+$unsafeMutants
 $mutated=(Cs (Body $gate 'OwnsTemporaryCrewExit')).Replace($guard,'false')
 Add-Type -TypeDefinition ('namespace '+$variant+' {'+$template.Replace('OWNED',$mutated)+'}') -Language CSharp
 $caught=$false
 try{([type]($variant+'.NativeGateCrewTests')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$caught=$true}
 if(!$caught){throw "Actual unsafe mutant escaped: $guard"}
}
foreach($change in @(@('!m_bPerformCompleted','false'),@('actions.Contains(m_Return)','true'),@('current != m_Perform','false'),@('m_Return.GetActionIsSuspended()','false'))){
 $unsafeMutants++;$variant='NativeGateHandoffUnsafe'+$unsafeMutants
 $mutatedTemplate=$template.Replace($change[0],$change[1])
 Add-Type -TypeDefinition ('namespace '+$variant+' {'+$mutatedTemplate.Replace('OWNED',(Cs (Body $gate 'OwnsTemporaryCrewExit')))+'}') -Language CSharp
 $caught=$false
 try{([type]($variant+'.NativeGateCrewTests')).GetMethod('Run').Invoke($null,@())|Out-Null}catch{$caught=$true}
 if(!$caught){throw "Actual unsafe handoff mutant escaped: $($change[0])"}
}
Write-Output "PASS native gate crew exit: actual_owned_native_chain=$actual source_mutants=$sourceMutants executable_unsafe_mutants=$unsafeMutants threat_priority=UNCHANGED original_deadline=UNCHANGED native=NOT_RUN"
