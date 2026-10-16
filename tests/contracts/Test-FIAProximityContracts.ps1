param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($path in @('Forces/AICF_FIAProximityLifecycle.c','State/Vehicles/AICF_FIALifecycleMember.c','Vehicles/AICF_FIAGarrisonService.c','Vehicles/AICF_FIAPatrolService.c','Vehicles/AICF_VehicleCleanupManager.c','Forces/AICF_FIAGarrisonCrew.c')) {
    $sources[$path] = Get-Content -LiteralPath (Join-Path $core $path) -Raw
}
$lifecycle = 'Forces/AICF_FIAProximityLifecycle.c'
$member = 'State/Vehicles/AICF_FIALifecycleMember.c'
$cleanup = 'Vehicles/AICF_VehicleCleanupManager.c'
$rules = @(
    @('NATIVE_BANDS', $lifecycle, 'SCR_EAIGroupLifecyclePolicy.ProximityDriven, ambientSystem.GetSpawnDistance(), ambientSystem.GetDespawnDistance()'),
    @('NATIVE_LIFECYCLE', $lifecycle, 'super.LifecycleTick();'),
    @('NATIVE_EXPANSION', $lifecycle, 'return super.ExpandOneMember();'),
    @('NATIVE_DORMANCY', $lifecycle, 'super.DespawnMembers();'),
    @('ACQUIRE_BUDGET', $lifecycle, '!ai.CanActivateGroup(null)'),
    @('OBSERVERS', $lifecycle, 'observers.HasObserverWithinRangeSq(position[0], position[2], range * range)'),
    @('AUTHORITY', $lifecycle, '!Replication.IsServer() || m_bStopped || !m_Group || m_Group.GetID() != m_GroupId'),
    @('MASTER', $lifecycle, '!rpl || !rpl.IsMaster()'),
    @('GARRISON_GENERATION', $lifecycle, 'm_Garrison.m_iGeneration == m_iGeneration'),
    @('PATROL_GENERATION', $lifecycle, 'm_Patrol.m_iGeneration == m_iGeneration'),
    @('BASE_OWNERSHIP', $lifecycle, 'm_Garrison.m_Base.GetFaction() != m_Garrison.m_Faction'),
    @('QUEUE_FENCE', $lifecycle, 'm_AICFFIALifecycle && !m_AICFFIALifecycle.PrepareExpansion()'),
    @('DESTRUCTIVE_FENCE', $lifecycle, 'm_AICFFIALifecycle && !m_AICFFIALifecycle.PrepareDormancy()'),
    @('LOGICAL_CAPACITY', $lifecycle, 'if (!member.m_bLost) count++;'),
    @('NO_RESURRECTION', $lifecycle, 'member.m_bLost = true;'),
    @('DORMANCY_NOT_LOSS', $lifecycle, 'm_bDespawning || !agent'),
    @('LIVE_ANCHOR', $lifecycle, 'AICF_GroupRuntime.ResolveAliveLeader(m_Group)'),
    @('RESTORE_GATE', $lifecycle, 'member.m_bRestoring ||'),
    @('EQUIPMENT_GATE', $member, 'IsFIAEquipmentReady(m_Entity)'),
    @('INVENTORY_CAPTURE', $member, 'AICF_LoadoutInventory.Capture(m_Entity, m_sInventory)'),
    @('INVENTORY_RESTORE', $member, 'AICF_LoadoutInventory.Restore(m_Entity, m_sInventory)'),
    @('HEALTH_PRESERVED', $member, 'zone.SetHealth(Math.Min(zone.GetHealth(), m_aHealth[index]))'),
    @('IDENTITY_OCCUPANCY', $cleanup, 'vehicle.GetID() != identity'),
    @('UNKNOWN_SEATS', $cleanup, 'if (seats.IsEmpty()) return false;'),
    @('PLAYER_OCCUPANT', $cleanup, 'SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity(occupant) > 0'),
    @('FOREIGN_OCCUPANT', $cleanup, 'control.GetAIAgent().GetParentGroup() != companion'),
    @('KNOWN_OCCUPANT_ID', $cleanup, 'occupant.GetID() != knownIds[index]'),
    @('EMPTY_OCCUPANT_GUARD', $cleanup, 'if (occupant)'),
    @('TRANSITION_OCCUPANCY', $cleanup, 'access.IsGettingIn() || access.IsGettingOut()'),
    @('GARRISON_PROXIMITY', 'Vehicles/AICF_FIAGarrisonService.c', 'AICF_FIAProximityLifecycle.CanAcquire(g.m_vHome)'),
    @('PATROL_PROXIMITY', 'Vehicles/AICF_FIAPatrolService.c', 'AICF_FIAProximityLifecycle.CanAcquire(p.m_Current.GetOwner().GetOrigin())'),
    @('STOP_QUEUE', $lifecycle, 'world.PurgeSpawnRequestsForGroup(m_Group)'),
    @('DESANT_TRANSFER', 'Forces/AICF_FIAGarrisonCrew.c', 'g.m_Lifecycle.Transfer(member, g.m_DesantLifecycle)'),
    @('TRANSFER_BEGIN', 'Forces/AICF_FIAGarrisonCrew.c', 'g.m_Lifecycle.BeginTransfer(member, g.m_DesantLifecycle)'),
    @('TRANSFER_MUTATION_FENCE', 'Forces/AICF_FIAGarrisonCrew.c', 'g.m_Lifecycle.TransferCurrent(member, g.m_DesantLifecycle)'),
    @('PLANNED_REMOVAL', $lifecycle, 'if (member == m_TransferMember && member.m_Entity'),
    @('INCOMING_MEMBER', $lifecycle, 'if (m_IncomingMember && m_IncomingMember.m_Entity == entity'),
    @('TRANSFER_FROZEN_LIFECYCLE', $lifecycle, 'if (!Current() || m_TransferMember || m_IncomingMember) return false;'),
    @('TRANSFER_ROLLBACK', $lifecycle, 'if (member.m_Agent.GetParentGroup() == m_Group) return;'),
    @('TRANSFER_CLEANUP', $lifecycle, 'm_TransferTarget.m_IncomingMember = null;')
)
function Get-ProximityFailures([hashtable]$inputSources) {
    foreach ($rule in $rules) {
        if (-not $inputSources[$rule[1]].Contains($rule[2])) { $rule[0] }
    }
    foreach ($service in @('Vehicles/AICF_FIAGarrisonService.c','Vehicles/AICF_FIAPatrolService.c')) {
        if ($inputSources[$service] -match 'KeepCaptureEligible|PreventMaxLOD|BeginRosterSpawn') { "NATIVE_LIFECYCLE_BYPASS:$service" }
    }
    $adapter = $inputSources[$lifecycle]
    if ($adapter -match 'DeleteRplEntity|SpawnEntityPrefab|SetLimitOfActiveAIs|CallLater\(') { 'DOMAIN_OWNERSHIP' }
    if (([regex]::Matches($adapter,'CanDormantFIA\(vehicle, vehicleId, m_Group, known, knownIds, companion, reason\)')).Count -ne 2) { 'REPEATED_OCCUPANCY_CHECK' }
    if ($adapter -notmatch '(?s)void Stop\(\).*m_bStopped = true.*SCR_EAIGroupLifecyclePolicy.Manual.*PurgeSpawnRequestsForGroup') { 'STOP_LIFECYCLE' }
    $transfer = [regex]::Match($adapter, '(?s)void Transfer\(.*?(?=\s+void Stop\()').Value
    if (!$transfer -or $transfer -match 'm_bLost\s*=\s*false') { 'TRANSFER_NO_REVIVAL' }
}
$failures = @(Get-ProximityFailures $sources)
foreach ($rule in $rules) {
    $mutated = $sources.Clone()
    $mutated[$rule[1]] = $mutated[$rule[1]].Replace($rule[2], 'REMOVED_CONTRACT')
    $rejected = @(Get-ProximityFailures $mutated)
    if ($rule[0] -notin $rejected) { $failures += "NEGATIVE_$($rule[0])" }
}
$revival = $sources.Clone()
$revival[$lifecycle] = $revival[$lifecycle].Replace('m_aMembers.RemoveItem(member);', 'member.m_bLost = false; m_aMembers.RemoveItem(member);')
if ('TRANSFER_NO_REVIVAL' -notin @(Get-ProximityFailures $revival)) { $failures += 'NEGATIVE_TRANSFER_NO_REVIVAL' }
if ($failures.Count) { $failures | ForEach-Object { Write-Output "FAIL $_" }; exit 1 }
Write-Output "PASS FIA proximity contracts: $($rules.Count) positive/negative guards + no-revival mutation; runtime=NOT_RUN"
