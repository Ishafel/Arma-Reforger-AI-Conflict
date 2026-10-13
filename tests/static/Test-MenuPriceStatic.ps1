param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$rhs = Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Integration'
$policyPath = Join-Path $rhs 'AICF_RHSMenuPricePolicy.c'
$integrationPath = Join-Path $rhs 'AICF_RHSMenuPriceIntegration.c'
$wcsPath = Join-Path $RepositoryRoot 'AIConflictArlandWCSRHS/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSMenuPricePolicy.c'
foreach ($path in @($policyPath,$integrationPath,$wcsPath)) {
    if (-not (Test-Path -LiteralPath $path)) { $failures.Add("MISSING $path") }
}
if (-not $failures.Count) {
    $policy = Get-Content -LiteralPath $policyPath -Raw
    $integration = Get-Content -LiteralPath $integrationPath -Raw
    $wcs = Get-Content -LiteralPath $wcsPath -Raw
    $all = $policy + $integration + $wcs
    foreach ($forbidden in @('RplRpc\(', 'RequestConsumtion\(', 'RequestGeneration\(', 'AddSpawnerSupplies\(', 'SpawnEntityPrefab\(', 'Replication\.BumpMe\(', '\.SetObject\(', '\.SetAncestor\(')) {
        if ($all -match $forbidden) { $failures.Add("TRANSACTION_OR_RESOURCE_MUTATION $forbidden") }
    }
    foreach ($required in @('AICF_RHSContentProfile\.Cast', 'source\.Get\("m_aUnitPrefabSlots", members\)', 'foreach \(ResourceName member : members\)', 'total \+= cost;', 'return -1;')) {
        if ($policy -notmatch $required) { $failures.Add("POLICY_CONTRACT $required") }
    }
    foreach ($required in @('GetEntityAndChildrenBudgetCost', 'GetEntityChildrenBudgetCost', 'GetGroupBudgetCost', 'GetSupplyCost', 'GetEntityBudgetCost', 'AICF_MenuPriceSources.Clear\(\)', 'ref Resource>', 'EEditableEntityBudget.CAMPAIGN', 'AddBudgetValueToBudgetArray', 'super.GetSupplyCost\(\)', 'static override SCR_EditableEntityUIInfo GetInfo')) {
        if ($integration -notmatch $required) { $failures.Add("INTEGRATION_CONTRACT $required") }
    }
    if ($integration -match 'modded class SCR_EditableEntityUIInfo\b') { $failures.Add('PRIVATE_BASE_COPYFROM_UNSUPPORTED') }
    if ($wcs -notmatch 'override int VehicleCost' -or $wcs -notmatch 'super.VehicleCost\(path\)') { $failures.Add('WCS_SHARED_POLICY') }
    $core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts'
    if (Get-ChildItem -LiteralPath $core -Recurse -Filter '*.c' | Select-String 'AICF_RHSMenuPrice|AICF_MenuPriceBinding') { $failures.Add('CORE_DEPENDENCY') }
}
if ($failures.Count) { $failures | Write-Output; exit 1 }
Write-Output 'PASS MenuPrice static boundaries'
exit 0
