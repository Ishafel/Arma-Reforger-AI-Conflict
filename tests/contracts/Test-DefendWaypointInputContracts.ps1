param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source = Get-Content (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AINodeLifecycle.c') -Raw
function Test-DefendInput([string]$Source) {
 $code = ConvertTo-AICFCodeText $Source
 $body = [regex]::Match($code, '(?s)modded class SCR_AIGetDefendWaypointParameters.*?(?=modded class|\z)').Value
 $failures = [Collections.Generic.List[string]]::new()
 Assert-AICFContains $failures 'INPUT_GUARD' $body '(?s)bool hasInput = GetVariableIn\(PORT_WAYPOINT_IN, inputWaypoint\);\s*if \(!AICF_ResolveDefendWaypoint\(owner, hasInput, inputWaypoint\)\)\s*return ENodeResult.FAIL;\s*return super.EOnTaskSimulate\(owner, dt\);' 'Invalid type must fail before native node; valid input stays native'
 Assert-AICFContains $failures 'INPUT_PRIORITY' $body '(?s)if \(!hasInput\)\s*\{\s*AIGroup group = AIGroup.Cast\(owner\);\s*if \(!group\)\s*return null;\s*inputWaypoint = group.GetCurrentWaypoint\(\);\s*\}\s*return SCR_DefendWaypoint.Cast\(inputWaypoint\);' 'Explicit input including null wins over current waypoint'
 if ($body -match 'RemoveWaypoint|AddWaypoint|DeleteRplEntity|CallLater|\.Fail\(|Debug.Error|NodeError') {$failures.Add('[NO_SIDE_EFFECTS] Input guard must not mutate lifecycle')}
 $allocation = [regex]::Match($code, '(?s)modded class SCR_AIAllocateActionsForDefendActivity.*?(?=modded class|\z)').Value
 Assert-AICFContains $failures 'ALLOCATION_INPUT' $allocation 'GetVariableIn\(PORT_WAYPOINT, waypoint\);\s*if \(AICF_IsInvalidManagedDefendInput\(waypoint\)\)' 'Read the actual allocation port; do not substitute current waypoint'
 Assert-AICFContains $failures 'ALLOCATION_FAIL' $allocation '(?s)if \(AICF_IsInvalidManagedDefendInput\(waypoint\)\).*?return ENodeResult.FAIL;\s*\}\s*m_bAICFDefendInputReported = false;\s*return super.EOnTaskSimulate\(owner, dt\);' 'Invalid managed input fails before native allocation; valid input stays native'
 Assert-AICFContains $failures 'ALLOCATION_SCOPE' $allocation '(?s)if \(SCR_DefendWaypoint.Cast\(waypoint\) \|\| !Replication.IsServer\(\) \|\| !m_groupOwner\)\s*return false;\s*AICF_MatchController controller = AICF_MatchController.GetActiveController\(\);\s*return controller && controller.FindManagedInfantrySlot\(m_groupOwner\);' 'Keep authority and exact managed group ownership fences'
 Assert-AICFContains $failures 'ALLOCATION_DIAGNOSTIC' $allocation '(?s)if \(!m_bAICFDefendInputReported\).*m_bAICFDefendInputReported = true;.*Print\(string.Format\(.*LogLevel.WARNING' 'Bounded warning records cancellation instead of hiding it'
 Assert-AICFContains $failures 'ALLOCATION_DIAGNOSTIC' (Get-AICFStringLiteralText $Source) 'AI_DEFEND_ALLOCATION_CANCELLED.*reason=INVALID_DEFEND_INPUT' 'Cancellation event must remain observable'
 if ($allocation -match 'RemoveWaypoint|AddWaypoint|DeleteRplEntity|CallLater|\.Fail\(|Debug.Error|NodeError|GetCurrentWaypoint') {$failures.Add('[ALLOCATION_NO_SIDE_EFFECTS] Allocation guard must not mutate or substitute lifecycle')}
 return $failures.ToArray()
}
$failures = @(Test-DefendInput $source)
if ($failures.Count) {$failures; exit 1}
$mutations = @(
 @{Text=$source.Replace('return ENodeResult.FAIL;', 'return ENodeResult.SUCCESS;');Rule='INPUT_GUARD'},
 @{Text=$source.Replace('if (!hasInput)', 'if (!inputWaypoint)');Rule='INPUT_PRIORITY'},
 @{Text=$source.Replace('SCR_DefendWaypoint.Cast(inputWaypoint)', 'SCR_AIWaypoint.Cast(inputWaypoint)');Rule='INPUT_PRIORITY'},
 @{Text=$source.Replace('return super.EOnTaskSimulate(owner, dt);', 'return ENodeResult.FAIL;');Rule='INPUT_GUARD'},
 @{Text=$source.Replace('inputWaypoint = group.GetCurrentWaypoint();', 'group.RemoveWaypoint(group.GetCurrentWaypoint()); inputWaypoint = group.GetCurrentWaypoint();');Rule='NO_SIDE_EFFECTS'},
 @{Text=$source.Replace('if (AICF_IsInvalidManagedDefendInput(waypoint))', 'if (false)');Rule='ALLOCATION_INPUT'},
 @{Text=$source.Replace('return ENodeResult.FAIL;', 'return ENodeResult.SUCCESS;');Rule='ALLOCATION_FAIL'},
 @{Text=$source.Replace(' || !Replication.IsServer() || !m_groupOwner', ' || !m_groupOwner');Rule='ALLOCATION_SCOPE'},
 @{Text=$source.Replace('controller.FindManagedInfantrySlot(m_groupOwner)', 'true');Rule='ALLOCATION_SCOPE'},
 @{Text=$source.Replace('if (!m_bAICFDefendInputReported)', 'if (true)');Rule='ALLOCATION_DIAGNOSTIC'},
 @{Text=$source.Replace('GetVariableIn(PORT_WAYPOINT, waypoint);', 'waypoint = m_groupOwner.GetCurrentWaypoint();');Rule='ALLOCATION_NO_SIDE_EFFECTS'}
)
foreach($case in $mutations) {
 if (!(@(Test-DefendInput $case.Text) -match $case.Rule)) {throw "Mutation escaped: $($case.Rule)"}
}
'Defend waypoint contracts: PASS; negative mutations=11'
