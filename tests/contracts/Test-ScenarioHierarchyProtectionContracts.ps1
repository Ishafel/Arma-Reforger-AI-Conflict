param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleScenarioProbe.c'))
$runner=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleScenarios.ps1'))
function Body($s,$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
function Rules($s,$r){
 $collect=Body $s 'Collect';$apply=Body $s 'Apply';$verify=Body $s 'Verify';$readback=Body $s 'AICF_VSProtectionReadback'
 foreach($part in @('root.GetAllHitZonesInHierarchy(zones)','nativeCount != zones.Count()','zones.Count() > 512','!zone','zone.GetHitZoneContainer()','manager.GetOwner()','owner.GetWorld() != entity.GetWorld()','owner.FindComponent(DamageManagerComponent) != manager','!UnderRoot(root, manager)','managers.Contains(manager)')){if(!$collect.Contains($part)){'NATIVE_COMPLETE_EXACT_HIERARCHY';break}}
 foreach($part in @('!Replication.IsServer()','scenario != "repeat5"','retained != "1"','combat == "1"','SelectedScope() != "HIERARCHY"','entity.GetWorld() != GetGame().GetWorld()','!rpl.IsMaster()','rpl.IsProxy()','root.GetOwner() != entity','manager.GetOwner() != owners[index]','manager.EnableDamageHandling(false)','repeatedZones.Contains(originalZone)','repeatedOwners[repeatedIndex] != owners[managerIndex]','entity.GetID() != identity','entity.FindComponent(RplComponent) != rpl','enabledAfter == 0')){if(!$apply.Contains($part)){'CURRENT_SERVER_SINGLE_SETUP_REPEATED_NATIVE_READBACK';break}}
 foreach($part in @('AICF_VehicleBoardingMutationFence.IsAuthoritativeReplicatedEntity(entity)','Collect(entity, root, zones, managers, owners, nativeCount, reason)','manager.IsDamageHandlingEnabled()','container.IsDamageHandlingEnabled()','enabled != 0 || enabledChildZones != 0','read_only=1')){if(!$verify.Contains($part)){'READ_ONLY_ACTUAL_MANAGER_AND_CHILD_ZONE_COVERAGE';break}}
 if((ConvertTo-AICFCodeText $verify) -match 'EnableDamageHandling|SetHealth|SetLifeState|Teleport|SetOrigin'){ 'NO_REPROTECTION_OR_HEALTH_REPAIR_AFTER_SETUP' }
 foreach($part in @('AICF_VSOwnRecoveryRoster(slot)','m_iVSStep != 0 || m_iVSLeg != 1','trip.GetMovementState().GetRouteGeneration() != 0','m_bVSHierarchyVehicleApplied = true','AICF_VSHierarchyProtection.Apply(vehicle, setup)','AICF_VSHierarchyProtection.Verify(vehicle, vehicleReadback)','vehicle != m_VSHierarchyVehicle','vehicle.GetID() != m_VSHierarchyVehicleId','ComponentRpl(vehicle) != m_sVSHierarchyVehicleRpl')){if(!$readback.Contains($part)){'ONE_ORIGINAL_VEHICLE_BEFORE_ROUTE_NO_REAPPLICATION';break}}
 if($readback.IndexOf('m_bVSHierarchyVehicleApplied = true') -gt $readback.IndexOf('AICF_VSHierarchyProtection.Apply(vehicle, setup)')){'CONSUMED_BEFORE_NATIVE_SETUP'}
 if($s -notmatch 'SCENARIO_RETAINED_TERMINAL' -or $s.IndexOf('"RETAINED_TERMINAL_"') -gt $s.IndexOf('"RETAINED_IDENTITY_LOST"')){'TERMINAL_DIAGNOSIS_PRECEDES_IDENTITY_FAILURE'}
 if(!$r.Contains("[ValidateSet('ROOT_ONLY','HIERARCHY')]") -or !$r.Contains("'HIERARCHY requires explicit isolated RetainedBehavior repeat5'") -or !$r.Contains('-ProtectionScope $ProtectionScope')){'EXPLICIT_CONTROLLED_RUNNER_SCOPE'}
 if((ConvertTo-AICFCodeText ($apply+$verify+$readback)) -match '\b(?:SetHealth|SetHealthScaled|SetLifeState|ForceUnconsciousness|Kill|Teleport|SetOrigin)\s*\('){'NO_HEAL_REVIVE_OR_MOVEMENT_MUTATION'}
}
$bad=@(Rules $source $runner);if($bad.Count){throw "Hierarchy protection source failed: $bad"}
$mutants=@('nativeCount != zones.Count()','zones.Count() > 512','owner.FindComponent(DamageManagerComponent) != manager','!UnderRoot(root, manager)','scenario != "repeat5"','retained != "1"','SelectedScope() != "HIERARCHY"','!rpl.IsMaster()','repeatedZones.Contains(originalZone)','entity.FindComponent(RplComponent) != rpl','enabledAfter == 0','enabled != 0 || enabledChildZones != 0','trip.GetMovementState().GetRouteGeneration() != 0','m_bVSHierarchyVehicleApplied = true')
foreach($m in $mutants){if(!$source.Contains($m)){throw "Stale hierarchy mutant $m"};if(!@(Rules ($source.Replace($m,'false')) $runner).Count){throw "Escaped hierarchy mutant $m"}}
"PASS Scenario hierarchy: $($mutants.Count) native-tree/authority/one-shot/readonly/before-route source mutants; actual runtime receipts required"
