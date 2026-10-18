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
}.GetEnumerator()) { $sources[$entry.Key]=ConvertTo-AICFCodeText ([IO.File]::ReadAllText((Join-Path $core $entry.Value))) }
function Test-CoopContracts($s) {
    if($s.access -match 'class SCR_Faction\b|override bool IsPlayable\(') { 'NATIVE_FACTION_SETUP' }
    foreach($rule in @(
        @('rules','if \(factionKey == GetPlayerOnlyFaction\(\)\) return 0;\s*return AICF_Stage1Config.GROUP_SLOTS_PER_FACTION;','ROSTER_POLICY'),
        @('rules','bool UsesTicketDefeat.*?return GetManagedSlotCount\(factionKey\) > 0;','TICKET_POLICY'),
        @('rules','bool AllowsPlayerFaction.*?if \(!IsValid\(\)\) return false;.*?if \(m_eMode == AICF_EMatchMode.STANDARD\) return true;.*?GetStableFactionKey\(factionKey\) == GetPlayerOnlyFaction\(\)','FACTION_POLICY'),
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
