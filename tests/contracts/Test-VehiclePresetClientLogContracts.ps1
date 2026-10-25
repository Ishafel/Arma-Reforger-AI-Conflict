param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-vehicle-ui-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work|Out-Null
$argv=@('-gproj','C:\source\addon.gproj','-profile','C:\profile','-addonsDir','C:\source','-addons','CORE,MAP','-aicfVehiclePresetClient','1')
$header='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"
$launcher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)+"`n[SERVER_READY]"
$names=@('NO_FACTION_REJECTED','ALPHA_VEHICLE_FORM','CONTEXT','UAZ_WHITELIST','SELECT_HANDLER','NATIVE_SEAT_LABELS','DRIVER_SEAT','ASSIGN_HANDLER','SAVE_HANDLER','APPLY_HANDLER','REPEAT_APPLY_IDEMPOTENT','STALE_REVISION_REJECTED','FOREIGN_FACTION_REJECTED','LOAD_HANDLER','DELETE_HANDLER')
for($i=0;$i -lt 5;$i++){$names+='NO_FAKE_TURRET_'+$i}
$client=$header+(@($names|ForEach-Object{"[VEHICLE_UI_CHECK] rule=$_ passed=1"}) -join "`n")+"`n[VEHICLE_UI_DONE] checks=20 failures=0 jip=0`nGame destroyed."
$jip=$header+(@(@('ALPHA_VEHICLE_FORM','JIP_CONFIRMED_ASSIGNMENT','JIP_STABLE_SLOT')|ForEach-Object{"[VEHICLE_UI_CHECK] rule=$_ passed=1"}) -join "`n")+"`n[VEHICLE_UI_DONE] checks=3 failures=0 jip=1`nGame destroyed."
$server=$header+"[VEHICLE_UI_SERVER_READBACK] player=1 failures=0 jip=0`n[VEHICLE_UI_SERVER_READBACK] player=2 failures=0 jip=1`nGame destroyed."
$count=0
function Check([string]$Name,[string]$Server,[string]$Client,[string]$JIP,[int]$Expected,[string]$Rule){
 $options=@{}
 foreach($label in @('Server','Client','JIP')){
  $directory=Join-Path $work $label
  [void][IO.Directory]::CreateDirectory($directory)
  $text=$Server
  if($label -eq 'Client'){$text=$Client}
  if($label -eq 'JIP'){$text=$JIP}
  [IO.File]::WriteAllText("$directory/console.log",$text)
  [IO.File]::WriteAllText("$directory/launcher.txt",$launcher)
  [IO.File]::WriteAllText("$directory/exit.txt",'0')
  $options[$label+'Log']="$directory/console.log"
  $options[$label+'Launcher']="$directory/launcher.txt"
  $options[$label+'Exit']="$directory/exit.txt"
 }
 $output=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehiclePresetClientLog.ps1') @options
 $code=$LASTEXITCODE
 if($code -ne $Expected -or ($Rule -and ($output -join "`n") -notmatch $Rule)){throw "$Name unexpected verdict: $output"}
 $script:count++
}
try{
 Check 'complete' $server $client $jip 0 ''
 Check 'wrong-jip' $server $client ($jip.Replace('rule=JIP_CONFIRMED_ASSIGNMENT passed=1','rule=JIP_CONFIRMED_ASSIGNMENT passed=0')) 1 'JIP_CASE_JIP_CONFIRMED_ASSIGNMENT'
 Check 'missing-server-owner' ($server.Replace('player=2 failures=0 jip=1','player=2 failures=0 jip=0')) $client $jip 1 'SERVER_READBACK_1'
 Check 'stale-accepted' $server ($client.Replace('rule=STALE_REVISION_REJECTED passed=1','rule=STALE_REVISION_REJECTED passed=0')) $jip 1 'CLIENT_CASE_STALE_REVISION_REJECTED'
 Check 'duplicate-case' $server ($client+"`n[VEHICLE_UI_CHECK] rule=APPLY_HANDLER passed=1") $jip 1 'CLIENT_CASE_APPLY_HANDLER'
 Check 'unfinished-jip' $server $client ($jip.Replace('Game destroyed.','')) 1 'JIP_STOPPED'
 Check 'shutdown-leaks' ($server+"`nSCRIPT (E): Leaked script instance") $client $jip 1 'SERVER_FULL_FATAL'
 Check 'wrong-cli' $server ($client.Replace('-aicfVehiclePresetClient 1','-aicfVehiclePresetClient 0')) $jip 1 'CLIENT_CLI'
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe test cleanup path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
Write-Output "PASS Vehicle preset client/JIP log contracts controls=$count"
