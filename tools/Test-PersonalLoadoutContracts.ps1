param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$rules = @(
    @('Loadouts/AICF_PersonalLoadout.c', 'if \(!Replication.IsServer\(\) \|\| !player\) return;', 'SERVER_ONLY'),
    @('Loadouts/AICF_PersonalLoadout.c', 'SCR_FactionManager.SGetPlayerFaction\(player.GetPlayerId\(\)\)', 'SERVER_FACTION'),
    @('Loadouts/AICF_PersonalLoadout.c', 'loadout.GetFactionKey\(\) != faction.GetFactionKey\(\)', 'ROLE_FACTION'),
    @('Loadouts/AICF_PersonalLoadout.c', 'lock.IsLocked\(true\)', 'SPAWN_LOCK'),
    @('Loadouts/AICF_PersonalLoadout.c', 'revision == storedRevision && SameContext\(candidate, context\)', 'STALE_SAVE'),
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
    @('UI/AICF_LoadoutEditor.c', 'm_Catalog.SetPersonalRules\(m_Player.AICF_LoadoutLibrary\(\)\)', 'OWNER_CATALOG_RULES')
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
if (Test-Path (Join-Path $core 'Loadouts/AICF_PersonalLoadoutProbe.c')) { $failures += 'FIXTURE_IN_PRODUCTION' }
if ($failures.Count) { Write-Output "Personal loadout contracts: FAIL $($failures -join ', ')"; exit 1 }
Write-Output "Personal loadout contracts: PASS; negative mutations=$($rules.Count); runtime=NOT_RUN"
exit 0
