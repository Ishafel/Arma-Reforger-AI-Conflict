param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$work=Join-Path $tempBase ('aicf-coop-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$audit=Join-Path $RepositoryRoot 'tests/log-audits/Test-CoopScenarioLog.ps1'
$names=@('US_ROSTER','USSR_ROSTER','QUEUE_SETTLED','US_TICKETS','USSR_TICKETS','COMMANDERS','HELPERS','BUDGET_FLOOR','STANDARD_POLICY','MIRROR_POLICY','STABLE_ROSTER','REPLICATED_MODE','ZERO_PLAYER_TICKETS_NO_DEFEAT','CONFLICT_REJECTED','NO_GROUP_SUMMARY')
$positive=(@('[AICF][ROSTER_READY] us_groups=10 ussr_groups=0')+@($names | ForEach-Object {"[AICF][COOP_CASE] case=$_ pass=1"})+@('[AICF][COOP_DONE] mode=USSR checks=15 failed=0 elapsed_ms=30000','ENGINE : Game destroyed.')) -join "`n"
$manifest='AICF_RUNTIME_MANIFEST_JSON='+(@{role='Server';variant='StockCoopUSSR';arguments=@('-aicfCoopProbe','USSR')} | ConvertTo-Json -Compress)
function Check([string]$Name,[string]$Log,[string]$Launch,[int]$Expected,[string]$Rule,[switch]$Client,[switch]$RequirePlayer) {
    [IO.File]::WriteAllText("$work/log.txt",$Log)
    [IO.File]::WriteAllText("$work/launcher.txt",$Launch)
    $invoke=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$audit,'-LogPath',"$work/log.txt",'-LauncherPath',"$work/launcher.txt",'-NativeExitPath',"$work/exit.txt")
    if($Client){$invoke+='-Client'}
    if($RequirePlayer){$invoke+='-RequirePlayer'}
    $output=& powershell.exe @invoke 2>&1
    if($LASTEXITCODE -ne $Expected -or ($Rule -and ($output -join "`n") -notmatch $Rule)) {throw "Unexpected case ${Name}: $output"}
    "PASS $Name"
}
try {
    '0' | Set-Content "$work/exit.txt"
    Check 'positive' $positive $manifest 0 ''
    foreach($case in @(
        @('no-shutdown',$positive.Replace('Game destroyed.','running'),'STOPPED_LOG_REQUIRED'),
        @('script-error',($positive+"`nSCRIPT (E): error"),'SCRIPT_FATAL'),
        @('missing-case',$positive.Replace('case=HELPERS','case=OTHER'),'CASE_HELPERS'),
        @('duplicate-case',($positive+"`n[AICF][COOP_CASE] case=HELPERS pass=1"),'CASE_HELPERS'),
        @('failed-case',$positive.Replace('case=US_TICKETS pass=1','case=US_TICKETS pass=0'),'PROBE_FAILURE'),
        @('short-observation',$positive.Replace('elapsed_ms=30000','elapsed_ms=29999'),'COMPLETION'),
        @('wrong-mode',$positive.Replace('mode=USSR','mode=STANDARD'),'COMPLETION'),
        @('wrong-roster',$positive.Replace('ussr_groups=0','ussr_groups=10'),'ROSTER'),
        @('player-spawn',($positive+"`n[AICF][SPAWN_ATTEMPT_STARTED] faction=USSR slot=0"),'PLAYER_COMBAT_SPAWN'),
        @('timeout',($positive+"`n[AICF][COOP_TIMEOUT]"),'PROBE_FAILURE')
    )) { Check $case[0] $case[1] $manifest 1 $case[2] }
    Check 'missing-manifest' $positive '' 1 'MANIFEST'
    Check 'wrong-probe' $positive ($manifest.Replace('"USSR"','"STANDARD"')) 1 'PROBE_MODE'
    $clientLog="[AICF][COOP_CLIENT_DONE] pass=1 samples=15`nENGINE : Game destroyed."
    $clientManifest='AICF_RUNTIME_MANIFEST_JSON='+(@{role='Client';variant='StockCoopUSSR';arguments=@('-aicfCoopProbe','USSR')} | ConvertTo-Json -Compress)
    $clientLaunch=$clientManifest+"`n[AICF][RUNTIME_LAUNCHER][SERVER_READY] process_id=123 port=2411 log=server.log"
    $playerManifest='AICF_RUNTIME_MANIFEST_JSON='+(@{role='Client';variant='StockCoopUSSR';arguments=@('-aicfCoopProbe','USSR','-aicfCoopProbePlayer','1')} | ConvertTo-Json -Compress)
    $playerLaunch=$playerManifest+"`n[AICF][RUNTIME_LAUNCHER][SERVER_READY] process_id=123 port=2411 log=server.log"
    $playerLog=$clientLog+"`n[AICF][COOP_CLIENT_PLAYER] pass=1 entity=123"
    Check 'client-positive' $clientLog $clientLaunch 0 '' -Client
    Check 'client-player-positive' $playerLog $playerLaunch 0 '' -Client -RequirePlayer
    Check 'client-missing-player' $clientLog $playerLaunch 1 'PLAYER_SPAWN' -Client -RequirePlayer
    Check 'client-missing-player-mode' $playerLog $clientLaunch 1 'PLAYER_MODE' -Client -RequirePlayer
    Check 'client-wrong-player-mode' $playerLog ($playerLaunch.Replace('"1"','"0"')) 1 'PLAYER_MODE' -Client -RequirePlayer
    Check 'server-player-mode' $positive $manifest 1 'PLAYER_ROLE' -RequirePlayer
    Check 'client-no-readiness' $clientLog $clientManifest 1 'CLIENT_READINESS' -Client
    '1' | Set-Content "$work/exit.txt"
    Check 'native-exit' $positive $manifest 1 'NATIVE_EXIT'
}
finally {
    $resolved=[IO.Path]::GetFullPath($work)
    if(!$resolved.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-coop-log-*') {throw 'Unsafe cleanup path'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
'Co-op log contracts: PASS; positive=3 negative=18'
