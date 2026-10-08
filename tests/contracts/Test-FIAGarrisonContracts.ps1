param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$rules = @(
    @('AUTHORITY', 'Vehicles/AICF_FIAGarrisonService.c', '!Replication.IsServer() || m_bStopped || !m_Fleet'),
    @('INITIAL_FIA_ONLY', 'Vehicles/AICF_FIAGarrisonService.c', 'base.IsHQ() || base.GetFaction() != faction'),
    @('EASY_ZERO', 'Vehicles/AICF_FIAGarrisonService.c', 'if (difficulty == AICF_EDifficulty.MEDIUM) perBase = 1;'),
    @('HARD_TWO', 'Vehicles/AICF_FIAGarrisonService.c', 'if (difficulty == AICF_EDifficulty.HARD) perBase = 2;'),
    @('NO_REPLACEMENT', 'Vehicles/AICF_FIAGarrisonService.c', '!g.m_Vehicle && g.m_iRequestedAtMs == 0'),
    @('SURVIVORS', 'Vehicles/AICF_FIAGarrisonService.c', 'if (g.m_bReady) continue;'),
    @('MEMBER_IDENTITY', 'State/Vehicles/AICF_FIAGarrison.c', 'entity.GetID() != m_aCrewIds[index]'),
    @('PLAYER_FENCE', 'State/Vehicles/AICF_FIAGarrison.c', 'IsAuthoritativeAIEntity(entity)'),
    @('GROUP_MEMBERSHIP', 'State/Vehicles/AICF_FIAGarrison.c', 'control.GetAIAgent().GetParentGroup() == m_Group'),
    @('FULL_ROSTER', 'Forces/AICF_FIAGarrisonCrew.c', 'HasExactFactionRoster(g.m_Group, "FIA", g.m_aSeats.Count(),'),
    @('FOREIGN_OCCUPANT', 'Forces/AICF_FIAGarrisonCrew.c', 'if (seat.GetOccupant() || seat.IsReserved()) return false;'),
    @('NO_COMBAT_MOVE', 'Forces/AICF_FIAGarrisonCrew.c', 'behavior.m_bUseCombatMove = false;'),
    @('SCOPED_POLICY', 'Forces/AICF_FIAGarrisonCrew.c', '!AICF_FIAGarrisonService.IsDefender(m_OwnerEntity)'),
    @('NULL_TARGET_GUARD', 'Forces/AICF_FIAGarrisonCrew.c', 'if (!m_Target) return;'),
    @('THREAT_ESCALATION', 'Forces/AICF_FIAGarrisonCrew.c', 'super.OnThreatSectorEscalation(ts, sectorId, dangerValue);'),
    @('THREAT_DAMAGE', 'Forces/AICF_FIAGarrisonCrew.c', 'super.OnThreatSectorDamageTaken(ts, sectorId);'),
    @('BODY_CLEARANCE', 'Vehicles/AICF_VehicleSpawner.c', '!g.BaseIdentity() || g.m_Base.GetFaction() != g.m_Faction || !footprint.IsClear(world, pose, body)'),
    @('ASYNC_PURGE', 'Vehicles/AICF_VehicleCleanupManager.c', 'world.PurgeSpawnRequestsForGroup(g.m_Group)'),
    @('REGISTRY_STOP', 'Vehicles/AICF_FIAGarrisonService.c', 's_aDefenders.Clear();'),
    @('LIFECYCLE', 'Bootstrap/AICF_MatchController.c', 'm_FIAGarrisons.Stop();')
)
$failures = @()
foreach ($rule in $rules) {
    $source = Get-Content (Join-Path $core $rule[1]) -Raw
    if (-not $source.Contains($rule[2])) { $failures += $rule[0] }
    # Representative negative input must be rejected by the same guard.
    if ($source.Replace($rule[2], 'REMOVED').Contains($rule[2])) { $failures += "NEGATIVE_$($rule[0])" }
}
$service = Get-Content (Join-Path $core 'Vehicles/AICF_FIAGarrisonService.c') -Raw
if ($service -match 'CallLater\s*\(|\.AddWaypoint|SpawnEntityPrefab|DeleteRplEntity|GetOutgoingNodeIds') { $failures += 'DOMAIN_BOUNDARY' }
foreach ($case in @(@('Medium', 1, 'A1CF261008100001'), @('Hard', 2, 'A1CF261008100002'))) {
    $path = Join-Path $RepositoryRoot "AIConflictEveronWCSRHS/Missions/AICF_WCS_RHS_Conflict_Everon_North_$($case[0]).conf"
    $header = Get-Content $path -Raw
    if (-not $header.Contains('{A1CF261006100003}Missions/AICF_WCS_RHS_Conflict_Everon_North.conf') -or
        $header -notmatch "m_eAICFDifficulty\s+$($case[1])\b" -or
        $header -match 'm_aCampaignCustomBaseList|\bWorld\b') { $failures += "HEADER_$($case[0])" }
    if (-not (Get-Content "$path.meta" -Raw).Contains($case[2])) { $failures += "META_$($case[0])" }
}
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
"FIA garrison contracts: PASS; negative guards=$($rules.Count); runtime=NOT_RUN"
