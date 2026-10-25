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
Guard 'QOL_SOURCE_WHITELIST' (Body 'AICF_LoadoutService.c' 'KnownSource') 'recipe.m_sProfile != AICF_ContentProfile.GetActive\(\).GetProfileKey\(\).*ResolveLoadoutContext\(player, slotId, i.*source == recipe.m_sCharacter' 'source == recipe.m_sCharacter'
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
Guard 'QOL_ROLE_ALL_SOURCE' (Body 'AICF_LoadoutService.c' 'ApplyRole') '!AICF_LoadoutTransfer.Fits\(binding, faction, source, reason\)\) return false;.*batch.Commit\(\)' '!AICF_LoadoutTransfer.Fits(binding, faction, source, reason)'
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
Guard 'QOL_PERSONAL_RPC_SEPARATE' (Body 'AICF_LoadoutRpc.c' 'RpcAsk_AICFLoadout') 'slot == AICF_PersonalLoadout.SLOT && !targetFaction.IsEmpty\(\)' '!targetFaction.IsEmpty()'
Guard 'QOL_LIBRARY_ONLY_PRESERVES_DRAFT' (Body 'AICF_LoadoutEditor.c' 'Refresh') '!libraryOnly && m_Player.AICF_LoadoutAccepted\(\)' '!libraryOnly && '
Guard 'QOL_CLIENT_CANONICAL_COMPARE' (Body 'AICF_LoadoutEditor.c' 'Refresh') 'if \(m_Recipe\) m_sAppliedData = m_Recipe.Encode\(\)' 'm_sAppliedData = m_Recipe.Encode()'
Guard 'QOL_SIDE_DRAFT_FENCE' (Body 'AICF_LoadoutEditor.c' 'SelectSide') 'm_DraftMemory.Clear\(\); m_Recipe = null;' 'm_DraftMemory.Clear();'
Guard 'QOL_REVOKED_CLOSE' (Body 'AICF_LoadoutEditor.c' 'Refresh') '!SCR_Global.IsAdmin\(m_Player.GetPlayerId\(\)\)\) \{ Close\(\); return; \}' '!SCR_Global.IsAdmin(m_Player.GetPlayerId())'
Guard 'QOL_INSPECTOR_ENTRY' (Body 'AICF_LoadoutEditor.c' 'Create') 'return CreateInspector\(parent\)' 'return CreateInspector(parent)'
Guard 'QOL_ASSIGNED_NAMES_BINDING' (Body 'AICF_LoadoutStore.c' 'List') 'binding = slot.GetLoadout\(member\).*name = binding.m_Recipe.m_sName;.*assignedNames.Insert\(name\)' 'name = binding.m_Recipe.m_sName;'
Guard 'QOL_ASSIGNED_NAMES_SCOPE' (Body 'AICF_LoadoutEditor.c' 'Library') 'm_Player.AICF_LoadoutSlot\(\) != m_iSlotId.*m_Player.AICF_LoadoutFaction\(\) != m_sTargetFaction.*m_aAssignedNames.Clear\(\)' 'm_Player.AICF_LoadoutFaction() != m_sTargetFaction'
Guard 'QOL_CONFIRM_LIBRARY_IDENTITY' (Body 'AICF_LoadoutEditor.c' 'OnClick') 'libraryOperation && m_iConfirmLibraryRevision != m_iLibraryRevision' 'm_iConfirmLibraryRevision != m_iLibraryRevision'
Guard 'QOL_CONFIRM_SLOT_IDENTITY' (Body 'AICF_LoadoutEditor.c' 'OnClick') 'm_Player.AICF_LoadoutRevision\(\) != m_iConfirmSlotRevision' 'm_Player.AICF_LoadoutRevision() != m_iConfirmSlotRevision'
Guard 'QOL_MODAL_BACK' (Body 'AICF_LoadoutEditor.c' 'OnBack') 'm_iModal != 0\) \{ HideModal\(\); return;' 'HideModal();'
Guard 'QOL_OWNER_VIEW_EDITOR_ARBITRATION' (Body 'AICF_PersonalLoadoutUI.c' 'Update') 'AICF_LoadoutEditor.IsAIEditorOpen\(\).*ClearPreview\(\); return;' 'AICF_LoadoutEditor.IsAIEditorOpen()'
Guard 'QOL_MODAL_INPUT_LAYER' (Body 'AICF_LoadoutEditor.c' 'ModalSection') 'CreateRect\(panel,.*true\).*section.SetZOrder\(10\)' 'section.SetZOrder(10)'
Guard 'QOL_NATIVE_MODAL_REGISTER' (Body 'AICF_LoadoutEditor.c' 'ShowModal') 'AddModal\(m_wModal, focus\)' 'AddModal(m_wModal, focus)'
Guard 'QOL_NATIVE_MODAL_REMOVE' (Body 'AICF_LoadoutEditor.c' 'HideModal') 'RemoveModal\(m_wModal\).*m_wModal.SetVisible\(false\)' 'RemoveModal(m_wModal)'
Guard 'QOL_MODAL_LIFECYCLE' (Body 'AICF_LoadoutEditor.c' 'Close') 'HideModal\(\)' 'HideModal();'
Guard 'QOL_EMPTY_SIDE_PRESERVES_DRAFT' (Body 'AICF_LoadoutEditor.c' 'SelectSide') '!HasLoadoutGroups\(key\).*m_Side.SetCurrentItem\(previous, false, false, false\).*return;.*CancelSearch\(\)' '!HasLoadoutGroups(key)'
Guard 'QOL_SCROLL_VERTICAL' (Body 'AICF_LoadoutItemGrid.c' 'Create') 'm_Scrollbar.SetVertical\(true\)' 'm_Scrollbar.SetVertical(true)'
Guard 'QOL_SCROLL_CALLBACK_FENCE' (Body 'AICF_LoadoutItemGrid.c' 'OnChange') 'm_bScrollUpdating \|\| !m_bEnabled' 'm_bScrollUpdating || '
Guard 'QOL_SCROLL_INPUT_CLEANUP' (Body 'AICF_LoadoutItemGrid.c' 'Close') 'm_Scrollbar.RemoveHandler\(this\)' 'm_Scrollbar.RemoveHandler(this)'
Guard 'QOL_INSPECT_SEPARATE_DRAFT' (Body 'AICF_LoadoutEditor.c' 'Refresh') 'm_iOperation == 8.*ReceiveInspection\(status\);.*return;.*m_Recipe = AICF_LoadoutRecipe.Decode' 'ReceiveInspection(status);'
Guard 'QOL_INSPECT_SELECTION' (Body 'AICF_LoadoutEditor.c' 'ReceiveInspection') 'm_iModal != 1.*m_aTemplateIds\[index\] != m_iInspectId' 'm_aTemplateIds[index] != m_iInspectId'
Guard 'QOL_INSPECT_REVISION' (Body 'AICF_LoadoutEditor.c' 'ReceiveInspection') 'revision != m_iInspectLibraryRevision \|\| m_Player.AICF_LoadoutRevision\(\) != m_iRevision' 'revision != m_iInspectLibraryRevision'
Guard 'QOL_INSPECT_SERVER_CONTEXT' (Body 'AICF_LoadoutService.c' 'Request') 'ResolveLoadoutContext\(player, slotId, inspectMember.*inspectSlot == slot && inspectFaction == faction' 'inspectSlot == slot && inspectFaction == faction'
Guard 'QOL_INSPECT_SOURCE' (Body 'AICF_LoadoutService.c' 'Request') 'loaded && KnownSource\(player, slotId, loaded\).*Validate\(loaded, faction, reason\).*AICF_LoadoutTransfer.Fits\(inspected, faction, inspectSource, reason\)' 'KnownSource(player, slotId, loaded)'
Guard 'QOL_INSPECT_LIST_CONTEXT' (Body 'AICF_LoadoutService.c' 'Request') 'operation != 8\) libraryContext = recipe;.*m_Store.List\(libraryContext, slot\)' 'operation != 8'
Guard 'QOL_LIBRARY_DETACH_WORLD' (Body 'AICF_LoadoutLibraryView.c' 'Clear') 'SetWorld\(null, 0\).*m_Preview.Clear\(\).*m_Draft.Clear\(\)' 'SetWorld(null, 0)'
Guard 'QOL_SUMMARY_DEDUPLICATION' (Body 'AICF_LoadoutSummary.c' 'Collect') 'seen.Contains\(child\).*continue;.*seen.Insert\(child\)' 'seen.Contains(child)'
Guard 'QOL_SUMMARY_NESTED' (Body 'AICF_LoadoutSummary.c' 'Collect') 'counts.Set\(key, count \+ 1\).*Collect\(child, catalog, lines, seen, counts, names, depth \+ 1\)' 'Collect(child, catalog, lines, seen, counts, names, depth + 1)'
Guard 'QOL_SUMMARY_SCROLL_FENCE' (Body 'AICF_LoadoutLibraryView.c' 'OnChange') 'w != m_wScroll \|\| m_bUpdating' 'm_bUpdating'
Guard 'QOL_HOVER_ENABLED_ONLY' (Body 'AICF_LoadoutEditor.c' 'OnMouseEnter') 'm_aButtons.Contains\(w\) && w.IsEnabledInHierarchy\(\)' 'w.IsEnabledInHierarchy()'
Guard 'QOL_PRESS_RESET' (Body 'AICF_LoadoutEditor.c' 'OnMouseLeave') 'm_wHoveredButton = null; m_wPressedButton = null;' 'm_wPressedButton = null;'
Guard 'QOL_INSPECT_CALLBACK_CANCEL' (Body 'AICF_LoadoutEditor.c' 'HideModal') 'Remove\(SendInspection\)' 'Remove(SendInspection)'
Guard 'QOL_INSPECT_CALLBACK_IDENTITY' (Body 'AICF_LoadoutEditor.c' 'SendInspection') 'm_iModal != 1.*m_iInspectLibraryRevision != m_iLibraryRevision.*m_aTemplateIds\[index\] != m_iInspectId' 'm_iInspectLibraryRevision != m_iLibraryRevision'
Guard 'QOL_TRANSFER_SERVER' (Body 'AICF_LoadoutTransfer.c' 'Fits') '!Replication.IsServer\(\)' '!Replication.IsServer()'
Guard 'QOL_TRANSFER_FULL_READBACK' (Body 'AICF_LoadoutTransfer.c' 'Fits') 'probe.m_sCharacter = target;.*Restore\(draft.GetCharacter\(\), binding.m_sInventory\).*Signature\(draft.GetCharacter\(\), catalog, cost\) == binding.m_sSignature' '== binding.m_sSignature'
Guard 'QOL_CURRENT_SQUAD_ATOMIC' (Body 'AICF_LoadoutService.c' 'ApplyMembers') 'i < AICF_Stage1Config.MAX_GROUP_SIZE.*ResolveLoadoutContext\(player, slotId, i.*!AICF_LoadoutTransfer.Fits.*return false;.*batch.Add.*batch.Commit' 'batch.Commit()'
Guard 'QOL_QUEUE_COOLDOWN' (Body 'AICF_LoadoutEditor.c' 'Request') 'm_iToken = 0; m_bPending = true;.*CallLater\(DispatchRequest, Math.Max\(1, 1000 - System.GetTickCount\(m_iLastRequestSentAt\)\)' '1000 - System.GetTickCount(m_iLastRequestSentAt)'
Guard 'QOL_QUEUE_IDENTITY' (Body 'AICF_LoadoutEditor.c' 'DispatchRequest') 'm_iRequestSlot != m_iSlotId \|\| m_iRequestMember != m_iMember' 'm_iRequestMember != m_iMember'
Guard 'QOL_QUEUE_NO_STALE_TOKEN' (Body 'AICF_LoadoutEditor.c' 'Refresh') 'm_bPending && m_iToken > 0 && m_Player.AICF_LoadoutToken\(\) == m_iToken' 'm_iToken > 0 && '
Guard 'QOL_QUEUE_BOUNDED_RETRY' (Body 'AICF_LoadoutEditor.c' 'Refresh') 'status == "RATE_LIMITED" && m_iRequestRetries < 2.*m_iToken = 0;.*CallLater\(DispatchRequest, 1000, false\)' 'm_iRequestRetries < 2'
Guard 'QOL_QUEUE_CLOSE' (Body 'AICF_LoadoutEditor.c' 'Close') 'Remove\(DispatchRequest\)' 'Remove(DispatchRequest)'
Guard 'QOL_SUMMARY_VISIBLE' (Body 'AICF_LoadoutSummary.c' 'Collect') 'attributes && !attributes.IsVisible\(component\)' '!attributes.IsVisible(component)'
Guard 'QOL_SUMMARY_HIDDEN_CONTENTS' (Body 'AICF_LoadoutSummary.c' 'Collect') '!attributes.IsVisible\(component\).*seen.Insert\(child\); Collect\(child, catalog, lines, seen, counts, names, depth \+ 1\);.*continue;' 'Collect(child, catalog, lines, seen, counts, names, depth + 1);'
Guard 'QOL_SUMMARY_LOCALIZED' (Body 'AICF_LoadoutSummary.c' 'Collect') 'AICF_Localization.Resolve\(catalog.EntityName\(child\)\)' 'AICF_Localization.Resolve(catalog.EntityName(child))'
Guard 'QOL_PRECLEAR_PRIVATE_WORLD' (Body 'AICF_LoadoutInventory.c' 'PrepareRestoreLocal') 'entity.GetWorld\(\) == GetGame\(\).GetWorld\(\)' 'entity.GetWorld() == GetGame().GetWorld()'
Guard 'QOL_PRECLEAR_RECURSIVE' (Body 'AICF_LoadoutInventory.c' 'PrepareRestoreLocal') 'GetPrefabName\(item\) == prefab && !PrepareRestoreLocal\(item, context, depth \+ 1\)' '!PrepareRestoreLocal(item, context, depth + 1)'
Guard 'QOL_CARGO_PACK_PRIVATE' (Body 'AICF_LoadoutInventory.c' 'PackCargoLocal') 'entity.GetWorld\(\) == GetGame\(\).GetWorld\(\)' 'entity.GetWorld() == GetGame().GetWorld()'
Guard 'QOL_CARGO_PACK_ORDER' (Body 'AICF_LoadoutInventory.c' 'PackCargoLocal') 'SCR_UniversalInventoryStorageComponent.Cast\(storage\).*ordered.Sort\(\).*GetID\(\) != index.*GetAttachedEntity\(\) != item.GetOwner\(\)' 'ordered.Sort();'
Guard 'QOL_CARGO_NORMALIZE_VALIDATED_COPY' (Body 'AICF_LoadoutService.c' 'Validate') 'PackCargoLocal\(draft.GetCharacter\(\)\).*Signature\(draft.GetCharacter\(\)' 'PackCargoLocal(draft.GetCharacter())'
Guard 'QOL_CARGO_INSERT_MATERIALIZE' (Body 'AICF_LoadoutInventory.c' 'InsertLocal') 'SCR_UniversalInventoryStorageComponent.Cast\(storage\).*capacitySlot = -1;.*CanInsertResourceInStorage\(prefab, storage, capacitySlot\)' 'capacitySlot = -1;'
if ($failures.Count) { $failures | ForEach-Object { "FAIL $_" }; exit 1 }
Write-Output "PASS Loadout QoL contracts; mutations=$mutations; runtime=NOT_RUN"
