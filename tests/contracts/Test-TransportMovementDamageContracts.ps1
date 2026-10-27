param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $inspect=Body $text 'InspectVehicleFailure';$tick=Body $text 'Tick';$prepare=Body $text 'PrepareRoute'
 if(!$inspect -or !$tick -or !$prepare){'MISSING_METHOD';return}
 foreach($guard in @('SCR_DamageManagerComponent.GetDamageManager(vehicle)',
  'movementDamageManager.GetMovementDamage() >= 1.0',
  '!SCR_AIVehicleUsability.VehicleCanMove(vehicle, movementDamageManager)',
  'SCR_DamageManagerComponent.GetDamageManager(vehicle) == movementDamageManager',
  'return "VEHICLE_MOVEMENT_DESTROYED";')){
  if(!$inspect.Contains($guard)){'CONFIRMED_NATIVE_MOVEMENT_DAMAGE';break}
 }
 if($inspect -notmatch 'if \(movementDamageManager &&' -or
  ([regex]::Matches($inspect,'SCR_DamageManagerComponent.GetDamageManager\(vehicle\)')).Count -ne 2){'REPEATED_EXACT_DAMAGE_MANAGER'}
 foreach($body in @($tick,$prepare)){
  if($body -notmatch '(?s)InspectRouteAssetFailure\(trip\).*?ReportVehicleFailureSnapshot\(trip, vehicleFailure\);\s*AbortOwnedActions\(trip\);\s*return AICF_TripOutcome.FallbackToFoot\(vehicleFailure, causationId\);'){'TYPED_FALLBACK_WITH_SNAPSHOT_AND_OWNED_CANCELLATION'}
 }
 foreach($later in @('AdvancePendingNativeCrewEntries','ObserveNativeCombat','ObserveNativeGate','ProcessMandatoryCrew')){
  if($tick.IndexOf('InspectRouteAssetFailure') -gt $tick.IndexOf($later)){'DAMAGE_BEFORE_NEW_OR_PENDING_CREW';break}
 }
 if($inspect.IndexOf('VEHICLE_MOVEMENT_DESTROYED') -lt $inspect.IndexOf('VEHICLE_OVERTURNED')){'EXISTING_ASSET_FAILURE_PRIORITY'}
 if((ConvertTo-AICFCodeText $inspect) -match 'GetVelocity|GetTickCount|Observe|Set\w*\(|AddAction|BeginCrewRecovery|Teleport|Commit|BumpMe'){'PURE_HEALTH_NO_MOTION_OR_CLOCK_MUTATION'}
}
$failures=@(Rules $source)
if($failures.Count){throw "Transport movement damage: $failures"}
$controls=1
foreach($mutation in @(
 @('movementDamageManager.GetMovementDamage() >= 1.0','true','CONFIRMED_NATIVE_MOVEMENT_DAMAGE'),
 @('!SCR_AIVehicleUsability.VehicleCanMove(vehicle, movementDamageManager)','true','CONFIRMED_NATIVE_MOVEMENT_DAMAGE'),
 @('SCR_DamageManagerComponent.GetDamageManager(vehicle) == movementDamageManager','true','REPEATED_EXACT_DAMAGE_MANAGER'),
 @('if (movementDamageManager &&','if (true &&','REPEATED_EXACT_DAMAGE_MANAGER'),
 @('return "VEHICLE_MOVEMENT_DESTROYED";','return string.Empty;','CONFIRMED_NATIVE_MOVEMENT_DAMAGE'),
 @('ReportVehicleFailureSnapshot(trip, vehicleFailure);','Print("lost snapshot");','TYPED_FALLBACK_WITH_SNAPSHOT_AND_OWNED_CANCELLATION')
)){
 if(!$source.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$inspect=Body $source 'InspectVehicleFailure'
# Execute the production health classifier. The adapter changes only native
# reference syntax, Type tokens and the fixed vector array declaration.
$body=$inspect.Replace('!vehicle)','vehicle == null)').Replace('!usage ||','usage == null ||')
$body=$body.Replace('if (movementDamageManager &&','if (movementDamageManager != null &&')
$body=$body.Replace('vehicle.FindComponent(SCR_AIVehicleUsageComponent)','vehicle.FindComponent(typeof(SCR_AIVehicleUsageComponent))')
$body=$body.Replace('vector transform[4];','vector[] transform = new vector[4];')
$adapter=@'
using System;
public enum EDamageState { UNDAMAGED, DESTROYED }
public struct vector { public float Y; public float this[int index] { get {return Y;} } }
public class SCR_AIVehicleUsageComponent {
 public EDamageState State; public EDamageState GetDamageState(){return State;}
 public static SCR_AIVehicleUsageComponent Cast(object value){return value as SCR_AIVehicleUsageComponent;}
}
public class SCR_DamageManagerComponent {
 public float MovementDamage; public float GetMovementDamage(){return MovementDamage;}
 public static SCR_DamageManagerComponent GetDamageManager(Vehicle vehicle){return vehicle.ReadDamageManager();}
}
public class Vehicle {
 public SCR_AIVehicleUsageComponent Usage=new SCR_AIVehicleUsageComponent();
 public SCR_DamageManagerComponent Damage=new SCR_DamageManagerComponent(),Replacement;
 public bool Fire,CanMove=true;public float Up=1; public int Reads;
 public object FindComponent(Type type){return Usage;}
 public void GetWorldTransform(vector[] output){output[1]=new vector(){Y=Up};}
 public SCR_DamageManagerComponent ReadDamageManager(){Reads++;return Reads>1 && Replacement!=null?Replacement:Damage;}
}
public static class SCR_AIVehicleUsability {
 public static bool VehicleIsOnFire(Vehicle vehicle){return vehicle.Fire;}
 public static bool VehicleCanMove(Vehicle vehicle,SCR_DamageManagerComponent damage){return vehicle.CanMove;}
}
public class AICF_MovementDamageAdapter {
 public string InspectVehicleFailure(Vehicle vehicle) BODY
 public static string Evaluate(int scenario){
  var vehicle=new Vehicle();
  switch(scenario){
   case 1: vehicle.Damage.MovementDamage=1;vehicle.CanMove=false;break;
   case 2: vehicle.Damage.MovementDamage=0.999f;vehicle.CanMove=false;break;
   case 3: vehicle.CanMove=false;break;
   case 4: vehicle.Damage.MovementDamage=1;break;
   case 5: vehicle.Damage.MovementDamage=1;vehicle.CanMove=false;vehicle.Replacement=new SCR_DamageManagerComponent(){MovementDamage=1};break;
   case 6: vehicle.Damage=null;vehicle.CanMove=false;break;
   case 7: vehicle.Fire=true;vehicle.Damage.MovementDamage=1;vehicle.CanMove=false;break;
   case 8: vehicle.Usage.State=EDamageState.DESTROYED;vehicle.Damage.MovementDamage=1;vehicle.CanMove=false;break;
   case 9: vehicle.Up=0.249f;vehicle.Damage.MovementDamage=1;vehicle.CanMove=false;break;
   case 10: vehicle.Usage=null;break;
   case 11: vehicle=null;break;
   case 12: vehicle.Damage.MovementDamage=float.NaN;vehicle.CanMove=false;break;
   case 13: vehicle.Up=0.25f;break;
   case 14: vehicle.Damage.MovementDamage=0.99f;break;
  }
  return new AICF_MovementDamageAdapter().InspectVehicleFailure(vehicle);
 }
}
'@
if(!('AICF_MovementDamageAdapter' -as [type])){Add-Type -TypeDefinition $adapter.Replace('BODY',$body)}
foreach($case in @(
 @(0,'','healthy'),@(1,'VEHICLE_MOVEMENT_DESTROYED','native damage1, no crew admission'),
 @(2,'','partial damage below native threshold'),@(3,'','temporary unusability without movement damage'),
 @(4,'','contradictory native usability cannot prove destroyed movement'),@(5,'','replaced manager cannot prove exact damage'),
 @(6,'','missing manager cannot prove damage'),@(7,'VEHICLE_ON_FIRE','fire priority preserved'),
 @(8,'VEHICLE_DESTROYED','destroyed priority preserved'),@(9,'VEHICLE_OVERTURNED','overturn priority preserved'),
 @(10,'VEHICLE_DESTROYED','missing usage'),@(11,'VEHICLE_DESTROYED','missing vehicle'),
 @(12,'','NaN cannot prove threshold'),@(13,'','existing upright threshold'),@(14,'','partial damage healthy usability')
)){
 $actual=[AICF_MovementDamageAdapter]::Evaluate($case[0])
 if($actual -cne $case[1]){throw "Movement damage case failed: $($case[2]) actual=$actual expected=$($case[1])"}
 $controls++
}
$coordinator=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c'))
$monitor=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Diagnostics/AICF_VehicleAcceptanceMonitor.c'))
function DomainRules([string]$text,[string]$acceptance){
 $classify=Body $text 'ResolveTerminalFailureDomain';$observe=Body $acceptance 'ObserveCommittedTerminal';$pair=Body $acceptance 'TerminalPairMatches'
 if($classify -notmatch 'terminalOutcome.GetKind\(\) != AICF_ETripOutcomeKind.FALLBACK_TO_FOOT' -or
  $classify.IndexOf('terminalOutcome.GetKind()') -gt $classify.IndexOf('terminalReason')){'ONLY_COMMITTED_FALLBACK_CLASSIFICATION'}
 if($classify -notmatch '(?s)if \(terminalReason == "VEHICLE_MOVEMENT_DESTROYED" &&\s*\(previousPhase == AICF_ETransportTripPhase.BOARDING \|\| previousPhase == AICF_ETransportTripPhase.TRANSIT\)\)\s*\{[^}]*return AICF_EVehicleAcceptanceFailureDomain.NONE;'){'EXACT_DAMAGE_REASON_AND_PHASES_ONLY'}
 foreach($rule in @('case AICF_ETransportTripPhase.BOARDING:\s*return AICF_EVehicleAcceptanceFailureDomain.BOARDING;',
  'case AICF_ETransportTripPhase.TRANSIT:\s*return AICF_EVehicleAcceptanceFailureDomain.RECOVERY;',
  'case AICF_ETransportTripPhase.DISMOUNT:\s*return AICF_EVehicleAcceptanceFailureDomain.DISMOUNT;')){if($classify -notmatch $rule){'OTHER_RECOVERY_AND_DISMOUNT_FAILURES_PRESERVED'}}
 if($classify -notmatch 'terminalReason == "VEHICLE_ON_FIRE"' -or $classify -notmatch 'terminalReason == "VEHICLE_ON_FIRE_DURING_BOARDING"'){'EXISTING_FIRE_POLICY_PRESERVED'}
 if($observe -notmatch '!trip.IsTerminal\(\) \|\| !TerminalPairMatches\(trip, outcome\)' -or
  $observe.IndexOf('!TerminalPairMatches') -gt $observe.IndexOf('record.RecordTerminal(trip, outcome)') -or
  $observe.IndexOf('record.RecordTerminal(trip, outcome)') -gt $observe.IndexOf('failureDomain != AICF_EVehicleAcceptanceFailureDomain.NONE') -or
  $observe -notmatch 'if \(outcome.GetKind\(\) == AICF_ETripOutcomeKind.TERMINAL_FAIL_CLOSED\)'){'MONITOR_TERMINAL_PAIR_AND_FAIL_CLOSED_IDENTITY_PRESERVED'}
 if($observe -notmatch '(?s)if \(outcome.GetKind\(\) != AICF_ETripOutcomeKind.COMPLETE_TRIP\)\s*\{\s*EvaluateCandidate\(\);\s*return;\s*\}' -or
  $observe.IndexOf('outcome.GetKind() != AICF_ETripOutcomeKind.COMPLETE_TRIP') -gt $observe.IndexOf('record.MarkComplete(') -or
  $pair -notmatch 'return trip.GetPhase\(\) == AICF_ETransportTripPhase.FALLBACK;'){'BOUNDED_FALLBACK_NEVER_BECOMES_NORMAL_COMPLETE'}
}
$domainFailures=@(DomainRules $coordinator $monitor)
if($domainFailures.Count){throw "Movement damage accounting: $domainFailures"}
foreach($mutation in @(
 @('terminalReason == "VEHICLE_MOVEMENT_DESTROYED"','terminalReason.Contains("VEHICLE_MOVEMENT_DESTROYED")','EXACT_DAMAGE_REASON_AND_PHASES_ONLY'),
 @('previousPhase == AICF_ETransportTripPhase.BOARDING || previousPhase == AICF_ETransportTripPhase.TRANSIT','true','EXACT_DAMAGE_REASON_AND_PHASES_ONLY'),
 @('terminalOutcome.GetKind() != AICF_ETripOutcomeKind.FALLBACK_TO_FOOT','false','ONLY_COMMITTED_FALLBACK_CLASSIFICATION'),
 @('return AICF_EVehicleAcceptanceFailureDomain.RECOVERY;','return AICF_EVehicleAcceptanceFailureDomain.NONE;','OTHER_RECOVERY_AND_DISMOUNT_FAILURES_PRESERVED')
)){
 if(!$coordinator.Contains($mutation[0])){throw "Missing accounting mutation: $($mutation[2])"}
 if($mutation[2] -notin @(DomainRules ($coordinator.Replace($mutation[0],$mutation[1])) $monitor)){throw "Accounting mutation survived: $($mutation[2])"};$controls++
}
foreach($mutation in @(
 @('!trip.IsTerminal() || !TerminalPairMatches(trip, outcome)','false','MONITOR_TERMINAL_PAIR_AND_FAIL_CLOSED_IDENTITY_PRESERVED'),
 @('outcome.GetKind() != AICF_ETripOutcomeKind.COMPLETE_TRIP','false','BOUNDED_FALLBACK_NEVER_BECOMES_NORMAL_COMPLETE')
)){
 if($mutation[2] -notin @(DomainRules $coordinator ($monitor.Replace($mutation[0],$mutation[1])))){throw "Monitor mutation survived: $($mutation[2])"};$controls++
}
# Actual production classifier and Monitor/Record bodies, with native references
# replaced only by small observable adapters. No functional log gate is altered.
$domainBody=(Body $coordinator 'ResolveTerminalFailureDomain').Replace('!terminalOutcome ||','terminalOutcome == null ||')
$observeBody=(Body $monitor 'ObserveCommittedTerminal').Replace('!trip ||','trip == null ||').Replace('!outcome ||','outcome == null ||').Replace('if (!record)','if (record == null)')
$domainAdapter=@'
namespace AICF_DamageDomainCheck {
 public enum AICF_EVehicleAcceptanceFailureDomain { NONE,BOARDING,DISMOUNT,RECOVERY,CLEANUP,IDENTITY,DIAGNOSTICS }
 public enum AICF_ETransportTripPhase { BOARDING,TRANSIT,DISMOUNT,COMPLETE,FALLBACK,FAILED_CLOSED,HANDOFF }
 public enum AICF_ETripOutcomeKind { COMPLETE_TRIP,FALLBACK_TO_FOOT,TERMINAL_FAIL_CLOSED,WAIT }
 public enum AICF_EVehicleKind { TRANSPORT,LIGHT_TRANSPORT,ARMED_LIGHT,OTHER }
 public class AICF_TripOutcome {
  public AICF_ETripOutcomeKind Kind=AICF_ETripOutcomeKind.FALLBACK_TO_FOOT; public string Reason="VEHICLE_MOVEMENT_DESTROYED";
  public AICF_ETripOutcomeKind GetKind(){return Kind;} public string GetReason(){return Reason;} public string GetCausationId(){return "C1";}
  public bool IsTerminal(){return Kind!=AICF_ETripOutcomeKind.WAIT;}
 }
 public class AICF_VehicleLease { public AICF_EVehicleKind GetKind(){return AICF_EVehicleKind.LIGHT_TRANSPORT;} }
 public class AICF_TransportTrip {
  public AICF_ETransportTripPhase Phase=AICF_ETransportTripPhase.FALLBACK; public bool Terminal=true;
  public bool IsTerminal(){return Terminal;} public AICF_ETransportTripPhase GetPhase(){return Phase;}
  public string GetFactionKey(){return "US";} public int GetSlotId(){return 1;} public int GetGroupGeneration(){return 1;} public int GetTripGeneration(){return 1;} public string GetOperationId(){return "O1";}
  public AICF_VehicleLease GetLease(){return new AICF_VehicleLease();}
 }
 public class AICF_VehicleAcceptanceRecord {
  public bool Duplicate,Stale,m_bTransportComplete,m_bArmedComplete;
  public int m_iLastGroupGeneration,m_iLastTripGeneration;public AICF_ETripOutcomeKind m_LastOutcomeKind;public string m_sLastOperationId,m_sLastCausationId;
  public bool IsDuplicateTerminal(AICF_TransportTrip trip,AICF_TripOutcome outcome){return Duplicate;}
  public bool IsStaleOrCollidingTerminal(AICF_TransportTrip trip){return Stale;}
  public void RecordTerminal(AICF_TransportTrip trip,AICF_TripOutcome outcome) RECORD_BODY
  public bool MarkComplete(AICF_EVehicleKind kind) MARK_BODY
 }
 public class Adapter {
  public bool m_bStopped,MissingRecord,CompletionIdentity=true; public int Failures,Evaluations;public AICF_EVehicleAcceptanceFailureDomain LastDomain;
  public AICF_VehicleAcceptanceRecord Record=new AICF_VehicleAcceptanceRecord();
  public AICF_VehicleAcceptanceRecord FindRecord(string faction,int slot){return MissingRecord?null:Record;}
  public void ObserveTripFailure(AICF_TransportTrip trip,AICF_EVehicleAcceptanceFailureDomain domain,string reason){Failures++;LastDomain=domain;}
  public void EvaluateCandidate(){Evaluations++;} public bool HasCompletionIdentity(AICF_TransportTrip trip,AICF_VehicleLease lease){return CompletionIdentity;}
  public bool TerminalPairMatches(AICF_TransportTrip trip,AICF_TripOutcome outcome) PAIR_BODY
  public AICF_EVehicleAcceptanceFailureDomain ResolveTerminalFailureDomain(AICF_ETransportTripPhase previousPhase,AICF_TripOutcome terminalOutcome) DOMAIN_BODY
  public void ObserveCommittedTerminal(AICF_TransportTrip trip,AICF_TripOutcome outcome,AICF_EVehicleAcceptanceFailureDomain failureDomain) OBSERVE_BODY
  public static int Classify(string reason,int previousPhase,int kind){return (int)new Adapter().ResolveTerminalFailureDomain((AICF_ETransportTripPhase)previousPhase,new AICF_TripOutcome(){Reason=reason,Kind=(AICF_ETripOutcomeKind)kind});}
  public static int[] MonitorCase(int scenario){
   var owner=new Adapter();var trip=new AICF_TransportTrip();var outcome=new AICF_TripOutcome();var phase=AICF_ETransportTripPhase.TRANSIT;
   switch(scenario){
    case 1: phase=AICF_ETransportTripPhase.BOARDING;break;
    case 2: trip.Phase=AICF_ETransportTripPhase.COMPLETE;break;
    case 3: outcome.Kind=AICF_ETripOutcomeKind.COMPLETE_TRIP;break;
    case 4: trip.Phase=AICF_ETransportTripPhase.FAILED_CLOSED;outcome.Kind=AICF_ETripOutcomeKind.TERMINAL_FAIL_CLOSED;break;
    case 5: trip.Terminal=false;break;
    case 6: outcome.Kind=AICF_ETripOutcomeKind.WAIT;break;
    case 7: phase=AICF_ETransportTripPhase.DISMOUNT;break;
    case 8: outcome.Reason="DRIVER_RECOVERY_EXHAUSTED";break;
    case 9: outcome.Reason="GUNNER_RECOVERY_EXHAUSTED";break;
    case 10: owner.MissingRecord=true;break;
    case 11: owner.Record.Stale=true;break;
    case 12: owner.Record.Duplicate=true;break;
    case 13: trip.Phase=AICF_ETransportTripPhase.COMPLETE;outcome.Kind=AICF_ETripOutcomeKind.COMPLETE_TRIP;break;
    case 14: trip.Phase=AICF_ETransportTripPhase.COMPLETE;outcome.Kind=AICF_ETripOutcomeKind.COMPLETE_TRIP;owner.CompletionIdentity=false;break;
    case 15: trip=null;break;
    case 16: outcome=null;break;
    case 17: owner.m_bStopped=true;break;
   }
   var domain=owner.ResolveTerminalFailureDomain(phase,outcome);owner.ObserveCommittedTerminal(trip,outcome,domain);
   return new int[]{owner.Record.m_iLastGroupGeneration,owner.Record.m_bTransportComplete?1:0,owner.Failures,(int)owner.LastDomain,owner.Evaluations};
  }
 }
}
'@
$domainAdapter=$domainAdapter.Replace('RECORD_BODY',(Body $monitor 'RecordTerminal')).Replace('MARK_BODY',(Body $monitor 'MarkComplete')).Replace('PAIR_BODY',(Body $monitor 'TerminalPairMatches')).Replace('DOMAIN_BODY',$domainBody).Replace('OBSERVE_BODY',$observeBody)
if(!('AICF_DamageDomainCheck.Adapter' -as [type])){Add-Type -TypeDefinition $domainAdapter}
foreach($case in @(
 @('VEHICLE_MOVEMENT_DESTROYED',0,1,0,'boarding exact bounded damage'),@('VEHICLE_MOVEMENT_DESTROYED',1,1,0,'transit exact bounded damage'),
 @('VEHICLE_MOVEMENT_DESTROYED',2,1,2,'wrong dismount phase preserves failure'),@('VEHICLE_MOVEMENT_DESTROYED_SUFFIX',1,1,3,'suffix cannot spoof'),
 @('PREFIX_VEHICLE_MOVEMENT_DESTROYED',0,1,1,'prefix cannot spoof'),@('DRIVER_RECOVERY_EXHAUSTED',1,1,3,'driver failure remains'),
 @('GUNNER_RECOVERY_EXHAUSTED',1,1,3,'gunner failure remains'),@('RECOVERY_BUDGET_EXHAUSTED',1,1,3,'budget failure remains'),
 @('UNKNOWN',1,1,3,'unknown failure remains'),@('PRESET_BOARDING_DEADLINE',0,1,1,'boarding deadline remains'),
 @('VEHICLE_ON_FIRE',1,1,0,'existing fire'),@('VEHICLE_ON_FIRE_DURING_BOARDING',0,1,0,'existing boarding fire'),
 @('VEHICLE_MOVEMENT_DESTROYED',1,2,0,'failclosed domain reaches Monitor identity handling'),@('VEHICLE_MOVEMENT_DESTROYED',1,0,0,'complete kind never bypasses Monitor phase identity')
)){
 $actual=[AICF_DamageDomainCheck.Adapter]::Classify($case[0],$case[1],$case[2]);if($actual -ne $case[3]){throw "Damage domain failed: $($case[4]) actual=$actual expected=$($case[3])"};$controls++
}
foreach($case in @(
 @(0,'1,0,0,0,1','fallback recorded, never normal complete'),@(1,'1,0,0,0,1','boarding bounded fallback recorded'),
 @(2,'0,0,1,5,0','fallback/complete phase mismatch'),@(3,'0,0,1,5,0','complete/fallback mismatch'),
 @(4,'1,0,1,5,0','terminal failclosed retains identity failure'),@(5,'0,0,1,5,0','uncommitted terminal retains identity failure'),
 @(6,'0,0,0,0,0','nonterminal observation ignored'),@(7,'1,0,1,2,0','dismount failure not exempt'),
 @(8,'1,0,1,3,0','driver failure not exempt'),@(9,'1,0,1,3,0','gunner failure not exempt'),
 @(10,'0,0,1,5,0','unregistered slot rejected'),@(11,'0,0,1,5,0','stale terminal rejected'),
 @(12,'0,0,0,0,0','duplicate remains idempotent'),@(13,'1,1,0,0,1','real COMPLETE positive control'),
 @(14,'1,0,1,5,0','completion identity proof mandatory'),@(15,'0,0,0,0,0','null trip ignored'),
 @(16,'0,0,0,0,0','null outcome ignored'),@(17,'0,0,0,0,0','stopped monitor ignored')
)){
 $actual=[AICF_DamageDomainCheck.Adapter]::MonitorCase($case[0]) -join ',';if($actual -ne $case[1]){throw "Actual Monitor damage accounting failed: $($case[2]) actual=$actual expected=$($case[1])"};$controls++
}
"PASS Transport movement damage: exact native health classifier, bounded damage domain only, actual Monitor terminal receipt/phase/failclosed guards, no normal-complete forgery; controls=$controls"
