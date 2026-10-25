param(
 [Parameter(Mandatory)][string]$ServerLog,[Parameter(Mandatory)][string]$ClientLog,
 [Parameter(Mandatory)][string]$ServerLauncher,[Parameter(Mandatory)][string]$ClientLauncher,
 [Parameter(Mandatory)][string]$ServerExit,[Parameter(Mandatory)][string]$ClientExit
)
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
function Native([string]$label,[string]$path,[string]$launch,[string]$exitPath){
 $log=[IO.File]::ReadAllText($path)
 $all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $path) -Filter '*.log' -File)|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
 if((Get-Content -LiteralPath $exitPath -Raw).Trim() -ne '0'){$failures.Add($label+'_EXIT')}
 if(!$log.Contains('Game destroyed.')){$failures.Add($label+'_STOPPED')}
 if($all -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation'){$failures.Add($label+'_FULL_FATAL')}
 $launcher=[IO.File]::ReadAllText($launch)
 $manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
 if($manifests.Count -ne 1){$failures.Add($label+'_MANIFEST')}
 else{
  $argv=@(($manifests[0].Groups[1].Value|ConvertFrom-Json).arguments)
  foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfVehiclePresetClient','-aicfVehicleCrewReturnProbe')){
   $index=[array]::IndexOf($argv,$flag)
   if($index -lt 0 -or $index+1 -ge $argv.Count -or !$log.Contains($flag+' '+$argv[$index+1])){$failures.Add($label+'_CLI')}
  }
 }
 if($label -eq 'CLIENT' -and !$launcher.Contains('[SERVER_READY]')){$failures.Add('READINESS')}
 return $log
}
$server=Native SERVER $ServerLog $ServerLauncher $ServerExit
$client=Native CLIENT $ClientLog $ClientLauncher $ClientExit
foreach($rule in @('GROUP10_CREATED','TEN_TRACKED_NONPREFIX','RETURN_ROUTE_START_OUTSIDE_BASE','ONLY_ASSIGNED_MEMBER_RETAINED','ROLE_AND_TRACKING_REMAPPED','NINE_MANAGED_RETURNING','UAZ_SINGLE_EXACT_DRIVER','RETURN_FINISHED_BEFORE_DONE')){
 if(@([regex]::Matches($server,'\[CREW_CHECK\] rule='+$rule+' passed=1')).Count -ne 1){$failures.Add('SERVER_'+$rule)}
}
foreach($rule in @('NO_FACTION_REJECTED','ALPHA_VEHICLE_FORM','GROUP10_READY','UAZ_SELECTED','COMMANDER_SLIDER_AND_PERSISTENT_HOVER','PRIVATE_VEHICLE_PREVIEW','UNSEATED_TEXT_RESOLVED','BUTTON_HOVER_COLOR','BUTTON_HOVER_RESTORED','AUTOMATIC_PRESERVES_UNSAVED_SEATS','CONFIRMATION_WITHOUT_SIDE_EFFECTS','CANCEL_PRESERVES_SQUAD_AND_DRAFT','CONFIRM_AGAIN','WRONG_CONFIRMATION_REFUSED','CONFIRMED_10_TO_1')){
 if(@([regex]::Matches($client,'\[VEHICLE_UI_CHECK\] rule='+$rule+' passed=1')).Count -ne 1){$failures.Add('CLIENT_'+$rule)}
}
if(($server+"`n"+$client) -match '\[(CREW_CHECK|VEHICLE_UI_CHECK)\].*passed=0|VEHICLE_UI_TIMEOUT'){$failures.Add('FAILED_CASE')}
if(!$server.Contains('[ROSTER_READY]') -or !$server.Contains('[CREW_DONE] checks=8 failures=0') -or !$client.Contains('[VEHICLE_UI_DONE] checks=15 failures=0 jip=0')){$failures.Add('COMPLETION')}
if(!$server.Contains('[CREW_INVENTORY] unchanged=1')){$failures.Add('KIT_CHANGED')}
if(@([regex]::Matches($server,'\[VEHICLE_CREW_REDUCED\].*kept=1 returning=9 generation=2')).Count -ne 1){$failures.Add('REDUCTION')}
$returned=@([regex]::Matches($server,'\[VEHICLE_CREW_RETURNED\].*member=(\S+).*reason=ARRIVED'))
if($returned.Count -ne 9 -or @($returned|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -ne 9){$failures.Add('NINE_DISTINCT_ARRIVALS')}
$purchases=@([regex]::Matches($server,'\[VEHICLE_PRESET_PURCHASE\].*UAZ469_uncovered_CIV_blue.et supplies=50'))
if($purchases.Count -ne 1){$failures.Add('SINGLE_PURCHASE')}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
'PASS Vehicle crew return: UI15 server8 inventory unchanged, 10->1, nine distinct arrivals, exact driver, single purchase, native0/0 stopped2'
exit 0
