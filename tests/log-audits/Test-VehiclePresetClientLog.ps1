param([Parameter(Mandatory)][string]$ServerLog,[Parameter(Mandatory)][string]$ServerLauncher,[Parameter(Mandatory)][string]$ServerExit,
 [Parameter(Mandatory)][string]$ClientLog,[Parameter(Mandatory)][string]$ClientLauncher,[Parameter(Mandatory)][string]$ClientExit,
 [Parameter(Mandatory)][string]$JIPLog,[Parameter(Mandatory)][string]$JIPLauncher,[Parameter(Mandatory)][string]$JIPExit)
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
function Read-Native([string]$Label,[string]$LogPath,[string]$LauncherPath,[string]$ExitPath,[bool]$Client){
 $log=[IO.File]::ReadAllText($LogPath)
 $all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -Filter '*.log' -File)|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
 $launcher=[IO.File]::ReadAllText($LauncherPath)
 if((Get-Content -LiteralPath $ExitPath -Raw).Trim() -ne '0'){$failures.Add($Label+'_NATIVE_EXIT')}
 if(!$log.Contains('Game destroyed.')){$failures.Add($Label+'_STOPPED')}
 if($all -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation'){$failures.Add($Label+'_FULL_FATAL')}
 $manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
 if($manifests.Count -ne 1){$failures.Add($Label+'_MANIFEST')}
 else{
  $manifest=$manifests[0].Groups[1].Value|ConvertFrom-Json
  $argv=@($manifest.arguments)
  foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehiclePresetClient')){
   $index=[array]::IndexOf($argv,$flag)
   if($index -lt 0 -or $index+1 -ge $argv.Count -or !$log.Contains($flag+' '+$argv[$index+1])){$failures.Add($Label+'_CLI')}
  }
 }
 if($Client -and !$launcher.Contains('[SERVER_READY]')){$failures.Add($Label+'_READINESS')}
 return $log
}
$server=Read-Native 'SERVER' $ServerLog $ServerLauncher $ServerExit $false
$client=Read-Native 'CLIENT' $ClientLog $ClientLauncher $ClientExit $true
$jip=Read-Native 'JIP' $JIPLog $JIPLauncher $JIPExit $true
if(!$server.Contains('[ROSTER_READY]')){$failures.Add('SERVER_ROSTER')}
foreach($case in @('NO_FACTION_REJECTED','ALPHA_VEHICLE_FORM','CONTEXT','UAZ_WHITELIST','SELECT_HANDLER','NATIVE_SEAT_LABELS','DRIVER_SEAT','ASSIGN_HANDLER','SAVE_HANDLER','APPLY_HANDLER','REPEAT_APPLY_IDEMPOTENT','STALE_REVISION_REJECTED','FOREIGN_FACTION_REJECTED','LOAD_HANDLER','DELETE_HANDLER')){
 $found=@([regex]::Matches($client,'\[VEHICLE_UI_CHECK\] rule='+$case+' passed=1'))
 if($found.Count -ne 1){$failures.Add('CLIENT_CASE_'+$case)}
}
foreach($case in @('ALPHA_VEHICLE_FORM','JIP_CONFIRMED_ASSIGNMENT','JIP_STABLE_SLOT')){
 if(@([regex]::Matches($jip,'\[VEHICLE_UI_CHECK\] rule='+$case+' passed=1')).Count -ne 1){$failures.Add('JIP_CASE_'+$case)}
}
if(($client+"`n"+$jip) -match '\[VEHICLE_UI_CHECK\].*passed=0|VEHICLE_UI_TIMEOUT'){$failures.Add('FAILED_CASE')}
if(@([regex]::Matches($client,'\[VEHICLE_UI_DONE\] checks=20 failures=0 jip=0')).Count -ne 1){$failures.Add('CLIENT_COMPLETION')}
if(@([regex]::Matches($jip,'\[VEHICLE_UI_DONE\] checks=3 failures=0 jip=1')).Count -ne 1){$failures.Add('JIP_COMPLETION')}
foreach($late in @('0','1')){if(@([regex]::Matches($server,'\[VEHICLE_UI_SERVER_READBACK\] player=\d+ failures=0 jip='+$late)).Count -ne 1){$failures.Add('SERVER_READBACK_'+$late)}}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
'PASS Vehicle preset UI/RPC client=20 JIP=3 server_readback=2 native=0/0/0 stopped=3'
exit 0
