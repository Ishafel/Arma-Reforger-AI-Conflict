param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$flow=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleTransitFlow.c'))
$states=[IO.File]::ReadAllText((Join-Path $core 'State/Vehicles/AICF_VehiclePhaseStates.c'))
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleBehaviorProbe.c'))
function Body([string]$s,[string]$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules([string]$s,[string]$p){
 $tick=Body $s 'Tick';$needs=Body $s 'NeedsRouteQueueRecovery';$recover=Body $s 'RecoverLostNativeRoute'
 $arrival=$tick.IndexOf('state.GetRouteGeneration() > 0 && targetDistanceMeters <= m_Config.GetDismountDistanceMeters()')
 $lost=$tick.IndexOf('RecoverLostNativeRoute(trip, causationId)')
 if($arrival -lt 0 -or $lost -lt $arrival -or $tick.IndexOf('state.GetAssignmentRevision() != trip.GetAssignment().GetAssignmentRevision()') -gt $arrival -or $lost -gt $tick.IndexOf('if (!state.GetRouteWaypoint())')){'STAGED_CURRENT_ARRIVAL_THEN_LOST_THEN_FREE_PREPARE'}
 foreach($guard in @('behavior.InspectTransit(trip, causationId)','AdvancePendingNativeCrewEntries(trip, causationId, nowMs)','ObserveNativeCombat(trip, causationId, nowMs)','ObserveNativeGate(trip, causationId, nowMs)','ProcessMandatoryCrew(trip, causationId, nowMs)','trip.IsDeadlineReached(nowMs)','InspectRouteAssetFailure(trip)')){if($tick.IndexOf($guard) -lt 0 -or $tick.IndexOf($guard) -gt $lost){'EXISTING_OWNERS_HEALTH_AND_DEADLINE_FIRST';break}}
 if(!$needs.Contains('return cachedBound && routeGeneration > 0 && (!route || !queued.Contains(route));') -or ($needs+$recover).Contains('GetCurrentWaypoint')){'EXACT_ACTUAL_QUEUE_NOT_CURRENT_OR_CACHED_ONLY'}
 foreach($g in @('trip.GetAssignment().GetGroup().GetWaypoints(queued);','NeedsRouteQueueRecovery(state.IsRouteWaypointBound(), state.GetRouteGeneration(), state.GetRouteWaypoint(), queued)','if (!state.BeginMobilityRecovery())','NATIVE_ROUTE_WAYPOINT_LOST_RECOVERY_EXHAUSTED','return BeginMobilityRecovery(trip, causationId, reason, false);')){if(!$recover.Contains($g)){'EXISTING_BUDGET_AND_ROUTE_ONLY_REBUILD';break}}
 if((ConvertTo-AICFCodeText $recover) -match 'SetOrigin|Teleport|SetHealth|Reset\(|\.Begin\(|ConfirmRouteWaypointBound|AddWaypoint|RollbackUncommitted|m_i.*Deadline\s*=|m_i.*Attempts\s*='){'NO_FREE_REBIND_REARM_CLOCK_OR_PHYSICAL_MUTATION'}
 $init=Body $p 'AICF_VBInitialize';$begin=Body $p 'AICF_VBBeginNativeRouteLoss';$probe=Body $p 'AICF_VBNativeRouteLossUpdate';$update=Body $p 'Update'
 if(!$init.Contains('System.GetCLIParam("aicfNativeRouteLossProbe", routeLossFlag) && routeLossFlag == "1"') -or !$init.Contains('m_sVBScenario != "combat" || m_bVBReboardPilotLossProbe') -or !$begin.Contains('state.GetMobilityRecoveryAttempts() == 0 && state.GetMaximumMobilityRecoveryAttempts() == 2')){'ISOLATED_REAL_ZERO_BUDGET_PRECONDITION'}
 foreach($g in @('trip != m_VBTrip','trip.GetTripGeneration() != m_iVBRouteLossTripGeneration','trip.GetAbsoluteDeadlineMs() != m_iVBRouteLossDeadline','trip.GetLease() != m_VBRouteLossLease','trip.GetLease().GetLeaseGeneration() != m_iVBLeaseGeneration','trip.GetLease().GetVehicleGeneration() != m_iVBRouteLossVehicleGeneration','latency < 30000','m_iVBRouteLossRemoved == 3','NATIVE_ROUTE_WAYPOINT_LOST_RECOVERY_EXHAUSTED','actual.Contains(route)','route.GetID() != m_VBRouteLossWaypointId','state.GetRouteGeneration() == m_iVBRouteLossGeneration + 1','state.GetMobilityRecoveryAttempts() == m_iVBRouteLossRemoved','!state.WasPendingUnstuckRelocated()','m_VBGroup.RemoveWaypoint(expected);','state.GetRouteWaypoint() == expected && state.IsRouteWaypointBound()','NATIVE_ROUTE_LOSS_REMOVED_','NATIVE_ROUTE_LOSS_REBUILD_','NATIVE_ROUTE_LOSS_EXHAUSTED')){if(!$probe.Contains($g)){'CONTROLLED_EXTERNAL_REMOVAL_REAL_QUEUE_REBUILD_AND_EXHAUSTION';break}}
 if($probe -match 'BeginMobilityRecovery\(|ConfirmRouteWaypointBound\(|StageRouteWaypoint\(|DeleteRplEntity\(|SetHealth|Teleport|m_iMobilityRecoveryAttempts\s*=|m_iAbsoluteDeadlineMs\s*='){'PROBE_DOES_NOT_PRIME_OR_REPAIR_PRODUCTION_STATE'}
 $receipt=Body $p 'AICF_VBRouteLossExhaustionReceipt'
 foreach($g in @('removals == 3 && rebuilds == 2 && beforeExit == 2 && afterExit == 0','expectedFallback && latencyMs >= 0 && latencyMs < 30000')){if(!$receipt.Contains($g)){'BUDGET_PROVEN_BEFORE_AND_NORMAL_RESET_AFTER_EXIT';break}}
 if(!$p.Contains('protected int m_iVBRouteLossAttemptsBeforeExit = -1;') -or
  @([regex]::Matches($p,'m_iVBRouteLossAttemptsBeforeExit\s*=(?!=)')).Count -ne 2 -or
  !$probe.Contains('if (m_iVBRouteLossAttemptsBeforeExit != -1)') -or
  !$probe.Contains('m_iVBRouteLossAttemptsBeforeExit = state.GetMobilityRecoveryAttempts();') -or
  $probe.IndexOf('m_iVBRouteLossAttemptsBeforeExit = state.GetMobilityRecoveryAttempts();') -lt $probe.IndexOf('m_VBGroup.RemoveWaypoint(expected);') -or
  $probe.IndexOf('m_iVBRouteLossAttemptsBeforeExit = state.GetMobilityRecoveryAttempts();') -gt $probe.IndexOf('AICF_VBCheck("NATIVE_ROUTE_LOSS_REMOVED_') -or
  !$probe.Contains('m_iVBRouteLossRemoved != 3 || (m_iVBRouteLossAttemptsBeforeExit == 2 && m_iVBRouteLossRebuilt == 2)') -or
  $probe -notmatch 'm_iVBRouteLossAttemptsBeforeExit, state.GetMobilityRecoveryAttempts\(\), expectedFallback, latency' -or
  !$probe.Contains('state.GetRouteWaypoint() == expected && state.IsRouteWaypointBound() && budgetReceipt')){'ONCE_ONLY_THIRD_REMOVAL_RECEIPT_BEFORE_CONTROLLER_EXIT'}
 if(!$update.Contains('if (m_bVBNativeRouteLossActive) { AICF_VBNativeRouteLossUpdate(trip, now); return; }') -or $update.IndexOf('if (m_bVBNativeRouteLossActive)') -gt $update.IndexOf('if (trip.IsTerminal())') -or $update.IndexOf('AICF_VBBeginNativeRouteLoss(trip, now)') -lt $update.IndexOf('m_iVBStep == 6 && vector.DistanceXZ(m_vVBMovementStart, m_VBVehicle.GetOrigin()) >= 10')){'EXPLICIT_OPTIONAL_TERMINAL_ORACLE_AFTER_REAL_RESUME'}
}
$issues=@(Rules $flow $fixture);if($issues.Count){throw "Native route loss: $issues"};$checks=1
foreach($m in @(
 @('f','cachedBound && routeGeneration > 0 && (!route || !queued.Contains(route))','cachedBound','EXACT_ACTUAL_QUEUE_NOT_CURRENT_OR_CACHED_ONLY'),
 @('f','state.GetRouteGeneration() > 0 && targetDistanceMeters <= m_Config.GetDismountDistanceMeters()','targetDistanceMeters <= m_Config.GetDismountDistanceMeters()','STAGED_CURRENT_ARRIVAL_THEN_LOST_THEN_FREE_PREPARE'),
 @('f','if (!state.BeginMobilityRecovery())','if (false)','EXISTING_BUDGET_AND_ROUTE_ONLY_REBUILD'),
 @('f','return BeginMobilityRecovery(trip, causationId, reason, false);','return BeginMobilityRecovery(trip, causationId, reason, true);','EXISTING_BUDGET_AND_ROUTE_ONLY_REBUILD'),
 @('p','actual.Contains(route)','true','CONTROLLED_EXTERNAL_REMOVAL_REAL_QUEUE_REBUILD_AND_EXHAUSTION'),
 @('p','state.GetRouteGeneration() == m_iVBRouteLossGeneration + 1','true','CONTROLLED_EXTERNAL_REMOVAL_REAL_QUEUE_REBUILD_AND_EXHAUSTION'),
 @('p','latency < 30000','true','CONTROLLED_EXTERNAL_REMOVAL_REAL_QUEUE_REBUILD_AND_EXHAUSTION'),
 @('p','m_VBGroup.RemoveWaypoint(expected);','','CONTROLLED_EXTERNAL_REMOVAL_REAL_QUEUE_REBUILD_AND_EXHAUSTION')
 ,@('p','beforeExit == 2','true','BUDGET_PROVEN_BEFORE_AND_NORMAL_RESET_AFTER_EXIT')
 ,@('p','afterExit == 0','afterExit == 2','BUDGET_PROVEN_BEFORE_AND_NORMAL_RESET_AFTER_EXIT')
 ,@('p','m_iVBRouteLossAttemptsBeforeExit = state.GetMobilityRecoveryAttempts();','m_iVBRouteLossAttemptsBeforeExit = 2;','ONCE_ONLY_THIRD_REMOVAL_RECEIPT_BEFORE_CONTROLLER_EXIT')
 ,@('p','if (m_iVBRouteLossAttemptsBeforeExit != -1)','if (false)','ONCE_ONLY_THIRD_REMOVAL_RECEIPT_BEFORE_CONTROLLER_EXIT')
 ,@('p','m_iVBRouteLossRemoved != 3 || (m_iVBRouteLossAttemptsBeforeExit == 2 && m_iVBRouteLossRebuilt == 2)','true','ONCE_ONLY_THIRD_REMOVAL_RECEIPT_BEFORE_CONTROLLER_EXIT')
 ,@('p','m_iVBRouteLossAttemptsBeforeExit, state.GetMobilityRecoveryAttempts(), expectedFallback, latency','m_iVBRouteLossAttemptsBeforeExit, m_iVBRouteLossAttemptsBeforeExit, expectedFallback, latency','ONCE_ONLY_THIRD_REMOVAL_RECEIPT_BEFORE_CONTROLLER_EXIT')
)){$f=$flow;$p=$fixture;if($m[0] -eq 'f'){$f=$f.Replace($m[1],$m[2])}else{$p=$p.Replace($m[1],$m[2])};if(($f -ceq $flow -and $p -ceq $fixture) -or $m[3] -notin @(Rules $f $p)){throw "Route loss mutation survived: $($m[3])"};$checks++}
# Actual production queue predicate, bounded dispatch и existing budget body.
$needs=(Body $flow 'NeedsRouteQueueRecovery').Replace('!route ||','route == null ||')
$recover=(Body $flow 'RecoverLostNativeRoute').Replace('array<AIWaypoint> queued = {};','List<AIWaypoint> queued = new List<AIWaypoint>();').Replace('queued.Count()','queued.Count')
$budget=Body $states 'BeginMobilityRecovery'
$arrival=[regex]::Match((Body $flow 'Tick'),'state.GetRouteGeneration\(\) > 0 && targetDistanceMeters <= m_Config.GetDismountDistanceMeters\(\)').Value
$code=@'
using System.Collections.Generic;
public class AIWaypoint { public int Id; }
public class AICF_Group { public List<AIWaypoint> Queue=new List<AIWaypoint>();public AIWaypoint Current;public void GetWaypoints(List<AIWaypoint> q){q.AddRange(Queue);} }
public class AICF_Assignment {public AICF_Group Group=new AICF_Group();public AICF_Group GetGroup(){return Group;}}
public class AICF_VehicleMovementState {public int m_iMobilityRecoveryAttempts,m_iMaximumMobilityRecoveryAttempts=2,Generation=1;public bool Bound=true,Pending;public AIWaypoint Route=new AIWaypoint{Id=1};public bool IsRouteWaypointBound(){return Bound;}public int GetRouteGeneration(){return Generation;}public AIWaypoint GetRouteWaypoint(){return Route;}public int GetMobilityRecoveryAttempts(){return m_iMobilityRecoveryAttempts;}public int GetMaximumMobilityRecoveryAttempts(){return m_iMaximumMobilityRecoveryAttempts;}public bool IsRecoveryEvidencePending(){return Pending;}public bool BeginMobilityRecovery() BUDGET }
public class AICF_TransportTrip {public AICF_VehicleMovementState State=new AICF_VehicleMovementState();public AICF_Assignment Assignment=new AICF_Assignment();public AICF_VehicleMovementState GetMovementState(){return State;}public AICF_Assignment GetAssignment(){return Assignment;}}
public class AICF_TripOutcome{public string Kind,Reason;}
public class AICF_GroupTrace{public static string Entity(AIWaypoint w){return w==null?"NONE":w.Id.ToString();}}
public class AICF_Stage3Diagnostics{public static void Warning(string e,string d){}}
public class AICF_Config{public int GetDismountDistanceMeters(){return 150;}}
public class AICF_RouteLossProduction {
 public int Calls;public bool Stationary;public static AICF_Config m_Config=new AICF_Config();
 public static bool NeedsRouteQueueRecovery(bool cachedBound,int routeGeneration,AIWaypoint route,List<AIWaypoint> queued) NEEDS
 public static bool Arrived(AICF_VehicleMovementState state,float targetDistanceMeters){return ARRIVAL;}
 public static string FormatIdentity(AICF_TransportTrip t,string reason){return reason;}
 public static void ReportPendingRecoveryFailure(AICF_TransportTrip t,string r,bool final){}
 public static AICF_TripOutcome FailRecovery(AICF_TransportTrip t,string r,string k,string c){return new AICF_TripOutcome{Kind="FALLBACK",Reason=r};}
 public AICF_TripOutcome BeginMobilityRecovery(AICF_TransportTrip t,string c,string r,bool stationary){Calls++;Stationary=stationary;return new AICF_TripOutcome{Kind="REBUILD",Reason=r};}
 public AICF_TripOutcome RecoverLostNativeRoute(AICF_TransportTrip trip,string causationId) RECOVER
}
'@
Add-Type -TypeDefinition ($code.Replace(' BUDGET',' '+$budget).Replace(' NEEDS',' '+$needs).Replace('ARRIVAL',$arrival).Replace(' RECOVER',' '+$recover))
$t=New-Object AICF_TransportTrip;$sut=New-Object AICF_RouteLossProduction
$t.Assignment.Group.Queue.Add($t.State.Route);$t.Assignment.Group.Current=$null
if($sut.RecoverLostNativeRoute($t,'queued-but-not-current') -or $t.State.m_iMobilityRecoveryAttempts -ne 0){throw 'Queued route falsely failed due to different current WP'};$checks++
$t.Assignment.Group.Queue.Clear()
foreach($attempt in 1..2){$r=$sut.RecoverLostNativeRoute($t,'missing');if($r.Kind -ne 'REBUILD' -or $r.Reason -ne 'NATIVE_ROUTE_WAYPOINT_LOST' -or $t.State.m_iMobilityRecoveryAttempts -ne $attempt -or $sut.Stationary){throw 'Production lost-route dispatch/budget failed'};$checks++}
$r=$sut.RecoverLostNativeRoute($t,'third-loss');if($r.Kind -ne 'FALLBACK' -or $r.Reason -ne 'NATIVE_ROUTE_WAYPOINT_LOST_RECOVERY_EXHAUSTED' -or $t.State.m_iMobilityRecoveryAttempts -ne 2 -or $sut.Calls -ne 2){throw 'Production failed to bound third loss'};$checks++
$t=New-Object AICF_TransportTrip;$t.State.Route=$null;$r=$sut.RecoverLostNativeRoute($t,'weak-deleted');if($r.Kind -ne 'REBUILD' -or $t.State.m_iMobilityRecoveryAttempts -ne 1){throw 'Null weak route gave free rebind'};$checks++
$t.State.Bound=$false;if($sut.RecoverLostNativeRoute($t,'not-yet-bound')){throw 'Uncommitted route falsely recovered'};$checks++
$t.State.Bound=$true;$t.State.Generation=0;if($sut.RecoverLostNativeRoute($t,'never-staged')){throw 'Unstaged target falsely recovered'};$checks++
foreach($c in @(@(0,0,$false),@(1,0,$true),@(1,150,$true),@(1,151,$false))){$t.State.Generation=$c[0];if([AICF_RouteLossProduction]::Arrived($t.State,$c[1]) -ne $c[2]){throw 'Production arrival boundary failed'};$checks++}
$receipt=Body $fixture 'AICF_VBRouteLossExhaustionReceipt'
Add-Type -TypeDefinition ('public class AICF_ExhaustionReceiptProduction { public static bool Check(int removals,int rebuilds,int beforeExit,int afterExit,bool expectedFallback,int latencyMs) '+$receipt+' }')
foreach($c in @(
 @(3,2,2,0,$true,0,$true),@(3,2,2,0,$true,29999,$true),
 @(3,2,2,2,$true,1001,$false),@(3,2,0,0,$true,1001,$false),
 @(3,1,2,0,$true,1001,$false),@(2,2,2,0,$true,1001,$false),
 @(3,2,2,0,$false,1001,$false),@(3,2,2,0,$true,30000,$false),
 @(3,2,2,0,$true,-1,$false),@(3,2,-1,0,$true,1001,$false),@(3,2,2,1,$true,1001,$false)
)){
 if([AICF_ExhaustionReceiptProduction]::Check($c[0],$c[1],$c[2],$c[3],$c[4],$c[5]) -ne $c[6]){throw 'Actual fixture before/after exhaustion receipt failed'};$checks++
}
"PASS Native route loss controls=$checks actual_queue_dispatch_budget_arrival=EXECUTED native_external_removal=NOT_RUN"
