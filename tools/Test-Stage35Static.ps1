param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage3StaticAudit.Common.ps1')

$failures = [System.Collections.Generic.List[string]]::new()

function Add-Stage35Failures {
    param([string[]]$Items)
    foreach ($item in $Items) {
        if ($item) { $failures.Add($item) }
    }
}

function Get-Stage35RecordByName {
    param([object[]]$Records, [string]$Name)
    $matches = @($Records | Where-Object { $_.Name -eq $Name })
    if ($matches.Count -eq 1) { return $matches[0] }
    return $null
}

function Require-Stage35Record {
    param([object]$Record, [string]$Name)
    if (-not $Record) {
        Add-AICFAuditFailure $failures 'STAGE35_COMPONENT_MISSING' "Missing unique source file $Name"
        return $false
    }
    return $true
}

$records = @(Get-AICFSourceRecords $RepositoryRoot)
if ($records.Count -eq 0) {
    Add-AICFAuditFailure $failures 'SOURCE_TREE_MISSING' 'No AIConflictCore Enforce sources were found'
}

Add-Stage35Failures @(Invoke-AICFVehicleArchitectureAudit -RepositoryRoot $RepositoryRoot)
Add-Stage35Failures @(Invoke-AICFArchitectureNegativeSelfCheck)
Add-Stage35Failures @(Invoke-AICFLanguageAudit -RepositoryRoot $RepositoryRoot)

$stage1Config = Get-Stage35RecordByName $records 'AICF_Stage1Config.c'
if (Require-Stage35Record $stage1Config 'AICF_Stage1Config.c') {
    $forceConstants = @{
        'GROUP_SLOTS_PER_FACTION\s*=\s*10\s*;' = 'Ten stable slots per faction are required'
        'DEFAULT_GROUP_SIZE\s*=\s*4\s*;' = 'Managed rosters must default to four members'
        'MAX_GROUP_SIZE\s*=\s*10\s*;' = 'Commander-selected rosters must be capped at ten members'
        'ATTACK_SLOTS_PER_FACTION\s*=\s*6\s*;' = 'Default doctrine must expose six ATTACK slots'
        'DEFEND_SLOTS_PER_FACTION\s*=\s*3\s*;' = 'Default doctrine must expose three DEFEND/QRF slots'
        'RESERVE_SLOTS_PER_FACTION\s*=\s*1\s*;' = 'Default doctrine must expose one RESERVE slot'
        'LEGACY_ATTACK_SLOTS_PER_FACTION\s*=\s*5\s*;' = 'Roles-off baseline must retain five ATTACK slots'
        'LEGACY_DEFEND_SLOTS_PER_FACTION\s*=\s*3\s*;' = 'Roles-off baseline must retain three DEFEND slots'
        'LEGACY_RESERVE_SLOTS_PER_FACTION\s*=\s*2\s*;' = 'Roles-off baseline must retain two RESERVE slots'
        'MIN_MANAGED_AGENTS\s*=\s*80\s*;' = 'Managed-agent CLI floor must cover twenty default four-person groups'
        'DEFAULT_MAX_MANAGED_AGENTS\s*=\s*220\s*;' = 'Default managed-agent budget must cover twenty groups up to their configured cap'
    }
    foreach ($pattern in $forceConstants.Keys) {
        Assert-AICFContains $failures 'STAGE35_FORCE_STRUCTURE' $stage1Config.Code $pattern $forceConstants[$pattern]
    }
    Assert-AICFContains $failures 'STAGE35_ACTIVE_ROLE_TOGGLE' $stage1Config.Source '"aicfActiveForcesRolesEnabled"' 'Active-force roles must retain their explicit CLI toggle'
}

$stage3Config = Get-Stage35RecordByName $records 'AICF_Stage3Config.c'
if (Require-Stage35Record $stage3Config 'AICF_Stage3Config.c') {
    Assert-AICFContains $failures 'STAGE35_VEHICLE_CAP' $stage3Config.Code 'DEFAULT_MAX_VEHICLES_PER_FACTION\s*=\s*10\s*;' 'Active/reserved vehicle lease cap must cover all ten groups'
    Assert-AICFContains $failures 'STAGE35_MINIMUM_NEW_REQUEST' $stage3Config.Code 'DEFAULT_MINIMUM_VEHICLE_REQUEST_AGENTS\s*=\s*3\s*;' 'New vehicle request minimum must remain three'
    Assert-AICFContains $failures 'STAGE35_COHESION_WAIT' $stage3Config.Code 'DEFAULT_COHESION_WAIT_TIMEOUT_MS\s*=\s*300000\s*;' 'Fragmented cohesion wait must retain its five-minute default deadline'
    Assert-AICFContains $failures 'STAGE35_COHESION_WAIT' $stage3Config.Source '"aicfVehicleCohesionWaitTimeoutMs"' 'Cohesion wait must retain its CLI override'
    Assert-AICFContains $failures 'STAGE35_ALL_SLOT_MOTORIZATION' $stage3Config.Code 'NormalizeVehicleCounts[\s\S]*GROUP_SLOTS_PER_FACTION' 'Vehicle counts must normalize against all stable slots'
}

