param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$policy=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehiclePresetWaitPolicy.c'))
$registry=[IO.File]::ReadAllText((Join-Path $root 'State/Vehicles/AICF_TransportTripRegistry.c'))
$coordinator=[IO.File]::ReadAllText((Join-Path $root 'Vehicles/AICF_VehicleCoordinator.c'))
$match=[IO.File]::ReadAllText((Join-Path $root 'Bootstrap/AICF_MatchController.c'))
function Body([string]$text,[string]$name){
 Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name
}
function Rules([string]$source,[string]$view,[string]$controller){
 $wait=Body $source 'IsCurrentMountedWait'
 $meaningful=Body $controller 'HasMeaningfulTask'
 $idle=Body $controller 'IsBoundedVehicleLifecycle'
 if(!$wait -or !$meaningful -or !$idle){'MISSING_METHOD';return}
 if($wait -notmatch '!Replication.IsServer\(\)' -or $wait -notmatch 'trip.GetPhase\(\) != AICF_ETransportTripPhase.TRANSIT' -or $wait -notmatch '!trip.IsPresetWaitingForDestination\(\)' -or $wait -notmatch '!trip.GetVehiclePreset\(\)' -or $wait -notmatch '!trip.IsVehiclePresetCurrent\(\)' -or $wait -notmatch 'trip.IsDeadlineReached\(System.GetTickCount\(\)\)'){'COMMITTED_CURRENT_PRESET_WAIT'}
 if($wait -notmatch '!slot.IsCombatReady\(\)' -or $wait -notmatch '!assignment.IsValid\(\)' -or $wait -notmatch 'movement.GetRouteGeneration\(\) != 0' -or $wait -notmatch 'movement.GetRouteWaypoint\(\) \|\| movement.GetSupersededRouteWaypoint\(\)'){'NO_STARTED_OR_UNBOUND_ROUTE_EXEMPTION'}
 foreach($guard in @(
  'assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision()',
  'assignment.GetStrategicIntentRevision() != slot.GetStrategicIntentRevision()',
  'assignment.GetTargetKind() != slot.GetTargetKind()',
  'assignment.GetTargetBase() != slot.GetTargetBase()',
  'assignment.GetTargetPosition() != slot.GetTargetPosition()',
  'movement.GetAssignmentRevision() != assignment.GetAssignmentRevision()',
  'movement.GetTacticalTarget() != assignment.GetTargetPosition()'
 )){if(!$wait.Contains($guard)){'CURRENT_ASSIGNMENT_AND_INTENT'}}
 if($wait -notmatch '\(slot.GetTargetKind\(\) == AICF_EOrderTargetKind.POSITION &&\s*assignment.GetTargetPosition\(\) != slot.GetTargetPosition\(\)\)' -or $wait -notmatch 'currentBaseRevision < 0 \|\| assignment.GetBaseRevision\(\) != currentBaseRevision'){'KIND_AWARE_CURRENT_BASE_REVISION'}
 if($wait -notmatch 'lease.MatchesTripIdentity\(\s*trip.GetFactionKey\(\), trip.GetSlotId\(\), trip.GetGroupGeneration\(\), trip.GetTripGeneration\(\)\)' -or $wait -notmatch 'IsAuthoritativeReplicatedEntity\(vehicle\)' -or $wait -notmatch 'lease.MatchesEntityIdentity\(vehicle, vehicle.GetID\(\), rpl.Id\(\).ToString\(\)\)'){'CURRENT_LEASE_ENTITY_IDENTITY'}
 if($wait -notmatch 'AICF_VehiclePreset.Collect\(vehicle, seats\)' -or $wait -notmatch '!preset.MatchesSeats\(seats\)' -or $wait -notmatch 'int position = preset.m_aMembers\[index\]' -or $wait -notmatch 'if \(position < 0\)\s*\{\s*if \(seats\[index\].GetOccupant\(\)\)\s*return false'){'EXACT_RECIPE_NO_FOREIGN_OCCUPANTS'}
 if($wait -notmatch 'trip.VehiclePresetMember\(position\)' -or $wait -notmatch 'slot.GetRosterMember\(position\) != member' -or $wait -notmatch '!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity\(member\)' -or $wait -notmatch 'agent.GetParentGroup\(\) == group && agent.GetControlledEntity\(\) == member' -or $wait -notmatch '!currentMember \|\| !access'){'CURRENT_EXACT_AI_MEMBERS'}
 if($wait -notmatch '!member.IsInVehicle\(\) \|\| !access.IsInCompartment\(\)' -or $wait -notmatch 'access.IsGettingIn\(\) \|\| access.IsGettingOut\(\) \|\| access.IsSwitchingSeatsAnim\(\)' -or $wait -notmatch 'access.GetCompartment\(\) != seats\[index\] \|\| seats\[index\].GetOccupant\(\) != member' -or $wait -notmatch 'CompartmentAccessComponent.GetVehicleIn\(member\) != vehicle'){'PHYSICAL_EXACT_SETTLED_CREW'}
 if($wait -notmatch 'assigned > 0 && assigned == AICF_GroupRuntime.CountAliveAgents\(group\)' -or $wait -match 'assigned == (?:1|5|10)\b|seats.Count\(\) == 5'){'VARIABLE_PARTIAL_ASSIGNED_ROSTER'}
 if($view -notmatch 'm_bPresetDestinationWait = AICF_VehiclePresetWaitPolicy.IsCurrentMountedWait\(trip, currentBaseRevision\)' -or $view -notmatch 'bool IsPresetDestinationWait\(\) \{ return m_bPresetDestinationWait; \}'){'TYPED_READ_ONLY_SLOT_VIEW'}
 if($meaningful -notmatch 'if \(vehicleView && vehicleView.IsPresetDestinationWait\(\)\)\s*return true;' -or $idle -notmatch 'vehicleView.IsControllingMovement\(\) &&\s*\(vehicleView.HasExecutableVehicleTask\(\) \|\| vehicleView.IsPresetDestinationWait\(\)\)'){'TASK_AND_IDLE_CONSUME_SAME_PROOF'}
 if($controller -notmatch '" preset_destination_wait=%1", vehicleView && vehicleView.IsPresetDestinationWait\(\)'){'NATIVE_POLICY_READBACK'}
 if($source -match 'CallLater|Insert\(|SetOrigin|Teleport|SpawnEntity|DeleteEntity|CommitTransition|CommitPresetDestinationWait|SetPresetDestinationWaiting|ResetMob|ObserveUnexplainedMobIdle|DeadlineMs\s*='){'READ_ONLY_NO_BUDGET_MUTATION'}
}
$failures=@(Rules $policy $registry $match)
if($failures.Count){throw "Vehicle preset destination wait: $failures"}
if($coordinator -notmatch 'new AICF_VehicleSlotView\(trip, m_iObservedBaseRevision\)'){throw 'Slot view must consume the existing coordinator base revision'}
$geometry=[regex]::Match((Body $policy 'IsCurrentMountedWait'),'\(slot.GetTargetKind\(\) == AICF_EOrderTargetKind.POSITION &&\s*assignment.GetTargetPosition\(\) != slot.GetTargetPosition\(\)\)').Value
# Выполняем извлечённый production guard с символическими позициями: BASE может
# иметь штатный resolved spawn, отличный от origin базы; POSITION обязан совпасть.
$geometry=$geometry.Replace('slot.GetTargetKind()','$kind').Replace('AICF_EOrderTargetKind.POSITION','"POSITION"').Replace('assignment.GetTargetPosition()','$resolved').Replace('slot.GetTargetPosition()','$stored').Replace('==','-eq').Replace('!=','-ne').Replace('&&','-and')
$evaluate=[scriptblock]::Create('param($kind,$resolved,$stored) '+$geometry)
$geometryCases=@(
 @('BASE','randomized-native-spawn','base-owner-origin',$false),
 @('BASE','base-owner-origin','base-owner-origin',$false),
 @('POSITION','pointA','pointA',$false),
 @('POSITION','pointA','pointB',$true)
)
foreach($case in $geometryCases){if([bool](& $evaluate $case[0] $case[1] $case[2]) -ne $case[3]){throw "Incorrect $($case[0]) position mismatch gate"}}
$controls=1
foreach($mutation in @(
 @('!trip.IsPresetWaitingForDestination()','false','COMMITTED_CURRENT_PRESET_WAIT'),
 @('!trip.IsVehiclePresetCurrent()','false','COMMITTED_CURRENT_PRESET_WAIT'),
 @('trip.IsDeadlineReached(System.GetTickCount())','false','COMMITTED_CURRENT_PRESET_WAIT'),
 @('movement.GetRouteGeneration() != 0','false','NO_STARTED_OR_UNBOUND_ROUTE_EXEMPTION'),
 @('movement.GetRouteWaypoint() || movement.GetSupersededRouteWaypoint()','false','NO_STARTED_OR_UNBOUND_ROUTE_EXEMPTION'),
 @('assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision()','false','CURRENT_ASSIGNMENT_AND_INTENT'),
 @('assignment.GetStrategicIntentRevision() != slot.GetStrategicIntentRevision()','false','CURRENT_ASSIGNMENT_AND_INTENT'),
 @('assignment.GetTargetPosition() != slot.GetTargetPosition()','false','CURRENT_ASSIGNMENT_AND_INTENT'),
 @('slot.GetTargetKind() == AICF_EOrderTargetKind.POSITION','true','KIND_AWARE_CURRENT_BASE_REVISION'),
 @('assignment.GetBaseRevision() != currentBaseRevision','false','KIND_AWARE_CURRENT_BASE_REVISION'),
 @('lease.MatchesTripIdentity(','UnsafeLeaseIdentity(','CURRENT_LEASE_ENTITY_IDENTITY'),
 @('lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())','true','CURRENT_LEASE_ENTITY_IDENTITY'),
 @('!preset.MatchesSeats(seats)','false','EXACT_RECIPE_NO_FOREIGN_OCCUPANTS'),
 @('if (seats[index].GetOccupant())','if (false)','EXACT_RECIPE_NO_FOREIGN_OCCUPANTS'),
 @('slot.GetRosterMember(position) != member','false','CURRENT_EXACT_AI_MEMBERS'),
 @('!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)','false','CURRENT_EXACT_AI_MEMBERS'),
 @('agent.GetParentGroup() == group && agent.GetControlledEntity() == member','true','CURRENT_EXACT_AI_MEMBERS'),
 @('access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()','false','PHYSICAL_EXACT_SETTLED_CREW'),
 @('seats[index].GetOccupant() != member','false','PHYSICAL_EXACT_SETTLED_CREW'),
 @('CompartmentAccessComponent.GetVehicleIn(member) != vehicle','false','PHYSICAL_EXACT_SETTLED_CREW'),
 @('assigned > 0 && assigned == AICF_GroupRuntime.CountAliveAgents(group)','assigned == 5','VARIABLE_PARTIAL_ASSIGNED_ROSTER')
)){
 if(!$policy.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($policy.Replace($mutation[0],$mutation[1])) $registry $match)){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$broad=$match.Replace('vehicleView && vehicleView.IsPresetDestinationWait()','vehicleView && vehicleView.GetPhase() == "TRANSIT"')
if('TASK_AND_IDLE_CONSUME_SAME_PROOF' -notin @(Rules $policy $registry $broad)){throw 'Broad TRANSIT mutation survived'}
$controls++
$mutating=$policy.Replace('int assigned;','trip.CommitPresetDestinationWait(true, System.GetTickCount()); int assigned;')
if('READ_ONLY_NO_BUDGET_MUTATION' -notin @(Rules $mutating $registry $match)){throw 'Destination budget mutation survived'}
$controls++
"PASS Vehicle preset destination wait: exact current initial mounted wait is a meaningful task/idle exception; BASE resolved spawn differs legally, POSITION remains exact, current base revision and physical identity fenced; controls=$controls geometry_cases=$($geometryCases.Count)"
exit 0
