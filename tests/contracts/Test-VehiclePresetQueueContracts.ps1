param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBoardingFlow.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $tick=Body $text 'TickPreset';$yield=Body $text 'TryYieldPresetSeat';$retry=Body $text 'TryRetryPresetSeat';$begin=Body $text 'BeginPreset'
 if(!$tick -or !$yield -or !$retry -or !$begin){'MISSING_METHOD';return}
 if($tick -match 'bool pendingSeat|if \(!pendingSeat\)' -or $tick -notmatch 'AICF_VehicleBoardingActionToken firstUnqueued' -or $tick -notmatch 'AICF_VehicleBoardingActionToken firstRetry' -or $tick -notmatch 'token.GetRetryCount\(\) < firstRetry.GetRetryCount\(\)'){'NO_HEAD_OF_LINE_STARVATION'}
 $gate=$tick.IndexOf('if (memberTransition || activeEntries > 1)')
 $parallel=$tick.IndexOf('if (busyTokens.Count() == 1)')
 if($gate -lt 0 -or $parallel -lt 0 -or $tick -notmatch 'access.IsGettingIn\(\) \|\| access.IsGettingOut\(\) \|\| access.IsSwitchingSeatsAnim\(\)' -or $gate -gt $tick.IndexOf('TryYieldPresetSeat') -or $parallel -gt $tick.IndexOf('QueuePresetSeat') -or $tick -notmatch '(?s)if \(busyTokens.Count\(\) == 1\).*?foreach \(AICF_VehicleBoardingActionToken next : readyUnqueued\).*?!AICF_PresetEntrySeparation.CanPair\(trip, lease, group, next, busyTokens\[0\]\).*?!QueuePresetSeat\(trip, state, next\).*?return AICF_TripOutcome.Wait\("PRESET_INDEPENDENT_ENTRIES_PENDING"'){'BOUNDED_INDEPENDENT_ENTRIES'}
 if($tick -notmatch '(?s)memberTransition = true;\s*busyTokens.Insert\(token\)' -or $tick -notmatch 'if \(!busyTokens.Contains\(token\)\) busyTokens.Insert\(token\)' -or $tick -notmatch 'if \(readyOnFoot\) readyUnqueued.Insert\(token\)' -or $tick -notmatch '(?s)if \(!memberTransition && activeEntries > 1\).*?TryYieldPresetSeat.*?return AICF_TripOutcome.Wait\("PRESET_ENTRY_TRANSITION_PENDING"'){'UNION_BUSY_ENTRIES_AND_TRANSITIONS'}
 if($tick -notmatch 'token.IsTrackedActionOwnedByUtility\(\) && actionState != EAIActionState.FAILED && actionState != EAIActionState.COMPLETED' -or $tick -notmatch 'activeEntries\+\+'){'ACTIVE_OWNERSHIP_OBSERVATION'}
 if($tick -notmatch 'if \(activeToken\)\s*TryRetryPresetSeat' -or $tick -notmatch 'else if \(firstUnqueued\)\s*QueuePresetSeat' -or $tick -notmatch 'else if \(firstRetry\)\s*TryRetryPresetSeat' -or $tick.IndexOf('else if (firstUnqueued)') -gt $tick.IndexOf('else if (firstRetry)')){'FIRST_ATTEMPT_BEFORE_RETRIES'}
 if($tick -notmatch 'seats\[i\].IsCompartmentAccessible\(\) && !seats\[i\].IsGetInLockedFor\(member\)' -or $tick -notmatch '!seats\[i\].IsReserved\(\) \|\| seats\[i\].IsReservedBy\(member\)'){'READY_EXACT_UNQUEUED_PAIR'}
 if($tick -notmatch 'access.GetCompartment\(\) == seats\[i\] && !access.IsGettingIn\(\) && !access.IsGettingOut\(\) && !access.IsSwitchingSeatsAnim\(\)' -or $tick.IndexOf('memberTransition = true') -gt $tick.IndexOf('if (occupant == member')){'ALL_ASSIGNED_TRANSITIONS_BEFORE_SETTLED'}
 if($tick -notmatch 'if \(!firstUnqueued && readyOnFoot\)' -or $tick -notmatch 'else if \(readyOnFoot && token.GetRetryCount\(\) < 2' -or $tick -notmatch 'bool readyOnFoot = access && !access.IsInCompartment\(\) && !access.IsGettingIn\(\) && !access.IsGettingOut\(\)'){'RETRY_SELECTION_READY_SEAT_ONLY'}
 if($yield -notmatch '!trip.IsVehiclePresetCurrent\(\)' -or @([regex]::Matches($yield,'!token.Matches\(trip, lease, group\)')).Count -ne 2 -or @([regex]::Matches($yield,'!token.IsPhysicalMutationOwnerSafe\(\)')).Count -ne 2 -or $yield -notmatch 'seat.GetVehicle\(\) != lease.GetVehicle\(\)' -or $yield -notmatch 'seat.IsReserved\(\) && !seat.IsReservedBy\(member\)'){'EXACT_IDENTITY_BEFORE_AND_AFTER_YIELD'}
 if($yield -notmatch 'token.GetProgressAgeMs\(\) < 20000' -or @([regex]::Matches($yield,'access.IsInCompartment\(\) \|\| access.IsGettingIn\(\) \|\| access.IsGettingOut\(\) \|\| access.IsSwitchingSeatsAnim\(\)')).Count -ne 2 -or @([regex]::Matches($yield,'CompartmentAccessComponent.GetVehicleIn\(member\)')).Count -ne 2){'NO_SEATED_OR_TRANSITION_CANCEL'}
 if($yield -notmatch 'token.CancelOwnerSafe\(\)' -or $yield -notmatch 'token.IsTrackedActionOwnedByUtility\(\) && actionState != EAIActionState.FAILED && actionState != EAIActionState.COMPLETED' -or $yield -match 'ResetGroupVehicleActions|ReleaseCompartments|RecordPresetQueued|AddAction|new SCR_AI|\.Fail\('){'ONLY_OWNED_ACTION_YIELD'}
 if($tick.IndexOf('PRESET_BOARDING_DEADLINE') -gt $parallel -or $tick.IndexOf('PRESET_BOARDING_DEADLINE') -gt $gate -or $yield -notmatch 'System.GetTickCount\(\) >= state.GetAbsoluteDeadlineMs\(\)' -or (ConvertTo-AICFCodeText ($tick+$yield)) -match 'state.Begin\(|DeadlineMs\s*=|m_i.*Deadline.*=|SetPreset|CommitTransition|Teleport|SetOrigin|\.GetInVehicle\('){'FIXED_DEADLINE_NO_HIDDEN_MUTATION'}
 if($tick -notmatch '(?s)VEHICLE_PRESET_BOARDING_DEADLINE_SNAPSHOT.*?settled_polls=.*?required_settled_polls=.*?state.GetSettledPollCount\(\), SETTLED_POLLS_REQUIRED' -or $tick.IndexOf('VEHICLE_PRESET_BOARDING_DEADLINE_SNAPSHOT') -gt $tick.IndexOf('return Reject(trip, lease, causationId, "PRESET_BOARDING_DEADLINE")')){'DEADLINE_PROGRESS_RECEIPT'}
 if($retry -notmatch 'token.GetRetryCount\(\) >= 2' -or $retry -notmatch 'token.GetProgressAgeMs\(\) < 12000' -or $retry -notmatch 'state.RemoveActionFence\(token.GetFence\(\)\)' -or $retry -notmatch 'TrackToken\(state, replacement\)'){'RETRIES_REMAIN_BOUNDED_AND_FENCED'}
 if($begin -notmatch 'GetBoardingTimeoutMs\(\) \* Math.Max\(3, requiredSeats\)' -or $begin -notmatch 'Math.Min\(trip.GetAbsoluteDeadlineMs\(\), now \+ plannedTimeoutMs\)'){'ORIGINAL_TOTAL_BUDGET'}
}
$failures=@(Rules $source)
if($failures.Count){throw "Preset queue: $failures"}
$controls=1
foreach($mutation in @(
 @('if (memberTransition || activeEntries > 1)','if (false)','BOUNDED_INDEPENDENT_ENTRIES'),
 @('if (busyTokens.Count() == 1)','if (busyTokens.Count() <= 2)','BOUNDED_INDEPENDENT_ENTRIES'),
 @('!AICF_PresetEntrySeparation.CanPair(trip, lease, group, next, busyTokens[0])','false','BOUNDED_INDEPENDENT_ENTRIES'),
 @('if (!busyTokens.Contains(token)) busyTokens.Insert(token);','Print("omitted");','UNION_BUSY_ENTRIES_AND_TRANSITIONS'),
 @('if (readyOnFoot) readyUnqueued.Insert(token);','readyUnqueued.Insert(token);','UNION_BUSY_ENTRIES_AND_TRANSITIONS'),
 @('state.GetSettledPollCount(), SETTLED_POLLS_REQUIRED','0, 2','DEADLINE_PROGRESS_RECEIPT'),
 @('token.GetRetryCount() < firstRetry.GetRetryCount()','false','NO_HEAD_OF_LINE_STARVATION'),
 @('else if (firstUnqueued)','else if (false)','FIRST_ATTEMPT_BEFORE_RETRIES'),
 @('&& !access.IsGettingOut() && !access.IsSwitchingSeatsAnim() &&','&& !access.IsGettingOut() &&','ALL_ASSIGNED_TRANSITIONS_BEFORE_SETTLED'),
 @('else if (readyOnFoot && token.GetRetryCount() < 2','else if (token.GetRetryCount() < 2','RETRY_SELECTION_READY_SEAT_ONLY'),
 @('!seats[i].IsReserved() || seats[i].IsReservedBy(member)','true','READY_EXACT_UNQUEUED_PAIR'),
 @('!token.Matches(trip, lease, group)','false','EXACT_IDENTITY_BEFORE_AND_AFTER_YIELD'),
 @('token.GetProgressAgeMs() < 20000','false','NO_SEATED_OR_TRANSITION_CANCEL'),
 @('access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()','false','NO_SEATED_OR_TRANSITION_CANCEL'),
 @('token.CancelOwnerSafe();','m_Watchdog.ResetGroupVehicleActions(group);','ONLY_OWNED_ACTION_YIELD'),
 @('token.GetRetryCount() >= 2','false','RETRIES_REMAIN_BOUNDED_AND_FENCED')
)){
 if(!$source.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$yield=Body $source 'TryYieldPresetSeat'
$rearm=$yield.Insert(1,"`n state.Begin(nowMs, nowMs + 125000, 1, 0, trip.GetAbsoluteDeadlineMs());")
if('FIXED_DEADLINE_NO_HIDDEN_MUTATION' -notin @(Rules ($source.Replace($yield,$rearm)))){throw 'Deadline extension mutation survived'}
$controls++
$tick=Body $source 'TickPreset'
$transition=[regex]::Match($tick,'(?s)\s*if \(access && \(access.IsGettingIn\(\) \|\| access.IsGettingOut\(\) \|\| access.IsSwitchingSeatsAnim\(\)\)\)\s*\{\s*memberTransition = true;\s*busyTokens.Insert\(token\);\s*\}').Value
if(!$transition){throw 'Assigned transition mutation target absent'}
$lateTransition=$tick.Replace($transition,'')
$lateTransition=$lateTransition.Replace('if (!token.WasPresetQueued())',$transition+"`n if (!token.WasPresetQueued())")
if('ALL_ASSIGNED_TRANSITIONS_BEFORE_SETTLED' -notin @(Rules ($source.Replace($tick,$lateTransition)))){throw 'Late transition observation mutation survived'}
$controls++
"PASS Vehicle preset queue: at most two proven independent entries, complete busy-action/transition union, bounded owner-safe yielding, ready first attempts before retries, original deadline with progress receipt and exact identity/seat fences; controls=$controls"
