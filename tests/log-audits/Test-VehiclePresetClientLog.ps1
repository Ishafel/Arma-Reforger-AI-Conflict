param([Parameter(Mandatory)][string]$ServerLog,[Parameter(Mandatory)][string]$ServerLauncher,[Parameter(Mandatory)][string]$ServerExit,
 [Parameter(Mandatory)][string]$ClientLog,[Parameter(Mandatory)][string]$ClientLauncher,[Parameter(Mandatory)][string]$ClientExit,
 [Parameter(Mandatory)][string]$JIPLog,[Parameter(Mandatory)][string]$JIPLauncher,[Parameter(Mandatory)][string]$JIPExit,[switch]$CatalogFilters,[switch]$BehaviorSettings)
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
function Read-Native([string]$Label,[string]$LogPath,[string]$LauncherPath,[string]$ExitPath,[bool]$Client){
 $log=[IO.File]::ReadAllText($LogPath)
 $all=(@(Get-ChildItem -LiteralPath (Split-Path -Parent $LogPath) -Filter '*.log' -File)|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
 $launcher=[IO.File]::ReadAllText($LauncherPath)
 if((Get-Content -LiteralPath $ExitPath -Raw).Trim() -ne '0'){$failures.Add($Label+'_NATIVE_EXIT')}
 if(!$log.Contains('Game destroyed.')){$failures.Add($Label+'_STOPPED')}
 if($all -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|NULL pointer|Virtual Machine Exception|Assertion failed|Attempt to modify locked entity|Access violation'){$failures.Add($Label+'_FULL_FATAL')}
 $manifests=@([regex]::Matches($launcher,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
 if($manifests.Count -ne 1){$failures.Add($Label+'_MANIFEST')}
 else{
  $manifest=$manifests[0].Groups[1].Value|ConvertFrom-Json
  $argv=@($manifest.arguments)
  $flags=@('-gproj','-profile','-addonsDir','-addons','-aicfVehiclePresetClient')
  if($CatalogFilters){$flags+='-aicfVehicleCatalogFilters'}
  if($BehaviorSettings){$flags+='-aicfVehicleBehaviorReadback'}
  foreach($flag in $flags){
   $index=[array]::IndexOf($argv,$flag)
   if($index -lt 0 -or $index+1 -ge $argv.Count -or !$log.Contains($flag+' '+$argv[$index+1])){$failures.Add($Label+'_CLI')}
  }
 }
 if($Client -and !$launcher.Contains('[SERVER_READY]')){$failures.Add($Label+'_READINESS')}
 return $log
}
$server=Read-Native 'SERVER' $ServerLog $ServerLauncher $ServerExit $false
$client=Read-Native 'CLIENT' $ClientLog $ClientLauncher $ClientExit $true
$jip=Read-Native 'JIP' $JIPLog $JIPLauncher $JIPExit $true
if(!$server.Contains('[ROSTER_READY]')){$failures.Add('SERVER_ROSTER')}
foreach($case in @('NO_FACTION_REJECTED','ALPHA_VEHICLE_FORM','CONTEXT','UAZ_WHITELIST','SELECT_HANDLER','NATIVE_SEAT_LABELS','DRIVER_SEAT','ASSIGN_HANDLER','SAVE_HANDLER','APPLY_HANDLER','REPEAT_APPLY_IDEMPOTENT','STALE_REVISION_REJECTED','FOREIGN_FACTION_REJECTED','LOAD_HANDLER','DELETE_HANDLER')){
 $found=@([regex]::Matches($client,'\[VEHICLE_UI_CHECK\] rule='+$case+' passed=1'))
 if($found.Count -ne 1){$failures.Add('CLIENT_CASE_'+$case)}
}
$modernCases=@('CATALOG_SENTINEL_NO_RPC','NO_OBSOLETE_PAGE_SELECT_BUTTONS','NATIVE_MANDATORY_SEATS','AUTO_SEATS_MANDATORY_FIRST','AUTO_SEATS_NO_RPC','SEAT_SCROLL_24_ROWS','SEAT_SCROLL_64_ROWS','SEAT_SCROLL_CONTENT_HEIGHT','SEAT_SCROLL_WHEEL_CLAMPED','SEAT_SCROLL_DESCENDANT_WHEEL','PREVIEW_INPUT_ATTACHED','PREVIEW_CURSOR_ENABLED','PREVIEW_ROTATE_ZOOM_LOCAL','PREVIEW_INPUT_CLEANUP')
foreach($case in $modernCases){
 if(@([regex]::Matches($client,'\[VEHICLE_UI_CHECK\] rule='+$case+' passed=1')).Count -ne 1){$failures.Add('CLIENT_CASE_'+$case)}
}
$scroll=@([regex]::Matches($client,'\[VEHICLE_UI_SCROLL_SAMPLE\] rows=64 height=([0-9.]+) content_px=([0-9.]+) viewport_px=([0-9.]+) max_px=([0-9.]+) down=([0-9.]+) bottom=([0-9.]+) top=([0-9.]+) child_down=([0-9.]+) normalized_down=([0-9.]+) normalized_bottom=([0-9.]+) normalized_repeat=([0-9.]+) normalized_top=([0-9.]+) synthetic_presentation=1 rpc_sent=0 physical_seat_claim=0'))
if($scroll.Count -ne 1 -or @([regex]::Matches($client,'\[VEHICLE_UI_SCROLL_SAMPLE\]')).Count -ne 1){$failures.Add('CLIENT_SCROLL_NATIVE_READBACK')}
else{
  $values=@(1..12|ForEach-Object{[double]::Parse($scroll[0].Groups[$_].Value,[Globalization.CultureInfo]::InvariantCulture)})
 if($values[0] -ne 3712 -or $values[3] -le 0 -or $values[4] -le 0 -or $values[4] -gt $values[3] -or [Math]::Abs($values[5]-$values[3]) -gt 0.01 -or [Math]::Abs($values[6]) -gt 0.01 -or $values[7] -le 0 -or $values[7] -gt $values[3] -or $values[8] -le 0 -or $values[8] -gt 1 -or $values[9] -ne 1 -or $values[10] -ne 1 -or $values[11] -ne 0){$failures.Add('CLIENT_SCROLL_NATIVE_READBACK')}
}
foreach($case in @('ALPHA_VEHICLE_FORM','JIP_CONFIRMED_ASSIGNMENT','JIP_STABLE_SLOT')){
 if(@([regex]::Matches($jip,'\[VEHICLE_UI_CHECK\] rule='+$case+' passed=1')).Count -ne 1){$failures.Add('JIP_CASE_'+$case)}
}
if(($client+"`n"+$jip) -match '\[VEHICLE_UI_CHECK\].*passed=0|VEHICLE_UI_TIMEOUT'){$failures.Add('FAILED_CASE')}
$clientChecks=20+$modernCases.Count
$jipChecks=3
if($CatalogFilters){
 $clientChecks=28+$modernCases.Count
 $jipChecks=5
 foreach($case in @('ALL_NATIVE_CATALOG','CIVILIAN_FILTER','CAR_CATEGORY_FILTER','FACTION_FILTER','EMPTY_FILTER_SAFE','EMPTY_CATALOG_CHANGE_NO_RPC','FILTERED_PREFAB_MAPPING','LOAD_REVEALS_DRAFT')){
  if(@([regex]::Matches($client,'\[VEHICLE_UI_CHECK\] rule='+$case+' passed=1')).Count -ne 1){$failures.Add('FILTER_CASE_'+$case)}
 }
 foreach($case in @('JIP_CIVILIAN_AVAILABLE','JIP_FILTERED_SELECTION')){
  if(@([regex]::Matches($jip,'\[VEHICLE_UI_CHECK\] rule='+$case+' passed=1')).Count -ne 1){$failures.Add('FILTER_CASE_'+$case)}
 }
 foreach($launch in @($ServerLauncher,$ClientLauncher,$JIPLauncher)){if(![IO.File]::ReadAllText($launch).Contains('"-aicfVehicleCatalogFilters","1"')){$failures.Add('FILTER_CLI')}}
}
if($BehaviorSettings){
	$clientChecks+=6
	$jipChecks+=3
 foreach($case in @('BEHAVIOR_INVALID_DISTANCE_NO_RPC','BEHAVIOR_UI_CONTROLS','BEHAVIOR_SAVE_READBACK','BEHAVIOR_APPLY_READBACK','BEHAVIOR_LIBRARY_LOAD_READBACK','GC_NO_AUTHORITY_REJECTED')){
  if(@([regex]::Matches($client,'\[VEHICLE_UI_CHECK\] rule='+$case+' passed=1')).Count -ne 1){$failures.Add('BEHAVIOR_CASE_'+$case)}
 }
 foreach($case in @('JIP_BEHAVIOR_CONFIRMED','JIP_BEHAVIOR_UI_READBACK','GC_NO_AUTHORITY_REJECTED')){
  if(@([regex]::Matches($jip,'\[VEHICLE_UI_CHECK\] rule='+$case+' passed=1')).Count -ne 1){$failures.Add('BEHAVIOR_CASE_'+$case)}
 }
 foreach($entry in @(@('CLIENT',$client,'0'),@('JIP',$jip,'1'))){
  $pattern='\[VEHICLE_UI_GC_AUTHORITY\] jip='+$entry[2]+' actual_server=0 prepared=1 register_denied=1 protect_denied=1 passed=1 entity=(?!0\s|NONE)\S+ rpl=((?!NONE|-1\s)\S+) native_master=0 native_proxy=1 native_role=\d+'
  $authority=@([regex]::Matches($entry[1],$pattern))
  if(@([regex]::Matches($entry[1],'\[VEHICLE_UI_GC_AUTHORITY\]')).Count -ne 1 -or $authority.Count -ne 1){$failures.Add($entry[0]+'_GC_AUTHORITY_READBACK')}
  if($authority.Count -eq 1){
   $nativeRpl=[regex]::Escape($authority[0].Groups[1].Value)
   $nativeEntity='(?:0x[0-9a-fA-F]*[1-9a-fA-F][0-9a-fA-F]*|[1-9]\d*)(?: \{\})?'
   if($server -notmatch ('\[VEHICLE_UI_GC_SERVER_REPLY\] player=\d+ reason=SERVER_SPAWNED failed=0 entity='+$nativeEntity+' rpl='+$nativeRpl+'(?:\s|$)')){$failures.Add($entry[0]+'_GC_SERVER_NATIVE_ORIGIN')}
   if(@([regex]::Matches($server,'\[VEHICLE_UI_GC_SERVER_CLEANUP\] player=\d+ rpl='+$nativeRpl+' exact_empty_delete_requested=1')).Count -ne 1){$failures.Add($entry[0]+'_GC_SERVER_EXACT_CLEANUP')}
  }
 }
 $nativeSetups=@([regex]::Matches($server,'\[VEHICLE_UI_GC_SERVER_SETUP\] player=\d+ avatar_present=1 world_present=1 resource_valid=1 safe_spawn=1 vehicle_present=1 distance=([0-9.]+)'))
 if($nativeSetups.Count -ne 2 -or @([regex]::Matches($server,'\[VEHICLE_UI_GC_SERVER_SETUP\]')).Count -ne 2){$failures.Add('SERVER_GC_SAFE_SETUP')}
 foreach($setup in $nativeSetups){$distance=[double]::Parse($setup.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture);if($distance -lt 10 -or $distance -gt 20){$failures.Add('SERVER_GC_SAFE_SETUP')}}
 foreach($launch in @($ServerLauncher,$ClientLauncher,$JIPLauncher)){if(![IO.File]::ReadAllText($launch).Contains('"-aicfVehicleBehaviorReadback","1"')){$failures.Add('BEHAVIOR_CLI')}}
 $behaviorReadbacks=@([regex]::Matches($server,'\[VEHICLE_UI_SERVER_BEHAVIOR_READBACK\] player=(\d+) valid=1 behavior=1 return_distance=437 schema=2 revision=(\d+) jip=([01])'))
 if($behaviorReadbacks.Count -ne 2 -or @([regex]::Matches($server,'\[VEHICLE_UI_SERVER_BEHAVIOR_READBACK\]')).Count -ne 2){$failures.Add('SERVER_BEHAVIOR_READBACK_COUNT')}
 foreach($late in @('0','1')){
  $current=@($behaviorReadbacks|Where-Object{$_.Groups[3].Value -eq $late -and [int]$_.Groups[2].Value -gt 0})
  if($current.Count -ne 1){$failures.Add('SERVER_BEHAVIOR_READBACK_'+$late)}
 }
 if($behaviorReadbacks.Count -eq 2 -and $behaviorReadbacks[0].Groups[2].Value -ne $behaviorReadbacks[1].Groups[2].Value){$failures.Add('SERVER_BEHAVIOR_REVISION_CONTINUITY')}
}
if(@([regex]::Matches($client,'\[VEHICLE_UI_DONE\] checks='+$clientChecks+' failures=0 jip=0')).Count -ne 1){$failures.Add('CLIENT_COMPLETION')}
if(@([regex]::Matches($jip,'\[VEHICLE_UI_DONE\] checks='+$jipChecks+' failures=0 jip=1')).Count -ne 1){$failures.Add('JIP_COMPLETION')}
foreach($late in @('0','1')){if(@([regex]::Matches($server,'\[VEHICLE_UI_SERVER_READBACK\] player=\d+ failures=0 jip='+$late)).Count -ne 1){$failures.Add('SERVER_READBACK_'+$late)}}
$timer=@([regex]::Matches($client,'\[VEHICLE_UI_PREVIEW_TIMER\] scheduled=1 cancelled=1 before_ms=(\d+) after_ms=(-?\d+) hidden_visibility_fixture_override=1 physical_input=0'))
if($timer.Count -ne 1 -or @([regex]::Matches($client,'\[VEHICLE_UI_PREVIEW_TIMER\]')).Count -ne 1){$failures.Add('CLIENT_PREVIEW_TIMER_NATIVE_READBACK')}
elseif([int]$timer[0].Groups[1].Value -le 0 -or [int]$timer[0].Groups[2].Value -gt 0){$failures.Add('CLIENT_PREVIEW_TIMER_NATIVE_READBACK')}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
"PASS Vehicle preset UI/RPC client=$clientChecks JIP=$jipChecks server_readback=2 native=0/0/0 stopped=3"
exit 0
