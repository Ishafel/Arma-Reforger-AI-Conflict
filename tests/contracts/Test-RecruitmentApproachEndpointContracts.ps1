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
 $build=Body $source 'BuildCandidate'
 $begin=Body $plan 'BeginInfantryRecruitment'
 $repair=Body $plan 'RepairInfantryRecruitmentApproach'
 if(!$resolve -or !$build -or !$begin -or !$repair){'METHODS';return}
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
  $resolve -notmatch 'BuildCandidate\(origin, order.m_vPosition, attempt, query\)' -or
  $resolve -match 'GetClosestPositionOnNavmesh\(origin|group.GetOrigin\('){'ACTUAL_LEADER_GEOMETRY'}
 if($source -notmatch 'SAMPLES_PER_RING = 6;' -or
  $source -notmatch 'OUTER_RING_RADIUS_METERS = 32;' -or
  $source -notmatch 'INNER_RING_RADIUS_METERS = 22;' -or
  $build -notmatch 'sample < 0 \|\| sample >= MAXIMUM_SAMPLES' -or
  $build -notmatch 'vector direction = origin - service;' -or
  $build -notmatch 'direction\[1\] = 0;' -or $build -notmatch 'if \(length < 0.1\)' -or
  $build -notmatch 'direction = direction / length;' -or
  $build -notmatch 'vector lateral = Vector\(-direction\[2\], 0, direction\[0\]\);' -or
  $build -notmatch '(?s)float radius = OUTER_RING_RADIUS_METERS;\s*if \(sample >= SAMPLES_PER_RING\)\s*radius = INNER_RING_RADIUS_METERS;' -or
  $build -notmatch '(?s)int directionIndex = sample;\s*if \(directionIndex >= SAMPLES_PER_RING\)\s*directionIndex -= SAMPLES_PER_RING;' -or
  $build -notmatch 'float angle = directionIndex \* 60 \* Math.DEG2RAD;' -or
  $build -notmatch 'candidate = service \+ direction \* \(Math.Cos\(angle\) \* radius\) \+ lateral \* \(Math.Sin\(angle\) \* radius\);' -or
  (ConvertTo-AICFCodeText $source) -match 'GetReachablePoint|Math.Random|System.GetTickCount|\.RayTrace\('){'DETERMINISTIC_SERVICE_RING_SEARCH'}
 if((ConvertTo-AICFCodeText $build) -match '%'){'SUPPORTED_GEOMETRY_ARITHMETIC'}
 if($resolve -notmatch 'query\[1\] = world.GetSurfaceY\(query\[0\], query\[2\]\)' -or
  $resolve -notmatch '!navmesh.IsTileLoaded\(query\)' -or
  $resolve -notmatch '!pathfinding.GetClosestPositionOnNavmesh\(query, "2 3 2", candidate\)' -or
  $resolve -notmatch 'vector.DistanceSqXZ\(query, candidate\) > 4' -or
  $resolve -notmatch 'Math.AbsFloat\(query\[1\] - candidate\[1\]\) > 1'){'LOCAL_GROUND_PROJECTION'}
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
 if(([regex]::Matches($resolve,'AIPathfindingComponent.Cast\(order.m_Group.FindComponent\(AIPathfindingComponent\)\) != pathfinding')).Count -ne 3 -or
  ([regex]::Matches($resolve,'pathfinding.GetNavmeshComponent\(\) != navmesh')).Count -ne 2){'CURRENT_PATHFINDING_COMPONENT'}
 if($resolve -notmatch 'bool better = !found \|\| leg < bestLeg;' -or
  $resolve -notmatch 'if \(found && leg == bestLeg\)' -or
  $resolve -notmatch 'candidate\[0\] < endpoint\[0\] \|\| \(candidate\[0\] == endpoint\[0\] && candidate\[2\] < endpoint\[2\]\)'){'DETERMINISTIC_NEAREST_TIE_BREAK'}
 if($source -notmatch 'INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_SELECTED' -or
  $source -notmatch 'INFANTRY_RECRUITMENT_APPROACH_ENDPOINT_REJECTED' -or
  $source -notmatch 'service_distance_m=' -or $source -notmatch 'leader_leg_m=' -or
  $source -notmatch 'graph_revision=' -or $source -notmatch 'assignment_revision=' -or
  $source -notmatch 'native_projected=1 physically_clear=1 reachability=UNPROVEN movement_confirmation=PENDING' -or
  $source -match 'native_connected=1|reachability=PROVEN'){'OBSERVATION_IS_NOT_ARRIVAL'}
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
# Выполняем скалярные выражения production rings на четырёх повёрнутых/сдвинутых
# геометриях. Ближний край должен появляться среди12 samples даже тогда, когда
# прежние random queries возвращали только точки рядом с застрявшим leader.
$build=Body $endpoint 'BuildCandidate'
$outer=[double]([regex]::Match($endpoint,'OUTER_RING_RADIUS_METERS\s*=\s*(\d+)').Groups[1].Value)
$inner=[double]([regex]::Match($endpoint,'INNER_RING_RADIUS_METERS\s*=\s*(\d+)').Groups[1].Value)
$samples=[int]([regex]::Match($endpoint,'SAMPLES_PER_RING\s*=\s*(\d+)').Groups[1].Value)
$count=[int]([regex]::Match($endpoint,'MAXIMUM_SAMPLES\s*=\s*(\d+)').Groups[1].Value)
$angleExpression=[regex]::Match($build,'float angle = ([^;]+);').Groups[1].Value
$angleExpression=$angleExpression.Replace('directionIndex','$directionIndex').Replace('Math.DEG2RAD','([Math]::PI/180)')
$angleCheck=[scriptblock]::Create('param($directionIndex) '+$angleExpression)
$ringExpression=[regex]::Match($build,'if \((sample >= SAMPLES_PER_RING)\)').Groups[1].Value.Replace('sample','$sample').Replace('SAMPLES_PER_RING','$samples').Replace('>=','-ge')
$innerCheck=[scriptblock]::Create('param($sample,$samples) '+$ringExpression)
$cosExpression=[regex]::Match($build,'direction \* \((Math.Cos\(angle\) \* radius)\)').Groups[1].Value.Replace('Math.Cos','[Math]::Cos').Replace('angle','$angle').Replace('radius','$radius')
$sinExpression=[regex]::Match($build,'lateral \* \((Math.Sin\(angle\) \* radius)\)').Groups[1].Value.Replace('Math.Sin','[Math]::Sin').Replace('angle','$angle').Replace('radius','$radius')
$cosCheck=[scriptblock]::Create('param($angle,$radius) '+$cosExpression)
$sinCheck=[scriptblock]::Create('param($angle,$radius) '+$sinExpression)
$ringCases=@(
 @(1151.95,3278.09,1201.19,3262.19),
 @(0,100,0,0),
 @(100,-100,100,0),
 @(-50,-50,10,10)
)
foreach($case in $ringCases){
 $dx=$case[0]-$case[2];$dz=$case[1]-$case[3];$distance=[Math]::Sqrt($dx*$dx+$dz*$dz)
 $dx/=$distance;$dz/=$distance;$points=@()
 foreach($sample in 0..($count-1)){
  $radius=$outer;if(& $innerCheck $sample $samples){$radius=$inner}
  $directionIndex=$sample;if($directionIndex -ge $samples){$directionIndex-=$samples}
  $angle=[double](& $angleCheck $directionIndex)
  $forward=[double](& $cosCheck $angle $radius);$side=[double](& $sinCheck $angle $radius)
  $x=$case[2]+$dx*$forward-$dz*$side;$z=$case[3]+$dz*$forward+$dx*$side
  $remaining=[Math]::Sqrt(($x-$case[2])*($x-$case[2])+($z-$case[3])*($z-$case[3]))
  $circle=[double](& $radiusCheck $remaining $completion $arrival $margin)
  if([Math]::Abs($remaining-$radius) -gt 0.000001 -or $circle -lt $minimum -or $remaining+$circle+$margin -gt $arrival+0.000001){throw 'Service ring/completion circle escapes physical arrival'}
  foreach($point in $points){if([Math]::Sqrt(($x-$point.x)*($x-$point.x)+($z-$point.z)*($z-$point.z)) -lt 5){throw 'Ring search collapsed distinct endpoint candidates'}}
  $points+=@([pscustomobject]@{x=$x;z=$z;radius=$radius})
 }
 if($points.Count -ne 12){throw 'Ring query budget changed'}
 $near=$points[0];$nearLeg=[Math]::Sqrt(($near.x-$case[0])*($near.x-$case[0])+($near.z-$case[1])*($near.z-$case[1]))
 if([Math]::Abs($nearLeg-[Math]::Abs($distance-$outer)) -gt 0.000001){throw 'First service ring query does not face the actual leader'}
}
$historyExpression=[regex]::Match((Body $endpoint 'TryResolve'),'vector.DistanceXZ\(candidate, attempted\) < MINIMUM_SEPARATION_METERS').Value
$historyExpression=$historyExpression.Replace('vector.DistanceXZ(candidate, attempted)','$separation').Replace('MINIMUM_SEPARATION_METERS','5').Replace('<','-lt')
$historyCheck=[scriptblock]::Create('param($separation) '+$historyExpression)
foreach($case in @(@(0,$true),@(4.999,$true),@(5,$false),@(5.001,$false),@(20,$false))){
 if([bool](& $historyCheck $case[0]) -ne $case[1]){throw 'Previously committed endpoint history separation changed'}
}
if($service -notmatch 'if \(order.m_iApproachRepairs >= 2\)' -or
 $service -notmatch '(?s)order.m_iApproachRepairs\+\+;\s*if \(!order.m_bHiddenRecoveryPending && order.m_MoveFailure && order.m_MoveFailure.TryRecover\(order\)\)\s*\{\s*order.m_bHiddenRecoveryPending = true;\s*order.m_iHiddenRecoverySubmittedAtMs = now;\s*\}\s*order.m_MoveFailure = null;\s*bool issued = m_Planner.RepairInfantryRecruitmentApproach\(order\)' -or
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
 @('BuildCandidate(origin, order.m_vPosition, attempt, query)','BuildCandidate(order.m_Group.GetOrigin(), order.m_vPosition, attempt, query)','ACTUAL_LEADER_GEOMETRY'),
 @('vector origin = leader.GetOrigin()','vector origin = order.m_Group.GetOrigin()','ACTUAL_LEADER_GEOMETRY'),
 @('vector direction = origin - service;','vector direction = service - origin;','DETERMINISTIC_SERVICE_RING_SEARCH'),
 @('sample < 0 || sample >= MAXIMUM_SAMPLES','false','DETERMINISTIC_SERVICE_RING_SEARCH'),
 @('directionIndex -= SAMPLES_PER_RING;','directionIndex += SAMPLES_PER_RING;','DETERMINISTIC_SERVICE_RING_SEARCH'),
 @('int directionIndex = sample;','int directionIndex = sample % SAMPLES_PER_RING;','SUPPORTED_GEOMETRY_ARITHMETIC'),
 @('radius = INNER_RING_RADIUS_METERS;','radius = OUTER_RING_RADIUS_METERS;','DETERMINISTIC_SERVICE_RING_SEARCH'),
 @('!navmesh.IsTileLoaded(query)','false','LOCAL_GROUND_PROJECTION'),
 @('!pathfinding.GetClosestPositionOnNavmesh(query, "2 3 2", candidate)','false','LOCAL_GROUND_PROJECTION'),
 @('vector.DistanceSqXZ(query, candidate) > 4','false','LOCAL_GROUND_PROJECTION'),
 @('Math.AbsFloat(query[1] - candidate[1]) > 1','false','LOCAL_GROUND_PROJECTION'),
 @('remaining > maximumServiceDistance','false','DISTINCT_CONTAINED_ENDPOINT'),
 @('candidateRadius < MINIMUM_COMPLETION_RADIUS_METERS','false','ADAPTIVE_RADIUS_FULL_CIRCLE_CONTAINMENT'),
 @('remaining + candidateRadius + ARRIVAL_MARGIN_METERS > AICF_InfantryRecruitmentConfig.ARRIVAL_METERS','false','ADAPTIVE_RADIUS_FULL_CIRCLE_CONTAINMENT'),
 @('completionRadius = candidateRadius;','completionRadius = COMPLETION_RADIUS_METERS;','ADAPTIVE_RADIUS_FULL_CIRCLE_CONTAINMENT'),
 @('vector.DistanceXZ(candidate, attempted) < MINIMUM_SEPARATION_METERS','false','DISTINCT_CONTAINED_ENDPOINT'),
 @('!AICF_InfantrySpawnPlacement.IsUsable(order.m_Group, endpoint, recheckedPosition)','false','FRESH_PHYSICAL_CLEARANCE_EXACT_PROJECTION'),
 @('vector.Distance(candidate, checkedPosition) > 0.1','false','FRESH_PHYSICAL_CLEARANCE_EXACT_PROJECTION'),
 @('leader.GetID() != leaderId','false','REPEATED_IDENTITY_AFTER_NATIVE_QUERIES'),
 @('vector.Distance(origin, leader.GetOrigin()) > 0.1','false','REPEATED_IDENTITY_AFTER_NATIVE_QUERIES'),
 @('AIPathfindingComponent.Cast(order.m_Group.FindComponent(AIPathfindingComponent)) != pathfinding','false','CURRENT_PATHFINDING_COMPONENT'),
 @('pathfinding.GetNavmeshComponent() != navmesh','false','CURRENT_PATHFINDING_COMPONENT'),
 @('leg < bestLeg','leg > bestLeg','DETERMINISTIC_NEAREST_TIE_BREAK'),
 @('candidate[2] < endpoint[2]','candidate[2] > endpoint[2]','DETERMINISTIC_NEAREST_TIE_BREAK'),
 @('reachability=UNPROVEN','reachability=PROVEN','OBSERVATION_IS_NOT_ARRIVAL'),
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
"PASS Recruitment approach endpoint: bounded deterministic service rings oriented from physical leader, native local projection and fresh clearance without claimed connectivity, full arrival containment/current identity, unchanged paid purchase/deadlines/two repairs; controls=$controls envelope_cases=$($geometryCases.Count) ring_cases=$($ringCases.Count) history_cases=5"
exit 0
