[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
 [ValidateRange(1,65535)][int]$ServerPort=2588,
 [switch]$BoardingNavDomainProbe
)
$ErrorActionPreference='Stop'
$taskRepository=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
if(Test-Path -LiteralPath $taskEvidence){throw 'EvidenceRoot must be new'}
[void][IO.Directory]::CreateDirectory($taskEvidence)
$taskStage=Join-Path $taskEvidence 'stage'
$taskAddons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
$taskArchive=Join-Path $taskEvidence 'source.zip'
& git -C $taskRepository archive -o $taskArchive HEAD -- $taskAddons
if($LASTEXITCODE -ne 0){throw 'Cannot archive source'}
Expand-Archive -LiteralPath $taskArchive -DestinationPath $taskStage
$taskChanges=@(& git -C $taskRepository diff --name-only HEAD -- $taskAddons)
$taskChanges+=@(& git -C $taskRepository ls-files --others --exclude-standard -- $taskAddons)
foreach($taskFile in ($taskChanges|Sort-Object -Unique)){
 $taskSource=Join-Path $taskRepository $taskFile
 if(!(Test-Path -LiteralPath $taskSource -PathType Leaf)){throw "Deleted source unsupported: $taskFile"}
 $taskDestination=Join-Path $taskStage $taskFile
 [void][IO.Directory]::CreateDirectory((Split-Path -Parent $taskDestination))
 Copy-Item -LiteralPath $taskSource -Destination $taskDestination -Force
}
foreach($taskFixture in @('AICF_VehicleScenarioProbe.c','AICF_SpawnApproachFailureProbe.c')){
 $taskFixtureName=$taskFixture
 if($taskFixture -eq 'AICF_SpawnApproachFailureProbe.c'){$taskFixtureName='AICF_ZTransportDamageFallbackProbe.c'}
 Copy-Item -LiteralPath (Join-Path $taskRepository ('tests/fixtures/'+$taskFixture)) -Destination (Join-Path $taskStage ('AIConflictCore/Scripts/Game/AIConflict/'+$taskFixtureName))
}
if($BoardingNavDomainProbe){Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/fixtures/AICF_BoardingNavDomainProbe.c') -Destination (Join-Path $taskStage 'AIConflictCore/Scripts/Game/AIConflict/AICF_ZBoardingNavDomainProbe.c')}
$taskHashes=@(Get-ChildItem -LiteralPath $taskStage -Recurse -File|Where-Object{$_.Extension -in @('.c','.conf','.st','.gproj')}|ForEach-Object{[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
$taskHashes|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-hashes.json')
[ordered]@{commit=(& git -C $taskRepository rev-parse HEAD);status=@(& git -C $taskRepository status --short);classification='CONTROLLED_FAULT_INJECTION';normal_arrival_proof=$false;server_port=$ServerPort;boarding_nav_domain_probe=[bool]$BoardingNavDomainProbe;started=(Get-Date).ToString('o')}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'context.json')
$taskPlan=@(& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Client -Variant Stock -RepositoryRoot $taskStage -DryRun)
$taskPlan|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-plan.txt')
$taskManifest=([regex]::Match(($taskPlan -join "`n"),'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value)|ConvertFrom-Json
$taskArgs=$taskManifest.arguments
$taskProject=$taskArgs[[array]::IndexOf($taskArgs,'-gproj')+1]
$taskAddonsDir=$taskArgs[[array]::IndexOf($taskArgs,'-addonsDir')+1]
$taskGraph=$taskArgs[[array]::IndexOf($taskArgs,'-addons')+1]
$taskWorkbench='C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe'
$taskCompileArgs=@('-noThrow','-wbsilent','-gproj',$taskProject,'-addonsDir',$taskAddonsDir,'-addons',$taskGraph,'-logsDir',(Join-Path $taskEvidence 'workbench'),'-wbModule=ScriptEditor','-run','-validate')
ConvertTo-Json -InputObject (@($taskWorkbench)+$taskCompileArgs)|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-command.json')
& $taskWorkbench @taskCompileArgs|Out-Null
$taskCompileExit=$LASTEXITCODE
$taskCompileExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-exit.txt')
$taskCompile=(@(Get-ChildItem -LiteralPath (Join-Path $taskEvidence 'workbench') -Recurse -File -Filter '*.log')|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
if($taskCompileExit -ne 0 -or !$taskCompile.Contains('Script validation successful.') -or $taskCompile -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|Access violation'){throw 'Spawn approach failure fixture compile failed; full logs saved'}
$taskProfile=Join-Path $taskEvidence 'profile'
$taskLauncher=Join-Path $taskEvidence 'launcher.txt'
$taskAdditional=@('-aicfVehicleScenario','uaz5','-aicfVehicleScenarioCombat','0','-aicfSpawnApproachFailureProbe','1','-aicfGroupTrace','1','-aicfGroupTraceFaction','USSR','-aicfGroupTraceSlot','0','-addr',"127.0.0.1:$ServerPort")
if($BoardingNavDomainProbe){$taskAdditional+=@('-aicfBoardingNavDomainProbe','1')}
& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode USSR -RepositoryRoot $taskStage -ProfileRoot $taskProfile -ServerPort $ServerPort -AdditionalArguments $taskAdditional *> $taskLauncher
$taskNativeExit=$LASTEXITCODE
$taskExitPath=Join-Path $taskEvidence 'native-exit.txt'
$taskNativeExit|Set-Content -LiteralPath $taskExitPath
$taskConsole=Get-ChildItem -LiteralPath (Join-Path $taskProfile 'logs') -Recurse -Filter console.log|Select-Object -First 1
if(!$taskConsole){throw 'Spawn approach failure console.log missing'}
& (Join-Path $taskRepository 'tests/log-audits/Test-SpawnApproachFailureLog.ps1') -LogPath $taskConsole.FullName -LauncherPath $taskLauncher -NativeExitPath $taskExitPath *> (Join-Path $taskEvidence 'audit.txt')
$taskAuditExit=$LASTEXITCODE
$taskAuditExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'audit-exit.txt')
$taskAuditText=[IO.File]::ReadAllText((Join-Path $taskEvidence 'audit.txt'))
$taskResourceVerdict=[regex]::Match($taskAuditText,'gate=strict_native_resources verdict=(PASS|FAIL)').Groups[1].Value
$taskOverallVerdict='FAIL'
if($taskNativeExit -eq 0 -and $taskAuditExit -eq 0 -and $taskResourceVerdict -eq 'PASS'){$taskOverallVerdict='PASS'}
foreach($taskHash in $taskHashes){if((Get-FileHash -LiteralPath $taskHash.path -Algorithm SHA256).Hash -ne $taskHash.sha256){throw 'Stage changed during spawn approach failure probe'}}
[ordered]@{classification='CONTROLLED_FAULT_INJECTION';normal_arrival_proof=$false;native_exit=$taskNativeExit;targeted_audit_exit=$taskAuditExit;strict_native_resources=$taskResourceVerdict;overall_runtime_verdict=$taskOverallVerdict;console=$taskConsole.FullName;source_drift=0;ended=(Get-Date).ToString('o')}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'summary.json')
"SPAWN_APPROACH_FAILURE_FALLBACK_EVIDENCE=$taskEvidence NATIVE_EXIT=$taskNativeExit TARGETED_AUDIT_EXIT=$taskAuditExit"
# Preserve immutable stage and full stopped profile for inspection and archival.
if($taskNativeExit -ne 0 -or $taskAuditExit -ne 0){exit 1}
exit 0
