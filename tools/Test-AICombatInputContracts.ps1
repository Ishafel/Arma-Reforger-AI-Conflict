param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Stage3StaticAudit.Common.ps1')
$source = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Forces/AICF_AICombatInputGuards.c') -Raw -Encoding UTF8

function Test-CombatInputs([string]$Source) {
    $failures = [Collections.Generic.List[string]]::new()
    $code = ConvertTo-AICFCodeText $Source
    $classes = @('SCR_AIUpdateTargetAttackData','SCR_AIGetSuppressionVolumeCenterPosition','SCR_AIGetSuppressionVolumeLine')
    foreach ($class in $classes) {
        $body = [regex]::Match($code, ('(?s)modded class ' + $class + '\s*\{(.*?)(?=modded class|\z)')).Value
        Assert-AICFContains $failures ($class + '_NATIVE') $body 'return super.EOnTaskSimulate\(owner, dt\);' 'Valid inputs must use the native node'
        if ($class -eq 'SCR_AIUpdateTargetAttackData') {
            Assert-AICFContains $failures 'ATTACK_FIRST_UPDATE_GUARD' $body '(?s)if \(m_bFirstSimulate && m_CombatComponent\).*?GetSelectedWeapon\(weapon, muzzleId\);\s*if \(!weapon\)\s*return ENodeResult.FAIL;.*?return super' 'Missing weapon must fail before native initialization'
            if ($body -match 'm_bFirstSimulate\s*=') { $failures.Add('[ATTACK_RETRY_STATE] First-update state must remain native-owned') }
        } else {
            Assert-AICFContains $failures ($class + '_VOLUME') $body '(?s)GetVariableIn\(SUPPRESSION_VOLUME(_PORT)?, volume\);\s*if \(!volume\)\s*return ENodeResult.FAIL;.*?return super' 'Missing volume must fail before the native node'
        }
    }
    return $failures.ToArray()
}

$failures = @(Test-CombatInputs $source)
if ($failures.Count) { $failures; exit 1 }
$cases = @(
    @{Name='weapon-guard'; Source=$source.Replace('if (!weapon)', 'if (false)'); Rule='ATTACK_FIRST_UPDATE_GUARD'},
    @{Name='first-update-consumed'; Source=$source.Replace('if (!weapon)', 'm_bFirstSimulate = false; if (!weapon)'); Rule='ATTACK_RETRY_STATE'},
    @{Name='invalid-success'; Source=$source.Replace('return ENodeResult.FAIL;', 'return ENodeResult.SUCCESS;'); Rule='ATTACK_FIRST_UPDATE_GUARD'},
    @{Name='missing-center-guard'; Source=$source.Replace('GetVariableIn(SUPPRESSION_VOLUME, volume);', ''); Rule='SCR_AIGetSuppressionVolumeCenterPosition_VOLUME'},
    @{Name='missing-line-guard'; Source=$source.Replace('GetVariableIn(SUPPRESSION_VOLUME_PORT, volume);', ''); Rule='SCR_AIGetSuppressionVolumeLine_VOLUME'},
    @{Name='native-fallback'; Source=$source.Replace('return super.EOnTaskSimulate(owner, dt);', 'return ENodeResult.FAIL;'); Rule='SCR_AIUpdateTargetAttackData_NATIVE'}
)
foreach ($case in $cases) {
    $rejected = @(Test-CombatInputs $case.Source)
    if (-not ($rejected -match ('\[' + [regex]::Escape($case.Rule) + '\]'))) { throw "Mutation escaped: $($case.Name)" }
    Write-Output "PASS negative mutation=$($case.Name) rejected_by=$($case.Rule)"
}
Write-Output 'AI combat input contracts: PASS; negative mutations=6; runtime=NOT_RUN'
