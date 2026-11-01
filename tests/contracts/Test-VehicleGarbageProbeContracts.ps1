param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleGarbageProbe.c'))
function Body($s,$m){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $m}
function Rules($s){
 foreach($name in @('REAL_FLEET_ACTIVE_BIND','PRELEASE_NATIVE_TTL_FIVE','OWNED_DELETE_VETO','OWNED_SURVIVES_NATIVE_TTL','OWNED_TRACKING_ENDED','OWNED_INSERT_VETO','RELEASE_PENDING_PROTECTED','FAILED_CLOSED_PROTECTED','STALE_GROUP_GENERATION_REJECTED','STALE_TRIP_GENERATION_REJECTED','STALE_LEASE_GENERATION_REJECTED','STALE_VEHICLE_GENERATION_REJECTED','FAKE_ENTITY_REJECTED','LOCAL_NATIVE_AUTHORITY_READBACK','RELEASE_TO_WORLD_POOL','RELEASED_NATIVE_TRACKING_RESTORED','RELEASED_NO_PERMISSION','RELEASED_NATIVE_TTL_DELETE','UNOWNED_NATIVE_TTL_DELETE','STOCK_CALLBACKS_PRESERVED','LEASE_TEARDOWN_TRACKING_RESTORED')){if(([regex]::Matches($s,[regex]::Escape('"'+$name+'"'))).Count -ne 1){'MANDATORY_NATIVE_CASES';break}}
 if(!$s.Contains('aicfVehicleGarbageProbe') -or !$s.Contains('flag == "1"')){'EXPLICIT_NATIVE_OPT_IN'}
 foreach($part in @('garbage.Insert(m_GCPrimary, 5)','garbage.Insert(m_GCPrimary, 1)','garbage.Insert(m_GCTeardown, 5)','garbage.GetLifetime(m_GCPrimary) == 5','garbage.GetLifetime(m_GCTeardown) == 5','m_iGCPhase == 1 && AICF_VehicleGarbageProbeState.s_iDeleteVetoes >= 2','s_iDeleteVetoes == 2','permission.EndedTracking() && !garbage.IsInserted(m_GCPrimary)','AICF_VehicleGarbageProbeState.s_iOwnedInsertVetoes > m_iGCOwnedInsertBaseline && !garbage.IsInserted(m_GCPrimary)')){if(!$s.Contains($part)){'ACTUAL_NATIVE_TTL_INSERT_AND_DELETE_VETO';break}}
 foreach($part in @('m_GCPrimaryLease.BeginRelease(true','m_GCPrimaryLease.FailClosed()','permission.CanProtect(m_GCPrimary)','m_GCPrimaryFleet.ReleaseRetainedLeaseAt(','released.GetVehicle() == m_GCPrimary','garbage.GetLifetime(m_GCPrimary) > 0 && garbage.GetRemainingLifetime(m_GCPrimary) > 0')){if(!$s.Contains($part)){'CAP_HELD_PENDING_FAILED_RELEASE_RESTORE';break}}
 foreach($part in @('m_iGroup++','m_iGroup--','m_iTrip++','m_iTrip--','m_iLease++','m_iLease--','m_iVehicle++','m_iVehicle--','bool rejected = !CanProtect(m_Vehicle)','return rejected && CanProtect(m_Vehicle)')){if(!(Body $s 'AICF_GCProbeStale').Contains($part)){'ACTUAL_OBJECT_STALE_RESTORED_PREDICATES';break}}
 foreach($part in @('protected ref AICF_FactionFleet m_GCTeardownFleet','protected AICF_VehicleLease m_GCTeardownLease','m_GCTeardownLease = null','m_GCTeardownFleet = null','!AICF_VehicleGarbagePolicy.Find(m_GCTeardown) && garbage.IsInserted(m_GCTeardown)')){if(!$s.Contains($part)){'REAL_SOLE_FLEET_OWNER_DESTRUCTION';break}}
 if($s -match 'protected ref AICF_VehicleLease m_GCTeardownLease'){'NO_EXTRA_STRONG_LEASE_DURING_TEARDOWN'}
 foreach($part in @('m_iGCPhase == 3 && !m_GCPrimary && !m_GCControl','!GetGame().GetWorld().FindEntityByID(m_GCPrimaryId)','!GetGame().GetWorld().FindEntityByID(m_GCControlId)','s_iStockInserts >= 2 && AICF_VehicleGarbageProbeState.s_iStockDeletes == 2')){if(!$s.Contains($part)){'ACTUAL_UNOWNED_NATIVE_DELETE_AND_STOCK_CALLBACKS';break}}
 if(!$s.Contains('now - m_iGCStarted >= 180000') -or !$s.Contains('Check("TIMEOUT", false)') -or !(Body $s 'AICF_GCFinish').Contains('Stop(true)') -or !(Body $s 'AICF_GCClose').Contains('GetGame().RequestClose()') -or !(Body $s 'Stop').Contains('Remove(AICF_GCClose)')){'BOUNDED_FULL_STOP_LIFECYCLE'}
 if(!$s.Contains('m_iGCPhase == -1 && now - m_iGCStarted >= 2000') -or !$s.Contains('m_iGCHeartbeatAt = now + 5000') -or !$s.Contains('[AICF][VEHICLE_GC_HEARTBEAT]')){'SETTLED_NATIVE_SPAWN_AND_HONEST_WAIT_TELEMETRY'}
 foreach($part in @('AICF_GCBind(noAuthorityFleet, m_GCNoAuthority, 9, noAuthorityLease, "FORGED_NO_NATIVE_RPL")','noAuthorityLease.GetVehicle() == m_GCNoAuthority','!noAuthorityLease.AICF_GCProbePermission()')){if(!$s.Contains($part)){'EXACT_FORGED_NO_AUTHORITY_NATIVE_BINDING';break}}
 foreach($part in @('float accepted = super.OnInsertRequested(entity, lifetime)','lifetime == 5 && accepted > 0','[AICF][VEHICLE_GC_NATIVE_INSERT]','m_iGCPhase == -2 && now > m_iGCPhaseAt','s_iPrimaryNativeInsert > 0 && AICF_VehicleGarbageProbeState.s_iTeardownNativeInsert > 0','garbage.GetRemainingLifetime(m_GCPrimary) > 1','garbage.GetRemainingLifetime(m_GCTeardown) > 1','m_iGCPhase == 4 && now - m_iGCPhaseAt >= 1000')){if(!$s.Contains($part)){'COMMITTED_NATIVE_INSERT_BEFORE_BIND_AND_DEFERRED_VETO_ORACLE';break}}
 if(!$s.Contains('m_Vehicle == AICF_VehicleGarbageProbeState.s_Primary || m_Vehicle == AICF_VehicleGarbageProbeState.s_Teardown')){'EXACT_OWNED_NATIVE_VETO_COUNTERS'}
 foreach($part in @('s_iReleasedStockCommits > 0','entity == AICF_VehicleGarbageProbeState.s_Released && accepted > 0','SpawnEntityPrefabLocal(localResource','[AICF][VEHICLE_GC_BARE_AUTHORITY_SETUP]','native_master=','native_proxy=','native_role=')){if(!$s.Contains($part)){'COMMITTED_DEFAULT_RESTORE_AND_REAL_AUTHORITY_DIAGNOSTICS';break}}
 if((ConvertTo-AICFCodeText $s) -match '\b(?:Flush|UpdateBlacklist|SetHealth|SetHealthScaled|SetLifeState|Teleport|SetOrigin)\s*\('){'NO_GLOBAL_GC_OR_HEALTH_OR_TELEPORT'}
}
$bad=@(Rules $source);if($bad.Count){throw($bad -join ',')}
$mutants=@(
 @('s_iReleasedStockCommits > 0','true'),@('GetGame().SpawnEntityPrefabLocal(localResource','GetGame().SpawnEntityPrefab(localResource'),@('m_iGCPhase == -1 && now - m_iGCStarted >= 2000','m_iGCPhase == -1'),@('m_iGCHeartbeatAt = now + 5000','m_iGCHeartbeatAt = now + 999999'),@('noAuthorityLease.GetVehicle() == m_GCNoAuthority','true'),@('!noAuthorityLease.AICF_GCProbePermission()','true'),@('m_iGCPhase == -2 && now > m_iGCPhaseAt','m_iGCPhase == -2'),@('lifetime == 5 && accepted > 0','true'),@('garbage.GetRemainingLifetime(m_GCPrimary) > 1','true'),@('m_iGCPhase == 4 && now - m_iGCPhaseAt >= 1000','m_iGCPhase == 4'),@('m_Vehicle == AICF_VehicleGarbageProbeState.s_Primary || m_Vehicle == AICF_VehicleGarbageProbeState.s_Teardown','true'),@('garbage.Insert(m_GCPrimary, 5)','true'),@('garbage.Insert(m_GCPrimary, 1)','true'),@('garbage.Insert(m_GCTeardown, 5)','true'),
 @('s_iDeleteVetoes >= 2','s_iDeleteVetoes >= 0'),@('s_iDeleteVetoes == 2','true'),
 @('permission.EndedTracking() && !garbage.IsInserted(m_GCPrimary)','true'),
 @('m_GCPrimaryLease.BeginRelease(true','m_GCPrimaryLease.BeginRelease(false'),
 @('m_GCPrimaryLease.FailClosed()',''),@('m_GCPrimaryFleet.ReleaseRetainedLeaseAt(','m_GCPrimaryFleet.ReleaseLeaseAt('),
 @('m_iGroup++',''),@('m_iTrip++',''),@('m_iLease++',''),@('m_iVehicle++',''),
 @('m_GCTeardownFleet = null','m_GCTeardownFleet = m_GCPrimaryFleet'),
 @('protected AICF_VehicleLease m_GCTeardownLease','protected ref AICF_VehicleLease m_GCTeardownLease'),
 @('m_iGCPhase == 3 && !m_GCPrimary && !m_GCControl','m_iGCPhase == 3'),
 @('!GetGame().GetWorld().FindEntityByID(m_GCPrimaryId)','true'),
 @('s_iStockDeletes == 2','s_iStockDeletes >= 0'),@('>= 180000','>= 9999999'),
 @('Check("TIMEOUT", false)','Check("TIMEOUT", true)'),@('Stop(true)',''),@('GetGame().RequestClose()','')
)
foreach($m in $mutants){if(!$source.Contains($m[0])){throw "Stale mutant $($m[0])"};if(!@(Rules $source.Replace($m[0],$m[1])).Count){throw "Escaped mutant $($m[0])"}}
"PASS Vehicle garbage native fixture source contracts and $($mutants.Count) negative mutants"
