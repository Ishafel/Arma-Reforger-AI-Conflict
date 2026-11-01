param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$source=Get-Content (Join-Path $RepositoryRoot 'AIConflictArlandRHS/Scripts/Game/AIConflictArlandRHS/Content/AICF_RHSPMCEquipment.c') -Raw
$body=[regex]::Match($source,'class AICF_PMCEquipmentFrameBudget\s*\{[\s\S]*?\n\}').Value
if(!$body){throw 'Missing production budget'}
$body=$body.Replace('class AICF_', 'public class AICF_').Replace('protected static','public static').Replace('static bool TryBegin','public static bool TryBegin')
Add-Type -TypeDefinition (@'
public class BaseWorld {
 public int Frame;
 public int GetFrameNumber(){return Frame;}
 public static implicit operator bool(BaseWorld w){return w!=null;}
}
'@ + $body)
$checks=0
function Need($value,$reason){if(!$value){throw $reason};$script:checks++}
$world=[BaseWorld]::new()
Need (![AICF_PMCEquipmentFrameBudget]::TryBegin($null)) 'Null world admitted'
Need ([AICF_PMCEquipmentFrameBudget]::TryBegin($world)) 'Frame zero denied'
for($i=0;$i -lt 100;$i++){Need (![AICF_PMCEquipmentFrameBudget]::TryBegin($world)) 'Same frame batch admitted'}
$world.Frame=1
Need ([AICF_PMCEquipmentFrameBudget]::TryBegin($world)) 'Next frame denied'
Need (![AICF_PMCEquipmentFrameBudget]::TryBegin($world)) 'Second actor admitted'
$replacement=[BaseWorld]::new();$replacement.Frame=1
Need ([AICF_PMCEquipmentFrameBudget]::TryBegin($replacement)) 'Replacement world denied'
Need (![AICF_PMCEquipmentFrameBudget]::TryBegin($replacement)) 'Replacement duplicate admitted'
if($source -notmatch 'GetID\(\) != m_iAICFPMCIdentity[\s\S]*!m_bAICFPMCEquipmentPending[\s\S]*GetPrefabName\(this\) != m_sAICFPMCSource[\s\S]*!AICF_RHSPMCEquipment.Eligible\(this\)\) return;[\s\S]*if \(!AICF_PMCEquipmentFrameBudget.TryBegin\(GetWorld\(\)\)\)\s*\{\s*GetGame\(\).GetCallqueue\(\).CallLater\(AICF_ApplyPMCEquipment, 50, false\);\s*return;\s*\}\s*AICF_RHSPMCEquipment.Apply\(this\);'){throw 'Deferred callback lost identity/eligibility or fallthrough guard'}
if($source -notmatch 'Remove\(AICF_ApplyPMCEquipment\)'){throw 'Missing cleanup'}
"PASS actual production frame budget $checks cases; callback identity/defer/cleanup guards"
