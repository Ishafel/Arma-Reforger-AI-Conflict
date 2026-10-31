param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$source=[IO.File]::ReadAllText((Join-Path $core 'State/Vehicles/AICF_OverturnRecovery.c'))
$flow=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
$code=$source
$code=$code.Replace('class AICF_OverturnRecovery','public class AICF_OverturnRecovery').Replace('static const int','public const int').Replace('void AICF_OverturnRecovery(','public AICF_OverturnRecovery(')
$code=[regex]::Replace($code,'(?m)^(\s*)(static )?(bool|int) (\w+)\(', '$1public $2$3 $4(')
$code=$code.Replace('return trip &&','return trip != null &&').Replace('return physics &&','return physics != null &&').Replace('vector transform[4];','Vec[] transform = new Vec[4];').Replace('array<AIAgent> agents = {};','var agents = new List<AIAgent>();').Replace('int captured;','int captured = 0;').Replace('for (int index;','for (int index = 0;')
$code=$code.Replace('ReadScope(trip, reason)','ReadScope(trip, out reason)').Replace('Read(trip, utility, verifiedGroup, vehicle, registered, pilot, movement, reason)','Read(trip, utility, out verifiedGroup, out vehicle, out registered, out pilot, out movement, out reason)')
$code=$code.Replace('!observed ||','observed == null ||').Replace('agents.Count()','agents.Count')
$shim=@'
using System;
using System.Collections.Generic;
public class N { public static bool operator !(N n){return ReferenceEquals(n,null);} public static implicit operator bool(N n){return !ReferenceEquals(n,null);} }
public static class Replication {public static bool Server=true;public static bool IsServer(){return Server;}}
public static class Clock {public static int Now=1000;public static int GetTickCount(){return Now;}}
public enum AICF_ETransportTripPhase {TRANSIT,OTHER}
public enum EAIActionState {RUNNING,FAILED}
public enum EDamageState {UNDAMAGED,DAMAGED}
public class Vec {public float Y,L;public float this[int i]{get{return Y;}}public float Length(){return L;}}
public class Physics:N {public Vec Linear=new Vec(),Angular=new Vec();public Vec GetVelocity(){return Linear;}public Vec GetAngularVelocity(){return Angular;}}
public class AIWaypoint:N {}
public class IEntity:N {public bool Alive=true,Conscious=true,AI=true,Settled=true;}
public class AIAgent:N {}
public class AIGroupMovementComponent:N {}
public class SCR_AIGroupVehicle:N {public int GetSubgroupHandleId(){return 1;}}
public class HitZone:N {public float Health=1;public float GetHealthScaled(){return Health;}}
public class SCR_DamageManagerComponent:N {public HitZone Zone=new HitZone();public float Damage;public HitZone GetDefaultHitZone(){return Zone;}public float GetMovementDamage(){return Damage;}public static SCR_DamageManagerComponent GetDamageManager(Vehicle v){return v.Damage;}}
public class SCR_AIVehicleUsageComponent:N {public EDamageState State;public static SCR_AIVehicleUsageComponent Cast(N n){return n as SCR_AIVehicleUsageComponent;}public EDamageState GetDamageState(){return State;}}
public static class SCR_AIVehicleUsability {public static bool VehicleCanMove(Vehicle v){return v.CanMove;}public static bool VehicleIsOnFire(Vehicle v){return v.Fire;}}
public class Vehicle:N {public SCR_DamageManagerComponent Damage=new SCR_DamageManagerComponent();public SCR_AIVehicleUsageComponent Usage=new SCR_AIVehicleUsageComponent();public Physics Physics=new Physics();public bool CanMove=true,Fire;public float Up=-0.1f;public N FindComponent(Type t){return Usage;}public void GetWorldTransform(Vec[] v){v[1]=new Vec{Y=Up};}public Physics GetPhysics(){return Physics;}}
public class SCR_AIMoveActivity:N {public SCR_AIGroupUtilityComponent m_Utility;public AIWaypoint m_RelatedWaypoint;public EAIActionState State;public bool Remove,Suspended;public static SCR_AIMoveActivity Cast(N n){return n as SCR_AIMoveActivity;}public EAIActionState GetActionState(){return State;}public bool GetRemoveAction(){return Remove;}public bool GetActionIsSuspended(){return Suspended;}}
public class SCR_AIGroupUtilityComponent:N {public SCR_AIMoveActivity Current,Executed;public SCR_AIGroup Group;public Vehicle Vehicle;public N GetCurrentAction(){return Current;}public N GetExecutedAction(){return Executed;}}
public class SCR_AIGroup:N {public SCR_AIGroupUtilityComponent Utility;public AIWaypoint Waypoint;public int Count=2;public SCR_AIGroupUtilityComponent GetGroupUtilityComponent(){return Utility;}public AIWaypoint GetCurrentWaypoint(){return Waypoint;}public void GetAgents(List<AIAgent>a){for(int i=0;i<Count;i++)a.Add(new AIAgent());}}
public class AICF_StrategicAssignmentSnapshot:N {public SCR_AIGroup Group;public SCR_AIGroup GetGroup(){return Group;}}
public class AICF_VehicleLease:N {public Vehicle Vehicle;public Vehicle GetVehicle(){return Vehicle;}}
public class AICF_VehicleMovementState:N {public int Generation=1,Attempts,Maximum=2;public AIWaypoint Waypoint;public bool Bound=true;public N Failure,Combat;public AICF_OverturnRecovery Observed;public int GetRouteGeneration(){return Generation;}public AIWaypoint GetRouteWaypoint(){return Waypoint;}public bool IsRouteWaypointBound(){return Bound;}public N GetTransitMoveFailure(){return Failure;}public N GetNativeCombatActivity(){return Combat;}public int GetMobilityRecoveryAttempts(){return Attempts;}public int GetMaximumMobilityRecoveryAttempts(){return Maximum;}public AICF_OverturnRecovery GetOverturnRecovery(){return Observed;}}
public class AICF_TransportTrip:N {public bool Valid=true,Terminal,Current=true,Expired;public AICF_ETransportTripPhase Phase;public N Preset=new N();public AICF_StrategicAssignmentSnapshot Assignment;public AICF_VehicleLease Lease;public AICF_VehicleMovementState State;public IEntity[] Actors={new IEntity(),new IEntity()};public bool IsValid(){return Valid;}public bool IsTerminal(){return Terminal;}public bool IsVehiclePresetCurrent(){return Current;}public bool IsDeadlineReached(int n){return Expired;}public N GetVehiclePreset(){return Preset;}public AICF_ETransportTripPhase GetPhase(){return Phase;}public AICF_StrategicAssignmentSnapshot GetAssignment(){return Assignment;}public AICF_VehicleLease GetLease(){return Lease;}public AICF_VehicleMovementState GetMovementState(){return State;}public IEntity VehiclePresetMember(int i){return Actors[i];}}
public static class AICF_Stage1Config {public const int MAX_GROUP_SIZE=2;}
public static class AICF_GroupRuntime {public static bool IsAliveCharacter(IEntity a){return a.Alive;}}
public static class SCR_AIDamageHandling {public static bool IsConscious(IEntity a){return a.Conscious;}}
public static class AICF_VehicleBoardingMutationFence {public static bool IsAuthoritativeAIEntity(IEntity a){return a.AI;}}
public static class AICF_VehicleNativeSubgroup {public static bool Scope=true,Handler=true,Pilot=true;public static bool Read(AICF_TransportTrip t,SCR_AIGroupUtilityComponent u,out SCR_AIGroup g,out Vehicle v,out SCR_AIGroupVehicle r,out AIAgent p,out AIGroupMovementComponent m,out string reason){g=u.Group;v=u.Vehicle;r=new SCR_AIGroupVehicle();p=Pilot?new AIAgent():null;m=new AIGroupMovementComponent();reason="TEST_NATIVE_SCOPE";return Scope;}public static bool IsCurrentVehicleHandler(AICF_TransportTrip t,SCR_AIGroupUtilityComponent u,int h){return Handler;}public static bool Settled(IEntity a,Vehicle v){return a.Settled;}}
'@
$code=$code.Replace('vehicle.FindComponent(SCR_AIVehicleUsageComponent)','vehicle.FindComponent(typeof(SCR_AIVehicleUsageComponent))')
$driver=@'
public static class Execute {
 public static AICF_TransportTrip Fresh(){var w=new AIWaypoint();var v=new Vehicle();var g=new SCR_AIGroup{Waypoint=w};var u=new SCR_AIGroupUtilityComponent{Group=g,Vehicle=v};g.Utility=u;u.Current=new SCR_AIMoveActivity{m_Utility=u,m_RelatedWaypoint=w};u.Executed=u.Current;var t=new AICF_TransportTrip{Assignment=new AICF_StrategicAssignmentSnapshot{Group=g},Lease=new AICF_VehicleLease{Vehicle=v},State=new AICF_VehicleMovementState{Waypoint=w}};return t;}
 public static int Run(){int n=0;Action<bool,string>check=(b,s)=>{if(!b)throw new Exception(s);n++;};Action<Action<AICF_TransportTrip>,string>negative=(change,s)=>{var t=Fresh();change(t);check(!AICF_OverturnRecovery.CanConsider(t,System.Now),s);};
 check(AICF_OverturnRecovery.WithinWindow(1000,1000),"initial");check(AICF_OverturnRecovery.WithinWindow(4000,1000),"boundary3000");check(!AICF_OverturnRecovery.WithinWindow(4001,1000),"expired");check(!AICF_OverturnRecovery.WithinWindow(999,1000),"clockback");check(!AICF_OverturnRecovery.WithinWindow(1000,0),"missingcapture");
 check(AICF_OverturnRecovery.IsSlowEnough(1,.5f),"slowboundary");check(!AICF_OverturnRecovery.IsSlowEnough(1.001f,0),"fast");check(!AICF_OverturnRecovery.IsSlowEnough(0,.501f),"rotation");check(!AICF_OverturnRecovery.IsSlowEnough(float.NaN,0),"NaN");check(!AICF_OverturnRecovery.IsSlowEnough(-1,0),"invalidspeed");
 check(AICF_OverturnRecovery.IsLocalCorrectionDistance(0),"zero3D");check(AICF_OverturnRecovery.IsLocalCorrectionDistance(15),"maximum3D");check(!AICF_OverturnRecovery.IsLocalCorrectionDistance(15.001f),"far3D");check(!AICF_OverturnRecovery.IsLocalCorrectionDistance(16.125f),"XZ14_height8_rejected");check(!AICF_OverturnRecovery.IsLocalCorrectionDistance(float.NaN),"invalid3D");check(!AICF_OverturnRecovery.IsLocalCorrectionDistance(-1),"negative3D");
 check(AICF_OverturnRecovery.CanConsider(Fresh(),System.Now),"positive");Replication.Server=false;check(!AICF_OverturnRecovery.CanConsider(Fresh(),System.Now),"client");Replication.Server=true;
 negative(t=>t.Valid=false,"invalid");negative(t=>t.Terminal=true,"terminal");negative(t=>t.Phase=AICF_ETransportTripPhase.OTHER,"phase");negative(t=>t.Preset=null,"automatic");negative(t=>t.Current=false,"stale");negative(t=>t.Expired=true,"deadline");negative(t=>t.Assignment.Group=null,"groupmissing");
 AICF_VehicleNativeSubgroup.Scope=false;check(!AICF_OverturnRecovery.CanConsider(Fresh(),System.Now),"nativeidentityhumanforeignfence");AICF_VehicleNativeSubgroup.Scope=true;AICF_VehicleNativeSubgroup.Pilot=false;check(!AICF_OverturnRecovery.CanConsider(Fresh(),System.Now),"pilotmissing");AICF_VehicleNativeSubgroup.Pilot=true;AICF_VehicleNativeSubgroup.Handler=false;check(!AICF_OverturnRecovery.CanConsider(Fresh(),System.Now),"handlerstale");AICF_VehicleNativeSubgroup.Handler=true;
 negative(t=>t.State.Attempts=2,"budget");negative(t=>t.State.Failure=new N(),"otherreceipt");negative(t=>t.State.Combat=new N(),"combat");negative(t=>t.State.Generation=0,"noroute");negative(t=>t.State.Bound=false,"unbound");negative(t=>t.Assignment.Group.Utility.Executed=null,"executed");negative(t=>t.Assignment.Group.Utility.Current.State=EAIActionState.FAILED,"failed");negative(t=>t.Assignment.Group.Utility.Current.Remove=true,"cancelled");negative(t=>t.Assignment.Group.Utility.Current.Suspended=true,"suspended");negative(t=>t.Assignment.Group.Waypoint=new AIWaypoint(),"foreignwaypoint");
 negative(t=>t.Lease.Vehicle.Usage.State=EDamageState.DAMAGED,"damagedstate");negative(t=>t.Lease.Vehicle.Damage.Zone.Health=.99f,"health");negative(t=>t.Lease.Vehicle.Damage.Damage=.1f,"movementdamage");negative(t=>t.Lease.Vehicle.CanMove=false,"disabled");negative(t=>t.Lease.Vehicle.Fire=true,"fire");negative(t=>t.Lease.Vehicle.Up=.25f,"upright");negative(t=>t.Lease.Vehicle.Physics=null,"physics");negative(t=>t.Actors[0].Alive=false,"dead");negative(t=>t.Actors[0].Conscious=false,"incap");negative(t=>t.Actors[0].AI=false,"human");negative(t=>t.Actors[0].Settled=false,"unsettled");negative(t=>t.Assignment.Group.Count=1,"missingoriginal");
 var current=Fresh();check(!AICF_OverturnRecovery.CanMutateNow(current),"nomutationwithoutcapture");current.State.Observed=new AICF_OverturnRecovery(current,System.Now);check(AICF_OverturnRecovery.CanMutateNow(current),"capturedmutation");current.Lease.Vehicle.Physics.Linear.L=1.01f;check(!AICF_OverturnRecovery.CanMutateNow(current),"waitfast");current.Lease.Vehicle.Physics.Linear.L=0;current.State.Generation++;check(!AICF_OverturnRecovery.CanMutateNow(current),"changedroute");current.State.Generation--;System.Now=4001;check(!AICF_OverturnRecovery.CanConsider(current,System.Now),"originalwindowexpired");check(!AICF_OverturnRecovery.CanMutateNow(current),"expiredmutation");System.Now=1000;
 return n;}
}
'@
Add-Type -TypeDefinition ($shim+$code.Replace('System.GetTickCount()','Clock.GetTickCount()')+$driver.Replace('System.Now','Clock.Now'))
$checks=[Execute]::Run()
$recover=Body $flow 'RecoverHealthyOverturn';$sample=Body $flow 'TryRelocateVehicleForUnstuck';$begin=Body $flow 'BeginMobilityRecovery'
foreach($guard in @('m_Config.GetHiddenRecoveryEnabled()','m_Watchdog.CanSafelyRelocateVehicle(','new AICF_OverturnRecovery(trip, nowMs)','observed.Matches(trip, nowMs)','AICF_OverturnRecovery.CanMutateNow(trip)','state.BeginMobilityRecovery()')){if(!$recover.Contains($guard)){throw "Missing recovery guard: $guard"}}
if($sample.IndexOf('AICF_OverturnRecovery.CanMutateNow(trip)') -gt $sample.IndexOf('physics.SetVelocity(vector.Zero)') -or !$sample.Contains('trip.GetLease().GetVehicle() != vehicle')){throw 'Repeated scope must precede world mutation'}
if(!$sample.Contains('requireOverturnScope && !AICF_OverturnRecovery.IsLocalCorrectionDistance(vector.Distance(originalPosition, candidate))')){throw 'Overturn correction must also respect actual3D fifteen-meter boundary'}
if(!$begin.Contains('HEALTHY_OVERTURN_NO_SAFE_CANDIDATE') -or $begin.IndexOf('HEALTHY_OVERTURN_NO_SAFE_CANDIDATE') -gt $begin.IndexOf('state.SuspendRouteWaypoint()')){throw 'No safe candidate must not commit route progress'}
if(!(Body $flow 'AbortOwnedActions').Contains('state.SetOverturnRecovery(null)')){throw 'Stop must retire observation'}
if((ConvertTo-AICFCodeText $source) -match '\b(SetHealth|Heal|Revive|SetOrigin|SetVelocity|SetWorldTransform|CallLater|CommitTransition)\s*\('){throw 'Observation must remain read-only'}
Write-Output "PASS: Overturn recovery contracts ($checks executed extracted scope/window/mutation cases plus dispatch/fence contracts)"
