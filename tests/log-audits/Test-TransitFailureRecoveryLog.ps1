param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[string]$EvidenceRoot)
$ErrorActionPreference='Stop'
$full=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath))
$launcher=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LauncherPath))
$failures=[Collections.Generic.List[string]]::new()
function Require($value,$reason){if(!$value){$failures.Add($reason)}}
Require ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -eq '0') 'NATIVE_EXIT'
Require ($full.Contains('Game destroyed.') -and $full.Contains('CLI Params:') -and $full.Contains('[ROSTER_READY]')) 'FULL_STOPPED_LOG'
Require ($launcher.Contains('AICF_RUNTIME_MANIFEST_JSON=') -and $launcher.Contains('-aicfVehicleScenarioRetainedBehavior')) 'CANONICAL_MANIFEST'
Require ($full -notmatch 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|Unhandled exception') 'FULL_FATAL'
$rows=@()
foreach($line in ($full -split '\r?\n')){
 if($line -notmatch '\[AICF\]\[TRANSIT_FAILURE_RECOVERY_PROBE\]'){continue}
 $row=@{}
 foreach($match in [regex]::Matches($line,'(?<key>\w+)=(?<value><[^>]+>|0x[0-9A-Fa-f]+(?:\s*\{\})?|[^\s]+)')){$row[$match.Groups['key'].Value]=$match.Groups['value'].Value}
 $rows+=,$row
}
Require ($rows.Count -eq 8) 'EXACT_EVENT_COUNT'
$events=@($rows|ForEach-Object{$_.event})
Require (($events -join ',') -eq 'START,INJECTED,REPAIR,MOVED,INJECTED,REPAIR,MOVED,DONE') 'ORDERED_TWO_RECOVERIES'
foreach($row in $rows){Require ($row.classification -eq 'CONTROLLED_FAILURE_RECOVERY_ONLY' -and $row.natural_failure_proof -eq '0') 'EXPLICIT_CONTROLLED_CLASSIFICATION'}
if($rows.Count -eq 8){
 $operation=$rows[0].operation_id;$vehicle=($rows[0].vehicle -replace '\s|\{\}','');$deadline=[long]$rows[0].deadline
 Require ($operation -match '^trip-' -and $vehicle -match '^(?:0x[0-9A-Fa-f]+|[1-9][0-9]*)$' -and $deadline -gt 0) 'INITIAL_IDENTITIES'
 foreach($row in $rows){Require ($row.operation_id -eq $operation -and ($row.vehicle -replace '\s|\{\}','') -eq $vehicle) 'SAME_TRIP_AND_VEHICLE'}
 foreach($step in 1,2){
  $offset=1+($step-1)*3;$injected=$rows[$offset];$repair=$rows[$offset+1];$moved=$rows[$offset+2]
  $expected='STUCK';if($step -eq 2){$expected='UNKNOWN'}
  Require ($injected.step -eq [string]$step -and $injected.native_result -eq $expected -and $injected.accepted -eq '1' -and $injected.repeated -eq '1' -and [int]$injected.callbacks -eq $step) 'EXACT_ONCE_NATIVE_CALLBACK'
  Require ([long]$repair.t_ms -gt [long]$injected.t_ms -and [long]$repair.t_ms-[long]$injected.t_ms -lt 10000 -and [long]$repair.delay_ms -eq [long]$repair.t_ms-[long]$injected.t_ms) 'ACTUAL_NEXT_TICK_DELAY'
  Require ([int]$repair.attempts -eq $step -and $repair.maximum -eq '2' -and [int]$repair.route_generation -gt [int]$injected.route_generation) 'COUNTED_BOUNDED_REPAIR'
  Require ([long]$repair.deadline -eq $deadline -and [long]$moved.deadline -eq $deadline) 'UNCHANGED_ABSOLUTE_DEADLINE'
  Require ([long]$moved.t_ms -gt [long]$repair.t_ms -and [double]$moved.displacement_m -ge 25 -and [int]$moved.moving_polls -ge 3 -and [int]$moved.attempts -eq $step) 'PHYSICAL_MOVEMENT_AFTER_REPAIR'
 }
 Require ($rows[7].attempts -eq '2' -and $rows[7].maximum -eq '2' -and $rows[7].callbacks -eq '2' -and $rows[7].original_roster -eq '1' -and [long]$rows[7].deadline -eq $deadline) 'EXHAUSTED_BOUNDARY'
}
$checks=@([regex]::Matches($full,'\[AICF\]\[VEHICLE_SCENARIO_CHECK\][^\r\n]*name=(?<name>[^\s]+)[^\r\n]*pass=(?<pass>[01])'))
if(!$checks.Count){$checks=@([regex]::Matches($full,'\[AICF\]\[VEHICLE_SCENARIO_CHECK\][^\r\n]*check=(?<name>[^\s]+)[^\r\n]*passed=(?<pass>[01])'))}
Require ($full -match 'rule=TFR_EXHAUSTED_PRESERVES_STOCK\s+passed=1') 'EXHAUSTED_NATIVE_NEGATIVE_RECEIPT'
foreach($rule in @('TFR_UNRELATED_REJECTED','TFR_DEFAULT_HANDLER_REJECTED','TFR_MINUS_ONE_HANDLER_REJECTED','TFR_MINUS_TWO_HANDLER_REJECTED','TFR_FOREIGN_HANDLER_REJECTED','TFR_STOPPED_PRESERVES_STOCK','TFR_EXACT_NATIVE_FAILURE_CAPTURED','TFR_NEXT_TICK_BOUNDED_RECOVERY','TFR_REAL_MOVEMENT_AFTER_REPAIR')){
 Require ([regex]::Matches($full,('rule='+$rule+'\s+passed=1')).Count -eq 2) ('NATIVE_CHECK_'+$rule)
}
Require ([regex]::Matches($full,'rule=TFR_INITIAL_BUDGET_TWO\s+passed=1').Count -eq 1) 'INITIAL_BUDGET_NATIVE_CHECK'
Require ([regex]::Matches($full,'rule=TFR_NEW_ACTION_SAME_LOCATION_NOT_DUPLICATE\s+passed=1').Count -eq 1) 'NEW_ACTION_NOT_DEDUPLICATED'
Require ($full -notmatch '\[AICF\]\[VEHICLE_SCENARIO_CHECK\][^\r\n]*(?:pass|passed)=0') 'ALL_NATIVE_CHECKS'
Require ([regex]::Matches($full,'\[TRANSPORT_NATIVE_MOVE_FAILURE_CAPTURED\]').Count -eq 2 -and [regex]::Matches($full,'\[TRANSPORT_NATIVE_MOVE_FAILURE_RECOVERY\]').Count -eq 2) 'PRODUCTION_RECEIPTS'
if($rows.Count -eq 8){
 foreach($eventName in @('CAPTURED','RECOVERY')){
  $receipts=@($full -split '\r?\n'|Where-Object{$_ -match ('\[TRANSPORT_NATIVE_MOVE_FAILURE_'+$eventName+'\]')})
  $index=0
  foreach($line in $receipts){
   $receipt=@{};foreach($match in [regex]::Matches($line,'(?<key>\w+)=(?<value><[^>]+>|0x[0-9A-Fa-f]+(?:\s*\{\})?|[^\s]+)')){$receipt[$match.Groups['key'].Value]=$match.Groups['value'].Value}
   $expectedResult='NATIVE_STUCK';if($index -eq 1){$expectedResult='NATIVE_UNKNOWN'}
   Require ($receipt.operation_id -eq $operation -and ($receipt.vehicle -replace '\s|\{\}','') -eq $vehicle -and $receipt.native_result -eq $expectedResult -and $receipt.native_callback_delivered -eq '1') 'PRODUCTION_IDENTITY_BOUND_RECEIPTS'
   $index++
  }
 }
}
$nativeErrors=@($full -split '\r?\n'|Where-Object{$_ -match '\s\(E\):|\s\(F\):'})
$functional='PASS';if($failures.Count){$functional='FAIL'}
$resources='PASS';if($nativeErrors.Count){$resources='FAIL'}
$overall='PASS';if($functional -ne 'PASS' -or $resources -ne 'PASS'){$overall='FAIL'}
$result=[pscustomobject]@{classification='CONTROLLED_FAILURE_RECOVERY_ONLY';targeted=$functional;failures=@($failures);strict_native_resources=$resources;native_error_lines=$nativeErrors.Count;overall=$overall;events=$rows.Count;full_log=$LogPath}
if($EvidenceRoot){[void][IO.Directory]::CreateDirectory($EvidenceRoot);$result|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'summary.json')}
$result|ConvertTo-Json -Depth 5
if($functional -ne 'PASS'){exit 1};exit 0
