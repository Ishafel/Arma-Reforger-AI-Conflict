param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleBoardingFlow.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Conditions([string]$text){
 $code=ConvertTo-AICFCodeText $text
 foreach($match in [regex]::Matches($code,'\bif\s*\(')){
  $start=$match.Index+$match.Length;$depth=1;$cursor=$start
  while($cursor -lt $code.Length -and $depth -gt 0){
   if($code[$cursor] -eq '('){$depth++}
   elseif($code[$cursor] -eq ')'){$depth--}
   $cursor++
  }
  if($depth -eq 0){$code.Substring($start,$cursor-$start-1)}
 }
}
function Rules([string]$text){
 $trace=Body $text 'ReportPresetEntrySnapshot';$tick=Body $text 'TickPreset';$yieldBody=Body $text 'TryYieldPresetSeat';$begin=Body $text 'BeginPreset';$describe=Body $text 'DescribeTokenState'
 $path=Body $text 'ReportPresetEntryPath';$pathCurrent=Body $text 'IsPresetEntryPathCurrent'
 if(!$trace -or !$tick -or !$yieldBody -or !$describe){'MISSING_METHOD';return}
 if($yieldBody -notmatch 'ReportPresetEntrySnapshot\(trip, lease, group, state, token, "STALLED_NATIVE_ENTRY_BEFORE_YIELD"\)' -or $yieldBody.IndexOf('ReportPresetEntrySnapshot') -gt $yieldBody.IndexOf('token.CancelOwnerSafe()')){'PRE_CANCELLATION_OBSERVATION'}
 if($tick -notmatch 'ReportPresetEntrySnapshot\(trip, lease, group, state, expiredToken, "PRESET_BOARDING_DEADLINE"\)' -or $tick.IndexOf('expiredToken, "PRESET_BOARDING_DEADLINE"') -gt $tick.IndexOf('return Reject(trip, lease, causationId, "PRESET_BOARDING_DEADLINE")')){'ALL_ASSIGNED_DEADLINE_OBSERVATIONS'}
 foreach($guard in @('!Replication.IsServer()','trip.GetLease() != lease','!lease.HasPhysicalAsset()','trip.GetBoardingState() != state','!token.Matches(trip, lease, group)','!trip.IsVehiclePresetCurrent()','!token.IsPhysicalMutationOwnerSafe()','lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), vehicleRpl.Id().ToString())','seat.GetVehicle() != vehicle')){if(!$trace.Contains($guard)){'EXACT_READONLY_IDENTITY';break}}
 if($trace -notmatch 'DescribeContext\(trip, lease, trip.GetCausationId\(\), trigger\)' -or $trace -notmatch 'DescribeTokenState\(token, lease\)' -or $trace -notmatch 'member_component_rpl=' -or $trace -notmatch 'supervisor_ready_veto=' -or $trace -notmatch 'native_target_accessible=' -or $trace -notmatch 'native_can_get_in=' -or $describe -notmatch 'available_door_indices=' -or $describe -notmatch 'progress_age_ms='){'FULL_TOKEN_AND_ACCESS_PREDICATES'}
 foreach($api in @('seat.GetAvailableDoorIndices(doors)','manager.GetDoorInfo(doorIndex)','info.GetAIEntryPointInfo()','info.GetEntryPointInfo()','entryPoint.GetWorldTransform(transform)','vector.DistanceXZ(member.GetOrigin(), transform[3])','access.CanGetInVehicleViaDoor(vehicle, manager, doorIndex)','access.CanAccessDoor(vehicle, manager, doorIndex)','seat.IsAvailableFromDoor(doorIndex)')){if(!$trace.Contains($api)){'PINNED_EXACT_DOOR_READBACK';break}}
 if($trace -notmatch 'Math.Min\(16, doors.Count\(\)\)' -or $trace -notmatch 'index < doorLimit' -or $trace -notmatch 'entry_defined=' -or $trace -notmatch 'entry_pos=' -or $trace -notmatch 'doors_total=' -or $trace -notmatch 'native_bt_failure_cause=UNDETERMINED'){'BOUNDED_HONEST_NATIVE_EVIDENCE'}
 $code=ConvertTo-AICFCodeText $trace
 if($code -match '\.(?:Set\w*|AddAction|Fail|Complete|Cancel\w*|Interrupt\w*|GetInVehicle|GetOutVehicle|Begin\w*|Observe\w*|Record\w*)\(|CallLater|Teleport|DeleteRplEntity|LoadTileIn|FindPath|GetCallqueue|m_i\w*\s*[+]?='){'NO_GAMEPLAY_OR_DEADLINE_MUTATION'}
 if($begin -notmatch 'm_Config.GetBoardingTimeoutMs\(\) \* Math.Max\(3, requiredSeats\)' -or $begin -notmatch 'Math.Min\(trip.GetAbsoluteDeadlineMs\(\), now \+ plannedTimeoutMs\)' -or $yieldBody -notmatch 'token.GetProgressAgeMs\(\) < 20000'){'UNCHANGED_ACTION_AND_TIME_POLICY'}
 if(!$path -or !$pathCurrent -or $trace -notmatch 'ReportPresetEntryPath\(trip, lease, group, state, token, trigger\)'){'ENTRY_PATH_SAME_PRE_CANCEL_AND_DEADLINE_SNAPSHOT'}
 foreach($guard in @('!Replication.IsServer()','trip.GetPhase() != AICF_ETransportTripPhase.BOARDING',
  'trip.GetAssignment().GetGroup() != group','trip.GetLease() != lease','!lease.HasPhysicalAsset()',
  'trip.GetBoardingState() != state','!token.Matches(trip, lease, group)','!trip.IsVehiclePresetCurrent()',
  'slot.GetStrategicAssignmentRevision() != trip.GetAssignment().GetAssignmentRevision()',
  'slot.GetStrategicIntentRevision() != trip.GetAssignment().GetStrategicIntentRevision()',
  '!token.IsPhysicalMutationOwnerSafe()','!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity(member)',
  '!SCR_AIDamageHandling.IsConscious(member)','token.GetTargetVehicle() != vehicle',
  'token.GetReservedEntity() != member','token.GetCompartment() != seat','seat.GetVehicle() != vehicle',
  'token.GetAgent() != agent','agent.GetControlledEntity() != member','agent.GetParentGroup() != group',
  'member.GetAIControlComponent().GetControlAIAgent() != agent','agent.GetMovementComponent() != movement',
  '!token.WasPresetQueued()','!token.IsTrackedActionCurrent()','!token.IsTrackedActionOwnedByUtility()',
  'lease.MatchesEntityIdentity(vehicle, vehicle.GetID(), rpl.Id().ToString())',
  'access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()',
  'CompartmentAccessComponent.GetVehicleIn(member)','actionState == EAIActionState.FAILED || actionState == EAIActionState.COMPLETED',
  '!preset.MatchesSeats(seats)','trip.VehiclePresetMember(position) == member',
  'trip.GetVehiclePresetSlot().GetRosterMember(position) == member')){
  if(!$pathCurrent.Contains($guard)){'ENTRY_PATH_EXACT_CURRENT_ACTOR_ACTION_SEAT';break}
 }
 foreach($api in @('agent.GetMovementComponent()','movement.GetAIAgent()','movement.Type().ToString()',
  'movement.GetPathfindingComponent()','movement.GetCurrentPath(points)','movement.GetCurrentPath(repeatedPoints)',
  'movement.HasCompletedRequest(false)','movement.HasCompletedRequest(true)',
  'pathfinding.GetClosestPositionOnNavmesh(transform[3], "4 4 4", nav)')){
  if(!$path.Contains($api)){'PINNED_READONLY_NATIVE_ENTRY_PATH';break}
 }
 if($path -match '\.GetOwner\(' -or $pathCurrent -match '\.GetOwner\(' -or
  $pathCurrent -match 'GetAIAgent\(\)\s*[!=]=\s*agent'){'NO_GUESSED_MOVEMENT_OWNER_CONTRACT'}
 foreach($guard in @('member.GetID() != memberId','Id().ToString() != memberRpl','group.GetID() != groupId',
  'Id().ToString() != groupRpl','lease.MatchesEntityIdentity(vehicle, vehicleId, vehicleRpl)',
  'lease.GetLeaseGeneration() != leaseGeneration','lease.GetVehicleGeneration() != vehicleGeneration',
  'trip.GetAssignment().GetAssignmentRevision() != assignmentRevision','trip.GetAssignment().GetStrategicIntentRevision() != intentRevision','trip.GetVehiclePresetRevision() != presetRevision',
  'token.GetGetInAction() != action','movement.GetAIAgent() != associatedAgent',
  'movement.GetPathfindingComponent() != pathfinding','movement.HasCompletedRequest(false) != completedWithoutTime',
  'movement.HasCompletedRequest(true) != completedWithTime','points[pointIndex] != repeatedPoints[pointIndex]',
  'bool stablePath = count == repeatedPoints.Count()','!stablePath')){
  if(!$path.Contains($guard)){'REPEATED_NATIVE_GETTERS_AND_IDENTITY_FENCE';break}
 }
 if(([regex]::Matches($path,'!IsPresetEntryPathCurrent\(')).Count -ne 2 -or
  $path.IndexOf('!IsPresetEntryPathCurrent(') -gt $path.IndexOf('movement.GetCurrentPath(points)') -or
  $path.LastIndexOf('!IsPresetEntryPathCurrent(') -lt $path.IndexOf('movement.GetCurrentPath(repeatedPoints)') -or
  $path.LastIndexOf('!IsPresetEntryPathCurrent(') -gt $path.IndexOf('int emitted =') -or
  $path -notmatch 'path_status=CONTEXT_OR_PATH_CHANGED_DURING_READ_DISCARDED queried=1 native_point_count=-1 emitted_points=0'){'DISCARD_CHANGED_READ_BEFORE_PUBLICATION'}
 foreach($field in @('native_last_point_entry_distance_xz_m=','native_last_point_entry_height_delta_m=',
  'nav_projection_found=','nav_projection=','nav_projection_delta_xz_m=','nav_projection_height_delta_m=',
  'completed_without_time=','completed_with_time=','native_associated_agent=','movement_agent_is_member=',
  'final_corner_proven=0','remaining_distance_proven=0','reachability_proven=0','nav_search_half_extents=<4,4,4>')){
  if(!$path.Contains($field)){'HONEST_ENTRY_PATH_AND_DOOR_SEPARATION';break}
 }
 if($path -notmatch 'vector.DistanceXZ\(points\[count - 1\], transform\[3\]\)' -or
  $path -notmatch 'if \(projected\)\s*\{\s*projectedText = nav.ToString\(\)' -or
  $path -notmatch 'float projectionDelta = -1' -or $path -notmatch 'float lastEntryDistance = -1'){'NO_SYNTHETIC_NAV_OR_EMPTY_PATH_SUCCESS'}
 if($text -notmatch 'PRESET_ENTRY_PATH_POINT_LIMIT = 64' -or $path -notmatch 'Math.Min\(count, PRESET_ENTRY_PATH_POINT_LIMIT\)' -or
  $path -notmatch 'nativeIndex < emitted' -or $path -notmatch 'Math.Min\(16, doors.Count\(\)\)' -or
  $path -notmatch 'doorOffset < doorLimit' -or $path -notmatch 'point_limit=' -or $path -notmatch 'truncated='){'BOUNDED_ENTRY_PATH_EMISSION'}
 $pathCode=(ConvertTo-AICFCodeText ($path+$pathCurrent)) -replace 'doorSamples.Insert\([^;]+;',''
 if($pathCode -match '\.(?:Set\w*|AddAction|Fail|Complete|Cancel\w*|Interrupt\w*|GetInVehicle|GetOutVehicle|Begin\w*|Observe\w*|Record\w*|Insert|Remove)\(|CallLater|Teleport|DeleteRplEntity|LoadTileIn|FindPath|RequestFollowPath|GetCallqueue|m_i\w*\s*[+]?='){'NO_ENTRY_PATH_GAMEPLAY_OR_CLOCK_MUTATION'}
 # Native 1.8 compiler отверг исходный giant guard как Formula too complex.
 # Небольшие последовательные predicates сохраняют проверки и short-circuit.
 foreach($condition in @(Conditions ($pathCurrent+$path))){
  if(([regex]::Matches($condition,'&&|\|\|')).Count -gt 5){'ENTRY_PATH_SMALL_COMPILER_FORMULAS';break}
 }
 if($path -notmatch 'bool changedContext;' -or
  ([regex]::Matches($path,'if \(!changedContext &&')).Count -ne 6 -or
  $path -notmatch 'if \(changedContext \|\| !stablePath\)'){'POST_READ_SHORT_CIRCUIT_DISCARD'}
}
$failures=@(Rules $source)
if($failures.Count){throw "Preset entry trace: $failures"}
$controls=1
foreach($mutation in @(
 @('ReportPresetEntrySnapshot(trip, lease, group, state, token, "STALLED_NATIVE_ENTRY_BEFORE_YIELD");','Print("missing");','PRE_CANCELLATION_OBSERVATION'),
 @('ReportPresetEntrySnapshot(trip, lease, group, state, expiredToken, "PRESET_BOARDING_DEADLINE");','Print("missing");','ALL_ASSIGNED_DEADLINE_OBSERVATIONS'),
 @('!token.Matches(trip, lease, group)','false','EXACT_READONLY_IDENTITY'),
 @('vehicleRpl.Id().ToString()','Replication.FindItemId(vehicle).ToString()','EXACT_READONLY_IDENTITY'),
 @('DescribeTokenState(token, lease)','string.Empty','FULL_TOKEN_AND_ACCESS_PREDICATES'),
 @('info.GetAIEntryPointInfo()','null','PINNED_EXACT_DOOR_READBACK'),
 @('access.CanGetInVehicleViaDoor(vehicle, manager, doorIndex)','false','PINNED_EXACT_DOOR_READBACK'),
 @('Math.Min(16, doors.Count())','doors.Count()','BOUNDED_HONEST_NATIVE_EVIDENCE'),
 @('native_bt_failure_cause=UNDETERMINED','native_bt_failure_cause=PROVEN_GEOMETRY','BOUNDED_HONEST_NATIVE_EVIDENCE')
 ,@('ReportPresetEntryPath(trip, lease, group, state, token, trigger);','Print("missing path");','ENTRY_PATH_SAME_PRE_CANCEL_AND_DEADLINE_SNAPSHOT')
 ,@('!token.WasPresetQueued()','false','ENTRY_PATH_EXACT_CURRENT_ACTOR_ACTION_SEAT')
 ,@('agent.GetMovementComponent() != movement','false','ENTRY_PATH_EXACT_CURRENT_ACTOR_ACTION_SEAT')
 ,@('token.GetReservedEntity() != member','false','ENTRY_PATH_EXACT_CURRENT_ACTOR_ACTION_SEAT')
 ,@('trip.VehiclePresetMember(position) == member','true','ENTRY_PATH_EXACT_CURRENT_ACTOR_ACTION_SEAT')
 ,@('movement.GetCurrentPath(repeatedPoints);','Print("not repeated");','PINNED_READONLY_NATIVE_ENTRY_PATH')
 ,@('movement.GetAIAgent() != associatedAgent','false','REPEATED_NATIVE_GETTERS_AND_IDENTITY_FENCE')
 ,@('token.GetGetInAction() != action','false','REPEATED_NATIVE_GETTERS_AND_IDENTITY_FENCE')
 ,@('lease.GetLeaseGeneration() != leaseGeneration','false','REPEATED_NATIVE_GETTERS_AND_IDENTITY_FENCE')
 ,@('member.GetID() != memberId','false','REPEATED_NATIVE_GETTERS_AND_IDENTITY_FENCE')
 ,@('trip.GetAssignment().GetStrategicIntentRevision() != intentRevision','false','REPEATED_NATIVE_GETTERS_AND_IDENTITY_FENCE')
 ,@('points[pointIndex] != repeatedPoints[pointIndex]','false','REPEATED_NATIVE_GETTERS_AND_IDENTITY_FENCE')
 ,@('movement.HasCompletedRequest(true) != completedWithTime','false','REPEATED_NATIVE_GETTERS_AND_IDENTITY_FENCE')
 ,@('path_status=CONTEXT_OR_PATH_CHANGED_DURING_READ_DISCARDED queried=1 native_point_count=-1 emitted_points=0','path_status=RAW_NATIVE_ENTRY_PATH queried=1','DISCARD_CHANGED_READ_BEFORE_PUBLICATION')
 ,@('reachability_proven=0','reachability_proven=1','HONEST_ENTRY_PATH_AND_DOOR_SEPARATION')
 ,@('native_last_point_entry_distance_xz_m=','estimated_remaining_distance=','HONEST_ENTRY_PATH_AND_DOOR_SEPARATION')
 ,@('if (projected)','if (true)','NO_SYNTHETIC_NAV_OR_EMPTY_PATH_SUCCESS')
 ,@('PRESET_ENTRY_PATH_POINT_LIMIT = 64','PRESET_ENTRY_PATH_POINT_LIMIT = 256','BOUNDED_ENTRY_PATH_EMISSION')
 ,@('Math.Min(count, PRESET_ENTRY_PATH_POINT_LIMIT)','count','BOUNDED_ENTRY_PATH_EMISSION')
)){
 if(!$source.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($source.Replace($mutation[0],$mutation[1])))){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$trace=Body $source 'ReportPresetEntrySnapshot'
foreach($action in @('access.GetInVehicle(vehicle, seat, false, 0, ECloseDoorAfterActions.INVALID, false);','token.CancelOwnerSafe();','state.Begin(0, 99999999, 1, 0, 99999999);')){
 if('NO_GAMEPLAY_OR_DEADLINE_MUTATION' -notin @(Rules ($source.Replace($trace,$trace.Insert(1,"`n $action"))))){throw "Trace mutation survived: $action"}
 $controls++
}
$yieldBody=Body $source 'TryYieldPresetSeat'
$call='ReportPresetEntrySnapshot(trip, lease, group, state, token, "STALLED_NATIVE_ENTRY_BEFORE_YIELD");'
$late=$yieldBody.Replace($call,'').Replace('token.CancelOwnerSafe();','token.CancelOwnerSafe();'+"`n $call")
if('PRE_CANCELLATION_OBSERVATION' -notin @(Rules ($source.Replace($yieldBody,$late)))){throw 'Post-cancel snapshot mutation survived'}
$controls++
$path=Body $source 'ReportPresetEntryPath'
foreach($action in @('movement.RequestFollowPathOfEntity(vehicle);','token.CancelOwnerSafe();','movement.GetOwner();')){
 $mutated=$source.Replace($path,$path.Insert(1,"`n $action"))
 if(@(Rules $mutated).Count -eq 0){throw "Entry path side effect/guessed API survived: $action"}
 $controls++
}
$pathCurrent=Body $source 'IsPresetEntryPathCurrent'
$giant='if (!trip || !lease || !group || !state || !token || !member || !vehicle || !seat || !agent || !movement) return false;'
if('ENTRY_PATH_SMALL_COMPILER_FORMULAS' -notin @(Rules ($source.Replace($pathCurrent,$pathCurrent.Insert(1,$giant))))){throw 'Native compiler giant formula mutation survived'}
$controls++
$unsafePostRead=$source.Replace('if (!changedContext && (member.GetID() != memberId','if ((member.GetID() != memberId')
if('POST_READ_SHORT_CIRCUIT_DISCARD' -notin @(Rules $unsafePostRead)){throw 'Post-read identity short-circuit mutation survived'}
$controls++
# Проверяем лимит emission из production expression: native count не теряется,
# а пустой path не превращается в invented corner/zero-distance success.
$limit=[int]([regex]::Match($source,'PRESET_ENTRY_PATH_POINT_LIMIT = (\d+)').Groups[1].Value)
foreach($case in @(@(0,0),@(1,1),@(64,64),@(65,64),@(1024,64))){
 if([Math]::Min($case[0],$limit) -ne $case[1]){throw 'Native entry path bound changed'}
 $controls++
}
"PASS Vehicle preset entry trace: identity-safe pre-cancel/deadline snapshots, repeated native foot-path/getters with changed-context discard, bounded64 points/16 doors and explicit completion/nav projection/separation, no action/deadline changes; controls=$controls"
