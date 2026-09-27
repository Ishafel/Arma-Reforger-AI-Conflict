param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$files = @{}
foreach ($name in @('Respawn/AICF_SquadRespawnPolicy.c','Respawn/AICF_SquadRespawnRpc.c','Respawn/AICF_SquadRespawnHandler.c','UI/AICF_SquadRespawnUI.c')) {
    $files[$name] = [IO.File]::ReadAllText((Join-Path $core $name))
}
$rules = @(
    @('Respawn/AICF_SquadRespawnPolicy.c','\[BaseContainerProps\(category: "Respawn"\)\]\s*modded class SCR_SpawnLogic','NATIVE_LOGIC_REGISTRATION'),
    @('Respawn/AICF_SquadRespawnPolicy.c','Replication.IsServer\(\) && AICF_MatchController.GetActiveController\(\)','AUTHORITY'),
    @('Respawn/AICF_SquadRespawnPolicy.c','campaign.IsMaster\(\) && campaign.IsRunning\(\)','MATCH'),
    @('Respawn/AICF_SquadRespawnPolicy.c','!group.IsPlayerInGroup\(player.GetPlayerId\(\)\)','MEMBERSHIP'),
    @('Respawn/AICF_SquadRespawnPolicy.c','group.GetFaction\(\) != SCR_FactionManager.SGetPlayerFaction','PLAYER_FACTION'),
    @('Respawn/AICF_SquadRespawnPolicy.c','SCR_Faction.GetEntityFaction\(entity\) != group.GetFaction\(\)','AI_FACTION'),
    @('Respawn/AICF_SquadRespawnPolicy.c','!AICF_GroupRuntime.IsAliveCharacter\(entity\)','TARGET_ALIVE'),
    @('Respawn/AICF_SquadRespawnPolicy.c','players.GetPlayerIdFromControlledEntity\(entity\) != 0','OCCUPIED'),
    @('Respawn/AICF_SquadRespawnPolicy.c','SCR_PossessingManagerComponent.GetPlayerIdFromMainEntity\(entity\) != 0','MAIN_ENTITY'),
    @('Respawn/AICF_SquadRespawnPolicy.c','agent.GetControlledEntity\(\) == entity','ACTUAL_AGENT'),
    @('Respawn/AICF_SquadRespawnPolicy.c','slave.GetMaster\(\) != group','SLAVE_PARENT'),
    @('Respawn/AICF_SquadRespawnPolicy.c','slave.GetFaction\(\) != group.GetFaction\(\)','SLAVE_FACTION'),
    @('Respawn/AICF_SquadRespawnRpc.c','AICF_SquadRespawnPolicy.GetSquadAgents\(group, agents\)','SLAVE_LIST'),
    @('Respawn/AICF_SquadRespawnPolicy.c','MemberGroup\(attempt.m_Group, entity\) == attempt.m_MemberGroup','MEMBER_GROUP_REVALIDATION'),
    @('Respawn/AICF_SquadRespawnPolicy.c','entity.GetID\(\) == attempt.m_CharacterId','ENTITY_IDENTITY'),
    @('Respawn/AICF_SquadRespawnPolicy.c','attempt.m_Group.GetID\(\) == attempt.m_GroupId','GROUP_IDENTITY'),
    @('Respawn/AICF_SquadRespawnPolicy.c','player.AICF_GetSquadDeathRevision\(\) == attempt.m_iDeathRevision','DEATH_GENERATION'),
    @('Respawn/AICF_SquadRespawnRpc.c','m_bAICFSquadDeath && !AICF_GroupRuntime.IsAliveCharacter\(GetControlledEntity\(\)\)','DEAD_PLAYER'),
    @('Respawn/AICF_SquadRespawnRpc.c','deathRevision != m_iAICFSquadDeathRevision','STALE_UI'),
    @('Respawn/AICF_SquadRespawnRpc.c','respawn.RequestSpawn\(SCR_PossessSpawnData.FromRplId\(character\)\)','NATIVE_POSSESSION'),
    @('Respawn/AICF_SquadRespawnHandler.c','GetRespawnSystemComponent\(\).CanRequestSpawn_S\(requestComponent, this, data, result\)','NATIVE_TIMER'),
    @('Respawn/AICF_SquadRespawnHandler.c','(?s)AssignEntity_S\([^}]+IsCurrent\([^}]+return false;[^}]+super.AssignEntity_S','FINAL_REVALIDATION'),
    @('Respawn/AICF_SquadRespawnHandler.c','(?s)SendResponse_S\([^}]+AICF_FinishSquadRespawn\(response\);[^}]+super.SendResponse_S','REQUEST_CLEANUP'),
    @('UI/AICF_SquadRespawnUI.c','button.m_OnClicked.Remove\(AICF_OnCharacterClicked\)','UI_CLEANUP'),
    @('UI/AICF_SquadRespawnUI.c','CreateWidgets\(m_sLoadoutButton, GetContentRoot\(\)\)','NATIVE_CARD'),
    @('UI/AICF_SquadRespawnUI.c','AddItem\(card\)','NATIVE_GALLERY'),
    @('UI/AICF_SquadRespawnUI.c','info.SetIconTo\(GetImageWidget\(\)\)','CARD_ICON'),
    @('UI/AICF_SquadRespawnUI.c','manager.SetPreviewItem\(preview, entity\)','LIVE_CHARACTER_PREVIEW'),
    @('UI/AICF_SquadRespawnUI.c','override protected void RequestRespawn\(\)','NATIVE_DEPLOY_ACTION'),
    @('UI/AICF_SquadRespawnUI.c','m_iAICFSelectedDeathRevision != player.m_iAICFSquadListDeathRevision','UI_STALE_SELECTION')
)
$failures = @()
foreach ($rule in $rules) {
    if ($files[$rule[0]] -notmatch $rule[1]) { $failures += $rule[2] }
    $negative = [regex]::Replace($files[$rule[0]], $rule[1], '')
    if ($negative -match $rule[1]) { $failures += "MUTATION_ESCAPED:$($rule[2])" }
}
$production = ($files.Values -join "`n")
if ($production -match 'SpawnEntityPrefab|SetOrigin\(|SetHealth\(|SetInitialMainEntity\(') { $failures += 'BYPASS_NATIVE_PIPELINE' }
if ($files['UI/AICF_SquadRespawnUI.c'] -match 'CreateRect|FrameSlot|SCR_ComboBoxComponent') { $failures += 'SEPARATE_RESPAWN_PANEL' }
if ($files['Respawn/AICF_SquadRespawnRpc.c'] -match '(?s)void AICF_RecordSquadDeath\(\)[^}]*m_AICFSquadRespawn = null') { $failures += 'LOST_PENDING_IDENTITY' }
if (Test-Path (Join-Path $core 'Respawn/AICF_SquadRespawnProbe.c')) { $failures += 'FIXTURE_IN_PRODUCTION' }
if ($failures.Count) { Write-Output "Squad respawn contracts: FAIL $($failures -join ', ')"; exit 1 }
Write-Output "Squad respawn contracts: PASS; negative mutations=$($rules.Count); runtime=NOT_RUN"
