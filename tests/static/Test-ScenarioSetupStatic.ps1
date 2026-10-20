param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
. (Join-Path $RepositoryRoot 'tools/AICF_Scenarios.ps1')
$selection=ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_ScenarioSetup.c')))
$screen=ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_ScenarioSetupScreen.c')))
$entry=ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_ScenarioSetupEntry.c')))
$launch=ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_ScenarioLaunch.c')))
function Test-SetupGuards([string]$a,[string]$b,[string]$c) {
    $combined=$a+$b+$c
    if($combined -match 'Rpc\(|BumpMe|SetFaction|SetHealth|CallLater|GetOrigin|SpawnEntity|RequestClose|SetAICommanderMode|m_On\w+\.Insert|GetOn\w+\([^)]*\)\.Insert'){'PRELAUNCH_ONLY'}
    if($a -match 'Arland|Everon|Missions/'){ 'MAP_INDEPENDENT' }
    foreach($rule in @(
        @($a,'mode < 0 \|\| mode > 2 \|\| difficulty < 0 \|\| difficulty > 2','BOUNDS'),
        @($a,'!m_aAICFSetupPresets \|\| m_aAICFSetupPresets.Count\(\) != 9','COMPLETE'),
        @($a,'mode \* 3 \+ difficulty','INDEX'),
        @($a,'!target \|\| AICF_ScenarioSetupHeader.Cast\(target\)','NO_RECURSION'),
        @($a,'target.m_eAICFMatchMode != mode \|\| target.m_eAICFDifficulty != difficulty','METADATA'),
        @($a,'target.GetWorldResourceName\(\) != GetWorldResourceName\(\)','WORLD'),
        @($a,'if \(!m_Header \|\| !GetVariant\(\) \|\| GetVariant\(\).Resolve\(mode, difficulty\).IsEmpty\(\)\) return false;\s*m_iMode = mode;\s*m_iDifficulty = difficulty;','ATOMIC_CHOICE'),
        @($b,'AICF_ScenarioLaunch.Start\(m_Selection, m_Entry, m_Config\)','NATIVE_LAUNCH'),
        @($b,'m_aButtons\[8\].SetEnabled\(mode != 0\);\s*m_aButtons\[9\].SetEnabled\(mode != 0\);','STANDARD_SIDES'),
        @($b,'AddModal\(m_wRoot, m_aButtons\[0\]\)','MODAL'),
        @($b,'m_wRoot.RemoveHandler\(this\);.*?RemoveModal\(m_wRoot\);.*?m_wRoot.RemoveFromHierarchy\(\);','CLEANUP'),
        @($c,'if \(!header\) return false;','OPT_IN'),
        @($c,'AICF_GetSetupHeader\(MissionWorkshopItem scenario\).*?MissionHeader.ReadMissionHeader\(scenario.Id\(\)\)','CATALOG_HEADER'),
        @($c,'if \(AICF_OpenSetup\(scenario\)\) return;\s*super.TryPlayScenario\(scenario\);','PLAY_PASSTHROUGH'),
        @($c,'if \(AICF_OpenSetup\(scenario, config\)\) return;\s*super.TryHostScenario\(scenario, config\);','HOST_PASSTHROUGH')
    )){if($rule[0] -notmatch ('(?s)'+$rule[1])){$rule[2]}}
}
$failures=@(Test-SetupGuards $selection $screen $entry)
$mutations=@(
    @('a','mode < 0 || mode > 2','false','BOUNDS'),
    @('a','m_aAICFSetupPresets.Count() != 9','false','COMPLETE'),
    @('a','mode * 3 + difficulty','mode + difficulty','INDEX'),
    @('a','|| AICF_ScenarioSetupHeader.Cast(target)','','NO_RECURSION'),
    @('a','target.m_eAICFMatchMode != mode','false','METADATA'),
    @('a','target.GetWorldResourceName() != GetWorldResourceName()','false','WORLD'),
    @('a','if (!m_Header || !GetVariant() || GetVariant().Resolve(mode, difficulty).IsEmpty()) return false;','','ATOMIC_CHOICE'),
    @('b','AICF_ScenarioLaunch.Start(m_Selection, m_Entry, m_Config)','false','NATIVE_LAUNCH'),
    @('b','m_aButtons[8].SetEnabled(mode != 0);','m_aButtons[8].SetEnabled(true);','STANDARD_SIDES'),
    @('b','RemoveModal(m_wRoot);','GetModal();','CLEANUP'),
    @('c','if (!header) return false;','if (!header) return true;','OPT_IN'),
    @('c','MissionHeader.ReadMissionHeader(scenario.Id())','scenario.GetHeader()','CATALOG_HEADER'),
    @('c','super.TryPlayScenario(scenario);','','PLAY_PASSTHROUGH'),
    @('c','super.TryHostScenario(scenario, config);','','HOST_PASSTHROUGH'),
    @('b','Refresh();','GetGame().GetCallqueue().CallLater(Refresh, 1000);','PRELAUNCH_ONLY')
)
foreach($mutation in $mutations){
    $a=$selection;$b=$screen;$c=$entry
    switch($mutation[0]){'a'{$a=$a.Replace($mutation[1],$mutation[2])}'b'{$b=$b.Replace($mutation[1],$mutation[2])}'c'{$c=$c.Replace($mutation[1],$mutation[2])}}
    if($mutation[3] -notin @(Test-SetupGuards $a $b $c)){$failures+="MUTATION_ESCAPED $($mutation[3])"}
}
$path=Join-Path $RepositoryRoot 'AIConflictArland/Missions/AICF_Conflict_Arland_Setup.conf'
$header=[IO.File]::ReadAllText($path)
$resources=@([regex]::Matches($header,'"(\{[A-F0-9]{16}\}Missions/[^"\r\n]+)"')|ForEach-Object {$_.Groups[1].Value})
$expected=@('{A1CF261018460002}Missions/AICF_Conflict_Arland_Coop_USSR_Medium.conf')
$catalog=Get-AICFScenarioCatalog
foreach($mode in @('Standard','CoopUS','CoopUSSR')){foreach($difficulty in @('Easy','Medium','Hard')){$expected+=(Resolve-AICFScenario -Catalog $catalog -Variant Stock -MatchMode $mode -Difficulty $difficulty).scenario}}
if(($resources -join '|') -cne ($expected -join '|')){$failures+='PRESET_CATALOG'}
if($header -match '(?m)^\s*(World|SystemsConfig|m_sPath|m_aCampaignCustomBaseList)\b'){$failures+='INHERITED_WORLD'}
$meta=[IO.File]::ReadAllText($path+'.meta')
if($meta -notmatch '\{A1CF261019500001\}Missions/AICF_Conflict_Arland_Setup.conf'){$failures+='STABLE_RESOURCE'}
if($failures.Count){$failures;exit 1}
"Scenario setup static: PASS; presets=9; negative mutations=$($mutations.Count)"
exit 0
