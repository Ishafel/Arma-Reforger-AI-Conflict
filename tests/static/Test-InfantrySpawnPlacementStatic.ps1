param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_InfantrySpawnPlacement.c'
$source = Get-Content -LiteralPath $path -Raw -Encoding UTF8
function Test-Placement([string]$Text) {
    $code = ConvertTo-AICFCodeText $Text
    $issues = [Collections.Generic.List[string]]::new()
    Assert-AICFContains $issues 'NAVMESH_PENDING_REQUEST' $code 'if\s*\(!navmesh.IsTileLoaded\(query\)\)\s*\{\s*if\s*\(!navmesh.IsTileRequested\(query\)\)\s*navmesh.LoadTileIn\(query\);\s*return false;\s*\}' 'A pending tile must wait without duplicate native requests or premature placement'
    Assert-AICFContains $issues 'NAVMESH_READY_PROJECTION' $code '(?s)GetClosestPositionOnNavmesh\(query,.*?vector.DistanceSqXZ\(query, position\) > 2.*?Math.AbsFloat.*?> 1.*?return false;' 'Loaded tile still requires bounded, terrain-safe projection'
    return $issues.ToArray()
}
$issues = @(Test-Placement $source)
if ($issues.Count) { $issues; exit 1 }
foreach ($mutation in @(
    @{name='pending-guard-removed'; before='if (!navmesh.IsTileRequested(query))'; after=''},
    @{name='pending-guard-inverted'; before='if (!navmesh.IsTileRequested(query))'; after='if (navmesh.IsTileRequested(query))'},
    @{name='waiting-returns-ready'; before="navmesh.LoadTileIn(query);`r`n`t`t`treturn false;"; after="navmesh.LoadTileIn(query);`r`n`t`t`treturn true;"}
)) {
    $normalized = $source.Replace("`r`n", "`n")
    $changed = $normalized.Replace($mutation.before.Replace("`r`n", "`n"), $mutation.after.Replace("`r`n", "`n"))
    if ($changed -eq $normalized -or -not (@(Test-Placement $changed) -match '\[NAVMESH_PENDING_REQUEST\]')) {
        throw "Mutation escaped: $($mutation.name)"
    }
    Write-Output "PASS negative mutation=$($mutation.name)"
}
Write-Output 'Infantry spawn placement static: PASS; negative mutations=3'
exit 0