$acquisition = Find-AICFClassRecord $records 'AICF_VehicleAcquisitionFlow'
if ($acquisition) {
    $processWaiting = ConvertTo-AICFCodeText (Get-AICFMethodBody $acquisition 'ProcessWaitingForSite')
    $waitingExit = Get-AICFMethodBody $acquisition 'ReportWaitingExit'
    $waitingExitCode = ConvertTo-AICFCodeText $waitingExit
    $boundedExit = Get-AICFMethodBody $acquisition 'EndRequest'
    $boundedExitCode = ConvertTo-AICFCodeText $boundedExit
    $observeCohesion = Get-AICFMethodBody $acquisition 'ObserveBoundedCohesion'
    $observeCohesionCode = ConvertTo-AICFCodeText $observeCohesion
    $cohesionOutcome = Get-AICFMethodBody $acquisition 'ReportCohesionOutcome'
    $cohesionOutcomeCode = ConvertTo-AICFCodeText $cohesionOutcome

    foreach ($field in @(
        'outcome=ELIGIBLE_SITE_PREFLIGHT', 'wait_age_ms=', 'cumulative_attempts=',
        'old_request_generation=', 'base_revision=', 'target=', 'base=',
        'cap_reserved=0', 'vehicle_retry_suppressed=0'
    )) {
        Assert-AICFContains $failures 'STAGE35_WAITING_EXIT_TELEMETRY' $waitingExit ([regex]::Escape($field)) "Eligible-site WAITING_FOR_SITE_EXIT omits $field"
    }
    Assert-AICFContains $failures 'STAGE35_WAITING_EXIT_TELEMETRY' $waitingExitCode 'BuildIdentityContext\s*\(' 'Eligible-site exit must include current request/trip identity'
    Assert-AICFContains $failures 'STAGE35_WAITING_EXIT_TELEMETRY' $waitingExitCode 'GetTotalAttemptCount\s*\(' 'Eligible-site exit must report cumulative attempts from request state'
    Assert-AICFContains $failures 'STAGE35_WAITING_EXIT_ONE_SHOT' $processWaiting 'ExitWaitingForSite\s*\(\s*\)\s*;[\s\S]*ReportWaitingExit\s*\(' 'Eligible-site exit must be emitted only after leaving the waiting state'

    foreach ($field in @(
        'outcome=BOUNDED_INFANTRY_FALLBACK', 'wait_age_ms=', 'total_wait_age_ms=',
        'cumulative_attempts=', 'target=', 'request_generation=',
        'vehicle_retry_suppressed=1'
    )) {
        Assert-AICFContains $failures 'STAGE35_WAITING_EXIT_TELEMETRY' $boundedExit ([regex]::Escape($field)) "Bounded WAITING_FOR_SITE_EXIT omits $field"
    }
    foreach ($evidence in @(
        'GetWaitAgeMs\s*\(', 'GetTotalWaitAgeMs\s*\(',
        'GetTotalAttemptCount\s*\(', 'GetRequestGeneration\s*\('
    )) {
        Assert-AICFContains $failures 'STAGE35_WAITING_EXIT_TELEMETRY' $boundedExitCode $evidence 'Bounded waiting exit must derive telemetry from request state'
    }

    foreach ($field in @(
        'outcome=', 'wait_reason=', 'wait_age_ms=', 'deadline_ms=', 'alive=',
        'farthest_from_leader_m=', 'maximum_pair_m=', 'threshold_m=',
        'normalized=', 'order_outcome=', 'members='
    )) {
        Assert-AICFContains $failures 'STAGE35_COHESION_TELEMETRY' $cohesionOutcome ([regex]::Escape($field)) "COHESION_OUTCOME omits $field"
    }
    Assert-AICFContains $failures 'STAGE35_COHESION_TELEMETRY' $cohesionOutcomeCode 'GetLastFailureReason\s*\(' 'Cohesion telemetry must use the current wait reason'
    Assert-AICFContains $failures 'STAGE35_COHESION_TELEMETRY' $cohesionOutcomeCode 'GetCohesionWaitTimeoutMs\s*\(' 'Cohesion telemetry must use the configured absolute deadline'
    Assert-AICFContains $failures 'STAGE35_COHESION_ONE_SHOT' $observeCohesionCode '!requestState\.WasCohesionRecoveryAttempted\s*\(\s*\)[\s\S]*MarkCohesionRecoveryAttempted\s*\(\s*\)[\s\S]*ReportCohesionOutcome\s*\(' 'Cohesion normalization outcome must be guarded by its one-shot attempt latch'
    Assert-AICFContains $failures 'STAGE35_COHESION_ONE_SHOT' $observeCohesion 'ReportCohesionOutcome\s*\([\s\S]*INFANTRY_FALLBACK_REQUESTED[\s\S]*return\s+EndRequest\s*\(' 'Terminal cohesion telemetry must immediately produce the bounded request outcome'
} else {
    Add-AICFAuditFailure $failures 'STAGE35_COMPONENT_MISSING' 'Missing AICF_VehicleAcquisitionFlow'
}

$factionState = Find-AICFClassRecord $records 'AICF_FactionState'
if ($factionState) {
    $buildSlots = ConvertTo-AICFCodeText (Get-AICFMethodBody $factionState 'BuildDefaultSlots')
    Assert-AICFContains $failures 'STAGE35_ACTIVE_ROLE_TOGGLE' $buildSlots 'activeForcesRolesEnabled[\s\S]*LEGACY_ATTACK_SLOTS_PER_FACTION[\s\S]*LEGACY_DEFEND_SLOTS_PER_FACTION' 'Roles-off baseline must select the configured legacy boundaries'
    Assert-AICFContains $failures 'STAGE35_ROLE_LOCAL_IDENTITY' $buildSlots 'roleIndex' 'Stable slots must retain role-local A0/A1/A2/D0 ordinals'
}

