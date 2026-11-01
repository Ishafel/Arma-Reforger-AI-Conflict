param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$LauncherPath,
 [Parameter(Mandatory)][string]$NativeExitPath,
 [Parameter(Mandatory)][ValidateSet('uaz5','ural5','armed3','solo1','retarget5','repeat5')][string]$Scenario,
 [ValidateSet('US','USSR')][string]$Faction='USSR',
 [switch]$Combat,
 [switch]$RetainedBehavior,
 [ValidateSet('LEGACY_COMPLETE','CURRENT_RETAINED')][string]$ReceiptPolicy='LEGACY_COMPLETE',
 [ValidateSet('ROOT_ONLY','HIERARCHY')][string]$ProtectionScope='ROOT_ONLY',
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
# Receipt policy changes only with explicit caller opt-in, native CLI and v2 marker.
$retainedFlags=@($argv|Where-Object{$_ -eq '-aicfVehicleScenarioRetainedBehavior'})
$currentSingle=$ReceiptPolicy -eq 'CURRENT_RETAINED' -and !$RetainedBehavior
$currentFlags=@($argv|Where-Object{$_ -eq '-aicfVehicleScenarioCurrentBehavior'})
$policies=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_POLICY\] scenario=repeat5 version=(2|4) policy=RETAINED_TRANSPORT return_distance_m=300 same_asset=1'))
if($RetainedBehavior){
 $ri=[array]::IndexOf($argv,'-aicfVehicleScenarioRetainedBehavior')
 Require ($Scenario -eq 'repeat5' -and !$Combat -and !$DismountBudgetProbe -and $currentFlags.Count -eq 0 -and $retainedFlags.Count -eq 1 -and $ri+1 -lt $argv.Count -and $argv[$ri+1] -eq '1' -and $policies.Count -eq 1 -and @([regex]::Matches($log,'\[VEHICLE_SCENARIO_POLICY\]')).Count -eq 1) 'EXPLICIT_RETAINED_POLICY_V2'
 Require (@([regex]::Matches($log,'(?m)CLI Params:\s*([^\r\n]+)')).Count -eq 1 -and ([regex]::Match($log,'(?m)CLI Params:\s*([^\r\n]+)').Groups[1].Value.Trim()) -ceq ($argv -join ' ')) 'RETAINED_EXACT_NATIVE_CLI'
}elseif($currentSingle){
 $ci=[array]::IndexOf($argv,'-aicfVehicleScenarioCurrentBehavior')
 Require ($Scenario -ne 'repeat5' -and !$DismountBudgetProbe -and $retainedFlags.Count -eq 0 -and $currentFlags.Count -eq 1 -and $ci+1 -lt $argv.Count -and $argv[$ci+1] -eq '1') 'EXPLICIT_CURRENT_SINGLE_NATIVE_SCOPE'
 Require (@([regex]::Matches($log,'\[VEHICLE_SCENARIO_POLICY\] scenario='+$Scenario+' version=3 policy=CURRENT_RETAINED return_distance_m=300 same_asset=1')).Count -eq 1 -and @([regex]::Matches($log,'\[VEHICLE_SCENARIO_POLICY\]')).Count -eq 1) 'EXPLICIT_CURRENT_POLICY_V3'
 Require (@([regex]::Matches($log,'(?m)CLI Params:\s*([^\r\n]+)')).Count -eq 1 -and ([regex]::Match($log,'(?m)CLI Params:\s*([^\r\n]+)').Groups[1].Value.Trim()) -ceq ($argv -join ' ')) 'CURRENT_EXACT_NATIVE_CLI'
}else{Require ($retainedFlags.Count -eq 0 -and $currentFlags.Count -eq 0 -and $log -notmatch '\[VEHICLE_SCENARIO_POLICY\]') 'LEGACY_RECEIPT_POLICY'}
$hierarchy=$ProtectionScope -eq 'HIERARCHY'
$protections=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_PROTECTION\] ([^\r\n]+)'))
if($hierarchy){
 $indices=@(for($pi=0;$pi -lt $argv.Count;$pi++){if($argv[$pi] -eq '-aicfVehicleScenarioProtectionScope'){$pi}})
 Require ($RetainedBehavior -and !$Combat -and !$DismountBudgetProbe -and $indices.Count -eq 1 -and $indices[0]+1 -lt $argv.Count -and $argv[$indices[0]+1] -ceq 'HIERARCHY') 'HIERARCHY_EXPLICIT_NATIVE_SCOPE'
 Require (@([regex]::Matches($log,'\[VEHICLE_SCENARIO_PROTECTION_POLICY\] scope=HIERARCHY classification=CONTROLLED_RETAINED_CYCLE_ONLY mode=BEFORE_MOVEMENT_ONCE')).Count -eq 1 -and @([regex]::Matches($log,'\[VEHICLE_SCENARIO_PROTECTION_POLICY\]')).Count -eq 1) 'HIERARCHY_CONTROLLED_ONLY_MARKER'
 Require ($protections.Count -eq 6 -and @($protections|Where-Object{(Field $_.Value 'kind') -eq 'MEMBER'}).Count -eq 5 -and @($protections|Where-Object{(Field $_.Value 'kind') -eq 'VEHICLE'}).Count -eq 1) 'HIERARCHY_EXACT_FIVE_MEMBERS_ONE_VEHICLE'
 $firstBoard=[regex]::Match($log,'\[VEHICLE_SCENARIO_BOARDING\] scenario=repeat5 leg=1 operation_id=(\S+) vehicle=(\S+) group=(\S+) crew=5')
 foreach($protection in $protections){
  $line=$protection.Value
  Require ((Field $line 'event') -eq 'APPLIED' -and (Field $line 'scope') -eq 'HIERARCHY' -and (Field $line 'classification') -eq 'CONTROLLED_RETAINED_CYCLE_ONLY' -and (Field $line 'protection_status') -eq 'COVERED' -and (Field $line 'protection_errors') -eq '0' -and (Field $line 'protection_enabled_after') -eq '0' -and (Field $line 'protection_child_zones_enabled') -eq '0' -and (Field $line 'read_only') -eq '1') 'HIERARCHY_ACTUAL_DISABLED_READBACK'
  Require ((Field $line 'protection_zones') -match '^\d+$' -and [int](Field $line 'protection_zones') -gt 0 -and (Field $line 'protection_managers_covered') -match '^\d+$' -and [int](Field $line 'protection_managers_covered') -gt 0 -and [int](Field $line 'protection_child_managers') -eq [int](Field $line 'protection_managers_covered')-1) 'HIERARCHY_NATIVE_COUNTS'
  Require ($firstBoard.Success -and $protection.Index -lt $firstBoard.Index) 'HIERARCHY_BEFORE_FIRST_PHYSICAL_BOARDING'
  if((Field $line 'kind') -eq 'VEHICLE'){Require ((Field $line 'entity') -eq $firstBoard.Groups[2].Value -and (Field $line 'position') -eq '-1' -and [int](Field $line 'protection_child_managers') -gt 0 -and [int](Field $line 'protection_child_zones') -gt 0) 'HIERARCHY_ACTUAL_VEHICLE_CHILD_COVERAGE'}
 }
}else{Require ($protections.Count -eq 0 -and $argv -notcontains '-aicfVehicleScenarioProtectionScope' -and $log -notmatch '\[VEHICLE_SCENARIO_PROTECTION_POLICY\]') 'ROOT_ONLY_RECEIPT_POLICY'}
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
if($RetainedBehavior -or $currentSingle){
 Require ($selection -ne $null) 'RETAINED_EXPLICIT_FACTION_CATALOG'
 # Newly emitted all-factions proof is mandatory for v3 current singles. Old
 # v2 repeat evidence remains readable; present metadata is always verified.
 if(!$Combat -and ($currentSingle -or ($RetainedBehavior -and $policies.Count -eq 1 -and $policies[0].Groups[1].Value -eq '4') -or $log -match '\[VEHICLE_SCENARIO_TRUCE_SUMMARY\]')){
  $truce=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_TRUCE_SUMMARY\] factions=(\d+) pairs=(\d+) expected_pairs=(\d+) unavailable_pairs=0 hostile_pairs=0 classification=CONTROLLED_RETAINED_CYCLE_ONLY native_readback=1'))
  $pairs=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_TRUCE_PAIR\] first=(\S+) second=(\S+) first_friendly=1 second_friendly=1 native_readback=1'))
  Require ($truce.Count -eq 1 -and @([regex]::Matches($log,'\[VEHICLE_SCENARIO_TRUCE_SUMMARY\]')).Count -eq 1) 'CURRENT_ALL_FACTIONS_TRUCE_NATIVE_SUMMARY'
  if($truce.Count -eq 1){$n=[int]$truce[0].Groups[1].Value;$expected=$n*($n-1)/2;$keys=@($pairs|ForEach-Object{(@($_.Groups[1].Value,$_.Groups[2].Value)|Sort-Object)-join '|'});$factions=@($pairs|ForEach-Object{$_.Groups[1].Value;$_.Groups[2].Value}|Sort-Object -Unique);Require ($n -ge 2 -and [int]$truce[0].Groups[2].Value -eq $expected -and [int]$truce[0].Groups[3].Value -eq $expected -and $pairs.Count -eq $expected -and @($keys|Sort-Object -Unique).Count -eq $expected -and $factions.Count -eq $n -and @($pairs|Where-Object{$_.Groups[1].Value -eq $_.Groups[2].Value}).Count -eq 0 -and @([regex]::Matches($log,'\[VEHICLE_SCENARIO_TRUCE_PAIR\]')).Count -eq $pairs.Count) 'CURRENT_ALL_FACTIONS_PAIR_COVERAGE';$firstBoard=[regex]::Match($log,'\[VEHICLE_SCENARIO_BOARDING\] scenario='+$Scenario+' leg=1 ');Require ($firstBoard.Success -and $truce[0].Index -lt $firstBoard.Index -and @($pairs|Where-Object{$_.Index -ge $firstBoard.Index}).Count -eq 0) 'CURRENT_TRUCE_BEFORE_FIRST_BOARDING'}
 }
 if($RetainedBehavior){foreach($rule in @('REPEAT_ORDER','DISTANT_RETURN_INTENT','SAME_ASSET_REBOARD')){Require (@($checks|Where-Object{$_.Groups[2].Value -eq $rule -and $_.Groups[1].Value -eq '2'}).Count -eq 1) ('RETAINED_'+$rule)}}
 Require (@($checks|Where-Object{$_.Groups[2].Value -eq 'SINGLE_PAID_ASSET' -and $_.Groups[1].Value -eq [string]$legs}).Count -eq 1) 'RETAINED_SINGLE_PAID_ASSET'
 if($Scenario -eq 'retarget5'){Require (@($checks|Where-Object{$_.Groups[2].Value -eq 'FAR_RETARGET'}).Count -eq 1) 'CURRENT_FAR_RETARGET'}
 Require ($log -notmatch 'rule=EXPLICIT_READMISSION|SCENARIO_REPEAT_TRIP') 'NO_NEW_ADMISSION'
 $identities=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_RETAINED_IDENTITY\] leg=([12]) operation_id=(\S+) vehicle=(\S+) vehicle_rpl=(\S+) group=(\S+) lease_generation=(\d+) vehicle_generation=(\d+) trip_generation=(\d+)'))
 Require ($identities.Count -eq $legs -and @([regex]::Matches($log,'\[VEHICLE_SCENARIO_RETAINED_IDENTITY\]')).Count -eq $legs -and $identities[0].Groups[1].Value -eq '1') 'RETAINED_IDENTITY_READBACK_COUNT'
 if($identities.Count -eq 2){foreach($field in 2..8){Require ($identities[0].Groups[$field].Value -ceq $identities[1].Groups[$field].Value) 'SAME_TRIP_LEASE_ENTITY_RPL_GROUP_GENERATIONS'}}
 $priorFoot=-1;$priorMembers='';$priorSeats='';$operation='';$vehicle='';$lastFoot=-1
 for($leg=1;$leg -le $legs;$leg++){
  $prefix='scenario='+$Scenario+' leg='+$leg
  foreach($rule in @('EXACT_BOARDING','GATE_RETURNS_COMPLETE','ARRIVAL_AT_ORIGINAL_GOAL','REAL_TRAVEL','NORMAL_DISMOUNT','FOOT_ORDER_RESTORED','ALL_MEMBERS_AT_POINT')){Require (@($checks|Where-Object{$_.Groups[1].Value -eq [string]$leg -and $_.Groups[2].Value -eq $rule}).Count -eq 1) "RETAINED_LEG_${leg}_$rule"}
  $boards=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_BOARDING\] '+$prefix+' operation_id=(\S+) vehicle=(\S+) group=(\S+) crew='+$crew+'(?:\s|$)'))
  $goals=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_GOAL\] '+$prefix+' target=(<[^>]+>) intent_revision=(\d+) vehicle_start=<[^>]+> dismount_radius_m=([\d.]+)'))
  $arrivals=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_ARRIVAL\] '+$prefix+' vehicle=(\S+) distance_m=([\d.]+) displacement_m=([\d.]+) moving_polls=(\d+) intent_revision=(\d+) goal=(<[^>]+>)'))
  $feet=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_FOOT_ARRIVAL\] '+$prefix+' farthest_m=([\d.]+) alive='+$crew+' goal=(<[^>]+>)'))
  $dismounts=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_RETAINED_DISMOUNT\] leg='+$leg+' logical_occupants=0 transitions=0 inside_bounds=0 actual_order=1 same_asset=1'))
  $goalCount=1;if($Scenario -eq 'retarget5'){$goalCount=2}
  Require ($boards.Count -eq 1 -and $goals.Count -eq $goalCount -and $arrivals.Count -eq 1 -and $feet.Count -eq 1 -and $dismounts.Count -eq 1) "RETAINED_LEG_${leg}_UNIQUE_PHYSICAL_RECEIPTS"
  if($boards.Count -ne 1 -or $goals.Count -ne $goalCount -or $arrivals.Count -ne 1 -or $feet.Count -ne 1 -or $dismounts.Count -ne 1){continue}
  $board=$boards[0];$goal=$goals[-1];$arrival=$arrivals[0];$foot=$feet[0]
  Require ($board.Index -gt $priorFoot -and $arrival.Index -gt $board.Index -and $dismounts[0].Index -gt $arrival.Index -and $foot.Index -gt $dismounts[0].Index) "RETAINED_LEG_${leg}_PHYSICAL_SEQUENCE"
  Require ((Number $arrival.Groups[2].Value) -le (Number $goal.Groups[3].Value)+10 -and (Number $arrival.Groups[3].Value) -ge 100 -and [int]$arrival.Groups[4].Value -ge 5 -and (Number $foot.Groups[1].Value) -le 35) "RETAINED_LEG_${leg}_REAL_DRIVE_AND_ALL_FIVE_FOOT"
  Require ($arrival.Groups[1].Value -ceq $board.Groups[2].Value -and $arrival.Groups[5].Value -ceq $goal.Groups[2].Value -and $arrival.Groups[6].Value -ceq $goal.Groups[1].Value -and $foot.Groups[2].Value -ceq $goal.Groups[1].Value) "RETAINED_LEG_${leg}_EXACT_DESTINATION"
  if($identities.Count -eq $legs){$id=$identities[$leg-1];Require ($id.Groups[1].Value -eq [string]$leg -and $board.Groups[1].Value -ceq $id.Groups[2].Value -and $board.Groups[2].Value -ceq $id.Groups[3].Value -and $board.Groups[3].Value -ceq $id.Groups[5].Value) "RETAINED_LEG_${leg}_IDENTITY"}
  $seats=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_RETAINED_SEAT\] leg='+$leg+' position=(\d+) seat=(\S+) member=(\S+) occupant=(\S+)'))
  Require ($seats.Count -eq $crew -and @($seats|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq $crew -and @($seats|ForEach-Object{$_.Groups[2].Value}|Sort-Object -Unique).Count -eq $crew -and @($seats|ForEach-Object{$_.Groups[3].Value}|Sort-Object -Unique).Count -eq $crew) "RETAINED_LEG_${leg}_EXACT_ALL_UNIQUE_SLOTS"
  foreach($seat in $seats){Require ($seat.Groups[3].Value -ceq $seat.Groups[4].Value -and $seat.Index -lt $arrival.Index) "RETAINED_LEG_${leg}_ACTUAL_SEAT_OCCUPANT"}
  if($currentSingle){
   $original=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_ROSTER\] position=(\d+) member=(\S+) group=(\S+) original_capture=1'))
   Require ($original.Count -eq $crew -and @($original|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq $crew -and @($original|ForEach-Object{$_.Groups[2].Value}|Sort-Object -Unique).Count -eq $crew) 'CURRENT_ORIGINAL_CAPTURED_ROSTER'
   foreach($seat in $seats){Require (@($original|Where-Object{$_.Groups[1].Value -eq $seat.Groups[1].Value -and $_.Groups[2].Value -eq $seat.Groups[3].Value -and $_.Groups[3].Value -eq $board.Groups[3].Value -and $_.Index -lt $board.Index}).Count -eq 1) 'CURRENT_ORIGINAL_BODY_BEFORE_BOARDING'}
  }
  if($hierarchy -and $leg -eq 1){foreach($seat in $seats){Require (@($protections|Where-Object{(Field $_.Value 'kind') -eq 'MEMBER' -and (Field $_.Value 'position') -eq $seat.Groups[1].Value -and (Field $_.Value 'entity') -eq $seat.Groups[3].Value}).Count -eq 1) 'HIERARCHY_SAME_ORIGINAL_FIVE_BODIES'}}
  $members=($seats|Sort-Object{[int]$_.Groups[1].Value}|ForEach-Object{$_.Groups[3].Value}) -join ',';$keys=($seats|Sort-Object{[int]$_.Groups[1].Value}|ForEach-Object{$_.Groups[2].Value}) -join ','
  if($leg -eq 2){Require ($members -ceq $priorMembers -and $keys -ceq $priorSeats) 'RETAINED_ORIGINAL_FIVE_AND_SLOTS'}
  $operation=$board.Groups[1].Value;$vehicle=$board.Groups[2].Value
  $nativeDeploy=@([regex]::Matches($log,'\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id='+[regex]::Escape($operation)+'[^\r\n]*previous_state=TRANSIT state=TACTICAL_DEPLOYMENT')|Where-Object{$_.Index -gt $board.Index -and $_.Index -lt $foot.Index -and (Field $_.Value 'reason') -eq 'DESTINATION_REACHED'})
  Require ($nativeDeploy.Count -eq 1) "RETAINED_LEG_${leg}_NATIVE_TACTICAL_TRANSITION"
  $pending=@{}
  foreach($gate in @([regex]::Matches($log,'\[VEHICLE_SCENARIO_GATE\] '+$prefix+' event=(STARTED|RETURNED) member=(\S+) seat=(\S+) elapsed_ms=(\d+)'))){
   $key=$gate.Groups[2].Value+'|'+$gate.Groups[3].Value;Require ($gate.Index -gt $board.Index -and $gate.Index -lt $arrival.Index) "RETAINED_LEG_${leg}_GATE_ORDER"
   if($gate.Groups[1].Value -eq 'STARTED'){Require (!$pending.ContainsKey($key)) 'RETAINED_GATE_DUPLICATE';$pending[$key]=$true}
   else{Require ($pending.ContainsKey($key) -and [int]$gate.Groups[4].Value -le 60000) 'RETAINED_GATE_RETURN';$pending.Remove($key)}
  }
  Require ($pending.Count -eq 0) "RETAINED_LEG_${leg}_GATE_UNRETURNED"
  $group=[regex]::Escape($board.Groups[3].Value);Require ($log -match '\[GROUP_TRACE\][^\r\n]*group='+$group+'[^\r\n]*target_pos=') 'RETAINED_GROUP_DIAGNOSTICS'
  $membersTrace=@([regex]::Matches($log,'\[MEMBER_TRACE\][^\r\n]*group='+$group+'[^\r\n]*position=(\d+)[^\r\n]*selected_action=\S+ executed_action=\S+'));Require (@($membersTrace|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -eq $crew) 'RETAINED_ALL_NATIVE_MEMBER_DIAGNOSTICS'
  $priorFoot=$foot.Index;$lastFoot=$foot.Index;$priorMembers=$members;$priorSeats=$keys
 }
 $returns=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_RETAINED_RETURN\] distance_m=([\d.]+) threshold_m=300 previous_intent=(\d+) intent_revision=(\d+) target=(<[^>]+>)'))
 $returnCount=0;if($RetainedBehavior){$returnCount=1};Require ($returns.Count -eq $returnCount) 'RETAINED_NEW_DISTANT_INTENT_COUNT'
 if($returns.Count -eq 1){$secondGoal=[regex]::Match($log,'\[VEHICLE_SCENARIO_GOAL\] scenario=repeat5 leg=2 target=(<[^>]+>) intent_revision=(\d+)');$firstFoot=[regex]::Match($log,'\[VEHICLE_SCENARIO_FOOT_ARRIVAL\] scenario=repeat5 leg=1 [^\r\n]*');$secondBoard=[regex]::Match($log,'\[VEHICLE_SCENARIO_BOARDING\] scenario=repeat5 leg=2 [^\r\n]*');Require ((Number $returns[0].Groups[1].Value) -gt 300 -and $returns[0].Groups[2].Value -ne $returns[0].Groups[3].Value -and $returns[0].Groups[3].Value -eq $secondGoal.Groups[2].Value -and $returns[0].Groups[4].Value -eq $secondGoal.Groups[1].Value -and $returns[0].Index -gt $firstFoot.Index -and $returns[0].Index -lt $secondBoard.Index) 'RETAINED_DISTANT_INTENT_BEFORE_REBOARD'}
 $purchases=@([regex]::Matches($log,'\[VEHICLE_PRESET_PURCHASE\][^\r\n]*? operation=(\S+) prefab=(\S+) supplies=(\d+)'))
 Require ($purchases.Count -eq 1) 'RETAINED_EXACTLY_ONE_REAL_DEBIT'
 if($purchases.Count -eq 1 -and $selection){Require ($purchases[0].Groups[1].Value -eq $operation -and $operation.StartsWith('trip-'+$Faction+'-') -and $purchases[0].Groups[2].Value -eq $selection.Groups[3].Value -and $purchases[0].Groups[3].Value -eq $selection.Groups[5].Value -and $purchases[0].Index -lt $identities[0].Index) 'RETAINED_SELECTED_MODEL_PAID_ONCE'}
 $spawns=@([regex]::Matches($log,'\[VEHICLE_SPAWNED\][^\r\n]*')|Where-Object{(Field $_.Value 'operation_id') -eq $operation -and (Field $_.Value 'faction') -eq $Faction -and $selection -and (Field $_.Value 'prefab') -eq $selection.Groups[3].Value});Require ($spawns.Count -eq 1) 'RETAINED_ONE_SELECTED_NATIVE_SPAWN'
 $final=@([regex]::Matches($log,'\[VEHICLE_SCENARIO_RETAINED_FINAL\] operation_id='+[regex]::Escape($operation)+' spawned=1 paid=1 same_asset=1'));Require ($final.Count -eq 1 -and $final[0].Index -gt $lastFoot -and $done.Count -eq 1 -and $done[0].Index -gt $final[0].Index) 'RETAINED_FINAL_SINGLE_PAID_ASSET'
 # VSFinish stops the still-owned retained trip only after the strict FINAL
 # receipt. Native Stop emits one paired FAILED_CLOSED/latch. Bind that pair
 # to the original native admission identity; no reason-only whitelist.
 $failureStates=@([regex]::Matches($log,'\[VEHICLE_STATE_CHANGED\][^\r\n]*state=(?:FALLBACK|FAILED_CLOSED)[^\r\n]*'))
 $failureLatches=@([regex]::Matches($log,'\[ACCEPTANCE_FAILURE_LATCHED\][^\r\n]*'))
 $admissions=@([regex]::Matches($log,'\[VEHICLE_STATE_CHANGED\][^\r\n]*previous_state=BOARDING state=TRANSIT[^\r\n]*')|Where-Object{(Field $_.Value 'operation_id') -eq $operation})
 $teardownAllowed=$false
 if($final.Count -eq 1 -and $identities.Count -eq $legs -and $admissions.Count -eq 1 -and $failureStates.Count -eq 1 -and $failureLatches.Count -eq 1){
  $admission=$admissions[0];$identity=$identities[0];$state=$failureStates[0];$latch=$failureLatches[0]
  $groupGeneration=Field $admission.Value 'group_generation'
  $numericSlot=Field $admission.Value 'numeric_slot'
  $lifecycle=Field $admission.Value 'vehicle_lifecycle_id'
  $entity=$identity.Groups[3].Value.Replace('{}','');$rpl=$identity.Groups[4].Value
  $leaseGeneration=$identity.Groups[6].Value;$vehicleGeneration=$identity.Groups[7].Value;$tripGeneration=$identity.Groups[8].Value
  $cause=Field $state.Value 'causation_id'
  $firstBoard=[regex]::Match($log,'\[VEHICLE_SCENARIO_BOARDING\] scenario='+$Scenario+' leg=1 ')
  $operationParts=[regex]::Match($operation,'^trip-'+[regex]::Escape($Faction)+'-(\d+)-(\d+)-(\d+)$')
  $anchorCurrent=$firstBoard.Success -and $admission.Index -lt $firstBoard.Index -and $groupGeneration -match '^[1-9]\d*$' -and $lifecycle -ne '' -and $lifecycle -ne 'NONE' -and (Field $admission.Value 'faction') -ceq $Faction -and $operationParts.Success
  if($operationParts.Success -and ($operationParts.Groups[1].Value -cne $numericSlot -or $operationParts.Groups[2].Value -cne $groupGeneration -or $operationParts.Groups[3].Value -cne $tripGeneration)){$anchorCurrent=$false}
  foreach($entry in @(@('entity_id',$entity),@('rpl_id',$rpl),@('lease_generation',$leaseGeneration),@('vehicle_generation',$vehicleGeneration),@('trip_generation',$tripGeneration))){if((Field $admission.Value $entry[0]).Replace('{}','') -cne $entry[1]){$anchorCurrent=$false}}
  $teardownAllowed=$anchorCurrent -and $done.Count -eq 1 -and $state.Index -gt $final[0].Index -and $latch.Index -ge $state.Index -and $latch.Index -lt $done[0].Index -and $cause -match ('^'+[regex]::Escape($operation)+'-STOP_ABORT-\d+$')
  foreach($event in @($state,$latch)){
   foreach($entry in @(@('operation_id',$operation),@('faction',$Faction),@('reason','COORDINATOR_STOP'),@('entity_id',$entity),@('rpl_id',$rpl),@('vehicle_lifecycle_id',$lifecycle),@('causation_id',$cause))){if((Field $event.Value $entry[0]).Replace('{}','') -cne $entry[1]){$teardownAllowed=$false}}
  }
  foreach($entry in @(@('previous_state','TACTICAL_DEPLOYMENT'),@('numeric_slot',$numericSlot),@('group_generation',$groupGeneration),@('trip_generation',$tripGeneration),@('lease_generation',$leaseGeneration),@('vehicle_generation',$vehicleGeneration))){if((Field $state.Value $entry[0]) -cne $entry[1]){$teardownAllowed=$false}}
  $states=@([regex]::Matches($state.Value,'(?:^| )state=(\S+)'));if(!$states.Count -or @($states|Where-Object{$_.Groups[1].Value -cne 'FAILED_CLOSED'}).Count){$teardownAllowed=$false}
  foreach($entry in @(@('domain','IDENTITY'),@('slot',$numericSlot),@('group_gen',$groupGeneration),@('trip_gen',$tripGeneration),@('count','1'),@('first_failure','1'))){if((Field $latch.Value $entry[0]) -cne $entry[1]){$teardownAllowed=$false}}
 }
 Require (($failureStates.Count -eq 0 -and $failureLatches.Count -eq 0) -or $teardownAllowed) 'RETAINED_NO_TERMINAL_OR_FAILURE'
 Require ($log -notmatch 'previous_state=HANDOFF state=COMPLETE') 'RETAINED_NO_LEGACY_COMPLETION'
 if($failures.Count){$failures|Sort-Object -Unique;exit 1}
 $resourceErrors=@([regex]::Matches($all,'(?m)^.*\([EF]\).*$')).Count;$strictResources='PASS';if($resourceErrors){$strictResources='FAIL'}
 "VEHICLE_SCENARIO_GATE gate=strict_native_resources verdict=$strictResources error_lines=$resourceErrors independent_blocker=1"
 "PASS Vehicle scenario=$Scenario faction=$Faction receipt_policy=$ReceiptPolicy retained_repeat=$RetainedBehavior legs=$legs original_crew=$crew same_trip_lease_vehicle one_debit real_drives full_foot_arrivals native0/stopped"
 exit 0
}
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
 if($argv -contains '-aicfVehicleScenarioDismountBudgetAdapter'){
  $ai=[array]::IndexOf($argv,'-aicfVehicleScenarioDismountBudgetAdapter');Require ($argv[$ai+1] -eq '1' -and @($argv|Where-Object{$_ -eq '-aicfVehicleScenarioDismountBudgetAdapter'}).Count -eq 1) 'CONTROLLED_BUDGET_ADAPTER_CLI'
  $adapters=@($receipts|Where-Object{(Field $_.Value 'event') -eq 'ARRIVAL_ADAPTER'});Require ($adapters.Count -eq 1) 'CONTROLLED_BUDGET_ADAPTER_UNIQUE'
  foreach($adapter in $adapters){Require ((Field $adapter.Value 'operation_id') -eq $operation -and (Field $adapter.Value 'classification') -eq 'CONTROLLED_DISMOUNT_BRANCH_ONLY' -and (Field $adapter.Value 'source_reason') -eq 'DESTINATION_REACHED' -and (Field $adapter.Value 'native_arrival') -eq '1' -and (Field $adapter.Value 'direct_phase_mutation') -eq '0' -and [int](Field $adapter.Value 'route_generation') -gt 0 -and (Number (Field $adapter.Value 'target_distance_m')) -le (Number (Field $adapter.Value 'dismount_radius_m')) -and $batches.Count -gt 0 -and $adapter.Index -lt $batches[0].Index) 'CONTROLLED_BUDGET_ACTUAL_SOURCE_ARRIVAL'}
 }else{Require (@($receipts|Where-Object{(Field $_.Value 'event') -eq 'ARRIVAL_ADAPTER'}).Count -eq 0) 'HISTORICAL_BUDGET_NO_ADAPTER_MARKER'}
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
