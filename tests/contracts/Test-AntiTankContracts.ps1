param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Orders/AICF_AntiTankPolicy.c') -Raw -Encoding UTF8

function Test-AntiTank([string]$Source) {
    $failures = [Collections.Generic.List[string]]::new()
    $code = ConvertTo-AICFCodeText $Source
    $rules = [ordered]@{
        AUTHORITY = '!Replication\.IsServer\(\).*?!AICF_MatchController.GetActiveController\(\)'
        WEAPON = 'GetSelectedWeaponType\(\) != EWeaponType.WT_ROCKETLAUNCHER'
        TARGET_IDENTITY = 'target != combat.GetCurrentTarget\(\)'
        ENEMY = 'target.GetTargetCategory\(\) != ETargetCategory.ENEMY'
        ARMOR_ONLY = 'type == EAIUnitType.UnitType_VehicleMedium \|\| type == EAIUnitType.UnitType_VehicleHeavy'
        PLAYER = '!control.IsPlayerControlled\(\) && rpl && rpl.IsMaster\(\)'
        FOOT = '!access.IsInCompartment\(\) && !access.IsGettingIn\(\) && !access.IsGettingOut\(\)'
        SUPPRESSION = 'GetState\(\) != EAIThreatState.THREATENED'
        ORDERS = '!utility.ShouldKeepFormation\(\) && combat.GetCombatMode\(\) != EAIGroupCombatMode.HOLD_FIRE'
        FIRE_SOLUTION = 'combat.IsTargetVisible\(target\).*?target.GetDistance\(\) >= combat.GetSelectedWeaponMinDist\(\).*?target.GetDistance\(\) <= combat.GetSelectedWeaponMaxDist\(\).*?!combat.IsFriendlyInAim\(\)'
        RANGE_NATIVE = 'super.GetSelectedWeaponProperties\(outMinDistance, outMaxDistance, outDirectDamage\);'
        RANGE_LIMIT = 'outMaxDistance = AICF_AntiTankPolicy.ENGAGEMENT_RANGE;'
        AIM_NATIVE = 'float nativeTolerance = super.GetTolerance.*?weaponType != EWeaponType.WT_ROCKETLAUNCHER.*?return nativeTolerance;'
        MOVE_FALLBACK = 'if \(!AICF_CanRepositionAT\(\)\).*?super.PushRequestMove\(\);.*?return;'
        WAYPOINT = 'ResolveMoveRequestMovePosAndDir\(rq.m_vTargetPos, rq.m_vMovePos, rq.m_vAvoidStraightPathDir,'
        WAYPOINT_RETURN = 'vector.DistanceSq\(rq.m_vMovePos, rq.m_vTargetPos\) < 0.01'
        BOUNDED_MOVE = 'rq.m_fMoveDuration_s = AICF_AntiTankPolicy.MOVE_SECONDS'
        STOP_AIM = 'rq.m_bAimAtTarget = false;.*?rq.m_bAimAtTargetEnd = true;'
    }
    foreach ($rule in $rules.GetEnumerator()) {
        Assert-AICFContains $failures $rule.Key $code ('(?s)' + $rule.Value) 'Anti-tank boundary missing'
    }
    if ($code -match 'SetAISkill|SetPerceptionFactor|SetThreatValues|SetCombatMode|CallLater|\.Insert\(On') {
        $failures.Add('[SIDE_EFFECTS] No global combat tuning or asynchronous callbacks allowed')
    }
    return $failures.ToArray()
}
$failures = @(Test-AntiTank $source)
if ($failures.Count) { $failures; exit 1 }
$cases = @(
    @('authority', '!Replication.IsServer()', 'false', 'AUTHORITY'),
    @('weapon', 'GetSelectedWeaponType() != EWeaponType.WT_ROCKETLAUNCHER', 'GetSelectedWeaponType() == EWeaponType.WT_NONE', 'WEAPON'),
    @('stale-target', 'target != combat.GetCurrentTarget()', 'false', 'TARGET_IDENTITY'),
    @('friendly', 'target.GetTargetCategory() != ETargetCategory.ENEMY', 'false', 'ENEMY'),
    @('player', '!control.IsPlayerControlled()', 'true', 'PLAYER'),
    @('boarding', '!access.IsGettingIn()', 'true', 'FOOT'),
    @('suppressed', 'GetState() != EAIThreatState.THREATENED', 'GetState() != EAIThreatState.SAFE', 'SUPPRESSION'),
    @('formation', '!utility.ShouldKeepFormation()', 'true', 'ORDERS'),
    @('los', 'combat.IsTargetVisible(target)', 'true', 'FIRE_SOLUTION'),
    @('native-range', 'super.GetSelectedWeaponProperties(outMinDistance, outMaxDistance, outDirectDamage);', '', 'RANGE_NATIVE'),
    @('waypoint', 'ResolveMoveRequestMovePosAndDir(rq.m_vTargetPos, rq.m_vMovePos, rq.m_vAvoidStraightPathDir,', 'Removed(rq.m_vTargetPos, rq.m_vMovePos, rq.m_vAvoidStraightPathDir,', 'WAYPOINT'),
    @('waypoint-return', 'vector.DistanceSq(rq.m_vMovePos, rq.m_vTargetPos) < 0.01', 'true', 'WAYPOINT_RETURN'),
    @('shoot-moving', 'rq.m_bAimAtTarget = false;', 'rq.m_bAimAtTarget = true;', 'STOP_AIM')
)
foreach ($case in $cases) {
    $rejected = @(Test-AntiTank $source.Replace($case[1], $case[2]))
    if (-not ($rejected -match ('\[' + $case[3] + '\]'))) { throw "Mutation escaped: $($case[0])" }
    Write-Output "PASS negative mutation=$($case[0]) rejected_by=$($case[3])"
}
Write-Output "Anti-tank contracts: PASS; negative mutations=$($cases.Count); runtime=NOT_RUN"
