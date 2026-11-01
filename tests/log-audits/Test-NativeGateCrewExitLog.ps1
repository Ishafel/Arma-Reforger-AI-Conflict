param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[Parameter(Mandatory)][string]$EvidenceRoot,[ValidateSet('BEFORE','AFTER')][string]$Mode='AFTER')
$ErrorActionPreference='Stop'
function Test-AICFGateCrewProof([string]$Raw,[string]$ExpectedMode){
 $fail=New-Object 'System.Collections.Generic.List[string]'
 function Need([bool]$Ok,[string]$Reason){if(!$Ok -and !$fail.Contains($Reason)){$fail.Add($Reason)}}
 function Fields([string]$Line){$f=@{};foreach($m in [regex]::Matches($Line,'(?:^|\s)(\w+)=(.*?)(?=\s+\w+=|$)')){$f[$m.Groups[1].Value]=$m.Groups[2].Value.Trim()};return $f}
 $events=@([regex]::Matches($Raw,'\[GATE_CREW_PROBE\]([^\r\n]*)')|ForEach-Object{[pscustomobject]@{index=$_.Index;f=(Fields $_.Groups[1].Value)}})
 Need (@($events|Where-Object{$_.f.mode -ne $ExpectedMode -or $_.f.classification -ne 'CONTROLLED_GATE_SITE_ONLY'}).Count -eq 0) 'EXACT_CONTROLLED_MODE'
 $done=@($events|Where-Object{$_.f.event -eq 'DONE'});$birth=@($events|Where-Object{$_.f.event -eq 'EMPTY_BIRTH'});$ready=@($events|Where-Object{$_.f.event -eq 'ACTUAL_READY_CLOSED'});$chain=@($events|Where-Object{$_.f.event -eq 'NATIVE_PILOT_CHAIN'});$empty=@($events|Where-Object{$_.f.event -eq 'ACTUAL_EMPTY_BEFORE_TICK'});$returned=@($events|Where-Object{$_.f.event -eq 'NATIVE_COMPLETED_RETURN'})
 foreach($pair in @(@($done,'ONE_DONE'),@($birth,'ONE_EMPTY_NATIVE_BIRTH'),@($ready,'ONE_ACTUAL_READY_CLOSED'),@($chain,'ONE_NATIVE_PILOT_CHAIN'),@($empty,'ONE_ACTUAL_EMPTY_BEFORE_TICK'))){Need ($pair[0].Count -eq 1) $pair[1]}
 if($done.Count -ne 1 -or $birth.Count -ne 1 -or $ready.Count -ne 1 -or $chain.Count -ne 1 -or $empty.Count -ne 1){return $fail.ToArray()}
 $d=$done[0].f;$b=$birth[0].f;$r=$ready[0].f;$c=$chain[0].f;$e=$empty[0].f
 Need ($Raw.Contains('[ROSTER_READY]') -and $Raw.Contains('Game destroyed.')) 'FULL_STOPPED_ROSTER'
 Need ($b.agents -eq '0' -and $b.intended -eq '0' -and $b.actor_teleports -eq '0') 'FRESH_EMPTY_GROUP_ONLY'
 Need ($b.group -and $b.group -eq $r.group) 'SAME_NATIVE_BIRTH_READY_GROUP'
 Need ($r.actors -eq '1' -and $r.fake_ready -eq '0' -and $r.state -eq '0' -and $r.control -eq '0' -and $r.actor_rpl -and $r.actor_rpl -ne 'NONE') 'ACTUAL_READY_AND_CLOSED_DOOR'
 Need ($c.actual_original_pilot -eq '1' -and $c.closed_setup -eq '1' -and $c.gate -eq $r.gate -and $c.member -eq $r.actor) 'EXACT_NATIVE_ORIGINAL_PILOT'
 Need ([int]$c.window_start -gt 0 -and [int]$c.window_deadline -gt [int]$c.window_start -and [int]$c.window_deadline -le [int]$c.window_start + 60000 -and [int]$c.window_deadline -le [int]$d.deadline) 'ORIGINAL_BOUNDED_NATIVE_WINDOW'
 Need ($e.operation -eq $c.operation -and $e.member -eq $c.member -and $e.window_start -eq $c.window_start -and $e.window_deadline -eq $c.window_deadline) 'EXACT_EMPTY_NATIVE_GATE_OWNER'
 Need ($e.current_native -match '^SCR_AI(?:GetOutVehicle|PerformActionBehavior|GetInVehicle):' -and $e.actor_health -eq '1' -and $e.vehicle_health -eq '1' -and $e.conscious -eq '1') 'ACTUAL_HEALTHY_NATIVE_STAGE'
 Need ($birth[0].index -lt $ready[0].index -and $ready[0].index -lt $chain[0].index -and $chain[0].index -lt $empty[0].index -and $empty[0].index -lt $done[0].index) 'CAUSAL_EVENT_ORDER'
 $purchases=@([regex]::Matches($Raw,'\[VEHICLE_PRESET_PURCHASE\]([^\r\n]*)')|ForEach-Object{Fields $_.Groups[1].Value})
 Need ($purchases.Count -eq 1 -and $purchases[0].operation -eq $c.operation -and [int]$purchases[0].supplies -gt 0) 'ONE_REAL_NATIVE_PURCHASE'
 $spawns=@($events|Where-Object{$_.f.event -eq 'NATIVE_CAR_SPAWN'})
 Need ($spawns.Count -eq 1 -and $spawns[0].f.native_spawns -eq '1' -and $spawns[0].f.vehicle -eq $c.vehicle -and $spawns[0].f.vehicle_rpl -eq $c.vehicle_rpl -and $d.native_spawns -eq '1') 'ONE_REAL_MAIN_WORLD_CAR'
 Need ($d.passed -eq '1' -and [int]$d.actual_empty -gt 0) 'ACTUAL_CONTROLLED_RESULT'
 Need ([int]$d.actual_empty -ge [int]$c.window_start -and [int]$d.actual_empty -lt [int]$c.window_deadline) 'EMPTY_SEAT_OBSERVED_INSIDE_ORIGINAL_WINDOW'
 Need ($c.vehicle_rpl -and $c.vehicle_rpl -ne 'NONE' -and $d.vehicle_rpl -and $d.vehicle_rpl -ne 'NONE') 'NATIVE_VEHICLE_RPL_PRESENT'
 Need ($d.member -eq $r.actor -and $d.member_rpl -eq $r.actor_rpl -and $d.vehicle -eq $c.vehicle -and $d.vehicle_rpl -eq $c.vehicle_rpl) 'ORIGINAL_ACTOR_AND_VEHICLE_RETAINED'
 Need ($d.actor_health -eq '1' -and $d.captured_actor_health -eq '1' -and $d.vehicle_health -eq '1' -and $d.captured_vehicle_health -eq '1') 'ORIGINAL_HEALTHY_ENTITIES'
 $transitions=@([regex]::Matches($Raw,'\[VEHICLE_STATE_CHANGED\]([^\r\n]*)')|ForEach-Object{Fields $_.Groups[1].Value}|Where-Object{$_.operation_id -eq $c.operation -and $_.group_generation -and $_.trip_generation})
 Need ($transitions.Count -gt 0) 'ACTUAL_TRIP_GENERATION_CONTEXT'
 foreach($latch in [regex]::Matches($Raw,'\[ACCEPTANCE_FAILURE_LATCHED\]([^\r\n]*)')){
  $f=Fields $latch.Groups[1].Value
  $ownedStop=$transitions.Count -gt 0 -and $latch.Index -gt $done[0].index -and $f.reason -eq 'COORDINATOR_STOP' -and
   $f.operation_id -eq $c.operation -and $f.group_gen -eq $transitions[0].group_generation -and $f.trip_gen -eq $transitions[0].trip_generation
  Need $ownedStop 'NO_UNSCOPED_ACCEPTANCE_FAILURE'
 }
 if($ExpectedMode -eq 'BEFORE'){
  Need ($d.reason -in @('FALSE_CREW_UNAVAILABLE_TACTICAL','TERMINAL_TACTICAL_BEGIN_REJECTED')) 'OLD_FALSE_CREW_FAILURE_REPRODUCED'
  Need ($returned.Count -eq 0 -and $d.owned_polls -eq '0' -and $d.completion -eq '0' -and $d.returned -eq '0') 'OLD_FAILURE_PRECEDES_NATIVE_RETURN'
  Need ($Raw -match ('\[VEHICLE_STATE_CHANGED\][^\r\n]*operation_id='+[regex]::Escape($c.operation)+'[^\r\n]*reason=CREW_UNAVAILABLE')) 'ACTUAL_OLD_CREW_UNAVAILABLE'
 }else{
  Need ($Raw -match ('\[TRANSPORT_NATIVE_GATE_CREW_EXIT_OWNED\][^\r\n]*operation_id='+[regex]::Escape($c.operation)+'(?:\s|$)')) 'ACTUAL_PRODUCTION_CREW_EXIT_PROOF_EVENT'
  Need ($d.reason -eq 'ACTUAL_PILOT_NATIVE_RETURN_AND_10M' -and [int]$d.owned_polls -gt 0 -and [int]$d.moving_polls -ge 3) 'OWNED_NATIVE_CHAIN_AND_DISTINCT_MOTION'
  Need ($returned.Count -eq 1) 'ONE_ACTUAL_COMPLETED_RETURN'
  if($returned.Count -eq 1){$t=$returned[0].f;Need ($t.observed_exit -eq '1' -and $t.actual_callback -eq '1' -and [int]$t.completion -gt 0 -and [int]$t.returned -ge [int]$t.completion -and $t.window_start -eq $c.window_start -and $t.window_deadline -eq $c.window_deadline -and $returned[0].index -gt $empty[0].index) 'NATIVE_CALLBACK_AND_ORIGINAL_60S_WINDOW';Need ([int]$t.window_start -le [int]$t.completion -and [int]$t.completion -le [int]$t.returned -and [int]$t.returned -lt [int]$t.window_deadline -and $d.completion -eq $t.completion -and $d.returned -eq $t.returned) 'CALLBACK_WITHIN_WINDOW_AND_SAME_DONE_RECEIPT'}
  Need ($d.vehicle -eq $c.vehicle -and $d.vehicle_rpl -eq $c.vehicle_rpl -and $d.member -eq $r.actor -and $d.member_rpl -eq $r.actor_rpl -and $d.native_compartment_exact -eq '1') 'SAME_ENTITY_RPL_EXACT_PILOT_RETURN'
  Need ([double]::Parse($d.measured_motion_m,[Globalization.CultureInfo]::InvariantCulture) -ge 10) 'MEASURED_NATIVE_10M'
  Need ($d.actor_health -eq $d.captured_actor_health -and $d.vehicle_health -eq $d.captured_vehicle_health -and $d.current_deadline -eq $d.deadline -and $d.attempts -eq $d.captured_attempts) 'UNCHANGED_HEALTH_DEADLINE_AND_OLD_BUDGET'
  Need ($Raw -notmatch 'reason=TACTICAL_BEGIN_REJECTED|reason=CREW_UNAVAILABLE') 'NO_FALSE_CREW_FAILURE'
 }
 return $fail.ToArray()
}
if($MyInvocation.InvocationName -eq '.'){return}
$raw=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath).Path)
$issues=@(Test-AICFGateCrewProof $raw $Mode)
$manifest=[regex]::Match((Get-Content -LiteralPath $LauncherPath -Raw),'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value|ConvertFrom-Json
$cli=[regex]::Match($raw,'(?m)CLI Params:\s*([^\r\n]+)').Groups[1].Value.Trim()
if($cli -cne ($manifest.arguments -join ' ')){$issues+='EXACT_CANONICAL_CLI'}
if([int](Get-Content -LiteralPath $NativeExitPath -Raw) -ne 0){$issues+='NATIVE_EXIT_0'}
$files=@(Get-ChildItem -LiteralPath (Split-Path -Parent (Resolve-Path -LiteralPath $LogPath).Path) -File -Filter '*.log')
$fatal=@();$resources=@()
foreach($file in $files){$line=0;foreach($text in [IO.File]::ReadLines($file.FullName)){$line++;$entry=[pscustomobject]@{file=$file.FullName;line=$line;text=$text};if($text -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Access violation'){$fatal+=$entry}elseif($text -match '\([EF]\)\s*:'){$resources+=$entry}}}
$proof='PASS';if($issues.Count){$proof='FAIL'};$fatalVerdict='PASS';if($fatal.Count){$fatalVerdict='FAIL'};$resourceVerdict='PASS';if($resources.Count){$resourceVerdict='FAIL'}
[ordered]@{mode=$Mode;proof=$proof;scope='GATE_BOUNDARY_ONLY';original_trip_completion_proven=$false;before_product_failure=($Mode -eq 'BEFORE');fatal=$fatalVerdict;resources=$resourceVerdict;issues=$issues;raw_acceptance_latches=@([regex]::Matches($raw,'\[ACCEPTANCE_FAILURE_LATCHED\][^\r\n]*')|ForEach-Object{$_.Value});post_done_own_coordinator_stop='Only exact operation/group/trip generations after DONE are classified controlled teardown; raw retained';full_sibling_logs=@($files|ForEach-Object{@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash}});fatal_lines=$fatal;resource_lines=$resources}|ConvertTo-Json -Depth 7|Set-Content (Join-Path $EvidenceRoot 'gate-crew-audit.json') -Encoding utf8
"GATE_CREW_AUDIT mode=$Mode proof=$proof fatal=$fatalVerdict resources=$resourceVerdict issues=$($issues -join ',')"
if($issues.Count -or $fatal.Count -or $resources.Count){exit 1};exit 0
