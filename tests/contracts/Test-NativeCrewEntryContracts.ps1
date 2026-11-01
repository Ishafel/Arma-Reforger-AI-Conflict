param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$entry=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_TransportNativeCrewEntry.c'))
$flow=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleTransitFlow.c'))
$state=[IO.File]::ReadAllText((Join-Path $root 'State/Vehicles/AICF_VehiclePhaseStates.c'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
function Rules([string]$policy,[string]$transit,[string]$movement){
 $capture=Body $policy 'Capture';$current=Body $policy 'IsCurrentContext';$running=Body $policy 'ResolveRunningAction';$pending=Body $policy 'PendingFailure';$settled=Body $policy 'IsSettled'
 $observe=Body $transit 'ObserveNativeCrewEntry';$start=Body $transit 'StartCrewRecovery';$mandatory=Body $transit 'ProcessMandatoryCrew';$token=Body $transit 'ProcessCrewRecoveryToken';$tick=Body $transit 'Tick';$advance=Body $transit 'AdvancePendingNativeCrewEntries'
 if(!$capture -or !$current -or !$running -or !$pending -or !$observe -or !$advance){'MISSING_METHODS';return}
 if((ConvertTo-AICFCodeText $policy) -match 'SetReserved|CancelOwnerSafe|\.Fail\(|\.Complete\(|AddAction|SetCurrentAction|SetOrigin|CallLater|BeginCrewRecovery|ObserveRouteProgress|ObservePhysicalMotion' -or $policy -match 'ref SCR_AIGetInVehicle m_LastAction|ref IEntity m_Member|ref BaseCompartmentSlot m_Seat'){'NATIVE_ACTION_AND_RESERVATION_STAY_NATIVE_OWNED'}
 foreach($fence in @('seat.GetOccupant()','role != EAICompartmentType.Pilot && role != EAICompartmentType.Turret','seat.GetType() != ECompartmentType.PILOT','seat.GetType() != ECompartmentType.TURRET','AICF_VehiclePresetNativeSeatBinding.Capture(trip)','seat.GetVehicle() != binding.m_Vehicle','int index = seats.Find(seat)','trip.GetVehiclePreset().MatchesSeats(seats)','trip.GetVehiclePreset().m_aMembers[index]','trip.VehiclePresetMember(position)','IsAuthoritativeAIEntity(member)','seat.IsReservedBy(member)','utility.m_OwnerEntity != member')){if(!$capture.Contains($fence)){'EXACT_EMPTY_RESERVED_PRESET_ROLE';break}}
 foreach($fence in @('m_Binding.m_Trip != trip','!m_Binding.IsCurrent()','trip.GetAssignment().GetAssignmentRevision() != m_iAssignmentRevision','trip.GetAssignment().GetStrategicIntentRevision() != m_iIntentRevision','trip.GetMovementState().GetRouteGeneration() != m_iRouteGeneration','m_Binding.FindSeat(m_Member) != m_Seat','m_Utility.m_OwnerEntity != m_Member','agent.GetControlledEntity() != m_Member','agent.GetParentGroup() != m_Binding.m_Group','SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent)) != m_Utility')){if(!$current.Contains($fence)){'REPEAT_GENERATIONAL_BINDING_AND_UTILITY';break}}
 foreach($fence in @('IsCurrentActivity(m_Binding, activity)','m_Utility.GetActions(actions)','m_Utility.GetCurrentAction()','m_Utility.GetExecutedAction()','action != current && action != executed','action.m_Utility != m_Utility','action.GetActionState() != EAIActionState.RUNNING','action.GetRemoveAction()','action.GetActionIsSuspended()','action.m_Vehicle.m_Value != m_Binding.m_Vehicle','action.m_CompartmentToGetIn.m_Value != m_Seat','action.GetRelatedGroupActivity() && action.GetRelatedGroupActivity() != activity')){if(!$running.Contains($fence)){'CURRENT_RUNNING_EXACT_NATIVE_ACTION_NOT_QUEUED_OR_STALE';break}}
 if($capture -notmatch 'entry.m_iStartedAtMs = nowMs;' -or $capture -notmatch 'entry.m_iDeadlineMs = Math.Min\(trip.GetAbsoluteDeadlineMs\(\), nowMs \+ timeoutMs\);' -or ([regex]::Matches($policy,'m_iDeadlineMs\s*=(?!=)')).Count -ne 1 -or $observe -notmatch 'm_Config.GetBoardingTimeoutMs\(\)' -or $observe -notmatch 'if \(!entry\)' -or $pending -match 'm_i(?:StartedAt|Deadline)Ms\s*='){'CAPTURE_ONCE_IMMUTABLE_EXISTING_BOARDING_WINDOW'}
 foreach($fence in @('occupant && occupant != m_Member','m_Seat.IsReserved() && !m_Seat.IsReservedBy(m_Member)','if (!occupant && !m_Seat.IsReservedBy(m_Member))','m_LastAction.GetActionState() == EAIActionState.FAILED','NATIVE_CREW_ENTRY_ACTION_NOT_CURRENT')){if(!$pending.Contains($fence)){'OWNERSHIP_OR_NATIVE_FAILURE_ENDS_WAIT';break}}
 foreach($fence in @('m_Seat.GetOccupant() != m_Member','character.IsInVehicle()','access.IsInCompartment()','access.IsGettingIn()','access.IsGettingOut()','access.IsSwitchingSeatsAnim()','access.GetCompartment() == m_Seat','CompartmentAccessComponent.GetVehicleIn(character) == m_Binding.m_Vehicle')){if(!$settled.Contains($fence)){'PHYSICAL_EXACT_SETTLEMENT';break}}
 $format=Body $policy 'FormatDetails'
 if($capture -notmatch 'entry.m_sSeatKey = AICF_VehiclePreset.SeatKey\(seat\)' -or $format -notmatch 'm_sSeatKey' -or $format -match 'SeatKey\(m_Seat\)|m_Seat\.|m_Binding\.'){'STALE_CONTEXT_DIAGNOSTICS_DO_NOT_DEREFERENCE_WEAK_SEAT'}
 if($observe.IndexOf('entry.IsDeadlineReached(nowMs)') -lt 0 -or $observe.IndexOf('entry.IsDeadlineReached(nowMs)') -gt $observe.IndexOf('entry.IsSettled()') -or $observe -notmatch 'FallbackToFoot\("NATIVE_CREW_ENTRY_TIMEOUT"' -or $observe -notmatch 'TerminalFailClosed\("NATIVE_CREW_ENTRY_CONTEXT_CHANGED"' -or $observe -notmatch 'FallbackToFoot\(failure, causationId\)' -or $observe -notmatch 'Wait\("NATIVE_PRESET_CREW_ENTRY_PENDING"'){'EXPIRY_BEFORE_SETTLEMENT_TYPED_FAILURES'}
 if($observe -match 'BeginCrewRecovery|CancelOwnerSafe|SetReserved|SuspendRouteWaypoint|ObservePhysicalMotion|ObserveRouteProgress|SetPresetDestinationWaiting' -or $start.IndexOf('ObserveNativeCrewEntry(') -gt $start.IndexOf('state.BeginCrewRecovery()') -or $mandatory.IndexOf('if (token)') -gt $mandatory.IndexOf('ObserveNativeCrewEntry(')){'PASSIVE_WAIT_BEFORE_NEW_ATTEMPT_EXISTING_TOKEN_FIRST'}
 $expiryAt=$token.IndexOf('!token.IsOwnerValid() || actionState == EAIActionState.FAILED || token.IsDeadlineReached(nowMs)')
 if($expiryAt -lt 0 -or $expiryAt -gt $token.IndexOf('nativePilotEntry = ObserveNativeCrewEntry(') -or $token -notmatch 'if \(nativePilotEntry\) return nativePilotEntry;'){'EXISTING_RECOVERY_TOKEN_EXPIRY_PRIORITY'}
 if($tick.IndexOf('AdvancePendingNativeCrewEntries(') -lt 0 -or $tick.IndexOf('AdvancePendingNativeCrewEntries(') -gt $tick.IndexOf('ObserveNativeCombat(') -or $tick.IndexOf('AdvancePendingNativeCrewEntries(') -gt $tick.IndexOf('ObserveNativeGate(') -or $advance -notmatch '!state.GetNativeCrewEntry\(EAICompartmentType.Pilot\) && !state.GetNativeCrewEntry\(EAICompartmentType.Turret\)' -or $advance -notmatch 'return ProcessMandatoryCrew\(trip, causationId, nowMs\)'){'PENDING_WINDOW_ADVANCES_BEFORE_HIGHER_NATIVE_WAITS'}
 $inspect=Body $transit 'InspectPendingNativeCrewEntry';$tokenPreflight=Body $transit 'CrewTokenRequiresImmediateAdvance'
 if($advance -notmatch 'array<EAICompartmentType> roles = \{EAICompartmentType.Pilot, EAICompartmentType.Turret\}' -or $advance -notmatch 'foreach \(EAICompartmentType role : roles\)' -or $advance.IndexOf('InspectPendingNativeCrewEntry(') -gt $advance.IndexOf('return ProcessMandatoryCrew(') -or $inspect -match '\.Capture\(|TrackNativeCrewEntry|\.Wait\(' -or $inspect.IndexOf('entry.IsDeadlineReached(nowMs)') -gt $inspect.IndexOf('entry.IsSettled()') -or $inspect -notmatch 'entry.PendingFailure\(\)' -or $inspect -notmatch 'ClearNativeCrewEntry\(entry.GetRole\(\)\)'){'ALL_SAVED_WINDOWS_SWEEP_BEFORE_ANY_ROLE_WAIT'}
 if($advance.IndexOf('CrewTokenRequiresImmediateAdvance(') -gt $advance.IndexOf('InspectPendingNativeCrewEntry(') -or $tokenPreflight -notmatch '!token.Matches\(' -or $tokenPreflight -notmatch '!token.IsOwnerValid\(\) \|\| token.GetActionState\(\) == EAIActionState.FAILED \|\| token.IsDeadlineReached\(nowMs\)' -or $advance -notmatch 'return ProcessCrewRecoveryToken\(trip, token, causationId, nowMs\)'){'INVALID_OWNED_TOKEN_BEFORE_PASSIVE_SWEEP'}
 $track=Body $movement 'TrackNativeCrewEntry';$reset=Body $movement 'Reset';$abort=Body $transit 'AbortOwnedActions';$clear=Body $movement 'ClearNativeCrewEntries'
 if($track -notmatch 'entry.GetRole\(\) != role \|\| GetNativeCrewEntry\(role\)' -or $abort -notmatch 'state.ClearNativeCrewEntries\(\)' -or $movement -notmatch 'm_CrewRecoveryToken = null;\s*ClearNativeCrewEntries\(\)' -or $clear -notmatch 'm_NativePilotEntry = null' -or $clear -notmatch 'm_NativeGunnerEntry = null' -or ([regex]::Matches($observe,'state.ClearNativeCrewEntry\(role\)')).Count -lt 3){'ONE_RECORD_PER_ROLE_EXPLICIT_CLEAR_WITHOUT_NATIVE_CANCEL'}
}
$issues=@(Rules $entry $flow $state)
if($issues.Count){throw "Native crew entry contracts: $issues"}
$controls=1
foreach($mutation in @(
 @('seat.GetOccupant()','false','EXACT_EMPTY_RESERVED_PRESET_ROLE'),
 @('!seat.IsReservedBy(member)','false','EXACT_EMPTY_RESERVED_PRESET_ROLE'),
 @('AICF_VehiclePresetNativeSeatBinding.Capture(trip)','UnsafeCapture(trip)','EXACT_EMPTY_RESERVED_PRESET_ROLE'),
 @('trip.GetAssignment().GetAssignmentRevision() != m_iAssignmentRevision','false','REPEAT_GENERATIONAL_BINDING_AND_UTILITY'),
 @('trip.GetMovementState().GetRouteGeneration() != m_iRouteGeneration','false','REPEAT_GENERATIONAL_BINDING_AND_UTILITY'),
 @('m_Utility.m_OwnerEntity != m_Member','false','REPEAT_GENERATIONAL_BINDING_AND_UTILITY'),
 @('action != current && action != executed','false','CURRENT_RUNNING_EXACT_NATIVE_ACTION_NOT_QUEUED_OR_STALE'),
 @('action.GetActionState() != EAIActionState.RUNNING','false','CURRENT_RUNNING_EXACT_NATIVE_ACTION_NOT_QUEUED_OR_STALE'),
 @('action.GetRelatedGroupActivity() && action.GetRelatedGroupActivity() != activity','false','CURRENT_RUNNING_EXACT_NATIVE_ACTION_NOT_QUEUED_OR_STALE'),
 @('entry.m_iDeadlineMs = Math.Min(trip.GetAbsoluteDeadlineMs(), nowMs + timeoutMs);','entry.m_iDeadlineMs = nowMs + timeoutMs;','CAPTURE_ONCE_IMMUTABLE_EXISTING_BOARDING_WINDOW'),
 @('m_LastAction.GetActionState() == EAIActionState.FAILED','false','OWNERSHIP_OR_NATIVE_FAILURE_ENDS_WAIT'),
 @('access.GetCompartment() == m_Seat','true','PHYSICAL_EXACT_SETTLEMENT')
 ,@('AICF_GroupTrace.Entity(m_Member), m_sSeatKey,','AICF_GroupTrace.Entity(m_Member), AICF_VehiclePreset.SeatKey(m_Seat),','STALE_CONTEXT_DIAGNOSTICS_DO_NOT_DEREFERENCE_WEAK_SEAT')
)){
 if(!$entry.Contains($mutation[0])){throw "Entry mutation target absent: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($entry.Replace($mutation[0],$mutation[1])) $flow $state)){throw "Entry mutation survived: $($mutation[2])"}
 $controls++
}
foreach($mutation in @(
 @('entry.IsDeadlineReached(nowMs)','false','EXPIRY_BEFORE_SETTLEMENT_TYPED_FAILURES'),
 @('nativePilotEntry = ObserveNativeCrewEntry(','nativePilotEntry = UnsafeWait(','EXISTING_RECOVERY_TOKEN_EXPIRY_PRIORITY'),
 @('pendingNativeEntry = AdvancePendingNativeCrewEntries(','pendingNativeEntry = UnsafeWait(','PENDING_WINDOW_ADVANCES_BEFORE_HIGHER_NATIVE_WAITS'),
 @('foreach (EAICompartmentType role : roles)','foreach (EAICompartmentType role : PilotOnly)','ALL_SAVED_WINDOWS_SWEEP_BEFORE_ANY_ROLE_WAIT'),
 @('else failure = entry.PendingFailure();','else failure = string.Empty;','ALL_SAVED_WINDOWS_SWEEP_BEFORE_ANY_ROLE_WAIT'),
 @('!token.IsOwnerValid() || token.GetActionState() == EAIActionState.FAILED || token.IsDeadlineReached(nowMs)','false','INVALID_OWNED_TOKEN_BEFORE_PASSIVE_SWEEP'),
 @('state.ClearNativeCrewEntries();','Print("leaked observer");','ONE_RECORD_PER_ROLE_EXPLICIT_CLEAR_WITHOUT_NATIVE_CANCEL')
)){
 if(!$flow.Contains($mutation[0])){throw "Flow mutation target absent: $($mutation[0])"}
 if($mutation[2] -notin @(Rules $entry ($flow.Replace($mutation[0],$mutation[1])) $state)){throw "Flow mutation survived: $($mutation[2])"}
 $controls++
}
$capture=Body $entry 'Capture'
$deadlineExpression=[regex]::Match($capture,'entry.m_iDeadlineMs = (Math.Min\(trip.GetAbsoluteDeadlineMs\(\), nowMs \+ timeoutMs\));').Groups[1].Value
if(!$deadlineExpression){throw 'No production deadline expression'}
$deadlineExpression=$deadlineExpression.Replace('Math.Min','[Math]::Min').Replace('trip.GetAbsoluteDeadlineMs()','$absolute').Replace('nowMs','$now').Replace('timeoutMs','$timeout')
$getDeadline=[scriptblock]::Create('param($absolute,$now,$timeout) '+$deadlineExpression)
$deadlineCases=@(@(100000,1000,25000,26000),@(15000,1000,25000,15000),@(1000,1000,25000,1000))
foreach($case in $deadlineCases){if((& $getDeadline $case[0] $case[1] $case[2]) -ne $case[3]){throw 'Native entry absolute clamp failed'}}
$trackGuard=[regex]::Match((Body $state 'TrackNativeCrewEntry'),'!entry \|\| entry.GetRole\(\) != role \|\| GetNativeCrewEntry\(role\)').Value
$trackGuard=$trackGuard.Replace('entry.GetRole()','$candidateRole').Replace('GetNativeCrewEntry(role)','$existing').Replace('!entry','-not $candidate').Replace('role','$role').Replace('!=','-ne').Replace('||','-or')
$rejectReplacement=[scriptblock]::Create('param($candidate,$candidateRole,$role,$existing) '+$trackGuard)
$deadline=26000
foreach($replayAt in @(2000,6000,24000)){
 $newCandidate=& $getDeadline 100000 $replayAt 25000
 if(-not (& $rejectReplacement $true 0 0 $true)){$deadline=$newCandidate}
 if($deadline -ne 26000){throw 'Action replay rearmed observation'}
}
$expired=Body $entry 'IsDeadlineReached'
$predicate=[regex]::Match($expired,'nowMs >= m_iDeadlineMs').Value.Replace('nowMs','$now').Replace('m_iDeadlineMs','$deadline').Replace('>=','-ge')
if(!$predicate){throw 'Missing expiry predicate'}
$isExpired=[scriptblock]::Create('param($now,$deadline) '+$predicate)
foreach($case in @(@(25999,$false),@(26000,$true),@(26001,$true))){if([bool](& $isExpired $case[0] 26000) -ne $case[1]){throw 'Exact native entry expiry changed'}}
# Native combat/OpenGate after the first sample cannot postpone this deadline;
# AdvancePendingNativeCrewEntries is before both waits and routes through token-first crew logic.
foreach($higherActivity in @('VehicleCombat','OpenGate')){if(-not (& $isExpired 26001 26000)){throw "$higherActivity hid native entry expiry"}}
# У gunner более ранний deadline. Живой pilot WAIT / здоровый recovery token
# не меняет результат извлечённого expiry guard всех сохранённых windows.
foreach($otherOwner in @('PilotWait','PilotRecoveryToken')){
 if((& $isExpired 26001 30000) -or -not (& $isExpired 26001 26000)){throw "$otherOwner hid earlier gunner expiry"}
}
$select=[regex]::Match((Body $entry 'ResolveRunningAction'),'action != current && action != executed').Value
$select=$select.Replace('action','$action').Replace('current','$current').Replace('executed','$executed').Replace('!=','-ne').Replace('&&','-and')
$isUnselected=[scriptblock]::Create('param($action,$current,$executed) '+$select)
foreach($case in @(@(1,1,2,$false),@(1,2,1,$false),@(1,2,3,$true))){if([bool](& $isUnselected $case[0] $case[1] $case[2]) -ne $case[3]){throw 'Queued action treated as current'} }
$related=[regex]::Match((Body $entry 'ResolveRunningAction'),'action.GetRelatedGroupActivity\(\) && action.GetRelatedGroupActivity\(\) != activity').Value
$related=$related.Replace('action.GetRelatedGroupActivity()','$related').Replace('activity','$activity').Replace('!=','-ne').Replace('&&','-and')
$isStale=[scriptblock]::Create('param($related,$activity) '+$related)
foreach($case in @(@($null,1,$false),@(1,1,$false),@(2,1,$true))){if([bool](& $isStale $case[0] $case[1]) -ne $case[2]){throw 'Stale related native activity accepted or legitimate NULL rejected'}}
"PASS Native crew entry: exact current reserved role, native ownership, fixed per-role25s/absolute window, replay/queued/stale rejection, token and pending expiry precedence, explicit lifecycle; controls=$controls window_cases=11 action_cases=6"
