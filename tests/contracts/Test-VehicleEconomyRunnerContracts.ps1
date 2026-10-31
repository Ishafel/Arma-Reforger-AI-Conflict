param([string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$path=Join-Path $RepositoryRoot 'tools/Invoke-AICFVehicleEconomyDecline.ps1'
$source=[IO.File]::ReadAllText($path)
$tokens=$null;$errors=$null
[void][Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw "Economy runner parse: $errors"}
function Rules($s){
 foreach($guard in @("'INSUFFICIENT','AFFORDABLE','DRAIN_AFTER_RESERVE'",'Get-AICFEconomyProbeArguments','-Role Server -Variant Stock','-IsolatedLoadoutLibrary','AICF_RUNTIME_MANIFEST_JSON','Script validation successful','SkipCompile requires an existing validated stage','Shared stage lacks successful Workbench evidence','Assert-AICFVehicleSoakSources $StageRoot $taskSources','Assert-AICFEconomyProduction $StageRoot $taskExpected','frozen-auditor.ps1','source_drift=$taskDrift','ECONOMY_PREPARED=','NATIVE=NOT_RUN')){if(!$s.Contains($guard)){'MISSING_CANONICAL_GATE';break}}
 if($s -match 'Start-Process|Invoke-Expression|aicfVehiclesEnabled|aicfEconomyEnabled'){'UNSAFE_OR_OPTOUT_LAUNCH'}
 if($s.IndexOf('if($MyInvocation.InvocationName -eq ''.''){return}') -gt $s.IndexOf('$taskRepository=')){'DOT_SOURCE_HAS_SIDE_EFFECTS'}
}
if(@(Rules $source).Count){throw "Economy runner guards: $(Rules $source)"}
foreach($guard in @('Assert-AICFEconomyPreparedSources $StageRoot','Assert-AICFVehicleSoakSources $Stage $compiled','Production source set differs from canonical manifest','Unexpected production file outside canonical manifest')){if(!$source.Contains($guard)){throw 'Compiled/canonical exact source tie missing'}}
$mutants=0
foreach($guard in @('-Role Server -Variant Stock','-IsolatedLoadoutLibrary','AICF_RUNTIME_MANIFEST_JSON','Script validation successful','SkipCompile requires an existing validated stage','Shared stage lacks successful Workbench evidence','Assert-AICFVehicleSoakSources $StageRoot $taskSources','Assert-AICFEconomyProduction $StageRoot $taskExpected','frozen-auditor.ps1','source_drift=$taskDrift','NATIVE=NOT_RUN')){
 if(!@(Rules ($source.Replace($guard,'MUTANT'))).Count){throw "Runner mutant escaped: $guard"};$mutants++
}
$scratch=Join-Path $RepositoryRoot ('.codex-runtime/economy-runner-contract-'+[guid]::NewGuid().ToString('N'))
. $path -EvidenceRoot $scratch -Mode INSUFFICIENT -RepositoryRoot $RepositoryRoot
if(Test-Path -LiteralPath $scratch){throw 'Dot sourcing created evidence'}
$actual=0
foreach($mode in @('INSUFFICIENT','AFFORDABLE','DRAIN_AFTER_RESERVE','drain_after_reserve')){
 $argv=@(Get-AICFEconomyProbeArguments $mode us 2660)
 if($argv.Count -ne 8 -or $argv[1] -ne '1' -or $argv[3] -cne $mode.ToUpperInvariant() -or $argv[5] -cne 'US' -or $argv[7] -cne '127.0.0.1:2660'){throw 'Incorrect exact native arguments'};$actual++
}
foreach($case in @(@('UNKNOWN','US',2660),@('INSUFFICIENT','CIV',2660),@('INSUFFICIENT','US',0),@('INSUFFICIENT','US',65536))){$caught=$false;try{Get-AICFEconomyProbeArguments $case[0] $case[1] $case[2]|Out-Null}catch{$caught=$true};if(!$caught){throw 'Invalid native arguments accepted'};$actual++}
[void][IO.Directory]::CreateDirectory($scratch)
try{
 $file=Join-Path $scratch 'one.c';[IO.File]::WriteAllText($file,'known bytes')
 $manifest=@([pscustomobject]@{path='one.c';sha256=(Get-FileHash $file).Hash})
 Assert-AICFEconomyProduction $scratch $manifest;$actual++
 foreach($bad in @(@([pscustomobject]@{path='one.c';sha256='BAD'}),@([pscustomobject]@{path='../outside';sha256='BAD'}),@([pscustomobject]@{path='missing.c';sha256='BAD'}))){$caught=$false;try{Assert-AICFEconomyProduction $scratch $bad}catch{$caught=$true};if(!$caught){throw 'Raw production integrity failure accepted'};$actual++}
 $extra=Join-Path $scratch 'unexpected.c';[IO.File]::WriteAllText($extra,'extra')
 $caught=$false;try{Assert-AICFEconomyProduction $scratch $manifest}catch{$caught=$true};if(!$caught){throw 'Extra production source accepted'};$actual++
 Remove-Item -LiteralPath $extra
 $stage=Join-Path $scratch 'stage';[void][IO.Directory]::CreateDirectory($stage)
 $compiled=Join-Path $stage 'compiled.c';[IO.File]::WriteAllText($compiled,'compiled bytes')
 [pscustomobject]@{stage=$stage;files=@([pscustomobject]@{path=$compiled;sha256=(Get-FileHash $compiled).Hash})}|ConvertTo-Json -Depth 4|Set-Content (Join-Path $scratch 'source-manifest.json')
 Assert-AICFEconomyPreparedSources $stage;$actual++
 [IO.File]::WriteAllText($compiled,'changed after successful WB')
 $caught=$false;try{Assert-AICFEconomyPreparedSources $stage}catch{$caught=$true};if(!$caught){throw 'Changed compiled source accepted'};$actual++
 [IO.File]::WriteAllText($compiled,'compiled bytes');[IO.File]::WriteAllText((Join-Path $stage 'extra.c'),'not compiled')
 $caught=$false;try{Assert-AICFEconomyPreparedSources $stage}catch{$caught=$true};if(!$caught){throw 'Extra uncompiled source accepted'};$actual++
}finally{
 $absolute=[IO.Path]::GetFullPath($scratch);$prefix=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot '.codex-runtime'))+[IO.Path]::DirectorySeparatorChar
 if(!$absolute.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "Unsafe cleanup path: $absolute outside $prefix"}
 Remove-Item -LiteralPath $absolute -Recurse -Force
}
Write-Output "PASS economy runner: PS5.1 parser/native CLI/raw hash containment actual=$actual source_mutants=$mutants native=NOT_RUN"
