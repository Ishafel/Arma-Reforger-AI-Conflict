param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=[Collections.Generic.List[string]]::new()
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0'){$failures.Add('NATIVE_EXIT')}
if(!$log.Contains('Game destroyed.')){$failures.Add('STOPPED_LOG')}
if(!$log.Contains('[ROSTER_READY]')){$failures.Add('ROSTER')}
if($all -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation'){$failures.Add('FULL_LOG_FATAL')}
$manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
if($manifests.Count -ne 1){$failures.Add('MANIFEST')}
else{
 $argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
 foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehiclePresetTransitProbe')){
  $index=[array]::IndexOf($argv,$flag)
  if($index -lt 0 -or $index+1 -ge $argv.Count -or !$log.Contains($flag+' '+$argv[$index+1])){$failures.Add('CLI')}
 }
}
$rules=@('GROUP5_CREATED','UAZ_FIVE_REAL_SEATS','COMPATIBLE_APPLY','NEARBY_INITIAL_ORDER','NO_IMMEDIATE_DISMOUNT','PARKED_EXACT_FIVE','IDLE_BUDGET_PAUSED','FIVE_STAY_MOUNTED_30_SECONDS','WAIT_STATUS','NAVMESH_DESTINATION','ONWARD_ORDER','SAME_VEHICLE_REAL_MOTION','ROUTE_BUDGET_RESTORED','ARRIVAL_RETARGET','ARRIVAL_FULL_DISMOUNT','ONE_PURCHASE')
foreach($rule in $rules){if(@([regex]::Matches($log,'\[PRESET_TRANSIT_CHECK\] rule='+$rule+' passed=1')).Count -ne 1){$failures.Add('CASE_'+$rule)}}
if($log -match '\[PRESET_TRANSIT_CHECK\].*passed=0'){$failures.Add('FAILED_CASE')}
if(@([regex]::Matches($log,'\[PRESET_TRANSIT_DONE\] checks=16 failures=0')).Count -ne 1){$failures.Add('COMPLETION')}
$motion=[regex]::Match($log,'\[PRESET_TRANSIT_MOTION\] displacement_m=([0-9.]+) moving_polls=(\d+) same_vehicle=1 exact_occupants=5')
if(!$motion.Success -or [double]::Parse($motion.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture) -lt 25 -or [int]$motion.Groups[2].Value -lt 5){$failures.Add('PHYSICAL_MOTION')}
$dismount=[regex]::Match($log,'(?m)^.*\[VEHICLE_STATE_CHANGED\].*previous_state=TRANSIT state=DISMOUNT.*$')
if(!$dismount.Success -or !$motion.Success -or $dismount.Index -le $motion.Index){$failures.Add('DISMOUNT_BEFORE_TRAVEL')}
$complete=[regex]::Match($log,'\[DISEMBARK_COMPLETE\][^\r\n]*logical_occupants=0[^\r\n]*transitions=0[^\r\n]*inside_bounds=0')
if(!$complete.Success -or $complete.Index -le $dismount.Index -or $log -notmatch '\[VEHICLE_STATE_CHANGED\][^\r\n]*previous_state=HANDOFF state=COMPLETE'){$failures.Add('NORMAL_TRIP_COMPLETE')}
if($log -match '\[ACCEPTANCE_FAILURE_LATCHED\]|\[VEHICLE_STATE_CHANGED\][^\r\n]*previous_state=(?:TRANSIT|DISMOUNT|HANDOFF) state=(?:FALLBACK|FAILED_CLOSED)'){$failures.Add('TRIP_FAILURE')}
$waiting=@([regex]::Matches($log,'\[VEHICLE_PRESET_DESTINATION_WAIT\].* waiting=1 '))
$resumed=@([regex]::Matches($log,'\[VEHICLE_PRESET_DESTINATION_WAIT\].* waiting=0 waited_ms=(\d+)'))
if($waiting.Count -ne 1 -or $resumed.Count -ne 1 -or [int]$resumed[0].Groups[1].Value -lt 30000){$failures.Add('PARKED_WAIT_PROOF')}
$occupants=@([regex]::Matches($log,'\[VEHICLE_PRESET_OCCUPANT\].*position=(\d+) seat=(\S+) member=(\S+) occupant=(\S+)'))
if($occupants.Count -ne 5 -or @($occupants|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -ne 5){$failures.Add('FIVE_EXACT_OCCUPANTS')}
foreach($occupant in $occupants){if($occupant.Groups[3].Value -ne $occupant.Groups[4].Value){$failures.Add('WRONG_OCCUPANT')}}
if(@([regex]::Matches($log,'\[VEHICLE_PRESET_PURCHASE\].*UAZ469_uncovered.et supplies=125')).Count -ne 1){$failures.Add('SINGLE_PURCHASE')}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
'PASS Preset transit:16 checks,5 mounted30s, paused idle budget, real motion25m, arrival dismount, one purchase, native0/stopped'
exit 0
