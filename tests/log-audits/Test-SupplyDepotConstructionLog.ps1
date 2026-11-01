[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$LogPath,
 [Parameter(Mandatory=$true)][string]$NativeExitPath,
 [Parameter(Mandatory=$true)][string]$LauncherPath)
$ErrorActionPreference='Stop'
$raw=Get-Content -LiteralPath $LogPath -Raw
$failures=[System.Collections.Generic.List[string]]::new()
if(!(Test-Path -LiteralPath $NativeExitPath) -or (Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0'){$failures.Add('NATIVE_EXIT_ZERO')}
if($raw -notmatch 'Game destroyed\.'){$failures.Add('FULL_STOPPED_LOG')}
$launcher=Get-Content -LiteralPath $LauncherPath -Raw
$manifest=[regex]::Match($launcher,'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)')
if(!$manifest.Success){$failures.Add('CANONICAL_MANIFEST')}
else{
 $args=@(($manifest.Groups[1].Value|ConvertFrom-Json).arguments)
 $index=[array]::IndexOf($args,'-aicfSupplyDepotConstructionProbe')
 if($index -lt 0 -or $args[$index+1] -ne '1'){$failures.Add('PROBE_CLI')}
}
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Test-ConstructionLog.ps1') -LogPath $LogPath -ExpectedMode US -RequireCompletion
if($LASTEXITCODE -ne 0){$failures.Add('NORMAL_CONSTRUCTION_GATE')}
$siblings=@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -Filter '*.log' -File)
$errors=0
foreach($sibling in $siblings){
 $body=Get-Content -LiteralPath $sibling.FullName -Raw
 $errors+=[regex]::Matches($body,'\([EF]\)').Count
 if($body -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|Out of memory|Access violation'){$failures.Add('NATIVE_FATAL:'+ $sibling.Name)}
}
$events=@{}
foreach($line in ($raw -split "`n")){
 if($line -notmatch '\[SUPPLY_CONSTRUCTION_PROBE\] event=(\w+)') {continue}
 $event=$Matches[1]; $fields=@{}
 foreach($match in [regex]::Matches($line,'(\w+)=(\S+)')){$fields[$match.Groups[1].Value]=$match.Groups[2].Value}
 if(!$events.ContainsKey($event)){$events[$event]=@()}
 $events[$event]+=$fields
}
foreach($required in @('METADATA','START','PLACED','DONE')){if(!$events.ContainsKey($required)){$failures.Add('MISSING_'+$required)}}
if($events.ContainsKey('METADATA') -and @($events.METADATA|Where-Object {$_.valid -eq '1' -and $_.faction -eq 'US'}).Count -eq 0){$failures.Add('METADATA_INVALID')}
if($events.ContainsKey('START')){
 if($events.START.Count -ne 1){$failures.Add('START_COUNT')}
 $start=$events.START[0]
 if($start.human_veto -ne '1' -or $start.priority_controlled -ne '1' -or $start.economy_seed -ne '0' -or $start.capacity_write -ne '0' -or [int]$start.quote -le [int]$start.capacity){$failures.Add('INVALID_PRECONDITIONS')}
}
if($events.ContainsKey('PLACED')){
 foreach($placed in $events.PLACED){
  if($placed.accepted -ne '1' -or $placed.paid -ne '1' -or [int]$placed.cost -le 0 -or [math]::Abs([double]$placed.before-[double]$placed.after-[double]$placed.cost) -gt 0.01){$failures.Add('PAID_PLACEMENT')}
  $token=[regex]::Escape($placed.token)
  $placement=[regex]::Match($raw,'\[CONSTRUCTION_PLACED\][^\r\n]*token='+$token+' faction=US [^\r\n]*type=SUPPLY_DEPOT [^\r\n]*layout=(\S+)')
  $payment=[regex]::Match($raw,'\[CONSTRUCTION_PAYMENT\][^\r\n]*token='+$token+' faction=US [^\r\n]*type=SUPPLY_DEPOT [^\r\n]*cost='+[regex]::Escape($placed.cost)+' ')
  if(!$placement.Success -or !$payment.Success){$failures.Add('EXACT_PAID_SUPPLY_TOKEN')}
  else{
   $layout=[regex]::Escape($placement.Groups[1].Value)
   $completion=[regex]::Match($raw,'\[CONSTRUCTION_COMPLETED\][^\r\n]*token='+$token+' faction=US [^\r\n]*type=SUPPLY_DEPOT [^\r\n]*layout='+$layout+' [^\r\n]*service_online=1')
   if(!$completion.Success -or $payment.Index -gt $placement.Index -or $placement.Index -gt $completion.Index){$failures.Add('EXACT_SUPPLY_COMPLETION_CAUSALITY')}
  }
 }
}
if($events.ContainsKey('DONE')){
 if($events.DONE.Count -ne 1){$failures.Add('DONE_COUNT')}
 $done=$events.DONE[0]
 if($done.passed -ne '1' -or $done.online -ne '1' -or $done.human_veto -ne '1' -or [int]$done.capacity -le [int]$done.initial_capacity -or [int]$done.capacity -lt [int]$done.quote){$failures.Add('CAPACITY_NOT_PROVEN')}
}
$functional='PASS';if($failures.Count){$functional='FAIL'}
$strict='PASS';if($errors){$strict='FAIL'}
Write-Output "SUPPLY_DEPOT_CONSTRUCTION functional=$functional strict_resources=$strict failures=$($failures.Count) raw_sibling_error_lines=$errors"
if($failures.Count){Write-Output ('FAIL '+($failures -join ','))}
if($failures.Count -or $errors){exit 1}
