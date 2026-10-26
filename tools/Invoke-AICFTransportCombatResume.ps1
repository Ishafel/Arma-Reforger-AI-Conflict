[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
 [ValidateRange(1,65535)][int]$ServerPort=2528,
 [switch]$PrepareOnly
)
$ErrorActionPreference='Stop'
$taskRepository=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
if(Test-Path -LiteralPath $taskEvidence){throw 'EvidenceRoot must be new'}
[void][IO.Directory]::CreateDirectory($taskEvidence)
$taskStage=Join-Path $taskEvidence 'stage'
$taskAddons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
& git -C $taskRepository archive -o (Join-Path $taskEvidence 'source.zip') HEAD -- $taskAddons
if($LASTEXITCODE -ne 0){throw 'Cannot archive source'}
Expand-Archive -LiteralPath (Join-Path $taskEvidence 'source.zip') -DestinationPath $taskStage
$taskChanges=@(& git -C $taskRepository diff --name-only HEAD -- $taskAddons)
if($LASTEXITCODE -ne 0){throw 'Cannot inspect source changes'}
$taskChanges+=@(& git -C $taskRepository ls-files --others --exclude-standard -- $taskAddons)
if($LASTEXITCODE -ne 0){throw 'Cannot inspect untracked source'}
foreach($taskFile in ($taskChanges|Sort-Object -Unique)){
 $taskSource=Join-Path $taskRepository $taskFile
 if(!(Test-Path -LiteralPath $taskSource -PathType Leaf)){throw "Deleted source unsupported: $taskFile"}
 $taskDestination=Join-Path $taskStage $taskFile
 [void][IO.Directory]::CreateDirectory((Split-Path -Parent $taskDestination))
 Copy-Item -LiteralPath $taskSource -Destination $taskDestination -Force
}
$taskFixture='tests/fixtures/AICF_TransportCombatResumeProbe.c'
Copy-Item -LiteralPath (Join-Path $taskRepository $taskFixture) -Destination (Join-Path $taskStage 'AIConflictCore/Scripts/Game/AIConflict/AICF_TransportCombatResumeProbe.c')
function Get-TaskSourceHashes {
 @(Get-ChildItem -LiteralPath $taskStage -Recurse -File|Where-Object{$_.Extension -in @('.c','.conf','.st','.gproj','.et','.meta','.layout','.imageset')}|Sort-Object FullName|ForEach-Object{
  [pscustomobject]@{path=$_.FullName.Substring($taskStage.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
 })
}
$taskHashes=Get-TaskSourceHashes
$taskHashes|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-hashes-before.json')
[ordered]@{commit=(& git -C $taskRepository rev-parse HEAD);status=@(& git -C $taskRepository status --short --branch);changes=$taskChanges;fixture=$taskFixture;fixture_sha256=(Get-FileHash -LiteralPath (Join-Path $taskRepository $taskFixture)).Hash;stage=$taskStage;mode='FAULT_INJECTED';normal_war=$false}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-manifest.json')
if($PrepareOnly){"TRANSPORT_COMBAT_RESUME_PREPARED=$taskStage";exit 0}
$taskPlan=@(& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Client -Variant Stock -RepositoryRoot $taskStage -DryRun)
if($LASTEXITCODE -ne 0){throw 'Cannot plan compile'}
$taskPlan|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-plan.txt')
$taskManifest=([regex]::Match(($taskPlan -join "`n"),'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value)|ConvertFrom-Json
$taskArgs=$taskManifest.arguments
$taskProject=$taskArgs[[array]::IndexOf($taskArgs,'-gproj')+1]
$taskAddonDir=$taskArgs[[array]::IndexOf($taskArgs,'-addonsDir')+1]
$taskAddonNames=$taskArgs[[array]::IndexOf($taskArgs,'-addons')+1]
$taskWorkbench='C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe'
& $taskWorkbench -noThrow -wbsilent -gproj $taskProject -addonsDir $taskAddonDir -addons $taskAddonNames -logsDir (Join-Path $taskEvidence 'workbench') -wbModule=ScriptEditor -run -validate|Out-Null
$taskCompileExit=$LASTEXITCODE
$taskCompileExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-exit.txt')
$taskCompile=Get-Content -LiteralPath (Join-Path $taskEvidence 'workbench/script.log') -Raw
if($taskCompileExit -ne 0 -or !$taskCompile.Contains('Script validation successful.') -or $taskCompile -match 'SCRIPT\s+\([EF]\)'){throw 'Probe compilation failed; full logs saved'}
$taskProfile=Join-Path $taskEvidence 'profile'
$taskLauncher=Join-Path $taskEvidence 'launcher.txt'
& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode USSR -RepositoryRoot $taskStage -ProfileRoot $taskProfile -ServerPort $ServerPort -AdditionalArguments @('-aicfTransportCombatResume','1','-aicfGroupTrace','1','-aicfGroupTraceFaction','USSR','-aicfGroupTraceSlot','0','-addr',"127.0.0.1:$ServerPort") *> $taskLauncher
$taskNativeExit=$LASTEXITCODE
$taskExit=Join-Path $taskEvidence 'native-exit.txt'
$taskNativeExit|Set-Content -LiteralPath $taskExit
$taskConsole=@(Get-ChildItem -LiteralPath (Join-Path $taskProfile 'logs') -Recurse -Filter console.log)
if($taskConsole.Count -ne 1){throw 'Expected exactly one fresh native console.log'}
& (Join-Path $taskRepository 'tests/log-audits/Test-TransportCombatResumeLog.ps1') -LogPath $taskConsole[0].FullName -LauncherPath $taskLauncher -NativeExitPath $taskExit *> (Join-Path $taskEvidence 'audit.txt')
$taskAuditExit=$LASTEXITCODE
$taskAuditExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'audit-exit.txt')
$taskAfter=Get-TaskSourceHashes
$taskAfter|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-hashes-after.json')
$taskImmutable=!(Compare-Object ($taskHashes|ConvertTo-Json -Compress) ($taskAfter|ConvertTo-Json -Compress))
[ordered]@{native_exit=$taskNativeExit;audit_exit=$taskAuditExit;source_immutable=$taskImmutable;console=$taskConsole[0].FullName;stage=$taskStage;mode='FAULT_INJECTED';normal_war=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskEvidence 'summary.json')
"TRANSPORT_COMBAT_RESUME_EVIDENCE=$taskEvidence"
if(!$taskImmutable -or $taskNativeExit -ne 0 -or $taskAuditExit -ne 0){exit 1}
exit 0
