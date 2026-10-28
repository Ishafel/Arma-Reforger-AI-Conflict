[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [ValidateCount(1,6)][ValidateSet('uaz5','ural5','armed3','solo1','retarget5','repeat5')][string[]]$Cases=@('uaz5','ural5','armed3','solo1','retarget5','repeat5'),
 [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
 [string]$StageRoot,
 [ValidateRange(1,65535)][int]$ServerPort=2478,
 [ValidateSet('USSR','BOTH')][string]$AICommanderMode='USSR',
 [ValidateSet('US','USSR')][string]$Faction='USSR',
 [switch]$Combat,
 [switch]$RetainedBehavior,
 [ValidateSet('CURRENT_RETAINED','LEGACY_COMPLETE')][string]$ReceiptPolicy='CURRENT_RETAINED',
 [ValidateSet('ROOT_ONLY','HIERARCHY')][string]$ProtectionScope='ROOT_ONLY',
	[switch]$DismountBudgetProbe,
 [switch]$BoardingNavDomainProbe,
 [switch]$SkipCompile
)
$ErrorActionPreference='Stop'
if(@($Cases|Sort-Object -Unique).Count -ne $Cases.Count){throw 'Duplicate scenario names are not allowed'}
if($Faction -eq 'US' -and ($Cases.Count -ne 1 -or $Cases[0] -ne 'repeat5')){throw 'US faction requires isolated repeat5'}
if($RetainedBehavior -and ($Combat -or $DismountBudgetProbe -or $BoardingNavDomainProbe -or $Cases.Count -ne 1 -or $Cases[0] -ne 'repeat5')){throw 'RetainedBehavior requires isolated non-combat repeat5 only'}
if($ProtectionScope -eq 'HIERARCHY' -and !$RetainedBehavior){throw 'HIERARCHY requires explicit isolated RetainedBehavior repeat5'}
if($DismountBudgetProbe -and ($Combat -or $Cases.Count -ne 1 -or $Cases[0] -ne 'ural5')){throw 'DismountBudgetProbe requires isolated non-combat ural5 only'}
# Budget fixture deliberately exercises the bounded legacy dismount flow.
if($DismountBudgetProbe){$ReceiptPolicy='LEGACY_COMPLETE'}
if($ReceiptPolicy -eq 'LEGACY_COMPLETE' -and !$DismountBudgetProbe -and !$RetainedBehavior){throw 'LEGACY_COMPLETE is historical audit only; current native scenarios require CURRENT_RETAINED'}
if($ReceiptPolicy -eq 'CURRENT_RETAINED' -and $Combat -and $Cases -contains 'repeat5'){throw 'Current repeat5 requires isolated non-combat retained behavior'}
$taskRepository=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
if(Test-Path -LiteralPath $taskEvidence){throw 'EvidenceRoot must be new'}
[void][IO.Directory]::CreateDirectory($taskEvidence)
$taskOwnStage=!$StageRoot
if($taskOwnStage){
 $StageRoot=Join-Path $taskEvidence 'stage'
 $taskAddons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
 $taskArchive=Join-Path $taskEvidence 'source.zip'
 & git -C $taskRepository archive -o $taskArchive HEAD -- $taskAddons
 if($LASTEXITCODE -ne 0){throw 'Cannot archive source'}
 Expand-Archive -LiteralPath $taskArchive -DestinationPath $StageRoot
 $taskChanges=@(& git -C $taskRepository diff --name-only HEAD -- $taskAddons)
 $taskChanges+=@(& git -C $taskRepository ls-files --others --exclude-standard -- $taskAddons)
 foreach($taskFile in ($taskChanges|Sort-Object -Unique)){
  $taskSource=Join-Path $taskRepository $taskFile
  if(!(Test-Path -LiteralPath $taskSource -PathType Leaf)){throw "Deleted source unsupported: $taskFile"}
  $taskDestination=Join-Path $StageRoot $taskFile
  [void][IO.Directory]::CreateDirectory((Split-Path -Parent $taskDestination))
  Copy-Item -LiteralPath $taskSource -Destination $taskDestination -Force
 }
 Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/fixtures/AICF_VehicleScenarioProbe.c') -Destination (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_VehicleScenarioProbe.c')
 if($DismountBudgetProbe){Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/fixtures/AICF_DismountOperationBudgetProbe.c') -Destination (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_ZDismountOperationBudgetProbe.c')}
 if($BoardingNavDomainProbe){Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/fixtures/AICF_BoardingNavDomainProbe.c') -Destination (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_ZBoardingNavDomainProbe.c')}
}
$StageRoot=(Resolve-Path -LiteralPath $StageRoot).Path
if($StageRoot -eq $taskRepository){throw 'Production checkout cannot be used as a scenario stage'}
if(!(Test-Path -LiteralPath (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_VehicleScenarioProbe.c'))){throw 'Scenario fixture missing in stage'}
if($DismountBudgetProbe -and !(Test-Path -LiteralPath (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_ZDismountOperationBudgetProbe.c'))){throw 'Dismount operation budget fixture missing in stage'}
if($BoardingNavDomainProbe -and !(Test-Path -LiteralPath (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_ZBoardingNavDomainProbe.c'))){throw 'Requested boarding/staging nav observer missing in stage'}
[pscustomobject]@{faction=$Faction;retained_behavior=[bool]$RetainedBehavior;receipt_policy=$ReceiptPolicy;protection_scope=$ProtectionScope;boarding_nav_domain_probe=[bool]$BoardingNavDomainProbe;dismount_budget_probe=[bool]$DismountBudgetProbe;cases=$Cases;stage=$StageRoot}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $taskEvidence 'probe-options.json')
$taskHashes=@(Get-ChildItem -LiteralPath $StageRoot -Recurse -File|Where-Object{$_.Extension -in @('.c','.conf','.st','.gproj')}|ForEach-Object{[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
$taskHashes|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-hashes.json')
$taskAuditor=Join-Path $taskEvidence 'Test-VehicleScenarioLog.ps1'
Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/log-audits/Test-VehicleScenarioLog.ps1') -Destination $taskAuditor
$taskAuditorHash=(Get-FileHash -LiteralPath $taskAuditor -Algorithm SHA256).Hash
$taskAuditorHash|Set-Content -LiteralPath (Join-Path $taskEvidence 'auditor-sha256.txt')
if(!$SkipCompile){
 $taskPlan=@(& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Client -Variant Stock -RepositoryRoot $StageRoot -DryRun)
 $taskPlan|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-plan.txt')
 $taskManifest=([regex]::Match(($taskPlan -join "`n"),'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value)|ConvertFrom-Json
 $taskArgs=$taskManifest.arguments
 $taskProject=$taskArgs[[array]::IndexOf($taskArgs,'-gproj')+1]
 $taskAddonsDir=$taskArgs[[array]::IndexOf($taskArgs,'-addonsDir')+1]
 $taskAddons=$taskArgs[[array]::IndexOf($taskArgs,'-addons')+1]
 $taskWorkbench='C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe'
 & $taskWorkbench -noThrow -wbsilent -gproj $taskProject -addonsDir $taskAddonsDir -addons $taskAddons -logsDir (Join-Path $taskEvidence 'workbench') -wbModule=ScriptEditor -run -validate|Out-Null
 $taskCompileExit=$LASTEXITCODE
 $taskCompileExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-exit.txt')
 $taskCompile=Get-Content -LiteralPath (Join-Path $taskEvidence 'workbench/script.log') -Raw
 if($taskCompileExit -ne 0 -or !$taskCompile.Contains('Script validation successful.') -or $taskCompile -match 'SCRIPT\s+\([EF]\)'){throw 'Scenario compilation failed; full logs saved'}
}
$taskResults=@()
foreach($taskCase in $Cases){
 $taskCaseRoot=Join-Path $taskEvidence $taskCase
 [void][IO.Directory]::CreateDirectory($taskCaseRoot)
 $taskProfile=Join-Path $taskCaseRoot 'profile'
 $taskLauncher=Join-Path $taskCaseRoot 'launcher.txt'
 $taskCombat='0';if($Combat){$taskCombat='1'}
 $taskAdditional=@('-aicfVehicleScenario',$taskCase,'-aicfVehicleScenarioFaction',$Faction,'-aicfVehicleScenarioCombat',$taskCombat,'-aicfGroupTrace','1','-aicfGroupTraceFaction',$Faction,'-aicfGroupTraceSlot','0','-addr',"127.0.0.1:$ServerPort")
 if($DismountBudgetProbe){$taskAdditional+=@('-aicfVehicleScenarioDismountBudget','1','-aicfVehicleScenarioDismountBudgetAdapter','1')}
 if($BoardingNavDomainProbe){$taskAdditional+=@('-aicfBoardingNavDomainProbe','1')}
 $taskRetained=$RetainedBehavior -or ($ReceiptPolicy -eq 'CURRENT_RETAINED' -and $taskCase -eq 'repeat5')
 if($taskRetained){$taskAdditional+=@('-aicfVehicleScenarioRetainedBehavior','1')}
 if($ReceiptPolicy -eq 'CURRENT_RETAINED' -and !$taskRetained){$taskAdditional+=@('-aicfVehicleScenarioCurrentBehavior','1')}
 if($ProtectionScope -eq 'HIERARCHY'){$taskAdditional+=@('-aicfVehicleScenarioProtectionScope',$ProtectionScope)}
 & (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode $AICommanderMode -RepositoryRoot $StageRoot -ProfileRoot $taskProfile -ServerPort $ServerPort -AdditionalArguments $taskAdditional *> $taskLauncher
 $taskNativeExit=$LASTEXITCODE
 $taskExitPath=Join-Path $taskCaseRoot 'native-exit.txt'
 $taskNativeExit|Set-Content -LiteralPath $taskExitPath
 $taskConsole=Get-ChildItem -LiteralPath (Join-Path $taskProfile 'logs') -Recurse -Filter console.log|Select-Object -First 1
 if(!$taskConsole){throw "No console.log for $taskCase"}
 & $taskAuditor -LogPath $taskConsole.FullName -LauncherPath $taskLauncher -NativeExitPath $taskExitPath -Scenario $taskCase -Faction $Faction -Combat:$Combat -DismountBudgetProbe:$DismountBudgetProbe -RetainedBehavior:$taskRetained -ReceiptPolicy $ReceiptPolicy -ProtectionScope $ProtectionScope *> (Join-Path $taskCaseRoot 'audit.txt')
 $taskAuditExit=$LASTEXITCODE
 $taskAuditExit|Set-Content -LiteralPath (Join-Path $taskCaseRoot 'audit-exit.txt')
 $taskResults+=[pscustomobject]@{scenario=$taskCase;faction=$Faction;native_exit=$taskNativeExit;audit_exit=$taskAuditExit;console=$taskConsole.FullName;launcher=$taskLauncher}
 $taskResults|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'summary.json')
 "SCENARIO_FINISHED=$taskCase NATIVE_EXIT=$taskNativeExit AUDIT_EXIT=$taskAuditExit"
 foreach($taskHash in $taskHashes){if((Get-FileHash -LiteralPath $taskHash.path -Algorithm SHA256).Hash -ne $taskHash.sha256){throw 'Stage changed during scenario matrix'}}
 if((Get-FileHash -LiteralPath $taskAuditor -Algorithm SHA256).Hash -ne $taskAuditorHash){throw 'Frozen scenario auditor changed'}
}
# Stage/profiles с полными логами остаются в evidence для разбора. Не изменяйте
# stage, пока работает хоть один peer; после разбора архивируйте и уберите его.
"VEHICLE_SCENARIOS_EVIDENCE=$taskEvidence"
if(@($taskResults|Where-Object{$_.native_exit -ne 0 -or $_.audit_exit -ne 0}).Count){exit 1}
exit 0
