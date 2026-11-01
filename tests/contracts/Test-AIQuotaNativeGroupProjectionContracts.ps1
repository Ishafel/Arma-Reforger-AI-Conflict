param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$pinnedPath=Join-Path $RepositoryRoot '.cache/reforger-api/Arma-Reforger-Script-Diff-1.8.0.13/scripts/Game/Entities/SCR_AIGroup.c'
$adapterPath=Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_AIQuotaNativeGroup.c'
$pinned=[IO.File]::ReadAllText($pinnedPath).Replace("`r`n","`n");$adapter=[IO.File]::ReadAllText($adapterPath).Replace("`r`n","`n")
function Method([string]$text,[string]$name){Get-AICFMethodBody ([pscustomobject]@{Source=$text;Code=(ConvertTo-AICFCodeText $text)}) $name}
function Tokens([string]$text){
 @([regex]::Matches($text,'(?s)/\*.*?\*/|//[^\r\n]*|"(?:\\.|[^"\\])*"|[A-Za-z_]\w*|\d+(?:\.\d+)?|[^\s]')|Where-Object{!$_.Value.StartsWith('//')-and!$_.Value.StartsWith('/*')}|ForEach-Object{$_.Value}) -join "`n"
}
$expected=Tokens (Method $pinned 'SpawnGroupMember')
$names=@('ENTRY','BEGIN_FACTORY','OBSERVE_FACTORY','NONNULL_DISAPPEARED','PRE_ADD','POST_ADD','PRE_EVENT','FINISH')
function Projection([string]$text){
 $body=Method $text 'SpawnGroupMember'
 $found=@([regex]::Matches($body,'(?m)^\s*// AICF_QUOTA_BLOCK_BEGIN ([A-Z_]+)\s*$')|ForEach-Object{$_.Groups[1].Value})
 if($found.Count-ne8-or@($found|Select-Object -Unique).Count-ne8){throw 'Native adapter marker set invalid'}
 foreach($name in $names){if($found-notcontains$name){throw ('Missing native boundary marker '+$name)}}
 $restore=[regex]::Replace($body,'(?ms)^[\t ]*// AICF_QUOTA_BLOCK_BEGIN ([A-Z_]+)[^\r\n]*\r?\n.*?^[\t ]*// AICF_QUOTA_BLOCK_END \1[^\r\n]*(?:\r?\n|$)','')
 return Tokens $restore
}
if((Projection $adapter)-cne$expected){throw 'Pinned native body inverse token projection drifted'}
$cases=1
foreach($mutation in @(
 @('formation','Vector(index, 0, 0)','Vector(index + 1, 0, 0)'),
 @('navmesh','"10 10 10"','"100 100 100"'),
 @('water','pos = outWaterSurfacePoint;','pos = Vector(0, 0, 0);'),
 @('lastEvent','InvokeEventOnInit();','Print("skipped");'),
 @('factory','SpawnEntityPrefabEx(res, true, world, spawnParams)','SpawnEntityPrefabEx(res, false, world, spawnParams)'),
 @('stockNull',"if (!member)`n`t`t`treturn true;","if (!member)`n`t`t`treturn false;"),
 @('dropMarker','AICF_QUOTA_BLOCK_BEGIN PRE_ADD','AICF_QUOTA_BLOCK_BEGIN UNKNOWN_BLOCK')
)){
 if(!$adapter.Contains($mutation[1])){throw ('Unsafe fixture mutation has no matching actual source: '+$mutation[0])}
 $changed=$adapter.Replace($mutation[1],$mutation[2]);$rejected=$false
 try{$rejected=(Projection $changed)-cne$expected}catch{$rejected=$true}
 if(!$rejected){throw ('Unsafe pinned body mutation survived: '+$mutation[0])};$cases++
}
$body=Method $adapter 'SpawnGroupMember'
$checks=@(
 @('outside_edit_authority_chain',($body-match'if \(editMode \|\| !Replication.IsServer\(\) \|\| !quota\)' -and $body-match'return super\.SpawnGroupMember\(snapToTerrain, index, res, editMode, isLast\);')),
 @('veto_before_native',($body.IndexOf('if (!quotaBirth) return false;')-lt$body.IndexOf('IEntity member = GetGame().SpawnEntityPrefabEx'))),
 @('exact_return_before_add',($body.IndexOf('ObserveGroupFactory(quotaBirth, member)')-lt$body.IndexOf('AddAIEntityToGroup(member)'))),
 @('typed_null_only',($body-match'(?s)m_Status == AICF_EAIQuotaBirthStatus.NATIVE_NULL\)\s*\{\s*quota.EndGroupBirth\(quotaBirth\);\s*return true;')),
 @('nonnull_rejected_not_null',($body-match'(?s)m_Status != AICF_EAIQuotaBirthStatus.BOUND\)\s*return AICF_RejectQuotaBirth')),
 @('pre_and_post_add_fences',($body-match'IsGroupBirthCurrent\(quotaBirth, false\)' -and $body-match'IsGroupBirthCurrent\(quotaBirth, true\)')),
 @('final_native_observation',($body-match'if \(!quota.CompleteGroupBirth\(quotaBirth\)\) return false;'))
)
foreach($check in $checks){if(!$check[1]){throw ('Native adapter boundary contract '+$check[0])};$cases++}
$rollback=Method $adapter 'AICF_RejectQuotaBirth'
if($rollback-match'(?s)DeleteRplEntity.*CanRollbackGroupBirth' -or @([regex]::Matches($rollback,'CanRollbackGroupBirth\(birth\)')).Count-ne2 -or $rollback-notmatch'ObserveBirthAbsence\(birth\)'){throw 'Rollback does not repeat exact cancelled newborn proof/actual absence'}
$cases++
"PASS AI quota pinned native group projection source_cases=$cases inverse_token_equal=1 pinned_version=1.8.0.13 runtime=NOT_RUN"
