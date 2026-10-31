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
function Number([string]$value){if($value -notmatch '^-?\d+(?:\.\d+)?$'){return [double]::NaN};[double]::Parse($value,[Globalization.CultureInfo]::InvariantCulture)}
Require ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -eq '0') 'NATIVE_EXIT'
Require ($log.Contains('Game destroyed.') -and $log.Contains('[ROSTER_READY]')) 'FULL_STOPPED_ROSTER'
Require ($all -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation') 'FULL_LOG_FATAL'
Require ($log -match '\[VEHICLE_SCENARIO_ENV\] scenario=uaz5 combat=0 combat_mode=HOLD_FIRE damage_protection=1 truce=1') 'CONTROLLED_ENVIRONMENT'
$manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
Require ($manifests.Count -eq 1) 'MANIFEST'
if($manifests.Count -eq 1){
 $argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
 foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehicleScenario','-aicfTransportDamageFallbackProbe')){
  $i=[array]::IndexOf($argv,$flag)
  Require ($i -ge 0 -and $i+1 -lt $argv.Count -and $log.Contains($flag+' '+$argv[$i+1])) 'CLI'
 }
 $probe=[array]::IndexOf($argv,'-aicfTransportDamageFallbackProbe')
 Require ($probe -ge 0 -and $argv[$probe+1] -eq '1') 'PROBE_ENABLED'
}
$checks=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_CHECK\] scenario=uaz5 leg=1 rule=(\S+) passed=(\d)'))
Require (@($checks|Where-Object{$_.Groups[2].Value -ne '1'}).Count -eq 0) 'FAILED_CHECK'
foreach($rule in @('GROUP_CREATED','INITIAL_COHESION_POLICY','EXACT_RECIPE','INITIAL_ORDER','DAMAGE_EXACT_INITIAL_BOARDING','DAMAGE_MOUNTED_HOLD','DAMAGE_REMOTE_ORDER','DAMAGE_NATIVE_WHEEL_INJECTION','DAMAGE_TYPED_FALLBACK','DAMAGE_NO_FALSE_LIFECYCLE_FAILURE','DAMAGE_REAL_OWNER_RECOVERY_VERIFIED','DAMAGE_ORIGINAL_FOOT_ARRIVAL','DAMAGE_NO_REPURCHASE','DAMAGE_EXPLICIT_RETRY','DAMAGE_NEW_EXPLICIT_OPERATION_AND_DEBIT')){
 Require (@($checks|Where-Object{$_.Groups[1].Value -eq $rule}).Count -eq 1) ('CASE_'+$rule)
}
Require ($log -match '\[VEHICLE_SCENARIO_COHESION\] scenario=uaz5 [^\r\n]*formation=Column displacement=0 native_readback=1 identity_current=1 setup_parity_only=1 causal_foot_fix_proven=0') 'INITIAL_COHESION_NATIVE_READBACK'
$events=@{}
$previousIndex=-1
foreach($name in @('BOARDED','MOVING','INJECTED','FALLBACK','RECOVERY_VERIFIED','FOOT','EXPLICIT','RETRY_PURCHASED','DONE')){
 $matches=@([regex]::Matches($log,'\[TRANSPORT_DAMAGE_PROBE\] event='+$name+' [^\r\n]+'))
 Require ($matches.Count -eq 1) ('EVENT_'+$name)
 if($matches.Count -eq 1){
  $events[$name]=$matches[0]
  Require ($matches[0].Index -gt $previousIndex) ('ORDER_'+$name)
  $previousIndex=$matches[0].Index
  Require ((Field $matches[0].Value 'classification') -eq 'CONTROLLED_FAULT_INJECTION' -and (Field $matches[0].Value 'normal_arrival_proof') -eq '0') 'EXPLICIT_CONTROLLED_CLASSIFICATION'
 }
}
if($events.Count -eq 9){
 $board=$events.BOARDED.Value;$moving=$events.MOVING.Value;$inject=$events.INJECTED.Value;$fallback=$events.FALLBACK.Value;$foot=$events.FOOT.Value;$explicit=$events.EXPLICIT.Value;$retry=$events.RETRY_PURCHASED.Value;$done=$events.DONE.Value
 $operation=Field $inject 'operation_id';$escaped=[regex]::Escape($operation)
 foreach($event in $events.Values){Require ((Field $event.Value 'operation_id') -eq $operation) 'EXACT_ORIGINAL_OPERATION'}
 Require ((Field $board 'exact5') -eq '1' -and (Field $board 'debited') -eq '1') 'EXACT_BOARDING_AND_FIRST_DEBIT'
 $occupants=@([regex]::Matches($log,'\[VEHICLE_PRESET_OCCUPANT\] (?:run=\S+ t_ms=\d+ )?operation='+$escaped+' revision=\d+ position=(\d+) seat=(\S+) member=(\S+) occupant=(\S+)'))
 Require ($occupants.Count -eq 5 -and @($occupants|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq 5 -and @($occupants|ForEach-Object{$_.Groups[2].Value}|Sort-Object -Unique).Count -eq 5) 'NATIVE_EXACT_FIVE_SEATS'
 foreach($occupant in $occupants){Require ($occupant.Groups[3].Value -eq $occupant.Groups[4].Value -and $occupant.Index -lt $events.BOARDED.Index) 'NATIVE_EXACT_OCCUPANT'}
 Require ((Number (Field $moving 'displacement_m')) -ge 15 -and (Number (Field $moving 'moving_polls')) -ge 3) 'PHYSICAL_MOVEMENT_BEFORE_INJECTION'
 Require ((Number (Field $inject 'wheels')) -ge 4 -and (Field $inject 'destroyed_wheels') -eq (Field $inject 'wheels') -and (Number (Field $inject 'movement_damage')) -ge 1 -and (Field $inject 'can_move') -eq '0' -and (Field $inject 'exact5_before') -eq '1' -and (Field $inject 'crew_recoveries') -eq '0') 'REAL_WHEEL_DAMAGE_BEFORE_CREW'
 Require ((Field $fallback 'reason') -eq 'VEHICLE_MOVEMENT_DESTROYED' -and (Field $fallback 'phase') -eq 'FALLBACK' -and (Number (Field $fallback 'latency_ms')) -le 10000 -and (Field $fallback 'crew_recoveries') -eq '0') 'TYPED_BOUNDED_FALLBACK'
 Require ((Field $fallback 'lifecycle_failure_latched') -eq '0' -and (Field $fallback 'lifecycle_failure_count') -eq '0') 'NO_FALSE_LIFECYCLE_FAILURE_READBACK'
 $damageLatches=@([regex]::Matches($log,'\[ACCEPTANCE_FAILURE_LATCHED\][^\r\n]*')|Where-Object{(Field $_.Value 'operation_id') -eq $operation -and (Field $_.Value 'reason') -eq 'VEHICLE_MOVEMENT_DESTROYED'})
 Require ($damageLatches.Count -eq 0) 'NO_EXACT_OPERATION_DAMAGE_FAILURE_LATCH'
 Require ((Number (Field $foot 'held_ms')) -ge 30000 -and (Number (Field $foot 'farthest_m')) -le 35 -and (Field $foot 'outside5') -eq '1' -and (Field $foot 'alive') -eq '5' -and (Field $foot 'order_restored') -eq '1') 'UNCHANGED_INTENT_AND_ORIGINAL_FOOT_ARRIVAL'
 foreach($name in @('INJECTED','FALLBACK','RECOVERY_VERIFIED','FOOT','EXPLICIT','RETRY_PURCHASED','DONE')){Require ((Field $events[$name].Value 'intent_revision') -eq (Field $inject 'intent_revision')) 'ORIGINAL_INTENT_STABLE'}
 $recovery=$events.RECOVERY_VERIFIED.Value
 Require ((Field $recovery 'owner') -eq 'ProcessPendingOrderRecovery' -and (Field $recovery 'pending_after') -eq '0' -and (Field $recovery 'direct_clear') -eq '0' -and (Number (Field $recovery 'stable_ms')) -ge (Number (Field $recovery 'required_stable_ms')) -and (Number (Field $recovery 'required_stable_ms')) -ge 10000 -and (Number (Field $recovery 'stable_polls')) -ge 3) 'REAL_OWNER_UNCHANGED_DURABILITY'
 $binding=@([regex]::Matches($log,'\[ORDER_BINDING_STABLE\][^\r\n]*cause=VEHICLE_HANDOFF [^\r\n]*waypoint='+[regex]::Escape((Field $recovery 'waypoint_id'))+'(?: |$)[^\r\n]*'))
 Require ($binding.Count -eq 1 -and $binding[0].Index -gt $events.FALLBACK.Index -and $binding[0].Index -lt $events.RECOVERY_VERIFIED.Index) 'NATIVE_OWNER_BINDING_VERIFICATION_RECEIPT'
 Require ((Field $explicit 'accepted') -eq '1' -and (Field $explicit 'unchanged_intent') -eq '1') 'EXPLICIT_RETRY_ACCEPTED'
 $newOperation=Field $retry 'new_operation'
 Require ($newOperation -and $newOperation -ne $operation -and (Field $retry 'vehicle') -ne (Field $board 'vehicle') -and (Field $retry 'different_vehicle') -eq '1' -and (Field $retry 'debited') -eq '1' -and (Field $retry 'original_roster5') -eq '1' -and (Field $retry 'unchanged_intent') -eq '1' -and (Field $retry 'injection_count') -eq '1') 'EXPLICIT_NEW_OPERATION_AND_VEHICLE'
 Require ((Field $done 'checks') -eq [string]$checks.Count -and (Field $done 'failures') -eq '0' -and (Field $done 'step') -eq '6' -and (Field $done 'injected') -eq '1') 'COMPLETE_CONTROLLED_PROOF'
 Require ($log -match '\[VEHICLE_SCENARIO_DONE\] scenario=uaz5 checks='+$checks.Count+' failures=0 legs=1') 'NATIVE_CLOSE_RECEIPT'
 Require ($log -notmatch '\[VEHICLE_RECOVERY_STARTED\][^\r\n]*operation_id='+$escaped+'(?: |$)') 'NO_CREW_OR_MOBILITY_RECOVERY_AFTER_DESTROYED_WHEELS'
 $nativeFallback=@([regex]::Matches($log,'\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id='+$escaped+'[^\r\n]*reason=VEHICLE_MOVEMENT_DESTROYED[^\r\n]*previous_state=TRANSIT state=FALLBACK'))
 Require ($nativeFallback.Count -eq 1 -and $nativeFallback[0].Index -gt $events.INJECTED.Index) 'PRODUCTION_NATIVE_FALLBACK'
 $purchase=@([regex]::Matches($log,'\[VEHICLE_PRESET_PURCHASE\] (?:run=\S+ t_ms=\d+ )?operation='+$escaped+' prefab=(\S+) supplies=(\d+)'))
 $newPurchase=@([regex]::Matches($log,'\[VEHICLE_PRESET_PURCHASE\] (?:run=\S+ t_ms=\d+ )?operation='+[regex]::Escape($newOperation)+' prefab=(\S+) supplies=(\d+)'))
 Require ($purchase.Count -eq 1 -and $newPurchase.Count -eq 1) 'EXACT_TWO_REAL_PURCHASES'
 if($purchase.Count -eq 1 -and $newPurchase.Count -eq 1){Require ($purchase[0].Index -lt $events.INJECTED.Index -and $newPurchase[0].Index -gt $events.EXPLICIT.Index -and [int]$purchase[0].Groups[2].Value -gt 0 -and $purchase[0].Groups[2].Value -eq $newPurchase[0].Groups[2].Value -and $purchase[0].Groups[1].Value -eq $newPurchase[0].Groups[1].Value) 'PAID_EXACT_MODEL_ONLY_AFTER_EXPLICIT_RETRY'}
 $between=$log.Substring($events.FALLBACK.Index,$events.EXPLICIT.Index-$events.FALLBACK.Index)
 Require ($between -notmatch '\[VEHICLE_SPAWNED\][^\r\n]*faction=USSR [^\r\n]*numeric_slot=0 ') 'NO_REPURCHASE_BEFORE_EXPLICIT'
 foreach($memberEvent in @('BOARD','FOOT')){
  $members=@([regex]::Matches($log,'\[TRANSPORT_DAMAGE_MEMBER\] event='+$memberEvent+' position=(\d+) member=(\S+)'))
  Require ($members.Count -eq 5 -and @($members|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq 5 -and @($members|ForEach-Object{$_.Groups[2].Value}|Sort-Object -Unique).Count -eq 5) ('ORIGINAL_FIVE_'+$memberEvent)
 }
 for($position=0;$position -lt 5;$position++){
  $before=[regex]::Match($log,'\[TRANSPORT_DAMAGE_MEMBER\] event=BOARD position='+$position+' member=(\S+)').Groups[1].Value
  $after=[regex]::Match($log,'\[TRANSPORT_DAMAGE_MEMBER\] event=FOOT position='+$position+' member=(\S+)').Groups[1].Value
  Require ($before -and $before -eq $after) 'IMMUTABLE_ORIGINAL_MEMBER'
 }
}
$resourceErrors=@([regex]::Matches($all,'(?m)^.*\s\(E\).*')).Count
$resourceVerdict='PASS';if($resourceErrors){$resourceVerdict='FAIL'}
"TRANSPORT_DAMAGE_GATE gate=strict_native_resources verdict=$resourceVerdict error_lines=$resourceErrors independent_blocker=1"
if($failures.Count){"TRANSPORT_DAMAGE_AUDIT verdict=FAIL classification=CONTROLLED_FAULT_INJECTION normal_arrival_proof=0 reasons=$($failures -join ',')";exit 1}
"TRANSPORT_DAMAGE_AUDIT verdict=PASS scope=TARGETED_CONTROLLED_FUNCTIONAL_ONLY classification=CONTROLLED_FAULT_INJECTION normal_arrival_proof=0 strict_native_resources=$resourceVerdict"
exit 0
