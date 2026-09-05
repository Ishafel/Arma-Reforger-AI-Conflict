param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))

$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($name in @('Construction/AICF_BaseBuilderService.c', 'Construction/AICF_BaseBuilderSpawner.c', 'Orders/AICF_OrderPlanner.c', 'Bootstrap/AICF_MatchController.c')) {
    $sources[$name] = Get-Content -LiteralPath (Join-Path $core $name) -Raw -Encoding UTF8
}
$service = $sources['Construction/AICF_BaseBuilderService.c']
$spawner = $sources['Construction/AICF_BaseBuilderSpawner.c']
$planner = $sources['Orders/AICF_OrderPlanner.c']
$controller = $sources['Bootstrap/AICF_MatchController.c']
$builderState = Get-Content -LiteralPath (Join-Path $core 'Construction/AICF_BaseBuilder.c') -Raw
$danger = Get-Content -LiteralPath (Join-Path $core 'Construction/AICF_BaseBuilderDanger.c') -Raw
$failures = [System.Collections.Generic.List[string]]::new()

function Require-BuilderContract([string]$Id, [string]$Text, [string]$Pattern) {
    if ($Text -notmatch $Pattern) { $failures.Add($Id) }
}

Require-BuilderContract 'BUILDERS_SINGLE_BASE_SLOT' $service '(?s)FindBuilder\(base\).*?if \(!builder\).*?new AICF_BaseBuilder.*?m_aBuilders.Insert\(builder\)'
Require-BuilderContract 'BUILDERS_SINGLE_MEMBER' $spawner '(?s)IgnoreSpawning\(true\).*?SpawnEntityPrefabEx.*?IgnoreSpawning\(false\).*?SetSpawnImmediately\(false\).*?m_aUnitPrefabSlots.Clear\(\);\s*group.m_aUnitPrefabSlots.Insert\(characterPrefab\);'
Require-BuilderContract 'BUILDERS_BIND_BEFORE_REQUEST' $service '(?s)m_GroupId = builder.m_Group.GetID\(\).*?BeginRosterSpawn\(builder.m_Group, 1\)'
Require-BuilderContract 'BUILDERS_ASYNC_READY' $service 'HasExactFactionRoster\(builder.m_Group, faction.GetFactionKey\(\), 1,'
Require-BuilderContract 'BUILDERS_AUTHORITY' $service '(?s)void Update\(\).*?!Replication.IsServer\(\).*?!m_Campaign.IsMaster\(\)'
Require-BuilderContract 'BUILDERS_PROVIDER_RANGE' $service '(?s)GetCampaignMilitaryBaseComponent\(\) != builder.m_Base.*?GetBuildingRadius\(\).*?DistanceSqXZ.*?radius \* radius'
Require-BuilderContract 'BUILDERS_NATIVE_PROGRESS' $service 'layout.AddBuildingValue\(value\)'
Require-BuilderContract 'BUILDERS_PROGRESS_REQUIRES_TOOL' $service '(?s)if \(!StartTool\(builder, controller\)\).*?return;.*?layout.AddBuildingValue\(value\)'
Require-BuilderContract 'BUILDERS_EQUIP_BEFORE_USE' $service '(?s)GetAttachedGadgetAtLeftHandSlot\(\) != tool.*?SetGadgetMode\(tool, EGadgetMode.IN_HAND\).*?return false;.*?TryUseItemOverrideParams'
Require-BuilderContract 'BUILDERS_ANIMATION_CONFIRMED' $service 'builder.m_bToolActive && controller.IsUsingItem\(\)'
Require-BuilderContract 'BUILDERS_ANIMATION_IDENTITY' $builderState '(?s)OnToolUseBegan.*?item != m_UsedTool.*?GetID\(\) != m_CharacterId.*?GetID\(\) != m_GroupId.*?GetID\(\) != m_TargetId'
foreach ($toolEvent in @('OnToolUseBegan', 'OnToolUseEnded')) {
    Require-BuilderContract 'BUILDERS_TOOL_SUBSCRIBE' $service ('\.Insert\(builder\.' + $toolEvent + '\)')
    Require-BuilderContract 'BUILDERS_TOOL_UNSUBSCRIBE' $service ('\.Remove\(builder\.' + $toolEvent + '\)')
}
Require-BuilderContract 'BUILDERS_WORK_OUTSIDE_FOOTPRINT' $service '!builder.IsOutsideFootprint\(builder.m_Character.GetOrigin\(\)\)'
Require-BuilderContract 'BUILDERS_DANGER_SCOPED' $danger '(?s)GetID\(\) == m_CharacterId.*?m_iGeneration == m_iGeneration.*?FindWorkingOnLayout\(m_Layout\) == m_Builder'
Require-BuilderContract 'BUILDERS_OTHER_DANGER_PRESERVED' $danger 'return super.PerformReaction\(utility, threatSystem, dangerEvent, dangerEventCount\)'
if ($service -match '\.(SpawnComposition|SetBuildingValue|SetAffiliatedFaction)\(') { $failures.Add('BUILDERS_NO_COMPLETION_OR_OWNERSHIP_BYPASS') }
Require-BuilderContract 'BUILDERS_IDLE_PHYSICAL_GATE' $service '(?s)ReturnHome\(.*?if \(!MoveTo\(builder, home, now, HOME_METERS\)\).*?m_iIdleAtMs = 0;.*?return;.*?now - builder.m_iIdleAtMs >= IDLE_DELAY_MS'
Require-BuilderContract 'BUILDERS_QUEUE_PURGE' $service 'PurgeSpawnRequestsForGroup\(group\)'
Require-BuilderContract 'BUILDERS_SAFE_RETIRE' $service '(?s)controller.IsPlayerControlled\(\).*?!rpl.IsMaster\(\).*?agent.GetParentGroup\(\) != group.*?return false;.*?group.DespawnMembers\(\)'
Require-BuilderContract 'BUILDERS_WAYPOINT_DETACH_FIRST' $planner '(?s)void ClearBuilderWaypoint.*?RemoveWaypoint\(builder.m_Waypoint\).*?DeleteRplEntity\(builder.m_Waypoint'
Require-BuilderContract 'BUILDERS_AFTER_ARMY_READY' $controller 'if \(m_bRosterReady && m_BaseBuilders\)\s*m_BaseBuilders.Update\(\)'
Require-BuilderContract 'BUILDERS_STOP' $controller '(?s)m_bStopped = true;.*?m_BaseBuilders.Stop\(\)'
foreach ($event in @('OnPlaced')) {
    Require-BuilderContract 'BUILDERS_SUBSCRIBE' $service ('\.Insert\(' + $event + '\)')
    Require-BuilderContract 'BUILDERS_UNSUBSCRIBE' $service ('\.Remove\(' + $event + '\)')
}
if (Test-Path -LiteralPath (Join-Path $core 'Construction/AICF_BaseBuilderRuntimeProbe.c')) {
    $failures.Add('BUILDERS_RUNTIME_FIXTURE_IN_PRODUCTION')
}

if ($failures.Count) {
    Write-Output "Base builders static audit: FAIL ($($failures.Count) issue(s))"
    $failures | ForEach-Object { Write-Output " - [$_]" }
    exit 1
}
Write-Output 'Base builders static audit: PASS'
