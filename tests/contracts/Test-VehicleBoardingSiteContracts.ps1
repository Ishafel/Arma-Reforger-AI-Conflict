param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles'
$query=[IO.File]::ReadAllText((Join-Path $root 'AICF_VehicleBoardingSiteQuery.c'))
$spawner=[IO.File]::ReadAllText((Join-Path $root 'AICF_VehicleSpawner.c'))
$flow=[IO.File]::ReadAllText((Join-Path $root 'AICF_VehicleAcquisitionFlow.c'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
function Rules([string]$source,[string]$spawn,[string]$acquisition){
 $capture=Body $source 'Capture';$current=Body $source 'IsCurrent';$post=Body $source 'ReportSpawnedEntries';$suitable=Body $source 'IsSuitable'
 $select=Body $spawn 'EvaluateAcquisitionPosition';$commit=Body $spawn 'RevalidateSpawnCommit';$spawnCandidate=Body $acquisition 'SpawnCandidate';$process=Body $acquisition 'ProcessSpawnCommit'
 if(!$capture -or !$current -or !$post -or !$suitable -or !$select -or !$commit -or !$spawnCandidate -or !$process){'METHODS';return}
 $code=ConvertTo-AICFCodeText $source
 if($code -match '\b(?:LoadTileIn|RequestNavmeshLoad|RequestNavmeshRebuild|SetOrigin|Teleport|SetVelocity|SetCurrentAction|AddAction|SpawnEntityPrefabEx|DeleteRplEntity|PurchaseVehiclePreset|CallLater)\s*\(' -or
  $code -match 'static\s+(?:ref\s+)?(?:Vehicle|IEntity|SCR_AIGroup|Physics|AIPathfindingComponent|NavmeshWorldComponent)\s+\w+\s*[;=]'){'READ_ONLY_NO_RETAINED_ENGINE_STATE'}
 foreach($guard in @('!Replication.IsServer()','!AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(group)','if (!rpl || !world || !pathfinding || !group.GetFaction())','if (!navmesh) return result;','IsCurrent(group, groupId, rpl, componentRpl, faction, world, pathfinding, navmesh)')){if(!$capture.Contains($guard)){'CAPTURE_AUTHORITY_AND_CONTEXT';break}}
 foreach($guard in @('group.GetID() != groupId','group.FindComponent(RplComponent) != rpl','rpl.Id().ToString() != componentRpl','group.GetFaction() != faction','group.GetWorld() != world','group.FindComponent(AIPathfindingComponent) != pathfinding','pathfinding.GetNavmeshComponent() == navmesh')){if(!$current.Contains($guard)){'REPEATED_GROUP_PATHFINDING_CONTEXT';break}}
 if(!$capture.Contains('result.m_sReason = "FOOT_PAD_CONTEXT_CHANGED";') -or !$capture.Contains('result.m_aSamples.Clear();')){'DISCARD_CHANGED_CONTEXT'}
 if($source -notmatch 'PAD_RADIUS_METERS = 4;' -or !$capture.Contains('sample < offsets.Count()') -or !$capture.Contains('position + offsets[sample] * PAD_RADIUS_METERS') -or !$capture.Contains('query[1] = world.GetSurfaceY(query[0], query[2]);')){'BOUNDED_FOOT_PAD_GEOMETRY'}
 if(!$capture.Contains('bool loaded = navmesh.IsTileLoaded(query);') -or !$capture.Contains('bool requested = navmesh.IsTileRequested(query);') -or !$capture.Contains('bool found = loaded && pathfinding.GetClosestPositionOnNavmesh(query, "2 2 2", projected);')){'LOADED_NATIVE_PROJECTION_ONLY'}
 foreach($gate in @('delta > MAXIMUM_PROJECTION_XZ_METERS','Math.AbsFloat(heightDelta) > MAXIMUM_PROJECTION_HEIGHT_METERS','if (!loaded) reason = "FOOT_PAD_TILE_UNAVAILABLE";','else if (!found) reason = "FOOT_PAD_PROJECTION_UNAVAILABLE";','if (reason != "COVERED" && result.m_sReason.IsEmpty()) result.m_sReason = reason;')){if(!$capture.Contains($gate)){'ALL_SAMPLES_MUST_BE_COVERED';break}}
 if(!$suitable.Contains('return m_sReason.IsEmpty() && m_aSamples.Count() == 9;')){'FULL_NONEMPTY_COVERAGE_REQUIRED'}
 foreach($field in @('sample_phase=','VEHICLE_FOOT_PAD_PROBE','VEHICLE_FOOT_PAD_POINT','tile_loaded=','tile_requested=','projection_found=','delta_xz_m=','height_delta_m=','reachability_proven=0')){if(!$source.Contains($field)){'HONEST_COVERAGE_DIAGNOSTICS';break}}
 if($source -match 'reachability_proven=1|async_carving_proven=1|native_connected=1'){'HONEST_COVERAGE_DIAGNOSTICS'}
 if(!$select.Contains('AICF_VehicleBoardingSiteQuery.Capture(group, position)') -or !$select.Contains('footPad.Report(identityContext, "SITE_PREFLIGHT");') -or
  !$select.Contains('if (!footPad.IsSuitable()) return false;') -or $select.IndexOf('AICF_VehicleBoardingSiteQuery.Capture') -gt $select.IndexOf('result.m_sResult = "SELECTED"')){'PREFLIGHT_ELIGIBILITY_BEFORE_SELECTION'}
 if(!$commit.Contains('AICF_VehicleBoardingSiteQuery.Capture(group, selection.m_vPosition)') -or !$commit.Contains('if (!footPad.IsSuitable())') -or
  !$commit.Contains('failureReason = footPad.m_sReason;') -or $commit.IndexOf('AICF_VehicleBoardingSiteQuery.Capture') -gt $commit.LastIndexOf('failureReason = string.Empty;')){'FRESH_COMMIT_ELIGIBILITY'}
 if($commit.IndexOf('AICF_VehicleBoardingSiteQuery.Capture') -gt $commit.IndexOf('plan.MarkFootPadProbeResultReported') -or !$commit.Contains('if (footResult.IsEmpty()) footResult = "FOOT_PAD_COVERED";') -or
  $process.IndexOf('RevalidateSpawnCommit') -gt $process.IndexOf('EnsureReservedLease')){'LOG_DEDUP_NEVER_CACHES_ELIGIBILITY'}
 if(!$spawnCandidate.Contains('AICF_VehicleBoardingSiteQuery.ReportSpawnedEntries(trip.GetAssignment().GetGroup(), spawnedVehicle, identity);') -or
  $spawnCandidate.IndexOf('ReportSpawnedEntries') -gt $spawnCandidate.IndexOf('candidate.m_Vehicle = spawnedVehicle;')){'POST_SPAWN_READONLY_HOOK'}
 foreach($read in @('keys.Count() < MAXIMUM_ENTRY_DOORS','if (keys.Contains(key)) continue;','AICF_VehiclePreset.RelativeOwnerIndex(seat)','info.GetAIEntryPointInfo()','info.GetEntryPointInfo()','entry.GetWorldTransform(transform)','loaded = navmesh.IsTileLoaded(transform[3]);','found = loaded && pathfinding.GetClosestPositionOnNavmesh(transform[3], "4 4 4", projected);','sample_phase=POST_SPAWN_IMMEDIATE','async_carving_proven=0')){if(!$post.Contains($read)){'BOUNDED_UNIQUE_NATIVE_ENTRY_OBSERVATION';break}}
 if($source -notmatch 'MAXIMUM_ENTRY_DOORS = 16;'){'BOUNDED_UNIQUE_NATIVE_ENTRY_OBSERVATION'}
 foreach($fence in @('IsCurrent(group, groupId, groupRpl, groupComponentRpl, faction, world, pathfinding, navmesh)','vehicle.GetID() != vehicleId','vehicle.FindComponent(RplComponent) != vehicleRpl','vehicleRpl.Id().ToString() != vehicleComponentRpl','vector.Distance(vehicle.GetOrigin(), position) > 0.1','vehicle.GetYawPitchRoll() != angles')){if(!$post.Contains($fence)){'POST_SPAWN_REPEATED_IDENTITY_AND_POSE';break}}
 if($post.IndexOf('IsCurrent(group,') -gt $post.IndexOf('Info("VEHICLE_ENTRY_SITE_PROBE"')){'POST_PUBLISH_AFTER_FENCE'}
}
$errors=@(Rules $query $spawner $flow);if($errors.Count){throw "Boarding site contracts: $errors"}
$capture=Body $query 'Capture'
$offsetBlock=[regex]::Match($capture,'(?s)array<vector> offsets = \{(.*?)\};').Groups[1].Value
$offsets=@([regex]::Matches($offsetBlock,'"(-?\d+) 0 (-?\d+)"')|ForEach-Object{[pscustomobject]@{x=[int]$_.Groups[1].Value;z=[int]$_.Groups[2].Value}})
if($offsets.Count -ne 9 -or @($offsets|ForEach-Object{"$($_.x),$($_.z)"}|Sort-Object -Unique).Count -ne 9 -or
 @($offsets|Where-Object{[Math]::Abs($_.x) -gt 1 -or [Math]::Abs($_.z) -gt 1}).Count -or $offsets[0].x -ne 0 -or $offsets[0].z -ne 0){throw 'Foot pad sample grid is not distinct bounded center plus eight neighbors'}
$xz=[double]([regex]::Match($query,'MAXIMUM_PROJECTION_XZ_METERS = ([0-9.]+);').Groups[1].Value)
$height=[double]([regex]::Match($query,'MAXIMUM_PROJECTION_HEIGHT_METERS = ([0-9.]+);').Groups[1].Value)
$gapExpression=[regex]::Match($capture,'delta > MAXIMUM_PROJECTION_XZ_METERS').Value.Replace('delta','$delta').Replace('MAXIMUM_PROJECTION_XZ_METERS','$maximum').Replace('>','-gt')
$gapCheck=[scriptblock]::Create('param($delta,$maximum) '+$gapExpression)
$heightExpression=[regex]::Match($capture,'Math.AbsFloat\(heightDelta\) > MAXIMUM_PROJECTION_HEIGHT_METERS').Value.Replace('Math.AbsFloat','[Math]::Abs').Replace('heightDelta','$delta').Replace('MAXIMUM_PROJECTION_HEIGHT_METERS','$maximum').Replace('>','-gt')
$heightCheck=[scriptblock]::Create('param($delta,$maximum) '+$heightExpression)
$cases=@(@(0,0,$false),@(0.5,1,$false),@(0.5001,0,$true),@(0,-1,$false),@(0,-1.0001,$true),@(1.6521,-0.0842513,$true),@(1.73755,-0.019116,$true))
foreach($case in $cases){$rejected=(& $gapCheck $case[0] $xz) -or (& $heightCheck $case[1] $height);if([bool]$rejected -ne $case[2]){throw 'Native projection eligibility boundary failed'}}
$controls=1
foreach($mutation in @(
 @('if (!navmesh) return result;','if (false) return result;','CAPTURE_AUTHORITY_AND_CONTEXT'),
 @('group.GetID() != groupId','false','REPEATED_GROUP_PATHFINDING_CONTEXT'),
 @('group.FindComponent(RplComponent) != rpl','false','REPEATED_GROUP_PATHFINDING_CONTEXT'),
 @('pathfinding.GetNavmeshComponent() == navmesh','true','REPEATED_GROUP_PATHFINDING_CONTEXT'),
 @('result.m_aSamples.Clear();',';','DISCARD_CHANGED_CONTEXT'),
 @('PAD_RADIUS_METERS = 4;','PAD_RADIUS_METERS = 40;','BOUNDED_FOOT_PAD_GEOMETRY'),
 @('bool found = loaded && pathfinding.GetClosestPositionOnNavmesh(query, "2 2 2", projected);','bool found = true;','LOADED_NATIVE_PROJECTION_ONLY'),
 @('delta > MAXIMUM_PROJECTION_XZ_METERS','false','ALL_SAMPLES_MUST_BE_COVERED'),
 @('Math.AbsFloat(heightDelta) > MAXIMUM_PROJECTION_HEIGHT_METERS','false','ALL_SAMPLES_MUST_BE_COVERED'),
 @('if (!loaded) reason = "FOOT_PAD_TILE_UNAVAILABLE";','if (false) reason = "FOOT_PAD_TILE_UNAVAILABLE";','ALL_SAMPLES_MUST_BE_COVERED'),
 @('m_aSamples.Count() == 9','m_aSamples.Count() >= 0','FULL_NONEMPTY_COVERAGE_REQUIRED'),
 @('reachability_proven=0','reachability_proven=1','HONEST_COVERAGE_DIAGNOSTICS'),
 @('MAXIMUM_ENTRY_DOORS = 16;','MAXIMUM_ENTRY_DOORS = 160;','BOUNDED_UNIQUE_NATIVE_ENTRY_OBSERVATION'),
 @('if (keys.Contains(key)) continue;','if (false) continue;','BOUNDED_UNIQUE_NATIVE_ENTRY_OBSERVATION'),
 @('vehicleRpl.Id().ToString() != vehicleComponentRpl','false','POST_SPAWN_REPEATED_IDENTITY_AND_POSE'),
 @('vehicle.GetYawPitchRoll() != angles','false','POST_SPAWN_REPEATED_IDENTITY_AND_POSE')
)){
 if(!$query.Contains($mutation[0])){throw "Mutation target missing: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($query.Replace($mutation[0],$mutation[1])) $spawner $flow)){throw "Mutation survived: $($mutation[2])"};$controls++
}
foreach($mutation in @(
 @('if (!footPad.IsSuitable()) return false;','if (false) return false;','PREFLIGHT_ELIGIBILITY_BEFORE_SELECTION'),
 @('if (!footPad.IsSuitable())','if (false)','FRESH_COMMIT_ELIGIBILITY'),
 @('if (footResult.IsEmpty()) footResult = "FOOT_PAD_COVERED";',';','LOG_DEDUP_NEVER_CACHES_ELIGIBILITY')
)){
 if($mutation[2] -notin @(Rules $query ($spawner.Replace($mutation[0],$mutation[1])) $flow)){throw "Spawner mutation survived: $($mutation[2])"};$controls++
}
$hook='AICF_VehicleBoardingSiteQuery.ReportSpawnedEntries(trip.GetAssignment().GetGroup(), spawnedVehicle, identity);'
if('POST_SPAWN_READONLY_HOOK' -notin @(Rules $query $spawner ($flow.Replace($hook,'')))){throw 'Missing spawned-entry observation survived'};$controls++
$mutation=$query.Replace('result.m_vOrigin = position;','group.SetOrigin(position); result.m_vOrigin = position;')
if('READ_ONLY_NO_RETAINED_ENGINE_STATE' -notin @(Rules $mutation $spawner $flow)){throw 'Movement mutation survived'};$controls++
"PASS Vehicle boarding site: nine loaded native foot-pad projections, strict gaps/height, repeated identity, fresh selector/commit gate, <=16 unique immediate entry observations without reachability or async-carving proof; controls=$controls projection_cases=$($cases.Count) grid_samples=$($offsets.Count)"
