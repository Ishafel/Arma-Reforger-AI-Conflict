param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-instigator-'+[guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $work)
$baseCases=@('US_ROSTER','USSR_ROSTER','QUEUE_SETTLED','US_TICKETS','USSR_TICKETS','COMMANDERS','HELPERS','BUDGET_FLOOR','STANDARD_POLICY','MIRROR_POLICY','STABLE_ROSTER','REPLICATED_MODE','FACTION_ACCESS','AUTO_CONSTRUCTION','CONTENT_FACTIONS','HEADER_MODE')
$cases=@('REAL_OTHER_TYPE','OTHER_RELATION','OTHER_METADATA','OTHER_CONTROL','NATIVE_FRIENDLY_AI','REUSED_CONTEXT','OTHER_CHARACTER_OVERRIDE','NATIVE_SUICIDE','NATIVE_GM','NATIVE_NONE','NATIVE_DELETED','NATIVE_NON_CHARACTER','NATIVE_NULL_VICTIM','NATIVE_DEATH_EVENT','NATIVE_DEATH_RELATION','REAL_CHARACTER_DIED','DEATH_COMPLETED','NATIVE_ENEMY_AI')
$log="[ROSTER_READY] us_groups=10 ussr_groups=10`n[COOP_CONTEXT] mode=STANDARD us_faction=US ussr_faction=USSR nodes=5`n"+(@($baseCases|ForEach-Object {"[COOP_CASE] case=$_ pass=1"}) -join "`n")+"`n[COOP_DONE] mode=STANDARD checks=16 failed=0 elapsed_ms=35000`n"+(@($cases|ForEach-Object {"[INSTIGATOR_CASE] case=$_ pass=1"}) -join "`n")+"`n[INSTIGATOR_DONE] checks=18 failed=0`nGame destroyed."
$launch='AICF_RUNTIME_MANIFEST_JSON='+(@{role='Server';variant='Stock';matchMode='Standard';difficulty='Easy';arguments=@('-aicfCoopProbe','STANDARD','-aicfInstigatorProbe','1')}|ConvertTo-Json -Compress)
$count=0
function Check([string]$Name,[string]$Log,[string]$Launch,[int]$Expected,[string]$Rule,[switch]$Player){
    [IO.File]::WriteAllText("$work/log.txt",$Log)
    [IO.File]::WriteAllText("$work/launcher.txt",$Launch)
    $argsAudit=@('-NoProfile','-ExecutionPolicy','Bypass','-File',"$RepositoryRoot/tests/log-audits/Test-InstigatorContextLog.ps1",'-LogPath',"$work/log.txt",'-LauncherPath',"$work/launcher.txt",'-NativeExitPath',"$work/exit.txt")
    if($Player){$argsAudit+='-Player'}
    $output=& powershell.exe @argsAudit 2>&1
    if($LASTEXITCODE -ne $Expected -or ($Rule -and ($output -join "`n") -notmatch $Rule)){throw "Unexpected $Name : $output"}
    $script:count++
}
try{
    '0'|Set-Content "$work/exit.txt"
    Check 'positive' $log $launch 0 ''
    Check 'missing-case' ($log.Replace('case=REAL_OTHER_TYPE','case=OTHER')) $launch 1 'CASE_REAL_OTHER_TYPE'
    Check 'failed-relation' ($log.Replace('case=OTHER_RELATION pass=1','case=OTHER_RELATION pass=0')) $launch 1 'FAILED_CASE'
    Check 'duplicate-death' ($log+"`n[INSTIGATOR_CASE] case=NATIVE_DEATH_EVENT pass=1") $launch 1 'CASE_NATIVE_DEATH_EVENT'
    Check 'failed-completion' ($log.Replace('[INSTIGATOR_DONE] checks=18 failed=0','[INSTIGATOR_DONE] checks=18 failed=1')) $launch 1 'COMPLETION'
    Check 'missing-completion' ($log.Replace('[INSTIGATOR_DONE]','[OTHER_DONE]')) $launch 1 'COMPLETION'
    Check 'wrong-count' ($log.Replace('checks=18','checks=17')) $launch 1 'COMPLETION'
    Check 'native-script-error' ($log+"`nSCRIPT (E): SCR_InstigatorContextData: INSTIGATOR_OTHER is not supported!") $launch 1 'BASE_RUNTIME'
    Check 'live-log' ($log.Replace('Game destroyed.','running')) $launch 1 'BASE_RUNTIME'
    Check 'missing-cli' $log ($launch.Replace('-aicfInstigatorProbe','-unknown')) 1 'PROBE_ARGUMENT'
    Check 'missing-player-cases' $log $launch 1 'CASE_NATIVE_PLAYER' -Player
    $playerCases=$baseCases+@('ZERO_PLAYER_TICKETS_NO_DEFEAT','CONFLICT_REJECTED','NO_GROUP_SUMMARY')
    $playerLog="[ROSTER_READY] us_groups=0 ussr_groups=10`n[COOP_CONTEXT] mode=US us_faction=US ussr_faction=USSR nodes=5`n"+(@($playerCases|ForEach-Object {"[COOP_CASE] case=$_ pass=1"}) -join "`n")+"`n[COOP_DONE] mode=US checks=19 failed=0 elapsed_ms=35000`n"+(@(($cases|Where-Object {$_ -ne 'NATIVE_ENEMY_AI'})+@('NATIVE_PLAYER','OTHER_PLAYER_OVERRIDE','OTHER_PLAYER_VICTIM')|ForEach-Object {"[INSTIGATOR_CASE] case=$_ pass=1"}) -join "`n")+"`n[INSTIGATOR_DONE] checks=20 failed=0`nGame destroyed."
    $playerLaunch='AICF_RUNTIME_MANIFEST_JSON='+(@{role='Server';variant='Stock';matchMode='CoopUS';difficulty='Easy';arguments=@('-aicfCoopProbe','US','-aicfInstigatorProbe','1','-aicfInstigatorPlayer','1')}|ConvertTo-Json -Compress)
    Check 'player-positive' $playerLog $playerLaunch 0 '' -Player
    Check 'player-missing-argument' $playerLog ($playerLaunch.Replace('-aicfInstigatorPlayer','-unknown')) 1 'PLAYER_ARGUMENT' -Player
    '1'|Set-Content "$work/exit.txt"
    Check 'native-exit' $log $launch 1 'BASE_RUNTIME'
}
finally{
    $target=[IO.Path]::GetFullPath($work)
    $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if(!$target.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $target) -notlike 'aicf-instigator-*'){throw 'Unsafe cleanup'}
    Remove-Item -LiteralPath $target -Recurse -Force
}
"Instigator log contracts: PASS; cases=$count"
