param([string]$LogPath,[int]$NativeExitCode=[int]::MinValue,[string]$EvidenceRoot,[switch]$LibraryOnly)
$ErrorActionPreference='Stop'
function Get-AICFCompartmentFields([string]$Line){$fields=@{};foreach($m in [regex]::Matches($Line,'([A-Za-z_][A-Za-z0-9_]*)=([^\s\]]+)')){$fields[$m.Groups[1].Value]=$m.Groups[2].Value};return $fields}
function Test-AICFCompartmentProof([string[]]$Lines){
 $bad=@();$rows=@($Lines|Where-Object{$_-match'\[QUOTA_COMPARTMENT_PROBE\]'});$done=@($rows|Where-Object{$_-match'\[DONE\]'});$setup=@($rows|Where-Object{$_-match'\[SETUP\]'})
 if($done.Count-ne1-or$setup.Count-ne1){return @('EXACT_ONE_SETUP_DONE')}
 $d=Get-AICFCompartmentFields $done[0];$s=Get-AICFCompartmentFields $setup[0]
 if($d.passed-ne'1'-or$d.failures-ne'0'-or$d.reason-ne'ACTUAL_NEWBORN_HETEROGENEOUS_BASKET'-or$d.factories-ne'2'-or$d.births-ne'2'-or$d.entries-ne'2'){$bad+='ACTUAL_DONE_COUNTS'}
 if(!$s.car-or$s.car-in@('NONE','0','-1')-or!$s.car_rpl-or$s.car_rpl-in@('NONE','0','-1')-or!$s.character_prefab-or!$s.prefab_faction){$bad+='ACTUAL_SETUP_IDENTITY'}
 $required=@('FRESH_EMPTY_TWO_SEATS','FULL_STOCK_ZERO_NATIVE','FULL_STOCK_PRESERVED','EXACT_PRESSURE_RELEASE','FIRST_ACTUAL_NEWBORN','SECOND_SAME_CAR','ACTUAL_FACTORY_ENTRY_COUNT','EXACT_TWO_STOCK_CHARACTERS','ONE_STOCK_VEHICLE_HOLD','SETTLED_TAIL')
 foreach($rule in $required){$matches=@($rows|Where-Object{$_-match'\[CHECK\]'}|ForEach-Object{Get-AICFCompartmentFields $_}|Where-Object{$_.rule-eq$rule});if($matches.Count-ne1-or$matches[0].passed-ne'1'){$bad+=('CHECK_'+$rule)}}
 $checkRows=@($rows|Where-Object{$_-match'\[CHECK\]'})
 if(@($checkRows|Where-Object{(Get-AICFCompartmentFields $_).passed-ne'1'}).Count){$bad+='FAILED_CHECK'}
 $negative=@($rows|Where-Object{$_-match'rule=FULL_STOCK_ZERO_NATIVE\b'});$released=@($rows|Where-Object{$_-match'rule=EXACT_PRESSURE_RELEASE\b'})
 $bound=@($rows|Where-Object{$_-match'\[BOUND\]'});$entry=@($rows|Where-Object{$_-match'\[GETIN_RESULT\]'});$body=@($rows|Where-Object{$_-match'\[BODY\]'})
 if($bound.Count-ne2-or$entry.Count-ne2-or$body.Count-ne2){return $bad+@('EXACT_TWO_PHYSICAL_RECEIPTS')}
 if($negative.Count-ne1-or$released.Count-ne1-or[array]::IndexOf($rows,$negative[0])-ge[array]::IndexOf($rows,$released[0])-or[array]::IndexOf($rows,$released[0])-ge[array]::IndexOf($rows,$bound[0])){$bad+='PRESSURE_BEFORE_ALL_BIRTHS'}
 $ids=@();$rpls=@()
 for($i=0;$i-lt2;$i++){
  $b=Get-AICFCompartmentFields $bound[$i];$e=Get-AICFCompartmentFields $entry[$i];$p=Get-AICFCompartmentFields $body[$i]
  if($b.index-ne[string]($i+1)-or$e.index-ne[string]($i+1)-or$p.index-ne[string]($i+1)-or$b.exact_current_creation-ne'1'-or$b.character_bound-ne'1'-or$b.car_bound-ne'1'-or$b.before_activate_getin-ne'1'){$bad+='EXACT_BOUND_BEFORE_ACTIVATION'}
  if($e.accepted-ne'1'-or$e.marked_before_native-ne'1'-or$e.native_bool_observed-ne'1'-or$e.car-ne$s.car){$bad+='ACTUAL_NATIVE_ENTRY'}
  if(!$p.entity-or$p.entity-in@('NONE','0','-1')-or!$p.rpl-or$p.rpl-in@('NONE','0','-1')-or$p.entity-ne$b.entity-or$p.rpl-ne$b.rpl){$bad+='REPEATED_PHYSICAL_IDENTITY'}
  $health=0.0;if(![double]::TryParse($p.health,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$health)-or[double]::IsNaN($health)-or[double]::IsInfinity($health)-or$health-le0-or$health-gt1){$bad+='ALIVE_HEALTH'}
  if([array]::IndexOf($rows,$bound[$i])-ge[array]::IndexOf($rows,$entry[$i])-or[array]::IndexOf($rows,$entry[$i])-ge[array]::IndexOf($rows,$body[$i])){$bad+='BIND_ENTRY_BODY_ORDER'}
  $ids+=$p.entity;$rpls+=$p.rpl
 }
 if(@($ids|Sort-Object -Unique).Count-ne2-or@($rpls|Sort-Object -Unique).Count-ne2){$bad+='DISTINCT_ORIGINAL_BODIES'}
 foreach($rule in @('FIRST_ENTRY_SETTLED','BOTH_ENTRIES_SETTLED')){
  $checks=@($checkRows|ForEach-Object{Get-AICFCompartmentFields $_}|Where-Object{$_.rule-eq$rule})
  if($checks.Count-ne1-or$checks[0].passed-ne'1'){$bad+=('CHECK_'+$rule)}
 }
 $readiness=@($rows|Where-Object{$_-match'\[ENTRY_READINESS\]'})
 foreach($readyRow in $readiness){
  $r=Get-AICFCompartmentFields $readyRow;$index=0
  if(![int]::TryParse($r.index,[ref]$index)-or$index-lt1-or$index-gt2-or$r.entity-ne$ids[$index-1]-or$r.captured_rpl-ne$rpls[$index-1]-or$r.actual_rpl-ne$r.captured_rpl){$bad+='READINESS_IDENTITY_CHANGED'}
 }
 foreach($proof in @(@{index='1';stage='2';before=$bound[1];after=$body[0]},@{index='1';stage='3';before=($rows|Where-Object{$_-match'\[TAIL\]'}|Select-Object -First 1);after=$body[1]},@{index='2';stage='3';before=($rows|Where-Object{$_-match'\[TAIL\]'}|Select-Object -First 1);after=$body[1]})){
  $matching=@($readiness|Where-Object{$r=Get-AICFCompartmentFields $_;$r.index-eq$proof.index-and$r.stage-eq$proof.stage-and$r.exact_seat-eq'1'-and$r.getting_in-eq'0'-and$r.getting_out-eq'0'-and[array]::IndexOf($rows,$_)-gt[array]::IndexOf($rows,$proof.after)-and[array]::IndexOf($rows,$_)-lt[array]::IndexOf($rows,$proof.before)})
  if($matching.Count-lt1){$bad+='ASYNC_EXACT_SEAT_BEFORE_NEXT_PHASE'}
 }
 $tail=@($rows|Where-Object{$_-match'\[TAIL\]'})
 if($tail.Count-ne1){$bad+='EXACT_ONE_TAIL'}else{$t=Get-AICFCompartmentFields $tail[0];$ms=0;$polls=0;if(![int]::TryParse($t.actual_tail_ms,[ref]$ms)-or$ms-lt15000-or$ms-ge300000-or![int]::TryParse($t.distinct_polls,[ref]$polls)-or$polls-lt3-or$t.exact_healthy_bodies-ne'2'-or$t.exact_seats-ne'2'-or$t.exact_character_bindings-ne'2'-or$t.exact_car_bodies-ne'1'){$bad+='PHYSICAL_STABLE_TAIL'}}
 $starts=@($rows|Where-Object{$_-match'\[TAIL_WINDOW_STARTED\]'})
 $resets=@($rows|Where-Object{$_-match'\[TAIL_RESET\]'})
 $completed=@($rows|Where-Object{$_-match'\[TAIL_WINDOW_COMPLETED\]'})
 if($starts.Count-ne$resets.Count+1-or$completed.Count-ne1){$bad+='EXACT_KNOWN_WINDOW_LIFECYCLE'}else{
  $start=Get-AICFCompartmentFields $starts[-1];$end=Get-AICFCompartmentFields $completed[0]
  $startMs=0;$endMs=0;$window=0;$resetCount=0
  if(![int]::TryParse($start.started_elapsed_ms,[ref]$startMs)-or![int]::TryParse($end.elapsed_ms,[ref]$endMs)-or$startMs-lt0-or$endMs-ge300000-or$endMs-$startMs-lt15000-or$start.known_census-ne'1'-or$end.known_census-ne'1'-or$start.unknown_generation-ne$end.unknown_generation-or$start.window-ne$end.window-or![int]::TryParse($start.window,[ref]$window)-or$window-ne$starts.Count-or![int]::TryParse($end.reset_count,[ref]$resetCount)-or$resetCount-ne$resets.Count){$bad+='CONTINUOUS_KNOWN_15S_WINDOW'}
  $startIndex=[array]::IndexOf($rows,$starts[-1]);$endIndex=[array]::IndexOf($rows,$completed[0])
  if($startIndex-ge$endIndex-or$tail.Count-ne1-or[array]::IndexOf($rows,$tail[0])-le$startIndex-or[array]::IndexOf($rows,$tail[0])-ge$endIndex){$bad+='KNOWN_WINDOW_ORDER'}
  for($i=$startIndex+1;$i-lt$endIndex;$i++){if($rows[$i]-match'\[(?:CENSUS_REJECTED|BUILD_COUNTS_REJECTED|TAIL_RESET)\]'){$bad+='UNKNOWN_INSIDE_CLAIMED_STABLE_WINDOW'}}
  for($i=0;$i-lt$resets.Count;$i++){
   $r=Get-AICFCompartmentFields $resets[$i];$held=0
   if($r.window-ne[string]($i+1)-or$r.reset_count-ne[string]($i+1)-or$r.exact_bodies_seats_health-ne'1'-or![int]::TryParse($r.held_ms,[ref]$held)-or$held-lt0-or[array]::IndexOf($rows,$resets[$i])-le[array]::IndexOf($rows,$starts[$i])-or[array]::IndexOf($rows,$resets[$i])-ge[array]::IndexOf($rows,$starts[$i+1])){$bad+='EXACT_UNKNOWN_RESET_ORDER'}
  }
 }
 return $bad
}
if($LibraryOnly){return}
if(!$LogPath-or!(Test-Path -LiteralPath $LogPath -PathType Leaf)){throw 'Full stopped native log required'}
$main=@(Get-Content -LiteralPath $LogPath)
$fail=@(Test-AICFCompartmentProof $main);if($NativeExitCode-ne0){$fail+='NATIVE_EXIT'}
$cli=@($main|Where-Object{$_-match'CLI Params:'})
if($cli.Count-ne1-or$cli[0]-notmatch'(?:^|\s)-aicfQuotaCompartmentProbe\s+1(?:\s|$)'-or$cli[0]-notmatch'(?:^|\s)-gproj\s'){$fail+='EXACT_NATIVE_CLI_PROVENANCE'}
if(!@($main|Where-Object{$_-match'\[ROSTER_READY\]'}).Count){$fail+='ACTUAL_ROSTER_READY'}
$destroyed=@($main|Where-Object{$_-match'ENGINE\s*:\s*Game destroyed\.'})
$done=@($main|Where-Object{$_-match'\[QUOTA_COMPARTMENT_PROBE\]\[DONE\]'})
if($destroyed.Count-ne1-or$done.Count-ne1-or[array]::IndexOf($main,$destroyed[0])-le[array]::IndexOf($main,$done[0])){$fail+='FULL_STOPPED_LOG_REQUIRED'}
$siblings=@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -Filter '*.log' -File);$fatal=@();$resources=@()
$fatalPattern='SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|\b(?:Unhandled|Uncaught)\s+exception\b|\b(?:VM|Virtual\s*Machine)\b.*\b(?:exception|error)\b|\bApplication\s+crashed\b|\bAccess violation\b|\bNull (?:pointer|instance)\b|\bNullPointerException\b|Assertion failed|Entity out of world bounds|Fatal error'
foreach($file in $siblings){foreach($line in @(Get-Content -LiteralPath $file.FullName)){if($line-match$fatalPattern){$fatal+=($file.Name+':'+$line)}elseif($line-match'\([EF]\)'){$resources+=($file.Name+':'+$line)}}}
$result=[ordered]@{functional_pass=($fail.Count-eq0);functional_failures=$fail;native_exit=$NativeExitCode;full_sibling_fatal_count=$fatal.Count;resource_error_count=$resources.Count;resource_or_other_error_count=$resources.Count;strict_pass=($fail.Count-eq0-and$fatal.Count-eq0-and$resources.Count-eq0);full_logs=@($siblings.FullName)}
if($EvidenceRoot){[void][IO.Directory]::CreateDirectory($EvidenceRoot);$result|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'compartment-audit.json') -Encoding UTF8}
$result|ConvertTo-Json -Depth 5
if(!$result.strict_pass){exit 1}
