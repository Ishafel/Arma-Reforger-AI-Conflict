param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = 'AIConflictCore/Scripts/Game/AIConflict/'
$rules = @(
    @('SERVER_PLAN', ($core + 'Vehicles/AICF_FIAGarrisonPlanner.c'), '!Replication.IsServer() || !faction || !graph || m_Random'),
    @('EASY_ZERO', ($core + 'Vehicles/AICF_FIAGarrisonPlanner.c'), 'if (difficulty == AICF_EDifficulty.EASY) return true;'),
    @('EASY_PATROLS_ZERO', ($core + 'Vehicles/AICF_FIAPatrolService.c'), 'if (AICF_Difficulty.Get() == AICF_EDifficulty.EASY)'),
    @('STRONGPOINT_REQUIRED', ($core + 'Vehicles/AICF_FIAGarrisonPlanner.c'), 'm_sError = "NO_ELIGIBLE_STRONGPOINT";'),
    @('HQ_EDGES_BOTH_DIRECTIONS', ($core + 'Objectives/AICF_FIAStrongpointPolicy.c'), 'graph.GetNode(a).GetOutgoingNodeIds().Contains(b) || graph.GetNode(b).GetOutgoingNodeIds().Contains(a)'),
    @('INTERMEDIATE', ($core + 'Objectives/AICF_FIAStrongpointPolicy.c'), 'if (degree < 2) return false;'),
    @('HQ_COMPONENT', ($core + 'Objectives/AICF_FIAStrongpointPolicy.c'), '!Reachable(graph, nodeId, hq)'),
    @('NO_REROLL', ($core + 'Vehicles/AICF_FIAGarrisonService.c'), '|| m_Planner || m_bStopped) return;'),
    @('JIP', ($core + 'Integration/AICF_FIAGarrisonState.c'), '[RplProp()]'),
    @('IMMUTABLE_SNAPSHOT', ($core + 'Integration/AICF_FIAGarrisonState.c'), '!m_sAICFInitialGarrisonPlan.IsEmpty() || plan.IsEmpty()'),
    @('SEAT_CAPACITY', ($core + 'Forces/AICF_FIAGarrisonCrew.c'), 'if (passengers >= g.m_aPassengerPrefabs.Count()) continue;'),
    @('EXISTING_TEMPLATES', ($core + 'Content/AICF_FIAGarrisonCatalog.c'), 'EEntityCatalogType.GROUP'),
    @('FIA_MEMBERS', ($core + 'Content/AICF_FIAGarrisonCatalog.c'), '!character || !HasFIAIdentity(character)'),
    @('CHARACTER_CLASS', ($core + 'Content/AICF_FIAGarrisonCatalog.c'), 'memberSource.GetClassName().ToType().IsInherited(ChimeraCharacter)'),
    @('EXACT_RESOURCE', ($core + 'Content/AICF_FIAGarrisonCatalog.c'), 'resource.GetResource().GetResourceName() != prefab'),
    @('START_FAILURE', ($core + 'Bootstrap/AICF_MatchController.c'), 'if (!m_FIAGarrisons.IsPlanValid())'),
    @('RADIO_ROLLBACK', 'AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_FIAStrongpointRadioSetup.c', 'changed[i].AICF_SetGarrisonRadioRange(ranges[i], defaults[i])'),
    @('BRIDGE_ROLLBACK', 'AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_FIAStrongpointRadioSetup.c', 'Restore(bases, originalRanges, originalDefaults);'),
    @('RADIO_ACTUAL_GRAPH', 'AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_FIAStrongpointRadioSetup.c', 'SCR_RadioCoverageSystem.UpdateAll(true);')
)
$failures = @()
foreach ($rule in $rules) {
    $source = [IO.File]::ReadAllText((Join-Path $RepositoryRoot $rule[1]))
    if (-not $source.Contains($rule[2])) { $failures += $rule[0] }
}
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
"PASS FIA random garrison structural contracts ($($rules.Count)); behavior requires runtime probe"
