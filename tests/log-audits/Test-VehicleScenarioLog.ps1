param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$LauncherPath,
 [Parameter(Mandatory)][string]$NativeExitPath,
 [Parameter(Mandatory)][ValidateSet('uaz5','ural5','armed3','solo1','retarget5','repeat5')][string]$Scenario,
 [ValidateSet('US','USSR')][string]$Faction='USSR',
 [switch]$Combat,
 [switch]$DismountBudgetProbe
)
$ErrorActionPreference='Stop'
$log=[IO.File]::ReadAllText($LogPath)
$all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$launcher=[IO.File]::ReadAllText($LauncherPath)
$failures=[Collections.Generic.List[string]]::new()
function Require([bool]$condition,[string]$rule){if(!$condition){$failures.Add($rule)}}
function Number([string]$value){[double]::Parse($value,[Globalization.CultureInfo]::InvariantCulture)}
function Field([string]$line,[string]$key){[regex]::Match($line,'(?:^| )'+[regex]::Escape($key)+'=([^\s]+)').Groups[1].Value}
Require ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -eq '0') 'NATIVE_EXIT'
Require ($log.Contains('Game destroyed.') -and $log.Contains('[ROSTER_READY]')) 'FULL_STOPPED_ROSTER'
if($Combat){Require ($log -match '\[VEHICLE_SCENARIO_ENV\] scenario='+$Scenario+' combat=1 combat_mode=FIRE_AT_WILL') 'COMBAT_ENVIRONMENT'}
else{Require ($log -match '\[VEHICLE_SCENARIO_ENV\] scenario='+$Scenario+' combat=0 combat_mode=HOLD_FIRE damage_protection=1 truce=1') 'TRANSPORT_ONLY_ENVIRONMENT'}
Require ($all -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation') 'FULL_LOG_FATAL'
Require ($all -notmatch "Unknown class 'SCR_AIGoalReaction_GetOutVehicle'") 'NATIVE_REACTION_METADATA'
$manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
Require ($manifests.Count -eq 1) 'MANIFEST'
if($manifests.Count -eq 1){
 $argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
 foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehicleScenario')){
  $i=[array]::IndexOf($argv,$flag)
  Require ($i -ge 0 -and $i+1 -lt $argv.Count -and $log.Contains($flag+' '+$argv[$i+1])) 'CLI'
 }
 $i=[array]::IndexOf($argv,'-aicfVehicleScenario')
 Require ($i -ge 0 -and $argv[$i+1] -eq $Scenario) 'SCENARIO_CLI'
 $factionArg=[array]::IndexOf($argv,'-aicfVehicleScenarioFaction')
 $selectedScope=$factionArg -ge 0 -or $Faction -eq 'US'
 if($selectedScope){Require ($factionArg -ge 0 -and $argv[$factionArg+1] -eq $Faction -and $log -match ('(?:^|\s)-aicfVehicleScenarioFaction '+[regex]::Escape($Faction)+'(?:\s|$)')) 'FACTION_CLI'}
 if($DismountBudgetProbe){
  $i=[array]::IndexOf($argv,'-aicfVehicleScenarioDismountBudget')
  Require ($Scenario -eq 'ural5' -and !$Combat -and $i -ge 0 -and $argv[$i+1] -eq '1') 'DISMOUNT_BUDGET_CLI'
 }
}
# New production telemetry makes the split limit explicit. A batch aggregate
# up to6 is valid only with both per-operation counters <=3 and matching sum.
foreach($line in ($log -split "`n")){
 if($line -notmatch '\[(?:DISEMBARK_[A-Z_]+|FALLBACK_FORCE_DISEMBARK)\]' -or !$line.Contains(' force_attempts=')){continue}
 $exit=Field $line 'force_exit_attempts';$physical=Field $line 'physical_relocation_attempts';$aggregate=Field $line 'force_attempts'
 $present=$exit -match '^\d+$' -and $physical -match '^\d+$' -and $aggregate -match '^\d+$'
 Require $present 'DISMOUNT_OPERATION_BUDGET_METADATA'
 if($present){Require ([int]$exit -le 3 -and [int]$physical -le 3 -and [int]$aggregate -eq ([int]$exit+[int]$physical) -and [int]$aggregate -le 6) 'DISMOUNT_OPERATION_BOUNDS'}
 Require ((Field $line 'maximum_force_exit_attempts') -eq '3' -and (Field $line 'maximum_physical_relocation_attempts') -eq '3' -and (Field $line 'maximum_combined_batches_per_phase') -eq '6') 'DISMOUNT_DECLARED_TOTAL_BOUND'
}
$legs=1;$crew=5
if($Scenario -eq 'repeat5'){$legs=2}
if($Scenario -eq 'armed3'){$crew=3}
if($Scenario -eq 'solo1'){$crew=1}
$selection=$null
if($selectedScope){
 $selections=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_SELECTION\] scenario='+$Scenario+' faction=(\S+) actual_faction=(\S+) prefab=(\S+) native_seats=(\d+) native_cost=(\d+)'))
 Require ($selections.Count -eq 1) 'SELECTED_NATIVE_CATALOG'
 if($selections.Count -eq 1){
  $selection=$selections[0]
  Require ($selection.Groups[1].Value -eq $Faction -and $selection.Groups[2].Value -eq $Faction -and [int]$selection.Groups[4].Value -ge $crew -and [int]$selection.Groups[5].Value -gt 0) 'SELECTED_FACTION_AND_NATIVE_RECIPE'
  if($Faction -eq 'US'){Require ($Scenario -eq 'repeat5' -and $selection.Groups[3].Value.Contains('/M1025_') -and $selection.Groups[3].Value.Contains('M2HB') -and [int]$selection.Groups[4].Value -eq 5) 'US_EXACT5_HMMWV_MODEL'}
 }
}
$checks=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_CHECK\] scenario='+$Scenario+' leg=(\d+) rule=(\S+) passed=(\d)'))
Require (@($checks|Where-Object{$_.Groups[3].Value -ne '1'}).Count -eq 0) 'FAILED_CHECK'
$done=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_DONE\] scenario='+$Scenario+' checks=(\d+) failures=0 legs='+$legs))
Require ($done.Count -eq 1 -and [int]$done[0].Groups[1].Value -eq $checks.Count) 'COMPLETION'
foreach($rule in @('GROUP_CREATED','INITIAL_COHESION_POLICY','EXACT_RECIPE','INITIAL_ORDER','WAITING_MOUNTED','REMOTE_ORDER','PURCHASE_LIMIT')){
 Require (@($checks|Where-Object{$_.Groups[2].Value -eq $rule}).Count -eq 1) ('CASE_'+$rule)
}
Require ($log -match '\[VEHICLE_SCENARIO_COHESION\] scenario='+$Scenario+' [^\r\n]*formation=Column displacement=0 native_readback=1 identity_current=1 setup_parity_only=1 causal_foot_fix_proven=0') 'INITIAL_COHESION_NATIVE_READBACK'
if($Scenario -eq 'retarget5'){Require (@($checks|Where-Object{$_.Groups[2].Value -eq 'FAR_RETARGET'}).Count -eq 1) 'FAR_RETARGET'}
if($Scenario -eq 'repeat5'){
 foreach($rule in @('REPEAT_ORDER','EXPLICIT_READMISSION')){Require (@($checks|Where-Object{$_.Groups[2].Value -eq $rule}).Count -eq 1) $rule}
}
$priorOperation='';$priorVehicle='';$priorGroup='';$priorMembers=''
for($leg=1;$leg -le $legs;$leg++){
 foreach($rule in @('EXACT_BOARDING','GATE_RETURNS_COMPLETE','ARRIVAL_AT_ORIGINAL_GOAL','REAL_TRAVEL','NORMAL_DISMOUNT','FOOT_ORDER_RESTORED','ALL_MEMBERS_AT_POINT')){
  Require (@($checks|Where-Object{[int]$_.Groups[1].Value -eq $leg -and $_.Groups[2].Value -eq $rule}).Count -eq 1) "LEG_${leg}_$rule"
 }
 $prefix='scenario='+$Scenario+' leg='+$leg
 $boarding=[regex]::Match($log,'\[VEHICLE_SCENARIO_BOARDING\] '+$prefix+' operation_id=(\S+) vehicle=(\S+) group=(\S+) crew=(\d+)')
 Require ($boarding.Success -and [int]$boarding.Groups[4].Value -eq $crew) "LEG_${leg}_BOARDING"
 $goals=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_GOAL\] '+$prefix+' target=(<[^>]+>) intent_revision=(\d+) vehicle_start=<[^>]+> dismount_radius_m=([\d.]+)'))
 $goalCount=1;if($Scenario -eq 'retarget5'){$goalCount=2}
 Require ($goals.Count -eq $goalCount) "LEG_${leg}_GOALS"
 $arrival=[regex]::Match($log,'\[VEHICLE_SCENARIO_ARRIVAL\] '+$prefix+' vehicle=(\S+) distance_m=([\d.]+) displacement_m=([\d.]+) moving_polls=(\d+) intent_revision=(\d+) goal=(<[^>]+>)')
 $foot=[regex]::Match($log,'\[VEHICLE_SCENARIO_FOOT_ARRIVAL\] '+$prefix+' farthest_m=([\d.]+) alive=(\d+) goal=(<[^>]+>)')
 Require ($arrival.Success -and $foot.Success) "LEG_${leg}_ARRIVAL_RECEIPTS"
 if($arrival.Success -and $foot.Success -and $goals.Count -and $boarding.Success){
  $goal=$goals[-1]
  Require ((Number $arrival.Groups[2].Value) -le (Number $goal.Groups[3].Value)+10 -and (Number $arrival.Groups[3].Value) -ge 100 -and [int]$arrival.Groups[4].Value -ge 5) "LEG_${leg}_PHYSICAL_DRIVE"
  Require ($arrival.Groups[1].Value -eq $boarding.Groups[2].Value -and $arrival.Groups[5].Value -eq $goal.Groups[2].Value -and $arrival.Groups[6].Value -eq $goal.Groups[1].Value -and $foot.Groups[3].Value -eq $goal.Groups[1].Value) "LEG_${leg}_UNCHANGED_GOAL_AND_VEHICLE"
  Require ((Number $foot.Groups[1].Value) -le 35 -and [int]$foot.Groups[2].Value -eq $crew -and $foot.Index -gt $arrival.Index) "LEG_${leg}_FULL_FOOT_ARRIVAL"
  $operation=[regex]::Escape($boarding.Groups[1].Value)
  $occupants=@([regex]::Matches($log,'\[VEHICLE_PRESET_OCCUPANT\][^\r\n]*?operation='+$operation+' revision=\d+ position=(\d+) seat=(\S+) member=(\S+) occupant=(\S+)'))
  Require ($occupants.Count -eq $crew -and @($occupants|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq $crew) "LEG_${leg}_EXACT_OCCUPANTS"
  foreach($occupant in $occupants){Require ($occupant.Groups[3].Value -eq $occupant.Groups[4].Value -and $occupant.Index -lt $boarding.Index) "LEG_${leg}_MEMBER_SEAT"}
  if($selection){
   Require ($boarding.Groups[1].Value.StartsWith('trip-'+$Faction+'-')) "LEG_${leg}_SELECTED_OPERATION"
   $purchases=@([regex]::Matches($log,'\[VEHICLE_PRESET_PURCHASE\][^\r\n]*? operation='+$operation+' prefab=(\S+) supplies=(\d+)'))
   Require ($purchases.Count -eq 1) "LEG_${leg}_REAL_DEBIT"
   if($purchases.Count -eq 1){Require ($purchases[0].Groups[1].Value -eq $selection.Groups[3].Value -and $purchases[0].Groups[2].Value -eq $selection.Groups[5].Value -and $purchases[0].Index -lt $boarding.Index) "LEG_${leg}_SELECTED_MODEL_DEBIT"}
   $spawns=@([regex]::Matches($log,'\[VEHICLE_SPAWNED\][^\r\n]*')|Where-Object{(Field $_.Value 'operation_id') -eq $boarding.Groups[1].Value -and (Field $_.Value 'faction') -eq $Faction -and (Field $_.Value 'prefab') -eq $selection.Groups[3].Value})
   Require ($spawns.Count -eq 1) "LEG_${leg}_SELECTED_NATIVE_SPAWN"
  }
  $currentMembers=($occupants|Sort-Object {$_.Groups[1].Value}|ForEach-Object {$_.Groups[3].Value}) -join ','
  if($leg -gt 1){Require ($boarding.Groups[1].Value -ne $priorOperation -and $boarding.Groups[2].Value -ne $priorVehicle -and $boarding.Groups[3].Value -eq $priorGroup -and $currentMembers -eq $priorMembers) 'REPEAT_NEW_ASSET_SAME_ORIGINAL_ROSTER'}
  $priorOperation=$boarding.Groups[1].Value;$priorVehicle=$boarding.Groups[2].Value;$priorGroup=$boarding.Groups[3].Value;$priorMembers=$currentMembers
  $gates=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_GATE\] '+$prefix+' event=(STARTED|RETURNED) member=(\S+) seat=(\S+) elapsed_ms=(\d+)'))
  $pending=@{}
  foreach($gate in $gates){
   $key=$gate.Groups[2].Value+'|'+$gate.Groups[3].Value
   Require ($gate.Index -gt $boarding.Index -and $gate.Index -lt $arrival.Index) "LEG_${leg}_GATE_ORDER"
   if($gate.Groups[1].Value -eq 'STARTED'){Require (!$pending.ContainsKey($key)) "LEG_${leg}_GATE_DUPLICATE";$pending[$key]=$true}
   else{Require ($pending.ContainsKey($key) -and [int]$gate.Groups[4].Value -le 60000) "LEG_${leg}_GATE_RETURN";$pending.Remove($key)}
  }
  Require ($pending.Count -eq 0) "LEG_${leg}_GATE_UNRETURNED"
  $complete=[regex]::Match($log,'\[DISEMBARK_COMPLETE\][^\r\n]*operation_id='+$operation+'[^\r\n]*logical_occupants=0[^\r\n]*transitions=0[^\r\n]*inside_bounds=0')
  Require ($complete.Success -and $complete.Index -gt $arrival.Index -and $complete.Index -lt $foot.Index) "LEG_${leg}_PHYSICAL_DISMOUNT"
  Require ($log -match '\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id='+$operation+'[^\r\n]*previous_state=HANDOFF state=COMPLETE') "LEG_${leg}_NORMAL_COMPLETE"
  $group=[regex]::Escape($boarding.Groups[3].Value)
  Require ($log -match '\[GROUP_TRACE\][^\r\n]*group='+$group+'[^\r\n]*target_pos=') "LEG_${leg}_GROUP_DIAGNOSTICS"
  $members=@([regex]::Matches($log,'\[MEMBER_TRACE\][^\r\n]*group='+$group+'[^\r\n]*position=(\d+)[^\r\n]*selected_action=\S+ executed_action=\S+'))
  Require (@($members|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq $crew) "LEG_${leg}_MEMBER_DIAGNOSTICS"
 }
}
Require ($log -notmatch '\[ACCEPTANCE_FAILURE_LATCHED\]|\[VEHICLE_STATE_CHANGED\][^\r\n]*previous_state=(?:BOARDING|TRANSIT|DISMOUNT|HANDOFF) state=(?:FALLBACK|FAILED_CLOSED)') 'TRIP_FAILURE'
if($DismountBudgetProbe){
 Require (@($checks|Where-Object{$_.Groups[2].Value -eq 'DISMOUNT_OPERATION_BUDGET_CAUSAL'}).Count -eq 1) 'DISMOUNT_BUDGET_CAUSAL_CHECK'
 $receipts=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_DISMOUNT_BUDGET\][^\r\n]+'))
 $batches=@($receipts|Where-Object{$_.Value.Contains('event=FORCE_BATCH ') -and (Field $_.Value 'charged') -eq '1'})
 $ghost=@($receipts|Where-Object{$_.Value.Contains('event=PHYSICAL_GHOST ')})
 $relocated=@($receipts|Where-Object{$_.Value.Contains('event=NORMAL_RELOCATION ')})
 Require ($batches.Count -eq 3 -and $ghost.Count -eq 1 -and $relocated.Count -ge 1) 'DISMOUNT_BUDGET_NATIVE_BRANCH_RECEIPTS'
 $operation=[regex]::Match($log,'\[VEHICLE_SCENARIO_BOARDING\] scenario=ural5 leg=1 operation_id=(\S+)').Groups[1].Value
 $firstDeadline='';$terminalDeadline='';$expected=0
 foreach($batch in $batches){
  $expected++
  Require ((Field $batch.Value 'force_exit_attempts') -eq [string]$expected -and [int](Field $batch.Value 'native_calls') -gt 0 -and [int](Field $batch.Value 'accepted_calls') -gt 0 -and [int](Field $batch.Value 'logical_before') -gt 0 -and (Field $batch.Value 'physical_relocation_attempts') -eq '0') 'DISMOUNT_BUDGET_REAL_FORCE_CALLS'
  if(!$firstDeadline){$firstDeadline=Field $batch.Value 'normal_deadline_ms';$terminalDeadline=Field $batch.Value 'terminal_deadline_ms'}
 }
 foreach($receipt in @($batches)+@($ghost)+@($relocated)){
  Require ((Field $receipt.Value 'operation_id') -eq $operation -and (Field $receipt.Value 'normal_deadline_ms') -eq $firstDeadline -and (Field $receipt.Value 'terminal_deadline_ms') -eq $terminalDeadline -and [int]$firstDeadline -gt 0 -and [int]$terminalDeadline -gt [int]$firstDeadline) 'DISMOUNT_BUDGET_EXACT_IDENTITY_FIXED_DEADLINES'
 }
 if($ghost.Count -eq 1){
  $g=$ghost[0]
  Require ((Field $g.Value 'force_exit_attempts') -eq '3' -and (Field $g.Value 'logical_occupants') -eq '0' -and (Field $g.Value 'transitions') -eq '0' -and [int](Field $g.Value 'inside_bounds') -gt 0 -and (Field $g.Value 'exact_ghost') -eq '1' -and (Field $g.Value 'physical_relocation_attempts') -eq '0' -and $batches.Count -eq 3 -and $g.Index -gt $batches[-1].Index) 'DISMOUNT_BUDGET_ACTUAL_EGRESS_THEN_HULL_BLOCKER'
 }
 foreach($r in $relocated){
  Require ((Field $r.Value 'force_exit_attempts') -eq '3' -and [int](Field $r.Value 'physical_relocation_attempts') -in 1..3 -and (Field $r.Value 'ghost_moved_clear') -eq '1' -and (Number (Field $r.Value 'ghost_displacement_m')) -gt 1 -and (Field $r.Value 'unchanged_deadlines') -eq '1' -and (Field $r.Value 'terminal_only') -eq '0' -and $ghost.Count -eq 1 -and $r.Index -gt $ghost[0].Index) 'DISMOUNT_BUDGET_NORMAL_PHYSICAL_RECOVERY'
 }
}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
"PASS Vehicle scenario=$Scenario faction=$Faction legs=$legs crew=${crew}: real boarding, unchanged destination, physical arrival/dismount, all members at point, diagnostics, native0/stopped"
exit 0
