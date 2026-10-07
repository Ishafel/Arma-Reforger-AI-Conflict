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
 return $failures.ToArray()
}
$failures = @(Test-DefendInput $source)
if ($failures.Count) {$failures; exit 1}
$mutations = @(
 @{Text=$source.Replace('return ENodeResult.FAIL;', 'return ENodeResult.SUCCESS;');Rule='INPUT_GUARD'},
 @{Text=$source.Replace('if (!hasInput)', 'if (!inputWaypoint)');Rule='INPUT_PRIORITY'},
 @{Text=$source.Replace('SCR_DefendWaypoint.Cast(inputWaypoint)', 'SCR_AIWaypoint.Cast(inputWaypoint)');Rule='INPUT_PRIORITY'},
 @{Text=$source.Replace('return super.EOnTaskSimulate(owner, dt);', 'return ENodeResult.FAIL;');Rule='INPUT_GUARD'},
 @{Text=$source.Replace('inputWaypoint = group.GetCurrentWaypoint();', 'group.RemoveWaypoint(group.GetCurrentWaypoint()); inputWaypoint = group.GetCurrentWaypoint();');Rule='NO_SIDE_EFFECTS'}
)
foreach($case in $mutations) {
 if (!(@(Test-DefendInput $case.Text) -match $case.Rule)) {throw "Mutation escaped: $($case.Rule)"}
}
'Defend waypoint contracts: PASS; negative mutations=5'
