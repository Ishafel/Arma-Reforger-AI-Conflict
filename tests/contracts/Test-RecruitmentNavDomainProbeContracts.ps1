param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_RecruitmentNavDomainProbe.c'))
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleMultiSquad.ps1'))
function Body([string]$source,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $method}
function Rules([string]$source,[string]$launch){
 $code=ConvertTo-AICFCodeText $source
 $current=Body $source 'Current';$capture=Body $source 'Capture';$sample=Body $source 'Sample';$hook=Body $source 'CheckApproach';$contains=Body $source 'AICF_RNDContains';$slot=Body $source 'AICF_RNDMatchesOrder'
 if(!$current -or !$capture -or !$sample -or !$hook -or !$contains -or !$slot){'METHODS';return}
 $withoutLocalSamples=$code -replace '\bsamples\.Insert\(',''
 if($withoutLocalSamples -match '\b(?:LoadTileIn|SetOrigin|SetWorldTransform|Teleport|SetVelocity|SetCurrentAction|AddAction|GetInVehicle|GetOutVehicle|DeleteRplEntity|SpawnEntityPrefabEx|CallLater|Remove|Insert|Fail|Complete|RepairInfantryRecruitmentApproach|SetRecruitmentOrder|SetLOD|PreventMaxLOD)\s*\(' -or $code -match '\b(?:order|m_Order|m_Slot|slot)\.m_\w+\s*(?:=(?!=)|\+\+|--|\+=|-=)'){'NO_GAMEPLAY_WRITES_OR_ASYNC'}
 # Only the temporary context owns native references; probe and modded owners retain none.
 $probe=$source.Substring($source.IndexOf('class AICF_RecruitmentNavDomainProbe'))
 if($probe -match '(?m)^\s*(?:protected\s+)?(?:static\s+)?(?:ref\s+)?(?:IEntity|SCR_AIGroup|AIWaypoint|AIActionBase|AIAgent|AIBaseMovementComponent|AICF_InfantryRecruitmentOrder)\s+m_'){'NO_RETAINED_NATIVE_CONTEXT'}
 foreach($guard in @('!Replication.IsServer()','!m_Service.AICF_RNDContains(m_Order)',
  'm_Order.m_RecruitmentService != m_Service','!m_Order.IsCurrent(m_Slot)','!m_Slot.AICF_RNDMatchesOrder(m_Order)',
  'm_Order.m_bAwaitingSupplyPlan','m_Order.m_iArrivedAtMs > 0','m_Order.m_Donor',
  'm_Order.m_Group != m_Group','m_Group.GetID() != m_GroupId','m_Order.m_GroupId != m_GroupId',
  'm_Order.m_iGeneration != m_iGeneration','m_Slot.GetSpawnGeneration() != m_iGeneration',
  'm_Order.m_iAssignment != m_iAssignment','m_Order.m_iIntent != m_iIntent','m_Order.m_iGraphRevision != m_iGraph',
  'm_Order.m_iToken != m_iToken','m_Order.m_iApproachRepairs != m_iRepair','m_Order.m_bMovementFailed != m_bFailed',
  'm_Order.m_vPosition != m_vService','m_Order.m_Waypoint != m_Waypoint','m_Waypoint.GetID() != m_WaypointId',
  'm_Slot.GetWaypoint() != m_Waypoint','m_Group.GetCurrentWaypoint() != m_Waypoint','!waypoints.Contains(m_Waypoint)',
  'm_Waypoint.GetOrigin() != m_vEndpoint','m_Waypoint.GetCompletionRadius() != m_fRadius',
  'AICF_GroupRuntime.ResolveAliveLeader(m_Group) != m_Leader','m_Leader.GetID() != m_LeaderId','m_Leader.GetOrigin() != m_vActor',
  'm_Leader.GetAIControlComponent().GetControlAIAgent() != m_Agent','m_Agent.GetControlledEntity() != m_Leader','m_Agent.GetParentGroup() != m_Group',
  'm_Agent.GetMovementComponent() != m_Movement','m_Movement.GetPathfindingComponent() != m_ActorPath',
  'm_Group.FindComponent(AIPathfindingComponent) != m_GroupPath','m_GroupPath.GetNavmeshComponent() != m_GroupNav','m_ActorPath.GetNavmeshComponent() != m_ActorNav',
  'm_Agent.FindComponent(SCR_AIUtilityComponent) != m_Utility','m_Utility.m_OwnerEntity != m_Leader',
  'm_Utility.GetCurrentAction() != m_CurrentAction','m_Utility.GetExecutedAction() != m_ExecutedAction',
  'm_CurrentAction.GetActionState() != m_iCurrentState','m_ExecutedAction.GetActionState() != m_iExecutedState',
  'm_Group.GetGroupUtilityComponent() != m_GroupUtility','m_GroupUtility.GetCurrentAction() != m_GroupAction',
  'activity != m_Activity','activity.m_RelatedWaypoint != m_ActivityWaypoint',
  '!access.IsInCompartment()','!access.IsGettingIn()','!access.IsGettingOut()','!CompartmentAccessComponent.GetVehicleIn(m_Leader)')){
  if(!$current.Contains($guard)){'EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT';break}
 }
 if(!$contains.Contains('!m_bStopped') -or !$contains.Contains('m_aOrders.Contains(order)') -or !$contains.Contains('m_Graph.GetRevision() == order.m_iGraphRevision') -or !$contains.Contains('m_Campaign.IsMaster()') -or !$slot.Contains('m_RecruitmentOrder == order')){'SERVICE_SLOT_EXACT_ORDER'}
 if(([regex]::Matches($hook,'super.CheckApproach\(')).Count -ne 1 -or !$hook.Contains('return result;') -or $hook.IndexOf('"PRE_CHECK"') -gt $hook.IndexOf('super.CheckApproach(') -or $hook.IndexOf('"POST_CHECK"') -lt $hook.IndexOf('super.CheckApproach(')){'ONE_ORIGINAL_DELEGATE_PRE_REPAIR_SNAPSHOT'}
 foreach($proof in @('c.m_Movement.GetCurrentPath(points);','if (count > 512) return;','c.m_Movement.GetCurrentPath(repeated);',
  'if (count != repeated.Count()) return;','if (points[index] != repeated[index]) return;',
  'c.m_Movement.HasCompletedRequest(false) != completed','c.m_Movement.HasCompletedRequest(true) != completedWithTime',
  'c.m_Waypoint.IsWithinCompletionRadius(c.m_Group) != within','path_status=COMPLETE_REPEATED_NATIVE point_limit=512 truncated=0',
  '"GROUP", "ACTOR"','"ACTOR", "ACTOR"','"GROUP", "ENDPOINT"','"ACTOR", "ENDPOINT"',
  '"GROUP", "SERVICE"','"ACTOR", "SERVICE"','"GROUP", "LAST"','"ACTOR", "LAST"',
  'same_nav=','same_action=','activity_matches_waypoint=','readonly=1 cause_proven=0 reachability_proven=0')){
  if(!$capture.Contains($proof)){'FULL_REPEATED_PAIRED_NATIVE_PATH';break}
 }
 if(([regex]::Matches($capture,'!c.Current\(\)')).Count -ne 2 -or $capture.LastIndexOf('!c.Current()') -lt $capture.IndexOf('GetCurrentPath(repeated)') -or $capture.LastIndexOf('!c.Current()') -gt $capture.IndexOf('foreach (string sample : samples) Print')){'PUBLISH_ONLY_AFTER_REPEAT_FENCE'}
 foreach($api in @('nav.IsTileLoaded(query)','nav.IsTileValid(query)','nav.IsTileRequested(query)','path.GetClosestPositionOnNavmesh(query, "4 4 4", projection)')){if(!$sample.Contains($api)){'REAL_TILE_AND_PROJECTION_READS';break}}
 if(!$capture.Contains('if (!Enabled()') -or !$source.Contains('System.GetCLIParam("aicfRecruitmentNavDomainProbe", value) && value == "1"')){'NATIVE_OPT_IN'}
 foreach($part in @('[switch]$RecruitmentNavDomainProbe','if($RecruitmentNavDomainProbe){','tests/fixtures/AICF_RecruitmentNavDomainProbe.c',
  'AICF_ZRecruitmentNavDomainProbe.c','Shared stage lacks requested recruitment nav-domain observer',
  'recruitment_nav_domain_probe=[bool]$RecruitmentNavDomainProbe',"@('-aicfRecruitmentNavDomainProbe','1')",'$taskHashes=@(Get-AICFVehicleSoakSources $StageRoot)','Assert-AICFVehicleSoakSources $StageRoot $taskHashes')){
  if(!$launch.Contains($part)){'ISOLATED_IMMUTABLE_RUNNER_OPT_IN';break}
 }
 $collect=Body $source 'Collect';$query=Body $source 'Query';$accept=Body $source 'AcceptEntity'
 foreach($part in @('MAXIMUM_QUERIES = 32;','MAXIMUM_NATIVE_NODES = 3;')){if(!$source.Contains($part)){'PHYSICAL_QUERY_BOUND';break}}
 foreach($part in @('m_iQueries >= MAXIMUM_QUERIES','m_iQueries++;','body.Mins = "-0.3 0 -0.3"','body.Maxs = "0.3 1.6 0.3"',
  'world.TracePosition(trace, AcceptEntity)','world.TraceMove(trace, AcceptEntity)','trace.Exclude = m_ExcludedLeader',
  'TraceFlags.ENTS | TraceFlags.WORLD | TraceFlags.OCEAN','trace.LayerMask = mask',
  'trace.TraceEnt','trace.TraceNorm','trace.TraceDist','trace.ColliderIndex','trace.NodeIndex','trace.ColliderName','trace.TraceMaterial',
  'blocker.GetPrefabData().GetPrefabName()','root.GetPrefabData().GetPrefabName()',
  'if (kind == "RAY") contact = atHit.ToString();','else sweepOrigin = atHit.ToString();',
  'ray_surface_contact=','sweep_origin_at_hit=','surrogate_body=1 exact_character_capsule=0')){
  if(!$query.Contains($part)){'PHYSICAL_HONEST_CONTACT_AND_MASK';break}
 }
 if(!$accept.Contains('entity != m_ExcludedLeader') -or !$accept.Contains('entity.GetRootParent() != m_ExcludedLeader')){'PHYSICAL_EXCLUDE_SELF_HIERARCHY'}
 foreach($part in @('if (!c.m_bFailed && now - c.m_Order.m_iApproachProgressAtMs < 3000)',
  'AICF_RecruitmentPhysicalReadProbe probe = new AICF_RecruitmentPhysicalReadProbe()',
  'EPhysicsLayerPresets.CharacterAI, EPhysicsLayerPresets.Projectile','array<float> heights = { 0.25, 0.9, 1.5 }',
  'node < points.Count() && nodes < MAXIMUM_NATIVE_NODES','previous = target; nodes++',
  'vector.DistanceSqXZ(c.m_vActor, points[index])','physical_query_limit=32','physical_node_limit=3',
  'c.m_Controller.GetVelocity()','c.m_Controller.GetMovementVelocity()','c.m_Controller.GetMovementInput()','c.m_Controller.GetStance()')){
  if(!$collect.Contains($part)){'PHYSICAL_BOUNDED_OBSERVATION_AND_MOTION';break}
 }
 if(!$current.Contains('m_Leader.GetCharacterController() != m_Controller') -or !$current.Contains('m_Agent.GetLOD() != m_iActorLOD') -or !$current.Contains('m_Group.GetLOD() != m_iGroupLOD') -or !$current.Contains('AIAgent.GetMaxLOD() != m_iMaxLOD') -or !$capture.Contains('actor_lod=') -or !$capture.Contains('group_lod=') -or !$capture.Contains('max_lod=')){'READ_ONLY_REPEATED_LOD_AND_CONTROLLER'}
 if(!$capture.Contains('AICF_RecruitmentPhysicalReadProbe.Collect(c, points, samples, now)') -or $capture.IndexOf('AICF_RecruitmentPhysicalReadProbe.Collect(') -gt $capture.IndexOf('GetCurrentPath(repeated)')){'PHYSICAL_BEFORE_REPEAT_FENCE'}
}
$errors=@(Rules $fixture $runner);if($errors.Count){throw "Recruitment nav-domain probe: $errors"}
$controls=1
foreach($mutation in @(
 @('!m_Slot.AICF_RNDMatchesOrder(m_Order)','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('m_aOrders.Contains(order)','true','SERVICE_SLOT_EXACT_ORDER'),
 @('m_RecruitmentOrder == order','true','SERVICE_SLOT_EXACT_ORDER'),
 @('m_Order.m_iToken != m_iToken','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('m_Slot.GetSpawnGeneration() != m_iGeneration','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('m_Group.GetCurrentWaypoint() != m_Waypoint','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('m_Leader.GetID() != m_LeaderId','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('m_Leader.GetOrigin() != m_vActor','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('m_Utility.GetExecutedAction() != m_ExecutedAction','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('m_GroupUtility.GetCurrentAction() != m_GroupAction','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('activity.m_RelatedWaypoint != m_ActivityWaypoint','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('m_ActorPath.GetNavmeshComponent() != m_ActorNav','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT'),
 @('if (count > 512) return;','if (count > 5120) return;','FULL_REPEATED_PAIRED_NATIVE_PATH'),
 @('if (points[index] != repeated[index]) return;',';','FULL_REPEATED_PAIRED_NATIVE_PATH'),
 @('c.m_Movement.GetCurrentPath(repeated);',';','FULL_REPEATED_PAIRED_NATIVE_PATH'),
 @('c.m_Waypoint.IsWithinCompletionRadius(c.m_Group) != within','false','FULL_REPEATED_PAIRED_NATIVE_PATH'),
 @('"GROUP", "LAST"','"GROUP", "FIRST"','FULL_REPEATED_PAIRED_NATIVE_PATH'),
 @('nav.IsTileRequested(query)','false','REAL_TILE_AND_PROJECTION_READS'),
 @('string result = super.CheckApproach(order, position, now);','string result = string.Empty;','ONE_ORIGINAL_DELEGATE_PRE_REPAIR_SNAPSHOT'),
 @('!Replication.IsServer()','false','EXACT_CURRENT_ORDER_AND_NATIVE_CONTEXT')
 ,@('MAXIMUM_QUERIES = 32','MAXIMUM_QUERIES = 320','PHYSICAL_QUERY_BOUND')
 ,@('MAXIMUM_NATIVE_NODES = 3','MAXIMUM_NATIVE_NODES = 30','PHYSICAL_QUERY_BOUND')
 ,@('m_iQueries >= MAXIMUM_QUERIES','false','PHYSICAL_HONEST_CONTACT_AND_MASK')
 ,@('world.TraceMove(trace, AcceptEntity)','world.TraceMove(trace, null)','PHYSICAL_HONEST_CONTACT_AND_MASK')
 ,@('entity.GetRootParent() != m_ExcludedLeader','true','PHYSICAL_EXCLUDE_SELF_HIERARCHY')
 ,@('if (kind == "RAY") contact = atHit.ToString();','contact = atHit.ToString();','PHYSICAL_HONEST_CONTACT_AND_MASK')
 ,@('EPhysicsLayerPresets.CharacterAI, EPhysicsLayerPresets.Projectile','EPhysicsLayerPresets.Projectile, EPhysicsLayerPresets.Projectile','PHYSICAL_BOUNDED_OBSERVATION_AND_MOTION')
 ,@('node < points.Count() && nodes < MAXIMUM_NATIVE_NODES','node < points.Count()','PHYSICAL_BOUNDED_OBSERVATION_AND_MOTION')
 ,@('m_Agent.GetLOD() != m_iActorLOD','false','READ_ONLY_REPEATED_LOD_AND_CONTROLLER')
 ,@('m_Leader.GetCharacterController() != m_Controller','false','READ_ONLY_REPEATED_LOD_AND_CONTROLLER')
)){
 if(!$fixture.Contains($mutation[0])){throw "Mutation target missing: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($fixture.Replace($mutation[0],$mutation[1])) $runner)){throw "Mutation survived: $($mutation[2])"};$controls++
}
foreach($write in @('nav.LoadTileIn(query);','m_Leader.SetOrigin(m_vActor);','GetGame().GetCallqueue().CallLater(Current, 1000);','m_Order.m_Slot.SetRecruitmentOrder(m_Order);','m_Order.m_iApproachRepairs++;','m_Agent.SetLOD(0);','m_Agent.PreventMaxLOD();')){
 $mutated=$fixture.Replace('bool loaded, valid, requested, found;',"bool loaded, valid, requested, found; $write")
 if('NO_GAMEPLAY_WRITES_OR_ASYNC' -notin @(Rules $mutated $runner)){throw "Mutation write survived: $write"};$controls++
}
foreach($target in @('[switch]$RecruitmentNavDomainProbe','recruitment_nav_domain_probe=[bool]$RecruitmentNavDomainProbe',"@('-aicfRecruitmentNavDomainProbe','1')",'Shared stage lacks requested recruitment nav-domain observer')){
 if('ISOLATED_IMMUTABLE_RUNNER_OPT_IN' -notin @(Rules $fixture ($runner.Replace($target,'REMOVED')))){throw "Missing runner opt-in survived: $target"};$controls++
}
# Upper bound follows the actual two masks/three heights: two actor overlaps plus
# at most three nodes, each with two masks * (position+sweep+three rays).
$maximumNodes=[int]([regex]::Match($fixture,'MAXIMUM_NATIVE_NODES = (\d+)').Groups[1].Value)
$maximumQueries=[int]([regex]::Match($fixture,'MAXIMUM_QUERIES = (\d+)').Groups[1].Value)
for($nodeCount=0;$nodeCount -le $maximumNodes;$nodeCount++){
 $queryCount=2+$nodeCount*2*(2+3)
 if($queryCount -gt $maximumQueries){throw "Physical query budget exceeded: nodes=$nodeCount queries=$queryCount"}
 $controls++
}
"PASS Recruitment nav-domain observer: exact order/actions, complete repeated foot path and paired nav, bounded32 physical surrogate queries/three nodes/two masks with self hierarchy excluded, honest ray contacts/LOD/motion, one original delegate and immutable opt-in; controls=$controls"
