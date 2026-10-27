param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$spawner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleSpawner.c'))
$flow=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleAcquisitionFlow.c'))
function Body([string]$source,[string]$method){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $method}
function Rules([string]$spawn,[string]$acquisition){
 $predicate=Body $spawn 'IsStagingGeometryReady';$measure=Body $spawn 'MeasureStagingReadiness';$commit=Body $spawn 'RevalidateSpawnCommit'
 $approach=Body $acquisition 'ProcessApproachingSite';$member=Body $acquisition 'IsMemberWithinSpawnBoardingEnvelope';$current=Body $acquisition 'IsSpawnPlanCurrent';$relocate=Body $acquisition 'TryRelocateOneExhaustedStagingOutlier'
 if(!$predicate -or !$measure -or !$commit -or !$approach -or !$member -or !$current -or !$relocate){'MISSING_BOUNDARY';return}
 foreach($proof in @('stagingRadiusMeters <= 0','boardingEnvelopeRadiusMeters <= VEHICLE_CLEARANCE_RADIUS_METERS + REQUESTING_GROUP_CLEARANCE_MARGIN_METERS',
  'requireManualRally && !(stagingDistanceMeters <= stagingRadiusMeters)',
  'padDistanceMeters >= VEHICLE_CLEARANCE_RADIUS_METERS + REQUESTING_GROUP_CLEARANCE_MARGIN_METERS',
  'padDistanceMeters <= boardingEnvelopeRadiusMeters')){if(!$predicate.Contains($proof)){'EXACT_MANUAL_RALLY_AND_OLD_AUTO_ENVELOPE';break}}
 if(!$measure.Contains('IsStagingGeometryReady(distanceMeters, stagingRadiusMeters, padDistanceMeters, boardingEnvelopeRadiusMeters, requireManualRally)') -or
  !$spawn.Contains('bool requireManualRally = false')){'SHARED_MEASURE_PREDICATE_AUTO_DEFAULT'}
 if(!$commit.Contains('plan.RequiresManualRally()') -or !$approach.Contains('plan.RequiresManualRally()') -or !$member.Contains('AICF_VehicleSpawner.IsStagingGeometryReady(') -or
  !$member.Contains('m_Config.GetSpawnStagingRadiusMeters()') -or !$member.Contains('plan.RequiresManualRally()')){'FRESH_COMMIT_APPROACH_AND_MEMBER_SAME_POLICY'}
 if(!$spawn.Contains('m_bRequireManualRally = requireManualRally;') -or !$spawn.Contains('bool RequiresManualRally() { return m_bRequireManualRally; }') -or
  !$acquisition.Contains('trip.GetVehiclePreset() != null);') -or !$current.Contains('plan.RequiresManualRally() == (trip.GetVehiclePreset() != null)') -or
  !$current.Contains('m_Spawner.IsSiteReservationCurrent(')){'CAPTURED_MANUAL_SCOPE_CURRENT_REQUEST_FENCE'}
 $guard='if (plan && plan.RequiresManualRally()) return false;'
 if(!$relocate.Contains($guard) -or $relocate.IndexOf($guard) -gt $relocate.IndexOf('plan.CanProbeStagingRelocation(')){'MANUAL_NO_PHYSICAL_RELOCATION'}
 if($acquisition -match 'IsMemberWithinSpawnBoardingEnvelope\((?:entity|character), plan.GetSpawnPosition\(\)\)'){'NO_OLD_ENVELOPE_EARLY_CANCELLATION'}
 if($predicate -match 'SetOrigin|Teleport|GetTickCount|Set\w*\(|AddAction|Deadline|CallLater'){'PURE_GEOMETRY_POLICY'}
}
$errors=@(Rules $spawner $flow);if($errors.Count){throw "Manual vehicle rally: $errors"}
$controls=1
foreach($mutation in @(
 @('requireManualRally && !(stagingDistanceMeters <= stagingRadiusMeters)','false','EXACT_MANUAL_RALLY_AND_OLD_AUTO_ENVELOPE'),
 @('padDistanceMeters <= boardingEnvelopeRadiusMeters','true','EXACT_MANUAL_RALLY_AND_OLD_AUTO_ENVELOPE'),
 @('m_bRequireManualRally = requireManualRally;','m_bRequireManualRally = false;','CAPTURED_MANUAL_SCOPE_CURRENT_REQUEST_FENCE'),
 @('bool requireManualRally = false','bool requireManualRally = true','SHARED_MEASURE_PREDICATE_AUTO_DEFAULT')
)){
 if(!$spawner.Contains($mutation[0])){throw "Mutation target missing: $($mutation[0])"}
 if($mutation[2] -notin @(Rules ($spawner.Replace($mutation[0],$mutation[1])) $flow)){throw "Spawner mutation survived: $($mutation[2])"};$controls++
}
foreach($mutation in @(
 @('plan.RequiresManualRally() == (trip.GetVehiclePreset() != null)','true','CAPTURED_MANUAL_SCOPE_CURRENT_REQUEST_FENCE'),
 @('if (plan && plan.RequiresManualRally()) return false;',';','MANUAL_NO_PHYSICAL_RELOCATION'),
 @('trip.GetVehiclePreset() != null);','false);','CAPTURED_MANUAL_SCOPE_CURRENT_REQUEST_FENCE')
)){
 if(!$flow.Contains($mutation[0])){throw "Mutation target missing: $($mutation[0])"}
 if($mutation[2] -notin @(Rules $spawner ($flow.Replace($mutation[0],$mutation[1])))){throw "Flow mutation survived: $($mutation[2])"};$controls++
}
$body=Body $spawner 'IsStagingGeometryReady'
$clearance=[regex]::Match($spawner,'VEHICLE_CLEARANCE_RADIUS_METERS = ([0-9.]+);').Groups[1].Value
$margin=[regex]::Match($spawner,'REQUESTING_GROUP_CLEARANCE_MARGIN_METERS = ([0-9.]+);').Groups[1].Value
if(!$clearance -or !$margin){throw 'Production clearance constants missing'}
$adapter=@'
public static class AICF_ManualRallyAdapter {
 const float VEHICLE_CLEARANCE_RADIUS_METERS = CLEARANCE_VALUEf;
 const float REQUESTING_GROUP_CLEARANCE_MARGIN_METERS = MARGIN_VALUEf;
 public static bool Evaluate(float stagingDistanceMeters,float stagingRadiusMeters,float padDistanceMeters,float boardingEnvelopeRadiusMeters,bool requireManualRally) BODY
}
'@
$adapter=$adapter.Replace('CLEARANCE_VALUE',$clearance).Replace('MARGIN_VALUE',$margin).Replace('BODY',$body)
if(!('AICF_ManualRallyAdapter' -as [type])){Add-Type -TypeDefinition $adapter}
foreach($case in @(
 @(44.7784,25,87.7719,90,$false,$true,'M9 historical auto envelope preserved'),
 @(44.7784,25,87.7719,90,$true,$false,'M9 manual premature staging rejected'),
 @(25,25,90,90,$true,$true,'manual exact rally and pad boundary'),
 @(25.001,25,20,90,$true,$false,'manual just outside rally'),
 @(25.001,25,20,90,$false,$true,'auto ignores rally distance'),
 @(0,25,9.999,90,$true,$false,'spawn clearance remains strict'),
 @(0,25,10,90,$true,$true,'exact spawn clearance'),
 @(0,25,90.001,90,$false,$false,'auto boarding envelope unchanged'),
 @(0,25,90.001,90,$true,$false,'manual outer boarding envelope unchanged'),
 @(0,0,20,90,$true,$false,'invalid rally radius'),
 @([float]::NaN,25,20,90,$true,$false,'unknown manual distance fails closed')
)){
 $actual=[AICF_ManualRallyAdapter]::Evaluate($case[0],$case[1],$case[2],$case[3],$case[4])
 if($actual -ne $case[5]){throw "Manual rally predicate failed: $($case[6]) actual=$actual"};$controls++
}
"PASS Manual vehicle rally: actual production geometry rejects premature manual staging, retains old auto envelope, shares approach/commit/member policy, fences request scope, forbids manual physical relocation; controls=$controls"
