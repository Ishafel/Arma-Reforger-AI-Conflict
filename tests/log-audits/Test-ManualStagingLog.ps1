param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$LauncherPath,
 [Parameter(Mandatory)][string]$NativeExitPath,
 [switch]$Exhaustion
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
 $expected=@('CONTROLLED_NATIVE_MOVEMENT_BLOCK','EXHAUSTION_TWO_ATTEMPTS_SPENT','EXHAUSTION_LIVE_WAYPOINT_NOT_FALLBACK','EXHAUSTION_MISSING_REISSUE_NOT_FALLBACK','EXHAUSTION_WAYPOINT_REMOVED_FIRST','EXHAUSTION_WAYPOINT_REMOVED_REISSUE','EXHAUSTION_EARLY_TYPED_FALLBACK','EXHAUSTION_PLAN_RELEASED','EXHAUSTION_ORDER_RESTORED','EXHAUSTION_NO_TELEPORT_DEBIT','EXHAUSTION_ORIGINAL_HEALTH','EXHAUSTION_DEADLINE_UNCHANGED')
 Require ($argv -and ([array]::IndexOf($argv,'-aicfManualStagingExhaustionProbe') -ge 0)) 'EXHAUSTION_CLI'
 if($argv){$exIndex=[array]::IndexOf($argv,'-aicfManualStagingExhaustionProbe');Require ($exIndex -ge 0 -and $exIndex+1 -lt $argv.Count -and $argv[$exIndex+1] -eq '1') 'EXHAUSTION_VALUE'}
}
$cases=@([regex]::Matches($log,'\[MANUAL_STAGING_CASE\] case=(\S+) passed=([01]) details=\[([^\r\n]*)\]'))
Require ($cases.Count -ge $expected.Count) 'MINIMUM_CASES'
Require (@($cases|Where-Object{$_.Groups[2].Value -ne '1'}).Count -eq 0) 'FAILED_CASE'
Require (@($cases|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq $cases.Count) 'UNIQUE_CASES'
Require (@([regex]::Matches($log,'\[MANUAL_STAGING_CASE\]')).Count -eq $cases.Count) 'MALFORMED_CASE'
foreach($name in $expected){Require (@($cases|Where-Object{$_.Groups[1].Value -eq $name -and $_.Groups[2].Value -eq '1'}).Count -eq 1) ('CASE_'+$name)}
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
