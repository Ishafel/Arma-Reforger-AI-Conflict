param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$policy=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehiclePresetNativeSeatPolicy.c'))
$reaction=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehiclePresetNativeSeatReaction.c'))
$transit=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
function Rules([string]$p,[string]$r,[string]$t){
 $requestAt=$p.IndexOf('class AICF_VehiclePresetNativeSeatRequest')
 $policyAt=$p.IndexOf('class AICF_VehiclePresetNativeSeatPolicy')
 if($requestAt -lt 0 -or $policyAt -lt 0){'CLASSES';return}
 $binding=$p.Substring(0,$requestAt);$requestSource=$p.Substring($requestAt,$policyAt-$requestAt)
 $capture=Body $binding 'Capture';$current=Body $binding 'IsCurrent';$find=Body $binding 'FindSeat'
 $request=Body $requestSource 'IsCurrentContext';$requestCurrent=Body $requestSource 'IsCurrent';$rollback=Body $requestSource 'RollbackReservation'
 $prepare=Body $p 'Prepare';$activity=Body $p 'IsCurrentActivity';$selected=Body $p 'IsSelectedActivity';$other=Body $p 'IsGenuineOtherActivity';$duplicate=Body $p 'HasOwnedExactAction'
 $register=Body $p 'Register';$unregister=Body $p 'Unregister';$prune=Body $p 'Prune'
 $perform=Body $r 'PerformReaction';$tick=Body $t 'Tick';$abort=Body $t 'AbortOwnedActions'
 if(!$capture -or !$current -or !$request -or !$prepare -or !$activity -or !$perform){'METHODS';return}
 if((ConvertTo-AICFCodeText $binding) -match '\bref\s+(AICF_TransportTrip|SCR_AIGroup|AICF_VehicleLease|Vehicle|IEntity)' -or
  $binding -match 'array<ref IEntity>' -or $prune -notmatch '!s_aBindings\[index\].m_Trip \|\| s_aBindings\[index\].m_Trip.IsTerminal\(\)' -or
  $prune -notmatch '!s_aBindings\[index\].m_Group \|\| !s_aBindings\[index\].m_Lease \|\| !s_aBindings\[index\].m_Vehicle' -or
  $register -notmatch 'if \(current.m_Trip == trip\) return' -or $unregister -notmatch 's_aBindings\[index\].m_Trip == trip'){'WEAK_LIFETIME_NO_RECAPTURE'}
 $registerAt=$tick.IndexOf('AICF_VehiclePresetNativeSeatPolicy.Register(trip)')
 if($registerAt -lt 0 -or $registerAt -lt $tick.IndexOf('if (!ValidateContext(') -or
  $registerAt -gt $tick.IndexOf('ObserveNativeCombat(') -or $registerAt -gt $tick.IndexOf('return PrepareRoute(') -or
  $abort -notmatch '^\{\s*AICF_VehiclePresetNativeSeatPolicy.Unregister\(trip\);'){ 'TRANSIT_LIFECYCLE_BEFORE_MOUNTED_WAIT' }
 foreach($guard in @('!Replication.IsServer()','!m_Trip.IsValid()','m_Trip.IsTerminal()',
  'm_Trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','m_Trip.IsDeadlineReached(System.GetTickCount())',
  'm_Trip.GetOperationId() != m_sOperation','m_Trip.GetGroupGeneration() != m_iGroupGeneration',
  'm_Trip.GetTripGeneration() != m_iTripGeneration','m_Trip.GetVehiclePresetRevision() != m_iPresetRevision',
  '!m_Trip.IsVehiclePresetCurrent()','!slot.IsCombatReady()',
  'slot.GetStrategicAssignmentRevision() != assignment.GetAssignmentRevision()',
  'slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()')){
  if(!$current.Contains($guard)){'CURRENT_MANUAL_DEADLINE_REVISIONS';break}
 }
 foreach($guard in @('m_Trip.GetLease() != m_Lease','!m_Lease.HasPhysicalAsset()',
  'm_Lease.GetLeaseGeneration() != m_iLeaseGeneration','m_Lease.GetVehicleGeneration() != m_iVehicleGeneration',
  'm_Lease.MatchesTripIdentity(m_Trip.GetFactionKey(), m_Trip.GetSlotId(), m_iGroupGeneration, m_iTripGeneration)',
  'm_Group.GetID() != m_GroupId','ComponentRpl(m_Group) != m_sGroupRpl',
  'm_Vehicle.GetID() != m_VehicleId','ComponentRpl(m_Vehicle) != m_sVehicleRpl',
  'm_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)')){
  if(!$current.Contains($guard)){'IMMUTABLE_GROUP_VEHICLE_LEASE';break}
 }
 if($binding -match 'Replication.FindItemId' -or (Body $binding 'ComponentRpl') -notmatch 'return rpl.Id\(\).ToString\(\)' -or
  $capture -notmatch 'binding.m_aMemberIds.Insert\(member.GetID\(\)\)' -or $capture -notmatch 'binding.m_aMemberRpls.Insert\(ComponentRpl\(member\)\)'){'CANONICAL_COMPONENT_RPL'}
 foreach($guard in @('!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Group)',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(m_Vehicle)',
  '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)','!SCR_AIDamageHandling.IsConscious(member)',
  'member.GetID() != m_aMemberIds[index]','ComponentRpl(member) != m_aMemberRpls[index]',
  'm_Trip.VehiclePresetMember(m_aPositions[index]) != member',
  'm_Trip.GetVehiclePresetSlot().GetRosterMember(m_aPositions[index]) != member',
  'agent.GetControlledEntity() != member','agent.GetParentGroup() != m_Group',
  'linkedVehicle && linkedVehicle != m_Vehicle')){
  if(!$current.Contains($guard)){'EXACT_ALIVE_AI_ACTORS';break}
 }
 if($current -notmatch 'usage.GetDamageState\(\) == EDamageState.DESTROYED' -or $current -notmatch 'VehicleIsOnFire\(m_Vehicle\)' -or
  $current -notmatch '!SCR_AIVehicleUsability.VehicleCanMove\(m_Vehicle\)' -or $current -notmatch 'transform\[1\]\[1\] < 0.25' -or
  $current -notmatch 'position < 0 \|\| m_Trip.VehiclePresetMember\(position\) != occupant'){'DAMAGE_AND_FOREIGN_OCCUPANT_VETO'}
 if($find -notmatch 'if \(index < 0 \|\| !IsCurrent\(\)\)' -or $find -notmatch '!preset.MatchesSeats\(seats\)' -or
  $find -notmatch 'AICF_VehiclePreset.SeatKey\(seats\[seatIndex\]\) == m_aSeatKeys\[index\]' -or
  $find -notmatch 'preset.m_aMembers\[seatIndex\] == m_aPositions\[index\]'){'EXACT_SAVED_SEAT_SCHEMA'}
 foreach($guard in @('utility.m_Owner != binding.m_Group','utility.GetCurrentAction() != activity',
  'activity.m_Utility != utility','activity.GetRemoveAction()','activity.GetActionIsSuspended()',
  'state != EAIActionState.RUNNING && state != EAIActionState.EVALUATED','actions.Contains(activity)',
  'utility.m_VehicleMgr.FindVehicle(binding.m_Vehicle)','getIn.m_Vehicle.m_Value == binding.m_Vehicle',
  'movement.IsRouteWaypointBound()','move.m_RelatedWaypoint == movement.GetRouteWaypoint()',
  'binding.m_Group.GetCurrentWaypoint() == movement.GetRouteWaypoint()')){
  if(!($activity+$selected).Contains($guard)){'EXACT_SELECTED_GETIN_OR_OWNED_MOVE';break}
 }
 if($activity -notmatch 'if \(!IsSelectedActivity\(binding, activity\)\) return false'){'EXACT_SELECTED_GETIN_OR_OWNED_MOVE'}
 if($request -notmatch 'AICF_VehiclePresetNativeSeatPolicy.Contains\(m_Binding\) && m_Binding.IsCurrent\(\)' -or
  $request -notmatch 'm_Utility.m_OwnerEntity == m_Member' -or $request -notmatch 'm_Agent.GetControlledEntity\(\) == m_Member' -or
  $request -notmatch 'SCR_AIUtilityComponent.Cast\(m_Agent.FindComponent\(SCR_AIUtilityComponent\)\) == m_Utility' -or
  $request -notmatch 'm_Seat && m_Seat.GetVehicle\(\) == m_Binding.m_Vehicle' -or
  $request -notmatch 'm_Binding.FindSeat\(m_Member\) == m_Seat' -or
  $request -notmatch 'GetAssignmentRevision\(\) == m_iAssignmentRevision' -or $request -notmatch 'GetRouteGeneration\(\) == m_iRouteGeneration' -or
  $requestCurrent -notmatch 'return IsCurrentContext\(\) && AICF_VehiclePresetNativeSeatPolicy.IsCurrentActivity\(m_Binding, m_Activity\)'){'SYNCHRONOUS_REPEATED_REQUEST_FENCE'}
 if($prepare -notmatch 'if \(!binding\) return null' -or
  $prepare -notmatch 'bool currentGroup = candidate.m_Group && agent && agent.GetParentGroup\(\) == candidate.m_Group' -or
  $prepare -notmatch 'bool originalActorMessage = candidate.m_Group && candidate.m_aMembers.Find\(member\) >= 0;' -or
  $prepare -notmatch 'if \(binding\) ambiguous = true' -or
  $prepare -notmatch 'if \(ambiguous \|\| message.m_Vehicle != binding.m_Vehicle \|\| !request.IsCurrentContext\(\)' -or
  $prepare -notmatch 'message.GetSender\(\) != binding.m_Group' -or
  $prepare -notmatch 'message.GetReceiver\(\) != agent && message.GetReceiver\(\) != binding.m_Group' -or
  $prepare -notmatch 'message.m_RelatedGroupActivity && message.m_RelatedGroupActivity != activity' -or
  $prepare -notmatch 'request.Log\("CURRENT_MANUAL_CONTEXT_REJECTED"\); return request'){'UNRELATED_NATIVE_UNCHANGED_UNSAFE_MANUAL_HANDLED'}
 if($other -notmatch 'return activity && message.m_RelatedGroupActivity == activity &&\s*!SCR_AIGetInActivity.Cast\(activity\) && !SCR_AIMoveActivity.Cast\(activity\) && IsSelectedActivity\(binding, activity\)' -or
  $prepare -notmatch 'if \(!IsCurrentActivity\(binding, activity\)\)\s*\{\s*if \(IsGenuineOtherActivity\(binding, activity, message\)\) return null;\s*request.Log\("CURRENT_MANUAL_ACTIVITY_REJECTED"\);\s*return request;' -or
  $prepare.IndexOf('if (ambiguous ||') -gt $prepare.IndexOf('if (IsGenuineOtherActivity(') -or
  $prepare -match 'if \(!ambiguous && !SCR_AIGetInActivity.Cast\(activity\)'){'STALE_CONTEXT_REJECTED_BEFORE_NATIVE_FALLBACK'}
 if($prepare -notmatch 'seat.GetOccupant\(\) == member && access && access.IsInCompartment\(\)' -or
  $prepare -notmatch 'access.GetCompartment\(\) == seat' -or $prepare -notmatch 'EXACT_MEMBER_ALREADY_SETTLED' -or
  $prepare -notmatch 'if \(HasOwnedExactAction\(request\)\) \{ request.Log\("EXACT_OWNED_ACTION_ALREADY_PENDING"\); return request; \}' -or
  $duplicate -notmatch 'getIn.m_Utility == request.m_Utility' -or $duplicate -notmatch 'getIn.m_Vehicle.m_Value == request.m_Binding.m_Vehicle' -or
  $duplicate -notmatch 'getIn.m_CompartmentToGetIn.m_Value == request.m_Seat' -or
  $duplicate -notmatch 'EAIActionState.FAILED' -or $duplicate -notmatch 'EAIActionState.COMPLETED'){'SETTLED_AND_OWNED_EXACT_ACTION_NOOP'}
 foreach($guard in @('!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()',
  'CompartmentAccessComponent.GetVehicleIn(member) || seat.GetOccupant() || !seat.IsCompartmentAccessible()',
  'seat.IsGetInLockedFor(member) || (seat.IsReserved() && !seat.IsReservedBy(member))',
  'if (!request.IsCurrent())','request.m_bCreatedReservation = !seat.IsReserved()',
  'if (request.m_bCreatedReservation) seat.SetReserved(member)',
  'if (!request.IsCurrent() || !seat.IsReservedBy(member))')){
  if(!$prepare.Contains($guard)){'SAFE_REPEATED_RESERVATION';break}
 }
 if($prepare -notmatch 'SCR_AIMessage_GetIn.Create\(message.m_Vehicle, message.m_BoardingParams,\s*SCR_AICompartmentHandling.CompartmentClassToType\(seat.Type\(\)\), message.m_bIsWaypointRelated,\s*message.m_fPriorityLevel, message.m_RelatedWaypoint, message.m_RelatedGroupActivity, seat\)' -or
  $prepare -match '(?m)^\s*message\.\w+\s*=' -or $prepare -match 'RequestBroadcast|RequestSendMessage'){'CLONE_EXACT_SLOT_PRESERVE_NATIVE_PAYLOAD'}
 if($rollback -notmatch '!m_bCreatedReservation \|\| !IsCurrent\(\) \|\| !m_Seat \|\| m_Seat.GetOccupant\(\) \|\| !m_Seat.IsReservedBy\(m_Member\)' -or
  $rollback -notmatch 'access.IsInCompartment\(\) \|\| access.IsGettingIn\(\) \|\| access.IsGettingOut\(\) \|\| access.IsSwitchingSeatsAnim\(\)' -or
  $rollback -notmatch 'CompartmentAccessComponent.GetVehicleIn\(m_Member\)' -or $rollback -notmatch 'm_Seat.SetReserved\(null\)'){'ROLLBACK_ONLY_NEW_OWNED_SAFE_RESERVATION'}
 if($r -notmatch '\[BaseContainerProps\(\)\]\s*modded class SCR_AIGoalReaction_GetInVehicle' -or
  ([regex]::Matches($r,'override void PerformReaction')).Count -ne 1 -or $r -match 'notnull SCR_AIGroupUtilityComponent' -or
  $perform -notmatch 'if \(!request\)\s*\{\s*super.PerformReaction\(utility, message\);\s*return;\s*\}' -or
  $perform -notmatch 'if \(!request.m_bDispatch\) return' -or $perform -notmatch 'if \(!request.IsCurrent\(\)\)' -or
  $perform -notmatch 'super.PerformReaction\(utility, request.m_Message\)' -or
  $perform -notmatch 'if \(!AICF_VehiclePresetNativeSeatPolicy.HasOwnedExactAction\(request\)\)\s*\{\s*request.RollbackReservation\(\)'){'NATIVE_ACTOR_ONLY_DISPATCH_AND_ROLLBACK'}
 if((ConvertTo-AICFCodeText ($p+$r)) -match 'SetOrigin|Teleport|\.GetInVehicle\(|\.GetOutVehicle\(|\.Fail\(|\.Complete\(|\.Cancel\(|\.AddAction\(|SetCurrentAction|SetExecutedAction|CommitTransition|CallLater|\.Insert\(|\.Remove\('){
  # Только локальные arrays broker/snapshot, без engine subscriptions/actions.
  $sideEffects=(ConvertTo-AICFCodeText ($p+$r)) -replace '(?:s_aBindings|binding.m_a\w+)\.(?:Insert|Remove)\([^;]*\);',''
  if($sideEffects -match 'SetOrigin|Teleport|\.GetInVehicle\(|\.GetOutVehicle\(|\.Fail\(|\.Complete\(|\.Cancel\(|\.AddAction\(|SetCurrentAction|SetExecutedAction|CommitTransition|CallLater|\.Insert\(|\.Remove\('){'NO_NATIVE_ACTION_OR_EVENT_SIDE_EFFECT'}
 }
 if((ConvertTo-AICFCodeText ($p+$r)) -match 'ObservePhysicalMotion|ObserveRouteProgress|Set.*Deadline|m_i(?:LastPhysicalMotionAt|LastRouteProgressAt|AbsoluteDeadline)Ms\s*[+]?='){'NO_CLOCK_OR_PROGRESS_FORGERY'}
}
$issues=@(Rules $policy $reaction $transit)
if($issues.Count){throw "Native preset seat: $issues"}
$controls=1
foreach($mutation in @(
 @('p','AICF_TransportTrip m_Trip;','ref AICF_TransportTrip m_Trip;','WEAK_LIFETIME_NO_RECAPTURE'),
 @('p','if (current.m_Trip == trip) return;','if (false) return;','WEAK_LIFETIME_NO_RECAPTURE'),
 @('p','!s_aBindings[index].m_Group || !s_aBindings[index].m_Lease || !s_aBindings[index].m_Vehicle','false','WEAK_LIFETIME_NO_RECAPTURE'),
 @('p','m_Trip.IsDeadlineReached(System.GetTickCount())','false','CURRENT_MANUAL_DEADLINE_REVISIONS'),
 @('p','!m_Trip.IsVehiclePresetCurrent()','false','CURRENT_MANUAL_DEADLINE_REVISIONS'),
 @('p','slot.GetStrategicIntentRevision() != assignment.GetStrategicIntentRevision()','false','CURRENT_MANUAL_DEADLINE_REVISIONS'),
 @('p','m_Lease.GetLeaseGeneration() != m_iLeaseGeneration','false','IMMUTABLE_GROUP_VEHICLE_LEASE'),
 @('p','m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleRpl)','true','IMMUTABLE_GROUP_VEHICLE_LEASE'),
 @('p','return rpl.Id().ToString();','return Replication.FindItemId(entity).ToString();','CANONICAL_COMPONENT_RPL'),
 @('p','member.GetID() != m_aMemberIds[index]','false','EXACT_ALIVE_AI_ACTORS'),
 @('p','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)','false','EXACT_ALIVE_AI_ACTORS'),
 @('p','agent.GetParentGroup() != m_Group','false','EXACT_ALIVE_AI_ACTORS'),
 @('p','position < 0 || m_Trip.VehiclePresetMember(position) != occupant','false','DAMAGE_AND_FOREIGN_OCCUPANT_VETO'),
 @('p','!SCR_AIVehicleUsability.VehicleCanMove(m_Vehicle)','false','DAMAGE_AND_FOREIGN_OCCUPANT_VETO'),
 @('p','preset.m_aMembers[seatIndex] == m_aPositions[index]','true','EXACT_SAVED_SEAT_SCHEMA'),
 @('p','utility.GetCurrentAction() != activity','false','EXACT_SELECTED_GETIN_OR_OWNED_MOVE'),
 @('p','activity.GetRemoveAction()','false','EXACT_SELECTED_GETIN_OR_OWNED_MOVE'),
 @('p','activity.GetActionIsSuspended()','false','EXACT_SELECTED_GETIN_OR_OWNED_MOVE'),
 @('p','actions.Contains(activity)','true','EXACT_SELECTED_GETIN_OR_OWNED_MOVE'),
 @('p','move.m_RelatedWaypoint == movement.GetRouteWaypoint()','true','EXACT_SELECTED_GETIN_OR_OWNED_MOVE'),
 @('p','m_Binding.FindSeat(m_Member) == m_Seat','true','SYNCHRONOUS_REPEATED_REQUEST_FENCE'),
 @('p','GetAssignmentRevision() == m_iAssignmentRevision','GetAssignmentRevision() >= 0','SYNCHRONOUS_REPEATED_REQUEST_FENCE'),
 @('p','if (binding) ambiguous = true;','if (false) ambiguous = true;','UNRELATED_NATIVE_UNCHANGED_UNSAFE_MANUAL_HANDLED'),
 @('p','bool currentGroup = candidate.m_Group && agent','bool currentGroup = agent','UNRELATED_NATIVE_UNCHANGED_UNSAFE_MANUAL_HANDLED'),
 @('p','message.GetSender() != binding.m_Group','false','UNRELATED_NATIVE_UNCHANGED_UNSAFE_MANUAL_HANDLED'),
 @('p','message.m_RelatedGroupActivity && message.m_RelatedGroupActivity != activity','false','UNRELATED_NATIVE_UNCHANGED_UNSAFE_MANUAL_HANDLED'),
 @('p','bool originalActorMessage = candidate.m_Group && candidate.m_aMembers.Find(member) >= 0;','bool originalActorMessage = candidate.m_Group && candidate.m_aMembers.Find(member) >= 0 && message.GetSender() == candidate.m_Group;','UNRELATED_NATIVE_UNCHANGED_UNSAFE_MANUAL_HANDLED'),
 @('p','return activity && message.m_RelatedGroupActivity == activity &&','return activity &&','STALE_CONTEXT_REJECTED_BEFORE_NATIVE_FALLBACK'),
 @('p','access.GetCompartment() == seat','true','SETTLED_AND_OWNED_EXACT_ACTION_NOOP'),
 @('p','getIn.m_CompartmentToGetIn.m_Value == request.m_Seat','true','SETTLED_AND_OWNED_EXACT_ACTION_NOOP'),
 @('p','seat.IsGetInLockedFor(member)','false','SAFE_REPEATED_RESERVATION'),
 @('p','(seat.IsReserved() && !seat.IsReservedBy(member))','false','SAFE_REPEATED_RESERVATION'),
 @('p','if (request.m_bCreatedReservation) seat.SetReserved(member);','seat.SetReserved(member);','SAFE_REPEATED_RESERVATION'),
 @('p','message.m_RelatedWaypoint, message.m_RelatedGroupActivity, seat);','message.m_RelatedWaypoint, message.m_RelatedGroupActivity, null);','CLONE_EXACT_SLOT_PRESERVE_NATIVE_PAYLOAD'),
 @('p','!m_bCreatedReservation || !IsCurrent()','!IsCurrent()','ROLLBACK_ONLY_NEW_OWNED_SAFE_RESERVATION'),
 @('p','!m_Seat.IsReservedBy(m_Member)','false','ROLLBACK_ONLY_NEW_OWNED_SAFE_RESERVATION'),
 @('r','[BaseContainerProps()]','','NATIVE_ACTOR_ONLY_DISPATCH_AND_ROLLBACK'),
 @('r','if (!request.m_bDispatch) return;','if (!request.m_bDispatch) { super.PerformReaction(utility, message); return; }','NATIVE_ACTOR_ONLY_DISPATCH_AND_ROLLBACK'),
 @('t','AICF_VehiclePresetNativeSeatPolicy.Unregister(trip);','','TRANSIT_LIFECYCLE_BEFORE_MOUNTED_WAIT')
)){
 $p=$policy;$r=$reaction;$t=$transit
 $original=@{p=$p;r=$r;t=$t}[$mutation[0]]
 if(!$original.Contains($mutation[1])){throw "Mutation target absent: $($mutation[3])"}
 $changed=$original.Replace($mutation[1],$mutation[2])
 switch($mutation[0]){'p'{$p=$changed}'r'{$r=$changed}'t'{$t=$changed}}
 if($mutation[3] -notin @(Rules $p $r $t)){throw "Mutation survived: $($mutation[3])"}
 $controls++
}
# Выполняем production selector состояния: selected EVALUATED допустим, terminal нет.
$activity=Body $policy 'IsSelectedActivity'
$predicate=[regex]::Match($activity,'state != EAIActionState.RUNNING && state != EAIActionState.EVALUATED').Value
$predicate=$predicate.Replace('state','$state').Replace('EAIActionState.RUNNING',"'RUNNING'").Replace('EAIActionState.EVALUATED',"'EVALUATED'").Replace('!=','-ne').Replace('&&','-and')
$reject=[scriptblock]::Create('param($state) '+$predicate)
foreach($case in @(@('RUNNING',$false),@('EVALUATED',$false),@('FAILED',$true),@('COMPLETED',$true),@('INIT',$true))){
 if([bool](& $reject $case[0]) -ne $case[1]){throw 'Native activity state selector changed'}
 $controls++
}
# Исполняем два production predicate вместе: сначала identity/message veto,
# затем только доказанный current other activity может уйти к native super.
$prepare=Body $policy 'Prepare'
$contextPredicate=[regex]::Match($prepare,'(?s)if \((ambiguous \|\|.*?)\)\s*\{ request.Log\("CURRENT_MANUAL_CONTEXT_REJECTED"\)').Groups[1].Value
if(!$contextPredicate){throw 'Context rejection predicate absent'}
$contextPredicate=$contextPredicate.Replace('message.m_Vehicle != binding.m_Vehicle','$wrongVehicle').
 Replace('!request.IsCurrentContext()','!$contextCurrent').Replace('message.GetSender() != binding.m_Group','$wrongSender').
 Replace('(message.GetReceiver() && message.GetReceiver() != agent && message.GetReceiver() != binding.m_Group)','$wrongReceiver').
 Replace('(message.m_RelatedGroupActivity && message.m_RelatedGroupActivity != activity)','$staleRelated').
 Replace('ambiguous','$ambiguous').Replace('||','-or').Replace('!','-not ')
