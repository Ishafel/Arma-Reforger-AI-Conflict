param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools/AICF_VehicleSoakSources.ps1')
$taskRoot=Join-Path $RepositoryRoot ('.codex-runtime/soc-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$taskRepo=Join-Path $taskRoot 'repository'
$taskTools=Join-Path $taskRepo 'tools'
[void][IO.Directory]::CreateDirectory($taskTools)
foreach($name in @('Invoke-AICFVehicleSoak.ps1','AICF_VehicleSoakSources.ps1')){Copy-Item -LiteralPath (Join-Path $RepositoryRoot ('tools/'+$name)) -Destination $taskTools}
$taskAddons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
foreach($addon in $taskAddons){$dir=Join-Path $taskRepo $addon;[void][IO.Directory]::CreateDirectory($dir);Set-Content -LiteralPath (Join-Path $dir 'test.c') 'fixture-free mock source'}
$taskCore=Join-Path $taskRepo 'AIConflictCore/Scripts/Game/AIConflict';[void][IO.Directory]::CreateDirectory($taskCore);Set-Content -LiteralPath (Join-Path $taskCore 'test.c') 'mock core namespace'
$taskFixtures=Join-Path $taskRepo 'tests/fixtures'
$taskAudits=Join-Path $taskRepo 'tests/log-audits'
[void][IO.Directory]::CreateDirectory($taskFixtures);[void][IO.Directory]::CreateDirectory($taskAudits)
$taskNames=@('AICF_VehicleSoakProbe.c','AICF_VehicleSoakRecoveryObservationProbe.c','AICF_RecruitmentDecisionProbe.c','AICF_AIQuotaCensusDecisionProbe.c')
foreach($name in $taskNames){Set-Content -LiteralPath (Join-Path $taskFixtures $name) ('MOCK ONLY '+$name)}
$taskAudit=@'
param($LogPath,$EvidenceRoot,$Faction,$MinimumSeconds,$ProofVersion)
[pscustomobject]@{minimum_seconds=$MinimumSeconds;proof_version=$ProofVersion;faction=$Faction;log=$LogPath;boundary='SIMULATED_AUDITOR'}|ConvertTo-Json|Set-Content (Join-Path $EvidenceRoot 'mock-audit.json')
exit 0
'@
foreach($name in @('Test-VehicleSoakLog.ps1','Test-VehicleSoakObservationLog.ps1')){Set-Content -LiteralPath (Join-Path $taskAudits $name) $taskAudit}
$taskRuntime=@'
param($Role,$Variant,$AICommanderMode,$RepositoryRoot,$ProfileRoot,[switch]$IsolatedLoadoutLibrary,$ServerPort,[string[]]$AdditionalArguments,[switch]$DryRun)
if($Role-eq'Client'){throw 'MOCK_COMPILE_PLAN_BOUNDARY_NO_WORKBENCH'}
$e=Split-Path -Parent $ProfileRoot
[pscustomobject]@{role=$Role;variant=$Variant;mode=$AICommanderMode;port=$ServerPort;arguments=@($AdditionalArguments);boundary='SIMULATED_NATIVE_NO_GAME_PROCESS'}|ConvertTo-Json -Depth 4|Set-Content (Join-Path $e 'mock-runtime.json')
$logs=Join-Path $ProfileRoot 'logs/mock';[void][IO.Directory]::CreateDirectory($logs);Set-Content (Join-Path $logs 'console.log') 'Game destroyed. MOCK ONLY'
exit 0
'@
Set-Content -LiteralPath (Join-Path $taskTools 'Start-AICFRuntime.ps1') $taskRuntime
$taskWrapper=@'
param($Runner,$ArgumentsPath)
$ErrorActionPreference='Stop'
# The real runner is unchanged; only its external git/runtime/compiler boundaries are mocked.
function global:git {
 $a=@($args)
 if($a-contains'rev-parse'){$global:LASTEXITCODE=0;return ('0'*40)}
 if($a-contains'archive'){
  $repo=$a[[array]::IndexOf($a,'-C')+1];$dest=$a[[array]::IndexOf($a,'-o')+1]
  $addons=@('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')
  Compress-Archive -LiteralPath @($addons|ForEach-Object{Join-Path $repo $_}) -DestinationPath $dest
  $global:LASTEXITCODE=0;return
 }
 throw 'Unexpected git boundary'
}
$bound=@{};(Get-Content -LiteralPath $ArgumentsPath -Raw|ConvertFrom-Json).PSObject.Properties|ForEach-Object{$bound[$_.Name]=$_.Value}
& $Runner @bound
exit $LASTEXITCODE
'@
$taskWrapperPath=Join-Path $taskRoot 'InvokeMockBoundaries.ps1';Set-Content -LiteralPath $taskWrapperPath $taskWrapper
$taskRunner=Join-Path $taskTools 'Invoke-AICFVehicleSoak.ps1'
$taskShell=(Get-Command powershell.exe -CommandType Application).Source
$taskControls=0
$taskRows=@()
function Stage([string]$Name,[bool]$Recruit,[bool]$Census){
 $base=Join-Path $taskRoot ($Name+'-prepared');$stage=Join-Path $base 'stage';$core=Join-Path $stage 'AIConflictCore/Scripts/Game/AIConflict'
 [void][IO.Directory]::CreateDirectory($core)
 foreach($name in @('AICF_VehicleSoakProbe.c','AICF_VehicleSoakRecoveryObservationProbe.c')){Copy-Item -LiteralPath (Join-Path $taskFixtures $name) -Destination $core}
 if($Recruit){Copy-Item -LiteralPath (Join-Path $taskFixtures 'AICF_RecruitmentDecisionProbe.c') -Destination (Join-Path $core 'AICF_ZRecruitmentDecisionProbe.c')}
 if($Census){$dir=Join-Path $core 'Integration';[void][IO.Directory]::CreateDirectory($dir);Copy-Item -LiteralPath (Join-Path $taskFixtures 'AICF_AIQuotaCensusDecisionProbe.c') -Destination (Join-Path $dir 'AICF_ZZQuotaCensusDecisionProbe.c')}
 [pscustomobject]@{stage=$stage;base_commit=('0'*40);working_tree=$false;files=@(Get-AICFVehicleSoakSources $stage)}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $base 'source-manifest.json')
 Set-Content -LiteralPath (Join-Path $base 'compile-exit.txt') '0';$wb=Join-Path $base 'workbench';[void][IO.Directory]::CreateDirectory($wb);Set-Content -LiteralPath (Join-Path $wb 'script.log') 'Script validation successful.'
 return $stage
}
function RunCase([string]$Name,[string]$Stage,[bool]$Recruit,[bool]$Census,[bool]$Success,[string]$Failure){
 $e=Join-Path $taskRoot $Name;$argsPath=Join-Path $taskRoot ($Name+'-arguments.json')
 $bound=@{EvidenceRoot=$e;Faction='US';RepositoryRoot=$taskRepo;ProofVersion=4;DurationSeconds=1800;RecruitmentDecisionObserver=$Recruit;QuotaCensusDecisionObserver=$Census}
 if($Stage){$bound.StageRoot=$Stage;$bound.SkipCompile=$true}
 $bound|ConvertTo-Json|Set-Content -LiteralPath $argsPath
 $output=Join-Path $taskRoot ($Name+'-output.txt')
 $priorPreference=$ErrorActionPreference
 try{
  # WinPS wraps child stderr in NativeCommandError; inspect the real exit and full output below.
  $ErrorActionPreference='Continue'
  & $taskShell -NoProfile -ExecutionPolicy Bypass -File $taskWrapperPath -Runner $taskRunner -ArgumentsPath $argsPath *> $output
 }finally{$ErrorActionPreference=$priorPreference}
 $code=$LASTEXITCODE
 if($Success){
  if($code-ne0){throw ('Real separate CLI failed '+$Name+' '+(Get-Content $output -Raw))}
  $runtime=Get-Content (Join-Path $e 'mock-runtime.json') -Raw|ConvertFrom-Json
  $audit=Get-Content (Join-Path $e 'mock-audit.json') -Raw|ConvertFrom-Json
  if($runtime.mode-ne'BOTH'-or$audit.minimum_seconds-ne1800-or$audit.proof_version-ne4){throw 'Existing full duration/commander/auditor changed'}
  foreach($pair in @(@('-aicfRecruitmentDecisionProbe',$Recruit),@('-aicfQuotaCensusDecisionProbe',$Census))){$count=@($runtime.arguments|Where-Object{$_-eq$pair[0]}).Count;if($count-ne[int]$pair[1]){throw ('CLI opt-in mismatch '+$Name)};if($pair[1]-and$runtime.arguments[[array]::IndexOf($runtime.arguments,$pair[0])+1]-ne'1'){throw 'Observer value changed'}}
  if($runtime.arguments[[array]::IndexOf($runtime.arguments,'-aicfVehicleSoakDurationMs')+1]-ne'1800000'){throw '1800s CLI changed'}
  $m=Get-Content (Join-Path $e 'source-manifest.json') -Raw|ConvertFrom-Json
  if(@($m.observer_sources).Count-ne([int]$Recruit+[int]$Census)){throw 'Observer provenance missing'}
  foreach($row in $m.observer_sources){if((Get-FileHash -LiteralPath $row.path).Hash-ne$row.sha256){throw 'Observer SHA receipt wrong'}}
 }else{
  if($code-eq0-or!(Get-Content $output -Raw).Contains($Failure)){throw ('Expected rejection missing '+$Name)}
  if(Test-Path -LiteralPath (Join-Path $e 'mock-runtime.json')){throw 'Invalid observer stage reached runtime boundary'}
 }
 $script:taskControls++;$script:taskRows+=[pscustomobject]@{name=$Name;exit=$code;expected_success=$Success;boundary='SIMULATED_NO_NATIVE_NO_WB'}
 return $e
}
foreach($pair in @(@($false,$false),@($true,$false),@($false,$true),@($true,$true))){$name='shared-'+[int]$pair[0]+'-'+[int]$pair[1];$stage=Stage $name $pair[0] $pair[1];$null=RunCase $name $stage $pair[0] $pair[1] $true ''}
$stage=Stage missing-recruit $false $true;$null=RunCase missing-recruit $stage $true $true $false 'Requested observer fixture missing from stage: RecruitmentDecisionObserver'
$stage=Stage missing-census $true $false;$null=RunCase missing-census $stage $true $true $false 'Requested observer fixture missing from stage: QuotaCensusDecisionObserver'
$stage=Stage drift $true $true;Add-Content -LiteralPath (Join-Path $stage 'AIConflictCore/Scripts/Game/AIConflict/AICF_ZRecruitmentDecisionProbe.c') 'changed after compile';$null=RunCase drift $stage $true $true $false 'Stage source path or hash changed'
$stage=Stage uncompiled-extra $false $false;Copy-Item -LiteralPath (Join-Path $taskFixtures 'AICF_RecruitmentDecisionProbe.c') -Destination (Join-Path $stage 'AIConflictCore/Scripts/Game/AIConflict/AICF_ZRecruitmentDecisionProbe.c');$null=RunCase uncompiled-extra $stage $true $false $false 'Stage source set changed'
$e=RunCase own-copy '' $true $true $false 'MOCK_COMPILE_PLAN_BOUNDARY_NO_WORKBENCH'
$m=Get-Content (Join-Path $e 'source-manifest.json') -Raw|ConvertFrom-Json
if(@($m.observer_sources).Count-ne2){throw 'Own stage did not copy both observers'}
foreach($row in $m.observer_sources){if((Get-FileHash -LiteralPath (Join-Path $taskRepo $row.source)).Hash-ne$row.sha256-or(Get-FileHash -LiteralPath $row.path).Hash-ne$row.sha256){throw 'Own-stage observer copy changed'}}
Remove-Item -LiteralPath (Join-Path $taskFixtures 'AICF_AIQuotaCensusDecisionProbe.c')
$null=RunCase own-missing-source '' $true $true $false 'Requested observer source missing: QuotaCensusDecisionObserver'
$taskRows|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $taskRoot 'verdict.json')
"PASS Soak observer real separate WinPS CLI controls=$taskControls native_boundary=SIMULATED Workbench=NOT_RUN evidence=$taskRoot"
