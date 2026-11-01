param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$LauncherPath,
 [Parameter(Mandatory)][string]$NativeExitPath,
 [Parameter(Mandatory)][ValidateSet('transport','combat')][string]$Scenario,
 [ValidateSet('US','USSR')][string]$Faction='USSR',
 [switch]$CleanupIncapProbe,
 [switch]$CleanupAliveProbe,
 [switch]$ReboardPilotLossProbe,
 [switch]$NativeRouteLossProbe
)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=[Collections.Generic.List[string]]::new()
function Require([bool]$condition,[string]$rule){if(!$condition){$failures.Add($rule)}}
$probeCount=[int]$CleanupIncapProbe.IsPresent+[int]$CleanupAliveProbe.IsPresent+[int]$ReboardPilotLossProbe.IsPresent+[int]$NativeRouteLossProbe.IsPresent
Require ($probeCount -le 1 -and ($probeCount -eq 0 -or $Scenario -eq 'combat')) 'PROBE_MODE'
Require (([IO.File]::ReadAllText($NativeExitPath)).Trim() -eq '0') 'NATIVE_EXIT'
Require ($log.Contains('Game destroyed.') -and $log.Contains('[ROSTER_READY]')) 'FULL_STOPPED_ROSTER'
Require ($all -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation') 'FULL_LOG_FATAL'
$manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
Require ($manifests.Count -eq 1) 'MANIFEST'
if($manifests.Count -eq 1){
 try{
  $argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
  Require ($argv.Count -gt 0) 'MANIFEST_ARGUMENTS'
  $cli=@([regex]::Matches($log,'(?m)CLI Params:\s*([^\r\n]+)'))
  Require ($cli.Count -eq 1 -and $cli[0].Groups[1].Value.Trim() -ceq ($argv -join ' ')) 'EXACT_CLI'
  $flags=@('-gproj','-profile','-addonsDir','-addons','-aicfVehicleBehaviorProbe','-aicfVehicleBehaviorFaction')
  if($CleanupIncapProbe){$flags+='-aicfCleanupIncapProbe'}
  if($CleanupAliveProbe){$flags+='-aicfCleanupAliveProbe'}
  if($ReboardPilotLossProbe){$flags+='-aicfReboardPilotLossProbe'}
  if($NativeRouteLossProbe){$flags+='-aicfNativeRouteLossProbe'}
  foreach($flag in $flags){
   $indices=@(for($i=0;$i -lt $argv.Count;$i++){if($argv[$i] -eq $flag){$i}})
   Require ($indices.Count -eq 1 -and $indices[0]+1 -lt $argv.Count -and ![string]::IsNullOrWhiteSpace($argv[$indices[0]+1])) ('CLI_'+$flag)
  }
  foreach($probeFlag in @('-aicfCleanupIncapProbe','-aicfCleanupAliveProbe','-aicfReboardPilotLossProbe','-aicfNativeRouteLossProbe')){
   if($flags -contains $probeFlag){
    $probeIndex=[array]::IndexOf($argv,$probeFlag)
    Require ($probeIndex -ge 0 -and $probeIndex+1 -lt $argv.Count -and $argv[$probeIndex+1] -eq '1') ('PROBE_VALUE_'+$probeFlag)
   }
  }
  $scenarioIndex=[array]::IndexOf($argv,'-aicfVehicleBehaviorProbe')
  $factionIndex=[array]::IndexOf($argv,'-aicfVehicleBehaviorFaction')
  Require ($scenarioIndex -ge 0 -and $scenarioIndex+1 -lt $argv.Count -and $argv[$scenarioIndex+1] -eq $Scenario) 'SCENARIO_CLI'
  Require ($factionIndex -ge 0 -and $factionIndex+1 -lt $argv.Count -and $argv[$factionIndex+1] -eq $Faction) 'FACTION_CLI'
 }catch{$failures.Add('MALFORMED_MANIFEST')}
}
$cases=@([regex]::Matches($log,'\[VEHICLE_BEHAVIOR_CASE\] (?:run=\S+ t_ms=\d+ )?case=(\S+) passed=([01]) details=\[([^\r\n]*)\]'))
Require ($cases.Count -ge 10) 'MINIMUM_BEHAVIOR_CASES'
Require (@($cases|Where-Object{$_.Groups[2].Value -ne '1'}).Count -eq 0) 'FAILED_BEHAVIOR_CASE'
Require (@($cases|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq $cases.Count) 'UNIQUE_BEHAVIOR_CASES'
Require (@([regex]::Matches($log,'\[VEHICLE_BEHAVIOR_CASE\]')).Count -eq $cases.Count) 'MALFORMED_BEHAVIOR_CASE'
$physicalCases=@('BOARDING_PHYSICAL','THREAT_DISMOUNT_PHYSICAL','RETURN_SAME_ASSET','DRIVER_REPLACED_PHYSICAL','NATIVE_ALLOCATION_NOT_TOPOLOGY_CHANGE')
if($Scenario -eq 'combat'){$physicalCases+=@('COMBAT_CREW_RETAINED','COMBAT_REPLACEMENT_DEFERRED')}
if($Scenario -eq 'transport'){$physicalCases+=@('TRANSPORT_SECOND_THREAT_DEPLOYMENT','DISTANT_ORDER_COMBAT_OVERRIDE')}
if($CleanupIncapProbe){$physicalCases+=@('CLEANUP_INCAP_RELEASE_QUEUED','CLEANUP_INCAP_HULL_GHOST_ESTABLISHED','CLEANUP_INCAP_HULL_PHYSICAL_CLEAR','CLEANUP_INCAP_LIFE_HEALTH_PRESERVED','CLEANUP_INCAP_RECOVERY_BUDGET','CLEANUP_INCAP_FULL_RELEASE')}
if($CleanupAliveProbe){$physicalCases+=@('CLEANUP_ALIVE_RELEASE_QUEUED','CLEANUP_ALIVE_HULL_GHOST_ESTABLISHED','CLEANUP_ALIVE_HULL_PHYSICAL_CLEAR','CLEANUP_ALIVE_LIFE_HEALTH_PRESERVED','CLEANUP_ALIVE_RECOVERY_BUDGET','CLEANUP_ALIVE_FULL_RELEASE')}
if($ReboardPilotLossProbe){$physicalCases+=@('REBOARD_PILOT_LOSS_PHASE','REBOARD_PILOT_LOSS_NATIVE','REBOARD_PILOT_LOSS_RECOVERED','REBOARD_PILOT_LOSS_MOVEMENT')}
if($NativeRouteLossProbe){$physicalCases+=@('NATIVE_ROUTE_LOSS_PRECONDITION','NATIVE_ROUTE_LOSS_REMOVED_1','NATIVE_ROUTE_LOSS_REMOVED_2','NATIVE_ROUTE_LOSS_REMOVED_3','NATIVE_ROUTE_LOSS_REBUILD_1','NATIVE_ROUTE_LOSS_REBUILD_2','NATIVE_ROUTE_LOSS_EXHAUSTED')}
foreach($physicalCase in $physicalCases){Require (@($cases|Where-Object{$_.Groups[1].Value -eq $physicalCase -and $_.Groups[2].Value -eq '1'}).Count -eq 1) ('PHYSICAL_'+$physicalCase)}
$done=@([regex]::Matches($log,'\[VEHICLE_BEHAVIOR_FINISHED\] (?:run=\S+ t_ms=\d+ )?scenario=(\S+) checks=(\d+) failures=(\d+)'))
Require ($done.Count -eq 1 -and @([regex]::Matches($log,'\[VEHICLE_BEHAVIOR_FINISHED\]')).Count -eq 1) 'UNIQUE_BEHAVIOR_FINISHED'
if($done.Count -eq 1){
 Require ($done[0].Groups[1].Value -eq $Scenario -and [int]$done[0].Groups[2].Value -eq $cases.Count -and $done[0].Groups[3].Value -eq '0') 'BEHAVIOR_FINISHED_COUNTS'
 Require ($cases.Count -gt 0 -and $done[0].Index -gt $cases[-1].Index -and $log.IndexOf('Game destroyed.') -gt $done[0].Index) 'BEHAVIOR_FINISHED_BEFORE_STOP'
}
$expectedSettings=@('LEGACY_TRANSPORT300','SCHEMA2_COMBAT_CUSTOM_DISTANCE','SCHEMA2_MISSING_BEHAVIOR_REJECTED','SCHEMA2_MISSING_DISTANCE_REJECTED','FUTURE_SCHEMA_REJECTED','NEGATIVE_BEHAVIOR_REJECTED','UNKNOWN_BEHAVIOR_REJECTED','DISTANCE49_REJECTED','DISTANCE1001_REJECTED','DISTANCE50_ACCEPTED','DISTANCE1000_ACCEPTED','MODE_CHANGE_IS_NEW_ASSIGNMENT','DISTANCE_CHANGE_IS_NEW_ASSIGNMENT','NAME_ONLY_ASSIGNMENT_IDEMPOTENT','MUTATED_OBJECT_VALIDATION_REJECTED','LIBRARY_SAVE_COMBAT','LIBRARY_REPEAT_SAVE_IDEMPOTENT','LIBRARY_FRESH_STORE_READBACK','LIBRARY_UPDATE_TRANSPORT','LIBRARY_UPDATE_PERSISTS_MODE','LIBRARY_INVALID_UPDATE_REJECTED','LIBRARY_DELETE','LIBRARY_TOMBSTONE_SURVIVES_RELOAD')
$settings=@([regex]::Matches($log,'\[VEHICLE_BEHAVIOR_SETTINGS_CASE\] (?:run=\S+ t_ms=\d+ )?case=(\S+) passed=([01])(?=\s|$)'))
Require ($settings.Count -eq $expectedSettings.Count -and @([regex]::Matches($log,'\[VEHICLE_BEHAVIOR_SETTINGS_CASE\]')).Count -eq $settings.Count) 'SETTINGS_EXACT_CASE_COUNT'
foreach($name in $expectedSettings){
 $current=@($settings|Where-Object{$_.Groups[1].Value -eq $name})
 Require ($current.Count -eq 1 -and $current[0].Groups[2].Value -eq '1') ('SETTINGS_'+$name)
}
$settingsDone=@([regex]::Matches($log,'\[VEHICLE_BEHAVIOR_SETTINGS_FINISHED\] (?:run=\S+ t_ms=\d+ )?checks=(\d+) failures=(\d+)'))
Require ($settingsDone.Count -eq 1 -and @([regex]::Matches($log,'\[VEHICLE_BEHAVIOR_SETTINGS_FINISHED\]')).Count -eq 1) 'UNIQUE_SETTINGS_FINISHED'
if($settingsDone.Count -eq 1){
 Require ([int]$settingsDone[0].Groups[1].Value -eq $expectedSettings.Count -and $settingsDone[0].Groups[2].Value -eq '0') 'SETTINGS_FINISHED_COUNTS'
 Require ($settings.Count -gt 0 -and $settingsDone[0].Index -gt $settings[-1].Index -and $done.Count -eq 1 -and $settingsDone[0].Index -lt $done[0].Index) 'SETTINGS_FINISHED_ORDER'
}
$resourceErrors=@([regex]::Matches($all,'(?m)^.*\s\(E\).*')).Count
$resourceVerdict='PASS';if($resourceErrors){$resourceVerdict='FAIL'}
"VEHICLE_BEHAVIOR_GATE gate=strict_native_resources verdict=$resourceVerdict error_lines=$resourceErrors independent_blocker=1"
if($failures.Count){"VEHICLE_BEHAVIOR_AUDIT verdict=FAIL scenario=$Scenario faction=$Faction reasons=$((@($failures|Sort-Object -Unique)) -join ',')";exit 1}
"VEHICLE_BEHAVIOR_AUDIT verdict=PASS scope=TARGETED_CONTROLLED_FUNCTIONAL_ONLY scenario=$Scenario faction=$Faction behavior_checks=$($cases.Count) settings_checks=$($settings.Count) strict_native_resources=$resourceVerdict"
exit 0