$contextReject=[scriptblock]::Create('param($ambiguous,$wrongVehicle,$contextCurrent,$wrongSender,$wrongReceiver,$staleRelated) '+$contextPredicate)
$other=Body $policy 'IsGenuineOtherActivity'
$nativePredicate=[regex]::Match($other,'(?s)return (.*?);').Groups[1].Value.
 Replace('message.m_RelatedGroupActivity == activity','$relatedCurrent').
 Replace('!SCR_AIGetInActivity.Cast(activity)','$notGetIn').Replace('!SCR_AIMoveActivity.Cast(activity)','$notMove').
 Replace('IsSelectedActivity(binding, activity)','$selected').Replace('activity','$hasActivity').Replace('&&','-and')
$nativeAllow=[scriptblock]::Create('param($hasActivity,$relatedCurrent,$notGetIn,$notMove,$selected) '+$nativePredicate)
# name/current/related/context/sender/receiver/selected/wrongVehicle/ambiguous/expected
$trafficCases=@(
 @('stale-move-combat','Combat','oldMove',$true,$false,$false,$true,$false,$false,'REJECT'),
 @('stale-move-defend','Defend','oldMove',$true,$false,$false,$true,$false,$false,'REJECT'),
 @('stale-move-idle','Idle','oldMove',$true,$false,$false,$true,$false,$false,'REJECT'),
 @('stale-getin-combat','Combat','oldGetIn',$true,$false,$false,$true,$false,$false,'REJECT'),
 @('detached-original-actor','Combat','current',$false,$false,$false,$true,$false,$false,'REJECT'),
 @('possessed-original-actor','Combat','current',$false,$false,$false,$true,$false,$false,'REJECT'),
 @('reassigned-original-actor','Defend','current',$false,$false,$false,$true,$false,$false,'REJECT'),
 @('wrong-sender','Combat','current',$true,$true,$false,$true,$false,$false,'REJECT'),
 @('wrong-receiver','Combat','current',$true,$false,$true,$true,$false,$false,'REJECT'),
 @('ambiguous-trip','Combat','current',$true,$false,$false,$true,$false,$true,'REJECT'),
 @('unknown-none-combat','Combat','NONE',$true,$false,$false,$true,$false,$false,'REJECT'),
 @('genuine-current-combat','Combat','current',$true,$false,$false,$true,$false,$false,'NATIVE'),
 @('genuine-current-defend','Defend','current',$true,$false,$false,$true,$false,$false,'NATIVE'),
 @('suspended-combat','Combat','current',$true,$false,$false,$false,$false,$false,'REJECT'),
 @('removed-combat','Combat','current',$true,$false,$false,$false,$false,$false,'REJECT'),
 @('owned-current-move','Move','NONE',$true,$false,$false,$true,$false,$false,'REMAP'),
 @('owned-current-getin','GetIn','current',$true,$false,$false,$true,$false,$false,'REMAP'),
 @('orphan-move','Move','current',$true,$false,$false,$false,$false,$false,'REJECT'),
 @('wrong-vehicle-combat','Combat','current',$true,$false,$false,$true,$true,$false,'REJECT'),
 @('lease-replaced-combat','Combat','current',$false,$false,$false,$true,$false,$false,'REJECT')
)
foreach($case in $trafficCases){
 $staleRelated=$case[2] -ne 'NONE' -and $case[2] -ne 'current'
 if(& $contextReject $case[8] $case[7] $case[3] $case[4] $case[5] $staleRelated){$actual='REJECT'}
 elseif($case[1] -in @('Move','GetIn') -and $case[6]){$actual='REMAP'}
 elseif(& $nativeAllow ($case[1] -ne 'NONE') ($case[2] -eq 'current') ($case[1] -ne 'GetIn') ($case[1] -ne 'Move') $case[6]){$actual='NATIVE'}
 else{$actual='REJECT'}
 if($actual -ne $case[9]){throw "Native traffic classification $($case[0]): expected=$($case[9]) actual=$actual"}
 $controls++
}
$perform=Body $reaction 'PerformReaction'
foreach($statement in @('utility.AddAction(null);','m_Trip.SetAbsoluteDeadline(0);','ObservePhysicalMotion();')){
 $modified=$reaction.Replace($perform,$perform.Insert(1,$statement))
 if(@(Rules $policy $modified $transit).Count -eq 0){throw "Forbidden side effect survived: $statement"}
 $controls++
}
"PASS Native preset seats: weak trip lifecycle, exact current actor/lease/seat, scoped native GetIn/owned Move, stale traffic rejected before genuine current native fallback, native-created behavior and guarded reservation rollback; controls=$controls traffic_cases=$($trafficCases.Count)"
