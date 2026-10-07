param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$rules = @(
    @('Loadouts/AICF_PersonalLoadout.c', 'if \(!Replication.IsServer\(\) \|\| !player\) return;', 'SERVER_ONLY'),
    @('Loadouts/AICF_PersonalLoadout.c', 'SCR_FactionManager.SGetPlayerFaction\(player.GetPlayerId\(\)\)', 'SERVER_FACTION'),
    @('Loadouts/AICF_PersonalLoadout.c', 'loadout.GetFactionKey\(\) != faction.GetFactionKey\(\)', 'ROLE_FACTION'),
    @('Loadouts/AICF_PersonalLoadout.c', 'lock.IsLocked\(true\)', 'SPAWN_LOCK'),
    @('Loadouts/AICF_PersonalLoadout.c', 'revision == storedRevision && SameContext\(candidate, context\)', 'STALE_SAVE'),
    @('Loadouts/AICF_PersonalLoadout.c', '(?s)SaveFor\(player, context, candidate, storedRevision \+ 1\)\)\s*\{\s*storedRevision\+\+;\s*stored = candidate;\s*available = true;\s*//[^\r\n]*\s*selected = true;\s*accepted = true;', 'SELECT_ONLY_AFTER_SAVE_READBACK'),
    @('Loadouts/AICF_PersonalLoadout.c', 'if \(operation == 4\)\s*\{\s*selected = false;', 'EXPLICIT_DEFAULT_PRESERVED'),
    @('Loadouts/AICF_PersonalLoadout.c', 'recipe.m_sProfile == context.m_sProfile', 'PROFILE_IDENTITY'),
    @('Loadouts/AICF_PersonalLoadout.c', 'recipe.m_sFaction == context.m_sFaction', 'FACTION_IDENTITY'),
    @('Loadouts/AICF_PersonalLoadout.c', 'recipe.m_sCharacter == context.m_sCharacter', 'ROLE_IDENTITY'),
    @('Loadouts/AICF_PersonalLoadout.c', 'AICF_LoadoutService.Validate\(recipe, faction, reason, true\)', 'SPAWN_REVALIDATION'),
    @('Loadouts/AICF_PersonalLoadout.c', 'controller.IsPlayerControlled\(\) \|\| controller.IsDead\(\)', 'UNASSIGNED_CHARACTER'),
    @('Loadouts/AICF_PersonalLoadout.c', 'entity.GetID\(\) != identity \|\| controller.IsPlayerControlled\(\) \|\| !rpl.IsMaster\(\)', 'COMMIT_IDENTITY'),
    @('Loadouts/AICF_PersonalLoadout.c', 'Signature\(entity, catalog, ignored\) == binding.m_sSignature', 'READBACK'),
    @('Loadouts/AICF_PersonalLoadout.c', 'Restore\(entity, before\) &&\s*AICF_LoadoutInventory.Signature\(entity, catalog, ignored\) == signature', 'ROLLBACK_READBACK'),
    @('Loadouts/AICF_PersonalLoadout.c', 'return rollback;', 'FAILED_ROLLBACK_REJECTS_SPAWN'),
    @('Loadouts/AICF_PersonalLoadout.c', 'RplProp\(condition: RplCondition.OwnerOnly\)', 'PRIVATE_REPLICATION'),
    @('Loadouts/AICF_PersonalLoadoutStore.c', '\$profile:AICF_PersonalLoadouts', 'SEPARATE_STORAGE'),
    @('Loadouts/AICF_PersonalLoadoutStore.c', 'GetPlayerIdentityId\(player.GetPlayerId\(\)\)', 'STABLE_IDENTITY'),
    @('Loadouts/AICF_PersonalLoadoutStore.c', '!SafeKey\(uid\) \|\| !SafeKey\(context.m_sProfile\) \|\| !SafeKey\(context.m_sFaction\)', 'SAFE_PATH'),
    @('Loadouts/AICF_PersonalLoadoutStore.c', 'revision % 2', 'TWO_GENERATIONS'),
    @('Loadouts/AICF_PersonalLoadoutStore.c', 'actualRevision == revision && actual.Encode\(\) == recipe.Encode\(\)', 'SAVE_READBACK'),
    @('Loadouts/AICF_LoadoutService.c', 'personal && !AICF_LoadoutInventory.AllowsPersonalInventory', 'NESTED_RULES'),
    @('Loadouts/AICF_LoadoutCatalog.c', 'IsPrefabBlacklisted\(prefab\)', 'MATCH_BLACKLIST'),
    @('UI/AICF_PersonalLoadoutUI.c', 'new AICF_LoadoutEditor\(m_Style\)', 'SHARED_EDITOR'),
    @('UI/AICF_PersonalLoadoutUI.c', 'm_bPending && m_iOperation != 0', 'READ_DOES_NOT_BLOCK_DEFAULT_SPAWN'),
    @('UI/AICF_PersonalLoadoutUI.c', 'button.RemoveHandler\(this\)', 'UI_CLEANUP'),
    @('UI/AICF_LoadoutEditor.c', 'm_Catalog.SetPersonalRules\(m_Player.AICF_LoadoutLibrary\(\)\)', 'OWNER_CATALOG_RULES'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'AICF_LoadoutRecipe.Decode\(player.AICF_LoadoutData\(\)\)', 'PREVIEW_OWNER_SNAPSHOT'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'player.AICF_LoadoutSlot\(\) != AICF_PersonalLoadout.SLOT', 'PREVIEW_SLOT_IDENTITY'),
    @('UI/AICF_PersonalLoadoutPreview.c', '!AICF_PersonalLoadout.SameContext\(recipe, context\)', 'PREVIEW_CONTEXT_IDENTITY'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'character.GetWorld\(\) == GetGame\(\).GetWorld\(\)', 'PREVIEW_WORLD_ISOLATION'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'm_Player != player \|\| key != m_sKey', 'PREVIEW_OWNER_CACHE'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'if \(personal\) catalog.SetPersonalRules\(rules\);', 'PREVIEW_PERSONAL_RULES'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'void Clear\(\)\s*\{\s*Detach\(\);\s*m_Draft.Clear\(\);', 'PREVIEW_DETACH_BEFORE_WORLD_DELETE'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'm_Manager.SetPreviewItem\(m_wPreview, character, null, true\)', 'PREVIEW_NATIVE_VISUAL_COPY'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'm_Manager = world.GetItemPreviewManager\(\);', 'PREVIEW_CAMPAIGN_MANAGER'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'if \(m_Manager && m_wPreview\) m_Manager.SetPreviewItem\(m_wPreview, null\)', 'PREVIEW_RENDER_TARGET_CLEANUP'),
    @('UI/AICF_PersonalLoadoutUI.c', 'if \(characterSelected \|\| m_Editor.IsInputCaptured\(\)\)\s*\{\s*ClearPreview\(\);', 'PREVIEW_LIVE_CHARACTER_PRIORITY'),
    @('UI/AICF_PersonalLoadoutUI.c', 'void Close\(\)\s*\{\s*ClearPreview\(\);', 'PREVIEW_WIDGET_CLEANUP'),
    @('UI/AICF_PersonalLoadoutUI.c', 'm_Preview.Update\(loadoutImage, player, context, faction\)', 'PREVIEW_PERSONAL_PANEL_OWNER'),
    @('UI/AICF_PersonalLoadoutPreview.c', 'if \(!m_Manager\) \{ Detach\(\); return false; \}', 'PREVIEW_REQUIRES_NATIVE_MANAGER'),
    @('UI/AICF_SquadRespawnUI.c', 'if \(role && m_PreviewComp\) m_PreviewComp.SetPreviewedLoadout\(role\);', 'PREVIEW_RESTORES_NATIVE_IMAGE'),
    @('UI/AICF_SquadRespawnUI.c', 'if \(m_bAICFCharacterPreview \|\| m_bAICFPersonalPreviewActive\) return;', 'PREVIEW_BLOCKS_NATIVE_OVERWRITE'),
    @('UI/AICF_SquadRespawnUI.c', 'if \(active && m_wLoadoutPreview\) m_wLoadoutPreview.SetVisible\(true\);', 'PREVIEW_PRESERVES_NATIVE_VIEWPORT_GEOMETRY'),
    @('UI/AICF_SquadRespawnUI.c', 'if \(m_PreviewComp\) return m_PreviewComp.GetItemPreviewWidget\(\);', 'PREVIEW_USES_MODEL_VIEWPORT_NOT_ROLE_ICON'),
    @('UI/AICF_SquadRespawnUI.c', 'm_AICFPersonalCard.m_OnClicked.Remove\(AICF_OnPersonalClicked\)', 'PERSONAL_CARD_CLEANUP'),
    @('UI/AICF_SquadRespawnUI.c', 'm_AICFPersonalCard.AICF_SetPersonalPreset\(role\)', 'PERSONAL_NATIVE_CARD'),
    @('UI/AICF_SquadRespawnUI.c', 'selected != m_bAICFPersonalCardSelected && !busy', 'PERSONAL_PAGE_AFTER_EDITOR_CLOSE'),
    @('UI/AICF_PersonalLoadoutUI.c', 'if \(personal\) Request\(3\);\s*else Request\(4\);', 'PERSONAL_CARD_AUTHORITY_INTENT'),
    @('UI/AICF_PersonalLoadoutUI.c', 'if \(!m_Player\) return !personal;', 'INITIAL_DEFAULT_ROLE_AVAILABLE'),
    @('UI/AICF_SquadRespawnUI.c', 'm_iCountShownItems = Math.Max\(2, m_iCountShownItems\);', 'BOTH_LOADOUT_CARDS_VISIBLE'),
    @('UI/AICF_SquadRespawnUI.c', 'int pageStart = Math.Floor\(index / m_iCountShownItems\) \* m_iCountShownItems;', 'PERSONAL_SELECTION_PRESERVES_DEFAULT_PAGE'),
    @('UI/AICF_SquadRespawnUI.c', 'if \(m_iAICFNativePageSize > 0\) m_iCountShownItems = m_iAICFNativePageSize;', 'GALLERY_PAGE_SIZE_RESTORED')
)
$failures = @()
foreach ($rule in $rules) {
    $source = Get-Content (Join-Path $core $rule[0]) -Raw -Encoding UTF8
    if ($source -notmatch $rule[1]) { $failures += $rule[2] }
    $negative = [regex]::Replace($source, $rule[1], '')
    if ($negative -match $rule[1]) { $failures += "MUTATION_ESCAPED:$($rule[2])" }
}
$personal = Get-Content (Join-Path $core 'Loadouts/AICF_PersonalLoadout.c') -Raw -Encoding UTF8
if ($personal -match 'SetLoadout\(|AICF_LoadoutStore|SetInitialMainEntity|CallLater\(') { $failures += 'OWNER_BOUNDARY' }
$previewUI = Get-Content (Join-Path $core 'UI/AICF_PersonalLoadoutUI.c') -Raw -Encoding UTF8
if ($previewUI -match 'm_PreviewComp|GetItemPreviewWidget|m_LoadoutRequestUIHandler') { $failures += 'PREVIEW_DEPENDS_ON_NATIVE_LAYOUT' }
$preview = Get-Content (Join-Path $core 'UI/AICF_PersonalLoadoutPreview.c') -Raw -Encoding UTF8
if ($preview -match 'CreateWidget|RemoveFromHierarchy|SetZOrder|SetOpacity|FrameSlot|\.SetWorld\(') { $failures += 'PREVIEW_MUST_REUSE_NATIVE_WIDGET' }
if ($previewUI -match 'PlacePreview|m_wPreviewRoot|m_wCoveredImage') { $failures += 'PREVIEW_ROOT_OVERLAY' }
if ($previewUI -match '\|\| !player.AICF_PersonalSelected\(\)') { $failures += 'DEFAULT_PREVIEW_NOT_UPDATED' }
if (Test-Path (Join-Path $core 'Loadouts/AICF_PersonalLoadoutProbe.c')) { $failures += 'FIXTURE_IN_PRODUCTION' }
if ($failures.Count) { Write-Output "Personal loadout contracts: FAIL $($failures -join ', ')"; exit 1 }
Write-Output "Personal loadout contracts: PASS; negative mutations=$($rules.Count); runtime=NOT_RUN"
exit 0
