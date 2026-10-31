param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
. (Join-Path $PSScriptRoot '../lib/AIQuotaFloorAudit.Common.ps1')
function Body([string]$path,[string]$name){$s=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $path));Get-AICFMethodBody ([pscustomobject]@{Source=$s;Code=(ConvertTo-AICFCodeText $s)}) $name}
$forces='AIConflictCore/Scripts/Game/AIConflict/Forces'
$inputs=@{
 RootCapacity=Body 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c' 'EnsureAIWorldCapacity';
 Install=Body "$forces/AICF_AIWorldQuotaService.c" 'InstallNativeFloor';
 WorldCapture=Body "$forces/AICF_AIQuotaWorldState.c" 'AICF_AIQuotaWorldState';
 StartupCapture=Body "$forces/AICF_AIQuotaStartup.c" 'Capture';
 Start=Body 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c' 'Start'
}
$bad=@(Get-AICFQuotaFloorAuditFailures @inputs)
if($bad.Count){throw "Actual partition floor audit failed: $bad"}
$mutants=0
foreach($mutation in @(
 @('RootCapacity','quota.Initialize','quota.MissingInitialize','ROOT_PARTITION_INITIALIZE'),
 @('RootCapacity','quota.InstallNativeFloor','quota.MissingFloor','ROOT_PARTITION_INITIALIZE'),
 @('Install','previous < required','true','PRESERVE_HIGHER_NATIVE_FLOOR'),
 @('Install','m_Limits.GetTotalLimit()','220','PRESERVE_HIGHER_NATIVE_FLOOR'),
 @('Install','effective < required','false','VERIFY_NATIVE_READBACK'),
 @('Install','original_native=','hidden_original=','OBSERVABLE_PARTITION_FLOOR'),
 @('WorldCapture','world.GetLimitOfActiveAIs()','220','ORIGINAL_WORLD_CAPTURE'),
 @('StartupCapture','world.AICF_GetQuotaWorldState()','null','STARTUP_READONLY_ORIGINAL'),
 @('Start','m_QuotaStartup.Check()','AICF_EAIQuotaStartup.READY','CAPTURE_BEFORE_STARTED')
)){
 $changed=@{};foreach($key in $inputs.Keys){$changed[$key]=$inputs[$key]}
 if(!$changed[$mutation[0]].Contains($mutation[1])){throw "Missing mutation $($mutation[3])"}
 $changed[$mutation[0]]=$changed[$mutation[0]].Replace($mutation[1],$mutation[2])
 if(@(Get-AICFQuotaFloorAuditFailures @changed)-notcontains$mutation[3]){throw "Unsafe source survived $($mutation[3])"}
 $mutants++
}
"PASS quota floor audit actual=7 executed_unsafe=$mutants managed220_preserved=1 original_native_capture=1 runtime=NOT_RUN"
