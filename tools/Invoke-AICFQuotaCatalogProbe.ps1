# Separate Server and Client invocations keep native roles in distinct sessions.
[CmdletBinding()]
param(
 [string]$EvidenceRoot,
 [ValidateSet('Prepare','Server','Client','Audit')][string]$Phase='Prepare',
 [ValidateSet('group','character','vehicle')][string]$Case='group',
 [string]$Variant='Stock',
 [ValidateRange(1,65535)][int]$ServerPort=2670,
 [string]$RepositoryRoot,
 [string]$SourceRoot,
 [string]$RhsAddonsRoot,
 [string]$WorkbenchPath='C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe'
)
$ErrorActionPreference='Stop'
function Get-AICFQuotaCatalogPayload([string]$Root){
 $base=(Resolve-Path -LiteralPath $Root).Path.TrimEnd('\','/')
 @(foreach($addon in @('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')){
  Get-ChildItem -LiteralPath (Join-Path $base $addon) -Recurse -File|Where-Object{$_.Extension-ne'.rdb'}|Sort-Object FullName|ForEach-Object{
   [pscustomobject]@{path=$_.FullName.Substring($base.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
  }
 })
}
function Assert-AICFQuotaCatalogPayload($Manifest,[string]$Stage,[string]$Auditor){
 $root=[IO.Path]::GetFullPath($Stage).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
 if([IO.Path]::GetFullPath($Manifest.stage).TrimEnd('\','/')-ne$root.TrimEnd('\','/')){throw 'Prepared stage identity changed'}
 $expected=@{};foreach($row in @($Manifest.files)+@($Manifest.fixture)){
  $full=[IO.Path]::GetFullPath((Join-Path $root $row.path))
  if(!$full.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)-or$expected.ContainsKey($full)){throw 'Manifest escapes stage or duplicates source'}
  $expected[$full]=$row.sha256
 }
 $actual=@(Get-AICFQuotaCatalogPayload $Stage)
 if($actual.Count-ne$expected.Count){throw 'Frozen source set changed'}
 foreach($row in $actual){$full=[IO.Path]::GetFullPath((Join-Path $root $row.path));if(!$expected.ContainsKey($full)-or$expected[$full]-ne$row.sha256){throw ('Frozen source drift '+$row.path)}}
 if((Get-FileHash -LiteralPath $Auditor).Hash-ne$Manifest.auditor_sha256){throw 'Frozen auditor drift'}
}
function Get-AICFQuotaCatalogCanonical([string]$LauncherLog){
 $matches=@(Get-Content -LiteralPath $LauncherLog|Where-Object{$_-like'AICF_RUNTIME_MANIFEST_JSON=*'})
 if($matches.Count-ne1){throw 'Exactly one canonical runtime manifest required'}
 $matches[0].Substring('AICF_RUNTIME_MANIFEST_JSON='.Length)|ConvertFrom-Json
}
# Loading the reusable validation functions must not prepare or start anything.
if($MyInvocation.InvocationName-eq'.'){return}
if(!$EvidenceRoot){throw 'EvidenceRoot required'}
if(!$RepositoryRoot){$RepositoryRoot=Split-Path -Parent $PSScriptRoot}
$taskRepo=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
$taskStage=Join-Path $taskEvidence 'stage'
$taskManifest=Join-Path $taskEvidence 'source-manifest.json'
$taskAudit=Join-Path $taskEvidence 'Test-AIQuotaCatalogLog.ps1'
$taskLauncher=Join-Path $taskRepo 'tools/Start-AICFRuntime.ps1'
if($Phase-eq'Prepare'){
 if(Test-Path -LiteralPath $taskEvidence){throw 'Prepare requires a fresh evidence directory'}
 if(!$SourceRoot){$SourceRoot=$taskRepo}
 $taskSource=(Resolve-Path -LiteralPath $SourceRoot).Path
 $taskFiles=@(Get-AICFQuotaCatalogPayload $taskSource)
 $taskFixture='AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_ZZQuotaCatalogProbe.c'
 if($taskFixture-in$taskFiles.path-or@($taskFiles|Where-Object{$_.path-match'/AICF_ZZ[^/]*Probe\.c$'}).Count){throw 'SourceRoot must be fixture-free production'}
 [void][IO.Directory]::CreateDirectory($taskEvidence)
 foreach($row in $taskFiles){$destination=Join-Path $taskStage $row.path;[void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination));Copy-Item -LiteralPath (Join-Path $taskSource $row.path) -Destination $destination}
 Copy-Item -LiteralPath (Join-Path $taskRepo 'tests/fixtures/AICF_AIQuotaCatalogProbe.c') -Destination (Join-Path $taskStage $taskFixture)
 Copy-Item -LiteralPath (Join-Path $taskRepo 'tests/log-audits/Test-AIQuotaCatalogLog.ps1') -Destination $taskAudit
 $taskData=[pscustomobject]@{created=(Get-Date -Format o);source=$taskSource;stage=$taskStage;variant=$Variant;server_port=$ServerPort;rhs_addons_root=$RhsAddonsRoot;raw_files=$taskFiles.Count;files=$taskFiles;fixture=[pscustomobject]@{path=$taskFixture;sha256=(Get-FileHash (Join-Path $taskStage $taskFixture)).Hash};auditor_sha256=(Get-FileHash $taskAudit).Hash;canonical_launcher_sha256=(Get-FileHash $taskLauncher).Hash}
 $taskData|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $taskManifest -Encoding UTF8
 Assert-AICFQuotaCatalogPayload $taskData $taskStage $taskAudit
 $planArgs=@{Role='Client';Variant=$Variant;RepositoryRoot=$taskStage;DryRun=$true};if($RhsAddonsRoot){$planArgs.RhsAddonsRoot=$RhsAddonsRoot}
 $plan=@(& $taskLauncher @planArgs);if($LASTEXITCODE){throw 'Canonical compile plan failed'}
 $plan|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-plan.txt')
 $runtime=Get-AICFQuotaCatalogCanonical (Join-Path $taskEvidence 'compile-plan.txt');$a=$runtime.arguments
 & $WorkbenchPath -noThrow -wbsilent -gproj $a[[array]::IndexOf($a,'-gproj')+1] -addonsDir $a[[array]::IndexOf($a,'-addonsDir')+1] -addons $a[[array]::IndexOf($a,'-addons')+1] -logsDir (Join-Path $taskEvidence 'workbench') -wbModule=ScriptEditor -run -validate|Out-Null
 $compileExit=$LASTEXITCODE;$compileExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-exit.txt')
 Assert-AICFQuotaCatalogPayload $taskData $taskStage $taskAudit
 $compiler=Join-Path $taskEvidence 'workbench/script.log';$text=[IO.File]::ReadAllText($compiler)
 if($compileExit-ne0-or!$text.Contains('Script validation successful.')-or$text-match'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)'){throw 'Full compiler log failed'}
 [ordered]@{exit=$compileExit;log_sha256=(Get-FileHash $compiler).Hash;source_manifest_sha256=(Get-FileHash $taskManifest).Hash}|ConvertTo-Json|Set-Content (Join-Path $taskEvidence 'compile-receipt.json')
 'CATALOG_PREPARED='+$taskManifest;return
}
$taskData=Get-Content -LiteralPath $taskManifest -Raw|ConvertFrom-Json
if($taskData.variant-ne$Variant-or$taskData.server_port-ne$ServerPort){throw 'Prepared variant/port differs'}
Assert-AICFQuotaCatalogPayload $taskData $taskStage $taskAudit
if((Get-FileHash $taskLauncher).Hash-ne$taskData.canonical_launcher_sha256){throw 'Canonical launcher changed since preparation'}
$compile=Get-Content (Join-Path $taskEvidence 'compile-receipt.json') -Raw|ConvertFrom-Json
if($compile.exit-ne0-or$compile.source_manifest_sha256-ne(Get-FileHash $taskManifest).Hash-or$compile.log_sha256-ne(Get-FileHash (Join-Path $taskEvidence 'workbench/script.log')).Hash){throw 'Compile/source receipt mismatch'}
$preparedCompiler=[IO.File]::ReadAllText((Join-Path $taskEvidence 'workbench/script.log'))
if(!$preparedCompiler.Contains('Script validation successful.')-or$preparedCompiler-match'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)'){throw 'Full prepared compiler log does not prove validation'}
$taskCase=Join-Path $taskEvidence $Case
if($Phase-in@('Server','Client')){
 [void][IO.Directory]::CreateDirectory($taskCase)
 if($Phase-eq'Client'-and!(Test-Path -LiteralPath (Join-Path $taskCase 'Server-launcher.txt'))){throw 'Server must start in a separate session first'}
 if(Test-Path -LiteralPath (Join-Path $taskCase ($Phase+'-profile'))){throw 'Native role requires a fresh profile'}
 $native=@(Get-CimInstance Win32_Process -ErrorAction Stop|Where-Object{$_.Name-like'ArmaReforger*.exe'-and$_.Name-notlike'*Workbench*'})
 $free=(Get-CimInstance Win32_OperatingSystem -ErrorAction Stop).FreePhysicalMemory/1MB
 $required=0;if($Phase-eq'Client'){$required=1}
 if($native.Count-ne$required-or$free-lt8){throw 'Native allocation/RAM preflight rejected; no processes stopped'}
 if($Phase-eq'Client'-and($native[0].Name-notlike'ArmaReforgerServer*.exe'-or!$native[0].CommandLine-or!$native[0].CommandLine.Contains((Join-Path $taskCase 'Server-profile')))){throw 'Live native process is not this exact server'}
 $marker=Join-Path $taskCase ($Phase+'-started.json');$lock=[IO.File]::Open($marker,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
 try{$bytes=[Text.Encoding]::UTF8.GetBytes(([ordered]@{at=(Get-Date -Format o);source_manifest_sha256=(Get-FileHash $taskManifest).Hash;mode=$Case;role=$Phase}|ConvertTo-Json));$lock.Write($bytes,0,$bytes.Length)}finally{$lock.Dispose()}
 $options=@{Role=$Phase;Variant=$Variant;RepositoryRoot=$taskStage;ProfileRoot=(Join-Path $taskCase ($Phase+'-profile'));ServerPort=$ServerPort;IsolatedLoadoutLibrary=$true;ReadyTimeoutSeconds=240;AdditionalArguments=@('-aicfQuotaCatalogProbe',$Case)}
 if($taskData.rhs_addons_root){$options.RhsAddonsRoot=$taskData.rhs_addons_root}
 if($Phase-eq'Server'){$options.AdditionalArguments+=@('-addr',"127.0.0.1:$ServerPort")}else{$options.ServerProfileRoot=Join-Path $taskCase 'Server-profile'}
 $options|ConvertTo-Json -Depth 5|Set-Content (Join-Path $taskCase ($Phase+'-options.json'))
 & $taskLauncher @options *> (Join-Path $taskCase ($Phase+'-launcher.txt'))
 $nativeExit=$LASTEXITCODE;$nativeExit|Set-Content (Join-Path $taskCase ($Phase+'-exit.txt'))
 Assert-AICFQuotaCatalogPayload $taskData $taskStage $taskAudit
 if($nativeExit-ne0){exit 1};return
}
$results=@();$logHashes=@()
foreach($role in @('Server','Client')){
 $runtime=Get-AICFQuotaCatalogCanonical (Join-Path $taskCase ($role+'-launcher.txt'))
 if($runtime.role-ne$role-or$runtime.variant-ne$Variant-or$runtime.profile-ne(Join-Path $taskCase ($role+'-profile'))){throw 'Actual canonical role/profile differs'}
 $runtimeArguments=$runtime.arguments;$index=[array]::IndexOf($runtimeArguments,'-aicfQuotaCatalogProbe')
 if($index-lt0-or$runtimeArguments[$index+1]-ne$Case-or$runtimeArguments-contains'-autodeployFaction'){throw 'Actual native probe CLI differs'}
 $projectIndex=[array]::IndexOf($runtimeArguments,'-gproj')
 $stagePrefix=[IO.Path]::GetFullPath($taskStage).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
 if($projectIndex-lt0-or![IO.Path]::GetFullPath($runtimeArguments[$projectIndex+1]).StartsWith($stagePrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Native project is outside frozen stage'}
 $console=Get-ChildItem -LiteralPath (Join-Path $runtime.profile 'logs') -Filter console.log -Recurse|Select-Object -First 1
 if(!$console){throw 'Full native console missing'}
 $cli=@(Select-String -LiteralPath $console.FullName -Pattern 'CLI Params:')
 if($cli.Count-ne1-or$cli[0].Line-notmatch('-aicfQuotaCatalogProbe\s+'+[regex]::Escape($Case)+'(?:\s|$)')){throw 'Actual native CLI not proven'}
 $exit=[int](Get-Content -LiteralPath (Join-Path $taskCase ($role+'-exit.txt')))
 $launcherText=[IO.File]::ReadAllText((Join-Path $taskCase ($role+'-launcher.txt')))
 if($launcherText-notmatch('\[RUNTIME_LAUNCHER\]\[EXIT\].*role='+$role+'\b.*exit_code='+$exit+'\b')){throw 'Actual native launcher exit differs'}
 if($role-eq'Server'-and!(Select-String -LiteralPath $console.FullName -Pattern '[ROSTER_READY]' -SimpleMatch -Quiet)){throw 'Actual server roster readiness missing'}
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $taskAudit -LogPath $console.FullName -Mode $Case -NativeExitCode $exit -LogRole $role -EvidenceRoot (Join-Path $taskCase ($role+'-audit')) *> (Join-Path $taskCase ($role+'-audit.txt'))
 $auditExit=$LASTEXITCODE;$results+=[pscustomobject]@{role=$role;native_exit=$exit;audit_exit=$auditExit}
 $logHashes+=@(Get-ChildItem -LiteralPath $console.DirectoryName -File -Filter '*.log'|ForEach-Object{[pscustomobject]@{path=$_.FullName;sha256=(Get-FileHash $_.FullName).Hash;bytes=$_.Length}})
}
$clientLauncher=[IO.File]::ReadAllText((Join-Path $taskCase 'Client-launcher.txt'))
if(!$clientLauncher.Contains('[AICF][RUNTIME_LAUNCHER][SERVER_READY]')){throw 'Client readiness was not confirmed by canonical launcher'}
$logHashes|ConvertTo-Json -Depth 5|Set-Content (Join-Path $taskCase 'full-stopped-log-sha.json')
$results|ConvertTo-Json -Depth 4|Set-Content (Join-Path $taskCase 'summary.json')
Assert-AICFQuotaCatalogPayload $taskData $taskStage $taskAudit
$results|Format-Table
if(@($results|Where-Object{$_.native_exit-ne0-or$_.audit_exit-ne0}).Count){exit 1}
