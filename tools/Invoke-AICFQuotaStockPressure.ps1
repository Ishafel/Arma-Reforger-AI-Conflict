#requires -Version 7.0
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [ValidateSet('Prepare','Run','Full')][string]$Phase='Prepare',
 [ValidateRange(1,65535)][int]$ServerPort=2660,
 [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
 [string]$SourceRoot
)
$ErrorActionPreference='Stop'
$taskRepo=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
$taskStage=Join-Path $taskEvidence 'stage'
$taskManifest=Join-Path $taskEvidence 'source-manifest.json'
$taskFrozenAudit=Join-Path $taskEvidence 'Test-AIQuotaStockPressureLog.ps1'
$taskAddons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')

function Assert-AICFPressureSources($manifest){
 $expected=@($manifest.files)+@($manifest.fixtures)
 $actual=@(foreach($addon in $taskAddons){Get-ChildItem -LiteralPath (Join-Path $taskStage $addon) -Recurse -File|Where-Object Extension -ne '.rdb'})
 if($actual.Count-ne$expected.Count){throw 'Frozen source exact set changed'}
 foreach($row in $expected){
  $path=Join-Path $taskStage $row.path
  if(!(Test-Path -LiteralPath $path -PathType Leaf)-or(Get-FileHash -LiteralPath $path).Hash-ne$row.sha256){throw ('Frozen source SHA changed '+$row.path)}
 }
 if((Get-FileHash -LiteralPath $taskFrozenAudit).Hash-ne$manifest.auditor_sha256){throw 'Frozen auditor SHA changed'}
}

function Get-AICFPressureLogFindings([string]$Text){
 $fatal=@();$project=@();$resources=@();$number=0
 foreach($line in ($Text-split'\r?\n')){
  $number++;$row=[pscustomobject]@{line=$number;text=$line}
  if($line-match'SCRIPT\s*\([EF]\)|GUI\s*\([EF]\)|\b[A-Z][A-Z0-9_]*\s*\(F\)\s*:|Application crashed|NULL pointer|Access violation|Assertion failed|Unhandled exception|Virtual Machine Exception'){$fatal+=$row}
  elseif($line-match'\([EF]\)\s*:'){$resources+=$row}
  if($line-match'\[AICF\].*\[(ERROR|FAIL)\]'){$project+=$row}
 }
 [pscustomobject]@{fatal=$fatal;project=$project;resources=$resources}
}

