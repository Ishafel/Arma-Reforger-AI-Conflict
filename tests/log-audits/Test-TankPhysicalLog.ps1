param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$LauncherPath,
 [Parameter(Mandatory)][string]$NativeExitPath,
 [Parameter(Mandatory)][ValidateSet('US','USSR')][string]$Faction,
 [switch]$RequireNativeGate,
 [switch]$RequireExitHandoff,
 [switch]$RequirePositiveHandler
)
$ErrorActionPreference='Stop'
$full=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath).Path)
$launcher=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LauncherPath).Path)
$failures=[Collections.Generic.List[string]]::new()
function Require([bool]$condition,[string]$rule){if(!$condition){$failures.Add($rule)}}
Require (([IO.File]::ReadAllText((Resolve-Path -LiteralPath $NativeExitPath).Path).Trim()) -eq '0') 'NATIVE_EXIT_ZERO'
Require ($full.Contains('Game destroyed.')) 'FULL_STOPPED_CONSOLE'
$manifestMatch=[regex]::Match($launcher,'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)')
Require $manifestMatch.Success 'CANONICAL_MANIFEST'
if($manifestMatch.Success){
 $manifest=$manifestMatch.Groups[1].Value|ConvertFrom-Json
 $arguments=@($manifest.arguments)
 foreach($pair in @(@('-aicfTankPhysicalProbe','1'),@('-aicfVehicleScenarioCanonicalOwner','1'),@('-aicfVehicleScenario','tank'),@('-aicfVehicleScenarioFaction',$Faction))){
  $index=[array]::IndexOf($arguments,$pair[0]);Require ($index -ge 0 -and $arguments[$index+1] -eq $pair[1]) ('CLI_'+$pair[0])
 }
 if($RequirePositiveHandler){$index=[array]::IndexOf($arguments,'-aicfTankPositiveHandlerFault');Require ($index -ge 0 -and $arguments[$index+1] -eq '1') 'CONTROLLED_HANDLER_FAULT_OPT_IN'}
}
$crew=3;if($Faction -eq 'US'){$crew=4}
Require ($full -match ('\[TANK_PHYSICAL_PROBE\] event=CANONICAL_READY faction='+$Faction+' crew='+$crew+' initial_generation=1 before_coordinator_tick=1')) 'CANONICAL_INITIAL_CREW'
Require ($full -notmatch '\[VEHICLE_SCENARIO_CHECK\][^\r\n]*passed=0') 'NO_FAILED_SCAFFOLD_CHECKS'
foreach($rule in @('CANONICAL_OWNER','EXACT_RECIPE','EXACT_BOARDING','WAITING_MOUNTED','REMOTE_ORDER','TANK_EXPECTED_COMBAT_CREW','TANK_EXACT_NATIVE_CREW','TANK_REAL_NATIVE_MOVEMENT','TANK_COMBAT_ARRIVAL_CREW_RETAINED')){
 Require ($full -match ('\[VEHICLE_SCENARIO_CHECK\][^\r\n]*rule='+$rule+' passed=1')) $rule
}
$start=[regex]::Match($full,'\[TANK_PHYSICAL_PROBE\] event=START faction='+$Faction+' operation_id=(\S+) vehicle=(\S+) crew=(\d+) prefab=(\S+) initial_position=(\S+) deadline=(\d+)')
$done=[regex]::Match($full,'\[TANK_PHYSICAL_PROBE\] event=DONE faction='+$Faction+' operation_id=(\S+) vehicle=(\S+) displacement_m=([\d.]+) moving_polls=(\d+) crew=(\d+) loader_retained=(\d+) cargo_deployed=0 controlled_truce=1 actual_combat_threat=0')
Require $start.Success 'START_RECEIPT';Require $done.Success 'DONE_RECEIPT'
if($start.Success -and $done.Success){
 Require ($start.Index -lt $done.Index -and $start.Groups[1].Value -eq $done.Groups[1].Value -and $start.Groups[2].Value -eq $done.Groups[2].Value) 'SAME_OPERATION_AND_VEHICLE'
 Require ([int]$start.Groups[3].Value -eq $crew -and [int]$done.Groups[5].Value -eq $crew) 'EXACT_CREW_COUNT'
 Require ([double]::Parse($done.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture) -ge 50 -and [int]$done.Groups[4].Value -ge 3) 'PHYSICAL_50M_THREE_VELOCITY_POLLS'
 if($Faction -eq 'US'){Require ($done.Groups[6].Value -eq '1') 'US_CARGO_LOADER_RETAINED'}
 $operation=[regex]::Escape($start.Groups[1].Value);$prefab=[regex]::Escape($start.Groups[4].Value)
 $selection=[regex]::Match($full,'\[VEHICLE_SCENARIO_SELECTION\] scenario=tank faction='+$Faction+' actual_faction=\S+ prefab='+$prefab+' native_seats='+$crew+' native_cost=([1-9]\d*)')
 Require $selection.Success 'NATIVE_CATALOG_QUOTE_AND_SEATS'
 if($selection.Success){Require ($full -match ('\[VEHICLE_PRESET_PURCHASE\][^\r\n]*operation='+$operation+' prefab='+$prefab+' supplies='+$selection.Groups[1].Value+' base=\d+')) 'REAL_PRODUCTION_PAID_PURCHASE'}
 Require ($full.Substring($start.Index,$done.Index-$start.Index) -match ('\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id='+$operation+'\s[^\r\n]*reason=DESTINATION_REACHED[^\r\n]*state=TACTICAL_DEPLOYMENT')) 'ACTUAL_PRODUCTION_DESTINATION_TRANSITION'
 Require ($full.Substring($start.Index,$done.Index-$start.Index) -notmatch ('\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id='+$operation+'\s[^\r\n]*reason=CREW_UNAVAILABLE')) 'NO_PREMATURE_CREW_UNAVAILABLE_IN_CONTROLLED_TRUCE'
}
if($RequireNativeGate -or $RequireExitHandoff){
 $gateStart=[regex]::Match($full,'\[TRANSPORT_NATIVE_GATE_STARTED\][^\r\n]*operation_id=(\S+) member=(\S+) seat=(\S+) vehicle=(\S+) elapsed_ms=0 route_generation=(\d+) assignment_revision=(\d+)[^\r\n]*window_start_ms=(\d+) window_deadline_ms=(\d+)')
 $gateReturn=[regex]::Match($full,'\[TRANSPORT_NATIVE_GATE_RETURNED\][^\r\n]*operation_id=(\S+) member=(\S+) seat=(\S+) vehicle=(\S+) elapsed_ms=(\d+) route_generation=(\d+) assignment_revision=(\d+)[^\r\n]*window_start_ms=(\d+) window_deadline_ms=(\d+)[^\r\n]*observed_exit=1[^\r\n]*return_completed_receipt=1[^\r\n]*perform_completed_receipt=1')
 Require $gateStart.Success 'ACTUAL_NATIVE_GATE_STARTED';Require $gateReturn.Success 'ACTUAL_NATIVE_GATE_COMPLETED_RETURN'
 if($gateStart.Success -and $gateReturn.Success){
  Require ($gateStart.Index -lt $gateReturn.Index -and $start.Success -and $start.Index -lt $gateStart.Index) 'GATE_CAUSAL_RETURN'
  for($field=1;$field -le 4;$field++){Require ($gateStart.Groups[$field].Value -eq $gateReturn.Groups[$field].Value) ('GATE_EXACT_IDENTITY_'+$field)}
  Require ($gateStart.Groups[5].Value -eq $gateReturn.Groups[6].Value -and $gateStart.Groups[6].Value -eq $gateReturn.Groups[7].Value) 'GATE_UNCHANGED_ROUTE_AND_ASSIGNMENT'
  Require ($gateStart.Groups[7].Value -eq $gateReturn.Groups[8].Value -and $gateStart.Groups[8].Value -eq $gateReturn.Groups[9].Value) 'GATE_UNCHANGED_ABSOLUTE_WINDOW'
  Require ([long]$gateStart.Groups[8].Value-[long]$gateStart.Groups[7].Value -eq 60000 -and [int]$gateReturn.Groups[5].Value -lt 60000) 'ORIGINAL_NATIVE_60S_WINDOW'
  if($done.Success){Require ($gateReturn.Index -lt $done.Index -and $done.Groups[1].Value -eq $gateReturn.Groups[1].Value -and $done.Groups[2].Value -eq $gateReturn.Groups[4].Value) 'SAME_TANK_ARRIVAL_AFTER_NATIVE_GATE'}
 }
 if($RequireExitHandoff){
  $boundary=[regex]::Match($full,'\[TANK_GATE_EXIT_HANDOFF_PROBE\] event=EXIT_HANDOFF_BOUNDARY operation_id=(\S+) vehicle=(\S+) member=(\S+) accepted=1 current_is_exit=1 current_completed=1 current_removed=1 exit_receipt=1 exit_receipt_ms=(\d+) perform_queued=1 return_queued=1 window_start_ms=(\d+) window_deadline_ms=(\d+) observer_readonly=1 production_outcome_unchanged=1')
  Require $boundary.Success 'ACTUAL_COMPLETED_EXIT_PRODUCTION_PROOF_AT_NATIVE_INVOKER'
  if($boundary.Success -and $gateStart.Success -and $gateReturn.Success){
   Require ($gateStart.Index -lt $boundary.Index -and $boundary.Index -lt $gateReturn.Index) 'EXIT_HANDOFF_CAUSAL_NATIVE_ORDER'
   Require ($boundary.Groups[1].Value -eq $gateStart.Groups[1].Value -and $boundary.Groups[2].Value -eq $gateStart.Groups[4].Value -and $boundary.Groups[3].Value -eq $gateStart.Groups[2].Value) 'EXIT_HANDOFF_EXACT_OPERATION_VEHICLE_ACTOR'
   Require ($boundary.Groups[5].Value -eq $gateStart.Groups[7].Value -and $boundary.Groups[6].Value -eq $gateStart.Groups[8].Value -and [long]$boundary.Groups[4].Value -ge [long]$boundary.Groups[5].Value -and [long]$boundary.Groups[4].Value -lt [long]$boundary.Groups[6].Value) 'EXIT_HANDOFF_ORIGINAL_RECEIPT_WINDOW'
  }
 }
}
if($RequirePositiveHandler){
 $injected=[regex]::Match($full,'\[TANK_POSITIVE_HANDLER_PROBE\] event=INJECTED operation_id=(\S+) vehicle=(\S+) record=(\d+) pilot_handler=(\d+) deadline=(\d+) attempts=(\d+) lease_generation=(\d+) vehicle_generation=(\d+) injected=1 healthy=1 full_settled=1 controlled_fault=1 physical_mutation=0')
 $decision=[regex]::Match($full,'\[TANK_POSITIVE_HANDLER_PROBE\] event=DECISION operation_id=(\S+) vehicle=(\S+) accepted=1 record=(\d+) pilot_handler=(\d+) deadline=(\d+) attempts=(\d+) lease_generation=(\d+) vehicle_generation=(\d+) original_record=(\d+) created_handler=(\d+) same_identity=1 controlled_fault=1 production_outcome_unchanged=1 physical_mutation=0')
 Require $injected.Success 'ACTUAL_OWN_NATIVE_HANDLER_FAULT';Require $decision.Success 'ACTUAL_PRODUCTION_HANDLER_REPAIR'
 if($injected.Success -and $decision.Success){
  Require ($injected.Index -lt $decision.Index -and $start.Success -and $decision.Index -lt $start.Index) 'HANDLER_FAULT_REPAIR_BEFORE_PHYSICAL_TRIP'
  for($field=1;$field -le 2;$field++){Require ($injected.Groups[$field].Value -eq $decision.Groups[$field].Value) ('HANDLER_SAME_IDENTITY_'+$field)}
  for($field=5;$field -le 8;$field++){Require ($injected.Groups[$field].Value -eq $decision.Groups[$field].Value) ('HANDLER_UNCHANGED_DEADLINE_BUDGET_GENERATION_'+$field)}
  Require ([int]$injected.Groups[3].Value -gt 0 -and [int]$injected.Groups[4].Value -gt 0 -and $injected.Groups[3].Value -ne $injected.Groups[4].Value) 'ACTUAL_POSITIVE_HANDLER_MISMATCH'
  Require ($decision.Groups[3].Value -eq $injected.Groups[3].Value -and $decision.Groups[4].Value -eq $injected.Groups[3].Value -and $decision.Groups[9].Value -eq $injected.Groups[3].Value -and $decision.Groups[10].Value -eq $injected.Groups[4].Value) 'NATIVE_PILOT_REJOINED_ORIGINAL_RECORD'
  $op=[regex]::Escape($injected.Groups[1].Value);$asset=[regex]::Escape($injected.Groups[2].Value)
  $scope=$full.Substring($injected.Index,$decision.Index-$injected.Index)
  Require ($scope -match ('\[VEHICLE_NATIVE_SUBGROUP_POSITIVE_RECONCILE\][^\r\n]*operation_id='+$op+'\s[^\r\n]*vehicle='+$asset+'\srecord='+$injected.Groups[3].Value+' pilot_handler='+$injected.Groups[4].Value+'\s[^\r\n]*scope=EXACT_OWN_POSITIVE_HANDLERS[^\r\n]*physical_mutation=0 native_join_calls=1')) 'EXACT_OWN_SCOPE_BEFORE_ONE_NATIVE_JOIN'
  Require ($scope -match ('\[VEHICLE_NATIVE_SUBGROUP_BOUND\][^\r\n]*operation_id='+$op+'\s[^\r\n]*vehicle='+$asset+'\s[^\r\n]*record_before='+$injected.Groups[3].Value+' pilot_before='+$injected.Groups[4].Value+' handler='+$injected.Groups[3].Value+' pilot_handler='+$injected.Groups[3].Value+'\s')) 'NATIVE_HANDLER_READBACK_AFTER_REPAIR'
  if($start.Success){Require ($start.Groups[1].Value -eq $injected.Groups[1].Value -and $start.Groups[2].Value -eq $injected.Groups[2].Value) 'SAME_ASSET_PHYSICAL_MOVEMENT_AFTER_HANDLER_REPAIR'}
 }
}
$fatal=0;$errors=0
foreach($file in @(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -Filter *.log -File)){
 $text=[IO.File]::ReadAllText($file.FullName)
 $fatal+=([regex]::Matches($text,'(?m)^.*(?:SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Out of memory|Unhandled exception|NULL pointer|Virtual Machine Exception).*$')).Count
 $errors+=([regex]::Matches($text,'(?m)^.*\(E\).*$')).Count
}
$functional='PASS';if($failures.Count){$functional='FAIL'}
$fatalVerdict='PASS';if($fatal){$fatalVerdict='FAIL'}
$strict='PASS';if($errors){$strict='FAIL'}
"TANK_PHYSICAL functional=$functional fatal=$fatalVerdict strict_resources=$strict faction=$Faction failures=$($failures.Count) fatal_lines=$fatal raw_sibling_error_lines=$errors"
$failures|ForEach-Object{"FAIL $_"}
if($failures.Count -or $fatal -or $errors){exit 1}
