param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$domain=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$scope=[IO.File]::ReadAllText((Join-Path $domain 'State/Vehicles/AICF_ReboardSeatConflictScope.c'))
$flow=[IO.File]::ReadAllText((Join-Path $domain 'Vehicles/AICF_VehicleBehaviorFlow.c'))
$policy=[IO.File]::ReadAllText((Join-Path $domain 'Vehicles/AICF_VehicleBehaviorPolicy.c'))
$short=[IO.File]::ReadAllText((Join-Path $domain 'Vehicles/AICF_VehicleShortRecovery.c'))
function Body([string]$s,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $name}
function Rules([string]$s,[string]$f,[string]$p,[string]$h){
 $recover=Body $f 'RecoverOwnSeatConflict';$poll=Body $f 'PollReboarding';$exit=Body $h 'TryExit';$suppress=Body $p 'SuppressGetIn'
 if($poll.IndexOf('ReportSeatConflict(trip, actor, seat, "BEFORE_DECISION");') -lt 0 -or $poll.IndexOf('ReportSeatConflict(trip, actor, seat, "BEFORE_DECISION");') -gt $poll.IndexOf('TryClearIncapacitatedSeat(trip, seat, "REBOARD_INCAPACITATED_BLOCKER")')){'DIAGNOSE_ACTUAL_BRANCH_BEFORE_MUTATION'}
 foreach($g in @('FindAction(trip, scope.m_Occupant)','token.m_iShortAttempts >= 3','token.m_iShortAttempts++;','token.Retire();','!scope.IsCurrent()','!trip.GetBehaviorState().m_aActions.Contains(token)','TryExit(trip, scope.m_Occupant, "REBOARD_OWN_WRONG_SEAT", scope)')){if(!$recover.Contains($g)){'OCCUPANT_SHARED_TOKEN_BUDGET_AND_REENTRY';break}}
 if(([regex]::Matches($recover,'access.IsGettingOut\(\) \|\| access.IsGettingIn\(\) \|\| access.IsSwitchingSeatsAnim\(\)')).Count -ne 2 -or $recover.IndexOf('token.m_iShortAttempts++;') -lt $recover.LastIndexOf('access.IsGettingOut()')){'PENDING_TRANSITION_NO_SPEND_BEFORE_AND_AFTER_RETIRE'}
 if(!$exit.Contains('AICF_ReboardSeatConflictScope') -and !$h.Contains('AICF_ReboardSeatConflictScope conflict = null')){'OPTIONAL_OLD_CALLER_SIGNATURE'}
 if($exit.IndexOf('if (conflict && !conflict.IsCurrent()) return false;') -lt $exit.IndexOf('access.InterruptVehicleActionQueue(true, true, true);') -or $exit.IndexOf('if (conflict && !conflict.IsCurrent()) return false;') -gt $exit.IndexOf('access.GetOutVehicle_NoDoor(')){'FRESH_CONFLICT_AFTER_NATIVE_INTERRUPT'}
 if(!$suppress.Contains('AICF_EVehicleBehavior.TRANSPORT && IsCurrentReboarding(binding)')){'ONLY_BOUND_TRANSPORT_REBOARD_MESSAGE'}
 if((ConvertTo-AICFCodeText ((Body $s 'IsCurrent')+$recover+(Body $p 'IsCurrentReboarding'))) -match 'SetHealth|SetDamage|Resurrect|SetOrigin|SetWorldTransform|m_iReboardDeadline\s*=(?!=)|m_iRetries\s*=|m_iShortAttempts\s*=\s*0|CallLater|CommitTransition|AbortOwnedActions'){'NO_NEW_BUDGET_CLOCK_HEALTH_OR_BROAD_ABORT'}
}
$issues=@(Rules $scope $flow $policy $short);if($issues.Count){throw "Own seat source guards: $issues"};$sourceChecks=1
foreach($m in @(
 @('f','FindAction(trip, scope.m_Occupant)','FindAction(trip, expected)','OCCUPANT_SHARED_TOKEN_BUDGET_AND_REENTRY'),
 @('f','token.m_iShortAttempts >= 3','false','OCCUPANT_SHARED_TOKEN_BUDGET_AND_REENTRY'),
 @('f','ReportSeatConflict(trip, actor, seat, "BEFORE_DECISION");','','DIAGNOSE_ACTUAL_BRANCH_BEFORE_MUTATION'),
 @('h','if (conflict && !conflict.IsCurrent()) return false;','','FRESH_CONFLICT_AFTER_NATIVE_INTERRUPT'),
 @('p','AICF_EVehicleBehavior.TRANSPORT && IsCurrentReboarding(binding)','AICF_EVehicleBehavior.TRANSPORT','ONLY_BOUND_TRANSPORT_REBOARD_MESSAGE')
)){
 $s=$scope;$f=$flow;$p=$policy;$h=$short
 switch($m[0]){'f'{$f=$f.Replace($m[1],$m[2])}'h'{$h=$h.Replace($m[1],$m[2])}'p'{$p=$p.Replace($m[1],$m[2])}}
 if($m[3] -notin @(Rules $s $f $p $h)){throw "Unsafe source mutation survived: $($m[3])"};$sourceChecks++
}
# Исполняются реальные Capture/IsCurrent/IsOwnFaction, policy precondition и flow
# decision. Native adapters отвечают только за identity, getters и exit ledger.
function Adapt([string]$s){
 $s=$s.Replace('array<BaseCompartmentSlot>','NativeArray<BaseCompartmentSlot>').Replace('EntityID','int').Replace('int position;','int position = 0;').Replace('Faction faction;','Faction faction = null;').Replace('ChimeraCharacter character =','IEntity character =').Replace('ChimeraCharacter member =','IEntity member =').Replace('IEntity linked =','Vehicle linked =').Replace('System.GetTickCount()','Clock.Now')
 $s=$s.Replace('member.FindComponent(FactionAffiliationComponent)','member.Affiliation')
 $s=$s.Replace('protected bool IsOwnFaction','public bool IsOwnFaction')
  $s=$s.Replace('array<ref AIActionBase> actions = {};','NativeArray<AIActionBase> actions = new NativeArray<AIActionBase>();').Replace('foreach (AIActionBase action : actions)','foreach (AIActionBase action in actions)').Replace('agent.FindComponent(SCR_AIUtilityComponent)','agent.Utility')
 return $s
}
$scopeCode=Adapt $scope
$scopeCode=$scopeCode.Replace('class AICF_ReboardSeatConflictScope','public class AICF_ReboardSeatConflictScope:NativeObject')
$scopeCode=[regex]::Replace($scopeCode,'(?m)^\t((?:AICF_\w+|BaseCompartmentSlot|IEntity|int|bool|string) \w+;)',' public $1')
$scopeCode=[regex]::Replace($scopeCode,'(?m)^\t((?:static )?(?:bool|int|AICF_ReboardSeatConflictScope) \w+\()',' public $1')
$preflight=Adapt (Body $policy 'IsCurrentReboarding')
$suppress=(Adapt (Body $policy 'SuppressGetIn')).Replace('utility.m_OwnerEntity.FindComponent(CompartmentAccessComponent)','utility.m_OwnerEntity.Access')
$recover=Adapt (Body $flow 'RecoverOwnSeatConflict')
$pollBody=Body $flow 'PollReboarding';$start=$pollBody.IndexOf('if (state.m_ReboardSeatConflict)');$brace=$pollBody.IndexOf('{',$start);$depth=1;$end=$brace+1
while($end-lt$pollBody.Length-and$depth-gt0){if($pollBody[$end]-eq'{'){$depth++}elseif($pollBody[$end]-eq'}'){$depth--};$end++}
if($depth-ne0){throw 'Actual pending-scope owner branch missing'}
$pending=(Adapt $pollBody.Substring($start,$end-$start)).Replace('pendingConflict = state.m_ReboardSeatConflict','pendingConflict = (AICF_ReboardSeatConflictScope)state.m_ReboardSeatConflict')
$exit=(Adapt (Body $short 'TryExit')).Replace('vector destination[4];','vector[] destination = new vector[4];')
$seat=Adapt (Body $short 'TrySeat')
$clear=(Adapt (Body $short 'TryClearIncapacitatedSeat')).Replace('vector destination[4];','vector[] destination = new vector[4];')
$unsafeExitLog=$exit.Replace('operationId, memberId, seatKey,','trip.GetOperationId(), member.GetID(), AICF_VehiclePreset.SeatKey(seat),')
$unsafeSeatLog=$seat.Replace('operationId, memberId, seatKey,','trip.GetOperationId(), member.GetID(), AICF_VehiclePreset.SeatKey(seat),')
$unsafeClearLog=$clear.Replace('operationId, memberId, seatKey,','trip.GetOperationId(), member.GetID(), AICF_VehiclePreset.SeatKey(seat),')
$cancel=(Adapt (Body $flow 'CancelEntries')).Replace('foreach (AICF_VehicleBehaviorAction token : state.m_aActions)','foreach (AICF_VehicleBehaviorAction token in state.m_aActions)')
$entryPrefix=Body $flow 'EnsureEntry';$entryPrefix=(Adapt $entryPrefix.Substring(1,$entryPrefix.IndexOf(';'))).Replace('return;','return false;')
$code=@'
using System;
using System.Collections.Generic;
public class NativeObject {public bool Deleted;public static implicit operator bool(NativeObject value){return !Object.ReferenceEquals(value,null)&&!value.Deleted;}}
public static class NativeString {public static bool IsEmpty(this string value){return String.IsNullOrEmpty(value);}}
public static class Clock {public static int Now=1000;}
public class NativeArray<T>:List<T>{public static implicit operator bool(NativeArray<T> v){return v!=null;}public new int Count(){return base.Count;}public int Find(T value){return IndexOf(value);}public bool IsIndexValid(int i){return i>=0&&i<base.Count;}public void Insert(T value){Add(value);}}
public class Faction:NativeObject{public string Key="USSR";public string GetFactionKey(){return Key;}}
public class FactionAffiliationComponent:NativeObject{public Faction Faction=new Faction();public Faction GetAffiliatedFaction(){return Faction;}public static FactionAffiliationComponent Cast(FactionAffiliationComponent a){return a;}}
public class IEntity:NativeObject{public AIControl Control=new AIControl();public AIControl GetAIControlComponent(){return Control;}public bool IsInVehicle(){return Access.Inside;}public int Id;public string Rpl="rpl";public bool Current=true;public FactionAffiliationComponent Affiliation=new FactionAffiliationComponent();public CompartmentAccessComponent Access=new CompartmentAccessComponent();public vector Origin=new vector();public vector GetOrigin(){return Origin;}public int GetID(){if(Deleted)throw new Exception("native getter after actor delete");return Id;}public object GetWorld(){return this;}public void GetWorldTransform(vector[] v){v[3]=Origin;}public CompartmentAccessComponent GetCompartmentAccessComponent(){return Access;}}
public static class ChimeraCharacter{public static IEntity Cast(IEntity a){return a;}}
public class Vehicle:NativeObject{public bool Slow=true,Fire,Movable=true;public object GetWorld(){return this;}public static Vehicle Cast(Vehicle v){return v;}}
public enum ECloseDoorAfterActions{INVALID}public enum EAICompartmentType{Pilot,Turret,Cargo,None}
public class BaseCompartmentSlot:NativeObject{public Vehicle Vehicle;public IEntity Occupant;public IEntity Reserved;public int ReservationWrites;public Action OnReservationWrite;public bool IsReserved(){return Reserved!=null;}public bool IsReservedBy(IEntity a){return Reserved==a;}public void SetReserved(IEntity a){Reserved=a;ReservationWrites++;if(OnReservationWrite!=null)OnReservationWrite();}public string Key;public EAICompartmentType Role=EAICompartmentType.Cargo;public EAICompartmentType Type(){return Role;}public vector GetPosition(){return new vector();}public Vehicle GetVehicle(){if(Deleted)throw new Exception("native getter after seat delete");return Vehicle;}public IEntity GetOccupant(){return Occupant;}}
public class SCR_AIMessage_GetIn:NativeObject{public Vehicle m_Vehicle;public BaseCompartmentSlot m_CompartmentSlot;public EAICompartmentType m_eRoleInVehicle;}
public static class SCR_AICompartmentHandling{public static EAICompartmentType CompartmentClassToType(EAICompartmentType t){return t;}}
public class vector{public float Value;public static float Distance(vector a,vector b){return Math.Abs(a.Value-b.Value);}}
public static class ChimeraWorldUtils{public static bool Water;public static bool TryGetWaterSurfaceSimple(object world,vector pos){return Water;}}
public class CompartmentAccessComponent:NativeObject{public static CompartmentAccessComponent Cast(CompartmentAccessComponent c){return c;}public BaseCompartmentSlot Seat;public Vehicle Vehicle;public bool Inside=true,Entering,Exiting,Switching,TeleportFound=true;public int PhysicalExits,PhysicalEntries,Interrupts;public bool EntryAccepted=true;public float Distance=3;public Action OnInterrupt,OnGetOut,OnGetIn;public bool FindSuitableTeleportLocation(vector[] v){v[3]=new vector{Value=Distance};return TeleportFound;}public void InterruptVehicleActionQueue(bool a,bool b,bool c){Interrupts++;if(OnInterrupt!=null)OnInterrupt();}public bool GetOutVehicle_NoDoor(vector[] v,bool a,bool b,bool c){PhysicalExits++;if(OnGetOut!=null)OnGetOut();return true;}public bool GetInVehicle(Vehicle v,BaseCompartmentSlot s,bool a,int b,ECloseDoorAfterActions c,bool d){PhysicalEntries++;if(OnGetIn!=null)OnGetIn();return EntryAccepted;}public BaseCompartmentSlot GetCompartment(){return Seat;}public bool IsInCompartment(){return Inside;}public bool IsGettingIn(){return Entering;}public bool IsGettingOut(){return Exiting;}public bool IsSwitchingSeatsAnim(){return Switching;}public static Vehicle GetVehicleIn(IEntity a){return a.Access.Vehicle;}}
public static class SCR_AIVehicleUsability{public static bool VehicleIsOnFire(Vehicle a){return a.Fire;}public static bool VehicleCanMove(Vehicle a){return a.Movable;}}
public class SCR_AIGroup:NativeObject{}
public class AICF_StrategicAssignmentSnapshot:NativeObject{public SCR_AIGroup Group=new SCR_AIGroup();public int Revision=3,Intent=4;public SCR_AIGroup GetGroup(){return Group;}public int GetAssignmentRevision(){return Revision;}public int GetStrategicIntentRevision(){return Intent;}}
public class AICF_GroupSlot:NativeObject{public SCR_AIGroup Group;public int Revision=3,Intent=4;public SCR_AIGroup GetGroup(){return Group;}public int GetStrategicAssignmentRevision(){return Revision;}public int GetStrategicIntentRevision(){return Intent;}}
public enum AICF_ETransportTripPhase{TRANSIT,TACTICAL_DEPLOYMENT,FALLBACK}
public enum AICF_EVehicleBehavior{TRANSPORT,COMBAT}
public class AICF_VehiclePreset:NativeObject{public AICF_EVehicleBehavior m_iBehavior=AICF_EVehicleBehavior.TRANSPORT;public NativeArray<int> m_aMembers=new NativeArray<int>();public static string SeatKey(BaseCompartmentSlot s){if(s.Deleted)throw new Exception("seat key after native delete");return s.Key;}}
public class AICF_VehicleLease:NativeObject{public Vehicle Vehicle=new Vehicle();public Vehicle GetVehicle(){return Vehicle;}}
public class AICF_VehicleBehaviorState:NativeObject{public NativeObject m_ReboardSeatConflict;public bool m_bReboarding=true;public int m_iReboardDeadline=9000;public NativeArray<AICF_VehicleBehaviorAction> m_aActions=new NativeArray<AICF_VehicleBehaviorAction>();}
public class AICF_TransportTrip:NativeObject{public AICF_VehicleBehaviorBinding Binding;public int Transitions=5;public int GetTransitionCount(){return Transitions;}public bool Current=true,Expired;public AICF_StrategicAssignmentSnapshot Assignment=new AICF_StrategicAssignmentSnapshot();public AICF_GroupSlot Slot=new AICF_GroupSlot();public AICF_VehicleLease Lease=new AICF_VehicleLease();public AICF_VehicleBehaviorState State=new AICF_VehicleBehaviorState();public AICF_VehiclePreset Preset=new AICF_VehiclePreset();public IEntity[] Roster=new IEntity[5];public AICF_ETransportTripPhase Phase=AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT;public AICF_StrategicAssignmentSnapshot GetAssignment(){return Assignment;}public AICF_GroupSlot GetVehiclePresetSlot(){return Slot;}public AICF_VehicleLease GetLease(){return Lease;}public AICF_VehicleBehaviorState GetBehaviorState(){return State;}public AICF_VehiclePreset GetVehiclePreset(){return Preset;}public IEntity VehiclePresetMember(int p){return p>=0&&p<5?Roster[p]:null;}public string GetFactionKey(){return "USSR";}public AICF_ETransportTripPhase GetPhase(){return Phase;}public bool IsDeadlineReached(int n){return Expired;}public string GetOperationId(){if(Deleted)throw new Exception("trip getter after native callback");return "trip";}}
public class AICF_VehicleBehaviorBinding:NativeObject{public AICF_TransportTrip m_Trip;public Vehicle m_Vehicle;public bool Current=true;public bool IsCurrent(){return Current && m_Trip.Lease!=null;}public bool OwnsActor(IEntity a){return a!=null&&a.Current&&Array.IndexOf(m_Trip.Roster,a)>=0;}public bool IsDeployed(IEntity a){return false;}public static string ComponentRpl(IEntity a){return a.Rpl;}}
public static class AICF_Stage1Config{public const int MAX_GROUP_SIZE=5;}
public class AIControl:NativeObject{public AIAgent Agent=new AIAgent();public AIAgent GetControlAIAgent(){return Agent;}} public class AIAgent:NativeObject{public IEntity Controlled;public SCR_AIUtilityComponent Utility=new SCR_AIUtilityComponent();public IEntity GetControlledEntity(){return Controlled;}} public enum EAIActionState{EVALUATED,RUNNING,COMPLETED,FAILED} public class AIActionBase:NativeObject{} public class NativeValue<T>{public T m_Value;} public class SCR_AIGetInVehicle:AIActionBase{public SCR_AIUtilityComponent m_Utility;public NativeValue<Vehicle> m_Vehicle=new NativeValue<Vehicle>();public NativeValue<BaseCompartmentSlot> m_CompartmentToGetIn=new NativeValue<BaseCompartmentSlot>();public bool Remove;public EAIActionState State=EAIActionState.RUNNING;public bool GetRemoveAction(){return Remove;}public EAIActionState GetActionState(){return State;}public static SCR_AIGetInVehicle Cast(AIActionBase a){return a as SCR_AIGetInVehicle;}} public class SCR_AIUtilityComponent:NativeObject{public IEntity m_OwnerEntity;public NativeArray<AIActionBase> Actions=new NativeArray<AIActionBase>();public Action OnGetActions;public void GetActions(NativeArray<AIActionBase> a){a.AddRange(Actions);if(OnGetActions!=null)OnGetActions();}public static SCR_AIUtilityComponent Cast(SCR_AIUtilityComponent u){return u;}}
public class AICF_VehicleBehaviorAction:NativeObject{public AICF_TransportTrip Trip;public IEntity m_Actor;public BaseCompartmentSlot m_Seat;public int m_iShortAttempts,Retired;public bool Owner=true;public Action<AICF_VehicleBehaviorAction> OnRetire;public AICF_VehicleBehaviorAction(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s){Trip=t;m_Actor=a;m_Seat=s;}public SCR_AIUtilityComponent ResolveOwner(){return Owner?new SCR_AIUtilityComponent():null;}public void Retire(){Retired++;if(OnRetire!=null)OnRetire(this);}}
public static class AICF_VehicleShortRecovery{public static int Calls;public static Action<AICF_TransportTrip> OnExit;public static bool ExitAccepted=true,UseActualExit;public static bool IsCurrentMember(AICF_TransportTrip t,IEntity a){return t.Current&&a!=null&&a.Current;}public static bool IsSlowVehicle(Vehicle v){return v.Slow;}public static bool TryExit(AICF_TransportTrip t,IEntity a,string reason,AICF_ReboardSeatConflictScope scope){Calls++;if(OnExit!=null)OnExit(t);if(UseActualExit)return AICF_ExitProduction.TryExit(t,a,reason,scope);return ExitAccepted;}}
public class AICF_TripOutcome:NativeObject{public string Reason;public static AICF_TripOutcome FallbackToFoot(string r,string c){return new AICF_TripOutcome{Reason=r};}public static AICF_TripOutcome Wait(string r,string c){return new AICF_TripOutcome{Reason=r};}}
public static class AICF_GroupTrace{public static string Entity(IEntity a){return a.GetID().ToString();}}
public class AICF_AIQuotaVehicleDispatch:NativeObject{public static bool Allowed=true,Current=true;public static int Begins,Cancels,Observed,Marked;public static Action OnBegin,OnMark;public IEntity Actor;public Vehicle Vehicle;public BaseCompartmentSlot Seat;public string ActorRpl;public bool Ended;public static AICF_AIQuotaVehicleDispatch Begin(IEntity a,Vehicle v,BaseCompartmentSlot s){Begins++;if(OnBegin!=null)OnBegin();return Allowed?new AICF_AIQuotaVehicleDispatch{Actor=a,Vehicle=v,Seat=s,ActorRpl=a.Rpl}:null;}public bool IsCurrent(){return !Ended&&Current&&Actor&&Actor.Current&&Vehicle&&Seat&&Actor.Rpl==ActorRpl;}public bool Matches(IEntity a,Vehicle v,BaseCompartmentSlot s){return Actor==a&&Vehicle==v&&Seat==s&&IsCurrent();}public void MarkDispatched(){Marked++;if(OnMark!=null)OnMark();}public void ObserveNativeResult(bool result){Observed++;}public void Cancel(){if(!Ended){Ended=true;Cancels++;}}public static void Reset(){Allowed=true;Current=true;Begins=Cancels=Observed=Marked=0;OnBegin=OnMark=null;}}
public class AICF_VehicleShortSeatAttempt:NativeObject{public bool Commit(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s,int now){return true;}}
public static class AICF_Stage3Diagnostics{public static void Info(string e,string d){}public static void Warning(string e,string d){}}
public static class AICF_VehicleBehaviorPolicy{public static AICF_VehicleBehaviorBinding Binding;public static AICF_VehicleBehaviorBinding Find(Vehicle v){return Binding!=null&&Binding.m_Vehicle==v?Binding:null;}public static bool ShouldDeferCrewReplacement(Vehicle v){return false;}public static bool SuppressGetIn(SCR_AIUtilityComponent utility,SCR_AIMessage_GetIn message) SUPPRESS public static bool IsCurrentReboarding(AICF_VehicleBehaviorBinding binding) PREFLIGHT }
SCOPE
public static class AICF_ConflictProduction{
 public static bool IsCurrent(AICF_TransportTrip t){return t.Current;}
 public static AICF_VehicleBehaviorAction FindAction(AICF_TransportTrip t,IEntity a){return t.State.m_aActions.Find(x=>x.m_Actor==a);}
 public static void ReportSeatConflict(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s,string trigger){}
 public static AICF_TripOutcome RecoverOwnSeatConflict(AICF_TransportTrip trip,NativeArray<BaseCompartmentSlot> seats,IEntity expected,BaseCompartmentSlot blocked,string causationId) RECOVER
 public static AICF_TripOutcome PollPending(AICF_TransportTrip trip){AICF_VehicleBehaviorState state=trip.State;string causationId="cause"; PENDING return null;}
 public static void CancelEntries(AICF_TransportTrip trip) CANCEL
 public static bool EntryCanReachDispatch(AICF_TransportTrip trip,IEntity actor,BaseCompartmentSlot seat){ENTRY_PREFIX return true;}
}
public static class AICF_ExitProduction{
 public const float MAX_ACTOR_DISPLACEMENT_METERS=25;
 public static bool IsCurrentMember(AICF_TransportTrip t,IEntity a){return AICF_VehicleShortRecovery.IsCurrentMember(t,a);}
 public static bool IsSlowVehicle(Vehicle v){return v.Slow;}
 public static bool CanSeat(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s){return t&&t.Current&&a&&s;}
 public static bool IsIncapacitatedSeatCurrent(AICF_TransportTrip t,BaseCompartmentSlot s){return t&&t.Current&&s&&s.Occupant;}
 public static bool TryExit(AICF_TransportTrip trip,IEntity member,string reason,AICF_ReboardSeatConflictScope conflict=null) EXIT
 public static bool TrySeat(AICF_TransportTrip trip,IEntity member,BaseCompartmentSlot seat,string reason,AICF_AIQuotaVehicleDispatch admitted=null,AICF_VehicleShortSeatAttempt attempt=null) SEATBODY
 public static bool TryClearIncapacitatedSeat(AICF_TransportTrip trip,BaseCompartmentSlot seat,string reason) CLEARBODY
 public static bool UnsafeExitLog(AICF_TransportTrip trip,IEntity member,string reason,AICF_ReboardSeatConflictScope conflict=null) UNSAFEEXITLOG
 public static bool UnsafeSeatLog(AICF_TransportTrip trip,IEntity member,BaseCompartmentSlot seat,string reason,AICF_AIQuotaVehicleDispatch admitted=null,AICF_VehicleShortSeatAttempt attempt=null) UNSAFESEATLOG
 public static bool UnsafeClearLog(AICF_TransportTrip trip,BaseCompartmentSlot seat,string reason) UNSAFECLEARLOG
}
public static class AICF_ConflictFixture{
 public static NativeArray<BaseCompartmentSlot> Seats;
 public static AICF_TransportTrip Create(){var t=new AICF_TransportTrip();t.Slot.Group=t.Assignment.Group;Seats=new NativeArray<BaseCompartmentSlot>();for(int i=0;i<5;i++){var a=new IEntity{Id=i+1,Rpl="rpl"+i};a.Control.Agent.Controlled=a;a.Control.Agent.Utility.m_OwnerEntity=a;t.Roster[i]=a;t.Preset.m_aMembers.Add(i);Seats.Add(new BaseCompartmentSlot{Vehicle=t.Lease.Vehicle,Key="seat"+i});}Seats[0].Reserved=t.Roster[1];Seats[0].Occupant=t.Roster[1];t.Roster[1].Access.Seat=Seats[0];t.Roster[1].Access.Vehicle=t.Lease.Vehicle;t.Binding=new AICF_VehicleBehaviorBinding{m_Trip=t,m_Vehicle=t.Lease.Vehicle};AICF_VehicleBehaviorPolicy.Binding=t.Binding;AICF_VehicleShortRecovery.Calls=0;AICF_VehicleShortRecovery.OnExit=null;AICF_VehicleShortRecovery.UseActualExit=false;return t;}
 public static AICF_ReboardSeatConflictScope Capture(AICF_TransportTrip t){var s=AICF_ReboardSeatConflictScope.Capture(t,Seats,Seats[0],t.Roster[0]);t.State.m_ReboardSeatConflict=s;return s;}
 public static AICF_TripOutcome Recover(AICF_TransportTrip t){return AICF_ConflictProduction.RecoverOwnSeatConflict(t,Seats,t.Roster[0],Seats[0],"cause");}
}
'@
$compiled=$code.Replace(' PREFLIGHT',' '+$preflight).Replace(' SUPPRESS',' '+$suppress).Replace('SCOPE',$scopeCode).Replace(' RECOVER',' '+$recover).Replace(' EXIT',' '+$exit).Replace(' SEATBODY',' '+$seat).Replace(' CLEARBODY',' '+$clear).Replace(' PENDING',' '+$pending).Replace(' CANCEL',' '+$cancel).Replace('ENTRY_PREFIX',$entryPrefix)
$compiled=$compiled.Replace(' UNSAFEEXITLOG',' '+$unsafeExitLog).Replace(' UNSAFESEATLOG',' '+$unsafeSeatLog).Replace(' UNSAFECLEARLOG',' '+$unsafeClearLog)
$mutants=@(
 @('Budget',$recover.Replace('token.m_iShortAttempts >= 3','false')),
 @('Pending',$recover.Replace('access.IsGettingOut() || access.IsGettingIn() || access.IsSwitchingSeatsAnim()','Clock.Now < 0').Replace('!scope.CanBeginOwnedExit()','Clock.Now < 0')),
 @('Reentry',$recover.Replace('!scope.IsCurrent()','false'))
)

