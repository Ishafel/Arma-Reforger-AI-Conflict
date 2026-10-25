[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [ValidateCount(1,6)][ValidateSet('uaz5','ural5','armed3','solo1','retarget5','repeat5')][string[]]$Cases=@('uaz5','ural5','armed3','solo1','retarget5','repeat5'),
 [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
 [string]$StageRoot,
 [ValidateRange(1,65535)][int]$ServerPort=2478,
 [ValidateSet('USSR','BOTH')][string]$AICommanderMode='USSR',
 [switch]$Combat,
 [switch]$SkipCompile
)
$ErrorActionPreference='Stop'
if(@($Cases|Sort-Object -Unique).Count -ne $Cases.Count){throw 'Duplicate scenario names are not allowed'}
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
}
$StageRoot=(Resolve-Path -LiteralPath $StageRoot).Path
if($StageRoot -eq $taskRepository){throw 'Production checkout cannot be used as a scenario stage'}
if(!(Test-Path -LiteralPath (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_VehicleScenarioProbe.c'))){throw 'Scenario fixture missing in stage'}
$taskHashes=@(Get-ChildItem -LiteralPath $StageRoot -Recurse -File|Where-Object{$_.Extension -in @('.c','.conf','.st','.gproj')}|ForEach-Object{[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
$taskHashes|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-hashes.json')
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
 & (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode $AICommanderMode -RepositoryRoot $StageRoot -ProfileRoot $taskProfile -ServerPort $ServerPort -AdditionalArguments @('-aicfVehicleScenario',$taskCase,'-aicfVehicleScenarioCombat',$taskCombat,'-aicfGroupTrace','1','-aicfGroupTraceFaction','USSR','-aicfGroupTraceSlot','0','-addr',"127.0.0.1:$ServerPort") *> $taskLauncher
 $taskNativeExit=$LASTEXITCODE
 $taskExitPath=Join-Path $taskCaseRoot 'native-exit.txt'
 $taskNativeExit|Set-Content -LiteralPath $taskExitPath
 $taskConsole=Get-ChildItem -LiteralPath (Join-Path $taskProfile 'logs') -Recurse -Filter console.log|Select-Object -First 1
 if(!$taskConsole){throw "No console.log for $taskCase"}
 & (Join-Path $taskRepository 'tests/log-audits/Test-VehicleScenarioLog.ps1') -LogPath $taskConsole.FullName -LauncherPath $taskLauncher -NativeExitPath $taskExitPath -Scenario $taskCase -Combat:$Combat *> (Join-Path $taskCaseRoot 'audit.txt')
 $taskAuditExit=$LASTEXITCODE
 $taskAuditExit|Set-Content -LiteralPath (Join-Path $taskCaseRoot 'audit-exit.txt')
 $taskResults+=[pscustomobject]@{scenario=$taskCase;native_exit=$taskNativeExit;audit_exit=$taskAuditExit;console=$taskConsole.FullName;launcher=$taskLauncher}
 $taskResults|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'summary.json')
 "SCENARIO_FINISHED=$taskCase NATIVE_EXIT=$taskNativeExit AUDIT_EXIT=$taskAuditExit"
 foreach($taskHash in $taskHashes){if((Get-FileHash -LiteralPath $taskHash.path -Algorithm SHA256).Hash -ne $taskHash.sha256){throw 'Stage changed during scenario matrix'}}
}
# Stage/profiles с полными логами остаются в evidence для разбора. Не изменяйте
# stage, пока работает хоть один peer; после разбора архивируйте и уберите его.
"VEHICLE_SCENARIOS_EVIDENCE=$taskEvidence"
if(@($taskResults|Where-Object{$_.native_exit -ne 0 -or $_.audit_exit -ne 0}).Count){exit 1}
exit 0
