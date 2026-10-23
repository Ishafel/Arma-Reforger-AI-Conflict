param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[switch]$Network)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$fullLogs=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')|ForEach-Object {[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$launcher=[IO.File]::ReadAllText($LauncherPath)
$lines=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
$failures=@()
if($lines.Count -ne 1){throw 'Canonical manifest missing or duplicated'}
$manifest=$lines[0].Groups[1].Value|ConvertFrom-Json
$argv=@($manifest.arguments)
if((Get-Content $NativeExitPath -Raw).Trim() -ne '0'){$failures+='NATIVE_EXIT'}
if($log -notmatch 'Game destroyed\.'){ $failures+='STOPPED_LOG' }
if($fullLogs -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation'){$failures+='FULL_LOG_FATAL'}
if($fullLogs -match 'Attempting to send an RPC through unregistred item'){$failures+='UNREGISTERED_RPC'}
foreach($flag in @('-gproj','-profile','-addonsDir','-addons')){
 $i=[array]::IndexOf($argv,$flag)
 if($i -lt 0 -or $log -notmatch 'CLI Params:' -or !$log.Contains($flag+' '+$argv[$i+1])){$failures+='CLI_MISMATCH'}
}
$prefix='QUICKSLOT_CASE';$done='QUICKSLOT_DONE';$probe='-aicfQuickslotProbe'
$names=@('DRAFT','SECOND_STORAGE','SVD_AVAILABLE','PRESET_DECODE','FILE_SAVE','FILE_LOAD','PRESET_RELOAD','BOTH_WEAPONS','PRIMARY_QUICKSLOT','SECONDARY_QUICKSLOT','CUSTOM_BINDING','OCCUPIED_BINDING')
if($Network){
 $prefix='QUICKSLOT_NETWORK_CASE';$done='QUICKSLOT_NETWORK_DONE';$probe='-aicfQuickslotNetworkProbe'
 $names=@('RPC_0','RECIPE','RPC_1','SAVED_SELECTED','DEPLOYED_SVD_QUICKSLOT','USE_SECONDARY','SECONDARY_EQUIPPED')
 if($launcher -notmatch '\[RUNTIME_LAUNCHER\]\[SERVER_READY\] process_id=\d+ port=\d+ log='){$failures+='SERVER_REFERENCE'}
 if($log -notmatch '\[QUICKSLOT_NETWORK_STATUS\] SAVED(?:_SESSION)?'){$failures+='PRESET_SAVED'}
}elseif($log -notmatch '\[ROSTER_READY\]'){$failures+='ROSTER'}
$i=[array]::IndexOf($argv,$probe)
if($i -lt 0 -or $argv[$i+1] -ne '1' -or !$log.Contains($probe+' 1')){$failures+='PROBE_CLI'}
$cases=@([regex]::Matches($log,'\['+$prefix+'\] case=(\w+) pass=([01])'))
if($cases.Count -ne $names.Count){$failures+='CASE_COUNT'}
foreach($name in $names){$found=@($cases|Where-Object {$_.Groups[1].Value -ceq $name});if($found.Count -ne 1 -or $found[0].Groups[2].Value -ne '1'){$failures+='CASE_'+$name}}
$completion=@([regex]::Matches($log,'\['+$done+'\] checks=(\d+) failed=(\d+)'))
if($completion.Count -ne 1 -or $completion[0].Groups[1].Value -ne "$($names.Count)" -or $completion[0].Groups[2].Value -ne '0'){$failures+='COMPLETION'}
if($failures.Count){$failures;exit 1}
"Weapon quickslot full stopped log: PASS; network=$Network; cases=$($names.Count)"
exit 0
