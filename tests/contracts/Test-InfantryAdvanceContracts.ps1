param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_InfantryAdvancePolicy.c') -Raw -Encoding UTF8

# Проверяем границы опасного расширения области; поведение и таймеры проверяет engine fixture.
function Test-AdvanceContract([string]$Source) {
    $failures = [Collections.Generic.List[string]]::new()
    $code = ConvertTo-AICFCodeText $Source
    $rules = @{
        ADVANCE_AUTHORITY = '!Replication.IsServer\(\).*?!utility'
        ADVANCE_PLAYER_FENCE = 'control.IsPlayerControlled\(\).*?!rpl.IsMaster\(\)'
        ADVANCE_IDENTITY = 'agent.GetControlledEntity\(\) != character'
        ADVANCE_SLOT = 'slot.GetGroup\(\) != group.*?!slot.IsCombatReady\(\)'
        ADVANCE_WAYPOINT = 'group.GetCurrentWaypoint\(\) != waypoint'
        ADVANCE_INFANTRY = 'slot.GetUnitType\(\) != AICF_EGroupUnitType.INFANTRY'
        ADVANCE_HOLDS = 'slot.IsSystemHoldOrder\(\).*?slot.IsTemporaryRouteReplanHold\(\).*?slot.IsPersistentStuckFieldHold\(\)'
        ADVANCE_VEHICLE = 'access.IsInCompartment\(\).*?access.IsGettingIn\(\).*?access.IsGettingOut\(\)'
        ADVANCE_TARGET = 'utility.m_CombatComponent.GetCurrentTarget\(\)'
        ADVANCE_SUPPRESSION = 'GetSuppressionMeasure\(\) > MAX_SUPPRESSION'
        ADVANCE_INJURY = 'injury > 0'
        ADVANCE_SECTORS = 'index < SCR_AISectorThreatFilter.SECTOR_COUNT'
        ADVANCE_DIRECT_FIRE = 'flags & \(SCR_EAIThreatSectorFlags.DIRECTED_AT_ME \| SCR_EAIThreatSectorFlags.CAUSED_DAMAGE\)'
        ADVANCE_NATIVE = 'float nativePriority = super.CustomEvaluate\(\);'
        ADVANCE_ARRIVAL = 'DistanceSqXZ\(character.GetOrigin\(\), waypoint.GetOrigin\(\)\) > radius \* radius'
    }
    foreach ($rule in $rules.Keys) {
        Assert-AICFContains $failures $rule $code ('(?s)' + $rules[$rule]) 'Infantry advance safety boundary missing'
    }
    if ($code -match 'SetThreatValues|SetCombatMode|SetAISkill|SetPriorityLevel|CallLater|\.Insert\(') {
        $failures.Add('[ADVANCE_NO_SIDE_EFFECTS] Policy must only evaluate current context and priority')
    }
    return $failures.ToArray()
}

$failures = @(Test-AdvanceContract $source)
if ($failures.Count) { $failures; exit 1 }
$cases = @(
    @('authority','!Replication.IsServer()', 'false', 'ADVANCE_AUTHORITY'),
    @('player','control.IsPlayerControlled()', 'false', 'ADVANCE_PLAYER_FENCE'),
    @('identity','agent.GetControlledEntity() != character', 'false', 'ADVANCE_IDENTITY'),
    @('slot','slot.GetGroup() != group', 'false', 'ADVANCE_SLOT'),
    @('waypoint','group.GetCurrentWaypoint() != waypoint', 'false', 'ADVANCE_WAYPOINT'),
    @('infantry','slot.GetUnitType() != AICF_EGroupUnitType.INFANTRY', 'false', 'ADVANCE_INFANTRY'),
    @('hold','slot.IsSystemHoldOrder()', 'false', 'ADVANCE_HOLDS'),
    @('vehicle','access.IsGettingIn()', 'false', 'ADVANCE_VEHICLE'),
    @('target','utility.m_CombatComponent.GetCurrentTarget()', 'false', 'ADVANCE_TARGET'),
    @('suppression','GetSuppressionMeasure() > MAX_SUPPRESSION', 'GetSuppressionMeasure() < 0', 'ADVANCE_SUPPRESSION'),
    @('injury','injury > 0', 'false', 'ADVANCE_INJURY'),
    @('side-sector','index < SCR_AISectorThreatFilter.SECTOR_COUNT', 'index < 1', 'ADVANCE_SECTORS'),
    @('flyby','SCR_EAIThreatSectorFlags.DIRECTED_AT_ME | SCR_EAIThreatSectorFlags.CAUSED_DAMAGE', '0', 'ADVANCE_DIRECT_FIRE'),
    @('native','float nativePriority = super.CustomEvaluate();', 'float nativePriority = 69;', 'ADVANCE_NATIVE'),
    @('arrival','> radius * radius', '> 0', 'ADVANCE_ARRIVAL')
)
foreach ($case in $cases) {
    $rejected = @(Test-AdvanceContract ($source.Replace($case[1], $case[2])))
    if (-not ($rejected -match ('\[' + $case[3] + '\]'))) { throw "Mutation escaped: $($case[0])" }
    Write-Output "PASS negative mutation=$($case[0]) rejected_by=$($case[3])"
}
if (Test-Path -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_InfantryAdvanceProbe.c')) {
    throw 'Runtime fixture must not remain in production Core'
}
Write-Output 'Infantry advance contracts: PASS; negative mutations=15; runtime=NOT_RUN'
