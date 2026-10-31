param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_TransportNativeAbandon.c'))
$flow=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text,[string]$transit){
 $observe=Body $text 'Observe';$activity=Body $text 'IsExecutingAbandonActivity';$competing=Body $text 'HasCompetingOwner';$tick=Body $transit 'Tick'
 if(!$observe -or !$activity -or !$competing -or !$tick){'MISSING_METHOD';return}
 foreach($guard in @('!Replication.IsServer()','!trip.IsValid()','trip.IsTerminal()',
  'trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','utility.m_Owner != group',
  'SCR_AIGetOutActivity.Cast(utility.GetCurrentAction())','AICF_VehiclePresetNativeSeatBinding.Capture(trip)',
  '!preset.ValidateRoster(trip.GetVehiclePresetSlot(), seats, rosterReason)',
  'registered.GetEntity() != vehicle','registered.IsStatic()',
  'registered.GetVehicleUsageComponent() != usage','position < 0 && seats[index].IsReserved()',
  '!seats[index].IsReservedBy(trip.VehiclePresetMember(position))')){
  if(!$observe.Contains($guard)){'EXACT_FULL_AI_PRESET_LEASE_AND_REGISTERED_VEHICLE';break}
 }
 foreach($guard in @('activity.m_Utility != utility','utility.GetCurrentAction() != activity','utility.GetExecutedAction() != activity',
  'activity.GetRemoveAction()','activity.GetActionIsSuspended()','activity.GetActionState() != EAIActionState.RUNNING',
  '!activity.m_Vehicle','activity.m_Vehicle.m_Value != vehicle','activity.m_RelatedWaypoint',
  '!activity.m_BoardingParameters','activity.m_BoardingParameters.m_Value',
  'activity.GetPriority() != SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY',
  'activity.EvaluatePriorityLevel() != SCR_AIActionBase.PRIORITY_LEVEL_PLAYER','utility.GetActions(actions)','actions.Contains(activity)')){
  if(!$activity.Contains($guard)){'CURRENT_EXECUTED_EXACT_NATIVE_ABANDON_SIGNATURE';break}
 }
 if(([regex]::Matches($observe,'!binding.IsCurrent\(\)')).Count -ne 2 -or
  ([regex]::Matches($observe,'!IsExecutingAbandonActivity\(activity, utility, vehicle\)')).Count -ne 2){'REPEATED_BINDING_AND_NATIVE_ACTION'}
 foreach($guard in @('!movement.IsRouteWaypointBound()','movement.GetRouteGeneration() <= 0',
  'movement.GetAssignmentRevision() != assignment.GetAssignmentRevision()',
  '!waypoint || group.GetCurrentWaypoint() != waypoint','!queue.Contains(waypoint)',
  'trip.GetAssignment() != assignment','trip.GetMovementState() != movement',
  'assignment.GetAssignmentRevision() != assignmentRevision','assignment.GetStrategicIntentRevision() != intentRevision',
  'movement.GetRouteGeneration() != routeGeneration','movement.GetRouteWaypoint() != waypoint','waypoint.GetID() != waypointId',
  '!repeatedQueue.Contains(waypoint)','group.GetGroupUtilityComponent() != utility',
  'utility.m_VehicleMgr.FindVehicle(vehicle) != registered')){
  if(!$observe.Contains($guard)){'REPEATED_CURRENT_ROUTE_AND_ASSIGNMENT_SCOPE';break}
 }
 if($observe.IndexOf('int routeGeneration =') -gt $observe.IndexOf('AICF_VehiclePresetNativeSeatBinding.Capture(trip)') -or
  $observe -notmatch '!waypoint \|\| !usage \|\| SCR_AIVehicleUsageComponent.Cast\(vehicle.FindComponent\(SCR_AIVehicleUsageComponent\)\) != usage'){'PRE_READ_CONTEXT_CAPTURE_AND_WEAK_GUARDS'}
 if(([regex]::Matches($observe,'if \(HasCompetingOwner\(movement\)\) return false;')).Count -ne 2){'EXISTING_ASYNC_OWNERS_KEEP_PRIORITY'}
 foreach($guard in @('movement.GetCrewRecoveryToken()','movement.IsCrewRecoveryRoutePending()',
  'movement.GetNativeCrewEntry(EAICompartmentType.Pilot)','movement.GetNativeCrewEntry(EAICompartmentType.Turret)','movement.GetNativeGate()')){
  if(!$competing.Contains($guard)){'EXISTING_ASYNC_OWNERS_KEEP_PRIORITY';break}
 }
 $hook=$tick.IndexOf('AICF_TransportNativeAbandon.Observe(trip, nativeAbandonDetails)')
 if($hook -lt 0 -or $hook -lt $tick.IndexOf('trip.IsDeadlineReached(nowMs)') -or
  $hook -lt $tick.IndexOf('InspectRouteAssetFailure(trip)') -or $hook -gt $tick.IndexOf('AdvancePendingNativeCrewEntries') -or
  $hook -gt $tick.IndexOf('ProcessMandatoryCrew') -or
  $tick -notmatch '(?s)if \(AICF_TransportNativeAbandon.Observe\(trip, nativeAbandonDetails\)\).*?TRANSPORT_NATIVE_GROUP_ABANDONED.*?return AICF_TripOutcome.FallbackToFoot\("NATIVE_GROUP_ABANDONED_VEHICLE", causationId\);'){'ABANDON_BEFORE_CREW_AND_AFTER_SAFETY'}
 $code=ConvertTo-AICFCodeText $text
 if($code -match '\.(?:Set\w*|AddAction|Fail|Complete|Cancel\w*|Interrupt\w*|GetInVehicle|GetOutVehicle|Begin\w*|Record\w*|Credit\w*|Observe\w*|Insert|Remove|Commit\w*)\(|CallLater|Teleport|SetOrigin|FindPath|RequestFollowPath|m_[ifb]\w*\s*[+]?=|protected ref|ref SCR_AIGetOutActivity'){'STATELESS_READONLY_NO_NATIVE_OR_CLOCK_MUTATION'}
}
$failures=@(Rules $source $flow)
if($failures.Count){throw "Native group abandon: $failures"}
$controls=1
foreach($mutation in @(
 @('AICF_VehiclePresetNativeSeatBinding.Capture(trip)','null','EXACT_FULL_AI_PRESET_LEASE_AND_REGISTERED_VEHICLE'),
 @('!preset.ValidateRoster(trip.GetVehiclePresetSlot(), seats, rosterReason)','false','EXACT_FULL_AI_PRESET_LEASE_AND_REGISTERED_VEHICLE'),
 @('registered.GetEntity() != vehicle','false','EXACT_FULL_AI_PRESET_LEASE_AND_REGISTERED_VEHICLE'),
 @('!seats[index].IsReservedBy(trip.VehiclePresetMember(position))','false','EXACT_FULL_AI_PRESET_LEASE_AND_REGISTERED_VEHICLE'),
 @('utility.GetExecutedAction() != activity','false','CURRENT_EXECUTED_EXACT_NATIVE_ABANDON_SIGNATURE'),
 @('activity.GetActionState() != EAIActionState.RUNNING','false','CURRENT_EXECUTED_EXACT_NATIVE_ABANDON_SIGNATURE'),
 @('activity.m_Vehicle.m_Value != vehicle','false','CURRENT_EXECUTED_EXACT_NATIVE_ABANDON_SIGNATURE'),
 @('activity.m_RelatedWaypoint','false','CURRENT_EXECUTED_EXACT_NATIVE_ABANDON_SIGNATURE'),
 @('activity.GetPriority() != SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY','false','CURRENT_EXECUTED_EXACT_NATIVE_ABANDON_SIGNATURE'),
 @('actions.Contains(activity)','true','CURRENT_EXECUTED_EXACT_NATIVE_ABANDON_SIGNATURE'),
 @('!binding.IsCurrent()','false','REPEATED_BINDING_AND_NATIVE_ACTION'),
 @('assignment.GetStrategicIntentRevision() != intentRevision','false','REPEATED_CURRENT_ROUTE_AND_ASSIGNMENT_SCOPE'),
 @('!repeatedQueue.Contains(waypoint)','false','REPEATED_CURRENT_ROUTE_AND_ASSIGNMENT_SCOPE'),
 @('movement.GetNativeCrewEntry(EAICompartmentType.Turret)','false','EXISTING_ASYNC_OWNERS_KEEP_PRIORITY'),
 @('if (HasCompetingOwner(movement)) return false;','Print("ignored owner");','EXISTING_ASYNC_OWNERS_KEEP_PRIORITY')
)){
 if(!$source.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])) $flow)){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$observe=Body $source 'Observe'
foreach($mutation in @('activity.Fail();','utility.AddAction(activity);','trip.CommitTransition(0, "test", "test", 0);','movement.CreditNativeRouteProgress(999999);')){
 $changed=$source.Replace($observe,$observe.Insert(1,$mutation))
 if('STATELESS_READONLY_NO_NATIVE_OR_CLOCK_MUTATION' -notin @(Rules $changed $flow)){throw "Observer side effect survived: $mutation"}
 $controls++
}
$tick=Body $flow 'Tick'
$hook=[regex]::Match($tick,'(?s)\s*string nativeAbandonDetails;\s*if \(AICF_TransportNativeAbandon.Observe\(trip, nativeAbandonDetails\)\)\s*\{.*?\n\s*\}').Value
if(!$hook){throw 'Hook ordering mutation target absent'}
$lateTick=$tick.Replace($hook,'').Replace('if (state.GetAssignmentRevision()',$hook+"`n if (state.GetAssignmentRevision()")
if('ABANDON_BEFORE_CREW_AND_AFTER_SAFETY' -notin @(Rules $source ($flow.Replace($tick,$lateTick)))){throw 'Late abandon observation survived'}
$controls++
# Actual production activity predicate, with only native reference/array syntax adapted.
$body=Body $source 'IsExecutingAbandonActivity'
$body=$body.Replace('!activity ||','activity == null ||').Replace('!utility ||','utility == null ||').Replace('!vehicle)','vehicle == null)')
$body=$body.Replace('!activity.m_Vehicle ||','activity.m_Vehicle == null ||').Replace('activity.m_RelatedWaypoint ||','activity.m_RelatedWaypoint != null ||')
$body=$body.Replace('!activity.m_BoardingParameters ||','activity.m_BoardingParameters == null ||').Replace('activity.m_BoardingParameters.m_Value)','activity.m_BoardingParameters.m_Value != null)')
$body=$body.Replace('array<ref AIActionBase> actions = {};','List<AIActionBase> actions = new List<AIActionBase>();')
$adapter=@'
using System;
using System.Collections.Generic;
public enum EAIActionState { RUNNING,EVALUATED,COMPLETED,FAILED }
public class AIActionBase {}
public class Vehicle {}
public class AICF_Param<T> { public T m_Value; }
public class SCR_AIActionBase { public const double PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY=1162; public const double PRIORITY_LEVEL_PLAYER=1000; }
public class SCR_AIGroupUtilityComponent {
 public AIActionBase Current,Executed; public List<AIActionBase> Actions=new List<AIActionBase>();
 public AIActionBase GetCurrentAction(){return Current;} public AIActionBase GetExecutedAction(){return Executed;} public void GetActions(List<AIActionBase> output){output.AddRange(Actions);}
}
public class SCR_AIGetOutActivity : AIActionBase {
 public SCR_AIGroupUtilityComponent m_Utility; public AICF_Param<Vehicle> m_Vehicle=new AICF_Param<Vehicle>(); public AICF_Param<object> m_BoardingParameters=new AICF_Param<object>(); public object m_RelatedWaypoint;
 public bool Removed,Suspended; public EAIActionState State=EAIActionState.RUNNING; public double Priority=1162,Level=1000;
 public bool GetRemoveAction(){return Removed;} public bool GetActionIsSuspended(){return Suspended;} public EAIActionState GetActionState(){return State;} public double GetPriority(){return Priority;} public double EvaluatePriorityLevel(){return Level;}
}
public static class AICF_NativeAbandonAdapter {
 public static bool IsExecutingAbandonActivity(SCR_AIGetOutActivity activity,SCR_AIGroupUtilityComponent utility,Vehicle vehicle) BODY
 public static bool Evaluate(int scenario){
  var vehicle=new Vehicle();var utility=new SCR_AIGroupUtilityComponent();var action=new SCR_AIGetOutActivity(){m_Utility=utility};action.m_Vehicle.m_Value=vehicle;utility.Current=action;utility.Executed=action;utility.Actions.Add(action);
  switch(scenario){
   case 1: utility.Current=new AIActionBase();break;case 2: utility.Executed=new AIActionBase();break;
   case 3: action.m_Vehicle.m_Value=new Vehicle();break;case 4: action.m_Utility=new SCR_AIGroupUtilityComponent();break;
   case 5: action.Suspended=true;break;case 6: action.Removed=true;break;
   case 7: action.State=EAIActionState.FAILED;break;case 8: action.State=EAIActionState.COMPLETED;break;case 9: action.State=EAIActionState.EVALUATED;break;
   case 10: action.Priority=50;break;case 11: action.Level=0;break;case 12: action.m_RelatedWaypoint=new object();break;
   case 13: action.m_BoardingParameters.m_Value=new object();break;case 14: action.m_Vehicle=null;break;case 15: action.m_BoardingParameters=null;break;
   case 16: utility.Actions.Clear();break;case 17: action=null;break;case 18: utility=null;break;case 19: vehicle=null;break;
  }
  return IsExecutingAbandonActivity(action,utility,vehicle);
 }
}
'@
if(!('AICF_NativeAbandonAdapter' -as [type])){Add-Type -TypeDefinition $adapter.Replace('BODY',$body)}
foreach($scenario in 0..19){
 $expected=$scenario -eq 0
 if([AICF_NativeAbandonAdapter]::Evaluate($scenario) -ne $expected){throw "Native abandon current-action signature case failed: $scenario"}
 $controls++
}
"PASS Native group abandon: exact executing high-priority group GetOut, complete repeated AI/preset/lease/route proof, competing owners preserved, typed fallback before crew recovery, no native actions or progress clocks changed; controls=$controls"
