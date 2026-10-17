param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/Stage3StaticAudit.Common.ps1')
$bootstrap = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictArland/Scripts/Game/AIConflictArland/Integration/AICF_ArlandCampaignBootstrap.c') -Raw
$controller = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'AIConflictCore/Scripts/Game/AIConflict/Bootstrap/AICF_MatchController.c') -Raw
function Body([string]$source, [string]$name) {
    Get-AICFMethodBody ([pscustomobject]@{Source=$source; Code=(ConvertTo-AICFCodeText $source)}) $name
}
function Test-Shutdown([string]$boot, [string]$match) {
    $failures = [Collections.Generic.List[string]]::new()
    $end = ConvertTo-AICFCodeText (Body $boot 'OnGameEnd')
    Assert-AICFContains $failures 'SHUTDOWN_BEFORE_NATIVE' $end 'm_AICFMatchController\.Shutdown\(\);[\s\S]*super\.OnGameEnd\(\)' 'Match services must stop while native entities and faction data still exist'
    Assert-AICFContains $failures 'SHUTDOWN_RELEASE_OWNER' $end 'm_AICFMatchController\.Shutdown\(\);\s*m_AICFMatchController = null;' 'Bootstrap must release its controller after stopping it'
    $shutdown = ConvertTo-AICFCodeText (Body $match 'Shutdown')
    Assert-AICFContains $failures 'SHUTDOWN_NATIVE_ENTITY_OWNER' $shutdown '^\s*\{\s*Stop\(false\);\s*\}\s*$' 'World teardown keeps native ownership of gameplay entity deletion'
    $stop = ConvertTo-AICFCodeText (Body $match 'Stop')
    Assert-AICFContains $failures 'SHUTDOWN_IDEMPOTENT' $stop 'if \(m_bStopped\)\s*return;[\s\S]*m_bStopped = true;' 'Repeated end callbacks must not repeat side effects'
    Assert-AICFContains $failures 'SHUTDOWN_EQUIPMENT' $stop 'm_EquipmentService\.Stop\(\);\s*m_EquipmentService = null;' 'EquipmentPlan owners must release their preview worlds before teardown'
    return $failures.ToArray()
}
$failures = @(Test-Shutdown $bootstrap $controller)
if ($failures.Count) { $failures; exit 1 }
$cases = @(
    @{Name='missing-shutdown'; Boot=$bootstrap.Replace('m_AICFMatchController.Shutdown();',''); Match=$controller; Rule='SHUTDOWN_BEFORE_NATIVE'},
    @{Name='late-shutdown'; Boot=$bootstrap.Replace('m_AICFMatchController.Shutdown();','').Replace('super.OnGameEnd();','super.OnGameEnd(); m_AICFMatchController.Shutdown();'); Match=$controller; Rule='SHUTDOWN_BEFORE_NATIVE'},
    @{Name='retained-controller'; Boot=$bootstrap.Replace('m_AICFMatchController = null;',''); Match=$controller; Rule='SHUTDOWN_RELEASE_OWNER'},
    @{Name='entity-delete-on-shutdown'; Boot=$bootstrap; Match=$controller.Replace("void Shutdown()`r`n`t{`r`n`t`tStop(false);", "void Shutdown()`r`n`t{`r`n`t`tStop(true);").Replace("void Shutdown()`n`t{`n`t`tStop(false);", "void Shutdown()`n`t{`n`t`tStop(true);"); Rule='SHUTDOWN_NATIVE_ENTITY_OWNER'},
    @{Name='retained-plans'; Boot=$bootstrap; Match=$controller.Replace('m_EquipmentService.Stop();',''); Rule='SHUTDOWN_EQUIPMENT'}
)
foreach ($case in $cases) {
    $rejected = @(Test-Shutdown $case.Boot $case.Match)
    if (-not ($rejected -match ('\[' + $case.Rule + '\]'))) { throw "Mutation escaped: $($case.Name)" }
    Write-Output "PASS negative mutation=$($case.Name) rejected_by=$($case.Rule)"
}
Write-Output 'Match shutdown contracts: PASS; negative mutations=5; runtime is a separate gate'
exit 0
