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
function Field([string]$line,[string]$key){[regex]::Match($line,'(?:^| )'+[regex]::Escape($key)+'=([^\s]+)').Groups[1].Value}
function EntityId([string]$line,[string]$key){
 # EntityID's native formatter may append a separate {} token. Read the full
 # field so malformed/truncated braces cannot hide behind a valid hex prefix.
 $match=[regex]::Match($line,'(?:^| )'+[regex]::Escape($key)+'=([^\r\n]*?)(?= [A-Za-z_][A-Za-z0-9_]*=|$)')
 if(!$match.Success){return ''}
 $token=[regex]::Match($match.Groups[1].Value.Trim(),'^(?<id>[1-9][0-9]*|0x[0-9A-Fa-f]{16})(?: ?\{\})?$')
 if(!$token.Success){return ''}
 $id=$token.Groups['id'].Value
 [uint64]$number=0
 $style=[Globalization.NumberStyles]::None
 $digits=$id
 if($id.StartsWith('0x')){$style=[Globalization.NumberStyles]::AllowHexSpecifier;$digits=$id.Substring(2)}
 if(![uint64]::TryParse($digits,$style,[Globalization.CultureInfo]::InvariantCulture,[ref]$number) -or $number -eq 0){return ''}
 return $id.ToLowerInvariant()
}
Require ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -eq '0') 'NATIVE_EXIT'
Require ($log.Contains('Game destroyed.') -and $log.Contains('[ROSTER_READY]')) 'FULL_STOPPED_ROSTER'
Require ($all -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation') 'FULL_LOG_FATAL'
Require ($log -match '\[VEHICLE_SCENARIO_ENV\] scenario=uaz5 combat=0 combat_mode=HOLD_FIRE damage_protection=1 truce=1') 'CONTROLLED_ENVIRONMENT'
$manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
Require ($manifests.Count -eq 1) 'MANIFEST'
if($manifests.Count -eq 1){
 $argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
 foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-addr')){
  $i=[array]::IndexOf($argv,$flag);Require ($i -ge 0 -and $i+1 -lt $argv.Count -and $argv[$i+1]) ('RUNTIME_ARG_'+$flag)
 }
 foreach($pair in @(@('-aicfVehicleScenario','uaz5'),@('-aicfVehicleScenarioCombat','0'),@('-aicfSpawnApproachFailureProbe','1'))){
  $i=[array]::IndexOf($argv,$pair[0]);Require ($i -ge 0 -and $i+1 -lt $argv.Count -and $argv[$i+1] -eq $pair[1]) ('CLI_'+$pair[0])
 }
 $cli=[regex]::Match($log,'(?m)^.*CLI Params:\s*([^\r\n]+)')
 Require ($cli.Success -and $cli.Groups[1].Value.Trim() -ceq ($argv -join ' ')) 'EXACT_CLI'
 Require ($argv -notcontains '-aicfTransportDamageFallbackProbe') 'ISOLATED_APPROACH_CASE'
}
$events=@{}
foreach($name in @('INJECTED','DONE')){
 $found=@([regex]::Matches($log,'\[SPAWN_APPROACH_FAILURE_PROBE\] event='+$name+'[^\r\n]*'))
 Require ($found.Count -eq 1) ('EVENT_'+$name)
 if($found.Count -eq 1){$events[$name]=$found[0]}
}
if($events.Count -eq 2){
 $injected=$events.INJECTED.Value;$done=$events.DONE.Value
 foreach($line in @($injected,$done)){
  Require ((Field $line 'classification') -eq 'CONTROLLED_FAULT_INJECTION' -and (Field $line 'normal_arrival_proof') -eq '0') 'CONTROLLED_CLASSIFICATION'
 }
 $operation=Field $injected 'operation_id';$escaped=[regex]::Escape($operation)
 Require ($operation -match '^trip-USSR-0-\d+-\d+$' -and (Field $done 'operation_id') -ceq $operation) 'SAME_EXACT_OPERATION'
 Require ((Field $injected 'accepted') -eq '1' -and (Field $injected 'callbacks') -eq '1' -and (Field $injected 'phase') -eq 'APPROACHING_SITE' -and (Field $injected 'plan_pending') -eq '1' -and (Field $injected 'no_lease') -eq '1') 'REAL_CURRENT_APPROACH_PENDING_RECEIPT'
 $waypointId=EntityId $injected 'waypoint_id';$groupId=EntityId $injected 'group'
 Require ([bool]$waypointId -and [bool]$groupId) 'NATIVE_SOURCE_IDENTITY'
 $start=Field $injected 't_ms';$end=Field $done 't_ms';$elapsed=Field $done 'elapsed_ms'
 Require ($start -match '^\d+$' -and $end -match '^\d+$' -and $elapsed -match '^\d+$' -and [long]$end -gt [long]$start -and [long]$elapsed -eq ([long]$end-[long]$start) -and [long]$elapsed -le 120000) 'NEXT_TICK_BOUNDED'
 Require ((Field $done 'phase') -eq 'FALLBACK' -and (Field $done 'reason') -eq 'SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN' -and (Field $done 'plan_released') -eq '1' -and (Field $done 'order_restored') -eq '1' -and (Field $done 'callbacks') -eq '1' -and (Field $done 'failures') -eq '0' -and (Field $done 'original_roster') -eq '1') 'REAL_TYPED_FALLBACK_RELEASE_RESTORE'
 $reports=@([regex]::Matches($log,'\[VEHICLE_SPAWN_APPROACH_MOVE_FAILED\][^\r\n]*operation_id='+$escaped+'(?:\s|$)[^\r\n]*'))
 Require ($reports.Count -eq 1) 'PRODUCTION_REPORT_ONCE'
 if($reports.Count -eq 1){Require ((Field $reports[0].Value 'reason') -eq 'SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN' -and $waypointId -and (EntityId $reports[0].Value 'waypoint') -ceq $waypointId -and $reports[0].Index -lt $events.INJECTED.Index) 'EXACT_PHASE_WAYPOINT_REPORT'}
 $states=@([regex]::Matches($log,'\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id='+$escaped+'(?:\s|$)[^\r\n]*'))
 $fallback=@($states|Where-Object{(Field $_.Value 'previous_state') -eq 'APPROACHING_SITE' -and (Field $_.Value 'state') -eq 'FALLBACK' -and (Field $_.Value 'reason') -eq 'SPAWN_SITE_APPROACH_MOVE_FAILED_UNKNOWN'})
 Require ($fallback.Count -eq 1) 'PRODUCTION_CONTROLLER_FALLBACK'
 if($fallback.Count -eq 1){Require ($fallback[0].Index -gt $events.INJECTED.Index -and $fallback[0].Index -lt $events.DONE.Index) 'REPORT_THEN_TICK_THEN_RESTORE'}
 Require (@($states|Where-Object{(Field $_.Value 'state') -eq 'COMPLETE'}).Count -eq 0) 'FAILED_APPROACH_NEVER_COMPLETE'
 Require ($log -notmatch '\[VEHICLE_PRESET_PURCHASE\][^\r\n]*operation='+$escaped+'\s|\[VEHICLE_SPAWNED\][^\r\n]*operation_id='+$escaped+'(?:\s|$)') 'NO_PURCHASE_OR_SPAWN'
}
foreach($name in @('EXACT_APPROACH_REPORT_ACCEPTED','APPROACH_NEXT_TICK_FALLBACK','APPROACH_PLAN_RELEASED','APPROACH_ORDER_RESTORED','APPROACH_CALLBACK_ONCE')){
 $checks=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_CHECK\] scenario=uaz5 leg=1 rule='+$name+' passed=1(?:\s|$)'))
 Require ($checks.Count -eq 1) ('ACTUAL_CHECK_'+$name)
}
Require ($log -notmatch '\[VEHICLE_SCENARIO_CHECK\][^\r\n]*passed=0') 'NO_FAILED_CHECK'
Require (@([regex]::Matches($log,'\[VEHICLE_SCENARIO_DONE\] scenario=uaz5 checks=\d+ failures=0 legs=1(?:\s|$)')).Count -eq 1) 'PROBE_COMPLETED'
$resourceErrors=@([regex]::Matches($all,'(?m)^.*\s\(E\).*')).Count
$resourceVerdict='PASS';if($resourceErrors){$resourceVerdict='FAIL'}
"SPAWN_APPROACH_FAILURE_GATE gate=strict_native_resources verdict=$resourceVerdict error_lines=$resourceErrors independent_blocker=1"
if($failures.Count){"SPAWN_APPROACH_FAILURE_AUDIT verdict=FAIL classification=CONTROLLED_FAULT_INJECTION normal_arrival_proof=0 reasons=$($failures -join ',')";exit 1}
"SPAWN_APPROACH_FAILURE_AUDIT verdict=PASS scope=TARGETED_CONTROLLED_FUNCTIONAL_ONLY classification=CONTROLLED_FAULT_INJECTION normal_arrival_proof=0 strict_native_resources=$resourceVerdict"
exit 0
