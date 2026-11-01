param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$trace=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleHealthTrace.c'))
$groups=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Diagnostics/AICF_GroupTrace.c'))
$transit=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleTransitFlow.c'))
function Body([string]$source,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$source;Code=(ConvertTo-AICFCodeText $source)}) $name}
function Rules([string]$source,[string]$groupSource,[string]$transitSource){
 $capture=Body $source 'Capture';$current=Body $source 'IsCurrent';$zones=Body $source 'CaptureZones';$code=ConvertTo-AICFCodeText $source
 if(!$capture -or !$current -or !$zones){'MISSING_METHODS';return}
 if($code -match '\b(?:EnableDamageHandling|SetMovementDamage|SetHealth|SetHealthScaled|SetEngineEfficiency|SetGearboxEfficiency|SetWheelsEfficiency|SetVelocity|SetAngularVelocity|SetOrigin|SetCruiseSpeed|SetCurrentAction|CallLater|Insert)\s*\(' -or $code -match 'static\s+(?:ref\s+)?(?:Vehicle|IEntity|Physics|SCR_\w+Component)\s+\w+\s*[;=]'){'READ_ONLY_WITHOUT_RETAINED_ENGINE_STATE'}
 foreach($fence in @('if (!Replication.IsServer())','AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(vehicle)','rpl.Id().ToString()','if (!damage)','if (!defaultZone)','IsCurrent(vehicle, vehicleId, rpl, componentRpl, damage, defaultZone, physics)')){if(!$capture.Contains($fence)){'CAPTURE_AUTHORITY_AND_IDENTITY';break}}
 foreach($fence in @('if (!Replication.IsServer()','vehicle.GetID() != vehicleId','vehicle.FindComponent(RplComponent) != rpl','rpl.Id().ToString() != componentRpl','!damage || !defaultZone','SCR_DamageManagerComponent.GetDamageManager(vehicle) != damage','damage.GetDefaultHitZone() != defaultZone','vehicle.GetPhysics() != physics')){if(!$current.Contains($fence)){'REPEATED_READ_CONTEXT_FENCE';break}}
 if($capture.IndexOf('IsCurrent(vehicle,') -gt $capture.IndexOf('health_status=READ_ONLY')){'PUBLISH_AFTER_FENCE'}
 foreach($read in @('SCR_AIVehicleUsability.VehicleCanMove(vehicle, damage)','SCR_AIVehicleUsability.VehicleIsOnFire(vehicle, damage)','damage.GetMovementDamage()','damage.IsDamageHandlingEnabled()','defaultZone.GetHealthScaled()','defaultZone.GetDamageState()','vehicleDamage.GetEngineFunctional()','vehicleDamage.GetEngineEfficiency()','vehicleDamage.GetGearboxFunctional()','vehicleDamage.GetGearboxEfficiency()','wheeledDamage.GetWheelsEfficiency()')){if(!$capture.Contains($read)){'NATIVE_LEAF_READBACK';break}}
 foreach($read in @('vehicle.GetOrigin()','vehicle.GetYawPitchRoll()','physics.GetVelocity()','physics.GetAngularVelocity()','GetSurfaceY(position[0], position[2])','position[1] - terrainY','health_cause_proven=0')){if(!$capture.Contains($read)){'RAW_PHYSICS_WITHOUT_CAUSE_CLAIM';break}}
 foreach($read in @('damage.GetAllHitZonesInHierarchy(zones)','index < zones.Count() && index < 64','zone.GetDamageState()','zone.GetHealthScaled()','zone.GetHitZoneContainer()','zoneDamage.IsDamageHandlingEnabled()','scriptedDamage.IsOnFire(zone)','worstOwner = "NONE";','health_zones_truncated','health_child_zones_damage_enabled')){if(!$zones.Contains($read)){'BOUNDED_HIERARCHY_OBSERVATION';break}}
 $snapshot=Body $groupSource 'Snapshot';$groupUpdate=Body $groupSource 'Update';$failure=Body $transitSource 'ReportVehicleFailureSnapshot'
 if(!$snapshot.Contains('if (vehicle) details += " " + AICF_VehicleHealthTrace.Capture(vehicle);') -or $snapshot.IndexOf('AICF_VehicleHealthTrace.Capture(vehicle)') -gt $snapshot.IndexOf('Info("GROUP_TRACE"') -or !$groupUpdate.Contains('if (now < cursor.m_iNextMs) continue;')){'PERIODIC_GROUP_HOOK_WITH_EXISTING_CADENCE'}
 if(!$failure.Contains('details += " " + AICF_VehicleHealthTrace.Capture(vehicle);') -or $failure.IndexOf('AICF_VehicleHealthTrace.Capture(vehicle)') -gt $failure.IndexOf('Warning("VEHICLE_FAILURE_SNAPSHOT"')){'FAILURE_HOOK_BEFORE_WARNING'}
}
$issues=@(Rules $trace $groups $transit)
if($issues.Count){throw "Vehicle health trace contracts: $issues"}
$controls=1
foreach($mutation in @(
 @('if (!Replication.IsServer())','if (false)','CAPTURE_AUTHORITY_AND_IDENTITY'),
 @('vehicle.GetID() != vehicleId','false','REPEATED_READ_CONTEXT_FENCE'),
 @('vehicle.FindComponent(RplComponent) != rpl','false','REPEATED_READ_CONTEXT_FENCE'),
 @('rpl.Id().ToString() != componentRpl','false','REPEATED_READ_CONTEXT_FENCE'),
 @('if (!damage || !defaultZone) return false;','if (false) return false;','REPEATED_READ_CONTEXT_FENCE'),
 @('SCR_DamageManagerComponent.GetDamageManager(vehicle) != damage','false','REPEATED_READ_CONTEXT_FENCE'),
 @('damage.GetDefaultHitZone() != defaultZone','false','REPEATED_READ_CONTEXT_FENCE'),
 @('vehicle.GetPhysics() != physics','false','REPEATED_READ_CONTEXT_FENCE'),
 @('damage.GetMovementDamage()','0','NATIVE_LEAF_READBACK'),
 @('damage.IsDamageHandlingEnabled()','false','NATIVE_LEAF_READBACK'),
 @('SCR_AIVehicleUsability.VehicleIsOnFire(vehicle, damage)','false','NATIVE_LEAF_READBACK'),
 @('vehicleDamage.GetEngineFunctional()','true','NATIVE_LEAF_READBACK'),
 @('vehicleDamage.GetGearboxEfficiency()','1','NATIVE_LEAF_READBACK'),
 @('wheeledDamage.GetWheelsEfficiency()','1','NATIVE_LEAF_READBACK'),
 @('physics.GetAngularVelocity()','vector.Zero','RAW_PHYSICS_WITHOUT_CAUSE_CLAIM'),
 @('health_cause_proven=0','health_cause_proven=1','RAW_PHYSICS_WITHOUT_CAUSE_CLAIM'),
 @('index < zones.Count() && index < 64','index < zones.Count() && index < 256','BOUNDED_HIERARCHY_OBSERVATION'),
 @('zoneDamage.IsDamageHandlingEnabled()','false','BOUNDED_HIERARCHY_OBSERVATION')
)){
 if(!$trace.Contains($mutation[0])){throw "Mutation target absent: $($mutation[2])"}
 if($mutation[2] -notin @(Rules ($trace.Replace($mutation[0],$mutation[1])) $groups $transit)){throw "Mutation survived: $($mutation[2])"}
 $controls++
}
$capture=Body $trace 'Capture'
if('READ_ONLY_WITHOUT_RETAINED_ENGINE_STATE' -notin @(Rules ($trace.Replace($capture,$capture.Insert(1,'damage.SetMovementDamage(0);'))) $groups $transit)){throw 'Gameplay mutation survived'}
$controls++
if('PERIODIC_GROUP_HOOK_WITH_EXISTING_CADENCE' -notin @(Rules $trace ($groups.Replace('if (vehicle) details += " " + AICF_VehicleHealthTrace.Capture(vehicle);','')) $transit)){throw 'Omitted periodic health hook survived'}
$controls++
if('FAILURE_HOOK_BEFORE_WARNING' -notin @(Rules $trace $groups ($transit.Replace('details += " " + AICF_VehicleHealthTrace.Capture(vehicle);','')))){'Omitted failure health hook survived';exit 1}
$controls++
"PASS Vehicle health trace: native leaf readback, bounded hierarchy, readonly pose/terrain, repeated identity, periodic/failure hooks; controls=$controls"