$groupSpawner = Find-AICFClassRecord $records 'AICF_GroupSpawner'
if ($groupSpawner) {
    $spawnGroup = ConvertTo-AICFCodeText (Get-AICFMethodBody $groupSpawner 'SpawnGroup')
    $beginRosterSpawn = ConvertTo-AICFCodeText (Get-AICFMethodBody $groupSpawner 'BeginRosterSpawn')
    Assert-AICFContains $failures 'STAGE35_EXACT_ROSTER' $groupSpawner.Code 'SCR_AIGroup\s*\.\s*IgnoreSpawning\s*\(' 'Roster shaping must use the stock SCR_AIGroup spawn guard'
    Assert-AICFContains $failures 'STAGE35_EXACT_ROSTER' $groupSpawner.Code '\bm_aUnitPrefabSlots\b' 'Roster shaping must use faction-correct prefab unit slots'
    Assert-AICFContains $failures 'STAGE35_EXACT_ROSTER' $spawnGroup 'IgnoreSpawning\s*\(\s*true\s*\)[\s\S]*SpawnEntityPrefabEx[\s\S]*IgnoreSpawning\s*\(\s*false\s*\)[\s\S]*ConfigureManagedRoster' 'Group entity creation and exact roster shaping must preserve the one-shot stock spawn guard order'
    Assert-AICFNotContains $failures 'STAGE35_AI18_QUEUE' $spawnGroup 'SpawnUnits\s*\(' 'Reforger 1.8 managed group creation must not use the lossy synchronous SpawnUnits path'
    Assert-AICFContains $failures 'STAGE35_AI18_QUEUE' $beginRosterSpawn 'SetNumberOfMembersToSpawn\s*\([\s\S]*RequestSpawn\s*\(' 'Managed members must enter the Reforger 1.8 SCR_AIWorld request queue'
    Assert-AICFContains $failures 'STAGE35_EXACT_ROSTER' $spawnGroup 'desiredSize[\s\S]*ConfigureManagedRoster' 'Initial and replacement roster shaping must derive from the commander-selected desired size'
}

$groupRuntime = Find-AICFClassRecord $records 'AICF_GroupRuntime'
if ($groupRuntime) {
    $exactRoster = ConvertTo-AICFCodeText (Get-AICFMethodBody $groupRuntime 'HasExactFactionRoster')
    $spawnSnapshot = Get-AICFMethodBody $groupRuntime 'BuildSpawnSnapshot'
    Assert-AICFContains $failures 'STAGE35_EXACT_ROSTER' $exactRoster 'GetAgents[\s\S]*IsAliveCharacter[\s\S]*FactionAffiliationComponent[\s\S]*GetAffiliatedFaction[\s\S]*actualCount\s*==\s*expectedCount[\s\S]*factionMismatchCount\s*==\s*0[\s\S]*nonAliveCount\s*==\s*0' 'READY roster proof must check exact count, alive state, and faction affiliation for every member'
    foreach ($field in @(
        'group_entity_id=', 'generation=', 'actual=', 'alive=', 'faction_correct=',
        'spawning_pending=', 'pending_source=', 'observed_age_ms=', 'age_source=',
        'entity_exists=', 'replication_ready=', 'incomplete_reason=', 'members='
    )) {
        Assert-AICFContains $failures 'STAGE35_SPAWN_DIAGNOSTICS' $spawnSnapshot ([regex]::Escape($field)) "Spawn snapshot omits $field"
    }
    Assert-AICFContains $failures 'STAGE35_AI18_QUEUE' $spawnSnapshot 'AICF_EXPECTED_MINUS_ACTUAL' '1.8 pending telemetry must be owned by the managed expected-minus-actual invariant'
    Assert-AICFNotContains $failures 'STAGE35_AI18_QUEUE' (ConvertTo-AICFCodeText $spawnSnapshot) 'GetSpawnQueueSize\s*\(' 'The removed per-group queue must not be used as Reforger 1.8 runtime truth'
}

$fleet = Find-AICFClassRecord $records 'AICF_FactionFleet'
if ($fleet) {
    $reservedCount = ConvertTo-AICFCodeText (Get-AICFMethodBody $fleet 'GetReservedCount')
    $capCount = ConvertTo-AICFCodeText (Get-AICFMethodBody $fleet 'GetActiveOrReservedCount')
    Assert-AICFContains $failures 'STAGE35_FLEET_HEARTBEAT_TRUTH' $reservedCount 'AICF_EVehicleLeaseState\.RESERVED' 'Reserved telemetry must count only the exact RESERVED lease state'
    Assert-AICFNotContains $failures 'STAGE35_FLEET_HEARTBEAT_TRUTH' $reservedCount 'IsCapActive|ACTIVE|RELEASE_PENDING|FAILED_CLOSED' 'Reserved telemetry must not fold active, releasing, or fail-closed cap holders into RESERVED'
    Assert-AICFContains $failures 'STAGE35_FLEET_HEARTBEAT_TRUTH' $capCount 'IsCapActive\s*\(' 'Cap-held telemetry must use the Fleet-owned lease-state predicate'
} else {
    Add-AICFAuditFailure $failures 'STAGE35_COMPONENT_MISSING' 'Missing AICF_FactionFleet'
}

$vehicleCoordinator = Find-AICFClassRecord $records 'AICF_VehicleCoordinator'
$vehicleDiagnostics = Find-AICFClassRecord $records 'AICF_VehicleDomainDiagnostics'
if ($vehicleCoordinator -and $vehicleDiagnostics) {
    $coordinatorHeartbeat = Get-AICFMethodBody $vehicleCoordinator 'Heartbeat'
    $fleetHeartbeat = Get-AICFMethodBody $vehicleDiagnostics 'FleetHeartbeat'
    foreach ($field in @('reserved=', 'release_pending=', 'failed_closed=', 'cap_held=', 'retained_physical=')) {
        Assert-AICFContains $failures 'STAGE35_FLEET_HEARTBEAT_TRUTH' $coordinatorHeartbeat ([regex]::Escape($field)) "Stage 3 heartbeat omits truthful asset field $field"
        Assert-AICFContains $failures 'STAGE35_FLEET_HEARTBEAT_TRUTH' $fleetHeartbeat ([regex]::Escape($field)) "FORCE_HEARTBEAT omits truthful asset field $field"
    }
} else {
    Add-AICFAuditFailure $failures 'STAGE35_COMPONENT_MISSING' 'Missing vehicle coordinator or domain diagnostics for aggregate heartbeat audit'
}

