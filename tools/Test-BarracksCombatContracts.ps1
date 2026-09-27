param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$files = @{
    Safety = 'Forces/AICF_BarracksCombatSafety.c'
    Events = 'Forces/AICF_BarracksCombatEvents.c'
    Config = 'Config/AICF_InfantryRecruitmentConfig.c'
    Order = 'Forces/AICF_InfantryRecruitmentOrder.c'
    Service = 'Forces/AICF_InfantryRecruitmentService.c'
    Spawner = 'Forces/AICF_InfantryRecruitSpawner.c'
    Economy = 'Economy/AICF_InfantryRecruitmentEconomy.c'
}
$sources = @{}
foreach ($key in $files.Keys) { $sources[$key] = Get-Content (Join-Path $core $files[$key]) -Raw -Encoding UTF8 }
function Test-Contracts([hashtable]$s) {
    $rules = @(
        @('Config','COMBAT_RADIUS_METERS = 100;','RADIUS'),
        @('Config','COMBAT_QUIET_MS = 30000;','QUIET'),
        @('Safety','(?s)bool IsAuthority\(\).*?Replication.IsServer\(\) && m_Campaign && m_Campaign.IsMaster\(\) && m_Campaign.IsRunning\(\)','AUTHORITY'),
        @('Safety','(?s)static bool IsSafe.*?!s_Active \|\| !s_Active.IsAuthority\(\).*?return false;.*?FindZone\(service\).*?zone && System.GetTickCount\(\) >= zone.m_iQuietUntilMs','FAIL_CLOSED'),
        @('Safety','(?s)static void RecordCombat.*?!s_Active \|\| !s_Active.IsAuthority\(\).*?return;.*?zone.Matches\(zone.m_Service\).*?DistanceSqXZ\(position, zone.m_vPosition\) > radius \* radius.*?m_iQuietUntilMs = now \+ AICF_InfantryRecruitmentConfig.COMBAT_QUIET_MS','SLIDING_INCLUSIVE_RADIUS'),
        @('Safety','(?s)bool Matches.*?service == m_Service.*?GetID\(\) == m_Id.*?GetOrigin\(\) == m_vPosition','ZONE_IDENTITY'),
        @('Safety','(?s)void Stop\(\).*?s_Active = null;.*?observer.Stop\(\);.*?m_aObservers.Clear\(\);.*?m_aZones.Clear\(\);','CLEANUP'),
        @('Events','(?s)super.EOnInit\(owner\);.*?TrackCharacter\(this\);','NEW_CHARACTERS'),
        @('Events','(?s)super.OnDamage\(damageContext\);.*?damageContext.damageValue <= 0.*?EDamageType.KINETIC:.*?EDamageType.EXPLOSIVE:.*?RecordCombat\(position, "DAMAGE"\)','DAMAGE'),
        @('Order','services.Contains\(m_Service\) && AICF_BarracksCombatSafety.IsSafe\(m_Service\)','SHARED_GATE'),
        @('Service','(?s)m_CombatSafety.Update\(m_Campaign, m_Graph\);.*?Tick\(order, graphReady\)','UPDATE_BEFORE_ORDERS'),
        @('Service','(?s)void Stop\(\).*?Finish\(index, "STOP", false\);.*?m_CombatSafety.Stop\(\);','LIFECYCLE'),
        @('Spawner','(?s)bool BeginRecruit.*?!order.HasSafeBarracks\(\).*?BeginRosterSpawn','SPAWN_GATE'),
        @('Economy','(?s)QuoteInfantryRecruit.*?order.HasSafeBarracks\(\).*?DebitInfantryRecruit.*?!QuoteInfantryRecruit\(order\).*?AddSupplies\(-order.m_iCost\)','PAYMENT_GATE')
    )
    $failures = @()
    foreach ($rule in $rules) { if ($s[$rule[0]] -notmatch $rule[1]) { $failures += $rule[2] } }
    foreach ($event in @('OnProjectileShot','OnGrenadeThrown')) {
        foreach ($method in @('RegisterScriptHandler','RemoveScriptHandler')) {
            if ($s.Safety -notmatch ($method + '\("' + $event + '", this, \w+, true\);')) { $failures += "$event-$method" }
        }
    }
    if ($s.Events -match 'case EDamageType\.(BLEEDING|COLLISION|HEALING|REGENERATION)') { $failures += 'NON_COMBAT_DAMAGE' }
    return $failures
}
$failures = @(Test-Contracts $sources)
if ($failures.Count) { $failures; exit 1 }
$mutations = @(
    @('Config','COMBAT_RADIUS_METERS = 100;','COMBAT_RADIUS_METERS = 99;','RADIUS'),
    @('Config','COMBAT_QUIET_MS = 30000;','COMBAT_QUIET_MS = 3000;','QUIET'),
    @('Safety','> radius * radius','>= radius * radius','SLIDING_INCLUSIVE_RADIUS'),
    @('Safety','now + AICF_InfantryRecruitmentConfig.COMBAT_QUIET_MS','AICF_InfantryRecruitmentConfig.COMBAT_QUIET_MS','SLIDING_INCLUSIVE_RADIUS'),
    @('Safety','RemoveScriptHandler("OnProjectileShot"','RegisterScriptHandler("OnProjectileShot"','OnProjectileShot-RemoveScriptHandler'),
    @('Safety','OnWeaponFired, true','OnWeaponFired, false','OnProjectileShot-RegisterScriptHandler'),
    @('Order',' && AICF_BarracksCombatSafety.IsSafe(m_Service)','','SHARED_GATE'),
    @('Safety','zone && System.GetTickCount() >=','zone && System.GetTickCount() <=','FAIL_CLOSED'),
    @('Events','case EDamageType.MELEE:','case EDamageType.BLEEDING:','NON_COMBAT_DAMAGE')
)
foreach ($mutation in $mutations) {
    $changed = $sources.Clone()
    $changed[$mutation[0]] = $changed[$mutation[0]].Replace($mutation[1], $mutation[2])
    if (@(Test-Contracts $changed) -notcontains $mutation[3]) { throw "Mutation escaped: $($mutation[3])" }
    Write-Output "PASS negative mutation=$($mutation[3])"
}
Write-Output 'Barracks combat contracts: PASS; negative mutations=9; runtime=NOT_RUN'
