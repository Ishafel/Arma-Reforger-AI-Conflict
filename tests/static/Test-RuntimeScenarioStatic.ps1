param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources=@{}
foreach($entry in @{
 runtime='Config/AICF_RuntimeScenario.c'; state='Integration/AICF_CampaignState.c'
 registry='Config/AICF_ScenarioRegistry.c'; handoff='UI/AICF_ScenarioHostTransition.c'
}.GetEnumerator()){$sources[$entry.Key]=[regex]::Replace([IO.File]::ReadAllText((Join-Path $core $entry.Value)),'(?s)/\*.*?\*/|//[^\r\n]*','')}
$rules=@(
 @('runtime','string target = GetTarget\(\);.*?WriteValue\(.*, target\)','FULL_TARGET'),
 @('runtime','!selection.IsValid\(\) \|\| selection.GetTarget\(\) != target','EXACT_TARGET'),
 @('runtime','age < 0 \|\| age > 600000','TTL'),
 @('runtime','if \(!System.IsCLIParam\(.*server.*\)\) return null;','CLI_SERVER_ONLY'),
 @('runtime','mode != .*0.* && mode != .*1.* && mode != .*2','CLI_MODE_BOUNDS'),
 @('runtime','difficulty != .*0.* && difficulty != .*1.* && difficulty != .*2','CLI_DIFFICULTY_BOUNDS'),
 @('runtime','if \(System.IsCLIParam\(.*client.*\) \|\| RplSession.Mode\(\) == RplMode.Client\).*?ApplyReplica\(\);\s*return;','CLIENT_NO_INTENT'),
 @('runtime','Find\(SESSION_KEY, data\).*?ClearPending\(\);\s*value = AICF_RuntimeScenarioSelection.Decode\(data\)','CONSUME_ONCE'),
 @('runtime','value.GetTarget\(\) != header.GetHeaderResourceName\(\) \|\| !AICF_ScenarioHostTransition.HasExactGraph','TARGET_AND_GRAPH'),
 @('runtime','!registry.ConfigureRuntimeMap\(header, value.GetVariant\(\)\).*?End\(\);.*?return;','MAP_FAIL_CLOSED'),
 @('runtime','OnWorldPostProcess\(World world\).*?BeforeStart\(\);\s*super.OnWorldPostProcess\(world\)','BEFORE_NATIVE_WEATHER'),
 @('runtime','OnGameStart\(\).*?BeforeStart\(\);\s*return super.OnGameStart\(\)','BEFORE_NATIVE_BASES'),
 @('runtime','OnGameEnd\(\).*?super.OnGameEnd\(\);\s*AICF_RuntimeScenario.End\(\)','RESTORE_LIFECYCLE'),
 @('runtime','if \(s_Snapshot\) s_Snapshot.Restore\(\);\s*s_Snapshot = null;\s*s_Selection = null;','RELEASE_SNAPSHOT'),
 @('runtime','!campaign \|\| !campaign.AICF_HasRuntimeScenario\(\)\) return;','REPLICA_READY'),
 @('runtime','!value.IsValid\(\) \|\| value.GetTarget\(\) != header.GetHeaderResourceName\(\)\) return;','REPLICA_IDENTITY'),
 @('state','\[RplProp\(onRplName: .*AICF_OnRuntimeScenarioReplicated.*\)\]\s*protected string m_sAICFRuntimeVariant;','JIP_VARIANT'),
 @('state','\[RplProp\(onRplName: .*AICF_OnRuntimeScenarioReplicated.*\)\]\s*protected int m_iAICFRuntimeDifficulty;','JIP_DIFFICULTY'),
 @('state','\[RplProp\(onRplName: .*AICF_OnRuntimeScenarioReplicated.*\)\]\s*protected bool m_bAICFRuntimeReady;','JIP_READY'),
 @('state','AICF_SetRuntimeScenario\(.*?if \(!Replication.IsServer\(\) \|\| !IsMaster\(\) \|\| !selection \|\| !selection.IsValid\(\)\) return;','AUTHORITY'),
 @('state','m_iAICFRuntimeDifficulty == selection.m_iDifficulty && m_eAICFMatchMode == selection.m_iMode\) return;.*?m_bAICFRuntimeReady = true;\s*Replication.BumpMe\(\);','CHANGED_ONLY'),
 @('state','AICF_ApplyRuntimeReplicaOptions\(\).*?if \(Replication.IsServer\(\) \|\| !m_bAICFRuntimeReady\) return;','LOCAL_CACHE_READY'),
 @('handoff','applied.m_iMap != intent.m_iMap \|\| applied.m_iContent != intent.m_iContent \|\|\s*applied.m_iMode != intent.m_iMode \|\| applied.m_iDifficulty != intent.m_iDifficulty','SHARED_TARGET_POLICY'),
 @('registry','mode < 0 \|\| mode > 2 \|\| difficulty < 0 \|\| difficulty > 2','PLAN_BOUNDS')
)
$failures=@();$mutations=0
foreach($rule in $rules){
 $pattern='(?s)'+$rule[1];$source=$sources[$rule[0]]
 if($source -notmatch $pattern){$failures+=$rule[2];continue}
 if([regex]::Replace($source,$pattern,'',1) -match $pattern){$failures+='MUTATION_'+$rule[2]}
 $mutations++
}
# Snapshot must restore every header member assigned by runtime presentation.
$assigned=@([regex]::Matches($sources.runtime,'(?m)^\s*header\.(m_\w+) =')|ForEach-Object {$_.Groups[1].Value}|Sort-Object -Unique)
foreach($member in $assigned){if($sources.runtime -notmatch ('m_Header\.'+[regex]::Escape($member)+' =')){$failures+='SNAPSHOT_'+$member}}
$replica=[regex]::Match($sources.state,'(?s)void AICF_ApplyRuntimeReplicaOptions\(\)(.*?)void AICF_SetRuntimeScenario').Value
if($replica -match 'BumpMe|m_sAICFRuntimeVariant\s*=|m_iAICFRuntimeDifficulty\s*=|m_bAICFRuntimeReady\s*='){$failures+='REPLICA_WRITES_RPL'}
if($sources.runtime -match 'Arland|Everon|Missions/|CallLater|SpawnEntity|SetFaction|BumpMe'){$failures+='DOMAIN_BOUNDARY'}
$launcher=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tools/Start-AICFRuntime.ps1'))
if($launcher -notmatch '(?s)\$requiredCliFragments \+= \(\$scenarioPlan.selectionArguments\[\$index\].*?\$scenarioPlan.selectionArguments\[\$index \+ 1\].*?Wait-AICFServerReady'){$failures+='CLIENT_SELECTION_GATE'}
if($failures.Count){$failures;exit 1}
"Runtime scenario static: PASS; guards=$mutations snapshot_fields=$($assigned.Count)"
