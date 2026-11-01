param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$policy=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_TransportCombatHold.c'))
$transit=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'))
$state=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/State/Vehicles/AICF_VehiclePhaseStates.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$p,[string]$t,[string]$s){
 $resolve=Body $p 'ResolveCurrentCombat';$actors=Body $p 'HasOnlyCurrentAIActors';$current=Body $p 'IsCurrentCombat'
 $tick=Body $t 'Tick';$observe=Body $t 'ObserveNativeCombat';$abort=Body $t 'AbortOwnedActions'
 $begin=Body $s 'BeginNativeCombatHold';$clear=Body $s 'ClearNativeCombatHold';$report=Body $s 'MarkNativeCombatReportDue'
 if(!$resolve -or !$actors -or !$tick -or !$observe -or !$begin -or !$clear){'MISSING_METHOD';return}
 if($current -notmatch 'return ResolveCurrentCombat\(trip\) != null' -or $resolve -notmatch '!Replication.IsServer\(\)' -or $resolve -notmatch '!trip.IsValid\(\) \|\| trip.IsTerminal\(\)' -or $resolve -notmatch 'trip.GetPhase\(\) != AICF_ETransportTripPhase.TRANSIT'){'READONLY_CURRENT_TRANSIT_PROJECTION'}
 if($resolve -notmatch 'trip.IsDeadlineReached\(System.GetTickCount\(\)\)' -or $tick.IndexOf('trip.IsDeadlineReached(nowMs)') -lt 0 -or $tick.IndexOf('trip.IsDeadlineReached(nowMs)') -gt $tick.IndexOf('ObserveNativeCombat')){'ABSOLUTE_DEADLINE_PRECEDES_HOLD'}
 if($resolve -notmatch '!lease.HasPhysicalAsset\(\)' -or $resolve -notmatch 'lease.MatchesTripIdentity\(trip.GetFactionKey\(\), trip.GetSlotId\(\),\s*trip.GetGroupGeneration\(\), trip.GetTripGeneration\(\)\)' -or $resolve -notmatch 'lease.MatchesEntityIdentity\(vehicle, vehicle.GetID\(\), rpl.Id\(\).ToString\(\)\)'){'EXACT_CURRENT_LEASE'}
 if($resolve -notmatch 'IsAuthoritativeReplicatedEntity\(group\)' -or $resolve -notmatch 'IsAuthoritativeReplicatedEntity\(vehicle\)' -or $resolve -notmatch '!trip.GetVehiclePreset\(\) \|\| !trip.IsVehiclePresetCurrent\(\)' -or $actors -match 'return !preset \|\|'){'AUTHORITY_AND_MANUAL_ROSTER_CURRENT'}
 if($resolve -notmatch '!movement \|\| movement.GetCrewRecoveryToken\(\) \|\| movement.GetNativeGate\(\)'){'EXISTING_ASYNC_OWNERS_ADVANCE_NORMALLY'}
 if($resolve -notmatch 'SCR_AIVehicleCombatActivity.Cast\(utility.GetCurrentAction\(\)\)' -or $resolve -notmatch 'utility.m_Owner != group' -or $resolve -notmatch 'activity.m_Utility != utility' -or $resolve -notmatch 'utility.GetActions\(actions\)' -or $resolve -notmatch '!actions.Contains\(activity\)'){'EXACT_SELECTED_OWNED_ACTIVITY'}
 if($resolve -notmatch 'activity.GetRemoveAction\(\)' -or $resolve -notmatch 'activity.GetActionIsSuspended\(\)' -or $resolve -notmatch 'actionState != EAIActionState.RUNNING && actionState != EAIActionState.EVALUATED'){'NATIVE_SELECTED_EVALUATED_WITHOUT_TERMINAL_OR_SUSPENDED'}
 if($resolve -notmatch 'utility.m_VehicleMgr.FindVehicle\(vehicle\)' -or $resolve -notmatch 'registered.GetEntity\(\) != vehicle' -or $resolve -notmatch 'registered.GetVehicleUsageComponent\(\) != usage' -or $resolve -notmatch 'registered.IsStatic\(\)'){'EXACT_REGISTERED_NONSTATIC_VEHICLE'}
 if($resolve -notmatch 'usage.GetDamageState\(\) == EDamageState.DESTROYED' -or $resolve -notmatch 'VehicleIsOnFire\(vehicle\)' -or $resolve -notmatch '!SCR_AIVehicleUsability.VehicleCanMove\(vehicle\)' -or $resolve -notmatch 'transform\[1\]\[1\] < 0.25' -or $tick.IndexOf('InspectRouteAssetFailure') -gt $tick.IndexOf('ObserveNativeCombat')){'DAMAGE_VETO'}
 if($actors -notmatch 'agent.GetParentGroup\(\) != group' -or $actors -notmatch '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity\(member\)' -or $actors -notmatch 'linkedVehicle && linkedVehicle != vehicle' -or $actors -notmatch 'members.IsEmpty\(\)'){'CURRENT_ALIVE_AI_MEMBERS_ONLY'}
 if($actors -notmatch '!members.Contains\(occupant\)' -or $actors -notmatch '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity\(occupant\)' -or $actors -notmatch 'seat.GetVehicle\(\) != vehicle'){'FOREIGN_OR_PLAYER_OCCUPANT_VETO'}
 if($actors -notmatch '!preset \|\| seats.IsEmpty\(\) \|\| !preset.MatchesSeats\(seats\)' -or $actors -notmatch 'trip.VehiclePresetMember\(position\)' -or $actors -notmatch '!members.Contains\(expected\) \|\| \(occupant && occupant != expected\)' -or $actors -notmatch 'assigned == members.Count\(\)'){'EXACT_PRESET_MEMBERS_AND_SEATS'}
 $hold=$tick.IndexOf('ObserveNativeCombat')
 if($hold -lt 0 -or $hold -gt $tick.IndexOf('ObserveNativeGate') -or $hold -gt $tick.IndexOf('ProcessMandatoryCrew') -or $hold -gt $tick.IndexOf('ObserveProgressAndRecover') -or $observe -notmatch 'return AICF_TripOutcome.Wait\("NATIVE_VEHICLE_COMBAT", causationId\)'){'TYPED_WAIT_BEFORE_RECOVERY'}
 $observation=ConvertTo-AICFCodeText ($p+$observe+$begin+$clear+$report)
 if($observation -match 'SetOrigin|Teleport|\.GetInVehicle\(|\.GetOutVehicle\(|\.Fail\(|\.Complete\(|\.Cancel|AddAction|SetCurrentAction|SetCombatMode|SetAction|SetPresetDestinationWaiting|CommitTransition|\.Begin\(|ObserveRouteProgress|ObservePhysicalMotion|ClearMobilityRecoverySettlementDeferrals|m_i(?:AbsoluteDeadline|LastRouteProgressAt|LastPhysicalMotionAt)Ms\s*[+]?='){'NO_ACTION_DEADLINE_OR_PROGRESS_MUTATION'}
 if($observe -notmatch 'activity != state.GetNativeCombatActivity\(\)' -or $observe -notmatch 'state.ClearNativeCombatHold\(\)' -or $observe -notmatch 'TRANSPORT_NATIVE_COMBAT_STARTED' -or $observe -notmatch 'TRANSPORT_NATIVE_COMBAT_ENDED' -or $abort -notmatch 'state.ClearNativeCombatHold\(\)' -or $clear -notmatch 'm_NativeCombatActivity = null' -or $report -notmatch 'nowMs - m_iNativeCombatReportAtMs < intervalMs'){'OBSERVATION_LIFECYCLE_AND_BOUNDED_LOGGING'}
}
$failures=@(Rules $policy $transit $state)
if($failures.Count){throw "Transport combat hold: $failures"}
$controls=1
foreach($mutation in @(
 @('p','!Replication.IsServer()','false','READONLY_CURRENT_TRANSIT_PROJECTION'),
 @('p','trip.IsDeadlineReached(System.GetTickCount())','false','ABSOLUTE_DEADLINE_PRECEDES_HOLD'),
 @('p','lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())','true','EXACT_CURRENT_LEASE'),
 @('p','!trip.GetVehiclePreset() || !trip.IsVehiclePresetCurrent()','false','AUTHORITY_AND_MANUAL_ROSTER_CURRENT'),
 @('p','movement.GetCrewRecoveryToken()','false','EXISTING_ASYNC_OWNERS_ADVANCE_NORMALLY'),
 @('p','movement.GetNativeGate()','false','EXISTING_ASYNC_OWNERS_ADVANCE_NORMALLY'),
 @('p','SCR_AIVehicleCombatActivity.Cast(utility.GetCurrentAction())','SCR_AIVehicleCombatActivity.Cast(utility.GetExecutedAction())','EXACT_SELECTED_OWNED_ACTIVITY'),
 @('p','!actions.Contains(activity)','false','EXACT_SELECTED_OWNED_ACTIVITY'),
 @('p','activity.GetRemoveAction()','false','NATIVE_SELECTED_EVALUATED_WITHOUT_TERMINAL_OR_SUSPENDED'),
 @('p','activity.GetActionIsSuspended()','false','NATIVE_SELECTED_EVALUATED_WITHOUT_TERMINAL_OR_SUSPENDED'),
 @('p','actionState != EAIActionState.RUNNING && actionState != EAIActionState.EVALUATED','actionState == EAIActionState.FAILED','NATIVE_SELECTED_EVALUATED_WITHOUT_TERMINAL_OR_SUSPENDED'),
 @('p','registered.GetVehicleUsageComponent() != usage','false','EXACT_REGISTERED_NONSTATIC_VEHICLE'),
 @('p','!SCR_AIVehicleUsability.VehicleCanMove(vehicle)','false','DAMAGE_VETO'),
 @('p','agent.GetParentGroup() != group','false','CURRENT_ALIVE_AI_MEMBERS_ONLY'),
 @('p','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(occupant)','false','FOREIGN_OR_PLAYER_OCCUPANT_VETO'),
 @('p','!members.Contains(occupant)','false','FOREIGN_OR_PLAYER_OCCUPANT_VETO'),
 @('p','assigned == members.Count()','true','EXACT_PRESET_MEMBERS_AND_SEATS'),
 @('t','return AICF_TripOutcome.Wait("NATIVE_VEHICLE_COMBAT", causationId);','return null;','TYPED_WAIT_BEFORE_RECOVERY'),
 @('s','m_NativeCombatActivity = null;','m_NativeCombatActivity = m_NativeCombatActivity;','OBSERVATION_LIFECYCLE_AND_BOUNDED_LOGGING')
)){
 $p=$policy;$t=$transit;$s=$state
 $input=@{p=$p;t=$t;s=$s}[$mutation[0]]
 if(!$input.Contains($mutation[1])){throw "Mutation target absent: $($mutation[3])"}
 $changed=$input.Replace($mutation[1],$mutation[2])
 switch($mutation[0]){'p'{$p=$changed}'t'{$t=$changed}'s'{$s=$changed}}
 if($mutation[3] -notin @(Rules $p $t $s)){throw "Mutation survived: $($mutation[3])"}
 $controls++
}
$begin=Body $state 'BeginNativeCombatHold'
foreach($mutation in @('m_iLastPhysicalMotionAtMs = nowMs;','m_iAbsoluteDeadlineMs += 60000;','activity.Fail();')){
 $changed=$begin.Insert(1,"`n $mutation")
 if('NO_ACTION_DEADLINE_OR_PROGRESS_MUTATION' -notin @(Rules $policy $transit ($state.Replace($begin,$changed)))){throw "Hold mutation survived: $mutation"}
 $controls++
}
$tick=Body $transit 'Tick'
$hold=[regex]::Match($tick,'(?s)\s*AICF_TripOutcome combatOutcome = ObserveNativeCombat\(trip, causationId, nowMs\);\s*if \(combatOutcome\)\s*return combatOutcome;').Value
if(!$hold){throw 'Hold ordering mutation target absent'}
$late=$tick.Replace($hold,'').Replace('AICF_TripOutcome crewOutcome = ProcessMandatoryCrew',$hold+"`n AICF_TripOutcome crewOutcome = ProcessMandatoryCrew")
if('TYPED_WAIT_BEFORE_RECOVERY' -notin @(Rules $policy ($transit.Replace($tick,$late)) $state)){throw 'Late hold mutation survived'}
$controls++
"PASS Transport combat hold: exact selected live native activity/current lease, damage/roster/player vetoes, typed bounded transit wait, no action/deadline/progress mutation; controls=$controls"
