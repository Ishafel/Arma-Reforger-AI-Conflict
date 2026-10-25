param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$work=Join-Path ([IO.Path]::GetTempPath()) ('aicf-crew-log-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work|Out-Null
$argv=@('-gproj','source','-profile','profile','-addonsDir','addons','-addons','CORE,MAP','-aicfVehiclePresetClient','1','-aicfVehicleCrewReturnProbe','1')
$header='CLI Params: '+($argv -join ' ')+"`n[ROSTER_READY]`n"
$launcher='AICF_RUNTIME_MANIFEST_JSON='+(@{arguments=$argv}|ConvertTo-Json -Compress)+"`n[SERVER_READY]"
$server=$header
$serverRules=@('GROUP10_CREATED','TEN_TRACKED_NONPREFIX','RETURN_ROUTE_START_OUTSIDE_BASE','ONLY_ASSIGNED_MEMBER_RETAINED','ROLE_AND_TRACKING_REMAPPED','NINE_MANAGED_RETURNING','UAZ_SINGLE_EXACT_DRIVER','RETURN_FINISHED_BEFORE_DONE')
foreach($rule in $serverRules){$server+="[CREW_CHECK] rule=$rule passed=1`n"}
$server+="[CREW_INVENTORY] unchanged=1`n[VEHICLE_CREW_REDUCED] slot=S0 kept=1 returning=9 generation=2`n"
for($i=0;$i -lt 9;$i++){$server+="[VEHICLE_CREW_RETURNED] member=entity$i base=home reason=ARRIVED`n"}
$server+="[VEHICLE_PRESET_PURCHASE] prefab=UAZ469_uncovered_CIV_blue.et supplies=50`n[CREW_DONE] checks=8 failures=0`nGame destroyed."
$client=$header
$clientRules=@('NO_FACTION_REJECTED','ALPHA_VEHICLE_FORM','GROUP10_READY','UAZ_SELECTED','COMMANDER_SLIDER_AND_PERSISTENT_HOVER','PRIVATE_VEHICLE_PREVIEW','UNSEATED_TEXT_RESOLVED','BUTTON_HOVER_COLOR','BUTTON_HOVER_RESTORED','AUTOMATIC_PRESERVES_UNSAVED_SEATS','CONFIRMATION_WITHOUT_SIDE_EFFECTS','CANCEL_PRESERVES_SQUAD_AND_DRAFT','CONFIRM_AGAIN','WRONG_CONFIRMATION_REFUSED','CONFIRMED_10_TO_1')
foreach($rule in $clientRules){$client+="[VEHICLE_UI_CHECK] rule=$rule passed=1`n"}
$client+="[VEHICLE_UI_DONE] checks=15 failures=0 jip=0`nGame destroyed."
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
 foreach($rule in $serverRules){Run ('missing-'+$rule) ($server.Replace("rule=$rule passed=1", "rule=$rule passed=0")) $client 1}
 foreach($rule in $clientRules){Run ('missing-'+$rule) $server ($client.Replace("rule=$rule passed=1", "rule=$rule passed=0")) 1}
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
