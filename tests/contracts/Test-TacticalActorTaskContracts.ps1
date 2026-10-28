param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$view=[IO.File]::ReadAllText((Join-Path $root 'State/Vehicles/AICF_TransportTripRegistry.c'))
$controller=[IO.File]::ReadAllText((Join-Path $root 'Bootstrap/AICF_MatchController.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$v,[string]$c){
 $constructor=Body $v 'AICF_VehicleSlotView';$proof=Body $v 'IsTacticalActorTask';$meaningful=Body $c 'HasMeaningfulTask'
 if($constructor -notmatch 'm_bTacticalActorTask = currentBaseRevision >= 0 && assignment &&\s*assignment.GetBaseRevision\(\) == currentBaseRevision && IsTacticalActorTask\(trip\)'){'CURRENT_GRAPH_PROOF'}
 foreach($guard in @('!Replication.IsServer()','!trip.IsValid()','trip.GetPhase() != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT','!trip.IsVehiclePresetCurrent()',
  'deadline <= 0 || nowMs >= deadline','token.ResolveOwner()','token.OwnsAction(utility, action)','!action.GetRemoveAction()','!action.GetActionIsSuspended()',
  'action.GetActionState() != EAIActionState.FAILED','action.GetActionState() != EAIActionState.COMPLETED')){if(!$proof.Contains($guard)){'BOUNDED_CURRENT_OWNED_NATIVE_ACTION';break}}
 if($proof -notmatch 'if \(state.m_bReboarding\) deadline = state.m_iReboardDeadline' -or $proof -notmatch 'if \(state.m_bReboarding\) action = token.m_Entry'){'PHASE_ACTION_AND_DEADLINE_PAIRED'}
 if($meaningful -notmatch 'if \(vehicleView && vehicleView.HasTacticalActorTask\(\)\)\s*return true;' -or $c -notmatch 'native_tactical_actor_task=%1'){'TASK_CONSUMES_EXPORTED_PROOF'}
 if($proof -match '\.(?:AddAction|Complete|Fail|Set\w*|Commit\w*)\(|m_[ibf]\w*\s*='){'READ_ONLY_NO_CLOCK_OR_ACTION_MUTATION'}
}
$bad=@(Rules $view $controller);if($bad.Count){throw "Tactical actor task: $bad"}
$checks=1
foreach($mutation in @(
 @('v','currentBaseRevision >= 0','true','CURRENT_GRAPH_PROOF'),
 @('v','assignment.GetBaseRevision() == currentBaseRevision','true','CURRENT_GRAPH_PROOF'),
 @('v','!trip.IsVehiclePresetCurrent()','false','BOUNDED_CURRENT_OWNED_NATIVE_ACTION'),
 @('v','deadline <= 0 || nowMs >= deadline','false','BOUNDED_CURRENT_OWNED_NATIVE_ACTION'),
 @('v','token.OwnsAction(utility, action)','true','BOUNDED_CURRENT_OWNED_NATIVE_ACTION'),
 @('v','!action.GetRemoveAction()','true','BOUNDED_CURRENT_OWNED_NATIVE_ACTION'),
 @('v','!action.GetActionIsSuspended()','true','BOUNDED_CURRENT_OWNED_NATIVE_ACTION'),
 @('v','action.GetActionState() != EAIActionState.FAILED','true','BOUNDED_CURRENT_OWNED_NATIVE_ACTION'),
 @('v','action.GetActionState() != EAIActionState.COMPLETED','true','BOUNDED_CURRENT_OWNED_NATIVE_ACTION'),
 @('v','if (state.m_bReboarding) deadline = state.m_iReboardDeadline','if (false) deadline = state.m_iReboardDeadline','PHASE_ACTION_AND_DEADLINE_PAIRED'),
 @('c','if (vehicleView && vehicleView.HasTacticalActorTask())','if (vehicleView && vehicleView.GetPhase() == "TACTICAL_DEPLOYMENT")','TASK_CONSUMES_EXPORTED_PROOF')
)){
 $v=$view;$c=$controller;if($mutation[0] -eq 'v'){$v=$v.Replace($mutation[1],$mutation[2])}else{$c=$c.Replace($mutation[1],$mutation[2])}
 if(($v -ceq $view -and $c -ceq $controller) -or $mutation[3] -notin @(Rules $v $c)){throw "Tactical task mutation survived: $($mutation[3])"};$checks++
}
# Constructor's actual boolean is executed for stale/missing graph/native cases.
$expression=[regex]::Match((Body $view 'AICF_VehicleSlotView'),'(?s)m_bTacticalActorTask = (.*?);').Groups[1].Value
$expression=$expression.Replace('currentBaseRevision >= 0','$validRevision').Replace('assignment.GetBaseRevision() == currentBaseRevision','$sameRevision').Replace('assignment','$hasAssignment').Replace('IsTacticalActorTask(trip)','$nativeProof').Replace('&&','-and')
$evaluate=[scriptblock]::Create('param($validRevision,$sameRevision,$hasAssignment,$nativeProof) '+$expression)
for($index=-1;$index -lt 4;$index++){$flags=@($true,$true,$true,$true);if($index -ge 0){$flags[$index]=$false};if([bool](& $evaluate @flags) -ne ($index -lt 0)){throw 'Tactical task constructor accepted a stale/missing proof'};$checks++}
"PASS Tactical actor task: checks=$checks no_phase_only_task=1 original_taskless_deadline_unchanged=1"
