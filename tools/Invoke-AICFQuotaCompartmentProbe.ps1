#requires -Version 7.0
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [ValidateSet('Prepare','Run','Full')][string]$Phase='Prepare',
 [ValidateSet('Stock','RHS','EveronRHS','EveronNorthRHS')][string]$Variant='Stock',
 [ValidateRange(1,65535)][int]$ServerPort=2680,
 [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
 [string]$SourceRoot
)
$ErrorActionPreference='Stop'
$taskRepo=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
$taskStage=Join-Path $taskEvidence 'stage'
$taskManifest=Join-Path $taskEvidence 'source-manifest.json'
$taskFrozenAudit=Join-Path $taskEvidence 'Test-AIQuotaCompartmentLog.ps1'
function Assert-CompartmentSources($manifest){
 $actual=@(foreach($addon in @('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')){Get-ChildItem -LiteralPath (Join-Path $taskStage $addon) -Recurse -File|Where-Object{$_.Extension-ne'.rdb'}})
 if($actual.Count-ne@($manifest.files).Count+1){throw 'Frozen raw source set changed'}
 foreach($row in $manifest.files){$path=Join-Path $taskStage $row.path;if(!(Test-Path -LiteralPath $path -PathType Leaf)-or(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash-ne$row.sha256){throw ('Frozen source drift '+$row.path)}}
 $fixture=Join-Path $taskStage $manifest.fixture.path
 if((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash-ne$manifest.fixture.sha256){throw 'Frozen fixture drift'}
 if((Get-FileHash -LiteralPath $taskFrozenAudit -Algorithm SHA256).Hash-ne$manifest.auditor_sha256){throw 'Frozen auditor drift'}
}
if($Phase-ne'Run'){
 if(Test-Path -LiteralPath $taskEvidence){throw 'EvidenceRoot must be new for Prepare/Full'}
 [void][IO.Directory]::CreateDirectory($taskEvidence)
 if(!$SourceRoot){$SourceRoot=$taskRepo}
 $taskSource=(Resolve-Path -LiteralPath $SourceRoot).Path
 $taskAddons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
 $taskRows=@(foreach($addon in $taskAddons){
  $folder=Join-Path $taskSource $addon
  foreach($file in (Get-ChildItem -LiteralPath $folder -Recurse -File|Where-Object{$_.Extension-ne'.rdb'}|Sort-Object FullName)){
   $relative=[IO.Path]::GetRelativePath($taskSource,$file.FullName).Replace('\','/')
   $destination=Join-Path $taskStage $relative
   [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
   Copy-Item -LiteralPath $file.FullName -Destination $destination
   $hash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
   if((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash-ne$hash){throw ('Copy drift '+$relative)}
   [pscustomobject]@{path=$relative;sha256=$hash}
  }
 })
 # This observer must be later than the production vehicle-admission modded
 # layer. Root-level scripts cannot override methods introduced in that layer.
 $taskFixture='AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_ZZQuotaCompartmentProbe.c'
 if($taskFixture-in$taskRows.path){throw 'SourceRoot must be fixture-free production'}
 Copy-Item -LiteralPath (Join-Path $taskRepo 'tests/fixtures/AICF_AIQuotaCompartmentProbe.c') -Destination (Join-Path $taskStage $taskFixture)
 Copy-Item -LiteralPath (Join-Path $taskRepo 'tests/log-audits/Test-AIQuotaCompartmentLog.ps1') -Destination $taskFrozenAudit
 $taskData=[pscustomobject]@{created=(Get-Date -Format o);source=$taskSource;stage=$taskStage;variant=$Variant;server_port=$ServerPort;raw_files=$taskRows.Count;excluded='generated .rdb only';files=$taskRows;fixture=[pscustomobject]@{path=$taskFixture;sha256=(Get-FileHash -LiteralPath (Join-Path $taskStage $taskFixture) -Algorithm SHA256).Hash};auditor_sha256=(Get-FileHash -LiteralPath $taskFrozenAudit -Algorithm SHA256).Hash}
 $taskData|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $taskManifest -Encoding UTF8
 $taskPlan=@(& (Join-Path $taskRepo 'tools/Start-AICFRuntime.ps1') -Role Client -Variant $Variant -RepositoryRoot $taskStage -DryRun)
 $taskPlan|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-plan.txt')
 $taskRuntimeManifest=([regex]::Match(($taskPlan-join"`n"),'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value)|ConvertFrom-Json
 $taskArgs=$taskRuntimeManifest.arguments
 $project=$taskArgs[[array]::IndexOf($taskArgs,'-gproj')+1]
 $addonsDir=$taskArgs[[array]::IndexOf($taskArgs,'-addonsDir')+1]
 $addons=$taskArgs[[array]::IndexOf($taskArgs,'-addons')+1]
 & 'C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe' -noThrow -wbsilent -gproj $project -addonsDir $addonsDir -addons $addons -logsDir (Join-Path $taskEvidence 'workbench') -wbModule=ScriptEditor -run -validate|Out-Null
 $taskCompileExit=$LASTEXITCODE
 $taskCompileExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-exit.txt')
 $taskCompile=[IO.File]::ReadAllText((Join-Path $taskEvidence 'workbench/script.log'))
 Assert-CompartmentSources $taskData
 if($taskCompileExit-ne0-or!$taskCompile.Contains('Script validation successful.')-or$taskCompile-match'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)'){throw 'Compartment fixture compilation failed; full logs retained'}
 'COMPARTMENT_PREPARED='+$taskManifest
 if($Phase-eq'Prepare'){return}
}
$taskData=Get-Content -LiteralPath $taskManifest -Raw|ConvertFrom-Json
if($taskData.variant-ne$Variant-or$taskData.server_port-ne$ServerPort){throw 'Run variant/port differs from prepared manifest'}
if((Get-Content -LiteralPath (Join-Path $taskEvidence 'compile-exit.txt') -Raw).Trim()-ne'0'){throw 'Prepared compile did not pass'}
$taskPreparedCompile=[IO.File]::ReadAllText((Join-Path $taskEvidence 'workbench/script.log'))
if(!$taskPreparedCompile.Contains('Script validation successful.')-or$taskPreparedCompile-match'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)'){throw 'Prepared full compiler log does not prove validation'}
if(Test-Path -LiteralPath (Join-Path $taskEvidence 'native-exit.txt')){throw 'Prepared native run already finished; new run requires new profile/evidence'}
Assert-CompartmentSources $taskData
$taskLaunchMarker=Join-Path $taskEvidence 'native-launch-started.json'
$taskLock=[IO.File]::Open($taskLaunchMarker,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try{
 $taskRunMetadata=[pscustomobject]@{started=(Get-Date -Format o);runner_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash;prepared_manifest_sha256=(Get-FileHash -LiteralPath $taskManifest -Algorithm SHA256).Hash;variant=$Variant;server_port=$ServerPort;canonical_launcher=(Join-Path $taskRepo 'tools/Start-AICFRuntime.ps1')}
 $taskBytes=[Text.Encoding]::UTF8.GetBytes(($taskRunMetadata|ConvertTo-Json -Depth 3))
 $taskLock.Write($taskBytes,0,$taskBytes.Length)
}finally{$taskLock.Dispose()}
$taskProfile=Join-Path $taskEvidence 'profile'
$taskStarted=[DateTimeOffset]::UtcNow
& (Join-Path $taskRepo 'tools/Start-AICFRuntime.ps1') -Role Server -Variant $Variant -RepositoryRoot $taskStage -ProfileRoot $taskProfile -IsolatedLoadoutLibrary -ServerPort $ServerPort -AICommanderMode BOTH -AdditionalArguments @('-aicfQuotaCompartmentProbe','1','-addr',"127.0.0.1:$ServerPort") *> (Join-Path $taskEvidence 'launcher.txt')
$taskNativeExit=$LASTEXITCODE
$taskNativeExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'native-exit.txt')
Assert-CompartmentSources $taskData
$taskConsole=Get-ChildItem -LiteralPath (Join-Path $taskProfile 'logs') -Recurse -Filter console.log|Select-Object -First 1
if(!$taskConsole){throw 'Full stopped native console missing'}
& $taskFrozenAudit -LogPath $taskConsole.FullName -NativeExitCode $taskNativeExit -EvidenceRoot $taskEvidence *> (Join-Path $taskEvidence 'audit.txt')
$taskAuditExit=$LASTEXITCODE
[pscustomobject]@{native_exit=$taskNativeExit;audit_exit=$taskAuditExit;started=$taskStarted.ToString('o');ended=[DateTimeOffset]::UtcNow.ToString('o');full_console=$taskConsole.FullName}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskEvidence 'summary.json')
Get-Content -LiteralPath (Join-Path $taskEvidence 'audit.txt')
if($taskNativeExit-ne0-or$taskAuditExit-ne0){exit 1}
