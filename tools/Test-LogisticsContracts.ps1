param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$evidence = Join-Path $RepositoryRoot '.codex-runtime/logistics-contracts'
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$fixture = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'tools/fixtures/logistics/physical-delivery.fixture') -Raw
$logAudit = Join-Path $RepositoryRoot 'tools/Test-LogisticsLog.ps1'
$checks = 0
function Check-Log([string]$name, [string]$text, [int]$expected, [string]$rule) {
    $path = Join-Path $evidence "$name.fixture"
    [IO.File]::WriteAllText($path, $text)
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $logAudit -LogPath $path -RequireDelivery -RequirePolicy -RequireLedger -RequireGraph -RequireSearch 2>&1
    if ($LASTEXITCODE -ne $expected -or ($rule -and ($result -join "`n") -notmatch $rule)) { throw "Contract $name unexpected result: $result" }
    $script:checks++
}
Check-Log 'physical-fractional-partial-release' $fixture 0 ''
Check-Log 'nonfinite' ($fixture.Replace('loaded=100.125','loaded=NaN')) 1 'NONFINITE'
Check-Log 'double-release-hidden-zero' ($fixture.Replace('released=25.125','released=50.25')) 1 'BALANCE_CONSERVATION'
Check-Log 'unmatched-pair' ($fixture.Replace('from_after=800.375','from_after=800')) 1 'PAIR_CONSERVATION'
Check-Log 'pending-discrepancy' ($fixture.Replace('discrepancy=0','discrepancy=0.5')) 1 'DISCREPANCY'
Check-Log 'wrong-generation' ($fixture.Replace('phase=LOADING operation','phase=LOADING generation=2 operation')) 1 'WORK_BEFORE_EXACT_READY'
$load = ($fixture -split "`n" | Where-Object { $_ -match 'LOGISTICS_LOAD_COMMITTED' }) -join "`n"
Check-Log 'duplicate-operation' ($fixture.Replace('ENGINE : Game destroyed.', $load + "`nENGINE : Game destroyed.")) 1 'DUPLICATE_OPERATION'
Check-Log 'late-transfer' ($fixture.Replace('[AICF][STAGE4][INFO][LOGISTICS_JOB_RESERVED]', "[AICF][STAGE4][INFO][LOGISTICS_STOP]`n[AICF][STAGE4][INFO][LOGISTICS_JOB_RESERVED]")) 1 'WORK_AFTER_STOP'
Check-Log 'active-fragment' ($fixture.Replace('ENGINE : Game destroyed.','')) 1 'FULL_STOPPED_LOG_REQUIRED'
Check-Log 'missing-roster' ($fixture.Replace('[STAGE1][INFO][ROSTER_READY]','[STAGE1][INFO][ROSTER_PENDING]')) 1 'WORK_BEFORE_ROSTER_READY'
Check-Log 'unknown-resource' ($fixture.Replace('discrepancy=0','discrepancy=0 unknown_state=1')) 1 'UNKNOWN_RESOURCE_STATE'
Check-Log 'unknown-custody-with-zero-balance' ($fixture.Replace('discrepancy=0','discrepancy=0 fault=1')) 1 'CARGO_CUSTODY_FAULT'
Check-Log 'ledger-contract-missing' ($fixture.Replace('passed=18 total=18','passed=17 total=18')) 1 'PRODUCTION_LEDGER_CONTRACT_NOT_OBSERVED'
Check-Log 'graph-contract-missing' ($fixture.Replace('passed=12 total=12','passed=11 total=12')) 1 'PRODUCTION_GRAPH_CONTRACT_NOT_OBSERVED'
Check-Log 'search-contract-missing' ($fixture.Replace('passed=6 total=6','passed=5 total=6')) 1 'PRODUCTION_SEARCH_CONTRACT_NOT_OBSERVED'
Check-Log 'native-bind-error' ($fixture.Replace('ENGINE : Game destroyed.','NETWORK (E): Unable to start replication')) 1 'ENGINE_OR_AICF_ERROR'
Check-Log 'self-transfer' ($fixture.Replace('from_pool=A to_pool=V','from_pool=V to_pool=V')) 1 'SELF_TRANSFER'

