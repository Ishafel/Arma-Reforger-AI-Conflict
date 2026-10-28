param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$source=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'tests/fixtures/AICF_VehicleSoakProbe.c'))
function Body([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Rules([string]$text){
 $bind=Body $text 'Bind';$current=Body $text 'IsCurrent';$stop=Body $text 'Stop';$callback=Body $text 'OnDamage'
 foreach($fence in @('m_Actor.GetID() == m_ActorId','m_Actor.GetDamageManager() == m_Damage','m_Group.GetID() == m_GroupId',
  'm_Slot.GetGroup() == m_Group','m_Slot.GetRosterMember(m_iPosition) == m_Actor','Replication.FindItemId(m_Actor).ToString() == m_sActorRpl',
  'Replication.FindItemId(m_Group).ToString() == m_sGroupRpl','m_Slot.GetSpawnGeneration() == m_iGeneration')){
  if(!$current.Contains($fence)){'EXACT_ORIGINAL_IDENTITY';break}
 }
 if(!$bind.Contains('if (m_bBound') -or !$bind.Contains('m_Damage.GetOnDamage().Insert(OnDamage)') -or
  !$stop.Contains('m_Damage.GetOnDamage().Remove(OnDamage)') -or !$stop.Contains('m_bBound = false')){'PAIRED_SINGLE_BIND'}
 if(!$callback.Contains('if (!IsCurrent()) { Stop(); return; }') -or !$callback.Contains('context.hitEntity != m_Actor')){'CALLBACK_IDENTITY_FILTER'}
 foreach($field in @('context.damageType','context.damageValue','context.damageSource','context.hitPosition','context.impactVelocity',
  'context.instigator.GetInstigatorEntity()','context.instigator.GetInstigatorType()','m_Actor.GetOrigin()',
  'controller.GetLifeState()','agent.GetParentGroup()','CompartmentAccessComponent.GetVehicleIn(m_Actor)','cause=UNDETERMINED observer_only=1')){
  if(!$callback.Contains($field)){'ACTUAL_CONTEXT_ONLY';break}
 }
 $observer=$text.Substring($text.IndexOf('class AICF_VehicleSoakDamageObserver'),$text.IndexOf('modded class AICF_MatchController')-$text.IndexOf('class AICF_VehicleSoakDamageObserver'))
 if($observer -match '\.(?:SetHealth|SetHealthScaled|FullHeal|Heal|Kill|KillGM|ForceUnconsciousness|Teleport|SetOrigin|SetVelocity|SetDamage|Damage)\s*\('){'OBSERVER_MUTATES_GAMEPLAY'}
 if(@([regex]::Matches($text,'AICF_VehicleSoakStopDamageObservers\(\);')).Count -lt 4 -or
  !$text.Contains('damageObserver.Poll();') -or !$text.Contains('damageMember < 5') -or
  $text.IndexOf('observer.Bind(slot, damageMember') -lt $text.IndexOf('m_aVehicleSoakMembers.Insert')){'LIFECYCLE_ROSTER_BOUNDARY'}
}
$violations=@(Rules $source)
if($violations.Count){throw "Production observer failed: $violations"}
$mutants=@(
 @('actor ID','m_Actor.GetID() == m_ActorId','true'),
 @('actor Rpl','Replication.FindItemId(m_Actor).ToString() == m_sActorRpl','true'),
 @('group identity','m_Slot.GetGroup() == m_Group','true'),
 @('generation','m_Slot.GetSpawnGeneration() == m_iGeneration','true'),
 @('native hit entity','context.hitEntity != m_Actor','false'),
 @('remove','m_Damage.GetOnDamage().Remove(OnDamage)','Print("lost_remove")'),
 @('health mutation','Print(details);','m_Actor.GetDamageManager().SetHealthScaled(1); Print(details);')
)
foreach($mutant in $mutants){if(@(Rules ($source.Replace($mutant[1],$mutant[2]))).Count -eq 0){throw "Escaped mutant: $($mutant[0])"}}
$body=Body $source 'MatchesIdentity'
$type='AICFSoakDamageIdentity'+[guid]::NewGuid().ToString('N')
Add-Type "public static class $type { public static bool Check(bool exactActor, bool exactGroup, bool exactSlot, bool exactRpl, bool exactGeneration) {$body} }"
$method=([type]$type).GetMethod('Check')
for($mask=0;$mask -lt 32;$mask++){
 $args=@();for($bit=0;$bit -lt 5;$bit++){$args+= [bool]($mask -band (1 -shl $bit))}
 if($method.Invoke($null,$args) -ne ($mask -eq 31)){throw "Identity negative control escaped mask=$mask"}
}
$apiRoot=Join-Path $RepositoryRoot '.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/scripts'
if(Test-Path $apiRoot){
 $damage=[IO.File]::ReadAllText((Join-Path $apiRoot 'GameCode/Components/SCR_DamageManagerComponent.c'))
 $context=[IO.File]::ReadAllText((Join-Path $apiRoot 'Game/generated/Damage/BaseDamageContext.c'))
 $instigator=[IO.File]::ReadAllText((Join-Path $apiRoot 'Game/generated/Damage/Instigator.c'))
 if($damage -notmatch 'invoker.Invoke\(damageContext\)' -or $context -notmatch 'IEntity hitEntity' -or
  $instigator -notmatch 'IEntity GetInstigatorEntity\(\)'){throw 'Pinned native callback/API contract changed'}
}
Write-Output 'PASS VehicleSoak damage observer: read-only exact original entity/Rpl/generation, lifecycle Remove,7 source mutants +32 executed identity controls; pinned API1.8.0.13'
