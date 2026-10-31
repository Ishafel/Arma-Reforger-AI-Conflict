param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$LauncherPath,
 [Parameter(Mandatory)][string]$NativeExitPath,
 [switch]$Exhaustion,
 [switch]$Unblock
)
$ErrorActionPreference='Stop'
if($Exhaustion -and $Unblock){throw 'Distinct controlled modes cannot be combined'}
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
  foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfManualStagingProbe')){
   $indices=@(for($i=0;$i -lt $argv.Count;$i++){if($argv[$i] -eq $flag){$i}})
   Require ($indices.Count -eq 1 -and $indices[0]+1 -lt $argv.Count -and ![string]::IsNullOrWhiteSpace($argv[$indices[0]+1])) ('CLI_'+$flag)
  }
  $probeIndex=[array]::IndexOf($argv,'-aicfManualStagingProbe')
  Require ($probeIndex -ge 0 -and $probeIndex+1 -lt $argv.Count -and $argv[$probeIndex+1] -eq '1') 'PROBE_VALUE'
 }catch{$failures.Add('MALFORMED_MANIFEST')}
}
$expected=@('CONTROLLED_NATIVE_MOVEMENT_BLOCK','STALE_REQUEST_NO_MUTATION','STALE_PRESET_NO_MUTATION','STALE_RPL_NO_MUTATION','NO_CURRENT_ASSIGNMENT_NO_MUTATION','LOST_ACTION_NO_MUTATION','EXHAUSTED_TWO_OWNED_ATTEMPTS','SHORT_SUBMITTED_ONCE','DUPLICATE_NO_MUTATION','ACTUAL_SHORT_TELEPORT','ALL_FIVE_PHYSICAL_RALLY_BEFORE_PAYMENT','ORIGINAL_IDENTITIES_HEALTH_UNCHANGED','DEADLINES_UNCHANGED','NO_BUY_OR_DEBIT')
if($Exhaustion){
 $expected=@('EXHAUSTION_DISTANT_NATIVE_ORDER_SET','CONTROLLED_NATIVE_MOVEMENT_BLOCK','EXHAUSTION_TWO_ATTEMPTS_SPENT','EXHAUSTION_LIVE_WAYPOINT_NOT_FALLBACK','EXHAUSTION_MISSING_REISSUE_NOT_FALLBACK','EXHAUSTION_WAYPOINT_REMOVED_FIRST','EXHAUSTION_WAYPOINT_REMOVED_REISSUE','EXHAUSTION_EARLY_TYPED_FALLBACK','EXHAUSTION_PLAN_RELEASED','EXHAUSTION_ORDER_RESTORED','EXHAUSTION_NO_TELEPORT_DEBIT','EXHAUSTION_ORIGINAL_HEALTH','EXHAUSTION_DEADLINE_UNCHANGED')
 Require ($argv -and ([array]::IndexOf($argv,'-aicfManualStagingExhaustionProbe') -ge 0)) 'EXHAUSTION_CLI'
 if($argv){$exIndex=[array]::IndexOf($argv,'-aicfManualStagingExhaustionProbe');Require ($exIndex -ge 0 -and $exIndex+1 -lt $argv.Count -and $argv[$exIndex+1] -eq '1') 'EXHAUSTION_VALUE'}
}
if($Unblock){
 $expected=@('CONTROLLED_NATIVE_MOVEMENT_BLOCK','UNBLOCK_NATIVE_GROUP_WAYPOINT_ABSENT','STALE_REQUEST_NO_MUTATION','STALE_PRESET_NO_MUTATION','STALE_RPL_NO_MUTATION','NO_CURRENT_ASSIGNMENT_NO_MUTATION','LOST_ACTION_NO_MUTATION','EXHAUSTED_TWO_OWNED_ATTEMPTS','SHORT_SUBMITTED_ONCE','DUPLICATE_NO_MUTATION','UNBLOCK_SAME_NATIVE_ACTION_AFTER_SHORT','UNBLOCK_ACTUAL_NATIVE_WALK','UNBLOCK_NATIVE_RALLY_BEFORE_DEBIT','UNBLOCK_NATIVE_SPAWN_AND_DEBIT','UNBLOCK_ORIGINAL_HEALTH_DEADLINES')
 $unblockIndex=[array]::IndexOf($argv,'-aicfManualStagingUnblockProbe')
 Require ($unblockIndex -ge 0 -and $unblockIndex+1 -lt $argv.Count -and $argv[$unblockIndex+1] -eq '1') 'UNBLOCK_CLI'
}
$cases=@([regex]::Matches($log,'\[MANUAL_STAGING_CASE\] case=(\S+) passed=([01]) details=\[([^\r\n]*)\]'))
Require ($cases.Count -ge $expected.Count) 'MINIMUM_CASES'
Require (@($cases|Where-Object{$_.Groups[2].Value -ne '1'}).Count -eq 0) 'FAILED_CASE'
Require (@($cases|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq $cases.Count) 'UNIQUE_CASES'
Require (@([regex]::Matches($log,'\[MANUAL_STAGING_CASE\]')).Count -eq $cases.Count) 'MALFORMED_CASE'
foreach($name in $expected){Require (@($cases|Where-Object{$_.Groups[1].Value -eq $name -and $_.Groups[2].Value -eq '1'}).Count -eq 1) ('CASE_'+$name)}
if($Exhaustion){Require (@($cases|Where-Object{$_.Groups[1].Value -eq 'EXHAUSTION_TWO_ATTEMPTS_SPENT' -and $_.Groups[3].Value.Contains('controlled_all_bounded_placement_rejected=1')}).Count -eq 1) 'EXPLICIT_CONTROLLED_PLACEMENT_FAULT'}
if($Unblock){
 $escape=@([regex]::Matches($log,'\[MANUAL_STAGING_SETUP_ESCAPE\][^\r\n]*'))
 Require ($escape.Count -eq 1 -and $escape[0].Value.Contains('native_usable=1') -and $escape[0].Value.Contains('controlled_escape_precondition=1')) 'UNBLOCK_NATIVE_ESCAPE_PRECONDITION'
 $landing=@($cases|Where-Object{$_.Groups[1].Value -eq 'UNBLOCK_SAME_NATIVE_ACTION_AFTER_SHORT'})
 $walking=@($cases|Where-Object{$_.Groups[1].Value -eq 'UNBLOCK_ACTUAL_NATIVE_WALK'})
 $rally=@($cases|Where-Object{$_.Groups[1].Value -eq 'UNBLOCK_NATIVE_RALLY_BEFORE_DEBIT'})
 $buy=@($cases|Where-Object{$_.Groups[1].Value -eq 'UNBLOCK_NATIVE_SPAWN_AND_DEBIT'})
 if($landing.Count -eq 1 -and $walking.Count -eq 1 -and $rally.Count -eq 1 -and $buy.Count -eq 1){
  $shift=[regex]::Match($landing[0].Groups[3].Value,'actual_3d_correction_m=([0-9.]+)')
  $walk=[regex]::Match($walking[0].Groups[3].Value,'walk_after_correction_m=([0-9.]+)')
  Require ($shift.Success -and [double]$shift.Groups[1].Value -ge 2 -and [double]$shift.Groups[1].Value -le 8 -and $landing[0].Groups[3].Value.Contains('still_outside_rally=1')) 'UNBLOCK_ACTUAL_BOUNDED_LANDING'
  Require ($walk.Success -and [double]$walk.Groups[1].Value -ge 1 -and $walking[0].Groups[3].Value.Contains('same_native_action=1') -and $walking[0].Groups[3].Value.Contains('teleport_excluded=1')) 'UNBLOCK_REAL_WALK_EXCLUDES_TELEPORT'
  Require ($landing[0].Index -lt $walking[0].Index -and $walking[0].Index -lt $rally[0].Index -and $rally[0].Index -lt $buy[0].Index) 'UNBLOCK_WALK_RALLY_BUY_ORDER'
 }
 $submitted=@([regex]::Matches($log,'\[VEHICLE_MANUAL_STAGING_SHORT_SUBMITTED\][^\r\n]*'))
 Require ($submitted.Count -eq 1 -and $submitted[0].Value.Contains('same_native_action=1') -and $submitted[0].Value.Contains('new_retry=0')) 'UNBLOCK_INDEPENDENT_SAME_ACTION_SUBMISSION'
 Require ($log -match '\[VEHICLE_PRESET_PURCHASE\]' -and $log -notmatch 'reason=SPAWN_STAGING_ACTIONS_EXHAUSTED') 'UNBLOCK_ACTUAL_PURCHASE_NO_EXHAUSTION'
}
$done=@([regex]::Matches($log,'\[MANUAL_STAGING_FINISHED\] checks=(\d+) failures=(\d+)'))
Require ($done.Count -eq 1 -and @([regex]::Matches($log,'\[MANUAL_STAGING_FINISHED\]')).Count -eq 1) 'UNIQUE_FINISHED'
if($done.Count -eq 1){
 Require ([int]$done[0].Groups[1].Value -eq $cases.Count -and $done[0].Groups[2].Value -eq '0') 'FINISHED_COUNTS'
 Require ($cases.Count -gt 0 -and $done[0].Index -gt $cases[-1].Index -and $log.IndexOf('Game destroyed.') -gt $done[0].Index) 'FINISHED_BEFORE_STOP'
}
$errors=@([regex]::Matches($all,'(?m)^.*\([EF]\).*$')).Count
$resourceVerdict='PASS';if($errors){$resourceVerdict='FAIL'}
"MANUAL_STAGING_GATE gate=strict_native_resources verdict=$resourceVerdict error_lines=$errors independent_blocker=1"
if($failures.Count){$failures|ForEach-Object {"FAIL $_"};exit 1}
"MANUAL_STAGING_AUDIT verdict=PASS scope=TARGETED_CONTROLLED_FUNCTIONAL_ONLY checks=$($cases.Count) strict_native_resources=$resourceVerdict"
exit 0
