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
function Number([string]$value){[double]::Parse($value,[Globalization.CultureInfo]::InvariantCulture)}
function Events([string]$name){@([regex]::Matches($log,'\[TCR_'+$name+'\] t_ms=(\d+) step=(\d+) ([^\r\n]+)'))}
function Fields($event){$values=@{};foreach($field in [regex]::Matches($event.Groups[3].Value,'(\w+)=(<[^>]*>|\S+)')){$values[$field.Groups[1].Value]=$field.Groups[2].Value};$values}
Require (([IO.File]::ReadAllText($NativeExitPath)).Trim() -eq '0') 'NATIVE_EXIT'
Require ($log.Contains('Game destroyed.') -and $log.Contains('[ROSTER_READY]')) 'FULL_STOPPED_ROSTER'
Require ($all -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation') 'FULL_LOG_FATAL'
Require ($all -notmatch "Unknown class 'SCR_AIGoalReaction_Get(?:In|Out)Vehicle'") 'NATIVE_REACTION_METADATA'
Require ($log -notmatch '\[TCR_REJECT\]') 'IDENTITY_REJECTED'
$timeline=@([regex]::Matches($log,'\[TCR_\w+\] t_ms=(\d+) step=\d+ [^\r\n]+'))
$previousTick=-1L
foreach($receipt in $timeline){
 $tick=[long]$receipt.Groups[1].Value
 Require ($tick -ge $previousTick) 'MONOTONIC_NATIVE_TICKS'
 $previousTick=$tick
}
$manifest=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
Require ($manifest.Count -eq 1) 'MANIFEST'
if($manifest.Count -eq 1){
 $argv=@(($manifest[0].Groups[1].Value|ConvertFrom-Json).arguments)
 foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfTransportCombatResume')){
  $index=[array]::IndexOf($argv,$flag)
  Require ($index -ge 0 -and $index+1 -lt $argv.Count -and $log.Contains($flag+' '+$argv[$index+1])) 'CLI'
 }
 $index=[array]::IndexOf($argv,'-aicfTransportCombatResume')
 Require ($index -ge 0 -and $argv[$index+1] -eq '1') 'PROBE_CLI'
}
$events=@{}
foreach($name in @('ENV','GOAL','ARM','SELECTED','RELEASE','DESELECTED','RESUMED','ARRIVAL','FOOT','DONE')){
 $rows=@(Events $name)
 Require ($rows.Count -eq 1) ('SINGLE_'+$name)
 if($rows.Count -eq 1){$events[$name]=@{match=$rows[0];fields=(Fields $rows[0])}}
}
$checks=@(Events 'CHECK')
Require (@($checks|Where-Object{(Fields $_).passed -ne '1'}).Count -eq 0) 'FAILED_CHECK'
foreach($rule in @('TRUCE','GROUP_CREATED','RECIPE','INITIAL_ORDER','EXACT_BOARDING','ARRIVAL','NORMAL_COMPLETE','FOOT_ARRIVAL')){
 Require (@($checks|Where-Object{(Fields $_).rule -eq $rule}).Count -eq 1) ('RULE_'+$rule)
}
$members=@(Events 'MEMBER')
Require ($members.Count -eq 5 -and @($members|ForEach-Object{(Fields $_).position}|Sort-Object -Unique).Count -eq 5 -and @($members|ForEach-Object{(Fields $_).actor}|Sort-Object -Unique).Count -eq 5) 'FIVE_UNIQUE_MEMBERS'
if($events.ENV){$envFields=$events.ENV.fields;Require ($envFields.mode -eq 'FAULT_INJECTED' -and $envFields.truce -eq '1' -and $envFields.hold_fire -eq '1' -and $envFields.damage_protection -eq '1' -and $envFields.normal_war -eq '0' -and $envFields.teleport -eq '0') 'EXPLICIT_FAULT_INJECTION'}
if($events.DONE){Require ($events.DONE.fields.failures -eq '0' -and [int]$events.DONE.fields.checks -eq $checks.Count -and $events.DONE.fields.gate_cleared -eq '1') 'DONE_CHECKS_AND_CLEANUP'}
$names=@('GOAL','ARM','SELECTED','RELEASE','DESELECTED','RESUMED','ARRIVAL','FOOT','DONE')
for($index=1;$index -lt $names.Count;$index++){
 if($events.ContainsKey($names[$index-1]) -and $events.ContainsKey($names[$index])){Require ($events[$names[$index]].match.Index -gt $events[$names[$index-1]].match.Index) 'EVENT_ORDER'}
}
if(@($names|Where-Object{!$events.ContainsKey($_)}).Count -eq 0){
 $goal=$events.GOAL.fields;$arm=$events.ARM.fields;$selected=$events.SELECTED.fields;$release=$events.RELEASE.fields;$resumed=$events.RESUMED.fields;$arrival=$events.ARRIVAL.fields;$foot=$events.FOOT.fields
 $operation=$arm.operation_id
 $selectedTick=[long]$events.SELECTED.match.Groups[1].Value
 $releaseTick=[long]$events.RELEASE.match.Groups[1].Value
 $deselectedTick=[long]$events.DESELECTED.match.Groups[1].Value
 Require ($releaseTick-$selectedTick -ge 90000 -and $releaseTick-$selectedTick -eq [long]$release.hold_ms -and [long]$release.stationary_ms -le $releaseTick-$selectedTick) 'REAL_HOLD_TICK_SPAN'
 Require ($deselectedTick-$releaseTick -ge 0 -and $deselectedTick-$releaseTick -le 10000 -and $deselectedTick-$releaseTick -eq [long]$events.DESELECTED.fields.elapsed_ms) 'REAL_DESELECTION_TICK_SPAN'
 Require ([long]$events.RESUMED.match.Groups[1].Value-$releaseTick -le 150000 -and [long]$events.SELECTED.match.Groups[1].Value-[long]$events.ARM.match.Groups[1].Value -le 15000) 'REAL_SELECTION_AND_RESUME_BOUNDS'
 Require ((Number $arm.warm_displacement_m) -ge 25 -and [int]$arm.warm_moving_polls -ge 5) 'NATIVE_WARMUP_DRIVE'
 foreach($name in @('SELECTED','RELEASE','DESELECTED','RESUMED','ARRIVAL','FOOT')){Require ($events[$name].fields.operation_id -eq $operation) 'SAME_OPERATION'}
 Require ((Number $goal.initial_distance_m) -ge 650 -and $arm.target -eq $goal.target -and $arrival.target -eq $goal.target -and $foot.target -eq $goal.target) 'UNCHANGED_REMOTE_TARGET'
 Require ($goal.intent_revision -eq $arm.intent_revision -and $resumed.intent_revision -eq $arm.intent_revision -and $arrival.intent_revision -eq $arm.intent_revision -and $foot.intent_revision -eq $arm.intent_revision) 'UNCHANGED_INTENT'
 Require ($arm.vehicle -eq $resumed.vehicle -and $arm.vehicle -eq $arrival.vehicle -and [int]$arm.lease_generation -gt 0 -and [int]$arm.vehicle_generation -gt 0) 'SAME_PHYSICAL_LEASE'
 Require ($arm.deadline_ms -eq $selected.deadline_ms -and $arm.deadline_ms -eq $release.deadline_ms -and $arm.deadline_ms -eq $resumed.deadline_ms) 'FIXED_ABSOLUTE_DEADLINE'
 Require ([int]$release.hold_ms -ge 90000 -and [int]$release.stationary_ms -ge 60000 -and [int]$release.stationary_ms -le [int]$release.hold_ms -and [int]$release.motion_clock -eq [int]$selected.motion_clock -and [int]$release.progress_clock -eq [int]$selected.progress_clock) 'LONG_HOLD_REAL_CLOCKS'
 $holds=@(Events 'HOLD');Require ($holds.Count -ge 15) 'CONTINUOUS_HOLD_SAMPLES'
 $last=-1
 foreach($hold in $holds){
  $fields=Fields $hold
  $holdTick=[long]$hold.Groups[1].Value
  Require ($hold.Index -gt $events.SELECTED.match.Index -and $hold.Index -lt $events.RELEASE.match.Index -and $fields.operation_id -eq $operation -and $fields.exact_current -eq '1') 'HOLD_OWNER_AND_ORDER'
  Require ($holdTick-$selectedTick -eq [long]$fields.hold_ms -and $holdTick -ge $selectedTick -and $holdTick -le $releaseTick -and [long]$fields.stationary_ms -le $holdTick-$selectedTick) 'REAL_HOLD_SAMPLE_TICKS'
  Require ($fields.deadline_ms -eq $arm.deadline_ms -and $fields.motion_clock -eq $selected.motion_clock -and $fields.progress_clock -eq $selected.progress_clock -and [int]$fields.hold_ms -gt $last -and [int]$fields.stationary_ms -le [int]$fields.hold_ms) 'HOLD_CLOCKS'
  $last=[int]$fields.hold_ms
 }
 Require ([int]$events.DESELECTED.fields.elapsed_ms -le 10000 -and $events.DESELECTED.fields.policy_current -eq '0') 'NATIVE_DESELECTION'
 Require ((Number $resumed.displacement_m) -ge 100 -and [int]$resumed.moving_polls -ge 5 -and $resumed.exact_seats -eq '5' -and $resumed.native_route -eq '1') 'REAL_RESUME_DRIVE'
 Require ((Number $arrival.distance_m) -le (Number $goal.dismount_radius_m)+10 -and (Number $foot.farthest_m) -le 35 -and $foot.alive -eq '5') 'PHYSICAL_ARRIVAL_AND_FOOT'
 $escaped=[regex]::Escape($operation)
 $started=@([regex]::Matches($log,'\[TRANSPORT_NATIVE_COMBAT_STARTED\][^\r\n]*operation_id='+$escaped+'(?:\s|$)[^\r\n]*'))
 $ended=@([regex]::Matches($log,'\[TRANSPORT_NATIVE_COMBAT_ENDED\][^\r\n]*operation_id='+$escaped+'(?:\s|$)[^\r\n]*reason=ACTIVITY_OR_CONTEXT_CHANGED'))
 Require ($started.Count -eq 1 -and $started[0].Index -gt $events.ARM.match.Index -and $started[0].Index -lt $events.SELECTED.match.Index) 'PRODUCTION_NATIVE_START_ORDER'
 Require ($ended.Count -eq 1 -and $ended[0].Index -gt $events.RELEASE.match.Index -and $ended[0].Index -lt $events.RESUMED.match.Index) 'NATURAL_HOLD_END_ORDER'
 $dismount=@([regex]::Matches($log,'\[DISEMBARK_COMPLETE\][^\r\n]*operation_id='+$escaped+'(?:\s|$)[^\r\n]*logical_occupants=0[^\r\n]*transitions=0[^\r\n]*inside_bounds=0'))
 $complete=@([regex]::Matches($log,'\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id='+$escaped+'(?:\s|$)[^\r\n]*previous_state=HANDOFF state=COMPLETE'))
 Require ($dismount.Count -eq 1 -and $dismount[0].Index -gt $events.ARRIVAL.match.Index -and $dismount[0].Index -lt $events.FOOT.match.Index) 'PHYSICAL_DISMOUNT_ORDER'
 Require ($complete.Count -eq 1 -and $dismount.Count -eq 1 -and $complete[0].Index -gt $dismount[0].Index -and $complete[0].Index -lt $events.FOOT.match.Index) 'NORMAL_COMPLETE_ORDER'
}
Require ($log -notmatch '\[ACCEPTANCE_FAILURE_LATCHED\]|\[VEHICLE_STATE_CHANGED\][^\r\n]*state=(?:FALLBACK|FAILED_CLOSED)|\[VEHICLE_UNSTUCK_|\[VEHICLE_RECOVERY_DETECTED\]|\[VEHICLE_RECOVERY_DEFERRED_EXHAUSTED\]') 'NO_FALLBACK_OR_MOBILITY_RECOVERY'
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
'PASS FAULT_INJECTED native combat hold90s/release/resume100m/5polls/normal completion/all5 foot; no NORMAL WAR claim'
exit 0
