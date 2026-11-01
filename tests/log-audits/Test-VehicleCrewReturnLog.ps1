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
$modernRules=@('CATALOG_SENTINEL_NO_RPC','NO_OBSOLETE_PAGE_SELECT_BUTTONS','NATIVE_MANDATORY_SEATS','AUTO_SEATS_MANDATORY_FIRST','AUTO_SEATS_NO_RPC','SEAT_SCROLL_24_ROWS','SEAT_SCROLL_64_ROWS','SEAT_SCROLL_CONTENT_HEIGHT','SEAT_SCROLL_WHEEL_CLAMPED','SEAT_SCROLL_DESCENDANT_WHEEL','PREVIEW_INPUT_ATTACHED','PREVIEW_CURSOR_ENABLED','PREVIEW_ROTATE_ZOOM_LOCAL','PREVIEW_INPUT_CLEANUP')
foreach($rule in (@('NO_FACTION_REJECTED','ALPHA_VEHICLE_FORM','GROUP10_READY','UAZ_SELECTED','COMMANDER_SLIDER_AND_PERSISTENT_HOVER','PRIVATE_VEHICLE_PREVIEW','UNSEATED_TEXT_RESOLVED','BUTTON_HOVER_COLOR','BUTTON_HOVER_RESTORED','RESET_CLEARS_DRAFT_SEATS_PREVIEW','RESET_RESELECT_RESTORES_NATIVE_SEATS','CONFIRMATION_WITHOUT_SIDE_EFFECTS','CANCEL_PRESERVES_SQUAD_AND_DRAFT','CONFIRM_AGAIN','WRONG_CONFIRMATION_REFUSED','CONFIRMED_10_TO_1')+$modernRules)){
 if(@([regex]::Matches($client,'\[VEHICLE_UI_CHECK\] rule='+$rule+' passed=1')).Count -ne 1){$failures.Add('CLIENT_'+$rule)}
}
$scroll=@([regex]::Matches($client,'\[VEHICLE_UI_SCROLL_SAMPLE\] rows=64 height=([0-9.]+) content_px=([0-9.]+) viewport_px=([0-9.]+) max_px=([0-9.]+) down=([0-9.]+) bottom=([0-9.]+) top=([0-9.]+) child_down=([0-9.]+) normalized_down=([0-9.]+) normalized_bottom=([0-9.]+) normalized_repeat=([0-9.]+) normalized_top=([0-9.]+) synthetic_presentation=1 rpc_sent=0 physical_seat_claim=0'))
if($scroll.Count -ne 1 -or @([regex]::Matches($client,'\[VEHICLE_UI_SCROLL_SAMPLE\]')).Count -ne 1){$failures.Add('CLIENT_SCROLL_NATIVE_READBACK')}
else{
  $values=@(1..12|ForEach-Object{[double]::Parse($scroll[0].Groups[$_].Value,[Globalization.CultureInfo]::InvariantCulture)})
 if($values[0] -ne 3712 -or $values[3] -le 0 -or $values[4] -le 0 -or $values[4] -gt $values[3] -or [Math]::Abs($values[5]-$values[3]) -gt 0.01 -or [Math]::Abs($values[6]) -gt 0.01 -or $values[7] -le 0 -or $values[7] -gt $values[3] -or $values[8] -le 0 -or $values[8] -gt 1 -or $values[9] -ne 1 -or $values[10] -ne 1 -or $values[11] -ne 0){$failures.Add('CLIENT_SCROLL_NATIVE_READBACK')}
}
if(($server+"`n"+$client) -match '\[(CREW_CHECK|VEHICLE_UI_CHECK)\].*passed=0|VEHICLE_UI_TIMEOUT'){$failures.Add('FAILED_CASE')}
if(!$server.Contains('[ROSTER_READY]') -or !$server.Contains('[CREW_DONE] checks=8 failures=0') -or !$client.Contains('[VEHICLE_UI_DONE] checks=30 failures=0 jip=0')){$failures.Add('COMPLETION')}
if(!$server.Contains('[CREW_INVENTORY] unchanged=1')){$failures.Add('KIT_CHANGED')}
$canonical=@([regex]::Matches($server,'\[CREW_CANONICAL_BIND\] group=(\S+) initial_generation=1 desired=10 actual=10 quota_slot_current=1 managed_lookup_current=1 coordinator_current=1'))
if($canonical.Count -ne 1 -or $canonical[0].Groups[1].Value -eq 'NONE' -or @([regex]::Matches($server,'\[CREW_CANONICAL_BIND\]')).Count -ne 1){$failures.Add('CANONICAL_OWNER')}
if(@([regex]::Matches($server,'\[CREW_CONFIRMATION_HOLD\] player_intent=1 resize_allowed=1 waypoint=(?!NONE)(\S+)')).Count -ne 1){$failures.Add('CONFIRMATION_HOLD')}
if(@([regex]::Matches($server,'\[CREW_PROFILE_RESTORED\] generation=1 actual=10 unit_type=INFANTRY before_coordinator_tick=1')).Count -ne 1 -or @([regex]::Matches($server,'\[CREW_PROFILE_RESTORED\]')).Count -ne 1){$failures.Add('READY_PROFILE_RESTORE')}
# Initial deployment increments generation0->1; actual CommitVehicleCrew increments1->2.
# Ни ручной MarkReady, ни alternate state/coordinator не являются этим доказательством.
if(@([regex]::Matches($server,'\[VEHICLE_CREW_REDUCED\].*kept=1 returning=9 generation=2')).Count -ne 1){$failures.Add('REDUCTION')}
$returned=@([regex]::Matches($server,'\[VEHICLE_CREW_RETURNED\].*member=(\S+).*reason=ARRIVED'))
if($returned.Count -ne 9 -or @($returned|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique).Count -ne 9){$failures.Add('NINE_DISTINCT_ARRIVALS')}
$purchases=@([regex]::Matches($server,'\[VEHICLE_PRESET_PURCHASE\].*UAZ469_uncovered_CIV_blue.et supplies=50'))
if($purchases.Count -ne 1){$failures.Add('SINGLE_PURCHASE')}
$timer=@([regex]::Matches($client,'\[VEHICLE_UI_PREVIEW_TIMER\] scheduled=1 cancelled=1 before_ms=(\d+) after_ms=(-?\d+) hidden_visibility_fixture_override=1 physical_input=0'))
if($timer.Count -ne 1 -or @([regex]::Matches($client,'\[VEHICLE_UI_PREVIEW_TIMER\]')).Count -ne 1){$failures.Add('CLIENT_PREVIEW_TIMER_NATIVE_READBACK')}
elseif([int]$timer[0].Groups[1].Value -le 0 -or [int]$timer[0].Groups[2].Value -gt 0){$failures.Add('CLIENT_PREVIEW_TIMER_NATIVE_READBACK')}
if($failures.Count){$failures|Sort-Object -Unique;exit 1}
'PASS Vehicle crew return: UI30 server8 inventory unchanged, 10->1, nine distinct arrivals, exact driver, single purchase, native0/0 stopped2'
exit 0
