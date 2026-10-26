param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$endpoint=[IO.File]::ReadAllText((Join-Path $root 'Forces/AICF_RecruitmentApproachEndpoint.c'))
$planner=[IO.File]::ReadAllText((Join-Path $root 'Orders/AICF_OrderPlanner.c'))
$order=[IO.File]::ReadAllText((Join-Path $root 'Forces/AICF_InfantryRecruitmentOrder.c'))
$service=[IO.File]::ReadAllText((Join-Path $root 'Forces/AICF_InfantryRecruitmentService.c'))
$failure=[IO.File]::ReadAllText((Join-Path $root 'Forces/AICF_RecruitmentApproachFailure.c'))
$config=[IO.File]::ReadAllText((Join-Path $root 'Config/AICF_InfantryRecruitmentConfig.c'))
function Body([string]$text,[string]$name){
 Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name
}
function Rules([string]$source,[string]$plan,[string]$visit){
 $resolve=Body $source 'TryResolve'
 $begin=Body $plan 'BeginInfantryRecruitment'
 $repair=Body $plan 'RepairInfantryRecruitmentApproach'
 if(!$resolve -or !$begin -or !$repair){'METHODS';return}
 foreach($guard in @('!Replication.IsServer()','!order.IsCurrent(order.m_Slot)','!order.HasSafeBarracks()',
  'order.m_iApproachRepairs < 1 || order.m_iApproachRepairs > 2','order.m_iArrivedAtMs > 0 || order.m_Donor',
  'AICF_GroupRuntime.CountAliveAgentsInAnyVehicle(order.m_Group) > 0')){
  if(!$resolve.Contains($guard)){'CURRENT_EXISTING_REPAIR_ONLY'}
 }
 if($source -notmatch 'MAXIMUM_SAMPLES = 12;' -or $resolve -notmatch 'attempt < MAXIMUM_SAMPLES'){'BOUNDED_NATIVE_SAMPLING'}
 if($resolve -notmatch 'ResolveAliveLeader\(order.m_Group\)' -or
  $resolve -notmatch 'vector origin = leader.GetOrigin\(\)' -or
  $resolve -notmatch 'agent.GetParentGroup\(\) == order.m_Group && agent.GetControlledEntity\(\) == leader' -or
  $resolve -notmatch '!navmesh.IsTileLoaded\(origin\)' -or
  $resolve -notmatch '!navmesh.GetReachablePoint\(origin, requestedDistance, candidate\)' -or
  $resolve -match 'GetClosestPositionOnNavmesh\(origin|group.GetOrigin\('){'ACTUAL_CONNECTED_LEADER_ORIGIN'}
 if($resolve -notmatch 'remaining > maximumServiceDistance' -or
  $resolve -notmatch 'leg < MINIMUM_SEPARATION_METERS' -or
  $resolve -notmatch 'vector.DistanceXZ\(candidate, attempted\) < MINIMUM_SEPARATION_METERS' -or
  $visit -notmatch 'ref array<vector> m_aApproachEndpoints = \{\};'){'DISTINCT_CONTAINED_ENDPOINT'}
 if($resolve -notmatch 'candidateRadius < MINIMUM_COMPLETION_RADIUS_METERS' -or
  $resolve -notmatch 'remaining \+ candidateRadius \+ ARRIVAL_MARGIN_METERS > AICF_InfantryRecruitmentConfig.ARRIVAL_METERS' -or
  $resolve -notmatch 'completionRadius = candidateRadius;' -or $source -notmatch 'out vector endpoint, out float completionRadius' -or
  $source -notmatch 'completion_radius_m=%1 completion_containment_m=%2'){'ADAPTIVE_RADIUS_FULL_CIRCLE_CONTAINMENT'}
 if($resolve -notmatch '!AICF_InfantrySpawnPlacement.IsUsable\(order.m_Group, candidate, checkedPosition\)' -or
  $resolve -notmatch 'vector.Distance\(candidate, checkedPosition\) > 0.1' -or
  $resolve -notmatch '!AICF_InfantrySpawnPlacement.IsUsable\(order.m_Group, endpoint, recheckedPosition\)' -or
  $resolve -notmatch 'vector.Distance\(endpoint, recheckedPosition\) > 0.1'){'FRESH_PHYSICAL_CLEARANCE_EXACT_PROJECTION'}
 if(([regex]::Matches($resolve,'!order.IsCurrent\(order.m_Slot\)')).Count -lt 3 -or
  ([regex]::Matches($resolve,'!order.HasSafeBarracks\(\)')).Count -lt 3 -or
  ([regex]::Matches($resolve,'ResolveAliveLeader\(order.m_Group\) != leader \|\| leader.GetID\(\) != leaderId')).Count -ne 2 -or
  ([regex]::Matches($resolve,'!AICF_VehicleBoardingMutationFence.IsAuthoritativeAIEntity\(leader\)')).Count -ne 3 -or
  ([regex]::Matches($resolve,'vector.Distance\(origin, leader.GetOrigin\(\)\) > 0.1')).Count -ne 2){'REPEATED_IDENTITY_AFTER_NATIVE_QUERIES'}
 if($source -notmatch 'INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_SELECTED' -or
  $source -notmatch 'INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_REJECTED' -or
  $source -notmatch 'service_distance_m=' -or $source -notmatch 'leader_leg_m=' -or
  $source -notmatch 'graph_revision=' -or $source -notmatch 'assignment_revision=' -or
  $source -notmatch 'native_connected=1 physically_clear=1 movement_confirmation=PENDING'){'OBSERVATION_IS_NOT_ARRIVAL'}
 if($begin -notmatch 'if \(order.m_iApproachRepairs > 0\)\s*\{\s*vector alternative;' -or
  $begin -notmatch 'AICF_RecruitmentApproachEndpoint.TryResolve\(order, pathfinding, alternative, alternativeRadius\)' -or
  $begin -notmatch 'fallback=ORIGINAL_BOUNDED_ENDPOINT' -or
  $begin -notmatch 'float completionRadius = AICF_RecruitmentApproachEndpoint.COMPLETION_RADIUS_METERS;' -or
  $begin -notmatch '(?s)TryResolve\(order, pathfinding, alternative, alternativeRadius\)\)\s*\{\s*endpoint = alternative;\s*completionRadius = alternativeRadius;\s*\}' -or
  $begin -notmatch 'SetCompletionRadius\(completionRadius\)' -or
  $begin -notmatch 'INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_COMMITTED' -or $begin -notmatch 'waypoint.GetCompletionRadius\(\)'){'PLANNER_REPAIR_ONLY_INTEGRATION'}
 if($begin -notmatch '(?s)if \(order.m_iApproachRepairs > 0 && \(!order.IsCurrent\(order.m_Slot\) \|\| !order.HasSafeBarracks\(\)\)\)\s*\{\s*RplComponent.DeleteRplEntity\(waypoint, false\);\s*return false;\s*\}\s*ClearOrder\(order.m_Slot\)' -or
  $begin -notmatch '(?s)order.m_Waypoint = waypoint;\s*order.m_iAssignment = order.m_Slot.GetStrategicAssignmentRevision\(\);\s*order.m_Slot.SetRecruitmentOrder\(order\);\s*order.m_aApproachEndpoints.Insert\(endpoint\)'){'COMMIT_FENCE_AND_ACTUAL_ENDPOINT_HISTORY'}
 if($resolve -match 'CallLater|SetOrigin|Teleport|SpawnEntity|DeleteRplEntity|ClearOrder|AddSupplies|m_iApproachRepairs\+\+|m_i\w+(?:AtMs|Revision|Token)\s*=' -or
  $begin -match 'm_i(?:StartedAtMs|ArrivedAtMs|ApproachProgressAtMs|ApproachRepairs|Intent|Generation|GraphRevision|Token)\s*='){'NO_MOVEMENT_PAYMENT_OR_BUDGET_MUTATION'}
}
$errors=@(Rules $endpoint $planner $order)
if($errors.Count){throw "Recruitment approach: $errors"}
# Выполняем выражение production envelope: любая точка полной completion circle
# обязана оставаться внутри физического ARRIVAL, включая худшее направление.
$arrival=[double]([regex]::Match($config,'ARRIVAL_METERS\s*=\s*(\d+)').Groups[1].Value)
$completion=[double]([regex]::Match($endpoint,'COMPLETION_RADIUS_METERS\s*=\s*(\d+)').Groups[1].Value)
$minimum=[double]([regex]::Match($endpoint,'MINIMUM_COMPLETION_RADIUS_METERS\s*=\s*(\d+)').Groups[1].Value)
$margin=[double]([regex]::Match($endpoint,'ARRIVAL_MARGIN_METERS\s*=\s*(\d+)').Groups[1].Value)
$expression=[regex]::Match((Body $endpoint 'MaximumServiceDistance'),'(?s)return (Math.Max\(0,.*?\));').Groups[1].Value
$expression=$expression.Replace('Math.Max(0,','[Math]::Max(0,')
$expression=$expression.Replace('AICF_InfantryRecruitmentConfig.ARRIVAL_METERS','$arrival').Replace('MINIMUM_COMPLETION_RADIUS_METERS','$minimum').Replace('ARRIVAL_MARGIN_METERS','$margin')
$maximum=[double](& ([scriptblock]::Create('param($arrival,$minimum,$margin) '+$expression)) $arrival $minimum $margin)
if($arrival -ne 35 -or $completion -ne 15 -or $minimum -ne 1 -or $margin -ne 1 -or $maximum -ne 33){throw 'Unexpected adaptive completion envelope'}
$radiusExpression=[regex]::Match((Body $endpoint 'CompletionRadiusForServiceDistance'),'(?s)return (Math.Min\(.*?\));').Groups[1].Value
$radiusExpression=$radiusExpression.Replace('Math.Min','[Math]::Min').Replace('Math.Max(0,','[Math]::Max(0.0,').Replace('COMPLETION_RADIUS_METERS','$completion').Replace('AICF_InfantryRecruitmentConfig.ARRIVAL_METERS','$arrival').Replace('ARRIVAL_MARGIN_METERS','$margin').Replace('remaining','$remaining')
$radiusCheck=[scriptblock]::Create('param($remaining,$completion,$arrival,$margin) '+$radiusExpression)
$envelope=[regex]::Match((Body $endpoint 'TryResolve'),'remaining > maximumServiceDistance').Value.Replace('remaining','$remaining').Replace('maximumServiceDistance','$maximum').Replace('>','-gt')
$evaluate=[scriptblock]::Create('param($remaining,$maximum) '+$envelope)
$geometryCases=@(@(0,$false,15),@(18.99,$false,15),@(19,$false,15),@(19.001,$false,14.999),@(30.8624,$false,3.1376),@(33,$false,1),@(33.001,$true,0.999),@(34,$true,0),@(35,$true,0))
foreach($case in $geometryCases){
 $rejected=[bool](& $evaluate $case[0] $maximum)
 if($rejected -ne $case[1]){throw "Envelope boundary failed: $($case[0])"}
 $radius=[double](& $radiusCheck $case[0] $completion $arrival $margin)
 if([Math]::Abs($radius-$case[2]) -gt 0.000001){throw "Wrong adaptive radius for native distance $($case[0])"}
 if(!$rejected -and ($radius -lt $minimum -or $case[0]+$radius+$margin -gt $arrival+0.000001)){throw 'Completion circle permits arrival outside physical radius'}
}
if($service -notmatch 'if \(order.m_iApproachRepairs >= 2\)' -or
 $service -notmatch '(?s)order.m_iApproachRepairs\+\+;\s*bool issued = m_Planner.RepairInfantryRecruitmentApproach\(order\)' -or
 $service -notmatch 'order.m_iGraphRevision != m_Graph.GetRevision\(\)' -or
 $failure -notmatch 'graphRevision != m_iGraphRevision' -or
 $failure -notmatch 'DistanceSqXZ\(leader.GetOrigin\(\), m_vGroupPosition\) < 1225'){
 throw 'Existing graph, failure memoization or two-repair budget changed'
}
$controls=1
foreach($mutation in @(
 @('order.m_iApproachRepairs < 1 || order.m_iApproachRepairs > 2','false','CURRENT_EXISTING_REPAIR_ONLY'),
 @('!order.HasSafeBarracks()','false','CURRENT_EXISTING_REPAIR_ONLY'),
 @('MAXIMUM_SAMPLES = 12;','MAXIMUM_SAMPLES = 120;','BOUNDED_NATIVE_SAMPLING'),
 @('GetReachablePoint(origin, requestedDistance, candidate)','GetReachablePoint(order.m_vPosition, requestedDistance, candidate)','ACTUAL_CONNECTED_LEADER_ORIGIN'),
 @('vector origin = leader.GetOrigin()','vector origin = order.m_Group.GetOrigin()','ACTUAL_CONNECTED_LEADER_ORIGIN'),
 @('remaining > maximumServiceDistance','false','DISTINCT_CONTAINED_ENDPOINT'),
 @('candidateRadius < MINIMUM_COMPLETION_RADIUS_METERS','false','ADAPTIVE_RADIUS_FULL_CIRCLE_CONTAINMENT'),
 @('remaining + candidateRadius + ARRIVAL_MARGIN_METERS > AICF_InfantryRecruitmentConfig.ARRIVAL_METERS','false','ADAPTIVE_RADIUS_FULL_CIRCLE_CONTAINMENT'),
 @('completionRadius = candidateRadius;','completionRadius = COMPLETION_RADIUS_METERS;','ADAPTIVE_RADIUS_FULL_CIRCLE_CONTAINMENT'),
 @('vector.DistanceXZ(candidate, attempted) < MINIMUM_SEPARATION_METERS','false','DISTINCT_CONTAINED_ENDPOINT'),
 @('!AICF_InfantrySpawnPlacement.IsUsable(order.m_Group, endpoint, recheckedPosition)','false','FRESH_PHYSICAL_CLEARANCE_EXACT_PROJECTION'),
 @('vector.Distance(candidate, checkedPosition) > 0.1','false','FRESH_PHYSICAL_CLEARANCE_EXACT_PROJECTION'),
 @('leader.GetID() != leaderId','false','REPEATED_IDENTITY_AFTER_NATIVE_QUERIES'),
 @('vector.Distance(origin, leader.GetOrigin()) > 0.1','false','REPEATED_IDENTITY_AFTER_NATIVE_QUERIES'),
 @('movement_confirmation=PENDING','movement_confirmation=PASSED','OBSERVATION_IS_NOT_ARRIVAL')
)){
 if(!$endpoint.Contains($mutation[0])){throw "Mutation target absent: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($endpoint.Replace($mutation[0],$mutation[1])) $planner $order)){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
foreach($mutation in @(
 @('fallback=ORIGINAL_BOUNDED_ENDPOINT','fallback=UNBOUNDED_RETRY','PLANNER_REPAIR_ONLY_INTEGRATION'),
 @('completionRadius = alternativeRadius;','completionRadius = AICF_RecruitmentApproachEndpoint.COMPLETION_RADIUS_METERS;','PLANNER_REPAIR_ONLY_INTEGRATION'),
 @('order.m_iApproachRepairs > 0 && (!order.IsCurrent(order.m_Slot) || !order.HasSafeBarracks())','false','COMMIT_FENCE_AND_ACTUAL_ENDPOINT_HISTORY'),
 @('order.m_aApproachEndpoints.Insert(endpoint);','order.m_aApproachEndpoints.Insert(order.m_vPosition);','COMMIT_FENCE_AND_ACTUAL_ENDPOINT_HISTORY')
)){
 if(!$planner.Contains($mutation[0])){throw "Planner mutation target absent: $($mutation[0])"}
 if($mutation[2] -notin @(Rules $endpoint ($planner.Replace($mutation[0],$mutation[1])) $order)){throw "Planner mutation survived: $($mutation[2])"}
 $controls++
}
$teleport=$endpoint.Replace('endpoint = candidate;','leader.SetOrigin(candidate); endpoint = candidate;')
if('NO_MOVEMENT_PAYMENT_OR_BUDGET_MUTATION' -notin @(Rules $teleport $planner $order)){throw 'Teleport mutation survived'}
$controls++
"PASS Recruitment approach endpoint: bounded native connected alternative from physical leader, full arrival containment, fresh clearance/current identity, unchanged paid purchase/deadlines/two repairs; controls=$controls geometry_cases=$($geometryCases.Count)"
exit 0
