param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-crew-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work|Out-Null
$argv=@('-gproj','source','-profile','profile','-addonsDir','addons','-addons','CORE,MAP','-aicfVehiclePresetClient','1','-aicfVehicleCrewReturnProbe','1')
$header='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"
$launcher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)+"`n[SERVER_READY]"
$server=$header
$canonical="[CREW_CANONICAL_BIND] group=group1 initial_generation=1 desired=10 actual=10 quota_slot_current=1 managed_lookup_current=1 coordinator_current=1`n"
$server+=$canonical
$hold="[CREW_CONFIRMATION_HOLD] player_intent=1 resize_allowed=1 waypoint=hold1`n"
$server+=$hold
$profile="[CREW_PROFILE_RESTORED] generation=1 actual=10 unit_type=INFANTRY before_coordinator_tick=1`n"
$server+=$profile
$serverRules=@('GROUP10_CREATED','TEN_TRACKED_NONPREFIX','RETURN_ROUTE_START_OUTSIDE_BASE','ONLY_ASSIGNED_MEMBER_RETAINED','ROLE_AND_TRACKING_REMAPPED','NINE_MANAGED_RETURNING','UAZ_SINGLE_EXACT_DRIVER','RETURN_FINISHED_BEFORE_DONE')
foreach($rule in $serverRules){$server+="[CREW_CHECK] rule=$rule passed=1`n"}
$server+="[CREW_INVENTORY] unchanged=1`n[VEHICLE_CREW_REDUCED] slot=S0 kept=1 returning=9 generation=2`n"
for($i=0;$i -lt 9;$i++){$server+="[VEHICLE_CREW_RETURNED] member=entity$i base=home reason=ARRIVED`n"}
$server+="[VEHICLE_PRESET_PURCHASE] prefab=UAZ469_uncovered_CIV_blue.et supplies=50`n[CREW_DONE] checks=8 failures=0`nGame destroyed."
$client=$header
$clientRules=@('NO_FACTION_REJECTED','ALPHA_VEHICLE_FORM','GROUP10_READY','UAZ_SELECTED','COMMANDER_SLIDER_AND_PERSISTENT_HOVER','PRIVATE_VEHICLE_PREVIEW','UNSEATED_TEXT_RESOLVED','BUTTON_HOVER_COLOR','BUTTON_HOVER_RESTORED','RESET_CLEARS_DRAFT_SEATS_PREVIEW','RESET_RESELECT_RESTORES_NATIVE_SEATS','CONFIRMATION_WITHOUT_SIDE_EFFECTS','CANCEL_PRESERVES_SQUAD_AND_DRAFT','CONFIRM_AGAIN','WRONG_CONFIRMATION_REFUSED','CONFIRMED_10_TO_1','CATALOG_SENTINEL_NO_RPC','NO_OBSOLETE_PAGE_SELECT_BUTTONS','NATIVE_MANDATORY_SEATS','AUTO_SEATS_MANDATORY_FIRST','AUTO_SEATS_NO_RPC','SEAT_SCROLL_24_ROWS','SEAT_SCROLL_64_ROWS','SEAT_SCROLL_CONTENT_HEIGHT','SEAT_SCROLL_WHEEL_CLAMPED','SEAT_SCROLL_DESCENDANT_WHEEL','PREVIEW_INPUT_ATTACHED','PREVIEW_CURSOR_ENABLED','PREVIEW_ROTATE_ZOOM_LOCAL','PREVIEW_INPUT_CLEANUP')
foreach($rule in $clientRules){$client+="[VEHICLE_UI_CHECK] rule=$rule passed=1`n"}
$scroll="[VEHICLE_UI_SCROLL_SAMPLE] rows=64 height=3712 content_px=5568 viewport_px=618.667 max_px=4949.333 down=58 bottom=4949.333 top=0 child_down=58 normalized_down=0.012 normalized_bottom=1 normalized_repeat=1 normalized_top=0 synthetic_presentation=1 rpc_sent=0 physical_seat_claim=0`n"
$client+=$scroll+"[VEHICLE_UI_DONE] checks=30 failures=0 jip=0`nGame destroyed."
$timer="`n[VEHICLE_UI_PREVIEW_TIMER] scheduled=1 cancelled=1 before_ms=33 after_ms=-1 hidden_visibility_fixture_override=1 physical_input=0"
$client+=$timer
$checks=0
function Run([string]$name,[string]$serverText,[string]$clientText,[int]$expected,[string]$sibling=''){
 $options=@{}
 foreach($role in @('Server','Client')){
  $directory=Join-Path $work $role
  [void][IO.Directory]::CreateDirectory($directory)
  $body=$serverText
  if($role -eq 'Client'){$body=$clientText}
  [IO.File]::WriteAllText("$directory/console.log",$body)
  [IO.File]::WriteAllText("$directory/script.log",$sibling)
  [IO.File]::WriteAllText("$directory/launcher.txt",$launcher)
  [IO.File]::WriteAllText("$directory/exit.txt",'0')
  $options[$role+'Log']="$directory/console.log"
  $options[$role+'Launcher']="$directory/launcher.txt"
  $options[$role+'Exit']="$directory/exit.txt"
 }
 $output=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehicleCrewReturnLog.ps1') @options
 if($LASTEXITCODE -ne $expected){throw "$name unexpected verdict: $output"}
 $script:checks++
}
try{
 Run complete $server $client 0
 Run missing-canonical ($server.Replace($canonical,'')) $client 1
 Run duplicate-canonical ($server+$canonical) $client 1
 Run foreign-canonical ($server.Replace('quota_slot_current=1','quota_slot_current=0')) $client 1
 Run foreign-managed-group ($server.Replace('managed_lookup_current=1','managed_lookup_current=0')) $client 1
 Run foreign-coordinator ($server.Replace('coordinator_current=1','coordinator_current=0')) $client 1
 Run wrong-initial-generation ($server.Replace('initial_generation=1','initial_generation=2')) $client 1
 Run missing-canonical-entity ($server.Replace('group=group1','group=NONE')) $client 1
 Run missing-confirmation-hold ($server.Replace($hold,'')) $client 1
 Run confirmation-resize-forbidden ($server.Replace('resize_allowed=1','resize_allowed=0')) $client 1
 Run confirmation-no-player-intent ($server.Replace('player_intent=1','player_intent=0')) $client 1
 Run confirmation-no-waypoint ($server.Replace('waypoint=hold1','waypoint=NONE')) $client 1
 Run missing-profile-restore ($server.Replace($profile,'')) $client 1
 Run duplicate-profile-restore ($server+$profile) $client 1
 Run profile-actual-one ($server.Replace('actual=10 unit_type=INFANTRY','actual=1 unit_type=INFANTRY')) $client 1
 Run profile-after-coordinator ($server.Replace('before_coordinator_tick=1','before_coordinator_tick=0')) $client 1
 foreach($rule in $serverRules){Run ('missing-'+$rule) ($server.Replace("rule=$rule passed=1", "rule=$rule passed=0")) $client 1}
 foreach($rule in $clientRules){Run ('missing-'+$rule) $server ($client.Replace("rule=$rule passed=1", "rule=$rule passed=0")) 1}
 Run missing-scroll $server ($client.Replace($scroll,'')) 1
 Run missing-timer $server ($client.Replace($timer,'')) 1
 Run duplicate-timer $server ($client+$timer) 1
 Run timer-not-scheduled $server ($client.Replace('before_ms=33','before_ms=0')) 1
 Run timer-not-cancelled $server ($client.Replace('after_ms=-1','after_ms=33')) 1
 Run wrong-scroll-height $server ($client.Replace('height=3712','height=58')) 1
 Run not-scrolling $server ($client.Replace('down=58 bottom','down=0 bottom')) 1
 Run not-clamped $server ($client.Replace('top=0','top=1')) 1
 Run descendant-not-scrolling $server ($client.Replace('child_down=58','child_down=0')) 1
 Run physical-seat-claim $server ($client.Replace('physical_seat_claim=0','physical_seat_claim=1')) 1
 Run duplicate-scroll $server ($client+$scroll) 1
 Run old-reset-contract $server ($client.Replace('rule=RESET_CLEARS_DRAFT_SEATS_PREVIEW','rule=AUTOMATIC_PRESERVES_UNSAVED_SEATS')) 1
 Run independent-native-endpoint $server ($client.Replace('max_px=4949.333','max_px=3712').Replace('bottom=4949.333','bottom=3712')) 0
 Run endpoint-mismatch $server ($client.Replace('max_px=4949.333','max_px=3712')) 1
 Run normalized-not-bottom $server ($client.Replace('normalized_bottom=1','normalized_bottom=0.9')) 1
 Run normalized-repeat-drift $server ($client.Replace('normalized_repeat=1','normalized_repeat=0.9')) 1
 Run normalized-not-top $server ($client.Replace('normalized_top=0','normalized_top=0.1')) 1
 Run normalized-no-movement $server ($client.Replace('normalized_down=0.012','normalized_down=0')) 1
 Run wrong-kit ($server.Replace('unchanged=1','unchanged=0')) $client 1
 Run duplicate-arrival ($server.Replace('member=entity8','member=entity7')) $client 1
 Run missing-arrival ($server.Replace('[VEHICLE_CREW_RETURNED] member=entity8','[MISSING] member=entity8')) $client 1
 Run duplicate-debit ($server+"`n[VEHICLE_PRESET_PURCHASE] prefab=UAZ469_uncovered_CIV_blue.et supplies=50") $client 1
 Run early-exit ($server.Replace('Game destroyed.','')) $client 1
 Run full-log-fatal $server $client 1 'SCRIPT (E): native leak'
 Run missing-completion ($server.Replace('[CREW_DONE]','[MISSING_DONE]')) $client 1
 Run missing-reduction ($server.Replace('kept=1 returning=9','kept=1 returning=8')) $client 1
}
finally{
 $resolved=[IO.Path]::GetFullPath($work)
 if(!$resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'aicf-crew-log-*'){throw 'Unsafe fixture cleanup'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS Vehicle crew log contracts checks=$checks"