foreach($m in $mutants){
 $name='AICF_ConflictUnsafe'+$m[0]
 $wrapper='public static class '+$name+'{public static bool IsCurrent(AICF_TransportTrip t){return t.Current;}public static AICF_VehicleBehaviorAction FindAction(AICF_TransportTrip t,IEntity a){return AICF_ConflictProduction.FindAction(t,a);}public static void ReportSeatConflict(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s,string trigger){}public static AICF_TripOutcome Run(AICF_TransportTrip trip,NativeArray<BaseCompartmentSlot> seats,IEntity expected,BaseCompartmentSlot blocked,string causationId)'+$m[1]+'}'
 $compiled+=$wrapper

}
$liveEntryUnsafe=[regex]::Replace($scopeCode,'if \(HasLiveWrongSeatEntry\(\)\)\s*\{\s*if \(!Is(?:AfterExit|ExitedActor)Current\(\)\) return -1;\s*return 0;\s*\}','if (Clock.Now < 0) return 0;').Replace('!IsAfterExitCurrent() || HasLiveWrongSeatEntry()','!IsAfterExitCurrent()')
$reservationMutants=@(
 @('Captured',$scopeCode.Replace('!m_bCapturedOwnReservation','Clock.Now < 0')),
 @('ChangedOwner',$scopeCode.Replace('!m_BlockedSeat.IsReservedBy(m_Occupant)','Clock.Now < 0')),
 @('LiveEntry',$liveEntryUnsafe)
)
foreach($m in $reservationMutants){$compiled+=$m[1].Replace('AICF_ReboardSeatConflictScope',('AICF_ReservationUnsafe'+$m[0]))}

