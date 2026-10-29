param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$gate=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_TransportNativeGate.c'))
$flow=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$s,[string]$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules([string]$g,[string]$f){
 $recover=Body $g 'CanRecoverExpiredReturn';$observe=Body $f 'ObserveNativeGate'
 foreach($guard in @('!Matches(trip)','!IsExpired(nowMs)','!m_bObservedExit','!m_bReturnCompleted','m_iReturnCompletedAtMs <= 0','m_iReturnedAtMs < m_iReturnCompletedAtMs','m_iReturnedAtMs > nowMs','AICF_VehicleShortRecovery.IsSlowVehicle(m_Vehicle)','AICF_VehiclePresetNativeSeatBinding.Capture(trip)','m_Group.GetAgentsCount() != binding.m_aMembers.Count()','ValidateRoster(trip.GetVehiclePresetSlot(), seats, rosterReason)','binding.FindSeat(member)','AICF_VehicleShortRecovery.IsCurrentMember(trip, member)','AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS','return binding.IsCurrent() && Matches(trip)')){if(!$recover.Contains($guard)){'EXACT_COMPLETED_HEALTHY_NEAR_ROSTER';break}}
 if((ConvertTo-AICFCodeText $recover) -match '\b(?:SetOrigin|GetInVehicle|GetOutVehicle|AddAction|Fail|Complete|SetReserved|CallLater|Mark\w*Progress|Record\w*Progress)\s*\('){'NO_GATE_PHYSICAL_OR_PROGRESS_EFFECTS'}
 foreach($guard in @('bool recoverCompletedGate = gate.CanRecoverExpiredReturn(trip, nowMs);','if (recoverCompletedGate) gate.Log("TRANSPORT_NATIVE_GATE_RECOVERY_REQUESTED");','if (!state.BeginMobilityRecovery())','FailRecovery(trip, "NATIVE_GATE_RECOVERY_EXHAUSTED", "MOBILITY", causationId)','BeginMobilityRecovery(trip, causationId, "NATIVE_GATE_WINDOW_EXHAUSTED", false)','FallbackToFoot("NATIVE_GATE_IDENTITY_OR_DEADLINE", causationId)')){if(!$observe.Contains($guard)){'EXISTING_BUDGET_ROUTE_ONLY_OR_ORIGINAL_FALLBACK';break}}
 if($observe.IndexOf('gate.CanRecoverExpiredReturn') -gt $observe.IndexOf('state.SetNativeGate(null)') -or $observe.IndexOf('state.SetNativeGate(null)') -gt $observe.IndexOf('state.BeginMobilityRecovery()')){'GATE_STOP_PRECEDES_BOUNDED_RECOVERY'}
 if((ConvertTo-AICFCodeText $observe) -match 'CaptureNativeGateWindowStart|MarkRouteProgress|CreditNativeRouteProgress|ObservePhysicalMotion|Set.*Deadline'){ 'NO_WINDOW_RENEWAL_OR_FAKE_PROGRESS' }
}
$issues=@(Rules $gate $flow);if($issues.Count){throw "Gate recovery: $issues"};$controls=1
foreach($m in @(
 @('g','!m_bReturnCompleted','false','EXACT_COMPLETED_HEALTHY_NEAR_ROSTER'),
 @('g','!IsExpired(nowMs)','false','EXACT_COMPLETED_HEALTHY_NEAR_ROSTER'),
 @('g','m_iReturnedAtMs < m_iReturnCompletedAtMs','false','EXACT_COMPLETED_HEALTHY_NEAR_ROSTER'),
 @('g','m_Group.GetAgentsCount() != binding.m_aMembers.Count()','false','EXACT_COMPLETED_HEALTHY_NEAR_ROSTER'),
 @('g','AICF_VehicleShortRecovery.MAX_ACTOR_DISPLACEMENT_METERS','100000','EXACT_COMPLETED_HEALTHY_NEAR_ROSTER'),
 @('f','if (!state.BeginMobilityRecovery())','if (false)','EXISTING_BUDGET_ROUTE_ONLY_OR_ORIGINAL_FALLBACK'),
 @('f','"NATIVE_GATE_WINDOW_EXHAUSTED", false)','"NATIVE_GATE_WINDOW_EXHAUSTED", true)','EXISTING_BUDGET_ROUTE_ONLY_OR_ORIGINAL_FALLBACK'),
 @('f','state.SetNativeGate(null);',';','GATE_STOP_PRECEDES_BOUNDED_RECOVERY')
)){
 $g=$gate;$f=$flow;if($m[0] -eq 'g'){$g=$g.Replace($m[1],$m[2])}else{$f=$f.Replace($m[1],$m[2])}
 if(($g -ceq $gate -and $f -ceq $flow) -or $m[3] -notin @(Rules $g $f)){throw "Gate mutation survived: $($m[3])"};$controls++
}
$recover=(Body $gate 'CanRecoverExpiredReturn').Replace('!binding ||','binding == null ||').Replace('!seat ||','seat == null ||').Replace('array<BaseCompartmentSlot> seats = {};','List<BaseCompartmentSlot> seats = new List<BaseCompartmentSlot>();').Replace('.Count()','.Count').Replace('int index;','int index=0;').Replace('string rosterReason;','string rosterReason;').Replace('seats, rosterReason)','seats, out rosterReason)')
$observe=(Body $flow 'ObserveNativeGate').Replace('!gate &&','gate == null &&').Replace('!state.GetCrewRecoveryToken()','state.GetCrewRecoveryToken() == null').Replace('if (gate)','if (gate != null)').Replace('if (!gate)','if (gate == null)')
$code=@'
using System; using System.Collections.Generic;
public struct Vec { public float X,Y,Z; public Vec(float x,float y,float z){X=x;Y=y;Z=z;} }
public static class vector { public static float Distance(Vec a,Vec b){return (float)Math.Sqrt(Math.Pow(a.X-b.X,2)+Math.Pow(a.Y-b.Y,2)+Math.Pow(a.Z-b.Z,2));} }
public class IEntity { public bool Current=true; public Vec Position; public BaseCompartmentSlot Seat=new BaseCompartmentSlot(); public Vec GetOrigin(){return Position;} }
public class BaseCompartmentSlot { public Vec Position; public Vec GetPosition(){return Position;} }
public class Vehicle { public bool Slow=true; }
public class Group { public int Agents=1; public int GetAgentsCount(){return Agents;} }
public static class AICF_VehicleShortRecovery { public const float MAX_ACTOR_DISPLACEMENT_METERS=25; public static bool IsSlowVehicle(Vehicle v){return v!=null && v.Slow;} public static bool IsCurrentMember(AICF_TransportTrip t,IEntity m){return m!=null && m.Current;} }
public class Preset { public bool Valid=true; public bool ValidateRoster(object slot,List<BaseCompartmentSlot> seats,out string reason){reason="";return Valid;} }
public static class AICF_VehiclePreset { public static void Collect(Vehicle v,List<BaseCompartmentSlot> seats){} }
public class AICF_VehiclePresetNativeSeatBinding { public bool Current=true; public int CurrentReads,InvalidateAt=int.MaxValue; public List<IEntity> m_aMembers=new List<IEntity>{new IEntity()}; public bool IsCurrent(){CurrentReads++;return Current && CurrentReads<InvalidateAt;} public static AICF_VehiclePresetNativeSeatBinding Capture(AICF_TransportTrip t){return t.Binding;} public BaseCompartmentSlot FindSeat(IEntity m){return m.Seat;} }
public class AICF_TransportTrip { public AICF_VehiclePresetNativeSeatBinding Binding=new AICF_VehiclePresetNativeSeatBinding(); public Preset Preset=new Preset(); public AICF_VehicleMovementState Movement=new AICF_VehicleMovementState(); public Preset GetVehiclePreset(){return Preset;} public object GetVehiclePresetSlot(){return this;} public AICF_VehicleMovementState GetMovementState(){return Movement;} }
public class AICF_VehicleMovementState { public AICF_TransportNativeGate Gate; public int Attempts,Maximum=2,StopCalls; public bool Pending; public AICF_TransportNativeGate GetNativeGate(){return Gate;} public object GetCrewRecoveryToken(){return null;} public void SetNativeGate(AICF_TransportNativeGate g){if(Gate!=null && g==null)StopCalls++;Gate=g;} public bool BeginMobilityRecovery(){if(Attempts>=Maximum)return false;Attempts++;return true;} public bool IsRecoveryEvidencePending(){return Pending;} }
public class AICF_TransportNativeGate { public bool Current=true,Expired=true,m_bObservedExit=true,m_bReturnCompleted=true,Returned=true,Resumed; public int m_iReturnCompletedAtMs=100,m_iReturnedAtMs=200,MatchReads,InvalidateMatchAt=int.MaxValue; public Vehicle m_Vehicle=new Vehicle(); public Group m_Group=new Group(); public List<string> Logs=new List<string>(); public bool Matches(AICF_TransportTrip t){MatchReads++;return Current && MatchReads<InvalidateMatchAt;} public bool IsExpired(int nowMs){return Expired;} public bool CanRecoverExpiredReturn(AICF_TransportTrip trip,int nowMs) RECOVER public static AICF_TransportNativeGate TryBegin(AICF_TransportTrip t){return null;} public void Log(string s){Logs.Add(s);} public bool IsReturned(){return Returned;} public bool HasResumed(float m){return Resumed;} }
public class AICF_TripOutcome { public string Kind,Reason; public static AICF_TripOutcome Wait(string reason,string id){return new AICF_TripOutcome{Kind="WAIT",Reason=reason};} public static AICF_TripOutcome FallbackToFoot(string reason,string id){return new AICF_TripOutcome{Kind="FALLBACK",Reason=reason};} }
public class Config { public float GetMotionMeters(){return 2;} }
public class Flow { public Config m_Config=new Config(); public bool Physical; public int RecoveryCalls,PendingReports; public AICF_TripOutcome BeginMobilityRecovery(AICF_TransportTrip t,string id,string reason,bool stationary){Physical=stationary;RecoveryCalls++;if(t.Movement.Gate!=null)throw new Exception("Gate still attached");return new AICF_TripOutcome{Kind="REBUILD",Reason=reason};} public AICF_TripOutcome FailRecovery(AICF_TransportTrip t,string reason,string domain,string id){return new AICF_TripOutcome{Kind="FAIL",Reason=reason};} public void ReportPendingRecoveryFailure(AICF_TransportTrip t,string reason,bool final){PendingReports++;} public AICF_TripOutcome Observe(AICF_TransportTrip trip,string causationId,int nowMs) OBSERVE }
'@
Add-Type -TypeDefinition ($code.Replace(' RECOVER',' '+$recover).Replace(' OBSERVE',' '+$observe))
function Good { $t=New-Object AICF_TransportTrip;$g=New-Object AICF_TransportNativeGate;$t.Movement.Gate=$g;return @($t,$g) }
$cases=@(
 @{n='current';bad={param($t,$g)$g.Current=$false}},@{n='not-expired';bad={param($t,$g)$g.Expired=$false}},
 @{n='no-exit';bad={param($t,$g)$g.m_bObservedExit=$false}},@{n='no-completion';bad={param($t,$g)$g.m_bReturnCompleted=$false}},
 @{n='zero-completion';bad={param($t,$g)$g.m_iReturnCompletedAtMs=0}},@{n='early-return';bad={param($t,$g)$g.m_iReturnedAtMs=99}},
 @{n='future-return';bad={param($t,$g)$g.m_iReturnedAtMs=301}},@{n='moving';bad={param($t,$g)$g.m_Vehicle.Slow=$false}},
 @{n='missing-binding';bad={param($t,$g)$t.Binding=$null}},@{n='stale-binding';bad={param($t,$g)$t.Binding.Current=$false}},
 @{n='new-roster-member';bad={param($t,$g)$g.m_Group.Agents=2}},@{n='bad-roster';bad={param($t,$g)$t.Preset.Valid=$false}},
 @{n='foreign-member';bad={param($t,$g)$t.Binding.m_aMembers[0].Current=$false}},@{n='missing-seat';bad={param($t,$g)$t.Binding.m_aMembers[0].Seat=$null}},
 @{n='far-horizontal';bad={param($t,$g)$t.Binding.m_aMembers[0].Position=[Vec]::new(25.001,0,0)}},
 @{n='far-vertical';bad={param($t,$g)$t.Binding.m_aMembers[0].Position=[Vec]::new(0,25.001,0)}},
 @{n='binding-change';bad={param($t,$g)$t.Binding.InvalidateAt=2}},@{n='context-change';bad={param($t,$g)$g.InvalidateMatchAt=2}}
)
foreach($case in $cases){$t,$g=Good;& $case.bad $t $g;if($g.CanRecoverExpiredReturn($t,300)){throw "Recovery accepted invalid $($case.n)"};$controls++}
foreach($distance in @(0,24.999,25)){$t,$g=Good;$t.Binding.m_aMembers[0].Position=[Vec]::new($distance,0,0);if(!$g.CanRecoverExpiredReturn($t,300)){throw "Valid distance rejected $distance"};$controls++}
$t,$g=Good;$f=New-Object Flow;$t.Movement.Pending=$true;$o=$f.Observe($t,'test',300);if($o.Kind -ne 'REBUILD' -or $f.Physical -or $t.Movement.Attempts -ne 1 -or $t.Movement.StopCalls -ne 1 -or $f.PendingReports -ne 1 -or !$g.Logs.Contains('TRANSPORT_NATIVE_GATE_RECOVERY_REQUESTED')){throw 'Expired completed gate did not consume one route-only attempt'};$controls++
$t,$g=Good;$f=New-Object Flow;$t.Movement.Attempts=2;$o=$f.Observe($t,'test',300);if($o.Reason -ne 'NATIVE_GATE_RECOVERY_EXHAUSTED' -or $f.RecoveryCalls -ne 0 -or $t.Movement.Attempts -ne 2){throw 'Exhausted budget was renewed'};$controls++
$t,$g=Good;$f=New-Object Flow;$g.m_bReturnCompleted=$false;$o=$f.Observe($t,'test',300);if($o.Reason -ne 'NATIVE_GATE_IDENTITY_OR_DEADLINE' -or $t.Movement.Attempts -ne 0){throw 'Unproven gate recovery'};$controls++
$t,$g=Good;$f=New-Object Flow;$g.Expired=$false;$g.Resumed=$true;$o=$f.Observe($t,'test',300);if($null -ne $o -or $t.Movement.Attempts -ne 0 -or $g.Logs.Contains('TRANSPORT_NATIVE_GATE_RECOVERY_REQUESTED')){throw 'Ordinary successful native gate changed'};$controls++
"PASS Native gate recovery controls=$controls extracted_recovery_and_dispatch=EXECUTED native_world_gate=SEPARATE"
