param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c'))
$snapshot=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_StrategicAssignmentSnapshot.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $receipt=Body $text 'HasCompletedStrategicTransportIntent';$replace=Body $text 'ShouldReplaceTerminalTrip';$process=Body $text 'ProcessSlot';$admit=Body $text 'IsAdmissionEligible'
 if(!$receipt -or !$replace -or !$process -or !$admit){'MISSING_METHOD';return}
 foreach($guard in @('!Replication.IsServer()','!trip','!assignment','!slot','trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE',
  '!trip.GetHandoffState().IsOrderRestored()',
  '!completed.MatchesCurrent(assignment.GetFactionKey(), slot.GetSlotId(), slot.GetSpawnGeneration(), slot.GetGroup())',
  '!assignment.MatchesCurrent(assignment.GetFactionKey(), slot.GetSlotId(), slot.GetSpawnGeneration(), slot.GetGroup())',
  'completed.GetUnitType() != slot.GetUnitType()','assignment.GetUnitType() != slot.GetUnitType()',
  'completed.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()',
  '!completed.MatchesDestination(assignment)','trip.GetVehiclePresetSlot() && trip.GetVehiclePresetSlot() != slot',
  'trip.GetVehiclePresetRevision() == slot.GetVehiclePresetRevision()')){
  if(!$receipt.Contains($guard)){'EXACT_NORMAL_COMPLETE_INTENT_RECEIPT';break}
 }
 if($receipt -match 'GetAssignmentRevision|GetBaseRevision|GetWaypoint|GetTickCount|GetOrigin|GetAlive|CountAlive|Deadline|Cooldown|UntilMs'){'NO_INCIDENTAL_REVISION_OR_COOLDOWN_REARM'}
 if($replace.IndexOf('!m_TripController.CanRetireTrip(trip)') -gt $replace.IndexOf('HasCompletedStrategicTransportIntent') -or
  $replace -notmatch '(?s)if \(HasCompletedStrategicTransportIntent\(trip, assignment, slot\)\).*?if \(HasExplicitVehicleAdmission\(slot, assignment\)\) return true;.*?ReportAdmissionOnce\(assignment, "STRATEGIC_TRANSPORT_INTENT_ALREADY_COMPLETED"\);\s*return false;'){'CLEANUP_FIRST_EXPLICIT_OR_SAME_INTENT_VETO'}
 foreach($rearm in @('trip.GetVehiclePresetRevision() != trip.GetVehiclePresetSlot().GetVehiclePresetRevision()',
  'trip.GetAssignment().GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()',
  'trip.GetAssignment().GetAssignmentRevision() != assignment.GetAssignmentRevision()',
  'trip.GetAssignment().GetBaseRevision() != assignment.GetBaseRevision()',
  '!trip.GetAssignment().MatchesDestination(assignment)')){if(!$replace.Contains($rearm)){'OTHER_TERMINAL_REPLACEMENT_POLICY_PRESERVED';break}}
 if($process -notmatch '!ShouldReplaceTerminalTrip\(trip, assignment, slot\)' -or
  $process.IndexOf('!ShouldReplaceTerminalTrip') -gt $process.LastIndexOf('m_Trips.RemoveTerminal(trip)') -or
  $process.LastIndexOf('m_Trips.RemoveTerminal(trip)') -gt $process.IndexOf('!IsAdmissionEligible(slot, assignment, fleet, ineligibleReason)') -or
  $process.IndexOf('!IsAdmissionEligible(slot, assignment, fleet, ineligibleReason)') -gt $process.IndexOf('m_Trips.Create(')){'VETO_BEFORE_REMOVAL_AND_ORIGINAL_ADMISSION'}
 foreach($gate in @('GetInfantryOrderAdmissionFenceReason(slot)','MINIMUM_ROSTER_NOT_MET','MEANINGFUL_INFANTRY_ORDER_MISSING',
  'ALIVE_LEADER_MISSING','ROUTE_BELOW_VEHICLE_THRESHOLD','SLOT_LEASE_ALREADY_PRESENT','VEHICLE_CAP_UNAVAILABLE_PRE_ADMISSION')){
  if(!$admit.Contains($gate)){'FULL_ADMISSION_GATES_PRESERVED';break}
 }
 $code=ConvertTo-AICFCodeText ($receipt+$replace)
 if($code -match '\.(?:Set\w*|Begin\w*|Commit\w*|Create|RemoveTerminal|Purchase\w*|AddAction|Record\w*|BumpMe)\(|CallLater|SetOrigin|Teleport|m_[ifb]\w*\s*[+]?='){'NO_NEW_STATE_OR_NATIVE_SIDE_EFFECT'}
}
$failures=@(Rules $source)
if($failures.Count){throw "Completed transport intent: $failures"}
$controls=1
foreach($mutation in @(
 @('trip.GetPhase() != AICF_ETransportTripPhase.COMPLETE','false','EXACT_NORMAL_COMPLETE_INTENT_RECEIPT'),
 @('!trip.GetHandoffState().IsOrderRestored()','false','EXACT_NORMAL_COMPLETE_INTENT_RECEIPT'),
 @('!completed.MatchesCurrent(assignment.GetFactionKey(), slot.GetSlotId(), slot.GetSpawnGeneration(), slot.GetGroup())','false','EXACT_NORMAL_COMPLETE_INTENT_RECEIPT'),
 @('!assignment.MatchesCurrent(assignment.GetFactionKey(), slot.GetSlotId(), slot.GetSpawnGeneration(), slot.GetGroup())','false','EXACT_NORMAL_COMPLETE_INTENT_RECEIPT'),
 @('completed.GetUnitType() != slot.GetUnitType()','false','EXACT_NORMAL_COMPLETE_INTENT_RECEIPT'),
 @('completed.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()','false','EXACT_NORMAL_COMPLETE_INTENT_RECEIPT'),
 @('!completed.MatchesDestination(assignment)','false','EXACT_NORMAL_COMPLETE_INTENT_RECEIPT'),
 @('trip.GetVehiclePresetRevision() == slot.GetVehiclePresetRevision()','true','EXACT_NORMAL_COMPLETE_INTENT_RECEIPT'),
 @('if (HasExplicitVehicleAdmission(slot, assignment)) return true;','if (false) return true;','CLEANUP_FIRST_EXPLICIT_OR_SAME_INTENT_VETO'),
 @('STRATEGIC_TRANSPORT_INTENT_ALREADY_COMPLETED','ROUTE_BELOW_VEHICLE_THRESHOLD','CLEANUP_FIRST_EXPLICIT_OR_SAME_INTENT_VETO'),
 @('!ShouldReplaceTerminalTrip(trip, assignment, slot)','false','VETO_BEFORE_REMOVAL_AND_ORIGINAL_ADMISSION')
)){
 if(!$source.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$receipt=Body $source 'HasCompletedStrategicTransportIntent'
foreach($line in @('if (completed.GetAssignmentRevision() != assignment.GetAssignmentRevision()) return false;',
 'if (completed.GetBaseRevision() != assignment.GetBaseRevision()) return false;',
 'if (System.GetTickCount() > 10000) return false;')){
 if('NO_INCIDENTAL_REVISION_OR_COOLDOWN_REARM' -notin @(Rules ($source.Replace($receipt,$receipt.Insert(1,$line))))){throw 'Incidental rearm mutation survived'}
 $controls++
}
$replace=Body $source 'ShouldReplaceTerminalTrip'
$early=$replace.Replace('if (!trip || !assignment || !m_TripController.CanRetireTrip(trip))','if (!trip || !assignment)').Replace('bool presetChanged', 'if (!m_TripController.CanRetireTrip(trip)) return false;' + "`n bool presetChanged")
if('CLEANUP_FIRST_EXPLICIT_OR_SAME_INTENT_VETO' -notin @(Rules ($source.Replace($replace,$early)))){throw 'Explicit request before cleanup proof survived'}
$controls++
# Исполняем production receipt/replacement bodies и production immutable snapshot
# identity/destination predicates. Адаптер меняет только reference-bool syntax.
function CSharpBody([string]$body){
 $body=$body.Replace('!trip ||','trip == null ||').Replace('!assignment ||','assignment == null ||').Replace('!assignment)', 'assignment == null)')
 $body=$body.Replace('!slot)', 'slot == null)').Replace('!completed ||','completed == null ||').Replace('!other ||','other == null ||')
 $body=$body.Replace('trip.GetVehiclePresetSlot() &&','trip.GetVehiclePresetSlot() != null &&').Replace('&& group &&','&& group != null &&')
 return $body
}
$adapter=@'
using System;
public enum AICF_ETransportTripPhase { COMPLETE, FALLBACK, FAILED_CLOSED, HANDOFF }
public enum AICF_EOrderTargetKind { POSITION, BASE }
public static class Replication { public static bool Server=true; public static bool IsServer(){return Server;} }
public class AICF_GroupSlot { public int Slot=5,Generation=1,Unit=1,PresetRevision=1; public object Group; public int GetSlotId(){return Slot;} public int GetSpawnGeneration(){return Generation;} public object GetGroup(){return Group;} public int GetUnitType(){return Unit;} public int GetVehiclePresetRevision(){return PresetRevision;} }
public class AICF_StrategicAssignmentSnapshot {
 public string m_sFactionKey="US"; public int m_iSlotId=5,m_iGroupGeneration=1,Unit=1,Intent=3,Assignment=6,BaseRevision=1;
 public object m_Group,m_TargetBase; public AICF_EOrderTargetKind m_TargetKind=AICF_EOrderTargetKind.POSITION; public int m_vTargetPosition=2238;
 public string GetFactionKey(){return m_sFactionKey;} public int GetUnitType(){return Unit;} public int GetStrategicIntentRevision(){return Intent;}
 public int GetAssignmentRevision(){return Assignment;} public int GetBaseRevision(){return BaseRevision;}
 public AICF_EOrderTargetKind GetTargetKind(){return m_TargetKind;} public object GetTargetBase(){return m_TargetBase;} public int GetTargetPosition(){return m_vTargetPosition;}
 public bool MatchesCurrent(string factionKey,int slotId,int groupGeneration,object group) CURRENT
 public bool MatchesDestination(AICF_StrategicAssignmentSnapshot other) DESTINATION
}
public class AICF_IntentHandoff { public bool Restored=true; public bool IsOrderRestored(){return Restored;} }
public class AICF_TransportTrip {
 public AICF_ETransportTripPhase Phase=AICF_ETransportTripPhase.COMPLETE; public AICF_IntentHandoff Handoff=new AICF_IntentHandoff(); public AICF_StrategicAssignmentSnapshot Assignment; public AICF_GroupSlot PresetSlot; public int PresetRevision=1;
 public AICF_ETransportTripPhase GetPhase(){return Phase;} public AICF_IntentHandoff GetHandoffState(){return Handoff;} public AICF_StrategicAssignmentSnapshot GetAssignment(){return Assignment;} public AICF_GroupSlot GetVehiclePresetSlot(){return PresetSlot;} public int GetVehiclePresetRevision(){return PresetRevision;}
}
public class AICF_IntentController { public bool Retire=true; public bool CanRetireTrip(AICF_TransportTrip trip){return Retire;} }
public class AICF_CompletedIntentAdapter {
 public AICF_IntentController m_TripController=new AICF_IntentController(); public bool Explicit;
 public bool HasExplicitVehicleAdmission(AICF_GroupSlot slot,AICF_StrategicAssignmentSnapshot assignment){return Explicit;}
 public void ReportAdmissionOnce(AICF_StrategicAssignmentSnapshot assignment,string reason){}
 public bool HasCompletedStrategicTransportIntent(AICF_TransportTrip trip,AICF_StrategicAssignmentSnapshot assignment,AICF_GroupSlot slot) RECEIPT
 public bool ShouldReplaceTerminalTrip(AICF_TransportTrip trip,AICF_StrategicAssignmentSnapshot assignment,AICF_GroupSlot slot) REPLACE
 public static bool[] Evaluate(int scenario){
  Replication.Server=true; var owner=new AICF_CompletedIntentAdapter(); var group=new object(); var slot=new AICF_GroupSlot(){Group=group};
  var completed=new AICF_StrategicAssignmentSnapshot(){m_Group=group}; var current=new AICF_StrategicAssignmentSnapshot(){m_Group=group,Assignment=7};
  var trip=new AICF_TransportTrip(){Assignment=completed,PresetSlot=slot};
  switch(scenario){
   case 1: current.Assignment=99;current.BaseRevision=99;break;
   case 2: owner.Explicit=true;break;
   case 3: current.Intent=4;break;
   case 4: current.m_vTargetPosition=999;break;
   case 5: slot.PresetRevision=2;break;
   case 6: slot.Generation=2;break;
   case 7: current.m_Group=new object();break;
   case 8: completed.m_Group=new object();break;
   case 9: slot.Unit=2;break;
   case 10: trip.Phase=AICF_ETransportTripPhase.FALLBACK;break;
   case 11: trip.Phase=AICF_ETransportTripPhase.FAILED_CLOSED;break;
   case 12: Replication.Server=false;break;
   case 13: trip.PresetSlot=new AICF_GroupSlot(){Group=group};break;
   case 14: trip.Handoff.Restored=false;break;
   case 15: owner.m_TripController.Retire=false;owner.Explicit=true;break;
   case 16: trip.PresetSlot=null;trip.PresetRevision=0;slot.PresetRevision=0;break;
   case 17: completed.m_TargetKind=AICF_EOrderTargetKind.BASE;current.m_TargetKind=AICF_EOrderTargetKind.BASE;completed.m_TargetBase=new object();current.m_TargetBase=new object();break;
   case 18: completed.m_TargetKind=AICF_EOrderTargetKind.BASE;current.m_TargetKind=AICF_EOrderTargetKind.BASE;completed.m_TargetBase=new object();current.m_TargetBase=completed.m_TargetBase;current.m_vTargetPosition=999;break;
  }
  return new bool[]{owner.HasCompletedStrategicTransportIntent(trip,current,slot),owner.ShouldReplaceTerminalTrip(trip,current,slot)};
 }
}
'@
$adapter=$adapter.Replace('CURRENT',(CSharpBody (Body $snapshot 'MatchesCurrent'))).Replace('DESTINATION',(CSharpBody (Body $snapshot 'MatchesDestination')))
$adapter=$adapter.Replace('RECEIPT',(CSharpBody $receipt)).Replace('REPLACE',(CSharpBody $replace))
if(!('AICF_CompletedIntentAdapter' -as [type])){Add-Type -TypeDefinition $adapter}
foreach($case in @(
 @(0,$true,$false,'normal handoff assignment6 to7 same intent3'),
 @(1,$true,$false,'assignment/base churn cannot rearm'),
 @(2,$true,$true,'explicit admission remains available'),
 @(3,$false,$true,'new intent'),@(4,$false,$true,'new point'),@(5,$false,$true,'new preset'),
 @(6,$false,$true,'new generation'),@(7,$false,$true,'foreign current group'),@(8,$false,$true,'foreign completed group'),
 @(9,$false,$true,'new unit type'),@(10,$false,$true,'fallback retains replacement policy'),@(11,$false,$true,'failedclosed retains replacement policy'),
 @(12,$false,$true,'client cannot prove completed receipt'),@(13,$false,$true,'foreign preset slot'),@(14,$false,$true,'incomplete handoff cannot prove success'),
 @(15,$true,$false,'explicit cannot skip cleanup'),@(16,$true,$false,'automatic transport also keeps completed intent'),
 @(17,$false,$true,'new base identity'),@(18,$true,$false,'same base tolerates position refresh')
)){
 $actual=[AICF_CompletedIntentAdapter]::Evaluate($case[0])
 if($actual[0] -ne $case[1] -or $actual[1] -ne $case[2]){throw "Completed intent behavior failed: $($case[3]) actual=$actual"}
 $controls++
}
"PASS Completed transport intent: normal exact COMPLETE receipt survives assignment/base/waypoint rebuild; new intent/target/preset/explicit requests keep original cleanup and admission gates; controls=$controls"
