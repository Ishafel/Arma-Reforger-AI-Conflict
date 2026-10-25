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
function Check([string]$Name,[string]$Server,[string]$Client,[string]$JIP,[int]$Expected,[string]$Rule,[switch]$Filters){
 $options=@{}
 foreach($label in @('Server','Client','JIP')){
  $directory=Join-Path $work $label
  [void][IO.Directory]::CreateDirectory($directory)
  $text=$Server
  if($label -eq 'Client'){$text=$Client}
  if($label -eq 'JIP'){$text=$JIP}
  [IO.File]::WriteAllText("$directory/console.log",$text)
  $currentLauncher=$launcher
  if($Filters){$currentLauncher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv+@('-aicfVehicleCatalogFilters','1')}|ConvertTo-Json -Compress)+"`n[SERVER_READY]"}
  [IO.File]::WriteAllText("$directory/launcher.txt",$currentLauncher)
  [IO.File]::WriteAllText("$directory/exit.txt",'0')
  $options[$label+'Log']="$directory/console.log"
  $options[$label+'Launcher']="$directory/launcher.txt"
  $options[$label+'Exit']="$directory/exit.txt"
 }
 if($Filters){$options.CatalogFilters=$true}
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
 $filterNames=@('ALL_NATIVE_CATALOG','CIVILIAN_FILTER','CAR_CATEGORY_FILTER','FACTION_FILTER','EMPTY_FILTER_SAFE','EMPTY_SELECT_NO_RPC','FILTERED_PREFAB_MAPPING','LOAD_REVEALS_DRAFT')
 $filterClient=$client.Replace('[ROSTER_READY]','-aicfVehicleCatalogFilters 1'+"`n[ROSTER_READY]").Replace('checks=20','checks=28')+"`n"+(@($filterNames|ForEach-Object{"[VEHICLE_UI_CHECK] rule=$_ passed=1"}) -join "`n")
 $filterJIP=$jip.Replace('[ROSTER_READY]','-aicfVehicleCatalogFilters 1'+"`n[ROSTER_READY]").Replace('checks=3','checks=5')+"`n[VEHICLE_UI_CHECK] rule=JIP_CIVILIAN_AVAILABLE passed=1`n[VEHICLE_UI_CHECK] rule=JIP_FILTERED_SELECTION passed=1"
 $filterServer=$server.Replace('[ROSTER_READY]','-aicfVehicleCatalogFilters 1'+"`n[ROSTER_READY]")
 Check 'catalog-filters' $filterServer $filterClient $filterJIP 0 '' -Filters
 Check 'wrong-filter-mapping' $filterServer ($filterClient.Replace('rule=FILTERED_PREFAB_MAPPING passed=1','rule=FILTERED_PREFAB_MAPPING passed=0')) $filterJIP 1 'FILTER_CASE_FILTERED_PREFAB_MAPPING' -Filters
 Check 'missing-filter-load' $filterServer ($filterClient.Replace('rule=LOAD_REVEALS_DRAFT','rule=REMOVED')) $filterJIP 1 'FILTER_CASE_LOAD_REVEALS_DRAFT' -Filters
 Check 'filter-shutdown-leaks' ($filterServer+"`nSCRIPT (E): Leaked script instance") $filterClient $filterJIP 1 'SERVER_FULL_FATAL' -Filters
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe test cleanup path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
Write-Output "PASS Vehicle preset client/JIP log contracts controls=$count"
