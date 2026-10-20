param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$files=@('Config/AICF_ScenarioRegistry.c','Config/AICF_ScenarioSetup.c','UI/AICF_ScenarioLaunch.c','UI/AICF_ScenarioSetupScreen.c','UI/AICF_ScenarioSetupEntry.c','State/AICF_GroupSlot.c','Bootstrap/AICF_MatchController.c')
$sources=@($files|ForEach-Object {ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $core $_)))})
$rules=@(
 @(0,'available.Find\(id\) >= 0.*?missing = id;.*?return false;','ALL_DEPENDENCIES'),
 @(0,'GetAvailableAddons\(available\);.*?!HasDependencies\(available, missing\)','LOCAL_AVAILABILITY'),
 @(0,'revision.IsDownloaded\(\) && !revision.IsCorrupted\(\) && item.IsReadyToRun\(\)','REVISION_READY'),
 @(0,'foreach \(string id : m_aAddons\).*?result \+= id;','EXACT_GRAPH'),
 @(1,'!variant.IsAvailable\(missing\)\) return false;\s*m_iMap = mapIndex;\s*m_iContent = content;','ATOMIC_VARIANT'),
 @(2,'!CanStart\(selection, entry, config\)\) return false;','PREFLIGHT'),
 @(2,'IsInsideMainMenu\(\).*?IsTransitionRequestedOrInProgress\(\)','FRONTEND_ONLY'),
 @(2,'IsHostingDisabledOnPlatform\(\).*?CanHost\(entry\).*?CanPlay\(entry\)','NATIVE_GATES'),
 @(2,'GetDownloadQueueState\(completed, total\);\s*if \(total > 0\) return false;','DOWNLOAD_GATE'),
 @(2,'variant.IsAvailable\(missing\) && IsForcedGraphCompatible\(variant\)','FORCED_GRAPH_GATE'),
 @(2,'!GameProject.IsVanillaAddon\(id\) && variant.m_aAddons.Find\(id\) < 0\) return false;','FORCED_SUBSET'),
 @(2,'RequestScenarioChangeTransition\(resource,\s*, variant.GetAddonList\(\)\)','PLAY_GRAPH'),
 @(2,'config.game.mods = variant.BuildHostMods\(\);.*?RequestPublicServerTransition\(config\)','HOST_GRAPH'),
 @(2,'if \(!accepted\).*?scenarioId = oldScenario;.*?hostedScenarioModId = oldOwner;.*?mods = oldMods;','HOST_ROLLBACK'),
 @(2,'if \(!accepted\) return false;.*?AICF_CloseSetup\(\);.*?CloseAllMenus\(\)','CLOSE_AFTER_ACCEPT'),
 @(3,'OnFocus\(.*?Help\(index\).*?OnMouseEnter\(.*?Help\(index\)','HELP_INPUT'),
 @(3,'Flag\(8, westFlag\);.*?Flag\(9, eastFlag\);','FLAGS'),
 @(3,'m_aButtons\[contentButton \+ 3\].SetEnabled\(available\)','DISABLED_CONTENT'),
 @(4,'registry.IsLeaf\(scenario.Id\(\)\)\) return null;.*?super.CreateLine','HIDE_LEAVES'),
 @(4,'GetDefaultScenarios\(\).*?AICF_FilterPresets.*?GetModdedScenarios\(\).*?AICF_FilterPresets','HOST_LISTS'),
 @(5,'bool BeginInitialSpawn\(\).*?m_tSpawnStartedAt = GetGame\(\).GetWorld\(\).GetTimestamp\(\);','INITIAL_CLOCK'),
 @(5,'bool BeginReplacementSpawn\(int nowMilliseconds\).*?m_tSpawnStartedAt = GetGame\(\).GetWorld\(\).GetTimestamp\(\);','REPLACEMENT_CLOCK'),
 @(5,'GetSpawnSimulationAgeMs\(\).*?GetTimestamp\(\).DiffMilliseconds\(m_tSpawnStartedAt\)','SIMULATION_CLOCK'),
 @(6,'slot.GetSpawnSimulationAgeMs\(\) >= GROUP_SPAWN_TIMEOUT_MS','PAUSE_SAFE_DEADLINE')
)
$failures=@();$mutations=0
foreach($rule in $rules){
 $source=$sources[$rule[0]];$pattern='(?s)'+$rule[1]
 if($source -notmatch $pattern){$failures+=$rule[2];continue}
 # Removing each guarded region must be caught by this rule.
 $changed=[regex]::Replace($source,$pattern,'',1)
 if($changed -match $pattern){$failures+='MUTATION_'+$rule[2]}
 $mutations++
}
if(($sources[0..4] -join '') -match 'RequestReload\(|Rpc\(|BumpMe\(|SpawnEntity|SetAICommanderMode'){$failures+='PRELAUNCH_ONLY'}
if($sources[0] -match 'Arland|Everon|Missions/'){$failures+='INTEGRATION_BOUNDARY'}
& (Join-Path $RepositoryRoot 'tools/Update-AICFScenarioRegistry.ps1') -RepositoryRoot $RepositoryRoot -Check
if($failures.Count){$failures;exit 1}
"Universal scenario static: PASS; guarded mutations=$mutations; variants=9; presets=81"
