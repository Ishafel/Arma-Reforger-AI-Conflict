param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$runnerTestRepo=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$runnerTestPath=Join-Path $runnerTestRepo 'tools/Invoke-AICFQuotaCatalogProbe.ps1'
$runnerTestRoot=Join-Path $runnerTestRepo ('.codex-runtime/catalog-runner-contract-'+[guid]::NewGuid().ToString('N'))
$runnerSentinel=Join-Path $runnerTestRoot 'dot-source-must-not-create'
. $runnerTestPath -EvidenceRoot $runnerSentinel -RepositoryRoot $runnerTestRepo
if(Test-Path -LiteralPath $runnerSentinel){throw 'Dot-source performed filesystem preparation'}
foreach($name in @('Get-AICFQuotaCatalogPayload','Assert-AICFQuotaCatalogPayload','Get-AICFQuotaCatalogCanonical')){if(!(Get-Command $name -ErrorAction SilentlyContinue)){throw 'Validation functions not loaded'}}
$runnerCases=1
$probeContract=[IO.File]::ReadAllText((Join-Path $runnerTestRepo 'tests/contracts/Test-AIQuotaCatalogProbeContracts.ps1'))
$positive=[regex]::Match($probeContract,"(?s)\`$log=@'\r?\n(.*?)\r?\n'@").Groups[1].Value
if(!$positive.Contains('[SELECTED]')){throw 'Controlled parser log fixture absent'}
function New-CatalogRunnerControlledCase([string]$Name,[string]$ProbeCase='group',[bool]$DepotSetup=$false){
 $folder=Join-Path $runnerTestRoot $Name;$stage=Join-Path $folder 'stage';[void][IO.Directory]::CreateDirectory($stage)
 foreach($addon in @('AIConflictCore','AIConflictArland','AIConflictEveron','AIConflictArlandRHS','AIConflictEveronRHS','AIConflictArlandWCSRHS','AIConflictEveronWCSRHS')){
  [void][IO.Directory]::CreateDirectory((Join-Path $stage $addon));[IO.File]::WriteAllText((Join-Path $stage ($addon+'/addon.gproj')),'CONTROLLED_TEST_INPUT_NOT_NATIVE_COMPILE')
 }
 $files=@(Get-AICFQuotaCatalogPayload $stage);$fixture='AIConflictCore/Scripts/Game/AIConflict/Integration/AICF_ZZQuotaCatalogProbe.c'
 [void][IO.Directory]::CreateDirectory((Split-Path -Parent (Join-Path $stage $fixture)));[IO.File]::WriteAllText((Join-Path $stage $fixture),'CONTROLLED_TEST_FIXTURE_NOT_GAMEPLAY')
 $audit=Join-Path $folder 'Test-AIQuotaCatalogLog.ps1';Copy-Item -LiteralPath (Join-Path $runnerTestRepo 'tests/log-audits/Test-AIQuotaCatalogLog.ps1') -Destination $audit
 $manifest=[pscustomobject]@{stage=$stage;variant='Stock';server_port=2670;rhs_addons_root='';controlled_depot_setup=$DepotSetup;files=$files;fixture=[pscustomobject]@{path=$fixture;sha256=(Get-FileHash (Join-Path $stage $fixture)).Hash};auditor_sha256=(Get-FileHash $audit).Hash;canonical_launcher_sha256=(Get-FileHash (Join-Path $runnerTestRepo 'tools/Start-AICFRuntime.ps1')).Hash}
 $manifestPath=Join-Path $folder 'source-manifest.json';$manifest|ConvertTo-Json -Depth 7|Set-Content $manifestPath -Encoding UTF8
 [void][IO.Directory]::CreateDirectory((Join-Path $folder 'workbench'));$compiler=Join-Path $folder 'workbench/script.log';[IO.File]::WriteAllText($compiler,"CONTROLLED TEST RECEIPT, NOT AN ACTUAL WORKBENCH RESULT`nScript validation successful.")
 [ordered]@{exit=0;source_manifest_sha256=(Get-FileHash $manifestPath).Hash;log_sha256=(Get-FileHash $compiler).Hash}|ConvertTo-Json|Set-Content (Join-Path $folder 'compile-receipt.json')
 foreach($nativeRole in @('Server','Client')){
  $profile=Join-Path $folder ($ProbeCase+'/'+$nativeRole+'-profile');$logs=Join-Path $profile 'logs/test';[void][IO.Directory]::CreateDirectory($logs)
  $nativeArgs=@('-gproj',(Join-Path $stage 'AIConflictArland/addon.gproj'),'-aicfQuotaCatalogProbe',$ProbeCase)
  if($DepotSetup){$nativeArgs+=@('-aicfQuotaCatalogControlledDepotSetup','1')}
  $runtime=[ordered]@{role=$nativeRole;variant='Stock';profile=$profile;arguments=$nativeArgs}
  $launcher='AICF_RUNTIME_MANIFEST_JSON='+($runtime|ConvertTo-Json -Depth 5 -Compress)+"`n[AICF][RUNTIME_LAUNCHER][EXIT] role=$nativeRole exit_code=0 exit_code_source=NATIVE"
  if($nativeRole-eq'Client'){$launcher+="`n[AICF][RUNTIME_LAUNCHER][SERVER_READY] process_id=999999"}
  [IO.File]::WriteAllText((Join-Path $folder ($ProbeCase+'/'+$nativeRole+'-launcher.txt')),$launcher)
  [IO.File]::WriteAllText((Join-Path $folder ($ProbeCase+'/'+$nativeRole+'-exit.txt')),'0')
  $text='CLI Params: -gproj '+(Join-Path $stage 'AIConflictArland/addon.gproj')+' -aicfQuotaCatalogProbe '+$ProbeCase
  if($DepotSetup){$text+=' -aicfQuotaCatalogControlledDepotSetup 1'}
  $text+="`n"
  if($nativeRole-eq'Server'){
   $proof=$positive
   if($ProbeCase-eq'vehicle'){
    $proof=($positive -split '\r?\n'|Where-Object {$_-notmatch'\[(?:NATIVE_HEADER|FULL_BASKET_NEGATIVE|NATIVE_CALLBACK)\]' -and $_-notmatch'entity=E2'}) -join "`n"
    $proof=$proof.Replace('mode=group','mode=vehicle').Replace('expected=2','expected=1').Replace('factories=2','factories=1').Replace('callbacks=2','callbacks=0').Replace('headers=1','headers=0').Replace('retained_original_body_count=2','retained_original_body_count=1')
    $proof+="`n[AICF][QUOTA_CATALOG_PROBE][NATIVE_VEHICLE_FACTORY] mode=vehicle actual_protected_factory_return=1 actual_vehicle=1 retained_physical_identity=1 character_basket=0`n[AICF][QUOTA_CATALOG_PROBE][EMPTY_VEHICLE] mode=vehicle actual_compartments_read=1 occupants=0 reservations=0 counted_vehicle_agent=0 character_bucket_full=1"
   }
   $text+="[ROSTER_READY]`n"+$proof
  }else{$text+='[AICF][QUOTA_CLIENT_BOOTSTRAP] phase=4 actual_actor=1 actual_faction=USSR player=1'}
  $text+="`nENGINE : Game destroyed.";[IO.File]::WriteAllText((Join-Path $logs 'console.log'),$text)
 }
 [pscustomobject]@{folder=$folder;stage=$stage;manifest=$manifest;audit=$audit;compiler=$compiler}
}
$base=New-CatalogRunnerControlledCase 'source-functions'
Assert-AICFQuotaCatalogPayload $base.manifest $base.stage $base.audit;$runnerCases++
foreach($mutation in @('file','extra','auditor','outside','duplicate')){
 $data=New-CatalogRunnerControlledCase ('function-'+$mutation)
 switch($mutation){
  file {[IO.File]::AppendAllText((Join-Path $data.stage $data.manifest.files[0].path),'changed')}
  extra {[IO.File]::WriteAllText((Join-Path $data.stage 'AIConflictCore/unexpected.c'),'extra')}
  auditor {[IO.File]::AppendAllText($data.audit,"`n# drift")}
  outside {$data.manifest.files[0].path='../escaped.c'}
  duplicate {$data.manifest.files+=@($data.manifest.files[0])}
 }
 $rejected=$false;try{Assert-AICFQuotaCatalogPayload $data.manifest $data.stage $data.audit}catch{$rejected=$true}
 if(!$rejected){throw ('Actual stage validation accepted '+$mutation)};$runnerCases++
}
$actualCli=0
foreach($scenario in @('positive','sourceDrift','compileDrift','wrongProject','missingRoster','clientResource','clientFatal','missingClientAvatar','missingClientReady','nativeExit','missingManifest')){
 $data=New-CatalogRunnerControlledCase ('cli-'+$scenario);$caseFolder=Join-Path $data.folder 'group'
 switch($scenario){
  sourceDrift {[IO.File]::AppendAllText((Join-Path $data.stage $data.manifest.files[0].path),'drift')}
  compileDrift {[IO.File]::AppendAllText($data.compiler,'changed')}
  wrongProject {$path=Join-Path $caseFolder 'Client-launcher.txt';$s=[IO.File]::ReadAllText($path);$s=$s.Replace(($data.stage.Replace('\','\\')),'C:\\uncompiled');[IO.File]::WriteAllText($path,$s)}
  missingRoster {$path=Join-Path $caseFolder 'Server-profile/logs/test/console.log';[IO.File]::WriteAllText($path,([IO.File]::ReadAllText($path)).Replace('[ROSTER_READY]','[NOT_READY]'))}
  clientResource {[IO.File]::WriteAllText((Join-Path $caseFolder 'Client-profile/logs/test/error.log'),'RESOURCES (E): controlled absence')}
  clientFatal {[IO.File]::WriteAllText((Join-Path $caseFolder 'Client-profile/logs/test/error.log'),'VM : Uncaught exception')}
  missingClientAvatar {$path=Join-Path $caseFolder 'Client-profile/logs/test/console.log';[IO.File]::WriteAllText($path,([IO.File]::ReadAllText($path)).Replace('actual_actor=1','actual_actor=0'))}
  missingClientReady {$path=Join-Path $caseFolder 'Client-launcher.txt';[IO.File]::WriteAllText($path,([IO.File]::ReadAllText($path)).Replace('[SERVER_READY]','[NOT_READY]'))}
  nativeExit {[IO.File]::WriteAllText((Join-Path $caseFolder 'Client-exit.txt'),'-1')}
  missingManifest {Remove-Item -LiteralPath (Join-Path $data.folder 'source-manifest.json')}
 }
 $savedPreference=$ErrorActionPreference;$ErrorActionPreference='Continue'
 try{$output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runnerTestPath -Phase Audit -Case group -EvidenceRoot $data.folder -RepositoryRoot $runnerTestRepo 2>&1);$actualExit=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
 $output|Set-Content (Join-Path $data.folder 'actual-cli.txt')
 $expected=1;if($scenario-eq'positive'){$expected=0}
 if($actualExit-ne$expected){throw "Actual runner CLI $scenario returned $actualExit expected $expected at $($data.folder): $output"};$actualCli++
}
$setupCli=0
foreach($invalidCase in @('group','character')){
 $invalidEvidence=Join-Path $runnerTestRoot ('invalid-setup-'+$invalidCase)
 $savedPreference=$ErrorActionPreference;$ErrorActionPreference='Continue'
 try{$output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runnerTestPath -Phase Prepare -Case $invalidCase -ControlledDepotSetup -EvidenceRoot $invalidEvidence -RepositoryRoot $runnerTestRepo 2>&1);$actualExit=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
 if($actualExit-ne1-or(Test-Path -LiteralPath $invalidEvidence)-or($output-join' ')-notmatch'ControlledDepotSetup requires vehicle case'){throw 'Nonvehicle controlled setup reached preparation'};$setupCli++
}
foreach($scenario in @('enabled','disabled','manifestMismatch','missingFlag','wrongValue','duplicateFlag','unexpectedFlag','consoleMissing','consoleWrong','consoleDuplicate')){
 $enabled=$scenario-ne'disabled'-and$scenario-ne'unexpectedFlag'
 $data=New-CatalogRunnerControlledCase ('setup-'+$scenario) 'vehicle' $enabled
 $caseFolder=Join-Path $data.folder 'vehicle'
 $launcherPath=Join-Path $caseFolder 'Client-launcher.txt'
 $consolePath=Join-Path $caseFolder 'Client-profile/logs/test/console.log'
 $runtimeLines=@(Get-Content $launcherPath);$runtime=$runtimeLines[0].Substring('AICF_RUNTIME_MANIFEST_JSON='.Length)|ConvertFrom-Json
 switch($scenario){
  missingFlag {$runtime.arguments=@($runtime.arguments|Where-Object {$_-ne'-aicfQuotaCatalogControlledDepotSetup'-and$_-ne'1'})}
  wrongValue {$runtime.arguments[-1]='0'}
  duplicateFlag {$runtime.arguments+=@('-aicfQuotaCatalogControlledDepotSetup','1')}
  unexpectedFlag {$runtime.arguments+=@('-aicfQuotaCatalogControlledDepotSetup','1')}
  consoleMissing {[IO.File]::WriteAllText($consolePath,([IO.File]::ReadAllText($consolePath)).Replace(' -aicfQuotaCatalogControlledDepotSetup 1',''))}
  consoleWrong {[IO.File]::WriteAllText($consolePath,([IO.File]::ReadAllText($consolePath)).Replace(' -aicfQuotaCatalogControlledDepotSetup 1',' -aicfQuotaCatalogControlledDepotSetup 0'))}
  consoleDuplicate {[IO.File]::WriteAllText($consolePath,([IO.File]::ReadAllText($consolePath)).Replace(' -aicfQuotaCatalogControlledDepotSetup 1',' -aicfQuotaCatalogControlledDepotSetup 1 -aicfQuotaCatalogControlledDepotSetup 1'))}
 }
 $runtimeLines[0]='AICF_RUNTIME_MANIFEST_JSON='+($runtime|ConvertTo-Json -Depth 5 -Compress);$runtimeLines|Set-Content $launcherPath
 $setupArguments=@();if($enabled-and$scenario-ne'manifestMismatch'){$setupArguments=@('-ControlledDepotSetup')}
 $savedPreference=$ErrorActionPreference;$ErrorActionPreference='Continue'
 try{$output=@(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runnerTestPath -Phase Audit -Case vehicle -EvidenceRoot $data.folder -RepositoryRoot $runnerTestRepo @setupArguments 2>&1);$actualExit=$LASTEXITCODE}finally{$ErrorActionPreference=$savedPreference}
 $output|Set-Content (Join-Path $data.folder 'actual-cli.txt')
 $expected=1;if($scenario-in@('enabled','disabled')){$expected=0}
 if($actualExit-ne$expected){throw "Controlled depot runner $scenario returned $actualExit expected $expected at $($data.folder): $output"};$setupCli++
}
"PASS Catalog runner actual_stage_and_dot_source=$runnerCases actual_separate_cli=$actualCli controlled_setup_cli=$setupCli native=NOT_RUN controlled_test_receipts_only=1 evidence=$runnerTestRoot"