$match = Find-AICFClassRecord $records 'AICF_MatchController'
if ($match) {
    $processFaction = ConvertTo-AICFCodeText (Get-AICFMethodBody $match 'ProcessFaction')
    $bindManaged = Get-AICFMethodBody $match 'BindManagedGroup'
    $bindManagedCode = ConvertTo-AICFCodeText $bindManaged
    $spawnTimeout = Get-AICFMethodBody $match 'HandleSpawnTimeout'
    $spawnTimeoutCode = ConvertTo-AICFCodeText $spawnTimeout
    $releaseGroups = ConvertTo-AICFCodeText (Get-AICFMethodBody $match 'ReleaseFactionGroups')
    Assert-AICFContains $failures 'STAGE35_EXACT_ROSTER' $processFaction 'expectedSize\s*=\s*slot\.GetDesiredSize\s*\([\s\S]*IsRosterSpawnRequested\s*\([\s\S]*actualCount\s*==\s*expectedSize[\s\S]*GetNumberOfMembersToSpawn\s*\([\s\S]*HasExactFactionRoster[\s\S]*MarkReady' 'Slot READY requires an issued request and the exact alive faction-correct commander-selected roster'
    Assert-AICFNotContains $failures 'STAGE35_AI18_QUEUE' $processFaction 'GetSpawnQueueSize\s*\(' 'READY must not use the Reforger 1.8 compatibility queue-size stub'
    Assert-AICFNotContains $failures 'STAGE35_EXACT_ROSTER' $match.Code 'GetAgentsCount\s*\(\s*\)\s*>\s*0[\s\S]{0,100}MarkReady' 'Legacy non-empty READY gate must not survive'
    Assert-AICFContains $failures 'STAGE35_AI18_QUEUE' $bindManagedCode 'BindSpawnedGroup[\s\S]*GetOnAgentAdded[\s\S]*GetOnAllDelayedEntitySpawned[\s\S]*MarkRosterSpawnRequested[\s\S]*BeginRosterSpawn' 'Managed ownership, generation observers, and pending intent must be installed before the 1.8 roster request is issued'
    Assert-AICFContains $failures 'STAGE35_SPAWN_DIAGNOSTICS' $processFaction 'MaybeLogSpawnAudit\s*\(' 'Every SPAWNING poll must offer a periodic roster audit'
    Assert-AICFContains $failures 'STAGE35_SPAWN_DIAGNOSTICS' $spawnTimeout 'GROUP_SPAWN_TIMEOUT' 'Timeout must retain its dedicated diagnostic event'
    Assert-AICFContains $failures 'STAGE35_SPAWN_DIAGNOSTICS' $spawnTimeoutCode 'BuildGroupSpawnSnapshot[\s\S]*AuditAllSpawningGroups[\s\S]*DetachSpawnObservers[\s\S]*DeleteRplEntity[\s\S]*MarkDestroyed' 'Timeout must snapshot the failed and remaining spawning groups before callback detach, entity deletion, and slot clearing'
    Assert-AICFContains $failures 'STAGE35_SPAWN_CALLBACK_CLEANUP' $releaseGroups 'DetachSpawnObservers' 'Shutdown must remove roster spawn callbacks even when entities are retained'
    $replacementStart = ConvertTo-AICFCodeText (Get-AICFMethodBody $match 'TryStartReplacement')
    $projectedAgents = ConvertTo-AICFCodeText (Get-AICFMethodBody $match 'CountProjectedFactionAgents')
    Assert-AICFContains $failures 'STAGE35_AGENT_BUDGET' $projectedAgents 'slot\s*==\s*pendingSlot[\s\S]*slot\.GetState\s*\(\s*\)\s*==\s*AICF_EGroupSlotState\.SPAWNING[\s\S]*Math\.Max\s*\(\s*actualAgents\s*,\s*slot\.GetDesiredSize\s*\(\s*\)\s*\)' 'Agent projection must reserve each pending or partially materialized slot at its configured desired roster size'
    Assert-AICFNotContains $failures 'STAGE35_AGENT_BUDGET' $projectedAgents 'MAX_GROUP_SIZE|PENDING_GROUP_AGENT_BUDGET' 'Agent projection must not charge every pending slot at the global maximum group size'
    Assert-AICFContains $failures 'STAGE35_AGENT_BUDGET' $replacementStart 'CountProjectedManagedAgentsForSpawn\s*\(\s*slot\s*\)[\s\S]*projectedManagedAgents\s*>\s*managedAgentLimit[\s\S]*PostponeReinforcementUntil[\s\S]*BeginReplacementSpawn' 'AI-limit preflight must run before the slot enters SPAWNING so blocked retries do not consume group generations'
    $forceHeartbeat = ConvertTo-AICFCodeText (Get-AICFMethodBody $match 'Heartbeat')
    Assert-AICFContains $failures 'STAGE35_FLEET_HEARTBEAT_TRUTH' $forceHeartbeat 'trackedEntities[\s\S]*usRetainedPhysical[\s\S]*ussrRetainedPhysical' 'Tracked entity telemetry must include retained or unconfirmed physical vehicle assets'
}

$catalog = Find-AICFClassRecord $records 'AICF_VehicleCatalog'
if ($catalog) {
    Assert-AICFContains $failures 'STAGE35_FACTION_CATALOG' $catalog.Source 'M923A1[\\/]M923A1_transport\.et' 'US truck catalog candidate must remain M923A1'
    Assert-AICFContains $failures 'STAGE35_FACTION_CATALOG' $catalog.Source 'M998[\\/]M998_covered_long\.et' 'US roomy unarmed light candidate must remain M998'
    Assert-AICFContains $failures 'STAGE35_FACTION_CATALOG' $catalog.Source 'Ural4320[\\/]Ural4320_transport\.et' 'USSR truck catalog candidate must remain Ural'
    Assert-AICFContains $failures 'STAGE35_FACTION_CATALOG' $catalog.Source 'UAZ452[\\/]UAZ452_transport\.et' 'USSR roomy unarmed light candidate must remain UAZ-452'
    Assert-AICFContains $failures 'STAGE35_FACTION_CATALOG' $catalog.Code 'AICF_EVehicleKind\.LIGHT_TRANSPORT' 'Unarmed light transport must remain distinct from trucks and armed light vehicles'
}

