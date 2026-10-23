param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$base=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$work=Join-Path $base ('aicf-feet-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$argv=@('-gproj','C:\source\addon.gproj','-profile','C:\profile','-addonsDir','C:\source','-addons','CORE,MAP','-aicfFeetProbe','1')
$manifest=@{arguments=$argv} | ConvertTo-Json -Compress
$launcher='AICF_RUNTIME_MANIFEST_JSON='+$manifest
$names=@('LOCAL_MODEL','LOCAL_DENIED','LOCAL_NO_DAMAGE_EFFECT','LOCAL_CLEARED','LIVE_REGISTERED','LIVE_ALLOWED','LIVE_BOOTS_PRESENT','LIVE_BOOTS_DETACHED','LIVE_EFFECT_ADDED','LIVE_BOOTS_RESTORED','LIVE_EFFECT_REMOVED')
$good=(@('CLI Params: '+($argv -join ' '),'[ROSTER_READY]')+@($names | ForEach-Object {"[FEET_CASE] case=$_ pass=1"})+@('[FEET_DONE] checks=11 failed=0','Game destroyed.')) -join "`n"
$count=0
function Check([string]$Name,[string]$Log,[int]$Expected,[string]$Rule){
 [IO.File]::WriteAllText("$work/console.log",$Log)
 [IO.File]::WriteAllText("$work/launcher.txt",$launcher)
 [IO.File]::WriteAllText("$work/exit.txt",'0')
 $output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-FeetDamageLog.ps1') -LogPath "$work/console.log" -LauncherPath "$work/launcher.txt" -NativeExitPath "$work/exit.txt" 2>&1
 if($LASTEXITCODE -ne $Expected -or ($Rule -and ($output -join "`n") -notmatch $Rule)){throw "$Name incorrectly accepted/rejected: $output"}
 $script:count++
}
try{
 Check positive $good 0 ''
 foreach($name in $names){Check $name ($good.Replace("case=$name pass=1","case=$name pass=0")) 1 ('CASE_'+$name)}
 Check missing ($good.Replace('[FEET_CASE] case=LIVE_EFFECT_ADDED pass=1','')) 1 CASE_COUNT
 Check duplicate ($good+"`n[FEET_CASE] case=LIVE_EFFECT_ADDED pass=1") 1 CASE_COUNT
 Check count ($good.Replace('checks=11 failed=0','checks=10 failed=0')) 1 COMPLETION
 Check stopped ($good.Replace('Game destroyed.','')) 1 STOPPED_LOG
 Check roster ($good.Replace('[ROSTER_READY]','')) 1 ROSTER
 Check rpc ($good+"`nRPL (E): Attempting to send an RPC through unregistred item script::Game::SCR_CharacterDamageManagerComponent.") 1 UNREGISTERED_RPC
 [IO.File]::WriteAllText("$work/crash.log",'Assertion failed')
 try{Check crash $good 1 FULL_LOG_FATAL}finally{Remove-Item -LiteralPath "$work/crash.log"}
 Check script ($good+"`nSCRIPT (E): VM failure") 1 FULL_LOG_FATAL
}
finally{
 $target=[IO.Path]::GetFullPath($work)
 if(!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $target) -notlike 'aicf-feet-log-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $target -Recurse -Force
}
"Feet damage log contracts: PASS; cases=$count"
