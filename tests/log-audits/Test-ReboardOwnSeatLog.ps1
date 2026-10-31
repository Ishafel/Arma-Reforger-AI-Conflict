param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[Parameter(Mandatory)][string]$EvidenceRoot,[ValidateSet('BEFORE','AFTER')][string]$Mode='AFTER',[switch]$Interrupt)
$ErrorActionPreference='Stop'
function Test-AICFOwnSeatProof([string]$Raw,[string]$ExpectedMode,[bool]$RequireInterrupt=$false){
 $fail=New-Object 'System.Collections.Generic.List[string]'
 function Need([bool]$ok,[string]$rule){if(!$ok -and !$fail.Contains($rule)){$fail.Add($rule)}}
 function Fields([string]$line){$f=@{};foreach($m in [regex]::Matches($line,'(?:^|\s)(\w+)=(.*?)(?=\s+\w+=|$)')){$f[$m.Groups[1].Value]=$m.Groups[2].Value.Trim()};return $f}
 $rows=@([regex]::Matches($Raw,'\[OWN_SEAT_PROBE\]([^\r\n]*)')|ForEach-Object{[pscustomobject]@{index=$_.Index;f=(Fields $_.Groups[1].Value)}})
 Need ($rows.Count -gt 0 -and @($rows|Where-Object{$_.f.mode -ne $ExpectedMode -or $_.f.scope -ne 'CONTROLLED_OWN_WRONG_SEAT'}).Count -eq 0) 'EXACT_MODE_SCOPE'
 $events=@{}
 foreach($event in @('READY','INITIAL_EXACT5','NATIVE_FAULT_SUBMITTED','DONE','NATIVE_PURCHASE','NATIVE_SPAWN')){
  $found=@($rows|Where-Object{$_.f.event -eq $event});Need ($found.Count -eq 1) ('ONE_'+$event);if($found.Count -eq 1){$events[$event]=$found[0]}
 }
 if($events.Count -ne 6){return $fail.ToArray()}
 $done=$events.DONE;$d=$done.f;$r=$events.READY;$i=$events.INITIAL_EXACT5;$fault=$events.NATIVE_FAULT_SUBMITTED;$purchase=$events.NATIVE_PURCHASE;$spawn=$events.NATIVE_SPAWN
 $originalDeadline=$fault.f.original_deadline
 $conflicts=@($rows|Where-Object{$_.f.event -eq 'ACTUAL_CONFLICT_BEFORE_SUPER'});Need ($conflicts.Count -gt 0) 'ACTUAL_PREDECISION_CONFLICT'
 $actors=@($rows|Where-Object{$_.f.event -eq 'ACTOR'});$initial=@($actors|Where-Object{$_.f.phase -eq 'INITIAL'});$final=@($actors|Where-Object{$_.f.phase -eq 'FINAL'})
 Need ($initial.Count -eq 5 -and $final.Count -eq 5) 'FULL_FIVE_ACTOR_LEDGERS'
 Need ($Raw.Contains('[ROSTER_READY]') -and $Raw.Contains('Game destroyed.')) 'FULL_STOPPED_NATIVE_READY'
 Need ($r.f.actual -eq '5' -and $r.f.aggregate_ready -eq '1' -and $r.f.fake_ready -eq '0' -and $r.f.group_rpl -and $r.f.group_rpl -ne 'NONE') 'ACTUAL_MANAGED_FIVE_READY'
 Need ($r.index -lt $i.index -and $i.index -lt $fault.index -and $fault.index -lt $done.index) 'CAUSAL_EVENT_ORDER'
 Need ($d.passed -eq '1' -and $d.fault_calls -eq '1' -and $d.conflict -eq '1' -and [int]$d.deadline -gt 0 -and $d.current_deadline -eq $d.deadline) 'CONTROLLED_RESULT_AND_ORIGINAL_DEADLINE'
 Need ($fault.f.accepted -eq '1' -and $fault.f.calls -eq '1' -and $fault.f.actor_displacement_bound -eq '25' -and $fault.f.mapping_write -eq '0' -and $fault.f.health_write -eq '0' -and $fault.f.vehicle_pose_write -eq '0') 'ONE_BOUNDED_NATIVE_FAULT'
 Need ([int]$originalDeadline -gt 0 -and [int]$fault.f.submitted_at -lt [int]$originalDeadline -and [int]$fault.f.submitted_at -ge [int]$originalDeadline-125000) 'FAULT_INSIDE_ORIGINAL_125S'
 Need ($d.interrupt_case -eq [string][int]$RequireInterrupt) 'EXACT_INTERRUPT_MODE'
 if(!$RequireInterrupt){Need ($originalDeadline -eq $d.deadline) 'UNINTERRUPTED_ORIGINAL_WINDOW'}
 Need ($i.f.operation -eq $d.operation -and $fault.f.operation -eq $d.operation -and $purchase.f.operation -eq $d.operation) 'EXACT_OPERATION_JOIN'
 Need ($purchase.f.purchased -eq '1' -and [int]$purchase.f.paid -gt 0 -and $purchase.f.payments -eq '1' -and $d.payments -eq '1' -and $d.paid -eq $purchase.f.paid) 'ONE_REAL_NATIVE_PAYMENT'
 Need ($spawn.f.count -eq '1' -and $d.physical_spawns -eq '1' -and $spawn.f.vehicle -eq $i.f.vehicle -and $spawn.f.rpl -eq $i.f.vehicle_rpl -and $d.car -eq $i.f.vehicle -and $d.car_rpl -eq $i.f.vehicle_rpl -and $d.car_rpl -and $d.car_rpl -ne 'NONE') 'ONE_SAME_MAINWORLD_ASSET'
 Need ([double]$d.car_health -gt 0 -and $d.car_health -eq $d.captured_car_health) 'UNCHANGED_CAR_HEALTH'
 foreach($row in $conflicts){$c=$row.f;Need ($row.index -gt $fault.index -and $row.index -lt $done.index -and $c.operation -eq $d.operation -and $c.expected -and $c.occupant -and $c.expected -ne $c.occupant -and $c.seat -ne $c.occupant_assigned -and $c.actual_wrongseat -eq '1' -and $c.mapping_unchanged -eq '1' -and $c.health_unchanged -eq '1' -and $c.deadline -eq $originalDeadline -and [int]$c.remaining -gt 0) 'REAL_OWN_CONFLICT_BEFORE_SUPER'}
 $distinct=New-Object 'System.Collections.Generic.HashSet[string]'
 for($p=0;$p -lt 5;$p++){
  $a=@($initial|Where-Object{$_.f.position -eq [string]$p});$b=@($final|Where-Object{$_.f.position -eq [string]$p})
  Need ($a.Count -eq 1 -and $b.Count -eq 1) 'EXACT_FIVE_POSITIONS'
  if($a.Count -eq 1 -and $b.Count -eq 1){$x=$a[0].f;$y=$b[0].f;Need ($x.actor -and $x.actor -ne 'NONE' -and $distinct.Add($x.actor) -and $x.actor -eq $y.actor -and $x.rpl -eq $y.rpl -and $x.rpl -and $x.rpl -ne 'NONE' -and [double]$x.health -gt 0 -and $x.health -eq $x.captured_health -and $y.health -eq $x.health -and $y.captured_id -eq $x.actor -and $y.captured_rpl -eq $x.rpl) 'EXACT_ORIGINAL_FIVE_HEALTH_RPL';Need ($x.expected_seat -and $x.expected_seat -ne 'UNKNOWN' -and $x.actual_seat -eq $x.expected_seat -and $y.expected_seat -eq $x.expected_seat -and ($ExpectedMode -eq 'BEFORE' -or $y.actual_seat -eq $y.expected_seat)) 'EXACT_NATIVE_ASSIGNED_SEAT_MAPPING'}
 }
 foreach($row in $conflicts){$pilot=@($initial|Where-Object{$_.f.actor -eq $row.f.expected});$cargo=@($initial|Where-Object{$_.f.actor -eq $row.f.occupant});Need ($pilot.Count -eq 1 -and $cargo.Count -eq 1 -and $pilot[0].f.expected_seat -eq $row.f.seat -and $cargo[0].f.expected_seat -eq $row.f.occupant_assigned) 'CONFLICT_LINKS_EXACT_ORIGINAL_ASSIGNMENTS'}
 if($ExpectedMode -eq 'BEFORE'){
  Need ($d.reason -eq 'TERMINAL_TACTICAL_SEAT_OCCUPIED' -and $Raw -match ('\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id='+[regex]::Escape($d.operation)+'[^\r\n]*reason=TACTICAL_SEAT_OCCUPIED')) 'OLD_FLOW_REAL_OCCUPIED_FAILURE'
 }else{
  $minimumShort=1;if($RequireInterrupt){$minimumShort=0}
  Need ($d.reason -eq 'OWN_ORIGINAL_RECOVERED_EXACT5_NATIVE_10M' -and $d.exit_readback -eq '1' -and $d.current_identity -eq '1' -and $d.exact5 -eq '1' -and [int]$d.shared_short -ge $minimumShort -and [int]$d.shared_short -le 3 -and $d.current_mobility_attempts -eq $d.old_mobility_attempts -and [int]$d.moving_polls -ge 3 -and [double]::Parse($d.displacement_m,[Globalization.CultureInfo]::InvariantCulture) -ge 10 -and [double]::Parse($d.displacement_m,[Globalization.CultureInfo]::InvariantCulture) -le 1000000) 'OWN_OCCUPANT_BUDGET_PHYSICAL_EXIT_EXACT5_NATIVE_MOTION'
  $exit=@([regex]::Matches($Raw,'\[VEHICLE_REBOARD_OWN_SEAT_CLEARANCE\]([^\r\n]*)')|ForEach-Object{$f=Fields $_.Groups[1].Value;$f['_index']=$_.Index;$f})
  Need ($exit.Count -ge 1 -and $exit.Count -le 3 -and @($exit|Where-Object{$_.operation_id -ne $d.operation -or $_.maximum_shared_short_attempts -ne '3' -or $_.reboard_deadline_ms -ne $originalDeadline -or $_.mapping_unchanged -ne '1'}).Count -eq 0 -and @($exit|Where-Object{$_.accepted -eq '1'}).Count -gt 0) 'ACTUAL_PRODUCTION_SHARED_SHORT_EXIT'
  $previous=0
  foreach($x in $exit){Need ($conflicts.Count -gt 0 -and $x.occupant -eq $conflicts[0].f.occupant -and $x.wrong_seat -eq $conflicts[0].f.seat -and $x.assigned_seat -eq $conflicts[0].f.occupant_assigned -and [int]$x.short_attempt -gt $previous -and [int]$x.short_attempt -le 3) 'EXACT_OCCUPANT_SHARED_BUDGET_NO_RESET';$previous=[int]$x.short_attempt}
  if(!$RequireInterrupt){Need ($previous -le [int]$d.shared_short -and [int]$d.shared_short -le 3) 'FINAL_SHARED_BUDGET_RETAINS_EXIT_AND_SUBSEQUENT_ENTRY'}
  $reservation=@([regex]::Matches($Raw,'\[VEHICLE_REBOARD_OWN_RESERVATION\]([^\r\n]*)')|ForEach-Object{$f=Fields $_.Groups[1].Value;$f['_index']=$_.Index;$f})
  $beforeClear=@($reservation|Where-Object{$_.phase -eq 'BEFORE_EXIT_CLEAR'});$afterClear=@($reservation|Where-Object{$_.phase -eq 'AFTER_EXIT_CLEAR'})
  Need ($beforeClear.Count -eq 1 -and $afterClear.Count -eq 1) 'ACTUAL_CAPTURED_RESERVATION_CLEAR_COVERAGE'
  if($beforeClear.Count -eq 1 -and $afterClear.Count -eq 1 -and $conflicts.Count -gt 0){$b=$beforeClear[0];$a=$afterClear[0];Need ($b.operation_id -eq $d.operation -and $a.operation_id -eq $d.operation -and $b.occupant -eq $conflicts[0].f.occupant -and $a.occupant -eq $b.occupant -and $b.seat -eq $conflicts[0].f.seat -and $a.seat -eq $b.seat -and $b.captured_own_reservation -eq '1' -and $a.captured_own_reservation -eq '1' -and $b.before_clear_own -eq '1' -and $b.occupied_same -eq '1' -and $a.occupied_same -eq '1' -and $b.native_wrong_entry -eq '0' -and $a.after_clear_unreserved -eq '1') 'EXACT_CAPTURED_OWN_RESERVATION_AND_PHYSICAL_READBACK'}
  if($beforeClear.Count -eq 1 -and $afterClear.Count -eq 1 -and $conflicts.Count -gt 0 -and $exit.Count -gt 0){Need ($beforeClear[0]._index -gt $conflicts[0].index -and $afterClear[0]._index -gt $beforeClear[0]._index -and $afterClear[0]._index -lt $exit[0]._index -and $exit[0]._index -lt $done.index) 'RESERVATION_CLEAR_CAUSAL_ORDER'}
  if($RequireInterrupt){
   $submitted=@($rows|Where-Object{$_.f.event -eq 'INTERRUPT_NATIVE_THREAT_SUBMITTED'});$cancel=@($rows|Where-Object{$_.f.event -eq 'ACTUAL_LOCAL_THREAT_CANCELLED'});$second=@($rows|Where-Object{$_.f.event -eq 'CORE_SECOND_REBOARD_WINDOW'})
   Need ($submitted.Count -eq 1 -and $cancel.Count -eq 1 -and $second.Count -eq 1) 'ACTUAL_NATIVE_INTERRUPT_AND_CORE_SECOND_WINDOW'
   if($submitted.Count -eq 1 -and $cancel.Count -eq 1 -and $second.Count -eq 1){$s=$submitted[0].f;$c=$cancel[0].f;$w=$second[0].f
    Need ($s.operation -eq $d.operation -and $c.operation -eq $d.operation -and $w.operation -eq $d.operation -and $s.first_deadline -eq $originalDeadline -and $c.first_deadline -eq $originalDeadline -and $w.first_deadline -eq $originalDeadline -and $d.first_deadline -eq $originalDeadline -and $w.second_deadline -eq $d.deadline -and $d.second_deadline -eq $d.deadline) 'SAME_INTENT_TRIP_REAL_TWO_EPISODES'
    Need ($s.pending_receipt -eq '1' -and $s.pilot_unreserved -eq '1' -and $s.actual_native_threat_next_tick -eq '1' -and $c.pending_before -eq '1' -and $c.pending_after -eq '0' -and $c.combat_argument -eq '1' -and $c.distant_before -eq '0' -and $c.pilot_unreserved -eq '1' -and $c.outcome -eq 'COMBAT_REBOARD_CANCELLED') 'REAL_THREAT_CANCEL_BEFORE_OLD_POST_EXIT_OBSERVER'
    Need ($submitted[0].index -gt $exit[0]._index -and $cancel[0].index -gt $submitted[0].index -and $second[0].index -gt $cancel[0].index -and $second[0].index -lt $done.index -and [int]$w.second_deadline -gt [int]$originalDeadline -and [int]$w.second_deadline -gt [int]$w.observed_at -and [int]$w.second_deadline-[int]$w.observed_at -le125000 -and $w.fixture_clock_write -eq '0') 'ONLY_CORE_REBOARD_WINDOW_AFTER_ACTUAL_CANCEL'
    Need ([int]$d.first_shared_short -eq $previous -and [int]$s.shared_short -eq $previous -and [int]$c.first_shared_short -eq $previous -and $d.actual_interrupt -eq '1') 'ORIGINAL_CANCELLED_EPISODE_SHARED_BUDGET_RETAINED'
   }
  }
  Need ($Raw -match ('\[VEHICLE_REBOARD_SEAT_CONFLICT\][^\r\n]*operation_id='+[regex]::Escape($d.operation)+'[^\r\n]*trigger=BEFORE_DECISION')) 'ACTUAL_PRODUCTION_PRECONFLICT_DIAGNOSTIC'
  $clocks=@($rows|Where-Object{$_.f.event -eq 'NATIVE_CLOCK_PAUSE'});Need ($clocks.Count -ge 2 -and @($clocks|Where-Object{$_.f.exact_native_clock -ne '1' -or $_.f.clock_write -ne '0'}).Count -eq 0 -and @($clocks|Where-Object{$_.f.paused -eq '0' -and $_.f.accepted -eq '1'}).Count -eq 1) 'ONLY_NORMAL_NATIVE_BEHAVIOR_CLOCK_PAUSE'
 }
 foreach($latch in [regex]::Matches($Raw,'\[ACCEPTANCE_FAILURE_LATCHED\]([^\r\n]*)')){
  $f=Fields $latch.Groups[1].Value
  $own=$f.operation_id -eq $d.operation -and $f.group_gen -eq $d.group_gen -and $f.trip_gen -eq $d.trip_gen
  $allowed=$own -and (($ExpectedMode -eq 'BEFORE' -and $f.reason -eq 'TACTICAL_SEAT_OCCUPIED' -and $conflicts.Count -gt 0 -and $latch.Index -gt $conflicts[0].index) -or ($latch.Index -gt $done.index -and $f.reason -eq 'COORDINATOR_STOP'))
  Need $allowed 'NO_UNRELATED_OR_PREEXISTING_FAILURE'
 }
 return $fail.ToArray()
}
if($MyInvocation.InvocationName -eq '.'){return}
$raw=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath).Path)
$issues=@(Test-AICFOwnSeatProof $raw $Mode $Interrupt.IsPresent)
$launcher=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LauncherPath).Path)
$manifest=[regex]::Match($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value|ConvertFrom-Json
$cli=[regex]::Match($raw,'(?m)CLI Params:\s*([^\r\n]+)').Groups[1].Value.Trim()
if($cli -cne ($manifest.arguments -join ' ')){$issues+='EXACT_CANONICAL_CLI'}
if([int](Get-Content -LiteralPath $NativeExitPath -Raw)-ne0){$issues+='NATIVE_EXIT'}
$siblings=@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')
$fatal=@();$resources=@()
foreach($file in $siblings){$number=0;foreach($line in [IO.File]::ReadLines($file.FullName)){$number++;$record=[ordered]@{file=$file.FullName;line=$number;text=$line};if($line -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation'){$fatal+=,$record}elseif($line -match '\([EF]\)\s*:'){$resources+=,$record}}}
$functional='PASS';if($issues.Count){$functional='FAIL'};$fatalVerdict='PASS';if($fatal.Count){$fatalVerdict='FAIL'};$resourceVerdict='PASS';if($resources.Count){$resourceVerdict='FAIL'}
[ordered]@{functional=$functional;fatal=$fatalVerdict;resources=$resourceVerdict;failures=$issues;full_stopped_logs=@($siblings.FullName);fatal_lines=$fatal;resource_lines=$resources;scope='OWN_SEAT_BOUNDARY_ONLY';original_route_proven=$false}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'own-seat-audit.json') -Encoding UTF8
"OWN_SEAT_AUDIT functional=$functional fatal=$fatalVerdict resources=$resourceVerdict reasons=$($issues -join ',')"
if($issues.Count -or $fatal.Count -or $resources.Count){exit 1};exit 0
