param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$source=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_TransportNativeGate.c'))
$phase=[IO.File]::ReadAllText((Join-Path $core 'State/Vehicles/AICF_VehiclePhaseStates.c'))
$flow=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleTransitFlow.c'))
$controller=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_TransportTripController.c'))
$movement=[regex]::Match($phase,'(?s)class AICF_VehicleMovementState\b.*?(?=\r?\nclass |\z)').Value
function Body([string]$text,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $method}
function Rules([string]$text,[string]$state,[string]$transit,[string]$control=$controller){
 $attach=Body $text 'AttachReturnCompletion';$callback=Body $text 'OnReturnCompleted';$stop=Body $text 'Stop';$returned=Body $text 'IsReturned';$begin=Body $text 'Begin';$setter=Body $state 'SetNativeGate';$reset=Body $state 'Reset';$abort=Body $transit 'AbortOwnedActions'
 if(!$attach -or !$callback -or !$stop -or !$setter -or !$reset){'METHODS';return}
 foreach($guard in @('m_bStopped || m_bReturnListenerAttached || !m_Return || !Matches(trip)',
  'm_Trip = trip;','m_bReturnListenerAttached = true;','m_Return.m_OnActionCompleted.Insert(OnReturnCompleted);')){if(!$attach.Contains($guard)){'EXACT_NATIVE_RETURN_SUBSCRIPTION';break}}
 if(!$begin.Contains('if (!gate.AttachReturnCompletion(trip)) continue;') -or
  $begin.IndexOf('gate.AttachReturnCompletion(trip)') -lt $begin.IndexOf('gate.m_iDeadline =')){'SUBSCRIBE_AFTER_IDENTITY_BEFORE_NATIVE_RETURN'}
 foreach($guard in @('m_bStopped || !m_bReturnListenerAttached || !m_Trip || action != m_Return',
  '!Matches(m_Trip) || m_Trip.GetMovementState().GetNativeGate() != this','m_bReturnCompleted = true;',
  'm_iReturnCompletedAtMs = System.GetTickCount();')){if(!$callback.Contains($guard)){'CURRENT_EXACT_COMPLETION_RECEIPT';break}}
 $writes=@([regex]::Matches((ConvertTo-AICFCodeText $callback),'\b(m_\w+)\s*=')|ForEach-Object {$_.Groups[1].Value})
 if($writes.Count -ne 2 -or @($writes|Where-Object {$_ -notin @('m_bReturnCompleted','m_iReturnCompletedAtMs')}).Count){'CALLBACK_ONLY_POSITIVE_RECEIPT'}
 if(!$returned.Contains('!m_bObservedExit || !m_Return || !m_bReturnCompleted') -or
  $returned.Contains('m_Return.GetActionState() != EAIActionState.COMPLETED') -or
  !$returned.Contains('!character.IsInVehicle() && !access.IsInCompartment() && !CompartmentAccessComponent.GetVehicleIn(character)') -or
  !(Body $text 'HasResumed').Contains('vector.DistanceXZ(m_Vehicle.GetOrigin(), m_vReturnPosition) >= minimumMotionMeters') -or
  !(Body $text 'IsExpired').Contains('now - m_iStarted >= 60000 || now >= m_iDeadline')){'RECEIPT_SURVIVES_DESELECT_WITH_PHYSICAL_AND_DEADLINE_PROOF'}
 foreach($guard in @('if (m_bStopped) return;','m_bStopped = true;',
  'm_Return.m_OnActionCompleted.Remove(OnReturnCompleted);','m_bReturnListenerAttached = false;',
  'm_Trip = null;','m_Return = null;','m_Exit = null;','m_Perform = null;')){if(!$stop.Contains($guard)){'IDEMPOTENT_LISTENER_REMOVAL_AND_CYCLE_BREAK';break}}
 if($stop.IndexOf('m_bStopped = true;') -gt $stop.IndexOf('.Remove(OnReturnCompleted)') -or
  $stop.IndexOf('.Remove(OnReturnCompleted)') -gt $stop.IndexOf('m_Return = null;')){'IDEMPOTENT_LISTENER_REMOVAL_AND_CYCLE_BREAK'}
 if(!$setter.Contains('if (m_NativeGate == gate) return;') -or !$setter.Contains('if (m_NativeGate) m_NativeGate.Stop();') -or
  $setter.IndexOf('m_NativeGate.Stop()') -gt $setter.IndexOf('m_NativeGate = gate;') -or
  !$reset.Contains('SetNativeGate(null);') -or $reset.Contains('m_NativeGate = null;') -or
  !$abort.Contains('state.SetNativeGate(null);')){'RESET_REPLACEMENT_ABORT_USE_STOP_BOUNDARY'}
 $controllerStop=Body $control 'AbortForStop';$invalid=$controllerStop.IndexOf('if (!trip.IsValid())')
 if($invalid -lt 0){'INVALID_TRIP_STOP_DETACHES_LOCAL_OBSERVER';return}
 $before=ConvertTo-AICFCodeText $controllerStop.Substring(0,$invalid)
 # Новый lifecycle также снимает identity-safe tactical tokens до invalid-trip
 # return. Разрешаем только этот конкретный cleanup, не произвольные actions.
 if($before -notmatch '(?s)^\s*\{\s*if \(!trip\)\s*return AICF_TripOutcome.TerminalFailClosed\([^;]+\);\s*AICF_VehicleMovementState stopMovement = trip.GetMovementState\(\);\s*if \(stopMovement\) stopMovement.SetNativeGate\(null\);\s*m_BehaviorFlow.Stop\(trip\);\s*$' -or
  $before -match 'AbortOwnedActions|CancelResidualOwnedEffects|TransitionTo\('){'INVALID_TRIP_STOP_DETACHES_LOCAL_OBSERVER'}
}
$issues=@(Rules $source $movement $flow);if($issues.Count){throw "Native gate completion contracts: $issues"}
$controls=1
foreach($mutation in @(
 @('g','action != m_Return','false','CURRENT_EXACT_COMPLETION_RECEIPT'),
 @('g','!Matches(m_Trip)','false','CURRENT_EXACT_COMPLETION_RECEIPT'),
 @('g','m_Trip.GetMovementState().GetNativeGate() != this','false','CURRENT_EXACT_COMPLETION_RECEIPT'),
 @('g','m_bStopped || !m_bReturnListenerAttached','false || !m_bReturnListenerAttached','CURRENT_EXACT_COMPLETION_RECEIPT'),
 @('g','m_Return.m_OnActionCompleted.Insert(OnReturnCompleted);',';','EXACT_NATIVE_RETURN_SUBSCRIPTION'),
 @('g','m_Return.m_OnActionCompleted.Remove(OnReturnCompleted);',';','IDEMPOTENT_LISTENER_REMOVAL_AND_CYCLE_BREAK'),
 @('g','m_bStopped = true;',';','IDEMPOTENT_LISTENER_REMOVAL_AND_CYCLE_BREAK'),
 @('g','!m_bObservedExit || !m_Return || !m_bReturnCompleted','!m_bObservedExit || !m_Return || m_Return.GetActionState() != EAIActionState.COMPLETED','RECEIPT_SURVIVES_DESELECT_WITH_PHYSICAL_AND_DEADLINE_PROOF'),
 @('s','if (m_NativeGate) m_NativeGate.Stop();',';','RESET_REPLACEMENT_ABORT_USE_STOP_BOUNDARY'),
 @('s','SetNativeGate(null);','m_NativeGate = null;','RESET_REPLACEMENT_ABORT_USE_STOP_BOUNDARY')
)){
 $g=$source;$s=$movement
 if($mutation[0] -eq 'g'){$g=$g.Replace($mutation[1],$mutation[2])}else{$s=$s.Replace($mutation[1],$mutation[2])}
 if($g -ceq $source -and $s -ceq $movement){throw 'Missing completion mutation target'}
 if($mutation[3] -notin @(Rules $g $s $flow)){throw "Completion mutation survived: $($mutation[3])"};$controls++
}
foreach($mutated in @(
 $controller.Replace('if (stopMovement) stopMovement.SetNativeGate(null);',';'),
 $controller.Replace('if (!trip)', 'AICF_VehicleMovementState unsafeRead = trip.GetMovementState(); if (!trip)')
)){
 if('INVALID_TRIP_STOP_DETACHES_LOCAL_OBSERVER' -notin @(Rules $source $movement $flow $mutated)){throw 'Invalid/null trip Stop cleanup mutation survived'};$controls++
}
# Actual production callback/attach/stop/setter bodies. Adapter changes only
# object truthiness and the clock; native Complete -> Deselected resets State.
function Adapt([string]$body){
 $body=$body.Replace('!m_Return.m_OnActionCompleted','m_Return.m_OnActionCompleted == null').Replace('!m_Return','m_Return == null').Replace('!m_Trip','m_Trip == null').Replace('&& m_Return)','&& m_Return != null)').Replace('if (m_NativeGate)','if (m_NativeGate != null)').Replace('System.GetTickCount()','1000')
 return $body
}
$attach=Adapt (Body $source 'AttachReturnCompletion');$callback=Adapt (Body $source 'OnReturnCompleted');$stop=Adapt (Body $source 'Stop');$setter=Adapt (Body $movement 'SetNativeGate')
$code=@'
using System; using System.Collections.Generic;
public class AICF_CallbackInvoker {
 public List<Action<AICF_CallbackAction>> List=new List<Action<AICF_CallbackAction>>();
 public void Insert(Action<AICF_CallbackAction> action){List.Add(action);}
 public void Remove(Action<AICF_CallbackAction> action){List.Remove(action);}
 public void Invoke(AICF_CallbackAction action){foreach(var callback in List.ToArray()) callback(action);}
}
public class AICF_CallbackAction {
 public AICF_CallbackInvoker m_OnActionCompleted=new AICF_CallbackInvoker(); public int State;
 public void Complete(){State=2;m_OnActionCompleted.Invoke(this);} public void Deselect(){State=0;} public void Cancel(){State=3;}
}
public class AICF_CallbackTrip { public bool Valid=true; public bool IsValid(){return Valid;} public AICF_CallbackMovement State=new AICF_CallbackMovement(); public AICF_CallbackMovement GetMovementState(){return State;} }
public class AICF_CallbackMovement { public AICF_CallbackGate m_NativeGate; public AICF_CallbackGate GetNativeGate(){return m_NativeGate;} public void SetNativeGate(AICF_CallbackGate gate) SETTER }
public class AICF_CallbackGate {
 public bool m_bStopped,m_bReturnListenerAttached,m_bReturnCompleted; public int m_iReturnCompletedAtMs;
 public bool ContextCurrent=true; public AICF_CallbackTrip m_Trip; public AICF_CallbackAction m_Return,m_Exit,m_Perform;
 public bool Matches(AICF_CallbackTrip trip){return ContextCurrent && trip!=null;}
 public bool AttachReturnCompletion(AICF_CallbackTrip trip) ATTACH
 public void OnReturnCompleted(AICF_CallbackAction action) CALLBACK
 public void Stop() STOP
}
'@
$code=$code.Replace('SETTER',$setter).Replace('ATTACH',$attach).Replace('CALLBACK',$callback).Replace('STOP',$stop)
$controllerStop=Body $controller 'AbortForStop'
$prelude=$controllerStop.Substring(0,$controllerStop.IndexOf('if (reason.IsEmpty())'))
$prelude=$prelude.Replace('if (!trip)','if (trip == null)').Replace('AICF_VehicleMovementState','AICF_CallbackMovement').Replace('if (stopMovement)','if (stopMovement != null)')
$stopCode='public class AICF_TripOutcome { public string Reason; public AICF_TripOutcome(string reason){Reason=reason;} public static AICF_TripOutcome TerminalFailClosed(string reason,string cause){return new AICF_TripOutcome(reason);} } public class AICF_CallbackBehavior { public int StopCalls; public void Stop(AICF_CallbackTrip trip){if(trip==null) throw new Exception("null tactical stop"); StopCalls++;} } public class AICF_CallbackController { public AICF_CallbackBehavior m_BehaviorFlow=new AICF_CallbackBehavior(); public int ReleaseCalls; public void ReleaseSpawnPlan(AICF_CallbackTrip trip,string reason){ReleaseCalls++;} public AICF_TripOutcome AbortForStop(AICF_CallbackTrip trip,string reason) '+$prelude+' return new AICF_TripOutcome("VALID_AFTER_LOCAL_STOP"); } }'
$code+=$stopCode
Add-Type -TypeDefinition $code
function Setup {
 $trip=New-Object AICF_CallbackTrip;$gate=New-Object AICF_CallbackGate;$action=New-Object AICF_CallbackAction
 $gate.m_Return=$action;$trip.State.SetNativeGate($gate)
 if(!$gate.AttachReturnCompletion($trip)){throw 'Callback fixture attach failed'}
 return [pscustomobject]@{trip=$trip;gate=$gate;action=$action}
}
$case=Setup;$case.action.Complete();$case.action.Deselect()
if(!$case.gate.m_bReturnCompleted -or $case.action.State -ne 0){throw 'Transient native completion was lost on Deselected EVALUATED'};$controls++
$case=Setup;$other=New-Object AICF_CallbackAction;$case.action.m_OnActionCompleted.Invoke($other)
if($case.gate.m_bReturnCompleted){throw 'Foreign action supplied completion receipt'};$controls++
$case=Setup;$case.gate.ContextCurrent=$false;$case.action.Complete()
if($case.gate.m_bReturnCompleted){throw 'Stale context supplied completion receipt'};$controls++
$case=Setup;$case.trip.State.SetNativeGate((New-Object AICF_CallbackGate));$case.action.Complete()
if($case.gate.m_bReturnCompleted -or $case.action.m_OnActionCompleted.List.Count){throw 'Replaced observer retained listener or receipt'};$controls++
$case=Setup;$case.action.Cancel();$case.action.Deselect()
if($case.gate.m_bReturnCompleted){throw 'Cancellation without native completion supplied receipt'};$controls++
$case=Setup;$pending=$case.action.m_OnActionCompleted.List.ToArray();$case.gate.Stop();foreach($listener in $pending){$listener.Invoke($case.action)}
if($case.gate.m_bReturnCompleted -or $case.action.m_OnActionCompleted.List.Count -or $null -ne $case.gate.m_Return){throw 'Stop did not remove listener/cycle or fence in-flight callback'};$controls++
$case=Setup;if($case.gate.AttachReturnCompletion($case.trip) -or $case.action.m_OnActionCompleted.List.Count -ne 1){throw 'Duplicate attachment was accepted'};$case.gate.Stop();$case.gate.Stop();$controls++
$owner=New-Object AICF_CallbackController;$case=Setup;$case.trip.Valid=$false;$beforeState=$case.action.State
$result=$owner.AbortForStop($case.trip,'test')
if($case.action.State -ne $beforeState){throw 'Invalid trip local observer Stop changed native action state'}
$case.action.Complete()
if($result.Reason -ne 'STOP_TRIP_IDENTITY_INVALID' -or !$case.gate.m_bStopped -or $case.action.m_OnActionCompleted.List.Count -or $case.gate.m_bReturnCompleted -or $owner.ReleaseCalls -ne 1 -or $owner.m_BehaviorFlow.StopCalls -ne 1){throw 'Invalid trip Stop skipped listener removal or accepted late completion'};$controls++
$owner=New-Object AICF_CallbackController;$result=$owner.AbortForStop($null,'test')
if($result.Reason -ne 'STOP_TRIP_IDENTITY_INVALID' -or $owner.ReleaseCalls -or $owner.m_BehaviorFlow.StopCalls){throw 'Null trip Stop dereferenced trip or changed legacy release behavior'};$controls++
$trip=New-Object AICF_CallbackTrip;$trip.Valid=$false;$trip.State=$null;$result=$owner.AbortForStop($trip,'test')
if($result.Reason -ne 'STOP_TRIP_IDENTITY_INVALID'){throw 'Missing movement state invalid Stop did not fail closed'};$controls++
"PASS Native gate completion: exact native callback survives Completed-to-Evaluated recycling; wrong/stale/canceled/stopped/replaced callbacks rejected; Insert/Remove lifecycle and physical/deadline proof preserved; controls=$controls"