$acquisition = Find-AICFClassRecord $records 'AICF_VehicleAcquisitionFlow'
if ($acquisition) {
    Assert-AICFNotContains $failures 'STAGE35_ALL_SLOT_ELIGIBILITY' $acquisition.Code 'GetRole\s*\(\s*\)\s*!=\s*AICF_EGroupRole\.ATTACK' 'DEFEND/QRF must not be excluded from vehicle eligibility'
    Assert-AICFContains $failures 'STAGE35_VEHICLE_PREFERENCE' $acquisition.Code 'GetUnitType[\s\S]*MOTORIZED_LIGHT[\s\S]*LIGHT_TRANSPORT[\s\S]*MOTORIZED_TRUCK[\s\S]*TRANSPORT[\s\S]*MOTORIZED_ARMED_LIGHT[\s\S]*ARMED_LIGHT' 'Vehicle preference must follow commander-selected transport or armed-light mobility type'
    Assert-AICFContains $failures 'STAGE35_CAPACITY_PREFLIGHT' $acquisition.Code 'requiredSeats|livingRoster|aliveAgents' 'Capacity preflight must use the complete living managed roster'
    Assert-AICFContains $failures 'STAGE35_CAPACITY_PREFLIGHT' $acquisition.Code 'capacity|Capacity|accessibleSeats' 'Acquisition must preflight candidate capacity before accepted binding'
    Assert-AICFContains $failures 'STAGE35_LIGHT_TO_TRUCK_FALLBACK' $acquisition.Code 'LIGHT_TRANSPORT[\s\S]*TRANSPORT' 'A roomy light rejection must advance to faction truck before foot fallback'
    Assert-AICFContains $failures 'STAGE35_MINIMUM_NEW_REQUEST' $acquisition.Code 'minimum|Minimum|GetMinimumVehicleRequestAgents' 'Acquisition admission must apply the minimum only to new requests'
    Assert-AICFContains $failures 'STAGE35_PRESERVE_ASSIGNED_ASSET' $acquisition.Code 'HasLease|assignedVehicle|PRESERVE_EXISTING|preserveExisting' 'An existing usable lease must survive later roster losses'
}

$selector = Find-AICFClassRecord $records 'AICF_TargetSelector'
if ($selector) {
    $attackPlan = Get-AICFMethodBody $selector 'SelectAttackPlanNode'
    $defendPlan = Get-AICFMethodBody $selector 'SelectDefendTarget'
    Assert-AICFContains $failures 'STAGE35_ATTACK_DISTRIBUTION' $attackPlan 'PRIMARY_RANKED_REACHABLE[\s\S]*ADJACENT_TO_PRIMARY[\s\S]*SUPPORT_ADJACENT_DIRECTION[\s\S]*SUPPORT_SECONDARY_DIRECTION' 'A0/A1/A2 must map deterministically to primary, adjacent, and support directions'
    Assert-AICFContains $failures 'STAGE35_ATTACK_DISTRIBUTION' $attackPlan 'AreAttackNodesAdjacent' 'Secondary/support choices must remain causally adjacent to the primary direction'
    Assert-AICFContains $failures 'STAGE35_D0_QRF' $defendPlan 'IsThreatened[\s\S]*QRF[\s\S]*HQ_THREAT[\s\S]*CONTESTED' 'D0 must prioritize threatened friendly bases with explicit QRF triggers'
}

$orderPlanner = Find-AICFClassRecord $records 'AICF_OrderPlanner'
if ($orderPlanner) {
    $reconcile = Get-AICFMethodBody $orderPlanner 'ReconcileStrategicOrder'
    Assert-AICFContains $failures 'STAGE35_QRF_HYSTERESIS' $reconcile 'urgentQRF[\s\S]*minimumDwellMs[\s\S]*stableCandidateMs[\s\S]*STRATEGIC_CANDIDATE_HELD' 'QRF escalation and stabilized return must retain hysteresis/minimum dwell'
    Assert-AICFContains $failures 'STAGE35_WAYPOINT_LIFECYCLE' $orderPlanner.Strings 'WAYPOINT_REMOVED' 'Infantry waypoint removal must remain observable'
    Assert-AICFContains $failures 'STAGE35_WAYPOINT_PREMATURE_COMPLETION' $orderPlanner.Code 'SetCompletionType\s*\(\s*EAIWaypointCompletionType\.All\s*\)[\s\S]*SetHoldingTime\s*\(\s*DEFEND_HOLDING_TIME_SECONDS\s*\)' 'Strategic movement and defend waypoints must not complete on a lone member or a short prefab hold'
}

