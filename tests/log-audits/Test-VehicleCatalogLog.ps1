param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -Filter '*.log' -File)|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=@()
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0'){$failures+='NATIVE_EXIT'}
if(!$log.Contains('Game destroyed.')){$failures+='STOPPED_LOG'}
if(!$log.Contains('[ROSTER_READY]')){$failures+='ROSTER'}
if($all -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Access violation'){$failures+='FULL_LOG_FATAL'}
$manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
if($manifests.Count -ne 1){$failures+='MANIFEST'}
else{
 $argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
 foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehicleCatalogProbe')){
  $index=[array]::IndexOf($argv,$flag)
  if($index -lt 0 -or $index+1 -ge $argv.Count -or !$log.Contains($flag+' '+$argv[$index+1])){$failures+='CLI_MISMATCH'}
 }
 $probe=[array]::IndexOf($argv,'-aicfVehicleCatalogProbe')
 if($probe -lt 0 -or $argv[$probe+1] -ne '1'){$failures+='PROBE_CLI'}
}
$cases=@([regex]::Matches($log,'\[VEHICLE_CATALOG_CHECK\] rule=(\w+) passed=([01])'))
$names=@('NO_PURCHASE_OR_SPAWN')
foreach($faction in @('US','USSR')){
 foreach($rule in @('PARALLEL_COLUMNS','ALL_NATIVE_ENTRIES','BUDGETS_UNCHANGED','STABLE_QUOTES','OWN_SOURCE','CIVILIAN_SOURCE','NATIVE_CATEGORIES','ENEMY_REJECTED','CIVILIAN_INSPECTED','CIVILIAN_RECIPIENT')){$names+=$faction+'_'+$rule}
}
foreach($name in $names){
 $found=@($cases|Where-Object{$_.Groups[1].Value -ceq $name})
 if($found.Count -ne 1){$failures+='MISSING_OR_DUPLICATE_'+$name}
 elseif($found[0].Groups[2].Value -ne '1'){$failures+='FAILED_'+$name}
}
foreach($case in $cases){if($case.Groups[2].Value -ne '1'){$failures+='FAILED_CASE'}}
$done=@([regex]::Matches($log,'\[VEHICLE_CATALOG_DONE\] checks=(\d+) failures=(\d+)'))
if($done.Count -ne 1 -or $done[0].Groups[1].Value -ne $cases.Count.ToString() -or $done[0].Groups[2].Value -ne '0'){$failures+='COMPLETION'}
foreach($faction in @('US','USSR')){
 $totals=@([regex]::Matches($log,'\[VEHICLE_CATALOG_TOTAL\] faction='+$faction+'_ own=(\d+) civilian=(\d+) total=(\d+) cars=(\d+) trucks=(\d+) armor=(\d+)'))
 if($totals.Count -ne 1){$failures+='TOTAL_'+$faction;continue}
 $total=$totals[0]
 if([int]$total.Groups[1].Value -le 0 -or [int]$total.Groups[2].Value -le 0 -or [int]$total.Groups[1].Value+[int]$total.Groups[2].Value -ne [int]$total.Groups[3].Value -or [int]$total.Groups[4].Value -le 0 -or [int]$total.Groups[5].Value -le 0){$failures+='INVALID_TOTAL_'+$faction}
}
if($log -match '\[VEHICLE_PRESET_PURCHASE\]'){$failures+='UNEXPECTED_PURCHASE'}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
"PASS Vehicle native catalog checks=$($cases.Count) factions=2 source=own+civilian stopped=1"
exit 0
