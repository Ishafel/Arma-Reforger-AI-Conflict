param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-vehicle-catalog-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','C:\source\addon.gproj','-profile','C:\profile','-addonsDir','C:\source','-addons','CORE,MAP','-aicfVehicleCatalogProbe','1')
$launcher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)
$names=@('NO_PURCHASE_OR_SPAWN')
foreach($faction in @('US','USSR')){foreach($rule in @('PARALLEL_COLUMNS','ALL_NATIVE_ENTRIES','BUDGETS_UNCHANGED','STABLE_QUOTES','OWN_SOURCE','CIVILIAN_SOURCE','NATIVE_CATEGORIES','ENEMY_REJECTED','CIVILIAN_INSPECTED','CIVILIAN_RECIPIENT')){$names+=$faction+'_'+$rule}}
$good=(@('CLI Params: '+($argv -join ' '),'[ROSTER_READY]')+@($names|ForEach-Object{"[VEHICLE_CATALOG_CHECK] rule=$_ passed=1"})+@('[VEHICLE_CATALOG_TOTAL] faction=US_ own=10 civilian=5 total=15 cars=9 trucks=4 armor=2','[VEHICLE_CATALOG_TOTAL] faction=USSR_ own=8 civilian=5 total=13 cars=7 trucks=4 armor=2','[VEHICLE_CATALOG_DONE] checks=21 failures=0','Game destroyed.')) -join "`n"
$count=0
function Check([string]$Name,[string]$Log,[int]$Expected,[string]$Rule,[string]$Exit='0'){
 [IO.File]::WriteAllText("$work/console.log",$Log)
 [IO.File]::WriteAllText("$work/launcher.txt",$launcher)
 [IO.File]::WriteAllText("$work/exit.txt",$Exit)
 $output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleCatalogLog.ps1') -LogPath "$work/console.log" -LauncherPath "$work/launcher.txt" -NativeExitPath "$work/exit.txt" 2>&1
 if($LASTEXITCODE -ne $Expected -or ($Rule -and ($output -join "`n") -notmatch $Rule)){throw "$Name unexpected verdict: $output"}
 $script:count++
}
try{
 Check 'complete' $good 0 ''
 Check 'missing-civilian' ($good.Replace('rule=US_CIVILIAN_SOURCE','rule=REMOVED')) 1 'MISSING_OR_DUPLICATE_US_CIVILIAN_SOURCE'
 Check 'failed-price' ($good.Replace('rule=US_STABLE_QUOTES passed=1','rule=US_STABLE_QUOTES passed=0')) 1 'FAILED_US_STABLE_QUOTES'
 Check 'duplicate-case' ($good+"`n[VEHICLE_CATALOG_CHECK] rule=US_OWN_SOURCE passed=1") 1 'MISSING_OR_DUPLICATE_US_OWN_SOURCE'
 Check 'enemy-leak' ($good.Replace('rule=US_ENEMY_REJECTED passed=1','rule=US_ENEMY_REJECTED passed=0')) 1 'FAILED_US_ENEMY_REJECTED'
 Check 'changed-budget' ($good.Replace('rule=US_BUDGETS_UNCHANGED passed=1','rule=US_BUDGETS_UNCHANGED passed=0')) 1 'FAILED_US_BUDGETS_UNCHANGED'
 Check 'bad-total' ($good.Replace('own=10 civilian=5 total=15','own=10 civilian=5 total=16')) 1 'INVALID_TOTAL_US'
 Check 'missing-total' ($good.Replace('faction=US_ own','faction=OTHER_ own')) 1 'TOTAL_US'
 Check 'unfinished' ($good.Replace('Game destroyed.','')) 1 'STOPPED_LOG'
 Check 'wrong-cli' ($good.Replace('-aicfVehicleCatalogProbe 1','-aicfVehicleCatalogProbe 0')) 1 'CLI_MISMATCH'
 Check 'native-failure' $good 1 'NATIVE_EXIT' '-1'
 Check 'script-error' ($good+"`nSCRIPT (E): NULL pointer") 1 'FULL_LOG_FATAL'
 Check 'side-effect' ($good+"`n[VEHICLE_PRESET_PURCHASE] operation=unexpected") 1 'UNEXPECTED_PURCHASE'
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Vehicle catalog log controls=$count"
