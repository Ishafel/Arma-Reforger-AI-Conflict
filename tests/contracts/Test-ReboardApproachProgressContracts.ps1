param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$token=[IO.File]::ReadAllText((Join-Path $root 'State/Vehicles/AICF_VehicleBehaviorAction.c'))
$flow=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleBehaviorFlow.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
$observe=Body $token 'ObserveEntryProgress'
$blocked=Body $token 'EntryBlocked'
$trace=Body $flow 'ReportReboarding'
$poll=Body $flow 'PollReboarding'
if(!$observe -or !$blocked.Contains('ObserveEntryProgress(m_Actor.GetOrigin(), m_Seat.GetPosition(), System.GetTickCount());') -or
 !$blocked.Contains('m_Seat.GetVehicle() != m_Vehicle') -or !$blocked.Contains('System.GetTickCount() - m_iLastProgressAt >= 2000')){throw 'Exact seat approach must drive existing bounded stall clock'}
if($observe -match 'CallLater|m_iRetries|m_iShortAttempts|Deadline|GetInVehicle|SetOrigin|SetWorldTransform'){throw 'Progress observation changes recovery policy'}
foreach($field in @('can_short_seat=','distance_m=','height_m=','speed_mps=','remaining_ms=','vehicle_rpl=','member_rpl=','lease_generation=','preset_revision=','accessible=','locked=','reserved_self=','occupant=','getting_in=','getting_out=','switching=','actual_seat=','native_target_accessible=','native_can_get_in=','short_attempts=','retries=','progress_age_ms=','selected_action=','executed_action=')){
 if(!$trace.Contains($field)){throw "Missing reboard diagnostic $field"}
}
if($trace.Contains('EntryBlocked(') -or $trace -match 'Retire\(|IssueEntry\(|TrySeat\(|m_iLastProgressAt\s*=|m_iShortAttempts\+\+'){throw 'Diagnostic must remain read-only'}
if(!$poll.Contains('ReportReboarding(trip, "DEADLINE");') -or !$poll.Contains('state.m_iNextReboardTraceAt = System.GetTickCount() + 5000;')){throw 'Missing bounded cadence or timeout snapshot'}
# Выполняется извлечённый production predicate: одинаковая текущая seat position
# для обеих actor positions исключает credit от одного движения машины.
$body=$observe.Replace('vector.Distance','Vec.Distance').Replace('vector ','Vec ').Replace('float ','double ')
$source=@'
using System;
public struct Vec { public double X,Y,Z; public Vec(double x,double y,double z){X=x;Y=y;Z=z;} public static double Distance(Vec a,Vec b){return Math.Sqrt((a.X-b.X)*(a.X-b.X)+(a.Y-b.Y)*(a.Y-b.Y)+(a.Z-b.Z)*(a.Z-b.Z));}}
public class Approach { public Vec m_vLastProgressPosition;public int m_iLastProgressAt=100;public bool Observe(Vec actorPosition,Vec seatPosition,int nowMs) BODY }
'@
$oldRule=$body.Replace('previousDistance - currentDistance','Vec.Distance(m_vLastProgressPosition, actorPosition)')
if($oldRule -ceq $body){throw 'Mutation target absent'}
$mutantSource='public class JitterMutant { public Vec m_vLastProgressPosition;public int m_iLastProgressAt=100;public bool Observe(Vec actorPosition,Vec seatPosition,int nowMs) '+$oldRule+' }'
Add-Type ($source.Replace('BODY',$body)+$mutantSource)
function V([double]$x,[double]$y=0,[double]$z=0){New-Object Vec($x,$y,$z)}
$checks=0
$p=New-Object Approach;$p.m_vLastProgressPosition=V 6
foreach($a in @((V 0 0 6),(V -6),(V 0 0 -6),(V 6),(V 6.5),(V 6))){if($p.Observe($a,(V 0),1000+$checks)){throw 'Circular jitter/retreat reset production progress clock'};$checks++}
if($p.m_iLastProgressAt -ne 100){throw 'Stall clock changed without approach'};$checks++
if($p.Observe((V 5.8),(V 0),2000)){throw 'Subthreshold progress credited'};$checks++
if(!$p.Observe((V 5.7),(V 0),2100) -or $p.m_iLastProgressAt -ne 2100){throw 'Cumulative physical approach not credited'};$checks++
foreach($seat in @((V 2),(V -8),(V 5.7 0 20))){if($p.Observe((V 5.7),$seat,2200)){throw 'Moving seat credited a stationary actor'};$checks++}
$p=New-Object Approach;$p.m_vLastProgressPosition=V 0 2
if(!$p.Observe((V 0 1.5),(V 0),3000)){throw 'Real vertical 3D approach ignored'};$checks++
# Мутация прежнего правила любой подвижки действительно ломает эту проверку.
$mutant=New-Object JitterMutant;$mutant.m_vLastProgressPosition=V 6
if(!$mutant.Observe((V 0 0 6),(V 0),4000)){throw 'Former jitter regression mutant unexpectedly survived'};$checks++
"PASS Reboard approach progress: extracted production predicate rejects circles, retreat and moving vehicle credit; real 3D approach retained; read-only bounded diagnostics; controls=$checks"
# Reproduce the observed ordinary-war gap with the actual EnsureEntry body:
# a current RUNNING action stalled70m away must use the existing native retry,
# while short seating remains unavailable and no remote physical move occurs.
$entryBody=Body $flow 'EnsureEntry'
$retryNamespace='AICFNativeRetry'+[guid]::NewGuid().ToString('N')
$retryHarness=@'
using System;using System.Collections.Generic;namespace RETRY_NAMESPACE {
public enum EAIActionState{RUNNING,FAILED,COMPLETED}
public static class System{public static int GetTickCount(){return 10000;}}
public static class vector{public static double Distance(double a,double b){return Math.Abs(a-b);}}
public static class AICF_GroupTrace{public static string Entity(IEntity a){return "actor";}}
public static class AICF_VehiclePreset{public static string SeatKey(BaseCompartmentSlot s){return "seat";}}
public static class AICF_Stage3Diagnostics{public static void Info(string name,string value){}}
public class IEntity{public bool Current=true,ShortSafe;public double GetOrigin(){return 70;}public static implicit operator bool(IEntity x){return x!=null;}}
public class BaseCompartmentSlot{public IEntity Occupant;public bool Reserved,Self=true;public object GetVehicle(){return this;}public double GetPosition(){return 0;}public IEntity GetOccupant(){return Occupant;}public bool IsReserved(){return Reserved;}public bool IsReservedBy(IEntity a){return Self;}public static implicit operator bool(BaseCompartmentSlot x){return x!=null;}}
public class NativeEntry{public EAIActionState State=EAIActionState.RUNNING;public bool Removed;public EAIActionState GetActionState(){return State;}public bool GetRemoveAction(){return Removed;}public static implicit operator bool(NativeEntry x){return x!=null;}}
public class TokenList:List<AICF_VehicleBehaviorAction>{public void Insert(AICF_VehicleBehaviorAction t){Add(t);}}
public class BehaviorState{public TokenList m_aActions=new TokenList();public int m_iReboardDeadline=100000;}
public class AICF_TransportTrip{public BehaviorState State=new BehaviorState();public BehaviorState GetBehaviorState(){return State;}public string GetOperationId(){return "trip";}}
public class AICF_VehicleBehaviorAction{public BaseCompartmentSlot m_Seat;public NativeEntry m_Entry=new NativeEntry();public int m_iShortAttempts,m_iRetries,Retired,m_iLastProgressAt;public bool Owner=true,Blocked=true,RestartSafe=true;public AICF_VehicleBehaviorAction(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s){m_Seat=s;}public bool ResolveOwner(){return Owner;}public bool EntryBlocked(){return Blocked;}public bool CanRestartEntry(){return RestartSafe;}public void Retire(){Retired++;m_Entry=null;}public static implicit operator bool(AICF_VehicleBehaviorAction x){return x!=null;}}
public class AICF_AIQuotaVehicleDispatch{public static bool Allowed=true;public static AICF_AIQuotaVehicleDispatch Begin(IEntity a,object v,BaseCompartmentSlot s){return Allowed?new AICF_AIQuotaVehicleDispatch():null;}public bool IsCurrent(){return Allowed;}public void Cancel(){}public static implicit operator bool(AICF_AIQuotaVehicleDispatch x){return x!=null;}}
public class AICF_VehicleShortSeatAttempt{public AICF_VehicleBehaviorAction Token;public int Captured;public static AICF_VehicleShortSeatAttempt ForBehavior(AICF_TransportTrip t,AICF_VehicleBehaviorAction token){return new AICF_VehicleShortSeatAttempt{Token=token,Captured=token.m_iShortAttempts};}public bool IsCurrent(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s,int now){return Token.Owner&&Token.m_iShortAttempts==Captured&&Captured<3;}public bool Commit(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s,int now){if(!IsCurrent(t,a,s,now))return false;Token.m_iShortAttempts++;return true;}public static implicit operator bool(AICF_VehicleShortSeatAttempt x){return x!=null;}}
public static class AICF_VehicleShortRecovery{public static int PhysicalCalls;public static bool IsCurrentMember(AICF_TransportTrip t,IEntity a){return a.Current;}public static bool CanSeat(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s){return a.ShortSafe;}public static bool TrySeat(AICF_TransportTrip t,IEntity a,BaseCompartmentSlot s,string reason,AICF_AIQuotaVehicleDispatch quota=null,AICF_VehicleShortSeatAttempt attempt=null){if(attempt!=null&&!attempt.Commit(t,a,s,10000))return false;PhysicalCalls++;return true;}}
public class Flow{public AICF_VehicleBehaviorAction Existing;public int NativeIssues;public AICF_VehicleBehaviorAction FindAction(AICF_TransportTrip t,IEntity a){return Existing;}public void IssueEntry(AICF_TransportTrip t,AICF_VehicleBehaviorAction token,IEntity a,BaseCompartmentSlot s){NativeIssues++;token.m_Entry=new NativeEntry();}public void EnsureEntry(AICF_TransportTrip trip,IEntity actor,BaseCompartmentSlot seat) ENTRY_BODY}
}
'@
Add-Type ($retryHarness.Replace('RETRY_NAMESPACE',$retryNamespace).Replace('ENTRY_BODY',$entryBody))
$retryCases=0
foreach($kind in @('runningStallFar','runningProgressFar','runningStallNear','nativeBudgetSpent','transition','nativeFailureFar','missingEntry','foreignReservation','staleActor','foreignSeat','ownerLost','missingToken','quotaVetoNear','quotaVetoFar')){
 $actor=[Activator]::CreateInstance([type]($retryNamespace+'.IEntity'))
 $seat=[Activator]::CreateInstance([type]($retryNamespace+'.BaseCompartmentSlot'))
 $trip=[Activator]::CreateInstance([type]($retryNamespace+'.AICF_TransportTrip'))
 $entryToken=[Activator]::CreateInstance([type]($retryNamespace+'.AICF_VehicleBehaviorAction'),@($trip,$actor,$seat))
 $model=[Activator]::CreateInstance([type]($retryNamespace+'.Flow'));$model.Existing=$entryToken
 $shortType=[type]($retryNamespace+'.AICF_VehicleShortRecovery');$shortType.GetField('PhysicalCalls').SetValue($null,0)
 $quotaType=[type]($retryNamespace+'.AICF_AIQuotaVehicleDispatch');$quotaType.GetField('Allowed').SetValue($null,$true)
 switch($kind){
  runningProgressFar{$entryToken.Blocked=$false}runningStallNear{$actor.ShortSafe=$true}nativeBudgetSpent{$entryToken.m_iRetries=2}
  transition{$entryToken.RestartSafe=$false}nativeFailureFar{$entryToken.m_Entry.State=[Enum]::Parse([type]($retryNamespace+'.EAIActionState'),'FAILED')}
  missingEntry{$entryToken.m_Entry=$null}foreignReservation{$seat.Reserved=$true;$seat.Self=$false}staleActor{$actor.Current=$false}
  foreignSeat{$entryToken.m_Seat=[Activator]::CreateInstance([type]($retryNamespace+'.BaseCompartmentSlot'))}ownerLost{$entryToken.Owner=$false}missingToken{$model.Existing=$null}
  quotaVetoNear{$actor.ShortSafe=$true;$quotaType.GetField('Allowed').SetValue($null,$false)}quotaVetoFar{$quotaType.GetField('Allowed').SetValue($null,$false)}
 }
 $model.EnsureEntry($trip,$actor,$seat)
 $expectedNative=[int]($kind -in @('runningStallFar','nativeFailureFar','missingEntry','missingToken'))
 $expectedPhysical=[int]($kind -eq 'runningStallNear')
 if($model.NativeIssues -ne $expectedNative -or $shortType.GetField('PhysicalCalls').GetValue($null) -ne $expectedPhysical){throw "Actual EnsureEntry retry regression kind=$kind native=$($model.NativeIssues) expected=$expectedNative"}
 if($kind -eq 'runningStallFar' -and ($entryToken.m_iRetries -ne 1 -or $entryToken.m_iShortAttempts -ne 0)){throw 'RUNNING far stall did not consume exactly one native-only retry'}
 if($kind-in@('quotaVetoNear','quotaVetoFar')-and($entryToken.m_iRetries-ne0-or$entryToken.m_iShortAttempts-ne0-or$entryToken.Retired-ne0)){throw 'Quota veto consumed existing recovery allowance or cancelled owned native action'}
 $retryCases++
}
"PASS Reboard native retry: actual EnsureEntry controls=$retryCases; RUNNING far stall restarts native entry, no remote physical pull, original2 retry cap and identity/transition vetoes retained"
$restartBody=Body $token 'CanRestartEntry'
if(!$restartBody){throw 'Missing native restart physical/ownership guard'}
$guardNamespace='AICFRestartGuard'+[guid]::NewGuid().ToString('N')
$guardHarness=@'
namespace GUARD_NAMESPACE {
public enum EAIActionState{RUNNING,FAILED,COMPLETED}
public class SCR_AIUtilityComponent{public static implicit operator bool(SCR_AIUtilityComponent x){return x!=null;}}
public class BaseCompartmentSlot{public int Vehicle=1;public bool Occupant;public int GetVehicle(){return Vehicle;}public bool GetOccupant(){return Occupant;}public static implicit operator bool(BaseCompartmentSlot x){return x!=null;}}
public class CompartmentAccessComponent{public bool Linked,Inside,GettingIn,GettingOut,Switching;public BaseCompartmentSlot Seat;public static bool GetVehicleIn(ChimeraCharacter a){return a.Access!=null&&a.Access.Linked;}public bool IsInCompartment(){return Inside;}public BaseCompartmentSlot GetCompartment(){return Seat;}public bool IsGettingIn(){return GettingIn;}public bool IsGettingOut(){return GettingOut;}public bool IsSwitchingSeatsAnim(){return Switching;}public static implicit operator bool(CompartmentAccessComponent x){return x!=null;}}
public class ChimeraCharacter{public bool InVehicle;public CompartmentAccessComponent Access=new CompartmentAccessComponent();public static ChimeraCharacter Cast(ChimeraCharacter a){return a;}public bool IsInVehicle(){return InVehicle;}public CompartmentAccessComponent GetCompartmentAccessComponent(){return Access;}public static implicit operator bool(ChimeraCharacter x){return x!=null;}}
public class Param{public BaseCompartmentSlot m_Value;}
public class Entry{public Param m_CompartmentToGetIn=new Param();public EAIActionState State=EAIActionState.RUNNING;public EAIActionState GetActionState(){return State;}public static implicit operator bool(Entry x){return x!=null;}}
public class Guard{public BaseCompartmentSlot m_Seat=new BaseCompartmentSlot();public int m_Vehicle=1;public Entry m_Entry=new Entry();public ChimeraCharacter m_Actor=new ChimeraCharacter();public bool Owner=true,Owned=true;public Guard(){m_Entry.m_CompartmentToGetIn.m_Value=m_Seat;}public SCR_AIUtilityComponent ResolveOwner(){if(Owner)return new SCR_AIUtilityComponent();return null;}public bool OwnsAction(SCR_AIUtilityComponent u,Entry a){return Owned;}public bool CanRestartEntry() RESTART_BODY}
}
'@
Add-Type ($guardHarness.Replace('GUARD_NAMESPACE',$guardNamespace).Replace('RESTART_BODY',$restartBody))
$guardCases=0
foreach($kind in @('freeOwn','missingEntry','completedReleased','ownerLost','foreignVehicle','occupied','wrongTarget','foreignRunning','missingActor','characterLinked','nativeLinked','missingAccess','inside','compartment','gettingIn','gettingOut','switching')){
 $guard=[Activator]::CreateInstance([type]($guardNamespace+'.Guard'))
 switch($kind){
  missingEntry{$guard.m_Entry=$null}completedReleased{$guard.Owned=$false;$guard.m_Entry.State=[Enum]::Parse([type]($guardNamespace+'.EAIActionState'),'COMPLETED')}
  ownerLost{$guard.Owner=$false}foreignVehicle{$guard.m_Seat.Vehicle=2}occupied{$guard.m_Seat.Occupant=$true}
  wrongTarget{$guard.m_Entry.m_CompartmentToGetIn.m_Value=[Activator]::CreateInstance([type]($guardNamespace+'.BaseCompartmentSlot'))}
  foreignRunning{$guard.Owned=$false}missingActor{$guard.m_Actor=$null}characterLinked{$guard.m_Actor.InVehicle=$true}
  nativeLinked{$guard.m_Actor.Access.Linked=$true}missingAccess{$guard.m_Actor.Access=$null}inside{$guard.m_Actor.Access.Inside=$true}
  compartment{$guard.m_Actor.Access.Seat=$guard.m_Seat}gettingIn{$guard.m_Actor.Access.GettingIn=$true}gettingOut{$guard.m_Actor.Access.GettingOut=$true}switching{$guard.m_Actor.Access.Switching=$true}
 }
 if($guard.CanRestartEntry() -ne ($kind -in @('freeOwn','missingEntry','completedReleased'))){throw "Actual native restart guard escaped $kind"}
 $guardCases++
}
$issue=Body $flow 'IssueEntry'
if(!$issue.Contains('if (token.OwnsAction(utility, token.m_Entry))') -or !$issue.Contains('token.m_vLastProgressPosition = actor.GetOrigin();') -or !$issue.Contains('token.m_iLastProgressAt = System.GetTickCount();') -or $issue -match 'm_iReboardDeadline\s*=|m_iRetries\s*=|m_iShortAttempts\s*='){throw 'New owned entry must reset only its local observation clock, never deadline/budget'}
"PASS Native restart guards: extracted physical/ownership cases=$guardCases; linked/transition/foreign actor untouched; per-action progress baseline only"
