param([Parameter(Mandatory=$true)][string]$LogPath, [switch]$Baseline)
$ErrorActionPreference = 'Stop'
$lines = [IO.File]::ReadAllLines((Resolve-Path -LiteralPath $LogPath))
$text = $lines -join "`n"
$failed = [Collections.Generic.List[string]]::new()
$cliMatches = [regex]::Matches($text, 'CLI Params: ([^\r\n]+)')
$cli = ''
if ($cliMatches.Count -eq 1) { $cli = $cliMatches[0].Groups[1].Value }
$addons = [regex]::Match($cli, '-addons ([0-9A-F,]+)').Groups[1].Value.Split(',')
if (-not $cli -or '9178E5822AFE48EA' -notin $addons) { $failed.Add('CLI_GRAPH') }
if ($text -notmatch '\[ROSTER_READY\]' -or $text -notmatch 'Game destroyed\.') { $failed.Add('FULL_STOPPED_LOG') }
$checks = @([regex]::Matches($text, '\[AMBIENT_CHECK\] rule=(\S+) passed=([01])'))
if (-not $checks.Count -or @($checks | Where-Object { $_.Groups[2].Value -ne '1' }).Count) { $failed.Add('RUNTIME_CASE_FAILED') }
$done = [regex]::Matches($text, '\[AMBIENT_DONE\] mode=(\S+) checks=(\d+) failures=(\d+)')
if ($done.Count -ne 1 -or [int]$done[0].Groups[2].Value -ne $checks.Count -or [int]$done[0].Groups[3].Value -ne 0) { $failed.Add('DONE_COUNTS') }
$required = @('NO_PLAYERS','NATIVE_POSITION','FAR_NO_SPAWN','PHYSICAL_APPROACH')
$copies = 1
if ($Baseline) { $required += 'REPRO_NO_CONTROLLER_OR_MEMBERS' }
else {
    $copies = 2
    $required += @('POLICY_NULL','POLICY_LIVE','POLICY_WRONG_FACTION','POLICY_FIA_REJECTED','POLICY_EMPTY_CONTROLLER','BUDGET_NATIVE_REFUSAL','NEGATIVE_ROSTER_MATERIALIZED','POLICY_KILLED_ROSTER','LIVE_ACTIVE_NATIVE_FIA','HELD_15_LIFECYCLE_INTERVALS','NATIVE_DEMATERIALIZE','NO_DUPLICATE_CONTROLLER_OR_MEMBERS','KILLED_NO_LIVE_FIA','NO_RESURRECTION','EXTRA_GARRISON_IDENTITIES')
    foreach ($rule in @('NATIVE_BASE_REMNANT_SELECTED','NATIVE_FACTION_RETIRES_REMNANT','STOP_REMOVES_ALL_OWNED_OBSERVERS')) {
        if (@($checks | Where-Object { $_.Groups[1].Value -eq $rule }).Count -ne 1) { $failed.Add("CASE_$rule") }
    }
    if ($text -notmatch '\[AMBIENT_SIDE\] faction=USSR') { $failed.Add('BOTH_FACTIONS') }
    $members = @($lines | Where-Object { $_ -match '\[AMBIENT_MEMBER\] phase=(APPROACH|HELD) ' })
    if (-not $members.Count -or @($members | Where-Object { $_ -notmatch 'alive=1 active=1' }).Count) { $failed.Add('ACTIVE_MEMBER_READBACK') }
    if ('9F88011DA22B471C' -in $addons) {
        $ids = @($members | ForEach-Object { [regex]::Match($_, 'id=(0x[0-9A-F]+)').Groups[1].Value } | Sort-Object -Unique)
        foreach ($id in $ids) {
            if ($text -notmatch ('\[PMC_EQUIPMENT_APPLIED\] entity=' + [regex]::Escape($id) + ' .*inventory_verified=1')) { $failed.Add("PROFILE_INVENTORY_$id") }
        }
    }
    if (@($lines | Where-Object { $_ -match '\[AMBIENT_BUDGET\] reason=AI_BUDGET active=\d+ limit=1 allowed=0' }).Count -ne 2) { $failed.Add('NATIVE_BUDGET_READBACK') }
}
foreach ($rule in $required) {
    if (@($checks | Where-Object { $_.Groups[1].Value -eq $rule }).Count -ne $copies) { $failed.Add("CASE_$rule") }
}
if ($Baseline -and $text -notmatch '\[AMBIENT_SAMPLE\] phase=APPROACH .*spawned=0 .*group=NONE alive=0 .*attacker=[1-9]') { $failed.Add('REPRO_READBACK') }
$scriptErrors = @($lines | Where-Object { $_ -match 'SCRIPT\s+\((E|F)\)' -and $_ -notmatch "Leaked '.*' script instance|==== Total Leaks" })
$leaks = @($lines | Where-Object { $_ -match "SCRIPT\s+\(E\).*Leaked '" })
$ownLeaks = @($leaks | Where-Object { $_ -match "Leaked 'AICF_|Leaked '.*<.*AICF_" })
if ($scriptErrors.Count -or $ownLeaks.Count -or $text -match 'ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|NULL instance') { $failed.Add('SCRIPT_OR_FATAL_ERROR') }
if ($text -match '\[AICF\].*\[ERROR\]|\[PMC_EQUIPMENT_FAILED\]') { $failed.Add('AICF_ERROR') }
"Native/content diagnostics: leaks=$($leaks.Count), other_errors=$(@($lines | Where-Object { $_ -match '\(E\)' -and $_ -notmatch 'SCRIPT\s+\(E\)' }).Count); unresolved native log cleanliness is separate from functional verdict"
if ($failed.Count) { throw ('Ambient patrol log FAIL: ' + ($failed -join ', ')) }
"PASS Ambient patrol functional cases=$($checks.Count); baseline=$Baseline; full stopped log=$LogPath"
