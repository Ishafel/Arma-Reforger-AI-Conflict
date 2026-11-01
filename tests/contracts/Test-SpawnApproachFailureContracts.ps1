param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$base=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function ReadSource([string]$path){[IO.File]::ReadAllText((Join-Path $base $path))}
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$receipt=ReadSource 'State/Vehicles/AICF_SpawnApproachFailure.c'
$flow=ReadSource 'Vehicles/AICF_VehicleAcquisitionFlow.c'
$node=ReadSource 'Forces/AICF_AINodeLifecycle.c'
$report=Body $flow 'ReportSpawnApproachFailure'
$observe=Body $flow 'ObserveSpawnApproachFailure'
$handle=Body $node 'AICF_HandleSpawnApproachFailedMovement'
if(!$report -or !$observe -or !$handle){throw 'Missing actual approach failure method'}
$controls=0
function Require([bool]$ok,[string]$name){if(!$ok){throw "Approach failure contract: $name"};$script:controls++}
Require ((Body $flow 'ProcessApproachingSite').IndexOf('ObserveSpawnApproachFailure') -lt (Body $flow 'ProcessApproachingSite').IndexOf('MeasureStagingReadiness')) 'failure before readiness/spawn'
Require ($observe.Contains('return EndRequest(trip, fleet, requestState, "SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN", nowMs);')) 'typed failure through existing flow outcome'
Require ($handle -match '(?s)if \(!failure.IsSourceCurrent\(\)\) return false;\s*m_GroupUtilityComponent.OnMoveFailed.*?failure.ConfirmDelivery\(\) && m_GroupUtilityComponent.GetExecutedAction\(\) == failedAction.*?failedAction.Fail\(\)') 'native callback followed by repeated identity fence'
Require ($handle -notmatch 'CompleteWaypoint|CommitTransition|Teleport|ClearOrder|RequestSpawn') 'node does not own phase/spawn/order'
Require ($report -notmatch 'CommitTransition|Fail\(|OnMoveFailed|EndVehicleControl|Teleport') 'report only records phase-local receipt'
Require ($node -match 'hasHandler && hasRelated && hasLocation && AICF_HandleSpawnApproachFailedMovement') 'all result ports required'
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleScenarioProbe.c'))
$resolver=Body $fixture 'ResolveVehicleSpawnApproachOwner'
Require ($resolver -match '(?s)super.ResolveVehicleSpawnApproachOwner.*?group != m_VSGroup.*?AICF_VSOwnRecoveryRoster\(ownSlot\).*?coordinator = m_VSVehicles') 'fixture super-first exact owner parity'
foreach($path in @('Bootstrap/AICF_MatchController.c','Vehicles/AICF_VehicleCoordinator.c','Vehicles/AICF_TransportTripController.c')){
 Require ((ReadSource $path).Contains('ReportSpawnApproachFailure(') -or (ReadSource $path).Contains('ReportVehicleSpawnApproachFailure(')) "owner delegation $path"
}
$rootOwner=Body (ReadSource 'Bootstrap/AICF_MatchController.c') 'ResolveVehicleSpawnApproachOwner'
Require ($rootOwner -match 'FindManagedInfantrySlot\(group\)' -and $rootOwner -match 'm_USState.FindSlotByGroup\(group\) == slot' -and $rootOwner -match 'm_USSRState.FindSlotByGroup\(group\) == slot' -and $rootOwner -match 'group.GetFaction\(\) != faction') 'ordinary owner derives actual current state and faction'
$coordinatorReport=Body (ReadSource 'Vehicles/AICF_VehicleCoordinator.c') 'ReportSpawnApproachFailure'
Require ($coordinatorReport -match 'FindTripForSlot\(slot\)' -and $coordinatorReport -match 'TryCreateAssignmentSnapshot\(slot, faction,\s*baseRevision, assignment\)' -and $coordinatorReport -match 'm_Fleets.FindFleet\(faction.GetFactionKey\(\)\)' -and $coordinatorReport -match 'm_TripController.ReportSpawnApproachFailure\(trip, assignment, slot, fleet,') 'coordinator passes current graph snapshot and existing fleet to owner'
$controllerReport=Body (ReadSource 'Vehicles/AICF_TransportTripController.c') 'ReportSpawnApproachFailure'
Require ($controllerReport -match 'HasCurrentIdentity\(trip, currentAssignment, slot, fleet, faction\)' -and $controllerReport -match 'HasStaleRevision\(trip, currentAssignment\)' -and $controllerReport -match 'return m_AcquisitionFlow.ReportSpawnApproachFailure') 'controller fences then delegates report without phase effects'
function Adapt([string]$text){
 $text=$text.Replace('protected ','public ').Replace('EntityID','long').Replace('System.GetTickCount()','AICF_Clock.GetTickCount()')
 $text=$text.Replace('void AICF_SpawnApproachFailure(','public AICF_SpawnApproachFailure(').Replace('class AICF_SpawnApproachFailure','public class AICF_SpawnApproachFailure')
 $text=[regex]::Replace($text,'(?m)^\t(AIActionBase|AIWaypoint|bool) (\w+)\(','`tpublic $1 $2(').Replace('`t',"`t")
 $text=[regex]::Replace($text,'!(m_\w+|trip|slot|utility|action|waypoint|requestState|plan|failure|controller|activity|failedAction|access)(?![\w.(])','$1 == null')
 foreach($name in @('rpl','failure','agent','approachFailureOutcome')){$text=$text.Replace("if ($name)","if ($name != null)")}
 $text=$text.Replace('rpl &&','rpl != null &&').Replace('m_Trip.GetLease() ||','m_Trip.GetLease() != null ||')
 $text=$text.Replace('trip.GetLease())','trip.GetLease() != null)').Replace('m_Trip.GetVehiclePresetSlot() &&','m_Trip.GetVehiclePresetSlot() != null &&')
 $text=$text.Replace('m_Group.GetFaction() &&','m_Group.GetFaction() != null &&').Replace('CompartmentAccessComponent.GetVehicleIn(member))','CompartmentAccessComponent.GetVehicleIn(member) != null)')
 $text=$text.Replace('FindComponent(RplComponent)','FindComponent(typeof(RplComponent))').Replace('FindComponent(CompartmentAccessComponent)','FindComponent(typeof(CompartmentAccessComponent))')
 $text=$text.Replace('array<AIWaypoint> queue = {};','List<AIWaypoint> queue = new List<AIWaypoint>();').Replace('array<AIAgent> agents = {};','List<AIAgent> agents = new List<AIAgent>();').Replace('foreach (AIAgent agent : agents)','foreach (AIAgent agent in agents)')
	$text=$text.Replace('IEntity member;','IEntity member = null;').Replace('int alive;','int alive = 0;')
	$text=$text.Replace('!activity.m_RelatedWaypoint','activity.m_RelatedWaypoint == null').Replace('!rpl ||','rpl == null ||')
 return $text
}
$adapter=@'
using System;
using System.Collections.Generic;
public enum AICF_ETransportTripPhase { APPROACHING_SITE, BOARDING, FALLBACK }
public enum EAIActionState { RUNNING, FAILED, COMPLETED }
public static class EMoveError { public const int UNKNOWN=0, STUCK=1; }
public struct vector { public float X; public vector(float x){X=x;} public static float Distance(vector a,vector b){return Math.Abs(a.X-b.X);} }
public static class AICF_Clock { public static int Now=100; public static int GetTickCount(){return Now;} }
public static class Replication { public static bool Server=true;public static bool IsServer(){return Server;} }
public class RplComponent {public int Identity=9;public bool Master=true,Proxy;public bool IsMaster(){return Master;}public bool IsProxy(){return Proxy;}public int Id(){return Identity;}public static RplComponent Cast(object o){return o as RplComponent;} }
public class Faction { public string Key="USSR";public string GetFactionKey(){return Key;} }
public class SCR_CampaignFaction: Faction {}
public class IEntity {public bool Alive=true,Authority=true;public RplComponent Rpl=new RplComponent();public CompartmentAccessComponent Access=new CompartmentAccessComponent();public object FindComponent(Type t){return t==typeof(RplComponent)?(object)Rpl:Access;} }
public class CompartmentAccessComponent {public bool In,Entering,Exiting,Switching;public IEntity Vehicle;public bool IsInCompartment(){return In;}public bool IsGettingIn(){return Entering;}public bool IsGettingOut(){return Exiting;}public bool IsSwitchingSeatsAnim(){return Switching;}public static CompartmentAccessComponent Cast(object o){return o as CompartmentAccessComponent;}public static IEntity GetVehicleIn(IEntity e){return e.Access==null?null:e.Access.Vehicle;} }
public class AIAgent {public IEntity Entity=new IEntity();public SCR_AIGroup Group;public IEntity GetControlledEntity(){return Entity;}public SCR_AIGroup GetParentGroup(){return Group;} }
public class AIWaypoint {public int Id=30;public vector Position=new vector(10);public int GetID(){return Id;}public vector GetOrigin(){return Position;} }
public class AIActionBase {public int Fails;public EAIActionState State=EAIActionState.RUNNING;public EAIActionState GetActionState(){return State;}public void Fail(){Fails++;State=EAIActionState.FAILED;} }
public class SCR_AIActivityBase:AIActionBase {public AIWaypoint m_RelatedWaypoint;public static SCR_AIActivityBase Cast(object o){return o as SCR_AIActivityBase;} }
public class SCR_AIGroupUtilityComponent {public AIActionBase Action;public SCR_AIActivityBase Activity;public int Callbacks;public Action Callback;public AIActionBase GetExecutedAction(){return Action;}public object GetCurrentAction(){return Activity;}public void OnMoveFailed(int r,IEntity v,bool b,vector l){Callbacks++;if(Callback!=null)Callback();} }
public class SCR_AIGroup:IEntity {public long Id=8;public Faction Faction=new Faction();public SCR_AIGroupUtilityComponent Utility=new SCR_AIGroupUtilityComponent();public AIWaypoint Current;public List<AIWaypoint> Queue=new List<AIWaypoint>();public List<AIAgent> Agents=new List<AIAgent>();public long GetID(){return Id;}public Faction GetFaction(){return Faction;}public SCR_AIGroupUtilityComponent GetGroupUtilityComponent(){return Utility;}public AIWaypoint GetCurrentWaypoint(){return Current;}public void GetWaypoints(List<AIWaypoint> q){q.AddRange(Queue);}public void GetAgents(List<AIAgent>a){a.AddRange(Agents);} }
public static class AICF_GroupRuntime {public static bool IsAliveCharacter(IEntity e){return e!=null&&e.Alive;} }
public static class AICF_VehicleBoardingMutationFence {public static bool IsAuthoritativeAIEntity(IEntity e){return e.Authority;} }
public class AICF_StrategicAssignmentSnapshot {public SCR_AIGroup Group;public int Gen=1,Slot=0,Revision=2,Intent=3;public SCR_AIGroup GetGroup(){return Group;}public int GetGroupGeneration(){return Gen;}public int GetSlotId(){return Slot;}public int GetAssignmentRevision(){return Revision;}public int GetStrategicIntentRevision(){return Intent;}public string GetFactionKey(){return "USSR";}public bool MatchesAssignmentRevision(AICF_StrategicAssignmentSnapshot a){return a==this;} }
public class AICF_GroupSlot {public SCR_AIGroup Group;public bool Ready=true;public int Gen=1,Slot=0,Revision=2,Intent=3;public bool IsCombatReady(){return Ready;}public SCR_AIGroup GetGroup(){return Group;}public int GetSpawnGeneration(){return Gen;}public int GetSlotId(){return Slot;}public int GetStrategicAssignmentRevision(){return Revision;}public int GetStrategicIntentRevision(){return Intent;} }
public class AICF_VehicleRequestState {public AICF_VehicleSpawnPlan Plan;public int Gen=1;public bool Deadline;public int GetRequestGeneration(){return Gen;}public AICF_VehicleSpawnPlan GetSpawnPlan(){return Plan;}public bool IsDeadlineReached(int t){return Deadline;} }
public class AICF_VehicleSpawnPlan {public AIWaypoint Waypoint;public AICF_SpawnApproachFailure Failure;public bool Valid=true;public int Deadline=1000;public AIWaypoint GetApproachWaypoint(){return Waypoint;}public AICF_SpawnApproachFailure GetApproachFailure(){return Failure;}public bool IsValid(){return Valid;}public int GetApproachDeadlineMs(){return Deadline;}public object GetReservation(){return this;}public bool BindApproachFailure(AICF_SpawnApproachFailure f){if(Failure!=null)return false;Failure=f;return true;}public void ClearApproachFailure(AICF_SpawnApproachFailure f){if(Failure==f)Failure=null;} }
public class AICF_VehicleSpawner {public bool Current=true;public bool IsSiteReservationCurrent(object p,AICF_TransportTrip t,AICF_VehicleRequestState r,int n){return Current;} }
public class AICF_TransportTrip {public AICF_StrategicAssignmentSnapshot Assignment;public AICF_VehicleRequestState Request;public AICF_ETransportTripPhase Phase=AICF_ETransportTripPhase.APPROACHING_SITE;public bool Valid=true,Terminal,Deadline,PresetCurrent=true;public object Lease;public AICF_GroupSlot PresetSlot;public int TripGen=1;public int GetTripGeneration(){return TripGen;}public AICF_StrategicAssignmentSnapshot GetAssignment(){return Assignment;}public AICF_VehicleRequestState GetRequestState(){return Request;}public bool IsValid(){return Valid;}public bool IsTerminal(){return Terminal;}public AICF_ETransportTripPhase GetPhase(){return Phase;}public object GetLease(){return Lease;}public AICF_GroupSlot GetVehiclePresetSlot(){return PresetSlot;}public bool IsVehiclePresetCurrent(){return PresetCurrent;}public bool IsDeadlineReached(int n){return Deadline;} }
public class AICF_FactionFleet {}
public class AICF_TripOutcome {public string Reason;}
public static class AICF_Stage35Diagnostics {public static void Warning(string e,string d){} }
RECEIPT
public class AICF_ApproachFlowAdapter {
 public AICF_VehicleSpawner m_Spawner=new AICF_VehicleSpawner();public bool Context=true,PlanCurrent=true,Queued=true;
 public bool IsAuthorityContextValid(AICF_TransportTrip t,AICF_StrategicAssignmentSnapshot a,AICF_FactionFleet f,SCR_CampaignFaction c){return Context;}
 public bool IsSpawnPlanCurrent(AICF_TransportTrip t,AICF_VehicleRequestState r,AICF_VehicleSpawnPlan p,int n){return PlanCurrent&&p!=null&&p==r.Plan;}
 public bool IsApproachWaypointQueued(AICF_TransportTrip t,AICF_VehicleSpawnPlan p){return Queued;}
 public string BuildIdentityContext(AICF_TransportTrip t,AICF_VehicleRequestState r,object l,string c){return "";}
 public string BuildCausationId(AICF_TransportTrip t,AICF_VehicleRequestState r,string c){return c;}
 public AICF_TripOutcome EndRequest(AICF_TransportTrip t,AICF_FactionFleet f,AICF_VehicleRequestState r,string why,int now){return new AICF_TripOutcome(){Reason=why};}
 public AICF_SpawnApproachFailure ReportSpawnApproachFailure(AICF_TransportTrip trip,AICF_StrategicAssignmentSnapshot currentAssignment,AICF_GroupSlot slot,AICF_FactionFleet fleet,SCR_CampaignFaction faction,SCR_AIGroupUtilityComponent utility,AIActionBase action,AIWaypoint waypoint,vector location) REPORT
 public AICF_TripOutcome ObserveSpawnApproachFailure(AICF_TransportTrip trip,AICF_FactionFleet fleet,AICF_VehicleRequestState requestState,AICF_VehicleSpawnPlan plan,int nowMs) OBSERVE
}
public class AICF_MatchController {
 public static AICF_MatchController Active;public AICF_SpawnApproachFailure Failure;public static AICF_MatchController GetActiveController(){return Active;}
 public AICF_SpawnApproachFailure ReportVehicleSpawnApproachFailure(SCR_AIGroup g,SCR_AIGroupUtilityComponent u,AIActionBase a,AIWaypoint w,vector l){return Failure;}
}
public class AICF_ApproachNodeAdapter {
 public SCR_AIGroup m_Group;public SCR_AIGroupUtilityComponent m_GroupUtilityComponent;public bool HandlerAllowed=true;
 public bool AICF_IsInfantryMoveHandler(int h){return HandlerAllowed;}
 public bool AICF_HandleSpawnApproachFailedMovement(int result,int handler,bool related,vector location) HANDLE
}
public static class AICF_ApproachCases {
 public static string Run(int test){
  Replication.Server=true;AICF_Clock.Now=100;
  var g=new SCR_AIGroup();var w=new AIWaypoint();g.Current=w;g.Queue.Add(w);
  var actor=new AIAgent(){Group=g};g.Agents.Add(actor);
  var a=new SCR_AIActivityBase(){m_RelatedWaypoint=w};g.Utility.Action=a;g.Utility.Activity=a;
  var slot=new AICF_GroupSlot(){Group=g};var assignment=new AICF_StrategicAssignmentSnapshot(){Group=g};
  var plan=new AICF_VehicleSpawnPlan(){Waypoint=w};var request=new AICF_VehicleRequestState(){Plan=plan};
  var trip=new AICF_TransportTrip(){Assignment=assignment,Request=request};var flow=new AICF_ApproachFlowAdapter();
  var location=w.Position;AIWaypoint input=w;AIActionBase action=a;var inputAssignment=assignment;var inputUtility=g.Utility;
  switch(test){
   case 1:Replication.Server=false;break;case 2:trip.Phase=AICF_ETransportTripPhase.BOARDING;break;case 3:trip.Terminal=true;break;case 4:trip.Lease=new object();break;
   case 5:slot.Gen++;break;case 6:slot.Revision++;break;case 7:slot.Intent++;break;case 8:slot.Slot++;break;case 9:slot.Group=new SCR_AIGroup();break;
   case 10:g.Rpl.Master=false;break;case 11:g.Rpl.Proxy=true;break;case 12:slot.Ready=false;break;case 13:plan.Valid=false;break;case 14:flow.m_Spawner.Current=false;break;
   case 15:trip.Deadline=true;break;case 16:request.Deadline=true;break;case 17:plan.Deadline=100;break;case 18:trip.PresetSlot=slot;trip.PresetCurrent=false;break;
   case 19:g.Current=new AIWaypoint();break;case 20:g.Queue.Clear();break;case 21:g.Utility.Action=new AIActionBase();break;case 22:g.Utility.Activity=new SCR_AIActivityBase(){m_RelatedWaypoint=w};break;
   case 23:a.m_RelatedWaypoint=new AIWaypoint();break;case 24:a.State=EAIActionState.FAILED;break;case 25:a.State=EAIActionState.COMPLETED;break;
   case 26:actor.Group=new SCR_AIGroup();break;case 27:actor.Entity.Authority=false;break;case 28:actor.Entity.Access.In=true;break;case 29:actor.Entity.Access.Entering=true;break;
   case 30:actor.Entity.Access.Exiting=true;break;case 31:actor.Entity.Access.Vehicle=new IEntity();break;case 32:actor.Entity.Access=null;break;case 33:g.Agents.Clear();break;
   case 34:location=new vector(float.NaN);break;case 35:location=new vector(11.001f);break;case 36:input=new AIWaypoint();break;case 37:inputAssignment=new AICF_StrategicAssignmentSnapshot(){Group=g};break;
   case 38:flow.Context=false;break;case 39:flow.PlanCurrent=false;break;case 40:flow.Queued=false;break;case 41:action=null;break;case 42:inputUtility=null;break;case 43:input=null;break;
   case 44:actor.Entity.Access.Switching=true;break;
  }
  var f=flow.ReportSpawnApproachFailure(trip,inputAssignment,slot,new AICF_FactionFleet(),new SCR_CampaignFaction(),inputUtility,action,input,location);
  if(test<100)return f==null?"REJECT":"ACCEPT";
  if(f==null)return "SETUP_FAILED";
  var root=new AICF_MatchController(){Failure=f};AICF_MatchController.Active=root;
  var node=new AICF_ApproachNodeAdapter(){m_Group=g,m_GroupUtilityComponent=g.Utility};int result=(int)EMoveError.UNKNOWN;bool related=true;
  switch(test){
   case 101:result=(int)EMoveError.STUCK;break;case 102:related=false;break;case 103:node.HandlerAllowed=false;break;case 104:AICF_MatchController.Active=null;break;
   case 105:g.Utility.Callback=()=>{g.Utility.Action=new AIActionBase();};break;
   case 106:g.Utility.Callback=()=>{slot.Revision++;};break;
   case 107:g.Utility.Callback=()=>{request.Gen++;};break;
   case 108:g.Utility.Callback=()=>{g.Rpl.Identity++;};break;
   case 109:g.Utility.Callback=()=>{plan.Failure=null;};break;
   case 110:g.Utility.Callback=()=>{request.Plan=new AICF_VehicleSpawnPlan();};break;
   case 111:g.Utility.Callback=()=>{trip.Assignment=new AICF_StrategicAssignmentSnapshot(){Group=g};};break;
   case 112:g.Utility.Callback=()=>{trip.Terminal=true;};break;
   case 113:g.Utility.Callback=()=>{trip.Phase=AICF_ETransportTripPhase.BOARDING;};break;
   case 114:g.Utility.Callback=()=>{flow.m_Spawner.Current=false;};break;
   case 115:g.Utility.Callback=()=>{g.Utility.Activity=new SCR_AIActivityBase(){m_RelatedWaypoint=w};};break;
   case 116:g.Utility.Callback=()=>{g.Current=new AIWaypoint();};break;
   case 117:g.Utility.Callback=()=>{actor.Entity.Access.Entering=true;};break;
   case 118:g.Utility.Callback=()=>{g.Rpl.Master=false;};break;
   case 119:g.Utility.Callback=()=>{g.Id++;};break;
   case 120:g.Utility.Callback=()=>{trip.PresetSlot=slot;trip.PresetCurrent=false;};break;
   case 121:g.Utility.Callback=()=>{trip.Deadline=true;};break;
   case 123:g.Utility.Callback=()=>{actor.Entity.Access.Switching=true;};break;
   case 124:g.Utility.Callback=()=>{trip.TripGen++;};break;
  }
  bool handled=node.AICF_HandleSpawnApproachFailedMovement(result,-1,related,location);
  if(test==122)node.AICF_HandleSpawnApproachFailedMovement(result,-1,related,location);
  switch(test){
   case 130:request.Gen++;break;case 131:slot.Revision++;break;case 132:trip.Phase=AICF_ETransportTripPhase.BOARDING;break;
   case 133:trip.Assignment=new AICF_StrategicAssignmentSnapshot(){Group=g};break;case 134:plan.Waypoint=new AIWaypoint();break;
   case 135:flow.m_Spawner.Current=false;break;case 136:trip.Deadline=true;break;
   case 137:trip=new AICF_TransportTrip(){Assignment=assignment,Request=request};break;case 138:request.Plan=new AICF_VehicleSpawnPlan();break;
  }
  var outcome=flow.ObserveSpawnApproachFailure(trip,new AICF_FactionFleet(),request,plan,101);
  return handled+"/"+g.Utility.Callbacks+"/"+a.Fails+"/"+f.WasDelivered()+"/"+(outcome==null?"NONE":outcome.Reason);
 }
}
'@
$adapter=$adapter.Replace('RECEIPT',(Adapt $receipt)).Replace('REPORT',(Adapt $report)).Replace('OBSERVE',(Adapt $observe)).Replace('HANDLE',(Adapt $handle))
if(!('AICF_ApproachCases' -as [type])){Add-Type -TypeDefinition $adapter}
Require ([AICF_ApproachCases]::Run(0) -eq 'ACCEPT') 'actual source receipt accepts current owned approach'
foreach($case in 1..44){Require ([AICF_ApproachCases]::Run($case) -eq 'REJECT') "actual report rejects wrong/stale/foreign case=$case"}
$positive='True/1/1/True/SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN'
Require ([AICF_ApproachCases]::Run(100) -eq $positive) 'actual node callback then next-tick typed fallback'
foreach($case in 101..104){Require ([AICF_ApproachCases]::Run($case) -eq 'False/0/0/False/NONE') "unhandled scope keeps native case=$case"}
foreach($case in @((105..121)+123+124)){Require ([AICF_ApproachCases]::Run($case) -eq 'True/1/0/False/NONE') "callback identity replacement preserves new state case=$case"}
Require ([AICF_ApproachCases]::Run(122) -eq $positive) 'duplicate delivery never repeats native callback or action failure'
foreach($case in 130..138){Require ([AICF_ApproachCases]::Run($case) -eq 'True/1/1/True/NONE') "delivered receipt never affects replacement next Tick case=$case"}
"Spawn approach failure contracts: PASS; controls=$controls; actual-body adapters preserve scope, identity, callback and outcome owners"
