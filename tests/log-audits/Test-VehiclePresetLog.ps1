param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[switch]$LibraryReload,[switch]$Lifecycle,[switch]$HumanProtection,[switch]$Policy,[switch]$Civilian)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -Filter '*.log' -File)|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=@()
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0'){$failures+='NATIVE_EXIT'}
if(!$log.Contains('Game destroyed.')){$failures+='STOPPED_LOG'}
if(!$log.Contains('[ROSTER_READY]')){$failures+='ROSTER'}
if($all -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation'){$failures+='FULL_LOG_FATAL'}
$manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
if($manifests.Count -ne 1){$failures+='MANIFEST'}
else{
 $manifest=$manifests[0].Groups[1].Value|ConvertFrom-Json
 $argv=@($manifest.arguments)
 foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehiclePresetProbe')){
  $index=[array]::IndexOf($argv,$flag)
  if($index -lt 0 -or $index+1 -ge $argv.Count -or !$log.Contains($flag+' '+$argv[$index+1])){$failures+='CLI_MISMATCH'}
 }
 $probe=[array]::IndexOf($argv,'-aicfVehiclePresetProbe')
 if($probe -lt 0 -or $argv[$probe+1] -ne '1'){$failures+='PROBE_CLI'}
}
$cases=@([regex]::Matches($log,'\[VEHICLE_PRESET_CHECK\] rule=(\w+) passed=([01])'))
foreach($case in $cases){if($case.Groups[2].Value -ne '1'){$failures+='FAILED_CASE_'+$case.Groups[1].Value}}
$names=@('US_CATALOG_INSPECTED','USSR_CATALOG_INSPECTED','ARMED_UAZ_TURRET','SELECT_UAZ','VALID_ROSTER','UNASSIGN','MANDATORY_CREW_REJECTED','DUPLICATE_MEMBER_REJECTED','DUPLICATE_SEAT_REJECTED','MISSING_SEAT_REJECTED','INACTIVE_MEMBER_REJECTED','REASSIGN_UNIQUE','CAPACITY_REJECTED','VALID_AFTER_NEGATIVES','NEGATIVES_KEEP_BINDING','STORE_SAVE','STORE_LOAD','STORE_REOPEN','APPLY','REPEAT_APPLY','STORE_DELETE','DELETE_KEEPS_BINDING','EXACT_MODEL','PAID','UNCHANGED_LOADOUT_SIZE')
if($Civilian){
 $names=@($names|Where-Object{$_ -ne 'ARMED_UAZ_TURRET'})+@('CIVILIAN_MODEL')
 $civilianFlag=[array]::IndexOf($argv,'-aicfVehiclePresetCivilian')
 if($civilianFlag -lt 0 -or $argv[$civilianFlag+1] -ne '1'){$failures+='CIVILIAN_CLI'}
}
if($LibraryReload){
 $names=@('US_CATALOG_INSPECTED','USSR_CATALOG_INSPECTED','RESTART_MODEL_SEATS','RESTART_TOMBSTONE','RESTART_LEGACY','RESTART_NO_ASSIGNMENT','RESTART_NO_VEHICLE')
 $reloadFlag=[array]::IndexOf($argv,'-aicfVehiclePresetReload')
 if($reloadFlag -lt 0 -or $argv[$reloadFlag+1] -ne '1'){$failures+='RELOAD_CLI'}
}
if($Lifecycle){$names+=@('UNAVAILABLE_PREFAB_REJECTED','FLEET_CAP_REJECTED_ATOMIC','SUPPLIES_REJECTED','SUPPLIES_RESTORED_NO_BINDING','STORE_UPDATE_DRAFT','UPDATE_KEEPS_ACTIVE','LEGACY_UNCHANGED','REPEAT_LIVE_APPLY','REPEAT_NO_SPAWN_DEBIT','MANUAL_PRIORITY','RESET','OLD_REQUEST_CANCELLED','OLD_GENERATION_CALLBACK','RESET_KEEPS_PROFILE','MODEL_CHANGE_CANCELS_OLD_TRIP','ROLE_CHANGE_KEEPS_BINDING','REPLACEMENT_GROUP_REQUESTED','REPLACEMENT_REJECTS_OLD_TRIP','NEW_GROUP_REVALIDATED')}
if($Policy){$names+=@('MANUAL_OVERRIDES_AUTOMATIC_POLICY','AUTOMATIC_POLICY_RESTORED')}
if($HumanProtection){
 $names+=@('HUMAN_TAKEOVER','HUMAN_VEHICLE_RETAINED','HUMAN_NOT_EJECTED','RESET','OLD_REQUEST_CANCELLED')
 $humanFlag=[array]::IndexOf($argv,'-aicfVehiclePresetHuman')
 if($humanFlag -lt 0 -or $argv[$humanFlag+1] -ne '1'){$failures+='HUMAN_CLI'}
}
foreach($name in $names){
 $found=@($cases|Where-Object{$_.Groups[1].Value -ceq $name})
 if($found.Count -ne 1){$failures+='MISSING_OR_DUPLICATE_CASE_'+$name}
}
$done=@([regex]::Matches($log,'\[VEHICLE_PRESET_DONE\] checks=(\d+) failures=(\d+)'))
if($done.Count -ne 1 -or $done[0].Groups[1].Value -ne $cases.Count.ToString() -or $done[0].Groups[2].Value -ne '0'){$failures+='COMPLETION'}
if($LibraryReload){
 if($log -match '\[VEHICLE_PRESET_PURCHASE\]|\[VEHICLE_PRESET_OCCUPANT\]'){$failures+='RELOAD_SIDE_EFFECT'}
 if($failures.Count){$failures|Sort-Object -Unique;exit 1}
 Write-Output "PASS Vehicle preset library restart stopped log checks=$($cases.Count) physical_gate=NOT_RUN"
 exit 0
}
$occupants=@([regex]::Matches($log,'\[VEHICLE_PRESET_OCCUPANT\](?: run=\S+ t_ms=\d+)? operation=(\S+) revision=(\d+) position=(\d+) seat=(\S+) member=(\S+) occupant=(\S+)'))
if(!$occupants.Count){$failures+='PHYSICAL_OCCUPANTS_MISSING'}
foreach($occupant in $occupants){if($occupant.Groups[5].Value -cne $occupant.Groups[6].Value){$failures+='WRONG_OCCUPANT'}}
$expectedOccupants=1
$crewFlag=[array]::IndexOf($argv,'-aicfVehiclePresetCrew')
if($crewFlag -ge 0){$expectedOccupants=4;if($argv[$crewFlag+1] -eq 'armed'){$expectedOccupants=3};if($argv[$crewFlag+1] -eq 'driver'){$expectedOccupants=1}}
if($occupants.Count -ne $expectedOccupants -or @($occupants|ForEach-Object{$_.Groups[3].Value}|Select-Object -Unique).Count -ne $expectedOccupants -or @($occupants|ForEach-Object{$_.Groups[4].Value}|Select-Object -Unique).Count -ne $expectedOccupants){$failures+='INCOMPLETE_OR_DUPLICATE_OCCUPANTS'}
if(!$log.Contains('PRESET_EXACT_OCCUPANTS_CONFIRMED')){$failures+='BOARDING_INCOMPLETE'}
$purchases=@([regex]::Matches($log,'\[VEHICLE_PRESET_PURCHASE\](?: run=\S+ t_ms=\d+)? operation=(\S+) prefab=(\S+) supplies=(\d+)'))
if($purchases.Count -ne 1 -or [int]$purchases[0].Groups[3].Value -le 0){$failures+='DUPLICATE_OR_MISSING_DEBIT'}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
Write-Output "PASS Vehicle preset native stopped log checks=$($cases.Count) occupants=$($occupants.Count) purchases=$($purchases.Count)"
exit 0
