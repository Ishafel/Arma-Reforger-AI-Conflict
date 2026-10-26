[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$LogPath,
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [Parameter(Mandatory)][ValidateSet('US','USSR')][string]$Faction,
 [ValidateRange(1,7200)][int]$MinimumSeconds=1800
)
$ErrorActionPreference='Stop'
$gates=[ordered]@{}
foreach($name in @('duration','liveness','stopped_native','provenance','functional','fatal_errors','resource_errors')){
 $gates[$name]=[Collections.Generic.List[string]]::new()
}
function Require([string]$gate,[bool]$condition,[string]$reason){if(!$condition -and !$gates[$gate].Contains($reason)){$gates[$gate].Add($reason)}}
function Fields([string]$line){
 $result=@{}
 foreach($item in [regex]::Matches($line,'(?:^|\s)([A-Za-z_][A-Za-z0-9_]*)=(.*?)(?=\s+[A-Za-z_][A-Za-z0-9_]*=|$)')){$result[$item.Groups[1].Value]=$item.Groups[2].Value.Trim()}
 return $result
}
function Number($value){
 $number=0.0
 if(![double]::TryParse([string]$value,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$number) -or [double]::IsNaN($number) -or [double]::IsInfinity($number)){return [double]::NaN}
 return $number
}
function Entity($value){
 $identity=([string]$value).Trim()
 if($identity -match '^(0x[0-9a-fA-F]+)\s*(?:\{\s*\})?$'){return $Matches[1].ToUpperInvariant()}
 return $identity
}
function NativeEntityMatches($actual,$original){
 $actualId=Entity $actual;$originalId=Entity $original
 if($actualId -eq $originalId){return $true}
 # string.Format старого native receipt печатает EntityID как signed low32.
 # Полный ID независимо привязывается через SEAT_REQUESTED той же позиции/seat.
 if($originalId -notmatch '^0X([0-9A-F]{1,16})$' -or $actualId -notmatch '^-?\d+$'){return $false}
 try{
  $actualNumber=[long]::Parse($actualId,[Globalization.CultureInfo]::InvariantCulture)
  if($actualNumber -lt -2147483648 -or $actualNumber -gt 4294967295){return $false}
  $expectedLow=[Convert]::ToUInt64($originalId.Substring(2),16) -band 4294967295L
  return ($actualNumber -band 4294967295L) -eq $expectedLow
 }catch{return $false}
}
function Events([string]$name){return @($script:events|Where-Object{$_.name -eq $name})}
function Single([string]$name){
 $items=@(Events $name)
 Require functional ($items.Count -eq 1) ('EVENT_'+$name+'_ONCE')
 if($items.Count){return $items[0]}
 return [pscustomobject]@{name=$name;index=-1;fields=@{}}
}
$log=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $LogPath).Path)
$root=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
[void][IO.Directory]::CreateDirectory($root)
$siblings=@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -File -Filter '*.log')
$nativeErrors=[Collections.Generic.List[object]]::new()
$fatalPattern='SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation|Unknown class .SCR_AIGoalReaction_GetOutVehicle'
foreach($file in $siblings){
 $lineNumber=0
 foreach($line in [IO.File]::ReadLines($file.FullName)){
  $lineNumber++
  if($line -match $fatalPattern){
   $nativeErrors.Add([pscustomobject]@{kind='fatal';file=$file.FullName;line=$lineNumber;text=$line})
   Require fatal_errors $false 'FULL_SIBLING_LOG_FATAL'
  }elseif($line -match '\([EF]\)\s*:'){
   $nativeErrors.Add([pscustomobject]@{kind='native_error';file=$file.FullName;line=$lineNumber;text=$line})
   Require resource_errors $false 'NATIVE_ERROR_LINES_REQUIRE_BASELINE_REVIEW'
  }
  if($line -match '\[ACCEPTANCE_FAILURE_LATCHED\]|\[MOB_EGRESS_DEADLINE_MISSED\]|\[MEANINGFUL_TASK_DEADLINE_MISSED\]|\[VEHICLE_SOAK_(?:FAILURE|ABORTED)\]'){
   Require functional $false 'ACCEPTANCE_OR_TASK_FAILURE'
  }
 }
}
$script:events=@()
foreach($item in [regex]::Matches($log,'\[VEHICLE_SOAK_([A-Z_]+)\]([^\r\n]*)')){
 $fields=Fields $item.Groups[2].Value
 Require functional ($fields.faction -eq $Faction -and $fields.numeric_slot -eq '0') 'SOAK_FACTION_SLOT'
 $script:events+=[pscustomobject]@{name=$item.Groups[1].Value;index=$item.Index;fields=$fields}
}
$starts=@(Events START);$dones=@(Events DONE);$heartbeats=@(Events HEARTBEAT)
Require duration ($starts.Count -eq 1 -and $dones.Count -eq 1) 'START_DONE_ONCE'
$minimumMs=$MinimumSeconds*1000
$start=$null;$done=$null;$maxGap=0.0;$wallSeconds=[double]::NaN
if($starts.Count -eq 1){$start=$starts[0]}
if($dones.Count -eq 1){$done=$dones[0]}
if($starts.Count -eq 1 -and $dones.Count -eq 1){
 $start=$starts[0];$done=$dones[0]
 $elapsed=(Number $done.fields.elapsed_ms)-(Number $start.fields.elapsed_ms)
 $ticks=(Number $done.fields.t_ms)-(Number $start.fields.t_ms)
 Require duration ($done.index -gt $start.index -and $elapsed -ge $minimumMs -and $ticks -ge $minimumMs) 'MINIMUM_ENGINE_DURATION'
 Require duration ((Number $start.fields.elapsed_ms) -ge 0 -and (Number $start.fields.requested_duration_ms) -ge $minimumMs -and $done.fields.duration_satisfied -eq '1' -and (Number $done.fields.requested_duration_ms) -eq (Number $start.fields.requested_duration_ms) -and $elapsed -ge (Number $done.fields.requested_duration_ms)) 'REQUESTED_DURATION_SATISFIED'
 Require duration (((Number $start.fields.t_ms)-(Number $start.fields.elapsed_ms)) -eq ((Number $done.fields.t_ms)-(Number $done.fields.elapsed_ms))) 'IDENTICAL_ENGINE_CLOCK_ORIGIN'
 Require functional ($start.fields.desired -eq '5' -and $start.fields.config_accepted -eq '1' -and $start.fields.home_order_accepted -eq '1' -and $start.fields.truce -eq '0' -and $start.fields.invulnerability -eq '0' -and $start.fields.teleport -eq '0') 'NORMAL_GAME_INITIAL_COMMAND'
 Require functional ($done.fields.failures -eq '0' -and $done.fields.route_complete -eq '1' -and $done.fields.phase -eq '7') 'FUNCTIONAL_DONE'
 $previous=$start
 Require liveness ($heartbeats.Count -gt 0) 'HEARTBEATS_PRESENT'
 foreach($heartbeat in $heartbeats){
  $gap=(Number $heartbeat.fields.elapsed_ms)-(Number $previous.fields.elapsed_ms)
  $tickGap=(Number $heartbeat.fields.t_ms)-(Number $previous.fields.t_ms)
  Require liveness ($heartbeat.index -ge $start.index -and $heartbeat.index -lt $done.index -and $gap -ge 0 -and $tickGap -ge 0 -and ($previous -eq $start -or ($gap -gt 0 -and $tickGap -gt 0))) 'MONOTONIC_HEARTBEATS'
  Require liveness ($gap -le 60000 -and $tickGap -le 60000) 'HEARTBEAT_GAP_MAX_60S'
  Require liveness ([Math]::Abs($gap-$tickGap) -le 1) 'HEARTBEAT_CLOCK_CONSISTENCY'
  $maxGap=[Math]::Max($maxGap,[Math]::Max($gap,$tickGap));$previous=$heartbeat
 }
 $tailGap=(Number $done.fields.elapsed_ms)-(Number $previous.fields.elapsed_ms)
 $maxGap=[Math]::Max($maxGap,$tailGap)
 Require liveness ($tailGap -ge 0 -and $tailGap -le 60000) 'FINAL_HEARTBEAT_GAP_MAX_60S'
 Require stopped_native ($log.IndexOf('Game destroyed.',$done.index,[StringComparison]::Ordinal) -gt $done.index) 'STOPPED_AFTER_DONE'
}else{Require liveness $false 'NO_COMPLETE_OBSERVATION_WINDOW'}
Require duration (@(Events ABORTED).Count -eq 0) 'NO_EARLY_ABORT'
$timingPath=Join-Path $root 'timing.json'
if(Test-Path -LiteralPath $timingPath){
 try{
  $timing=[IO.File]::ReadAllText($timingPath)|ConvertFrom-Json
  $wallSeconds=Number $timing.wall_seconds
  Require duration ($wallSeconds -ge $MinimumSeconds -and $timing.faction -eq $Faction -and (Number $timing.requested_seconds) -ge $MinimumSeconds) 'MINIMUM_WALL_DURATION'
  $wallClock=([DateTimeOffset]$timing.ended_utc-[DateTimeOffset]$timing.started_utc).TotalSeconds
  Require duration ($wallClock -ge $MinimumSeconds -and [Math]::Abs($wallClock-$wallSeconds) -le 1) 'WALL_TIMING_CONSISTENCY'
 }catch{Require duration $false 'INVALID_TIMING_JSON'}
}else{Require duration $false 'TIMING_JSON_MISSING'}
$exitPath=Join-Path $root 'native-exit.txt'
Require stopped_native ((Test-Path -LiteralPath $exitPath) -and [IO.File]::ReadAllText($exitPath).Trim() -eq '0') 'NATIVE_EXIT_ZERO'
Require stopped_native ($log.Contains('[ROSTER_READY]') -and $log.Contains('Game destroyed.')) 'FULL_STOPPED_ROSTER_LOG'
$launcherPath=Join-Path $root 'launcher.txt'
if(Test-Path -LiteralPath $launcherPath){
 try{
  $launcher=[IO.File]::ReadAllText($launcherPath)
  $manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
  Require provenance ($manifests.Count -eq 1) 'SINGLE_RUNTIME_MANIFEST'
  if($manifests.Count -eq 1){
   $manifest=$manifests[0].Groups[1].Value|ConvertFrom-Json
   $argv=@($manifest.arguments)
   $cli=[regex]::Match($log,'CLI Params: ([^\r\n]+)').Groups[1].Value
   foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehicleSoakFaction','-aicfVehicleSoakDurationMs')){
    $argument=[array]::IndexOf($argv,$flag)
    Require provenance ($argument -ge 0 -and $argument+1 -lt $argv.Count) ('MANIFEST_'+$flag)
    if($argument -ge 0 -and $argument+1 -lt $argv.Count){
     $value=[string]$argv[$argument+1]
     $cliArgument='(?:^|\s)'+[regex]::Escape($flag)+'\s+"?'+[regex]::Escape($value)+'"?(?=\s|$)'
     Require provenance ($cli -match $cliArgument) ('ACTUAL_CLI_'+$flag)
     if($flag -eq '-aicfVehicleSoakFaction'){Require provenance ($value -eq $Faction) 'MANIFEST_FACTION'}
     if($flag -eq '-aicfVehicleSoakDurationMs'){Require provenance ((Number $value) -ge $minimumMs -and $start -and (Number $value) -eq (Number $start.fields.requested_duration_ms)) 'MANIFEST_DURATION'}
     if($flag -eq '-profile'){
      $profile=[IO.Path]::GetFullPath($value).TrimEnd([IO.Path]::DirectorySeparatorChar)
      # Архивные/readback копии могут находиться вне profile. Его identity уже
      # независимо подтверждена точным -profile в полном native CLI Params.
      Require provenance ([IO.Path]::GetFullPath([string]$manifest.profile) -eq $profile) 'LOG_PROFILE_IDENTITY'
     }
    }
   }
  }
 }catch{Require provenance $false 'INVALID_RUNTIME_MANIFEST'}
}else{Require provenance $false 'LAUNCHER_MANIFEST_MISSING'}
Require functional (@(Events FAILURE).Count -eq 0 -and @(Events ABORTED).Count -eq 0) 'SOAK_FAILURE_OR_ABORT'
$recruited=Single RECRUITED;$vehicle=Single VEHICLE;$trip=Single TRIP;$boarded=Single BOARDED;$target=Single TARGET;$arrival=Single ARRIVAL;$dismounted=Single DISMOUNTED;$foot=Single FOOT_ARRIVAL
$accepted=@(Events RECRUIT_REQUEST|Where-Object{$_.fields.accepted -eq '1' -and $_.fields.desired -eq '5' -and $_.fields.authority_path -eq 'SERVER_FIXTURE_DOWNSTREAM' -and $_.index -lt $recruited.index})
Require functional ($accepted.Count -ge 1) 'REAL_RECRUIT_REQUEST_ACCEPTED'
Require functional ($recruited.fields.alive -eq '5' -and $recruited.fields.desired -eq '5' -and $start -and (Entity $recruited.fields.group) -eq (Entity $start.fields.group) -and $recruited.fields.generation -eq $start.fields.generation -and (Number $recruited.fields.initial_alive) -ge 0 -and (Number $recruited.fields.initial_alive) -lt 5) 'RECRUITED_CURRENT_GROUP_FIVE'
$members=@(Events MEMBER)
$memberIds=@($members|ForEach-Object{Entity $_.fields.member})
$membersByPosition=@{}
foreach($member in $members){$membersByPosition[[string]$member.fields.position]=Entity $member.fields.member}
$positions=@($members|ForEach-Object{[string]$_.fields.position}|Sort-Object -Unique)
Require functional ($members.Count -eq 5 -and @($memberIds|Sort-Object -Unique).Count -eq 5 -and @($memberIds|Where-Object{!$_ -or $_ -eq 'NONE'}).Count -eq 0 -and ($positions -join ',') -eq '0,1,2,3,4' -and @($members|Where-Object{$_.index -le $recruited.index -or $_.index -ge $vehicle.index}).Count -eq 0) 'FIVE_UNIQUE_ORIGINAL_MEMBERS'
$prefab=[string]$vehicle.fields.prefab
$model=$prefab.EndsWith('/UAZ469.et',[StringComparison]::Ordinal)
if($Faction -eq 'US'){$model=$prefab.Contains('/M1025_') -and $prefab.Contains('M2HB') -and $prefab.EndsWith('.et',[StringComparison]::Ordinal)}
Require functional ($model -and $vehicle.fields.seats -eq '5' -and $vehicle.fields.assigned -eq '5' -and (Number $vehicle.fields.cost) -gt 0 -and (Number $vehicle.fields.revision) -ge 1 -and $vehicle.fields.authority_path -eq 'SERVER_FIXTURE_DOWNSTREAM') 'FACTION_FIVE_SEAT_MODEL'
$operation=[string]$trip.fields.operation_id
Require functional ($operation -match ('^trip-'+$Faction+'-0-\d+-\d+$')) 'MANAGED_SLOT_OPERATION'
foreach($event in @($boarded,$target,$arrival,$dismounted,$foot)){Require functional ($event.fields.operation_id -eq $operation -and $operation) 'OPERATION_CONTINUITY'}
Require functional ($recruited.index -lt $vehicle.index -and $vehicle.index -lt $trip.index -and $trip.index -lt $boarded.index -and $boarded.index -lt $target.index -and $target.index -lt $arrival.index -and $arrival.index -lt $dismounted.index -and $dismounted.index -lt $foot.index -and $done -and $foot.index -lt $done.index) 'FUNCTIONAL_EVENT_ORDER'
Require functional ($boarded.fields.seats -eq '5' -and $boarded.fields.occupants -eq '5') 'EXACT_FIVE_BOARDED'
Require functional ((Entity $boarded.fields.vehicle) -ne '' -and (Entity $boarded.fields.vehicle) -ne 'NONE' -and (Entity $boarded.fields.vehicle) -eq (Entity $target.fields.vehicle) -and (Entity $boarded.fields.vehicle) -eq (Entity $arrival.fields.vehicle)) 'SAME_VEHICLE_IDENTITY'
Require functional ((Number $target.fields.intent_revision) -ge 1 -and $arrival.fields.intent_revision -eq $target.fields.intent_revision -and $foot.fields.intent_revision -eq $target.fields.intent_revision) 'UNCHANGED_POINT_INTENT_REVISION'
Require functional ($target.fields.position -and $target.fields.position -eq $foot.fields.position) 'UNCHANGED_POINT_POSITION'
Require functional ($arrival.fields.passed -eq '1' -and (Number $arrival.fields.distance_m) -ge 0 -and (Number $arrival.fields.distance_m) -le (Number $target.fields.dismount_radius_m)+10 -and (Number $arrival.fields.displacement_m) -ge 100 -and (Number $arrival.fields.moving_polls) -ge 5) 'PHYSICAL_DRIVE_ARRIVAL'
Require functional ($dismounted.fields.alive -eq '5' -and $dismounted.fields.in_vehicle -eq '0' -and $dismounted.fields.normal_complete -eq '1') 'NORMAL_COMPLETE_DISMOUNT'
Require functional ($foot.fields.alive -eq '5' -and (Number $foot.fields.farthest_m) -ge 0 -and (Number $foot.fields.farthest_m) -le 35) 'FIVE_AT_FOOT_DESTINATION'
# Штатные сообщения подтверждают платный найм и покупку независимо от fixture.
$production=@()
foreach($item in [regex]::Matches($log,'\[(INFANTRY_RECRUITMENT_STARTED|INFANTRY_RECRUITMENT_FINISHED|INFANTRY_RECRUIT_JOINED|VEHICLE_PRESET_PURCHASE|VEHICLE_PRESET_SEAT_REQUESTED|VEHICLE_PRESET_OCCUPANT|VEHICLE_STATE_CHANGED|DISEMBARK_COMPLETE)\]([^\r\n]*)')){
 $production+=[pscustomobject]@{name=$item.Groups[1].Value;index=$item.Index;fields=(Fields $item.Groups[2].Value)}
}
$recruitProof=@($production|Where-Object{$_.fields.faction -eq $Faction -and $_.fields.slot -eq '0' -and $_.fields.source -eq 'PLAYER_RECRUIT' -and $start -and (Entity $_.fields.group) -eq (Entity $start.fields.group) -and $_.fields.generation -eq $start.fields.generation -and $_.index -lt $recruited.index})
Require functional (@($recruitProof|Where-Object{$_.name -eq 'INFANTRY_RECRUITMENT_STARTED'}).Count -ge 1 -and @($recruitProof|Where-Object{$_.name -eq 'INFANTRY_RECRUITMENT_FINISHED' -and $_.fields.reason -eq 'ROSTER_COMPLETE_OR_EMPTY'}).Count -ge 1) 'NATIVE_PLAYER_RECRUITMENT_LIFECYCLE'
$joins=@($recruitProof|Where-Object{$_.name -eq 'INFANTRY_RECRUIT_JOINED' -and (Number $_.fields.cost) -gt 0})
$joinedIds=@($joins|ForEach-Object{Entity $_.fields.character}|Sort-Object -Unique)
Require functional ($joins.Count -eq 5-(Number $recruited.fields.initial_alive) -and $joinedIds.Count -eq $joins.Count -and @($joinedIds|Where-Object{$_ -notin $memberIds}).Count -eq 0) 'PAID_RECRUITS_IN_ORIGINAL_ROSTER'
$purchases=@($production|Where-Object{$_.name -eq 'VEHICLE_PRESET_PURCHASE' -and $_.fields.operation -eq $operation})
Require functional ($purchases.Count -eq 1 -and $purchases[0].fields.prefab -eq $prefab -and (Number $purchases[0].fields.supplies) -eq (Number $vehicle.fields.cost) -and $purchases[0].index -gt $vehicle.index -and $purchases[0].index -lt $boarded.index) 'ONE_REAL_VEHICLE_PURCHASE'
$occupants=@($production|Where-Object{$_.name -eq 'VEHICLE_PRESET_OCCUPANT' -and $_.fields.operation -eq $operation})
Require functional ($occupants.Count -eq 5 -and (@($occupants|ForEach-Object{$_.fields.position}|Sort-Object -Unique) -join ',') -eq '0,1,2,3,4' -and @($occupants|ForEach-Object{$_.fields.seat}|Sort-Object -Unique).Count -eq 5 -and @($occupants|ForEach-Object{$_.fields.member}|Sort-Object -Unique).Count -eq 5 -and @($occupants|Where-Object{!$_.fields.member -or $_.fields.member -ne $_.fields.occupant -or $_.index -ge $boarded.index -or $_.index -le $vehicle.index}).Count -eq 0) 'FIVE_NATIVE_EXACT_OCCUPANTS'
$projections=@($memberIds|ForEach-Object{if($_ -match '^0X[0-9A-F]{1,16}$'){([Convert]::ToUInt64($_.Substring(2),16) -band 4294967295L).ToString()}else{$_}})
Require functional (@($projections|Sort-Object -Unique).Count -eq $memberIds.Count) 'NATIVE_MEMBER_PROJECTION_UNAMBIGUOUS'
foreach($occupant in $occupants){
 $original=$membersByPosition[[string]$occupant.fields.position]
 $requests=@($production|Where-Object{$_.name -eq 'VEHICLE_PRESET_SEAT_REQUESTED' -and $_.fields.operation -eq $operation -and $_.fields.position -eq $occupant.fields.position -and $_.fields.seat -eq $occupant.fields.seat -and (Entity $_.fields.member) -eq $original -and $_.index -lt $occupant.index -and $_.index -gt $vehicle.index})
 Require functional ($original -and $requests.Count -ge 1 -and (NativeEntityMatches $occupant.fields.member $original) -and (NativeEntityMatches $occupant.fields.occupant $original)) 'NATIVE_OCCUPANT_ORIGINAL_POSITION_MEMBER'
}
$clear=@($production|Where-Object{$_.name -eq 'DISEMBARK_COMPLETE' -and $_.fields.operation_id -eq $operation -and $_.fields.logical_occupants -eq '0' -and $_.fields.transitions -eq '0' -and $_.fields.inside_bounds -eq '0' -and $_.index -gt $arrival.index -and $_.index -lt $dismounted.index})
Require functional ($clear.Count -eq 1) 'PHYSICAL_NATIVE_DISEMBARK_COMPLETE'
$complete=@($production|Where-Object{$_.name -eq 'VEHICLE_STATE_CHANGED' -and $_.fields.operation_id -eq $operation -and $_.fields.previous_state -eq 'HANDOFF' -and $_.fields.state -eq 'COMPLETE' -and $_.index -gt $arrival.index -and $_.index -lt $dismounted.index})
Require functional ($complete.Count -eq 1) 'NATIVE_NORMAL_TRIP_COMPLETE'
Require functional (@($production|Where-Object{$_.name -eq 'VEHICLE_STATE_CHANGED' -and $_.fields.operation_id -eq $operation -and $_.fields.state -in @('FALLBACK','FAILED_CLOSED')}).Count -eq 0) 'NO_SELECTED_TRIP_FALLBACK'
$resultGates=[ordered]@{};$allPass=$true
foreach($name in $gates.Keys){
 $passed=$gates[$name].Count -eq 0
 if(!$passed){$allPass=$false}
 $verdict='FAIL';if($passed){$verdict='PASS'}
 $resultGates[$name]=[ordered]@{verdict=$verdict;reasons=@($gates[$name])}
}
$verdict='FAIL';if($allPass){$verdict='PASS'}
$summary=[ordered]@{verdict=$verdict;faction=$Faction;minimum_seconds=$MinimumSeconds;log_path=$LogPath;full_sibling_logs=@($siblings.FullName);gates=$resultGates;observations=@{heartbeats=$heartbeats.Count;max_heartbeat_gap_ms=$maxGap;wall_seconds=$wallSeconds;operation_id=$operation;native_error_count=$nativeErrors.Count};native_errors=@($nativeErrors);resource_baseline='ERRORS_RECORDED_SEPARATELY_NO_IMPLICIT_WHITELIST'}
$summary|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $root 'audit-summary.json')
foreach($name in $resultGates.Keys){"SOAK_GATE faction=$Faction gate=$name verdict=$($resultGates[$name].verdict) reasons=$($resultGates[$name].reasons -join ',')"}
"SOAK_AUDIT faction=$Faction verdict=$verdict summary=$(Join-Path $root 'audit-summary.json')"
if(!$allPass){exit 1}
exit 0
