param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-manual-staging-log-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$argv=@('-gproj','C:\source path\addon.gproj','-profile','C:\profile','-addonsDir','C:\source path','-addons','CORE,MAP','-aicfManualStagingProbe','1')
$manifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)
$names=@('CONTROLLED_NATIVE_MOVEMENT_BLOCK','STALE_REQUEST_NO_MUTATION','STALE_PRESET_NO_MUTATION','STALE_RPL_NO_MUTATION','NO_CURRENT_ASSIGNMENT_NO_MUTATION','LOST_ACTION_NO_MUTATION','EXHAUSTED_TWO_OWNED_ATTEMPTS','SHORT_SUBMITTED_ONCE','DUPLICATE_NO_MUTATION','ACTUAL_SHORT_TELEPORT','ALL_FIVE_PHYSICAL_RALLY_BEFORE_PAYMENT','ORIGINAL_IDENTITIES_HEALTH_UNCHANGED','DEADLINES_UNCHANGED','NO_BUY_OR_DEBIT')
$rows=@(('CLI Params: '+($argv -join ' ')),'[ROSTER_READY]')
$rows+=@($names|ForEach-Object{"[AICF][MANUAL_STAGING_CASE] case=$_ passed=1 details=[native_readback=1]"})
$rows+='[AICF][MANUAL_STAGING_FINISHED] checks=14 failures=0'
$rows+='Game destroyed.'
$good=$rows -join "`n"
$checks=0
function Check([string]$name,[string]$log,[int]$expected,[string]$rule='', [string]$launcher=$manifest,[string]$nativeExit='0',[string]$sibling='',[string[]]$extraArgs=@()){
 [IO.File]::WriteAllText((Join-Path $work 'console.log'),$log)
 [IO.File]::WriteAllText((Join-Path $work 'script.log'),$sibling)
 [IO.File]::WriteAllText((Join-Path $work 'launcher.txt'),$launcher)
 [IO.File]::WriteAllText((Join-Path $work 'exit.txt'),$nativeExit)
 $output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepositoryRoot 'tests/log-audits/Test-ManualStagingLog.ps1') -LogPath (Join-Path $work 'console.log') -LauncherPath (Join-Path $work 'launcher.txt') -NativeExitPath (Join-Path $work 'exit.txt') @extraArgs 2>&1)
 if($LASTEXITCODE -ne $expected -or ($rule -and ($output -join "`n") -notmatch $rule)){throw "$name unexpected audit: $($output -join ' ')"}
 $script:checks++
}
try{
 Check complete $good 0 'TARGETED_CONTROLLED_FUNCTIONAL_ONLY.*checks=14'
 foreach($name in $names){Check ('missing-'+$name) ($good.Replace(('case='+$name+' '),'case=UNPROVEN ')) 1 ('CASE_'+$name)}
 Check failed-hop ($good.Replace('ACTUAL_SHORT_TELEPORT passed=1','ACTUAL_SHORT_TELEPORT passed=0')) 1 'FAILED_CASE'
 Check malformed-hop ($good.Replace('ACTUAL_SHORT_TELEPORT passed=1 details=[native_readback=1]','ACTUAL_SHORT_TELEPORT passed=1')) 1 'MALFORMED_CASE'
 Check duplicate ($good.Replace('case=ALL_FIVE_PHYSICAL_RALLY_BEFORE_PAYMENT ','case=ACTUAL_SHORT_TELEPORT ')) 1 'UNIQUE_CASES'
 Check unfinished ($good.Replace('Game destroyed.','')) 1 'FULL_STOPPED_ROSTER'
 Check native-failure $good 1 'NATIVE_EXIT' $manifest '-1'
 Check no-roster ($good.Replace('[ROSTER_READY]','')) 1 'FULL_STOPPED_ROSTER'
 Check wrong-count ($good.Replace('checks=14 failures=0','checks=19 failures=0')) 1 'FINISHED_COUNTS'
 Check failed-summary ($good.Replace('checks=14 failures=0','checks=14 failures=1')) 1 'FINISHED_COUNTS'
 Check duplicate-finished ($good+"`n[MANUAL_STAGING_FINISHED] checks=14 failures=0") 1 'UNIQUE_FINISHED'
 Check early-summary ($good.Replace('[AICF][MANUAL_STAGING_FINISHED] checks=14 failures=0','').Replace('[ROSTER_READY]',"[ROSTER_READY]`n[MANUAL_STAGING_FINISHED] checks=14 failures=0")) 1 'FINISHED_BEFORE_STOP'
 Check no-manifest $good 1 'MANIFEST' ''
 Check duplicate-manifest $good 1 'MANIFEST' ($manifest+"`n"+$manifest)
 Check malformed-manifest $good 1 'MALFORMED_MANIFEST' 'AICF_RUNTIME_MANIFEST_JSON=bad'
 Check wrong-cli ($good.Replace('-addons CORE,MAP','-addons OTHER')) 1 'EXACT_CLI'
 Check zero-probe ($good.Replace('ManualStagingProbe 1','ManualStagingProbe 0')) 1 'PROBE_VALUE' ($manifest.Replace('Probe","1"','Probe","0"'))
 Check sibling-fatal $good 1 'FULL_LOG_FATAL' $manifest '0' 'SCRIPT (E): injected exception'
 Check resources-independent $good 0 'gate=strict_native_resources verdict=FAIL' $manifest '0' 'RESOURCES (E): missing texture'
 $exNames=@('EXHAUSTION_DISTANT_NATIVE_ORDER_SET','CONTROLLED_NATIVE_MOVEMENT_BLOCK','EXHAUSTION_TWO_ATTEMPTS_SPENT','EXHAUSTION_LIVE_WAYPOINT_NOT_FALLBACK','EXHAUSTION_MISSING_REISSUE_NOT_FALLBACK','EXHAUSTION_WAYPOINT_REMOVED_FIRST','EXHAUSTION_WAYPOINT_REMOVED_REISSUE','EXHAUSTION_EARLY_TYPED_FALLBACK','EXHAUSTION_PLAN_RELEASED','EXHAUSTION_ORDER_RESTORED','EXHAUSTION_NO_TELEPORT_DEBIT','EXHAUSTION_ORIGINAL_HEALTH','EXHAUSTION_DEADLINE_UNCHANGED')
 $exArgv=$argv+@('-aicfManualStagingExhaustionProbe','1')
 $exManifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$exArgv}|ConvertTo-Json -Compress)
 $exRows=@(('CLI Params: '+($exArgv -join ' ')),'[ROSTER_READY]')
 $exRows+=@($exNames|ForEach-Object {$details='native_readback=1';if($_ -eq 'EXHAUSTION_TWO_ATTEMPTS_SPENT'){$details='controlled_all_bounded_placement_rejected=1'};"[AICF][MANUAL_STAGING_CASE] case=$_ passed=1 details=[$details]"})
 $exRows+='[AICF][MANUAL_STAGING_FINISHED] checks=13 failures=0';$exRows+='Game destroyed.'
 $exGood=$exRows -join "`n"
 Check exhaustion-complete $exGood 0 'checks=13' $exManifest '0' '' @('-Exhaustion')
 Check exhaustion-undeclared-placement ($exGood.Replace('controlled_all_bounded_placement_rejected=1','controlled_all_bounded_placement_rejected=0')) 1 'EXPLICIT_CONTROLLED_PLACEMENT_FAULT' $exManifest '0' '' @('-Exhaustion')
 foreach($name in $exNames){Check ('exhaustion-missing-'+$name) ($exGood.Replace(('case='+$name+' '),'case=UNPROVEN ')) 1 ('CASE_'+$name) $exManifest '0' '' @('-Exhaustion')}
 Check exhaustion-no-cli $good 1 'EXHAUSTION_CLI' $manifest '0' '' @('-Exhaustion')
 Check exhaustion-zero-cli ($exGood.Replace('ExhaustionProbe 1','ExhaustionProbe 0')) 1 'EXHAUSTION_VALUE' ($exManifest.Replace('ExhaustionProbe","1"','ExhaustionProbe","0"')) '0' '' @('-Exhaustion')
 $unNames=@('CONTROLLED_NATIVE_MOVEMENT_BLOCK','UNBLOCK_NATIVE_GROUP_WAYPOINT_ABSENT','STALE_REQUEST_NO_MUTATION','STALE_PRESET_NO_MUTATION','STALE_RPL_NO_MUTATION','NO_CURRENT_ASSIGNMENT_NO_MUTATION','LOST_ACTION_NO_MUTATION','EXHAUSTED_TWO_OWNED_ATTEMPTS','SHORT_SUBMITTED_ONCE','DUPLICATE_NO_MUTATION','UNBLOCK_SAME_NATIVE_ACTION_AFTER_SHORT','UNBLOCK_ACTUAL_NATIVE_WALK','UNBLOCK_NATIVE_RALLY_BEFORE_DEBIT','UNBLOCK_NATIVE_SPAWN_AND_DEBIT','UNBLOCK_ORIGINAL_HEALTH_DEADLINES')
 $unArgv=$argv+@('-aicfManualStagingUnblockProbe','1')
 $unManifest='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$unArgv}|ConvertTo-Json -Compress)
 $unRows=@(('CLI Params: '+($unArgv -join ' ')),'[ROSTER_READY]')
 $unRows+='[MANUAL_STAGING_SETUP_ESCAPE] native_usable=1 controlled_escape_precondition=1'
 foreach($name in $unNames){
  $details='native_readback=1'
  if($name -eq 'UNBLOCK_SAME_NATIVE_ACTION_AFTER_SHORT'){$details='actual_3d_correction_m=6 still_outside_rally=1'}
  if($name -eq 'UNBLOCK_ACTUAL_NATIVE_WALK'){$details='walk_after_correction_m=1.5 same_native_action=1 teleport_excluded=1'}
  $unRows+="[AICF][MANUAL_STAGING_CASE] case=$name passed=1 details=[$details]"
 }
 $unRows+='[VEHICLE_MANUAL_STAGING_SHORT_SUBMITTED] same_native_action=1 new_retry=0'
 $unRows+='[VEHICLE_PRESET_PURCHASE]'
 $unRows+='[MANUAL_STAGING_FINISHED] checks=15 failures=0';$unRows+='Game destroyed.'
 $unGood=$unRows -join "`n"
 Check unblock-complete $unGood 0 'checks=15' $unManifest '0' '' @('-Unblock')
 Check unblock-no-native-escape ($unGood.Replace('[MANUAL_STAGING_SETUP_ESCAPE]','[UNPROVEN_ESCAPE]')) 1 'UNBLOCK_NATIVE_ESCAPE_PRECONDITION' $unManifest '0' '' @('-Unblock')
 Check unblock-false-native-escape ($unGood.Replace('native_usable=1','native_usable=0')) 1 'UNBLOCK_NATIVE_ESCAPE_PRECONDITION' $unManifest '0' '' @('-Unblock')
 foreach($name in $unNames){Check ('unblock-missing-'+$name) ($unGood.Replace(('case='+$name+' '),'case=UNPROVEN ')) 1 ('CASE_'+$name) $unManifest '0' '' @('-Unblock')}
 Check unblock-overshift ($unGood.Replace('actual_3d_correction_m=6','actual_3d_correction_m=8.01')) 1 'UNBLOCK_ACTUAL_BOUNDED_LANDING' $unManifest '0' '' @('-Unblock')
 Check unblock-falsewalk ($unGood.Replace('walk_after_correction_m=1.5','walk_after_correction_m=0.99')) 1 'UNBLOCK_REAL_WALK_EXCLUDES_TELEPORT' $unManifest '0' '' @('-Unblock')
 Check unblock-teleport-counted ($unGood.Replace('teleport_excluded=1','teleport_excluded=0')) 1 'UNBLOCK_REAL_WALK_EXCLUDES_TELEPORT' $unManifest '0' '' @('-Unblock')
 Check unblock-new-action ($unGood.Replace('same_native_action=1','same_native_action=0')) 1 'UNBLOCK_REAL_WALK_EXCLUDES_TELEPORT' $unManifest '0' '' @('-Unblock')
 Check unblock-extra-retry ($unGood.Replace('new_retry=0','new_retry=1')) 1 'UNBLOCK_INDEPENDENT_SAME_ACTION_SUBMISSION' $unManifest '0' '' @('-Unblock')
 Check unblock-no-purchase ($unGood.Replace('[VEHICLE_PRESET_PURCHASE]','')) 1 'UNBLOCK_ACTUAL_PURCHASE_NO_EXHAUSTION' $unManifest '0' '' @('-Unblock')
 Check unblock-fallback ($unGood.Replace('[VEHICLE_PRESET_PURCHASE]','[VEHICLE_PRESET_PURCHASE] reason=SPAWN_STAGING_ACTIONS_EXHAUSTED')) 1 'UNBLOCK_ACTUAL_PURCHASE_NO_EXHAUSTION' $unManifest '0' '' @('-Unblock')
 $unEarly=$unGood.Replace('UNBLOCK_NATIVE_RALLY_BEFORE_DEBIT','SWAP').Replace('UNBLOCK_ACTUAL_NATIVE_WALK','UNBLOCK_NATIVE_RALLY_BEFORE_DEBIT').Replace('SWAP','UNBLOCK_ACTUAL_NATIVE_WALK')
 Check unblock-rally-before-walk $unEarly 1 'UNBLOCK_WALK_RALLY_BUY_ORDER' $unManifest '0' '' @('-Unblock')
 Check unblock-no-cli $good 1 'UNBLOCK_CLI' $manifest '0' '' @('-Unblock')
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $allowed=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-manual-staging-log-*'){throw 'Unsafe contract cleanup path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Manual staging log controls checks=$checks"
