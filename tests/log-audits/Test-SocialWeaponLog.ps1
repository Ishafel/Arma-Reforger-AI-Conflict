param([Parameter(Mandatory)][string]$LogPath, [string]$ClientLogPath)
$ErrorActionPreference = 'Stop'
$log = [IO.File]::ReadAllText((Resolve-Path $LogPath))
if ($log -notmatch 'Game destroyed\.' -or $log -match 'SCRIPT\s+\([EF]\)|Virtual Machine Exception|NULL pointer') {
    throw 'Нужен полный остановленный log без SCRIPT errors/exceptions'
}
$expected = foreach ($case in 0..8) {
    "WEAPON_INITIAL_$case"
    "WEAPON_RESTORED_$case"
    if ($case -ne 6) { "WEAPON_HOLSTERED_$case"; "WEAPON_CANCEL_$case" }
}
foreach ($case in $expected) {
    if ([regex]::Matches($log, "\[SOCIAL_PROBE\] case=$case pass=1\b").Count -ne 1) { throw "Нет единственного PASS: $case" }
}
if ($log -match '\[SOCIAL_PROBE\] case=\S+ pass=0' -or $log -notmatch '\[SOCIAL_PROBE\] finished=1 passed=34 failed=0') {
    throw 'Runtime matrix не завершилась успешно'
}
if ($ClientLogPath) {
    $client = [IO.File]::ReadAllText((Resolve-Path $ClientLogPath))
    if ($client -notmatch 'Game destroyed\.' -or $client -match 'SCRIPT\s+\([EF]\)|Virtual Machine Exception|NULL pointer') { throw 'Client log incomplete or script failure' }
    if ($client -match '\[WEAPON_CLIENT\] case=\d+ pass=0') { throw 'Client reported a failed observation' }
    foreach ($case in 0..8) {
        if ([regex]::Matches($client, "\[WEAPON_CLIENT\] case=$case pass=1\b").Count -ne 1) { throw "Client weapon state failed: $case" }
    }
}
Write-Output 'Social weapon restoration: PASS; cases=34; variants=4; order/threat/early/unarmed/Stop; visual=NOT_RUN'
