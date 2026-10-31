param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$source=[IO.File]::ReadAllText((Join-Path $core 'State/Vehicles/AICF_TransitMoveFailure.c'))
$flow=[IO.File]::ReadAllText((Join-Path $core 'Vehicles/AICF_VehicleTransitFlow.c'))
$node=[IO.File]::ReadAllText((Join-Path $core 'Forces/AICF_AINodeLifecycle.c'))
$phase=[IO.File]::ReadAllText((Join-Path $core 'State/Vehicles/AICF_VehiclePhaseStates.c'))
$state=[regex]::Match($phase,'(?s)class AICF_VehicleMovementState\b.*?(?=\r?\nclass |\z)').Value
function Body($text,$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules($receipt,$transit,$native,$movement){
 $capture=Body $receipt 'CaptureWithReason';$current=Body $receipt 'Matches';$recover=Body $transit 'RecoverNativeMoveFailure';$hook=Body $native 'AICF_HandleTransitFailedMovement';$tick=Body $transit 'Tick';$abort=Body $transit 'AbortOwnedActions'
 if(!$capture -or !$current -or !$recover -or !$hook){'METHODS';return}
 foreach($guard in @('!Replication.IsServer()','!Supports(result) || !related','handler < 0','handler == AIGroupMovementComponent.DEFAULT_HANDLER_ID',
  '!AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(trip, utility, handler)',
  'state.GetMobilityRecoveryAttempts() >= state.GetMaximumMobilityRecoveryAttempts()',
  'utility.m_VehicleMgr.FindVehicleBySubgroupId(handler)','nativeVehicle.GetEntity() != trip.GetLease().GetVehicle()',
  'move.m_Utility != utility','move.GetRemoveAction()','move.GetActionIsSuspended()',
  'utility.GetExecutedAction() != move','move.m_RelatedWaypoint != state.GetRouteWaypoint()',
  'move.GetActionState() != EAIActionState.RUNNING','receipt.m_sVehicleRpl = vehicleRpl.Id().ToString();','if (!receipt.Matches(trip))')){
  if(!$capture.Contains($guard)){'CAPTURE_CURRENT_NATIVE_SOURCE';break}
 }
 foreach($guard in @('trip != m_Trip','!trip.IsValid()','trip.IsTerminal()',
  'trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','trip.IsDeadlineReached(System.GetTickCount())',
  '!trip.IsVehiclePresetCurrent()','trip.GetAssignment() != m_Assignment','trip.GetLease() != m_Lease',
  '!m_Lease.HasPhysicalAsset()','m_Group.GetID() != m_GroupId',
  '!m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)',
  'vehicleRpl.Id().ToString() != m_sVehicleRpl',
  'm_Lease.GetLeaseGeneration() != m_iLeaseGeneration','m_Lease.GetVehicleGeneration() != m_iVehicleGeneration',
  '!m_Lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())',
  'slot.GetStrategicAssignmentRevision() != m_iAssignmentRevision','slot.GetStrategicIntentRevision() != m_iIntentRevision',
  'm_Assignment.GetTargetPosition() != slot.GetTargetPosition()','m_Assignment.GetTargetBase() != slot.GetTargetBase()',
  'state.GetRouteGeneration() != m_iRouteGeneration','state.GetRouteWaypoint() != m_Waypoint',
  'state.GetMobilityRecoveryAttempts() >= state.GetMaximumMobilityRecoveryAttempts()',
  'state.GetCrewRecoveryToken()','state.GetNativeGate()','state.IsCrewRecoveryRoutePending()',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group)',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle)',
  'm_Utility.GetCurrentAction() != m_Action','m_Utility.GetExecutedAction() != m_Action',
  'm_Group.GetCurrentWaypoint() != m_Waypoint','!queue.Contains(m_Waypoint)',
  '!SCR_AIVehicleUsability.VehicleCanMove(m_Vehicle)','damage.GetMovementDamage() >= 1',
  '!watchdog.AreAllAliveMembersSettledInVehicle(m_Group, m_Vehicle)',
  'agents.Count() != m_aMembers.Count()','member.GetID() != m_aMemberIds[index]',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)',
  '!AICF_GroupRuntime.IsAliveCharacter(member)','occupant && m_aMembers.Find(occupant) < 0')){
  if(!$current.Contains($guard)){'CURRENT_IDENTITY_HEALTH_AND_ROSTER';break}
 }
 if(@([regex]::Matches((ConvertTo-AICFCodeText $hook),'\.OnMoveFailed\(')).Count -ne 1 -or
  !$hook.Contains('if (receipt.WasDelivered()) { m_AICFTransitFailure = receipt; return true; }') -or !$hook.Contains('if (!receipt.ConfirmDelivery()) return false;') -or
  $hook.IndexOf('receipt.ConfirmDelivery()') -gt $hook.IndexOf('.OnMoveFailed(') -or
  !$hook.Contains('if (!nativeVehicle || !receipt.OwnsVehicle(nativeVehicle.GetEntity())) return false;') -or
  $hook.IndexOf('receipt.OwnsVehicle(') -gt $hook.IndexOf('receipt.ConfirmDelivery()') -or
  (ConvertTo-AICFCodeText $hook) -match '\b(?:SetOrigin|SetVelocity|AddAction|Fail|Complete|CommitTransition|PrepareRoute|BeginMobilityRecovery|CallLater)\s*\('){'CALLBACK_ONCE_RECEIPT_ONLY'}
 if(!$native.Contains('hasHandler && hasRelated && hasLocation && AICF_HandleTransitFailedMovement(result, handler, related, location)') -or
  $native -notmatch '(?s)AICF_HandleTransitFailedMovement\(result, handler, related, location\)\)\s*return ENodeResult.FAIL;'){'EXACT_PORTS_AND_BOUNDED_HANDOFF'}
 if(!$recover.Contains('failure.WasDelivered() && failure.Matches(trip)') -or !$recover.Contains('state.SetTransitMoveFailure(null);') -or
  !$recover.Contains('if (!state.BeginMobilityRecovery())') -or !$recover.Contains('ReportPendingRecoveryFailure(trip, failure.Reason(), false)') -or
  !$recover.Contains('BeginMobilityRecovery(trip, causationId, failure.Reason(), failure.NeedsPhysicalCorrection())') -or
  $recover.IndexOf('state.BeginMobilityRecovery()') -gt $recover.IndexOf('return BeginMobilityRecovery(')){'EXISTING_OWNER_AND_COUNTED_BUDGET'}
 if($tick.IndexOf('trip.IsDeadlineReached(') -gt $tick.IndexOf('RecoverNativeMoveFailure(') -or
  $tick.IndexOf('RecoverNativeMoveFailure(') -gt $tick.IndexOf('AICF_TransportNativeAbandon.Observe(') -or
  !$abort.Contains('state.SetTransitMoveFailure(null);') -or !(Body $movement 'Reset').Contains('m_TransitMoveFailure = null;')){'HEALTH_DEADLINE_AND_CLEANUP_BOUNDARIES'}
 if((ConvertTo-AICFCodeText $receipt) -match '\b(?:SetOrigin|SetVelocity|AddAction|Fail|Complete|CommitTransition|PrepareRoute|BeginMobilityRecovery|CallLater|Heal|SetHealth)\s*\(' -or
  (ConvertTo-AICFCodeText $recover) -match '\b(?:Reset|Begin|CreditNativeRouteProgress|ObservePhysicalMotion|ObserveRouteProgress|CommitTransition|SetOrigin|Heal|SetHealth)\s*\('){'NO_CLOCK_OR_PHYSICAL_BYPASS'}
 $physical=Body $receipt 'NeedsPhysicalCorrection'
 if(!$physical.Contains('m_iResult != EMoveError.STUCK') -or !$physical.Contains('m_fCapturedSpeed > 1') -or
  !$physical.Contains('m_Vehicle.GetID() != m_VehicleId') -or !$physical.Contains('physics.GetVelocity().Length() <= 1')){'SLOW_STUCK_ONLY_PHYSICAL_CORRECTION'}
 $retired=Body $receipt 'CanDiscardRetiredNativeFailure'
 foreach($guard in @('age < 0 || age > 5000','result != m_iResult || handler != m_iHandler',
  'vector.Distance(location, m_vFailureLocation) > 0.1','utility.GetCurrentAction() != m_Action','utility.GetExecutedAction() != m_Action',
  'm_Action.GetActionState() != EAIActionState.FAILED && !m_Action.GetRemoveAction()',
  'state.GetRouteGeneration() <= m_iRouteGeneration','state.GetRouteWaypoint() == m_Waypoint',
  'state.GetSupersededRouteWaypoint() != m_Waypoint && queue.Contains(m_Waypoint)')){
  if(!$retired.Contains($guard)){'ONLY_BOUNDED_RETIRED_SOURCE_DEDUP';break}
 }
 $nativeMove=[regex]::Match($native,'(?s)modded class SCR_AIProcessFailedMovementResult\b.*?(?=\r?\n(?:modded )?class |\z)').Value
 if(!(Body $nativeMove 'OnEnter').Contains('m_AICFTransitFailure = null;') -or
  !$hook.Contains('m_AICFTransitFailure.CanDiscardRetiredNativeFailure(')){'NODE_CYCLE_DEDUP_RESET'}
}
$issues=@(Rules $source $flow $node $state);if($issues.Count){throw "Transit move failure contracts: $issues"}
$controls=1
foreach($mutation in @(
 @('r','!Supports(result) || !related','false','CAPTURE_CURRENT_NATIVE_SOURCE'),
 @('r','handler == AIGroupMovementComponent.DEFAULT_HANDLER_ID','false','CAPTURE_CURRENT_NATIVE_SOURCE'),
 @('r','handler < 0','false','CAPTURE_CURRENT_NATIVE_SOURCE'),
 @('r','!AICF_VehicleNativeSubgroup.IsCurrentVehicleHandler(trip, utility, handler)','false','CAPTURE_CURRENT_NATIVE_SOURCE'),
 @('r','nativeVehicle.GetEntity() != trip.GetLease().GetVehicle()','false','CAPTURE_CURRENT_NATIVE_SOURCE'),
 @('r','move.m_Utility != utility','false','CAPTURE_CURRENT_NATIVE_SOURCE'),
 @('r','trip.IsDeadlineReached(System.GetTickCount())','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','!trip.IsVehiclePresetCurrent()','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','m_Lease.GetLeaseGeneration() != m_iLeaseGeneration','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','receipt.m_sVehicleRpl = vehicleRpl.Id().ToString();','receipt.m_sVehicleRpl = Replication.FindItemId(receipt.m_Vehicle).ToString();','CAPTURE_CURRENT_NATIVE_SOURCE'),
 @('r','slot.GetStrategicIntentRevision() != m_iIntentRevision','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','state.GetRouteGeneration() != m_iRouteGeneration','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','!SCR_AIVehicleUsability.VehicleCanMove(m_Vehicle)','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','damage.GetMovementDamage() >= 1','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','occupant && m_aMembers.Find(occupant) < 0','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','state.GetMobilityRecoveryAttempts() >= state.GetMaximumMobilityRecoveryAttempts()','false','CURRENT_IDENTITY_HEALTH_AND_ROSTER'),
 @('r','m_fCapturedSpeed > 1','false','SLOW_STUCK_ONLY_PHYSICAL_CORRECTION'),
 @('r','age < 0 || age > 5000','false','ONLY_BOUNDED_RETIRED_SOURCE_DEDUP'),
 @('r','utility.GetCurrentAction() != m_Action','false','ONLY_BOUNDED_RETIRED_SOURCE_DEDUP'),
 @('r','state.GetRouteGeneration() <= m_iRouteGeneration','false','ONLY_BOUNDED_RETIRED_SOURCE_DEDUP'),
 @('r','m_Action.GetActionState() != EAIActionState.FAILED && !m_Action.GetRemoveAction()','false','ONLY_BOUNDED_RETIRED_SOURCE_DEDUP'),
 @('n','if (receipt.WasDelivered()) { m_AICFTransitFailure = receipt; return true; }',';','CALLBACK_ONCE_RECEIPT_ONLY'),
 @('n','if (!receipt.ConfirmDelivery()) return false;',';','CALLBACK_ONCE_RECEIPT_ONLY'),
 @('n','if (!nativeVehicle || !receipt.OwnsVehicle(nativeVehicle.GetEntity())) return false;',';','CALLBACK_ONCE_RECEIPT_ONLY'),
 @('n','m_AICFTransitFailure = null;',';','NODE_CYCLE_DEDUP_RESET'),
 @('f','if (!state.BeginMobilityRecovery())','if (false)','EXISTING_OWNER_AND_COUNTED_BUDGET'),
 @('f','failure.WasDelivered() && failure.Matches(trip)','true','EXISTING_OWNER_AND_COUNTED_BUDGET'),
 @('f','state.SetTransitMoveFailure(null);',';','HEALTH_DEADLINE_AND_CLEANUP_BOUNDARIES'),
 @('s','m_TransitMoveFailure = null;',';','HEALTH_DEADLINE_AND_CLEANUP_BOUNDARIES')
)){
 $r=$source;$f=$flow;$n=$node;$s=$state
 if($mutation[0] -eq 'r'){$r=$r.Replace($mutation[1],$mutation[2])}
 elseif($mutation[0] -eq 'f'){$f=$f.Replace($mutation[1],$mutation[2])}
 elseif($mutation[0] -eq 'n'){$n=$n.Replace($mutation[1],$mutation[2])}
 else{$s=$s.Replace($mutation[1],$mutation[2])}
 if($r -ceq $source -and $f -ceq $flow -and $n -ceq $node -and $s -ceq $state){throw "Missing mutation target: $($mutation[1])"}
 if($mutation[3] -notin @(Rules $r $f $n $s)){throw "Transit mutation survived: $($mutation[3])"};$controls++
}
Write-Output "PASS TransitMoveFailureContracts controls=$controls"
