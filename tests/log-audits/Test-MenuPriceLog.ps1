param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [ValidateSet(48, 122)][int]$ExpectedVehicles = 122,
    [switch]$Client,
    [switch]$Transactions,
    [switch]$Measurements,
    [string]$CompareLogPath,
    [string]$CsvPath
)
$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Read-Prices([string]$Path) {
    $lines = @(Get-Content -LiteralPath $Path)
    if (-not ($lines -match 'Game destroyed\.')) { $failures.Add("INCOMPLETE_LOG $Path") }
    if ($lines -match 'SCRIPT\s+\((E|F)\):|ENGINE\s+\(F\):|NULL pointer|Unhandled exception|VM Exception|Division by zero') {
        $failures.Add("SCRIPT_FATAL $Path")
    }
    $rows = @{}
    foreach ($line in $lines) {
        if ($line -notmatch '\[MENU_PRICE_CASE\] pass=(\d+) faction=(\S+) index=(\d+) old=(\d+) new=(\d+) debit=(\d+) spawner=(-?\d+) members=(\d+) prefab=(.+?) roles=(.*)$') { continue }
        $row = [pscustomobject]@{ faction=$matches[2]; index=[int]$matches[3]; old=[int]$matches[4]; price=[int]$matches[5]; debit=[int]$matches[6]; spawner=[int]$matches[7]; members=[int]$matches[8]; prefab=$matches[9]; roles=$matches[10] }
        if ($matches[1] -ne '1' -or $row.price -le 0 -or $row.price -ne $row.debit -or ($row.spawner -ge 0 -and $row.price -ne $row.spawner)) { $failures.Add("PRICE_MISMATCH $($row.prefab)") }
        if ($rows.ContainsKey($row.prefab)) { $failures.Add("DUPLICATE $($row.prefab)") }
        $rows[$row.prefab] = $row
    }
    $groups = @($rows.Values | Where-Object members -GT 0)
    $vehicles = @($rows.Values | Where-Object members -EQ 0)
    if (@($lines | Select-String '\[MENU_PRICE_CASE\]').Count -ne $rows.Count) { $failures.Add('MALFORMED_CASE') }
    if ($groups.Count -ne 67 -or $vehicles.Count -ne $ExpectedVehicles) { $failures.Add("COVERAGE groups=$($groups.Count) vehicles=$($vehicles.Count)") }
    if (@($groups | Where-Object faction -EQ RHS_USAF).Count -ne 20 -or @($groups | Where-Object faction -EQ RHS_AFRF).Count -ne 47) { $failures.Add('FACTION_COVERAGE') }
    foreach ($group in $groups) {
        $roles = @([regex]::Matches($group.roles, ':([0-9]+),'))
        $sum = 0
        foreach ($role in $roles) { $sum += [int]$role.Groups[1].Value }
        if ($roles.Count -ne $group.members -or $sum -ne $group.price -or $group.price -lt 50 * $group.members -or $group.price -gt 100 * $group.members) { $failures.Add("ROSTER_PRICE $($group.prefab)") }
    }
    $results = @($lines | Select-String '\[MENU_PRICE_RESULT\]')
    if ($results.Count -ne 1 -or $results[0].Line -notmatch '\[MENU_PRICE_RESULT\] server=(\d+) passed=(\d+) total=(\d+)') { $failures.Add('MISSING_RESULT') }
    elseif ([int]$matches[2] -ne $rows.Count -or [int]$matches[3] -ne $rows.Count) { $failures.Add('RESULT_COUNT') }
    return ,$rows
}

$rows = Read-Prices $LogPath
$content = Get-Content -LiteralPath $LogPath -Raw
$peer = 1
if ($Client) { $peer = 0 }
if ($content -notmatch "\[MENU_PRICE_RESULT\] server=$peer ") { $failures.Add('WRONG_PEER') }
if ($Transactions) {
    $transactionsFound = @([regex]::Matches($content, '\[MENU_TRANSACTION\] pass=1 case=(\d+) price=(\d+) before=([\d.]+) after=([\d.]+) prefab=(.+)'))
    if ($transactionsFound.Count -ne 4 -or [regex]::Matches($content,'\[MENU_TRANSACTION\]').Count -ne 4 -or
        [regex]::Matches($content,'\[MENU_TRANSACTION_PREFLIGHT\]').Count -ne 4 -or
        [regex]::Matches($content,'\[MENU_TRANSACTION_RESULT\]').Count -ne 1 -or
        $content -notmatch '\[MENU_TRANSACTION_RESULT\] passed=4 total=4') { $failures.Add('TRANSACTION_COVERAGE') }
    $cases = @{}
    foreach ($transaction in $transactionsFound) {
        $id = $transaction.Groups[1].Value
        if ([int]$id -gt 3) { $failures.Add("TRANSACTION_ID $id") }
        $cases[$id] = $true
        $price = [double]::Parse($transaction.Groups[2].Value, [cultureinfo]::InvariantCulture)
        $before = [double]::Parse($transaction.Groups[3].Value, [cultureinfo]::InvariantCulture)
        $after = [double]::Parse($transaction.Groups[4].Value, [cultureinfo]::InvariantCulture)
        if ([math]::Abs($before - $after - $price) -gt 0.01) { $failures.Add("TRANSACTION_DEBIT $id") }
        if ($content -notmatch "\[MENU_TRANSACTION_PREFLIGHT\] case=$id insufficient=1 exact=1 cancel_unchanged=1") { $failures.Add("TRANSACTION_PREFLIGHT $id") }
    }
    if ($cases.Count -ne 4) { $failures.Add('TRANSACTION_DUPLICATE') }
}
if ($Measurements) {
    $measured = @([regex]::Matches($content, '\[MENU_PRICE_MEASURE\] pass=1 prefab=(.+?) seats=(\d+) types=([^\s]+) weapons=(.*)'))
    $prefabs = @{}
    foreach ($measurement in $measured) {
        $prefab = $measurement.Groups[1].Value
        if (-not $rows.ContainsKey($prefab) -or $rows[$prefab].members -ne 0 -or $prefabs.ContainsKey($prefab)) { $failures.Add("MEASUREMENT_ID $prefab") }
        $prefabs[$prefab] = $true
        if ([int]$measurement.Groups[2].Value -ne @($measurement.Groups[3].Value.Split(',') | Where-Object { $_ }).Count) { $failures.Add("MEASUREMENT_SEATS $prefab") }
    }
    if ($measured.Count -ne $ExpectedVehicles -or [regex]::Matches($content,'\[MENU_PRICE_MEASURE\]').Count -ne $ExpectedVehicles) { $failures.Add('MEASUREMENT_COVERAGE') }
}
if ($CompareLogPath) {
    $other = Read-Prices $CompareLogPath
    if ((Get-Content -LiteralPath $CompareLogPath -Raw) -notmatch '\[MENU_PRICE_RESULT\] server=1 ') { $failures.Add('COMPARE_NOT_SERVER') }
    foreach ($key in $rows.Keys) {
        if (-not $other.ContainsKey($key)) { $failures.Add("PEER_MISSING $key"); continue }
        foreach ($property in @('faction','index','old','price','debit','spawner','members','roles')) {
            if ($rows[$key].$property -cne $other[$key].$property) { $failures.Add("PEER_MISMATCH $property $key") }
        }
    }
}
if ($CsvPath) { $rows.Values | Sort-Object faction,members,index,prefab | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $CsvPath }
if ($failures.Count) {
    $failures | Write-Output
    exit 1
}
Write-Output "PASS MenuPrice rows=$($rows.Count) vehicles=$ExpectedVehicles peer=$peer"
exit 0
