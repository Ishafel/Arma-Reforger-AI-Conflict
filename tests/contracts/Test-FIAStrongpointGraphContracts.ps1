param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$setup = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_FIAStrongpointRadioSetup.c') -Raw
$policy = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Objectives/AICF_FIAStrongpointPolicy.c') -Raw
$planner = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Vehicles/AICF_FIAGarrisonPlanner.c') -Raw

function Test-GraphGates([string]$Setup, [string]$Policy, [string]$Planner) {
    $failures = [Collections.Generic.List[string]]::new()
    $s = ConvertTo-AICFCodeText $Setup
    $p = ConvertTo-AICFCodeText $Policy
    $b = ConvertTo-AICFCodeText $Planner
    $gate = 'AICF_FIAStrongpointPolicy.IsGraphReady(graph, headquarters, candidates)'
    if ([regex]::Matches($s, [regex]::Escape($gate)).Count -ne 3 -or $s.Contains('if (!candidates.IsEmpty()) return true;')) {
        $failures.Add('ALL_SETUP_SUCCESS_PATHS')
    }
    if ($p -notmatch 'if \(!Reachable\(graph, 0, i\)\) return false;') { $failures.Add('WHOLE_GRAPH_CONNECTED') }
    if ($p -notmatch 'node.IsObjective\(\) && !headquarters.Contains\(next\)' -or $p -notmatch 'if \(!hasExit\) return false;') { $failures.Add('HQ_OBJECTIVE_EGRESS') }
    if ($b -notmatch 'if \(!AICF_FIAStrongpointPolicy.IsGraphReady\(graph, headquarters, candidates\)\)' -or -not $Planner.Contains('m_sError = "STRONGPOINT_GRAPH_NOT_READY";')) { $failures.Add('PLANNER_REJECTS_INVALID_GRAPH') }
    return $failures.ToArray()
}
$failures = @(Test-GraphGates $setup $policy $planner)
if ($failures.Count) { $failures; exit 1 }
$gate = 'AICF_FIAStrongpointPolicy.IsGraphReady(graph, headquarters, candidates)'
for ($index = 0; $index -lt 3; $index++) {
    $matches = [regex]::Matches($setup, [regex]::Escape($gate))
    $at = $matches[$index].Index
    $mutated = $setup.Remove($at, $gate.Length).Insert($at, '!candidates.IsEmpty()')
    if ('ALL_SETUP_SUCCESS_PATHS' -notin @(Test-GraphGates $mutated $policy $planner)) { throw "Setup bypass escaped: $index" }
}
if ('WHOLE_GRAPH_CONNECTED' -notin @(Test-GraphGates $setup $policy.Replace('if (!Reachable(graph, 0, i)) return false;', '') $planner)) { throw 'Disconnected graph mutation escaped' }
if ('HQ_OBJECTIVE_EGRESS' -notin @(Test-GraphGates $setup $policy.Replace('if (!hasExit) return false;', '') $planner)) { throw 'HQ egress mutation escaped' }
if ('PLANNER_REJECTS_INVALID_GRAPH' -notin @(Test-GraphGates $setup $policy $planner.Replace("if (!$gate)", 'if (false)'))) { throw 'Planner bypass escaped' }
'PASS strongpoint graph gates; negative mutations=6; behavior requires runtime probe'
