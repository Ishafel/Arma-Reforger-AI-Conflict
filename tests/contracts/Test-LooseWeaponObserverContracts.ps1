param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_LooseWeaponObserverProbe.c'))
function Body([string]$s,[string]$n){Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $n}
function Rules([string]$s){
 $baseline=Body $s 'AICF_LWRunBaseline';$observe=ConvertTo-AICFCodeText ($s.Replace($baseline,'{}'))
 if($observe -match '\bdelete\b|DeleteEntity|DeleteRplEntity|SetOrigin\(|SetWorldTransform\(|SetHealth\(|SetDamage\(|EnableDamageHandling\(|ChangeSimulationState\(|SetVelocity\(|DisablePhysics\(|TryRemoveItemFromStorage\(|TryMoveItemToStorage\('){'READONLY_ORDINARY_OBSERVER'}
 $task=Body $s 'EOnTaskSimulate';$removed=Body $s 'OnItemRemoved';$added=Body $s 'OnItemAdded'
 if(@([regex]::Matches($task,'super\.EOnTaskSimulate\(owner, dt\)')).Count -ne 1 -or !$task.Contains('return result;')){'NATIVE_TASK_SUPER_ONCE_UNCHANGED'}
 if(@([regex]::Matches($removed,'super\.OnItemRemoved\(storageOwner, item\)')).Count -ne 1 -or
  @([regex]::Matches($added,'super\.OnItemAdded\(storageOwner, item\)')).Count -ne 1){'NATIVE_INVENTORY_SUPER_ONCE'}
 $current=Body $s 'Current';foreach($guard in @('record.m_Item.GetID() == record.m_Id','Rpl(record.m_Item) == record.m_sRpl','World(record.m_Item) == record.m_sWorld')){if(!$current.Contains($guard)){'IMMUTABLE_ITEM_WORLD_RPL';break}}
 $track=Body $s 'Track';$tick=Body $s 'Tick';$emit=Body $s 'Emit'
 if(!$track.Contains('s_aRecords.Count() >= MAX_RECORDS') -or !$s.Contains('MAX_RECORDS = 512;') -or !$s.Contains('TTL_MS = 120000;') -or !$tick.Contains('System.GetTickCount() - record.m_iCreatedAt >= TTL_MS')){'BOUNDED_LEDGER'}
 if($emit -notmatch 'int held = -1; int weaponSlot = -1; int contained = -1; int changing = -1' -or
  !$emit.Contains('record.m_Source.GetID() == record.m_SourceId && Rpl(record.m_Source) == record.m_sSourceRpl')){'UNKNOWN_SOURCE_IS_NOT_ABSENCE_PROOF'}
 foreach($guard in @('GetCurrentItemInHands() == item','GetRightHandItem() == item','GetLeftHandGadgetEntity() == item','GetWeaponEntity() == item','manager.Contains(item)','inventory.GetParentSlot()','item.GetWorld() == GetGame().GetWorld()','physics.IsDynamic()','physics.GetSimulationState()','physics.GetInteractionLayer()')){if(!$emit.Contains($guard)){'ACTUAL_INVENTORY_HELD_PHYSICS_CONTEXT';break}}
 $dynamicMatch=[regex]::Match($emit,'(?s)if \(dynamicBody\)\s*\{(?<body>.*?)\}\s*else')
 if(!$emit.Contains('bool dynamicBody = physics.IsDynamic();') -or !$dynamicMatch.Success -or
  $emit.Replace($dynamicMatch.Value,'') -match 'physics\.(GetSimulationState|GetInteractionLayer|IsKinematic|IsActive|GetVelocity|GetMass)\(' -or
  !$emit.Contains('simulation=-1 layer=-1 dynamic=0 kinematic=-1 active=-1 velocity=UNKNOWN_NON_DYNAMIC mass=UNKNOWN_NON_DYNAMIC')){'NO_UNSAFE_NON_DYNAMIC_PHYSICS_GETTERS'}
 if(!$baseline.Contains('System.GetCLIParam("aicfLooseWeaponBaseline", flag)') -or !$baseline.Contains('m_bAICFLWDropRequested = true;') -or
  !$baseline.Contains('SCR_AIDropWeapon drop = new SCR_AIDropWeapon();') -or !$baseline.Contains('drop.EOnTaskSimulate(agent, 0.016)') -or
  !$baseline.Contains('System.GetTickCount() - m_iAICFLWBaselineStarted < 60000')){'EXPLICIT_SINGLE_NATIVE_DROP_BASELINE'}
 if(!$s.Contains('s_bStopped = true; s_aRecords.Clear();') -or $s -match '\bref\s+(IEntity|BaseWorld|Physics|SCR_AIGroup)\b' -or
  $observe -match 'CallLater\(|\.Insert\(.*(?:Callback|Tick)'){ 'NO_STRONG_ENGINE_REFS_OR_UNBALANCED_ASYNC'}
 if(!(Body $s 'AICF_LWObservePlans').Contains('plan.m_Draft.GetCharacter()') -or !$s.Contains('"PRIVATE_MODEL_PLAN_"') -or !$s.Contains('"PRIVATE_WEAPON_PLAN_"')){'ACTUAL_CACHED_PRIVATE_MODELS'}
}
$failures=@(Rules $source);if($failures.Count){throw "Loose weapon observer: $failures"}
$checks=1
foreach($m in @(
 @('Rpl(record.m_Item) == record.m_sRpl','true','IMMUTABLE_ITEM_WORLD_RPL'),
 @('World(record.m_Item) == record.m_sWorld','true','IMMUTABLE_ITEM_WORLD_RPL'),
 @('s_aRecords.Count() >= MAX_RECORDS','false','BOUNDED_LEDGER'),
 @('int held = -1;','int held = 0;','UNKNOWN_SOURCE_IS_NOT_ABSENCE_PROOF'),
 @('GetCurrentItemInHands() == item','GetCurrentItemInHands() == null','ACTUAL_INVENTORY_HELD_PHYSICS_CONTEXT'),
 @('if (dynamicBody)','if (physics)','NO_UNSAFE_NON_DYNAMIC_PHYSICS_GETTERS'),
 @('super.EOnTaskSimulate(owner, dt)','ENodeResult.SUCCESS','NATIVE_TASK_SUPER_ONCE_UNCHANGED'),
 @('s_bStopped = true; s_aRecords.Clear();','s_bStopped = true;','NO_STRONG_ENGINE_REFS_OR_UNBALANCED_ASYNC'),
 @('System.GetCLIParam("aicfLooseWeaponBaseline", flag)','true','EXPLICIT_SINGLE_NATIVE_DROP_BASELINE')
)){$changed=$source.Replace($m[0],$m[1]);if($changed -ceq $source -or $m[2] -notin @(Rules $changed)){throw "Surviving observer mutant $($m[2])"};$checks++}
$body=Body $source 'Emit';$changed=$source.Replace($body,$body.Insert(1,"`n item.SetOrigin(vector.Zero);"));if('READONLY_ORDINARY_OBSERVER' -notin @(Rules $changed)){throw 'Teleport mutant survived'};$checks++
$changed=$source.Replace($body,$body.Insert(1,"`n RplComponent.DeleteRplEntity(item, false);"));if('READONLY_ORDINARY_OBSERVER' -notin @(Rules $changed)){throw 'Delete mutant survived'};$checks++
$changed=$source.Replace($body,$body.Insert(1,"`n physics.GetInteractionLayer();"));if('NO_UNSAFE_NON_DYNAMIC_PHYSICS_GETTERS' -notin @(Rules $changed)){throw 'Static physics getter mutant survived'};$checks++
"PASS Loose weapon observer contracts: readonly ordinary mode, single explicit native-drop baseline, weak bounded identity/world ledger, actual held/storage/private-model/physics samples; controls=$checks"
