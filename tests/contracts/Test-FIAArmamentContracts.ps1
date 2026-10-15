param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$sources = @{}
$rhs = 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Content/'
foreach ($name in @('AICF_RHSPMCEquipment.c','AICF_RHSPMCArmament.c','AICF_RHSContentProfile.c')) {
    $sources[$name] = [IO.File]::ReadAllText((Join-Path $RepositoryRoot ($rhs + $name)))
}
$sources['wcs'] = [IO.File]::ReadAllText((Join-Path $RepositoryRoot 'AIConflictArlandWCSRHS/Scripts/Game/AIConflictArlandWCSRHS/AICF_WCSRHSContentProfile.c'))
function Rules($s) {
    $equipment = $s['AICF_RHSPMCEquipment.c']
    $arms = $s['AICF_RHSPMCArmament.c']
    $profile = $s['AICF_RHSContentProfile.c']
    [ordered]@{
        AUTHORITY = $equipment.Contains('!Replication.IsServer()') -and $equipment.Contains('rpl.IsMaster()') -and $equipment.Contains('!controller.IsPlayerControlled()')
        PROFILE_STATE = $profile.Contains('protected ref map<string, int> m_mPMCNextVariant') -and $profile.Contains('(next + 1) % count') -and $profile.Contains('!Replication.IsServer()')
        ONCE = $equipment.Contains('if (character.AICF_PMCEquipmentApplied()) return true;')
        STABLE_SELECTION = $equipment.Contains('if (m_iAICFPMCWeaponVariant < 0)') -and $equipment.Contains('m_iAICFPMCWeaponVariant = AICF_RHSPMCArmament.NextVariant(m_sAICFPMCSource)') -and $equipment.Contains('int variant = character.AICF_PMCWeaponVariant();')
        IDENTITY = $equipment.Contains('character.GetID() != identity') -and $equipment.Contains('SCR_ResourceNameUtils.GetPrefabName(character) != source') -and $equipment.Contains('GetID() != m_iAICFPMCIdentity') -and $equipment.Contains('!= m_sAICFPMCSource')
        SNAPSHOT_FENCE = $equipment.Contains('!AICF_LoadoutInventory.Capture(character, current) || current != before')
        VALIDATE_COMMIT = $equipment.Contains('inventoryRestored && signatureMatches && armamentValid') -and $equipment.Contains('AICF_LoadoutInventory.Restore(character, before)')
        PENDING_SUCCESS = $equipment -match 'm_bAICFPMCEquipmentApplied = true;\s*m_bAICFPMCEquipmentPending = false;' -and $equipment -notmatch 'Apply\(this\);\s*m_bAICFPMCEquipmentPending = false'
        BOARDING = ([regex]::Matches($equipment, 'character.AICF_IsPMCEquipmentPending\(\)\) return false;').Count -eq 2)
        CANDIDATE_BOUNDS = $arms.Contains('variant < 0 || variant >= VariantCount(source)')
        RHS_POOL = $arms.Contains('/AK74M/') -and $arms.Contains('Rifle_DOMk18_ION_RMR_SD.et') -and $arms.Contains('Rifle_AR15_GA_UD145_BLK_ION1.et') -and $arms.Contains('Rifle_DOM4A1_FSP_ION_VCOGRMR_SD.et') -and $arms.Contains('/SVD/') -and $arms.Contains('/PKM/')
        WCS_POOL = $s['wcs'].Contains('Rifle_AK12_EOTECH_SQUAD.et') -and $s['wcs'].Contains('Rifle_AK105_SQUAD.et') -and $s['wcs'].Contains('{4E0FB248B9FB1F5B}Prefabs/Weapons/Rifles/M4A1_URGI/Variants/Rifle_M4A1_URGI_145_Suppressed_SQUAD.et') -and $s['wcs'].Contains('Rifle_G28_Suppressed_SF_SQUAD.et')
        SUPPORT = $arms.Contains('SupportMagazinePrefab()') -and $arms.Contains('if (boxes < 3) return false;') -and $equipment.Contains('AICF_RHSPMCArmament.SupportMagazinePrefab(), 3')
        MAGAZINES = $arms.Contains('magazine.GetMagazineWell().Type() == muzzle.GetMagazineWell().Type()') -and $arms.Contains('magazine != muzzle.GetMagazine()') -and $arms.Contains('SpareMagazines(model, muzzle) < MinimumSpareMagazines(source)')
        LOCAL_DRAFT = $arms.Contains('model.GetWorld() == GetGame().GetWorld()') -and $equipment.Contains('draft.Clear();')
        CLOTHING_INDEPENDENT = $equipment.Contains('int clothingVariant = Variant(source);') -and $equipment.Contains('DressLocal(draft.GetCharacter(), clothingVariant,')
    }
}
$rules = Rules $sources
$failed = @($rules.Keys | Where-Object { -not $rules[$_] })
if ($failed.Count) { throw ('FIA armament contracts: ' + ($failed -join ', ')) }
$mutations = @(
    @('AICF_RHSPMCEquipment.c','rpl.IsMaster()', 'true','AUTHORITY'),
    @('AICF_RHSContentProfile.c','(next + 1) % count', '0','PROFILE_STATE'),
    @('AICF_RHSPMCEquipment.c','if (character.AICF_PMCEquipmentApplied()) return true;', '', 'ONCE'),
    @('AICF_RHSPMCEquipment.c','if (m_iAICFPMCWeaponVariant < 0)', 'if (true)','STABLE_SELECTION'),
    @('AICF_RHSPMCEquipment.c','character.GetID() != identity','false','IDENTITY'),
    @('AICF_RHSPMCEquipment.c','current != before','false','SNAPSHOT_FENCE'),
    @('AICF_RHSPMCEquipment.c','inventoryRestored && signatureMatches && armamentValid','inventoryRestored','VALIDATE_COMMIT'),
    @('AICF_RHSPMCEquipment.c','m_bAICFPMCEquipmentPending = false;', '', 'PENDING_SUCCESS'),
    @('AICF_RHSPMCEquipment.c','character.AICF_IsPMCEquipmentPending()) return false;', 'false) return false;', 'BOARDING'),
    @('AICF_RHSPMCArmament.c','variant < 0 || variant >= VariantCount(source)','false','CANDIDATE_BOUNDS'),
    @('wcs','Rifle_M4A1_URGI_145_Suppressed_SQUAD.et','Rifle_M4A1_WCS_URGI_Suppressed_SQUAD.et','WCS_POOL'),
    @('AICF_RHSPMCArmament.c','magazine.GetMagazineWell().Type() == muzzle.GetMagazineWell().Type()', 'true','MAGAZINES'),
    @('AICF_RHSPMCArmament.c','if (boxes < 3) return false;', '', 'SUPPORT')
)
foreach ($mutation in $mutations) {
    $copy = $sources.Clone()
    if (-not $copy[$mutation[0]].Contains($mutation[1])) { throw "Missing mutation $($mutation[3])" }
    $copy[$mutation[0]] = $copy[$mutation[0]].Replace($mutation[1], $mutation[2])
    if ((Rules $copy)[$mutation[3]]) { throw "Mutation survived: $($mutation[3])" }
}
"PASS FIA armament contracts: $($rules.Count) rules, $($mutations.Count) negative mutations; compile/runtime gates separate"
