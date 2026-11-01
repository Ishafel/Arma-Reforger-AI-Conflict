param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($path in @('State/Vehicles/AICF_FIAGarrison.c','State/Vehicles/AICF_FIAGarrisonIntegrity.c','Vehicles/AICF_FIAGarrisonRecovery.c','Forces/AICF_FIAGarrisonCrew.c','Vehicles/AICF_FIAGarrisonService.c','Vehicles/AICF_VehicleSpawner.c')) {
    $sources[$path] = Get-Content (Join-Path $core $path) -Raw
}
$state = 'State/Vehicles/AICF_FIAGarrison.c'
$watch = 'State/Vehicles/AICF_FIAGarrisonIntegrity.c'
$recovery = 'Vehicles/AICF_FIAGarrisonRecovery.c'
$crew = 'Forces/AICF_FIAGarrisonCrew.c'
$rules = @(
    @('BAN_LATCH', $state, 'if (!Replication.IsServer() || !m_sRecoveryBan.IsEmpty()) return;'),
    @('BAN_PERSISTENT', $state, 'if (!m_sRecoveryBan.IsEmpty()) return false;'),
    @('DRIVE_PENDING_RELEASE', $state, 'm_bCrewRecoveryPending = false;'),
    @('ORIGINAL_MEMBER_OBSERVER', $state, '!m_aMemberIntegrity[i].Matches(m_aCrew[i], m_aCrewIds[i])'),
    @('ROSTER_LENGTH', $state, 'm_aEssentialSeats.Count() != count || m_aMemberIntegrity.Count() != count'),
    @('ROSTER_MISSING', $state, 'BlockRecovery("MISSING_OR_STALE_MEMBER");'),
    @('MEMBER_INDEX_BOUNDS', $state, 'index >= m_aCrewIds.Count()'),
    @('MEMBER_OBSERVERS', $crew, 'g.WatchMemberIntegrity(member, g.m_aEssentialSeats[g.m_aCrew.Count() - 1]);'),
    @('VEHICLE_OBSERVER', 'Vehicles/AICF_VehicleSpawner.c', 'g.WatchVehicleIntegrity();'),
    @('GENERATION_FENCE', $watch, 'm_Garrison.m_iGeneration != m_iGeneration'),
    @('VEHICLE_FENCE', $watch, 'm_Garrison.m_Vehicle != m_Entity || m_Garrison.m_VehicleId != m_EntityId'),
    @('HEALTH_EVENT', $watch, 'scripted.GetOnHealthChanged().Insert(OnHealthChanged);'),
    @('HEALTH_REMOVE', $watch, 'zone.GetOnHealthChanged(false).Remove(OnHealthChanged);'),
    @('DAMAGE_EVENT', $watch, 'm_Damage.GetOnDamage().Insert(OnDamage);'),
    @('DAMAGE_REMOVE', $watch, 'm_Damage.GetOnDamage().Remove(OnDamage);'),
    @('HIERARCHY_SCAN', $watch, 'm_Damage.GetAllHitZonesInHierarchy(current);'),
    @('ACTUAL_DAMAGE', $watch, '!(zone.GetHealth() >= zone.GetMaxHealth())'),
    @('PASSENGER_DEATH', $watch, 'else if (!m_bEssential) loss = "PASSENGER_DEAD";'),
    @('ESSENTIAL_WOUND', $watch, '(m_bVehicle || m_bEssential)'),
    @('UNKNOWN_OBSERVER', $watch, 'if (!scripted) { g.BlockRecovery("UNKNOWN_HITZONE_OBSERVER"); continue; }'),
    @('UNKNOWN_MANAGER', $watch, 'm_Entity.FindComponent(SCR_DamageManagerComponent) != m_Damage'),
    @('STOP', 'Vehicles/AICF_FIAGarrisonService.c', 'g.StopIntegrity();'),
    @('RELOCATE_EARLY_GATE', $recovery, 'if (!g.RecoveryAllowed()) { Blocked(g, g.RecoveryBan()); return false; }'),
    @('FORCED_CREW_GATE', $crew, 'if (!g.RecoveryAllowed() || !g.OwnsMember(member) || !g.VehicleIdentity() || seat.GetOccupant() ||')
)
function Test-Sources($candidate) {
    $failed = @()
    foreach ($rule in $rules) { if (-not $candidate[$rule[1]].Contains($rule[2])) { $failed += $rule[0] } }
    if ([regex]::Matches($candidate[$recovery], 'if \(!g\.RecoveryAllowed\(\)\)').Count -ne 3) { $failed += 'RELOCATE_THREE_GATES' }
    if ($candidate[$recovery] -notmatch 'ClearFIAGarrisonWaypoint\(g\);[\s\S]*?RecoveryAllowed\(\)[\s\S]*?SetVelocity') { $failed += 'POST_WAYPOINT_GATE' }
    if ($candidate[$crew] -notmatch 'InterruptVehicleActionQueue\(true, true, true\);[\s\S]*?RecoveryAllowed\(\)[\s\S]*?access.GetInVehicle') { $failed += 'POST_QUEUE_GATE' }
    # Native re-materialization — новая incarnation выжившего, не recovery
    # существующего бойца. Initial boarding по-прежнему требует RecoveryAllowed;
    # единственное исключение — fenced native reactivation после Ready/restore.
    if ($candidate[$crew] -notmatch 'if \(!g.RecoveryAllowed\(\) && \(!g.m_Lifecycle \|\| !g.m_Lifecycle.IsReactivating\(\)\)\) return false;\s*if \(g.m_Lifecycle && !g.m_Lifecycle.Current\(\)\) return false;\s*AICF_AIQuotaVehicleDispatch quota = AICF_AIQuotaVehicleDispatch.Begin') { $failed += 'INITIAL_BOARDING_GATE' }
    if ($candidate[$crew] -notmatch 'if \(\(!g.RecoveryAllowed\(\) && \(!g.m_Lifecycle \|\| !g.m_Lifecycle.IsReactivating\(\)\)\) \|\| !quota.IsCurrent\(\)\)\s*\{\s*quota.Cancel\(\);\s*return false;\s*\}\s*bool accepted = access.GetInVehicle') { $failed += 'POST_QUOTA_RECOVERY_GATE' }
    if ($candidate[$recovery] -notmatch 'RecoveryAllowed\(\)[\s\S]*?OccupantsSafe\(g\)[\s\S]*?Candidate\(g' -or
        $candidate[$recovery] -notmatch 'Candidate\(g[\s\S]*?RecoveryAllowed\(\)[\s\S]*?SetWorldTransform\(pose\)') { $failed += 'RELOCATE_GATE_ORDER' }
    if ($candidate[$state] -match 'm_sRecoveryBan\s*=\s*(string\.Empty|""|reason)' -and [regex]::Matches($candidate[$state], 'm_sRecoveryBan\s*=').Count -ne 1) { $failed += 'BAN_RESET' }
    if ($candidate[$state] -match 'RecoveryAllowed[\s\S]*?bool CanDrive\(\)[\s\S]*?RecoveryBan') { $failed += 'DRIVING_BAN_COUPLING' }
    return $failed
}
$failures = @(Test-Sources $sources)
$mutations = 0
foreach ($rule in $rules) {
    $copy = $sources.Clone()
    $copy[$rule[1]] = $copy[$rule[1]].Replace($rule[2], 'REMOVED')
    if ($rule[0] -notin @(Test-Sources $copy)) { $failures += "MUTATION_$($rule[0])" }
    $mutations++
}
$copy = $sources.Clone()
$copy[$recovery] = $copy[$recovery].Replace('if (!g.RecoveryAllowed()) { Blocked(g, g.RecoveryBan()); return false; }', '')
if ('RELOCATE_THREE_GATES' -notin @(Test-Sources $copy)) { $failures += 'MUTATION_THREE_GATES' }
$mutations++
$copy = $sources.Clone()
$copy[$crew] = $copy[$crew].Replace('!g.m_Lifecycle.IsReactivating()', 'false')
if ('INITIAL_BOARDING_GATE' -notin @(Test-Sources $copy)) { $failures += 'MUTATION_INITIAL_BOARDING_GATE' }
$mutations++
 $copy = $sources.Clone()
 $copy[$crew] = $copy[$crew].Replace('if ((!g.RecoveryAllowed() && (!g.m_Lifecycle || !g.m_Lifecycle.IsReactivating())) || !quota.IsCurrent())','if (!quota.IsCurrent())')
 if ('POST_QUOTA_RECOVERY_GATE' -notin @(Test-Sources $copy)) { $failures += 'MUTATION_POST_QUOTA_RECOVERY_GATE' }
 $mutations++
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
"FIA integrity contracts: PASS; rules=$($rules.Count); negative_mutations=$mutations; runtime=NOT_RUN"
