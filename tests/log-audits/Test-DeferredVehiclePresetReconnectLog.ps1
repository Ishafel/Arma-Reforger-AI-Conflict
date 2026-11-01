param(
 [Parameter(Mandatory)][string]$ServerLog,[Parameter(Mandatory)][string]$ClientLog,[Parameter(Mandatory)][string]$JIPLog,
 [Parameter(Mandatory)][string]$ServerLauncher,[Parameter(Mandatory)][string]$ClientLauncher,[Parameter(Mandatory)][string]$JIPLauncher,
 [Parameter(Mandatory)][string]$ServerExit,[Parameter(Mandatory)][string]$ClientExit,[Parameter(Mandatory)][string]$JIPExit
)
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
$errors=[ordered]@{}
function Native([string]$name,[string]$path,[string]$launcherPath,[string]$exitPath){
 $log=[IO.File]::ReadAllText($path)
 $siblings=(@(Get-ChildItem (Split-Path $path -Parent) -File -Filter '*.log')|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
 if((Get-Content $exitPath -Raw).Trim() -ne '0' -or !$log.Contains('Game destroyed.')){$failures.Add($name+'_STOPPED_NATIVE_EXIT')}
 if($siblings -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|MEMORY\s+\([EF]\)|Virtual Machine Exception|Access violation|Out of memory|Assertion failed'){$failures.Add($name+'_FULL_FATAL')}
 $errors[$name]=@([regex]::Matches($siblings,'(?m)^.*\(E\).*$')).Count
 $launcher=[IO.File]::ReadAllText($launcherPath)
 $manifest=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
 if($manifest.Count -ne 1){$failures.Add($name+'_MANIFEST')}
 else{
  $argv=@(($manifest[0].Groups[1].Value|ConvertFrom-Json).arguments)
  foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfDeferredVehiclePresetProbe','-aicfDeferredVehiclePresetSequential')){
   $index=[array]::IndexOf($argv,$flag)
   if($index -lt 0 -or $index+1 -ge $argv.Count -or !$log.Contains($flag+' '+$argv[$index+1])){$failures.Add($name+'_CLI')}
  }
 }
 if($name -ne 'SERVER' -and (!$launcher.Contains('[SERVER_READY]') -or !$log.Contains('[DEFERRED_NATIVE_PLAYER] stock_respawn=1 managed_possession=0'))){$failures.Add($name+'_READINESS_OR_NATIVE_PLAYER')}
 return $log
}
$server=Native SERVER $ServerLog $ServerLauncher $ServerExit
$client=Native CLIENT $ClientLog $ClientLauncher $ClientExit
$jip=Native JIP $JIPLog $JIPLauncher $JIPExit
foreach($rule in @('BOUND_BEFORE_MUSTER_COMPLETE','NO_VEHICLE_BEFORE_MUSTER','BINDING_SURVIVED_RECONNECT')){
 if(@([regex]::Matches($server,'\[DEFERRED_CHECK\] rule='+$rule+' passed=1')).Count -ne 1){$failures.Add($rule)}
}
foreach($rule in @('PARTIAL_ROSTER_FUTURE_POSITIONS','CATALOG_NATIVE_UAZ','SELECT_NATIVE_TOPOLOGY','CONFIRM_BEFORE_RECRUITMENT','BOUND_WHILE_RECRUITING')){
 if(@([regex]::Matches($client,'\[DEFERRED_CLIENT_CHECK\] rule='+$rule+' passed=1')).Count -ne 1){$failures.Add('CLIENT_'+$rule)}
}
if(!$server.Contains('[DEFERRED_JIP_SERVER_READ] owner_context=1 same_binding=1 first_disconnected=1') -or !$server.Contains('[DEFERRED_SEQUENTIAL_DONE] checks=3 failures=0')){$failures.Add('SERVER_JIP_PROOF')}
if(!$client.Contains('[DEFERRED_CLIENT_DONE] checks=5 failures=0')){$failures.Add('FIRST_CLIENT_COMPLETION')}
if(!$jip.Contains('[DEFERRED_JIP_CHECK] assigned_readback=1 passed=1') -or !$jip.Contains('[DEFERRED_JIP_DONE] checks=1 failures=0')){$failures.Add('JIP_READBACK')}
$all=$server+"`n"+$client+"`n"+$jip
if($all -match '\[DEFERRED_(CLIENT_)?CHECK\].*passed=0|\[DEFERRED_JIP_CHECK\].*passed=0'){$failures.Add('FAILED_CASE')}
Write-Output ('FULL_RAW_ERROR_COUNTS='+($errors|ConvertTo-Json -Compress))
if(($errors.Values|Measure-Object -Sum).Sum -gt 0){Write-Output 'STRICT_FULL_ERROR_GATE=FAIL'}else{Write-Output 'STRICT_FULL_ERROR_GATE=PASS'}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
'PASS Deferred reconnect: real binding before recruitment, native primary close, native JIP owner READ, same binding, native0/0/0 stopped3'
exit 0
