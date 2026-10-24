param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$path = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_ScenarioBootstrap.c'
if (-not (Test-Path -LiteralPath $path)) { 'WORKSHOP_BOOTSTRAP_MISSING'; exit 1 }
$source = [IO.File]::ReadAllText($path)
$code = ConvertTo-AICFCodeText $source
$launch = ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_ScenarioLaunch.c')))
$screen = ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/UI/AICF_ScenarioSetupScreen.c')))
$failures = @()
$rules = @(
    @($code,'if \(GameStateTransitions.IsInsideMainMenu\(\)\) return false;', 'PREPARATION_WORLD'),
    @($source,'System.IsCLIParam\("server"\).*?System.IsCLIParam\("client"\).*?System.IsCLIParam\("world"\)', 'EXPLICIT_CLI'),
    @($code,'RplSession.Mode\(\) == RplMode.Client.*?RplSession.Mode\(\) == RplMode.Dedicated', 'REMOTE_PEER'),
    @($code,'return header && header.m_bAICFSetupEntry;', 'HEADER_OPT_IN'),
    @($code,'!entry \|\| !ShouldIntercept\(\).*?entry.Id\(\) == GetGame\(\).GetMissionHeader\(\).GetHeaderResourceName\(\)', 'ENTRY_IDENTITY'),
    @($code,'!ShouldIntercept\(\) \|\| !active \|\| active.GetHeaderResourceName\(\) != m_sEntry', 'CALLBACK_IDENTITY'),
    @($code,'if \(!m_bScanRequested\).*?m_bScanRequested = true;.*?workshop.ScanOfflineItems\(\);', 'SCAN_ONCE'),
    @($code,'m_iAttempts\+\+ < 120', 'BOUNDED_SCAN'),
    @($code,'WorkshopApi.GetCurrentMission\(\);.*?entry.Id\(\) != m_sEntry', 'CURRENT_MISSION'),
    @($code,'AICF_OpenSetup\(entry\).*?AICF_IsSetupOpen\(\)', 'OPEN_READBACK'),
    @($code,'Remove\(Open\);.*?m_AICFScenarioBootstrap.Stop\(\);.*?super.OnGameEnd\(\);', 'CALLBACK_CLEANUP'),
    @($code,'OnBeforeWorldCleanup\(\).*?AICF_CloseSetup\(\);.*?super.OnBeforeWorldCleanup\(\);', 'UI_CLEANUP'),
    @($code,'void Cancel\(\).*?!ShouldIntercept\(\) \|\| GameStateTransitions.IsTransitionRequestedOrInProgress\(\).*?RequestGameplayEndTransition\(\);', 'CANCEL_BOUNDARY'),
    @($launch,'if \(!AICF_ScenarioBootstrap.CanConfigure\(entry\)\) return false;.*?workshop.GetDownloads\(nativeDownloads\);.*?count < 0 \|\| count != nativeDownloads.Count\(\).*?item.IsProcessed\(\) \|\| item.GetPendingDownload\(\)', 'NATIVE_DOWNLOAD_GATE'),
    @($screen,'index == 14\) Cancel\(\);.*?modalRoot == m_wRoot\) Cancel\(\);.*?void Cancel\(\).*?AICF_CloseSetup\(\);.*?AICF_ScenarioBootstrap.Cancel\(\);', 'CANCEL_ACTION')
)
$mutations = 0
foreach ($rule in $rules) {
    $pattern = '(?s)' + $rule[1]
    if ($rule[0] -notmatch $pattern) { $failures += $rule[2]; continue }
    if ([regex]::Replace($rule[0], $pattern, '', 1) -match $pattern) { $failures += 'MUTATION_' + $rule[2] }
    $mutations++
}
if ($source -match 'Arland|Everon|[A-F0-9]{16}|GameSessionStorage|SpawnEntity|Rpc\(|BumpMe') { $failures += 'PREPARATION_BOUNDARY' }
$header = [IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArland/Missions/AICF_Conflict_Arland_Setup.conf'))
if ($header -notmatch '^SCR_MissionHeaderCampaign\s*:' -or $header -notmatch 'm_bAICFSetupEntry\s+1') { $failures += 'NATIVE_HEADER_TYPE' }
if ($header -notmatch 'm_sName\s+"AI Conflict"' -or $header -match 'm_s(Description|Details)\s+"#') { $failures += 'VANILLA_VISIBLE_METADATA' }
$bootstrap = [IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_ArlandCampaignBootstrap.c'))
if ($bootstrap -notmatch '(?s)super.OnGameStart\(\);.*?if \(AICF_ScenarioBootstrap.ShouldIntercept\(\)\)\s*return;.*?m_sAICFMapKey =') { $failures += 'NO_INTERMEDIATE_MATCH' }
if ($failures.Count) { $failures; exit 1 }
"Workshop bootstrap static: PASS; guarded mutations=$mutations; native header; exact local entry; no intermediate AICF match"
exit 0
