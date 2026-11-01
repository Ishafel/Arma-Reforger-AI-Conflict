param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Read([string]$path){[IO.File]::ReadAllText((Join-Path $core $path))}
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$proof=Read 'Vehicles/AICF_VehicleStagingTaskProof.c'
$view=Read 'State/Vehicles/AICF_TransportTripRegistry.c'
$controller=Read 'Bootstrap/AICF_MatchController.c'
$flow=Read 'Vehicles/AICF_VehicleAcquisitionFlow.c'
$tokens=Read 'Vehicles/AICF_VehicleSpawner.c'
function Rules([string]$p,[string]$v,[string]$c,[string]$f,[string]$t){
 $body=Body $p 'HasCurrentTask'
 foreach($guard in @('!Replication.IsServer()','!trip.IsValid()','trip.IsTerminal()','trip.GetLease()',
  'trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE','!trip.IsVehiclePresetCurrent()',
  'trip.IsDeadlineReached(nowMs)','assignment.GetBaseRevision() != currentBaseRevision',
  'slot.GetSpawnGeneration() != trip.GetGroupGeneration()',
  'slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision()',
  'slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()',
  'request.IsDeadlineReached(nowMs)','nowMs >= plan.GetApproachDeadlineMs()',
  'reservation.IsExpired(nowMs)','!reservation.MatchesTrip(trip, request.GetRequestGeneration())',
  '!token.Matches(trip, request.GetRequestGeneration(), group)','!token.MatchesTaskAssignment(assignment)',
  'token.GetTargetPosition() != plan.GetStagingPosition()','!token.IsActionOwnedByUtility()',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor)',
  '!AICF_VehicleNativeSubgroup.CapturedMember(trip, actor)',
  'character.GetAIControlComponent().GetControlAIAgent() != token.GetAgent()',
  'action.GetActionState() != EAIActionState.RUNNING','action.GetRemoveAction()','action.GetActionIsSuspended()',
  'utility.GetCurrentAction() != action','utility.GetExecutedAction() != action',
  'action.m_vPosition.m_Value != token.GetTargetPosition()','action.m_Entity.m_Value')){
  if(!$body.Contains($guard)){'ACTUAL_CURRENT_STAGING_TASK_ONLY';break}
 }
 if(!(Body $t 'CaptureTaskAssignment').Contains('if (m_TaskAssignment || !assignment || !assignment.IsValid()) return false;') -or
  !(Body $t 'MatchesTaskAssignment').Contains('m_TaskAssignment.MatchesAssignmentRevision(assignment)') -or
  !(Body $f 'IssueStagingMemberAction').Contains('!token.CaptureTaskAssignment(trip.GetAssignment()) || !plan.TrackStagingActionToken(token)')){'IMMUTABLE_ASSIGNMENT_CAPTURE'}
 if(!(Body $v 'AICF_VehicleSlotView').Contains('m_bStagingActorTask = AICF_VehicleStagingTaskProof.HasCurrentTask(trip, currentBaseRevision, System.GetTickCount());') -or
  !(Body $c 'HasMeaningfulTask').Contains('if (vehicleView && vehicleView.HasStagingActorTask())')){'READ_ONLY_PROOF_EXPORT'}
 if((ConvertTo-AICFCodeText $p) -match '\b(?:SetOrigin|Teleport|AddAction|RemoveAction|ObserveMeaningfulTaskLoss|SetHealth|Heal|Begin|Reset|CommitTransition|CallLater)\s*\('){'NO_TIME_OR_ACTION_MUTATION'}
}
$issues=@(Rules $proof $view $controller $flow $tokens);if($issues.Count){throw "Staging actor task: $issues"};$checks=1
foreach($guard in @('!trip.IsValid()','!trip.IsVehiclePresetCurrent()','trip.IsDeadlineReached(nowMs)',
 'assignment.GetBaseRevision() != currentBaseRevision','slot.GetSpawnGeneration() != trip.GetGroupGeneration()',
 'slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision()',
 'request.IsDeadlineReached(nowMs)','nowMs >= plan.GetApproachDeadlineMs()',
 'reservation.IsExpired(nowMs)','!token.Matches(trip, request.GetRequestGeneration(), group)',
 '!token.MatchesTaskAssignment(assignment)','token.GetTargetPosition() != plan.GetStagingPosition()',
 '!token.IsActionOwnedByUtility()','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(actor)',
 'action.GetActionState() != EAIActionState.RUNNING','action.GetRemoveAction()','action.GetActionIsSuspended()',
 'utility.GetCurrentAction() != action','utility.GetExecutedAction() != action','action.m_vPosition.m_Value != token.GetTargetPosition()')){
 if('ACTUAL_CURRENT_STAGING_TASK_ONLY' -notin @(Rules ($proof.Replace($guard,'false')) $view $controller $flow $tokens)){throw "Staging mutation survived: $guard"};$checks++
}
$bad=$flow.Replace('!token.CaptureTaskAssignment(trip.GetAssignment()) || ','')
if('IMMUTABLE_ASSIGNMENT_CAPTURE' -notin @(Rules $proof $view $controller $bad $tokens)){throw 'Assignment capture mutation survived'};$checks++
$bad=$controller.Replace('vehicleView.HasStagingActorTask()','vehicleView.GetPhase() == "APPROACHING_SITE"')
if('READ_ONLY_PROOF_EXPORT' -notin @(Rules $proof $view $bad $flow $tokens)){throw 'Phase exemption mutation survived'};$checks++
"PASS Staging actor task proof controls=$checks actual_native=NOT_RUN"
