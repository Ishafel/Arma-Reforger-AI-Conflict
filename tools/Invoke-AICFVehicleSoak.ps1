[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$EvidenceRoot,
    [Parameter(Mandatory)][ValidateSet('US','USSR')][string]$Faction,
    [ValidateRange(1,3600)][int]$DurationSeconds=1800,
    [ValidateRange(10,180)][int]$MountedHoldSeconds=10,
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
    [string]$SourceRef='HEAD',
    [switch]$SnapshotWorkingTree,
    [string]$StageRoot,
    [switch]$SkipCompile,
    [ValidateRange(1,65535)][int]$ServerPort=2510,
    [ValidateSet('US','USSR','BOTH')][string]$AICommanderMode='BOTH',
    [ValidateSet(1,2,3,4)][int]$ProofVersion=2,
    [switch]$LooseWeaponObserver,
    [switch]$RecruitmentDecisionObserver,
    [switch]$QuotaCensusDecisionObserver
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AICF_VehicleSoakSources.ps1')
$taskRepository=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskEvidence=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
if(Test-Path -LiteralPath $taskEvidence){throw 'EvidenceRoot must be new'}
[void][IO.Directory]::CreateDirectory($taskEvidence)
$taskAuditScript=Join-Path $taskEvidence 'Test-VehicleSoakLog.ps1'
$taskAuditSource='tests/log-audits/Test-VehicleSoakLog.ps1'
if($ProofVersion -eq 4){$taskAuditSource='tests/log-audits/Test-VehicleSoakObservationLog.ps1'}
Copy-Item -LiteralPath (Join-Path $taskRepository $taskAuditSource) -Destination $taskAuditScript
$taskAuditHash=(Get-FileHash -LiteralPath $taskAuditScript -Algorithm SHA256).Hash
$taskCommit=(& git -C $taskRepository rev-parse --verify "$SourceRef^{commit}").Trim()
if($LASTEXITCODE -ne 0){throw 'Cannot resolve source commit'}
$taskOwnStage=!$StageRoot
$taskObservers=@()
if($RecruitmentDecisionObserver){$taskObservers+=[pscustomobject]@{name='RecruitmentDecisionObserver';source='tests/fixtures/AICF_RecruitmentDecisionProbe.c';relative='AIConflictCore/Scripts/Game/AIConflict/AICF_ZRecruitmentDecisionProbe.c';flag='-aicfRecruitmentDecisionProbe'}}
if($QuotaCensusDecisionObserver){$taskObservers+=[pscustomobject]@{name='QuotaCensusDecisionObserver';source='tests/fixtures/AICF_AIQuotaCensusDecisionProbe.c';relative='AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_ZZQuotaCensusDecisionProbe.c';flag='-aicfQuotaCensusDecisionProbe'}}
if($SkipCompile -and $taskOwnStage){throw 'SkipCompile requires an existing compiled stage'}
if(!$StageRoot){
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
    Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/fixtures/AICF_VehicleSoakProbe.c') -Destination (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_VehicleSoakProbe.c')
    if($ProofVersion -eq 4){Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/fixtures/AICF_VehicleSoakRecoveryObservationProbe.c') -Destination (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_VehicleSoakRecoveryObservationProbe.c')}
    if($LooseWeaponObserver){Copy-Item -LiteralPath (Join-Path $taskRepository 'tests/fixtures/AICF_LooseWeaponObserverProbe.c') -Destination (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_LooseWeaponObserverProbe.c')}
    foreach($taskObserver in $taskObservers){
        $taskObserverSource=Join-Path $taskRepository $taskObserver.source
        if(!(Test-Path -LiteralPath $taskObserverSource -PathType Leaf)){throw ('Requested observer source missing: '+$taskObserver.name)}
        $taskObserverHash=(Get-FileHash -LiteralPath $taskObserverSource -Algorithm SHA256).Hash
        $taskObserverDestination=Join-Path $StageRoot $taskObserver.relative
        [void][IO.Directory]::CreateDirectory((Split-Path -Parent $taskObserverDestination))
        Copy-Item -LiteralPath $taskObserverSource -Destination $taskObserverDestination
        if((Get-FileHash -LiteralPath $taskObserverDestination -Algorithm SHA256).Hash-ne$taskObserverHash-or(Get-FileHash -LiteralPath $taskObserverSource -Algorithm SHA256).Hash-ne$taskObserverHash){throw ('Observer source copy drift: '+$taskObserver.name)}
    }
}
$StageRoot=(Resolve-Path -LiteralPath $StageRoot).Path
if($StageRoot -eq $taskRepository){throw 'Production checkout cannot be a soak stage'}
if(!(Test-Path -LiteralPath (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_VehicleSoakProbe.c'))){throw 'Soak fixture missing'}
if($ProofVersion -eq 4 -and !(Test-Path -LiteralPath (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_VehicleSoakRecoveryObservationProbe.c'))){throw 'Proof4 observation fixture missing'}
if($LooseWeaponObserver -and !(Test-Path -LiteralPath (Join-Path $StageRoot 'AIConflictCore/Scripts/Game/AIConflict/AICF_LooseWeaponObserverProbe.c'))){throw 'Loose weapon observer fixture missing'}
$taskObserverSources=@(foreach($taskObserver in $taskObservers){
    $taskObserverPath=Join-Path $StageRoot $taskObserver.relative
    if(!(Test-Path -LiteralPath $taskObserverPath -PathType Leaf)){throw ('Requested observer fixture missing from stage: '+$taskObserver.name)}
    [pscustomobject]@{name=$taskObserver.name;source=$taskObserver.source;path=$taskObserverPath;sha256=(Get-FileHash -LiteralPath $taskObserverPath -Algorithm SHA256).Hash;flag=$taskObserver.flag}
})
$taskWorkingTree=[bool]$SnapshotWorkingTree
$taskOriginManifest=$null
if(!$taskOwnStage){
    $taskOriginManifest=Join-Path (Split-Path -Parent $StageRoot) 'source-manifest.json'
    if(!(Test-Path -LiteralPath $taskOriginManifest -PathType Leaf)){throw 'Shared stage requires its original source manifest'}
    $taskOrigin=Get-Content -LiteralPath $taskOriginManifest -Raw|ConvertFrom-Json
    if((Resolve-Path -LiteralPath $taskOrigin.stage).Path -ne $StageRoot){throw 'Shared stage manifest path mismatch'}
    $taskCommit=$taskOrigin.base_commit
    $taskWorkingTree=[bool]$taskOrigin.working_tree
    Assert-AICFVehicleSoakSources $StageRoot @($taskOrigin.files)
    if($SkipCompile){
        $taskCompileRoot=Split-Path -Parent $taskOriginManifest
        $taskPriorCompileExit=Get-Content -LiteralPath (Join-Path $taskCompileRoot 'compile-exit.txt') -Raw
        $taskPriorCompile=Get-Content -LiteralPath (Join-Path $taskCompileRoot 'workbench/script.log') -Raw
        if($taskPriorCompileExit.Trim() -ne '0' -or !$taskPriorCompile.Contains('Script validation successful.') -or $taskPriorCompile -match 'SCRIPT\s+\([EF]\)'){throw 'Shared stage has no successful compile evidence'}
    }
}
$taskHashes=@(Get-AICFVehicleSoakSources $StageRoot)
[pscustomobject]@{base_commit=$taskCommit;working_tree=$taskWorkingTree;origin_manifest=$taskOriginManifest;stage=$StageRoot;faction=$Faction;duration_seconds=$DurationSeconds;mounted_hold_seconds=$MountedHoldSeconds;proof_version=$ProofVersion;auditor=$taskAuditScript;auditor_sha256=$taskAuditHash;recruitment_decision_observer=[bool]$RecruitmentDecisionObserver;quota_census_decision_observer=[bool]$QuotaCensusDecisionObserver;observer_sources=$taskObserverSources;files=$taskHashes}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskEvidence 'source-manifest.json')
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
    if($taskCompileExit -ne 0 -or !$taskCompile.Contains('Script validation successful.') -or $taskCompile -match 'SCRIPT\s+\([EF]\)'){throw 'Soak compilation failed; full logs saved'}
}
$taskProfile=Join-Path $taskEvidence 'profile'
"SOAK_PROFILE=$taskProfile FACTION=$Faction DURATION_SECONDS=$DurationSeconds"
$taskStarted=[DateTimeOffset]::UtcNow
$taskRuntimeArguments=@('-aicfVehicleSoakFaction',$Faction,'-aicfVehicleSoakDurationMs',([string]($DurationSeconds*1000)),'-aicfVehicleSoakMountedHoldMs',([string]($MountedHoldSeconds*1000)),'-aicfGroupTrace','1','-aicfGroupTraceFaction',$Faction,'-aicfGroupTraceSlot','0','-addr',"127.0.0.1:$ServerPort")
if($ProofVersion -eq 4){$taskRuntimeArguments+=@('-aicfVehicleSoakObservation4','1')}
if($LooseWeaponObserver){$taskRuntimeArguments+=@('-aicfLooseWeaponObserver','1')}
foreach($taskObserver in $taskObservers){$taskRuntimeArguments+=@($taskObserver.flag,'1')}
& (Join-Path $taskRepository 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode $AICommanderMode -RepositoryRoot $StageRoot -ProfileRoot $taskProfile -IsolatedLoadoutLibrary -ServerPort $ServerPort -AdditionalArguments $taskRuntimeArguments *> (Join-Path $taskEvidence 'launcher.txt')
$taskNativeExit=$LASTEXITCODE
$taskEnded=[DateTimeOffset]::UtcNow
$taskNativeExit|Set-Content -LiteralPath (Join-Path $taskEvidence 'native-exit.txt')
[pscustomobject]@{started_utc=$taskStarted.ToString('o');ended_utc=$taskEnded.ToString('o');wall_seconds=($taskEnded-$taskStarted).TotalSeconds;requested_seconds=$DurationSeconds;faction=$Faction}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskEvidence 'timing.json')
Assert-AICFVehicleSoakSources $StageRoot $taskHashes
$taskConsole=Get-ChildItem -LiteralPath (Join-Path $taskProfile 'logs') -Recurse -Filter console.log|Select-Object -First 1
if(!$taskConsole){throw 'No full console log'}
if((Get-FileHash -LiteralPath $taskAuditScript -Algorithm SHA256).Hash -ne $taskAuditHash){throw 'Frozen soak auditor changed during run'}
& $taskAuditScript -LogPath $taskConsole.FullName -EvidenceRoot $taskEvidence -Faction $Faction -MinimumSeconds $DurationSeconds -ProofVersion $ProofVersion *> (Join-Path $taskEvidence 'audit.txt')
$taskAuditExit=$LASTEXITCODE
[pscustomobject]@{faction=$Faction;proof_version=$ProofVersion;auditor_sha256=$taskAuditHash;native_exit=$taskNativeExit;audit_exit=$taskAuditExit;console=$taskConsole.FullName;wall_seconds=($taskEnded-$taskStarted).TotalSeconds}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskEvidence 'summary.json')
"SOAK_FINISHED=$Faction NATIVE_EXIT=$taskNativeExit AUDIT_EXIT=$taskAuditExit"
if($taskNativeExit -ne 0 -or $taskAuditExit -ne 0){exit 1}
exit 0
