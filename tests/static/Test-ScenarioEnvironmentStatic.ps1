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
 @(3,'time < 0 \|\| time > 4 \|\| weather < 0 \|\| weather > 4\) return false;.*?m_Environment.m_iTime = time;.*?m_Environment.m_iWeather = weather','ATOMIC_SELECTION'),
 @(4,'headings\[7\].*?m_aButtons.Count\(\) != 25','UI_CONTROLS'),
 @(4,'SelectEnvironment\(index - 15, m_Selection.GetEnvironment\(\).m_iWeather\).*?SelectEnvironment\(m_Selection.GetEnvironment\(\).m_iTime, index - 20\)','UI_INDEPENDENT_AXES')
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
