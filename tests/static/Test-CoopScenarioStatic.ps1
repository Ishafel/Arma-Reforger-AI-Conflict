param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources=@{}
foreach($entry in @{
    rules='Config/AICF_MatchRules.c'; config='Config/AICF_Stage1Config.c'; state='State/AICF_FactionState.c'
    controller='Bootstrap/AICF_MatchController.c'; campaign='Integration/AICF_CampaignState.c'
    ui='UI/AICF_StrategicUI.c'; victory='Victory/AICF_VictorySystem.c'
    access='Integration/AICF_CoopFactionAccess.c'; construction='Construction/AICF_ConstructionPlanner.c'
    factionMenu='UI/AICF_CoopFactionMenu.c'
    playerGroups='Integration/AICF_CoopPlayerGroups.c'
    recruitment='Integration/AICF_BarracksRecruitment.c'
}.GetEnumerator()) { $sources[$entry.Key]=ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $core $entry.Value))) }
function Test-CoopContracts($s) {
    if($s.access -match 'class SCR_Faction\b|override bool IsPlayable\(') { 'NATIVE_FACTION_SETUP' }
    if($s.recruitment -match 'SetRecruited\(|SetMaxAIPerGroup\(|RequestAddAIAgent\(|override bool IsRecruitable\(') { 'NATIVE_RECRUIT_RULES' }
    if($s.recruitment -match 'GetMode\(\)|AICF_EMatchMode') { 'PURCHASE_ALL_MODES' }
    foreach($rule in @(
        @('rules','if \(factionKey == GetPlayerOnlyFaction\(\)\) return 0;\s*return AICF_Stage1Config.GROUP_SLOTS_PER_FACTION;','ROSTER_POLICY'),
        @('rules','bool UsesTicketDefeat.*?return GetManagedSlotCount\(factionKey\) > 0;','TICKET_POLICY'),
        @('rules','bool AllowsPlayerFaction.*?if \(!IsValid\(\)\) return false;.*?if \(m_eMode == AICF_EMatchMode.STANDARD\) return true;.*?GetStableFactionKey\(factionKey\) == GetPlayerOnlyFaction\(\)','FACTION_POLICY'),
        @('factionMenu','modded class SCR_WelcomeScreenFactionContent.*?int previousCount = m_aFactionWidgets.Count\(\);\s*super.FillFactionWidget\(faction, content, color\);.*?AllowsPlayerFaction\(faction.GetFactionKey\(\)\).*?m_aFactionWidgets.Count\(\) == previousCount\) return;.*?Widget factionWidget = m_aFactionWidgets\[previousCount\];\s*factionWidget.SetVisible\(false\);\s*factionWidget.SetEnabled\(false\);','WELCOME_FACTIONS'),
        @('playerGroups','super.OnPlayerFactionChanged\(owner, previousFaction, newFaction\);.*?!Replication.IsServer\(\).*?!GetGameMode\(\).IsMaster\(\).*?rules.GetMode\(\) == AICF_EMatchMode.STANDARD.*?CallLater\(AICF_EnsureCoopPlayerGroup, 0, false, owner, newFaction\);','GROUP_JOIN_BOUNDARY'),
        @('playerGroups','owner.GetAffiliatedFaction\(\) != faction.*?GetPlayerController\(player.GetPlayerId\(\)\) != player.*?factions.GetPlayerFaction\(player.GetPlayerId\(\)\) != faction.*?group.IsPlayerInGroup\(player.GetPlayerId\(\)\)\) return;.*?component.CreateAndJoinGroup\(faction, true\);','GROUP_JOIN_IDENTITY'),
        @('playerGroups','OnGameEnd\(\).*?Remove\(AICF_EnsureCoopPlayerGroup\);.*?super.OnGameEnd\(\);','GROUP_JOIN_CLEANUP'),
        @('playerGroups','IsGroupMenuAllowed\(\).*?GetMode\(\) != AICF_EMatchMode.STANDARD\) return true;\s*return super.IsGroupMenuAllowed\(\);','GROUP_MENU_ACCESS'),
        @('playerGroups','IsGroupLeaderVolunteeringAllowed\(\).*?GetMode\(\) != AICF_EMatchMode.STANDARD\) return true;\s*return super.IsGroupLeaderVolunteeringAllowed\(\);','GROUP_LEADER_ACCESS'),
        @('recruitment','super.OnEntityRequested\(spawnedEntity, user, faction, service\);\s*if \(!Replication.IsServer\(\) \|\| !IsMaster\(\) \|\| !user\) return;.*?if \(!rules.AllowsPlayerFaction\(requestedFaction.GetFactionKey\(\)\)\) return;','PURCHASE_AUTHORITY'),
        @('recruitment','if \(!requestedFaction \|\| \(faction && faction != requestedFaction\)\) return;.*?if \(!provider \|\| !provider.CanCommandAI\(\)\) return;.*?playerId <= 0 \|\| players.GetPlayerControlledEntity\(playerId\) != user.*?factions.GetPlayerFaction\(playerId\) != requestedFaction \|\| SCR_Faction.GetEntityFaction\(user\) != requestedFaction.*?group.AICF_AllowPurchasedRecruitment\(requestedFaction\);','PURCHASE_IDENTITY'),
        @('recruitment','GetAgents\(agents\);\s*foreach \(AIAgent agent : agents\) AICF_AllowPurchasedMember\(agent\);.*?OnAgentAdded\(AIAgent child\).*?super.OnAgentAdded\(child\);\s*AICF_AllowPurchasedMember\(child\);','PURCHASE_ASYNC_MEMBERS'),
        @('recruitment','m_sAICFPurchasedRecruitFaction.IsEmpty\(\) \|\| !agent \|\| agent.GetParentGroup\(\) != this.*?!mode.IsMaster\(\).*?!rules.AllowsPlayerFaction\(m_sAICFPurchasedRecruitFaction\).*?GetFaction\(\) != faction.*?character.GetFaction\(\) != faction.*?GetPlayerIdFromControlledEntity\(character\) != 0','PURCHASE_MEMBER_FENCE'),
        @('recruitment','if \(!character.IsRecruitable\(\)\) character.SetRecruitable\(true\);','PURCHASE_CHANGED_ONLY'),
        @('factionMenu','!factionButton.GetFaction\(\) \|\| rules.AllowsPlayerFaction\(factionButton.GetFaction\(\).GetFactionKey\(\)\)\) continue;.*?SetVisible\(false, false\);.*?m_aActiveFactionButtons.RemoveItem\(factionButton\);','FACTION_MENU'),
        @('access','CanRequestFaction_S\(Faction faction\).*?if \(faction && !AICF_MatchRules.FromMissionHeader\(\).AllowsPlayerFaction\(faction.GetFactionKey\(\)\)\) return false;.*?super.CanRequestFaction_S\(faction\)','FACTION_REQUEST'),
        @('access','SetFaction_S\(Faction faction\).*?if \(faction && !AICF_MatchRules.FromMissionHeader\(\).AllowsPlayerFaction\(faction.GetFactionKey\(\)\)\) return false;.*?super.SetFaction_S\(faction\)','FACTION_ASSIGNMENT'),
        @('construction','Commander\(SCR_CampaignFaction faction\).*?!m_MatchRules.AllowsAutomaticConstruction\(faction.GetFactionKey\(\)\)\)\s*return null;','CONSTRUCTION_POLICY'),
        @('config','m_MatchRules.GetMode\(\) == AICF_EMatchMode.STANDARD \|\| m_sAICommanderMode == m_MatchRules.GetAICommanderMode\(\)','COMMANDER_CONFLICT'),
        @('config','ClampInt\(value, GetMinimumManagedAgents\(\), MAX_MANAGED_AGENTS\)','BUDGET'),
        @('state','m_iExpectedSlotCount = resolvedConfig.GetMatchRules\(\).GetManagedSlotCount\(factionKey\);','SLOT_POLICY'),
        @('state','slotId < m_iExpectedSlotCount','SLOT_CREATION'),
        @('state','m_aGroupSlots.Count\(\) != m_iExpectedSlotCount','QUEUE_SHAPE'),
        @('controller','GetExpectedSlotCount\(\) == 0\)\s*return factionState.GetSlotCount\(\) == 0;','EMPTY_SPAWN'),
        @('controller','m_USSRState.CountSlotsByState\(AICF_EGroupSlotState.READY\) != m_USSRState.GetExpectedSlotCount\(\)','READY_GATE'),
        @('campaign','\[RplProp\(\)\]\s*protected AICF_EMatchMode m_eAICFMatchMode;','JIP'),
        @('campaign','if \(!Replication.IsServer\(\) \|\| !IsMaster\(\)\) return;.*?if \(m_eAICFMatchMode == mode\) return;\s*m_eAICFMatchMode = mode;\s*Replication.BumpMe\(\);','AUTHORITY'),
        @('ui','AICF_IsPlayerOnlyFaction\(m_bLocalUSSR\)\)\s*return;\s*m_wMapToggle =','COMMAND_UI'),
        @('victory','ussrState.UsesTicketDefeat\(\) && ussrState.GetTickets\(\) <= 0','TICKET_VICTORY'),
        @('victory','loserState.UsesTicketDefeat\(\) && loserState.GetTickets\(\) <= 0','VICTORY_REASON')
    )) { if($s[$rule[0]] -notmatch ('(?s)'+$rule[1])) { $rule[2] } }
}
$failures=@(Test-CoopContracts $sources)
$mutations=@(
    @('rules','if (factionKey == GetPlayerOnlyFaction()) return 0;','','ROSTER_POLICY'),
    @('rules','return GetManagedSlotCount(factionKey) > 0;','return true;','TICKET_POLICY'),
    @('rules','GetStableFactionKey(factionKey) == GetPlayerOnlyFaction()','true','FACTION_POLICY'),
    @('factionMenu','super.FillFactionWidget(faction, content, color);','','WELCOME_FACTIONS'),
    @('factionMenu','factionWidget.SetVisible(false);','factionWidget.SetVisible(true);','WELCOME_FACTIONS'),
    @('factionMenu','m_aFactionWidgets[previousCount]','m_aFactionWidgets[0]','WELCOME_FACTIONS'),
    @('playerGroups','!Replication.IsServer()','false','GROUP_JOIN_BOUNDARY'),
    @('playerGroups','rules.GetMode() == AICF_EMatchMode.STANDARD','false','GROUP_JOIN_BOUNDARY'),
    @('playerGroups','owner.GetAffiliatedFaction() != faction','false','GROUP_JOIN_IDENTITY'),
    @('playerGroups','group.IsPlayerInGroup(player.GetPlayerId())','true','GROUP_JOIN_IDENTITY'),
    @('playerGroups','GetGame().GetCallqueue().Remove(AICF_EnsureCoopPlayerGroup);','','GROUP_JOIN_CLEANUP'),
    @('playerGroups','return super.IsGroupMenuAllowed();','return true;','GROUP_MENU_ACCESS'),
    @('playerGroups','return super.IsGroupLeaderVolunteeringAllowed();','return true;','GROUP_LEADER_ACCESS'),
    @('recruitment','!Replication.IsServer() || !IsMaster() || !user','false','PURCHASE_AUTHORITY'),
    @('recruitment','if (!rules.AllowsPlayerFaction(requestedFaction.GetFactionKey()))','if (rules.GetMode() == AICF_EMatchMode.STANDARD || !rules.AllowsPlayerFaction(requestedFaction.GetFactionKey()))','PURCHASE_ALL_MODES'),
    @('recruitment','!provider.CanCommandAI()','false','PURCHASE_IDENTITY'),
    @('recruitment','players.GetPlayerControlledEntity(playerId) != user','false','PURCHASE_IDENTITY'),
    @('recruitment','AICF_AllowPurchasedMember(child);','','PURCHASE_ASYNC_MEMBERS'),
    @('recruitment','m_sAICFPurchasedRecruitFaction.IsEmpty()','false','PURCHASE_MEMBER_FENCE'),
    @('recruitment','agent.GetParentGroup() != this','false','PURCHASE_MEMBER_FENCE'),
    @('recruitment','GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) != 0','false','PURCHASE_MEMBER_FENCE'),
    @('recruitment','if (!character.IsRecruitable()) character.SetRecruitable(true);','character.SetRecruitable(true);','PURCHASE_CHANGED_ONLY'),
    @('recruitment','character.SetRecruitable(true);','character.SetRecruitable(true); character.SetRecruited(true);','NATIVE_RECRUIT_RULES'),
    @('factionMenu','m_aActiveFactionButtons.RemoveItem(factionButton);','','FACTION_MENU'),
    @('access','return super.CanRequestFaction_S(faction);','return true;','FACTION_REQUEST'),
    @('access','return super.SetFaction_S(faction);','return true;','FACTION_ASSIGNMENT'),
    @('access','modded class SCR_PlayerFactionAffiliationComponent','modded class SCR_Faction { override bool IsPlayable() { return false; } } modded class SCR_PlayerFactionAffiliationComponent','NATIVE_FACTION_SETUP'),
    @('construction','!m_MatchRules.AllowsAutomaticConstruction(faction.GetFactionKey())','false','CONSTRUCTION_POLICY'),
    @('config','m_sAICommanderMode == m_MatchRules.GetAICommanderMode()','true','COMMANDER_CONFLICT'),
    @('config','ClampInt(value, GetMinimumManagedAgents(), MAX_MANAGED_AGENTS)','ClampInt(value, MIN_MANAGED_AGENTS, MAX_MANAGED_AGENTS)','BUDGET'),
    @('state','slotId < m_iExpectedSlotCount','slotId < AICF_Stage1Config.GROUP_SLOTS_PER_FACTION','SLOT_CREATION'),
    @('state','m_aGroupSlots.Count() != m_iExpectedSlotCount','false','QUEUE_SHAPE'),
    @('controller','return factionState.GetSlotCount() == 0;','return false;','EMPTY_SPAWN'),
    @('controller','m_USSRState.GetExpectedSlotCount()','AICF_Stage1Config.GROUP_SLOTS_PER_FACTION','READY_GATE'),
    @('campaign','[RplProp()]','/* removed */','JIP'),
    @('campaign','if (m_eAICFMatchMode == mode) return;','','AUTHORITY'),
    @('ui','if (m_Campaign.AICF_IsPlayerOnlyFaction(m_bLocalUSSR))','if (false)','COMMAND_UI'),
    @('victory','ussrState.UsesTicketDefeat() && ','','TICKET_VICTORY'),
    @('victory','loserState.UsesTicketDefeat() && ','','VICTORY_REASON')
)
foreach($mutation in $mutations) {
    $copy=$sources.Clone()
    if(!$copy[$mutation[0]].Contains($mutation[1])) { throw "Missing mutation: $($mutation[3])" }
    $copy[$mutation[0]]=$copy[$mutation[0]].Replace($mutation[1],$mutation[2])
    if($mutation[3] -notin @(Test-CoopContracts $copy)) { $failures+="MUTATION_ESCAPED $($mutation[3])" }
}
$header=Get-Content (Join-Path $RepositoryRoot 'AIConflictArland/Missions/AICF_Conflict_Arland_Coop_USSR.conf') -Raw
if($header -notmatch '\{BC2437E4861B4FD2\}Missions/AICF_Conflict_Arland.conf' -or $header -notmatch 'm_eAICFMatchMode 2') { $failures+='HEADER_POLICY' }
if($header -match '(?m)^\s*(World|SystemsConfig|m_aCampaignCustomBaseList)\b') { $failures+='HEADER_OWNERSHIP' }
if($failures.Count) { $failures; exit 1 }
"Co-op static: PASS; negative mutations=$($mutations.Count)"
