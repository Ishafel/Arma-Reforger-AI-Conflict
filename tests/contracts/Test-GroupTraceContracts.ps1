param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Diagnostics/AICF_GroupTrace.c'
$trace=[IO.File]::ReadAllText($path)
$coordinator=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c'))
function Rules([string]$source){
 $code=ConvertTo-AICFCodeText $source
 $record=[pscustomobject]@{Source=$source;Code=$code}
 $snapshot=Get-AICFMethodBody $record 'Snapshot'
 $update=Get-AICFMethodBody $record 'Update'
 $member=Get-AICFMethodBody $record 'Member'
 $physical=Get-AICFMethodBody $record 'PhysicalMember'
 $path=Get-AICFMethodBody $record 'NativeVehiclePath'
 $pathTrip=Get-AICFMethodBody $record 'IsNativePathTripCurrent'
 $pathDriver=Get-AICFMethodBody $record 'IsNativePathDriverCurrent'
 $unavailable=Get-AICFMethodBody $record 'NativePathUnavailable'
 if($snapshot -notmatch '!Replication.IsServer\(\)'){ 'AUTHORITY' }
 if($code -match '\b(?:SetReserved|AddAction|GetInVehicle|GetOutVehicle|SetOrigin|CommitTransition|BumpMe|SpawnEntityPrefab|DeleteRplEntity|CallLater|Insert)\(' -and $code -notmatch '^$'){
  # Единственная Insert — собственный cursor, без удержания engine entities.
  if($code -match '\b(?:SetReserved|AddAction|GetInVehicle|GetOutVehicle|SetOrigin|CommitTransition|BumpMe|SpawnEntityPrefab|DeleteRplEntity|CallLater)\('){'SIDE_EFFECT'}
 }
 if($update -notmatch 'now < cursor.m_iNextMs' -or $source -notmatch 'Math.Clamp\(value.ToInt\(\), 1000, 60000\)'){'RATE_BOUND'}
 if($update -notmatch 'slot.IsCombatReady\(\)' -or $update -notmatch '!m_bAllGroups && !slot.HasVehiclePreset\(\)'){'SCOPE'}
 if($snapshot -notmatch 'AICF_GroupRuntime.ResolveAliveLeader\(group\)' -or $snapshot -match 'Position\(group\)'){'REAL_LEADER'}
 if($member -notmatch 'agent && boarding && boarding.GetTokens\(\)'){'UNSTARTED_BOARDING_SAFE'}
 if($snapshot -notmatch 'group.GetAgents\(physicalAgents\)' -or $snapshot -notmatch 'physicalIndex < physicalAgents.Count\(\)' -or $snapshot -notmatch 'PhysicalMember\(slot, group, physicalAgents\[physicalIndex\], identity, physicalIndex\)'){'FULL_PHYSICAL_GROUP'}
 if($snapshot -notmatch 'position < slot.GetDeploymentSize\(\)' -or $snapshot -notmatch 'Member\(slot, trip, seats, identity, position\)'){'DECLARED_ROSTER_DIAGNOSTICS'}
 if(!$physical -or $physical -notmatch 'entity = agent.GetControlledEntity\(\)' -or $physical -match 'slot.GetDeploymentSize\(\)'){'PHYSICAL_ENTITY_FROM_ACTUAL_AGENT'}
 if($physical -notmatch 'int rosterPosition = -1;' -or $physical -notmatch 'if \(entity\)' -or $physical -notmatch 'position < AICF_Stage1Config.MAX_GROUP_SIZE' -or $physical -notmatch 'slot.GetRosterMember\(position\) != entity' -or $physical -notmatch 'rosterPosition = position;'){'OBSERVED_ROSTER_MAPPING'}
 if($physical -notmatch 'parent = agent.GetParentGroup\(\)' -or $physical -notmatch 'parent == group' -or $physical -notmatch 'GetControlAIAgent\(\) == agent' -or $physical -notmatch 'rpl.Id\(\).ToString\(\)'){'PHYSICAL_IDENTITY_PROOF'}
 if($physical -notmatch 'vector.DistanceXZ\(entity.GetOrigin\(\), waypoint.GetOrigin\(\)\)' -or $physical -notmatch 'waypoint.GetCompletionRadius\(\)' -or $physical -notmatch 'AICF_GroupRuntime.IsAliveCharacter\(entity\)'){'PHYSICAL_ARRIVAL_PROOF'}
 if($physical -notmatch 'behavior.GetGroupActivityContext\(\)' -or $physical -notmatch 'Entity\(activity.m_RelatedWaypoint\)' -or $physical -notmatch 'groupUtility.GetCurrentAction\(\) == activity' -or $physical -notmatch 'Entity\(defend.m_RelatedWaypoint.m_Value\)'){'NATIVE_DEFEND_OWNERSHIP_PROOF'}
 if($physical -notmatch 'AICF_Stage3Diagnostics.Info\("PHYSICAL_MEMBER_TRACE", details\)'){'SEPARATE_PHYSICAL_EVENT'}
 if(!$path -or !$pathTrip -or !$pathDriver -or !$unavailable -or $snapshot -notmatch 'NativeVehiclePath\(slot, group, trip, identity\)'){'NATIVE_PATH_ON_EXISTING_CADENCE'}
 foreach($guard in @('!Replication.IsServer()','!slot.IsCombatReady()','slot.GetGroup() != group','!trip.IsValid()','trip.IsTerminal()','trip.GetPhase() != AICF_ETransportTripPhase.TRANSIT','assignment.GetGroup() != group','assignment.GetSlotId() != slot.GetSlotId()','assignment.GetGroupGeneration() != slot.GetSpawnGeneration()','assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision()','assignment.GetStrategicIntentRevision() != slot.GetStrategicIntentRevision()','!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group)','!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)','!lease.MatchesTripIdentity','!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), vehicleRpl.Id().ToString())')){if(!$pathTrip.Contains($guard)){'CURRENT_PATH_TRIP_LEASE_AUTHORITY';break}}
 foreach($guard in @('!movement.IsRouteWaypointBound()','movement.GetRouteGeneration() <= 0','movement.GetAssignmentRevision() != assignment.GetAssignmentRevision()','group.GetCurrentWaypoint() != route','!queue.Contains(route)','SCR_AIMoveActivity.Cast(utility.GetCurrentAction())','move.m_Utility != utility','move.m_RelatedWaypoint != route','move.GetRemoveAction() || move.GetActionIsSuspended()')){if(!$pathTrip.Contains($guard)){'EXACT_CURRENT_NATIVE_MOVE_ROUTE';break}}
 foreach($guard in @('!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(driver)','!SCR_AIDamageHandling.IsConscious(driver)','GetControlAIAgent() != driverAgent','driverAgent.GetControlledEntity() != driver','driverAgent.GetParentGroup() != group','driverAgent.GetMovementComponent() != movementComponent','pilot.GetVehicle() != vehicle','pilot.GetOccupant() != driver','!access.IsInCompartment()','access.IsGettingIn()','access.IsGettingOut()','access.IsSwitchingSeatsAnim()','access.GetCompartment() != pilot','CompartmentAccessComponent.GetVehicleIn(driver) != vehicle','!agents.Contains(driverAgent)','registered.GetDriver() != driver','!movementComponent')){if(!$pathDriver.Contains($guard)){'ACTUAL_SETTLED_AUTHORITY_AI_PILOT';break}}
 if($path -notmatch 'PilotCompartmentSlot.Cast\(driver.GetCompartmentAccessComponent\(\).GetCompartment\(\)\)' -or $path -notmatch 'AIBaseMovementComponent movementComponent' -or $path -notmatch 'movementComponent = driverAgent.GetMovementComponent\(\)' -or $path -notmatch 'movementComponent.GetCurrentPath\(points\)' -or $path -notmatch 'source=DRIVER_MOVEMENT_COMPONENT' -or $path -match 'group.GetMovementComponent\(\).GetCurrentPath'){ 'NATIVE_DRIVER_PATH_SOURCE' }
 foreach($guard in @('trip.GetLease() != lease','lease.GetLeaseGeneration() != leaseGeneration','lease.GetVehicleGeneration() != vehicleGeneration','!lease.MatchesEntityIdentity(vehicle, vehicleId, vehicleRpl)','movement.GetRouteWaypoint() != route','movement.GetRouteGeneration() != routeGeneration','trip.GetAssignment().GetAssignmentRevision() != assignmentRevision','utility.GetCurrentAction() != move','driver.GetID() != driverId','RplComponent.Cast(driver.FindComponent(RplComponent)).Id().ToString() != driverRpl')){if(!$path.Contains($guard)){'PATH_POST_READ_IDENTITY';break}}
 if(@([regex]::Matches($path,'IsNativePathTripCurrent\(slot, group, trip\)')).Count -ne 2 -or @([regex]::Matches($path,'IsNativePathDriverCurrent\(group, vehicle, driver, driverAgent, pilot, movementComponent, driverReason\)')).Count -ne 2 -or $path.IndexOf('movementComponent.GetCurrentPath(points)') -gt $path.IndexOf('CONTEXT_CHANGED_DURING_READ_DISCARDED') -or $path -notmatch 'movementComponent.GetAIAgent\(\) != componentAgent'){'PATH_POST_READ_IDENTITY'}
 if($pathDriver -match 'GetAIAgent\(\)\s*!=\s*driverAgent|!carMovement' -or $path -notmatch 'AICarMovementComponent.Cast\(movementComponent\)' -or $path -notmatch 'movementComponent.Type\(\).ToString\(\)' -or $path -notmatch 'componentAgent = movementComponent.GetAIAgent\(\)'){'RAW_COMPONENT_IDENTITY_WITHOUT_OWNER_ASSUMPTION'}
 if($path -match '\.GetOwner\(' -or $pathDriver -match '\.GetOwner\('){'UNSUPPORTED_AI_COMPONENT_OWNER_API'}
 foreach($reason in @('DRIVER_NOT_AUTHORITY_AI','DRIVER_NOT_CONSCIOUS','DRIVER_CONTROL_MISSING','DRIVER_AGENT_MISSING','DRIVER_CONTROL_AGENT_CHANGED','DRIVER_AGENT_ACTOR_MISMATCH','DRIVER_AGENT_GROUP_MISMATCH','ACTUAL_PILOT_COMPARTMENT_MISSING','PILOT_VEHICLE_MISMATCH','PILOT_OCCUPANT_MISMATCH','DRIVER_ACCESS_MISSING','DRIVER_NOT_IN_VEHICLE','DRIVER_NOT_IN_COMPARTMENT','DRIVER_GETTING_IN','DRIVER_GETTING_OUT','DRIVER_SWITCHING_SEATS','DRIVER_ACTUAL_COMPARTMENT_CHANGED','DRIVER_LINKED_VEHICLE_MISMATCH','DRIVER_AGENT_NOT_IN_GROUP','VEHICLE_MANAGER_MISSING','REGISTERED_VEHICLE_MISMATCH','REGISTERED_DRIVER_CHANGED','DRIVER_MOVEMENT_COMPONENT_MISSING','DRIVER_MOVEMENT_COMPONENT_CHANGED')){if(!$pathDriver.Contains('reason = "'+$reason+'"')){'EXACT_DRIVER_REJECTION_REASONS';break}}
 foreach($field in @('driver_candidate=','driver_agent=','movement_component=','movement_component_ref=','native_associated_agent=','car_cast=','movement_agent_is_driver=','driver_control_ref=','driver_control_associated_agent=','control_agent=','driver_agent_controlled=','driver_agent_parent=','movement_agent_controlled=','movement_agent_parent=','actual_pilot=','actual_seat=')){if(!$path.Contains($field)){'RAW_COMPONENT_AND_PILOT_OBSERVATIONS';break}}
 if($source -notmatch 'NATIVE_PATH_POINT_LIMIT = 128;' -or $path -notmatch 'int emitted = Math.Min\(count, NATIVE_PATH_POINT_LIMIT\)' -or $path -notmatch 'index < emitted' -or $path -notmatch 'index, points\[index\]' -or $path -notmatch 'count > emitted, first, last' -or $path -notmatch 'points\[count - 1\].ToString\(\)'){'BOUNDED_RAW_NATIVE_ORDER'}
 if($path -notmatch 'if \(count == 0\) status = "EMPTY_NATIVE_PATH"' -or $unavailable -notmatch 'native_point_count=-1 emitted_points=0' -or $path -notmatch 'CONTEXT_CHANGED_DURING_READ_DISCARDED", true' -or $source -notmatch 'remaining_distance_proven=0' -or $path -match 'remaining_distance_proven=1'){'NO_INVENTED_PATH_OR_PROGRESS_PROOF'}
 if($code -match 'OnPathSet|SetCurrentAction\(|SetExecutedAction\(|ObservePhysicalMotion\(|ObserveRouteProgress\(|\.Fail\(|\.Complete\('){'NO_PATH_CLOCK_ACTION_OR_CALLBACK_MUTATION'}
 foreach($field in @('selected_action=','executed_action=','expected_seat=','actual_seat=','reserved_by_member=','get_in_locked=','getting_in=','getting_out=','life=')){if(!$source.Contains($field)){'FIELD_'+$field}}
 foreach($field in @('physical_index=','roster_position=','agent=','parent_group=','parent_group_exact=','control_agent_exact=','member_rpl=','waypoint_distance_m=','completion_radius_m=','selected_group_activity=','activity_waypoint=','group_activity_current=','defend_waypoint=')){if(!$physical.Contains($field)){'PHYSICAL_FIELD_'+$field}}
 if((Get-AICFMethodBody $record 'Stop') -notmatch 'm_aCursors.Clear\(\)'){'STOP'}
}
$failures=@(Rules $trace)
if($failures.Count){throw "Trace guards: $failures"}
if($coordinator -notmatch 'm_GroupTrace.Update\(usState, this\)' -or $coordinator -notmatch 'm_GroupTrace.Update\(ussrState, this\)' -or $coordinator -notmatch 'm_GroupTrace.Stop\(\)'){throw 'Coordinator lifecycle'}
$controls=1
$native=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Diagnostics/AICF_NativeVehicleTrace.c'))
if(@([regex]::Matches($native,'super.OnActionSelected\(\);')).Count -ne 2 -or @([regex]::Matches($native,'super.WrapBehaviorOutsideOfVehicle\(action\);')).Count -ne 1 -or @([regex]::Matches($native,'super.PerformReaction\(utility, message\);')).Count -ne 1){throw 'Native callbacks must delegate exactly once'}
if($native -match '\b(?:AddAction|Fail|Complete|SetOrigin|GetOutVehicle|GetInVehicle)\('){throw 'Native observer changes behavior'}
if($native -notmatch '\[BaseContainerProps\(\)\]\s*modded class SCR_AIGoalReaction_GetOutVehicle'){throw 'Native serialized reaction must retain BaseContainerProps'}
if(@([regex]::Matches($native,'super.OnMoveFailed\(moveResult, vehicleUsed, isWaypointRelated, moveLocation\);')).Count -ne 1 -or !$native.Contains('NATIVE_GROUP_MOVE_FAILED')){throw 'Native movement failure observer must preserve original callback'}
$controls++
foreach($mutation in @(
 @('!Replication.IsServer()','false','AUTHORITY'),
 @('now < cursor.m_iNextMs','false','RATE_BOUND'),
 @('agent && boarding && boarding.GetTokens()','agent','UNSTARTED_BOARDING_SAFE'),
 @('AICF_GroupRuntime.ResolveAliveLeader(group)','group','REAL_LEADER'),
 @('AICF_Stage3Diagnostics.Info("GROUP_TRACE", details);','group.SetOrigin(vector.Zero);','SIDE_EFFECT'),
 @('group.GetAgents(physicalAgents);','physicalAgents.Clear();','FULL_PHYSICAL_GROUP'),
 @('physicalIndex < physicalAgents.Count()','physicalIndex < slot.GetDeploymentSize()','FULL_PHYSICAL_GROUP'),
 @('entity = agent.GetControlledEntity();','entity = slot.GetRosterMember(physicalIndex);','PHYSICAL_ENTITY_FROM_ACTUAL_AGENT'),
 @('int rosterPosition = -1;','int rosterPosition = physicalIndex;','OBSERVED_ROSTER_MAPPING'),
 @('position < AICF_Stage1Config.MAX_GROUP_SIZE','position < slot.GetDeploymentSize()','OBSERVED_ROSTER_MAPPING'),
 @('slot.GetRosterMember(position) != entity','false','OBSERVED_ROSTER_MAPPING'),
 @('parent == group','true','PHYSICAL_IDENTITY_PROOF'),
 @('GetControlAIAgent() == agent','GetControlAIAgent() != null','PHYSICAL_IDENTITY_PROOF'),
 @('vector.DistanceXZ(entity.GetOrigin(), waypoint.GetOrigin())','0','PHYSICAL_ARRIVAL_PROOF'),
 @('groupUtility.GetCurrentAction() == activity','true','NATIVE_DEFEND_OWNERSHIP_PROOF'),
 @('Entity(defend.m_RelatedWaypoint.m_Value)','Entity(slot.GetWaypoint())','NATIVE_DEFEND_OWNERSHIP_PROOF'),
 @('AICF_Stage3Diagnostics.Info("PHYSICAL_MEMBER_TRACE", details);','AICF_Stage3Diagnostics.Info("MEMBER_TRACE", details);','SEPARATE_PHYSICAL_EVENT'),
 @('NativeVehiclePath(slot, group, trip, identity);','Print(identity);','NATIVE_PATH_ON_EXISTING_CADENCE'),
 @('assignment.GetAssignmentRevision() != slot.GetStrategicAssignmentRevision()','false','CURRENT_PATH_TRIP_LEASE_AUTHORITY'),
 @('assignment.GetGroupGeneration() != slot.GetSpawnGeneration()','false','CURRENT_PATH_TRIP_LEASE_AUTHORITY'),
 @('!lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), vehicleRpl.Id().ToString())','false','CURRENT_PATH_TRIP_LEASE_AUTHORITY'),
 @('move.m_RelatedWaypoint != route','false','EXACT_CURRENT_NATIVE_MOVE_ROUTE'),
 @('group.GetCurrentWaypoint() != route','false','EXACT_CURRENT_NATIVE_MOVE_ROUTE'),
 @('GetControlAIAgent() != driverAgent','false','ACTUAL_SETTLED_AUTHORITY_AI_PILOT'),
 @('driverAgent.GetMovementComponent() != movementComponent','false','ACTUAL_SETTLED_AUTHORITY_AI_PILOT'),
 @('pilot.GetVehicle() != vehicle','false','ACTUAL_SETTLED_AUTHORITY_AI_PILOT'),
 @('pilot.GetOccupant() != driver','false','ACTUAL_SETTLED_AUTHORITY_AI_PILOT'),
 @('access.IsSwitchingSeatsAnim()','false','ACTUAL_SETTLED_AUTHORITY_AI_PILOT'),
 @('movementComponent = driverAgent.GetMovementComponent()','movementComponent = group.GetMovementComponent()','NATIVE_DRIVER_PATH_SOURCE'),
 @('source=DRIVER_MOVEMENT_COMPONENT','source=PROVEN_VEHICLE_ROUTE','NATIVE_DRIVER_PATH_SOURCE'),
 @('movementComponent.GetAIAgent() != componentAgent','false','PATH_POST_READ_IDENTITY'),
 @('componentAgent = movementComponent.GetAIAgent();','componentAgent = AIAgent.Cast(movementComponent.GetOwner());','UNSUPPORTED_AI_COMPONENT_OWNER_API'),
 @('driver.GetAIControlComponent().ToString()','driver.GetAIControlComponent().GetOwner().ToString()','UNSUPPORTED_AI_COMPONENT_OWNER_API'),
 @('movementComponent.Type().ToString()','"AICarMovementComponent"','RAW_COMPONENT_IDENTITY_WITHOUT_OWNER_ASSUMPTION'),
 @('movement_component_ref=','unreported_component=','RAW_COMPONENT_AND_PILOT_OBSERVATIONS'),
 @('reason = "DRIVER_MOVEMENT_COMPONENT_MISSING"','reason = "NO_EXACT_CURRENT_AI_PILOT"','EXACT_DRIVER_REJECTION_REASONS'),
 @('lease.GetLeaseGeneration() != leaseGeneration','false','PATH_POST_READ_IDENTITY'),
 @('movement.GetRouteGeneration() != routeGeneration','false','PATH_POST_READ_IDENTITY'),
 @('driver.GetID() != driverId','false','PATH_POST_READ_IDENTITY'),
 @('NATIVE_PATH_POINT_LIMIT = 128;','NATIVE_PATH_POINT_LIMIT = 129;','BOUNDED_RAW_NATIVE_ORDER'),
 @('Math.Min(count, NATIVE_PATH_POINT_LIMIT)','count','BOUNDED_RAW_NATIVE_ORDER'),
 @('index, points[index]','index, points[emitted - index - 1]','BOUNDED_RAW_NATIVE_ORDER'),
 @('remaining_distance_proven=0','remaining_distance_proven=1','NO_INVENTED_PATH_OR_PROGRESS_PROOF'),
 @('movementComponent.GetCurrentPath(points);','movementComponent.GetCurrentPath(points); movement.ObserveRouteProgress(0, 1, System.GetTickCount());','NO_PATH_CLOCK_ACTION_OR_CALLBACK_MUTATION'),
 @('movementComponent.GetCurrentPath(points);','movementComponent.GetCurrentPath(points); movementComponent.OnPathSet();','NO_PATH_CLOCK_ACTION_OR_CALLBACK_MUTATION')
)){
 if(!$trace.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($trace.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$driver=Get-AICFMethodBody ([pscustomobject]@{Source=$trace;Code=(ConvertTo-AICFCodeText $trace)}) 'IsNativePathDriverCurrent'
$ownerAssumption=$trace.Replace($driver,$driver.Insert(1,'if (movementComponent && movementComponent.GetAIAgent() != driverAgent) return false;'))
if('RAW_COMPONENT_IDENTITY_WITHOUT_OWNER_ASSUMPTION' -notin @(Rules $ownerAssumption)){throw 'Unproven native owner assumption survived'}
$controls++
"PASS Group trace: read-only authority/current pilot/lease, repeated raw base movement identity, supported associated-agent API/types/cast and rejection reasons, <=128 raw ordered points without vehicle-path or remaining-distance proof; controls=$controls"
