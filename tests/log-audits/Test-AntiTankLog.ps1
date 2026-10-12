param([Parameter(Mandatory=$true)][string]$LogPath)
$ErrorActionPreference = 'Stop'
$log = Get-Content -LiteralPath $LogPath -Raw -Encoding UTF8
$failures = [Collections.Generic.List[string]]::new()
if ($log -notmatch '\[ROSTER_READY\]' -or $log -notmatch 'Game destroyed\.') { $failures.Add('Incomplete runtime lifecycle') }
if ($log -notmatch '\[AT_ENGAGEMENT_FINISHED\] placed=1 selected=1 fired=1') { $failures.Add('No successful anti-tank engagement') }
$shots = [regex]::Matches($log, '\[AT_ENGAGEMENT_PROJECTILE\] distance=([\d.]+) projectile=')
$longShot = $false
foreach ($shot in $shots) {
    $distance = [double]::Parse($shot.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ($distance -ge 300 -and $distance -le 400) { $longShot = $true }
}
if (-not $longShot) { $failures.Add('No actual RPG projectile against the fixture vehicle at 300-400 m') }
if ($log -notmatch '\[AT_ENGAGEMENT_SAMPLE\].*selected=1 .* max=400 ') { $failures.Add('Effective range not observed') }
if ($log -notmatch '\[AT_ENGAGEMENT_AIM\] pass=1 ' -or $log -notmatch '\[AT_ENGAGEMENT_MOVE\] pass=1 ') { $failures.Add('Runtime aim/movement contracts not confirmed') }
if ($log -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Unhandled exception|NULL pointer|PMC_EQUIPMENT_FAILED') { $failures.Add('SCRIPT/fatal gate failed') }
$otherErrors = [regex]::Matches($log, '\([EF]\):').Count
Write-Output "Full-log error lines retained: $otherErrors"
if ($failures.Count) { $failures; exit 1 }
Write-Output "Anti-tank runtime: PASS; actual projectile events=$($shots.Count); hit rate/client/JIP=NOT_RUN"
exit 0
