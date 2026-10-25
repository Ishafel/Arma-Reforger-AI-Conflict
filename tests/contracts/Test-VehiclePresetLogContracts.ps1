param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-vehicle-preset-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work|Out-Null
$argv=@('-gproj','C:\source\addon.gproj','-profile','C:\profile','-addonsDir','C:\source','-addons','CORE,MAP','-aicfVehiclePresetProbe','1')
$launcher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)
$names=@('US_CATALOG_INSPECTED','USSR_CATALOG_INSPECTED','ARMED_UAZ_TURRET','SELECT_UAZ','VALID_ROSTER','UNASSIGN','MANDATORY_CREW_REJECTED','DUPLICATE_MEMBER_REJECTED','DUPLICATE_SEAT_REJECTED','MISSING_SEAT_REJECTED','INACTIVE_MEMBER_REJECTED','REASSIGN_UNIQUE','CAPACITY_REJECTED','VALID_AFTER_NEGATIVES','NEGATIVES_KEEP_BINDING','STORE_SAVE','STORE_LOAD','STORE_REOPEN','APPLY','REPEAT_APPLY','STORE_DELETE','DELETE_KEEPS_BINDING','EXACT_MODEL','PAID','UNCHANGED_LOADOUT_SIZE')
$purchase='[VEHICLE_PRESET_PURCHASE] operation=trip-1 prefab=vehicle.et supplies=125'
$good=(@('CLI Params: '+($argv -join ' '),'[ROSTER_READY]')+@($names|ForEach-Object{"[VEHICLE_PRESET_CHECK] rule=$_ passed=1"})+@('[VEHICLE_PRESET_OCCUPANT] operation=trip-1 revision=1 position=0 seat=0:2:0 member=123 occupant=123',$purchase,'PRESET_EXACT_OCCUPANTS_CONFIRMED',('[VEHICLE_PRESET_DONE] checks='+$names.Count+' failures=0'),'Game destroyed.')) -join "`n"
$count=0
function Check([string]$Name,[string]$Log,[int]$Expected,[string]$Rule,[string]$ExitCode='0',[switch]$Reload,[switch]$Lifecycle,[switch]$Human,[switch]$Policy,[switch]$Civilian){
 [IO.File]::WriteAllText("$work/console.log",$Log)
 $currentLauncher=$launcher
 if($Reload){$currentLauncher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv+@('-aicfVehiclePresetReload','1')}|ConvertTo-Json -Compress)}
 if($Human){$currentLauncher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv+@('-aicfVehiclePresetHuman','1')}|ConvertTo-Json -Compress)}
 if($Civilian){$currentLauncher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv+@('-aicfVehiclePresetCivilian','1')}|ConvertTo-Json -Compress)}
 [IO.File]::WriteAllText("$work/launcher.txt",$currentLauncher)
 [IO.File]::WriteAllText("$work/exit.txt",$ExitCode)
 $options=@()
 if($Reload){$options=@('-LibraryReload')}
 if($Lifecycle){$options=@('-Lifecycle')}
 if($Human){$options=@('-HumanProtection')}
 if($Policy){$options=@('-Policy')}
 if($Civilian){$options=@('-Civilian')}
 $output=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehiclePresetLog.ps1') -LogPath "$work/console.log" -LauncherPath "$work/launcher.txt" -NativeExitPath "$work/exit.txt" @options 2>&1
 if($LASTEXITCODE -ne $Expected -or ($Rule -and ($output -join "`n") -notmatch $Rule)){throw "$Name unexpected verdict: $output"}
 $script:count++
}
try{
 Check 'complete' $good 0 ''
 Check 'wrong-occupant' ($good.Replace('occupant=123','occupant=999')) 1 'WRONG_OCCUPANT'
 Check 'missing-occupants' ($good.Replace('[VEHICLE_PRESET_OCCUPANT]','[REMOVED]')) 1 'PHYSICAL_OCCUPANTS_MISSING'
 Check 'intent-only' ($good.Replace('PRESET_EXACT_OCCUPANTS_CONFIRMED','BOARDING_REQUESTED')) 1 'BOARDING_INCOMPLETE'
 Check 'duplicate-debit' ($good+"`n"+$purchase) 1 'DUPLICATE_OR_MISSING_DEBIT'
 Check 'failed-check' ($good.Replace('rule=VALID_ROSTER passed=1','rule=VALID_ROSTER passed=0')) 1 'FAILED_CASE_VALID_ROSTER'
 Check 'missing-check' ($good.Replace('rule=APPLY','rule=REMOVED')) 1 'MISSING_OR_DUPLICATE_CASE_APPLY'
 Check 'duplicate-check' ($good+"`n[VEHICLE_PRESET_CHECK] rule=APPLY passed=1") 1 'MISSING_OR_DUPLICATE_CASE_APPLY'
 Check 'unfinished' ($good.Replace('Game destroyed.','')) 1 'STOPPED_LOG'
 Check 'wrong-cli' ($good.Replace('-aicfVehiclePresetProbe 1','-aicfVehiclePresetProbe 0')) 1 'CLI_MISMATCH'
 Check 'native-failure' $good 1 'NATIVE_EXIT' '-1'
 Check 'script-error' ($good+"`nSCRIPT (E): NULL pointer") 1 'FULL_LOG_FATAL'
 Check 'duplicate-occupant' ($good+"`n[VEHICLE_PRESET_OCCUPANT] operation=trip-1 revision=1 position=0 seat=0:2:0 member=123 occupant=123") 1 'INCOMPLETE_OR_DUPLICATE_OCCUPANTS'
 $lifeNames=@('UNAVAILABLE_PREFAB_REJECTED','FLEET_CAP_REJECTED_ATOMIC','SUPPLIES_REJECTED','SUPPLIES_RESTORED_NO_BINDING','STORE_UPDATE_DRAFT','UPDATE_KEEPS_ACTIVE','LEGACY_UNCHANGED','REPEAT_LIVE_APPLY','REPEAT_NO_SPAWN_DEBIT','MANUAL_PRIORITY','RESET','OLD_REQUEST_CANCELLED','OLD_GENERATION_CALLBACK','RESET_KEEPS_PROFILE','MODEL_CHANGE_CANCELS_OLD_TRIP','ROLE_CHANGE_KEEPS_BINDING','REPLACEMENT_GROUP_REQUESTED','REPLACEMENT_REJECTS_OLD_TRIP','NEW_GROUP_REVALIDATED')
 $life=$good.Replace('[VEHICLE_PRESET_DONE] checks='+$names.Count+' failures=0', '[VEHICLE_PRESET_DONE] checks='+($names.Count+$lifeNames.Count)+' failures=0')+"`n"+(@($lifeNames|ForEach-Object{"[VEHICLE_PRESET_CHECK] rule=$_ passed=1"}) -join "`n")
 Check 'lifecycle' $life 0 '' -Lifecycle
 Check 'lifecycle-missing-replacement' ($life.Replace('rule=NEW_GROUP_REVALIDATED','rule=REMOVED')) 1 'MISSING_OR_DUPLICATE_CASE_NEW_GROUP_REVALIDATED' -Lifecycle
 $humanNames=@('HUMAN_TAKEOVER','HUMAN_VEHICLE_RETAINED','HUMAN_NOT_EJECTED','RESET','OLD_REQUEST_CANCELLED')
 $human=$good.Replace('CLI Params: '+($argv -join ' '),'CLI Params: '+(($argv+@('-aicfVehiclePresetHuman','1')) -join ' ')).Replace('[VEHICLE_PRESET_DONE] checks='+$names.Count+' failures=0','[VEHICLE_PRESET_DONE] checks='+($names.Count+$humanNames.Count)+' failures=0')+"`n"+(@($humanNames|ForEach-Object{"[VEHICLE_PRESET_CHECK] rule=$_ passed=1"}) -join "`n")
 Check 'human-protection' $human 0 '' -Human
 Check 'human-missing-retain' ($human.Replace('rule=HUMAN_VEHICLE_RETAINED','rule=REMOVED')) 1 'MISSING_OR_DUPLICATE_CASE_HUMAN_VEHICLE_RETAINED' -Human
 $policy=$good.Replace('[VEHICLE_PRESET_DONE] checks='+$names.Count+' failures=0','[VEHICLE_PRESET_DONE] checks='+($names.Count+2)+' failures=0')+"`n[VEHICLE_PRESET_CHECK] rule=MANUAL_OVERRIDES_AUTOMATIC_POLICY passed=1`n[VEHICLE_PRESET_CHECK] rule=AUTOMATIC_POLICY_RESTORED passed=1"
 Check 'policy' $policy 0 '' -Policy
 Check 'policy-missing-reset' ($policy.Replace('rule=AUTOMATIC_POLICY_RESTORED','rule=REMOVED')) 1 'MISSING_OR_DUPLICATE_CASE_AUTOMATIC_POLICY_RESTORED' -Policy
 $civilian=$good.Replace('CLI Params: '+($argv -join ' '),'CLI Params: '+(($argv+@('-aicfVehiclePresetCivilian','1')) -join ' ')).Replace('rule=ARMED_UAZ_TURRET','rule=CIVILIAN_MODEL')
 Check 'civilian' $civilian 0 '' -Civilian
 Check 'civilian-missing-permission' ($civilian.Replace('rule=CIVILIAN_MODEL','rule=REMOVED')) 1 'MISSING_OR_DUPLICATE_CASE_CIVILIAN_MODEL' -Civilian
 $restartNames=@('US_CATALOG_INSPECTED','USSR_CATALOG_INSPECTED','RESTART_MODEL_SEATS','RESTART_TOMBSTONE','RESTART_LEGACY','RESTART_NO_ASSIGNMENT','RESTART_NO_VEHICLE')
 $restart=(@('CLI Params: '+(($argv+@('-aicfVehiclePresetReload','1')) -join ' '),'[ROSTER_READY]')+@($restartNames|ForEach-Object{"[VEHICLE_PRESET_CHECK] rule=$_ passed=1"})+@('[VEHICLE_PRESET_DONE] checks=7 failures=0','Game destroyed.')) -join "`n"
 Check 'restart' $restart 0 '' -Reload
 Check 'restart-missing-model' ($restart.Replace('rule=RESTART_MODEL_SEATS','rule=REMOVED')) 1 'MISSING_OR_DUPLICATE_CASE_RESTART_MODEL_SEATS' -Reload
 Check 'restart-spawn-debit' ($restart+"`n"+$purchase) 1 'RELOAD_SIDE_EFFECT' -Reload
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe test cleanup path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
Write-Output "PASS Vehicle preset log contracts controls=$count"
