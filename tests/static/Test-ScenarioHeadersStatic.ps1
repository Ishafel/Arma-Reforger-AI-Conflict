param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$failures=[Collections.Generic.List[string]]::new()
function Add-Failure($Rule,$Message){$failures.Add("[$Rule] $Message")}
function Require-Match($Rule,$Text,$Pattern,$Message){if($Text -notmatch $Pattern){Add-Failure $Rule $Message}}
function Forbid-Match($Rule,$Text,$Pattern,$Message){if($Text -match $Pattern){Add-Failure $Rule $Message}}
. (Join-Path $RepositoryRoot 'tools/AICF_Scenarios.ps1')
$catalog=Get-AICFScenarioCatalog
$retired=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/retired-scenarios.json'))|ConvertFrom-Json
if($retired.resources.Count -ne 81){Add-Failure 'RETIRED_COUNT' 'Expected all 81 retired identities'}
foreach($row in $retired.resources){foreach($path in @($row.path,($row.path+'.meta'))){if(Test-Path -LiteralPath (Join-Path $RepositoryRoot $path)){Add-Failure 'RETIRED_RESOURCE' $path}}}
$missions=@()
foreach($project in $catalog.localProjects.PSObject.Properties){
 $directory=Join-Path $RepositoryRoot $project.Value
 $gproj=[IO.File]::ReadAllText((Join-Path $directory 'addon.gproj'))
 Require-Match 'PROJECT_ID' $gproj ('GUID\s+"'+$project.Name+'"') $project.Value
 Require-Match 'VANILLA_DEPENDENCY' $gproj '"58D0FB3206B6F859"' $project.Value
 foreach($platform in @('PC','HEADLESS','XBOX_ONE','XBOX_SERIES','PS4')){Require-Match 'PROJECT_PLATFORM' $gproj ('GameProjectConfig\s+'+$platform+'\b') $platform}
 $files=@(Get-ChildItem -LiteralPath $directory -Recurse -File)
 if(@($files|Where-Object Extension -in @('.ent','.layer')).Count){Add-Failure 'WORLD_OWNERSHIP' $project.Value}
 $missions+=@($files|Where-Object { $_.Extension -ceq '.conf' -and $_.Directory.Name -ceq 'Missions' })
 $source=(@($files|Where-Object Extension -in @('.c','.conf','.meta')|ForEach-Object {[IO.File]::ReadAllText($_.FullName)}) -join "`n")
 foreach($row in $retired.resources){if($source.Contains($row.resource)){Add-Failure 'RETIRED_REFERENCE' $row.resource}}
}
if($missions.Count -ne 1 -or $missions[0].Name -cne 'AICF_Conflict_Arland_Setup.conf'){Add-Failure 'ONE_MISSION' 'Only setup must remain'}
$setup=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArland/Missions/AICF_Conflict_Arland_Setup.conf'))
Require-Match 'SETUP_NATIVE_PARENT' $setup '^SCR_MissionHeaderCampaign\s*:\s*"\{C41618FD18E9D714\}Missions/23_Campaign_Arland.conf"' 'Native Arland parent'
Require-Match 'SETUP_VISIBLE' $setup 'm_bShowInScenarioMenu\s+1' 'Visible common entry'
Forbid-Match 'SETUP_TOPOLOGY' $setup '(?m)^\s*(World|SystemsConfig|m_aCampaignCustomBaseList|m_aAICFSetupPresets)\b' 'No topology or legacy presets'
$meta=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArland/Missions/AICF_Conflict_Arland_Setup.conf.meta'))
Require-Match 'SETUP_ID' $meta '\{A1CF261019500001\}Missions/AICF_Conflict_Arland_Setup.conf' 'Stable entry identity'
foreach($platform in @('PC','HEADLESS','XBOX_ONE','XBOX_SERIES','PS4')){Require-Match 'SETUP_PLATFORM' $meta ('CONFResourceClass\s+'+$platform+'\b') $platform}
$runtime=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Config/AICF_RuntimeScenario.c'))
foreach($policy in @('header.m_eSaveTypes = 0;','header.m_eStartingRank = SCR_ECharacterRank.GENERAL;','header.m_bIgnoreMinimumVehicleRank = true;','header.m_bRandomStartingWeather = true;','header.m_bRandomWeatherChanges = true;')){if(!$runtime.Contains($policy)){Add-Failure 'RUNTIME_POLICY' $policy}}
$rhsEveronScripts = @(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'AIConflictEveronRHS') -Recurse -File -Filter '*.c' -ErrorAction SilentlyContinue)
if ($rhsEveronScripts.Count -ne 1 -or $rhsEveronScripts[0].Name -cne 'AICF_RHSEveronCallsignPool.c') {
    Add-Failure 'SCENARIO_RHS_EVERON_COMPOSITION' 'RHS Everon must contain only the local callsign compatibility adapter; lifecycle, radio policy and RHS profile remain in dependencies'
}
else {
    $callsignAdapter = Get-Content -LiteralPath $rhsEveronScripts[0].FullName -Raw
    Require-Match 'SCENARIO_RHS_EVERON_CALLSIGN' $callsignAdapter `
        'modded\s+class\s+SCR_CampaignMilitaryBaseManager[\s\S]*override\s+protected\s+void\s+GetSharedCallsignPool\(notnull array<int> outIndexes\)' `
        'RHS Everon callsign adapter must use the pinned stock pool boundary'
    Require-Match 'SCENARIO_RHS_EVERON_CALLSIGN_AUTHORITY' $callsignAdapter `
        'super\.GetSharedCallsignPool\(outIndexes\);[\s\S]*!GetGame\(\)\.InPlayMode\(\)[\s\S]*!Replication\.IsServer\(\)[\s\S]*!m_Campaign\.IsMaster\(\)[\s\S]*return;' `
        'RHS Everon callsign extension must preserve the stock pool and require server/master authority'
    Require-Match 'SCENARIO_RHS_EVERON_CALLSIGN_SCOPE' $callsignAdapter `
        '!GetGame\(\)\.GetWorldFile\(\)\.Contains\("CTI_Campaign_Eden_RHS"\)[\s\S]*return;' `
        'The compatibility adapter must leave Arland and stock Everon unchanged'
    Require-Match 'SCENARIO_RHS_EVERON_CALLSIGN_CAPACITY' $callsignAdapter `
        'originalCount == 0[\s\S]*return;[\s\S]*base && base\.IsInitialized\(\)[\s\S]*initializedBases\+\+[\s\S]*initializedBases <= originalCount[\s\S]*return;[\s\S]*int index = originalCount; index < initializedBases; index\+\+[\s\S]*outIndexes\.Insert\(index\)' `
        'Extend only an insufficient nonempty pool, with unique indexes bounded by initialized bases'
    Forbid-Match 'SCENARIO_RHS_EVERON_COMPOSITION' $callsignAdapter `
        'OnGameStart|OnGameEnd|CallLater|new\s+AICF_MatchController|SetFaction\(|SetCallsignIndex\(|Replication\.BumpMe|SpawnEntity|DeleteEntity' `
        'The adapter must only compose pool data; stock owns initialization and replicated callsign assignment'
}


if($failures.Count){$failures;exit 1}
'Scenario resources: PASS; setup=1 retired=81; project identities, platforms, native policy and composition checked'