Add-Type -TypeDefinition $compiled
$cases=0
function Check([bool]$ok,[string]$name){if(!$ok){throw "Actual conflict body: $name"};$script:cases++}
$t=[AICF_ConflictFixture]::Create();$s=[AICF_ConflictFixture]::Capture($t);Check ($null -ne $s) 'own original occupies other original seat'
$invalid=@(
 @('foreign faction',{param($t)$t.Roster[1].Affiliation.Faction.Key='US'}),
 @('expected foreign faction',{param($t)$t.Roster[0].Affiliation.Faction.Key='US'}),
 @('unknown faction',{param($t)$t.Roster[1].Affiliation=$null}),
 @('foreign human/dead/controller delegate',{param($t)$t.Roster[1].Current=$false}),
 @('expected stale delegate',{param($t)$t.Roster[0].Current=$false}),
 @('stale binding',{param($t)$t.Binding.Current=$false}),
 @('trip deadline',{param($t)$t.Expired=$true}),
 @('original reboard deadline',{param($t)$t.State.m_iReboardDeadline=1000}),
 @('not reboarding',{param($t)$t.State.m_bReboarding=$false}),
 @('wrong phase',{param($t)$t.Phase=[AICF_ETransportTripPhase]::TRANSIT}),
 @('assignment revision',{param($t)$t.Slot.Revision++}),
 @('intent revision',{param($t)$t.Slot.Intent++}),
 @('other group',{param($t)$t.Slot.Group=New-Object SCR_AIGroup}),
 @('absent group slot',{param($t)$t.Slot=$null}),
 @('occupant not original',{param($t)$t.Roster[1]=$null}),
 @('unknown rpl',{param($t)$t.Roster[1].Rpl='NONE'}),
 @('empty rpl',{param($t)$t.Roster[0].Rpl=''}),
 @('missing own assignment',{param($t)$t.Preset.m_aMembers[1]=-1}),
 @('speed above guard',{param($t)$t.Lease.Vehicle.Slow=$false}),
 @('burning car',{param($t)$t.Lease.Vehicle.Fire=$true}),
 @('immobile car',{param($t)$t.Lease.Vehicle.Movable=$false}),
 @('foreign current vehicle',{param($t)$t.Roster[1].Access.Vehicle=New-Object Vehicle}),
 @('wrong current compartment',{param($t)$t.Roster[1].Access.Seat=[AICF_ConflictFixture]::Seats[1]}),
 @('not in compartment',{param($t)$t.Roster[1].Access.Inside=$false})
)
foreach($test in $invalid){$t=[AICF_ConflictFixture]::Create();& $test[1] $t;Check ($null -eq [AICF_ConflictFixture]::Capture($t)) $test[0]}
foreach($test in @(
 @('actor ID',{param($t)$t.Roster[1].Id++}),@('expected ID',{param($t)$t.Roster[0].Id++}),
 @('occupant Rpl',{param($t)$t.Roster[1].Rpl='changed'}),@('expected Rpl',{param($t)$t.Roster[0].Rpl='changed'}),
 @('assignment pointer',{param($t)$t.Assignment=New-Object AICF_StrategicAssignmentSnapshot}),
 @('mapping',{param($t)$t.Preset.m_aMembers[0]=1}),@('seat key',{param($t)[AICF_ConflictFixture]::Seats[0].Key='changed'}),
 @('seat owner',{param($t)[AICF_ConflictFixture]::Seats[0].Vehicle=New-Object Vehicle}),
 @('deadline renewal',{param($t)$t.State.m_iReboardDeadline++}),@('occupant changed',{param($t)[AICF_ConflictFixture]::Seats[0].Occupant=$t.Roster[2]})
 ,@('assignment mutated together',{param($t)$t.Slot.Revision++;$t.Assignment.Revision++})
 ,@('intent mutated together',{param($t)$t.Slot.Intent++;$t.Assignment.Intent++})
 ,@('binding replacement',{param($t)[AICF_VehicleBehaviorPolicy]::Binding=New-Object AICF_VehicleBehaviorBinding})
)){$t=[AICF_ConflictFixture]::Create();$s=[AICF_ConflictFixture]::Capture($t);& $test[1] $t;Check (!$s.IsCurrent()) ('repeat '+$test[0])}
foreach($test in @(
 @('exact transport',$null,$true),
 @('stale assignment',{param($t)$t.Slot.Revision++},$false),
 @('stale intent',{param($t)$t.Slot.Intent++},$false),
 @('expired reboard',{param($t)$t.State.m_iReboardDeadline=1000},$false),
 @('expired trip',{param($t)$t.Expired=$true},$false),
 @('not reboard',{param($t)$t.State.m_bReboarding=$false},$false),
 @('transit',{param($t)$t.Phase=[AICF_ETransportTripPhase]::TRANSIT},$false),
 @('combat car',{param($t)$t.Preset.m_iBehavior=[AICF_EVehicleBehavior]::COMBAT},$false),
 @('foreign actor',{param($t)$t.Roster[1].Current=$false},$false)
)){
 $t=[AICF_ConflictFixture]::Create();$utility=New-Object SCR_AIUtilityComponent;$utility.m_OwnerEntity=$t.Roster[1];$message=New-Object SCR_AIMessage_GetIn;$message.m_Vehicle=$t.Lease.Vehicle
 if($test[1]){& $test[1] $t};Check ([AICF_VehicleBehaviorPolicy]::SuppressGetIn($utility,$message) -eq $test[2]) ('actual native message policy '+$test[0])
}
$t=[AICF_ConflictFixture]::Create();$result=[AICF_ConflictFixture]::Recover($t);Check ($result.Reason -eq 'TACTICAL_OWN_SEAT_CLEARANCE_PENDING' -and [AICF_VehicleShortRecovery]::Calls -eq 1 -and $t.State.m_aActions[0].m_Actor -eq $t.Roster[1] -and $t.State.m_aActions[0].m_iShortAttempts -eq 1) 'new occupant token uses own shared budget'
$t=[AICF_ConflictFixture]::Create();$pilot=New-Object AICF_VehicleBehaviorAction($t,$t.Roster[0],[AICF_ConflictFixture]::Seats[0]);$pilot.m_iShortAttempts=2;$cargo=New-Object AICF_VehicleBehaviorAction($t,$t.Roster[1],[AICF_ConflictFixture]::Seats[1]);$cargo.m_iShortAttempts=2;$far=New-Object AICF_VehicleBehaviorAction($t,$t.Roster[4],[AICF_ConflictFixture]::Seats[4]);$t.State.m_aActions.Add($pilot);$t.State.m_aActions.Add($cargo);$t.State.m_aActions.Add($far)
$result=[AICF_ConflictFixture]::Recover($t);Check ($cargo.m_iShortAttempts -eq 3 -and $pilot.m_iShortAttempts -eq 2 -and $pilot.Retired -eq 0 -and $far.Retired -eq 0 -and $t.State.m_iReboardDeadline -eq 9000) 'no other actor cancellation or deadline/budget reset'
$result=[AICF_ConflictFixture]::Recover($t);Check ($result.Reason -eq 'TACTICAL_OWN_SEAT_SCOPE_CHANGED' -and [AICF_VehicleShortRecovery]::Calls -eq 1 -and $cargo.m_iShortAttempts -eq 3) 'pending receipt prevents a repeated recovery batch'
$t.State.m_ReboardSeatConflict=$null
$result=[AICF_ConflictFixture]::Recover($t);Check ($null -eq $result -and [AICF_VehicleShortRecovery]::Calls -eq 1 -and $cargo.m_iShortAttempts -eq 3) 'exhausted same token fails after receipt retirement without resetting budget'
foreach($pending in @('Entering','Exiting','Switching')){$t=[AICF_ConflictFixture]::Create();$t.Roster[1].Access.$pending=$true;$result=[AICF_ConflictFixture]::Recover($t);Check ($result.Reason -eq 'TACTICAL_OWN_SEAT_NATIVE_TRANSITION_PENDING' -and [AICF_VehicleShortRecovery]::Calls -eq 0 -and $t.State.m_aActions.Count -eq 0) ('native pending '+$pending)}
foreach($change in @('Scope','Pending','TokenRemoved','Owner')){
 $t=[AICF_ConflictFixture]::Create();$cargo=New-Object AICF_VehicleBehaviorAction($t,$t.Roster[1],[AICF_ConflictFixture]::Seats[1]);$t.State.m_aActions.Add($cargo)
 switch($change){
  Scope {$cargo.OnRetire=[Action[AICF_VehicleBehaviorAction]]{param($a)$a.Trip.Slot.Revision++}}
  Pending {$cargo.OnRetire=[Action[AICF_VehicleBehaviorAction]]{param($a)$a.m_Actor.Access.Exiting=$true}}
  TokenRemoved {$cargo.OnRetire=[Action[AICF_VehicleBehaviorAction]]{param($a)$a.Trip.State.m_aActions.Clear()}}
  Owner {$cargo.OnRetire=[Action[AICF_VehicleBehaviorAction]]{param($a)$a.Owner=$false}}
 }
 $result=[AICF_ConflictFixture]::Recover($t);Check ($null -ne $result -and [AICF_VehicleShortRecovery]::Calls -eq 0 -and $cargo.m_iShortAttempts -eq 0) ('reentrant retirement '+$change)
}
foreach($change in @('stop','lease','assignment')){
 $t=[AICF_ConflictFixture]::Create()
 switch($change){
  stop {[AICF_VehicleShortRecovery]::OnExit=[Action[AICF_TransportTrip]]{param($x)$x.Current=$false;$x.Binding.Current=$false}}
  lease {[AICF_VehicleShortRecovery]::OnExit=[Action[AICF_TransportTrip]]{param($x)$x.Lease=$null}}
  assignment {[AICF_VehicleShortRecovery]::OnExit=[Action[AICF_TransportTrip]]{param($x)$x.Assignment=$null}}
 }
 $result=[AICF_ConflictFixture]::Recover($t)
 Check ($result.Reason -eq 'TACTICAL_OWN_SEAT_SCOPE_CHANGED' -and [AICF_VehicleShortRecovery]::Calls -eq 1) ('actual GetOut return invalidates flow context '+$change)
}
function Set-ActualExited([AICF_TransportTrip]$t){
 [AICF_ConflictFixture]::Seats[0].Occupant=$null
 $t.Roster[1].Access.Seat=$null;$t.Roster[1].Access.Vehicle=$null;$t.Roster[1].Access.Inside=$false
}
foreach($test in @(
 @('captured original reservation cleared',$null,1,1),
 @('native reservation already gone',{param($t,$scope)[AICF_ConflictFixture]::Seats[0].Reserved=$null},1,0),
 @('other owner now',{param($t,$scope)[AICF_ConflictFixture]::Seats[0].Reserved=$t.Roster[2]},-1,0),
 @('unknown at original capture',{param($t,$scope)$scope.m_bCapturedOwnReservation=$false},-1,0),
 @('stale preset/lease delegate',{param($t,$scope)$t.Binding.Current=$false},-1,0),
 @('new epoch',{param($t,$scope)$t.Assignment.Revision++;$t.Slot.Revision++},-1,0),
 @('phase ABA',{param($t,$scope)$t.Transitions+=2},-1,0),
 @('new behavior owner',{param($t,$scope)$t.State=New-Object AICF_VehicleBehaviorState},-1,0),
 @('deadline expired',{param($t,$scope)$t.State.m_iReboardDeadline=1000},-1,0),
 @('dead/player/foreign original',{param($t,$scope)$t.Roster[1].Current=$false},-1,0),
 @('component Rpl changed',{param($t,$scope)$t.Roster[1].Rpl='new'},-1,0),
 @('human/foreign current group delegate',{param($t,$scope)$t.Current=$false},-1,0),
 @('entering pending',{param($t,$scope)$t.Roster[1].Access.Entering=$true},0,0),
 @('exiting pending',{param($t,$scope)$t.Roster[1].Access.Exiting=$true},0,0),
 @('switch pending',{param($t,$scope)$t.Roster[1].Access.Switching=$true},0,0),
 @('foreign actual car',{param($t,$scope)$t.Roster[1].Access.Vehicle=New-Object Vehicle},-1,0),
 @('unknown occupancy new actor',{param($t,$scope)[AICF_ConflictFixture]::Seats[0].Occupant=$t.Roster[2]},-1,0),
 @('native queue read changes epoch',{param($t,$scope)$t.Roster[1].Control.Agent.Utility.OnGetActions=[Action]{$t.Slot.Revision++}.GetNewClosure()},-1,0),
 @('pending replaced on native queue read',{param($t,$scope)$t.Roster[1].Control.Agent.Utility.OnGetActions=[Action]{$t.State.m_ReboardSeatConflict=New-Object NativeObject}.GetNewClosure()},-1,0),
 @('pending cleared on native queue read',{param($t,$scope)$t.Roster[1].Control.Agent.Utility.OnGetActions=[Action]{$t.State.m_ReboardSeatConflict=$null}.GetNewClosure()},-1,0),
 @('pending replaced on clear callback',{param($t,$scope)[AICF_ConflictFixture]::Seats[0].OnReservationWrite=[Action]{$t.State.m_ReboardSeatConflict=New-Object NativeObject}.GetNewClosure()},-1,1),
 @('pending cleared on clear callback',{param($t,$scope)[AICF_ConflictFixture]::Seats[0].OnReservationWrite=[Action]{$t.State.m_ReboardSeatConflict=$null}.GetNewClosure()},-1,1),
 @('reservation mutation callback stops owner',{param($t,$scope)[AICF_ConflictFixture]::Seats[0].OnReservationWrite=[Action]{$t.Binding.Current=$false;$t.Lease=$null}.GetNewClosure()},-1,1)
)){
 $t=[AICF_ConflictFixture]::Create();$s=[AICF_ConflictFixture]::Capture($t);Set-ActualExited $t
 if($test[1]){& $test[1] $t $s}
 $status=$s.ObserveExitedReservation()
 Check ($status -eq $test[2] -and [AICF_ConflictFixture]::Seats[0].ReservationWrites -eq $test[3]) ('actual captured reservation '+$test[0])
}
foreach($state in @('RUNNING','EVALUATED','COMPLETED','FAILED','REMOVED')){
 $t=[AICF_ConflictFixture]::Create();$s=[AICF_ConflictFixture]::Capture($t);Set-ActualExited $t
 $u=$t.Roster[1].Control.Agent.Utility;$entry=New-Object SCR_AIGetInVehicle;$entry.m_Utility=$u;$entry.m_Vehicle.m_Value=$t.Lease.Vehicle;$entry.m_CompartmentToGetIn.m_Value=[AICF_ConflictFixture]::Seats[0]
 if($state-eq'REMOVED'){$entry.Remove=$true}else{$entry.State=[EAIActionState]::$state};$u.Actions.Add($entry)
 $live=$state-in@('RUNNING','EVALUATED');$expected=1;if($live){$expected=0}
 Check ($s.ObserveExitedReservation() -eq $expected -and [AICF_ConflictFixture]::Seats[0].ReservationWrites -eq [int](!$live)) ('actual wrong-seat native entry '+$state)
}
foreach($where in @('GetActions','SetReserved')){
 $t=[AICF_ConflictFixture]::Create();$s=[AICF_ConflictFixture]::Capture($t);Set-ActualExited $t;$successor=New-Object AICF_ReboardSeatConflictScope
 $replace=[Action]{$t.State.m_ReboardSeatConflict=$successor}.GetNewClosure()
 if($where-eq'GetActions'){$t.Roster[1].Control.Agent.Utility.OnGetActions=$replace}else{[AICF_ConflictFixture]::Seats[0].OnReservationWrite=$replace}
 $result=[AICF_ConflictProduction]::PollPending($t)
 Check ($result.Reason-eq'TACTICAL_OWN_SEAT_SCOPE_CHANGED'-and$t.State.m_ReboardSeatConflict-eq$successor) ('actual Poll preserves successor after '+$where)
}
foreach($test in @(
 @('exact occupied own reservation',$null,$true,1,1),
 @('already entering',{param($t,$s)$t.Roster[1].Access.Entering=$true},$false,0,0),
 @('already exiting',{param($t,$s)$t.Roster[1].Access.Exiting=$true},$false,0,0),
 @('already switching',{param($t,$s)$t.Roster[1].Access.Switching=$true},$false,0,0),
 @('no original owner proof',{param($t,$s)$s.m_bCapturedOwnReservation=$false},$false,0,0),
 @('unknown new owner',{param($t,$s)[AICF_ConflictFixture]::Seats[0].Reserved=$t.Roster[2]},$false,0,0),
 @('fresh correct Pilot reservation preserved',{param($t,$s)[AICF_ConflictFixture]::Seats[0].Reserved=$t.Roster[0]},$true,0,1),
 @('lease changed on clear',{param($t,$s)[AICF_ConflictFixture]::Seats[0].OnReservationWrite=[Action]{$t.Lease=$null}.GetNewClosure()},$false,1,0),
 @('intent changed on clear',{param($t,$s)[AICF_ConflictFixture]::Seats[0].OnReservationWrite=[Action]{$t.Assignment.Intent++;$t.Slot.Intent++}.GetNewClosure()},$false,1,0),
 @('actor moved past short bound on clear',{param($t,$s)[AICF_ConflictFixture]::Seats[0].OnReservationWrite=[Action]{$t.Roster[1].Origin.Value=100}.GetNewClosure()},$false,1,0),
 @('successor receipt on clear',{param($t,$s)[AICF_ConflictFixture]::Seats[0].OnReservationWrite=[Action]{$t.State.m_ReboardSeatConflict=New-Object AICF_ReboardSeatConflictScope}.GetNewClosure()},$false,1,0),
 @('new correct Pilot reservation on clear',{param($t,$s)[AICF_ConflictFixture]::Seats[0].OnReservationWrite=[Action]{[AICF_ConflictFixture]::Seats[0].Reserved=$t.Roster[0]}.GetNewClosure()},$false,1,0),
 @('Cargo legitimately remapped to Pilot on clear',{param($t,$s)[AICF_ConflictFixture]::Seats[0].OnReservationWrite=[Action]{$t.Preset.m_aMembers[0]=1;$t.Preset.m_aMembers[1]=0;[AICF_ConflictFixture]::Seats[0].Reserved=$t.Roster[1]}.GetNewClosure()},$false,1,0),
 @('binding removed during native queue snapshot',{param($t,$s)$t.Roster[1].Control.Agent.Utility.OnGetActions=[Action]{$s.m_Binding=$null;[AICF_VehicleBehaviorPolicy]::Binding=$null}.GetNewClosure()},$false,0,0)
)){
 $t=[AICF_ConflictFixture]::Create();$s=[AICF_ConflictFixture]::Capture($t);$t.State.m_ReboardSeatConflict=$null
 if($test[1]){& $test[1] $t $s}
 $ok=[AICF_ExitProduction]::TryExit($t,$t.Roster[1],'atomic-test',$s)
 Check ($ok-eq$test[2]-and[AICF_ConflictFixture]::Seats[0].ReservationWrites-eq$test[3]-and$t.Roster[1].Access.PhysicalExits-eq$test[4]) ('actual atomic pre-exit '+$test[0])
 if($test[0]-eq'new correct Pilot reservation on clear'){Check ([AICF_ConflictFixture]::Seats[0].Reserved-eq$t.Roster[0]) 'legitimate newly reserved Pilot is not erased'}
 if($test[0]-eq'Cargo legitimately remapped to Pilot on clear'){Check ([AICF_ConflictFixture]::Seats[0].Reserved-eq$t.Roster[1]) 'legitimate new DriverRole survives stale exit decision'}
}
# Реальные Recover -> TryExit -> pre-clear; затем реальный CancelEntries.
# Native GetOut adapter ещё не убрал occupant: отмена попадает в тот же gap.
$t=[AICF_ConflictFixture]::Create();[AICF_VehicleShortRecovery]::UseActualExit=$true
$result=[AICF_ConflictFixture]::Recover($t);$token=$t.State.m_aActions[0]
Check ($result.Reason-eq'TACTICAL_OWN_SEAT_CLEARANCE_PENDING'-and$token.m_iShortAttempts-eq1-and$t.Roster[1].Access.PhysicalExits-eq1-and![AICF_ConflictFixture]::Seats[0].IsReserved()-and[AICF_ConflictFixture]::Seats[0].Occupant-eq$t.Roster[1]) 'accepted native exit already released only its own reservation'
[AICF_ConflictProduction]::CancelEntries($t)
Set-ActualExited $t
$t.State.m_bReboarding=$false;$t.State.m_bReboarding=$true;$t.State.m_iReboardDeadline+=1000
Check ($null-eq$t.State.m_ReboardSeatConflict-and[AICF_ConflictProduction]::EntryCanReachDispatch($t,$t.Roster[0],[AICF_ConflictFixture]::Seats[0])-and$token.m_iShortAttempts-eq1) 'actual cancellation -> later reboard reaches entry without losing reserved-seat cleanup or resetting old token budget'
foreach($change in @('unchanged','assignment','occupant','vehicleSpeed','originalRoster','actorRpl','nativeSeat')){
 $t=[AICF_ConflictFixture]::Create();$s=[AICF_ConflictFixture]::Capture($t);$actor=$t.Roster[1]
 switch($change){
  assignment {$actor.Access.OnInterrupt=[Action]{$t.Slot.Revision++}.GetNewClosure()}
  occupant {$actor.Access.OnInterrupt=[Action]{[AICF_ConflictFixture]::Seats[0].Occupant=$t.Roster[2]}.GetNewClosure()}
  vehicleSpeed {$actor.Access.OnInterrupt=[Action]{$t.Lease.Vehicle.Slow=$false}.GetNewClosure()}
  originalRoster {$actor.Access.OnInterrupt=[Action]{$t.Roster[1]=$null}.GetNewClosure()}
  actorRpl {$actor.Access.OnInterrupt=[Action]{$actor.Rpl='replaced'}.GetNewClosure()}
  nativeSeat {$actor.Access.OnInterrupt=[Action]{$actor.Access.Seat=[AICF_ConflictFixture]::Seats[1]}.GetNewClosure()}
 }
 $t.State.m_ReboardSeatConflict=$null
 $accepted=[AICF_ExitProduction]::TryExit($t,$actor,'test',$s)
 Check ($accepted -eq ($change -eq 'unchanged') -and $actor.Access.PhysicalExits -eq [int]$accepted -and $actor.Access.Interrupts -eq 1) ('actual TryExit after Interrupt '+$change)
}
$t=[AICF_ConflictFixture]::Create();Check ([AICF_ExitProduction]::TryExit($t,$t.Roster[1],'legacy')) 'legacy3argument caller unchanged'
foreach($distance in @(25,25.001)){$t=[AICF_ConflictFixture]::Create();$t.Roster[1].Access.Distance=$distance;$s=[AICF_ConflictFixture]::Capture($t);$t.State.m_ReboardSeatConflict=$null;$ok=[AICF_ExitProduction]::TryExit($t,$t.Roster[1],'test',$s);Check ($ok -eq ($distance -le 25)) ('actual short exit boundary '+$distance)}
$t=[AICF_ConflictFixture]::Create();[ChimeraWorldUtils]::Water=$true;Check (![AICF_ExitProduction]::TryExit($t,$t.Roster[1],'water',[AICF_ConflictFixture]::Capture($t))) 'water veto';[ChimeraWorldUtils]::Water=$false
# Полные реальные native-call bodies; только upstream CanSeat/incapacitated
# precondition — adapter. Native callback удаляет identity ДО logging tail.
foreach($kind in @('Exit','Seat','Incapacitated')){
 foreach($deleted in @('actor','seat','trip','all')){
  $t=[AICF_ConflictFixture]::Create();$actor=$t.Roster[1];$seat=[AICF_ConflictFixture]::Seats[0];$access=$actor.Access
  $callback=[Action]{
   if($deleted-in@('actor','all')){$actor.Deleted=$true;$actor.Current=$false}
   if($deleted-in@('seat','all')){$seat.Deleted=$true}
   if($deleted-in@('trip','all')){$t.Deleted=$true;$t.Current=$false;$t.Lease=$null}
  }.GetNewClosure()
  switch($kind){
   Exit {$access.OnGetOut=$callback;$ok=[AICF_ExitProduction]::TryExit($t,$actor,'callback-delete')}
   Seat {$access.OnGetIn=$callback;$ok=[AICF_ExitProduction]::TrySeat($t,$actor,$seat,'callback-delete')}
   Incapacitated {$access.OnGetOut=$callback;$ok=[AICF_ExitProduction]::TryClearIncapacitatedSeat($t,$seat,'callback-delete')}
  }
  Check ($ok-and($access.PhysicalExits+$access.PhysicalEntries)-eq1) ('actual native '+$kind+' immutable accepted log after '+$deleted+' deletion')
 }
}
$t=[AICF_ConflictFixture]::Create();$actor=$t.Roster[1];$seat=[AICF_ConflictFixture]::Seats[0];$actor.Access.EntryAccepted=$false
$actor.Access.OnGetIn=[Action]{$seat.Deleted=$true}.GetNewClosure()
Check (![AICF_ExitProduction]::TrySeat($t,$actor,$seat,'rejected-deleted-seat')) 'actual native rejected entry does not dereference deleted cleanup seat'
# Изменённые реальные bodies исполняются с теми же native adapters. Контроль
# должен показать нарушение конкретной границы, иначе unsafe mutant выжил.
$executedMutants=0
foreach($kind in @('Exit','Seat','Clear')){
 $t=[AICF_ConflictFixture]::Create();$actor=$t.Roster[1];$seat=[AICF_ConflictFixture]::Seats[0]
 $callback=[Action]{$actor.Deleted=$true;$actor.Current=$false;$seat.Deleted=$true;$t.Deleted=$true;$t.Current=$false;$t.Lease=$null}.GetNewClosure()
 $actor.Access.OnGetIn=$callback;$actor.Access.OnGetOut=$callback;$rejected=$false
 try{
  switch($kind){
   Exit {$null=[AICF_ExitProduction]::UnsafeExitLog($t,$actor,'unsafe-callback-delete',$null)}
   Seat {$null=[AICF_ExitProduction]::UnsafeSeatLog($t,$actor,$seat,'unsafe-callback-delete')}
   Clear {$null=[AICF_ExitProduction]::UnsafeClearLog($t,$seat,'unsafe-callback-delete')}
  }
 }catch{if($_.Exception.ToString()-match'after native callback|after actor delete|after native delete'){$rejected=$true}else{throw}}
 if(!$rejected){throw "Unsafe post-native getter logger survived actual callback deletion $kind"};$executedMutants++
}
foreach($m in $mutants){
 $name='AICF_ConflictUnsafe'+$m[0]
 $t=[AICF_ConflictFixture]::Create();$cargo=New-Object AICF_VehicleBehaviorAction($t,$t.Roster[1],[AICF_ConflictFixture]::Seats[1]);$t.State.m_aActions.Add($cargo)
 switch($m[0]){
  Budget {$cargo.m_iShortAttempts=3}
  Pending {$cargo.m_Actor.Access.Exiting=$true}
  Reentry {$cargo.OnRetire=[Action[AICF_VehicleBehaviorAction]]{param($a)$a.Trip.Slot.Revision++}}
 }
 $method=($name -as [type]).GetMethod('Run');$null=$method.Invoke($null,@($t,[AICF_ConflictFixture]::Seats,$t.Roster[0],[AICF_ConflictFixture]::Seats[0],'cause'))
 if([AICF_VehicleShortRecovery]::Calls -ne 1 -or $cargo.m_iShortAttempts -lt 1){throw "Unsafe mutation did not demonstrate forbidden exit/budget spend: $($m[0])"};$executedMutants++
}
foreach($m in $reservationMutants){
 $t=[AICF_ConflictFixture]::Create();$seats=[AICF_ConflictFixture]::Seats
 if($m[0]-eq'Captured'){$seats[0].Reserved=$t.Roster[2]}
 $type=('AICF_ReservationUnsafe'+$m[0])-as[type];$capture=$type.GetMethod('Capture');$s=$capture.Invoke($null,[object[]]@($t,$seats,$seats[0],$t.Roster[0]));$t.State.m_ReboardSeatConflict=$s;Set-ActualExited $t
 switch($m[0]){
  Captured {$seats[0].Reserved=$t.Roster[1]}
  ChangedOwner {$seats[0].Reserved=$t.Roster[2]}
  LiveEntry {$u=$t.Roster[1].Control.Agent.Utility;$entry=New-Object SCR_AIGetInVehicle;$entry.m_Utility=$u;$entry.m_Vehicle.m_Value=$t.Lease.Vehicle;$entry.m_CompartmentToGetIn.m_Value=$seats[0];$u.Actions.Add($entry)}
 }
 if($s.ObserveExitedReservation()-ne1-or$seats[0].ReservationWrites-ne1){throw "Reservation unsafe mutant did not demonstrate forbidden clear: $($m[0])"};$executedMutants++
}
"PASS Own wrong seat: actual_scope_policy_flow_exit=$cases source_controls=$sourceChecks executed_unsafe_mutants=$executedMutants native_physical=NOT_RUN"
