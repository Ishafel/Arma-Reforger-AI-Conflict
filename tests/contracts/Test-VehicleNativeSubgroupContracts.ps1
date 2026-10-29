param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Read([string]$path){[IO.File]::ReadAllText((Join-Path $core $path))}
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$source=Read 'Vehicles/AICF_VehicleNativeSubgroup.c'
$owner=Read 'Vehicles/AICF_TransportTripController.c'
$handoff=Read 'Vehicles/AICF_VehicleTaskHandoff.c'
function Rules([string]$scope,[string]$controller,[string]$boundary){
 $read=Body $scope 'Read';$reconcile=Body $scope 'AICF_ReconcileExactVehicleSubgroup';$handler=Body $scope 'IsCurrentVehicleHandler'
 foreach($guard in @('!Replication.IsServer()','!trip.IsVehiclePresetCurrent()','trip.IsDeadlineReached(System.GetTickCount())',
  'slot.GetSpawnGeneration() != trip.GetGroupGeneration()','slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision()',
  'slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()','assignment.GetTargetPosition() != slot.GetTargetPosition()',
  '!lease.MatchesTripIdentity(trip.GetFactionKey(), trip.GetSlotId(), trip.GetGroupGeneration(), trip.GetTripGeneration())',
  '!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())','utility.m_Owner != group',
  'state.GetNativeGate()','state.GetCrewRecoveryToken()','state.GetNativeCrewEntry(EAICompartmentType.Pilot)',
  '!CapturedMember(trip, actor)','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor)',
  'current.GetActionState() == EAIActionState.RUNNING','SCR_AIGetInVehicle.Cast(current)','SCR_AIGetOutVehicle.Cast(current)',
  '!CapturedMember(trip, occupant)','!Settled(occupant, vehicle)','registered.GetDriver() != pilot',
  'pilotAgent.GetParentGroup() != group','car.GetAIAgent().GetControlledEntity() != vehicle')){
  if(!$read.Contains($guard)){'EXACT_CURRENT_SCOPE';break}
 }
 if(!$handler.Contains('handler < 0 || handler == AIGroupMovementComponent.DEFAULT_HANDLER_ID') -or
  !$handler.Contains('registered.GetSubgroupHandleId() == handler && movement.GetAgentMoveHandlerId(pilot) == handler')){'REAL_POSITIVE_PILOT_HANDLER'}
 if(!$reconcile.Contains('if (!pilot) return true;') -or !$read.Contains('if (!pilot || !AICF_GroupRuntime.IsAliveCharacter(pilot) || !SCR_AIDamageHandling.IsConscious(pilot)) return true;')){'COMBAT_NO_CREW_REPLACEMENT'}
 if(!$reconcile.Contains('if (!AICF_VehicleNativeSubgroup.CanReconcileHandle(before, pilotBefore))') -or
  !$reconcile.Contains('bool repair = before < 0;') -or $reconcile.IndexOf('CanReconcileHandle(') -gt $reconcile.IndexOf('OnJoinGroupFromVehicle(')){'STALE_POSITIVE_HANDLE_FAIL_CLOSED'}
 if(!$reconcile.Contains('if (repair && !OnJoinGroupFromVehicle(pilot, true)) return false;') -or
  !$reconcile.Contains('if (!OnJoinGroupFromVehicle(agent, false) || movement.GetAgentMoveHandlerId(agent) != handle) return false;') -or
  @([regex]::Matches($reconcile,'AICF_VehicleNativeSubgroup.Read\(')).Count -ne 4 -or
  @([regex]::Matches($reconcile,'repeatedRecord != registered \|\| repeatedPilot != pilot \|\| repeatedMovement != movement')).Count -ne 3 -or
  !$reconcile.Contains('!AICF_VehicleNativeSubgroup.Settled(actor, vehicle)')){'REPEATED_STOCK_RECONCILIATION'}
 if((ConvertTo-AICFCodeText $scope) -match '\b(?:SetOrigin|SetVelocity|GetInVehicle|GetOutVehicle|SetHealth|Heal|CreateNewSubgroup|AddAction|RemoveAction|CallLater|PrepareRoute|BeginMobilityRecovery)\s*\('){'NO_PHYSICAL_ACTION_OR_CLOCK_MUTATION'}
 $exit=Body $controller 'ExitPhaseEffects'
 if((ConvertTo-AICFCodeText $exit) -notmatch 'case AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT:\s*m_BehaviorFlow.Stop\(trip\);\s*if \(nextPhase != AICF_ETransportTripPhase.TRANSIT\)\s*m_Handoff.DetachVehicleUtility\(trip\);' -or
  !(Body $controller 'TransitionTo').Contains('ExitPhaseEffects(trip, previousPhase, nextPhase, reason)')){'PRESERVE_RESUMED_NATIVE_RECORD'}
 $attach=Body $boundary 'AttachVehicleUtility'
 if(!$attach.Contains('groupUtility.AICF_ReconcileExactVehicleSubgroup(trip)') -or
  $attach.IndexOf('groupUtility.AddUsableVehicle(') -gt $attach.IndexOf('groupUtility.AICF_ReconcileExactVehicleSubgroup(trip)')){'REGISTER_BEFORE_RECONCILE'}
}
$issues=@(Rules $source $owner $handoff);if($issues.Count){throw "Vehicle native subgroup contracts: $issues"}
$predicate=(Body $source 'CanReconcileHandle').Trim()
if($predicate -notmatch '^\{\s*return (?<expression>[^;]+);\s*\}$'){throw 'Handle predicate must remain a pure return expression'}
$expression=$Matches.expression.Replace('AIGroupMovementComponent.DEFAULT_HANDLER_ID','0').Replace('recordHandle','$recordHandle').Replace('pilotHandle','$pilotHandle')
if($expression -match '[^\w\s$()!<>=&|0-9]'){throw 'Unsupported handle predicate token'}
$expression=$expression.Replace('!=',' -ne ').Replace('==',' -eq ').Replace('<',' -lt ').Replace('||',' -or ').Replace('&&',' -and ')
$evaluate=[scriptblock]::Create('param($recordHandle,$pilotHandle) '+$expression)
$cases=@(@(-1,1,$true),@(-2,1,$true),@(1,1,$true),@(2,2,$true),@(0,0,$false),@(1,0,$false),@(2,1,$false),@(1,2,$false),@(1,-1,$false))
foreach($case in $cases){if((& $evaluate $case[0] $case[1]) -ne $case[2]){throw "Actual handle predicate rejected control: $case"}}
$controls=1
foreach($mutation in @(
 @('s','!trip.IsVehiclePresetCurrent()','false','EXACT_CURRENT_SCOPE'),
 @('s','slot.GetSpawnGeneration() != trip.GetGroupGeneration()','false','EXACT_CURRENT_SCOPE'),
 @('s','slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision()','false','EXACT_CURRENT_SCOPE'),
 @('s','!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())','false','EXACT_CURRENT_SCOPE'),
 @('s','state.GetNativeGate()','false','EXACT_CURRENT_SCOPE'),
 @('s','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor)','false','EXACT_CURRENT_SCOPE'),
 @('s','!Settled(occupant, vehicle)','false','EXACT_CURRENT_SCOPE'),
 @('s','car.GetAIAgent().GetControlledEntity() != vehicle','false','EXACT_CURRENT_SCOPE'),
 @('s','handler < 0 || handler == AIGroupMovementComponent.DEFAULT_HANDLER_ID','false','REAL_POSITIVE_PILOT_HANDLER'),
 @('s','movement.GetAgentMoveHandlerId(pilot) == handler','true','REAL_POSITIVE_PILOT_HANDLER'),
 @('s','if (!pilot) return true;','if (!pilot) return false;','COMBAT_NO_CREW_REPLACEMENT'),
 @('s','if (!AICF_VehicleNativeSubgroup.CanReconcileHandle(before, pilotBefore))','if (false)','STALE_POSITIVE_HANDLE_FAIL_CLOSED'),
 @('s','bool repair = before < 0;','bool repair = before != pilotBefore;','STALE_POSITIVE_HANDLE_FAIL_CLOSED'),
 @('s','OnJoinGroupFromVehicle(pilot, true)','OnJoinGroupFromVehicle(pilot, false)','REPEATED_STOCK_RECONCILIATION'),
 @('s','repeatedRecord != registered || repeatedPilot != pilot || repeatedMovement != movement','false','REPEATED_STOCK_RECONCILIATION'),
 @('o','if (nextPhase != AICF_ETransportTripPhase.TRANSIT)','if (true)','PRESERVE_RESUMED_NATIVE_RECORD'),
 @('h','groupUtility.AICF_ReconcileExactVehicleSubgroup(trip)','true','REGISTER_BEFORE_RECONCILE')
)){
 $s=$source;$o=$owner;$h=$handoff
 switch($mutation[0]){'s'{$s=$s.Replace($mutation[1],$mutation[2])};'o'{$o=$o.Replace($mutation[1],$mutation[2])};'h'{$h=$h.Replace($mutation[1],$mutation[2])}}
 if(@(Rules $s $o $h) -notcontains $mutation[3]){throw "Mutation survived: $($mutation[1])"};$controls++
}
Write-Output "PASS: Vehicle native subgroup contracts ($controls controls, $($cases.Count) actual predicate cases)"
