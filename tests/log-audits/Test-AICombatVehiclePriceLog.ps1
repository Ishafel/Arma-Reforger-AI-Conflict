param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$LauncherPath,[Parameter(Mandatory)][string]$NativeExitPath,[switch]$Baseline,[switch]$RequirePurchase)
$ErrorActionPreference='Stop'
$raw=Get-Content -LiteralPath $LogPath -Raw
$failures=[Collections.Generic.List[string]]::new()
function Need([bool]$ok,[string]$rule){if(!$ok){$failures.Add($rule)}}
Need ((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -eq '0') 'NATIVE_EXIT_ZERO'
Need ($raw.Contains('Game destroyed.')) 'FULL_STOPPED'
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
$manifest=[regex]::Match($launcher,'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)')
Need $manifest.Success 'CANONICAL_MANIFEST'
if($manifest.Success){$arguments=@(($manifest.Groups[1].Value|ConvertFrom-Json).arguments);$i=[array]::IndexOf($arguments,'-aicfAIPriceProbe');Need ($i -ge 0 -and $arguments[$i+1] -eq '1') 'NATIVE_PROBE_FLAG'}
$rows=@()
foreach($line in $raw -split "`n"){
 if($line -notmatch '\[AI_PRICE_CASE\]'){continue}
 $fields=@{};foreach($m in [regex]::Matches($line,'(\w+)=(\S+)')){$fields[$m.Groups[1].Value]=$m.Groups[2].Value};$rows+=$fields
 Need ($fields.passed -eq '1') 'NATIVE_CASE_PASS'
 if(!$fields.ContainsKey('raw')){continue}
 $native=[long]$fields.raw;$expected=$native
 if(!$Baseline -and ($fields.armed -eq '1' -or $fields.armor -eq '1' -or $fields.apc -eq '1')){$expected=[math]::Max(1,[math]::Floor(($native+1)/3))}
 Need ($native -gt 0 -and [long]$fields.ai -eq $expected) 'EXACT_AI_PRICE'
 if(!$Baseline){Need ($fields.ContainsKey('human') -and [long]$fields.human -eq $native) 'NATIVE_HUMAN_PRICE_UNCHANGED'}
}
$done=[regex]::Match($raw,'\[AI_PRICE_DONE\] checks=(8|10) failures=0 profile=(\S+)')
Need $done.Success 'COMPLETE_NATIVE_PRICE_MATRIX'
if($done.Success){Need ($rows.Count -eq [int]$done.Groups[1].Value) 'EXACT_CASE_COUNT'}
foreach($side in @('US','USSR')){
 foreach($role in @(0,1,2,3)){Need (@($rows|Where-Object {$_.faction -eq $side -and $_.role -eq [string]$role}).Count -eq 1) ('CASE_'+$side+'_'+$role)}
}
if($done.Success -and $done.Groups[1].Value -eq '10'){foreach($side in @('US','USSR')){Need (@($rows|Where-Object {$_.faction -eq $side -and $_.role -eq '4'}).Count -eq 1) ('TANK_'+$side)}}
if($RequirePurchase){
 $debit=[regex]::Matches($raw,'\[AI_PRICE_DEBIT\] operation=(\S+) raw=(\d+) quote=(\d+) paid=(\d+) before=(\d+) after=(\d+) human_before=(\d+) human_after=(\d+) passed=1 prefab=(\S+)')
 Need ($debit.Count -eq 1) 'ONE_REAL_PRODUCTION_DEBIT'
 if($debit.Count -eq 1){
  $d=$debit[0];$native=[long]$d.Groups[2].Value;$quote=[long]$d.Groups[3].Value
  Need (@($rows|Where-Object {$_.prefab -eq $d.Groups[9].Value -and [long]$_.raw -eq $native -and [long]$_.ai -eq $quote}).Count -ge 1) 'DEBIT_MATCHES_NATIVE_CATALOG_CASE'
  Need ($quote -eq [math]::Max(1,[math]::Floor(($native+1)/3)) -and $quote -eq [long]$d.Groups[4].Value -and [long]$d.Groups[5].Value-[long]$d.Groups[6].Value -eq $quote) 'ACTUAL_DEBIT_EQUALS_AI_QUOTE'
  Need ($d.Groups[7].Value -eq $d.Groups[8].Value -and [long]$d.Groups[7].Value -eq $native) 'HUMAN_PRICE_UNCHANGED_AFTER_PAYMENT'
  $operation=[regex]::Escape($d.Groups[1].Value);$prefab=[regex]::Escape($d.Groups[9].Value)
  Need ($raw -match ('\[VEHICLE_PRESET_PURCHASE\][^\r\n]*operation='+$operation+' prefab='+$prefab+' supplies='+$quote+'\s')) 'SAME_PRODUCTION_PURCHASE_RECEIPT'
  Need ($raw -match ('\[VEHICLE_SPAWNED\][^\r\n]*lease_generation=[1-9]\d* vehicle_generation=[1-9]\d* vehicle_lifecycle_id=\S+ vehicle=\S+[^\r\n]*prefab='+$prefab+' operation_id='+$operation+'\s')) 'SAME_IDENTITY_BOUND_LEASE_AFTER_PAYMENT'
  Need ($raw -match '\[AI_PRICE_PURCHASE_DONE\] production_debit_observed=1') 'PURCHASE_OBSERVER_FINISHED'
 }
}
$fatal=0;$errors=0
foreach($file in Get-ChildItem (Split-Path -Parent $LogPath) -Filter '*.log' -File){$text=Get-Content $file.FullName -Raw;$errors+=[regex]::Matches($text,'\([EF]\)').Count;$fatal+=[regex]::Matches($text,'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Out of memory|Unhandled exception').Count}
Need ($fatal -eq 0) 'ALL_SIBLING_FATAL_ZERO'
$functional='PASS';if($failures.Count){$functional='FAIL'};$strict='PASS';if($errors){$strict='FAIL'}
"AI_COMBAT_PRICE functional=$functional fatal_lines=$fatal strict_resources=$strict raw_errors=$errors failures=$($failures.Count)"
$failures|ForEach-Object {"FAIL $_"}
if($failures.Count -or $errors){exit 1}
