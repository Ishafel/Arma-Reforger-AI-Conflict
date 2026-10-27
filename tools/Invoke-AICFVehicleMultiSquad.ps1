[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [Parameter(Mandatory)][ValidateSet('NORMAL','CAPACITY')][string]$Mode,
 [ValidateSet('MOVING','MOUNTED_STAGING')][string]$ConcurrencyMode='MOVING',
 [ValidateRange(1,3600)][int]$DurationSeconds=1800,
 [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
 [string]$SourceRef='HEAD',
 [switch]$SnapshotWorkingTree,
 [string]$StageRoot,
 [switch]$SkipCompile,
 [switch]$PrepareOnly,
 [ValidateRange(0,65535)][int]$ServerPort=0
)
$ErrorActionPreference='Stop'
if($ConcurrencyMode -eq 'MOUNTED_STAGING' -and $Mode -ne 'CAPACITY'){throw 'MOUNTED_STAGING is a separate controlled CAPACITY test only'}
$taskClassification='NORMAL_CAMPAIGN'
$taskMusterPolicy='PER_GROUP_BEFORE_VEHICLE'
if($Mode -eq 'CAPACITY'){$taskClassification='CONTROLLED_CAPACITY_ONLY'}
if($ConcurrencyMode -eq 'MOUNTED_STAGING'){$taskClassification='CAPACITY_MOUNTED_ONLY'}
. (Join-Path $PSScriptRoot 'AICF_VehicleSoakSources.ps1')
$taskRepository=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
if(Test-Path -LiteralPath $taskEvidence){throw 'EvidenceRoot must be new'}
if(!$ServerPort){$ServerPort=2520;if($Mode -eq 'CAPACITY'){$ServerPort=2521}}
$taskOwnStage=!$StageRoot
if($SkipCompile -and $taskOwnStage){throw 'SkipCompile requires a separately compiled stage'}
[void][IO.Directory]::CreateDirectory($taskEvidence)
$taskCommit=(& git -C $taskRepository rev-parse --verify "$SourceRef^{commit}").Trim()
if($LASTEXITCODE -ne 0){throw 'Cannot resolve source commit'}
$taskWorkingTree=[bool]$SnapshotWorkingTree
$taskOriginManifest=$null
if($taskOwnStage){
 $StageRoot=Join-Path $taskEvidence 'stage'
 $taskAddons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
 $taskArchive=Join-Path $taskEvidence 'source.zip'
 & git -C $taskRepository archive -o $taskArchive $taskCommit -- $taskAddons
 if($LASTEXITCODE -ne 0){throw 'Cannot archive source'}
 Expand-Archive -LiteralPath $taskArchive -DestinationPath $StageRoot
 if($SnapshotWorkingTree){
  $taskChanges=@(& git -C $taskRepository diff --name-only $taskCommit -- $taskAddons)
  $taskChanges+=@(& git -C $taskRepository ls-files --others --exclude-standard -- $taskAddons)
  foreach($taskRelative in ($taskChanges|Sort-Object -Unique)){
   $taskSource=Join-Path $taskRepository $taskRelative
   if(!(Test-Path -LiteralPath $taskSource -PathType Leaf)){throw "Deleted source unsupported: $taskRelative"}
   $taskDestination=Join-Path $StageRoot $taskRelative
   [void][IO.Directory]::CreateDirectory((Split-Path -Parent $taskDestination))
   Copy-Item -LiteralPath $taskSource -Destination $taskDestination -Force
  }
 }
 Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/fixtures/AICF_VehicleMultiSquadProbe.c') -Destination (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_VehicleMultiSquadProbe.c')
}else{
 $StageRoot=(Resolve-Path -LiteralPath $StageRoot).Path
 $taskOriginManifest=Join-Path (Split-Path -Parent $StageRoot) 'source-manifest.json'
 if(!(Test-Path -LiteralPath $taskOriginManifest)){throw 'Shared stage requires its original source manifest'}
 $taskOrigin=Get-Content -LiteralPath $taskOriginManifest -Raw|ConvertFrom-Json
 if((Resolve-Path -LiteralPath $taskOrigin.stage).Path -ne $StageRoot){throw 'Shared stage manifest path mismatch'}
 $taskCommit=$taskOrigin.base_commit
 $taskWorkingTree=[bool]$taskOrigin.working_tree
 Assert-AICFVehicleSoakSources $StageRoot @($taskOrigin.files)
 if($SkipCompile){
  $taskCompileEvidence=Split-Path -Parent $taskOriginManifest
  $taskOldExit=Get-Content -LiteralPath (Join-Path $taskCompileEvidence 'compile-exit.txt') -Raw
  $taskOldCompile=Get-Content -LiteralPath (Join-Path $taskCompileEvidence 'workbench/script.log') -Raw
  if($taskOldExit.Trim() -ne '0' -or !$taskOldCompile.Contains('Script validation successful.') -or $taskOldCompile -match 'SCRIPT\s+\([EF]\)'){throw 'Shared stage has no successful compile evidence'}
 }
}
$StageRoot=(Resolve-Path -LiteralPath $StageRoot).Path
if($StageRoot -eq $taskRepository){throw 'Production checkout cannot be a runtime stage'}
$taskCoreScripts=Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict'
if(!(Test-Path -LiteralPath (Join-Path $taskCoreScripts 'AICF_VehicleMultiSquadProbe.c'))){throw 'Multi-squad fixture missing'}
if(!([IO.File]::ReadAllText((Join-Path $taskCoreScripts 'AICF_VehicleMultiSquadProbe.c'))).Contains('muster_policy=PER_GROUP_BEFORE_VEHICLE')){throw 'Compiled stage fixture does not support the PER_GROUP_BEFORE_VEHICLE muster policy'}
if($ConcurrencyMode -eq 'MOUNTED_STAGING' -and !([IO.File]::ReadAllText((Join-Path $taskCoreScripts 'AICF_VehicleMultiSquadProbe.c'))).Contains('aicfVehicleMultiConcurrencyMode')){throw 'Compiled stage fixture does not support the separate MOUNTED_STAGING oracle'}
foreach($taskOther in @('AICF_VehicleSoakProbe.c','AICF_VehicleScenarioProbe.c')){
 if(Test-Path -LiteralPath (Join-Path $taskCoreScripts $taskOther)){throw 'Competing transport fixture in runtime stage'}
}
$taskHashes=@(Get-AICFVehicleSoakSources $StageRoot)
[pscustomobject]@{base_commit=$taskCommit;working_tree=$taskWorkingTree;origin_manifest=$taskOriginManifest;stage=$StageRoot;mode=$Mode;concurrency_mode=$ConcurrencyMode;classification=$taskClassification;notmovingproof=($ConcurrencyMode -eq 'MOUNTED_STAGING');muster_policy=$taskMusterPolicy;duration_seconds=$DurationSeconds;groups_per_faction=6;cohorts=@(2,4,6);capacity_grant_budget_per_faction=4000;normal_campaign_evidence=($Mode -eq 'NORMAL');files=$taskHashes}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-manifest.json')
if(!$SkipCompile){
 $taskPlan=@(& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Client -Variant Stock -RepositoryRoot $StageRoot -DryRun)
 $taskPlan|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-plan.txt')
 $taskManifest=([regex]::Match(($taskPlan -join "`n"),'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value)|ConvertFrom-Json
 $taskArgs=$taskManifest.arguments
 $taskProject=$taskArgs[[array]::IndexOf($taskArgs,'-gproj')+1]
 $taskAddonsDir=$taskArgs[[array]::IndexOf($taskArgs,'-addonsDir')+1]
 $taskAddons=$taskArgs[[array]::IndexOf($taskArgs,'-addons')+1]
 & 'C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe' -noThrow -wbsilent -gproj $taskProject -addonsDir $taskAddonsDir -addons $taskAddons -logsDir (Join-Path $taskEvidence 'workbench') -wbModule=ScriptEditor -run -validate|Out-Null
 $taskCompileExit=$LASTEXITCODE
 $taskCompileExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-exit.txt')
 $taskCompile=Get-Content -LiteralPath (Join-Path $taskEvidence 'workbench/script.log') -Raw
 if($taskCompileExit -ne 0 -or !$taskCompile.Contains('Script validation successful.') -or $taskCompile -match 'SCRIPT\s+\([EF]\)'){throw 'Multi-squad compile failed; full logs saved'}
}
if($PrepareOnly){
 Assert-AICFVehicleSoakSources $StageRoot $taskHashes
 "MULTI_PREPARED_STAGE=$StageRoot MODE=$Mode CONCURRENCY_MODE=$ConcurrencyMode CLASSIFICATION=$taskClassification NATIVE_RUN=NOT_RUN"
 exit 0
}
$taskProfile=Join-Path $taskEvidence 'profile'
"MULTI_PROFILE=$taskProfile MODE=$Mode CONCURRENCY_MODE=$ConcurrencyMode CLASSIFICATION=$taskClassification GROUPS_PER_FACTION=6 DURATION_SECONDS=$DurationSeconds"
$taskStarted=[DateTimeOffset]::UtcNow
& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode BOTH -RepositoryRoot $StageRoot -ProfileRoot $taskProfile -IsolatedLoadoutLibrary -ServerPort $ServerPort -AdditionalArguments @('-aicfVehicleMultiMode',$Mode,'-aicfVehicleMultiConcurrencyMode',$ConcurrencyMode,'-aicfVehicleMultiDurationMs',([string]($DurationSeconds*1000)),'-aicfGroupTrace','1','-addr',"127.0.0.1:$ServerPort") *> (Join-Path $taskEvidence 'launcher.txt')
$taskNativeExit=$LASTEXITCODE
$taskEnded=[DateTimeOffset]::UtcNow
$taskNativeExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'native-exit.txt')
[pscustomobject]@{started_utc=$taskStarted.ToString('o');ended_utc=$taskEnded.ToString('o');wall_seconds=($taskEnded-$taskStarted).TotalSeconds;requested_seconds=$DurationSeconds;mode=$Mode;concurrency_mode=$ConcurrencyMode;muster_policy=$taskMusterPolicy}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskEvidence 'timing.json')
Assert-AICFVehicleSoakSources $StageRoot $taskHashes
$taskConsole=Get-ChildItem -LiteralPath (Join-Path $taskProfile 'logs') -Recurse -Filter console.log|Select-Object -First 1
if(!$taskConsole){throw 'No full console log'}
& (Join-Path $taskRepository 'tests/log-audits/Test-VehicleMultiSquadLog.ps1') -LogPath $taskConsole.FullName -EvidenceRoot $taskEvidence -Mode $Mode -ConcurrencyMode $ConcurrencyMode -MinimumSeconds $DurationSeconds *> (Join-Path $taskEvidence 'audit.txt')
$taskAuditExit=$LASTEXITCODE
[pscustomobject]@{mode=$Mode;concurrency_mode=$ConcurrencyMode;classification=$taskClassification;notmovingproof=($ConcurrencyMode -eq 'MOUNTED_STAGING');muster_policy=$taskMusterPolicy;native_exit=$taskNativeExit;audit_exit=$taskAuditExit;console=$taskConsole.FullName;wall_seconds=($taskEnded-$taskStarted).TotalSeconds;normal_campaign_evidence=($Mode -eq 'NORMAL')}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskEvidence 'summary.json')
"MULTI_FINISHED=$Mode CONCURRENCY_MODE=$ConcurrencyMode CLASSIFICATION=$taskClassification NATIVE_EXIT=$taskNativeExit AUDIT_EXIT=$taskAuditExit"
if($taskNativeExit -ne 0 -or $taskAuditExit -ne 0){exit 1}
exit 0
