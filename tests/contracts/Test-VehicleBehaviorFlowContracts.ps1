param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$flow=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleBehaviorFlow.c'))
$token=[IO.File]::ReadAllText((Join-Path $root 'State/Vehicles/AICF_VehicleBehaviorAction.c'))
$state=[IO.File]::ReadAllText((Join-Path $root 'State/Vehicles/AICF_VehicleBehaviorState.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$f,[string]$t,[string]$s){
 $current=Body $f 'IsCurrent';$inspect=Body $f 'InspectTransit';$begin=Body $f 'Begin';$tick=Body $f 'Tick'
 $restore=Body $f 'RestoreCrewAssignments';$clear=Body $f 'WaitForCrewClear';$free=Body $f 'FindFreeSurvivor'
 $captured=Body $f 'FindCapturedPosition'
 $deploy=Body $f 'DeployActors';$exit=Body $f 'EnsureExit';$entry=Body $f 'EnsureEntry';$issue=Body $f 'IssueEntry';$poll=Body $f 'PollReboarding'
 $cancel=Body $f 'CancelEntries';$stop=Body $f 'Stop';$distant=Body $f 'HasDistantOrder'
 $owner=Body $t 'ResolveOwner';$owned=Body $t 'OwnsAction';$settled=Body $t 'IsSettled';$blocked=Body $t 'EntryBlocked';$retire=Body $t 'Retire'
 if(!$owner -or !$restore -or !$tick -or !$entry -or !$poll){'METHODS';return}
 if($current -notmatch '!Replication.IsServer\(\)' -or $current -notmatch '!trip.IsVehiclePresetCurrent\(\)' -or
  $current -notmatch 'lease.MatchesTripIdentity\(trip.GetFactionKey\(\), trip.GetSlotId\(\), trip.GetGroupGeneration\(\), trip.GetTripGeneration\(\)\)' -or
  $current -notmatch 'lease.MatchesEntityIdentity\(vehicle, vehicle.GetID\(\), AICF_VehicleBehaviorBinding.ComponentRpl\(vehicle\)\)'){'EXACT_LIVE_TRIP_ASSET'}
 if($begin -notmatch 'state.m_iDeploymentDeadline = System.GetTickCount\(\) \+ DEPLOYMENT_TIMEOUT_MS' -or
  $tick -notmatch 'System.GetTickCount\(\) >= state.m_iDeploymentDeadline' -or $tick -notmatch 'TACTICAL_DISEMBARK_TIMEOUT' -or
  $tick -match 'm_iDeploymentDeadline\s*=' -or $f -notmatch 'DEPLOYMENT_TIMEOUT_MS = 40000'){'FIXED_PHYSICAL_EGRESS_DEADLINE'}
 if($begin -notmatch 'preset.m_iBehavior == AICF_EVehicleBehavior.COMBAT && IsCrewSeat\(seats\[index\]\)' -or
  $begin.IndexOf('AICF_VehicleBehaviorPolicy.Update') -lt $begin.IndexOf('state.m_aDeployed.Insert') -or
  $deploy -notmatch 'EnsureExit\(trip, actor, access.GetCompartment\(\)\)' -or
  $exit -notmatch 'new SCR_AIGetOutVehicle' -or $exit -notmatch 'PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY' -or
  $deploy -notmatch 'TryExit\(trip, actor, "TACTICAL_DEPLOYMENT"\)' -or
  $deploy -notmatch '!CompartmentAccessComponent.GetVehicleIn\(actor\)'){'SCOPED_CARGO_OR_ALL_DEPLOYMENT_WITH_NATIVE_STOP'}
 if($restore -notmatch 'for \(int pass; pass < 2; pass\+\+\)' -or
  $restore -notmatch '\(pass == 0\) != \(seat.GetType\(\) == ECompartmentType.PILOT\)' -or
  $restore -notmatch 'replacement < 0 && seat.GetType\(\) == ECompartmentType.PILOT' -or
  $restore -notmatch 'if \(seat.GetType\(\) == ECompartmentType.PILOT\) return false' -or
  $restore -notmatch 'trip.ReassignVehiclePresetSeat\(index, -1\)' -or
  $inspect -notmatch 'seat.GetType\(\) != ECompartmentType.PILOT && trip.GetVehiclePreset\(\).m_aMembers\[index\] < 0' -or
  $free -notmatch 'if \(!allowTurret \|\| seats\[seatIndex\].GetType\(\) == ECompartmentType.PILOT\) continue'){'PILOT_PRIORITY_OPTIONAL_TURRET_NO_RESTORE_LOOP'}
 if($restore -notmatch 'IsFit\(occupant\) && AICF_VehicleShortRecovery.IsCurrentMember\(trip, occupant\)' -or
  $restore -notmatch 'int actualPosition = FindCapturedPosition\(trip, occupant\)' -or
  $restore -notmatch 'if \(actualPosition < 0\)' -or $restore -notmatch '!trip.ReassignVehiclePresetSeat\(index, actualPosition\)' -or
  $restore.IndexOf('int actualPosition') -gt $restore.IndexOf('IEntity assigned') -or
  $captured -notmatch '!AICF_VehicleShortRecovery.IsCurrentMember\(trip, actor\)' -or
  $captured -notmatch 'trip.VehiclePresetMember\(position\) == actor && slot.GetRosterMember\(position\) == actor'){'ADOPT_ONLY_EXACT_FIT_NATIVE_CREW'}
 if($restore -notmatch 'if \(seat.GetOccupant\(\)\)\s*\{\s*AICF_VehicleShortRecovery.TryClearIncapacitatedSeat\(trip, seat, "CREW_REPLACEMENT"\);\s*return WaitForCrewClear\(trip, reason\)' -or
  $restore -notmatch 'TryExit\(trip, actor, "CREW_ROLE_CHANGE"\);\s*return WaitForCrewClear\(trip, reason\)' -or
  $clear -notmatch 'if \(state.m_iCrewRestoreDeadline <= 0\) state.m_iCrewRestoreDeadline =' -or
  $clear -notmatch 'System.GetTickCount\(\) >= state.m_iCrewRestoreDeadline' -or
  $tick -notmatch 'if \(failure == "CREW_RESTORE_PENDING"\) return AICF_TripOutcome.Wait'){'ACTUAL_EMPTY_AND_OUTSIDE_BEFORE_ROLE_COMMIT'}
 $deferAt=$tick.IndexOf('COMBAT_CREW_RECOVERY_DEFERRED');$restoreAt=$tick.IndexOf('RestoreCrewAssignments');$transportAt=$tick.IndexOf('TRANSPORT_FOOT_COMBAT')
 if($deferAt -lt 0 -or $restoreAt -lt $deferAt -or $transportAt -lt $restoreAt -or
  $tick -notmatch 'if \(combat && trip.GetVehiclePreset\(\).m_iBehavior == AICF_EVehicleBehavior.COMBAT\)' -or
  $tick -notmatch 'if \(combat && !distantOrder\)' -or $tick -notmatch 'if \(!state.m_bThreatDeployment && !distantOrder\)'){'TRANSPORT_ASSIGN_NOW_COMBAT_ASSIGN_AFTER_CALM'}
 if($tick -notmatch 'm_Config.GetBoardingTimeoutMs\(\) \* Math.Max\(3, livingAssigned\)' -or
  $poll -notmatch 'System.GetTickCount\(\) >= state.m_iReboardDeadline' -or
  $poll -notmatch 'COMBAT \|\| !HasDistantOrder\(trip\)' -or
  $poll -notmatch 'state.m_iSettledPolls < 2' -or $poll -notmatch 'access.IsInCompartment\(\)' -or
  $poll -notmatch '!access.IsGettingIn\(\) && !access.IsGettingOut\(\) && !access.IsSwitchingSeatsAnim\(\)' -or
  $poll -notmatch 'CompartmentAccessComponent.GetVehicleIn\(actor\) == trip.GetLease\(\).GetVehicle\(\)'){'BOUNDED_EXACT_REBOARD_NOT_INTENT_ONLY'}
 if($entry -notmatch 'bool blocked = token.EntryBlocked\(\)' -or
  $entry -notmatch 'blocked && token.m_iShortAttempts < 3 && AICF_VehicleShortRecovery.CanSeat\(trip, actor, seat\)' -or
  $entry -notmatch 'blocked && token.m_iRetries < 2 && token.CanRestartEntry\(\)' -or
  $entry -notmatch 'token.m_iRetries < 2' -or $entry -notmatch 'IssueEntry\(trip, token, actor, seat\)' -or
  $issue -notmatch 'seat.IsReserved\(\) && !seat.IsReservedBy\(actor\)' -or
  $issue -notmatch 'SCR_AICompartmentHandling.CompartmentClassToType\(seat.Type\(\)\)' -or
  $blocked -notmatch 'EAIActionState.FAILED' -or $blocked -notmatch 'System.GetTickCount\(\) - m_iLastProgressAt >= 2000'){'IMMEDIATE_NATIVE_FAILURE_SHORT_CORRECTION_NOT_REMOTE_PULL'}
 foreach($guard in @('!Replication.IsServer()','m_Trip.GetLease() != m_Lease',
  'm_Trip.GetGroupGeneration() != m_iGroupGeneration','m_Trip.GetTripGeneration() != m_iTripGeneration',
  'm_Lease.GetLeaseGeneration() != m_iLeaseGeneration','m_Lease.GetVehicleGeneration() != m_iVehicleGeneration',
  'm_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Actor)',
  'm_Actor.GetID() != m_ActorId','AICF_VehicleBehaviorBinding.ComponentRpl(m_Actor) != m_sActorRpl',
  'agent.GetControlledEntity() != m_Actor','agent.GetParentGroup() != m_Group')){
  if(!$owner.Contains($guard)){'IMMUTABLE_ACTION_IDENTITY';break}
 }
 if($owned -notmatch 'action.m_Utility != utility' -or $owned -notmatch 'action.m_Vehicle.m_Value != m_Vehicle' -or
  $owned -notmatch 'return actions.Contains\(action\)' -or
  $retire -notmatch 'm_Entry.m_CompartmentToGetIn.m_Value == m_Seat' -or
  $retire -notmatch 'if \(IsSettled\(\)\) m_Entry.Complete\(\)' -or
  $retire -notmatch 'else if \(!linked && !transitioning\) m_Entry.Fail\(\)' -or
  $retire -notmatch 'else m_Entry.SetRemoveAction\(true\)' -or
  $cancel -notmatch 'token.Retire\(\)' -or $cancel -match '\.Fail\(' -or
  $stop -notmatch 'AICF_VehicleBehaviorPolicy.Unregister\(trip\)'){'ONLY_OWNED_ACTIONS_NEVER_FAIL_MOUNTED_ACTOR'}
 if((ConvertTo-AICFCodeText $retire) -match 'SetReserved\('){'NATIVE_CALLBACK_RESERVATION_ABA'}
 if($poll -notmatch 'if \(combat && trip.GetVehiclePreset\(\).m_iBehavior == AICF_EVehicleBehavior.TRANSPORT && HasDistantOrder\(trip\)\)' -or
  $poll.IndexOf('m_iCombatOverrideIntent = trip.GetAssignment().GetStrategicIntentRevision()') -lt $poll.IndexOf('state.m_iSettledPolls < 2') -or
  $inspect -notmatch 'if \(!combat\) trip.GetBehaviorState\(\).m_iCombatOverrideIntent = -1' -or
  $inspect -notmatch '!AICF_VehicleBehaviorPolicy.HasTransportCombatOverride\(trip\)' -or
  $begin -notmatch 'state.m_iCombatOverrideIntent = -1'){'PHYSICAL_CURRENT_FAR_ORDER_RECEIPT_WITH_CALM_RESET'}
 if((ConvertTo-AICFCodeText ($f+$t+$s)) -match 'SetHealth|SetDamage|Resurrect|SpawnEntity|RequestSpawn|CommitTransition|SetOrigin|SetWorldTransform'){'NO_HEAL_SPAWN_LIFECYCLE_OR_REMOTE_TELEPORT'}
}
$failures=@(Rules $flow $token $state)
if($failures.Count){throw "Vehicle behavior flow: $failures"}
$controls=1
foreach($mutation in @(
 @('f','!trip.IsVehiclePresetCurrent()','false','EXACT_LIVE_TRIP_ASSET'),
 @('f','System.GetTickCount() >= state.m_iDeploymentDeadline','false','FIXED_PHYSICAL_EGRESS_DEADLINE'),
 @('f','COMBAT && IsCrewSeat(seats[index])','TRANSPORT && IsCrewSeat(seats[index])','SCOPED_CARGO_OR_ALL_DEPLOYMENT_WITH_NATIVE_STOP'),
 @('f','PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY','PRIORITY_BEHAVIOR_GET_IN_VEHICLE','SCOPED_CARGO_OR_ALL_DEPLOYMENT_WITH_NATIVE_STOP'),
 @('f','trip.ReassignVehiclePresetSeat(index, -1)','false','PILOT_PRIORITY_OPTIONAL_TURRET_NO_RESTORE_LOOP'),
 @('f','seat.GetType() != ECompartmentType.PILOT && trip.GetVehiclePreset().m_aMembers[index] < 0','false','PILOT_PRIORITY_OPTIONAL_TURRET_NO_RESTORE_LOOP'),
 @('f','IsFit(occupant) && AICF_VehicleShortRecovery.IsCurrentMember(trip, occupant)','IsFit(occupant)','ADOPT_ONLY_EXACT_FIT_NATIVE_CREW'),
 @('f','!trip.ReassignVehiclePresetSeat(index, actualPosition)','false','ADOPT_ONLY_EXACT_FIT_NATIVE_CREW'),
 @('f','trip.VehiclePresetMember(position) == actor && slot.GetRosterMember(position) == actor','slot.GetRosterMember(position) == actor','ADOPT_ONLY_EXACT_FIT_NATIVE_CREW'),
 @('f','return WaitForCrewClear(trip, reason);','return true;','ACTUAL_EMPTY_AND_OUTSIDE_BEFORE_ROLE_COMMIT'),
 @('f','if (state.m_iCrewRestoreDeadline <= 0)','if (true)','ACTUAL_EMPTY_AND_OUTSIDE_BEFORE_ROLE_COMMIT'),
 @('f','if (combat && !distantOrder)','if (combat)','TRANSPORT_ASSIGN_NOW_COMBAT_ASSIGN_AFTER_CALM'),
 @('f','m_Config.GetBoardingTimeoutMs() * Math.Max(3, livingAssigned)','m_Config.GetBoardingTimeoutMs() * 99','BOUNDED_EXACT_REBOARD_NOT_INTENT_ONLY'),
 @('f','state.m_iSettledPolls < 2','state.m_iSettledPolls < 0','BOUNDED_EXACT_REBOARD_NOT_INTENT_ONLY'),
 @('f','AICF_VehicleShortRecovery.CanSeat(trip, actor, seat)','true','IMMEDIATE_NATIVE_FAILURE_SHORT_CORRECTION_NOT_REMOTE_PULL'),
 @('t','m_Lease.GetVehicleGeneration() != m_iVehicleGeneration','false','IMMUTABLE_ACTION_IDENTITY'),
 @('t','m_Actor.GetID() != m_ActorId','false','IMMUTABLE_ACTION_IDENTITY'),
 @('t','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(m_Actor)','false','IMMUTABLE_ACTION_IDENTITY'),
 @('t','return actions.Contains(action);','return true;','ONLY_OWNED_ACTIONS_NEVER_FAIL_MOUNTED_ACTOR'),
 @('t','if (IsSettled()) m_Entry.Complete();','if (IsSettled()) m_Entry.Fail();','ONLY_OWNED_ACTIONS_NEVER_FAIL_MOUNTED_ACTOR'),
 @('t','else if (!linked && !transitioning) m_Entry.Fail();','else m_Entry.Fail();','ONLY_OWNED_ACTIONS_NEVER_FAIL_MOUNTED_ACTOR'),
 @('t','m_Entry = null;','m_Entry = null; m_Seat.SetReserved(null);','NATIVE_CALLBACK_RESERVATION_ABA'),
 @('f','if (!combat) trip.GetBehaviorState().m_iCombatOverrideIntent = -1;','','PHYSICAL_CURRENT_FAR_ORDER_RECEIPT_WITH_CALM_RESET')
)){
 $f=$flow;$t=$token;$s=$state
 $original=@{f=$f;t=$t;s=$s}[$mutation[0]]
 if(!$original.Contains($mutation[1])){throw "Mutation target absent: $($mutation[3])"}
 $changed=$original.Replace($mutation[1],$mutation[2])
 switch($mutation[0]){'f'{$f=$changed}'t'{$t=$changed}'s'{$s=$changed}}
 if($mutation[3] -notin @(Rules $f $t $s)){throw "Mutation survived: $($mutation[3])"}
 $controls++
}
# Выполняем фактическое условие N: нового intent недостаточно без дальности.
$body=Body $flow 'HasDistantOrder'
$expression=[regex]::Match($body,'(?s)return\s+(.*?);').Groups[1].Value.
 Replace('trip.GetAssignment().GetStrategicIntentRevision() != trip.GetBehaviorState().m_iDeploymentIntent','$changed').
 Replace('vector.DistanceXZ(leader.GetOrigin(), trip.GetAssignment().GetTargetPosition())','$distance').
 Replace('trip.GetVehiclePreset().m_iReturnDistance','$threshold').Replace('leader','$leader').Replace('&&','-and').Replace('>','-gt')
$predicate=[scriptblock]::Create('param($leader,$changed,$distance,$threshold) '+$expression)
foreach($case in @(@($true,$true,301,300,$true),@($true,$true,300,300,$false),@($true,$true,299,300,$false),@($true,$false,999,300,$false),@($false,$true,999,300,$false))){
 if([bool](& $predicate $case[0] $case[1] $case[2] $case[3]) -ne $case[4]){throw "Distant order predicate failed: $case"}
 $controls++
}
"PASS Vehicle behavior flow: bounded physical deployment/reboard, Pilot priority, actual cleared role, scoped native actions and N boundary; controls=$controls"
