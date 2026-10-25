param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$core = Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict'
$sources = @{}
foreach ($file in @('Loadouts/AICF_EquipmentInventory.c','Loadouts/AICF_EquipmentMember.c','Loadouts/AICF_EquipmentOrder.c','Loadouts/AICF_EquipmentService.c','Loadouts/AICF_EquipmentPlanFailures.c','Loadouts/AICF_EquipmentApplicator.c','Orders/AICF_OrderPlanner.c','State/AICF_GroupSlot.c','Vehicles/AICF_VehicleCoordinator.c')) {
    $sources[$file] = [IO.File]::ReadAllText((Join-Path $core $file))
}
function Rules($inputSources) {
    $inventory = $inputSources['Loadouts/AICF_EquipmentInventory.c']
    $member = $inputSources['Loadouts/AICF_EquipmentMember.c']
    $order = $inputSources['Loadouts/AICF_EquipmentOrder.c']
    $service = $inputSources['Loadouts/AICF_EquipmentService.c']
    $failures = $inputSources['Loadouts/AICF_EquipmentPlanFailures.c']
    $apply = $inputSources['Loadouts/AICF_EquipmentApplicator.c']
    $planner = $inputSources['Orders/AICF_OrderPlanner.c']
    $slot = $inputSources['State/AICF_GroupSlot.c']
    $vehicle = $inputSources['Vehicles/AICF_VehicleCoordinator.c']
    $rounds = Get-AICFMethodBody ([pscustomobject]@{Source=$inventory;Code=$inventory}) 'RoundsIn'
    $loaded = Get-AICFMethodBody ([pscustomobject]@{Source=$inventory;Code=$inventory}) 'Loaded'
    $consumables = Get-AICFMethodBody ([pscustomobject]@{Source=$member;Code=$member}) 'ConsumablesIn'
    [ordered]@{
        AUTHORITY = $service.Contains('!Replication.IsServer()') -and $service.Contains('!m_Campaign.IsMaster()') -and $service.Contains('!m_Campaign.IsRunning()')
        PLAN_FAILURE_KEY = $service.Contains('faction.GetFactionKey() + "|" + source') -and $service.Contains('key += "|" + binding.m_sInventory')
        PLAN_BACKOFF = $service.Contains('if (!m_PlanFailures.CanAttempt(key, now)) return null;') -and $service.IndexOf('m_PlanFailures.CanAttempt') -lt $service.IndexOf('new AICF_EquipmentPlan()') -and $service.Contains('m_PlanFailures.Record(key, System.GetTickCount())') -and $service.Contains('m_PlanFailures.Forget(key)')
        PLAN_BACKOFF_BOUND = $failures.Contains('RETRY_MS = 120000') -and $failures.Contains('MAX_ENTRIES = 256') -and $failures.Contains('if (age >= RETRY_MS) m_aEntries.Remove(index)') -and $failures.Contains('return m_aEntries.Count() < MAX_ENTRIES') -and $failures -notmatch 'IEntity|SharedItemRef|CallLater'
        PLAN_FAILURE_STOP = $service.Contains('m_PlanFailures.Clear();')
        CORPSE_SOURCE = $inventory.Contains('ECharacterLifeState.DEAD') -and $inventory.Contains('IsPlayerControlled()') -and $inventory.Contains('GetPlayerIdFromControlledEntity(character)')
        PHYSICAL_TRANSFER = $inventory.Contains('member.m_Character.GetOrigin(), corpse.GetOrigin(), AICF_EquipmentConfig.TRANSFER_METERS') -and $inventory.Contains('corpse.GetID() != corpseId') -and $inventory.Contains('item.GetID() != itemId')
        NATIVE_COMPATIBILITY = $inventory.Contains('muzzle.GetMagazineWell().Type() == magazine.GetMagazineWell().Type()') -and $inventory.Contains('magazine.GetAmmoCount()') -and $inventory.Contains('magazine != muzzle.GetMagazine()') -and $inventory.Contains('muzzle.IsBarrelChambered(i)')
        UNIQUE_IDENTITY = $inventory.Contains('item && !unique.Contains(item)') -and $inventory.Contains('unique.Insert(item)')
        UNIQUE_ROUNDS = $inventory.Contains('return RoundsIn(items, weapon);') -and $rounds.Contains('UniqueItems(items, unique);') -and $rounds.Contains('foreach (IEntity item : unique)')
        UNIQUE_CONSUMABLES = $member.Contains('return ConsumablesIn(items, prefab);') -and $consumables.Contains('AICF_EquipmentInventory.UniqueItems(items, unique);') -and $consumables.Contains('foreach (IEntity item : unique)')
        UNIQUE_TARGET = $member.Contains('AICF_EquipmentInventory.UniqueItems(rawItems, items);')
        MODEL_MUZZLE_LIFETIME = $member.Contains('BaseMuzzleComponent primaryMuzzle = AICF_EquipmentInventory.Muzzle(modelPrimary);') -and $member.Contains('primaryMuzzle = null;') -and $member -notmatch 'for\s*\([^;]*;[^;]*AICF_EquipmentInventory\.Muzzle\('
        INCOMPLETE_BINDING_ONLY = $member.Contains('(!modelPrimary && !binding) || (modelPrimary && !primaryMuzzle)') -and $member.Contains('m_iPrimaryRounds <= 0 && !binding')
        INCOMPLETE_READBACK = $member.Contains('binding && m_sSignature != binding.m_sSignature') -and $member.Contains('"BINDING_SIGNATURE_MISMATCH"')
        INCOMPLETE_MUZZLE_GUARD = $member -match 'm_iPrimaryRounds = 0;[\s\S]*if \(primaryMuzzle\)[\s\S]*m_iPrimaryRounds = AICF_EquipmentInventory.Rounds\(model, modelPrimary\)'
        EMPTY_MAGAZINE_CHAMBER = $loaded -match 'MT_BaseMuzzle\)[\s\S]*rounds = 0;[\s\S]*if \(magazine\) rounds = magazine.GetAmmoCount\(\);' -and $loaded.Contains('if (muzzle.IsBarrelChambered(i)) rounds++;')
        REAL_TRANSFER = $inventory.Contains('target.TryMoveItemToStorage(item, storage)') -and $inventory.Contains('target.Contains(item) && !source.Contains(item)') -and $inventory -notmatch 'SpawnEntity|AddSupplies|SetAmmoCount|Ticket'
        MEMBER_REVISION = $member.Contains('m_Slot.GetRosterMember(m_iIndex) != m_Character') -and $member.Contains('binding.m_iRevision == m_iRevision') -and $member.Contains('m_Slot.GetSpawnGeneration() != m_iGeneration')
        OPERATION_IDENTITY = $order.Contains('m_Slot.GetEquipmentOrder() == this') -and $order.Contains('m_Slot.GetStrategicIntentRevision() == m_iIntent') -and $order.Contains('m_Slot.GetStrategicAssignmentRevision() == m_iAssignment')
        CORPSE_BOUNDARY = $service -notmatch '\.AddWaypoint|\.RemoveWaypoint|DeleteRplEntity|\.ClearOrder' -and $service.Contains('m_Planner.BeginEquipmentOrder(order)')
        SERVICE_CAPABILITY = $order.Contains('SCR_EServicePointType.ARMORY') -and $order.Contains('SCR_EServicePointStatus.ONLINE') -and $order.Contains('CollectAvailable') -and $order.Contains('GetAvailablePrefabs') -and $order.Contains('m_Base.GetFaction() != m_Faction')
        INVENTORY_RECEIPTS = $apply.Contains('class AICF_EquipmentReceipt : ScriptedInventoryOperationCallback') -and $apply.Contains('!tx.m_Receipt.m_bDone') -and $apply.Contains('m_bCancelled') -and $apply.Contains('Recover(member)')
        PRESERVE_ORIGINALS = $apply.Contains('GetStorage().GetOwner() != character') -and $apply.Contains('m_aOriginal') -and $apply.Contains('manager.Contains(verify.m_Item)') -and $apply -notmatch 'DeleteRplEntity\(member.m_Character|\.Kill\('
        BOUNDED_RECOVERY = $apply.Contains('System.GetTickCount() >= tx.m_iRecoveryDeadline') -and $apply.Contains('tx.m_iRecoveryDeadline = System.GetTickCount() + 60000')
        MOVEMENT_RELEASE = $apply.Contains('controller.SetDisableMovementControls(true)') -and $apply.Contains('controller.SetDisableMovementControls(false)') -and $apply.Contains('Release(member);') -and $service.Contains('m_Applicator.Release(remaining)')
        MODEL_CLEANUP = $member.Contains('class AICF_EquipmentDraft : AICF_LoadoutDraft') -and $member.Contains('m_Character.GetWorld() != GetGame().GetWorld()') -and $member.Contains('DetachRoots();') -and $member.Contains('parent.DetachEntity()')
        MODEL_ROOT_DELETE = $member.Contains('!parent || parent.GetStorage().GetOwner() != m_Character') -and $member.Contains('entity && item && !item.GetParentSlot() && entity.GetWorld() == m_Character.GetWorld()') -and $member.Contains('SCR_EntityHelper.DeleteEntityAndChildren(entity);')
        MODEL_MUZZLE_RELEASE = $member.Contains('BaseMuzzleComponent primaryMuzzle = AICF_EquipmentInventory.Muzzle(modelPrimary);') -and $member.Contains('primaryMuzzle = null;') -and $member -notmatch 'Muzzle\(modelPrimary\)\.(GetBarrelsCount|IsBarrelChambered)'
        SINGLE_OPERATION = $slot.Contains('HasEquipmentInventoryTransaction()') -and $vehicle.Contains('slot.HasEquipmentInventoryTransaction()') -and $planner.Contains('slot.HasEquipmentInventoryTransaction()')
        STOP = $service.Contains('m_aRecoveries.Clear()') -and $service.Contains('m_aPlans.Clear()') -and $service -notmatch '\.CallLater\(|\.Insert\(On'
    }
}
$rules = Rules $sources
$failed = @($rules.Keys | Where-Object {-not $rules[$_]})
if ($failed.Count) { throw ('Equipment contract failures: ' + ($failed -join ', ')) }
$mutations = @(
    @('Loadouts/AICF_EquipmentMember.c','primaryMuzzle = null;', '', 'MODEL_MUZZLE_RELEASE'),
    @('Loadouts/AICF_EquipmentMember.c','primaryMuzzle.GetBarrelsCount()', 'AICF_EquipmentInventory.Muzzle(modelPrimary).GetBarrelsCount()', 'MODEL_MUZZLE_RELEASE'),
    @('Loadouts/AICF_EquipmentService.c','faction.GetFactionKey() + "|" + source', 'source', 'PLAN_FAILURE_KEY'),
    @('Loadouts/AICF_EquipmentService.c','key += "|" + binding.m_sInventory', 'key += ""', 'PLAN_FAILURE_KEY'),
    @('Loadouts/AICF_EquipmentService.c','if (!m_PlanFailures.CanAttempt(key, now)) return null;', '', 'PLAN_BACKOFF'),
    @('Loadouts/AICF_EquipmentService.c','m_PlanFailures.Record(key, System.GetTickCount())', 'm_PlanFailures.Forget(key)', 'PLAN_BACKOFF'),
    @('Loadouts/AICF_EquipmentService.c','m_PlanFailures.Clear();', '', 'PLAN_FAILURE_STOP'),
    @('Loadouts/AICF_EquipmentPlanFailures.c','RETRY_MS = 120000', 'RETRY_MS = 0', 'PLAN_BACKOFF_BOUND'),
    @('Loadouts/AICF_EquipmentPlanFailures.c','MAX_ENTRIES = 256', 'MAX_ENTRIES = 0', 'PLAN_BACKOFF_BOUND'),
    @('Loadouts/AICF_EquipmentPlanFailures.c','return m_aEntries.Count() < MAX_ENTRIES', 'return true', 'PLAN_BACKOFF_BOUND'),
    @('Loadouts/AICF_EquipmentMember.c','SCR_EntityHelper.DeleteEntityAndChildren(entity);', '', 'MODEL_ROOT_DELETE'),
    @('Loadouts/AICF_EquipmentMember.c','entity.GetWorld() == m_Character.GetWorld()', 'true', 'MODEL_ROOT_DELETE'),
    @('Loadouts/AICF_EquipmentInventory.c','item && !unique.Contains(item)', 'item', 'UNIQUE_IDENTITY'),
    @('Loadouts/AICF_EquipmentInventory.c','foreach (IEntity item : unique)', 'foreach (IEntity item : items)', 'UNIQUE_ROUNDS'),
    @('Loadouts/AICF_EquipmentMember.c','foreach (IEntity item : unique)', 'foreach (IEntity item : items)', 'UNIQUE_CONSUMABLES'),
    @('Loadouts/AICF_EquipmentMember.c','AICF_EquipmentInventory.UniqueItems(rawItems, items);', 'items = rawItems;', 'UNIQUE_TARGET'),
    @('Loadouts/AICF_EquipmentMember.c','primaryMuzzle = null;', '', 'MODEL_MUZZLE_LIFETIME'),
    @('Loadouts/AICF_EquipmentMember.c','!modelPrimary && !binding', '!modelPrimary', 'INCOMPLETE_BINDING_ONLY'),
    @('Loadouts/AICF_EquipmentMember.c','m_iPrimaryRounds <= 0 && !binding', 'm_iPrimaryRounds <= 0', 'INCOMPLETE_BINDING_ONLY'),
    @('Loadouts/AICF_EquipmentMember.c','binding && m_sSignature != binding.m_sSignature', 'false', 'INCOMPLETE_READBACK'),
    @('Loadouts/AICF_EquipmentMember.c','if (primaryMuzzle)', 'if (true)', 'INCOMPLETE_MUZZLE_GUARD'),
    @('Loadouts/AICF_EquipmentInventory.c','rounds = 0;', 'rounds = muzzle.GetAmmoCount();', 'EMPTY_MAGAZINE_CHAMBER'),
    @('Loadouts/AICF_EquipmentMember.c','barrel < primaryMuzzle.GetBarrelsCount()', 'barrel < AICF_EquipmentInventory.Muzzle(modelPrimary).GetBarrelsCount()', 'MODEL_MUZZLE_LIFETIME'),
    @('Loadouts/AICF_EquipmentInventory.c','muzzle.GetMagazineWell().Type() == magazine.GetMagazineWell().Type()', 'true', 'NATIVE_COMPATIBILITY'),
    @('Loadouts/AICF_EquipmentInventory.c','member.m_Character.GetOrigin(), corpse.GetOrigin(), AICF_EquipmentConfig.TRANSFER_METERS', 'member.m_Character.GetOrigin(), corpse.GetOrigin(), 5000', 'PHYSICAL_TRANSFER'),
    @('Loadouts/AICF_EquipmentInventory.c','target.Contains(item) && !source.Contains(item)', 'target.Contains(item)', 'REAL_TRANSFER'),
    @('Loadouts/AICF_EquipmentMember.c','binding.m_iRevision == m_iRevision', 'true', 'MEMBER_REVISION'),
    @('Loadouts/AICF_EquipmentOrder.c','m_Slot.GetEquipmentOrder() == this', 'true', 'OPERATION_IDENTITY'),
    @('Loadouts/AICF_EquipmentApplicator.c','GetStorage().GetOwner() != character', 'GetStorage().GetOwner() == null', 'PRESERVE_ORIGINALS'),
    @('Loadouts/AICF_EquipmentApplicator.c','!tx.m_Receipt.m_bDone', 'false', 'INVENTORY_RECEIPTS'),
    @('Loadouts/AICF_EquipmentService.c','m_aRecoveries.Clear()', 'm_aSlots.Clear()', 'STOP'),
    @('Loadouts/AICF_EquipmentApplicator.c','System.GetTickCount() >= tx.m_iRecoveryDeadline', 'false', 'BOUNDED_RECOVERY'),
    @('Loadouts/AICF_EquipmentApplicator.c','controller.SetDisableMovementControls(false)', 'controller.SetDisableMovementControls(true)', 'MOVEMENT_RELEASE'),
    @('Loadouts/AICF_EquipmentMember.c','DetachRoots();', '// no detach', 'MODEL_CLEANUP')
)
foreach ($mutation in $mutations) {
    $copy = $sources.Clone()
    if (-not $copy[$mutation[0]].Contains($mutation[1])) { throw "Missing mutation: $($mutation[3])" }
    $copy[$mutation[0]] = $copy[$mutation[0]].Replace($mutation[1], $mutation[2])
    $result = Rules $copy
    if ($result[$mutation[3]]) { throw "Mutation survived: $($mutation[3])" }
}
"PASS Equipment contracts: $($rules.Count) rules, $($mutations.Count) negative mutations; runtime gates separate"
