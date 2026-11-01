param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
function Body($s,$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
$helper=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_UnstuckFailedSites.c'))
$state=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehiclePhaseStates.c'))
$flow=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'))
function Rules($h,$s,$f){
 foreach($v in @('reason != "NO_PHYSICAL_MOVEMENT"','now - state.GetLastPhysicalMotionAtMs() < timeoutMs','state.GetMaximumMobilityRecoveryAttempts() != 2','m_aSites.Count() < 2','trip.GetTransitionCount() != m_iTransitions','m_State.GetRouteGeneration() == m_iRouteGeneration','m_State.GetUnstuckFailedSites() != this','m_State.GetSupersededRouteWaypoint() != m_Waypoint','bound != m_ExpectedWaypoint','bound.GetID() != m_ExpectedWaypointId','!queue.Contains(bound)','transformed && m_bPrepared')){if(!$h.Contains($v)){'BOUNDED_PROVEN_ORIGIN_AND_EXACT_OWNED_ROUTE';break}}
 $sample=Body $f 'TryRelocateVehicleForUnstuck';$begin=Body $f 'BeginMobilityRecovery';$confirm=Body $f 'ConfirmRouteWaypointBound'
 foreach($v in @('failedSites.Rejects(trip, vehicle, candidate, UNSTUCK_MIN_DISPLACEMENT_METERS)','failedSites.PreparedCurrent(trip, vehicle)','failedSites.Rejects(trip, vehicle, selectedCandidate, UNSTUCK_MIN_DISPLACEMENT_METERS)','failedSites.CommitSuccessfulRelocation(trip, vehicle, true)')){if(!$sample.Contains($v)){'ACTUAL_SAMPLER_REPEATED_SITE_AND_OWNERSHIP_VETO';break}}
 if($sample.IndexOf('failedSites.CommitSuccessfulRelocation')-lt$sample.IndexOf('if (vehicle.SetWorldTransform(relocatedTransform))')){'NO_INVENTED_RECORD_ON_FAILED_TRANSFORM'}
 foreach($v in @('failedSites.PrepareFailure(trip, vehicle, stuckReason, m_Config.GetStuckTimeoutMs())','if (!relocated) failedSites.CancelPreparedFailure()','failedSites.BeginOwnedRouteReplacement(trip, vehicle)','failedSites.StageOwnedRouteReplacement(trip, vehicle, outcome.GetKind() == AICF_ETripOutcomeKind.START_MOVEMENT)')){if(!$begin.Contains($v)){'OWNED_REPLACEMENT_EXPLICIT_FLOW_SEQUENCE';break}}
 if($begin.IndexOf('failedSites.BeginOwnedRouteReplacement')-gt$begin.IndexOf('state.SuspendRouteWaypoint()')-or$begin.IndexOf('failedSites.StageOwnedRouteReplacement')-lt$begin.IndexOf('AICF_TripOutcome outcome = CreateAndStageRoute')){'OWNED_REPLACEMENT_EXPLICIT_FLOW_SEQUENCE'}
 if(!$confirm.Contains('if (confirmed) trip.GetMovementState().GetUnstuckFailedSites().AdvanceOwnedRoute')){'ADVANCE_ONLY_ACTUAL_BOUND_WAYPOINT'}
 $movement=$s.Substring($s.IndexOf('class AICF_VehicleMovementState'))
 if(!(Body $movement 'Reset').Contains('ClearUnstuckFailedSites()')-or!$f.Contains('if (physicalMotion) state.ClearUnstuckFailedSites()')){'RESET_AND_OBSERVED_PHYSICAL_PROGRESS_INVALIDATE'}
 if((ConvertTo-AICFCodeText $h)-match'\b(?:SetOrigin|SetWorldTransform|SetHealth|SetPhase|CreditNativeRouteProgress|MarkProgress|SetVelocity|ActivateAI|SetLOD)\s*\('){'PASSIVE_HISTORY_CANNOT_MUTATE_WORLD_OR_PROGRESS'}
}
$bad=@(Rules $helper $state $flow);if($bad.Count){throw "Failed-site contract $bad"};$sourceChecks=1
foreach($v in @('reason != "NO_PHYSICAL_MOVEMENT"','now - state.GetLastPhysicalMotionAtMs() < timeoutMs','m_aSites.Count() < 2','trip.GetTransitionCount() != m_iTransitions','m_State.GetRouteGeneration() == m_iRouteGeneration','m_State.GetSupersededRouteWaypoint() != m_Waypoint','bound != m_ExpectedWaypoint','transformed && m_bPrepared')){if(!@(Rules $helper.Replace($v,'false') $state $flow).Count){throw "Failed-site source mutant escaped $v"};$sourceChecks++}
function Trans($s){
 $s=$s.Replace('protected ','public ').Replace('protected ref ','public ').Replace('ref array<vector>','List<Vec>').Replace('array<vector>','List<Vec>').Replace('array<AIWaypoint>','List<AIWaypoint>').Replace(' = {};',' = new List<Vec>();').Replace('List<AIWaypoint> queue = new List<Vec>();','List<AIWaypoint> queue = new List<AIWaypoint>();')
 $s=$s.Replace('EntityID','long').Replace('vector','Vec').Replace('System.GetTickCount()','Clock.GetTickCount()').Replace('m_aSites.Count()','m_aSites.Count').Replace('m_aSites.Insert(','m_aSites.Add(')
 $s=$s.Replace('bool duplicate;','bool duplicate=false;')
 $s=[regex]::Replace($s,'foreach \(Vec (\w+) : m_aSites\)','foreach (Vec $1 in m_aSites)')
 $s=$s.Replace('string.Format(','F(')
 return $s
}
$translated=Trans $helper
$translated=$translated.Replace('class AICF_UnstuckFailedSites','public class AICF_UnstuckFailedSites:N')
$translated=[regex]::Replace($translated,'(?m)^\t(bool|void|int|string|static bool) (\w+)\(',' public $1 $2(')
$translated=$translated.Replace('public ref List<Vec>','public List<Vec>').Replace('public ref AICF_UnstuckMutationScope','public AICF_UnstuckMutationScope')
$translated=$translated.Replace('public class AICF_UnstuckFailedSites:N'+"`n"+'{','public class AICF_UnstuckFailedSites:N'+"`n"+'{'+"`n"+'static string F(string s,params object[] a){for(int i=0;i<a.Length;i++)s=s.Replace("%"+(i+1),Convert.ToString(a[i],System.Globalization.CultureInfo.InvariantCulture));return s;}')
if(!$translated.Contains('static string F(')){ $translated=$translated.Replace('public class AICF_UnstuckFailedSites:N'+"`r`n"+'{','public class AICF_UnstuckFailedSites:N'+"`r`n"+'{'+"`r`n"+'static string F(string s,params object[] a){for(int i=0;i<a.Length;i++)s=s.Replace("%"+(i+1),Convert.ToString(a[i],System.Globalization.CultureInfo.InvariantCulture));return s;}') }
$motion=(Body $state 'ObservePhysicalMotion').Replace('vector.','Vec.')
$arm=(Body $state 'ArmRecoveryEvidence')
$stage=(Body $state 'StageRouteWaypoint').Replace('vector.','Vec.')
$suspend=Body $state 'SuspendRouteWaypoint';$bound=Body $state 'ConfirmRouteWaypointBound'
$motionFlow=Body $flow 'ObserveProgressAndRecover';$start=$motionFlow.IndexOf('bool physicalMotion =');$end=$motionFlow.IndexOf('if (routeProgress || physicalMotion)',$start);$motionFlow=$motionFlow.Substring($start,$end-$start)
$cs=@'
using System;using System.Collections.Generic;
namespace AICFFailedSitesActual {
public static class Strings {public static bool IsEmpty(this string s){return string.IsNullOrEmpty(s);}}
public class N {public static bool operator !(N x){return ReferenceEquals(x,null);}public static implicit operator bool(N x){return !ReferenceEquals(x,null);}}
public struct Vec {public float X,Y,Z;public Vec(float x,float y,float z){X=x;Y=y;Z=z;}public static float Distance(Vec a,Vec b){return(float)Math.Sqrt((a.X-b.X)*(a.X-b.X)+(a.Y-b.Y)*(a.Y-b.Y)+(a.Z-b.Z)*(a.Z-b.Z));}public static float DistanceSqXZ(Vec a,Vec b){return(a.X-b.X)*(a.X-b.X)+(a.Z-b.Z)*(a.Z-b.Z);}public static float DistanceXZ(Vec a,Vec b){return(float)Math.Sqrt(DistanceSqXZ(a,b));}public static bool operator==(Vec a,Vec b){return a.X==b.X&&a.Y==b.Y&&a.Z==b.Z;}public static bool operator!=(Vec a,Vec b){return !(a==b);}public override bool Equals(object o){return o is Vec&&this==(Vec)o;}public override int GetHashCode(){return X.GetHashCode();}}
public static class Clock {public static int Now=50000;public static int GetTickCount(){return Now;}}
public enum AICF_EOrderTargetKind {POSITION,BASE}
public class IEntity:N {public long Id=1;public string Rpl="1";public Vec Pos;public long GetID(){return Id;}public Vec GetOrigin(){return Pos;}}
public class Vehicle:IEntity {}
public class AIWaypoint:IEntity {}
public class SCR_CampaignMilitaryBaseComponent:N {}
public class SCR_AIGroup:IEntity {public List<AIWaypoint> Queue=new List<AIWaypoint>();public Action OnQueueRead;public void GetWaypoints(List<AIWaypoint> q){q.AddRange(Queue);if(OnQueueRead!=null)OnQueueRead();}}
public class AICF_VehicleBehaviorBinding {public static string ComponentRpl(IEntity x){return x==null?"NONE":x.Rpl;}}
public class AICF_StrategicAssignmentSnapshot:N {public SCR_AIGroup Group;public int Revision=8,Intent=3;public Vec Goal=new Vec(100,0,100);public AICF_EOrderTargetKind Kind;public SCR_CampaignMilitaryBaseComponent Base;public SCR_AIGroup GetGroup(){return Group;}public int GetAssignmentRevision(){return Revision;}public int GetStrategicIntentRevision(){return Intent;}public Vec GetTargetPosition(){return Goal;}public AICF_EOrderTargetKind GetTargetKind(){return Kind;}public SCR_CampaignMilitaryBaseComponent GetTargetBase(){return Base;}}
public class AICF_VehicleLease:N {public Vehicle Car;public int Lease=1,Generation=1;public int GetLeaseGeneration(){return Lease;}public int GetVehicleGeneration(){return Generation;}}
public class AICF_TransportTrip:N {public AICF_VehicleLease Lease;public AICF_VehicleMovementState State;public AICF_StrategicAssignmentSnapshot Assignment;public bool ScopeSafe=true;public int Transitions=8,GroupGeneration=1,TripGeneration=1,Slot;public string Faction="US";public string GetFactionKey(){return Faction;}public int GetSlotId(){return Slot;}public int GetGroupGeneration(){return GroupGeneration;}public int GetTripGeneration(){return TripGeneration;}public AICF_VehicleLease GetLease(){return Lease;}public AICF_VehicleMovementState GetMovementState(){return State;}public AICF_StrategicAssignmentSnapshot GetAssignment(){return Assignment;}public int GetTransitionCount(){return Transitions;}}
// Controlled upstream mutation-scope predicate; its real 59+3 cases run separately.
public class AICF_UnstuckMutationScope:N {AICF_TransportTrip T;Vehicle C;Vec P;public static AICF_UnstuckMutationScope Capture(AICF_TransportTrip t,Vehicle c){return t!=null&&c!=null&&t.ScopeSafe&&t.Lease!=null&&t.Lease.Car==c?new AICF_UnstuckMutationScope{T=t,C=c,P=c.Pos}:null;}public bool IsCurrent(){return T.ScopeSafe&&T.Lease!=null&&T.Lease.Car==C&&P==C.Pos;}}
public class AICF_VehicleMovementState:N {
 public AICF_UnstuckFailedSites History;public AIWaypoint m_RouteWaypoint,m_SupersededRouteWaypoint;public bool m_bRouteWaypointBound,m_bHasPhysicalSample;public int m_iRouteGeneration,m_iAssignmentRevision,m_iLastRouteProgressAtMs,m_iLastPhysicalMotionAtMs;public Vec m_vTacticalTarget,m_vRouteEndpoint,m_vLastPhysicalPosition;public string m_sRouteMode;public float m_fBestRouteDistanceMeters;public int Attempts=1,Maximum=2;
 public bool m_bRecoveryEvidencePending,m_bRecoveryRequiresRouteProgress,m_bRecoveryPhysicalEvidence,m_bRecoveryRouteEvidence,m_bRecoveryMobilityRestoredReported,m_bPendingUnstuckRelocated;public int m_iRecoveryEvidenceArmedAtMs;public string m_sPendingRecoveryReason;
 public AICF_UnstuckFailedSites GetUnstuckFailedSites(){if(History==null)History=new AICF_UnstuckFailedSites();return History;}public void ClearUnstuckFailedSites(){History=null;}public int GetLastPhysicalMotionAtMs(){return m_iLastPhysicalMotionAtMs;}public int GetMobilityRecoveryAttempts(){return Attempts;}public int GetMaximumMobilityRecoveryAttempts(){return Maximum;}public AIWaypoint GetRouteWaypoint(){return m_RouteWaypoint;}public AIWaypoint GetSupersededRouteWaypoint(){return m_SupersededRouteWaypoint;}public int GetRouteGeneration(){return m_iRouteGeneration;}public bool IsRouteWaypointBound(){return m_bRouteWaypointBound;}
 public bool StageRouteWaypoint(AIWaypoint waypoint,Vec tacticalTarget,Vec routeEndpoint,string routeMode,int assignmentRevision,Vec vehiclePosition,int nowMs) __STAGE__
 public bool SuspendRouteWaypoint() __SUSPEND__
 public bool ConfirmRouteWaypointBound(AIWaypoint expected,bool bound) __BOUND__
 public bool ObservePhysicalMotion(Vec position,float minimumMovementMeters,int nowMs) __MOTION__
 public void ArmRecoveryEvidence(string reason,bool requireRouteProgress,bool unstuckRelocated,Vec vehiclePosition,float routeDistanceMeters,int nowMs) __ARM__
}
__HELPER__
public class Config {public float GetMotionMeters(){return 2;}}
public class MotionFlow {public Config m_Config=new Config();public bool Check(AICF_VehicleMovementState state,Vehicle vehicle,int nowMs){__MOTION_FLOW__ return physicalMotion;}}
public class Env {public AICF_TransportTrip T;public Vehicle C;public SCR_AIGroup G;public AICF_VehicleMovementState S;public AICF_UnstuckFailedSites H;public static Env Fresh(){Clock.Now=50000;var c=new Vehicle{Id=202,Rpl="car"};var g=new SCR_AIGroup{Id=101,Rpl="group"};var s=new AICF_VehicleMovementState();var t=new AICF_TransportTrip{Lease=new AICF_VehicleLease{Car=c},State=s,Assignment=new AICF_StrategicAssignmentSnapshot{Group=g}};var wp=new AIWaypoint{Id=11};s.StageRouteWaypoint(wp,t.Assignment.Goal,t.Assignment.Goal,"ROAD",8,c.Pos,100);s.ConfirmRouteWaypointBound(wp,true);g.Queue.Add(wp);return new Env{T=t,C=c,G=g,S=s,H=s.GetUnstuckFailedSites()};}
 public void Record(){if(!H.PrepareFailure(T,C,"NO_PHYSICAL_MOVEMENT",45000))throw new Exception("prepare");C.Pos=new Vec(8,0,0);if(!H.CommitSuccessfulRelocation(T,C,true))throw new Exception("commit");}
 public void Advance(){H.BeginOwnedRouteReplacement(T,C);S.SuspendRouteWaypoint();var wp=new AIWaypoint{Id=12};S.StageRouteWaypoint(wp,T.Assignment.Goal,T.Assignment.Goal,"ROAD",8,C.Pos,Clock.Now);H.StageOwnedRouteReplacement(T,C,true);S.m_SupersededRouteWaypoint=null;G.Queue.Clear();G.Queue.Add(wp);S.ConfirmRouteWaypointBound(wp,true);H.AdvanceOwnedRoute(T,C,wp);}
}
public static class Run {public static int Execute(){int n=0;Action<bool,string>c=(v,s)=>{if(!v)throw new Exception(s);n++;};var e=Env.Fresh();c(e.H.PrepareFailure(e.T,e.C,"NO_PHYSICAL_MOVEMENT",45000),"valid prepare");c(e.H.PreparedCurrent(e.T,e.C),"current prepared proof");c(e.H.Count()==0,"prepared origin not yet recorded");e.C.Pos=new Vec(8,0,0);c(e.H.CommitSuccessfulRelocation(e.T,e.C,true)&&e.H.Count()==1,"actual successful move records failed A");e.Advance();Clock.Now=100000;e.S.Attempts=2;c(e.H.PrepareFailure(e.T,e.C,"NO_PHYSICAL_MOVEMENT",45000),"second same-owned route epoch");c(e.H.Rejects(e.T,e.C,new Vec(0,0,0),4),"A-B-A rejected");c(!e.H.Rejects(e.T,e.C,new Vec(8,0,8),4),"untried C remains candidate");e.C.Pos=new Vec(8,0,8);c(e.H.CommitSuccessfulRelocation(e.T,e.C,true)&&e.H.Count()==2,"record B only after second physical move");c(AICF_UnstuckFailedSites.NearFailedSite(new Vec(0,4,0),new Vec(0,0,0),4),"inclusive3D4");c(!AICF_UnstuckFailedSites.NearFailedSite(new Vec(0,4.01f,0),new Vec(0,0,0),4),"other-height notXZfalsepositive");
 Action<Action<Env>,string>negative=(change,label)=>{var x=Env.Fresh();x.Record();x.Advance();change(x);c(!x.H.Rejects(x.T,x.C,new Vec(0,0,0),4),label);};
 negative(x=>x.S.m_iRouteGeneration++,"unrelated+1 rejected");negative(x=>x.S.m_RouteWaypoint=new AIWaypoint{Id=99},"unrelated waypoint");negative(x=>x.T.Transitions+=2,"TRANSIT-TACTICAL-TRANSIT ABA");negative(x=>x.T.ScopeSafe=false,"stopped/foreign upstream scope");negative(x=>x.T.Lease=new AICF_VehicleLease{Car=x.C},"lease instance swap");negative(x=>x.T.Lease.Lease++,"lease generation");negative(x=>x.T.Assignment.Revision++,"assignment revision");negative(x=>x.T.Assignment.Intent++,"intent revision");negative(x=>x.T.Assignment.Goal=new Vec(2,0,2),"target change");negative(x=>x.T.Assignment.Kind=AICF_EOrderTargetKind.BASE,"target kind");negative(x=>x.T.Assignment.Base=new SCR_CampaignMilitaryBaseComponent(),"target base");negative(x=>x.C.Rpl="new","car Rpl change");negative(x=>x.G.Id++,"group incarnation");negative(x=>x.S.ClearUnstuckFailedSites(),"Reset replacement invalidates captured holder");
 e=Env.Fresh();c(!e.H.PrepareFailure(e.T,e.C,"NATIVE_UNREACHABLE",45000),"not inferred stationary");e=Env.Fresh();Clock.Now=44999;c(!e.H.PrepareFailure(e.T,e.C,"NO_PHYSICAL_MOVEMENT",45000),"old timeout not elapsed");e=Env.Fresh();e.S.Attempts=3;c(!e.H.PrepareFailure(e.T,e.C,"NO_PHYSICAL_MOVEMENT",45000),"no third attempt");e=Env.Fresh();e.H.PrepareFailure(e.T,e.C,"NO_PHYSICAL_MOVEMENT",45000);c(!e.H.CommitSuccessfulRelocation(e.T,e.C,false)&&e.H.Count()==0,"failed transform invents no record");e=Env.Fresh();e.Record();e.Advance();c(!new MotionFlow().Check(e.S,e.C,50001)&&e.S.GetUnstuckFailedSites()==e.H,"own placement/staged baseline not motion");e.C.Pos=new Vec(11,0,0);c(new MotionFlow().Check(e.S,e.C,51000)&&e.S.GetUnstuckFailedSites()!=e.H,"actual observed motion invalidates history");
 e=Env.Fresh();e.Record();e.H.BeginOwnedRouteReplacement(e.T,e.C);e.S.SuspendRouteWaypoint();e.S.StageRouteWaypoint(new AIWaypoint{Id=12},e.T.Assignment.Goal,e.T.Assignment.Goal,"ROAD",8,e.C.Pos,50000);e.H.StageOwnedRouteReplacement(e.T,e.C,false);c(e.H.Count()==0,"failed owned replacement no authority advance");e=Env.Fresh();e.Record();e.H.BeginOwnedRouteReplacement(e.T,e.C);e.S.SuspendRouteWaypoint();e.S.StageRouteWaypoint(new AIWaypoint{Id=12},e.T.Assignment.Goal,e.T.Assignment.Goal,"ROAD",8,e.C.Pos,50000);e.H.StageOwnedRouteReplacement(e.T,e.C,true);e.S.m_RouteWaypoint=new AIWaypoint{Id=99};e.S.ConfirmRouteWaypointBound(e.S.m_RouteWaypoint,true);e.G.Queue.Clear();e.G.Queue.Add(e.S.m_RouteWaypoint);e.H.AdvanceOwnedRoute(e.T,e.C,e.S.m_RouteWaypoint);c(e.H.Count()==0,"different binding cannot advance prepared route");
 e=Env.Fresh();e.Record();var callback=e;e.G.OnQueueRead=()=>callback.T.Lease=new AICF_VehicleLease{Car=callback.C};e.Advance();c(e.H.Count()==0,"queue callback lease swap cannot authorize route advance");
 e=Env.Fresh();e.Record();callback=e;e.G.OnQueueRead=()=>callback.S.m_iRouteGeneration++;e.Advance();c(e.H.Count()==0,"queue callback unrelated generation cannot authorize route advance");
 e=Env.Fresh();e.Record();e.T.Lease=null;e.H.m_Lease=null;c(!e.H.Rejects(e.T,e.C,new Vec(0,0,0),4),"both weak lease pointers cleared without dereference");
 e=Env.Fresh();e.H.PrepareFailure(e.T,e.C,"NO_PHYSICAL_MOVEMENT",45000);Clock.Now=53001;e.C.Pos=new Vec(8,0,0);c(!e.H.CommitSuccessfulRelocation(e.T,e.C,true)&&e.H.Count()==0,"prepared proof real3001ms expiry");
 e=Env.Fresh();e.Record();e.Advance();Clock.Now=100000;e.S.Attempts=2;e.H.PrepareFailure(e.T,e.C,"NO_PHYSICAL_MOVEMENT",45000);c(!e.H.CommitSuccessfulRelocation(e.T,e.C,false)&&e.H.Count()==1&&e.H.Rejects(e.T,e.C,new Vec(0,0,0),4),"failed second transform preserves only first proven origin");return n;}}
}
'@
$cs=$cs.Replace('__HELPER__',$translated).Replace('__STAGE__',$stage).Replace('__SUSPEND__',$suspend).Replace('__BOUND__',$bound).Replace('__MOTION__',$motion).Replace('__ARM__',$arm).Replace('__MOTION_FLOW__',$motionFlow)
Add-Type -TypeDefinition $cs
$actual=[AICFFailedSitesActual.Run]::Execute()
$mutants=@(
 @('route epoch', $cs.Replace('m_State.GetRouteGeneration() == m_iRouteGeneration &&','true &&')),
 @('phase ABA', $cs.Replace('trip.GetTransitionCount() != m_iTransitions','false')),
 @('failed transform', $cs.Replace('transformed && m_bPrepared','m_bPrepared')),
 @('motion invalidation', $cs.Replace('if (physicalMotion) state.ClearUnstuckFailedSites();',';'))
)
$k=0
foreach($mutant in $mutants){
 if($mutant[1]-eq$cs){throw "Failed-site executable mutant did not match $($mutant[0])"}
 $name='AICFFailedSitesUnsafe'+$k;$types=Add-Type -TypeDefinition $mutant[1].Replace('AICFFailedSitesActual',$name) -PassThru
 $run=$types|Where-Object{$_.FullName-eq($name+'.Run')};$caught=$false
 try{$null=$run.GetMethod('Execute').Invoke($null,@())}catch{$caught=$true}
 if(!$caught){throw "Failed-site executable unsafe mutant escaped $($mutant[0])"};$k++
}
"PASS Unstuck failed sites: source_controls=$sourceChecks actual_history_and_route_motion_controls=$actual executable_unsafe_mutants=$k native=NOT_RUN"
