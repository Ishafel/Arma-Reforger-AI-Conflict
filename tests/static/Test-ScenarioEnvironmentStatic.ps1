param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$root=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$files=@('Config/AICF_ScenarioEnvironment.c','Integration/AICF_ScenarioEnvironmentHandler.c','UI/AICF_ScenarioLaunch.c','Config/AICF_ScenarioSetup.c','UI/AICF_ScenarioSetupScreen.c')
$sources=@($files|ForEach-Object {ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $root $_)))})
$rules=@(
 @(0,'GameSessionStorage.s_Data\[SESSION_KEY\] = json.SaveToString\(\)','RELOAD_STORAGE'),
 @(0,'Consume\(ResourceName actual\).*?Find\(SESSION_KEY, data\).*?Clear\(\);.*?json.LoadFromString','CONSUME_ONCE'),
 @(0,'ReadValue\(.*, target\) \|\| target != actual','EXACT_TARGET'),
 @(0,'age < 0 \|\| age > 600000 \|\| !registry.IsLeaf\(actual\)','FRESH_LEAF'),
 @(0,'OnMissionSet\(MissionHeader mission\).*?super.OnMissionSet\(mission\);.*?Consume\(actual\).*?header.m_AICFEnvironment = settings','MISSION_BOUNDARY'),
 @(1,'if \(!Replication.IsServer\(\).*?loadDone \|\| m_bSavedSettingApplied \|\| !loadedWeatherState.IsEmpty\(\)\).*?super.SetupDaytimeAndWeather.*?return;','AUTHORITY_SAVELOAD'),
 @(1,'if \(m_bAICFEnvironmentApplied\) return;','IDEMPOTENT'),
 @(1,'GetWeatherStatesList\(states\).*?state.GetStateName\(\) == weather','SUPPORTED_WEATHER'),
 @(1,'m_bRandomStartingDaytimeRange = false;.*?m_bUsePredefineStartingTimeAndWeather = false;','INDEPENDENT_TIME'),
 @(1,'m_bRandomStartingWeather = weather.IsEmpty\(\);.*?super.SetupDaytimeAndWeather\(hours, minutes, seconds, weather, false\)','NATIVE_OWNER'),
 @(2,'!CanStart\(selection, entry, config\)\) return false;.*?ScenarioEnvironment.Queue\(resource, selection.GetEnvironment\(\)\).*?RequestScenarioChangeTransition.*?RequestPublicServerTransition','QUEUE_BEFORE_TRANSITION'),
 @(2,'if \(!accepted\)\s*\{\s*AICF_ScenarioEnvironment.Clear\(\);\s*return false;','REJECT_CLEANUP'),
 @(0,'WriteValue\(.*, 2\).*?ReadValue\(.*, schema\) \|\| schema != 2','MINUTE_SCHEMA'),
 @(1,'minutes = settings.GetMinute\(\);','MINUTE_APPLICATION'),
 @(3,'minutes < 0 \|\| minutes > AICF_ScenarioEnvironment.RANDOM_TIME \|\| weather < 0 \|\| weather > 4\) return false;.*?m_Environment.m_iStartMinutes = minutes;.*?m_Environment.m_iWeather = weather','ATOMIC_SELECTION'),
 @(4,'headings\[7\].*?m_aButtons.Count\(\) != 21','UI_CONTROLS'),
 @(4,'SetRange\(0, 1439\);.*?SetStep\(1\)','MINUTE_SLIDER'),
 @(4,'OnChange\(Widget w, bool finished\).*?w != m_wTimeSlider \|\| m_bClosing \|\| m_bTimeRefreshing.*?SelectEnvironment\(Math.Round\(m_wTimeSlider.GetCurrent\(\)\), m_Selection.GetEnvironment\(\).m_iWeather\)','UI_INDEPENDENT_AXES'),
 @(4,'bool SelectEnvironment\(int time, int weather\).*?RefreshEnvironment\(\);\s*return true;','LIGHTWEIGHT_DRAG')
)
$failures=@();$mutations=0
foreach($rule in $rules){
 $pattern='(?s)'+$rule[1];$source=$sources[$rule[0]]
 if($source -notmatch $pattern){$failures+=$rule[2];continue}
 if([regex]::Replace($source,$pattern,'',1) -match $pattern){$failures+='MUTATION_'+$rule[2]}
 $mutations++
}
if(($sources[0..1]-join '') -match 'Rpc\(|BumpMe|CallLater|SaveToFile|LoadFromFile|SetHoursMinutesSeconds|ForceWeatherTo\('){$failures+='NATIVE_STATE_BOUNDARY'}
if($sources[0] -match 'Arland|Everon|Missions/'){$failures+='MAP_INDEPENDENT'}
if($failures.Count){$failures;exit 1}
"Scenario environment static: PASS; guarded mutations=$mutations"
