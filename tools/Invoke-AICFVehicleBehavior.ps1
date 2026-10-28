[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [ValidateCount(1,2)][ValidateSet('transport','combat')][string[]]$Cases=@('transport','combat'),
 [ValidateSet('US','USSR')][string]$Faction='USSR',
 [switch]$CleanupIncapProbe,
 [switch]$CleanupAliveProbe,
 [switch]$ReboardPilotLossProbe,
 [switch]$NativeRouteLossProbe,
 [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
 [ValidateRange(1,65535)][int]$ServerPort=2610
)
$ErrorActionPreference='Stop'
if(@($Cases|Sort-Object -Unique).Count -ne $Cases.Count){throw 'Duplicate behavior scenarios are not allowed'}
if($CleanupIncapProbe -and ($Cases.Count -ne 1 -or $Cases[0] -ne 'combat')){throw 'CleanupIncapProbe requires isolated combat scenario'}
if($ReboardPilotLossProbe -and ($Cases.Count -ne 1 -or $Cases[0] -ne 'combat' -or $CleanupIncapProbe)){throw 'ReboardPilotLossProbe requires isolated combat scenario without CleanupIncapProbe'}
if($NativeRouteLossProbe -and ($Cases.Count -ne 1 -or $Cases[0] -ne 'combat' -or $CleanupIncapProbe -or $ReboardPilotLossProbe)){throw 'NativeRouteLossProbe requires isolated combat scenario without other probes'}
if($CleanupAliveProbe -and ($Cases.Count -ne 1 -or $Cases[0] -ne 'combat' -or $CleanupIncapProbe -or $ReboardPilotLossProbe -or $NativeRouteLossProbe)){throw 'CleanupAliveProbe requires isolated combat scenario without other probes'}
$taskRepository=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
if(Test-Path -LiteralPath $taskEvidence){throw 'EvidenceRoot must be new'}
[void][IO.Directory]::CreateDirectory($taskEvidence)
$taskStage=Join-Path $taskEvidence 'stage'
$taskAddons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
$taskSources=@(& git -C $taskRepository ls-files --cached -- $taskAddons)
if($LASTEXITCODE -ne 0){throw 'Cannot enumerate tracked addon sources'}
$taskSources+=@(& git -C $taskRepository ls-files --others --exclude-standard -- $taskAddons)
if($LASTEXITCODE -ne 0){throw 'Cannot enumerate new addon sources'}
$taskSourceHashes=@()
# Копируем точные checkout bytes: git archive на Windows нормализует EOL.
foreach($taskRelative in ($taskSources|Sort-Object -Unique)){
 $taskSource=Join-Path $taskRepository $taskRelative
 if(!(Test-Path -LiteralPath $taskSource -PathType Leaf)){throw "Deleted source unsupported: $taskRelative"}
 $taskDestination=Join-Path $taskStage $taskRelative
 [void][IO.Directory]::CreateDirectory((Split-Path -Parent $taskDestination))
 Copy-Item -LiteralPath $taskSource -Destination $taskDestination
 $taskHash=(Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash
 if((Get-FileHash -LiteralPath $taskDestination -Algorithm SHA256).Hash -ne $taskHash){throw "Source copy mismatch: $taskRelative"}
 $taskSourceHashes+=[pscustomobject]@{relative=$taskRelative;source=$taskSource;path=$taskDestination;sha256=$taskHash;fixture=$false}
}
foreach($taskFixture in @('AICF_VehicleBehaviorProbe.c','AICF_VehicleBehaviorSettingsProbe.c')){
 $taskFixtureSource=Join-Path $taskRepository ('tests/fixtures/'+$taskFixture)
 $taskFixtureDestination=Join-Path $taskStage ('AIConflictCore/Scripts/Game/AIConflict/'+$taskFixture)
 Copy-Item -LiteralPath $taskFixtureSource -Destination $taskFixtureDestination
 $taskFixtureHash=(Get-FileHash -LiteralPath $taskFixtureSource -Algorithm SHA256).Hash
 if((Get-FileHash -LiteralPath $taskFixtureDestination -Algorithm SHA256).Hash -ne $taskFixtureHash){throw "Fixture copy mismatch: $taskFixture"}
 $taskSourceHashes+=[pscustomobject]@{relative=('tests/fixtures/'+$taskFixture);source=$taskFixtureSource;path=$taskFixtureDestination;sha256=$taskFixtureHash;fixture=$true}
}
ConvertTo-Json -InputObject @($taskSourceHashes) -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-hashes.json')
[ordered]@{commit=(& git -C $taskRepository rev-parse HEAD);status=@(& git -C $taskRepository status --short);cases=$Cases;faction=$Faction;classification='CONTROLLED_BEHAVIOR_PROBE';normal_war_proof=$false;server_port=$ServerPort;started=(Get-Date).ToString('o')}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'context.json')
$taskPlan=@(& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Client -Variant Stock -RepositoryRoot $taskStage -DryRun)
$taskPlan|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-plan.txt')
$taskManifests=@([regex]::Matches(($taskPlan -join "`n"),'(?m)^AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)'))
if($taskManifests.Count -ne 1){throw 'One exact compile graph manifest is required'}
$taskManifest=$taskManifests[0].Groups[1].Value|ConvertFrom-Json
$taskArgs=@($taskManifest.arguments)
function GraphValue([string]$flag){
 $index=[array]::IndexOf($taskArgs,$flag)
 if($index -lt 0 -or $index+1 -ge $taskArgs.Count){throw "Missing compile graph argument: $flag"}
 return $taskArgs[$index+1]
}
$taskWorkbench='C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe'
$taskCompileArgs=@('-noThrow','-wbsilent','-gproj',(GraphValue '-gproj'),'-addonsDir',(GraphValue '-addonsDir'),'-addons',(GraphValue '-addons'),'-logsDir',(Join-Path $taskEvidence 'workbench'),'-wbModule=ScriptEditor','-run','-validate')
ConvertTo-Json -InputObject (@($taskWorkbench)+$taskCompileArgs)|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-command.json')
& $taskWorkbench @taskCompileArgs|Out-Null
$taskCompileExit=$LASTEXITCODE
$taskCompileExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'compile-exit.txt')
$taskCompile=(@(Get-ChildItem -LiteralPath (Join-Path $taskEvidence 'workbench') -Recurse -File -Filter '*.log')|ForEach-Object{[IO.File]::ReadAllText($_.FullName)}) -join "`n"
if($taskCompileExit -ne 0 -or !$taskCompile.Contains('Script validation successful.') -or $taskCompile -match 'SCRIPT\s+\([EF]\)|ENGINE\s+\(F\)|Virtual Machine Exception|NULL pointer|Access violation'){throw 'Behavior fixture compile failed; full logs saved'}
$taskResults=@()
foreach($taskCase in $Cases){
 $taskCaseRoot=Join-Path $taskEvidence $taskCase
 [void][IO.Directory]::CreateDirectory($taskCaseRoot)
 $taskProfile=Join-Path $taskCaseRoot 'profile'
 $taskLauncher=Join-Path $taskCaseRoot 'launcher.txt'
 $taskAdditional=@('-aicfVehicleBehaviorProbe',$taskCase,'-aicfVehicleBehaviorFaction',$Faction,'-aicfGroupTrace','1','-aicfGroupTraceFaction',$Faction,'-aicfGroupTraceSlot','0','-addr',"127.0.0.1:$ServerPort")
 if($CleanupIncapProbe){$taskAdditional+=@('-aicfCleanupIncapProbe','1')}
 if($CleanupAliveProbe){$taskAdditional+=@('-aicfCleanupAliveProbe','1')}
 if($ReboardPilotLossProbe){$taskAdditional+=@('-aicfReboardPilotLossProbe','1')}
 if($NativeRouteLossProbe){$taskAdditional+=@('-aicfNativeRouteLossProbe','1')}
 & (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode $Faction -RepositoryRoot $taskStage -ProfileRoot $taskProfile -ServerPort $ServerPort -AdditionalArguments $taskAdditional *> $taskLauncher
 $taskNativeExit=$LASTEXITCODE
 $taskExitPath=Join-Path $taskCaseRoot 'native-exit.txt'
 $taskNativeExit|Set-Content -LiteralPath $taskExitPath
 $taskConsoles=@(Get-ChildItem -LiteralPath (Join-Path $taskProfile 'logs') -Recurse -Filter console.log)
 if($taskConsoles.Count -ne 1){throw "Expected one fresh console.log for $taskCase"}
 $taskAuditPath=Join-Path $taskCaseRoot 'audit.txt'
 $taskAuditArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $taskRepository 'tests/log-audits/Test-VehicleBehaviorLog.ps1'),'-LogPath',$taskConsoles[0].FullName,'-LauncherPath',$taskLauncher,'-NativeExitPath',$taskExitPath,'-Scenario',$taskCase,'-Faction',$Faction)
 if($CleanupIncapProbe){$taskAuditArgs+='-CleanupIncapProbe'}
 if($CleanupAliveProbe){$taskAuditArgs+='-CleanupAliveProbe'}
 if($ReboardPilotLossProbe){$taskAuditArgs+='-ReboardPilotLossProbe'}
 if($NativeRouteLossProbe){$taskAuditArgs+='-NativeRouteLossProbe'}
 & powershell.exe @taskAuditArgs *> $taskAuditPath
 $taskAuditExit=$LASTEXITCODE
 $taskAuditExit|Set-Content -LiteralPath (Join-Path $taskCaseRoot 'audit-exit.txt')
 $taskResourceVerdict=[regex]::Match([IO.File]::ReadAllText($taskAuditPath),'gate=strict_native_resources verdict=(PASS|FAIL)').Groups[1].Value
 $taskOverall='FAIL'
 if($taskNativeExit -eq 0 -and $taskAuditExit -eq 0 -and $taskResourceVerdict -eq 'PASS'){$taskOverall='PASS'}
 foreach($taskHash in $taskSourceHashes){if((Get-FileHash -LiteralPath $taskHash.path -Algorithm SHA256).Hash -ne $taskHash.sha256){throw 'Stage changed during behavior matrix'}}
 $taskResults+=[pscustomobject]@{scenario=$taskCase;faction=$Faction;classification='CONTROLLED_BEHAVIOR_PROBE';normal_war_proof=$false;native_exit=$taskNativeExit;targeted_audit_exit=$taskAuditExit;strict_native_resources=$taskResourceVerdict;overall_runtime_verdict=$taskOverall;source_drift=0;console=$taskConsoles[0].FullName;launcher=$taskLauncher;ended=(Get-Date).ToString('o')}
 ConvertTo-Json -InputObject @($taskResults) -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'summary.json')
 "BEHAVIOR_FINISHED=$taskCase FACTION=$Faction NATIVE_EXIT=$taskNativeExit TARGETED_AUDIT_EXIT=$taskAuditExit STRICT_NATIVE_RESOURCES=$taskResourceVerdict OVERALL=$taskOverall"
}
# Immutable stage и свежие profiles с полными stopped logs остаются evidence.
"VEHICLE_BEHAVIOR_EVIDENCE=$taskEvidence"
if(@($taskResults|Where-Object{$_.overall_runtime_verdict -ne 'PASS'}).Count){exit 1}
exit 0
