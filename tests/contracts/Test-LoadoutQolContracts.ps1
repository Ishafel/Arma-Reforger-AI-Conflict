param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$records = @(Get-AICFSourceRecords $RepositoryRoot)
$failures = [Collections.Generic.List[string]]::new()
$mutations = 0
function Body([string]$File,[string]$Method) {
    $record = @($records | Where-Object Name -eq $File)
    if ($record.Count -ne 1) { throw "Expected single source: $File" }
    $body = Get-AICFMethodBody $record[0] $Method
    $body = [regex]::Replace($body, '(?s)"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*.*?\*/', {
        param($token)
        if ($token.Value.StartsWith('"')) { return $token.Value }
        return ' '
    })
    $body -replace '\s+',' '
}
function Guard([string]$Id,[string]$Code,[string]$Pattern,[string]$Remove) {
    Assert-AICFContains $failures $Id $Code $Pattern 'Issue 66 contract'
    if (-not $Code.Contains($Remove)) { throw "Missing mutation: $Id" }
    if ($Code.Replace($Remove,'') -match $Pattern) { $failures.Add("Mutation escaped: $Id") }
    $script:mutations++
}
$request = Body 'AICF_LoadoutService.c' 'Request'
Guard 'QOL_SERVER' $request '!Replication.IsServer\(\)' '!Replication.IsServer()'
Guard 'QOL_CONTEXT' $request 'ResolveLoadoutContext\(player, slotId, member, slot, faction, source, role, targetFaction\)' 'targetFaction)'
Guard 'QOL_LIBRARY_REVISION' $request 'libraryOperation && libraryRevision != m_Store.GetRevision\(\)' 'libraryRevision != m_Store.GetRevision()'
Guard 'QOL_SLOT_REVISION' $request 'operation != 0 && revision != slot.GetLoadoutRevision\(\)' 'revision != slot.GetLoadoutRevision()'
Guard 'QOL_APPLY_NO_SAVE' $request 'binding && operation == 1\) accepted = slot.SetLoadout\(member, binding, revision\);' 'operation == 1'
Guard 'QOL_LOAD_VALIDATED' $request 'Validate\(loaded, faction, reason\)' 'Validate(loaded, faction, reason)'
Guard 'QOL_DELETE_SCOPE' $request 'preset.m_sProfile == recipe.m_sProfile && preset.m_sFaction == recipe.m_sFaction' 'preset.m_sFaction == recipe.m_sFaction'
Guard 'QOL_DELETE_CHARACTER' $request 'preset.m_sKind == "squad" \|\| preset.m_sCharacter == source' 'preset.m_sCharacter == source'
$save = Body 'AICF_LoadoutStore.c' 'SavePreset'
Guard 'QOL_DEDUPE' $save 'current.m_sData == preset.m_sData.*reason = string.Empty; return true;' 'current.m_sData == preset.m_sData'
Guard 'QOL_UPDATE_KEY' $save 'current.SameKey\(preset\).*return Write\(i, preset, reason\)' 'current.SameKey(preset)'
$write = Body 'AICF_LoadoutStore.c' 'Write'
Guard 'QOL_TWO_GENERATIONS' $write 'version % 2' 'version % 2'
Guard 'QOL_WRITE_READBACK' $write 'saved.Encode\(\) != preset.Encode\(\).*m_aTemplates\[id\] = saved; m_iRevision\+\+' 'saved.Encode() != preset.Encode()'
Guard 'QOL_DELETE_TOMBSTONE' (Body 'AICF_LoadoutStore.c' 'Delete') 'tombstone.m_bDeleted = true; return Write\(id, tombstone, reason\)' 'tombstone.m_bDeleted = true;'
Guard 'QOL_FILTER_TOMBSTONE' (Body 'AICF_LoadoutStore.c' 'GetPreset') 'm_aTemplates\[id\].m_bDeleted' 'm_aTemplates[id].m_bDeleted'
Guard 'QOL_RELOAD_NEWEST' (Body 'AICF_LoadoutStore.c' 'AICF_LoadoutStore') 'first.m_iVersion >= second.m_iVersion' 'first.m_iVersion >= second.m_iVersion'
Guard 'QOL_ALL_ROLES_COUNT' (Body 'AICF_LoadoutPreset.c' 'Decode') 'squad.m_aRecipes.Count\(\) != AICF_Stage1Config.MAX_GROUP_SIZE' 'squad.m_aRecipes.Count() != AICF_Stage1Config.MAX_GROUP_SIZE'
Guard 'QOL_ROLE_ALL_SOURCE' (Body 'AICF_LoadoutService.c' 'ApplyRole') 'source != recipe.m_sCharacter\) return false;.*batch.Commit\(\)' 'source != recipe.m_sCharacter'
Guard 'QOL_SQUAD_VALIDATE' (Body 'AICF_LoadoutService.c' 'ApplySquad') 'i < AICF_Stage1Config.MAX_GROUP_SIZE.*Validate\(recipe, faction, reason\).*if \(!binding\) return false;.*batch.Commit\(\)' 'if (!binding) return false;'
$commit = Body 'AICF_LoadoutPreset.c' 'Commit'
Guard 'QOL_ATOMIC_REVISION' $commit 'GetLoadoutRevision\(\) != m_aRevisions\[i\].*return false;.*for \(int j;' 'GetLoadoutRevision() != m_aRevisions[i]'
Guard 'QOL_ATOMIC_INVALID_RECIPE' $commit '!AICF_LoadoutRecipe.Decode\(m_aBindings\[i\].m_Recipe.Encode\(\)\).*return false;.*for \(int j;' '!AICF_LoadoutRecipe.Decode(m_aBindings[i].m_Recipe.Encode())'
$assign = Body 'AICF_GroupSlot.c' 'SetLoadout'
Guard 'QOL_BINDING_COPY' $assign 'assigned = new AICF_LoadoutBinding\(\).*assigned.m_iRevision = \+\+m_iLoadoutRevision;.*m_aLoadouts\[member\] = assigned' 'assigned = new AICF_LoadoutBinding()'
Guard 'QOL_NOOP_REVISION' $assign 'current.m_Recipe.Encode\(\) == binding.m_Recipe.Encode\(\).*return true;' 'current.m_Recipe.Encode() == binding.m_Recipe.Encode()'
$context = Body 'AICF_MatchController.c' 'ResolveLoadoutContext'
Guard 'QOL_ENEMY_ADMIN' $context '!SCR_Global.IsAdmin\(player.GetPlayerId\(\)\)' '!SCR_Global.IsAdmin(player.GetPlayerId())'
Guard 'QOL_ENEMY_ENUM' $context 'targetFaction != "US" && targetFaction != "USSR"' 'targetFaction != "USSR"'
Guard 'QOL_PERSONAL_RPC_SEPARATE' (Body 'AICF_LoadoutRpc.c' 'RpcAsk_AICFLoadout') 'slot == AICF_PersonalLoadout.SLOT && \(operation > 4 \|\| !targetFaction.IsEmpty\(\)\)' '!targetFaction.IsEmpty()'
Guard 'QOL_LIBRARY_ONLY_PRESERVES_DRAFT' (Body 'AICF_LoadoutEditor.c' 'Refresh') '!libraryOnly && m_Player.AICF_LoadoutAccepted\(\)' '!libraryOnly && '
Guard 'QOL_CLIENT_CANONICAL_COMPARE' (Body 'AICF_LoadoutEditor.c' 'Refresh') 'if \(m_Recipe\) m_sAppliedData = m_Recipe.Encode\(\)' 'm_sAppliedData = m_Recipe.Encode()'
Guard 'QOL_SIDE_DRAFT_FENCE' (Body 'AICF_LoadoutEditor.c' 'SelectSide') 'm_DraftMemory.Clear\(\); m_Recipe = null;' 'm_DraftMemory.Clear();'
Guard 'QOL_REVOKED_CLOSE' (Body 'AICF_LoadoutEditor.c' 'Refresh') '!SCR_Global.IsAdmin\(m_Player.GetPlayerId\(\)\)\) \{ Close\(\); return; \}' '!SCR_Global.IsAdmin(m_Player.GetPlayerId())'
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
Write-Output "PASS Loadout QoL contracts; mutations=$mutations; runtime=NOT_RUN"
