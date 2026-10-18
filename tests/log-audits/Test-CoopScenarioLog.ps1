param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$NativeExitPath,
    [switch]$Client,
    [switch]$RequirePlayer,
    [string]$ExpectedWinner,
    [int]$MinimumObservationMs=30000
)
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
if($RequirePlayer -and -not $Client) { $failures.Add('PLAYER_ROLE') }
$log=[IO.File]::ReadAllText($LogPath)
$launcher=[IO.File]::ReadAllText($LauncherPath)
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0') { $failures.Add('NATIVE_EXIT') }
if($log -notmatch 'Game destroyed\.') { $failures.Add('STOPPED_LOG_REQUIRED') }
if($log -match 'SCRIPT\s+\([EF]\):|ENGINE\s+\(F\):|NULL pointer|Virtual Machine Exception|Unhandled exception') { $failures.Add('SCRIPT_FATAL') }
if($log -match '\[COOP_TIMEOUT\]|\[COOP_CASE\] case=\w+ pass=0') { $failures.Add('PROBE_FAILURE') }
$manifest=$null
if($launcher -notmatch 'AICF_RUNTIME_MANIFEST_JSON=({[^\r\n]+})') { $failures.Add('MANIFEST') }
else { $manifest=$matches[1] | ConvertFrom-Json }
if($manifest) {
    $expectedRole='Server'; if($Client) { $expectedRole='Client' }
    if($manifest.role -cne $expectedRole -or $manifest.variant -notin @('Stock','StockCoopUSSR')) { $failures.Add('LAUNCH_ROLE_VARIANT') }
    $mode='USSR'; if($manifest.variant -eq 'Stock') { $mode='STANDARD' }
    $argsList=@($manifest.arguments)
    $indices=@(for($i=0;$i -lt $argsList.Count;$i++) { if($argsList[$i] -ceq '-aicfCoopProbe') {$i} })
    if($indices.Count -ne 1 -or $indices[0]+1 -ge $argsList.Count -or $argsList[$indices[0]+1] -cne $mode) { $failures.Add('PROBE_MODE') }
    if($Client) {
        if($manifest.variant -ne 'StockCoopUSSR' -or $launcher -notmatch '\[SERVER_READY\] process_id=\d+ port=\d+ log=') { $failures.Add('CLIENT_READINESS') }
        if([regex]::Matches($log,'\[COOP_CLIENT_DONE\] pass=1 samples=\d+').Count -ne 1) { $failures.Add('CLIENT_READBACK') }
        if($RequirePlayer -and $log -notmatch '\[COOP_CLIENT_PLAYER\] pass=1 entity=') { $failures.Add('PLAYER_SPAWN') }
        if($RequirePlayer) {
            foreach($side in @('US','FIA')) {
                if([regex]::Matches($log,("\[COOP_CLIENT_FACTION\] faction=$side accepted=0 pass=1")).Count -ne 1) { $failures.Add("FACTION_DENIED_$side") }
            }
            if($log -match '\[COOP_CLIENT_FACTION\][^\r\n]*pass=0') { $failures.Add('FACTION_ACCESS_FAILED') }
        }
        if($RequirePlayer) {
            $playerIndices=@(for($i=0;$i -lt $argsList.Count;$i++) { if($argsList[$i] -ceq '-aicfCoopProbePlayer') {$i} })
            if($playerIndices.Count -ne 1 -or $playerIndices[0]+1 -ge $argsList.Count -or $argsList[$playerIndices[0]+1] -cne '1') { $failures.Add('PLAYER_MODE') }
        }
    } else {
        $done=[regex]::Matches($log,'\[COOP_DONE\] mode=(USSR|STANDARD) checks=(\d+) failed=0 elapsed_ms=(\d+)')
        if($done.Count -ne 1 -or $done[0].Groups[1].Value -cne $mode -or [int]$done[0].Groups[3].Value -lt $MinimumObservationMs) { $failures.Add('COMPLETION') }
        $expected=@('US_ROSTER','USSR_ROSTER','QUEUE_SETTLED','US_TICKETS','USSR_TICKETS','COMMANDERS','HELPERS','BUDGET_FLOOR','STANDARD_POLICY','MIRROR_POLICY','STABLE_ROSTER','REPLICATED_MODE','FACTION_ACCESS','AUTO_CONSTRUCTION')
        if($mode -eq 'USSR') { $expected+=@('ZERO_PLAYER_TICKETS_NO_DEFEAT','CONFLICT_REJECTED','NO_GROUP_SUMMARY') }
        if($ExpectedWinner) { $expected+='TERRITORIAL_WINNER' }
        $cases=[regex]::Matches($log,'\[COOP_CASE\] case=(\w+) pass=1')
        if($cases.Count -ne $expected.Count -or $done.Count -eq 1 -and [int]$done[0].Groups[2].Value -ne $expected.Count) { $failures.Add('CASE_COUNT') }
        foreach($name in $expected) { if(@($cases | Where-Object {$_.Groups[1].Value -ceq $name}).Count -ne 1) { $failures.Add("CASE_$name") } }
        $ussr=10; if($mode -eq 'USSR') {$ussr=0}
        if($log -notmatch ("\[ROSTER_READY\][^\r\n]*us_groups=10 ussr_groups=$ussr\b")) { $failures.Add('ROSTER') }
        if($mode -eq 'USSR' -and $log -match '\[(?:SPAWN_ATTEMPT_STARTED|GROUP_ROSTER_READY|REPLACEMENT_SPAWNED)\][^\r\n]*faction=USSR\b') { $failures.Add('PLAYER_COMBAT_SPAWN') }
        if($ExpectedWinner -and $log -notmatch ("\[VICTORY\][^\r\n]*winner=$ExpectedWinner\b[^\r\n]*reason=ALL_OBJECTIVES_UNCONTESTED")) { $failures.Add('TERRITORIAL_RESULT') }
    }
}
"Co-op log diagnostics: script_errors=$([regex]::Matches($log,'SCRIPT\s+\([EF]\):').Count) other_errors=$([regex]::Matches($log,'\(E\):').Count)"
if($failures.Count) { $failures | ForEach-Object {"FAIL $_"}; exit 1 }
'Co-op full stopped log: PASS'
exit 0
