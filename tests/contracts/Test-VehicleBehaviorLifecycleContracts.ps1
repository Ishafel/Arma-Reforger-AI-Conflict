param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
function Read([string]$path){[IO.File]::ReadAllText((Join-Path $root $path))}
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
$controller=Read 'Vehicles/AICF_TransportTripController.c'
$trip=Read 'State/Vehicles/AICF_TransportTrip.c'
$registry=Read 'State/Vehicles/AICF_TransportTripRegistry.c'
$coordinator=Read 'Vehicles/AICF_VehicleCoordinator.c'
$enums=Read 'State/Vehicles/AICF_TransportTripEnums.c'
$transit=Read 'Vehicles/AICF_VehicleTransitFlow.c'
function MountedWaitUsesDeliveryRadius([string]$source){
 $prepare=Body $source 'PrepareRoute'
 return $prepare -notmatch 'm_iReturnDistance' -and $prepare -match 'targetDistanceMeters <= m_Config.GetDismountDistanceMeters\(\)'
}
if(!(MountedWaitUsesDeliveryRadius $transit)){throw 'N must not expand mounted destination wait or stop an unfinished delivery'}
$incorrectWait=$transit.Replace('targetDistanceMeters <= m_Config.GetDismountDistanceMeters()', 'targetDistanceMeters <= trip.GetVehiclePreset().m_iReturnDistance')
if($incorrectWait -ceq $transit -or (MountedWaitUsesDeliveryRadius $incorrectWait)){throw 'Expanded mounted wait radius mutation survived'}
function Rules([string]$owner,[string]$state,[string]$view,[string]$facade){
 $dispatch=Body $owner 'DispatchCurrentPhase'
 $tactical=[regex]::Match($dispatch,'(?s)case AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT:(.*?)break;').Groups[1].Value
 if($tactical -notmatch 'outcome = m_BehaviorFlow.Tick\(trip, causationId\)'){'TACTICAL_DISPATCH'}
 if($tactical -notmatch 'if \(slot.HasPendingOrderRecovery\(\) && slot.PendingOrderRecoveryCountsAsReliabilityAttempt\(\)\)\s*return AICF_TripOutcome.Wait\("TACTICAL_INFANTRY_VERIFICATION_PENDING", causationId\);' -or
  $tactical.IndexOf('return AICF_TripOutcome.Wait(') -gt $tactical.IndexOf('m_BehaviorFlow.Tick(')){'RELIABILITY_OWNS_PENDING_BEFORE_FLOW_OR_HANDOFF'}
 $apply=Body $owner 'ApplyOutcome'
 if($apply -notmatch 'case AICF_ETripOutcomeKind.START_TACTICAL_DEPLOYMENT:\s*return StartTacticalDeployment' -or $apply -notmatch 'case AICF_ETripOutcomeKind.RESUME_TACTICAL_MOVEMENT:\s*return ResumeTacticalMovement'){'TYPED_OWNER_OUTCOMES'}
 $start=Body $owner 'StartTacticalDeployment'
 if($start -notmatch '!trip.GetVehiclePreset\(\)' -or $start -notmatch 'trip.GetPhase\(\) != AICF_ETransportTripPhase.TRANSIT' -or $start -notmatch '!TransitionTo\(trip, AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT' -or $start -notmatch '!trip.CommitBehaviorClockPause\(true, System.GetTickCount\(\)\)' -or $start -notmatch '!m_BehaviorFlow.Begin\(trip, outcome.GetReason\(\)\)'){'CURRENT_PRESET_TRANSIT_ENTRY'}
 if($start.IndexOf('TransitionTo(') -gt $start.IndexOf('CommitBehaviorClockPause(') -or $start.IndexOf('CommitBehaviorClockPause(') -gt $start.IndexOf('m_BehaviorFlow.Begin(')){'ENTRY_ORDER'}
 $resume=Body $owner 'ResumeTacticalMovement'
 if($resume -notmatch 'trip.GetPhase\(\) != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT' -or $resume -notmatch '!m_Handoff.SuspendInfantryOrder\(trip, slot, "TACTICAL_RETURN"\)' -or $resume -notmatch '!TransitionTo\(trip, AICF_ETransportTripPhase.TRANSIT' -or $resume -notmatch '!m_Handoff.AttachVehicleUtility\(trip\)' -or $resume -notmatch 'ApplyOutcome\(trip, m_TransitFlow.PrepareRoute\(trip, outcome.GetCausationId\(\)\)'){'SAME_LEASE_RETURN_OWNER_GATES'}
 if(($start+$resume) -match 'SpawnEntity|DeleteRplEntity|TryReserveLease|AddSupplies|GetInVehicle\(|GetOutVehicle\('){'ORCHESTRATION_ONLY'}
 $exit=Body $owner 'ExitPhaseEffects'
 if($exit -notmatch 'case AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT:\s*m_BehaviorFlow.Stop\(trip\);\s*m_Handoff.DetachVehicleUtility\(trip\);\s*return trip.CommitBehaviorClockPause\(false, System.GetTickCount\(\)\)'){'EXIT_RELEASES_BINDING_UTILITY_CLOCK'}
 $reset=Body $state 'ResetExitedPhase'
 if($reset -notmatch 'exitedPhase == AICF_ETransportTripPhase.TRANSIT\)\s*m_MovementState.Reset\(\)'){'OWNER_RESETS_OLD_DELIVERY_CLOCKS'}
 $commit=Body $state 'CommitTransition'
 if($commit -notmatch 'ResetExitedPhase\(previousPhase, nextPhase\)' -or $commit.IndexOf('ResetExitedPhase(') -gt $commit.IndexOf('m_Phase = nextPhase')){'RESET_BEFORE_COMMIT'}
 $pause=Body $state 'CommitBehaviorClockPause'
 if($pause -notmatch '!Replication.IsServer\(\) \|\| !m_VehiclePreset' -or $pause -notmatch 'paused && m_Phase != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT' -or $pause -notmatch 'if \(!paused\) m_iAbsoluteDeadlineMs \+= m_BehaviorState.PausedMs\(nowMs\)' -or $pause -notmatch 'm_BehaviorState.SetPaused\(paused, nowMs\)'){'AUTHORITY_SCOPED_TACTICAL_CLOCK'}
 $deadline=Body $state 'IsDeadlineReached'
 if($deadline -notmatch 'm_BehaviorState.PausedMs\(nowMs\)' -or (Body $state 'GetAbsoluteDeadlineMs') -notmatch 'm_BehaviorState.PausedMs\(System.GetTickCount\(\)\)'){'CLOCK_READERS_INCLUDE_SAME_PAUSE'}
 $abort=Body $owner 'AbortForStop'
 if($abort -notmatch 'm_BehaviorFlow.Stop\(trip\)' -or $abort.IndexOf('m_BehaviorFlow.Stop(trip)') -gt $abort.IndexOf('if (!trip.IsValid())')){'STOP_BEFORE_INVALID_RETURN'}
 $reassign=Body $state 'ReassignVehiclePresetSeat'
 if($reassign -notmatch '!Replication.IsServer\(\)' -or $reassign -notmatch '!IsVehiclePresetCurrent\(\)' -or $reassign -notmatch '!AICF_VehicleShortRecovery.IsCurrentMember\(this, VehiclePresetMember\(memberPosition\)\)' -or $reassign -match 'm_VehiclePresetSlot\.|SetVehiclePreset|m_aVehiclePresetMembers\.(Set|Insert|Remove)'){'REASSIGN_TRIP_COPY_CURRENT_MEMBER_ONLY'}
 $current=Body $state 'IsVehiclePresetCurrent'
 if($current -notmatch 'if \(!AICF_GroupRuntime.IsAliveCharacter\(member\)\)' -or $current -notmatch 'if \(AICF_GroupRuntime.IsAliveCharacter\(currentMember\)\) return false' -or $current -notmatch 'currentMember != member' -or (Body $state 'VehiclePresetMember') -notmatch 'member.GetID\(\) != m_aVehiclePresetMemberIds\[index\]'){'DEATH_IS_NOT_PERMISSION_FOR_NEW_IDENTITY'}
 if($view -notmatch 'trip.GetPhase\(\) == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT\)\s*m_bControlsMovement = trip.GetBehaviorState\(\).m_bReboarding' -or $view -notmatch 'm_bExecutableVehicleTask = trip.GetBehaviorState\(\).m_bReboarding &&\s*System.GetTickCount\(\) < trip.GetBehaviorState\(\).m_iReboardDeadline'){'FOOT_ORDER_VERSUS_BOUNDED_REBOARD_VIEW'}
 if((Body $facade 'IsInfantryOrderSuspended') -notmatch 'phase == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT && trip.GetBehaviorState\(\).m_bReboarding'){'REBOARD_SUSPENDS_INFANTRY'}
}
$failures=@(Rules $controller $trip $registry $coordinator)
if($failures.Count){throw "Vehicle behavior lifecycle: $($failures -join ',')"}
$checks=3
$phaseNames=@([regex]::Match($enums,'(?s)enum AICF_ETransportTripPhase\s*\{(.*?)\}').Groups[1].Value -split ','|ForEach-Object{($_ -replace '=\s*0','').Trim()})
$expected=@('WAITING_FOR_SITE','SITE_PLANNED','APPROACHING_SITE','STAGING_CONFIRMED','SPAWN_COMMIT','BOARDING','TRANSIT','DISMOUNT','HANDOFF','COMPLETE','FALLBACK','FAILED_CLOSED','TACTICAL_DEPLOYMENT')
if(($phaseNames -join ',') -cne ($expected -join ',')){throw 'Existing phase values must remain stable and tactical phase must be appended'}
$checks++
$matrix=Body $trip 'IsAllowedTransition'
foreach($entry in @(@('TRANSIT','DISMOUNT,TACTICAL_DEPLOYMENT,FALLBACK,FAILED_CLOSED'),@('TACTICAL_DEPLOYMENT','TRANSIT,FALLBACK,FAILED_CLOSED'))){
 $section=[regex]::Match($matrix,'(?s)case AICF_ETransportTripPhase\.'+$entry[0]+':\s*return (.*?);').Groups[1].Value
 $targets=@([regex]::Matches($section,'toPhase == AICF_ETransportTripPhase\.(\w+)')|ForEach-Object{$_.Groups[1].Value})
 if(($targets -join ',') -cne $entry[1]){throw "Invalid allowed transitions from $($entry[0])"}
 $checks++
}
$mutants=@(
 @('owner','outcome = m_BehaviorFlow.Tick(trip, causationId)','outcome = null','TACTICAL_DISPATCH'),
 @('owner','slot.HasPendingOrderRecovery() && slot.PendingOrderRecoveryCountsAsReliabilityAttempt()','false','RELIABILITY_OWNS_PENDING_BEFORE_FLOW_OR_HANDOFF'),
 @('owner','return AICF_TripOutcome.Wait("TACTICAL_INFANTRY_VERIFICATION_PENDING", causationId);','outcome = AICF_TripOutcome.Wait("TACTICAL_INFANTRY_VERIFICATION_PENDING", causationId);','RELIABILITY_OWNS_PENDING_BEFORE_FLOW_OR_HANDOFF'),
 @('owner','return StartTacticalDeployment(trip, outcome','return IgnoredDeployment(trip, outcome','TYPED_OWNER_OUTCOMES'),
 @('owner','!trip.CommitBehaviorClockPause(true, System.GetTickCount())','false','CURRENT_PRESET_TRANSIT_ENTRY'),
 @('owner','!m_Handoff.SuspendInfantryOrder(trip, slot, "TACTICAL_RETURN")','false','SAME_LEASE_RETURN_OWNER_GATES'),
 @('owner','m_BehaviorFlow.Stop(trip);','SkipBehaviorStop(trip);','EXIT_RELEASES_BINDING_UTILITY_CLOCK'),
 @('state','m_MovementState.Reset();','SkipMovementReset();','OWNER_RESETS_OLD_DELIVERY_CLOCKS'),
 @('state','ResetExitedPhase(previousPhase, nextPhase);','SkipPhaseReset(previousPhase, nextPhase);','RESET_BEFORE_COMMIT'),
 @('state','paused && m_Phase != AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT','false','AUTHORITY_SCOPED_TACTICAL_CLOCK'),
 @('state','m_BehaviorState.PausedMs(nowMs)','0','CLOCK_READERS_INCLUDE_SAME_PAUSE'),
 @('owner','m_BehaviorFlow.Stop(trip);','SkipBehaviorStop(trip);','STOP_BEFORE_INVALID_RETURN'),
 @('state','!AICF_VehicleShortRecovery.IsCurrentMember(this, VehiclePresetMember(memberPosition))','false','REASSIGN_TRIP_COPY_CURRENT_MEMBER_ONLY'),
 @('state','if (AICF_GroupRuntime.IsAliveCharacter(currentMember)) return false;','if (false) return false;','DEATH_IS_NOT_PERMISSION_FOR_NEW_IDENTITY'),
 @('view','m_bControlsMovement = trip.GetBehaviorState().m_bReboarding','m_bControlsMovement = true','FOOT_ORDER_VERSUS_BOUNDED_REBOARD_VIEW'),
 @('facade','phase == AICF_ETransportTripPhase.TACTICAL_DEPLOYMENT && trip.GetBehaviorState().m_bReboarding','false','REBOARD_SUSPENDS_INFANTRY')
)
foreach($mutant in $mutants){
 $nextOwner=$controller;$nextState=$trip;$nextView=$registry;$nextFacade=$coordinator
 switch($mutant[0]){
  owner{$nextOwner=$controller.Replace($mutant[1],$mutant[2]);if($nextOwner -ceq $controller){throw "Ineffective mutation: $($mutant[3])"}}
  state{$nextState=$trip.Replace($mutant[1],$mutant[2]);if($nextState -ceq $trip){throw "Ineffective mutation: $($mutant[3])"}}
  view{$nextView=$registry.Replace($mutant[1],$mutant[2]);if($nextView -ceq $registry){throw "Ineffective mutation: $($mutant[3])"}}
  facade{$nextFacade=$coordinator.Replace($mutant[1],$mutant[2]);if($nextFacade -ceq $coordinator){throw "Ineffective mutation: $($mutant[3])"}}
 }
 $found=@(Rules $nextOwner $nextState $nextView $nextFacade)
 if($mutant[3] -notin $found){throw "Negative mutation escaped: $($mutant[3])"}
 $checks++
}
"PASS Vehicle behavior lifecycle contracts checks=$checks native_runtime=NOT_RUN"
