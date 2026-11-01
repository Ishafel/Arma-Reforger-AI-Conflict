param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Read($p){[IO.File]::ReadAllText((Join-Path $root $p))}
function Body($s,$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
$identity=Read 'State/Vehicles/AICF_TripRetirementIdentity.c';$trip=Read 'State/Vehicles/AICF_TransportTrip.c'
$owner=Read 'Vehicles/AICF_TransportTripController.c';$facade=Read 'Vehicles/AICF_VehicleCoordinator.c'
$monitor=Read 'Diagnostics/AICF_VehicleAcceptanceMonitor.c';$manager=Read 'Vehicles/AICF_VehicleCleanupManager.c'
function Rules($i,$o,$f,$m,$c){
 $stale=Body $o 'TerminateStaleTrip';$auth=Body $i 'RecordOrphanTerminalCommit';$classify=Body $i 'ClassifyObservation'
 if($stale.IndexOf('if (trip.IsTerminal())') -gt $stale.IndexOf('bool capturedDeadBeforeTransition') -or
  !$stale.Contains('bool capturedDeadBeforeTransition = trip.HasCapturedDeadRetirementEvidence();') -or
  !$stale.Contains('if (capturedDeadBeforeTransition) trip.RecordCapturedDeadRetirementCommit(fleet);') -or
  $stale.IndexOf('RecordCapturedDeadRetirementCommit') -lt $stale.IndexOf('if (!TransitionTo(')){'ONLY_OWN_NONTERMINAL_ALLDEAD_BEFORE_TRANSITION_CAUSE'}
 foreach($part in @('!BindCleanupLease(trip, fleet, trip.GetLease())','m_iAuthorizedTerminalTransition = trip.GetTransitionCount();','m_sAuthorizedTerminalCausation = trip.GetCausationId();')){if(!$auth.Contains($part)){'EXACT_BOUND_LEASE_AND_OWN_COMMIT';break}}
 foreach($part in @('trip.GetTransitionCount() != m_iAuthorizedTerminalTransition','trip.GetCausationId() != m_sAuthorizedTerminalCausation','m_bCleanupContextCaptured && m_bAcknowledgementReceipt','m_bPoolReleaseReceipt || (m_bRetirementReleaseReceipt && m_bAbsenceReceipt)','completedAt <= m_iCleanupTerminalAt','handoff.IsCleanupRetainedFailClosed()','!handoff.IsCleanupQueueAccepted()','!handoff.IsClearanceSafe()')){if(!$classify.Contains($part)){'OWN_IMMUTABLE_RELEASE_ACK_AND_POSTCONDITIONS';break}}
 if($classify.IndexOf('completedAt <= m_iCleanupTerminalAt') -gt $classify.IndexOf('!MatchesAbsentGroup(trip)') -or $classify.Contains('GetActiveCount(') -or $classify.Contains('GetWorldPoolCount(')){'HISTORICAL_RELEASE_SURVIVES_REACQUIRE_OTHER_LEASES'}
 foreach($pair in @(@('ReportWorldPoolReleased','ObservePhysicalRelease(job, true)'),@('ReportRetirementQuarantined','ObservePhysicalRelease(job, false)'),@('ReportCleanupConfirmed','ObserveConfirmedAbsence(job)'),@('AcknowledgeLeaseRelease','ObserveAcknowledgement(job)'))){if(!(Body $c $pair[0]).Contains($pair[1])){'NATIVE_OWNER_RECEIPTS_AT_ACTUAL_BOUNDARY';break}}
 $physical=Body $i 'ObservePhysicalRelease'
 if(!$physical.Contains('vehicle = job.m_WorldPoolAsset.GetVehicle()') -or !$physical.Contains('vehicle = job.m_RetirementAsset.GetVehicle()') -or $physical.Contains('vehicle = m_CleanupLease.GetVehicle()')){'ACTUAL_POST_TRANSFER_PHYSICAL_REFERENCE'}
 $bridge=Body $f 'ObserveTerminalCommit'
 if(!$bridge.Contains('AICF_ETripRetirementObservation.PENDING && !m_bStopped') -or !$bridge.Contains('ObserveConfirmedCapturedDeathRetirement(trip, retirement)') -or !$bridge.Contains('CAPTURED_DEATH_RETIREMENT_UNCONFIRMED') -or
  !$bridge.Contains('m_aPendingCapturedDeathRetirements.Insert(trip)') -or !(Body $f 'StopWithFactionContexts').Contains('m_aPendingCapturedDeathRetirements.Clear()')){'BOUNDED_PENDING_NOT_PASS_STOP_AND_RETIRED_REGISTRY'}
 $confirm=Body $m 'ObserveConfirmedCapturedDeathRetirement'
 if(!$confirm.Contains('!evidence.IsConfirmed() || !evidence.Matches(trip)') -or $confirm -match 'MarkComplete|RecordTerminal\(|m_bFailureLatched\s*=|m_iFailureCount\s*=|Clear|Reset|GetWorld|GetLease\('){'MONITOR_ONLY_IMMUTABLE_PROJECTION_PRESERVES_EARLIER_FAILURE'}
 if(!$confirm.Contains('m_aConfirmedCapturedDeaths.Find(key)') -or !$confirm.Contains('m_aConfirmedCapturedDeaths.Insert(key)')){'DEDUP_SEPARATE_FROM_LATER_TERMINAL_HISTORY'}
}
$bad=@(Rules $identity $owner $facade $monitor $manager);if($bad.Count){throw "Retirement projection source: $bad"}
foreach($mutation in @(
 @('i','!BindCleanupLease(trip, fleet, trip.GetLease())','false'),
 @('i','trip.GetTransitionCount() != m_iAuthorizedTerminalTransition','false'),
 @('i','trip.GetCausationId() != m_sAuthorizedTerminalCausation','false'),
 @('i','m_bCleanupContextCaptured && m_bAcknowledgementReceipt','true'),
 @('i','completedAt <= m_iCleanupTerminalAt','true'),
 @('i','vehicle = job.m_WorldPoolAsset.GetVehicle()','vehicle = m_CleanupLease.GetVehicle()'),
 @('o','if (capturedDeadBeforeTransition) trip.RecordCapturedDeadRetirementCommit(fleet);','trip.RecordCapturedDeadRetirementCommit(fleet);'),
 @('f','AICF_ETripRetirementObservation.PENDING && !m_bStopped','AICF_ETripRetirementObservation.PENDING'),
 @('m','m_aConfirmedCapturedDeaths.Insert(key);','m_bFailureLatched = false;'),
 @('m','EvaluateCandidate();','record.MarkComplete(); EvaluateCandidate();'),
 @('c','ObserveAcknowledgement(job)','SkipAcknowledgement(job)')
)){
 $i=$identity;$o=$owner;$f=$facade;$m=$monitor;$c=$manager
 switch($mutation[0]){i{$i=$i.Replace($mutation[1],$mutation[2])}o{$o=$o.Replace($mutation[1],$mutation[2])}f{$f=$f.Replace($mutation[1],$mutation[2])}m{$m=$m.Replace($mutation[1],$mutation[2])}c{$c=$c.Replace($mutation[1],$mutation[2])}}
 if(!@(Rules $i $o $f $m $c).Count){throw "Projection mutation survived: $($mutation[1])"}
}
$classify=Body $identity 'ClassifyObservation'
$classify=$classify.Replace('!trip ||','trip == null ||').Replace('!m_CleanupLease ||','m_CleanupLease == null ||').Replace('!m_CleanupFleet)','m_CleanupFleet == null)').Replace('!handoff.GetCleanupFailureReason().IsEmpty()','!string.IsNullOrEmpty(handoff.GetCleanupFailureReason())').Replace('|| trip.GetLease())','|| trip.GetLease() != null)').Replace('System.GetTickCount()','Clock')
$code=@'
using System;
public enum AICF_ETripRetirementObservation {NONE,PENDING,CONFIRMED,REJECTED}
public enum AICF_ETransportTripPhase {TRANSIT,FAILED_CLOSED}
public class AICF_VehicleHandoffState {public bool Queue=true,Released=true,Clear=true,Retained;public string Failure="";public bool IsCleanupQueueAccepted(){return Queue;}public bool IsCleanupReleaseComplete(){return Released;}public bool IsClearanceSafe(){return Clear;}public bool IsCleanupRetainedFailClosed(){return Retained;}public string GetCleanupFailureReason(){return Failure;}}
public class AICF_ProjectionTrip {public AICF_ETransportTripPhase Phase=AICF_ETransportTripPhase.FAILED_CLOSED;public int Count=7;public string Cause="own-cause";public object Lease;public AICF_VehicleHandoffState Handoff=new AICF_VehicleHandoffState();public AICF_ETransportTripPhase GetPhase(){return Phase;}public int GetTransitionCount(){return Count;}public string GetCausationId(){return Cause;}public bool IsTerminal(){return Phase==AICF_ETransportTripPhase.FAILED_CLOSED;}public object GetLease(){return Lease;}public AICF_VehicleHandoffState GetHandoffState(){return Handoff;}}
public class AICF_ProjectionQuery {
 public bool m_bAllDeadObserved=true,m_bCleanupContextCaptured=true,m_bAcknowledgementReceipt=true,m_bPoolReleaseReceipt=true,m_bRetirementReleaseReceipt,m_bAbsenceReceipt;
 public int m_iAuthorizedTerminalTransition=7,m_iReleaseReceiptAt=20,m_iAcknowledgementAt=25,m_iAbsenceReceiptAt,m_iCleanupTerminalAt=60,m_iObservationDeadlineAt=59,Clock=30;
 public string m_sAuthorizedTerminalCausation="own-cause";public object m_CleanupLease=new object(),m_CleanupFleet=new object();public bool Absent=true,Live,Known=true;
 public bool MatchesAbsentGroup(AICF_ProjectionTrip trip){return Absent;}public bool MatchesLiveCapturedGroup(AICF_ProjectionTrip trip){return Live;}public bool AllObservedDeadRemainKnown(){return Known;}
 public AICF_ETripRetirementObservation ClassifyObservation(AICF_ProjectionTrip trip) BODY
}
'@
Add-Type -TypeDefinition ($code.Replace('BODY',$classify))
foreach($case in @('pool-confirmed','historical-reacquired-other-active','destructive-confirmed','pending','pending-expired','missing-own-context','wrong-cause','wrong-transition','earlier-terminal-no-authorization','earlier-fallback','retained-cleanup','lease-still-attached','late-actual-release','destructive-not-absent','missing-ack','evidence-lost')){
 $q=[AICF_ProjectionQuery]::new();$t=[AICF_ProjectionTrip]::new();$expected=[AICF_ETripRetirementObservation]::CONFIRMED
 switch($case){
  'historical-reacquired-other-active'{$q.Absent=$false;$q.Live=$false;$q.Known=$false;$q.Clock=10000}
  'destructive-confirmed'{$q.m_bPoolReleaseReceipt=$false;$q.m_bRetirementReleaseReceipt=$true;$q.m_bAbsenceReceipt=$true;$q.m_iAbsenceReceiptAt=45}
  'pending'{$q.m_bPoolReleaseReceipt=$false;$q.m_bAcknowledgementReceipt=$false;$expected=[AICF_ETripRetirementObservation]::PENDING}
  'pending-expired'{$q.m_bPoolReleaseReceipt=$false;$q.m_bAcknowledgementReceipt=$false;$q.Clock=61;$expected=[AICF_ETripRetirementObservation]::REJECTED}
  'missing-own-context'{$q.m_bCleanupContextCaptured=$false;$expected=[AICF_ETripRetirementObservation]::PENDING}
  'wrong-cause'{$t.Cause='earlier-unrelated-cause';$expected=[AICF_ETripRetirementObservation]::NONE}
  'wrong-transition'{$t.Count=8;$expected=[AICF_ETripRetirementObservation]::NONE}
  'earlier-terminal-no-authorization'{$q.m_iAuthorizedTerminalTransition=0;$expected=[AICF_ETripRetirementObservation]::NONE}
  'earlier-fallback'{$t.Phase=[AICF_ETransportTripPhase]::TRANSIT;$expected=[AICF_ETripRetirementObservation]::NONE}
  'retained-cleanup'{$t.Handoff.Retained=$true;$expected=[AICF_ETripRetirementObservation]::REJECTED}
  'lease-still-attached'{$t.Lease=[object]::new();$expected=[AICF_ETripRetirementObservation]::REJECTED}
  'late-actual-release'{$q.m_iReleaseReceiptAt=61;$expected=[AICF_ETripRetirementObservation]::REJECTED}
  'destructive-not-absent'{$q.m_bPoolReleaseReceipt=$false;$q.m_bRetirementReleaseReceipt=$true;$expected=[AICF_ETripRetirementObservation]::PENDING}
  'missing-ack'{$q.m_bAcknowledgementReceipt=$false;$expected=[AICF_ETripRetirementObservation]::PENDING}
  'evidence-lost'{$q.m_bPoolReleaseReceipt=$false;$q.Known=$false;$expected=[AICF_ETripRetirementObservation]::REJECTED}
 }
 if($q.ClassifyObservation($t) -ne $expected){throw "Actual projection classifier failed: $case"}
}
$confirm=Body $monitor 'ObserveConfirmedCapturedDeathRetirement'
$confirm=$confirm.Replace('!trip ||','trip == null ||').Replace('!evidence ||','evidence == null ||').Replace('!record)','record == null)').Replace('"%1|%2|%3|%4|%5"','"{0}|{1}|{2}|{3}|{4}"')
$code=@'
using System; using System.Collections.Generic;
public enum AICF_EVehicleAcceptanceFailureDomain{IDENTITY}
public enum AICF_ETransportTripPhase {TRANSIT,FAILED_CLOSED}
public class AICF_TransportTrip {public AICF_ETransportTripPhase Phase=AICF_ETransportTripPhase.FAILED_CLOSED;public string Op="old-operation";public AICF_ETransportTripPhase GetPhase(){return Phase;}public string GetFactionKey(){return "USSR";}public int GetSlotId(){return 0;}public int GetGroupGeneration(){return 1;}public int GetTripGeneration(){return 1;}public string GetOperationId(){return Op;}}
public class AICF_TripRetirementObservation {public bool Confirmed=true,Identity=true;public bool IsConfirmed(){return Confirmed;}public bool Matches(AICF_TransportTrip trip){return Identity;}public string Details(){return "typed";}}
public class AICF_VehicleAcceptanceRecord {public bool Complete;public int LaterGeneration=9;}
public class AICF_ProjectionKeys {public List<string> Values=new List<string>();public int Find(string key){return Values.IndexOf(key);}public void Insert(string key){Values.Add(key);}}
public static class AICF_Stage3Diagnostics {public static int Emitted;public static void Info(string name,string details){Emitted++;}}
public class AICF_ProjectionMonitor {public bool m_bStopped,Failure;public int FailureCount;public AICF_ProjectionKeys m_aConfirmedCapturedDeaths=new AICF_ProjectionKeys();public AICF_VehicleAcceptanceRecord Record=new AICF_VehicleAcceptanceRecord();public AICF_VehicleAcceptanceRecord FindRecord(string faction,int slot){return Record;}public void ObserveTripFailure(AICF_TransportTrip trip,AICF_EVehicleAcceptanceFailureDomain domain,string reason){Failure=true;FailureCount++;}public void EvaluateCandidate(){}public void ObserveConfirmedCapturedDeathRetirement(AICF_TransportTrip trip,AICF_TripRetirementObservation evidence) BODY}
'@
Add-Type -TypeDefinition ('namespace AICFProjectionMonitor { '+$code.Replace('BODY',$confirm)+' }')
foreach($case in @('confirmed','earlier-failure-preserved','pending-not-confirmed','wrong-identity','wrong-phase','missing-record','stopped-no-effects','late-history-preserved','duplicate')){
 $m=[AICFProjectionMonitor.AICF_ProjectionMonitor]::new();$t=[AICFProjectionMonitor.AICF_TransportTrip]::new();$e=[AICFProjectionMonitor.AICF_TripRetirementObservation]::new();$expectedFailure=$false
 switch($case){'earlier-failure-preserved'{$m.Failure=$true;$m.FailureCount=4;$expectedFailure=$true}'pending-not-confirmed'{$e.Confirmed=$false;$expectedFailure=$true}'wrong-identity'{$e.Identity=$false;$expectedFailure=$true}'wrong-phase'{$t.Phase=[AICFProjectionMonitor.AICF_ETransportTripPhase]::TRANSIT;$expectedFailure=$true}'missing-record'{$m.Record=$null;$expectedFailure=$true}'stopped-no-effects'{$m.m_bStopped=$true}}
 $before=[AICFProjectionMonitor.AICF_Stage3Diagnostics]::Emitted;$m.ObserveConfirmedCapturedDeathRetirement($t,$e)
 if($case -eq 'duplicate'){$m.ObserveConfirmedCapturedDeathRetirement($t,$e);if([AICFProjectionMonitor.AICF_Stage3Diagnostics]::Emitted-$before -ne 1){throw 'Duplicate proof not deduplicated'}}
 if($m.Failure -ne $expectedFailure){throw "Actual Monitor failure latch mismatch: $case"}
 if($case -eq 'earlier-failure-preserved' -and $m.FailureCount -ne 4){throw 'Earlier failure was cleared or rewritten'}
 if($m.Record -and ($m.Record.Complete -or $m.Record.LaterGeneration -ne 9)){throw 'Natural retirement manufactured completion or overwrote later terminal history'}
}
'PASS Captured-death retirement projection: source_mutants=10 actual_classifier_cases=16 actual_monitor_cases=9; earlier failure retained, pending not PASS, historical reacquire/other leases do not undo valid release'
$bind=Body $identity 'BindCleanupLease';$cause=Body $identity 'RecordOrphanTerminalCommit'
$bind=$bind.Replace('!lease ||','lease == null ||').Replace('if (m_CleanupLease)','if (m_CleanupLease != null)')
$cause=$cause.Replace('!trip ||','trip == null ||')
$code=@'
using System;
public enum AICF_ETransportTripPhase{TRANSIT,FAILED_CLOSED}
public class AICF_FactionFleet {public bool Current=true; public string GetFactionKey(){return "USSR";}}
public class Lease {public int Epoch=1,VehicleEpoch=1;public long Id=100;public string Rpl="original";public int GetLeaseGeneration(){return Epoch;}public int GetVehicleGeneration(){return VehicleEpoch;}public long GetEntityId(){return Id;}public string GetRplId(){return Rpl;}}
public class Handoff {public bool IsLeaseReleaseRequested(){return false;}}
public class AICF_TransportTrip {public Lease Lease;public bool Terminal=true;public int Authorizations;public string GetFactionKey(){return "USSR";}public bool IsValidForRetirement(){return true;}public bool IsTerminal(){return Terminal;}public Handoff GetHandoffState(){return new Handoff();}public AICF_ETransportTripPhase GetPhase(){return AICF_ETransportTripPhase.FAILED_CLOSED;}public Lease GetLease(){return Lease;}public int GetTransitionCount(){return 7;}public string GetCausationId(){return "owned";}}
public class Identity {public bool m_bAllDeadObserved=true,Absent=true,Live=false,Known=true;public AICF_FactionFleet m_CleanupFleet=new AICF_FactionFleet();public Lease m_CleanupLease=new Lease();public int m_iCleanupLeaseGeneration=1,m_iCleanupVehicleGeneration=1,m_iAuthorizedTerminalTransition;public long m_CleanupVehicleId=100;public string m_sCleanupVehicleRpl="original",m_sAuthorizedTerminalCausation;
public bool IsFleetAssetCurrent(AICF_TransportTrip trip,AICF_FactionFleet fleet){return fleet!=null&&fleet.Current;}public bool MatchesAbsentGroup(AICF_TransportTrip trip){return Absent;}public bool MatchesLiveCapturedGroup(AICF_TransportTrip trip){return Live;}public bool AllObservedDeadRemainKnown(){return Known;}
public bool BindCleanupLease(AICF_TransportTrip trip,AICF_FactionFleet fleet,Lease lease) BIND
public void RecordOrphanTerminalCommit(AICF_TransportTrip trip,AICF_FactionFleet fleet) CAUSE
}
public class AICF_TripOutcome {public static AICF_TripOutcome TerminalFailClosed(string reason,string cause){return new AICF_TripOutcome();}public static AICF_TripOutcome Wait(string reason,string cause){return new AICF_TripOutcome();}}
public class Owner {public string BuildCausationId(AICF_TransportTrip trip,string reason){return "cause";}public bool IsStaleTerminationLeaseCurrent(AICF_TransportTrip trip,AICF_FactionFleet fleet){return fleet.Current;}public void ReleaseSpawnPlan(AICF_TransportTrip trip,string reason){}public AICF_TripOutcome AdvanceLeaseCleanup(AICF_TransportTrip trip,AICF_FactionFleet fleet,string cause){return new AICF_TripOutcome();}public AICF_TripOutcome ContinueTerminalClearanceWithoutRestore(AICF_TransportTrip trip,AICF_FactionFleet fleet,string cause){return new AICF_TripOutcome();}public void AuditTerminalPostconditions(AICF_TransportTrip trip,object assignment,object slot){}public AICF_TripOutcome Prelude(AICF_TransportTrip trip,AICF_FactionFleet fleet,string reason) PRELUDE }
'@
$stale=Body $owner 'TerminateStaleTrip';$prelude=$stale.Substring(0,$stale.IndexOf('trip.RecordCapturedDeadRetirement('))
$prelude=$prelude.Replace('!trip)','trip == null)').Replace('!fleet ||','fleet == null ||').Replace('reason.IsEmpty()','string.IsNullOrEmpty(reason)')+'trip.Authorizations++; return new AICF_TripOutcome(); }'
$code=$code.Replace('BIND',$bind).Replace('CAUSE',$cause).Replace('PRELUDE',$prelude)
Add-Type -TypeDefinition ('namespace AICFRetirementCause { '+$code+' }')
foreach($case in @('same-bound','different-lease-pointer','lease-epoch-changed','vehicle-epoch-changed','car-id-changed','car-rpl-changed','different-fleet','missing-current-lease','current-fleet-veto','native-actor-unknown','no-captured-group-scope')){
 $proof=[AICFRetirementCause.Identity]::new();$ctx=[AICFRetirementCause.AICF_TransportTrip]::new();$ctx.Lease=$proof.m_CleanupLease;$fleet=$proof.m_CleanupFleet
 switch($case){'different-lease-pointer'{$ctx.Lease=[AICFRetirementCause.Lease]::new()}'lease-epoch-changed'{$ctx.Lease.Epoch=2}'vehicle-epoch-changed'{$ctx.Lease.VehicleEpoch=2}'car-id-changed'{$ctx.Lease.Id=101}'car-rpl-changed'{$ctx.Lease.Rpl='foreign'}'different-fleet'{$fleet=[AICFRetirementCause.AICF_FactionFleet]::new()}'missing-current-lease'{$ctx.Lease=$null}'current-fleet-veto'{$fleet.Current=$false}'native-actor-unknown'{$proof.Known=$false}'no-captured-group-scope'{$proof.Absent=$false;$proof.Live=$false}}
 $proof.RecordOrphanTerminalCommit($ctx,$fleet)
 if(($proof.m_iAuthorizedTerminalTransition -gt 0) -ne ($case -eq 'same-bound')){throw "Actual bound cause guard failed: $case"}
}
foreach($terminal in @($false,$true)){
 $ctx=[AICFRetirementCause.AICF_TransportTrip]::new();$ctx.Terminal=$terminal;$ctx.Lease=[AICFRetirementCause.Lease]::new()
 $ownerCase=[AICFRetirementCause.Owner]::new();[void]$ownerCase.Prelude($ctx,[AICFRetirementCause.AICF_FactionFleet]::new(),'arbitrary-reason')
 if($ctx.Authorizations -ne [int](!$terminal)){throw 'Earlier committed but not yet latched terminal could receive a new orphan cause authorization'}
}
'PASS Actual cause/bound-lease guard: lease-pointer/epoch/car/fleet/unknown negatives=10; earlier committed terminal exits before authorization; no reason whitelist'
