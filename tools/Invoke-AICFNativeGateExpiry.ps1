[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$EvidenceRoot,
 [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
 [ValidateSet('US','USSR')][string]$Faction='US',
 [ValidateRange(1,65535)][int]$ServerPort=2689,
 [switch]$GateHQ
)
$ErrorActionPreference='Stop'
$taskRepo=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$taskRoot=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceRoot)
if(Test-Path -LiteralPath $taskRoot){throw 'Fresh evidence required'}
[void][IO.Directory]::CreateDirectory($taskRoot)
$taskStage=Join-Path $taskRoot 'stage'
$taskAddons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
$taskFiles=@(& git -C $taskRepo ls-files --cached --others --exclude-standard -- $taskAddons|Sort-Object -Unique)
if($LASTEXITCODE -ne 0 -or !$taskFiles.Count){throw 'Source inventory failed'}
$taskHashes=@()
foreach($taskRelative in $taskFiles){
 $taskSource=Join-Path $taskRepo $taskRelative
 $taskDestination=Join-Path $taskStage $taskRelative
 [void][IO.Directory]::CreateDirectory((Split-Path -Parent $taskDestination))
 Copy-Item -LiteralPath $taskSource -Destination $taskDestination
 $taskHash=(Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash
 if((Get-FileHash -LiteralPath $taskDestination -Algorithm SHA256).Hash -ne $taskHash){throw 'Source copy mismatch'}
 $taskHashes+=[pscustomobject]@{relative=$taskRelative;path=$taskDestination;sha256=$taskHash;fixture=$false}
}
foreach($taskFixture in @('AICF_VehicleScenarioProbe.c','AICF_NativeGateExpiryProbe.c')){
 $taskSource=Join-Path $taskRepo ('tests/fixtures/'+$taskFixture)
 $taskName=$taskFixture
 if($taskFixture -eq 'AICF_NativeGateExpiryProbe.c'){$taskName='ZZ_'+$taskFixture}
 $taskDestination=Join-Path $taskStage ('AIConflictCore/Scripts/Game/AIConflict/'+$taskName)
 Copy-Item -LiteralPath $taskSource -Destination $taskDestination
 $taskHashes+=[pscustomobject]@{relative=('tests/fixtures/'+$taskFixture);path=$taskDestination;sha256=(Get-FileHash -LiteralPath $taskDestination -Algorithm SHA256).Hash;fixture=$true}
}
$taskHashes|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskRoot 'source-manifest.json')
$taskAuditor=Join-Path $taskRoot 'Test-NativeGateExpiryLog.ps1'
Copy-Item -LiteralPath (Join-Path $taskRepo 'tests/log-audits/Test-NativeGateExpiryLog.ps1') -Destination $taskAuditor
$taskAuditorHash=(Get-FileHash -LiteralPath $taskAuditor -Algorithm SHA256).Hash
$taskPlan=@(& (Join-Path $taskRepo 'tools/Start-AICFRuntime.ps1') -Role Client -Variant Stock -RepositoryRoot $taskStage -DryRun)
$taskPlan|Set-Content -LiteralPath (Join-Path $taskRoot 'compile-plan.txt')
$taskManifest=([regex]::Match(($taskPlan -join "`n"),'AICF_RUNTIME_MANIFEST_JSON=([^\r\n]+)').Groups[1].Value)|ConvertFrom-Json
$taskArgs=$taskManifest.arguments
& 'C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe' -noThrow -wbsilent -gproj $taskArgs[[array]::IndexOf($taskArgs,'-gproj')+1] -addonsDir $taskArgs[[array]::IndexOf($taskArgs,'-addonsDir')+1] -addons $taskArgs[[array]::IndexOf($taskArgs,'-addons')+1] -logsDir (Join-Path $taskRoot 'workbench') -wbModule=ScriptEditor -run -validate|Out-Null
$taskCompile=$LASTEXITCODE
$taskCompile|Set-Content -LiteralPath (Join-Path $taskRoot 'compile-exit.txt')
$taskScript=Get-Content -LiteralPath (Join-Path $taskRoot 'workbench/script.log') -Raw
if($taskCompile -ne 0 -or !$taskScript.Contains('Script validation successful.') -or $taskScript -match 'SCRIPT\s+\([EF]\)'){throw 'Gate expiry compile failed; full logs saved'}
$taskAdditional=@('-aicfVehicleScenario','repeat5','-aicfVehicleScenarioFaction',$Faction,'-aicfVehicleScenarioCombat','0','-aicfVehicleScenarioRetainedBehavior','1','-aicfVehicleScenarioProtectionScope','HIERARCHY','-aicfNativeGateExpiryProbe','1','-aicfGroupTrace','1','-aicfGroupTraceFaction',$Faction,'-aicfGroupTraceSlot','0','-addr',"127.0.0.1:$ServerPort")
if($GateHQ){$taskAdditional+=@('-aicfNativeGateExpiryGateHQ','1')}
& (Join-Path $taskRepo 'tools/Start-AICFRuntime.ps1') -Role Server -Variant Stock -AICommanderMode BOTH -RepositoryRoot $taskStage -ProfileRoot (Join-Path $taskRoot 'profile') -ServerPort $ServerPort -AdditionalArguments $taskAdditional *> (Join-Path $taskRoot 'launcher.txt')
$taskNative=$LASTEXITCODE
$taskNative|Set-Content -LiteralPath (Join-Path $taskRoot 'native-exit.txt')
foreach($taskHash in $taskHashes){if((Get-FileHash -LiteralPath $taskHash.path -Algorithm SHA256).Hash -ne $taskHash.sha256){throw 'Active stage changed'}}
if((Get-FileHash -LiteralPath $taskAuditor -Algorithm SHA256).Hash -ne $taskAuditorHash){throw 'Frozen auditor changed'}
$taskConsole=(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'profile/logs') -Recurse -Filter console.log|Select-Object -First 1).FullName
& $taskAuditor -LogPath $taskConsole -LauncherPath (Join-Path $taskRoot 'launcher.txt') -NativeExitPath (Join-Path $taskRoot 'native-exit.txt') -EvidenceRoot $taskRoot *> (Join-Path $taskRoot 'audit.txt')
$taskAudit=$LASTEXITCODE
$taskAudit|Set-Content -LiteralPath (Join-Path $taskRoot 'audit-exit.txt')
"NATIVE_GATE_EXPIRY_FINISHED NATIVE=$taskNative TARGETED_AUDIT=$taskAudit"
if($taskNative -ne 0 -or $taskAudit -ne 0){exit 1}
exit 0