if ($match) {
    $taskAudit = Get-AICFMethodBody $match 'AuditActiveFactionTasking'
    $meaningfulTask = ConvertTo-AICFCodeText (Get-AICFMethodBody $match 'HasMeaningfulTask')
    $waypointBind = ConvertTo-AICFCodeText (Get-AICFMethodBody $match 'IsWaypointBoundToGroup')
    $restore = Get-AICFMethodBody $match 'TryRecoverOrder'
    $safeWaitDelegate = ConvertTo-AICFCodeText (Get-AICFMethodBody $match 'IsSafeVehicleSpawnWait')

    Assert-AICFContains $failures 'STAGE35_MEANINGFUL_TASK_DEADLINE' $taskAudit 'HasMeaningfulTask[\s\S]*ObserveMeaningfulTaskLoss[\s\S]*TryRecoverOrder[\s\S]*2\s*\*\s*m_Config\.GetCommanderIntervalMs[\s\S]*MEANINGFUL_TASK_DEADLINE_MISSED' 'Every combat-ready slot must repair and hard-fail a taskless state within two commander intervals'
    Assert-AICFContains $failures 'STAGE35_MEANINGFUL_TASK_GRACE' $taskAudit 'CountAliveAgents[\s\S]*alive\s*<=\s*0[\s\S]*ObserveMeaningfulTaskLoss\s*\(\s*false\s*\)[\s\S]*continue' 'Empty managed groups must not emit authority task loss/recovery events'
    Assert-AICFContains $failures 'STAGE35_MEANINGFUL_TASK_GRACE' $taskAudit 'taskLossGraceMs[\s\S]*GetReliabilityIntervalMs[\s\S]*tasklessAgeMs\s*>=\s*taskLossGraceMs[\s\S]*MarkMeaningfulTaskLossReported' 'Transient handoff gaps must survive one reliability interval before MEANINGFUL_TASK_LOST'
    Assert-AICFContains $failures 'STAGE35_MEANINGFUL_TASK_GRACE' $taskAudit 'meaningfulTaskLossWasReported[\s\S]*MEANINGFUL_TASK_RECOVERED' 'MEANINGFUL_TASK_RECOVERED must require a previously emitted loss edge'
    Assert-AICFContains $failures 'STAGE35_MOB_EGRESS_DEADLINE' $taskAudit 'mobPresenceRequiresEgress[\s\S]*recentOutwardProgress[\s\S]*ObserveUnexplainedMobIdle\s*\(\s*mobPresenceRequiresEgress\s*&&\s*!recentOutwardProgress[\s\S]*MOB_EGRESS_HARD_DEADLINE_INTERVALS[\s\S]*MOB_EGRESS_DEADLINE_MISSED' 'Only continuously stalled MOB presence may fail the bounded hard deadline; recent outward progress must reset the stall episode'
    Assert-AICFContains $failures 'STAGE35_MOB_EGRESS_TELEMETRY' $taskAudit 'mob_presence_ms=[\s\S]*motion_age_ms=[\s\S]*egress_deadline_ms=' 'MOB egress failure must distinguish continuous presence from physical motion evidence'
    Assert-AICFContains $failures 'STAGE35_MEANINGFUL_TASK_PROOF' $meaningfulTask 'IsWaypointBoundToGroup' 'Meaningful task must be based on a waypoint actually bound to the group'
    Assert-AICFContains $failures 'STAGE35_MEANINGFUL_TASK_PROOF' $waypointBind 'GetWaypoints[\s\S]*Contains' 'Waypoint evidence must use the authoritative group queue'
    Assert-AICFContains $failures 'STAGE35_ORDER_RESTORE_PROOF' $restore 'GetWaypoints[\s\S]*Contains[\s\S]*boundToGroup|GetWaypoints[\s\S]*Contains[\s\S]*bound_to_group' 'Reliability restore must prove queue binding rather than allocation only'
    Assert-AICFContains $failures 'STAGE35_SAFE_WAIT_WHITELIST' $safeWaitDelegate 'vehicleView[\s\S]*IsSafeSpawnWait\s*\(\s*\)' 'MatchController must consume the immutable slot-view safe-wait projection'
    Assert-AICFNotContains $failures 'STAGE35_SAFE_WAIT_WHITELIST' $safeWaitDelegate 'switch\s*\(|case\s+' 'MatchController must not duplicate safe-site reason policy'
}

$tripController = Find-AICFClassRecord $records 'AICF_TransportTripController'
$transitFlow = Find-AICFClassRecord $records 'AICF_VehicleTransitFlow'
$dismountFlow = Find-AICFClassRecord $records 'AICF_VehicleDismountFlow'
$boardingFlow = Find-AICFClassRecord $records 'AICF_VehicleBoardingFlow'
if ($tripController -and $transitFlow -and $dismountFlow -and $boardingFlow) {
    $transitTick = Get-AICFMethodBody $transitFlow 'Tick'
    $prepareRoute = Get-AICFMethodBody $transitFlow 'PrepareRoute'
    $terminalClearance = Get-AICFMethodBody $dismountFlow 'ProcessTerminalClearance'
    $maintainPassengers = Get-AICFMethodBody $boardingFlow 'MaintainPassengerActions'
    Assert-AICFContains $failures 'STAGE35_DESTROYED_ROUTE_GUARD' $transitTick 'InspectRouteAssetFailure[\s\S]*GetSupersededRouteWaypoint[\s\S]*PrepareRoute' 'Destroyed/on-fire assets must be rejected before route reconciliation and creation'
    Assert-AICFContains $failures 'STAGE35_DESTROYED_ROUTE_GUARD' $prepareRoute 'InspectRouteAssetFailure[\s\S]*CreateAndStageRoute' 'PrepareRoute must not stage a waypoint for an unusable vehicle'
    Assert-AICFContains $failures 'STAGE35_TERMINAL_CLEARANCE_BOUND' $terminalClearance 'TERMINAL_DEADLINE_REACHED[\s\S]*AICF_TripOutcome\.ReleaseLease' 'Expired managed clearance must transfer to the independent protected cleanup scan'
	Assert-AICFContains $failures 'STAGE35_EXACT_CARGO_STALL' $maintainPassengers 'ObserveSpatialProgress[\s\S]*EAIActionState\.RUNNING[\s\S]*GetProgressAgeMs[\s\S]*PASSENGER_STALL_MS[\s\S]*ReissueExactCargo' 'Every RUNNING exact Cargo token, including an orphaned action reference, must have a bounded progress-aware retry'
	$canRetireTrip = ConvertTo-AICFCodeText (Get-AICFMethodBody $tripController 'CanRetireTrip')
	$advanceCleanup = ConvertTo-AICFCodeText (Get-AICFMethodBody $tripController 'AdvanceLeaseCleanup')
	Assert-AICFContains $failures 'STAGE35_RETAINED_CLEANUP_OWNERSHIP' $canRetireTrip 'IsCleanupRetainedFailClosed[\s\S]*IsCleanupOwnershipAcceptedTerminal[\s\S]*return true' 'Retained Trip retirement requires explicit independent cleanup ownership'
	Assert-AICFContains $failures 'STAGE35_RETAINED_CLEANUP_OWNERSHIP' $advanceCleanup 'IsRetainedFailClosed[\s\S]*AcknowledgeRetainedLease[\s\S]*RecordCleanupOwnershipAcceptedTerminal[\s\S]*DetachLease' 'Retained cleanup must transfer exact ownership before Trip lease detachment'
    foreach ($methodName in @('ReleaseSupersededTransitWaypoint', 'ReleaseCurrentTransitWaypoint', 'ReleaseSupersededDismountWaypoint', 'ReleaseCurrentDismountWaypoint')) {
        $lifecycleBody = ConvertTo-AICFCodeText (Get-AICFMethodBody $tripController $methodName)
        Assert-AICFContains $failures 'STAGE35_WAYPOINT_DELETE_ORDER' $lifecycleBody 'DetachVehicleWaypoint[\s\S]*Confirm[A-Za-z]*WaypointRemoved[\s\S]*DeleteDetachedVehicleWaypoint' "$methodName must detach, clear exact state, then delete the waypoint entity"
    }
}

