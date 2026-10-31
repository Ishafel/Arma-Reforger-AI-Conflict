param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-vehicle-ui-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work|Out-Null
$argv=@('-gproj','C:\source\addon.gproj','-profile','C:\profile','-addonsDir','C:\source','-addons','CORE,MAP','-aicfVehiclePresetClient','1')
$header='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"
$launcher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)+"`n[SERVER_READY]"
$names=@('NO_FACTION_REJECTED','ALPHA_VEHICLE_FORM','CONTEXT','UAZ_WHITELIST','SELECT_HANDLER','NATIVE_SEAT_LABELS','DRIVER_SEAT','ASSIGN_HANDLER','SAVE_HANDLER','APPLY_HANDLER','REPEAT_APPLY_IDEMPOTENT','STALE_REVISION_REJECTED','FOREIGN_FACTION_REJECTED','LOAD_HANDLER','DELETE_HANDLER')
$modernCases=@('CATALOG_SENTINEL_NO_RPC','NO_OBSOLETE_PAGE_SELECT_BUTTONS','NATIVE_MANDATORY_SEATS','AUTO_SEATS_MANDATORY_FIRST','AUTO_SEATS_NO_RPC','SEAT_SCROLL_24_ROWS','SEAT_SCROLL_64_ROWS','SEAT_SCROLL_CONTENT_HEIGHT','SEAT_SCROLL_WHEEL_CLAMPED','SEAT_SCROLL_DESCENDANT_WHEEL','PREVIEW_INPUT_ATTACHED','PREVIEW_CURSOR_ENABLED','PREVIEW_ROTATE_ZOOM_LOCAL','PREVIEW_INPUT_CLEANUP')
$names+=$modernCases
for($i=0;$i -lt 5;$i++){$names+='NO_FAKE_TURRET_'+$i}
$scroll="`n[VEHICLE_UI_SCROLL_SAMPLE] rows=64 height=3712 content_px=5568 viewport_px=618.667 max_px=4949.333 down=58 bottom=4949.333 top=0 child_down=58 normalized_down=0.012 normalized_bottom=1 normalized_repeat=1 normalized_top=0 synthetic_presentation=1 rpc_sent=0 physical_seat_claim=0"
$client=$header+(@($names|ForEach-Object{"[VEHICLE_UI_CHECK] rule=$_ passed=1"}) -join "`n")+$scroll+"`n[VEHICLE_UI_DONE] checks=34 failures=0 jip=0`nGame destroyed."
$timer="`n[VEHICLE_UI_PREVIEW_TIMER] scheduled=1 cancelled=1 before_ms=33 after_ms=-1 hidden_visibility_fixture_override=1 physical_input=0"
$client+=$timer
$jip=$header+(@(@('ALPHA_VEHICLE_FORM','JIP_CONFIRMED_ASSIGNMENT','JIP_STABLE_SLOT')|ForEach-Object{"[VEHICLE_UI_CHECK] rule=$_ passed=1"}) -join "`n")+"`n[VEHICLE_UI_DONE] checks=3 failures=0 jip=1`nGame destroyed."
$server=$header+"[VEHICLE_UI_SERVER_READBACK] player=1 failures=0 jip=0`n[VEHICLE_UI_SERVER_READBACK] player=2 failures=0 jip=1`nGame destroyed."
$count=0
function Check([string]$Name,[string]$Server,[string]$Client,[string]$JIP,[int]$Expected,[string]$Rule,[switch]$Filters,[switch]$Behavior){
 $options=@{}
 foreach($label in @('Server','Client','JIP')){
  $directory=Join-Path $work $label
  [void][IO.Directory]::CreateDirectory($directory)
  $text=$Server
  if($label -eq 'Client'){$text=$Client}
  if($label -eq 'JIP'){$text=$JIP}
  [IO.File]::WriteAllText("$directory/console.log",$text)
  $currentLauncher=$launcher
  $extra=@()
  if($Filters){$extra+=@('-aicfVehicleCatalogFilters','1')}
  if($Behavior){$extra+=@('-aicfVehicleBehaviorReadback','1')}
  if($extra.Count){$currentLauncher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv+$extra}|ConvertTo-Json -Compress)+"`n[SERVER_READY]"}
  [IO.File]::WriteAllText("$directory/launcher.txt",$currentLauncher)
  [IO.File]::WriteAllText("$directory/exit.txt",'0')
  $options[$label+'Log']="$directory/console.log"
  $options[$label+'Launcher']="$directory/launcher.txt"
  $options[$label+'Exit']="$directory/exit.txt"
 }
 if($Filters){$options.CatalogFilters=$true}
 if($Behavior){$options.BehaviorSettings=$true}
 $output=& (Join-Path $RepositoryRoot 'tests/log-audits/Test-VehiclePresetClientLog.ps1') @options
 $code=$LASTEXITCODE
 if($code -ne $Expected -or ($Rule -and ($output -join "`n") -notmatch $Rule)){throw "$Name unexpected verdict: $output"}
 $script:count++
}
try{
 Check 'complete' $server $client $jip 0 ''
 foreach($case in $modernCases){
  Check ('modern-missing-'+$case) $server ($client.Replace('rule='+$case+' passed=1','rule=REMOVED passed=1')) $jip 1 ('CLIENT_CASE_'+$case)
  Check ('modern-duplicate-'+$case) $server ($client+"`n[VEHICLE_UI_CHECK] rule=$case passed=1") $jip 1 ('CLIENT_CASE_'+$case)
 }
 Check 'scroll-missing' $server ($client.Replace($scroll,'')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'timer-missing' $server ($client.Replace($timer,'')) $jip 1 'CLIENT_PREVIEW_TIMER_NATIVE_READBACK'
 Check 'timer-duplicate' $server ($client+$timer) $jip 1 'CLIENT_PREVIEW_TIMER_NATIVE_READBACK'
 Check 'timer-not-scheduled' $server ($client.Replace('before_ms=33','before_ms=0')) $jip 1 'CLIENT_PREVIEW_TIMER_NATIVE_READBACK'
 Check 'timer-not-cancelled' $server ($client.Replace('after_ms=-1','after_ms=33')) $jip 1 'CLIENT_PREVIEW_TIMER_NATIVE_READBACK'
 Check 'scroll-wrong-height' $server ($client.Replace('height=3712','height=58')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-not-moving' $server ($client.Replace('down=58 bottom','down=0 bottom')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-not-clamped' $server ($client.Replace('top=0','top=1')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-descendant-not-moving' $server ($client.Replace('child_down=58','child_down=0')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-physical-claim' $server ($client.Replace('physical_seat_claim=0','physical_seat_claim=1')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-duplicate' $server ($client+$scroll) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-native-endpoint-not-dip-height' $server ($client.Replace('max_px=4949.333','max_px=3712').Replace('bottom=4949.333','bottom=3712')) $jip 0 ''
 Check 'scroll-endpoint-mismatch' $server ($client.Replace('max_px=4949.333','max_px=3712')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-normalized-not-at-bottom' $server ($client.Replace('normalized_bottom=1','normalized_bottom=0.9')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-normalized-repeat-drift' $server ($client.Replace('normalized_repeat=1','normalized_repeat=0.9')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-normalized-not-at-top' $server ($client.Replace('normalized_top=0','normalized_top=0.1')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'scroll-normalized-no-movement' $server ($client.Replace('normalized_down=0.012','normalized_down=0')) $jip 1 'CLIENT_SCROLL_NATIVE_READBACK'
 Check 'wrong-jip' $server $client ($jip.Replace('rule=JIP_CONFIRMED_ASSIGNMENT passed=1','rule=JIP_CONFIRMED_ASSIGNMENT passed=0')) 1 'JIP_CASE_JIP_CONFIRMED_ASSIGNMENT'
 Check 'missing-server-owner' ($server.Replace('player=2 failures=0 jip=1','player=2 failures=0 jip=0')) $client $jip 1 'SERVER_READBACK_1'
 Check 'stale-accepted' $server ($client.Replace('rule=STALE_REVISION_REJECTED passed=1','rule=STALE_REVISION_REJECTED passed=0')) $jip 1 'CLIENT_CASE_STALE_REVISION_REJECTED'
 Check 'duplicate-case' $server ($client+"`n[VEHICLE_UI_CHECK] rule=APPLY_HANDLER passed=1") $jip 1 'CLIENT_CASE_APPLY_HANDLER'
 Check 'unfinished-jip' $server $client ($jip.Replace('Game destroyed.','')) 1 'JIP_STOPPED'
 Check 'shutdown-leaks' ($server+"`nSCRIPT (E): Leaked script instance") $client $jip 1 'SERVER_FULL_FATAL'
 Check 'wrong-cli' $server ($client.Replace('-aicfVehiclePresetClient 1','-aicfVehiclePresetClient 0')) $jip 1 'CLIENT_CLI'
 $filterNames=@('ALL_NATIVE_CATALOG','CIVILIAN_FILTER','CAR_CATEGORY_FILTER','FACTION_FILTER','EMPTY_FILTER_SAFE','EMPTY_CATALOG_CHANGE_NO_RPC','FILTERED_PREFAB_MAPPING','LOAD_REVEALS_DRAFT')
 $filterClient=$client.Replace('[ROSTER_READY]','-aicfVehicleCatalogFilters 1'+"`n[ROSTER_READY]").Replace('checks=34','checks=42')+"`n"+(@($filterNames|ForEach-Object{"[VEHICLE_UI_CHECK] rule=$_ passed=1"}) -join "`n")
 $filterJIP=$jip.Replace('[ROSTER_READY]','-aicfVehicleCatalogFilters 1'+"`n[ROSTER_READY]").Replace('checks=3','checks=5')+"`n[VEHICLE_UI_CHECK] rule=JIP_CIVILIAN_AVAILABLE passed=1`n[VEHICLE_UI_CHECK] rule=JIP_FILTERED_SELECTION passed=1"
 $filterServer=$server.Replace('[ROSTER_READY]','-aicfVehicleCatalogFilters 1'+"`n[ROSTER_READY]")
 Check 'catalog-filters' $filterServer $filterClient $filterJIP 0 '' -Filters
 Check 'wrong-filter-mapping' $filterServer ($filterClient.Replace('rule=FILTERED_PREFAB_MAPPING passed=1','rule=FILTERED_PREFAB_MAPPING passed=0')) $filterJIP 1 'FILTER_CASE_FILTERED_PREFAB_MAPPING' -Filters
 Check 'missing-filter-load' $filterServer ($filterClient.Replace('rule=LOAD_REVEALS_DRAFT','rule=REMOVED')) $filterJIP 1 'FILTER_CASE_LOAD_REVEALS_DRAFT' -Filters
 Check 'filter-shutdown-leaks' ($filterServer+"`nSCRIPT (E): Leaked script instance") $filterClient $filterJIP 1 'SERVER_FULL_FATAL' -Filters
 $behaviorCases=@('BEHAVIOR_INVALID_DISTANCE_NO_RPC','BEHAVIOR_UI_CONTROLS','BEHAVIOR_SAVE_READBACK','BEHAVIOR_APPLY_READBACK','BEHAVIOR_LIBRARY_LOAD_READBACK','GC_NO_AUTHORITY_REJECTED')
 $behaviorClient=$client.Replace('[ROSTER_READY]','-aicfVehicleBehaviorReadback 1'+"`n[ROSTER_READY]").Replace('checks=34','checks=40')+"`n"+(@($behaviorCases|ForEach-Object{"[VEHICLE_UI_CHECK] rule=$_ passed=1"}) -join "`n")
 $behaviorJIP=$jip.Replace('[ROSTER_READY]','-aicfVehicleBehaviorReadback 1'+"`n[ROSTER_READY]").Replace('checks=3','checks=6')+"`n[VEHICLE_UI_CHECK] rule=JIP_BEHAVIOR_CONFIRMED passed=1`n[VEHICLE_UI_CHECK] rule=JIP_BEHAVIOR_UI_READBACK passed=1"+"`n[VEHICLE_UI_CHECK] rule=GC_NO_AUTHORITY_REJECTED passed=1"
 $behaviorServer=$server.Replace('[ROSTER_READY]','-aicfVehicleBehaviorReadback 1'+"`n[ROSTER_READY]")+"`n[VEHICLE_UI_SERVER_BEHAVIOR_READBACK] player=1 valid=1 behavior=1 return_distance=437 schema=2 revision=1 jip=0`n[VEHICLE_UI_SERVER_BEHAVIOR_READBACK] player=2 valid=1 behavior=1 return_distance=437 schema=2 revision=1 jip=1"
 $gcClientMarker="`n[VEHICLE_UI_GC_AUTHORITY] jip=0 actual_server=0 prepared=1 register_denied=1 protect_denied=1 passed=1 entity=123 rpl=321 native_master=0 native_proxy=1 native_role=1"
 $gcJIPMarker=$gcClientMarker.Replace('jip=0','jip=1')
 $gcJIPMarker=$gcJIPMarker.Replace('rpl=321','rpl=322')
 $gcServerSetup="`n[VEHICLE_UI_GC_SERVER_SETUP] player=1 avatar_present=1 world_present=1 resource_valid=1 safe_spawn=1 vehicle_present=1 distance=15`n[VEHICLE_UI_GC_SERVER_SETUP] player=2 avatar_present=1 world_present=1 resource_valid=1 safe_spawn=1 vehicle_present=1 distance=15.2"
 $gcServerOrigin="`n[VEHICLE_UI_GC_SERVER_REPLY] player=1 reason=SERVER_SPAWNED failed=0 entity=500 rpl=321`n[VEHICLE_UI_GC_SERVER_REPLY] player=2 reason=SERVER_SPAWNED failed=0 entity=501 rpl=322"
 $gcServerCleanup="`n[VEHICLE_UI_GC_SERVER_CLEANUP] player=1 rpl=321 exact_empty_delete_requested=1`n[VEHICLE_UI_GC_SERVER_CLEANUP] player=2 rpl=322 exact_empty_delete_requested=1"
 $behaviorServer+=$gcServerSetup+$gcServerOrigin+$gcServerCleanup
 $behaviorClient+=$gcClientMarker
 $behaviorJIP+=$gcJIPMarker
 Check 'behavior-settings' $behaviorServer $behaviorClient $behaviorJIP 0 '' -Behavior
 foreach($case in $behaviorCases){Check ('behavior-missing-'+$case) $behaviorServer ($behaviorClient.Replace('rule='+$case+' passed=1','rule=REMOVED passed=1')) $behaviorJIP 1 ('BEHAVIOR_CASE_'+$case) -Behavior}
 Check 'behavior-jip-wrong-value' $behaviorServer $behaviorClient ($behaviorJIP.Replace('rule=JIP_BEHAVIOR_CONFIRMED passed=1','rule=JIP_BEHAVIOR_CONFIRMED passed=0')) 1 'BEHAVIOR_CASE_JIP_BEHAVIOR_CONFIRMED' -Behavior
 Check 'behavior-server-invalid' ($behaviorServer.Replace('valid=1 behavior=1','valid=0 behavior=1')) $behaviorClient $behaviorJIP 1 'SERVER_BEHAVIOR_READBACK_COUNT' -Behavior
 Check 'behavior-server-wrong-mode' ($behaviorServer.Replace('behavior=1 return_distance','behavior=0 return_distance')) $behaviorClient $behaviorJIP 1 'SERVER_BEHAVIOR_READBACK_COUNT' -Behavior
 Check 'behavior-server-wrong-distance' ($behaviorServer.Replace('return_distance=437','return_distance=300')) $behaviorClient $behaviorJIP 1 'SERVER_BEHAVIOR_READBACK_COUNT' -Behavior
 Check 'behavior-server-old-schema' ($behaviorServer.Replace('schema=2','schema=1')) $behaviorClient $behaviorJIP 1 'SERVER_BEHAVIOR_READBACK_COUNT' -Behavior
 Check 'behavior-server-missing-jip' ($behaviorServer.Replace('revision=1 jip=1','revision=1 jip=0')) $behaviorClient $behaviorJIP 1 'SERVER_BEHAVIOR_READBACK_1' -Behavior
 Check 'behavior-server-changed-revision' ($behaviorServer.Replace('revision=1 jip=1','revision=2 jip=1')) $behaviorClient $behaviorJIP 1 'SERVER_BEHAVIOR_REVISION_CONTINUITY' -Behavior
 Check 'behavior-leaks-still-block' ($behaviorServer+"`nSCRIPT (E): Leaked script instance") $behaviorClient $behaviorJIP 1 'SERVER_FULL_FATAL' -Behavior
 Check 'behavior-cli-required' $server $behaviorClient $behaviorJIP 1 'SERVER_CLI' -Behavior
 $bothClient=$filterClient.Replace('-aicfVehicleCatalogFilters 1','-aicfVehicleCatalogFilters 1 -aicfVehicleBehaviorReadback 1').Replace('checks=42','checks=48')+"`n"+(@($behaviorCases|ForEach-Object{"[VEHICLE_UI_CHECK] rule=$_ passed=1"}) -join "`n")
 $bothJIP=$filterJIP.Replace('-aicfVehicleCatalogFilters 1','-aicfVehicleCatalogFilters 1 -aicfVehicleBehaviorReadback 1').Replace('checks=5','checks=8')+"`n[VEHICLE_UI_CHECK] rule=JIP_BEHAVIOR_CONFIRMED passed=1`n[VEHICLE_UI_CHECK] rule=JIP_BEHAVIOR_UI_READBACK passed=1"
 $bothServer=$behaviorServer.Replace('-aicfVehicleBehaviorReadback 1','-aicfVehicleCatalogFilters 1 -aicfVehicleBehaviorReadback 1')
 $bothClient+=$gcClientMarker
 $bothJIP+="`n[VEHICLE_UI_CHECK] rule=GC_NO_AUTHORITY_REJECTED passed=1"+$gcJIPMarker
 Check 'catalog-and-behavior-settings' $bothServer $bothClient $bothJIP 0 '' -Behavior -Filters
 Check 'gc-missing-client-marker' $behaviorServer ($behaviorClient.Replace($gcClientMarker,'')) $behaviorJIP 1 'CLIENT_GC_AUTHORITY_READBACK' -Behavior
 Check 'gc-missing-jip-marker' $behaviorServer $behaviorClient ($behaviorJIP.Replace($gcJIPMarker,'')) 1 'JIP_GC_AUTHORITY_READBACK' -Behavior
 Check 'gc-wrong-native-realm' $behaviorServer ($behaviorClient.Replace('actual_server=0','actual_server=1')) $behaviorJIP 1 'CLIENT_GC_AUTHORITY_READBACK' -Behavior
 Check 'gc-native-master' $behaviorServer ($behaviorClient.Replace('native_master=0','native_master=1')) $behaviorJIP 1 'CLIENT_GC_AUTHORITY_READBACK' -Behavior
 Check 'gc-not-native-proxy' $behaviorServer $behaviorClient ($behaviorJIP.Replace('native_proxy=1','native_proxy=0')) 1 'JIP_GC_AUTHORITY_READBACK' -Behavior
 Check 'gc-server-origin-missing' ($behaviorServer.Replace($gcServerOrigin,'')) $behaviorClient $behaviorJIP 1 'CLIENT_GC_SERVER_NATIVE_ORIGIN' -Behavior
 Check 'gc-server-rpl-mismatch' ($behaviorServer.Replace('entity=500 rpl=321','entity=500 rpl=999')) $behaviorClient $behaviorJIP 1 'CLIENT_GC_SERVER_NATIVE_ORIGIN' -Behavior
 Check 'gc-server-unsafe-spawn' ($behaviorServer.Replace('safe_spawn=1','safe_spawn=0')) $behaviorClient $behaviorJIP 1 'SERVER_GC_SAFE_SETUP' -Behavior
 Check 'gc-server-distant-spawn' ($behaviorServer.Replace('distance=15.2','distance=500')) $behaviorClient $behaviorJIP 1 'SERVER_GC_SAFE_SETUP' -Behavior
 Check 'gc-server-cleanup-missing' ($behaviorServer.Replace($gcServerCleanup,'')) $behaviorClient $behaviorJIP 1 'CLIENT_GC_SERVER_EXACT_CLEANUP' -Behavior
 $gcRealServer=$behaviorServer.Replace('entity=500 rpl=321','entity=0x4000000000001DE8 {} rpl=321').Replace('entity=501 rpl=322','entity=0x4000000000001F38 {} rpl=322')
 Check 'gc-real-native-hex-with-empty-braces' $gcRealServer $behaviorClient $behaviorJIP 0 '' -Behavior
 Check 'gc-zero-native-hex-identity' ($gcRealServer.Replace('entity=0x4000000000001DE8 {}','entity=0x00000000 {}')) $behaviorClient $behaviorJIP 1 'CLIENT_GC_SERVER_NATIVE_ORIGIN' -Behavior
 Check 'gc-zero-native-decimal-identity' ($behaviorServer.Replace('entity=500 rpl','entity=0 rpl')) $behaviorClient $behaviorJIP 1 'CLIENT_GC_SERVER_NATIVE_ORIGIN' -Behavior
 Check 'gc-extra-native-identity-token' ($gcRealServer.Replace('entity=0x4000000000001DE8 {} rpl','entity=0x4000000000001DE8 {} EXTRA rpl')) $behaviorClient $behaviorJIP 1 'CLIENT_GC_SERVER_NATIVE_ORIGIN' -Behavior
 Check 'gc-teardown-null-manager-fatal' ($behaviorServer+"`nSCRIPT (E): NULL pointer to instance. AICF_VehicleGCProbeSafeDelete GetPlayerManager") $behaviorClient $behaviorJIP 1 'SERVER_FULL_FATAL' -Behavior
 Check 'gc-unprepared' $behaviorServer ($behaviorClient.Replace('prepared=1','prepared=0')) $behaviorJIP 1 'CLIENT_GC_AUTHORITY_READBACK' -Behavior
 Check 'gc-register-allowed' $behaviorServer ($behaviorClient.Replace('register_denied=1','register_denied=0')) $behaviorJIP 1 'CLIENT_GC_AUTHORITY_READBACK' -Behavior
 Check 'gc-protect-allowed' $behaviorServer ($behaviorClient.Replace('protect_denied=1','protect_denied=0')) $behaviorJIP 1 'CLIENT_GC_AUTHORITY_READBACK' -Behavior
 Check 'gc-duplicate-marker' $behaviorServer ($behaviorClient+$gcClientMarker) $behaviorJIP 1 'CLIENT_GC_AUTHORITY_READBACK' -Behavior
 Check 'gc-jip-case-missing' $behaviorServer $behaviorClient ($behaviorJIP.Replace('rule=GC_NO_AUTHORITY_REJECTED passed=1','rule=REMOVED passed=1')) 1 'BEHAVIOR_CASE_GC_NO_AUTHORITY_REJECTED' -Behavior
}finally{
 $resolved=[IO.Path]::GetFullPath($work)
 $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe test cleanup path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
Write-Output "PASS Vehicle preset client/JIP log contracts controls=$count"
