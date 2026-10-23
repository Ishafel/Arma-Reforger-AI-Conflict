param([Parameter(Mandatory)][string]$LogPath,[Parameter(Mandatory)][string]$ManifestPath,[Parameter(Mandatory)][string]$NativeExitPath,[switch]$Transition,[switch]$Dedicated)
$ErrorActionPreference='Stop'
if($Dedicated -and !$Transition){throw 'Dedicated requires Transition readback mode'}
$log=[IO.File]::ReadAllText($LogPath)
$manifest=[IO.File]::ReadAllText($ManifestPath)|ConvertFrom-Json
$failures=@()
if((Get-Content -LiteralPath $NativeExitPath -Raw).Trim() -ne '0'){$failures+='NATIVE_EXIT'}
if($log -notmatch 'Game destroyed\.'){$failures+='STOPPED_LOG'}
if($log -match 'SCRIPT\s*\([EF]\)|ENGINE\s*\(F\)|SCRIPT EXCEPTION|NULL pointer|VM exception|Unhandled exception'){$failures+='SCRIPT_VM_FATAL'}
if($log -match 'RESOURCES\s*\(E\):[^\r\n]*(?:Flags/|AICF_)'){$failures+='PROJECT_RESOURCE'}
$argv=@($manifest.arguments)
$hostProbe=[array]::IndexOf($argv,'-aicfProbeHost') -ge 0
if($hostProbe -and (!$Transition -or $Dedicated)){throw 'Host requires frontend Transition mode'}
$probe='-aicfUniversalProbe';if($Transition){$probe='-aicfTransitionProbe'}
if(@($argv|Where-Object {$_ -ceq $probe}).Count -ne 1){$failures+='PROBE_CLI'}
$cli=@([regex]::Matches($log,'CLI Params: ([^\r\n]+)'))
if($cli.Count -ne 1){$failures+='CLI_COUNT'}
else{
 foreach($flag in @('-gproj','-profile','-addonsDir','-addons','-aicfExpectedReadyVariants')){
  $index=[array]::IndexOf($argv,$flag)
  if($index -lt 0 -or !$cli[0].Groups[1].Value.Contains($flag+' '+$argv[$index+1])){$failures+='CLI_MISMATCH'}
 }
}
if($Transition){
 . (Join-Path $PSScriptRoot '../../tools/AICF_Scenarios.ps1')
 $catalog=Get-AICFScenarioCatalog
 $values=@{}
 foreach($flag in @('-aicfProbeMap','-aicfProbeContent','-aicfProbeMode','-aicfProbeDifficulty')){
  $index=[array]::IndexOf($argv,$flag)
  if($index -lt 0 -or $argv[$index+1] -notmatch '^[012]$'){throw 'Invalid transition manifest'}
  $values[$flag]=[int]$argv[$index+1]
  if($cli.Count -ne 1 -or !$cli[0].Groups[1].Value.Contains($flag+' '+$argv[$index+1])){$failures+='CLI_MISMATCH'}
 }
 $entry=@($catalog.variants|Where-Object {$_.mapIndex -eq $values['-aicfProbeMap'] -and $_.contentIndex -eq $values['-aicfProbeContent']})
 if($entry.Count -ne 1){throw 'Invalid transition target'}
 $plan=Resolve-AICFScenario -Catalog $catalog -Variant $entry[0].name -MatchMode @('Standard','CoopUS','CoopUSSR')[$values['-aicfProbeMode']] -Difficulty @('Easy','Medium','Hard')[$values['-aicfProbeDifficulty']]
 $launch=@([regex]::Matches($log,'\[SCENARIO_LAUNCH\] variant=(\w+) mode=(\d) difficulty=(\d) resource=(\S+) addons=([^\r\n]+)'))
 if($Dedicated){
  foreach($flag in @('-server','-MissionHeader')){
   $index=[array]::IndexOf($argv,$flag)
   $target=$plan.scenario;if($flag -eq '-MissionHeader'){$target=$plan.missionHeader}
   if($index -lt 0 -or $argv[$index+1] -cne $target -or $cli.Count -ne 1 -or !$cli[0].Groups[1].Value.Contains($flag+' '+$target)){$failures+='LAUNCH_PLAN'}
  }
 }elseif($launch.Count -ne 1 -or $launch[0].Groups[1].Value -cne $entry[0].name -or $launch[0].Groups[2].Value -ne "$($plan.mode)" -or $launch[0].Groups[3].Value -ne "$($plan.difficultyIndex)" -or $launch[0].Groups[4].Value -cne $plan.scenario -or $launch[0].Groups[5].Value.Trim() -cne $plan.addonIds){$failures+='LAUNCH_PLAN'}
 $cases=@([regex]::Matches($log,'\[TRANSITION_CASE\] case=(\w+) pass=([01])'))
 $requiredCases=@('PREFLIGHT','ACCEPTED','ACTIVE_HEADER','EXACT_GRAPH')
 if($Dedicated){$requiredCases=@('ACTIVE_HEADER','EXACT_GRAPH')}
 if($cases.Count -ne $requiredCases.Count){$failures+='CASE_COUNT'}
 foreach($name in $requiredCases){
  $found=@($cases|Where-Object {$_.Groups[1].Value -ceq $name})
  if($found.Count -ne 1 -or $found[0].Groups[2].Value -ne '1'){$failures+='CASE_'+$name}
 }
 $loaded=@([regex]::Matches($log,'\[TRANSITION_LOADED\] id=([A-F0-9]{16}) vanilla=([01])'))
 $required=@($plan.addonIds.Split(','))
 foreach($id in $required){if(@($loaded|Where-Object {$_.Groups[1].Value -ceq $id}).Count -ne 1){$failures+='LOADED_'+$id}}
 foreach($addon in $loaded){if($addon.Groups[2].Value -eq '0' -and $addon.Groups[1].Value -cnotin $required){$failures+='EXTRA_ADDON'}}
 if(@([regex]::Matches($log,'\[TRANSITION_DONE\] header=1 graph=1 roster=1')).Count -ne 1 -or $log -notmatch '\[ROSTER_READY\]' -or $log -match '\[TRANSITION_TIMEOUT\]'){$failures+='COMPLETION'}
 if($hostProbe){
  $hostIndex=[array]::IndexOf($argv,'-aicfProbeHost')
  if($argv[$hostIndex+1] -cne '1' -or @($argv|Where-Object {$_ -ceq '-aicfProbeHost'}).Count -ne 1){throw 'Invalid Host manifest'}
  if($cli.Count -ne 1 -or !$cli[0].Groups[1].Value.Contains('-aicfProbeHost 1')){$failures+='CLI_MISMATCH'}
  $hostMode=@([regex]::Matches($log,'\[HOST_MODE\] mode=(\d+) listen=([01])'))
  if($hostMode.Count -ne 1 -or $hostMode[0].Value -cne '[HOST_MODE] mode=2 listen=1' -or $log -notmatch 'Starting RPL server, listening .*:24021'){$failures+='HOST_LISTEN'}
  $hostRequest=@([regex]::Matches($log,'\[HOST_REQUEST\] pass=([01])'))
  if($hostRequest.Count -ne 1 -or $hostRequest[0].Groups[1].Value -ne '1'){$failures+='HOST_REQUEST'}
 }
 $resumeIndex=[array]::IndexOf($argv,'-aicfProbeResumeAfter')
 if($resumeIndex -ge 0){
  $pause=@([regex]::Matches($log,'\[TRANSITION_PAUSE\] pass=([01]) real_ms=(\d+) simulation_ms=([\d.]+)'))
  if([int]$argv[$resumeIndex+1] -lt 35 -or $pause.Count -ne 1 -or $pause[0].Groups[1].Value -ne '1' -or [double]$pause[0].Groups[2].Value -lt [int]$argv[$resumeIndex+1]*1000 -or [double]$pause[0].Groups[3].Value -ge 2000){$failures+='PAUSE_OBSERVATION'}
 }
 $timeIndex=[array]::IndexOf($argv,'-aicfProbeTime')
 $weatherIndex=[array]::IndexOf($argv,'-aicfProbeWeather')
 if($timeIndex -ge 0 -or $weatherIndex -ge 0){
  if($timeIndex -lt 0 -or $weatherIndex -lt 0 -or $argv[$timeIndex+1] -notmatch '^\d{1,4}$' -or [int]$argv[$timeIndex+1] -gt 1440 -or $argv[$weatherIndex+1] -notmatch '^[0-4]$'){throw 'Invalid environment manifest'}
  $time=[int]$argv[$timeIndex+1];$weather=[int]$argv[$weatherIndex+1]
  foreach($flag in @('-aicfProbeTime','-aicfProbeWeather')){
   $index=[array]::IndexOf($argv,$flag)
   if($cli.Count -ne 1 -or !$cli[0].Groups[1].Value.Contains($flag+' '+$argv[$index+1])){$failures+='CLI_MISMATCH'}
  }
  $applied=@([regex]::Matches($log,'\[ENVIRONMENT_APPLIED\] time_minutes=(\d{1,4}) weather_mode=(\d) hour=([\d.]+) weather=(\w+) authority=server'))
  $observed=@([regex]::Matches($log,'\[TRANSITION_ENVIRONMENT\] pass=([01]) time_minutes=(\d{1,4}) weather_mode=(\d) hour=([\d.]+) weather=(\w+)'))
  if($applied.Count -ne 1 -or $observed.Count -ne 1){$failures+='ENVIRONMENT_COUNT'}
  else{
   $record=$applied[0];$hour=[double]$record.Groups[3].Value
   if([int]$record.Groups[1].Value -ne $time -or [int]$record.Groups[2].Value -ne $weather -or $hour -lt 0 -or $hour -ge 24){$failures+='ENVIRONMENT_SELECTION'}
   if($time -lt 1440 -and [math]::Abs($hour-$time/60.0) -gt 0.001){$failures+='ENVIRONMENT_TIME'}
   if($weather -gt 0 -and $record.Groups[4].Value -cne @('','Clear','Cloudy','Overcast','Rainy')[$weather]){$failures+='ENVIRONMENT_WEATHER'}
   if($observed[0].Groups[1].Value -ne '1' -or [int]$observed[0].Groups[2].Value -ne $time -or [int]$observed[0].Groups[3].Value -ne $weather){$failures+='ENVIRONMENT_READBACK'}
  }
  foreach($name in @('ENVIRONMENT_REPEAT','ENVIRONMENT_SAVE_RESTORE','ENVIRONMENT_CLOCK')){
   $found=@([regex]::Matches($log,('\['+$name+'\] pass=([01])')))
   if($found.Count -ne 1 -or $found[0].Groups[1].Value -ne '1'){$failures+=$name}
  }
  if($log -match '\[ENVIRONMENT_FALLBACK\]'){$failures+='ENVIRONMENT_FALLBACK'}
 }
 if($failures.Count){$failures;exit 1}
 "Universal transition full stopped log: PASS; target=$($entry[0].name); dedicated=$Dedicated; cases=$($requiredCases.Count); loaded=$($required.Count)"
 exit 0
}
$variants=@('Stock','Everon','EveronNorth','RHS','EveronRHS','EveronNorthRHS','ArlandWCSRHS','EveronWCSRHS','EveronNorthWCSRHS')
$names=@('REGISTRY','ENABLED_MATRIX')+@($variants|ForEach-Object {'PLAN_'+$_})+@('EXPECTED_AVAILABLE','VANILLA_READY','BASE_SOURCE_COMPATIBLE','ATOMIC_INVALID','HOST_GRAPH','NULL_ENTRY_BLOCKED','FORCED_GRAPH','UI_OPEN','UI_GUIDE_BUTTON','UI_GUIDE_OPEN','UI_GUIDE_LAYOUT','UI_GUIDE_RESTORE','UI_GUIDE_REOPEN','UI_BOUNDS','UI_MODAL','UI_FLAGS_HELP','UI_MAP_PREVIEWS','UI_CARD_IDENTITY','UI_BRIEFING_LAYOUT','UI_TEXT_PALETTE','UI_NO_RANDOM_TIME','UI_START_ENABLED','UI_CONTENT_MASK','UI_ENVIRONMENT','UI_TIME_SLIDER','ENVIRONMENT_STORAGE')
if([array]::IndexOf($argv,'-aicfProbeEnabledContent') -ge 0){$names+='ENABLED_CONFIGURATION'}
for($mode=0;$mode -lt 3;$mode++){for($difficulty=0;$difficulty -lt 3;$difficulty++){$names+="UI_${mode}_${difficulty}"}}
$names+=@('UI_ALL_HELP')+@($variants|ForEach-Object {'UI_VARIANT_'+$_})+@('UI_CLOSED','UI_IDEMPOTENT')
$cases=@([regex]::Matches($log,'\[UNIVERSAL_CASE\] case=(\w+) pass=([01])'))
if($cases.Count -ne $names.Count){$failures+='CASE_COUNT'}
foreach($name in $names){$found=@($cases|Where-Object {$_.Groups[1].Value -ceq $name});if($found.Count -ne 1 -or $found[0].Groups[2].Value -cne '1'){$failures+='CASE_'+$name}}
$ready=@([regex]::Matches($log,'\[UNIVERSAL_AVAILABLE\] variant=(\w+) available=([01]) missing=([^\r\n]*?) graph=([^\r\n]+)'))
foreach($variant in $variants){if(@($ready|Where-Object {$_.Groups[1].Value -ceq $variant}).Count -ne 1){$failures+='AVAILABILITY_'+$variant}}
$expectedIndex=[array]::IndexOf($argv,'-aicfExpectedReadyVariants')
if($expectedIndex -lt 0 -or @($ready|Where-Object {$_.Groups[2].Value -eq '1'}).Count -ne [int]$argv[$expectedIndex+1]){$failures+='AVAILABLE_COUNT'}
$done=@([regex]::Matches($log,'\[UNIVERSAL_DONE\] checks=(\d+) failed=(\d+)'))
$maskIndex=[array]::IndexOf($argv,'-aicfExpectedContentMask')
$content=@([regex]::Matches($log,'\[UNIVERSAL_CONTENT\] expected=(\d+) visible=(\d+)'))
if($maskIndex -lt 0 -or $argv[$maskIndex+1] -notin @('1','3','7') -or $content.Count -ne 1 -or $content[0].Groups[1].Value -cne $argv[$maskIndex+1] -or $content[0].Groups[2].Value -cne $argv[$maskIndex+1]){$failures+='CONTENT_MASK'}
foreach($flag in @('-aicfExpectedContentMask','-aicfProbeEnabledContent')){
 $index=[array]::IndexOf($argv,$flag)
 if($index -ge 0 -and ($cli.Count -ne 1 -or !$cli[0].Groups[1].Value.Contains($flag+' '+$argv[$index+1]))){$failures+='CLI_MISMATCH'}
}
if($done.Count -ne 1 -or $done[0].Groups[1].Value -ne "$($names.Count)" -or $done[0].Groups[2].Value -ne '0'){$failures+='COMPLETION'}
if($failures.Count){$failures;exit 1}
"Universal scenario full stopped log: PASS; cases=$($names.Count); available=$($argv[$expectedIndex+1])"
