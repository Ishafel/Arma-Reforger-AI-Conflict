[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [Parameter(Mandatory)][ValidateSet('NORMAL','CAPACITY')][string]$Mode,
 [ValidateSet('MOVING','MOUNTED_STAGING')][string]$ConcurrencyMode='MOVING',
 [ValidateSet('ROOT_ONLY','HIERARCHY')][string]$ProtectionScope='ROOT_ONLY',
 [ValidateRange(1,3600)][int]$MinimumSeconds=1800
)
$ErrorActionPreference='Stop'
$mounted=$ConcurrencyMode -eq 'MOUNTED_STAGING'
$hierarchy=$ProtectionScope -eq 'HIERARCHY'
$classification='NORMAL_CAMPAIGN';if($Mode -eq 'CAPACITY'){$classification='CONTROLLED_CAPACITY_ONLY'}
$notMovingFlag='0';if($mounted){$classification='CAPACITY_MOUNTED_ONLY';$notMovingFlag='1'}
$gates=[ordered]@{}
foreach($name in @('duration','liveness','provenance','environment','protection','funding','recruitment','cohorts','functional','native_fatal','native_resources')){$gates[$name]=[Collections.Generic.List[string]]::new()}
function Require([string]$gate,[bool]$ok,[string]$reason){if(!$ok -and !$gates[$gate].Contains($reason)){$gates[$gate].Add($reason)}}
Require environment (!$mounted -or $Mode -eq 'CAPACITY') 'MOUNTED_STAGING_REQUIRES_CONTROLLED_CAPACITY'
Require environment (!$hierarchy -or $Mode -eq 'CAPACITY') 'HIERARCHY_REQUIRES_CONTROLLED_CAPACITY'
function Number($value){$n=0.0;if(![double]::TryParse([string]$value,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$n) -or [double]::IsNaN($n) -or [double]::IsInfinity($n)){return [double]::NaN};return $n}
function Fields([string]$line){$f=@{};foreach($v in [regex]::Matches($line,'(?:^|\s)([A-Za-z_][A-Za-z0-9_]*)=(.*?)(?=\s+[A-Za-z_][A-Za-z0-9_]*=|$)')){$f[$v.Groups[1].Value]=$v.Groups[2].Value.Trim()};return $f}
function Entity($value){$v=([string]$value).Trim();if($v -match '^(0x[0-9a-fA-F]+)\s*(?:\{\s*\})?$'){return $Matches[1].ToUpperInvariant()};return $v}
function NativeId($actual,$original){
 $a=Entity $actual;$o=Entity $original;if($a -eq $o){return $true}
 if($o -notmatch '^0X[0-9A-F]{1,16}$' -or $a -notmatch '^-?\d+$'){return $false}
 try{$n=[long]::Parse($a,[Globalization.CultureInfo]::InvariantCulture);return $n -ge -2147483648 -and $n -le 4294967295 -and ($n -band 4294967295L) -eq ([Convert]::ToUInt64($o.Substring(2),16) -band 4294967295L)}catch{return $false}
}
function One([string]$name,$context=$null,[string]$gate='functional'){
 $items=@();if($context){if($context.events.ContainsKey($name)){$items=@($context.events[$name])}}elseif($byName.ContainsKey($name)){$items=@($byName[$name])}
 $key='GLOBAL';if($context){$key=$context.key}
 Require $gate ($items.Count -eq 1) ($key+'_'+$name+'_ONCE')
 if($items.Count){return $items[0]};return [pscustomobject]@{name=$name;index=-1;fields=@{}}
}
$root=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
[void][IO.Directory]::CreateDirectory($root)
$log=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath).Path)
$siblings=@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')
$nativeErrors=[Collections.Generic.List[object]]::new()
foreach($file in $siblings){
 $lineNumber=0
 foreach($line in [IO.File]::ReadLines($file.FullName)){
  $lineNumber++
  if($line -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|Assertion failed|Access violation|Attempt to modify locked entity|Unknown class .SCR_AIGoalReaction_Get(?:In|Out)Vehicle'){
   Require native_fatal $false 'FULL_SIBLING_FATAL';$nativeErrors.Add([pscustomobject]@{kind='fatal';file=$file.FullName;line=$lineNumber;text=$line})
  }elseif($line -match '\([EF]\)\s*:'){
   Require native_resources $false 'NATIVE_ERRORS_REQUIRE_BASELINE_REVIEW';$nativeErrors.Add([pscustomobject]@{kind='native_error';file=$file.FullName;line=$lineNumber;text=$line})
  }
  if($line -match '\[ACCEPTANCE_FAILURE_LATCHED\]|\[MOB_EGRESS_DEADLINE_MISSED\]|\[MEANINGFUL_TASK_DEADLINE_MISSED\]|\[VEHICLE_MULTI_(?:FAILURE|ABORTED)\]'){Require functional $false 'LATCHED_OR_OBSERVED_FAILURE'}
 }
}
$byName=@{};$contexts=@{};$samples=@{}
foreach($faction in @('US','USSR')){for($slot=0;$slot -lt 6;$slot++){$key="$faction/$slot";$contexts[$key]=@{key=$key;faction=$faction;slot=[string]$slot;events=@{}}}}
foreach($m in [regex]::Matches($log,'\[VEHICLE_MULTI_([A-Z_]+)\]([^\r\n]*)')){
 $e=[pscustomobject]@{name=$m.Groups[1].Value;index=$m.Index;fields=(Fields $m.Groups[2].Value)}
 Require environment ($e.fields.mode -eq $Mode) 'ALL_MARKERS_EXPECTED_MODE'
 Require environment ($e.fields.concurrency_mode -eq $ConcurrencyMode -and $e.fields.classification -eq $classification -and $e.fields.notmovingproof -eq $notMovingFlag) 'NO_MIXED_OR_UNLABELED_CONCURRENCY_ORACLES'
 Require environment ($e.fields.muster_policy -eq 'PER_GROUP_BEFORE_VEHICLE') 'EXPLICIT_PER_GROUP_MUSTER_POLICY'
 if($hierarchy -or $e.fields.ContainsKey('protection_scope')){Require environment ($e.fields.protection_scope -eq $ProtectionScope) 'NO_MIXED_OR_MISSING_PROTECTION_SCOPE'}
 if($hierarchy){Require protection ($e.fields.test_only -eq '1') 'HIERARCHY_EXPLICIT_TEST_ONLY_MARKERS'}
 if(!$byName.ContainsKey($e.name)){$byName[$e.name]=[Collections.Generic.List[object]]::new()};$byName[$e.name].Add($e)
 $key=[string]$e.fields.faction+'/'+[string]$e.fields.numeric_slot
 if($contexts.ContainsKey($key)){$ctx=$contexts[$key];if(!$ctx.events.ContainsKey($e.name)){$ctx.events[$e.name]=[Collections.Generic.List[object]]::new()};$ctx.events[$e.name].Add($e)}
 elseif($e.name -notin @('ENV','GRANT','FUNDING')){Require functional ($e.fields.faction -eq 'BOTH' -and $e.fields.numeric_slot -eq '-1') 'GLOBAL_OR_REAL_MANAGED_SLOT_ONLY'}
 if($e.name -eq 'SLOT'){$id=[string]$e.fields.sample_id;if(!$samples.ContainsKey($id)){$samples[$id]=[Collections.Generic.List[object]]::new()};$samples[$id].Add($e)}
}
$start=One START $null duration;$done=One DONE $null duration;$env=One ENV $null environment
$musters=@($byName['MUSTERED']);$stages=@($byName['COHORT'])
Require recruitment ($musters.Count -eq 3 -and (@($musters|ForEach-Object{$_.fields.cohort}) -join ',') -eq '2,4,6') 'STAGED_PAID_MUSTER_TWO_FOUR_SIX'
foreach($muster in $musters){
 if(!$muster){continue};$n=Number $muster.fields.cohort
 Require recruitment ((Number $muster.fields.groups) -eq $n*2 -and (Number $muster.fields.soldiers) -eq $n*10 -and $muster.fields.paid_muster -eq '1') 'REAL_COHORT_GROUPS_AND_SOLDIERS'
 if($n -gt 2){$prior=@($stages|Where-Object{(Number $_.fields.cohort) -eq $n-2});Require recruitment ($prior.Count -eq 1 -and $prior[0].index -lt $muster.index) 'MUSTER_AFTER_PREVIOUS_PHYSICAL_COHORT'}
}
$minMs=$MinimumSeconds*1000
$lastNativeTick=-1
foreach($event in @($byName.Values|ForEach-Object{$_}|Where-Object{$_.fields.ContainsKey('t_ms')}|Sort-Object index)){
 $nativeTick=Number $event.fields.t_ms;$elapsed=Number $event.fields.elapsed_ms
 Require liveness ($nativeTick -ge $lastNativeTick -and $nativeTick-$elapsed -eq (Number $start.fields.t_ms)-(Number $start.fields.elapsed_ms)) 'MONOTONIC_NATIVE_EVENT_CLOCKS'
 $lastNativeTick=$nativeTick
}
Require duration ($start.index -ge 0 -and $done.index -gt $start.index -and (Number $start.fields.elapsed_ms) -ge 0 -and (Number $done.fields.elapsed_ms)-(Number $start.fields.elapsed_ms) -ge $minMs -and (Number $done.fields.t_ms)-(Number $start.fields.t_ms) -ge $minMs) 'MINIMUM_ENGINE_SPAN'
Require duration ((Number $start.fields.requested_duration_ms) -ge $minMs -and $done.fields.duration_satisfied -eq '1' -and $done.fields.requested_duration_ms -eq $start.fields.requested_duration_ms -and (Number $done.fields.elapsed_ms)-(Number $start.fields.elapsed_ms) -ge (Number $start.fields.requested_duration_ms) -and (Number $start.fields.t_ms)-(Number $start.fields.elapsed_ms) -eq (Number $done.fields.t_ms)-(Number $done.fields.elapsed_ms)) 'REQUESTED_DURATION_AND_CLOCK_ORIGIN'
$wall=[double]::NaN
try{$timing=Get-Content -LiteralPath (Join-Path $root 'timing.json') -Raw|ConvertFrom-Json;$wall=Number $timing.wall_seconds;$clock=([DateTimeOffset]$timing.ended_utc-[DateTimeOffset]$timing.started_utc).TotalSeconds;Require duration ($wall -ge $MinimumSeconds -and $clock -ge $MinimumSeconds -and [Math]::Abs($wall-$clock) -le 1 -and $timing.mode -eq $Mode -and (Number $timing.requested_seconds) -ge $MinimumSeconds) 'MINIMUM_WALL_TIME';Require provenance ($timing.concurrency_mode -eq $ConcurrencyMode) 'TIMING_CONCURRENCY_MATCH';if($hierarchy -or $timing.PSObject.Properties.Name -contains 'protection_scope'){Require provenance ($timing.protection_scope -eq $ProtectionScope) 'TIMING_PROTECTION_SCOPE_MATCH'}}catch{Require duration $false 'TIMING_MISSING_OR_INVALID'}
try{$nativeExit=(Get-Content -LiteralPath (Join-Path $root 'native-exit.txt') -Raw).Trim();Require duration ($nativeExit -eq '0') 'NATIVE_EXIT_ZERO'}catch{Require duration $false 'NATIVE_EXIT_MISSING'}
Require duration ($log.Contains('[ROSTER_READY]') -and $done.index -ge 0 -and $log.IndexOf('Game destroyed.',$done.index,[StringComparison]::Ordinal) -gt $done.index) 'STOPPED_FULL_LOG_AFTER_DONE'
Require duration (!$byName.ContainsKey('ABORTED')) 'NO_EARLY_ABORT'
Require functional ($done.fields.failures -eq '0' -and $done.fields.cohort -eq '8' -and $done.fields.completed_groups -eq '12' -and !$byName.ContainsKey('FAILURE')) 'DONE_ALL_TWELVE_GROUPS_SUCCEEDED'
Require recruitment ($start.fields.slots_per_faction -eq '6' -and $start.fields.crew -eq '5' -and $start.fields.cohorts -eq '2,4,6') 'REAL_TWELVE_GROUP_MUSTER'
$capacity=$Mode -eq 'CAPACITY';$expectedFlag='0';$expectedBudget='0';if($capacity){$expectedFlag='1';$expectedBudget='4000'}
Require environment ($env.fields.truce -eq $expectedFlag -and $env.fields.hold_fire -eq $expectedFlag -and $env.fields.damage_protection -eq $expectedFlag -and $env.fields.grant_budget_per_faction -eq $expectedBudget -and $env.fields.setup_before_roster -eq '1' -and $env.index -lt $start.index -and $env.fields.test_only -eq '1') 'EXPLICIT_MODE_ENVIRONMENT_BEFORE_ROSTER'
try{
 $launcher=Get-Content -LiteralPath (Join-Path $root 'launcher.txt') -Raw
 $matches=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'));Require provenance ($matches.Count -eq 1) 'SINGLE_RUNTIME_MANIFEST'
 if($matches.Count -eq 1){
  $manifest=$matches[0].Groups[1].Value|ConvertFrom-Json;$argv=@($manifest.arguments);$cli=[regex]::Match($log,'CLI Params: ([^\r\n]+)').Groups[1].Value
  $flags=@('-gproj','-profile','-addonsDir','-addons','-aicfAICommanderMode','-aicfVehicleMultiMode','-aicfVehicleMultiConcurrencyMode','-aicfVehicleMultiDurationMs')
  if($hierarchy -or $argv -contains '-aicfVehicleMultiProtectionScope'){$flags+='-aicfVehicleMultiProtectionScope'}
  foreach($flag in $flags){
   $i=[array]::IndexOf($argv,$flag);Require provenance ($i -ge 0 -and $i+1 -lt $argv.Count) ('MANIFEST_'+$flag)
   if($i -ge 0 -and $i+1 -lt $argv.Count){$v=[string]$argv[$i+1];Require provenance ($cli -match ('(?:^|\s)'+[regex]::Escape($flag)+'\s+"?'+[regex]::Escape($v)+'"?(?=\s|$)')) ('ACTUAL_CLI_'+$flag)
    if($flag -eq '-aicfVehicleMultiMode'){Require provenance ($v -eq $Mode) 'MODE_MATCH'}
    if($flag -eq '-aicfVehicleMultiConcurrencyMode'){Require provenance ($v -eq $ConcurrencyMode) 'CONCURRENCY_MODE_MATCH'}
    if($flag -eq '-aicfVehicleMultiProtectionScope'){Require provenance ($v -eq $ProtectionScope) 'PROTECTION_SCOPE_MATCH'}
    if($flag -eq '-aicfAICommanderMode'){Require provenance ($v -eq 'BOTH') 'BOTH_REAL_FACTIONS'}
    if($flag -eq '-aicfVehicleMultiDurationMs'){Require provenance ((Number $v) -eq (Number $start.fields.requested_duration_ms)) 'DURATION_MATCH'}
    if($flag -eq '-profile'){Require provenance ([IO.Path]::GetFullPath($v) -eq [IO.Path]::GetFullPath([string]$manifest.profile)) 'PROFILE_MATCH'}
   }
  }
 }
}catch{Require provenance $false 'LAUNCHER_MANIFEST_MISSING_OR_INVALID'}
try{$sourceManifest=Get-Content -LiteralPath (Join-Path $root 'source-manifest.json') -Raw|ConvertFrom-Json;Require provenance ($sourceManifest.muster_policy -eq 'PER_GROUP_BEFORE_VEHICLE') 'SOURCE_MUSTER_POLICY_MATCH';if($hierarchy -or $sourceManifest.PSObject.Properties.Name -contains 'protection_scope'){Require provenance ($sourceManifest.protection_scope -eq $ProtectionScope) 'SOURCE_PROTECTION_SCOPE_MATCH'}}catch{Require provenance $false 'SOURCE_MUSTER_POLICY_MISSING_OR_INVALID'}
$production=@{}
foreach($m in [regex]::Matches($log,'\[(INFANTRY_RECRUITMENT_STARTED|INFANTRY_RECRUITMENT_PRIORITY_CHANGED|INFANTRY_RECRUITMENT_FINISHED|INFANTRY_RECRUIT_DEBITED|INFANTRY_RECRUIT_JOINED|VEHICLE_PRESET_PURCHASE|VEHICLE_PRESET_SEAT_REQUESTED|VEHICLE_PRESET_OCCUPANT|VEHICLE_SPAWN_ATTEMPT|VEHICLE_STATE_CHANGED|DISEMBARK_COMPLETE)\]([^\r\n]*)')){
 $e=[pscustomobject]@{name=$m.Groups[1].Value;index=$m.Index;fields=(Fields $m.Groups[2].Value)}
 if(!$production.ContainsKey($e.name)){$production[$e.name]=[Collections.Generic.List[object]]::new()};$production[$e.name].Add($e)
}
$allActors=[Collections.Generic.List[string]]::new();$allVehicles=[Collections.Generic.List[string]]::new();$allRpl=[Collections.Generic.List[string]]::new()
foreach($ctx in $contexts.Values){
 $ctx.start=One SLOT_START $ctx recruitment;$ctx.recruited=One RECRUITED $ctx recruitment;$ctx.vehicle=One VEHICLE $ctx functional;$ctx.trip=One TRIP $ctx functional;$ctx.boarded=One BOARDED $ctx functional;$ctx.final=One FINAL_TARGET $ctx functional;$ctx.arrival=One ARRIVAL $ctx functional;$ctx.dismounted=One DISMOUNTED $ctx functional;$ctx.foot=One FOOT_ARRIVAL $ctx functional
 $s=$ctx.start;$r=$ctx.recruited;$v=$ctx.vehicle;$b=$ctx.boarded;$f=$ctx.final;$a=$ctx.arrival;$d=$ctx.dismounted;$foot=$ctx.foot;$key=$ctx.key
 $activation=One ACTIVATED $ctx recruitment
 $requiredCohort=2;if([int]$ctx.slot -ge 2){$requiredCohort=4};if([int]$ctx.slot -ge 4){$requiredCohort=6}
 $musterSet=@($musters|Where-Object{(Number $_.fields.cohort) -eq $requiredCohort});$mustered=[pscustomobject]@{index=-1;fields=@{}};if($musterSet.Count){$mustered=$musterSet[0]}
 Require recruitment ($activation.fields.cohort -eq [string]$requiredCohort -and $activation.fields.prior_desired -eq '1' -and $activation.fields.desired -eq '5' -and $activation.fields.paid_muster -eq '1' -and $activation.index -gt $s.index -and $activation.index -lt $r.index) ($key+'_ACTIVATED_ONLY_IN_CURRENT_COHORT')
 if($requiredCohort -gt 2){$prior=@($stages|Where-Object{(Number $_.fields.cohort) -eq $requiredCohort-2});Require recruitment ($prior.Count -eq 1 -and $activation.index -gt $prior[0].index) ($key+'_NO_FUTURE_COHORT_RECRUITMENT')}
 Require recruitment ($s.fields.pre_muster_infantry -eq '1' -and $s.fields.initial_deployment_size -eq '1' -and (Number $s.fields.initial_alive) -ge 1 -and (Number $s.fields.initial_alive) -lt 5 -and $s.fields.desired -eq '5' -and $s.fields.initial_desired -eq '1' -and $s.fields.configured -eq '1' -and $s.fields.home_order -eq '1') ($key+'_NO_FREE_MOTORIZED_MUSTER')
 Require recruitment ($r.fields.alive -eq '5' -and $r.fields.desired -eq '5' -and $r.fields.initial_alive -eq $s.fields.initial_alive -and (Entity $r.fields.group) -eq (Entity $s.fields.group) -and $r.fields.generation -eq $s.fields.generation -and $s.index -lt $r.index -and $r.index -lt $mustered.index -and $r.index -lt $v.index) ($key+'_OWN_PAID_MUSTER_BEFORE_OWN_VEHICLE')
 $cohortProof=@($stages|Where-Object{(Number $_.fields.cohort) -eq $requiredCohort})
 Require recruitment ($cohortProof.Count -eq 1 -and $v.index -lt $cohortProof[0].index -and $mustered.index -lt $cohortProof[0].index) ($key+'_OWN_VEHICLE_AND_ALL_PAID_BEFORE_COHORT_PROOF')
 $requests=@($ctx.events['RECRUIT_REQUEST']|Where-Object{$_.fields.accepted -eq '1' -and $_.fields.authority_path -eq 'SERVER_FIXTURE_DOWNSTREAM' -and $_.index -gt $activation.index -and $_.index -lt $r.index});Require recruitment ($requests.Count -ge 1) ($key+'_ACCEPTED_REAL_RECRUIT_INTENT')
 $members=@($ctx.events['MEMBER']);$ctx.members=@{}
 foreach($member in $members){$ctx.members[[string]$member.fields.position]=Entity $member.fields.member;$allActors.Add((Entity $member.fields.member))}
 Require recruitment ($members.Count -eq 5 -and (($ctx.members.Keys|Sort-Object) -join ',') -eq '0,1,2,3,4' -and @($ctx.members.Values|Sort-Object -Unique).Count -eq 5 -and @($ctx.members.Values|Where-Object{!$_ -or $_ -eq 'NONE'}).Count -eq 0) ($key+'_FIVE_ORIGINAL_MEMBERS')
 Require recruitment (@($members|Where-Object{$_.index -le $r.index -or $_.index -ge $v.index}).Count -eq 0) ($key+'_OWN_MEMBER_IDS_BEFORE_OWN_VEHICLE')
 $joins=@($production['INFANTRY_RECRUIT_JOINED']|Where-Object{$_.fields.faction -eq $ctx.faction -and $_.fields.slot -eq $ctx.slot -and $_.fields.generation -eq $s.fields.generation -and (Entity $_.fields.group) -eq (Entity $s.fields.group) -and $_.index -lt $r.index})
 Require recruitment ($joins.Count -eq 4 -and @($joins|ForEach-Object{Entity $_.fields.character}|Sort-Object -Unique).Count -eq 4 -and @($joins|Where-Object{(Number $_.fields.cost) -le 0 -or (Entity $_.fields.character) -notin $ctx.members.Values}).Count -eq 0) ($key+'_FOUR_NATIVE_PAID_ADDITIONS_AUTO_OR_PLAYER')
 Require recruitment (@($joins|Where-Object{$_.index -gt $s.index}).Count -eq 5-(Number $s.fields.initial_alive)) ($key+'_INITIAL_ROSTER_ACCOUNTING')
 foreach($join in $joins){$debits=@($production['INFANTRY_RECRUIT_DEBITED']|Where-Object{$_.fields.faction -eq $ctx.faction -and $_.fields.slot -eq $ctx.slot -and $_.fields.token -eq $join.fields.token -and $_.fields.role -eq $join.fields.role -and $_.fields.cost -eq $join.fields.cost -and (Number $_.fields.supplies_before)-(Number $_.fields.supplies_after) -eq (Number $join.fields.cost) -and $_.index -lt $join.index});Require recruitment ($debits.Count -ge 1) ($key+'_NATIVE_DEBIT_FOR_EACH_RECRUIT')}
 $life=@($production['INFANTRY_RECRUITMENT_FINISHED']|Where-Object{$_.fields.faction -eq $ctx.faction -and $_.fields.slot -eq $ctx.slot -and $_.fields.generation -eq $s.fields.generation -and (Entity $_.fields.group) -eq (Entity $s.fields.group) -and $_.fields.reason -eq 'ROSTER_COMPLETE_OR_EMPTY' -and $_.index -lt $r.index});Require recruitment ($life.Count -ge 1) ($key+'_NATIVE_RECRUITMENT_FINISHED')
 $ctx.operation=[string]$ctx.trip.fields.operation_id
 Require functional ($ctx.operation -match ('^trip-'+$ctx.faction+'-'+$ctx.slot+'-\d+-\d+$')) ($key+'_REAL_SLOT_TRIP_IDENTITY')
 foreach($e in @($b,$f,$a,$d,$foot)){Require functional ($e.fields.operation_id -eq $ctx.operation -and $ctx.operation) ($key+'_OPERATION_CONTINUITY')}
 $prefab=[string]$v.fields.prefab;$model=$prefab.EndsWith('/UAZ469.et',[StringComparison]::Ordinal);if($ctx.faction -eq 'US'){$model=$prefab.Contains('/M1025_') -and $prefab.Contains('M2HB')}
 Require functional ($model -and $v.fields.seats -eq '5' -and $v.fields.assigned -eq '5' -and (Number $v.fields.cost) -gt 0 -and $v.fields.paid_muster_before_motorized -eq '1' -and $b.fields.seats -eq '5' -and $b.fields.occupants -eq '5') ($key+'_FACTION_MODEL_EXACT_BOARDING')
 $allVehicles.Add((Entity $b.fields.vehicle));$allRpl.Add([string]$b.fields.rpl)
 Require functional ((Entity $b.fields.vehicle) -ne '' -and (Entity $b.fields.vehicle) -ne 'NONE' -and $b.fields.rpl -and (Number $b.fields.lease_generation) -gt 0 -and (Number $b.fields.vehicle_generation) -gt 0 -and (NativeId $a.fields.vehicle $b.fields.vehicle) -and (NativeId $f.fields.vehicle $b.fields.vehicle)) ($key+'_CURRENT_PHYSICAL_LEASE_IDENTITY')
 $purchase=@($production['VEHICLE_PRESET_PURCHASE']|Where-Object{$_.fields.operation -eq $ctx.operation});Require funding ($purchase.Count -eq 1 -and $purchase[0].fields.prefab -eq $prefab -and (Number $purchase[0].fields.supplies) -eq (Number $v.fields.cost) -and $purchase[0].index -gt $v.index -and $purchase[0].index -lt $b.index) ($key+'_ONE_PAID_VEHICLE_PURCHASE')
 $quotes=@();if($ctx.events.ContainsKey('JIT_QUOTE')){$quotes=@($ctx.events['JIT_QUOTE'])}
 if($capacity){foreach($stage in @('PRESPAWN','PURCHASE')){Require funding (@($quotes|Where-Object{$_.fields.stage -eq $stage}).Count -ge 1) ($key+'_JIT_'+$stage+'_ACTUAL_PRICE_SCOPE')}}
 else{Require funding ($quotes.Count -eq 0) ($key+'_NORMAL_HAS_NO_JIT_FUNDING')}
 foreach($quote in $quotes){
  $q=$quote.fields;$before=Number $q.stock_before;$after=Number $q.stock_after;$grant=Number $q.grant;$obligations=Number $q.obligations
  Require funding ($capacity -and $q.stage -in @('PRESPAWN','PURCHASE') -and $q.operation_id -eq $ctx.operation -and $q.prefab -eq $prefab -and (Number $q.quoted_cost) -eq (Number $v.fields.cost) -and (Entity $q.group) -eq (Entity $s.fields.group) -and $q.generation -eq $s.fields.generation -and (Number $q.lease_generation) -ge -1 -and (Number $q.request_generation) -ge 1 -and (Number $q.attempt) -ge 1 -and $q.budget -eq '4000' -and $q.native_debit -eq 'PENDING' -and $quote.index -gt $v.index -and $purchase.Count -eq 1 -and $quote.index -lt $purchase[0].index -and $q.base -eq $purchase[0].fields.base -and $q.site_owner -eq ('site-'+$ctx.operation.Substring(5)+'-'+$q.request_generation)) ($key+'_JIT_EXACT_SELECTED_NATIVE_PURCHASE')
  $attempts=@($production['VEHICLE_SPAWN_ATTEMPT']|Where-Object{$_ -and $_.fields.operation_id -eq $ctx.operation -and $_.fields.lease_generation -eq $q.lease_generation -and $_.fields.request_generation -eq $q.request_generation -and $_.fields.attempt -eq $q.attempt -and $_.index -lt $quote.index});Require funding ($attempts.Count -ge 1) ($key+'_JIT_CURRENT_NATIVE_SPAWN_ATTEMPT')
  Require funding ($obligations -ge 0 -and (Number $q.required_stock) -eq (Number $q.quoted_cost)+[Math]::Ceiling($obligations) -and $grant -ge 0 -and $grant -eq [Math]::Max(0,(Number $q.required_stock)-$before) -and $after-$before -eq $grant -and $after -ge (Number $q.required_stock)) ($key+'_JIT_ONLY_DEFICIT_INCLUDING_OBLIGATIONS')
  $priorGrants=@($byName['GRANT']|Where-Object{$_ -and $_.fields.faction -eq $ctx.faction -and $_.index -lt $quote.index});$priorTotal=0.0;foreach($priorGrant in $priorGrants){$priorTotal+=Number $priorGrant.fields.applied}
  Require funding ((Number $q.total_granted) -eq $priorTotal -and $priorTotal -le 4000) ($key+'_JIT_USES_SHARED_FINITE_LEDGER')
  if($grant -gt 0){$receipt=@($priorGrants|Where-Object{$_.fields.token -eq $q.token -and (Number $_.fields.before) -eq $before -and (Number $_.fields.after) -eq $after -and (Number $_.fields.applied) -eq $grant});Require funding ($receipt.Count -eq 1) ($key+'_JIT_GRANT_HAS_ACTUAL_OWNER_RECEIPT')}
 }
 $occupants=@($production['VEHICLE_PRESET_OCCUPANT']|Where-Object{$_.fields.operation -eq $ctx.operation})
 Require functional ($occupants.Count -eq 5 -and (@($occupants|ForEach-Object{$_.fields.position}|Sort-Object -Unique) -join ',') -eq '0,1,2,3,4' -and @($occupants|ForEach-Object{$_.fields.seat}|Sort-Object -Unique).Count -eq 5) ($key+'_FIVE_NATIVE_SEAT_RECEIPTS')
 foreach($occupant in $occupants){$original=$ctx.members[[string]$occupant.fields.position];$seatRequests=@($production['VEHICLE_PRESET_SEAT_REQUESTED']|Where-Object{$_.fields.operation -eq $ctx.operation -and $_.fields.position -eq $occupant.fields.position -and $_.fields.seat -eq $occupant.fields.seat -and (Entity $_.fields.member) -eq $original -and $_.index -lt $occupant.index});Require functional ($original -and $seatRequests.Count -ge 1 -and (NativeId $occupant.fields.member $original) -and (NativeId $occupant.fields.occupant $original)) ($key+'_NATIVE_ORIGINAL_MEMBER_SEAT_LINK')}
 Require functional ($v.index -lt $ctx.trip.index -and $ctx.trip.index -lt $b.index -and $b.index -lt $f.index -and $f.index -lt $a.index -and $a.index -lt $d.index -and $d.index -lt $foot.index -and $foot.index -lt $done.index) ($key+'_NATIVE_LIFECYCLE_ORDER')
 Require functional ($a.fields.passed -eq '1' -and (Number $a.fields.distance_m) -ge 0 -and (Number $a.fields.distance_m) -le (Number $f.fields.dismount_radius_m)+10 -and (Number $a.fields.displacement_m) -ge 100 -and (Number $a.fields.moving_polls) -ge 5) ($key+'_REAL_FINAL_TRAVEL')
 Require functional ($f.fields.final -eq '1' -and (Number $f.fields.intent_revision) -gt 0 -and $a.fields.intent_revision -eq $f.fields.intent_revision -and $d.fields.intent_revision -eq $f.fields.intent_revision -and $foot.fields.intent_revision -eq $f.fields.intent_revision -and $foot.fields.position -eq $f.fields.position -and $d.fields.position -eq $f.fields.position -and $foot.fields.leg -eq $f.fields.leg -and $a.fields.leg -eq $f.fields.leg) ($key+'_LIVE_UNCHANGED_FINAL_POINT_INTENT')
 Require functional ($d.fields.alive -eq '5' -and $d.fields.in_vehicle -eq '0' -and $d.fields.normal_complete -eq '1' -and $foot.fields.alive -eq '5' -and (Number $foot.fields.farthest_m) -ge 0 -and (Number $foot.fields.farthest_m) -le 35) ($key+'_NORMAL_DISMOUNT_AND_FIVE_FOOT_ARRIVALS')
 $complete=@($production['VEHICLE_STATE_CHANGED']|Where-Object{$_.fields.operation_id -eq $ctx.operation -and $_.fields.previous_state -eq 'HANDOFF' -and $_.fields.state -eq 'COMPLETE' -and $_.index -gt $a.index -and $_.index -lt $d.index});Require functional ($complete.Count -eq 1) ($key+'_NATIVE_NORMAL_COMPLETE')
 $clear=@($production['DISEMBARK_COMPLETE']|Where-Object{$_.fields.operation_id -eq $ctx.operation -and $_.fields.logical_occupants -eq '0' -and $_.fields.transitions -eq '0' -and $_.fields.inside_bounds -eq '0' -and $_.index -gt $a.index -and $_.index -lt $d.index});Require functional ($clear.Count -eq 1) ($key+'_NATIVE_PHYSICAL_CLEARANCE')
 Require functional (@($production['VEHICLE_STATE_CHANGED']|Where-Object{$_.fields.operation_id -eq $ctx.operation -and $_.fields.state -in @('FALLBACK','FAILED_CLOSED')}).Count -eq 0) ($key+'_NO_SELECTED_TRIP_FALLBACK')
 $ctx.targets=@();if($ctx.events.ContainsKey('TARGET')){$ctx.targets=@($ctx.events['TARGET'])}
 if($mounted){Require cohorts ($ctx.targets.Count -eq 0 -and $f.fields.leg -eq '1') ($key+'_MOUNTED_NO_EARLY_OUTBOUND_FIRST_FINAL_LEG')}
 else{Require cohorts ($ctx.targets.Count -ge 1 -and $ctx.targets[0].index -gt $b.index -and $ctx.targets[0].index -lt $f.index) ($key+'_REAL_OUTBOUND_TARGET_BEFORE_FINAL')}
 $previousLeg=0;$previousRevision=0
 foreach($target in @($ctx.targets)+@($f)){Require functional ($target.fields.operation_id -eq $ctx.operation -and (NativeId $target.fields.vehicle $b.fields.vehicle) -and (Number $target.fields.leg) -eq $previousLeg+1 -and (Number $target.fields.intent_revision) -gt $previousRevision) ($key+'_EXPLICIT_ORDERED_ROLLING_RETARGETS');$previousLeg=Number $target.fields.leg;$previousRevision=Number $target.fields.intent_revision}
}
Require recruitment ($allActors.Count -eq 60 -and @($allActors|Sort-Object -Unique).Count -eq 60) 'SIXTY_DISTINCT_REAL_ACTORS'
Require cohorts ($allVehicles.Count -eq 12 -and @($allVehicles|Sort-Object -Unique).Count -eq 12 -and @($allRpl|Sort-Object -Unique).Count -eq 12 -and @($allRpl|Where-Object{!$_ -or $_ -eq 'NONE'}).Count -eq 0) 'TWELVE_DISTINCT_PHYSICAL_VEHICLES_AND_RPL'
if($hierarchy){
 foreach($ctx in $contexts.Values){
  $ctx.protectionByEntity=@{}
  $receipts=@();if($ctx.events.ContainsKey('PROTECTION')){$receipts=@($ctx.events['PROTECTION'])}
  Require protection ($receipts.Count -gt 0) ($ctx.key+'_HIERARCHY_RECEIPTS_PRESENT')
  foreach($receipt in $receipts){
   $p=$receipt.fields;$id=Entity $p.entity;$covered=Number $p.protection_managers_covered;$children=Number $p.protection_child_managers;$zones=Number $p.protection_zones
   Require protection ($p.protection_status -eq 'COVERED' -and $p.protection_reason -eq 'NONE' -and $p.protection_errors -eq '0' -and $p.protection_enabled_after -eq '0') ($ctx.key+'_NO_FAILED_OR_ENABLED_HIERARCHY_READBACK')
   Require protection ($covered -ge 1 -and [Math]::Floor($covered) -eq $covered -and $children -eq $covered-1 -and $zones -ge $covered -and $zones -le 512 -and [Math]::Floor($zones) -eq $zones -and (Number $p.protection_zones_repeated) -eq $zones -and (Number $p.protection_enabled_before) -ge 0 -and (Number $p.protection_enabled_before) -le $covered) ($ctx.key+'_FULL_REPEAT_COUNTS_AND_COVERED_MANAGERS')
   $identity=$false
   if($p.kind -eq 'MEMBER'){$identity=$id -in $ctx.members.Values}
   if($p.kind -eq 'VEHICLE'){$identity=$id -eq (Entity $ctx.boarded.fields.vehicle);Require protection ($children -gt 0) ($ctx.key+'_ACTUAL_VEHICLE_CHILD_MANAGERS_COVERED')}
   Require protection ($identity -and $id -match '^0X[0-9A-F]+$') ($ctx.key+'_EXACT_ORIGINAL_PROTECTED_ENTITY')
   if(!$ctx.protectionByEntity.ContainsKey($id)){$ctx.protectionByEntity[$id]=[Collections.Generic.List[object]]::new()}
   $ctx.protectionByEntity[$id].Add($receipt)
  }
  foreach($actorId in $ctx.members.Values){Require protection ($ctx.protectionByEntity.ContainsKey([string]$actorId)) ($ctx.key+'_ALL_FIVE_ORIGINAL_ACTORS_PROTECTED')}
  Require protection ($ctx.protectionByEntity.ContainsKey((Entity $ctx.boarded.fields.vehicle))) ($ctx.key+'_ORIGINAL_VEHICLE_PROTECTED')
 }
}
function Has-RecentProtection($context,[string]$identity,$sample){
 if(!$context.protectionByEntity.ContainsKey($identity)){return $false}
 $items=$context.protectionByEntity[$identity];$low=0;$high=$items.Count-1;$best=-1
 while($low -le $high){$middle=[int][Math]::Floor(($low+$high)/2);if($items[$middle].index -lt $sample.index){$best=$middle;$low=$middle+1}else{$high=$middle-1}}
 if($best -lt 0){return $false}
 $age=(Number $sample.fields.t_ms)-(Number $items[$best].fields.t_ms)
 return $age -ge 0 -and $age -le 10000
}
$granted=@{US=0.0;USSR=0.0};$grantTokens=@{}
foreach($grant in @($byName['GRANT'])){
 if(!$grant){continue};$g=$grant.fields;$faction=[string]$g.faction
 Require funding ($capacity -and $faction -in @('US','USSR') -and $g.mode -eq 'CAPACITY' -and $g.owner -eq 'ECONOMY_SYSTEM' -and $g.test_only -eq '1' -and $g.budget -eq '4000' -and $g.token -and !$grantTokens.ContainsKey([string]$g.token)) 'CONTROLLED_GRANT_OWNER_TOKEN_BUDGET'
 $grantTokens[[string]$g.token]=$true
 if($faction -in @('US','USSR')){
  $applied=Number $g.applied;$granted[$faction]+=$applied
  Require funding ($applied -gt 0 -and $applied -le (Number $g.requested) -and (Number $g.after)-(Number $g.before) -eq $applied -and (Number $g.after) -le (Number $g.quoted_demand) -and $granted[$faction] -le 4000 -and (Number $g.total_granted) -eq $granted[$faction]) 'FINITE_ACTUAL_GRANT_LEDGER'
 }
}
foreach($faction in @('US','USSR')){
 $field=$faction.ToLowerInvariant()+'_granted';Require funding ((Number $done.fields[$field]) -eq $granted[$faction] -and ($capacity -or $granted[$faction] -eq 0)) ($faction+'_DONE_GRANT_LEDGER')
 $observed=@($byName['FUNDING']|Where-Object{$_.fields.faction -eq $faction});Require funding ($observed.Count -ge 1 -and @($observed|Where-Object{$_.fields.budget -ne $expectedBudget -or (Number $_.fields.total_granted) -gt 4000 -or (!$capacity -and ((Number $_.fields.total_granted) -ne 0 -or (Number $_.fields.grant) -ne 0)) -or [double]::IsNaN((Number $_.fields.income)) -or [double]::IsNaN((Number $_.fields.income_interval_s))}).Count -eq 0) ($faction+'_NATIVE_PRICE_INCOME_AND_FUNDING_MODE')
}
$heartbeats=@($byName['HEARTBEAT']);$proof=@{};$previous=$start;$previousSample=0;$maxGap=0.0
Require liveness ($heartbeats.Count -gt 0) 'HEARTBEATS_PRESENT'
foreach($heartbeat in $heartbeats){
 if(!$heartbeat){continue};$h=$heartbeat.fields;$gap=(Number $h.elapsed_ms)-(Number $previous.fields.elapsed_ms)
 Require liveness ($heartbeat.index -gt $start.index -and $heartbeat.index -lt $done.index -and $gap -ge 0 -and $gap -le 60000 -and (Number $h.sample_id) -gt $previousSample -and (Number $h.t_ms)-(Number $h.elapsed_ms) -eq (Number $start.fields.t_ms)-(Number $start.fields.elapsed_ms)) 'MONOTONIC_LIVE_HEARTBEAT_MAX_60S'
 $maxGap=[Math]::Max($maxGap,$gap);$previous=$heartbeat;$previousSample=Number $h.sample_id
 $rows=@($samples[[string]$h.sample_id]);$rowKeys=@($rows|ForEach-Object{[string]$_.fields.faction+'/'+[string]$_.fields.numeric_slot})
 Require cohorts ((Number $h.sample_elapsed_ms) -eq (Number $h.elapsed_ms)) 'SAMPLE_TIED_TO_NATIVE_TICK'
 Require cohorts ($rows.Count -eq 12 -and @($rowKeys|Sort-Object -Unique).Count -eq 12 -and @($rowKeys|Where-Object{!$contexts.ContainsKey($_)}).Count -eq 0) 'COMPLETE_ATOMIC_TWELVE_SLOT_SAMPLE'
 $counts=@{US=0;USSR=0};$ids=@{};$rpls=@{}
 foreach($row in $rows){
  $r=$row.fields;$key=[string]$r.faction+'/'+[string]$r.numeric_slot;if(!$contexts.ContainsKey($key)){continue};$ctx=$contexts[$key]
  Require cohorts ($row.index -lt $heartbeat.index -and (Number $h.elapsed_ms)-(Number $r.elapsed_ms) -ge 0 -and (Number $h.elapsed_ms)-(Number $r.elapsed_ms) -le 5000) 'CURRENT_SAME_FRAME_SLOT_READBACK'
  if($r.exact_active -ne '1'){continue}
  $b=$ctx.boarded
  Require cohorts ($r.stage -eq '3' -and $r.phase -eq 'TRANSIT' -and $r.alive -eq '5' -and $r.operation_id -eq $ctx.operation -and (Entity $r.group) -eq (Entity $ctx.start.fields.group) -and $r.generation -eq $ctx.start.fields.generation -and (NativeId $r.vehicle $b.fields.vehicle) -and $r.rpl -eq $b.fields.rpl -and $r.lease_generation -eq $b.fields.lease_generation -and $r.vehicle_generation -eq $b.fields.vehicle_generation -and $b.index -lt $row.index) 'ACTIVE_EXACT_ORIGINAL_LEASE_GROUP'
  if($mounted){Require cohorts ($r.leg -eq '0' -and $r.preset_destination_wait -eq '1' -and $r.route_generation -eq '0' -and $r.route_waypoint_present -eq '0' -and $ctx.targets.Count -eq 0) 'ACTIVE_EXACT_MOUNTED_FIRST_DESTINATION_WAIT_ONLY'}
  else{Require cohorts ((Number $r.leg) -ge 1 -and (Number $r.displacement_m) -ge 25 -and (Number $r.moving_polls) -gt 0 -and $ctx.targets.Count -ge 1 -and $ctx.targets[0].index -lt $row.index) 'ACTIVE_EXACT_ORIGINAL_LEASE_MOVING_GROUP'}
  if($capacity){Require environment ($r.protected_members -eq '5' -and $r.vehicle_protected -eq '1' -and $r.hold_fire -eq '1') 'ACTUAL_SELECTED_CAPACITY_PROTECTION'}
  if($hierarchy){
   Require protection (Has-RecentProtection $ctx (Entity $b.fields.vehicle) $row) ($key+'_CURRENT_VEHICLE_HIERARCHY_READBACK')
   foreach($actorId in $ctx.members.Values){Require protection (Has-RecentProtection $ctx ([string]$actorId) $row) ($key+'_CURRENT_FIVE_ACTOR_HIERARCHY_READBACK')}
  }
  $id=Entity $b.fields.vehicle;Require cohorts (!$ids.ContainsKey($id) -and !$rpls.ContainsKey([string]$r.rpl)) 'SIMULTANEOUS_DISTINCT_LEASE_ASSETS';$ids[$id]=$true;$rpls[[string]$r.rpl]=$true;$counts[[string]$r.faction]++
 }
 Require cohorts ($counts.US -eq (Number $h.us_exact_active) -and $counts.USSR -eq (Number $h.ussr_exact_active)) 'RECONSTRUCTED_COUNTS_EQUAL_HEARTBEAT'
 $proof[[string]$h.sample_id]=[pscustomobject]@{us=$counts.US;ussr=$counts.USSR;sample_elapsed=(Number $h.sample_elapsed_ms);cohort=(Number $h.cohort);heartbeat=$heartbeat}
}
$tailGap=(Number $done.fields.elapsed_ms)-(Number $previous.fields.elapsed_ms);Require liveness ($tailGap -ge 0 -and $tailGap -le 60000) 'FINAL_HEARTBEAT_COVERAGE'
$cohorts=@($byName['COHORT']);Require cohorts ($cohorts.Count -eq 3 -and (@($cohorts|ForEach-Object{$_.fields.cohort}) -join ',') -eq '2,4,6') 'ORDERED_PHYSICAL_COHORTS_TWO_FOUR_SIX'
foreach($cohort in $cohorts){
 if(!$cohort){continue};$c=$cohort.fields;$n=Number $c.cohort;$first=Number $c.stable_started_elapsed_ms;$last=Number $c.sample_elapsed_ms
 Require cohorts ((Number $c.stable_ms) -ge 30000 -and $last-$first -eq (Number $c.stable_ms) -and $first -ge 0 -and $c.required_ms -eq '30000' -and $proof.ContainsKey([string]$c.sample_id)) ('COHORT_'+$n+'_REAL_STABLE_30_SECONDS')
 Require cohorts ($last -eq (Number $c.elapsed_ms)) ('COHORT_'+$n+'_EVENT_TIED_TO_NATIVE_TICK')
 $window=@($proof.Values|Where-Object{$_.sample_elapsed -ge $first -and $_.sample_elapsed -le $last}|Sort-Object sample_elapsed)
 Require cohorts ($window.Count -ge 2 -and $window[0].sample_elapsed -eq $first -and $window[-1].sample_elapsed -eq $last) ('COHORT_'+$n+'_FULL_NATIVE_SAMPLE_WINDOW')
 $previousTime=$first
 foreach($sample in $window){Require cohorts ($sample.cohort -eq $n -and $sample.us -ge $n -and $sample.ussr -ge $n -and $sample.sample_elapsed-$previousTime -le 5000) ('COHORT_'+$n+'_CONTINUOUS_REAL_COUNTS');$previousTime=$sample.sample_elapsed}
 if($proof.ContainsKey([string]$c.sample_id)){Require cohorts ($proof[[string]$c.sample_id].us -eq (Number $c.us_exact_active) -and $proof[[string]$c.sample_id].ussr -eq (Number $c.ussr_exact_active)) ('COHORT_'+$n+'_RECEIPT_MATCHES_ACTUAL_SAMPLE')}
}
if($cohorts.Count -eq 3){foreach($ctx in $contexts.Values){Require functional ($ctx.final.index -gt $cohorts[2].index) ($ctx.key+'_FINAL_TARGET_AFTER_SIX_SIMULTANEOUS_PER_SIDE')}}
$result=[ordered]@{};$passed=$true
foreach($name in $gates.Keys){$verdict='PASS';if($gates[$name].Count){$verdict='FAIL';$passed=$false};$result[$name]=@{verdict=$verdict;reasons=@($gates[$name])}}
$overall='FAIL';if($passed){$overall='PASS'}
$coverageValid=(!$hierarchy -or $result.protection.verdict -eq 'PASS')
[ordered]@{verdict=$overall;mode=$Mode;concurrency_mode=$ConcurrencyMode;protection_scope=$ProtectionScope;hierarchy_protection_proved=($hierarchy -and $coverageValid -and $result.environment.verdict -eq 'PASS');classification=$classification;notmovingproof=$mounted;muster_policy='PER_GROUP_BEFORE_VEHICLE';concurrent_moving_cohorts_proved=(!$mounted -and $coverageValid -and $result.cohorts.verdict -eq 'PASS' -and $result.environment.verdict -eq 'PASS');mounted_staging_cohorts_proved=($mounted -and $coverageValid -and $result.cohorts.verdict -eq 'PASS' -and $result.environment.verdict -eq 'PASS');normal_campaign_evidence=(!$capacity -and !$mounted);minimum_seconds=$MinimumSeconds;log_path=$LogPath;full_sibling_logs=@($siblings.FullName);gates=$result;observations=@{heartbeats=$heartbeats.Count;max_heartbeat_gap_ms=$maxGap;wall_seconds=$wall;granted=$granted;native_error_count=$nativeErrors.Count};native_errors=@($nativeErrors);resource_baseline='RECORDED_SEPARATELY_NO_WHITELIST'}|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $root 'audit-summary.json')
foreach($name in $result.Keys){"MULTI_GATE mode=$Mode concurrency_mode=$ConcurrencyMode gate=$name verdict=$($result[$name].verdict) reasons=$($result[$name].reasons -join ',')"}
"MULTI_AUDIT mode=$Mode concurrency_mode=$ConcurrencyMode verdict=$overall classification=$classification notmovingproof=$notMovingFlag"
if(!$passed){exit 1}
exit 0
