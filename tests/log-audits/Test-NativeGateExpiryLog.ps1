param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[string]$EvidenceRoot)
$ErrorActionPreference='Stop'
$console=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath))
$logs=@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')
$full=($logs|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
$launcher=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LauncherPath))
$failures=[Collections.Generic.List[string]]::new()
function Require($value,[string]$reason){if(!$value -and !$failures.Contains($reason)){$failures.Add($reason)}}
function Fields([string]$line){
 $row=@{}
 foreach($item in [regex]::Matches($line,'(?<key>\w+)=(?<value><[^>]+>|0x[0-9A-Fa-f]+(?:\s*\{\})?|[^\s]+)')){$row[$item.Groups['key'].Value]=$item.Groups['value'].Value}
 return $row
}
function Id([string]$value){$value -replace '\s|\{\}',''}
function Rows([string]$event){@($console -split '\r?\n'|Where-Object{$_ -match ('\['+[regex]::Escape($event)+'\]')}|ForEach-Object{Fields $_})}
Require ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -eq '0') 'NATIVE_EXIT'
Require ($console.Contains('Game destroyed.') -and $console.Contains('CLI Params:') -and $console.Contains('[ROSTER_READY]')) 'FULL_STOPPED_LOG'
Require ($launcher.Contains('AICF_RUNTIME_MANIFEST_JSON=') -and $launcher.Contains('-aicfNativeGateExpiryProbe') -and $launcher.Contains('-aicfVehicleScenarioRetainedBehavior')) 'CANONICAL_MANIFEST'
Require ($full -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|Unhandled exception|Access violation|Assertion failed|Attempt to modify locked entity') 'FULL_SIBLING_FATAL'
$rows=@(Rows 'NATIVE_GATE_EXPIRY_PROBE')
Require ($rows.Count -eq 4 -and (@($rows|ForEach-Object{$_.event}) -join ',') -eq 'START,EXPIRED,REPAIR,MOVED') 'ORDERED_ONE_EXPIRY_REPAIR'
foreach($row in $rows){Require ($row.classification -eq 'CONTROLLED_GATE_EXPIRY_ONLY' -and $row.natural_expiry_proof -eq '0') 'EXPLICIT_CONTROLLED_SCOPE'}
if($rows.Count -eq 4){
 $start=$rows[0];$expired=$rows[1];$repair=$rows[2];$moved=$rows[3]
 $operation=$start.operation_id;$vehicle=Id $start.vehicle;$rpl=$start.vehicle_rpl;$nodeRpl=$start.gate_node_rpl
 Require ($operation -match '^trip-' -and $vehicle -match '^(0x[\da-fA-F]+|[1-9]\d*)$' -and $rpl -and $rpl -ne '-1') 'REAL_IDENTITIES'
 foreach($row in $rows){Require ($row.operation_id -eq $operation -and (Id $row.vehicle) -eq $vehicle -and $row.vehicle_rpl -eq $rpl -and $row.gate_node_rpl -eq $nodeRpl -and $nodeRpl -and $nodeRpl -ne '-1') 'SAME_TRIP_CAR_RPL'}
 Require ($expired.observed_exit -eq '1' -and $expired.return_completed_receipt -eq '1' -and [long]$expired.return_completed_at_ms -gt 0 -and [long]$expired.returned_at_ms -ge [long]$expired.return_completed_at_ms -and [long]$expired.returned_at_ms -le [long]$expired.t_ms) 'ACTUAL_NATIVE_RETURN_HISTORY'
 Require ($expired.native_history_actual -eq '1' -and $expired.clocks_written -eq '0' -and $expired.actions_issued -eq '0' -and $expired.physical_correction -eq '0' -and [long]$expired.t_ms -lt [long]$expired.window_deadline_ms) 'EXPIRY_ONLY_FAULT'
 $deadline=[long]$expired.trip_deadline_ms;$attempt=[int]$expired.attempts
 Require ([int]$expired.maximum -eq 2 -and $attempt -ge 0 -and $attempt -lt 2) 'EXISTING_BUDGET'
 Require ([int]$repair.attempts -eq $attempt+1 -and [int]$moved.attempts -eq $attempt+1 -and $repair.maximum -eq '2' -and $moved.maximum -eq '2' -and [int]$repair.route_generation -gt [int]$expired.route_generation) 'ONE_BOUNDED_ROUTE_REPAIR'
 Require ([long]$repair.trip_deadline_ms -eq $deadline -and [long]$moved.trip_deadline_ms -eq $deadline) 'UNCHANGED_TRIP_DEADLINE'
 Require ([long]$repair.observed_at_ms -gt [long]$expired.injected_at_ms -and [long]$repair.delay_ms -eq [long]$repair.observed_at_ms-[long]$expired.injected_at_ms -and [long]$moved.observed_at_ms -gt [long]$repair.observed_at_ms -and [long]$expired.t_ms -ge [long]$expired.injected_at_ms -and [long]$repair.t_ms -ge [long]$repair.observed_at_ms) 'CAUSAL_RECOVERY_TIMING'
 Require ([double]$moved.displacement_m -ge 25 -and [int]$moved.moving_polls -ge 3 -and [double]$moved.speed_mps -ge 0.5 -and $moved.exact_crew -eq '1' -and $moved.original_roster -eq '1' -and $moved.one_purchase -eq '1') 'ACTUAL_NATIVE_MOVEMENT'
 $before=@();$after=@()
 if($repair.position -and $moved.position){$before=@($repair.position.Trim('<','>').Split(',')|ForEach-Object{[double]$_});$after=@($moved.position.Trim('<','>').Split(',')|ForEach-Object{[double]$_})}
 if($before.Count -eq 3 -and $after.Count -eq 3){
  $actual=[math]::Sqrt([math]::Pow($after[0]-$before[0],2)+[math]::Pow($after[2]-$before[2],2))
  Require ($actual -ge 24.99 -and [math]::Abs($actual-[double]$moved.displacement_m) -lt 0.2) 'POSITION_READBACK_DISTANCE'
 }else{Require $false 'POSITION_READBACK_DISTANCE'}
 foreach($event in @('TRANSPORT_NATIVE_GATE_CONTROLLED_EXPIRY','TRANSPORT_NATIVE_GATE_RECOVERY_REQUESTED')){
  $receipts=@(Rows $event|Where-Object{$_.operation_id -eq $operation})
  Require ($receipts.Count -eq 1) ('PRODUCTION_RECEIPT_'+$event)
  if($receipts.Count -eq 1){$receipt=$receipts[0]
   Require ((Id $receipt.vehicle) -eq $vehicle -and (Id $receipt.member) -eq (Id $expired.member) -and $receipt.seat -eq $expired.seat -and (Id $receipt.smart_owner) -eq (Id $expired.smart_owner) -and $receipt.smart_tag -eq 'OpenGate') 'EXACT_NATIVE_GATE_OWNER'
   Require ($receipt.observed_exit -eq '1' -and $receipt.return_completed_receipt -eq '1' -and $receipt.return_completed_at_ms -eq $expired.return_completed_at_ms -and $receipt.returned_at_ms -eq $expired.returned_at_ms) 'PRODUCTION_COMPLETION_HISTORY'
   Require ($receipt.window_start_ms -eq $expired.window_start_ms -and $receipt.window_deadline_ms -eq $expired.window_deadline_ms -and $receipt.progress_written -eq '0') 'UNCHANGED_ORIGINAL_GATE_WINDOW'
  }
 }
 $setup=@(Rows 'NATIVE_GATE_EXPIRY_SETUP')
 if($setup.Count){
  $selected=@($setup|Where-Object{$_.event -eq 'SELECTED'});$closed=@($setup|Where-Object{$_.event -eq 'CLOSED_READBACK'});$closeRequest=@($setup|Where-Object{$_.event -eq 'CLOSE_REQUESTED'})
  Require ($selected.Count -eq 1 -and $closed.Count -eq 1 -and $closeRequest.Count -eq 1) 'ACTUAL_GATE_SETUP_RECEIPTS'
  if($selected.Count -eq 1 -and $closed.Count -eq 1 -and $closeRequest.Count -eq 1){
   Require ($selected[0].actual_tag -eq 'OpenGate' -and (Id $selected[0].gate) -eq (Id $expired.smart_owner) -and (Id $closed[0].gate) -eq (Id $expired.smart_owner) -and (Id $closeRequest[0].gate) -eq (Id $expired.smart_owner) -and [math]::Abs([double]$closed[0].state) -le 0.01 -and [math]::Abs([double]$closed[0].control) -le 0.01 -and $closed[0].actual_native_state -eq '1') 'EXACT_CLOSED_GATE_READBACK'
  }
 }
 $started=@(Rows 'TRANSPORT_NATIVE_GATE_STARTED'|Where-Object{$_.operation_id -eq $operation -and $_.window_start_ms -eq $expired.window_start_ms})
 Require ($started.Count -ge 1 -and @($started|Where-Object{$_.return_completed_receipt -eq '0' -and $_.returned_at_ms -eq '0' -and $_.window_deadline_ms -eq $expired.window_deadline_ms}).Count -ge 1) 'ACTUAL_NATIVE_GATE_BEGIN'
 $recovery=@(Rows 'VEHICLE_RECOVERY_STARTED'|Where-Object{$_.operation_id -eq $operation -and $_.reason -eq 'NATIVE_GATE_WINDOW_EXHAUSTED'})
 Require ($recovery.Count -eq 1 -and $recovery[0].unstuck_mode -eq 'ROUTE_REBUILD_ONLY' -and [int]$recovery[0].mobility_attempt -eq $attempt+1) 'PRODUCTION_ROUTE_ONLY_RECOVERY'
 $purchase=@(Rows 'VEHICLE_PRESET_PURCHASE'|Where-Object{$_.operation -eq $operation})
 Require ($purchase.Count -eq 1) 'EXACT_ONE_PURCHASE'
 $seats=@(Rows 'VEHICLE_SCENARIO_RETAINED_SEAT')
 Require ($seats.Count -eq 10) 'ORIGINAL_AND_FINAL_FIVE_SEATS'
 if($seats.Count -eq 10){for($index=0;$index -lt 5;$index++){
  $first=$seats[$index];$last=$seats[$index+5]
  Require ($first.position -eq $last.position -and $first.seat -eq $last.seat -and (Id $first.member) -eq (Id $last.member) -and (Id $last.member) -eq (Id $last.occupant)) 'EXACT_ORIGINAL_ROSTER_READBACK'
 }}
}
foreach($rule in @('GEP_ONE_BOUNDED_ROUTE_REPAIR','GEP_NATIVE_25M_THREE_POLLS_SAME_ASSET')){Require ([regex]::Matches($console,('rule='+$rule+'\s+passed=1')).Count -eq 1) ('NATIVE_CHECK_'+$rule)}
Require ($console -notmatch '\[VEHICLE_SCENARIO_CHECK\][^\r\n]*passed=0') 'ALL_NATIVE_CHECKS'
$errors=@([regex]::Matches($full,'(?m)^.*\([EF]\):.*$')).Count
$targeted='PASS';if($failures.Count){$targeted='FAIL'}
$resources='PASS';if($errors){$resources='FAIL'}
$overall='FAIL';if($targeted -eq 'PASS' -and $resources -eq 'PASS'){$overall='PASS'}
$result=[ordered]@{classification='CONTROLLED_GATE_EXPIRY_ONLY';targeted=$targeted;strict_resources=$resources;overall=$overall;failures=@($failures);events=$rows.Count;native_error_lines_all_logs=$errors;logs=@($logs|ForEach-Object{[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})}
if($EvidenceRoot){$result|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'verified-result.json')}
$result|ConvertTo-Json -Depth 6
if($failures.Count){exit 1}
exit 0
