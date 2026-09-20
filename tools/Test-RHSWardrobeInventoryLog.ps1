[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$lines = @(Get-Content -LiteralPath $LogPath)
$failures = New-Object 'System.Collections.Generic.List[string]'
$items = New-Object 'System.Collections.Generic.List[object]'
$muzzles = New-Object 'System.Collections.Generic.List[object]'
$models = New-Object 'System.Collections.Generic.List[object]'

function Assert-Inventory([bool]$Condition, [string]$Message) {
    if (-not $Condition) { $failures.Add($Message) }
}

foreach ($line in $lines) {
    if ($line -match '\[INVENTORY_MODEL\] phase=(\w+) index=(\d+) count=(\d+) source=(.+)$') {
        $models.Add([pscustomobject]@{ Phase=$Matches[1]; Index=[int]$Matches[2]; Count=[int]$Matches[3]; Source=$Matches[4] })
    }
    elseif ($line -match '\[INVENTORY_ITEM\] phase=(\w+) index=(\d+) item=.+? cloth=(\d) well=(\S+) ammo=(-?\d+) capacity=(-?\d+)(?: chambered=(\d+))? prefab=(.+)$') {
        $items.Add([pscustomobject]@{ Phase=$Matches[1]; Index=[int]$Matches[2]; Cloth=[int]$Matches[3]; Well=$Matches[4]; Ammo=[int]$Matches[5]; Capacity=[int]$Matches[6]; Chambered=[int]$Matches[7]; Prefab=$Matches[8] })
    }
    elseif ($line -match '\[INVENTORY_MUZZLE\] phase=(\w+) index=(\d+) item=.+? muzzle=(\d+) well=(\S+) loaded=(\d+) spares=(\d+) spare_rounds=(\d+) disposable=(\d) prefab=(.+)$') {
        $muzzles.Add([pscustomobject]@{ Phase=$Matches[1]; Index=[int]$Matches[2]; Muzzle=[int]$Matches[3]; Well=$Matches[4]; Loaded=[int]$Matches[5]; Spares=[int]$Matches[6]; SpareRounds=[int]$Matches[7]; Disposable=[int]$Matches[8]; Prefab=$Matches[9] })
    }
}

Assert-Inventory ([bool]($lines -match 'ENGINE\s+: Game destroyed\.')) 'Требуется полный остановленный server console.log.'
Assert-Inventory ($models.Count -eq 46) 'Ожидались 23 модели в каждой фазе before/after.'
Assert-Inventory (-not [bool]($lines -match '\[PMC_.*FAILED\]')) 'В логе есть ошибка equipment draft/commit.'
Assert-Inventory (@($lines -match '\[PMC_ARMAMENT_CHECK\].*passed=1').Count -eq 13) 'Не подтверждены все 13 новых комплектов вооружения ЧВК.'
Assert-Inventory (@($lines -match '\[WARDROBE_CHECK\].*usable_weapon=1.*capability_preserved=1 ai_active=0').Count -eq 23) 'Не подтверждены все 23 wardrobe checks.'

$rows = New-Object 'System.Collections.Generic.List[object]'
foreach ($index in 1..23) {
    $startFailures = $failures.Count
    $before = @($items | Where-Object { $_.Phase -eq 'before' -and $_.Index -eq $index })
    $after = @($items | Where-Object { $_.Phase -eq 'after' -and $_.Index -eq $index })
    foreach ($phase in @('before','after')) {
        $model = @($models | Where-Object { $_.Phase -eq $phase -and $_.Index -eq $index })
        $phaseItems = @($items | Where-Object { $_.Phase -eq $phase -and $_.Index -eq $index })
        Assert-Inventory ($model.Count -eq 1 -and $model[0].Count -eq $phaseItems.Count) "#$index неполный inventory dump: $phase"
    }

    # Сравниваем всё функциональное снаряжение, включая gadget items,
    # которые native считает одеждой. Обычный рюкзак заменяется намеренно.
    $retained = @($before | Where-Object {
        $_.Prefab -match '/Items/Medicine/|/Weapons/Grenades/|/Weapons/Explosives/|/Weapons/Flares/' -or
        ($_.Prefab -match '/Items/Equipment/' -and $_.Prefab -notmatch '/Backpacks/')
    })
    foreach ($group in ($retained | Group-Object Prefab)) {
        $remaining = @($after | Where-Object Prefab -eq $group.Name).Count
        Assert-Inventory ($remaining -ge $group.Count) "#$index потеряно $($group.Count - $remaining) x $($group.Name)"
    }
    foreach ($required in @('/FieldDressing_', '/Tourniquet_', '/Maps/', '/Compass/', '/Radios/', '/Watches/', '/Flashlights/', '/ETool_')) {
        Assert-Inventory ([bool]($after.Prefab -match $required)) "#$index отсутствует $required"
    }
    if ($index -in @(2,13)) {
        foreach ($required in @('/FieldDressing_', '/Tourniquet_', '/MorphineInjection_', '/SalineBag_')) {
            Assert-Inventory (@($after | Where-Object Prefab -match $required).Count -ge 4) "#$index недостаточный медкомплект: $required"
        }
    }
    if ($index -in @(1,12,23)) {
        Assert-Inventory ([bool]($after.Prefab -match '/Binoculars/')) "#$index нет бинокля по роли"
    }
    if ($index -eq 20) {
        Assert-Inventory ([bool]($after.Prefab -match '/Radio_RF10/')) '#20 радист потерял ранцевую RF-10'
    }
    if ($index -eq 21) {
        Assert-Inventory (@($after | Where-Object Prefab -match '/Mine_PMN4/').Count -ge 5) '#21 недостаточно мин'
        Assert-Inventory (@($after | Where-Object Prefab -match '/DemoBlock_TSh400g/').Count -ge 5) '#21 недостаточно подрывных зарядов'
        Assert-Inventory ([bool]($after.Prefab -match '/RepairKit_01/')) '#21 нет ремонтного комплекта'
    }

    $weapons = @($muzzles | Where-Object { $_.Phase -eq 'after' -and $_.Index -eq $index -and $_.Prefab -match '/Rifles/|/MachineGuns/|/Handguns/|/Launchers/' })
    Assert-Inventory ($weapons.Count -gt 0) "#$index нет проверенных стволов"
    foreach ($weapon in $weapons) {
        Assert-Inventory ($weapon.Loaded -gt 0) "#$index пустое оружие: $($weapon.Prefab) muzzle=$($weapon.Muzzle)"
        if (-not $weapon.Disposable) {
            Assert-Inventory ($weapon.Spares -gt 0 -and $weapon.SpareRounds -gt 0 -and $weapon.Well -ne 'NONE') "#$index нет совместимого запаса: $($weapon.Prefab) muzzle=$($weapon.Muzzle)"
        }
    }

    # Дополнительные калибры разрешены только помощникам соответствующего
    # расчёта. Это ловит оставшиеся после перевооружения чужие магазины.
    $supportWell = ''
    $supportMinimum = 0
    switch ($index) {
        8  { $supportWell='MagazineWellPKM'; $supportMinimum=400 }
        9  { $supportWell='MagazineWellRPG7'; $supportMinimum=3 }
        15 { $supportWell='MagazineWellPKM'; $supportMinimum=300 }
        18 { $supportWell='MagazineWellRPG7'; $supportMinimum=3 }
    }
    $allMuzzles = @($muzzles | Where-Object { $_.Phase -eq 'after' -and $_.Index -eq $index })
    foreach ($magazine in ($after | Where-Object Capacity -gt 0)) {
        # У магазина патронник вычитается из Ammo; у одиночной гранаты/
        # ракеты native оставляет Ammo=1 и одновременно chambered=1.
        $full = $magazine.Ammo -eq $magazine.Capacity -or ($magazine.Capacity -gt 1 -and $magazine.Ammo + $magazine.Chambered -eq $magazine.Capacity)
        Assert-Inventory $full "#$index неполные патроны: $($magazine.Prefab)"
        if ($magazine.Well -ne 'NONE') {
            Assert-Inventory ($magazine.Well -in $allMuzzles.Well -or $magazine.Well -eq $supportWell) "#$index чужой калибр $($magazine.Well): $($magazine.Prefab)"
        }
    }
    if ($supportWell) {
        $rounds = ($after | Where-Object Well -eq $supportWell | Measure-Object Ammo -Sum).Sum
        Assert-Inventory ($rounds -ge $supportMinimum) "#$index недостаточно боезапаса расчёта: $supportWell"
        $factionIndices = 1..10
        if ($index -gt 10) { $factionIndices = 11..23 }
        Assert-Inventory ([bool]($muzzles | Where-Object { $_.Phase -eq 'after' -and $_.Index -in $factionIndices -and $_.Well -eq $supportWell })) "#$index нет совместимого оружия в составе"
    }
    $source = @($models | Where-Object { $_.Phase -eq 'after' -and $_.Index -eq $index })
    $rows.Add([pscustomobject]@{
        Index=$index; Source=($source.Source -join ';'); Passed=($failures.Count -eq $startFailures)
        Weapons=(($weapons | ForEach-Object { "$(Split-Path $_.Prefab -Leaf)#$($_.Muzzle): $($_.Loaded)+$($_.SpareRounds)" }) -join '; ')
        Dressing=@($after | Where-Object Prefab -match '/FieldDressing_').Count
        Tourniquet=@($after | Where-Object Prefab -match '/Tourniquet_').Count
        Morphine=@($after | Where-Object Prefab -match '/MorphineInjection_').Count
        Saline=@($after | Where-Object Prefab -match '/SalineBag_').Count
        Equipment=(($after | Where-Object { $_.Prefab -match '/Items/Equipment/|/Weapons/Grenades/|/Weapons/Explosives/' } | Group-Object Prefab | ForEach-Object { "$($_.Count)x $(Split-Path $_.Name -Leaf)" }) -join '; ')
    })
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$items | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $OutputDirectory 'items.csv')
$muzzles | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $OutputDirectory 'muzzles.csv')
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $OutputDirectory 'roster.csv')
$summary = [ordered]@{
    LogPath=(Resolve-Path -LiteralPath $LogPath).Path
    Models=23; Passed=@($rows | Where-Object Passed).Count; Failures=@($failures)
    ScriptErrors=@($lines -match 'SCRIPT\s+\(E\)').Count
    RpcErrors=@($lines -match 'RPL\s+\(E\)').Count
    Scope='Inventory assertions only; native/RHS/stock log errors are reported separately.'
}
$summary | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $OutputDirectory 'summary.json')
if ($failures.Count) {
    Write-Output "RHS wardrobe inventory: FAIL ($($failures.Count) issue(s))"
    $failures | ForEach-Object { Write-Output " - $_" }
    exit 1
}
Write-Output 'RHS wardrobe inventory: PASS (23/23; all muzzles, compatible ammunition, medicine and retained role equipment)'
Write-Output "Full log: $($summary.LogPath); SCRIPT errors=$($summary.ScriptErrors); RPL errors=$($summary.RpcErrors)"
exit 0
