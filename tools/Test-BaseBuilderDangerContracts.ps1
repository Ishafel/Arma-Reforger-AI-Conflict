param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage3StaticAudit.Common.ps1')
$path = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Construction/AICF_BaseBuilderDanger.c'
$danger = Get-Content -LiteralPath $path -Raw -Encoding UTF8

function Test-DangerContract([string]$Source) {
    $failures = [Collections.Generic.List[string]]::new()
    $code = ConvertTo-AICFCodeText $Source
    Assert-AICFContains $failures 'DANGER_CONFIG_REGISTRATION' $code '\[BaseContainerProps\(\)\]\s*modded class SCR_AIDangerReaction_UnsafeArea' 'Danger reaction must remain registered for prefab deserialization'
    Assert-AICFContains $failures 'DANGER_NATIVE_FALLBACK' $code 'return super.PerformReaction\(utility, threatSystem, dangerEvent, dangerEventCount\)' 'Unrelated AI must keep the native danger reaction'
    return $failures.ToArray()
}

$failures = @(Test-DangerContract $danger)
if ($failures.Count) { $failures; exit 1 }
$cases = @(
    @{Name='metadata'; Source=$danger.Replace('[BaseContainerProps()]', '/* [BaseContainerProps()] */'); Rule='DANGER_CONFIG_REGISTRATION'},
    @{Name='native-fallback'; Source=$danger.Replace('return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);', 'return false;'); Rule='DANGER_NATIVE_FALLBACK'}
)
foreach ($case in $cases) {
    $rejected = @(Test-DangerContract $case.Source)
    if (-not ($rejected -match ('\[' + [regex]::Escape($case.Rule) + '\]'))) {
        throw "Mutation escaped: $($case.Name)"
    }
    Write-Output "PASS negative mutation=$($case.Name) rejected_by=$($case.Rule)"
}
Write-Output 'Base builder danger contracts: PASS; negative mutations=2; runtime=NOT_RUN'
exit 0
