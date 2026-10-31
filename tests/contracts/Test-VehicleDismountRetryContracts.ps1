param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$flow=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleDismountFlow.c'))
$states=[IO.File]::ReadAllText((Join-Path $root 'State/Vehicles/AICF_VehiclePhaseStates.c'))
$start=$states.IndexOf('class AICF_VehicleDismountState')
$end=$states.IndexOf('class AICF_VehicleHandoffState',$start)
if($start -lt 0 -or $end -le $start){throw 'Dismount state class missing'}
$state=$states.Substring($start,$end-$start)
function Body([string]$text,[string]$name){
 Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name
}
function Rules([string]$source,[string]$phase){
 $normal=Body $source 'ProcessNormalDismount'
 $retry=Body $source 'TryRequestAnimatedExactManagedExits'
 $candidate=Body $source 'IsExactAnimatedExitCandidate'
 $member=Body $source 'IsExactAliveCurrentMember'
 $record=Body $phase 'TryRecordAnimatedExactExitAttempt'
 $reset=Body $phase 'Reset'
 if(!$normal -or !$retry -or !$candidate -or !$record -or !$reset){'MISSING_METHOD';return}
 if($source -notmatch 'ANIMATED_EXACT_EXIT_DELAY_MS = 5000' -or $source -notmatch 'ANIMATED_EXACT_EXIT_MAX_ATTEMPTS = 3' -or $normal -notmatch 'TryRequestAnimatedExactManagedExits\(trip, sample, nowMs\)' -or $retry -notmatch 'nowMs, ANIMATED_EXACT_EXIT_MAX_ATTEMPTS, ANIMATED_EXACT_EXIT_DELAY_MS'){'FIXED_THREE_OPPORTUNITIES'}
 if($source -match 'WasAnimatedExactExitAttempted|MarkAnimatedExactExitAttempted' -or $phase -match 'm_bAnimatedExactExitAttempted'){'NO_ONE_SHOT_GATE'}
 if($record -notmatch 'm_iAnimatedExactExitAttempts >= Math.Max\(0, maximumAttempts\)' -or $record -notmatch 'm_iAnimatedExactExitAttempts\+\+'){'ATTEMPT_CAP'}
 if($record -notmatch 'nowMs - m_iStartedAtMs < retryIntervalMs' -or $record -notmatch 'nowMs - m_iLastAnimatedExactExitAttemptAtMs < retryIntervalMs' -or $record -notmatch 'm_iLastAnimatedExactExitAttemptAtMs = nowMs'){'RETRY_INTERVAL'}
 if($record -notmatch 'nowMs >= m_iNormalDeadlineMs' -or $record -notmatch 'nowMs >= m_iTerminalDeadlineMs' -or $record -notmatch 'm_bTerminalClearanceStarted \|\| m_bTerminalClearanceStopped' -or $candidate -notmatch 'nowMs < state.GetNormalDeadlineMs\(\) && nowMs < state.GetTerminalDeadlineMs\(\)' -or $candidate -notmatch '!trip.IsDeadlineReached\(nowMs\)'){'FIXED_DEADLINE_BOUND'}
 if(($record+$retry+$candidate) -match '(?:m_iNormalDeadlineMs|m_iTerminalDeadlineMs)\s*(?:=|\+=)|state.Begin\(|CommitPresetDestinationWait|SetAbsoluteDeadline'){'NO_REARM'}
 if($reset -notmatch 'm_iAnimatedExactExitAttempts = 0' -or $reset -notmatch 'm_iLastAnimatedExactExitAttemptAtMs = 0'){'PHASE_RESET'}
 if($candidate -notmatch 'IsAuthoritativeTripAssetCurrent\(trip\)' -or $candidate -notmatch 'trip.GetPhase\(\) != AICF_ETransportTripPhase.DISMOUNT' -or $candidate -notmatch 'trip.GetAssignment\(\).GetGroup\(\) != group' -or $candidate -notmatch 'trip.GetLease\(\).GetVehicle\(\) != vehicle' -or $candidate -notmatch '!IsExactAliveCurrentMember\(group, agent\)' -or $candidate -notmatch 'agent.GetControlledEntity\(\) != character'){'CURRENT_TRIP_MEMBER'}
 if($member -notmatch 'agent.GetParentGroup\(\) == group' -or $member -notmatch 'AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity'){'AI_AUTHORITY_ONLY'}
 if($candidate -notmatch 'character.GetCompartmentAccessComponent\(\) != access' -or $candidate -notmatch 'access.GetCompartment\(\) != compartment' -or $candidate -notmatch 'compartment.GetVehicle\(\) != vehicle' -or $candidate -notmatch 'compartment.GetOccupant\(\) != character' -or $candidate -notmatch 'CompartmentAccessComponent.GetVehicleIn\(character\) != vehicle'){'EXACT_SETTLED_OCCUPANT'}
 if($candidate -notmatch 'access.IsGettingIn\(\) \|\| access.IsGettingOut\(\) \|\| access.IsSwitchingSeatsAnim\(\)'){'NO_TRANSITION_INTERRUPTION'}
 if($retry -match 'ResetGroupVehicleActions|ReleaseCompartments'){'NO_GLOBAL_RESET'}
 $guard='IsExactAnimatedExitCandidate(trip, group, vehicle, agent, character, access, compartment)'
 if($retry.IndexOf($guard) -lt 0 -or $retry.IndexOf($guard) -gt $retry.IndexOf('state.TryRecordAnimatedExactExitAttempt')){'ELIGIBILITY_BEFORE_BUDGET'}
 if(@([regex]::Matches($retry,[regex]::Escape($guard))).Count -ne 2 -or $retry.LastIndexOf($guard) -lt $retry.IndexOf('access.InterruptVehicleActionQueue') -or $retry.LastIndexOf($guard) -gt $retry.IndexOf('access.GetOutVehicle')){'POST_INTERRUPT_REVALIDATION'}
 if($retry -notmatch 'EGetOutType.ANIMATED' -or ($retry+$candidate) -match 'Teleport|EjectOccupant|GetOutVehicle_NoDoor|SetOrigin|DeleteEntity|CallLater|CommitTransition'){'ANIMATED_ONLY'}
}
$failures=@(Rules $flow $state)
if($failures.Count){throw "Dismount retry: $failures"}
$controls=1
foreach($mutation in @(
 @('state','m_iAnimatedExactExitAttempts >= Math.Max(0, maximumAttempts)','false','ATTEMPT_CAP'),
 @('state','nowMs - m_iLastAnimatedExactExitAttemptAtMs < retryIntervalMs','false','RETRY_INTERVAL'),
 @('state','nowMs >= m_iNormalDeadlineMs','false','FIXED_DEADLINE_BOUND'),
 @('state','nowMs >= m_iTerminalDeadlineMs','false','FIXED_DEADLINE_BOUND'),
 @('state','m_iAnimatedExactExitAttempts = 0','m_iAnimatedExactExitAttempts = 99','PHASE_RESET'),
 @('flow','agent.GetControlledEntity() != character','false','CURRENT_TRIP_MEMBER'),
 @('flow','AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(','UnsafeAIEntity(','AI_AUTHORITY_ONLY'),
 @('flow','compartment.GetOccupant() != character','false','EXACT_SETTLED_OCCUPANT'),
 @('flow','access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()','false','NO_TRANSITION_INTERRUPTION'),
 @('flow','access.InterruptVehicleActionQueue(true, true, true);',"m_Watchdog.ResetGroupVehicleActions(group);`n access.InterruptVehicleActionQueue(true, true, true);",'NO_GLOBAL_RESET')
)){
 $mutatedFlow=$flow;$mutatedState=$state
 if($mutation[0] -eq 'state'){
  if(!$state.Contains($mutation[1])){throw "Mutation target absent: $($mutation[3])"}
  $mutatedState=$state.Replace($mutation[1],$mutation[2])
 }else{
  if(!$flow.Contains($mutation[1])){throw "Mutation target absent: $($mutation[3])"}
  $mutatedFlow=$flow.Replace($mutation[1],$mutation[2])
 }
 if($mutation[3] -notin @(Rules $mutatedFlow $mutatedState)){throw "Mutation survived: $($mutation[3])"}
 $controls++
}
$retry=Body $flow 'TryRequestAnimatedExactManagedExits'
$guard='IsExactAnimatedExitCandidate(trip, group, vehicle, agent, character, access, compartment)'
$last=$retry.LastIndexOf($guard)
$oneGuard=$retry.Remove($last,$guard.Length).Insert($last,'true')
if('POST_INTERRUPT_REVALIDATION' -notin @(Rules ($flow.Replace($retry,$oneGuard)) $state)){throw 'Post-interrupt identity mutation survived'}
$controls++
$record=Body $state 'TryRecordAnimatedExactExitAttempt'
$rearm=$record.Insert(1,"`n m_iNormalDeadlineMs = nowMs + 20000;")
if('NO_REARM' -notin @(Rules $flow ($state.Replace($record,$rearm)))){throw 'Deadline rearm mutation survived'}
$controls++
$teleport=$retry.Insert(1,"`n character.Teleport(transform);")
if('ANIMATED_ONLY' -notin @(Rules ($flow.Replace($retry,$teleport)) $state)){throw 'Teleport mutation survived'}
$controls++
"PASS Vehicle dismount retry: three spaced opportunities before fixed deadlines, exact authoritative stationary occupants, repeated identity checks and no global reset/hidden mutation; controls=$controls"