if ($vehicleCoordinator) {
	$terminalObservation = ConvertTo-AICFCodeText (Get-AICFMethodBody $vehicleCoordinator 'ObserveTerminalCommit')
	Assert-AICFContains $failures 'STAGE35_TERMINAL_OUTCOME_COMMIT' $terminalObservation 'observed[\s\S]*observed\.IsTerminal[\s\S]*!trip\.IsTerminal[\s\S]*ObserveTripFailure' 'A terminal flow result may not be ignored unless the Trip has committed its terminal phase'
	Assert-AICFContains $failures 'STAGE35_TERMINAL_OUTCOME_COMMIT' $vehicleCoordinator.Strings 'UNCOMMITTED_TERMINAL_OUTCOME:' 'The uncommitted terminal guard must preserve an explicit failure reason'
}

$vehicleHandoff = Find-AICFClassRecord $records 'AICF_VehicleTaskHandoff'
if ($vehicleHandoff) {
	$detachWaypoint = ConvertTo-AICFCodeText (Get-AICFMethodBody $vehicleHandoff 'DetachVehicleWaypoint')
	$exactWaypointOwner = ConvertTo-AICFCodeText (Get-AICFMethodBody $vehicleHandoff 'IsExactTripOwnedVehicleWaypoint')
	Assert-AICFContains $failures 'STAGE35_TERMINAL_ASSET_LOSS_DETACH' $detachWaypoint 'IsExactTripOwnedVehicleWaypoint[\s\S]*RemoveWaypoint' 'Asset-loss terminal transition must detach the exact phase-owned waypoint'
	Assert-AICFNotContains $failures 'STAGE35_TERMINAL_ASSET_LOSS_DETACH' $detachWaypoint 'IsTripLeaseCurrent|HasPhysicalAsset|GetVehicle' 'Asset-loss terminal detachment may not depend on live vehicle identity'
	Assert-AICFContains $failures 'STAGE35_TERMINAL_ASSET_LOSS_DETACH' $exactWaypointOwner 'TRANSIT[\s\S]*GetRouteWaypoint[\s\S]*GetSupersededRouteWaypoint[\s\S]*DISMOUNT[\s\S]*GetDismountWaypoint[\s\S]*GetSupersededDismountWaypoint' 'Asset-independent detach must remain exact phase-state pointer fenced'
}

$cleanupManager = Find-AICFClassRecord $records 'AICF_VehicleCleanupManager'
if ($cleanupManager) {
	$leaseRelease = ConvertTo-AICFCodeText (Get-AICFMethodBody $cleanupManager 'ProcessLeaseRelease')
	$retainedAck = ConvertTo-AICFCodeText (Get-AICFMethodBody $cleanupManager 'AcknowledgeRetainedLease')
	Assert-AICFContains $failures 'STAGE35_BOUNDED_PROTECTED_CLEARANCE' $leaseRelease 'GetAbsoluteDeadlineMs[\s\S]*RetainFailClosed' 'WAIT_PROTECTED_CLEARANCE must end in an observable retained fail-closed outcome at its absolute deadline'
	Assert-AICFContains $failures 'STAGE35_BOUNDED_PROTECTED_CLEARANCE' $cleanupManager.Strings 'PROTECTED_CLEARANCE_DEADLINE_EXCEEDED:' 'The retained deadline outcome must preserve the protected blocker signature'
	foreach ($proof in @('job\.m_bReleaseComplete', 'FAILED_CLOSED', 'FleetContainsLease', 'FindLeaseForSlot', 'MatchesLease')) {
		Assert-AICFContains $failures 'STAGE35_RETAINED_CLEANUP_OWNERSHIP' $retainedAck $proof 'Retained ownership acknowledgement must prove the exact unreleased Fleet lease and cap holder'
	}
	Assert-AICFContains $failures 'STAGE35_RETAINED_CLEANUP_ACCEPTANCE' $cleanupManager.Code 'ObserveCleanupFailureFromFence' 'Protected-clearance deadline failure must reach acceptance before any release snapshot exists'
}

$slotView = Find-AICFClassRecord $records 'AICF_VehicleSlotView'
if ($slotView) {
    $slotViewConstructor = ConvertTo-AICFCodeText (Get-AICFMethodBody $slotView 'AICF_VehicleSlotView')
    $safeSiteReason = Get-AICFMethodBody $slotView 'IsSafeSiteReason'
    Assert-AICFContains $failures 'STAGE35_SAFE_WAIT_WHITELIST' $slotViewConstructor 'WAITING_FOR_SITE[\s\S]*IsSafeSiteReason\s*\(' 'Safe-site suppression must require the immutable WAITING_FOR_SITE projection'
    Assert-AICFContains $failures 'STAGE35_SAFE_WAIT_WHITELIST' $safeSiteReason 'SPAWN_POINT_DISABLED[\s\S]*NO_SAFE_SPAWN_AVAILABLE[\s\S]*NO_BOARDING_SITE_WITHIN_RANGE' 'Slot view must own the explicit safe-site wait whitelist'
    Assert-AICFNotContains $failures 'STAGE35_SAFE_WAIT_WHITELIST' $safeSiteReason 'case\s+"(?:VEHICLE_CAP_UNAVAILABLE|TRIP_CONTEXT_NOT_READY|POST_APPROACH_COHESION_WAIT)"' 'Cap, invalid trip context, and fragmented cohesion are not safe-site MOB exceptions'
} else {
    Add-AICFAuditFailure $failures 'STAGE35_COMPONENT_MISSING' 'Missing AICF_VehicleSlotView immutable projection'
}

