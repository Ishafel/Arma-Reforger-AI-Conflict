param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$sources = @{
    rhs = [IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Integration/AICF_RHSPreviewStorage.c'))
    wcs = [IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArlandWCSRHS/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSInfantryEquipment.c'))
}
function Rules($inputSources) {
    $rhs = $inputSources.rhs
    $secondary = Get-AICFMethodBody ([pscustomobject]@{Source=$inputSources.wcs;Code=$inputSources.wcs}) 'ArmSecondaryLauncher'
    [ordered]@{
        PRIVATE_VISUALS = $rhs.Contains('owner && owner.GetWorld() != GetGame().GetWorld()') -and $rhs.Contains('Remove(RefreshAttachedItemVisuals)') -and $rhs -match 'RefreshAttachedItemVisuals\(slotID\);\s+return;' -and $rhs.Contains('super.RequestAttachedItemVisualsRefresh(slotID);') -and $rhs -notmatch '\.CallLater|SpawnEntity|DeleteEntity|Replication.BumpMe'
        PRIVATE_REPLACEMENT = $secondary.Contains('model.GetWorld() == GetGame().GetWorld()') -and $secondary.Contains('prefab.IsEmpty()')
        DISPOSABLE_BOUNDARY = $secondary.Contains('disposable.GetWeaponType() != EWeaponType.WT_ROCKETLAUNCHER') -and $secondary.Contains('!disposable.GetCurrentMuzzle().IsDisposable()') -and $secondary.Contains('"SECONDARY_OCCUPIED"')
        NATIVE_READBACK = $secondary.Contains('if (!HasSecondaryLauncher(model, faction, source))') -and $secondary.Contains('"SECONDARY_READBACK"') -and $secondary.Contains('"SECONDARY_EXISTING_AMMO"')
    }
}
$rules = Rules $sources
$failed = @($rules.Keys | Where-Object { -not $rules[$_] })
if ($failed.Count) { throw ('Equipment interop failures: ' + ($failed -join ', ')) }
$mutations = @(
    @('rhs','owner.GetWorld() != GetGame().GetWorld()', 'true', 'PRIVATE_VISUALS'),
    @('rhs','super.RequestAttachedItemVisualsRefresh(slotID);', '', 'PRIVATE_VISUALS'),
    @('rhs','GetGame().GetCallqueue().Remove(RefreshAttachedItemVisuals);', '', 'PRIVATE_VISUALS'),
    @('rhs','RefreshAttachedItemVisuals(slotID);', '', 'PRIVATE_VISUALS'),
    @('wcs','model.GetWorld() == GetGame().GetWorld()', 'false', 'PRIVATE_REPLACEMENT'),
    @('wcs','disposable.GetWeaponType() != EWeaponType.WT_ROCKETLAUNCHER', 'false', 'DISPOSABLE_BOUNDARY'),
    @('wcs','!disposable.GetCurrentMuzzle().IsDisposable()', 'false', 'DISPOSABLE_BOUNDARY'),
    @('wcs','if (!HasSecondaryLauncher(model, faction, source))', 'if (false)', 'NATIVE_READBACK')
)
foreach ($mutation in $mutations) {
    $copy = $sources.Clone()
    if (-not $copy[$mutation[0]].Contains($mutation[1])) { throw "Missing mutation: $($mutation[3])" }
    $copy[$mutation[0]] = $copy[$mutation[0]].Replace($mutation[1], $mutation[2])
    if ((Rules $copy)[$mutation[3]]) { throw "Mutation survived: $($mutation[3])" }
}
"PASS Equipment interop: $($rules.Count) rules, $($mutations.Count) negative mutations; native gates separate"