# Синтетические logs проверяют анализатор, а не фактическое открытие шлагбаума.
$waitIdentity = 'run=fixture faction=US slot=1000000 generation=1 vehicle=V1 driver=D1 phase=TO_SOURCE job=J1 wait_job=J1 wait_generation=1 wait_group=G1 wait_vehicle=V1 wait_driver=D1 target=T1 wait_vehicle_rpl=VR1 wait_driver_rpl=DR1'
$waitStart = "[AICF][STAGE4][INFO][LOGISTICS_DRIVER_INTERACTION_STARTED] $waitIdentity t_ms=3100 elapsed_ms=0 reason=NATIVE_OPEN_GATE"
$waitReturn = "[AICF][STAGE4][INFO][LOGISTICS_DRIVER_INTERACTION_RETURNED] $waitIdentity t_ms=3900 elapsed_ms=800 leg_wait_ms=800 exact_return=1 reason=EXACT_DRIVER_RETURNED"
$waitFailure = $waitReturn.Replace('INTERACTION_RETURNED','INTERACTION_FAILED').Replace('EXACT_DRIVER_RETURNED','DRIVER_INTERACTION_NO_PROGRESS')
$loadMarker = '[AICF][STAGE4][INFO][LOGISTICS_LOAD_COMMITTED]'
$waitFixture = $fixture.Replace($loadMarker, "$waitStart`n$waitReturn`n$loadMarker")
Check-Log 'gate-return-delivery' $waitFixture 0 ''
Check-Log 'gate-transfer-while-absent' ($fixture.Replace($loadMarker, "$waitStart`n$loadMarker")) 1 'TRANSFER_DURING_DRIVER_INTERACTION'
Check-Log 'gate-return-wrong-driver' ($waitFixture.Replace($waitReturn, $waitReturn.Replace('driver=D1 phase','driver=D2 phase'))) 1 'DRIVER_INTERACTION_RETURN_IDENTITY'
Check-Log 'gate-rearmed-wait' ($waitFixture.Replace($waitStart, "$waitStart`n$waitStart")) 1 'DRIVER_INTERACTION_START'
Check-Log 'gate-hard-timeout-return' ($waitFixture.Replace('elapsed_ms=800','elapsed_ms=120000')) 1 'DRIVER_INTERACTION_RETURN'
Check-Log 'gate-terminal-then-transfer' ($waitFixture.Replace($waitReturn,$waitFailure)) 1 'TRANSFER_AFTER_DRIVER_INTERACTION_FAILURE'
Check-Log 'unknown-exit-return' ($waitFixture.Replace('reason=NATIVE_OPEN_GATE','reason=EXIT_CAUSE_UNRECOGNIZED')) 0 ''
Check-Log 'unknown-exit-timeout-return' ($waitFixture.Replace('reason=NATIVE_OPEN_GATE','reason=EXIT_CAUSE_UNRECOGNIZED').Replace('elapsed_ms=800','elapsed_ms=60000')) 1 'UNKNOWN_DRIVER_RECOVERY_DEADLINE'
$recoveryIdentity = 'run=fixture faction=US slot=1000000 generation=1 vehicle=V1 driver=D1 phase=TO_SOURCE job=J1'
$attemptLog = "[AICF][STAGE4][INFO][LOGISTICS_RECOVERY_ATTEMPT] $recoveryIdentity attempt=1 t_ms=3100"
$successLog = "[AICF][STAGE4][INFO][LOGISTICS_RECOVERY_SUCCEEDED] $recoveryIdentity attempt=1 t_ms=3900 displacement_m=7 route_progress_m=4"
$recoveryFixture = $fixture.Replace($loadMarker, "$attemptLog`n$successLog`n$loadMarker")
Check-Log 'recovery-same-vehicle-delivery' $recoveryFixture 0 ''
Check-Log 'recovery-without-motion' ($recoveryFixture.Replace('displacement_m=7','displacement_m=0')) 1 'RECOVERY_NO_PHYSICAL_EVIDENCE'
Check-Log 'recovery-without-route-progress' ($recoveryFixture.Replace('route_progress_m=4','route_progress_m=0')) 1 'RECOVERY_NO_PHYSICAL_EVIDENCE'
Check-Log 'recovery-wrong-vehicle' ($recoveryFixture.Replace($successLog,$successLog.Replace('vehicle=V1','vehicle=V2'))) 1 'RECOVERY_IDENTITY'
Check-Log 'recovery-budget-overrun' ($recoveryFixture.Replace('attempt=1','attempt=3')) 1 'RECOVERY_ATTEMPT_BUDGET'
$fallbackIdentity = 'run=fixture faction=US slot=1000000 generation=1 vehicle=V1 driver=D1 phase=TO_DESTINATION job=J1 vehicle_rpl=VR1'
$fallbackStart = "[AICF][STAGE4][INFO][LOGISTICS_FALLBACK_STARTED] $fallbackIdentity attempt=1 t_ms=5000 relocate=1 cargo=100.125"
$fallbackSeat = "[AICF][STAGE4][INFO][LOGISTICS_DRIVER_TELEPORT_ISSUED] $fallbackIdentity attempt=1 t_ms=6000 issued=1 driver_rpl=DR1 cargo=100.125"
$fallbackMove = "[AICF][STAGE4][INFO][LOGISTICS_VEHICLE_RELOCATED] $fallbackIdentity attempt=1 t_ms=7000 displacement_m=30 cargo_before=100.125 cargo_after=100.125"
$fallbackResume = "[AICF][STAGE4][INFO][LOGISTICS_FALLBACK_RESUMED] $fallbackIdentity attempt=1 t_ms=7500 relocated=1 cargo=100.125"
$fallbackMotion = "[AICF][STAGE4][INFO][LOGISTICS_RECOVERY_SUCCEEDED] $fallbackIdentity attempt=0 t_ms=8500 displacement_m=7 route_progress_m=4"
$unloadMarker = '[AICF][STAGE4][INFO][LOGISTICS_UNLOAD_COMMITTED]'
$fallbackFixture = $fixture.Replace($unloadMarker, "$fallbackStart`n$fallbackSeat`n$fallbackMove`n$fallbackResume`n$fallbackMotion`n$unloadMarker")
Check-Log 'fallback-return-motion-delivery' $fallbackFixture 0 ''
Check-Log 'fallback-cargo-created' ($fallbackFixture.Replace('cargo_after=100.125','cargo_after=200.125')) 1 'FALLBACK_CARGO_CHANGED'
Check-Log 'fallback-wrong-driver' ($fallbackFixture.Replace($fallbackSeat,$fallbackSeat.Replace('driver=D1','driver=D2'))) 1 'FALLBACK_IDENTITY'
Check-Log 'fallback-no-movement' ($fallbackFixture.Replace($fallbackMotion,'')) 1 'FALLBACK_TRANSFER_BEFORE_MOTION'
Check-Log 'fallback-teleport-not-seat-proof' ($fallbackFixture.Replace($fallbackResume,'')) 1 'TRANSFER_DURING_FALLBACK'
Check-Log 'fallback-too-far' ($fallbackFixture.Replace('displacement_m=30','displacement_m=300')) 1 'FALLBACK_RELOCATION_BOUND'
Check-Log 'fallback-expired' ($fallbackFixture.Replace('t_ms=7500','t_ms=50000')) 1 'FALLBACK_DEADLINE'
Check-Log 'fallback-resume-without-teleport' ($fallbackFixture.Replace($fallbackMove,'')) 1 'FALLBACK_RELOCATION_PROOF'
Check-Log 'recovery-rearm' ($recoveryFixture.Replace($attemptLog,"$attemptLog`n$attemptLog")) 1 'RECOVERY_ATTEMPT_BUDGET'

