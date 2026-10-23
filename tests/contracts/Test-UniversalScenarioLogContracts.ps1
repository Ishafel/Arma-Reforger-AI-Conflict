param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$base=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$work=Join-Path $base ('aicf-universal-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work|Out-Null
$audit=Join-Path $RepositoryRoot 'tests/log-audits/Test-UniversalScenarioLog.ps1'
$manifest=@{arguments=@('-gproj','C:\source\addon.gproj','-profile','C:\fresh','-addonsDir','C:\source,C:\mods','-addons','CORE,MAP','-aicfUniversalProbe','1','-aicfExpectedReadyVariants','9')}|ConvertTo-Json -Compress
$variants=@('Stock','Everon','EveronNorth','RHS','EveronRHS','EveronNorthRHS','ArlandWCSRHS','EveronWCSRHS','EveronNorthWCSRHS')
$names=@('REGISTRY','ENABLED_MATRIX')+@($variants|ForEach-Object {'PLAN_'+$_})+@('EXPECTED_AVAILABLE','VANILLA_READY','BASE_SOURCE_COMPATIBLE','ATOMIC_INVALID','HOST_GRAPH','NULL_ENTRY_BLOCKED','FORCED_GRAPH','UI_OPEN','UI_BOUNDS','UI_MODAL','UI_FLAGS_HELP','UI_MAP_PREVIEWS','UI_CARD_IDENTITY','UI_BRIEFING_LAYOUT','UI_TEXT_PALETTE','UI_NO_RANDOM_TIME','UI_START_ENABLED','UI_CONTENT_MASK','UI_ENVIRONMENT','UI_TIME_SLIDER','ENVIRONMENT_STORAGE')
for($mode=0;$mode -lt 3;$mode++){for($difficulty=0;$difficulty -lt 3;$difficulty++){$names+="UI_${mode}_${difficulty}"}}
$names+=@('UI_ALL_HELP')+@($variants|ForEach-Object {'UI_VARIANT_'+$_})+@('UI_CLOSED','UI_IDEMPOTENT')
$log=(@('CLI Params: -gproj C:\source\addon.gproj -profile C:\fresh -addonsDir C:\source,C:\mods -addons CORE,MAP -aicfUniversalProbe 1 -aicfExpectedReadyVariants 9')+@($names|ForEach-Object {"[UNIVERSAL_CASE] case=$_ pass=1"})+@($variants|ForEach-Object {"[UNIVERSAL_AVAILABLE] variant=$_ available=1 missing= graph=CORE,MAP"})+@("[UNIVERSAL_DONE] checks=$($names.Count) failed=0",'Game destroyed.'))-join "`n"
$maskManifest=$manifest|ConvertFrom-Json
$maskManifest.arguments+=@('-aicfExpectedContentMask','7')
$manifest=$maskManifest|ConvertTo-Json -Compress
$log=$log.Replace('-aicfExpectedReadyVariants 9','-aicfExpectedReadyVariants 9 -aicfExpectedContentMask 7')+[Environment]::NewLine+'[UNIVERSAL_CONTENT] expected=7 visible=7'
$count=0
function Check([string]$name,[string]$value,[string]$plan,[int]$native,[int]$expected,[string]$rule,[switch]$Transition,[switch]$Dedicated){
 [IO.File]::WriteAllText("$work/log.txt",$value);[IO.File]::WriteAllText("$work/manifest.json",$plan);[IO.File]::WriteAllText("$work/exit.txt","$native")
 $auditArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$audit,'-LogPath',"$work/log.txt",'-ManifestPath',"$work/manifest.json",'-NativeExitPath',"$work/exit.txt")
 if($Transition){$auditArgs+='-Transition'}
 if($Dedicated){$auditArgs+='-Dedicated'}
 $output=& powershell.exe @auditArgs 2>&1
 if($LASTEXITCODE -ne $expected -or ($rule -and ($output-join "`n") -notmatch $rule)){throw "Unexpected ${name}: $output"}
 $script:count++;"PASS $name"
}
try{
 Check positive $log $manifest 0 0 ''
 $unicodePath='C:\'+[char]0x041C+[char]0x043E+[char]0x0434+[char]0x044B
 Check unicode ($log.Replace('C:\mods',$unicodePath)) ($manifest.Replace('C:\\mods',$unicodePath.Replace('\','\\'))) 0 0 ''
 Check missing ($log.Replace('[UNIVERSAL_CASE] case=EXPECTED_AVAILABLE pass=1','')) $manifest 0 1 CASE_EXPECTED_AVAILABLE
 Check failed ($log.Replace('case=UI_FLAGS_HELP pass=1','case=UI_FLAGS_HELP pass=0')) $manifest 0 1 CASE_UI_FLAGS_HELP
 Check duplicate ($log+"`n[UNIVERSAL_CASE] case=UI_OPEN pass=1") $manifest 0 1 CASE_UI_OPEN
 Check completion ($log.Replace('failed=0','failed=1')) $manifest 0 1 COMPLETION
 Check missingdone ($log.Replace("[UNIVERSAL_DONE] checks=$($names.Count) failed=0",'')) $manifest 0 1 COMPLETION
 Check stopped ($log.Replace('Game destroyed.','running')) $manifest 0 1 STOPPED_LOG
 Check script ($log+"`nSCRIPT (E): test") $manifest 0 1 SCRIPT_VM_FATAL
 Check vm ($log+"`nVM exception") $manifest 0 1 SCRIPT_VM_FATAL
 Check resource ($log+"`nRESOURCES (E): Wrong GUID Flags/flag_us") $manifest 0 1 PROJECT_RESOURCE
 Check native $log $manifest 1 1 NATIVE_EXIT
 Check cli ($log.Replace('-addons CORE,MAP','-addons OTHER')) $manifest 0 1 CLI_MISMATCH
 Check probe $log ($manifest.Replace('-aicfUniversalProbe','-unknown')) 0 1 PROBE_CLI
 Check availability ($log.Replace('variant=RHS available=1','variant=RHS available=0')) $manifest 0 1 AVAILABLE_COUNT
 Check missingvariant ($log.Replace('[UNIVERSAL_AVAILABLE] variant=RHS available=1 missing= graph=CORE,MAP','')) $manifest 0 1 AVAILABILITY_RHS
 $target='{A1CF261018460002}Missions/AICF_Conflict_Arland_Coop_USSR_Medium.conf'
 $graph='9178E5822AFE48EA,B52C5F6AEDBF423E'
 $transitionArgs=@('-gproj','C:\source\addon.gproj','-profile','C:\fresh','-addonsDir','C:\source,C:\mods','-addons',$graph,'-aicfTransitionProbe','1','-aicfExpectedReadyVariants','9','-aicfProbeMap','0','-aicfProbeContent','0','-aicfProbeMode','2','-aicfProbeDifficulty','1')
 $transitionManifest=@{arguments=$transitionArgs}|ConvertTo-Json -Compress
 $transitionLog=(@('CLI Params: '+($transitionArgs -join ' '),"[SCENARIO_LAUNCH] variant=Stock mode=2 difficulty=1 resource=$target addons=$graph")+@(@('PREFLIGHT','ACCEPTED','ACTIVE_HEADER','EXACT_GRAPH')|ForEach-Object {"[TRANSITION_CASE] case=$_ pass=1"})+@('[TRANSITION_LOADED] id=9178E5822AFE48EA vanilla=0','[TRANSITION_LOADED] id=B52C5F6AEDBF423E vanilla=0','[ROSTER_READY]','[TRANSITION_DONE] header=1 graph=1 roster=1','Game destroyed.'))-join "`n"
 Check transition $transitionLog $transitionManifest 0 0 '' -Transition
 Check wrongtarget ($transitionLog.Replace("resource=$target",'resource=OTHER')) $transitionManifest 0 1 LAUNCH_PLAN -Transition
 Check extramod ($transitionLog+"`n[TRANSITION_LOADED] id=1337C0DE5DABBEEF vanilla=0") $transitionManifest 0 1 EXTRA_ADDON -Transition
 Check missingmod ($transitionLog.Replace('[TRANSITION_LOADED] id=B52C5F6AEDBF423E vanilla=0','')) $transitionManifest 0 1 LOADED_B52C5F6AEDBF423E -Transition
 Check wronggraph ($transitionLog.Replace('case=EXACT_GRAPH pass=1','case=EXACT_GRAPH pass=0')) $transitionManifest 0 1 CASE_EXACT_GRAPH -Transition
 Check noroster ($transitionLog.Replace('[ROSTER_READY]','')) $transitionManifest 0 1 COMPLETION -Transition
 Check lateerror ($transitionLog+"`nSCRIPT (E): after transition") $transitionManifest 0 1 SCRIPT_VM_FATAL -Transition
 $hostArgs=$transitionArgs+@('-aicfProbeHost','1')
 $hostManifest=@{arguments=$hostArgs}|ConvertTo-Json -Compress
 $hostLog=$transitionLog.Replace('CLI Params: '+($transitionArgs -join ' '),'CLI Params: '+($hostArgs -join ' '))+"`nStarting RPL server, listening 0.0.0.0:24021`n[HOST_MODE] mode=2 listen=1`n[HOST_REQUEST] pass=1"
 Check host $hostLog $hostManifest 0 0 '' -Transition
 Check hostplay ($hostLog.Replace('[HOST_MODE] mode=2 listen=1','[HOST_MODE] mode=0 listen=0')) $hostManifest 0 1 HOST_LISTEN -Transition
 Check hostnosocket ($hostLog.Replace('Starting RPL server, listening','Missing socket')) $hostManifest 0 1 HOST_LISTEN -Transition
 Check hostrequest ($hostLog.Replace('HOST_REQUEST] pass=1','HOST_REQUEST] pass=0')) $hostManifest 0 1 HOST_REQUEST -Transition
 Check hostmissing ($hostLog.Replace('[HOST_REQUEST] pass=1','')) $hostManifest 0 1 HOST_REQUEST -Transition
 Check hosttimeout ($hostLog+"`n[TRANSITION_TIMEOUT]") $hostManifest 0 1 COMPLETION -Transition
 Check hostlateerror ($hostLog+"`nSCRIPT (E): host late error") $hostManifest 0 1 SCRIPT_VM_FATAL -Transition
 Check hostduplicate ($hostLog+"`n[HOST_MODE] mode=0 listen=0") $hostManifest 0 1 HOST_LISTEN -Transition
 Check hostrequestduplicate ($hostLog+"`n[HOST_REQUEST] pass=0") $hostManifest 0 1 HOST_REQUEST -Transition
 $dedicatedArgs=$transitionArgs+@('-server',$target,'-MissionHeader',$target.Substring(18))
 $dedicatedManifest=@{arguments=$dedicatedArgs}|ConvertTo-Json -Compress
 $dedicatedLines=@($transitionLog -split "`n"|Where-Object {$_ -notmatch '^CLI Params|\[SCENARIO_LAUNCH\]|case=PREFLIGHT|case=ACCEPTED'})
 $dedicatedLog=(@('CLI Params: '+($dedicatedArgs -join ' '))+$dedicatedLines)-join "`n"
 Check dedicated $dedicatedLog $dedicatedManifest 0 0 '' -Transition -Dedicated
 Check dedicatedwrong ($dedicatedLog.Replace('-server '+$target,'-server OTHER')) $dedicatedManifest 0 1 LAUNCH_PLAN -Transition -Dedicated
 Check dedicatedgraph ($dedicatedLog.Replace('case=EXACT_GRAPH pass=1','case=EXACT_GRAPH pass=0')) $dedicatedManifest 0 1 CASE_EXACT_GRAPH -Transition -Dedicated
 $pauseManifest=@{arguments=$transitionArgs+@('-aicfProbeResumeAfter','35')}|ConvertTo-Json -Compress
 $pauseLog=$transitionLog.Replace('CLI Params: '+($transitionArgs -join ' '),'CLI Params: '+(($transitionArgs+@('-aicfProbeResumeAfter','35')) -join ' '))+"`n[TRANSITION_PAUSE] pass=1 real_ms=35050 simulation_ms=33"
 Check paused $pauseLog $pauseManifest 0 0 '' -Transition
 Check pausefailed ($pauseLog.Replace('TRANSITION_PAUSE] pass=1','TRANSITION_PAUSE] pass=0')) $pauseManifest 0 1 PAUSE_OBSERVATION -Transition
 Check pauserealtime ($pauseLog.Replace('real_ms=35050','real_ms=30000')) $pauseManifest 0 1 PAUSE_OBSERVATION -Transition
 Check pausesimulation ($pauseLog.Replace('simulation_ms=33','simulation_ms=35000')) $pauseManifest 0 1 PAUSE_OBSERVATION -Transition
 $environmentArgs=$transitionArgs+@('-aicfProbeTime','817','-aicfProbeWeather','4')
 $environmentManifest=@{arguments=$environmentArgs}|ConvertTo-Json -Compress
 $environmentLog=$transitionLog.Replace('CLI Params: '+($transitionArgs -join ' '),'CLI Params: '+($environmentArgs -join ' '))+"`n[ENVIRONMENT_APPLIED] time_minutes=817 weather_mode=4 hour=13.616667 weather=Rainy authority=server`n[TRANSITION_ENVIRONMENT] pass=1 time_minutes=817 weather_mode=4 hour=13.62 weather=Rainy`n[ENVIRONMENT_REPEAT] pass=1`n[ENVIRONMENT_SAVE_RESTORE] pass=1`n[ENVIRONMENT_CLOCK] pass=1"
 Check environment $environmentLog $environmentManifest 0 0 '' -Transition
 Check environmenttime ($environmentLog.Replace('hour=13.616667 weather','hour=13 weather')) $environmentManifest 0 1 ENVIRONMENT_TIME -Transition
 Check environmentweather ($environmentLog.Replace('weather=Rainy authority','weather=Clear authority')) $environmentManifest 0 1 ENVIRONMENT_WEATHER -Transition
 Check environmentrepeat ($environmentLog.Replace('ENVIRONMENT_REPEAT] pass=1','ENVIRONMENT_REPEAT] pass=0')) $environmentManifest 0 1 ENVIRONMENT_REPEAT -Transition
 Check environmentsave ($environmentLog.Replace('ENVIRONMENT_SAVE_RESTORE] pass=1','ENVIRONMENT_SAVE_RESTORE] pass=0')) $environmentManifest 0 1 ENVIRONMENT_SAVE_RESTORE -Transition
 Check environmentread ($environmentLog.Replace('TRANSITION_ENVIRONMENT] pass=1','TRANSITION_ENVIRONMENT] pass=0')) $environmentManifest 0 1 ENVIRONMENT_READBACK -Transition
 Check environmentmissing ($environmentLog.Replace('[ENVIRONMENT_APPLIED]','[MISSING]')) $environmentManifest 0 1 ENVIRONMENT_COUNT -Transition
 Check environmentfallback ($environmentLog+"`n[ENVIRONMENT_FALLBACK]") $environmentManifest 0 1 ENVIRONMENT_FALLBACK -Transition
 Check environmentclock ($environmentLog.Replace('ENVIRONMENT_CLOCK] pass=1','ENVIRONMENT_CLOCK] pass=0')) $environmentManifest 0 1 ENVIRONMENT_CLOCK -Transition
 Check environmentminute ($environmentLog.Replace('time_minutes=817 weather_mode=4 hour=13.616667','time_minutes=816 weather_mode=4 hour=13.616667')) $environmentManifest 0 1 ENVIRONMENT_SELECTION -Transition
 Check environmentslider ($log.Replace('UI_TIME_SLIDER pass=1','UI_TIME_SLIDER pass=0')) $manifest 0 1 CASE_UI_TIME_SLIDER
 Check enabledmatrix ($log.Replace('ENABLED_MATRIX pass=1','ENABLED_MATRIX pass=0')) $manifest 0 1 CASE_ENABLED_MATRIX
 Check previews ($log.Replace('UI_MAP_PREVIEWS pass=1','UI_MAP_PREVIEWS pass=0')) $manifest 0 1 CASE_UI_MAP_PREVIEWS
 Check cardidentity ($log.Replace('UI_CARD_IDENTITY pass=1','UI_CARD_IDENTITY pass=0')) $manifest 0 1 CASE_UI_CARD_IDENTITY
 Check briefinglayout ($log.Replace('UI_BRIEFING_LAYOUT pass=1','UI_BRIEFING_LAYOUT pass=0')) $manifest 0 1 CASE_UI_BRIEFING_LAYOUT
 Check textpalette ($log.Replace('UI_TEXT_PALETTE pass=1','UI_TEXT_PALETTE pass=0')) $manifest 0 1 CASE_UI_TEXT_PALETTE
 Check norandomtime ($log.Replace('UI_NO_RANDOM_TIME pass=1','UI_NO_RANDOM_TIME pass=0')) $manifest 0 1 CASE_UI_NO_RANDOM_TIME
 Check hiddencontent ($log.Replace('visible=7','visible=3')) $manifest 0 1 CONTENT_MASK
 Check missingcontent ($log.Replace('[UNIVERSAL_CONTENT] expected=7 visible=7','')) $manifest 0 1 CONTENT_MASK
 Check contentcli ($log.Replace('-aicfExpectedContentMask 7','-aicfExpectedContentMask 3')) $manifest 0 1 CLI_MISMATCH
}
finally{
 $target=[IO.Path]::GetFullPath($work)
 if(!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $target) -notlike 'aicf-universal-log-*'){throw 'Unsafe cleanup'}
 Remove-Item -LiteralPath $target -Recurse -Force
}
"Universal scenario log contracts: PASS; cases=$count"
