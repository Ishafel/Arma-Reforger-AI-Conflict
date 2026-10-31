[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [Parameter(Mandatory)][ValidateSet('INSUFFICIENT','AFFORDABLE','DRAIN_AFTER_RESERVE')][string]$Mode,
 [ValidateSet('US','USSR')][string]$Faction='US',
 [string]$RepositoryRoot,
 [string]$SourceRoot,
 [string]$StageRoot,
 [string]$ExpectedProductionManifest,
 [ValidateRange(1,65535)][int]$ServerPort=2660,
 [switch]$PrepareOnly,
 [switch]$SkipCompile,
 [string]$WorkbenchExe='C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe'
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AICF_VehicleSoakSources.ps1')
function Get-AICFEconomyProbeArguments(
 [ValidateSet('INSUFFICIENT','AFFORDABLE','DRAIN_AFTER_RESERVE')][string]$Mode,
 [ValidateSet('US','USSR')][string]$Faction,[ValidateRange(1,65535)][int]$Port){
 @('-aicfPresetEconomyProbe','1','-aicfPresetEconomyCase',$Mode.ToUpperInvariant(),'-aicfVehicleBehaviorFaction',$Faction.ToUpperInvariant(),'-addr',"127.0.0.1:$Port")
}
function Assert-AICFEconomyProduction([string]$Stage,[object[]]$Files){
 $prefix=[IO.Path]::GetFullPath($Stage).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
 $expectedPaths=@{}
 foreach($file in $Files){
  $path=[IO.Path]::GetFullPath((Join-Path $Stage $file.path))
  if(!$path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Production manifest escapes stage'}
  if($expectedPaths.ContainsKey($path)){throw 'Duplicate production source'}
  $expectedPaths[$path]=$true
  if(!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256){throw "Production source mismatch: $($file.path)"}
 }
 $fixturePaths=@('AICF_VehicleBehaviorProbe.c','AICF_VehicleBehaviorSettingsProbe.c','ZZ_AICF_VehiclePresetEconomyDeclineProbe.c')|ForEach-Object{Join-Path $Stage ('AIConflictCore/Scripts/Game/AIConflict/'+$_)}
 $actual=@(Get-ChildItem -LiteralPath $Stage -Recurse -File|Where-Object{$_.Name -ne 'resourceDatabase.rdb' -and $_.FullName -notin $fixturePaths})
 if($actual.Count -ne $expectedPaths.Count){throw 'Production source set differs from canonical manifest'}
 foreach($file in $actual){if(!$expectedPaths.ContainsKey($file.FullName)){throw 'Unexpected production file outside canonical manifest'}}
}
function Assert-AICFEconomyPreparedSources([string]$Stage){
 $prepared=Join-Path (Split-Path -Parent $Stage) 'source-manifest.json'
 if(!(Test-Path -LiteralPath $prepared)){throw 'Compiled stage lacks source manifest'}
 $manifest=Get-Content -LiteralPath $prepared -Raw|ConvertFrom-Json
 if([IO.Path]::GetFullPath($manifest.stage) -ne [IO.Path]::GetFullPath($Stage)){throw 'Compiled manifest belongs to a different stage'}
 $compiled=@($manifest.files|Where-Object{[IO.Path]::GetExtension($_.path) -in @('.c','.conf','.st','.gproj')})
 Assert-AICFVehicleSoakSources $Stage $compiled
}
if($MyInvocation.InvocationName -eq '.'){return}
if(!$RepositoryRoot){$RepositoryRoot=Split-Path -Parent $PSScriptRoot}
$Mode=$Mode.ToUpperInvariant();$Faction=$Faction.ToUpperInvariant()
$taskRepository=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
if(Test-Path -LiteralPath $taskEvidence){throw 'EvidenceRoot must be new'}
[void][IO.Directory]::CreateDirectory($taskEvidence)
$taskOwnStage=!$StageRoot
if(!$SourceRoot){$SourceRoot=$taskRepository}
$taskSource=(Resolve-Path -LiteralPath $SourceRoot).Path
if($taskOwnStage){
 $StageRoot=Join-Path $taskEvidence 'stage'
 [void][IO.Directory]::CreateDirectory($StageRoot)
 $taskPackages=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
 $taskPaths=@(& git -C $taskRepository ls-files --cached --others --exclude-standard -- @taskPackages|Sort-Object -Unique)
 if($LASTEXITCODE -ne 0){throw 'Cannot enumerate production sources'}
 foreach($taskPath in $taskPaths){
  if([IO.Path]::GetExtension($taskPath) -eq '.rdb'){continue}
  $taskInput=Join-Path $taskSource $taskPath
  if(!(Test-Path -LiteralPath $taskInput)){continue}
  $taskOutput=Join-Path $StageRoot $taskPath
  [void][IO.Directory]::CreateDirectory((Split-Path -Parent $taskOutput))
  Copy-Item -LiteralPath $taskInput -Destination $taskOutput
 }
}
$StageRoot=(Resolve-Path -LiteralPath $StageRoot).Path
if($StageRoot -eq $taskRepository){throw 'Production checkout cannot be a native stage'}
$taskScripts=Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict'
$taskFixtures=@(
 @('AICF_VehicleBehaviorProbe.c','AICF_VehicleBehaviorProbe.c'),
 @('AICF_VehicleBehaviorSettingsProbe.c','AICF_VehicleBehaviorSettingsProbe.c'),
 @('AICF_VehiclePresetEconomyDeclineProbe.c','ZZ_AICF_VehiclePresetEconomyDeclineProbe.c'))
foreach($taskFixture in $taskFixtures){
 $taskInput=Join-Path $taskRepository ('tests/fixtures/'+$taskFixture[0]);$taskOutput=Join-Path $taskScripts $taskFixture[1]
 if($taskOwnStage){Copy-Item -LiteralPath $taskInput -Destination $taskOutput}
 elseif(!(Test-Path -LiteralPath $taskOutput) -or (Get-FileHash $taskInput).Hash -ne (Get-FileHash $taskOutput).Hash){throw 'Shared stage has a different economy fixture'}
}
foreach($taskOther in @('AICF_VehicleSoakProbe.c','AICF_VehicleMultiSquadProbe.c','ZZ_AICF_ManualStagingProbe.c')){
 if(Test-Path -LiteralPath (Join-Path $taskScripts $taskOther)){throw 'Competing native fixture in stage'}
}
$taskExpected=@()
if($ExpectedProductionManifest){
 $taskManifest=Get-Content -LiteralPath $ExpectedProductionManifest -Raw|ConvertFrom-Json
 if($taskManifest.PSObject.Properties.Name -contains 'files'){$taskExpected=@($taskManifest.files)}
 elseif($taskManifest.PSObject.Properties.Name -contains 'production'){$taskExpected=@($taskManifest.production)}
 if(!$taskExpected.Count){throw 'Empty production manifest'}
 Assert-AICFEconomyProduction $StageRoot $taskExpected
}
$taskSources=@(Get-AICFVehicleSoakSources $StageRoot)
$taskAudit=Join-Path $taskEvidence 'frozen-auditor.ps1'
Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/log-audits/Test-VehiclePresetEconomyDeclineLog.ps1') -Destination $taskAudit
[pscustomobject]@{stage=$StageRoot;source_root=$taskSource;shared_validated_stage=(!$taskOwnStage);mode=$Mode;faction=$Faction;scope='CONTROLLED_PREASSET_ECONOMY_ONLY';original_route_proven=$false;normal_war=$false;auditor_sha256=(Get-FileHash $taskAudit).Hash;files=$taskSources}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-manifest.json')
if(!$SkipCompile){
 $taskPlan=@(& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Client -Variant Stock -RepositoryRoot $StageRoot -DryRun)
 $taskPlan|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-plan.txt')
 $taskLaunch=[regex]::Match(($taskPlan -join "`n"),'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value|ConvertFrom-Json
 $taskArgs=$taskLaunch.arguments
 $taskProject=$taskArgs[[array]::IndexOf($taskArgs,'-gproj')+1];$taskAddonsDir=$taskArgs[[array]::IndexOf($taskArgs,'-addonsDir')+1];$taskAddons=$taskArgs[[array]::IndexOf($taskArgs,'-addons')+1]
 & $WorkbenchExe -noThrow -wbsilent -gproj $taskProject -addonsDir $taskAddonsDir -addons $taskAddons -logsDir (Join-Path $taskEvidence 'workbench') -wbModule=ScriptEditor -run -validate|Out-Null
 $taskCompileExit=$LASTEXITCODE
 $taskCompileExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-exit.txt')
 if($taskCompileExit -ne 0 -or (Get-Content (Join-Path $taskEvidence 'workbench/script.log') -Raw) -notmatch 'Script validation successful\.'){throw 'Terminal Workbench validation failed'}
}else{
 if($taskOwnStage){throw 'SkipCompile requires an existing validated stage'}
 $taskPrepared=Split-Path -Parent $StageRoot
 if(!(Test-Path (Join-Path $taskPrepared 'compile-exit.txt')) -or [int](Get-Content (Join-Path $taskPrepared 'compile-exit.txt') -Raw) -ne 0 -or
  (Get-Content (Join-Path $taskPrepared 'workbench/script.log') -Raw) -notmatch 'Script validation successful\.'){throw 'Shared stage lacks successful Workbench evidence'}
 Assert-AICFEconomyPreparedSources $StageRoot
}
Assert-AICFVehicleSoakSources $StageRoot $taskSources
if($taskExpected.Count){Assert-AICFEconomyProduction $StageRoot $taskExpected}
if($PrepareOnly){"ECONOMY_PREPARED=$StageRoot NATIVE=NOT_RUN";exit 0}
$taskStarted=[DateTimeOffset]::UtcNow
$taskArguments=@(Get-AICFEconomyProbeArguments $Mode $Faction $ServerPort)
& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode BOTH -RepositoryRoot $StageRoot -ProfileRoot (Join-Path $taskEvidence 'profile') -IsolatedLoadoutLibrary -ServerPort $ServerPort -AdditionalArguments $taskArguments *> (Join-Path $taskEvidence 'launcher.txt')
$taskNativeExit=$LASTEXITCODE;$taskEnded=[DateTimeOffset]::UtcNow
$taskNativeExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'native-exit.txt')
$taskDrift=0
try{Assert-AICFVehicleSoakSources $StageRoot $taskSources;if($taskExpected.Count){Assert-AICFEconomyProduction $StageRoot $taskExpected}}catch{$taskDrift=1}
[pscustomobject]@{started_utc=$taskStarted.ToString('o');ended_utc=$taskEnded.ToString('o');wall_seconds=($taskEnded-$taskStarted).TotalSeconds;mode=$Mode}|ConvertTo-Json|Set-Content (Join-Path $taskEvidence 'timing.json')
[pscustomobject]@{native_exit=$taskNativeExit;source_drift=$taskDrift;mode=$Mode;stage=$StageRoot;original_route_proven=$false}|ConvertTo-Json|Set-Content (Join-Path $taskEvidence 'summary.json')
$taskAuditArgs=@{EvidenceRoot=$taskEvidence};if($Mode -eq 'AFFORDABLE'){$taskAuditArgs.Affordable=$true};if($Mode -eq 'DRAIN_AFTER_RESERVE'){$taskAuditArgs.ReservedEmpty=$true}
& $taskAudit @taskAuditArgs *> (Join-Path $taskEvidence 'audit.txt')
$taskAuditExit=$LASTEXITCODE
Get-Content -LiteralPath (Join-Path $taskEvidence 'audit.txt')
if($taskNativeExit -ne 0 -or $taskDrift -ne 0 -or $taskAuditExit -ne 0){exit 1};exit 0