$clientFixture = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'tools/fixtures/logistics/client-loaded.fixture') -Raw
$serverFixturePath = Join-Path $evidence 'client-server.fixture'
[IO.File]::WriteAllText($serverFixturePath, $fixture)
$clientCases = @(
    @('client-loaded', $clientFixture, 0, ''),
    @('client-wrong-rpl', $clientFixture.Replace('vehicle_rpl=VR1','vehicle_rpl=VR2'), 1, 'CLIENT_EXACT_IDENTITY'),
    @('client-wrong-generation', $clientFixture.Replace('generation=1','generation=2'), 1, 'CLIENT_EXACT_IDENTITY'),
    @('client-nonfinite', $clientFixture.Replace('cargo=100.125','cargo=NaN'), 1, 'NONFINITE'),
    @('client-empty-only', $clientFixture.Replace('cargo=100.125','cargo=0'), 1, 'LOADED_CLIENT_REPLICA_NOT_OBSERVED'),
    @('client-active-fragment', $clientFixture.Replace('ENGINE : Game destroyed.',''), 1, 'FULL_STOPPED_CLIENT_LOG_REQUIRED')
)
foreach ($case in $clientCases) {
    $clientPath = Join-Path $evidence ($case[0] + '.fixture')
    [IO.File]::WriteAllText($clientPath, $case[1])
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $logAudit -LogPath $serverFixturePath -ClientLogPath $clientPath -RequireDelivery -RequireLoadedClient 2>&1
    if ($LASTEXITCODE -ne $case[2] -or ($case[3] -and ($result -join "`n") -notmatch $case[3])) { throw "Client contract $($case[0]) unexpected result: $result" }
    $checks++
}

