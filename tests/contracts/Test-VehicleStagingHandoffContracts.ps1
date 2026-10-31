param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles'
$controller=[IO.File]::ReadAllText((Join-Path $root 'AICF_TransportTripController.c'))
$coordinator=[IO.File]::ReadAllText((Join-Path $root 'AICF_VehicleCoordinator.c'))
$handoff=[IO.File]::ReadAllText((Join-Path $root 'AICF_VehicleTaskHandoff.c'))
function Body([string]$source,[string]$method){
 Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $method
}
function Rules([string]$control,[string]$schedule){
 $begin=Body $control 'BeginOrReissueSiteApproach'
 $suspended=Body $schedule 'IsInfantryOrderSuspended'
 $lookup=Body $schedule 'FindTripForSlot'
 $end=Body $control 'EndVehicleControl'
 if(!$begin -or !$suspended -or !$lookup -or !$end){'MISSING_METHOD';return}
 $release=$begin.IndexOf('m_Handoff.SuspendInfantryOrder(trip, slot, "SPAWN_APPROACH_BEGIN")')
 if($release -lt 0 -or $release -gt $begin.IndexOf('CreateSpawnStagingWaypoint(') -or
    $release -gt $begin.IndexOf('BindVehicleWaypoint(') -or
    $release -gt $begin.IndexOf('RemoveSpawnApproachWaypoint(')){'SUSPEND_BEFORE_OWNED_STAGING'}
 if($begin -notmatch '!m_Handoff.SuspendInfantryOrder' -or
    $begin -notmatch 'SPAWN_APPROACH_STRATEGIC_SUSPEND_REJECTED"[\s\S]*?outcome.GetCausationId\(\), true\)'){'REJECT_AND_RESTORE'}
 if($begin -match 'm_OrderPlanner|SuspendOrderForVehicle|SetOrigin|Teleport|CallLater|Set.*Deadline'){'CONTROLLER_USES_HANDOFF_ONLY'}
 foreach($phase in @('SITE_PLANNED','APPROACHING_SITE','STAGING_CONFIRMED','SPAWN_COMMIT')){
  if($suspended -notmatch ('phase == AICF_ETransportTripPhase\.'+$phase+'\b')){'ACQUISITION_PHASE_'+$phase}
 }
 foreach($phase in @('BOARDING','TRANSIT','DISMOUNT','HANDOFF')){
  if($suspended -notmatch ('phase == AICF_ETransportTripPhase\.'+$phase+'\b')){'RETAINED_PHASE_'+$phase}
 }
 if($suspended -match 'phase == AICF_ETransportTripPhase\.WAITING_FOR_SITE\b'){'SAFE_WAIT_RETAINS_INFANTRY'}
 if($suspended -notmatch '!slot \|\| slot.GetWaypoint\(\)' -or
    $suspended -notmatch '!trip \|\| trip.GetHandoffState\(\).IsOrderRestored\(\)' -or
    $suspended -notmatch 'FindTripForSlot\(slot\)' -or
    $suspended -notmatch 'trip.GetHandoffState\(\).IsRestoreRequested\(\)'){'SUSPENSION_CURRENT_OWNER_GUARDS'}
 if($lookup -notmatch 'trip.GetSlotId\(\) == slot.GetSlotId\(\)' -or
    $lookup -notmatch 'trip.GetGroupGeneration\(\) == slot.GetSpawnGeneration\(\)' -or
    $lookup -notmatch 'trip.GetAssignment\(\).GetGroup\(\) == slot.GetGroup\(\)'){'STABLE_SLOT_GENERATION_AND_GROUP'}
 if($end.IndexOf('ReleaseSpawnPlan(') -gt $end.IndexOf('RestoreInfantryOrder(') -or
    $end -notmatch 'restoreCurrentOrder && slot && faction'){'CANCEL_RELEASE_THEN_CURRENT_RESTORE'}
}
$issues=@(Rules $controller $coordinator)
if($issues.Count){throw "Staging handoff: $issues"}
$controls=1
foreach($mutation in @(
 @('c','!m_Handoff.SuspendInfantryOrder(trip, slot, "SPAWN_APPROACH_BEGIN")','false','SUSPEND_BEFORE_OWNED_STAGING'),
 @('c','SPAWN_APPROACH_STRATEGIC_SUSPEND_REJECTED','IGNORED_SUSPEND','REJECT_AND_RESTORE'),
 @('c','restoreCurrentOrder && slot && faction','false','CANCEL_RELEASE_THEN_CURRENT_RESTORE'),
 @('v','!slot || slot.GetWaypoint()','!slot','SUSPENSION_CURRENT_OWNER_GUARDS'),
 @('v','!trip || trip.GetHandoffState().IsOrderRestored()','!trip','SUSPENSION_CURRENT_OWNER_GUARDS'),
 @('v','trip.GetHandoffState().IsRestoreRequested()','false','SUSPENSION_CURRENT_OWNER_GUARDS'),
 @('v','trip.GetGroupGeneration() == slot.GetSpawnGeneration()','true','STABLE_SLOT_GENERATION_AND_GROUP'),
 @('v','trip.GetAssignment().GetGroup() == slot.GetGroup()','true','STABLE_SLOT_GENERATION_AND_GROUP')
)){
 $c=$controller;$v=$coordinator
 if($mutation[0] -eq 'c'){$c=$c.Replace($mutation[1],$mutation[2])}else{$v=$v.Replace($mutation[1],$mutation[2])}
 if($c -ceq $controller -and $v -ceq $coordinator){throw 'Absent mutation target'}
 if($mutation[3] -notin @(Rules $c $v)){throw "Mutation survived: $($mutation[3])"}
 $controls++
}
foreach($phase in @('SITE_PLANNED','APPROACHING_SITE','STAGING_CONFIRMED','SPAWN_COMMIT','BOARDING','TRANSIT','DISMOUNT','HANDOFF')){
 $mutated=$coordinator.Replace('phase == AICF_ETransportTripPhase.'+$phase,'false')
 $rule='ACQUISITION_PHASE_'+$phase
 if($phase -in @('BOARDING','TRANSIT','DISMOUNT','HANDOFF')){$rule='RETAINED_PHASE_'+$phase}
 if($rule -notin @(Rules $controller $mutated)){throw "Phase mutation survived: $phase"}
 $controls++
}
$suspended=Body $coordinator 'IsInfantryOrderSuspended'
$waiting=$coordinator.Replace($suspended,$suspended.Replace('return phase ==','return phase == AICF_ETransportTripPhase.WAITING_FOR_SITE || phase =='))
if('SAFE_WAIT_RETAINS_INFANTRY' -notin @(Rules $controller $waiting)){throw 'Unsafe waiting suspension survived'}
$controls++
$begin=Body $controller 'BeginOrReissueSiteApproach'
$sideEffect=$controller.Replace($begin,$begin.Insert(1,'slot.GetGroup().SetOrigin(plan.GetStagingPosition());'))
if('CONTROLLER_USES_HANDOFF_ONLY' -notin @(Rules $sideEffect $coordinator)){throw 'Movement mutation survived'}
$controls++
$suspend=Body $handoff 'SuspendInfantryOrder'
if($suspend -notmatch 'IsCurrentAssignment\(trip, slot\)' -or
   $suspend -notmatch 'oldWaypoint != trip.GetAssignment\(\).GetMeaningfulInfantryWaypoint\(\)' -or
   $suspend -notmatch 'm_OrderPlanner.SuspendOrderForVehicle\(slot\)' -or
   $suspend -notmatch 'return slot.GetWaypoint\(\) == null'){
 throw 'Existing exact infantry suspension fence was removed'
}
"PASS Staging handoff: exact Handoff suspension before approach bind/reissue, four acquisition phases without safe wait, prior transport phases/restore preserved, stable slot/generation/group and cancellation restore; controls=$controls"
