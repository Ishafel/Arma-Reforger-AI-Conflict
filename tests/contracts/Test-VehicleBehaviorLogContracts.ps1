param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-vehicle-behavior-log-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','C:\source path\addon.gproj','-profile','C:\profile','-addonsDir','C:\source path','-addons','CORE,MAP','-aicfVehicleBehaviorProbe','transport','-aicfVehicleBehaviorFaction','USSR')
$manifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)
$settings=@('LEGACY_TRANSPORT300','SCHEMA2_COMBAT_CUSTOM_DISTANCE','SCHEMA2_MISSING_BEHAVIOR_REJECTED','SCHEMA2_MISSING_DISTANCE_REJECTED','FUTURE_SCHEMA_REJECTED','NEGATIVE_BEHAVIOR_REJECTED','UNKNOWN_BEHAVIOR_REJECTED','DISTANCE49_REJECTED','DISTANCE1001_REJECTED','DISTANCE50_ACCEPTED','DISTANCE1000_ACCEPTED','MODE_CHANGE_IS_NEW_ASSIGNMENT','DISTANCE_CHANGE_IS_NEW_ASSIGNMENT','NAME_ONLY_ASSIGNMENT_IDEMPOTENT','MUTATED_OBJECT_VALIDATION_REJECTED','LIBRARY_SAVE_COMBAT','LIBRARY_REPEAT_SAVE_IDEMPOTENT','LIBRARY_FRESH_STORE_READBACK','LIBRARY_UPDATE_TRANSPORT','LIBRARY_UPDATE_PERSISTS_MODE','LIBRARY_INVALID_UPDATE_REJECTED','LIBRARY_DELETE','LIBRARY_TOMBSTONE_SURVIVES_RELOAD')
$rows=@(('CLI Params: '+($argv -join ' ')),'[ROSTER_READY]')
$rows+=@($settings|ForEach-Object{"[VEHICLE_BEHAVIOR_SETTINGS_CASE] case=$_ passed=1"})
$rows+='[VEHICLE_BEHAVIOR_SETTINGS_FINISHED] checks=23 failures=0'
$behaviorCases=@('BOARDING_PHYSICAL','THREAT_DISMOUNT_PHYSICAL','RETURN_SAME_ASSET','DRIVER_REPLACED_PHYSICAL','COMBAT_CREW_RETAINED','COMBAT_REPLACEMENT_DEFERRED','TRANSPORT_SECOND_THREAT_DEPLOYMENT','DISTANT_ORDER_COMBAT_OVERRIDE','NATIVE_ALLOCATION_NOT_TOPOLOGY_CHANGE','CASE10')
$rows+=@($behaviorCases|ForEach-Object{"[VEHICLE_BEHAVIOR_CASE] case=$_ passed=1 details=[sample=1]"})
$rows+='[VEHICLE_BEHAVIOR_FINISHED] scenario=transport checks=10 failures=0'
$rows+='Game destroyed.'
$good=$rows -join "`n"
$checks=0
function Check([string]$name,[string]$log,[int]$expected,[string]$rule='', [string]$launcher=$manifest,[string]$nativeExit='0',[string]$sibling='',[string]$scenario='transport',[string[]]$probeArgs=@()){
 [IO.File]::WriteAllText((Join-Path $work 'console.log'),$log)
 [IO.File]::WriteAllText((Join-Path $work 'script.log'),$sibling)
 [IO.File]::WriteAllText((Join-Path $work 'launcher.txt'),$launcher)
 [IO.File]::WriteAllText((Join-Path $work 'exit.txt'),$nativeExit)
 $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleBehaviorLog.ps1') -LogPath (Join-Path $work 'console.log') -LauncherPath (Join-Path $work 'launcher.txt') -NativeExitPath (Join-Path $work 'exit.txt') -Scenario $scenario -Faction USSR @probeArgs 2>&1)
 if($LASTEXITCODE -ne $expected -or ($rule -and ($output -join "`n") -notmatch $rule)){throw "$name unexpected audit: $($output -join ' ')"}
 $script:checks++
}
try{
 Check complete $good 0 'scope=TARGETED_CONTROLLED_FUNCTIONAL_ONLY.*strict_native_resources=PASS'
 $prefixed=$good.Replace('[VEHICLE_BEHAVIOR_CASE] ','[VEHICLE_BEHAVIOR_CASE] run=stage1-server-9725 t_ms=4022 ').Replace('[VEHICLE_BEHAVIOR_FINISHED] ','[VEHICLE_BEHAVIOR_FINISHED] run=stage1-server-9725 t_ms=4022 ').Replace('[VEHICLE_BEHAVIOR_SETTINGS_CASE] ','[VEHICLE_BEHAVIOR_SETTINGS_CASE] run=stage1-server-9725 t_ms=4022 ').Replace('[VEHICLE_BEHAVIOR_SETTINGS_FINISHED] ','[VEHICLE_BEHAVIOR_SETTINGS_FINISHED] run=stage1-server-9725 t_ms=4022 ')
 Check native-prefix $prefixed 0 'scope=TARGETED_CONTROLLED_FUNCTIONAL_ONLY.*settings_checks=23'
 Check native-prefix-failed-case ($prefixed.Replace('case=BOARDING_PHYSICAL passed=1','case=BOARDING_PHYSICAL passed=0')) 1 'FAILED_BEHAVIOR_CASE'
 Check native-prefix-failed-settings ($prefixed.Replace('case=LIBRARY_FRESH_STORE_READBACK passed=1','case=LIBRARY_FRESH_STORE_READBACK passed=0')) 1 'SETTINGS_LIBRARY_FRESH_STORE_READBACK'
 Check malformed-native-pass-value ($prefixed.Replace('case=LIBRARY_FRESH_STORE_READBACK passed=1','case=LIBRARY_FRESH_STORE_READBACK passed=10')) 1 'SETTINGS_EXACT_CASE_COUNT'
 Check malformed-native-prefix ($prefixed.Replace('t_ms=4022 case=BOARDING_PHYSICAL','t_ms=unknown case=BOARDING_PHYSICAL')) 1 'MALFORMED_BEHAVIOR_CASE'
 Check no-stop ($good.Replace('Game destroyed.','')) 1 'FULL_STOPPED_ROSTER'
 Check no-roster ($good.Replace('[ROSTER_READY]','')) 1 'FULL_STOPPED_ROSTER'
 Check native-failure $good 1 'NATIVE_EXIT' $manifest '1'
 Check no-manifest $good 1 'MANIFEST' ''
 Check duplicate-manifest $good 1 'MANIFEST' ($manifest+"`n"+$manifest)
 Check malformed-manifest $good 1 'MALFORMED_MANIFEST' 'AICF_RUNTIME_MANIFEST_JSON=not-json'
 Check wrong-cli ($good.Replace('-addons CORE,MAP','-addons DIFFERENT')) 1 'EXACT_CLI'
 Check wrong-faction ($good.Replace('-aicfVehicleBehaviorFaction USSR','-aicfVehicleBehaviorFaction US')) 1 'EXACT_CLI'
 Check failed-case ($good.Replace('case=BOARDING_PHYSICAL passed=1','case=BOARDING_PHYSICAL passed=0')) 1 'FAILED_BEHAVIOR_CASE'
 Check no-current-combat-override ($good.Replace('[VEHICLE_BEHAVIOR_CASE] case=DISTANT_ORDER_COMBAT_OVERRIDE passed=1 details=[sample=1]','')) 1 'PHYSICAL_DISTANT_ORDER_COMBAT_OVERRIDE'
 Check no-captured-allocation-proof ($good.Replace('[VEHICLE_BEHAVIOR_CASE] case=NATIVE_ALLOCATION_NOT_TOPOLOGY_CHANGE passed=1 details=[sample=1]','')) 1 'PHYSICAL_NATIVE_ALLOCATION_NOT_TOPOLOGY_CHANGE'
 Check malformed-case ($good.Replace('case=BOARDING_PHYSICAL passed=1 details=[sample=1]','case=BOARDING_PHYSICAL passed=1')) 1 'MALFORMED_BEHAVIOR_CASE'
 Check missing-case ($good.Replace('[VEHICLE_BEHAVIOR_CASE] case=BOARDING_PHYSICAL passed=1 details=[sample=1]','')) 1 'MINIMUM_BEHAVIOR_CASES'
 Check duplicate-case ($good.Replace('case=THREAT_DISMOUNT_PHYSICAL ','case=BOARDING_PHYSICAL ')) 1 'UNIQUE_BEHAVIOR_CASES'
 Check intent-only ($good.Replace('case=BOARDING_PHYSICAL ','case=BOARDING_REQUESTED ')) 1 'PHYSICAL_BOARDING_PHYSICAL'
 Check wrong-count ($good.Replace('scenario=transport checks=10','scenario=transport checks=11')) 1 'BEHAVIOR_FINISHED_COUNTS'
 Check wrong-scenario ($good.Replace('scenario=transport checks=10','scenario=combat checks=10')) 1 'BEHAVIOR_FINISHED_COUNTS'
 Check failure-summary ($good.Replace('scenario=transport checks=10 failures=0','scenario=transport checks=10 failures=1')) 1 'BEHAVIOR_FINISHED_COUNTS'
 Check duplicate-finished ($good+"`n[VEHICLE_BEHAVIOR_FINISHED] scenario=transport checks=10 failures=0") 1 'UNIQUE_BEHAVIOR_FINISHED'
 Check missing-finished ($good.Replace('[VEHICLE_BEHAVIOR_FINISHED] scenario=transport checks=10 failures=0','')) 1 'UNIQUE_BEHAVIOR_FINISHED'
 Check missing-settings ($good.Replace('[VEHICLE_BEHAVIOR_SETTINGS_CASE] case=DISTANCE49_REJECTED passed=1','')) 1 'SETTINGS_DISTANCE49_REJECTED'
 Check failed-settings ($good.Replace('case=DISTANCE49_REJECTED passed=1','case=DISTANCE49_REJECTED passed=0')) 1 'SETTINGS_DISTANCE49_REJECTED'
 Check duplicate-settings ($good.Replace('case=DISTANCE50_ACCEPTED ','case=DISTANCE49_REJECTED ')) 1 'SETTINGS_DISTANCE49_REJECTED'
 Check incomplete-library ($good.Replace('case=LIBRARY_FRESH_STORE_READBACK','case=UNPROVEN_LIBRARY')) 1 'SETTINGS_LIBRARY_FRESH_STORE_READBACK'
 Check missing-settings-finished ($good.Replace('[VEHICLE_BEHAVIOR_SETTINGS_FINISHED] checks=23 failures=0','')) 1 'UNIQUE_SETTINGS_FINISHED'
 Check wrong-settings-count ($good.Replace('SETTINGS_FINISHED] checks=23','SETTINGS_FINISHED] checks=22')) 1 'SETTINGS_FINISHED_COUNTS'
 Check sibling-fatal $good 1 'FULL_LOG_FATAL' $manifest '0' 'SCRIPT (E): null fault'
 Check resource-independent-blocker $good 0 'gate=strict_native_resources verdict=FAIL' $manifest '0' 'RESOURCES (E): missing texture'
 $combat=$good.Replace('-aicfVehicleBehaviorProbe transport','-aicfVehicleBehaviorProbe combat').Replace('scenario=transport','scenario=combat')
 $combatManifest=$manifest.Replace('"transport"','"combat"')
 Check combat-complete $combat 0 'scope=TARGETED_CONTROLLED_FUNCTIONAL_ONLY.*scenario=combat' $combatManifest '0' '' combat
 Check combat-crew-unproven ($combat.Replace('case=COMBAT_CREW_RETAINED ','case=COMBAT_CREW_INTENT ')) 1 'PHYSICAL_COMBAT_CREW_RETAINED' $combatManifest '0' '' combat
 Check combat-defer-unproven ($combat.Replace('case=COMBAT_REPLACEMENT_DEFERRED ','case=COMBAT_REPLACEMENT_QUEUED ')) 1 'PHYSICAL_COMBAT_REPLACEMENT_DEFERRED' $combatManifest '0' '' combat
 foreach($probe in @(
  @{name='reboard';flag='-aicfReboardPilotLossProbe';switch='-ReboardPilotLossProbe';cases=@('REBOARD_PILOT_LOSS_PHASE','REBOARD_PILOT_LOSS_NATIVE','REBOARD_PILOT_LOSS_RECOVERED','REBOARD_PILOT_LOSS_MOVEMENT')},
  @{name='incap';flag='-aicfCleanupIncapProbe';switch='-CleanupIncapProbe';cases=@('CLEANUP_INCAP_RELEASE_QUEUED','CLEANUP_INCAP_HULL_GHOST_ESTABLISHED','CLEANUP_INCAP_HULL_PHYSICAL_CLEAR','CLEANUP_INCAP_LIFE_HEALTH_PRESERVED','CLEANUP_INCAP_RECOVERY_BUDGET','CLEANUP_INCAP_FULL_RELEASE')},
  @{name='route';flag='-aicfNativeRouteLossProbe';switch='-NativeRouteLossProbe';cases=@('NATIVE_ROUTE_LOSS_PRECONDITION','NATIVE_ROUTE_LOSS_REMOVED_1','NATIVE_ROUTE_LOSS_REMOVED_2','NATIVE_ROUTE_LOSS_REMOVED_3','NATIVE_ROUTE_LOSS_REBUILD_1','NATIVE_ROUTE_LOSS_REBUILD_2','NATIVE_ROUTE_LOSS_EXHAUSTED')},
  @{name='alive';flag='-aicfCleanupAliveProbe';switch='-CleanupAliveProbe';cases=@('CLEANUP_ALIVE_RELEASE_QUEUED','CLEANUP_ALIVE_HULL_GHOST_ESTABLISHED','CLEANUP_ALIVE_HULL_PHYSICAL_CLEAR','CLEANUP_ALIVE_LIFE_HEALTH_PRESERVED','CLEANUP_ALIVE_RECOVERY_BUDGET','CLEANUP_ALIVE_FULL_RELEASE')}
 )){
  $probeCli=$combat.Replace('-aicfVehicleBehaviorFaction USSR',('-aicfVehicleBehaviorFaction USSR '+$probe.flag+' 1'))
  $probeRows=($probe.cases|ForEach-Object {"[VEHICLE_BEHAVIOR_CASE] case=$_ passed=1 details=[actual=1]"}) -join "`n"
  $probeLog=$probeCli.Replace('[VEHICLE_BEHAVIOR_FINISHED]',($probeRows+"`n[VEHICLE_BEHAVIOR_FINISHED]")).Replace('checks=10 failures=0',('checks='+(10+$probe.cases.Count)+' failures=0'))
  $probeArgv=@($argv|ForEach-Object {if($_ -eq 'transport'){'combat'}else{$_}})+@($probe.flag,'1')
  $probeManifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$probeArgv}|ConvertTo-Json -Compress)
  Check ($probe.name+'-complete') $probeLog 0 'scope=TARGETED_CONTROLLED_FUNCTIONAL_ONLY' $probeManifest '0' '' combat @($probe.switch)
  foreach($required in $probe.cases){
   Check ($probe.name+'-missing-'+$required) ($probeLog.Replace(('case='+$required+' '),'case=UNPROVEN ')) 1 ('PHYSICAL_'+$required) $probeManifest '0' '' combat @($probe.switch)
  }
  Check ($probe.name+'-wrong-cli-value') ($probeLog.Replace(($probe.flag+' 1'),($probe.flag+' 0'))) 1 ('PROBE_VALUE_'+$probe.flag) ($probeManifest.Replace(('"'+$probe.flag+'","1"'),('"'+$probe.flag+'","0"'))) '0' '' combat @($probe.switch)
  Check ($probe.name+'-missing-cli') $combat 1 ('CLI_'+$probe.flag) $combatManifest '0' '' combat @($probe.switch)
  Check ($probe.name+'-wrong-scenario') $probeLog 1 'PROBE_MODE' $probeManifest '0' '' transport @($probe.switch)
 }
 Check incompatible-probes $combat 1 'PROBE_MODE' $combatManifest '0' '' combat @('-CleanupIncapProbe','-ReboardPilotLossProbe')
 Check incompatible-route-probes $combat 1 'PROBE_MODE' $combatManifest '0' '' combat @('-NativeRouteLossProbe','-ReboardPilotLossProbe')
 Check incompatible-alive-probes $combat 1 'PROBE_MODE' $combatManifest '0' '' combat @('-CleanupAliveProbe','-CleanupIncapProbe')
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $allowed=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-vehicle-behavior-log-*'){throw 'Unsafe log contract cleanup path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Vehicle behavior log controls checks=$checks"