# Отрицательные source fixtures вызывают тот же production static auditor.
$sourceRoot = Join-Path $evidence 'source'
$relativeCore = 'AIConflictCore/Scripts/Game/AIConflict'
$originalCore = Join-Path $RepositoryRoot $relativeCore
$targetCore = Join-Path $sourceRoot $relativeCore
foreach ($directory in @('Config','Economy','Vehicles','State/Vehicles')) {
    [IO.Directory]::CreateDirectory((Join-Path $targetCore $directory)) | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $originalCore $directory) -Filter '*.c' -File | Copy-Item -Destination (Join-Path $targetCore $directory)
}
$mutations = @(
    @('Vehicles/AICF_LogisticsVehicleFootprint.c','float penetration = world.TracePosition(body, null);','body.ExcludeArray = {}; float penetration = world.TracePosition(body, null);','SPAWN_ALL_PHYSICAL_OBSTACLES'),
    @('Vehicles/AICF_LogisticsSpawnGeometry.c','world.TraceMove(trace, null) < 1','false','SPAWN_EXIT_SWEEP'),
    @('Vehicles/AICF_LogisticsSpawnGeometry.c','!FitToSurface(world, footprint, next)','false','SPAWN_EXIT_GROUND'),
    @('Vehicles/AICF_LogisticsSpawnGeometry.c','ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, waterPoint)','false','SPAWN_GROUND_WATER'),
    @('Vehicles/AICF_LogisticsSpawnGeometry.c','highest - lowest > 0.6','false','SPAWN_GROUND_SUPPORT'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','m_vRoute = w.m_vSpawnExit;','m_vRoute = w.m_vEndpoint;','SPAWN_FIRST_LEG_EXIT'),
    @('Vehicles/AICF_VehicleSpawner.c','currentTransform[axis] != w.m_aSlotTransform[axis]','false','SPAWN_SLOT_REVALIDATED'),
    @('Vehicles/AICF_VehicleSpawner.c','candidate < AICF_LogisticsSpawnGeometry.CANDIDATES','true','SPAWN_SEARCH_BOUNDED'),
    @('Vehicles/AICF_TransportTripController.c','observation.IsTerminal() && observation.GetKind() != AICF_ETripOutcomeKind.COMPLETE_TRIP','observation.IsTerminal()','DRIVER_SUCCESS_NOT_RETIREMENT'),
    @('Vehicles/AICF_LogisticsDriverRecovery.c','!access.GetCompartment() || access.GetCompartment() == m_Seat','true','UNKNOWN_EXIT_NO_OTHER_SEAT'),
    @('Vehicles/AICF_LogisticsDriverRecovery.c','w.m_Waypoint && !NativeBusy(true)','false','UNKNOWN_PAUSE_OWN_ROUTE'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','m_bIntermediate && !m_bSpawnEgress && !m_bActive && vector.DistanceXZ(position, m_vRoute) <= 8','m_bIntermediate && vector.DistanceXZ(position, m_vRoute) <= 8','RECOVERY_INTERMEDIATE_AFTER_EVIDENCE'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','m_vRoute = initialRoad;','m_vRoute = vector.Zero;','RECOVERY_QUERY_COMMIT_ON_SUCCESS'),
    @('Vehicles/AICF_LogisticsDriverRecovery.c','access.IsGettingIn() || (m_Seat.GetOccupant() == m_Driver && CompartmentAccessComponent.GetVehicleIn(m_Driver) == m_Vehicle)','false','UNKNOWN_RETURN_SETTLING'),
    @('Vehicles/AICF_LogisticsDriverRecovery.c','if (token != m_sToken) same = false;','// removed','UNKNOWN_RETURN_CALLBACK_TOKEN'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','w.m_iRouteRetries++;','w.m_iRouteRetries++; w.MarkProgress(now);','RECOVERY_WAYPOINT_NOT_PROGRESS'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','!m_Context || !m_Context.Matches(w)','false','RECOVERY_EXACT_CONTEXT'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','w.m_iRouteRetries >= AICF_LogisticsConfig.MAX_ROUTE_RETRIES','false','RECOVERY_BOUNDED_ATTEMPTS'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','RecoveryAge(w, now) >= AICF_LogisticsConfig.RECOVERY_BUDGET_MS','false','RECOVERY_HARD_BUDGET'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','vector.DistanceXZ(position, m_vAttemptPosition) >= 6','true','RECOVERY_PHYSICAL_EVIDENCE'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','vector.DistanceXZ(position, m_vRoute) + 3 <= m_fAttemptDistance','true','RECOVERY_ROUTE_EVIDENCE'),
    @('Vehicles/AICF_LogisticsFallback.c','!Replication.IsServer() || !Matches(w) || w.HasForeignOccupant(true)','false','FALLBACK_AUTHORITY_IDENTITY'),
    @('Vehicles/AICF_LogisticsAcquisitionFlow.c','m_Drivers.CreateBuilder(w.m_Faction, driverPosition)','m_Drivers.CreateBuilder(w.m_Faction, w.m_Vehicle.GetOrigin())','DRIVER_SPAWN_OUTSIDE_VEHICLE'),
    @('Vehicles/AICF_LogisticsFallback.c','w.m_CargoPool != m_CargoPool || !m_CargoPool || !m_CargoPool.Valid()','false','FALLBACK_CARGO_IDENTITY'),
    @('Vehicles/AICF_LogisticsFallback.c','m_Job && now >= m_Job.m_iExpiresAtMs','false','FALLBACK_TTL_NOT_RESURRECTED'),
    @('Vehicles/AICF_LogisticsFallback.c','w.m_iFallbackAttempts >= MAX_PER_LEG','false','FALLBACK_BOUNDED_LEG'),
    @('Vehicles/AICF_LogisticsFallback.c','now - m_iStartedAtMs >= TIMEOUT_MS','false','FALLBACK_BOUNDED_TIME'),
    @('Vehicles/AICF_LogisticsFallback.c','!linked || linked == m_Vehicle','true','FALLBACK_NO_OTHER_VEHICLE'),
    @('Vehicles/AICF_LogisticsFallback.c','!access.GetCompartment() || access.GetCompartment() == m_Seat','true','FALLBACK_NO_OTHER_SEAT'),
    @('Vehicles/AICF_VehicleBoardingFlow.c','!recovery.Safe(w, System.GetTickCount()) || w.HasForeignOccupant(true)','false','FALLBACK_DRIVER_RECHECK'),
    @('Vehicles/AICF_VehicleBoardingFlow.c','recovery.m_iSeatAttempts >= 3','false','FALLBACK_SEAT_RETRY_BOUND'),
    @('Vehicles/AICF_VehicleTransitFlow.c','!recovery.Safe(w, System.GetTickCount()) || !w.Ready() || w.HasForeignOccupant()','false','FALLBACK_TRANSFORM_RECHECK'),
    @('Vehicles/AICF_VehicleTransitFlow.c','!footprint.IsClear(w.m_Vehicle.GetWorld(), pose, body)','false','FALLBACK_DESTINATION_COLLISION'),
    @('Vehicles/AICF_VehicleTransitFlow.c','Math.AbsFloat(cargoBefore - cargoAfter) > AICF_LogisticsConfig.RESOURCE_EPSILON','false','FALLBACK_CARGO_READBACK'),
    @('Vehicles/AICF_VehicleTransitFlow.c','!AICF_LogisticsFallback.PlayersClear(source, pose[3])','false','FALLBACK_PLAYER_USE'),
    @('Vehicles/AICF_VehicleTransitFlow.c','!w.m_RouteRecovery.m_bAwaitingRelocationMotion && physics','physics','FALLBACK_JUMP_NOT_ARRIVAL'),
    @('Vehicles/AICF_LogisticsRouteRecovery.c','w.m_vProgressPosition = m_vAttemptPosition;','// removed','FALLBACK_JUMP_NOT_MOTION'),
    @('Economy/AICF_LogisticsLedger.c','now >= job.m_iExpiresAtMs','false','RECOVERY_TTL_NOT_RESURRECTED'),
    @('Vehicles/AICF_LogisticsDriverRecovery.c','now - m_iStartedAtMs >= AICF_LogisticsConfig.UNKNOWN_DRIVER_TIMEOUT_MS','false','UNKNOWN_EXIT_HARD_BUDGET'),
    @('Vehicles/AICF_LogisticsDriverRecovery.c','m_iWaitBeforeMs + now - m_iStartedAtMs >= AICF_LogisticsConfig.DRIVER_INTERACTION_LEG_BUDGET_MS','false','UNKNOWN_EXIT_TOTAL_BUDGET'),
    @('Vehicles/AICF_LogisticsDriverRecovery.c','access.IsInCompartment() || NativeBusy(true)','access.IsInCompartment()','UNKNOWN_EXIT_NATIVE_PRIORITY'),
    @('Vehicles/AICF_LogisticsDriverRecovery.c','if (w.HasForeignOccupant(true))','if (false)','UNKNOWN_EXIT_FOREIGN_OCCUPANT'),
    @('Vehicles/AICF_LogisticsDriverRecovery.c','if (w.Ready()) return AICF_TripOutcome.CompleteTrip','if (true) return AICF_TripOutcome.CompleteTrip','UNKNOWN_EXIT_EXACT_RETURN'),
    @('Economy/AICF_LogisticsExitHistory.c','failure.m_Slot.GetOwner().GetID() != failure.m_SlotId','false','EXIT_COOLDOWN_EXACT_IDENTITY'),
    @('Economy/AICF_LogisticsExitHistory.c','now >= failure.m_iUntilMs','false','EXIT_COOLDOWN_EXPIRES'),
    @('Economy/AICF_LogisticsExitHistory.c','w.m_iRouteRetries >= AICF_LogisticsConfig.MAX_ROUTE_RETRIES','true','EXIT_COOLDOWN_PROVEN_FAILURE'),
    @('Vehicles/AICF_VehicleSpawner.c','w.m_ExitHistory.Cooling(candidate, System.GetTickCount())','false','EXIT_COOLDOWN_SKIP'),
    @('Economy/AICF_LogisticsService.c','w.m_ExitHistory.AllCooling(w, now)','false','EXIT_COOLDOWN_WAIT_WITHOUT_SPAWN'),
    @('Vehicles/AICF_LogisticsVehicleFootprint.c','return penetration >= 0;','return true;','SPAWN_PHYSICS_PENETRATION'),
    @('Vehicles/AICF_LogisticsVehicleFootprint.c','body.Mins = m_vMin;','body.Mins = m_vMin - Vector(2, 0, 2);','SPAWN_BODY_MIN_UNPADDED'),
    @('Vehicles/AICF_LogisticsVehicleFootprint.c','body.Maxs = m_vMax;','body.Maxs = m_vMax + Vector(2, 0, 2);','SPAWN_BODY_MAX_UNPADDED'),
    @('Vehicles/AICF_LogisticsVehicleFootprint.c','body.LayerMask = EPhysicsLayerPresets.Vehicle;','body.LayerMask = 0;','SPAWN_BODY_LAYER'),
    @('Vehicles/AICF_LogisticsVehicleFootprint.c','body.Mat[axis] = transform[axis];','body.Mat[axis] = vector.Zero;','SPAWN_BODY_ORIENTATION'),
    @('Vehicles/AICF_VehicleSpawner.c','!footprint.IsClear(world, w.m_aSpawnTransform, body)','false','SPAWN_BODY_COLLISION'),
    @('Vehicles/AICF_VehicleSpawner.c','!IsWheeledSpawnSurfaceSuitable(w.m_aSpawnTransform[3], world, surface, water, delta, probes)','false','SPAWN_SURFACE_REJECTION'),
    @('Vehicles/AICF_VehicleSpawner.c','if (!LogisticsSpawnClear(w, false)) continue;','// removed','SPAWN_RESERVE_LIVE_BODY'),
    @('Vehicles/AICF_VehicleSpawner.c','|| !LogisticsSpawnClear(w)) return false;',') return false;','SPAWN_COMMIT_LIVE_BODY'),
    @('Vehicles/AICF_VehicleSpawner.c','if (!ai || !ai.GetRoadNetworkManager()) return true;','if (!ai || !ai.GetRoadNetworkManager()) return false;','SPAWN_ROAD_OPTIONAL'),
    @('Vehicles/AICF_VehicleSpawner.c','!AICF_LogisticsSpawnGeometry.ExitClear(world, footprint, w.m_aSpawnTransform, w.m_vSpawnExit, exitTrace)','false','SPAWN_EXIT_COLLISION'),
    @('Vehicles/AICF_VehicleSpawner.c','s_aConstructionSites.Insert(w.m_Site);','// removed','SPAWN_SHARED_RESERVATIONS'),
    @('Vehicles/AICF_VehicleSpawner.c','!AICF_ConstructionPlanner.VehicleAreaClear(position, 12)','false','SPAWN_CONSTRUCTION_RESERVATIONS'),
    @('Economy/AICF_LogisticsDepotRegistry.c','slot.AICF_LogisticsClear(entry.GetPrefab())','true','SPAWN_SLOT_PREFAB_IDENTITY'),
    @('Vehicles/AICF_VehicleTaskHandoff.c','wait.m_Return.m_CompartmentToGetIn.m_Value = null;','// removed','GATE_TERMINAL_FOREIGN_SEAT_PROTECTED'),
    @('Economy/AICF_LogisticsJob.c','!access.IsGettingOut()','true','DRIVER_READY_STILL_REJECTS_EXIT'),
    @('Vehicles/AICF_LogisticsDriverInteraction.c','tags.Contains(SCR_AIGoalReaction_OpenNavlinkDoor.SMART_ACTION_TAG)','true','GATE_TAG_EVIDENCE'),
    @('Vehicles/AICF_LogisticsDriverInteraction.c','current != perform && (!exitAction || current != exitAction)','false','GATE_SELECTED_CHAIN'),
    @('Vehicles/AICF_LogisticsDriverInteraction.c','getIn.m_CompartmentToGetIn.m_Value == w.m_Seat','true','GATE_EXACT_NATIVE_RETURN'),
    @('Vehicles/AICF_LogisticsDriverInteraction.c','m_Job.m_sToken != m_sToken','false','GATE_IMMUTABLE_JOB_TOKEN'),
    @('Vehicles/AICF_LogisticsDriverInteraction.c','w.m_iGeneration != m_iGeneration','false','GATE_IMMUTABLE_GENERATION'),
    @('Vehicles/AICF_LogisticsDriverInteraction.c','m_iWaitBeforeMs + elapsed >= AICF_LogisticsConfig.DRIVER_INTERACTION_LEG_BUDGET_MS','false','GATE_TOTAL_LEG_BUDGET'),
    @('Vehicles/AICF_LogisticsDriverInteraction.c','now - m_iProgressAtMs >= AICF_LogisticsConfig.DRIVER_INTERACTION_STALL_MS','false','GATE_STALL_DEADLINE'),
    @('Vehicles/AICF_LogisticsDriverInteraction.c','opened && w.Ready() && !Live(m_Exit)','opened','GATE_EXACT_SETTLED_RETURN'),
    @('Economy/AICF_LogisticsLedger.c','job.m_bCancelled || w.m_DriverInteraction || !w.Ready()','job.m_bCancelled || !w.Ready()','GATE_TRANSFER_CLOSED'),
    @('Economy/AICF_LogisticsJob.c','seat == m_Seat && seat.IsReservedBy(m_Driver)','true','GATE_RESERVATION_EXACT_OWNER'),
    @('Vehicles/AICF_TransportTripController.c','m_Handoff.CancelLogisticsDriverInteraction(w, reason);','// removed','GATE_TERMINAL_BEFORE_JOB_CANCEL'),
    @('Config/AICF_LogisticsConfig.c','Decimal(text, false, parsed)','true','CONFIG_STRICT_PARSE'),
    @('Economy/AICF_LogisticsPlanner.c','actual < capacity * below / 100','actual <= capacity * below / 100','THRESHOLD_STRICT'),
    @('Economy/AICF_LogisticsPlanner.c','search.m_aEndpoints[search.m_iSourceCursor++]','search.m_aEndpoints[0]','RESUMABLE_CANDIDATE_PREPARATION'),
    @('Economy/AICF_LogisticsPlanner.c','!search.m_bPrepared && !PrepareCandidates(w)','false','PREPARATION_PENDING_NOT_FAILURE'),
    @('Economy/AICF_LogisticsLedger.c','if (receipt.m_bAccounted) return false;','// removed','RECEIPT_EXACTLY_ONCE'),
    @('Economy/AICF_LogisticsJob.c','m_Seat.GetOccupant() != m_Driver','false','EXACT_PILOT'),
    @('Economy/AICF_LogisticsJob.c','occupant && occupant != m_Driver','occupant != null','OWN_DRIVER_NOT_FOREIGN_OCCUPANT'),
    @('Economy/AICF_LogisticsResourcePool.c','actual.Valid() && expected.Same(actual)','actual.Valid()','OPERATION_EXACT_POOL'),
    @('Economy/AICF_LogisticsResourcePool.c','!operation.CanInteractWith(container)','false','OPERATION_RESOURCE_RIGHTS'),
    @('Economy/AICF_LogisticsResourcePool.c','!consumer.IsConsuming()','false','OPERATION_CONSUMING_STATE'),
    @('Economy/AICF_LogisticsResourcePool.c','generator.GetResourceMultiplier() != 1','consumer.GetBuyMultiplier() != 1','PHYSICAL_TRANSFER_NOT_TRADE_PRICE'),
    @('Economy/AICF_LogisticsDepotRegistry.c','data.CanSpawnInSlot(slot.GetSlotType())','true','DEPOT_ALLOWED_SLOT'),
    @('Economy/AICF_LogisticsDepotRegistry.c','!w.m_Production.AICF_LogisticsSupports(entry)','false','PREFERENCE_SKIPS_INCOMPATIBLE_ENTRY'),
    @('Vehicles/AICF_VehicleCleanupManager.c','if (!AICF_LogisticsResourceAdapter.CanDeleteVehicle(vehicle))','if (false)','GLOBAL_CARGO_DELETE_GUARD')
)
foreach ($mutation in $mutations) {
    $path = Join-Path $targetCore $mutation[0]
    $original = Get-Content -LiteralPath $path -Raw
    if (-not $original.Contains($mutation[1])) { throw "Mutation target missing: $($mutation[3])" }
    [IO.File]::WriteAllText($path, $original.Replace($mutation[1],$mutation[2]))
    $result = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tools/Test-LogisticsStatic.ps1') -RepositoryRoot $sourceRoot 2>&1
    $verdict = $LASTEXITCODE
    [IO.File]::WriteAllText($path, $original)
    if ($verdict -ne 1 -or ($result -join "`n") -notmatch $mutation[3]) { throw "Mutation survived: $($mutation[3]) $result" }
    $checks++
}
"PASS Logistics contracts: $checks positive/negative cases; planner formulas additionally require Enforce runtime policy probe"
