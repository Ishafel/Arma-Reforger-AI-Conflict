param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$NativeExitPath,[ValidateSet('Standard','Capacity','Injected')][string]$Mode='Standard')
$ErrorActionPreference='Stop'
function Get-AICFReplacementStressFailures([string]$Raw,[int]$NativeExit,[string]$Mode='Standard'){
 $failures=[Collections.Generic.List[string]]::new()
 if($NativeExit -ne 0){$failures.Add('NATIVE_EXIT')}
 if(!$Raw.Contains('Game destroyed.')){$failures.Add('STOPPED_LOG')}
 if(([regex]::Matches($Raw,'\[REPLACEMENT_STRESS_START\]')).Count -ne 1){$failures.Add('UNIQUE_START')}
 $waves=2
 if($Mode -ne 'Standard'){$waves=1}
 if(([regex]::Matches($Raw,"\[REPLACEMENT_STRESS_DONE\] waves=$waves failures=0 elapsed_ms=\d+")).Count -ne 1){$failures.Add('EXACT_DONE')}
 if($Mode -ne 'Standard'){
  $classification='CONTROLLED_NATIVE_CAPACITY_STRESS';$changedLimit='0';$mechanism='KILL_OTHER_OWN_MANAGED_AI'
  if($Mode -eq 'Injected'){$classification='CONTROLLED_NATIVE_CAPACITY_INJECTION';$changedLimit='1';$mechanism='RESTORE_ORIGINAL_NATIVE_LIMIT'}
  if($Raw -notmatch ('classification='+$classification)){$failures.Add('CAPACITY_CLASSIFICATION')}
  $pressure=[regex]::Match($Raw,('\[REPLACEMENT_STRESS_PRESSURE\] active=(\d+) intended=(\d+) limit=(\d+) free=(-?\d+) desired=5 group_can_activate=(?:-1|[01]) limit_changed='+$changedLimit+' importance_changed=0'))
  if(!$pressure.Success -or [int]$pressure.Groups[4].Value -ge 5 -or [int]$pressure.Groups[4].Value -ne [int]$pressure.Groups[3].Value-[Math]::Max([int]$pressure.Groups[1].Value,[int]$pressure.Groups[2].Value)){$failures.Add('CAPACITY_ACTUAL_PRESSURE')}
  $hold=[regex]::Match($Raw,'\[REPLACEMENT_STRESS_PRESSURE_HOLD\][^\r\n]*free=(-?\d+) request_age_ms=(\d+) actual=(\d+) global_can_activate=([01]) phase=(SPAWNING|WAITING_NO_NEW_GENERATION)[^\r\n]*')
  $validHold=$hold.Success -and [int]$hold.Groups[1].Value -lt 5 -and [int]$hold.Groups[3].Value -lt 5
  if($hold.Groups[5].Value -eq 'SPAWNING'){$validHold=$validHold -and [int]$hold.Groups[2].Value -ge 5000 -and $hold.Groups[4].Value -eq '0'}
  else{$validHold=$validHold -and [int]$hold.Groups[2].Value -eq 0 -and [int]$hold.Groups[3].Value -eq 0 -and $hold.Value -match 'no_new_generation=1 elapsed_since_kill_ms=(\d+)' -and [int]$Matches[1] -ge 35000}
  if(!$validHold){$failures.Add('CAPACITY_NATIVE_HOLD')}
  if($hold.Success -and $hold.Groups[5].Value -eq 'WAITING_NO_NEW_GENERATION'){
   $identity=[regex]::Match($hold.Value,'faction=(US|USSR) numeric_slot=(\d+) generation=(\d+)')
   $waits=@([regex]::Matches($Raw,'\[GROUP_SPAWN_NATIVE_CAPACITY_WAIT\][^\r\n]+')|Where-Object{
    $_.Value -match ('faction='+$identity.Groups[1].Value+' slot='+$identity.Groups[2].Value+'\b') -and
    $_.Value -match ('generation='+$identity.Groups[3].Value+'\b') -and $_.Value -match 'generation_unchanged=1' -and $_.Index -lt $hold.Index -and $_.Index -gt $pressure.Index -and
    $_.Value -match 'active=(\d+) intended=(\d+) hard_limit=(\d+) pending=(\d+) requested=(\d+) projected=(\d+) global_can_activate=([01])' -and
    [int]$Matches[5] -eq 5 -and [int]$Matches[6] -eq [Math]::Max([int]$Matches[1],[int]$Matches[2])+[int]$Matches[4]+[int]$Matches[5] -and
    ([int]$Matches[6] -gt [int]$Matches[3] -or $Matches[7] -eq '0')
   })
   if($waits.Count -lt 1){$failures.Add('CAPACITY_PRODUCTION_ADMISSION_WAIT')}
  }
  $release=[regex]::Match($Raw,('\[REPLACEMENT_STRESS_CAPACITY_RELEASE\] elapsed_since_kill_ms=(\d+) active_before=\d+ intended_before=\d+ limit=\d+ mechanism='+$mechanism+' limit_changed='+$changedLimit+' importance_changed=0'))
  if(!$release.Success -or [int]$release.Groups[1].Value -lt 65000 -or $release.Index -le $pressure.Index -or $release.Index -le $hold.Index){$failures.Add('CAPACITY_REAL_RELEASE')}
  if($hold.Success -and $hold.Groups[5].Value -eq 'WAITING_NO_NEW_GENERATION' -and $release.Success){
   $during=$Raw.Substring($pressure.Index,$release.Index-$pressure.Index)
   if($during -match '\[(?:ROSTER_SPAWN_REQUESTED|SPAWN_BOUND|DEPLOYMENT_RESERVED)\][^\r\n]*faction=(US|USSR) slot=[012]\b'){$failures.Add('CAPACITY_NO_EARLY_SPAWN_OR_DEBIT')}
   $resumes=@([regex]::Matches($Raw,'\[GROUP_SPAWN_NATIVE_CAPACITY_RELEASED\][^\r\n]+')|Where-Object{
    $_.Index -gt $release.Index -and $_.Value -match ('faction='+$identity.Groups[1].Value+' slot='+$identity.Groups[2].Value+'\b') -and $_.Value -match ('generation='+$identity.Groups[3].Value+'\b') -and
    $_.Value -match 'active=(\d+) intended=(\d+) hard_limit=(\d+) pending=(\d+) requested=(\d+) projected=(\d+) global_can_activate=1' -and
    [int]$Matches[5] -eq 5 -and [int]$Matches[6] -eq [Math]::Max([int]$Matches[1],[int]$Matches[2])+[int]$Matches[4]+[int]$Matches[5] -and [int]$Matches[6] -le [int]$Matches[3]
   })
   if($resumes.Count -lt 1){$failures.Add('CAPACITY_PRODUCTION_ADMISSION_RELEASE')}
  }
  if($Mode -eq 'Injected'){
   $restored=[regex]::Match($Raw,'\[REPLACEMENT_STRESS_NATIVE_LIMIT_RESTORED\] original=(\d+) actual=(\d+)')
   $original=[regex]::Match($Raw,'limit_changed=1 importance_changed=0 original_limit=(\d+) classification=CONTROLLED_NATIVE_CAPACITY_INJECTION')
   if(!$restored.Success -or !$original.Success -or $restored.Groups[1].Value -ne $restored.Groups[2].Value -or $restored.Groups[1].Value -ne $original.Groups[1].Value){$failures.Add('INJECTED_LIMIT_RESTORED')}
  }
 }
 if($Raw -match '\[GROUP_SPAWN_TIMEOUT\]|SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|Access violation'){$failures.Add('FULL_FATAL')}
 $kills=@{};$ready=@{}
 foreach($match in [regex]::Matches($Raw,'\[REPLACEMENT_STRESS_KILL\] wave=(\d+) faction=(US|USSR) numeric_slot=(\d+) generation=(\d+) group=(\S+) actual=(\d+)')){
  $key="$($match.Groups[1].Value):$($match.Groups[2].Value):$($match.Groups[3].Value)"
  if($kills.ContainsKey($key)){$failures.Add('DUPLICATE_KILL');continue}
  $kills[$key]=$match
 }
 foreach($match in [regex]::Matches($Raw,'\[REPLACEMENT_STRESS_READY\] wave=(\d+) faction=(US|USSR) numeric_slot=(\d+) generation=(\d+) exact=(\d+) first_generation=(\d+) actual=(\d+)')){
  $key="$($match.Groups[1].Value):$($match.Groups[2].Value):$($match.Groups[3].Value)"
  if($ready.ContainsKey($key)){$failures.Add('DUPLICATE_READY');continue}
  $ready[$key]=$match
  if(!$kills.ContainsKey($key)){$failures.Add('READY_WITHOUT_KILL');continue}
  if($match.Index -le $kills[$key].Index){$failures.Add('READY_BEFORE_KILL')}
  if([int]$match.Groups[4].Value -ne [int]$kills[$key].Groups[4].Value+1){$failures.Add('GENERATION_SKIP')}
  if($match.Groups[5].Value -ne '1' -or $match.Groups[6].Value -ne '1' -or $match.Groups[7].Value -ne '5'){$failures.Add('EXACT_FIVE')}
 }
 foreach($wave in 1..$waves){foreach($faction in @('US','USSR')){foreach($slot in 0..2){
  $key="${wave}:${faction}:${slot}"
  if(!$kills.ContainsKey($key) -or !$ready.ContainsKey($key)){$failures.Add('MISSING_CASE')}
 }}}
 if($Mode -ne 'Standard' -and $hold.Success -and $hold.Groups[5].Value -eq 'WAITING_NO_NEW_GENERATION'){
  $killKey="1:$($identity.Groups[1].Value):$($identity.Groups[2].Value)"
  if(!$kills.ContainsKey($killKey) -or $kills[$killKey].Groups[4].Value -ne $identity.Groups[3].Value){$failures.Add('CAPACITY_ORIGINAL_GENERATION')}
 }
 if($kills.Count -ne $waves*6 -or $ready.Count -ne $waves*6){$failures.Add('EXACT_CASE_COUNT')}
 return $failures.ToArray()
}
if($MyInvocation.InvocationName -eq '.'){return}
$taskRaw=Get-Content -LiteralPath $LogPath -Raw
$taskNative=[int](Get-Content -LiteralPath $NativeExitPath -Raw).Trim()
$taskFailures=@(Get-AICFReplacementStressFailures $taskRaw $taskNative $Mode)
$taskResourceCount=0
foreach($taskSibling in @(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -Filter '*.log' -File)){
 $taskSiblingRaw=[IO.File]::ReadAllText($taskSibling.FullName)
 if($taskSiblingRaw -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Access violation|Attempt to modify locked entity'){
  $taskFailures+='FULL_SIBLING_LOG_FATAL'
 }
 $taskResourceCount+=([regex]::Matches($taskSiblingRaw,'(?m)^.*\s\([EF]\):.*$')).Count
}
$taskFunctional='PASS';if($taskFailures.Count){$taskFunctional='FAIL'}
$taskResource='PASS';if($taskResourceCount){$taskResource='FAIL'}
Write-Output "gate=functional verdict=$taskFunctional failures=$($taskFailures -join ',')"
Write-Output "gate=strict_native_resources verdict=$taskResource error_lines=$taskResourceCount"
if($taskFailures.Count -or $taskResourceCount){exit 1}
exit 0
