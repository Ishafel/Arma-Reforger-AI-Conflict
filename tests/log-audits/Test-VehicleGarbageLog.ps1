param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$LauncherPath,
 [Parameter(Mandatory)][string]$NativeExitPath
)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=[Collections.Generic.List[string]]::new()
function Require([bool]$condition,[string]$rule){if(!$condition){$failures.Add($rule)}}
Require (([IO.File]::ReadAllText($NativeExitPath)).Trim() -eq '0') 'NATIVE_EXIT'
Require ($log.Contains('Game destroyed.') -and $log.Contains('[ROSTER_READY]')) 'FULL_STOPPED_ROSTER'
Require ($all -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation') 'FULL_LOG_FATAL'
$manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
Require ($manifests.Count -eq 1) 'MANIFEST'
if($manifests.Count -eq 1){
 try{
  $argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
  $cli=@([regex]::Matches($log,'(?m)CLI Params:\s*([^\r\n]+)'))
  Require ($argv.Count -gt 0 -and $cli.Count -eq 1 -and $cli[0].Groups[1].Value.Trim() -ceq ($argv -join ' ')) 'EXACT_CLI'
  foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehicleGarbageProbe')){
   $indices=@(for($i=0;$i -lt $argv.Count;$i++){if($argv[$i] -eq $flag){$i}})
   Require ($indices.Count -eq 1 -and $indices[0]+1 -lt $argv.Count -and ![string]::IsNullOrWhiteSpace($argv[$indices[0]+1])) ('CLI_'+$flag)
  }
  $probeIndex=[array]::IndexOf($argv,'-aicfVehicleGarbageProbe')
  Require ($probeIndex -ge 0 -and $probeIndex+1 -lt $argv.Count -and $argv[$probeIndex+1] -eq '1') 'PROBE_VALUE'
 }catch{$failures.Add('MALFORMED_MANIFEST')}
}
$expected=@('REAL_FLEET_ACTIVE_BIND','PRELEASE_NATIVE_TTL_FIVE','OWNED_DELETE_VETO','OWNED_SURVIVES_NATIVE_TTL','OWNED_TRACKING_ENDED','OWNED_INSERT_VETO','RELEASE_PENDING_PROTECTED','FAILED_CLOSED_PROTECTED','STALE_GROUP_GENERATION_REJECTED','STALE_TRIP_GENERATION_REJECTED','STALE_LEASE_GENERATION_REJECTED','STALE_VEHICLE_GENERATION_REJECTED','FAKE_ENTITY_REJECTED','LOCAL_NATIVE_AUTHORITY_READBACK','RELEASE_TO_WORLD_POOL','RELEASED_NATIVE_TRACKING_RESTORED','RELEASED_NO_PERMISSION','RELEASED_NATIVE_TTL_DELETE','UNOWNED_NATIVE_TTL_DELETE','STOCK_CALLBACKS_PRESERVED','LEASE_TEARDOWN_TRACKING_RESTORED')
$cases=@([regex]::Matches($log,'\[VEHICLE_GC_CASE\] case=(\S+) passed=([01]) details=\[([^\r\n]*)\]'))
Require ($cases.Count -ge $expected.Count) 'MINIMUM_CASES'
Require (@($cases|Where-Object{$_.Groups[2].Value -ne '1'}).Count -eq 0) 'FAILED_CASE'
Require (@($cases|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq $cases.Count) 'UNIQUE_CASES'
Require (@([regex]::Matches($log,'\[VEHICLE_GC_CASE\]')).Count -eq $cases.Count) 'MALFORMED_CASE'
foreach($name in $expected){Require (@($cases|Where-Object{$_.Groups[1].Value -eq $name -and $_.Groups[2].Value -eq '1'}).Count -eq 1) ('CASE_'+$name)}
$done=@([regex]::Matches($log,'\[VEHICLE_GC_FINISHED\] checks=(\d+) failures=(\d+)'))
Require ($done.Count -eq 1 -and @([regex]::Matches($log,'\[VEHICLE_GC_FINISHED\]')).Count -eq 1) 'UNIQUE_FINISHED'
if($done.Count -eq 1){
 Require ([int]$done[0].Groups[1].Value -eq $cases.Count -and $done[0].Groups[2].Value -eq '0') 'FINISHED_COUNTS'
 Require ($cases.Count -gt 0 -and $done[0].Index -gt $cases[-1].Index -and $log.IndexOf('Game destroyed.') -gt $done[0].Index) 'FINISHED_BEFORE_STOP'
}
$errors=@([regex]::Matches($all,'(?m)^.*\([EF]\).*$')).Count
$resourceVerdict='PASS';if($errors){$resourceVerdict='FAIL'}
"VEHICLE_GC_GATE gate=strict_native_resources verdict=$resourceVerdict error_lines=$errors independent_blocker=1"
if($failures.Count){$failures|ForEach-Object {"FAIL $_"};exit 1}
"VEHICLE_GC_AUDIT verdict=PASS scope=TARGETED_CONTROLLED_FUNCTIONAL_ONLY checks=$($cases.Count) strict_native_resources=$resourceVerdict"
exit 0