if($Phase-ne'Run'){
 if(Test-Path -LiteralPath $taskEvidence){throw 'Prepare/Full requires a new evidence directory'}
 [void][IO.Directory]::CreateDirectory($taskEvidence)
 if(!$SourceRoot){$SourceRoot=$taskRepo}
 $taskSource=(Resolve-Path -LiteralPath $SourceRoot).Path
 $taskRows=@(foreach($addon in $taskAddons){
  foreach($file in (Get-ChildItem -LiteralPath (Join-Path $taskSource $addon) -Recurse -File|Where-Object Extension -ne '.rdb'|Sort-Object FullName)){
   $relative=[IO.Path]::GetRelativePath($taskSource,$file.FullName).Replace('\','/')
   $destination=Join-Path $taskStage $relative
   [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
   $hash=(Get-FileHash -LiteralPath $file.FullName).Hash
   Copy-Item -LiteralPath $file.FullName -Destination $destination
   if((Get-FileHash -LiteralPath $destination).Hash-ne$hash-or(Get-FileHash -LiteralPath $file.FullName).Hash-ne$hash){throw ('Snapshot copy drift '+$relative)}
   [pscustomobject]@{path=$relative;sha256=$hash}
  }
 })
 $taskFixtures=@(foreach($pair in @(
  @('AICF_AIQuotaStockPressureProbe.c','AICF_ZQuotaStockPressureProbe.c'),
  @('AICF_AIQuotaBootReadbackProbe.c','AICF_ZQuotaBootReadback.c')
 )){
  $relative='AIConflictCore/Scripts/Game/AIConflict/'+$pair[1]
  if($relative-in$taskRows.path){throw 'SourceRoot must be fixture-free production'}
  $destination=Join-Path $taskStage $relative
  Copy-Item -LiteralPath (Join-Path $taskRepo ('tests/fixtures/'+$pair[0])) -Destination $destination
  [pscustomobject]@{path=$relative;sha256=(Get-FileHash -LiteralPath $destination).Hash}
 })
 Copy-Item -LiteralPath (Join-Path $taskRepo 'tests/log-audits/Test-AIQuotaStockPressureLog.ps1') -Destination $taskFrozenAudit
 $taskData=[pscustomobject]@{created=(Get-Date -Format o);scope='STOCK_CHARACTER_MANAGED_RECRUIT_ONLY';source=$taskSource;stage=$taskStage;variant='Stock';server_port=$ServerPort;raw_files=$taskRows.Count;files=$taskRows;fixtures=$taskFixtures;excluded='generated .rdb only';auditor_sha256=(Get-FileHash -LiteralPath $taskFrozenAudit).Hash}
 $taskData|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $taskManifest -Encoding utf8
 $taskPlan=@(& (Join-Path $taskRepo 'tools/Start-AICFRuntime.ps1') -Role Client -Variant Stock -RepositoryRoot $taskStage -DryRun)
 $taskPlan|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-plan.txt')
 $taskRuntime=([regex]::Match(($taskPlan-join"`n"),'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value)|ConvertFrom-Json
 $taskArgs=$taskRuntime.arguments
 $project=$taskArgs[[array]::IndexOf($taskArgs,'-gproj')+1]
 $addonsDir=$taskArgs[[array]::IndexOf($taskArgs,'-addonsDir')+1]
 $addons=$taskArgs[[array]::IndexOf($taskArgs,'-addons')+1]
 & 'C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe' -noThrow -wbsilent -gproj $project -addonsDir $addonsDir -addons $addons -logsDir (Join-Path $taskEvidence 'workbench') -wbModule=ScriptEditor -run -validate|Out-Null
 $taskExit=$LASTEXITCODE
 $taskCompilerLogs=@(Get-ChildItem -LiteralPath (Join-Path $taskEvidence 'workbench') -File -Filter '*.log')
 $taskCompilerFatal=@(foreach($file in $taskCompilerLogs){(Get-AICFPressureLogFindings ([IO.File]::ReadAllText($file.FullName))).fatal})
 $taskValidated=[IO.File]::ReadAllText((Join-Path $taskEvidence 'workbench/script.log')).Contains('Script validation successful.')
 Assert-AICFPressureSources $taskData
 [pscustomobject]@{native_exit=$taskExit;validated=$taskValidated;full_fatal=$taskCompilerFatal.Count;source_manifest_sha256=(Get-FileHash -LiteralPath $taskManifest).Hash}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-verdict.json')
 if($taskExit-ne0-or!$taskValidated-or$taskCompilerFatal.Count){throw 'Pressure fixture compile failed; full logs retained'}
 if($Phase-eq'Prepare'){return}
}
$taskData=Get-Content -LiteralPath $taskManifest -Raw|ConvertFrom-Json
$taskCompile=Get-Content -LiteralPath (Join-Path $taskEvidence 'compile-verdict.json') -Raw|ConvertFrom-Json
if($taskData.variant-ne'Stock'-or$taskData.server_port-ne$ServerPort){throw 'Prepared variant/port changed'}
if($taskCompile.native_exit-ne0-or!$taskCompile.validated-or$taskCompile.full_fatal-ne0-or$taskCompile.source_manifest_sha256-ne(Get-FileHash -LiteralPath $taskManifest).Hash){throw 'Compile is not bound to this frozen source manifest'}
$taskCompileLogs=@(Get-ChildItem -LiteralPath (Join-Path $taskEvidence 'workbench') -File -Filter '*.log')
$taskPreparedFatal=@(foreach($file in $taskCompileLogs){(Get-AICFPressureLogFindings ([IO.File]::ReadAllText($file.FullName))).fatal})
if($taskPreparedFatal.Count-or![IO.File]::ReadAllText((Join-Path $taskEvidence 'workbench/script.log')).Contains('Script validation successful.')){throw 'Prepared full compiler logs no longer prove validation'}
Assert-AICFPressureSources $taskData
$taskNative=@(Get-CimInstance Win32_Process|Where-Object{$_.Name-like'ArmaReforger*.exe'-and$_.Name-notlike'*Workbench*'})
$taskFree=(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1MB
if($taskNative.Count-ge2-or$taskFree-le8){throw 'Native max-two/RAM-above-8GiB preflight failed'}
[pscustomobject]@{at=(Get-Date -Format o);free_gib=$taskFree;native=@($taskNative|Select-Object ProcessId,Name,CreationDate,CommandLine)}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'preflight.json')
$taskLock=[IO.File]::Open((Join-Path $taskEvidence 'native-launch-started.json'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try{
 $bytes=[Text.Encoding]::UTF8.GetBytes(([pscustomobject]@{started=(Get-Date -Format o);runner_sha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;source_manifest_sha256=(Get-FileHash -LiteralPath $taskManifest).Hash}|ConvertTo-Json))
 $taskLock.Write($bytes,0,$bytes.Length)
}finally{$taskLock.Dispose()}
$taskStarted=Get-Date
$taskProfile=Join-Path $taskEvidence 'profile'
& (Join-Path $taskRepo 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode BOTH -RepositoryRoot $taskStage -ProfileRoot $taskProfile -ServerPort $ServerPort -IsolatedLoadoutLibrary -AdditionalArguments @('-aicfQuotaStockPressure','1','-aicfQuotaBootReadback','1','-addr',"127.0.0.1:$ServerPort") *> (Join-Path $taskEvidence 'launcher.txt')
$taskNativeExit=$LASTEXITCODE
$taskNativeExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'native-exit.txt')
Assert-AICFPressureSources $taskData
$taskOwn=@(Get-CimInstance Win32_Process|Where-Object{$_.Name-like'ArmaReforger*.exe'-and$_.CommandLine-and$_.CommandLine.Contains($taskProfile)})
$taskLogs=@(Get-ChildItem -LiteralPath (Join-Path $taskProfile 'logs') -Recurse -File -Filter '*.log')
$taskConsoles=@($taskLogs|Where-Object Name -eq 'console.log')
if($taskConsoles.Count-ne1){throw 'Expected exactly one complete stopped console'}
$taskRaw=[IO.File]::ReadAllText($taskConsoles[0].FullName)
$taskLaunch=[IO.File]::ReadAllText((Join-Path $taskEvidence 'launcher.txt'))
$taskRuntime=([regex]::Match($taskLaunch,'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value)|ConvertFrom-Json
$taskCLI=[regex]::Match($taskRaw,'(?m)CLI Params:\s*([^\r\n]+)').Groups[1].Value.Trim()
$taskCLIExact=$taskCLI-ceq($taskRuntime.arguments-join' ')
& pwsh.exe -NoProfile -ExecutionPolicy Bypass -File $taskFrozenAudit -LogPath $taskConsoles[0].FullName -OutputPath (Join-Path $taskEvidence 'functional-verdict.json') *> (Join-Path $taskEvidence 'audit.txt')
$taskAuditExit=$LASTEXITCODE
$taskFatal=@();$taskResources=@();$taskProject=@()
$taskLogHashes=@(foreach($file in $taskLogs){
 $found=Get-AICFPressureLogFindings ([IO.File]::ReadAllText($file.FullName))
 foreach($row in $found.fatal){$taskFatal+=[pscustomobject]@{path=$file.FullName;line=$row.line;text=$row.text}}
 foreach($row in $found.project){$taskProject+=[pscustomobject]@{path=$file.FullName;line=$row.line;text=$row.text}}
 foreach($row in $found.resources){$taskResources+=[pscustomobject]@{path=$file.FullName;line=$row.line;text=$row.text}}
 [pscustomobject]@{path=$file.FullName;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
})
$taskLogHashes|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $taskEvidence 'full-stopped-log-sha.json')
$taskFPS=@([regex]::Matches($taskRaw,'FPS:\s*([0-9.]+)')|ForEach-Object{[double]$_.Groups[1].Value})
$taskPerformance=$taskFPS.Count-gt0-and@($taskFPS|Where-Object{$_-lt30}).Count-eq0
$taskStopped=$taskNativeExit-eq0-and$taskOwn.Count-eq0-and$taskRaw.Contains('Game destroyed.')
[pscustomobject]@{scope='STOCK_CHARACTER_MANAGED_RECRUIT_ONLY';native_exit=$taskNativeExit;audit_exit=$taskAuditExit;cli_exact=$taskCLIExact;stopped=$taskStopped;own_cim=$taskOwn.Count;source_sha_drift=0;wall_seconds=((Get-Date)-$taskStarted).TotalSeconds;full_fatal=$taskFatal;project_errors=$taskProject;resources=$taskResources;fps_min=($taskFPS|Measure-Object -Minimum).Minimum;performance_pass=$taskPerformance;original_route_pass=$false;universal_quota_pass=$false;full_logs=$taskLogs.FullName}|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $taskEvidence 'verdict.json')
Get-Content -LiteralPath (Join-Path $taskEvidence 'verdict.json') -Raw
if(!$taskStopped-or!$taskCLIExact-or$taskAuditExit-ne0-or$taskFatal.Count-or$taskProject.Count-or$taskResources.Count-or!$taskPerformance){exit 1}
