param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log') | ForEach-Object {[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=@()
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0'){$failures+='NATIVE_EXIT'}
if(!$log.Contains('Game destroyed.')){$failures+='STOPPED_LOG'}
if(!$log.Contains('[ROSTER_READY]')){$failures+='ROSTER'}
if($all -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation'){$failures+='FULL_LOG_FATAL'}
if($all -match 'Attempting to send an RPC through unregistred item'){$failures+='UNREGISTERED_RPC'}
$manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
if($manifests.Count -ne 1){$failures+='MANIFEST'}
else{
 $manifest=$manifests[0].Groups[1].Value | ConvertFrom-Json
 $argv=@($manifest.arguments)
 foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfFeetProbe')){
  $index=[array]::IndexOf($argv,$flag)
  if($index -lt 0 -or $index+1 -ge $argv.Count -or !$log.Contains($flag+' '+$argv[$index+1])){$failures+='CLI_MISMATCH'}
 }
 $probe=[array]::IndexOf($argv,'-aicfFeetProbe')
 if($probe -lt 0 -or $argv[$probe+1] -ne '1'){$failures+='PROBE_CLI'}
}
$names=@('LOCAL_MODEL','LOCAL_DENIED','LOCAL_NO_DAMAGE_EFFECT','LOCAL_CLEARED','LIVE_REGISTERED','LIVE_ALLOWED','LIVE_BOOTS_PRESENT','LIVE_BOOTS_DETACHED','LIVE_EFFECT_ADDED','LIVE_BOOTS_RESTORED','LIVE_EFFECT_REMOVED')
$cases=@([regex]::Matches($log,'\[FEET_CASE\] case=(\w+) pass=([01])'))
if($cases.Count -ne $names.Count){$failures+='CASE_COUNT'}
foreach($name in $names){
 $found=@($cases | Where-Object {$_.Groups[1].Value -ceq $name})
 if($found.Count -ne 1 -or $found[0].Groups[2].Value -ne '1'){$failures+='CASE_'+$name}
}
$done=@([regex]::Matches($log,'\[FEET_DONE\] checks=(\d+) failed=(\d+)'))
if($done.Count -ne 1 -or $done[0].Groups[1].Value -ne '11' -or $done[0].Groups[2].Value -ne '0'){$failures+='COMPLETION'}
if($failures.Count){$failures;exit 1}
'Feet damage native full stopped log: PASS; cases=11; unregistered_rpc=0'
exit 0
