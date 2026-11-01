param(
 [Parameter(Mandatory)][string]$ServerLog,[Parameter(Mandatory)][string]$ClientLog,
 [Parameter(Mandatory)][string]$ServerLauncher,[Parameter(Mandatory)][string]$ClientLauncher,
 [Parameter(Mandatory)][string]$ServerExit,[Parameter(Mandatory)][string]$ClientExit
)
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
$errors=[ordered]@{}
function Native([string]$label,[string]$path,[string]$launch,[string]$exitPath){
 $log=[IO.File]::ReadAllText($path)
 $all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $path) -Filter '*.log' -File)|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
 if((Get-Content -LiteralPath $exitPath -Raw).Trim() -ne '0'){$failures.Add($label+'_EXIT')}
 if(!$log.Contains('Game destroyed.')){$failures.Add($label+'_STOPPED')}
 if($all -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|MEMORY\s+\([EF]\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation|Out of memory'){$failures.Add($label+'_FULL_FATAL')}
 $errors[$label]=@([regex]::Matches($all,'(?m)^.*\(E\).*$')).Count
 $launcher=[IO.File]::ReadAllText($launch)
 $manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
 if($manifests.Count -ne 1){$failures.Add($label+'_MANIFEST')}
 else{
  $argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
  foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfDeferredVehiclePresetProbe')){
   $index=[array]::IndexOf($argv,$flag)
   if($index -lt 0 -or $index+1 -ge $argv.Count -or !$log.Contains($flag+' '+$argv[$index+1])){$failures.Add($label+'_CLI')}
  }
 }
 if($label -eq 'CLIENT' -and !$launcher.Contains('[SERVER_READY]')){$failures.Add('READINESS')}
 return $log
}
$server=Native SERVER $ServerLog $ServerLauncher $ServerExit
$client=Native CLIENT $ClientLog $ClientLauncher $ClientExit
foreach($rule in @('BOUND_BEFORE_MUSTER_COMPLETE','NO_VEHICLE_BEFORE_MUSTER','DEFERRED_CREW_REDUCED','POST_MUSTER_POINT_ORDER','PURCHASED_EXACT_DRIVER')){
 if(@([regex]::Matches($server,'\[DEFERRED_CHECK\] rule='+$rule+' passed=1')).Count -ne 1){$failures.Add('SERVER_'+$rule)}
}
foreach($rule in @('PARTIAL_ROSTER_FUTURE_POSITIONS','CATALOG_NATIVE_UAZ','SELECT_NATIVE_TOPOLOGY','CONFIRM_BEFORE_RECRUITMENT','BOUND_WHILE_RECRUITING','DEFERRED_CREW_OWNER_READBACK')){
 if(@([regex]::Matches($client,'\[DEFERRED_CLIENT_CHECK\] rule='+$rule+' passed=1')).Count -ne 1){$failures.Add('CLIENT_'+$rule)}
}
if(($server+"`n"+$client) -match '\[DEFERRED_(CLIENT_)?CHECK\].*passed=0'){$failures.Add('FAILED_CASE')}
if(!$server.Contains('[ROSTER_READY]') -or !$server.Contains('[DEFERRED_DONE] checks=5 failures=0') -or !$client.Contains('[DEFERRED_CLIENT_DONE] checks=6 failures=0')){$failures.Add('COMPLETION')}
if(@([regex]::Matches($server,'\[DEFERRED_CREW_COMMIT\].*retained=1 kit_unchanged=1 generation=2')).Count -ne 1){$failures.Add('KIT_OR_GENERATION')}
$joined=@([regex]::Matches($server,'\[INFANTRY_RECRUIT_JOINED\][^\r\n]*faction=USSR slot=0[^\r\n]*character=(\S+)'))
if($joined.Count -ne 9 -or @($joined|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -ne 9){$failures.Add('NINE_PAID_DISTINCT_RECRUITS')}
$purchases=@([regex]::Matches($server,'\[VEHICLE_PRESET_PURCHASE\].*UAZ469_uncovered_CIV_blue.et supplies=50'))
if($purchases.Count -ne 1){$failures.Add('SINGLE_PAID_PURCHASE')}
$confirm=$server.IndexOf('accepted=1 reason=WAITING_FOR_REINFORCEMENTS')
$firstJoin=$server.IndexOf('[INFANTRY_RECRUIT_JOINED]')
if($confirm -lt 0 -or ($joined.Count -gt 0 -and $confirm -gt $joined[0].Index) -or ($purchases.Count -gt 0 -and $confirm -gt $purchases[0].Index)){$failures.Add('CAUSAL_BIND_BEFORE_RECRUIT_AND_PURCHASE')}
Write-Output ('FULL_RAW_ERROR_COUNTS='+($errors|ConvertTo-Json -Compress))
# Functional gate does not relabel native resource baseline as clean logs.
if(($errors.Values|Measure-Object -Sum).Sum -gt 0){Write-Output 'STRICT_FULL_ERROR_GATE=FAIL'}else{Write-Output 'STRICT_FULL_ERROR_GATE=PASS'}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
'PASS Deferred vehicle preset: real owner RPC while partial, nine paid recruits, identity-safe crew, single purchase/exact driver, native0/0 stopped2'
exit 0
