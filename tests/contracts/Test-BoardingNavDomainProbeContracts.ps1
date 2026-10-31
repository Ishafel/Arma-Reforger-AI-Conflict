param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$fixture=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_BoardingNavDomainProbe.c'))
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleMultiSquad.ps1'))
$scenarioRunner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleScenarios.ps1'))
$damageRunner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFTransportDamageFallback.ps1'))
function Body([string]$source,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $method}
function Rules([string]$source,[string]$launch){
 if(!$damageRunner.Contains('[switch]$BoardingNavDomainProbe') -or !$damageRunner.Contains("if(`$BoardingNavDomainProbe){Copy-Item") -or !$damageRunner.Contains('AICF_ZBoardingNavDomainProbe.c') -or !$damageRunner.Contains('boarding_nav_domain_probe=[bool]$BoardingNavDomainProbe') -or !$damageRunner.Contains("if(`$BoardingNavDomainProbe){`$taskAdditional+=@('-aicfBoardingNavDomainProbe','1')}")){'DAMAGE_ISOLATED_OPT_IN'}
 $code=ConvertTo-AICFCodeText $source
 $current=Body $source 'Current';$capture=Body $source 'Capture';$sample=Body $source 'Sample';$spawn=Body $source 'SpawnCandidate';$tick=Body $source 'TickPreset'
 if(!$current -or !$capture -or !$sample -or !$spawn -or !$tick){'MISSING_BOUNDARY';return}
 if($code -match '\b(?:LoadTileIn|SetOrigin|SetWorldTransform|Teleport|SetVelocity|SetCurrentAction|AddAction|GetInVehicle|GetOutVehicle|DeleteRplEntity|SpawnEntityPrefabEx|CallLater|Fail|Complete|SetLOD|SetPermanentLOD|PreventMaxLOD|AllowMaxLOD)\s*\(' -or $code -match 'static\s+(?:ref\s+)?(?:IEntity|Vehicle|SCR_AIGroup|AIPathfindingComponent|NavmeshWorldComponent)\b'){'READ_ONLY_NO_ASYNC_OR_RETAINED_ENTITIES'}
 foreach($guard in @('!Replication.IsServer()','!m_Trip.IsVehiclePresetCurrent()','m_Group.GetID() != m_GroupId','m_Member.GetID() != m_MemberId',
  'm_Trip.VehiclePresetMember(m_iPosition) != m_Member','m_Trip.GetVehiclePresetSlot().GetRosterMember(m_iPosition) != m_Member',
  'm_Trip.GetGroupGeneration() != m_iGroupGeneration','m_Trip.GetTripGeneration() != m_iTripGeneration',
  'm_Trip.GetAssignment().GetAssignmentRevision() != m_iAssignment','m_Trip.GetAssignment().GetStrategicIntentRevision() != m_iIntent',
  'm_Trip.GetVehiclePresetRevision() != m_iPreset','m_Agent.GetControlledEntity() != m_Member','m_Agent.GetParentGroup() != m_Group',
  'm_Movement.GetPathfindingComponent() != m_ActorPath','m_Group.FindComponent(AIPathfindingComponent) != m_GroupPath',
  'm_GroupPath.GetNavmeshComponent() != m_GroupNav','m_ActorPath.GetNavmeshComponent() != m_ActorNav',
  'm_Vehicle.GetID() != m_VehicleId','m_Vehicle.GetOrigin() != m_vVehiclePosition','m_Vehicle.GetYawPitchRoll() != m_vVehicleAngles')){
  if(!$current.Contains($guard)){'EXACT_REPEATED_CONTEXT';break}
 }
 foreach($api in @('nav.IsTileLoaded(query)','nav.IsTileValid(query)','nav.IsTileRequested(query)','path.GetClosestPositionOnNavmesh(query, "4 4 4", projection)')){
  if(!$sample.Contains($api)){'REAL_TILE_AND_PROJECTION_GETTERS';break}
 }
 foreach($guard in @('if (m_bRequireLease)','!m_Lease || m_Trip.GetLease() != m_Lease || !m_Lease.HasPhysicalAsset()',
  'm_Lease.GetVehicle() != m_Vehicle','m_Lease.GetLeaseGeneration() != m_iLeaseGeneration',
  'm_Lease.GetVehicleGeneration() != m_iVehicleGeneration','m_Vehicle.FindComponent(RplComponent) != m_VehicleRpl',
  'm_VehicleRpl.Id().ToString() != m_sVehicleComponentRpl','!m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleComponentRpl)')){
  if(!$current.Contains($guard)){'BOUND_BOARDING_EXACT_LEASE';break}
 }
 if(!$capture.Contains('c.m_bRequireLease = phase == "BOARDING";') -or !$capture.Contains('if (c.m_bRequireLease)') -or
  !$capture.Contains('c.m_Lease = trip.GetLease();') -or !$capture.Contains('bound_lease_required=') -or !$capture.Contains('vehicle_component_rpl=')){'CANDIDATE_AND_BOUND_LEASE_SCOPE'}
 if(([regex]::Matches($capture,'if \(!c.Current\(\)\) continue;')).Count -ne 2 -or $capture.LastIndexOf('if (!c.Current()) continue;') -lt $capture.IndexOf('samples.Insert(Sample(') -or $capture.LastIndexOf('if (!c.Current()) continue;') -gt $capture.IndexOf('foreach (string sample : samples) Print(context + sample)')){'PUBLISH_AFTER_REPEATED_FENCE'}
 foreach($field in @('group_path=','actor_path=','group_nav=','actor_nav=','same_path=','same_nav=','movement_component=','readonly=1 cause_proven=0 reachability_proven=0')){if(!$capture.Contains($field)){'PAIRED_HONEST_NAV_DOMAINS';break}}
 if(!$capture.Contains('position < 10;') -or !$capture.Contains('doorKeys.Count() >= 16)') -or !$capture.Contains('"GROUP", names[queryIndex]') -or !$capture.Contains('"ACTOR", names[queryIndex]') -or !$tick.Contains('m_aAICFNavOperations.Count() >= 32)') -or !$tick.Contains('now + 5000;')){'BOUNDED_MEMBERS_DOORS_HISTORY_CADENCE'}
 if(([regex]::Matches($spawn,'super.SpawnCandidate\(')).Count -ne 1 -or ([regex]::Matches($tick,'super.TickPreset\(')).Count -ne 1 -or !$spawn.Contains('return result;') -or !$tick.Contains('return super.TickPreset(')){'UNCHANGED_NATIVE_DELEGATE'}
 if($spawn.IndexOf('"PRE_SPAWN"') -gt $spawn.IndexOf('super.SpawnCandidate(') -or $spawn.IndexOf('"POST_SPAWN"') -lt $spawn.IndexOf('super.SpawnCandidate(')){'BEFORE_AFTER_SPAWN'}
 if(!$capture.Contains('if (!Enabled()') -or $source -notmatch 'System.GetCLIParam\("aicfBoardingNavDomainProbe", value\) && value == "1"'){'OPT_IN_ONLY'}
 if(!$launch.Contains('[switch]$BoardingNavDomainProbe') -or !$launch.Contains('if($BoardingNavDomainProbe){') -or !$launch.Contains('boarding_nav_domain_probe=[bool]$BoardingNavDomainProbe') -or !$launch.Contains("@('-aicfBoardingNavDomainProbe','1')") -or !$launch.Contains('Shared stage lacks requested boarding nav-domain observer')){'ISOLATED_RUNNER_OPT_IN_PROVENANCE'}
 $scope=$source.IndexOf('class AICF_StagingPathReadContext')
 if($scope -lt 0){'STAGING_METHODS';return}
 $stageCurrent=Body $source.Substring($scope) 'Current';$stage=Body $source 'CaptureStagingPath';$stageHook=Body $source 'ReportStagingMemberAction'
 if(!$stageCurrent -or !$stage -or !$stageHook){'STAGING_METHODS';return}
 foreach($lodCapture in @($capture,$stage)){
  foreach($native in @('int agentLOD = c.m_Agent.GetLOD();','int groupLOD = c.m_Group.GetLOD();','int maxLOD = AIAgent.GetMaxLOD();',' agent_lod=',' group_lod=',' max_lod=')){
   if(!$lodCapture.Contains($native)){'ACTUAL_NATIVE_LOD_READBACK';break}
  }
  foreach($repeat in @('agentLOD != c.m_Agent.GetLOD()','groupLOD != c.m_Group.GetLOD()','maxLOD != AIAgent.GetMaxLOD()')){
   if(!$lodCapture.Contains($repeat) -or $lodCapture.IndexOf($repeat) -gt $lodCapture.LastIndexOf('!c.Current()')){'REPEATED_LOD_BEFORE_FINAL_FENCE';break}
  }
 }
 foreach($guard in @('!super.Current()','m_Trip.GetPhase() != AICF_ETransportTripPhase.APPROACHING_SITE',
  'm_Request.GetRequestGeneration() != m_iRequestGeneration','m_Request.GetSpawnPlan() != m_Plan',
  'm_Plan.GetStagingPosition() != m_vTarget','m_Plan.GetApproachDeadlineMs() != m_iApproachDeadline',
  'm_Plan.GetReservation().MatchesTrip(m_Trip, m_iRequestGeneration)',
  '!m_Token.Matches(m_Trip, m_iRequestGeneration, m_Group)','m_Token.GetAction() != m_Action',
  'm_Token.GetAgent() != m_Agent','m_Token.GetReservedEntity() != m_Member','!m_Token.IsActionOwnedByUtility()',
  'm_Utility.GetCurrentAction() != m_Action','m_Utility.GetExecutedAction() != m_Action',
  'm_Action.GetActionState() != EAIActionState.RUNNING','m_Member.GetOrigin() != m_vActorPosition',
  'access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim()',
  '!CompartmentAccessComponent.GetVehicleIn(character)','AICharacterMovementComponent.Cast(m_Movement) != null')){
  if(!$stageCurrent.Contains($guard)){'STAGING_EXACT_CURRENT_OWNED_FOOT_TOKEN';break}
 }
 foreach($proof in @('if (count > 512) return;','c.m_Movement.GetCurrentPath(points);','c.m_Movement.GetCurrentPath(repeated);',
  'if (count != repeated.Count()) return;','if (points[index] != repeated[index]) return;',
  'c.m_Movement.HasCompletedRequest(false) != completed','c.m_Movement.HasCompletedRequest(true) != completedWithTime',
  '"ACTOR", "ACTOR"','"ACTOR", "TARGET"','"ACTOR", "LAST"','last_target_xz_m=',
  'path_status=COMPLETE_REPEATED_NATIVE','point_limit=512 truncated=0','reachability_proven=0',
  'reason != "ACTION_ACTIVE" && reason != "NO_PROGRESS_TIMEOUT"','SCR_AIDamageHandling.IsConscious(c.m_Member)')){
  if(!$stage.Contains($proof)){'STAGING_COMPLETE_REPEATED_HONEST_PATH';break}
 }
 if(([regex]::Matches($stage,'!c.Current\(\)')).Count -ne 2 -or
  $stage.LastIndexOf('!c.Current()') -lt $stage.IndexOf('GetCurrentPath(repeated)') -or
  $stage.LastIndexOf('!c.Current()') -gt $stage.IndexOf('foreach (string sample : samples) Print(context + sample)')){'STAGING_PUBLISH_AFTER_FINAL_FENCE'}
 if(([regex]::Matches($stageHook,'super.ReportStagingMemberAction\(')).Count -ne 1 -or !$stageHook.Contains('CaptureStagingPath(trip, requestState, plan, token, reason)')){'STAGING_ONE_NATIVE_REPORT_DELEGATE'}
 if(!$scenarioRunner.Contains('[switch]$BoardingNavDomainProbe') -or !$scenarioRunner.Contains('if($BoardingNavDomainProbe){Copy-Item') -or
  !$scenarioRunner.Contains("@('-aicfBoardingNavDomainProbe','1')") -or !$scenarioRunner.Contains('Requested boarding/staging nav observer missing in stage') -or
  !$scenarioRunner.Contains('boarding_nav_domain_probe=[bool]$BoardingNavDomainProbe')){'SCENARIO_ISOLATED_OPT_IN'}
}
$errors=@(Rules $fixture $runner);if($errors.Count){throw "Boarding nav-domain probe: $errors"}
$controls=1
foreach($mutation in @(
 @('m_ActorPath.GetNavmeshComponent() != m_ActorNav','false','EXACT_REPEATED_CONTEXT'),
 @('m_Trip.GetVehiclePresetSlot().GetRosterMember(m_iPosition) != m_Member','false','EXACT_REPEATED_CONTEXT'),
 @('m_Vehicle.GetOrigin() != m_vVehiclePosition','false','EXACT_REPEATED_CONTEXT'),
 @('valid = nav.IsTileValid(query);','valid = true;','REAL_TILE_AND_PROJECTION_GETTERS'),
 @('requested = nav.IsTileRequested(query);','requested = false;','REAL_TILE_AND_PROJECTION_GETTERS'),
 @('if (!c.Current()) continue;','if (false) continue;','PUBLISH_AFTER_REPEATED_FENCE'),
 @('readonly=1 cause_proven=0 reachability_proven=0','readonly=1 cause_proven=1 reachability_proven=1','PAIRED_HONEST_NAV_DOMAINS'),
 @('doorKeys.Count() >= 16','doorKeys.Count() >= 160','BOUNDED_MEMBERS_DOORS_HISTORY_CADENCE'),
 @('now + 5000','now + 5','BOUNDED_MEMBERS_DOORS_HISTORY_CADENCE'),
 @('return super.TickPreset(trip, lease, group, state, causationId);','return null;','UNCHANGED_NATIVE_DELEGATE'),
 @('if (!Enabled()','if (false','OPT_IN_ONLY')
 ,@('m_Trip.GetLease() != m_Lease','false','BOUND_BOARDING_EXACT_LEASE')
 ,@('m_Lease.GetVehicleGeneration() != m_iVehicleGeneration','false','BOUND_BOARDING_EXACT_LEASE')
 ,@('m_VehicleRpl.Id().ToString() != m_sVehicleComponentRpl','false','BOUND_BOARDING_EXACT_LEASE')
 ,@('!m_Lease.MatchesEntityIdentity(m_Vehicle, m_VehicleId, m_sVehicleComponentRpl)','false','BOUND_BOARDING_EXACT_LEASE')
 ,@('c.m_bRequireLease = phase == "BOARDING";','c.m_bRequireLease = true;','CANDIDATE_AND_BOUND_LEASE_SCOPE')
 ,@('m_Utility.GetCurrentAction() != m_Action','false','STAGING_EXACT_CURRENT_OWNED_FOOT_TOKEN')
 ,@('m_Utility.GetExecutedAction() != m_Action','false','STAGING_EXACT_CURRENT_OWNED_FOOT_TOKEN')
 ,@('!m_Token.IsActionOwnedByUtility()','false','STAGING_EXACT_CURRENT_OWNED_FOOT_TOKEN')
 ,@('!m_Token.Matches(m_Trip, m_iRequestGeneration, m_Group)','false','STAGING_EXACT_CURRENT_OWNED_FOOT_TOKEN')
 ,@('m_Request.GetSpawnPlan() != m_Plan','false','STAGING_EXACT_CURRENT_OWNED_FOOT_TOKEN')
 ,@('if (points[index] != repeated[index]) return;',';','STAGING_COMPLETE_REPEATED_HONEST_PATH')
 ,@('if (count > 512) return;','if (count > 5120) return;','STAGING_COMPLETE_REPEATED_HONEST_PATH')
 ,@('c.m_Movement.GetCurrentPath(repeated);',';','STAGING_COMPLETE_REPEATED_HONEST_PATH')
 ,@(' agent_lod=',' OMITTED_AGENT=','ACTUAL_NATIVE_LOD_READBACK')
 ,@(' group_lod=',' OMITTED_GROUP=','ACTUAL_NATIVE_LOD_READBACK')
 ,@(' max_lod=',' OMITTED_MAX=','ACTUAL_NATIVE_LOD_READBACK')
 ,@('int agentLOD = c.m_Agent.GetLOD();','int agentLOD = 0;','ACTUAL_NATIVE_LOD_READBACK')
 ,@('int groupLOD = c.m_Group.GetLOD();','int groupLOD = 0;','ACTUAL_NATIVE_LOD_READBACK')
 ,@('int maxLOD = AIAgent.GetMaxLOD();','int maxLOD = 10;','ACTUAL_NATIVE_LOD_READBACK')
 ,@('agentLOD != c.m_Agent.GetLOD()','false','REPEATED_LOD_BEFORE_FINAL_FENCE')
 ,@('groupLOD != c.m_Group.GetLOD()','false','REPEATED_LOD_BEFORE_FINAL_FENCE')
 ,@('maxLOD != AIAgent.GetMaxLOD()','false','REPEATED_LOD_BEFORE_FINAL_FENCE')
)){
 if(!$fixture.Contains($mutation[0])){throw "Mutation target missing: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($fixture.Replace($mutation[0],$mutation[1])) $runner)){throw "Mutation survived: $($mutation[2])"};$controls++
}
$mutated=$fixture.Replace('bool loaded, valid, requested, found;','bool loaded, valid, requested, found; nav.LoadTileIn(query);')
if('READ_ONLY_NO_ASYNC_OR_RETAINED_ENTITIES' -notin @(Rules $mutated $runner)){throw 'LoadTileIn side effect survived'};$controls++
foreach($write in @('SetLOD(0)','SetPermanentLOD(0)','PreventMaxLOD()','AllowMaxLOD()')){
 $mutated=$fixture.Replace('int agentLOD = c.m_Agent.GetLOD();','int agentLOD = c.m_Agent.GetLOD(); c.m_Agent.'+$write+';')
 if('READ_ONLY_NO_ASYNC_OR_RETAINED_ENTITIES' -notin @(Rules $mutated $runner)){throw "Native LOD mutation survived: $write"};$controls++
}
if('ISOLATED_RUNNER_OPT_IN_PROVENANCE' -notin @(Rules $fixture ($runner.Replace("@('-aicfBoardingNavDomainProbe','1')",'@()')))){throw 'Missing native opt-in survived'};$controls++
$previousScenario=$scenarioRunner
$scenarioRunner=$scenarioRunner.Replace("@('-aicfBoardingNavDomainProbe','1')",'@()')
if('SCENARIO_ISOLATED_OPT_IN' -notin @(Rules $fixture $runner)){throw 'Missing scenario opt-in survived'};$controls++
$scenarioRunner=$previousScenario
$previousDamage=$damageRunner
foreach($target in @('[switch]$BoardingNavDomainProbe','AICF_ZBoardingNavDomainProbe.c','boarding_nav_domain_probe=[bool]$BoardingNavDomainProbe',"@('-aicfBoardingNavDomainProbe','1')")){
 $damageRunner=$previousDamage.Replace($target,'REMOVED')
 if('DAMAGE_ISOLATED_OPT_IN' -notin @(Rules $fixture $runner)){throw "Missing damage opt-in/provenance survived: $target"};$controls++
}
$damageRunner=$previousDamage
"PASS Boarding nav-domain probe: paired exact group/actor projections, loaded/valid/requested, repeated identities, bounded read-only observer, one native delegate and isolated opt-in; controls=$controls"
