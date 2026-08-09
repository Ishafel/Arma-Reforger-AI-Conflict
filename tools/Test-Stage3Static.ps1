param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Assert-FileContains {
    param(
        [string]$RelativePath,
        [string]$Pattern,
        [string]$Description
    )

    $path = Join-Path $RepositoryRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        $failures.Add("Missing file: $RelativePath")
        return
    }

    $content = Get-Content -LiteralPath $path -Raw
    if ($content -notmatch $Pattern) {
        $failures.Add("$Description ($RelativePath)")
    }
}

function Assert-FileNotContains {
    param(
        [string]$RelativePath,
        [string]$Pattern,
        [string]$Description
    )

    $path = Join-Path $RepositoryRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        $failures.Add("Missing file: $RelativePath")
        return
    }

    $content = Get-Content -LiteralPath $path -Raw
    if ($content -match $Pattern) {
        $failures.Add("$Description ($RelativePath)")
    }
}

function Get-CMethodBody {
    param(
        [string]$RelativePath,
        [string]$SignaturePattern,
        [string]$Description
    )

    $path = Join-Path $RepositoryRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        $failures.Add("Missing file: $RelativePath")
        return ''
    }

    $content = Get-Content -LiteralPath $path -Raw
    $matches = [regex]::Matches($content, $SignaturePattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if ($matches.Count -ne 1) {
        $failures.Add("$Description must have one unique method signature; found $($matches.Count) ($RelativePath)")
        return ''
    }

    $openBrace = $content.IndexOf('{', $matches[0].Index + $matches[0].Length)
    if ($openBrace -lt 0) {
        $failures.Add("$Description has no method body ($RelativePath)")
        return ''
    }

    $depth = 0
    for ($i = $openBrace; $i -lt $content.Length; $i++) {
        if ($content[$i] -eq '{') {
            $depth++
        }
        elseif ($content[$i] -eq '}') {
            $depth--
            if ($depth -eq 0) {
                return $content.Substring($openBrace, $i - $openBrace + 1)
            }
        }
    }

    $failures.Add("$Description has unbalanced braces ($RelativePath)")
    return ''
}

function Get-CGuardBody {
    param(
        [string]$Text,
        [string]$ConditionPattern,
        [string]$Description
    )

    $matches = [regex]::Matches($Text, $ConditionPattern)
    if ($matches.Count -ne 1) {
        $failures.Add("$Description must have one unique guard; found $($matches.Count)")
        return ''
    }

    $openBrace = $Text.IndexOf('{', $matches[0].Index + $matches[0].Length)
    if ($openBrace -lt 0) {
        $failures.Add("$Description has no guard body")
        return ''
    }

    $depth = 0
    for ($i = $openBrace; $i -lt $Text.Length; $i++) {
        if ($Text[$i] -eq '{') {
            $depth++
        }
        elseif ($Text[$i] -eq '}') {
            $depth--
            if ($depth -eq 0) {
                return $Text.Substring($openBrace, $i - $openBrace + 1)
            }
        }
    }

    $failures.Add("$Description has unbalanced braces")
    return ''
}

function Assert-TextContains {
    param([string]$Text, [string]$Pattern, [string]$Description)
    if ($Text -notmatch $Pattern) {
        $failures.Add($Description)
    }
}

function Assert-TextNotContains {
    param([string]$Text, [string]$Pattern, [string]$Description)
    if ($Text -match $Pattern) {
        $failures.Add($Description)
    }
}

function Assert-TextSequence {
    param([string]$Text, [string[]]$Patterns, [string]$Description)

    $cursor = 0
    foreach ($pattern in $Patterns) {
        $match = [regex]::Match($Text.Substring($cursor), $pattern)
        if (-not $match.Success) {
            $failures.Add($Description)
            return
        }
        $cursor += $match.Index + $match.Length
    }
}

function Assert-OccurrenceCount {
    param([string]$Text, [string]$Pattern, [int]$Expected, [string]$Description)
    $actual = [regex]::Matches($Text, $Pattern).Count
    if ($actual -ne $Expected) {
        $failures.Add("$Description; expected $Expected, found $actual")
    }
}

$coordinator = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleCoordinator.c'
$runtime = 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_VehicleRuntime.c'
$slot = 'AIConflictCore/Scripts/Game/AIConflict/State/AICF_GroupSlot.c'
$spawner = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleSpawner.c'
$waypoints = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleWaypointFactory.c'
$watchdog = 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_VehicleWatchdog.c'
$adapter = 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_ConflictAdapter.c'
$match = 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c'
$orderPlanner = 'AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_OrderPlanner.c'
$config = 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage3Config.c'
$stage1Config = 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_Stage1Config.c'
$groupMarkers = 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_GroupMapMarkers.c'

Assert-FileContains $config 'm_bVehiclesEnabled\s*=\s*false' 'Stage 3 must preserve Stage 2 unless explicitly enabled'
Assert-FileContains $spawner '!Replication\.IsServer\(\).*campaign\.IsMaster\(\)' 'Vehicle spawning must be server/master-only'
Assert-FileContains $spawner 'SCR_WorldTools\.FindEmptyTerrainPosition' 'Vehicle spawning must use an empty-terrain query'
Assert-FileContains $spawner 'affiliation\.SetAffiliatedFaction\(faction\)[\s\S]*affiliation\.GetAffiliatedFaction\(\)' 'Catalog vehicles must receive and validate their active faction after spawn'
Assert-FileContains $spawner 'out string failureReason[\s\S]*out bool retryable' 'Spawner must preserve exact terminal/retryable failure semantics'
Assert-FileContains $spawner 'MeasureAliveGroupDistancesToPosition[\s\S]*if \(aliveCount <= 0\)[\s\S]*siteFailureReason = "GROUP_NOT_READY"[\s\S]*break;[\s\S]*GetGame\(\)\.SpawnEntityPrefabEx' 'Spawner must not create an entity after the managed group loses every living member during exact-site preflight'
Assert-FileContains $adapter 'spawnFactionKey\.IsEmpty\(\) && base == faction\.GetMainBase\(\)[\s\S]*SPAWN_FACTION_INITIALIZING' 'An empty main-base spawn key must be treated as stock initialization, not a mismatch'
Assert-FileContains $coordinator 'SPAWN_FACTION_INITIALIZING[\s\S]*VEHICLE_SPAWN_DEFERRED' 'Stock spawn-point initialization must use a short non-warning retry'
Assert-FileContains $coordinator 'runtimes\[slotId\]\s*=\s*runtime;\s*slot\.SetVehicleRuntime\(runtime\);' 'A slot must reserve cap capacity before spawning'
Assert-FileContains $coordinator 'slot\.GetSpawnGeneration\(\)\s*==\s*runtime\.GetGroupGeneration\(\)' 'Runtime callbacks/polls must be guarded by group generation'
Assert-FileContains $runtime 'm_iVehicleGeneration\+\+' 'Safe reuse must advance vehicle generation'
Assert-FileContains $runtime 'm_sLastSpawnIssueReportKey\s*==\s*reportKey' 'Identical retryable spawn failures must be reported once per runtime generation'
Assert-FileNotContains $runtime 'm_bSpawnBlockedReported\s*=\s*false' 'State transitions must not reset spawn-failure suppression'
Assert-FileContains $runtime 'if \(m_State == state\)\s*return;' 'Repeated terminal polls must not reset state start time'
Assert-FileContains $runtime 'RestartPhaseDeadline\(\)' 'Multi-step crew recovery must own an explicit phase deadline'
Assert-FileContains $coordinator 'SetCrewRecoveryPhase\(AICF_EVehicleCrewRecoveryPhase\.DRIVER\)[\s\S]*RestartPhaseDeadline\(\)[\s\S]*SetCrewRecoveryPhase\(AICF_EVehicleCrewRecoveryPhase\.GUNNER\)[\s\S]*RestartPhaseDeadline\(\)' 'Driver and gunner recovery phases must each receive a fresh deadline'
Assert-FileContains $coordinator 'GUNNER_RECOVERY_TIMEOUT[\s\S]*DRIVER_RECOVERY_TIMEOUT' 'Crew recovery timeout diagnostics must identify the active phase'
Assert-FileContains $runtime 'AICF_EVehicleBoardingPhase m_BoardingPhase' 'Initial boarding must persist its role-ordered phase'
Assert-FileNotContains $waypoints 'CreateBoardingWaypoint\(Vehicle vehicle, bool allowGunner\)' 'A broad pilot-plus-cargo waypoint must not reintroduce the T3 seat race'
Assert-FileContains $watchdog 'IsAliveGroupMember\(SCR_AIGroup group, IEntity entity\)' 'Crew occupants must belong to the managed group'
Assert-FileContains $runtime 'm_iBoardingStartedAtMs' 'Initial boarding must retain a total deadline separate from the phase clock'
Assert-FileContains $coordinator 'phase_age_ms=%1 timeout_ms=%2 total_age_ms=%3 total_timeout_ms=%4 planned_phases=%5 deadline_scope=%6' 'Boarding timeout must expose phase and total bounded deadlines'

$waypointText = Get-Content -LiteralPath (Join-Path $RepositoryRoot $waypoints) -Raw
$coordinatorText = Get-Content -LiteralPath (Join-Path $RepositoryRoot $coordinator) -Raw
$spawnerText = Get-Content -LiteralPath (Join-Path $RepositoryRoot $spawner) -Raw
$trySpawnBody = Get-CMethodBody $spawner '^\s*bool\s+TrySpawn\s*\(' 'TrySpawn'
Assert-TextSequence $trySpawnBody @('GetSpawnRejectionReason\(candidate, faction\)', 'distanceSq = vector\.DistanceSqXZ', 'candidateKey = AICF_Stage1Diagnostics\.BaseKey\(candidate\)', 'for \(int safeIndex = 0; safeIndex < safeCandidates\.Count\(\); safeIndex\+\+\)', 'distanceSq < sortedDistanceSq', 'distanceSq == sortedDistanceSq && candidateKey\.Compare\(sortedCandidateKey\) < 0', 'safeCandidates\.InsertAt\(candidate, insertIndex\)') 'Safe friendly bases must be deterministically ordered by distance and base key before site validation'
Assert-TextSequence $trySpawnBody @('insertIndex >= safeCandidates\.Count\(\)', 'safeCandidates\.Insert\(candidate\)', 'else', 'safeCandidates\.InsertAt\(candidate, insertIndex\)') 'Sorted spawn candidates must use explicit append semantics at the end of the array'
Assert-TextSequence $trySpawnBody @('if \(safeCandidates\.IsEmpty\(\)\)', 'meaningfulRejectionReason', 'firstRejectionReason', 'NO_SAFE_SPAWN_AVAILABLE', 'retryable = true', 'return false') 'Unsafe-only failure must preserve the actionable initialization/ownership rejection contract'
Assert-TextSequence $trySpawnBody @('foreach \(SCR_CampaignMilitaryBaseComponent safeCandidate : safeCandidates\)', 'candidateDistanceSq', 'maximumSpawnDistanceMeters', 'siteFailureReason = "TOO_FAR"', 'continue;', 'FindEmptyTerrainPosition', 'siteFailureReason = "NO_EMPTY_TERRAIN"', 'continue;', 'MeasureAliveGroupDistancesToPosition', 'farthestDistanceMeters > maximumBoardingDistanceMeters', 'siteFailureReason = "NO_BOARDING_SITE_WITHIN_RANGE"', 'continue;', 'spawnBase = safeCandidate', 'position = candidatePosition', 'break;') 'Every ordered safe base must pass maximum distance, exact terrain, and all-member boarding distance before selection'
Assert-TextSequence $trySpawnBody @('siteFailureReason\.IsEmpty\(\)', '"TOO_FAR"', 'siteFailureReason\.IsEmpty\(\) \|\| siteFailureReason == "TOO_FAR"', '"NO_EMPTY_TERRAIN"', 'siteFailureReason != "NO_BOARDING_SITE_WITHIN_RANGE"', '"NO_BOARDING_SITE_WITHIN_RANGE"', 'failureReason = siteFailureReason', 'failureBase = siteFailureBase', 'retryable = true') 'Exhausted candidates must return the nearest most precise retryable site failure'
Assert-TextSequence $trySpawnBody @('siteFailureReason != "NO_BOARDING_SITE_WITHIN_RANGE"', 'boardingRejectionBase = safeCandidate', 'continue;', 'if \(boardingRejectionBase\)', 'AICF_Stage1Diagnostics\.BaseKey\(boardingRejectionBase\)', 'runtime\.MarkSpawnIssueReported\(reportKey\)', '"VEHICLE_SPAWN_SITE_REJECTED"') 'Exact boarding-site telemetry must report only the nearest precise rejection through the runtime one-shot key'
Assert-OccurrenceCount $trySpawnBody 'runtime\.MarkSpawnIssueReported\(reportKey\)' 1 'Candidate traversal must not rotate one-shot telemetry keys within one spawn attempt'
Assert-TextSequence $trySpawnBody @('if \(!spawnBase\)', 'return false;', '"VEHICLE_SPAWN_SITE_SELECTED"', 'GetGame\(\)\.SpawnEntityPrefabEx') 'No vehicle entity may be created until an exact viable candidate site has been selected'
Assert-TextSequence $trySpawnBody @('if \(preflightOnly\)', 'VEHICLE_SPAWN_PREFLIGHT_READY', 'entity_created=0', 'return true;', 'VEHICLE_SPAWN_SITE_SELECTED', 'SpawnEntityPrefabEx') 'WAITING_FOR_SITE preflight must return before the only entity-creation boundary'
Assert-OccurrenceCount $trySpawnBody 'SpawnEntityPrefabEx\(' 1 'TrySpawn must retain a single authoritative entity-creation boundary'
$driverWaypointBody = Get-CMethodBody $waypoints '^\s*SCR_BoardingEntityWaypoint\s+CreateDriverBoardingWaypoint\s*\(' 'CreateDriverBoardingWaypoint'
$gunnerWaypointBody = Get-CMethodBody $waypoints '^\s*SCR_BoardingEntityWaypoint\s+CreateGunnerBoardingWaypoint\s*\(' 'CreateGunnerBoardingWaypoint'
$passengerWaypointBody = Get-CMethodBody $waypoints '^\s*SCR_BoardingEntityWaypoint\s+CreatePassengerBoardingWaypoint\s*\(' 'CreatePassengerBoardingWaypoint'
$roleWaypointBody = Get-CMethodBody $waypoints '^\s*protected\s+SCR_BoardingEntityWaypoint\s+CreateRoleBoardingWaypoint\s*\(' 'CreateRoleBoardingWaypoint'
Assert-TextContains $driverWaypointBody '^\{\s*return\s+CreateRoleBoardingWaypoint\(vehicle,\s*true,\s*false,\s*false\);\s*\}$' 'Driver wrapper must map only to pilot allowance'
Assert-TextContains $gunnerWaypointBody '^\{\s*return\s+CreateRoleBoardingWaypoint\(vehicle,\s*false,\s*true,\s*false\);\s*\}$' 'Gunner wrapper must map only to turret allowance'
Assert-TextContains $passengerWaypointBody '^\{\s*return\s+CreateRoleBoardingWaypoint\(vehicle,\s*false,\s*false,\s*true\);\s*\}$' 'Passenger wrapper must map only to cargo allowance'
Assert-OccurrenceCount $waypointText 'return\s+CreateRoleBoardingWaypoint\(' 3 'Exactly three public role wrappers must call CreateRoleBoardingWaypoint'
Assert-TextContains $roleWaypointBody 'SetAllowance\(allowDriver,\s*allowGunner,\s*allowCargo\)' 'Generic role waypoint must forward the wrapper allowances unchanged'
Assert-OccurrenceCount $waypointText 'CreateRoleBoardingWaypoint\(' 4 'Only the generic definition and three exact wrappers may reference CreateRoleBoardingWaypoint'

$startBoardingBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+StartBoarding\s*\(' 'StartBoarding'
$startBoardingPhaseBody = Get-CMethodBody $coordinator '^\s*protected\s+bool\s+StartBoardingPhase\s*\(' 'StartBoardingPhase'
$processBoardingBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessBoarding\s*\(' 'ProcessBoarding'
$completeBoardingBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+CompleteBoarding\s*\(' 'CompleteBoarding'
$attachVehicleBody = Get-CMethodBody $coordinator '^\s*protected\s+bool\s+AttachVehicleToGroup\s*\(' 'AttachVehicleToGroup'
$processBoardingRoleResetBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessBoardingRoleReset\s*\(' 'ProcessBoardingRoleReset'
$issueBoardingRoleRetryBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+IssueBoardingRoleRetry\s*\(' 'IssueBoardingRoleRetry'
$rejectBoardingRoleViolationBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+RejectBoardingRoleViolation\s*\(' 'RejectBoardingRoleViolation'
$getBoardingRoleResetTimeoutBody = Get-CMethodBody $coordinator '^\s*protected\s+int\s+GetBoardingRoleResetTimeoutMs\s*\(' 'GetBoardingRoleResetTimeoutMs'
$processMovingBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessMoving\s*\(' 'ProcessMoving'
$processFactionBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessFaction\s*\(' 'ProcessFaction'
$beginBoardingDeadlineBody = Get-CMethodBody $runtime '^\s*void\s+BeginBoardingDeadline\s*\(' 'BeginBoardingDeadline'
$getBoardingAgeBody = Get-CMethodBody $runtime '^\s*int\s+GetBoardingAgeMs\s*\(' 'GetBoardingAgeMs'
$beginBoardingRoleResetBody = Get-CMethodBody $runtime '^\s*bool\s+BeginBoardingRoleReset\s*\(' 'BeginBoardingRoleReset'
$markBoardingRoleRetryBody = Get-CMethodBody $runtime '^\s*void\s+MarkBoardingRoleRetryIssued\s*\(' 'MarkBoardingRoleRetryIssued'
$calculateBoardingPhasesBody = Get-CMethodBody $coordinator '^\s*protected\s+int\s+CalculatePlannedBoardingPhaseCount\s*\(' 'CalculatePlannedBoardingPhaseCount'
$continueRoleBoardingBody = Get-CMethodBody $coordinator '^\s*protected\s+bool\s+ContinueRoleOrderedBoarding\s*\(' 'ContinueRoleOrderedBoarding'
$observeBoardingProgressBody = Get-CMethodBody $runtime '^\s*bool\s+ObserveBoardingProgress\s*\(' 'ObserveBoardingProgress'
$observeApproachProgressBody = Get-CMethodBody $runtime '^\s*bool\s+ObserveBoardingApproachProgress\s*\(' 'ObserveBoardingApproachProgress'
$evaluateBoardingGraceBody = Get-CMethodBody $runtime '^\s*bool\s+EvaluateBoardingTransitionGrace\s*\(' 'EvaluateBoardingTransitionGrace'
$inspectBoardingProgressBody = Get-CMethodBody $watchdog '^\s*bool\s+InspectBoardingProgress\s*\(' 'InspectBoardingProgress'
$startApproachActionsBody = Get-CMethodBody $coordinator '^\s*protected\s+bool\s+StartBoardingApproachActions\s*\(' 'StartBoardingApproachActions'
$maintainApproachActionsBody = Get-CMethodBody $coordinator '^\s*protected\s+bool\s+MaintainBoardingApproachActions\s*\(' 'MaintainBoardingApproachActions'
$cancelApproachActionsBody = Get-CMethodBody $runtime '^\s*int\s+CancelBoardingApproachActions\s*\(' 'CancelBoardingApproachActions'
$bindVehicleBody = Get-CMethodBody $runtime '^\s*bool\s+BindVehicle\s*\(' 'BindVehicle'
$beginReuseBody = Get-CMethodBody $runtime '^\s*void\s+BeginReuse\s*\(' 'BeginReuse'
$measureDistancesBody = Get-CMethodBody $watchdog '^\s*bool\s+MeasureAliveGroupDistances\s*\(' 'MeasureAliveGroupDistances'
$resetVehicleActionsBody = Get-CMethodBody $watchdog '^\s*int\s+ResetGroupVehicleActions\s*\(' 'ResetGroupVehicleActions'
$describeOccupantsBody = Get-CMethodBody $watchdog '^\s*string\s+DescribeGroupVehicleOccupants\s*\(' 'DescribeGroupVehicleOccupants'
$tooFarGuardBody = Get-CGuardBody $startBoardingBody 'if \(distanceAlive > 0 && farthestDistanceMeters > maximumBoardingDistanceMeters\)' 'StartBoarding farthest-member distance gate'
Assert-TextSequence $startBoardingBody @('MeasureAliveGroupDistances\(', 'GetMaximumReuseDistanceMeters\(\)', 'farthestDistanceMeters > maximumBoardingDistanceMeters', '"BOARDING_REJECTED"', '"VEHICLE_TOO_FAR"', 'BeginFallback\(runtime, faction, slot, "VEHICLE_TOO_FAR"\)', 'ResetGroupVehicleActions\(group\)', 'CountAccessibleSeats\(', 'CountAliveGroupMembersInVehicle\(', 'availableCapacity = accessibleSeats \+ mounted', 'DetachVehicleFromGroup\(runtime\)', 'SetState\(AICF_EVehicleState\.BOARDING\)', 'CalculatePlannedBoardingPhaseCount', 'BeginBoardingDeadline\(') 'Boarding must reject a distant member before action reset, utility attachment, or any role action'
Assert-TextNotContains $tooFarGuardBody 'AddUsableVehicle|AttachVehicleToGroup|StartBoardingPhase|CreateCrewRecoveryAction|CreatePassengerBoardingWaypoint' 'The distance rejection branch must never attach or issue a GetIn action'
Assert-TextSequence $tooFarGuardBody @('"BOARDING_REJECTED"', 'leader_m=', 'nearest_m=', 'farthest_m=', 'maximum_m=', 'member_samples=', 'BeginFallback\(runtime, faction, slot, "VEHICLE_TOO_FAR"\)', 'return;') 'Distance rejection must expose leader/nearest/farthest/member telemetry and immediately fall back'
Assert-TextNotContains $startBoardingBody 'AddUsableVehicle\(' 'Initial DRIVER/GUNNER ordering must not expose the vehicle to generic group utility'
Assert-TextSequence $attachVehicleBody @('GetGroupUtilityComponent\(\)', 'AddUsableVehicle\(runtime\.GetVehicleUsage\(\)\)', 'return true') 'Vehicle utility attachment must be centralized after mandatory crew occupation'
Assert-OccurrenceCount $attachVehicleBody 'AddUsableVehicle\(' 1 'The centralized attachment helper must add one usable vehicle'
Assert-TextSequence $measureDistancesBody @('ResolveAliveLeader\(group\)', 'GetAgents\(agents\)', 'IsAliveCharacter\(entity\)', 'DistanceSqXZ\(entity\.GetOrigin\(\), vehicle\.GetOrigin\(\)\)', 'nearestDistanceMeters = Math\.Min', 'farthestDistanceMeters = Math\.Max', 'entity == leader', 'memberSamples \+= string\.Format') 'Distance telemetry must sample every alive member in XZ and separately retain leader/nearest/farthest values'
Assert-TextSequence $resetVehicleActionsBody @('group\.ReleaseCompartments\(\)', 'GetAgents\(agents\)', 'GetLifeState\(\) == ECharacterLifeState\.DEAD', 'InterruptVehicleActionQueue\(true, true, true\)') 'Seat normalization must release reservations and interrupt stale vehicle queues for non-dead members'
Assert-TextSequence $describeOccupantsBody @('GetPilotCompartmentSlot', 'GetTurretCompartmentSlot', 'GetVehicleIn\(character\) != runtime\.GetVehicle\(\)', 'role = "CARGO"', 'role = "DRIVER"', 'role = "GUNNER"') 'Occupant diagnostics must identify current-group driver, gunner and cargo occupants'
Assert-TextSequence $startBoardingBody @('ResolveAliveDriver\(runtime\)', 'IsAliveGroupMember\(group, driver\)', 'ResolveAliveGunner\(runtime\)', 'IsAliveGroupMember\(group, gunner\)', 'CountAccessibleSeats\(', 'CountAliveGroupMembersInVehicle\(', 'availableCapacity = accessibleSeats \+ mounted') 'Safe reuse must validate mounted current-group roles and add them back to empty-seat capacity'
Assert-TextContains $startBoardingBody '\(!driver && !hasFreePilot\)' 'An already-mounted current-group driver must satisfy the pilot requirement'
Assert-TextContains $startBoardingBody '\(!gunner && !hasFreeTurret\)' 'An already-mounted current-group gunner must satisfy the armed turret requirement'
Assert-TextNotContains $startBoardingBody 'accessibleSeats < aliveAgents' 'Empty-seat capacity alone must not reject a partially mounted reuse group'
Assert-TextSequence $startBoardingBody @('farthestDistanceMeters > stagingThresholdMeters', 'CalculatePlannedBoardingPhaseCount', 'BeginBoardingDeadline\(', 'if \(needsApproach\)', 'StartBoardingPhase\(runtime, slot, AICF_EVehicleBoardingPhase\.APPROACH\)', 'ContinueRoleOrderedBoarding') 'A distant group must stage together before the centralized exact-role selector runs'
Assert-TextSequence $startBoardingBody @('driverPhasePlanned = driver == null', 'gunnerPhasePlanned = runtime\.GetKind\(\) == AICF_EVehicleKind\.ARMED_LIGHT', '\(gunner == null \|\| driverPhasePlanned\)', 'CalculatePlannedBoardingPhaseCount', 'driverPhasePlanned,', 'gunnerPhasePlanned\);', 'BeginBoardingDeadline', 'plannedPhases,', 'driverPhasePlanned,', 'gunnerPhasePlanned\);') 'An armed plan must reserve a gunner phase whenever a missing driver can force an existing gunner out during role normalization'
Assert-TextContains $coordinatorText 'BOARDING_STAGING_THRESHOLD_METERS\s*=\s*75\.0\s*;' 'Staging must leave a safety margin inside the stock 100 metre vehicle-search radius'
Assert-TextContains $coordinatorText 'BOARDING_APPROACH_ACTION_RADIUS_METERS\s*=\s*70\.0\s*;' 'Every exact approach action must stop inside the authoritative seventy-five metre staging threshold'
Assert-TextContains $coordinatorText 'BOARDING_APPROACH_STALL_MS\s*=\s*15000\s*;' 'A non-progressing member action must have a finite stall window'
Assert-TextContains $coordinatorText 'BOARDING_APPROACH_MAX_RETRIES\s*=\s*1\s*;' 'Each member approach action must have one bounded retry'
Assert-OccurrenceCount $startBoardingBody 'StartBoardingPhase\(' 1 'StartBoarding may directly issue only the optional MOVE-only approach phase'
Assert-TextNotContains $startBoardingPhaseBody 'SetState\(' 'Boarding phase transitions must not restart the BOARDING state or total deadline'
Assert-TextNotContains $startBoardingPhaseBody 'BeginBoardingDeadline\(' 'Boarding phase transitions must not restart the immutable total deadline'
Assert-OccurrenceCount $processBoardingBody 'RestartPhaseDeadline\(' 1 'Only the APPROACH-to-DRIVER wrong-seat transition may start a fresh phase deadline during boarding polling'
Assert-TextSequence $processBoardingBody @('BOARDING_APPROACH_COMPLETE', 'currentDriver', 'currentMounted', '!currentDriver && currentMounted > 0', 'SetBoardingPhase\(AICF_EVehicleBoardingPhase\.DRIVER\)', 'RestartPhaseDeadline\(\)', 'ProcessBoardingRoleReset', 'return;') 'Approach completion must normalize a pre-existing wrong seat before any exact DRIVER action and start that new phase clock once'
Assert-TextSequence $startBoardingBody @('!driver && mounted > 0', 'SetBoardingPhase\(AICF_EVehicleBoardingPhase\.DRIVER\)', 'RestartPhaseDeadline\(\)', 'ProcessBoardingRoleReset', 'return;', 'ContinueRoleOrderedBoarding') 'Near-start wrong-seat occupancy must be normalized synchronously before the exact role selector runs'
Assert-TextNotContains $processBoardingBody 'BeginBoardingDeadline\(' 'Polling an unchanged boarding phase must never extend the total deadline'
Assert-OccurrenceCount $coordinatorText 'BeginBoardingDeadline\(' 1 'Only StartBoarding may start the immutable total boarding deadline'
Assert-OccurrenceCount $coordinatorText 'StartBoardingPhase\(' 10 'Only the definition, approach start, centralized role starts, guarded forward transitions and bounded driver retry may reference StartBoardingPhase'
Assert-OccurrenceCount $startBoardingPhaseBody 'RestartPhaseDeadline\(' 2 'Only the no-cargo and assigned-waypoint success paths may start a fresh phase deadline'
Assert-TextSequence $startBoardingPhaseBody @('phase == AICF_EVehicleBoardingPhase\.APPROACH', 'DetachVehicleFromGroup\(runtime\)', 'StartBoardingApproachActions\(runtime, slot\.GetGroup\(\), runtime\.GetVehicle\(\)\)', 'phase == AICF_EVehicleBoardingPhase\.DRIVER', 'CreateCrewRecoveryAction\(runtime, driverAgent, EAICompartmentType\.Pilot\)', 'TrackCrewRecovery\(driverAgent, driverAction\)', 'phase == AICF_EVehicleBoardingPhase\.GUNNER', 'CreateCrewRecoveryAction\(runtime, gunnerAgent, EAICompartmentType\.Turret\)', 'TrackCrewRecovery\(gunnerAgent, gunnerAction\)', 'phase == AICF_EVehicleBoardingPhase\.PASSENGERS', 'AttachVehicleToGroup\(runtime, slot\)', 'CreatePassengerBoardingWaypoint') 'Exact per-member staging and mandatory crew actions must precede cargo utility attachment'
Assert-TextSequence $startApproachActionsBody @('CancelBoardingApproachActions\(runtime\)', 'group\.GetAgents\(agents\)', 'IsAliveCharacter', 'distanceMeters <= thresholdMeters', 'CreateBoardingApproachAction', 'TrackBoardingApproachAction') 'APPROACH must issue a separately tracked normal movement action for every distant alive member'
Assert-TextNotContains $startApproachActionsBody 'AddWaypointAt|CreateBoardingApproachWaypoint|SetAllowance|SCR_AIGetInVehicle' 'APPROACH must not use a leader-completable group waypoint or expose a GetIn action'
Assert-TextNotContains $startApproachActionsBody '\bbreak\s*;' 'APPROACH must visit every alive member rather than stopping after the first action token'
Assert-TextSequence $maintainApproachActionsBody @('FindBoardingApproachAction\(agent\)', 'token\.ObserveProgress', 'GetActionState\(\)', 'BOARDING_APPROACH_STALL_MS', 'retryCount >= BOARDING_APPROACH_MAX_RETRIES', 'CancelBoardingApproachAction', 'TrackBoardingApproachAction', 'BOARDING_APPROACH_REISSUED') 'Each member action must be polled, progress-checked and reissued only within its bounded budget'
Assert-TextSequence $cancelApproachActionsBody @('GetActionState\(\)', 'state == EAIActionState\.COMPLETED \|\| state == EAIActionState\.FAILED', 'token\.GetAction\(\)\.Fail\(\)', 'm_aBoardingApproachActions\.Clear\(\)') 'Approach cleanup must fail only live exact action tokens and clear all tracking'
Assert-TextSequence $beginBoardingDeadlineBody @('m_iBoardingStartedAtMs > 0', 'return;', 'plannedPhaseCount < 1', 'plannedPhaseCount > 4', 'm_iBoardingStartedAtMs = System\.GetTickCount\(\)', 'm_iPlannedBoardingPhaseCount = plannedPhaseCount') 'The immutable total clock and finite dynamic phase plan must be captured once'
Assert-TextNotContains $beginBoardingDeadlineBody 'm_iStateStartedAtMs|RestartPhaseDeadline|SetState' 'Starting the total boarding clock must not mutate phase or state clocks'
Assert-TextSequence $getBoardingAgeBody @('m_iBoardingStartedAtMs <= 0', 'System\.GetTickCount\(m_iBoardingStartedAtMs\)') 'Total boarding age must use wrap-safe tick accounting'
Assert-TextSequence $calculateBoardingPhasesBody @('int plannedPhases = 1', 'approachPlanned', 'plannedPhases\+\+', 'driverPlanned', 'plannedPhases\+\+', 'AICF_EVehicleKind\.ARMED_LIGHT && gunnerPlanned', 'plannedPhases\+\+', 'return plannedPhases') 'Dynamic total budget must include only passenger plus actually planned approach/mandatory roles'
Assert-TextContains $bindVehicleBody 'm_iBoardingStartedAtMs = 0' 'Binding a new vehicle must clear stale total boarding age'
Assert-TextContains $beginReuseBody 'm_iBoardingStartedAtMs = 0' 'Reusing a vehicle must clear stale total boarding age'
Assert-TextSequence $beginBoardingDeadlineBody @('m_bBoardingRoleResetAttempted = false', 'm_bBoardingRoleRetryIssued = false', 'm_iBoardingRoleResetAtMs = 0') 'A new total boarding attempt must reset the one-shot role-repair state exactly at the attempt boundary'
Assert-TextSequence $bindVehicleBody @('m_bBoardingRoleResetAttempted = false', 'm_bBoardingRoleRetryIssued = false', 'm_iBoardingRoleResetAtMs = 0') 'Binding a new vehicle must clear stale role-repair state'
Assert-TextSequence $beginReuseBody @('m_bBoardingRoleResetAttempted = false', 'm_bBoardingRoleRetryIssued = false', 'm_iBoardingRoleResetAtMs = 0') 'Safe reuse must clear stale role-repair state'
Assert-TextSequence $beginBoardingRoleResetBody @('if \(m_bBoardingRoleResetAttempted\)', 'return false', 'm_bBoardingRoleResetAttempted = true', 'm_bBoardingRoleRetryIssued = false', 'm_iBoardingRoleResetAtMs = System\.GetTickCount\(\)', 'return true') 'Runtime role reset must be a one-shot transition with its own bounded clock'
Assert-TextContains $markBoardingRoleRetryBody '^\{\s*m_bBoardingRoleRetryIssued = true;\s*\}$' 'Runtime must persist the single driver retry issuance'
Assert-TextSequence $processBoardingBody @('InspectBoardingProgress', 'ObserveBoardingProgress', 'DescribeBoardingApproachActions', 'AICF_EVehicleBoardingPhase\.APPROACH', 'MaintainBoardingApproachActions', 'farthestDistanceMeters <= stagingThresholdMeters', 'RecordBoardingSettledPoll\(true\)', 'BOARDING_SETTLED_POLLS_REQUIRED', 'CancelBoardingApproachActions', 'BOARDING_APPROACH_COMPLETE', 'ContinueRoleOrderedBoarding', 'ResolveAliveDriver\(runtime\)', 'IsAliveGroupMember\(group, driver\)', 'ResolveAliveGunner\(runtime\)', 'IsAliveGroupMember\(group, gunner\)') 'Boarding must stage every alive member for two polls, expose action progress, then validate current-group crew'
Assert-TextSequence $processBoardingBody @('if \(!MaintainBoardingApproachActions', 'BOARDING_APPROACH_MEMBER_STALLED', 'LatchAcceptanceFailure\(runtime, "BOARDING_APPROACH_MEMBER_STALLED"\)', 'BeginFallback') 'Exhausted member movement must invalidate acceptance before bounded fallback'
Assert-TextSequence $inspectBoardingProgressBody @('GetBounds', 'CoordToLocal', 'targetScoped = linked \|\| insideTransitionScope', 'inCompartment = linked && access && access\.IsInCompartment\(\)', 'gettingIn = targetScoped && access && access\.IsGettingIn\(\)', 'characterVehicle = linked && character\.IsInVehicle\(\)', 'settled = linked && inCompartment && !gettingIn && !gettingOut && characterVehicle') 'Boarding progress and transition grace must be scoped to the target vehicle'
Assert-TextSequence $observeApproachProgressBody @('m_fBestBoardingFarthestDistanceMeters', 'minimumProgressMeters', 'm_iLastBoardingProgressAtMs = System\.GetTickCount\(\)') 'Approach grace may rely only on monotonic farthest-member progress'
Assert-TextSequence $processBoardingBody @('phaseAgeMs >= phaseTimeoutMs', 'totalAgeMs >= totalTimeoutMs', 'softDeadlineExpired = phaseExpired \|\| totalExpired', 'EvaluateBoardingTransitionGrace\(graceEligible\)', 'phaseTimeoutMs \+ BOARDING_TRANSITION_GRACE_MS', 'totalTimeoutMs \+ BOARDING_TRANSITION_GRACE_MS') 'Every phase must retain a soft deadline, one non-rolling grace decision and absolute phase/total hard caps'
Assert-TextSequence $evaluateBoardingGraceBody @('m_bBoardingGraceEvaluated', 'return m_bBoardingGraceGranted', 'm_bBoardingGraceEvaluated = true', 'm_bBoardingGraceGranted = eligible') 'Transition grace must be evaluated only once for the whole immutable boarding attempt'
Assert-TextContains $coordinatorText 'BOARDING_TRANSITION_GRACE_MS\s*=\s*10000\s*;' 'Boarding transition grace must remain a fixed ten-second hard allowance'
$driverPhaseBody = Get-CGuardBody $processBoardingBody 'if \(phase == AICF_EVehicleBoardingPhase\.DRIVER && driverSettled\)' 'ProcessBoarding DRIVER phase'
$gunnerPhaseBody = Get-CGuardBody $processBoardingBody 'if \(phase == AICF_EVehicleBoardingPhase\.GUNNER\)' 'ProcessBoarding GUNNER phase'
$passengerPhaseBody = Get-CGuardBody $processBoardingBody 'if \(phase == AICF_EVehicleBoardingPhase\.PASSENGERS\)' 'ProcessBoarding PASSENGERS phase'
$noCargoPhaseBody = Get-CGuardBody $startBoardingPhaseBody 'if \(passengerCount == 0\)' 'StartBoardingPhase no-cargo branch'
$armedDriverPhaseBody = Get-CGuardBody $driverPhaseBody 'if \(runtime\.GetKind\(\) == AICF_EVehicleKind\.ARMED_LIGHT\)' 'ProcessBoarding armed DRIVER branch'
$armedSettledGunnerBody = Get-CGuardBody $armedDriverPhaseBody 'if \(gunnerSettled\)' 'ProcessBoarding armed DRIVER settled-gunner branch'
$occupiedGunnerBody = Get-CGuardBody $gunnerPhaseBody 'if \(gunnerSettled\)' 'ProcessBoarding settled GUNNER branch'
$allPassengersMountedBody = Get-CGuardBody $passengerPhaseBody 'if \(settledPolls >= BOARDING_SETTLED_POLLS_REQUIRED\)' 'ProcessBoarding stable settled PASSENGERS branch'
$wrongSeatDriverGuard = 'if \(phase == AICF_EVehicleBoardingPhase\.DRIVER && !driver &&\s*\(mounted > 0 \|\| \(runtime\.IsBoardingRoleResetAttempted\(\) && !runtime\.IsBoardingRoleRetryIssued\(\)\)\)\)'
$wrongSeatDriverBody = Get-CGuardBody $processBoardingBody $wrongSeatDriverGuard 'ProcessBoarding mounted-without-driver or pending-reset branch'
Assert-TextSequence $processBoardingBody @('int mounted = m_Watchdog\.CountAliveGroupMembersInVehicle', $wrongSeatDriverGuard, 'ProcessBoardingRoleReset\(', 'return;', 'phase == AICF_EVehicleBoardingPhase\.DRIVER && driverSettled') 'A wrong-seat DRIVER phase and its asynchronous empty transition must remain inside bounded normalization until retry is issued'
Assert-TextSequence $wrongSeatDriverBody @('ProcessBoardingRoleReset\(', 'return;') 'Wrong-seat detection must stop the current boarding poll after role normalization'
Assert-TextNotContains $wrongSeatDriverBody 'RestartPhaseDeadline\(' 'Polling an unchanged DRIVER normalization phase must never extend its deadline'
Assert-TextContains $armedDriverPhaseBody 'StartBoardingPhase\(runtime, slot, AICF_EVehicleBoardingPhase\.GUNNER\)' 'Armed DRIVER phase must issue a planned gunner action when the turret is not settled'
Assert-TextSequence $armedSettledGunnerBody @('StartBoardingPhase\(runtime, slot, AICF_EVehicleBoardingPhase\.PASSENGERS\)', 'return;') 'Armed DRIVER phase may skip a redundant turret action only for an already-settled current-group gunner'
Assert-TextSequence $armedDriverPhaseBody @('if \(gunnerSettled\)', 'AICF_EVehicleBoardingPhase\.PASSENGERS', 'return;', '!runtime\.IsBoardingGunnerPhasePlanned\(\)', 'GUNNER_LOST_BEFORE_ROLE_PHASE', 'AICF_EVehicleBoardingPhase\.GUNNER') 'An unsettled armed gunner must consume the immutable planned role phase before cargo starts'
Assert-OccurrenceCount $driverPhaseBody 'StartBoardingPhase\(runtime, slot, AICF_EVehicleBoardingPhase\.GUNNER\)' 1 'DRIVER phase must issue exactly one armed gunner transition'
Assert-OccurrenceCount $driverPhaseBody 'StartBoardingPhase\(runtime, slot, AICF_EVehicleBoardingPhase\.PASSENGERS\)' 2 'DRIVER phase may reach passengers only through the settled-gunner armed path or the transport path'
Assert-TextSequence $driverPhaseBody @('AICF_EVehicleKind\.ARMED_LIGHT', 'AICF_EVehicleBoardingPhase\.GUNNER', 'return;', 'AICF_EVehicleBoardingPhase\.PASSENGERS') 'The ordinary transport passenger transition must remain outside the armed-only branch'
Assert-TextContains $occupiedGunnerBody 'StartBoardingPhase\(runtime, slot, AICF_EVehicleBoardingPhase\.PASSENGERS\)' 'GUNNER phase must wait for a real turret occupant before passengers'
Assert-OccurrenceCount $gunnerPhaseBody 'StartBoardingPhase\(runtime, slot, AICF_EVehicleBoardingPhase\.PASSENGERS\)' 1 'GUNNER phase must issue exactly one passenger transition'
Assert-TextSequence $passengerPhaseBody @('!driverSettled', '!gunnerSettled', 'settledCount == aliveCount', 'AreAllAliveMembersSettledInVehicle', 'RecordBoardingSettledPoll', 'BOARDING_SETTLED_POLLS_REQUIRED') 'PASSENGERS phase must retain mandatory crew and two-poll settled guards'
Assert-TextSequence $allPassengersMountedBody @('CompleteBoarding\(', 'return;') 'Only the fully mounted PASSENGERS branch may invoke the shared completion helper'
Assert-OccurrenceCount $passengerPhaseBody 'CompleteBoarding\(' 1 'PASSENGERS phase must complete through the shared helper exactly once'
Assert-TextNotContains $passengerPhaseBody 'StartBoardingPhase\(' 'PASSENGERS is terminal within the forward-only boarding phase graph'
Assert-TextSequence $completeBoardingBody @('CountAliveGroupMembersInVehicle', 'AttachVehicleToGroup\(runtime, slot\)', 'DeleteRuntimeWaypoint', 'ClearCrewRecoveryTracking', 'SetBoardingPhase\(AICF_EVehicleBoardingPhase\.NONE\)', '"BOARDING_COMPLETE"', 'StartMovement\(') 'Shared boarding completion must attach utility only after crew, then clear the phase before movement'
Assert-OccurrenceCount $coordinatorText 'CompleteBoarding\(' 2 'Only the helper definition and two-poll PASSENGERS completion may reference CompleteBoarding'
Assert-OccurrenceCount $processBoardingRoleResetBody 'ForceAliveGroupMembersOut\(' 1 'Role normalization may force occupants out only once'
Assert-TextSequence $processBoardingRoleResetBody @('!runtime\.IsBoardingRoleResetAttempted\(\)', 'BeginBoardingRoleReset\(\)', 'DeleteRuntimeWaypoint\(runtime\)', 'ResetGroupVehicleActions\(group\)', 'ForceAliveGroupMembersOut\(', 'CountAliveGroupMembersInVehicle', '"BOARDING_ROLE_RESET"', 'remaining <= 0', 'IssueBoardingRoleRetry\(', 'return;') 'First wrong-seat detection must clear actions, force out once, report occupants and retry only after the vehicle is empty'
Assert-TextSequence $processBoardingRoleResetBody @('runtime\.IsBoardingRoleRetryIssued\(\)', 'RejectBoardingRoleViolation\(runtime, faction, slot, "ROLE_RETRY_OCCUPIED_NON_DRIVER"\)', 'return;') 'A repeated wrong-seat result after retry must become a role violation'
Assert-TextSequence $processBoardingRoleResetBody @('remainingMounted <= 0', 'IssueBoardingRoleRetry\(', 'return;', 'GetBoardingRoleResetAgeMs\(\) >= GetBoardingRoleResetTimeoutMs\(\)', 'RejectBoardingRoleViolation') 'Role reset must issue one retry when empty or reject it at the bounded deadline'
Assert-TextSequence $issueBoardingRoleRetryBody @('ResetGroupVehicleActions', 'StartBoardingPhase\(runtime, slot, AICF_EVehicleBoardingPhase\.DRIVER\)', 'RejectBoardingRoleViolation', 'MarkBoardingRoleRetryIssued', '"BOARDING_ROLE_RETRY"') 'The single retry must re-normalize actions, target pilot only, and convert waypoint failure into a role violation'
Assert-TextSequence $rejectBoardingRoleViolationBody @('LatchAcceptanceFailure\(runtime, "BOARDING_ROLE_VIOLATION"\)', '"BOARDING_ROLE_VIOLATION"', 'DescribeGroupVehicleOccupants', 'BeginFallback\(runtime, faction, slot, "BOARDING_ROLE_VIOLATION"\)') 'A role violation must latch acceptance failure, expose occupants, and fall back'
Assert-TextSequence $getBoardingRoleResetTimeoutBody @('GetBoardingTimeoutMs\(\) / 2', 'timeoutMs > 10000', 'timeoutMs = 10000', 'timeoutMs < 1000', 'timeoutMs = 1000') 'Role normalization must have a bounded 1-10 second deadline'
Assert-TextSequence $noCargoPhaseBody @('SetBoardingPhase\(phase\)', 'RestartPhaseDeadline\(\)', 'BOARDING_PHASE_STARTED', 'PASSENGERS_ASSIGNED', 'return true') 'No-cargo passenger phase must still receive and report one bounded phase deadline'
Assert-TextSequence $processMovingBody @('ResolveAliveDriver\(runtime\)', 'IsAliveGroupMember\(slot\.GetGroup\(\), driver\)', 'if \(!driver\)', 'BeginDriverRecovery\(runtime, faction, slot\)') 'Moving vehicle driver must belong to the managed group before recovery decisions'

$createRecoveryBody = Get-CMethodBody $coordinator '^\s*protected\s+SCR_AIGetInVehicle\s+CreateCrewRecoveryAction\s*\(' 'CreateCrewRecoveryAction'
$beginDriverRecoveryBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+BeginDriverRecovery\s*\(' 'BeginDriverRecovery'
$beginGunnerRecoveryBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+BeginGunnerRecovery\s*\(' 'BeginGunnerRecovery'
$cancelRecoveryBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+CancelCrewRecovery\s*\(' 'CancelCrewRecovery'
$processCrewRecoveryBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessDriverRecovery\s*\(' 'ProcessDriverRecovery'
$trackRecoveryBody = Get-CMethodBody $runtime '^\s*void\s+TrackCrewRecovery\s*\(' 'TrackCrewRecovery'
$clearRecoveryBody = Get-CMethodBody $runtime '^\s*void\s+ClearCrewRecoveryTracking\s*\(' 'ClearCrewRecoveryTracking'
Assert-TextContains $beginDriverRecoveryBody 'CreateCrewRecoveryAction\([\s\S]*EAICompartmentType\.Pilot' 'Driver recovery must synchronously target the pilot role'
Assert-TextNotContains $beginDriverRecoveryBody 'EAICompartmentType\.Turret|SendGetInMessage' 'Driver recovery must not target turret or use async messaging'
Assert-TextContains $beginGunnerRecoveryBody 'CreateCrewRecoveryAction\([\s\S]*EAICompartmentType\.Turret' 'Gunner recovery must synchronously target the turret role'
Assert-TextNotContains $beginGunnerRecoveryBody 'EAICompartmentType\.Pilot|SendGetInMessage' 'Gunner recovery must not target pilot or use async messaging'
Assert-TextSequence $beginDriverRecoveryBody @('CreateCrewRecoveryAction\(', 'if \(!recoveryAgent \|\| !recoveryAction\)', 'TrackCrewRecovery\(recoveryAgent, recoveryAction\)', 'SetCrewRecoveryPhase\(AICF_EVehicleCrewRecoveryPhase\.DRIVER\)', 'SetState\(AICF_EVehicleState\.RECOVERING\)', 'RestartPhaseDeadline\(\)') 'Driver recovery must retain its exact action before starting a fresh driver deadline'
Assert-TextSequence $beginGunnerRecoveryBody @('CreateCrewRecoveryAction\(', 'if \(!recoveryAgent \|\| !recoveryAction\)', 'TrackCrewRecovery\(recoveryAgent, recoveryAction\)', 'SetCrewRecoveryPhase\(AICF_EVehicleCrewRecoveryPhase\.GUNNER\)', 'SetState\(AICF_EVehicleState\.RECOVERING\)', 'RestartPhaseDeadline\(\)') 'Gunner recovery must retain its exact action before starting a fresh gunner deadline'
Assert-FileContains $runtime 'protected ref SCR_AIGetInVehicle m_CrewRecoveryAction' 'Runtime must strongly retain the exact crew recovery action token'
Assert-TextSequence $trackRecoveryBody @('m_CrewRecoveryAgent = agent', 'm_CrewRecoveryAction = action') 'TrackCrewRecovery must retain the selected agent and exact action together'
Assert-TextSequence $clearRecoveryBody @('m_CrewRecoveryAgent = null', 'm_CrewRecoveryAction = null') 'Crew recovery success/abort must release both tracking references'
Assert-OccurrenceCount $processCrewRecoveryBody 'ClearCrewRecoveryTracking\(' 2 'Successful driver/gunner recovery must clear tracking in both success branches'
Assert-TextNotContains $processCrewRecoveryBody 'CancelCrewRecovery\(' 'Successful recovery must not Fail and eject the newly seated crew member'
Assert-TextSequence $processCrewRecoveryBody @('GetCrewRecoveryPhase\(\)', 'ResolveAliveDriver\(runtime\)', 'IsAliveGroupMember\(slot\.GetGroup\(\), driver\)', 'IsMemberSettledInVehicle\(driver, runtime\.GetVehicle\(\)\)', 'if \(!driverSettled && recoveryPhase == AICF_EVehicleCrewRecoveryPhase\.GUNNER\)', 'BeginDriverRecovery\(runtime, faction, slot\)') 'Recovered driver must be settled, belong to the current managed group, and be restored before gunner recovery'
Assert-TextContains $processCrewRecoveryBody 'ALL_REQUIRED_CREW_RESTORED' 'Crew recovery may succeed only after the complete mandatory crew set is restored'
Assert-TextSequence $processCrewRecoveryBody @('ResolveAliveGunner\(runtime\)', 'IsAliveGroupMember\(slot\.GetGroup\(\), gunner\)') 'Recovered gunner must belong to the current managed group'
Assert-TextSequence $createRecoveryBody @('role == EAICompartmentType\.Pilot', 'GetPilotCompartmentSlot\(\)', 'role == EAICompartmentType\.Turret', 'GetTurretCompartmentSlot\(\)') 'Crew recovery roles must map to their exact vehicle compartment slots'
Assert-TextSequence $createRecoveryBody @('roleSlot\.SetReserved\(recoveryEntity\)', 'new SCR_AIGetInVehicle\(', 'utility\.AddAction\(action\)', 'return action') 'Crew recovery must reserve the exact role slot and retain the exact stock action token'
Assert-TextContains $createRecoveryBody 'new SCR_AIGetInVehicle\(\s*utility,\s*null,\s*runtime\.GetVehicle\(\),\s*roleSlot,\s*role,\s*SCR_AIActionBase\.PRIORITY_BEHAVIOR_GET_IN_VEHICLE,\s*SCR_AIActionBase\.PRIORITY_LEVEL_NORMAL\)' 'Direct crew recovery must use the exact stock action constructor mapping'
Assert-TextSequence $cancelRecoveryBody @('runtime\.GetCrewRecoveryAction\(\)', 'GetActionState\(\)', 'state != EAIActionState\.COMPLETED && state != EAIActionState\.FAILED', 'action\.Fail\(\)', 'runtime\.ClearCrewRecoveryTracking\(\)') 'Abort must fail only a live exact tracked crew recovery action'

$beginFallbackBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+BeginFallback\s*\(' 'BeginFallback'
$processFallbackBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessFallback\s*\(' 'ProcessFallback'
$forceOutBody = Get-CMethodBody $coordinator '^\s*protected\s+int\s+ForceAliveGroupMembersOut\s*\(' 'ForceAliveGroupMembersOut'
$processTerminalBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessTerminal\s*\(' 'ProcessTerminal'
$requestVehicleDeleteBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+RequestVehicleDelete\s*\(' 'RequestVehicleDelete'
$processVehicleDeleteBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessVehicleDeleteConfirmation\s*\(' 'ProcessVehicleDeleteConfirmation'
$releaseWorldPoolBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ReleaseFunctionalVehicleToWorldPool\s*\(' 'ReleaseFunctionalVehicleToWorldPool'
$processWorldPoolBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessWorldPool\s*\(' 'ProcessWorldPool'
$canDeleteSafelyBody = Get-CMethodBody $coordinator '^\s*protected\s+bool\s+CanDeleteVehicleSafely\s*\(' 'CanDeleteVehicleSafely'
$pendingDeleteIdentityBody = Get-CMethodBody $coordinator '^\s*protected\s+bool\s+PendingDeleteIdentityMatches\s*\(' 'PendingDeleteIdentityMatches'
$recoverableApproachBody = Get-CMethodBody $coordinator '^\s*protected\s+bool\s+IsRecoverableApproachFailure\s*\(' 'IsRecoverableApproachFailure'
$stopRuntimesBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+StopRuntimes\s*\(' 'StopRuntimes'
$scheduleStoppedCleanupBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ScheduleStoppedCleanupPoll\s*\(' 'ScheduleStoppedCleanupPoll'
$processStoppedCleanupBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessStoppedCleanup\s*\(' 'ProcessStoppedCleanup'
$requestWorldPoolRetirementBody = Get-CMethodBody $runtime '^\s*void\s+RequestWorldPoolRetirement\s*\(' 'RequestWorldPoolRetirement'
$clearWorldPoolRetirementBody = Get-CMethodBody $runtime '^\s*void\s+ClearWorldPoolRetirementRequest\s*\(' 'ClearWorldPoolRetirementRequest'
$inspectCleanupUseBody = Get-CMethodBody $watchdog '^\s*bool\s+InspectProtectedCleanupUse\s*\(' 'InspectProtectedCleanupUse'
$processRequestedBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessRequested\s*\(' 'ProcessRequested'
$processWaitingBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessWaitingForSite\s*\(' 'ProcessWaitingForSite'
$syncRequestContextBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+SynchronizePendingRequestContext\s*\(' 'SynchronizePendingRequestContext'
$countReservedBody = Get-CMethodBody $coordinator '^\s*protected\s+int\s+CountReserved\s*\(' 'CountReserved'
$finalizeVehicleCleanupBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+FinalizeVehicleCleanup\s*\(' 'FinalizeVehicleCleanup'
$beginDismountBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+BeginDismount\s*\(' 'BeginDismount'
$processDismountBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+ProcessDismount\s*\(' 'ProcessDismount'
$issueDismountWaypointBody = Get-CMethodBody $coordinator '^\s*protected\s+bool\s+IssueDismountWaypoint\s*\(' 'IssueDismountWaypoint'
$latchAcceptanceFailureBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+LatchAcceptanceFailure\s*\(' 'LatchAcceptanceFailure'
$resultCandidateBody = Get-CMethodBody $coordinator '^\s*protected\s+void\s+TryEmitResultCandidate\s*\(' 'TryEmitResultCandidate'
$stopBody = Get-CMethodBody $coordinator '^\s*void\s+Stop\s*\(' 'Stop'
$resetDismountReissueBody = Get-CMethodBody $runtime '^\s*void\s+ResetDismountReissue\s*\(' 'ResetDismountReissue'
$markDismountReissueBody = Get-CMethodBody $runtime '^\s*bool\s+MarkDismountReissueAttempted\s*\(' 'MarkDismountReissueAttempted'
$observeDismountClearanceBody = Get-CMethodBody $runtime '^\s*int\s+ObserveDismountClearance\s*\(' 'ObserveDismountClearance'
$inspectDismountClearanceBody = Get-CMethodBody $watchdog '^\s*bool\s+InspectProtectedMemberDismountClearance\s*\(' 'InspectProtectedMemberDismountClearance'
$relocateDismountedBody = Get-CMethodBody $coordinator '^\s*protected\s+int\s+RelocateTrappedDismountedMembers\s*\(' 'RelocateTrappedDismountedMembers'
$boardingGraceGuardBody = Get-CGuardBody $processBoardingBody 'if \(softDeadlineExpired\)\s*(?=\{\s*bool graceWasGranted)' 'ProcessBoarding one-shot grace branch'
$boardingTimeoutGuardBody = Get-CGuardBody $processBoardingBody 'if \(softDeadlineExpired\)\s*(?=\{\s*int alive)' 'ProcessBoarding timeout branch'
$dismountTimeoutGuardBody = Get-CGuardBody $processDismountBody 'if \(dismountAgeMs >= dismountTimeoutMs\)' 'ProcessDismount timeout branch'
$dismountReissueGuardBody = Get-CGuardBody $processDismountBody 'if \(!runtime\.IsDismountReissueAttempted\(\) && dismountAgeMs >= dismountTimeoutMs / 2\)' 'ProcessDismount bounded reissue branch'
$staleCooldownGuardBody = Get-CGuardBody $processFactionBody 'if \(nextVehicleGeneration\.IsIndexValid\(slotId\) && nextVehicleAtMs\.IsIndexValid\(slotId\) &&\s*nextVehicleGeneration\[slotId\] != slot\.GetSpawnGeneration\(\)\)' 'ProcessFaction stale cooldown generation guard'
$waitingSiteNotReadyGuardBody = Get-CGuardBody $processWaitingBody 'if \(!siteReady\)' 'ProcessWaitingForSite failed preflight branch'
$cleanupUnsafeGuardBody = Get-CGuardBody $canDeleteSafelyBody 'if \(!safelyClear\)' 'CanDeleteVehicleSafely unsafe scan branch'
Assert-TextSequence $beginDismountBody @('ResetDismountReissue\(\)', 'ResetGroupVehicleActions', 'IssueDismountWaypoint', 'SetState\(AICF_EVehicleState\.DISEMBARKING\)', '"DISEMBARK_STARTED"', 'DescribeGroupVehicleOccupants') 'Dismount must clear stale reservations/actions before the first GetOut waypoint and report occupants'
Assert-TextSequence $processDismountBody @('InspectProtectedMemberDismountClearance', 'ObserveDismountClearance', 'clearPolls >= DISMOUNT_CLEAR_POLLS_REQUIRED', 'CompleteDismount', 'physicalOnlyBlocked', 'DISEMBARK_CLEARANCE_RECOVERY', 'dismountAgeMs >= dismountTimeoutMs', '"DISEMBARK_TIMEOUT"', 'LatchAcceptanceFailure', 'BeginFallback', '!runtime\.IsDismountReissueAttempted\(\)', 'dismountTimeoutMs / 2', 'MarkDismountReissueAttempted', 'ResetGroupVehicleActions', 'IssueDismountWaypoint', '"DISEMBARK_REISSUED"') 'Dismount must prove stable physical clearance, recover a trapped member, then enforce its hard timeout and one reissue'
Assert-TextSequence $dismountTimeoutGuardBody @('"DISEMBARK_TIMEOUT"', 'logical=', 'transitions=', 'inside_bounds=', 'clearance_recovery_attempts=', 'LatchAcceptanceFailure\(runtime, "DISEMBARK_TIMEOUT"\)', 'BeginFallback\(runtime, faction, slot, "DISEMBARK_TIMEOUT"\)', 'return;') 'Dismount timeout must be a distinct acceptance failure with physical-transition telemetry'
Assert-TextSequence $dismountReissueGuardBody @('MarkDismountReissueAttempted', 'DescribeGroupVehicleOccupants', 'ResetGroupVehicleActions', 'IssueDismountWaypoint', '"DISEMBARK_REISSUED"') 'Dismount reissue must be one-shot, normalized, and observable'
Assert-TextNotContains $processDismountBody 'SetState\(|RestartPhaseDeadline' 'Dismount retry must not reset its immutable state deadline'
Assert-TextSequence $issueDismountWaypointBody @('DeleteRuntimeWaypoint', 'CreateDismountWaypoint', 'AddWaypointAt', 'SetActiveWaypoint', 'return true') 'Both dismount attempts must use the shared exact GetOut waypoint helper'
Assert-OccurrenceCount $coordinatorText 'IssueDismountWaypoint\(' 3 'Only the helper definition, initial dismount and one bounded reissue may reference IssueDismountWaypoint'
Assert-TextSequence $resetDismountReissueBody @('m_bDismountReissueAttempted = false', 'm_bDismountClearanceRecoveryAttempted = false', 'm_iDismountClearanceBlockedAtMs = 0', 'm_iDismountClearPollCount = 0', 'm_iDismountClearanceRecoveryAttempts = 0') 'Each dismount attempt must reset reissue, clearance stability, and bounded relocation state'
Assert-TextSequence $markDismountReissueBody @('if \(m_bDismountReissueAttempted\)', 'return false', 'm_bDismountReissueAttempted = true', 'return true') 'Runtime dismount reissue mutation must be idempotent'
Assert-TextSequence $observeDismountClearanceBody @('if \(safelyClear\)', 'm_iDismountClearPollCount\+\+', 'if \(!physicalOnlyBlocked\)', 'm_iDismountClearanceBlockedAtMs = 0', 'm_iDismountClearanceBlockedAtMs = System\.GetTickCount') 'Physical-only clearance timing must not inherit time spent in a normal get-out transition'
Assert-TextSequence $inspectDismountClearanceBody @('GetVehicleIn\(character\) == vehicle', 'CoordToLocal', 'linkedToVehicle && access\.IsInCompartment\(\)', '\(linkedToVehicle \|\| insideBounds\) && access\.IsGettingIn\(\)', '\(linkedToVehicle \|\| insideBounds\) && access\.IsGettingOut\(\)', 'linkedToVehicle && character\.IsInVehicle\(\)', 'logicalOccupantCount == 0 && transitionCount == 0 && insideBoundsCount == 0') 'Dismount clearance must scope logical/transition evidence to this vehicle and combine it with oriented physical bounds'
Assert-TextSequence $relocateDismountedBody @('GetVehicleIn\(character\) == vehicle', 'IsInCompartment\(\)', 'IsGettingIn\(\)', 'IsGettingOut\(\)', 'IsInsideExpandedDismountBounds', 'FindEmptyTerrainPosition', 'character\.Teleport') 'Clearance recovery may move only a logically-out alive member from inside the vehicle bounds to verified empty terrain'
Assert-TextSequence $boardingGraceGuardBody @('graceEligible', 'EvaluateBoardingTransitionGrace', 'BOARDING_TRANSITION_GRACE', 'phaseHardExpired', 'totalHardExpired', 'return;') 'A soft deadline may enter only one observable, hard-bounded grace window'
Assert-TextSequence $boardingTimeoutGuardBody @('getting_in=', 'waypoint=', 'members=', '"BOARDING_TIMEOUT"', 'LatchAcceptanceFailure', 'BeginFallback') 'Boarding timeout must expose transition evidence and latch acceptance failure before fallback'
Assert-TextSequence $processFallbackBody @('m_Config\.GetBoardingTimeoutMs\(\) \* 2', 'LatchAcceptanceFailure\(runtime, "FALLBACK_DISEMBARK_FAILED"\)', '"FALLBACK_DISEMBARK_FAILED"', 'SetState\(AICF_EVehicleState\.ABANDONED\)') 'Hard fallback with protected occupants must latch acceptance failure before terminalization'
Assert-TextSequence $latchAcceptanceFailureBody @('m_bAcceptanceFailureLatched = true', 'm_iAcceptanceFailureCount\+\+', 'm_sFirstAcceptanceFailureReason\.IsEmpty\(\)', '"ACCEPTANCE_FAILURE_LATCHED"', 'm_bResultCandidateLogged', '"RESULT_CANDIDATE"', 'status=INVALIDATED') 'Acceptance failures must accumulate, retain their first cause, and invalidate an earlier READY candidate'
Assert-TextNotContains $latchAcceptanceFailureBody 'm_bAcceptanceFailureLatched = false|m_iAcceptanceFailureCount = 0|m_sFirstAcceptanceFailureReason = string\.Empty' 'The cumulative acceptance-failure latch must never clear itself'
Assert-TextSequence $resultCandidateBody @('m_bResultCandidateLogged \|\| m_bAcceptanceFailureLatched \|\| AICF_Stage3Diagnostics\.HasErrors\(\)', 'transportComplete', 'armedComplete', 'm_bResultCandidateLogged = true', '"RESULT_CANDIDATE"', 'status=READY', 'final=0') 'Automated invariants may emit only a non-final READY candidate when no failure is latched'
Assert-TextNotContains $resultCandidateBody '"RESULT"|status=PASS|m_bResultLogged = true' 'Mid-run completion must not claim final RESULT/PASS'
Assert-TextSequence $stopBody @('m_bAcceptanceFailureLatched', 'AICF_Stage3Diagnostics\.HasErrors\(\)', 'm_bResultCandidateLogged', '"RESULT"', 'status=FAIL') 'Shutdown must emit only a final FAIL pending external log acceptance'
Assert-TextNotContains $coordinatorText 'status=PASS' 'Stage 3 runtime must never claim PASS before external acceptance review'
Assert-TextSequence $beginFallbackBody @('CancelCrewRecovery\(runtime\)', 'DeleteRuntimeWaypoint\(runtime\)', 'SetTerminalReason\(reason\)', 'SetState\(AICF_EVehicleState\.INFANTRY_FALLBACK\)') 'Fallback entry must cancel recovery before becoming terminal'
Assert-OccurrenceCount $processFallbackBody 'ForceAliveGroupMembersOut\(' 1 'Fallback must force survivors out exactly once per poll'
Assert-OccurrenceCount $processFallbackBody 'RestoreInfantryOrder\(' 1 'Fallback may restore immediately only on the verified all-out path'
Assert-TextSequence $processFallbackBody @('System\.GetTickCount\(runtime\.GetStateStartedAtMs\(\)\)', 'ForceAliveGroupMembersOut', 'RecoverProtectedDismountClearance', 'm_Config\.GetBoardingTimeoutMs\(\) \* 2', 'DetachVehicleFromGroup', 'SuppressVehicleTripForAssignment', 'MarkInfantryFallbackRestorePending', 'SetState\(AICF_EVehicleState\.ABANDONED\)') 'Fallback hard deadline must terminalize while retaining deferred clearance and order restoration for trapped survivors'
Assert-TextContains $processFallbackBody 'primaryTerminalReason \+ "\+FALLBACK_DISEMBARK_FAILED"' 'Hard fallback failure must append its cause without erasing the primary terminal reason'
Assert-TextContains $processFallbackBody 'forced > 0 && runtime\.MarkFallbackForceExitReported\(\)' 'Forced-exit diagnostics must be emitted once per runtime generation'
Assert-TextSequence $forceOutBody @('IsAliveCharacter', 'CompartmentAccessComponent\.GetVehicleIn', 'InterruptVehicleActionQueue', 'FindSuitableTeleportLocation', 'GetOutVehicle_NoDoor') 'Forced fallback must operate only on alive same-vehicle members and use a no-door teleport escape'
Assert-TextSequence $processTerminalBody @('runtime\.IsInfantryFallbackRestorePending\(\)', 'RecoverProtectedDismountClearance', 'ForceAliveGroupMembersOut', 'if \(groupOut && groupCurrent\)', 'NormalizeAfterVehicle', 'RestoreInfantryOrder', 'ClearInfantryFallbackRestorePending', 'logicalOccupants == 0 && transitions == 0 && insideBounds > 0', 'NormalizeAfterVehicle', 'RestoreInfantryOrder\(runtime, faction, slot, "PHYSICAL_CLEARANCE_PENDING"\)', 'ClearInfantryFallbackRestorePending', 'if \(!groupOut\)', 'return;', '!RecoverProtectedDismountClearance', 'IsFunctionalAbandonedVehicle', 'CanDeleteVehicleSafely') 'Terminal polling may restore movement for a logically-out physical blocker, release a functional vehicle, and destructively clean only after global safe-clear'
Assert-TextSequence $processTerminalBody @('runtime\.IsInfantryFallbackRestorePending\(\) && !runtime\.GetGroup\(\)', 'ClearInfantryFallbackRestorePending', '"INFANTRY_FALLBACK_RESTORE_SKIPPED"', 'runtime\.IsInfantryFallbackRestorePending\(\)', 'IsFunctionalAbandonedVehicle', 'CanDeleteVehicleSafely') 'A vanished stale group must release only its impossible restore obligation while retaining global cleanup protection'
Assert-TextSequence $processTerminalBody @('IsFunctionalAbandonedVehicle\(runtime\)', 'GetWorldPool', 'ReleaseFunctionalVehicleToWorldPool', 'return;', 'cleanupDue = System\.GetTickCount\(\) >= runtime\.GetCleanupAtMs\(\)', '!cleanupDue && !expediteForReplacement', 'CanDeleteVehicleSafely', 'RequestVehicleDelete') 'Every functional abandoned vehicle must leave AI cap through the player-available pool before any destructive cleanup'
Assert-TextNotContains $processTerminalBody 'VEHICLE_WORLD_POOL_CAP_BLOCKED|SAFE_BACKPRESSURE|worldPool\.Count\(\) >= [\s\S]*return;' 'A protected/full pool must never keep a functional terminal runtime in the AI cap'
Assert-TextSequence $releaseWorldPoolBody @('CancelBoardingApproachActions', 'DetachVehicleFromGroup', 'slot\.ClearVehicleRuntime', 'runtimes\[slotId\] = null', 'nextVehicleAtMs\[slotId\] = 0', 'runtime\.SetGroup\(null\)', 'MarkReleasedToWorldPool', 'worldPool\.Insert\(runtime\)', 'VEHICLE_WORLD_POOL_RELEASED') 'World-pool release must free AI ownership and cap without deleting the functional entity'
Assert-TextNotContains $releaseWorldPoolBody 'DeleteRplEntity|RequestVehicleDelete' 'Releasing a functional vehicle to players must be non-destructive'
Assert-TextSequence $releaseWorldPoolBody @('worldPool\.Count\(\) > poolLimit', 'candidate\.RequestWorldPoolRetirement\(\)', 'VEHICLE_WORLD_POOL_SOFT_OVERFLOW', 'PLAYER_SAFE_DEFERRED_RETIREMENT') 'Pool overflow must remain soft while marking every safe candidate for deferred retirement'
Assert-TextSequence $releaseWorldPoolBody @('IsRecoverableApproachFailure', 'slot\.ClearVehicleTripSuppression', 'new AICF_VehicleRuntime', 'SetState\(AICF_EVehicleState\.WAITING_FOR_SITE\)', 'runtimes\[slotId\] = waitingRuntime', 'slot\.SetVehicleRuntime\(waitingRuntime\)', 'VEHICLE_REQUEST_WAITING') 'A recoverable APPROACH failure must become a cap-free request wait after the old functional vehicle is released'
Assert-TextSequence $recoverableApproachBody @('reason\.Contains\("FALLBACK_DISEMBARK_FAILED"\)', 'return false', 'reason\.Contains\("BOARDING_APPROACH"\)', 'reason\.Contains\("BOARDING_TIMEOUT_APPROACH"\)', 'reason\.Contains\("VEHICLE_TOO_FAR"\)', 'reason\.Contains\("GROUP_COHESION"\)') 'Approach, timeout, distance and cohesion failures remain recoverable only when fallback itself safely dismounted the group'
Assert-TextContains $processFallbackBody 'if \(ShouldSuppressVehicleTripAfterFallback\(runtime\.GetTerminalReason\(\)\)\)\s*slot\.SuppressVehicleTripForAssignment' 'Fallback suppression must exempt recoverable approach/cohesion failures'
Assert-TextSequence $inspectCleanupUseBody @('GetCompartments', 'IsProtectedCharacter', 'GetAllPlayers', 'GetPlayerControlledEntity', 'DistanceSqXZ', 'GetVehicleIn\(character\) == vehicle', 'IsGettingIn\(\)', 'IsGettingOut\(\)', 'linkedPlayerCount == 0', 'playerTransitionCount == 0', 'nearbyPlayerCount == 0') 'Cleanup protection must cover occupants, linked players, active interaction transitions and nearby players'
Assert-TextSequence $canDeleteSafelyBody @('InspectProtectedCleanupUse', 'ObserveCleanupClear\(safelyClear\)', 'VEHICLE_CLEANUP_DEFERRED', 'stableClearMs < VEHICLE_CLEANUP_STABLE_CLEAR_MS', 'InspectProtectedCleanupUse', 'ObserveCleanupClear\(false\)', 'return true') 'Destructive cleanup must require continuous stable-clear and re-scan immediately before delete'
Assert-TextSequence $processWorldPoolBody @('MISSING_ENTITY_REFERENCE', 'worldPool\.RemoveOrdered\(staleIndex\)', 'pendingDeleteCount', 'capacityDeletesRemaining', 'for \(int i = 0; i < worldPool\.Count\(\); i\+\+\)', 'FindEntityByID', 'PendingDeleteIdentityMatches', 'if \(!remaining\)', 'ClearVehicleDeleteConfirmation', 'worldPool\.RemoveOrdered\(i\)', 'CanDeleteVehicleSafely\(runtime, remainingVehicle, "WORLD_POOL_DELETE_RETRY"\)', 'CanRetryVehicleDelete', 'DeleteRplEntity') 'World-pool bookkeeping must preserve release order, evict missing refs, verify identity, and protect every delete retry'
Assert-TextNotContains $processWorldPoolBody 'worldPool\.Remove\(' 'World-pool cleanup must not use unordered removal that destroys oldest-first retirement order'
Assert-TextSequence $processWorldPoolBody @('IsDestroyed', 'IsOnFire', 'IsOverturned', '!m_Watchdog\.CanMove', 'IsWorldPoolRetirementRequested', 'capacityDeletesRemaining <= 0', 'CanDeleteVehicleSafely', 'RequestVehicleDelete', 'capacityDeletesRemaining--') 'Only unusable or budgeted overflow entries may enter protected destructive cleanup'
Assert-TextSequence $requestWorldPoolRetirementBody @('if \(!m_bWorldPoolRetirementRequested\)', 'm_iCleanupClearStartedAtMs = 0', 'm_bWorldPoolRetirementRequested = true') 'Each new retirement request must begin a fresh stable-clear window'
Assert-TextSequence $clearWorldPoolRetirementBody @('m_bWorldPoolRetirementRequested = false', 'm_iCleanupClearStartedAtMs = 0') 'Cancelled overflow retirement must discard stale clearance evidence'
Assert-TextSequence $pendingDeleteIdentityBody @('rpl\.Id\(\)\.ToString\(\)', 'expectedRplId\.IsEmpty\(\) \|\| expectedRplId == "NONE"', 'return false', 'actualRplId == expectedRplId') 'Delete retry identity must fail closed when exact replicated identity is unavailable or changed'
Assert-FileContains $config 'DEFAULT_SPAWN_MAX_ATTEMPTS\s*=\s*4\s*;' 'Spawn retries must have a finite default attempt budget'
Assert-FileContains $config 'DEFAULT_WAIT_PROBE_INTERVAL_MS\s*=\s*60000\s*;' 'An exhausted request must switch to a low-frequency wait probe'
Assert-TextSequence $processRequestedBody @('CanStartVehicleTrip\(slot\)', 'EnterWaitingForSite', 'CountReservedExcluding', 'RecordSpawnAttempt', 'SetState\(AICF_EVehicleState\.SPAWNING\)', 'spawnAttempt >= m_Config\.GetSpawnMaxAttempts\(\)', 'CalculateSpawnRetryDelayMs', 'SetState\(AICF_EVehicleState\.WAITING_FOR_SITE\)', 'GetWaitProbeIntervalMs', 'VEHICLE_REQUEST_WAITING') 'Spawn acquisition must use bounded exponential retries and then enter a non-churning wait state'
Assert-TextSequence $processWaitingBody @('GetNextAttemptAtMs', 'CanStartVehicleTrip', 'VEHICLE_SPAWN_WAIT_HEARTBEAT', 'ClearSpawnIssueReport', 'm_Spawner\.TrySpawn', 'true,', 'if \(!siteReady\)', 'SetNextAttemptAtMs', 'VEHICLE_SPAWN_WAIT_HEARTBEAT', 'ResetSpawnRequestContext', 'VEHICLE_REQUEST_RESUMED') 'WAITING_FOR_SITE must preserve infantry movement, run entity-free preflight, and resume only when an exact site becomes eligible'
Assert-TextNotContains $processWaitingBody 'RecordSpawnAttempt|SetState\(AICF_EVehicleState\.SPAWNING\)|SpawnEntityPrefabEx' 'A waiting preflight must not consume an attempt, churn state, or create an entity'
Assert-TextSequence $waitingSiteNotReadyGuardBody @('if \(!retryable\)', 'BeginFallback', 'return;', 'RecordSpawnFailure', 'SetNextAttemptAtMs', 'VEHICLE_SPAWN_WAIT_HEARTBEAT', 'return;') 'A failed wait preflight must never fall through into a false request resume'
Assert-TextSequence $processWaitingBody @('prefab\.IsEmpty\(\)', 'RecordVehicleTerminalFailure', 'LatchAcceptanceFailure', 'BeginFallback', 'return;') 'A missing configured prefab must terminate rather than wait forever'
Assert-TextSequence $syncRequestContextBody @('runtime\.GetTargetBase\(\)', 'slot\.GetTargetBase\(\)', 'GetObservedBaseRevision', 'ResetSpawnRequestContext', 'VEHICLE_REQUEST_CONTEXT_CHANGED') 'Target or base-graph revision must reset one request generation and expose fresh diagnostics'
Assert-TextContains $countReservedBody 'if \(runtime && runtime\.GetState\(\) != AICF_EVehicleState\.WAITING_FOR_SITE\)\s*count\+\+;' 'A waiting request must be the only runtime state excluded from AI vehicle cap'
Assert-FileContains $match 'NotifyStrategicContextChanged\("BASE_GRAPH_REBUILT"\)' 'A successful live graph rebuild must wake pending vehicle requests'
Assert-TextSequence $requestVehicleDeleteBody @('vehicle\.GetID\(\)', 'rpl\.Id\(\)', 'vehicle\.GetOrigin\(\)', 'BeginVehicleDeleteConfirmation', '"VEHICLE_DELETE_REQUESTED"', 'RplComponent\.DeleteRplEntity\(vehicle, false\)', 'ClearVehicleReferenceAfterDeleteRequest') 'Delete request must preserve exact entity/Rpl identity before authority deletion and retain cap until confirmation'
Assert-OccurrenceCount $requestVehicleDeleteBody 'RplComponent\.DeleteRplEntity\(' 1 'The initial replicated delete must be issued exactly once per request'
Assert-TextSequence $processVehicleDeleteBody @('FindEntityByID\(runtime\.GetVehicleDeleteEntityId\(\)\)', 'PendingDeleteIdentityMatches', 'DO_NOT_DELETE_REPLACEMENT', 'CanRetryVehicleDelete', 'RecordVehicleDeleteRetry', '"VEHICLE_DELETE_RETRIED"', 'RplComponent\.DeleteRplEntity\(remaining, false\)', 'GetVehicleDeleteAgeMs', 'VEHICLE_DELETE_CONFIRM_TIMEOUT_MS', 'LatchAcceptanceFailure', '"VEHICLE_DELETE_NOT_CONFIRMED"', 'FinalizeVehicleCleanup') 'Authority confirmation must verify exact Rpl identity, retry bounded deletion, and never delete an EntityID replacement'
Assert-OccurrenceCount $processVehicleDeleteBody 'RplComponent\.DeleteRplEntity\(' 1 'Delete confirmation may have exactly one bounded retry site'
Assert-TextSequence $stopBody @('StopRuntimes\(m_aUSRuntime', 'StopRuntimes\(m_aUSSRRuntime', 'StopRuntimes\(m_aUSWorldPool', 'StopRuntimes\(m_aUSSRWorldPool', 'VEHICLE_STOP_CLEANUP_STARTED', 'ScheduleStoppedCleanupPoll') 'Coordinator stop must hand all active and pooled vehicles to a deferred safety poll'
Assert-TextNotContains $stopRuntimesBody 'DeleteRplEntity|RequestVehicleDelete|CanDeleteVehicleSafely' 'StopRuntimes must only detach and enqueue; it may not perform a one-shot destructive cleanup'
Assert-TextSequence $stopRuntimesBody @('CancelBoardingApproachActions', 'DetachVehicleFromGroup', 'ObserveCleanupClear\(false\)', 'm_aStoppedCleanupRuntimes\.Find\(runtime\) < 0', 'm_aStoppedCleanupRuntimes\.Insert\(runtime\)') 'Stop enqueue must cancel exact actions, reset clearance evidence, and deduplicate runtimes'
Assert-TextSequence $scheduleStoppedCleanupBody @('m_bStoppedCleanupScheduled', 'CallLater', 'ProcessStoppedCleanup', 'VEHICLE_STOP_CLEANUP_POLL_MS', 'false') 'Stopped cleanup must use bounded one-shot polls whose bound delegate retains the coordinator'
Assert-TextSequence $processStoppedCleanupBody @('FindEntityByID', 'PendingDeleteIdentityMatches', 'DO_NOT_DELETE_REPLACEMENT', 'CanDeleteVehicleSafely', 'CanRetryVehicleDelete', 'DeleteRplEntity', 'GetVehicleDeleteAgeMs', 'VEHICLE_DELETE_CONFIRM_TIMEOUT_MS', 'CanDeleteVehicleSafely\(runtime, vehicle, "COORDINATOR_STOP"\)', 'RequestVehicleDelete', 'VEHICLE_STOP_CLEANUP_ACQUIRE_TIMEOUT_MS', 'STABLE_CLEAR_NOT_ACQUIRED', 'ScheduleStoppedCleanupPoll') 'Deferred stop cleanup must verify identity, poll stable-clear, retry bounded deletion and fail closed on timeout'
Assert-TextSequence $cleanupUnsafeGuardBody @('VEHICLE_CLEANUP_DEFERRED', 'return false') 'Any protected occupant, player interaction or nearby player must reset and block destructive cleanup'
Assert-TextSequence $finalizeVehicleCleanupBody @('entity_id=', 'rpl_id=', 'delete_attempts=', 'slot\.ClearVehicleRuntime', 'runtimes\[slotId\] = null', 'if \(replacementCapacityRequired && nextVehicleAtMs\.IsIndexValid\(slotId\)\)', 'nextVehicleAtMs\[slotId\] = 0\s*;', 'nextVehicleGeneration\[slotId\] = -1\s*;', '"VEHICLE_CLEANUP_CONFIRMED"', 'ClearVehicleDeleteConfirmation') 'Only confirmed authority deletion may release cap and stale-generation cooldown'
Assert-TextSequence $finalizeVehicleCleanupBody @('nextVehicleAtMs\[slotId\] = 0\s*;', 'nextVehicleGeneration\[slotId\] = -1\s*;') 'Confirmed early replacement cleanup must clear both cooldown timestamp and generation owner'
Assert-TextSequence $staleCooldownGuardBody @('nextVehicleGeneration\[slotId\] = -1\s*;', 'nextVehicleAtMs\[slotId\] = 0\s*;') 'A new group generation must discard both values of the previous cooldown inside the exact mismatch guard'
Assert-TextSequence $processFactionBody @('nextVehicleGeneration\[slotId\] != slot\.GetSpawnGeneration\(\)', 'nextVehicleAtMs\[slotId\] = 0\s*;', 'System\.GetTickCount\(\) < nextVehicleAtMs\[slotId\]') 'Stale cooldown reset must happen before trip eligibility'
Assert-TextSequence $processFallbackBody @('nextVehicleAtMs\[slot\.GetSlotId\(\)\] = System\.GetTickCount\(\)', 'nextVehicleGeneration\[slot\.GetSlotId\(\)\] = runtime\.GetGroupGeneration\(\)\s*;') 'Fallback cooldown must record the exact group generation that owns it'
Assert-FileContains $coordinator 'case AICF_EVehicleState\.ABANDONED:[\s\S]*case AICF_EVehicleState\.DESTROYED:[\s\S]*return runtime\.IsInfantryFallbackRestorePending\(\)' 'Reliability/commander logic must not restore an order while terminal fallback survivors remain mounted'
Assert-FileContains $watchdog 'IsProtectedCharacter[\s\S]*GetLifeState\(\) != ECharacterLifeState\.DEAD' 'Cleanup must protect incapacitated as well as alive occupants'
Assert-FileContains $coordinator 'ProcessDismount[\s\S]*InspectProtectedMemberDismountClearance[\s\S]*DISMOUNT_CLEAR_POLLS_REQUIRED' 'Normal dismount must require stable safe clearance for every protected managed member'
Assert-FileContains $runtime 'm_iCleanupAtMs <= 0 \|\| cleanupAtMs < m_iCleanupAtMs' 'Cleanup deadlines must never be extended by repeated polls'
Assert-FileContains $slot 'HasVehicleTerminalFailure\(\)' 'Terminal vehicle configuration failures must block retries for the current group generation'
Assert-FileContains $coordinator 'slot\.RecordVehicleTerminalFailure\(failureReason\)' 'Coordinator must latch terminal spawn failures on the stable group slot'
Assert-FileContains $coordinator 'runtime\.DescribeContext\(failureReason\)' 'Coordinator must report the actual spawner failure reason'
Assert-FileNotContains $coordinator 'BeginFallback\(runtime, faction, slot, "VEHICLE_IMMOBILIZED"\)' 'Movement-damage metadata must not bypass the progress/stuck recovery contract'
Assert-FileContains $coordinator 'runtime\.IsStationary\(m_Config\.GetStuckTimeoutMs\(\)\)[\s\S]*runtime\.IsRouteStalled\(m_Config\.GetObjectiveProgressTimeoutMs\(\)\)[\s\S]*m_Watchdog\.CanMove\(runtime\)[\s\S]*VEHICLE_STUCK_DETECTED[\s\S]*VEHICLE_RECOVERY_STARTED' 'Stationary and objective-stall failures must use independent bounded deadlines'
Assert-FileContains $coordinator 'if \(stationary && !movementUsable\)[\s\S]*VEHICLE_RECOVERY_MOBILITY_UNAVAILABLE' 'Confirmed movement damage without physical motion must end in a causal recovery failure'
Assert-FileContains $coordinator 'TryResolveTargetPosition\(runtime\.GetTargetBase\(\), slot\.GetRole\(\), targetPosition\)[\s\S]*targetDistanceMeters <= m_Config\.GetDismountDistanceMeters\(\)' 'Vehicle dismount must be measured against the same tactical capture point as the infantry order'
Assert-FileContains $coordinator 'assignedTarget != runtime\.GetTargetBase\(\)[\s\S]*runtime\.SetTargetBase\(assignedTarget\)[\s\S]*StartMovement\(runtime, faction, slot, "STRATEGIC_TARGET_CHANGED"\)' 'A live commander retarget must reroute the existing vehicle instead of abandoning or replacing it'
Assert-FileContains $coordinator 'ReplanControlledMovement[\s\S]*AssignOrder[\s\S]*SuspendOrderForVehicle' 'A graph rebuild must update the strategic target without leaving an infantry waypoint beside the vehicle waypoint'
Assert-FileContains $match 'IsControllingMovement\(slot\)[\s\S]*ReplanControlledMovement\([\s\S]*BASE_OWNER_CHANGED' 'Base-change replanning must include slots currently controlled by Stage 3 vehicles'
Assert-FileContains $match 'm_VehicleCoordinator && !m_bReplanScheduled[\s\S]*m_VehicleCoordinator\.Update' 'Vehicle polling must wait for the scheduled graph rebuild before judging a captured target'
Assert-FileNotContains $match 'm_VehicleCoordinator && !m_bGraphRebuildNeeded && !m_bReplanScheduled' 'A failed graph rebuild must not freeze vehicle safety and cleanup polling'
Assert-FileContains $coordinator 'runtime\.ObserveProgress\(routeDistanceMeters' 'Vehicle progress must be measured against the active reachable route endpoint'
Assert-FileContains $runtime 'ObserveMotion\(vector position, float minimumMovementMeters\)' 'Physical movement must prevent false stationary detection on winding routes'
Assert-FileContains $runtime 'm_bRecoveryRequiresRouteProgress' 'Recovery success criteria must preserve whether the failure was stationary or objective-level'
Assert-FileContains $coordinator 'if \(routeProgress\)[\s\S]*if \(runtime\.HasPendingRouteRecovery\(\)\)[\s\S]*else if \(physicalMotion\)[\s\S]*HasPendingRouteRecovery\(\) && !runtime\.RecoveryRequiresRouteProgress\(\)' 'Route progress must confirm any recovery while physical motion confirms stationary recovery only'
Assert-FileContains $config 'DEFAULT_MOTION_METERS\s*=\s*3\.0' 'Physical motion must use a threshold independent of objective progress'
Assert-FileContains $config 'DEFAULT_OBJECTIVE_PROGRESS_TIMEOUT_MS\s*=\s*300000' 'Objective divergence must use a longer deadline than stationary detection'
Assert-FileContains $config 'NormalizeVehicleCounts\(\)' 'Configured transport and armed slots must fit the available ATTACK slots'
Assert-FileContains $coordinator 'AreConfiguredTripsComplete\(m_aUSTransportCompletedSlots, 0, transportCount\)[\s\S]*AreConfiguredTripsComplete\(m_aUSSRTransportCompletedSlots, 0, transportCount\)' 'Transport RESULT must require every configured slot for both factions'
Assert-FileContains $coordinator 'AreConfiguredTripsComplete\(m_aUSArmedCompletedSlots, transportCount, armedCount\)[\s\S]*AreConfiguredTripsComplete\(m_aUSSRArmedCompletedSlots, transportCount, armedCount\)' 'Armed RESULT must require every configured slot for both factions'
Assert-FileContains $coordinator 'VEHICLE_MOTION' 'Physical movement without immediate route reduction must remain observable'
Assert-FileContains $coordinator 'MarkMotionReportDue\(MOTION_REPORT_INTERVAL_MS\)' 'Routine physical-motion telemetry must be rate-limited for long runs'
Assert-FileContains $waypoints 'GetReachableWaypointInRoad\(fromPosition, targetPosition, targetRangeMeters, roadPosition\)' 'Vehicle routes must prefer a road-reachable endpoint near the objective'
Assert-FileContains $coordinator 'route_mode=%3 endpoint_offset_m=%4' 'Route diagnostics must distinguish road projection from direct fallback'
Assert-FileContains $coordinator 'runtime\.GetState\(\) != AICF_EVehicleState\.ABANDONED[\s\S]*BeginDetachedCleanup' 'Detached cleanup must be entered only once per vehicle generation'
Assert-FileContains $coordinator 'runtime && \(runtime\.GetState\(\) == AICF_EVehicleState\.ABANDONED[\s\S]*ProcessTerminal\(runtime, faction, slot, runtimes, nextVehicleAtMs, nextVehicleGeneration, slotId\);[\s\S]*if \(runtime && !IsRuntimeCurrent' 'Terminal runtimes must be polled without re-entering stale cleanup'
Assert-FileContains $coordinator 'if \(!runtime \|\| runtime\.GetState\(\) == AICF_EVehicleState\.ABANDONED \|\|[\s\S]*AICF_EVehicleState\.DESTROYED\)' 'Detached cleanup must be idempotent at its mutation boundary'
Assert-FileContains $coordinator 'runtimes\.IsIndexValid\(slotId\) && runtimes\[slotId\] == runtime' 'Stale cleanup must not clear a newer runtime generation'
Assert-FileContains $watchdog 'GetMovementDamage\(\)' 'Stuck diagnostics must preserve the stock movement-damage signal without treating it as physical motion'
Assert-FileContains $coordinator 'RemoveUsableVehicle' 'Fallback/cleanup must detach the vehicle from group utility'
Assert-FileContains $waypoints 'group\.RemoveWaypoint\(waypoint\)[\s\S]*RplComponent\.DeleteRplEntity\(waypoint' 'Stage 3 waypoints must be removed and deleted'
Assert-TextSequence $requestVehicleDeleteBody @('CanDeleteVehicleSafely|VEHICLE_DELETE_REQUESTED', 'RplComponent\.DeleteRplEntity\(vehicle, false\)') 'Terminal vehicle cleanup must route replicated deletion through the identity-tracked request helper'
Assert-FileContains $coordinator 'RestoreInfantryOrder' 'Every vehicle terminal path must preserve an infantry fallback'
Assert-FileContains $slot 'SuppressVehicleTripForAssignment[\s\S]*IsVehicleTripSuppressedForCurrentAssignment' 'Infantry fallback must suppress another vehicle for the same group/objective assignment'
Assert-FileContains $slot 'm_VehicleFallbackTargetBase && m_VehicleFallbackTargetBase != targetBase[\s\S]*ClearVehicleTripSuppression' 'A genuinely new strategic target must release the previous fallback suppression'
Assert-FileContains $coordinator 'fallbackTarget = slot\.GetTargetBase\(\)[\s\S]*slot\.SuppressVehicleTripForAssignment\(runtime\.GetGroupGeneration\(\), fallbackTarget\)[\s\S]*RestoreInfantryOrder' 'Fallback suppression must use the current slot target and be latched before order restore'
Assert-FileContains $coordinator 'slot\.IsVehicleTripSuppressedForCurrentAssignment\(\)[\s\S]*!slot\.IsCombatReady\(\)' 'A suppressed infantry assignment must not immediately request a replacement vehicle'
Assert-FileContains $match 'm_VehicleCoordinator\.Stop\(cleanupEntities\)' 'Match shutdown must stop the vehicle coordinator'
Assert-FileContains $match 'm_VehicleCoordinator\.IsControllingMovement\(slot\)' 'Infantry reliability must not overwrite vehicle waypoints'
Assert-FileContains $match 'ORDER_RECOVERY_STABLE_POLLS\s*=\s*2\s*;' 'Order recovery must require two consecutive reliability confirmations'
$recoverOrderBody = Get-CMethodBody $orderPlanner '^\s*bool\s+RecoverOrder\s*\(' 'RecoverOrder'
$getOrderFailureBody = Get-CMethodBody $orderPlanner '^\s*string\s+GetOrderFailureReason\s*\(' 'GetOrderFailureReason'
$pendingOrderRecoveryBody = Get-CMethodBody $match '^\s*protected\s+void\s+ProcessPendingOrderRecovery\s*\(' 'ProcessPendingOrderRecovery'
$revalidateFactionOrdersBody = Get-CMethodBody $match '^\s*protected\s+bool\s+RevalidateFactionOrders\s*\(' 'RevalidateFactionOrders'
$reliabilityBody = Get-CMethodBody $match '^\s*protected\s+void\s+ProcessFactionReliability\s*\(' 'ProcessFactionReliability'
$beginOrderRecoveryVerificationBody = Get-CMethodBody $slot '^\s*void\s+BeginOrderRecoveryVerification\s*\(' 'BeginOrderRecoveryVerification'
$clearPendingOrderRecoveryBody = Get-CMethodBody $slot '^\s*void\s+ClearPendingOrderRecovery\s*\(' 'ClearPendingOrderRecovery'
$recordStuckRecoveryBody = Get-CMethodBody $slot '^\s*void\s+RecordStuckRecovery\s*\(' 'RecordStuckRecovery'
Assert-TextSequence $recoverOrderBody @('ReplaceOrder', 'BeginOrderRecoveryVerification', '"ORDER_RECOVERY_ISSUED"') 'RecoverOrder must issue a verification candidate instead of declaring immediate success'
Assert-TextNotContains $recoverOrderBody '"ORDER_RECOVERED"' 'RecoverOrder must never claim success before reliability confirmation'
Assert-TextSequence $getOrderFailureBody @('IsTargetValidForRole', 'return "TARGET_INVALID"', '!slot\.GetWaypoint', 'return "WAYPOINT_REFERENCE_MISSING"', 'GetCurrentWaypoint\(\) != slot\.GetWaypoint\(\)', 'return "WAYPOINT_NOT_CURRENT"') 'Strategic retargeting must outrank transient waypoint loss so it never consumes stuck recovery budget'
Assert-TextSequence $beginOrderRecoveryVerificationBody @('ClearPendingOrderRecovery', 'm_PendingOrderRecoveryGroup = m_Group', 'm_PendingOrderRecoveryTargetBase = m_TargetBase', 'm_PendingOrderRecoveryWaypoint = m_Waypoint', 'm_iOrderRecoveryStartedAtMs = System\.GetTickCount') 'Pending recovery must snapshot exact group, target, waypoint, and issue time'
Assert-TextSequence $clearPendingOrderRecoveryBody @('m_PendingOrderRecoveryGroup = null', 'm_PendingOrderRecoveryTargetBase = null', 'm_PendingOrderRecoveryWaypoint = null', 'm_iOrderRecoveryStablePolls = 0') 'Pending recovery reset must clear identity and stability state together'
Assert-TextContains $revalidateFactionOrdersBody 'if \(slot\.HasPendingOrderRecovery\(\)\)\s*continue;' 'Commander polling must not replace or confirm a pending recovery candidate'
Assert-TextSequence $reliabilityBody @('slot\.HasPendingOrderRecovery\(\)', 'ProcessPendingOrderRecovery\(factionState, slot, faction\)', 'continue;') 'Reliability polling must exclusively own pending recovery verification'
Assert-TextSequence $pendingOrderRecoveryBody @('IsPendingOrderRecoveryContextCurrent', 'GetPendingOrderRecoveryWaypoint', 'group\.GetCurrentWaypoint\(\)', 'currentWaypoint == expectedWaypoint', 'RecordPendingOrderRecoveryStablePoll', 'stablePolls < ORDER_RECOVERY_STABLE_POLLS', 'ClearPendingOrderRecovery', 'm_iOrderRecoveries\+\+', '"ORDER_RECOVERED"') 'ORDER_RECOVERED must require exact identity across two reliability polls'
Assert-TextSequence $pendingOrderRecoveryBody @('group\.GetWaypoints\(waypointQueue\)', 'waypointQueue\.Contains\(expectedWaypoint\)', '"ORDER_RECOVERY_UNSTABLE"', 'slot\.ClearPendingOrderRecovery\(\)', 'slot\.RecordStuckRecovery\(distanceMeters\)', 'GetMaxStuckRecoveries', 'RecyclePersistentStuckGroup', 'TryRecoverOrder') 'An unstable recovery must expose queue identity and consume bounded stuck recovery before retry'
Assert-TextContains $recordStuckRecoveryBody 'if \(currentDistanceMeters >= 0\)\s*m_fBestDistanceToTarget = currentDistanceMeters;' 'A missing distance sample must not poison future positive progress baselines'
Assert-FileContains $match 'm_GroupMapMarkers\s*=\s*new AICF_GroupMapMarkerSystem\(\)' 'Gameplay group markers must always be created'
Assert-FileNotContains $stage1Config 'aicfDebugMapMarkers|DebugMapMarkers' 'Gameplay group markers must not depend on the removed debug CLI flag'
Assert-FileContains $groupMarkers 'GROUP_MAP_MARKERS_READY' 'Gameplay marker readiness must use the non-debug event contract'
Assert-FileContains $groupMarkers 'SetFaction\(null\)[\s\S]*SetGlobalVisible\(true\)' 'Current marker policy must show both factions globally'
Assert-OccurrenceCount $coordinatorText 'CallLater\(' 1 'Vehicle lifecycle may schedule only the bounded one-shot stop-cleanup poll'
Assert-OccurrenceCount $coordinatorText 'GetCallqueue\(' 1 'Only stop cleanup may retain the coordinator through the call queue'
Assert-FileNotContains $coordinator '\?[^\r\n]*:' 'Enforce 1.7 does not support C-style ternary expressions'

$requiredEvents = @(
    'CONFIG', 'VEHICLE_STATE_CHANGED', 'VEHICLE_REQUESTED', 'VEHICLE_SPAWN_SITE_SELECTED',
    'VEHICLE_SPAWN_SITE_REJECTED', 'VEHICLE_SPAWN_DEFERRED', 'VEHICLE_SPAWNED', 'VEHICLE_ASSIGNED',
    'DRIVER_ASSIGNED', 'GUNNER_ASSIGNED', 'PASSENGERS_ASSIGNED', 'BOARDING_STARTED',
    'BOARDING_PHASE_STARTED', 'BOARDING_REJECTED', 'BOARDING_ROLE_RESET',
    'BOARDING_ROLE_RETRY', 'BOARDING_ROLE_VIOLATION',
    'BOARDING_APPROACH_REISSUED', 'BOARDING_APPROACH_COMPLETE',
    'BOARDING_COMPLETE', 'BOARDING_TIMEOUT', 'VEHICLE_ROUTE_ASSIGNED', 'VEHICLE_PROGRESS', 'VEHICLE_MOTION', 'DISEMBARK_STARTED',
    'DISEMBARK_REISSUED', 'DISEMBARK_TIMEOUT', 'DISEMBARK_COMPLETE', 'VEHICLE_STUCK_DETECTED', 'VEHICLE_RECOVERY_STARTED',
    'VEHICLE_RECOVERY_SUCCEEDED', 'VEHICLE_RECOVERY_FAILED', 'DRIVER_LOST',
    'DRIVER_REASSIGNED', 'FALLBACK_FORCE_DISEMBARK', 'FALLBACK_DISEMBARK_FAILED',
    'VEHICLE_ABANDONED', 'INFANTRY_FALLBACK',
    'VEHICLE_DESTROYED', 'VEHICLE_CAP_BLOCKED', 'VEHICLE_REQUEST_WAITING', 'VEHICLE_REQUEST_RESUMED',
    'VEHICLE_SPAWN_WAIT_HEARTBEAT', 'VEHICLE_WORLD_POOL_RELEASED', 'VEHICLE_WORLD_POOL_SOFT_OVERFLOW',
    'VEHICLE_WORLD_POOL_STALE_REMOVED', 'VEHICLE_CLEANUP_DEFERRED', 'VEHICLE_STOP_CLEANUP_STARTED',
    'VEHICLE_STOP_CLEANUP_CONFIRMED', 'VEHICLE_STOP_CLEANUP_RETAINED', 'VEHICLE_DELETE_REQUESTED',
    'VEHICLE_DELETE_RETRIED', 'VEHICLE_DELETE_NOT_CONFIRMED', 'VEHICLE_CLEANUP_CONFIRMED', 'VEHICLE_CLEANUP',
    'ACCEPTANCE_FAILURE_LATCHED', 'ORDER_RECOVERY_ISSUED', 'ORDER_RECOVERY_UNSTABLE',
    'ORDER_RECOVERED', 'HEARTBEAT', 'RESULT_CANDIDATE', 'RESULT'
)

$stage3Sources = Get-ChildItem (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict') -Filter '*.c' -File -Recurse |
    Get-Content -Raw
$allStage3Source = $stage3Sources -join "`n"
foreach ($eventName in $requiredEvents) {
    if ($allStage3Source -notmatch [regex]::Escape('"' + $eventName + '"')) {
        $failures.Add("Missing Stage 3 diagnostic event: $eventName")
    }
}

if ($failures.Count -gt 0) {
    Write-Host "Stage 3 static audit: FAIL ($($failures.Count) issue(s))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
}

Write-Host 'Stage 3 static audit: PASS'
Write-Host 'Checked: authority, disabled default, faction assignment, per-member approach and seat-normalized role boarding, one-shot role repair, immutable boarding/dismount deadlines, bounded request waiting, cumulative acceptance failures, non-final READY result candidate, exact/deduplicated spawn failures, terminal retry latch, road-reachable routing, route/physical progress, bounded mobility recovery, player-safe world-pool cleanup, identity-guarded deletion, deferred stop cleanup, generations, waypoint/entity cleanup, infantry fallback, always-global group markers, diagnostics contract.'
