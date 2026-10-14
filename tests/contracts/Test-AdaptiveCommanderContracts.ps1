param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$source = @('Command/AICF_AdaptiveScore.c','Command/AICF_AdaptiveCommanderPolicy.c',
    'Command/AICF_AICommander.c','Orders/AICF_OrderPlanner.c','Config/AICF_AdaptiveCommanderConfig.c') |
    ForEach-Object { Get-Content -LiteralPath (Join-Path $root $_) -Raw -Encoding UTF8 }
$text = $source -join "`n"
function Test-AdaptiveContracts([string]$InputText) {
    $code = ConvertTo-AICFCodeText $InputText
    $failures = [Collections.Generic.List[string]]::new()
    $rules = [ordered]@{
        AUTHORITY = 'if \(!Replication.IsServer\(\) \|\| !m_State'
        FACTION_SLOT = 'm_State.GetSlot\(slot.GetSlotId\(\)\) != slot'
        LIVE_POSITION = 'IEntity leader = AICF_GroupRuntime.ResolveAliveLeader\(slot.GetGroup\(\)\);'
        ALIVE_FORCE = 'AICF_GroupRuntime.CountAliveAgents\(other.GetGroup\(\)\)'
        READINESS = '!slot.IsCombatReady\(\)'
        MANUAL = 'slot.HasPlayerStrategicIntent\(\) \|\| slot.HasPlayerStrategicOrder\(\)'
        RECRUITING = 'other.IsRecruitingInfantry\(\)'
        ROUTE = 'candidate.m_bDeferred = slot.IsRouteTargetDeferred\(base\);'
        UNKNOWN = 'Math.Max\(AICF_AdaptiveCommanderConfig.UNKNOWN_RESISTANCE, candidate.m_fDemand\)'
        THREAT_TTL = 'now - candidate.m_iThreatAtMs <= AICF_AdaptiveCommanderConfig.THREAT_TTL_MS'
        STRATEGIC = 'OBJECTIVE_VALUE[\s\S]*OPEN_DIRECTION_VALUE[\s\S]*CONNECTIVITY_VALUE'
        ALLOCATION = 'candidate.m_fAllocated / demand'
        GENERATION = 'slot.GetSpawnGeneration\(\) == m_iGeneration'
        GRAPH = 'graph.GetRevision\(\) == m_iGraphRevision'
        ASSIGNMENT = 'slot.GetStrategicAssignmentRevision\(\) == m_iAssignmentRevision'
        GROUP_ID = 'm_Group.GetID\(\) != m_GroupId'
        LOSS_IDENTITY = 'int previous = oldBases.Find\(base\);'
        GATES = 'bool urgentQRF = decision.m_bUrgent;[\s\S]*if \(!urgentQRF\)[\s\S]*IsStrategicCandidateReady'
        MARGIN = 'AICF_AdaptiveCommanderConfig.SWITCH_MARGIN'
        TIE = 'score.m_fTotal == best.m_fTotal && node < bestNode'
        TRACE = 'now - at < AICF_AdaptiveCommanderConfig.TRACE_INTERVAL_MS'
        SUSPENSION = 'waypointSuspendedByVehicle,\s*decision\);'
    }
    foreach ($rule in $rules.GetEnumerator()) {
        Assert-AICFContains $failures $rule.Key $code $rule.Value 'Adaptive commander contract missing'
    }
    if ($source[1] -match 'SetFaction\(|SpawnEntity|AddWaypoint\(|DeleteRplEntity|CallLater|GetAllPlayers|GetEnemies') {
        $failures.Add('[OWNERSHIP] Policy must stay read-only and avoid enemy enumeration')
    }
    return $failures.ToArray()
}
$failures = @(Test-AdaptiveContracts $text)
if ($failures.Count) { $failures; exit 1 }
$mutations = @(
    @('authority', '!Replication.IsServer() || !m_State', '!m_State', 'AUTHORITY'),
    @('slot', 'm_State.GetSlot(slot.GetSlotId()) != slot', 'false', 'FACTION_SLOT'),
    @('position', 'IEntity leader = AICF_GroupRuntime.ResolveAliveLeader(slot.GetGroup());', 'IEntity leader = slot.GetGroup();', 'LIVE_POSITION'),
    @('force', 'AICF_GroupRuntime.CountAliveAgents(other.GetGroup())', '10', 'ALIVE_FORCE'),
    @('manual', 'slot.HasPlayerStrategicIntent() || slot.HasPlayerStrategicOrder()', 'false', 'MANUAL'),
    @('recruitment', 'other.IsRecruitingInfantry()', 'false', 'RECRUITING'),
    @('route', 'candidate.m_bDeferred = slot.IsRouteTargetDeferred(base);', 'candidate.m_bDeferred = false;', 'ROUTE'),
    @('unknown', 'Math.Max(AICF_AdaptiveCommanderConfig.UNKNOWN_RESISTANCE, candidate.m_fDemand)', 'candidate.m_fDemand', 'UNKNOWN'),
    @('ttl', 'now - candidate.m_iThreatAtMs <= AICF_AdaptiveCommanderConfig.THREAT_TTL_MS', 'true', 'THREAT_TTL'),
    @('allocation', 'candidate.m_fAllocated / demand', '0', 'ALLOCATION'),
    @('generation', 'slot.GetSpawnGeneration() == m_iGeneration', 'true', 'GENERATION'),
    @('graph', 'graph.GetRevision() == m_iGraphRevision', 'true', 'GRAPH'),
    @('assignment', 'slot.GetStrategicAssignmentRevision() == m_iAssignmentRevision', 'true', 'ASSIGNMENT'),
    @('group-id', 'm_Group.GetID() != m_GroupId', 'false', 'GROUP_ID'),
    @('loss-identity', 'int previous = oldBases.Find(base);', 'int previous = -1;', 'LOSS_IDENTITY'),
    @('urgent', 'bool urgentQRF = decision.m_bUrgent;', 'bool urgentQRF = false;', 'GATES'),
    @('margin', 'AICF_AdaptiveCommanderConfig.SWITCH_MARGIN', '0', 'MARGIN'),
    @('tie', 'node < bestNode', 'node > bestNode', 'TIE'),
    @('trace', 'now - at < AICF_AdaptiveCommanderConfig.TRACE_INTERVAL_MS', 'false', 'TRACE')
)
foreach ($case in $mutations) {
    if (-not $text.Contains($case[1])) { throw "Missing mutation input: $($case[0])" }
    $negative = @(Test-AdaptiveContracts ($text.Replace($case[1], $case[2])))
    if (-not ($negative -match ('\[' + $case[3] + '\]'))) { throw "Mutation survived: $($case[0])" }
}
Write-Output "Adaptive commander contracts: PASS; 22 rules; $($mutations.Count) negative mutations"
exit 0