$groupSlot = Find-AICFClassRecord $records 'AICF_GroupSlot'
if ($groupSlot) {
    $taskLoss = ConvertTo-AICFCodeText (Get-AICFMethodBody $groupSlot 'ObserveMeaningfulTaskLoss')
    Assert-AICFContains $failures 'STAGE35_MEANINGFUL_TASK_DEADLINE' $taskLoss '!taskLost[\s\S]*MeaningfulTaskLostStartedAtMs\s*=\s*0[\s\S]*MeaningfulTaskDeadlineReported\s*=\s*false' 'Task-loss deadline must reset only on recovery'
    Assert-AICFContains $failures 'STAGE35_FIELD_HOLD' $groupSlot.Code 'PersistentStuckFieldHold' 'Persistent-stuck reliability must retain preserved-group field hold state'
}

$marker = Find-AICFClassRecord $records 'AICF_GroupMapMarkerSystem'
if ($marker) {
    $markerText = ConvertTo-AICFCodeText (Get-AICFMethodBody $marker 'BuildMarkerText')
    Assert-AICFContains $failures 'STAGE35_ROLE_LOCAL_IDENTITY' $markerText 'RoleLocal|ROLE_LOCAL|GetRoleLocalMarkerKey' 'Marker identity must use A0/A1/A2/D0 role-local keys'
    Assert-AICFContains $failures 'STAGE35_ROLE_LOCAL_IDENTITY' $marker.Code 'GetRoleIndex' 'Dynamic role-local marker numbering must use the reindexed slot role ordinal'
}

$allStrings = ($records | ForEach-Object { $_.Strings }) -join "`n"
$requiredEvents = @(
    'CONFIG', 'GROUP_ENTITY_SPAWNED', 'ROSTER_SPAWN_REQUESTED', 'GROUP_SPAWN_AUDIT',
    'GROUP_SPAWN_TIMEOUT', 'GROUP_ROSTER_READY', 'STRATEGIC_ASSIGNMENT',
    'STRATEGIC_CANDIDATE_HELD', 'DEFEND_POSTURE_CHANGED', 'VEHICLE_CAPACITY_PREFLIGHT',
    'VEHICLE_TRANSPORT_FALLBACK', 'VEHICLE_REQUEST_INELIGIBLE', 'VEHICLE_SPAWN_CANDIDATE_REJECTED',
    'FORCE_HEARTBEAT', 'SLOT_ACTIVITY', 'MEANINGFUL_TASK_LOST', 'MEANINGFUL_TASK_RECOVERED',
    'MEANINGFUL_TASK_DEADLINE_MISSED', 'ORDER_RESTORE_REQUESTED', 'ORDER_RESTORE_RESULT',
    'WAYPOINT_REMOVED', 'WAYPOINT_BIND_MISMATCH', 'ABANDONED_EXIT_AUDIT', 'IDLE_DEADLINE_SUPPRESSED',
    'FORCE_DISEMBARK_MEMBER', 'COHESION_OUTCOME', 'WAITING_FOR_SITE_EXIT',
    'GROUP_ROSTER_CONFIG_INVALID', 'GROUP_ROSTER_REJECTED', 'MOB_EGRESS_DEADLINE_MISSED'
)
foreach ($eventName in $requiredEvents) {
    if ($allStrings -notmatch ('(?m)^' + [regex]::Escape($eventName) + '\r?$')) {
        Add-AICFAuditFailure $failures 'STAGE35_EVENT_CONTRACT' "Missing live string literal for Stage 3.5 event $eventName"
    }
}

foreach ($identityField in @('vehicle_lifecycle_id', 'operation_id', 'causation_id', 'trip_generation', 'lease_generation')) {
    if ($allStrings -notmatch [regex]::Escape($identityField)) {
        Add-AICFAuditFailure $failures 'STAGE35_DIAGNOSTIC_IDENTITY' "Diagnostics omit $identityField"
    }
}
foreach ($terminalField in @('logical_occupants', 'transitions', 'inside_bounds', 'restore_pending', 'meaningful_task', 'force_attempts', 'next_action')) {
    if ($allStrings -notmatch [regex]::Escape($terminalField)) {
        Add-AICFAuditFailure $failures 'STAGE35_TERMINAL_AUDIT' "Terminal pending telemetry omits $terminalField"
    }
}

Assert-AICFNotContains $failures 'STAGE35_NO_STATIC_PASS' $allStrings 'status=PASS' 'Static/runtime code must not administratively manufacture Stage 3.5 PASS'

if ($failures.Count -gt 0) {
    Write-Host "Stage 3.5 static audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
}

Write-Host 'Stage 3.5 static audit: PASS' -ForegroundColor Green
Write-Host 'Negative fixture self-check: PASS (COORDINATOR_SIDE_EFFECT, FLOW_CROSS_CALL, WAYPOINT_SIDE_EFFECT_OWNER, TRANSITION_OUTSIDE_CONTROLLER, TRANSITION_EFFECT_ORDER, WAITING_WITH_LEASE, HANDOFF_CLEARANCE_GATE, CLEANUP_CLEARANCE_OWNER, CLEANUP_IDENTITY_SAFETY, VEHICLE_LIVENESS_OWNERSHIP)'
Write-Host 'Checked: exact 5x4 rosters and 48/64 budget, 2/1/1 baseline and 3/1/0 active roles, A0/A1/A2/D0 planning, QRF hysteresis, all-slot lease admission and cap, faction vehicle policy, capacity/minimum-roster contracts, meaningful-task deadlines, independent handoff, cleanup-only physical-clearance proof, exclusive vehicle-waypoint queue ownership, ordered transition effects, Repeat-T2 telemetry, architecture ownership, and Enforce language limits.'
