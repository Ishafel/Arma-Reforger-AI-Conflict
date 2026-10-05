param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$SuppressionLogPath
)
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
    Assert-AICFContains $failures 'SUPPRESSION_GEOMETRY_BOUNDARY' $code '(?s)if \(!owner\)\s*return ENodeResult.FAIL;\s*IEntity shooter = owner.GetControlledEntity\(\);\s*if \(!shooter \|\| !AICF_SuppressionInputGuard.CanGenerateLine\(volume, shooter.GetOrigin\(\)\)\)\s*return ENodeResult.FAIL;\s*return super.EOnTaskSimulate' 'Geometry must fail before native line generation'
    Assert-AICFContains $failures 'SUPPRESSION_HORIZONTAL_DISTANCE' $code '(?s)shooterPos\[1\] = 0;\s*centerPos\[1\] = 0;\s*if \(!\(vector.DistanceXZ\(shooterPos, centerPos\) > 0.001\)\)\s*return false;' 'Coincident horizontal positions must fail before normalization and distancePerDeg division'
    Assert-AICFContains $failures 'SUPPRESSION_BOX_SCOPE' $code '(?s)SCR_AISuppressionVolumeBox box = SCR_AISuppressionVolumeBox.Cast\(volume\);\s*if \(!box\)\s*return true;' 'Slope guard applies to all box descendants, not spheres'
    Assert-AICFContains $failures 'SUPPRESSION_BOX_BOUNDS' $code 'if \(!\(box.m_vBBMax\[0\] > box.m_vBBMin\[0\]\) \|\| !\(box.m_vBBMax\[2\] > box.m_vBBMin\[2\]\) \|\| !\(box.m_vBBMax\[1\] >= box.m_vBBMin\[1\]\)\)\s*return false;' 'Empty/inverted box must fail'
    Assert-AICFContains $failures 'SUPPRESSION_BOX_SLOPE' $code 'vector direction = vector.Direction\(shooterPos, centerPos\).Normalized\(\);\s*return Math.AbsFloat\(direction\[2\]\) > 0.000001;' 'Rotated direction X comes from original direction Z'
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
    @{Name='native-fallback'; Source=$source.Replace('return super.EOnTaskSimulate(owner, dt);', 'return ENodeResult.FAIL;'); Rule='SCR_AIUpdateTargetAttackData_NATIVE'},
    @{Name='geometry-bypass'; Source=$source.Replace('!AICF_SuppressionInputGuard.CanGenerateLine(volume, shooter.GetOrigin())', 'false'); Rule='SUPPRESSION_GEOMETRY_BOUNDARY'},
    @{Name='coincident-allowed'; Source=$source.Replace('> 0.001', '>= 0'); Rule='SUPPRESSION_HORIZONTAL_DISTANCE'},
    @{Name='target-only'; Source=$source.Replace('SCR_AISuppressionVolumeBox.Cast(volume)', 'SCR_AISuppressionVolumeBaseTargetBox.Cast(volume)'); Rule='SUPPRESSION_BOX_SCOPE'},
    @{Name='flat-box'; Source=$source.Replace('box.m_vBBMax[0] > box.m_vBBMin[0]', 'box.m_vBBMax[0] >= box.m_vBBMin[0]'); Rule='SUPPRESSION_BOX_BOUNDS'},
    @{Name='wrong-axis'; Source=$source.Replace('Math.AbsFloat(direction[2])', 'Math.AbsFloat(direction[0])'); Rule='SUPPRESSION_BOX_SLOPE'},
    @{Name='zero-slope-allowed'; Source=$source.Replace('> 0.000001', '>= 0'); Rule='SUPPRESSION_BOX_SLOPE'}
)
foreach ($case in $cases) {
    $rejected = @(Test-CombatInputs $case.Source)
    if (-not ($rejected -match ('\[' + [regex]::Escape($case.Rule) + '\]'))) { throw "Mutation escaped: $($case.Name)" }
    Write-Output "PASS negative mutation=$($case.Name) rejected_by=$($case.Rule)"
}
Write-Output "AI combat input contracts: PASS; negative mutations=$($cases.Count)"

if ($SuppressionLogPath) {
    $log = Get-Content -LiteralPath $SuppressionLogPath -Raw
    $expected = [Collections.Generic.List[string]]::new()
    foreach ($side in @('US','USSR')) {
        foreach ($name in @('SHOOTER','SAME_Z_POSITIVE_X','SAME_Z_NEGATIVE_X','SAME_XZ','VERTICAL_ONLY','SAME_X_POSITIVE_Z','SAME_X_NEGATIVE_Z','QUADRANT_PP','QUADRANT_PN','QUADRANT_NP','QUADRANT_NN')) {
            $expected.Add("${side}_$name")
        }
    }
    foreach ($name in @('NULL_VOLUME','NEAR_ZERO_SLOPE_REJECTED','SMALL_NONZERO_SLOPE_ALLOWED','NEAR_CENTER_REJECTED','OUTSIDE_CENTER_TOLERANCE_ALLOWED','ZERO_WIDTH','ZERO_DEPTH','INVERTED_WIDTH','INVERTED_HEIGHT','SPHERE_SAME_Z_ALLOWED','SPHERE_SAME_XZ_REJECTED')) {
        $expected.Add($name)
    }
    function Test-SuppressionProbeLog([string]$Text) {
        if ($Text -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|Division by zero|NULL pointer|\[SUPPRESSION_NATIVE_REPRO\]') { return $false }
        if ($Text -notmatch 'Game destroyed\.' -or $Text -notmatch '\[SUPPRESSION_INPUT_FINISHED\] checks=33 failures=0\b') { return $false }
        $checks = [regex]::Matches($Text, '\[SUPPRESSION_INPUT_CHECK\] name=(\S+) passed=(\d+)')
        if ($checks.Count -ne $expected.Count) { return $false }
        foreach ($name in $expected) {
            $matched = @($checks | Where-Object { $_.Groups[1].Value -eq $name -and $_.Groups[2].Value -eq '1' })
            if ($matched.Count -ne 1) { return $false }
        }
        return $true
    }
    if (!(Test-SuppressionProbeLog $log)) { throw 'Suppression boundary runtime: FAIL (incomplete checks, exceptions, or live log)' }
    $badLogs = @(
        $log.Replace('name=ZERO_WIDTH passed=1', 'name=ZERO_WIDTH passed=0'),
        $log.Replace('name=ZERO_DEPTH passed=1', 'name=UNEXPECTED passed=1'),
        $log.Replace('checks=33 failures=0', 'checks=33 failures=1'),
        $log.Replace('Game destroyed.', ''),
        ($log + "`nSCRIPT (E): Virtual Machine Exception`nReason: Division by zero")
    )
    foreach ($badLog in $badLogs) {
        if (Test-SuppressionProbeLog $badLog) { throw 'Suppression log negative mutation escaped' }
    }
    Write-Output 'Suppression boundary runtime: PASS; checks=33; negative log mutations=5'
}
