param([Parameter(Mandatory)][string]$EvidenceRoot,[switch]$Affordable,[switch]$ReservedEmpty)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $EvidenceRoot).Path
$console=@(Get-ChildItem (Join-Path $root profile) -Recurse -Filter console.log)
if($console.Count -ne 1){throw 'One full native console required'}
$raw=[IO.File]::ReadAllText($console[0].FullName)
$siblings=@(Get-ChildItem $console[0].DirectoryName -File -Filter '*.log')
$issues=New-Object 'System.Collections.Generic.List[string]'
function Need([bool]$ok,[string]$reason){if(!$ok -and !$issues.Contains($reason)){$issues.Add($reason)}}
function Fields($line){$f=@{};foreach($m in [regex]::Matches($line,'(?:^|\s)(\w+)=(.*?)(?=\s+\w+=|$)')){$f[$m.Groups[1].Value]=$m.Groups[2].Value.Trim()};return $f}
$summary=Get-Content (Join-Path $root summary.json) -Raw|ConvertFrom-Json
$launcher=[regex]::Match((Get-Content (Join-Path $root launcher.txt) -Raw),'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value|ConvertFrom-Json
$cli=[regex]::Match($raw,'(?m)CLI Params:\s*([^\r\n]+)').Groups[1].Value.Trim()
Need ($cli -ceq ($launcher.arguments -join ' ')) 'EXACT_CLI_MANIFEST'
Need ($summary.native_exit -eq 0 -and $raw.Contains('Game destroyed.') -and $raw.Contains('[ROSTER_READY]')) 'FULL_STOPPED_NATIVE_ROSTER'
Need ($summary.source_drift -eq 0) 'FROZEN_SOURCE'
$done=@([regex]::Matches($raw,'\[PRESET_ECONOMY_DONE\]([^\r\n]*)'))
Need ($done.Count -eq 1) 'ONE_DONE'
$doneIndex=$raw.Length;$doneFields=@{}
if($done.Count -eq 1){$doneIndex=$done[0].Index;$doneFields=Fields $done[0].Groups[1].Value}
Need ($doneFields.failures -eq '0' -and $doneFields.original_route_proven -eq '0') 'CONTROLLED_CASE_SUCCESS_NOT_ROUTE'
$checks=@([regex]::Matches($raw,'\[PRESET_ECONOMY_CHECK\]([^\r\n]*)')|ForEach-Object{Fields $_.Groups[1].Value})
Need (@($checks|Where-Object{$_.passed -ne '1'}).Count -eq 0) 'ALL_CHECKS'
$required=@('EXACT_CONTROLLED_STOCK','REAL_FOOT_DESTINATION')
if($Affordable){$required+='AFFORDABLE_REAL_PURCHASE'}else{$required+=@('EXPECTED_ATOMIC_DECLINE','SAME_INTENT_ONE_SHOT_NATIVE_FOOT_WALK')}
if($ReservedEmpty){$required+=@('AFTER_NATIVE_RESERVE_STOCK_DRAIN','EXACT_RESERVED_EMPTY_DRAIN_WINDOW','EXACT_EMPTY_LEASE_RELEASE')}
foreach($name in $required){Need (@($checks|Where-Object{$_.name -eq $name -and $_.passed -eq '1'}).Count -eq 1) ('ACTUAL_'+$name)}
if(!$Affordable){$walk=@($checks|Where-Object name -eq 'SAME_INTENT_ONE_SHOT_NATIVE_FOOT_WALK');Need ($walk.Count -eq 1 -and [double]::Parse($walk[0].moved_m,[Globalization.CultureInfo]::InvariantCulture) -ge 10 -and $walk[0].operations -eq '1' -and [int]$walk[0].barrier_ms -ge 20000 -and $walk[0].trip_creation_boundary -eq 'REGISTRY_CREATE' -and $walk[0].current_trip_exact -eq '1') 'MEASURED_NATIVE_FOOT_WALK'}
$created=@([regex]::Matches($raw.Substring(0,$doneIndex),'\[PRESET_ECONOMY_TRIP_CREATED\]([^\r\n]*)'))
Need ($created.Count -eq 1 -and (Fields $created[0].Groups[1].Value).created -eq '1') 'NO_NEW_TRIP_BEFORE_COMMIT'
$allowedStops=New-Object 'System.Collections.Generic.List[string]'
$functionalPattern='\[(?:ACCEPTANCE_FAILURE_LATCHED|MEANINGFUL_TASK_DEADLINE_MISSED|GROUP_SPAWN_TIMEOUT)\][^\r\n]*'
foreach($errorEvent in [regex]::Matches($raw,$functionalPattern)){
 $e=Fields $errorEvent.Value;$c=@{};if($created.Count -eq 1){$c=Fields $created[0].Groups[1].Value}
 $teardown=$Affordable -and $doneFields.affordable -eq '1' -and $errorEvent.Index -gt $doneIndex -and
  $errorEvent.Value.StartsWith('[ACCEPTANCE_FAILURE_LATCHED]') -and $e.reason -eq 'COORDINATOR_STOP' -and
  $e.operation_id -eq $c.operation_id -and $e.group_gen -eq $c.group_generation -and $e.trip_gen -eq $c.trip_generation
 Need $teardown 'NO_UNSCOPED_LIFECYCLE_FAILURE'
 if($teardown){$allowedStops.Add($errorEvent.Value.Trim())}
}
Need ($raw.Substring(0,$doneIndex) -notmatch '\[ACCEPTANCE_FAILURE_LATCHED\]|\[MEANINGFUL_TASK_DEADLINE_MISSED\]|\[GROUP_SPAWN_TIMEOUT\]') 'NO_LIFECYCLE_FAILURE_BEFORE_DONE'
$declines=@([regex]::Matches($raw,'\[VEHICLE_PRESET_ECONOMY_DECLINED\]([^\r\n]*)'))
if($Affordable){
 Need ($doneFields.affordable -eq '1' -and [int]$doneFields.leases -ge 1 -and $doneFields.main_world_cars -eq '1' -and $doneFields.purchase_calls -eq '1' -and [int]$doneFields.paid -gt 0 -and $declines.Count -eq 0) 'AFFORDABLE_ONE_REAL_PURCHASE'
 $purchase=@($checks|Where-Object name -eq 'AFFORDABLE_REAL_PURCHASE');$accepted=''
 if($purchase.Count -eq 1){$accepted=$purchase[0].accepted_lease_generation}
 Need ($purchase.Count -eq 1 -and $purchase[0].current_trip_exact -eq '1' -and $purchase[0].current_cap -eq '1' -and $purchase[0].current_physical -eq '1' -and $purchase[0].cap_peak -eq '1' -and [int]$purchase[0].empty_releases -eq ([int]$doneFields.leases - 1) -and [int]$accepted -gt 0) 'EXACT_ONE_CURRENT_BOUND_LEASE'
 $reserved=@([regex]::Matches($raw.Substring(0,$doneIndex),'\[PRESET_ECONOMY_LEASE_RESERVED\]([^\r\n]*)')|ForEach-Object{Fields $_.Groups[1].Value})
 $released=@([regex]::Matches($raw.Substring(0,$doneIndex),'\[PRESET_ECONOMY_EMPTY_RELEASE\]([^\r\n]*)')|ForEach-Object{Fields $_.Groups[1].Value})
 if($created.Count -eq 1){$c=Fields $created[0].Groups[1].Value;Need (@($reserved|Where-Object{$_.operation_id -ne $c.operation_id}).Count -eq 0 -and @($released|Where-Object{$_.operation_id -ne $c.operation_id}).Count -eq 0) 'EXACT_LEASE_OPERATION'}
 Need ($reserved.Count -eq [int]$doneFields.leases -and @($reserved.lease_generation|Sort-Object -Unique).Count -eq $reserved.Count -and $released.Count -eq ($reserved.Count - 1) -and @($reserved|Where-Object{$_.current_cap -ne '1' -or $_.has_asset -ne '0'}).Count -eq 0) 'EVERY_RESERVATION_ACCOUNTED_CAP_PEAK_ONE'
 foreach($r in $released){Need ($r.empty -eq '1' -and $r.cap_active -eq '0' -and $r.registered -eq '0' -and $r.lease_generation -ne $accepted -and @($reserved|Where-Object{$_.operation_id -eq $r.operation_id -and $_.lease_generation -eq $r.lease_generation}).Count -eq 1) 'EXACT_EMPTY_PRIOR_LEASE_RELEASED'}
 Need (@($released.lease_generation|Sort-Object -Unique).Count -eq $released.Count -and @($reserved|Where-Object{$_.lease_generation -eq $accepted}).Count -eq 1) 'ONE_DISTINCT_ACCEPTED_LEASE'
 $nativePurchases=@([regex]::Matches($raw.Substring(0,$doneIndex),'\[VEHICLE_PRESET_PURCHASE\]([^\r\n]*)')|ForEach-Object{Fields $_.Groups[1].Value})
 $spawned=@([regex]::Matches($raw.Substring(0,$doneIndex),'\[VEHICLE_SPAWNED\]([^\r\n]*)')|ForEach-Object{Fields $_.Groups[1].Value})
 if($created.Count -eq 1){$c=Fields $created[0].Groups[1].Value;Need (@($nativePurchases|Where-Object{$_.operation -eq $c.operation_id -and $_.supplies -eq $doneFields.paid}).Count -eq 1 -and @($spawned|Where-Object{$_.operation_id -eq $c.operation_id -and $_.lease_generation -eq $accepted -and $_.vehicle_generation -eq '1' -and $_.rpl_id -and $_.rpl_id -ne 'NONE'}).Count -eq 1) 'INDEPENDENT_REAL_PURCHASE_AND_BOUND_ASSET'}
}else{
 $expectedCap='0';if($ReservedEmpty){$expectedCap='1'}
 Need ($doneFields.affordable -eq '0' -and $doneFields.leases -eq $expectedCap -and $doneFields.main_world_cars -eq '0' -and $doneFields.purchase_calls -eq '0' -and $doneFields.paid -eq '0' -and $doneFields.operations -eq '1') 'NO_LEASE_SPAWN_PAYMENT_RETRY'
 Need ($declines.Count -eq 1) 'ONE_TYPED_DECLINE'
 if($declines.Count -eq 1){
  $d=Fields $declines[0].Groups[1].Value
  if($created.Count -eq 1){$c=Fields $created[0].Groups[1].Value;Need ($c.operation_id -eq $d.operation_id -and $c.group_generation -eq $d.group_generation -and $c.trip_generation -eq $d.trip_generation) 'EXACT_CREATED_DECLINED_TRIP'}
  Need ($d.decision -eq '2' -and [int]$d.quote -gt 0 -and [int]$d.available -lt [int]$d.quote -and [int]$d.stock -ge 0 -and [int]$d.obligations -ge 0 -and $d.cap_reserved -eq $expectedCap -and $d.main_world_spawned -eq '0' -and $d.vehicle_debit -eq '0' -and $d.base_rpl -and $d.base_rpl -ne 'NONE') 'TYPED_VALID_INSUFFICIENT_QUOTE'
  if($ReservedEmpty){
   $drain=@([regex]::Matches($raw,'\[PRESET_ECONOMY_STOCK_DRAIN\]([^\r\n]*)'))
   $decisionReturn=@([regex]::Matches($raw,'\[PRESET_ECONOMY_DECLINE_RETURNED\]([^\r\n]*)'))
   $emptyRelease=@([regex]::Matches($raw.Substring(0,$doneIndex),'\[PRESET_ECONOMY_EMPTY_RELEASE\]([^\r\n]*)'))
   Need ($drain.Count -eq 1 -and $decisionReturn.Count -eq 1 -and $emptyRelease.Count -eq 1) 'EXACT_CROSS_FRAME_DRAIN_RECEIPTS'
   if($drain.Count -eq 1 -and $decisionReturn.Count -eq 1 -and $emptyRelease.Count -eq 1){
    $dr=Fields $drain[0].Groups[1].Value;$ret=Fields $decisionReturn[0].Groups[1].Value;$rel=Fields $emptyRelease[0].Groups[1].Value
    Need ($drain[0].Index -lt $declines[0].Index -and $declines[0].Index -lt $decisionReturn[0].Index -and $decisionReturn[0].Index -lt $emptyRelease[0].Index -and
     $dr.operation_id -eq $d.operation_id -and $ret.operation_id -eq $d.operation_id -and $rel.operation_id -eq $d.operation_id -and
     $dr.lease_generation -eq $d.captured_lease_generation -and $rel.lease_generation -eq $d.captured_lease_generation -and [int]$d.captured_lease_generation -gt 0 -and
     $ret.cap_reserved -eq '1' -and $ret.captured_lease_generation -eq $d.captured_lease_generation -and $ret.own_cap_before -eq '1' -and $ret.physical_before -eq '0' -and
     $rel.empty -eq '1' -and $rel.cap_active -eq '0' -and $rel.registered -eq '0') 'SAME_CAPTURED_RESERVED_EPOCH_RELEASE_AFTER_DECLINE'
   }
   $window=@($checks|Where-Object name -eq 'EXACT_RESERVED_EMPTY_DRAIN_WINDOW');Need ($window.Count -eq 1 -and $window[0].own_cap_after -eq '0' -and $window[0].actual_same_drained_lease -eq '1') 'OWN_CAP_RELEASED_AFTER_WINDOW'
  }
  $restores=@([regex]::Matches($raw,'\[ORDER_RESTORE_RESULT\]([^\r\n]*)')|ForEach-Object{Fields $_.Groups[1].Value}|Where-Object{$_.operation_id -eq $d.operation_id -and $_.group_generation -eq $d.group_generation -and $_.trip_generation -eq $d.trip_generation -and $_.state -eq 'FALLBACK' -and $_.success -eq '1' -and $_.is_current -eq '1' -and $_.postcondition_meaningful_task -eq '1'})
  Need ($restores.Count -eq 1) 'INDEPENDENT_CURRENT_FOOT_RESTORE'
 }
}
$fatal=New-Object 'System.Collections.Generic.List[object]';$resources=New-Object 'System.Collections.Generic.List[object]'
foreach($file in $siblings){$line=0;foreach($text in [IO.File]::ReadLines($file.FullName)){$line++;$record=[pscustomobject]@{file=$file.FullName;line=$line;text=$text};if($text -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation'){$fatal.Add($record)}elseif($text -match '\([EF]\)\s*:'){$resources.Add($record)};$functionalEvent=[regex]::Match($text,$functionalPattern);if($functionalEvent.Success -and !$allowedStops.Contains($functionalEvent.Value.Trim())){Need $false 'FULL_SIBLING_LIFECYCLE_FAILURE'}}}
$functional='PASS';if($issues.Count){$functional='FAIL'};$fatalVerdict='PASS';if($fatal.Count){$fatalVerdict='FAIL'};$resourceVerdict='PASS';if($resources.Count){$resourceVerdict='FAIL'}
[pscustomobject]@{functional=$functional;fatal=$fatalVerdict;resources=$resourceVerdict;failures=$issues.ToArray();native_exit=$summary.native_exit;source_drift=$summary.source_drift;full_sibling_logs=@($siblings.FullName);fatal_lines=$fatal.ToArray();resource_lines=$resources.ToArray();post_done_exact_purchase_teardown=$allowedStops.ToArray();original_route_proven=$false}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $root economy-audit.json)
"ECONOMY_AUDIT functional=$functional fatal=$fatalVerdict resources=$resourceVerdict reasons=$($issues -join ',')"
if($issues.Count -or $fatal.Count -or $resources.Count){exit 1};exit 0
